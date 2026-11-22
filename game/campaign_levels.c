#include "campaign_content.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

LevelSpec campaignLevels[CAMPAIGN_LEVEL_COUNT];

enum { AUTHORED_MAZE_SIDE = 31 };

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

static const char *ReadField(char **cursor, const char *field)
{
    const char *line = NextLine(cursor);
    size_t length = strlen(field);
    if (!line || strncmp(line, field, length) || line[length] != ' ') return NULL;
    return line + length + 1;
}

static bool ReadNumbers(const char *text, float *values, size_t count)
{
    if (!text) return false;
    for (size_t i = 0; i < count; ++i)
    {
        char *end;
        float n = strtof(text, &end);
        if (end == text || !isfinite(n) || (*end && !isspace((unsigned char)*end))) return false;
        values[i] = n;
        text = end;
    }
    while (isspace((unsigned char)*text)) ++text;
    return *text == '\0';
}

static bool ByteValue(float value)
{
    return value >= 0 && value <= 255 && floorf(value) == value;
}

static bool ReadColor(char **cursor, const char *field, Color *color)
{
    float v[3];
    if (!ReadNumbers(ReadField(cursor, field), v, 3) ||
        !ByteValue(v[0]) || !ByteValue(v[1]) || !ByteValue(v[2])) return false;
    *color = (Color){ (unsigned char)v[0], (unsigned char)v[1], (unsigned char)v[2], 255 };
    return true;
}

Vector2 CampaignMazeCellCenter(const LevelSpec *level, int row, int col)
{
    return (Vector2){ (col + 0.5f - level->mazeCols * 0.5f) * CAMPAIGN_MAZE_CELL,
                      (row + 0.5f - level->mazeRows) * CAMPAIGN_MAZE_CELL };
}

static bool MazeIsConnected(const LevelSpec *level, int start)
{
    bool visited[AUTHORED_MAZE_SIDE * AUTHORED_MAZE_SIDE] = { 0 };
    int queue[AUTHORED_MAZE_SIDE * AUTHORED_MAZE_SIDE] = { start };
    int tail = 1;
    visited[start] = true;
    const int dx[] = { 1, -1, 0, 0 }, dz[] = { 0, 0, 1, -1 };
    for (int head = 0; head < tail; ++head)
    {
        int col = queue[head] % level->mazeCols, row = queue[head] / level->mazeCols;
        for (int i = 0; i < 4; ++i)
        {
            int x = col + dx[i], z = row + dz[i];
            if (x < 0 || x >= level->mazeCols || z < 0 || z >= level->mazeRows) continue;
            int cell = z * level->mazeCols + x;
            if (level->maze[cell] == '#' || visited[cell]) continue;
            visited[cell] = true;
            queue[tail++] = cell;
        }
    }
    for (int i = 0; i < level->mazeRows * level->mazeCols; ++i)
        if (level->maze[i] != '#' && !visited[i]) return false;
    return true;
}

static void AddArenaWalls(LevelSpec *level)
{
    BoundingBox arena = level->platforms[level->platformCount - 1];
    float left = arena.min.x, right = arena.max.x;
    float north = arena.min.z, south = arena.max.z;
    float floor = arena.max.y, ceiling = floor + 6.0f;
    BoundingBox walls[] = {
        { { left, floor, north }, { right, ceiling, north + 0.4f } },
        { { left, floor, north }, { left + 0.4f, ceiling, south } },
        { { right - 0.4f, floor, north }, { right, ceiling, south } },
    };
    for (size_t i = 0; i < sizeof(walls) / sizeof(walls[0]); ++i)
        level->colliders[level->colliderCount++] = walls[i];

    if (level->mazeRows)
    {
        float center = (left + right) * 0.5f, halfDoor = CAMPAIGN_MAZE_CELL * 0.5f;
        level->colliders[level->colliderCount++] = (BoundingBox){
            { left, floor, south - 0.4f }, { center - halfDoor, ceiling, south }
        };
        level->colliders[level->colliderCount++] = (BoundingBox){
            { center + halfDoor, floor, south - 0.4f }, { right, ceiling, south }
        };
        level->colliders[level->colliderCount++] = (BoundingBox){
            { center - halfDoor, floor + CAMPAIGN_MAZE_HEIGHT, south - 0.4f },
            { center + halfDoor, ceiling, south }
        };
    }
    level->wallEnd = level->colliderCount;
    level->colliders[level->colliderCount++] = (BoundingBox){
        { left, ceiling, north }, { right, ceiling + 0.35f, south }
    };
}

