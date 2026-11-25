#pragma once

#include <Abstract/FootClass.h>
#include <COM/IUnknown.h>

// Forward declarations
class TargetClass;
class INIClass;
class BuildingTypeClass;

// ============================================================================
// AircraftClass - base for all aircraft units
// Inherits FootClass, also implements IFlyControl
// ============================================================================
class NOVTABLE AircraftClass : public FootClass, public IFlyControl {
public:
    static const AbstractType AbsID = AbstractType::Aircraft;
    static DynamicVectorClass<AircraftClass*>* Array;

    // ========================================================================
    // IPersistStream
    // ========================================================================
    virtual HRESULT __stdcall Load(IStream* pStm) override;
    virtual HRESULT __stdcall Save(IStream* pStm, BOOL fClearDirty) override;

    // ========================================================================
    // Destructor
    // ========================================================================
    virtual ~AircraftClass();

    // ========================================================================
    // TechnoClass overrides
    // ========================================================================
    virtual bool IsVoxel() const override;
    virtual void Destroyed(ObjectClass* Killer) override;
    virtual bool CanScatter() const override;
    virtual int32 GetDefaultSpeed() const override;
    virtual bool IsInAir() const override;
    virtual bool IsOnFloor() const override;

    // ========================================================================
    // IFlyControl
    // ========================================================================
    virtual int32 __stdcall Landing_Altitude() override;
    virtual int32 __stdcall Landing_Direction() override;
    virtual LONG __stdcall Is_Loaded() override;
    virtual LONG __stdcall Is_Strafe() override;
    virtual LONG __stdcall Is_Fighter() override;
    virtual LONG __stdcall Is_Locked() override;

    // ========================================================================
    // AircraftClass virtuals
    // ========================================================================
    virtual bool IsFlying() const;
 // AircraftClass_GetPoseDir: the shared RulesData pose
    // direction used to orient parked aircraft.
    int32 GetPoseDir() const;
 // AircraftClass_IsGroundUnit: forwards to the OnFloor
    // vtable slot so the aircraft reports as grounded while parked.
    bool IsGroundUnit() const;
    virtual bool IsLandingNow() const;
    virtual bool IsTakingOffNow() const;
    virtual void Fly();
    virtual void Land();
    virtual void TakeOff();
    virtual void Dock();
    virtual void Undock();
    virtual bool CanDockAt(BuildingClass* pBuilding) const;
    virtual bool IsDocked() const;
    virtual int32 GetAltitude() const;
    virtual void SetAltitude(int32 alt);
    virtual void SpawnParachuted(const CoordStruct& coords);

    // ========================================================================
    // Bring parent class methods into scope to prevent hiding
    // (SetCoords and TakeDamage are not brought in because AircraftClass
    //  declares new overloads with different signatures that would conflict.)
    // ========================================================================
    using ObjectClass::GetCoords;
    using TechnoClass::Cloak;
    using TechnoClass::Scatter;

    // ========================================================================
    // AircraftClass specific virtuals - flight, combat, docking
    // ========================================================================
    virtual bool CanFireNow() const;
    virtual bool CanEnterCell(CellClass* pCell) const;
    virtual void EnteredCell();
    virtual void ExitCell();
    virtual void Crash();
    virtual void CircleTarget(CoordStruct target);
    virtual bool StrafeTarget(TechnoClass* pTarget, int32 weaponIndex);
    virtual void ReturnToBase();
    virtual bool NeedToReturn() const;
    virtual void SetTarget(AbstractClass* pTarget);
    virtual AbstractClass* GetTarget() const;
    virtual void ClearTarget();
    virtual bool HasTarget() const;
    virtual void Patrol(CoordStruct point);
    virtual void AttackMove(CoordStruct point);
    virtual bool IsAttacking() const;
    virtual bool IsPatrolling() const;
    virtual bool IsReturning() const;
    virtual void SetMission(Mission mission);
    virtual Mission GetMission() const;
    virtual void QueueMission(Mission mission);
    virtual Mission GetQueuedMission() const;

