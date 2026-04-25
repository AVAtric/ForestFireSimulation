# ForestFire

An interactive simulation of the classic *forest-fire* cellular automaton, rendered with SDL2 and Dear ImGui. The simulation step is pluggable: choose between an OpenMP CPU backend and a GPU backend (Metal on macOS, OpenCL on Linux/Windows) at runtime.

Each cell on a 2D grid is in one of three states:

| State   | Meaning                       |
| ------- | ----------------------------- |
| `EMPTY` | bare ground, may regrow       |
| `TREE`  | living tree, may catch fire   |
| `FIRE`  | burning tree, becomes `EMPTY` |

At every step, for each cell:

- a `FIRE` becomes `EMPTY`,
- a `TREE` becomes `FIRE` if a neighbour is on fire **or** with probability `f` (spontaneous lightning),
- an `EMPTY` cell becomes a `TREE` with probability `g` (regrowth).

Neighbourhoods can be switched between **Von Neumann** (4 cells) and **Moore** (8 cells).

## Features

- Real-time rendering of grids up to 1024 × 1024.
- **Pluggable compute backends**, selectable at runtime in the Settings window:
  - **OpenMP** — multi-threaded CPU step using a per-thread Mersenne-Twister RNG. Always available.
  - **GPU** — same step running as a compute kernel. The dispatcher tries each available backend in order and picks the first one that initialises:
    1. **Metal** (Apple platforms) — preferred on macOS; future-proof since Apple has deprecated OpenCL.
    2. **OpenCL** (Linux/Windows, and as a fallback on macOS) — uses the OpenCL ICD provided by your GPU vendor (NVIDIA / AMD / Intel / Mesa Rusticl / pocl, etc.).
  - Falls back to OpenMP when no GPU device is detected.
- OS detection at startup — the GPU option is disabled (greyed-out) in the UI on systems where no GPU device was found, and the device's name is shown next to the combo.
- Adjustable parameters at runtime: grid size, zoom, fire probability, growth probability, neighbourhood logic, backend.
- Animation controls: free-running, FPS-limited, or stepwise (manual single-step).
- Click any tree in the window to ignite it.
- Customisable tree / fire colours.
- Built-in benchmark mode that times 1, 10, 100, 1 000 and 10 000 steps and logs the durations — useful for comparing OpenMP vs. GPU.

## Build

### Prerequisites

- CMake ≥ 3.23 and a C++20 compiler.
- **SDL2** (development headers/library).
  - macOS: install the official `SDL2.framework` into `/Library/Frameworks/`.
  - Linux: `sudo apt install libsdl2-dev` (Debian/Ubuntu) or your distro's equivalent.
  - Windows: `vcpkg install sdl2` or grab the official SDL2 development package.
- **OpenMP**.
  - macOS / Apple Clang: `brew install libomp`.
  - Linux/GCC and Linux/Clang: shipped by default.
  - Windows/MSVC: shipped by default; Clang-CL needs `clang_rt.openmp` from LLVM.
- **Metal** (optional, macOS only — required for the Metal GPU backend).
  - Bundled with macOS — no install needed. The build automatically links `Metal.framework` and `Foundation.framework` and compiles `MetalBackend.mm` as Objective-C++ with ARC.
- **OpenCL** (optional — required only for the OpenCL GPU backend).
  - macOS: bundled with the OS (deprecated since 10.14 but still functional through current macOS releases). Used only as a fallback if Metal is disabled or unavailable.
  - Linux: vendor ICD package (`nvidia-opencl-icd`, `rocm-opencl-runtime`, `intel-opencl-icd`, `mesa-opencl-icd`, `pocl`, …) plus headers (`opencl-headers`, `ocl-icd-opencl-dev`).
  - Windows: any GPU driver that registers an OpenCL ICD (most modern NVIDIA / AMD / Intel drivers do). Headers via the OpenCL SDK or `vcpkg install opencl`.

Dear ImGui (v1.89.9, SDL2 + SDL_Renderer backends) is fetched automatically by CMake.

### Build options

| Option                     | Default | Effect                                                                                          |
| -------------------------- | :-----: | ----------------------------------------------------------------------------------------------- |
| `FORESTFIRE_BUILD_TESTS`   |  `ON`   | Build the unit tests (registered with CTest).                                                   |
| `FORESTFIRE_ENABLE_METAL`  |  `ON`   | Enable the Metal GPU backend on Apple platforms. No effect on Linux/Windows.                    |
| `FORESTFIRE_ENABLE_OPENCL` |  `ON`   | Enable the OpenCL GPU backend if `find_package(OpenCL)` succeeds. Disabling forces OpenMP-only. |

### Build & run

```sh
cmake -S . -B build
cmake --build build
./build/ForestFire
```

During configuration you'll see lines such as:

