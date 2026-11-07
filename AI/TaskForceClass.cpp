#include "TaskForceClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"
#include "../Game/Externs.h"

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
// task-force section is a "count,unitTypeID" pair.
//
// asm 权威布局（原版）：
//   +0x98 Group(整型, 构造 -1) / +0x9C cntObjects / +0xA0 isGlobal /
//   +0xA4.. 内联 6 项成员数组，每项 8 字节 {int cnt; TechnoTypeClass*}。
// OpenYR 用 DynamicVectorClass 承载成员表，字段名与顺序不必逐字节对应，
// 只要语义一致。
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
    : AbstractTypeClass(pID), Grouping(-1), IsGlobal(0) {
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
// asm: TaskForceClass_Load (0x6E86A0) retn 8 —— 链 AbstractClass::Load 之后
// 重新盖章本类 vtable，再对 6 项成员槽逐个调用 SwizzleManager 还原指针。
// OpenYR 无原版 Swizzle 机制，改按名称重新绑定 TechnoTypeClass*。
// ----------------------------------------------------------------------------
HRESULT TaskForceClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // 读 ID
    char idBuf[0x18];
    hr = pStm->Read(idBuf, sizeof(idBuf), &read);
    if (hr < 0 || read != sizeof(idBuf)) return E_FAIL;
    std::memcpy(ID, idBuf, sizeof(ID));
    ID[sizeof(ID) - 1] = '\0';

    // 读 Group
    hr = pStm->Read(&Grouping, sizeof(Grouping), &read);
    if (hr < 0 || read != sizeof(Grouping)) return E_FAIL;

    // 读 isGlobal
    hr = pStm->Read(&IsGlobal, sizeof(IsGlobal), &read);
    if (hr < 0 || read != sizeof(IsGlobal)) return E_FAIL;

    // 读成员条目数
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
            member.Type = TechnoTypeClass::Find(typeName);
        }
        Members.Add(member);
    }

    return S_OK;
}

// ----------------------------------------------------------------------------
// Save - write the task force's persistent state to a binary stream.
//
// asm: TaskForceClass_Save (0x6E8870) retn 0Ch —— 仅链 AbstractClass::Save，
// 返回其 HRESULT（<0 保留，否则归零）。
// ----------------------------------------------------------------------------
HRESULT TaskForceClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    hr = pStm->Write(ID, sizeof(ID), &written);
    if (hr < 0 || written != sizeof(ID)) return E_FAIL;

    hr = pStm->Write(&Grouping, sizeof(Grouping), &written);
    if (hr < 0 || written != sizeof(Grouping)) return E_FAIL;

    hr = pStm->Write(&IsGlobal, sizeof(IsGlobal), &written);
    if (hr < 0 || written != sizeof(IsGlobal)) return E_FAIL;

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

// asm: TaskForceClass_GetAbstractDerivationID (0x6E87D4) —— 返回常量 0x21 = 33
AbstractType TaskForceClass::GetAbstractDerivationID() const {
    return AbstractType::TaskForce;
}

// ----------------------------------------------------------------------------
// GetTechLevel - asm: TaskForceClass_GetTechLevel (0x6E87E0)
// 遍历成员，取各成员类型 Prerequisite[0] 的最大值；若某成员的前提列表首项为
// -1（"需全部前提"）且会话正在运行，则记为 0x0B。任何成员按 -1 记 0x0B。
// ----------------------------------------------------------------------------
int32 TaskForceClass::GetTechLevel() const {
    int32 techLevel = 0;
    const int32 count = Members.Count;
    if (count <= 0) return techLevel;

    for (int32 i = 0; i < count; ++i) {
        const TechnoTypeClass* pType = Members.Items[i].Type;
        if (pType == nullptr) continue;

        int32 prereq = 0;
        if (pType->Prerequisite.Count > 0) {
            prereq = pType->Prerequisite[0];
        }

        if (prereq > techLevel) {
            techLevel = prereq;
        } else if (prereq == -1 && TheSession != nullptr) {
            techLevel = 0x0B;
        }
    }
    return techLevel;
}

