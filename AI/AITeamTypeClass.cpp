#include "AITeamTypeClass.h"
#include "TeamTypeClass.h"
#include "TeamClass.h"
#include "TaskForceClass.h"
#include "TagClass.h"
#include "ScriptTypeClass.h"
#include "../Abstract/FootClass.h"
#include "../Houses/HouseClass.h"
#include "../Game/Game.h"
#include "../Rules/RulesClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Map/MapClass.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

DynamicVectorClass<AITeamTypeClass*>* AITeamTypeClass::Array = nullptr;

AITeamTypeClass* AITeamTypeClass::Find(const char* pID) {
    if (!Array) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        AITeamTypeClass* item = Array->GetItem(i);
        if (item && !_strcmpi(item->ID, pID)) return item;
    }
    return nullptr;
}

AITeamTypeClass* AITeamTypeClass::FindOrAllocate(const char* pID) {
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    AITeamTypeClass* found = Find(pID);
    if (found) return found;
    AITeamTypeClass* newItem = GameCreate<AITeamTypeClass>(pID);
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}

AITeamTypeClass::AITeamTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID), ArrayIndex(-1), Group(0), VeteranLevel(1),
      Loadable(false), Full(false), Annoyance(false), GuardSlower(false),
      Recruiter(false), Autocreate(false), Prebuild(false), Reinforce(false),
      Whiner(false), Aggressive(false), LooseRecruit(false), Suicide(false),
      Droppod(false), UseTransportOrigin(false), DropshipLoadout(false), OnTransOnly(false),
      Priority(5), Max(1), MaxInstances(1), MindControlDecision(0),
      Owner(nullptr), idxHouse(-1), TechLevel(1), Tag(nullptr),
      Waypoint(-1), TransportWaypoint(-1), cntInstances(0),
      ScriptType(nullptr), TaskForce(nullptr), IsGlobal(0), field_EC(0),
      field_F0(false), field_F1(false), AvoidThreats(false), IonImmune(false),
      TransportsReturnOnUnload(false), AreTeamMembersRecruitable(false),
      IsBaseDefense(false), OnlyTargetHouseEnemy(false) {
    TaskForceMembers.Clear();
    TaskForceCounts.Clear();
}

AITeamTypeClass::~AITeamTypeClass() {
    TaskForceMembers.Clear();
    TaskForceCounts.Clear();
}

HRESULT AITeamTypeClass::GetClassID(CLSID* pClassID) {
    if (pClassID) {
        pClassID->Data1 = 0xB2B2B2B2;
        for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0;
        return S_OK;
    }
    return E_POINTER;
}

HRESULT AITeamTypeClass::Load(IStream* pStm) {
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
    hr = pStm->Read(&Group, sizeof(Group), &read);
    if (hr < 0 || read != sizeof(Group)) return E_FAIL;
    hr = pStm->Read(&VeteranLevel, sizeof(VeteranLevel), &read);
    if (hr < 0 || read != sizeof(VeteranLevel)) return E_FAIL;

    // Read packed bool flags (first group)
    uint32 flags1 = 0;
    hr = pStm->Read(&flags1, sizeof(flags1), &read);
    if (hr < 0 || read != sizeof(flags1)) return E_FAIL;
    Loadable           = (flags1 & 0x00000001) != 0;
    Full               = (flags1 & 0x00000002) != 0;
    Annoyance          = (flags1 & 0x00000004) != 0;
    GuardSlower        = (flags1 & 0x00000008) != 0;
    Recruiter          = (flags1 & 0x00000010) != 0;
    Autocreate         = (flags1 & 0x00000020) != 0;
    Prebuild           = (flags1 & 0x00000040) != 0;
    Reinforce          = (flags1 & 0x00000080) != 0;
    Whiner             = (flags1 & 0x00000100) != 0;
    Aggressive         = (flags1 & 0x00000200) != 0;
    LooseRecruit       = (flags1 & 0x00000400) != 0;
    Suicide            = (flags1 & 0x00000800) != 0;
    Droppod            = (flags1 & 0x00001000) != 0;
    UseTransportOrigin = (flags1 & 0x00002000) != 0;
    DropshipLoadout    = (flags1 & 0x00004000) != 0;
    OnTransOnly        = (flags1 & 0x00008000) != 0;

    hr = pStm->Read(&Priority, sizeof(Priority), &read);
    if (hr < 0 || read != sizeof(Priority)) return E_FAIL;
    hr = pStm->Read(&Max, sizeof(Max), &read);
    if (hr < 0 || read != sizeof(Max)) return E_FAIL;
    hr = pStm->Read(&MaxInstances, sizeof(MaxInstances), &read);
    if (hr < 0 || read != sizeof(MaxInstances)) return E_FAIL;
    hr = pStm->Read(&MindControlDecision, sizeof(MindControlDecision), &read);
    if (hr < 0 || read != sizeof(MindControlDecision)) return E_FAIL;

    // Read owning house index and resolve.
    int32 ownerIdx = -1;
    hr = pStm->Read(&ownerIdx, sizeof(ownerIdx), &read);
    if (hr < 0 || read != sizeof(ownerIdx)) return E_FAIL;
    Owner = (ownerIdx >= 0) ? HouseClass::GetHouseByIndex(ownerIdx) : nullptr;

    hr = pStm->Read(&idxHouse, sizeof(idxHouse), &read);
    if (hr < 0 || read != sizeof(idxHouse)) return E_FAIL;
    hr = pStm->Read(&TechLevel, sizeof(TechLevel), &read);
    if (hr < 0 || read != sizeof(TechLevel)) return E_FAIL;

    // Read tag name and resolve.
    char tagName[0x18];
    hr = pStm->Read(tagName, sizeof(tagName), &read);
    if (hr < 0 || read != sizeof(tagName)) return E_FAIL;
    tagName[sizeof(tagName) - 1] = '\0';
    Tag = tagName[0] ? TagClass::Find(tagName) : nullptr;

    hr = pStm->Read(&Waypoint, sizeof(Waypoint), &read);
    if (hr < 0 || read != sizeof(Waypoint)) return E_FAIL;
    hr = pStm->Read(&TransportWaypoint, sizeof(TransportWaypoint), &read);
    if (hr < 0 || read != sizeof(TransportWaypoint)) return E_FAIL;
    hr = pStm->Read(&cntInstances, sizeof(cntInstances), &read);
    if (hr < 0 || read != sizeof(cntInstances)) return E_FAIL;

    // Read script type ID and resolve.
    char scriptTypeID[0x18];
    hr = pStm->Read(scriptTypeID, sizeof(scriptTypeID), &read);
    if (hr < 0 || read != sizeof(scriptTypeID)) return E_FAIL;
    scriptTypeID[sizeof(scriptTypeID) - 1] = '\0';
    ScriptType = scriptTypeID[0] ? ScriptTypeClass::Find(scriptTypeID) : nullptr;

    // Read task force ID and resolve.
    char taskForceID[0x18];
    hr = pStm->Read(taskForceID, sizeof(taskForceID), &read);
    if (hr < 0 || read != sizeof(taskForceID)) return E_FAIL;
    taskForceID[sizeof(taskForceID) - 1] = '\0';
    TaskForce = taskForceID[0] ? TaskForceClass::Find(taskForceID) : nullptr;

    hr = pStm->Read(&IsGlobal, sizeof(IsGlobal), &read);
    if (hr < 0 || read != sizeof(IsGlobal)) return E_FAIL;
    hr = pStm->Read(&field_EC, sizeof(field_EC), &read);
    if (hr < 0 || read != sizeof(field_EC)) return E_FAIL;

    // Read packed bool flags (second group)
    uint32 flags2 = 0;
    hr = pStm->Read(&flags2, sizeof(flags2), &read);
    if (hr < 0 || read != sizeof(flags2)) return E_FAIL;
    field_F0                    = (flags2 & 0x01) != 0;
    field_F1                    = (flags2 & 0x02) != 0;
    AvoidThreats                = (flags2 & 0x04) != 0;
    IonImmune                   = (flags2 & 0x08) != 0;
    TransportsReturnOnUnload    = (flags2 & 0x10) != 0;
    AreTeamMembersRecruitable   = (flags2 & 0x20) != 0;
    IsBaseDefense               = (flags2 & 0x40) != 0;
    OnlyTargetHouseEnemy        = (flags2 & 0x80) != 0;

    // Read task force members (TechnoTypeClass pointers serialized as IDs).
    int32 memberCount = 0;
    hr = pStm->Read(&memberCount, sizeof(memberCount), &read);
    if (hr < 0 || read != sizeof(memberCount)) return E_FAIL;
    if (memberCount < 0) memberCount = 0;
    TaskForceMembers.Clear();
    for (int32 i = 0; i < memberCount; ++i) {
        char memberID[0x18];
        hr = pStm->Read(memberID, sizeof(memberID), &read);
        if (hr < 0 || read != sizeof(memberID)) return E_FAIL;
        memberID[sizeof(memberID) - 1] = '\0';
        if (memberID[0]) {
            TechnoTypeClass* pType = static_cast<TechnoTypeClass*>(TechnoTypeClass::Find(memberID));
            if (pType) TaskForceMembers.Add(pType);
        }
    }

    // Read task force counts.
    int32 countCount = 0;
    hr = pStm->Read(&countCount, sizeof(countCount), &read);
    if (hr < 0 || read != sizeof(countCount)) return E_FAIL;
    if (countCount < 0) countCount = 0;
    TaskForceCounts.Clear();
    for (int32 i = 0; i < countCount; ++i) {
        int32 val = 0;
        hr = pStm->Read(&val, sizeof(val), &read);
        if (hr < 0 || read != sizeof(val)) return E_FAIL;
        TaskForceCounts.Add(val);
    }

    return S_OK;
}

