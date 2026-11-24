#include "campaign.h"
#include "campaign_content.h"
#include "raymath.h"
#include "rlgl.h"
#include "ui.h"

#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static bool HasExtraMaze(const Campaign *campaign)
{
    return !campaign->endlessLevel && campaignExtraMazes[campaign->level].mazeRows > 0;
}

static bool IsMinibossMaze(const Campaign *campaign)
{
    return HasExtraMaze(campaign) && !campaign->secondMaze;
}

static const LevelSpec *BaseMaze(const Campaign *campaign)
{
    if (campaign->endlessLevel) return campaign->endlessLevel;
    return campaign->secondMaze ? &campaignExtraMazes[campaign->level] : &campaignLevels[campaign->level];
}

static const LevelSpec *CurrentLevel(const Campaign *campaign)
{
    return campaign->mazeFloor > 0 ? &campaign->upperMaze : BaseMaze(campaign);
}

static bool HasElevator(const Campaign *campaign)
{
    return CurrentLevel(campaign)->mazeRows > 0 && campaign->mazeFloor + 1 < MAZE_FLOORS;
}

static BoundingBox Arena(const Campaign *campaign)
{
    const LevelSpec *level = CurrentLevel(campaign);
    return level->platforms[level->platformCount - 1];
}

static const BossSpec *CurrentBoss(const Campaign *campaign)
{
    if (IsMinibossMaze(campaign))
        return campaign->bossIndex == 0 ? &campaignMinibosses[campaign->level] : NULL;
    const LevelSpec *level = BaseMaze(campaign);
    return campaign->bossIndex < level->bossCount
        ? &campaignBosses[level->firstBoss + campaign->bossIndex] : NULL;
}

static const BossPhase *CurrentPhase(const Campaign *campaign, const BossSpec *boss)
{
    return &boss->phases[campaign->bossPhase];
}

typedef struct MazeRegion {
    int firstRow, lastRow, firstCol, lastCol;
} MazeRegion;

static Vector2 MazeGridPosition(const LevelSpec *level, Vector3 position)
{
    return (Vector2){ position.x / CAMPAIGN_MAZE_CELL + level->mazeCols * 0.5f,
                      position.z / CAMPAIGN_MAZE_CELL + level->mazeRows };
}

static MazeRegion NearbyMaze(const LevelSpec *level, Vector3 position, int radius)
{
    Vector2 grid = MazeGridPosition(level, position);
    float col = floorf(grid.x), row = floorf(grid.y);
    return (MazeRegion){
        (int)Clamp(row - radius, 0, (float)level->mazeRows),
        (int)Clamp(row + radius + 1, 0, (float)level->mazeRows),
        (int)Clamp(col - radius, 0, (float)level->mazeCols),
        (int)Clamp(col + radius + 1, 0, (float)level->mazeCols),
    };
}

static BoundingBox MazeWall(const LevelSpec *level, int row, int firstCol, int lastCol)
{
    const BoundingBox floor = level->platforms[0];
    float z = floor.min.z + row * CAMPAIGN_MAZE_CELL;
    return (BoundingBox){
        { floor.min.x + firstCol * CAMPAIGN_MAZE_CELL, 0, z },
        { floor.min.x + lastCol * CAMPAIGN_MAZE_CELL, CAMPAIGN_MAZE_HEIGHT, z + CAMPAIGN_MAZE_CELL },
    };
}

static bool WallTorch(const LevelSpec *level, int row, int col, Vector3 *flame, Vector3 *mount)
{
    if (row <= 1 || col <= 0 || row >= level->mazeRows - 1 || col >= level->mazeCols - 1)
        return false;
    char tile = level->maze[row * level->mazeCols + col];
    unsigned int pattern = (unsigned int)row * 73856093u ^ (unsigned int)col * 19349663u;
    if (tile != 'S' && (tile != '.' || pattern % 5 != 0)) return false;

    const int dx[] = { 1, 0, -1, 0 }, dz[] = { 0, 1, 0, -1 };
    Vector2 center = CampaignMazeCellCenter(level, row, col);
    for (unsigned int offset = 0; offset < 4; ++offset)
    {
        unsigned int side = (pattern / 5 + offset) % 4;
        int neighbor = (row + dz[side]) * level->mazeCols + col + dx[side];
        if (level->maze[neighbor] != '#') continue;
        const float half = CAMPAIGN_MAZE_CELL * 0.5f;
        *flame = (Vector3){ center.x + dx[side] * (half - 0.23f), 1.38f,
                           center.y + dz[side] * (half - 0.23f) };
        if (mount)
            *mount = (Vector3){ center.x + dx[side] * (half - 0.02f), 0.97f,
                               center.y + dz[side] * (half - 0.02f) };
        return true;
    }
    return false;
}

static int MazeCellAt(const LevelSpec *level, Vector3 position)
{
    Vector2 grid = MazeGridPosition(level, position);
    if (grid.y < 0 || grid.y >= level->mazeRows || grid.x < 0 || grid.x >= level->mazeCols) return -1;
    return (int)grid.y * level->mazeCols + (int)grid.x;
}

static void TrackMazePosition(Campaign *campaign)
{
    int cell = MazeCellAt(CurrentLevel(campaign), campaign->player.feet);
    if (cell >= 0 && CurrentLevel(campaign)->maze[cell] != '#') campaign->mazeVisited[cell] = true;
}

static float VoidHeight(const void *data, float x, float z)
{
    (void)data;
    (void)x;
    (void)z;
    return -30.0f;
}

static bool OnPlatform(Vector3 feet, BoundingBox box)
{
    return feet.x >= box.min.x && feet.x <= box.max.x &&
           feet.z >= box.min.z && feet.z <= box.max.z &&
           fabsf(feet.y - box.max.y) <= 0.03f;
}

static Vector3 GatePosition(BoundingBox arena)
{
    return (Vector3){ (arena.min.x + arena.max.x) * 0.5f, arena.max.y, arena.min.z + 1.3f };
}

static void UpdateBossPosition(Campaign *campaign)
{
    const BossSpec *boss = CurrentBoss(campaign);
    if (!boss) return;
    BoundingBox arena = Arena(campaign);
    const BossPhase *phase = CurrentPhase(campaign, boss);
    float range = phase->range;
    if (campaign->bossPhase > 0 && campaign->phaseNotice > 0.7f)
        range = Lerp(boss->phases[campaign->bossPhase - 1].range, range,
                     Clamp((1.3f - campaign->phaseNotice) / 0.6f, 0.f, 1.f));
    campaign->bossPosition = (Vector3){
        (arena.min.x + arena.max.x) * 0.5f + sinf(campaign->bossMotion) * range,
        arena.max.y,
        (arena.min.z + arena.max.z) * 0.5f + (cosf(campaign->bossMotion) - 1.0f) * (range * 0.35f),
    };
}

static void ResetEncounter(Campaign *campaign)
{
    const BossSpec *boss = CurrentBoss(campaign);
    campaign->bossHealth = boss ? boss->health : 0;
    campaign->bossPhase = 0;
    campaign->bossMotion = 0;
    campaign->phaseNotice = 0;
    campaign->attackTimer = boss ? boss->phases[0].cooldown : 0;
    campaign->guardTimer = 0;
    campaign->attackCount = 0;
    memset(campaign->orbs, 0, sizeof(campaign->orbs));
    UpdateBossPosition(campaign);
}

enum { COLLISION_RADIUS = 2, COLLISION_WIDTH = 2 * COLLISION_RADIUS + 1 };

static PlayerWorld CampaignWorld(const LevelSpec *level, Vector3 position, BoundingBox *boxes, DebugOptions debug)
{
    int count = level->colliderCount;
    memcpy(boxes, level->colliders, (size_t)count * sizeof(*boxes));

    PlayerWorld world = { .groundHeight = VoidHeight, .boxes = boxes, .disableSliding = debug.noSliding };
    if (debug.noWalls)
    {
        int ceilings = count - level->wallEnd;
        memcpy(boxes + level->platformCount, boxes + level->wallEnd, (size_t)ceilings * sizeof(*boxes));
        world.boxCount = (size_t)(level->platformCount + ceilings);
        return world;
    }

    // The player can move less than one tile in a simulation step.
    MazeRegion region = NearbyMaze(level, position, COLLISION_RADIUS);
    for (int row = region.firstRow; row < region.lastRow; ++row)
        for (int col = region.firstCol; col < region.lastCol; ++col)
            if (level->maze[row * level->mazeCols + col] == '#')
                boxes[count++] = MazeWall(level, row, col, col + 1);

    world.boxCount = (size_t)count;
    return world;
}

