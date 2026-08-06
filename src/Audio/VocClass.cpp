#include "VocClass.h"
#include "../IO/CCFileClass.h"
#include "../IO/FileSystem.h"

#include <cstring>
#include <cstdlib>
#include <cmath>

// ============================================================
// VocClass
// ============================================================

VocClass::VocClass()
    : VocData(nullptr), VocSize(0), SampleRate(22050), Channels(1), BitsPerSample(16)
    , Duration(0), Format(VocType::PCM), IsLoaded(false), IsPlaying(false)
    , Volume(128), Pan(64), Frequency(22050), CurrentPosition(0)
    , LoopCount(0), CurrentLoop(0), IsLooping(false), Priority(128)
    , ChannelIndex(-1), Category(0), Is3D(false), SourcePosition(0, 0, 0)
    , MaxDistance(1000), MinDistance(100), RolloffFactor(1.0f)
    , DopplerFactor(1.0f), Pitch(1.0f), IsStreaming(false)
    , StreamBuffer(nullptr), StreamBufferSize(0), PlaybackTimer(0), PitchShift(0) {
}

VocClass::~VocClass() {
    Unload();
}

bool VocClass::Load(const char* filename) {
    if (!filename || !filename[0]) return false;

    Unload();

    CCFileClass file(filename);
    if (!file.Exists()) return false;

    int32 fileSize = file.Size();
    if (fileSize <= 0) return false;

    uint8* fileData = static_cast<uint8*>(std::malloc(fileSize));
    if (!fileData) return false;

    if (!file.Read(fileData, fileSize)) {
        std::free(fileData);
        return false;
    }

    bool result = false;
    if (IsWAVFile(fileData, fileSize)) {
        result = ParseWAV(fileData, fileSize);
    } else if (IsVOCFile(fileData, fileSize)) {
        result = ParseVOC(fileData, fileSize);
    } else {
        result = ParseRaw(fileData, fileSize);
    }

    std::free(fileData);
    return result;
}

bool VocClass::IsWAVFile(const uint8* data, int32 size) {
    if (size < 44) return false;
    return data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F';
}

bool VocClass::IsVOCFile(const uint8* data, int32 size) {
    if (size < 26) return false;
    return data[0] == 'C' && data[1] == 'r' && data[2] == 'e' && data[3] == 'a';
}

bool VocClass::ParseWAV(const uint8* data, int32 size) {
    if (size < 44) return false;

    int32 fmtChunkSize = data[16] | (data[17] << 8) | (data[18] << 16) | (data[19] << 24);
    int16 format = static_cast<int16>(data[20] | (data[21] << 8));
    Channels = static_cast<int16>(data[22] | (data[23] << 8));
    SampleRate = data[24] | (data[25] << 8) | (data[26] << 16) | (data[27] << 24);
    BitsPerSample = static_cast<int16>(data[34] | (data[35] << 8));

    int32 dataOffset = 44;
    int32 dataSize = data[40] | (data[41] << 8) | (data[42] << 16) | (data[43] << 24);

    if (dataOffset + dataSize > size) {
        dataSize = size - dataOffset;
    }

    if (format == 1) {
        Format = VocType::PCM;
    } else {
        Format = VocType::Raw;
    }

    return AllocateVocData(data + dataOffset, dataSize);
}

bool VocClass::ParseVOC(const uint8* data, int32 size) {
    if (size < 26) return false;

    SampleRate = 1000000 / (256 - data[24]);
    if (data[25] != 0) {
        SampleRate = 256000000 / (static_cast<int32>(data[25]) * (256 - data[24]));
    }
    Channels = 1;
    BitsPerSample = 8;
    Format = VocType::PCM;

    int32 dataOffset = 26;
    int32 dataSize = size - dataOffset;

    return AllocateVocData(data + dataOffset, dataSize);
}

bool VocClass::ParseRaw(const uint8* data, int32 size) {
    SampleRate = 22050;
    Channels = 1;
    BitsPerSample = 16;
    Format = VocType::Raw;

    return AllocateVocData(data, size);
}

bool VocClass::AllocateVocData(const uint8* data, int32 dataSize) {
    if (!data || dataSize <= 0) return false;

    VocData = static_cast<uint8*>(std::malloc(dataSize));
    if (!VocData) return false;

    std::memcpy(VocData, data, dataSize);
    VocSize = dataSize;
    Frequency = SampleRate;
    Duration = VocSize * 60 / (SampleRate * Channels * BitsPerSample / 8);
    IsLoaded = true;
    CurrentPosition = 0;
    return true;
}

