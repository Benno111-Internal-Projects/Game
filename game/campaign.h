#ifndef DORF_CAMPAIGN_H
#define DORF_CAMPAIGN_H

#include "camera.h"
#include "input.h"
#include "player.h"
#include "player_touch.h"

enum { CAMPAIGN_LEVEL_COUNT = 7, CAMPAIGN_BOSS_COUNT = 9, CAMPAIGN_MAX_ORBS = 48, CAMPAIGN_MAZE_SIDE = 31 };

typedef struct CampaignSurfaces {
    Texture2D walls, platforms, ceiling;
} CampaignSurfaces;

typedef enum CampaignPauseAction {
    CAMPAIGN_PAUSE_NONE, CAMPAIGN_PAUSE_RESUME, CAMPAIGN_PAUSE_RETRY, CAMPAIGN_PAUSE_LEAVE
} CampaignPauseAction;

typedef struct CampaignOrb {
    Vector3 position, velocity;
    float life;
} CampaignOrb;

struct LevelSpec;

enum { MAZE_RUN_LIVES = 3 };

typedef struct Campaign {
    struct LevelSpec *endlessLevel;
    unsigned long long stage;
    int lives;
    bool gameOver;
    PlayerController player;
    WalkCamera view;
    Camera3D camera;
    PlayerTouch touch;
    CampaignOrb orbs[CAMPAIGN_MAX_ORBS];
    int level, bossIndex, bossHealth, health, bossPhase;
    float time, attackTimer, guardTimer, hitTimer, phaseNotice, bossMotion;
    unsigned int attackCount;
    Vector3 bossPosition;
    bool checkpoint, debugRun, cleared, paused;
    bool mazeVisited[CAMPAIGN_MAZE_SIDE * CAMPAIGN_MAZE_SIDE];
} Campaign;

void StartCampaign(Campaign *campaign, int level, bool debugRun);
void StartEndlessMazes(Campaign *campaign, struct LevelSpec *level);
void StepCampaign(Campaign *campaign, Vector2 movement, Vector2 look, bool jump,
                  bool sprint, bool retry, float dt);
bool LoadCampaignSurfaces(CampaignSurfaces *surfaces);
void UnloadCampaignSurfaces(CampaignSurfaces surfaces);
void DrawCampaignWorld(const Campaign *campaign, const CampaignSurfaces *surfaces);
CampaignPauseAction ReadCampaignPause(InputPointer pointer);
void DrawCampaignHud(const Campaign *campaign, InputPointer pointer);
bool ReadCampaignMenu(InputPointer pointer);
int ReadDebugLevelSelect(InputPointer pointer);
void DrawDebugLevelSelect(Vector2 pointer);

#endif
