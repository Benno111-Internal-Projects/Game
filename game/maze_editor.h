#ifndef DORF_MAZE_EDITOR_H
#define DORF_MAZE_EDITOR_H

#include "campaign_content.h"
#include "input.h"

enum { MAZE_EDITOR_TILES = AUTHORED_MAZE_SIDE * AUTHORED_MAZE_SIDE };

typedef struct MazeEditor {
    char tiles[2][MAZE_EDITOR_TILES];
    char undo[MAZE_EDITOR_TILES];
    int level, part, tool, undoPart, lastTile;
    bool drawing;
    const char *status;
} MazeEditor;

void OpenMazeEditor(MazeEditor *editor, int level);
typedef enum MazeEditorAction { MAZE_EDIT_CONTINUE, MAZE_EDIT_BACK, MAZE_EDIT_PLAY } MazeEditorAction;
MazeEditorAction UpdateMazeEditor(MazeEditor *editor, InputPointer pointer);
void DrawMazeEditor(const MazeEditor *editor, InputPointer pointer);

#endif
