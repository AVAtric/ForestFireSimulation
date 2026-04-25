#include <iostream>
#include <random>
#include <queue>
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

float progressAllSteps(11111.0);
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

unsigned int lastFrame, lastUpdate;

ImVec4 clearColor{CLEAR_COLOR};

std::vector<std::vector<CellState>> forest(currentWidth, std::vector<CellState>(currentHeight, EMPTY));

std::priority_queue<int, std::vector<int>, std::greater<>> measureSteps;

void initForest() {
    std::mt19937 initRng(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_real_distribution<double> dist(0.0, 1.0);

#pragma omp parallel for collapse(2) default(none) shared(forest, START_GROWTH, currentWidth, currentHeight) private(dist, initRng)
    for (int i = 0; i < currentWidth; ++i)
        for (int j = 0; j < currentHeight; ++j)
            forest[i][j] = (dist(initRng) < START_GROWTH) ? TREE : EMPTY;
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
    std::vector<std::vector<CellState>> newForest = forest;

    // Prepare RNGs for each thread
    auto maxThreads = omp_get_max_threads();
    std::vector<std::mt19937> randomGens;
    std::random_device rd;

    for (int i = 0; i < maxThreads; ++i)
        randomGens.emplace_back(rd());

    std::uniform_real_distribution<double> dist(0.0, 1.0);

#pragma omp parallel for collapse(2) default(none) shared(forest, newForest, currentLogic, p, g, randomGens, currentWidth, currentHeight) private(dist)
    for (int x = 0; x < currentWidth; ++x) {
        for (int y = 0; y < currentHeight; ++y) {
            std::mt19937 &rngLocal = randomGens[omp_get_thread_num()];

            if (forest[x][y] == FIRE) {
                newForest[x][y] = EMPTY;
            } else if (forest[x][y] == TREE) {
                bool fireNearby = isFireNearby(x, y, currentLogic);

                if (fireNearby || dist(rngLocal) < p)
                    newForest[x][y] = FIRE;
            } else if (forest[x][y] == EMPTY) {
                if (dist(rngLocal) < g)
                    newForest[x][y] = TREE;
            }
        }
    }

    forest = newForest;
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