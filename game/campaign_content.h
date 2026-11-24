#ifndef DORF_CAMPAIGN_CONTENT_H
#define DORF_CAMPAIGN_CONTENT_H

#include "campaign.h"

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

extern const BossSpec campaignBosses[CAMPAIGN_BOSS_COUNT];
extern LevelSpec campaignLevels[CAMPAIGN_LEVEL_COUNT];
extern LevelSpec campaignExtraMazes[CAMPAIGN_LEVEL_COUNT];
extern const BossSpec campaignMinibosses[CAMPAIGN_LEVEL_COUNT];

// Read and validate the complete numbered pack before replacing the active levels.
bool LoadCampaignLevels(const char *directory);
void LoadCampaignMazeOverrides(void);
typedef enum MazeSaveResult { MAZE_SAVED, MAZE_INVALID, MAZE_SAVE_ERROR } MazeSaveResult;
MazeSaveResult SaveCampaignMazeEdit(int index, const char *first, const char *second);
void GenerateUpperMaze(LevelSpec *floor, const LevelSpec *base, unsigned int seed);
void GenerateMazeLevel(LevelSpec *level, unsigned long long stage);
Vector2 CampaignMazeCellCenter(const LevelSpec *level, int row, int col);

#endif
