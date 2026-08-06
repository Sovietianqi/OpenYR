#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Abstract/AbstractClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class AITriggerTypeClass;
class HouseClass;
class TeamTypeClass;

// ============================================================================
// AITriggerClass - runtime instance of an AI trigger
// Wraps an AITriggerTypeClass and tracks its live state (current weight,
// execution counters) during a running game. Inherits AbstractClass so it can
// participate in the global persistence (Load/Save) and RTTI machinery.
// ============================================================================
class NOVTABLE AITriggerClass : public AbstractClass {
public:
    static const AbstractType AbsID = AbstractType::AITrigger;

    static DynamicVectorClass<AITriggerClass*>* Array;

    AITriggerClass(AITriggerTypeClass* pType = nullptr) noexcept;
    explicit AITriggerClass(noinit_t) noexcept : AbstractClass(noinit) {}
    virtual ~AITriggerClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    virtual void Update();

    // Task-required runtime API
    bool Is_Satisfied() const;
    bool Is_Satisfied(HouseClass* pHouse) const;
    void Reset();
    HouseClass* Get_Owner_House() const;
    TeamTypeClass* Get_Team_Type() const;
    TeamTypeClass* Get_Secondary_Team_Type() const;
    bool Is_Enabled() const { return IsEnabled; }
    void Set_Enabled(bool enabled) { IsEnabled = enabled; }
    AITriggerTypeClass* Get_Type() const { return Type; }
    double Get_Current_Weight() const { return CurrentWeight; }
    int32 Get_Times_Triggered() const { return TimesTriggered; }
    int32 Get_Times_Completed() const { return TimesCompleted; }
    void Set_Cooldown(int32 frames) { CooldownTimer = frames; }
    bool Is_On_Cooldown() const { return CooldownTimer > 0; }
    void Fire();

    AITriggerTypeClass* Type;
    HouseClass* OwnerHouse;
    double CurrentWeight;
    int32 TimesTriggered;
    int32 TimesCompleted;
    int32 CooldownTimer;
    bool IsEnabled;
};
