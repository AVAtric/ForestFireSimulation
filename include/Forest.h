#pragma once

#include <cstdint>
#include <vector>

enum CellState : uint8_t {
    TREE = 0,
    FIRE = 1,
    EMPTY = 2
};

enum NeighborhoodLogic : uint8_t {
    MOORE = 0,
    VON_NEUMANN = 1
};

enum class BackendKind : uint8_t {
    OpenMP = 0,
    GPU = 1
};

using ForestGrid = std::vector<std::vector<CellState>>;

inline bool isFireNeighbour(const ForestGrid &grid, int x, int y, int width, int height, NeighborhoodLogic logic) {
    if ((x > 0 && grid[x - 1][y] == FIRE) ||
        (y > 0 && grid[x][y - 1] == FIRE) ||
        (x < width - 1 && grid[x + 1][y] == FIRE) ||
        (y < height - 1 && grid[x][y + 1] == FIRE))
        return true;

    if (logic == VON_NEUMANN)
        return false;

    return (x > 0 && y > 0 && grid[x - 1][y - 1] == FIRE) ||
           (x < width - 1 && y > 0 && grid[x + 1][y - 1] == FIRE) ||
           (x > 0 && y < height - 1 && grid[x - 1][y + 1] == FIRE) ||
           (x < width - 1 && y < height - 1 && grid[x + 1][y + 1] == FIRE);
}
