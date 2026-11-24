#include "game.h"

#if defined(BENNO_IOS)
#include <SDL_main.h>
#include "ios_audio.h"
#endif
#include "audio.h"
#include "camera.h"
#include "campaign.h"
#include "campaign_content.h"
#include "dynamic_buffer.h"
#include "input.h"
#include "game_logic.h"
#include "physics.h"
#include "player.h"
#include "player_touch.h"
#include "prefab.h"
#include "ui.h"
#include "title_screen.h"
#include "window.h"
#include "world_campaign.h"
#include "world_editor.h"
#include "scene.h"
#include "scene_editor.h"
#include "scene_music.h"
#include "render.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum GameAction {
    ACTION_SPACE, ACTION_SKY, ACTION_GRID, ACTION_EXIT, ACTION_WALK,
    ACTION_RESET, ACTION_RIGHT, ACTION_LEFT, ACTION_FORWARD, ACTION_BACK,
    ACTION_DROP, ACTION_OCCLUSION, ACTION_MUTE, ACTION_FULLSCREEN, ACTION_START, ACTION_SPRINT, ACTION_COUNT
};

static const int gameKeys[ACTION_COUNT] = {
    [ACTION_SPACE] = KEY_SPACE, [ACTION_SKY] = KEY_B, [ACTION_GRID] = KEY_G,
    [ACTION_EXIT] = KEY_ESCAPE, [ACTION_WALK] = KEY_F, [ACTION_RESET] = KEY_R,
    [ACTION_RIGHT] = KEY_D, [ACTION_LEFT] = KEY_A, [ACTION_FORWARD] = KEY_W,
    [ACTION_BACK] = KEY_S, [ACTION_DROP] = KEY_P, [ACTION_OCCLUSION] = KEY_C,
    [ACTION_MUTE] = KEY_M, [ACTION_FULLSCREEN] = KEY_F11, [ACTION_START] = KEY_ENTER,
    [ACTION_SPRINT] = KEY_LEFT_SHIFT,
};

typedef struct BallDrawData {
    const DynamicVertexBuffer *marker;
    Material markerMaterial;
} BallDrawData;

static void DrawSceneModel(const void *context, Vector3 position, float scale, float yaw, size_t lod)
{
    const Model *model = context;
    (void)lod;
    DrawModelEx(*model, position, (Vector3){ 0, 1, 0 }, yaw, (Vector3){ scale, scale, scale }, WHITE);
}

static void DrawSceneBall(const void *context, Vector3 position, float radius, size_t lod)
{
    const BallDrawData *data = context;
    static const int rings[] = { 12, 8, 4 };
    static const int slices[] = { 16, 10, 6 };
    DrawDynamicTriangles(data->marker, data->markerMaterial, MatrixIdentity());
    DrawSphereEx(position, radius, rings[lod], slices[lod], ORANGE);
}

static void DrawEditorSphere(const void *context, Vector3 position, float radius, size_t lod)
{
    (void)context;
    (void)lod;
    DrawSphereEx(position, radius, 12, 16, SKYBLUE);
}

static float TerrainGround(const void *surface, float x, float z)
{
    return GetTerrainHeight(surface, x, z);
}

static Vector3 TerrainSlope(const void *surface, float x, float z)
{
    return GetTerrainNormal(surface, x, z);
}

static Vector2 UpdateBallMarker(DynamicVertexBuffer *buffer, PhysicsSphere ball, const TerrainMap *terrain)
{
    float x = ball.position.x;
    float z = ball.position.z;
    float radius = ball.radius;
    float vertices[] = {
        x - radius, 0, z,  x, 0, z + radius,  x + radius, 0, z,
        x - radius, 0, z,  x + radius, 0, z,  x, 0, z - radius,
    };
    Vector2 limits = { INFINITY, -INFINITY };
    for (int i = 0; i < 18; i += 3)
    {
        float height = GetTerrainHeight(terrain, vertices[i], vertices[i + 2]) + 0.012f;
        vertices[i + 1] = height;
        limits.x = fminf(limits.x, height);
        limits.y = fmaxf(limits.y, height);
    }
    UploadDynamicTriangles(buffer, vertices, sizeof(vertices) / (3 * sizeof(vertices[0])));
    return limits;
}

typedef struct StarterGame {
    const char *modelPath;
    const char *texturePath;
    const char *skyPath;
    Model model;
    Texture2D override;
    Texture2D *originals;
    Model sky;
    Shader shaded;
    Shader skyShader;
    DynamicVertexBuffer ballMarker;
    Material markerMaterial;
    float floorHeight;
    Camera3D camera;
    float yaw, pitch, distance;
    bool custom, showSky, grid, walking, pointerCaptured, occlusionEnabled;
    WalkCamera walk;
    PlayerController player;
    PlayerTouch playerTouch;
    bool playerSpawned;
    PhysicsSphere ball;
    BallDrawData ballDraw;
    ScenePrefab modelPrefab, ballPrefab, spherePrefab;
    PrefabInstance ballInstance, titleModel;
    bool titleOpen, editorOpen, campaignOpen, debugOpen, mazesOpen, worldOpen, worldPickerOpen, worldEditorOpen;
    Campaign campaign;
    WorldCampaign worlds;
    WorldEditor worldEditor;
    LevelSpec endlessLevel;
    CampaignSurfaces campaignSurfaces;
    TerrainLayers terrainLayers;
    SceneEditor editor;
    float titleTime;
    Vector2 titlePointer;
    SceneObject ballObject;
    Vector2 ballMarkerHeight;
    Rectangle dropBounds, modeBounds, menuBounds;
    UiButtonState drop, modeButton, menuButton;
    SceneMusic music;
    Sound ballSound;
    AudioVoice dropVoice, bounceVoice;
    bool audioReady, muted;
} StarterGame;

