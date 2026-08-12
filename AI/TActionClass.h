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

private:
    void Action_WinGame();
    void Action_LoseGame();
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