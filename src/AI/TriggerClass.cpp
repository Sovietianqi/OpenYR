// =============================================================================
// TriggerClass - scenario trigger instance implementation
//
// A TriggerClass binds a TEventClass (the condition) to a TActionClass (the
// consequence).  Triggers are attached to map objects via TagClass and are
// evaluated every frame by ProcessTriggerEvents().  When an event's condition
// becomes satisfied the trigger fires, executing its action and optionally
// cascading to a linked trigger.
//
// State machine:
//   Armed    -> actively checking conditions each frame.
//   Waiting  -> paused for Timer frames before re-arming (delayed re-trigger).
//   Disabled -> explicitly turned off; no evaluation occurs.
//   Fired    -> has fired at least once; non-repeatable triggers stay idle.
//
// Difficulty filtering:
//   Each trigger carries Easy/Normal/Medium/Hard flags.  Before firing we
//   compare against ScenarioClass::Instance->Difficulty so a trigger only
//   activates on the difficulties it was authored for.
// =============================================================================

#include "TriggerClass.h"
#include "TActionClass.h"
#include "TEventClass.h"
#include "TagClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Rules/RulesClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Game/Game.h"
#include "../Map/MapClass.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdio>
#include <ctime>

// -----------------------------------------------------------------------------
// Static array pointer - the global list of every loaded TriggerClass.
// -----------------------------------------------------------------------------
DynamicVectorClass<TriggerClass*>* TriggerClass::Array = nullptr;

// =============================================================================
// Local constants - tuning and safety limits.
// =============================================================================
namespace {
    // Maximum depth of linked-trigger cascades to prevent infinite loops.
    constexpr int32 MAX_LINK_DEPTH = 16;

    // Scenario difficulty constants (mirrors ScenarioClass::Difficulty values).
    constexpr int32 DIFFICULTY_EASY   = 0;
    constexpr int32 DIFFICULTY_NORMAL = 1;
    constexpr int32 DIFFICULTY_HARD   = 2;

    // -------------------------------------------------------------------------
    // DifficultyEnabled - returns true if the trigger should activate given the
    // current scenario difficulty.  If no difficulty flags are set the trigger
    // is treated as active on all difficulties.
    // -------------------------------------------------------------------------
    bool DifficultyEnabled(const TriggerClass* pTrigger) {
        if (!pTrigger) return false;
        if (!ScenarioClass::Instance) return true;

        int32 diff = ScenarioClass::Instance->Difficulty;
        switch (diff) {
            case DIFFICULTY_EASY:   return pTrigger->Easy;
            case DIFFICULTY_NORMAL: return pTrigger->Normal || pTrigger->Medium;
            case DIFFICULTY_HARD:   return pTrigger->Hard;
            default:                return true;
        }
    }

    // -------------------------------------------------------------------------
    // HouseIsActive - returns true if the house pointer is valid and the house
    // has not been defeated.  A null house is considered active (global trigger).
    // -------------------------------------------------------------------------
    bool HouseIsActive(HouseClass* pHouse) {
        if (!pHouse) return true;
        return !pHouse->IsDefeated;
    }

    // Forward declaration - defined after Fire() below.
    void FireLinkedChain(TriggerClass* pTrigger, int32 depth);
} // anonymous namespace

// =============================================================================
// Constructor
// =============================================================================
TriggerClass::TriggerClass(const char* pID) noexcept
    : ID(nullptr), IsEnabled(true), TriggerAction(nullptr), Event(nullptr),
      CurrentAction(nullptr), House(nullptr), Name(nullptr),
      Data(-1), HasBeenFired(false), JustFired(false),
      IsDisabled(false), IsBeingFired(false), IsLinked(false),
      Repeatable(false), Easy(false), Normal(false), Medium(false),
      Hard(false), LinkedTrigger(nullptr), LinkedAction(nullptr),
      Action(nullptr), forceFire(false), Activate(false),
      State(TriggerState::Armed), Timer(0) {
    if (pID) {
        int32 len = static_cast<int32>(strlen(pID)) + 1;
        ID = new char[len];
        if (ID) {
            for (int32 i = 0; i < len; ++i) ID[i] = pID[i];
        }
    }
}

