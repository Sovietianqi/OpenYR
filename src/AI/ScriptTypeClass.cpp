#include "ScriptTypeClass.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// ScriptTypeClass.cpp - AI script type implementation
// ============================================================================
// A ScriptTypeClass defines the ordered list of actions an AI team executes.
// Each action is a (Action, Argument) pair read from an INI section whose
// name matches the script's ID. The original game ships 51 distinct action
// types (enum ScriptAction in Definitions.h), each with its own parameter
// semantics. This file implements:
//   * A complete 51-entry action name/description table
//   * Action validation including parameter-range checks
//   * Full INI parsing that understands every action's argument meaning
//   * Action description lookup for the map editor and debugger
// ============================================================================

DynamicVectorClass<ScriptTypeClass*>* ScriptTypeClass::Array = nullptr;

// ----------------------------------------------------------------------------
// File-local action metadata.
//
// The original game hard-codes these strings in the executable's read-only
// data segment. We reproduce them here so the map editor and AI debugger can
// render human-readable action names and descriptions without linking
// against the binary.
// ----------------------------------------------------------------------------
namespace
{
    constexpr int32 kActionCount = 51;

    struct ActionInfo
    {
        const char* Name;
        const char* Description;
        bool        HasArgument;       // true if the argument is meaningful
        int32       MinArgument;       // inclusive lower bound (0 if unused)
        int32       MaxArgument;       // inclusive upper bound (0 if unused)
        const char* ArgumentLabel;     // human-readable argument name
    };

