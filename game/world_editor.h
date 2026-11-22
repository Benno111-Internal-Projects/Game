#ifndef DORF_WORLD_EDITOR_H
#define DORF_WORLD_EDITOR_H

#include "input.h"
#include "world_levels.h"

typedef struct WorldEditor {
    WorldSpec draft;
    Texture2D map;
    int world, tool;
    Vector2 lastBrush;
    bool brushing;
    float radius;
    const char *status;
} WorldEditor;

enum { WORLD_PICK_NONE = -1, WORLD_PICK_FULL = 2 * WORLD_COUNT, WORLD_PICK_BACK };

int ReadWorldPicker(InputPointer pointer);
void DrawWorldPicker(InputPointer pointer);
bool OpenWorldEditor(WorldEditor *editor, int world);
void CloseWorldEditor(WorldEditor *editor);
bool UpdateWorldEditor(WorldEditor *editor, InputPointer pointer);
void DrawWorldEditor(const WorldEditor *editor, InputPointer pointer);

#endif