static bool InitStarter(void *state)
{
    StarterGame *game = state;
#if defined(PLATFORM_ANDROID)
    // NativeActivity assets use APK-relative paths.
    const char *assetsDir = "";
    const char *shadersDir = "";
#else
    const char *appDir = GetApplicationDirectory();
    char assetsDir[4096];
    char shadersDir[4096];
    if (snprintf(assetsDir, sizeof(assetsDir), "%sassets/", appDir) >= (int)sizeof(assetsDir) ||
        snprintf(shadersDir, sizeof(shadersDir), "%sshaders/", appDir) >= (int)sizeof(shadersDir))
        return false;
#endif
    if (!LoadWorldLevels(TextFormat("%sworlds", assetsDir)) ||
        !LoadCampaignLevels(TextFormat("%slevels", assetsDir)) ||
        !LoadCampaignSurfaces(&game->campaignSurfaces, shadersDir) ||
        !LoadTerrainLayers(&game->terrainLayers, assetsDir, shadersDir)) return false;
    LoadWorldOverrides();
    const char *assetPath = game->modelPath ? game->modelPath : TextFormat("%ssample.obj", assetsDir);
    game->model = LoadModel(assetPath);
    if (!IsModelValid(game->model) || game->model.meshes[0].vertexCount == 0) return false;
    if (game->texturePath)
    {
        game->override = LoadTexture(game->texturePath);
        if (!IsTextureValid(game->override)) return false;
        GenTextureMipmaps(&game->override);
        SetTextureFilter(game->override, TEXTURE_FILTER_TRILINEAR);
        game->originals = calloc((size_t)game->model.materialCount, sizeof(*game->originals));
        if (!game->originals) return false;
        for (int i = 0; i < game->model.materialCount; ++i)
        {
            game->originals[i] = game->model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture;
            SetMaterialTexture(&game->model.materials[i], MATERIAL_MAP_ALBEDO, game->override);
        }
    }

    game->shaded = LoadShader(TextFormat("%smodel.vert", shadersDir),
                        TextFormat("%smodel.frag", shadersDir));
    game->skyShader = LoadShader(TextFormat("%ssky.vert", shadersDir),
                           TextFormat("%ssky.frag", shadersDir));
    if (!IsShaderValid(game->shaded) || game->shaded.id == rlGetShaderIdDefault() ||
        !IsShaderValid(game->skyShader) || game->skyShader.id == rlGetShaderIdDefault()) return false;
    Vector3 light = { 0.5f, 1.0f, 0.6f };
    SetShaderValue(game->shaded, GetShaderLocation(game->shaded, "lightDirection"), &light, SHADER_UNIFORM_VEC3);

    for (int i = 0; i < game->model.materialCount; ++i)
        game->model.materials[i].shader = game->shaded;

    game->sky = LoadSkybox(game->skyPath, game->skyShader);
    if (!IsModelValid(game->sky)) return false;

    BoundingBox bounds = GetModelBoundingBox(game->model);
    Vector3 center = Vector3Scale(Vector3Add(bounds.min, bounds.max), 0.5f);
    float diameter = Vector3Distance(bounds.min, bounds.max);
    float scale = 3.5f / fmaxf(diameter, 0.000001f);
    game->model.transform = MatrixMultiply(game->model.transform, MatrixTranslate(-center.x, -center.y, -center.z));

    game->floorHeight = (bounds.min.y - center.y) * scale - 0.02f;
    game->camera = (Camera3D){
        .target = { 0, 0, 0 }, .up = { 0, 1, 0 }, .fovy = 45, .projection = CAMERA_PERSPECTIVE
    };
    game->yaw = 0.65f;
    game->pitch = 0.34f;
    game->distance = 5.5f;
    game->custom = true;
    game->showSky = true;
    game->grid = true;
    game->occlusionEnabled = true;
    game->markerMaterial = LoadMaterialDefault();
    game->markerMaterial.maps[MATERIAL_MAP_ALBEDO].color = (Color){ 86, 57, 24, 255 };
    game->ballDraw = (BallDrawData){ &game->ballMarker, game->markerMaterial };
    static const float ballLodDistances[] = { 7.5f, 11.0f };
    game->modelPrefab = (ScenePrefab){
        .localBounds = { Vector3Subtract(bounds.min, center), Vector3Subtract(bounds.max, center) },
        .drawRotated = DrawSceneModel, .data = &game->model,
        .opaqueBox = !game->modelPath && !game->texturePath,
    };
    game->ballPrefab = (ScenePrefab){
        .localBounds = { { -1, -1, -1 }, { 1, 1, 1 } },
        .lodDistances = ballLodDistances,
        .lodDistanceCount = sizeof(ballLodDistances) / sizeof(ballLodDistances[0]),
        .draw = DrawSceneBall, .data = &game->ballDraw,
    };
    game->titleModel = (PrefabInstance){
        .prefab = &game->modelPrefab, .scale = scale * 1.16f, .enabled = true,
    };
    game->spherePrefab = (ScenePrefab){
        .localBounds = { { -1, -1, -1 }, { 1, 1, 1 } },
        .draw = DrawEditorSphere,
    };
    InitSceneEditor(&game->editor, &game->modelPrefab, scale, &game->spherePrefab, game->floorHeight);
    game->ballInstance.prefab = &game->ballPrefab;
    game->titleOpen = true;
    InitAudioDevice();
    game->audioReady = IsAudioDeviceReady();
    if (game->audioReady)
    {
#if defined(BENNO_IOS)
        ConfigureIosGameAudio();
#endif
        game->ballSound = LoadSound(TextFormat("%sball-tap.wav", assetsDir));
        const SceneMusicSettings sceneMusic = {
            .path = TextFormat("%sstarter-ambient.ogg", assetsDir),
            .volume = 0.28f,
            .loop = true,
        };
        if (!SetSceneMusic(&game->music, sceneMusic))
            TraceLog(LOG_WARNING, "Could not load the starter scene music");

        game->dropVoice = CreateAudioVoice(game->ballSound);
        game->bounceVoice = CreateAudioVoice(game->ballSound);
    }
    return true;
}

