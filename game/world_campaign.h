#ifndef DORF_WORLD_CAMPAIGN_H
#define DORF_WORLD_CAMPAIGN_H

#include "camera.h"
#include "debug_tools.h"
#include "input.h"
#include "player.h"
#include "player_touch.h"
#include "terrain.h"

typedef struct WorldChunk WorldChunk;
typedef struct WorldModels WorldModels;

typedef struct WorldCampaign {
    WorldChunk *chunks;
    WorldModels *models;
    int world, waypoint;
    PlayerController player;
    DebugOptions debugOptions;
    PlayerTouch touch;
    WalkCamera view;
    Camera3D camera;
    bool paused, complete, singleWorld;
} WorldCampaign;

// Pass -1 to play the full campaign, or a world index to play one level.
bool OpenWorldCampaign(WorldCampaign *campaign, int level);
void TeleportWorldRandomly(WorldCampaign *campaign);
int WorldCampaignVisibleChunks(const WorldCampaign *campaign);
void CloseWorldCampaign(WorldCampaign *campaign);
// Returns false when the player chooses to return to the title screen.
bool UpdateWorldCampaign(WorldCampaign *campaign, InputPointer pointer, Vector2 movement,
                         bool jump, bool sprint, bool retry, bool escape, bool enter, float dt);
void DrawWorldLandscape(const WorldCampaign *campaign, const TerrainLayers *layers);
void DrawWorldCampaignHud(const WorldCampaign *campaign, InputPointer pointer);

#endif
