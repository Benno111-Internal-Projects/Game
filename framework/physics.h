#ifndef RENDER_FRAMEWORK_PHYSICS_H
#define RENDER_FRAMEWORK_PHYSICS_H

#include "raylib.h"

typedef struct PhysicsSphere {
    Vector3 position;
    Vector3 velocity;
    float radius;
    float restitution;
    bool resting;
} PhysicsSphere;

// Integrates gravity and collisions with an infinite horizontal floor.
// Set resting to false after applying an impulse.
void StepPhysicsSphere(PhysicsSphere *body, float floorHeight, float dt);

typedef float (*PhysicsGroundHeight)(const void *surface, float x, float z);

// Samples the ground under the body during each physics substep.
void StepPhysicsSphereOnSurface(PhysicsSphere *body, PhysicsGroundHeight height,
                               const void *surface, float dt);

#endif
