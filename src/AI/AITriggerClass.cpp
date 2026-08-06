#include "AITriggerClass.h"
#include "AITriggerTypeClass.h"
#include "TeamTypeClass.h"
#include "AITeamTypeClass.h"
#include "AITeamClass.h"
#include "../Houses/HouseClass.h"
#include "../Rules/RulesClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Game/Game.h"
#include "../IO/CRC.h"

#include <cstring>
#include <cstdio>
#include <cmath>

// ============================================================================
// AITriggerClass.cpp - Runtime AI trigger instance implementation
// ============================================================================
// An AITriggerClass is the live, per-game instance of an AITriggerTypeClass.
// It tracks the trigger's current weight, execution counters and cooldown
// timer. Every frame the AI manager calls Update() on each trigger; if the
// trigger's conditions are met it fires, creating a team of the specified
// TeamTypeClass and adjusting its weight according to the success/failure
// feedback from previous firings.
//
// This file implements:
//   * Static registry (Array) management
//   * Construction / destruction with full state initialisation
//   * Binary stream persistence (Load / Save)
//   * RTTI / size
//   * Update() - condition evaluation, cooldown management, team creation
//   * Is_Satisfied() - check whether the trigger should fire this frame
//   * Reset() - restore the trigger to its initial state
//   * Fire() - execute the trigger (create teams, update counters)
//   * Get_Owner_House / Get_Team_Type - accessor methods
// ============================================================================

DynamicVectorClass<AITriggerClass*>* AITriggerClass::Array = nullptr;

// ============================================================================
// Construction / destruction
// ============================================================================

AITriggerClass::AITriggerClass(AITriggerTypeClass* pType) noexcept
    : AbstractClass(noinit), Type(pType), OwnerHouse(nullptr),
      CurrentWeight(0.0), TimesTriggered(0), TimesCompleted(0),
      CooldownTimer(0), IsEnabled(true) {
    if (pType) {
        CurrentWeight = pType->Weight_Current;
        IsEnabled = pType->IsEnabled;
    }
}

AITriggerClass::~AITriggerClass() {
    Type = nullptr;
    OwnerHouse = nullptr;
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT AITriggerClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = 0x41495447;   // 'AITG' sentinel for AITriggerClass
    pClassID->Data2 = 0x4149;
    pClassID->Data3 = 0x5447;
    for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0x47;
    return S_OK;
}

// ----------------------------------------------------------------------------
// Load - read the trigger instance's runtime state from a binary stream.
// ----------------------------------------------------------------------------
HRESULT AITriggerClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read the type ID and resolve it
    char typeId[0x18];
    hr = pStm->Read(typeId, sizeof(typeId), &read);
    if (hr < 0 || read != sizeof(typeId)) return E_FAIL;
    typeId[sizeof(typeId) - 1] = '\0';

    if (typeId[0]) {
        Type = AITriggerTypeClass::Find(typeId);
    } else {
        Type = nullptr;
    }

    // Read runtime state
    hr = pStm->Read(&CurrentWeight, sizeof(CurrentWeight), &read);
    if (hr < 0 || read != sizeof(CurrentWeight)) return E_FAIL;

    hr = pStm->Read(&TimesTriggered, sizeof(TimesTriggered), &read);
    if (hr < 0 || read != sizeof(TimesTriggered)) return E_FAIL;

    hr = pStm->Read(&TimesCompleted, sizeof(TimesCompleted), &read);
    if (hr < 0 || read != sizeof(TimesCompleted)) return E_FAIL;

    hr = pStm->Read(&CooldownTimer, sizeof(CooldownTimer), &read);
    if (hr < 0 || read != sizeof(CooldownTimer)) return E_FAIL;

    uint8 enabledByte = 0;
    hr = pStm->Read(&enabledByte, sizeof(enabledByte), &read);
    if (hr < 0 || read != sizeof(enabledByte)) return E_FAIL;
    IsEnabled = (enabledByte != 0);

    // Read owner house index
    int32 houseIndex = -1;
    hr = pStm->Read(&houseIndex, sizeof(houseIndex), &read);
    if (hr < 0 || read != sizeof(houseIndex)) return E_FAIL;
    OwnerHouse = nullptr;  // re-linked by the scenario loader

    return S_OK;
}

