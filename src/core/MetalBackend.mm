#include "Backend.h"

#ifndef FORESTFIRE_HAS_METAL

bool metalBackendAvailable() { return false; }

std::string metalBackendDescription() { return "Metal disabled at build time"; }

std::unique_ptr<IBackend> makeMetalBackend() { return nullptr; }

#else

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

NSString *const kKernelSource = @R"MSL(
#include <metal_stdlib>
using namespace metal;

inline uint ff_hash(uint v) {
    v ^= v << 13;
    v ^= v >> 17;
    v ^= v << 5;
    return v;
}

inline float ff_rand(uint x, uint y, uint seed) {
    uint h = ff_hash(x * 1973u + y * 9277u + seed * 26699u);
    h = ff_hash(h);
    return float(h & 0xFFFFFFu) / float(0x1000000);
}

struct Params {
    float p;
    float g;
    int width;
    int height;
    int useMoore;
    uint seed;
};

kernel void ff_step(
    device const uchar *current  [[buffer(0)]],
    device uchar *next           [[buffer(1)]],
    constant Params &params      [[buffer(2)]],
    uint2 gid                    [[thread_position_in_grid]]) {
    int x = int(gid.x);
    int y = int(gid.y);
    if (x >= params.width || y >= params.height) return;
    int idx = x * params.height + y;
    uchar state = current[idx];

    if (state == 1) {
        next[idx] = 2;
        return;
    }
    if (state == 0) {
        bool fire_nearby = false;
        if (x > 0 && current[(x - 1) * params.height + y] == 1) fire_nearby = true;
        if (y > 0 && current[x * params.height + (y - 1)] == 1) fire_nearby = true;
        if (x < params.width - 1 && current[(x + 1) * params.height + y] == 1) fire_nearby = true;
        if (y < params.height - 1 && current[x * params.height + (y + 1)] == 1) fire_nearby = true;
        if (params.useMoore != 0 && !fire_nearby) {
            if (x > 0 && y > 0 && current[(x - 1) * params.height + (y - 1)] == 1) fire_nearby = true;
            if (!fire_nearby && x < params.width - 1 && y > 0 && current[(x + 1) * params.height + (y - 1)] == 1) fire_nearby = true;
            if (!fire_nearby && x > 0 && y < params.height - 1 && current[(x - 1) * params.height + (y + 1)] == 1) fire_nearby = true;
            if (!fire_nearby && x < params.width - 1 && y < params.height - 1 && current[(x + 1) * params.height + (y + 1)] == 1) fire_nearby = true;
        }
        next[idx] = (fire_nearby || ff_rand(uint(x), uint(y), params.seed) < params.p) ? 1 : 0;
        return;
    }
    next[idx] = (ff_rand(uint(x), uint(y), params.seed + 0x9e3779b9u) < params.g) ? 0 : 2;
}
)MSL";

struct Params {
    float p;
    float g;
    int width;
    int height;
    int useMoore;
    uint32_t seed;
};

class MetalBackend final : public IBackend {
public:
    MetalBackend(id<MTLDevice> device, std::string deviceName)
        : device_(device), description_("GPU/Metal: " + std::move(deviceName)) {
        @autoreleasepool {
            queue_ = [device_ newCommandQueue];
            if (!queue_) throw std::runtime_error("Metal: failed to create command queue");

            NSError *error = nil;
            MTLCompileOptions *opts = [[MTLCompileOptions alloc] init];
            id<MTLLibrary> library = [device_ newLibraryWithSource:kKernelSource options:opts error:&error];
            if (!library) {
                std::string msg = error ? error.localizedDescription.UTF8String : "unknown error";
                throw std::runtime_error("Metal: shader compile failed: " + msg);
            }

            id<MTLFunction> fn = [library newFunctionWithName:@"ff_step"];
            if (!fn) throw std::runtime_error("Metal: kernel function not found");

            pipeline_ = [device_ newComputePipelineStateWithFunction:fn error:&error];
            if (!pipeline_) {
                std::string msg = error ? error.localizedDescription.UTF8String : "unknown error";
                throw std::runtime_error("Metal: pipeline creation failed: " + msg);
            }

            std::random_device rd;
            seedCounter_ = rd();
        }
    }

    ~MetalBackend() override = default;

    std::string name() const override { return description_; }
    BackendKind kind() const override { return BackendKind::GPU; }