static void Respawn(Campaign *campaign)
{
    campaign->elevatorTime = 0;
    const LevelSpec *level = CurrentLevel(campaign);
    BoundingBox arena = Arena(campaign);
    Vector2 spawn = campaign->checkpoint
        ? (Vector2){ (arena.min.x + arena.max.x) * 0.5f, arena.max.z - 0.9f }
        : level->start;
    BoundingBox boxes[CAMPAIGN_MAX_COLLIDERS + COLLISION_WIDTH * COLLISION_WIDTH];
    PlayerWorld world = CampaignWorld(level, (Vector3){ spawn.x, 0, spawn.y }, boxes, campaign->debugOptions);
    campaign->player = SpawnPlayer(spawn, &world);
    campaign->player.feet.y = campaign->checkpoint ? arena.max.y : level->platforms[0].max.y;
    Vector3 eye = GetPlayerEye(&campaign->player);
    campaign->view = (WalkCamera){ .position = eye, .yaw = PI, .pitch = -0.14f };
    ResetPlayerTouch(&campaign->touch);
    campaign->health = 3;
    campaign->hitTimer = 0.65f;
    ResetEncounter(campaign);
    TrackMazePosition(campaign);
    UpdateWalkCamera(&campaign->camera, &campaign->view, Vector2Zero(), Vector2Zero(), 0);
}

static void PlaceCampaignPlayer(Campaign *campaign, Vector2 destination, float height)
{
    BoundingBox boxes[CAMPAIGN_MAX_COLLIDERS + COLLISION_WIDTH * COLLISION_WIDTH];
    PlayerWorld world = CampaignWorld(CurrentLevel(campaign), (Vector3){ destination.x, height, destination.y },
                                      boxes, campaign->debugOptions);
    campaign->player = SpawnPlayer(destination, &world);
    campaign->player.feet.y = height;
    campaign->view.position = GetPlayerEye(&campaign->player);
    UpdateWalkCamera(&campaign->camera, &campaign->view, Vector2Zero(), Vector2Zero(), 0);
    ResetPlayerTouch(&campaign->touch);
    TrackMazePosition(campaign);
}

void TeleportCampaignRandomly(Campaign *campaign)
{
    if (campaign->elevatorTime > 0) return;
    const LevelSpec *level = CurrentLevel(campaign);
    Vector2 destination = level->start;
    float height = level->platforms[0].max.y;
    if (level->mazeRows)
    {
        int selected = -1, passages = 0;
        int here = MazeCellAt(level, campaign->player.feet);
        for (int i = 0; i < level->mazeRows * level->mazeCols; ++i)
        {
            if (i == here || (level->maze[i] != '.' && level->maze[i] != 'S')) continue;
            if (GetRandomValue(1, ++passages) == 1) selected = i;
        }
        if (selected >= 0)
            destination = CampaignMazeCellCenter(level, selected / level->mazeCols, selected % level->mazeCols);
    }
    else
    {
        BoundingBox platform = level->platforms[GetRandomValue(0, level->platformCount - 1)];
        destination = (Vector2){ (platform.min.x + platform.max.x) * 0.5f,
                                 (platform.min.z + platform.max.z) * 0.5f };
        height = platform.max.y;
    }
    PlaceCampaignPlayer(campaign, destination, height);
}

static void BeginStage(Campaign *campaign, int level, bool debugRun, int lives,
                       LevelSpec *endlessLevel, unsigned long long stage, DebugOptions debugOptions, bool secondMaze)
{
    *campaign = (Campaign){
        .level = level, .debugRun = debugRun, .lives = lives, .debugOptions = debugOptions,
        .endlessLevel = endlessLevel, .stage = stage, .secondMaze = secondMaze,
        .camera = { .up = { 0, 1, 0 }, .fovy = 48, .projection = CAMERA_PERSPECTIVE },
    };
    if (endlessLevel)
    {
        GenerateMazeLevel(endlessLevel, stage);
        campaign->mazeSeed = (unsigned int)GetRandomValue(0, 0x7fffff);
    }
    Respawn(campaign);
}

void StartCampaign(Campaign *campaign, int level, bool debugRun)
{
    if (level < 0 || level >= CAMPAIGN_LEVEL_COUNT) level = 0;
    BeginStage(campaign, level, debugRun, MAZE_RUN_LIVES, NULL, 0, (DebugOptions){0}, false);
}

void StartEndlessMazes(Campaign *campaign, LevelSpec *level)
{
    BeginStage(campaign, 0, false, MAZE_RUN_LIVES, level, 1, (DebugOptions){0}, false);
}

static void LoseLife(Campaign *campaign, CampaignDeath cause)
{
    campaign->death = cause;
    if (cause != CAMPAIGN_DEATH_NONE) campaign->health = 0;
    if (campaign->lives > INT_MIN) --campaign->lives;
    if (!campaign->debugRun && !campaign->debugOptions.enabled && campaign->lives <= 0)
    {
        campaign->gameOver = true;
        return;
    }
    if (cause == CAMPAIGN_DEATH_NONE) Respawn(campaign);
}

void ContinueCampaign(Campaign *campaign)
{
    if (!campaign->gameOver && campaign->death != CAMPAIGN_DEATH_NONE)
    {
        campaign->death = CAMPAIGN_DEATH_NONE;
        Respawn(campaign);
    }
}

static void SpawnOrb(Campaign *campaign, Vector3 start, float angle, float speed)
{
    for (int i = 0; i < CAMPAIGN_MAX_ORBS; ++i)
    {
        if (campaign->orbs[i].life > 0) continue;
        campaign->orbs[i] = (CampaignOrb){
            .position = start, .velocity = { cosf(angle) * speed, 0, sinf(angle) * speed }, .life = 3.5f,
        };
        return;
    }
}

static void BossAttackNow(Campaign *campaign, const BossSpec *boss)
{
    Vector3 start = campaign->bossPosition;
    start.y += 0.48f;
    Vector3 toward = Vector3Subtract(campaign->player.feet, start);
    float aim = atan2f(toward.z, toward.x);
    BossAttack attack = CurrentPhase(campaign, boss)->attacks[campaign->attackCount % 2];
    if (attack == BOSS_AIMED)
        SpawnOrb(campaign, start, aim, 5.1f);
    else if (attack == BOSS_FAN || attack == BOSS_SWEEP)
    {
        int spread = attack == BOSS_SWEEP ? 3 : 2;
        for (int i = -spread; i <= spread; ++i)
        {
            float height = start.y;
            if (attack == BOSS_SWEEP && i % 2 != 0) height += 0.62f;
            Vector3 origin = { start.x, height, start.z };
            SpawnOrb(campaign, origin, aim + i * 0.25f, attack == BOSS_SWEEP ? 5.1f : 4.5f);
        }
    }
    else if (attack == BOSS_CROSS)
    {
        for (int i = 0; i < 4; ++i)
        {
            float angle = aim + i * (PI * 0.5f);
            SpawnOrb(campaign, start, angle, 5.3f);
            SpawnOrb(campaign, start, angle + PI * 0.25f, 3.3f);
        }
    }
    else
    {
        int count = attack == BOSS_PULSE ? 10 : 12;
        float phase = attack == BOSS_PULSE ? 0.0f : campaign->attackCount * 0.55f;
        for (int i = 0; i < count; ++i)
            SpawnOrb(campaign, start, phase + i * (2 * PI / count), attack == BOSS_PULSE ? 3.6f : 4.3f);
    }
    ++campaign->attackCount;
}

