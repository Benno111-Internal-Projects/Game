#include "prefab.h"
#include "raymath.h"

#include <math.h>

static void DrawPrefabInstance(const void *data, size_t lod)
{
    const PrefabInstance *instance = data;
    const ScenePrefab *prefab = instance->prefab;
    prefab->draw(prefab->data, instance->position, instance->scale, lod);
}

SceneObject MakePrefabSceneObject(const PrefabInstance *instance)
{
    if (!instance || !instance->prefab || !instance->prefab->draw ||
        !isfinite(instance->scale) || instance->scale <= 0.0f)
        return (SceneObject){ 0 };

    const ScenePrefab *prefab = instance->prefab;
    return (SceneObject){
        .bounds = {
            Vector3Add(instance->position, Vector3Scale(prefab->localBounds.min, instance->scale)),
            Vector3Add(instance->position, Vector3Scale(prefab->localBounds.max, instance->scale)),
        },
        .lodDistances = prefab->lodDistances,
        .lodDistanceCount = prefab->lodDistanceCount,
        .draw = DrawPrefabInstance,
        .data = instance,
        .enabled = instance->enabled,
        .opaqueBox = prefab->opaqueBox,
    };
}
