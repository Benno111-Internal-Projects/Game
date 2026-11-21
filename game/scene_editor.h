#ifndef BENNO111_SCENE_EDITOR_H
#define BENNO111_SCENE_EDITOR_H

#include "input.h"
#include "prefab.h"
#include "terrain.h"

enum { EDITOR_MAX_OBJECTS = 64, EDITOR_PREFAB_COUNT = 2 };

typedef struct EditorEntry {
    int kind;
    PrefabInstance instance;
} EditorEntry;

typedef enum TerrainBrush {
    TERRAIN_RAISE, TERRAIN_LOWER, TERRAIN_LEVEL, TERRAIN_ORBIT
} TerrainBrush;

typedef struct SceneEditor {
    const ScenePrefab *prefabs[EDITOR_PREFAB_COUNT];
    float defaultScales[EDITOR_PREFAB_COUNT];
    EditorEntry entries[EDITOR_MAX_OBJECTS];
    int count, selected;
    float floorHeight, yaw, pitch, distance;
    Vector2 pointer, dragStart, dragLast;
    bool dragging, moved;
    const char *status;
    TerrainMap terrain;
    TerrainBrush brush;
    float brushRadius;
    bool terrainMode, brushDown;
    RayCollision brushHit;
} SceneEditor;

void InitSceneEditor(SceneEditor *editor, const ScenePrefab *model, float scale,
                     const ScenePrefab *sphere, float floorHeight);
size_t GetEditedScene(const SceneEditor *editor, SceneObject objects[EDITOR_MAX_OBJECTS]);
Camera3D GetEditorCamera(const SceneEditor *editor);
bool UpdateSceneEditor(SceneEditor *editor, InputPointer pointer, float deltaTime);
void DrawEditorOverlay(const SceneEditor *editor);
void DrawEditorSelection(const SceneEditor *editor);

#endif
