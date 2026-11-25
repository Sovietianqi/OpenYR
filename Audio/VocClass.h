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
    int32             Attack;                    // +138
    int32             Decay;                     // +13C

    // asm: Sound_Effect 里的 "mov edi,[ecx]" —— 音效定义自带的静态坐标
    // （环境音）。全零表示该声没有固定声源，不做距离衰减。
    CoordStruct       SelfPosition;

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

// ---------------------------------------------------------------------------
// 声音对象的链表节点。原版把每个 VocClass 挂进一条 intrusive 环形链表：
// 节点 +0x00 = 前驱(NearLink)，+0x04 = 后继(FarLink)，首节点即表头哨兵。
// asm: VocCtrl_Add / sub_407440 / VocClass_RemoveFromList / VocClass_GetPrevious
// 都只操作这两个指针，故这里只需一个两指针的基类即可复刻其语义。
// ---------------------------------------------------------------------------
struct VocClassHeader {
    VocClassHeader* NearLink;   // +0x00
    VocClassHeader* FarLink;    // +0x04

    VocClassHeader() : NearLink(this), FarLink(this) {}
};

// ---------------------------------------------------------------------------
// asm: AudioMiniBuffer (0x3C 字节) —— 一条"正在播放的静态音效"记录。
//
// 原版把每条实例挂进 AudioBufferNewest 这条环形双向链表（VocCtrl_Add 插入
// 到表头之后），节点偏移已由 asm 证实：
//   +0x00 NearLink      前驱                (VocClass_GetPrevious 读 [ecx])
//   +0x04 FarLink       后继
//   +0x0C Handle        StrX 句柄槽         (PlayIndexAtPos: StrX_Init(esi+0Ch))
//   +0x14 Owner         关联的 VocClass*    (SaveData: mov ecx,[edi+14h])
//   +0x18 Voice         引擎 voice 句柄     (SaveData: mov ecx,[edi+24h] 前的 [edi+20h])
//   +0x20 Position      dwXYZ (3×int32)     (SaveData 原样写 12 字节)
//   +0x2C Argument      PlayIndexAtPos 的第 3 参
//   +0x30 Countdown     剩余帧数
// ---------------------------------------------------------------------------
struct VocInstance {
    VocClassHeader Link;       // +0x00 / +0x04
    int32          Pad08;      // +0x08
    void*          Handle;     // +0x0C
    SoundDefinition* Owner;    // +0x14
    void*          Voice;      // +0x18
    CoordStruct    Position;   // +0x20
    int32          Argument;   // +0x2C
    int32          Countdown;  // +0x30

    VocInstance() : Pad08(0), Handle(nullptr), Owner(nullptr), Voice(nullptr),
                    Position(0, 0, 0), Argument(0), Countdown(0) {}
};

// ---------------------------------------------------------------------------
// asm: StrX_406130 / StrX_406170 / StrX_4060F0 的最小语义替身。
//
// 原版把"某个对象当前挂着的音效"放在一个 StrX 槽里，提供三个操作：取句柄、
// 取该句柄绑定的坐标、把句柄写回。对象/渲染层在本工程中尚未建模，故以纯
// 虚接口固化这三条语义，由持有音效的对象按需实现（例如 AnimClass 的 Report
// 音效槽）。
// ---------------------------------------------------------------------------
class VocSoundAnchor {
public:
    virtual ~VocSoundAnchor() {}
    virtual void* Get_Current_Sound() const = 0;              // asm: StrX_406130
    virtual const CoordStruct* Get_Current_Sound_Coords() const = 0; // StrX_406170
    virtual void  Set_Current_Sound(void* pSound) = 0;        // asm: StrX_4060F0
};

