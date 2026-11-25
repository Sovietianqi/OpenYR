#pragma once

#include "TechnoClass.h"
#include "../Math/Facing.h"
#include "../Containers/DynamicVectorClass.h"

class LocomotionClass;
class TeamClass;

class FootClass : public TechnoClass {
public:
    static const AbstractType AbsID = AbstractType::Foot;

    static DynamicVectorClass<FootClass*>* Array;

    FootClass() noexcept : TechnoClass(), Pitch(0), CurrentSequence(Sequence::Ready), Locomotion(nullptr),
        Team(nullptr), NextTeamMember(nullptr), IsTeamLeader(false), Recruitable(false), Group(-1),
        IsEnteringBioReactor(false), IsEnteringBattleBunker(false), IsEnteringGarrison(false),
        CurrentTarget(nullptr), Transporter(nullptr),
        IsVisibleNowFlag(false), PlanningToken(-1), PlanningTokenIdx(-1), IsUnselectable(false),
        WalkFrameCounter(0), BalloonHoverTainted(false), MoveSoundPlaying(false),
        MoveSoundCooldown(0), IsHunting(false)
    {
        // +0x578 SpeedPercentage：原版构造为 1.0（"正常速度"）。
        //   本树此前没有初始化，值域里是 0，于是 Walk/Drive 的
        //   `if (Owner->SpeedPercentage <= 0) return;` 门恒真 ——
        //   所有单位"在移动"却一步都走不动。
        SpeedPercentage = 1.0;
    }
    virtual ~FootClass() {}

    virtual AbstractType WhatAmI() const override { return AbstractType::Foot; }
    virtual int32 Size() const override { return sizeof(FootClass); }

    // ========================================================================
    // Static Array management
    // ========================================================================
    static void Init_Array();
    static void Delete_Array();
    static int32 Add_To_Array(FootClass* pInstance);
    static bool Remove_From_Array(FootClass* pInstance);
    static int32 Get_Total_Count();
    static FootClass* Get_Instance(int32 index);
    static int32 Find_Index(FootClass* pInstance);

    // ========================================================================
    // Path management
    // ========================================================================
    bool Has_Path() const;
    int32 Get_Path_Length() const;
    CoordStruct Get_Path_At(int32 index) const;
    void Set_Path(const CoordStruct* pCoords, int32 count);
    void Append_Path(const CoordStruct& coord);
    void Clear_Path();
    CoordStruct Peek_Next_Path() const;
    CoordStruct Pop_Next_Path();
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知足类补全原版命名形态：就近机位搜索、规划航点
    // 执行与推进、悬停光标、VXL 阴影绘制与水面尾迹。
    // ------------------------------------------------------------------------
    BuildingClass* FindNearestDock(int32 dockIndex, int32 a3, int32* pRetDistance);
    void ExecutePlanningWaypoint(int32 wpt, const CellStruct& coords, bool flag, const CoordStruct& position);
    void ProceedToNextPlanningWaypoint();
    int32 GetCursor_MouseOverCell(const CellStruct& where, bool a3, bool a4);
    HRESULT DrawVXLShadow(void* pVXL, int32 shadowIndex, int32 a3, int32 a4, const CoordStruct& pos);
    void CreateWake(int32 X, int32 Y, int32 Z);

    // 规划路径推进状态：-1 表示没有进行中的规划路径。
    int32 PlanningPathIndex = -1;
    DynamicVectorClass<CellStruct> PlanningWaypoints;

    // ========================================================================
    // Facing management
    // ========================================================================
    void SetFacing(DirStruct facing);
    DirStruct GetFacing() const;
    void SetTurretFacing(DirStruct facing);
    DirStruct GetTurretFacing() const;
    void SetPitch(int32 pitch) { Pitch = pitch; }
    int32 GetPitch() const { return Pitch; }

    // ========================================================================
    // Sequence
    // ========================================================================
    void SetSequence(Sequence seq) { CurrentSequence = seq; }
    Sequence GetSequence() const { return CurrentSequence; }

    // ========================================================================
    // Coordinate management
    // ========================================================================
    void SetCoords_Impl(const CoordStruct& coord);
    void SetCoords(const CoordStruct& coord);
    CoordStruct GetCoords_Impl() const;

