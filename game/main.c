#include "game.h"

#if defined(BENNO_IOS)
#include <SDL_main.h>
#endif
#include "audio.h"
#include "camera.h"
#include "dynamic_buffer.h"
#include "input.h"
#include "game_logic.h"
#include "physics.h"
#include "ui.h"
#include "window.h"
#include "scene.h"
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
    ACTION_SHADER, ACTION_SKY, ACTION_GRID, ACTION_EXIT, ACTION_WALK,
    ACTION_RESET, ACTION_RIGHT, ACTION_LEFT, ACTION_FORWARD, ACTION_BACK,
    ACTION_DROP, ACTION_OCCLUSION, ACTION_MUTE, ACTION_FULLSCREEN, ACTION_COUNT
};

static const int gameKeys[ACTION_COUNT] = {
    [ACTION_SHADER] = KEY_SPACE, [ACTION_SKY] = KEY_B, [ACTION_GRID] = KEY_G,
    [ACTION_EXIT] = KEY_ESCAPE, [ACTION_WALK] = KEY_F, [ACTION_RESET] = KEY_R,
    [ACTION_RIGHT] = KEY_D, [ACTION_LEFT] = KEY_A, [ACTION_FORWARD] = KEY_W,
    [ACTION_BACK] = KEY_S, [ACTION_DROP] = KEY_P, [ACTION_OCCLUSION] = KEY_C,
    [ACTION_MUTE] = KEY_M, [ACTION_FULLSCREEN] = KEY_F11,
};

typedef struct ModelDrawData {
    const Model *model;
    float scale;
} ModelDrawData;

typedef struct BallDrawData {
    const PhysicsSphere *ball;
    const DynamicVertexBuffer *marker;
    Material markerMaterial;
} BallDrawData;

static void DrawSceneModel(const void *context, size_t lod)
{
    const ModelDrawData *data = context;
    (void)lod;
    DrawModel(*data->model, Vector3Zero(), data->scale, WHITE);
}

static void DrawSceneBall(const void *context, size_t lod)
{
    const BallDrawData *data = context;
    static const int rings[] = { 12, 8, 4 };
    static const int slices[] = { 16, 10, 6 };
    DrawDynamicTriangles(data->marker, data->markerMaterial, MatrixIdentity());
    DrawSphereEx(data->ball->position, data->ball->radius, rings[lod], slices[lod], ORANGE);
}

static void UpdateBallMarker(DynamicVertexBuffer *buffer, PhysicsSphere ball, float floorHeight)
{
    float x = ball.position.x;
    float z = ball.position.z;
    float y = floorHeight + 0.012f;
    float radius = ball.radius;
    float vertices[] = {
        x - radius, y, z,  x, y, z + radius,  x + radius, y, z,
        x - radius, y, z,  x + radius, y, z,  x, y, z - radius,
    };
    UploadDynamicTriangles(buffer, vertices, sizeof(vertices) / (3 * sizeof(vertices[0])));
}

