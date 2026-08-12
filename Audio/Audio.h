#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../IO/FileSystem.h"
#include "../IO/CCFileClass.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>

// ============================================================================
// WAV file format structures
// ============================================================================

#pragma pack(push, 1)

struct RIFFHeader {
    uint32 ChunkID;       // "RIFF"
    uint32 ChunkSize;
    uint32 Format;        // "WAVE"
};

struct WAVFmtChunk {
    uint32 SubchunkID;    // "fmt "
    uint32 SubchunkSize;
    uint16 AudioFormat;
    uint16 NumChannels;
    uint32 SampleRate;
    uint32 ByteRate;
    uint16 BlockAlign;
    uint16 BitsPerSample;
};

struct WAVDataChunkHeader {
    uint32 SubchunkID;   // "data"
    uint32 SubchunkSize;
};

#pragma pack(pop)

// ============================================================================
// AudioSampleData - stores WAV sample metadata
// ============================================================================

struct AudioSampleData {
    uint8* Data;
    uint32 Format;
    uint32 SampleRate;
    uint32 NumChannels;
    uint32 BitsPerSample;
    uint32 BytesPerSample;
    uint32 ByteRate;
    uint32 BlockAlign;
    uint32 Flags;
    uint32 DataSize;

    AudioSampleData() :
        Data(nullptr),
        Format(0),
        SampleRate(0),
        NumChannels(0),
        BitsPerSample(0),
        BytesPerSample(0),
        ByteRate(0),
        BlockAlign(0),
        Flags(0),
        DataSize(0)
    {}

    void Release() {
        if (Data) {
            YRMemory::Deallocate(Data);
            Data = nullptr;
        }
        DataSize = 0;
    }
};

// ============================================================================
// AudioMiniBuffer - tiny audio buffer for queue management
// ============================================================================

struct AudioMiniBuffer {
    uint8* Buffer;
    uint32 Size;
    uint32 Position;
    bool IsValid;

    AudioMiniBuffer() : Buffer(nullptr), Size(0), Position(0), IsValid(false) {}

    AudioMiniBuffer(uint8* buf, uint32 sz) :
        Buffer(buf), Size(sz), Position(0), IsValid(true)
    {}

    uint32 Read(uint8* dest, uint32 bytesToRead) {
        if (!IsValid || !Buffer || !dest) return 0;
        uint32 remaining = Size - Position;
        uint32 toRead = (bytesToRead < remaining) ? bytesToRead : remaining;
        std::memcpy(dest, Buffer + Position, toRead);
        Position += toRead;
        return toRead;
    }

    bool IsDone() const { return Position >= Size; }
    void Reset() { Position = 0; }
    void Release() {
        if (Buffer) {
            YRMemory::Deallocate(Buffer);
            Buffer = nullptr;
        }
        Size = 0;
        Position = 0;
        IsValid = false;
    }
};

// ============================================================================
// AudioBase - base class for audio playback
// ============================================================================

class AudioBase {
public:
    AudioBase();
    virtual ~AudioBase();

    virtual bool Play() = 0;
    virtual bool Stop() = 0;
    virtual bool Pause() = 0;
    virtual bool IsPlaying() const = 0;

    void SetVolume(float vol);
    float GetVolume() const { return Volume; }
    void SetPanning(int32 pan);
    int32 GetPanning() const { return Panning; }
    void SetMute(bool mute);
    bool IsMuted() const { return Muted; }

    uint32 GetSampleRate() const { return SampleRate; }
    uint32 GetBitsPerSample() const { return BitsPerSample; }
    uint32 GetNumChannels() const { return NumChannels; }

protected:
    uint32 SampleRate;
    uint32 BitsPerSample;
    uint32 NumChannels;
    float Volume;
    int32 Panning;
    bool Muted;
};

// ============================================================================
// AudioSample - WAV sample playback
// ============================================================================

class AudioSample : public AudioBase {
public:
    AudioSample();
    explicit AudioSample(const AudioSampleData& sampleData);
    virtual ~AudioSample();