static void BuildMaze(LevelSpec *level, int spawn)
{
    float halfWidth = level->mazeCols * CAMPAIGN_MAZE_CELL * 0.5f;
    float north = -level->mazeRows * CAMPAIGN_MAZE_CELL;
    float exitX = CampaignMazeCellCenter(level, 0, level->mazeExit).x;
    level->start = CampaignMazeCellCenter(level, spawn / level->mazeCols, spawn % level->mazeCols);
    level->platformCount = level->colliderCount = 2;
    level->platforms[0] = (BoundingBox){ { -halfWidth, -0.8f, north }, { halfWidth, 0, 0 } };
    level->platforms[1] = (BoundingBox){ { exitX - 7, -0.8f, north - 15 }, { exitX + 7, 0, north } };
    memcpy(level->colliders, level->platforms, 2 * sizeof(level->platforms[0]));

    BoundingBox roof = { { -halfWidth, CAMPAIGN_MAZE_HEIGHT, north },
                         { halfWidth, CAMPAIGN_MAZE_HEIGHT + 0.35f, 0 } };

    AddArenaWalls(level);
    level->colliders[level->colliderCount++] = roof;
}

static bool ReadMaze(char *cursor, LevelSpec *level)
{
    float size[2];
    if (!ReadNumbers(ReadField(&cursor, "maze"), size, 2) ||
        size[0] < 7 || size[0] > AUTHORED_MAZE_SIDE || floorf(size[0]) != size[0] ||
        size[1] < 7 || size[1] > AUTHORED_MAZE_SIDE || floorf(size[1]) != size[1]) return false;
    level->mazeRows = (int)size[0];
    level->mazeCols = (int)size[1];
    level->mazeExit = -1;
    int spawn = -1;
    for (int row = 0; row < level->mazeRows; ++row)
    {
        const char *line = NextLine(&cursor);
        if (!line || strlen(line) != (size_t)level->mazeCols) return false;
        for (int col = 0; col < level->mazeCols; ++col)
        {
            char tile = line[col];
            bool border = row == 0 || col == 0 || row == level->mazeRows - 1 || col == level->mazeCols - 1;
            int cell = row * level->mazeCols + col;
            if (tile == 'S' && !border && spawn < 0) spawn = cell;
            else if (tile == 'E' && row == 0 && col > 0 && col < level->mazeCols - 1 && level->mazeExit < 0)
                level->mazeExit = col;
            else if (tile != '#' && (tile != '.' || border)) return false;
            level->maze[cell] = tile;
        }
    }
    if (*cursor || spawn < 0 || level->mazeExit < 0 || !MazeIsConnected(level, spawn)) return false;

    BuildMaze(level, spawn);
    return true;
}

void GenerateMazeLevel(LevelSpec *level, unsigned long long stage)
{
    int layout = (int)((stage - 1) % CAMPAIGN_LEVEL_COUNT);
    int rows = GetRandomValue(17, CAMPAIGN_MAZE_SIDE);
    int cols = GetRandomValue(17, CAMPAIGN_MAZE_SIDE);
    const LevelSpec *palette = &campaignLevels[layout];
    *level = (LevelSpec){
        .stone = palette->stone, .trim = palette->trim,
        .mazeRows = rows, .mazeCols = cols,
        .firstBoss = (int)((stage - 1) % CAMPAIGN_BOSS_COUNT), .bossCount = 1,
    };
    snprintf(level->name, sizeof(level->name), "Infinite maze %llu", stage);
    memset(level->maze, '#', sizeof(level->maze));
    int exit = 1 + 2 * GetRandomValue(0, (cols - 3) / 2);
    level->mazeExit = exit;
    level->maze[exit] = 'E';
    // Depth-first carving visits only odd-numbered interior rows and columns.
    enum { MAX_ROOMS = ((CAMPAIGN_MAZE_SIDE - 1) / 2) * ((CAMPAIGN_MAZE_SIDE - 1) / 2) };
    uint16_t path[MAX_ROOMS] = { (uint16_t)(cols + exit) };
    int depth = 1, deepest = 0, spawn = path[0];
    level->maze[spawn] = '.';
    const int dx[] = { 2, -2, 0, 0 }, dz[] = { 0, 0, 2, -2 };
    while (depth > 0)
    {
        int cell = path[depth - 1], candidates[4], count = 0;
        for (int i = 0; i < 4; ++i)
        {
            int x = cell % cols + dx[i], z = cell / cols + dz[i];
            if (x > 0 && x < cols - 1 && z > 0 && z < rows - 1 &&
                level->maze[z * cols + x] == '#') candidates[count++] = z * cols + x;
        }
        if (count == 0)
        {
            --depth;
            continue;
        }
        int next = candidates[GetRandomValue(0, count - 1)];
        level->maze[(cell + next) / 2] = level->maze[next] = '.';
        path[depth++] = (uint16_t)next;
        if (depth > deepest)
        {
            deepest = depth;
            spawn = next;
        }
    }
    level->maze[spawn] = 'S';
    BuildMaze(level, spawn);
}

