#pragma once

#include <Abstract/FootClass.h>
#include <Math/Timer.h>

// ============================================================================
// InfantryClass - base for all infantry units
// Inherits FootClass
// ============================================================================
class NOVTABLE InfantryClass : public FootClass {
public:
    static const AbstractType AbsID = AbstractType::Infantry;
    static DynamicVectorClass<InfantryClass*>* Array;

    // ========================================================================
    // IPersistStream
    // ========================================================================
    virtual HRESULT __stdcall Load(IStream* pStm) override;
    virtual HRESULT __stdcall Save(IStream* pStm, BOOL fClearDirty) override;

    // ========================================================================
    // Destructor
    // ========================================================================
    virtual ~InfantryClass();

    // ========================================================================
    // TechnoClass overrides
    // ========================================================================
    virtual bool IsVoxel() const override;
    virtual void Destroyed(ObjectClass* Killer) override;
    virtual bool CanScatter() const override;
    virtual int32 GetDefaultSpeed() const override;
    virtual bool IsEngineer() const override;
    virtual bool CanDeploySlashUnload() const override;
    virtual bool IsCloseEnough(AbstractClass* pTarget, int32 idxWeapon) const override;
    virtual bool IsCloseEnoughToAttack(AbstractClass* pTarget) const override;
    virtual void TakeDamage(int32 damage, ObjectClass* source, WarheadTypeClass* warhead);

    // ========================================================================
    // InfantryClass virtuals
    // ========================================================================
    virtual bool CanBeEngineer() const;
    virtual bool IsCrawling() const;
    virtual bool IsProne() const;
    virtual bool IsDeployed() const;
    virtual void StartCrawling();
    virtual void StopCrawling();
    virtual void GoProne();
    virtual void StandUp();
    virtual void Panic() override;
    virtual void Scatter(const CoordStruct& crd, bool ignoreMission, bool ignoreDestination) override;
    virtual void UpdateIdleAction() override;
    virtual void PerCellProcess();
    virtual void FireDeathWeapon(int32 additionalDamage);
    virtual void GetFiringCoords();
    virtual void GetFiringCoordsFromBomb();
    virtual int32 GetFiringSync();

    // ========================================================================
    // InfantryClass specific virtuals
    // ========================================================================
    virtual bool CanFireNow() const;
    virtual bool CanEnterCell(CellClass* pCell) const;
    virtual void EnteredCell();
    virtual void ExitCell();
    virtual void Prone();
    virtual void Unprone();
    virtual void Crawl();
    virtual void StopCrawl();
    virtual void Deploy();
    virtual void Undeploy();
    virtual bool CanDeploy() const;
    virtual void UnPanic();
    virtual bool IsPanicked() const;
    virtual void Berzerk();
    virtual void UnBerzerk();
    virtual bool IsBerzerk() const;
    virtual void Stun();
    virtual void UnStun();
    virtual bool IsStunned() const;
    virtual void Sleep();
    virtual void Wake();
    virtual bool IsSleeping() const;
    virtual void SetTarget(AbstractClass* pTarget);
    virtual AbstractClass* GetTarget() const;
    virtual void ClearTarget();
    virtual bool HasTarget() const;
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
    virtual void MissionHarvest();
    virtual void MissionCapture();
    virtual void MissionEnter();
    virtual void MissionUnload();
    virtual void MissionPatrol();
    virtual void MissionAreaGuard();
    virtual void MissionParaDrop();
    virtual void UpdateMission();
    virtual void AI_Update();
    virtual void Combat_AI();
    virtual void Movement_AI();
    virtual void Fire_At(AbstractClass* pTarget, int32 weaponIndex);
    virtual bool Can_Fire_At(TechnoClass* pTarget, int32 weaponIndex) const;
    virtual int32 GetWeaponRange(int32 weaponIndex) const;
    virtual int32 GetWeaponDamage(int32 weaponIndex) const;
    virtual CoordStruct GetFireCoords(int32 weaponIndex) const;
    virtual void MuzzleFlash(int32 weaponIndex);
    virtual void OnFired(int32 weaponIndex);
    virtual int32 GetWeaponCount() const;
    virtual void TakeDamage(int32 damage, TechnoClass* pSource, WarheadTypeClass* pWarhead);
    virtual void OnDestroyed();
    virtual void OnCaptured(HouseClass* pNewOwner);
    virtual void OnVeterancyUp();
    virtual void Draw(Point2D& point, RectangleStruct& rect);
    virtual void DrawSHP(Point2D& point, RectangleStruct& rect, int32 brightness, int32 tint);
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
    virtual bool CanDeployNow() const;
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
    virtual bool CanCaptureBuilding() const;
    virtual bool CanInfiltrate() const;
    virtual void Infiltrate(BuildingClass* pBuilding);
    virtual void CaptureBuilding(BuildingClass* pBuilding);
    virtual void Detach_Target();
    virtual void Attach_Target(AbstractClass* pTarget);
    virtual void PlayAnim(Sequence index, bool force, bool randomStartFrame);
    virtual bool IsTechno() const;
    virtual bool IsInfantry() const;
    virtual bool IsUnit() const;
    virtual bool IsAircraft() const;
    virtual bool IsBuilding() const;
    virtual AbstractType WhatAmI() const;
    virtual int32 Size() const;
    virtual HRESULT GetClassID(CLSID* pClassID);