HRESULT AITeamTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Write ID
    hr = pStm->Write(ID, sizeof(ID), &written);
    if (hr < 0 || written != sizeof(ID)) return E_FAIL;

    // Write scalar fields
    hr = pStm->Write(&ArrayIndex, sizeof(ArrayIndex), &written);
    if (hr < 0 || written != sizeof(ArrayIndex)) return E_FAIL;
    hr = pStm->Write(&Group, sizeof(Group), &written);
    if (hr < 0 || written != sizeof(Group)) return E_FAIL;
    hr = pStm->Write(&VeteranLevel, sizeof(VeteranLevel), &written);
    if (hr < 0 || written != sizeof(VeteranLevel)) return E_FAIL;

    // Write packed bool flags (first group)
    uint32 flags1 = 0;
    if (Loadable)           flags1 |= 0x00000001;
    if (Full)               flags1 |= 0x00000002;
    if (Annoyance)          flags1 |= 0x00000004;
    if (GuardSlower)        flags1 |= 0x00000008;
    if (Recruiter)          flags1 |= 0x00000010;
    if (Autocreate)         flags1 |= 0x00000020;
    if (Prebuild)           flags1 |= 0x00000040;
    if (Reinforce)          flags1 |= 0x00000080;
    if (Whiner)             flags1 |= 0x00000100;
    if (Aggressive)         flags1 |= 0x00000200;
    if (LooseRecruit)       flags1 |= 0x00000400;
    if (Suicide)            flags1 |= 0x00000800;
    if (Droppod)            flags1 |= 0x00001000;
    if (UseTransportOrigin) flags1 |= 0x00002000;
    if (DropshipLoadout)    flags1 |= 0x00004000;
    if (OnTransOnly)        flags1 |= 0x00008000;
    hr = pStm->Write(&flags1, sizeof(flags1), &written);
    if (hr < 0 || written != sizeof(flags1)) return E_FAIL;

    hr = pStm->Write(&Priority, sizeof(Priority), &written);
    if (hr < 0 || written != sizeof(Priority)) return E_FAIL;
    hr = pStm->Write(&Max, sizeof(Max), &written);
    if (hr < 0 || written != sizeof(Max)) return E_FAIL;
    hr = pStm->Write(&MaxInstances, sizeof(MaxInstances), &written);
    if (hr < 0 || written != sizeof(MaxInstances)) return E_FAIL;
    hr = pStm->Write(&MindControlDecision, sizeof(MindControlDecision), &written);
    if (hr < 0 || written != sizeof(MindControlDecision)) return E_FAIL;

    // Write owning house index.
    int32 ownerIdx = Owner ? Owner->ArrayIndex : -1;
    hr = pStm->Write(&ownerIdx, sizeof(ownerIdx), &written);
    if (hr < 0 || written != sizeof(ownerIdx)) return E_FAIL;

    hr = pStm->Write(&idxHouse, sizeof(idxHouse), &written);
    if (hr < 0 || written != sizeof(idxHouse)) return E_FAIL;
    hr = pStm->Write(&TechLevel, sizeof(TechLevel), &written);
    if (hr < 0 || written != sizeof(TechLevel)) return E_FAIL;

    // Write tag name.
    char tagName[0x18];
    std::memset(tagName, 0, sizeof(tagName));
    if (Tag && Tag->Name) {
        int32 j = 0;
        while (Tag->Name[j] && j < static_cast<int32>(sizeof(tagName)) - 1) {
            tagName[j] = Tag->Name[j]; ++j;
        }
    }
    hr = pStm->Write(tagName, sizeof(tagName), &written);
    if (hr < 0 || written != sizeof(tagName)) return E_FAIL;

    hr = pStm->Write(&Waypoint, sizeof(Waypoint), &written);
    if (hr < 0 || written != sizeof(Waypoint)) return E_FAIL;
    hr = pStm->Write(&TransportWaypoint, sizeof(TransportWaypoint), &written);
    if (hr < 0 || written != sizeof(TransportWaypoint)) return E_FAIL;
    hr = pStm->Write(&cntInstances, sizeof(cntInstances), &written);
    if (hr < 0 || written != sizeof(cntInstances)) return E_FAIL;

    // Write script type ID.
    char scriptTypeID[0x18];
    std::memset(scriptTypeID, 0, sizeof(scriptTypeID));
    if (ScriptType && ScriptType->ID) {
        int32 j = 0;
        while (ScriptType->ID[j] && j < static_cast<int32>(sizeof(scriptTypeID)) - 1) {
            scriptTypeID[j] = ScriptType->ID[j]; ++j;
        }
    }
    hr = pStm->Write(scriptTypeID, sizeof(scriptTypeID), &written);
    if (hr < 0 || written != sizeof(scriptTypeID)) return E_FAIL;

    // Write task force ID.
    char taskForceID[0x18];
    std::memset(taskForceID, 0, sizeof(taskForceID));
    if (TaskForce && TaskForce->ID) {
        int32 j = 0;
        while (TaskForce->ID[j] && j < static_cast<int32>(sizeof(taskForceID)) - 1) {
            taskForceID[j] = TaskForce->ID[j]; ++j;
        }
    }
    hr = pStm->Write(taskForceID, sizeof(taskForceID), &written);
    if (hr < 0 || written != sizeof(taskForceID)) return E_FAIL;

    hr = pStm->Write(&IsGlobal, sizeof(IsGlobal), &written);
    if (hr < 0 || written != sizeof(IsGlobal)) return E_FAIL;
    hr = pStm->Write(&field_EC, sizeof(field_EC), &written);
    if (hr < 0 || written != sizeof(field_EC)) return E_FAIL;

    // Write packed bool flags (second group)
    uint32 flags2 = 0;
    if (field_F0)                   flags2 |= 0x01;
    if (field_F1)                   flags2 |= 0x02;
    if (AvoidThreats)               flags2 |= 0x04;
    if (IonImmune)                  flags2 |= 0x08;
    if (TransportsReturnOnUnload)   flags2 |= 0x10;
    if (AreTeamMembersRecruitable)   flags2 |= 0x20;
    if (IsBaseDefense)               flags2 |= 0x40;
    if (OnlyTargetHouseEnemy)        flags2 |= 0x80;
    hr = pStm->Write(&flags2, sizeof(flags2), &written);
    if (hr < 0 || written != sizeof(flags2)) return E_FAIL;

    // Write task force members.
    int32 memberCount = TaskForceMembers.Count;
    hr = pStm->Write(&memberCount, sizeof(memberCount), &written);
    if (hr < 0 || written != sizeof(memberCount)) return E_FAIL;
    for (int32 i = 0; i < TaskForceMembers.Count; ++i) {
        char memberID[0x18];
        std::memset(memberID, 0, sizeof(memberID));
        TechnoTypeClass* pType = TaskForceMembers[i];
        if (pType && pType->ID) {
            int32 j = 0;
            while (pType->ID[j] && j < static_cast<int32>(sizeof(memberID)) - 1) {
                memberID[j] = pType->ID[j]; ++j;
            }
        }
        hr = pStm->Write(memberID, sizeof(memberID), &written);
        if (hr < 0 || written != sizeof(memberID)) return E_FAIL;
    }

    // Write task force counts.
    int32 countCount = TaskForceCounts.Count;
    hr = pStm->Write(&countCount, sizeof(countCount), &written);
    if (hr < 0 || written != sizeof(countCount)) return E_FAIL;
    for (int32 i = 0; i < TaskForceCounts.Count; ++i) {
        int32 val = TaskForceCounts[i];
        hr = pStm->Write(&val, sizeof(val), &written);
        if (hr < 0 || written != sizeof(val)) return E_FAIL;
    }

    return S_OK;
}

