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

// ============================================================================
// TriggerTypeClass - definition of a trigger (events + actions)
// Inherits AbstractTypeClass. A trigger type binds one or more events
// (TEventClass) to one or more actions (TActionClass) and is referenced by
// TagClass / TriggerClass instances at runtime. Parsed from the [Triggers]
// INI block of a scenario.
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
          NextTrigger(nullptr), IsEnabled(true), IsRepeatable(false),
          Easy(true), Normal(true), Hard(true), TriggerFlags(0) {}
    virtual ~TriggerTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual int32 GetCRC() const;

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

    TEventClass* FirstEvent;
    TActionClass* FirstAction;
    DynamicVectorClass<TEventClass*> EventList;
    DynamicVectorClass<TActionClass*> ActionList;
    TriggerTypeClass* NextTrigger;
    bool IsEnabled;
    bool IsRepeatable;
    bool Easy;
    bool Normal;
    bool Hard;
    int32 TriggerFlags;
};