void VocClass::Unload() {
    if (VocData) {
        std::free(VocData);
        VocData = nullptr;
    }
    if (StreamBuffer) {
        std::free(StreamBuffer);
        StreamBuffer = nullptr;
    }
    VocSize = 0;
    IsLoaded = false;
    IsPlaying = false;
    IsStreaming = false;
    CurrentPosition = 0;
    ChannelIndex = -1;
}

void VocClass::Play() {
    if (!IsLoaded) return;
    IsPlaying = true;
    CurrentPosition = 0;
    PlaybackTimer = 0;
    CurrentLoop = 0;
}

void VocClass::Stop() {
    IsPlaying = false;
    CurrentPosition = 0;
    PlaybackTimer = 0;
}

void VocClass::Pause() {
    IsPlaying = false;
}

void VocClass::Resume() {
    if (IsLoaded) {
        IsPlaying = true;
    }
}

void VocClass::Update() {
    if (!IsPlaying || !IsLoaded) return;

    ++PlaybackTimer;
    CurrentPosition = (PlaybackTimer * SampleRate * Channels * BitsPerSample / 8) / 60;

    if (CurrentPosition >= VocSize) {
        if (IsLooping && (LoopCount == 0 || CurrentLoop < LoopCount)) {
            CurrentPosition = 0;
            PlaybackTimer = 0;
            ++CurrentLoop;
        } else {
            Stop();
        }
    }
}

void VocClass::SetVolume(int32 volume) {
    Volume = volume;
    if (Volume < 0) Volume = 0;
    if (Volume > 255) Volume = 255;
}

void VocClass::SetPan(int32 pan) {
    Pan = pan;
    if (Pan < 0) Pan = 0;
    if (Pan > 128) Pan = 128;
}

void VocClass::SetFrequency(int32 frequency) {
    Frequency = frequency;
    if (Frequency < 100) Frequency = 100;
    if (Frequency > 48000) Frequency = 48000;
    Pitch = static_cast<float>(Frequency) / static_cast<float>(SampleRate);
}

void VocClass::SetPitch(float pitch) {
    Pitch = pitch;
    Frequency = static_cast<int32>(SampleRate * pitch);
    if (Frequency < 100) Frequency = 100;
    if (Frequency > 48000) Frequency = 48000;
}

void VocClass::SetLooping(bool looping, int32 count) {
    IsLooping = looping;
    LoopCount = count;
    CurrentLoop = 0;
}

void VocClass::SetPriority(int32 priority) {
    Priority = priority;
    if (Priority < 0) Priority = 0;
    if (Priority > 255) Priority = 255;
}

void VocClass::SetChannel(int32 channel) {
    ChannelIndex = channel;
}

void VocClass::SetCategory(int32 category) {
    Category = category;
}

void VocClass::Set3D(bool enabled) {
    Is3D = enabled;
}

void VocClass::Set3DPosition(int32 x, int32 y, int32 z) {
    SourcePosition.X = x;
    SourcePosition.Y = y;
    SourcePosition.Z = z;
}

void VocClass::Set3DDistance(int32 minDist, int32 maxDist) {
    MinDistance = minDist;
    MaxDistance = maxDist;
    if (MinDistance < 0) MinDistance = 0;
    if (MaxDistance < MinDistance) MaxDistance = MinDistance;
}

void VocClass::Calculate3DVolumeAndPan(const CoordStruct& listenerPos) {
    if (!Is3D) return;

    float dx = static_cast<float>(SourcePosition.X - listenerPos.X);
    float dy = static_cast<float>(SourcePosition.Y - listenerPos.Y);
    float dz = static_cast<float>(SourcePosition.Z - listenerPos.Z);
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    float volume = 1.0f;
    if (distance > MinDistance) {
        if (distance >= MaxDistance) {
            volume = 0.0f;
        } else {
            float t = (distance - MinDistance) / (MaxDistance - MinDistance);
            volume = 1.0f / (1.0f + RolloffFactor * t);
        }
    }

    Volume = static_cast<int32>(volume * 255.0f);
    if (Volume > 255) Volume = 255;
    if (Volume < 0) Volume = 0;

    if (distance > 0.001f) {
        float pan = (dx / distance) * 0.5f + 0.5f;
        Pan = static_cast<int32>(pan * 128.0f);
        if (Pan < 0) Pan = 0;
        if (Pan > 128) Pan = 128;
    } else {
        Pan = 64;
    }

    float doppler = 1.0f;
    if (DopplerFactor > 0.0f) {
        doppler = 1.0f + DopplerFactor * 0.01f;
    }
    Frequency = static_cast<int32>(SampleRate * Pitch * doppler);
    if (Frequency < 100) Frequency = 100;
    if (Frequency > 48000) Frequency = 48000;
}

