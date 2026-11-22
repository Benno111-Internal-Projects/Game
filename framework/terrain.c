#include "terrain.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stddef.h>

static const Vector2 editorOrigin = { -10.0f, -10.0f };
static const float editorSpacing = 0.625f;
static const Color low = { 68, 91, 74, 255 };
static const Color high = { 156, 186, 122, 255 };

static Vector3 SamplePoint(const TerrainMap *map, int x, int z)
{
    Vector2 origin = map->source ? map->origin : editorOrigin;
    float spacing = map->source ? map->spacing : editorSpacing;
    return (Vector3){ origin.x + x * spacing, map->baseHeight + map->heights[z * TERRAIN_SIDE + x],
                      origin.y + z * spacing };
}

static Vector3 NeighborPoint(const TerrainMap *map, int x, int z)
{
    if (x >= 0 && x < TERRAIN_SIDE && z >= 0 && z < TERRAIN_SIDE) return SamplePoint(map, x, z);
    if (map->source)
    {
        Vector3 point = { map->origin.x + x * map->spacing, 0, map->origin.y + z * map->spacing };
        point.y = map->source(map->sourceData, point.x, point.z);
        return point;
    }
    return SamplePoint(map, (int)Clamp(x, 0, TERRAIN_SIDE - 1), (int)Clamp(z, 0, TERRAIN_SIDE - 1));
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

            // Raylib's non-VAO mesh path always binds a primary UV buffer.
            // Store world X/Z so layered texture coordinates join across chunks.
            map->mesh.texcoords[i * 2] = point.x;
            map->mesh.texcoords[i * 2 + 1] = point.z;

            Vector3 dx = Vector3Subtract(NeighborPoint(map, x + 1, z), NeighborPoint(map, x - 1, z));
            Vector3 dz = Vector3Subtract(NeighborPoint(map, x, z + 1), NeighborPoint(map, x, z - 1));
            Vector3 normal = Vector3Normalize(Vector3CrossProduct(dz, dx));
            map->mesh.normals[3 * i] = normal.x;
            map->mesh.normals[3 * i + 1] = normal.y;
            map->mesh.normals[3 * i + 2] = normal.z;
            float shade = 0.42f + 0.58f * fmaxf(Vector3DotProduct(normal, light), 0.0f);
            float elevation = (map->heights[i] + 2.0f) / 6.0f;
            if (map->source)
            {
                elevation = 0.5f + map->heights[i] / (2.0f * map->colorHeightRange);
                float grain = sinf(point.x * 1.37f + point.z * 0.83f) * cosf(point.z * 1.11f);
                shade *= 0.93f + 0.07f * grain;
            }
            Color color = ColorLerp(map->lowColor, map->highColor, Clamp(elevation, 0.0f, 1.0f));
            map->mesh.colors[i * 4] = (unsigned char)(color.r * shade);
            map->mesh.colors[i * 4 + 1] = (unsigned char)(color.g * shade);
            map->mesh.colors[i * 4 + 2] = (unsigned char)(color.b * shade);
            map->mesh.colors[i * 4 + 3] = 255;
        }
    }
}

static bool InitMesh(TerrainMap *map, bool dynamic)
{
    const int cells = TERRAIN_SIDE - 1;
    Mesh *mesh = &map->mesh;
    mesh->vertexCount = TERRAIN_SAMPLES;
    mesh->triangleCount = cells * cells * 2;
    mesh->vertices = MemAlloc(TERRAIN_SAMPLES * 3 * sizeof(float));
    mesh->texcoords = MemAlloc(TERRAIN_SAMPLES * 2 * sizeof(float));
    mesh->normals = MemAlloc(TERRAIN_SAMPLES * 3 * sizeof(float));
    mesh->colors = MemAlloc(TERRAIN_SAMPLES * 4);
    mesh->indices = MemAlloc((unsigned int)mesh->triangleCount * 3 * sizeof(unsigned short));
    if (!mesh->vertices || !mesh->texcoords || !mesh->normals || !mesh->colors || !mesh->indices)
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
    UploadMesh(mesh, dynamic);
    map->material = LoadMaterialDefault();
    if (!mesh->vboId || !mesh->vboId[0] || !mesh->vboId[1] || !mesh->vboId[2] || !mesh->vboId[3] || !mesh->vboId[6] || !map->material.maps)
    {
        UnloadTerrainMap(map);
        return false;
    }
    return true;
}

bool InitTerrainMap(TerrainMap *map, float baseHeight)
{
    *map = (TerrainMap){ .baseHeight = baseHeight, .lowColor = low, .highColor = high };
    return InitMesh(map, true);
}

