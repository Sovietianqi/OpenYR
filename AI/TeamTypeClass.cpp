#include "TeamTypeClass.h"
#include "ScriptTypeClass.h"
#include "TaskForceClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Rules/RulesClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// TeamTypeClass.cpp - AI team type implementation
// ============================================================================
// A TeamTypeClass binds a TaskForceClass (unit composition) to a
// ScriptTypeClass (behaviour script) along with build limits, recruitment
// flags and difficulty filters. The original game reads team types from the
// [TeamTypes] INI block: each ID has its own section containing keys like
// "TaskForce", "Script", "Max", "Group", "VeteransLevel", etc.
//
// This file implements:
//   * Static registry (Array) management and name-based lookup
//   * Construction / destruction with full member initialisation
//   * Binary stream persistence (Load / Save) of all team-type fields
//   * INI parsing that resolves task-force and script names to pointers
//   * CRC computation for multiplayer sync verification
//   * Accessor methods for all fields
// ============================================================================

DynamicVectorClass<TeamTypeClass*>* TeamTypeClass::Array = nullptr;

// ----------------------------------------------------------------------------
// Find - locate a team type by its (case-insensitive) ID.
// ----------------------------------------------------------------------------
TeamTypeClass* TeamTypeClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        TeamTypeClass* item = Array->GetItem(i);
        if (item && !_strcmpi(item->ID, pID)) {
            return item;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// FindOrAllocate - return an existing team type or allocate a new one.
// ----------------------------------------------------------------------------
TeamTypeClass* TeamTypeClass::FindOrAllocate(const char* pID) {
    if (!pID || !pID[0]) return nullptr;
    if (!_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;

    TeamTypeClass* found = Find(pID);
    if (found) return found;

    TeamTypeClass* newItem = GameCreate<TeamTypeClass>(pID);
    if (newItem && Array) {
        Array->Add(newItem);
    }
    return newItem;
}

// ----------------------------------------------------------------------------
// GetCount - number of registered team types.
// ----------------------------------------------------------------------------
int32 TeamTypeClass::GetCount() {
    return Array ? Array->Count : 0;
}

// ============================================================================
// Construction / destruction
// ============================================================================

TeamTypeClass::TeamTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID), ScriptType(nullptr), TaskForce(nullptr),
      Max(1), Autocreate(false), Full(false), AreMembersRecruitable(false),
      Annoyance(0), Suicide(false), Loadable(false), Prebuild(false),
      Grouping(-1), TransportsReturn(false), VeteransLevel(0),
      Priority(0), OriginHouseIndex(-1), ReplayScriptType(nullptr),
      Aggressive(true), UseSibling(false), Easy(true), Normal(true),
      Hard(true) {
}

TeamTypeClass::~TeamTypeClass() {
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT TeamTypeClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = 0x54454D54;   // 'TEMT' sentinel for TeamTypeClass
    pClassID->Data2 = 0x5459;
    pClassID->Data3 = 0x5045;
    for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0x54;
    return S_OK;
}

// ----------------------------------------------------------------------------
// Load - read the team type's persistent state from a binary stream.
// ----------------------------------------------------------------------------
HRESULT TeamTypeClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    char idBuf[0x18];
    hr = pStm->Read(idBuf, sizeof(idBuf), &read);
    if (hr < 0 || read != sizeof(idBuf)) return E_FAIL;
    std::memcpy(ID, idBuf, sizeof(ID));
    ID[sizeof(ID) - 1] = '\0';

    // Read scalar fields
    struct {
        int32 Max;
        int32 Grouping;
        int32 OriginHouseIndex;
        int32 Annoyance;
        int32 Priority;
        int32 VeteransLevel;
    } scalars;

    hr = pStm->Read(&scalars, sizeof(scalars), &read);
    if (hr < 0 || read != sizeof(scalars)) return E_FAIL;
    Max = scalars.Max;
    Grouping = scalars.Grouping;
    OriginHouseIndex = scalars.OriginHouseIndex;
    Annoyance = scalars.Annoyance;
    Priority = scalars.Priority;
    VeteransLevel = scalars.VeteransLevel;

    // Read boolean flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    Autocreate          = (flags & 0x0001) != 0;
    Full                = (flags & 0x0002) != 0;
    AreMembersRecruitable = (flags & 0x0004) != 0;
    Suicide             = (flags & 0x0008) != 0;
    Loadable            = (flags & 0x0010) != 0;
    Prebuild            = (flags & 0x0020) != 0;
    TransportsReturn    = (flags & 0x0040) != 0;
    Aggressive          = (flags & 0x0080) != 0;
    UseSibling          = (flags & 0x0100) != 0;
    Easy                = (flags & 0x0200) != 0;
    Normal              = (flags & 0x0400) != 0;
    Hard                = (flags & 0x0800) != 0;

    // Read linked type names and resolve them
    char scriptName[0x18], replayScriptName[0x18], taskForceName[0x18];
    hr = pStm->Read(scriptName, sizeof(scriptName), &read);
    if (hr < 0 || read != sizeof(scriptName)) return E_FAIL;
    scriptName[sizeof(scriptName) - 1] = '\0';

    hr = pStm->Read(replayScriptName, sizeof(replayScriptName), &read);
    if (hr < 0 || read != sizeof(replayScriptName)) return E_FAIL;
    replayScriptName[sizeof(replayScriptName) - 1] = '\0';

    hr = pStm->Read(taskForceName, sizeof(taskForceName), &read);
    if (hr < 0 || read != sizeof(taskForceName)) return E_FAIL;
    taskForceName[sizeof(taskForceName) - 1] = '\0';

    ScriptType = scriptName[0] ? ScriptTypeClass::Find(scriptName) : nullptr;
    ReplayScriptType = replayScriptName[0] ? ScriptTypeClass::Find(replayScriptName) : nullptr;
    TaskForce = taskForceName[0] ? TaskForceClass::Find(taskForceName) : nullptr;

    return S_OK;
}

