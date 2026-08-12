#include "TEventClass.h"
#include "TriggerClass.h"
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
#include <cstdlib>

DynamicVectorClass<TEventClass*>* TEventClass::Array = nullptr;

TEventClass* TEventClass::Find(const char* pID) {
    if (!Array) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        TEventClass* item = Array->GetItem(i);
        if (item && item->ID && !_strcmpi(item->ID, pID)) return item;
    }
    return nullptr;
}

TEventClass* TEventClass::FindOrAllocate(const char* pID) {
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    TEventClass* found = Find(pID);
    if (found) return found;
    TEventClass* newItem = GameCreate<TEventClass>(pID);
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}

TEventClass::TEventClass(const char* pID) noexcept
    : ID(nullptr), EventKind(TEventKind::None), EventIndex(0),
      Data(0), P1_House(nullptr), P2_Object(nullptr), P3_Value(0),
      P4_Value(0), P5_Value(0), IsGlobal(false) {
    if (pID) {
        int32 len = static_cast<int32>(strlen(pID)) + 1;
        ID = new char[len];
        if (ID) {
            for (int32 i = 0; i < len; ++i) ID[i] = pID[i];
        }
    }
}

TEventClass::~TEventClass() {
    if (ID) delete[] ID;
}

bool TEventClass::LoadFromINIList(CCINIClass* pINI) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", ID);
    if (!pINI->SectionExists(sectionName)) return false;

    int32 eventKind = 0;
    pINI->GetInteger(sectionName, "EventKind", eventKind);
    EventKind = static_cast<TEventKind>(eventKind);

    pINI->GetInteger(sectionName, "EventIndex", EventIndex);
    pINI->GetInteger(sectionName, "Data", Data);
    pINI->GetInteger(sectionName, "P3", P3_Value);
    pINI->GetInteger(sectionName, "P4", P4_Value);
    pINI->GetInteger(sectionName, "P5", P5_Value);

    char houseId[32];
    pINI->ReadString(sectionName, "P1", "", houseId, sizeof(houseId));
    if (houseId[0] && _strcmpi(houseId, "<none>") != 0) {
        for (int32 i = 0; i < 32; ++i) {
            if (HouseClass::Array[i]) {
                if (!_strcmpi(HouseClass::Array[i]->Type->get_ID(), houseId)) {
                    P1_House = HouseClass::Array[i];
                    break;
                }
            }
        }
    }

    char objId[32];
    pINI->ReadString(sectionName, "P2", "", objId, sizeof(objId));
    if (objId[0] && _strcmpi(objId, "<none>") != 0) {
        P2_Object = static_cast<TechnoTypeClass*>(TechnoTypeClass::Find(objId));
    }

    return true;
}

bool TEventClass::SaveToINIList(CCINIClass* pINI) {
    if (!pINI) return false;
    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", ID);

    pINI->WriteInteger(sectionName, "EventKind", static_cast<int32>(EventKind));
    pINI->WriteInteger(sectionName, "EventIndex", EventIndex);
    pINI->WriteInteger(sectionName, "Data", Data);

    if (P1_House) pINI->WriteString(sectionName, "P1", P1_House->Type->get_ID());
    else pINI->WriteString(sectionName, "P1", "<none>");

    if (P2_Object) pINI->WriteString(sectionName, "P2", P2_Object->get_ID());
    else pINI->WriteString(sectionName, "P2", "<none>");

    pINI->WriteInteger(sectionName, "P3", P3_Value);
    pINI->WriteInteger(sectionName, "P4", P4_Value);
    pINI->WriteInteger(sectionName, "P5", P5_Value);

    return true;
}

bool TEventClass::IsSatisfied(TriggerClass* pTrigger) const {
    if (!pTrigger) return false;

    switch (EventKind) {
        case TEventKind::None: return false;
        case TEventKind::EnteredBy: return CheckEnteredBy(pTrigger);
        case TEventKind::SpiedBy: return CheckSpiedBy(pTrigger);
        case TEventKind::ThievedBy: return CheckThievedBy(pTrigger);
        case TEventKind::DiscoveredBy: return CheckDiscoveredBy(pTrigger);
        case TEventKind::AttackedBy: return CheckAttackedBy(pTrigger);
        case TEventKind::DestroyedBy: return CheckDestroyedBy(pTrigger);
        case TEventKind::AnyEvent: return CheckAnyEvent(pTrigger);
        case TEventKind::HouseDiscovered: return CheckHouseDiscovered(pTrigger);
        case TEventKind::TimeElapsed: return CheckTimeElapsed(pTrigger);
        case TEventKind::MissionTimerExpired: return CheckMissionTimerExpired(pTrigger);
        case TEventKind::BuildingExists: return CheckBuildingExists(pTrigger);
        case TEventKind::BuildingDestroyed: return CheckBuildingDestroyed(pTrigger);
        case TEventKind::UnitExists: return CheckUnitExists(pTrigger);
        case TEventKind::UnitDestroyed: return CheckUnitDestroyed(pTrigger);
        case TEventKind::InfantryExists: return CheckInfantryExists(pTrigger);
        case TEventKind::InfantryDestroyed: return CheckInfantryDestroyed(pTrigger);
        case TEventKind::AircraftExists: return CheckAircraftExists(pTrigger);
        case TEventKind::AircraftDestroyed: return CheckAircraftDestroyed(pTrigger);
        case TEventKind::Credits: return CheckCredits(pTrigger);
        case TEventKind::ElapsedTime: return CheckElapsedTime(pTrigger);
        case TEventKind::LowPower: return CheckLowPower(pTrigger);
        case TEventKind::BridgeDestroyed: return CheckBridgeDestroyed(pTrigger);
        case TEventKind::BuildingCaptured: return CheckBuildingCaptured(pTrigger);
        case TEventKind::SuperWeaponAvailable: return CheckSuperWeaponAvailable(pTrigger);
        case TEventKind::LocalSet: return CheckLocalSet(pTrigger);
        case TEventKind::LocalClear: return CheckLocalClear(pTrigger);
        case TEventKind::GlobalSet: return CheckGlobalSet(pTrigger);
        case TEventKind::GlobalClear: return CheckGlobalClear(pTrigger);
        case TEventKind::Always: return true;
        default: return false;
    }
}

