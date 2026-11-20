#ifndef RENDER_FRAMEWORK_PREFAB_H
#define RENDER_FRAMEWORK_PREFAB_H

#include "scene.h"

typedef struct ScenePrefab {
    BoundingBox localBounds;
    const float *lodDistances;
    size_t lodDistanceCount;
    void (*draw)(const void *data, Vector3 position, float scale, size_t lod);
    const void *data;
    bool opaqueBox;
} ScenePrefab;

typedef struct PrefabInstance {
    const ScenePrefab *prefab;
    Vector3 position;
    float scale;
    bool enabled;
} PrefabInstance;

// The instance, template, and draw data stay caller-owned. Rebuild the
// scene object after changing its instance to keep its bounds current.
SceneObject MakePrefabSceneObject(const PrefabInstance *instance);

#endif
