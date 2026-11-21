#ifndef DORF_CAMPAIGN_CONTENT_H
#define DORF_CAMPAIGN_CONTENT_H

#include "campaign.h"

enum { CAMPAIGN_MAX_PLATFORMS = 9 };

typedef enum BossAttack { BOSS_PULSE, BOSS_AIMED, BOSS_SPIRAL, BOSS_FAN } BossAttack;

typedef struct BossSpec {
    const char *name;
    Color color;
    int health;
    float radius, height, range, speed, cooldown;
    BossAttack attacks[2];
} BossSpec;

typedef struct LevelSpec {
    char name[64];
    Color stone, trim;
    Vector2 start;
    BoundingBox platforms[CAMPAIGN_MAX_PLATFORMS];
    int platformCount;
    int firstBoss, bossCount;
} LevelSpec;

extern const BossSpec campaignBosses[CAMPAIGN_BOSS_COUNT];
extern LevelSpec campaignLevels[CAMPAIGN_LEVEL_COUNT];

// Read and validate the complete numbered pack before replacing the active levels.
bool LoadCampaignLevels(const char *directory);

#endif