    // ========================================================================
    // 开火入口 / 任务跳表覆写（asm: AircraftClass::Fire_At 3306 /
    //   Mi_Attack / Mi_Move / Mi_Guard / Mi_Hunt / Mi_Retreat / Mi_Enter /
    //   Mi_Unload）
    //
    //  飞机的任务跳表与地面单位共用同一批槽位，但每个槽位都换成 Mi_*
    //  系列的"先选空域、再决定悬停/掠过"的实现。这组覆写就是 asm 里
    //  vftable 指向 Mi_* 的那几个槽位本身。
    // ========================================================================
    virtual BulletClass* Fire(AbstractClass* pTarget, int32 nWeaponIndex) override;
    virtual int32 Mission_Attack() override;
    virtual int32 Mission_Move() override;
    virtual int32 Mission_Guard() override;
    virtual int32 Mission_Hunt() override;
    virtual int32 Mission_Return() override;
    virtual int32 Mission_Enter() override;
    virtual int32 Mission_Unload() override;
    virtual int32 Mission_Capture() override;

    // ------------------------------------------------------------------------
    // asm: __thiscall AircraftClass_UpdateTurret(AircraftClass *this,
    //          AbstractClass *target)
    //  机载炮塔逐帧：求目标方向的期望朝向，按机体转向率限速靠拢。
    // ------------------------------------------------------------------------
    void Update_Turret(AbstractClass* pTarget);
    virtual void MissionAttack();
    virtual void MissionMove();
    virtual void MissionGuard();
    virtual void MissionSleep();
    virtual void MissionHunt();
    virtual void MissionReturn();
    virtual void MissionStop();
    virtual void MissionCircle();
    virtual void MissionStrafe();
    virtual void MissionParaDrop();
    virtual void MissionSpyPlane();
    virtual void UpdateMission();
    virtual void AI_Update();
    virtual void Combat_AI();
    virtual void Movement_AI();
    virtual void Fire_At(TargetClass* pTarget, int32 weaponIndex);
    virtual bool Can_Fire_At(TechnoClass* pTarget, int32 weaponIndex) const;
    virtual int32 GetWeaponRange(int32 weaponIndex) const;
    virtual int32 GetWeaponDamage(int32 weaponIndex) const;
    virtual CoordStruct GetFireCoords(int32 weaponIndex) const;
    virtual void MuzzleFlash(int32 weaponIndex);
    virtual void OnFired(int32 weaponIndex);
    virtual bool SelectWeapon(int32 weaponIndex);
    virtual int32 GetSelectedWeapon() const;
    virtual int32 GetWeaponCount() const;
    virtual void TakeDamage(int32 damage, TechnoClass* pSource, WarheadTypeClass* pWarhead);
    virtual void OnDestroyed();
    virtual void OnCaptured(HouseClass* pNewOwner);
    virtual void OnVeterancyUp();
    virtual void Draw(Point2D& point, RectangleStruct& rect);
    virtual void DrawVoxel(Point2D& point, RectangleStruct& rect, int32 brightness, int32 tint);
    virtual void DrawShadow(Point2D& point);
    virtual int32 GetZBias() const;
    virtual bool IsVisibleTo(HouseClass* pHouse) const;
    virtual void RevealTo(HouseClass* pHouse);
    virtual void ShroudFrom(HouseClass* pHouse);
    virtual int32 GetSightRange() const;
    virtual int32 GetArmor() const;
    virtual int32 GetMaxHealth() const;
    virtual int32 GetHealth() const;
    virtual void SetHealth(int32 hp);
    virtual bool IsAlive() const;
    virtual bool IsDead() const;
    virtual bool IsDamaged() const;
    virtual bool IsGreenHP() const;
    virtual bool IsYellowHP() const;
    virtual bool IsRedHP() const;
    virtual float GetHealthRatio() const;
    virtual void Repair(int32 amount);
    virtual void Kill();
    virtual void Detach();
    virtual void Attach();
    virtual bool CanDeploy() const;
    virtual bool CanEnter() const;
    virtual bool CanBeEntered() const;
    virtual bool CanCrate() const;
    virtual void CreateCrate();
    virtual void PickUpCrate();
    virtual int32 GetValue() const;
    virtual int32 GetCost() const;
    virtual int32 GetBuildTime() const;
    virtual int32 GetSpeed() const;
    virtual int32 GetROF() const;
    virtual DirStruct GetDirection() const;
    virtual void SetDirection(DirStruct dir);
    virtual DirStruct GetTurretDir() const;
    virtual void SetTurretDir(DirStruct dir);
    virtual CoordStruct GetCoords() const;
    virtual void SetCoords(CoordStruct coords);
    virtual CoordStruct GetDestination() const;
    virtual void SetDestination(CoordStruct dest);
    virtual void Stop();
    virtual void Scatter();

