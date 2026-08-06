#pragma once

#include <Abstract/TechnoClass.h>
#include <Core/Macros.h>
#include <Containers/DynamicVectorClass.h>

// Forward declarations for fields/types used by BuildingClass.
class FactoryClass;
class InfantryClass;
class LightSourceClass;
class TargetClass;
class CellClass;
class AnimClass;
class BuildingLightClass;
class WarheadTypeClass;

// ============================================================================
// BStateType - the high-level state a building's art/logic is in. Mirrors the
// original binary's BStateType enumeration.
// ============================================================================
enum class BStateType : int32 {
    Construction = 0,
    Idle         = 1,
    Active       = 2,
    Full         = 3,
    Aux1         = 4,
    Aux2         = 5,
    None         = -1
};

// ============================================================================
// BuildingAnimSlot - index into the per-building animation array. The
// original binary uses a fixed array of 0x15 (21) animation slots.
// ============================================================================
enum class BuildingAnimSlot : int32 {
    None          = -1,
    Default       = 0,
    Active,
    Special1,
    Special2,
    Special3,
    Idle1,
    Idle2,
    Idle3,
    Aux1,
    Aux2,
    Aux3,
    Turret,
    Garrison,
    Garrisoned,
    Damaged,
    DamagedActive,
    DamagedIdle,
    Construct,
    Sell,
    PoweredOff,
    Reserve
};

// Number of animation slots carried by every BuildingClass instance.
static constexpr int32 BUILDING_ANIM_SLOT_COUNT = 21;
// Number of damage-fire animation slots.
static constexpr int32 BUILDING_DAMAGE_FIRE_ANIM_COUNT = 8;
// Number of upgrade type slots a building may carry.
static constexpr int32 BUILDING_UPGRADE_COUNT = 3;

// ============================================================================
// BuildingAnimationClass - manages building animation frames
// ============================================================================
class BuildingAnimationClass {
public:
    BuildingAnimationClass() : AnimationValue(0) {}

    void Update();
    void SetAnimation(int32 state, int32 frame);
    int32 GetCurrentFrame() const;

    int32 AnimationValue;
    int32 CurrentFrame;
    bool IsAnimating;
    bool IsDamaged;
};

// ============================================================================
// BuildingClass - base for all buildings (inherits TechnoClass directly, NOT FootClass)
// ============================================================================
class NOVTABLE BuildingClass : public TechnoClass {
public:
    static const AbstractType AbsID = AbstractType::Building;
    static DynamicVectorClass<BuildingClass*>* Array;

    // ========================================================================
    // IPersistStream
    // ========================================================================
    virtual HRESULT __stdcall Load(IStream* pStm) override;
    virtual HRESULT __stdcall Save(IStream* pStm, BOOL fClearDirty) override;

    // ========================================================================
    // Destructor
    // ========================================================================
    virtual ~BuildingClass();

    // ========================================================================
    // TechnoClass overrides
    // ========================================================================
    virtual bool IsPowerOnline() const override;
    virtual bool IsUnitFactory() const override;
    virtual bool IsArmed() const override;
    virtual bool CanOccupyFire() const override;
    virtual double GetStoragePercentage() const override;
    virtual int32 GetRefund() const override;
    virtual BulletClass* Fire(AbstractClass* pTarget, int32 nWeaponIndex) override;
    virtual void Uncloak(bool bPlaySound) override;
    virtual void Cloak(bool bPlaySound) override;
    virtual bool IsClearlyVisibleTo(HouseClass* House) const override;
    virtual bool CanScatter() const override;

    // ========================================================================
    // BuildingClass virtuals
    // ========================================================================
    virtual void Sell(DWORD dwUnk);
    virtual bool CanBeSold() const;
    virtual bool CanBeRepaired() const;
    virtual void RepairWithMoney(int32 money);
    virtual bool SWAvailable() const;
    virtual bool SW2Available() const;
    virtual bool IsControllable() const override;
    virtual bool IsSelectable() const override;
    virtual bool CanBeSelected() const override;
    virtual bool CanBeSelectedNow() const override;
    virtual bool IsManaDrainPossible() const;
    virtual AbstractClass* FindFactoryTarget(AbstractClass* pTarget) const;
    virtual bool HasNavigationDeal() const;
    virtual bool IsFactory() const;
    virtual bool IsFactoryExplicit() const;
    virtual bool IsToggledRallyPoint() const;