    // ========================================================================
    // Constructor
    // ========================================================================
    InfantryClass(HouseClass* pOwner) noexcept;

protected:
    explicit __forceinline InfantryClass(noinit_t) noexcept : FootClass(noinit) {}

    // ========================================================================
    // Properties
    // ========================================================================
public:
    InfantryTypeClass* Type;
    Sequence CurrentSequence;
    Sequence UnkSequence;
    bool IsCrawlingNow;
    bool IsProneNow;
    bool IsDeployedNow;
    bool IsParadropping;
    bool IsBoarding;
    bool IsUnboarding;
    bool IsFiringNow;
    bool IsAiming;
    bool IsStunnedNow;
    BYTE align_7D9[3];
    int32 FearLevel;
    int32 PanicTimerVal;
    CDTimerClass PanicTimer;
    bool IsPanicking;
    bool IsABombNow;
    bool IsC4Now;
    bool IsTerrorDrone;
    BYTE align_7F0[4];
    bool IsCivilian;
    bool IsBrute;
    bool IsOccupying;
    bool IsUsingDeployFireWeapon;
    bool IsUsingSecondaryWeapon;
    bool IsFiringWhileMoving;
    bool IsSwimming;
    bool IsDog;
    bool IsEngineerNow;
    bool IsThief;
    bool IsCow;
    bool IsChrono;
    bool IsTanya;
    bool IsBoris;
    bool IsSEAL;
    bool IsSpy;
    bool IsIvan;
    bool IsDesolator;
    bool IsCrazyIvan;
    bool IsCosmonaut;
    bool IsYuri;
    bool IsInitiate;
    bool IsVirus;
    bool IsSuperSoldier;
    bool IsMutant;
    bool IsJumpJet;
    bool IsDeployedFire;
    bool IsFiringFromVehicle;
    bool IsInWater;
    bool IsDemolition;
    bool IsInVehicle;
    bool IsInOpenTopped;
    BYTE align_818[4];
    // Reserved layout buffer for binary compatibility with original engine.
    // These bytes correspond to internal state fields in the original binary
    // (mission state, targeting, pathfinding, animation timers, etc.) that
    // are managed through the virtual method implementations above.
    BYTE ReservedLayout[0xA10 - 0x81C];

    // ========================================================================
    // Reconstruction working state
    //
    // Named fields that back the virtual-method implementations below.  These
    // complement the binary-compatible ReservedLayout buffer above and model
    // the high-level gameplay state (mission queue, targeting, destination
    // and the special-effect "Now" flags) that the original engine stored
    // inside the reserved region.  They are declared after the reserved
    // buffer so the binary-compatible footprint (offsets up to 0xA10) is
    // preserved; the cloak / iron-curtain / force-shield / temporal states
    // intentionally reuse the timers inherited from TechnoClass rather than
    // duplicating them here.
    // ========================================================================
    Mission        CurrentMission;     // active mission (backed by Get/SetMission)
    Mission        QueuedMission;      // mission to run once the current ends
    AbstractClass* TargetObj;          // current target (backed by Get/SetTarget)
    CoordStruct    DestinationCoord;   // current movement destination
    bool           IsSleepingNow;      // true while the Sleep mission is active
    bool           IsLockedNow;        // true while command input is locked out
    bool           IsFrozenNow;        // true while frozen ( Freeze / Unfreeze )
    bool           IsAliveNow;         // liveness flag (cleared by Kill/OnDestroyed)
    bool           IsMindControlledNow;// true while under external mind control
    bool           IsDisguisedNow;     // true while a spy is disguised
    TechnoClass*   MindControlVictim;  // techno this infantry is controlling
};
