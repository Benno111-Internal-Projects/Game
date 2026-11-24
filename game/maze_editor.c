#include "maze_editor.h"
#include "player_touch.h"
#include "raymath.h"
#include "ui.h"

#include <stdlib.h>
#include <string.h>

enum { WALL, PASSAGE, SPAWN, EXIT, SWITCH_MAZE, UNDO, SAVE, BACK, PLAY, TOOL_COUNT };

static const LevelSpec *EditedMaze(const MazeEditor *editor)
{
    return editor->part ? &campaignExtraMazes[editor->level] : &campaignLevels[editor->level];
}

static Rectangle Board(const MazeEditor *editor)
{
    const LevelSpec *level = EditedMaze(editor);
    Vector2 inset = GameUiInset();
    float cell = fminf((GetScreenWidth() * 0.64f - inset.x - 30) / level->mazeCols,
                       (GetScreenHeight() - 98 - 2 * inset.y) / level->mazeRows);
    return (Rectangle){ inset.x + 16, inset.y + 59, cell * level->mazeCols, cell * level->mazeRows };
}

static Rectangle Tool(const MazeEditor *editor, int slot)
{
    Rectangle board = Board(editor);
    Vector2 inset = GameUiInset();
    float left = board.x + board.width + 16;
    float width = (GetScreenWidth() - inset.x - left - 24) / 2;
    float row = fminf(43, (GetScreenHeight() - 100 - 2 * inset.y) / 5);
    return (Rectangle){ left + (slot % 2) * (width + 6), inset.y + 59 + (slot / 2) * row, width, row - 5 };
}

static bool ToolEnabled(const MazeEditor *editor, int tool)
{
    if (tool == SWITCH_MAZE) return campaignExtraMazes[editor->level].mazeRows > 0;
    if (tool == UNDO) return editor->undoPart >= 0;
    return true;
}

void OpenMazeEditor(MazeEditor *editor, int level)
{
    *editor = (MazeEditor){ .level = level, .undoPart = -1 };
    const LevelSpec *first = &campaignLevels[level], *second = &campaignExtraMazes[level];
    memcpy(editor->tiles[0], first->maze, (size_t)first->mazeRows * first->mazeCols);
    memcpy(editor->tiles[1], second->maze, (size_t)second->mazeRows * second->mazeCols);
}

static bool PaintTile(MazeEditor *editor, int index)
{
    const LevelSpec *level = EditedMaze(editor);
    int row = index / level->mazeCols, col = index % level->mazeCols;
    bool side = col == 0 || col == level->mazeCols - 1;
    char *tiles = editor->tiles[editor->part];
    static const char paint[] = { '#', '.', 'S', 'E' };
    char tile = paint[editor->tool];
    bool border = side || row == 0 || row == level->mazeRows - 1;
    if ((tile == 'E' && (side || row != 0)) || (tile != 'E' && border) ||
        tiles[index] == tile || (tile != 'S' && tiles[index] == 'S')) return false;
    if (tile == 'S' || tile == 'E')
        for (int i = 0; i < level->mazeRows * level->mazeCols; ++i)
            if (tiles[i] == tile) tiles[i] = tile == 'S' ? '.' : '#';
    tiles[index] = tile;
    return true;
}

static bool PaintLine(MazeEditor *editor, int end)
{
    int cols = EditedMaze(editor)->mazeCols;
    int x = editor->lastTile % cols, y = editor->lastTile / cols;
    int lastX = end % cols, lastY = end / cols;
    int dx = abs(lastX - x), sx = x < lastX ? 1 : -1;
    int dy = -abs(lastY - y), sy = y < lastY ? 1 : -1;
    int error = dx + dy;
    bool changed = false;
    for (;;)
    {
        changed |= PaintTile(editor, y * cols + x);
        if (x == lastX && y == lastY) break;
        int next = 2 * error;
        if (next >= dy) { error += dy; x += sx; }
        if (next <= dx) { error += dx; y += sy; }
    }
    editor->lastTile = end;
    return changed;
}