static bool StarterAudioAvailable(const StarterGame *game)
{
    return IsSoundValid(game->ballSound) || IsMusicValid(game->music.track);
}

static void UpdateStarterAudio(StarterGame *game)
{
    game->dropVoice.options.muted = game->muted;
    game->bounceVoice.options.muted = game->muted;
    UpdateAudioVoice(&game->dropVoice, game->camera);
    UpdateAudioVoice(&game->bounceVoice, game->camera);
    UpdateSceneMusic(&game->music, game->muted);
}

static PlayerWorld GetPlayerWorld(const StarterGame *game, BoundingBox boxes[EDITOR_MAX_OBJECTS])
{
    PlayerWorld world = {
        .groundHeight = TerrainGround, .groundNormal = TerrainSlope,
        .ground = &game->editor.terrain, .boxes = boxes,
    };
    // The bundled model is a solid crate. External models may be hollow or irregular.
    if (!game->modelPath)
    {
        for (int i = 0; i < game->editor.count; ++i)
        {
            const EditorEntry *entry = &game->editor.entries[i];
            if (entry->kind != 0) continue;
            SceneObject object = MakePrefabSceneObject(&entry->instance);
            if (object.enabled) boxes[world.boxCount++] = object.bounds;
        }
    }
    return world;
}

static void ResetStarterPlayer(StarterGame *game, const PlayerWorld *world)
{
    float yaw = game->yaw;
    game->player = SpawnPlayer((Vector2){ 5.5f * sinf(yaw), 5.5f * cosf(yaw) }, world);
    Vector3 eye = GetPlayerEye(&game->player);
    game->walk = (WalkCamera){ .position = eye, .yaw = yaw + PI, .pitch = atan2f(-eye.y, 5.5f) };
    game->playerSpawned = true;
}

static void MoveStarterPlayer(StarterGame *game, const PlayerWorld *world, Vector2 movement,
                              Vector2 look, bool jump, bool sprint, float dt)
{
    game->walk.position = GetPlayerEye(&game->player);
    UpdateWalkCamera(&game->camera, &game->walk, look, Vector2Zero(), 0);
    Vector3 direction = Vector3Subtract(game->camera.target, game->camera.position);
    Vector2 forward = Vector2Normalize((Vector2){ direction.x, direction.z });
    Vector2 right = { -forward.y, forward.x };
    PlayerInput input = {
        .movement = Vector2Add(Vector2Scale(right, movement.x), Vector2Scale(forward, movement.y)),
        .jumpPressed = jump,
        .sprint = sprint,
    };
    StepPlayer(&game->player, world, input, dt);
    game->walk.position = game->camera.position = GetPlayerEye(&game->player);
    game->camera.target = Vector3Add(game->camera.position, direction);
}

static void DrawPlayerAvatar(const PlayerController *player)
{
    Vector3 bottom = player->feet;
    bottom.y += player->radius;
    Vector3 top = player->feet;
    top.y += player->height - player->radius;
    DrawCapsule(bottom, top, player->radius, 10, 5, (Color){ 187, 224, 153, 255 });
}

static void OpenCampaign(StarterGame *game, int level, bool debug, bool infinite)
{
    game->campaignSurfaces.litLevel = NULL;
    if (infinite) StartEndlessMazes(&game->campaign, &game->endlessLevel);
    else StartCampaign(&game->campaign, level, debug);
    game->campaignOpen = true;
    game->titleOpen = game->debugOpen = game->mazesOpen = false;
    PrepareCampaignLighting(&game->campaign, &game->campaignSurfaces);
    if (!HasPlayerTouchControls()) DisableCursor();
}

