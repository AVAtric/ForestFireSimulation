#include <SDL.h>

#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>

#include <chrono>
#include <cmath>

#include "App.h"
#include "GUI.h"
#include "MeasurementsLog.h"

int main(int, char **) {
    auto treeColor = DEFAULT_TREE_COLOR;
    auto fireColor = DEFAULT_FIRE_COLOR;
    auto calculateForest = true;
    auto lastHeight = currentHeight, lastWidth = currentWidth, lastSize = currentSize;
    auto start = std::chrono::high_resolution_clock::now();

    lastUpdate = SDL_GetTicks();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("Error: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    createWindow();

    if (renderer == nullptr) {
        SDL_Log("Error creating SDL_Renderer!");
        return EXIT_FAILURE;
    }

    backend = makeOpenMPBackend();
    requestedBackend = BackendKind::OpenMP;
    backend->prepare(currentWidth, currentHeight);
    backend->seed(forest, START_GROWTH);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    unsigned int totalFrameTicks = 0;
    unsigned int totalFrames = 0;
    while (running) {
        totalFrames++;
        Uint32 startTicks = SDL_GetTicks();
        Uint64 startPerf = SDL_GetPerformanceCounter();
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))
                running = false;
            if (event.type == SDL_MOUSEBUTTONDOWN && !ImGui::GetIO().WantCaptureMouse)
                if (event.button.button == SDL_BUTTON_LEFT) {
                    int x, y;
                    SDL_GetMouseState(&x, &y);

                    auto gridX = x / currentSize, gridY = y / currentSize;

                    if (gridX >= 0 && gridX < currentWidth && gridY >= 0 && gridY < currentHeight)
                        if (forest[gridX][gridY] == TREE)
                            forest[gridX][gridY] = FIRE;
                }
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        mainMenu();

        initSettings(lastHeight, lastWidth, lastSize);

        if ((currentStep == maxSteps) && startMeasure) {
            auto stop = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

            measurements.AddLog("[%s] Time taken for %i steps: %lld ms\n", "info", maxSteps, duration.count());

            if (!measureSteps.empty()) {
                nextSteps();
                start = std::chrono::high_resolution_clock::now();
            } else {
                resetMeasure();
                measurements.AddLog("[%s] Measurement finished!\n", "info");
            }
        }

        if (measurementWindow) {
            ImGui::SetNextWindowSize(ImVec2(400, 300));

            ImGui::Begin("Measurements", &measurementWindow);

            if (!startMeasure) {
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 128, 0, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 100, 0, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 90, 0, 255));

                if (ImGui::SmallButton("Start")) {
                    start = std::chrono::high_resolution_clock::now();
                    measureSteps = std::priority_queue<int, std::vector<int>, std::greater<>>(
                            std::begin(MEASUREMENT_STEPS), std::end(MEASUREMENT_STEPS));
                    startMeasure = true;
                    nextSteps();
                    measurements.AddLog("[%s] Measurement started!\n", "info");
                }
                ImGui::PopStyleColor(3);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(128, 0, 0, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(100, 0, 0, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(90, 0, 0, 255));

                if (ImGui::SmallButton("Stop")) {
                    resetMeasure();
                    measurements.AddLog("[%s] Measurement aborted!\n", "warn");
                }

                ImGui::PopStyleColor(3);
                ImGui::SameLine();

                ImGui::ProgressBar(progressCurrentStep / progressAllSteps, ImVec2(0.0f, 0.0f));
            }

            ImGui::End();
            measurements.Draw("Measurements", &measurementWindow);
        }

        if (colorWindow) {
            static ImVec4 pickerTreeColor{static_cast<float>(treeColor.r / 255.0),
                                          static_cast<float>(treeColor.g / 255.0),
                                          static_cast<float>(treeColor.b / 255.0),
                                          static_cast<float>(treeColor.a / 255.0)};
            static ImVec4 pickerFireColor{static_cast<float>(fireColor.r / 255.0),
                                          static_cast<float>(fireColor.g / 255.0),
                                          static_cast<float>(fireColor.b / 255.0),
                                          static_cast<float>(fireColor.a / 255.0)};

            ImGui::SetNextWindowSize(ImVec2(0, 0));
            ImGui::Begin("Colors", &colorWindow);
            ImGui::SetNextItemWidth(100);

            if (ImGui::ColorPicker4("Tree", (float *) &pickerTreeColor,
                                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_HDR,
                                    (float *) &RESET_TREE_COLOR))
                treeColor = {static_cast<unsigned char>(pickerTreeColor.x * 255.0),
                             static_cast<unsigned char>(pickerTreeColor.y * 255.0),
                             static_cast<unsigned char>(pickerTreeColor.z * 255.0), DEFAULT_TREE_COLOR.a};

            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);

            if (ImGui::ColorPicker4("Fire", (float *) &pickerFireColor,
                                    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_HDR,
                                    (float *) &RESET_FIRE_COLOR))
                fireColor = {static_cast<unsigned char>(pickerFireColor.x * 255.0),
                             static_cast<unsigned char>(pickerFireColor.y * 255.0),
                             static_cast<unsigned char>(pickerFireColor.z * 255.0), DEFAULT_FIRE_COLOR.a};

            ImGui::End();
        }

        if (!limitAnimation)
            calculateForest = true;

        if (stepwiseAnimation && !animationStep)
            calculateForest = false;

        if (requestedBackend != backend->kind()) {
            std::unique_ptr<IBackend> next;
            if (requestedBackend == BackendKind::GPU)
                next = makeGPUBackend();
            if (!next) {
                next = makeOpenMPBackend();
                requestedBackend = BackendKind::OpenMP;
            }
            backend = std::move(next);
            backend->prepare(currentWidth, currentHeight);
            measurements.AddLog("[%s] Active backend: %s\n", "info", backend->name().c_str());
        }

        if (calculateForest) {
            backend->prepare(currentWidth, currentHeight);
            backend->step(forest, fire, growth, currentLogic);
            calculateForest = false;
        }

        ImGui::Render();

        SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, static_cast<int>(clearColor.x * 255),
                               static_cast<int>(clearColor.y * 255),
                               static_cast<int>(clearColor.z * 255),
                               static_cast<int>(clearColor.w * 255));

        SDL_RenderClear(renderer);

        for (int i = 0; i < currentWidth; ++i)
            for (int j = 0; j < currentHeight; ++j)
                if (forest[i][j] == TREE)
                    drawSquare(i, j, treeColor);
                else if (forest[i][j] == FIRE)
                    drawSquare(i, j, fireColor);

        auto endTicks = SDL_GetTicks();
        auto endPerf = SDL_GetPerformanceCounter();

        measurements.framePerf = endPerf - startPerf;
        auto frameMs = static_cast<float>(endTicks - startTicks);
        measurements.fps = (frameMs > 0.0f) ? (1000.0f / frameMs) : 0.0f;
        totalFrameTicks += endTicks - startTicks;
        measurements.avg = (totalFrameTicks > 0)
                           ? (1000.0f * static_cast<float>(totalFrames) / static_cast<float>(totalFrameTicks))
                           : 0.0f;

        if (limitAnimation) {
            auto dT = (static_cast<float>(endTicks) - static_cast<float>(lastUpdate)) / 1000.0f;
            auto framesToUpdate = static_cast<int>(std::floor(dT / (1.0f / currentSpeed)));

            if (framesToUpdate > 0) {
                lastUpdate = endTicks;
                calculateForest = true;
            }
        }

        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);

        if (startMeasure) {
            currentStep++;
            progressCurrentStep++;
        }

        if (animationStep)
            animationStep = false;
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return EXIT_SUCCESS;
}
