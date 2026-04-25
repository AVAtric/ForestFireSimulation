#include "Backend.h"

#include <algorithm>
#include <random>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#else
static inline int omp_get_max_threads() { return 1; }
static inline int omp_get_thread_num() { return 0; }
#endif

namespace {

class OpenMPBackend final : public IBackend {
public:
    std::string name() const override { return "OpenMP"; }
    BackendKind kind() const override { return BackendKind::OpenMP; }

    void prepare(int w, int h) override {
        ensureRngs();
        if (w == width && h == height && !nextGrid.empty())
            return;
        width = w;
        height = h;
        nextGrid.assign(w, std::vector<CellState>(h, EMPTY));
    }

    void seed(ForestGrid &grid, double startGrowth) override {
        ensureRngs();

        const int w = static_cast<int>(grid.size());
        const int h = w > 0 ? static_cast<int>(grid[0].size()) : 0;

#pragma omp parallel for collapse(2) default(none) shared(grid, startGrowth, w, h)
        for (int i = 0; i < w; ++i)
            for (int j = 0; j < h; ++j) {
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                grid[i][j] = (dist(rngs[omp_get_thread_num()]) < startGrowth) ? TREE : EMPTY;
            }
    }

    void step(ForestGrid &grid, double p, double g, NeighborhoodLogic logic) override {
        const int w = static_cast<int>(grid.size());
        const int h = w > 0 ? static_cast<int>(grid[0].size()) : 0;

        if (static_cast<int>(nextGrid.size()) != w ||
            (w > 0 && static_cast<int>(nextGrid[0].size()) != h))
            nextGrid.assign(w, std::vector<CellState>(h, EMPTY));

        ensureRngs();

#pragma omp parallel for collapse(2) default(none) shared(grid, p, g, logic, w, h)
        for (int x = 0; x < w; ++x) {
            for (int y = 0; y < h; ++y) {
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                auto &rng = rngs[omp_get_thread_num()];

                switch (grid[x][y]) {
                    case FIRE:
                        nextGrid[x][y] = EMPTY;
                        break;
                    case TREE:
                        nextGrid[x][y] = (isFireNeighbour(grid, x, y, w, h, logic) || dist(rng) < p) ? FIRE : TREE;
                        break;
                    case EMPTY:
                        nextGrid[x][y] = (dist(rng) < g) ? TREE : EMPTY;
                        break;
                }
            }
        }

        grid.swap(nextGrid);
    }

private:
    void ensureRngs() {
        const int n = std::max(1, omp_get_max_threads());
        if (static_cast<int>(rngs.size()) >= n)
            return;
        std::random_device rd;
        rngs.reserve(n);
        while (static_cast<int>(rngs.size()) < n)
            rngs.emplace_back(rd());
    }

    int width{0};
    int height{0};
    ForestGrid nextGrid;
    std::vector<std::mt19937> rngs;
};

}

std::unique_ptr<IBackend> makeOpenMPBackend() {
    return std::make_unique<OpenMPBackend>();
}
