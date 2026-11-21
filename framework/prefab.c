#include "prefab.h"
#include "raymath.h"

#include <math.h>

static void DrawPrefabInstance(const void *data, size_t lod)
{
    const PrefabInstance *instance = data;
    const ScenePrefab *prefab = instance->prefab;
    if (prefab->drawRotated)
        prefab->drawRotated(prefab->data, instance->position, instance->scale, instance->yawDegrees, lod);
    else
        prefab->draw(prefab->data, instance->position, instance->scale, lod);
}

SceneObject MakePrefabSceneObject(const PrefabInstance *instance)
{
    if (!instance || !instance->prefab || (!instance->prefab->draw && !instance->prefab->drawRotated) ||
        !isfinite(instance->scale) || instance->scale <= 0.0f)
        return (SceneObject){ 0 };

    const ScenePrefab *prefab = instance->prefab;
    BoundingBox bounds = prefab->localBounds;
    float yaw = prefab->drawRotated ? instance->yawDegrees : 0.0f;
    if (!isfinite(yaw)) return (SceneObject){ 0 };
    bool rotated = fmodf(yaw, 360.0f) != 0.0f;
    if (rotated)
    {
        Vector3 halfSize = Vector3Scale(Vector3Subtract(bounds.max, bounds.min), 0.5f);
        Vector3 center = Vector3Scale(Vector3Add(bounds.min, bounds.max), 0.5f);
        float angle = yaw * DEG2RAD;
        center = Vector3RotateByAxisAngle(center, (Vector3){ 0, 1, 0 }, angle);
        float c = fabsf(cosf(angle)), s = fabsf(sinf(angle));
        Vector3 extent = { c * halfSize.x + s * halfSize.z, halfSize.y,
                           s * halfSize.x + c * halfSize.z };
        bounds = (BoundingBox){ Vector3Subtract(center, extent), Vector3Add(center, extent) };
    }
    return (SceneObject){
        .bounds = {
            Vector3Add(instance->position, Vector3Scale(bounds.min, instance->scale)),
            Vector3Add(instance->position, Vector3Scale(bounds.max, instance->scale)),
        },
        .lodDistances = prefab->lodDistances,
        .lodDistanceCount = prefab->lodDistanceCount,
        .draw = DrawPrefabInstance,
        .data = instance,
        .enabled = instance->enabled,
        .opaqueBox = prefab->opaqueBox && !rotated,
    };
}
