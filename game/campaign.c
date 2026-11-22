#include "campaign.h"
#include "campaign_content.h"
#include "raymath.h"
#include "ui.h"

#include <math.h>
#include <string.h>

static const LevelSpec *CurrentLevel(const Campaign *campaign)
{
    return &campaignLevels[campaign->level];
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

static int MazeCellAt(const LevelSpec *level, Vector3 position)
{
    if (level->mazeRows == 0) return -1;
    int row = (int)floorf(position.z / CAMPAIGN_MAZE_CELL + level->mazeRows);
    int col = (int)floorf(position.x / CAMPAIGN_MAZE_CELL + level->mazeCols * 0.5f);
    if (row < 0 || row >= level->mazeRows || col < 0 || col >= level->mazeCols) return -1;
    return row * level->mazeCols + col;
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

static PlayerWorld CampaignWorld(const Campaign *campaign)
{
    const LevelSpec *level = CurrentLevel(campaign);
    return (PlayerWorld){
        .groundHeight = VoidHeight, .boxes = level->colliders, .boxCount = (size_t)level->colliderCount,
    };
}

static void Respawn(Campaign *campaign)
{
    const LevelSpec *level = CurrentLevel(campaign);
    BoundingBox arena = Arena(campaign);
    Vector2 spawn = campaign->checkpoint
        ? (Vector2){ (arena.min.x + arena.max.x) * 0.5f, arena.max.z - 0.9f }
        : level->start;
    PlayerWorld world = CampaignWorld(campaign);
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

void StartCampaign(Campaign *campaign, int level, bool debugRun)
{
    if (level < 0 || level >= CAMPAIGN_LEVEL_COUNT) level = 0;
    *campaign = (Campaign){
        .level = level, .debugRun = debugRun,
        .camera = { .up = { 0, 1, 0 }, .fovy = 48, .projection = CAMERA_PERSPECTIVE },
    };
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

static void DamagePlayer(Campaign *campaign, Vector3 origin)
{
    if (campaign->hitTimer > 0) return;
    if (--campaign->health == 0)
    {
        Respawn(campaign);
        return;
    }
    campaign->hitTimer = 0.95f;
    Vector3 away = Vector3Subtract(campaign->player.feet, origin);
    Vector2 push = Vector2Scale(Vector2Normalize((Vector2){ away.x, away.z }), 3.8f);
    campaign->player.velocity = (Vector3){ push.x, 4.1f, push.y };
    campaign->player.grounded = false;
    campaign->player.groundGrace = campaign->player.jumpBuffer = 0;
}

static void UpdateEncounter(Campaign *campaign, Vector3 oldFeet, float dt)
{
    const BossSpec *boss = CurrentBoss(campaign);
    if (!boss || !campaign->checkpoint) return;
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
                    return;
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
            int previousHealth = campaign->health;
            DamagePlayer(campaign, center);
            if (previousHealth == 1 && campaign->health == 3) return;
        }
    }

    for (int i = 0; i < CAMPAIGN_MAX_ORBS; ++i)
    {
        CampaignOrb *orb = &campaign->orbs[i];
        if (orb->life <= 0) continue;
        orb->life -= dt;
        orb->position = Vector3Add(orb->position, Vector3Scale(orb->velocity, dt));
        float x = orb->position.x - campaign->player.feet.x;
        float z = orb->position.z - campaign->player.feet.z;
        float radiusHit = campaign->player.radius + 0.18f;
        if (x*x + z*z <= radiusHit*radiusHit && orb->position.y + 0.18f >= campaign->player.feet.y &&
            orb->position.y - 0.18f <= campaign->player.feet.y + campaign->player.height)
        {
            orb->life = 0;
            int previousHealth = campaign->health;
            DamagePlayer(campaign, orb->position);
            if (previousHealth == 1 && campaign->health == 3) return;
        }
    }
}

void StepCampaign(Campaign *campaign, Vector2 movement, Vector2 look, bool jump,
                  bool sprint, bool retry, float dt)
{
    if (campaign->cleared) return;
    if (retry)
    {
        Respawn(campaign);
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
    PlayerWorld world = CampaignWorld(campaign);
    StepPlayer(&campaign->player, &world, controls, dt);
    if (campaign->player.feet.y < -12.0f)
    {
        Respawn(campaign);
        return;
    }

    BoundingBox arena = Arena(campaign);
    TrackMazePosition(campaign);
    if (OnPlatform(campaign->player.feet, arena)) campaign->checkpoint = true;
    int health = campaign->health;
    UpdateEncounter(campaign, oldFeet, dt);
    if (health == 1 && campaign->health == 3) return;

    if (!CurrentBoss(campaign))
    {
        Vector3 portal = GatePosition(arena);
        if (Vector3DistanceSqr(campaign->player.feet, portal) < 1.15f * 1.15f)
        {
            if (campaign->debugRun || campaign->level == CAMPAIGN_LEVEL_COUNT - 1)
                campaign->cleared = true;
            else
                StartCampaign(campaign, campaign->level + 1, false);
            return;
        }
    }
    campaign->view.position = campaign->camera.position = GetPlayerEye(&campaign->player);
    campaign->camera.target = Vector3Add(campaign->camera.position, direction);
}

static void DrawPlatforms(const LevelSpec *level)
{
    for (int i = 0; i < level->platformCount; ++i)
    {
        BoundingBox box = level->platforms[i];
        Vector3 size = Vector3Subtract(box.max, box.min);
        Vector3 center = Vector3Scale(Vector3Add(box.min, box.max), 0.5f);
        DrawCubeV(center, size, level->stone);
        Color surface = ColorLerp(level->stone, level->trim, 0.19f);
        float top = box.max.y + 0.002f;
        Vector3 northwest = { box.min.x, top, box.min.z };
        Vector3 southwest = { box.min.x, top, box.max.z };
        Vector3 northeast = { box.max.x, top, box.min.z };
        Vector3 southeast = { box.max.x, top, box.max.z };
        DrawTriangle3D(northwest, southwest, northeast, surface);
        DrawTriangle3D(northeast, southwest, southeast, surface);
        DrawCubeWiresV(center, size, Fade(level->trim, 0.65f));
        float y = box.max.y + 0.012f;
        Color seam = ColorLerp(surface, level->stone, 0.68f);
        for (float z = box.min.z + 1.0f; z < box.max.z; z += 1.0f)
            DrawLine3D((Vector3){ box.min.x, y, z }, (Vector3){ box.max.x, y, z }, seam);
        for (float x = box.min.x + 1.0f; x < box.max.x; x += 1.0f)
            DrawLine3D((Vector3){ x, y, box.min.z }, (Vector3){ x, y, box.max.z }, seam);
        float z = box.min.z + 0.23f;
        DrawCube((Vector3){ center.x, y, z }, size.x - 0.35f, 0.012f, 0.075f, level->trim);
        for (int tile = -1; tile <= 1; ++tile)
            DrawCube((Vector3){ center.x + tile * 0.3f, y, center.z - 0.38f }, 0.13f, 0.016f, 0.13f,
                     Fade(level->trim, 0.82f));
    }
}

static void DrawMazeWalls(const LevelSpec *level)
{
    if (level->mazeRows == 0) return;
    const Color wall = ColorLerp(level->stone, (Color){ 15, 24, 35, 255 }, 0.34f);
    const Color cap = ColorLerp(level->stone, level->trim, 0.32f);
    for (int i = level->platformCount; i < level->colliderCount; ++i)
    {
        BoundingBox box = level->colliders[i];
        Vector3 center = Vector3Scale(Vector3Add(box.min, box.max), 0.5f);
        Vector3 size = Vector3Subtract(box.max, box.min);
        DrawCubeV(center, size, wall);
        center.y = box.max.y - 0.075f;
        DrawCube(center, size.x, 0.15f, size.z, cap);
        center.y = box.min.y + 0.22f;
        DrawCube(center, size.x + 0.007f, 0.055f, size.z + 0.007f, cap);
    }
    Vector2 exit = CampaignMazeCellCenter(level, 0, level->mazeExit);
    DrawCube((Vector3){ exit.x, 3.25f, exit.y }, CAMPAIGN_MAZE_CELL, 0.23f, 0.32f, level->trim);
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

void DrawCampaignWorld(const Campaign *campaign)
{
    const LevelSpec *level = CurrentLevel(campaign);
    DrawPlane((Vector3){ 0, -13, -19 }, (Vector2){ 130, 130 }, (Color){ 27, 47, 59, 255 });
    DrawPlatforms(level);
    DrawMazeWalls(level);
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

static void DrawMazeMap(const Campaign *campaign)
{
    const LevelSpec *level = CurrentLevel(campaign);
    if (!level->mazeRows || campaign->checkpoint) return;

    int maxSize = GetScreenHeight() < 480 ? 94 : 144;
    int unit = maxSize / (level->mazeRows > level->mazeCols ? level->mazeRows : level->mazeCols);
    int width = level->mazeCols * unit, height = level->mazeRows * unit;
    int x = GetScreenWidth() - width - 20, y = 106;
    DrawUiPanel((Rectangle){ (float)x - 6, (float)y - 20, (float)width + 12, (float)height + 26 },
                (Color){ 12, 20, 29, 223 });
    DrawText("N  /  EXIT", x, y - 16, 11, RAYWHITE);
    for (int i = 0; i < level->mazeRows * level->mazeCols; ++i)
    {
        if (!campaign->mazeVisited[i] && i != level->mazeExit) continue;
        DrawRectangle(x + (i % level->mazeCols) * unit, y + (i / level->mazeCols) * unit, unit, unit,
                      i == level->mazeExit ? level->trim : (Color){ 105, 132, 135, 255 });
    }
    int cell = MazeCellAt(level, campaign->player.feet);
    if (cell >= 0)
        DrawRectangle(x + (cell % level->mazeCols) * unit, y + (cell / level->mazeCols) * unit,
                      unit, unit, RAYWHITE);
}

void DrawCampaignHud(const Campaign *campaign, InputPointer pointer)
{
    const LevelSpec *level = CurrentLevel(campaign);
    int width = GetScreenWidth(), height = GetScreenHeight();
    const Color panel = { 15, 24, 35, 234 };
    DrawUiPanel((Rectangle){ 0, 0, (float)width, 79 }, panel);
    DrawText(TextFormat("%d / %d   %s", campaign->level + 1, CAMPAIGN_LEVEL_COUNT, level->name),
             16, 12, 20, RAYWHITE);
    for (int i = 0; i < 3; ++i)
        DrawRectangle(18 + 23 * i, 51, 17, 10, i < campaign->health ? level->trim : DARKGRAY);
    const BossSpec *boss = CurrentBoss(campaign);
    const char *objective = !campaign->checkpoint ? (level->mazeRows ? "Find the maze exit" : "Reach the arena")
        : boss ? TextFormat("Stomp %s  %d/%d  /  Phase %d/%d", boss->name, campaign->bossHealth, boss->health,
                            campaign->bossPhase + 1, boss->phaseCount)
               : "Exit open";
    int objectiveSize = 15;
    while (objectiveSize > 9 && MeasureText(objective, objectiveSize) > width - 103) --objectiveSize;
    DrawText(objective, 96, 48, objectiveSize, RAYWHITE);
    bool touch = HasPlayerTouchControls();
    const char *back = campaign->debugRun ? (touch ? "Levels" : "Esc Levels")
                                        : (touch ? "Menu" : "Esc Menu");
    DrawUiButton(MenuBounds(), back, ReadUiButton(MenuBounds(), pointer, true), true);

    if (campaign->cleared)
    {
        DrawRectangle(0, 79, width, height - 79, (Color){ 8, 15, 24, 225 });
        const char *message = campaign->debugRun ? "Level clear" : "Campaign complete";
        DrawText(message, (width - MeasureText(message, 33)) / 2, height / 2 - 30, 33, level->trim);
        const char *prompt = campaign->debugRun ? "Return to level select" : "All nine bosses defeated";
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
