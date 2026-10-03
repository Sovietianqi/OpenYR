#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"

enum class TAction {
    None = 0,
    WinGame = 1,
    LoseGame = 2,
    Production = 3,
    CreateTeam = 4,
    ReinforceTeam = 5,
    ChangeHouse = 6,
    ChangeAI = 7,
    PlayMovie = 8,
    TextTrigger = 9,
    DestroyTeam = 10,
    DestroyAll = 11,
    DestroyBuilding = 12,
    DestroyUnit = 13,
    DestroyInfantry = 14,
    DestroyEntity = 15,
    RevealMap = 16,
    UnrevealMap = 17,
    RevealWaypoint = 18,
    RevealArea = 19,
    PlaySound = 20,
    PlayMusic = 21,
    PlaySpeech = 22,
    ForceFire = 23,
    TimerStart = 24,
    TimerStop = 25,
    TimerSet = 26,
    TimerAdd = 27,
    TimerSubtract = 28,
    TimerExpired = 29,
    GlobalSet = 30,
    GlobalClear = 31,
    AutoBase = 32,
    GrowShroud = 33,
    DestroyAttached = 34,
    FlashTeam = 35,
    Reinforcement = 36,
    Airstrike = 37,
    SpySat = 38,
    IonStorm = 39,
    NukeStrike = 40,
    LightningStrike = 41,
    ChronoWarp = 42,
    IronCurtain = 43,
    ParaDrop = 44,
    PsychicDominator = 45,
    GeneticMutator = 46,
    ForceShield = 47
};

class TActionClass {
public:
    static DynamicVectorClass<TActionClass*>* Array;

    static TActionClass* Find(const char* pID);
    static TActionClass* FindOrAllocate(const char* pID);

    TActionClass(const char* pID) noexcept;
    virtual ~TActionClass();

    bool LoadFromINIList(CCINIClass* pINI);
    bool SaveToINIList(CCINIClass* pINI);

    void ExecuteAction(TriggerClass* pTrigger);
    void GetActionName(char* buffer, int32 bufferSize) const;
    void SetTrigger(TriggerClass* pTrigger);
    TriggerClass* GetTrigger() const;

    // ========================================================================
    // Scripted action helpers (asm 0x6E42C3..0x6E460E)
    //
    //  The original binary implements these as standalone ActionClass_*
    //  functions despatched from TActionClass::Execute with `this` already in
    //  ECX, so they behave as member functions.  Each returns true when it
    //  applied the action and false when a precondition (usually the target
    //  house or the waypoint) was not met.
    // ========================================================================
    bool SetTargetCell(TriggerClass* pTrigger);
    bool ClearTargetCell(TriggerClass* pTrigger);
    bool SetDefensiveCell(TriggerClass* pTrigger);
    bool ClearDefensiveCell(TriggerClass* pTrigger);
    bool SetBaseCenter(TriggerClass* pTrigger);
    bool ClearBaseCenter(TriggerClass* pTrigger);
    bool SetSWCharge(TriggerClass* pTrigger);
    bool SetSWRecharge(TriggerClass* pTrigger);
    bool ResetSWRecharge(TriggerClass* pTrigger);
    bool ResetSW(TriggerClass* pTrigger);
    HouseClass* FindHouseByIdx(TriggerClass* pTrigger, int32 idx);

    // ========================================================================
    // Special strike actions (asm 0x6E35F0 / 0x6E38C0 / 0x6E33A0)
    // ========================================================================
    bool FireChemLauncher(HouseClass* pOwner, int32 a3, TriggerClass* pTrigger, int32 a5);
    bool TriggerNukeStrike(HouseClass* pOwner, int32 a3, TriggerClass* pTrigger, int32 a5);

