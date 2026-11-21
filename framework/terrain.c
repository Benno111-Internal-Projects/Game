#include "terrain.h"
#include "raymath.h"
#include <math.h>
#include <stddef.h>

static const float spacing = 0.625f;
static const float edge = 10.0f;
static const Color low = { 68, 91, 74, 255 };
static const Color high = { 156, 186, 122, 255 };

static Vector3 SamplePoint(const TerrainMap *map, int x, int z)
{
    return (Vector3){ x * spacing - edge, map->baseHeight + map->heights[z * TERRAIN_SIDE + x],
                      z * spacing - edge };
}

static void FillVertices(TerrainMap *map)
{
    Vector3 light = Vector3Normalize((Vector3){ -0.6f, 1.0f, 0.45f });
    for (int z = 0; z < TERRAIN_SIDE; ++z)
    {
        for (int x = 0; x < TERRAIN_SIDE; ++x)
        {
            int i = z * TERRAIN_SIDE + x;
            Vector3 point = SamplePoint(map, x, z);
            map->mesh.vertices[i * 3] = point.x;
            map->mesh.vertices[i * 3 + 1] = point.y;
            map->mesh.vertices[i * 3 + 2] = point.z;

            Vector3 dx = Vector3Subtract(SamplePoint(map, x < TERRAIN_SIDE - 1 ? x + 1 : x, z),
                                         SamplePoint(map, x > 0 ? x - 1 : x, z));
            Vector3 dz = Vector3Subtract(SamplePoint(map, x, z < TERRAIN_SIDE - 1 ? z + 1 : z),
                                         SamplePoint(map, x, z > 0 ? z - 1 : z));
            Vector3 normal = Vector3Normalize(Vector3CrossProduct(dz, dx));
            float shade = 0.42f + 0.58f * fmaxf(Vector3DotProduct(normal, light), 0.0f);
            Color color = ColorLerp(low, high, Clamp((map->heights[i] + 2.0f) / 6.0f, 0.0f, 1.0f));
            map->mesh.colors[i * 4] = (unsigned char)(color.r * shade);
            map->mesh.colors[i * 4 + 1] = (unsigned char)(color.g * shade);
            map->mesh.colors[i * 4 + 2] = (unsigned char)(color.b * shade);
            map->mesh.colors[i * 4 + 3] = 255;
        }
    }
}

bool InitTerrainMap(TerrainMap *map, float baseHeight)
{
    *map = (TerrainMap){ .baseHeight = baseHeight };
    const int cells = TERRAIN_SIDE - 1;
    Mesh *mesh = &map->mesh;
    mesh->vertexCount = TERRAIN_SAMPLES;
    mesh->triangleCount = cells * cells * 2;
    mesh->vertices = MemAlloc(TERRAIN_SAMPLES * 3 * sizeof(float));
    mesh->colors = MemAlloc(TERRAIN_SAMPLES * 4);
    mesh->indices = MemAlloc((unsigned int)mesh->triangleCount * 3 * sizeof(unsigned short));
    if (!mesh->vertices || !mesh->colors || !mesh->indices)
    {
        UnloadTerrainMap(map);
        return false;
    }
    FillVertices(map);
    int next = 0;
    for (int z = 0; z < cells; ++z)
    {
        for (int x = 0; x < cells; ++x)
        {
            unsigned short a = (unsigned short)(z * TERRAIN_SIDE + x);
            unsigned short b = (unsigned short)(a + TERRAIN_SIDE);
            mesh->indices[next++] = a;
            mesh->indices[next++] = b;
            mesh->indices[next++] = (unsigned short)(a + 1);
            mesh->indices[next++] = (unsigned short)(a + 1);
            mesh->indices[next++] = b;
            mesh->indices[next++] = (unsigned short)(b + 1);
        }
    }
    UploadMesh(mesh, true);
    map->material = LoadMaterialDefault();
    if (!mesh->vboId || !mesh->vboId[0] || !mesh->vboId[3] || !mesh->vboId[6] || !map->material.maps)
    {
        UnloadTerrainMap(map);
        return false;
    }
    return true;
}