    // ========================================================================
    // 任务处理器
    //
    //  飞机的"散开"任务：只有当任务配置表里允许散开时才真正执行，否则原地
    //  不动。散开的具体动作交给对象自己的散开实现。
    // ========================================================================
    void Mi_Scatter(void* pParam);

    // ========================================================================
    // 飞机专用任务处理器
    //
    //  下面这一组是飞机对基础任务的重写：同样的任务名，但飞机的执行方式与
    //  地面单位不同——它要先决定飞往哪个空域、悬停还是掠过、以及是否要让
    //  乘员跳伞。每一个入口都在基础任务之上追加了飞机特有的落点与高度处理，
    //  返回当前任务步进值（0 表示继续，非 0 表示任务已完成）。
    // ========================================================================
    int32 Mi_Attack();
    int32 Mi_Guard();
    int32 Mi_Hunt();
    int32 Mi_Move();
    int32 Mi_Retreat();
    int32 Mi_Enter();
    int32 Mi_Unload();
    int32 Mi_Patrol();
    int32 Mi_AreaGuard();
    int32 Mi_ParaDropApproach();
    int32 Mi_ParaDropOverfly();
    int32 Mi_SpyPlaneApproach();
    int32 Mi_SpyPlaneOverfly();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ReplaceDestination 负责设定飞机下一个要去的空域：
    //  若目的地已经就是目标点则原地不动；否则记下新目的地，并把"已到达"
    //  标记清掉，让飞机重新朝新点飞。
    // ------------------------------------------------------------------------
    void ReplaceDestination(AbstractClass* pDest);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 SetNewTarget 负责给飞机换一个攻击目标：先解除与
    //  原目标的关联，再记下新目标并复位任务步进，使任务从攻击的第一步重新
    //  开始。
    // ------------------------------------------------------------------------
    void SetNewTarget(AbstractClass* pTarget);

