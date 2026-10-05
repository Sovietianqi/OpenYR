#pragma once

#include <Abstract/FootClass.h>

class Surface;
class TargetClass;

// ============================================================================
// UnitClass - base for all vehicle units (tanks, harvesters, etc.)
// Inherits FootClass
// ============================================================================
class NOVTABLE UnitClass : public FootClass {
public:
    static const AbstractType AbsID = AbstractType::Unit;
    static DynamicVectorClass<UnitClass*>* Array;

    // ========================================================================
    // IPersistStream
    // ========================================================================
    virtual HRESULT __stdcall Load(IStream* pStm) override;
    virtual HRESULT __stdcall Save(IStream* pStm, BOOL fClearDirty) override;

    // ========================================================================
    // Destructor
    // ========================================================================
    virtual ~UnitClass();

    // ========================================================================
    // TechnoClass overrides
    // ========================================================================
    virtual bool IsVoxel() const override;
    virtual void Destroyed(ObjectClass* Killer) override;
    virtual bool CanScatter() const override;
    virtual int32 GetDefaultSpeed() const override;
    virtual bool HasTurret() const override;
    virtual bool CanDeploySlashUnload() const override;
    virtual bool IsUnitFactory() const override;

    // ========================================================================
    // UnitClass virtuals
    // ========================================================================
    virtual bool CanHarvest() const;
    virtual bool IsHarvesting() const;
    virtual int32 GetTiberiumLoad() const;
    virtual float GetTiberiumValue() const;
    virtual double Get_Tiberium() const override;
    virtual bool IsHarvestingTooMuch() const;

 // 根据游戏行为，可知 GetTiberiumPipFullness 负责下面这段逻辑。
    //
    //  Fraction of the harvester's ore bay that is full, used to size the
    //  collection pip.  Non-harvesters (neither Harvester nor Weeder on the
    //  type) report 0.0; harvesters divide their accumulated ore by the
    //  type's Storage capacity.
    double GetTiberiumPipFullness() const;

 // 根据游戏行为，可知 CanCrush 负责下面这段逻辑。
    //
    //  True when this unit may roll over `pTarget`.  The unit must be able to
    //  crush (the type's Crusher flag, or the CRUSHER veteran/elite ability),
    //  the target must exist and be crushable, and the two must be within
    //  crushing distance.
    bool CanCrush(FootClass* pTarget) const;
    virtual void StartHarvesting();
    virtual void StopHarvesting();
    virtual void HarvestTiberium();
    virtual void EnterTiberiumField();
    virtual void DumpTiberium();

    // ========================================================================
    // UnitClass specific virtuals - driving, harvesting, turret
    // ========================================================================
    virtual void DrawAsVXL(Point2D Coords, RectangleStruct BoundingRect, int32 Brightness, int32 Tint);
    virtual void DrawAsSHP(Point2D Coords, RectangleStruct BoundingRect, int32 Brightness, int32 Tint);
    virtual void DrawObject(Surface* pSurface, Point2D Coords, RectangleStruct CacheRect, int32 Brightness, int32 Tint);
    virtual bool IsDeactivated() const;
    virtual void UpdateTube();
    virtual void UpdateRotation();
    virtual void UpdateEdgeOfWorld();
    virtual void UpdateFiring();
    virtual void UpdateVisceroid();

    // ========================================================================
    // 根据游戏行为，可知 ExitObject 让本载具的第一名乘客下车并回到地图上，
    //  没有乘客时返回空。
    // ========================================================================
    FootClass* ExitObject();

    // ========================================================================
    // 根据游戏行为，可知 Die 是载具死亡的统一入口：先赶光乘客，再走通用
    //  死亡结算。
    // ========================================================================
    virtual void Die(TechnoClass* pKiller) override;

    // ========================================================================
    // 根据游戏行为，可知 AutoCrushSomething 在载具脚下找可碾压的步兵并顺手
    //  压死，返回真表示这一帧碾到了东西。
    // ========================================================================
    bool AutoCrushSomething();

    // ========================================================================
    // 根据游戏行为，可知 VisceroidWander 是病毒体的游荡步进：在矿脉附近随机
    //  换方向挪一格。
    // ========================================================================
    void VisceroidWander();

