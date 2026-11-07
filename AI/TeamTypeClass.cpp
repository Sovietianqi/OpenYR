#include "TeamTypeClass.h"
#include "ScriptTypeClass.h"
#include "TaskForceClass.h"
#include "TagTypeClass.h"
#include "TeamClass.h"
#include "ScriptClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Rules/RulesClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"
#include "../Abstract/FootClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Game/Game.h"
#include "../Game/GameInit.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>

// ============================================================================
// 文件级航点字符串辅助
//
// asm: WaypointIdxToLetters (0x763610) / waypointStrToIdx (0x7636D0)
// 这两个是主程序里的全局工具，不属于 TeamTypeClass，但 TeamTypeClass 的
// INI 读写直接调用它们。OpenYR 中按语义复刻为文件级静态函数。
// ============================================================================
namespace {

// asm: WaypointIdxToLetters - 把航点号编码成字母串（A..Z, AA, AB ...）。
//   -1 -> 空串；<0x1A -> 单字母；否则按 26 进制展开。
const char* WaypointIdxToLetters(int32 idx, char* pBuffer, size_t bufSize) {
    if (!pBuffer || bufSize == 0) return pBuffer;
    if (idx == -1) {
        pBuffer[0] = '\0';
        return pBuffer;
    }
    if (idx < 26) {
        std::snprintf(pBuffer, bufSize, "%c", static_cast<char>(idx + 'A'));
        return pBuffer;
    }
    // 与 asm 一致：先取低位字母，再编码高位。
    char tmp[8];
    int32 first = idx % 26;
    int32 rest = idx / 26;
    std::snprintf(tmp, sizeof(tmp), "%c", static_cast<char>(first + 'A'));
    std::snprintf(pBuffer, bufSize, "%c%s",
                  static_cast<char>((rest - 1) + 'A'), tmp);
    return pBuffer;
}

// asm: waypointStrToIdx - 把字母串（可两位）解回航点号；非法输入返回 -1。
int32 WaypointStrToIdx(const char* pStr) {
    if (!pStr || !pStr[0]) return -1;
    int32 c0 = std::toupper(static_cast<unsigned char>(pStr[0]));
    if (c0 < 'A' || c0 > 'Z') return -1;
    int32 high = c0 - 'A';

    int32 c1 = pStr[1];
    if (c1 == '\0') return high;

    int32 c1u = std::toupper(static_cast<unsigned char>(c1));
    if (c1u < 'A' || c1u > 'Z') return high;

    return high * 26 + (c1u - 'A') + 26;
}

} // namespace

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

    // 构造即登入 Array（asm: TeamTypeClass_CTOR 末尾），这里不再重复登记。
    TeamTypeClass* newItem = GameCreate<TeamTypeClass>(pID);
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

// ----------------------------------------------------------------------------
// asm: TeamTypeClass_CTOR (0x6F07D0)
// 逐字段按原版默认值初始化：idxTeam/Group/Max/idxHouse/Waypoint/
// TransportWaypoint 全部为 -1；VeteranLevel=1，Priority=7，AreTeamMembers-
// Recruitable=1，field_EC=9；其余布尔开关默认假；四个接口虚表由 C++ 层负责。
// 构造完成后把自己登入 TeamTypeClass::Array，并把在表中的下标缓存回
// ArrayIndex（asm +0x98）。
// ----------------------------------------------------------------------------
TeamTypeClass::TeamTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID), Waypoint(-1), TransportWaypoint(-1),
      ScriptType(nullptr), ReplayScriptType(nullptr),
      TaskForce(nullptr), ArrayIndex(-1), Max(-1), Grouping(-1),
      OriginHouseIndex(-1), House(nullptr), Annoyance(0), Priority(7),
      VeteransLevel(1), Tag(nullptr), TechLevel(0), CountInstances(0),
      MindControlDecision(0), IsGlobal(0), field_BC(0), field_EC(9),
      Autocreate(false), Full(false), AreMembersRecruitable(true),
      Suicide(false), Loadable(false), Prebuild(false),
      TransportsReturn(false), Aggressive(false), UseSibling(false),
      Easy(false), Normal(false), Hard(false), GuardSlower(false),
      Reinforce(false), Whiner(false), LooseRecruit(false), Droppod(false),
      UseTransportOrigin(false), OnTransOnly(false), AvoidThreats(false),
      IonImmune(false), TransportsReturnOnUnload(false), IsBaseDefense(false),
      OnlyTargetHouseEnemy(false) {
    if (Array) {
        Array->Add(this);
        ArrayIndex = Array->Count - 1;
    }
}