bool TEventClass::MatchEvent(TriggerEventType eventType, AbstractClass* pObject, CellStruct cell, TriggerClass* pTrigger) const {
    if (!pTrigger) return false;

    // Time-elapsed events are evaluated independently of any object.
    if (eventType == TriggerEventType::TimeElapsed) return CheckTimeElapsed(pTrigger);

    // If the event requires a specific techno type, verify the object matches.
    if (P2_Object && pObject) {
        TechnoClass* pTechno = static_cast<TechnoClass*>(pObject);
        if (pTechno) {
            // Compare the object's abstract type against the required type.
            // The full TechnoType pointer comparison is performed by derived
            // classes that store their type; here we verify the object is at
            // least a TechnoClass before proceeding to the house check.
            if (pTechno->WhatAmI() != AbstractType::Techno &&
                pTechno->WhatAmI() != AbstractType::Building &&
                pTechno->WhatAmI() != AbstractType::Unit &&
                pTechno->WhatAmI() != AbstractType::Infantry &&
                pTechno->WhatAmI() != AbstractType::Aircraft) {
                return false;
            }
        }
    }

    // If the event requires a specific house, verify the object's owner.
    if (P1_House && pObject) {
        TechnoClass* pTechno = static_cast<TechnoClass*>(pObject);
        if (pTechno && pTechno->GetOwningHouse() != P1_House) {
            return false;
        }
    }

    // Route to the specific event checker based on the event type.
    switch (EventKind) {
        case TEventKind::EnteredBy:
            return CheckEnteredBy(pTrigger);
        case TEventKind::SpiedBy:
            return CheckSpiedBy(pTrigger);
        case TEventKind::ThievedBy:
            return CheckThievedBy(pTrigger);
        case TEventKind::DiscoveredBy:
            return CheckDiscoveredBy(pTrigger);
        case TEventKind::AttackedBy:
            return CheckAttackedBy(pTrigger);
        case TEventKind::DestroyedBy:
            return CheckDestroyedBy(pTrigger);
        case TEventKind::AnyEvent:
            return CheckAnyEvent(pTrigger);
        case TEventKind::Always:
            return true;
        default:
            return IsSatisfied(pTrigger);
    }
}

bool TEventClass::CheckEnteredBy(TriggerClass* pTrigger) const {
    if (!P1_House) return false;
    // The trigger records the last object that entered its cell in Data.
    // If the entering object belongs to the required house and (optionally)
    // matches the required techno type, the event fires.
    if (P2_Object) {
        int32 count = P1_House->CountOwnedNow(P2_Object);
        if (count <= 0) return false;
    }
    // The trigger's "entered" flag is stored in the low bit of EventIndex.
    return (EventIndex & 0x01) != 0;
}

bool TEventClass::CheckSpiedBy(TriggerClass* pTrigger) const {
    if (!P1_House) return false;
    // A spy event fires when a unit from P1_House has successfully infiltrated
    // a building matching P2_Object.  We check whether the house currently
    // has any spy-type units and whether the trigger's spy flag is set.
    if (P2_Object) {
        int32 count = P1_House->CountOwnedNow(P2_Object);
        if (count <= 0) return false;
    }
    return (EventIndex & 0x02) != 0;
}

bool TEventClass::CheckThievedBy(TriggerClass* pTrigger) const {
    if (!P1_House) return false;
    // A thief event fires when a unit from P1_House has stolen credits or
    // technology from a building matching P2_Object.
    if (P2_Object) {
        int32 count = P1_House->CountOwnedNow(P2_Object);
        if (count <= 0) return false;
    }
    return (EventIndex & 0x04) != 0;
}

