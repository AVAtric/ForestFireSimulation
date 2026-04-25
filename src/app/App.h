#pragma once

#include <SDL.h>
#include <imgui.h>

#include <memory>
#include <queue>
#include <vector>

#include "Backend.h"
#include "Forest.h"

// --- Configuration -----------------------------------------------------------
extern const int SIZE;
extern const int WIDTH;
extern const int HEIGHT;

extern const double START_GROWTH;
extern const float DEFAULT_FIRE;
extern const float DEFAULT_GROWTH;

extern const bool SPEED_CONTROL;
extern const bool STEP_ANIMATION;
extern const float FPS_LIMIT;

extern const ImVec4 CLEAR_COLOR;
extern const SDL_Color DEFAULT_TREE_COLOR;
extern const SDL_Color DEFAULT_FIRE_COLOR;
extern const ImVec4 RESET_TREE_COLOR;
extern const ImVec4 RESET_FIRE_COLOR;

extern const int MEASUREMENT_STEPS[5];

// --- Runtime state -----------------------------------------------------------
extern SDL_Window *window;
extern SDL_Renderer *renderer;

extern NeighborhoodLogic currentLogic;

extern bool running;
extern bool startMeasure;

extern bool settingsWindow;
extern bool measurementWindow;
extern bool colorWindow;

extern int currentStep;
extern int maxSteps;

extern float progressAllSteps;
extern float progressCurrentStep;

extern float fire;
extern float growth;

extern int currentSize;
extern int currentWidth;
extern int currentHeight;

extern float currentSpeed;
extern bool limitAnimation;

extern bool stepwiseAnimation;
extern bool animationStep;

extern unsigned int lastUpdate;

extern ImVec4 clearColor;

extern ForestGrid forest;
extern std::priority_queue<int, std::vector<int>, std::greater<>> measureSteps;

extern std::unique_ptr<IBackend> backend;
extern BackendKind requestedBackend;

// --- Helpers -----------------------------------------------------------------
void drawSquare(int x, int y, SDL_Color color);
void resetMeasure();
void nextSteps();
