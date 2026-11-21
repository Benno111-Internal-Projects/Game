#include "scene_editor.h"
#include "raymath.h"
#include "ui.h"

#if defined(BENNO_IOS)
#include <SDL.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum EditorCommand {
    BACK, SAVE, LOAD, ADD_MODEL, ADD_SPHERE, DELETE_OBJECT,
    PREVIOUS, NEXT, RESET_VIEW,
    X_MINUS, X_PLUS, TURN_LEFT,
    Y_MINUS, Y_PLUS, TURN_RIGHT,
    Z_MINUS, Z_PLUS, SMALLER,
    ZOOM_OUT, ZOOM_IN, LARGER,
    COMMAND_COUNT
} EditorCommand;

static const char *const labels[COMMAND_COUNT] = {
    "Menu", "Save", "Load", "Add model", "Add sphere", "Delete",
    "Previous", "Next", "Reset view",
    "X -", "X +", "Turn -",
    "Y -", "Y +", "Turn +",
    "Z -", "Z +", "Scale -",
    "Zoom -", "Zoom +", "Scale +",
};

static Rectangle EditorPanel(void)
{
    float width = fminf(330.0f, GetScreenWidth() * 0.47f);
    return (Rectangle){ GetScreenWidth() - width, 0, width, (float)GetScreenHeight() };
}

static Rectangle CommandBounds(Rectangle panel, EditorCommand command)
{
    float gap = panel.width < 230.0f ? 3.0f : 6.0f;
    float width = (panel.width - 4.0f * gap) / 3.0f;
    float row = Clamp((panel.height - 110.0f) / 7.0f, 20.0f, 48.0f);
    return (Rectangle){ panel.x + gap + (command % 3) * (width + gap),
                        61.0f + (command / 3) * row, width, row - 3.0f };
}

static bool CommandEnabled(const SceneEditor *editor, EditorCommand command)
{
    if (command == ADD_MODEL || command == ADD_SPHERE) return editor->count < EDITOR_MAX_OBJECTS;
    if (command == PREVIOUS || command == NEXT) return editor->count > 0;
    return (command != DELETE_OBJECT && (command < X_MINUS || command == ZOOM_OUT || command == ZOOM_IN)) ||
           editor->selected >= 0;
}

static void ResetView(SceneEditor *editor)
{
    editor->yaw = 0.64f;
    editor->pitch = 0.42f;
    editor->distance = 9.5f;
}

static void AddEntry(SceneEditor *editor, int kind, Vector3 position)
{
    editor->selected = editor->count++;
    editor->entries[editor->selected] = (EditorEntry){
        .kind = kind,
        .instance = { .prefab = editor->prefabs[kind], .position = position,
                      .scale = editor->defaultScales[kind], .enabled = true },
    };
}

void InitSceneEditor(SceneEditor *editor, const ScenePrefab *model, float scale,
                     const ScenePrefab *sphere, float floorHeight)
{
    *editor = (SceneEditor){ .prefabs = { model, sphere }, .defaultScales = { scale, 0.6f },
                             .floorHeight = floorHeight, .selected = -1 };
    ResetView(editor);
    AddEntry(editor, 0, Vector3Zero());
}

size_t GetEditedScene(const SceneEditor *editor, SceneObject objects[EDITOR_MAX_OBJECTS])
{
    for (int i = 0; i < editor->count; ++i)
        objects[i] = MakePrefabSceneObject(&editor->entries[i].instance);
    return (size_t)editor->count;
}

Camera3D GetEditorCamera(const SceneEditor *editor)
{
    float horizontal = editor->distance * cosf(editor->pitch);
    const float fov = 45;
    float shift = EditorPanel().width / fmaxf((float)GetScreenHeight(), 1.0f) *
                  editor->distance * tanf(fov * DEG2RAD / 2.0f);
    Vector3 target = { cosf(editor->yaw) * shift, 0, -sinf(editor->yaw) * shift };
    Vector3 offset = { sinf(editor->yaw) * horizontal, sinf(editor->pitch) * editor->distance,
                       cosf(editor->yaw) * horizontal };
    return (Camera3D){
        .position = Vector3Add(target, offset), .target = target,
        .up = { 0, 1, 0 }, .fovy = fov, .projection = CAMERA_PERSPECTIVE,
    };
}

