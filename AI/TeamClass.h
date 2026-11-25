#pragma once

#include "../Abstract/AbstractClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"
#include "../Math/CoordStruct.h"

class BuildingTypeClass;
class TechnoTypeClass;

class TeamClass {
public:
    static DynamicVectorClass<TeamClass*>* Array;

    TeamClass(TeamTypeClass* pType, HouseClass* pOwner, int32 nFlags) noexcept;
    virtual ~TeamClass();

    void Update();
    bool AddMember(TechnoClass* pTechno, bool isLeader);
    bool RemoveMember(int32 index);
    // TeamClass::Remove (asm 0x6EA87A): detaches Unit from the team, decrements
    // the per-slot counter for the task-force slot the unit occupied (unless a
    // slot index is supplied in idx), updates TotalObjects/totalStrength and
    // clears the unit's team links.  Returns true when the unit was a member.
    bool Remove(FootClass* pUnit, int32 idx, bool count);
    int32 GetMemberCount() const;
    TechnoClass* GetMember(int32 index) const;
    void AssignMissionToAll(Mission mission);
    void AssignTargetToAll(AbstractClass* pTarget);
    void Form();
    void Disband();
    void MoveToWaypoint(int32 waypointIndex);
    void MoveToLocation(CoordStruct location);
    void AttackTarget(AbstractClass* pTarget);
    void GuardArea(CoordStruct location, int32 radius);
    void GuardTarget(AbstractClass* pTarget);
    void PatrolArea(CoordStruct toLocation);
    void HandleMemberDeath(TechnoClass* pTechno);
    bool DoesTeamStillExist() const;

    // ------------------------------------------------------------------------
    // asm: TeamClass::Took_Damage (asm 0x6EB3xx), retn 0Ch = 3 params.
    //  FootClass_ReceiveDamage calls this when one of the team's units is hit.
    //  The team reacts by re-establishing an anchor (spawnCell / target) so the
    //  members regroup around wherever the fighting is: the very first hit
    //  (before the first member ever took damage) simply clears the spawn cell
    //  and flags the team as having taken fire; afterwards, if the attacker is
    //  not a member, is hostile, and is not already the current target building,
    //  the team anchors on it (when the type allows retargeting) and the leader
    //  is told to fire back at the attacker.
    // ------------------------------------------------------------------------
    void Took_Damage(TechnoClass* pObject, int32 damageState, TechnoClass* pAttacker);

    // ------------------------------------------------------------------------
    // asm: TeamClass::Is_Leaving_Map (asm 0x6EC2xx), retn (no args).
    //  True only once the team has actually started running: it must have begun
    //  formation (+0x7F), the script must not be on its last action, and the
    //  current script line must be a "move to waypoint" whose waypoint lies
    //  outside the player's radar view (i.e. the team is marching off-screen).
    // ------------------------------------------------------------------------
    bool Is_Leaving_Map() const;

    // ------------------------------------------------------------------------
    // asm: TeamClass::Has_Entered_Map (asm 0x6EC3xx), retn (no args).
    //  True when every member of the team has actually been placed on the map
    //  (the "produced/spawned" byte is set on each unit).  An empty team counts
    //  as having entered.
    // ------------------------------------------------------------------------
    bool Has_Entered_Map() const;
    bool CanRecruit() const;
    int32 GetTotalStrength() const;
    bool IsTeamFull() const;
    void ReinforceTeam(int32 nUnits);
    void ReGroup();
    void SortByThreatValue();
    void UpdateRecruitTimer();
    void SetRecruitTimer(int32 frames);
    bool IsRecruitTimerExpired() const;
    int32 GetThreatValue(TechnoClass* pTechno) const;

private:
    void CleanupDeadMembers();
    void DoDisappear();
    CoordStruct ComputeFormationCenter();
    // 坑位缺员且现成单位招不到时，向房主的对应产线下单造齐（原版
    //   TeamClass_Update 的补员段 -> HouseClass_BeginProductionOf）。
    void RequestProductionForSlot(int32 idx);
    // asm: AITeamClass_FindUnitToFollow (asm 0x6EB7xx): 为队伍确定一个"跟随锚点"
    //  ——优先取距离队伍锚点（spawnCell）最近、且仍在场上的成员；找不到就取
    //  第一个成员。结果分别写入 SpawnCell（若为空）与 FollowAnchor。
    void FindUnitToFollow();

public:
    // ========================================================================
    // Script-action handlers (asm 0x6EDBxx..0x6EE1xx)
    //
    //  These are the TeamClass half of the script-action table: the script VM
    //  invokes one of them per action line, with `this` in ECX and the action's
    //  parameters on the stack.  Each stamps the "action executed" byte at
    //  +0x80 so the script advances.
    // ========================================================================
    void SetGlobal(void* pParam);
    void ClearGlobal(void* pParam);
    void SetLocal(void* pParam);
    void ClearLocal(void* pParam);
    void Panic(void* pParam);
    void Unpanic(void* pParam);
    void Win(void* pParam);
    void Lose(void* pParam);
    void Dud(void* pParam);
    void PlayEVA(void* pParam);
    void PlayMovie(void* pParam);
    void PlayTheme(void* pParam);
    void ReduceTiberium(void* pParam);
    void EnableHouseProduction(void* pParam);
    void ForceSale(void* pParam);
    void Suicide(void* pParam);
    void StartLStorm(void* pParam);
    void StopLStorm(void* pParam);
    void ShroudMap(void* pParam);
    void UnshroudMap(void* pParam);
    void AchieveSuccess(void* pParam);
    // TeamClass_GoToScriptAction (asm 0x6F1B4C): jumps the team's script to
    // the line the script-action argument names (argument - 2) and marks the
    // action executed.
    void GoToScriptAction(void* pParam);

