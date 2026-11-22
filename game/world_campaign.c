#include "world_campaign.h"

#include "raymath.h"
#include "ui.h"
#include "world_levels.h"

#include <stdlib.h>

enum {
    CHUNK_SIDE = TERRAIN_SIDE - 1, CHUNKS_PER_SIDE = WORLD_SIDE / CHUNK_SIDE,
    CHUNK_RADIUS = 5, CACHE_SIDE = 2 * CHUNK_RADIUS + 1
};

struct WorldChunk {
    TerrainMap map;
    int col, row;
    bool visible;
};

static float GroundHeight(const void *data, float x, float z)
{
    return SampleTerrainGrid(WorldHeight, data, (Vector2){0}, 1, x, z, NULL);
}

static Vector3 GroundNormal(const void *data, float x, float z)
{
    Vector3 normal;
    SampleTerrainGrid(WorldHeight, data, (Vector2){0}, 1, x, z, &normal);
    return normal;
}

static PlayerWorld WorldPhysics(const WorldCampaign *campaign)
{
    return (PlayerWorld){
        .groundHeight = GroundHeight, .groundNormal = GroundNormal,
        .ground = &worlds[campaign->world],
    };
}

static Vector2 Destination(const WorldCampaign *campaign)
{
    const WorldSpec *world = &worlds[campaign->world];
    return campaign->waypoint < WAYPOINT_COUNT ? world->waypoints[campaign->waypoint] : world->gate;
}

static void RespawnWorld(WorldCampaign *campaign)
{
    const WorldSpec *world = &worlds[campaign->world];
    Vector2 spawn = campaign->waypoint ? world->waypoints[campaign->waypoint - 1] : world->spawn;
    Vector2 target = Vector2Subtract(Destination(campaign), spawn);
    PlayerWorld physics = WorldPhysics(campaign);
    campaign->player = SpawnPlayer(spawn, &physics);
    campaign->view = (WalkCamera){ .position = GetPlayerEye(&campaign->player),
                                    .yaw = atan2f(target.x, target.y), .pitch = -0.06f };
    campaign->camera = (Camera3D){ .up = { 0, 1, 0 }, .fovy = 58, .projection = CAMERA_PERSPECTIVE };
    UpdateWalkCamera(&campaign->camera, &campaign->view, Vector2Zero(), Vector2Zero(), 0);
    ResetPlayerTouch(&campaign->touch);
}

static void ClearChunks(WorldCampaign *campaign)
{
    for (int i = 0; campaign->chunks && i < CACHE_SIDE * CACHE_SIDE; ++i)
        if (campaign->chunks[i].map.enabled) UnloadTerrainMap(&campaign->chunks[i].map);
}

typedef struct TerrainView {
    Vector3 eye, forward, right, up;
    float tanX, tanY, clipX, clipY;
} TerrainView;

static TerrainView LandscapeView(Camera3D camera)
{
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    float tanY = tanf(camera.fovy * DEG2RAD * 0.5f);
    int height = GetRenderHeight();
    float tanX = tanY * GetRenderWidth() / (height > 0 ? height : 1);
    return (TerrainView){
        camera.position, forward, right, Vector3CrossProduct(right, forward),
        tanX, tanY, sqrtf(1 + tanX * tanX), sqrtf(1 + tanY * tanY),
    };
}

static bool ChunkInView(TerrainView view, int col, int row, float relief)
{
    Vector3 center = { (col + 0.5f) * CHUNK_SIDE, 0, (row + 0.5f) * CHUNK_SIDE };
    Vector3 delta = Vector3Subtract(center, view.eye);
    // A sphere encloses the whole chunk, including all possible heights.
    float radius = sqrtf(CHUNK_SIDE * CHUNK_SIDE * 0.5f + relief * relief);
    float depth = Vector3DotProduct(delta, view.forward);
    return depth >= -radius &&
           fabsf(Vector3DotProduct(delta, view.right)) <= depth * view.tanX + radius * view.clipX &&
           fabsf(Vector3DotProduct(delta, view.up)) <= depth * view.tanY + radius * view.clipY;
}

