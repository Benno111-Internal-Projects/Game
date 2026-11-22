#include "world_levels.h"
#include "raymath.h"
#include "user_content.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

WorldSpec worlds[WORLD_COUNT];

static char *NextLine(char **cursor)
{
    if (!**cursor) return NULL;
    char *line = *cursor;
    char *end = strchr(line, '\n');
    *cursor = end ? end + 1 : line + strlen(line);
    if (end) *end = '\0';
    size_t length = strlen(line);
    if (length && line[length - 1] == '\r') line[length - 1] = '\0';
    return line;
}

static bool ReadValues(char **cursor, const char *field, float *values, int count)
{
    char *line = NextLine(cursor);
    size_t length = strlen(field);
    if (!line || strncmp(line, field, length) != 0 || line[length] != ' ') return false;
    line += length + 1;
    for (int i = 0; i < count; ++i)
    {
        char *end;
        values[i] = strtof(line, &end);
        if (end == line || !isfinite(values[i]) || (*end && !isspace((unsigned char)*end))) return false;
        line = end;
    }
    while (isspace((unsigned char)*line)) ++line;
    return !*line;
}

static bool ReadColor(char **cursor, const char *field, Color *color)
{
    float rgb[3];
    if (!ReadValues(cursor, field, rgb, 3)) return false;
    for (int i = 0; i < 3; ++i)
        if (rgb[i] < 0 || rgb[i] > 255 || rgb[i] != floorf(rgb[i])) return false;
    *color = (Color){ (unsigned char)rgb[0], (unsigned char)rgb[1], (unsigned char)rgb[2], 255 };
    return true;
}

static bool Inside(float x, float z)
{
    return x >= 4 && z >= 4 && x <= WORLD_SIDE - 4 && z <= WORLD_SIDE - 4;
}

static bool ReadPoint(char **cursor, const char *field, Vector2 *point)
{
    float p[2];
    if (!ReadValues(cursor, field, p, 2) || !Inside(p[0], p[1])) return false;
    *point = (Vector2){ p[0], p[1] };
    return true;
}

static bool ParseWorld(char *cursor, WorldSpec *world)
{
    *world = (WorldSpec){0};
    const char *line = NextLine(&cursor);
    if (!line || strcmp(line, "DORF-WORLD 1") != 0) return false;
    line = NextLine(&cursor);
    if (!line || strncmp(line, "name ", 5) != 0 || !line[5] || strlen(line + 5) >= sizeof(world->name)) return false;
    strcpy(world->name, line + 5);
    float terrain[3];
    if (!ReadColor(&cursor, "low", &world->low) || !ReadColor(&cursor, "high", &world->high) ||
        !ReadValues(&cursor, "terrain", terrain, 3) || terrain[0] < 0 || terrain[0] > 32 ||
        terrain[1] < 0.001f || terrain[1] > 0.06f || fabsf(terrain[2]) > 100 ||
        !ReadPoint(&cursor, "spawn", &world->spawn)) return false;
    world->relief = terrain[0];
    world->frequency = terrain[1];
    world->phase = terrain[2];
    for (int i = 0; i < WAYPOINT_COUNT; ++i)
        if (!ReadPoint(&cursor, "beacon", &world->waypoints[i])) return false;
    if (!ReadPoint(&cursor, "gate", &world->gate)) return false;
    while (*cursor)
    {
        float brush[4];
        if (world->brushCount == WORLD_MAX_BRUSHES || !ReadValues(&cursor, "brush", brush, 4) ||
            !Inside(brush[0], brush[1]) || brush[2] < 8 || brush[2] > 96 || fabsf(brush[3]) > 5) return false;
        world->brushes[world->brushCount++] = (WorldBrush){ { brush[0], brush[1] }, brush[2], brush[3] };
    }
    return true;
}

static bool ReadWorldFile(const char *path, WorldSpec *world)
{
    char *text = LoadFileText(path);
    bool valid = text && ParseWorld(text, world);
    if (text) UnloadFileText(text);
    return valid;
}