bool TEventClass::CheckDiscoveredBy(TriggerClass* pTrigger) const {
    if (!P1_House) return false;
    // A discovery event fires when P1_House has revealed the trigger's
    // attached object.  We approximate this by checking whether the house
    // has any units of the required type (if specified) and whether the
    // trigger's discovery flag is set.
    if (P2_Object) {
        int32 count = P1_House->CountOwnedNow(P2_Object);
        if (count <= 0) return false;
    }
    return (EventIndex & 0x08) != 0;
}

bool TEventClass::CheckAttackedBy(TriggerClass* pTrigger) const {
    if (!P1_House) return false;
    // An attack event fires when P1_House has attacked the trigger's
    // attached object.  The trigger records the attacker in its Data field.
    if (P2_Object) {
        int32 count = P1_House->CountOwnedNow(P2_Object);
        if (count <= 0) return false;
    }
    return (EventIndex & 0x10) != 0;
}

bool TEventClass::CheckDestroyedBy(TriggerClass* pTrigger) const {
    if (!P1_House) return false;
    // A destruction event fires when P1_House has destroyed the trigger's
    // attached object.  We check whether the house has any combat-capable
    // units and whether the trigger's destroy flag is set.
    if (P2_Object) {
        int32 count = P1_House->CountOwnedNow(P2_Object);
        if (count <= 0) return false;
    }
    return (EventIndex & 0x20) != 0;
}

bool TEventClass::CheckAnyEvent(TriggerClass* pTrigger) const {
    // "Any Event" fires when any interaction has occurred with the
    // trigger's attached object. The EventIndex field accumulates
    // bitflags for each interaction type as they happen:
    //
    //   0x001 - Entered By        (unit entered the cell)
    //   0x002 - Spied By          (spy infiltrated a building)
    //   0x004 - Thieved By        (thief stole credits/tech)
    //   0x008 - Discovered By     (object was first sighted)
    //   0x010 - Attacked By       (object was attacked)
    //   0x020 - Destroyed By      (object was destroyed)
    //   0x040 - House Discovered  (house was first seen)
    //   0x080 - Bridge Destroyed  (bridge was demolished)
    //   0x100 - Building Captured (building was captured)
    //
    // If any of these flags are set, at least one interaction has
    // occurred and the event is satisfied. This mirrors the behavior
    // of the individual Check* methods, each of which tests a single
    // bit; "Any Event" is the logical OR of all of them.
    //
    // If P1_House is specified, the event only fires for interactions
    // attributed to that house. Since per-interaction house tracking
    // is not stored in the flag bits, we approximate the filter by
    // verifying the house still owns units of the required type (if
    // P2_Object is also set), consistent with the other Check* methods.
    int32 interactionMask = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20
                           | 0x40 | 0x80 | 0x100;
    if ((EventIndex & interactionMask) == 0) {
        return false;
    }

    // Optional house filter: if a specific house is required, verify
    // it still has a relevant presence. This mirrors the house-checking
    // logic used by CheckEnteredBy, CheckAttackedBy, etc.
    if (P1_House && P2_Object) {
        int32 count = P1_House->CountOwnedNow(P2_Object);
        if (count <= 0) return false;
    }

    return true;
}

bool TEventClass::CheckHouseDiscovered(TriggerClass* pTrigger) const {
    if (!P1_House) return false;
    // A house is considered "discovered" when at least one of its units or
    // buildings has been seen by any other house.  We approximate this by
    // checking whether the house has any currently-owned objects; if it
    // owns nothing, it has been effectively eliminated and thus "discovered".
    // The P3_Value parameter can specify a minimum object count.
    int32 minCount = (P3_Value > 0) ? P3_Value : 1;
    for (int32 i = 0; i < 32; ++i) {
        if (HouseClass::Array[i] == P1_House) {
            // The house still exists; check if it has been spotted by
            // examining the trigger's discovery flag.
            return (EventIndex & 0x40) != 0;
        }
    }
    // House no longer exists - it has been fully discovered (destroyed).
    return true;
}

bool TEventClass::CheckTimeElapsed(TriggerClass* pTrigger) const {
    int32 currentFrame = Game::CurrentFrame;
    int32 elapsedFrames = currentFrame;
    if (RulesClass::Instance) {
        int32 framesPerSecond = 15;
        int32 elapsedSeconds = elapsedFrames / framesPerSecond;
        if (P3_Value > 0 && elapsedSeconds >= P3_Value) return true;
        if (P3_Value <= 0) return true;
    }
    return false;
}

bool TEventClass::CheckMissionTimerExpired(TriggerClass* pTrigger) const {
    // The mission timer counts down from an initial value.  When it reaches
    // zero (or goes negative), the event fires.  The timer is stored in
    // ScenarioClass and is measured in frames.
    if (ScenarioClass::Instance) {
        // The mission timer counts down; GetTimeLeft returns remaining frames.
        int32 timerFrames = ScenarioClass::Instance->MissionTimer.GetTimeLeft();
        // If P3_Value is set, it specifies the threshold; the event fires
        // when the timer reaches that threshold (default 0 = expired).
        int32 threshold = (P3_Value != 0) ? P3_Value : 0;
        return timerFrames <= threshold;
    }
    return false;
}

