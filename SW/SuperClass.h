#pragma once

#include "../Abstract/AbstractClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Math/CoordStruct.h"

// ============================================================================
// Forward declarations
// ============================================================================

class SuperWeaponTypeClass;
class HouseClass;
class AbstractClass;

enum class SuperWeaponType : int32;
enum class MissionType : int32;

// ============================================================================
// SWState
// ============================================================================

enum class SWState : int32 {
    None    = 0,
    Idle    = 1,
    Ready   = 2,
    Firing  = 3,
    Active  = 4,
    Done    = 5,
    Count   = 6
};

// ============================================================================
// SuperClass
// ============================================================================

class SuperClass : public AbstractClass {
public:
    static DynamicVectorClass<SuperClass*>* Array;

    static SuperClass* Find(const char* pID);
    static SuperClass* FindByIndex(int32 index);
    static int32 GetCount();

    // ── Global weather / global-effect state ──────────────────────────────
    // LightningStorm_Active (asm 0x...): set while a lightning storm is
    // running anywhere on the map.  LightningStorm_IsActive simply reads it.
    static bool LightningStorm_Active;

    // PsyDom_Status (asm 0x...): non-zero while a psychic dominator is
    // active.  PsyDom_IsActive returns `Status != 0`.
    static int32 PsyDom_Status;

    // byte __cdecl LightningStorm_IsActive()  (asm 0x68F0C0)
    static bool LightningStorm_IsActive() { return LightningStorm_Active; }

    // void __cdecl LightningStorm_Strike(CellStruct cell)  (asm 0x6E0060).
    // Fires a single lightning bolt at the given cell; used by the
    // 'Lightning strike at waypoint' trigger action.
    static void LightningStorm_Strike(const CellStruct& cell);

    // byte __cdecl PsyDom_IsActive()          (asm 0x68EFD0)
    static bool PsyDom_IsActive() { return PsyDom_Status != 0; }

    SuperClass(SuperWeaponTypeClass* pType, HouseClass* pOwner) noexcept;
    virtual ~SuperClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    virtual void Update() override;
    virtual void PointerExpired(AbstractClass* pAbstract, bool removed) override;

    // Recharge management
    void UpdateRecharge();
    void OnReady();
    bool IsReady() const;
    bool IsCharged() const;
    bool IsPresent() const;
    bool IsFiring() const;
    bool IsAvailable() const;
    void CheckAvailability();
    bool CheckAuxBuildings() const;

    // Launch
    void Launch(CellStruct target);
    void UpdateFiring();
    void UpdateActive();
    void OnDone();

    // Type-specific launch methods
    void LaunchNuke();
    void LaunchIronCurtain();
    void LaunchForceShield();
    void LaunchLightningStorm();
    void LaunchPsychicDominator();
    void LaunchGeneticMutator();
    void LaunchChronoSphere();
    void LaunchChronoWarp();
    void LaunchParaDrop();
    void LaunchSpyPlane();
    void LaunchPsychicReveal();

    // Type-specific update methods
    void UpdateNukeFiring();
    void UpdateIronCurtainActive();
    void UpdateForceShieldActive();
    void UpdateLightningStormFiring();
    void UpdateLightningStormActive();
    void UpdateDominatorFiring();
    void UpdateGeneticMutatorFiring();
    void UpdateChronoWarpFiring();
    void UpdateChronoWarpActive();
    void UpdateParaDropFiring();
    void UpdateSpyPlaneFiring();

    // Type-specific effect methods
    void DetonateNuke();
    void DoLightningStrike();
    void ActivateDominator();
    void ActivateGeneticMutator();
    void SpawnParaDropPlane();
    void SpawnSpyPlane();

    // Targeting
    void SetTarget(CellStruct target);
    CellStruct GetTarget() const;
    CoordStruct GetTargetCoord() const;
    bool CanTargetCell(CellStruct cell) const;
    bool IsValidTarget() const;

    // Cursor management
    int32 GetCursor() const;
    int32 GetNoCursor() const;
    bool IsClickLaunch() const;
    bool IsDesignator() const;
    bool IsSelfTargeted() const;
    bool IsAutoFire() const;
    bool IsTargetable() const;

    // State management
    void Suspend();
    void Resume();
    void Reset();
    void ForceFire();
    void Grant();
    void Revoke();

