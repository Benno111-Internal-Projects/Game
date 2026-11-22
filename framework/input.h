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

enum { INPUT_TOUCH_CAPACITY = 10 };

typedef struct InputTouch {
    int id;
    Vector2 position;
    bool down, pressed;
} InputTouch;

// Start after opening the window and close before shutting down the platform.
bool InitInputTouches(void);
void CloseInputTouches(void);
// Snapshot shared by UI and gameplay, updated by PollInput.
size_t GetInputTouches(const InputTouch **points);

// One action per key. The caller defines the action order and owns the arrays.
InputPointer PollInput(const int *keys, InputAction *actions, size_t count);

#endif
