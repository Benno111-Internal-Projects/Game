#include "player.h"
#include "raymath.h"

#include <math.h>

static const float contact = 0.001f;
static const float minWalkableNormalY = 0.70710678f; // cos(45 degrees)
static const float gravity = 16.0f;

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

static Vector3 GroundNormalAt(const PlayerWorld *world, Vector3 point)
{
    if (world->groundNormal) return world->groundNormal(world->ground, point.x, point.z);
    if (!world->groundHeight) return (Vector3){ 0, 1, 0 };

    const float sample = 0.05f;
    Vector3 x = { sample, 0, 0 }, z = { 0, 0, sample };
    float dx = FloorAt(world, Vector3Add(point, x)) - FloorAt(world, Vector3Subtract(point, x));
    float dz = FloorAt(world, Vector3Add(point, z)) - FloorAt(world, Vector3Subtract(point, z));
    return Vector3Normalize((Vector3){ -dx, 2 * sample, -dz });
}

static bool IsSteep(const PlayerWorld *world, Vector3 point)
{
    return !world->disableSliding && GroundNormalAt(world, point).y < minWalkableNormalY;
}

PlayerController SpawnPlayer(Vector2 position, const PlayerWorld *world)
{
    PlayerController player = { .feet = { position.x, 0, position.y }, .radius = 0.30f,
                                .height = 1.72f, .eyeHeight = 1.58f, .grounded = true };
    float ground = FloorAt(world, player.feet);
    player.feet.y = ground;
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
    player.sliding = player.feet.y == ground && IsSteep(world, player.feet);
    player.grounded = !player.sliding;
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
    if (next != start)
    {
        Vector3 nextFeet = player->feet;
        if (xAxis) nextFeet.x = next;
        else nextFeet.z = next;
        float nextHeight = FloorAt(world, nextFeet);
        if (nextHeight > FloorAt(world, player->feet) + 0.00001f &&
            nextHeight >= player->feet.y - contact && IsSteep(world, nextFeet))
            next = start;
    }
    *position = next;
    if (next != desired) *velocity = 0;
}

static void MoveVertical(PlayerController *player, const PlayerWorld *world, float dt, bool followGround)
{
    float start = player->feet.y;
    float next = start + player->velocity.y * dt;
    player->grounded = false;
    player->sliding = false;

    if (next <= start)
    {
        float landing = FloorAt(world, player->feet);
        bool onBox = false;
        for (size_t i = 0; i < world->boxCount; ++i)
        {
            BoundingBox box = world->boxes[i];
            if (OverBox(player, box) && start >= box.max.y - contact && box.max.y >= landing)
            {
                landing = box.max.y;
                onBox = true;
            }
        }
        // Follow small downward changes in terrain without snapping off crate tops.
        if (next <= landing || (followGround && !onBox && start - landing <= 0.16f))
        {
            next = landing;
            player->velocity.y = 0;
            player->sliding = !onBox && IsSteep(world, player->feet);
            player->grounded = !player->sliding;
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
    if (input.jumpPressed) player->jumpBuffered = true;
    if (!(dt > 0.0f)) return;
    if (world->disableSliding && player->sliding)
    {
        player->sliding = false;
        player->grounded = true;
    }
    dt = fminf(dt, 0.1f);
    int steps = (int)ceilf(dt * 120.0f);
    float step = dt / steps;
    Vector2 movement = input.movement;
    float length = Vector2Length(movement);
    if (length > 1) movement = Vector2Scale(movement, 1 / length);
    Vector2 desired = Vector2Scale(movement, input.sprint ? 18.3f : 4.1f);

    for (int i = 0; i < steps; ++i)
    {
        player->groundGrace = player->grounded ? 0.08f : (player->sliding ? 0 : fmaxf(0, player->groundGrace - step));
        if (player->jumpBuffered && player->groundGrace > 0)
        {
            player->velocity.y = 9.1f;
            player->grounded = false;
            player->groundGrace = 0;
            player->jumpBuffered = false;
        }

        bool followGround = (player->grounded || player->sliding) &&
            fabsf(player->feet.y - FloorAt(world, player->feet)) <= 2 * contact;
        Vector2 target = desired;
        Vector2 slide = { 0 };
        if (player->sliding)
        {
            Vector3 normal = GroundNormalAt(world, player->feet);
            Vector2 downhill = { normal.x, normal.z };
            Vector2 downhillDirection = Vector2Normalize(downhill);
            float uphillSpeed = fminf(Vector2DotProduct(target, downhillDirection), 0);
            target = Vector2Subtract(target, Vector2Scale(downhillDirection, uphillSpeed));
            slide = Vector2Scale(downhill, gravity * normal.y);
        }

        float response = player->grounded ? 24.0f : (player->sliding ? 3.5f : 9.0f);
        float approach = fminf(response * step, 1);
        player->velocity.x = Lerp(player->velocity.x, target.x, approach) + slide.x * step;
        player->velocity.z = Lerp(player->velocity.z, target.y, approach) + slide.y * step;
        if (target.x == 0 && slide.x == 0 && fabsf(player->velocity.x) < 0.01f) player->velocity.x = 0;
        if (target.y == 0 && slide.y == 0 && fabsf(player->velocity.z) < 0.01f) player->velocity.z = 0;
        player->velocity.y -= gravity * step;
        MoveHorizontal(player, world, step, true);
        MoveHorizontal(player, world, step, false);
        MoveVertical(player, world, step, followGround);
    }
}

Vector3 GetPlayerEye(const PlayerController *player)
{
    return Vector3Add(player->feet, (Vector3){ 0, player->eyeHeight, 0 });
}