    // ========================================================================
    // 根据游戏行为，可知 DrawVXL 负责"画车体"、DrawAt 负责"画在哪"：两层
    //  分开，特殊状态的绘制也能复用同一套画体逻辑。
    // ========================================================================
    virtual void DrawVXL(const Point2D& coords, const RectangleStruct& rect);
    virtual void DrawAt(const Point2D& coords, const RectangleStruct& rect);
    virtual void UpdateDisguise();
    virtual void Explode();
    virtual bool GotoClearSpot();
    virtual bool TryToDeploy();
    virtual void Deploy();
    virtual void Undeploy();
    virtual bool Harvesting();
    virtual bool FlagAttach(int32 nHouseIdx);
    virtual bool FlagRemove();
    virtual void APCCloseDoor();
    virtual void APCOpenDoor();
    virtual bool ShouldCrashIt(TechnoClass* pTarget);
    virtual AbstractClass* AssignDestination(AbstractClass* pTarget);
    virtual bool AStarAttempt(const CellStruct& cell1, const CellStruct& cell2);
    bool Find_Path(const CellStruct& start, const CellStruct& dest,
                   DynamicVectorClass<CellStruct>& outPath);
    virtual Action MouseOverCell(CellStruct const* pCell, bool checkFog, bool ignoreForce) const;
    virtual Action MouseOverObject(ObjectClass const* pObject, bool ignoreForce) const;
    virtual void MarkAllOccupationBits(const CoordStruct& coords);
    virtual void UnmarkAllOccupationBits(const CoordStruct& coords);
    virtual void SetTarget(AbstractClass* pTarget);
    virtual AbstractClass* GetTarget() const;
    virtual void ClearTarget();
    virtual bool HasTarget() const;
    virtual void SetMission(Mission mission);
    virtual Mission GetMission() const;
    virtual void QueueMission(Mission mission);
    virtual void MissionAttack();
    virtual void MissionMove();
    virtual void MissionGuard();
    virtual void MissionSleep();
    virtual void MissionHunt();
    virtual void MissionReturn();
    virtual void MissionStop();
    virtual void MissionHarvest();
    virtual void MissionUnload();
    virtual void MissionEnter();
    virtual void MissionPatrol();
    virtual void MissionAreaGuard();
    virtual void UpdateMission();
    virtual void AI_Update();
    virtual void Combat_AI();
    virtual void Movement_AI();
    virtual void Fire_At(TargetClass* pTarget, int32 weaponIndex);
    virtual bool Can_Fire_At(TechnoClass* pTarget, int32 weaponIndex) const;
    virtual int32 GetWeaponRange(int32 weaponIndex) const;
    virtual int32 GetWeaponDamage(int32 weaponIndex) const;
    virtual void MuzzleFlash(int32 weaponIndex);
    virtual void OnFired(int32 weaponIndex);
    virtual int32 GetWeaponCount() const;
    virtual void TakeDamage(int32 damage, TechnoClass* pSource, WarheadTypeClass* pWarhead);
    virtual void OnDestroyed();
    virtual void OnCaptured(HouseClass* pNewOwner);
    virtual void OnVeterancyUp();
    virtual void Draw(Point2D& point, RectangleStruct& rect);
    virtual void DrawShadow(Point2D& point);
    virtual int32 GetZBias() const;
    virtual bool IsVisibleTo(HouseClass* pHouse) const;
    virtual void RevealTo(HouseClass* pHouse);
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
    virtual void Hold();
    virtual bool IsMoving() const;
    virtual bool IsFiring() const;
    virtual bool IsIdle() const;
    virtual void SetIdle();
    virtual void Freeze();
    virtual void Unfreeze();
    virtual bool Limbo();
    virtual bool Unlimbo();
    virtual bool InLimbo() const;
    virtual void Mark(MarkType mark);
    virtual void Unmark();
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
    virtual void TemporalWarp();
    virtual void UnTemporal();
    virtual bool IsTemporalWarped() const;
    virtual void MindControl(TechnoClass* pTarget);
    virtual void UnMindControl();
    virtual bool IsMindControlled() const;
    virtual void Disguise();
    virtual void UnDisguise();
    virtual bool IsDisguised() const;
    virtual bool IsUnit() const;
    virtual AbstractType WhatAmI() const;
    virtual int32 Size() const;

    // ========================================================================
    // Type-flag probes
    // ========================================================================
 // UnitClass_IsARealVehicle: true when the unit type is not
    // flagged IsConsideredVehicle (UnitTypeClass+0xE1B == 0).
    bool IsARealVehicle() const;
 // UnitClass_IsDeployer: the unit type's IsSimpleDeployer
    // byte (UnitTypeClass+0x6AC).
    bool IsDeployable() const;
 // UnitClass_GetCrewCount: thunk to TechnoClass::Crew_Type -
    // the infantry type this unit leaves behind when destroyed.
    InfantryTypeClass* GetCrewCount() const;
 // UnitClass_CalcPipPercentage: the percentage of the
    // unit's pip bar that is filled, used for the passenger/ammo pip display.
    int32 CalcPipPercentage() const;
 // UnitClass_ClearSomeVec: clears the object's attached
    // abstract vector (FootClass vec_Abs).
    void ClearSomeVec();

    // ========================================================================
    // Constructor
    // ========================================================================
    UnitClass(HouseClass* pOwner) noexcept;

protected:
    explicit __forceinline UnitClass(noinit_t) noexcept : FootClass(noinit) {}

