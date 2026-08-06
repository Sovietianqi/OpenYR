#include "Audio.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

// ============================================================================
// WAV FourCC helpers
// ============================================================================

namespace {
    constexpr uint32 FourCC_RIFF = 0x46464952; // "RIFF"
    constexpr uint32 FourCC_WAVE = 0x45564157; // "WAVE"
    constexpr uint32 FourCC_fmt  = 0x20746D66; // "fmt "
    constexpr uint32 FourCC_data = 0x61746164; // "data"

    FORCEINLINE uint32 MakeFourCC(const char* s) {
        return static_cast<uint32>(s[0]) |
               (static_cast<uint32>(s[1]) << 8) |
               (static_cast<uint32>(s[2]) << 16) |
               (static_cast<uint32>(s[3]) << 24);
    }
}

// ============================================================================
// AudioBase
// ============================================================================

AudioBase::AudioBase() :
    SampleRate(22050),
    BitsPerSample(16),
    NumChannels(1),
    Volume(1.0f),
    Panning(0x2000),
    Muted(false)
{}

AudioBase::~AudioBase() {}

void AudioBase::SetVolume(float vol) {
    Volume = (vol < 0.0f) ? 0.0f : ((vol > 1.0f) ? 1.0f : vol);
}

void AudioBase::SetPanning(int32 pan) {
    Panning = (pan < 0) ? 0 : ((pan > 0x4000) ? 0x4000 : pan);
}

void AudioBase::SetMute(bool mute) {
    Muted = mute;
}

// ============================================================================
// AudioSample
// ============================================================================

AudioSample::AudioSample() :
    SampleData(nullptr),
    DataSize(0),
    CurrentPosition(0),
    Looping(false),
    Pitch(1.0f),
    Playing(false),
    Paused(false)
{
    SampleRate = 22050;
    BitsPerSample = 16;
    NumChannels = 1;
}

AudioSample::AudioSample(const AudioSampleData& sampleData) :
    SampleData(nullptr),
    DataSize(0),
    CurrentPosition(0),
    Looping(false),
    Pitch(1.0f),
    Playing(false),
    Paused(false)
{
    LoadFromMemory(sampleData);
}

AudioSample::~AudioSample() {
    Release();
}

bool AudioSample::LoadFromMemory(const AudioSampleData& sampleData) {
    Release();

    if (!sampleData.Data || sampleData.DataSize == 0) {
        return false;
    }

    SampleRate = sampleData.SampleRate;
    BitsPerSample = sampleData.BitsPerSample;
    NumChannels = sampleData.NumChannels;

    DataSize = sampleData.DataSize;
    SampleData = static_cast<uint8*>(YRMemory::Allocate(DataSize));
    if (!SampleData) {
        return false;
    }

    std::memcpy(SampleData, sampleData.Data, DataSize);
    CurrentPosition = 0;
    return true;
}

bool AudioSample::LoadFromFile(const char* filename) {
    AudioSampleData sampleData;
    if (!Audio_ReadWAVFile(filename, &sampleData)) {
        return false;
    }

    bool result = LoadFromMemory(sampleData);
    sampleData.Release();
    return result;
}

void AudioSample::Release() {
    Stop();
    if (SampleData) {
        YRMemory::Deallocate(SampleData);
        SampleData = nullptr;
    }
    DataSize = 0;
    CurrentPosition = 0;
}

bool AudioSample::Play() {
    // Validate that sample data has been loaded.
    if (!SampleData || DataSize == 0) {
        return false;
    }

    // Validate PCM format parameters: sample rate and channel count must
    // be non-zero, and the bit depth must be a supported PCM size (8 or 16).
    if (SampleRate == 0 || NumChannels == 0) {
        return false;
    }
    if (BitsPerSample != 8 && BitsPerSample != 16) {
        return false;
    }

    // Apply volume from the Volume field, clamped to the valid [0, 1] range.
    // A muted sample still tracks its playback position but produces no output.
    if (Volume < 0.0f) {
        Volume = 0.0f;
    } else if (Volume > 1.0f) {
        Volume = 1.0f;
    }

    if (Paused) {
        // Resume from pause: preserve CurrentPosition so playback continues
        // from the point where it was interrupted rather than restarting.
        Paused = false;
        Playing = true;
        return true;
    }

    // Fresh playback: rewind to the beginning of the sample buffer.
    CurrentPosition = 0;
    Playing = true;
    Paused = false;
    return true;
}

