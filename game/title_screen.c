#include "title_screen.h"
#include "game.h"
#include "raymath.h"
#include "player_touch.h"
#include "ui.h"

enum { TITLE_BUTTONS = TITLE_QUIT - TITLE_CAMPAIGN + 1 };

typedef struct TitleLayout {
    float x;
    float y;
    float scale;
    Rectangle buttons[TITLE_BUTTONS];
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
    Vector2 inset = GameUiInset();
    layout.scale = fminf(layout.scale, (height - 2 * inset.y - 12) / 442.0f);
    float buttonWidth = fminf(width - 36.0f - 2 * inset.x, 292.0f * layout.scale);
    layout.x = layout.narrow ? (width - buttonWidth) / 2.0f : fmaxf(width * 0.085f, inset.x + 12);
    layout.y = (height - 442.0f * layout.scale) / 2.0f;
    for (int i = 0; i < TITLE_BUTTONS; ++i)
    {
        layout.buttons[i] = (Rectangle){
            layout.x, layout.y + (69.0f + i * 54.0f) * layout.scale,
            buttonWidth, 45.0f * layout.scale,
        };
    }
    return layout;
}

TitleAction ReadTitleScreen(InputPointer pointer, bool audioAvailable)
{
    TitleLayout layout = LayoutTitleScreen();
    for (int i = 0; i < TITLE_BUTTONS; ++i)
    {
        TitleAction action = TITLE_CAMPAIGN + i;
        if (ReadUiButton(layout.buttons[i], pointer, action != TITLE_AUDIO || audioAvailable).activated) return action;
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

    const char *audioLabel = !audioAvailable ? "Audio unavailable"
                                             : (muted ? "Audio off [M]" : "Audio on [M]");
    bool touch = HasPlayerTouchControls();
    if (touch && audioAvailable) audioLabel = muted ? "Audio off" : "Audio on";
    const char *labels[] = { touch ? "Main Campaign" : "Main Campaign [Enter]", "Mazes", "Debug Level Select",
                             "Sandbox", "Scene Editor", audioLabel, touch ? "Quit" : "Quit [Esc]" };
    for (int i = 0; i < TITLE_BUTTONS; ++i)
    {
        Rectangle button = layout.buttons[i];
        bool audio = i == TITLE_AUDIO - TITLE_CAMPAIGN;
        bool hover = (!audio || audioAvailable) && CheckCollisionPointRec(pointer, button);
        Color fill = i == 0 ? (hover ? (Color){ 226, 249, 185, 255 } : accent)
                             : (hover ? (Color){ 46, 71, 84, 255 } : (Color){ 23, 42, 54, 255 });
        Color ink = i == 0 ? (Color){ 16, 35, 25, 255 } : RAYWHITE;
        if (audio && !audioAvailable) ink = secondary;
        DrawRectangleRounded(button, 0.22f, 8, fill);
        int size = (int)(18.0f * layout.scale);
        DrawUiLabel(labels[i], (int)(button.x + 15.0f * layout.scale),
                    (int)(button.y + (button.height - size) / 2.0f), size,
                    button.width - 28.0f * layout.scale, ink);
    }
}

static Rectangle MazesPanel(void)
{
    Vector2 inset = GameUiInset();
    float width = fminf(310, GetScreenWidth() - 2 * inset.x - 24);
    float scale = fminf(1, (GetScreenHeight() - 2 * inset.y - 12) / 234);
    float height = 234 * scale;
    float y = fminf(GetScreenHeight() * 0.5f - 108 * scale, GetScreenHeight() - inset.y - height);
    return (Rectangle){ (GetScreenWidth() - width) * 0.5f, y, width, height };
}

static Rectangle MazesButton(int index)
{
    Rectangle panel = MazesPanel();
    float scale = panel.height / 234;
    return (Rectangle){ panel.x + 20, panel.y + (74 + index * 50) * scale,
                        panel.width - 40, 40 * scale };
}

MazesAction ReadMazesMenu(InputPointer pointer)
{
    for (int i = 0; i < 3; ++i)
        if (ReadUiButton(MazesButton(i), pointer, true).activated) return MAZES_CLASSIC + i;
    return MAZES_NONE;
}

void DrawMazesMenu(Vector2 pointer)
{
    const char *labels[] = { "Classic (seven mazes)", "Infinite (random mazes)", "Back" };
    Rectangle panel = MazesPanel();
    DrawUiPanel(panel, (Color){ 15, 24, 35, 255 });
    int size = (int)fminf(30, panel.height / 7.8f);
    DrawText("Mazes", (GetScreenWidth() - MeasureText("Mazes", size)) / 2,
             (int)(panel.y + 18 * panel.height / 234), size, RAYWHITE);
    for (int i = 0; i < 3; ++i)
    {
        Rectangle button = MazesButton(i);
        DrawUiButton(button, labels[i], ReadUiButton(button, (InputPointer){ .position = pointer }, true), true);
    }
}