    // ========================================================================
    // Properties
    // ========================================================================
public:
    UnitTypeClass* Type;
    DirStruct TurretDir;
    DirStruct BarrelDir;
    int32 TurretRotation;
    int32 BarrelRotation;
    bool IsHarvestingTiberium;
    bool IsDumpingTiberium;
    bool IsShowingCrate;
    bool IsMCV;
    bool IsHarvester;
    bool IsAPC;
    bool IsDeployer;
    bool IsChrono;
    bool IsBombing;
    bool IsUnderground;
    bool IsSubterranean;
    bool IsButtMissile;
    bool IsSpace;
    bool IsTank;
    bool IsWalker;
    bool IsAntiAir;
    bool IsOpenTopped;
    bool IsCarryall;
    bool IsCarryAllFly;
    bool IsJumpJet;
    BYTE align_7D0[3];
    int32 HarvestAmount;
    int32 HarvestRate;
    int32 TotalTiberiumValue;
    int32 FlagHouseIndex;
    bool Deployed;
    bool Deploying;
    bool Undeploying;
    bool Unloading;
    int32 DeathFrameCounter;
    int32 NonPassengerCount;
    bool HasFollowerCar;

    // 根据游戏行为，可知 Gunner 是当前坐在本单位炮手位上的那名步兵；
    //  GunnerWeaponIndex 记录炮手兵种对应的武器档位（-1 表示尚未确定）。
    //  炮手下车时两者一并清空。
    InfantryClass* Gunner;
    int32          GunnerWeaponIndex;

    // ── Carried flag identity (+0x6CC) ──────────────────────────────────────
    // House index of the rally/capture flag this unit currently carries, or -1
    // when it carries none.  UnitClass_DropFlag/PickUpFlag own this slot, and
    // both re-broadcast ownership through the object's vtable slot (+0x124).
    int32 CarriesFlagOfHouse;

    bool IsBusy() const;
    bool DropFlag();
    bool PickUpFlag(int32 houseIndex);

    // ========================================================================
    // 炮手 / 载员管理
    //
    //  多功能车、战斗要塞一类载具可以现场接收一名步兵充当"炮手"，炮手的
    //  兵种决定载具当前使用哪一档武器。下面这组接口负责炮手的上下车与
    //  切换，以及把首位乘客卸下。
    // ========================================================================

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ReceiveGunner 负责接收一名步兵上车充当炮手：先
    //  检查该步兵确实能担任炮手，再把它挂到本单位的炮手位上，并按兵种切换
    //  当前武器档位。
    // ------------------------------------------------------------------------
    bool ReceiveGunner(InfantryClass* pGunner);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 RemoveGunner 负责让当前炮手下车：清掉炮手位，让
    //  步兵重新回到地图上，并把武器档位复位到默认。
    // ------------------------------------------------------------------------
    bool RemoveGunner(InfantryClass* pGunner);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 SwitchGunner 负责按给定档位切换炮手武器：档位在
    //  合法范围内就切过去，越界则忽略。
    // ------------------------------------------------------------------------
    bool SwitchGunner(int32 idx);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 RemoveFirstPassenger 负责把首位乘客卸下：从乘客
    //  链上摘下第一个，让它回到地图上；若本载具的武器要靠乘客提供、而卸完
    //  之后已经没人了，就把武器档位一并复位。
    // ------------------------------------------------------------------------
    void RemoveFirstPassenger();

    // ========================================================================
    // 变形 / 展开
    // ========================================================================

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ToggleDeployState 负责切换本单位的"变形"状态：
    //  只有类型上允许变形、且当前确实停稳时才会进行；展开中的再点一次就
    //  收回、收起中的再点一次就展开。
    // ------------------------------------------------------------------------
    void ToggleDeployState();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ToggleSimpleDeploy 负责切换"简易展开"（例如采矿车
    //  就地卸矿这类不改变外形的展开）：按当前是展开还是收起状态，走对应的
    //  展开/收起流程。
    // ------------------------------------------------------------------------
    void ToggleSimpleDeploy();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ChangeOwnership 负责把本单位转交给新的拥有者：
    //  随车拖挂的从车一并转手；若本单位载着乘客，乘客也随之易主；最后更新
    //  自己的归属并让界面重绘。
    // ------------------------------------------------------------------------
    void ChangeOwnership(HouseClass* pNewOwner, bool a3);

    // Per-instance serialization for the save-game stream (base + derived).
    void Save(class SaveGameClass& saver) const;
    void Load(class LoadGameClass& loader);
    UnitClass* FollowerCar;
    DWORD unknown_7E0;
    DWORD unknown_7E4;
    DWORD unknown_7E8;
    DWORD unknown_7EC;
    DWORD unknown_7F0;
    DWORD unknown_7F4;
    DWORD unknown_7F8;
    DWORD unknown_7FC;
    DWORD unknown_800;
    DWORD unknown_804;
    DWORD unknown_808;
    DWORD unknown_80C;
    DWORD unknown_810;
    DWORD unknown_814;
    DWORD unknown_818;
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
};