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
    bool paused, complete, singleWorld;
} WorldCampaign;

// Pass -1 to play the full campaign, or a world index to play one level.
bool OpenWorldCampaign(WorldCampaign *campaign, int level);
void CloseWorldCampaign(WorldCampaign *campaign);
// Returns false when the player chooses to return to the title screen.
bool UpdateWorldCampaign(WorldCampaign *campaign, InputPointer pointer, Vector2 movement,
                         bool jump, bool sprint, bool retry, bool escape, bool enter, float dt);
void DrawWorldLandscape(const WorldCampaign *campaign);
void DrawWorldCampaignHud(const WorldCampaign *campaign, InputPointer pointer);

#endif