// ============================================================================
// INI loading
//
// asm: TaskForceClass_LoadFromINI (0x6E84F0) retn 4
//   链 AbstractTypeClass::LoadFromINI；随后清空成员表，对 key=0..5 依次
//   GetString(本段, "N")，非空则 LoadObjects 解析 "数量,单位ID"；仅当解析出的
//   类型非空才把该成员计入 cntObjects。最后读 [Group] 整型（默认沿用旧值）。
// ============================================================================

bool TaskForceClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;

    const char* sectionName = this->ID;

    // asm: 先走基类解析（Name/UIName/Cost/TechLevel），失败则整体失败。
    if (!AbstractTypeClass::LoadFromINI(pINI)) {
        return false;
    }

    Members.Clear();

    // asm: 硬编码 6 项上限。
    for (int32 i = 0; i < 6; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "%d", i);

        char value[0x80];
        value[0] = '\0';
        if (pINI->ReadString(sectionName, key, "", value, sizeof(value)) <= 0) {
            continue;
        }

        TaskForceMember member;
        LoadObjects(&member, value);
        if (member.Type != nullptr) {
            Members.Add(member);
        }
    }

    // asm: GetInteger(section, "Group", fallback = 旧 Grouping)。
    Grouping = pINI->ReadInteger(sectionName, "Group", Grouping);

    (void)sectionName;
    return true;
}

// ----------------------------------------------------------------------------
// SaveToINI - asm: TaskForceClass_SaveToINI (0x6E852F) retn 4
//   链 AbstractTypeClass::SaveToINI；对 key=0..5：若 i < cntObjects 则把
//   第 i 个成员格式化 "数量,单位ID" 写回，否则写空串清除陈旧键；最后写
//   [Group] 整型（十六进制无）。
// ----------------------------------------------------------------------------
bool TaskForceClass::SaveToINI(CCINIClass* pINI) {
    if (pINI == nullptr) return false;

    if (!AbstractTypeClass::SaveToINI(pINI)) {
        return false;
    }

    const int32 count = Members.Count;

    for (int32 i = 0; i < 6; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "%d", i);

        if (i < count) {
            const TaskForceMember& m = Members.Items[i];
            char value[0x80];
            const char* pName = (m.Type != nullptr) ? m.Type->ID : "";
            snprintf(value, sizeof(value), "%d,%s", m.Count, pName);
            pINI->WriteString(this->ID, key, value);
        } else {
            // mangleStuff_：清除越界的陈旧编号键。
            pINI->WriteString(this->ID, key, "");
        }
    }

    pINI->WriteInteger(this->ID, "Group", Grouping, false);
    return true;
}

// ============================================================================
// CRC computation
//
// asm: TaskForceClass_GetCRC (0x6E87FC) retn 4
//   链 AbstractTypeClass::GetCRC(buf)，随后把本对象 Group(+0x98) 与成员数
//   (+0x9C) 各喂入一个 DWORD。
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

// ----------------------------------------------------------------------------
// LoadObjects - asm: TaskForceClass_LoadObjects (0x6E7E90) retn 4
//   __thiscall (TaskForceMember* pOut, const char* str)。把 "数量,单位ID"
//   经 sscanf("%d,%s") 拆开：数量入 pOut->Count，ID 依次在步兵/载具/飞机
//   三张类型表中解析，命中即写 pOut->Type（未命中保持空）。
// ----------------------------------------------------------------------------
void TaskForceClass::LoadObjects(TaskForceMember* pOut, const char* str) {
    if (pOut == nullptr) return;
    pOut->Count = 0;
    pOut->MinCount = 0;
    pOut->MaxCount = 0;
    pOut->Type = nullptr;
    if (str == nullptr) return;

    int32 count = 0;
    char objID[0x40];
    objID[0] = '\0';
    if (sscanf(str, "%d,%63s", &count, objID) != 2) {
        return;
    }

    pOut->Count = count;
    if (objID[0] && _strcmpi(objID, "<none>") != 0 && _strcmpi(objID, "none") != 0) {
        pOut->Type = TechnoTypeClass::Find(objID);
    }
    pOut->MinCount = count;
    pOut->MaxCount = count;
}

