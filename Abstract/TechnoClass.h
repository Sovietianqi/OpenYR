#pragma once

#include "ObjectClass.h"
#include "TechnoTypeClass.h"
#include <Math/Timer.h>

class BuildingClass;

// ============================================================================
// CloakStateEnum - tracks the cloak fade animation
// ============================================================================
enum class CloakStateEnum : int32 {
    Idle        = 0,
    Cloaking    = 1,
    Cloaked     = 2,
    Uncloaking  = 3
};

class TechnoClass : public ObjectClass {
public:
    static const AbstractType AbsID = AbstractType::Techno;

    static DynamicVectorClass<TechnoClass*>* Array;

    TechnoClass() noexcept
        : ObjectClass()
        , TechnoType(nullptr)
        , Health(0)
        , MaxHealth(0)
        , VeterancyLevel(0)
        , Experience(0)
        , CloakState(CloakStateEnum::Idle)
        , CloakAlpha(255)
        , FireRechargeTimer(0)
        , CloakTimer(0)
        , RepairActive(false)
        , RepairRate(0)
        , IronCurtainTimer(0)
        , ForceShieldTimer(0)
        , IronTintTimer(0), IronTintStage(0)
        , AirstrikeTintTimer(0), AirstrikeTintStage(0)
        , LastFireFrame(-0x7FFFFFFF)
        , FireDamageTimer(0)
        , SparkyCounter(0)
        , IsParasited(false)
        , TemporalTimer(0)
        , GasTimer(0)
        , RadiationTimer(0)
        , Tunnel(false)
        , ActiveTurretIndex(0)
        , WeaponStageFrame(0)
        , RecoilAmount(0)
        , RecoilStartFrame(0)
        , IsDyingNow(false)
        , KilledBy(nullptr)
        , IsTalking(false)
        , TalkBubbleText(nullptr)
        , TalkBubbleEnd(0)
        , LastTalkFrame(0)
        , PlanningDestination()
        , SmokeSystemActive(false)
        , SmokeSystemStage(0)
        , CurrentWeaponNumber(-1)
        , OrigOwner(nullptr)
        , Captured(false)
        , IsDisguisedFlag(false)
        , WarpInTimer(0)
        , WarpOutTimer(0)
        , IsWarpingOutFlag(false)
        , DisguiseCreationFrame(-1)
        , DisguiseBlinkTimer()
        , DisguiseTypeId(-1), DisguiseHouse(nullptr)
        , BunkerLinkedItem(nullptr)
        , PassengerCount(0), PassengerHead(nullptr), PassengerCapacityCount(0)
        , PrimaryFacing()
        , GroundHeight(0)
        , DrainTimer(0)
        , PlanningToken(-1)
        , WeaponStage(0)
        , FocusOnUnit(nullptr)
        , TemporalImUsing(nullptr)
        , QueuedVoiceIndex(-1)
        , CurrentAmmo(0)
        , ReloadTimer()
        , AirstrikeTimeStart(0)
        , AirstrikeTimeLeft(0)
        , AirstrikeTimeGen(0)
    {}
    virtual ~TechnoClass() {}

    virtual AbstractType WhatAmI() const override { return AbstractType::Techno; }
    virtual int32 Size() const override { return sizeof(TechnoClass); }

    virtual HRESULT GetClassID(CLSID* pClassID) override { return E_FAIL; }
    virtual HRESULT Load(IStream* pStm) override { return S_OK; }
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override { return S_OK; }

    HouseClass* GetOwningHouse() const { return Owner; }
    int32 GetOwningHouseIndex() const { return 0; }

 // TechnoClass_GetThreatPosed.  The threat this object
    // presents to the enemy, counted into the owning house's threat grid.
    //
    //   * Without a type the answer is zero.
    //   * A building carrying occupants (WhatAmI == 6) multiplies the occupant
    //     count by RulesClass::ThreatPerOccupant.
    //   * Otherwise the type's ThreatPosed value is returned.
    virtual int32 GetThreatValue() const;

    // The cell the object currently stands in.
    CellStruct Get_Cell_Ptr_Coord() const;

    // ========================================================================
    // Static Array management
    // ========================================================================
    static void Init_Array();
    static void Delete_Array();
    static int32 Add_To_Array(TechnoClass* pInstance);
    static bool Remove_From_Array(TechnoClass* pInstance);
    static int32 Get_Total_Count();
    static TechnoClass* Get_Instance(int32 index);
    static int32 Find_Index(TechnoClass* pInstance);

    // ========================================================================
    // Update loop (AI, combat, cloaking)
    // ========================================================================
    virtual void Update() override;
    void Update_AI();
    virtual void Update_Combat();
    void Update_Cloak();
    void Update_Repair();
    void Update_Veterancy();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 UpdateTint_IronCurtain 负责推进"铁幕"染色动画：
    //  单位不在铁幕之下时什么都不做；在铁幕之下则按阶段计时，从一个阶段
    //  逐步走到下一个阶段，阶段切换的同时重设本阶段的持续时间，从而形成
    //  金属光泽循环闪烁的观感。
    // ------------------------------------------------------------------------
    void UpdateTint_IronCurtain();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 UpdateTint_Airstrike 负责推进"空袭标记"染色动画：
    //  只有正被空袭击中的目标才需要闪烁；按阶段计时逐个推进，闪烁一轮后
    //  若空袭已经结束就把阶段复位，否则重新开始下一轮。
    // ------------------------------------------------------------------------
    void UpdateTint_Airstrike();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ClearPlanningNodes 负责清掉本单位所属的行军规划
    //  令牌：找到本阵营当前使用的规划令牌后，把令牌上记录的节点清空，使
    //  单位不再沿既有路线行动。单位被俘获或易主时用它。
    // ------------------------------------------------------------------------
    void ClearPlanningNodes();