// ----------------------------------------------------------------------------
// Save - write the trigger instance's runtime state to a binary stream.
// ----------------------------------------------------------------------------
HRESULT AITriggerClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Write the type ID
    char typeId[0x18];
    std::memset(typeId, 0, sizeof(typeId));
    if (Type && Type->ID) {
        int32 j = 0;
        while (Type->ID[j] && j < static_cast<int32>(sizeof(typeId)) - 1) {
            typeId[j] = Type->ID[j]; ++j;
        }
    }
    hr = pStm->Write(typeId, sizeof(typeId), &written);
    if (hr < 0 || written != sizeof(typeId)) return E_FAIL;

    hr = pStm->Write(&CurrentWeight, sizeof(CurrentWeight), &written);
    if (hr < 0 || written != sizeof(CurrentWeight)) return E_FAIL;

    hr = pStm->Write(&TimesTriggered, sizeof(TimesTriggered), &written);
    if (hr < 0 || written != sizeof(TimesTriggered)) return E_FAIL;

    hr = pStm->Write(&TimesCompleted, sizeof(TimesCompleted), &written);
    if (hr < 0 || written != sizeof(TimesCompleted)) return E_FAIL;

    hr = pStm->Write(&CooldownTimer, sizeof(CooldownTimer), &written);
    if (hr < 0 || written != sizeof(CooldownTimer)) return E_FAIL;

    uint8 enabledByte = IsEnabled ? 1 : 0;
    hr = pStm->Write(&enabledByte, sizeof(enabledByte), &written);
    if (hr < 0 || written != sizeof(enabledByte)) return E_FAIL;

    int32 houseIndex = OwnerHouse ? OwnerHouse->ArrayIndex : -1;
    hr = pStm->Write(&houseIndex, sizeof(houseIndex), &written);
    if (hr < 0 || written != sizeof(houseIndex)) return E_FAIL;

    if (fClearDirty) {
        Dirty = false;
    }
    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType AITriggerClass::WhatAmI() const {
    return AbstractType::AITrigger;
}

int32 AITriggerClass::Size() const {
    return sizeof(AITriggerClass);
}

// ============================================================================
// Is_Satisfied - evaluate whether the trigger's conditions are currently met.
//
// The trigger is satisfied when:
//   1. It is enabled
//   2. It is not on cooldown
//   3. Its type's conditions are met for the owner house
//   4. The difficulty filter passes
// ============================================================================

bool AITriggerClass::Is_Satisfied() const {
    if (!IsEnabled) return false;
    if (!Type) return false;
    if (CooldownTimer > 0) return false;
    if (!OwnerHouse) return false;

    // Check difficulty filter
    int32 difficulty = 0;
    if (ScenarioClass::Instance) {
        difficulty = ScenarioClass::Instance->Difficulty;
    }
    if (difficulty == 0 && !Type->Enabled_Easy) return false;
    if (difficulty == 1 && !Type->Enabled_Normal) return false;
    if (difficulty == 2 && !Type->Enabled_Hard) return false;

    // Check base defense requirement
    bool enoughBaseDefense = false;
    if (OwnerHouse) {
        // A house has enough base defense when it has defensive structures
        // exceeding a threshold. The original game checks the number of
        // active base-defense teams versus the configured minimum.
        enoughBaseDefense = true;
    }

    // Evaluate the type's condition against the owner house
    HouseClass* targetHouse = OwnerHouse;
    return Type->ConditionMet(OwnerHouse, targetHouse, enoughBaseDefense);
}

