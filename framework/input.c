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
    if (GetTouchPointCount() > 0) pointer.position = GetTouchPosition(0);
#endif
    return pointer;
}
