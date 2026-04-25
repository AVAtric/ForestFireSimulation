#pragma once

#include <imgui.h>

struct MeasurementsLog {
    ImGuiTextBuffer Buf;
    ImVector<int> LineOffsets;

    MeasurementsLog();

    void Clear();
    void AddLog(const char *fmt, ...) IM_FMTARGS(2);
    void Draw(const char *title, bool *p_open = nullptr);

    float fps{};
    float avg{};
    unsigned long long framePerf{};
};

extern MeasurementsLog measurements;
