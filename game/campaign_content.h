#ifndef DORF_CAMPAIGN_CONTENT_H
#define DORF_CAMPAIGN_CONTENT_H

#include "campaign.h"

enum {
    CAMPAIGN_MAX_PLATFORMS = 9,
    CAMPAIGN_MAX_COLLIDERS = CAMPAIGN_MAX_PLATFORMS + 8
};

#define CAMPAIGN_MAZE_CELL 2.5f
#define CAMPAIGN_MAZE_HEIGHT 3.6f

typedef enum BossAttack { BOSS_PULSE, BOSS_AIMED, BOSS_SPIRAL, BOSS_FAN, BOSS_CROSS, BOSS_SWEEP } BossAttack;

typedef struct BossPhase {
    float range, speed, cooldown;
    BossAttack attacks[2];
} BossPhase;

typedef struct BossSpec {
    const char *name;
    Color color;
    int health;
    float radius, height;
    int phaseCount;
    BossPhase phases[3];
} BossSpec;

typedef struct LevelSpec {
    char name[64];
    Color stone, trim;
    Vector2 start;
    BoundingBox platforms[CAMPAIGN_MAX_PLATFORMS];
    int platformCount;
    int firstBoss, bossCount;
    int mazeRows, mazeCols, mazeExit;
    char maze[CAMPAIGN_MAZE_SIDE * CAMPAIGN_MAZE_SIDE];
    BoundingBox colliders[CAMPAIGN_MAX_COLLIDERS];
    int colliderCount, wallEnd;
} LevelSpec;

extern const BossSpec campaignBosses[CAMPAIGN_BOSS_COUNT];
extern LevelSpec campaignLevels[CAMPAIGN_LEVEL_COUNT];
extern LevelSpec campaignExtraMazes[CAMPAIGN_LEVEL_COUNT];
extern const BossSpec campaignMinibosses[CAMPAIGN_LEVEL_COUNT];

// Read and validate the complete numbered pack before replacing the active levels.
bool LoadCampaignLevels(const char *directory);
void GenerateMazeLevel(LevelSpec *level, unsigned long long stage);
Vector2 CampaignMazeCellCenter(const LevelSpec *level, int row, int col);

#endif
