#include "AITriggerClass.h"
#include "AITriggerTypeClass.h"
#include "TeamTypeClass.h"
#include "../Houses/HouseClass.h"

#include <cstring>

// ============================================================================
// AITriggerClass.cpp —— AI 触发器运行期视图 / 适配层实现。
//
// ★ asm 依据（见 AITriggerClass.h 头部注释）：
//   反汇编中无 `AITriggerClass` 符号。触发器的运行期状态与评估逻辑都挂在
//   `AITriggerTypeClass` 上（WeightCur / cntSuccesses / cntRuns / Team1 / Team2
//   + `AITriggerTypeClass_ConditionMet`），评估入口是
//   `HouseClass_TryFindEligibleAITriggers`。
//
//   故本文件不再自持任何运行期状态，全部转发到持有的 `AITriggerTypeClass*`。
// ============================================================================

DynamicVectorClass<AITriggerClass*>* AITriggerClass::Array = nullptr;

// ============================================================================
// 构造 / 析构
// ============================================================================

AITriggerClass::AITriggerClass(AITriggerTypeClass* pType) noexcept
    : AbstractClass(noinit), Type(pType), OwnerHouse(nullptr) {
}

AITriggerClass::~AITriggerClass() {
    Type = nullptr;
    OwnerHouse = nullptr;
}

// ============================================================================
// RTTI / 尺寸
// ============================================================================

AbstractType AITriggerClass::WhatAmI() const {
    return AbstractType::AITrigger;
}

int32 AITriggerClass::Size() const {
    return sizeof(AITriggerClass);
}

// ============================================================================
// 转发到 AITriggerTypeClass
// ============================================================================

// asm: 评估主体是 AITriggerTypeClass_ConditionMet(this, CallingHouse,
//      targetHouse, AlreadyHasMinimumBaseDefense)。适配层给
//      AlreadyHasMinimumBaseDefense 传 true（该 gate 由
//      HouseClass_TryFindEligibleAITriggers 在调用前算好）。
bool AITriggerClass::Is_Satisfied(HouseClass* pHouse) const {
    if (!Type) return false;
    if (!Type->IsEnabled) return false;
    if (!pHouse) return false;
    return Type->ConditionMet(pHouse, pHouse, true);
}

// asm: AITriggerTypeClass.WeightCur（+0xB8，double）。
double AITriggerClass::Get_Current_Weight() const {
    return Type ? Type->Weight_Current : 0.0;
}

// asm: AITriggerTypeClass.cntSuccesses（+0x104）。
int32 AITriggerClass::Get_Times_Completed() const {
    return Type ? Type->TimesCompleted : 0;
}

// asm: AITriggerTypeClass.cntRuns（+0x108）。
int32 AITriggerClass::Get_Times_Triggered() const {
    return Type ? Type->TimesExecuted : 0;
}

bool AITriggerClass::Is_Enabled() const {
    return Type ? Type->IsEnabled : false;
}

void AITriggerClass::Set_Enabled(bool enabled) {
    if (Type) Type->IsEnabled = enabled;
}

// asm: AITriggerTypeClass_RegisterSuccess —— WeightCur += Rules 的成功增量，
//      夹在 [WeightMin, WeightMax]；cntSuccesses++、cntRuns++。
void AITriggerClass::RegisterSuccess() {
    if (Type) Type->RegisterSuccess();
}

// asm: AITriggerTypeClass_RegisterFailure —— WeightCur += Rules 的失败增量，
//      夹在 [WeightMin, WeightMax]；cntRuns++。
void AITriggerClass::RegisterFailure() {
    if (Type) Type->RegisterFailure();
}

// asm: AITriggerTypeClass.Team1（+0xDC，指向 TeamTypeClass）。
TeamTypeClass* AITriggerClass::Get_Team_Type() const {
    return Type ? Type->Team1 : nullptr;
}

// asm: AITriggerTypeClass.Team2（+0xE0，指向 TeamTypeClass）。
TeamTypeClass* AITriggerClass::Get_Secondary_Team_Type() const {
    return Type ? Type->Team2 : nullptr;
}

// 触发成功走与原版一致的登记路径。实际建队在
// HouseClass_TryFindEligibleAITriggers 里完成（把 Team1/Team2 打入
// AITeamTypeClass.Autocreate 队列），本层只登记成功反馈。
void AITriggerClass::Fire() {
    if (Type) Type->RegisterSuccess();
}

// ============================================================================
// RTTI / 序列化（补齐 AbstractClass 的纯虚：GetClassID / Load / Save）
//
// 原版的运行期状态全在 AITriggerTypeClass 上，本适配层只持有"哪个类型 +
// 最近评估它的 owner"，故序列化也只落这两样。
// ============================================================================

// asm: AITriggerTypeClass_GetClassID (asm:107933) —— 回填本类的 CLSID。
HRESULT AITriggerClass::GetClassID(CLSID* pClassID) {
    if (pClassID == nullptr) return E_POINTER;
    pClassID->Data1 = 0x41495452;   // 'AITR'
    pClassID->Data2 = 0x6767;
    pClassID->Data3 = 0x6767;
    for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0;
    return S_OK;
}

