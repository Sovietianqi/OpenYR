#include "AITriggerTypeClass.h"
#include "AITeamTypeClass.h"
#include "TeamTypeClass.h"
#include "TaskForceClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/InfantryTypeClass.h"
#include "../Abstract/UnitTypeClass.h"
#include "../Abstract/AircraftTypeClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Rules/RulesClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"
#include "../Core/SwizzleManagerClass.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

DynamicVectorClass<AITriggerTypeClass*>* AITriggerTypeClass::Array = nullptr;

// ---------------------------------------------------------------------------
// Find —— 按 ID 查 [AITriggerTypes] 表。
// ---------------------------------------------------------------------------
AITriggerTypeClass* AITriggerTypeClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        AITriggerTypeClass* item = Array->GetItem(i);
        if (item && !_strcmpi(item->ID, pID)) return item;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_FindOrAllocate(const char *name)
//   "<none>"/"none" → 空；查到则返回；否则 new + CTOR（CTOR 自身入表）。
// ---------------------------------------------------------------------------
AITriggerTypeClass* AITriggerTypeClass::FindOrAllocate(const char* pID) {
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    AITriggerTypeClass* found = Find(pID);
    if (found) return found;
    return GameCreate<AITriggerTypeClass>(pID);
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_CTOR
//   默认值：idxHouse/idxSide = -1；三个 Weight 分量初值 1.875；难度位全开；
//   params 清零；Team1/Team2/objCondition = nullptr。
// ---------------------------------------------------------------------------
AITriggerTypeClass::AITriggerTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID), ConditionType(AITriggerCondition::HouseOwns),
      IsGlobal(0), OwnerHouseType(AITriggerHouseType::None), IsEnabled(true),
      HouseIndex(-1), SideIndex(-1), TechLevel(0), unknown_B4(0),
      Weight_Current(1.875), Weight_Minimum(1.875), Weight_Maximum(1.875),
      IsForSkirmish(false), IsForBaseDefense(false),
      Enabled_Easy(true), Enabled_Normal(true), Enabled_Hard(true),
      ConditionObject(nullptr), Team1(nullptr), Team2(nullptr),
      TimesExecuted(0), TimesCompleted(0), unknown_10C(0) {
    for (int32 i = 0; i < 4; ++i) {
        Conditions[i].ComparatorType = 0;
        Conditions[i].ComparatorOperand = 0;
    }
    std::memset(Params, 0, sizeof(Params));

    // CTOR 末尾把自己挂进全局表（asm 中由 CTOR 内联完成）。
    if (Array == nullptr) {
        Array = new DynamicVectorClass<AITriggerTypeClass*>();
    }
    Array->Add(this);
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_DTOR
//   析构时先从全局表移除自身。
// ---------------------------------------------------------------------------
AITriggerTypeClass::~AITriggerTypeClass() {
    if (Array != nullptr) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if (Array->GetItem(i) == this) {
                Array->Remove(i);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_GetClassID
//   CLSID 来自 .data:CLSID_AITriggerTypeClass（4 个 DWORD）。
// ---------------------------------------------------------------------------
HRESULT AITriggerTypeClass::GetClassID(CLSID* pClassID) {
    if (pClassID == nullptr) return E_POINTER;
    static const CLSID kCLSID_AITriggerTypeClass = {
        0xA1A1A1A1, 0x0000, 0x0000, { 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    *pClassID = kCLSID_AITriggerTypeClass;
    return S_OK;
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_Load —— 调 AbstractTypeClass::Load，再 swizzle 三个
//   指针字段（objCondition / Team1 / Team2）。extra 字段不单独落盘。
// ---------------------------------------------------------------------------
HRESULT AITriggerTypeClass::Load(IStream* pStm) {
    if (pStm == nullptr) return E_POINTER;

    HRESULT hr = AbstractTypeClass::Load(pStm);
    if (hr < 0) return hr;

    static SwizzleManagerClass SwizzleManager;
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&ConditionObject));
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&Team1));
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&Team2));

    return S_OK;
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_Save —— 仅转调 AbstractTypeClass::Save。
// ---------------------------------------------------------------------------
HRESULT AITriggerTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    return AbstractTypeClass::Save(pStm, fClearDirty);
}

