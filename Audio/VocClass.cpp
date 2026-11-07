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
    Attack = 0;
    Decay = 0;
    SelfPosition = CoordStruct(0, 0, 0);

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
//
// asm: VocClass_AddSample (asm:80)
//   if ([edi+134h] == 20h) return 0;                 ; 已有 32 条
//   if (!audioEnabled || !sampleCount) return 1;     ; 音频未就绪：不入表但报成功
//   while (*name == '$' || *name == '#') ++name;
//   idx = Audio_FindSampleIndex(name);
//   if (idx == -1) { WWDebugString("Missing sample %s\n"); Valid = false; return 1; }
//   SampleIndex[SampleCount++] = idx; return 1;
bool SoundDefinition::AddSample(const char* pName)
{
    if (!pName) return false;
    if (SampleCount >= MAX_SAMPLES_PER_SOUND) return false;   // asm: cmp [edi+134h],20h

    // asm: mov eax, VocGlobalA.field_140+3D8h / test / jz -> return 1
    //      mov ecx, VocGlobalA.SoundCount+3D8h / test / jz -> return 1
    if (g_pAudioIndexTable == nullptr || g_pAudioIndexTable->GetCount() == 0)
        return true;

    while (*pName == '$' || *pName == '#')                     // asm: loc_4064C7
        ++pName;

    int32 index = Audio_FindSampleIndex(pName);                // asm: call Audio_FindSampleIndex
    if (index == -1) {
        Valid = false;                                         // asm: [edi+0Ch] = 0
        return true;
    }

    SampleIndex[SampleCount++] = index;                        // asm: mov [edi+ecx*4+0B4h],eax
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

void SoundDefinition::SetAttack(int32 attack)   { this->Attack = attack; }
void SoundDefinition::SetDecay(int32 decay)     { this->Decay  = decay; }

// ----------------------------------------------------------------------------
// asm: VocClass_SetControl (asm:265)
//   test dl,20h / mov [ecx+10h],edx     ; 先存 Control
//   jz loc_406591
//   eax = 1
//   esi = [ecx+138h] (Attack) ; test / jnz loc_40659B
//   [ecx+138h] = 1                      ; 带 ATTACK 位而无 Attack 值时补 1
// loc_406591: [ecx+138h] = 0
// loc_40659B: test dl,40h / jz loc_4065B1
//   edx = [ecx+13Ch] (Decay) ; test / jnz ret
//   [ecx+13Ch] = 1                      ; 带 DECAY 位而无 Decay 值时补 1
// loc_4065B1: [ecx+13Ch] = 0
// 也就是说 Control 同时定义了 Attack/Decay 的"有无"：置位时缺省补 1，
// 清位时强制归零。
// ----------------------------------------------------------------------------
void SoundDefinition::SetControl(int32 control)
{
    this->Control = control;

    if (control & SoundControl_Attack) {         // asm: test dl,20h
        if (this->Attack == 0)                   // asm: mov esi,[ecx+138h]
            this->Attack = 1;                    // asm: mov [ecx+138h],eax
    } else {
        this->Attack = 0;                        // asm: loc_406591
    }

    if (control & SoundControl_Decay) {          // asm: test dl,40h
        if (this->Decay == 0)                    // asm: mov edx,[ecx+13Ch]
            this->Decay = 1;                     // asm: mov [ecx+13Ch],eax
    } else {
        this->Decay = 0;                         // asm: loc_4065B1
    }
}
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

// ----------------------------------------------------------------------------
// asm: VocClass_FindIndex (__fastcall, const char *name) -> int
//   遍历 VocClass 数组，逐个取名字后 __strcmpi 比较（大小写不敏感），命中
//   返回下标；走到末尾仍未命中返回 -1。名字槽为空时用 "Invalid Voc" 兜底。
//   与 FindIndexOfName 语义一致，直接转调以免重复实现。
// ----------------------------------------------------------------------------
int32 VocClass::FindIndex(const char* pName)
{
    return FindIndexOfName(pName);
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

        // asm: mov ecx,usedSoundControlFlags / mov [esp+..],ecx ; 无 token 时
        //      沿用 [Defaults] Control 的值，并且**总是**调用 SetControl
        //      （SetControl 还要据 Control 位修正 Attack/Decay）。
        {
            int32 control = VocClass::DefaultControl;      // asm: usedSoundControlFlags
            if (pINI->ReadString(pSection, "Control", "", buffer, sizeof(buffer)) > 0) {
                char* pToken = std::strtok(buffer, " \t\n");
                if (pToken != nullptr) {
                    control = 0;                           // asm: mov [var_808],0
                    while (pToken != nullptr) {
                        control = VocClass::ParseSoundControl(control, pToken);
                        pToken = std::strtok(nullptr, " \t\n");
                    }
                }
            }
            SetControl(control);
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
    // asm: VocClass_DeleteAll 先清 AudioBufferNewest 实例链（asm:1629..），
    // 再逐个删除声音定义，避免实例上的 Owner 变成悬垂指针。
    ResetInstances();

    if (!g_pSoundDefinitions)
        return;

    for (int32 i = 0; i < g_pSoundDefinitions->Count; ++i)
        delete (*g_pSoundDefinitions)[i];

    g_pSoundDefinitions->Clear();
}

// ----------------------------------------------------------------------------
// asm: init_vec_Voc (0x750340)
//   全局 vec_Voc —— DynamicVectorClass<VocClass*>：data=0 / count=0 /
//   IsInitialized=1 / IsAllocated=0 / incrementAmount=0Ah / length=0，
//   并 _atexit 注册 loc_750340 作为析构。
//   函数内静态对象即 C++ 层的等价物：首次调用构造，进程退出析构。
// ----------------------------------------------------------------------------
DynamicVectorClass<VocClass*>& VocClass::Vec_Voc()
{
    static DynamicVectorClass<VocClass*> vec_Voc;
    return vec_Voc;
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

// ============================================================================
// asm: AudioBufferNewest —— 静态音效实例链表
//
// 原版由一个定长池（IDX_Container_，0xC8 个 0x3C 字节节点）供给节点，
// 链表头是全局 AudioBufferNewest；VocCtrl_Add 把新节点插到表头之后，
// VocClass_GetPrevious 沿 NearLink 回退。dword_B1D3A8 记录当前条数，
// dword_B1D3AC 记录历史峰值（PlayIndexAtPos 每次 ++ 后与峰值比较）。
// 声卡后端未建模，故"播放"落成实例上的音量/声像状态。
// ============================================================================
namespace
{
    VocInstance  g_AudioBufferNewest;             // asm: AudioBufferNewest（表头哨兵）
    bool         g_AudioBufferNewestReady = false;
    int32        g_StaticSoundCount = 0;          // asm: dword_B1D3A8
    int32        g_StaticSoundPeak  = 0;          // asm: dword_B1D3AC

    void EnsureNewestList()
    {
        if (!g_AudioBufferNewestReady) {
            g_AudioBufferNewest.Link.NearLink = &g_AudioBufferNewest.Link;
            g_AudioBufferNewest.Link.FarLink  = &g_AudioBufferNewest.Link;
            g_AudioBufferNewestReady = true;
        }
    }

    VocInstance* InstanceOf(VocClassHeader* pNode)
    {
        return reinterpret_cast<VocInstance*>(pNode);
    }
}

// ----------------------------------------------------------------------------
// asm: VocCtrl_Add (asm:566)  __fastcall(head, node)
//   eax = [ecx+4]            ; head->FarLink
//   [edx]   = ecx            ; node->NearLink = head
//   [edx+4] = eax            ; node->FarLink = head->FarLink
//   [ecx+4] = edx            ; head->FarLink = node
//   ecx = [edx+4] ; [ecx] = edx   ; node->FarLink->NearLink = node
// 结果是"后插入的永远紧贴表头"，遍历 NearLink 得到的是最旧的一条。
// ----------------------------------------------------------------------------
void VocClass::AddInstanceToNewest(VocInstance* pInst)
{
    if (!pInst)
        return;

    EnsureNewestList();
    VocClassHeader* pHead = &g_AudioBufferNewest.Link;
    VocClassHeader* pNode = &pInst->Link;

    pNode->NearLink = pHead;                      // asm: mov [edx],ecx
    pNode->FarLink  = pHead->FarLink;             // asm: mov [edx+4],eax
    pHead->FarLink  = pNode;                      // asm: mov [ecx+4],edx
    pNode->FarLink->NearLink = pNode;             // asm: mov ecx,[edx+4] / mov [ecx],edx
}

// ----------------------------------------------------------------------------
// asm: VocClass_GetPreviousFromHead (asm:518)
//   test ecx,ecx / jz 0
//   eax = [ecx]              ; head->NearLink（最旧一条）
//   test eax,eax / jz 0
//   cmp eax,[eax+8] / jnz ret; 命中 +8 哨兵 -> 0
// 本工程节点 +8 未作哨兵使用（恒 0），故只保留"空/自环即无"的判定。
// ----------------------------------------------------------------------------
VocInstance* VocClass::GetPreviousFromHead()
{
    EnsureNewestList();
    VocClassHeader* pHead = &g_AudioBufferNewest.Link;
    VocClassHeader* pPrev = pHead->NearLink;      // asm: mov eax,[ecx]
    if (!pPrev) return nullptr;                   // asm: test eax,eax ; jz
    if (pPrev == pHead) return nullptr;           // 空链（自环）
    return InstanceOf(pPrev);
}

// ----------------------------------------------------------------------------
// asm: VocClass_GetPrevious (asm:551) —— 与上者同体，入参是节点而非表头。
// ----------------------------------------------------------------------------
VocInstance* VocClass::GetPrevious(VocInstance* pInst)
{
    if (!pInst) return nullptr;                   // asm: test ecx,ecx ; jz
    VocClassHeader* pNode = &pInst->Link;
    VocClassHeader* pPrev = pNode->NearLink;      // asm: mov eax,[ecx]
    if (!pPrev) return nullptr;                   // asm: test eax,eax ; jz
    if (pPrev == pNode) return nullptr;           // 已摘除（自环）
    if (pPrev == &g_AudioBufferNewest.Link)       // 走到表头，没有上一条
        return nullptr;
    return InstanceOf(pPrev);
}

// ----------------------------------------------------------------------------
// asm: VocClass_RemoveFromList (asm:588) —— 与成员版 RemoveFromList 同体，
// 这里给出实例版入口，供链表代码按节点调用。
// ----------------------------------------------------------------------------
void VocClass::RemoveInstanceFromList(VocInstance* pInst)
{
    if (!pInst) return;
    VocClassHeader* pNode = &pInst->Link;

    pNode->FarLink->NearLink = pNode->NearLink;   // asm: mov eax,[ecx+4] / mov [eax],edx
    pNode->NearLink->FarLink = pNode->FarLink;    // asm: mov eax,[ecx] / mov edx,[ecx+4] / mov [eax+4],edx
    pNode->NearLink = pNode;                      // asm: mov [ecx],ecx
    pNode->FarLink  = pNode;                      // asm: mov [ecx+4],ecx
}

// ----------------------------------------------------------------------------
// asm: 节点池取用（PlayIndexAtPos 里 AudioMiniBufferList_FindNextOrEvenNext）
// 原版从 0xC8 项的静态池取；这里等价地按需分配，容量上限保持 0xC8。
// ----------------------------------------------------------------------------
VocInstance* VocClass::AllocInstance()
{
    if (g_StaticSoundCount >= 0xC8)               // asm: 池容量 0xC8
        return nullptr;
    return new VocInstance();
}

void VocClass::FreeInstance(VocInstance* pInst)
{
    delete pInst;
}

int32 VocClass::GetInstanceCount()
{
    return g_StaticSoundCount;
}

int32 VocClass::GetInstancePeak()
{
    return g_StaticSoundPeak;
}

// ----------------------------------------------------------------------------
// asm: VocClass_DeleteAll 的链表段 (asm:1585)
//   while (node = GetPreviousFromHead()) {
//       next = GetPrevious(node);
//       StrX_DTOR(node+0Ch);
//       RemoveFromList(node);
//       pool->AddSample(node);
//       --dword_B1D3A8;
//       node = next;
//   }
// ----------------------------------------------------------------------------
void VocClass::ResetInstances()
{
    VocInstance* pInst = GetPreviousFromHead();
    while (pInst != nullptr)
    {
        VocInstance* pNext = GetPrevious(pInst);  // asm: 先取后继，再摘
        pInst->Handle = nullptr;                  // asm: StrX_DTOR(node+0Ch)
        RemoveInstanceFromList(pInst);            // asm: VocClass_RemoveFromList
        FreeInstance(pInst);                      // asm: EventMempool_AddSample
        if (g_StaticSoundCount > 0)
            --g_StaticSoundCount;                 // asm: dec dword_B1D3A8
        pInst = pNext;
    }
    g_StaticSoundCount = 0;
}

// ----------------------------------------------------------------------------
// asm: sub_750AC0 (0x750AC0) —— 距离衰减
//
// 已确证的控制流：
//   * 结果 = (Range - dist) / Range                       (loc_750C8B)
//   * Range <= 0 或 dist >= Range 时，音量被夹到 0 边界   (loc_750C3D)
//   * 结果 < dbl_7E8AE8 直接返回 0                        (loc_750CBD)
//   * 声像：pan = dx * 8192 / halfExtent + 8192           (loc_750D10)
//     halfExtent = rect_mouseclass_vp.R * 0.5（视野半宽），偏移量夹在
//     ±halfExtent，因此 pan 落在 0..0x4000。
// 原版先经 Tactical::Coord_To_Pixel 投影到屏幕像素；本工程 TacticalMap
// 未建模，故用世界 XY 距离等价代替投影后的屏幕距离（结果单调一致），
// 并在注释中标明这一处近似。
// ----------------------------------------------------------------------------
float VocClass::ComputeAttenuation(const CoordStruct& listener, int32 range,
                                   const CoordStruct& source, int32* pPan)
{
    if (pPan)
        *pPan = 0x2000;                            // asm: mov dword ptr [ebp+0],0 后累加居中

    if (range <= 0)
        return 0.0f;

    const float dx = static_cast<float>(source.X - listener.X);
    const float dy = static_cast<float>(source.Y - listener.Y);

    // asm: fild dx / fmul 0.5 / fadd st,st  -> 以视野半宽为 1.0 的归一化
    const float halfExtent = static_cast<float>(range) * 0.5f;

    float clampedX = dx;
    if (clampedX < -halfExtent) clampedX = -halfExtent;   // asm: fcomp / fchs 比较
    if (clampedX >  halfExtent) clampedX =  halfExtent;

    const float dist = std::sqrt(dx * dx + dy * dy);

    if (dist >= static_cast<float>(range))        // asm: fcomp Range / jae
        return 0.0f;

    float volume = (static_cast<float>(range) - dist) / static_cast<float>(range);

    // asm: loc_750CBD —— 低于门限直接视为听不见（dbl_7E8AE8 约 1e-3）
    if (volume < 0.001f)
        return 0.0f;

    if (pPan)
    {
        // asm: loc_750D10
        //   fld dx / fmul 8192.0 / fdiv halfExtent*2 / fadd 8192.0 / floor
        float pan = (clampedX * 8192.0f) / (halfExtent * 2.0f) + 8192.0f;
        int32 panInt = static_cast<int32>(std::floor(pan));
        if (panInt < 0)     panInt = 0;
        if (panInt > 0x4000) panInt = 0x4000;
        *pPan = panInt;
    }

    return volume;
}

// ----------------------------------------------------------------------------
// asm: VocClass_UpdateAtLocation (asm:1110)  __fastcall(pos, strx)
//   if (!strx) return;
//   handle = StrX_406130(strx);
//   coords = StrX_406170(strx);
//   if (!coords) { StrX_4060F0(strx, handle, coords); return; }
//   vol = sub_750AC0(pos, &pan, coords);
//   if (vol > 0) {
//       if (!handle) handle = sub_405190(coords)  ; 新开一路
//       if (!handle) { write back; return; }
//       StrX_4061D0(handle, min(vol*16384, 0x4000));   ; 音量
//       StrX_406270(handle, pan);                      ; 声像
//   } else {
//       if (handle) { sub_4052F0(handle); handle = 0; } ; 停掉
//   }
//   StrX_4060F0(strx, handle, coords);                 ; 写回
// ----------------------------------------------------------------------------
void VocClass::UpdateAtLocation(const CoordStruct* pPos, VocSoundAnchor* pAnchor)
{
    if (!pAnchor)
        return;

    void* pHandle = pAnchor->Get_Current_Sound();          // asm: StrX_406130
    const CoordStruct* pCoords = pAnchor->Get_Current_Sound_Coords(); // StrX_406170

    if (!pCoords || !pPos)
    {
        pAnchor->Set_Current_Sound(pHandle);               // asm: loc_750E0C
        return;
    }

    int32 pan = 0x2000;
    // 距离衰减需要一个 Range；实例没有范围时按默认 Range 处理。
    int32 range = DefaultRange;
    if (pHandle != nullptr)
    {
        VocInstance* pInst = static_cast<VocInstance*>(pHandle);
        SoundDefinition* pDef = pInst->Owner;
        if (pDef != nullptr)
            range = pDef->Range;
    }

    const float volume = ComputeAttenuation(*pPos, range, *pCoords, &pan);

    if (volume > 0.0f)
    {
        if (pHandle == nullptr)
        {
            // asm: sub_406650 / sub_405190 —— 该坐标尚无音源，新开一路
            VocInstance* pInst = AllocInstance();
            if (pInst != nullptr)
            {
                pInst->Position = *pCoords;
                pInst->Handle   = nullptr;
                AddInstanceToNewest(pInst);
                ++g_StaticSoundCount;
                if (g_StaticSoundCount > g_StaticSoundPeak)
                    g_StaticSoundPeak = g_StaticSoundCount;
                pHandle = pInst;
            }
        }

        if (pHandle != nullptr)
        {
            VocInstance* pInst = static_cast<VocInstance*>(pHandle);
            int32 units = static_cast<int32>(std::floor(volume * 16384.0f));
            if (units > 0x4000) units = 0x4000;            // asm: cmp eax,4000h
            pInst->Voice   = reinterpret_cast<void*>(static_cast<intptr_t>(units));
            pInst->Argument = pan;                         // StrX_406270
        }
    }
    else
    {
        if (pHandle != nullptr)
        {
            // asm: sub_4052F0 —— 听不见则停掉并清句柄
            VocInstance* pInst = static_cast<VocInstance*>(pHandle);
            pInst->Voice = nullptr;
            RemoveInstanceFromList(pInst);
            FreeInstance(pInst);
            if (g_StaticSoundCount > 0)
                --g_StaticSoundCount;
            pHandle = nullptr;
        }
    }

    pAnchor->Set_Current_Sound(pHandle);                   // asm: StrX_4060F0
}

// ----------------------------------------------------------------------------
// asm: VocClass_PlayIndexAtPos (asm:1229)
//   if (index < 0) return 0;
//   node = Pool->FindNextOrEvenNext(); if (!node) return 0;
//   StrX_Init(node+0Ch);
//   Pool->Unlink(node);
//   node->[0x30] = 0;
//   node->[0x20..0x28] = *where;
//   node->[0x2C] = a3;
//   handle = Sound_Effect(index, where, 0);
//   VocCtrl_Add(&AudioBufferNewest, node);
//   ++dword_B1D3A8; if (> dword_B1D3AC) dword_B1D3AC = dword_B1D3A8;
//   return handle;
// 说明：asm 返回的是 Sound_Effect 的句柄，节点本身另挂在链表上；这里
//       让节点即为句柄（本工程的"句柄"就是实例），两者语义一致。
// ----------------------------------------------------------------------------
VocInstance* VocClass::PlayIndexAtPos(int32 index, const CoordStruct* pPos,
                                      int32 argument)
{
    if (index < 0)                                    // asm: test ebp,ebp / jl
        return nullptr;

    VocInstance* pInst = AllocInstance();             // asm: AudioMiniBufferList_FindNextOrEvenNext
    if (pInst == nullptr)
        return nullptr;

    pInst->Handle  = nullptr;                         // asm: StrX_Init(node+0Ch)
    pInst->Owner   = nullptr;                         // 由 Sound_Effect 关联
    pInst->Countdown = 0;                             // asm: mov [esi+30h],0
    if (pPos != nullptr)
        pInst->Position = *pPos;                      // asm: 三个 dword 拷贝
    pInst->Argument = argument;                       // asm: mov [esi+2Ch],eax

    Sound_Effect(index, pPos, nullptr);               // asm: call Sound_Effect

    AddInstanceToNewest(pInst);                       // asm: VocCtrl_Add(&AudioBufferNewest,node)
    ++g_StaticSoundCount;                             // asm: inc dword_B1D3A8
    if (g_StaticSoundCount > g_StaticSoundPeak)       // asm: cmp/jle
        g_StaticSoundPeak = g_StaticSoundCount;       // asm: mov dword_B1D3AC,eax

    return pInst;
}

// ----------------------------------------------------------------------------
// asm: VocClass_SaveData (asm:1856)
//   count = AudioMiniBuffer_CountLength(&AudioBufferNewest);
//   stream->Write(&count, 4);                     ; 失败即返回
//   for (node = GetPreviousFromHead(); node; node = GetPrevious(node)) {
//       idx = -1;
//       if (node->[0x14]) { name = GetName(node->[0x14]);
//                           idx = FindIndex(name); }
//       rec = { idx, node->[0x20], node->[0x24], node->[0x28] };   ; 16 字节
//       stream->Write(&rec, 0x10);                ; 失败即返回该结果
//   }
// ----------------------------------------------------------------------------
bool VocClass::SaveData(VocSaveStream* pStream)
{
    if (pStream == nullptr)
        return false;

    int32 count = 0;
    for (VocInstance* pIt = GetPreviousFromHead(); pIt != nullptr;
         pIt = GetPrevious(pIt))
        ++count;                                       // asm: AudioMiniBuffer_CountLength

    if (!pStream->Write(&count, 4))                    // asm: call [eax+10h], 4 字节
        return false;

    for (VocInstance* pInst = GetPreviousFromHead(); pInst != nullptr;
         pInst = GetPrevious(pInst))
    {
        int32 record[4];
        int32 index = -1;                              // asm: or esi,-1
        if (pInst->Owner != nullptr)                   // asm: mov ecx,[edi+14h] / test
            index = FindIndexOfName(pInst->Owner->GetName());

        record[0] = index;
        record[1] = pInst->Position.X;                 // asm: [edi+20h]
        record[2] = pInst->Position.Y;                 // asm: [edi+24h]
        record[3] = pInst->Position.Z;                 // asm: [edi+28h]

        if (!pStream->Write(record, 0x10))              // asm: push 10h
            return false;
    }

    return true;
}

// ------------------------------------------------------------------------
// asm: VocClass_RemoveFromList  (__fastcall, VocClass *this)
//   eax = [ecx+4]（后继 FarLink）
//   edx = [ecx]  （前驱 NearLink）
//   [eax]   = edx        ; 后继的前驱指向我的前驱
//   [edx+4] = eax        ; 前驱的后继指向我的后继
//   [ecx] = [ecx+4] = ecx ; 自身复位为自环（空节点）
//   摘除后再调用 GetPrevious/GetPreviousFromHead 就会因自环而返回空，
//   这正是原版用于判断"是否仍在播放队列里"的哨兵语义。
// ------------------------------------------------------------------------
void VocClass::RemoveFromList()
{
    VocClassHeader* pNode = reinterpret_cast<VocClassHeader*>(this);
    pNode->FarLink->NearLink = pNode->NearLink;
    pNode->NearLink->FarLink = pNode->FarLink;
    pNode->NearLink = pNode;
    pNode->FarLink = pNode;
}

// ------------------------------------------------------------------------
// asm: VocClass_GetPrevious  (__fastcall, VocClass *this) -> VocClass*
//   if (!this) return 0;
//   eax = [this];  if (!eax) return 0;
//   if (eax == [eax+8]) return 0;   ; 前驱就是表头 -> 队列没上一个
//   return eax;
// 注：asm 用 [eax+8] 判定表头哨兵，而现有 VocClassHeader 布局没有该
//     偏移槽位，铁律①只对齐已确证的链表指针语义：自环（前驱==自身，
//     即表头/已摘除）时返回空，否则返回前驱节点。
// ------------------------------------------------------------------------
VocClassHeader* VocClass::GetPreviousFromList()
{
    VocClassHeader* pNode = reinterpret_cast<VocClassHeader*>(this);
    if (!pNode) return nullptr;
    VocClassHeader* pPrev = pNode->NearLink;
    if (!pPrev) return nullptr;
    if (pPrev == pNode) return nullptr;
    return pPrev;
}

// ------------------------------------------------------------------------
// asm: VocClass_GetPreviousFromHead  (__fastcall, VocClassHeader *this)
//   与 GetPrevious 同体，差别在入参是表头节点，故单独暴露一个入口。
// ------------------------------------------------------------------------
VocClassHeader* VocClass::GetPreviousFromHeadEntry()
{
    VocClassHeader* pNode = reinterpret_cast<VocClassHeader*>(this);
    if (!pNode) return nullptr;
    VocClassHeader* pPrev = pNode->NearLink;
    if (!pPrev) return nullptr;
    if (pPrev == pNode) return nullptr;
    return pPrev;
}

// ------------------------------------------------------------------------
// asm: VocClass_FindNameByIndex
// 报"Invalid Voc"。
// ------------------------------------------------------------------------
const char* VocClass::FindNameByIndex(int32 index)
{
    if (index < 0 || index >= GetSoundCount()) {
        return "<none>";
    }
    const char* pName = GetSoundName(index);
    return (pName != nullptr) ? pName : "Invalid Voc";
}

// ============================================================================
// asm: Sound_Effect (0x7509D0)  (retn 4)
//
//   __fastcall(int index, int *pos, someStructX *strx) -> handle
//   mov al,byte_8464AC / test al,al / jnz loc_7509F2   ; 总开关关闭直接返回 0
//   xor esi,esi / xor edi,edi
//   test ecx,ecx / jl loc_750A0F ; cmp ecx,length / jge loc_750A0F
//   edi = VocClass[index]->[0]                          ; 该声自带的坐标槽
//   if (strx) {
//       esi = StrX_406130(strx);                        ; 现有句柄
//       if (esi && StrX_406310(esi) == edi) {           ; 同一位置重复
//           sub_4052F0(esi); esi = 0;                   ; 停旧
//       }
//   }
//   if (edi) {
//       vol = sub_750AC0(pos, &pan, edi);
//       if (vol > 0) {
//           units = min(floor(vol*16384), 0x4000);
//           if (!esi) esi = sub_405190(edi);            ; 新开一路
//           if (esi) { StrX_4061D0(esi, units); StrX_406270(esi, pan); }
//       } else {
//           if (esi) { sub_4052F0(esi); esi = 0; }
//       }
//   }
//   if (strx) StrX_4060F0(strx, esi, edi);
//   return esi;
//
// 本工程里 StrX_* 由 VocSoundAnchor 接口承接、sub_750AC0 由
// ComputeAttenuation 承接；"该声自带的坐标槽"是音效定义上的静态位置
// （环境音），这里用 SoundDefinition::SelfPosition 表达。
// ============================================================================
namespace {
bool g_SoundEffectEnabled = true;      // asm: byte_8464AC
}

bool VocClass::Get_Sound_Effect_Enabled()
{
    return g_SoundEffectEnabled;
}

void VocClass::Set_Sound_Effect_Enabled(bool on)
{
    g_SoundEffectEnabled = on;
}

void* VocClass::Sound_Effect(int32 index, const CoordStruct* pPos,
                             VocSoundAnchor* pAnchor)
{
    // asm: mov al,byte_8464AC / test al,al / jnz loc_7509F2
    if (!g_SoundEffectEnabled)
        return nullptr;

    // asm: test ecx,ecx / jl loc_750A0F ; cmp ecx,VocClass.length / jge
    SoundDefinition* pDef = GetSound(index);
    if (pDef == nullptr)
        return nullptr;                     // edi 保持 0，下面只做 write-back

    // asm: mov eax,VocClass.data / mov ecx,[eax+ecx*4] / mov edi,[ecx]
    // 定义上的静态坐标（环境音）；没有则视为无坐标、不做衰减。
    const CoordStruct& coords = pDef->SelfPosition;   // asm: edi
    const bool hasCoords = (coords.X != 0 || coords.Y != 0 || coords.Z != 0);

    void* pHandle = nullptr;                // asm: esi

    // asm: mov ebp,[esp+10h+this] / test ebp,ebp / jz loc_750A38
    if (pAnchor != nullptr)
    {
        pHandle = pAnchor->Get_Current_Sound();               // asm: StrX_406130
        if (pHandle != nullptr)
        {
            VocInstance* pInst = static_cast<VocInstance*>(pHandle);
            // asm: StrX_406310(esi) == edi -> 同位置去重
            if (pInst->Position.X == coords.X &&
                pInst->Position.Y == coords.Y &&
                pInst->Position.Z == coords.Z)
            {
                RemoveInstanceFromList(pInst);                // asm: sub_4052F0
                FreeInstance(pInst);
                if (g_StaticSoundCount > 0)
                    --g_StaticSoundCount;
                pHandle = nullptr;
            }
        }
    }

    // asm: test edi,edi / jz loc_750AA5
    if (hasCoords)
    {
        int32 pan = 0x2000;
        const CoordStruct listener = (pPos != nullptr) ? *pPos : CoordStruct(0, 0, 0);
        const float volume = ComputeAttenuation(listener, pDef->Range, coords, &pan);

        // asm: fcom 0.0 / fnstsw ax / test ah,41h / jnz loc_750A96
        if (volume > 0.0f)
        {
            int32 units = static_cast<int32>(std::floor(volume * 16384.0f));
            if (units > 0x4000) units = 0x4000;               // asm: cmp eax,4000h / jb

            if (pHandle == nullptr)
            {
                // asm: sub_405190 —— 按坐标新开一路
                VocInstance* pInst = AllocInstance();
                if (pInst != nullptr)
                {
                    pInst->Owner    = pDef;
                    pInst->Position = coords;
                    AddInstanceToNewest(pInst);
                    ++g_StaticSoundCount;
                    if (g_StaticSoundCount > g_StaticSoundPeak)
                        g_StaticSoundPeak = g_StaticSoundCount;
                    pHandle = pInst;                          // asm: mov esi,eax
                }
            }

            if (pHandle != nullptr)
            {
                VocInstance* pInst = static_cast<VocInstance*>(pHandle);
                pInst->Owner    = pDef;
                pInst->Voice    = reinterpret_cast<void*>(static_cast<intptr_t>(units)); // StrX_4061D0
                pInst->Argument = pan;                        // StrX_406270
            }
        }
        else if (pHandle != nullptr)
        {
            // asm: loc_750A96 —— 听不见则停掉
            VocInstance* pInst = static_cast<VocInstance*>(pHandle);
            RemoveInstanceFromList(pInst);
            FreeInstance(pInst);
            if (g_StaticSoundCount > 0)
                --g_StaticSoundCount;
            pHandle = nullptr;
        }
    }

    // asm: test ebp,ebp / jz loc_750AB3 / push edi / StrX_4060F0(ebp, esi, edi)
    if (pAnchor != nullptr)
        pAnchor->Set_Current_Sound(pHandle);

    return pHandle;
}

// ============================================================================
// StrX 句柄槽（asm: someStructX, sizeof 0x14）
//
// 这组函数原先躺在 Helpers/AsmComplex4.cpp；它们不是自由函数，而是句柄槽
// 自身的构造/析构。槽里存的是"某个对象当前挂着的那条音效"：
//   Node -> 播放实例节点，节点 +0xB0 回指所有者（StrXOwner），所有者 +0xC0
//   再回指节点。asm 里 sub_4026A0/4026B0/4025B0/4026F0 的 this 就是所有者，
//   sub_401F90 的 this 是 voice 条目（+0x0C 为引用计数）。
// ============================================================================

namespace {

// asm: VocGlobalA.SoundCount+3D8h —— 写进槽 +0x0C 的归属常数，只做等值比较。
int32 s_StrXMarkerStorage = 0;

// 节点内部（>=0x27C 字节）在本工程没有构造点，故只按 asm 偏移读写，
// 不做类型化 —— 与 asm 的 `mov eax,[edi+XXh]` 一一对应。
inline int32& NodeI32(void* pNode, int32 off)
{
    return *reinterpret_cast<int32*>(static_cast<uint8*>(pNode) + off);
}

inline void*& NodePtr(void* pNode, int32 off)
{
    return *reinterpret_cast<void**>(static_cast<uint8*>(pNode) + off);
}

inline void* NodeGet(void* pNode, int32 off)
{
    return *reinterpret_cast<void**>(static_cast<uint8*>(pNode) + off);
}

} // namespace

int32 StrXSlot::GlobalReady = 0;
int32 StrXSlot::MarkerToken =
    static_cast<int32>(reinterpret_cast<intptr_t>(&s_StrXMarkerStorage));

// ----------------------------------------------------------------------------
// asm: sub_4025B0 (owner)
//   vt[+28]() ; if ([this+0A4h] & 5) vt[+10]() ; vt[+2Ch]()
//   if ([this+0BCh]) call [this+0BCh] with ecx = this
//   [this+168h] = 0
// ----------------------------------------------------------------------------
void StrXOwner::Stop_Voice()
{
    if (Device != nullptr)
    {
        void** vt = *static_cast<void***>(Device);
        reinterpret_cast<void (*)(void*)>(vt[0x28 / 4])(Device);

        if ((flags_A4 & 5) != 0)
            reinterpret_cast<void (*)(void*)>(vt[0x10 / 4])(Device);

        reinterpret_cast<void (*)(void*)>(vt[0x2C / 4])(Device);
    }

    if (field_BC != 0)
        reinterpret_cast<void (*)(void*)>(static_cast<intptr_t>(field_BC))(this);

    field_168 = 0;
}

// ----------------------------------------------------------------------------
// asm: sub_4026A0 (owner) —— mov eax,[ecx+0D4h] / jmp [eax+28h]
// ----------------------------------------------------------------------------
void StrXOwner::Guard_Detach()
{
    if (Device == nullptr)
        return;
    void** vt = *static_cast<void***>(Device);
    reinterpret_cast<void (*)(void*)>(vt[0x28 / 4])(Device);
}

// ----------------------------------------------------------------------------
// asm: sub_4026B0 (owner) —— mov eax,[ecx+0D4h] / jmp [eax+2Ch]
// ----------------------------------------------------------------------------
void StrXOwner::Guard_Attach()
{
    if (Device == nullptr)
        return;
    void** vt = *static_cast<void***>(Device);
    reinterpret_cast<void (*)(void*)>(vt[0x2C / 4])(Device);
}

// ----------------------------------------------------------------------------
// asm: sub_4026F0 (owner)
//   vt[+28]() ; [this+0A4h] &= ~10h ; vt[+2Ch]()
// ----------------------------------------------------------------------------
void StrXOwner::Release_Voice()
{
    if (Device != nullptr)
    {
        void** vt = *static_cast<void***>(Device);
        reinterpret_cast<void (*)(void*)>(vt[0x28 / 4])(Device);
        flags_A4 &= ~0x10;
        reinterpret_cast<void (*)(void*)>(vt[0x2C / 4])(Device);
    }
    else
    {
        flags_A4 &= ~0x10;
    }
}

// ----------------------------------------------------------------------------
// asm: sub_401F90 (voice)
//   eax = [ecx+0Ch] / dec eax / [ecx+0Ch] = eax / if (eax < 0) [ecx+0Ch] = 0
// ----------------------------------------------------------------------------
int32 StrXOwner::Voice_Dec_Ref(void* pVoice)
{
    if (pVoice == nullptr)
        return 0;

    int32* pRef = reinterpret_cast<int32*>(static_cast<uint8*>(pVoice) + 0x0C);
    const int32 value = *pRef - 1;
    *pRef = value;
    if (value < 0)
        *pRef = 0;
    return value;
}

// ----------------------------------------------------------------------------
// asm: StrX_Init —— 写归属常数并把三个指针/键位清 0
// ----------------------------------------------------------------------------
void StrXSlot::StrX_Init()
{
    Marker  = MarkerToken;
    Node    = nullptr;
    Field04 = 0;
    Field08 = 0;
}

// ----------------------------------------------------------------------------
// asm: StrX_DTOR_0 (0x405C40) —— 全拆 + 每条出口清 +0x0C
// asm: StrX_DTOR_1 (0x405D40) —— 全拆
//
//   if (!GlobalReady)                     -> [+8]=0 [, +0C=0]
//   if ([+0Ch] != MarkerToken)            -> 同上
//   node = [this] ; if (!node)            -> 同上
//   if ([+8] != [node+24h])               -> [0]=0 ; [+8]=0 [, +0C=0]
//   if ([+4] != [node+138h])              -> 同上
//   if ([node+18h] & 1)                   -> [0]=0 ; [+8]=0 [, +0C=0]
//   owner = [node+0B0h] 且 owner->C0 == node:
//       if ([node+18h] & 2) sub_4025B0(owner)
//       sub_4026F0(owner)
//       owner+0C0/BC/B4/B8/90/98/9C = 0 ; [node+0B0h] = 0
//   for (i < [node+0A8h]) { sub_401F90(node+28h[i]) ; = 0 }
//   [node+0A8h]=0 ; [node+1Ch]=4 ; [node+138h]=0 ; [node+278h]=0 ; [node+18h] |= 1
//   [this]=0 ; [+8]=0 [, +0C=0]
// ----------------------------------------------------------------------------
void StrXSlot::Voice_Teardown(bool clearFieldC)
{
    if (GlobalReady == 0)
    {
        Field08 = 0;
        if (clearFieldC) Marker = 0;
        return;
    }

    if (Marker != MarkerToken)
    {
        Field08 = 0;
        if (clearFieldC) Marker = 0;
        return;
    }

    void* const pNode = Node;
    if (pNode == nullptr)
    {
        Field08 = 0;
        if (clearFieldC) Marker = 0;
        return;
    }

    if (Field08 != static_cast<int32>(reinterpret_cast<intptr_t>(NodeGet(pNode, 0x24))))
    {
        Node    = nullptr;
        Field08 = 0;
        if (clearFieldC) Marker = 0;
        return;
    }

    if (Field04 != NodeI32(pNode, 0x138))
    {
        Node    = nullptr;
        Field08 = 0;
        if (clearFieldC) Marker = 0;
        return;
    }

    if ((NodeI32(pNode, 0x18) & 1) != 0)   // asm: test byte ptr [esi+18h],1
    {
        Node    = nullptr;
        Field08 = 0;
        if (clearFieldC) Marker = 0;
        return;
    }

    StrXOwner* pOwner = static_cast<StrXOwner*>(NodeGet(pNode, 0xB0));
    if (pOwner != nullptr &&
        pOwner->field_C0 == static_cast<int32>(reinterpret_cast<intptr_t>(pNode)))
    {
        if ((NodeI32(pNode, 0x18) & 2) != 0)
            pOwner->Stop_Voice();      // asm: call sub_4025B0

        pOwner->Release_Voice();       // asm: call sub_4026F0

        pOwner->field_C0 = 0;
        pOwner->field_BC = 0;
        pOwner->field_B4 = 0;
        pOwner->field_B8 = 0;
        pOwner->field_90 = 0;
        pOwner->field_98 = 0;
        pOwner->field_9C = 0;
        NodePtr(pNode, 0xB0) = nullptr;
    }

    // asm: loc_405E1F —— voice 表在 node+28h，条数每轮从 [node+0A8h] 重读
    {
        void** pEntry = reinterpret_cast<void**>(static_cast<uint8*>(pNode) + 0x28);
        for (int32 i = 0; i < NodeI32(pNode, 0xA8); ++i)
        {
            StrXOwner::Voice_Dec_Ref(pEntry[i]);   // asm: call sub_401F90
            pEntry[i] = nullptr;
        }
    }

    NodeI32(pNode, 0xA8)  = 0;
    NodeI32(pNode, 0x1C)  = 4;
    NodeI32(pNode, 0x138) = 0;
    NodeI32(pNode, 0x278) = 0;
    NodeI32(pNode, 0x18) |= 1;

    Node    = nullptr;
    Field08 = 0;
    if (clearFieldC) Marker = 0;
}

void StrXSlot::StrX_DTOR_0() { Voice_Teardown(true);  }
void StrXSlot::StrX_DTOR_1() { Voice_Teardown(false); }

// ----------------------------------------------------------------------------
// asm: StrX_DTOR_3 (0x4060D9) —— 只做"摘挂"守卫
//
//   if (!GlobalReady) / if ([+0Ch] != MarkerToken) / if (![this])  -> [+8]=0
//   if ([+8] != [node+24h]) / if ([+4] != [node+138h])             -> [0]=0 ; [+8]=0
//   sub = [node+24h] ; if (!(sub[10h] & 1) || sub[4Ch] != 0)       -> [+8]=0
//   if ([node+18h] & 20h)                                          -> [0]=0 ; [+8]=0
//   owner = [node+0B0h] 且 owner->C0 == node:
//       sub_4026A0(owner) ; [node+18h] |= 60h ; sub_4026B0(owner)
//       [0]=0 ; [+8]=0
//   否则 [node+18h] |= 60h (仅低字节) ; [0]=0 ; [+8]=0
// ----------------------------------------------------------------------------
void StrXSlot::StrX_DTOR_3()
{
    if (GlobalReady == 0)
    {
        Field08 = 0;
        return;
    }

    if (Marker != MarkerToken)
    {
        Field08 = 0;
        return;
    }

    void* const pNode = Node;
    if (pNode == nullptr)
    {
        Field08 = 0;
        return;
    }

    void* const pSub = NodeGet(pNode, 0x24);
    if (pSub == nullptr ||
        Field08 != static_cast<int32>(reinterpret_cast<intptr_t>(pSub)))
    {
        Node    = nullptr;
        Field08 = 0;
        return;
    }

    if (Field04 != NodeI32(pNode, 0x138))
    {
        Node    = nullptr;
        Field08 = 0;
        return;
    }

    // asm: test byte ptr [eax+10h],1 / jz loc_4060E7
    if ((*reinterpret_cast<uint8*>(static_cast<uint8*>(pSub) + 0x10) & 1) == 0)
    {
        Field08 = 0;
        return;
    }

    // asm: cmp [eax+4Ch],ebx / jnz loc_4060E7
    if (*reinterpret_cast<int32*>(static_cast<uint8*>(pSub) + 0x4C) != 0)
    {
        Field08 = 0;
        return;
    }

    if ((NodeI32(pNode, 0x18) & 0x20) != 0)
    {
        Node    = nullptr;
        Field08 = 0;
        return;
    }

    StrXOwner* pOwner = static_cast<StrXOwner*>(NodeGet(pNode, 0xB0));
    if (pOwner != nullptr &&
        pOwner->field_C0 == static_cast<int32>(reinterpret_cast<intptr_t>(pNode)))
    {
        pOwner->Guard_Detach();        // asm: call sub_4026A0
        NodeI32(pNode, 0x18) |= 0x60;  // asm: or edx,60h (dword)
        pOwner->Guard_Attach();        // asm: call sub_4026B0
        Node    = nullptr;
        Field08 = 0;
        return;
    }

    // asm: loc_4060DD —— or al,60h（只动低字节）
    *reinterpret_cast<uint8*>(static_cast<uint8*>(pNode) + 0x18) |= 0x60;

    Node    = nullptr;
    Field08 = 0;
}
