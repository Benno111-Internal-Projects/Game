#ifndef RENDER_FRAMEWORK_TERRAIN_H
#define RENDER_FRAMEWORK_TERRAIN_H

#include "raylib.h"

enum { TERRAIN_SIDE = 33, TERRAIN_SAMPLES = TERRAIN_SIDE * TERRAIN_SIDE };

typedef struct TerrainMap {
    float heights[TERRAIN_SAMPLES]; // Offsets from baseHeight, in row-major order.
    float baseHeight;
    Mesh mesh;
    Material material;
    bool enabled;
} TerrainMap;

// Initialize after opening the graphics context and unload before closing it.
bool InitTerrainMap(TerrainMap *map, float baseHeight);
void UnloadTerrainMap(TerrainMap *map);
void SyncTerrainMesh(TerrainMap *map);
void DrawTerrainMap(const TerrainMap *map);
float GetTerrainHeight(const TerrainMap *map, float x, float z);
Vector3 GetTerrainNormal(const TerrainMap *map, float x, float z);
RayCollision PickTerrain(const TerrainMap *map, Ray ray);

// Positive amount raises, negative amount lowers, and flatten levels to baseHeight.
bool SculptTerrain(TerrainMap *map, Vector3 center, float radius, float amount, bool flatten);

#endif
