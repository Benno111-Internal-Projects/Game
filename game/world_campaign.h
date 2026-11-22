#ifndef DORF_WORLD_CAMPAIGN_H
#define DORF_WORLD_CAMPAIGN_H

#include "camera.h"
#include "input.h"
#include "player.h"
#include "player_touch.h"

typedef struct WorldChunk WorldChunk;

typedef struct WorldCampaign {
    WorldChunk *chunks;
    int world, waypoint;
    PlayerController player;
    PlayerTouch touch;
    WalkCamera view;
    Camera3D camera;
    bool paused, complete;
} WorldCampaign;

bool OpenWorldCampaign(WorldCampaign *campaign);
void CloseWorldCampaign(WorldCampaign *campaign);
// Returns false when the player chooses to return to the title screen.
bool UpdateWorldCampaign(WorldCampaign *campaign, InputPointer pointer, Vector2 movement,
                         bool jump, bool sprint, bool retry, bool escape, bool enter, float dt);
void DrawWorldLandscape(const WorldCampaign *campaign);
void DrawWorldCampaignHud(const WorldCampaign *campaign, InputPointer pointer);

#endif
