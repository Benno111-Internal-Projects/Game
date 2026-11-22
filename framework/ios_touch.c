#include "ios_touch.h"

#include <SDL.h>
#include <limits.h>

typedef struct IosFinger {
    SDL_TouchID device;
    SDL_FingerID finger;
    Vector2 position;
    int id;
    bool down, pressed;
} IosFinger;

static IosFinger fingers[INPUT_TOUCH_CAPACITY];
static SDL_mutex *fingerLock;
static int nextId;

static int CaptureFinger(void *unused, SDL_Event *event)
{
    (void)unused;
    if (event->type == SDL_APP_WILLENTERBACKGROUND ||
        (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST))
    {
        SDL_LockMutex(fingerLock);
        for (size_t i = 0; i < INPUT_TOUCH_CAPACITY; ++i) fingers[i] = (IosFinger){0};
        SDL_UnlockMutex(fingerLock);
        return 1;
    }
    if (event->type != SDL_FINGERDOWN && event->type != SDL_FINGERMOTION && event->type != SDL_FINGERUP)
        return 1;

    SDL_LockMutex(fingerLock);
    IosFinger *active = NULL, *empty = NULL;
    for (size_t i = 0; i < INPUT_TOUCH_CAPACITY; ++i)
    {
        IosFinger *finger = &fingers[i];
        if (finger->down && finger->device == event->tfinger.touchId && finger->finger == event->tfinger.fingerId)
            active = finger;
        if (!finger->down && !finger->pressed && !empty) empty = finger;
    }
    if (event->type == SDL_FINGERDOWN && !active && empty)
    {
        if (nextId == INT_MAX) nextId = 0;
        active = empty;
        *active = (IosFinger){
            .device = event->tfinger.touchId, .finger = event->tfinger.fingerId,
            .id = ++nextId, .down = true, .pressed = true,
        };
    }
    if (active)
    {
        if (event->type == SDL_FINGERUP)
            active->down = false;
        else
            active->position = (Vector2){ event->tfinger.x, event->tfinger.y };
    }
    SDL_UnlockMutex(fingerLock);
    return 1;
}

bool InitIosTouch(void)
{
    fingerLock = SDL_CreateMutex();
    if (!fingerLock) return false;
    SDL_AddEventWatch(CaptureFinger, NULL);
    return true;
}

void CloseIosTouch(void)
{
    if (!fingerLock) return;
    SDL_DelEventWatch(CaptureFinger, NULL);
    SDL_DestroyMutex(fingerLock);
    fingerLock = NULL;
    for (size_t i = 0; i < INPUT_TOUCH_CAPACITY; ++i) fingers[i] = (IosFinger){0};
}

size_t ReadIosTouch(InputTouch *points, size_t capacity)
{
    size_t count = 0;
    if (!fingerLock) return count;
    int width = GetScreenWidth(), height = GetScreenHeight();
    SDL_LockMutex(fingerLock);
    for (size_t i = 0; i < INPUT_TOUCH_CAPACITY && count < capacity; ++i)
    {
        IosFinger *finger = &fingers[i];
        if (!finger->down && !finger->pressed) continue;
        points[count++] = (InputTouch){
            .id = finger->id,
            .position = { finger->position.x * width, finger->position.y * height },
            .down = finger->down, .pressed = finger->pressed,
        };
        finger->pressed = false;
    }
    SDL_UnlockMutex(fingerLock);
    return count;
}