// asm: AITriggerTypeClass_Load (asm:107967) —— 读回类型名与 owner 下标。
HRESULT AITriggerClass::Load(IStream* pStm) {
    if (pStm == nullptr) return E_POINTER;

    ULONG read = 0;

    // 类型 ID（[AITriggerTypes] 段里的名字）。
    char typeID[0x18];
    HRESULT hr = pStm->Read(typeID, sizeof(typeID), &read);
    if (hr < 0 || read != sizeof(typeID)) return E_FAIL;
    typeID[sizeof(typeID) - 1] = '\0';
    Type = typeID[0] ? AITriggerTypeClass::Find(typeID) : nullptr;

    // 最近评估它的 owner 下标。
    int32 ownerIdx = -1;
    hr = pStm->Read(&ownerIdx, sizeof(ownerIdx), &read);
    if (hr < 0 || read != sizeof(ownerIdx)) return E_FAIL;
    OwnerHouse = (ownerIdx >= 0) ? HouseClass::GetHouseByIndex(ownerIdx) : nullptr;

    return S_OK;
}

// asm: AITriggerTypeClass_Save (asm:108027) —— 回写类型名与 owner 下标。
HRESULT AITriggerClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (pStm == nullptr) return E_POINTER;

    ULONG written = 0;

    char typeID[0x18];
    std::memset(typeID, 0, sizeof(typeID));
    if (Type != nullptr && Type->ID != nullptr) {
        int32 j = 0;
        while (Type->ID[j] != '\0' && j < static_cast<int32>(sizeof(typeID)) - 1) {
            typeID[j] = Type->ID[j];
            ++j;
        }
    }
    HRESULT hr = pStm->Write(typeID, sizeof(typeID), &written);
    if (hr < 0 || written != sizeof(typeID)) return E_FAIL;

    int32 ownerIdx = OwnerHouse ? OwnerHouse->ArrayIndex : -1;
    hr = pStm->Write(&ownerIdx, sizeof(ownerIdx), &written);
    if (hr < 0 || written != sizeof(ownerIdx)) return E_FAIL;

    if (fClearDirty) {
        Dirty = false;
    }
    return S_OK;
}

// ============================================================================
// Evaluate —— 显式透传 baseDefense 的判定入口。
//
// asm: AITriggerTypeClass_ConditionMet (asm:108190)：先看 Team1/Team2 是否
//   IsBaseDefense，再据此决定是否用 AlreadyHasMinimumBaseDefense 门；随后是
//   isGlobal / Scenario.IgnoreGlobalAITriggers / OwnerHouseType 等一串前置门，
//   最后分派到 HouseOwns / EnemyOwns / HouseCredits 等条件分支。
//  这里把前置门（IsEnabled）与条件判定串起来，参数不再硬编码。
// ============================================================================
bool AITriggerClass::Evaluate(HouseClass* pHouse, HouseClass* pTarget,
                              bool AlreadyHasMinimumBaseDefense) const {
    if (Type == nullptr) return false;
    if (!Type->IsEnabled) return false;
    if (pHouse == nullptr) return false;
    if (pTarget == nullptr) pTarget = pHouse;
    return Type->ConditionMet(pHouse, pTarget, AlreadyHasMinimumBaseDefense);
}

// ============================================================================
// EvaluateWeight —— 判定 + 返回权重。
//
// 条件不成立返回 0；成立则返回 WeightCur（原版据此做加权随机挑选）。
// ============================================================================
double AITriggerClass::EvaluateWeight(HouseClass* pHouse, HouseClass* pTarget,
                                      bool AlreadyHasMinimumBaseDefense) const {
    if (!Evaluate(pHouse, pTarget, AlreadyHasMinimumBaseDefense)) {
        return 0.0;
    }
    return Type->Weight_Current;
}

// ============================================================================
// Update —— 单条触发器的运行期评估 tick。
//
// 复刻 HouseClass_TryFindEligibleAITriggers (asm:1543745) 循环体对一条触发器
// 的处理：先过 TechLevel / WeightMin 两道门，再跑 ConditionMet；命中即
// RegisterSuccess()（权重 += Rules.AITriggerSuccessWeightDelta，夹在
// [WeightMin,WeightMax]，cntSuccesses++ / cntRuns++）并返回真；未命中则
// RegisterFailure()（权重 += Rules.AITriggerFailureWeightDelta，cntRuns++）。
// ============================================================================
bool AITriggerClass::Update(HouseClass* pHouse, HouseClass* pTarget,
                            bool AlreadyHasMinimumBaseDefense) {
    if (Type == nullptr) return false;
    if (!Type->IsEnabled) return false;
    if (pHouse == nullptr) return false;
    if (pHouse->TechLevel < Type->TechLevel) return false;
    if (Type->Weight_Current < Type->Weight_Minimum) return false;

    if (Evaluate(pHouse, pTarget, AlreadyHasMinimumBaseDefense)) {
        Type->RegisterSuccess();
        return true;
    }

    Type->RegisterFailure();
    return false;
}
