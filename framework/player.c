#include "player.h"
#include "raymath.h"

#include <math.h>

static const float contact = 0.001f;

static bool Overlaps(float center, float radius, float min, float max)
{
    return center + radius > min + contact && center - radius < max - contact;
}

static bool OverBox(const PlayerController *player, BoundingBox box)
{
    return Overlaps(player->feet.x, player->radius, box.min.x, box.max.x) &&
           Overlaps(player->feet.z, player->radius, box.min.z, box.max.z);
}

static float FloorAt(const PlayerWorld *world, Vector3 point)
{
    return world->groundHeight ? world->groundHeight(world->ground, point.x, point.z) : 0.0f;
}

PlayerController SpawnPlayer(Vector2 position, const PlayerWorld *world)
{
    PlayerController player = { .feet = { position.x, 0, position.y }, .radius = 0.30f,
                                .height = 1.72f, .eyeHeight = 1.58f, .grounded = true };
    player.feet.y = FloorAt(world, player.feet);
    // If an editor object occupies the spawn point, place the player on its top.
    for (size_t pass = 0; pass < world->boxCount; ++pass)
    {
        float previous = player.feet.y;
        for (size_t i = 0; i < world->boxCount; ++i)
        {
            BoundingBox box = world->boxes[i];
            if (OverBox(&player, box) && player.feet.y < box.max.y &&
                player.feet.y + player.height > box.min.y)
                player.feet.y = box.max.y;
        }
        if (player.feet.y == previous) break;
    }
    return player;
}

static void MoveHorizontal(PlayerController *player, const PlayerWorld *world, float dt, bool xAxis)
{
    float *position = xAxis ? &player->feet.x : &player->feet.z;
    float *velocity = xAxis ? &player->velocity.x : &player->velocity.z;
    float cross = xAxis ? player->feet.z : player->feet.x;
    float start = *position;
    float next = start + *velocity * dt;
    float desired = next;

    for (size_t i = 0; i < world->boxCount; ++i)
    {
        BoundingBox box = world->boxes[i];
        if (player->feet.y >= box.max.y - contact ||
            player->feet.y + player->height <= box.min.y + contact) continue;
        float sideMin = xAxis ? box.min.z : box.min.x;
        float sideMax = xAxis ? box.max.z : box.max.x;
        if (!Overlaps(cross, player->radius, sideMin, sideMax)) continue;

        float nearEdge = (xAxis ? box.min.x : box.min.z) - player->radius;
        float farEdge = (xAxis ? box.max.x : box.max.z) + player->radius;
        if (desired > start && start <= nearEdge + contact && next > nearEdge)
            next = fminf(next, nearEdge);
        else if (desired < start && start >= farEdge - contact && next < farEdge)
            next = fmaxf(next, farEdge);
    }
    *position = next;
    if (next != desired) *velocity = 0;
}

static void MoveVertical(PlayerController *player, const PlayerWorld *world, float dt)
{
    float start = player->feet.y;
    float next = start + player->velocity.y * dt;
    player->grounded = false;

    if (next <= start)
    {
        float landing = FloorAt(world, player->feet);
        for (size_t i = 0; i < world->boxCount; ++i)
        {
            BoundingBox box = world->boxes[i];
            if (OverBox(player, box) && start >= box.max.y - contact)
                landing = fmaxf(landing, box.max.y);
        }
        if (next <= landing)
        {
            next = landing;
            player->velocity.y = 0;
            player->grounded = true;
        }
    }
    else
    {
        for (size_t i = 0; i < world->boxCount; ++i)
        {
            BoundingBox box = world->boxes[i];
            float ceiling = box.min.y - player->height;
            if (OverBox(player, box) && start <= ceiling + contact && next >= ceiling)
            {
                next = ceiling;
                player->velocity.y = 0;
            }
        }
    }
    player->feet.y = next;
}

void StepPlayer(PlayerController *player, const PlayerWorld *world, PlayerInput input, float dt)
{
    if (input.jumpPressed) player->jumpBuffer = 0.12f;
    if (!(dt > 0.0f)) return;
    dt = fminf(dt, 0.1f);
    int steps = (int)ceilf(dt * 120.0f);
    float step = dt / steps;
    Vector2 movement = input.movement;
    float length = Vector2Length(movement);
    if (length > 1) movement = Vector2Scale(movement, 1 / length);
    Vector2 desired = Vector2Scale(movement, input.sprint ? 6.1f : 4.1f);

    for (int i = 0; i < steps; ++i)
    {
        player->groundGrace = player->grounded ? 0.08f : fmaxf(0, player->groundGrace - step);
        if (player->jumpBuffer > 0 && player->groundGrace > 0)
        {
            player->velocity.y = 9.1f;
            player->grounded = false;
            player->groundGrace = player->jumpBuffer = 0;
        }
        player->jumpBuffer = fmaxf(0, player->jumpBuffer - step);

        float response = player->grounded ? 24.0f : 9.0f;
        float approach = fminf(response * step, 1);
        player->velocity.x = Lerp(player->velocity.x, desired.x, approach);
        player->velocity.z = Lerp(player->velocity.z, desired.y, approach);
        if (desired.x == 0 && fabsf(player->velocity.x) < 0.01f) player->velocity.x = 0;
        if (desired.y == 0 && fabsf(player->velocity.z) < 0.01f) player->velocity.z = 0;
        player->velocity.y -= 16.0f * step;
        MoveHorizontal(player, world, step, true);
        MoveHorizontal(player, world, step, false);
        MoveVertical(player, world, step);
    }
}

Vector3 GetPlayerEye(const PlayerController *player)
{
    return Vector3Add(player->feet, (Vector3){ 0, player->eyeHeight, 0 });
}