// ----------------------------------------------------------------------------
// Save - write the team type's persistent state to a binary stream.
// ----------------------------------------------------------------------------
HRESULT TeamTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    hr = pStm->Write(ID, sizeof(ID), &written);
    if (hr < 0 || written != sizeof(ID)) return E_FAIL;

    struct {
        int32 Max;
        int32 Grouping;
        int32 OriginHouseIndex;
        int32 Annoyance;
        int32 Priority;
        int32 VeteransLevel;
    } scalars;
    scalars.Max = Max;
    scalars.Grouping = Grouping;
    scalars.OriginHouseIndex = OriginHouseIndex;
    scalars.Annoyance = Annoyance;
    scalars.Priority = Priority;
    scalars.VeteransLevel = VeteransLevel;

    hr = pStm->Write(&scalars, sizeof(scalars), &written);
    if (hr < 0 || written != sizeof(scalars)) return E_FAIL;

    uint32 flags = 0;
    if (Autocreate)          flags |= 0x0001;
    if (Full)                flags |= 0x0002;
    if (AreMembersRecruitable) flags |= 0x0004;
    if (Suicide)             flags |= 0x0008;
    if (Loadable)            flags |= 0x0010;
    if (Prebuild)            flags |= 0x0020;
    if (TransportsReturn)    flags |= 0x0040;
    if (Aggressive)          flags |= 0x0080;
    if (UseSibling)          flags |= 0x0100;
    if (Easy)                flags |= 0x0200;
    if (Normal)              flags |= 0x0400;
    if (Hard)                flags |= 0x0800;

    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    char scriptName[0x18], replayScriptName[0x18], taskForceName[0x18];
    std::memset(scriptName, 0, sizeof(scriptName));
    std::memset(replayScriptName, 0, sizeof(replayScriptName));
    std::memset(taskForceName, 0, sizeof(taskForceName));

    if (ScriptType && ScriptType->ID) {
        int32 j = 0;
        while (ScriptType->ID[j] && j < static_cast<int32>(sizeof(scriptName)) - 1) {
            scriptName[j] = ScriptType->ID[j]; ++j;
        }
    }
    if (ReplayScriptType && ReplayScriptType->ID) {
        int32 j = 0;
        while (ReplayScriptType->ID[j] && j < static_cast<int32>(sizeof(replayScriptName)) - 1) {
            replayScriptName[j] = ReplayScriptType->ID[j]; ++j;
        }
    }
    if (TaskForce && TaskForce->ID) {
        int32 j = 0;
        while (TaskForce->ID[j] && j < static_cast<int32>(sizeof(taskForceName)) - 1) {
            taskForceName[j] = TaskForce->ID[j]; ++j;
        }
    }

    hr = pStm->Write(scriptName, sizeof(scriptName), &written);
    if (hr < 0 || written != sizeof(scriptName)) return E_FAIL;
    hr = pStm->Write(replayScriptName, sizeof(replayScriptName), &written);
    if (hr < 0 || written != sizeof(replayScriptName)) return E_FAIL;
    hr = pStm->Write(taskForceName, sizeof(taskForceName), &written);
    if (hr < 0 || written != sizeof(taskForceName)) return E_FAIL;

    if (fClearDirty) {
        Dirty = false;
    }
    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType TeamTypeClass::WhatAmI() const {
    return AbstractType::TeamType;
}

