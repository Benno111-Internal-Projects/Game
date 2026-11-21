#include "campaign_content.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LevelSpec campaignLevels[CAMPAIGN_LEVEL_COUNT];

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

static bool ReadLevel(char *cursor, LevelSpec *level)
{
    const char *header = NextLine(&cursor);
    if (!header || strcmp(header, "DORF-LEVEL 1") != 0) return false;
    const char *name = ReadField(&cursor, "name");
    if (!name || !*name || strlen(name) >= sizeof(level->name)) return false;
    memcpy(level->name, name, strlen(name) + 1);

    float start[2], bosses[2];
    if (!ReadColor(&cursor, "stone", &level->stone) || !ReadColor(&cursor, "trim", &level->trim) ||
        !ReadNumbers(ReadField(&cursor, "start"), start, 2) ||
        !ReadNumbers(ReadField(&cursor, "bosses"), bosses, 2) ||
        bosses[0] < 1 || bosses[0] > CAMPAIGN_BOSS_COUNT || floorf(bosses[0]) != bosses[0] ||
        bosses[1] < 1 || bosses[1] > 2 || floorf(bosses[1]) != bosses[1] ||
        bosses[0] + bosses[1] - 1 > CAMPAIGN_BOSS_COUNT) return false;

    level->start = (Vector2){ start[0], start[1] };
    level->firstBoss = (int)bosses[0] - 1;
    level->bossCount = (int)bosses[1];

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
    BoundingBox first = level->platforms[0];
    BoundingBox arena = level->platforms[level->platformCount - 1];
    return start[0] >= first.min.x + 0.3f && start[0] <= first.max.x - 0.3f &&
           start[1] >= first.min.z + 0.3f && start[1] <= first.max.z - 0.3f &&
           arena.max.x - arena.min.x >= 8 && arena.max.z - arena.min.z >= 8;
}

bool LoadCampaignLevels(const char *directory)
{
    LevelSpec levels[CAMPAIGN_LEVEL_COUNT] = { 0 };
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
    memcpy(campaignLevels, levels, sizeof(levels));
    return true;
}