// ---------------------------------------------------------------------------
// asm: 音效归属对象 —— StrX 槽指向的播放实例（节点）的持有者。
//
// 节点（slot +0x00）只被这些析构函数按偏移读写；节点自己的 +0xB0 回指本
// 对象，本对象的 +0xC0 又回指节点（双向绑定）。asm 里 sub_4026A0 / 4026B0 /
// 4025B0 / 4026F0 的 `this` 就是它（IDA 注释误标成 VocClass*）：
//   +0x90/0x98/0x9C/0xB4/0xB8/0xBC/0xC0  int32 —— 摘链时一并清零
//   +0xA4  int32  状态位（bit0 | bit2 = "样本已被换掉"）
//   +0xD4  void*  声卡设备对象（虚表 +0x10 / +0x28 / +0x2C 三个槽）
//   +0x168 int32  设备计数器
// ---------------------------------------------------------------------------
class StrXOwner {
public:
    // asm: sub_4026A0 —— 设备 vt+0x28
    void Guard_Detach();
    // asm: sub_4026B0 —— 设备 vt+0x2C
    void Guard_Attach();
    // asm: sub_4025B0 —— 停声：vt+28、按位 vt+0x10、vt+2C、回调 [+0BCh]、清 +168
    void Stop_Voice();
    // asm: sub_4026F0 —— 清 bit4 后再 vt+28 / vt+2C
    void Release_Voice();

    // asm: sub_401F90 —— voice 条目引用计数递减（下限 0），返回递减后的值
    static int32 Voice_Dec_Ref(void* pVoice);

    int32 field_90;   // +0x90
    int32 field_98;   // +0x98
    int32 field_9C;   // +0x9C
    int32 flags_A4;   // +0xA4
    int32 field_B4;   // +0xB4
    int32 field_B8;   // +0xB8
    int32 field_BC;   // +0xBC
    int32 field_C0;   // +0xC0  当前节点（与 slot->Node 同值）
    void* Device;     // +0xD4
    int32 field_168;  // +0x168
};

// ---------------------------------------------------------------------------
// asm: someStructX (sizeof = 0x14) —— "某个对象当前挂着的音效"句柄槽。
//   AnimClass 里是 someStruct1/2、strX，TechnoClass 里是 StrX_3..6。
//   +0x00 Node     播放实例节点
//   +0x04 Field04  与 [node+138h] 比对
//   +0x08 Field08  与 [node+24h] 比对；每条出口都清 0
//   +0x0C Marker   asm: offset VocGlobalA.SoundCount+3D8h —— 归属标记（常数）
//   +0x10 Field10  本组函数不触碰
// ---------------------------------------------------------------------------
class StrXSlot {
public:
    // asm: StrX_Init
    void StrX_Init();
    // asm: StrX_DTOR_0 (0x405C40) —— 全量拆除，并在每条出口清 +0x0C
    void StrX_DTOR_0();
    // asm: StrX_DTOR_1 (0x405D40) —— 全量拆除
    void StrX_DTOR_1();
    // asm: StrX_DTOR_3 (0x4060D9) —— 只做"摘挂"守卫，不拆 voice 表
    void StrX_DTOR_3();

    // asm: VocGlobalA.field_140+3D8h —— 非 0 才允许拆除
    static int32 GlobalReady;
    // asm: offset VocGlobalA.SoundCount+3D8h —— 写进 Marker 的归属常数
    static int32 MarkerToken;

    void* Node;      // +0x00
    int32 Field04;   // +0x04
    int32 Field08;   // +0x08
    int32 Marker;    // +0x0C
    int32 Field10;   // +0x10

private:
    // DTOR_0 / DTOR_1 的公共体；clearFieldC 决定是否多清 +0x0C
    void Voice_Teardown(bool clearFieldC);
};

// ---------------------------------------------------------------------------
// asm: IStream::Write(vt+0x10) 的最小替身。SaveData 只用到"写一段字节"，
// COM 流在本工程未建模，故以纯虚接口固定同样的字节序列。
// ---------------------------------------------------------------------------
class VocSaveStream {
public:
    virtual ~VocSaveStream() {}
    virtual bool Write(const void* pData, int32 length) = 0;
};