bool AudioSample::Stop() {
    Playing = false;
    Paused = false;
    CurrentPosition = 0;
    return true;
}

bool AudioSample::Pause() {
    // Can only pause a sample that is actively playing.
    if (!Playing) {
        return false;
    }

    // Store the current playback position for resume. CurrentPosition is a
    // persistent member field; Play() checks the Paused flag on re-entry and
    // skips the rewind when resuming, so the position is preserved across
    // the pause/resume cycle. Clamp to valid bounds to guard against overrun.
    if (CurrentPosition > DataSize) {
        CurrentPosition = DataSize;
    }

    Paused = true;
    Playing = false;
    return true;
}

bool AudioSample::IsPlaying() const {
    return Playing && !Paused;
}

void AudioSample::SetLooping(bool loop) {
    Looping = loop;
}

void AudioSample::SetPitch(float pitch) {
    Pitch = (pitch < 0.25f) ? 0.25f : ((pitch > 4.0f) ? 4.0f : pitch);
}

// ============================================================================
// AudioStream
// ============================================================================

AudioStream::AudioStream() :
    BufferedSize(0),
    CurrentBufferPosition(0),
    CurrentBuffer(nullptr),
    HasStreamThread(false)
{
    Streaming.store(false);
    StreamActive.store(false);
    ShouldStop.store(false);
    SampleRate = 22050;
    BitsPerSample = 16;
    NumChannels = 2;
}

AudioStream::~AudioStream() {
    StopStreamThread();
    Stop();

    // Clean up remaining buffers
    std::lock_guard<std::mutex> lock(BufferMutex);
    while (!BufferQueue.empty()) {
        AudioMiniBuffer* buf = BufferQueue.front();
        BufferQueue.pop();
        if (buf) {
            buf->Release();
            delete buf;
        }
    }
    if (CurrentBuffer) {
        CurrentBuffer->Release();
        delete CurrentBuffer;
        CurrentBuffer = nullptr;
    }
}

bool AudioStream::PlayWAV(const char* filename, bool useThread) {
    AudioSampleData sampleData;
    if (!Audio_ReadWAVFile(filename, &sampleData)) {
        return false;
    }

    EnqueueSampleData(sampleData);
    // EnqueueSampleData makes its own internal copy of the data, so release
    // the temporary buffer to avoid a memory leak.
    sampleData.Release();

    if (useThread) {
        StartStreamThread();
    }

    return Play();
}

