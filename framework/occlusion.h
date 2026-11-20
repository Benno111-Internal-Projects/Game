#ifndef RENDER_FRAMEWORK_OCCLUSION_H
#define RENDER_FRAMEWORK_OCCLUSION_H

#include "raylib.h"

// The occluder must be a solid, opaque world-space box contained in the rendered geometry.
bool IsBoxOccluded(Camera3D camera, float nearPlane, BoundingBox target, BoundingBox occluder);

#endif
