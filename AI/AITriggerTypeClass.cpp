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
#include "../Abstract/BuildingClass.h"
#include "../Houses/HouseClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Rules/RulesClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"
#include "../Core/SwizzleManagerClass.h"
#include "../Game/Game.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace {

// ── OPENYR_TRIGDBG=1 时的条件判定探针（默认关闭）────────────────────────
//   每 300 帧一个窗口，每窗口最多 20 行，避免刷爆 1200 帧的日志。
bool TrigDebugOn() {
    static const bool on = (std::getenv("OPENYR_TRIGDBG") != nullptr);
    return on;
}

int32 s_trigDbgBucket = -1;
int32 s_trigDbgLines  = 0;

void TraceTriggerCondition(const AITriggerTypeClass* pTrigger, int32 condType,
                           int32 comparator, int32 operand, int32 measured,
                           const void* pObject, bool result) {
    if (!TrigDebugOn()) return;

    const int32 bucket = Game::CurrentFrame / 300;
    if (bucket != s_trigDbgBucket) {
        s_trigDbgBucket = bucket;
        s_trigDbgLines  = 0;
    }
    if (s_trigDbgLines >= 20) return;
    ++s_trigDbgLines;

    // 走 stderr：与项目其余诊断开关（OPENYR_AIDBG 等）一致，不污染 stdout 的
    //   正常统计行。
    std::fprintf(stderr, "[TRIG] trig=%s cond=%d cmp=%d op=%d count=%d obj=%p result=%d\n",
                 pTrigger ? pTrigger->get_ID() : "?",
                 condType, comparator, operand, measured, pObject, result ? 1 : 0);
}

}  // namespace

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
    : AbstractTypeClass(pID), ConditionType(AITriggerCondition::None),
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
    // asm: INIClass_GetString(ini, "AITriggerTypes", this->ID [+0x24], ...)
    //   —— 查表用的键是**对象的 ID**（形如 "0x01000001"），不是显示名。
    //   原实现用 this->Name 查表，而 Name 此时还是空的（它就是被读出来的
    //   第一字段），于是每一条触发器都读不到自己的行。
    if (pINI->ReadString("AITriggerTypes", this->ID, "", buffer, sizeof(buffer)) <= 0) {
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

    // 5. ConditionObject（asm 依次查 Infantry / Unit / Aircraft / Building 四张表）
    //
    //   ★ 原实现只查了步兵表 + 一个不存在的 TechnoTypeClass::Find 兜底，
    //     于是 [AITriggerTypes] 里最常见的条件对象（如 GAWEAP 这样的建筑）
    //     一律解析成 nullptr，HouseOwns 计数恒 0、条件恒不成立。
    tok = strtok_r(nullptr, ",", &ctx);
    if (!tok) return false;
    {
        char name[0x18];
        strncpy(name, tok, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';

        ConditionObject = InfantryTypeClass::Find(name);
        if (ConditionObject == nullptr)
            ConditionObject = UnitTypeClass::Find(name);
        if (ConditionObject == nullptr)
            ConditionObject = AircraftTypeClass::Find(name);
        if (ConditionObject == nullptr)
            ConditionObject = BuildingTypeClass::Find(name);
    }

    // 6. params 十六进制串（asm 逐两字符解码一字节，最多 32 字节）
    //
    //   ★ 解码必须严格"两个字符一字节"：原先用 strtoul 一次吞掉整个串，
    //     "0400" 会被读成 0x400 → 只落一个 0x00，比较器与操作数全丢，
    //     随后 Conditions[0] 保持 {0,0} 使每条条件都判 false。
    tok = strtok_r(nullptr, ",", &ctx);
    if (!tok) return false;
    {
        std::memset(Params, 0, sizeof(Params));
        int32 n = 0;
        for (const char* p = tok; *p && n < static_cast<int32>(sizeof(Params)); ) {
            while (*p == ' ' || *p == '\t') ++p;
            if (!*p) break;
            char pair[3] = { p[0], (p[1] != '\0') ? p[1] : '0', '\0' };
            Params[n++] = static_cast<uint8>(strtoul(pair, nullptr, 16));
            p += (p[1] != '\0') ? 2 : 1;
        }
    }

    // params 逐字节落在 +0xE4 起的 32 字节缓冲（asm LoadFromINI 0x41F859
    //   `mov byte ptr [edi+ebp+params.arg], al`，edi = 0..0x1F）。
    //   条件函数读的是两个 **dword**：
    //     0x41EB9B HouseOwns   : mov ecx,[this+params.cond] → 比较器 switch
    //     0x41EB9F             : mov ecx,[this+params.arg]  → 比较操作数
    //     0x41F253 CompareHouseMoney 同上成对出现
    //   params.arg 就是缓冲首地址（+0xE4），params.cond 紧随其后（+0xE8），
    //   于是布局是 4 组 8 字节的 (操作数, 比较器) 对，条件分支只取第 0 组。
    //   ★ 原实现把前两个 **字节** 当比较器/操作数，只能解析 "0400" 这类短串；
    //     按 asm 的 dword 语义，"> 0" 应写成 "0000000004000000"。
    Conditions[0].ComparatorOperand = ComparatorOperand();
    Conditions[0].ComparatorType    = ComparatorType();

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

    // ── 条件分派（asm 0x41E8F0 .. 0x41E9E1）─────────────────────────────
    //   mov eax,[this+98h]        ; eax = ConditionType
    //   test ebx,ebx / jz ...     ; ebx = targetHouse
    //   inc eax / cmp eax,8 / ja default / jmp off_41EACC[eax*4]
    //   跳转表（0x41EACC）：无条件, HouseOwns, EnemyOwns, 电力<100, 电力<0,
    //                       Credits, IronCurtain, Chronosphere, Civilian
    //   即 ConditionType = -1,0,1,2,3,4,5,6,7；≥8 全部走 default(false)。
    //
    //   ★ 现存枚举把 2..5 写作 CivilianOwns/HouseCredits/IronCurtainReady/
    //     ChronosphereReady，与 asm 有两位偏移。GetCRC（0x41E63F）只在
    //     ConditionType ∈ {0,1,4,7} 时把 params 混进 CRC，而真正吃 params 的
    //     分支正是 HouseOwns/EnemyOwns/Credits/Civilian —— 与跳转表交叉一致。
    //     故此处一律按 asm 的数值语义分派，不使用枚举名做判据。
    const int32 condType = static_cast<int32>(ConditionType);
    const int32 comparator = ComparatorType();
    const int32 operand = ComparatorOperand();
    int32 measured = 0;
    bool condResult = false;

    if (TargetHouse == nullptr) {
        // asm 0x41E9??：没有目标房时不进跳转表 —— ConditionType==1 仍走
        //   EnemyOwns(Owner, nullptr)，ConditionType==-1 恒真，其余 false。
        condResult = (condType == 1) ? EnemyHouseOwns(CallingHouse, nullptr, &measured)
                                     : (condType == -1);
    } else {
        switch (condType) {
            case -1:   // asm 跳转表 [0]：无条件，直接进入队伍判定。
                condResult = true;
                break;
            case 0:    // asm 0x41E90F case 1 → AITriggerTypeClass_HouseOwns
                condResult = OwnerHouseOwns(CallingHouse, TargetHouse, &measured);
                break;
            case 1:    // asm 0x41E91D case 2 → AITriggerTypeClass_EnemyOwns
                condResult = EnemyHouseOwns(CallingHouse, TargetHouse, &measured);
                break;
            case 2:    // asm 0x41E92B case 3：PowerOutput - PowerDrain < 100
                condResult = PowerSurplusBelow(TargetHouse, 100, &measured);
                break;
            case 3:    // asm 0x41E965 case 4：PowerOutput - PowerDrain < 0
                condResult = PowerSurplusBelow(TargetHouse, 0, &measured);
                break;
            case 4:    // asm 0x41E9?? case 5 → AITriggerTypeClass_CompareHouseMoney
                condResult = CompareHouseMoney(TargetHouse, &measured);
                break;
            case 5:    // asm case 6 → IronCurtainPercentReady(this, a2=CallingHouse)
                measured = -1;   // 该分支判的是"就绪百分比"，没有计数值
                condResult = IronCurtainPercentReady(CallingHouse);
                break;
            case 6:    // asm case 7 → ChronospherePercentReady(this, a2=CallingHouse)
                measured = -1;
                condResult = ChronospherePercentReady(CallingHouse);
                break;
            case 7:    // asm case 8 → AITriggerTypeClass_CivilianHouseOwns
                condResult = CivilianHouseOwns(CallingHouse, TargetHouse, &measured);
                break;
            default:   // asm 0x41E9E1 default：ConditionType ≥ 8 → 恒 false
                condResult = false;
                break;
        }
    }

    TraceTriggerCondition(this, condType, comparator, operand, measured,
                          ConditionObject, condResult);

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
// 条件对象在某个阵营的持有数。
//   asm HouseOwns 0x41EB00：先 objCondition->WhatAmI()（vtbl+0x2Ch），
//   再用 byte_41EC48 映射到 4 张 CounterClass：
//     AbstractType 7(BuildingType)→OwnedBuildingTypes、40(UnitType)→
//     OwnedVehicleTypes、16(InfantryType)→OwnedInfantryTypes、
//     3(AircraftType)→OwnedAircraftTypes，其余 → 0；objCondition==nullptr → 0。
//   HouseClass::CountOwnedNow 已按同一张表分派，直接复用。
// ---------------------------------------------------------------------------
int32 AITriggerTypeClass::CountConditionObject(const HouseClass* pHouse) const {
    if (pHouse == nullptr || ConditionObject == nullptr) return 0;
    return pHouse->CountOwnedNow(ConditionObject);
}

// ---------------------------------------------------------------------------
// params 的两个 dword。
//   asm 条件分支统一用 `mov ecx,[this+params.cond]` 作比较器（0..5，越界走
//   default→false），用 `mov ecx,[this+params.arg]` 作操作数。params.arg
//   就是 LoadFromINI 逐字节写入的缓冲首地址（+0xE4），params.cond 在 +0xE8。
// ---------------------------------------------------------------------------
int32 AITriggerTypeClass::ComparatorType() const {
    int32 value = 0;
    std::memcpy(&value, Params + 4, sizeof(value));
    return value;
}

int32 AITriggerTypeClass::ComparatorOperand() const {
    int32 value = 0;
    std::memcpy(&value, Params + 0, sizeof(value));
    return value;
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_HouseOwns(this, Owner, target)
//   对 target 持有的 ConditionObject 计数，按 params 比较器判定。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::OwnerHouseOwns(HouseClass* /*CallingHouse*/, HouseClass* TargetHouse, int32* pCountOut) const {
    if (!TargetHouse) return false;

    const int32 count = CountConditionObject(TargetHouse);
    if (pCountOut) *pCountOut = count;
    return EvaluateComparator(count, ComparatorType(), ComparatorOperand());
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_EnemyOwns(this, Owner, target)
//   对 Owner（呼叫方）持有的 ConditionObject 计数后比较（target 未使用）。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::EnemyHouseOwns(HouseClass* CallingHouse, HouseClass* /*TargetHouse*/, int32* pCountOut) const {
    if (!CallingHouse) return false;

    const int32 count = CountConditionObject(CallingHouse);
    if (pCountOut) *pCountOut = count;
    return EvaluateComparator(count, ComparatorType(), ComparatorOperand());
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_CivilianHouseOwns(this, a2, a3)
//   在全局房表中找到阵营为 "Civilian" 的房子，对其持有的 ConditionObject
//   计数后比较。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::CivilianHouseOwns(HouseClass* /*CallingHouse*/, HouseClass* /*TargetHouse*/, int32* pCountOut) const {
    // asm 0x41ECAD: 以 vec_Houses.length 为循环上界；找不到平民房时直接返回
    //   结果变量初值 0（不套比较器）。
    if (HouseClass::ArrayCount <= 0) return false;

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
    if (civilian == nullptr) return false;

    const int32 count = CountConditionObject(civilian);
    if (pCountOut) *pCountOut = count;
    return EvaluateComparator(count, ComparatorType(), ComparatorOperand());
}

// ---------------------------------------------------------------------------
// asm: AITriggerTypeClass_HouseCredits（历史转发，当前无调用方）
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::HouseCredits(HouseClass* CallingHouse, HouseClass* /*TargetHouse*/) const {
    return CompareHouseMoney(CallingHouse);
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
// asm: AITriggerTypeClass_CompareHouseMoney(this, House, arg_4)
//   0x41F1E5: `mov ecx,[eax+IHouse.IHouse] / add eax,24h / call [ecx+18h]`
//   —— 走 IHouse 第 6 个虚槽取 AvailableMoney（= 矿石价值×IncomeMult +
//   Money，见 0x4Fxxxx HouseClass_IHouse_AvailableMoney），再与 params 比较。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::CompareHouseMoney(HouseClass* pHouse, int32* pCountOut) const {
    if (pHouse == nullptr) return false;

    const int32 money = pHouse->IHouse_Available_Money();
    if (pCountOut) *pCountOut = money;
    return EvaluateComparator(money, ComparatorType(), ComparatorOperand());
}

// ---------------------------------------------------------------------------
// asm 0x41E92B（ConditionType 2）/ 0x41E965（ConditionType 3）
//   mov eax,[ebx+24h] / push ebp / call [eax+24h]   ; IHouse vtbl+0x24
//   mov ecx,[ebp]     / push ebp / call [ecx+20h]   ; IHouse vtbl+0x20
//   sub eax,edx / fild / fcomp num_100 | double_0 / 结果 = (差值 < 阈值)
//   槽位 6=AvailableMoney(0x18)、7=AvailableStorage(0x20)、8=PowerOutput
//   (0x20+4)、9=PowerDrain —— 因此两个分支量的是目标阵营的电力盈余，
//   阈值写死在代码里，不读 params（GetCRC 也只在 0/1/4/7 混 params）。
// ---------------------------------------------------------------------------
bool AITriggerTypeClass::PowerSurplusBelow(HouseClass* pHouse, int32 threshold, int32* pCountOut) const {
    if (pHouse == nullptr) return false;

    const int32 surplus = pHouse->PowerOutput - pHouse->PowerDrain;
    if (pCountOut) *pCountOut = surplus;
    return surplus < threshold;
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
