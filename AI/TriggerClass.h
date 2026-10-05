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

    // TriggerClass_FindRelatedHouse (asm 0x726910): returns the house stored
    // at +0x2C, which is the "related house" the trigger was bound to.
    HouseClass* FindRelatedHouse() const;
    void SetRelatedHouse(HouseClass* pHouse);

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
    // The "related house" slot at +0x2C that FindRelatedHouse hands back.
    HouseClass* RelatedHouse;
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
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知触发器层补全全局/本地变量条件复检、跨越检查线
    // 判定、动作链执行、标志打包与“原地消失”摘除等入口。
    // ------------------------------------------------------------------------
    virtual bool CheckGlobals();
    virtual bool CheckLocals();
    virtual bool CrossHorizontalZone(int32 zone);
    virtual bool CrossedHorizontal(int32 y);
    virtual bool CrossedVertical(int32 x);
    virtual void FireActions();
    virtual int32 GetFlags() const;
    virtual void GlobalUpdated(int32 id, int32 value);
    virtual void LocalUpdated(int32 id, int32 value);
    virtual bool HaveAllEventsOccured();
    virtual bool InvolvesAllowWin() const;
    virtual void Poof();

};