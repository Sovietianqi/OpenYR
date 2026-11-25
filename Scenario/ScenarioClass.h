#pragma once

#include <cstdlib>

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <Core/Random2Class.h>
#include <Math/CoordStruct.h>
#include <Math/Timer.h>
#include <Map/CellClass.h>
#include <Rendering/Blitter.h>

class CCINIClass;
class TechnoTypeClass;
class HouseClass;
class ParticleSystemClass;

// ============================================================================
// asm: .data AISlots —— AI 槽位表。原版是四组并行数组（每组 8 项，
//   Assign_Houses 的第二段按 `ebx` 从 StartingSpots 走到 Colours，
//   步长 4，正好 8 轮，其余三组用 +20h/+40h/+60h 取）：
//
//     +0x00 StartingSpots[8]  国家/阵营下标（-1、-3 表示空槽）
//     +0x20 Colours[8]        颜色槽
//     +0x40 StartSlots[8]     起始格（写入 house+16058h）
//     +0x60 Teams[8]          队伍号（写入 house+1605Ch，-1 表示无队伍）
// ============================================================================
struct AISlotsClass {
    int32 AIDifficulties[8];   // -0x20 相对 StartingSpots：该槽的 AI 难度
    int32 StartingSpots[8];
    int32 Colours[8];
    int32 StartSlots[8];
    int32 Teams[8];

    AISlotsClass() noexcept {
        for (int32 i = 0; i < 8; ++i) {
            AIDifficulties[i] = 0;
            StartingSpots[i] = -1;
            Colours[i] = -1;
            StartSlots[i] = -1;
            Teams[i] = -1;
        }
    }
};
extern AISlotsClass AISlots;

// asm: .data ParticleSystem_Gas —— 场景初始化时建好的 GasCloudSys 粒子系统。
extern ParticleSystemClass* ParticleSystem_Gas;

// ============================================================================
// asm: Fill_In_Data (0x684C30, cdecl) —— 场景数据填充的收尾阶段。
// 见 Scenario/ScenarioClass.cpp 的完整实现与逐段 asm 注释。
// ============================================================================
void Fill_In_Data();

// ============================================================================
// Variable struct
// ============================================================================
struct Variable {
    char Name[40];
    int8 Value;

    Variable() : Value(0) { Name[0] = '\0'; }
};

// TintStruct is defined in Rendering/Blitter.h

// ============================================================================
// LightingStruct
// ============================================================================
struct LightingStruct {
    TintStruct Tint;
    int32 Ground;
    int32 Level;

    LightingStruct() : Ground(0), Level(0) {}
};

// ============================================================================
// ScenarioFlags - bitfield of scenario settings
// ============================================================================
struct ScenarioFlags {
    uint32 Raw;

    ScenarioFlags() : Raw(0) {}

    bool GetBit(int32 idx) const { return (Raw & (1u << idx)) != 0; }
    void SetBit(int32 idx, bool val) {
        if (val) Raw |= (1u << idx);
        else Raw &= ~(1u << idx);
    }

    bool CTFMode()              const { return GetBit(4); }
    bool Inert()                const { return GetBit(5); }
    bool TiberiumGrows()        const { return GetBit(6); }
    bool TiberiumSpreads()      const { return GetBit(7); }
    bool MCVDeploy()            const { return GetBit(8); }
    bool InitialVeteran()       const { return GetBit(9); }
    bool FixedAlliance()        const { return GetBit(10); }
    bool HarvesterImmune()      const { return GetBit(11); }
    bool FogOfWar()             const { return GetBit(12); }
    bool TiberiumExplosive()    const { return GetBit(14); }
    bool DestroyableBridges()   const { return GetBit(15); }
    bool Meteorites()           const { return GetBit(16); }
    bool IonStorms()            const { return GetBit(17); }
    bool Visceroids()           const { return GetBit(18); }
};

// ============================================================================
// Randomizer —— 场景同步随机流（asm: ScenarioClass+0x218，类型 RandomizerClass）
//
//   原版这里就是 RandomizerClass（250 槽滞后异或表，见 Core/Random2Class）。
//   早先这份实现是一个 4 字节 LCG，且 Randomize() 用 `this` 指针当种子 ——
//   ScenarioClass 在堆上 new 出来、进程又是 PIE，地址每跑一次都不同，
//   于是同一 seed 的两次运行从第一个随机数起就分叉。已改为原版算法。
// ============================================================================
struct Randomizer : public Random2Class {
    Randomizer() noexcept : Random2Class() {}