    bool LoadFromMemory(const AudioSampleData& sampleData);
    bool LoadFromFile(const char* filename);
    void Release();

    virtual bool Play() override;
    virtual bool Stop() override;
    virtual bool Pause() override;
    virtual bool IsPlaying() const override;

    void SetLooping(bool loop);
    bool IsLooping() const { return Looping; }
    void SetPitch(float pitch);
    float GetPitch() const { return Pitch; }

    uint32 GetCurrentPosition() const { return CurrentPosition; }
    uint32 GetTotalSamples() const { return DataSize; }
    bool IsEnded() const { return CurrentPosition >= DataSize && !Looping; }

private:
    uint8* SampleData;
    uint32 DataSize;
    uint32 CurrentPosition;
    bool Looping;
    float Pitch;
    bool Playing;
    bool Paused;
};

// ============================================================================
// AudioStream - streaming audio with buffer queue and threading
// ============================================================================

class AudioStream : public AudioBase {
public:
    AudioStream();
    virtual ~AudioStream();

    bool PlayWAV(const char* filename, bool useThread = true);
    bool PlayFromMemory(const uint8* data, uint32 size);

    virtual bool Play() override;
    virtual bool Stop() override;
    virtual bool Pause() override;
    virtual bool IsPlaying() const override;

    void EnqueueBuffer(AudioMiniBuffer* buffer);
    void EnqueueSampleData(const AudioSampleData& sampleData);
    uint32 GetBufferedCount() const;
    uint32 GetTotalBufferedSize() const;

    void SetStreamVolume(float vol);
    void SetStreamPanning(int32 pan);

    void StartStreamThread();
    void StopStreamThread();
    bool IsStreamThreadRunning() const;

private:
    void StreamWorker();
    bool ProcessStreamBuffer();

    std::mutex BufferMutex;
    std::queue<AudioMiniBuffer*> BufferQueue;
    std::atomic<bool> Streaming;
    std::atomic<bool> StreamActive;
    std::atomic<bool> ShouldStop;
    std::thread StreamThread;
    uint32 BufferedSize;
    uint32 CurrentBufferPosition;
    AudioMiniBuffer* CurrentBuffer;
    bool HasStreamThread;
};

// ============================================================================
// AudioManager - singleton managing all audio playback
// ============================================================================

class AudioManager {
public:
    static AudioManager* Instance();

    bool Initialize();
    void Shutdown();

    // Sample management
    AudioSample* LoadSample(const char* filename);
    AudioSample* LoadSampleFromMemory(const AudioSampleData& data);
    void ReleaseSample(AudioSample* sample);

    // Stream management
    AudioStream* CreateStream();
    void ReleaseStream(AudioStream* stream);

    // Global controls
    void SetMasterVolume(float vol);
    float GetMasterVolume() const { return MasterVolume; }
    void SetSFXVolume(float vol);
    float GetSFXVolume() const { return SFXVolume; }
    void SetMusicVolume(float vol);
    float GetMusicVolume() const { return MusicVolume; }
    void MuteAll(bool mute);
    bool IsMuted() const { return Muted; }

    // Playback
    void PlaySample(AudioSample* sample, float volume = 1.0f, int32 panning = 0x2000);
    void StopAllSamples();
    void StopAllStreams();

    // WAV file reading
    static bool ReadWAVFile(const char* filename, AudioSampleData* pSampleData);
    static bool ReadWAVFile(CCFileClass* pFile, AudioSampleData* pSampleData);

private:
    AudioManager();
    ~AudioManager();
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    static AudioManager* s_Instance;

    DynamicVectorClass<AudioSample*> ActiveSamples;
    DynamicVectorClass<AudioStream*> ActiveStreams;
    float MasterVolume;
    float SFXVolume;
    float MusicVolume;
    bool Muted;
    bool Initialized;
};

// ============================================================================
// Free functions
// ============================================================================

bool Audio_ReadWAVFile(const char* filename, AudioSampleData* pSampleData);
bool Audio_ReadWAVFile(CCFileClass* pFile, AudioSampleData* pSampleData);