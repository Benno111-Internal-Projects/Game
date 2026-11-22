#include "ui.h"

UiButtonState ReadUiButton(Rectangle bounds, InputPointer pointer, bool enabled)
{
    bool hovered = enabled && CheckCollisionPointRec(pointer.position, bounds);
    return (UiButtonState){ .hovered = hovered, .activated = hovered && pointer.pressed };
}

void DrawUiLabel(const char *label, int x, int y, int size, float width, Color color)
{
    if (width < 1) return;
    while (size > 9 && MeasureText(label, size) > width) --size;
    if (MeasureText(label, size) <= width)
    {
        DrawText(label, x, y, size, color);
        return;
    }
    BeginScissorMode(x, y, (int)width, size + 2);
    DrawText(label, x, y, size, color);
    EndScissorMode();
}

void DrawUiPanel(Rectangle bounds, Color background)
{
    DrawRectangleRec(bounds, background);
}

void DrawUiButton(Rectangle bounds, const char *label, UiButtonState state, bool enabled)
{
    int fontSize = 14;
    while (fontSize > 8 && (MeasureText(label, fontSize) > bounds.width - 6.0f ||
                            fontSize > bounds.height - 2.0f))
        --fontSize;
    Color fill = state.hovered ? (Color){ 66, 104, 130, 255 } : (Color){ 38, 64, 84, 255 };
    Color text = enabled ? RAYWHITE : GRAY;
    DrawUiPanel(bounds, fill);
    int offset = (int)(bounds.width - MeasureText(label, fontSize)) / 2;
    if (offset < 3) offset = 3;
    DrawUiLabel(label, (int)bounds.x + offset, (int)(bounds.y + (bounds.height - fontSize) / 2.0f),
                fontSize, bounds.width - offset - 3, text);
}
