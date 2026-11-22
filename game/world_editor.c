#include "world_editor.h"
#include "raymath.h"
#include "ui.h"

#include <stdio.h>

static Rectangle PickerButton(int world, bool edit)
{
    float width = fminf(510, GetScreenWidth() - 32.0f);
    float step = fminf(45, (GetScreenHeight() - 86.0f) / (WORLD_COUNT + 1));
    float x = (GetScreenWidth() - width) / 2, y = 53 + world * step;
    if (edit) return (Rectangle){ x + width - 102, y, 102, step - 3 };
    return (Rectangle){ x, y, width - 109, step - 3 };
}

int ReadWorldPicker(InputPointer pointer)
{
    for (int i = 0; i <= WORLD_COUNT; ++i)
    {
        if (ReadUiButton(PickerButton(i, false), pointer, true).activated) return 2 * i;
        if (ReadUiButton(PickerButton(i, true), pointer, true).activated) return 2 * i + 1;
    }
    return WORLD_PICK_NONE;
}

void DrawWorldPicker(InputPointer pointer)
{
    const char *title = "Main Campaign";
    DrawText(title, (GetScreenWidth() - MeasureText(title, 25)) / 2, 15, 25, RAYWHITE);
    for (int i = 0; i <= WORLD_COUNT; ++i)
    {
        Rectangle play = PickerButton(i, false), edit = PickerButton(i, true);
        const char *label = i == WORLD_COUNT ? "Play full campaign [Enter]" :
            TextFormat("%d. %s", i + 1, worlds[i].name);
        DrawUiButton(play, label, ReadUiButton(play, pointer, true), true);
        DrawUiButton(edit, i == WORLD_COUNT ? "Back" : "Edit", ReadUiButton(edit, pointer, true), true);
    }
}

enum {
    TOOL_RAISE, TOOL_LOWER, TOOL_SPAWN, TOOL_FIRST_BEACON,
    TOOL_SECOND_BEACON, TOOL_THIRD_BEACON, TOOL_GATE, TOOL_SMALLER,
    TOOL_LARGER, TOOL_UNDO, TOOL_SAVE, TOOL_BACK, TOOL_COUNT
};

static const char *const toolLabels[] = {
    "Raise terrain", "Lower terrain", "Set spawn", "Set beacon 1",
    "Set beacon 2", "Set beacon 3", "Set gate", "Smaller brush",
    "Larger brush", "Undo last brush", "Save update", "Back"
};

static Rectangle MapBounds(void)
{
    float side = fminf(GetScreenWidth() * 0.53f - 26, GetScreenHeight() - 100.0f);
    return (Rectangle){ 18, 65, side, side };
}

static Rectangle ToolBounds(int tool)
{
    float left = MapBounds().width + 34;
    float width = (GetScreenWidth() - left - 24.0f) / 2;
    float step = fminf(43, (GetScreenHeight() - 83.0f) / 6);
    return (Rectangle){ left + (tool % 2) * (width + 4), 53 + (tool / 2) * step, width, step - 4 };
}

static void RefreshMap(WorldEditor *editor, const WorldBrush *brush)
{
    enum { PREVIEW_SIDE = 128 };
    Color pixels[PREVIEW_SIDE * PREVIEW_SIDE];
    const WorldSpec *world = &editor->draft;
    float scale = (float)WORLD_SIDE / PREVIEW_SIDE;
    int left = 0, top = 0, right = PREVIEW_SIDE, bottom = PREVIEW_SIDE;
    if (editor->map.id && brush)
    {
        left = (int)fmaxf(0, floorf((brush->center.x - brush->radius) / scale));
        top = (int)fmaxf(0, floorf((brush->center.y - brush->radius) / scale));
        right = (int)fminf(PREVIEW_SIDE, ceilf((brush->center.x + brush->radius) / scale));
        bottom = (int)fminf(PREVIEW_SIDE, ceilf((brush->center.y + brush->radius) / scale));
    }
    int width = right - left;
    for (int row = top; row < bottom; ++row)
        for (int col = left; col < right; ++col)
        {
            float h = WorldHeight(world, (col + 0.5f) * scale, (row + 0.5f) * scale);
            float tone = Clamp(0.5f + h / (2 * fmaxf(world->relief, 5)), 0, 1);
            pixels[(row - top) * width + col - left] = ColorLerp(world->low, world->high, tone);
        }
    if (editor->map.id)
        UpdateTextureRec(editor->map, (Rectangle){ (float)left, (float)top, (float)width, (float)(bottom - top) }, pixels);
    else
    {
        editor->map = LoadTextureFromImage((Image){ pixels, PREVIEW_SIDE, PREVIEW_SIDE, 1,
                                                    PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 });
        SetTextureFilter(editor->map, TEXTURE_FILTER_BILINEAR);
    }
}

bool OpenWorldEditor(WorldEditor *editor, int world)
{
    WorldEditor opened = { .world = world, .radius = 40 };
    if (!CopyWorldSpec(&opened.draft, &worlds[world])) return false;
    *editor = opened;
    RefreshMap(editor, NULL);
    return true;
}