bool LoadWorldLevels(const char *directory)
{
    WorldSpec pack[WORLD_COUNT];
    for (int i = 0; i < WORLD_COUNT; ++i)
    {
        char path[4096];
        int length = snprintf(path, sizeof(path), "%s/%02d.world", directory, i + 1);
        if (length < 0 || (size_t)length >= sizeof(path) || !ReadWorldFile(path, &pack[i]))
        {
            TraceLog(LOG_ERROR, "Invalid world %d in %s", i + 1, directory);
            return false;
        }
    }
    memcpy(worlds, pack, sizeof(pack));
    return true;
}

static bool OverridePath(int index, char *path, size_t capacity, bool create)
{
    char filename[32];
    snprintf(filename, sizeof(filename), "%02d.world", index + 1);
    return UserContentPath(path, capacity, filename, create);
}

void LoadWorldOverrides(void)
{
    for (int i = 0; i < WORLD_COUNT; ++i)
    {
        char path[1024];
        WorldSpec candidate;
        if (!OverridePath(i, path, sizeof(path), false)) continue;
#if !defined(PLATFORM_ANDROID)
        if (!FileExists(path)) continue;
#endif
        if (ReadWorldFile(path, &candidate)) worlds[i] = candidate;
    }
}

bool SaveWorldOverride(int index, const WorldSpec *world, char *path, size_t capacity)
{
    if (index < 0 || index >= WORLD_COUNT || world->brushCount < 0 || world->brushCount > WORLD_MAX_BRUSHES ||
        !OverridePath(index, path, capacity, true)) return false;
    char text[4096];
    int length = snprintf(text, sizeof(text),
        "DORF-WORLD 1\nname %s\nlow %u %u %u\nhigh %u %u %u\nterrain %.9g %.9g %.9g\nspawn %.9g %.9g\n",
        world->name, (unsigned)world->low.r, (unsigned)world->low.g, (unsigned)world->low.b,
        (unsigned)world->high.r, (unsigned)world->high.g, (unsigned)world->high.b,
        world->relief, world->frequency, world->phase, world->spawn.x, world->spawn.y);
    if (length < 0 || (size_t)length >= sizeof(text)) return false;
    size_t used = (size_t)length;
    for (int i = 0; i <= WAYPOINT_COUNT + world->brushCount; ++i)
    {
        if (i < WAYPOINT_COUNT)
            length = snprintf(text + used, sizeof(text) - used, "beacon %.9g %.9g\n",
                              world->waypoints[i].x, world->waypoints[i].y);
        else if (i == WAYPOINT_COUNT)
            length = snprintf(text + used, sizeof(text) - used, "gate %.9g %.9g\n", world->gate.x, world->gate.y);
        else
        {
            const WorldBrush *brush = &world->brushes[i - WAYPOINT_COUNT - 1];
            length = snprintf(text + used, sizeof(text) - used, "brush %.9g %.9g %.9g %.9g\n",
                              brush->center.x, brush->center.y, brush->radius, brush->height);
        }
        if (length < 0 || (size_t)length >= sizeof(text) - used) return false;
        used += (size_t)length;
    }
    char copy[sizeof(text)];
    memcpy(copy, text, used + 1);
    WorldSpec validated;
    if (!ParseWorld(copy, &validated) || !SaveFileText(path, text)) return false;
    worlds[index] = validated;
    return true;
}

float WorldHeight(const void *data, float x, float z)
{
    const WorldSpec *world = data;
    float p = x * world->frequency, q = z * world->frequency;
    float phase = world->phase;
    float ridges = sinf(p + 0.8f * sinf(q * 0.61f + phase));
    float folds = cosf(q * 0.83f - p * 0.39f + phase);
    float detail = sinf(p * 2.1f + q * 1.6f + phase);
    float height = world->relief * (0.49f * ridges + 0.37f * folds + 0.07f * detail);
    float sculpt = 0;
    for (int i = 0; i < world->brushCount; ++i)
    {
        const WorldBrush *brush = &world->brushes[i];
        float dx = x - brush->center.x, dz = z - brush->center.y;
        float weight = fmaxf(0, 1 - (dx * dx + dz * dz) / (brush->radius * brush->radius));
        sculpt += brush->height * weight * weight;
    }
    return height + Clamp(sculpt, -WORLD_BRUSH_LIMIT, WORLD_BRUSH_LIMIT);
}