    virtual void Hold();
    virtual void Deploy();
    virtual void Undeploy();
    virtual bool IsMoving() const;
    virtual bool IsFiring() const;
    virtual bool IsIdle() const;
    virtual void SetIdle();
    virtual void Freeze();
    virtual void Unfreezeze();
    virtual bool Limbo();
    virtual bool Unlimbo();
    virtual bool InLimbo() const;
    virtual void Mark(MarkType mark);
    virtual void Unmark();
    virtual bool IsMarked() const;
    virtual void Sync();
    virtual void Unsync();
    virtual void Lock();
    virtual void Unlock();
    virtual bool IsLocked() const;
    virtual void Disable();
    virtual void Enable();
    virtual bool IsDisabled() const;
    virtual void Activate();
    virtual void Deactivate();
    virtual bool IsActive() const;
    virtual void Cloak();
    virtual void Decloak();
    virtual bool IsCloaked() const;
    virtual void SetCloak(bool on);
    virtual void EMPulse();
    virtual void UnEMP();
    virtual bool IsEMPed() const;
    virtual void IronCurtain();
    virtual void UnIronCurtain();
    virtual bool IsIronCurtained() const;
    virtual void ForceShield();
    virtual void UnForceShield();
    virtual bool IsForceShielded() const;
    virtual void ChronoShift();
    virtual bool CanChronoShift() const;
    virtual void TemporalWarp();
    virtual void UnTemporal();
    virtual bool IsTemporalWarped() const;
    virtual void MindControl(TechnoClass* pTarget);
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知飞行控制面板补全原版命名形态的槽位：COM 三槽
    // 转发，就绪/锁定/载荷/姿态查询读自身状态，着陆朝向取第一个链路的
    // 朝向。
    // ------------------------------------------------------------------------
    HRESULT FlasherClass_QueryInterface(REFIID iid, void** ppvObject);
    ULONG FlasherClass_AddRef();
    ULONG FlasherClass_Release();
    bool FlasherClass_IsFighter() const;
    bool FlasherClass_IsLocked() const;
    bool FlasherClass_IsLoaded() const;
    bool FlasherClass_IsStrafe() const;
    int32 FlasherClass_LandingAltitude() const;
    void FlasherClass_LandingDirection(DirStruct* pOut) const;
    virtual void UnMindControl();
    virtual bool IsMindControlled() const;
    virtual void Parasite(TechnoClass* pHost);
    virtual void UnParasite();
    virtual bool IsParasited() const;
    virtual void Disguise();
    virtual void UnDisguise();
    virtual bool IsDisguised() const;
    virtual void Berzerk();
    virtual void UnBerzerk();
    virtual bool IsBerzerk() const;
    virtual void Panic();
    virtual void UnPanic();
    virtual bool IsPanicked() const;
    virtual void Stun();
    virtual void UnStun();
    virtual bool IsStunned() const;
    virtual void Sleep();
    virtual void Wake();
    virtual bool IsSleeping() const;

    // ========================================================================
    // 反射 / 类型信息（asm: AircraftClass_GetType / _GetSize /
    // _GetAbstractDerivationID / _GetClassID / _GetUIName）
    //
    //  GetType 直接返回机体类型；Size 返回原版 AircraftClass 的结构体字节数
    //  (0x6D8)；GetAbstractDerivationID 返回 AbstractType::Aircraft（2）；
    //  GetClassID 把本类的 CLSID 写进出参并返回 S_OK；GetUIName 取类型名。
    // ========================================================================
    virtual TechnoTypeClass* GetType() const override;
    virtual AbstractType WhatAmI() const override { return AbstractType::Aircraft; }
    virtual AircraftTypeClass* Get_Aircraft_Type() const;
    virtual int32 Size() const override;
    virtual AbstractType GetAbstractDerivationID() const;
    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual const wchar_t* GetUIName() const;

    // ========================================================================
    // 地面/空中判定覆写（asm: AircraftClass_InAir / _OnFloor / _IsGroundUnit）
    //
    //  当机体类型属于"机场绑定/悬停"两类时，改由 IFlyControl 载荷查询决定
    //  是否在空中；否则退回 FootClass / TechnoClass 的基础判定。
    // ========================================================================
    bool InAir() const;
    bool OnFloor() const;
    virtual bool IsGroundUnit_Impl() const;

    // ========================================================================
    // 通用生命周期（asm: AircraftClass_Init / _GetCRC / _PointerGotInvalid /
    // _StartMission / _ReceivedRadioCommand / _ReceiveDamage / _Put /
    // _KickOutUnit / _ClickedMission / _See / _Draw / _Update）
    // ========================================================================
    virtual void Init() override;
    virtual void Update() override;
    virtual void Draw(int32 a2, int32 a3, int32 a4) override;