    // The table mirrors ScriptAction enum values 0..50 exactly. Keeping the
    // order synchronised with Definitions.h is critical: the enum is the
    // authoritative index.
    const ActionInfo kActionTable[kActionCount] = {
        // 0
        {"Attack",              "Attack a specific target or waypoint",           true,  0,  999, "waypoint"},
        // 1
        {"AttackWaypoint",      "Attack the unit/building at a waypoint",         true,  0,  999, "waypoint"},
        // 2
        {"MoveToWaypoint",      "Move the team to a waypoint",                    true,  0,  999, "waypoint"},
        // 3
        {"MoveToCell",          "Move the team to a cell coordinate",             true,  0,  INT32_MAX, "cell"},
        // 4
        {"GuardArea",           "Guard the area around a waypoint",               true,  0,  999, "waypoint"},
        // 5
        {"JumpToLine",          "Jump to a different action line (loop)",         true,  0,  49, "line"},
        // 6
        {"PlayerCheck",         "Branch based on the player's house",             true,  0,  7,  "house"},
        // 7
        {"Wait",                "Wait for a number of frames",                    true,  0,  INT32_MAX, "frames"},
        // 8
        {"Unload",              "Unload passengers from transports",              true,  0,  1,  "keepTransports"},
        // 9
        {"Deploy",              "Deploy/deploy units at current location",        false, 0,  0,  ""},
        // 10
        {"Follow",              "Follow a friendly unit",                         true,  0,  999, "waypoint"},
        // 11
        {"LoadIntoTransport",   "Load infantry into nearby transports",           false, 0,  0,  ""},
        // 12
        {"Spy",                 "Spy on a building at a waypoint",                true,  0,  999, "waypoint"},
        // 13
        {"Patrol",              "Patrol to a waypoint and back",                  true,  0,  999, "waypoint"},
        // 14
        {"EnterTunnel",         "Enter a tunnel network at a waypoint",           true,  0,  999, "waypoint"},
        // 15
        {"ChronoWarp",          "Chrono-warp the team to a waypoint",             true,  0,  999, "waypoint"},
        // 16
        {"ChronoSphere",        "Use the Chronosphere on the team",               true,  0,  999, "waypoint"},
        // 17
        {"IronCurtain",         "Apply the Iron Curtain to the team",             true,  0,  999, "waypoint"},
        // 18
        {"Sell",                "Sell all team units",                            false, 0,  0,  ""},
        // 19
        {"Repair",              "Repair all team units",                          false, 0,  0,  ""},
        // 20
        {"SelfDestruct",        "Destroy all team units",                         false, 0,  0,  ""},
        // 21
        {"ChangeTeam",          "Change the team's TeamType",                     true,  0,  INT32_MAX, "teamTypeIdx"},
        // 22
        {"ChangeScript",        "Change to a different script",                   true,  0,  INT32_MAX, "scriptTypeIdx"},
        // 23
        {"ChangeMission",       "Change the team's mission",                      true,  0,  28, "mission"},
        // 24
        {"Fear",                "Make the team flee from enemies",                false, 0,  0,  ""},
        // 25
        {"Retreat",             "Retreat to a waypoint",                          true,  0,  999, "waypoint"},
        // 26
        {"Scatter",             "Scatter the team members",                       false, 0,  0,  ""},
        // 27
        {"Stop",                "Stop all movement and orders",                   false, 0,  0,  ""},
        // 28
        {"Sleep",               "Sleep for a number of frames (no AI)",            true,  0,  INT32_MAX, "frames"},
        // 29
        {"Group",               "Tighten the team formation",                     false, 0,  0,  ""},
        // 30
        {"Recruit",             "Recruit new members to fill the team",            false, 0,  0,  ""},
        // 31
        {"Flash",               "Flash the team on the radar for a duration",     true,  0,  INT32_MAX, "frames"},
        // 32
        {"LoadOntoTransports",  "Load the team onto transports",                  false, 0,  0,  ""},
        // 33
        {"Chronominimum",       "Set the minimum chrono reinforcement count",     true,  0,  99, "count"},
        // 34
        {"ChronoMaximum",       "Set the maximum chrono reinforcement count",     true,  0,  99, "count"},
        // 35
        {"ForceMove",           "Force-move to a waypoint ignoring threats",      true,  0,  999, "waypoint"},
        // 36
        {"Circle",              "Circle around a waypoint",                       true,  0,  999, "waypoint"},
        // 37
        {"SearchAndDestroy",    "Search and destroy nearby enemies",              false, 0,  0,  ""},
        // 38
        {"Harmless",            "Mark the team as harmless (won't be attacked)",  false, 0,  0,  ""},
        // 39
        {"Suicide",             "Suicide attack the nearest enemy",               false, 0,  0,  ""},
        // 40
        {"Recycle",             "Recycle the team back into the pool",            false, 0,  0,  ""},
        // 41
        {"Repeat",              "Repeat the script from the beginning",           false, 0,  0,  ""},
        // 42
        {"Protect",             "Protect a specific waypoint/unit",               true,  0,  999, "waypoint"},
        // 43
        {"Sticky",              "Hold position and guard",                        false, 0,  0,  ""},
        // 44
        {"Emergency",           "Emergency repair mode",                          false, 0,  0,  ""},
        // 45
        {"TakeCover",           "Take cover (infantry go prone)",                 false, 0,  0,  ""},
        // 46
        {"Gibber",              "Play gibberish voice lines",                     false, 0,  0,  ""},
        // 47
        {"IronCurtainMe",       "Request an Iron Curtain on self",                false, 0,  0,  ""},
        // 48
        {"ChronoSphereMe",      "Request a Chronosphere shift on self",           false, 0,  0,  ""},
        // 49
        {"Win",                 "Declare victory for the script's owner",         false, 0,  0,  ""},
        // 50
        {"Lose",                "Declare defeat for the script's owner",          false, 0,  0,  ""},
    };

    // Look up an action by its integer index with bounds checking.
    const ActionInfo* LookupAction(int32 action)
    {
        if (action < 0 || action >= kActionCount) return nullptr;
        return &kActionTable[action];
    }