    // ------------------------------------------------------------------------
    // 脚本动作表（asm Team_* 0x6EC98E..0x6F03xx）：
    //   路径点攻击/潜入/巡逻、选敌我建筑、跟随友军、按类型索敌、超武传送等。
    //   每个 proc 由 TeamClass_Update 按动作号调用；见 .cpp 内逐函数 asm 注解。
    // ------------------------------------------------------------------------
    void AttackWaypoint(void* pParam);
    void SetFlashing(void* pParam);
    void LoadOntoTransport(void* pParam);
    void GatherAtEnemyBase(void* pParam);
    BuildingClass* PickFriendlyStructure(BuildingTypeClass* pType, HouseClass* pHouse,
                                         TechnoClass* pUnit, int32 flags, bool onlyHouseEnemy);
    void AssignNewMission(void* pParam);
    void Scout(void* pParam);
    void AttackStructureAtWaypoint(void* pParam);
    void MoveToFriendlyStructure(void* pParam);
    void AttackEnemyStructure(void* pParam);
    void MoveToEnemyStructure(void* pParam);
    void GatherAtFriendlyBase(void* pParam);
    void SpyStructureAtWaypoint(void* pParam);
    void AttackTargetType(void* pParam);
    void GetTaskForceEntries(DynamicVectorClass<TechnoTypeClass*>* pOut);
    void SetElite_old(void* pParam);
    void PlayAnimType(void* pParam);
    void FollowFriendlies(void* pParam);
    void ChronoSphereToStructure(void* pParam);
    void ChronoWarpToStructure(void* pParam);
    void PatrolToWaypoint(void* pParam);
    BuildingClass* PickEnemyStructure(BuildingTypeClass* pType, HouseClass* pCaller,
                                      TechnoClass* pUnit, int32 flags, bool onlyHouseEnemy);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 TRUCKBtoTRUCKA 负责把队伍里型号为 TRUCKB 的载具
    //  换成 TRUCKA：逐个成员检查类型名，命中就改写它的类型指针；处理完把这
    //  一行动作标记为已执行。
    // ------------------------------------------------------------------------
    void TRUCKBtoTRUCKA(void* pParam);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 TRUCKAtoTRUCKB 负责把队伍里型号为 TRUCKA 的载具
    //  换成 TRUCKB，逻辑与 TRUCKBtoTRUCKA 对称。
    // ------------------------------------------------------------------------
    void TRUCKAtoTRUCKB(void* pParam);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 LiberateAllMembers 负责让全队成员脱离本队：逐个检查
    //  成员是否可被解放，可解放的从队伍中移除；处理完把本行动作标记为已执行。
    // ------------------------------------------------------------------------
    void LiberateAllMembers(void* pParam);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 GuardAreaForX 负责按给定半径守备一段时间：可选地
    //  先重置守备计时器（时长按参数换算），随后交给底层守备逻辑推进；计时器
    //  到期即把本行动作标记为已执行。
    // ------------------------------------------------------------------------
    void GuardAreaForX(void* pParam, bool resetTimer);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 LoadIntoTransport 负责把队员装进运输载具：逐个成员
    //  比较其"可载人数"与已载乘客数，只要还有一座载具装得下就直接结束；
    //  全部装不下才把本行动作标记为已执行。
    // ------------------------------------------------------------------------
    void LoadIntoTransport(void* pParam);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ChangeToScript 负责把本队的脚本切换成参数指定的
    //  新脚本：先释放旧脚本，再按脚本类型新建一份并复位到首行；无论成功与否
    //  都把本行动作标记为已执行。
    // ------------------------------------------------------------------------
    void ChangeToScript(void* pParam);