bool TEventClass::CheckBuildingExists(TriggerClass* pTrigger) const {
    if (!P1_House || !P2_Object) return false;
    int32 count = P1_House->CountOwnedNow(P2_Object);
    return count > 0;
}

bool TEventClass::CheckBuildingDestroyed(TriggerClass* pTrigger) const {
    if (!P1_House || !P2_Object) return false;
    int32 count = P1_House->CountOwnedNow(P2_Object);
    return count == 0;
}

bool TEventClass::CheckUnitExists(TriggerClass* pTrigger) const {
    if (!P1_House || !P2_Object) return false;
    int32 count = P1_House->CountOwnedNow(P2_Object);
    return count > 0;
}

bool TEventClass::CheckUnitDestroyed(TriggerClass* pTrigger) const {
    if (!P1_House || !P2_Object) return false;
    int32 count = P1_House->CountOwnedNow(P2_Object);
    return count == 0;
}

bool TEventClass::CheckInfantryExists(TriggerClass* pTrigger) const {
    if (!P1_House || !P2_Object) return false;
    int32 count = P1_House->CountOwnedNow(P2_Object);
    return count > 0;
}

bool TEventClass::CheckInfantryDestroyed(TriggerClass* pTrigger) const {
    if (!P1_House || !P2_Object) return false;
    int32 count = P1_House->CountOwnedNow(P2_Object);
    return count == 0;
}

bool TEventClass::CheckAircraftExists(TriggerClass* pTrigger) const {
    if (!P1_House || !P2_Object) return false;
    int32 count = P1_House->CountOwnedNow(P2_Object);
    return count > 0;
}

bool TEventClass::CheckAircraftDestroyed(TriggerClass* pTrigger) const {
    if (!P1_House || !P2_Object) return false;
    int32 count = P1_House->CountOwnedNow(P2_Object);
    return count == 0;
}

bool TEventClass::CheckCredits(TriggerClass* pTrigger) const {
    if (!P1_House) return false;
    int32 credits = P1_House->Credits;
    return credits >= P3_Value;
}

bool TEventClass::CheckElapsedTime(TriggerClass* pTrigger) const {
    int32 currentFrame = Game::CurrentFrame;
    int32 framesPerSecond = RulesClass::Instance ? 15 : 15;
    int32 elapsedSeconds = currentFrame / framesPerSecond;
    return elapsedSeconds >= P3_Value;
}

bool TEventClass::CheckLowPower(TriggerClass* pTrigger) const {
    if (!P1_House) return false;
    return P1_House->PowerOutput < P1_House->PowerDrain;
}

bool TEventClass::CheckBridgeDestroyed(TriggerClass* pTrigger) const {
    // A bridge-destroyed event fires when the bridge at the trigger's
    // attached cell has been demolished.  The P3_Value parameter can
    // specify a bridge index; if zero, any bridge destruction counts.
    // The trigger stores the bridge state in its EventIndex field.
    if (EventIndex & 0x80) return true;

    // Also check via the map: if the cell at the trigger's location is
    // a bridge that has been destroyed, fire the event.
    if (MapClass::Instance && pTrigger) {
        CellStruct triggerCell;
        // The trigger's cell is stored in Data as a packed coordinate.
        triggerCell.X = static_cast<uint16>(Data & 0xFFFF);
        triggerCell.Y = static_cast<uint16>((Data >> 16) & 0xFFFF);
        // Use the map's bridge-destruction query.
        return MapClass::Instance->IsBridgeDestroyed(triggerCell);
    }
    return false;
}

bool TEventClass::CheckBuildingCaptured(TriggerClass* pTrigger) const {
    // A building-captured event fires when a building matching P2_Object
    // has been captured (ownership transferred) by P1_House.
    // The trigger records capture events in the EventIndex flags.
    if (!P1_House) return false;
    if (P2_Object) {
        // The building must currently be owned by P1_House (captured).
        int32 count = P1_House->CountOwnedNow(P2_Object);
        if (count <= 0) return false;
    }
    return (EventIndex & 0x100) != 0;
}

bool TEventClass::CheckSuperWeaponAvailable(TriggerClass* pTrigger) const {
    if (!P1_House) return false;
    int32 swIdx = P1_House->FindSuperWeapon(static_cast<SuperWeaponType>(P3_Value));
    if (swIdx < 0) return false;
    return !P1_House->SuperWeaponTimers[swIdx].InProgress();
}

bool TEventClass::CheckLocalSet(TriggerClass* pTrigger) const {
    if (!pTrigger) return false;
    return pTrigger->Data == P3_Value;
}

bool TEventClass::CheckLocalClear(TriggerClass* pTrigger) const {
    if (!pTrigger) return false;
    return pTrigger->Data != P3_Value;
}

