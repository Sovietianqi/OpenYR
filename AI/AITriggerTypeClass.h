#pragma once

#include "../Abstract/AbstractTypeClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"

class HouseClass;
class TeamTypeClass;
class TechnoTypeClass;
class CCINIClass;
class CRCEngine;

// ---------------------------------------------------------------------------
// AITriggerConditionComparator —— 旧版内联比较器结构，保留以兼容尚未迁移的调用方。
// asm 中真正的比较器/操作数是叠加在 params[] 缓冲上按 32 位/字节读取的。
// ---------------------------------------------------------------------------
struct AITriggerConditionComparator {
    int32 ComparatorType;
    int32 ComparatorOperand;
};

// ---------------------------------------------------------------------------
// AITriggerTypeClass —— [AITriggerTypes] 段的一条“触发条件类型”。
//   “当 owner 满足 ConditionType（对 ConditionObject 计数/比较 params）时，
//    把 Team1/Team2 编成队伍推上战场”。
// 对应 asm AITriggerTypeClass（vftable 槽位见 .rdata）。
// ---------------------------------------------------------------------------
class AITriggerTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::AITriggerType;

    static DynamicVectorClass<AITriggerTypeClass*>* Array;

    // asm: AITriggerTypeClass_CreateFromINIList —— 遍历 [AITriggerTypes] 段
    //（global 时另读 [AITriggerTypesEnable]）建表。
    static void CreateFromINIList(CCINIClass* pINI, bool global);

    static AITriggerTypeClass* Find(const char* pID);
    // asm: AITriggerTypeClass_FindOrAllocate —— 按 name（ID）查表，未命中则新分配。
    static AITriggerTypeClass* FindOrAllocate(const char* pID);

    virtual ~AITriggerTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual int32 GetAbstractDerivationID() const;

    // asm: AITriggerTypeClass_GetCRC —— 基类 CRC 之上再混入触发条件字段。
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // asm: AITriggerTypeClass_LoadFromINI(CCINIClass*) —— 解析单行逗号分隔定义。
    virtual bool LoadFromINI(CCINIClass* pINI) override;
    // asm: AITriggerTypeClass_SaveToINI(CCINIClass*) —— 回写单行逗号分隔定义。
    virtual bool SaveToINI(CCINIClass* pINI) override;

    // 保留旧名做转发（简报规则⑤：命名可漂移，语义需一致）。
    bool LoadFromINIList(CCINIClass* pINI);
    bool SaveToINIList(CCINIClass* pINI, bool Global);

    // asm: AITriggerTypeClass_ConditionMet —— 触发条件总判定。
    bool ConditionMet(HouseClass* CallingHouse, HouseClass* TargetHouse, bool AlreadyHasMinimumBaseDefense) const;
    void RegisterSuccess();
    void RegisterFailure();
    double GetSelectionProbability() const;

    // asm: AITriggerTypeClass_HouseOwns / _EnemyOwns / _CivilianHouseOwns
    //   —— 对目标/自己/平民阵营持有的 ConditionObject 计数并比较。
    //   pCountOut 仅供 OPENYR_TRIGDBG 探针取"本次被测量的原始数值"，默认不写。
    bool OwnerHouseOwns(HouseClass* CallingHouse, HouseClass* TargetHouse, int32* pCountOut = nullptr) const;
    bool CivilianHouseOwns(HouseClass* CallingHouse, HouseClass* TargetHouse, int32* pCountOut = nullptr) const;
    bool EnemyHouseOwns(HouseClass* CallingHouse, HouseClass* TargetHouse, int32* pCountOut = nullptr) const;
    bool HouseCredits(HouseClass* CallingHouse, HouseClass* TargetHouse) const;
    bool IronCurtainCharged(HouseClass* CallingHouse, HouseClass* TargetHouse) const;
    bool ChronoSphereCharged(HouseClass* CallingHouse, HouseClass* TargetHouse) const;

    // asm: AITriggerTypeClass_CompareHouseMoney / _IronCurtainPercentReady /
    //   _ChronospherePercentReady —— 具体条件分支实现。
    bool CompareHouseMoney(HouseClass* pHouse, int32* pCountOut = nullptr) const;
    bool IronCurtainPercentReady(HouseClass* pHouse) const;
    bool ChronospherePercentReady(HouseClass* pHouse) const;

    // asm 0x41E92B / 0x41E965（ConditionType 2 / 3）：目标阵营的
    //   (PowerOutput - PowerDrain) 与固定阈值比较（<100 / <0），不吃 params。
    bool PowerSurplusBelow(HouseClass* pHouse, int32 threshold, int32* pCountOut = nullptr) const;

    // 条件对象在指定阵营的持有数（asm: objCondition->WhatAmI() 分派到
    //   OwnedBuilding/Vehicle/Infantry/Aircraft 四张 CounterClass）。
    int32 CountConditionObject(const HouseClass* pHouse) const;

    // params 的两个 dword（asm: params.arg [+0xE4] 与 params.cond [+0xE8]）。
    int32 ComparatorType() const;
    int32 ComparatorOperand() const;

    // 计数/比较辅助（对 ConditionObject 计数后套用比较器）。
    bool EvaluateBuildingCount(HouseClass* pHouse) const;
    bool EvaluateUnitCount(HouseClass* pHouse) const;
    bool EvaluateInfantryCount(HouseClass* pHouse) const;
    bool EvaluateAircraftCount(HouseClass* pHouse) const;
    bool EvaluatePowerOutput(HouseClass* pHouse) const;
    bool EvaluateHasSuperWeapon(HouseClass* pHouse) const;
    bool EvaluateTechLevel(HouseClass* pHouse) const;
    bool EvaluateWeight() const;
    bool EvaluateComparator(int32 currentValue, int32 comparatorType, int32 comparatorOperand) const;
    bool OwnerHouseIndexMatches(HouseClass* pHouse) const;

    // asm: AITriggerTypeClass_WriteToTriggerEnable —— 场景存档时把使能位写回 INI。
    void WriteToTriggerEnable(bool enable);

    void FormatForSaving(char* buffer, size_t size) const;

    AITriggerTypeClass(const char* pID) noexcept;

