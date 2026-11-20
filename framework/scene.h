#ifndef RENDER_FRAMEWORK_SCENE_H
#define RENDER_FRAMEWORK_SCENE_H

#include "raylib.h"
#include <stddef.h>

typedef struct SceneObject {
    BoundingBox bounds; // World bounds enclose the draw at every LOD.
    const float *lodDistances;
    size_t lodDistanceCount;
    void (*draw)(const void *data, size_t lod);
    const void *data;
    bool enabled;
    bool opaqueBox; // Every LOD must cover the solid box in bounds.
} SceneObject;

typedef struct SceneStats {
    size_t drawn;
    size_t occluded;
} SceneStats;

// Call inside BeginMode3D. Scene objects and draw resources are caller-owned.
SceneStats DrawScene(const SceneObject *objects, size_t count, Camera3D camera,
                     float nearPlane, bool occlusionEnabled);

#endif