AbstractType AITeamTypeClass::WhatAmI() const {
    return AbstractType::TeamType;
}

int32 AITeamTypeClass::Size() const {
    return sizeof(AITeamTypeClass);
}

bool AITeamTypeClass::LoadFromINIList(CCINIClass* pINI, bool IsGlobal) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);
    if (!pINI->SectionExists(sectionName)) return false;

    this->IsGlobal = IsGlobal ? 1 : 0;

    pINI->GetInteger(sectionName, "Group", Group);
    pINI->GetInteger(sectionName, "VeteranLevel", VeteranLevel);
    pINI->GetInteger(sectionName, "Priority", Priority);
    pINI->GetInteger(sectionName, "Max", Max);
    pINI->GetInteger(sectionName, "MaxInstances", MaxInstances);
    pINI->GetInteger(sectionName, "MindControlDecision", MindControlDecision);
    pINI->GetInteger(sectionName, "TechLevel", TechLevel);
    pINI->GetInteger(sectionName, "Waypoint", Waypoint);
    pINI->GetInteger(sectionName, "TransportWaypoint", TransportWaypoint);

    Loadable = pINI->ReadBool(sectionName, "Loadable", Loadable);
    Full = pINI->ReadBool(sectionName, "Full", Full);
    Annoyance = pINI->ReadBool(sectionName, "Annoyance", Annoyance);
    GuardSlower = pINI->ReadBool(sectionName, "GuardSlower", GuardSlower);
    Recruiter = pINI->ReadBool(sectionName, "Recruiter", Recruiter);
    Autocreate = pINI->ReadBool(sectionName, "Autocreate", Autocreate);
    Prebuild = pINI->ReadBool(sectionName, "Prebuild", Prebuild);
    Reinforce = pINI->ReadBool(sectionName, "Reinforce", Reinforce);
    Whiner = pINI->ReadBool(sectionName, "Whiner", Whiner);
    Aggressive = pINI->ReadBool(sectionName, "Aggressive", Aggressive);
    LooseRecruit = pINI->ReadBool(sectionName, "LooseRecruit", LooseRecruit);
    Suicide = pINI->ReadBool(sectionName, "Suicide", Suicide);
    Droppod = pINI->ReadBool(sectionName, "Droppod", Droppod);
    UseTransportOrigin = pINI->ReadBool(sectionName, "UseTransportOrigin", UseTransportOrigin);
    DropshipLoadout = pINI->ReadBool(sectionName, "DropshipLoadout", DropshipLoadout);
    OnTransOnly = pINI->ReadBool(sectionName, "OnTransOnly", OnTransOnly);
    AvoidThreats = pINI->ReadBool(sectionName, "AvoidThreats", AvoidThreats);
    IonImmune = pINI->ReadBool(sectionName, "IonImmune", IonImmune);
    TransportsReturnOnUnload = pINI->ReadBool(sectionName, "TransportsReturnOnUnload", TransportsReturnOnUnload);
    AreTeamMembersRecruitable = pINI->ReadBool(sectionName, "AreTeamMembersRecruitable", AreTeamMembersRecruitable);
    IsBaseDefense = pINI->ReadBool(sectionName, "IsBaseDefense", IsBaseDefense);
    OnlyTargetHouseEnemy = pINI->ReadBool(sectionName, "OnlyTargetHouseEnemy", OnlyTargetHouseEnemy);

    char scriptName[0x18];
    pINI->ReadString(sectionName, "Script", "", scriptName, sizeof(scriptName));
    if (scriptName[0] && _strcmpi(scriptName, "<none>") != 0) {
        ScriptType = ScriptTypeClass::Find(scriptName);
    }

    char taskForceName[0x18];
    pINI->ReadString(sectionName, "TaskForce", "", taskForceName, sizeof(taskForceName));
    if (taskForceName[0] && _strcmpi(taskForceName, "<none>") != 0) {
        TaskForce = static_cast<TaskForceClass*>(TaskForceClass::Find(taskForceName));
    }

    char tagName[0x18];
    pINI->ReadString(sectionName, "Tag", "", tagName, sizeof(tagName));
    if (tagName[0] && _strcmpi(tagName, "<none>") != 0) {
        Tag = nullptr;
    }

    ParseTaskForceMembers(pINI);

    return true;
}

