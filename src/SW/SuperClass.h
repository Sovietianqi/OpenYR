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
    void LaunchSonarPulse();
    void LaunchHunterSeeker();
    void LaunchDropPod();

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