static bool DamagePlayer(Campaign *campaign, Vector3 origin, CampaignDeath cause)
{
    if (campaign->debugOptions.noDamage || campaign->hitTimer > 0) return false;
    if (--campaign->health == 0)
    {
        LoseLife(campaign, cause);
        return true;
    }
    campaign->hitTimer = 0.95f;
    Vector3 away = Vector3Subtract(campaign->player.feet, origin);
    Vector2 push = Vector2Scale(Vector2Normalize((Vector2){ away.x, away.z }), 3.8f);
    campaign->player.velocity = (Vector3){ push.x, 4.1f, push.y };
    campaign->player.grounded = false;
    campaign->player.groundGrace = 0;
    return false;
}

static bool UpdateEncounter(Campaign *campaign, Vector3 oldFeet, float dt)
{
    const BossSpec *boss = CurrentBoss(campaign);
    if (!boss || !campaign->checkpoint) return false;
    const BossPhase *phase = CurrentPhase(campaign, boss);
    campaign->bossMotion = fmodf(campaign->bossMotion + dt * phase->speed, 2 * PI);
    campaign->phaseNotice = fmaxf(0, campaign->phaseNotice - dt);
    UpdateBossPosition(campaign);
    campaign->guardTimer = fmaxf(0, campaign->guardTimer - dt);
    campaign->attackTimer -= dt;
    if (campaign->attackTimer <= 0)
    {
        BossAttackNow(campaign, boss);
        campaign->attackTimer = phase->cooldown;
    }

    Vector3 feet = campaign->player.feet;
    Vector3 center = campaign->bossPosition;
    float dx = feet.x - center.x, dz = feet.z - center.z;
    float radius = boss->radius + campaign->player.radius;
    float top = center.y + boss->height;
    if (dx*dx + dz*dz < radius*radius)
    {
        if (oldFeet.y >= top && feet.y <= top && campaign->player.velocity.y <= 0)
        {
            campaign->player.feet.y = top;
            campaign->player.velocity.y = 6.4f;
            campaign->player.grounded = false;
            campaign->player.groundGrace = 0;
            if (campaign->guardTimer == 0)
            {
                if (--campaign->bossHealth == 0)
                {
                    ++campaign->bossIndex;
                    ResetEncounter(campaign);
                    campaign->hitTimer = 0.8f;
                    return false;
                }
                campaign->guardTimer = 0.35f;
                campaign->hitTimer = fmaxf(campaign->hitTimer, 0.25f);
                int nextPhase = (boss->health - campaign->bossHealth) * boss->phaseCount / boss->health;
                if (nextPhase > campaign->bossPhase)
                {
                    campaign->bossPhase = nextPhase;
                    campaign->phaseNotice = 1.3f;
                    campaign->attackCount = 0;
                    campaign->attackTimer = CurrentPhase(campaign, boss)->cooldown + 0.55f;
                    campaign->hitTimer = fmaxf(campaign->hitTimer, 0.55f);
                    memset(campaign->orbs, 0, sizeof(campaign->orbs));
                }
            }
        }
        else if (feet.y < top - 0.08f && feet.y + campaign->player.height > center.y)
        {
            if (DamagePlayer(campaign, center, CAMPAIGN_DEATH_BOSS)) return true;
        }
    }

    BoundingBox arena = Arena(campaign);
    for (int i = 0; i < CAMPAIGN_MAX_ORBS; ++i)
    {
        CampaignOrb *orb = &campaign->orbs[i];
        if (orb->life <= 0) continue;
        orb->life -= dt;
        orb->position = Vector3Add(orb->position, Vector3Scale(orb->velocity, dt));
        if (orb->position.x < arena.min.x || orb->position.x > arena.max.x ||
            orb->position.z < arena.min.z || orb->position.z > arena.max.z)
        {
            orb->life = 0;
            continue;
        }
        float x = orb->position.x - campaign->player.feet.x;
        float z = orb->position.z - campaign->player.feet.z;
        float radiusHit = campaign->player.radius + 0.18f;
        if (x*x + z*z <= radiusHit*radiusHit && orb->position.y + 0.18f >= campaign->player.feet.y &&
            orb->position.y - 0.18f <= campaign->player.feet.y + campaign->player.height)
        {
            orb->life = 0;
            if (DamagePlayer(campaign, orb->position, CAMPAIGN_DEATH_BOSS)) return true;
        }
    }
    return false;
}

static bool TouchTorches(Campaign *campaign, const LevelSpec *level)
{
    if (campaign->hitTimer > 0) return false;
    const PlayerController *player = &campaign->player;
    const Vector3 feet = player->feet;
    const float fireRadius = 0.26f;
    MazeRegion region = NearbyMaze(level, feet, 1);
    for (int row = region.firstRow; row < region.lastRow; ++row)
        for (int col = region.firstCol; col < region.lastCol; ++col)
        {
            Vector3 flame;
            if (!WallTorch(level, row, col, &flame, NULL)) continue;
            float dx = feet.x - flame.x, dz = feet.z - flame.z;
            float reach = player->radius + fireRadius;
            if (dx * dx + dz * dz <= reach * reach && feet.y <= flame.y + fireRadius &&
                feet.y + player->height >= flame.y - fireRadius)
                return DamagePlayer(campaign, flame, CAMPAIGN_DEATH_TORCH);
        }
    return false;
}

#define ELEVATOR_RIDE_SECONDS 1.6f
#define ELEVATOR_RISE 5.0f

static Vector2 ElevatorCenter(const LevelSpec *level)
{
    return CampaignMazeCellCenter(level, 0, level->mazeExit);
}

static void RideMazeElevator(Campaign *campaign, float dt)
{
    campaign->elevatorTime = fmaxf(0, campaign->elevatorTime - dt);
    if (campaign->elevatorTime > 0)
    {
        float progress = 1.0f - campaign->elevatorTime / ELEVATOR_RIDE_SECONDS;
        float height = ELEVATOR_RISE * progress * progress * (3 - 2 * progress);
        float rise = height + GetPlayerEye(&campaign->player).y - campaign->camera.position.y;
        campaign->camera.position.y += rise;
        campaign->camera.target.y += rise;
        return;
    }

    ++campaign->mazeFloor;
    const LevelSpec *base = BaseMaze(campaign);
    unsigned int seed = campaign->mazeSeed + (unsigned int)campaign->stage * 2654435761u +
        (unsigned int)(campaign->level * 2 + campaign->secondMaze) * 2246822519u +
        (unsigned int)campaign->mazeFloor * 3266489917u;
    GenerateUpperMaze(&campaign->upperMaze, base, seed);
    UpdateBossPosition(campaign);
    memset(campaign->mazeVisited, 0, sizeof(campaign->mazeVisited));
    campaign->hitTimer = fmaxf(campaign->hitTimer, 0.65f);
    PlaceCampaignPlayer(campaign, campaign->upperMaze.start, 0);
}