// =============================================================================
// Destructor - release owned strings and event/action objects.
// =============================================================================
TriggerClass::~TriggerClass() {
    if (ID) { delete[] ID; ID = nullptr; }
    if (Name) { delete[] Name; Name = nullptr; }
    if (TriggerAction) { GameDelete(TriggerAction); TriggerAction = nullptr; }
    if (Event) { GameDelete(Event); Event = nullptr; }
    if (CurrentAction) { GameDelete(CurrentAction); CurrentAction = nullptr; }
}

// =============================================================================
// Find - locate a trigger by its ID string (case-insensitive).
// =============================================================================
TriggerClass* TriggerClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        TriggerClass* item = Array->GetItem(i);
        if (item && item->ID && !_strcmpi(item->ID, pID)) return item;
    }
    return nullptr;
}

// =============================================================================
// FindOrAllocate - find an existing trigger or create a new one.  Returns
// nullptr for the sentinel "<none>" / "none" IDs used in INI files.
// =============================================================================
TriggerClass* TriggerClass::FindOrAllocate(const char* pID) {
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    TriggerClass* found = Find(pID);
    if (found) return found;
    TriggerClass* newItem = GameCreate<TriggerClass>(pID);
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}

// =============================================================================
// LoadFromINIList - parse the trigger's configuration from its INI section.
//
// The section name is the trigger's ID.  Recognised keys:
//   Name, Repeatable, Easy, Normal, Medium, Hard, Event, Action, House,
//   LinkedTrigger, Data, Timer, Owner, Disabled
// =============================================================================
bool TriggerClass::LoadFromINIList(CCINIClass* pINI) {
    if (!pINI) return false;
    if (!ID) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", ID);
    if (!pINI->SectionExists(sectionName)) return false;

    char buffer[256];

    // --- Name ---
    pINI->ReadString(sectionName, "Name", "", buffer, sizeof(buffer));
    if (buffer[0]) {
        if (Name) { delete[] Name; Name = nullptr; }
        int32 len = static_cast<int32>(strlen(buffer)) + 1;
        Name = new char[len];
        if (Name) {
            for (int32 i = 0; i < len; ++i) Name[i] = buffer[i];
        }
    }

    // --- Difficulty flags ---
    Repeatable = pINI->ReadBool(sectionName, "Repeatable", Repeatable);
    Easy   = pINI->ReadBool(sectionName, "Easy", Easy);
    Normal = pINI->ReadBool(sectionName, "Normal", Normal);
    Medium = pINI->ReadBool(sectionName, "Medium", Medium);
    Hard   = pINI->ReadBool(sectionName, "Hard", Hard);

    // If no difficulty flags are set, default to all difficulties enabled so
    // the trigger is not silently inert.
    if (!Easy && !Normal && !Medium && !Hard) {
        Easy = Normal = Medium = Hard = true;
    }

    // --- Event reference ---
    char eventId[32];
    pINI->ReadString(sectionName, "Event", "", eventId, sizeof(eventId));
    if (eventId[0] && _strcmpi(eventId, "<none>") != 0) {
        Event = TEventClass::FindOrAllocate(eventId);
    }

    // --- Action reference ---
    char actionId[32];
    pINI->ReadString(sectionName, "Action", "", actionId, sizeof(actionId));
    if (actionId[0] && _strcmpi(actionId, "<none>") != 0) {
        TriggerAction = TActionClass::FindOrAllocate(actionId);
    }

    // --- House ownership ---
    char houseId[32];
    pINI->ReadString(sectionName, "House", "", houseId, sizeof(houseId));
    if (houseId[0] && _strcmpi(houseId, "<none>") != 0) {
        for (int32 i = 0; i < 32; ++i) {
            if (HouseClass::Array[i]) {
                const char* pHouseName = HouseClass::Array[i]->Type->get_ID();
                if (pHouseName && !_strcmpi(pHouseName, houseId)) {
                    House = HouseClass::Array[i];
                    break;
                }
            }
        }
    }

    // --- Linked trigger ---
    char linkedId[32];
    pINI->ReadString(sectionName, "LinkedTrigger", "", linkedId, sizeof(linkedId));
    if (linkedId[0] && _strcmpi(linkedId, "<none>") != 0) {
        LinkedTrigger = TriggerClass::FindOrAllocate(linkedId);
        if (LinkedTrigger) IsLinked = true;
    }

    // --- Numeric data ---
    pINI->GetInteger(sectionName, "Data", Data);

    // --- Optional initial timer (delay before first evaluation) ---
    int32 iniTimer = 0;
    pINI->GetInteger(sectionName, "Timer", iniTimer);
    if (iniTimer > 0) {
        Timer = iniTimer;
        State = TriggerState::Waiting;
    }

    // --- Optional Disabled flag (start disabled) ---
    bool startDisabled = pINI->ReadBool(sectionName, "Disabled", false);
    if (startDisabled) {
        Disable();
    }

    return true;
}