static void UpdateChunks(WorldCampaign *campaign)
{
    const WorldSpec *world = &worlds[campaign->world];
    int centerCol = (int)(campaign->player.feet.x / CHUNK_SIDE);
    int centerRow = (int)(campaign->player.feet.z / CHUNK_SIDE);
    TerrainView view = LandscapeView(campaign->camera);
    for (int i = 0; i < CACHE_SIDE * CACHE_SIDE; ++i)
    {
        WorldChunk *chunk = &campaign->chunks[i];
        chunk->visible = false;
        if (chunk->map.enabled &&
            (abs(chunk->col - centerCol) > CHUNK_RADIUS || abs(chunk->row - centerRow) > CHUNK_RADIUS))
            UnloadTerrainMap(&chunk->map);
    }

    for (int row = centerRow - CHUNK_RADIUS; row <= centerRow + CHUNK_RADIUS; ++row)
        for (int col = centerCol - CHUNK_RADIUS; col <= centerCol + CHUNK_RADIUS; ++col)
        {
            if (col < 0 || row < 0 || col >= CHUNKS_PER_SIDE || row >= CHUNKS_PER_SIDE ||
                !ChunkInView(view, col, row, world->relief + (world->brushCount ? WORLD_BRUSH_LIMIT : 0))) continue;
            WorldChunk *chunk = &campaign->chunks[(row % CACHE_SIDE) * CACHE_SIDE + col % CACHE_SIDE];
            if (!chunk->map.enabled)
            {
                Vector2 origin = { (float)(col * CHUNK_SIDE), (float)(row * CHUNK_SIDE) };
                if (!InitTerrainChunk(&chunk->map, origin, 1, world->low, world->high, world->relief, WorldHeight, world))
                    continue;
                chunk->col = col;
                chunk->row = row;
            }
            chunk->visible = true;
        }
}

bool OpenWorldCampaign(WorldCampaign *campaign, int level)
{
    if (level < -1 || level >= WORLD_COUNT) return false;
    *campaign = (WorldCampaign){ .world = level < 0 ? 0 : level, .singleWorld = level >= 0 };
    campaign->chunks = calloc(CACHE_SIDE * CACHE_SIDE, sizeof(*campaign->chunks));
    if (!campaign->chunks) return false;
    RespawnWorld(campaign);
    UpdateChunks(campaign);
    return true;
}

void CloseWorldCampaign(WorldCampaign *campaign)
{
    ClearChunks(campaign);
    free(campaign->chunks);
    *campaign = (WorldCampaign){0};
}

static Rectangle WorldButton(int index)
{
    if (index == 0) return (Rectangle){ GetScreenWidth() - 110, 12, 98, 32 };
    return (Rectangle){ GetScreenWidth() * 0.5f - 116, GetScreenHeight() * 0.5f + (index - 1) * 47, 232, 39 };
}

static bool WorldClick(int index, InputPointer pointer)
{
    return ReadUiButton(WorldButton(index), pointer, true).activated;
}

bool UpdateWorldCampaign(WorldCampaign *campaign, InputPointer pointer, Vector2 movement,
                         bool jump, bool sprint, bool retry, bool escape, bool enter, float dt)
{
    bool touch = HasPlayerTouchControls();
    if (campaign->complete) return !(escape || enter || WorldClick(1, pointer));
    if (campaign->paused)
    {
        if (WorldClick(2, pointer)) return false;
        if (escape || enter || WorldClick(1, pointer))
        {
            campaign->paused = false;
            if (!touch) DisableCursor();
        }
        return true;
    }
    if (escape || (touch && WorldClick(0, pointer)))
    {
        campaign->paused = true;
        ResetPlayerTouch(&campaign->touch);
        if (!touch) EnableCursor();
        return true;
    }

    PlayerTouchInput mobile = ReadPlayerTouch(&campaign->touch);
    UpdateWalkCamera(&campaign->camera, &campaign->view, touch ? mobile.look : pointer.delta, Vector2Zero(), 0);
    Vector2 forward = { sinf(campaign->view.yaw), cosf(campaign->view.yaw) };
    Vector2 right = { -forward.y, forward.x };
    movement = Vector2Add(movement, mobile.movement);
    PlayerWorld physics = WorldPhysics(campaign);
    StepPlayer(&campaign->player, &physics, (PlayerInput){
        .movement = Vector2Add(Vector2Scale(right, movement.x), Vector2Scale(forward, movement.y)),
        .jumpPressed = jump || mobile.jumpPressed, .sprint = sprint,
    }, Clamp(dt, 0, 0.05f));
    Vector2 position = { campaign->player.feet.x, campaign->player.feet.z };
    if (retry || position.x < 0 || position.y < 0 || position.x >= WORLD_SIDE || position.y >= WORLD_SIDE)
        RespawnWorld(campaign);
    else if (Vector2DistanceSqr(position, Destination(campaign)) <= 3.0f * 3.0f)
    {
        if (campaign->waypoint < WAYPOINT_COUNT) ++campaign->waypoint;
        else if (campaign->singleWorld || campaign->world + 1 == WORLD_COUNT)
        {
            campaign->complete = true;
            if (!touch) EnableCursor();
        }
        else
        {
            ClearChunks(campaign);
            ++campaign->world;
            campaign->waypoint = 0;
            RespawnWorld(campaign);
        }
    }
    campaign->view.position = GetPlayerEye(&campaign->player);
    UpdateWalkCamera(&campaign->camera, &campaign->view, Vector2Zero(), Vector2Zero(), 0);
    UpdateChunks(campaign);
    return true;
}