    int32 GetCRC(int32* pCRC) const;
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool bRemoved) override;
    bool StartMission();
    int32 ReceivedRadioCommand(int32 cmd, int32 arg0, int32 a4);
    int32 ReceiveDamage(int32* pDamage, int32 typeSource,
                        WarheadTypeClass* pWarhead, TechnoClass* pAttacker,
                        int32 ignoreDefenses, int32 arg10,
                        HouseClass* pSourceHouse);
    virtual bool Put(const CoordStruct& coords, int32 a2) override;
    bool KickOutUnit(FootClass* pUnit) const;
    bool ClickedMission(int32 a2, int32 a3, int32 a4) const;
    void See(int32 a2, int32 a3) const;

    // ========================================================================
    // 光标 / 活动状态（asm: AircraftClass_GetCursorOverObject / _OverCell /
    // _GetObjectActivityState）
    // ========================================================================
    int32 GetCursorOverObject(int32 currentCursor, bool a3) const;
    int32 GetCursorOverCell(const CellStruct& cell, bool a3, bool a4) const;
    int32 GetObjectActivityState(AbstractClass* pTarget, int32 idxWeapon,
                                 int32 ignoreCloseEnough);

    // ========================================================================
    // 飞行落点与装载（asm: AircraftClass::New_LZ /
    // ::Cell_Seems_Ok / ::Desired_Load_Dir / ::Fire_Direction /
    // ::Find_Docking_Bay / ::Drop_Off_Cargo /
    // ::Do_Mission_Move_Carryall）
    // ========================================================================
    AbstractClass* New_LZ(FootClass* pTarget);
    bool Cell_Seems_Ok(const CellStruct& coords, bool a3) const;
    bool Desired_Load_Dir(TechnoClass* pUnit, CoordStruct* pOutCoords) const;
    DirStruct Fire_Direction() const;
    BuildingClass* Find_Docking_Bay(
        const DynamicVectorClass<BuildingTypeClass*>& types, int32 a3, int32 a4);
    bool Drop_Off_Cargo();
    int32 Do_Mission_Move_Carryall();

    // ========================================================================
    // 场景 INI 读写（asm: AircraftClass_SaveToMapINIList /
    // _CreateFromINIList）
    // ========================================================================
    static void SaveToMapINIList(INIClass* pINI);
    static void CreateFromINIList(INIClass* pINI);

    // ========================================================================
    // 飞行状态机阶段（asm: AircraftClass 的飞行 AI 状态字，落在 +0x7F4 偏移）。
    //   各类飞行行为（起飞/巡航/进近/着陆/停靠/掠过/神风/空投/坠毁）通过该
    //   入口把当前阶段写回机体，供 Update_AI 的飞行状态机读取。
    // ========================================================================
    static void SetFlightPhase(AircraftClass* pAircraft, int32 phase);

    // ========================================================================
    // Constructor
    // ========================================================================
    AircraftClass(HouseClass* pOwner) noexcept;

protected:
    explicit __forceinline AircraftClass(noinit_t) noexcept : FootClass(noinit) {}

    // ========================================================================
    // Properties
    // ========================================================================