    // ========================================================================
    // Locomotion interface delegation
    // ========================================================================
    void Set_Locomotion(LocomotionClass* pLoco);
    LocomotionClass* Get_Locomotion() const;
    bool Is_Moving() const;
    void Stop_Moving();
    void Move_To(const CoordStruct& coord);
    CoordStruct Get_Destination() const;

    // ========================================================================
    // Update loop for movement
    // ========================================================================
    virtual void Update() override;
    void Update_Movement();

    // ========================================================================
    // FootClass::AI  (asm:481281 .. 482328)
    //
    //  足类逐帧主体。原版由 UnitClass_Update / InfantryClass_Update /
    //  AircraftClass_Update 在各自 Update 里调用（asm 481281 处首行先 call
    //  TechnoClass_Update）。本实现把父类逐帧交给 FootClass::Update() 统一
    //  处理，AI() 只承载足类增量：辐射结算、气球悬停腐蚀、友军视野重揭、
    //  行走帧节流、移动音效、地面散射与传送收尾。
    // ========================================================================
    virtual void AI();

    // ========================================================================
    // Misc inline compatibility methods preserved from the original header
    // ========================================================================
    void SetAlpha(uint8 /*alpha*/) {}
    void PlaySoundEffect(int32 /*soundId*/) {}
    void TakeDamage(int32 /*damage*/, ObjectClass* /*source*/, WarheadTypeClass* /*warhead*/) {}

    // ========================================================================
    // CRC (declared later, next to the other foot-layer overrides)
    // ========================================================================

    // ==========================================================================
 // Mission-controller overrides (xx block of the FootClass vtable)
    //
    //  FootClass supplies the neutral bodies for the movement / morale slots
    //  so that vehicles, infantry and aircraft that do not implement a given
    //  behaviour still occupy the correct vtable entry.
    // ==========================================================================
 // FootClass_Panic (xx): bare retn at this layer.
    virtual void Panic();
    // FootClass_Unpanic: bare retn.
    virtual void Unpanic();
 // FootClass_PlayIdleAnim (x): retn 4 - derived types play their
    // own idle sequence.
    virtual void PlayIdleAnim(int32 a2);
 // FootClass_Draw (xx): retn 8.
    virtual void Draw(int32 a2, int32 a3, int32 a4);

    // ========================================================================
    // Team / layer probes
    // ========================================================================
 // FootClass_PartOfTeam: true when this unit belongs to a
    // team (FootClass::Team != null).
    bool PartOfTeam() const;
 // FootClass_InAir: thunk to the TechnoClass air-layer test.
    bool InAirLayer() const;
 // FootClass_CanAttack: thunk to the type's CanMobileAttack
    // slot (TechnoClass_4C0 -> GetTechnoType()->CanMobileAttack()).
    bool CanAttack() const;

    // ========================================================================
    // Movement helpers
    // ========================================================================
    // 设定行进目的地。
    //   目标格不可进入（被敌方单位/建筑占据、站满 3 人、水面）时，不再直接
    //   失败，而是在目标格周围 1..4 环内取"最贴目标、其次最贴自身"的可站
    //   格作为落点 —— 这就是原版"追击时停在对手身边"的行为。飞机不做降级
    //   （机位/停机坪必须是那一格本身），仍按不可入即失败处理。
    bool Set_Destination(const CoordStruct& dest);
    bool Can_Enter_Cell(const CellStruct& cell) const;

    // 目标格不可入时的替代落点搜索：以 centre 为中心按环半径 1..4 外扩，
    //   环内先比"到 centre 的欧氏平方距离"，再比"到自身的平方距离"。
    //   返回 false 表示 4 环内没有可站格。
    bool Find_Fallback_Cell(const CellStruct& centre, CellStruct& out) const;

    // 逐格 A* 路径（通行判据 = Can_Enter_Cell，含地形与占用）。
    //   outPath[0] 是起点格，末项是终点格；返回 false 表示不可达。
    //   终点格若被占满（例如敌方单位所在的格子已经站了 3 个人），会先在
    //   终点周围找一格可进入的落点再寻路 —— 这样步兵不会强行挤进同一格。
    bool Build_Path(const CellStruct& destCell,
                    DynamicVectorClass<CellStruct>& outPath) const;