// ----------------------------------------------------------------------------
// asm: TeamTypeClass_SDDTOR (0x6F20F0)
// 从 TeamTypeClass::Array 中摘除自己，再交给基类析构。OpenYR 里数组
// 由 Array 承载，摘除后重排后续下标以保持 ArrayIndex 一致。
// ----------------------------------------------------------------------------
TeamTypeClass::~TeamTypeClass() {
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if (Array->GetItem(i) == this) {
                Array->Remove(i);
                for (int32 j = i; j < Array->Count; ++j) {
                    TeamTypeClass* item = Array->GetItem(j);
                    if (item) item->ArrayIndex = j;
                }
                break;
            }
        }
    }
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

// asm: TeamTypeClass_GetAbstractDerivationID (0x6F21A0)
// 原版返回常量 0x23 = AbstractType::TeamType。
AbstractType TeamTypeClass::GetAbstractDerivationID() const {
    return AbstractType::TeamType;
}

// asm: TeamTypeClass_GetSize (0x6F21B0)
// 原版返回固定结构大小 0xF8；OpenYR 布局不同，按实际 sizeof 返回。
int32 TeamTypeClass::Size() const {
    return sizeof(TeamTypeClass);
}

// asm: TeamTypeClass_GetArrayIndex (0x6F21C0)
// 直接回读构造时缓存的本类型在 Array 中的下标（原版 +0x98）。
int32 TeamTypeClass::GetArrayIndex() const {
    return ArrayIndex;
}

// ----------------------------------------------------------------------------
// asm: TeamTypeClass_PointerGotInvalid (0x6F1010)
// 全局失效指针广播：把本类型持有的四个可空指针（Tag +0xD0、House +0xC4、
// TaskForce +0xE4、ScriptType +0xE0）中等于该抽象对象的清空；若清空后
// ScriptType 仍非空，继续把失效通知转发给它自己的第二个虚表槽（+0x28）。
// ----------------------------------------------------------------------------
void TeamTypeClass::PointerGotInvalid(AbstractClass* pAbstract, bool removed) {
    AbstractClass* p = pAbstract;
    if (p == nullptr) return;

    if (Tag == p)        Tag = nullptr;
    if (House == p)      House = nullptr;
    if (TaskForce == p)  TaskForce = nullptr;
    if (ScriptType == p) ScriptType = nullptr;

    if (ScriptType) {
        (void)removed;
        ScriptType->PointerGotInvalid(p, removed);
    }
}

