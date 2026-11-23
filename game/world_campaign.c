#include "world_campaign.h"

#include "raymath.h"
#include "ui.h"
#include "world_levels.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

enum {
    CHUNK_SIDE = TERRAIN_SIDE - 1, CHUNKS_PER_SIDE = WORLD_SIDE / CHUNK_SIDE,
    CHUNK_RADIUS = 5, CACHE_SIDE = 2 * CHUNK_RADIUS + 1,
    TREES_PER_CHUNK = 6, TREE_BOXES = 9 * TREES_PER_CHUNK
};

typedef struct BasinTree {
    Vector3 base;
    float scale;
} BasinTree;

struct WorldChunk {
    TerrainMap map;
    int col, row;
    bool visible;
    int treeCount;
    BasinTree trees[TREES_PER_CHUNK];
};

struct WorldModels {
    Model trees[2];
    Model beacon, column;
};

typedef struct PropPart {
    const Mesh *mesh;
    Vector3 offset, scale;
    Color color;
    float taper;
} PropPart;

// Build an ordinary GPU mesh so props never split across immediate batch flushes.
static Model MakeWorldProp(const PropPart *parts, size_t count)
{
    Mesh mesh = {0};
    for (size_t i = 0; i < count; ++i) mesh.triangleCount += parts[i].mesh->triangleCount;
    mesh.vertexCount = mesh.triangleCount * 3;
    mesh.vertices = MemAlloc((unsigned int)mesh.vertexCount * 3 * sizeof(float));
    mesh.normals = MemAlloc((unsigned int)mesh.vertexCount * 3 * sizeof(float));
    mesh.texcoords = MemAlloc((unsigned int)mesh.vertexCount * 2 * sizeof(float));
    mesh.colors = MemAlloc((unsigned int)mesh.vertexCount * 4);
    if (!mesh.vertices || !mesh.normals || !mesh.texcoords || !mesh.colors)
    {
        UnloadMesh(mesh);
        return (Model){0};
    }

    int out = 0;
    for (size_t i = 0; i < count; ++i)
    {
        const PropPart *part = &parts[i];
        const Mesh *source = part->mesh;
        for (int vertex = 0; vertex < source->triangleCount * 3; ++vertex, ++out)
        {
            int input = source->indices ? source->indices[vertex] : vertex;
            const float *point = &source->vertices[input * 3];
            float width = 1.0f - part->taper * point[1];
            mesh.vertices[out * 3] = part->offset.x + part->scale.x * point[0] * width;
            mesh.vertices[out * 3 + 1] = part->offset.y + part->scale.y * point[1];
            mesh.vertices[out * 3 + 2] = part->offset.z + part->scale.z * point[2] * width;
            memcpy(&mesh.normals[out * 3], &source->normals[input * 3], 3 * sizeof(float));
            memcpy(&mesh.texcoords[out * 2], &source->texcoords[input * 2], 2 * sizeof(float));
            mesh.colors[out * 4] = part->color.r;
            mesh.colors[out * 4 + 1] = part->color.g;
            mesh.colors[out * 4 + 2] = part->color.b;
            mesh.colors[out * 4 + 3] = part->color.a;
        }
    }
    // LoadModelFromMesh uses buffers already uploaded by the caller.
    UploadMesh(&mesh, false);
    if (!mesh.vboId || !mesh.vboId[0] || !mesh.vboId[1] || !mesh.vboId[2] || !mesh.vboId[3])
    {
        UnloadMesh(mesh);
        return (Model){0};
    }
    return LoadModelFromMesh(mesh);
}

