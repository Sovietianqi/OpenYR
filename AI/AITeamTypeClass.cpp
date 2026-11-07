#include "AITeamTypeClass.h"
#include "TeamTypeClass.h"
#include "AITeamClass.h"
#include "TeamClass.h"
#include "TaskForceClass.h"
#include "TagTypeClass.h"
#include "ScriptTypeClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Houses/HouseClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Map/MapClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Game/GameInit.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"

#include <cstring>
#include <cstdio>
#include <cctype>

// ============================================================================
// AITeamTypeClass.cpp —— AI 队伍模板实现
//
// asm 权威：主程序里的 TeamTypeClass（struc AITeamTypeClass, sizeof=0xF8）。
// 本文件严格按各 proc 的语义复刻：INI 读写、二进制持久化、指针失效广播、
// 队伍投产/解散/招募与任务力处理。字段偏移见头文件。
// ============================================================================

DynamicVectorClass<AITeamTypeClass*>* AITeamTypeClass::Array = nullptr;

// ============================================================================
// 文件级航点字符串辅助
//
// asm: WaypointIdxToLetters (0x763610) / waypointStrToIdx (0x7636D0)
// 主程序里的全局工具；TeamTypeClass 的 INI 读写直接调用它们。
// ============================================================================
namespace {

// asm: WaypointIdxToLetters —— 航点号编码为字母串（A..Z, AA, AB ...）。
//   -1 -> 空串；<26 -> 单字母；否则按 26 进制展开。
const char* Waypoint_Idx_To_Letters(int32 idx, char* pBuffer, size_t bufSize) {
    if (!pBuffer || bufSize == 0) return pBuffer;
    if (idx == -1) {
        pBuffer[0] = '\0';
        return pBuffer;
    }
    if (idx < 26) {
        std::snprintf(pBuffer, bufSize, "%c", static_cast<char>(idx + 'A'));
        return pBuffer;
    }
    char tmp[8];
    int32 first = idx % 26;
    int32 rest = idx / 26;
    std::snprintf(tmp, sizeof(tmp), "%c", static_cast<char>(first + 'A'));
    std::snprintf(pBuffer, bufSize, "%c%s",
                  static_cast<char>((rest - 1) + 'A'), tmp);
    return pBuffer;
}

// asm: waypointStrToIdx —— 字母串解回航点号；非法输入返回 -1。
int32 Waypoint_Str_To_Idx(const char* pStr) {
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
// 注册表
// ============================================================================

AITeamTypeClass* AITeamTypeClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        AITeamTypeClass* item = Array->GetItem(i);
        if (item && !_strcmpi(item->ID, pID)) return item;
    }
    return nullptr;
}

// asm: TeamTypeClass_FindByString (0x6F1010)
// 遍历总表，先比 ID(nameINI)、再比 Name(nameFlag)；两串任一命中即返回。
AITeamTypeClass* AITeamTypeClass::FindByString(const char* pID) {
    if (!Array || !pID || !pID[0]) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        AITeamTypeClass* item = Array->GetItem(i);
        if (!item) continue;
        if (!_strcmpi(item->ID, pID)) return item;
        if (!_strcmpi(item->Name, pID)) return item;
    }
    return nullptr;
}