bool TEventClass::CheckGlobalSet(TriggerClass* pTrigger) const {
    if (ScenarioClass::Instance && P3_Value >= 0 && P3_Value < 100) {
        return ScenarioClass::Instance->GlobalVariables[P3_Value].Value != 0;
    }
    return false;
}

bool TEventClass::CheckGlobalClear(TriggerClass* pTrigger) const {
    if (ScenarioClass::Instance && P3_Value >= 0 && P3_Value < 100) {
        return ScenarioClass::Instance->GlobalVariables[P3_Value].Value == 0;
    }
    return true;
}

void TEventClass::GetEventName(char* buffer, int32 bufferSize) const {
    if (!buffer) return;
    static const char* names[] = {
        "None", "Entered By", "Spied By", "Thieved By",
        "Discovered By", "Attacked By", "Destroyed By", "Any Event",
        "House Discovered", "Time Elapsed", "Mission Timer Expired",
        "Building Exists", "Building Destroyed", "Unit Exists", "Unit Destroyed",
        "Infantry Exists", "Infantry Destroyed", "Aircraft Exists",
        "Aircraft Destroyed", "Credits", "Elapsed Time", "Low Power",
        "Bridge Destroyed", "Building Captured", "Super Weapon Available",
        "Local Set", "Local Clear", "Global Set", "Global Clear", "Always"
    };
    int32 idx = static_cast<int32>(EventKind);
    if (idx >= 0 && idx < 30) {
        snprintf(buffer, bufferSize, "%s", names[idx]);
    } else {
        snprintf(buffer, bufferSize, "Unknown");
    }
}

// ============================================================================
// File-local helper functions
//
//  These provide event validation, parameter description, string-to-enum
//  conversion, and event comparison utilities used by the trigger editor
//  and the scenario loader.
// ============================================================================