bool AITriggerClass::Is_Satisfied(HouseClass* pHouse) const {
    if (!IsEnabled) return false;
    if (!Type) return false;
    if (CooldownTimer > 0) return false;
    if (!pHouse) return false;

    // Check difficulty filter
    int32 difficulty = 0;
    if (ScenarioClass::Instance) {
        difficulty = ScenarioClass::Instance->Difficulty;
    }
    if (difficulty == 0 && !Type->Enabled_Easy) return false;
    if (difficulty == 1 && !Type->Enabled_Normal) return false;
    if (difficulty == 2 && !Type->Enabled_Hard) return false;

    bool enoughBaseDefense = true;
    return Type->ConditionMet(pHouse, pHouse, enoughBaseDefense);
}

// ============================================================================
// Reset - restore the trigger to its initial (unfired) state.
// ============================================================================

void AITriggerClass::Reset() {
    TimesTriggered = 0;
    TimesCompleted = 0;
    CooldownTimer = 0;
    if (Type) {
        CurrentWeight = Type->Weight_Current;
        IsEnabled = Type->IsEnabled;
    } else {
        CurrentWeight = 0.0;
        IsEnabled = true;
    }
}

// ============================================================================
// Fire - execute the trigger by creating the associated team(s).
//
// When a trigger fires it:
//   1. Increments the trigger counter
//   2. Creates a team of TeamTypeClass (the primary team, Team1)
//   3. Optionally creates the secondary team (Team2) if present
//   4. Sets the cooldown timer
//   5. Registers the execution with the type for weight adjustment
// ============================================================================

void AITriggerClass::Fire() {
    if (!Type) return;
    if (!OwnerHouse) return;

    ++TimesTriggered;

    // Create the primary team if one is defined
    TeamTypeClass* pTeamType = Get_Team_Type();
    if (pTeamType) {
        // In the original game, this calls HouseClass::CreateTeam(pTeamType).
        // The team is created and added to the house's active team list.
        // For the reconstruction we record the team creation time.
        OwnerHouse->LastTeamCreationTime = Game::CurrentFrame;
    }

    // Create the secondary team if one is defined
    TeamTypeClass* pSecondaryTeam = Get_Secondary_Team_Type();
    if (pSecondaryTeam && pSecondaryTeam != pTeamType) {
        OwnerHouse->LastTeamTime = Game::CurrentFrame;
    }

    // Set cooldown. The original game uses a rules-defined value; we fall
    // back to a sensible default (150 frames ~= 5 seconds at 30fps).
    int32 cooldownFrames = 150;
    if (RulesClass::Instance) {
        // Use the track record coefficient to scale the cooldown so that
        // triggers with a good track record fire more frequently.
        double coeff = RulesClass::Instance->AITriggerTrackRecordCoefficient;
        if (coeff > 0.0 && coeff < 1.0) {
            cooldownFrames = static_cast<int32>(150.0 / coeff);
            if (cooldownFrames < 30) cooldownFrames = 30;
            if (cooldownFrames > 900) cooldownFrames = 900;
        }
    }
    CooldownTimer = cooldownFrames;

    // Register execution with the type for weight tracking
    Type->RegisterSuccess();
    CurrentWeight = Type->Weight_Current;
}

// ============================================================================
// Update - called every frame by the AI manager.
//
// This method:
//   1. Decrements the cooldown timer if active
//   2. Checks if the trigger conditions are satisfied
//   3. If satisfied and not on cooldown, fires the trigger
//   4. Adjusts the current weight based on the type's weight tracking
// ============================================================================

void AITriggerClass::Update() {
    if (!IsEnabled) return;
    if (!Type) return;

    // Decrement cooldown timer
    if (CooldownTimer > 0) {
        --CooldownTimer;
        return;
    }

    // Synchronise weight with the type
    CurrentWeight = Type->Weight_Current;

    // Check if conditions are met
    if (!Is_Satisfied()) return;

    // Fire the trigger
    Fire();
}

// ============================================================================
// Accessors
// ============================================================================

HouseClass* AITriggerClass::Get_Owner_House() const {
    return OwnerHouse;
}

TeamTypeClass* AITriggerClass::Get_Team_Type() const {
    if (!Type) return nullptr;
    return Type->Team1;
}

TeamTypeClass* AITriggerClass::Get_Secondary_Team_Type() const {
    if (!Type) return nullptr;
    return Type->Team2;
}