int32 TeamTypeClass::Size() const {
    return sizeof(TeamTypeClass);
}

// ============================================================================
// INI loading
//
// The original game stores team types in [TeamTypes] as a list of IDs. Each
// ID has its own section with keys: TaskForce, Script, Max, Group,
// VeteransLevel, Annoyance, Priority, and numerous Yes/No flags.
// ============================================================================

bool TeamTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);
    if (!pINI->SectionExists(sectionName)) return false;

    // Read display name
    char nameBuf[0x31];
    pINI->ReadString(sectionName, "Name", "", nameBuf, sizeof(nameBuf));
    if (nameBuf[0]) {
        int32 i = 0;
        while (nameBuf[i] && i < static_cast<int32>(sizeof(Name)) - 1) {
            Name[i] = nameBuf[i];
            ++i;
        }
        Name[i] = '\0';
    }

    // Resolve TaskForce reference
    char taskForceName[0x18];
    pINI->ReadString(sectionName, "TaskForce", "", taskForceName, sizeof(taskForceName));
    if (taskForceName[0] && _strcmpi(taskForceName, "<none>") != 0) {
        TaskForce = TaskForceClass::Find(taskForceName);
    } else {
        TaskForce = nullptr;
    }

    // Resolve Script reference
    char scriptName[0x18];
    pINI->ReadString(sectionName, "Script", "", scriptName, sizeof(scriptName));
    if (scriptName[0] && _strcmpi(scriptName, "<none>") != 0) {
        ScriptType = ScriptTypeClass::Find(scriptName);
    } else {
        ScriptType = nullptr;
    }

    // Resolve replay script reference
    char replayScriptName[0x18];
    pINI->ReadString(sectionName, "ReplayScript", "", replayScriptName, sizeof(replayScriptName));
    if (replayScriptName[0] && _strcmpi(replayScriptName, "<none>") != 0) {
        ReplayScriptType = ScriptTypeClass::Find(replayScriptName);
    } else {
        ReplayScriptType = nullptr;
    }

    // Read scalar fields
    Max = pINI->ReadInteger(sectionName, "Max", Max);
    Grouping = pINI->ReadInteger(sectionName, "Group", Grouping);
    VeteransLevel = pINI->ReadInteger(sectionName, "VeteransLevel", VeteransLevel);
    Annoyance = pINI->ReadInteger(sectionName, "Annoyance", Annoyance);
    Priority = pINI->ReadInteger(sectionName, "Priority", Priority);
    OriginHouseIndex = pINI->ReadInteger(sectionName, "OriginHouseIndex", OriginHouseIndex);

    // Read boolean flags
    Autocreate = pINI->ReadBool(sectionName, "Autocreate", Autocreate);
    Full = pINI->ReadBool(sectionName, "Full", Full);
    AreMembersRecruitable = pINI->ReadBool(sectionName, "AreMembersRecruitable", AreMembersRecruitable);
    Suicide = pINI->ReadBool(sectionName, "Suicide", Suicide);
    Loadable = pINI->ReadBool(sectionName, "Loadable", Loadable);
    Prebuild = pINI->ReadBool(sectionName, "Prebuild", Prebuild);
    TransportsReturn = pINI->ReadBool(sectionName, "TransportsReturn", TransportsReturn);
    Aggressive = pINI->ReadBool(sectionName, "Aggressive", Aggressive);
    UseSibling = pINI->ReadBool(sectionName, "UseSibling", UseSibling);
    Easy = pINI->ReadBool(sectionName, "Easy", Easy);
    Normal = pINI->ReadBool(sectionName, "Normal", Normal);
    Hard = pINI->ReadBool(sectionName, "Hard", Hard);

    return true;
}

// ============================================================================
// CRC computation
// ============================================================================

