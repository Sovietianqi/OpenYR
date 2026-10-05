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
class AnimClass;

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

    // 根据游戏行为，可知超武持有默认坐标常量，初始化例程负责清零。
    static CoordStruct Default_CellCoords;
    static CoordStruct Default_RoomCoords;

    static SuperClass* Find(const char* pID);
    static SuperClass* FindByIndex(int32 index);
    static int32 GetCount();

    // ── Global weather / global-effect state ──────────────────────────────
 // 根据游戏行为，可知 LightningStorm_Active 在闪电风暴生效期间为真
    // running anywhere on the map.  LightningStorm_IsActive simply reads it.
    static bool LightningStorm_Active;

 // 根据游戏行为，可知 PsyDom_Status 在心灵控制器生效期间非零
    // active.  PsyDom_IsActive returns `Status != 0`.
    static int32 PsyDom_Status;

 // byte __cdecl LightningStorm_IsActive()
    static bool LightningStorm_IsActive() { return LightningStorm_Active; }

 // void __cdecl LightningStorm_Strike(CellStruct cell).
    // Fires a single lightning bolt at the given cell; used by the
    // 'Lightning strike at waypoint' trigger action.
    static void LightningStorm_Strike(const CellStruct& cell);

 // byte __cdecl PsyDom_IsActive()
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
 // SuperClass_SetReadiness: store the arming flag at +0x6F.
    void SetReadiness(bool ready) { IsReady_ = ready; }

 // SuperClass_StopPreclickAnim: tear down the pre-click
    // animation that is playing for this weapon, if any.  `isPlayer` is
    // forwarded to the animation registry so a player-owned weapon also
    // clears its pending click.
    void StopPreclickAnim(bool isPlayer);

 // SuperClass::Discharged: consume the weapon's charge when
    // it is fired.  `ignoreRecharge` short-circuits the recharge bookkeeping
    // (used by the AI), `coords` is the cell the weapon was fired at.  The
    // launch itself is dispatched by Launch().
    void Discharged(const CellStruct& coords, bool ignoreRecharge);

    // ── Scripted charge manipulation ──────────────────────────────────────
    //  The trigger-action family (ActionClass_SetSWCharge / SetSWRecharge /
    //  ResetSWRecharge / ResetSW) drives these.  They are the scripted
    //  counterparts of the automatic recharge bookkeeping performed by
    //  UpdateRecharge.

 // SuperClass_SetSWCharge: force the weapon's charge
    // percentage (0..100), re-deriving the recharge timer from the type's
    // RechargeTime and the rules-side ChargeToDrainRatio.
    void SetCharge(int32 percent);
 // SuperClass_SetSWRecharge: overwrite the remaining
    // recharge frames and re-arm the timer.
    void SetRecharge(int32 frames);
 // SuperClass_ResetSWRecharge: restore the recharge timer to
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
    // 根据游戏行为，可知超武持有自己的放置动画与侧栏充能展示态，页签
    // 闪烁以"起始帧 + 帧数窗口"表达。
    int32 CameoChargeState = 0;      // 充能展示态：0 充能 / 1 就绪 / 2 生效中
    AnimClass* ChronoAnim = nullptr; // 超武自持的放置动画
    bool ChronoAnimPending = false;  // 是否已登记进动画跟踪表
    int32 FlashStartFrame = 0;       // 页签闪烁起始帧
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
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知超级武器层补全就绪应答、侧栏文案、点击屏蔽与
    // 时空动画入口，以及 COM 类型信息转发。
    // ------------------------------------------------------------------------
    virtual bool IsReadyToFire() const;
    virtual const char* NameReadiness() const;
    virtual bool ShouldFlash() const;
    virtual void IgnoreClick(bool ignore);
    virtual void CreateChronoAnim(const CoordStruct& coords);
    virtual HRESULT IRTTITypeInfo_GetClassID(CLSID* pClassID);
    static void Init_DefaultCellCoords();
    static void Init_DefaultRoomCoords();

};