static bool ScenePath(char *path, size_t capacity, bool create)
{
#if defined(PLATFORM_ANDROID)
    // raylib maps this path to the activity's private storage.
    (void)create;
    int length = snprintf(path, capacity, "edited-scene.benno");
#elif defined(BENNO_IOS)
    (void)create;
    char *dir = SDL_GetPrefPath("Benno111", "Benno111Engene");
    if (!dir) return false;
    int length = snprintf(path, capacity, "%sedited-scene.benno", dir);
    SDL_free(dir);
#else
    char dir[768];
#if defined(_WIN32)
    const char *base = getenv("LOCALAPPDATA");
    int dirLength = snprintf(dir, sizeof(dir), "%s/benno111-engene", base ? base : GetWorkingDirectory());
#else
    const char *base = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    int dirLength = base && *base ? snprintf(dir, sizeof(dir), "%s/benno111-engene", base)
        : snprintf(dir, sizeof(dir), "%s/.local/share/benno111-engene", home ? home : GetWorkingDirectory());
#endif
    if (dirLength < 0 || (size_t)dirLength >= sizeof(dir)) return false;
    if (create && !DirectoryExists(dir) && MakeDirectory(dir) != 0) return false;
    int length = snprintf(path, capacity, "%s/edited-scene.benno", dir);
#endif
    return length >= 0 && (size_t)length < capacity;
}

static bool SaveEditedScene(const SceneEditor *editor)
{
    char path[1024];
    if (!ScenePath(path, sizeof(path), true)) return false;
    char text[EDITOR_MAX_OBJECTS * 160 + 16] = "BENNO-SCENE 1\n";
    size_t used = strlen(text);
    for (int i = 0; i < editor->count; ++i)
    {
        const EditorEntry *entry = &editor->entries[i];
        const PrefabInstance *instance = &entry->instance;
        int length = snprintf(text + used, sizeof(text) - used, "%d %.9g %.9g %.9g %.9g %.9g\n",
            entry->kind, instance->position.x, instance->position.y, instance->position.z,
            instance->scale, instance->yawDegrees);
        if (length < 0 || (size_t)length >= sizeof(text) - used) return false;
        used += (size_t)length;
    }
    return SaveFileText(path, text);
}

static bool LoadEditedScene(SceneEditor *editor)
{
    char path[1024];
    if (!ScenePath(path, sizeof(path), false)) return false;
    char *text = LoadFileText(path);
    if (!text) return false;

    static const char header[] = "BENNO-SCENE 1\n";
    bool valid = strncmp(text, header, sizeof(header) - 1) == 0;
    EditorEntry entries[EDITOR_MAX_OBJECTS];
    int count = 0;
    char *line = text + (valid ? sizeof(header) - 1 : 0);
    while (valid && *line)
    {
        char *end = strchr(line, '\n');
        if (end) *end = '\0';
        EditorEntry entry = { 0 };
        PrefabInstance *instance = &entry.instance;
        char extra;
        int read = sscanf(line, "%2d %32f %32f %32f %32f %32f %c", &entry.kind,
            &instance->position.x, &instance->position.y, &instance->position.z,
            &instance->scale, &instance->yawDegrees, &extra);
        valid = count < EDITOR_MAX_OBJECTS && read == 6 && entry.kind >= 0 && entry.kind < EDITOR_PREFAB_COUNT &&
            fabsf(instance->position.x) <= 100 && fabsf(instance->position.y) <= 100 &&
            fabsf(instance->position.z) <= 100 &&
            instance->scale >= editor->defaultScales[entry.kind] * 0.05f &&
            instance->scale <= editor->defaultScales[entry.kind] * 20.0f &&
            fabsf(instance->yawDegrees) <= 360;
        if (!valid) break;
        instance->prefab = editor->prefabs[entry.kind];
        instance->enabled = true;
        entries[count++] = entry;
        if (!end) break;
        line = end + 1;
    }

    UnloadFileText(text);
    if (valid)
    {
        memcpy(editor->entries, entries, (size_t)count * sizeof(entries[0]));
        editor->count = count;
        editor->selected = count ? 0 : -1;
    }
    return valid;
}

