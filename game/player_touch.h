#ifndef BENNO111_PLAYER_TOUCH_H
#define BENNO111_PLAYER_TOUCH_H

#include "raylib.h"

typedef struct PlayerTouch {
    int stickId, lookId, jumpId, sprintId;
    Vector2 lookPosition;
    Vector2 movement;
} PlayerTouch;

typedef struct PlayerTouchInput {
    Vector2 movement;
    Vector2 look;
    bool jumpPressed;
    bool sprintHeld;
} PlayerTouchInput;

bool HasPlayerTouchControls(void);
Vector2 GameUiInset(void);
float PlayerTouchTop(void);
void ResetPlayerTouch(PlayerTouch *touch);
PlayerTouchInput ReadPlayerTouch(PlayerTouch *touch);
void DrawPlayerTouch(const PlayerTouch *touch);

#endif