void StepCampaign(Campaign *campaign, Vector2 movement, Vector2 look, bool jump,
                  bool sprint, bool retry, float dt)
{
    if (campaign->cleared || campaign->gameOver || campaign->paused || campaign->death != CAMPAIGN_DEATH_NONE) return;
    dt = Clamp(dt, 0, 0.05f);
    if (retry)
    {
        LoseLife(campaign, CAMPAIGN_DEATH_NONE);
        return;
    }
    if (campaign->elevatorTime > 0)
    {
        RideMazeElevator(campaign, dt);
        return;
    }
    campaign->time += dt;
    campaign->hitTimer = fmaxf(0, campaign->hitTimer - dt);

    campaign->view.position = GetPlayerEye(&campaign->player);
    UpdateWalkCamera(&campaign->camera, &campaign->view, look, Vector2Zero(), 0);
    Vector3 direction = Vector3Subtract(campaign->camera.target, campaign->camera.position);
    Vector2 forward = Vector2Normalize((Vector2){ direction.x, direction.z });
    Vector2 right = { -forward.y, forward.x };
    PlayerInput controls = {
        .movement = Vector2Add(Vector2Scale(right, movement.x), Vector2Scale(forward, movement.y)),
        .jumpPressed = jump, .sprint = sprint,
    };
    Vector3 oldFeet = campaign->player.feet;
    BoundingBox boxes[CAMPAIGN_MAX_COLLIDERS + COLLISION_WIDTH * COLLISION_WIDTH];
    PlayerWorld world = CampaignWorld(CurrentLevel(campaign), oldFeet, boxes, campaign->debugOptions);
    StepPlayer(&campaign->player, &world, controls, dt);
    if (campaign->player.feet.y < -12.0f)
    {
        if (campaign->debugOptions.noDamage)
        {
            const LevelSpec *level = CurrentLevel(campaign);
            BoundingBox arena = Arena(campaign);
            if (campaign->checkpoint)
                PlaceCampaignPlayer(campaign, (Vector2){ (arena.min.x + arena.max.x) * 0.5f, arena.max.z - 0.9f },
                                 arena.max.y);
            else PlaceCampaignPlayer(campaign, level->start, level->platforms[0].max.y);
        }
        else LoseLife(campaign, CAMPAIGN_DEATH_FALL);
        return;
    }

    const LevelSpec *level = CurrentLevel(campaign);
    bool elevator = HasElevator(campaign);
    if (elevator && MazeCellAt(level, campaign->player.feet) == level->mazeExit)
    {
        campaign->view.yaw = PI;
        campaign->view.pitch = 0;
        PlaceCampaignPlayer(campaign, ElevatorCenter(level), 0);
        campaign->elevatorTime = ELEVATOR_RIDE_SECONDS;
        return;
    }
    BoundingBox arena = Arena(campaign);
    TrackMazePosition(campaign);
    if (!elevator && OnPlatform(campaign->player.feet, arena)) campaign->checkpoint = true;
    if (TouchTorches(campaign, level) || UpdateEncounter(campaign, oldFeet, dt)) return;

    if (!elevator && !CurrentBoss(campaign))
    {
        Vector3 portal = GatePosition(arena);
        if (Vector3DistanceSqr(campaign->player.feet, portal) < 1.15f * 1.15f)
        {
            if (campaign->endlessLevel)
                BeginStage(campaign, 0, false, campaign->lives, campaign->endlessLevel, campaign->stage + 1, campaign->debugOptions, false);
            else if (IsMinibossMaze(campaign))
            {
                bool editorPlaytest = campaign->editorPlaytest;
                BeginStage(campaign, campaign->level, campaign->debugRun, campaign->lives, NULL, 0,
                           campaign->debugOptions, true);
                campaign->editorPlaytest = editorPlaytest;
            }
            else if (campaign->debugRun || campaign->level == CAMPAIGN_LEVEL_COUNT - 1)
                campaign->cleared = true;
            else
                BeginStage(campaign, campaign->level + 1, false, campaign->lives, NULL, 0, campaign->debugOptions, false);
            return;
        }
    }
    campaign->view.position = campaign->camera.position = GetPlayerEye(&campaign->player);
    campaign->camera.target = Vector3Add(campaign->camera.position, direction);
}

static Texture2D LoadStoneTexture(int tileWidth, int tileHeight, bool stagger)
{
    enum { SIZE = 64 };
    Color pixels[SIZE * SIZE];
    for (int y = 0; y < SIZE; ++y)
    {
        int offset = stagger && (y / tileHeight) % 2 ? tileWidth / 2 : 0;
        for (int x = 0; x < SIZE; ++x)
        {
            int column = (x + offset) % SIZE;
            unsigned int grain = ((unsigned int)(x + y * SIZE) * 2654435761u) >> 28;
            int shade = 211 + ((column / tileWidth * 11 + y / tileHeight * 17) % 30);
            if (column % tileWidth == 0 || y % tileHeight == 0) shade = 119;
            else if (column % tileWidth == 1 || y % tileHeight == 1) shade = 243;
            unsigned char value = (unsigned char)(shade - grain);
            pixels[y * SIZE + x] = (Color){ value, value, value, 255 };
        }
    }
    Image image = { pixels, SIZE, SIZE, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D texture = LoadTextureFromImage(image);
    if (IsTextureValid(texture))
    {
        GenTextureMipmaps(&texture);
        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
        SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
    }
    return texture;
}

bool LoadCampaignSurfaces(CampaignSurfaces *surfaces, const char *shaderDir)
{
    surfaces->walls = LoadStoneTexture(32, 16, true);
    surfaces->platforms = LoadStoneTexture(32, 32, false);
    surfaces->ceiling = LoadStoneTexture(16, 32, true);
    surfaces->mazeLighting = LoadShader(TextFormat("%smaze.vert", shaderDir),
                                        TextFormat("%smaze.frag", shaderDir));
    if (!IsShaderValid(surfaces->mazeLighting) || surfaces->mazeLighting.id == rlGetShaderIdDefault())
        return false;
    surfaces->lightmapLocation = GetShaderLocation(surfaces->mazeLighting, "lightmap");
    surfaces->mazeSizeLocation = GetShaderLocation(surfaces->mazeLighting, "mazeSize");
    surfaces->flickerLocation = GetShaderLocation(surfaces->mazeLighting, "flicker");
    return surfaces->lightmapLocation >= 0 && surfaces->mazeSizeLocation >= 0 &&
           surfaces->flickerLocation >= 0 && IsTextureValid(surfaces->walls) &&
           IsTextureValid(surfaces->platforms) && IsTextureValid(surfaces->ceiling);
}

void UnloadCampaignSurfaces(CampaignSurfaces surfaces)
{
    if (surfaces.walls.id) UnloadTexture(surfaces.walls);
    if (surfaces.platforms.id) UnloadTexture(surfaces.platforms);
    if (surfaces.ceiling.id) UnloadTexture(surfaces.ceiling);
    if (surfaces.mazeLightmap.id) UnloadTexture(surfaces.mazeLightmap);
    if (surfaces.mazeLighting.id && surfaces.mazeLighting.id != rlGetShaderIdDefault())
        UnloadShader(surfaces.mazeLighting);
}

static void StoneFace(Vector3 a, Vector3 b, Vector3 c, Vector3 d, int axis, float side, Color tint)
{
    rlNormal3f(axis == 0 ? side : 0, axis == 1 ? side : 0, axis == 2 ? side : 0);
    const Vector3 corners[] = { a, b, c, d };
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    for (int i = 0; i < 4; ++i)
    {
        Vector3 v = corners[i];
        float u = axis == 0 ? v.z : v.x;
        float w = axis == 1 ? v.z : -v.y;
        rlTexCoord2f(u * 0.5f, w * 0.5f);
        rlVertex3f(v.x, v.y, v.z);
    }
}

static void DrawStoneBox(BoundingBox box, Color tint)
{
    Vector3 lo = box.min, hi = box.max;
    Vector3 a = lo, b = { hi.x, lo.y, lo.z }, c = { hi.x, hi.y, lo.z }, d = { lo.x, hi.y, lo.z };
    Vector3 e = { lo.x, lo.y, hi.z }, f = { hi.x, lo.y, hi.z }, g = hi, h = { lo.x, hi.y, hi.z };
    Color side = ColorBrightness(tint, -0.12f);
    StoneFace(b, a, d, c, 2, -1, tint);
    StoneFace(e, f, g, h, 2, 1, tint);
    StoneFace(a, e, h, d, 0, -1, side);
    StoneFace(f, b, c, g, 0, 1, side);
    StoneFace(d, h, g, c, 1, 1, tint);
    StoneFace(a, b, f, e, 1, -1, side);
}

static void DrawTexturedStone(Texture2D texture, BoundingBox box, Color color)
{
    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);
    DrawStoneBox(box, color);
    rlEnd();
    rlSetTexture(0);
}

static float TorchFlicker(Vector3 flame, float time)
{
    return 0.89f + 0.08f * sinf(time * 12.0f + flame.x) + 0.07f * sinf(time * 19.0f + flame.z);
}