    // ========================================================================
    // BuildingClass specific virtuals - production, power, garrison
    // ========================================================================
    virtual bool CanFireNow() const;
    virtual bool CanEnterCell(CellClass* pCell) const;
    virtual void Place(bool captured);
    virtual void UpdateConstructionOptions();
    virtual void Draw(const Point2D& point, const RectangleStruct& rect);
    virtual DirStruct FireAngleTo(ObjectClass* pObject) const;
    virtual void Destroy(DWORD dwUnused, TechnoClass* pTechno, bool NoSurvivor, CellStruct& cell);
    virtual bool TogglePrimaryFactory();
    virtual void SensorArrayActivate(CellStruct cell);
    virtual void SensorArrayDeactivate(CellStruct cell);
    virtual void DisguiseDetectorActivate(CellStruct cell);
    virtual void DisguiseDetectorDeactivate(CellStruct cell);
    virtual int32 AlwaysZero();
    virtual bool ForceCreate(CoordStruct& coord, DWORD dwUnk);
    virtual CellStruct FindExitCell(DWORD dwUnk, DWORD dwUnk2) const;
    virtual int32 DistanceToDockingCoord(ObjectClass* pObj) const;
    virtual void BeginMode(BStateType bType);
    virtual void GoOnline();
    virtual void GoOffline();
    virtual int32 GetPowerOutput() const;
    virtual int32 GetPowerDrain() const;
    virtual void EnableStuff();
    virtual void DisableStuff();
    virtual void EnableTemporal();
    virtual void DisableTemporal();
    virtual void UpdateAnimations();
    virtual int32 GetCurrentFrame() const;
    virtual bool IsAllFogged() const;
    virtual void SetRallypoint(CellStruct* pTarget, bool bPlayEVA);
    virtual int32 FirstActiveSWIdx() const;
    virtual int32 SecondActiveSWIdx() const;
    virtual int32 GetShapeNumber() const;
    virtual void FireLaser(CoordStruct Coords);
    virtual bool IsBeingDrained() const;
    virtual bool UpdateBunker();
    virtual void KillOccupants(TechnoClass* pAssaulter);
    virtual bool MakeTraversable();
    virtual bool CheckFog() const;
    virtual bool IsTraversable() const;
    virtual void UnloadBunker();
    virtual void ClearBunker();
    virtual void EmptyBunker();
    virtual void AfterDestruction();
    virtual void DestroyNthAnim(BuildingAnimSlot Slot);
    virtual void PlayAnim(const char* animName, BuildingAnimSlot Slot, bool Damaged, bool Garrisoned, int32 effectDelay);
    virtual void ToggleDamagedAnims(bool isDamaged);
    virtual void CreateEndPost(bool arg);
    virtual DWORD GetFWFlags() const;
    virtual int32 GetOccupantCount() const;
    virtual bool AddOccupant(InfantryClass* pInfantry);
    virtual bool RemoveOccupant(InfantryClass* pInfantry);
    virtual InfantryClass* GetOccupant(int32 index) const;
    virtual void FireFromOccupant(TechnoClass* pTarget);
    virtual int32 GetOccupantWeaponIndex() const;
    virtual bool HasSuperWeapon(int32 index) const;
    virtual TechnoTypeClass* GetSecretProduction() const;
    virtual void SetTarget(AbstractClass* pTarget);
    virtual AbstractClass* GetTarget() const;
    virtual void ClearTarget();
    virtual bool HasTarget() const;
    virtual void SetMission(Mission mission);
    virtual Mission GetMission() const;
    virtual void QueueMission(Mission mission);
    virtual void MissionAttack();
    virtual void MissionGuard();
    virtual void MissionSleep();
    virtual void MissionConstruction();
    virtual void MissionSelling();
    virtual void MissionRepair();
    virtual void MissionActive();
    virtual void MissionIdle();
    virtual void UpdateMission();
    virtual void AI_Update();
    virtual void Combat_AI();
    virtual void Production_AI();
    virtual void Power_AI();
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
    virtual int32 GetValue() const;
    virtual int32 GetCost() const;
    virtual int32 GetBuildTime() const;
    virtual DirStruct GetDirection() const;
    virtual void SetDirection(DirStruct dir);
    virtual CoordStruct GetCoords() const;
    virtual void SetCoords(CoordStruct coords);
    virtual void Stop();
    virtual void Hold();
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
    virtual bool IsBuilding() const;
    virtual AbstractType WhatAmI() const;
    virtual int32 Size() const;
    virtual void Destroyed(ObjectClass* Killer);

