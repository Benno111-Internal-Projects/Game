#ifndef RENDER_FRAMEWORK_LOD_H
#define RENDER_FRAMEWORK_LOD_H

#include "raylib.h"
#include <stddef.h>

// Each sorted distance starts the next coarser level. No distances means level 0.
size_t SelectDistanceLod(Vector3 camera, Vector3 center, const float *distances, size_t count);

#endif