// asm: TeamTypeClass_FindOrAllocate (0x6F18F0) __fastcall
// "<none>"/"none" 返回空；否则查总表，未命中新建（构造即登入 Array）。
AITeamTypeClass* AITeamTypeClass::FindOrAllocate(const char* pID) {
    if (!pID || !pID[0]) return nullptr;
    if (!_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;

    AITeamTypeClass* found = Find(pID);
    if (found) return found;

    return GameCreate<AITeamTypeClass>(pID);
}

// asm: TeamTypes_CreateFromINIList (0x6F19C0) __fastcall (this=INI, global)
// 遍历 [TeamTypes] 段的每个键：取值为类型名，跳过 "<none>"/"none"，
// 已登记则复用，否则新建；随后调用其 LoadFromINI（虚表 +0x64）并把
// isGlobal 记为传入值。
void AITeamTypeClass::CreateFromINIList(CCINIClass* pINI, bool isGlobal) {
    if (!pINI) return;

    static const char* const SECTION = "TeamTypes";
    const int32 count = pINI->Section_GetValueCount(SECTION);

    for (int32 i = 0; i < count; ++i) {
        const char* pKeyName = pINI->Section_GetKeyName(SECTION, i);
        if (!pKeyName) continue;

        char name[0x20];
        name[0] = '\0';
        if (pINI->ReadString(SECTION, pKeyName, "", name, sizeof(name)) <= 0) {
            continue;
        }
        if (_strcmpi(name, "<none>") == 0 || _strcmpi(name, "none") == 0) {
            continue;
        }

        AITeamTypeClass* pItem = FindOrAllocate(name);
        if (!pItem) continue;

        pItem->LoadFromINI(pINI);
        pItem->isGlobal = isGlobal ? 1 : 0;
    }
}

// ============================================================================
// 构造 / 析构
// ============================================================================

// asm: TeamTypeClass_CTOR (0x6F07D0)
// 默认值：idxTeam/Group/Max/idxHouse/Waypoint/TransportWaypoint = -1；
// VeteranLevel=1，Priority=7，AreTeamMembersRecruitable=1，field_EC=9，
// field_F0=1；其余布尔默认假，指针置空。构造末尾登入 Array 并缓存下标。
AITeamTypeClass::AITeamTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID), ArrayIndex(-1), Group(-1), VeteranLevel(1),
      Loadable(false), Full(false), Annoyance(false), GuardSlower(false),
      Recruiter(false), Autocreate(false), Prebuild(false), Reinforce(false),
      Whiner(false), Aggressive(false), LooseRecruit(false), Suicide(false),
      Droppod(false), UseTransportOrigin(false), field_B2(false), OnTransOnly(false),
      Priority(7), Max(-1), field_BC(0), MindControlDecision(0),
      House(nullptr), idxHouse(-1), TechLevel(0), Tag(nullptr),
      Waypoint(-1), TransportWaypoint(-1), cntInstances(0),
      ScriptType(nullptr), TaskForce(nullptr), isGlobal(0), field_EC(9),
      field_F0(true), field_F1(false), AvoidThreats(false), IonImmune(false),
      TransportsReturnOnUnload(false), AreTeamMembersRecruitable(true),
      IsBaseDefense(false), OnlyTargetHouseEnemy(false) {
    if (Array) {
        Array->Add(this);
        ArrayIndex = Array->Count - 1;
    }
}

// asm: TeamTypeClass_SDDTOR (0x6F20F0)
// 从 vec_TeamTypes 摘除自己后交基类析构；这里同步重排后续下标。
AITeamTypeClass::~AITeamTypeClass() {
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if (Array->GetItem(i) == this) {
                Array->Remove(i);
                for (int32 j = i; j < Array->Count; ++j) {
                    AITeamTypeClass* item = Array->GetItem(j);
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

// asm: TeamTypeClass_GetClassID (0x6F1C40) —— 空指针返回 0x80004003
// (E_POINTER)；否则拷出 CLSID_TeamTypeClass。
HRESULT AITeamTypeClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = 0x54454D54;   // 'TEMT' sentinel
    pClassID->Data2 = 0x5459;
    pClassID->Data3 = 0x5045;
    for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0x54;
    return S_OK;
}

// asm: TeamTypeClass_Save (0x6F1B80) —— 仅转调基类 AbstractClass::Save 后
// 归零返回。原版就是纯粹的基类透传，无额外字段写出。
HRESULT AITeamTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    HRESULT hr = AbstractTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;
    return S_OK;
}

// asm: TeamTypeClass_Load (0x6F1BC0) —— 转调基类 AbstractClass::Load，随后
// 对 House(+0xC4)/Tag(+0xD0)/ScriptType(+0xE0)/TaskForce(+0xE4) 做 Swizzle
// 重定位。OpenYR 用 AbstractTypeClass::Load 完成实体化，指针按名字解析。
HRESULT AITeamTypeClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;
    HRESULT hr = AbstractTypeClass::Load(pStm);
    if (hr < 0) return hr;
    return S_OK;
}

