#include <chrono>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <vector>

#include <omp.h>

const int SIZE = 1;
const int WIDTH = 1024;
const int HEIGHT = 1024;

const double START_GROWTH = 0.5;

const float DEFAULT_FIRE = 0.0001;
const float DEFAULT_GROWTH = 0.03;

const bool SPEED_CONTROL = false;
const bool STEP_ANIMATION = false;

const float FPS_LIMIT = 15.0f;

const ImVec4 CLEAR_COLOR = {0.94f, 0.94f, 0.94f, 1.00f};

const SDL_Color DEFAULT_TREE_COLOR = {0, 128, 0, 255};
const SDL_Color DEFAULT_FIRE_COLOR = {200, 0, 0, 255};

const int MEASUREMENT_STEPS[] = {1, 10, 100, 1000, 10000};

const ImVec4 RESET_TREE_COLOR = {static_cast<float>(DEFAULT_TREE_COLOR.r / 255.0), static_cast<float>(DEFAULT_TREE_COLOR.g / 255.0),
                                 static_cast<float>(DEFAULT_TREE_COLOR.b / 255.0), static_cast<float>(DEFAULT_TREE_COLOR.a / 255.0)};

const ImVec4 RESET_FIRE_COLOR = {static_cast<float>(DEFAULT_FIRE_COLOR.r / 255.0), static_cast<float>(DEFAULT_FIRE_COLOR.g / 255.0),
                                 static_cast<float>(DEFAULT_FIRE_COLOR.b / 255.0), static_cast<float>(DEFAULT_FIRE_COLOR.a / 255.0)};

enum CellState {
    TREE, FIRE, EMPTY
};

enum NeighborhoodLogic {
    MOORE,
    VON_NEUMANN
};

SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;

NeighborhoodLogic currentLogic{VON_NEUMANN};

bool running{true};
bool startMeasure{false};

bool settingsWindow{false};
bool measurementWindow{false};
bool colorWindow{false};

int currentStep{0};
int maxSteps{0};

float progressAllSteps{static_cast<float>(std::accumulate(std::begin(MEASUREMENT_STEPS), std::end(MEASUREMENT_STEPS), 0))};
float progressCurrentStep{0.0};

float fire{DEFAULT_FIRE};
float growth{DEFAULT_GROWTH};

int currentSize{SIZE};
int currentWidth{WIDTH};
int currentHeight{HEIGHT};

float currentSpeed{FPS_LIMIT};
bool limitAnimation{SPEED_CONTROL};

bool stepwiseAnimation{STEP_ANIMATION};
bool animationStep{STEP_ANIMATION};

unsigned int lastUpdate;

ImVec4 clearColor{CLEAR_COLOR};

std::vector<std::vector<CellState>> forest(currentWidth, std::vector<CellState>(currentHeight, EMPTY));

std::priority_queue<int, std::vector<int>, std::greater<>> measureSteps;

void initForest() {
    auto maxThreads = omp_get_max_threads();
    std::random_device rd;
    std::vector<std::mt19937> rngs;
    rngs.reserve(maxThreads);
    for (int i = 0; i < maxThreads; ++i)
        rngs.emplace_back(rd());

#pragma omp parallel for collapse(2) default(none) shared(forest, rngs, currentWidth, currentHeight, START_GROWTH)
    for (int i = 0; i < currentWidth; ++i)
        for (int j = 0; j < currentHeight; ++j) {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            forest[i][j] = (dist(rngs[omp_get_thread_num()]) < START_GROWTH) ? TREE : EMPTY;
        }
}


void drawSquare(int x, int y, SDL_Color color) {
    SDL_Rect fillRect = {x * currentSize, y * currentSize, currentSize, currentSize};
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &fillRect);
}

bool isFireNearby(int x, int y, NeighborhoodLogic logic) {
    // Von Neumann neighborhood: up, down, left, right
    if ((x > 0 && forest[x - 1][y] == FIRE) ||
        (y > 0 && forest[x][y - 1] == FIRE) ||
        (x < currentWidth - 1 && forest[x + 1][y] == FIRE) ||
        (y < currentHeight - 1 && forest[x][y + 1] == FIRE))
        return true;

    // If Moore neighborhood is not selected, or we found fire in Von Neumann neighborhood, no need to check further.
    if (logic == VON_NEUMANN)
        return false;

    // Moore neighborhood: also consider diagonals
    return (x > 0 && y > 0 && forest[x - 1][y - 1] == FIRE) ||
           (x < currentWidth - 1 && y > 0 && forest[x + 1][y - 1] == FIRE) ||
           (x > 0 && y < currentHeight - 1 && forest[x - 1][y + 1] == FIRE) ||
           (x < currentWidth - 1 && y < currentHeight - 1 && forest[x + 1][y + 1] == FIRE);
}

void stepForest(double p, double g) {
    static std::vector<std::vector<CellState>> nextForest(currentWidth, std::vector<CellState>(currentHeight, EMPTY));
    if (static_cast<int>(nextForest.size()) != currentWidth ||
        static_cast<int>(nextForest[0].size()) != currentHeight)
        nextForest.assign(currentWidth, std::vector<CellState>(currentHeight, EMPTY));

    static std::vector<std::mt19937> randomGens = [] {
        auto maxThreads = omp_get_max_threads();
        std::vector<std::mt19937> gens;
        gens.reserve(maxThreads);
        std::random_device rd;
        for (int i = 0; i < maxThreads; ++i)
            gens.emplace_back(rd());
        return gens;
    }();

#pragma omp parallel for collapse(2) default(none) shared(forest, nextForest, currentLogic, p, g, randomGens, currentWidth, currentHeight)
    for (int x = 0; x < currentWidth; ++x) {
        for (int y = 0; y < currentHeight; ++y) {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            std::mt19937 &rngLocal = randomGens[omp_get_thread_num()];

            switch (forest[x][y]) {
                case FIRE:
                    nextForest[x][y] = EMPTY;
                    break;
                case TREE:
                    nextForest[x][y] = (isFireNearby(x, y, currentLogic) || dist(rngLocal) < p) ? FIRE : TREE;
                    break;
                case EMPTY:
                    nextForest[x][y] = (dist(rngLocal) < g) ? TREE : EMPTY;
                    break;
            }
        }
    }

    forest.swap(nextForest);
}

void resetMeasure() {
    startMeasure = false;
    currentStep = 0;
    progressCurrentStep = 0;
    maxSteps = 0;
}

void nextSteps() {
    currentStep = 0;
    maxSteps = measureSteps.top();
    measureSteps.pop();
}