static bool LoadWorldModels(WorldModels *models)
{
    Mesh column = GenMeshCylinder(1, 1, 8);
    Mesh sphere = GenMeshSphere(1, 6, 9);
    Mesh distantSphere = GenMeshSphere(1, 4, 6);
    bool ready = column.vertices && sphere.vertices && distantSphere.vertices;
    if (ready)
    {
        PropPart tree[] = {
            { &column, {0, -0.30f, 0}, {0.44f, 4.5f, 0.44f}, {103, 73, 49, 255}, 0.52f },
            { &sphere, {0, 4.35f, 0}, {2.05f, 2.05f, 2.05f}, {72, 123, 56, 255}, 0 },
            { &sphere, {1.2f, 3.8f, 0.55f}, {1.44f, 1.44f, 1.44f}, {91, 145, 62, 255}, 0 },
            { &sphere, {-1.35f, 3.73f, -0.45f}, {1.48f, 1.48f, 1.48f}, {54, 100, 49, 255}, 0 },
        };
        models->trees[0] = MakeWorldProp(tree, 4);
        tree[1].mesh = &distantSphere;
        models->trees[1] = MakeWorldProp(tree, 2);

        PropPart beacon[] = {
            { &column, {0, -0.12f, 0}, {1.65f, 0.47f, 1.65f}, GOLD, 0.30f },
            { &column, {0, 0.35f, 0}, {0.42f, 5.0f, 0.42f}, GOLD, 0.31f },
            { &sphere, {0, 5.5f, 0}, {0.78f, 0.78f, 0.78f}, GOLD, 0 },
        };
        models->beacon = MakeWorldProp(beacon, 3);
        PropPart post = { &column, {0, 0, 0}, {1, 1, 1}, WHITE, 0 };
        models->column = MakeWorldProp(&post, 1);
        ready = IsModelValid(models->trees[0]) && IsModelValid(models->trees[1]) &&
                IsModelValid(models->beacon) && IsModelValid(models->column);
    }
    UnloadMesh(column);
    UnloadMesh(sphere);
    UnloadMesh(distantSphere);
    return ready;
}

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

static float TreeVariation(uint32_t seed)
{
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    seed *= 0x846ca68bu;
    seed ^= seed >> 16;
    return (seed & 0xffffu) / 65535.0f;
}

static bool BasinTreeAt(const WorldSpec *world, int col, int row, int slot, BasinTree *tree)
{
    uint32_t seed = (uint32_t)((row * CHUNKS_PER_SIDE + col) * TREES_PER_CHUNK + slot) * 17u + 1u;
    float x = (col + (slot % 3 + 0.16f + 0.68f * TreeVariation(seed)) / 3.0f) * CHUNK_SIDE;
    float z = (row + (slot / 3 + 0.16f + 0.68f * TreeVariation(seed + 1u)) / 2.0f) * CHUNK_SIDE;
    if (TreeVariation(seed + 2u) < 0.18f) return false;
    Vector2 point = { x, z };
    Vector2 from = world->spawn;
    for (int i = 0; i <= WAYPOINT_COUNT; ++i)
    {
        Vector2 to = i == WAYPOINT_COUNT ? world->gate : world->waypoints[i];
        Vector2 leg = Vector2Subtract(to, from);
        float along = Clamp(Vector2DotProduct(Vector2Subtract(point, from), leg) /
                            fmaxf(Vector2LengthSqr(leg), 1.0f), 0, 1);
        Vector2 nearest = Vector2Add(from, Vector2Scale(leg, along));
        if (Vector2DistanceSqr(point, nearest) < 7.0f * 7.0f) return false;
        from = to;
    }
    *tree = (BasinTree){ { x, 0, z }, 0.80f + 0.50f * TreeVariation(seed + 3u) };
    return true;
}

