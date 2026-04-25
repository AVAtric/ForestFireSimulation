#include "Backend.h"

#ifndef FORESTFIRE_HAS_METAL
bool metalBackendAvailable() { return false; }
std::string metalBackendDescription() { return "Metal disabled at build time"; }
std::unique_ptr<IBackend> makeMetalBackend() { return nullptr; }
#endif

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