    void prepare(int w, int h) override {
        if (w == width_ && h == height_ && bufferA_ && bufferB_) return;

        @autoreleasepool {
            NSUInteger size = static_cast<NSUInteger>(w) * h;
            bufferA_ = [device_ newBufferWithLength:size options:MTLResourceStorageModeShared];
            bufferB_ = [device_ newBufferWithLength:size options:MTLResourceStorageModeShared];
            if (!bufferA_ || !bufferB_) throw std::runtime_error("Metal: buffer allocation failed");
            width_ = w;
            height_ = h;
            flat_.assign(size, EMPTY);
        }
    }

    void seed(ForestGrid &grid, double startGrowth) override {
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        for (auto &col: grid)
            for (auto &cell: col)
                cell = (dist(rng) < startGrowth) ? TREE : EMPTY;
    }

    void step(ForestGrid &grid, double p, double g, NeighborhoodLogic logic) override {
        const int w = static_cast<int>(grid.size());
        const int h = w > 0 ? static_cast<int>(grid[0].size()) : 0;

        prepare(w, h);
        flatten(grid);

        @autoreleasepool {
            std::memcpy([bufferA_ contents], flat_.data(), flat_.size());

            Params params{
                static_cast<float>(p),
                static_cast<float>(g),
                w,
                h,
                (logic == MOORE) ? 1 : 0,
                ++seedCounter_,
            };

            id<MTLCommandBuffer> cmd = [queue_ commandBuffer];
            id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
            [enc setComputePipelineState:pipeline_];
            [enc setBuffer:bufferA_ offset:0 atIndex:0];
            [enc setBuffer:bufferB_ offset:0 atIndex:1];
            [enc setBytes:&params length:sizeof(params) atIndex:2];

            MTLSize gridSize = MTLSizeMake(static_cast<NSUInteger>(w), static_cast<NSUInteger>(h), 1);
            NSUInteger tgw = pipeline_.threadExecutionWidth;
            NSUInteger tgh = std::max<NSUInteger>(1, pipeline_.maxTotalThreadsPerThreadgroup / tgw);
            MTLSize threadgroup = MTLSizeMake(tgw, tgh, 1);
            [enc dispatchThreads:gridSize threadsPerThreadgroup:threadgroup];
            [enc endEncoding];
            [cmd commit];
            [cmd waitUntilCompleted];

            std::memcpy(flat_.data(), [bufferB_ contents], flat_.size());
            unflatten(grid);
        }
    }

private:
    void flatten(const ForestGrid &grid) {
        for (int x = 0; x < width_; ++x)
            std::memcpy(flat_.data() + static_cast<size_t>(x) * height_, grid[x].data(), height_);
    }

    void unflatten(ForestGrid &grid) const {
        for (int x = 0; x < width_; ++x)
            std::memcpy(grid[x].data(), flat_.data() + static_cast<size_t>(x) * height_, height_);
    }

    id<MTLDevice> device_{};
    id<MTLCommandQueue> queue_{};
    id<MTLComputePipelineState> pipeline_{};
    id<MTLBuffer> bufferA_{};
    id<MTLBuffer> bufferB_{};
    int width_{0};
    int height_{0};
    std::vector<uint8_t> flat_;
    uint32_t seedCounter_{0};
    std::string description_;
};

struct MetalProbe {
    bool available{false};
    id<MTLDevice> device{};
    std::string deviceName;

    MetalProbe() {
        @autoreleasepool {
            id<MTLDevice> d = MTLCreateSystemDefaultDevice();
            if (d) {
                device = d;
                available = true;
                deviceName = d.name.UTF8String;
            } else {
                deviceName = "No Metal device";
            }
        }
    }
};

const MetalProbe &probe() {
    static MetalProbe p;
    return p;
}

}

bool metalBackendAvailable() {
    return probe().available;
}

std::string metalBackendDescription() {
    return probe().available ? std::string("GPU/Metal: ") + probe().deviceName
                              : probe().deviceName;
}

std::unique_ptr<IBackend> makeMetalBackend() {
    if (!probe().available) return nullptr;
    try {
        return std::make_unique<MetalBackend>(probe().device, probe().deviceName);
    } catch (const std::exception &ex) {
        std::cerr << "Metal backend init failed: " << ex.what() << "\n";
        return nullptr;
    }
}

#endif