int32 TeamTypeClass::GetCRC() const {
    CRCEngine crc;
    crc.AddData(ID, static_cast<int32>(sizeof(ID)));
    crc.AddData(&Max, sizeof(Max));
    crc.AddData(&Grouping, sizeof(Grouping));
    crc.AddData(&OriginHouseIndex, sizeof(OriginHouseIndex));
    crc.AddData(&Annoyance, sizeof(Annoyance));
    crc.AddData(&Priority, sizeof(Priority));
    crc.AddData(&VeteransLevel, sizeof(VeteransLevel));

    uint32 flags = 0;
    if (Autocreate)          flags |= 0x0001;
    if (Full)                flags |= 0x0002;
    if (AreMembersRecruitable) flags |= 0x0004;
    if (Suicide)             flags |= 0x0008;
    if (Loadable)            flags |= 0x0010;
    if (Prebuild)            flags |= 0x0020;
    if (TransportsReturn)    flags |= 0x0040;
    if (Aggressive)          flags |= 0x0080;
    if (UseSibling)          flags |= 0x0100;
    if (Easy)                flags |= 0x0200;
    if (Normal)              flags |= 0x0400;
    if (Hard)                flags |= 0x0800;
    crc.AddData(&flags, sizeof(flags));

    DWORD tfID = TaskForce ? TaskForce->Fetch_ID() : 0;
    DWORD scID = ScriptType ? ScriptType->Fetch_ID() : 0;
    DWORD rsID = ReplayScriptType ? ReplayScriptType->Fetch_ID() : 0;
    crc.AddData(&tfID, sizeof(tfID));
    crc.AddData(&scID, sizeof(scID));
    crc.AddData(&rsID, sizeof(rsID));

    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// Accessors
// ============================================================================

TaskForceClass* TeamTypeClass::Get_TaskForce() const {
    return TaskForce;
}

ScriptTypeClass* TeamTypeClass::Get_ScriptType() const {
    return ScriptType;
}

ScriptTypeClass* TeamTypeClass::Get_ReplayScriptType() const {
    return ReplayScriptType;
}

// ----------------------------------------------------------------------------
// Get_Initial_Threat - the initial threat rating for teams of this type.
// The original game computes this from the Annoyance and Priority fields.
// ----------------------------------------------------------------------------
int32 TeamTypeClass::Get_Initial_Threat() const {
    // Threat is a weighted combination of Annoyance and Priority.
    // Higher annoyance means the AI considers this team more dangerous.
    int32 threat = Annoyance;
    if (Priority > 0) {
        threat += Priority / 2;
    }
    if (threat < 0) threat = 0;
    return threat;
}

int32 TeamTypeClass::Get_Group() const {
    return Grouping;
}

int32 TeamTypeClass::Get_Max_Count() const {
    return Max;
}

int32 TeamTypeClass::Get_Priority() const {
    return Priority;
}

int32 TeamTypeClass::Get_Veterans_Level() const {
    return VeteransLevel;
}

// ----------------------------------------------------------------------------
// Is_Valid - a team type is valid if it has both a task force and a script.
// ----------------------------------------------------------------------------
bool TeamTypeClass::Is_Valid() const {
    if (!TaskForce) return false;
    if (!ScriptType) return false;
    if (Max <= 0) return false;
    return true;
}

bool TeamTypeClass::Is_Recruiter() const {
    return AreMembersRecruitable;
}

bool TeamTypeClass::Is_Autocreate() const {
    return Autocreate;
}

bool TeamTypeClass::Is_Full() const {
    return Full;
}

bool TeamTypeClass::Is_Suicide() const {
    return Suicide;
}

bool TeamTypeClass::Is_Loadable() const {
    return Loadable;
}

bool TeamTypeClass::Is_Prebuild() const {
    return Prebuild;
}

bool TeamTypeClass::Is_Aggressive() const {
    return Aggressive;
}

bool TeamTypeClass::Is_Transports_Return() const {
    return TransportsReturn;
}

// ----------------------------------------------------------------------------
// Is_Allowed_Difficulty - check if this team type is enabled for the given
// difficulty level (0=Easy, 1=Normal, 2=Hard).
// ----------------------------------------------------------------------------
bool TeamTypeClass::Is_Allowed_Difficulty(int32 difficulty) const {
    switch (difficulty) {
        case 0:  return Easy;
        case 1:  return Normal;
        case 2:  return Hard;
        default: return false;
    }
}