static void UpdateCampaignMode(StarterGame *game, const InputAction *actions,
                               InputPointer pointer, float deltaTime)
{
    Campaign *campaign = &game->campaign;
    bool touch = HasPlayerTouchControls();
    bool escape = actions[ACTION_EXIT].pressed;
    bool menu = touch && ReadCampaignMenu(pointer);
    bool deathScreen = campaign->death != CAMPAIGN_DEATH_NONE;
    bool advance = actions[ACTION_START].pressed || (deathScreen && ReadCampaignContinue(pointer));
    CampaignPauseAction pauseAction = campaign->paused ? ReadCampaignPause(pointer) : CAMPAIGN_PAUSE_NONE;
    if (pauseAction == CAMPAIGN_PAUSE_LEAVE ||
        ((campaign->cleared || campaign->gameOver) && (escape || (!deathScreen && menu) || advance)))
    {
        game->campaignOpen = false;
        game->debugOpen = campaign->debugRun;
        game->mazesOpen = !game->debugOpen;
        if (!touch) EnableCursor();
        return;
    }
    if (campaign->cleared || campaign->gameOver) return;
    if (deathScreen)
    {
        if (advance)
        {
            ContinueCampaign(campaign);
            if (!touch) DisableCursor();
        }
        return;
    }
    if (campaign->paused)
    {
        if (escape || actions[ACTION_START].pressed || pauseAction == CAMPAIGN_PAUSE_RESUME ||
            pauseAction == CAMPAIGN_PAUSE_RETRY)
        {
            campaign->paused = false;
            if (!touch) DisableCursor();
            if (pauseAction == CAMPAIGN_PAUSE_RETRY)
                StepCampaign(campaign, (Vector2){ 0 }, (Vector2){ 0 }, false, false, true, 0);
        }
        return;
    }
    if (escape || menu)
    {
        campaign->paused = true;
        ResetPlayerTouch(&campaign->touch);
        if (!touch) EnableCursor();
        return;
    }
    PlayerTouchInput touchInput = ReadPlayerTouch(&campaign->touch);
    Vector2 movement = {
        (float)(actions[ACTION_RIGHT].down - actions[ACTION_LEFT].down),
        (float)(actions[ACTION_FORWARD].down - actions[ACTION_BACK].down),
    };
    StepCampaign(campaign, Vector2Add(movement, touchInput.movement),
                 touch ? touchInput.look : pointer.delta,
                 actions[ACTION_SPACE].pressed || touchInput.jumpPressed,
                 actions[ACTION_SPRINT].down || touchInput.sprintHeld, actions[ACTION_RESET].pressed, deltaTime);
    if (!touch && campaign->death != CAMPAIGN_DEATH_NONE) EnableCursor();
}