    // ========================================================================
    // ActionClass_* handlers (asm 0x6E1xxx..0x6E4xxx).  The binary implements
    // these as free functions called with `this` already loaded into ECX, so
    // they are modelled as members here.
    // ========================================================================
    bool ClearSmudges();
    bool RetintRed();
    bool RetintGreen();
    bool RetintBlue();
    bool RadarBlackout(HouseClass* pHouse);
    bool TeleportAllTo(HouseClass* pHouse);
    bool ReshroudMap();
    bool DropFlare(HouseClass* pHouse, TriggerClass* pTrigger);
    bool StopSoundsAt(HouseClass* pHouse, TriggerClass* pTrigger);
    bool WakeupAttachedObjects(HouseClass* pHouse, int32 a3,
                               TriggerClass* pTrigger, int32 a5);
    bool MindControlHouseBuildings(HouseClass* pNewOwner, TriggerClass* pTrigger);
    bool ReturnControlHouseBuildings(HouseClass* pOwner, TriggerClass* pTrigger);
    bool ResizePlayerView();
    bool EnableTrigger(HouseClass* pHouse, TriggerClass* pTrigger);
    bool FlashCameo(HouseClass* pHouse, TriggerClass* pTrigger);
    bool CreateBuilding(int32 a1, int32 a3, int32 a4, int32 a5);
    bool FlashBuildingsOfType(HouseClass* pHouse);

    // The recurring house-resolution helper every ActionClass handler uses:
    // index 0x2325 means "the house related to the calling trigger", -1 means
    // "no house", and anything else is looked up in the house array - through
    // the MP-aware variant for the seven special multiplayer country slots.
    static HouseClass* Resolve_Action_House(int32 idx, TriggerClass* pTrigger);

    bool AttachedTagSwitchHouse(HouseClass* pHouse, TriggerClass* pTrigger);
    bool FireIronCurtain(HouseClass* pHouse, TriggerClass* pTrigger);
    bool DestroyAllOf(HouseClass* pHouse, TriggerClass* pTrigger);
    bool DestroyAllBuildingsOf(HouseClass* pHouse, TriggerClass* pTrigger);
    bool DestroyAllLandUnitsOf(HouseClass* pHouse, TriggerClass* pTrigger);
    bool DestroyAllNavalOf(HouseClass* pHouse, TriggerClass* pTrigger);
    bool RestoreStartingTechnoOf(HouseClass* pHouse, TriggerClass* pTrigger);
    bool RestoreStartingBuildingsOf(HouseClass* pHouse, TriggerClass* pTrigger);
    bool SetTab(int32 tabIndex);
    bool SetHouseTargetCell(HouseClass* pHouse);
    bool ClearHouseTargetCell(HouseClass* pHouse);

private:
    void Action_WinGame();    void Action_LoseGame();
    void Action_Production();
    void Action_CreateTeam();
    void Action_ReinforceTeam();
    void Action_ChangeHouse();
    void Action_ChangeAI();
    void Action_PlayMovie();
    void Action_TextTrigger();
    void Action_DestroyTeam();
    void Action_DestroyAll();
    void Action_DestroyBuilding();
    void Action_DestroyUnit();
    void Action_DestroyInfantry();
    void Action_DestroyEntity();
    void Action_RevealMap();
    void Action_UnrevealMap();
    void Action_RevealWaypoint();
    void Action_RevealArea();
    void Action_PlaySound();
    void Action_PlayMusic();
    void Action_PlaySpeech();
    void Action_ForceFire();
    void Action_TimerStart();
    void Action_TimerStop();
    void Action_TimerSet();
    void Action_TimerAdd();
    void Action_TimerSubtract();
    void Action_TimerExpired();
    void Action_GlobalSet();
    void Action_GlobalClear();
    void Action_AutoBase();
    void Action_GrowShroud();
    void Action_DestroyAttached();
    void Action_FlashTeam();
    void Action_Reinforcement();
    void Action_Airstrike();
    void Action_SpySat();
    void Action_IonStorm();
    void Action_NukeStrike();
    void Action_LightningStrike();
    void Action_ChronoWarp();
    void Action_IronCurtain();
    void Action_ParaDrop();
    void Action_PsychicDominator();
    void Action_GeneticMutator();
    void Action_ForceShield();

public:
    char* ID;
    TAction ActionKind;
    int32 ActionIndex;
    int32 Waypoint;
    HouseClass* P1_House;
    TechnoTypeClass* P2_Object;
    int32 P3_Value;
    int32 P4_Value;
    int32 P5_Value;
    int32 P6_Value;
    int32 P7_Value;
    int32 P8_Value;
    TriggerClass* Trigger;
    bool IsGlobal;
};