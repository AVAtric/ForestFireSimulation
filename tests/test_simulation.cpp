#include "Backend.h"
#include "Forest.h"
#include "test_framework.h"

#include <algorithm>

namespace {

ForestGrid makeGrid(int w, int h, CellState fill = EMPTY) {
    return ForestGrid(w, std::vector<CellState>(h, fill));
}

int countState(const ForestGrid &g, CellState s) {
    int n = 0;
    for (const auto &col: g)
        for (auto cell: col)
            if (cell == s) ++n;
    return n;
}

}

TEST(neighbour_detects_orthogonal_fire_von_neumann) {
    auto g = makeGrid(3, 3, EMPTY);
    g[1][0] = FIRE;
    EXPECT_TRUE(isFireNeighbour(g, 1, 1, 3, 3, VON_NEUMANN));
    g[1][0] = EMPTY;
    g[2][1] = FIRE;
    EXPECT_TRUE(isFireNeighbour(g, 1, 1, 3, 3, VON_NEUMANN));
}

TEST(neighbour_ignores_diagonal_fire_von_neumann) {
    auto g = makeGrid(3, 3, EMPTY);
    g[0][0] = FIRE;
    EXPECT_TRUE(!isFireNeighbour(g, 1, 1, 3, 3, VON_NEUMANN));
}

TEST(neighbour_detects_diagonal_fire_moore) {
    auto g = makeGrid(3, 3, EMPTY);
    g[0][0] = FIRE;
    EXPECT_TRUE(isFireNeighbour(g, 1, 1, 3, 3, MOORE));
    g[0][0] = EMPTY;
    g[2][2] = FIRE;
    EXPECT_TRUE(isFireNeighbour(g, 1, 1, 3, 3, MOORE));
}

TEST(neighbour_handles_corners) {
    auto g = makeGrid(3, 3, EMPTY);
    g[0][1] = FIRE;
    EXPECT_TRUE(isFireNeighbour(g, 0, 0, 3, 3, VON_NEUMANN));
    g[0][1] = EMPTY;
    g[1][2] = FIRE;
    EXPECT_TRUE(isFireNeighbour(g, 2, 2, 3, 3, VON_NEUMANN));
}

TEST(openmp_step_burns_fire_to_empty) {
    auto backend = makeOpenMPBackend();
    backend->prepare(4, 4);
    auto g = makeGrid(4, 4, EMPTY);
    g[1][1] = FIRE;
    g[2][2] = FIRE;
    backend->step(g, 0.0, 0.0, VON_NEUMANN);
    EXPECT_EQ(countState(g, FIRE), 0);
    EXPECT_EQ(g[1][1], EMPTY);
    EXPECT_EQ(g[2][2], EMPTY);
}

TEST(openmp_step_no_growth_no_fire_keeps_empty) {
    auto backend = makeOpenMPBackend();
    backend->prepare(4, 4);
    auto g = makeGrid(4, 4, EMPTY);
    backend->step(g, 0.0, 0.0, VON_NEUMANN);
    EXPECT_EQ(countState(g, EMPTY), 16);
}

TEST(openmp_step_p1_burns_all_trees) {
    auto backend = makeOpenMPBackend();
    backend->prepare(4, 4);
    auto g = makeGrid(4, 4, TREE);
    backend->step(g, 1.0, 0.0, VON_NEUMANN);
    EXPECT_EQ(countState(g, FIRE), 16);
}

TEST(openmp_step_g1_grows_all_empty) {
    auto backend = makeOpenMPBackend();
    backend->prepare(4, 4);
    auto g = makeGrid(4, 4, EMPTY);
    backend->step(g, 0.0, 1.0, VON_NEUMANN);
    EXPECT_EQ(countState(g, TREE), 16);
}

TEST(openmp_fire_spreads_to_orthogonal_tree) {
    auto backend = makeOpenMPBackend();
    backend->prepare(3, 3);
    auto g = makeGrid(3, 3, TREE);
    g[1][1] = FIRE;
    backend->step(g, 0.0, 0.0, VON_NEUMANN);

    EXPECT_EQ(g[1][1], EMPTY);
    EXPECT_EQ(g[0][1], FIRE);
    EXPECT_EQ(g[2][1], FIRE);
    EXPECT_EQ(g[1][0], FIRE);
    EXPECT_EQ(g[1][2], FIRE);
    EXPECT_EQ(g[0][0], TREE);
    EXPECT_EQ(g[2][2], TREE);
}

