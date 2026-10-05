#pragma once

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <Math/CoordStruct.h>
#include <Math/Timer.h>
#include <Map/CellClass.h>
#include <Rendering/Blitter.h>

class CCINIClass;
class TechnoTypeClass;

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
// Randomizer - simple pseudo-random number generator
// ============================================================================
struct Randomizer {
    int32 Seed;

    Randomizer() : Seed(0) {}

    void Randomize() {
        Seed = static_cast<int32>(reinterpret_cast<uintptr_t>(this) ^ 0x12345678);
    }

    int32 Next() {
        Seed = Seed * 1103515245 + 12345;
        return (Seed >> 16) & 0x7FFF;
    }

    int32 Next(int32 minVal, int32 maxVal) {
        if (minVal >= maxVal) return minVal;
        int32 range = maxVal - minVal + 1;
        return minVal + (Next() % range);
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

    // ========================================================================
    // Scenario loading / starting
    // ========================================================================
    static bool StartScenario(const char* FileName, bool Briefing, int32 CampaignIndex);
    bool LoadScenario(const char* pFileName);
    static void AssignHouses();
    void CreateUnits();
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
    wchar_t             Name[0x2D];
    char                UIName[0x20];
    wchar_t             UINameLoaded[0x2D];

    // Movies
    const char*         Intro;
    const char*         Brief;
    const char*         Win;
    const char*         Lose;
    const char*         Action;
    const char*         PostScore;
    const char*         PreMapSelect;

    wchar_t             Briefing[0x400];
    char                BriefingCSF[0x20];
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
    static void    WriteLightingBasic(CCINIClass* pINI);

    int32 PauseTicks = 0;           // 场景暂停剩余秒数（由场景更新递减）
    DynamicVectorClass<int32> DropshipLoadout;  // 运兵船装载清单

};

// ============================================================================
// Static member definition
// ============================================================================
inline ScenarioClass* ScenarioClass::Instance = nullptr;