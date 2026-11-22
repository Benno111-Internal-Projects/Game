#include "game_logic.h"
#include "window.h"
#include "raylib.h"

bool RunGameLoop(GameLogic logic, void *state)
{
    if (!logic.update || !logic.draw || !IsWindowReady()) return false;

    bool initialized = !logic.init || logic.init(state);
    if (initialized)
    {
        while (!WindowShouldClose())
        {
            if (!logic.update(state, GetFrameTime())) break;

            PrepareWindowFrame();
            BeginDrawing();
            logic.draw(state);
            EndDrawing();
        }
    }

    if (logic.shutdown) logic.shutdown(state);
    return initialized;
}
