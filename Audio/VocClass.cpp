#include "VocClass.h"
#include "Audio.h"
#include "../IO/CCFileClass.h"
#include "../IO/FileSystem.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdlib>
#include <cmath>

// ============================================================
// SoundDefinition
// ============================================================

SoundDefinition::SoundDefinition()
    : Unk00(0), Flags(0), IsLoaded(1), Valid(true)
    , Control(0), Type(SoundType_Screen), Volume(0)
    , Priority(static_cast<int32>(SoundPriority::Normal))
    , Limit(3), Loop(0), Range(10)
    , MinVolume(0.0), VShift(0), SampleCount(0)
{
    Name[0] = '\0';
    Delay[0] = 0;
    Delay[1] = 0;
    FDelta[0] = 0;
    FDelta[1] = 0;

    for (int32 i = 0; i < MAX_SAMPLES_PER_SOUND; ++i)
        SampleIndex[i] = 0;
}

SoundDefinition::~SoundDefinition()
{
    SampleCount = 0;
}

const char* SoundDefinition::GetName() const
{
    return Name;
}

void SoundDefinition::SetName(const char* pName)
{
    if (!pName) return;

    std::strncpy(Name, pName, sizeof(Name) - 1);
    Name[sizeof(Name) - 1] = '\0';
}

// A sample name may be prefixed with "$$" or "##" to force the sample to be
// taken from one of the alternate sample directories; the prefixes are not
// part of the name stored in the sample table.
bool SoundDefinition::AddSample(const char* pName)
{
    if (!pName) return false;
    if (SampleCount >= MAX_SAMPLES_PER_SOUND) return false;

    while (*pName == '$' || *pName == '#')
        ++pName;

    int32 index = Audio_FindSampleIndex(pName);
    if (index == -1) {
        Valid = false;
        return true;
    }

    SampleIndex[SampleCount++] = index;
    return true;
}

void SoundDefinition::SetVolume(int32 volume)
{
    this->Volume = volume;
}

void SoundDefinition::SetVShift(int32 vshift)
{
    if (vshift > 100)      this->VShift = 100;
    else if (vshift < 0)   this->VShift = 0;
    else                   this->VShift = vshift;
}

void SoundDefinition::SetMinVolume(double minVolume)
{
    this->MinVolume = minVolume;
}

void SoundDefinition::SetPriority(SoundPriority priority)
{
    this->Priority = static_cast<int32>(priority);
}

void SoundDefinition::SetAttack(int32 attack)   { this->Flags  = attack; }
void SoundDefinition::SetDecay(int32 decay)     { this->IsLoaded = decay; }
void SoundDefinition::SetControl(int32 control) { this->Control = control; }
void SoundDefinition::SetType(int32 type)       { this->Type   = type; }
void SoundDefinition::SetLimit(int32 limit)     { this->Limit  = limit; }
void SoundDefinition::SetLoop(int32 loop)       { this->Loop   = loop; }
void SoundDefinition::SetRange(int32 range)     { this->Range  = range; }

void SoundDefinition::SetDelay(int32 delay, int32 delay2)
{
    Delay[0] = delay;
    Delay[1] = delay2;
}

void SoundDefinition::SetFDelta(int32 fdelta, int32 fdelta2)
{
    FDelta[0] = fdelta;
    FDelta[1] = fdelta2;
}

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


// ============================================================================
// Sound name registry
// ============================================================================
namespace
{
    const int32 MaxSoundNames = 4096;
    const int32 MaxSoundNameLen = 64;

    char*  g_SoundNames[MaxSoundNames] = {};
    int32  g_SoundNameCount = 0;
}

int32 VocClass::FindIndexOfName(const char* pName)
{
    if (!pName || !*pName) return -1;
    for (int32 i = 0; i < g_SoundNameCount; ++i)
    {
        if (g_SoundNames[i] && !_strcmpi(g_SoundNames[i], pName))
            return i;
    }
    return -1;
}