    // ── Click-through pairing ─────────────────────────────────────────────
    // SuperClass_SetReadiness (asm 0x6CB893): store the arming flag at +0x6F.
    void SetReadiness(bool ready) { IsReady_ = ready; }

    // SuperClass_StopPreclickAnim (asm 0x6CB8A5): tear down the pre-click
    // animation that is playing for this weapon, if any.  `isPlayer` is
    // forwarded to the animation registry so a player-owned weapon also
    // clears its pending click.
    void StopPreclickAnim(bool isPlayer);

    // SuperClass::Discharged (asm 0x6CB920): consume the weapon's charge when
    // it is fired.  `ignoreRecharge` short-circuits the recharge bookkeeping
    // (used by the AI), `coords` is the cell the weapon was fired at.  The
    // launch itself is dispatched by Launch().
    void Discharged(const CellStruct& coords, bool ignoreRecharge);

    // ── Scripted charge manipulation ──────────────────────────────────────
    //  The trigger-action family (ActionClass_SetSWCharge / SetSWRecharge /
    //  ResetSWRecharge / ResetSW) drives these.  They are the scripted
    //  counterparts of the automatic recharge bookkeeping performed by
    //  UpdateRecharge.

    // SuperClass_SetSWCharge (asm 0x6CBE50): force the weapon's charge
    // percentage (0..100), re-deriving the recharge timer from the type's
    // RechargeTime and the rules-side ChargeToDrainRatio.
    void SetCharge(int32 percent);
    // SuperClass_SetSWRecharge (asm 0x6CBF10): overwrite the remaining
    // recharge frames and re-arm the timer.
    void SetRecharge(int32 frames);
    // SuperClass_ResetSWRecharge (asm 0x6CBF50): restore the recharge timer to
    // the type's nominal RechargeTime without changing the charge state.
    void ResetRecharge();

    // Static utility methods
    static void UpdateAll();
    static void RemoveAll();
    static SuperClass* FindByOwner(HouseClass* pOwner, SuperWeaponType swType);
    static int32 GetReadyCount(HouseClass* pOwner);

protected:
    explicit SuperClass(noinit_t) noexcept : AbstractClass(noinit) {}

public:
    SuperWeaponTypeClass* Type;
    HouseClass* Owner;
    int32 RechargeTimer;
    SWState State;
    int32 ChargeDrain;
    SuperClass* next;

    // Scriptable recharge override (asm +0x24).  -1 means "use the type's own
    // RechargeTime"; SetSWRecharge stores an explicit frame count here and
    // ResetSWRecharge restores the -1 sentinel.
    int32 CustomChargeTime;
    bool IsGranted;
    bool IsAnimationPlaying;
    bool IsAlreadyActivated;
    bool IsSuspended;
    bool IsDumb;
    bool IsOneTime;
    bool IsTemporallyUnavailable;
    bool IsPowered;
    bool IsReady_;
    bool IsCharged_;
    bool IsManual;
    bool PreClick;
    bool PostClick;
    bool IsDesignator_;
    bool GrantedByAnother;
    BYTE Pad1;
    BYTE Pad2;
    BYTE Pad3;
    int32 unknown_44;
    int32 unknown_48;
    int32 unknown_4C;
    int32 CurrMoney;
    int32 unknown_54;
    int32 unknown_58;
    int32 unknown_5C;
    int32 unknown_60;
    SWState deferredState;
    int32 deferredTimer;
    CellStruct deferredCell;

    // Type-specific state
    int32 LightningTimer;
    int32 LightningDeferment;
    int32 LightningStrikeCount;
    CellStruct LightningScatter;
    int32 ChronoWarpTimer;
    int32 ChronoWarpState;
    int32 ChronoWarpDamageDone;
    int32 DominatorTimer;
    int32 DominatorScroll;
    bool DominatorActivated;
    int32 GeneticMutatorTimer;
    int32 SpyPlaneTimer;
    int32 ParaDropTimer;
    int32 ParaDropCount;
    int32 NukeTimer;
    int32 NukeState;

    // Targeting
    CellStruct TargetCell;
    CoordStruct TargetCoord;
    CellStruct LastTargetCell;
    CoordStruct LastTargetCoord;

    // Camera
    CellStruct CameraStart;
    CellStruct CameraEnd;

    // Unknown/misc
    CellStruct unknown_130;
    int32 unknown_138;
    int32 unknown_13C;
};