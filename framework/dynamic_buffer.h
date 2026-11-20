#ifndef RENDER_FRAMEWORK_DYNAMIC_BUFFER_H
#define RENDER_FRAMEWORK_DYNAMIC_BUFFER_H

#include "raylib.h"
#include <stddef.h>

typedef struct DynamicVertexBuffer {
    Mesh mesh;
    size_t capacity;
} DynamicVertexBuffer;

// Zero-initialize before use. Positions contain three floats per vertex.
// Upload replaces the active triangle list, growing the GPU allocation as needed.
bool UploadDynamicTriangles(DynamicVertexBuffer *buffer, const float *positions, size_t vertexCount);
void DrawDynamicTriangles(const DynamicVertexBuffer *buffer, Material material, Matrix transform);
void UnloadDynamicTriangles(DynamicVertexBuffer *buffer);

#endif
