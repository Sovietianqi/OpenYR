#pragma once

#include "../Abstract/AbstractClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"
#include "../Math/Timer.h"
#include "../Math/CoordStruct.h"

class AITeamClass : public AbstractClass {
public:
    static const AbstractType AbsID = AbstractType::AITrigger;

    static DynamicVectorClass<AITeamClass*>* Array;

    virtual ~AITeamClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    // asm: TeamClass_PointerGotInvalid (0x6EAE60) retn 8
    //  (AbstractClass* a2, char a3)：当某个被本队持有的指针对象失效时，
    //  把队里引用它的字段（模板、拥有者、脚本、锚点）清空。
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;

    void Update();

    // ------------------------------------------------------------------------
    // UpdateAll —— 每帧驱动全局 AITeamClass::Array（对应 asm 里 LogicClass_Update
    //  第 7 步的 vec_Teams 循环 L751785：`call [obj+0x5C]`）。原版由
    //  LogicClass_Update 统一驱动，OpenYR 里由 MainLoop 的 Update_AI() 调。
    // ------------------------------------------------------------------------
    static void UpdateAll();

    // asm: TeamClass_Update (asm:1527680) 里"脚本之后"那段每帧动作：
    //  推进脚本、下发使命目标、复位各计数器。语义与 Update 同，供外部单独驱动。
    void AI();

    bool AddMember(TechnoClass* pTechno);
    bool RemoveMember(TechnoClass* pTechno);
    // asm: char __thiscall AITeamClass_FindUnitToFollow(AITeamClass* this,
    //      CellStruct* spawnCell, FootClass** pAnchor)  (0x6EB7xx) retn 8
    //  为队伍确定"跟随锚点"并写回两个出参：spawnCell 收到选中的集结格
    //  （指针目标非空时优先枚举全局单位表，否则按成员链求质心）；pAnchor
    //  收到被选中的跟随单位（找不到则为空）。返回是否选到了锚点。
    bool FindUnitToFollow(CellStruct* spawnCell, FootClass** pAnchor);
    // 无参重载（对应 TeamClass 私有 FindUnitToFollow）：就地刷新本队的
    // 跟随锚点 —— 结果写回 FollowAnchor / AnchorCell 两个字段。
    void FindUnitToFollow();
    void ExecuteScript();
    bool IsTeamFull() const;
    bool IsActive() const { return Active; }
    void SetActive(bool active) { Active = active; }
    void Suspend(int32 duration);
    void Resume();
    void ReGroup();
    void FormUp();
    void CleanupDeadMembers();
    void DestroyAllMembers();
    void Disband();
    void HandleMemberDeath(TechnoClass* pTechno);
    void AssignMissionToAll(Mission mission);
    void AssignTargetToAll(AbstractClass* pTarget);
    CoordStruct GetTeamCenter() const;
    CellStruct GetTeamCell() const;
    bool CanRecruit() const;
    bool ReinforceTeam(int32 nUnits);
    int32 GetTotalStrength() const;
    int32 GetActiveMemberCount() const;
    bool IsTeamCombatReady() const;
    CoordStruct ComputeFormationCenter() const;
    int32 GetMemberCount() const;

    // ========================================================================
    // 征兵链 —— 对应 asm TeamClass_RecruitUnit / CanRecruitUnit / SearchForRecruit
    // ========================================================================

    // asm: void __thiscall TeamClass_RecruitUnit(AITeamClass* this,
    //      _FootClass* Unit, char canRecruitExtras)  (asm:1530517) retn 8
    //  先过 CanRecruitUnit 校验；命中后先把单位从原队摘掉，再把坑位计数 +1、
    //  把它挂进本队成员链（首名成员即队长），再套用队伍组的编组号与老兵等级，
    //  最后置 actionExecuted/achievedSuccess 两标记。返回是否真的收了人。
    bool RecruitUnit(FootClass* pUnit, bool canRecruitExtras);

    // asm: char __thiscall TeamClass_CanRecruitUnit(AITeamClass* this,
    //      _FootClass* Unit, int* idxInTask, char canRecruitExtras)
    //      (asm:1530658) retn 0Ch
    //  逐条复刻 asm 的**提前返回**判据：不能是已属于本队/已死/非可招募；
    //  拥有者必须一致；在 task-force 里按类型定位坑位（写出 *idxInTask）；
    //  坑位越界时只有允许收编额外单位才放行；跨队招人要求对方队的优先度更低。
    bool CanRecruitUnit(FootClass* pUnit, int32* idxInTask, bool canRecruitExtras) const;

    // asm: TeamClass_RecruitThisUnit (asm:0x6EBFB0) —— 以 "允许额外收编" 转调
    //      RecruitUnit。
    void RecruitThisUnit(FootClass* pUnit);