static bool UpdateStarter(void *state, float deltaTime)
{
    StarterGame *game = state;
    InputAction actions[ACTION_COUNT];
    InputPointer pointer = PollInput(gameKeys, actions, ACTION_COUNT);
    if (actions[ACTION_FULLSCREEN].pressed) ToggleGameFullscreen();
    if (actions[ACTION_MUTE].pressed) game->muted = !game->muted;
    if (game->worldOpen)
    {
        game->titlePointer = pointer.position;
        Vector2 movement = {
            (float)(actions[ACTION_RIGHT].down - actions[ACTION_LEFT].down),
            (float)(actions[ACTION_FORWARD].down - actions[ACTION_BACK].down),
        };
        if (!UpdateWorldCampaign(&game->worlds, pointer, movement, actions[ACTION_SPACE].pressed,
                                 actions[ACTION_SPRINT].down, actions[ACTION_RESET].pressed,
                                 actions[ACTION_EXIT].pressed, actions[ACTION_START].pressed, deltaTime))
        {
            CloseWorldCampaign(&game->worlds);
            if (!HasPlayerTouchControls()) EnableCursor();
            game->worldOpen = false;
            game->worldPickerOpen = true;
        }
        UpdateStarterAudio(game);
        return true;
    }
    if (game->worldPickerOpen)
    {
        game->titlePointer = pointer.position;
        int choice = ReadWorldPicker(pointer);
        if (actions[ACTION_EXIT].pressed || choice == WORLD_PICK_BACK)
        {
            game->worldPickerOpen = false;
            game->titleOpen = true;
        }
        else
        {
            if (choice == WORLD_PICK_NONE)
                for (int i = 0; i < WORLD_COUNT; ++i)
                    if (IsKeyPressed(KEY_ONE + i)) choice = i * 2;
            if (choice == WORLD_PICK_NONE && actions[ACTION_START].pressed) choice = WORLD_PICK_FULL;
            if (choice >= 0 && choice < WORLD_PICK_FULL && choice % 2)
            {
                if (OpenWorldEditor(&game->worldEditor, choice / 2))
                {
                    game->worldEditorOpen = true;
                    game->worldPickerOpen = false;
                }
                else TraceLog(LOG_ERROR, "Could not allocate world editor draft");
            }
            else if (choice >= 0)
            {
                if (OpenWorldCampaign(&game->worlds, choice == WORLD_PICK_FULL ? -1 : choice / 2))
                {
                    game->worldOpen = true;
                    game->worldPickerOpen = false;
                    if (!HasPlayerTouchControls()) DisableCursor();
                }
                else TraceLog(LOG_ERROR, "Could not load campaign terrain and models");
            }
        }
        UpdateStarterAudio(game);
        return true;
    }
    if (game->worldEditorOpen)
    {
        game->titlePointer = pointer.position;
        if (actions[ACTION_EXIT].pressed || !UpdateWorldEditor(&game->worldEditor, pointer))
        {
            CloseWorldEditor(&game->worldEditor);
            game->worldEditorOpen = false;
            game->worldPickerOpen = true;
        }
        UpdateStarterAudio(game);
        return true;
    }
    if (game->campaignOpen)
    {
        game->titlePointer = pointer.position;
        UpdateCampaignMode(game, actions, pointer, deltaTime);
        if (game->campaignOpen) PrepareCampaignLighting(&game->campaign, &game->campaignSurfaces);
        UpdateStarterAudio(game);
        return true;
    }
    if (game->mazesOpen)
    {
        game->titlePointer = pointer.position;
        MazesAction choice = ReadMazesMenu(pointer);
        if (choice == MAZES_BACK || actions[ACTION_EXIT].pressed)
        {
            game->mazesOpen = false;
            game->titleOpen = true;
        }
        else if (choice == MAZES_CLASSIC || choice == MAZES_INFINITE || actions[ACTION_START].pressed)
            OpenCampaign(game, 0, false, choice == MAZES_INFINITE);
        UpdateStarterAudio(game);
        return true;
    }
    if (game->debugOpen)
    {
        game->titlePointer = pointer.position;
        int level = ReadDebugLevelSelect(pointer);
        if (actions[ACTION_EXIT].pressed || level == CAMPAIGN_LEVEL_COUNT)
        {
            game->debugOpen = false;
            game->titleOpen = true;
        }
        else if (level >= 0) OpenCampaign(game, level, true, false);
        UpdateStarterAudio(game);
        return true;
    }
    if (game->editorOpen)
    {
        if (actions[ACTION_EXIT].pressed || !UpdateSceneEditor(&game->editor, pointer, deltaTime))
        {
            game->editorOpen = false;
            game->editor.dragging = false;
            game->editor.brushDown = false;
            game->titleOpen = true;
            game->titlePointer = pointer.position;
        }
        UpdateStarterAudio(game);
        return true;
    }
    if (game->titleOpen)
    {
        game->titleTime = fmodf(game->titleTime + deltaTime, 60.0f);
        game->titlePointer = pointer.position;
        TitleAction action = ReadTitleScreen(pointer, StarterAudioAvailable(game));
        if (action == TITLE_QUIT || actions[ACTION_EXIT].pressed) return false;
        if (action == TITLE_AUDIO && !actions[ACTION_MUTE].pressed) game->muted = !game->muted;
        if (action == TITLE_EDITOR)
        {
            game->editorOpen = true;
            game->playerSpawned = false;
            game->titleOpen = false;
            game->editor.pointer = pointer.position;
            UpdateStarterAudio(game);
            return true;
        }
        if (action == TITLE_DEBUG)
        {
            game->titleOpen = false;
            game->debugOpen = true;
            UpdateStarterAudio(game);
            return true;
        }
        if (action == TITLE_CAMPAIGN || actions[ACTION_START].pressed)
        {
            game->worldPickerOpen = true;
            game->titleOpen = false;
            UpdateStarterAudio(game);
            return true;
        }
        if (action == TITLE_MAZES)
        {
            game->titleOpen = false;
            game->mazesOpen = true;
            UpdateStarterAudio(game);
            return true;
        }
        if (action == TITLE_SANDBOX)
        {
            game->titleOpen = false;
            game->pointerCaptured = pointer.down;
            pointer.pressed = false;
        }
        else
        {
            UpdateStarterAudio(game);
            return true;
        }
    }
    bool touch = HasPlayerTouchControls();
    Vector2 inset = GameUiInset();
    float buttonWidth = touch ? fminf(100.0f, (GetScreenWidth() - 2 * inset.x - 24) / 4.0f) : 116;
    game->dropBounds = (Rectangle){ GetScreenWidth() - 12 - inset.x - buttonWidth, 8 + inset.y, buttonWidth, 25 };
    game->modeBounds = (Rectangle){ game->dropBounds.x - buttonWidth - 8, 8 + inset.y, buttonWidth, 25 };
    game->menuBounds = (Rectangle){ game->modeBounds.x - buttonWidth - 8, 8 + inset.y, buttonWidth, 25 };
    game->drop = ReadUiButton(game->dropBounds, pointer, !game->walking);
    game->modeButton = ReadUiButton(game->modeBounds, pointer, touch);
    game->menuButton = ReadUiButton(game->menuBounds, pointer, touch && !game->walking);
    if (!pointer.down) game->pointerCaptured = false;
    if (game->drop.activated || game->modeButton.activated || game->menuButton.activated)
        game->pointerCaptured = true;

    const TerrainMap *terrain = &game->editor.terrain;
    bool dropped = game->drop.activated || actions[ACTION_DROP].pressed;
    float impact = 0.0f;
    if (dropped)
    {
        game->ball = (PhysicsSphere){
            .position = { 1.45f, GetTerrainHeight(terrain, 1.45f, -0.65f) + 2.75f, -0.65f },
            .velocity = { 0.25f, 0.0f, 0.1f },
            .radius = 0.22f,
            .restitution = 0.66f,
        };
        game->ballInstance.enabled = true;
    }
    float groundUnderBall = GetTerrainHeight(terrain, game->ball.position.x, game->ball.position.z);
    if (game->ball.resting && fabsf(game->ball.position.y - groundUnderBall - game->ball.radius) > 0.001f)
        game->ball.resting = false;
    if (game->ballInstance.enabled && !game->ball.resting)
    {
        float previousVelocity = game->ball.velocity.y;
        StepPhysicsSphereOnSurface(&game->ball, TerrainGround, terrain, deltaTime);
        if (previousVelocity < -0.6f && game->ball.velocity.y >= 0.0f)
            impact = -previousVelocity;
        game->ballMarkerHeight = UpdateBallMarker(&game->ballMarker, game->ball, terrain);
    }

    if (game->ballInstance.enabled)
    {
        game->ballInstance.position = game->ball.position;
        game->ballInstance.scale = game->ball.radius;
        game->ballObject = MakePrefabSceneObject(&game->ballInstance);
        // The attached dynamic marker remains at ground level.
        game->ballObject.bounds.min.y = fminf(game->ballObject.bounds.min.y, game->ballMarkerHeight.x);
        game->ballObject.bounds.max.y = fmaxf(game->ballObject.bounds.max.y, game->ballMarkerHeight.y);
    }
    if (actions[ACTION_OCCLUSION].pressed) game->occlusionEnabled = !game->occlusionEnabled;


    if (actions[ACTION_SKY].pressed) game->showSky = !game->showSky;
    if (actions[ACTION_GRID].pressed) game->grid = !game->grid;
    BoundingBox playerBoxes[EDITOR_MAX_OBJECTS];
    PlayerWorld world = GetPlayerWorld(game, playerBoxes);
    bool escape = actions[ACTION_EXIT].pressed || game->menuButton.activated;
    bool toggleWalk = actions[ACTION_WALK].pressed || game->modeButton.activated;
    if (game->walking && (escape || toggleWalk || (!touch && !IsWindowFocused())))
    {
        game->walking = false;
        if (!touch) EnableCursor();
    }
    else if (!game->walking && escape)
    {
        game->titleOpen = true;
        game->titlePointer = pointer.position;
        UpdateStarterAudio(game);
        return true;
    }
    else if (!game->walking && toggleWalk && (touch || IsWindowFocused()))
    {
        game->walking = true;
        if (!game->playerSpawned) ResetStarterPlayer(game, &world);
        ResetPlayerTouch(&game->playerTouch);
        if (!touch) DisableCursor();
    }

    if (game->walking)
    {
        bool resetWalk = actions[ACTION_RESET].pressed;
        if (resetWalk) ResetStarterPlayer(game, &world);
        PlayerTouchInput touchInput = ReadPlayerTouch(&game->playerTouch);
        Vector2 mouse = touch ? touchInput.look : pointer.delta;
        if (toggleWalk || resetWalk) mouse = Vector2Zero();
        Vector2 movement = {
            (float)(actions[ACTION_RIGHT].down - actions[ACTION_LEFT].down),
            (float)(actions[ACTION_FORWARD].down - actions[ACTION_BACK].down),
        };
        movement = Vector2Add(movement, touchInput.movement);
        MoveStarterPlayer(game, &world, movement, mouse,
            actions[ACTION_SPACE].pressed || touchInput.jumpPressed,
            actions[ACTION_SPRINT].down || touchInput.sprintHeld, deltaTime);
    }
    else
    {
        if (actions[ACTION_SPACE].pressed) game->custom = !game->custom;
        if (game->playerSpawned) StepPlayer(&game->player, &world, (PlayerInput){ 0 }, deltaTime);
        if (actions[ACTION_RESET].pressed) { game->yaw = 0.65f; game->pitch = 0.34f; game->distance = 5.5f; }
        if (pointer.down && !game->pointerCaptured)
        {
            Vector2 mouse = pointer.delta;
            game->yaw -= mouse.x * 0.008f;
            game->pitch = Clamp(game->pitch + mouse.y * 0.008f, -1.48f, 1.48f);
        }
        game->distance = Clamp(game->distance * powf(0.89f, pointer.wheel), 2.2f, 14.0f);
        float horizontal = game->distance * cosf(game->pitch);
        game->camera.position = (Vector3){ horizontal * sinf(game->yaw), game->distance * sinf(game->pitch),
                                     horizontal * cosf(game->yaw) };
        game->camera.target = Vector3Zero();
    }
    for (int i = 0; i < game->model.materialCount; ++i)
    {
        game->model.materials[i].shader = game->custom ? game->shaded
            : (Shader){ .id = rlGetShaderIdDefault(), .locs = rlGetShaderLocsDefault() };
    }

    if (dropped)
    {
        AudioVoiceOptions options = DefaultAudioVoiceOptions();
        options.volume = 0.38f;
        options.pitch = 1.6f;
        options.muted = game->muted;
        PlayAudioVoice(&game->dropVoice, options, game->camera);
    }
    if (impact > 0.0f)
    {
        AudioVoiceOptions options = DefaultAudioVoiceOptions();
        options.spatial = true;
        options.position = game->ball.position;
        options.volume = Clamp(impact * 0.19f, 0.0f, 1.0f);
        options.pitch = 0.8f;
        options.minDistance = 2.0f;
        options.maxDistance = 18.0f;
        options.muted = game->muted;
        PlayAudioVoice(&game->bounceVoice, options, game->camera);
    }
    UpdateStarterAudio(game);
    return true;
}

