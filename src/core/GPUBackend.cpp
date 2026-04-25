#include "Backend.h"

bool gpuBackendAvailable() {
    if (metalBackendAvailable()) return true;
    if (openclBackendAvailable()) return true;
    return false;
}

std::string gpuBackendDescription() {
    if (metalBackendAvailable()) return metalBackendDescription();
    if (openclBackendAvailable()) return openclBackendDescription();
    return "No GPU backend available";
}

std::unique_ptr<IBackend> makeGPUBackend() {
    if (auto b = makeMetalBackend()) return b;
    if (auto b = makeOpenCLBackend()) return b;
    return nullptr;
}