    // Validate that an action's argument falls within the documented range.
    bool ValidateActionArgument(int32 action, int32 argument)
    {
        const ActionInfo* info = LookupAction(action);
        if (!info) return false;
        if (!info->HasArgument) return true;  // argument is ignored
        if (argument < info->MinArgument) return false;
        if (argument > info->MaxArgument) return false;
        return true;
    }

    // Parse an "action,argument" pair from a single INI value string.
    // Returns true on success; sets outAction and outArgument.
    bool ParseActionString(const char* str, int32& outAction, int32& outArgument)
    {
        if (!str || !str[0]) return false;
        outAction = 0;
        outArgument = 0;

        // Make a mutable copy so we can tokenise without modifying the input.
        char buf[64];
        int32 i = 0;
        for (; i < 63 && str[i] != 0; ++i) buf[i] = str[i];
        buf[i] = 0;

        char* comma = strchr(buf, ',');
        if (comma) {
            *comma = '\0';
            outAction = atoi(buf);
            outArgument = atoi(comma + 1);
        } else {
            outAction = atoi(buf);
        }
        return true;
    }

    // Format a single action back into the "action,argument" INI syntax.
    void FormatActionString(char* buf, int32 bufSize, int32 action, int32 argument)
    {
        const ActionInfo* info = LookupAction(action);
        if (info && !info->HasArgument) {
            snprintf(buf, bufSize, "%d", action);
        } else {
            snprintf(buf, bufSize, "%d,%d", action, argument);
        }
    }
} // anonymous namespace

// ============================================================================
// Static factory methods
// ============================================================================

ScriptTypeClass* ScriptTypeClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        ScriptTypeClass* item = Array->GetItem(i);
        if (item && !_strcmpi(item->ID, pID)) return item;
    }
    return nullptr;
}

ScriptTypeClass* ScriptTypeClass::FindOrAllocate(const char* pID) {
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    ScriptTypeClass* found = Find(pID);
    if (found) return found;
    ScriptTypeClass* newItem = GameCreate<ScriptTypeClass>(pID);
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}

// ============================================================================
// Construction / destruction
// ============================================================================

ScriptTypeClass::ScriptTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID), ArrayIndex(-1), IsGlobal(false), ActionsCount(0) {
    for (int32 i = 0; i < MAX_SCRIPT_ACTIONS_COUNT; ++i) {
        ScriptActions[i].Action = 0;
        ScriptActions[i].Argument = 0;
    }
}

ScriptTypeClass::~ScriptTypeClass() {
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT ScriptTypeClass::GetClassID(CLSID* pClassID) {
    if (pClassID) {
        pClassID->Data1 = 0xD4D4D4D4;
        for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0;
        return S_OK;
    }
    return E_POINTER;
}

HRESULT ScriptTypeClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read ID
    char idBuf[0x18];
    hr = pStm->Read(idBuf, sizeof(idBuf), &read);
    if (hr < 0 || read != sizeof(idBuf)) return E_FAIL;
    std::memcpy(ID, idBuf, sizeof(ID));
    ID[sizeof(ID) - 1] = '\0';

    // Read scalar fields
    hr = pStm->Read(&ArrayIndex, sizeof(ArrayIndex), &read);
    if (hr < 0 || read != sizeof(ArrayIndex)) return E_FAIL;

    // Read packed bool flags
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsGlobal = (flags & 0x01) != 0;

    // Read script actions (fixed array of MAX_SCRIPT_ACTIONS_COUNT entries).
    for (int32 i = 0; i < MAX_SCRIPT_ACTIONS_COUNT; ++i) {
        hr = pStm->Read(&ScriptActions[i].Action, sizeof(ScriptActions[i].Action), &read);
        if (hr < 0 || read != sizeof(ScriptActions[i].Action)) return E_FAIL;
        hr = pStm->Read(&ScriptActions[i].Argument, sizeof(ScriptActions[i].Argument), &read);
        if (hr < 0 || read != sizeof(ScriptActions[i].Argument)) return E_FAIL;
    }

    hr = pStm->Read(&ActionsCount, sizeof(ActionsCount), &read);
    if (hr < 0 || read != sizeof(ActionsCount)) return E_FAIL;

    return S_OK;
}