static void ApplyCommand(SceneEditor *editor, EditorCommand command)
{
    if (command == SAVE)
        editor->status = SaveEditedScene(editor) ? "Scene saved." : "Could not save the scene.";
    else if (command == LOAD)
        editor->status = LoadEditedScene(editor) ? "Scene loaded." : "No valid saved scene.";
    else if (command == ADD_MODEL || command == ADD_SPHERE)
    {
        int kind = command == ADD_MODEL ? 0 : 1;
        float x = editor->count ? (float)(editor->count % 5) * 2.25f : 0.0f;
        float z = (float)(editor->count / 5) * 2.25f;
        float y = editor->floorHeight - editor->prefabs[kind]->localBounds.min.y * editor->defaultScales[kind];
        AddEntry(editor, kind, (Vector3){ x, y, z });
        editor->status = "Unsaved changes";
    }
    else if (command == DELETE_OBJECT)
    {
        int index = editor->selected;
        --editor->count;
        memmove(&editor->entries[index], &editor->entries[index + 1],
            (size_t)(editor->count - index) * sizeof(editor->entries[0]));
        if (index >= editor->count) editor->selected = editor->count - 1;
        editor->status = "Unsaved changes";
    }
    else if (command == PREVIOUS || command == NEXT)
    {
        int step = command == NEXT ? 1 : -1;
        int current = editor->selected < 0 ? (step > 0 ? -1 : 0) : editor->selected;
        editor->selected = (current + editor->count + step) % editor->count;
    }
    else if (command == RESET_VIEW) ResetView(editor);
    else if (command == ZOOM_IN || command == ZOOM_OUT)
        editor->distance = Clamp(editor->distance * (command == ZOOM_IN ? 0.8f : 1.25f), 2.5f, 36);
    else
    {
        EditorEntry *entry = &editor->entries[editor->selected];
        PrefabInstance *instance = &entry->instance;
        float baseScale = editor->defaultScales[entry->kind];
        const float step = 0.25f;
        switch (command)
        {
            case X_MINUS: instance->position.x = fmaxf(-100, instance->position.x - step); break;
            case X_PLUS:  instance->position.x = fminf(100, instance->position.x + step); break;
            case Y_MINUS: instance->position.y = fmaxf(-100, instance->position.y - step); break;
            case Y_PLUS:  instance->position.y = fminf(100, instance->position.y + step); break;
            case Z_MINUS: instance->position.z = fmaxf(-100, instance->position.z - step); break;
            case Z_PLUS:  instance->position.z = fminf(100, instance->position.z + step); break;
            case TURN_LEFT:  instance->yawDegrees = fmodf(instance->yawDegrees - 15, 360); break;
            case TURN_RIGHT: instance->yawDegrees = fmodf(instance->yawDegrees + 15, 360); break;
            case SMALLER: instance->scale = fmaxf(baseScale * 0.05f, instance->scale / 1.15f); break;
            case LARGER: instance->scale = fminf(baseScale * 20.0f, instance->scale * 1.15f); break;
            default: break;
        }
        editor->status = "Unsaved changes";
    }
}