    // ========================================================================
    // 根据游戏行为，可知 DrawVeterancy 在单位头顶按军衔画出升阶标记（老兵
    //  一枚、精锐两枚），新兵不画。
    // ========================================================================
    void DrawVeterancy(Point2D* pCoord, RectangleStruct* pRect);

    // ========================================================================
    // 根据游戏行为，可知 DisplayTalkBubble 让单位在头顶冒出一个带文本的气泡，
    //  UpdateTalkBubble 负责在到点后把气泡收掉。
    // ========================================================================
    bool DisplayTalkBubble(const wchar_t* pText, int32 duration);
    void UpdateTalkBubble();

    // ========================================================================
    // 根据游戏行为，可知 CreatePlanningToken 为一个将要执行的行军规划点分配
    //  一个规划令牌槽位；已有未消费令牌时复用。
    // ========================================================================
    int32 CreatePlanningToken(const CoordStruct& dest);

    // ========================================================================
    // 根据游戏行为，可知 GetNextMission 在没有外部指令时为单位挑一件"下一该
    //  做的事"（脚本/队列优先，其余进入警戒）。
    // ========================================================================
    Mission GetNextMission() const;

    // ========================================================================
    // 根据游戏行为，可知 PrintSelectedTip 把单位的名称与状态填进选中提示，
    //  供界面的信息栏显示。
    // ========================================================================
    void PrintSelectedTip(wchar_t* pBuffer, int32 bufferLength) const;

    // ========================================================================
    // 根据游戏行为，可知 ShouldSuppress 判断本次开火是否该被压住（目标已死、
    //  已不在有效打击范围）。
    // ========================================================================
    bool ShouldSuppress(AbstractClass* pTarget) const;

    // ========================================================================
    // 根据游戏行为，可知 PredictTargetCoords 按目标当前位置与速度推算炮弹
    //  抵达时刻目标会在哪，用于带提前量的武器。
    // ========================================================================
    CoordStruct PredictTargetCoords(AbstractClass* pTarget, int32 flightTime) const;

    // ========================================================================
    // 根据游戏行为，可知 FireEBolt 朝目标打出一道电弧并在命中时结算伤害。
    // ========================================================================
    bool FireEBolt(AbstractClass* pTarget, int32 damage);

    // ========================================================================
    // 根据游戏行为，可知 DistributeFire 把一轮火力按"离中心越远衰减越多"的
    //  方式分摊到目标附近区域内的敌人身上。
    // ========================================================================
    int32 DistributeFire(AbstractClass* pTarget, int32 damage, int32 radius);

    // ========================================================================
    // 根据游戏行为，可知 UpdateGattling_ 推进转管机枪类武器的射速档位：
    //  连射升档、停火降档。
    // ========================================================================
    void UpdateGattling_(int32 stages, int32 rate);

    // ========================================================================
    // 根据游戏行为，可知 Drain 处理吸取类效果：把本单位当前的能量/生命按
    //  额度转走一部分给发起者。
    // ========================================================================
    int32 Drain(int32 amount, TechnoClass* pSource);

    // ========================================================================
    // 根据游戏行为，可知 Die 是所有战斗单位死亡的统一入口：残骸、经验、
    //  统计都在这里结算，子类按各自的表现扩展它。
    // ========================================================================
    virtual void Die(TechnoClass* pKiller);

    // ========================================================================
    // 根据游戏行为，可知 Recoil 推进炮管后座的表现状态：开火瞬间后座拉满，
    //  随后按帧复位；复位期间渲染层把炮管画在偏后的位置。
    // ========================================================================
    void Recoil(int32 amount);

    // ========================================================================
    // 根据游戏行为，可知 GetThreatPosed 给出"本单位对某个阵营构成的威胁
    //  值"：AI 挑目标时用它排序，威胁越大的单位越优先被打。
    // ========================================================================
    int32 GetThreatPosed(HouseClass* pToHouse) const;

    // ========================================================================
    // Fire weapon implementation
    // ========================================================================
    BulletClass* Fire_Impl(AbstractClass* pTarget, int32 nWeaponIndex);

    // ========================================================================
    // TakeDamage implementation
    // ========================================================================
    bool TakeDamage_Impl(int32 damage, ObjectClass* pSource,
                         WarheadTypeClass* pWarhead);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ReceiveDamage 是单位承受伤害的统一入口：它把
    //  "打在身上多少伤害、来自谁、用什么弹头、是否忽略防御" 这组参数整理好，
    //  再交给底层结算。护盾、铁幕、免疫等判定都在底层完成。
    //  返回实际是否造成了伤害。
    // ------------------------------------------------------------------------
    int32 ReceiveDamage(int32 damage, TechnoClass* pSource,
                        WarheadTypeClass* pWarhead, int32 a4);

    // ========================================================================
    // Repair logic
    // ========================================================================
    void Repair_Start(int32 rate);
    void Repair_Stop();

    // ========================================================================
    // Cloak / Uncloak
    // ========================================================================
    virtual void Cloak(bool bPlaySound);
    virtual void Uncloak(bool bPlaySound);
    bool Is_Cloaked() const;
    bool Is_Cloaking() const;

    // ========================================================================
    // Veteran / Promote
    // ========================================================================
    void Promote(int32 experience);
    virtual int32 GetVeterancy() const;
    int32 Get_Experience() const;