    // asm: signed int __thiscall TeamClass_SearchForRecruit(AITeamClass* this,
    //      int idx)  (asm:1531212)
    //  替编成表第 idx 个坑位找一名新兵：定出集结点，按坑位要求的单位种类去
    //  载具/飞机/步兵名单里挑离集结点最近、且 CanRecruitUnit 通过的那一个；
    //  挑中即入队，连它的乘客一并收编。返回是否收到人。
    bool SearchForRecruit(int32 idx);

    // asm: TeamClass::Liberate (TeamClass_RecruitUnit 反向) —— 把全队成员解放
    //  回自由身：逐个清掉队伍归属与编组，再清空成员表与各坑位计数。
    void Liberate();

    // 队伍是否已无任何存活成员（asm 判 `FirstUnitInTeam == 0`）。
    bool IsEmpty() const;

    // asm: int __thiscall TeamClass_LeaveMap(AITeamClass* this)  (asm:0x6EA3xx)
    //  用编成表总人数与当前实到人数比对，刷新 IsFullyLoaded/IsLeavingMap/
    //  WantsToDelete 等中间状态；实到人数归零即判本队可以整体消失，返回假
    //  （调用方据此结束本帧）。否则返回真（队伍保留）。
    bool LeaveMap();

    // asm: TeamClass::Assign_Mission_Target (asm:0x6E90xx) —— 把队伍目标下发
    //  给全队：置 ITarget，并让每个成员朝目标行动（移动/攻击由成员自身决定）。
    void AssignMissionTarget(AbstractClass* pTarget);

    // asm: TeamClass_Update @@9-@@11：给成员链上每个单位打 "随队长移动" 标记
    //  (FootClass.field_689)，使其步调跟随队长。
    void MaskAllMembersForRemoval();

    AITeamClass(AITeamTypeClass* pType, HouseClass* pOwner) noexcept;

protected:
    explicit AITeamClass(noinit_t) noexcept : AbstractClass(noinit) {}

public:
    AITeamTypeClass* TeamType;
    HouseClass* Owner;
    ScriptClass* Script;
    DynamicVectorClass<TechnoClass*> Members;
    int32 CurrentAction;
    bool Active;
    int32 CurrentScriptLine;
    bool IsFormed;
    int32 CreationFrame;
    int32 TotalThreatValue;
    CDTimerClass GuardAreaTimer;
    CDTimerClass SuspendTimer;
    bool IsSuspended;
    bool IsReforming;
    bool NeedsReGrouping;
    CoordStruct FormationTarget;

    // ------------------------------------------------------------------------
    // asm 字段（与 TeamClass 结构体偏移一一对应；字段名不硬求一致，语义对齐）：
    //   SpawnCell      (+0x34)  队伍锚点/出生点，可为任意地图对象
    //   FollowAnchor   (+0x38)  FindUnitToFollow 选出的跟随单位
    //   ITarget        (+0x3C)  当前队伍目标（原 TargetBuilding）
    //   PrevTarget     (+0x40)  上一目标，切换时保留一帧
    //   TotalObjects   (+0x48)  当前实到成员数
    //   totalStrength  (+0x4C)  成员战力累计
    //   TeamCounts[6]  (+0x88)  每个 task-force 坑位已招到的人数
    // ------------------------------------------------------------------------
    AbstractClass* SpawnCell;
    AbstractClass* FollowAnchor;
    AbstractClass* ITarget;
    AbstractClass* PrevTarget;
    // 最近一次 FindUnitToFollow 解出的集结格（asm 写回的 spawnCell）。
    CellStruct     AnchorCell;
    int32 TotalObjects;
    int32 totalStrength;

    static constexpr int32 MaxTaskForceSlots = 6;
    int32 TeamCounts[MaxTaskForceSlots];

    // asm: TeamClass_Update @+0x80 "本行动作已执行"（脚本据此步进）与
    //      @+0xA1 "本行动作成功" 两个标记。
    bool ActionExecuted;
    bool AchievedSuccess;

    // asm: LeaveMap 刷新的中间状态位。
    //   IsFullyLoaded   (+0x79) 实到人数已达编成要求
    //   IsLeavingMap    (+0x7A) 本队准备撤离（编成允许缺人出行时更易成立）
    //   HasLeftMap      (+0x78) 已经完成撤离
    //   IsForming       (+0x7F) 队伍已开始编队/出动
    bool IsFullyLoaded;
    bool IsLeavingMap;
    bool HasLeftMap;
    bool IsForming;
    // asm +0x76：GuardSlower 驱动的等待标记（离场判定时写回）。
    bool WantsToDelete;
    // asm +0x7B：离场状态相比上一帧发生变化时置位，抑制本帧脚本。
    bool NeedsRecheckState;

    // asm: TeamClass_Update @+0x7E/@+0x7D/@0x81/@0x82 的流程控制位。
    bool NeedsToDisappear;   // +0x7E
    bool JustDisappeared;    // +0x7D
    bool ReassignTarget;     // +0x81  下一帧重下发目标
    bool RaiseTagEvent;      // +0x82  成功时向 Tag 抛事件
};