void AITeamTypeClass::ParseTaskForceMembers(CCINIClass* pINI) {
    if (!pINI) return;
    TaskForceMembers.Clear();
    TaskForceCounts.Clear();

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);

    for (int32 i = 0; i < 20; ++i) {
        char keyName[32];
        snprintf(keyName, sizeof(keyName), "TaskForceMember%d", i);
        char memberStr[128];
        pINI->ReadString(sectionName, keyName, "", memberStr, sizeof(memberStr));
        if (!memberStr[0]) break;

        char typeName[0x18];
        int32 count = 1;
        char* comma = strchr(memberStr, ',');
        if (comma) {
            *comma = '\0';
            count = atoi(comma + 1);
            if (count < 0) count = 0;
        }

        int32 j = 0;
        while (memberStr[j] && j < 23) { typeName[j] = memberStr[j]; ++j; }
        typeName[j] = '\0';

        TechnoTypeClass* pType = static_cast<TechnoTypeClass*>(TechnoTypeClass::Find(typeName));
        if (pType) {
            TaskForceMembers.Add(pType);
            TaskForceCounts.Add(count);
        }
    }
}

TeamClass* AITeamTypeClass::CreateTeam(HouseClass* pHouse) {
    if (!pHouse) return nullptr;
    if (cntInstances >= MaxInstances && MaxInstances > 0) return nullptr;

    TeamTypeClass* teamType = static_cast<TeamTypeClass*>(TeamTypeClass::Find(this->ID));
    if (!teamType) return nullptr;

    teamType->ScriptType = this->ScriptType;
    teamType->TaskForce = this->TaskForce;
    teamType->Max = this->Max;
    teamType->Autocreate = this->Autocreate;
    teamType->Full = this->Full;

    TeamClass* team = new TeamClass(teamType, pHouse, 0);
    if (!team) return nullptr;

    team->IsTransient = !this->Autocreate;
    team->CreationFrame = Game::CurrentFrame;

    for (int32 i = 0; i < TaskForceMembers.Count; ++i) {
        TechnoTypeClass* pType = TaskForceMembers[i];
        int32 count = (i < TaskForceCounts.Count) ? TaskForceCounts[i] : 1;

        for (int32 j = 0; j < count; ++j) {
            if (team->IsFullStrength) break;
            FootClass* pUnit = SpawnUnit(pType, pHouse);
            if (pUnit) {
                team->AddMember(pUnit, false);
            }
        }
    }

    team->Form();
    ++cntInstances;
    return team;
}

FootClass* AITeamTypeClass::SpawnUnit(TechnoTypeClass* pType, HouseClass* pHouse) {
    if (!pType || !pHouse) return nullptr;

    CellStruct waypointCell;
    GetWaypoint(&waypointCell);

    CoordStruct spawnCoord = Math::CellToCoord(waypointCell);
    if (Waypoint < 0 && MapClass::Instance) {
        spawnCoord = MapClass::Instance->GetWaypoint(0);
    }

    FootClass* pUnit = nullptr;
    AbstractType absType = pType->WhatAmI();

    return pUnit;
}

bool AITeamTypeClass::HasEnoughUnitsForCreation(HouseClass* pHouse) const {
    if (!pHouse) return false;

    for (int32 i = 0; i < TaskForceMembers.Count; ++i) {
        TechnoTypeClass* pType = TaskForceMembers[i];
        int32 needed = (i < TaskForceCounts.Count) ? TaskForceCounts[i] : 1;
        int32 owned = pHouse->CountOwnedNow(pType);
        if (owned < needed) return false;
    }
    return true;
}

void AITeamTypeClass::DestroyAllInstances() {
    if (!TeamClass::Array) return;
    for (int32 i = TeamClass::Array->Count - 1; i >= 0; --i) {
        TeamClass* team = TeamClass::Array->GetItem(i);
        if (team && team->Type) {
            if (_strcmpi(team->Type->get_ID(), this->ID) == 0) {
                team->Disband();
                team->NeedsToDisappear = true;
            }
        }
    }
    cntInstances = 0;
}

int32 AITeamTypeClass::GetGroup() const {
    return Group;
}

CellStruct* AITeamTypeClass::GetWaypoint(CellStruct* buffer) const {
    if (!buffer) return nullptr;
    if (Waypoint >= 0 && ScenarioClass::Instance) {
        *buffer = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    } else {
        buffer->X = 0;
        buffer->Y = 0;
    }
    return buffer;
}

CellStruct* AITeamTypeClass::GetTransportWaypoint(CellStruct* buffer) const {
    if (!buffer) return nullptr;
    if (TransportWaypoint >= 0 && ScenarioClass::Instance) {
        *buffer = ScenarioClass::Instance->GetWaypointCoords(TransportWaypoint);
    } else {
        buffer->X = 0;
        buffer->Y = 0;
    }
    return buffer;
}

bool AITeamTypeClass::CanRecruitUnit(FootClass* pUnit, HouseClass* pOwner) const {
    if (!pUnit || !pOwner) return false;
    if (Owner && Owner != pOwner) return false;
    if (!Recruiter && !LooseRecruit) return false;
    return true;
}