protected:
    explicit AITriggerTypeClass(noinit_t) noexcept : AbstractTypeClass(noinit) {}

public:
    // -----------------------------------------------------------------------
    // asm 字段（+0x98 起）：
    //   typeCondition  = ConditionType        （触发条件类别）
    //   isGlobal       = IsGlobal
    //   ownHouseType   = OwnerHouseType       （0 none / 1 指定 / 2 任意）
    //   idxHouse       = HouseIndex
    //   idxSide        = SideIndex            （+0xAC）
    //   techLevel      = TechLevel            （+0xB0，装载后由 TaskForce 推导）
    //   WeightCur/Min/Max                      （+0xB8/+0xC0/+0xC8）
    //   objCondition   = ConditionObject      （+0xD8）
    //   Team1/Team2                            （+0xDC/+0xE0）
    //   params[32]                             （+0xE4，十六进制解码的比较器/操作数）
    //   cntSuccesses   = TimesCompleted       （+0x104）
    //   cntRuns        = TimesExecuted        （+0x108）
    // -----------------------------------------------------------------------
    AITriggerCondition ConditionType;
    int32 IsGlobal;
    AITriggerHouseType OwnerHouseType;
    bool IsEnabled;
    int32 HouseIndex;
    int32 SideIndex;
    int32 TechLevel;
    int32 unknown_B4;
    double Weight_Current;
    double Weight_Minimum;
    double Weight_Maximum;
    bool IsForSkirmish;
    bool IsForBaseDefense;
    bool Enabled_Easy;
    bool Enabled_Normal;
    bool Enabled_Hard;
    TechnoTypeClass* ConditionObject;
    TeamTypeClass* Team1;
    TeamTypeClass* Team2;
    AITriggerConditionComparator Conditions[4];
    // params —— asm 中叠加在同一段缓冲上的“比较器 + 操作数”字节流。
    // [0] 解释为比较器(0..5)，[1..] 为操作数；由 INI 十六进制串解码而来。
    uint8 Params[32];
    int32 TimesExecuted;
    int32 TimesCompleted;
    int32 unknown_10C;
};