```
-- OpenCL found - OpenCL backend enabled (3.0)
-- Metal enabled - Metal backend will be preferred on macOS
```

If OpenCL is missing on a non-Apple platform you'll get `-- OpenCL not found - OpenCL backend disabled`; the build still succeeds, just without GPU support there.

### Run the tests

```sh
cmake --build build --target forestfire_tests
./build/tests/forestfire_tests
# or, equivalently:
ctest --test-dir build --output-on-failure
```

The suite includes per-backend parity tests (`metal_matches_openmp_*`, `opencl_matches_openmp_*`) that run the same step on the GPU and on OpenMP and assert identical output — each is skipped automatically when its backend is not built or no device is present. Non-OpenMP CPU tests are always run.

## Usage

| Action                                 | How                              |
| -------------------------------------- | -------------------------------- |
| Ignite a tree                          | Left-click on a green cell       |
| Open settings / measurements / colours | `View` menu                      |
| Switch compute backend                 | Settings → `Backend` combo       |
| Reset the forest                       | `File → Reset`                   |
| Exit                                   | `File → Exit` or close the window |

The **Settings** window exposes width, height, zoom, the two probabilities, the neighbourhood logic, the **backend selector** (with the detected GPU description), an FPS limiter, and a step-by-step mode.

The **Measurements** window shows live FPS / average FPS / per-frame performance counter, runs a benchmark suite (Start) over the steps configured in `MEASUREMENT_STEPS`, and logs each backend switch.

## Architecture

The simulation step is hidden behind a small interface so the rest of the app doesn't care which device runs the kernel:

```
            ┌────────────────────────────────────────┐
main.cpp ──►│  IBackend                              │
            │    prepare(width, height)              │
            │    seed(grid, startGrowth)             │
            │    step(grid, p, g, logic)             │
            └──────────┬─────────────────────────────┘
                       │
        ┌──────────────┼───────────────────────────────┐
        ▼              ▼                               ▼
  OpenMPBackend   MetalBackend                  OpenCLBackend
  (parallel for   (MTLComputePipelineState,     (cl_kernel,
   over the grid, threadgroups,                  hash-based PRNG)
   per-thread     hash-based PRNG)
   RNG)
```

A small dispatcher (`GPUBackend.cpp`) handles GPU selection:

```cpp
bool gpuBackendAvailable() {
    if (metalBackendAvailable()) return true;     // preferred on macOS
    if (openclBackendAvailable()) return true;    // fallback / Linux+Windows
    return false;
}

std::unique_ptr<IBackend> makeGPUBackend() {
    if (auto b = makeMetalBackend()) return b;
    if (auto b = makeOpenCLBackend()) return b;
    return nullptr;
}
```

Each per-backend probe (`MTLCreateSystemDefaultDevice` for Metal, `clGetPlatformIDs` for OpenCL) runs once at startup and feeds:

- the default backend choice (always OpenMP),
- whether the GPU option is enabled in the Settings combo,
- the device description shown to the user.

## Project layout

```
.
├── CMakeLists.txt          – top-level build, options, multiplatform SDL2/OpenMP/OpenCL
├── src/
│   ├── core/               – simulation library (no SDL/ImGui), built as ForestFireCore
│   │   ├── Forest.h        – shared types (CellState, NeighborhoodLogic, BackendKind, ForestGrid)
│   │   ├── Backend.h       – IBackend interface + factory functions
│   │   ├── OpenMPBackend.cpp  – CPU/OpenMP implementation
│   │   ├── MetalBackend.mm    – Metal implementation (Apple only, ObjC++ with ARC)
│   │   ├── OpenCLBackend.cpp  – OpenCL implementation (only active when OpenCL is found)
│   │   └── GPUBackend.cpp     – picks the preferred GPU backend at runtime
│   └── app/                – SDL/ImGui frontend, links against ForestFireCore
│       ├── App.h / App.cpp           – application/UI state (window, grid, current settings)
│       ├── GUI.h / GUI.cpp           – ImGui windows (settings, menu, backend combo)
│       ├── MeasurementsLog.h /
│       │   MeasurementsLog.cpp       – ImGui log widget for benchmark output
│       └── main.cpp                  – SDL/ImGui main loop
├── tests/
│   ├── CMakeLists.txt      – test target registered with CTest
│   ├── test_framework.h    – tiny header-only assertion + runner
│   └── test_simulation.cpp – neighbourhood, backend transitions, OpenMP/GPU parity
└── vendor/imgui            – Dear ImGui (fetched by CMake)
```

The simulation core under `src/core/` is built as a static library `ForestFireCore` and is the only thing the tests link against — no SDL2 or ImGui. The application code under `src/app/` links the core and adds the SDL/ImGui frontend.

## License

Released under the [MIT License](LICENSE).