static void PickObject(SceneEditor *editor, Vector2 pointer)
{
    Ray ray = GetScreenToWorldRay(pointer, GetEditorCamera(editor));
    float nearest = INFINITY;
    editor->selected = -1;
    for (int i = 0; i < editor->count; ++i)
    {
        SceneObject object = MakePrefabSceneObject(&editor->entries[i].instance);
        RayCollision hit = GetRayCollisionBox(ray, object.bounds);
        if (hit.hit && hit.distance >= 0 && hit.distance < nearest)
        {
            nearest = hit.distance;
            editor->selected = i;
        }
    }
}

bool UpdateSceneEditor(SceneEditor *editor, InputPointer pointer)
{
    editor->pointer = pointer.position;
    Rectangle panel = EditorPanel();
    bool onPanel = CheckCollisionPointRec(pointer.position, panel);
    if (pointer.pressed && onPanel)
    {
        for (int i = 0; i < COMMAND_COUNT; ++i)
        {
            if (!ReadUiButton(CommandBounds(panel, i), pointer, CommandEnabled(editor, i)).activated) continue;
            if (i == BACK) return false;
            ApplyCommand(editor, i);
            break;
        }
    }
    if (!onPanel)
        editor->distance = Clamp(editor->distance * powf(0.85f, pointer.wheel), 2.5f, 36);
    if (pointer.pressed && !onPanel)
    {
        editor->dragging = true;
        editor->moved = false;
        editor->dragStart = editor->dragLast = pointer.position;
    }
    if (editor->dragging && pointer.down)
    {
        if (Vector2DistanceSqr(editor->dragStart, pointer.position) > 16) editor->moved = true;
        if (editor->moved)
        {
            Vector2 delta = Vector2Subtract(pointer.position, editor->dragLast);
            editor->yaw -= delta.x * 0.008f;
            editor->pitch = Clamp(editor->pitch + delta.y * 0.008f, -1.35f, 1.35f);
        }
        editor->dragLast = pointer.position;
    }
    else if (editor->dragging)
    {
        if (!editor->moved) PickObject(editor, editor->dragLast);
        editor->dragging = false;
    }
    return true;
}

void DrawEditorSelection(const SceneEditor *editor)
{
    if (editor->selected < 0) return;
    SceneObject selected = MakePrefabSceneObject(&editor->entries[editor->selected].instance);
    DrawBoundingBox(selected.bounds, LIME);
}

void DrawEditorOverlay(const SceneEditor *editor)
{
    Rectangle panel = EditorPanel();
    DrawUiPanel(panel, (Color){ 15, 24, 35, 247 });
    DrawText("Scene Editor", (int)panel.x + 10, 10, 22, RAYWHITE);
    const char *selection = editor->selected >= 0
        ? TextFormat("Object %d of %d", editor->selected + 1, editor->count)
        : TextFormat("%d objects  /  none selected", editor->count);
    DrawText(selection, (int)panel.x + 10, 38, 14, LIGHTGRAY);
    InputPointer pointer = { .position = editor->pointer };
    for (int i = 0; i < COMMAND_COUNT; ++i)
    {
        Rectangle bounds = CommandBounds(panel, i);
        bool enabled = CommandEnabled(editor, i);
        DrawUiButton(bounds, labels[i], ReadUiButton(bounds, pointer, enabled), enabled);
    }
    int y = (int)(CommandBounds(panel, LARGER).y + CommandBounds(panel, LARGER).height + 8);
    if (editor->selected >= 0)
    {
        PrefabInstance instance = editor->entries[editor->selected].instance;
        DrawText(TextFormat("X %.2f   Y %.2f   Z %.2f", instance.position.x, instance.position.y, instance.position.z),
                 (int)panel.x + 10, y, 13, LIGHTGRAY);
        DrawText(TextFormat("Turn %.0f   Scale %.2f", instance.yawDegrees, instance.scale),
                 (int)panel.x + 10, y + 17, 13, LIGHTGRAY);
    }
    if (editor->status)
        DrawText(editor->status, (int)panel.x + 10, y + 34, 13, RAYWHITE);
    DrawText("Tap to select / Drag to orbit", 12, GetScreenHeight() - 23, 14, RAYWHITE);
}
