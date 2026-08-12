#pragma once

#include <Abstract/FootClass.h>
#include <COM/IUnknown.h>

// Forward declarations
class TargetClass;

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
    Mission CurrentMission;
    int32 MissionStatus;
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
};