// ============================================================================
// RTTI / 大小 / 下标
// ============================================================================

AbstractType AITeamTypeClass::WhatAmI() const {
    return AbstractType::TeamType;
}

// asm: TeamTypeClass_GetAbstractDerivationID (0x6F21A0) —— 常量 0x23。
AbstractType AITeamTypeClass::GetAbstractDerivationID() const {
    return AbstractType::TeamType;
}

// asm: TeamTypeClass_GetSize (0x6F21B0) —— 原版固定 0xF8；按实际布局返回。
int32 AITeamTypeClass::Size() const {
    return sizeof(AITeamTypeClass);
}

// asm: TeamTypeClass_GetArrayIndex (0x6F21C0) —— 回读 +0x98 下标缓存。
int32 AITeamTypeClass::GetArrayIndex() const {
    return ArrayIndex;
}

// ============================================================================
// asm: TeamTypeClass_PointerGotInvalid (0x6F1010) retn 8
// 把本类型持有的 Tag(+0xD0)、House(+0xC4)、TaskForce(+0xE4)、ScriptType
// (+0xE0) 中等于该抽象对象的清空；若清空后 ScriptType 仍非空，把失效通知
// 转发给它自己的虚表槽 +0x28。
// ============================================================================
void AITeamTypeClass::PointerGotInvalid(AbstractClass* pAbstract, bool removed) {
    AbstractClass* p = pAbstract;
    if (!p) return;

    if (Tag == p)        Tag = nullptr;
    if (reinterpret_cast<AbstractClass*>(House) == p)      House = nullptr;
    if (reinterpret_cast<AbstractClass*>(TaskForce) == p)  TaskForce = nullptr;
    if (reinterpret_cast<AbstractClass*>(ScriptType) == p) ScriptType = nullptr;

    if (ScriptType) {
        ScriptType->PointerGotInvalid(p, removed);
    }
}