bool AudioStream::PlayFromMemory(const uint8* data, uint32 size) {
    // Validate the data pointer and size.
    if (!data || size == 0) {
        return false;
    }

    // Ensure the buffer is large enough to contain at least the RIFF header
    // plus one fmt chunk and one data chunk header.
    if (size < sizeof(RIFFHeader) + sizeof(WAVFmtChunk) + sizeof(WAVDataChunkHeader)) {
        return false;
    }

    // Parse the RIFF header to confirm this is a WAVE stream.
    const RIFFHeader* riff = reinterpret_cast<const RIFFHeader*>(data);
    if (riff->ChunkID != FourCC_RIFF || riff->Format != FourCC_WAVE) {
        return false;
    }

    AudioSampleData sampleData;
    uint32 offset = sizeof(RIFFHeader);
    uint32 dataSize = 0;
    const uint8* sampleBuf = nullptr;
    bool foundFmt = false;

    // Walk the WAV sub-chunks, extracting the fmt and data sections.
    while (offset + 8 <= size) {
        const uint32* chunkID = reinterpret_cast<const uint32*>(data + offset);
        const uint32* chunkSize = reinterpret_cast<const uint32*>(data + offset + 4);

        if (*chunkID == FourCC_fmt) {
            // Validate the fmt chunk is large enough for the standard fields
            // before reading from it.
            if (*chunkSize >= 16 && offset + sizeof(WAVFmtChunk) <= size) {
                const WAVFmtChunk* fmt = reinterpret_cast<const WAVFmtChunk*>(data + offset);
                sampleData.SampleRate = fmt->SampleRate;
                sampleData.BitsPerSample = fmt->BitsPerSample;
                sampleData.NumChannels = fmt->NumChannels;
                sampleData.BlockAlign = fmt->BlockAlign;
                sampleData.ByteRate = fmt->ByteRate;
                sampleData.BytesPerSample = fmt->BitsPerSample / 8;
                sampleData.Format = fmt->AudioFormat;
                foundFmt = true;
            }
        } else if (*chunkID == FourCC_data) {
            dataSize = *chunkSize;
            sampleBuf = data + offset + sizeof(WAVDataChunkHeader);
            // Guard against a truncated or corrupt data chunk that claims
            // more bytes than the buffer actually contains.
            if (sampleBuf + dataSize > data + size) {
                dataSize = static_cast<uint32>((data + size) - sampleBuf);
            }
        }

        // Advance past this chunk, accounting for word-alignment padding
        // (WAV chunks are padded to even byte boundaries).
        offset += 8 + *chunkSize + (*chunkSize & 1);
    }

    // Require both a valid fmt chunk and a non-empty data chunk.
    if (!foundFmt || dataSize == 0 || !sampleBuf) {
        return false;
    }

    // Validate the parsed PCM format parameters.
    if (sampleData.SampleRate == 0 || sampleData.NumChannels == 0) {
        return false;
    }
    if (sampleData.BitsPerSample != 8 && sampleData.BitsPerSample != 16) {
        return false;
    }

    // Set up the streaming buffer: copy the PCM data into an owned allocation.
    sampleData.Data = static_cast<uint8*>(YRMemory::Allocate(dataSize));
    if (!sampleData.Data) {
        return false;
    }
    std::memcpy(sampleData.Data, sampleBuf, dataSize);
    sampleData.DataSize = dataSize;

    // Enqueue the sample data into the stream buffer queue.
    // EnqueueSampleData makes its own internal copy, so release our
    // temporary buffer to avoid a memory leak.
    EnqueueSampleData(sampleData);
    sampleData.Release();

    // Start playback.
    return Play();
}

bool AudioStream::Play() {
    // Verify there is buffered audio ready to stream before starting.
    {
        std::lock_guard<std::mutex> lock(BufferMutex);
        if (BufferQueue.empty() && !CurrentBuffer) {
            // Nothing queued and no active buffer; cannot start playback.
            return false;
        }
    }

    // Start (or resume) streaming playback.
    StreamActive.store(true);
    Streaming.store(true);
    ShouldStop.store(false);
    return true;
}

bool AudioStream::Stop() {
    ShouldStop.store(true);
    Streaming.store(false);
    StreamActive.store(false);

    {
        std::lock_guard<std::mutex> lock(BufferMutex);
        while (!BufferQueue.empty()) {
            AudioMiniBuffer* buf = BufferQueue.front();
            BufferQueue.pop();
            if (buf) {
                buf->Release();
                delete buf;
            }
        }
        if (CurrentBuffer) {
            CurrentBuffer->Release();
            delete CurrentBuffer;
            CurrentBuffer = nullptr;
        }
        BufferedSize = 0;
        CurrentBufferPosition = 0;
    }

    return true;
}

bool AudioStream::Pause() {
    // Cannot pause a stream that is not active or has been stopped.
    if (!StreamActive.load() || ShouldStop.load()) {
        return false;
    }

    // Halt buffer consumption while keeping the stream alive so it can be
    // distinguished from a fully stopped stream and resumed later.
    // CurrentBuffer and CurrentBufferPosition are left untouched so that
    // playback resumes seamlessly from the paused point.
    Streaming.store(false);
    return true;
}

bool AudioStream::IsPlaying() const {
    return Streaming.load() && !ShouldStop.load();
}