int32 VocClass::RegisterSoundName(const char* pName)
{
    if (!pName || !*pName) return -1;

    int32 existing = FindIndexOfName(pName);
    if (existing >= 0) return existing;

    if (g_SoundNameCount >= MaxSoundNames) return -1;

    int32 len = static_cast<int32>(strlen(pName));
    if (len > MaxSoundNameLen - 1) len = MaxSoundNameLen - 1;

    char* copy = new char[MaxSoundNameLen];
    if (!copy) return -1;

    memcpy(copy, pName, len);
    copy[len] = '\0';

    g_SoundNames[g_SoundNameCount] = copy;
    return g_SoundNameCount++;
}

const char* VocClass::GetSoundName(int32 index)
{
    if (index < 0 || index >= g_SoundNameCount) return nullptr;
    return g_SoundNames[index];
}

int32 VocClass::GetSoundCount()
{
    return g_SoundNameCount;
}

void VocClass::ClearSoundRegistry()
{
    for (int32 i = 0; i < g_SoundNameCount; ++i)
    {
        delete[] g_SoundNames[i];
        g_SoundNames[i] = nullptr;
    }
    g_SoundNameCount = 0;
}


// ============================================================================
// Sound definition list
// ============================================================================

namespace
{
    // The [Defaults] block of sound(md).ini sets these; every sound starts
    // from them before its own section is applied.
    const double Sound_Default_Volume     = 80.0;
    const double Sound_Default_MinVolume  = 20.0;
    const int32  Sound_Default_Control    = 0;
    const int32  Sound_Default_Type       = 0x20;

    struct SoundPriorityEntry { const char* Name; int32 Value; };
    struct SoundFlagEntry     { const char* Name; int32 Value; };

    const SoundPriorityEntry g_SoundPriorities[6] = {
        { "LOWEST",   static_cast<int32>(SoundPriority::Lowest)   },
        { "LOW",      static_cast<int32>(SoundPriority::Low)      },
        { "NORMAL",   static_cast<int32>(SoundPriority::Normal)   },
        { "HIGH",     static_cast<int32>(SoundPriority::High)     },
        { "CRITICAL", static_cast<int32>(SoundPriority::Critical) },
        { nullptr,    0                                           },
    };

    const SoundFlagEntry g_SoundControlFlags[9] = {
        { "ALL",       SoundControl_All       },
        { "LOOP",      SoundControl_Loop      },
        { "RANDOM",    SoundControl_Random    },
        { "PREDELAY",  SoundControl_Predelay  },
        { "INTERRUPT", SoundControl_Interrupt },
        { "ATTACK",    SoundControl_Attack    },
        { "DECAY",     SoundControl_Decay     },
        { "AMBIENT",   SoundControl_Ambient   },
        { nullptr,     0                      },
    };

    const SoundFlagEntry g_SoundTypeFlags[15] = {
        { "AMBIENT",   SoundType_Ambient  },
        { "VIOLENT",   SoundType_Violent  },
        { "MOVEMENT",  SoundType_Movement },
        { "QUIET",     SoundType_Quiet    },
        { "LOUD",      SoundType_Loud     },
        { "GLOBAL",    SoundType_Global   },
        { "SCREEN",    SoundType_Screen   },
        { "LOCAL",     SoundType_Local    },
        { "PLAYER",    SoundType_Player   },
        { "NORMAL",    SoundType_Normal   },
        { "GUN_SHY",   SoundType_GunShy   },
        { "NOISE_SHY", SoundType_NoiseShy },
        { "UNSHROUD",  SoundType_Unshroud },
        { "SHROUD",    SoundType_Shroud   },
        { nullptr,     0                  },
    };

    // Registry of every sound created from [SoundList].
    DynamicVectorClass<SoundDefinition*>* g_pSoundDefinitions = nullptr;