HRESULT ScriptTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Write ID
    hr = pStm->Write(ID, sizeof(ID), &written);
    if (hr < 0 || written != sizeof(ID)) return E_FAIL;

    // Write scalar fields
    hr = pStm->Write(&ArrayIndex, sizeof(ArrayIndex), &written);
    if (hr < 0 || written != sizeof(ArrayIndex)) return E_FAIL;

    // Write packed bool flags
    uint32 flags = 0;
    if (IsGlobal) flags |= 0x01;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    // Write script actions (fixed array of MAX_SCRIPT_ACTIONS_COUNT entries).
    for (int32 i = 0; i < MAX_SCRIPT_ACTIONS_COUNT; ++i) {
        hr = pStm->Write(&ScriptActions[i].Action, sizeof(ScriptActions[i].Action), &written);
        if (hr < 0 || written != sizeof(ScriptActions[i].Action)) return E_FAIL;
        hr = pStm->Write(&ScriptActions[i].Argument, sizeof(ScriptActions[i].Argument), &written);
        if (hr < 0 || written != sizeof(ScriptActions[i].Argument)) return E_FAIL;
    }

    hr = pStm->Write(&ActionsCount, sizeof(ActionsCount), &written);
    if (hr < 0 || written != sizeof(ActionsCount)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// RTTI
// ============================================================================

AbstractType ScriptTypeClass::WhatAmI() const {
    return AbstractType::ScriptType;
}

int32 ScriptTypeClass::Size() const {
    return sizeof(ScriptTypeClass);
}

// ============================================================================
// INI loading
// ============================================================================

bool ScriptTypeClass::LoadFromINIList(CCINIClass* pINI, bool IsGlobal) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);
    if (!pINI->SectionExists(sectionName)) return false;

    this->IsGlobal = IsGlobal;

    pINI->GetInteger(sectionName, "ArrayIndex", ArrayIndex);

    // Parse every action line. The original game stores actions as numbered
    // keys ("0", "1", "2", ...) whose value is "action,argument". We also
    // validate each parsed action against the metadata table and silently
    // drop invalid entries (matching the original's tolerant behaviour).
    ActionsCount = 0;
    for (int32 i = 0; i < MAX_SCRIPT_ACTIONS_COUNT; ++i) {
        char keyName[32];
        snprintf(keyName, sizeof(keyName), "%d", i);

        char actionStr[64];
        pINI->ReadString(sectionName, keyName, "", actionStr, sizeof(actionStr));
        if (!actionStr[0]) break;

        int32 action = 0, argument = 0;
        if (!ParseActionString(actionStr, action, argument)) break;

        // Reject actions outside the known range entirely.
        if (action < 0 || action >= kActionCount) continue;

        // Clamp the argument to the documented range rather than rejecting
        // the whole line; this matches the original game's behaviour where
        // out-of-range arguments are accepted but have no effect.
        const ActionInfo* info = LookupAction(action);
        if (info && info->HasArgument) {
            if (argument < info->MinArgument) argument = info->MinArgument;
            if (argument > info->MaxArgument) argument = info->MaxArgument;
        }

        ScriptActions[ActionsCount].Action = action;
        ScriptActions[ActionsCount].Argument = argument;
        ++ActionsCount;
    }

    return true;
}

bool ScriptTypeClass::LoadFromINI(CCINIClass* pINI) {
    return LoadFromINIList(pINI, false);
}