void AudioStream::EnqueueBuffer(AudioMiniBuffer* buffer) {
    if (!buffer || !buffer->IsValid) return;

    std::lock_guard<std::mutex> lock(BufferMutex);
    BufferQueue.push(buffer);
    BufferedSize += buffer->Size;
}

void AudioStream::EnqueueSampleData(const AudioSampleData& sampleData) {
    if (!sampleData.Data || sampleData.DataSize == 0) return;

    uint8* buf = static_cast<uint8*>(YRMemory::Allocate(sampleData.DataSize));
    if (!buf) return;

    std::memcpy(buf, sampleData.Data, sampleData.DataSize);
    AudioMiniBuffer* miniBuf = new AudioMiniBuffer(buf, sampleData.DataSize);

    EnqueueBuffer(miniBuf);

    SampleRate = sampleData.SampleRate;
    BitsPerSample = sampleData.BitsPerSample;
    NumChannels = sampleData.NumChannels;
}

uint32 AudioStream::GetBufferedCount() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(BufferMutex));
    return static_cast<uint32>(BufferQueue.size());
}

uint32 AudioStream::GetTotalBufferedSize() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(BufferMutex));
    return BufferedSize;
}

void AudioStream::SetStreamVolume(float vol) {
    SetVolume(vol);
}

void AudioStream::SetStreamPanning(int32 pan) {
    SetPanning(pan);
}

void AudioStream::StartStreamThread() {
    if (HasStreamThread) return;
    HasStreamThread = true;
    StreamThread = std::thread(&AudioStream::StreamWorker, this);
}

void AudioStream::StopStreamThread() {
    ShouldStop.store(true);
    if (HasStreamThread && StreamThread.joinable()) {
        StreamThread.join();
    }
    HasStreamThread = false;
}

bool AudioStream::IsStreamThreadRunning() const {
    return HasStreamThread && Streaming.load();
}