    // ========================================================================
    // Is_Ally / Is_Enemy
    // ========================================================================
    bool Is_Ally(HouseClass* pHouse) const;
    bool Is_Enemy(HouseClass* pHouse) const;
    bool Is_Ally(TechnoClass* pTechno) const;
    bool Is_Enemy(TechnoClass* pTechno) const;

    // ========================================================================
    // Threat position
    // ========================================================================
    CoordStruct Get_Threat_Pos() const;

    // ========================================================================
    // CRC
    // ========================================================================
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // Per-instance serialization for the save-game stream.
    void Save(class SaveGameClass& saver) const;
    void Load(class LoadGameClass& loader);

    // ========================================================================
    // Combat core (mirrors TechnoClass_* in the original binary)
    // ========================================================================
    int32  SelectWeapon(AbstractClass* pTarget);
    bool   IsCloseEnoughToTarget(AbstractClass* pTarget, int32 idxWeapon);
    int32  EvalThreatRating(TechnoClass* pThreat, int32 idxWeapon);
    int32  EstimateDamage(AbstractClass* pTarget, int32 idxWeapon);
    bool   ShouldRetaliate(TechnoClass* pAttacker);
    void   RegisterDestruction();
    void   RegisterLoss();
    CoordStruct GetFLH(int32 nWeaponIndex, bool muzzle);
    bool   IsRadarVisible(HouseClass* pHouse) const;
    int32  Get_ZAdjustment() const;
    VisualType VisualCharacter(bool raw);
    void   CreateGap();
    void   DeleteGap();
 // TechnoClass_UpdatePowered: re-evaluates whether this
    // techno still receives the power feed it depends on.  Called by an
    // owning structure's BuildingClass_InitMore for each powered unit slot.
    void   UpdatePowered();
    void   UpdateSight();
    void   DrawExtras(Point2D* pCoord, RectangleStruct* pRect);
    void   DrawHidden(Point2D* pCoord, RectangleStruct* pRect);
    void   DealParticleDamage(TechnoClass* pVictim, WarheadTypeClass* pWarhead,
                              int32 damage, int32 distanceFromEpicenter);
    void   PointerGotInvalid(AbstractClass* pInvalid);
    int32  GetSightRange() const;
    bool   GapActive;

    // ========================================================================
    // TechnoClass virtuals (preserved from original header)
    // ========================================================================
    virtual bool IsVoxel() const { return false; }
    virtual void Destroyed(ObjectClass* Killer) {}
    virtual bool CanScatter() const { return false; }
    virtual int32 GetDefaultSpeed() const { return 0; }
    virtual bool HasTurret() const;
    virtual bool CanDeploySlashUnload() const { return false; }
    virtual bool IsUnitFactory() const { return false; }
    virtual void TakeDamage(int32 damage, ObjectClass* source, WarheadTypeClass* warhead) {}
    virtual bool IsEngineer() const { return false; }
    virtual bool IsCloseEnough(AbstractClass* pTarget, int32 idxWeapon) const { return false; }
    virtual bool IsCloseEnoughToAttack(AbstractClass* pTarget) const { return false; }
    virtual bool IsInAir() const;
    virtual bool IsOnFloor() const;
    virtual bool OnBridge() const;

    // ── TechnoClass state probes / accessors ──────────────────────────────
 // TechnoClass_IsCrewed: mirrors the type's Crewed flag.
    virtual bool IsCrewed() const;
 // TechnoClass_IsFactory: base-class answer is false.
    virtual bool IsFactory() const;
 // 根据游戏行为，可知 HasMultipleTurrets 负责下面这段逻辑。
    virtual bool HasMultipleTurrets() const;
 // 根据游戏行为，可知 GetZ 负责下面这段逻辑。
    virtual int32 GetZ() const;
 // 根据游戏行为，可知 GetActiveTurretIndex 负责下面这段逻辑。
    virtual int32 GetActiveTurretIndex() const;
 // 根据游戏行为，可知 GetTurretIndex 负责下面这段逻辑。
    virtual int32 GetTurretIndex() const;
 // 根据游戏行为，可知 CurrentWeaponSelected 负责下面这段逻辑。
    virtual bool CurrentWeaponSelected() const;
 // 根据游戏行为，可知 GetOwner 负责下面这段逻辑。
    HouseClass* Get_Owner() const;
    // ── Disguise ──────────────────────────────────────────────────────────
 // TechnoClass_IsDisguised: reads the type/instance disguise
    // flag.  The two-argument form adds a range parameter the original accepts
    // and ignores.
    virtual bool IsDisguised() const;
    virtual bool IsDisguised2(int32 a2) const;
 // TechnoClass_ClearDisguise: drops the disguise.
    virtual void ClearDisguise();

    // ── Cloak / warp / temporal state ─────────────────────────────────────
 // 根据游戏行为，可知 IsCloakable 负责下面这段逻辑。
    virtual bool IsCloakable() const;
 // 根据游戏行为，可知 IsBeingWarpedOut 负责下面这段逻辑。
    virtual bool IsBeingWarpedOut() const;
 // 根据游戏行为，可知 IsWarpingOut 负责下面这段逻辑。
    virtual bool IsWarpingOut() const;
 // 根据游戏行为，可知 IsNotTemporalLocked 负责下面这段逻辑。
    virtual bool IsNotTemporalLocked() const;
 // 根据游戏行为，可知 IsNotWarpingIn 负责下面这段逻辑。
    virtual bool IsNotWarpingIn() const;
 // 根据游戏行为，可知 IsDraining 负责下面这段逻辑。
    virtual bool IsDraining() const;

