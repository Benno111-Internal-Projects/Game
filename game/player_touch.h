#ifndef BENNO111_PLAYER_TOUCH_H
#define BENNO111_PLAYER_TOUCH_H

#include "raylib.h"

typedef struct PlayerTouch {
    int stickId, lookId, jumpId;
    Vector2 lookPosition;
    Vector2 movement;
} PlayerTouch;

typedef struct PlayerTouchInput {
    Vector2 movement;
    Vector2 look;
    bool jumpPressed;
} PlayerTouchInput;

bool HasPlayerTouchControls(void);
void ResetPlayerTouch(PlayerTouch *touch);
PlayerTouchInput ReadPlayerTouch(PlayerTouch *touch);
void DrawPlayerTouch(const PlayerTouch *touch);

#endif
