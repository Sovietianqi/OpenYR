#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"

class TriggerTypeClass;
class CRCEngine;

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
    TriggerClass(TriggerTypeClass* pType) noexcept;
    virtual ~TriggerClass();

    // ------------------------------------------------------------------------
    // asm vtable slots (TriggerClass::`vftable'{for `TriggerClass'}):
    //   GetClassID / Load / Save / GetCRC / GetSize / GetAbstractDerivationID /
    //   PointerGotInvalid / Poof / sub_726780 (Destroyed flag).
    // ------------------------------------------------------------------------
    HRESULT GetClassID(CLSID* pClassID);
    HRESULT Load(IStream* pStm);
    HRESULT Save(IStream* pStm, BOOL fClearDirty);
    virtual int32 GetCRC() const;                       // asm: TriggerClass_GetCRC (0x726740)
    int32 Size() const;                                 // asm: TriggerClass_GetSize (0x7268D0) = 0x48
    int32 GetAbstractDerivationID() const;              // asm: TriggerClass_GetAbstractDerivationID = 0x26
    bool IsDestroyed() const;                           // asm: sub_726780

    // 统一签名（见简报第三节）：被销毁对象若是本触发器的类型/后继/关联房，
    // 则清除或前移对应槽位。
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed);

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
    // asm +0x24: the type (template) this runtime trigger was instantiated from.
    // Events / actions / owner house are read back through this pointer.
    TriggerTypeClass* Type;
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
    // asm +0x30: "Destroyed" flag — set by Poof(), cleared by Reset().
    bool Destroyed;
    // asm +0x40: per-event "already fired" bitmask (bit i = event i fired).
    int32 EventsAlreadyFired;
    int32 Timer;
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知触发器层补全全局/本地变量条件复检、跨越检查线
    // 判定、动作链执行、标志打包与“原地消失”摘除等入口。
    // ------------------------------------------------------------------------
    virtual bool CheckGlobals(int32 idx);
    virtual bool CheckLocals(HouseClass* pHouse);
    virtual bool CrossHorizontalZone() const;
    virtual bool CrossVerticalZone() const;
    virtual bool IsWPZoneEntry() const;
    // Legacy zone-parameterised overloads kept for TagClass callers.
    bool CrossHorizontalZone(int32 zone) const;
    bool CrossVerticalZone(int32 zone) const;
    bool CrossedVertical(int32 zone) const;
    virtual bool CrossedHorizontal() const;
    virtual bool CrossedVertical() const;
    virtual bool FireActions(AbstractClass* pObject, CellStruct pos);
    virtual void GlobalUpdated(int32 idx);
    virtual void LocalUpdated(int32 idx);
    // asm: TriggerClass_HaveAllEventsOccured (asm:1645919, retn 14h)
    //   assumeSatisfied ← asm a4（RaiseEvent 的 isRepeating 实参）：非零则跳过
    //     逐事件判定，直接视作"全部事件已发生"。
    //   repeatingType   ← asm isRepeating：触发器类型可重复时，全部事件命中后 Reset。
    virtual bool HaveAllEventsOccured(int32 eventKind, AbstractClass* pObject,
                                      bool assumeSatisfied, bool repeatingType);
    virtual bool InvolvesAllowWin() const;
    virtual void Poof();

};