    // ── Temporal / weapon-legal probes ────────────────────────────────────
 // TechnoClass_IsTemporalSource: true when this techno is
    // currently the source of a temporal weapon that has a victim.
    bool IsTemporalSource() const;
 // TechnoClass_IsLegalWeapon: true when the weapon container
    // is non-null and holds a weapon id.
    bool IsLegalWeapon(const void* pWeapon) const;
 // TechnoClass_GetNonSprayWeapon (-adjacent): returns the weapon
    // in the slot that IsNoSprayAttack selects.
    void* GetNonSprayWeapon() const;
 // TechnoClass_CanAreaFire: true when the current weapon is
    // flagged as an area-effect weapon.
    bool CanAreaFire() const;

    // ── Weapon selection helpers ──────────────────────────────────────────
 // 根据游戏行为，可知 CanPassiveAquire 负责下面这段逻辑。
    virtual bool CanPassiveAquire() const;
 // 根据游戏行为，可知 CanTraverse 负责下面这段逻辑。
    virtual bool CanTraverse() const;
 // 根据游戏行为，可知 CanSetWaypoint 负责下面这段逻辑。
    virtual bool CanSetWaypoint() const;

    // ── Miscellaneous probes ──────────────────────────────────────────────
 // 根据游戏行为，可知 NeedsToSelfHeal 负责下面这段逻辑。
    virtual bool NeedsToSelfHeal() const;
 // TechnoClass_GetHealthState: 0 = healthy, 1 = damaged,
    // 2 = critical, matching the rules-side health thresholds.
    virtual int32 GetHealthState() const;
 // TechnoClass_GetXYDistanceFrom: planar distance to another
    // object, in leptons.
    virtual double GetXYDistanceFrom(const AbstractClass* pOther) const;
    // ── Panic / idle / power probes ───────────────────────────────────────
 // TechnoClass_Panic (xx): the zero-argument form does nothing at
    // this layer; the mission-controller entry point is FootClass::Panic.
    virtual void Panic();
    // TechnoClass_Unpanic: the counterpart that returns the object to normal
    // morale; the base class does nothing.
    virtual void Unpanic();
    virtual void Scatter(const CoordStruct& crd, bool ignoreMission, bool ignoreDestination) {}
 // TechnoClass_IdleAction: returns false - derived missions
    // override it to report that they have finished idling.
    virtual bool IdleAction();
    virtual void UpdateIdleAction() {}
 // TechnoClass_IsPowerOnline (x): true only for powered
    // structures; the base class answers false.
    virtual bool IsPowerOnline() const;
    virtual bool IsArmed() const { return false; }
    virtual bool IsBeingRepaired() const { return false; }
    virtual bool IsCurrentlyBeingSold() const { return false; }
    virtual bool IsPowered() const { return false; }
    virtual bool IsSelling() const { return false; }

    // ========================================================================
    // 任务调度入口（对应原版 vt 偏移 0x1E8 一带的mission层）
    //
    //  原版把"设定任务 / 排队任务 / 读取任务 / 设定目标 / 读取目标"这一组操作
    //  放在任务层，任何 Techno 都可以通过虚表统一下发。载具、步兵、建筑各自
    //  覆盖它们以实现自己的副作用（换序列、切换匍匐或待发状态等）。基类只提供
    //  空实现，让未被覆盖的对象也能安全接收调用。
    // ========================================================================
    virtual void SetMission(Mission /*mission*/) {}
    virtual Mission GetMission() const { return Mission::Sleep; }
    virtual void QueueMission(Mission /*mission*/) {}
    virtual void SetTarget(AbstractClass* /*pTarget*/) {}
    virtual AbstractClass* GetTarget() const { return nullptr; }
    virtual bool IsFiring() const { return false; }
    virtual bool IsDeploying() const { return false; }
    virtual bool IsBeingDrained() const { return false; }
    virtual bool IsSensorsOnline() const { return false; }
    virtual bool IsPowerDrain() const { return false; }
    virtual bool IsCharged() const { return false; }
    virtual bool IsFactoryActive() const { return false; }
    virtual bool IsLaserFencePost() const { return false; }
    virtual bool IsCapturable() const { return false; }
    virtual bool IsOccupiable() const { return false; }
    virtual bool IsRubble() const { return false; }
    virtual bool IsBridge() const { return false; }
    virtual bool IsWall() const { return false; }
    virtual bool IsGate() const { return false; }
    virtual bool IsOverlay() const { return false; }
    virtual bool IsLight() const { return false; }
    virtual bool IsVehicle() const { return false; }
    virtual bool IsTiberium() const { return false; }
 // TechnoClass_GetTiberium.  Sums the four tiberium storage
    // floats carried by a harvesting techno and floors the running total.  The
    // base implementation carries no storage and therefore reports zero.
    virtual double Get_Tiberium() const { return 0.0; }

    // IsBeingMindControlled - the vtable slot the original queries at +0x160.
    // True while this object is already under an external mind controller.
    virtual bool IsBeingMindControlled() const { return false; }

 // TechnoClass_CanBePermaMC.  Whether this object may be
    // permanently mind controlled by a psychic dominator:
    //
    //   * buildings never qualify (WhatAmI() == Building);
    //   * the techno type must not be ImmuneToPsionics;
    //   * the object must not already be mind controlled;
    //   * the techno type must not be a balloon-hover type;
    //   * the object must still be alive.
    virtual bool Can_Be_PermaMC() const
    {
        if (WhatAmI() == AbstractType::Building)
            return false;

        const TechnoTypeClass* pType = TechnoType;
        if (pType != nullptr && pType->IsImmuneToPsionics)
            return false;

        if (IsBeingMindControlled())
            return false;

        if (pType != nullptr && pType->IsBalloonHover)
            return false;

        return !IsDead();
    }
 // TechnoClass_CanOccupyFire: only buildings and the infantry
    // that garrison them answer true; the base class returns false.
    virtual bool CanOccupyFire() const;
 // TechnoClass_GetOccupantCount: number of occupants garrisoned.
    virtual int32 GetOccupantCount() const;

