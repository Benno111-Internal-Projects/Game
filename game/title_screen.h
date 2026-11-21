#ifndef BENNO111_ENGENE_TITLE_SCREEN_H
#define BENNO111_ENGENE_TITLE_SCREEN_H

#include "input.h"

typedef enum TitleAction {
    TITLE_NONE,
    TITLE_START,
    TITLE_EDITOR,
    TITLE_AUDIO,
    TITLE_QUIT
} TitleAction;

TitleAction ReadTitleScreen(InputPointer pointer, bool audioAvailable);
void DrawTitleScreen(Vector2 pointer, bool muted, bool audioAvailable);

#endif