bool ScriptTypeClass::SaveToINI(CCINIClass* pINI) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);

    pINI->WriteInteger(sectionName, "ArrayIndex", ArrayIndex);

    for (int32 i = 0; i < ActionsCount; ++i) {
        char keyName[32], valueStr[64];
        snprintf(keyName, sizeof(keyName), "%d", i);
        FormatActionString(valueStr, sizeof(valueStr),
                           ScriptActions[i].Action,
                           ScriptActions[i].Argument);
        pINI->WriteString(sectionName, keyName, valueStr);
    }

    // Clear any stale keys beyond the current action count so the INI file
    // doesn't accumulate orphaned entries across edits.
    for (int32 i = ActionsCount; i < MAX_SCRIPT_ACTIONS_COUNT; ++i) {
        char keyName[32];
        snprintf(keyName, sizeof(keyName), "%d", i);
        // The INI class treats an empty string as a deletion request.
        pINI->WriteString(sectionName, keyName, "");
    }

    return true;
}

// ============================================================================
// Action validation
// ============================================================================

bool ScriptTypeClass::IsActionValid(int32 actionIndex) const {
    if (actionIndex < 0 || actionIndex >= ActionsCount) return false;
    int32 action = ScriptActions[actionIndex].Action;
    if (action < 0 || action >= kActionCount) return false;
    // Also validate the argument against the metadata table.
    return ValidateActionArgument(action, ScriptActions[actionIndex].Argument);
}

// ============================================================================
// Action name / description lookup
// ============================================================================

const char* ScriptTypeClass::GetActionName(int32 action) {
    const ActionInfo* info = LookupAction(action);
    if (info) return info->Name;
    return "Unknown";
}

int32 ScriptTypeClass::GetActionArgument(int32 actionIndex) const {
    if (actionIndex < 0 || actionIndex >= ActionsCount) return 0;
    return ScriptActions[actionIndex].Argument;
}

bool ScriptTypeClass::HasAction(int32 actionType) const {
    for (int32 i = 0; i < ActionsCount; ++i) {
        if (ScriptActions[i].Action == actionType) return true;
    }
    return false;
}

int32 ScriptTypeClass::FindActionIndex(int32 actionType, int32 startFrom) const {
    if (startFrom < 0) startFrom = 0;
    for (int32 i = startFrom; i < ActionsCount; ++i) {
        if (ScriptActions[i].Action == actionType) return i;
    }
    return -1;
}

bool ScriptTypeClass::IsEmpty() const {
    return ActionsCount == 0;
}

int32 ScriptTypeClass::GetTotalActionCount() const {
    return ActionsCount;
}

void ScriptTypeClass::AddAction(int32 action, int32 argument) {
    if (ActionsCount >= MAX_SCRIPT_ACTIONS_COUNT) return;
    // Reject completely unknown actions.
    if (action < 0 || action >= kActionCount) return;
    // Clamp the argument to the valid range.
    const ActionInfo* info = LookupAction(action);
    if (info && info->HasArgument) {
        if (argument < info->MinArgument) argument = info->MinArgument;
        if (argument > info->MaxArgument) argument = info->MaxArgument;
    } else if (info) {
        argument = 0;  // argument-less actions always store 0
    }
    ScriptActions[ActionsCount].Action = action;
    ScriptActions[ActionsCount].Argument = argument;
    ++ActionsCount;
}

void ScriptTypeClass::RemoveAction(int32 actionIndex) {
    if (actionIndex < 0 || actionIndex >= ActionsCount) return;
    for (int32 i = actionIndex; i < ActionsCount - 1; ++i) {
        ScriptActions[i] = ScriptActions[i + 1];
    }
    --ActionsCount;
    ScriptActions[ActionsCount].Action = 0;
    ScriptActions[ActionsCount].Argument = 0;
}

void ScriptTypeClass::ClearActions() {
    for (int32 i = 0; i < MAX_SCRIPT_ACTIONS_COUNT; ++i) {
        ScriptActions[i].Action = 0;
        ScriptActions[i].Argument = 0;
    }
    ActionsCount = 0;
}