    // ========================================================================
    // Lifecycle
    // ========================================================================

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 CloneAndDie 负责把一个队伍"原样复制一份然后自毁"：
    //  先按同一个队伍类型、同一个拥有者在内存里新建一支克隆队，再把原来这支
    //  队的成员整体过户给克隆队（先从原队摘下、再让克隆队招募），原来的队伍
    //  在成员被搬空之后随即销毁自己。战场上的效果是：同一份编成被交接给一支
    //  全新的队伍实例继续执行脚本，而旧实例干净退出。
    // ------------------------------------------------------------------------
    void CloneAndDie(TeamTypeClass* pTeamType);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 LeaveMap 负责判断并记录"本队要不要撤离地图"：
    //  拿编成表规定的总人数与当前实际队员数相比，得出队伍是满员、缺人还是
    //  已经凑齐；再结合队伍类型上"必须凑齐才出动""可以缺人出动"这类开关，
    //  决定队列是原地待命、立即散开还是准备离场。当实际队员数已经为零时，
    //  队伍视同解散：清掉各种内部计数，并让地图上的触发器联动一次。
    //  返回值表示"队伍仍需保留"（真）还是"可以整体消失了"（假）。
    // ------------------------------------------------------------------------
    bool LeaveMap();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 SuspendTeamsByPriority 负责在某处基地遇袭时把
    //  "次要"的 AI 队伍集体挂起：凡是属于该拥有者、且优先度低于给定门槛的
    //  队伍，都先把队员依次解放回自由身，然后被打上"暂停"标记并设一个到期
    //  时间；在此之前它们不再参与任何行动，等计时耗尽才可能重新投入。
    // ------------------------------------------------------------------------
    static void SuspendTeamsByPriority(HouseClass* pOwner, int32 maxPriority);

    // TeamClass_RecruitThisUnit (asm 0x6EBFB0): forwards to RecruitUnit with
    // the "recruit extras" flag set.
    void RecruitThisUnit(FootClass* pUnit);
    // TeamClass_RecruitUnit (asm 0x6EA509).
    void RecruitUnit(FootClass* pUnit, bool canRecruitExtras);
    // TeamClass_CanRecruitUnit (asm 0x6EA8D1).
    bool CanRecruitUnit(FootClass* pUnit, int32* idxInTask, bool canRecruitExtras);

    // ── Queries ───────────────────────────────────────────────────────────
    // TeamClass_UnitInTeam (asm 0x6EC217): walks the member list +0x5D8 chain.
    bool UnitInTeam(TechnoClass* pUnit) const;
    // TeamClass_FindLeader (asm 0x6EC3D0): the member with the highest
    // LeadershipRating.
    TechnoClass* FindLeader() const;
    // TeamClass_CanAnyMembersAttack (asm 0x6F03F0).
    bool CanAnyMembersAttack() const;

    // ========================================================================
    // 根据游戏行为，可知空降/机降类队伍要靠运输机投送：编队前先查本队里
    //  有没有装着货的运输机。
    // ========================================================================
    bool DoesTeamHaveTransportAircraft() const;

    // ========================================================================
    // 根据游戏行为，可知采集队满载时把成员车斗里的矿按给定额度卸掉一部分，
    //  返回实际卸掉的总量。
    // ========================================================================
    int32 ReduceTiberium(int32 amount);

    // ========================================================================
    // 根据游戏行为，可知 GetStrayDistance 以队形中心为基准量出最远成员的
    //  距离，供"队伍走散"判断使用。
    // ========================================================================
    int32 GetStrayDistance() const;

    // ========================================================================
    // 根据游戏行为，可知 StartLStorm 让全队朝核心成员的目标集火；Suicide
    //  让全队按全额伤害自毁，走完整的死亡结算。
    // ========================================================================
    void StartLStorm();
    void Suicide();
    // TeamClass_GetSize (asm 0x6F0496): the fixed byte size of a TeamClass.
    int32 GetSize() const;
    // TeamClass_GetAbstractDerivationID (asm 0x6F04A2): AbstractType::Team.
    int32 GetAbstractDerivationID() const;
    // TeamClass_TargetTypeToFlags (asm 0x645BB0): maps the script's target
    // enumeration into the team member flag mask.
    int32 TargetTypeToFlags(int32 targetType) const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 SearchForRecruit 负责替编成表的第 idx 个坑位找一名
    //  合适的新兵补充进队：先在队伍当前的集结点周围，按该坑位要求的单位种类
    //  分别在地图的步兵、飞机、载具名单里挑选，优先取离集结点最近、且确实
    //  能被本队招入的那一个；找不到就原样返回。挑中以后把它招进队伍，如果
    //  它本身还载着乘客，连乘客一并收编。
    // ------------------------------------------------------------------------
    bool SearchForRecruit(int32 idx);