    void Scatter(const CoordStruct& from, bool ignoreMission = true);

    // ========================================================================
    // Mission-controller overrides (the FootClass layer of the vtable)
    //
    //  Infantry and vehicles share the same walk-there / enter / get-scattered
    //  behaviour, so the mission handlers that are common to both live here and
    //  are reused by the concrete types.  Each returns the current mission step
    //  (a value <= the pass threshold means "keep going").
    // ========================================================================
    virtual int32 Mission_Move();
    virtual int32 Mission_Attack();
    // 猎杀：TechnoClass_Mi_Hunt 在 FootClass 层的实体。原版由
    //   MissionClass_Mi_Hunt 槽位（跳表 15）分派；基类只 `mov eax,1C2h`
    //   （not-done），真正"往敌方推进 + 沿途索敌"落在这里。
    virtual int32 Mission_Hunt();
    virtual int32 Mission_Guard();
    virtual int32 Mission_AreaGuard();
    virtual int32 Mission_Patrol();
    virtual int32 Mission_Retreat();
    virtual int32 Mission_Enter();
    virtual int32 Mission_Unload();

    // ========================================================================
    // Motion / speed
    // ========================================================================
    // FootClass_GetCurrentSpeed: the unit's effective movement speed after the
    // owning house's speed multiplier, the per-instance multiplier, the FASTER
    // veteran bonus and the SpeedPercentage are folded in.  A harvester
    // carrying ore is halved.
    virtual int32 GetCurrentSpeed() const;

    // ========================================================================
    // Layer / status probes and handlers
    // ========================================================================
    // FootClass_InAir: thunk to the TechnoClass air-layer test.
    virtual bool InAir() const;
    // FootClass_InWhichLayer: the draw layer reported by the locomotor.
    virtual int32 InWhichLayer() const;
    // FootClass_Get_ZAdjustment: the ground-snap height this unit draws at.
    virtual int32 Get_ZAdjustment() const;
    // FootClass_CanCloak: cloakable, and either the type allows cloaking while
    // the locomotor is stationary, or the locomotor permits it outright.
    virtual bool CanCloak() const;
    // FootClass_CanBeSold: a mobile unit may be sold only while carrying
    // nothing and only when it is standing on a service depot.
    virtual bool CanBeSold() const;

    // FootClass_IronCurtain: applies the iron-curtain effect.  Organic units
    // take direct damage instead of being shielded; everything else gets the
    // shield timer and the tint.
    virtual void IronCurtain(ObjectClass* pSource, int32 a3, int32 a4) const;

    // FootClass_Crash: an airborne unit that is about to come down hard fires
    // the crash / destroyed events and drops out of the sky.
    virtual void Crash();

    // FootClass_ChronowarpTo: teleport the unit to a target cell, creating a
    // teleport locomotor and snapping the position when it is a vehicle.
    virtual void ChronowarpTo(AbstractClass* pTarget, int32 a3);

    // FootClass_Destroyed: raise the "unit lost" radar event and EVA line for
    // the local player, then chain to the parent.
    virtual void Destroyed(ObjectClass* Killer) override;

    // ========================================================================
    // Drawing / pick and per-frame updates
    // ========================================================================
    // FootClass_DrawSHP: forwards to the SHP blitter with the 16-argument
    // signature the original uses.
    void DrawSHP(void* pSHP, int32 frameCount, int32 a3, int32 a4, int32 a5,
                 int32 a6, int32 zDelta, int32 a8, int32 a9, int32 a10,
                 int32 tintColour, int32 buildingZ_sha, int32 zsX, int32 zsY,
                 int32 a15, int32 a16);
    // FootClass_GetImage: the drawing image (SHP plus frame) for this unit.
    void* GetImage(void* pImage, bool a2);
    // FootClass_GetCursorOverObject: the mouse cursor to show over a target.
    virtual int32 GetCursorOverObject(AbstractClass* pTarget, int32 a3);
    // FootClass_UpdatePosition: the per-frame position / layer bookkeeping.
    void UpdatePosition(int32 a2) const;