void AudioStream::StreamWorker() {
    while (!ShouldStop.load()) {
        if (Streaming.load()) {
            ProcessStreamBuffer();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool AudioStream::ProcessStreamBuffer() {
    std::lock_guard<std::mutex> lock(BufferMutex);

    if (CurrentBuffer && CurrentBuffer->IsDone()) {
        CurrentBuffer->Release();
        delete CurrentBuffer;
        CurrentBuffer = nullptr;
        CurrentBufferPosition = 0;
    }

    if (!CurrentBuffer && !BufferQueue.empty()) {
        CurrentBuffer = BufferQueue.front();
        BufferQueue.pop();
        CurrentBufferPosition = 0;
        if (CurrentBuffer) {
            BufferedSize -= CurrentBuffer->Size;
        }
    }

    if (!CurrentBuffer) {
        return false;
    }

    // Simulate buffer consumption (~10ms worth of audio)
    uint32 bytesPerFrame = (SampleRate * (BitsPerSample / 8) * NumChannels) / 100;
    uint32 consumed = CurrentBuffer->Read(nullptr, bytesPerFrame);
    CurrentBufferPosition += consumed;

    return true;
}

// ============================================================================
// AudioManager
// ============================================================================

AudioManager* AudioManager::s_Instance = nullptr;

AudioManager* AudioManager::Instance() {
    if (!s_Instance) {
        s_Instance = new AudioManager();
    }
    return s_Instance;
}

AudioManager::AudioManager() :
    MasterVolume(1.0f),
    SFXVolume(1.0f),
    MusicVolume(1.0f),
    Muted(false),
    Initialized(false)
{}

AudioManager::~AudioManager() {
    Shutdown();
}

bool AudioManager::Initialize() {
    if (Initialized) return true;

    MasterVolume = 1.0f;
    SFXVolume = 1.0f;
    MusicVolume = 1.0f;
    Muted = false;
    Initialized = true;

    return true;
}

void AudioManager::Shutdown() {
    StopAllSamples();
    StopAllStreams();

    Initialized = false;
}

AudioSample* AudioManager::LoadSample(const char* filename) {
    AudioSample* sample = new AudioSample();
    if (!sample->LoadFromFile(filename)) {
        delete sample;
        return nullptr;
    }
    ActiveSamples.Add(sample);
    return sample;
}

AudioSample* AudioManager::LoadSampleFromMemory(const AudioSampleData& data) {
    AudioSample* sample = new AudioSample(data);
    if (!sample->IsPlaying() && sample->GetTotalSamples() == 0) {
        delete sample;
        return nullptr;
    }
    ActiveSamples.Add(sample);
    return sample;
}

void AudioManager::ReleaseSample(AudioSample* sample) {
    if (!sample) return;
    for (int32 i = 0; i < ActiveSamples.Count; ++i) {
        if (ActiveSamples[i] == sample) {
            ActiveSamples.Remove(i);
            break;
        }
    }
    delete sample;
}

AudioStream* AudioManager::CreateStream() {
    AudioStream* stream = new AudioStream();
    ActiveStreams.Add(stream);
    return stream;
}

void AudioManager::ReleaseStream(AudioStream* stream) {
    if (!stream) return;
    for (int32 i = 0; i < ActiveStreams.Count; ++i) {
        if (ActiveStreams[i] == stream) {
            ActiveStreams.Remove(i);
            break;
        }
    }
    delete stream;
}

void AudioManager::SetMasterVolume(float vol) {
    MasterVolume = (vol < 0.0f) ? 0.0f : ((vol > 1.0f) ? 1.0f : vol);
}

void AudioManager::SetSFXVolume(float vol) {
    SFXVolume = (vol < 0.0f) ? 0.0f : ((vol > 1.0f) ? 1.0f : vol);
}

void AudioManager::SetMusicVolume(float vol) {
    MusicVolume = (vol < 0.0f) ? 0.0f : ((vol > 1.0f) ? 1.0f : vol);
}

void AudioManager::MuteAll(bool mute) {
    Muted = mute;
}

void AudioManager::PlaySample(AudioSample* sample, float volume, int32 panning) {
    if (!sample || !Initialized) return;
    if (Muted) return;

    float effectiveVolume = volume * MasterVolume * SFXVolume;
    sample->SetVolume(effectiveVolume);
    sample->SetPanning(panning);
    sample->Play();
}

void AudioManager::StopAllSamples() {
    for (int32 i = 0; i < ActiveSamples.Count; ++i) {
        if (ActiveSamples[i]) {
            ActiveSamples[i]->Stop();
        }
    }
}

void AudioManager::StopAllStreams() {
    for (int32 i = 0; i < ActiveStreams.Count; ++i) {
        if (ActiveStreams[i]) {
            ActiveStreams[i]->Stop();
        }
    }
}

bool AudioManager::ReadWAVFile(const char* filename, AudioSampleData* pSampleData) {
    return Audio_ReadWAVFile(filename, pSampleData);
}

bool AudioManager::ReadWAVFile(CCFileClass* pFile, AudioSampleData* pSampleData) {
    return Audio_ReadWAVFile(pFile, pSampleData);
}

// ============================================================================
// Audio_ReadWAVFile - WAV file parser
// ============================================================================

bool Audio_ReadWAVFile(const char* filename, AudioSampleData* pSampleData) {
    if (!filename || !pSampleData) return false;

    FILE* f = std::fopen(filename, "rb");
    if (!f) return false;

    bool result = false;

    RIFFHeader riffHeader;
    if (std::fread(&riffHeader, sizeof(RIFFHeader), 1, f) != 1) {
        std::fclose(f);
        return false;
    }

    if (riffHeader.ChunkID != FourCC_RIFF || riffHeader.Format != FourCC_WAVE) {
        std::fclose(f);
        return false;
    }

    bool foundFmt = false;
    bool foundData = false;

    while (!foundData) {
        uint32 chunkID;
        uint32 chunkSize;

        if (std::fread(&chunkID, sizeof(uint32), 1, f) != 1) break;
        if (std::fread(&chunkSize, sizeof(uint32), 1, f) != 1) break;

        if (chunkID == FourCC_fmt) {
            WAVFmtChunk fmtChunk;
            if (chunkSize > sizeof(WAVFmtChunk)) {
                std::fseek(f, static_cast<long>(chunkSize), SEEK_CUR);
                continue;
            }

            if (std::fread(&fmtChunk, chunkSize, 1, f) != 1) break;

            pSampleData->Format = fmtChunk.AudioFormat;
            pSampleData->NumChannels = fmtChunk.NumChannels;
            pSampleData->SampleRate = fmtChunk.SampleRate;
            pSampleData->ByteRate = fmtChunk.ByteRate;
            pSampleData->BlockAlign = fmtChunk.BlockAlign;
            pSampleData->BytesPerSample = fmtChunk.BitsPerSample / 8;
            foundFmt = true;
        } else if (chunkID == FourCC_data) {
            if (!foundFmt) {
                std::fclose(f);
                return false;
            }

            pSampleData->DataSize = chunkSize;
            pSampleData->Data = static_cast<uint8*>(YRMemory::Allocate(chunkSize));
            if (!pSampleData->Data) {
                std::fclose(f);
                return false;
            }

            if (std::fread(pSampleData->Data, 1, chunkSize, f) != chunkSize) {
                pSampleData->Release();
                std::fclose(f);
                return false;
            }

            foundData = true;
            result = true;
        } else {
            // Skip unknown chunks
            std::fseek(f, static_cast<long>(chunkSize), SEEK_CUR);
        }
    }

    std::fclose(f);
    return result;
}

bool Audio_ReadWAVFile(CCFileClass* pFile, AudioSampleData* pSampleData) {
    if (!pFile || !pSampleData) return false;

    // Read entire file into memory buffer
    int32 fileSize = pFile->GetSize();
    if (fileSize <= 0) return false;

    uint8* fileBuffer = static_cast<uint8*>(YRMemory::Allocate(static_cast<size_t>(fileSize)));
    if (!fileBuffer) return false;

    if (pFile->Read(fileBuffer, fileSize) != fileSize) {
        YRMemory::Deallocate(fileBuffer);
        return false;
    }

    RIFFHeader* riffHeader = reinterpret_cast<RIFFHeader*>(fileBuffer);
    if (riffHeader->ChunkID != FourCC_RIFF || riffHeader->Format != FourCC_WAVE) {
        YRMemory::Deallocate(fileBuffer);
        return false;
    }

    bool foundFmt = false;
    bool foundData = false;
    uint32 offset = sizeof(RIFFHeader);

    while (offset + 8 <= static_cast<uint32>(fileSize)) {
        uint32 chunkID = *reinterpret_cast<uint32*>(fileBuffer + offset);
        uint32 chunkSize = *reinterpret_cast<uint32*>(fileBuffer + offset + 4);

        if (chunkID == FourCC_fmt && chunkSize >= 16) {
            WAVFmtChunk* fmtChunk = reinterpret_cast<WAVFmtChunk*>(fileBuffer + offset);
            pSampleData->Format = fmtChunk->AudioFormat;
            pSampleData->NumChannels = fmtChunk->NumChannels;
            pSampleData->SampleRate = fmtChunk->SampleRate;
            pSampleData->ByteRate = fmtChunk->ByteRate;
            pSampleData->BlockAlign = fmtChunk->BlockAlign;
            pSampleData->BytesPerSample = fmtChunk->BitsPerSample / 8;
            foundFmt = true;
        } else if (chunkID == FourCC_data) {
            if (!foundFmt) {
                YRMemory::Deallocate(fileBuffer);
                return false;
            }

            pSampleData->DataSize = chunkSize;
            pSampleData->Data = static_cast<uint8*>(YRMemory::Allocate(chunkSize));
            if (!pSampleData->Data) {
                YRMemory::Deallocate(fileBuffer);
                return false;
            }

            std::memcpy(pSampleData->Data, fileBuffer + offset + 8, chunkSize);
            foundData = true;
            break;
        }

        offset += 8 + chunkSize;
    }

    YRMemory::Deallocate(fileBuffer);
    return foundData;
}