void VocClass::SetStreaming(bool streaming) {
    IsStreaming = streaming;
    if (streaming) {
        StreamBufferSize = 16384;
        if (StreamBuffer) {
            std::free(StreamBuffer);
        }
        StreamBuffer = static_cast<uint8*>(std::malloc(StreamBufferSize));
    }
}

int32 VocClass::GetSampleRate() const {
    return SampleRate;
}

int32 VocClass::GetChannels() const {
    return Channels;
}

int32 VocClass::GetBitsPerSample() const {
    return BitsPerSample;
}

int32 VocClass::GetDuration() const {
    return Duration;
}

int32 VocClass::GetCurrentPosition() const {
    return CurrentPosition;
}

int32 VocClass::GetVolume() const {
    return Volume;
}

int32 VocClass::GetPan() const {
    return Pan;
}

int32 VocClass::GetFrequency() const {
    return Frequency;
}

bool VocClass::IsLoadedVoc() const {
    return IsLoaded;
}

bool VocClass::IsPlayingVoc() const {
    return IsPlaying;
}

bool VocClass::Is3DVoc() const {
    return Is3D;
}

// ============================================================
// VocManagerClass
// ============================================================

static VocManagerClass* g_VocManagerInstance = nullptr;

VocManagerClass::VocManagerClass()
    : ActiveCount(0), ChannelCount(MAX_VOC_CHANNELS), GlobalVolume(255), GlobalMute(false)
    , MasterVolume(255), SFXVolume(255), SpeechVolume(255), AmbientVolume(255)
    , Enable3D(true), ListenerPosition(0, 0, 0), DopplerScale(1.0f)
    , RolloffScale(1.0f), DistanceFactor(1.0f) {
    for (int32 i = 0; i < MAX_VOC_CHANNELS; ++i) {
        Channels[i].Voc = nullptr;
        Channels[i].Priority = 0;
        Channels[i].IsActive = false;
    }
}

VocManagerClass::~VocManagerClass() {
    StopAll();
}

VocManagerClass* VocManagerClass::GetInstance() {
    if (!g_VocManagerInstance) {
        g_VocManagerInstance = new VocManagerClass();
    }
    return g_VocManagerInstance;
}

int32 VocManagerClass::Play(VocClass* voc, int32 priority) {
    if (!voc || !voc->IsLoaded) return -1;
    if (GlobalMute) return -1;

    int32 channel = AllocateChannel(priority);
    if (channel < 0) {
        channel = FindLowestPriorityChannel();
        if (channel < 0) return -1;
        if (Channels[channel].Priority > priority) return -1;
        Stop(channel);
    }

    Channels[channel].Voc = voc;
    Channels[channel].Priority = priority;
    Channels[channel].IsActive = true;
    Channels[channel].Volume = voc->Volume;
    Channels[channel].Pan = voc->Pan;
    Channels[channel].Frequency = voc->Frequency;

    voc->SetChannel(channel);
    voc->Play();
    ++ActiveCount;
    return channel;
}

int32 VocManagerClass::PlayFile(const char* filename, int32 priority) {
    VocClass* voc = new VocClass();
    if (!voc->Load(filename)) {
        delete voc;
        return -1;
    }

    int32 channel = Play(voc, priority);
    return channel;
}

void VocManagerClass::Stop(int32 channel) {
    if (channel < 0 || channel >= MAX_VOC_CHANNELS) return;
    if (Channels[channel].Voc) {
        Channels[channel].Voc->Stop();
        Channels[channel].Voc = nullptr;
    }
    Channels[channel].IsActive = false;
    Channels[channel].Priority = 0;
    --ActiveCount;
}

void VocManagerClass::StopAll() {
    for (int32 i = 0; i < MAX_VOC_CHANNELS; ++i) {
        if (Channels[i].IsActive) {
            Stop(i);
        }
    }
    ActiveCount = 0;
}

void VocManagerClass::PauseAll() {
    for (int32 i = 0; i < MAX_VOC_CHANNELS; ++i) {
        if (Channels[i].IsActive && Channels[i].Voc) {
            Channels[i].Voc->Pause();
        }
    }
}

void VocManagerClass::ResumeAll() {
    for (int32 i = 0; i < MAX_VOC_CHANNELS; ++i) {
        if (Channels[i].IsActive && Channels[i].Voc) {
            Channels[i].Voc->Resume();
        }
    }
}

