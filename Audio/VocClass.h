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

// [Defaults] Priority= is matched against this table; the stored value is the
// table position, "NORMAL" (2) being the default.
enum class SoundPriority : int32 {
    Lowest   = 0,
    Low      = 1,
    Normal   = 2,
    High     = 3,
    Critical = 4
};

// Control= is a whitespace separated run of these tokens OR-ed together.
enum SoundControlFlags : int32 {
    SoundControl_None      = 0x00,
    SoundControl_Loop      = 0x01,
    SoundControl_Random    = 0x02,
    SoundControl_All       = 0x04,
    SoundControl_Predelay  = 0x08,
    SoundControl_Interrupt = 0x10,
    SoundControl_Attack    = 0x20,
    SoundControl_Decay     = 0x40,
    SoundControl_Ambient   = 0x80
};

// Type= is a whitespace separated run of these tokens.  The low nibble and
// bits 0x0C are mutually exclusive groups, so a new token clears the group
// it belongs to before it is OR-ed in.
enum SoundTypeFlags : int32 {
    SoundType_Normal   = 0x0000,
    SoundType_Violent  = 0x0001,
    SoundType_Movement = 0x0002,
    SoundType_Quiet    = 0x0004,
    SoundType_Loud     = 0x0008,
    SoundType_Global   = 0x0010,
    SoundType_Screen   = 0x0020,
    SoundType_Local    = 0x0040,
    SoundType_Player   = 0x0080,
    SoundType_NoiseShy = 0x0100,
    SoundType_GunShy   = 0x0200,
    SoundType_Unshroud = 0x0400,
    SoundType_Shroud   = 0x0800,
    SoundType_Ambient  = 0x1000
};

// Number of samples a single sound may reference.  VocClass_AddSample bails
// out as soon as this many entries are present.
static constexpr int32 MAX_SAMPLES_PER_SOUND = 32;

// One entry of [SoundList].  "Volume" is scaled to the 0..16384 range the
// mixer works with, "MinVolume" is kept as a 0..1 fraction.  The member order
// follows the binary so that the offsets every VocClass_Set* helper writes
// line up with the layout the original code walks.
struct SoundDefinition {
    int32             Unk00;                     // +00
    int32             Flags;                     // +04
    int32             IsLoaded;                  // +08
    bool              Valid;                     // +0C  cleared when a sample is missing
    uint8             pad0D[3];                  // +0D
    int32             Control;                   // +10
    int32             Type;                      // +14
    int32             Volume;                    // +18
    uint8             pad1C[0x24];               // +1C
    int32             Priority;                  // +40
    int32             Limit;                     // +48
    int32             Loop;                      // +4C
    int32             Range;                     // +50
    double            MinVolume;                 // +54
    int32             Delay[2];                  // +58, +5C
    int32             FDelta[2];                 // +60, +64
    int32             VShift;                    // +68
    char              Name[0x20];                // +6C
    uint8             pad8C[0xA8];               // +8C .. +133
    int32             SampleCount;               // +134
    int32             SampleIndex[MAX_SAMPLES_PER_SOUND]; // +0B4

    SoundDefinition();
    ~SoundDefinition();

    const char* GetName() const;
    void SetName(const char* pName);
    bool AddSample(const char* pName);

    void SetVolume(int32 volume);
    void SetVShift(int32 vshift);
    void SetMinVolume(double minVolume);
    void SetPriority(SoundPriority priority);
    void SetAttack(int32 attack);
    void SetDecay(int32 decay);
    void SetControl(int32 control);
    void SetType(int32 type);
    void SetLimit(int32 limit);
    void SetLoop(int32 loop);
    void SetRange(int32 range);
    void SetDelay(int32 delay, int32 delay2);
    void SetFDelta(int32 fdelta, int32 fdelta2);

    bool LoadFromINI(class CCINIClass* pINI);
};

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


    // -------------------------------------------------------------------
    // Sound name registry
    //
    // The engine resolves every INI sound reference (VoiceMove, Report,
    // CreateSound, ...) to a stable index into this table.  Names are
    // registered while the sound list is parsed and looked up afterwards;
    // an unknown name yields -1 so the caller keeps its previous value.
    // -------------------------------------------------------------------
    static int32 RegisterSoundName(const char* pName);
    static int32 FindIndexOfName(const char* pName);
    static const char* GetSoundName(int32 index);
    static int32 GetSoundCount();
    static void ClearSoundRegistry();

    // 根据游戏行为，可知通道回退查找与按位播放是声音面的两个
    // 底层入口：回退沿最近使用顺序取上一个，按位播放把索引绑定
    // 到世界坐标。
    static int32 GetPreviousFromHead();
    static int32 GetPrevious();
    static const char* FindNameByIndex(int32 index);
    static void SaveData();
    static void UpdateAtLocation(int32 index, void* pAnchor);
    static bool PlayIndexAtPos(int32 index, const struct CoordStruct* pPos);

    // -------------------------------------------------------------------
    // Sound definition list
    //
    // sound(md).ini carries a [Defaults] block holding the values every
    // sound starts from and a [SoundList] block whose key names are the
    // sound IDs.  Each of those IDs then has a section of its own holding
    // the sample names and the playback parameters.
    // -------------------------------------------------------------------
    static void CreateFromINIList(class CCINIClass* pINI);
    static void DeleteAll();
    static SoundDefinition* FindSound(const char* pName);
    static SoundDefinition* GetSound(int32 nIndex);
    static int32 GetSoundDefinitionCount();

    static SoundPriority ParseSoundPriority(SoundPriority nCurrent, const char* pValue,
                                            bool* pMatched = nullptr);
    static int32 ParseSoundControl(int32 nCurrent, const char* pValue,
                                   bool* pMatched = nullptr);
    static int32 ParseSoundType(int32 nCurrent, const char* pValue,
                                bool* pMatched = nullptr);

    static int32 AddSampleIndex(int32 index);

    static double   DefaultVolume;
    static double   DefaultMinVolume;
    static SoundPriority DefaultPriority;
    static int32    DefaultControl;
    static int32    DefaultType;
    static int32    DefaultLimit;
    static int32    DefaultRange;

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