    // asm: ScenarioClass_Init → Random_Init(this+0x218, 0)
    //   原版固定用种子 0（对局真正的会话种子由 sub_52FC20 另行灌入）。
    //   回归时允许 OPENYR_SEED 覆盖，便于验证"同种子=同序列"。
    void Randomize() {
        int32 seed = 0;
        if (const char* pSeed = std::getenv("OPENYR_SEED")) {
            seed = static_cast<int32>(std::strtol(pSeed, nullptr, 10));
        }
        Init(seed);
    }

    int32 Next() { return Random2Class::operator()(); }

    int32 Next(int32 minVal, int32 maxVal) {
        return Random2Class::operator()(minVal, maxVal);
    }
};

// ============================================================================
// ScenarioClass - Scenario/mission manager, singleton
// ============================================================================
class ScenarioClass {
public:
    static constexpr int32 MaxWaypoints = 702;
    static constexpr int32 MaxGlobalVariables = 50;
    static constexpr int32 MaxLocalVariables = 100;
    static constexpr int32 MaxStartingPoints = 8;

    // Static singleton
    static ScenarioClass* Instance;

    // ========================================================================
    // Constructor / Destructor
    // ========================================================================
    ScenarioClass();
    ~ScenarioClass();

    // ========================================================================
    // Initialization
    // ========================================================================
    void Init();
    void ClearClasses();

    // asm: ScenarioClass_Pause / ScenarioClass_Resume (asm:698 / 797).  The
    //  original additionally freezes audio/tooltips/mouse; within the scenario
    //  object the semantics are: Pause accumulates the elapsed timer and marks
    //  the game paused, Resume restarts the timer and clears the flag.
    void Pause();
    void Resume();

    // ========================================================================
    // Scenario loading / starting
    // ========================================================================
    static bool StartScenario(const char* FileName, bool Briefing, int32 CampaignIndex);
    bool LoadScenario(const char* pFileName);

    // ------------------------------------------------------------------------
    // asm: Read_Scenario_INI
    //
    // 原版签名是 char __thiscall Read_Scenario_INI(const char* filename)，
    // **ecx 就是文件名**、无栈参、retn 0（调用点：
    //   lea ecx, [esp+248h+var_208] ; call Read_Scenario_INI
    // 见 asm 行 1324770 一带）。AsmComplex6 批注写成
    //   (void* pScenario, const char* pFilename)
    // 是错的：它既不是两栈参，第一个参数也不是 ScenarioClass 指针。
    // 原版函数体内用全局单例 Scenario 取 ScenarioClass，
    // 这里因此实现为普通成员函数，文件名走形参。
    //
    //   流程：
    //     if (!CD::ForceAvailable(ForcedCDNumber)) return 0
    //     CCINIClass 就地构造；CCFileClass 就地以 filename 构造
    //     WWDebugString("Read_Scenario_INI - Filename is %s\n", filename)
    //     if (!CCINIClass::Load(ini, file, 1, 0)):
    //         WWDebugString("Scenario ini load failed!\n") ; 析构 ; return 0
    //     memcpy(Scenario->MapFileName, filename, strlen(filename) + 1)
    //     ok = INIClass_ReadScenario(ini)        (IsRandom = 0)
    //     析构 file / ini ; return ok
    // ------------------------------------------------------------------------
    bool Read_Scenario_INI(const char* pFilename);

    static void AssignHouses();

    // asm: .data someHouse —— 剧本里的"机房主"（Assign_Houses 把
    //   node+6Bh == -1 的那一幢记到这里）。与 PlayerPtr 不同：
    //   PlayerPtr 是本地玩家，someHouse 是剧本指定的主角房。
    static HouseClass* SomeHouse;
    void CreateUnits();
    // asm: Get_Starting_Locations (0x6882D0, __thiscall) —— 收集多人开局可用的
    //   出生格：先取前 8 个已定义的航点，不足时再按地图范围随机补足。
    //   Create_Units 用它做候选起始点表。
    void GetStartingLocations(DynamicVectorClass<CellStruct>& out);
    void EndGame();
    void ReadStartPoints(CCINIClass& ini);

    // Reads the [SpecialFlags] section into SpecialFlags.  Every key uses
    // the bit it currently holds as its fallback, so a partially specified
    // section preserves the scenario's own settings.
    void GetGlobalFlags(CCINIClass* pINI);

