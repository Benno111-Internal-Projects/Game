#ifndef RENDER_FRAMEWORK_PLAYER_H
#define RENDER_FRAMEWORK_PLAYER_H

#include "physics.h"
#include <stddef.h>

typedef struct PlayerWorld {
    PhysicsGroundHeight groundHeight;
    const void *ground;
    const BoundingBox *boxes;
    size_t boxCount;
} PlayerWorld;

typedef struct PlayerController {
    Vector3 feet;
    Vector3 velocity;
    float radius, height, eyeHeight;
    float jumpBuffer, groundGrace;
    bool grounded;
} PlayerController;

typedef struct PlayerInput {
    Vector2 movement; // World-space X and Z input, with magnitude up to 1.
    bool jumpPressed;
    bool sprint;
} PlayerInput;

// The supplied position uses world X and Z. The world determines the spawn height.
PlayerController SpawnPlayer(Vector2 position, const PlayerWorld *world);
void StepPlayer(PlayerController *player, const PlayerWorld *world, PlayerInput input, float dt);
Vector3 GetPlayerEye(const PlayerController *player);

#endif
