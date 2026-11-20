#include "occlusion.h"
#include "raymath.h"

#include <math.h>

static Vector3 BoxCorner(BoundingBox box, int corner)
{
    return (Vector3){ corner & 1 ? box.max.x : box.min.x,
                      corner & 2 ? box.max.y : box.min.y,
                      corner & 4 ? box.max.z : box.min.z };
}

static bool SegmentHitsBox(Vector3 eye, Vector3 corner, BoundingBox box)
{
    Vector3 delta = Vector3Subtract(corner, eye);
    float direction[] = { delta.x, delta.y, delta.z };
    float origin[] = { eye.x, eye.y, eye.z };
    float lower[] = { box.min.x, box.min.y, box.min.z };
    float upper[] = { box.max.x, box.max.y, box.max.z };
    float enter = 0.0f, leave = 1.0f;

    for (int axis = 0; axis < 3; ++axis)
    {
        if (fabsf(direction[axis]) < 0.000001f)
        {
            if (origin[axis] <= lower[axis] || origin[axis] >= upper[axis]) return false;
            continue;
        }
        float a = (lower[axis] - origin[axis]) / direction[axis];
        float b = (upper[axis] - origin[axis]) / direction[axis];
        enter = fmaxf(enter, fminf(a, b));
        leave = fminf(leave, fmaxf(a, b));
        if (enter >= leave) return false;
    }
    return enter > 0.0f && enter < 1.0f;
}

bool IsBoxOccluded(Camera3D camera, float nearPlane, BoundingBox target, BoundingBox occluder)
{
    if (camera.projection != CAMERA_PERSPECTIVE || CheckCollisionBoxes(target, occluder))
        return false;

    // Avoid culling at the silhouette and when the camera clips into the blocker.
    const Vector3 margin = { 0.002f, 0.002f, 0.002f };
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    for (int i = 0; i < 8; ++i)
    {
        Vector3 offset = Vector3Subtract(BoxCorner(occluder, i), camera.position);
        if (Vector3DotProduct(offset, forward) <= nearPlane + margin.x) return false;
    }
    occluder.min = Vector3Add(occluder.min, margin);
    occluder.max = Vector3Subtract(occluder.max, margin);
    if (occluder.min.x >= occluder.max.x || occluder.min.y >= occluder.max.y ||
        occluder.min.z >= occluder.max.z) return false;

    // The volume behind a convex box, viewed from one point, is convex.
    // If all eight corners are in that volume, the entire target is covered.
    for (int i = 0; i < 8; ++i)
    {
        if (!SegmentHitsBox(camera.position, BoxCorner(target, i), occluder)) return false;
    }
    return true;
}