    // ========================================================================
    // Global / local variable accessors
    //
    //  The script actions (TeamClass_SetGlobal and friends) and the trigger
    //  conditions read and write the 50 global and 100 local variable slots by
    //  index.  SetGlobalValue / SetLocalValue store 0 or 1 into the slot's
    //  Value; an out-of-range index is ignored.
    // ========================================================================
    void SetGlobalValue(int32 index, bool value);
    void SetLocalValue(int32 index, bool value);
    bool GetGlobalValue(int32 index) const;
    bool GetLocalValue(int32 index) const;

    // ScenarioClass_ReadLocalVariables - the [VariableNames] pass.  The
    // section is purely index-keyed: every key parses as an integer slot,
    // the value splits on ',' into "<name>,<initial value>" and the second
    // token, when it survives, becomes a boolean stored 0x28 bytes into the
    // slot.  All 100 slots are cleared first, so a scenario that names no
    // variables leaves the table empty.
    void ReadLocalVariables(CCINIClass* pINI);

    // The [Ranking] pass at the tail of Scenario_ReadLightingAndBasic.  The
    // three par times go through Get_Time ("%02d:%02d:%02d"), while the four
    // under/over-par captions are ordinary 0x1F-byte string reads.
    void ReadRanking(CCINIClass* pINI);

    // ========================================================================
    // Save / Load
    // ========================================================================
    static bool SaveGame(const char* FileName, const wchar_t* Description, bool BarGraph = false);
    static bool LoadGame(const char* FileName);

    // ========================================================================
    // Lighting
    // ========================================================================
    static void UpdateCellLighting();
    static void UpdateLighting();
    static void UpdateHashPalLighting(int32 R, int32 G, int32 B, bool tint);
    static void ScenarioLighting(int32* r, int32* g, int32* b);
    static void RecalcLighting(int32 R, int32 G, int32 B, bool tint);

    // ========================================================================
    // Waypoints
    // ========================================================================
    bool IsDefinedWaypoint(int32 idx) const;
    // ScenarioClass_NotAHomeCell (asm 0x6E0700 caller): true when the given
    // waypoint does not resolve to this scenario's home cell.
    bool NotAHomeCell(int32 idx) const;
    CellStruct GetWaypointCoords(int32 idx) const;
    void SetWaypointCoords(int32 idx, const CellStruct& cell);

    // ========================================================================
    // Properties
    // ========================================================================
    ScenarioFlags       SpecialFlags;
    char                NextScenario[0x104];
    char                AltNextScenario[0x104];
    int32               HomeCell;
    int32               AltHomeCell;
    int32               UniqueID;
    Randomizer          Random;
    DWORD               Difficulty1;
    DWORD               Difficulty2;
    CDTimerClass        ElapsedTimer;
    CDTimerClass        PauseTimer;
    DWORD               unknown_62C;
    bool                IsGamePaused;
    CellStruct          Waypoints[MaxWaypoints];

    // Map Header
    int32               StartX;
    int32               StartY;
    int32               Width;
    int32               Height;
    int32               NumberStartingPoints;
    Point2D             StartingPoints[MaxStartingPoints];
    int32               HouseIndices[0x10];
    CellStruct          HouseHomeCells[MaxStartingPoints];
    bool                TeamsPresent;
    int32               NumCoopHumanStartSpots;
    CDTimerClass        MissionTimer;
    wchar_t*            MissionTimerTextCSF;
    char                MissionTimerText[32];
    CDTimerClass        ShroudRegrowTimer;
    CDTimerClass        FogTimer;
    CDTimerClass        IceTimer;
    CDTimerClass        unknown_timer_123c;
    CDTimerClass        AmbientTimer;
    // asm ScenarioClass+0x00 —— 场景标志位（flags dd）。bit 0x0400 表示
    //   "该模式不发送外交状态包"（见 RadarClass_Diplomacy 的 sub_731840）。
    int32               flags = 0;
    int32               TechLevel;

    // ------------------------------------------------------------------
    // Map tint (asm ScenarioClass+0x3534 / +0x3538 / +0x353C).
    //
    //  The action 'Retint Red/Green/Blue' overwrites one channel of the map
    //  tint; the value stored here is the raw scenario field, whereas the
    //  lighting routine consumes it pre-multiplied by 10 (the `lea eax,
    //  [eax+eax*4] / shl eax, 1` sequence in the original).
    // ------------------------------------------------------------------
    int32               MapTintR;
    int32               MapTintG;
    int32               MapTintB;
    TheaterType         Theater;
    char                FileName[0x104];
    // asm: ScenarioClass+0x125C —— 联机开局前交换的剧本文件名。
    //       Negotiate_Scenario_Transfer 从主机收到名字后整串拷进这里，
    //       Send_File 也直接以它作为 "Send file %s to clients" 的源。
    char                MapFileName[0x100];
    wchar_t             Name[0x2D];
    char                UIName[0x20];
    wchar_t             UINameLoaded[0x2D];

