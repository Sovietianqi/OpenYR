#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"

enum class TriggerState {
    Armed = 0,
    Disabled = 1,
    Waiting = 2,
    Fired = 3
};

class TriggerClass {
public:
    static DynamicVectorClass<TriggerClass*>* Array;

    static TriggerClass* Find(const char* pID);
    static TriggerClass* FindOrAllocate(const char* pID);

    TriggerClass(const char* pID) noexcept;
    virtual ~TriggerClass();

    bool LoadFromINIList(CCINIClass* pINI);
    bool SaveToINIList(CCINIClass* pINI);

    void Update();
    bool CheckConditions();
    void Fire();
    void Enable();
    void Disable();
    void Reset();
    void Spring(TriggerEventType eventType, AbstractClass* pObject, CellStruct cell);
    void ForceFire();
    void SetEnabled(bool enabled);
    bool IsSatisfied();
    bool IsSatisfied(HouseClass* pHouse);
    void SetEvent(TEventClass* pEvent);
    void SetAction(TActionClass* pAction);
    void SetHouse(HouseClass* pHouse);
    void SetLinkedTrigger(TriggerClass* pTrigger);
    void SetData(int32 data);
    void SetTimer(int32 frames);

    static void ProcessTriggerEvents();
    static void ResetAllTriggers();
    static void FireAllTriggersForEvent(TriggerEventType eventType, AbstractClass* pObject, CellStruct cell);

public:
    char* ID;
    bool IsEnabled;
    TActionClass* TriggerAction;
    TEventClass* Event;
    TActionClass* CurrentAction;
    HouseClass* House;
    char* Name;
    int32 Data;
    bool HasBeenFired;
    bool JustFired;
    bool IsDisabled;
    bool IsBeingFired;
    bool IsLinked;
    bool Repeatable;
    bool Easy;
    bool Normal;
    bool Medium;
    bool Hard;
    TriggerClass* LinkedTrigger;
    TActionClass* LinkedAction;
    TActionClass* Action;
    bool forceFire;
    bool Activate;
    TriggerState State;
    int32 Timer;
};