static PlayerWorld WorldPhysics(const WorldCampaign *campaign, Vector2 position, BoundingBox *boxes)
{
    const WorldSpec *world = &worlds[campaign->world];
    PlayerWorld physics = { .groundHeight = GroundHeight, .groundNormal = GroundNormal,
                            .ground = world, .boxes = boxes };
    if (campaign->world != 0) return physics;

    // Check neighboring chunks regardless of the render camera.
    int col = (int)(position.x / CHUNK_SIDE), row = (int)(position.y / CHUNK_SIDE);
    for (int z = row - 1; z <= row + 1; ++z)
        for (int x = col - 1; x <= col + 1; ++x)
        {
            if (x < 0 || z < 0 || x >= CHUNKS_PER_SIDE || z >= CHUNKS_PER_SIDE) continue;
            for (int slot = 0; slot < TREES_PER_CHUNK; ++slot)
            {
                BasinTree tree;
                if (!BasinTreeAt(world, x, z, slot, &tree) ||
                    Vector2DistanceSqr(position, (Vector2){ tree.base.x, tree.base.z }) > 9) continue;
                tree.base.y = GroundHeight(world, tree.base.x, tree.base.z);
                float half = 0.34f * tree.scale;
                boxes[physics.boxCount++] = (BoundingBox){
                    { tree.base.x - half, tree.base.y - 0.30f, tree.base.z - half },
                    { tree.base.x + half, tree.base.y + 4.2f * tree.scale, tree.base.z + half },
                };
            }
        }
    return physics;
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
    BoundingBox boxes[TREE_BOXES];
    PlayerWorld physics = WorldPhysics(campaign, spawn, boxes);
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
    float extent = world->relief + (world->brushCount ? WORLD_BRUSH_LIMIT : 0);
    if (campaign->world == 0) extent += 10.0f;
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
                !ChunkInView(view, col, row, extent)) continue;
            WorldChunk *chunk = &campaign->chunks[(row % CACHE_SIDE) * CACHE_SIDE + col % CACHE_SIDE];
            if (!chunk->map.enabled)
            {
                Vector2 origin = { (float)(col * CHUNK_SIDE), (float)(row * CHUNK_SIDE) };
                if (!InitTerrainChunk(&chunk->map, origin, 1, world->low, world->high, world->relief, WorldHeight, world))
                    continue;
                chunk->col = col;
                chunk->row = row;
                chunk->treeCount = 0;
                if (campaign->world == 0)
                    for (int slot = 0; slot < TREES_PER_CHUNK; ++slot)
                    {
                        BasinTree tree;
                        if (!BasinTreeAt(world, col, row, slot, &tree)) continue;
                        tree.base.y = GroundHeight(world, tree.base.x, tree.base.z);
                        chunk->trees[chunk->treeCount++] = tree;
                    }
            }
            chunk->visible = true;
        }
}

bool OpenWorldCampaign(WorldCampaign *campaign, int level)
{
    if (level < -1 || level >= WORLD_COUNT) return false;
    *campaign = (WorldCampaign){ .world = level < 0 ? 0 : level, .singleWorld = level >= 0 };
    campaign->chunks = calloc(CACHE_SIDE * CACHE_SIDE, sizeof(*campaign->chunks));
    campaign->models = calloc(1, sizeof(*campaign->models));
    if (!campaign->chunks || !campaign->models || !LoadWorldModels(campaign->models))
    {
        CloseWorldCampaign(campaign);
        return false;
    }
    RespawnWorld(campaign);
    UpdateChunks(campaign);
    return true;
}

void CloseWorldCampaign(WorldCampaign *campaign)
{
    ClearChunks(campaign);
    free(campaign->chunks);
    if (campaign->models)
    {
        const WorldModels *models = campaign->models;
        for (int i = 0; i < 2; ++i) UnloadModel(models->trees[i]);
        UnloadModel(models->beacon);
        UnloadModel(models->column);
        free(campaign->models);
    }
    *campaign = (WorldCampaign){0};
}