    // ── Layer / cell helpers ──────────────────────────────────────────────
 // TechnoClass_InWhichLayer: asks the locomotor which draw
    // layer this techno currently occupies.
    virtual int32 InWhichLayer() const;
 // TechnoClass_GetCellCoords: converts the world coordinates
    // into the owning cell's X/Y, dividing by 0x100 (one leptons-per-cell unit).
    virtual CellStruct GetCellCoords() const;

 // ── Threat / value ratings ( / 0x41B54F / 0x41B557) ──────
    virtual int32 GetAntiAirValue() const;
    virtual int32 GetAntiArmorValue() const;
    virtual int32 GetAntiInfantryValue() const;

 // TechnoClass_UpdateRefinerySmokeSystems: 根据游戏行为，可知建筑受损后开始
    //  冒烟，受损越重烟越浓；完好时不冒烟。
    virtual void UpdateRefinerySmokeSystems();

    // ========================================================================
    // Planning-token and type-flag probes
    // ========================================================================
 // TechnoClass_GetPlanningToken: the waypoint token slot
    // (+0x514).
    int32 GetPlanningToken() const;
 // TechnoClass_AttachPlanningToken: stores token into the
    // waypoint token slot (+0x514).
    void AttachPlanningToken(int32 token);
 // TechnoClass_Assign_Destination_Cell: stores the target
    // building into the "focus on unit" slot.
    void Assign_Destination_Cell(BuildingClass* pTarget);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 PresumeMissionComplete 负责在任务被打断时收尾：
    //  先松开正在施加的时间武器（若有），再检查对象是否还能继续执行任务，
    //  能在必要时请求下一个任务。返回是否"任务确已完成"。
    // ------------------------------------------------------------------------
    bool PresumeMissionComplete();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 IsNotCloakedByOthers 负责判断"这个单位此刻并没有
    //  被别人掩蔽"：它自身要么本来就能隐形，要么带隐形标记；并且不受 EMP、
    //  瘫痪、正在传送出入等影响；老兵/精英级的"不可被探测"能力会直接否决；
    //  最后要求所在格子上没有掩蔽该单位的隐身发生器（非本方的那一种）。
    // ------------------------------------------------------------------------
    bool IsNotCloakedByOthers() const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 IsCloakedByOthers 负责判断"这个单位此刻正被别的
    //  东西掩蔽"，它是 IsNotCloakedByOthers 的补集。
    // ------------------------------------------------------------------------
    bool IsCloakedByOthers() const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 DeselectIfNotPlayerOwned 负责在只剩一个选中对象时，
    //  若它的拥有方不是玩家则把它取消选中。返回是否真的执行了取消选中。
    // ------------------------------------------------------------------------
    bool DeselectIfNotPlayerOwned();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ExpireDrain 负责清理"抽能"状态：让正在被抽取的
    //  目标松开抽能源，并且通知施加方的抽能对象解除关联。
    // ------------------------------------------------------------------------
    void ExpireDrain();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 UpdateThreatToCell 负责把本单位当前的威胁值写进
    //  它所在格子的威胁记录，供寻路与 AI 使用。
    // ------------------------------------------------------------------------
    void UpdateThreatToCell();

 // TechnoClass_NotSubmerged: true when the object's height is
    // above the submarine threshold (-20).
    bool NotSubmerged() const;
 // 根据游戏行为，可知 IsNotSprayAttack 负责下面这段逻辑。
    bool IsNotSprayAttack() const;
 // 根据游戏行为，可知 IsNotSprayAttack2 负责下面这段逻辑。
    bool IsNotSprayAttack2() const;
 // TechnoClass_SetCurrentWeaponStage: stores idx into the
    // multi-stage weapon counter (+0x140) when it is non-negative.
    void SetCurrentWeaponStage(int32 idx);
 // TechnoClass_HasTurretTooltips: the type's turret-tooltip
    // flag.
    bool HasTurretTooltips() const;

 // TechnoClass::Greatest_Threat.  The engine's universal
    // target-acquisition entry point.  `projFlags` is the projectile-
    // capability bitmask (ProjectileTypeFlags); `curThreat` seeds the best
    // threat so a caller can require a strictly better candidate; `a4`
    // enumerates the caller class (0 = building, 1 = vehicle, 2 = infantry).
    // Returns the best target found, or null.
    virtual ObjectClass* Greatest_Threat(int32 projFlags, int32 curThreat, int32 a4);

 // TechnoClass_Combat_Damage.  Weapon-slot damage query used
    // by Greatest_Threat to decide whether a special movement class (engineer
    // / terrorist) should ignore military targets.
    int32 Combat_Damage(int32 idxWeapon) const;

 // TechnoClass_Techno_31C.  Vtable +0x31C - resolves the
    // techno's current target object honouring the requested slot.
    ObjectClass* Techno_31C(int32 which) const;

    // ========================================================================
    // Position / altitude probes
    // ========================================================================

 // TechnoClass_GetCellCoords1.  Writes the techno's owning
    // map cell (floored to the cell grid) into `pOut` and returns it.
    CellStruct* GetCellCoords1(CellStruct* pOut) const;