void VocManagerClass::UpdateAll() {
    for (int32 i = 0; i < MAX_VOC_CHANNELS; ++i) {
        if (Channels[i].IsActive && Channels[i].Voc) {
            Channels[i].Voc->Update();
            if (!Channels[i].Voc->IsPlaying) {
                Stop(i);
            } else if (Channels[i].Voc->Is3D) {
                Channels[i].Voc->Calculate3DVolumeAndPan(ListenerPosition);
                Channels[i].Volume = Channels[i].Voc->Volume;
                Channels[i].Pan = Channels[i].Voc->Pan;
            }
        }
    }
}

void VocManagerClass::SetVolume(int32 channel, int32 volume) {
    if (channel < 0 || channel >= MAX_VOC_CHANNELS) return;
    Channels[channel].Volume = volume;
    if (Channels[channel].Voc) {
        Channels[channel].Voc->SetVolume(volume);
    }
}

void VocManagerClass::SetPan(int32 channel, int32 pan) {
    if (channel < 0 || channel >= MAX_VOC_CHANNELS) return;
    Channels[channel].Pan = pan;
    if (Channels[channel].Voc) {
        Channels[channel].Voc->SetPan(pan);
    }
}

void VocManagerClass::SetFrequency(int32 channel, int32 frequency) {
    if (channel < 0 || channel >= MAX_VOC_CHANNELS) return;
    Channels[channel].Frequency = frequency;
    if (Channels[channel].Voc) {
        Channels[channel].Voc->SetFrequency(frequency);
    }
}

void VocManagerClass::SetGlobalVolume(int32 volume) {
    GlobalVolume = volume;
    if (GlobalVolume < 0) GlobalVolume = 0;
    if (GlobalVolume > 255) GlobalVolume = 255;
}

void VocManagerClass::SetMasterVolume(int32 volume) {
    MasterVolume = volume;
    if (MasterVolume < 0) MasterVolume = 0;
    if (MasterVolume > 255) MasterVolume = 255;
}

void VocManagerClass::SetSFXVolume(int32 volume) {
    SFXVolume = volume;
    if (SFXVolume < 0) SFXVolume = 0;
    if (SFXVolume > 255) SFXVolume = 255;
}

void VocManagerClass::SetSpeechVolume(int32 volume) {
    SpeechVolume = volume;
    if (SpeechVolume < 0) SpeechVolume = 0;
    if (SpeechVolume > 255) SpeechVolume = 255;
}

void VocManagerClass::SetAmbientVolume(int32 volume) {
    AmbientVolume = volume;
    if (AmbientVolume < 0) AmbientVolume = 0;
    if (AmbientVolume > 255) AmbientVolume = 255;
}

void VocManagerClass::SetMute(bool mute) {
    GlobalMute = mute;
}

void VocManagerClass::SetListenerPosition(int32 x, int32 y, int32 z) {
    ListenerPosition.X = x;
    ListenerPosition.Y = y;
    ListenerPosition.Z = z;
}

void VocManagerClass::Set3DSettings(float dopplerScale, float rolloffScale, float distanceFactor) {
    DopplerScale = dopplerScale;
    RolloffScale = rolloffScale;
    DistanceFactor = distanceFactor;
}

int32 VocManagerClass::AllocateChannel(int32 priority) {
    for (int32 i = 0; i < ChannelCount; ++i) {
        if (!Channels[i].IsActive) {
            return i;
        }
    }
    return -1;
}

int32 VocManagerClass::FindLowestPriorityChannel() {
    int32 lowestPriority = 256;
    int32 lowestChannel = -1;
    for (int32 i = 0; i < ChannelCount; ++i) {
        if (Channels[i].IsActive && Channels[i].Priority < lowestPriority) {
            lowestPriority = Channels[i].Priority;
            lowestChannel = i;
        }
    }
    return lowestChannel;
}

int32 VocManagerClass::GetActiveCount() const {
    return ActiveCount;
}

bool VocManagerClass::IsPlaying(int32 channel) const {
    if (channel < 0 || channel >= MAX_VOC_CHANNELS) return false;
    return Channels[channel].IsActive && Channels[channel].Voc &&
           Channels[channel].Voc->IsPlaying;
}

VocClass* VocManagerClass::GetChannelVoc(int32 channel) const {
    if (channel < 0 || channel >= MAX_VOC_CHANNELS) return nullptr;
    return Channels[channel].Voc;
}