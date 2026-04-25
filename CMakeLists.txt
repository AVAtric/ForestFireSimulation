cmake_minimum_required(VERSION 3.23)
project(ForestFire)

set(CMAKE_CXX_STANDARD 20)
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -fopenmp")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

add_executable(${PROJECT_NAME} main.cpp)

INCLUDE(FindPkgConfig)

find_package(OpenMP REQUIRED)

if (OpenMP_CXX_FOUND)
    target_include_directories(${PROJECT_NAME} PUBLIC OpenMP::OpenMP_CXX_INCLUDE_DIRS)
    target_link_libraries(${PROJECT_NAME} PUBLIC OpenMP::OpenMP_CXX)
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(SDL2_LIBRARIES "/Library/Frameworks/SDL2.framework/SDL2")
    set(SDL2_INCLUDE_DIRS "/Library/Frameworks/SDL2.framework/Headers")
else()
    find_package(SDL2 REQUIRED)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES} imgui)

add_subdirectory(vendor)