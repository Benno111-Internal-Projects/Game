#ifndef DORF_CAMPAIGN_H
#define DORF_CAMPAIGN_H

#include "camera.h"
#include "input.h"
#include "player.h"
#include "player_touch.h"

enum { CAMPAIGN_LEVEL_COUNT = 7, CAMPAIGN_BOSS_COUNT = 9, CAMPAIGN_MAX_ORBS = 48 };

typedef struct CampaignOrb {
    Vector3 position, velocity;
    float life;
} CampaignOrb;

typedef struct Campaign {
    PlayerController player;
    WalkCamera view;
    Camera3D camera;
    PlayerTouch touch;
    CampaignOrb orbs[CAMPAIGN_MAX_ORBS];
    int level, bossIndex, bossHealth, health;
    float time, attackTimer, guardTimer, hitTimer;
    unsigned int attackCount;
    Vector3 bossPosition;
    bool checkpoint, debugRun, cleared;
} Campaign;

void StartCampaign(Campaign *campaign, int level, bool debugRun);
void StepCampaign(Campaign *campaign, Vector2 movement, Vector2 look, bool jump,
                  bool sprint, bool retry, float dt);
void DrawCampaignWorld(const Campaign *campaign);
void DrawCampaignHud(const Campaign *campaign, InputPointer pointer);
bool ReadCampaignMenu(InputPointer pointer);
int ReadDebugLevelSelect(InputPointer pointer);
void DrawDebugLevelSelect(Vector2 pointer);

#endif
