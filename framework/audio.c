#include "audio.h"
#include "raymath.h"

#include <math.h>

AudioVoiceOptions DefaultAudioVoiceOptions(void)
{
    return (AudioVoiceOptions){
        .volume = 1.0f,
        .pitch = 1.0f,
        .falloff = AUDIO_FALLOFF_INVERSE,
        .minDistance = 1.0f,
        .maxDistance = 24.0f,
        .rolloff = 2.0f,
        .stereoWidth = 1.0f,
    };
}

static float FiniteOr(float value, float fallback)
{
    return isfinite(value) ? value : fallback;
}

AudioMix CalculateAudioMix(AudioVoiceOptions options, Camera3D listener)
{
    AudioMix mix = {
        .gain = options.muted ? 0.0f : Clamp(FiniteOr(options.volume, 1.0f), 0.0f, 1.0f),
        .pan = Clamp(FiniteOr(options.pan, 0.0f), -1.0f, 1.0f),
    };
    if (!options.spatial) return mix;

    Vector3 offset = Vector3Subtract(options.position, listener.position);
    float distance = Vector3Length(offset);
    if (!isfinite(distance)) return (AudioMix){ .pan = mix.pan };
    if (options.falloff != AUDIO_FALLOFF_NONE)
    {
        float minDistance = fmaxf(FiniteOr(options.minDistance, 1.0f), 0.0f);
        float maxDistance = fmaxf(FiniteOr(options.maxDistance, 24.0f), minDistance);
        float t = 0.0f;
        if (distance > minDistance)
            t = (distance >= maxDistance) ? 1.0f : (distance - minDistance) / (maxDistance - minDistance);
        float falloff = 1.0f - t;
        if (options.falloff == AUDIO_FALLOFF_INVERSE)
            falloff /= 1.0f + Clamp(FiniteOr(options.rolloff, 2.0f), 0.0f, 16.0f) * t;
        mix.gain *= falloff;
    }

    if (distance > 0.00001f)
    {
        Vector3 forward = Vector3Subtract(listener.target, listener.position);
        Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, listener.up));
        float side = Vector3DotProduct(offset, right) / distance;
        float width = Clamp(FiniteOr(options.stereoWidth, 1.0f), 0.0f, 1.0f);
        mix.pan = Clamp(mix.pan + FiniteOr(side, 0.0f) * width, -1.0f, 1.0f);
    }
    return mix;
}

AudioVoice CreateAudioVoice(Sound source)
{
    AudioVoice voice = { .options = DefaultAudioVoiceOptions() };
    if (IsAudioDeviceReady() && IsSoundValid(source)) voice.sound = LoadSoundAlias(source);
    return voice;
}

void DestroyAudioVoice(AudioVoice *voice)
{
    if (voice->sound.stream.buffer) UnloadSoundAlias(voice->sound);
    *voice = (AudioVoice){ 0 };
}

static void ApplyAudioMix(AudioVoice *voice, Camera3D listener)
{
    AudioMix mix = CalculateAudioMix(voice->options, listener);
    SetSoundVolume(voice->sound, mix.gain);
    // raylib 5.5 uses 1 for full left and 0 for full right.
    SetSoundPan(voice->sound, 0.5f * (1.0f - mix.pan));
    float pitch = Clamp(FiniteOr(voice->options.pitch, 1.0f), 0.125f, 4.0f);
    if (pitch != voice->appliedPitch)
    {
        SetSoundPitch(voice->sound, pitch);
        voice->appliedPitch = pitch;
    }
}

void PlayAudioVoice(AudioVoice *voice, AudioVoiceOptions options, Camera3D listener)
{
    if (!IsSoundValid(voice->sound)) return;
    StopSound(voice->sound);
    voice->options = options;
    ApplyAudioMix(voice, listener);
    PlaySound(voice->sound);
    voice->state = AUDIO_VOICE_PLAYING;
}

AudioPlaybackState GetAudioVoiceState(const AudioVoice *voice)
{
    if (voice->state == AUDIO_VOICE_PLAYING && !voice->options.loop && !IsSoundPlaying(voice->sound))
        return AUDIO_VOICE_STOPPED;
    return voice->state;
}

void PauseAudioVoice(AudioVoice *voice)
{
    if (GetAudioVoiceState(voice) != AUDIO_VOICE_PLAYING) return;
    PauseSound(voice->sound);
    voice->state = AUDIO_VOICE_PAUSED;
}

void ResumeAudioVoice(AudioVoice *voice)
{
    if (voice->state != AUDIO_VOICE_PAUSED) return;
    ResumeSound(voice->sound);
    voice->state = AUDIO_VOICE_PLAYING;
}

void StopAudioVoice(AudioVoice *voice)
{
    if (voice->state != AUDIO_VOICE_STOPPED) StopSound(voice->sound);
    voice->state = AUDIO_VOICE_STOPPED;
}

void UpdateAudioVoice(AudioVoice *voice, Camera3D listener)
{
    if (voice->state == AUDIO_VOICE_STOPPED) return;
    ApplyAudioMix(voice, listener);
    if (voice->state == AUDIO_VOICE_PAUSED || IsSoundPlaying(voice->sound)) return;
    if (voice->options.loop) PlaySound(voice->sound);
    else voice->state = AUDIO_VOICE_STOPPED;
}
