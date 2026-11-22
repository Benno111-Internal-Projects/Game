#ifndef DORF_WORLD_LEVELS_H
#define DORF_WORLD_LEVELS_H

#include "raylib.h"
#include <stddef.h>

enum { WORLD_COUNT = 7, WORLD_SIDE = 1024, WAYPOINT_COUNT = 3, WORLD_MAX_BRUSHES = 48, WORLD_BRUSH_LIMIT = 12 };

typedef struct WorldBrush {
    Vector2 center;
    float radius, height;
} WorldBrush;

typedef struct WorldSpec {
    char name[48];
    Color low, high;
    float relief, frequency, phase;
    Vector2 spawn, waypoints[WAYPOINT_COUNT], gate;
    WorldBrush brushes[WORLD_MAX_BRUSHES];
    int brushCount;
} WorldSpec;

extern WorldSpec worlds[WORLD_COUNT];

bool LoadWorldLevels(const char *directory);
void LoadWorldOverrides(void);
bool SaveWorldOverride(int index, const WorldSpec *world, char *path, size_t capacity);
float WorldHeight(const void *data, float x, float z);

#endif
