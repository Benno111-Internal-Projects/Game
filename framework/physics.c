#include "physics.h"
#include "raymath.h"

#include <math.h>
#include <stddef.h>

static void StepSphere(PhysicsSphere *body, float floorHeight, PhysicsGroundHeight height,
                       const void *surface, float dt)
{
    if (body->resting || !(dt > 0.0f)) return;

    const float gravity = 9.81f;
    dt = fminf(dt, 0.1f);
    int steps = (int)ceilf(dt * 120.0f);
    float step = dt / steps;

    for (int i = 0; i < steps; ++i)
    {
        body->velocity.y -= gravity * step;
        body->position = Vector3Add(body->position, Vector3Scale(body->velocity, step));
        float ground = height ? height(surface, body->position.x, body->position.z) : floorHeight;
        float floorCenter = ground + body->radius;
        if (body->position.y > floorCenter) continue;

        body->position.y = floorCenter;
        if (body->velocity.y >= 0.0f) continue;

        body->velocity.y *= -Clamp(body->restitution, 0.0f, 1.0f);
        body->velocity.x *= 0.7f;
        body->velocity.z *= 0.7f;
        if (body->velocity.y < 0.35f)
        {
            body->velocity = Vector3Zero();
            body->resting = true;
            break;
        }
    }
}


void StepPhysicsSphere(PhysicsSphere *body, float floorHeight, float dt)
{
    StepSphere(body, floorHeight, NULL, NULL, dt);
}

void StepPhysicsSphereOnSurface(PhysicsSphere *body, PhysicsGroundHeight height,
                               const void *surface, float dt)
{
    StepSphere(body, 0, height, surface, dt);
}