    SoundDefinition* FindExistingSound(const char* pName)
    {
        if (!g_pSoundDefinitions || !pName)
            return nullptr;

        for (int32 i = 0; i < g_pSoundDefinitions->Count; ++i) {
            SoundDefinition* pDef = (*g_pSoundDefinitions)[i];
            if (pDef && _strcmpi(pDef->GetName(), pName) == 0)
                return pDef;
        }
        return nullptr;
    }
}

double   VocClass::DefaultVolume    = Sound_Default_Volume;
double   VocClass::DefaultMinVolume = Sound_Default_MinVolume;
SoundPriority VocClass::DefaultPriority = SoundPriority::Normal;
int32    VocClass::DefaultControl   = Sound_Default_Control;
int32    VocClass::DefaultType      = Sound_Default_Type;
int32    VocClass::DefaultLimit     = 3;
int32    VocClass::DefaultRange     = 10;

// Walks the priority table looking for a case insensitive match and, when one
// is found, publishes its value through pMatched.  Returns the matched entry
// itself so the caller can tell a hit from a miss.
SoundPriority VocClass::ParseSoundPriority(SoundPriority nCurrent, const char* pValue,
                                           bool* pMatched)
{
    const SoundPriorityEntry* pEntry = g_SoundPriorities;
    bool found = false;

    if (pValue && g_SoundPriorities[0].Name) {
        for (; pEntry->Name; ++pEntry) {
            if (_strcmpi(pValue, pEntry->Name) == 0) {
                found = true;
                break;
            }
        }
    }

    if (found)
        nCurrent = static_cast<SoundPriority>(pEntry->Value);

    if (pMatched)
        *pMatched = found;

    return nCurrent;
}

int32 VocClass::ParseSoundControl(int32 nCurrent, const char* pValue, bool* pMatched)
{
    const SoundFlagEntry* pEntry = g_SoundControlFlags;
    bool found = false;

    if (pValue && g_SoundControlFlags[0].Name) {
        for (; pEntry->Name; ++pEntry) {
            if (_strcmpi(pValue, pEntry->Name) == 0) {
                found = true;
                break;
            }
        }
    }

    if (found)
        nCurrent |= pEntry->Value;

    if (pMatched)
        *pMatched = found;

    return nCurrent;
}

// The type flags are grouped: bits 0x60 (GLOBAL/SCREEN/LOCAL) form one group
// and bits 0x0C (VIOLENT/MOVEMENT/QUIET/LOUD) another.  A new token clears
// the group it belongs to before being OR-ed in, so the last flag seen in a
// group wins.
int32 VocClass::ParseSoundType(int32 nCurrent, const char* pValue, bool* pMatched)
{
    const SoundFlagEntry* pEntry = g_SoundTypeFlags;
    bool found = false;

    if (pValue && g_SoundTypeFlags[0].Name) {
        for (; pEntry->Name; ++pEntry) {
            if (_strcmpi(pValue, pEntry->Name) == 0) {
                found = true;
                break;
            }
        }
    }

    if (found) {
        int32 value = pEntry->Value;

        if (value & 0x60) {
            nCurrent &= ~0x60;
        } else if (value & 0x0C) {
            nCurrent &= ~0x0C;
        }

        nCurrent |= value;
    }

    if (pMatched)
        *pMatched = found;

    return nCurrent;
}