bool InitTerrainChunk(TerrainMap *map, Vector2 origin, float spacing,
                      Color lowColor, Color highColor, float colorHeightRange,
                      TerrainHeightSource source, const void *data)
{
    if (!source || !isfinite(spacing) || spacing <= 0)
    {
        *map = (TerrainMap){0};
        return false;
    }
    *map = (TerrainMap){
        .origin = origin, .spacing = spacing, .lowColor = lowColor, .highColor = highColor,
        .source = source, .sourceData = data, .colorHeightRange = fmaxf(colorHeightRange, 0.001f),
    };
    for (int z = 0; z < TERRAIN_SIDE; ++z)
        for (int x = 0; x < TERRAIN_SIDE; ++x)
            map->heights[z * TERRAIN_SIDE + x] = source(data, origin.x + x * spacing, origin.y + z * spacing);
    map->enabled = InitMesh(map, false);
    return map->enabled;
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
    UpdateMeshBuffer(map->mesh, 2, map->mesh.normals, TERRAIN_SAMPLES * 3 * sizeof(float), 0);
    UpdateMeshBuffer(map->mesh, 3, map->mesh.colors, TERRAIN_SAMPLES * 4, 0);
}

void DrawTerrainMap(const TerrainMap *map)
{
    if (map->enabled) DrawMesh(map->mesh, map->material, MatrixIdentity());
}

bool LoadTerrainLayers(TerrainLayers *layers, const char *assetDir, const char *shaderDir)
{
    *layers = (TerrainLayers){ .material = LoadMaterialDefault() };
    if (!layers->material.maps) return false;
    Shader shader = LoadShader(TextFormat("%sterrain.vert", shaderDir), TextFormat("%sterrain.frag", shaderDir));
    if (!IsShaderValid(shader) || shader.id == rlGetShaderIdDefault()) goto failure;
    layers->material.shader = shader;
    layers->elevationLocation = GetShaderLocation(shader, "elevation");
    if (layers->elevationLocation < 0) goto failure;

    static const char *const names[] = { "ground", "sediment", "rock" };
    for (int i = 0; i < 3; ++i)
    {
        int slot = MATERIAL_MAP_ALBEDO + i;
        int location = GetShaderLocation(shader, names[i]);
        if (location < 0) goto failure;
        shader.locs[SHADER_LOC_MAP_ALBEDO + i] = location;
        Texture2D texture = LoadTexture(TextFormat("%sterrain/%s.png", assetDir, names[i]));
        if (!IsTextureValid(texture)) goto failure;
        GenTextureMipmaps(&texture);
        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
        SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
        SetMaterialTexture(&layers->material, slot, texture);
    }
    return true;

failure:
    UnloadTerrainLayers(layers);
    return false;
}

void UnloadTerrainLayers(TerrainLayers *layers)
{
    if (layers->material.maps) UnloadMaterial(layers->material);
    *layers = (TerrainLayers){0};
}

void SetTerrainLayerElevation(const TerrainLayers *layers, float baseHeight, float heightRange)
{
    Vector2 elevation = { baseHeight, 1.0f / fmaxf(heightRange, 1.0f) };
    SetShaderValue(layers->material.shader, layers->elevationLocation, &elevation, SHADER_UNIFORM_VEC2);
}

void DrawLayeredTerrainMap(const TerrainMap *map, const TerrainLayers *layers)
{
    if (map->enabled) DrawMesh(map->mesh, layers->material, MatrixIdentity());
}

static float TerrainTriangle(float a, float b, float c, float d, float spacing,
                             float u, float v, Vector3 *normal)
{
    if (u + v <= 1)
    {
        if (normal) *normal = Vector3Normalize((Vector3){ a - c, spacing, a - b });
        return a + u * (c - a) + v * (b - a);
    }
    if (normal) *normal = Vector3Normalize((Vector3){ b - d, spacing, c - d });
    return d + (1 - u) * (b - d) + (1 - v) * (c - d);
}

float SampleTerrainGrid(TerrainHeightSource source, const void *data, Vector2 origin,
                        float spacing, float x, float z, Vector3 *normal)
{
    float gx = (x - origin.x) / spacing, gz = (z - origin.y) / spacing;
    float ix = floorf(gx), iz = floorf(gz);
    float left = origin.x + ix * spacing, front = origin.y + iz * spacing;
    return TerrainTriangle(source(data, left, front), source(data, left, front + spacing),
                           source(data, left + spacing, front), source(data, left + spacing, front + spacing),
                           spacing, gx - ix, gz - iz, normal);
}

static float SampleTerrainSurface(const TerrainMap *map, float x, float z, Vector3 *normal)
{
    if (normal) *normal = (Vector3){ 0, 1, 0 };
    if (!map->enabled) return map->baseHeight;
    Vector2 origin = map->source ? map->origin : editorOrigin;
    float spacing = map->source ? map->spacing : editorSpacing;
    float gx = (x - origin.x) / spacing, gz = (z - origin.y) / spacing;
    if (!(gx >= 0 && gz >= 0 && gx <= TERRAIN_SIDE - 1 && gz <= TERRAIN_SIDE - 1)) return map->baseHeight;
    int ix = (int)fminf(gx, TERRAIN_SIDE - 2), iz = (int)fminf(gz, TERRAIN_SIDE - 2);
    return TerrainTriangle(SamplePoint(map, ix, iz).y, SamplePoint(map, ix, iz + 1).y,
                           SamplePoint(map, ix + 1, iz).y, SamplePoint(map, ix + 1, iz + 1).y,
                           spacing, gx - ix, gz - iz, normal);
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
