#include "lod.h"
#include "raymath.h"

size_t SelectDistanceLod(Vector3 camera, Vector3 center, const float *distances, size_t count)
{
    float distanceSquared = Vector3DistanceSqr(camera, center);
    size_t level = 0;
    while (level < count && distanceSquared >= distances[level] * distances[level]) ++level;
    return level;
}