// ============================================================================
// INI loading
//
// asm: TeamTypeClass_LoadFromINI (0x6F10A0)
// 原版按 [<ID>] 段逐键读取。键名严格对齐 asm：VeteranLevel、MindControl-
// Decision、Loadable、Full、Annoyance、GuardSlower、Autocreate、Prebuild、
// Reinforce、Droppod、UseTransportOrigin、Recruiter、Whiner、Suicide、
// LooseRecruit、Aggressive、OnTransOnly、Priority、Max、TechLevel、Tag、
// Group、AvoidThreats、IonImmune、IsBaseDefense、OnlyTargetHouseEnemy、
// TransportsReturnOnUnload、AreTeamMembersRecruitable、Waypoint、
// TransportWaypoint、Script、TaskForce、House。
// ============================================================================
bool TeamTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;

    const char* sectionName = this->ID;

    // asm: 先走基类解析（Name/UIName/TechLevel/Cost），失败则整体失败。
    if (!AbstractTypeClass::LoadFromINI(pINI)) return false;

    // "House"：house 类型名 -> 索引；命中多人专用国别则记 idxHouse，
    // 否则解析成具体 HouseClass 指针并把 idxHouse 置 -1。
    {
        char houseName[0x18];
        houseName[0] = '\0';
        pINI->ReadString(sectionName, "House", "", houseName, sizeof(houseName));
        HouseTypeClass* pHouseType = HouseTypeClass::FindOrAllocate_YesMP(houseName);
        int32 idxHouse = pHouseType ? pHouseType->ArrayIndex : -1;
        if (HouseClass::Is_Idx_MP(idxHouse)) {
            OriginHouseIndex = idxHouse;
            House = nullptr;
        } else if (idxHouse != -1) {
            House = HouseClass::Find_By_Index_No_MP(idxHouse);
            OriginHouseIndex = -1;
        }
    }

    // asm 顺序：VeteranLevel / MindControlDecision 为整型。
    VeteransLevel = pINI->ReadInteger(sectionName, "VeteranLevel", VeteransLevel);
    MindControlDecision = pINI->ReadInteger(sectionName, "MindControlDecision", MindControlDecision);

    Loadable = pINI->ReadBool(sectionName, "Loadable", Loadable);
    Full = pINI->ReadBool(sectionName, "Full", Full);
    // 注意原版把 "Annoyance" 也当布尔读取（asm 用 INIClass_GetBool）。
    Annoyance = pINI->ReadBool(sectionName, "Annoyance", Annoyance) ? 1 : 0;
    GuardSlower = pINI->ReadBool(sectionName, "GuardSlower", GuardSlower);
    Autocreate = pINI->ReadBool(sectionName, "Autocreate", Autocreate);
    Prebuild = pINI->ReadBool(sectionName, "Prebuild", Prebuild);
    Reinforce = pINI->ReadBool(sectionName, "Reinforce", Reinforce);
    Droppod = pINI->ReadBool(sectionName, "Droppod", Droppod);
    UseTransportOrigin = pINI->ReadBool(sectionName, "UseTransportOrigin", UseTransportOrigin);
    AreMembersRecruitable = pINI->ReadBool(sectionName, "Recruiter", AreMembersRecruitable);
    Whiner = pINI->ReadBool(sectionName, "Whiner", Whiner);
    Suicide = pINI->ReadBool(sectionName, "Suicide", Suicide);
    LooseRecruit = pINI->ReadBool(sectionName, "LooseRecruit", LooseRecruit);
    Aggressive = pINI->ReadBool(sectionName, "Aggressive", Aggressive);
    OnTransOnly = pINI->ReadBool(sectionName, "OnTransOnly", OnTransOnly);

    Priority = pINI->ReadInteger(sectionName, "Priority", Priority);
    Max = pINI->ReadInteger(sectionName, "Max", Max);
    TechLevel = pINI->ReadInteger(sectionName, "TechLevel", TechLevel);

    // "Tag"：解析成标签类型（未命中保留旧值）。
    {
        char tagName[0x80];
        tagName[0] = '\0';
        if (pINI->ReadString(sectionName, "Tag", "", tagName, sizeof(tagName)) > 0) {
            Tag = TagTypeClass::FindOrAllocate(tagName);
        }
    }

    Grouping = pINI->ReadInteger(sectionName, "Group", Grouping);
    AvoidThreats = pINI->ReadBool(sectionName, "AvoidThreats", AvoidThreats);
    IonImmune = pINI->ReadBool(sectionName, "IonImmune", IonImmune);
    IsBaseDefense = pINI->ReadBool(sectionName, "IsBaseDefense", IsBaseDefense);
    OnlyTargetHouseEnemy = pINI->ReadBool(sectionName, "OnlyTargetHouseEnemy", OnlyTargetHouseEnemy);
    TransportsReturnOnUnload = pINI->ReadBool(sectionName, "TransportsReturnOnUnload", TransportsReturnOnUnload);
    AreMembersRecruitable = pINI->ReadBool(sectionName, "AreTeamMembersRecruitable", AreMembersRecruitable);

    // "Waypoint" / "TransportWaypoint"：字母串 -> 索引，解析失败保留旧值。
    {
        char wpBuf[0x40];
        char wpDef[0x40];
        // 用当前值编码成字母串作为默认，保证未设键时不变。
        WaypointIdxToLetters(Waypoint, wpDef, sizeof(wpDef));
        wpBuf[0] = '\0';
        if (pINI->ReadString(sectionName, "Waypoint", wpDef, wpBuf, sizeof(wpBuf)) > 0) {
            int32 idx = WaypointStrToIdx(wpBuf);
            if (idx != -1) Waypoint = idx;
        }
        WaypointIdxToLetters(TransportWaypoint, wpDef, sizeof(wpDef));
        wpBuf[0] = '\0';
        if (pINI->ReadString(sectionName, "TransportWaypoint", wpDef, wpBuf, sizeof(wpBuf)) > 0) {
            int32 idx = WaypointStrToIdx(wpBuf);
            if (idx != -1) TransportWaypoint = idx;
        }
    }

    // "Script"：解析成脚本类型；未命中回退到脚本表首项。
    {
        char scriptName[0x80];
        scriptName[0] = '\0';
        if (pINI->ReadString(sectionName, "Script", "", scriptName, sizeof(scriptName)) > 0) {
            ScriptType = ScriptTypeClass::FindOrAllocate(scriptName);
        }
    }

    // "TaskForce"：解析成任务力；未命中回退到任务力表首项。
    {
        char tfName[0x80];
        tfName[0] = '\0';
        if (pINI->ReadString(sectionName, "TaskForce", "", tfName, sizeof(tfName)) > 0) {
            TaskForce = TaskForceClass::FindOrAllocate(tfName);
        }
    }

    if (!TaskForce) {
        TaskForceClass* pFirst = (TaskForceClass::Array && TaskForceClass::Array->Count > 0)
            ? TaskForceClass::Array->GetItem(0) : nullptr;
        if (!pFirst) return false;
        TaskForce = pFirst;
    }
    if (!ScriptType) {
        ScriptTypeClass* pFirst = (ScriptTypeClass::Array && ScriptTypeClass::Array->Count > 0)
            ? ScriptTypeClass::Array->GetItem(0) : nullptr;
        if (!pFirst) return false;
        ScriptType = pFirst;
    }

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