static bool ReadLevel(char *cursor, LevelSpec *level)
{
    const char *header = NextLine(&cursor);
    if (!header) return false;
    bool maze = strcmp(header, "DORF-LEVEL 2") == 0;
    if (!maze && strcmp(header, "DORF-LEVEL 1") != 0) return false;
    const char *name = ReadField(&cursor, "name");
    if (!name || !*name || strlen(name) >= sizeof(level->name)) return false;
    memcpy(level->name, name, strlen(name) + 1);

    float start[2], bosses[2];
    if (!ReadColor(&cursor, "stone", &level->stone) || !ReadColor(&cursor, "trim", &level->trim) ||
        (!maze && !ReadNumbers(ReadField(&cursor, "start"), start, 2)) ||
        !ReadNumbers(ReadField(&cursor, "bosses"), bosses, 2) ||
        bosses[0] < 1 || bosses[0] > CAMPAIGN_BOSS_COUNT || floorf(bosses[0]) != bosses[0] ||
        bosses[1] < 1 || bosses[1] > 2 || floorf(bosses[1]) != bosses[1] ||
        bosses[0] + bosses[1] - 1 > CAMPAIGN_BOSS_COUNT) return false;

    level->firstBoss = (int)bosses[0] - 1;
    level->bossCount = (int)bosses[1];
    if (maze) return ReadMaze(cursor, level);
    level->start = (Vector2){ start[0], start[1] };

    while (*cursor)
    {
        float v[5];
        if (level->platformCount == CAMPAIGN_MAX_PLATFORMS ||
            !ReadNumbers(ReadField(&cursor, "platform"), v, 5) ||
            fabsf(v[0]) > 200 || fabsf(v[1]) > 200 ||
            v[2] < 1 || v[2] > 48 || v[3] < 1 || v[3] > 48 || v[4] < -8 || v[4] > 40)
            return false;
        level->platforms[level->platformCount++] = (BoundingBox){
            .min = { v[0] - v[2] / 2, v[4] - 0.8f, v[1] - v[3] / 2 },
            .max = { v[0] + v[2] / 2, v[4], v[1] + v[3] / 2 },
        };
    }

    if (level->platformCount < 2) return false;
    level->colliderCount = level->platformCount;
    memcpy(level->colliders, level->platforms, (size_t)level->platformCount * sizeof(level->platforms[0]));
    BoundingBox first = level->platforms[0];
    BoundingBox arena = level->platforms[level->platformCount - 1];
    bool valid = start[0] >= first.min.x + 0.3f && start[0] <= first.max.x - 0.3f &&
           start[1] >= first.min.z + 0.3f && start[1] <= first.max.z - 0.3f &&
           arena.max.x - arena.min.x >= 8 && arena.max.z - arena.min.z >= 8;
    if (valid) AddArenaWalls(level);
    return valid;
}

static bool ReadLevelPack(const char *directory, LevelSpec *levels)
{
    unsigned int bossMask = 0;
    for (int i = 0; i < CAMPAIGN_LEVEL_COUNT; ++i)
    {
        char path[1024];
        int length = snprintf(path, sizeof(path), "%s/%02d.level", directory, i + 1);
        if (length < 0 || (size_t)length >= sizeof(path))
        {
            TraceLog(LOG_ERROR, "Campaign level pack path is too long");
            return false;
        }
        char *text = LoadFileText(path);
        bool valid = text && ReadLevel(text, &levels[i]);
        if (text) UnloadFileText(text);
        if (!valid)
        {
            TraceLog(LOG_ERROR, "Invalid campaign level file: %s", path);
            return false;
        }
        for (int boss = levels[i].firstBoss; boss < levels[i].firstBoss + levels[i].bossCount; ++boss)
        {
            unsigned int bit = 1U << boss;
            if (bossMask & bit)
            {
                TraceLog(LOG_ERROR, "Duplicate campaign boss %d in %s", boss + 1, path);
                return false;
            }
            bossMask |= bit;
        }
    }
    if (bossMask != (1U << CAMPAIGN_BOSS_COUNT) - 1)
    {
        TraceLog(LOG_ERROR, "Campaign level pack must use all %d bosses", CAMPAIGN_BOSS_COUNT);
        return false;
    }
    return true;
}

bool LoadCampaignLevels(const char *directory)
{
    LevelSpec *levels = calloc(CAMPAIGN_LEVEL_COUNT, sizeof(*levels));
    if (!levels) return false;
    bool valid = ReadLevelPack(directory, levels);
    if (valid) memcpy(campaignLevels, levels, sizeof(campaignLevels));
    free(levels);
    return valid;
}