enum {
    LIGHT_SAMPLES = 4,
    LIGHT_SAMPLE_COST = 10,
    LIGHT_DIAGONAL_COST = 14,
    LIGHT_RANGE_COST = 104,
    LIGHT_RADIUS = LIGHT_RANGE_COST / LIGHT_SAMPLE_COST,
    LIGHT_DIAMETER = 2 * LIGHT_RADIUS + 1,
    LIGHT_WINDOW = LIGHT_DIAMETER * LIGHT_DIAMETER
};
#define LIGHT_COST_TO_WORLD (CAMPAIGN_MAZE_CELL / (LIGHT_SAMPLES * LIGHT_SAMPLE_COST))
#define TORCH_RANGE_SQUARED ((LIGHT_RANGE_COST * LIGHT_COST_TO_WORLD) * (LIGHT_RANGE_COST * LIGHT_COST_TO_WORLD))

static bool BlocksLight(const LevelSpec *level, int row, int col)
{
    if (row < 0 || row >= level->mazeRows || col < 0 || col >= level->mazeCols) return true;
    char tile = level->maze[row * level->mazeCols + col];
    return tile == '#' || tile == 'E';
}

static unsigned char AddTorchSample(unsigned char existing, float squaredDistance)
{
    float falloff = fmaxf(0, 1.0f - squaredDistance / TORCH_RANGE_SQUARED);
    return (unsigned char)fminf(255, existing + 255 * falloff * falloff);
}

static void BakeTorch(const LevelSpec *level, Vector3 flame, Color *pixels, int width)
{
    Vector2 source = MazeGridPosition(level, flame);
    int sourceCol = (int)(source.x * LIGHT_SAMPLES), sourceRow = (int)(source.y * LIGHT_SAMPLES);
    int firstCol = sourceCol - LIGHT_RADIUS, firstRow = sourceRow - LIGHT_RADIUS;
    unsigned char cost[LIGHT_WINDOW];
    bool open[LIGHT_WINDOW], queued[LIGHT_WINDOW] = { 0 };
    int queue[LIGHT_WINDOW];
    for (int y = 0; y < LIGHT_DIAMETER; ++y)
        for (int x = 0; x < LIGHT_DIAMETER; ++x)
        {
            int col = firstCol + x, row = firstRow + y;
            int i = y * LIGHT_DIAMETER + x;
            open[i] = col >= 0 && row >= 0 &&
                      !BlocksLight(level, row / LIGHT_SAMPLES, col / LIGHT_SAMPLES);
            cost[i] = LIGHT_RANGE_COST;
        }

    int origin = LIGHT_RADIUS * LIGHT_DIAMETER + LIGHT_RADIUS;
    if (!open[origin]) return;
    cost[origin] = 0;
    queued[origin] = true;
    queue[0] = origin;

    // Relax nearby samples along open passages. Diagonal moves must clear both sides.
    int head = 0, tail = 1, pending = 1;
    while (pending > 0)
    {
        int i = queue[head];
        head = (head + 1) % LIGHT_WINDOW;
        --pending;
        queued[i] = false;
        int x = i % LIGHT_DIAMETER, y = i / LIGHT_DIAMETER;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || nx >= LIGHT_DIAMETER || ny < 0 || ny >= LIGHT_DIAMETER) continue;
                int next = ny * LIGHT_DIAMETER + nx;
                int distance = cost[i] + (dx && dy ? LIGHT_DIAGONAL_COST : LIGHT_SAMPLE_COST);
                if (!open[next] || distance >= cost[next]) continue;
                if (dx && dy && (!open[i + dx] || !open[i + dy * LIGHT_DIAMETER])) continue;
                cost[next] = (unsigned char)distance;
                if (!queued[next])
                {
                    queue[tail] = next;
                    tail = (tail + 1) % LIGHT_WINDOW;
                    ++pending;
                    queued[next] = true;
                }
            }
    }

    float ceilingOffset = CAMPAIGN_MAZE_HEIGHT - flame.y;
    for (int i = 0; i < LIGHT_WINDOW; ++i)
    {
        if (cost[i] >= LIGHT_RANGE_COST) continue;
        int row = firstRow + i / LIGHT_DIAMETER, col = firstCol + i % LIGHT_DIAMETER;
        Color *pixel = &pixels[row * width + col];
        float distance = cost[i] * LIGHT_COST_TO_WORLD;
        float squaredDistance = distance * distance;
        pixel->r = AddTorchSample(pixel->r, squaredDistance + flame.y * flame.y);
        pixel->g = AddTorchSample(pixel->g, squaredDistance);
        pixel->b = AddTorchSample(pixel->b, squaredDistance + ceilingOffset * ceilingOffset);
    }
}