// ----------------------------------------------------------------------------
// CreateFromINIList - TaskForceClass_CreateFromINIList (asm 0x6E822B)
//
//   __fastcall (CCINIClass* this, int global)。遍历 [TaskForces] 段：每个键
//   的值即要创建的特遣队名；"<none>"/"none" 跳过，已登记的名称复用而非重建；
//   新建时把 global 写入该对象的 isGlobal。
// ----------------------------------------------------------------------------
void TaskForceClass::CreateFromINIList(CCINIClass* pINI, int32 global)
{
    if (pINI == nullptr) {
        return;
    }

    static const char* const SECTION = "TaskForces";
    const int32 count = pINI->GetKeyCount(SECTION);

    for (int32 i = 0; i < count; ++i)
    {
        const char* pKeyName = pINI->GetKeyName(SECTION, i);
        if (pKeyName == nullptr) {
            continue;
        }

        char name[0x18];
        name[0] = '\0';
        if (pINI->ReadString(SECTION, pKeyName, "", name, sizeof(name)) <= 0) {
            continue;
        }

        if (_strcmpi(name, "<none>") == 0 || _strcmpi(name, "none") == 0) {
            continue;
        }

        TaskForceClass* pItem = FindOrAllocate(name);
        if (pItem != nullptr) {
            pItem->IsGlobal = global;
            pItem->LoadFromINI(pINI);
        }
    }
}

// ----------------------------------------------------------------------------
// SaveListToINI - asm: TaskForceClass_SaveListToINI (0x6E833F) retn 0
//   __fastcall (CCINIClass* this, int global)。先遍历 [TaskForces] 现有键做
//   一次规范化（mangleStuff_：写空串清除），再对 isGlobal==global 的对象
//   按序号 0.. 写回其名称并调用各自的 SaveToINI。
// ----------------------------------------------------------------------------
void TaskForceClass::SaveListToINI(CCINIClass* pINI, int32 global)
{
    if (pINI == nullptr) {
        return;
    }

    static const char* const SECTION = "TaskForces";

    // 规范化：把现有编号键全部清空，随后按归属逐个重写。
    int32 existing = pINI->GetKeyCount(SECTION);
    for (int32 i = 0; i < existing; ++i) {
        const char* pKeyName = pINI->GetKeyName(SECTION, i);
        if (pKeyName != nullptr) {
            pINI->WriteString(SECTION, pKeyName, "");
        }
    }

    if (!Array) {
        return;
    }

    int32 idx = 0;
    for (int32 i = 0; i < Array->Count; ++i) {
        TaskForceClass* pTF = Array->GetItem(i);
        if (pTF == nullptr || pTF->IsGlobal != global) {
            continue;
        }

        char key[16];
        snprintf(key, sizeof(key), "%d", idx);
        ++idx;
        pINI->WriteString(SECTION, key, pTF->ID);
        pTF->SaveToINI(pINI);
    }
}

// ------------------------------------------------------------------------
// asm: int __thiscall TaskForceClass_CountObjects(AITaskTypeClass *this)
// 逐项累加成员数量。
// ------------------------------------------------------------------------
int32 TaskForceClass::CountObjects() const
{
    int32 total = 0;
    const int32 count = Members.GetCount();
    for (int32 i = 0; i < count; ++i) {
        total += Members[i].Count;
    }
    return total;
}

TaskForceClass* TaskForceClass::Find_obsolete(const char* pID)
{
    if (pID == nullptr) {
        return nullptr;
    }

    const int32 count = Array->GetCount();
    for (int32 i = 0; i < count; ++i) {
        TaskForceClass* pTF = (*Array)[i];
        if (pTF != nullptr && strcasecmp(pID, pTF->ID) == 0) {
            return pTF;
        }
    }
    return nullptr;
}