// Reads the section named after this sound and applies every parameter to the
// receiver.  Returns false when the section does not exist.
bool SoundDefinition::LoadFromINI(CCINIClass* pINI)
{
    if (!pINI)
        return false;

    const char* pSection = GetName();
    if (!pSection || !pSection[0])
        pSection = "Invalid Voc";

    if (pINI->GetSection(pSection) == nullptr)
        return false;

    {
        char buffer[0x800];
        buffer[0] = '\0';

        if (pINI->ReadString(pSection, "Sounds", "", buffer, sizeof(buffer)) > 0) {
            char* pToken = std::strtok(buffer, " \t\n");
            while (pToken != nullptr) {
                AddSample(pToken);
                pToken = std::strtok(nullptr, " \t\n");
            }
        }
    }

    {
        char buffer[0x800];
        buffer[0] = '\0';

        double volume = pINI->ReadFixed(pSection, "Volume", Sound_Default_Volume);
        volume *= 0.01;
        if (volume > 1.0) volume = 1.0;
        if (volume < 0.0) volume = 0.0;
        SetVolume(static_cast<int32>(std::floor(volume * 16384.0)));

        SetVShift(pINI->ReadInteger(pSection, "VShift", 0));

        double minVolume = pINI->ReadFixed(pSection, "MinVolume", Sound_Default_MinVolume);
        minVolume *= 0.01;
        if (minVolume > 1.0) minVolume = 1.0;
        if (minVolume < 0.0) minVolume = 0.0;
        SetMinVolume(minVolume);

        int32 priority = static_cast<int32>(VocClass::DefaultPriority);
        if (pINI->ReadString(pSection, "Priority", "NORMAL", buffer, sizeof(buffer)) > 0) {
            bool matched = false;
            int32 parsed = static_cast<int32>(
                VocClass::ParseSoundPriority(static_cast<SoundPriority>(priority), buffer, &matched));
            if (matched)
                priority = parsed;
        }
        SetPriority(static_cast<SoundPriority>(priority));

        SetAttack(pINI->ReadInteger(pSection, "Attack", 0));
        SetDecay(pINI->ReadInteger(pSection, "Decay", 0));

        if (pINI->ReadString(pSection, "Control", "", buffer, sizeof(buffer)) > 0) {
            char* pToken = std::strtok(buffer, " \t\n");
            if (pToken != nullptr) {
                int32 control = 0;
                while (pToken != nullptr) {
                    control = VocClass::ParseSoundControl(control, pToken);
                    pToken = std::strtok(nullptr, " \t\n");
                }
                SetControl(control);
            }
        }

        if (pINI->ReadString(pSection, "Type", "", buffer, sizeof(buffer)) > 0) {
            char* pToken = std::strtok(buffer, " \t\n");
            if (pToken != nullptr) {
                int32 type = Sound_Default_Type;
                while (pToken != nullptr) {
                    type = VocClass::ParseSoundType(type, pToken);
                    pToken = std::strtok(nullptr, " \t\n");
                }
                SetType(type);
            }
        }

        SetLimit(pINI->ReadInteger(pSection, "Limit", VocClass::DefaultLimit));
        SetLoop(pINI->ReadInteger(pSection, "Loop", 0));
        SetRange(pINI->ReadInteger(pSection, "Range", VocClass::DefaultRange));

        if (pINI->ReadString(pSection, "Delay", "", buffer, sizeof(buffer)) > 0) {
            int32 delay1 = 0;
            int32 delay2 = 0;
            char* pToken = std::strtok(buffer, " \t\n");
            if (pToken != nullptr) {
                delay1 = std::atoi(pToken);
                pToken = std::strtok(nullptr, " \t\n");
                if (pToken != nullptr)
                    delay2 = std::atoi(pToken);
            }
            SetDelay(delay1, delay2);
        }

        if (pINI->ReadString(pSection, "FShift", "", buffer, sizeof(buffer)) > 0) {
            int32 fshift1 = 0;
            int32 fshift2 = 0;
            char* pToken = std::strtok(buffer, " \t\n");
            if (pToken != nullptr) {
                fshift1 = std::atoi(pToken);
                pToken = std::strtok(nullptr, " \t\n");
                if (pToken != nullptr)
                    fshift2 = std::atoi(pToken);
            }
            SetFDelta(fshift1, fshift2);
        }
    }

    return true;
}

