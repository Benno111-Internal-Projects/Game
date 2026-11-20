#ifndef RENDER_FRAMEWORK_AUDIO_H
#define RENDER_FRAMEWORK_AUDIO_H

#include "raylib.h"

typedef enum AudioFalloff {
    AUDIO_FALLOFF_NONE,
    AUDIO_FALLOFF_LINEAR,
    AUDIO_FALLOFF_INVERSE
} AudioFalloff;

typedef enum AudioPlaybackState {
    AUDIO_VOICE_STOPPED,
    AUDIO_VOICE_PLAYING,
    AUDIO_VOICE_PAUSED
} AudioPlaybackState;

typedef struct AudioVoiceOptions {
    float volume;       // 0 to 1
    float pitch;        // 0.125 to 4.0, with 1.0 as original pitch
    float pan;          // -1 left, 0 center, +1 right
    bool loop;
    bool muted;
    bool spatial;
    Vector3 position;
    AudioFalloff falloff;
    float minDistance;  // Full gain through this distance
    float maxDistance;  // Silent from this distance when falloff is enabled
    float rolloff;      // Curvature for inverse falloff, 0 to 16
    float stereoWidth;  // Spatial panning amount, 0 to 1
} AudioVoiceOptions;

typedef struct AudioMix {
    float gain;
    float pan;          // -1 left, +1 right
} AudioMix;

typedef struct AudioVoice {
    Sound sound;
    AudioVoiceOptions options; // Editable on the game thread between updates
    AudioPlaybackState state;
    float appliedPitch;
} AudioVoice;

AudioVoiceOptions DefaultAudioVoiceOptions(void);

// Uses the camera as the listener. A 2D voice ignores its position and orientation.
AudioMix CalculateAudioMix(AudioVoiceOptions options, Camera3D listener);

// Initialize raylib audio and load the source Sound first. Each voice can play
// independently. Keep the source and audio device alive until all voices are destroyed.
// A voice owns its alias. Keep one owning instance and pass it by pointer.
AudioVoice CreateAudioVoice(Sound source);
void DestroyAudioVoice(AudioVoice *voice);
void PlayAudioVoice(AudioVoice *voice, AudioVoiceOptions options, Camera3D listener);
void PauseAudioVoice(AudioVoice *voice);
void ResumeAudioVoice(AudioVoice *voice);
void StopAudioVoice(AudioVoice *voice);
AudioPlaybackState GetAudioVoiceState(const AudioVoice *voice);

// Call once per game update for each voice to apply moving emitters, options,
// and loop restarts. Loops can have a gap of up to one game update plus audio buffering.
void UpdateAudioVoice(AudioVoice *voice, Camera3D listener);

#endif