class VocClass {
public:
    VocClass();
    ~VocClass();

    bool Load(const char* filename);
    bool IsWAVFile(const uint8* data, int32 size);
    bool IsVOCFile(const uint8* data, int32 size);
    // asm: RA2/YR 的语音与音效来自 .aud（Westwood 容器 + IMA/WW ADPCM）。
    //   AudFile.h 负责容器与解码，VocClass 只接管 PCM 结果。
    bool IsAUDFile(const uint8* data, int32 size);
    bool ParseWAV(const uint8* data, int32 size);
    bool ParseVOC(const uint8* data, int32 size);
    bool ParseAUD(const uint8* data, int32 size);
    bool ParseRaw(const uint8* data, int32 size);
    bool AllocateVocData(const uint8* data, int32 dataSize);
    void Unload();

    void Play();
    void Stop();

    // -------------------------------------------------------------------
    // 采样库直取
    //
    // 零售素材里声音不在散落文件里，而是 audio.bag 中的一块，靠 audio.idx
    // 按名索引。LoadFromAudioBank 把"名字 -> 字节 -> 解码 -> PCM"这段接通；
    // FindOrLoadShared 在进程内缓存同一个名字的 VocClass，避免每次播报都
    // 重新解一遍 ADPCM（EVA 短句重播频率很高）。
    // -------------------------------------------------------------------
    bool LoadFromAudioBank(const char* pName);
    bool LoadFromAudioBankByIndex(int32 index);
    static VocClass* FindOrLoadShared(const char* pName);
    static VocClass* FindOrLoadByIndex(int32 index);
    static void      ClearSharedCache();
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
    static const char* FindNameByIndex(int32 index);
    static bool SaveData(VocSaveStream* pStream);
    static void UpdateAtLocation(const CoordStruct* pPos, VocSoundAnchor* pAnchor);
    static VocInstance* PlayIndexAtPos(int32 index, const CoordStruct* pPos,
                                       int32 argument = 0);

    // -------------------------------------------------------------------
    // AudioBufferNewest —— 静态音效实例链表（asm 全局 dword_B1D3A8/AC 为
    // 当前条数与历史峰值）。这些入口只操作链表本身，不触碰声卡后端。
    //   GetPreviousFromHead : asm 518  （表头 -> 最旧一条）
    //   GetPrevious         : asm 551  （节点 -> 其前驱，遇到表头哨兵返回 0）
    // -------------------------------------------------------------------
    static VocInstance* AllocInstance();                 // 池取节点
    static void         FreeInstance(VocInstance* pInst);
    static void         AddInstanceToNewest(VocInstance* pInst);  // VocCtrl_Add
    static VocInstance* GetPreviousFromHead();           // GetPreviousFromHead
    static VocInstance* GetPrevious(VocInstance* pInst); // GetPrevious
    static void         RemoveInstanceFromList(VocInstance* pInst); // RemoveFromList
    static void         ResetInstances();                // DeleteAll 的链表段
    static int32        GetInstanceCount();              // dword_B1D3A8
    static int32        GetInstancePeak();               // dword_B1D3AC

    // asm: sub_750AC0 —— 距离衰减。返回 0..1 的音量系数并把声像写入
    // *pPan（0..0x4000，0x2000 为正中）。源坐标为世界坐标，像素投影
    // 在本工程尚未建模，故以世界 XY 距离等价折算（见 .cpp 注释）。
    static float ComputeAttenuation(const CoordStruct& listener, int32 range,
                                    const CoordStruct& source, int32* pPan);

    // asm: VocClass_FindIndex —— 在 VocClass 数组里按名字大小写不敏感地
    // 查下标，未命中返回 -1。语义与 FindIndexOfName 完全等价，直接转调。
    static int32 FindIndex(const char* pName);

