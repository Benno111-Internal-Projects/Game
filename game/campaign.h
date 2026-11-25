#ifndef DORF_CAMPAIGN_H
#define DORF_CAMPAIGN_H

#include "camera.h"
#include "debug_tools.h"
#include "input.h"
#include "player.h"
#include "player_touch.h"

enum { CAMPAIGN_LEVEL_COUNT = 7, CAMPAIGN_BOSS_COUNT = 9, CAMPAIGN_MAX_ORBS = 48, CAMPAIGN_MAZE_SIDE = 200 };

enum {
    AUTHORED_MAZE_SIDE = 31,
    CAMPAIGN_MAX_PLATFORMS = 9,
    CAMPAIGN_MAX_COLLIDERS = CAMPAIGN_MAX_PLATFORMS + 8
};

#define CAMPAIGN_MAZE_CELL 2.5f
#define CAMPAIGN_MAZE_HEIGHT 3.6f
#define CAMPAIGN_ARENA_ENTRY 2.5f

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

enum { MAZE_FLOORS = 3 };

typedef struct CampaignSurfaces {
    Texture2D walls, platforms, ceiling;
    Model torch, torchFlame;
    Model cube, sphere, boss, bossWarning, pedestal, plane;
    Vector3 stoneFaces[6][6];
    Shader mazeLighting;
    Texture2D mazeLightmap;
    const LevelSpec *litLevel;
    unsigned long long litStage;
    int litFloor;
    int lightmapLocation, mazeSizeLocation, flickerLocation, arenaEntryLocation;
} CampaignSurfaces;

typedef enum CampaignPauseAction {
    CAMPAIGN_PAUSE_NONE, CAMPAIGN_PAUSE_RESUME, CAMPAIGN_PAUSE_RETRY, CAMPAIGN_PAUSE_LEAVE
} CampaignPauseAction;

typedef enum CampaignDeath {
    CAMPAIGN_DEATH_NONE, CAMPAIGN_DEATH_TORCH, CAMPAIGN_DEATH_BOSS, CAMPAIGN_DEATH_FALL
} CampaignDeath;

typedef struct CampaignOrb {
    Vector3 position, velocity;
    float life;
} CampaignOrb;

enum { MAZE_RUN_LIVES = 3 };

typedef struct Campaign {
    LevelSpec *endlessLevel;
    const LevelSpec *remoteMaze;
    LevelSpec upperMaze;
    int mazeFloor;
    unsigned int mazeSeed;
    float elevatorTime;
    unsigned long long stage;
    int lives;
    bool gameOver;
    CampaignDeath death;
    PlayerController player;
    DebugOptions debugOptions;
    WalkCamera view;
    Camera3D camera;
    PlayerTouch touch;
    CampaignOrb orbs[CAMPAIGN_MAX_ORBS];
    int level, bossIndex, bossHealth, health, bossPhase;
    float time, attackTimer, guardTimer, hitTimer, phaseNotice, bossMotion;
    unsigned int attackCount;
    Vector3 bossPosition;
    bool checkpoint, debugRun, cleared, paused, secondMaze, editorPlaytest;
    bool mazeVisited[CAMPAIGN_MAZE_SIDE * CAMPAIGN_MAZE_SIDE];
} Campaign;

typedef struct MazePartner {
    PlayerController player;
    WalkCamera view;
    Camera3D camera;
    float hitTimer;
    int health;
    CampaignDeath death;
} MazePartner;

struct MazeLanFrame;
struct MazeLanLayout;
struct MazeLanInput;
const LevelSpec *ActiveCampaignMaze(const Campaign *campaign);
void ResetMazePartner(const Campaign *campaign, MazePartner *partner);
void StepMazePartner(Campaign *campaign, MazePartner *partner, const struct MazeLanInput *input, float dt);
void ExportMazeLanFrame(const Campaign *campaign, const MazePartner *partner, struct MazeLanFrame *frame);
bool ApplyMazeLanFrame(Campaign *campaign, LevelSpec *maze, const struct MazeLanFrame *frame,
                       const struct MazeLanLayout *layout);

void StartCampaign(Campaign *campaign, int level, bool debugRun);
void StartEndlessMazes(Campaign *campaign, LevelSpec *level);
void TeleportCampaignRandomly(Campaign *campaign);
void StepCampaign(Campaign *campaign, Vector2 movement, Vector2 look, bool jump,
                  bool sprint, bool retry, float dt);
bool LoadCampaignSurfaces(CampaignSurfaces *surfaces, const char *assetDir, const char *shaderDir);
void ContinueCampaign(Campaign *campaign);
bool ReadCampaignContinue(InputPointer pointer);
void UnloadCampaignSurfaces(CampaignSurfaces surfaces);
void PrepareCampaignLighting(const Campaign *campaign, CampaignSurfaces *surfaces);
void DrawCampaignWorld(const Campaign *campaign, const CampaignSurfaces *surfaces);
CampaignPauseAction ReadCampaignPause(InputPointer pointer);
void DrawCampaignHud(const Campaign *campaign, InputPointer pointer);
bool ReadCampaignMenu(InputPointer pointer);
int ReadMazeLevelSelect(InputPointer pointer, bool editing);
void DrawMazeLevelSelect(Vector2 pointer, bool editing);

#endif
