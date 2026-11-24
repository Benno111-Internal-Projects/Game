#include "debug_tools.h"
#include "player_touch.h"
#include "ui.h"
#include "raymath.h"

static Rectangle DebugButton(void)
{
    Vector2 inset = GameUiInset();
    return (Rectangle){ inset.x + 12, inset.y + 85, 76, 30 };
}

static Rectangle DebugPanel(void)
{
    Vector2 inset = GameUiInset();
    float width = fminf(440, GetScreenWidth() - 2 * inset.x - 24);
    float height = fminf(265, GetScreenHeight() - 2 * inset.y - 16);
    return (Rectangle){ (GetScreenWidth() - width) * 0.5f, (GetScreenHeight() - height) * 0.5f, width, height };
}

static Rectangle DebugCommand(int index)
{
    Rectangle panel = DebugPanel();
    float width = (panel.width - 36) / 2;
    float step = (panel.height - 57) / 4;
    return (Rectangle){ panel.x + 12 + (index % 2) * (width + 12),
                        panel.y + 45 + (index / 2) * step, width, step - 7 };
}

DebugAction UpdateDebugTools(DebugTools *debug, InputPointer pointer, bool toggle, bool escape)
{
    if (toggle || (debug->open && escape) ||
        (!debug->open && HasPlayerTouchControls() && ReadUiButton(DebugButton(), pointer, true).activated))
    {
        debug->open = !debug->open;
        if (debug->open && !debug->options.enabled)
        {
            debug->options.enabled = true;
            debug->overlay = true;
        }
        return DEBUG_BLOCK;
    }
    if (!debug->open) return DEBUG_PLAY;

    for (int i = 0; i < 7; ++i)
    {
        if (!ReadUiButton(DebugCommand(i), pointer, true).activated) continue;
        switch (i)
        {
            case 0: debug->options.noDamage = !debug->options.noDamage; break;
            case 1: debug->options.noWalls = !debug->options.noWalls; break;
            case 2: debug->options.noSliding = !debug->options.noSliding; break;
            case 3: debug->overlay = !debug->overlay; break;
            case 4: return DEBUG_TELEPORT;
            case 5: debug->open = false; break;
            case 6: *debug = (DebugTools){0}; break;
        }
        break;
    }
    return DEBUG_BLOCK;
}

void DrawDebugTools(const DebugTools *debug, InputPointer pointer, const PlayerController *player,
                    const char *gameState, const char *progress)
{
    if (debug->open)
    {
        Rectangle panel = DebugPanel();
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 5, 12, 18, 191 });
        DrawUiPanel(panel, (Color){ 17, 31, 44, 255 });
        DrawText("Debug", (int)panel.x + 12, (int)panel.y + 12, 21, RAYWHITE);
        const char *labels[] = {
            debug->options.noDamage ? "Damage off" : "Damage on",
            debug->options.noWalls ? "Wall collision off" : "Wall collision on",
            debug->options.noSliding ? "Sliding off" : "Sliding on",
            debug->overlay ? "Overlay on" : "Overlay off",
            "Random teleport", "Resume", "Exit debug mode",
        };
        for (int i = 0; i < 7; ++i)
        {
            Rectangle button = DebugCommand(i);
            DrawUiButton(button, labels[i], ReadUiButton(button, pointer, true), true);
        }
        return;
    }
    Rectangle button = DebugButton();
    bool touch = HasPlayerTouchControls();
    if (touch) DrawUiButton(button, "Debug", ReadUiButton(button, pointer, true), true);
    if (!debug->overlay) return;
    int x = (int)button.x;
    int y = (int)(touch ? button.y + button.height + 6 : button.y);
    if (touch && y + 109 > PlayerTouchTop() - 6)
    {
        // Short screens have room beside the button, above the touch pads.
        x += (int)button.width + 8;
        y = (int)button.y;
    }
    float width = fminf(246, GetScreenWidth() * 0.5f - x - 10);
    if (width < 52) return;
    DrawUiPanel((Rectangle){ x, y, width, 109 }, (Color){ 12, 24, 34, 232 });
    DrawUiLabel(gameState, x + 7, y + 6, 13, width - 14, GOLD);
    DrawUiLabel(progress, x + 7, y + 22, 13, width - 14, RAYWHITE);
    DrawUiLabel(TextFormat("XYZ %.1f  %.1f  %.1f", player->feet.x, player->feet.y, player->feet.z),
                x + 7, y + 38, 13, width - 14, RAYWHITE);
    DrawUiLabel(TextFormat("VEL %.1f  %.1f  %.1f", player->velocity.x, player->velocity.y, player->velocity.z),
                x + 7, y + 54, 13, width - 14, RAYWHITE);
    DrawUiLabel(TextFormat("%s  /  Jump %s",
                           player->sliding ? "Sliding" : player->grounded ? "Grounded" : "Airborne",
                           player->jumpBuffered ? "queued" : "clear"),
                x + 7, y + 70, 13, width - 14, RAYWHITE);
    DrawUiLabel(TextFormat("Damage %s  Walls %s  Slide %s", debug->options.noDamage ? "off" : "on",
                           debug->options.noWalls ? "off" : "on", debug->options.noSliding ? "off" : "on"),
                x + 7, y + 87, 12, width - 14, RAYWHITE);
}