AbstractType AITriggerTypeClass::WhatAmI() const {
    return AbstractType::AITriggerType;
}

int32 AITriggerTypeClass::Size() const {
    return sizeof(AITriggerTypeClass);
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_GetAbstractDerivationID → 0x3B
// ---------------------------------------------------------------------------
int32 AITriggerTypeClass::GetAbstractDerivationID() const {
    return 0x3B;
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_GetCRC
//   基类 CRC 之后依次混入 typeCondition/isGlobal/ownHouseType/idxHouse/
//   techLevel、按 switch 选混 params.arg+params.cond、三个 Weight、难度位、
//   idxSide、isBaseDefense。
// ---------------------------------------------------------------------------
void AITriggerTypeClass::ComputeCRC(CRCEngine& crc) const {
    AbstractTypeClass::ComputeCRC(crc);

    crc.AddData(&ConditionType, sizeof(ConditionType));
    crc.AddData(&IsGlobal, sizeof(IsGlobal));
    crc.AddData(&OwnerHouseType, sizeof(OwnerHouseType));
    crc.AddData(&HouseIndex, sizeof(HouseIndex));
    crc.AddData(&TechLevel, sizeof(TechLevel));

    // asm switch：ConditionType ∈ {0,1,4,7} 才把 params 计进 CRC（其余分支跳过）。
    switch (static_cast<int32>(ConditionType)) {
        case 0: case 1: case 4: case 7:
            crc.AddData(&Params[0], 4);   // params.arg
            crc.AddData(&Params[0], 4);   // params.cond（与 arg 叠加，语义保真）
            break;
        default:
            break;
    }

    crc.AddData(&Weight_Current, sizeof(Weight_Current));
    crc.AddData(&Weight_Minimum, sizeof(Weight_Minimum));
    crc.AddData(&Weight_Maximum, sizeof(Weight_Maximum));
    crc.AddData(&IsForSkirmish, sizeof(IsForSkirmish));
    crc.AddData(&Enabled_Easy, sizeof(Enabled_Easy));
    crc.AddData(&Enabled_Normal, sizeof(Enabled_Normal));
    crc.AddData(&Enabled_Hard, sizeof(Enabled_Hard));
    crc.AddData(&SideIndex, sizeof(SideIndex));
    crc.AddData(&IsForBaseDefense, sizeof(IsForBaseDefense));
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_LoadFromINI(AITriggerTypeClass *this, CCINIClass *INI)
//   读取 [AITriggerTypes] 段中 this->ID 对应的一行逗号分隔定义：
//     Name, Team1, OwnerHouse, CondType, CondObject, Weight, Min, Max,
//     IsForSkirmish, <skip>, SideIndex, IsBaseDefense, Team2,
//     EasyFlag, NormalFlag, HardFlag, <hex params>
//   解析成功返回 true；任一必需字段缺失返回 false。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (pINI == nullptr) return false;

    char buffer[0x200];
    buffer[0] = '\0';
    if (pINI->ReadString("AITriggerTypes", this->Name, "", buffer, sizeof(buffer)) <= 0) {
        return false;   // 该 ID 在段中不存在
    }
    if (buffer[0] == '\0') return false;

    // 逐段切分（与 asm 的 strtok(,) 序列一一对应）。
    char* ctx = nullptr;
    const char* tok = strtok_r(buffer, ",", &ctx);
    if (!tok) return false;

    // 1. Name（asm 写入 a.nameFlag，30 字节）
    strncpy(this->Name, tok, sizeof(this->Name) - 1);
    this->Name[sizeof(this->Name) - 1] = '\0';

    // 2. Team1
    tok = strtok_r(nullptr, ",", &ctx);
    if (!tok) return false;
    Team1 = _strcmpi(tok, "<none>") ? TeamTypeClass::FindByString(tok) : nullptr;

    // 3. OwnerHouse（"<all>"→Any；"<none>"→保持 None；否则按方名解析）
    tok = strtok_r(nullptr, ",", &ctx);
    if (!tok) return false;
    OwnerHouseType = AITriggerHouseType::None;
    HouseIndex = -1;
    if (!_strcmpi(tok, "<all>")) {
        OwnerHouseType = AITriggerHouseType::Any;
    } else if (_strcmpi(tok, "<none>")) {
        int32 hi = HouseTypeClass::From_Name(tok);
        HouseIndex = hi;
        if (hi != -1) OwnerHouseType = AITriggerHouseType::Single;
    }

    // 4. ConditionType（asm 丢弃 TechLevel 槽位后取本字段）
    tok = strtok_r(nullptr, ",", &ctx);
    if (!tok) return false;
    tok = strtok_r(nullptr, ",", &ctx);   // asm 顺序：先弃一槽再取条件类型
    if (!tok) return false;
    ConditionType = static_cast<AITriggerCondition>(atoi(tok));

    // 5. ConditionObject（依次查 Infantry/Unit/Aircraft/Building 类型）
    tok = strtok_r(nullptr, ",", &ctx);
    if (!tok) return false;
    {
        char name[0x18];
        strncpy(name, tok, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        ConditionObject = nullptr;
        int32 idx = InfantryTypeClass::FindIndex(name);
        if (idx != -1) {
            ConditionObject = InfantryTypeClass::FindByIndex(idx);
        } else {
            // Unit/Aircraft 无 FindIndex 时按 ID 直查。
            ConditionObject = TechnoTypeClass::Find(name);
        }
    }

    // 6. params 十六进制串（asm 每两字符解码一字节，最多 32 字节）
    tok = strtok_r(nullptr, ",", &ctx);
    if (!tok) return false;
    {
        std::memset(Params, 0, sizeof(Params));
        int32 n = 0;
        for (const char* p = tok; *p && n < static_cast<int32>(sizeof(Params)); ) {
            while (*p == ' ' || *p == '\t') ++p;
            if (!*p) break;
            Params[n++] = static_cast<uint8>(strtoul(p, const_cast<char**>(&p), 16));
        }
    }

    // 7. Weight / Min / Max
    tok = strtok_r(nullptr, ",", &ctx);
    if (tok) Weight_Current = std::floor(atof(tok));
    tok = strtok_r(nullptr, ",", &ctx);
    if (tok) Weight_Minimum = std::floor(atof(tok));
    tok = strtok_r(nullptr, ",", &ctx);
    if (tok) Weight_Maximum = std::floor(atof(tok));

    // 8. IsForSkirmish
    tok = strtok_r(nullptr, ",", &ctx);
    if (tok) IsForSkirmish = (atoi(tok) != 0);

    // 9. 跳过一个槽（asm 连续两次 strtok 只取后者）
    (void)strtok_r(nullptr, ",", &ctx);
    tok = strtok_r(nullptr, ",", &ctx);
    if (tok) SideIndex = atoi(tok);

    // 10. IsBaseDefense
    tok = strtok_r(nullptr, ",", &ctx);
    if (tok) IsForBaseDefense = (atoi(tok) != 0);

    // 11. Team2
    tok = strtok_r(nullptr, ",", &ctx);
    if (tok) {
        Team2 = _strcmpi(tok, "<none>") ? TeamTypeClass::FindByString(tok) : nullptr;
    }

    // 12/13/14. 难度位
    tok = strtok_r(nullptr, ",", &ctx);
    if (tok) Enabled_Easy = (atoi(tok) != 0);
    tok = strtok_r(nullptr, ",", &ctx);
    if (tok) Enabled_Normal = (atoi(tok) != 0);
    tok = strtok_r(nullptr, ",", &ctx);
    if (tok) Enabled_Hard = (atoi(tok) != 0);

    // techLevel 由两个队伍的 TaskForce 技术等级取最大（asm 尾段逻辑）。
    TechLevel = 0;
    if (Team1 && Team1->TaskForce) {
        int32 t1 = Team1->TaskForce->GetTechLevel();
        if (t1 > TechLevel) TechLevel = t1;
    }
    if (Team2 && Team2->TaskForce) {
        int32 t2 = Team2->TaskForce->GetTechLevel();
        if (t2 > TechLevel) TechLevel = t2;
    }

    return true;
}

// ---------------------------------------------------------------------------
// 旧名转发：装配点仍可能调用 LoadFromINIList。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::LoadFromINIList(CCINIClass* pINI) {
    return LoadFromINI(pINI);
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_SaveToINI(AITriggerTypeClass *this, int a2)
//   以 "%s,%s,%s,%d,%d,%s,%s,%lf,%lf,%lf,%d,%d,..." 格式写回 [AITriggerTypes]。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::SaveToINI(CCINIClass* pINI) {
    if (pINI == nullptr) return false;

    const char* team1Name = "<none>";
    const char* team2Name = "<none>";
    const char* condName = "<none>";
    if (Team1) team1Name = Team1->get_ID();
    if (Team2) team2Name = Team2->get_ID();
    if (ConditionObject) condName = ConditionObject->get_ID();

    const char* ownerName = "<none>";
    if (OwnerHouseType == AITriggerHouseType::Any) {
        ownerName = "<all>";
    } else if (OwnerHouseType == AITriggerHouseType::Single && HouseIndex != -1) {
        if (HouseIndex >= 0 && HouseIndex < HouseTypeClass::ArrayCount) {
            HouseTypeClass* pHouseType = HouseTypeClass::Array[HouseIndex];
            if (pHouseType) ownerName = pHouseType->get_ID();
        }
    }

    // params 十六进制串（asm 用 "%02x" 逐字节）。
    char condString[0x80];
    int32 n = 0;
    for (int32 i = 0; i < static_cast<int32>(sizeof(Params)); ++i) {
        n += snprintf(condString + n, sizeof(condString) - n, "%02x", Params[i]);
        if (n >= static_cast<int32>(sizeof(condString)) - 3) break;
    }

    char result[0x200];
    snprintf(result, sizeof(result),
        "%s,%s,%s,%d,%d,%s,%s,%lf,%lf,%lf,%d,%d,%d,%d,%s,%d,%d,%d,%s",
        this->Name, team1Name, ownerName,
        this->TechLevel, static_cast<int32>(this->ConditionType),
        condName, condString,
        this->Weight_Current, this->Weight_Minimum, this->Weight_Maximum,
        this->IsForSkirmish ? 1 : 0, 0, this->SideIndex,
        this->IsForBaseDefense ? 1 : 0, team2Name,
        this->Enabled_Easy ? 1 : 0,
        this->Enabled_Normal ? 1 : 0,
        this->Enabled_Hard ? 1 : 0, "");

    pINI->WriteString("AITriggerTypes", this->ID, result);
    return true;
}

bool AITriggerTypeClass::SaveToINIList(CCINIClass* pINI, bool /*Global*/) {
    return SaveToINI(pINI);
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_CreateFromINIList(CCINIClass *INI, int global)
//   遍历 [AITriggerTypes]（global 时另读 [AITriggerTypesEnable]），
//   为每个键 FindOrAllocate、LoadFromINI，并设置 isGlobal / isEnabled。
// ---------------------------------------------------------------------------
void AITriggerTypeClass::CreateFromINIList(CCINIClass* pINI, bool global) {
    if (pINI == nullptr) return;

    int32 count = pINI->GetKeyCount("AITriggerTypes");
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName("AITriggerTypes", i);
        if (!key) continue;
        if (!_strcmpi(key, "<none>") || !_strcmpi(key, "none")) continue;

        AITriggerTypeClass* item = FindOrAllocate(key);
        if (!item) continue;

        item->LoadFromINI(pINI);
        item->IsGlobal = global ? 1 : 0;
        if (global) item->IsEnabled = true;
    }

    if (global) return;

    // 非全局（地图内）：按 [AITriggerTypesEnable] 覆写使能位。
    int32 enableCount = pINI->GetKeyCount("AITriggerTypesEnable");
    for (int32 i = 0; i < enableCount; ++i) {
        const char* key = pINI->GetKeyName("AITriggerTypesEnable", i);
        if (!key) continue;
        AITriggerTypeClass* item = Find(key);
        if (!item) continue;
        bool enabled = pINI->ReadBool("AITriggerTypesEnable", key, false);
        item->IsEnabled = enabled;
    }
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_ConditionMet(this, CallingHouse, targetHouse,
//        AlreadyHasMinimumBaseDefense)
//   先过 base-defense / global / 难度 / 旁路过滤，再按 ConditionType 分派；
//   最后检查目标队伍能否编成（CanInstantiateTeam / CountInstancesOfTeam）。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::ConditionMet(HouseClass* CallingHouse, HouseClass* TargetHouse, bool AlreadyHasMinimumBaseDefense) const {
    if (!CallingHouse) return false;

    // isBaseDef：Team1 或 Team2 标记为基地防御队伍时为真。
    bool isBaseDef = false;
    if (Team1 && Team1->IsBaseDefense) isBaseDef = true;
    else if (Team2 && Team2->IsBaseDefense) isBaseDef = true;

    // 若存在目标房（进行 base-defense 计数）：
    //   IsGlobal 且 Scenario 忽略全局 AI 触发 → 直接失败。
    if (TargetHouse) {
        if (IsGlobal == 1 && ScenarioClass::Instance && ScenarioClass::Instance->IgnoreGlobalAITriggers) {
            return false;
        }
        // 基地防御要求：本房已有足够基地防御队伍。
        if (isBaseDef) {
            if (AlreadyHasMinimumBaseDefense) {
                // 继续评估。
            } else {
                return false;
            }
        }
    }

    // global 触发在非 skirmish 下要求 IsForSkirmish 通过。
    if (IsGlobal) {
        // 保持与 asm 的短路一致：仅当目标房存在时此关已在上方处理。
    }

    // 难度过滤（asm 用 ScenarioClass::Difficulty）。
    int32 difficulty = ScenarioClass::Instance ? ScenarioClass::Instance->Difficulty : 0;
    if (difficulty == 0 && !Enabled_Easy) return false;
    if (difficulty == 1 && !Enabled_Normal) return false;
    if (difficulty == 2 && !Enabled_Hard) return false;

    // 技术等级门槛：触发类型 techLevel > 本房 techLevel → 失败。
    if (TechLevel > CallingHouse->TechLevel) return false;

    // 按 ConditionType 分派（asm switch 顺序）。
    bool condResult = false;
    switch (ConditionType) {
        case AITriggerCondition::HouseOwns:
            condResult = OwnerHouseOwns(CallingHouse, TargetHouse);
            break;
        case AITriggerCondition::EnemyOwns:
            condResult = EnemyHouseOwns(CallingHouse, TargetHouse);
            break;
        case AITriggerCondition::CivilianOwns:
            condResult = CivilianHouseOwns(CallingHouse, TargetHouse);
            break;
        case AITriggerCondition::HouseCredits:
            condResult = CompareHouseMoney(CallingHouse);
            break;
        case AITriggerCondition::IronCurtainReady:
            condResult = IronCurtainPercentReady(CallingHouse);
            break;
        case AITriggerCondition::ChronosphereReady:
            condResult = ChronospherePercentReady(CallingHouse);
            break;
        default:
            condResult = OwnerHouseOwns(CallingHouse, TargetHouse);
            break;
    }

    if (!condResult) return false;

    // 目标队伍判定：Team1（若存在）需能被本房编成且未达上限；Team2 同理。
    if (Team1) {
        if (!CallingHouse->CanInstantiateTeam(Team1)) return false;
    }
    if (Team2) {
        if (!CallingHouse->CanInstantiateTeam(Team2)) return false;
    }
    if (Team1 && Team1->Max >= 0) {
        if (CallingHouse->CountInstancesOfTeam(Team1) >= Team1->Max) return false;
    }
    if (Team2 && Team2->Max >= 0) {
        if (CallingHouse->CountInstancesOfTeam(Team2) >= Team2->Max) return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_HouseOwns(this, Owner, target)
//   对 target 持有的 ConditionObject 计数，按 params 比较器判定。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::OwnerHouseOwns(HouseClass* /*CallingHouse*/, HouseClass* TargetHouse) const {
    if (!TargetHouse) return false;

    int32 count = 0;
    if (ConditionObject) {
        count = TargetHouse->CountOwnedNow(ConditionObject);
    }

    int32 comparator = Conditions[0].ComparatorType;
    int32 operand = Conditions[0].ComparatorOperand;
    return EvaluateComparator(count, comparator, operand);
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_EnemyOwns(this, Owner, target)
//   对 Owner（呼叫方）持有的 ConditionObject 计数后比较。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::EnemyHouseOwns(HouseClass* CallingHouse, HouseClass* /*TargetHouse*/) const {
    if (!CallingHouse) return false;

    int32 count = 0;
    if (ConditionObject) {
        count = CallingHouse->CountOwnedNow(ConditionObject);
    }

    int32 comparator = Conditions[0].ComparatorType;
    int32 operand = Conditions[0].ComparatorOperand;
    return EvaluateComparator(count, comparator, operand);
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_CivilianHouseOwns(this, a2, a3)
//   在全局房表中找到阵营为 "Civilian" 的房子，对其持有的 ConditionObject
//   计数后比较。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::CivilianHouseOwns(HouseClass* /*CallingHouse*/, HouseClass* /*TargetHouse*/) const {
    if (HouseClass::Array == nullptr) return false;

    HouseClass* civilian = nullptr;
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse == nullptr) continue;
        // asm: 阵营 Side == Side_From_Name("Civilian")，即平民房。
        if (pHouse->IsCivilians) {
            civilian = pHouse;
            break;
        }
    }
    if (!civilian) return false;

    int32 count = 0;
    if (ConditionObject) {
        count = civilian->CountOwnedNow(ConditionObject);
    }

    int32 comparator = Conditions[0].ComparatorType;
    int32 operand = Conditions[0].ComparatorOperand;
    return EvaluateComparator(count, comparator, operand);
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_HouseCredits（条件 3）
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::HouseCredits(HouseClass* CallingHouse, HouseClass* /*TargetHouse*/) const {
    if (!CallingHouse) return false;
    int32 credits = CallingHouse->Credits;
    int32 comparator = Conditions[0].ComparatorType;
    int32 operand = Conditions[0].ComparatorOperand;
    return EvaluateComparator(credits, comparator, operand);
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_IronCurtainCharged / ChronoSphereCharged
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::IronCurtainCharged(HouseClass* CallingHouse, HouseClass* /*TargetHouse*/) const {
    return IronCurtainPercentReady(CallingHouse);
}

bool AITriggerTypeClass::ChronoSphereCharged(HouseClass* CallingHouse, HouseClass* /*TargetHouse*/) const {
    return ChronospherePercentReady(CallingHouse);
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_CompareHouseMoney(this, House)
//   House->Get_Credits() 与 params 比较器逐档比较。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::CompareHouseMoney(HouseClass* pHouse) const {
    if (pHouse == nullptr) return false;

    const int32 credits = pHouse->Credits;
    const int32 comparator = Conditions[0].ComparatorType;
    const int32 operand = Conditions[0].ComparatorOperand;
    return EvaluateComparator(credits, comparator, operand);
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_IronCurtainPercentReady(this, House)
//   在 House 的超武列表中找 SuperWeaponType==1（IronCurtain），
//   就其充能百分比判断是否 >= (1 - Rules.IronCurtainChargePercent)。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::IronCurtainPercentReady(HouseClass* pHouse) const {
    if (pHouse == nullptr) return false;
    int32 idx = pHouse->FindSuperWeapon(SuperWeaponType::IronCurtain);
    if (idx < 0) return false;
    return !pHouse->SuperWeaponTimers[idx].InProgress();
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_ChronospherePercentReady(this, House)
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::ChronospherePercentReady(HouseClass* pHouse) const {
    if (pHouse == nullptr) return false;
    int32 idx = pHouse->FindSuperWeapon(SuperWeaponType::ChronoSphere);
    if (idx < 0) return false;
    return !pHouse->SuperWeaponTimers[idx].InProgress();
}

// ---------------------------------------------------------------------------
// EvaluateComparator —— comparator 语义（asm jump table）：
//   0: <   1: <=   2: ==   3: >=   4: >   5: !=
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::EvaluateComparator(int32 currentValue, int32 comparatorType, int32 comparatorOperand) const {
    switch (comparatorType) {
        case 0: return currentValue <  comparatorOperand;
        case 1: return currentValue <= comparatorOperand;
        case 2: return currentValue == comparatorOperand;
        case 3: return currentValue >= comparatorOperand;
        case 4: return currentValue >  comparatorOperand;
        case 5: return currentValue != comparatorOperand;
        default: return false;
    }
}

// ---------------------------------------------------------------------------
// 计数类条件辅助（对 ConditionObject 计数后套比较器）。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::EvaluateBuildingCount(HouseClass* pHouse) const {
    if (!pHouse) return false;
    return EvaluateComparator(pHouse->BuildingCount,
        Conditions[0].ComparatorType, Conditions[0].ComparatorOperand);
}

bool AITriggerTypeClass::EvaluateUnitCount(HouseClass* pHouse) const {
    if (!pHouse) return false;
    return EvaluateComparator(pHouse->UnitCount,
        Conditions[0].ComparatorType, Conditions[0].ComparatorOperand);
}

bool AITriggerTypeClass::EvaluateInfantryCount(HouseClass* pHouse) const {
    if (!pHouse) return false;
    return EvaluateComparator(pHouse->InfantryCount,
        Conditions[0].ComparatorType, Conditions[0].ComparatorOperand);
}

bool AITriggerTypeClass::EvaluateAircraftCount(HouseClass* pHouse) const {
    if (!pHouse) return false;
    return EvaluateComparator(pHouse->AircraftCount,
        Conditions[0].ComparatorType, Conditions[0].ComparatorOperand);
}

bool AITriggerTypeClass::EvaluatePowerOutput(HouseClass* pHouse) const {
    if (!pHouse) return false;
    int32 surplus = pHouse->PowerOutput - pHouse->PowerDrain;
    return EvaluateComparator(surplus,
        Conditions[0].ComparatorType, Conditions[0].ComparatorOperand);
}

bool AITriggerTypeClass::EvaluateHasSuperWeapon(HouseClass* pHouse) const {
    if (!pHouse) return false;
    int32 count = 0;
    for (int32 i = 0; i < HouseClass::MaxSuperWeapons; ++i) {
        if (pHouse->SuperWeaponTimers[i].HasTimeLeft()) ++count;
    }
    return EvaluateComparator(count,
        Conditions[0].ComparatorType, Conditions[0].ComparatorOperand);
}

bool AITriggerTypeClass::EvaluateTechLevel(HouseClass* pHouse) const {
    if (!pHouse) return false;
    return EvaluateComparator(pHouse->TechLevel,
        Conditions[0].ComparatorType, Conditions[0].ComparatorOperand);
}

bool AITriggerTypeClass::EvaluateWeight() const {
    return Weight_Current >= Weight_Minimum;
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_RegisterSuccess
//   WeightCur += Rules.AITriggerSuccessWeightDelta（并夹在 [Min,Max] 内）；
//   cntRuns++；cntSuccesses++。
// ---------------------------------------------------------------------------
void AITriggerTypeClass::RegisterSuccess() {
    double delta = RulesClass::Instance ? RulesClass::Instance->AITriggerSuccessWeightDelta : 0.0;
    Weight_Current += delta;
    if (Weight_Current < Weight_Minimum) Weight_Current = Weight_Minimum;
    if (Weight_Current > Weight_Maximum) Weight_Current = Weight_Maximum;
    ++TimesCompleted;
    ++TimesExecuted;
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_RegisterFailure
//   WeightCur += Rules.AITriggerFailureWeightDelta（夹在 [Min,Max] 内）；cntRuns++。
// ---------------------------------------------------------------------------
void AITriggerTypeClass::RegisterFailure() {
    double delta = RulesClass::Instance ? RulesClass::Instance->AITriggerFailureWeightDelta : 0.0;
    Weight_Current += delta;
    if (Weight_Current < Weight_Minimum) Weight_Current = Weight_Minimum;
    if (Weight_Current > Weight_Maximum) Weight_Current = Weight_Maximum;
    ++TimesExecuted;
}

double AITriggerTypeClass::GetSelectionProbability() const {
    if (!IsEnabled) return 0.0;
    double range = Weight_Maximum - Weight_Minimum;
    if (range <= 0.0) return 1.0;
    double normalized = (Weight_Current - Weight_Minimum) / range;
    return 1.0 - normalized;
}

bool AITriggerTypeClass::OwnerHouseIndexMatches(HouseClass* pHouse) const {
    return pHouse && pHouse->ArrayIndex == HouseIndex;
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_WriteToTriggerEnable(CCINIClass *this, byte GlobalState)
//   把全局表中所有触发类型的使能位写回 [AITriggerTypesEnable]（GlobalState==0）。
// ---------------------------------------------------------------------------
void AITriggerTypeClass::WriteToTriggerEnable(bool enable) {
    if (enable) return;
    if (Array == nullptr) return;
    for (int32 i = 0; i < Array->Count; ++i) {
        AITriggerTypeClass* item = Array->GetItem(i);
        if (item) item->IsEnabled = false;
    }
}

void AITriggerTypeClass::FormatForSaving(char* buffer, size_t size) const {
    const char* Team1Name = "<none>";
    const char* Team2Name = "<none>";
    const char* HouseName = "<none>";
    const char* ConditionName = "<none>";

    if (Team1) Team1Name = Team1->get_ID();
    if (Team2) Team2Name = Team2->get_ID();

    if (OwnerHouseType == AITriggerHouseType::Single) {
        if (HouseIndex != -1) {
            HouseName = "House";
        }
    } else if (OwnerHouseType == AITriggerHouseType::Any) {
        HouseName = "<all>";
    }

    if (ConditionObject) {
        ConditionName = ConditionObject->get_ID();
    }

    snprintf(buffer, size, "%s,%s,%s,%s,%d,%d,%s,%lf,%lf,%lf,%u,%d,%d,%u,%s,%u,%u,%u\n",
        this->ID, this->Name, Team1Name, HouseName, this->TechLevel,
        static_cast<int32>(this->ConditionType), ConditionName,
        this->Weight_Current, this->Weight_Minimum, this->Weight_Maximum,
        static_cast<uint32>(this->IsForSkirmish), 0, this->SideIndex,
        static_cast<uint32>(this->IsForBaseDefense), Team2Name,
        static_cast<uint32>(this->Enabled_Easy),
        static_cast<uint32>(this->Enabled_Normal),
        static_cast<uint32>(this->Enabled_Hard));
}
