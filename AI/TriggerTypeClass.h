#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Abstract/AbstractTypeClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class CCINIClass;
class TEventClass;
class TActionClass;
class TriggerClass;
class HouseTypeClass;

// ============================================================================
// TriggerTypeClass - definition of a trigger (events + actions)
// Inherits AbstractTypeClass. A trigger type binds one or more events
// (TEventClass) to one or more actions (TActionClass) and is referenced by
// TagClass / TriggerClass instances at runtime. Parsed from the [Triggers]
// INI block of a scenario.
//
// asm layout (sizeof = 0xB4):
//   +0x00  AbstractTypeClass
//   +0x98  idxInArray                 index cache in vec_TriggerTypes
//   +0x9C  Easy                       difficulty gate 1
//   +0x9D  Normal                     difficulty gate 2
//   +0x9E  Hard                       difficulty gate 3
//   +0x9F  IsDisabled                 stored inverted (0 => "1" written to INI)
//   +0xA0  IsRepeatable               repeat flag
//   +0xA4  Owner                      HouseTypeClass* (owner country)
//   +0xA8  NextTrigger            TriggerTypeClass* swizzle slot (next chain)
//   +0xAC  FirstEvent                 TEventClass* chain head
//   +0xB0  FirstAction                TActionClass* chain head
// ============================================================================
class NOVTABLE TriggerTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::TriggerType;

    static DynamicVectorClass<TriggerTypeClass*>* Array;

    static TriggerTypeClass* Find(const char* pID);
    static TriggerTypeClass* FindOrAllocate(const char* pID);
    static int32 GetCount();

    TriggerTypeClass(const char* pID) noexcept;
    explicit TriggerTypeClass(noinit_t) noexcept
        : AbstractTypeClass(noinit), FirstEvent(nullptr), FirstAction(nullptr),
          NextTrigger(nullptr), Owner(nullptr), idxInArray(-1),
          Easy(true), Normal(true), Hard(true), IsDisabled(false),
          IsRepeatable(false), IsEnabled(true) {}
    virtual ~TriggerTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual AbstractType GetAbstractDerivationID() const;   // asm: GetAbstractDerivationID
    virtual int32 Size() const override;                    // asm: GetSize
    virtual int32 GetArrayIndex() const override;           // asm: GetArrayIndex

    virtual bool LoadFromINI(CCINIClass* pINI) override;    // asm: TriggerTypeClass_LoadFromINI
    virtual bool SaveToINI(CCINIClass* pINI) override;      // asm: TriggerTypeClass_SaveToINI
    virtual int32 GetCRC() const;                           // asm: TriggerTypeClass_GetCRC

    // ------------------------------------------------------------------------
    // [Triggers] section bulk IO (asm: CreateFromINIList / SaveToINIList).
    // ------------------------------------------------------------------------
    // asm 里是 __fastcall 自由函数，与实例无关。
    static void CreateFromINIList(CCINIClass* pINI);
    void SaveToINIList(CCINIClass* pINI);

    // ------------------------------------------------------------------------
    // Chain queries. All walk a chain and test the element kind field
    // (asm +0x2C) against a fixed event/action id; they are boolean queries.
    // ------------------------------------------------------------------------
    bool CrossVerticalZone() const;               // event kind == 0x1A
    bool CrossHorizontalZone() const;             // event kind == 0x19
    bool IsWPZoneEntry() const;                   // event kind == 0x18
    bool InvolvesAllowWin() const;                // action kind == 0x0F
    bool InvolvesGlobalChecking(int32 idx = 0) const; // event kind in {0x1B,0x1C}, param == idx
    bool CheckLocals(int32 house) const;          // event kind in {0x24,0x25}, param == house

    // asm: TriggerClass_GetFlags (0x7271E0) — ORs the per-event and per-action
    // flag bits of this type's chains, then recurses into NextTrigger.
    int32 GetFlags() const;

    // asm: TriggerTypeClass_PointerGotInvalid —— 被销毁实例若正是 NextTrigger
    // 或事件/动作链头，则把对应槽前进一格，并把失效指针沿两条链下发给每个元素。
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed);

    // 沿类型表按标识符做大小写不敏感的线性比对，找不到则新建 TriggerClass 实例。
    static TriggerClass* FindInstance(TriggerTypeClass* pType);
    static TriggerTypeClass* FindInstance(const char* pID);

    void AttachEvent(TEventClass* pEvent);
    void AttachAction(TActionClass* pAction);

    // Task-required event/action management API
    int32 Get_Event_Count() const;
    int32 Get_Action_Count() const;
    TEventClass* Get_Event(int32 index) const;
    TActionClass* Get_Action(int32 index) const;
    void Add_Event(TEventClass* pEvent);
    void Add_Action(TActionClass* pAction);
    void Remove_Event(int32 index);
    void Remove_Action(int32 index);
    void Clear_Events();
    void Clear_Actions();
    bool Is_Enabled() const;
    void Set_Enabled(bool enabled);
    TriggerTypeClass* Get_Next_Trigger() const;
    void Set_Next_Trigger(TriggerTypeClass* pTrigger);
    bool Is_Repeating() const;
    void Set_Repeating(bool repeat);
    bool Is_Allowed_Difficulty(int32 difficulty) const;

    // ------------------------------------------------------------------------
    // asm semantic fields
    // ------------------------------------------------------------------------
    TEventClass* FirstEvent;          // +0xAC chain head
    TActionClass* FirstAction;        // +0xB0 chain head
    DynamicVectorClass<TEventClass*> EventList;
    DynamicVectorClass<TActionClass*> ActionList;
    // +0xA8 swizzle slot: the next chained trigger type (asm "NextTrigger").
    // Named NextTrigger to match the pre-existing cross-class API (TagTypeClass).
    TriggerTypeClass* NextTrigger;
    HouseTypeClass* Owner;             // +0xA4 owner country
    int32 idxInArray;                  // +0x98
    bool Easy;                         // +0x9C
    bool Normal;                       // +0x9D
    bool Hard;                         // +0x9E
    bool IsDisabled;                   // +0x9F
    bool IsRepeatable;                 // +0xA0
    bool IsEnabled;                    // mirror of !IsDisabled (kept for legacy API)
};
