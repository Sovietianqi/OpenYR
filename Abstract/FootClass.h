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
        CurrentTarget(nullptr), Transporter(nullptr) {}
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
    // Misc inline compatibility methods preserved from the original header
    // ========================================================================
    void SetAlpha(uint8 /*alpha*/) {}
    void PlaySoundEffect(int32 /*soundId*/) {}
    void TakeDamage(int32 /*damage*/, ObjectClass* /*source*/, WarheadTypeClass* /*warhead*/) {}

    // ========================================================================
    // CRC
    // ========================================================================
    virtual void ComputeCRC(CRCEngine& crc) const override;

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
    bool Set_Destination(const CoordStruct& dest);
    bool Can_Enter_Cell(const CellStruct& cell) const;
    void Scatter(const CoordStruct& from, bool ignoreMission = true);

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
    bool  IsParalysed() const;
    void  SetSpeedPercentage(double pct);
    int32 GetDistance(const CoordStruct& other) const;
    CoordStruct GetCoords_unknown1() const;
    bool  SetLayer(int32 layer);
    bool  CanGetCrushed(ObjectClass* pSource) const;
    bool  CanBeRecruited(HouseClass* pHouse) const;
    bool  CanFightBack() const;
    void  Sensors_AddAt(const CellStruct& cell);
    void  Sensors_RemoveAt(const CellStruct& cell);
    void  AddThreatIntoCell(class CellClass* pCell);
    void  RemoveThreatFromCell(class CellClass* pCell);
    void  AbandonHunt();
    bool  UpdateTargetingTimer();

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


protected:
    explicit __forceinline FootClass(noinit_t) noexcept : TechnoClass(noinit), Pitch(0), CurrentSequence(Sequence::Ready), Locomotion(nullptr),
        Team(nullptr), NextTeamMember(nullptr), IsTeamLeader(false), Recruitable(false), Group(-1),
        IsEnteringBioReactor(false), IsEnteringBattleBunker(false), IsEnteringGarrison(false),
        CurrentTarget(nullptr), Transporter(nullptr) {}
};