 // TechnoClass_GetCell1.  The CellClass the techno is
    // standing on, or null off-map.
    CellClass* GetCell1() const;

 // TechnoClass_OnFloor / _InAir.  True when
    // the techno's Z puts it on the ground / in the air.  Both test the
    // "has height" flag at +0x74 first, then compare the current Z against
    // twice the ObjectClass::HeightAtSpawn offset (the parked-on-ground
    // threshold).
    bool OnFloor() const;
    bool InAir() const;

 // TechnoClass_GetZFudgeCliff / _Column /
 // _Tunnel.  FootClass::Get_ZAdjustment consults these to
    // snap a unit's draw height to the terrain in front of it.  Each returns
    // a fudge value in pixels.
    int32 GetZFudgeCliff() const;
    int32 GetZFudgeColumn() const;
    int32 GetZFudgeTunnel() const;

 // TechnoClass_GetElevationRangeBonus /
 // _GetElevationBonusNoSqrt.  The extra weapon range a
    // height advantage confers.  The NoSqrt variant skips the square root and
    // is used by the cheaper proximity test.
    double GetElevationRangeBonus(ObjectClass* pTarget) const;
    double GetElevationBonusNoSqrt(ObjectClass* pTarget) const;

 // TechnoClass_TimeForCellInset.  True when the distance to
    // the target exceeds (warhead CellSpread - CellInset), i.e. the shot has
    // already cleared the minimum arming distance.
    bool TimeForCellInset(TechnoClass* pTarget) const;

    // ========================================================================
    // Combat / role classifiers
    // ========================================================================

 // TechnoClass_CanLobber.  True when the techno's current
    // weapon is flagged as a lobber (arcing artillery).
    bool CanLobber() const;

 // TechnoClass_HasAbility.  True when the type carries the
    // requested special ability flag.
    bool HasAbility(int32 ability) const;

 // TechnoClass_CanBeBunkered.  True when this techno may be
    // loaded into a battle bunker / tank bunker.
    bool CanBeBunkered() const;

    // 根据游戏行为，可知 BunkerLinkedItem 是"掩体"与"窝在掩体里的单位"之间的
    // 双向链接指针：掩体这一端存着里面的单位，单位这一端反过来存着它所在的掩体。
    // 建立链接时两侧同时写入，拆除时两侧同时清空；它既是占用标记，也是反查入口。
    TechnoClass* BunkerLinkedItem;

    // ========================================================================
    // 乘客链（对应原版的 cPassengerNode）：{ 数量, 首节点 }
    //
    //  运输载具用它挂载乘员：首节点是一个 TechnoClass 指针，乘员之间再用
    //  自身的 NextObject 串成单向链。数量字段只做快速判空与容量判断用。
    // ========================================================================
    int32         PassengerCount;
    TechnoClass*  PassengerHead;

    // 根据游戏行为，可知 PassengerCapacityCount 是本单位作为运输载具时
    // 能装下的乘客数；非运输单位恒为零，满员判断据此进行。
    int32         PassengerCapacityCount;

    // 返回链条上的第一个乘客（无乘客时为空）。
    TechnoClass* Attached_Object() const { return PassengerHead; }

    // 把整个乘客链全部"封口"：逐一点掉每位乘客的对外射击许可。
    void BlockAllOpenToppedPassengers();

 // TechnoClass_CanBePermaMC.  True when this techno may be
    // permanently mind-controlled (Yuri Prime's capture).
    bool CanBePermaMC() const;

 // TechnoClass_BelongsToPlayer /
 // _PlayerOwnedAliveAndNamed.  Ownership probes used by the
    // damage text / EVA paths.
    bool BelongsToPlayer() const;
    bool PlayerOwnedAliveAndNamed() const;

 // TechnoClass_GetPointsValue.  The score value this techno
    // contributes when destroyed: the type's PointValue, plus the value of
    // everything it carries, plus the locomotor's contributed value.
    int32 GetPointsValue() const;

 // TechnoClass_GetTiberiumPercentage.  Ore storage fill
    // fraction (0.0..1.0).  Zero when the type has no storage.
    double GetTiberiumPercentage() const;

 // TechnoClass_GetFacingAgain.  Writes the current facing
    // through the out-pointer and returns it.
    DirStruct* GetFacingAgain(DirStruct* pOut) const;

 // TechnoClass_GetDisguiseFlags /
 // _IsDisguisedAgainst.  Disguise-blinking state helpers.
    int32 GetDisguiseFlags(int32 flags) const;
    bool IsDisguisedAgainst(HouseClass* pHouse) const;

    // 根据游戏行为，可知 BlinkDisguise 负责重设"伪装即将被识破"的闪烁计时器：
    //  当该单位对玩家呈现为伪装身份、并且此刻展示的不是玩家自己的伪装外貌
    //  时，把传入时长写入闪烁计时器，让伪装在随后若干帧内周期性地闪现真身；
    //  其余情况下忽略这次写入。闪烁计时器随后由 IsDisguisedAgainst 读取。
    void BlinkDisguise(int32 duration);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 +0x518/+0x51C 这一对保存"当前伪装"的两个分量：
    //  前者是伪装所冒充的外观编号，后者是伪装所归属的房屋指针。
    // ------------------------------------------------------------------------
    int32         DisguiseTypeId;    // +0x518 冒充的外观编号
    HouseClass*   DisguiseHouse;     // +0x51C 伪装归属的房屋