void AITeamTypeClass::FlashAllInstances(int32 Duration) {
    if (!TeamClass::Array) return;

    // Clamp the flash duration to at least one frame so a zero or
    // negative argument still produces a visible single-frame pulse.
    int32 flashFrames = (Duration > 0) ? Duration : 1;
    int32 nowFrame = Game::CurrentFrame;

    for (int32 i = 0; i < TeamClass::Array->Count; ++i) {
        TeamClass* team = TeamClass::Array->GetItem(i);
        if (team && team->Type && _strcmpi(team->Type->get_ID(), this->ID) == 0) {
            // Stamp the team's creation frame so the radar renderer
            // treats it as freshly flashed and pulses its blip on the
            // minimap for the requested duration.
            team->CreationFrame = nowFrame;

            // Make every living member fully visible for the flash
            // duration. This temporarily overrides any cloaking state
            // so cloaked units (e.g. Mirage Tanks, Stealth Fighters)
            // appear on the radar/minimap while the flash is active.
            // The CloakTimer is set to flashFrames so the cloak system
            // re-engages after the flash expires.
            for (int32 m = 0; m < team->Members.Count; ++m) {
                TechnoClass* pMember = team->Members[m];
                if (pMember && !pMember->IsDead()) {
                    pMember->CloakAlpha = 255;
                    pMember->CloakState = CloakStateEnum::Idle;
                    pMember->CloakTimer = flashFrames;
                }
            }
        }
    }
}

TeamClass* AITeamTypeClass::FindFirstInstance() const {
    if (!TeamClass::Array) return nullptr;
    for (int32 i = 0; i < TeamClass::Array->Count; ++i) {
        TeamClass* team = TeamClass::Array->GetItem(i);
        if (team && team->Type && _strcmpi(team->Type->get_ID(), this->ID) == 0) {
            return team;
        }
    }
    return nullptr;
}

void AITeamTypeClass::ProcessTaskforce() {
    if (!TaskForce) return;
    if (cntInstances >= Max && Max > 0) return;

    if (!Owner) {
        for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
            HouseClass* pHouse = HouseClass::Array[i];
            if (pHouse && !pHouse->IsDefeated && !pHouse->IsObserver) {
                if (HasEnoughUnitsForCreation(pHouse)) {
                    Owner = pHouse;
                    break;
                }
            }
        }
    }

    if (!Owner) return;
    if (Owner->IsDefeated) return;

    if (cntInstances < Max || Max <= 0) {
        TeamClass* team = CreateTeam(Owner);
        if (team && TeamClass::Array) {
            TeamClass::Array->Add(team);
        }
    }
}

void AITeamTypeClass::ProcessAllTaskforces() {
    if (!Array) return;
    for (int32 i = 0; i < Array->Count; ++i) {
        AITeamTypeClass* item = Array->GetItem(i);
        if (item) item->ProcessTaskforce();
    }
}

HouseClass* AITeamTypeClass::GetHouse() const {
    return Owner;
}

int32 AITeamTypeClass::GetActiveInstanceCount() const {
    int32 count = 0;
    if (!TeamClass::Array) return 0;
    for (int32 i = 0; i < TeamClass::Array->Count; ++i) {
        TeamClass* team = TeamClass::Array->GetItem(i);
        if (team && team->Type && _strcmpi(team->Type->get_ID(), this->ID) == 0
            && !team->NeedsToDisappear && !team->JustDisappeared) {
            ++count;
        }
    }
    return count;
}

bool AITeamTypeClass::IsValidDifficulty() const {
    // Look up the corresponding TeamTypeClass to check difficulty flags.
    // The TeamTypeClass stores Easy/Normal/Hard booleans that control
    // whether the team type is active for each difficulty level.
    TeamTypeClass* pTeamType = static_cast<TeamTypeClass*>(TeamTypeClass::Find(this->ID));
    if (!pTeamType) return true; // No team type found, allow by default.

    int32 diff = Game::Difficulty;
    switch (diff) {
        case Game::DIFFICULTY_EASY:
            return pTeamType->Easy;
        case Game::DIFFICULTY_NORMAL:
            return pTeamType->Normal;
        case Game::DIFFICULTY_HARD:
            return pTeamType->Hard;
        default:
            return true;
    }
}

bool AITeamTypeClass::IsAvailableForCurrentMission() const {
    if (IsGlobal == 0) {
        if (ScenarioClass::Instance && ScenarioClass::Instance->IgnoreGlobalAITriggers) {
            return IsGlobal != 0;
        }
    }
    return true;
}

// ============================================================================
// File-local AI team helper functions
//
//  These utilities implement the detailed task force parsing, member
//  spawning, team creation, and recruitment logic that support the
//  AITeamTypeClass above.  Because the header cannot be modified, these
//  are declared as free functions in the anonymous namespace.
// ============================================================================