static void DrawStarterTitle(const StarterGame *game)
{
    float angle = 0.68f + 0.23f * sinf(game->titleTime * (2.0f * PI) / 15.0f);
    Camera3D camera = game->camera;
    camera.position = (Vector3){ 6.0f * sinf(angle), 2.1f, 6.0f * cosf(angle) };
    camera.target = Vector3Zero();
    if (GetScreenWidth() >= 760)
    {
        Vector3 offset = { -1.35f * cosf(angle), 0.0f, 1.35f * sinf(angle) };
        camera.position = Vector3Add(camera.position, offset);
        camera.target = offset;
    }

    BeginMode3D(camera);
    DrawSky(game->sky, camera.position);
    SceneObject showcase = MakePrefabSceneObject(&game->titleModel);
    DrawScene(&showcase, 1, camera, (float)rlGetCullDistanceNear(), false);
    EndMode3D();
    DrawTitleScreen(game->titlePointer, game->muted, StarterAudioAvailable(game));
}

static void DrawStarter(const void *state)
{
    const StarterGame *game = state;
    ClearBackground((Color){ 29, 42, 54, 255 });
    if (game->worldOpen)
    {
        BeginMode3D(game->worlds.camera);
        DrawSky(game->sky, game->worlds.camera.position);
        DrawWorldLandscape(&game->worlds, &game->terrainLayers);
        EndMode3D();
        DrawWorldCampaignHud(&game->worlds, (InputPointer){ .position = game->titlePointer });
        return;
    }
    if (game->worldPickerOpen)
    {
        DrawWorldPicker((InputPointer){ .position = game->titlePointer });
        return;
    }
    if (game->worldEditorOpen)
    {
        DrawWorldEditor(&game->worldEditor, (InputPointer){ .position = game->titlePointer });
        return;
    }
    if (game->campaignOpen)
    {
        BeginMode3D(game->campaign.camera);
        DrawSky(game->sky, game->campaign.camera.position);
        DrawCampaignWorld(&game->campaign, &game->campaignSurfaces);
        EndMode3D();
        DrawCampaignHud(&game->campaign, (InputPointer){ .position = game->titlePointer });
        return;
    }
    if (game->mazesOpen)
    {
        DrawMazesMenu(game->titlePointer);
        return;
    }
    if (game->debugOpen)
    {
        DrawDebugLevelSelect(game->titlePointer);
        return;
    }
    if (game->titleOpen)
    {
        DrawStarterTitle(game);
        return;
    }
    Camera3D camera = game->editorOpen ? GetEditorCamera(&game->editor) : game->camera;
    BeginMode3D(camera);
    if (game->editorOpen || game->showSky) DrawSky(game->sky, camera.position);
    if (game->editor.terrain.enabled)
    {
        SetTerrainLayerElevation(&game->terrainLayers, game->editor.terrain.baseHeight, 5);
        DrawLayeredTerrainMap(&game->editor.terrain, &game->terrainLayers);
    }
    else if (game->editorOpen || game->grid)
    {
        rlPushMatrix();
        rlTranslatef(0, game->floorHeight, 0);
        DrawGrid(game->editorOpen ? 40 : 20, 0.5f);
        rlPopMatrix();
    }
    SceneObject objects[EDITOR_MAX_OBJECTS + 1];
    size_t count = GetEditedScene(&game->editor, objects);
    if (!game->editorOpen && game->ballObject.enabled) objects[count++] = game->ballObject;
    SceneStats stats = DrawScene(objects, count, camera, (float)rlGetCullDistanceNear(),
                                !game->editorOpen && game->occlusionEnabled);
    if (game->editorOpen) DrawEditorSelection(&game->editor);
    else if (game->playerSpawned && !game->walking) DrawPlayerAvatar(&game->player);
    EndMode3D();
    if (game->editorOpen)
    {
        DrawEditorOverlay(&game->editor);
        return;
    }
    Vector2 inset = GameUiInset();
    int left = (int)inset.x + 12, top = (int)inset.y;
    DrawUiPanel((Rectangle){ 0, 0, (float)GetScreenWidth(), 70 + inset.y }, (Color){ 15, 24, 35, 235 });
    bool touch = HasPlayerTouchControls();
    DrawUiButton(game->dropBounds, touch ? "Drop ball" : "Drop ball [P]", game->drop, !game->walking);
    if (touch)
    {
        DrawUiButton(game->modeBounds, game->walking ? "Orbit" : "Player", game->modeButton, true);
        if (!game->walking) DrawUiButton(game->menuBounds, "Menu", game->menuButton, true);
    }
    float headingRight = touch ? (game->walking ? game->modeBounds.x : game->menuBounds.x) : game->dropBounds.x;
    if (CanToggleGameFullscreen())
    {
        const int fontSize = 14;
        const char *shortcut = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)
            ? "F11 Exit fullscreen" : "F11 Fullscreen";
        int x = (int)game->dropBounds.x - 12 - MeasureText(shortcut, fontSize);
        DrawText(shortcut, x, 14, fontSize, RAYWHITE);
        headingRight = (float)x;
    }
    const char *heading = touch ? (game->walking ? "Player" : "Orbit")
        : (game->walking ? GAME_WINDOW_TITLE " / Walk" : GAME_WINDOW_TITLE " / Orbit");
    DrawUiLabel(heading, left, 10 + top, 23, headingRight - left - 8, RAYWHITE);
    const char *controls = game->walking
        ? "WASD move / Space jump / Shift sprint / Mouse look / R reset / F or Esc orbit"
        : "Drag orbit / Scroll zoom / F player / Esc menu / R reset / Space shader / B sky / G grid";
    if (touch)
        controls = game->walking ? "Stick to move / Swipe to look / Hold Sprint / Tap Jump" : "Drag to orbit / Tap Player to move";
    DrawUiLabel(controls, left, 43 + top, 14, GetScreenWidth() - 2 * left, (Color){ 195, 214, 230, 255 });
    if (game->walking)
    {
        int x = GetScreenWidth() / 2, y = GetScreenHeight() / 2;
        DrawLine(x - 5, y, x + 5, y, RAYWHITE);
        DrawLine(x, y - 5, x, y + 5, RAYWHITE);
        if (game->player.sliding)
            DrawText("Sliding", x - MeasureText("Sliding", 14) / 2, y + 16, 14, RAYWHITE);
        if (touch)
        {
            DrawPlayerTouch(&game->playerTouch);
            return;
        }
    }
    float statsWidth = fminf(420, GetScreenWidth() - 2 * left);
    int statsY = GetScreenHeight() - 33 - top;
    DrawUiPanel((Rectangle){ (float)left, (float)statsY, statsWidth, 25 }, (Color){ 15, 24, 35, 235 });
    DrawUiLabel(TextFormat("Drawn %d  /  Occluded %d  /  C culling: %s  /  M audio: %s",
                (int)stats.drawn, (int)stats.occluded, game->occlusionEnabled ? "on" : "off",
                !StarterAudioAvailable(game) ? "unavailable" : (game->muted ? "off" : "on")),
                left + 8, statsY + 6, 14, statsWidth - 16, RAYWHITE);
}

