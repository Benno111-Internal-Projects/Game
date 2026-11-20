#include "dynamic_buffer.h"

#include <limits.h>
#include <string.h>

bool UploadDynamicTriangles(DynamicVertexBuffer *buffer, const float *positions, size_t vertexCount)
{
    const size_t stride = 3 * sizeof(float);
    const size_t maxVertices = INT_MAX / stride;
    if (vertexCount % 3 || vertexCount > maxVertices || (vertexCount && !positions)) return false;

    if (vertexCount > buffer->capacity)
    {
        size_t capacity = buffer->capacity ? buffer->capacity : 3;
        while (capacity < vertexCount)
            capacity = capacity <= maxVertices / 2 ? capacity * 2 : maxVertices;

        Mesh mesh = { .vertexCount = (int)capacity };
        mesh.vertices = MemAlloc((unsigned int)(capacity * stride));
        if (!mesh.vertices) return false;
        memset(mesh.vertices, 0, capacity * stride);
        memcpy(mesh.vertices, positions, vertexCount * stride);
        UploadMesh(&mesh, true);
        if (!mesh.vboId || !mesh.vboId[0])
        {
            UnloadMesh(mesh);
            return false;
        }

        UnloadDynamicTriangles(buffer);
        buffer->mesh = mesh;
        buffer->capacity = capacity;
    }
    else if (vertexCount)
    {
        UpdateMeshBuffer(buffer->mesh, 0, positions, (int)(vertexCount * stride), 0);
    }

    buffer->mesh.vertexCount = (int)vertexCount;
    buffer->mesh.triangleCount = (int)(vertexCount / 3);
    return true;
}

void DrawDynamicTriangles(const DynamicVertexBuffer *buffer, Material material, Matrix transform)
{
    if (buffer->mesh.vertexCount) DrawMesh(buffer->mesh, material, transform);
}

void UnloadDynamicTriangles(DynamicVertexBuffer *buffer)
{
    if (buffer->capacity) UnloadMesh(buffer->mesh);
    *buffer = (DynamicVertexBuffer){ 0 };
}