    TeamTypeClass* Type;
    HouseClass* Owner;
    int32 CreationFrame;
    bool IsTransient;
    bool IsFullStrength;
    bool NeedsToDisappear;
    bool JustDisappeared;
    int32 Value;
    int32 RecruitRadius;
    int32 RecruitTimer;
    ScriptClass* Script;
    DynamicVectorClass<TechnoClass*> Members;

    // 根据游戏行为，可知脚本闪烁需求记在队伍上，绘制层按队伍消费。
    bool IsFlashing = false;
    AbstractClass* ITarget;      // +0x3C  当前队伍目标（原 TargetBuilding）
    AbstractClass* PrevTarget;   // +0x40  上一个目标，切换时保留一帧
    AbstractClass* SpawnCell;    // +0x34  队伍的锚点/出生点（可以是任意地图对象）
    AbstractClass* FollowAnchor; // +0x38  队伍跟随锚点（FindUnitToFollow 的产物）
    TeamClass* NextTeam;
    TeamClass* PrevTeam;
    int32 GuardAreaTimer;
    int32 CurrentMission;
    int32 TotalThreatValue;
    int32 totalStrength;
    int32 idxTeam;

    // The "action executed" byte the script VM stamps at +0x80 after each
    // script-action handler runs, so the script can advance past the line.
    bool ActionExecuted;

    // The "action achieved success" byte at +0xA1 that the script engine reads
    // when an action reports success.
    bool AchievedSuccess;

    // -- Leave / dissolve state -----------------------------------------------
    // 根据游戏行为，可知下面这组标记描述一支队伍"是否人齐、要不要走"的中间
    // 状态，由每次编成核对时刷新：
    //   IsFullyLoaded   : 实到人数已经达到编成表要求。
    //   IsMissingMembers: 人还没凑齐，需要继续招人。
    //   IsLeavingMap    : 本队准备撤离地图（编成表允许缺人出行时更易成立）。
    //   HasLeftMap      : 已经完成撤离，之后只在原地等待被回收。
    bool IsFullyLoaded;
    bool IsMissingMembers;
    bool IsLeavingMap;
    bool HasLeftMap;

    // 根据游戏行为，可知 NeedsToDelete 表示本队已被判定为"可以整体消失"，
    // 下一轮清理时会被移出全局队伍表；TimeToDissapear 是与之配套的"已进入
    // 消失流程"标记（原字段名即如此拼写）。WantsToDelete 表示因为
    // "必须凑齐才出动"的约束没满足，本队暂时按兵不动。
    bool NeedsToDelete;
    bool TimeToDissapear;
    bool WantsToDelete;

    // 根据游戏行为，可知 TeamSuspended / SuspendFrame / SuspendDuration 是
    // 挂起机制的三半（asm +0x74/+0x64/+0x6C 那组计时器）：
    //   前者是"本队当前被暂停"的开关；
    //   SuspendFrame 是开始挂起的帧号（asm 用 0xFFFFFFFF 表示"尚未挂起"）；
    //   SuspendDuration 是要挂起的时长（帧）。
    // 基地遇袭时优先度不足的队伍会被打上暂停并写下起始帧与时长，等
    // Frame - SuspendFrame >= SuspendDuration 才解除，在此之前不参与行动。
    bool TeamSuspended;
    int32 SuspendFrame;
    int32 SuspendDuration;

    // asm +0x7F: "队伍已开始编队/出动"的开关。Update 首次发现队伍就绪时置 1，
    // 之后只在真正编队时才推进脚本；散开/集结辅助逻辑也会复位它。
    bool IsForming;

    // asm +0x75: "本队已经挨过打"的开关。Took_Damage 首次被击中且尚未编队时置 1。
    bool HasTakenDamage;

    // Per-task-force-slot unit counters (asm +0x88, six dwords: cntObjects).
    // RecruitUnit bumps the slot the new member filled; the AI uses these to
    // decide whether a slot still needs more units.
    static constexpr int32 MaxTaskForceSlots = 6;
    int32 TeamCounts[MaxTaskForceSlots];
};