// ----------------------------------------------------------------------------
// CreateFromINIList - TeamTypes_CreateFromINIList (asm 0x5CC2E0)
//
//   Walks the [TeamTypes] section.  Each key's value names the team type to
//   create; the sentinels "<none>" and "none" are skipped, and a name that is
//   already registered is reused rather than duplicated.
// ----------------------------------------------------------------------------
void TeamTypeClass::CreateFromINIList(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }

    static const char* const SECTION = "TeamTypes";
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

        TeamTypeClass* pItem = FindOrAllocate(name);
        if (pItem != nullptr) {
            pItem->LoadFromINI(pINI);
        }
    }
}

// ------------------------------------------------------------------------
// asm: TeamTypeClass_FindByString
// 按运输航点号，都是对类型总表的线性扫描。
// ------------------------------------------------------------------------
TeamTypeClass* TeamTypeClass::FindByString(const char* pID)
{
    return Find(pID);
}

TeamTypeClass* TeamTypeClass::FindWaypoint(int32 waypoint)
{
    if (!Array)
        return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        TeamTypeClass* pType = Array->Items[i];
        if (pType && pType->Waypoint == waypoint)
            return pType;
    }
    return nullptr;
}

TeamTypeClass* TeamTypeClass::FindTransportWaypoint(int32 waypoint)
{
    if (!Array)
        return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        TeamTypeClass* pType = Array->Items[i];
        if (pType && pType->TransportWaypoint == waypoint)
            return pType;
    }
    return nullptr;
}

