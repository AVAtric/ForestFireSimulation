#include "Backend.h"

#ifndef FORESTFIRE_HAS_OPENCL

bool openclBackendAvailable() { return false; }

std::string openclBackendDescription() { return "OpenCL disabled at build time"; }

std::unique_ptr<IBackend> makeOpenCLBackend() { return nullptr; }

#else

#define CL_TARGET_OPENCL_VERSION 120
#define CL_SILENCE_DEPRECATION

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const char *kKernelSource = R"CLC(
inline uint ff_hash(uint v) {
    v ^= v << 13;
    v ^= v >> 17;
    v ^= v << 5;
    return v;
}

inline float ff_rand(uint x, uint y, uint seed) {
    uint h = ff_hash(x * 1973u + y * 9277u + seed * 26699u);
    h = ff_hash(h);
    return (float)(h & 0xFFFFFFu) / (float)0x1000000;
}

__kernel void ff_step(
    __global const uchar *current,
    __global uchar *next,
    const float p,
    const float g,
    const int width,
    const int height,
    const int use_moore,
    const uint seed) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= width || y >= height) return;
    int idx = x * height + y;
    uchar state = current[idx];

    if (state == 1u) {
        next[idx] = 2u;
        return;
    }
    if (state == 0u) {
        bool fire_nearby = false;
        if (x > 0 && current[(x - 1) * height + y] == 1u) fire_nearby = true;
        if (y > 0 && current[x * height + (y - 1)] == 1u) fire_nearby = true;
        if (x < width - 1 && current[(x + 1) * height + y] == 1u) fire_nearby = true;
        if (y < height - 1 && current[x * height + (y + 1)] == 1u) fire_nearby = true;
        if (use_moore != 0 && !fire_nearby) {
            if (x > 0 && y > 0 && current[(x - 1) * height + (y - 1)] == 1u) fire_nearby = true;
            if (!fire_nearby && x < width - 1 && y > 0 && current[(x + 1) * height + (y - 1)] == 1u) fire_nearby = true;
            if (!fire_nearby && x > 0 && y < height - 1 && current[(x - 1) * height + (y + 1)] == 1u) fire_nearby = true;
            if (!fire_nearby && x < width - 1 && y < height - 1 && current[(x + 1) * height + (y + 1)] == 1u) fire_nearby = true;
        }
        next[idx] = (fire_nearby || ff_rand(x, y, seed) < p) ? 1u : 0u;
        return;
    }
    next[idx] = (ff_rand(x, y, seed + 0x9e3779b9u) < g) ? 0u : 2u;
}
)CLC";

bool pickPlatformAndDevice(cl_platform_id &platformOut, cl_device_id &deviceOut, std::string &deviceName) {
    cl_uint numPlatforms = 0;
    if (clGetPlatformIDs(0, nullptr, &numPlatforms) != CL_SUCCESS || numPlatforms == 0)
        return false;

    std::vector<cl_platform_id> platforms(numPlatforms);
    if (clGetPlatformIDs(numPlatforms, platforms.data(), nullptr) != CL_SUCCESS)
        return false;

    for (auto deviceType: {CL_DEVICE_TYPE_GPU, CL_DEVICE_TYPE_CPU}) {
        for (auto platform: platforms) {
            cl_uint numDevices = 0;
            if (clGetDeviceIDs(platform, deviceType, 0, nullptr, &numDevices) != CL_SUCCESS || numDevices == 0)
                continue;

            std::vector<cl_device_id> devices(numDevices);
            if (clGetDeviceIDs(platform, deviceType, numDevices, devices.data(), nullptr) != CL_SUCCESS)
                continue;

            char nameBuf[256] = {};
            clGetDeviceInfo(devices[0], CL_DEVICE_NAME, sizeof(nameBuf), nameBuf, nullptr);

            platformOut = platform;
            deviceOut = devices[0];
            deviceName = nameBuf;
            if (deviceType == CL_DEVICE_TYPE_GPU)
                deviceName = "GPU/OpenCL: " + deviceName;
            else
                deviceName = "CPU/OpenCL: " + deviceName;
            return true;
        }
    }
    return false;
}

class OpenCLBackend final : public IBackend {
public:
    OpenCLBackend(cl_platform_id platform, cl_device_id device, std::string deviceName)
        : device(device), description(std::move(deviceName)) {
        cl_int err = CL_SUCCESS;
        cl_context_properties props[] = {
            CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(platform),
            0
        };
        context = clCreateContext(props, 1, &device, nullptr, nullptr, &err);
        if (err != CL_SUCCESS) throw std::runtime_error("clCreateContext failed");

        queue = clCreateCommandQueue(context, device, 0, &err);
        if (err != CL_SUCCESS) throw std::runtime_error("clCreateCommandQueue failed");

        const char *src = kKernelSource;
        program = clCreateProgramWithSource(context, 1, &src, nullptr, &err);
        if (err != CL_SUCCESS) throw std::runtime_error("clCreateProgramWithSource failed");

        if (clBuildProgram(program, 1, &device, "-cl-std=CL1.2", nullptr, nullptr) != CL_SUCCESS) {
            size_t logSize = 0;
            clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
            std::string log(logSize, '\0');
            clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, log.data(), nullptr);
            std::cerr << "OpenCL build log:\n" << log << "\n";
            throw std::runtime_error("clBuildProgram failed");
        }