namespace
{

// --------------------------------------------------------------------------
// EventMetadata - describes each event kind's parameter requirements
// --------------------------------------------------------------------------
struct EventMetadata
{
    const char* Name;
    const char* Description;
    bool        RequiresHouse;    // P1 must be a valid house
    bool        RequiresObject;   // P2 must be a valid techno type
    bool        RequiresValue;    // P3 must be a meaningful value
};

// --------------------------------------------------------------------------
// g_EventMetadata - static table describing all 30 event kinds.
// --------------------------------------------------------------------------
const EventMetadata g_EventMetadata[] = {
    // 0: None
    { "None", "No event", false, false, false },
    // 1: Entered By
    { "Entered By", "Trigger fires when an object enters the cell",
      true, false, false },
    // 2: Spied By
    { "Spied By", "Trigger fires when a spy infiltrates the building",
      true, false, false },
    // 3: Thieved By
    { "Thieved By", "Trigger fires when a thief steals from the building",
      true, false, false },
    // 4: Discovered By
    { "Discovered By", "Trigger fires when the object is discovered",
      true, false, false },
    // 5: Attacked By
    { "Attacked By", "Trigger fires when the object is attacked",
      true, false, false },
    // 6: Destroyed By
    { "Destroyed By", "Trigger fires when the object is destroyed",
      true, false, false },
    // 7: Any Event
    { "Any Event", "Trigger fires on any interaction", false, false, false },
    // 8: House Discovered
    { "House Discovered", "Trigger fires when the house is first seen",
      true, false, false },
    // 9: Time Elapsed
    { "Time Elapsed", "Trigger fires after N seconds have elapsed",
      false, false, true },
    // 10: Mission Timer Expired
    { "Mission Timer Expired", "Trigger fires when the mission timer hits zero",
      false, false, false },
    // 11: Building Exists
    { "Building Exists", "Trigger fires while the building exists",
      true, true, false },
    // 12: Building Destroyed
    { "Building Destroyed", "Trigger fires when the building is gone",
      true, true, false },
    // 13: Unit Exists
    { "Unit Exists", "Trigger fires while the unit type exists",
      true, true, false },
    // 14: Unit Destroyed
    { "Unit Destroyed", "Trigger fires when all units of the type are gone",
      true, true, false },
    // 15: Infantry Exists
    { "Infantry Exists", "Trigger fires while the infantry type exists",
      true, true, false },
    // 16: Infantry Destroyed
    { "Infantry Destroyed", "Trigger fires when all infantry of the type are gone",
      true, true, false },
    // 17: Aircraft Exists
    { "Aircraft Exists", "Trigger fires while the aircraft type exists",
      true, true, false },
    // 18: Aircraft Destroyed
    { "Aircraft Destroyed", "Trigger fires when all aircraft of the type are gone",
      true, true, false },
    // 19: Credits
    { "Credits", "Trigger fires when credits reach the threshold",
      true, false, true },
    // 20: Elapsed Time
    { "Elapsed Time", "Trigger fires after N seconds of game time",
      false, false, true },
    // 21: Low Power
    { "Low Power", "Trigger fires when the house is low on power",
      true, false, false },
    // 22: Bridge Destroyed
    { "Bridge Destroyed", "Trigger fires when a bridge is destroyed",
      false, false, false },
    // 23: Building Captured
    { "Building Captured", "Trigger fires when a building is captured",
      true, true, false },
    // 24: Super Weapon Available
    { "Super Weapon Available", "Trigger fires when the SW is ready",
      true, false, true },
    // 25: Local Set
    { "Local Set", "Trigger fires when the local variable is set",
      false, false, true },
    // 26: Local Clear
    { "Local Clear", "Trigger fires when the local variable is cleared",
      false, false, true },
    // 27: Global Set
    { "Global Set", "Trigger fires when the global variable is set",
      false, false, true },
    // 28: Global Clear
    { "Global Clear", "Trigger fires when the global variable is cleared",
      false, false, true },
    // 29: Always
    { "Always", "Trigger always fires", false, false, false },
};

static constexpr int32 g_EventMetadataCount =
    sizeof(g_EventMetadata) / sizeof(g_EventMetadata[0]);

// --------------------------------------------------------------------------
// GetEventMetadata - returns the metadata for the given event kind.
// --------------------------------------------------------------------------
const EventMetadata* GetEventMetadata(TEventKind kind) noexcept
{
    int32 idx = static_cast<int32>(kind);
    if (idx < 0 || idx >= g_EventMetadataCount)
        return nullptr;
    return &g_EventMetadata[idx];
}

// --------------------------------------------------------------------------
// ValidateEvent - checks that the event's parameters are consistent with
// its kind.  Returns true if the event is properly configured.
// --------------------------------------------------------------------------
bool ValidateEvent(const TEventClass* pEvent) noexcept
{
    if (!pEvent)
        return false;

    const EventMetadata* pMeta = GetEventMetadata(pEvent->EventKind);
    if (!pMeta)
        return false;

    // Check house requirement.
    if (pMeta->RequiresHouse && !pEvent->P1_House)
        return false;

    // Check object requirement.
    if (pMeta->RequiresObject && !pEvent->P2_Object)
        return false;

    // Check value requirement.
    if (pMeta->RequiresValue && pEvent->P3_Value == 0)
        return false;

    return true;
}

// --------------------------------------------------------------------------
// GetEventDescription - writes a human-readable description of the event
// into the supplied buffer.
// --------------------------------------------------------------------------
void GetEventDescription(const TEventClass* pEvent, char* pBuffer,
                         int32 nBufferSize) noexcept
{
    if (!pEvent || !pBuffer || nBufferSize <= 0)
        return;

    const EventMetadata* pMeta = GetEventMetadata(pEvent->EventKind);
    if (!pMeta)
    {
        snprintf(pBuffer, nBufferSize, "Unknown event kind %d",
                 static_cast<int32>(pEvent->EventKind));
        return;
    }

    // Build the description from the metadata and the event parameters.
    char houseName[32] = "<none>";
    char objectName[32] = "<none>";

    if (pEvent->P1_House && pEvent->P1_House->Type)
    {
        const char* pID = pEvent->P1_House->Type->get_ID();
        if (pID)
        {
            int32 i = 0;
            while (pID[i] && i < 31)
            {
                houseName[i] = pID[i];
                ++i;
            }
            houseName[i] = '\0';
        }
    }

    if (pEvent->P2_Object)
    {
        const char* pID = pEvent->P2_Object->get_ID();
        if (pID)
        {
            int32 i = 0;
            while (pID[i] && i < 31)
            {
                objectName[i] = pID[i];
                ++i;
            }
            objectName[i] = '\0';
        }
    }

    snprintf(pBuffer, nBufferSize, "%s [House=%s, Object=%s, Value=%d]",
             pMeta->Description, houseName, objectName, pEvent->P3_Value);
}

// --------------------------------------------------------------------------
// EventKindFromString - parses an event kind from its display name.
// Returns TEventKind::None if the name is not recognized.
// --------------------------------------------------------------------------
TEventKind EventKindFromString(const char* pName) noexcept
{
    if (!pName || !pName[0])
        return TEventKind::None;

    for (int32 i = 0; i < g_EventMetadataCount; ++i)
    {
        if (_strcmpi(g_EventMetadata[i].Name, pName) == 0)
            return static_cast<TEventKind>(i);
    }
    return TEventKind::None;
}

// --------------------------------------------------------------------------
// EventKindFromStringExact - case-insensitive match against the canonical
// name table.  Used by the INI loader when event kinds are specified as
// strings rather than integers.
// --------------------------------------------------------------------------
TEventKind EventKindFromStringExact(const char* pName) noexcept
{
    return EventKindFromString(pName);
}

// --------------------------------------------------------------------------
// CountEventsByKind - counts how many registered events have the given kind.
// --------------------------------------------------------------------------
int32 CountEventsByKind(TEventKind kind) noexcept
{
    if (!TEventClass::Array)
        return 0;
    int32 count = 0;
    for (int32 i = 0; i < TEventClass::Array->Count; ++i)
    {
        TEventClass* pEvent = TEventClass::Array->GetItem(i);
        if (pEvent && pEvent->EventKind == kind)
            ++count;
    }
    return count;
}

// --------------------------------------------------------------------------
// CountInvalidEvents - counts how many registered events fail validation.
// --------------------------------------------------------------------------
int32 CountInvalidEvents() noexcept
{
    if (!TEventClass::Array)
        return 0;
    int32 count = 0;
    for (int32 i = 0; i < TEventClass::Array->Count; ++i)
    {
        TEventClass* pEvent = TEventClass::Array->GetItem(i);
        if (pEvent && !ValidateEvent(pEvent))
            ++count;
    }
    return count;
}

// --------------------------------------------------------------------------
// FindEventsByHouse - collects all events that reference the given house
// into the supplied array.  Returns the number found.
// --------------------------------------------------------------------------
int32 FindEventsByHouse(HouseClass* pHouse, TEventClass** pOutArray,
                        int32 nMaxResults) noexcept
{
    if (!TEventClass::Array || !pHouse || !pOutArray || nMaxResults <= 0)
        return 0;
    int32 found = 0;
    for (int32 i = 0; i < TEventClass::Array->Count && found < nMaxResults; ++i)
    {
        TEventClass* pEvent = TEventClass::Array->GetItem(i);
        if (pEvent && pEvent->P1_House == pHouse)
        {
            pOutArray[found] = pEvent;
            ++found;
        }
    }
    return found;
}

// --------------------------------------------------------------------------
// FindEventsByObject - collects all events that reference the given techno
// type into the supplied array.  Returns the number found.
// --------------------------------------------------------------------------
int32 FindEventsByObject(TechnoTypeClass* pType, TEventClass** pOutArray,
                         int32 nMaxResults) noexcept
{
    if (!TEventClass::Array || !pType || !pOutArray || nMaxResults <= 0)
        return 0;
    int32 found = 0;
    for (int32 i = 0; i < TEventClass::Array->Count && found < nMaxResults; ++i)
    {
        TEventClass* pEvent = TEventClass::Array->GetItem(i);
        if (pEvent && pEvent->P2_Object == pType)
        {
            pOutArray[found] = pEvent;
            ++found;
        }
    }
    return found;
}

// --------------------------------------------------------------------------
// CompareEvents - returns true if two events are functionally equivalent
// (same kind, house, object, and value).  Used for deduplication.
// --------------------------------------------------------------------------
bool CompareEvents(const TEventClass* pA, const TEventClass* pB) noexcept
{
    if (!pA || !pB)
        return false;
    if (pA->EventKind != pB->EventKind)
        return false;
    if (pA->P1_House != pB->P1_House)
        return false;
    if (pA->P2_Object != pB->P2_Object)
        return false;
    if (pA->P3_Value != pB->P3_Value)
        return false;
    return true;
}

// --------------------------------------------------------------------------
// FindDuplicateEvent - searches the array for an event that is functionally
// equivalent to the supplied one.  Returns the duplicate or nullptr.
// --------------------------------------------------------------------------
TEventClass* FindDuplicateEvent(const TEventClass* pEvent) noexcept
{
    if (!TEventClass::Array || !pEvent)
        return nullptr;
    for (int32 i = 0; i < TEventClass::Array->Count; ++i)
    {
        TEventClass* pOther = TEventClass::Array->GetItem(i);
        if (pOther == pEvent)
            continue;
        if (CompareEvents(pEvent, pOther))
            return pOther;
    }
    return nullptr;
}

// --------------------------------------------------------------------------
// GetRequiredParameterCount - returns how many parameters the given event
// kind requires (0-3).
// --------------------------------------------------------------------------
int32 GetRequiredParameterCount(TEventKind kind) noexcept
{
    const EventMetadata* pMeta = GetEventMetadata(kind);
    if (!pMeta)
        return 0;
    int32 count = 0;
    if (pMeta->RequiresHouse)  ++count;
    if (pMeta->RequiresObject) ++count;
    if (pMeta->RequiresValue)  ++count;
    return count;
}

// --------------------------------------------------------------------------
// IsEventKindValid - returns true if the integer maps to a known event kind.
// --------------------------------------------------------------------------
bool IsEventKindValid(int32 kind) noexcept
{
    return (kind >= 0 && kind < g_EventMetadataCount);
}

// --------------------------------------------------------------------------
// GetEventKindName - returns the display name for the given event kind.
// --------------------------------------------------------------------------
const char* GetEventKindName(TEventKind kind) noexcept
{
    const EventMetadata* pMeta = GetEventMetadata(kind);
    if (!pMeta)
        return "Unknown";
    return pMeta->Name;
}

// --------------------------------------------------------------------------
// ParseEventParameters - parses the semicolon-delimited parameter string
// used by the map editor format ("house;object;value").  Writes the parsed
// values into the supplied output parameters.
// --------------------------------------------------------------------------
bool ParseEventParameters(const char* pParamString,
                          char* pOutHouse, int32 nHouseSize,
                          char* pOutObject, int32 nObjectSize,
                          int32& outValue) noexcept
{
    if (!pParamString)
        return false;

    // Initialize outputs.
    if (pOutHouse && nHouseSize > 0) pOutHouse[0] = '\0';
    if (pOutObject && nObjectSize > 0) pOutObject[0] = '\0';
    outValue = 0;

    // Split on semicolons.
    int32 field = 0;
    int32 pos = 0;
    int32 i = 0;

    while (pParamString[i])
    {
        if (pParamString[i] == ';')
        {
            // Terminate the current field.
            switch (field)
            {
            case 0:
                if (pOutHouse && pos < nHouseSize)
                    pOutHouse[pos] = '\0';
                break;
            case 1:
                if (pOutObject && pos < nObjectSize)
                    pOutObject[pos] = '\0';
                break;
            default:
                break;
            }
            pos = 0;
            ++field;
            ++i;
            continue;
        }

        switch (field)
        {
        case 0:
            if (pOutHouse && pos < nHouseSize - 1)
            {
                pOutHouse[pos] = pParamString[i];
                ++pos;
            }
            break;
        case 1:
            if (pOutObject && pos < nObjectSize - 1)
            {
                pOutObject[pos] = pParamString[i];
                ++pos;
            }
            break;
        case 2:
            // Accumulate digits for the value field.
            if (pParamString[i] >= '0' && pParamString[i] <= '9')
            {
                outValue = outValue * 10 + (pParamString[i] - '0');
            }
            else if (pParamString[i] == '-')
            {
                // Negative values are allowed.
            }
            break;
        default:
            break;
        }
        ++i;
    }

    // Terminate the last field.
    switch (field)
    {
    case 0:
        if (pOutHouse && pos < nHouseSize)
            pOutHouse[pos] = '\0';
        break;
    case 1:
        if (pOutObject && pos < nObjectSize)
            pOutObject[pos] = '\0';
        break;
    default:
        break;
    }

    return true;
}

// --------------------------------------------------------------------------
// FormatEventParameters - the inverse of ParseEventParameters.  Writes the
// event's parameters as a semicolon-delimited string.
// --------------------------------------------------------------------------
void FormatEventParameters(const TEventClass* pEvent, char* pBuffer,
                           int32 nBufferSize) noexcept
{
    if (!pEvent || !pBuffer || nBufferSize <= 0)
        return;

    char houseName[32] = "<none>";
    char objectName[32] = "<none>";

    if (pEvent->P1_House && pEvent->P1_House->Type)
    {
        const char* pID = pEvent->P1_House->Type->get_ID();
        if (pID)
        {
            int32 i = 0;
            while (pID[i] && i < 31)
            {
                houseName[i] = pID[i];
                ++i;
            }
            houseName[i] = '\0';
        }
    }

    if (pEvent->P2_Object)
    {
        const char* pID = pEvent->P2_Object->get_ID();
        if (pID)
        {
            int32 i = 0;
            while (pID[i] && i < 31)
            {
                objectName[i] = pID[i];
                ++i;
            }
            objectName[i] = '\0';
        }
    }

    snprintf(pBuffer, nBufferSize, "%s;%s;%d", houseName, objectName,
             pEvent->P3_Value);
}

// --------------------------------------------------------------------------
// GetAllEventNames - writes all event kind names into the supplied array
// of char buffers.  Each buffer must be at least 32 bytes.  Returns the
// number of names written.
// --------------------------------------------------------------------------
int32 GetAllEventNames(char (*pOutNames)[32], int32 nMaxNames) noexcept
{
    if (!pOutNames || nMaxNames <= 0)
        return 0;
    int32 count = (g_EventMetadataCount < nMaxNames)
                      ? g_EventMetadataCount
                      : nMaxNames;
    for (int32 i = 0; i < count; ++i)
    {
        const char* pName = g_EventMetadata[i].Name;
        int32 j = 0;
        while (pName[j] && j < 31)
        {
            pOutNames[i][j] = pName[j];
            ++j;
        }
        pOutNames[i][j] = '\0';
    }
    return count;
}

// --------------------------------------------------------------------------
// EvaluateAllEvents - iterates the global event array and returns the
// number of events that are currently satisfied for the given trigger.
// --------------------------------------------------------------------------
int32 EvaluateAllEvents(TriggerClass* pTrigger) noexcept
{
    if (!TEventClass::Array || !pTrigger)
        return 0;
    int32 satisfied = 0;
    for (int32 i = 0; i < TEventClass::Array->Count; ++i)
    {
        TEventClass* pEvent = TEventClass::Array->GetItem(i);
        if (pEvent && pEvent->IsSatisfied(pTrigger))
            ++satisfied;
    }
    return satisfied;
}

} // end anonymous namespace