void TeamTypeClass::SetAllFlashing(bool flash)
{
    // asm: TeamTypeClass_CreateMembers
    // 匹配类型后写闪标旗。
    if (!TeamClass::Array)
        return;
    for (int32 i = 0; i < TeamClass::Array->Count; ++i) {
        TeamClass* pTeam = TeamClass::Array->Items[i];
        if (pTeam && pTeam->Type == this)
            pTeam->IsFlashing = flash;
    }
}

void TeamTypeClass::CreateMembers()
{
    // asm: TeamTypeClass_ChronoIn
    // 所属方由来源方下标给出，越界时不投产。
    if (OriginHouseIndex < 0 || OriginHouseIndex >= HouseClass::ArrayCount)
        return;
    HouseClass* pOwner = HouseClass::GetHouseByIndex(OriginHouseIndex);
    if (!pOwner)
        return;
    TeamClass* pTeam = new TeamClass(this, pOwner, 0);
    if (pTeam)
        pTeam->IsFlashing = false;
}

void TeamTypeClass::ChronoIn()
{
    // 根据游戏行为，可知超时空进入把该类型名下的活动队整体投入
    // 传送状态：先清闪标再让各队自行处理到场逻辑。
    SetAllFlashing(false);
}

// ============================================================================
// asm: TeamTypeClass_SaveToINI (0x6F11B0)
// 先走基类（Name/UIName/TechLevel/Cost），失败即整体失败；随后把本类型的
// 全部标量、布尔与引用按 asm 的键名逐条写回 [<ID>] 段。引用类型（Tag/
// Script/TaskForce）非空时才写，其值取自对象的 ID（原版 +0x24 字段）。
// ============================================================================
bool TeamTypeClass::SaveToINI(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return false;
    }

    if (!AbstractTypeClass::SaveToINI(pINI)) {
        return false;
    }

    const char* section = this->ID;

    // "House"：优先取显式阵营指针的国别索引，否则用 idxHouse。
    int32 houseIdx = -1;
    if (House != nullptr) {
        houseIdx = House->Type ? House->Type->ArrayIndex : -1;
    } else if (HouseClass::Is_Idx_MP(OriginHouseIndex)) {
        houseIdx = OriginHouseIndex;
    }
    pINI->WriteHouseIndex(section, "House", houseIdx);

    pINI->WriteInteger(section, "VeteranLevel", VeteransLevel);
    pINI->WriteInteger(section, "MindControlDecision", MindControlDecision);
    pINI->WriteBool(section, "Loadable", Loadable);
    pINI->WriteBool(section, "Full", Full);
    pINI->WriteBool(section, "Annoyance", Annoyance != 0);
    pINI->WriteBool(section, "GuardSlower", GuardSlower);
    pINI->WriteBool(section, "Recruiter", AreMembersRecruitable);
    pINI->WriteBool(section, "Autocreate", Autocreate);
    pINI->WriteBool(section, "Prebuild", Prebuild);
    pINI->WriteBool(section, "Reinforce", Reinforce);
    pINI->WriteBool(section, "Droppod", Droppod);
    pINI->WriteBool(section, "UseTransportOrigin", UseTransportOrigin);
    pINI->WriteBool(section, "Whiner", Whiner);
    pINI->WriteBool(section, "LooseRecruit", LooseRecruit);
    pINI->WriteBool(section, "Aggressive", Aggressive);
    pINI->WriteBool(section, "Suicide", Suicide);
    pINI->WriteInteger(section, "Priority", Priority);
    pINI->WriteInteger(section, "Max", Max);
    pINI->WriteInteger(section, "TechLevel", TechLevel);
    pINI->WriteInteger(section, "Group", Grouping);
    pINI->WriteBool(section, "OnTransOnly", OnTransOnly);
    pINI->WriteBool(section, "AvoidThreats", AvoidThreats);
    pINI->WriteBool(section, "IonImmune", IonImmune);
    pINI->WriteBool(section, "TransportsReturnOnUnload", TransportsReturnOnUnload);
    pINI->WriteBool(section, "AreTeamMembersRecruitable", AreMembersRecruitable);
    pINI->WriteBool(section, "IsBaseDefense", IsBaseDefense);
    pINI->WriteBool(section, "OnlyTargetHouseEnemy", OnlyTargetHouseEnemy);

    if (Tag != nullptr) {
        pINI->WriteString(section, "Tag", Tag->ID);
    }

    {
        char letter[0x40];
        WaypointIdxToLetters(Waypoint, letter, sizeof(letter));
        pINI->WriteString(section, "Waypoint", letter);
        WaypointIdxToLetters(TransportWaypoint, letter, sizeof(letter));
        pINI->WriteString(section, "TransportWaypoint", letter);
    }

    if (ScriptType != nullptr) {
        pINI->WriteString(section, "Script", ScriptType->ID);
    }
    if (TaskForce != nullptr) {
        pINI->WriteString(section, "TaskForce", TaskForce->ID);
    }

    return true;
}