MazeEditorAction UpdateMazeEditor(MazeEditor *editor, InputPointer pointer)
{
    for (int i = 0; i < TOOL_COUNT; ++i)
    {
        if (!ReadUiButton(Tool(editor, i), pointer, ToolEnabled(editor, i)).activated) continue;
        editor->drawing = false;
        if (i == BACK) return MAZE_EDIT_BACK;
        if (i == PLAY) return MAZE_EDIT_PLAY;
        if (i <= EXIT) editor->tool = i;
        else if (i == SWITCH_MAZE) editor->part ^= 1;
        else if (i == UNDO)
        {
            editor->part = editor->undoPart;
            memcpy(editor->tiles[editor->part], editor->undo, sizeof(editor->undo));
            editor->undoPart = -1;
            editor->status = "Unsaved changes";
        }
        else if (i == SAVE)
        {
            MazeSaveResult saved = SaveCampaignMazeEdit(editor->level, editor->tiles[0], editor->tiles[1]);
            editor->status = saved == MAZE_SAVED ? "Saved and applied locally." :
                             saved == MAZE_INVALID ? "Connect every passage to the spawn and exit." : "Save failed";
        }
        return MAZE_EDIT_CONTINUE;
    }

    Rectangle board = Board(editor);
    if (!CheckCollisionPointRec(pointer.position, board) || (!pointer.down && !pointer.pressed))
    {
        editor->drawing = false;
        return MAZE_EDIT_CONTINUE;
    }
    const LevelSpec *level = EditedMaze(editor);
    int col = (int)((pointer.position.x - board.x) * level->mazeCols / board.width);
    int row = (int)((pointer.position.y - board.y) * level->mazeRows / board.height);
    if (col < 0 || col >= level->mazeCols || row < 0 || row >= level->mazeRows) return MAZE_EDIT_CONTINUE;
    int tile = row * level->mazeCols + col;
    if (pointer.pressed)
    {
        char before[MAZE_EDITOR_TILES];
        memcpy(before, editor->tiles[editor->part], sizeof(before));
        editor->drawing = editor->tool <= PASSAGE;
        editor->lastTile = tile;
        bool changed = PaintTile(editor, tile);
        if (changed || editor->drawing)
        {
            memcpy(editor->undo, before, sizeof(editor->undo));
            editor->undoPart = editor->part;
        }
        if (changed) editor->status = "Unsaved changes";
    }
    else if (editor->drawing && PaintLine(editor, tile)) editor->status = "Unsaved changes";
    return MAZE_EDIT_CONTINUE;
}

void DrawMazeEditor(const MazeEditor *editor, InputPointer pointer)
{
    Rectangle board = Board(editor);
    const LevelSpec *level = EditedMaze(editor);
    bool extra = campaignExtraMazes[editor->level].mazeRows > 0;
    int x = (int)board.x;
    int top = (int)GameUiInset().y;
    float width = GetScreenWidth() - 2 * board.x;
    DrawUiLabel(extra ? TextFormat("Edit %s  /  Maze %d of 2", level->name, editor->part + 1)
                      : TextFormat("Edit %s", level->name), x, top + 10, 20, width, RAYWHITE);
    DrawUiLabel("Drag to paint. Move the exit along the top edge.", x, top + 37, 13, width, LIGHTGRAY);
    float w = board.width / level->mazeCols, h = board.height / level->mazeRows;
    for (int row = 0; row < level->mazeRows; ++row)
        for (int col = 0; col < level->mazeCols; ++col)
        {
            char tile = editor->tiles[editor->part][row * level->mazeCols + col];
            Color ink = tile == '#' ? level->stone : tile == 'S' ? GREEN : tile == 'E' ? GOLD : (Color){ 25, 38, 48, 255 };
            DrawRectangleRec((Rectangle){ board.x + col * w, board.y + row * h, w + 0.2f, h + 0.2f }, ink);
        }

    const Color grid = { 8, 15, 23, 84 };
    for (int col = 0; col <= level->mazeCols; ++col)
        DrawLineV((Vector2){ board.x + col * w, board.y },
                  (Vector2){ board.x + col * w, board.y + board.height }, grid);
    for (int row = 0; row <= level->mazeRows; ++row)
        DrawLineV((Vector2){ board.x, board.y + row * h },
                  (Vector2){ board.x + board.width, board.y + row * h }, grid);

    const char *labels[] = { "Wall", "Passage", "Move spawn", "Move exit",
                             editor->part ? "Edit first maze" : "Edit second maze", "Undo", "Save update", "Back", "Play saved" };
    for (int i = 0; i < TOOL_COUNT; ++i)
    {
        Rectangle bounds = Tool(editor, i);
        bool enabled = ToolEnabled(editor, i);
        UiButtonState state = ReadUiButton(bounds, pointer, enabled);
        state.hovered |= i == editor->tool;
        DrawUiButton(bounds, labels[i], state, enabled);
    }
    DrawUiLabel(editor->status ? editor->status : "Green spawn   /   Gold exit",
                x, (int)(board.y + board.height + 8), 13, width, LIGHTGRAY);
}