// =============================================================================
// SaveToINIList - serialise the trigger configuration back to INI.
// =============================================================================
bool TriggerClass::SaveToINIList(CCINIClass* pINI) {
    if (!pINI) return false;
    if (!ID) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", ID);

    pINI->WriteString(sectionName, "Name", Name ? Name : "");
    pINI->WriteBool(sectionName, "Repeatable", Repeatable);
    pINI->WriteBool(sectionName, "Easy", Easy);
    pINI->WriteBool(sectionName, "Normal", Normal);
    pINI->WriteBool(sectionName, "Medium", Medium);
    pINI->WriteBool(sectionName, "Hard", Hard);

    if (Event) pINI->WriteString(sectionName, "Event", Event->ID);
    else pINI->WriteString(sectionName, "Event", "<none>");

    if (TriggerAction) pINI->WriteString(sectionName, "Action", TriggerAction->ID);
    else pINI->WriteString(sectionName, "Action", "<none>");

    if (House) pINI->WriteString(sectionName, "House", House->Type->get_ID());
    else pINI->WriteString(sectionName, "House", "<none>");

    if (LinkedTrigger) pINI->WriteString(sectionName, "LinkedTrigger", LinkedTrigger->ID);
    else pINI->WriteString(sectionName, "LinkedTrigger", "<none>");

    pINI->WriteInteger(sectionName, "Data", Data);

    if (Timer > 0) {
        pINI->WriteInteger(sectionName, "Timer", Timer);
    }

    return true;
}

// =============================================================================
// Update - per-frame trigger evaluation.
//
// Steps:
//   1. Clear the JustFired one-shot flag from the previous frame.
//   2. Bail out if disabled, not enabled, or already fired (non-repeatable).
//   3. Count down any active Waiting timer; transition to Armed when it hits 0.
//   4. Only evaluate conditions when in the Armed state.
//   5. If conditions are met, fire.
// =============================================================================
void TriggerClass::Update() {
    // Clear the per-frame "just fired" pulse.
    if (JustFired) {
        JustFired = false;
    }

    if (IsDisabled) return;
    if (!IsEnabled) return;
    if (HasBeenFired && !Repeatable) return;

    // Difficulty gate - skip evaluation entirely on non-matching difficulties.
    if (!DifficultyEnabled(this)) return;

    // House gate - skip if the owning house has been defeated.
    if (!HouseIsActive(House)) return;

    // Handle the Waiting (delayed) state.
    if (State == TriggerState::Waiting) {
        if (Timer > 0) {
            --Timer;
            if (Timer > 0) return;
        }
        // Timer elapsed - re-arm.
        State = TriggerState::Armed;
    }

    if (State != TriggerState::Armed) return;

    if (CheckConditions()) {
        Fire();
    }
}

// =============================================================================
// CheckConditions - evaluate the trigger's event condition plus the force-fire
// and one-shot activate flags.
// =============================================================================
bool TriggerClass::CheckConditions() {
    // Primary: the attached event reports satisfaction.
    if (Event && Event->IsSatisfied(this)) {
        return true;
    }

    // Forced fire (set via ForceFire or SetEnabled with a force flag).
    if (forceFire) {
        forceFire = false;
        return true;
    }

    // One-shot activation flag used by external systems.
    if (Activate && !HasBeenFired) {
        Activate = false;
        return true;
    }

    return false;
}