    virtual double GetStoragePercentage() const { return 0.0; }
    virtual int32 GetRefund() const { return 0; }
    virtual BulletClass* Fire(AbstractClass* pTarget, int32 nWeaponIndex) { return nullptr; }
    virtual bool IsClearlyVisibleTo(HouseClass* House) const { return true; }
    virtual bool IsControllable() const { return false; }
    virtual bool IsActive() const { return true; }
    virtual bool IsSelectable() const { return true; }
    virtual bool CanBeSelected() const { return true; }
    virtual bool CanBeSelectedNow() const { return true; }

    // ========================================================================
    // Iron Curtain / Force Shield / damage-effect state
    //
    // The Iron Curtain and Force Shield super weapons grant absolute
    // invulnerability for a bounded number of frames. The damage-effect
    // timers track the secondary warhead effects (fire, sparks, parasite,
    // temporal freeze, gas, radiation) that ApplyToTechno attaches to a
    // target after the primary damage has been resolved.
    // ========================================================================
    bool IsIronCurtained() const { return IronCurtainTimer > 0; }
    bool IsForceShielded() const { return ForceShieldTimer > 0; }
    bool IsShielded() const { return IronCurtainTimer > 0 || ForceShieldTimer > 0; }

    void ApplyIronCurtain(int32 frames) { if (frames > IronCurtainTimer) IronCurtainTimer = frames; }
    void ApplyForceShield(int32 frames) { if (frames > ForceShieldTimer) ForceShieldTimer = frames; }

    bool IsOnFire() const { return FireDamageTimer > 0; }
    void SetOnFire(int32 frames) { if (frames > FireDamageTimer) FireDamageTimer = frames; }

    bool IsSparky() const { return SparkyCounter > 0; }
    void SetSparky(int32 count) { SparkyCounter += count; }

    bool IsParasiteAttached() const { return IsParasited; }
    void SetParasite() { IsParasited = true; }

    bool IsTemporalized() const { return TemporalTimer > 0; }
    void SetTemporal(int32 frames) { if (frames > TemporalTimer) TemporalTimer = frames; }

 // 根据游戏行为，可知 Reload 负责下面这段逻辑。
    //
    //  Ticks the ammo counter up by one once the reload timer has run out and
    //  the magazine is not yet full.  A type with Ammo == -1 never reloads
    //  (infinite magazine); a type with a finite magazine whose timer is
    //  still running is left alone.  On a successful reload the techno is
    //  Mark()ed (ground layer) and Techno_Update_Reloading restarts the timer
    //  for the next round.
    void Reload();

 // Techno_Update_Reloading: recomputes and restarts the
    //  reload delay after a round has been loaded.
    void Update_Reloading();

 // TechnoClass_StartAirstrikeTimer / _StopAirstrikeTimer
 //: arm / disarm the frame window during which a follow-up
    //  airstrike may be requested.  Start also zeroes the generation counter.
    void StartAirstrikeTimer(int32 duration);
    void StopAirstrikeTimer();

    bool IsGassed() const { return GasTimer > 0; }
    void SetGas(int32 frames) { if (frames > GasTimer) GasTimer = frames; }

    bool IsIrradiated() const { return RadiationTimer > 0; }
    void SetRadiation(int32 frames) { if (frames > RadiationTimer) RadiationTimer = frames; }

    int32 GetLastFireFrame() const { return LastFireFrame; }
    void SetLastFireFrame(int32 frame) { LastFireFrame = frame; }

    // ========================================================================
    // State (TechnoClass-specific)
    // ========================================================================
    TechnoTypeClass* TechnoType;        // back-pointer to this techno's type definition
    int32         Health;
    int32         MaxHealth;
    int32         VeterancyLevel;     // 0=Rookie, 1=Veteran, 2=Elite
    int32         Experience;
    CloakStateEnum CloakState;
    uint8         CloakAlpha;         // 255 = fully visible, 0 = fully cloaked
    int32         FireRechargeTimer;  // frames remaining before next shot
    int32         CloakTimer;         // frames remaining in current cloak state
    bool          RepairActive;       // true while a service depot is healing us
    int32         RepairRate;         // HP per frame while being repaired

    // Iron Curtain / Force Shield invulnerability timers (frames remaining).
    int32         IronCurtainTimer;
    int32         ForceShieldTimer;

    // 根据游戏行为，可知钢铁化与空袭都会给单位叠上一层随时间变化的染色：
    // 计时器记录这一段染色的剩余帧数，阶段号记录当前处在染色曲线的哪一段
    // （上色、保持、褪色……）。绘制时按阶段与剩余帧数算出调制系数，再乘到
    // 本体颜色上，于是钢铁单位泛蓝、被空袭锁定的单位泛红。
    int32         IronTintTimer;
    int32         IronTintStage;
    int32         AirstrikeTintTimer;
    int32         AirstrikeTintStage;

    // Frame stamp of the last successful weapon discharge (Game::CurrentFrame).
    // Used by Fire_Impl to gate firing on the weapon's rate of fire.
    int32         LastFireFrame;

    // Layer flag at +0x74: true while this object occupies a tunnel, bridge or
    // similar non-ground layer.  Consulted by IsOnFloor / IsInAir.
    bool          Tunnel;
    uint8         pad_Tunnel[3];

    // Turret-slot bookkeeping.  ActiveTurretIndex addresses +0x124 (the slot
    // the unit is currently drawn with) and CurrentWeaponNumber addresses
    // +0x134; -1 means "no weapon selected".
    int32         ActiveTurretIndex;
    int32         CurrentWeaponNumber;

    // House that owned this object before it was captured.  Only meaningful
    // while Captured is set.  (asm +0x2E0)
    HouseClass*   OrigOwner;
    bool          Captured;

