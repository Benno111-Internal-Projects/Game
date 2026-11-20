#ifndef RENDER_FRAMEWORK_SCENE_MUSIC_H
#define RENDER_FRAMEWORK_SCENE_MUSIC_H

#include "raylib.h"

typedef struct SceneMusicSettings {
    const char *path; // NULL or empty leaves the scene silent
    float volume;    // 0 to 1
    bool loop;
} SceneMusicSettings;

typedef struct SceneMusic {
    Music track;
    float volume;
    bool muted;
} SceneMusic;

// Zero-initialize the player and open the raylib audio device before loading.
// Starts the new track. If it cannot be loaded, any current track keeps playing.
bool SetSceneMusic(SceneMusic *player, SceneMusicSettings settings);

// Call once per update. Muting leaves the playback position advancing.
void UpdateSceneMusic(SceneMusic *player, bool muted);
void UnloadSceneMusic(SceneMusic *player);

#endif
