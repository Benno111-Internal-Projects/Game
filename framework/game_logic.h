#ifndef RENDER_FRAMEWORK_GAME_LOGIC_H
#define RENDER_FRAMEWORK_GAME_LOGIC_H

#include <stdbool.h>

typedef struct GameLogic {
    // Optional. Return false if initialization fails.
    bool (*init)(void *state);

    // Required. Runs once per frame, with delta time in seconds.
    // Return false to exit before drawing the current frame.
    bool (*update)(void *state, float deltaTime);

    // Required. Runs between raylib's BeginDrawing and EndDrawing.
    void (*draw)(const void *state);

    // Optional. Called once after the loop, including when init returns false.
    void (*shutdown)(void *state);
} GameLogic;

// Run on the thread with the open raylib window. The caller owns the window and state.
// Returns false for invalid arguments, a closed window, or failed initialization.
bool RunGameLoop(GameLogic logic, void *state);

#endif
