#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"

enum class TEventKind {
    None = 0,
    EnteredBy = 1,
    SpiedBy = 2,
    ThievedBy = 3,
    DiscoveredBy = 4,
    AttackedBy = 5,
    DestroyedBy = 6,
    AnyEvent = 7,
    HouseDiscovered = 8,
    TimeElapsed = 9,
    MissionTimerExpired = 10,
    BuildingExists = 11,
    BuildingDestroyed = 12,
    UnitExists = 13,
    UnitDestroyed = 14,
    InfantryExists = 15,
    InfantryDestroyed = 16,
    AircraftExists = 17,
    AircraftDestroyed = 18,
    Credits = 19,
    ElapsedTime = 20,
    LowPower = 21,
    BridgeDestroyed = 22,
    BuildingCaptured = 23,
    SuperWeaponAvailable = 24,
    LocalSet = 25,
    LocalClear = 26,
    GlobalSet = 27,
    GlobalClear = 28,
    Always = 29
};

class TEventClass {
public:
    static DynamicVectorClass<TEventClass*>* Array;

    static TEventClass* Find(const char* pID);
    static TEventClass* FindOrAllocate(const char* pID);

    TEventClass(const char* pID) noexcept;
    virtual ~TEventClass();

    bool LoadFromINIList(CCINIClass* pINI);
    bool SaveToINIList(CCINIClass* pINI);

    bool IsSatisfied(TriggerClass* pTrigger) const;
    bool MatchEvent(TriggerEventType eventType, AbstractClass* pObject, CellStruct cell, TriggerClass* pTrigger) const;
    void GetEventName(char* buffer, int32 bufferSize) const;

private:
    bool CheckEnteredBy(TriggerClass* pTrigger) const;
    bool CheckSpiedBy(TriggerClass* pTrigger) const;
    bool CheckThievedBy(TriggerClass* pTrigger) const;
    bool CheckDiscoveredBy(TriggerClass* pTrigger) const;
    bool CheckAttackedBy(TriggerClass* pTrigger) const;
    bool CheckDestroyedBy(TriggerClass* pTrigger) const;
    bool CheckAnyEvent(TriggerClass* pTrigger) const;
    bool CheckHouseDiscovered(TriggerClass* pTrigger) const;
    bool CheckTimeElapsed(TriggerClass* pTrigger) const;
    bool CheckMissionTimerExpired(TriggerClass* pTrigger) const;
    bool CheckBuildingExists(TriggerClass* pTrigger) const;
    bool CheckBuildingDestroyed(TriggerClass* pTrigger) const;
    bool CheckUnitExists(TriggerClass* pTrigger) const;
    bool CheckUnitDestroyed(TriggerClass* pTrigger) const;
    bool CheckInfantryExists(TriggerClass* pTrigger) const;
    bool CheckInfantryDestroyed(TriggerClass* pTrigger) const;
    bool CheckAircraftExists(TriggerClass* pTrigger) const;
    bool CheckAircraftDestroyed(TriggerClass* pTrigger) const;
    bool CheckCredits(TriggerClass* pTrigger) const;
    bool CheckElapsedTime(TriggerClass* pTrigger) const;
    bool CheckLowPower(TriggerClass* pTrigger) const;
    bool CheckBridgeDestroyed(TriggerClass* pTrigger) const;
    bool CheckBuildingCaptured(TriggerClass* pTrigger) const;
    bool CheckSuperWeaponAvailable(TriggerClass* pTrigger) const;
    bool CheckLocalSet(TriggerClass* pTrigger) const;
    bool CheckLocalClear(TriggerClass* pTrigger) const;
    bool CheckGlobalSet(TriggerClass* pTrigger) const;
    bool CheckGlobalClear(TriggerClass* pTrigger) const;

public:
    char* ID;
    TEventKind EventKind;
    int32 EventIndex;
    int32 Data;
    HouseClass* P1_House;
    TechnoTypeClass* P2_Object;
    int32 P3_Value;
    int32 P4_Value;
    int32 P5_Value;
    bool IsGlobal;
};