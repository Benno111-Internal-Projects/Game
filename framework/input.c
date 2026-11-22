#include "input.h"

#if defined(RENDER_FRAMEWORK_IOS)
#include "ios_touch.h"
#endif

static InputTouch touches[INPUT_TOUCH_CAPACITY];
static size_t touchCount;

bool InitInputTouches(void)
{
#if defined(RENDER_FRAMEWORK_IOS)
    return InitIosTouch();
#else
    return true;
#endif
}

void CloseInputTouches(void)
{
#if defined(RENDER_FRAMEWORK_IOS)
    CloseIosTouch();
#endif
    touchCount = 0;
}

size_t GetInputTouches(const InputTouch **points)
{
    *points = touches;
    return touchCount;
}

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
#if defined(RENDER_FRAMEWORK_IOS)
    touchCount = ReadIosTouch(touches, INPUT_TOUCH_CAPACITY);
#else
    int total = GetTouchPointCount();
    touchCount = 0;
    for (int i = 0; i < total && touchCount < INPUT_TOUCH_CAPACITY; ++i)
    {
        int id = GetTouchPointId(i);
        if (id == -1) continue;
        touches[touchCount++] = (InputTouch){ .id = id, .position = GetTouchPosition(i), .down = true };
    }
#endif
    if (touchCount > 0)
    {
        bool started = !trackingTouch;
        if (started) activeTouch = touches[0].id;
        trackingTouch = true;
        pointer = (InputPointer){ .position = lastTouch };
        bool anyDown = false;
        for (size_t i = 0; i < touchCount; ++i)
        {
            const InputTouch *touch = &touches[i];
            anyDown |= touch->down;
            if (touch->id != activeTouch) continue;
            pointer.position = touch->position;
            pointer.delta = started ? (Vector2){0} :
                (Vector2){ pointer.position.x - lastTouch.x, pointer.position.y - lastTouch.y };
            pointer.pressed = started;
            pointer.down = touch->down;
            lastTouch = pointer.position;
        }
        trackingTouch = anyDown;
    }
    else if (trackingTouch)
    {
        pointer = (InputPointer){ .position = lastTouch };
        trackingTouch = false;
    }
#endif
    return pointer;
}
