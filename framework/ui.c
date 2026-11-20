#include "ui.h"

UiButtonState ReadUiButton(Rectangle bounds, InputPointer pointer, bool enabled)
{
    bool hovered = enabled && CheckCollisionPointRec(pointer.position, bounds);
    return (UiButtonState){ .hovered = hovered, .activated = hovered && pointer.pressed };
}

void DrawUiPanel(Rectangle bounds, Color background)
{
    DrawRectangleRec(bounds, background);
}

void DrawUiButton(Rectangle bounds, const char *label, UiButtonState state, bool enabled)
{
    const int fontSize = 14;
    Color fill = state.hovered ? (Color){ 66, 104, 130, 255 } : (Color){ 38, 64, 84, 255 };
    Color text = enabled ? RAYWHITE : GRAY;
    DrawUiPanel(bounds, fill);
    DrawText(label, (int)(bounds.x + (bounds.width - MeasureText(label, fontSize)) / 2.0f),
             (int)(bounds.y + (bounds.height - fontSize) / 2.0f), fontSize, text);
}
