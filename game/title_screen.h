#ifndef BENNO111_ENGENE_TITLE_SCREEN_H
#define BENNO111_ENGENE_TITLE_SCREEN_H

#include "input.h"

typedef enum TitleAction {
    TITLE_NONE,
    TITLE_CAMPAIGN,
    TITLE_MAZES,
    TITLE_DEBUG,
    TITLE_SANDBOX,
    TITLE_EDITOR,
    TITLE_AUDIO,
    TITLE_QUIT
} TitleAction;

typedef enum MazesAction {
    MAZES_NONE, MAZES_CLASSIC, MAZES_INFINITE, MAZES_EDIT, MAZES_BACK
} MazesAction;

MazesAction ReadMazesMenu(InputPointer pointer);
void DrawMazesMenu(Vector2 pointer);
TitleAction ReadTitleScreen(InputPointer pointer, bool audioAvailable);
void DrawTitleScreen(Vector2 pointer, bool muted, bool audioAvailable);

#endif
