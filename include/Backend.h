#pragma once

#include "Forest.h"

#include <memory>
#include <string>

class IBackend {
public:
    virtual ~IBackend() = default;

    virtual std::string name() const = 0;
    virtual BackendKind kind() const = 0;

    virtual void prepare(int width, int height) = 0;

    virtual void seed(ForestGrid &grid, double startGrowth) = 0;

    virtual void step(ForestGrid &grid, double fireProb, double growthProb, NeighborhoodLogic logic) = 0;
};

std::unique_ptr<IBackend> makeOpenMPBackend();

bool openclBackendAvailable();
std::string openclBackendDescription();
std::unique_ptr<IBackend> makeOpenCLBackend();

bool metalBackendAvailable();
std::string metalBackendDescription();
std::unique_ptr<IBackend> makeMetalBackend();

bool gpuBackendAvailable();
std::string gpuBackendDescription();
std::unique_ptr<IBackend> makeGPUBackend();
