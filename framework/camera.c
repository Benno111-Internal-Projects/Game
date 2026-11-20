#include "camera.h"
#include "raymath.h"

void UpdateWalkCamera(Camera3D *camera, WalkCamera *walk, Vector2 mouse,
                      Vector2 movement, float distance)
{
    walk->yaw -= mouse.x * 0.003f;
    walk->pitch = Clamp(walk->pitch - mouse.y * 0.003f, -1.48f, 1.48f);
    Vector3 forward = { sinf(walk->yaw), 0.0f, cosf(walk->yaw) };
    Vector3 right = { -forward.z, 0.0f, forward.x };
    Vector3 direction = Vector3Add(Vector3Scale(forward, movement.y), Vector3Scale(right, movement.x));
    walk->position = Vector3Add(walk->position,
        Vector3Scale(Vector3Normalize(direction), distance));

    Vector3 look = Vector3Scale(forward, cosf(walk->pitch));
    look.y = sinf(walk->pitch);
    camera->position = walk->position;
    camera->target = Vector3Add(walk->position, look);
}
