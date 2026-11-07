#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class CCINIClass;

// ============================================================================
// VoxClass - one EVA / speech line.
//
//   The original keeps a fixed pool of VoxClass objects, one per entry of the
//   [DialogList] section in ra2md.ini.  Each object is 0x54 bytes:
//
//     +00  ID[0x28]        the INI key name (the dialog identifier)
//     +28  Volume          float, default 1.0
//     +2C  Yuri[9]         the Yuri-side sample name
//     +35  Russian[9]      the Soviet-side sample name
//     +3E  Allied[9]       the Allied-side sample name
//     +47  (pad)
//     +48  Priority        int, default 1 (0 LOW, 1 NORMAL, 2 IMPORTANT, 3 CRITICAL)
//     +4C  Type            int, default 0 (0 STANDARD, 1 QUEUE,
//                                          2 INTERRUPT, 3 QUEUED_INTERRUPT)
//     +50  field_50        int, default 2
//
//   The three sample names hold at most eight characters plus a terminator,
//   which the original writes with a plain strncpy(..., 9).
// ============================================================================

class VoxClass
{
public:
    // Priority tokens the "Priority" key accepts.
    enum PriorityType : int32 {
        Priority_Low       = 0,
        Priority_Normal    = 1,
        Priority_Important = 2,
        Priority_Critical  = 3
    };

    // Type tokens the "Type" key accepts.
    enum VoxType : int32 {
        VoxType_Standard         = 0,
        VoxType_Queue            = 1,
        VoxType_Interrupt        = 2,
        VoxType_QueuedInterrupt  = 3
    };

    VoxClass();
    ~VoxClass();

    // VoxClass_LoadFromINI - reads the section named after this object's ID.
    bool LoadFromINI(CCINIClass* pINI);

    // Reads a single EVA line.  Kept as a free-standing helper so the list
    // walker can reuse it for both the reserved and the allocated objects.
    void Read(CCINIClass* pINI);

    // VoxClass_CreateFromINIList - walks [DialogList], allocating one object
    // per dialog identifier that is not already registered.
    static void CreateFromINIList(CCINIClass* pINI);

    // ── +00 ──────────────────────────────────────────────────────────────
    char    ID[0x28];

    // ── +28 ──────────────────────────────────────────────────────────────
    float   Volume;

    // ── +2C ──────────────────────────────────────────────────────────────
    char    Yuri[9];        // +2C
    char    Russian[9];     // +35
    char    Allied[9];      // +3E

    // ── +48 ──────────────────────────────────────────────────────────────
    int32   Priority;
    int32   Type;
    int32   field_50;       // default 2

    // ── Global dialog list ───────────────────────────────────────────────
    static DynamicVectorClass<VoxClass*>* Array;

    // -------------------------------------------------------------------
    // 播放面（asm 0x7524xx..0x752Bxx）
    //
    // 原版用一组全局量驱动 EVA：
    //   SoundIndices+0Ch  表长             (+0x0C)
    //   SoundIndices+14h  当前语音句柄槽
    //   SoundIndices+20h  正在播的 VoxClass*
    //   SoundIndices+24h  阵营号（0=通用 1=苏军 2=尤里）
    //   SoundIndices+28h  语音通道（非空才可播）
    //   SoundIndices+2C/30 播放计时（64 位）
    //   dword_B1D3D8      全局语音静音开关
    // -------------------------------------------------------------------
    static void  PlayFromIndex(int32 index, int32 typeFlags, int32 priorityFlags);
    static void  PlayFromName(const char* pName, int32 typeFlags, int32 priorityFlags);
    static void  PlayEVASideSpecific();
    static void  StopIndex(int32 index);

    static const char* FindByIndex(int32 index);   // asm: VoxClass_FindByIndex
    static int32 FindIndex(const char* pID);       // asm: VoxClass_FindIndex
    static void  DeleteAll();                      // asm: VoxClass_DeleteAll

    static bool  Get_Speech_Enabled();             // asm: dword_B1D3D8
    static void  Set_Speech_Enabled(bool on);
    static void  Set_Voice_Side(int32 side);       // asm: SoundIndices+24h
    static int32 Get_Voice_Side();
    static VoxClass* Get_Current();                // asm: SoundIndices+20h

    // 音频后端未建模：AudioStream_Play_WAV 以可注入回调落点，返回是否
    // 成功开播；默认回调只记录样本名并视为成功（静默环境）。
    typedef bool (*VoiceBackendProc)(const char* pFileName);
    static void Set_Voice_Backend(VoiceBackendProc pProc);

    static VoxClass* Find(const char* pID);
    static void Clear();
};
