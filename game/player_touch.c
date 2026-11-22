#include "player_touch.h"
#include "raymath.h"

bool HasPlayerTouchControls(void)
{
#if defined(PLATFORM_ANDROID) || defined(BENNO_IOS)
    return true;
#else
    return false;
#endif
}

void ResetPlayerTouch(PlayerTouch *touch)
{
    *touch = (PlayerTouch){ .stickId = -1, .lookId = -1, .jumpId = -1 };
}

Vector2 GameUiInset(void)
{
    if (!HasPlayerTouchControls()) return (Vector2){0};
    float height = (float)GetScreenHeight();
    return (Vector2){ fminf(GetScreenWidth() * 0.055f, height * 0.18f), height * 0.04f };
}

static float ControlRadius(void)
{
    return Clamp(fminf(GetScreenHeight() * 0.14f, GetScreenWidth() * 0.12f), 25.0f, 60.0f);
}

float PlayerTouchTop(void)
{
    return GetScreenHeight() - GameUiInset().y - 12 - 2 * ControlRadius();
}

static Vector2 StickCenter(float radius)
{
    Vector2 inset = GameUiInset();
    return (Vector2){ radius + 12 + inset.x, GetScreenHeight() - radius - 12 - inset.y };
}

static Vector2 JumpCenter(float radius)
{
    Vector2 center = StickCenter(radius);
    center.x = GetScreenWidth() - center.x;
    return center;
}

static Vector2 StickInput(Vector2 position, Vector2 center, float radius)
{
    Vector2 movement = Vector2ClampValue(Vector2Scale(Vector2Subtract(position, center),
                                                        1.0f / (radius * 0.72f)), 0, 1);
    return (Vector2){ movement.x, -movement.y };
}

PlayerTouchInput ReadPlayerTouch(PlayerTouch *touch)
{
    PlayerTouchInput input = { 0 };
    if (!HasPlayerTouchControls()) return input;
    bool stickHeld = false, lookHeld = false, jumpHeld = false;
    float radius = ControlRadius();
    Vector2 stickCenter = StickCenter(radius);
    int count = GetTouchPointCount();
    for (int i = 0; i < count; ++i)
    {
        int id = GetTouchPointId(i);
        if (id == -1) continue;
        Vector2 position = GetTouchPosition(i);
        if (id == touch->stickId)
        {
            stickHeld = true;
            input.movement = StickInput(position, stickCenter, radius);
        }
        else if (id == touch->lookId)
        {
            lookHeld = true;
            input.look = Vector2Subtract(position, touch->lookPosition);
            touch->lookPosition = position;
        }
        else if (id == touch->jumpId) jumpHeld = true;
    }
    if (!stickHeld) touch->stickId = -1;
    if (!lookHeld) touch->lookId = -1;
    if (!jumpHeld) touch->jumpId = -1;

    for (int i = 0; i < count; ++i)
    {
        int id = GetTouchPointId(i);
        if (id == -1 || id == touch->stickId || id == touch->lookId || id == touch->jumpId) continue;
        Vector2 position = GetTouchPosition(i);
        if (touch->stickId == -1 && CheckCollisionPointCircle(position, stickCenter, radius + 14))
        {
            touch->stickId = id;
            input.movement = StickInput(position, stickCenter, radius);
        }
        else if (touch->jumpId == -1 && CheckCollisionPointCircle(position, JumpCenter(radius), radius + 8))
        {
            touch->jumpId = id;
            input.jumpPressed = true;
        }
        else if (touch->lookId == -1 && position.x > GetScreenWidth() * 0.4f && position.y > 80.0f + GameUiInset().y)
        {
            touch->lookId = id;
            touch->lookPosition = position;
        }
    }
    touch->movement = input.movement;
    return input;
}

void DrawPlayerTouch(const PlayerTouch *touch)
{
    if (!HasPlayerTouchControls()) return;
    float radius = ControlRadius();
    Vector2 stick = StickCenter(radius), jump = JumpCenter(radius);
    const Color background = { 18, 32, 42, 190 };
    Vector2 thumb = Vector2Add(stick, Vector2Scale((Vector2){ touch->movement.x, -touch->movement.y }, radius * 0.63f));
    DrawCircleV(stick, radius, background);
    DrawCircleV(thumb, radius * 0.29f, RAYWHITE);
    DrawCircleV(jump, radius, touch->jumpId == -1 ? background : (Color){ 91, 123, 105, 235 });
    DrawText("Jump", (int)jump.x - MeasureText("Jump", 17) / 2, (int)jump.y - 8, 17, RAYWHITE);
}
