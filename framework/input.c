#include "input.h"

InputPointer PollInput(const int *keys, InputAction *actions, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        actions[i].down = IsKeyDown(keys[i]);
        actions[i].pressed = IsKeyPressed(keys[i]);
    }

    InputPointer pointer = {
        .position = GetMousePosition(),
        .delta = GetMouseDelta(),
        .wheel = GetMouseWheelMove(),
        .down = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        .pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
    };
#if defined(RENDER_FRAMEWORK_MOBILE)
    static int activeTouch;
    static bool trackingTouch;
    static Vector2 lastTouch;
    int touches = GetTouchPointCount();
    if (touches > 0)
    {
        bool started = !trackingTouch;
        if (started) activeTouch = GetTouchPointId(0);
        trackingTouch = true;
        pointer = (InputPointer){ .position = lastTouch };
        for (int i = 0; i < touches; ++i)
        {
            if (GetTouchPointId(i) != activeTouch) continue;
            pointer.position = GetTouchPosition(i);
            pointer.delta = started ? (Vector2){0} :
                (Vector2){ pointer.position.x - lastTouch.x, pointer.position.y - lastTouch.y };
            pointer.pressed = started;
            pointer.down = true;
            lastTouch = pointer.position;
            break;
        }
    }
    else if (trackingTouch)
    {
        pointer = (InputPointer){ .position = lastTouch };
        trackingTouch = false;
    }
#endif
    return pointer;
}