void VocClass::CreateFromINIList(CCINIClass* pINI)
{
    if (!pINI)
        return;

    if (!g_pSoundDefinitions)
        g_pSoundDefinitions = new DynamicVectorClass<SoundDefinition*>();

    DeleteAll();

    DefaultVolume    = Sound_Default_Volume;
    DefaultMinVolume = Sound_Default_MinVolume;
    DefaultPriority  = SoundPriority::Normal;
    DefaultControl   = Sound_Default_Control;
    DefaultType      = Sound_Default_Type;
    DefaultLimit     = 3;
    DefaultRange     = 10;

    if (pINI->GetSection("Defaults") != nullptr) {
        char buffer[0x800];

        DefaultVolume = pINI->ReadFixed("Defaults", "Volume", DefaultVolume);
        DefaultMinVolume = pINI->ReadFixed("Defaults", "MinVolume", DefaultMinVolume);

        buffer[0] = '\0';
        if (pINI->ReadString("Defaults", "Priority", "NORMAL", buffer, sizeof(buffer)) > 0) {
            bool matched = false;
            SoundPriority parsed = ParseSoundPriority(DefaultPriority, buffer, &matched);
            if (matched)
                DefaultPriority = parsed;
        }

        buffer[0] = '\0';
        if (pINI->ReadString("Defaults", "Control", "", buffer, sizeof(buffer)) > 0) {
            char* pToken = std::strtok(buffer, " \t\n");
            if (pToken != nullptr) {
                DefaultControl = 0;
                while (pToken != nullptr) {
                    DefaultControl = ParseSoundControl(DefaultControl, pToken);
                    pToken = std::strtok(nullptr, " \t\n");
                }
            }
        }

        buffer[0] = '\0';
        if (pINI->ReadString("Defaults", "Type", "", buffer, sizeof(buffer)) > 0) {
            char* pToken = std::strtok(buffer, " \t\n");
            if (pToken != nullptr) {
                DefaultType = Sound_Default_Type;
                while (pToken != nullptr) {
                    DefaultType = ParseSoundType(DefaultType, pToken);
                    pToken = std::strtok(nullptr, " \t\n");
                }
            }
        }

        DefaultLimit = pINI->ReadInteger("Defaults", "Limit", DefaultLimit);
        DefaultRange = pINI->ReadInteger("Defaults", "Range", DefaultRange);
    }

    if (pINI->GetSection("SoundList") == nullptr)
        return;

    int32 count = pINI->GetKeyCount("SoundList");

    for (int32 i = 0; i < count; ++i) {
        const char* pKeyName = pINI->GetKeyName("SoundList", i);
        if (!pKeyName || !pKeyName[0])
            continue;

        char buffer[0x800];
        buffer[0] = '\0';
        if (pINI->ReadString("SoundList", pKeyName, "", buffer, sizeof(buffer)) <= 0)
            continue;

        SoundDefinition* pDef = FindExistingSound(pKeyName);
        if (pDef == nullptr) {
            pDef = new SoundDefinition();
            if (!pDef)
                continue;
            pDef->SetName(pKeyName);
            RegisterSoundName(pKeyName);
            if (!g_pSoundDefinitions->Add(pDef)) {
                delete pDef;
                continue;
            }
        }

        pDef->LoadFromINI(pINI);
    }
}

void VocClass::DeleteAll()
{
    if (!g_pSoundDefinitions)
        return;

    for (int32 i = 0; i < g_pSoundDefinitions->Count; ++i)
        delete (*g_pSoundDefinitions)[i];

    g_pSoundDefinitions->Clear();
}

SoundDefinition* VocClass::FindSound(const char* pName)
{
    return FindExistingSound(pName);
}

SoundDefinition* VocClass::GetSound(int32 nIndex)
{
    if (!g_pSoundDefinitions || nIndex < 0 || nIndex >= g_pSoundDefinitions->Count)
        return nullptr;

    return (*g_pSoundDefinitions)[nIndex];
}

int32 VocClass::GetSoundDefinitionCount()
{
    if (!g_pSoundDefinitions)
        return 0;

    return g_pSoundDefinitions->Count;
}

int32 VocClass::AddSampleIndex(int32 index)
{
    return index;
}