static WalkCamera SpawnWalker(float orbitYaw, float floorHeight)
{
    Vector3 position = { 5.5f * sinf(orbitYaw), floorHeight + 1.65f, 5.5f * cosf(orbitYaw) };
    return (WalkCamera){ .position = position, .yaw = orbitYaw + PI,
                         .pitch = atan2f(-position.y, 5.5f) };
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
    PhysicsSphere ball;
    ModelDrawData modelDraw;
    BallDrawData ballDraw;
    SceneObject scene[2];
    Rectangle dropBounds;
    UiButtonState drop;
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
    game->modelDraw = (ModelDrawData){ &game->model, scale };
    game->ballDraw = (BallDrawData){ &game->ball, &game->ballMarker, game->markerMaterial };
    static const float ballLodDistances[] = { 7.5f, 11.0f };
    game->scene[0] = (SceneObject){
        .bounds = { Vector3Scale(Vector3Subtract(bounds.min, center), scale),
                    Vector3Scale(Vector3Subtract(bounds.max, center), scale) },
        .draw = DrawSceneModel, .data = &game->modelDraw,
        .enabled = true, .opaqueBox = !game->modelPath && !game->texturePath,
    };
    game->scene[1] = (SceneObject){
        .lodDistances = ballLodDistances,
        .lodDistanceCount = sizeof(ballLodDistances) / sizeof(ballLodDistances[0]),
        .draw = DrawSceneBall, .data = &game->ballDraw,
    };
    InitAudioDevice();
    game->audioReady = IsAudioDeviceReady();
    if (game->audioReady)
    {
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

static bool UpdateStarter(void *state, float deltaTime)
{
    StarterGame *game = state;
    InputAction actions[ACTION_COUNT];
    InputPointer pointer = PollInput(gameKeys, actions, ACTION_COUNT);
    if (actions[ACTION_FULLSCREEN].pressed) ToggleGameFullscreen();
    game->dropBounds = (Rectangle){ GetScreenWidth() - 128.0f, 8, 116, 25 };
    game->drop = ReadUiButton(game->dropBounds, pointer, !game->walking);
    if (!pointer.down) game->pointerCaptured = false;
    if (game->drop.activated) game->pointerCaptured = true;

    if (actions[ACTION_MUTE].pressed) game->muted = !game->muted;
    bool dropped = game->drop.activated || actions[ACTION_DROP].pressed;
    float impact = 0.0f;
    if (dropped)
    {
        game->ball = (PhysicsSphere){
            .position = { 1.45f, game->floorHeight + 2.75f, -0.65f },
            .velocity = { 0.25f, 0.0f, 0.1f },
            .radius = 0.22f,
            .restitution = 0.66f,
        };
        game->scene[1].enabled = true;
    }
    if (game->scene[1].enabled && !game->ball.resting)
    {
        float previousVelocity = game->ball.velocity.y;
        StepPhysicsSphere(&game->ball, game->floorHeight, deltaTime);
        if (previousVelocity < -0.6f && game->ball.velocity.y >= 0.0f)
            impact = -previousVelocity;
        UpdateBallMarker(&game->ballMarker, game->ball, game->floorHeight);
    }

    if (game->scene[1].enabled)
    {
        Vector3 radius = { game->ball.radius, game->ball.radius, game->ball.radius };
        game->scene[1].bounds.min = Vector3Subtract(game->ball.position, radius);
        game->scene[1].bounds.min.y = fminf(game->scene[1].bounds.min.y, game->floorHeight);
        game->scene[1].bounds.max = Vector3Add(game->ball.position, radius);
    }
    if (actions[ACTION_OCCLUSION].pressed) game->occlusionEnabled = !game->occlusionEnabled;

    if (actions[ACTION_SHADER].pressed) game->custom = !game->custom;
    if (actions[ACTION_SKY].pressed) game->showSky = !game->showSky;
    if (actions[ACTION_GRID].pressed) game->grid = !game->grid;
    bool escape = actions[ACTION_EXIT].pressed;
    bool toggleWalk = actions[ACTION_WALK].pressed;
    if (game->walking && (escape || toggleWalk || !IsWindowFocused()))
    {
        game->walking = false;
        EnableCursor();
    }
    else if (!game->walking && escape) return false;
    else if (!game->walking && toggleWalk && IsWindowFocused())
    {
        game->walking = true;
        game->walk = SpawnWalker(game->yaw, game->floorHeight);
        DisableCursor();
    }

    if (game->walking)
    {
        bool resetWalk = actions[ACTION_RESET].pressed;
        if (resetWalk) game->walk = SpawnWalker(game->yaw, game->floorHeight);
        Vector2 mouse = (toggleWalk || resetWalk) ? (Vector2){ 0 } : pointer.delta;
        Vector2 movement = {
            (float)(actions[ACTION_RIGHT].down - actions[ACTION_LEFT].down),
            (float)(actions[ACTION_FORWARD].down - actions[ACTION_BACK].down),
        };
        UpdateWalkCamera(&game->camera, &game->walk, mouse, movement, 3.5f * fminf(deltaTime, 0.05f));
    }
    else
    {
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
    game->dropVoice.options.muted = game->muted;
    game->bounceVoice.options.muted = game->muted;
    UpdateAudioVoice(&game->dropVoice, game->camera);
    UpdateAudioVoice(&game->bounceVoice, game->camera);
    UpdateSceneMusic(&game->music, game->muted);

    return true;
}

static void DrawStarter(const void *state)
{
    const StarterGame *game = state;
    ClearBackground((Color){ 29, 42, 54, 255 });
    BeginMode3D(game->camera);
    if (game->showSky) DrawSky(game->sky, game->camera.position);
    if (game->grid)
    {
        rlPushMatrix();
        rlTranslatef(0, game->floorHeight, 0);
        DrawGrid(20, 0.5f);
        rlPopMatrix();
    }
    SceneStats stats = DrawScene(game->scene, sizeof(game->scene) / sizeof(game->scene[0]), game->camera,
                                (float)rlGetCullDistanceNear(), game->occlusionEnabled);
    EndMode3D();
    DrawUiPanel((Rectangle){ 0, 0, (float)GetScreenWidth(), 70 }, (Color){ 15, 24, 35, 235 });
    DrawUiButton(game->dropBounds, "Drop ball [P]", game->drop, !game->walking);
    if (CanToggleGameFullscreen())
    {
        const int fontSize = 14;
        const char *shortcut = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)
            ? "F11 Exit fullscreen" : "F11 Fullscreen";
        int x = (int)game->dropBounds.x - 12 - MeasureText(shortcut, fontSize);
        DrawText(shortcut, x, 14, fontSize, RAYWHITE);
    }
    DrawText(game->walking ? GAME_WINDOW_TITLE " / Walk" : GAME_WINDOW_TITLE " / Orbit", 20, 10, 23, RAYWHITE);
    DrawText(game->walking
             ? "WASD move / Mouse look / F or Esc orbit / R reset / Space shader / B sky / G grid"
             : "Drag orbit / Scroll zoom / F walk / R reset / Space shader / B sky / G grid",
             20, 43, 14, (Color){ 195, 214, 230, 255 });
    DrawUiPanel((Rectangle){ 12, GetScreenHeight() - 33.0f, 420, 25 },
                (Color){ 15, 24, 35, 235 });
    DrawText(TextFormat("Drawn %d  /  Occluded %d  /  C culling: %s  /  M audio: %s",
             (int)stats.drawn, (int)stats.occluded, game->occlusionEnabled ? "on" : "off",
             !IsSoundValid(game->ballSound) && !IsMusicValid(game->music.track) ? "unavailable" : (game->muted ? "off" : "on")),
             20, GetScreenHeight() - 27, 14, RAYWHITE);
}

static void ShutdownStarter(void *state)
{
    StarterGame *game = state;
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
    if (argc > 4 || (argc > 1 && strcmp(argv[1], "--help") == 0))
    {
        puts("Usage: benno111-engene [model] [diffuse-texture|-] [skybox-strip|-]");
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
    if (!success) TraceLog(LOG_ERROR, "Could not initialize model, texture, or shaders");
    CloseWindow();
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
