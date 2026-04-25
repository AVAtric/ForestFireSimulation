cmake_minimum_required(VERSION 3.23)
project(ForestFire LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FORESTFIRE_BUILD_TESTS "Build ForestFire unit tests" ON)
option(FORESTFIRE_ENABLE_OPENCL "Enable OpenCL GPU backend if found" ON)
option(FORESTFIRE_ENABLE_METAL "Enable Metal GPU backend on Apple platforms" ON)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -fopenmp")
endif ()

include(FindPkgConfig)

# --- Simulation core (no SDL/ImGui, used by tests too) -----------------------
set(FORESTFIRE_CORE_SOURCES
        src/core/OpenMPBackend.cpp
        src/core/OpenCLBackend.cpp
        src/core/GPUBackend.cpp)

if (APPLE AND FORESTFIRE_ENABLE_METAL)
    enable_language(OBJCXX)
    list(APPEND FORESTFIRE_CORE_SOURCES src/core/MetalBackend.mm)
endif ()

add_library(ForestFireCore STATIC ${FORESTFIRE_CORE_SOURCES})
target_include_directories(ForestFireCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/core)

if (MSVC)
    target_compile_options(ForestFireCore PRIVATE /openmp:llvm)
endif ()

find_package(OpenMP)
if (OpenMP_CXX_FOUND)
    target_link_libraries(ForestFireCore PUBLIC OpenMP::OpenMP_CXX)
endif ()

if (FORESTFIRE_ENABLE_OPENCL)
    find_package(OpenCL QUIET)
    if (OpenCL_FOUND)
        message(STATUS "OpenCL found - OpenCL backend enabled (${OpenCL_VERSION_STRING})")
        target_compile_definitions(ForestFireCore PUBLIC FORESTFIRE_HAS_OPENCL)
        target_link_libraries(ForestFireCore PUBLIC OpenCL::OpenCL)
    else ()
        message(STATUS "OpenCL not found - OpenCL backend disabled")
    endif ()
endif ()

if (APPLE AND FORESTFIRE_ENABLE_METAL)
    message(STATUS "Metal enabled - Metal backend will be preferred on macOS")
    set_source_files_properties(src/core/MetalBackend.mm PROPERTIES
            COMPILE_FLAGS "-fobjc-arc"
            COMPILE_OPTIONS "-Wno-unused-command-line-argument")
    target_compile_definitions(ForestFireCore PUBLIC FORESTFIRE_HAS_METAL)
    target_link_libraries(ForestFireCore PUBLIC
            "-framework Metal"
            "-framework Foundation")
endif ()

# --- Application -------------------------------------------------------------
add_executable(${PROJECT_NAME}
        src/app/main.cpp
        src/app/App.cpp
        src/app/GUI.cpp
        src/app/MeasurementsLog.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app)
target_link_libraries(${PROJECT_NAME} PRIVATE ForestFireCore)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(SDL2_LIBRARIES "/Library/Frameworks/SDL2.framework/SDL2")
    set(SDL2_INCLUDE_DIRS "/Library/Frameworks/SDL2.framework/Headers")
else ()
    find_package(SDL2 REQUIRED)
    if (TARGET SDL2::SDL2)
        set(SDL2_LIBRARIES SDL2::SDL2)
    endif ()
endif ()

target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES} imgui)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX _USE_MATH_DEFINES)
endif ()

add_subdirectory(vendor)

# --- Tests -------------------------------------------------------------------
if (FORESTFIRE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