void UnloadTerrainMap(TerrainMap *map)
{
    UnloadMesh(map->mesh);
    if (map->material.maps) UnloadMaterial(map->material);
    *map = (TerrainMap){ 0 };
}

void SyncTerrainMesh(TerrainMap *map)
{
    FillVertices(map);
    UpdateMeshBuffer(map->mesh, 0, map->mesh.vertices, TERRAIN_SAMPLES * 3 * sizeof(float), 0);
    UpdateMeshBuffer(map->mesh, 3, map->mesh.colors, TERRAIN_SAMPLES * 4, 0);
}

void DrawTerrainMap(const TerrainMap *map)
{
    if (map->enabled) DrawMesh(map->mesh, map->material, MatrixIdentity());
}

static float SampleTerrainSurface(const TerrainMap *map, float x, float z, Vector3 *normal)
{
    if (normal) *normal = (Vector3){ 0, 1, 0 };
    if (!map->enabled || !(x >= -edge && x <= edge && z >= -edge && z <= edge))
        return map->baseHeight;
    float gx = (x + edge) / spacing, gz = (z + edge) / spacing;
    int ix = (int)fminf(gx, TERRAIN_SIDE - 2), iz = (int)fminf(gz, TERRAIN_SIDE - 2);
    float u = gx - ix, v = gz - iz;
    float a = SamplePoint(map, ix, iz).y, b = SamplePoint(map, ix, iz + 1).y;
    float c = SamplePoint(map, ix + 1, iz).y, d = SamplePoint(map, ix + 1, iz + 1).y;
    if (u + v <= 1)
    {
        if (normal) *normal = Vector3Normalize((Vector3){ a - c, spacing, a - b });
        return a + u * (c - a) + v * (b - a);
    }
    if (normal) *normal = Vector3Normalize((Vector3){ b - d, spacing, c - d });
    return d + (1 - u) * (b - d) + (1 - v) * (c - d);
}

float GetTerrainHeight(const TerrainMap *map, float x, float z)
{
    return SampleTerrainSurface(map, x, z, NULL);
}

Vector3 GetTerrainNormal(const TerrainMap *map, float x, float z)
{
    Vector3 normal;
    SampleTerrainSurface(map, x, z, &normal);
    return normal;
}

RayCollision PickTerrain(const TerrainMap *map, Ray ray)
{
    RayCollision nearest = { 0 };
    if (!map->enabled) return nearest;
    for (int z = 0; z < TERRAIN_SIDE - 1; ++z)
    {
        for (int x = 0; x < TERRAIN_SIDE - 1; ++x)
        {
            Vector3 a = SamplePoint(map, x, z), b = SamplePoint(map, x, z + 1);
            Vector3 c = SamplePoint(map, x + 1, z), d = SamplePoint(map, x + 1, z + 1);
            RayCollision first = GetRayCollisionTriangle(ray, a, b, c);
            RayCollision second = GetRayCollisionTriangle(ray, c, b, d);
            if (first.hit && (!nearest.hit || first.distance < nearest.distance)) nearest = first;
            if (second.hit && (!nearest.hit || second.distance < nearest.distance)) nearest = second;
        }
    }
    return nearest;
}

bool SculptTerrain(TerrainMap *map, Vector3 center, float radius, float amount, bool flatten)
{
    if (!map->enabled || !isfinite(radius) || radius <= 0 || !isfinite(amount)) return false;
    bool changed = false;
    // Leave the outer ring flat so the mesh meets the ground at its border.
    for (int z = 1; z < TERRAIN_SIDE - 1; ++z)
    {
        for (int x = 1; x < TERRAIN_SIDE - 1; ++x)
        {
            Vector3 point = SamplePoint(map, x, z);
            float distance = Vector2Distance((Vector2){ point.x, point.z }, (Vector2){ center.x, center.z });
            if (!(distance < radius)) continue;
            float weight = 1.0f - distance / radius;
            float *height = &map->heights[z * TERRAIN_SIDE + x];
            float value = flatten ? Lerp(*height, 0, Clamp(fabsf(amount) * weight, 0, 1))
                                  : Clamp(*height + amount * weight, -4.0f, 6.0f);
            changed |= value != *height;
            *height = value;
        }
    }
    if (changed) SyncTerrainMesh(map);
    return changed;
}