    // ── Disguise / warp / drain state ─────────────────────────────────────
    bool          IsDisguisedFlag;    // +0x81 style disguise flag
    int32         WarpInTimer;        // remaining frames of a chrono-warp-in
    int32         WarpOutTimer;       // remaining frames of a chrono-warp-out
    bool          IsWarpingOutFlag;   // warp-out has completed
    int32         DrainTimer;         // remaining frames of a drain effect

    // Disguise blinking.  `DisguiseCreationFrame` (+0x1DC) remembers when the
    // disguise was taken; `DisguiseBlinkTimer` (+0x1EC / +0x1F4) drives the
    // periodic flicker that tips the player off.  GetDisguiseFlags and
    // IsDisguisedAgainst read both.
    int32         DisguiseCreationFrame;
    CDTimerClass  DisguiseBlinkTimer;

    // Planning-token slot: -1 when the object has no waypoint assigned.
    int32         PlanningToken;

    // Multi-stage weapon counter at +0x140 (gattling / prism style weapons).
    int32         WeaponStage;

    // 根据游戏行为，可知 WeaponStageFrame 记录射速档位最近一次变化的帧，
    //  用于判断升档/降档是否已经到点。
    int32         WeaponStageFrame;

    // ── 炮管后座状态 ──────────────────────────────────────────────────────
    // 根据游戏行为，可知开火瞬间后座拉满（RecoilAmount 为正），随后按帧
    //  复位；RecoilStartFrame 记录后座开始的那一帧。
    int32         RecoilAmount;
    int32         RecoilStartFrame;

    // ── 死亡记账 ──────────────────────────────────────────────────────────
    // 根据游戏行为，可知死亡判重靠 IsDyingNow：Die 進来先看它，置位后重复
    //  调用直接返回；KilledBy 记"谁杀的"，供经验与战果统计使用。
    bool          IsDyingNow;
    TechnoClass*  KilledBy;

    // ── 说话气泡状态 ──────────────────────────────────────────────────────
    // 根据游戏行为，可知单位正在显示气泡时 IsTalking 为真，气泡文本与结束
    //  帧分别记在 TalkBubbleText / TalkBubbleEnd 上；LastTalkFrame 是说话
    //  的最短间隔基准，避免连续触发刷屏。
    bool          IsTalking;
    const wchar_t* TalkBubbleText;
    int32         TalkBubbleEnd;
    int32         LastTalkFrame;

    // 根据游戏行为，可知行军规划令牌除了槽位编号，还要记住该令牌指向的目的
    //  地，单位每帧据此决定下一步往哪走。
    CoordStruct   PlanningDestination;

    // ── 冒烟表现状态 ──────────────────────────────────────────────────────
    // 根据游戏行为，可知建筑受损后冒烟系统被拉起，SmokeSystemStage 表示烟的
    //  浓淡档位；完好时 SmokeSystemActive 复位。
    bool          SmokeSystemActive;
    int32         SmokeSystemStage;

    // 根据游戏行为，可知规划令牌的槽位编号由全局计数器统一分配，保证同一
    //  时刻不会有两条规划撞到同一个槽位。
    static int32  sNextPlanningToken;

    // The building this techno is currently focused on (asm FocusOnUnit).
    BuildingClass* FocusOnUnit;

    // Primary facing (+0x388).  Where the object points.  FootClass keeps its
    // own copy for the locomotor; this is the location the binary's
    // Desired_Facing256 reads for structures and turreted units.
    DirStruct     PrimaryFacing;

    // Height used by the on-floor / in-air tests (+0x68 in the original's
    // type block, mirrored here).  Zero for ground vehicles and structures.
    int32         GroundHeight;

    // Index of a voice line queued for playback, or -1.
    int32         QueuedVoiceIndex;

    // The locomotor COM object driving this techno's every-frame movement
    // (asm +0x674).  May be null for statics such as buildings.
    ILocomotion*  Locomotor;

    // Secondary warhead-effect state. These timers/counters are decremented
    // by Update_AI each frame and consulted by the damage / rendering code.
    int32         FireDamageTimer;    // frames remaining while burning
    int32         SparkyCounter;      // pending spark particle spawns
    bool          IsParasited;        // a parasite is attached to this techno
    int32         TemporalTimer;      // frames frozen by the chronosphere weapon

    // The temporal weapon object this techno is currently applying (asm
    // TemporalImUsing), or null.  IsTemporalSource tests both this and its
    // victim slot.
    void*         TemporalImUsing;
    int32         GasTimer;           // frames affected by gas
    int32         RadiationTimer;     // frames irradiated by a rad warhead

    // ========================================================================
    // Ammunition & reloading (asm currentAmmo / ReloadTimer)
    // ========================================================================
    // CurrentAmmo (+0x[+], asm TechnoClass.currentAmmo): rounds remaining.
    //   Reload adds one round once ReloadTimer has elapsed, up to the type's
    //   Ammo ceiling (-1 = infinite, in which case Reload does nothing).
    int32         CurrentAmmo;
    CDTimerClass  ReloadTimer;

    // ========================================================================
    // Airstrike timer (asm TechnoClass.StartAirstrikeTimer / StopAirstrikeTimer)
    // ========================================================================
    // The airstrike window is a plain frame stamp plus a generation counter,
    // touched by the two helpers below rather than by a real countdown.
    int32         AirstrikeTimeStart;
    int32         AirstrikeTimeLeft;
    int32         AirstrikeTimeGen;

protected:
    explicit TechnoClass(noinit_t) noexcept : ObjectClass(noinit) {}
};