    // ========================================================================
    // Constructor
    // ========================================================================
    BuildingClass(HouseClass* pOwner) noexcept;

protected:
    explicit __forceinline BuildingClass(noinit_t) noexcept : TechnoClass(noinit) {}

    // ========================================================================
    // Properties
    // ========================================================================
public:
    BuildingAnimationClass Animation;
    BuildingTypeClass* Type;
    bool IsConstructed;
    bool IsBeingDrained_;   // backing store for IsBeingDrained() (renamed to avoid name clash)
    bool IsOnline;
    bool IsPowerPlant;
    bool IsOverpowered;
    BYTE align_52D[3];
    int32 PowerOutput;
    int32 PowerDrain;

    // ========================================================================
    // BuildingClass specific state - production, power, garrison, anims.
    // These named members back the BuildingClass virtual methods. They mirror
    // the layout/semantics of the original binary's BuildingClass fields.
    // ========================================================================
    FactoryClass*       Factory;                       // production queue owner
    BStateType          BState;                        // current building art/logic state
    BStateType          QueueBState;                   // state queued for next transition
    InfantryClass*      C4AppliedBy;                   // infantry that planted C4 on us
    bool                C4Applied;                     // C4 has been planted
    AnimClass*          Anims[BUILDING_ANIM_SLOT_COUNT];     // per-slot active anims
    bool                AnimStates[BUILDING_ANIM_SLOT_COUNT];// whether each anim was enabled
    AnimClass*          DamageFireAnims[BUILDING_DAMAGE_FIRE_ANIM_COUNT];
    BuildingTypeClass*  Upgrades[BUILDING_UPGRADE_COUNT];    // installed upgrade types
    int32               FiringSWType;                  // super-weapon currently launching
    BuildingLightClass* Spotlight;                     // attached building light
    int32               GateTimer;                     // frames remaining for gate anim
    LightSourceClass*   LightSource;                   // tiled light source
    bool                HasPower;                      // power is currently available
    bool                RegisteredAsPoweredUnitSource;  // registered w/ powered-unit system
    DWORD               SupportingPrisms;              // prism chain contribution count
    bool                HasExtraPowerBonus;
    bool                HasExtraPowerDrain;
    DynamicVectorClass<InfantryClass*> Overpowerers;   // tesla troopers boosting us
    DynamicVectorClass<InfantryClass*> Occupants;      // garrisoned infantry
    int32               FiringOccupantIndex;           // occupant whose weapon fires next
    bool                WasOnline;                     // online state at last Update()
    bool                StuffEnabled;                  // set by EnableStuff/DisableStuff
    bool                BeingProduced;                 // AI_REBUILDABLE flag
    bool                ShouldRebuild;                 // AI_REPAIRABLE flag
    bool                HasBeenCaptured;               // ownership changed at least once
    bool                IsFogged;                      // currently hidden by fog
    bool                IsSensorActive;                // sensor array online
    bool                IsDetectorActive;              // disguise detector online
    bool                IsFrozen_;                     // frozen by temporal/chronoshift
    bool                IsLocked_;                     // update lock held
    bool                IsDisabled_;                   // explicitly disabled
    bool                IsDisguised_;                  // disguised (spy)
    bool                IsMindControlled_;             // under mind control
    bool                IsPrimaryFactory;              // primary factory for its type
    int32               BunkerState;                   // garrison state machine value
    int32               PrismStage;                    // prism charge state
    CoordStruct         PrismTargetCoords;             // prism fire destination
    int32               DelayBeforeFiring;            // frames before next shot
    TechnoTypeClass*    SecretProduction;              // secret lab bonus type
    DWORD               StorageFilledSlots;            // silo occupancy
    CellStruct          RallyPoint;                    // factory rally point
    AbstractClass*      Target;                        // current target object
    Mission             CurrentMission;               // active mission
    Mission             QueuedMission;                 // mission to switch to

    // Reserved layout buffers for binary compatibility with original engine.
    // These bytes correspond to internal state fields in the original binary
    // (production state, animation timers, targeting, garrison state, etc.)
    // that are managed through the virtual method implementations above.
    BYTE ReservedLayout_538[0x544 - 0x538];
    bool IsTentativelyOccupied;
    bool IsCurrentlyOccupied;
    bool IsStateChanging;
    bool IsBeingSabotaged;
    BYTE ReservedLayout[0x70C - 0x548];
};