#include "TaskForceClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// TaskForceClass.cpp - AI task force implementation
// ============================================================================
// A TaskForceClass defines the unit composition of an AI team. Each member
// entry pairs a TechnoTypeClass pointer with a desired count (and optional
// min/max bounds for team-recruitment flexibility). The original game reads
// task forces from the [TaskForces] INI block: every numbered entry in a
// task-force section is a "unitType,count" pair.
//
// This file implements:
//   * Static registry (Array) management and name-based lookup
//   * Construction / destruction with full member initialisation
//   * Binary stream persistence (Load / Save) of the member list
//   * INI parsing that resolves unit-type names to TechnoTypeClass pointers
//   * CRC computation for multiplayer sync verification
//   * Member management: add / remove / query / validate
// ============================================================================

DynamicVectorClass<TaskForceClass*>* TaskForceClass::Array = nullptr;

// ----------------------------------------------------------------------------
// Find - locate a task force by its (case-insensitive) ID.
// ----------------------------------------------------------------------------
TaskForceClass* TaskForceClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        TaskForceClass* item = Array->GetItem(i);
        if (item && !_strcmpi(item->ID, pID)) {
            return item;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// FindOrAllocate - return an existing task force or allocate a new one.
// The sentinel strings "<none>" and "none" map to no task force.
// ----------------------------------------------------------------------------
TaskForceClass* TaskForceClass::FindOrAllocate(const char* pID) {
    if (!pID || !pID[0]) return nullptr;
    if (!_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;

    TaskForceClass* found = Find(pID);
    if (found) return found;

    TaskForceClass* newItem = GameCreate<TaskForceClass>(pID);
    if (newItem && Array) {
        Array->Add(newItem);
    }
    return newItem;
}

// ----------------------------------------------------------------------------
// GetCount - number of registered task forces.
// ----------------------------------------------------------------------------
int32 TaskForceClass::GetCount() {
    return Array ? Array->Count : 0;
}

// ============================================================================
// Construction / destruction
// ============================================================================

TaskForceClass::TaskForceClass(const char* pID) noexcept
    : AbstractTypeClass(pID), Grouping(-1) {
}

TaskForceClass::~TaskForceClass() {
    Members.Clear();
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT TaskForceClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = 0x54535446;   // 'TSTF' sentinel for TaskForceClass
    pClassID->Data2 = 0x5446;
    pClassID->Data3 = 0x5446;
    for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0x46;
    return S_OK;
}

// ----------------------------------------------------------------------------
// Load - read the task force's persistent state from a binary stream.
//
// The on-disk layout is: ID[24] | Grouping | MemberCount | for each member a
// 24-byte type-name string plus Count, MinCount, MaxCount. Type pointers are
// re-resolved by name after the type registries have been loaded.
// ----------------------------------------------------------------------------
HRESULT TaskForceClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read ID
    char idBuf[0x18];
    hr = pStm->Read(idBuf, sizeof(idBuf), &read);
    if (hr < 0 || read != sizeof(idBuf)) return E_FAIL;
    std::memcpy(ID, idBuf, sizeof(ID));
    ID[sizeof(ID) - 1] = '\0';

    // Read Grouping
    hr = pStm->Read(&Grouping, sizeof(Grouping), &read);
    if (hr < 0 || read != sizeof(Grouping)) return E_FAIL;

    // Read member count
    int32 memCount = 0;
    hr = pStm->Read(&memCount, sizeof(memCount), &read);
    if (hr < 0 || read != sizeof(memCount)) return E_FAIL;
    if (memCount < 0) memCount = 0;

    Members.Clear();
    for (int32 i = 0; i < memCount; ++i) {
        char typeName[0x18];
        hr = pStm->Read(typeName, sizeof(typeName), &read);
        if (hr < 0 || read != sizeof(typeName)) return E_FAIL;
        typeName[sizeof(typeName) - 1] = '\0';

        int32 count = 0, minCount = 0, maxCount = 0;
        hr = pStm->Read(&count, sizeof(count), &read);
        if (hr < 0 || read != sizeof(count)) return E_FAIL;
        hr = pStm->Read(&minCount, sizeof(minCount), &read);
        if (hr < 0 || read != sizeof(minCount)) return E_FAIL;
        hr = pStm->Read(&maxCount, sizeof(maxCount), &read);
        if (hr < 0 || read != sizeof(maxCount)) return E_FAIL;

        TaskForceMember member;
        member.Type = nullptr;
        member.Count = count;
        member.MinCount = minCount;
        member.MaxCount = maxCount;
        if (typeName[0]) {
            member.Type = static_cast<TechnoTypeClass*>(
                TechnoTypeClass::Find(typeName));
        }
        Members.Add(member);
    }

    return S_OK;
}

// ----------------------------------------------------------------------------
// Save - write the task force's persistent state to a binary stream.
// ----------------------------------------------------------------------------
HRESULT TaskForceClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    hr = pStm->Write(ID, sizeof(ID), &written);
    if (hr < 0 || written != sizeof(ID)) return E_FAIL;

    hr = pStm->Write(&Grouping, sizeof(Grouping), &written);
    if (hr < 0 || written != sizeof(Grouping)) return E_FAIL;

    int32 memCount = Members.Count;
    hr = pStm->Write(&memCount, sizeof(memCount), &written);
    if (hr < 0 || written != sizeof(memCount)) return E_FAIL;

    for (int32 i = 0; i < Members.Count; ++i) {
        const TaskForceMember& m = Members.Items[i];
        char typeName[0x18];
        std::memset(typeName, 0, sizeof(typeName));
        if (m.Type && m.Type->ID) {
            int32 j = 0;
            while (m.Type->ID[j] && j < static_cast<int32>(sizeof(typeName)) - 1) {
                typeName[j] = m.Type->ID[j];
                ++j;
            }
            typeName[j] = '\0';
        }
        hr = pStm->Write(typeName, sizeof(typeName), &written);
        if (hr < 0 || written != sizeof(typeName)) return E_FAIL;

        hr = pStm->Write(&m.Count, sizeof(m.Count), &written);
        if (hr < 0 || written != sizeof(m.Count)) return E_FAIL;
        hr = pStm->Write(&m.MinCount, sizeof(m.MinCount), &written);
        if (hr < 0 || written != sizeof(m.MinCount)) return E_FAIL;
        hr = pStm->Write(&m.MaxCount, sizeof(m.MaxCount), &written);
        if (hr < 0 || written != sizeof(m.MaxCount)) return E_FAIL;
    }

    if (fClearDirty) {
        Dirty = false;
    }
    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType TaskForceClass::WhatAmI() const {
    return AbstractType::TaskForce;
}

int32 TaskForceClass::Size() const {
    return sizeof(TaskForceClass);
}

// ============================================================================
// INI loading
//
// The original game stores task forces in [TaskForces] as a list of IDs.
// Each ID has its own section containing numbered entries "N=type,count"
// plus an optional "Group" key.
// ============================================================================

bool TaskForceClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);
    if (!pINI->SectionExists(sectionName)) return false;

    // Read the grouping (formation tightness) if present.
    Grouping = pINI->ReadInteger(sectionName, "Group", Grouping);

    // Read the "Name" display label.
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

    // Parse every numbered member entry. The INI format is:
    //   0=GI,2
    //   1=Engineer,1
    // The value is "unitTypeID,count". We also support an extended
    // "unitTypeID,count,minCount,maxCount" form for editor compatibility.
    Members.Clear();
    for (int32 i = 0; i < 50; ++i) {
        char keyName[32];
        snprintf(keyName, sizeof(keyName), "%d", i);

        char entryStr[128];
        pINI->ReadString(sectionName, keyName, "", entryStr, sizeof(entryStr));
        if (!entryStr[0]) break;

        // Tokenise the value by commas.
        char typeBuf[0x18];
        int32 count = 0, minCount = 0, maxCount = 0;
        int32 fieldIndex = 0;

        char* token = entryStr;
        char* comma = strchr(entryStr, ',');
        while (token) {
            if (comma) *comma = '\0';
            if (fieldIndex == 0) {
                int32 k = 0;
                while (token[k] && k < static_cast<int32>(sizeof(typeBuf)) - 1) {
                    typeBuf[k] = token[k];
                    ++k;
                }
                typeBuf[k] = '\0';
            } else if (fieldIndex == 1) {
                count = atoi(token);
            } else if (fieldIndex == 2) {
                minCount = atoi(token);
            } else if (fieldIndex == 3) {
                maxCount = atoi(token);
            }
            ++fieldIndex;
            if (comma) {
                token = comma + 1;
                comma = strchr(token, ',');
            } else {
                token = nullptr;
            }
        }

        if (fieldIndex < 2) count = 1;  // default count if only type given

        TaskForceMember member;
        member.Count = count;
        member.MinCount = minCount;
        member.MaxCount = (maxCount > 0) ? maxCount : count;
        member.Type = nullptr;
        if (typeBuf[0] && _strcmpi(typeBuf, "<none>") != 0) {
            member.Type = static_cast<TechnoTypeClass*>(
                TechnoTypeClass::Find(typeBuf));
        }
        Members.Add(member);
    }

    return true;
}