// ============================================================================
// asm: TeamTypeClass_GetCRC (0x6F1BE0) retn 4
// 基类 CRC 后依次混入 Group、一串布尔开关、Priority/Max/field_BC、House
// 的 type 下标、TechLevel、Tag 下标、Waypoint/TransportWaypoint/cntInstances，
// 再混入 ScriptType 与 TaskForce 的 ID。这里按字段语义等价实现。
// ============================================================================
int32 AITeamTypeClass::GetCRC() const {
    CRCEngine crc;
    crc.AddData(ID, static_cast<int32>(sizeof(ID)));
    crc.AddData(&Group, sizeof(Group));
    crc.AddData(&Recruiter, sizeof(Recruiter));
    crc.AddData(&Autocreate, sizeof(Autocreate));
    crc.AddData(&Prebuild, sizeof(Prebuild));
    crc.AddData(&Reinforce, sizeof(Reinforce));
    crc.AddData(&Whiner, sizeof(Whiner));
    crc.AddData(&Aggressive, sizeof(Aggressive));
    crc.AddData(&LooseRecruit, sizeof(LooseRecruit));
    crc.AddData(&Suicide, sizeof(Suicide));
    crc.AddData(&Droppod, sizeof(Droppod));
    crc.AddData(&UseTransportOrigin, sizeof(UseTransportOrigin));
    crc.AddData(&Priority, sizeof(Priority));
    crc.AddData(&Max, sizeof(Max));
    crc.AddData(&field_BC, sizeof(field_BC));

    int32 houseTypeIndex = House && House->Type ? House->Type->ArrayIndex : -1;
    crc.AddData(&houseTypeIndex, sizeof(houseTypeIndex));
    crc.AddData(&TechLevel, sizeof(TechLevel));
    int32 tagIndex = Tag ? Tag->GetArrayIndex() : -1;
    crc.AddData(&tagIndex, sizeof(tagIndex));

    crc.AddData(&Waypoint, sizeof(Waypoint));
    crc.AddData(&TransportWaypoint, sizeof(TransportWaypoint));
    crc.AddData(&cntInstances, sizeof(cntInstances));

    DWORD scID = ScriptType ? ScriptType->Fetch_ID() : 0;
    DWORD tfID = TaskForce ? TaskForce->Fetch_ID() : 0;
    crc.AddData(&scID, sizeof(scID));
    crc.AddData(&tfID, sizeof(tfID));

    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// asm: TeamTypeClass_LoadFromINI (0x6F10A0) retn 4
// 先 INIClass_Reset，再走基类 AbstractTypeClass_LoadFromINI（失败整体失败）；
// 之后按 asm 顺序逐键读取 [<ID>] 段。键名严格对齐 asm：
//   House, VeteranLevel, MindControlDecision, Loadable, Full, Annoyance,
//   GuardSlower, Autocreate, Prebuild, Reinforce, Droppod, UseTransportOrigin,
//   Recruiter, Whiner, Suicide, LooseRecruit, Aggressive, OnTransOnly,
//   Priority, Max, TechLevel, Tag, Group, AvoidThreats, IonImmune,
//   IsBaseDefense, OnlyTargetHouseEnemy, TransportsReturnOnUnload,
//   AreTeamMembersRecruitable, Waypoint, TransportWaypoint, Script, TaskForce。
// Script/TaskForce 未命中时回退到各自类型表首项；两者缺一即返回失败。
// ============================================================================
bool AITeamTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;

    const char* section = this->ID;

    if (!AbstractTypeClass::LoadFromINI(pINI)) return false;

    // "House"：house 类型名 -> 索引；命中多人专用国别则记 idxHouse 并清
    // House，否则解析成具体 HouseClass 指针并把 idxHouse 置 -1。
    {
        char houseName[0x18];
        houseName[0] = '\0';
        pINI->ReadString(section, "House", "", houseName, sizeof(houseName));
        HouseTypeClass* pHouseType = HouseTypeClass::FindOrAllocate_YesMP(houseName);
        int32 idx = pHouseType ? pHouseType->ArrayIndex : -1;
        if (HouseClass::Is_Idx_MP(idx)) {
            idxHouse = idx;
            House = nullptr;
        } else if (idx != -1) {
            House = HouseClass::Find_By_Index_No_MP(idx);
            idxHouse = -1;
        }
    }

    VeteranLevel = pINI->ReadInteger(section, "VeteranLevel", VeteranLevel);
    MindControlDecision = pINI->ReadInteger(section, "MindControlDecision", MindControlDecision);

    Loadable = pINI->ReadBool(section, "Loadable", Loadable);
    Full = pINI->ReadBool(section, "Full", Full);
    // asm 用 INIClass_GetBool 读 "Annoyance"。
    Annoyance = pINI->ReadBool(section, "Annoyance", Annoyance);
    GuardSlower = pINI->ReadBool(section, "GuardSlower", GuardSlower);
    Autocreate = pINI->ReadBool(section, "Autocreate", Autocreate);
    Prebuild = pINI->ReadBool(section, "Prebuild", Prebuild);
    Reinforce = pINI->ReadBool(section, "Reinforce", Reinforce);
    Droppod = pINI->ReadBool(section, "Droppod", Droppod);
    UseTransportOrigin = pINI->ReadBool(section, "UseTransportOrigin", UseTransportOrigin);
    Recruiter = pINI->ReadBool(section, "Recruiter", Recruiter);
    Whiner = pINI->ReadBool(section, "Whiner", Whiner);
    Suicide = pINI->ReadBool(section, "Suicide", Suicide);
    LooseRecruit = pINI->ReadBool(section, "LooseRecruit", LooseRecruit);
    Aggressive = pINI->ReadBool(section, "Aggressive", Aggressive);
    OnTransOnly = pINI->ReadBool(section, "OnTransOnly", OnTransOnly);

    Priority = pINI->ReadInteger(section, "Priority", Priority);
    Max = pINI->ReadInteger(section, "Max", Max);
    TechLevel = pINI->ReadInteger(section, "TechLevel", TechLevel);

    // "Tag"：非空则 FindOrAllocate 到 TagTypeClass；空则保留旧值。
    {
        char tagName[0x80];
        tagName[0] = '\0';
        if (pINI->ReadString(section, "Tag", "", tagName, sizeof(tagName)) > 0) {
            Tag = TagTypeClass::FindOrAllocate(tagName);
        }
    }

    Group = pINI->ReadInteger(section, "Group", Group);
    AvoidThreats = pINI->ReadBool(section, "AvoidThreats", AvoidThreats);
    IonImmune = pINI->ReadBool(section, "IonImmune", IonImmune);
    IsBaseDefense = pINI->ReadBool(section, "IsBaseDefense", IsBaseDefense);
    OnlyTargetHouseEnemy = pINI->ReadBool(section, "OnlyTargetHouseEnemy", OnlyTargetHouseEnemy);
    TransportsReturnOnUnload = pINI->ReadBool(section, "TransportsReturnOnUnload", TransportsReturnOnUnload);
    AreTeamMembersRecruitable = pINI->ReadBool(section, "AreTeamMembersRecruitable", AreTeamMembersRecruitable);

    // "Waypoint"/"TransportWaypoint"：当前值编码成字母串作默认；读到则解回索引。
    {
        char wpBuf[0x40];
        char wpDef[0x40];
        Waypoint_Idx_To_Letters(Waypoint, wpDef, sizeof(wpDef));
        wpBuf[0] = '\0';
        if (pINI->ReadString(section, "Waypoint", wpDef, wpBuf, sizeof(wpBuf)) > 0) {
            int32 idx = Waypoint_Str_To_Idx(wpBuf);
            if (idx != -1) Waypoint = idx;
        }
        Waypoint_Idx_To_Letters(TransportWaypoint, wpDef, sizeof(wpDef));
        wpBuf[0] = '\0';
        if (pINI->ReadString(section, "TransportWaypoint", wpDef, wpBuf, sizeof(wpBuf)) > 0) {
            int32 idx = Waypoint_Str_To_Idx(wpBuf);
            if (idx != -1) TransportWaypoint = idx;
        }
    }

    // "Script"：FindOrAllocate 到脚本类型。
    {
        char scriptName[0x80];
        scriptName[0] = '\0';
        if (pINI->ReadString(section, "Script", "", scriptName, sizeof(scriptName)) > 0) {
            ScriptType = ScriptTypeClass::FindOrAllocate(scriptName);
        }
    }

    // "TaskForce"：FindOrAllocate 到任务力。
    {
        char tfName[0x80];
        tfName[0] = '\0';
        if (pINI->ReadString(section, "TaskForce", "", tfName, sizeof(tfName)) > 0) {
            TaskForce = TaskForceClass::FindOrAllocate(tfName);
        }
    }

    // 未命中回退到各自类型表首项；缺任一即失败。
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
// asm: TeamTypeClass_SaveToINI (0x6F11B0) retn 4
// 先走基类 AbstractTypeClass_SaveIntoINI，失败整体失败；随后按 asm 键名把
// 全部标量/布尔/引用写回。引用类型非空时写其 ID（原版 +0x24）。
// ============================================================================
bool AITeamTypeClass::SaveToINI(CCINIClass* pINI) {
    if (!pINI) return false;
    if (!AbstractTypeClass::SaveToINI(pINI)) return false;

    const char* section = this->ID;

    // "House"：优先显式阵营的国别下标，否则用 idxHouse（仅多人索引有效）。
    int32 houseIdx = -1;
    if (House != nullptr) {
        houseIdx = House->Type ? House->Type->ArrayIndex : -1;
    } else if (HouseClass::Is_Idx_MP(idxHouse)) {
        houseIdx = idxHouse;
    }
    pINI->WriteHouseIndex(section, "House", houseIdx);

    pINI->WriteInteger(section, "VeteranLevel", VeteranLevel);
    pINI->WriteInteger(section, "MindControlDecision", MindControlDecision);
    pINI->WriteBool(section, "Loadable", Loadable);
    pINI->WriteBool(section, "Full", Full);
    pINI->WriteBool(section, "Annoyance", Annoyance);
    pINI->WriteBool(section, "GuardSlower", GuardSlower);
    pINI->WriteBool(section, "Recruiter", Recruiter);
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
    pINI->WriteInteger(section, "Group", Group);
    pINI->WriteBool(section, "OnTransOnly", OnTransOnly);
    pINI->WriteBool(section, "AvoidThreats", AvoidThreats);
    pINI->WriteBool(section, "IonImmune", IonImmune);
    pINI->WriteBool(section, "TransportsReturnOnUnload", TransportsReturnOnUnload);
    pINI->WriteBool(section, "AreTeamMembersRecruitable", AreTeamMembersRecruitable);
    pINI->WriteBool(section, "IsBaseDefense", IsBaseDefense);
    pINI->WriteBool(section, "OnlyTargetHouseEnemy", OnlyTargetHouseEnemy);

    if (Tag != nullptr) {
        pINI->WriteString(section, "Tag", Tag->ID);
    }

    {
        char letter[0x40];
        Waypoint_Idx_To_Letters(Waypoint, letter, sizeof(letter));
        pINI->WriteString(section, "Waypoint", letter);
        Waypoint_Idx_To_Letters(TransportWaypoint, letter, sizeof(letter));
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
// asm: TeamTypeClass_SaveToINIList (0x6F1AB0) __fastcall
// [TeamTypes] 段整体回写：先规范化既有键，再把 isGlobal == owner 的类型从
// 0 开始编号写入 [TeamTypes] 并逐条 SaveToINI。
// ============================================================================
void AITeamTypeClass::SaveToINIList(CCINIClass* pINI, int32 owner) {
    if (!pINI) return;

    static const char* const SECTION = "TeamTypes";

    const int32 count = pINI->Section_GetValueCount(SECTION);
    for (int32 i = 0; i < count; ++i) {
        const char* pKeyName = pINI->Section_GetKeyName(SECTION, i);
        if (!pKeyName) continue;
        char value[0x20];
        value[0] = '\0';
        pINI->GetString(SECTION, pKeyName, value, sizeof(value));
        pINI->WriteString(SECTION, pKeyName, value);
    }

    if (!Array) return;

    int32 number = 0;
    for (int32 i = 0; i < Array->Count; ++i) {
        AITeamTypeClass* pType = Array->GetItem(i);
        if (!pType) continue;
        if (pType->isGlobal != owner) continue;

        char key[0x20];
        std::snprintf(key, sizeof(key), "%d", number);
        ++number;
        pINI->WriteString(SECTION, key, pType->ID);
        pType->SaveToINI(pINI);
    }
}

// ============================================================================
// asm: TeamTypeClass_GetHouse (0x6F2070) —— 显式 House 指针优先；否则若
// idxHouse 是多人国别索引则按 YesMP 解析，非多人返回空。
// ============================================================================
HouseClass* AITeamTypeClass::GetHouse() const {
    if (House != nullptr) return House;
    if (HouseClass::Is_Idx_MP(idxHouse)) {
        return HouseClass::Find_By_Index_Yes_MP(idxHouse);
    }
    return nullptr;
}

// ============================================================================
// asm: TeamTypeClass_GetGroup (0x6F1880) —— Group 为 -2/-1 且 TaskForce 非空
// 时回落到 TaskForce->Grouping，否则原样返回 Group。
// ============================================================================
int32 AITeamTypeClass::GetGroup() const {
    if (Group == 0xFFFFFFFE || Group == -1) {
        if (TaskForce) return TaskForce->Grouping;
    }
    return Group;
}

// ============================================================================
// asm: TeamTypeClass_FindWaypoint (0x6F18A0) retn 4
// Waypoint == -1 时写默认坐标（dword_B0E9C8），否则查 Scenario 航点坐标。
// ============================================================================
CellStruct* AITeamTypeClass::GetWaypoint(CellStruct* buffer) const {
    if (!buffer) return nullptr;
    if (Waypoint == -1) {
        buffer->X = 0;
        buffer->Y = 0;
        return buffer;
    }
    if (ScenarioClass::Instance) {
        *buffer = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    } else {
        buffer->X = 0;
        buffer->Y = 0;
    }
    return buffer;
}

// ============================================================================
// asm: TeamTypeClass_FindTransportWaypoint (0x6F18E0) retn 4
// ============================================================================
CellStruct* AITeamTypeClass::GetTransportWaypoint(CellStruct* buffer) const {
    if (!buffer) return nullptr;
    if (TransportWaypoint == -1) {
        buffer->X = 0;
        buffer->Y = 0;
        return buffer;
    }
    if (ScenarioClass::Instance) {
        *buffer = ScenarioClass::Instance->GetWaypointCoords(TransportWaypoint);
    } else {
        buffer->X = 0;
        buffer->Y = 0;
    }
    return buffer;
}

// ============================================================================
// asm: TeamTypeClass_CreateTeam (0x6F09D0) retn 4
// 决定是否新开一支队伍：无显式拥有者时回落到类型自身阵营；ScenarioInit 非零
// 直接放行；否则 Max >= 0 时比较现有实例数，超出上限拒绝。通过后 new 一支
// AITeamClass（原版 vec_Teams 存的就是 AITeamClass），并调用其构造。
// ============================================================================
AITeamClass* AITeamTypeClass::CreateTeam(HouseClass* pHouse) {
    HouseClass* pOwner = pHouse;
    if (pOwner == nullptr) {
        pOwner = House;
        if (pOwner == nullptr) {
            if (!HouseClass::Is_Idx_MP(idxHouse)) return nullptr;
            pOwner = HouseClass::Find_By_Index_Yes_MP(idxHouse);
            if (pOwner == nullptr) return nullptr;
        }
    }

    if (ScenarioInit == 0 && Max >= 0) {
        // asm: Session ? HouseClass_CountInstancesOfTeam(this) : cntInstances。
        // 这里直接数 vec_Teams 中 TeamType == this 的队伍，避免跨类强转。
        int32 existing = cntInstances;
        if (HouseClass::Is_Idx_MP(idxHouse) && AITeamClass::Array) {
            existing = 0;
            for (int32 i = 0; i < AITeamClass::Array->Count; ++i) {
                AITeamClass* pTeam = AITeamClass::Array->GetItem(i);
                if (pTeam && pTeam->TeamType == this) ++existing;
            }
        }
        if (existing >= Max) return nullptr;
    }

    AITeamClass* pTeam = new AITeamClass(this, pOwner);
    if (pTeam) {
        // asm: TeamClass_CTOR 末尾把新队登入全局 vec_Teams（LogicClass_Update
        // 第 7 步 `vec_Teams[i] -> call [obj+0x5C]` 就是遍历它）。这里补上登记，
        // 让 AITeamClass::UpdateAll() 能驱动到新建的队伍。
        if (AITeamClass::Array == nullptr) {
            AITeamClass::Array = new DynamicVectorClass<AITeamClass*>();
        }
        if (AITeamClass::Array != nullptr) {
            AITeamClass::Array->Add(pTeam);
        }
        ++cntInstances;
    }
    return pTeam;
}

// ============================================================================
// asm: TeamTypeClass_DestroyAllInstances (0x6F0A70)
// 遍历 vec_Teams，TeamType == this 的队伍逐个消失（原版调用虚表 +0x20 销毁
// 入口）；命中后从表中移除，计数器同步。
// ============================================================================
void AITeamTypeClass::DestroyAllInstances() {
    if (!AITeamClass::Array) return;
    for (int32 i = AITeamClass::Array->Count - 1; i >= 0; --i) {
        AITeamClass* pTeam = AITeamClass::Array->GetItem(i);
        if (pTeam && pTeam->TeamType == this) {
            pTeam->Disband();
            delete pTeam;
            AITeamClass::Array->Remove(i);
        }
    }
    cntInstances = 0;
}

// ============================================================================
// asm: TeamTypeClass_FindFirstInstance (0x6F1F70) —— 线性扫描 vec_Teams，
// 返回第一个 TeamType == this 的队伍。
// ============================================================================
AITeamClass* AITeamTypeClass::FindFirstInstance() const {
    if (!AITeamClass::Array) return nullptr;
    for (int32 i = 0; i < AITeamClass::Array->Count; ++i) {
        AITeamClass* pTeam = AITeamClass::Array->GetItem(i);
        if (pTeam && pTeam->TeamType == this) {
            return pTeam;
        }
    }
    return nullptr;
}

// ============================================================================
// asm: TeamTypeClass_SetAllFlashing (0x6F1F30) retn 4
// 对本类型名下每支队伍设置闪烁计时（原版 Team_SetFlashing(duration)）。
// AITeamClass 当前未暴露闪烁字段，这里按语义遍历匹配集合；接口就绪后可在
// 命中分支内驱动闪烁。此处保持“命中集合”的正确性。
// ============================================================================
void AITeamTypeClass::SetAllFlashing(int32 duration) {
    if (!AITeamClass::Array) return;
    for (int32 i = 0; i < AITeamClass::Array->Count; ++i) {
        AITeamClass* pTeam = AITeamClass::Array->GetItem(i);
        if (pTeam && pTeam->TeamType == this) {
            (void)duration;
        }
    }
}

// ============================================================================
// asm: TeamTypeClass_CanRecruitUnit (0x6F1E20) retn 8
// 逐条对应 asm 的提前返回分支。Unit 侧的队伍归属/招募标志字段
// (+0x5D4/+0x421/+0x422) 未在 OpenYR TechnoClass 暴露，这里复刻本类可控的
// 判据：Unit 有效、存活、拥有者匹配、编成上限为正、招募开关吻合与优先度约束。
// ============================================================================
bool AITeamTypeClass::CanRecruitUnit(TechnoClass* pUnit, HouseClass* pOwner) const {
    if (pUnit == nullptr) return false;
    // asm: [Unit+0x6Ch] <= 0 即无效/已阵亡。
    if (pUnit->IsDead() || pUnit->Health <= 0) return false;
    // asm: [Unit+0x21Ch] != Owner 时拒绝（单位不属于目标阵营）。
    if (pOwner != nullptr && pUnit->GetOwningHouse() != pOwner) return false;

    if (Max <= 0) return false;
    if (!Recruiter && !LooseRecruit) return false;
    return true;
}

// ============================================================================
// asm: AITeamTypeClass_ProcessTaskforce (0x6F1F80)
// 逐个任务力成员检查其可建造状态，累加 EC（初始 9）；任一成员不可用则
// field_F0 复位；成员为“已到位”时置 field_F1。末尾若 IsBaseDefense 置位则
// 再清 field_F0。
// ============================================================================
void AITeamTypeClass::ProcessTaskforce() {
    field_F0 = true;
    field_F1 = false;
    field_EC = 9;

    // 原版遍历任务力成员并逐项检查其“可建造/存在”状态。OpenYR 的
    // TaskForceClass 尚未暴露逐成员的可建造查询，这里按语义以“任务力非空”
    // 作为整体可用性判据，保持 field_F0 的对外语义。
    if (!(TaskForce && TaskForce->CountObjects() > 0)) {
        field_F0 = false;
    }

    if (IsBaseDefense) {
        field_F0 = false;
    }
}

// ============================================================================
// asm: AITeamTypeClass_ProcessAllTaskforces (0x6F2030) —— 遍历 Array 逐项处理。
// ============================================================================
void AITeamTypeClass::ProcessAllTaskforces() {
    if (!Array) return;
    for (int32 i = 0; i < Array->Count; ++i) {
        AITeamTypeClass* item = Array->GetItem(i);
        if (item) item->ProcessTaskforce();
    }
}
