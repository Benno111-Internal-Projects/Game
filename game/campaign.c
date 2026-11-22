#include "campaign.h"
#include "campaign_content.h"
#include "raymath.h"
#include "rlgl.h"
#include "ui.h"

#include <math.h>
#include <string.h>

static const LevelSpec *CurrentLevel(const Campaign *campaign)
{
    return campaign->endlessLevel ? campaign->endlessLevel : &campaignLevels[campaign->level];
}

static BoundingBox Arena(const Campaign *campaign)
{
    const LevelSpec *level = CurrentLevel(campaign);
    return level->platforms[level->platformCount - 1];
}

static const BossSpec *CurrentBoss(const Campaign *campaign)
{
    const LevelSpec *level = CurrentLevel(campaign);
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

static PlayerWorld CampaignWorld(const LevelSpec *level, Vector3 position, BoundingBox *boxes)
{
    int count = level->colliderCount;
    memcpy(boxes, level->colliders, (size_t)count * sizeof(*boxes));

    // The player can move less than one tile in a simulation step.
    MazeRegion region = NearbyMaze(level, position, COLLISION_RADIUS);
    for (int row = region.firstRow; row < region.lastRow; ++row)
        for (int col = region.firstCol; col < region.lastCol; ++col)
            if (level->maze[row * level->mazeCols + col] == '#')
                boxes[count++] = MazeWall(level, row, col, col + 1);

    return (PlayerWorld){ .groundHeight = VoidHeight, .boxes = boxes, .boxCount = (size_t)count };
}

static void Respawn(Campaign *campaign)
{
    const LevelSpec *level = CurrentLevel(campaign);
    BoundingBox arena = Arena(campaign);
    Vector2 spawn = campaign->checkpoint
        ? (Vector2){ (arena.min.x + arena.max.x) * 0.5f, arena.max.z - 0.9f }
        : level->start;
    BoundingBox boxes[CAMPAIGN_MAX_COLLIDERS + COLLISION_WIDTH * COLLISION_WIDTH];
    PlayerWorld world = CampaignWorld(level, (Vector3){ spawn.x, 0, spawn.y }, boxes);
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

static void BeginStage(Campaign *campaign, int level, bool debugRun, int lives,
                       LevelSpec *endlessLevel, unsigned long long stage)
{
    *campaign = (Campaign){
        .level = level, .debugRun = debugRun, .lives = lives,
        .endlessLevel = endlessLevel, .stage = stage,
        .camera = { .up = { 0, 1, 0 }, .fovy = 48, .projection = CAMERA_PERSPECTIVE },
    };
    if (endlessLevel) GenerateMazeLevel(endlessLevel, stage);
    Respawn(campaign);
}

void StartCampaign(Campaign *campaign, int level, bool debugRun)
{
    if (level < 0 || level >= CAMPAIGN_LEVEL_COUNT) level = 0;
    BeginStage(campaign, level, debugRun, MAZE_RUN_LIVES, NULL, 0);
}

void StartEndlessMazes(Campaign *campaign, LevelSpec *level)
{
    BeginStage(campaign, 0, false, MAZE_RUN_LIVES, level, 1);
}

static void LoseLife(Campaign *campaign)
{
    if (!campaign->debugRun && --campaign->lives == 0)
    {
        campaign->gameOver = true;
        return;
    }
    Respawn(campaign);
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

static bool DamagePlayer(Campaign *campaign, Vector3 origin)
{
    if (campaign->hitTimer > 0) return false;
    if (--campaign->health == 0)
    {
        LoseLife(campaign);
        return true;
    }
    campaign->hitTimer = 0.95f;
    Vector3 away = Vector3Subtract(campaign->player.feet, origin);
    Vector2 push = Vector2Scale(Vector2Normalize((Vector2){ away.x, away.z }), 3.8f);
    campaign->player.velocity = (Vector3){ push.x, 4.1f, push.y };
    campaign->player.grounded = false;
    campaign->player.groundGrace = campaign->player.jumpBuffer = 0;
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
            if (DamagePlayer(campaign, center)) return true;
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
            if (DamagePlayer(campaign, orb->position)) return true;
        }
    }
    return false;
}

void StepCampaign(Campaign *campaign, Vector2 movement, Vector2 look, bool jump,
                  bool sprint, bool retry, float dt)
{
    if (campaign->cleared || campaign->gameOver || campaign->paused) return;
    if (retry)
    {
        LoseLife(campaign);
        return;
    }
    dt = Clamp(dt, 0, 0.05f);
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
    PlayerWorld world = CampaignWorld(CurrentLevel(campaign), oldFeet, boxes);
    StepPlayer(&campaign->player, &world, controls, dt);
    if (campaign->player.feet.y < -12.0f)
    {
        LoseLife(campaign);
        return;
    }

    BoundingBox arena = Arena(campaign);
    TrackMazePosition(campaign);
    if (OnPlatform(campaign->player.feet, arena)) campaign->checkpoint = true;
    if (UpdateEncounter(campaign, oldFeet, dt)) return;

    if (!CurrentBoss(campaign))
    {
        Vector3 portal = GatePosition(arena);
        if (Vector3DistanceSqr(campaign->player.feet, portal) < 1.15f * 1.15f)
        {
            if (campaign->endlessLevel)
                BeginStage(campaign, 0, false, campaign->lives, campaign->endlessLevel, campaign->stage + 1);
            else if (campaign->debugRun || campaign->level == CAMPAIGN_LEVEL_COUNT - 1)
                campaign->cleared = true;
            else
                BeginStage(campaign, campaign->level + 1, false, campaign->lives, NULL, 0);
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

bool LoadCampaignSurfaces(CampaignSurfaces *surfaces)
{
    surfaces->walls = LoadStoneTexture(32, 16, true);
    surfaces->platforms = LoadStoneTexture(32, 32, false);
    surfaces->ceiling = LoadStoneTexture(16, 32, true);
    return IsTextureValid(surfaces->walls) && IsTextureValid(surfaces->platforms) &&
           IsTextureValid(surfaces->ceiling);
}

void UnloadCampaignSurfaces(CampaignSurfaces surfaces)
{
    if (surfaces.walls.id) UnloadTexture(surfaces.walls);
    if (surfaces.platforms.id) UnloadTexture(surfaces.platforms);
    if (surfaces.ceiling.id) UnloadTexture(surfaces.ceiling);
}

static void StoneFace(Vector3 a, Vector3 b, Vector3 c, Vector3 d, int axis, Color tint)
{
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
    StoneFace(b, a, d, c, 2, tint);
    StoneFace(e, f, g, h, 2, tint);
    StoneFace(a, e, h, d, 0, side);
    StoneFace(f, b, c, g, 0, side);
    StoneFace(d, h, g, c, 1, tint);
    StoneFace(a, b, f, e, 1, side);
}

static void DrawCampaignSurfaces(const LevelSpec *level, const CampaignSurfaces *surfaces, Vector3 eye)
{
    rlSetTexture(surfaces->platforms.id);
    rlBegin(RL_QUADS);
    Color floor = ColorLerp(level->stone, level->trim, 0.19f);
    for (int i = 0; i < level->platformCount; ++i) DrawStoneBox(level->platforms[i], floor);
    rlEnd();

    rlSetTexture(surfaces->walls.id);
    rlBegin(RL_QUADS);
    Color wall = ColorLerp(level->stone, (Color){ 15, 24, 35, 255 }, 0.24f);
    for (int i = level->platformCount; i < level->wallEnd; ++i) DrawStoneBox(level->colliders[i], wall);
    MazeRegion region = NearbyMaze(level, eye, 32);
    for (int row = region.firstRow; row < region.lastRow; ++row)
    {
        const char *tiles = &level->maze[row * level->mazeCols];
        int col = region.firstCol;
        while (col < region.lastCol)
        {
            if (tiles[col] != '#' && tiles[col] != 'E')
            {
                ++col;
                continue;
            }
            int first = col++;
            // The exit tile looks like its neighbors and has no collider.
            while (col < region.lastCol && (tiles[col] == '#' || tiles[col] == 'E')) ++col;
            DrawStoneBox(MazeWall(level, row, first, col), wall);
        }
    }
    rlEnd();

    rlSetTexture(surfaces->ceiling.id);
    rlBegin(RL_QUADS);
    for (int i = level->wallEnd; i < level->colliderCount; ++i)
        DrawStoneBox(level->colliders[i], ColorBrightness(floor, -0.15f));
    rlEnd();
    rlSetTexture(0);
}

static void DrawBossModel(const Campaign *campaign, const BossSpec *boss)
{
    Vector3 center = campaign->bossPosition;
    Color phaseColor = ColorLerp(boss->color, ORANGE, campaign->bossPhase * 0.26f);
    Color body = campaign->guardTimer > 0 ? RAYWHITE : phaseColor;
    DrawCylinder(center, boss->radius * 0.76f, boss->radius, boss->height, 12, body);
    DrawCylinderWires(center, boss->radius * 0.76f, boss->radius, boss->height, 12, DARKGRAY);
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

void DrawCampaignWorld(const Campaign *campaign, const CampaignSurfaces *surfaces)
{
    const LevelSpec *level = CurrentLevel(campaign);
    DrawPlane((Vector3){ 0, -13, -19 }, (Vector2){ 130, 130 }, (Color){ 27, 47, 59, 255 });
    DrawCampaignSurfaces(level, surfaces, campaign->camera.position);
    BoundingBox arena = Arena(campaign);
    Vector3 gate = GatePosition(arena);
    Color frame = CurrentBoss(campaign) ? (Color){ 96, 116, 128, 255 } : level->trim;
    DrawCube((Vector3){ gate.x - 0.83f, gate.y + 1.1f, gate.z }, 0.2f, 2.2f, 0.26f, frame);
    DrawCube((Vector3){ gate.x + 0.83f, gate.y + 1.1f, gate.z }, 0.2f, 2.2f, 0.26f, frame);
    DrawCube((Vector3){ gate.x, gate.y + 2.2f, gate.z }, 1.86f, 0.21f, 0.26f, frame);
    if (!CurrentBoss(campaign))
    {
        DrawCube((Vector3){ gate.x, gate.y + 1.05f, gate.z }, 1.45f, 2.08f, 0.055f, Fade(frame, 0.44f));
        DrawCubeWires((Vector3){ gate.x, gate.y + 1.05f, gate.z }, 1.45f, 2.08f, 0.08f, RAYWHITE);
    }
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
}

static Rectangle MenuBounds(void)
{
    return (Rectangle){ GetScreenWidth() - 111.0f, 12, 99, 29 };
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
        const char *labels[] = { "Resume", retry, campaign->debugRun ? "Level select" : "Mazes menu" };
        const char *label = labels[i];
        DrawUiButton(button, label, ReadUiButton(button, pointer, true), true);
    }
}

static void DrawMazeMap(const Campaign *campaign)
{
    const LevelSpec *level = CurrentLevel(campaign);
    if (!level->mazeRows || campaign->checkpoint) return;

    int maxSize = GetScreenHeight() < 480 ? 94 : 144;
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
    int x = GetScreenWidth() - width - 20, y = 106;
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
    const LevelSpec *level = CurrentLevel(campaign);
    int width = GetScreenWidth(), height = GetScreenHeight();
    const Color panel = { 15, 24, 35, 234 };
    DrawUiPanel((Rectangle){ 0, 0, (float)width, 79 }, panel);
    const char *heading = campaign->endlessLevel ? level->name :
        TextFormat("%d / %d   %s", campaign->level + 1, CAMPAIGN_LEVEL_COUNT, level->name);
    DrawText(heading, 16, 12, 20, RAYWHITE);
    if (campaign->endlessLevel)
        DrawText(TextFormat("Lives %d    %d x %d", campaign->lives, level->mazeCols, level->mazeRows),
                 96, 31, 13, level->trim);
    else if (!campaign->debugRun)
        DrawText(TextFormat("Lives %d", campaign->lives), 96, 31, 13, level->trim);
    for (int i = 0; i < 3; ++i)
        DrawRectangle(18 + 23 * i, 51, 17, 10, i < campaign->health ? level->trim : DARKGRAY);
    const BossSpec *boss = CurrentBoss(campaign);
    const char *objective = !campaign->checkpoint ? (level->mazeRows ? "Explore the maze" : "Reach the arena")
        : boss ? TextFormat("Stomp %s  %d/%d  /  Phase %d/%d", boss->name, campaign->bossHealth, boss->health,
                            campaign->bossPhase + 1, boss->phaseCount)
               : "Exit open";
    int objectiveSize = 15;
    while (objectiveSize > 9 && MeasureText(objective, objectiveSize) > width - 103) --objectiveSize;
    DrawText(objective, 96, 48, objectiveSize, RAYWHITE);
    bool touch = HasPlayerTouchControls();
    const char *back = (campaign->cleared || campaign->gameOver) ? (campaign->debugRun ? "Levels" : "Mazes")
                                        : (touch ? "Pause" : "Esc Pause");
    DrawUiButton(MenuBounds(), back, ReadUiButton(MenuBounds(), pointer, true), true);

    if (campaign->cleared || campaign->gameOver)
    {
        DrawRectangle(0, 79, width, height - 79, (Color){ 8, 15, 24, 225 });
        const char *message = campaign->gameOver ? "Game over" : (campaign->debugRun ? "Level clear" : "Mazes complete");
        DrawText(message, (width - MeasureText(message, 33)) / 2, height / 2 - 30, 33, level->trim);
        const char *prompt = campaign->debugRun ? "Return to level select" : "All nine bosses defeated";
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
    if (campaign->paused)
    {
        DrawCampaignPause(campaign, pointer);
        return;
    }
    if (campaign->phaseNotice > 0)
    {
        const char *label = TextFormat("Phase %d", campaign->bossPhase + 1);
        DrawText(label, (width - MeasureText(label, 24)) / 2, 105, 24, GOLD);
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

int ReadDebugLevelSelect(InputPointer pointer)
{
    for (int i = 0; i <= CAMPAIGN_LEVEL_COUNT; ++i)
    {
        if (ReadUiButton(DebugButton(i), pointer, true).activated ||
            (i < CAMPAIGN_LEVEL_COUNT && IsKeyPressed(KEY_ONE + i)))
            return i;
    }
    return -1;
}

void DrawDebugLevelSelect(Vector2 position)
{
    int width = GetScreenWidth(), height = GetScreenHeight();
    DrawRectangleGradientV(0, 0, width, height, (Color){ 12, 26, 41, 255 }, (Color){ 42, 67, 67, 255 });
    const char *title = "Debug Level Select";
    DrawText(title, (width - MeasureText(title, 25)) / 2, (int)DebugButton(0).y - 44, 25, RAYWHITE);
    InputPointer pointer = { .position = position };
    for (int i = 0; i <= CAMPAIGN_LEVEL_COUNT; ++i)
    {
        Rectangle button = DebugButton(i);
        bool back = i == CAMPAIGN_LEVEL_COUNT;
        const char *label = back ? "Back" : TextFormat("%d   %s", i + 1, campaignLevels[i].name);
        DrawUiButton(button, label, ReadUiButton(button, pointer, true), true);
    }
}
