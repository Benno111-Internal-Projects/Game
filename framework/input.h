#ifndef RENDER_FRAMEWORK_INPUT_H
#define RENDER_FRAMEWORK_INPUT_H

#include "raylib.h"
#include <stddef.h>

typedef struct InputAction {
    bool down;
    bool pressed;
} InputAction;

typedef struct InputPointer {
    Vector2 position;
    Vector2 delta;
    float wheel;
    bool down;
    bool pressed;
} InputPointer;

// One action per key. The caller defines the action order and owns the arrays.
InputPointer PollInput(const int *keys, InputAction *actions, size_t count);

#endif