static Rectangle WorldButton(int index)
{
    Vector2 inset = GameUiInset();
    if (index == 0) return (Rectangle){ GetScreenWidth() - 110 - inset.x, 12 + inset.y, 98, 32 };
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
    BoundingBox boxes[TREE_BOXES];
    Vector2 feet = { campaign->player.feet.x, campaign->player.feet.z };
    PlayerWorld physics = WorldPhysics(campaign, feet, boxes);
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

static void DrawWorldColumn(Model model, Vector3 start, Vector3 end, float radius, Color tint)
{
    Vector3 delta = Vector3Subtract(end, start);
    Vector3 up = { 0, 1, 0 };
    float length = Vector3Length(delta);
    if (length < 0.001f) return;
    Vector3 direction = Vector3Scale(delta, 1 / length);
    Vector3 axis = Vector3CrossProduct(up, direction);
    float angle = acosf(Clamp(direction.y, -1, 1)) * RAD2DEG;
    if (Vector3LengthSqr(axis) < 0.00001f) axis = (Vector3){1, 0, 0};
    DrawModelEx(model, start, axis, angle, (Vector3){ radius, length, radius }, tint);
}

void DrawWorldLandscape(const WorldCampaign *campaign, const TerrainLayers *layers)
{
    const WorldSpec *world = &worlds[campaign->world];
    SetTerrainLayerElevation(layers, 0, world->relief);
    for (int i = 0; i < CACHE_SIDE * CACHE_SIDE; ++i)
        if (campaign->chunks[i].visible) DrawLayeredTerrainMap(&campaign->chunks[i].map, layers);

    for (int i = 0; i < CACHE_SIDE * CACHE_SIDE; ++i)
    {
        const WorldChunk *chunk = &campaign->chunks[i];
        if (!chunk->visible) continue;
        for (int i = 0; i < chunk->treeCount; ++i)
        {
            BasinTree tree = chunk->trees[i];
            bool distant = Vector3DistanceSqr(campaign->camera.position, tree.base) >= 55.0f * 55.0f;
            DrawModel(campaign->models->trees[distant], tree.base, tree.scale, WHITE);
        }
    }

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
        Model column = campaign->models->column;
        DrawWorldColumn(column, left, leftTop, 0.60f, world->high);
        DrawWorldColumn(column, right, rightTop, 0.60f, world->high);
        DrawWorldColumn(column, leftTop, rightTop, 0.53f, SKYBLUE);
        return;
    }

    DrawModel(campaign->models->beacon, base, 1, WHITE);
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

    Vector2 inset = GameUiInset();
    int left = (int)inset.x + 14, top = (int)inset.y;
    Rectangle pause = WorldButton(0);
    DrawUiPanel((Rectangle){ 0, 0, (float)width, 80 + inset.y }, panel);
    DrawUiLabel(TextFormat("%d/%d  %s", campaign->world + 1, WORLD_COUNT, worlds[campaign->world].name),
                left, 12 + top, 19, pause.x - left - 8, RAYWHITE);
    Vector2 target = Destination(campaign);
    Vector2 delta = Vector2Subtract(target, (Vector2){ campaign->player.feet.x, campaign->player.feet.z });
    float bearing = atan2f(delta.x, delta.y) - campaign->view.yaw;
    float relative = atan2f(sinf(bearing), cosf(bearing));
    const char *turn = fabsf(relative) < 0.20f ? "ahead" : relative > 0 ? "left" : "right";
    const char *objective = campaign->waypoint < WAYPOINT_COUNT ?
        TextFormat("Beacon %d/%d", campaign->waypoint + 1, WAYPOINT_COUNT) : "World gate";
    DrawUiLabel(TextFormat("%s  /  %.0f m  /  %s", objective, Vector2Length(delta), turn),
                left, 47 + top, 15, width - 2 * left, GOLD);
    bool touch = HasPlayerTouchControls();
    DrawUiButton(pause, touch ? "Pause" : "Esc Pause", ReadUiButton(pause, pointer, touch), true);
    if (touch) DrawPlayerTouch(&campaign->touch);
    else DrawText("WASD move   Space jump   Shift sprint   R checkpoint", 14, height - 25, 14, RAYWHITE);
}