void CloseWorldEditor(WorldEditor *editor)
{
    if (editor->map.id) UnloadTexture(editor->map);
    FreeWorldSpec(&editor->draft);
    *editor = (WorldEditor){0};
}

static void EditPoint(WorldEditor *editor, Vector2 point)
{
    WorldSpec *world = &editor->draft;
    if (editor->tool == TOOL_RAISE || editor->tool == TOOL_LOWER)
    {
        float height = editor->tool == TOOL_RAISE ? 3 : -3;
        if (!AddWorldBrush(world, (WorldBrush){ point, editor->radius, height }))
        {
            editor->status = "Could not allocate terrain edit";
            return;
        }
        RefreshMap(editor, &world->brushes[world->brushCount - 1]);
    }
    else if (editor->tool == TOOL_SPAWN) world->spawn = point;
    else if (editor->tool == TOOL_GATE) world->gate = point;
    else world->waypoints[editor->tool - TOOL_FIRST_BEACON] = point;
    editor->status = "Unsaved changes";
}

bool UpdateWorldEditor(WorldEditor *editor, InputPointer pointer)
{
    for (int i = 0; i < TOOL_COUNT; ++i)
    {
        if (!ReadUiButton(ToolBounds(i), pointer, true).activated) continue;
        if (i == TOOL_BACK) return false;
        if (i <= TOOL_GATE) editor->tool = i;
        else if (i == TOOL_SMALLER) editor->radius = fmaxf(8, editor->radius - 8);
        else if (i == TOOL_LARGER) editor->radius = fminf(96, editor->radius + 8);
        else if (i == TOOL_UNDO && editor->draft.brushCount > 0)
        {
            --editor->draft.brushCount;
            editor->status = "Unsaved changes";
            RefreshMap(editor, &editor->draft.brushes[editor->draft.brushCount]);
        }
        else if (i == TOOL_SAVE)
        {
            char path[1024];
            bool saved = SaveWorldOverride(editor->world, &editor->draft, path, sizeof(path));
            editor->status = saved ? "Saved and applied locally" : "Save failed";
            if (saved) TraceLog(LOG_INFO, "World update saved to %s", path);
        }
        return true;
    }
    Rectangle map = MapBounds();
    if (pointer.pressed && CheckCollisionPointRec(pointer.position, map))
    {
        Vector2 point = { Clamp((pointer.position.x - map.x) * WORLD_SIDE / map.width, 4, WORLD_SIDE - 4),
                          Clamp((pointer.position.y - map.y) * WORLD_SIDE / map.height, 4, WORLD_SIDE - 4) };
        EditPoint(editor, point);
    }
    return true;
}

static void DrawMarker(Rectangle bounds, Vector2 point, Color color, const char *label)
{
    Vector2 center = { bounds.x + point.x * bounds.width / WORLD_SIDE,
                       bounds.y + point.y * bounds.height / WORLD_SIDE };
    DrawCircleV(center, 9, (Color){ 15, 24, 35, 255 });
    DrawText(label, (int)center.x - MeasureText(label, 11) / 2, (int)center.y - 5, 11, color);
}

void DrawWorldEditor(const WorldEditor *editor, InputPointer pointer)
{
    Rectangle map = MapBounds();
    const WorldSpec *world = &editor->draft;
    DrawText(TextFormat("Edit %s", world->name), 18, 14, 20, RAYWHITE);
    DrawText("Choose a tool, then click or tap the map.", 18, 39, 13, LIGHTGRAY);
    if (editor->map.id)
        DrawTexturePro(editor->map, (Rectangle){ 0, 0, (float)editor->map.width, (float)editor->map.height },
                       map, Vector2Zero(), 0, WHITE);
    DrawMarker(map, world->spawn, GREEN, "S");
    for (int i = 0; i < WAYPOINT_COUNT; ++i) DrawMarker(map, world->waypoints[i], GOLD, TextFormat("%d", i + 1));
    DrawMarker(map, world->gate, SKYBLUE, "G");
    if (editor->tool <= TOOL_LOWER && CheckCollisionPointRec(pointer.position, map))
        DrawCircleLinesV(pointer.position, editor->radius * map.width / WORLD_SIDE, RAYWHITE);
    for (int i = 0; i < TOOL_COUNT; ++i)
    {
        Rectangle button = ToolBounds(i);
        UiButtonState state = ReadUiButton(button, pointer, true);
        state.hovered |= i == editor->tool;
        DrawUiButton(button, toolLabels[i], state, true);
    }
    DrawText(TextFormat("Brush %.0f m   %zu edits", editor->radius, world->brushCount),
             18, (int)(map.y + map.height) + 8, 13, LIGHTGRAY);
    if (editor->status)
        DrawText(editor->status, (int)ToolBounds(0).x, (int)ToolBounds(TOOL_SAVE).y + 45, 13, GOLD);
}
