#include "scene_music.h"
#include "raymath.h"

#include <math.h>

void UnloadSceneMusic(SceneMusic *player)
{
    if (IsMusicValid(player->track))
    {
        StopMusicStream(player->track);
        UnloadMusicStream(player->track);
    }
    *player = (SceneMusic){ 0 };
}

bool SetSceneMusic(SceneMusic *player, SceneMusicSettings settings)
{
    const bool muted = player->muted;
    if (!settings.path || !settings.path[0])
    {
        UnloadSceneMusic(player);
        player->muted = muted;
        return true;
    }
    if (!IsAudioDeviceReady()) return false;

    Music track = LoadMusicStream(settings.path);
    if (!IsMusicValid(track))
    {
        if (track.ctxData || track.stream.buffer) UnloadMusicStream(track);
        return false;
    }

    UnloadSceneMusic(player);
    player->track = track;
    player->track.looping = settings.loop;
    player->volume = isfinite(settings.volume) ? Clamp(settings.volume, 0.0f, 1.0f) : 0.0f;
    player->muted = muted;
    SetMusicVolume(player->track, muted ? 0.0f : player->volume);
    PlayMusicStream(player->track);
    UpdateMusicStream(player->track);
    return true;
}

void UpdateSceneMusic(SceneMusic *player, bool muted)
{
    if (IsMusicValid(player->track))
    {
        if (muted != player->muted) SetMusicVolume(player->track, muted ? 0.0f : player->volume);
        UpdateMusicStream(player->track);
    }
    player->muted = muted;
}