static void ShutdownStarter(void *state)
{
    StarterGame *game = state;
    CloseWorldCampaign(&game->worlds);
    CloseWorldEditor(&game->worldEditor);
    UnloadWorldLevels();
    UnloadCampaignSurfaces(game->campaignSurfaces);
    UnloadTerrainLayers(&game->terrainLayers);
    UnloadTerrainMap(&game->editor.terrain);
    UnloadSceneMusic(&game->music);
    DestroyAudioVoice(&game->dropVoice);
    DestroyAudioVoice(&game->bounceVoice);
    if (IsSoundValid(game->ballSound)) UnloadSound(game->ballSound);
    if (game->audioReady) CloseAudioDevice();
    if (game->originals)
    {
        for (int i = 0; i < game->model.materialCount; ++i)
            SetMaterialTexture(&game->model.materials[i], MATERIAL_MAP_ALBEDO, game->originals[i]);
        free(game->originals);
    }
    if (game->override.id) UnloadTexture(game->override);
    UnloadImportedModelTextures(game->model);
    UnloadModel(game->model);
    UnloadSkybox(game->sky);
    if (game->shaded.id && game->shaded.id != rlGetShaderIdDefault()) UnloadShader(game->shaded);
    if (game->skyShader.id && game->skyShader.id != rlGetShaderIdDefault()) UnloadShader(game->skyShader);
    UnloadDynamicTriangles(&game->ballMarker);
    if (game->markerMaterial.maps) UnloadMaterial(game->markerMaterial);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--check-worlds") == 0)
    {
        if (argc > 3) return EXIT_FAILURE;
        const char *directory = argc == 3 ? argv[2] : TextFormat("%sassets/worlds", GetApplicationDirectory());
        if (!LoadWorldLevels(directory)) return EXIT_FAILURE;
        printf("Validated %d campaign worlds.\n", WORLD_COUNT);
        UnloadWorldLevels();
        return EXIT_SUCCESS;
    }
    if (argc > 1 && strcmp(argv[1], "--check-levels") == 0)
    {
        if (argc > 3)
        {
            fputs("Usage: benno111-engene --check-levels [directory]\n", stderr);
            return EXIT_FAILURE;
        }
        const char *directory = argc == 3 ? argv[2] : TextFormat("%sassets/levels", GetApplicationDirectory());
        if (!LoadCampaignLevels(directory)) return EXIT_FAILURE;
        printf("Validated %d levels and %d boss assignments.\n", CAMPAIGN_LEVEL_COUNT, CAMPAIGN_BOSS_COUNT);
        return EXIT_SUCCESS;
    }
    if (argc > 4 || (argc > 1 && strcmp(argv[1], "--help") == 0))
    {
        puts("Usage: benno111-engene [model] [diffuse-texture|-] [skybox-strip|-]");
        puts("       benno111-engene --check-levels [directory]");
        puts("       benno111-engene --check-worlds [directory]");
        return argc > 4 ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    // Use working-directory paths for user assets and executable-relative paths for bundled assets.
    const char *modelPath = argc > 1 ? argv[1] : NULL;
    const char *texturePath = argc > 2 && strcmp(argv[2], "-") != 0 ? argv[2] : NULL;
    const char *skyPath = argc > 3 && strcmp(argv[3], "-") != 0 ? argv[3] : NULL;
    if ((modelPath && !FileExists(modelPath)) || (texturePath && !FileExists(texturePath)) ||
        (skyPath && !FileExists(skyPath)))
    {
        fputs("An input file was not found. Check the model, texture, and skybox paths.\n", stderr);
        return EXIT_FAILURE;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1100, 720, GAME_WINDOW_TITLE);
    if (!IsWindowReady()) return EXIT_FAILURE;
#if defined(BENNO_IOS)
    // Spend more of the depth buffer on the actual viewable scene.
    // The farthest streamed terrain corners are within 272 units of the player.
    rlSetClipPlanes(0.12, 320.0);
#endif
    if (!InitInputTouches())
    {
        TraceLog(LOG_ERROR, "Could not initialize native touch input");
        CloseWindow();
        return EXIT_FAILURE;
    }
    SetWindowMinSize(640, 360);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    StarterGame game = { .modelPath = modelPath, .texturePath = texturePath, .skyPath = skyPath };
    GameLogic logic = {
        .init = InitStarter,
        .update = UpdateStarter,
        .draw = DrawStarter,
        .shutdown = ShutdownStarter,
    };
    bool success = RunGameLoop(logic, &game);
    if (!success) TraceLog(LOG_ERROR, "Could not initialize campaign levels or game assets");
    CloseInputTouches();
    CloseWindow();
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