        kernel = clCreateKernel(program, "ff_step", &err);
        if (err != CL_SUCCESS) throw std::runtime_error("clCreateKernel failed");

        std::random_device rd;
        seedCounter = rd();
    }

    ~OpenCLBackend() override {
        if (bufferA) clReleaseMemObject(bufferA);
        if (bufferB) clReleaseMemObject(bufferB);
        if (kernel) clReleaseKernel(kernel);
        if (program) clReleaseProgram(program);
        if (queue) clReleaseCommandQueue(queue);
        if (context) clReleaseContext(context);
    }

    std::string name() const override { return description; }
    BackendKind kind() const override { return BackendKind::GPU; }

    void prepare(int w, int h) override {
        if (w == width && h == height && bufferA && bufferB)
            return;

        if (bufferA) {
            clReleaseMemObject(bufferA);
            bufferA = nullptr;
        }
        if (bufferB) {
            clReleaseMemObject(bufferB);
            bufferB = nullptr;
        }

        width = w;
        height = h;
        flat.assign(static_cast<size_t>(w) * h, EMPTY);

        cl_int err = CL_SUCCESS;
        bufferA = clCreateBuffer(context, CL_MEM_READ_WRITE, flat.size(), nullptr, &err);
        if (err != CL_SUCCESS) throw std::runtime_error("clCreateBuffer A failed");
        bufferB = clCreateBuffer(context, CL_MEM_READ_WRITE, flat.size(), nullptr, &err);
        if (err != CL_SUCCESS) throw std::runtime_error("clCreateBuffer B failed");
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

        if (clEnqueueWriteBuffer(queue, bufferA, CL_TRUE, 0, flat.size(), flat.data(), 0, nullptr, nullptr) != CL_SUCCESS)
            throw std::runtime_error("clEnqueueWriteBuffer failed");

        const float pf = static_cast<float>(p);
        const float gf = static_cast<float>(g);
        const cl_int useMoore = (logic == MOORE) ? 1 : 0;
        const cl_uint seed = ++seedCounter;

        clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufferA);
        clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufferB);
        clSetKernelArg(kernel, 2, sizeof(cl_float), &pf);
        clSetKernelArg(kernel, 3, sizeof(cl_float), &gf);
        clSetKernelArg(kernel, 4, sizeof(cl_int), &w);
        clSetKernelArg(kernel, 5, sizeof(cl_int), &h);
        clSetKernelArg(kernel, 6, sizeof(cl_int), &useMoore);
        clSetKernelArg(kernel, 7, sizeof(cl_uint), &seed);

        const size_t global[2] = {static_cast<size_t>(w), static_cast<size_t>(h)};
        if (clEnqueueNDRangeKernel(queue, kernel, 2, nullptr, global, nullptr, 0, nullptr, nullptr) != CL_SUCCESS)
            throw std::runtime_error("clEnqueueNDRangeKernel failed");

        if (clEnqueueReadBuffer(queue, bufferB, CL_TRUE, 0, flat.size(), flat.data(), 0, nullptr, nullptr) != CL_SUCCESS)
            throw std::runtime_error("clEnqueueReadBuffer failed");

        unflatten(grid);
    }

private:
    void flatten(const ForestGrid &grid) {
        for (int x = 0; x < width; ++x)
            std::memcpy(flat.data() + static_cast<size_t>(x) * height, grid[x].data(), height);
    }

    void unflatten(ForestGrid &grid) const {
        for (int x = 0; x < width; ++x)
            std::memcpy(grid[x].data(), flat.data() + static_cast<size_t>(x) * height, height);
    }

    cl_device_id device{};
    cl_context context{};
    cl_command_queue queue{};
    cl_program program{};
    cl_kernel kernel{};
    cl_mem bufferA{};
    cl_mem bufferB{};
    int width{0};
    int height{0};
    std::vector<uint8_t> flat;
    uint32_t seedCounter{0};
    std::string description;
};

struct GPUProbe {
    bool available{false};
    cl_platform_id platform{};
    cl_device_id device{};
    std::string description;

    GPUProbe() {
        available = pickPlatformAndDevice(platform, device, description);
        if (!available)
            description = "No OpenCL device found";
    }
};

const GPUProbe &probe() {
    static GPUProbe p;
    return p;
}

}

bool openclBackendAvailable() {
    return probe().available;
}

std::string openclBackendDescription() {
    return probe().description;
}

std::unique_ptr<IBackend> makeOpenCLBackend() {
    if (!probe().available)
        return nullptr;
    try {
        return std::make_unique<OpenCLBackend>(probe().platform, probe().device, probe().description);
    } catch (const std::exception &ex) {
        std::cerr << "OpenCL backend init failed: " << ex.what() << "\n";
        return nullptr;
    }
}

#endif
