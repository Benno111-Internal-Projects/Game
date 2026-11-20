#ifndef RENDER_FRAMEWORK_CAMERA_H
#define RENDER_FRAMEWORK_CAMERA_H

#include "raylib.h"

typedef struct WalkCamera {
    Vector3 position;
    float yaw;
    float pitch;
} WalkCamera;

// movement.x is strafe, movement.y is forward. Distance is per frame.
void UpdateWalkCamera(Camera3D *camera, WalkCamera *walk, Vector2 mouse,
                      Vector2 movement, float distance);

#endif