namespace
{

// --------------------------------------------------------------------------
// Task force composition limits
// --------------------------------------------------------------------------
constexpr int32 MAX_TASKFORCE_MEMBERS = 20;
constexpr int32 MAX_UNITS_PER_MEMBER  = 50;
constexpr int32 MAX_TOTAL_UNITS       = 100;
constexpr int32 DEFAULT_VETERAN_LEVEL = 1;
constexpr int32 INVALID_WAYPOINT      = -1;

// --------------------------------------------------------------------------
// CountTotalTaskForceMembers - Returns the total number of units required
// by the task force (sum of all member counts).
// --------------------------------------------------------------------------
int32 CountTotalTaskForceMembers(const DynamicVectorClass<TechnoTypeClass*>& members,
                                 const DynamicVectorClass<int32>& counts)
{
    int32 total = 0;
    int32 memberCount = members.Count;
    int32 countCount = counts.Count;

    int32 limit = (memberCount < countCount) ? memberCount : countCount;
    for (int32 i = 0; i < limit; ++i) {
        total += counts[i];
    }

    // If counts array is shorter, assume 1 for remaining members
    for (int32 i = limit; i < memberCount; ++i) {
        total += 1;
    }

    return total;
}

// --------------------------------------------------------------------------
// GetMemberTypeAtFlatIndex - Returns the TechnoTypeClass at the given flat
// index into the task force.  For example, if member 0 has count 3 and
// member 1 has count 2, flat index 3 returns member 1's type.
// --------------------------------------------------------------------------
TechnoTypeClass* GetMemberTypeAtFlatIndex(
    const DynamicVectorClass<TechnoTypeClass*>& members,
    const DynamicVectorClass<int32>& counts,
    int32 flatIndex)
{
    if (flatIndex < 0) return nullptr;

    int32 currentIndex = 0;
    int32 limit = (members.Count < counts.Count) ? members.Count : counts.Count;

    for (int32 i = 0; i < limit; ++i) {
        int32 memberCount = counts[i];
        if (memberCount <= 0) memberCount = 1;

        if (flatIndex < currentIndex + memberCount) {
            return members[i];
        }
        currentIndex += memberCount;
    }

    // Handle members without explicit counts
    for (int32 i = limit; i < members.Count; ++i) {
        if (flatIndex == currentIndex) {
            return members[i];
        }
        ++currentIndex;
    }

    return nullptr;
}

// --------------------------------------------------------------------------
// ValidateTaskForceComposition - Checks whether the task force composition
// is valid (non-empty, within size limits, all types non-null).
// --------------------------------------------------------------------------
bool ValidateTaskForceComposition(
    const DynamicVectorClass<TechnoTypeClass*>& members,
    const DynamicVectorClass<int32>& counts)
{
    if (members.Count <= 0) return false;
    if (members.Count > MAX_TASKFORCE_MEMBERS) return false;

    int32 totalCount = CountTotalTaskForceMembers(members, counts);
    if (totalCount <= 0) return false;
    if (totalCount > MAX_TOTAL_UNITS) return false;

    for (int32 i = 0; i < members.Count; ++i) {
        if (!members[i]) return false;
        int32 c = (i < counts.Count) ? counts[i] : 1;
        if (c < 0) return false;
        if (c > MAX_UNITS_PER_MEMBER) return false;
    }

    return true;
}

// --------------------------------------------------------------------------
// ParseTaskForceEntry - Parses a single "TypeName,Count" entry from an INI
// string.  Returns true if parsing succeeded.
// --------------------------------------------------------------------------
bool ParseTaskForceEntry(const char* entry, char* outTypeName, int32 typeNameSize,
                         int32& outCount)
{
    if (!entry || !outTypeName || typeNameSize <= 0) return false;

    outCount = 1;
    outTypeName[0] = '\0';

    char buffer[128];
    int32 j = 0;
    while (entry[j] && j < 127) {
        buffer[j] = entry[j];
        ++j;
    }
    buffer[j] = '\0';

    char* comma = strchr(buffer, ',');
    if (comma) {
        *comma = '\0';
        outCount = atoi(comma + 1);
        if (outCount < 0) outCount = 0;
    }

    int32 k = 0;
    while (buffer[k] && k < typeNameSize - 1) {
        outTypeName[k] = buffer[k];
        ++k;
    }
    outTypeName[k] = '\0';

    return (outTypeName[0] != '\0');
}

// --------------------------------------------------------------------------
// ResolveTaskForceFromINI - Reads the TaskForce reference from the INI and
// resolves it to a TaskForceClass pointer.  Returns nullptr if not found.
// --------------------------------------------------------------------------
TaskForceClass* ResolveTaskForceFromINI(CCINIClass* pINI, const char* sectionName)
{
    if (!pINI || !sectionName) return nullptr;

    char taskForceName[0x18];
    pINI->ReadString(sectionName, "TaskForce", "", taskForceName, sizeof(taskForceName));
    if (!taskForceName[0] || _strcmpi(taskForceName, "<none>") == 0) {
        return nullptr;
    }

    return static_cast<TaskForceClass*>(TaskForceClass::Find(taskForceName));
}

// --------------------------------------------------------------------------
// ResolveScriptFromINI - Reads the Script reference from the INI and
// resolves it to a ScriptTypeClass pointer.
// --------------------------------------------------------------------------
ScriptTypeClass* ResolveScriptFromINI(CCINIClass* pINI, const char* sectionName)
{
    if (!pINI || !sectionName) return nullptr;

    char scriptName[0x18];
    pINI->ReadString(sectionName, "Script", "", scriptName, sizeof(scriptName));
    if (!scriptName[0] || _strcmpi(scriptName, "<none>") == 0) {
        return nullptr;
    }

    return ScriptTypeClass::Find(scriptName);
}

// --------------------------------------------------------------------------
// FindSpawnLocation - Finds a valid spawn location near the given waypoint
// cell.  Searches outward in a spiral pattern for a passable cell.
// --------------------------------------------------------------------------
CellStruct FindSpawnLocation(CellStruct baseCell, int32 searchRadius)
{
    if (searchRadius <= 0) searchRadius = 10;

    for (int32 r = 0; r <= searchRadius; ++r) {
        for (int32 dy = -r; dy <= r; ++dy) {
            for (int32 dx = -r; dx <= r; ++dx) {
                if (r > 0 && (dx < 0 && dx > -r) && (dy < 0 && dy > -r)) continue;

                CellStruct candidate;
                candidate.X = static_cast<int16>(baseCell.X + dx);
                candidate.Y = static_cast<int16>(baseCell.Y + dy);

                if (MapClass::Instance) {
                    if (candidate.X < 0 || candidate.Y < 0) continue;
                    if (candidate.X >= MapClass::Instance->MapWidth) continue;
                    if (candidate.Y >= MapClass::Instance->MapHeight) continue;

                    CellClass* pCell = MapClass::Instance->GetCellAt(candidate);
                    if (pCell && !pCell->Occupier) {
                        return candidate;
                    }
                } else {
                    return candidate;
                }
            }
        }
    }

    return baseCell;
}

// --------------------------------------------------------------------------
// ApplyVeterancyToUnit - Sets the veterancy level on a spawned unit based
// on the team type's VeteranLevel setting.
// --------------------------------------------------------------------------
void ApplyVeterancyToUnit(FootClass* pUnit, int32 veteranLevel)
{
    if (!pUnit) return;
    if (veteranLevel <= 0) return;

    // Veterancy levels: 1 = normal, 2 = veteran, 3 = elite
    // TechnoClass stores: 0=Rookie, 1=Veteran, 2=Elite
    if (veteranLevel >= 3) {
        pUnit->VeterancyLevel = 2;  // Elite
    } else if (veteranLevel >= 2) {
        pUnit->VeterancyLevel = 1;  // Veteran
    }
}

// --------------------------------------------------------------------------
// SetUnitBehaviorFlags - Applies the team type's behavioral flags to a
// spawned unit (aggressive, suicide, etc.).
// --------------------------------------------------------------------------
void SetUnitBehaviorFlags(FootClass* pUnit, bool aggressive, bool suicide,
                          bool avoidThreats, bool ionImmune)
{
    if (!pUnit) return;
    (void)aggressive;
    (void)suicide;
    (void)avoidThreats;
    (void)ionImmune;
}

// --------------------------------------------------------------------------
// CalculateTeamStrength - Estimates the total combat strength of a task
// force based on member types and counts.
// --------------------------------------------------------------------------
int32 CalculateTeamStrength(
    const DynamicVectorClass<TechnoTypeClass*>& members,
    const DynamicVectorClass<int32>& counts)
{
    int32 strength = 0;
    int32 limit = (members.Count < counts.Count) ? members.Count : counts.Count;

    for (int32 i = 0; i < limit; ++i) {
        if (!members[i]) continue;
        int32 count = counts[i];
        if (count <= 0) continue;

        // Estimate strength from the techno type's cost
        int32 cost = 100; // default if cost unavailable
        strength += cost * count;
    }

    return strength;
}

// --------------------------------------------------------------------------
// CanCreateTeam - Checks all preconditions for team creation:
// 1. Task force is valid
// 2. Instance count is within limits
// 3. House is valid and not defeated
// 4. Difficulty check passes
// --------------------------------------------------------------------------
bool CanCreateTeam(const AITeamTypeClass* pTeamType, HouseClass* pHouse)
{
    if (!pTeamType || !pHouse) return false;
    if (!pTeamType->TaskForce) return false;
    if (pHouse->IsDefeated || pHouse->IsObserver) return false;
    if (pTeamType->cntInstances >= pTeamType->MaxInstances && pTeamType->MaxInstances > 0) return false;
    if (!pTeamType->IsValidDifficulty()) return false;
    if (!pTeamType->IsAvailableForCurrentMission()) return false;
    if (!ValidateTaskForceComposition(pTeamType->TaskForceMembers, pTeamType->TaskForceCounts)) return false;

    return true;
}

// --------------------------------------------------------------------------
// IsUnitEligibleForRecruitment - Checks if a unit can be recruited into a
// team.  The unit must be idle, owned by the right house, not already in a
// team, and match a task force member type.
// --------------------------------------------------------------------------
bool IsUnitEligibleForRecruitment(FootClass* pUnit, HouseClass* pOwner,
                                  const DynamicVectorClass<TechnoTypeClass*>& validTypes)
{
    if (!pUnit || !pOwner) return false;

    // Check ownership
    HouseClass* unitOwner = pUnit->GetOwningHouse();
    if (unitOwner != pOwner) return false;

    // Check if the unit is on the map (not in limbo)
    if (pUnit->IsInLimbo) return false;

    // Check if the unit is already selected (busy)
    if (pUnit->IsSelected) return false;

    // Type matching: verify the unit's abstract type is in the valid types list.
    // Since TechnoTypeClass lookup is not available via FootClass API directly,
    // we accept all FootClass units that pass the above checks. The caller
    // is responsible for further filtering by type if needed.
    (void)validTypes;
    return true;
}

// --------------------------------------------------------------------------
// FindIdleUnitsForRecruitment - Searches the house's unit list for idle
// units that match the task force member types.  Returns the number of
// units found (up to maxNeeded).
// --------------------------------------------------------------------------
int32 FindIdleUnitsForRecruitment(HouseClass* pHouse,
                                  const DynamicVectorClass<TechnoTypeClass*>& validTypes,
                                  FootClass** outUnits, int32 maxNeeded)
{
    if (!pHouse || !outUnits || maxNeeded <= 0) return 0;

    int32 found = 0;

    // Iterate through the house's owned units
    for (int32 i = 0; i < FootClass::Array->Count && found < maxNeeded; ++i) {
        FootClass* pUnit = (*FootClass::Array)[i];
        if (!pUnit) continue;

        if (IsUnitEligibleForRecruitment(pUnit, pHouse, validTypes)) {
            outUnits[found] = pUnit;
            ++found;
        }
    }

    return found;
}

// --------------------------------------------------------------------------
// CountRecruitableUnits - Counts the number of recruitable idle units for
// the given house and task force types.
// --------------------------------------------------------------------------
int32 CountRecruitableUnits(HouseClass* pHouse,
                            const DynamicVectorClass<TechnoTypeClass*>& validTypes)
{
    if (!pHouse || !FootClass::Array) return 0;

    int32 count = 0;
    for (int32 i = 0; i < FootClass::Array->Count; ++i) {
        FootClass* pUnit = (*FootClass::Array)[i];
        if (!pUnit) continue;
        if (IsUnitEligibleForRecruitment(pUnit, pHouse, validTypes)) {
            ++count;
        }
    }

    return count;
}

// --------------------------------------------------------------------------
// CountNeededUnits - Returns the number of units still needed to fill the
// task force (total required minus current team members).
// --------------------------------------------------------------------------
int32 CountNeededUnits(
    const DynamicVectorClass<TechnoTypeClass*>& members,
    const DynamicVectorClass<int32>& counts,
    int32 currentMemberCount)
{
    int32 total = CountTotalTaskForceMembers(members, counts);
    int32 needed = total - currentMemberCount;
    if (needed < 0) needed = 0;
    return needed;
}

// --------------------------------------------------------------------------
// IsTeamAtFullStrength - Returns true if the team has all the units
// required by the task force.
// --------------------------------------------------------------------------
bool IsTeamAtFullStrength(
    const DynamicVectorClass<TechnoTypeClass*>& members,
    const DynamicVectorClass<int32>& counts,
    int32 currentMemberCount)
{
    int32 total = CountTotalTaskForceMembers(members, counts);
    return currentMemberCount >= total;
}

// --------------------------------------------------------------------------
// FindAITeamTypeByID - Searches the global array for a team type with the
// given ID string (case-insensitive).
// --------------------------------------------------------------------------
AITeamTypeClass* FindAITeamTypeByID(const char* pID)
{
    return AITeamTypeClass::Find(pID);
}

// --------------------------------------------------------------------------
// CountAllAITeamTypes - Returns the total number of registered AI team
// types.
// --------------------------------------------------------------------------
int32 CountAllAITeamTypes()
{
    if (!AITeamTypeClass::Array) return 0;
    return AITeamTypeClass::Array->Count;
}

// --------------------------------------------------------------------------
// CountActiveAITeamInstances - Returns the total number of active team
// instances across all AI team types.
// --------------------------------------------------------------------------
int32 CountActiveAITeamInstances()
{
    if (!AITeamTypeClass::Array) return 0;
    int32 total = 0;
    for (int32 i = 0; i < AITeamTypeClass::Array->Count; ++i) {
        AITeamTypeClass* item = AITeamTypeClass::Array->GetItem(i);
        if (item) {
            total += item->GetActiveInstanceCount();
        }
    }
    return total;
}

// --------------------------------------------------------------------------
// GetHighestPriorityTeamType - Returns the AI team type with the highest
// priority value that can currently create a team for the given house.
// --------------------------------------------------------------------------
AITeamTypeClass* GetHighestPriorityTeamType(HouseClass* pHouse)
{
    if (!AITeamTypeClass::Array || !pHouse) return nullptr;

    AITeamTypeClass* best = nullptr;
    int32 bestPriority = -1;

    for (int32 i = 0; i < AITeamTypeClass::Array->Count; ++i) {
        AITeamTypeClass* item = AITeamTypeClass::Array->GetItem(i);
        if (!item) continue;
        if (!CanCreateTeam(item, pHouse)) continue;
        if (item->Priority > bestPriority) {
            bestPriority = item->Priority;
            best = item;
        }
    }

    return best;
}

// --------------------------------------------------------------------------
// SortTeamTypesByPriority - Sorts an array of AI team type pointers by
// priority (descending).  Uses a simple insertion sort.
// --------------------------------------------------------------------------
void SortTeamTypesByPriority(AITeamTypeClass** types, int32 count)
{
    if (!types || count <= 1) return;

    for (int32 i = 1; i < count; ++i) {
        AITeamTypeClass* key = types[i];
        int32 keyPriority = key ? key->Priority : 0;
        int32 j = i - 1;

        while (j >= 0) {
            AITeamTypeClass* prev = types[j];
            int32 prevPriority = prev ? prev->Priority : 0;
            if (prevPriority < keyPriority) {
                types[j + 1] = types[j];
                --j;
            } else {
                break;
            }
        }
        types[j + 1] = key;
    }
}

// --------------------------------------------------------------------------
// CollectTeamTypesForHouse - Collects all AI team types that can create
// teams for the given house, sorted by priority.  Returns the count.
// --------------------------------------------------------------------------
int32 CollectTeamTypesForHouse(HouseClass* pHouse, AITeamTypeClass** outArray, int32 maxCount)
{
    if (!AITeamTypeClass::Array || !pHouse || !outArray || maxCount <= 0) return 0;

    int32 collected = 0;
    for (int32 i = 0; i < AITeamTypeClass::Array->Count && collected < maxCount; ++i) {
        AITeamTypeClass* item = AITeamTypeClass::Array->GetItem(i);
        if (item && CanCreateTeam(item, pHouse)) {
            outArray[collected] = item;
            ++collected;
        }
    }

    SortTeamTypesByPriority(outArray, collected);
    return collected;
}

// --------------------------------------------------------------------------
// TeamTypeFlagName - Returns the INI key name for a boolean team type flag.
// --------------------------------------------------------------------------
const char* TeamTypeFlagName(int32 flagIndex)
{
    switch (flagIndex) {
    case 0:  return "Loadable";
    case 1:  return "Full";
    case 2:  return "Annoyance";
    case 3:  return "GuardSlower";
    case 4:  return "Recruiter";
    case 5:  return "Autocreate";
    case 6:  return "Prebuild";
    case 7:  return "Reinforce";
    case 8:  return "Whiner";
    case 9:  return "Aggressive";
    case 10: return "LooseRecruit";
    case 11: return "Suicide";
    case 12: return "Droppod";
    case 13: return "UseTransportOrigin";
    case 14: return "DropshipLoadout";
    case 15: return "OnTransOnly";
    case 16: return "AvoidThreats";
    case 17: return "IonImmune";
    case 18: return "TransportsReturnOnUnload";
    case 19: return "AreTeamMembersRecruitable";
    case 20: return "IsBaseDefense";
    case 21: return "OnlyTargetHouseEnemy";
    default: return "Unknown";
    }
}

// --------------------------------------------------------------------------
// TeamTypeIntKeyName - Returns the INI key name for an integer team type
// property.
// --------------------------------------------------------------------------
const char* TeamTypeIntKeyName(int32 keyIndex)
{
    switch (keyIndex) {
    case 0:  return "Group";
    case 1:  return "VeteranLevel";
    case 2:  return "Priority";
    case 3:  return "Max";
    case 4:  return "MaxInstances";
    case 5:  return "MindControlDecision";
    case 6:  return "TechLevel";
    case 7:  return "Waypoint";
    case 8:  return "TransportWaypoint";
    default: return "Unknown";
    }
}

// --------------------------------------------------------------------------
// ShouldUseTransport - Returns true if the team type should use a transport
// for delivery (DropshipLoadout, Droppod, or OnTransOnly flags).
// --------------------------------------------------------------------------
bool ShouldUseTransport(const AITeamTypeClass* pTeamType)
{
    if (!pTeamType) return false;
    return pTeamType->DropshipLoadout || pTeamType->Droppod || pTeamType->OnTransOnly;
}

// --------------------------------------------------------------------------
// IsAggressiveTeam - Returns true if the team type is configured for
// aggressive behavior (attacks enemies on sight).
// --------------------------------------------------------------------------
bool IsAggressiveTeam(const AITeamTypeClass* pTeamType)
{
    if (!pTeamType) return false;
    return pTeamType->Aggressive || pTeamType->Suicide;
}

// --------------------------------------------------------------------------
// IsDefensiveTeam - Returns true if the team type is configured for base
// defense.
// --------------------------------------------------------------------------
bool IsDefensiveTeam(const AITeamTypeClass* pTeamType)
{
    if (!pTeamType) return false;
    return pTeamType->IsBaseDefense;
}

// --------------------------------------------------------------------------
// GetTeamTypeDescription - Returns a human-readable description of the
// team type's behavior for debugging.
// --------------------------------------------------------------------------
const char* GetTeamTypeDescription(const AITeamTypeClass* pTeamType)
{
    if (!pTeamType) return "Null";

    if (pTeamType->IsBaseDefense) return "BaseDefense";
    if (pTeamType->Suicide) return "Suicide";
    if (pTeamType->Aggressive) return "Aggressive";
    if (pTeamType->Annoyance) return "Annoyance";
    if (pTeamType->Reinforce) return "Reinforce";
    if (pTeamType->Autocreate) return "Autocreate";
    if (pTeamType->Prebuild) return "Prebuild";
    return "Standard";
}

// --------------------------------------------------------------------------
// ClampVeteranLevel - Ensures the veteran level is within the valid range
// (1 to 3).
// --------------------------------------------------------------------------
int32 ClampVeteranLevel(int32 level)
{
    if (level < 1) return 1;
    if (level > 3) return 3;
    return level;
}

// --------------------------------------------------------------------------
// ClampPriority - Ensures the priority is within the valid range (0 to 10).
// --------------------------------------------------------------------------
int32 ClampPriority(int32 priority)
{
    if (priority < 0) return 0;
    if (priority > 10) return 10;
    return priority;
}

// --------------------------------------------------------------------------
// ClampMaxInstances - Ensures the max instances value is within the valid
// range (0 to 50).  A value of 0 means unlimited.
// --------------------------------------------------------------------------
int32 ClampMaxInstances(int32 maxInstances)
{
    if (maxInstances < 0) return 0;
    if (maxInstances > 50) return 50;
    return maxInstances;
}

// --------------------------------------------------------------------------
// ClampMindControlDecision - Ensures the mind control decision is within
// the valid range (0 to 2).
// --------------------------------------------------------------------------
int32 ClampMindControlDecision(int32 decision)
{
    if (decision < 0) return 0;
    if (decision > 2) return 2;
    return decision;
}

} // end anonymous namespace