    // ========================================================================
    // Lifecycle / bookkeeping
    // ========================================================================
    // FootClass_UnInit: tears the unit down - releases any capture manager,
    // detaches from its transporter, leaves its team and unlinks from the map.
    virtual void UnInit();
    // FootClass_ResetMission: restores the saved mission and re-issues the
    // stored destination.
    virtual bool ResetMission();
    // FootClass_AssignPlanningPath: starts following the player's planning path
    // `idxPath` from waypoint `idxWP`, or clears the path when idxPath is -1.
    void AssignPlanningPath(int32 idxPath, uint8 idxWP);
    // FootClass_Exists: true when the unit is live, has not been removed, and
    // (unless the scenario is initialising) is not limboed.
    bool Exists() const;
    // FootClass_PointerGotInvalid: clears any internal pointer that still names
    // the object being invalidated.
    // asm: FootClass_PointerGotInvalid proc near
    //          coords = dword ptr 4   ; a2
    //          a3     = dword ptr 8   ; int removed
    //          (ends with `retn 8` -> __thiscall, 2 args, virtual)
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed);
    // FootClass_GetCRC: chains the parent CRC and mixes in the foot-layer
    // state (paralysis timer, speeds, facings).
    virtual void ComputeCRC(CRCEngine& crc) const override;
    // FootClass_ReceiveDamage: the foot-layer damage entry point.
    virtual int32 ReceiveDamage(int32* pDamage, int32 typeSource,
                                WarheadTypeClass* pWarhead, TechnoClass* pAttacker,
                                int32 ignoreDefenses, int32 arg10,
                                HouseClass* pSourceHouse);
    // FootClass_ClickedMission: handles a left-click selecting this unit for a
    // mission (currently only the "enter building" click is meaningful).
    virtual bool ClickedMission(int32 a2, int32 a3, int32 a4) const;

    // FootClass_LoadTables: called by the container loaders after a saved unit
    // has been reconstructed.  Chains the parent, re-stamps the pathfinding
    // vectors' vtable, clears the (stale) locomotion pointer and re-installs
    // the four interface vtables.
    virtual void LoadTables(IStream* pStm) override;

    // ========================================================================
    // Original vtable slots the disassembler left unnamed (foot-layer offsets
    // 0xC0 / 0x138 / 0x2F0 / 0x47C / 0x4AC)
    // ========================================================================
    // FootClass_138 (vt+0x138): a unit may be selected unless it is flagged as
    // temporarily unselectable (the "hidden" byte at +0x6A9).
    virtual bool CanBeSelected() const override;
    // FootClass_C0 (vt+0xC0): the "visible in the current draw pass" byte at
    // +0x6B2, cleared every frame by UpdatePosition.
    bool IsVisibleNow() const;
    // FootClass_2F0: the height gradient reported by the locomotor, or 2 when
    // the unit has no locomotion.
    int32 GetZ_Gradient() const;
    // FootClass_47C: stores the waypoint token at +0x5A0.
    void Set_Planning_Token(int32 token);
    // FootClass_4AC: true when the planning-token slot (+0x5C4) holds a valid
    // index (i.e. is not -1).
    bool Has_Planning_Token() const;

    DirStruct PrimaryFacing;
    DirStruct TurretFacing;
    int32 Pitch;
    Sequence CurrentSequence;
    DynamicVectorClass<CoordStruct> Path;
    LocomotionClass* Locomotion;

    // ========================================================================
    // Team membership links
    //
    //  The AI team system threads every member of a team together.  The
    //  original stores the owning team at FootClass+0x5D0 ("PartOfTeam",
    //  reached as _FootClass+0x5D4), the next member in the chain at +0x5D4
    //  ("NextUnitInTeam", _FootClass+0x5D8) and the leader flag at +0x685
    //  ("Team_Leader", _FootClass+0x689).  TeamClass::RecruitUnit fills these
    //  in; TeamClass::Remove clears them.
    // ========================================================================
    TeamClass* Team;            // +0x5D0  owning team (null when unattached)
    FootClass* NextTeamMember;  // +0x5D4  next unit on the team's member chain
    bool       IsTeamLeader;    // +0x685  true when this unit leads its team

    // The per-unit "recruitable" byte at +0x421 and the current group id the
    // team hands out at +0x214.
    bool       Recruitable;
    int32      Group;

    // ========================================================================
    // Motion / status state
    // ========================================================================

    // ParalysisTimer (+0x528/+0x530): disabled by a warhead such as the
    // EMPulse or a temporal weapon.  IsParalysed reports whether it is still
    // ticking.
    CDTimerClass ParalysisTimer;

    // SpeedPercentage (+0x578): a double multiplier applied to the
    // locomotor's speed.  1.0 is normal.
    double      SpeedPercentage;

    // SensorArrayRadius (+0x5F0 in the type): the radius in cells this unit's
    // sensors sweep.  Sensors_AddAt / Sensors_RemoveAt stamp the owner's bit
    // into every cell of the circle.
    int32       SensorArrayRadius;

    // ThreatValue (+0x508): the last threat this unit contributed to the
    // cell it occupies.  RemoveThreatFromCell subtracts exactly this back out.
    int32       ThreatValue;

    // TunnelNumber (+0x8C): the tube this unit is travelling through, or a
    // negative value when it is not in a tunnel.
    int8        TunnelNumber;

    // TargetingTimer (+0x180/+0x188): paces how often a unit re-evaluates
    // its target while on area guard.
    CDTimerClass TargetingTimer;

    // 根据游戏行为，可知 CurrentTarget 记录本单位当前攻击/前往的目标：
    // 任务处理器在读它决定该往哪走，换目标时先清空再写新的。
    AbstractClass* CurrentTarget;

    // 根据游戏行为，可知 Transporter 指向"正载着本单位"的那一个运输载具，
    // 空则表示本单位自主行动在地图上。
    TechnoClass* Transporter;

    // ========================================================================
    // Motion / status probes
    // ========================================================================
    // asm vt+0x380 —— FootClass_IsParalysed：ParalysisTimer 未走完即为真。
    //   覆写 TechnoClass 的恒假基类实现，供 CanSetWaypoint / Can_Be_Commanded
    //   这类基类虚调用正确落到脚类实体上。
    bool  IsParalysed() const override;
    void  SetSpeedPercentage(double pct);
    int32 GetDistance(const CoordStruct& other) const;
    CoordStruct GetCoords_unknown1() const;
    bool  SetLayer(int32 layer);
    bool  CanGetCrushed(ObjectClass* pSource) const;
    bool  CanBeRecruited(HouseClass* pHouse) const;
    bool  CanFightBack() const;

    // ------------------------------------------------------------------------
    // asm: FootClass::Greatest_Threat (_FootClass *this, int projFlags,
    //        int curThreat, char a4)   (asm:5770)   retn 0Ch
    //
    //   追猎状态（+0x688）为真时，强制把 projFlags 的"只打可移动目标"位
    //   （bit1）清掉、并置上"优先"位（bit0），再交给 TechnoClass 的通用
    //   威胁评估；若追猎状态为真却评估不到任何目标，则自动摘掉追猎位。
    // ------------------------------------------------------------------------
    virtual ObjectClass* Greatest_Threat(int32 projFlags, int32 curThreat, int32 a4) override;

    // ------------------------------------------------------------------------
    // asm: FootClass::Captured (_FootClass *this, HouseClass *newOwner, int a3)
    //        (asm:7874)   retn 8
    //
    //   被占领 / 心灵控制易主：先把本单位从所在格的传感器圈里撤下（若类型
    //   带传感器视野），链到 TechnoClass::Captured 完成易主；成功后退出所属
    //   队伍、按新位置重挂传感器；无论成败都按新归属重挂传感器视野。
    // ------------------------------------------------------------------------
    bool Captured(HouseClass* newOwner, int32 a3 = 1);
    void  Sensors_AddAt(const CellStruct& cell);
    void  Sensors_RemoveAt(const CellStruct& cell);
    void  AddThreatIntoCell(class CellClass* pCell);
    void  RemoveThreatFromCell(class CellClass* pCell);
    void  AbandonHunt();
    bool  UpdateTargetingTimer();

    // ------------------------------------------------------------------------
    // asm: TechnoClass::Target_Something_Nearby(this, arg0, arg4)   (asm:1590090)
    //
    //   空闲单位的"目标重评"入口：
    //     1. 重设定目标计时器：StartTime=Frame，TimeLeft = Rules 的重评延迟
    //        （AreaGuard 用 GuardAreaTargetingDelay，其余用 NormalTargetingDelay）
    //        + Random(0,2)；
    //     2. 已有目标、但开火状态是 5/6/8（无武器 / 超出射程 / 目标不能打）→
    //        丢目标；
    //     3. 没有目标 → 在警戒半径内重新索敌并设为目标。
    //   返回"评估后是否持有目标"。
    // ------------------------------------------------------------------------
    bool  Target_Something_Nearby(int32 arg0, int32 arg4);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 FindNearestOfBuildingsOfTypes 负责在给定的建筑类型
    //  数组里，找出离本单位最近、且通过"可停靠/可使用"判定的那一座：遍历
    //  类型数组里的每一座建筑，命中判定后与当前最近者比较距离取最近；若最近
    //  者已确定且新候选带"强制优先"标记则直接改选。全部不合格时返回空。
    // ------------------------------------------------------------------------
    BuildingClass* FindNearestOfBuildingsOfTypes(
        const DynamicVectorClass<BuildingTypeClass*>& types,
        int32 a3, int32 a4);

    // ========================================================================
    // 进入类建筑（进建筑 / 驻守 / 掩体 / 吞噬 / 反应堆）的挑选与启动
    //
    //  这一组函数共享同一套流程：把本单位自身作为候选，在所有已知建筑里按
    //  类型条件过滤后取最近的一座，命中后设置目的地并转入相应的任务。原版把
    //  它们挂在"进入建筑"的各个候选类型上，由目标类型分发到这里。
    // ========================================================================
    bool EnterGrinder(TechnoClass* pTarget);        // 粉碎机：直接吞噬
    bool EnterBioReactor(TechnoClass* pTarget);     // 生物反应堆：当作燃料
    bool EnterBattleBunker(TechnoClass* pTarget);   // 战斗碉堡：步兵驻守射击
    bool EnterTankBunker(TechnoClass* pTarget);     // 坦克掩体：载具隐蔽
    bool GarrisonStructure(TechnoClass* pTarget);   // 民房进驻

    // ========================================================================
    // 地面单位的通用任务处理器
    //
    //  步兵与载具共用同一套行为，因此这些任务处理器放在基类里，由具体单位
    //  类型复用。每个入口返回当前任务步进值（0 表示继续，非 0 表示完成）。
    // ========================================================================
    int32 Mi_Capture();
    int32 Mi_Eaten();
    int32 Mi_Rescue();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 SetNewTarget 负责给地面单位换一个目标：先解除与
    //  原目标的关联，再记下新目标并复位任务步进。
    // ------------------------------------------------------------------------
    void SetNewTarget(AbstractClass* pTarget);

    // ------------------------------------------------------------------------
    // 目标字段的唯一写入口：覆写 TechnoClass 的空实现，只负责写 FootClass
    //  层这一份 CurrentTarget。
    //
    //  必须由派生类的 SetTarget 显式调用（UnitClass / InfantryClass /
    //  AircraftClass 各自还存着一份目标指针）。否则 UpdateTargeting 读
    //  CurrentTarget、派生类读自己的字段，两边看到的是不同目标 ——
    //  表现为"永远不索敌、Mission_Enter 永远不真的进建筑"。
    //
    //  这里覆写的是 TechnoClass 已有的虚表槽位，不新增槽位、不改布局。
    // ------------------------------------------------------------------------
    virtual void SetTarget(AbstractClass* pTarget) override;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 EnterAsPassenger 负责让本单位以乘客身份进入某个
    //  运输载具：客位占得下就上车，把坐标挪到载具身上并从地面上摘掉。
    // ------------------------------------------------------------------------
    bool EnterAsPassenger(TechnoClass* pTransport);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 LetGoOfUnit 负责让本单位与运送者脱钩，重新回到
    //  地图上自主行动。
    // ------------------------------------------------------------------------
    void LetGoOfUnit();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 FindNearestDock 负责为本单位找一个最近的可停靠
    //  建筑：遍历地图上的建筑，按停靠要求筛选（类型允许、当前空着），在
    //  合格者中取距离最近的一座；距离通过输出参数带回。
    //  没有可停靠的目标时返回空。
    // ------------------------------------------------------------------------
    BuildingClass* FindNearestDock2(int32 idx, int32 a3, int32 a4, int* pRetDistance);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ImbueLocomotor 负责给本单位换上一套新的移动方式：
    //  按给定的移动类型标识新建一套行走逻辑，替换旧的那套，并把当前坐标与
    //  朝向交过去，使单位立刻按新方式行动。
    // ------------------------------------------------------------------------
    bool ImbueLocomotor(const void* pClassId, AbstractClass* pTarget);

    // 派生类型实现：按移动类型标识新建一套行走方式并接管本单位；基类无从
    // 新建，返回假。
    virtual bool ImbueLocomotion(const void* pClassId) { (void)pClassId; return false; }

    // 进入流程的中间状态位：本帧正在走向上述某一类目标。用完即清。
    bool IsEnteringBioReactor;
    bool IsEnteringBattleBunker;
    bool IsEnteringGarrison;

    // ========================================================================
    // 原版 vt+0xC0 / 0x47C / 0x4AC 三个槽位背后的状态：
    //   IsVisibleNowFlag (+0x6B2)：本帧可否绘制；每帧 UpdatePosition = 2 时清空。
    //   PlanningToken   (+0x5A0)：vt+0x47C 写入的规划航点令牌。
    //   PlanningTokenIdx(+0x5C4)：vt+0x4AC 读取的规划索引；-1 表示无。
    //   IsUnselectable  (+0x6A9)：vt+0x138 判定用的"暂不可选"标记。
    // ========================================================================
    bool  IsVisibleNowFlag;
    int32 PlanningToken;
    int32 PlanningTokenIdx;
    bool  IsUnselectable;

    // ========================================================================
    // FootClass::AI 的逐帧暂存状态（asm 各分支读写的私有槽位）
    //
    //  这些槽位在 asm 里散落在 FootClass 尾部（+0x3D5 / +0x538 / +0x53C /
    //  +0x6B8 等），只服务于 FootClass::AI 一轮里的判定与节流，不参与存档。
    // ========================================================================
    // WalkFrameCounter (+0x6B8): 行走帧计数（走路动画节流用）。
    int32 WalkFrameCounter;
    // BalloonHoverTainted (+0x3D5): 气球悬停单位落到被泰伯利亚污染的格子上
    //   后置位，避免每帧重复检测。
    bool  BalloonHoverTainted;
    // MoveSoundPlaying (+0x538) / MoveSoundCooldown (+0x53C): 移动音效正在
    //   播放 / 收尾倒计时（行走逻辑停下后还要多响几帧）。
    bool  MoveSoundPlaying;
    int32 MoveSoundCooldown;
    // SightRevealTimer (+0x65C/+0x660/+0x664): 友军空中单位周期性重揭视野
    //   的节流计时器（帧起点 + 上次揭开时的 Z + 间隔）。
    CDTimerClass SightRevealTimer;
    // IsHunting (+0x688): 单位正处于"主动追猎"状态。真时 Greatest_Threat
    //   会强制带上"优先锁定可移动目标"的标志，并在找不到目标时自动摘掉。
    bool IsHunting;


protected:
    explicit __forceinline FootClass(noinit_t) noexcept : TechnoClass(noinit), Pitch(0), CurrentSequence(Sequence::Ready), Locomotion(nullptr),
        Team(nullptr), NextTeamMember(nullptr), IsTeamLeader(false), Recruitable(false), Group(-1),
        IsEnteringBioReactor(false), IsEnteringBattleBunker(false), IsEnteringGarrison(false),
        CurrentTarget(nullptr), Transporter(nullptr),
        IsVisibleNowFlag(false), PlanningToken(-1), PlanningTokenIdx(-1), IsUnselectable(false) {}
};
