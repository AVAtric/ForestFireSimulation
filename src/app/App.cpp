#include "App.h"

#include <numeric>

const int SIZE = 1;
const int WIDTH = 1024;
const int HEIGHT = 1024;

const double START_GROWTH = 0.5;

const float DEFAULT_FIRE = 0.0001f;
const float DEFAULT_GROWTH = 0.03f;

const bool SPEED_CONTROL = false;
const bool STEP_ANIMATION = false;

const float FPS_LIMIT = 15.0f;

const ImVec4 CLEAR_COLOR = {0.94f, 0.94f, 0.94f, 1.00f};

const SDL_Color DEFAULT_TREE_COLOR = {0, 128, 0, 255};
const SDL_Color DEFAULT_FIRE_COLOR = {200, 0, 0, 255};

const int MEASUREMENT_STEPS[5] = {1, 10, 100, 1000, 10000};

const ImVec4 RESET_TREE_COLOR = {static_cast<float>(DEFAULT_TREE_COLOR.r / 255.0),
                                 static_cast<float>(DEFAULT_TREE_COLOR.g / 255.0),
                                 static_cast<float>(DEFAULT_TREE_COLOR.b / 255.0),
                                 static_cast<float>(DEFAULT_TREE_COLOR.a / 255.0)};

const ImVec4 RESET_FIRE_COLOR = {static_cast<float>(DEFAULT_FIRE_COLOR.r / 255.0),
                                 static_cast<float>(DEFAULT_FIRE_COLOR.g / 255.0),
                                 static_cast<float>(DEFAULT_FIRE_COLOR.b / 255.0),
                                 static_cast<float>(DEFAULT_FIRE_COLOR.a / 255.0)};

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
float progressCurrentStep{0.0f};

float fire{DEFAULT_FIRE};
float growth{DEFAULT_GROWTH};

int currentSize{SIZE};
int currentWidth{WIDTH};
int currentHeight{HEIGHT};

float currentSpeed{FPS_LIMIT};
bool limitAnimation{SPEED_CONTROL};

bool stepwiseAnimation{STEP_ANIMATION};
bool animationStep{STEP_ANIMATION};

unsigned int lastUpdate{0};

ImVec4 clearColor{CLEAR_COLOR};

ForestGrid forest(currentWidth, std::vector<CellState>(currentHeight, EMPTY));

std::priority_queue<int, std::vector<int>, std::greater<>> measureSteps;

std::unique_ptr<IBackend> backend;
BackendKind requestedBackend{BackendKind::OpenMP};

void drawSquare(int x, int y, SDL_Color color) {
    SDL_Rect fillRect = {x * currentSize, y * currentSize, currentSize, currentSize};
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &fillRect);
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