public:
    AircraftTypeClass* Type;
    int32 Altitude;
    bool IsLanding;
    bool IsTakingOff;
    bool IsDockedNow;
    bool IsFlyingNow;
    bool IsDocking;
    bool IsCrashing;
    bool IsParadropping;
    bool IsSpyplane;
    bool IsKamikaze;
    bool IsOnCarryall;
    bool IsCarryall;
    bool IsCarryallFlying;
    bool IsAntiAir;
    bool IsFighter;
    bool IsStrafe;
    bool LockedFlag;
    // 根据游戏行为，可知机体名下挂着一个被运载的乘客对象，载荷查询
    // 以它为依据。
    FootClass* Passenger = nullptr;
    bool IsLoaded;
    BYTE align_7E0[3];
    BuildingClass* DockTarget;
    BuildingClass* LastDockTarget;
    int32 LandingDirection;
    int32 LandingAltitude;
    DWORD unknown_7F4;
    DWORD unknown_7F8;
    DWORD unknown_7FC;
    DWORD unknown_800;
    DWORD unknown_804;
    DWORD unknown_808;
    DWORD unknown_80C;
    DWORD unknown_810;
    // ★ 任务状态由基类 MissionClass 持有（原版 +0xA8/+0xB8）。

    // Per-instance serialization for the save-game stream (base + derived).
    void Save(class SaveGameClass& saver) const;
    void Load(class LoadGameClass& loader);
    DWORD unknown_81C;
    DWORD unknown_820;
    DWORD unknown_824;
    DWORD unknown_828;
    DWORD unknown_82C;
    DWORD unknown_830;
    DWORD unknown_834;
    DWORD unknown_838;
    DWORD unknown_83C;
    DWORD unknown_840;
    DWORD unknown_844;
    DWORD unknown_848;
    DWORD unknown_84C;
    DWORD unknown_850;
    DWORD unknown_854;
    DWORD unknown_858;
    DWORD unknown_85C;
    DWORD unknown_860;
    DWORD unknown_864;
    DWORD unknown_868;
    DWORD unknown_86C;
    DWORD unknown_870;
    DWORD unknown_874;
    DWORD unknown_878;
    DWORD unknown_87C;
    DWORD unknown_880;
    DWORD unknown_884;
    DWORD unknown_888;
    DWORD unknown_88C;
    DWORD unknown_890;
    DWORD unknown_894;
    DWORD unknown_898;
    DWORD unknown_89C;
    DWORD unknown_8A0;
    DWORD unknown_8A4;
    DWORD unknown_8A8;
    DWORD unknown_8AC;
    DWORD unknown_8B0;
    DWORD unknown_8B4;
    DWORD unknown_8B8;
    DWORD unknown_8BC;
    DWORD unknown_8C0;
    DWORD unknown_8C4;
    DWORD unknown_8C8;
    DWORD unknown_8CC;
    DWORD unknown_8D0;
    DWORD unknown_8D4;
    DWORD unknown_8D8;
    DWORD unknown_8DC;
    DWORD unknown_8E0;
    DWORD unknown_8E4;
    DWORD unknown_8E8;
    DWORD unknown_8EC;
    DWORD unknown_8F0;
    DWORD unknown_8F4;
    DWORD unknown_8F8;
    DWORD unknown_8FC;
    DWORD unknown_900;
    DWORD unknown_904;
    DWORD unknown_908;
    DWORD unknown_90C;
    DWORD unknown_910;
    DWORD unknown_914;
    DWORD unknown_918;
    DWORD unknown_91C;
    DWORD unknown_920;
    DWORD unknown_924;
    DWORD unknown_928;
    DWORD unknown_92C;
    DWORD unknown_930;
    DWORD unknown_934;
    DWORD unknown_938;
    DWORD unknown_93C;
    DWORD unknown_940;
    DWORD unknown_944;
    DWORD unknown_948;
    DWORD unknown_94C;
    DWORD unknown_950;
    DWORD unknown_954;
    DWORD unknown_958;
    DWORD unknown_95C;
    DWORD unknown_960;
    DWORD unknown_964;
    DWORD unknown_968;
    DWORD unknown_96C;
    DWORD unknown_970;
    DWORD unknown_974;
    DWORD unknown_978;
    DWORD unknown_97C;
    DWORD unknown_980;
    DWORD unknown_984;
    DWORD unknown_988;
    DWORD unknown_98C;
    DWORD unknown_990;
    DWORD unknown_994;
    DWORD unknown_998;
    DWORD unknown_99C;
    DWORD unknown_9A0;
    DWORD unknown_9A4;
    DWORD unknown_9A8;
    DWORD unknown_9AC;
    DWORD unknown_9B0;
    DWORD unknown_9B4;
    DWORD unknown_9B8;
    DWORD unknown_9BC;

    // 最近一次本体绘制的屏幕包围盒（SHP 帧 dst），供血条/选中框定位。
    RectangleStruct LastRenderRect;
};