    // -------------------------------------------------------------------
    // asm: VocClass_RemoveFromList (__fastcall, VocClass *this)
    //   把 this 从它所在的环形链表里摘掉，并把自己恢复成自环（空节点）。
    // asm: VocClass_GetPrevious / GetPreviousFromHead
    //   __fastcall(VocClassHeader *this) -> VocClass*，返回 NearLink，
    //   但当 this 为空、NearLink 为空或该节点就是表头（自环）时返回 nullptr。
    // asm: VocClass_SaveData (IStream *this)
    //   复刻：先写入音频缓冲长度(4B)，再遍历链表写出每个声音的名字下标
    //   (4B) 与 [ +0x20, +0x24, +0x28 ](共 16B)。每段写失败即以该
    //   HRESULT 提前返回。
    // -------------------------------------------------------------------
    void RemoveFromList();
    VocClassHeader* GetPreviousFromList();
    VocClassHeader* GetPreviousFromHeadEntry();

    // -------------------------------------------------------------------
    // asm: Sound_Effect (0x7509D0)   (retn 4)
    //
    //   Sound_Effect proc near        ; dump 无 `; void ...` 签名注释行
    //   按函数体推断：__fastcall(ecx=voiceIdx, edx=a1, 栈 a2=pWhat) -> void*
    //   调用者极多（20+ 处，如 AircraftClass_Mi_SpyPlaneApproach）。
    //
    //   mov al,byte_8464AC / test al,al / jnz loc_7509F2 ; 关闭时直接返回 0
    //   test ecx,ecx / jl loc_750A0F
    //   cmp ecx,VocClass.length / jge loc_750A0F         ; 索引越界保护
    //   mov eax,VocClass.data / mov ecx,[eax+ecx*4] / mov edi,[ecx]
    //                                                    ; edi = 该声的坐标槽
    //   若 pWhat: pHandle = StrX_406130(pWhat)
    //             if (StrX_406310(pHandle) == coords) {
    //                 StrX_4052F0(pHandle) ; pHandle = 0     ; 同位置去重
    //             }
    //   若 coords != 0:
    //     volume = sub_750AC0(a1, &pWhat, coords)        ; 距离衰减（ST0）
    //     if (volume <= 0.0f):                           ; 听不见
    //        if (pHandle) StrX_4052F0(pHandle) ; pHandle = 0
    //     else:  ... 按 volume*16384 播放并绑定 StrX 句柄 ...
    //   return pHandle
    //
    // 铁律①：sub_750AC0(衰减) / StrX_406130 / StrX_406310 / StrX_4052F0 /
    //        StrX_405190 / StrX_4061D0 / StrX_406270 / StrX_4060F0 与全局
    //        byte_8464AC 均尚未落地，此处只固化已确证的入口语义、越界保护、
    //        去重判定与衰减门槛，不臆造实现。
    // -------------------------------------------------------------------
    static bool  Get_Sound_Effect_Enabled();         // asm: byte_8464AC
    static void  Set_Sound_Effect_Enabled(bool on);
    static void* Sound_Effect(int32 index, const CoordStruct* pPos,
                              VocSoundAnchor* pAnchor);

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

    // -------------------------------------------------------------------
    // asm: init_vec_Voc (0x750340) —— 全局 vec_Voc（DynamicVectorClass<
    //   VocClass*>）的静态铺底
    //   data=0 / count=0 / IsInitialized=1 / IsAllocated=0 /
    //   vtable=DynamicVectorClass<VocClass*>::vftable / incrementAmount=0Ah /
    //   length=0，随后 _atexit(loc_750340) 注册析构：析构把 vtable 回落成
    //   VectorClass<VocClass*>，IsAllocated 为真时 delete data，再清
    //   IsAllocated / count。
    //   本工程以函数内静态对象表达同一语义（同 MixFileClass::sctor2 的手法）。
    // -------------------------------------------------------------------
    static DynamicVectorClass<VocClass*>& Vec_Voc();
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