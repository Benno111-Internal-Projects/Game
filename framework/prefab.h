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
    // Optional override that also receives yaw around the world Y axis.
    void (*drawRotated)(const void *data, Vector3 position, float scale, float yawDegrees, size_t lod);
} ScenePrefab;

typedef struct PrefabInstance {
    const ScenePrefab *prefab;
    Vector3 position;
    float scale;
    bool enabled;
    float yawDegrees; // Used by prefabs that provide drawRotated.
} PrefabInstance;

// The instance, template, and draw data stay caller-owned. Rebuild the
// scene object after changing its instance to keep its bounds current.
SceneObject MakePrefabSceneObject(const PrefabInstance *instance);

#endif