// ============================================================================
// CRC computation
//
// Feeds the task force's ID, grouping and member list into the CRC engine so
// the multiplayer sync check can detect team-composition divergence.
// ============================================================================

int32 TaskForceClass::GetCRC() const {
    CRCEngine crc;
    crc.AddData(ID, static_cast<int32>(sizeof(ID)));
    crc.AddData(&Grouping, sizeof(Grouping));
    int32 memCount = Members.Count;
    crc.AddData(&memCount, sizeof(memCount));
    for (int32 i = 0; i < Members.Count; ++i) {
        const TaskForceMember& m = Members.Items[i];
        DWORD typeID = m.Type ? m.Type->Fetch_ID() : 0;
        crc.AddData(&typeID, sizeof(typeID));
        crc.AddData(&m.Count, sizeof(m.Count));
        crc.AddData(&m.MinCount, sizeof(m.MinCount));
        crc.AddData(&m.MaxCount, sizeof(m.MaxCount));
    }
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// Member management
// ============================================================================

int32 TaskForceClass::GetMemberCount() const {
    return Members.Count;
}

int32 TaskForceClass::GetTotalUnitCount() const {
    int32 total = 0;
    for (int32 i = 0; i < Members.Count; ++i) {
        total += Members.Items[i].Count;
    }
    return total;
}

TaskForceMember* TaskForceClass::GetMember(int32 index) {
    if (index < 0 || index >= Members.Count) return nullptr;
    return &Members.Items[index];
}

// ----------------------------------------------------------------------------
// Add_Member - append a new member entry to the task force.
// ----------------------------------------------------------------------------
void TaskForceClass::Add_Member(TechnoTypeClass* pType, int32 count,
                                 int32 minCount, int32 maxCount) {
    if (!pType || count <= 0) return;

    // If the type already exists, update its count instead of duplicating.
    for (int32 i = 0; i < Members.Count; ++i) {
        if (Members.Items[i].Type == pType) {
            Members.Items[i].Count += count;
            if (maxCount > 0) Members.Items[i].MaxCount = maxCount;
            if (minCount > 0) Members.Items[i].MinCount = minCount;
            return;
        }
    }

    TaskForceMember member;
    member.Type = pType;
    member.Count = count;
    member.MinCount = minCount;
    member.MaxCount = (maxCount > 0) ? maxCount : count;
    Members.Add(member);
}

// ----------------------------------------------------------------------------
// Remove_Member - remove a member by its index.
// ----------------------------------------------------------------------------
void TaskForceClass::Remove_Member(int32 index) {
    if (index < 0 || index >= Members.Count) return;
    Members.Remove(index);
}

// ----------------------------------------------------------------------------
// Remove_Member - remove a member by its TechnoTypeClass pointer.
// ----------------------------------------------------------------------------
void TaskForceClass::Remove_Member(TechnoTypeClass* pType) {
    if (!pType) return;
    for (int32 i = 0; i < Members.Count; ++i) {
        if (Members.Items[i].Type == pType) {
            Members.Remove(i);
            return;
        }
    }
}

// ----------------------------------------------------------------------------
// Get_Member_Count - number of distinct unit-type entries.
// ----------------------------------------------------------------------------
int32 TaskForceClass::Get_Member_Count() const {
    return Members.Count;
}

// ----------------------------------------------------------------------------
// Get_Total_Units - sum of all member counts (total units in the team).
// ----------------------------------------------------------------------------
int32 TaskForceClass::Get_Total_Units() const {
    return GetTotalUnitCount();
}

// ----------------------------------------------------------------------------
// Has_Member_Type - true if the given type is part of this task force.
// ----------------------------------------------------------------------------
bool TaskForceClass::Has_Member_Type(TechnoTypeClass* pType) const {
    if (!pType) return false;
    for (int32 i = 0; i < Members.Count; ++i) {
        if (Members.Items[i].Type == pType) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Get_Member_Entry - fetch a member entry by index (bounds-checked).
// ----------------------------------------------------------------------------
TaskForceMember* TaskForceClass::Get_Member_Entry(int32 index) {
    if (index < 0 || index >= Members.Count) return nullptr;
    return &Members.Items[index];
}

// ----------------------------------------------------------------------------
// Get_All_Members - return a pointer to the internal member list.
// ----------------------------------------------------------------------------
const DynamicVectorClass<TaskForceMember>* TaskForceClass::Get_All_Members() const {
    return &Members;
}

// ----------------------------------------------------------------------------
// Is_Valid - a task force is valid if it has at least one member with a
// non-null type and a positive count.
// ----------------------------------------------------------------------------
bool TaskForceClass::Is_Valid() const {
    if (Members.Count <= 0) return false;
    for (int32 i = 0; i < Members.Count; ++i) {
        const TaskForceMember& m = Members.Items[i];
        if (m.Type && m.Count > 0) {
            return true;
        }
    }
    return false;
}
