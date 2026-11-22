#ifndef RENDER_FRAMEWORK_UI_H
#define RENDER_FRAMEWORK_UI_H

#include "input.h"

typedef struct UiButtonState {
    bool hovered;
    bool activated;
} UiButtonState;

UiButtonState ReadUiButton(Rectangle bounds, InputPointer pointer, bool enabled);
void DrawUiLabel(const char *label, int x, int y, int size, float width, Color color);
void DrawUiPanel(Rectangle bounds, Color background);
void DrawUiButton(Rectangle bounds, const char *label, UiButtonState state, bool enabled);

#endif