void DrawWorldLandscape(const WorldCampaign *campaign, const TerrainLayers *layers)
{
    const WorldSpec *world = &worlds[campaign->world];
    SetTerrainLayerElevation(layers, 0, world->relief);
    for (int i = 0; i < CACHE_SIDE * CACHE_SIDE; ++i)
        if (campaign->chunks[i].visible) DrawLayeredTerrainMap(&campaign->chunks[i].map, layers);

    Vector2 target = Destination(campaign);
    Vector2 eye = { campaign->camera.position.x, campaign->camera.position.z };
    if (Vector2DistanceSqr(eye, target) > 145.0f * 145.0f) return;
    Vector3 base = { target.x, GroundHeight(world, target.x, target.y), target.y };
    if (campaign->waypoint == WAYPOINT_COUNT)
    {
        Vector2 heading = Vector2Normalize(Vector2Subtract(target, world->waypoints[WAYPOINT_COUNT - 1]));
        Vector3 side = { -heading.y, 0, heading.x };
        Vector3 left = Vector3Add(base, Vector3Scale(side, 2));
        Vector3 right = Vector3Subtract(base, Vector3Scale(side, 2));
        Vector3 leftTop = Vector3Add(left, (Vector3){ 0, 5.0f, 0 });
        Vector3 rightTop = Vector3Add(right, (Vector3){ 0, 5.0f, 0 });
        left.y = GroundHeight(world, left.x, left.z) - 0.15f;
        right.y = GroundHeight(world, right.x, right.z) - 0.15f;
        DrawCylinderEx(left, leftTop, 0.60f, 0.48f, 8, world->high);
        DrawCylinderEx(right, rightTop, 0.60f, 0.48f, 8, world->high);
        DrawCylinderEx(leftTop, rightTop, 0.53f, 0.53f, 8, SKYBLUE);
        return;
    }

    DrawCylinder(base, 1.15f, 1.65f, 0.35f, 12, GOLD);
    Vector3 mast = { base.x, base.y + 0.35f, base.z };
    DrawCylinder(mast, 0.29f, 0.42f, 5.0f, 8, GOLD);
    DrawSphere((Vector3){ base.x, base.y + 5.5f, base.z }, 0.78f, GOLD);
}

void DrawWorldCampaignHud(const WorldCampaign *campaign, InputPointer pointer)
{
    int width = GetScreenWidth(), height = GetScreenHeight();
    Color panel = { 13, 24, 34, 240 };
    if (campaign->paused || campaign->complete)
    {
        const char *heading = !campaign->complete ? "Paused" :
                              campaign->singleWorld ? "World complete" : "Campaign complete";
        DrawRectangle(0, 0, width, height, (Color){ 8, 15, 23, 215 });
        DrawText(heading, (width - MeasureText(heading, 28)) / 2, height / 2 - 56, 28, RAYWHITE);
        Rectangle first = WorldButton(1);
        DrawUiButton(first, campaign->complete ? "Campaign menu" : "Resume", ReadUiButton(first, pointer, true), true);
        if (campaign->paused)
        {
            Rectangle second = WorldButton(2);
            DrawUiButton(second, "Campaign menu", ReadUiButton(second, pointer, true), true);
        }
        return;
    }

    DrawUiPanel((Rectangle){ 0, 0, (float)width, 80 }, panel);
    DrawText(TextFormat("%d/%d  %s", campaign->world + 1, WORLD_COUNT, worlds[campaign->world].name),
             14, 12, 19, RAYWHITE);
    Vector2 target = Destination(campaign);
    Vector2 delta = Vector2Subtract(target, (Vector2){ campaign->player.feet.x, campaign->player.feet.z });
    float bearing = atan2f(delta.x, delta.y) - campaign->view.yaw;
    float relative = atan2f(sinf(bearing), cosf(bearing));
    const char *turn = fabsf(relative) < 0.20f ? "ahead" : relative > 0 ? "left" : "right";
    const char *objective = campaign->waypoint < WAYPOINT_COUNT ?
        TextFormat("Beacon %d/%d", campaign->waypoint + 1, WAYPOINT_COUNT) : "World gate";
    DrawText(TextFormat("%s  /  %.0f m  /  %s", objective, Vector2Length(delta), turn), 14, 47, 15, GOLD);
    Rectangle pause = WorldButton(0);
    bool touch = HasPlayerTouchControls();
    DrawUiButton(pause, touch ? "Pause" : "Esc Pause", ReadUiButton(pause, pointer, touch), true);
    if (touch) DrawPlayerTouch(&campaign->touch);
    else DrawText("WASD move   Space jump   Shift sprint   R checkpoint", 14, height - 25, 14, RAYWHITE);
}
