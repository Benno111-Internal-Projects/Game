#include "scene.h"
#include "lod.h"
#include "occlusion.h"
#include "raymath.h"

SceneStats DrawScene(const SceneObject *objects, size_t count, Camera3D camera,
                     float nearPlane, bool occlusionEnabled)
{
    SceneStats stats = { 0 };
    for (size_t i = 0; i < count; ++i)
    {
        const SceneObject *object = &objects[i];
        if (!object->enabled || !object->draw) continue;

        bool hidden = false;
        if (occlusionEnabled)
        {
            for (size_t j = 0; j < count; ++j)
            {
                const SceneObject *blocker = &objects[j];
                if (j == i || !blocker->enabled || !blocker->draw || !blocker->opaqueBox) continue;
                if (IsBoxOccluded(camera, nearPlane, object->bounds, blocker->bounds))
                {
                    hidden = true;
                    break;
                }
            }
        }
        if (hidden)
        {
            ++stats.occluded;
            continue;
        }

        Vector3 center = Vector3Scale(Vector3Add(object->bounds.min, object->bounds.max), 0.5f);
        object->draw(object->data, SelectDistanceLod(camera.position, center,
            object->lodDistances, object->lodDistanceCount));
        ++stats.drawn;
    }
    return stats;
}