    // Movies - indices into the global movie table (asm stores FindMovieIndex
    // results directly in these slots; -1 means "unset").
    int32               Intro;
    int32               Brief;
    int32               Win;
    int32               Lose;
    int32               Action;
    int32               PostScore;
    int32               PreMapSelect;

    wchar_t             Briefing[0x400];
    char                BriefingCSF[0x20];
    // asm: ScenarioClass+0x11F4 —— 简报串指针；当 +0x11F8 处的 CSF 键非空时
    //   由 Get_String(键) 填进来，否则写 wNull（= 本工程的 nullptr）。
    //   +0x11F8 就是紧随其后的 BriefingCSF 缓冲首字节。
    const char*         BriefingText;
    int32               ThemeIndex;
    int32               HumanPlayerHouseTypeIndex;
    double              CarryOverMoney;
    int32               CarryOverCap;
    int32               Percent;

    Variable            GlobalVariables[MaxGlobalVariables];
    Variable            LocalVariables[MaxLocalVariables];

    CellStruct          View1;
    CellStruct          View2;
    CellStruct          View3;
    CellStruct          View4;
    DWORD               unknown_34A0;
    bool                FreeRadar;
    bool                TrainCrate;
    bool                TiberiumGrowthEnabled;
    bool                VeinGrowthEnabled;
    bool                IceGrowthEnabled;
    bool                BridgeDestroyed;
    bool                VariablesChanged;
    bool                AmbientChanged;
    bool                EndOfGame;
    bool                TimerInherit;
    bool                SkipScore;
    bool                OneTimeOnly;
    bool                SkipMapSelect;
    bool                TruckCrate;
    bool                FillSilos;
    bool                TiberiumDeathToVisceroid;
    bool                IgnoreGlobalAITriggers;
    // asm: ScenarioClass+0x34A5 —— “允许空袭”标志，由动作 sub_6E3B20 取反切换。
    bool                AirstrikeAllowedFlag;
    bool                unknown_bool_34B5;
    bool                unknown_bool_34B6;
    bool                unknown_bool_34B7;
    int32               PlayerSideIndex;
    bool                MultiplayerOnly;
    bool                IsRandom;
    bool                PickedUpAnyCrate;
    CDTimerClass        unknown_timer_34C0;
    int32               CampaignIndex;
    int32               StartingDropships;
    // asm: ScenarioClass+0x3528 / +0x3530 —— 闪电风暴时长设定值与生效值，
    //   由动作 sub_6E2F4F 写入。
    int32               LightningStormDuration;
    int32               LightningStormActiveDuration;
    DynamicVectorClass<TechnoTypeClass*> AllowableUnits;
    DynamicVectorClass<int32> AllowableUnitMaximums;
    DynamicVectorClass<int32> DropshipUnitCounts;

    // General Lighting
    int32               AmbientOriginal;
    int32               AmbientCurrent;
    int32               AmbientTarget;
    LightingStruct      NormalLighting;

    // Ion lighting
    int32               IonAmbient;
    LightingStruct      IonLighting;

    // Nuke flash lighting
    int32               NukeAmbient;
    LightingStruct      NukeLighting;
    int32               NukeAmbientChangeRate;

    // Dominator lighting
    int32               DominatorAmbient;
    LightingStruct      DominatorLighting;
    int32               DominatorAmbientChangeRate;

    DWORD               unknown_3598;
    int32               InitTime;
    int16               Stage;
    bool                UserInputLocked;
    bool                unknown_35A3;
    int32               ParTimeEasy;
    int32               ParTimeMedium;
    int32               ParTimeDifficult;
    char                UnderParTitle[0x1F];
    char                UnderParMessage[0x1F];
    char                OverParTitle[0x1F];
    char                OverParMessage[0x1F];
    char                LSLoadMessage[0x1F];
    char                LSBrief[0x1F];
    int32               LS640BriefLocX;
    int32               LS640BriefLocY;
    int32               LS800BriefLocX;
    int32               LS800BriefLocY;
    char                LS640BkgdName[0x40];
    char                LS800BkgdName[0x40];
    char                LS800BkgdPal[0x40];

