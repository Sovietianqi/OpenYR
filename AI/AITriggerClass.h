#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Abstract/AbstractClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class AITriggerTypeClass;
class HouseClass;
class TeamTypeClass;

// ============================================================================
// AITriggerClass —— AI 触发器的“运行期视图 / 适配层”。
//
// ★ asm 事实（重要）：
//   完整反汇编（gamemd.exe.asm）中**不存在**名为 `AITriggerClass` 的符号或
//   proc。原版把一条 AI 触发器的运行期状态（当前权重 WeightCur、执行次数
//   cntRuns、成功次数 cntSuccesses）**直接内嵌在 `AITriggerTypeClass` 对象
//   上**（结构定义见 asm `AITriggerTypeClass struc ; sizeof=0x110`：
//   +0xB8 WeightCur / +0xC0 WeightMin / +0xC8 WeightMax / +0x104 cntSuccesses
//   / +0x108 cntRuns）。运行期的评估入口是
//   `HouseClass_TryFindEligibleAITriggers`，它遍历全局 `vec_AITriggerTypes`
//   并对每个 `AITriggerTypeClass` 调用 `AITriggerTypeClass_ConditionMet`，再按
//   `WeightCur` 做加权随机挑选，取出 `Team1` / `Team2` 建队。
//
//   因此本类不是原版的对象，而是把“一条触发器的运行期视图”收编成的**薄适配
//   层**：它持有一个 `AITriggerTypeClass*`，所有查询都转发到该类型对象，运行
//   期状态不在此重复存储（避免与 TypeClass 出现两份真相）。
//   这符合并行简报第六节“命名漂移收编”的第三步：旧名降级为适配层。
// ============================================================================
class NOVTABLE AITriggerClass : public AbstractClass {
public:
    static const AbstractType AbsID = AbstractType::AITrigger;

    // 触发器的“运行实例”就是其类型对象本身；此列表仅用于登记，便于遍历。
    static DynamicVectorClass<AITriggerClass*>* Array;

    explicit AITriggerClass(AITriggerTypeClass* pType = nullptr) noexcept;
    explicit AITriggerClass(noinit_t) noexcept : AbstractClass(noinit) {}
    virtual ~AITriggerClass();

    // asm: 原版运行期由 `AITriggerTypeClass` 的 vftable 槽位承担 ——
    //   GetClassID (+0x1C of vt_AbstractClass) / Load (+0x14) / Save (+0x18)。
    //   适配层据此把"触发器的运行期视图"接上序列化链（否则本类因这三个纯虚
    //   未实现而是抽象类，永远无法实例化）。
    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    // ---- 转发到 AITriggerTypeClass 的运行期查询 ----------------------------
    AITriggerTypeClass* Get_Type() const { return Type; }

    // 触发条件是否对给定 owner 成立（转调 Type->ConditionMet）。
    // 第二参数为目标房；缺省时以 owner 自身为目标房。
    bool Is_Satisfied(HouseClass* pHouse) const;

    // asm: AITriggerTypeClass_ConditionMet(this, CallingHouse, targetHouse,
    //      AlreadyHasMinimumBaseDefense) —— 判定入口，参数显式透传（不再把
    //      baseDefense 硬编码成 true）。
    bool Evaluate(HouseClass* pHouse, HouseClass* pTarget,
                  bool AlreadyHasMinimumBaseDefense) const;

    // 判定 + 返回权重：条件成立时返回本触发器的当前权重 WeightCur，否则 0。
    // asm: HouseClass_TryFindEligibleAITriggers 用 `[esi+WeightCur]` 取权重做
    //      DiscreteDistribution 加权随机挑选（asm:1543745 附近）。
    double EvaluateWeight(HouseClass* pHouse, HouseClass* pTarget,
                          bool AlreadyHasMinimumBaseDefense) const;

    // 单条触发器的运行期评估 tick：等价于
    // HouseClass_TryFindEligibleAITriggers 循环体里对一条触发器的处理 ——
    // 通过 TechLevel / WeightMin 门后跑 ConditionMet；命中则 RegisterSuccess()
    // （权重 +Rules 成功增量、cntSuccesses++、cntRuns++）并返回真，否则
    // RegisterFailure()（权重 +Rules 失败增量、cntRuns++）并返回假。
    bool Update(HouseClass* pHouse, HouseClass* pTarget,
                bool AlreadyHasMinimumBaseDefense);

    // 当前权重（asm: AITriggerTypeClass.WeightCur，double）。
    double Get_Current_Weight() const;

    // 成功次数 / 执行次数（asm: cntSuccesses / cntRuns）。
    int32 Get_Times_Completed() const;
    int32 Get_Times_Triggered() const;

    bool Is_Enabled() const;

    // 触发成功 / 失败：把权重按 Rules 的 delta 增减，并累加计数。
    // 直接转调 Type->RegisterSuccess / RegisterFailure。
    void RegisterSuccess();
    void RegisterFailure();

    // 主 / 次队伍类型（asm: AITriggerTypeClass.Team1 / Team2）。
    TeamTypeClass* Get_Team_Type() const;
    TeamTypeClass* Get_Secondary_Team_Type() const;

    // 运行实例所归属的阵营。原版一个触发器类型是全局共享的，并未绑定单一
    // 阵营；此处仅记录最近一次评估它的 owner，供调用方引用。
    HouseClass* Get_Owner_House() const { return OwnerHouse; }
    void Set_Owner_House(HouseClass* pHouse) { OwnerHouse = pHouse; }

    // 该触发器类型是否被全局使能（asm: AITriggerTypeClass.isEnabled）。
    // 旧接口名保留：Set_Enabled 直接写回类型对象的使能位。
    void Set_Enabled(bool enabled);
    bool Is_On_Cooldown() const { return false; }

    // 兼容旧接口名的空操作占位：原版无独立的冷却计时器，节流由
    // HouseClass_TryFindEligibleAITriggers 的 AITriggerTimer / RatioAITriggerTeam
    // 控制，不在本层。
    void Set_Cooldown(int32) {}
    void Reset() { OwnerHouse = nullptr; }

    // 转发到类型的公共入口：RegisterSuccess。
    void Fire();

    AITriggerTypeClass* Type;
    HouseClass*         OwnerHouse;
};