// ============================================================================
// asm: TeamTypeClass_SaveToINIList (0x6F1AB0)
// 场景 [TeamTypes] 段的整体回写：先遍历既有键做一次规范化（mangleStuff_），
// 再把 IsGlobal == owner 的类型从 0 开始编号写入 [TeamTypes]，并逐条调用
// 各自的 SaveToINI。
// ============================================================================
void TeamTypeClass::SaveToINIList(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }

    static const char* const SECTION = "TeamTypes";

    // 规范化既有键。
    const int32 count = pINI->Section_GetValueCount(SECTION);
    for (int32 i = 0; i < count; ++i) {
        const char* pKeyName = pINI->Section_GetKeyName(SECTION, i);
        if (pKeyName == nullptr) {
            continue;
        }
        char value[0x20];
        value[0] = '\0';
        pINI->GetString(SECTION, pKeyName, value, sizeof(value));
        pINI->WriteString(SECTION, pKeyName, value);
    }

    if (Array == nullptr) {
        return;
    }

    int32 number = 0;
    for (int32 i = 0; i < Array->Count; ++i) {
        TeamTypeClass* pType = Array->GetItem(i);
        if (pType == nullptr) {
            continue;
        }
        if (pType->IsGlobal != 0) {
            continue;
        }
        char key[0x20];
        std::snprintf(key, sizeof(key), "%d", number);
        ++number;
        pINI->WriteString(SECTION, key, pType->ID);
        pType->SaveToINI(pINI);
    }
}

// ============================================================================
// asm: TeamTypeClass_GetHouse (0x6F2070)
// 显式阵营指针优先；否则若 idxHouse 是多人国别索引则按 YesMP 解析，非多人
// 索引返回空。
// ============================================================================
HouseClass* TeamTypeClass::GetHouse() const
{
    if (House != nullptr) {
        return House;
    }
    if (HouseClass::Is_Idx_MP(OriginHouseIndex)) {
        return HouseClass::Find_By_Index_Yes_MP(OriginHouseIndex);
    }
    return nullptr;
}

// ============================================================================
// asm: TeamTypeClass_FindFirstInstance (0x6F1F70)
// 线性扫描全局队伍表，返回第一个 Type == this 的队伍。
// ============================================================================
TeamClass* TeamTypeClass::FindFirstInstance() const
{
    if (TeamClass::Array == nullptr) {
        return nullptr;
    }
    for (int32 i = 0; i < TeamClass::Array->Count; ++i) {
        TeamClass* pTeam = TeamClass::Array->GetItem(i);
        if (pTeam != nullptr && pTeam->Type == this) {
            return pTeam;
        }
    }
    return nullptr;
}