// =============================================================================
// Fire - execute the trigger's action and cascade to linked triggers.
//
// The linked-trigger cascade is depth-limited to MAX_LINK_DEPTH to prevent
// infinite loops in cyclic trigger graphs.
// =============================================================================
void TriggerClass::Fire() {
    if (HasBeenFired && !Repeatable) return;

    IsBeingFired = true;

    // Execute the primary action.
    if (TriggerAction) {
        TriggerAction->ExecuteAction(this);
    }

    HasBeenFired = true;
    JustFired = true;
    IsBeingFired = false;

    // Cascade to the linked trigger (if any) with depth limiting.
    if (LinkedTrigger && IsLinked) {
        FireLinkedChain(LinkedTrigger, 1);
    }

    // Non-repeatable triggers disable themselves after firing.
    if (!Repeatable) {
        IsEnabled = false;
        State = TriggerState::Fired;
    } else {
        // Repeatable triggers re-arm for the next occurrence.
        State = TriggerState::Armed;
    }
}

// =============================================================================
// FireLinkedChain - recursively fire linked triggers up to a maximum depth.
// This is a file-local helper used by Fire().
// =============================================================================
namespace {
    void FireLinkedChain(TriggerClass* pTrigger, int32 depth) {
        if (!pTrigger) return;
        if (depth > MAX_LINK_DEPTH) return;

        // Respect the linked trigger's own repeatable/already-fired rules.
        if (pTrigger->HasBeenFired && !pTrigger->Repeatable) return;
        if (!pTrigger->IsEnabled || pTrigger->IsDisabled) return;

        if (pTrigger->TriggerAction) {
            pTrigger->TriggerAction->ExecuteAction(pTrigger);
        }
        pTrigger->HasBeenFired = true;
        pTrigger->JustFired = true;

        if (!pTrigger->Repeatable) {
            pTrigger->IsEnabled = false;
            pTrigger->State = TriggerState::Fired;
        }

        // Recurse into the next link.
        if (pTrigger->LinkedTrigger && pTrigger->IsLinked) {
            FireLinkedChain(pTrigger->LinkedTrigger, depth + 1);
        }
    }
} // anonymous namespace

// =============================================================================
// Enable - re-arm the trigger and clear the disabled flag.
// =============================================================================
void TriggerClass::Enable() {
    IsEnabled = true;
    IsDisabled = false;
    State = TriggerState::Armed;
}

// =============================================================================
// Disable - turn the trigger off; it will not evaluate until re-enabled.
// =============================================================================
void TriggerClass::Disable() {
    IsEnabled = false;
    IsDisabled = true;
    State = TriggerState::Disabled;
}

// =============================================================================
// Reset - restore the trigger to its initial armed state, clearing the
// fired flag so it can fire again even if non-repeatable.
// =============================================================================
void TriggerClass::Reset() {
    HasBeenFired = false;
    JustFired = false;
    IsEnabled = true;
    IsDisabled = false;
    IsBeingFired = false;
    forceFire = false;
    Activate = false;
    State = TriggerState::Armed;
    Timer = 0;
}

// =============================================================================
// Spring - respond to an external event notification (e.g. a unit entering a
// cell, a building being destroyed).  If the trigger's event matches, fire.
// =============================================================================
void TriggerClass::Spring(TriggerEventType eventType, AbstractClass* pObject, CellStruct cell) {
    if (!IsEnabled || IsDisabled) return;
    if (HasBeenFired && !Repeatable) return;
    if (!DifficultyEnabled(this)) return;
    if (!HouseIsActive(House)) return;

    if (State == TriggerState::Waiting) return;

    if (Event && Event->MatchEvent(eventType, pObject, cell, this)) {
        Fire();
    }
}

// =============================================================================
// ForceFire - schedule the trigger to fire on the next Update().
// =============================================================================
void TriggerClass::ForceFire() {
    forceFire = true;
    // Make sure the trigger is in a state where Update() will honour the flag.
    if (State == TriggerState::Disabled) {
        Enable();
    }
}