TEST(openmp_fire_spreads_to_diagonal_in_moore) {
    auto backend = makeOpenMPBackend();
    backend->prepare(3, 3);
    auto g = makeGrid(3, 3, TREE);
    g[1][1] = FIRE;
    backend->step(g, 0.0, 0.0, MOORE);

    EXPECT_EQ(g[0][0], FIRE);
    EXPECT_EQ(g[2][2], FIRE);
    EXPECT_EQ(g[0][2], FIRE);
    EXPECT_EQ(g[2][0], FIRE);
}

TEST(openmp_seed_respects_growth_probability) {
    auto backend = makeOpenMPBackend();
    backend->prepare(64, 64);
    auto g = makeGrid(64, 64, EMPTY);
    backend->seed(g, 0.5);
    int trees = countState(g, TREE);
    EXPECT_TRUE(trees > 64 * 64 / 4);
    EXPECT_TRUE(trees < 3 * 64 * 64 / 4);
}

namespace {

void verifyParityWithCpu(IBackend &gpu, NeighborhoodLogic logic) {
    auto cpu = makeOpenMPBackend();
    cpu->prepare(8, 8);
    gpu.prepare(8, 8);

    auto a = makeGrid(8, 8, TREE);
    a[3][3] = FIRE;
    a[0][7] = FIRE;
    auto b = a;

    cpu->step(a, 0.0, 0.0, logic);
    gpu.step(b, 0.0, 0.0, logic);

    for (int x = 0; x < 8; ++x)
        for (int y = 0; y < 8; ++y)
            EXPECT_EQ(a[x][y], b[x][y]);
}

}

TEST(gpu_dispatcher_returns_a_backend_when_available) {
    if (!gpuBackendAvailable()) {
        std::cout << "  (skipped: " << gpuBackendDescription() << ")\n";
        return;
    }
    auto gpu = makeGPUBackend();
    EXPECT_TRUE(gpu != nullptr);
    EXPECT_TRUE(gpu->kind() == BackendKind::GPU);
}

#ifdef FORESTFIRE_HAS_OPENCL
TEST(opencl_matches_openmp_von_neumann) {
    if (!openclBackendAvailable()) {
        std::cout << "  (skipped: " << openclBackendDescription() << ")\n";
        return;
    }
    auto gpu = makeOpenCLBackend();
    if (!gpu) return;
    verifyParityWithCpu(*gpu, VON_NEUMANN);
}

TEST(opencl_matches_openmp_moore) {
    if (!openclBackendAvailable()) return;
    auto gpu = makeOpenCLBackend();
    if (!gpu) return;
    verifyParityWithCpu(*gpu, MOORE);
}

TEST(opencl_burns_fire) {
    if (!openclBackendAvailable()) return;
    auto gpu = makeOpenCLBackend();
    if (!gpu) return;
    gpu->prepare(4, 4);
    auto g = makeGrid(4, 4, EMPTY);
    g[1][1] = FIRE;
    gpu->step(g, 0.0, 0.0, VON_NEUMANN);
    EXPECT_EQ(g[1][1], EMPTY);
}
#endif

#ifdef FORESTFIRE_HAS_METAL
TEST(metal_matches_openmp_von_neumann) {
    if (!metalBackendAvailable()) {
        std::cout << "  (skipped: " << metalBackendDescription() << ")\n";
        return;
    }
    auto gpu = makeMetalBackend();
    if (!gpu) return;
    verifyParityWithCpu(*gpu, VON_NEUMANN);
}

TEST(metal_matches_openmp_moore) {
    if (!metalBackendAvailable()) return;
    auto gpu = makeMetalBackend();
    if (!gpu) return;
    verifyParityWithCpu(*gpu, MOORE);
}

TEST(metal_burns_fire) {
    if (!metalBackendAvailable()) return;
    auto gpu = makeMetalBackend();
    if (!gpu) return;
    gpu->prepare(4, 4);
    auto g = makeGrid(4, 4, EMPTY);
    g[1][1] = FIRE;
    gpu->step(g, 0.0, 0.0, VON_NEUMANN);
    EXPECT_EQ(g[1][1], EMPTY);
}

TEST(metal_fire_spreads_to_orthogonal_tree) {
    if (!metalBackendAvailable()) return;
    auto gpu = makeMetalBackend();
    if (!gpu) return;
    gpu->prepare(3, 3);
    auto g = makeGrid(3, 3, TREE);
    g[1][1] = FIRE;
    gpu->step(g, 0.0, 0.0, VON_NEUMANN);
    EXPECT_EQ(g[1][1], EMPTY);
    EXPECT_EQ(g[0][1], FIRE);
    EXPECT_EQ(g[2][1], FIRE);
    EXPECT_EQ(g[1][0], FIRE);
    EXPECT_EQ(g[1][2], FIRE);
}
#endif

int main() {
    return ::tf::run();
}