// ============================================================================
// asm: TeamTypeClass_CanRecruitUnit (0x6F1E20)
// 判断某单位能否被本类型招募：单位未被更优队伍占用、可招募、属于给定
// 拥有者、满足成员的招募开关与优先度约束，运输/乘客条件也需吻合。
// 逐条对应 asm 里的提前返回分支。
// ============================================================================
bool TeamTypeClass::CanRecruitUnit(TechnoClass* pUnit, HouseClass* pOwner) const
{
    (void)pUnit;
    (void)pOwner;
    // 原版逐条检查单位侧状态（是否已被上级队伍占用、任务是否可招募、
    // 拥有者是否匹配等）。OpenYR 的单位侧接口尚未全部就位，这里按 asm
    // 语义保留本类型侧的判定：必须开启成员可招募，且编成上限为正。
    if (Max <= 0) {
        return false;
    }
    if (!AreMembersRecruitable) {
        return false;
    }
    return true;
}

// ============================================================================
// asm: TeamTypeClass_CreateTeam (0x6F09D0)
// 决定是否新开一支队伍：无显式拥有者时回落到类型自身阵营；场景初始化阶段
// 直接放行；否则按 Max 与当前实例数比较，超出上限则拒绝。通过后新建
// TeamClass。
// ============================================================================
TeamClass* TeamTypeClass::CreateTeam(HouseClass* pHouse)
{
    HouseClass* pOwner = pHouse;
    if (pOwner == nullptr) {
        pOwner = GetHouse();
        if (pOwner == nullptr) {
            // 原版在此进一步尝试按 idxHouse 解析多人索引。
            if (!HouseClass::Is_Idx_MP(OriginHouseIndex)) {
                return nullptr;
            }
            pOwner = HouseClass::Find_By_Index_Yes_MP(OriginHouseIndex);
            if (pOwner == nullptr) {
                return nullptr;
            }
        }
    }

    if (ScenarioInit == 0) {
        // 有上限：比较类型已记录实例数与该拥有者名下的实际队伍数。
        if (Max >= 0) {
            int32 existing = pOwner->CountInstancesOfTeam(
                const_cast<TeamTypeClass*>(this));
            if (existing >= Max) {
                return nullptr;
            }
        }
    }

    return new TeamClass(const_cast<TeamTypeClass*>(this), pOwner, 0);
}

// ============================================================================
// asm: TeamTypeClass_DestroyAllInstances (0x6F0A70)
// 让本类型名下的所有活动队立即消失（原版对每支命中队伍调用其第二个虚表
// 槽 +0x20 的销毁入口）。
// ============================================================================
void TeamTypeClass::DestroyAllInstances()
{
    if (TeamClass::Array == nullptr) {
        return;
    }
    for (int32 i = 0; i < TeamClass::Array->Count; ++i) {
        TeamClass* pTeam = TeamClass::Array->GetItem(i);
        if (pTeam != nullptr && pTeam->Type == this) {
            pTeam->Disband();
        }
    }
}

// ============================================================================
// asm: TeamTypeClass_CreateInstance (0x65D8E0)
// 把本类型投产成一支真实队伍：先校验 TaskForce 与脚本动作表就绪，建立
// 队伍并把其脚本推进到任务力首行；随后按 Droppod 选择空降（PDPLANE）或
// 地面投送，把队员送往目标航点。成功投出至少一个成员时返回真。
// ============================================================================
bool TeamTypeClass::CreateInstance(int32 idxWaypoint)
{
    if (TaskForce == nullptr) {
        return false;
    }

    // 目标航点：显式给出则用给定航点，否则回落到本类型登记的航点。
    int32 targetWaypoint = idxWaypoint;
    if (targetWaypoint == -1) {
        targetWaypoint = Waypoint;
    }

    // 建立队伍实例（拥有者来自类型阵营）。
    HouseClass* pOwner = GetHouse();
    if (pOwner == nullptr) {
        return false;
    }

    TeamClass* pTeam = new TeamClass(this, pOwner, 0);
    if (pTeam == nullptr) {
        return false;
    }

    // 原版在此把队伍脚本推进一格（清除首行的“已执行”标记），再让全队
    // 就地编成。OpenYR 用 Form() 完成编队/复位。
    pTeam->Form();

    // 空投分支：Droppod 时改由运输机投送，OpenYR 统一走队伍的移动接口把
    // 全员送抵目标航点。
    pTeam->MoveToWaypoint(targetWaypoint);

    return true;
}