// =============================================================================
// SetEnabled - convenience wrapper that calls Enable or Disable.
// =============================================================================
void TriggerClass::SetEnabled(bool enabled) {
    if (enabled) {
        Enable();
    } else {
        Disable();
    }
}

// =============================================================================
// IsSatisfied - check whether the trigger's conditions are currently met.
// =============================================================================
bool TriggerClass::IsSatisfied() {
    if (!IsEnabled || IsDisabled) return false;
    if (HasBeenFired && !Repeatable) return false;
    if (!DifficultyEnabled(this)) return false;
    return CheckConditions();
}

// =============================================================================
// IsSatisfied - house-filtered variant: only returns true if the trigger
// belongs to (or is global for) the specified house.
// =============================================================================
bool TriggerClass::IsSatisfied(HouseClass* pHouse) {
    if (!pHouse) return false;
    // If the trigger has a specific house and it does not match, it is not
    // satisfied for the queried house.
    if (House && House != pHouse) return false;
    if (!IsEnabled || IsDisabled) return false;
    if (HasBeenFired && !Repeatable) return false;
    if (!DifficultyEnabled(this)) return false;
    return CheckConditions();
}

// =============================================================================
// SetEvent - attach an event object to this trigger.
// =============================================================================
void TriggerClass::SetEvent(TEventClass* pEvent) {
    Event = pEvent;
}

// =============================================================================
// SetAction - attach an action object to this trigger.
// =============================================================================
void TriggerClass::SetAction(TActionClass* pAction) {
    TriggerAction = pAction;
}

// =============================================================================
// SetHouse - set the owning house for this trigger.
// =============================================================================
void TriggerClass::SetHouse(HouseClass* pHouse) {
    House = pHouse;
}

// =============================================================================
// SetLinkedTrigger - establish a link to another trigger.  When this trigger
// fires, the linked trigger also fires (subject to its own enable rules).
// =============================================================================
void TriggerClass::SetLinkedTrigger(TriggerClass* pTrigger) {
    LinkedTrigger = pTrigger;
    IsLinked = (pTrigger != nullptr);
}

// =============================================================================
// SetData - store an arbitrary integer data value used by the action.
// =============================================================================
void TriggerClass::SetData(int32 data) {
    Data = data;
}

// =============================================================================
// SetTimer - arm a delay timer.  The trigger enters the Waiting state and will
// not evaluate conditions until the timer counts down to zero.
// =============================================================================
void TriggerClass::SetTimer(int32 frames) {
    Timer = frames;
    if (frames > 0) {
        State = TriggerState::Waiting;
    } else {
        // A zero or negative timer immediately re-arms.
        State = TriggerState::Armed;
    }
}

// =============================================================================
// ProcessTriggerEvents - static per-frame entry point.  Iterates every loaded
// trigger and calls Update() on each.
// =============================================================================
void TriggerClass::ProcessTriggerEvents() {
    if (!Array) return;
    for (int32 i = 0; i < Array->Count; ++i) {
        TriggerClass* trigger = Array->GetItem(i);
        if (trigger) trigger->Update();
    }
}

// =============================================================================
// ResetAllTriggers - reset every loaded trigger to its initial armed state.
// Typically called at scenario start.
// =============================================================================
void TriggerClass::ResetAllTriggers() {
    if (!Array) return;
    for (int32 i = 0; i < Array->Count; ++i) {
        TriggerClass* trigger = Array->GetItem(i);
        if (trigger) trigger->Reset();
    }
}

// =============================================================================
// FireAllTriggersForEvent - broadcast an external event to every trigger.
// Used by game systems (e.g. combat, discovery) to notify all triggers of a
// world event in a single pass.
// =============================================================================
void TriggerClass::FireAllTriggersForEvent(TriggerEventType eventType,
                                           AbstractClass* pObject,
                                           CellStruct cell) {
    if (!Array) return;
    for (int32 i = 0; i < Array->Count; ++i) {
        TriggerClass* trigger = Array->GetItem(i);
        if (trigger) trigger->Spring(eventType, pObject, cell);
    }
}
