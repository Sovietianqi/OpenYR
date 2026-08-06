#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"

enum class VocType : int32 {
    PCM = 0,
    Raw = 1,
    ADPCM = 2,
    MP3 = 3
};

static constexpr int32 MAX_VOC_CHANNELS = 32;

struct VocChannel {
    class VocClass* Voc;
    int32 Priority;
    int32 Volume;
    int32 Pan;
    int32 Frequency;
    bool IsActive;
};

class VocClass {
public:
    VocClass();
    ~VocClass();

    bool Load(const char* filename);
    bool IsWAVFile(const uint8* data, int32 size);
    bool IsVOCFile(const uint8* data, int32 size);
    bool ParseWAV(const uint8* data, int32 size);
    bool ParseVOC(const uint8* data, int32 size);
    bool ParseRaw(const uint8* data, int32 size);
    bool AllocateVocData(const uint8* data, int32 dataSize);
    void Unload();

    void Play();
    void Stop();
    void Pause();
    void Resume();
    void Update();

    void SetVolume(int32 volume);
    void SetPan(int32 pan);
    void SetFrequency(int32 frequency);
    void SetPitch(float pitch);
    void SetLooping(bool looping, int32 count);
    void SetPriority(int32 priority);
    void SetChannel(int32 channel);
    void SetCategory(int32 category);
    void Set3D(bool enabled);
    void Set3DPosition(int32 x, int32 y, int32 z);
    void Set3DDistance(int32 minDist, int32 maxDist);
    void Calculate3DVolumeAndPan(const CoordStruct& listenerPos);
    void SetStreaming(bool streaming);

    int32 GetSampleRate() const;
    int32 GetChannels() const;
    int32 GetBitsPerSample() const;
    int32 GetDuration() const;
    int32 GetCurrentPosition() const;
    int32 GetVolume() const;
    int32 GetPan() const;
    int32 GetFrequency() const;
    bool IsLoadedVoc() const;
    bool IsPlayingVoc() const;
    bool Is3DVoc() const;

    uint8* VocData;
    int32 VocSize;
    int32 SampleRate;
    int32 Channels;
    int32 BitsPerSample;
    int32 Duration;
    VocType Format;
    bool IsLoaded;
    bool IsPlaying;
    int32 Volume;
    int32 Pan;
    int32 Frequency;
    int32 CurrentPosition;
    int32 LoopCount;
    int32 CurrentLoop;
    bool IsLooping;
    int32 Priority;
    int32 ChannelIndex;
    int32 Category;
    bool Is3D;
    CoordStruct SourcePosition;
    int32 MaxDistance;
    int32 MinDistance;
    float RolloffFactor;
    float DopplerFactor;
    float Pitch;
    bool IsStreaming;
    uint8* StreamBuffer;
    int32 StreamBufferSize;
    int32 PlaybackTimer;
    int32 PitchShift;
};

class VocManagerClass {
public:
    VocManagerClass();
    ~VocManagerClass();

    static VocManagerClass* GetInstance();

    int32 Play(VocClass* voc, int32 priority);
    int32 PlayFile(const char* filename, int32 priority);
    void Stop(int32 channel);
    void StopAll();
    void PauseAll();
    void ResumeAll();
    void UpdateAll();

    void SetVolume(int32 channel, int32 volume);
    void SetPan(int32 channel, int32 pan);
    void SetFrequency(int32 channel, int32 frequency);
    void SetGlobalVolume(int32 volume);
    void SetMasterVolume(int32 volume);
    void SetSFXVolume(int32 volume);
    void SetSpeechVolume(int32 volume);
    void SetAmbientVolume(int32 volume);
    void SetMute(bool mute);
    void SetListenerPosition(int32 x, int32 y, int32 z);
    void Set3DSettings(float dopplerScale, float rolloffScale, float distanceFactor);

    int32 AllocateChannel(int32 priority);
    int32 FindLowestPriorityChannel();
    int32 GetActiveCount() const;
    bool IsPlaying(int32 channel) const;
    VocClass* GetChannelVoc(int32 channel) const;

    int32 ActiveCount;
    int32 ChannelCount;
    VocChannel Channels[MAX_VOC_CHANNELS];
    int32 GlobalVolume;
    bool GlobalMute;
    int32 MasterVolume;
    int32 SFXVolume;
    int32 SpeechVolume;
    int32 AmbientVolume;
    bool Enable3D;
    CoordStruct ListenerPosition;
    float DopplerScale;
    float RolloffScale;
    float DistanceFactor;
};