void PrepareCampaignLighting(const Campaign *campaign, CampaignSurfaces *surfaces)
{
    const LevelSpec *level = CurrentLevel(campaign);
    unsigned long long stage = campaign->stage;
    if (!level->mazeRows) return;
    if (surfaces->litLevel == level && surfaces->litStage == stage && surfaces->litFloor == campaign->mazeFloor) return;
    if (surfaces->mazeLightmap.id) UnloadTexture(surfaces->mazeLightmap);
    surfaces->mazeLightmap = (Texture2D){ 0 };
    surfaces->litLevel = level;
    surfaces->litStage = stage;
    surfaces->litFloor = campaign->mazeFloor;

    int width = level->mazeCols * LIGHT_SAMPLES, height = level->mazeRows * LIGHT_SAMPLES;
    Color *pixels = calloc((size_t)width * height, sizeof(*pixels));
    if (!pixels) return;
    for (int row = 0; row < level->mazeRows; ++row)
        for (int col = 0; col < level->mazeCols; ++col)
        {
            Vector3 flame;
            if (WallTorch(level, row, col, &flame, NULL)) BakeTorch(level, flame, pixels, width);
        }

    Image image = { pixels, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    surfaces->mazeLightmap = LoadTextureFromImage(image);
    free(pixels);
    if (surfaces->mazeLightmap.id)
    {
        SetTextureFilter(surfaces->mazeLightmap, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(surfaces->mazeLightmap, TEXTURE_WRAP_CLAMP);
    }
}

static void DrawWallTorches(const LevelSpec *level, Vector3 eye, float time)
{
    MazeRegion region = NearbyMaze(level, eye, 10);
    for (int row = region.firstRow; row < region.lastRow; ++row)
        for (int col = region.firstCol; col < region.lastCol; ++col)
        {
            Vector3 flame, mount;
            if (!WallTorch(level, row, col, &flame, &mount)) continue;
            Vector3 base = { flame.x, flame.y - 0.13f, flame.z };
            DrawCylinderEx(mount, base, 0.065f, 0.095f, 6, (Color){ 88, 54, 31, 255 });
            DrawCylinder(base, 0.13f, 0.09f, 0.10f, 7, (Color){ 65, 55, 51, 255 });
            float flicker = TorchFlicker(flame, time);
            Vector3 tip = { flame.x + 0.05f * sinf(time * 15.0f + flame.z),
                            flame.y + 0.36f * flicker, flame.z };
            Vector3 wick = Vector3Add(base, (Vector3){ 0, 0.101f, 0 });
            DrawCylinderEx(wick, tip, 0.12f, 0.015f, 7, ORANGE);
            DrawSphereEx(flame, 0.09f * flicker, 5, 7, YELLOW);
        }
}

// Test the entire box against the active clip volume, including near and far.
static bool MazeChunkInView(BoundingBox box, Matrix clip)
{
    Vector3 center = Vector3Scale(Vector3Add(box.min, box.max), 0.5f);
    Vector3 half = Vector3Subtract(box.max, center);
    const Vector4 axes[] = {
        { clip.m0, clip.m4, clip.m8, clip.m12 },
        { clip.m1, clip.m5, clip.m9, clip.m13 },
        { clip.m2, clip.m6, clip.m10, clip.m14 },
    };
    for (int i = 0; i < 3; ++i)
        for (int sign = -1; sign <= 1; sign += 2)
        {
            Vector3 normal = { clip.m3 + sign * axes[i].x, clip.m7 + sign * axes[i].y,
                               clip.m11 + sign * axes[i].z };
            float distance = Vector3DotProduct(normal, center) + clip.m15 + sign * axes[i].w;
            float radius = fabsf(normal.x) * half.x + fabsf(normal.y) * half.y + fabsf(normal.z) * half.z;
            if (distance < -radius) return false;
        }
    return true;
}

static bool IsMazeStone(const LevelSpec *level, int row, int col, bool hideExit)
{
    // The concealed exit renders as stone but stays traversable.
    if (row < 0 || row >= level->mazeRows || col < 0 || col >= level->mazeCols) return false;
    char tile = level->maze[row * level->mazeCols + col];
    return tile == '#' || (hideExit && tile == 'E');
}

static void DrawMazeWalls(const LevelSpec *level, const CampaignSurfaces *surfaces,
                          Texture2D lightmap, Color wall, bool hideExit)
{
    enum { CHUNK_SIDE = 16 };
    Matrix clip = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    Color side = ColorBrightness(wall, -0.12f);
    rlSetTexture(surfaces->walls.id);
    rlBegin(RL_QUADS);
    for (int row = 0; row < level->mazeRows; row += CHUNK_SIDE)
        for (int col = 0; col < level->mazeCols; col += CHUNK_SIDE)
        {
            int lastRow = (int)fminf(row + CHUNK_SIDE, level->mazeRows);
            int lastCol = (int)fminf(col + CHUNK_SIDE, level->mazeCols);
            BoundingBox bounds = MazeWall(level, row, col, lastCol);
            bounds.max.z = level->platforms[0].min.z + lastRow * CAMPAIGN_MAZE_CELL;
            if (!MazeChunkInView(bounds, clip)) continue;

            for (int z = row; z < lastRow; ++z)
                for (int x = col; x < lastCol; ++x)
                {
                    if (!IsMazeStone(level, z, x, hideExit)) continue;
                    bool north = !IsMazeStone(level, z - 1, x, hideExit), south = !IsMazeStone(level, z + 1, x, hideExit);
                    bool west = !IsMazeStone(level, z, x - 1, hideExit), east = !IsMazeStone(level, z, x + 1, hideExit);
                    int vertices = 4 * (north + south + west + east);
                    if (!vertices) continue;
                    // A batch flush clears secondary samplers.
                    if (rlCheckRenderBatchLimit(vertices))
                        SetShaderValueTexture(surfaces->mazeLighting, surfaces->lightmapLocation, lightmap);
                    BoundingBox box = MazeWall(level, z, x, x + 1);
                    Vector3 lo = box.min, hi = box.max;
                    Vector3 a = lo, b = { hi.x, lo.y, lo.z }, c = { hi.x, hi.y, lo.z }, d = { lo.x, hi.y, lo.z };
                    Vector3 e = { lo.x, lo.y, hi.z }, f = { hi.x, lo.y, hi.z }, g = hi, h = { lo.x, hi.y, hi.z };
                    // The continuous floor and roof cover the tops and bottoms of the walls.
                    if (north) StoneFace(b, a, d, c, 2, -1, wall);
                    if (south) StoneFace(e, f, g, h, 2, 1, wall);
                    if (west) StoneFace(a, e, h, d, 0, -1, side);
                    if (east) StoneFace(f, b, c, g, 0, 1, side);
                }
        }
    rlEnd();
    rlSetTexture(0);
}

static void DrawCampaignSurfaces(const LevelSpec *level, const CampaignSurfaces *surfaces,
                                 Vector3 eye, float time, bool elevator)
{
    Color floor = ColorLerp(level->stone, level->trim, 0.19f);
    Color wall = ColorLerp(level->stone, (Color){ 15, 24, 35, 255 }, 0.24f);
    Color ceiling = ColorBrightness(floor, -0.15f);
    if (!elevator)
    {
        int firstPlatform = level->mazeRows ? 1 : 0;
        for (int i = firstPlatform; i < level->platformCount; ++i)
            DrawTexturedStone(surfaces->platforms, level->platforms[i], floor);

        rlSetTexture(surfaces->walls.id);
        rlBegin(RL_QUADS);
        for (int i = level->platformCount; i < level->wallEnd; ++i) DrawStoneBox(level->colliders[i], wall);
        rlEnd();
        rlSetTexture(0);
        DrawTexturedStone(surfaces->ceiling, level->colliders[level->wallEnd], ceiling);
    }
    if (!level->mazeRows) return;

    Vector2 dims = { (float)level->mazeCols, (float)level->mazeRows };
    float flicker = surfaces->mazeLightmap.id ? TorchFlicker(Vector3Zero(), time) : 0;
    Texture2D lightmap = surfaces->mazeLightmap.id ? surfaces->mazeLightmap : surfaces->walls;
    BeginShaderMode(surfaces->mazeLighting);
    SetShaderValue(surfaces->mazeLighting, surfaces->mazeSizeLocation, &dims, SHADER_UNIFORM_VEC2);
    SetShaderValue(surfaces->mazeLighting, surfaces->flickerLocation, &flicker, SHADER_UNIFORM_FLOAT);
    SetShaderValueTexture(surfaces->mazeLighting, surfaces->lightmapLocation, lightmap);
    DrawTexturedStone(surfaces->platforms, level->platforms[0], floor);
    DrawTexturedStone(surfaces->ceiling, level->colliders[level->wallEnd + 1], ceiling);
    DrawMazeWalls(level, surfaces, lightmap, wall, !elevator);
    EndShaderMode();
    DrawWallTorches(level, eye, time);
}

static void DrawBossModel(const Campaign *campaign, const BossSpec *boss)
{
    Vector3 center = campaign->bossPosition;
    Color phaseColor = ColorLerp(boss->color, ORANGE, campaign->bossPhase * 0.26f);
    Color body = campaign->guardTimer > 0 ? RAYWHITE : phaseColor;
    DrawCylinder(center, boss->radius * 0.76f, boss->radius, boss->height, 12, body);
    Vector3 core = center;
    core.y += boss->height * 0.96f;
    DrawSphereEx(core, boss->radius * 0.32f, 8, 12, RAYWHITE);
    if (campaign->checkpoint && campaign->attackTimer < 0.58f)
    {
        Vector3 warning = campaign->bossPosition;
        warning.y += 0.024f;
        DrawCylinderWires(warning, boss->radius * 1.6f, boss->radius * 1.6f, 0.035f, 24, GOLD);
    }
}

static void DrawMazeElevator(const Campaign *campaign, const LevelSpec *level)
{
    Vector2 center = ElevatorCenter(level);
    bool riding = campaign->elevatorTime > 0;
    float y = riding ? campaign->camera.position.y - GetPlayerEye(&campaign->player).y : 0;
    const Color steel = { 64, 84, 96, 255 };
    DrawCube((Vector3){ center.x, y - 0.095f, center.y }, 2.48f, 0.24f, 2.48f, steel);
    DrawCube((Vector3){ center.x, y + 3.12f, center.y }, 2.48f, 0.24f, 2.48f, steel);
    for (int side = -1; side <= 1; side += 2)
    {
        float x = center.x + side * 1.18f;
        DrawCube((Vector3){ x, y + 1.5f, center.y }, 0.12f, 3.0f, 2.48f, steel);
        DrawCube((Vector3){ x, y + 2.65f, center.y + 1.24f }, 0.13f, 0.7f, 0.08f, GOLD);
    }

    if (!riding)
        DrawCube((Vector3){ center.x, 1.5f, center.y - 1.18f }, 2.48f, 3.0f, 0.12f, steel);
    else
    {
        DrawCube((Vector3){ center.x, y + 1.5f, center.y + 1.18f }, 2.48f, 3.0f, 0.12f, steel);
        // The fixed bands behind the moving cabin make the ascent visible.
        DrawCube((Vector3){ center.x, 4.0f, center.y - 1.4f }, 2.48f, 9.0f, 0.14f, (Color){ 38, 47, 56, 255 });
        for (int band = 1; band < 9; ++band)
            DrawCube((Vector3){ center.x, (float)band, center.y - 1.31f }, 1.9f, 0.09f, 0.08f,
                     band % 2 ? GOLD : level->trim);
    }
}

void DrawCampaignWorld(const Campaign *campaign, const CampaignSurfaces *surfaces)
{
    const LevelSpec *level = CurrentLevel(campaign);
    if (campaign->elevatorTime > 0)
    {
        DrawMazeElevator(campaign, level);
        return;
    }
    DrawPlane((Vector3){ 0, -13, -19 }, (Vector2){ 130, 130 }, (Color){ 27, 47, 59, 255 });
    bool elevator = HasElevator(campaign);
    DrawCampaignSurfaces(level, surfaces, campaign->camera.position, campaign->time, elevator);
    if (elevator)
    {
        DrawMazeElevator(campaign, level);
        return;
    }
    BoundingBox arena = Arena(campaign);
    Vector3 gate = GatePosition(arena);
    Color frame = CurrentBoss(campaign) ? (Color){ 96, 116, 128, 255 } : level->trim;
    DrawCube((Vector3){ gate.x - 0.83f, gate.y + 1.1f, gate.z }, 0.2f, 2.2f, 0.26f, frame);
    DrawCube((Vector3){ gate.x + 0.83f, gate.y + 1.1f, gate.z }, 0.2f, 2.2f, 0.26f, frame);
    DrawCube((Vector3){ gate.x, gate.y + 2.305f, gate.z }, 1.86f, 0.21f, 0.26f, frame);
    for (int side = -1; side <= 1; side += 2)
    {
        float x = side < 0 ? arena.min.x - 0.55f : arena.max.x + 0.55f;
        float z = arena.max.z - 1.0f;
        DrawCylinder((Vector3){ x, arena.max.y - 1.5f, z }, 0.31f, 0.46f, 2.42f, 6, level->stone);
        float pulse = 0.04f * sinf(campaign->time * 3.0f + side);
        DrawSphereEx((Vector3){ x, arena.max.y + 1.17f + pulse, z }, 0.23f, 6, 8, level->trim);
    }
    const BossSpec *boss = CurrentBoss(campaign);
    if (boss) DrawBossModel(campaign, boss);
    for (int i = 0; i < CAMPAIGN_MAX_ORBS; ++i)
        if (campaign->orbs[i].life > 0) DrawSphereEx(campaign->orbs[i].position, 0.18f, 5, 8, ORANGE);
    if (!boss)
    {
        Vector3 panel = { gate.x, gate.y + 1.05f, gate.z };
        rlDrawRenderBatchActive();
        rlDisableDepthMask();
        rlDisableBackfaceCulling();
        DrawTriangle3D((Vector3){ panel.x - 0.725f, panel.y - 1.04f, panel.z },
                       (Vector3){ panel.x + 0.725f, panel.y - 1.04f, panel.z },
                       (Vector3){ panel.x + 0.725f, panel.y + 1.04f, panel.z }, Fade(frame, 0.44f));
        DrawTriangle3D((Vector3){ panel.x - 0.725f, panel.y - 1.04f, panel.z },
                       (Vector3){ panel.x + 0.725f, panel.y + 1.04f, panel.z },
                       (Vector3){ panel.x - 0.725f, panel.y + 1.04f, panel.z }, Fade(frame, 0.44f));
        rlDrawRenderBatchActive();
        rlEnableBackfaceCulling();
        rlEnableDepthMask();
        DrawCubeWires(panel, 1.45f, 2.08f, 0.08f, RAYWHITE);
    }
}

static Rectangle MenuBounds(void)
{
    Vector2 inset = GameUiInset();
    return (Rectangle){ GetScreenWidth() - 111.0f - inset.x, 12 + inset.y, 99, 29 };
}

bool ReadCampaignMenu(InputPointer pointer)
{
    return ReadUiButton(MenuBounds(), pointer, true).activated;
}



static Rectangle PauseButton(int index)
{
    return (Rectangle){ GetScreenWidth() * 0.5f - 116, GetScreenHeight() * 0.5f - 42 + 46 * index, 232, 38 };
}

CampaignPauseAction ReadCampaignPause(InputPointer pointer)
{
    for (int i = 0; i < 3; ++i)
        if (ReadUiButton(PauseButton(i), pointer, true).activated) return CAMPAIGN_PAUSE_RESUME + i;
    return CAMPAIGN_PAUSE_NONE;
}

static void DrawCampaignPause(const Campaign *campaign, InputPointer pointer)
{
    int width = GetScreenWidth(), height = GetScreenHeight();
    DrawRectangle(0, 0, width, height, (Color){ 8, 15, 24, 201 });
    DrawUiPanel((Rectangle){ width * 0.5f - 140, height * 0.5f - 109, 280, 219 },
                (Color){ 15, 24, 35, 255 });
    DrawText("Paused", (width - MeasureText("Paused", 28)) / 2, height / 2 - 89, 28, RAYWHITE);
    for (int i = 0; i < 3; ++i)
    {
        Rectangle button = PauseButton(i);
        const char *retry = campaign->checkpoint ? "Retry checkpoint" : "Retry maze";
        if (!campaign->debugRun) retry = "Retry (costs a life)";
        const char *labels[] = { "Resume", retry, campaign->editorPlaytest ? "Maze editor" : campaign->debugRun ? "Level select" : "Mazes menu" };
        const char *label = labels[i];
        DrawUiButton(button, label, ReadUiButton(button, pointer, true), true);
    }
}

static Rectangle ContinueBounds(void)
{
    return (Rectangle){ GetScreenWidth() * 0.5f - 116, GetScreenHeight() * 0.5f + 42, 232, 38 };
}

bool ReadCampaignContinue(InputPointer pointer)
{
    return ReadUiButton(ContinueBounds(), pointer, true).activated;
}

static void DrawCampaignDeath(const Campaign *campaign, InputPointer pointer)
{
    int width = GetScreenWidth(), height = GetScreenHeight();
    const char *title = "You died";
    const char *cause = campaign->death == CAMPAIGN_DEATH_TORCH ? "Burned by a wall torch" :
                        campaign->death == CAMPAIGN_DEATH_FALL ? "Fell into the void" : "Defeated by the boss";
    const char *status = campaign->debugRun ? "Debug run" : TextFormat("Lives remaining %d", campaign->lives);
    if (campaign->gameOver)
        status = TextFormat("No lives left   /   Mazes cleared %llu", campaign->endlessLevel ?
                            campaign->stage - 1 : (unsigned long long)campaign->level);
    const char *button = campaign->gameOver ? "Return to Mazes" :
                         campaign->checkpoint ? "Respawn at checkpoint" : "Respawn at maze start";
    DrawRectangle(0, 0, width, height, (Color){ 25, 9, 16, 229 });
    DrawText(title, (width - MeasureText(title, 32)) / 2, height / 2 - 83, 32, (Color){ 250, 134, 111, 255 });
    DrawText(cause, (width - MeasureText(cause, 17)) / 2, height / 2 - 31, 17, RAYWHITE);
    DrawText(status, (width - MeasureText(status, 15)) / 2, height / 2, 15, LIGHTGRAY);
    Rectangle bounds = ContinueBounds();
    DrawUiButton(bounds, button, ReadUiButton(bounds, pointer, true), true);
}

static void DrawMazeMap(const Campaign *campaign)
{
    const LevelSpec *level = CurrentLevel(campaign);
    if (!level->mazeRows || campaign->checkpoint) return;

    Vector2 inset = GameUiInset();
    int y = (int)inset.y + 106;
    int maxSize = GetScreenHeight() < 480 ? 94 : 144;
    if (HasPlayerTouchControls())
    {
        // Keep the map clear of the crosshair and the jump button.
        maxSize = (int)fminf(maxSize, PlayerTouchTop() - y - 14);
        maxSize = (int)fminf(maxSize, GetScreenWidth() * 0.5f - inset.x - 36);
    }
    if (maxSize < 28) return;
    int longest = level->mazeRows > level->mazeCols ? level->mazeRows : level->mazeCols;
    int unit = maxSize / longest;
    int visibleRows = level->mazeRows, visibleCols = level->mazeCols;
    int firstRow = 0, firstCol = 0;
    if (unit < 2)
    {
        unit = 2;
        int tiles = maxSize / unit;
        if (visibleRows > tiles) visibleRows = tiles;
        if (visibleCols > tiles) visibleCols = tiles;
        Vector2 grid = MazeGridPosition(level, campaign->player.feet);
        firstRow = (int)Clamp((int)grid.y - visibleRows / 2, 0, level->mazeRows - visibleRows);
        firstCol = (int)Clamp((int)grid.x - visibleCols / 2, 0, level->mazeCols - visibleCols);
    }

    int width = visibleCols * unit, height = visibleRows * unit;
    int x = GetScreenWidth() - width - 20 - (int)inset.x;
    DrawUiPanel((Rectangle){ (float)x - 6, (float)y - 20, (float)width + 12, (float)height + 26 },
                (Color){ 12, 20, 29, 223 });
    DrawText("N", x, y - 16, 11, RAYWHITE);
    int cell = MazeCellAt(level, campaign->player.feet);
    for (int row = 0; row < visibleRows; ++row)
    {
        int start = (firstRow + row) * level->mazeCols + firstCol;
        for (int col = 0; col < visibleCols; ++col)
        {
            int i = start + col;
            if (!campaign->mazeVisited[i] && i != cell) continue;
            Color ink = i == cell ? RAYWHITE : (Color){ 105, 132, 135, 255 };
            DrawRectangle(x + col * unit, y + row * unit, unit, unit, ink);
        }
    }
}

void DrawCampaignHud(const Campaign *campaign, InputPointer pointer)
{
    if (campaign->death != CAMPAIGN_DEATH_NONE)
    {
        DrawCampaignDeath(campaign, pointer);
        return;
    }
    if (campaign->paused)
    {
        DrawCampaignPause(campaign, pointer);
        return;
    }
    const LevelSpec *level = CurrentLevel(campaign);
    int width = GetScreenWidth(), height = GetScreenHeight();
    Vector2 inset = GameUiInset();
    int hudX = (int)inset.x, hudY = (int)inset.y;
    int hudBottom = 79 + hudY;
    const Color panel = { 15, 24, 35, 234 };
    DrawUiPanel((Rectangle){ 0, 0, (float)width, (float)hudBottom }, panel);
    const char *heading = campaign->endlessLevel ? level->name :
        TextFormat("%d / %d   %s", campaign->level + 1, CAMPAIGN_LEVEL_COUNT, level->name);
    if (HasExtraMaze(campaign))
        heading = TextFormat("%s  /  Maze %d of 2", heading, campaign->secondMaze ? 2 : 1);
    DrawUiLabel(heading, 16 + hudX, 12 + hudY, 20, MenuBounds().x - 24 - hudX, RAYWHITE);
    if (campaign->endlessLevel)
        DrawText(TextFormat("Lives %d    %d x %d", campaign->lives, level->mazeCols, level->mazeRows),
                 96 + hudX, 31 + hudY, 13, level->trim);
    else if (!campaign->debugRun || campaign->debugOptions.enabled)
        DrawText(TextFormat("Lives %d", campaign->lives), 96 + hudX, 31 + hudY, 13, level->trim);
    for (int i = 0; i < 3; ++i)
        DrawRectangle(18 + hudX + 23 * i, 51 + hudY, 17, 10, i < campaign->health ? level->trim : DARKGRAY);
    const BossSpec *boss = CurrentBoss(campaign);
    const char *objective = !campaign->checkpoint ? (!level->mazeRows ? "Reach the arena" :
           campaign->elevatorTime > 0 ? TextFormat("Elevator to floor %d", campaign->mazeFloor + 2) :
           TextFormat("Floor %d / %d   |   %s", campaign->mazeFloor + 1, MAZE_FLOORS,
                      HasElevator(campaign) ? "Find the elevator" : "Explore the maze"))
        : boss ? TextFormat("Stomp %s  %d/%d  /  Phase %d/%d", boss->name, campaign->bossHealth, boss->health,
                            campaign->bossPhase + 1, boss->phaseCount)
               : IsMinibossMaze(campaign) ? "Gate to the second maze open" : "Exit open";
    DrawUiLabel(objective, 96 + hudX, 48 + hudY, 15, width - 104 - 2 * hudX, RAYWHITE);
    bool touch = HasPlayerTouchControls();
    const char *back = (campaign->cleared || campaign->gameOver) ? (campaign->editorPlaytest ? "Editor" : campaign->debugRun ? "Levels" : "Mazes")
                                        : (touch ? "Pause" : "Esc Pause");
    DrawUiButton(MenuBounds(), back, ReadUiButton(MenuBounds(), pointer, true), true);

    if (campaign->cleared || campaign->gameOver)
    {
        DrawRectangle(0, hudBottom, width, height - hudBottom, (Color){ 8, 15, 24, 225 });
        const char *message = campaign->gameOver ? "Game over" : (campaign->debugRun ? "Level clear" : "Mazes complete");
        DrawText(message, (width - MeasureText(message, 33)) / 2, height / 2 - 30, 33, level->trim);
        const char *prompt = campaign->editorPlaytest ? "Return to maze editor" :
                             campaign->debugRun ? "Return to level select" : "All bosses defeated";
        if (campaign->gameOver)
            prompt = TextFormat("Mazes cleared %llu", campaign->endlessLevel ?
                               campaign->stage - 1 : (unsigned long long)campaign->level);
        DrawText(prompt, (width - MeasureText(prompt, 17)) / 2, height / 2 + 15, 17, RAYWHITE);
        if (!touch)
        {
            const char *hint = "Press Enter to return";
            DrawText(hint, (width - MeasureText(hint, 14)) / 2, height / 2 + 44, 14, LIGHTGRAY);
        }
        return;
    }

    DrawMazeMap(campaign);
    if (campaign->phaseNotice > 0)
    {
        const char *label = TextFormat("Phase %d", campaign->bossPhase + 1);
        DrawText(label, (width - MeasureText(label, 24)) / 2, 105 + hudY, 24, GOLD);
    }
    int x = width / 2, y = height / 2;
    DrawLine(x - 4, y, x + 4, y, RAYWHITE);
    DrawLine(x, y - 4, x, y + 4, RAYWHITE);
    if (campaign->hitTimer > 0.68f) DrawRectangleLines(2, 2, width - 4, height - 4, RED);
    if (HasPlayerTouchControls()) DrawPlayerTouch(&campaign->touch);
    else DrawText("WASD move   Space jump   Shift sprint   R retry", 15, height - 26, 14, RAYWHITE);
}

static Rectangle DebugButton(int slot)
{
    float width = (float)GetScreenWidth(), height = (float)GetScreenHeight();
    float buttonWidth = fminf(246, (width - 40) / 2);
    float row = Clamp((height - 95) / 4.0f, 25, 57);
    float x = (width - (buttonWidth * 2 + 12)) / 2;
    return (Rectangle){ x + (slot % 2) * (buttonWidth + 12),
                        (height - 4 * row) / 2 + 14 + (slot / 2) * row, buttonWidth, row - 7 };
}

int ReadMazeLevelSelect(InputPointer pointer, bool editing)
{
    for (int i = 0; i <= CAMPAIGN_LEVEL_COUNT; ++i)
    {
        if (editing && i < CAMPAIGN_LEVEL_COUNT && !campaignLevels[i].mazeRows) continue;
        if (ReadUiButton(DebugButton(i), pointer, true).activated ||
            (i < CAMPAIGN_LEVEL_COUNT && IsKeyPressed(KEY_ONE + i)))
            return i;
    }
    return -1;
}

void DrawMazeLevelSelect(Vector2 position, bool editing)
{
    int width = GetScreenWidth(), height = GetScreenHeight();
    DrawRectangleGradientV(0, 0, width, height, (Color){ 12, 26, 41, 255 }, (Color){ 42, 67, 67, 255 });
    const char *title = editing ? "Maze Editor" : "Debug Level Select";
    DrawText(title, (width - MeasureText(title, 25)) / 2, (int)DebugButton(0).y - 44, 25, RAYWHITE);
    InputPointer pointer = { .position = position };
    for (int i = 0; i <= CAMPAIGN_LEVEL_COUNT; ++i)
    {
        Rectangle button = DebugButton(i);
        bool back = i == CAMPAIGN_LEVEL_COUNT;
        const char *label = back ? "Back" : TextFormat("%d   %s", i + 1, campaignLevels[i].name);
        bool enabled = back || !editing || campaignLevels[i].mazeRows > 0;
        DrawUiButton(button, label, ReadUiButton(button, pointer, enabled), enabled);
    }
}
