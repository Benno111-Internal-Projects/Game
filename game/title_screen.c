#include "title_screen.h"
#include "game.h"
#include "raymath.h"
#include "ui.h"

typedef struct TitleLayout {
    float x;
    float y;
    float scale;
    Rectangle buttons[3];
    bool narrow;
} TitleLayout;

static TitleLayout LayoutTitleScreen(void)
{
    float width = (float)GetScreenWidth();
    float height = (float)GetScreenHeight();
    TitleLayout layout = {
        .scale = Clamp(height / 540.0f, 0.52f, 1.2f),
        .narrow = width < 760.0f,
    };
    float buttonWidth = fminf(width - 36.0f, 292.0f * layout.scale);
    layout.x = layout.narrow ? (width - buttonWidth) / 2.0f : width * 0.085f;
    layout.y = (height - 264.0f * layout.scale) / 2.0f;
    for (int i = 0; i < 3; ++i)
    {
        layout.buttons[i] = (Rectangle){
            layout.x, layout.y + (104.0f + i * 54.0f) * layout.scale,
            buttonWidth, 45.0f * layout.scale,
        };
    }
    return layout;
}

TitleAction ReadTitleScreen(InputPointer pointer, bool audioAvailable)
{
    TitleLayout layout = LayoutTitleScreen();
    const TitleAction actions[] = { TITLE_START, TITLE_AUDIO, TITLE_QUIT };
    for (int i = 0; i < 3; ++i)
    {
        if (ReadUiButton(layout.buttons[i], pointer, i != 1 || audioAvailable).activated) return actions[i];
    }
    return TITLE_NONE;
}

void DrawTitleScreen(Vector2 pointer, bool muted, bool audioAvailable)
{
    TitleLayout layout = LayoutTitleScreen();
    const Color background = { 7, 16, 24, 248 };
    const Color accent = { 204, 232, 155, 255 };
    const Color secondary = { 187, 209, 217, 255 };
    Color farEdge = Fade(background, layout.narrow ? 0.75f : 0.24f);
    DrawRectangleGradientH(0, 0, GetScreenWidth(), GetScreenHeight(), background, farEdge);

    int titleSize = (int)(44.0f * layout.scale);
    int titleWidth = layout.narrow ? GetScreenWidth() - 36 : (int)(GetScreenWidth() * 0.48f);
    while (titleSize > 12 && MeasureText(GAME_WINDOW_TITLE, titleSize) > titleWidth) --titleSize;
    int titleX = layout.narrow ? (GetScreenWidth() - MeasureText(GAME_WINDOW_TITLE, titleSize)) / 2
                               : (int)layout.x;
    DrawText(GAME_WINDOW_TITLE, titleX, (int)layout.y, titleSize, RAYWHITE);
    DrawText("3D scene demo", (int)layout.x, (int)(layout.y + 56.0f * layout.scale),
             (int)(19.0f * layout.scale), secondary);

    const char *audioLabel = !audioAvailable ? "Audio unavailable"
                                             : (muted ? "Audio off [M]" : "Audio on [M]");
    const char *labels[] = { "Start [Enter]", audioLabel, "Quit [Esc]" };
    for (int i = 0; i < 3; ++i)
    {
        Rectangle button = layout.buttons[i];
        bool hover = (i != 1 || audioAvailable) && CheckCollisionPointRec(pointer, button);
        Color fill = i == 0 ? (hover ? (Color){ 226, 249, 185, 255 } : accent)
                             : (hover ? (Color){ 46, 71, 84, 255 } : (Color){ 23, 42, 54, 255 });
        Color ink = i == 0 ? (Color){ 16, 35, 25, 255 } : RAYWHITE;
        if (i == 1 && !audioAvailable) ink = secondary;
        DrawRectangleRounded(button, 0.22f, 8, fill);
        int size = (int)(18.0f * layout.scale);
        DrawText(labels[i], (int)(button.x + 15.0f * layout.scale),
                 (int)(button.y + (button.height - size) / 2.0f), size, ink);
    }
}