    // Convenience aliases
    const char*         ScenarioName;
    const char*         ScenarioDescription;
    const char*         ScenarioFileName;
    bool                IsMultiplayer;
    bool                IsCampaign;
    int32               Difficulty;
    int32               InitialMoney;
    int32               MapWidth;
    int32               MapHeight;
    bool                IsSkirmish;
    bool                IsBridgeDestructionEnabled;
    bool                IsFogOfWar;
    bool                IsMCVRepack;
    bool                IsShortGame;
    bool                IsCrates;
    bool                IsSuperWeapons;
    bool                IsMultiEngineer;
    bool                IsBuildOffAlly;
    bool                IsBases;
    int32               RandomSeed;
    int32               FrameCount;
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知场景层补全：秘密科技生产分配、航点装卸、全局/
    // 本地变量装卸、运兵船清单、下一任务推进、留分结算与多人起点变体。
    // ------------------------------------------------------------------------
    virtual TechnoTypeClass* AssignSecretProduction(HouseClass* pHouse);
    virtual void   ClearWaypoints();
    virtual void   GenerateDropshipLoadout(HouseClass* pHouse);
    virtual int32  Get_Next_ID();
    virtual void   InitMoreBuffers();
    virtual void   LoadWaypoints(CCINIClass* pINI);
    virtual CellClass* LocateWaypoint(int32 idx);
    virtual int32  MPIdxToHouseIdx(int32 mpIdx) const;
    virtual void   PauseForSeconds(int32 seconds);
    virtual void   ProceedToNextMission();
    virtual void   PutGlobalFlags(CCINIClass* pINI);
    virtual void   ReadGlobalVariables(CCINIClass* pINI);
    virtual void   SaveMap(CCINIClass* pINI) const;
    virtual void   SaveWaypoints(CCINIClass* pINI) const;
    virtual void   ShowCredits();
    virtual void   WriteLocalVariables(CCINIClass* pINI) const;

    // asm: ScenarioClass_GetCRC(CRC* this)  (asm:1765): folds the scenario's
    //  identity/state fields into the running CRC.  The original walks a fixed
    //  sequence of raw object offsets; here the equivalent named fields are
    //  folded in the same order through the tree's CRC engine.
    void           GetCRC(CRCEngine& crc);

    // “Scenario 全局入口”族（对应原版 Scenario 池）
    static void    GenerateRandomCountries();
    static CellClass* GetWaypointCell(int32 idx);
    static void    GuestReceiveOptions();
    static int32   NameToStartingSlot(const char* pName);
    static bool    ReadBasic(CCINIClass* pINI);
    static void    ReadLightingAndBasic(CCINIClass* pINI);
    static void    RecalcTint();
    static void    ResetAllSuperWeapons();
    static void    Smth();
    static void    SmthStartingHouses();
    static void    SmthStartingHouses5();
    static void    SmthStartingHouses6();
    void           WriteLightingBasic(CCINIClass* pINI);

    int32 PauseTicks = 0;           // 场景暂停剩余秒数（由场景更新递减）
    DynamicVectorClass<int32> DropshipLoadout;  // 运兵船装载清单

    // ----------------------------------------------------------------------
    // asm: ScenarioClass+0x614 —— "载入耗时"计时器（LoadGame_slave2 专用）
    //   +0x00 int32          Start        （-1 = 停表）
    //   +0x04 WinTimerClass  子对象        （本工程只保留最近一次时基读数）
    //   +0x08 int32          Accumulated  （累计毫秒）
    // ----------------------------------------------------------------------
    struct LoadElapsedTimer {
        int32 Start;          // +0x00
        int32 LastTick;       // +0x04（降级：原版是 WinTimerClass 子对象）
        int32 Accumulated;    // +0x08
    };
    LoadElapsedTimer LoadTimer = { -1, 0, 0 };

    // ----------------------------------------------------------------------
    // asm: LoadGame_slave2 (0x689470, __fastcall, retn 4)
    //   ScenarioClass *this / 栈参 = 存档文件对象
    //   累积停表时间 -> 读 0x3740 字节存档块 -> 重建三张 TypeList
    //   （+34D4 TechnoType* / +34F0 int / +350C int）-> Swizzle +34D8/+34E4
    //   -> 恢复简报串（+11F4 / +11F8）-> 重启计时器 -> 打印耗时。
    // ----------------------------------------------------------------------
    void Load_Game_Payload(CCFileClass* pFile);

};

// ============================================================================
// Static member definition
// ============================================================================
inline ScenarioClass* ScenarioClass::Instance = nullptr;