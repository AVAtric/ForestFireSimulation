# ForestFire

An interactive simulation of the classic *forest-fire* cellular automaton, rendered with SDL2 and Dear ImGui and parallelised with OpenMP.

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
- Parallel grid update via OpenMP with a per-thread Mersenne-Twister RNG.
- Adjustable parameters at runtime: grid size, zoom, fire probability, growth probability, neighbourhood logic.
- Animation controls: free-running, FPS-limited, or stepwise (manual single-step).
- Click any tree in the window to ignite it.
- Customisable tree / fire colours.
- Built-in benchmark mode that times 1, 10, 100, 1 000 and 10 000 steps and logs the durations.

## Build

### Prerequisites

- CMake ≥ 3.23 and a C++20 compiler.
- SDL2.
  - **macOS**: install the official `SDL2.framework` into `/Library/Frameworks/`.
  - **Linux / Windows**: install SDL2 development packages (`libsdl2-dev`, vcpkg, etc.) — `find_package(SDL2)` is used.
- OpenMP.
  - **macOS / Apple Clang**: install `libomp` (`brew install libomp`).

Dear ImGui (v1.89.9, SDL2 + SDL_Renderer backends) is fetched automatically by CMake.

### Build & run

```sh
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
./cmake-build-debug/ForestFire
```

## Usage

| Action                           | How                                      |
| -------------------------------- | ---------------------------------------- |
| Ignite a tree                    | Left-click on a green cell               |
| Open settings / measurements / colours | `View` menu                        |
| Reset the forest                 | `File → Reset`                           |
| Exit                             | `File → Exit` or close the window        |

The **Settings** window exposes width, height, zoom, the two probabilities, the neighbourhood logic, an FPS limiter, and a step-by-step mode.

The **Measurements** window shows live FPS / average FPS / per-frame performance counter, and runs a benchmark suite (Start) over the steps configured in `MEASUREMENT_STEPS`.

## Project layout

```
.
├── CMakeLists.txt        – top-level build
├── main.cpp              – SDL/ImGui main loop
├── Forest.cpp            – simulation state and step kernel
├── GUI.cpp               – window + ImGui panels
├── MeasurementsLog.cpp   – ImGui log widget for benchmark output
└── vendor/imgui          – Dear ImGui (fetched by CMake)
```

The source files are stitched together via `#include` from `main.cpp`; only `main.cpp` is compiled directly.

## License

No license file is provided. All rights reserved by the author.
