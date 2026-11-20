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

#endif
