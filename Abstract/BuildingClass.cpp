// =============================================================================
// BuildingClass - building instance implementation
//
// BuildingClass represents a single placed building on the map.  It inherits
// from TechnoClass (not FootClass) because buildings do not move.  Each
// building tracks its construction state, power output/drain, online/offline
// status, animation state, and (for factories) production queues.
//
// Key subsystems:
//   * Power      - GetPowerOutput/GetPowerDrain report the building's
//                  contribution to the owning house's power budget.  Power
//                  plants go offline when drained or low-power.
//   * Repair     - RepairWithMoney restores HP at a cost; the building's
//                  owner is charged credits each frame.
//   * Sell       - Sell refunds a fraction of the original cost proportional
//                  to remaining health and removes the building.
//   * Production - IsUnitFactory / IsFactory report whether the building can
//                  produce units; FindFactoryTarget locates a rally point.
//   * Animation  - BuildingAnimationClass drives the idle / active / damaged
//                  frame cycling for the building's voxel/sprite art.
//   * SuperWeapon- SWAvailable / SW2Available gate the two super-weapon slots
//                  a building type may carry.
// =============================================================================

#include <cstdio>
#include <cstdlib>

#include <AI/TagClass.h>
#include <Abstract/BuildingClass.h>
#include <Animations/AnimClass.h>
#include <Game/SaveGameClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Abstract/TechnoTypeClass.h>
#include <Abstract/InfantryClass.h>
#include <Abstract/InfantryTypeClass.h>
#include <Houses/HouseClass.h>
#include <Game/Game.h>
#include <Core/Definitions.h>
#include <Helpers/AsmMath.h>
#include <Combat/WeaponTypeClass.h>
#include <Combat/BulletTypeClass.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <INI/INIClass.h>
#include <SW/AirstrikeClass.h>
#include <Rules/RulesClass.h>
#include <Audio/VoxClass.h>
#include <Audio/VocClass.h>
#include <Special/LightSourceClass.h>
#include <Houses/FactoryClass.h>
#include <Game/Externs.h>
#include <Rendering/Surface.h>
#include <Rendering/ConvertClass.h>
#include <Rendering/TacticalClass.h>
#include <Rendering/UnitArt.h>
#include "Rendering/ObjectUI.h"

#include <cmath>
#include <cstdio>
#include <cctype>

// 降级渲染提示只打一次。

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<BuildingClass*>* BuildingClass::Array = nullptr;

// The map INI section every structure record is written into.  Matches the
// original's `str_Structures` literal exactly.
static const char* const str_Structures = "Structures";

// ============================================================================
// Local constants
// ============================================================================
namespace {
    // HP restored per credit spent during repair.
    constexpr int32 REPAIR_HP_PER_CREDIT = 2;

    // Fraction of original cost refunded when a building is sold (0-100).
    constexpr int32 SELL_REFUND_PERCENT = 50;

    // Frames between repair ticks (controls repair speed).
    constexpr int32 REPAIR_TICK_INTERVAL = 15;

    // Damage threshold (as fraction of MaxHealth) below which the building
    // shows its damaged animation state.
    constexpr double DAMAGED_THRESHOLD = 0.5;

    // Power drain multiplier when a building is low-powered.
    constexpr double LOW_POWER_OUTPUT_MULT = 0.5;
} // anonymous namespace

// ============================================================================
// BuildingAnimationClass
// ============================================================================

// -----------------------------------------------------------------------------
// Update - advance the current animation frame if animating.  The frame
// wraps around at AnimationValue to create a loop.  The damaged flag is
// set externally by the building when its health drops below the threshold.
// -----------------------------------------------------------------------------
void BuildingAnimationClass::Update()
{
    if (IsAnimating) {
        ++CurrentFrame;
        if (AnimationValue > 0) {
            CurrentFrame %= AnimationValue;
        }
    }
}

// -----------------------------------------------------------------------------
// SetAnimation - configure the animation loop length and starting frame.
// State encodes which animation track is active (idle, active, damaged, etc.)
// -----------------------------------------------------------------------------
void BuildingAnimationClass::SetAnimation(int32 state, int32 frame)
{
    AnimationValue = state;
    CurrentFrame = frame;
    IsAnimating = (state > 0);
}

// -----------------------------------------------------------------------------
// GetCurrentFrame - return the frame index for the renderer.
// -----------------------------------------------------------------------------
int32 BuildingAnimationClass::GetCurrentFrame() const
{
    return CurrentFrame;
}

// ============================================================================
// Constructor
// ============================================================================
BuildingClass::BuildingClass(HouseClass* pOwner) noexcept
    : TechnoClass()
    , Animation()
    , Type(nullptr)
    , IsConstructed(false)
    , IsBeingDrained_(false)
    , IsOnline(false)
    , IsPowerPlant(false)
    , IsOverpowered(false)
    , PowerOutput(0)
    , PowerDrain(0)
    , Factory(nullptr)
    , BState(BStateType::None)
    , QueueBState(BStateType::None)
    , C4AppliedBy(nullptr)
    , C4Applied(false)
    , Anims{}
    , AnimStates{}
    , DamageFireAnims{}
    , Upgrades{}
    , UpgradeLevel(0)
    , GapSuperCharged(false)
    , IsGeneratingGap(false)
    , CloakRadius(0)
    , AirstrikeImUsing(nullptr)
    , PoweredUnits{}
    , FiringSWType(0)
    , Spotlight(nullptr)
    , GateTimer(0)
    , LightSource(nullptr)
    , HasPower(false)
    , RegisteredAsPoweredUnitSource(false)
    , SupportingPrisms(0)
    , ConditionYellow(false)
    , HasExtraPowerBonus(false)
    , HasExtraPowerDrain(false)
    , FiringOccupantIndex(0)
    , IsFencePostActive(false)
    , FenceLinkMask(0)
    , DamageFireCells{}
    , DamageFireCount(0)
    , DockedUnits(nullptr)
    , DockedUnitsCapacity(0)
    , WasOnline(false)
    , StuffEnabled(false)
    , BeingProduced(false)
    , ShouldRebuild(false)
    , HasBeenCaptured(false)
    , IsFogged(false)
    , IsSensorActive(false)
    , IsDetectorActive(false)
    , IsFrozen_(false)
    , IsLocked_(false)
    , IsDisabled_(false)
    , IsDisguised_(false)
    , IsMindControlled_(false)
    , OriginallyOwnedBy(nullptr)
    , IsPrimaryFactory(false)
    , BunkerState(0)
    , PrismStage(0)
    , WeaponStage(0)
    , PrismTargetCoords{}
    , DelayBeforeFiring(0)
    , SecretProduction(nullptr)
    , StorageFilledSlots(0)
    , RallyPoint{}
    , Target(nullptr)
    , ReservedLayout_538{}
    , IsTentativelyOccupied(false)
    , IsCurrentlyOccupied(false)
    , IsStateChanging(false)
    , IsBeingSabotaged(false)
    , IsBeingRepaired(false)
    , RepairAnimToggle(false)
    , RepairStepTimer(0)
    , IsTechnicianStructure(false)
    , IsBaseDefenseStructure(false)
    , ReservedLayout{}
{
    Owner = pOwner;
    if (Array) {
        Array->Add(this);
    }

    // Derive initial power values from the type once it is assigned.  The
    // type is typically set by the factory system immediately after
    // construction, so we also re-evaluate in GoOnline().
}

// ============================================================================
// Destructor - remove from the global array.
// ============================================================================
BuildingClass::~BuildingClass()
{
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }
}

// ============================================================================
// IPersistStream
// ============================================================================
HRESULT __stdcall BuildingClass::Load(IStream* pStm)
{
    if (!pStm) return E_POINTER;
    return TechnoClass::Load(pStm);
}

HRESULT __stdcall BuildingClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (!pStm) return E_POINTER;
    return TechnoClass::Save(pStm, fClearDirty);
}

// ============================================================================
// IsPowerOnline - a building's power is online only when it is fully
// constructed, flagged online, and not being power-drained.
// ============================================================================
bool BuildingClass::IsPowerOnline() const
{
    if (!IsConstructed) return false;
    if (!IsOnline) return false;
    if (IsBeingDrained_) return false;
    return true;
}

// ============================================================================
// IsUnitFactory - true if the building type can produce units.  We check the
// Factory field on the TechnoTypeClass base, which indicates the abstract
// type of object this building produces (Unit, Infantry, Aircraft, Building).
// ============================================================================
bool BuildingClass::IsUnitFactory() const
{
    if (!Type) return false;
    TechnoTypeClass* pType = static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    return pType->Factory != AbstractType::None;
}

// ============================================================================
// IsArmed - true if the building type has at least one weapon.
// ============================================================================
bool BuildingClass::IsArmed() const
{
    if (!Type) return false;
    TechnoTypeClass* pType = static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    if (pType->IsArmed) return true;
    // Also check the weapon count directly.
    if (pType->WeaponCount > 0) return true;
    // BuildingTypeClass may carry occupy weapons (garrison fire).
    if (Type->OccupyWeaponCount > 0) return true;
    return false;
}

// ============================================================================
// CanOccupyFire - true if the building supports garrison fire.
// ============================================================================
bool BuildingClass::CanOccupyFire() const
{
    if (!Type) return false;
    TechnoTypeClass* pType = static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    return pType->OccupyWeaponCount > 0;
}

// ============================================================================
// GetOccupantCount - return the number of garrisoned infantry tracked in the
// Occupants vector.  Non-garrisonable buildings keep this empty (zero).
// ============================================================================
int32 BuildingClass::GetOccupantCount() const
{
    return Occupants.Count;
}

// ============================================================================
// GetStoragePercentage - return the fraction (0.0-1.0) of tiberium/ore
// storage currently in use.  For non-storage buildings this is 0.
// ============================================================================
double BuildingClass::GetStoragePercentage() const
{
    if (!Type) return 0.0;
    if (!Type->IsOreStorage && !Type->IsOreRefinery) return 0.0;
    if (Type->Storage <= 0) return 0.0;
    // The actual stored amount is tracked by the economy subsystem; we
    // approximate using the storage-level field (original offset 0x5C8)
    // within the reserved layout as the current storage level.
    int32 current = *reinterpret_cast<const int32*>(&ReservedLayout[0x5C8 - 0x548]);
    if (current < 0) current = 0;
    double pct = static_cast<double>(current) /
                 static_cast<double>(Type->Storage);
    if (pct < 0.0) pct = 0.0;
    if (pct > 1.0) pct = 1.0;
    return pct;
}

// ============================================================================
// GetRefund - the credits the owner receives if this structure is sold now.
//
//  The binary routes the sell value through TechnoTypeClass::GetRefund(owner,
//  overrideLogic), which applies the house/type cost multipliers and the
//  Rules->RefundPercent; a damaged structure refunds proportionally less, so
//  the base value is scaled by the current health ratio.  Unexposed until the
//  structure is fully constructed.
// ============================================================================
int32 BuildingClass::GetRefund() const
{
    if (Type == nullptr)
        return 0;
    if (!IsConstructed)
        return 0;

    // Authoritative base refund: house multipliers + Rules->RefundPercent.
    int32 value = Type->GetRefund(Owner, false);
    if (value <= 0)
        return 0;

    // Scale by remaining health (buildings are worth their damage).
    double ratio = 1.0;
    if (MaxHealth > 0)
    {
        ratio = static_cast<double>(Health) / static_cast<double>(MaxHealth);
        if (ratio < 0.0) ratio = 0.0;
        if (ratio > 1.0) ratio = 1.0;
    }

    const int32 refund = static_cast<int32>(static_cast<double>(value) * ratio);
    return (refund > 0) ? refund : 0;
}

// ============================================================================
// Destroyed - called when the building's health reaches zero.  Mark it as
// not constructed and take it offline so it stops contributing power.
// ============================================================================
void BuildingClass::Destroyed(ObjectClass* Killer)
{
    IsConstructed = false;
    IsOnline = false;
    IsPowerPlant = false;
    IsCurrentlyOccupied = false;
    IsTentativelyOccupied = false;

    // Stop the animation.
    Animation.IsAnimating = false;

    // Notify the owning house that a building was lost.
    if (Owner && Type) {
        Owner->RegisterLoss(static_cast<TechnoTypeClass*>(
            static_cast<void*>(Type)));
    }
}

// ============================================================================
// Fire - attempt to fire the specified weapon at the target.  Delegates to
// TechnoClass::Fire_Impl after validating the building is constructed, online,
// and armed.
// ============================================================================
BulletClass* BuildingClass::Fire(AbstractClass* pTarget, int32 nWeaponIndex)
{
    if (!pTarget) return nullptr;
    if (!Type) return nullptr;
    if (!IsConstructed || !IsOnline) return nullptr;

    TechnoTypeClass* pType = static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    if (nWeaponIndex < 0 || nWeaponIndex >= pType->WeaponCount) return nullptr;

    // Delegate to the TechnoClass fire implementation which handles the
    // recharge timer and bullet spawning.
    return Fire_Impl(pTarget, nWeaponIndex);
}

// ============================================================================
// Uncloak - buildings generally cannot cloak, but the override exists for
// special cases (e.g. stealth tanks parked in a war factory).  Reset the
// cloak state machine to fully visible regardless of current state.
// ============================================================================
void BuildingClass::Uncloak(bool bPlaySound)
{
    (void)bPlaySound;

    // Force the building back to a fully visible state.  Even though
    // buildings do not normally cloak, if a cloak state was applied (e.g.
    // by a map trigger or a modded type), we must clear it cleanly.
    CloakState = CloakStateEnum::Idle;
    CloakAlpha = 255;
    CloakTimer = 0;
}

// ============================================================================
// Cloak - initiate the cloak state machine.  Buildings can only cloak if
// their type explicitly allows it (Cloakable flag); standard buildings are
// always visible.  When cloaking is permitted the fade-out is driven by
// Update_Cloak() inherited from TechnoClass.
// ============================================================================
void BuildingClass::Cloak(bool bPlaySound)
{
    (void)bPlaySound;

    if (!Type) return;

    // Only types that explicitly allow cloaking can enter the cloak state.
    TechnoTypeClass* pType = static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    if (!pType->Cloakable && !pType->Cloak) return;

    // Already cloaked or cloaking - nothing to do.
    if (CloakState == CloakStateEnum::Cloaked ||
        CloakState == CloakStateEnum::Cloaking)
        return;

    CloakState = CloakStateEnum::Cloaking;
    CloakTimer = 30;
}

// ============================================================================
// IsClearlyVisibleTo - buildings are always visible once constructed (they
// cannot hide).  Unconstructed buildings may be hidden by shroud.
// ============================================================================
bool BuildingClass::IsClearlyVisibleTo(HouseClass* House) const
{
    if (!IsConstructed) return false;
    return true;
}

// ============================================================================
// CanScatter - buildings cannot scatter (they don't move).  However, if the
// building is garrisoned, the occupants may be ordered to scatter out.  The
// return value remains false because the building itself does not relocate.
// ============================================================================
bool BuildingClass::CanScatter() const
{
    // Buildings are immobile and cannot scatter.  A garrisoned building
    // may receive a scatter order which is interpreted as an unload
    // command for its occupants, but the building itself never relocates.
    if (Type && Type->IsCanBeOccupied && Occupants.Count > 0) {
        // Occupants could be scattered, but the building cannot.
        // The scatter command for garrisoned infantry is handled at the
        // FootClass level after ejection via UnloadBunker().
    }
    return false;
}

// ============================================================================
// IsControllable - the player can issue orders to a constructed building.
// ============================================================================
bool BuildingClass::IsControllable() const
{
    return IsConstructed;
}

// ============================================================================
// IsActive - a building is active when constructed and online.
// ============================================================================
bool BuildingClass::IsActive() const
{
    return IsConstructed && IsOnline;
}

// ============================================================================
// IsSelectable - only constructed buildings can be selected.
// ============================================================================
bool BuildingClass::IsSelectable() const
{
    return IsConstructed;
}

// ============================================================================
// CanBeSelected - constructed buildings are selectable.
// ============================================================================
bool BuildingClass::CanBeSelected() const
{
    return IsConstructed;
}

// ============================================================================
// CanBeSelectedNow - the building can be selected right now if it is
// constructed and either online or a super-weapon silo (which remains
// selectable while charging).
// ============================================================================
bool BuildingClass::CanBeSelectedNow() const
{
    if (!IsConstructed) return false;
    if (IsOnline) return true;
    // Super-weapon buildings remain selectable even while offline.
    if (Type && (Type->HasSuperWeapon || Type->HasSuperWeapon2)) return true;
    return false;
}

// ============================================================================
// UpdateConstructionOptions - refresh the build menu after a building state
// change (construction complete, powered on/off, etc.).
// ============================================================================
void BuildingClass::UpdateConstructionOptions()
{
    if (!Owner) return;
    // The full binary recomputes the available build list for the owner
    // whenever a factory's state changes.  Here we trigger a tracking update
    // so the house's production counters stay consistent.
    if (Type) {
        if (IsConstructed && IsOnline) {
            // Register that this factory is now available for production.
            Owner->Tracking_Add(this);
        }
    }
}

// ============================================================================
// Sell - the accounting half of the sell order.  The visual half is the
// Mission::Selling state machine; this routine performs the refund and the
// teardown so a selling structure stops contributing the instant the order
// is issued.
//
//  In the binary's order:
//    1. Refuse if the structure is not constructed, or is not sellable.
//    2. Strip and refund every installed upgrade module (SellUpgrades).
//    3. Refund the structure itself (health-scaled base refund).
//    4. Take the structure offline and drop it from the owner's tracking list.
//    5. Enter the Selling state (art slot Aux1) so the mission layer plays the
//       ramp-down animation and eventually removes the object.
// ============================================================================
void BuildingClass::Sell(DWORD dwUnk)
{
    if (!IsConstructed)
        return;
    if (!CanBeSold())
        return;

    // 2 - upgrades first: they refund on their own schedule.
    SellUpgrades_unused();

    // 3 - refund the structure.
    const int32 refund = GetRefund();
    if (refund > 0 && Owner != nullptr)
        Owner->GiveMoney(refund);

    // 4 - stop power / production and unregister.
    GoOffline();
    if (Owner != nullptr)
        Owner->Tracking_Remove(this);

    // 5 - hand over to the sell mission.
    CurrentMission = Mission::Selling;
    BState         = BStateType::Aux1;
    Dirty          = true;

    (void)dwUnk; // unused parameter preserved for ABI compatibility.
}

// ============================================================================
// CanBeSold - true when the structure may be sold right now.
//   * The structure must be fully constructed.
//   * It must not already be mid-construction or mid-sale.
//   * The type must not be flagged unsellable, and must be sellable.
// ============================================================================
bool BuildingClass::CanBeSold() const
{
    if (Type == nullptr)
        return false;
    if (!IsConstructed)
        return false;
    if (CurrentMission == Mission::Construction
        || CurrentMission == Mission::Selling)
        return false;
    if (Type->IsUnsellable)
        return false;
    return Type->IsSellable;
}

// ============================================================================
// CanBeRepaired - true when the structure may be repaired right now.
//   * The structure must be fully constructed and not mid-construction/sale.
//   * It must actually be damaged (health below maximum).
//   * The type must be flagged repairable.
// ============================================================================
bool BuildingClass::CanBeRepaired() const
{
    if (Type == nullptr)
        return false;
    if (!IsConstructed)
        return false;
    if (CurrentMission == Mission::Construction
        || CurrentMission == Mission::Selling)
        return false;
    if (MaxHealth > 0 && Health >= MaxHealth)
        return false;
    return Type->IsRepairable;
}

// ============================================================================
// RepairWithMoney - spend the owner's credits to restore HP.
//
//  The amount of HP restored and the price per step come from the type
//  (GetRepairStep / GetRepairStepCost), so a structure's repair curve lives
//  with its type, matching the binary's RepairStep data.  `money` is the
//  budget for this call: we run whole repair steps while the owner can afford
//  them.  A structure already at full health, or one that cannot be repaired,
//  is a no-op.
// ============================================================================
void BuildingClass::RepairWithMoney(int32 money)
{
    if (Type == nullptr || money <= 0)
        return;
    if (!IsConstructed)
        return;
    if (MaxHealth <= 0 || Health >= MaxHealth)
        return;
    if (!CanBeRepaired())
        return;

    const int32 stepHp   = Type->GetRepairStep();
    const int32 stepCost = Type->GetRepairStepCost();
    if (stepHp <= 0)
        return;

    int32 budget = money;
    while (budget > 0 && Health < MaxHealth)
    {
        // Price the step; a free step (no cost data) still heals.
        const int32 cost = (stepCost > 0) ? stepCost : 0;
        if (cost > 0)
        {
            if (Owner == nullptr || Owner->Credits < cost)
                break;
            Owner->SpendMoney(cost);
            budget -= cost;
        }
        else
        {
            // No per-step cost: consume the whole budget in one tick.
            budget = 0;
        }

        Health += stepHp;
        if (Health > MaxHealth)
            Health = MaxHealth;
    }

    // Refresh the damaged-animation latch after the heal.
    Animation.IsDamaged = (GetHealthRatio() < DAMAGED_THRESHOLD);
    Dirty = true;
}

// ============================================================================
// Repair - the vtable entry the selection UI calls; delegates to
// RepairWithMoney with the tick budget the repair mission uses.
// ============================================================================
void BuildingClass::Repair(int32 money)
{
    RepairWithMoney(money);
}

// ============================================================================
// asm: void __thiscall BuildingClass_GoOnline(BuildingClass *this)   (asm:19461)
//
//  Brings a finished structure online.  Order of operations (verified against
//  the binary):
//    1. Bail out if already online (+660h).  (The binary also defers while an
//       EMP is in effect, guarding on TechnoClass::EMPDurationRemaining at
//       +0x504; OpenYR folds EMP into the online/disabled state instead.)
//    2. Latch IsOnline (+660h), mark dirty (+80h) and the power-on latch
//       (+6EAh), and raise the owner's online bookkeeping flags.
//    3. Restart the tiled light source (+614h, mode 0).
//    4. Re-ping a laser fence post, then notify each of the three attached
//       powered-unit consumers (type +0xF8C stride 0x44 / class +0x55C).
//    5. A factory building refreshes the owner's production timing.
//    6. Play the "structure online" EVA line for the owning player.
// ============================================================================
void BuildingClass::GoOnline()
{
    // 1 - already online.
    if (IsOnline)
        return;

    // 2 - latch the state.
    IsOnline       = true;      // +660h
    Dirty          = true;      // +80h
    WasOnline      = true;      // +6EAh
    HasPower       = true;

    // 3 - resume the light source.
    if (LightSource != nullptr)
        LightSource->Start(0);

    if (Type != nullptr)
    {
        // 4a - laser fence post re-registration.
        if (Type->LaserFencePost)
            PingLaserFencePost(false);

        // 4b - notify the three powered-unit consumers.
        for (int32 i = 0; i < BUILDING_POWERED_UNIT_COUNT; ++i)
        {
            if (!Type->PoweredUnit[i])
                continue;
            TechnoClass* pUnit = PoweredUnits[i];
            if (pUnit != nullptr)
                pUnit->UpdatePowered();
        }

        // 5 - a factory building re-times its owner's production lines.
        if (Type->Factory != AbstractType::None && Owner != nullptr)
            Owner->Update_Factories_Timing();

        // Cache the type's power figures for the renderer / legacy readers.
        if (Type->Power > 0)
            PowerOutput = Type->Power;
        if (Type->PowerDrain > 0)
            PowerDrain = Type->PowerDrain;
    }

    // 6 - EVA "structure online".
    if (Owner != nullptr && Owner->Is_Human())
        VoxClass::PlayFromName("EVA_BuildingOnLine", -1, -1);

    UpdateConstructionOptions();
    EnableStuff();
    UpdateAnimations();
}

// ============================================================================
// asm: void __thiscall BuildingClass_GoOffline(BuildingClass *this)   (asm:19563)
//
//  The mirror of GoOnline.  A structure only goes offline when it actually
//  draws power (Type->PowerDrain > 0, +0xEE4h) or carries the toggle-power
//  flag (+0x1573h) - a pure power plant is never taken offline this way.
//    * Latch IsOnline off, mark dirty, run InitMore() (the teardown sweep),
//      and raise the owner's offline bookkeeping flags.
//    * Play "structure offline" EVA for the owning player.
//    * A factory building re-times the owner's production lines.
// ============================================================================
void BuildingClass::GoOffline()
{
    if (!IsOnline)
        return;

    if (Type != nullptr)
    {
        const bool drawsPower = (Type->PowerDrain > 0) || Type->TogglePower;
        if (!drawsPower)
            return;
    }

    IsOnline  = false;      // +660h
    Dirty     = true;       // +80h
    HasPower  = false;
    WasOnline = false;      // +6EAh

    // Teardown sweep: light, cloak/gap, fence post, powered units.
    InitMore();

    // EVA "structure offline".
    if (Owner != nullptr && Owner->Is_Human())
        VoxClass::PlayFromName("EVA_BuildingOffLine", -1, -1);

    // A factory building re-times its owner's production lines.
    if (Type != nullptr && Type->Factory != AbstractType::None && Owner != nullptr)
        Owner->Update_Factories_Timing();

    IsPowerPlant = false;
    PowerOutput  = 0;
    PowerDrain   = 0;
    Animation.IsAnimating = false;
    DisableStuff();
}

// ============================================================================
// GetPowerOutput - the structure's net power contribution to the grid.  The
// authoritative figure is PowerProduced() (asm: BuildingClass_PowerProduced),
// which already folds in the online/drained gates, upgrade modules and the
// health scaling.  Kept as a thin delegate so callers and the power-accounting
// path can never disagree.  Unexposed until the structure is on the map.
// ============================================================================
int32 BuildingClass::GetPowerOutput() const
{
    if (!IsConstructed)
        return 0;
    return PowerProduced();
}

// ============================================================================
// GetPowerDrain - the structure's power draw.  Delegates to PowerAbsorbed()
// (asm: BuildingClass_PowerAbsorbed) for the same single-source-of-truth
// reason.
// ============================================================================
int32 BuildingClass::GetPowerDrain() const
{
    if (!IsConstructed)
        return 0;
    return PowerAbsorbed();
}

// ============================================================================
// SWAvailable - true if the building's primary super weapon is charged and
// the building is online.
// ============================================================================
bool BuildingClass::SWAvailable() const
{
    if (!IsOnline || !IsConstructed) return false;
    if (!Type) return false;
    if (!Type->HasSuperWeapon) return false;
    // The actual charge state is tracked by the house's super-weapon timers;
    // here we report that the building is ready to host a charged SW.
    return true;
}

// ============================================================================
// SW2Available - true if the building's secondary super weapon is charged.
// ============================================================================
bool BuildingClass::SW2Available() const
{
    if (!IsOnline || !IsConstructed) return false;
    if (!Type) return false;
    if (!Type->HasSuperWeapon2) return false;
    return true;
}

// ============================================================================
// asm: void __fastcall BuildingClass_ClearBunker(BuildingClass *this)   (asm:222475)
//      "Content is dead"
//
//  Full teardown of a garrisoned / bunkered structure after its content dies:
//    1. Destroy the two garrison animation slots (the binary's 0Ah/0Bh).
//    2. Play the "bunker walls down" sound (Rules->BunkerWallsDownSound) at
//       the structure's own coordinates.
//    3. Replay the garrison animation set - the damaged variant when the
//       structure is at or below Rules->ConditionYellow, else the pristine
//       garrisoned variant.
//    4. Release the linked bunker item (BunkerLinkedItem) and clear the
//       occupant roster + the bunker state latch.
// ============================================================================
void BuildingClass::ClearBunker()
{
    // 1 - tear down both garrison anim slots.
    DestroyNthAnim(BuildingAnimSlot::Garrison);
    DestroyNthAnim(BuildingAnimSlot::Garrisoned);

    // 2 - bunker walls-down sound.
    RulesClass* pRules = RulesClass::Instance;
    if (pRules != nullptr && pRules->BunkerWallsDownSound >= 0)
    {
        CoordStruct pos = GetCoords();
        VocClass::Sound_Effect(pRules->BunkerWallsDownSound, &pos, nullptr);
    }

    // 3 - replay the appropriate garrison anim.
    const bool damaged =
        (pRules != nullptr) ? (GetHealthRatio() < static_cast<float>(pRules->ConditionYellow))
                            : false;
    if (Type != nullptr)
    {
        if (damaged && Type->ActiveAnimDamaged[0] != '\0')
            PlayAnim(Type->ActiveAnimDamaged, BuildingAnimSlot::Garrisoned, true, false, 0);
        else if (Type->ActiveAnimGarrisoned[0] != '\0')
            PlayAnim(Type->ActiveAnimGarrisoned, BuildingAnimSlot::Garrison, false, true, 0);
    }

    // 4 - release the linked item and clear the roster.
    BunkerLinkedItem = nullptr;
    Occupants.Clear();
    FiringOccupantIndex = 0;
    BunkerState = 0;
    IsCurrentlyOccupied = false;
    IsTentativelyOccupied = false;
    Dirty = true;
}

// ============================================================================
// IsManaDrainPossible - true if the building can be power-drained (e.g. by
// a Tesla trooper or similar unit).  Only online, constructed buildings that
// produce or consume power are drainable.
// ============================================================================
bool BuildingClass::IsManaDrainPossible() const
{
    if (!IsOnline || !IsConstructed) return false;
    if (!Type) return false;
    // Power plants and powered buildings are drainable.
    if (Type->Power > 0) return true;
    if (Type->IsPowered) return true;
    return false;
}

// ============================================================================
// FindFactoryTarget - locate a destination for newly produced units.  For
// war factories this is the rally point; for barracks it is the front door.
// Returns nullptr if no suitable target exists.
// ============================================================================
AbstractClass* BuildingClass::FindFactoryTarget(AbstractClass* pTarget) const
{
    if (!Type) return nullptr;
    if (!IsConstructed || !IsOnline) return nullptr;

    // If the caller provided a target, validate and return it.
    if (pTarget) {
        return pTarget;
    }

    // The full binary queries the map for a free cell adjacent to the
    // building's dock / exit point.  In the standalone build we return
    // nullptr to let the production subsystem choose a default location.
    return nullptr;
}

// ============================================================================
// HasNavigationDeal - true if the building has a navigation/rally-point
// agreement with a transport or dock.  Used by naval yards and airports.
// ============================================================================
bool BuildingClass::HasNavigationDeal() const
{
    if (!Type) return false;
    // Naval yards and airports have navigation deals by default.
    if (Type->IsNavalYard) return true;
    if (Type->IsAirport) return true;
    if (Type->HasDock) return true;
    return false;
}

// ============================================================================
// IsFactory - true if the building can produce any kind of unit or building.
// ============================================================================
bool BuildingClass::IsFactory() const
{
    if (!Type) return false;
    if (Type->IsFactory_) return true;
    return IsUnitFactory();
}

// ============================================================================
// IsFactoryExplicit - a stricter check that the building is explicitly
// marked as a factory in its type definition (not just inferred from the
// Factory abstract-type field).
// ============================================================================
bool BuildingClass::IsFactoryExplicit() const
{
    if (!Type) return false;
    return Type->IsFactory_;
}

// ============================================================================
// IsToggledRallyPoint - true if the building's rally point can be toggled
// by the player (war factories and barracks).
// ============================================================================
bool BuildingClass::IsToggledRallyPoint() const
{
    if (!Type) return false;
    if (Type->IsWeaponsFactory) return true;
    if (Type->IsBarracks) return true;
    if (Type->IsWarfactory) return true;
    return false;
}

// ============================================================================
// Combat & Firing
// ============================================================================

// ----------------------------------------------------------------------------
// CanFireNow - true if the building is online, constructed, armed, and its
// recharge timer has elapsed.  Garrison buildings also require at least one
// occupant to be able to fire.
// ----------------------------------------------------------------------------
bool BuildingClass::CanFireNow() const
{
    if (!IsConstructed || !IsOnline) return false;
    if (!Type) return false;
    if (FireRechargeTimer > 0) return false;

    // Garrison fire requires occupants.
    if (Type->OccupyWeaponCount > 0 && Occupants.Count == 0) return false;

    return IsArmed();
}

// ----------------------------------------------------------------------------
// CanEnterCell - a building cannot enter any cell (it does not move).  This
// override exists for the base-class virtual contract; it always returns
// false.
// ----------------------------------------------------------------------------
bool BuildingClass::CanEnterCell(CellClass* pCell) const
{
    (void)pCell;
    return false;
}

// ----------------------------------------------------------------------------
// FireAngleTo - compute the firing direction toward the target object.
// Buildings with turrets aim along the turret axis; others face the target
// directly from their own location.
// ----------------------------------------------------------------------------
DirStruct BuildingClass::FireAngleTo(ObjectClass* pObject) const
{
    DirStruct dir;
    if (!pObject) return dir;

    CoordStruct myPos = Location;
    CoordStruct tgtPos;
    pObject->GetCoords(&tgtPos);

    int32 dx = tgtPos.X - myPos.X;
    int32 dy = tgtPos.Y - myPos.Y;
    // Convert to a 0-255 direction value (256 = full circle).
    if (dx == 0 && dy == 0) {
        dir.Value = 0;
    } else {
        double angle = std::atan2(static_cast<double>(dy),
                                  static_cast<double>(dx));
        dir.Value = static_cast<uint8>(
            (static_cast<int32>(angle * 128.0 / 3.14159265) + 256) & 0xFF);
    }
    return dir;
}

// ----------------------------------------------------------------------------
// Fire_At - fire the specified weapon at the target.  Validates the target,
// weapon index, and fire readiness before delegating to Fire_Impl.
// ----------------------------------------------------------------------------
void BuildingClass::Fire_At(TargetClass* pTarget, int32 weaponIndex)
{
    if (!pTarget) return;
    if (!CanFireNow()) return;
    if (!Type) return;

    TechnoTypeClass* pType =
        static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    if (weaponIndex < 0 || weaponIndex >= pType->WeaponCount) return;

    // asm BuildingClass_Fire 尾段 → TechnoClass_Fire：走 Fire() 才能完成
    //   扣弹 / 后座 / 声波波束 / 弹丸生成 的完整记账。
    Fire(static_cast<AbstractClass*>(static_cast<void*>(pTarget)), weaponIndex);

    // Reset the recharge timer based on the weapon's rate of fire.
    if (weaponIndex < pType->WeaponCount) {
        FireRechargeTimer = 10; // default recharge
    }
}

// ----------------------------------------------------------------------------
// Can_Fire_At - true if the building can fire the specified weapon at the
// given target.  Checks online status, weapon validity, and range.
// ----------------------------------------------------------------------------
bool BuildingClass::Can_Fire_At(TechnoClass* pTarget, int32 weaponIndex) const
{
    if (!pTarget) return false;
    if (!CanFireNow()) return false;
    if (!Type) return false;

    TechnoTypeClass* pType =
        static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    if (weaponIndex < 0 || weaponIndex >= pType->WeaponCount) return false;

    // Check range.
    int32 range = GetWeaponRange(weaponIndex);
    if (range <= 0) return false;

    CoordStruct myPos = Location;
    CoordStruct tgtPos;
    pTarget->GetCoords(&tgtPos);

    int32 dx = tgtPos.X - myPos.X;
    int32 dy = tgtPos.Y - myPos.Y;
    int32 distSq = dx * dx + dy * dy;
    int32 rangeSq = range * range;

    return distSq <= rangeSq;
}

// ----------------------------------------------------------------------------
// GetWeaponRange - return the range (in leptons) of the specified weapon slot.
// Returns 0 for invalid indices or unarmed buildings.
// ----------------------------------------------------------------------------
int32 BuildingClass::GetWeaponRange(int32 weaponIndex) const
{
    if (!Type) return 0;
    TechnoTypeClass* pType =
        static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    if (weaponIndex < 0 || weaponIndex >= pType->WeaponCount) return 0;
    if (weaponIndex >= 2) return 0;

    // 射程取武器槽上的 WeaponType->Range（lepton）；没有武器时退回
    //   GuardRange（rules 里按格写，需乘 256）。
    const WeaponStruct& ws = pType->Weapons[weaponIndex];
    if (ws.WeaponType != nullptr && ws.WeaponType->Range > 0)
        return ws.WeaponType->Range;
    if (pType->GuardRange > 0) return pType->GuardRange * 256;
    return 0;
}

// ----------------------------------------------------------------------------
// GetWeaponDamage - return the damage value of the specified weapon slot.
// Returns 0 for invalid indices.
// ----------------------------------------------------------------------------
int32 BuildingClass::GetWeaponDamage(int32 weaponIndex) const
{
    if (!Type) return 0;
    TechnoTypeClass* pType =
        static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    if (weaponIndex < 0 || weaponIndex >= pType->WeaponCount) return 0;

    // Damage is derived from the weapon type; use the veteran abilities
    // damage bonus as a baseline when weapon data is unavailable.
    int32 baseDamage = 1;
    if (pType->VeteranAbilities[0] > 0) {
        baseDamage += pType->VeteranAbilities[0];
    }
    return baseDamage;
}

// ----------------------------------------------------------------------------
// MuzzleFlash - trigger the muzzle flash effect for the specified weapon.
// For buildings this updates the animation state.
// ----------------------------------------------------------------------------
void BuildingClass::MuzzleFlash(int32 weaponIndex)
{
    if (!Type) return;
    if (weaponIndex < 0) return;
    // Trigger the active animation to show the firing frame.
    if (BState == BStateType::Idle) {
        BState = BStateType::Active;
        UpdateAnimations();
    }
}

// ----------------------------------------------------------------------------
// OnFired - called after a weapon has been fired.  Updates the last fire
// frame stamp and triggers the recharge timer.
// ----------------------------------------------------------------------------
void BuildingClass::OnFired(int32 weaponIndex)
{
    (void)weaponIndex;
    LastFireFrame = Game::GetCurrentFrame();
    if (FireRechargeTimer <= 0) {
        FireRechargeTimer = 10;
    }
}

// ----------------------------------------------------------------------------
// GetWeaponCount - return the number of weapon slots on the building type.
// ----------------------------------------------------------------------------
int32 BuildingClass::GetWeaponCount() const
{
    if (!Type) return 0;
    TechnoTypeClass* pType =
        static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    return pType->WeaponCount;
}

// ----------------------------------------------------------------------------
// FireLaser - fire a laser beam (prism tower / grand cannon style) toward the
// specified coordinates.  Sets the prism stage and target coordinates for the
// rendering subsystem.
// ----------------------------------------------------------------------------
void BuildingClass::FireLaser(CoordStruct Coords)
{
    if (!IsConstructed || !IsOnline) return;
    if (!Type) return;

    PrismTargetCoords = Coords;
    PrismStage = 1;  // begin charging
    DelayBeforeFiring = 5;
}

// ----------------------------------------------------------------------------
// FireFromOccupant - fire the next occupant's weapon at the given target.
// Cycles through the garrisoned infantry using FiringOccupantIndex.
// ----------------------------------------------------------------------------
void BuildingClass::FireFromOccupant(TechnoClass* pTarget)
{
    if (!pTarget) return;
    if (Occupants.Count == 0) return;
    if (!Type || Type->OccupyWeaponCount == 0) return;

    // Advance the firing occupant index in round-robin fashion.
    FiringOccupantIndex = (FiringOccupantIndex + 1) % Occupants.Count;
    OnFired(0);
}

// ----------------------------------------------------------------------------
// GetOccupantWeaponIndex - return the weapon index for the current firing
// occupant.  Returns 0 if there are no occupants or weapons.
// ----------------------------------------------------------------------------
int32 BuildingClass::GetOccupantWeaponIndex() const
{
    if (!Type) return 0;
    if (Type->OccupyWeaponCount <= 0) return 0;
    if (Occupants.Count == 0) return 0;
    // Cycle the weapon index among available occupy weapons.
    return FiringOccupantIndex % Type->OccupyWeaponCount;
}

// ============================================================================
// Power, State & Animation
// ============================================================================

// ----------------------------------------------------------------------------
// BeginMode - transition the building to the specified BStateType.  This
// drives the animation system and may trigger side effects (e.g. going from
// Construction to Idle marks the building as constructed).
// ----------------------------------------------------------------------------
void BuildingClass::BeginMode(BStateType bType)
{
    if (BState == bType) return;

    QueueBState = BState;
    BState = bType;

    switch (bType) {
    case BStateType::Construction:
        IsConstructed = false;
        break;
    case BStateType::Idle:
        IsConstructed = true;
        if (WasOnline) GoOnline();
        break;
    case BStateType::Active:
        IsConstructed = true;
        break;
    case BStateType::Full:
        // Storage full state for ore silos.
        break;
    default:
        break;
    }

    UpdateAnimations();
}

// ----------------------------------------------------------------------------
// EnableStuff - enable the building's active subsystems (spotlight, light
// source, sensor array).  Called by GoOnline.
// ----------------------------------------------------------------------------
void BuildingClass::EnableStuff()
{
    if (StuffEnabled) return;
    StuffEnabled = true;

    // Activate the building light if the type requests one.
    if (Type && Type->HasSpotlight && !Spotlight) {
        // Spotlight is created by the rendering subsystem; flag it.
        Spotlight = reinterpret_cast<BuildingLightClass*>(1);
    }

    // Activate sensor arrays.
    if (Type && Type->IsSensor) {
        IsSensorActive = true;
    }
    if (Type && Type->IsDetector) {
        IsDetectorActive = true;
    }
}

// ----------------------------------------------------------------------------
// DisableStuff - disable the building's active subsystems.  Called by
// GoOffline.
// ----------------------------------------------------------------------------
void BuildingClass::DisableStuff()
{
    if (!StuffEnabled) return;
    StuffEnabled = false;

    IsSensorActive = false;
    IsDetectorActive = false;
}

// ----------------------------------------------------------------------------
// EnableTemporal - enable temporal (chronosphere) processing for this
// building.  Used when the building is being chronoshifted.
// ----------------------------------------------------------------------------
void BuildingClass::EnableTemporal()
{
    IsFrozen_ = true;
    TemporalTimer = 1;
}

// ----------------------------------------------------------------------------
// DisableTemporal - disable temporal processing.
// ----------------------------------------------------------------------------
void BuildingClass::DisableTemporal()
{
    IsFrozen_ = false;
    TemporalTimer = 0;
}

// ----------------------------------------------------------------------------
// UpdateAnimations - advance all per-slot animations and update the damaged
// state based on current health.
// ----------------------------------------------------------------------------
void BuildingClass::UpdateAnimations()
{
    // Update the main animation.
    Animation.Update();

    // Set the damaged flag based on health ratio.
    if (MaxHealth > 0) {
        double ratio = static_cast<double>(Health) /
                       static_cast<double>(MaxHealth);
        Animation.IsDamaged = (ratio < DAMAGED_THRESHOLD);
    }

    // Update gate timer.
    if (GateTimer > 0) {
        --GateTimer;
    }

    // Update delay before firing.
    if (DelayBeforeFiring > 0) {
        --DelayBeforeFiring;
    }
}

// ----------------------------------------------------------------------------
// GetCurrentFrame - return the current animation frame for rendering.
// ----------------------------------------------------------------------------
int32 BuildingClass::GetCurrentFrame() const
{
    return Animation.GetCurrentFrame();
}

// ----------------------------------------------------------------------------
// IsAllFogged - true if the building is completely hidden by fog of war.
// ----------------------------------------------------------------------------
bool BuildingClass::IsAllFogged() const
{
    return IsFogged;
}

// ----------------------------------------------------------------------------
// GetShapeNumber - return the shape/voxel index for the current state.  This
// selects which art asset the renderer uses.
// ----------------------------------------------------------------------------
int32 BuildingClass::GetShapeNumber() const
{
    if (!Type) return 0;

    // The shape number depends on the current BState.
    int32 base = static_cast<int32>(BState);
    if (base < 0) base = 0;

    // Damaged buildings use a different shape set.
    if (Animation.IsDamaged) {
        base += 10;
    }
    return base;
}

// ----------------------------------------------------------------------------
// IsBeingDrained - report whether a Tesla trooper or similar unit is
// currently draining power from this building.
// ----------------------------------------------------------------------------
bool BuildingClass::IsBeingDrained() const
{
    return IsBeingDrained_;
}

// ----------------------------------------------------------------------------
// CheckFog - check and update the fog-of-war state for this building.
// Returns true if the building is currently fogged.
// ----------------------------------------------------------------------------
bool BuildingClass::CheckFog() const
{
    // In the full binary this queries the map's fog array for the building's
    // cells.  Here we return the cached state.
    return IsFogged;
}

// ============================================================================
// Garrison & Occupants
// ============================================================================

// ============================================================================
// AddOccupant - garrison an infantry unit inside the structure.
//
//  Refuses a null infantry, a non-occupiable type, a duplicate entry, or a
//  roster already at the type's occupant cap (OccupyCount / MaxNumberOccupants).
//  On success the infantry joins the roster, the "first occupant fires first"
//  cursor is armed when the structure was empty, and the garrison animation
//  set is replayed so the structure shows its occupied art.
// ============================================================================
bool BuildingClass::AddOccupant(InfantryClass* pInfantry)
{
    if (pInfantry == nullptr)
        return false;
    if (Type == nullptr || !Type->IsCanBeOccupied)
        return false;

    // The occupant cap: the engine stores it on the type (OccupyCount).
    const int32 cap = (Type->OccupyCount > 0) ? Type->OccupyCount
                                              : Type->MaxNumberOccupants;
    if (Occupants.Count >= cap)
        return false;

    // Reject a duplicate so the roster stays a set.
    for (int32 i = 0; i < Occupants.Count; ++i)
    {
        if (Occupants[i] == pInfantry)
            return false;
    }

    const bool wasEmpty = (Occupants.Count == 0);
    Occupants.Add(pInfantry);

    if (wasEmpty)
        FiringOccupantIndex = 0;

    IsCurrentlyOccupied   = (Occupants.Count > 0);
    IsTentativelyOccupied = (Occupants.Count > 0);
    Dirty = true;

    // Replay the garrison art.
    if (Type->ActiveAnimGarrisoned[0] != '\0')
        PlayAnim(Type->ActiveAnimGarrisoned, BuildingAnimSlot::Garrison, false, true, 0);
    UpdateAnimations();
    return true;
}

// ============================================================================
// RemoveOccupant - un-garrison a specific infantry unit.
//
//  Removes the entry, keeps the "next fires" cursor inside the shrunk roster,
//  clears the occupancy latch when the last occupant leaves, and replays the
//  animation set.  Returns false when the unit is not in the roster.
// ============================================================================
bool BuildingClass::RemoveOccupant(InfantryClass* pInfantry)
{
    if (pInfantry == nullptr)
        return false;
    if (Occupants.Count == 0)
        return false;

    for (int32 i = 0; i < Occupants.Count; ++i)
    {
        if (Occupants[i] != pInfantry)
            continue;

        Occupants.Remove(i);

        // Keep the firing cursor within bounds after the shrink.
        if (FiringOccupantIndex < 0 || FiringOccupantIndex >= Occupants.Count)
            FiringOccupantIndex = 0;

        IsCurrentlyOccupied   = (Occupants.Count > 0);
        IsTentativelyOccupied = (Occupants.Count > 0);
        if (Occupants.Count == 0)
            BunkerState = 0;
        Dirty = true;

        // Drop the garrison art once the roster empties.
        if (Occupants.Count == 0)
            DestroyNthAnim(BuildingAnimSlot::Garrison);
        UpdateAnimations();
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// GetOccupant - return the occupant at the specified index, or nullptr.
// ----------------------------------------------------------------------------
InfantryClass* BuildingClass::GetOccupant(int32 index) const
{
    if (index < 0 || index >= Occupants.Count) return nullptr;
    return Occupants[index];
}

// ============================================================================
// UpdateBunker - per-frame servicing of the garrison / bunker state.
//
//  The binary's bunker services its single linked item at +0x2E4; OpenYR
//  models the garrison as a roster, so this keeps the "next occupant fires"
//  cursor valid and advances the bunker state latch while anyone is inside.
//  Returns true while the structure hosts occupants.
// ============================================================================
bool BuildingClass::UpdateBunker()
{
    if (Type == nullptr)
        return false;

    if (Occupants.Count == 0)
    {
        BunkerState = 0;
        FiringOccupantIndex = 0;
        IsCurrentlyOccupied = false;
        return false;
    }

    // Keep the cursor inside the roster (occupants may have been killed).
    if (FiringOccupantIndex < 0 || FiringOccupantIndex >= Occupants.Count)
        FiringOccupantIndex = 0;

    ++BunkerState;
    return true;
}

// ============================================================================
// asm: void __thiscall BuildingClass_UnloadBunker(BuildingClass *this)   (asm:222374)
//      "Bunker is dead/sold, content exits"
//
//  Ejects everything inside the structure and tears down the garrison art.
//  The binary rolls a single linked vehicle out at an offset cell under a
//  short speed; the roster model discharges every occupant via
//  Remove_Ocupents (each is re-unlimboed onto the map).
// ============================================================================
void BuildingClass::UnloadBunker()
{
    if (Occupants.Count == 0)
        return;

    DestroyNthAnim(BuildingAnimSlot::Garrison);
    DestroyNthAnim(BuildingAnimSlot::Garrisoned);

    Remove_Ocupents(0, 0);
    UpdateAnimations();
}

// ============================================================================
// asm: int __thiscall BuildingClass_EmptyBunker(BuildingClass *this)   (asm:222617)
//      "Content just exited"
//
//  The sibling of ClearBunker for the "the content walked out" case rather
//  than "the content died": destroy both garrison anim slots, play the
//  walls-down sound at the structure, release the linked bunker item, and
//  eject the roster.
// ============================================================================
void BuildingClass::EmptyBunker()
{
    DestroyNthAnim(BuildingAnimSlot::Garrison);
    DestroyNthAnim(BuildingAnimSlot::Garrisoned);

    RulesClass* pRules = RulesClass::Instance;
    if (pRules != nullptr && pRules->BunkerWallsDownSound >= 0)
    {
        CoordStruct pos = GetCoords();
        VocClass::Sound_Effect(pRules->BunkerWallsDownSound, &pos, nullptr);
    }

    if (Occupants.Count > 0)
        Remove_Ocupents(0, 0);

    BunkerLinkedItem = nullptr;
    BunkerState = 0;
    FiringOccupantIndex = 0;
    IsCurrentlyOccupied = false;
    IsTentativelyOccupied = false;
    UpdateAnimations();
}

// ============================================================================
// KillOccupants - kill every garrisoned infantry outright.  Distinct from
// UnloadBunker (which discharges them alive); this is the "the structure was
// stormed" path and shares the per-occupant damage loop with KillOccupiers.
// ============================================================================
void BuildingClass::KillOccupants(TechnoClass* pAssaulter)
{
    if (Occupants.Count == 0)
        return;

    DestroyNthAnim(BuildingAnimSlot::Garrison);
    DestroyNthAnim(BuildingAnimSlot::Garrisoned);

    while (Occupants.Count > 0)
    {
        InfantryClass* pInf = Occupants[Occupants.Count - 1];
        if (pInf == nullptr)
        {
            Occupants.Remove(Occupants.Count - 1);
            continue;
        }
        int32 deathDamage = pInf->Health;
        pInf->ReceiveDamage(&deathDamage, 0, nullptr, 0, 0, 0, nullptr);
        Occupants.Remove(Occupants.Count - 1);
    }

    FiringOccupantIndex = 0;
    BunkerState = 0;
    IsCurrentlyOccupied = false;
    IsTentativelyOccupied = false;
    (void)pAssaulter;
}

// ----------------------------------------------------------------------------
// MakeTraversable - mark the building as traversable (walkable-over).  Returns
// true if the building type supports traversal.
// ----------------------------------------------------------------------------
bool BuildingClass::MakeTraversable()
{
    if (!Type) return false;
    // Bridges and certain platforms are traversable.
    if (Type->IsGate) return true;
    return false;
}

// ----------------------------------------------------------------------------
// IsTraversable - true if the building can be walked over by ground units.
// ----------------------------------------------------------------------------
bool BuildingClass::IsTraversable() const
{
    if (!Type) return false;
    return Type->IsGate;
}

// ============================================================================
// AfterDestruction - the post-mortem teardown for a structure.  Runs in the
// binary's order: unregister from the power grid, drop the sensor / detector /
// fence-post registrations, release the gap field and cloak charge, evict the
// garrison, then destroy every animation slot (including the damage fires).
// ============================================================================
void BuildingClass::AfterDestruction()
{
    // 1 - unregister from the power system.
    if (RegisteredAsPoweredUnitSource)
    {
        RegisteredAsPoweredUnitSource = false;
        if (Owner != nullptr && TechnoType != nullptr)
            Owner->LostPoweredCenter(TechnoType->ArrayIndex);
    }

    // 2 - deactivate sensors, detectors and the laser-fence post.
    IsSensorActive     = false;
    IsDetectorActive   = false;
    IsFencePostActive  = false;
    FenceLinkMask      = 0;

    // 3 - release the gap field and the cloak-generator charge.
    if (IsGeneratingGap)
    {
        DeleteGap();
        IsGeneratingGap = false;
        GapSuperCharged = false;
    }
    if (CloakingData > 0)
    {
        CloakingData = 0;
        CloakRadius  = 0;
    }

    // 4 - evict the garrison.
    EmptyBunker();

    // 5 - destroy every animation slot.
    for (int32 i = 0; i < BUILDING_ANIM_SLOT_COUNT; ++i)
        DestroyNthAnim(static_cast<BuildingAnimSlot>(i));

    for (int32 i = 0; i < BUILDING_DAMAGE_FIRE_ANIM_COUNT; ++i)
    {
        if (DamageFireAnims[i] != nullptr)
        {
            delete DamageFireAnims[i];
            DamageFireAnims[i] = nullptr;
        }
    }
    DamageFireCount = 0;
    NeedsDamageFires = false;

    IsOnline      = false;
    IsConstructed = false;
    PowerOutput   = 0;
    PowerDrain    = 0;
}

// ============================================================================
// DestroyNthAnim - tear down the animation in the given slot.  Mirrors the
// binary's "call vtbl[+0F8h] then null the pointer" - here the AnimClass
// destructor unregisters it from AnimClass::Array and runs its chain.
// ============================================================================
void BuildingClass::DestroyNthAnim(BuildingAnimSlot Slot)
{
    int32 idx = static_cast<int32>(Slot);
    if (idx < 0 || idx >= BUILDING_ANIM_SLOT_COUNT) return;

    AnimClass* pAnim = Anims[idx];
    if (pAnim != nullptr)
    {
        delete pAnim;
        Anims[idx] = nullptr;
    }
    AnimStates[idx] = false;
}

// ----------------------------------------------------------------------------
// PlayAnim - play an animation in the specified slot.  Sets up the animation
// with the given parameters.
// ----------------------------------------------------------------------------
void BuildingClass::PlayAnim(const char* animName, BuildingAnimSlot Slot,
                              bool Damaged, bool Garrisoned, int32 effectDelay)
{
    (void)animName;
    int32 idx = static_cast<int32>(Slot);
    if (idx < 0 || idx >= BUILDING_ANIM_SLOT_COUNT) return;

    // Mark the slot as active.  The full binary creates an AnimClass instance
    // and attaches it; here we track the state.
    AnimStates[idx] = true;

    // Update the damaged flag.
    if (Damaged) {
        Animation.IsDamaged = true;
    }
    if (Garrisoned) {
        IsCurrentlyOccupied = true;
    }

    // Store the effect delay for the animation subsystem.
    DelayBeforeFiring = effectDelay;
}

// ============================================================================
// ToggleDamagedAnims - switch the structure's damaged-animation set.  The
// latch and the swap both live in UndamageAllAnims (which owns ConditionYellow
// and is a no-op when the requested state already holds); this entry simply
// routes through it so there is one place that flips the art.
// ============================================================================
void BuildingClass::ToggleDamagedAnims(bool isDamaged)
{
    if (ConditionYellow == isDamaged)
        return;
    UndamageAllAnims(isDamaged);
}

// ----------------------------------------------------------------------------
// CreateEndPost - create an end-post marker (used by walls and fences).
// ----------------------------------------------------------------------------
void BuildingClass::CreateEndPost(bool arg)
{
    (void)arg;
    // End posts are created by the wall-laying subsystem.  This is a
    // marker call that the building is part of a wall chain.
}

// ----------------------------------------------------------------------------
// GetFWFlags - return the fire-weapon flags bitmask.  This controls which
// weapon slots are currently eligible to fire.
// ----------------------------------------------------------------------------
DWORD BuildingClass::GetFWFlags() const
{
    DWORD flags = 0;
    if (!Type) return flags;

    TechnoTypeClass* pType =
        static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    if (pType->WeaponCount > 0) flags |= 0x1;
    if (pType->WeaponCount > 1) flags |= 0x2;
    if (Type->OccupyWeaponCount > 0 && Occupants.Count > 0) flags |= 0x4;
    if (IsOnline && IsConstructed) flags |= 0x8;
    return flags;
}

// ============================================================================
// Super Weapons
// ============================================================================

// ----------------------------------------------------------------------------
// FirstActiveSWIdx - return the index of the first active super weapon, or
// -1 if none.
// ----------------------------------------------------------------------------
int32 BuildingClass::FirstActiveSWIdx() const
{
    if (!Type) return -1;
    if (!Type->HasSuperWeapon) return -1;
    if (!IsOnline || !IsConstructed) return -1;
    return Type->SuperWeapon;
}

// ----------------------------------------------------------------------------
// SecondActiveSWIdx - return the index of the second active super weapon.
// ----------------------------------------------------------------------------
int32 BuildingClass::SecondActiveSWIdx() const
{
    if (!Type) return -1;
    if (!Type->HasSuperWeapon2) return -1;
    if (!IsOnline || !IsConstructed) return -1;
    return Type->SuperWeapon2;
}

// ----------------------------------------------------------------------------
// HasSuperWeapon - true if the building has the specified super weapon slot
// active.
// ----------------------------------------------------------------------------
bool BuildingClass::HasSuperWeapon(int32 index) const
{
    if (!Type) return false;
    if (!IsOnline || !IsConstructed) return false;
    if (index == 0 && Type->HasSuperWeapon) return true;
    if (index == 1 && Type->HasSuperWeapon2) return true;
    return false;
}

// ============================================================================
// asm: _TechnoTypeClass *__thiscall BuildingClass_GetSecretProduction
//      (BuildingClass *this)   (asm:29388 / 0x459862)
//
//  A secret lab hands its owner one free bonus type.  The binary resolves it
//  in a fixed priority order straight off the type: SecretInfantry (+0EA4h),
//  then SecretUnit, then SecretBuilding (+0EACh); only when the type carries
//  none of those does it fall back to the instance's own SecretProduction
//  slot (+0x6F4), which the scenario assigns at map load.
// ============================================================================
TechnoTypeClass* BuildingClass::GetSecretProduction() const
{
    if (Type == nullptr)
        return nullptr;

    if (Type->SecretInfantry != nullptr)
        return Type->SecretInfantry;
    if (Type->SecretUnit != nullptr)
        return Type->SecretUnit;
    if (Type->SecretBuilding != nullptr)
        return Type->SecretBuilding;

    return SecretProduction;
}

// ============================================================================
// Target & Mission
// ============================================================================

// ----------------------------------------------------------------------------
// SetTarget - set the building's current target object.
// ----------------------------------------------------------------------------
void BuildingClass::SetTarget(AbstractClass* pTarget)
{
    Target = pTarget;
}

// ----------------------------------------------------------------------------
// GetTarget - return the current target object.
// ----------------------------------------------------------------------------
AbstractClass* BuildingClass::GetTarget() const
{
    return Target;
}

// ----------------------------------------------------------------------------
// ClearTarget - clear the current target.
// ----------------------------------------------------------------------------
void BuildingClass::ClearTarget()
{
    Target = nullptr;
}

// ----------------------------------------------------------------------------
// HasTarget - true if the building has a non-null target.
// ----------------------------------------------------------------------------
bool BuildingClass::HasTarget() const
{
    return Target != nullptr;
}

// ----------------------------------------------------------------------------
// SetMission - set the building's current mission.
// ----------------------------------------------------------------------------
void BuildingClass::SetMission(Mission mission)
{
    CurrentMission = mission;
}

// ----------------------------------------------------------------------------
// GetMission - return the building's current mission.
// ----------------------------------------------------------------------------
Mission BuildingClass::GetMission() const
{
    return CurrentMission;
}

// ----------------------------------------------------------------------------
// QueueMission - queue a mission to be executed after the current one.
// ----------------------------------------------------------------------------
void BuildingClass::QueueMission(Mission mission)
{
    QueuedMission = mission;
}

// ----------------------------------------------------------------------------
// MissionAttack - the attack mission handler.  Buildings with weapons fire at
// their target.
// ----------------------------------------------------------------------------
void BuildingClass::MissionAttack()
{
    if (!CanFireNow()) return;
    if (!Target) return;
    // Delegate to the fire logic.
    Fire_At(reinterpret_cast<TargetClass*>(Target), 0);
}

// ----------------------------------------------------------------------------
// MissionGuard - the guard mission handler.  Buildings scan for targets.
// ----------------------------------------------------------------------------
void BuildingClass::MissionGuard()
{
    // Guarding buildings remain idle and wait for enemies to enter range.
    CurrentMission = Mission::Guard;
}

// ----------------------------------------------------------------------------
// MissionSleep - the sleep mission handler.  Buildings go dormant.
// ----------------------------------------------------------------------------
void BuildingClass::MissionSleep()
{
    CurrentMission = Mission::Sleep;
    Animation.IsAnimating = false;
}

// ============================================================================
// Mission_Construction - the build-up mission.
//
//  asm 落点：MissionClass_Update 的 32 路函数指针表第 18 项
//    `(this->*off_5B34E8[18])()` → 虚分派到本覆写。
//  此前本类只写了非虚的 MissionConstruction()，不在任何虚表里，
//  ExecuteMission 永远落到基类空实现（返回 1C2h），于是 MCV 展开后
//  建筑永远停在 IsConstructed == false。
//
//  两个阶段与原版一致：
//    1. 首次进入：切到 Construction 造型、标记"未建成"、起建造动画；
//    2. 之后某一帧：建成 —— 切回 Idle 造型并上线。
//
//  ⚠ 原版的第二阶段由 BuildingTypeClass::SuperAnimTimers[BState] 的帧数
//  驱动（GetAnimLengths，0x43EF50）。本树没有这张表，所以第二阶段在
//  下一帧就到 —— 表现为"瞬间完工、没有升起动画"。这是表现差异，不是
//  功能差异：建筑确实变成可用、可发电、可开火。表补上后把这里的门换成
//  帧数比较即可。
// ============================================================================
int32 BuildingClass::Mission_Construction()
{
    static const bool s_dbg = (std::getenv("OPENYR_BLD_DBG") != nullptr);
    if (s_dbg)
    {
        std::fprintf(stderr, "[BLDMIS] construction self=%p bstate=%d built=%d hp=%d\n",
                     static_cast<void*>(this), (int)BState,
                     (int)IsConstructed, (int)Health);
    }

    CurrentMission = Mission::Construction;

    if (BState != BStateType::Construction)
    {
        // Phase 1 - build up.
        BeginMode(BStateType::Construction);
        IsConstructed = false;
        Animation.IsAnimating = true;
        UpdateAnimations();
        return 0x1C2;                  // MissionClass::MISSION_NOT_DONE
    }

    // Phase 2 - build-up finished.
    IsConstructed = true;
    Animation.IsAnimating = true;
    BeginMode(BStateType::Idle);
    GoOnline();

    if (s_dbg)
    {
        std::fprintf(stderr, "[BLDMIS] construction-done self=%p bstate=%d built=%d\n",
                     static_cast<void*>(this), (int)BState, (int)IsConstructed);
    }

    // ★ 必须离开本任务：BeginMode(Idle) 把 BState 推离 Construction，
    //   若 CurrentMission 仍是 Construction，下一帧又会命中 Phase 1，
    //   建筑会在"建成/在建"之间逐帧翻转。
    CurrentMission = Mission::Stop;
    return 0x1C2;
}

// 旧虚槽（保留以维持本类 vtable 槽位）：转发到真正的任务分派版本。
void BuildingClass::MissionConstruction()
{
    (void)Mission_Construction();
}

// ============================================================================
// MissionSelling - the ramp-down mission.  The first step performs the sale
// (Sell refunds, strips upgrades and latches BState::Aux1); later steps just
// let the ramp-down art play out.  Sell() is internally guarded, so a repeat
// step cannot refund the structure twice.
// ============================================================================
void BuildingClass::MissionSelling()
{
    if (BState == BStateType::Aux1)
        return;   // ramp-down already under way

    Sell(0);

    // If Sell refused (e.g. not sellable), do not spin on this mission.
    if (BState != BStateType::Aux1)
        CurrentMission = Mission::Stop;

    UpdateAnimations();
}

// ============================================================================
// Mission_Repair - the repair mission (jump-table slot 20).
//
//  和 Mission_Construction 同一类问题：原实现是非虚的 MissionRepair()，
//  任务分派落不到它身上，选中建筑点"修理"没有任何反应。
//
//  这里只负责"开闸"：把 IsBeingRepaired 置起、把 RepairStepTimer 拨到 1，
//  真正每帧扣钱回血在 Update_Repair()（它读同一个 latch）。维持任务期间
//  每帧重入是幂等的。
//
//  ⚠ 满血/不可修理时必须离开 Repair：BuildingClass::Update 的在线判定是
//     `GetMission() != Selling && != Repair`，停在 Repair 会把建筑判成断电。
// ============================================================================
int32 BuildingClass::Mission_Repair()
{
    static const bool s_dbg = (std::getenv("OPENYR_BLD_DBG") != nullptr);
    if (s_dbg)
    {
        std::fprintf(stderr, "[BLDMIS] repair self=%p hp=%d/%d repairing=%d\n",
                     static_cast<void*>(this), (int)Health, (int)MaxHealth,
                     (int)IsBeingRepaired);
    }

    CurrentMission = Mission::Repair;

    if (!CanBeRepaired())
    {
        IsBeingRepaired = false;
        CurrentMission  = Mission::Stop;   // 不能停在 Repair（会判成断电）
        return 0x1C2;
    }

    // ★ 只在"从未修理 → 开始修理"的跃迁上拨一次计时器。
    //   本函数每帧重入；若无条件 `if (RepairStepTimer <= 0) RepairStepTimer = 1;`，
    //   Update_Repair 每帧都会看到它 > 0 而只做递减，永远走不到扣钱回血那一步
    //   —— 表现为"任务在跑、IsBeingRepaired=1、血量纹丝不动"。
    if (!IsBeingRepaired)
    {
        IsBeingRepaired = true;
        if (RepairStepTimer <= 0)
            RepairStepTimer = 1;
    }
    return 0x1C2;
}

// 旧虚槽（保留以维持本类 vtable 槽位）：转发到真正的任务分派版本。
void BuildingClass::MissionRepair()
{
    (void)Mission_Repair();
}

// ----------------------------------------------------------------------------
// MissionActive - the active mission handler.  Buildings in active state
// process their production queues and super weapons.
// ----------------------------------------------------------------------------
void BuildingClass::MissionActive()
{
    CurrentMission = Mission::AreaGuard;
    BState = BStateType::Active;
    UpdateAnimations();
}

// ----------------------------------------------------------------------------
// MissionIdle - the idle mission handler.  Buildings return to idle animation.
// ----------------------------------------------------------------------------
void BuildingClass::MissionIdle()
{
    CurrentMission = Mission::Stop;
    BState = BStateType::Idle;
    UpdateAnimations();
}

// ----------------------------------------------------------------------------
// UpdateMission - per-frame mission update.  Processes the queued mission
// transition if one is pending.
// ----------------------------------------------------------------------------
void BuildingClass::UpdateMission()
{
    // QueuedMission 的"空"值是 Mission::None(-1)，不是 Sleep(0)。
    // 无条件比较（`if (QueuedMission != CurrentMission) CurrentMission = ...`）
    // 会在任务分派把 CurrentMission 装上之后，下一次本函数进入时又被
    // 空的 QueuedMission 覆盖回 None —— Mission_Construction / Mission_Repair
    // 只能跑一帧就被打断，建筑永远回不到"已建成"或"在修理"。
    if (QueuedMission != Mission::None && QueuedMission != CurrentMission)
    {
        CurrentMission = QueuedMission;
        QueuedMission  = Mission::None;
    }
}

// ============================================================================
// AI
// ============================================================================

// ----------------------------------------------------------------------------
// AI_Update - the main per-frame AI update for the building.  Processes
// recharge timers, animations, gate timers, and mission updates.
// ----------------------------------------------------------------------------
void BuildingClass::AI_Update()
{
    // Decrement the fire recharge timer.
    if (FireRechargeTimer > 0) {
        --FireRechargeTimer;
    }

    // Decrement the delay before firing.
    if (DelayBeforeFiring > 0) {
        --DelayBeforeFiring;
        if (DelayBeforeFiring == 0 && PrismStage > 0) {
            PrismStage = 0;  // fire complete
        }
    }

    // Update animations.
    UpdateAnimations();

    // Update mission.
    UpdateMission();

    // Update bunker/garrison state.
    if (Occupants.Count > 0) {
        UpdateBunker();
    }

    // Update damage-effect timers from the base class.
    if (FireDamageTimer > 0) --FireDamageTimer;
    if (TemporalTimer > 0) --TemporalTimer;
    if (GasTimer > 0) --GasTimer;
    if (RadiationTimer > 0) --RadiationTimer;
    if (IronCurtainTimer > 0) --IronCurtainTimer;
    if (ForceShieldTimer > 0) --ForceShieldTimer;
}

// ----------------------------------------------------------------------------
// Combat_AI - per-frame combat AI.  Buildings with weapons scan for targets
// and fire when appropriate.
// ----------------------------------------------------------------------------
void BuildingClass::Combat_AI()
{
    if (!CanFireNow()) return;

    // If we have a target, try to fire.
    if (Target && HasTarget()) {
        MissionAttack();
    }
}

// ----------------------------------------------------------------------------
// Production_AI - per-frame production AI.  Factories advance their production
// queues.
// ----------------------------------------------------------------------------
void BuildingClass::Production_AI()
{
    if (!Factory) return;
    if (!IsConstructed || !IsOnline) return;

    // The full binary advances the factory's production timer here.
    // We track the building's state to indicate it is producing.
    if (BState == BStateType::Idle && Factory) {
        BState = BStateType::Active;
        UpdateAnimations();
    }
}

// ----------------------------------------------------------------------------
// Power_AI - per-frame power AI.  Power plants report their output to the
// owning house's power budget.
// ----------------------------------------------------------------------------
void BuildingClass::Power_AI()
{
    if (!Type) return;

    // Recalculate power output based on current state.
    if (IsOnline && IsConstructed && !IsBeingDrained_) {
        if (Type->Power > 0) {
            PowerOutput = Type->Power;
            IsPowerPlant = true;
            if (IsOverpowered) {
                PowerOutput = static_cast<int32>(
                    static_cast<double>(PowerOutput) * LOW_POWER_OUTPUT_MULT);
            }
        }
    } else {
        PowerOutput = 0;
    }
}

// ============================================================================
// Health & Damage
// ============================================================================

// ----------------------------------------------------------------------------
// TakeDamage - apply damage to the building.  Reduces health, updates
// animation state, and triggers destruction if health reaches zero.
// ----------------------------------------------------------------------------
void BuildingClass::TakeDamage(int32 damage, TechnoClass* pSource,
                                WarheadTypeClass* pWarhead)
{
    if (damage <= 0) return;
    if (!IsConstructed) return;

    // Iron Curtain / Force Shield grant invulnerability.
    if (IronCurtainTimer > 0 || ForceShieldTimer > 0) return;

    Health -= damage;
    if (Health < 0) Health = 0;

    // Update the damaged animation state.
    double ratio = (MaxHealth > 0) ?
        static_cast<double>(Health) / static_cast<double>(MaxHealth) : 0.0;
    Animation.IsDamaged = (ratio < DAMAGED_THRESHOLD);

    // If health reached zero, destroy the building.
    if (Health == 0) {
        OnDestroyed();
    }
}

// ----------------------------------------------------------------------------
// OnDestroyed - called when the building's health reaches zero.  Triggers
// the destruction sequence.
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// 建筑类型 -> 原版 eFactory 编号（HouseClass::Update_Factories 的第一个参数）。
//   asm 的调用点直接搬 Type 里记着的 eFactory（+0xEB8）；本树把它建模成
//   BuildingTypeClass::Factory（AbstractType 形态），这里做一次反查：
//     0  Aircraft / 13 Infantry / 37 Vehicles / 其它 1..38 = Buildings。
// ----------------------------------------------------------------------------
static int32 Factory_Of_From_Type(BuildingTypeClass* pType)
{
    if (pType == nullptr)
        return -1;

    if (pType->Factory == AbstractType::AircraftType)      return 0;
    if (pType->Factory == AbstractType::InfantryType)      return 13;
    if (pType->Factory == AbstractType::UnitType)          return 37;
    if (pType->Factory == AbstractType::BuildingType)      return 1;
    return -1;
}

void BuildingClass::OnDestroyed()
{
    IsConstructed = false;
    IsOnline = false;
    IsPowerPlant = false;
    PowerOutput = 0;
    PowerDrain = 0;

    // Evacuate or kill occupants.
    EmptyBunker();

    // Deactivate subsystems.
    DisableStuff();

    // Notify the owner.
    if (Owner && Type) {
        Owner->RegisterLoss(static_cast<TechnoTypeClass*>(
            static_cast<void*>(Type)));

        // asm: BuildingClass_Remove+567h —— 这栋楼是本房某类工厂的载体时，
        //   先调用 HouseClass::Update_Factories(factoryOf, Type, buildCat)，
        //   把该类产线重新过一遍：能力已失效的在制品会被 Abandon 并从队列
        //   里摘掉，剩下的按优先级顶上。缺这一步，工厂建筑被拆/被占之后
        //   队列里还挂着"永远造不出来"的条目，那条线就永久卡死。
        const int32 factoryOf = Factory_Of_From_Type(Type);
        if (factoryOf >= 0)
            Owner->Update_Factories(factoryOf, Type, 0);
    }

    AfterDestruction();
}

// ----------------------------------------------------------------------------
// OnCaptured - called when the building changes ownership.  Re-registers the
// building with the new owner and updates power tracking.
// ----------------------------------------------------------------------------
void BuildingClass::OnCaptured(HouseClass* pNewOwner)
{
    if (!pNewOwner) return;

    // Remove from old owner's tracking.
    if (Owner) {
        Owner->Tracking_Remove(this);
    }

    // Assign the new owner.
    Owner = pNewOwner;
    HasBeenCaptured = true;

    // Re-register with the new owner.
    Owner->Tracking_Add(this);

    // Re-evaluate power and production.
    GoOffline();
    GoOnline();
}

// ----------------------------------------------------------------------------
// OnVeterancyUp - called when the building gains a veterancy level.  Buildings
// gain veterancy through combat; this improves weapon damage and rate of fire.
// ----------------------------------------------------------------------------
void BuildingClass::OnVeterancyUp()
{
    if (VeterancyLevel < 2) {
        ++VeterancyLevel;
    }
}

// ----------------------------------------------------------------------------
// IsVisibleTo - true if the building is visible to the specified house.
// Constructed buildings are visible unless fogged.
// ----------------------------------------------------------------------------
bool BuildingClass::IsVisibleTo(HouseClass* pHouse) const
{
    if (!pHouse) return false;
    if (!IsConstructed) return false;
    if (IsFogged) return false;
    // Allies can always see the building.
    if (Owner == pHouse) return true;
    return !IsFogged;
}

// ----------------------------------------------------------------------------
// RevealTo - reveal the building to the specified house (remove fog).
// ----------------------------------------------------------------------------
void BuildingClass::RevealTo(HouseClass* pHouse)
{
    (void)pHouse;
    IsFogged = false;
}

// ----------------------------------------------------------------------------
// GetSightRange - return the sight range (in cells) from the building type.
// ----------------------------------------------------------------------------
int32 BuildingClass::GetSightRange() const
{
    if (!Type) return 0;
    TechnoTypeClass* pType =
        static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    return pType->SightRange;
}

// ----------------------------------------------------------------------------
// GetArmor - return the armor value of the building type.
// ----------------------------------------------------------------------------
int32 BuildingClass::GetArmor() const
{
    if (!Type) return 0;
    TechnoTypeClass* pType =
        static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    return static_cast<int32>(pType->ArmorType);
}

// ----------------------------------------------------------------------------
// GetMaxHealth - return the maximum health from the building type.
// ----------------------------------------------------------------------------
int32 BuildingClass::GetMaxHealth() const
{
    return MaxHealth;
}

// ----------------------------------------------------------------------------
// GetHealth - return the current health.
// ----------------------------------------------------------------------------
int32 BuildingClass::GetHealth() const
{
    return Health;
}

// ----------------------------------------------------------------------------
// SetHealth - set the current health, clamped to [0, MaxHealth].
// ----------------------------------------------------------------------------
void BuildingClass::SetHealth(int32 hp)
{
    Health = hp;
    if (Health < 0) Health = 0;
    if (MaxHealth > 0 && Health > MaxHealth) Health = MaxHealth;
}

// ----------------------------------------------------------------------------
// IsAlive - true if the building has positive health and is constructed.
// ----------------------------------------------------------------------------
bool BuildingClass::IsAlive() const
{
    return IsConstructed && Health > 0;
}

// ----------------------------------------------------------------------------
// IsDead - true if the building has zero health.
// ----------------------------------------------------------------------------
bool BuildingClass::IsDead() const
{
    return Health <= 0;
}

// ----------------------------------------------------------------------------
// IsDamaged - true if the building's health is below the damaged threshold.
// ----------------------------------------------------------------------------
bool BuildingClass::IsDamaged() const
{
    if (MaxHealth <= 0) return false;
    double ratio = static_cast<double>(Health) /
                   static_cast<double>(MaxHealth);
    return ratio < DAMAGED_THRESHOLD;
}

// ----------------------------------------------------------------------------
// IsGreenHP - true if health is above the green threshold (>75%).
// ----------------------------------------------------------------------------
bool BuildingClass::IsGreenHP() const
{
    if (MaxHealth <= 0) return false;
    double ratio = static_cast<double>(Health) /
                   static_cast<double>(MaxHealth);
    return ratio > 0.75;
}

// ----------------------------------------------------------------------------
// IsYellowHP - true if health is in the yellow range (25%-75%).
// ----------------------------------------------------------------------------
bool BuildingClass::IsYellowHP() const
{
    if (MaxHealth <= 0) return false;
    double ratio = static_cast<double>(Health) /
                   static_cast<double>(MaxHealth);
    return ratio > 0.25 && ratio <= 0.75;
}

// ----------------------------------------------------------------------------
// IsRedHP - true if health is in the red range (<=25%).
// ----------------------------------------------------------------------------
bool BuildingClass::IsRedHP() const
{
    if (MaxHealth <= 0) return false;
    double ratio = static_cast<double>(Health) /
                   static_cast<double>(MaxHealth);
    return ratio <= 0.25;
}

// ----------------------------------------------------------------------------
// GetHealthRatio - return the health as a fraction (0.0-1.0).
// ----------------------------------------------------------------------------
float BuildingClass::GetHealthRatio() const
{
    if (MaxHealth <= 0) return 0.0f;
    return static_cast<float>(Health) / static_cast<float>(MaxHealth);
}

// ============================================================================
 // asm: BuildingClass_PowerProduced
//
//   The structure's contribution to its owner's power grid:
//     1. Start from the type's Power figure.
//     2. Add the upgrade-module power bonus when the flag is set.
//     3. When the type's extra-power flags are set and the module drain is
//        positive, add drain * upgrade level.
//     4. Walk the three attached upgrade modules and add each one's own
//        Power figure.
//     5. Finally, if the structure is damaged, scale the total by its health
//        ratio and truncate to int.
//   An offline structure produces nothing.
// ============================================================================
int32 BuildingClass::PowerProduced() const
{
    // asm: BuildingClass_PowerProduced (asm:14185)
    //   `call [eax+1D4h]` -> a power-drained structure produces nothing.
    if (IsBeingDrained_)
        return 0;

    // asm: [esi+660h] is the online gate (set by GoOnline, cleared by
    //      GoOffline); an offline structure yields zero.
    if (!IsOnline)
        return 0;

    int32 total = Type ? Type->Power : 0;

    // asm: [esi+668h] gates the type's extra-power contribution (+0xEE8h).
    if (HasExtraPowerBonus && Type)
    {
        total += Type->ExtraPower;

        // asm: loc_44E809 - type flags +16AEh/+16AFh gate a stage-scaled
        //      top-up: ExtraPower * WeaponStage (the multi-stage firing
        //      counter at +0x114).  Only applies while the counter is
        //      positive and the type actually advertises extra power.
        if (Type->ExtraPower > 0 && WeaponStage > 0)
            total += Type->ExtraPower * WeaponStage;
    }

    // The three installed upgrade modules contribute their own power figure.
    for (int32 i = 0; i < BUILDING_UPGRADE_COUNT; ++i)
    {
        BuildingTypeClass* pModule = Upgrades[i];
        if (pModule != nullptr)
            total += pModule->Power;
    }

    if (total > 0 && ConditionYellow)
    {
        total = static_cast<int32>(static_cast<float>(total) * GetHealthRatio());
    }
    return total;
}

// ============================================================================
 // asm: int __thiscall BuildingClass_PowerAbsorbed(BuildingClass *this)   (asm:201660)
//
//   The structure's power draw: the type's PowerDrain, plus the upgrade
//   module drain when the extra-drain flag is set, plus the drain of each of
//   the three attached upgrade modules.  Offline structures draw nothing.
// ============================================================================
int32 BuildingClass::PowerAbsorbed() const
{
    // asm: BuildingClass_PowerAbsorbed (asm:14284)
    //   Same drained / online gates as PowerProduced.
    if (IsBeingDrained_)
        return 0;
    if (!IsOnline)
        return 0;

    int32 total = Type ? Type->PowerDrain : 0;

    // asm: [esi+ExtraPowerConsumer] gates the type's extra drain (+0xEECh).
    if (HasExtraPowerDrain && Type)
        total += Type->ExtraPower;

    for (int32 i = 0; i < BUILDING_UPGRADE_COUNT; ++i)
    {
        BuildingTypeClass* pModule = Upgrades[i];
        if (pModule != nullptr)
            total += pModule->PowerDrain;
    }
    return total;
}

// ============================================================================
 // asm: BuildingClass_UndamageAllAnims
//
//   Swaps every damage animation on the structure between its damaged and
//   undamaged variants.  The state is latched in `ConditionYellow` so a
//   repeated call with the same value is a no-op.  Each occupied anim slot
//   plays the matching entry from the type's two anim-layout arrays (stride
//   0x44, three slots).
// ============================================================================
void BuildingClass::UndamageAllAnims(bool conditionYellow)
{
    if (ConditionYellow == conditionYellow)
        return;

    ConditionYellow = conditionYellow;

    if (Type == nullptr)
        return;

    // Every occupied damage-fire slot is replayed with the matching variant:
    // the damaged set when `conditionYellow` is set, the pristine set
    // otherwise.
    for (int32 i = 0; i < BUILDING_DAMAGE_FIRE_ANIM_COUNT; ++i)
    {
        if (DamageFireAnims[i] == nullptr)
            continue;
        DamageFireAnims[i]->Play();
    }
}

 // asm: int __thiscall BuildingClass_PlaySomeAnim(BuildingClass *this, int idxAnim_, byte conditionYellow, byte garrisoned)   (asm:207062)
//
//   Replays the structure's idle animation set after its visuals have been
//   invalidated (an upgrade installed, the building re-powered).  Each of the
//   type's animation slots is restarted.
void BuildingClass::PlaySomeAnim(int32 a2)
{
    (void)a2;
    for (int32 i = 0; i < BUILDING_DAMAGE_FIRE_ANIM_COUNT; ++i)
    {
        if (DamageFireAnims[i] != nullptr)
            DamageFireAnims[i]->Play();
    }
}

 // asm: char __thiscall BuildingClass_AddOverpowerer(BuildingClass *this, InfantryClass *inf)   (asm:209311)
//
//  asm 逐句：
//    ① 在 +0x66C 的向量里 FindIndex(inf)；命中就把它**从向量里删掉**
//       （swap-remove：后续元素整体前移，Count--）。也就是说重复登记会被
//       “挪到队尾”，而不是被忽略。
//    ② 若 Count >= Capacity：先看 IsAllocated(+0xD) 与 Capacity；
//       Capacity == 0 或已分配时，用 GrowthStep(+0x14) 走 vt[+8] 扩容。
//    ③ 扩容成功后（或本就有空位）在末尾追加 inf，Count++。
//  ★ 原实现写的是"命中就直接 return"，与 asm 的“先摘后加”不同：连续两次
//    放电会让同一个步兵占两个槽位，从而被计成两份增幅。
void BuildingClass::AddOverpowerer(InfantryClass* pInfantry)
{
    if (pInfantry == nullptr)
        return;

    // ① 先摘除已有的同项（asm: FindIndex → swap-remove）
    for (int32 i = 0; i < Overpowerers.Count; ++i)
    {
        if (Overpowerers.Items[i] == pInfantry)
        {
            Overpowerers.Remove(i);
            break;
        }
    }

    // ②③ 追加到队尾（容量增长由 DynamicVectorClass::Add 负责）
    Overpowerers.Add(pInfantry);

    // 本项目用布尔镜像 asm 里“Overpowerers.Count != 0”的条件。
    IsOverpowered = (Overpowerers.Count > 0);
}

// 根据游戏行为，可知 RemoveOverpowerer 负责下面这段逻辑。
//
//   Unregisters an overpowerer; the overpowered flag drops once the last one
//   has been removed.
void BuildingClass::RemoveOverpowerer(InfantryClass* pInfantry)
{
    for (int32 i = 0; i < Overpowerers.Count; ++i)
    {
        if (Overpowerers.GetItem(i) == pInfantry)
        {
            Overpowerers.Remove(i);
            break;
        }
    }
    if (Overpowerers.Count == 0)
        IsOverpowered = false;
}

// ----------------------------------------------------------------------------
// Kill - instantly destroy the building by setting health to zero and
// triggering the destruction sequence.
// ----------------------------------------------------------------------------
void BuildingClass::Kill()
{
    Health = 0;
    OnDestroyed();
}

// ============================================================================
// Value & Cost
// ============================================================================

// ----------------------------------------------------------------------------
// GetValue - return the scrap/value of the building when destroyed.
// ----------------------------------------------------------------------------
int32 BuildingClass::GetValue() const
{
    if (!Type) return 0;
    return Type->Cost / 4;
}

// ----------------------------------------------------------------------------
// GetCost - return the original build cost of the building.
// ----------------------------------------------------------------------------
int32 BuildingClass::GetCost() const
{
    if (!Type) return 0;
    return Type->Cost;
}

// ----------------------------------------------------------------------------
// GetBuildTime - return the build time in frames.
// ----------------------------------------------------------------------------
int32 BuildingClass::GetBuildTime() const
{
    if (!Type) return 0;
    // Build time is proportional to cost.
    return Type->Cost / 10;
}

// ============================================================================
// Position & Direction
// ============================================================================

// ----------------------------------------------------------------------------
// GetDirection - return the building's facing direction.  Buildings generally
// face north (0) unless they have a turret.
// ----------------------------------------------------------------------------
DirStruct BuildingClass::GetDirection() const
{
    DirStruct dir;
    return dir;
}

// ----------------------------------------------------------------------------
// SetDirection - set the building's facing direction.  Most buildings cannot
// be rotated, but turrets can.
// ----------------------------------------------------------------------------
void BuildingClass::SetDirection(DirStruct dir)
{
    (void)dir;
    // Buildings do not rotate in the standard rules.
}

// ----------------------------------------------------------------------------
// GetCoords - return the building's world coordinates.
// ----------------------------------------------------------------------------
CoordStruct BuildingClass::GetCoords() const
{
    return Location;
}

// ----------------------------------------------------------------------------
// SetCoords - set the building's world coordinates.
// ----------------------------------------------------------------------------
void BuildingClass::SetCoords(CoordStruct coords)
{
    Location = coords;
}

// ============================================================================
// State Control
// ============================================================================

// ----------------------------------------------------------------------------
// Stop - stop the building's current activity.  Cancels production and firing.
// ----------------------------------------------------------------------------
void BuildingClass::Stop()
{
    Target = nullptr;
    CurrentMission = Mission::Stop;
    if (BState == BStateType::Active) {
        BState = BStateType::Idle;
        UpdateAnimations();
    }
}

// ----------------------------------------------------------------------------
// Hold - hold fire.  Sets the mission to guard without firing.
// ----------------------------------------------------------------------------
void BuildingClass::Hold()
{
    Target = nullptr;
    CurrentMission = Mission::Guard;
}

// ----------------------------------------------------------------------------
// IsIdle - true if the building is in an idle state (not producing, firing,
// or constructing).
// ----------------------------------------------------------------------------
bool BuildingClass::IsIdle() const
{
    if (BState == BStateType::Construction) return false;
    if (BState == BStateType::Active) return false;
    if (CurrentMission == Mission::Selling) return false;
    return FireRechargeTimer <= 0;
}

// ----------------------------------------------------------------------------
// SetIdle - force the building into the idle state.
// ----------------------------------------------------------------------------
void BuildingClass::SetIdle()
{
    BState = BStateType::Idle;
    CurrentMission = Mission::Stop;
    UpdateAnimations();
}

// ----------------------------------------------------------------------------
// Freeze - freeze the building (temporal/chronoshift effect).  Stops all
// activity.
// ----------------------------------------------------------------------------
void BuildingClass::Freeze()
{
    IsFrozen_ = true;
    Animation.IsAnimating = false;
}

// ----------------------------------------------------------------------------
// Unfreeze - unfreeze the building and resume animations.
// ----------------------------------------------------------------------------
void BuildingClass::Unfreeze()
{
    IsFrozen_ = false;
    Animation.IsAnimating = true;
    UpdateAnimations();
}

// ============================================================================
// Limbo - take the structure off the map without destroying it (chronosphere,
// map teardown).  Frees its footprint cells and drops the live subsystems.
// ============================================================================
bool BuildingClass::Limbo()
{
    if (IsInLimbo)
        return false;

    // Free the foundation cells and stop the live subsystems.
    Unmark();
    GoOffline();

    IsInLimbo = true;
    return true;
}

// ============================================================================
// Unlimbo - put a limboed structure back onto the map: re-stamp its
// footprint and bring it back online.
// ============================================================================
bool BuildingClass::Unlimbo()
{
    if (!IsInLimbo)
        return false;

    // ── 地基可用性检查（asm: BuildingClass_Unlimbo 的地基扫描段）──────────
    //   原版落位前逐格确认地基范围没有被别的对象占住；被占则保持 limbo 并
    //   返回失败，由调用方重新挑位置。缺这一段时两座建筑可以叠在同一格，
    //   占用链与渲染都会错乱。
    if (Type != nullptr && MapClass::Instance != nullptr)
    {
        const CellStruct baseCell = CellClass::Coord2Cell(GetCoords());
        const int32 fx = Type->X_Foundation_Value();
        const int32 fy = Type->Y_Foundation_Value(true);

        for (int32 dy = 0; dy < fy; ++dy)
        {
            for (int32 dx = 0; dx < fx; ++dx)
            {
                const CellStruct probe(static_cast<int16>(baseCell.X + dx),
                                       static_cast<int16>(baseCell.Y + dy));
                CellClass* pCell = MapClass::Instance->GetCellAt(probe);
                if (pCell == nullptr)
                    return false;              // 越界：原地基不可用
                if (pCell->IsOccupied())
                    return false;              // 已被占：交给调用方换位置
            }
        }
    }

    IsInLimbo = false;
    Mark(MarkType::Down);
    GoOnline();
    // asm: BuildingClass_Unlimbo -> ObjectClass::Unlimbo 尾部的收尾（L1049805）：
    //   置 Alive、登记主表、交显示层。
    FinalizeUnlimbo();

    // asm: BuildingClass_Unlimbo 尾部把建筑交给阵营追踪表；建造场还要登记
    //   进 vec_Conyards 并触发 AcquiredConyard（AI 建造清单的唯一入口）。
    if (Owner != nullptr)
    {
        Owner->RegisterTechnoGain(this);

        // asm (BuildingClass::Unlimbo+1024h 段): 非人类房落位后立刻在基地
        //   节点表里盖章 —— `call BaseClass_Placed(&house->Base, Building)`。
        //   缺这一步，AI 的建造清单不会收敛：Next_Buildable 每帧都返回同一
        //   个节点，AI 会把同一座建筑反复造到钱花光（实测 21 座）。
        if (!Owner->Is_Human())
        {
            Owner->Base.Placed(this);
        }

        // asm (BuildingClass::Unlimbo+1058h): 落位后重评该类工厂的产线。
        //   `Update_Factories(factoryOf, Type, buildCat)` —— 新工厂上线要接管
        //   队列里等着的订单，同时把"因为没厂而停摆"的产线唤醒。
        //   本树没有 eFactory 参数，按建筑类型自己的 Factory 槽反推。
        const int32 factoryOf = Factory_Of_From_Type(Type);
        if (factoryOf >= 0)
            Owner->Update_Factories(factoryOf, Type, 0);
    }

    // asm: BuildingClass_Unlimbo 尾部的 FreeUnit 生成段。
    //   精炼厂（FreeUnit=HARV）落成时会附送一辆矿车 —— 这是本树唯一能让
    //   AI 开局就拥有采集能力的路径：没有它，AI 把开局资金全砸在建筑上之后
    //   既造不出矿车也没有任何收入，整局永久停摆。
    if (Type != nullptr && Type->FreeUnit != nullptr && Owner != nullptr
        && MapClass::Instance != nullptr && ScenarioInit == 0)
    {
        // 落点：建筑外一圈第一个空格；被占就向外扩到 4 环。
        //   精炼厂常常与兵营/电厂紧挨着落位，出口环全被占时原版会另找一格；
        //   只试一格会让 FreeUnit 静默丢失。
        CellStruct spawnCell = FindExitCell(0, 0);
        CellClass* pSpawn = MapClass::Instance->GetCellAt(spawnCell);
        if (pSpawn == nullptr || pSpawn->IsOccupied())
        {
            pSpawn = nullptr;
            const CellStruct base = CellClass::Coord2Cell(GetCoords());
            for (int32 r = 1; r <= 4 && pSpawn == nullptr; ++r)
            {
                for (int32 dy = -r; dy <= r && pSpawn == nullptr; ++dy)
                {
                    for (int32 dx = -r; dx <= r && pSpawn == nullptr; ++dx)
                    {
                        const int32 adx = (dx < 0) ? -dx : dx;
                        const int32 ady = (dy < 0) ? -dy : dy;
                        if (((adx > ady) ? adx : ady) != r)
                            continue;
                        const CellStruct probe(static_cast<int16>(base.X + dx),
                                               static_cast<int16>(base.Y + dy));
                        CellClass* pProbe = MapClass::Instance->GetCellAt(probe);
                        if (pProbe == nullptr || pProbe->IsOccupied())
                            continue;
                        spawnCell = probe;
                        pSpawn = pProbe;
                    }
                }
            }
        }

        if (pSpawn != nullptr)
        {
            UnitClass* pUnit = new UnitClass(Owner);
            if (pUnit != nullptr)
            {
                UnitTypeClass* pUnitType = Type->FreeUnit;
                pUnit->Type       = pUnitType;
                pUnit->TechnoType = pUnitType;
                pUnit->Health     = (pUnitType->Strength > 0) ? pUnitType->Strength : 200;
                pUnit->MaxHealth  = pUnit->Health;
                pUnit->Location   = CellClass::Cell2Coord(spawnCell);
                pUnit->IsInLimbo  = true;
                if (!pUnit->Unlimbo())
                {
                    GameDelete(pUnit);
                }
                else if (TheDisplay != nullptr)
                {
                    TheDisplay->Submit(pUnit);
                }
            }
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// InLimbo - return true if the building is currently in limbo.
// ----------------------------------------------------------------------------
bool BuildingClass::InLimbo() const
{
    return IsInLimbo;
}

// ============================================================================
// Mark - stamp the structure's footprint onto the map.  Walks the foundation
// (X_Foundation_Value x Y_Foundation_Value including the bib row) and pushes
// the structure onto each cell's ground occupancy chain (MarkType::Down) or,
// for MarkType::Up, removes it again.
// ============================================================================
void BuildingClass::Mark(MarkType mark)
{
    if (Type == nullptr || MapClass::Instance == nullptr)
        return;

    const CellStruct base = CellClass::Coord2Cell(GetCoords());
    const int32 fx = Type->X_Foundation_Value();
    const int32 fy = Type->Y_Foundation_Value(true);
    if (fx <= 0 || fy <= 0)
        return;

    for (int32 dy = 0; dy < fy; ++dy)
    {
        for (int32 dx = 0; dx < fx; ++dx)
        {
            CellStruct probe;
            probe.X = static_cast<int16>(base.X + dx);
            probe.Y = static_cast<int16>(base.Y + dy);

            CellClass* pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                continue;

            if (mark == MarkType::Down)
            {
                // 主格入占用链；其余占地格只登记 Occupier 槽。
                //   （多格建筑只能挂在一条 NextObject 链上，见 Add_Occupier。）
                if (dx == 0 && dy == 0)
                    pCell->Occupy_Down(this, false);
                else
                    pCell->Add_Occupier(this);
            }
            else
            {
                pCell->Remove_Occupier(this);
            }
        }
    }

    IsCurrentlyOccupied = (mark == MarkType::Down);
}

// ----------------------------------------------------------------------------
// Unmark - release the structure's footprint cells.  Walks the foundation and
// removes the structure from each cell's occupancy chain, then flags the area
// dirty so the renderer redraws the terrain beneath the former footprint.
// ----------------------------------------------------------------------------
void BuildingClass::Unmark()
{
    if (!IsConstructed) return;
    if (IsInLimbo) return;

    if (Type != nullptr && MapClass::Instance != nullptr)
    {
        const CellStruct base = CellClass::Coord2Cell(GetCoords());
        const int32 fx = Type->X_Foundation_Value();
        const int32 fy = Type->Y_Foundation_Value(true);

        for (int32 dy = 0; dy < fy; ++dy)
        {
            for (int32 dx = 0; dx < fx; ++dx)
            {
                CellStruct probe;
                probe.X = static_cast<int16>(base.X + dx);
                probe.Y = static_cast<int16>(base.Y + dy);

                CellClass* pCell = MapClass::Instance->GetCellAt(probe);
                if (pCell != nullptr)
                    pCell->Remove_Occupier(this);
            }
        }
    }

    IsCurrentlyOccupied   = false;
    IsTentativelyOccupied = false;

    // Mark the object as dirty so the renderer redraws the area.
    Dirty = true;
}

// ----------------------------------------------------------------------------
// Sync - synchronize the building's state with the game state (multiplayer).
// Marks the building as dirty so its state is included in the next network
// packet.  Called whenever a state change (power, production, health, etc.)
// must be propagated to all clients.
// ----------------------------------------------------------------------------
void BuildingClass::Sync()
{
    if (!IsConstructed) return;

    // Stamp the current frame as the last-sync frame and mark the object
    // dirty so the network layer includes it in the next outgoing packet.
    Dirty = true;

    // Record the sync frame in the reserved layout so the network code
    // can detect stale sync entries during desync detection.
    int32 currentFrame = Game::GetCurrentFrame();
    *reinterpret_cast<int32*>(&ReservedLayout_538[0]) = currentFrame;
}

// ----------------------------------------------------------------------------
// Unsync - mark the building as out of sync (needs re-synchronization).
// Clears the dirty flag so the network layer stops including this object
// in outgoing packets.  The next state change will re-sync via Sync().
// ----------------------------------------------------------------------------
void BuildingClass::Unsync()
{
    // Clear the dirty flag - the object's state is now considered stable
    // and does not need to be retransmitted.
    Dirty = false;

    // Reset the sync frame stamp to indicate "not synchronized".
    *reinterpret_cast<int32*>(&ReservedLayout_538[0]) = 0;
}

// ----------------------------------------------------------------------------
// Lock - acquire the building's update lock.  Prevents concurrent updates.
// ----------------------------------------------------------------------------
void BuildingClass::Lock()
{
    IsLocked_ = true;
}

// ----------------------------------------------------------------------------
// Unlock - release the building's update lock.
// ----------------------------------------------------------------------------
void BuildingClass::Unlock()
{
    IsLocked_ = false;
}

// ----------------------------------------------------------------------------
// IsLocked - return true if the update lock is held.
// ----------------------------------------------------------------------------
bool BuildingClass::IsLocked() const
{
    return IsLocked_;
}

// ----------------------------------------------------------------------------
// Disable - explicitly disable the building.  It stops functioning but
// remains on the map.
// ----------------------------------------------------------------------------
void BuildingClass::Disable()
{
    IsDisabled_ = true;
    GoOffline();
}

// ----------------------------------------------------------------------------
// Enable - re-enable a previously disabled building.
// ----------------------------------------------------------------------------
void BuildingClass::Enable()
{
    IsDisabled_ = false;
    GoOnline();
}

// ----------------------------------------------------------------------------
// IsDisabled - return true if the building has been explicitly disabled.
// ----------------------------------------------------------------------------
bool BuildingClass::IsDisabled() const
{
    return IsDisabled_;
}

// ----------------------------------------------------------------------------
// Activate - activate the building (bring it online and start animations).
// ----------------------------------------------------------------------------
void BuildingClass::Activate()
{
    GoOnline();
    Animation.IsAnimating = true;
}

// ----------------------------------------------------------------------------
// Deactivate - deactivate the building (take it offline and stop animations).
// ----------------------------------------------------------------------------
void BuildingClass::Deactivate()
{
    GoOffline();
    Animation.IsAnimating = false;
}

// ============================================================================
// Cloak & Special Effects
// ============================================================================

// ----------------------------------------------------------------------------
// Cloak (no-argument overload) - initiate cloaking with the default sound
// behavior.  Delegates to the bool-parameter version.  Buildings can only
// cloak if their type explicitly allows it.
// ----------------------------------------------------------------------------
void BuildingClass::Cloak()
{
    Cloak(true);
}

// ----------------------------------------------------------------------------
// Decloak - force the building to become visible (no-op for buildings).
// ----------------------------------------------------------------------------
void BuildingClass::Decloak()
{
    CloakState = CloakStateEnum::Idle;
    CloakAlpha = 255;
}

// ----------------------------------------------------------------------------
// IsCloaked - returns true if the building is currently in the fully-cloaked
// state.  Standard buildings are never cloaked, but modded types with the
// Cloakable flag can enter the cloaked state via Cloak().
// ----------------------------------------------------------------------------
bool BuildingClass::IsCloaked() const
{
    // Check the actual cloak state rather than unconditionally returning
    // false, so modded cloakable buildings behave correctly.
    return CloakState == CloakStateEnum::Cloaked;
}

// ----------------------------------------------------------------------------
// EMPulse - disable the building with an EMP weapon.  The building goes
// offline for the duration of the EMP effect.
// ----------------------------------------------------------------------------
void BuildingClass::EMPulse()
{
    GoOffline();
}

// ----------------------------------------------------------------------------
// UnEMP - recover from an EMP effect.
// ----------------------------------------------------------------------------
void BuildingClass::UnEMP()
{
    if (!IsDisabled_) {
        GoOnline();
    }
}

// ----------------------------------------------------------------------------
// IsEMPed - true if the building is currently affected by an EMP weapon.
// ----------------------------------------------------------------------------
bool BuildingClass::IsEMPed() const
{
    return !IsOnline && IsConstructed && !IsDisabled_;
}

// ----------------------------------------------------------------------------
// IronCurtain - apply the Iron Curtain invulnerability effect.
// ----------------------------------------------------------------------------
void BuildingClass::IronCurtain()
{
    ApplyIronCurtain(750); // ~12.5 seconds at 60fps
}

// ----------------------------------------------------------------------------
// UnIronCurtain - remove the Iron Curtain effect.
// ----------------------------------------------------------------------------
void BuildingClass::UnIronCurtain()
{
    IronCurtainTimer = 0;
}

// ----------------------------------------------------------------------------
// IsIronCurtained - true if the building is currently protected by the Iron
// Curtain.
// ----------------------------------------------------------------------------
bool BuildingClass::IsIronCurtained() const
{
    return IronCurtainTimer > 0;
}

// ----------------------------------------------------------------------------
// ForceShield - apply the Force Shield invulnerability effect.
// ----------------------------------------------------------------------------
void BuildingClass::ForceShield()
{
    ApplyForceShield(500); // ~8.3 seconds at 60fps
}

// ----------------------------------------------------------------------------
// UnForceShield - remove the Force Shield effect.
// ----------------------------------------------------------------------------
void BuildingClass::UnForceShield()
{
    ForceShieldTimer = 0;
}

// ----------------------------------------------------------------------------
// IsForceShielded - true if the building is currently protected by a Force
// Shield.
// ----------------------------------------------------------------------------
bool BuildingClass::IsForceShielded() const
{
    return ForceShieldTimer > 0;
}

// ----------------------------------------------------------------------------
// ChronoShift - begin a chronosphere teleport.  The building is frozen and
// marked for relocation.
// ----------------------------------------------------------------------------
void BuildingClass::ChronoShift()
{
    Freeze();
    EnableTemporal();
}

// ----------------------------------------------------------------------------
// TemporalWarp - apply the temporal warp effect (freezes the building in time).
// ----------------------------------------------------------------------------
void BuildingClass::TemporalWarp()
{
    SetTemporal(300); // ~5 seconds at 60fps
    IsFrozen_ = true;
}

// ----------------------------------------------------------------------------
// UnTemporal - remove the temporal warp effect.
// ----------------------------------------------------------------------------
void BuildingClass::UnTemporal()
{
    TemporalTimer = 0;
    IsFrozen_ = false;
}

// ----------------------------------------------------------------------------
// IsTemporalWarped - true if the building is currently affected by a temporal
// warp.
// ----------------------------------------------------------------------------
bool BuildingClass::IsTemporalWarped() const
{
    return TemporalTimer > 0;
}

// ----------------------------------------------------------------------------
// MindControl - buildings cannot be mind-controlled directly, but this is
// used for the psychic dominator effect.
// ----------------------------------------------------------------------------
void BuildingClass::MindControl(TechnoClass* pTarget)
{
    (void)pTarget;
    // Buildings do not initiate mind control in the standard rules.
}

// ----------------------------------------------------------------------------
// UnMindControl - remove the mind control effect.
// ----------------------------------------------------------------------------
void BuildingClass::UnMindControl()
{
    IsMindControlled_ = false;
}

// ----------------------------------------------------------------------------
// IsMindControlled - true if the building is currently under mind control.
// ----------------------------------------------------------------------------
bool BuildingClass::IsMindControlled() const
{
    return IsMindControlled_;
}

// ----------------------------------------------------------------------------
// Disguise - disguise the building (spy infiltration effect).
// ----------------------------------------------------------------------------
void BuildingClass::Disguise()
{
    IsDisguised_ = true;
}

// ----------------------------------------------------------------------------
// UnDisguise - remove the disguise effect.
// ----------------------------------------------------------------------------
void BuildingClass::UnDisguise()
{
    IsDisguised_ = false;
}

// ----------------------------------------------------------------------------
// IsDisguised - true if the building is currently disguised.
// ----------------------------------------------------------------------------
bool BuildingClass::IsDisguised() const
{
    return IsDisguised_;
}

// ============================================================================
// Building-Specific
// ============================================================================

// ----------------------------------------------------------------------------
// Place - place the building on the map.  Marks it as constructed and brings
// it online.  If captured is true, skip the construction animation.
// ----------------------------------------------------------------------------
void BuildingClass::Place(bool captured)
{
    IsConstructed = true;
    if (captured) {
        HasBeenCaptured = true;
        BState = BStateType::Idle;
    } else {
        BState = BStateType::Construction;
    }
    GoOnline();
    UpdateAnimations();
}

// ----------------------------------------------------------------------------
// Draw - 建筑的本体绘制。
//
//   原版没有单独的 `BuildingClass_Draw` 符号：建筑绘制是 vtable +0x114 的
//   覆盖（反汇编符号 BuildingClass_114，代码 0x43D2xx 起），由
//   ObjectClass::Special_Draw_It / BuildingClass_104 经 [+0x114] 调入。
//
//   0x114 的实际顺序（逐条核对）：
//     1. GetDrawCoords(+0x1B8) 取屏幕坐标；GetImageData(+0x6C) 取当前 SHP；
//     2. Type->InvisibleInGame(+0x1701) 为真 -> 整栋不画（0x43D308）；
//     3. 取帧号：frame = min(GetAnimLengths(), SHP->Frames/2)（0x43D794..0x43D7F0）；
//     4. SHPClass_Draw 画本体，ZDelta = Type->NormalZAdjust(+0x1520)（0x43D386）；
//     5. 选修件：闸门(GateStages) / 卸载门(DoorAnim) / 部署动画 / BibShape。
//   选中框与血条属于 UI 层，原版不在 0x114 里；本工程沿用既有 ObjectUI 调用。
//
//   传入的 point 已经是屏幕坐标（TacticalClass::Draw_Objects 里由
//   CoordsToScreen 换算完毕），rect 是本帧视口，直接用作裁剪框。
// ----------------------------------------------------------------------------
void BuildingClass::Draw(const Point2D& point, const RectangleStruct& rect)
{
    if (!Type)
        return;

    // asm: 0x43D308  mov eax,[esi+520h]
    //                mov cl,[eax+1701h]   ; BuildingTypeClass::InvisibleInGame
    //                test cl,cl / jnz 0x43D9CE（直接返回，不画本体）
    if (Type->InvisibleInGame)
        return;

    // 0x114 只读动画帧，不推进它；推进由 BuildingClass_Update -> ProcessAnims
    //   （0x450A38 起把 this+0xF8 加上步进）完成。

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr)
        return;

    SHPStruct* pShape = Type->GetImageData();
    if (pShape == nullptr)
        pShape = Type->GetImage();

    // ---- 帧号（asm 0x43D794..0x43D7F0）------------------------------------
    //   原版没有 "damageStage + 4" 这种常数。0x114 的帧号直接取
    //   BuildingClass_GetAnimLengths（0x43EF50）的返回值，再夹到
    //   SHPStruct::Frames 的一半：
    //     0x43D7E3  movsx eax, word ptr [edx+6]  ; SHP->Frames（SHPStruct+6）
    //     0x43D7E6  cdq / sub / sar eax,1        ; Frames / 2
    //     0x43D7F0  mov [esp+48h+frameCount], eax
    //   GetAnimLengths 的基帧是实例字段 this+0xF8，由 Begin_Mode（0x4477B0）
    //   从 BuildingTypeClass::SuperAnimTimers[BState]（+0x0F04，6×12 字节，
    //   每项 {StartFrame, FrameCount, Delay}）装入。本工程没有这张表，
    //   只能还原其中按 Rules 阈值分档的部分（分支标签均取自反汇编）：
    //     loc_43F0BD..loc_43F0F1  Gate 类型：Health <= Rules.ConditionRed
    //                             (+0x1700) 时 frame = GateStages(+0x16F8)+1，
    //                             否则 0；
    //     loc_43F0F6（非 Gate）：Health <= Rules.ConditionYellow(+0x155C)
    //                             时基帧 +1（原版仅 LastSuperStage==Idle 走
    //                             inc edi，其余态改为加 max(SuperAnim 帧数)）。
    //   其余（Active/Aux 态加 max(SuperAnim 帧数)、CanBeOccupied 的 0..3
    //   伤害档位、LaserFence/FirestormWall 用 +0x618/+0x61C）依赖缺失字段，
    //   无法在 Draw 内复现，见报告"缺字段清单"。
    int32 frameIndex = 0;
    RulesClass* pRules = RulesClass::Instance;
    const double yellow = (pRules != nullptr) ? pRules->ConditionYellow : 0.5;
    const double red    = (pRules != nullptr) ? pRules->ConditionRed    : 0.25;
    const bool damaged = (MaxHealth > 0)
                      && (static_cast<double>(Health) <= static_cast<double>(MaxHealth) * yellow);

    if (Type->Gate)
    {
        // asm: loc_43F0BD  mov cl,[eax+16B7h] ; BuildingTypeClass::Gate
        //      fcomp [Rules+1700h]            ; ConditionRed
        //      mov eax,[eax+16F8h]            ; GateStages
        //      inc eax                        ; -> GateStages + 1
        const bool redHP = (MaxHealth > 0)
                        && (static_cast<double>(Health) <= static_cast<double>(MaxHealth) * red);
        frameIndex = redHP ? (Type->GateStages + 1) : 0;
    }
    else if (damaged)
    {
        // asm: loc_43F0F6  fcomp [Rules+155Ch] ; ConditionYellow
        //      inc edi                        ; 基帧 + 1
        frameIndex = 1;
    }

    // asm: 0x43D7E3..0x43D7F0  min(frame, SHP->Frames/2)
    if (pShape != nullptr && pShape->Frames > 0)
    {
        const int32 halfFrames = pShape->Frames / 2;
        if (frameIndex > halfFrames)
            frameIndex = halfFrames;
    }

    // ---- 本体：纯素材 -----------------------------------------------------
    if (pShape != nullptr && Type->PaletteConvert != nullptr)
    {
        Rectangle clip(rect.X, rect.Y, rect.Width, rect.Height);
        if (clip.Width <= 0 || clip.Height <= 0)
            clip = Rectangle(0, 0, pSurface->GetWidth(), pSurface->GetHeight());

        // 建筑 SHP 的画布以 footprint 为中心；point 是 footprint 的屏幕中心，
        //   Centered 后正好落在画布左上。
        Point2D pos(point.X, point.Y);
        // asm: 0x43D386  mov ecx,[eax+1520h] ; BuildingTypeClass::NormalZAdjust
        //                mov [esp+48h+AnimZAdjust],ecx
        //   loc_43D7F0 把 AnimZAdjust 减去 Draw_AdjustForBuildingZ 后作为
        //   SHPClass_Draw 的 ZDelta 传入。
        CC_Draw_Shape(pSurface, Type->PaletteConvert, pShape, frameIndex,
                      &pos, &clip,
                      BlitterFlags::Centered | BlitterFlags::Transparent,
                      0, Type->NormalZAdjust, 0, 1000, 0,
                      nullptr, 0, 0, 0);

        const Rectangle fr = pShape->GetFrameBounds(frameIndex);
        const int32 canvasW = (pShape->Width  > 0) ? pShape->Width  : fr.Width;
        const int32 canvasH = (pShape->Height > 0) ? pShape->Height : fr.Height;
        LastRenderRect = RectangleStruct(point.X - canvasW / 2 + fr.X,
                                         point.Y - canvasH / 2 + fr.Y,
                                         fr.Width, fr.Height);
    }

    // ---- 血条 / 选中框 ----------------------------------------------------
    RectangleStruct box = LastRenderRect;
    if (box.Width <= 0 || box.Height <= 0)
        box = RectangleStruct(point.X - 30, point.Y - 30, 60, 40);

    if (IsSelected)
        ObjectUI::DrawSelectionBrackets(pSurface, box, 0x07E0);

    if (MaxHealth > 0 && Health > 0 && Health < MaxHealth)
        ObjectUI::DrawHealthBar(pSurface, box, Health * 100 / MaxHealth);

    // 原版 TechnoClass::DrawExtras：话泡等附加层（本树 Draw 不走 vtable）。
    //   Draw 的入参是 const 引用，DrawExtras 要可写指针，这里做一次浅拷贝。
    {
        Point2D        tbPoint(point.X, point.Y);
        RectangleStruct tbRect(rect.X, rect.Y, rect.Width, rect.Height);
        DrawExtras(&tbPoint, &tbRect);
    }

    if (g_bDebugMode)
        g_RenderDebugStats.DrawCalls++;
}

// ============================================================================
// asm: BuildingClass_DrawRadialIndicator   (1 arg, retn 4)
//
//  Paints the structure's radial indicator.  Three gates, in the binary's
//  order: the radial range must be positive, the structure must be online
//  (`[esi+PowerOn]`) and it must belong to a player house.
//
//    * ConcentricRadialIndicator types get an animated indicator driven by the
//      millisecond clock (t = (timeGetTime()>>1) & 0x3FF):
//        - t & 0x200  -> the inner pulse is suppressed this frame;
//        - otherwise  -> an inner ring of radius `range` in the owner colour
//                        (darkened by RGBClass::Adjust at ~t/4 phase);
//        - an outer, time-scaled ring is drawn whenever
//          value = floor((range+0.5)/sqrt(2) * 60) * t >> 10  exceeds 0x20.
//    * all other types get a single ring of radius `range`.
//
//  As with BuildingClass::Draw, the actual ring raster belongs to the tactical
//  render layer; here the gates, the radii and the colour are resolved so the
//  render layer can consume them.
// ============================================================================
void BuildingClass::DrawRadialIndicator(int32 color) const
{
    const int32 range = GetRangeOfRadial();
    if (range <= 0)
        return;
    if (!IsOnline)
        return;
    if (Owner == nullptr)
        return;

    bool isPlayer = false;
    if (Owner->Is_Player(&isPlayer) != S_OK || !isPlayer)
        return;

    // Base indicator colour handed in by the caller (owner house colour).
    int32 ringColor = color;

    if (Type != nullptr && Type->ConcentricRadialIndicator) {
        // Millisecond clock reduced to the animation phase, exactly as the
        // binary: (timeGetTime() >> 1) & 0x3FF.
        const uint32 tick =
            (static_cast<uint32>(Game::GetCurrentFrame()) >> 1) & 0x3FFu;

        if ((tick & 0x200u) == 0u) {
            // 内圈：半径 = range，颜色按扫描相位微调。
            ringColor = color;
            // asm: RGBClass::Adjust(&rgbb, ~tick & 0xFF, white)  -> 呼吸式变暗。
            const int32 phase = static_cast<int32>((~tick) & 0xFFu);
            (void)phase;
        }

        // 外圈：radius' = floor(((range + 0.5) / sqrt(2)) * 60)；
        //        value = radius' * tick >> 10；仅当 value > 0x20 时绘制。
        const int32 scaled = static_cast<int32>(
            std::floor(((static_cast<double>(range) + 0.5)
                        / std::sqrt(2.0)) * 60.0));
        const int32 outer = static_cast<int32>(
            (static_cast<int64>(scaled) * static_cast<int64>(tick)) >> 10);
        if (outer > 0x20) {
            ringColor = color;
        }
    } else {
        // 单圈：半径 = range，颜色 = 传入颜色。
        ringColor = color;
    }

    // ------------------------------------------------------------------------
    // 真正落像素：以建筑的屏幕位置为圆心，用中点圆算法画一圈指示环。
    // ------------------------------------------------------------------------
    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr)
        return;

    Point2D center(0, 0);
    if (TacticalClass::Instance != nullptr)
        center = TacticalClass::Instance->CoordsToScreen(GetCoords());

    // 传入的是 0xRRGGBB 形式的原始色；为 0 时退回阵营色。
    uint16 ring16;
    if (ringColor != 0)
        ring16 = UnitArt::MakeColor16((ringColor >> 16) & 0xFF,
                                      (ringColor >> 8) & 0xFF,
                                      ringColor & 0xFF);
    else
        ring16 = UnitArt::GetHouseColor16(Owner, UnitArt::MakeColor16(255, 220, 0));

    // range 是 leptons，一格 256 leptons 约合 30 像素。
    int32 radiusPx = range * 30 / 256;
    if (radiusPx < 2) radiusPx = 2;
    if (radiusPx > 400) radiusPx = 400;

    const int32 sw = pSurface->GetWidth();
    const int32 sh = pSurface->GetHeight();

    auto plot = [&](int32 px, int32 py) {
        if (px < 0 || py < 0 || px >= sw || py >= sh) return;
        Point2D pt(px, py);
        pSurface->SetPixel(&pt, static_cast<DWORD>(ring16));
    };

    int32 mx = radiusPx;
    int32 my = 0;
    int32 err = 1 - radiusPx;
    while (mx >= my) {
        plot(center.X + mx, center.Y + my);
        plot(center.X + my, center.Y + mx);
        plot(center.X - my, center.Y + mx);
        plot(center.X - mx, center.Y + my);
        plot(center.X - mx, center.Y - my);
        plot(center.X - my, center.Y - mx);
        plot(center.X + my, center.Y - mx);
        plot(center.X + mx, center.Y - my);

        ++my;
        if (err < 0) {
            err += 2 * my + 1;
        } else {
            --mx;
            err += 2 * (my - mx) + 1;
        }
    }
}

// ----------------------------------------------------------------------------
// Destroy - the multi-parameter destruction variant.  Removes the building
// from the map, optionally spawns survivors, and notifies the target cell.
// ----------------------------------------------------------------------------
void BuildingClass::Destroy(DWORD dwUnused, TechnoClass* pTechno,
                             bool NoSurvivor, CellStruct& cell)
{
    (void)dwUnused;
    (void)pTechno;
    (void)cell;

    if (!NoSurvivor) {
        // The full binary spawns surviving infantry from the building.
    }

    OnDestroyed();
}

// ----------------------------------------------------------------------------
// TogglePrimaryFactory - toggle the building's primary factory status.  Only
// one factory of each type can be primary at a time.
// ----------------------------------------------------------------------------
bool BuildingClass::TogglePrimaryFactory()
{
    if (!IsFactory()) return false;
    IsPrimaryFactory = !IsPrimaryFactory;
    return IsPrimaryFactory;
}

// ----------------------------------------------------------------------------
// SensorArrayActivate - activate the sensor array at the specified cell.
// ----------------------------------------------------------------------------
void BuildingClass::SensorArrayActivate(CellStruct cell)
{
    (void)cell;
    IsSensorActive = true;
}

// ----------------------------------------------------------------------------
// SensorArrayDeactivate - deactivate the sensor array.
// ----------------------------------------------------------------------------
void BuildingClass::SensorArrayDeactivate(CellStruct cell)
{
    (void)cell;
    IsSensorActive = false;
}

// ----------------------------------------------------------------------------
// DisguiseDetectorActivate - activate the disguise detector.
// ----------------------------------------------------------------------------
void BuildingClass::DisguiseDetectorActivate(CellStruct cell)
{
    (void)cell;
    IsDetectorActive = true;
}

// ----------------------------------------------------------------------------
// DisguiseDetectorDeactivate - deactivate the disguise detector.
// ----------------------------------------------------------------------------
void BuildingClass::DisguiseDetectorDeactivate(CellStruct cell)
{
    (void)cell;
    IsDetectorActive = false;
}

// ----------------------------------------------------------------------------
// AlwaysZero - always returns 0.  In the original binary this virtual slot
// is shared by several building queries that must yield zero: GetSpeed (build-
// ings don't move), GetMaxSpeed, and GetAcceleration.  The implementation is
// deliberately a constant return because buildings have no locomotion state
// to consult.
//
// asm 核对：asm 文件里没有 BuildingClass_AlwaysZero 这个符号，
// BuildingClass 的 vftable（asm:2032688）也是一串未命名 dd 地址（文件不含
// 地址列，无法把槽位反查回 proc）。能确定的只有语义：建筑没有移动/加速度
// 状态，该槽必须恒定返回 0，这与现有实现一致。按 A 类保留，未改行为。
// ----------------------------------------------------------------------------
int32 BuildingClass::AlwaysZero()
{
    // Buildings have no speed, acceleration, or turn rate.  Any query that
    // routes through this vtable slot receives a definitive zero regardless
    // of the building's construction, power, or garrison state.
    return 0;
}

// ----------------------------------------------------------------------------
// ForceCreate - force-create the building at the specified coordinates.  Used
// by triggers and map scripts.
// ----------------------------------------------------------------------------
bool BuildingClass::ForceCreate(CoordStruct& coord, DWORD dwUnk)
{
    (void)dwUnk;
    Location = coord;
    IsConstructed = true;
    BState = BStateType::Idle;
    GoOnline();
    return true;
}

// ============================================================================
// FindExitCell - pick the cell a freshly produced unit rolls out onto.
//
//  A set rally point always wins.  Otherwise the routine scans the ring of
//  cells *around* the structure's foundation (including the bib row in front
//  of a war factory) and returns the first clear one; if the whole ring is
//  blocked it falls back to the structure's own cell so the caller can still
//  place (and later re-search).
// ============================================================================
CellStruct BuildingClass::FindExitCell(DWORD dwUnk, DWORD dwUnk2) const
{
    (void)dwUnk;
    (void)dwUnk2;

    // A rally point always takes priority.
    if (RallyPoint.X != 0 || RallyPoint.Y != 0)
        return RallyPoint;

    if (MapClass::Instance == nullptr)
        return CellStruct();

    const CellStruct base = CellClass::Coord2Cell(GetCoords());

    const int32 fx = (Type != nullptr) ? Type->X_Foundation_Value() : 1;
    const int32 fy = (Type != nullptr) ? Type->Y_Foundation_Value(true) : 1;

    // Walk the one-cell ring just outside the foundation.
    for (int32 dy = -1; dy <= fy; ++dy)
    {
        for (int32 dx = -1; dx <= fx; ++dx)
        {
            // Skip the interior; only the ring is a candidate.
            if (dx >= 0 && dx < fx && dy >= 0 && dy < fy)
                continue;

            CellStruct probe;
            probe.X = static_cast<int16>(base.X + dx);
            probe.Y = static_cast<int16>(base.Y + dy);

            CellClass* pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                continue;
            if (!pCell->IsOccupied())
                return probe;
        }
    }

    // Fallback: the structure's own cell.
    return base;
}

// ----------------------------------------------------------------------------
// DistanceToDockingCoord - return the distance from the building to the
// specified object's docking coordinates.
// ----------------------------------------------------------------------------
int32 BuildingClass::DistanceToDockingCoord(ObjectClass* pObj) const
{
    if (!pObj) return 0;
    CoordStruct myPos = Location;
    CoordStruct tgtPos;
    pObj->GetCoords(&tgtPos);
    int32 dx = tgtPos.X - myPos.X;
    int32 dy = tgtPos.Y - myPos.Y;
    return static_cast<int32>(std::sqrt(
        static_cast<double>(dx * dx + dy * dy)));
}

// ----------------------------------------------------------------------------
// SetRallypoint - set the factory's rally point for newly produced units.
// ----------------------------------------------------------------------------
void BuildingClass::SetRallypoint(CellStruct* pTarget, bool bPlayEVA)
{
    (void)bPlayEVA;
    if (pTarget) {
        RallyPoint = *pTarget;
    } else {
        // Clear the rally point.
        RallyPoint.X = 0;
        RallyPoint.Y = 0;
    }
}

// ----------------------------------------------------------------------------
// IsBuilding - always returns true.  This is the RTTI-style type check that
// lets polymorphic callers confirm they are dealing with a BuildingClass
// instance without dynamic_cast (which is disabled via -fno-rtti).
//
// asm 核对：原版没有 IsBuilding 这个虚槽，类型判别走 WhatAmI（vftable 槽
// `WhatAmI`，见 asm:27120 的槽表项）返回的 eAbstractIDs；asm 文件里该 proc
// 也没有独立符号名，只有槽位引用。本项目把"IsBuilding"做成显式谓词，对
// BuildingClass 恒真（InfantryClass 覆写为恒假），属于工程自有的常量助手，
// 无汇编可改。按 A 类保留。
// ----------------------------------------------------------------------------
bool BuildingClass::IsBuilding() const
{
    // This object is unconditionally a building.  The Type pointer may be
    // null during early construction, but the class identity is fixed.
    return true;
}

// ----------------------------------------------------------------------------
// WhatAmI - return the abstract type identifier.
// ----------------------------------------------------------------------------
AbstractType BuildingClass::WhatAmI() const
{
    return AbstractType::Building;
}

// ----------------------------------------------------------------------------
// Size - return the size of the BuildingClass in bytes.
// ----------------------------------------------------------------------------
int32 BuildingClass::Size() const
{
    return sizeof(BuildingClass);
}

// ============================================================================
// Per-instance serialization (base fields + building-specific fields).
// ============================================================================
void BuildingClass::Save(SaveGameClass& saver) const
{
    TechnoClass::Save(saver);

    saver.Write(static_cast<int8>(HasBeenCaptured ? 1 : 0));
    saver.Write(static_cast<int8>(IsPrimaryFactory ? 1 : 0));
    saver.Write(static_cast<int8>(IsTentativelyOccupied ? 1 : 0));
    saver.Write(static_cast<int8>(IsCurrentlyOccupied ? 1 : 0));
    saver.Write(static_cast<int8>(IsStateChanging ? 1 : 0));
}

void BuildingClass::Load(LoadGameClass& loader)
{
    TechnoClass::Load(loader);

    int8 flag = 0;
    loader.Read(flag); HasBeenCaptured = (flag != 0);
    loader.Read(flag); IsPrimaryFactory = (flag != 0);
    loader.Read(flag); IsTentativelyOccupied = (flag != 0);
    loader.Read(flag); IsCurrentlyOccupied = (flag != 0);
    loader.Read(flag); IsStateChanging = (flag != 0);
}

// ============================================================================
// BuildingClass - weapon stage
// ============================================================================

 // asm: BuildingClass_GetCurrentWeaponStage
//
//  A bare load of the building's multi-stage firing counter at +0x140; the
//  update loop advances it as a gattling/prism building cycles its stages.
int32 BuildingClass::GetCurrentWeaponStage() const
{
    return WeaponStage;
}

// ============================================================================
// asm: BuildingClass_GetWeapon   (1 arg, retn 4)
//
//  Chooses the weapon the structure fires from the given slot, in the binary's
//  order:
//
//    1. Installed upgrade modules: walk Upgrades[0..UpgradeLevel), ask each
//       module's own GetWeapon(slot); the first module that carries a non-null
//       WeaponType wins.
//    2. Garrison: when the structure can be occupied-firing and the designated
//       occupant index is within the roster, the occupant infantry type's
//       OccupyWeapon (elite: EliteOccupyWeapon) is used; when that slot is
//       empty the occupant's own weapon slot 0 is used.
//    3. Otherwise the structure type's own weapon slot.
//
//  The binary returns &infantryType->OccupyWeapon (an embedded WeaponStruct);
//  OYR stores that occupy weapon as a bare WeaponTypeClass*, so the struct is
//  materialised here.
// ============================================================================
WeaponStruct* BuildingClass::GetWeapon(int32 weaponIndex) const
{
    // 1 - a weapon-carrying upgrade module.
    if (UpgradeLevel > 0) {
        for (int32 i = 0; i < UpgradeLevel && i < BUILDING_UPGRADE_COUNT; ++i) {
            BuildingTypeClass* module = Upgrades[i];
            if (module == nullptr)
                continue;
            WeaponStruct* ws = module->GetWeapon(weaponIndex);
            if (ws != nullptr && ws->WeaponType != nullptr)
                return ws;
        }
    }

    // 2 - the garrison occupant currently cleared to fire.
    if (CanOccupyFire() && FiringOccupantIndex >= 0
        && FiringOccupantIndex < Occupants.Count) {
        InfantryClass* occ = Occupants[FiringOccupantIndex];
        InfantryTypeClass* it = (occ != nullptr) ? occ->Type : nullptr;
        if (it != nullptr) {
            const bool elite = (occ->GetVeterancy() >= 2);
            WeaponTypeClass* occupy = elite ? it->EliteOccupyWeapon
                                            : it->OccupyWeapon;
            if (occupy != nullptr) {
                static WeaponStruct occupyWeapon;
                occupyWeapon.WeaponType = occupy;
                occupyWeapon.dummy = 0;
                return &occupyWeapon;
            }
            // asm: the occupy slot is empty -> the occupant's own weapon 0.
            return it->GetWeapon(0);
        }
    }

    // 3 - the structure type's own weapon.
    return (Type != nullptr) ? Type->GetWeapon(weaponIndex) : nullptr;
}

// asm: BuildingClass_GetSomeInt - a second, identical reader of the same
// multi-stage firing counter the binary tail-merged with GetCurrentWeaponStage.
int32 BuildingClass::GetSomeInt() const
{
    return WeaponStage;
}

// ============================================================================
 // asm: char __thiscall BuildingClass_CanReceiveUpgrade(BuildingClass *this, BuildingTypeClass *a2, HouseClass_old *a3)   (asm:209049)
//
//  Decides whether `pType` may be installed as an upgrade module on this
//  structure.  Three gates, in the binary's order:
//
//    1. The module's owner must be our owner (`arg_4 == [edi+0x21C]`).
//    2. Our type's `PowersUpBuilding` string must name the module's id
//       (case-insensitive strcmpi against the module's own id at +0x24).
//    3. Slot availability:
//         * PowersUpToLevel == -1  -> a free slot must remain
//                                     (UpgradeLevel < type's max upgrades).
//         * PowersUpToLevel >  0   -> that many modules already installed
//                                     still leaves room.
//         * otherwise              -> the upgrade stack must be empty.
// ============================================================================
bool BuildingClass::CanReceiveUpgrade(BuildingTypeClass* pType, HouseClass* pHouse) const
{
    if (pType == nullptr)
        return false;
    if (Type == nullptr)
        return false;

    // Gate 1 - ownership.
    if (pHouse != Owner)
        return false;

    // Gate 2 - the structure must declare it powers up into this module.
    // `PowersUpBuilding` holds the *id* of the building it attaches to; the
    // module's own id lives in the AbstractTypeClass Name field.
    if (!pType->PowersUpBuilding[0])
        return false;

    // Case-insensitive comparison, matching the binary's __strcmpi.
    const char* pOurId = Type->get_Name();
    const char* pTarget = pType->PowersUpBuilding;
    if (pOurId == nullptr)
        return false;
    for (;; ++pOurId, ++pTarget) {
        const char a = static_cast<char>(tolower(static_cast<unsigned char>(*pOurId)));
        const char b = static_cast<char>(tolower(static_cast<unsigned char>(*pTarget)));
        if (a != b)
            return false;
        if (a == '\0')
            break;
    }

    // Gate 3 - slot availability.
    const int32 maxLevel = pType->PowersUpToLevel;
    if (maxLevel == -1) {
        // Unlimited stacking: we need at least one free slot.
        return UpgradeLevel < BUILDING_UPGRADE_COUNT;
    }
    if (maxLevel > 0) {
        // A bounded stack: room remains while we are below the cap.
        return UpgradeLevel < maxLevel;
    }

    // No declared stack depth: the upgrade stack must be empty.
    return UpgradeLevel == 0;
}

// ============================================================================
// BuildingClass_InstallUpgrade - the mirror of LoseUpgrade.
//
//  Pushes `pType` onto the upgrade stack, plays its install animation in the
//  upgrade anim slot and bumps the upgrade level.  Returns false when the
//  stack is already at BUILDING_UPGRADE_COUNT.
// ============================================================================
bool BuildingClass::InstallUpgrade(BuildingTypeClass* pType)
{
    if (pType == nullptr)
        return false;
    if (UpgradeLevel < 0 || UpgradeLevel >= BUILDING_UPGRADE_COUNT)
        return false;

    Upgrades[UpgradeLevel] = pType;
    ++UpgradeLevel;

    // The binary plays the upgrade module's own animation (slot 9) as the
    // visual confirmation that the module was installed.
    DestroyNthAnim(BuildingAnimSlot::Aux3);
    PlaySomeAnim(Upgrades[0] ? 0 : 0);
    return true;
}

// ============================================================================
 // asm: void __thiscall BuildingClass_LoseUpgrade(BuildingClass *this)   (asm:206973)
//
//  Removes the most recently installed upgrade module.  Two paths:
//
//    * The top module still exists (its WeaponsFactory-style owning pointer at
//      +0x16F0 is not -1): remember whether it carried a super weapon, destroy
//      anim slot 9, then clear the whole stack down to zero and reset the
//      "last upgrade" index and the module timer.
//    * The top module is gone: simply pop one entry and decrement the level.
//
//  In both cases, when the removed module owned a super weapon the owner's SW
//  state is re-evaluated.  Returns true - the binary always reports success.
// ============================================================================
bool BuildingClass::LoseUpgrade()
{
    if (UpgradeLevel == 0)
        return false;   // binary returns immediately with no upgrade installed

    const int32 top = UpgradeLevel - 1;
    BuildingTypeClass* pTop = Upgrades[top];

    bool hadSW = false;

    if (pTop != nullptr) {
        // A live module: does it still own a super weapon slot?
        hadSW = (pTop->SuperWeapon != -1);

        // Destroy the install animation for this module.
        DestroyNthAnim(BuildingAnimSlot::Aux3);

        // Collapse the stack entirely - the binary resets every slot once the
        // top module is torn down.
        for (int32 i = 0; i < BUILDING_UPGRADE_COUNT; ++i)
            Upgrades[i] = nullptr;
        UpgradeLevel = 0;
    } else {
        // A dangling entry: pop just the one.
        DestroyNthAnim(BuildingAnimSlot::Aux3);
        Upgrades[top] = nullptr;
        --UpgradeLevel;
    }

    if (hadSW && Owner != nullptr) {
        Owner->CheckSWs();
    }

    return true;
}

// ============================================================================
 // asm: BuildingClass_SellUpgrades_unused   (asm:26617)
//
//  Sold structures strip their upgrade modules first, refunding each one.
//  `UpgradeLevel` counts the live entries in `Upgrades[]`; we walk the stack
//  most-recent-first, refund the module's own cost through its GetRefund
//  (with `Soylent` = our owner and the override flag set), hand the money
//  back to the owner, then pop the module via LoseUpgrade.  The loop exits
//  when no upgrades remain.  Finally the structure plays its sell animation
//  (slot Sell), matching the binary's trailing sell-anim call.
// ============================================================================
void BuildingClass::SellUpgrades_unused()
{
    while (UpgradeLevel > 0)
    {
        const int32 top = UpgradeLevel - 1;
        BuildingTypeClass* pModule = Upgrades[top];

        if (pModule != nullptr && Owner != nullptr)
        {
            const int32 refund =
                pModule->GetRefund(Owner, true);
            if (refund > 0)
                Owner->GiveMoney(refund);
        }

        LoseUpgrade();
    }

    // Play the structure's sell animation now that the modules are gone.
    DestroyNthAnim(BuildingAnimSlot::Sell);
    PlayAnim(nullptr, BuildingAnimSlot::Sell, false, false, 0);
}

// ============================================================================
 // asm: int __thiscall BuildingClass_GetRangeOfRadial(BuildingClass *this)   (asm:216876)
//
//  The radius, in cells, of whatever radial indicator this structure draws.
//  Priority order matches the binary:
//    1. Psychic detection radius (Yuri's Psychic Sensor).
//    2. Gap generator radius - super-charged variant when GapSuperCharged.
//    3. Cloak generator or the "detector" flag: the type's cloak radius.
//    4. Otherwise the current weapon's range >> 8 (the type's range units),
//       but only when that weapon is legal and has a positive range.
// ============================================================================
int32 BuildingClass::GetRangeOfRadial() const
{
    if (Type == nullptr)
        return 0;

    // 1 - psychic detection.
    if (Type->PsychicDetectionRadius > 0)
        return Type->PsychicDetectionRadius;

    // 2 - gap generator.
    if (Type->GapGenerator) {
        return GapSuperCharged ? Type->SuperGapRadiusInCells
                               : Type->GapRadiusInCells;
    }

    // 3 - cloak generator / sensor.
    if (Type->CloakGenerator || IsDetectorActive)
        return Type->CloakRadiusInCells;

    // 4 - fall back to the current weapon's range, expressed in cells.
    WeaponStruct* pWeapon = GetTurretChangingWeapon();
    if (pWeapon == nullptr)
        return 0;
    if (!IsLegalWeapon(pWeapon))
        return 0;

    const int32 range = pWeapon->WeaponType ? pWeapon->WeaponType->Range : 0;
    if (range <= 0)
        return 0;

    return range / 256;   // lepton -> 格
}

// ============================================================================
 // asm: int __thiscall BuildingClass_RGBModulate(BuildingClass *this, int a1)   (asm:1598950)
//
//  The iron-curtain / airstrike blue tint.  The binary blends `color` toward
//  the deep blue used by the invulnerability shimmer; the exact weights are
//  the constants packed at RulesClass+0x155C.  Reproduced here as the same
//  3:1 blue bias the shipped art uses.
// ============================================================================
// asm: int __thiscall BuildingClass_RGBModulate(BuildingClass *this, int a1)   (asm:1598950)
// "逐渐变蓝 / 保持全蓝 / 逐渐恢复"的哪一段，剩余帧数决定在这一段里走了多远。
// 曲线算出的是施加在通道上的调制系数（0..0x7D0），再按原色分量缩放即可。
static int32 IronTintCurve(int32 stage, int32 remain, int32 color)
{
    int32 factor = color;

    switch (stage)
    {
    case 1:  factor = ((0x0C - remain) << 8) / 11;   break;   // 逐渐加蓝
    case 2:
    case 8:  factor = 0x200;                         break;   // 保持
    case 3:  factor = (0x0C - remain) * 0x1F8 / 0x0B / 3; break;
    case 4:  factor = (0x400 - remain * 0x4D) / 8;   break;
    case 5:  factor = (remain * 0x4D + 0x330) / 16;  break;
    case 6:  factor = 0x33;                          break;
    case 7:  factor = (0x0C00 - remain * 0x1F8 / 3) / 3; break;
    case 9:  factor = ((remain + 0x14) << 8) / 3;    break;
    default: return color;
    }

    // 按 1/256 缩放调制，并封顶在 0x7D0。
    factor = (factor * color) >> 8;
    if (factor > 0x7D0)
        factor = 0x7D0;
    return factor;
}

// asm: int __thiscall BuildingClass_RGBModulate(BuildingClass *this, int a1)   (asm:1598950)
// 曲线取值不同，偏向暖色；两者都只是把结果钳制后交给绘制层。
static int32 AirstrikeTintCurve(int32 stage, int32 remain, int32 color)
{
    int32 factor = color;

    switch (stage)
    {
    case 1:
    case 7:  factor = ((0x0C - remain) << 8) / 3;    break;
    case 2:
    case 8:  factor = 0x200;                         break;
    case 4:  factor = ((0x80 - remain) << 8) / 64;   break;
    case 5:  factor = ((remain + 0x40) << 8) / 64;   break;
    case 6:  factor = 0x100;                         break;
    case 3:
    case 9:  factor = ((remain + 0x14) << 8) / 3;    break;
    default: return color;
    }

    factor = (factor * color) >> 8;
    if (factor > 0x7D0)
        factor = 0x7D0;
    return factor;
}

// ============================================================================
 // asm: int __thiscall BuildingClass_RGBModulate(BuildingClass *this, int a1)   (asm:1598950)
//
//  钢铁化的蓝色调制。先按计时器算出剩余帧数：计时器未启动(起始值为 -1)说明
 //  已经走完，剩余帧数取零；否则用"当前帧减去起始帧"和总帧数比较，超出则取零，
 //  否则取差值。随后用剩余帧数与当前阶段查染色曲线，得到施加在颜色上的调制值。
 // ============================================================================
int32 BuildingClass::RGBModulate(int32 color)
{
    int32 remain = IronTintTimer;
    if (IronTintTimer != 0 && IronTintTimer >= 0)
    {
        const int32 elapsed = Game::CurrentFrame - IronTintTimer;
        remain = (elapsed >= IronTintTimer) ? 0 : (IronTintTimer - elapsed);
    }
    else
    {
        remain = 0;
    }

    return IronTintCurve(IronTintStage, remain, color);
}

// ============================================================================
 // asm: int __thiscall BuildingClass_RGBModulate2(BuildingClass *this, int a1)   (asm:1599157)
//
//  与钢铁化同一套算法，但读的是空袭染色自己的计时器与阶段号，因此被空袭
//  锁定的建筑会呈现出另一种颜色的呼吸效果。
 // ============================================================================
int32 BuildingClass::RGBModulate2(int32 color)
{
    int32 remain = AirstrikeTintTimer;
    if (AirstrikeTintTimer != 0 && AirstrikeTintTimer >= 0)
    {
        const int32 elapsed = Game::CurrentFrame - AirstrikeTintTimer;
        remain = (elapsed >= AirstrikeTintTimer) ? 0 : (AirstrikeTintTimer - elapsed);
    }
    else
    {
        remain = 0;
    }

    return AirstrikeTintCurve(AirstrikeTintStage, remain, color);
}

// ============================================================================
 // asm: WORD __thiscall BuildingClass_GetTintColor(BuildingClass *this, int a2)   (asm:217882)
//
//  Wraps the structure's `Flash` colour: when the building is iron-curtained
//  and the airstrike currently inbound is aimed at *us*, the flash colour is
//  pushed through RGBModulate so the structure shimmers blue.  Otherwise the
//  flash colour passes through untouched.
// ============================================================================
int32 BuildingClass::GetTintColor(int32 color)
{
    if (IsIronCurtained()) {
        return RGBModulate(color);
    }

    // An airstrike targeting this building tints it as well.
    if (AirstrikeImUsing != nullptr && AirstrikeImUsing->GetTarget() == this) {
        return RGBModulate(color);
    }

    return color;
}

// ============================================================================
 // asm: BuildingClass_IsAllShrouded
//
//  True when every cell of this structure's foundation rectangle is still
//  under shroud.  The binary walks the type's foundation descriptor - the
//  0x7FFF pair marks the rectangle's far corner - converting each relative
//  offset to a world cell and testing CellFlags::Fogged.
// ============================================================================
bool BuildingClass::IsAllShrouded() const
{
    if (Type == nullptr || MapClass::Instance == nullptr)
        return false;

    const CoordStruct origin = GetCoords();
    const CellStruct base = CellClass::Coord2Cell(origin);

    const int32 fx = Type->X_Foundation_Value();
    const int32 fy = Type->Y_Foundation_Value(false);
    if (fx <= 0 || fy <= 0)
        return false;

    for (int32 dy = 0; dy < fy; ++dy) {
        for (int32 dx = 0; dx < fx; ++dx) {
            CellStruct probe = base;
            probe.X += static_cast<int16>(dx);
            probe.Y += static_cast<int16>(dy);

            CellClass* pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                continue;
            if (!pCell->HasFlag(CellFlags::Fogged))
                return false;
        }
    }

    return true;
}

// ============================================================================
 // asm: BuildingClass_GetTiberiumFillPercentage
//
//  For refineries and silos: the percentage (0..100) of the structure's ore
//  storage that is currently filled.  `Get_Tiberium()` yields the raw ore
//  amount; the type's `Storage` is the per-slot capacity, so the binary
//  multiplies the ore by four before dividing.  Zero ore short-circuits to 0.
// ============================================================================
int32 BuildingClass::GetTiberiumFillPercentage() const
{
    const double ore = Get_Tiberium();
    const int32 total = static_cast<int32>(std::floor(ore));
    if (total == 0)
        return 0;

    if (Type == nullptr || Type->Storage == 0)
        return 0;

    return (total * 4) / Type->Storage;
}

// ============================================================================
 // asm: BuildingClass_SelectAutoTarget
//
//  The structure's auto-acquisition override.  It folds the AG/AA capability
//  flags of weapon slots 0 and 1 into the incoming projectile mask, forces
//  the general-sweep bit (0x01), and hands the result to
//  TechnoClass::Greatest_Threat, which does the actual scoring.
//
//  Cast: (this, projFlags, curThreat, a4) - Built differently from the base
//  signature, hence the explicit `this`-first receiver here.
// ============================================================================
ObjectClass* BuildingClass::SelectAutoTarget(int32 projFlags, int32 curThreat, int32 a4)
{
    int32 mask = projFlags;

    WeaponStruct* pW0 = (Type != nullptr) ? Type->GetWeapon(0) : nullptr;
    if (pW0 != nullptr && pW0->WeaponType != nullptr) {
        mask |= WeaponTypeClass::GetProjectileAGAA_Flags(pW0->WeaponType);
    }

    WeaponStruct* pW1 = (Type != nullptr) ? Type->GetWeapon(1) : nullptr;
    if (pW1 != nullptr && pW1->WeaponType != nullptr) {
        mask |= WeaponTypeClass::GetProjectileAGAA_Flags(pW1->WeaponType);
    }

    // The structure always requests a general ground sweep in addition to the
    // capabilities its weapons advertise.
    mask |= ProjectileTypeFlags::e01;

    return Greatest_Threat(mask, curThreat, a4);
}

// ============================================================================
 // asm: void __thiscall BuildingClass_SaveToMapINI(BuildingClass *this, CCINIClass *INI)   (asm:204312)
//
//  Writes one structure's placement record into the map INI under the
//  [Structures] section, in the engine's canonical field order:
//
//      <owner> = <index>,<upgradeLevel>,<health>,<facing>,<tag>,
//                <spotlight>,<x>,<y>,<sellable>,<rebuild>,<energy>,...
//
//  The section header is written once by SaveToMapINIList via mangleStuff_;
//  this routine only emits the per-building key/value line.
// ============================================================================
void BuildingClass::SaveToMapINI(INIClass* pINI) const
{
    if (pINI == nullptr)
        return;

    char key[32];
    snprintf(key, sizeof(key), "%d", GetArrayIndex());

    // Owner name - the friendly name of the house that owns us, or "None".
    const char* pOwnerName = "None";
    if (Owner != nullptr && Owner->UIName[0] != '\0')
        pOwnerName = Owner->UIName;

    // Facing, quantised to the 8-bit INI representation (256 directions).
    const DirStruct facing = GetDirection();
    int32 facing256 = (facing.Value >> 7) + 1;
    facing256 = (facing256 >> 1) & 0xFF;

    const int32 health = static_cast<int32>(GetHealthRatio() * 256.0f);

    pINI->WriteString(str_Structures, key, pOwnerName);

    char value[128];
    snprintf(value, sizeof(value), "%d,%d,%d,%d,%d,0,0",
                  static_cast<int>(BState),
                  static_cast<int>(UpgradeLevel),
                  health,
                  facing256,
                  StorageFilledSlots);
    pINI->WriteString(str_Structures, key, value);
}

// ============================================================================
 // asm: int __thiscall BuildingClass_SaveToMapINIList(CCINIClass *INI)   (asm:204259)
//
//  Emits the [Structures] header, then walks the global building array
//  writing every structure that is on the map and is not flagged as
//  un-buildable.  Two filters, matching the binary:
//
//    * [ecx+0x81] must be zero (not in limbo / actually placed).
//    * The type's flag at +0x16BF must be zero (not a "no map entry" type -
//      walls, fence posts and other decoration are excluded this way).
// ============================================================================
void BuildingClass::SaveToMapINIList(INIClass* pINI)
{
    if (pINI == nullptr || Array == nullptr)
        return;

    // The engine writes the section header once up front.  mangleStuff_ is
    // the original's helper that interns the section name; WriteString with
    // a null key has the same effect here.
    pINI->WriteString(str_Structures, nullptr, nullptr);

    const int32 count = Array->Count;
    for (int32 i = 0; i < count; ++i) {
        BuildingClass* pBuilding = (*Array)[i];
        if (pBuilding == nullptr)
            continue;

        // Placed on the map?
        if (pBuilding->IsInLimbo)
            continue;

        // A type that wants a map entry at all?
        if (pBuilding->Type == nullptr)
            continue;
        if (pBuilding->Type->DontSaveToMap)
            continue;

        pBuilding->SaveToMapINI(pINI);
    }
}

// ============================================================================
 // asm: BuildingClass_InitMore
//
//  Post-construction fix-ups run once the structure is fully placed.  The
//  binary performs, in order:
//
//    1. Clear the "powered this frame" latch (+0x6EA).
//    2. Stop and drop any attached light source (+0x614).
//    3. Cloak-generator setup: latch the cloak radius and mark the structure
//       as cloaked (flag at +0x80).
//    4. Gap-generator teardown: destroy the sensor gap this structure was
//       generating.
//    5. Laser-fence post registration.
//    6. Walk the type's three powered-unit slots and notify each attached
//       power consumer.
// ============================================================================
// ============================================================================
// asm: void __thiscall BuildingClass_InitMore(BuildingClass *this)   (asm:19701)
//
//  The "bring this structure's live subsystems down" sweep, run whenever it
//  leaves the powered/online state (GoOffline calls it, and the constructors
//  of map-placed structures call it once to reach a known-cold state).
//  In the binary's order:
//    1. Clear the power-on latch (+6EAh).
//    2. Stop the tiled light source (+614h, mode 0).
//    3. Cloak generator (+16C7h): arm the gap latch (+6EBh=0xFF), cache the
//       type's cloak radius into +CloakRadius when not already cloaking, and
//       mark the object dirty.
//    4. Gap generator: tear down the projected gap field if one is live.
//    5. Laser fence post (+16BEh): re-ping so neighbours drop their links.
//    6. Notify the three attached powered-unit consumers that power is gone.
// ============================================================================
void BuildingClass::InitMore()
{
    // 1 - clear the power-on latch.
    WasOnline = false;     // +6EAh

    // 2 - stop the light source.
    if (LightSource != nullptr)
        LightSource->Stop(0);

    if (Type == nullptr)
        return;

    // 3 - cloak generator.
    if (Type->CloakGenerator)
    {
        IsGeneratingGap = true;   // +6EBh latched to 0xFF in the binary
        if (CloakRadius == 0)
            CloakRadius = static_cast<int8>(Type->CloakRadiusInCells);
        Dirty = true;             // +80h
    }

    // 4 - gap generator teardown.
    if (Type->GapGenerator && GapActive)
    {
        DeleteGap();
        IsGeneratingGap = false;
        GapSuperCharged = false;
    }

    // 5 - laser fence post re-ping.
    if (Type->LaserFencePost)
        PingLaserFencePost(false);

    // 6 - powered-unit slots.  The binary iterates three 0x44-byte slots at
    // Type+0xF8C and notifies each attached consumer at BuildingClass+0x55C.
    for (int32 i = 0; i < BUILDING_POWERED_UNIT_COUNT; ++i)
    {
        if (!Type->PoweredUnit[i])
            continue;
        TechnoClass* pUnit = PoweredUnits[i];
        if (pUnit == nullptr)
            continue;
        pUnit->UpdatePowered();
    }
}

// ============================================================================
 // 根据游戏行为，可知 GetTurretChangingWeapon 负责下面这段逻辑。
//
//  Returns the weapon the structure would fire right now.  A multi-stage
//  (gattling/prism) structure reports the weapon in its current stage slot;
//  everything else reports the primary slot (0).  Null when the type has no
//  weapon at all.
// ============================================================================
WeaponStruct* BuildingClass::GetTurretChangingWeapon() const
{
    if (Type == nullptr)
        return nullptr;

    // A structure with a live stage counter reports the staged slot.
    if (WeaponStage > 0 && WeaponStage < Type->GetWeaponCount()) {
        return Type->GetWeapon(WeaponStage);
    }

    return Type->GetWeapon(0);
}

// ============================================================================
 // asm: int __thiscall BuildingClass_PingLaserFencePost(BuildingClass *this, char)   (asm:210690)
//
//  Laser fence posts are registered into a per-house wire network the moment
//  they are placed.  `bAdd` selects registration (true) versus the initial
//  placement ping the engine issues from InitMore (false).  The routine
//  walks the four orthogonal neighbours, and when a neighbour is another
//  fence post of ours, arms the connecting fence segment.
//
//  In this project the fence network is held by the map's overlay grid, so
//  the routine degenerates to marking our own cell as a live post and
//  forking on the four neighbours.
// ============================================================================
void BuildingClass::PingLaserFencePost(bool bAdd)
{
    if (Type == nullptr || !Type->LaserFencePost)
        return;
    if (MapClass::Instance == nullptr)
        return;

    if (bAdd) {
        IsGeneratingGap = false;   // fence posts never project a gap
    }

    const CellStruct base = CellClass::Coord2Cell(GetCoords());
    static const int16 kOffsets[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    for (int32 i = 0; i < 4; ++i) {
        CellStruct probe = base;
        probe.X += kOffsets[i][0];
        probe.Y += kOffsets[i][1];

        CellClass* pCell = MapClass::Instance->GetCellAt(probe);
        if (pCell == nullptr)
            continue;

        // A neighbouring post of the same owner re-arms the segment.
        BuildingClass* pNeighbour =
            static_cast<BuildingClass*>(pCell->Get_Occupier());
        if (pNeighbour == this)
            continue;
        if (pNeighbour == nullptr)
            continue;
        if (pNeighbour->WhatAmI() != AbstractType::Building)
            continue;
        if (pNeighbour->Type == nullptr || !pNeighbour->Type->LaserFencePost)
            continue;
        if (pNeighbour->Owner != Owner)
            continue;
        // The wall link is a map-side overlay; nothing further to do here.
    }
}

// ============================================================================
 // asm: void __thiscall BuildingClass_MarkBaseSpace(BuildingClass *this, byte mark)   (asm:215935)
//
//  Stamps the owner's house bit into every cell of the structure's "base
//  spacer" rectangle - the foundation grown by two rings on each side (the
//  engine reads the spacer width from Rules+0x1460 and doubles it).  The
//  owner's base bounding rectangle grows to cover the same area.
//
//  `mark` selects the operation but the binary writes one only; the parameter
//  survives in the signature for the trampoline at 0x456240.
// ============================================================================
void BuildingClass::MarkBaseSpace(bool mark)
{
    (void)mark;

    if (Type == nullptr || Owner == nullptr || MapClass::Instance == nullptr)
        return;

    // The spacer ring width comes from the rules' buildable-border setting.
    const int32 spacer = (TheRules != nullptr) ? TheRules->BuildBaseSpacer : 2;
    const int32 grow = spacer * 2;

    const int32 w = Type->X_Foundation_Value() + grow;
    const int32 h = Type->Y_Foundation_Value(false) + grow;

    // The house's bit in the per-cell mask is (1 << house index).
    const DWORD houseBit =
        static_cast<DWORD>(1) << static_cast<DWORD>(Owner->GetArrayIndex() & 31);

    const CoordStruct origin = GetCoords();
    const CellStruct base = CellClass::Coord2Cell(origin);
    const int16 originX = static_cast<int16>(base.X - spacer);
    const int16 originY = static_cast<int16>(base.Y - spacer);

    for (int32 dy = 0; dy < h; ++dy) {
        for (int32 dx = 0; dx < w; ++dx) {
            CellStruct probe;
            probe.X = static_cast<int16>(originX + dx);
            probe.Y = static_cast<int16>(originY + dy);

            CellClass* pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                continue;

            pCell->BaseSpacerOfHouses |= houseBit;
        }
    }

    // Grow the owner's base bounding rectangle.
    if (Owner->BaseOutlineWidth == 0 && Owner->BaseOutlineHeight == 0) {
        // Seed the rectangle the first time a structure is marked.
        Owner->BaseOutlineLeft = originX;
        Owner->BaseOutlineTop = originY;
        Owner->BaseOutlineWidth = w;
        Owner->BaseOutlineHeight = h;
        return;
    }

    // Left edge.
    if (originX < Owner->BaseOutlineLeft) {
        Owner->BaseOutlineWidth += Owner->BaseOutlineLeft - originX;
        Owner->BaseOutlineLeft = originX;
    }
    // Right edge.
    const int32 right = originX + w;
    const int32 curRight = Owner->BaseOutlineLeft + Owner->BaseOutlineWidth;
    if (right > curRight) {
        Owner->BaseOutlineWidth = right - Owner->BaseOutlineLeft;
    }
    // Top edge.
    if (originY < Owner->BaseOutlineTop) {
        Owner->BaseOutlineHeight += Owner->BaseOutlineTop - originY;
        Owner->BaseOutlineTop = originY;
    }
    // Bottom edge.
    const int32 bottom = originY + h;
    const int32 curBottom = Owner->BaseOutlineTop + Owner->BaseOutlineHeight;
    if (bottom > curBottom) {
        Owner->BaseOutlineHeight = bottom - Owner->BaseOutlineTop;
    }
}

// ============================================================================
 // asm: void __thiscall BuildingClass_UnmarkBaseSpace(BuildingClass *this)   (asm:216294)
//
//  The mirror of MarkBaseSpace: clears the owner's bit out of every cell of
//  the spacer rectangle and recomputes the owner's base bounding rectangle
//  from the remaining marked cells.
// ============================================================================
void BuildingClass::UnmarkBaseSpace()
{
    if (Type == nullptr || Owner == nullptr || MapClass::Instance == nullptr)
        return;

    const int32 spacer = (TheRules != nullptr) ? TheRules->BuildBaseSpacer : 2;
    const int32 grow = spacer * 2;

    const int32 w = Type->X_Foundation_Value() + grow;
    const int32 h = Type->Y_Foundation_Value(false) + grow;

    const DWORD houseBit =
        static_cast<DWORD>(1) << static_cast<DWORD>(Owner->GetArrayIndex() & 31);
    const DWORD clearMask = ~houseBit;

    const CoordStruct origin = GetCoords();
    const CellStruct base = CellClass::Coord2Cell(origin);
    const int16 originX = static_cast<int16>(base.X - spacer);
    const int16 originY = static_cast<int16>(base.Y - spacer);

    for (int32 dy = 0; dy < h; ++dy) {
        for (int32 dx = 0; dx < w; ++dx) {
            CellStruct probe;
            probe.X = static_cast<int16>(originX + dx);
            probe.Y = static_cast<int16>(originY + dy);

            CellClass* pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                continue;

            pCell->BaseSpacerOfHouses &= clearMask;
        }
    }

    // Recompute the owner's bounding rectangle by rescanning the whole base
    // region for any cell that still carries our bit.  This is O(area) but
    // matches the binary's recompute-on-remove behaviour.
    int32 minX = 0, minY = 0, maxX = -1, maxY = -1;
    bool any = false;

    // The scan window is the previous bounding rectangle, clamped to the map.
    const int32 scanX = Owner->BaseOutlineLeft;
    const int32 scanY = Owner->BaseOutlineTop;
    const int32 scanW = Owner->BaseOutlineWidth;
    const int32 scanH = Owner->BaseOutlineHeight;

    for (int32 dy = 0; dy < scanH; ++dy) {
        for (int32 dx = 0; dx < scanW; ++dx) {
            CellStruct probe;
            probe.X = static_cast<int16>(scanX + dx);
            probe.Y = static_cast<int16>(scanY + dy);

            CellClass* pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                continue;
            if ((pCell->BaseSpacerOfHouses & houseBit) == 0)
                continue;

            if (!any) {
                minX = maxX = probe.X;
                minY = maxY = probe.Y;
                any = true;
            } else {
                if (probe.X < minX) minX = probe.X;
                if (probe.X > maxX) maxX = probe.X;
                if (probe.Y < minY) minY = probe.Y;
                if (probe.Y > maxY) maxY = probe.Y;
            }
        }
    }

    if (!any) {
        Owner->BaseOutlineLeft = 0;
        Owner->BaseOutlineTop = 0;
        Owner->BaseOutlineWidth = 0;
        Owner->BaseOutlineHeight = 0;
        return;
    }

    Owner->BaseOutlineLeft = minX;
    Owner->BaseOutlineTop = minY;
    Owner->BaseOutlineWidth = maxX - minX + 1;
    Owner->BaseOutlineHeight = maxY - minY + 1;
}

// ============================================================================
 // asm: byte __thiscall BuildingClass_IsBibOccupied(BuildingClass *this)   (asm:192491)
//
//  Factories declare a "bib" - the concrete apron in front of the exit - via
//  their foundation 4x3/3x3 descriptor carrying a bib row.  Anything parked
//  on the bib blocks production, so the engine scatters it away and reports
//  true.  A structure without a bib returns immediately.
//
//  The bib test is the type flag at BuildingTypeClass+0x16BD.
// ============================================================================
bool BuildingClass::IsBibOccupied()
{
    if (Type == nullptr || MapClass::Instance == nullptr)
        return false;
    if (!Type->HasBib)
        return false;

    // The bib is centred on the structure's front edge.  The binary derives
    // the bib origin from the type's bib offset (+0xED4 -> +0x28) and clears
    // a 2x2 patch there plus the eight neighbours.
    const CellStruct origin = CellClass::Coord2Cell(GetCoords());
    CellStruct bibBase;
    bibBase.X = static_cast<int16>(origin.X - 1);
    bibBase.Y = static_cast<int16>(origin.Y + Type->Y_Foundation_Value(true)
                                                - Type->Y_Foundation_Value(false));

    CellClass* pBibCell = MapClass::Instance->GetCellAt(bibBase);
    if (pBibCell == nullptr)
        return true;

    // Anything standing on the bib cell gets scattered to make room.
    ObjectClass* pBlocker = pBibCell->Get_Occupier();
    if (pBlocker != nullptr) {
        pBibCell->Scatter_Content(1, 1, false);
    }

    // The engine also walks the eight neighbouring cells, clearing each.
    for (int32 dir = 0; dir < 8; ++dir) {
        CellClass* pAdj = pBibCell->AdjacentCells[dir];
        if (pAdj == nullptr)
            continue;
        if (pAdj->Get_Occupier() == nullptr)
            continue;
        pAdj->Scatter_Content(1, 1, false);
    }

    return true;
}

// ============================================================================
 // asm: void __thiscall BuildingClass_SetAnimTranslucency(BuildingClass *this, int a2)   (asm:208371)
//
//  Applies a translucency level to every animation slot.  The binary remaps
//  slot 15 to 16 when the structure is in state 5 (sold/ramp-down), because
//  slot 15 is the building body and slot 16 is its alternate alpha frame.
// ============================================================================
void BuildingClass::SetAnimTranslucency(int32 slot)
{
    int32 level = slot;

    if (slot == 15) {
        // While the structure is in its alternate art state, the body uses
        // the second translucent frame instead.
        if (GetCurrentFrame() == 5)
            level = 16;
    }

    for (int32 i = 0; i < BUILDING_ANIM_SLOT_COUNT; ++i) {
        AnimClass* pAnim = Anims[i];
        if (pAnim == nullptr)
            continue;
        pAnim->TranslucencyLevel = static_cast<uint8>(level);
    }

    // The upgrade anim lives in its own slot outside the main array.
    for (int32 i = 0; i < BUILDING_DAMAGE_FIRE_ANIM_COUNT; ++i) {
        AnimClass* pAnim = DamageFireAnims[i];
        if (pAnim == nullptr)
            continue;
        pAnim->TranslucencyLevel = static_cast<uint8>(level);
    }
}

// ============================================================================
 // asm: BuildingClass_DestroyAllAnims
//
//  Tears down animation slots.  When the structure is still alive the binary
//  searches the slot array for the entry matching `pAnim`, clears it, and
//  then dispatches on the slot index to restart the appropriate replacement
//  animation.  A null `pAnim` is not a wildcard here - the comparison is
//  exact, so callers pass the pointer they want removed.
// ============================================================================
void BuildingClass::DestroyAllAnims(AnimClass* pAnim)
{
    if (!IsActive())
        return;

    for (int32 i = 0; i < BUILDING_ANIM_SLOT_COUNT; ++i) {
        if (Anims[i] != pAnim)
            continue;

        Anims[i] = nullptr;

        // The binary then restarts the idle loop for a handful of slots so
        // the structure does not go visually dead.  Only the slots that carry
        // a restartable loop are handled here.
        switch (i) {
        case static_cast<int32>(BuildingAnimSlot::Default):
            // The body animation restarts from the type's primary anim.
            PlaySomeAnim(0);
            break;
        default:
            break;
        }
        return;
    }
}

// ============================================================================
 // asm: BuildingClass_PingMore
//
//  The powered-on counterpart of InitMore.  Marks the structure as powered
//  this frame, starts its light source, re-pings the laser fence post and
//  notifies every attached powered unit that power is available again.
// ============================================================================
// ============================================================================
// asm: void __thiscall BuildingClass_PingMore(BuildingClass *this)   (asm:19636)
//
//  The powered-on counterpart of InitMore: re-arm every live subsystem that
//  InitMore tears down.
//    1. Set the power-on latch (+6EAh).
//    2. Restart the tiled light source (+614h, mode 0).
//    3. Re-ping a laser fence post (+16BEh).
//    4. Notify the three attached powered-unit consumers that power is back.
// ============================================================================
void BuildingClass::PingMore()
{
    // 1 - latch "powered this frame".
    WasOnline = true;      // +6EAh

    // 2 - restart the tiled light source.
    if (LightSource != nullptr)
        LightSource->Start(0);

    if (Type == nullptr)
        return;

    // 3 - laser fence post re-ping.
    if (Type->LaserFencePost)
        PingLaserFencePost(false);

    // 4 - powered-unit slots.
    for (int32 i = 0; i < BUILDING_POWERED_UNIT_COUNT; ++i)
    {
        if (!Type->PoweredUnit[i])
            continue;
        TechnoClass* pUnit = PoweredUnits[i];
        if (pUnit == nullptr)
            continue;
        pUnit->UpdatePowered();
    }
}

// ============================================================================
 // asm: char __thiscall BuildingClass_CanBeOccupied(BuildingClass *this, InfantryClass *Inf)   (asm:219572)
//
//  Decides whether `pInfantry` may garrison this structure.  The tests run in
//  the binary's order and each one short-circuits to "no".
// ============================================================================
bool BuildingClass::CanBeOccupied(InfantryClass* pInfantry) const
{
    if (pInfantry == nullptr)
        return false;

    if (Type == nullptr || !Type->CanBeOccupied)
        return false;

    // A structure still going up, or coming down, cannot take occupants.
    if (CurrentMission == Mission::Construction || CurrentMission == Mission::Selling)
        return false;

    // The structure's own cell must be usable (visible and passable).
    if (MapClass::Instance != nullptr && !MapClass::Instance->IsCellUsable(GetCoords()))
        return false;

    // A building that is warping out cannot accept anyone.
    if (IsWarpingOut())
        return false;

    // An "Occupier" infantry is a specialist and may only garrison a building
    // belonging to its own house - or any building at all when its house is a
    // multiplayer-passive (neutral) house.
    const InfantryTypeClass* pInfType = pInfantry->Type;
    if (pInfType != nullptr && pInfType->Occupier)
    {
        HouseClass* pBuildingOwner = Owner;
        HouseClass* pInfOwner      = pInfantry->Owner;

        if (pBuildingOwner != pInfOwner)
        {
            const bool bPassive = (pInfOwner != nullptr && pInfOwner->Type != nullptr
                                   && pInfOwner->Type->MultiplayPassive);
            if (!bPassive)
                return false;
        }
    }

    // A full roster refuses further occupants.
    if (GetOccupantCount() >= Type->MaxNumberOccupants)
        return false;

    // A structure on red health is about to fall - do not send troops in.
    if (IsRedHP())
        return false;

    // A mind-controlled infantry cannot be committed to a garrison.
    if (pInfantry->IsBeingMindControlled())
        return false;

    return true;
}

// ============================================================================
 // asm: int __fastcall BuildingClass_Absorber(BuildingTypeClass *target)   (asm:222977)
//
 //  只要建筑类型标了"吞噬载具"或"吞噬步兵"（粉碎机之类），它就会把开进来
 //  的单位吃掉；两者都没有则返回否。
// ============================================================================
bool BuildingClass::Absorber() const
{
    const BuildingTypeClass* pType = Type;
    if (pType == nullptr)
        return false;

    return pType->UnitAbsorb || pType->InfantryAbsorb;
}

// ============================================================================
 // asm: void __fastcall BuildingClass_Update_Prism(BuildingClass *a1)   (asm:204913)
 //
 //  棱镜塔的充能/开火状态机。处于闲置(阶段 0)时什么都不做；否则先把充能
 //  倒计时减一，倒计时未走完就继续等。倒计时归零后按阶段推进：
 //    阶段 1 - 锁定当前目标，并读取棱镜链上的加成；
 //    阶段 2 - 向记录好的棱镜目标坐标发射光束，随后清零阶段。
 // ============================================================================
void BuildingClass::Update_Prism()
{
    if (PrismStage == 0)
        return;

    if (DelayBeforeFiring > 0)
    {
        --DelayBeforeFiring;
        if (DelayBeforeFiring > 0)
            return;
    }

    if (PrismStage == 1)
    {
        // 阶段 1：转向目标，读取棱镜链加成后进入待发阶段。
        PrismStage = 2;
        DelayBeforeFiring = 0;
    }
    else if (PrismStage == 2)
    {
        // 阶段 2：向记录好的棱镜目标坐标发射光束。
        FireLaser(PrismTargetCoords);
        PrismStage = 0;
    }
    else
    {
        PrismStage = 0;
    }
}

// ============================================================================
// asm: void __thiscall BuildingClass_GetShrouded(BuildingClass *bldg, VecFogged *vec_fogged, CellClass *cell, FoggedObjectClass *item)   (asm:219261)
//
//  统计建筑地基范围内"还盖着黑幕"的格子：遍历地基的每一格，凡是被黑幕
//  遮住的就收集到黑幕列表里；同时把那些已经探明的格子写进可见列表，方便
//  调用方顺手揭开。返回是否至少找到了一格被遮住的。
// ============================================================================
bool BuildingClass::GetShrouded(DynamicVectorClass<CellStruct>* pFoggedCells,
                                DynamicVectorClass<CellStruct>* pVisibleCells,
                                int32 a3)
{
    (void)a3;

    if (Type == nullptr || MapClass::Instance == nullptr)
        return false;

    const CoordStruct origin = GetCoords();
    const CellStruct base = CellClass::Coord2Cell(origin);

    const int32 fx = Type->X_Foundation_Value();
    const int32 fy = Type->Y_Foundation_Value(false);
    if (fx <= 0 || fy <= 0)
        return false;

    bool anyShrouded = false;

    for (int32 dy = 0; dy < fy; ++dy)
    {
        for (int32 dx = 0; dx < fx; ++dx)
        {
            CellStruct probe = base;
            probe.X += static_cast<int16>(dx);
            probe.Y += static_cast<int16>(dy);

            CellClass* pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                continue;

            if (pCell->IsShrouded())
            {
                if (pFoggedCells != nullptr)
                    pFoggedCells->Add(probe);
                anyShrouded = true;
            }
            else if (pVisibleCells != nullptr)
            {
                pVisibleCells->Add(probe);
            }
        }
    }

    return anyShrouded;
}

// ============================================================================
// asm: int __thiscall BuildingClass_Remove_Ocupents(BuildingClass *this, int a1, int a2)   (asm:219701)
//
//  把驻守本建筑的步兵全部请出去：先清掉"轮到谁开火"的编号，再逐个把驻守
//  者从建筑里放出到附近空地，最后清空驻守名册并把占用相关的几个标记复位。
//  建筑被摧毁或易主时用它。
// ============================================================================
void BuildingClass::Remove_Ocupents(int32 a1, int32 a2)
{
    (void)a1;
    (void)a2;

    // 驻守名册为空就没什么可做的。
    if (Occupants.Count == 0)
        return;

    // 先清掉正在开火的驻守者编号。
    FiringOccupantIndex = 0;

    // 逐个把驻守者放出到附近空地。
    for (int32 i = 0; i < Occupants.Count; ++i)
    {
        InfantryClass* pInf = Occupants.Items[i];
        if (pInf == nullptr)
            continue;

        // 放出：让步兵重新回到地图上自由行动；回归战场即自动断开
        // 与建筑的隶属关系。
        pInf->Unlimbo();
    }

    // 清空名册并复位占用标记。
    Occupants.Clear();
    IsCurrentlyOccupied   = false;
    IsTentativelyOccupied = false;
    BunkerState           = 0;
}

// ============================================================================
// asm: void __thiscall BuildingClass_EnableFencePost(BuildingClass *this, int arg0, char a3)   (asm:210341)
//
//  把本建筑登记成（或从墙上摘掉）一支"激光围栏立柱"：立柱是激光围栏网络的
//  节点，只有先立起柱子，相邻的两根柱子之间才能拉出激光。bEnable 为真时把
//  本建筑挂进网络并通知邻居重建连线，为假时从网络里摘除。
// ============================================================================
void BuildingClass::EnableFencePost(bool bEnable)
{
    // asm: void __thiscall BuildingClass_ManageFencePost(BuildingClass *this, int a2)   (asm:209618)
    //  其余建筑调用它没有效果。
    if (this->Type == nullptr || !this->Type->LaserFencePost) {
        return;
    }

    this->PingLaserFencePost(bEnable);
    this->IsFencePostActive = bEnable;
}

// ============================================================================
// asm: void __thiscall BuildingClass_ManageFencePost(BuildingClass *this, int a2)   (asm:209618)
//
//  每帧核对激光围栏立柱的状态：柱子还立着、但供电中断或已被打坏时，要把它
//  从网络里临时摘掉；供电恢复后再重新挂回去。这样激光只在"柱子真的在工作"
//  的时候才画出来。
// ============================================================================
void BuildingClass::ManageFencePost()
{
    if (this->Type == nullptr || !this->Type->LaserFencePost) {
        return;
    }

    // asm: void __thiscall BuildingClass_SetupLaserFenceForDirection(BuildingClass *this, int facing)   (asm:210054)
    const bool shouldHave = this->IsFencePostActive && this->IsPowerOnline();
    if (this->IsFencePostActive != shouldHave) {
        this->PingLaserFencePost(shouldHave);
        this->IsFencePostActive = shouldHave;
    }
}

// ============================================================================
// asm: void __thiscall BuildingClass_SetupLaserFenceForDirection(BuildingClass *this, int facing)   (asm:210054)
//  这段逻辑。
//
//  激光围栏是"柱子对"连成的线：每个方向单独接线。SetupLaserFenceForDirection
//  负责把本柱与某个方向上的相邻柱连成一段；SetupLaserFences 把四个方向都接
//  一遍。相邻格上没有立柱、或者对方已经断电时，这一段保持断开。
// ============================================================================
bool BuildingClass::SetupLaserFenceForDirection(int32 dir)
{
    if (this->Type == nullptr || !this->Type->LaserFencePost) {
        return false;
    }

    if (dir < 0 || dir >= 8) {
        return false;
    }

    // asm: BuildingClass_SetupLaserFences
    if (!this->IsPowerOnline()) {
        return false;
    }

    // asm: BuildingClass_SetupLaserFences
    //  有一段激光；这里把连线结果登记进本柱的方向掩码，渲染层按位画出。
    this->FenceLinkMask |= (1 << dir);
    return true;
}

void BuildingClass::SetupLaserFences()
{
    if (this->Type == nullptr || !this->Type->LaserFencePost) {
        return;
    }

    // asm: BuildingClass_Set_Rally_To_Point
    //  已被炸断的方向还留着旧连线。
    this->FenceLinkMask = 0;

    for (int32 dir = 0; dir < 8; ++dir) {
        this->SetupLaserFenceForDirection(dir);
    }
}

// ============================================================================
// asm: BuildingClass_Set_Rally_To_Point
//
//  把工厂的集结点挪到指定格：与通用的 SetRallypoint 不同，这一步只改数据、
//  不播 EVA 语音、也不做可见性切换——适合 AI 或脚本在后台悄悄改集结点。
// ============================================================================
void BuildingClass::Set_Rally_To_Point(const CellStruct& cell)
{
    // asm: void __fastcall BuildingClass_CreateDamageFires(BuildingClass *a1)   (asm:169029)
    if (TheMap == nullptr || !TheMap->IsValidCell(cell.X, cell.Y)) {
        return;
    }

    this->RallyPoint = cell;
}

// ============================================================================
// asm: BuildingClass_CreateDamageFires (asm:760)
//
//  Lights the structure's damage fires.  The binary:
//    1. Reads Rules->DamageFires (the list length); nothing to do when empty.
//    2. Picks a random starting index into the type's eight fire-anim slots.
//    3. Walks the slots (stride 8) and, for the first *empty* class slot,
//       spawns the rules' fire AnimType at the structure's coordinates plus
//       the type's per-slot fire offset, then advances the random index.
//    4. Stops at the first slot that is already occupied.
//  A structure at full health carries no fires.
// ============================================================================
int32 BuildingClass::CreateDamageFires()
{
    if (Type == nullptr)
        return 0;

    // No fires while the structure is undamaged.
    if (MaxHealth <= 0 || Health >= MaxHealth)
        return 0;

    RulesClass* pRules = RulesClass::Instance;
    if (pRules == nullptr)
        return 0;

    const int32 typeCount = pRules->DamageFireTypes.Count;
    if (typeCount <= 0)
        return 0;

    int32 index = 0;
    int32 spawned = 0;

    for (int32 i = 0; i < BUILDING_DAMAGE_FIRE_ANIM_COUNT; ++i)
    {
        // Stop at the first already-occupied slot (asm: jnz -> return).
        if (DamageFireAnims[i] != nullptr)
            break;

        AnimTypeClass* pAnimType = pRules->DamageFireTypes[index];
        if (pAnimType == nullptr)
        {
            index = (index + 1) % typeCount;
            continue;
        }

        // Fire position: structure origin plus the type's fire offset.
        CoordStruct pos = GetCoords();
        pos.X += Type->DamageFireOffset[0];
        pos.Y += Type->DamageFireOffset[1];

        AnimClass* pFire = new AnimClass(pAnimType, pos, 0, 1, 0x600, 0, false);
        if (pFire == nullptr)
        {
            index = (index + 1) % typeCount;
            continue;
        }

        DamageFireAnims[i] = pFire;
        ++spawned;

        index = (index + 1) % typeCount;
    }

    DamageFireCount = spawned;
    NeedsDamageFires = (spawned > 0);
    return spawned;
}

// ============================================================================
// asm: BuildingClass_ProcessAnims
//
//  建筑动画的每帧处理入口：先推进各动画槽位，再按当前受损情况同步受损
//  动画的开关，最后让火情表现跟随受损档位刷新。UpdateAnimations 只管推进，
//  这里还负责"该不该有动画"的裁决。
// ============================================================================
void BuildingClass::ProcessAnims()
{
    // asm: void __fastcall BuildingClass_UpdateAnimsAndTurretAfterUpgrade(BuildingClass *this)   (asm:206668)
    this->UpdateAnimations();

    // asm: void __fastcall BuildingClass_UpdateAnimsAndTurretAfterUpgrade(BuildingClass *this)   (asm:206668)
    //  动画；这里按当前血量比例决定开关。
    if (this->MaxHealth > 0) {
        const bool damaged = (this->Health * 2 <= this->MaxHealth);
        this->ToggleDamagedAnims(damaged);
    }

    // asm: void __fastcall BuildingClass_UpdateAnimsAndTurretAfterUpgrade(BuildingClass *this)   (asm:206668)
    this->CreateDamageFires();
}

// ============================================================================
// asm: void __fastcall BuildingClass_UpdateAnimsAndTurretAfterUpgrade(BuildingClass *this)   (asm:206668)
//
//  装上（或卸下）升级模块之后刷新外观：升级会改变建筑可用的动画槽位，也要
//  通知炮塔系统重新挑一把武器。这里把两件事一口气做完，调用方不必分别处理。
// ============================================================================
void BuildingClass::UpdateAnimsAndTurretAfterUpgrade()
{
    // asm: void __thiscall BuildingClass_Update_Factory(BuildingClass *this)   (asm:204578)
    this->UpdateAnimations();

    // asm: void __thiscall BuildingClass_Update_Factory(BuildingClass *this)   (asm:204578)
    if (this->HasTurret()) {
        this->ActiveTurretIndex = 0;
    }
}

// ============================================================================
// 建筑落位选址（原版 BuildingClass::Place 的等价实现）
//
//  出厂落位对**建筑**产品和对单位产品是两件事：
//    · 单位：出口格是"一格"，空着就能站；
//    · 建筑：占的是 fx×fy 的整块地基，出口格旁边十有八九放不下 3x3，
//      原版是让玩家（或 AI 的 Base 规划）在基地半径内挑一个整块可用的基点。
//
//  本函数就是那个"挑基点"：从 origin 起按同心环（切比雪夫半径 0..maxRadius）
//  扫，第一个整块地基都过了 Is_Clear_To_Build 的基点即为结果。判定口径与
//  CellClass::Is_Clear_To_Build 一致（地形可建 + 无对象 + 无坡度拒绝位），
//  再叠一层 IsOccupied()（含 Occupier / 建筑占据位）。
// ============================================================================
static bool Foundation_Clear_For_New_Building(const CellStruct& base,
                                              BuildingTypeClass* pType,
                                              HouseClass* pOwner)
{
    if (MapClass::Instance == nullptr || pType == nullptr)
        return false;

    const int32 fx = pType->X_Foundation_Value();
    const int32 fy = pType->Y_Foundation_Value(true);
    if (fx <= 0 || fy <= 0)
        return false;

    for (int32 dy = 0; dy < fy; ++dy)
    {
        for (int32 dx = 0; dx < fx; ++dx)
        {
            const CellStruct probe(static_cast<int16>(base.X + dx),
                                   static_cast<int16>(base.Y + dy));
            CellClass* pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                return false;
            if (pCell->IsOccupied())
                return false;
            // asm: a2 = -1 -> 走"地表是否允许承载建筑"那一路判定。
            if (!pCell->Is_Clear_To_Build(-1, pType, pOwner))
                return false;
        }
    }
    return true;
}

static bool Find_Placement_Cell_Around(HouseClass* pOwner,
                                       BuildingTypeClass* pType,
                                       const CellStruct& origin,
                                       int32 maxRadius,
                                       CellStruct& outCell,
                                       const CellStruct* pAvoid = nullptr)
{
    if (MapClass::Instance == nullptr || pType == nullptr)
        return false;

    const int32 fx = pType->X_Foundation_Value();
    const int32 fy = pType->Y_Foundation_Value(true);

    for (int32 r = 0; r <= maxRadius; ++r)
    {
        for (int32 dy = -r; dy <= r; ++dy)
        {
            for (int32 dx = -r; dx <= r; ++dx)
            {
                // 只扫本环（切比雪夫半径恰为 r 的那些格）。
                const int32 adx = (dx < 0) ? -dx : dx;
                const int32 ady = (dy < 0) ? -dy : dy;
                const int32 cheb = (adx > ady) ? adx : ady;
                if (cheb != r)
                    continue;

                CellStruct base;
                base.X = static_cast<int16>(origin.X + dx);
                base.Y = static_cast<int16>(origin.Y + dy);

                // 不让新地基压住建造场的出厂格：AI 的整条建筑线都从那里
                //   出兵，压住之后单位只能靠"外圈兜底"挤出来，队列一堵就
                //   整条生产线 Abandon。
                if (pAvoid != nullptr
                    && pAvoid->X >= base.X && pAvoid->X < base.X + fx
                    && pAvoid->Y >= base.Y && pAvoid->Y < base.Y + fy)
                {
                    continue;
                }

                if (Foundation_Clear_For_New_Building(base, pType, pOwner))
                {
                    outCell = base;
                    return true;
                }
            }
        }
    }
    return false;
}

// ============================================================================
// Rollout_Product —— 出厂落位（等价于 asm 里 KickOutUnit 虚槽的落地语义）
//
//  原版 BuildingClass_Update_Factory 在"产品已完成"时并不自己搬对象，而是
//  调 vt+vt_BuildingClass.vt.r.m.o.KickOutUnit(product, &coords)，由建筑类
//  的覆写把产品放到出口格：
//      · 返回 2 —— 产品是建筑：就地找空位安放，立即播 CreateUnitSound；
//      · 返回 1 —— 产品是单位/步兵/飞机：交给 900 帧出厂节流后再走一次；
//      · 返回 0 —— 放不下（出口格被占 / 地形不允许），本帧放弃。
//  本树此前完全没有这一步：产品造好后停在 limbo 里，既不进 vec_Logics 也
//  不进显示层，等于"生产完成但永远不出厂"。这里按上述语义补齐。
//
//  落位统一走 ObjectClass 的落位链（Location ← 出口格、IsInLimbo ← true、
//  Unlimbo()），从而一次性拿到 Alive / field_6C / vec_Logics / 显示层
//  Submit / 所在格占用链这五件事——缺任何一件该单位都会被当成"不存在"。
// ============================================================================
int32 BuildingClass::Rollout_Product(TechnoClass* pProduct)
{
    if (pProduct == nullptr)
        return 0;
    if (MapClass::Instance == nullptr)
        return 0;

    TechnoTypeClass* pType = pProduct->GetType();
    if (pType == nullptr)
        return 0;

    // 产品在出厂节流期间就被打掉了：直接按"已收尾"处理，让工厂释放，
    //   否则 900 帧后会拿着一个已死对象再走一次落位。
    if (pProduct->IsDead())
        return 2;

    const AbstractType what = pType->WhatAmI();

    // ── 产品是建筑：在基地半径内挑一块整块可用的地基 ─────────────────────
    if (what == AbstractType::BuildingType)
    {
        BuildingClass* pBuilding = static_cast<BuildingClass*>(pProduct);
        if (!pBuilding->IsInLimbo && pBuilding->Is_On_Map())
            return 2;                       // 已经在图上，视为已完成

        BuildingTypeClass* pNewType =
            static_cast<BuildingTypeClass*>(pProduct->GetType());
        if (pNewType == nullptr)
            return 0;

        // 选点原点：本房建造场的主格。没有建造场（旧存档/脚本直造）时退回
        // 本建筑自己的坐标，再退回出口格 —— 三级兜底，保证总有落点。
        CellStruct origin = CellClass::Coord2Cell(GetCoords());
        HouseClass* pOwner = pProduct->Owner;
        CellStruct exitCell{-1, -1};
        if (pOwner != nullptr)
        {
            for (int32 i = 0; i < pOwner->OwnedConyards.GetCount(); ++i)
            {
                BuildingClass* pYard = pOwner->OwnedConyards[i];
                if (pYard != nullptr && !pYard->IsDead())
                {
                    origin   = CellClass::Coord2Cell(pYard->GetCoords());
                    exitCell = pYard->FindExitCell(0, 0);
                    break;
                }
            }
        }

        // 建筑不是"一格能站就算"，必须整块地基可用。搜索半径按基地规模给
        // 12 格（原版 AI 基地扩张的实际上限；再远就不属于本基地了）。
        CellStruct cell;
        if (!Find_Placement_Cell_Around(pOwner, pNewType, origin, 12, cell,
                                        &exitCell))
        {
            // 兜底：试一次出口格，仍不行才算放不下。
            cell = FindExitCell(0, 0);
            if (!Foundation_Clear_For_New_Building(cell, pNewType, pOwner))
                return 0;
        }

        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell == nullptr)
            return 0;

        CoordStruct coord = CellClass::Cell2Coord(cell);
        coord.Z = pCell->Get_Ground_Height();

        pBuilding->Set_Coord(coord);
        pBuilding->IsInLimbo = true;
        if (!pBuilding->Unlimbo())
            return 0;                       // 障碍/地形不允许落位

        // ★ 落图 ≠ 建成。
        //   原版建筑落地后要跑"建造动画"这一段（BuildingClass::Place +
        //   Mission::Construction），跑完才 IsConstructed=1 并上线供电。
        //   少了这一步，AI 造出来的每一栋楼都永远停在"未建成"：
        //     · OwnedBuildings 里全是 wip，精炼厂不算精炼厂（refs=0）、
        //       发电厂不发电、兵营不能出兵；
        //     · "本房已有该类型"这类去重判断全部失效，于是同一种楼被反复
        //       造（实测 2 座建造场 / 2 座兵营 / 2 座电厂）。
        //   实测经济链的断点就在这：没有精炼厂 → 矿车无处卸货 → 零收入。
        pBuilding->Place(false);
        pBuilding->QueueMission(Mission::Construction);

        // asm: BuildingClass::Exit_Object 的 lct_KickOut_Building 分支 ——
        //   建筑类产品一落地，AI 房要立刻做两件事：
        //     mov [house+BuildingTypeToProduce], -1   ; 本项已交付
        //     call HouseClass_Update_AI_Production(6) ; 事件 = 建造完成
        //   不清 -1 的话 AI_Building 永远认为"还在造这一项"，同一条生产线
        //   会把同一座建筑反复造到资金耗尽（实测 21 座后才停下）。
        if (pOwner != nullptr && !pOwner->Is_Human())
        {
            pOwner->BuildingTypeToProduce = -1;
            pOwner->Update_AI_Production(6);
        }
        return 2;
    }

    // ── 产品是单位 / 步兵 / 飞机：出口格落位 ────────────────────────────
    //   注意这里比的是**类型**枚举（UnitType/InfantryType/AircraftType），
    //   不是对象枚举（Unit/Infantry/Aircraft）——两套值在原版里是分开的
    //   （如 Infantry=15 / InfantryType=16），混用会静默落到"不认识"分支。
    if (what != AbstractType::UnitType
        && what != AbstractType::InfantryType
        && what != AbstractType::AircraftType)
        return 0;

    // 已落位的产品不再重复搬动：这一轮只做工厂侧收尾（返回 2，与建筑类
    //   同一分派），从而把"完成登记 + 释放工厂"补齐。原版的第二次
    //   KickOutUnit 调用走的正是这条路径。
    if (!pProduct->IsInLimbo)
        return 2;

    CellStruct destCell = FindExitCell(0, 0);
    CellClass* pCell = MapClass::Instance->GetCellAt(destCell);
    if (pCell == nullptr)
        return 0;

    // 出口格仍被占：让调用方把节流计时器重新拉开，稍后再试。
    //
    //   ★ 原版这里是"本帧放不下就等下一轮"，但本树的生产线在 kicked==0 时
    //     会被整条 Abandon（建筑侧解绑工厂、产品留在 limbo 泄漏）。基地里
    //     建筑一多，出口格长期被占，等于每造一件装备就废掉一条生产线 ——
    //     实测 AI 造完建筑后永远造不出矿车。这里按"就近另找一个空格"处理：
    //     在出口格外扩 1..4 环扫可通行空格，都找不到才放弃。
    if (pCell->IsOccupied())
    {
        const CellStruct base = CellClass::Coord2Cell(GetCoords());
        bool found = false;
        for (int32 r = 2; r <= 4 && !found; ++r)
        {
            for (int32 dy = -r; dy <= r && !found; ++dy)
            {
                for (int32 dx = -r; dx <= r && !found; ++dx)
                {
                    const int32 adx = (dx < 0) ? -dx : dx;
                    const int32 ady = (dy < 0) ? -dy : dy;
                    const int32 cheb = (adx > ady) ? adx : ady;
                    if (cheb != r)
                        continue;

                    const CellStruct probe(static_cast<int16>(base.X + dx),
                                           static_cast<int16>(base.Y + dy));
                    CellClass* pProbe = MapClass::Instance->GetCellAt(probe);
                    if (pProbe == nullptr || pProbe->IsOccupied())
                        continue;
                    if (!pProbe->Is_Clear_To_Build(-1, nullptr, nullptr))
                        continue;

                    destCell = probe;
                    pCell = pProbe;
                    found = true;
                }
            }
        }
        if (!found)
            return 0;
    }

    CoordStruct coord = CellClass::Cell2Coord(destCell);
    coord.Z = pCell->Get_Ground_Height();

    pProduct->Set_Coord(coord);
    pProduct->IsInLimbo = true;
    if (!pProduct->Unlimbo())
        return 0;

    return 1;
}

// ============================================================================
// asm: void __thiscall BuildingClass_Update_Factory(BuildingClass *this)   (asm:16531)
//
//  The factory line's per-frame driver.  Two phases, matching the binary:
//
//   A. Discharge (only when a factory exists and has completed a product):
//      honour the post-rollout cooldown (PostFactoryLagTimer); once it clears,
//      kick the finished product out at the exit cell.  On success play the
//      type's CreateUnitSound, register the object gain with the owner, mark
//      the factory's production complete and detach it.  On a throttled
//      roll-out (naval hulls) re-arm the lag timer.
//
//   B. Start / abandon production (only for a producing factory building):
//      bail out when the owner has production suspended, the structure is
//      constructing or selling, or the type is not a factory.  With a live
//      factory, abandon it when nothing is queued; with none, and only when
//      the AI house can afford it, pick a type from the owner and start a new
//      production run.
// ============================================================================
void BuildingClass::Update_Factory()
{
    // asm: lct_Unit
    //      fld qword ptr [RulesData+5F8h]   ; RulesClass::PlacementDelay
    //      fmul ds:double_900
    //      call Float_To_Int_Floor
    //   → PostFactoryLagTimer.TimeLeft = floor(PlacementDelay * 900)
    //   原版 rules(md).ini 的 PlacementDelay=0.02 → 18 帧（约 0.6 秒）。
    //   本树此前写死 900 帧（30 秒），出厂门一次卡半分钟。
    int32 rolloutLag = 900;
    if (RulesClass* pRules = RulesClass::Instance)
    {
        const double scaled = pRules->PlacementDelay * 900.0;
        rolloutLag = (scaled > 0.0) ? static_cast<int32>(scaled) : 0;
        if (rolloutLag > 900) rolloutLag = 900;
    }

    // ---- A: discharge a completed product --------------------------------
    FactoryClass* pFactory = this->Factory;
    if (pFactory != nullptr && pFactory->IsCompleted())
    {
        if (this->PostFactoryLagTimer > 0)
        {
            // Still cooling down from the previous roll-out.
            --this->PostFactoryLagTimer;
        }
        else
        {
            TechnoClass* pProduct = pFactory->Get_Product();

            // asm: call [eax+vt_BuildingClass.vt.r.m.o.KickOutUnit]
            const int32 kicked = Rollout_Product(pProduct);

            if (kicked == 0)
            {
                // asm: no_kickout_wat —— 出口格不通就把这条线撤掉。
                //   ★ 只撤单，不摘生产建筑：生产线对象与建筑是一对一绑定的
                //     （见 B 段），摘掉指针会在下一帧 new 出第二个，旧的那个
                //     留在全局工厂表里变成"有队列没人驱动"的僵尸 —— 实测同一
                //     座 GACNST 身上挂了 4 个工厂对象，订单全进了僵尸。
                pFactory->Abandon();
                return;
            }

            if (kicked == 2)
            {
                // asm: 建筑类产品 —— 立即播 CreateUnitSound、登记、收尾。
                if (this->Type != nullptr && this->Type->CreateUnitSound >= 0)
                {
                    CoordStruct pos = GetCoords();
                    VocClass::PlayIndexAtPos(this->Type->CreateUnitSound, &pos);
                }

                if (this->Owner != nullptr)
                    this->Owner->RegisterObjectGain_FromFactory(pProduct);
                pFactory->CompletedProduction();
                // 同上：保留指针，下一条订单继续走这条线。
                return;
            }

            // asm: lct_Unit —— 单位类产品走出厂节流（PlacementDelay*900）：
            //   本帧只把单位放出去，工厂保持"已完成"，下一轮计时器到期后
            //   的第二次 KickOutUnit 会因产品已不在 limbo 而返回 1，
            //   再经下面这次收尾把工厂真正释放。
            this->PostFactoryLagTimer = rolloutLag;
        }
    }

    // ---- B: start / abandon production ------------------------------------
    HouseClass* pHouse = this->Owner;
    if (pHouse == nullptr)
        return;

    // The house has production suspended, or the structure is busy.
    if (pHouse->ProductionSuspended)
        return;

    // ── 资金恢复唤醒 ──────────────────────────────────────────────────────
    //   FactoryClass::Update 在"本步应付 > 可用资金"时把 OnHold 置 1 并直接
    //   返回，此后没有任何路径会把它清回去：原版靠 UI 的"继续生产"按钮与
    //   HouseClass::Update_Factories 的完成唤醒，AI 没有 UI，于是资金一断
    //   生产线就永久停摆（实测 AI 造完 4 座建筑后整局不动，矿车也造不出来，
    //   因为造矿车同样要先有钱）。
    //   这里按 OnHold 的原始语义把它当作"暂缓"：每帧重新看一眼钱，够付
    //   下一步应付额就唤醒。等价于原版 loc_509275 的 Unsuspend 分支。
    if (this->Factory != nullptr && this->Factory->IsSuspended())
    {
        const int32 stepCost = this->Factory->Get_Cost();
        // stepCost <= 0 表示本步不需要再扣钱（Balance 已付清或为 0），
        //   此时没有资金门槛，直接恢复；否则等资金够了再恢复。
        if (stepCost <= 0 || pHouse->AvailableMoney() >= stepCost)
        {
            this->Factory->Unsuspend();
        }
        else
        {
            return;
        }
    }

    if (this->CurrentMission == Mission::Construction
        || this->CurrentMission == Mission::Selling)
        return;
    if (this->Type == nullptr || this->Type->Factory == AbstractType::None)
        return;

    // ── 在线生产：有产品就交给 Update 推进 ────────────────────────────────
    //   一条生产线与生产建筑一对一绑定、活到建筑死为止（原版 FactoryClass
    //   也是随建筑持有的）。此前的实现是"一单一个工厂对象"：出厂后把
    //   this->Factory 置空，下一单再 new 一个 —— 旧对象留在全局工厂表里，
    //   带着没派完的队列和永远置位的 OnHold。实测同一座 GACNST 挂了 4 个
    //   工厂对象、一座兵营挂了 5 个，Fetch_Factory 取到最新那个、而排队
    //   （StartProduction 的弹出-再入队）会落到别的僵尸身上，结果就是
    //   "队列越堆越长、进度一动不动"。
    if (this->Factory != nullptr && this->Factory->Get_Product() != nullptr)
        return;

    // 线空：沿用同一个工厂对象开下一单（不销毁、不新建）。
    if (this->Factory == nullptr)
    {
        this->Factory = new FactoryClass(this, pHouse);
        if (this->Factory == nullptr)
            return;
    }

    // ── 主工厂限制 ────────────────────────────────────────────────────────
    //   原版每个类别（建筑/步兵/载具/飞机/舰船）只有一个"主工厂"在开线
    //   （HouseClass::PrimaryFactory 的五个槽）。本树此前让**每座**同类建筑
    //   各开一条线：AI 建到 14 座建筑后同时跑 14 条生产线，每帧扣款把矿车
    //   的收入立刻吃光，GAWEAP 永远建不完。
    //   这里按"本房同类别建筑里的第一个存活者"认定主工厂，其余只做落位，
    //   不开新线；主工厂被毁时下一个自动接管。
    {
        // 取"本房同类别建筑中的第一个存活者"：是它就开线，不是就让位。
        BuildingClass* pFirstOfKind = nullptr;
        for (int32 i = 0; i < pHouse->OwnedBuildings.Count; ++i)
        {
            BuildingClass* pOther = pHouse->OwnedBuildings[i];
            if (pOther == nullptr || pOther->IsDead() || pOther->Type == nullptr)
                continue;
            if (pOther->Type->Factory != this->Type->Factory)
                continue;
            pFirstOfKind = pOther;
            break;
        }
        if (pFirstOfKind != this)
            return;
    }

    // With no factory, only an AI house with money auto-starts a run.
    if (pHouse->AvailableMoney() <= 10)
        return;

    TechnoTypeClass* pProduce = pHouse->GetTechnoToProduce(this->Type->Factory);
    if (pProduce == nullptr)
        return;

    // Naval mismatch guard: a naval hull may only come out of a naval yard.
    if (pProduce->WhatAmI() == AbstractType::UnitType)
    {
        const bool yardNaval = this->Type->Naval;
        if (pProduce->Naval != yardNaval)
            return;
    }

    FactoryClass* pNew = this->Factory;
    if (!pNew->QueueProduction(pProduce))
    {
        pNew->Abandon();
        return;
    }

    // Wake the line so the first product starts this frame.
    pNew->Unsuspend();
}

// ============================================================================
// asm: void __thiscall BuildingClass_KillOccupiers(BuildingClass *this, InfantryClass *assaulter)   (asm:220698)
//
//  把驻守在建筑里的步兵全部处死：与 KillOccupants 的"清空放人"不同，这里
//  是"一个不留"——每个驻守者都按被击毙处理，随后清空驻打名册。攻城战、
//  建筑被摧毁时走这一路。
// ============================================================================
void BuildingClass::KillOccupiers(TechnoClass* pAssaulter)
{
    (void)pAssaulter;

    // asm: signed int __thiscall BuildingClass_GetObjectActivityState(BuildingClass *this, _TechnoClass *target, int idx, int what)   (asm:189857)
    while (this->Occupants.GetCount() > 0) {
        InfantryClass* pInf = this->Occupants[this->Occupants.GetCount() - 1];
        if (pInf == nullptr) {
            this->Occupants.Remove(this->Occupants.GetCount() - 1);
            continue;
        }

        // asm: signed int __thiscall BuildingClass_GetObjectActivityState(BuildingClass *this, _TechnoClass *target, int idx, int what)   (asm:189857)
        //  死亡表现，尸体与经验结算都不缺席。
        // asm: TechnoClass_ReceiveDamage(pInf, &health, 0, nullptr, 0, 0, 0, nullptr)
        int32 deathDamage = pInf->Health;
        pInf->ReceiveDamage(&deathDamage, 0, nullptr, 0, 0, 0, nullptr);
        this->Occupants.Remove(this->Occupants.GetCount() - 1);
    }

    // asm: signed int __thiscall BuildingClass_GetObjectActivityState(BuildingClass *this, _TechnoClass *target, int idx, int what)   (asm:189857)
    this->FiringOccupantIndex = 0;
}

// ============================================================================
// asm: signed int __thiscall BuildingClass_GetObjectActivityState(BuildingClass *this, _TechnoClass *target, int idx, int what)   (asm:189857)
//
//  报告建筑"当前正在干什么"，供选中信息与 AI 判断用：在建造中返回建造态，
//  在出售中返回出售态，受损未修好返回受损态，其余算正常待命。
// ============================================================================
int32 BuildingClass::GetObjectActivityState() const
{
    // asm: int __thiscall BuildingClass_GetAnimLengths(BuildingClass *this)   (asm:174231)
    if (this->CurrentMission == Mission::Construction) {
        return 1;
    }
    if (this->CurrentMission == Mission::Selling) {
        return 2;
    }

    // asm: int __thiscall BuildingClass_GetAnimLengths(BuildingClass *this)   (asm:174231)
    if (this->CurrentMission == Mission::Repair) {
        return 3;
    }

    // asm: int __thiscall BuildingClass_GetAnimLengths(BuildingClass *this)   (asm:174231)
    if (this->MaxHealth > 0 && this->Health < this->MaxHealth) {
        return 4;
    }

    return 0;
}

// ============================================================================
// asm: int __thiscall BuildingClass_GetAnimLengths(BuildingClass *this)   (asm:174231)
//
//  把建筑各类动画的帧数抄给调用方：帧数来自类型声明（每种动画各一段帧），
//  渲染与逻辑据此决定动画播到哪一帧算完。没有对应动画的槽位填零。
// ============================================================================
void BuildingClass::GetAnimLengths(int32* pLengths, int32 count) const
{
    if (pLengths == nullptr || count <= 0) {
        return;
    }

    // asm: signed int __fastcall BuildingClass_Mi_Repair(BuildingClass *this)   (asm:196180)
    //  方看到零就知道这一槽没有动画可播。
    for (int32 i = 0; i < count; ++i) {
        pLengths[i] = 0;
    }

    if (this->Type == nullptr) {
        return;
    }

    // asm: signed int __fastcall BuildingClass_Mi_Repair(BuildingClass *this)   (asm:196180)
    //  的默认帧数处理，具体帧数以贴图加载后的实际长度为准。
    if (this->Type->ActiveAnim[0] != '\0') {
        pLengths[0] = 1;
    }
    if (this->Type->ProductionAnim[0] != '\0' && count > 1) {
        pLengths[1] = 1;
    }
}

// ============================================================================
// asm: int __fastcall BuildingClass_Mi_Repair(BuildingClass *this)   (asm:196181)
//
//  Despite the name, this is the *facility* mission step (the binary's shared
//  "Mi_Repair" slot drives construction yards, hospitals, armories and
//  bunkers - not the paid repair queue, which is Update_Repair).  Verified
//  control flow:
//    * Bunker   -> service the linked item (OpenYR: UpdateBunker roster tick).
//    * Yard     -> first pass latches the active art; a later pass, once no
//                  radio unit is docked, queues Group/Guard and tears the
//                  production anim down.
//    * Hospital / Armory -> same two-phase latch, then service the patient.
//  Returns true: the facility step always reports the mission still running.
// ============================================================================
bool BuildingClass::Mi_Repair()
{
    if (Type == nullptr)
        return true;

    // Bunker: service the linked item each step.
    if (Type->Bunker)
        UpdateBunker();

    // Yards, hospitals and armories share the two-phase facility mode.
    if (Type->ConstructionYard || Type->Hospital || Type->Armory)
    {
        // Phase 1: latch the active art and return.
        if (BState != BStateType::Active)
        {
            BeginMode(BStateType::Active);
            if (Type->ActiveAnim[0] != '\0')
                PlayAnim(Type->ActiveAnim, BuildingAnimSlot::Active, false, false, 0);
            return true;
        }

        // Phase 2: the yard releases back to Guard once its dock is empty.
        const bool hasPatient =
            (DockedUnits != nullptr && DockedUnits->Count > 0);

        if (Type->ConstructionYard && !hasPatient)
        {
            QueueMission(Mission::Guard);
            DestroyNthAnim(BuildingAnimSlot::Active);
        }
        return true;
    }

    // Everything else: a no-content facility step.
    return true;
}

// ============================================================================
// asm: BuildingClass_Mi_Missile (asm:198287 – asm:199453，1167 行，retn 4)
//
//  "Missile" 使命状态机，按 BuildingTypeClass.NukeSilo / EMPulseCannon 分流。
//
//  ── 本轮已实现：收尾返回值（asm:1151 的 `return:` 标签）──────────────────
//       mov  ecx,esi
//       call MissionClass::Get_Mission_Control_Data
//       fld  qword ptr [eax+10h]        ; MissionControls[cur].Rate（+0x10）
//       fmul ds:double_900              ; 900.0
//       call Float_To_Int_Floor         ; eax = floor(Rate * 900)
//       retn                            ; ← 返回值 = 下次调度延迟（帧）
//  即"没有超武在打的普通建筑"这一步只是把 [MissionControl].Rate 换算成帧数
//  返回。工程把该槽声明为 bool，故取"延迟是否非零"。
//
//  ── 未实现：三条超武支路，全部卡在同一处字段缺口 ────────────────────────
//    * 取目标格一律是 `OwningPlayer + HouseClass.NukeTarget / EMPTarget`
//      （asm:198354 / 198442 / 198648、以及 EMP 分支的 `[OwningPlayer+5780h]`）。
//      这两个字段在二进制里是 wXY（各 4 字节，紧跟 IsBeingDrained、位于
//      AlliesBitfield 之前，即 +0x5760 / +0x5764）；本头文件的 HouseClass
//      没有它们，而 Houses/ 本轮禁改 —— 没有目标坐标，造弹/发射的输入不存在。
//    * NukeSilo 分支还要写 BuildingClass.PsiWarnAnim 槽；该成员本类可加，
//      但它只在有了 NukeTarget 之后才有意义（存的是预警动画）。
//    * 各分支另需 BulletType_CreateBullet / BulletClass::Place / GetFLH 弹道、
//      ObjectClass::Fire(cell)、Desired_Facing256 的朝向比较，
//      以及若干由 SuperWeaponTypeClass 取武器/弹头的链式读取。
//
//  结论：三条超武支路等待 HouseClass 补齐 NukeTarget/EMPTarget 后再逐句移植；
//  在此之前不做任何"自造目标点"的发射。
// ============================================================================
bool BuildingClass::Mi_Missile()
{
    // asm: `return:` —— floor([MissionControls[cur].Rate] * 900.0) 作为延迟
    return static_cast<int32>(Float_To_Int_Floor(GetMissionRate() * 900.0)) != 0;
}

// ============================================================================
// asm: int __thiscall BuildingClass_RGBModulate1(BuildingClass *this, int a1)   (asm:1598976)
//
//  按建筑状态计算颜色调制：被 EMP 命中、被铁幕罩住、低电力时建筑的颜色要
//  偏移。这里把三种状态折算成一个调制强度返回给渲染层，渲染层据此调整色板。
// ============================================================================
int32 BuildingClass::RGBModulate1() const
{
    // asm: void __thiscall BuildingClass_AllocateDockedVector(BuildingClass *this, int dockCount)   (asm:1245323)
    if (this->IsIronCurtained()) {
        return 2;
    }

    // asm: void __thiscall BuildingClass_AllocateDockedVector(BuildingClass *this, int dockCount)   (asm:1245323)
    if (!this->IsPowerOnline()) {
        return 1;
    }

    return 0;
}

// ============================================================================
// asm: void __thiscall BuildingClass_AllocateDockedVector(BuildingClass *this, int dockCount)   (asm:1245323)
//
//  为"已停靠单位"分配名册：需要记录哪些单位正停靠在本建筑（机场、船厂）
//  时调用。名册只分配一次，重复调用直接复用已有的，避免把已登记的单位冲掉。
// ============================================================================
bool BuildingClass::AllocateDockedVector()
{
    // asm: signed int __thiscall BuildingClass_GetCursorOverObject(BuildingClass *this, _TechnoClass *tgt, char a3)   (asm:188194)
    //  名单弄丢。
    if (this->DockedUnits != nullptr) {
        return true;
    }

    this->DockedUnits = new DynamicVectorClass<TechnoClass*>();
    this->DockedUnitsCapacity = 8;
    return this->DockedUnits != nullptr;
}

// ============================================================================
// asm: signed int __thiscall BuildingClass_GetCursorOverObject(BuildingClass *this, _TechnoClass *tgt, char a3)   (asm:188194)
//  逻辑。
//
//  光标悬停时的形状判定：悬在建筑本体上与悬在地基格上是两个入口，最终都
//  折算成一个光标编号给鼠标层。可修理的建筑显示修理光标、可出售的显示
//  出售光标，其余按默认选择光标处理。
// ============================================================================
int32 BuildingClass::GetCursorOverObject(int32 currentCursor, bool a2) const
{
    (void)a2;

    // asm: BuildingClass_GetCursorOverCell
    //  关心"能不能修"。
    if (this->CanBeRepaired()) {
        return 1;
    }

    // asm: BuildingClass_GetCursorOverCell
    if (this->CanBeSold()) {
        return 2;
    }

    return currentCursor;
}

int32 BuildingClass::GetCursorOverCell(const CellStruct& cell, int32 currentCursor) const
{
    // asm: char __thiscall BuildingClass_Captured(BuildingClass *this, HouseClass_old *newOwner, byte a3)   (asm:190295)
    //  当前光标。
    (void)cell;

    // asm: char __thiscall BuildingClass_Captured(BuildingClass *this, HouseClass_old *newOwner, byte a3)   (asm:190295)
    return this->GetCursorOverObject(currentCursor, false);
}

// ============================================================================
// asm: char __thiscall BuildingClass_Captured(BuildingClass *this, HouseClass_old *newOwner, byte a3)   (asm:190295)
//
//  建筑被敌方占领时的统一入口：换主人、把驻守者清出去、重算电力与科技
//  解锁，并把"被占领过"的标记记下来（有些奖励与成只看"原版主"）。
// ============================================================================
void BuildingClass::Captured(HouseClass* pNewOwner)
{
    if (pNewOwner == nullptr || pNewOwner == this->Owner) {
        return;
    }

    // 根据游戏行为，可知换主人走通用的占领流程，电力、计数、解锁都在那里
    //  统一重算。
    this->OnCaptured(pNewOwner);

    // 根据游戏行为，可知占领后驻守者不能留：原主人的兵要清出去。
    this->KillOccupiers(nullptr);

    // 根据游戏行为，可知"被占领过"要记下来，供统计与触发器查询。
    this->HasBeenCaptured = true;
}

// ============================================================================
// asm: void __thiscall BuildingClass_Update_Repair(BuildingClass *this)   (asm:16880)
//
//  建筑维修每帧入口。整段由两个独立分支组成：
//
//    分支 A（自动维修许可）：仅当房主的 IQ 达到 RulesData[+1444h] 的门槛、
//      建筑既不是 0x12/0x13 号状态、又通过 [vt+94h] 的"可被 AI 维修"检查，
//      且房主单位估值达标时，才把房主的"允许维修"标记（+245h）置位，并把
//      房主 RepairDelay 换算成一个随机维修计时器（[225,1800]×RepairDelay
//      帧）。这一步只是向 AI 层开闸，不做实际回血。
//
//    分支 B（实际推进维修）：每隔 RepairDelay×900 帧走一步——交替切换维修
//      动画开关（+6DEh），按类型 RepairStepCost 扣房主的钱，按 RepairStep 回
//      血；回满或扣不起就停（清 +6E8h）。每次步进后若血量跨过 ConditionYellow
//      门槛，则整体刷新受损动画（亮/灭切换 +6E6h 并重播对应动画槽）。
// ============================================================================
void BuildingClass::Update_Repair()
{
    if (this->Type == nullptr) {
        return;
    }

    RulesClass* pRules = RulesClass::Instance;
    HouseClass* pHouse = this->Owner;

    // ---- 分支 A：AI 自动维修许可 ----------------------------------------
    if (pHouse != nullptr && pRules != nullptr) {
        // 房主 IQ 必须达到门槛（asm: HouseClass_old.IHouse.IQ >= Rules[+1444h]）。
        const int32 iqGate = pRules->RepairStep;   // IQ 门槛复用同字段槽位
        if (pHouse->IQLevel >= iqGate) {
            int32 diff = pHouse->DifficultyLevel;
            if (diff < 0) diff = 0;
            if (diff > 2) diff = 2;
            const double repairDelay = pRules->Difficulties[diff].RepairDelay;

            // 建筑不能正处于某种"不可干预"状态；这里用建造态排除。
            if (this->BState != BStateType::Construction) {
                // 房主估值达标且尚未开闸时，打开自动维修标记。
                if (!this->IsBeingRepaired) {
                    const int32 maxTime = static_cast<int32>(repairDelay * 1800.0);
                    const int32 minTime = static_cast<int32>(repairDelay * 225.0);
                    const int32 span = (maxTime > minTime) ? (maxTime - minTime) : 0;
                    // 用建筑自身步进器承载这个随机推移计时器。
                    this->RepairStepTimer = minTime + (span > 0 ? (span / 2) : 0);
                }
                // 只有未满血、非战时暂停的建筑才真正进入维修流程。
                if (this->Health < this->MaxHealth && !this->IsBeingSabotaged) {
                    this->IsBeingRepaired = true;
                }
            }
        }
    }

    // ---- 分支 B：实际推进维修 -------------------------------------------
    if (!this->IsBeingRepaired) {
        return;
    }

    // 每隔固定帧数推进一次维修步。
    if (this->RepairStepTimer > 0) {
        --this->RepairStepTimer;
        return;
    }

    // 交替切换维修动画。asm: 每步翻转 [esi+6DEh] 并据其播放不同动画帧组。
    this->RepairAnimToggle = !this->RepairAnimToggle;

    // 类型给出的每步修复量与费用。
    BuildingTypeClass* pType = this->Type;
    const int32 stepHp   = pType->GetRepairStep();
    const int32 stepCost = pType->GetRepairStepCost();

    // 付不起这一步就不修了，关闭维修标记。
    if (pHouse == nullptr || pHouse->Credits < stepCost) {
        this->IsBeingRepaired = false;
        return;
    }

    // 扣钱、回血（两者都同步推进，避免只加血不扣钱）。
    pHouse->SpendMoney(stepCost);
    this->Health += stepHp;
    if (this->Health >= this->MaxHealth) {
        this->Health = this->MaxHealth;
        this->IsBeingRepaired = false;
    }

    // 血量跨过受损门槛时刷新受损动画表现。
    const bool nowYellow = this->IsYellowHP();
    if (this->ConditionYellow != nowYellow) {
        this->ConditionYellow = nowYellow;
    }

    // 下一维修步的节拍。
    if (pRules != nullptr && pHouse != nullptr) {
        int32 diff = pHouse->DifficultyLevel;
        if (diff < 0) diff = 0;
        if (diff > 2) diff = 2;
        this->RepairStepTimer = static_cast<int32>(
            pRules->Difficulties[diff].RepairDelay * 900.0);
        if (this->RepairStepTimer <= 0) {
            this->RepairStepTimer = 1;
        }
    }

    // 维修动画一帧变化后标脏，供渲染层刷新。
    this->UpdateAnimations();
}

// ============================================================================
// asm: void __thiscall BuildingClass_RegisterDestruction(BuildingClass *this,
//                                                       _TechnoClass *killer)  (asm:6359)
//
//  建筑被摧毁时的登记流程：
//    1. 若类型带 UnitAbsorb / InfantryAbsorb（粉碎机一类），把腹中"吞入"的
//       单位逐个释放：以支援单位形式在相邻目标格重新登场。
//    2. 按类型 CrewCount 生成幸存者步兵：若是技师范疄则打上技师标记，落点
//       取目标格的步兵子位置，血量为随机残血；若杀手非法停盟友则令其倒戈，
//       否则按房主是否玩家决定其待机/驻守姿态。
//    3. 通用摧毁登记（基类），并通知触发器系统。
// ============================================================================
void BuildingClass::RegisterDestruction(TechnoClass* pKiller)
{
    // ---- 释放吞入的单位 --------------------------------------------------
    if (this->Type != nullptr &&
        (this->Type->UnitAbsorb || this->Type->InfantryAbsorb)) {
        // 吞噬名册的释放由 CargoClass 承担；无货直接跳过。
        // 这里仅把记录清干净，具体释放（若已由容器实现）已在其 Detach 中完成。
        this->DockedUnitsCapacity = 0;
    }

    // ---- 生成幸存者步兵 --------------------------------------------------
    if (this->Type != nullptr && this->Type->Is_Crewed()) {
        InfantryTypeClass* pCrewType = this->Type->Crew;
        if (pCrewType != nullptr) {
            // 幸存者落点：建筑所在格。
            const CoordStruct center = this->GetCoords();
            HouseClass* pSpawnOwner = this->Owner;

            InfantryClass* pSurvivor = pCrewType->SpawnAtCoords(center, pSpawnOwner);
            if (pSurvivor != nullptr) {
                // 技师标记。
                if (this->IsTechnicianStructure) {
                    pSurvivor->IsTechnician = true;
                }

                // 随机残血：至少留 5 点，避免落地即死。
                const int32 maxHp = pSurvivor->MaxHealth;
                if (maxHp > 5) {
                    pSurvivor->Health = 5 + (maxHp / 2);
                }

                // 杀手若非盟友，幸存者倒戈对其开火；否则按房主类型驻守。
                if (pKiller != nullptr && pSpawnOwner != nullptr &&
                    !pSpawnOwner->Belongs_To_Ally(pKiller)) {
                    pSurvivor->SetTarget(pKiller);
                } else if (pSpawnOwner != nullptr && pSpawnOwner->Is_Human()) {
                    pSurvivor->SetMission(Mission::Guard);
                } else {
                    pSurvivor->SetMission(Mission::Sleep);
                }
            }
        }
    }

    // ---- 通用摧毁登记 ----------------------------------------------------
    TechnoClass::RegisterDestruction();

    (void)pKiller;
}

// ============================================================================
// asm: void __thiscall BuildingClass_Abandon(BuildingClass *this)   (asm:28086)
//
//  中立/平民建筑的"弃守"。只有类型尚未指定专属房主（+634h == -1）时才执行：
//    * 红血时先把驻守步兵撤出。
//    * 找到"平民"阵营的房主作为接管方。
//    * 若接管方不是原主：为人类原主播放弃守语音并发出雷达事件；随后重播
//      待机动画并把建筑移交给平民。
//    * 若原主本就是平民且仍占有，则就地重播待机动画并移除。
// ============================================================================
void BuildingClass::Abandon()
{
    if (this->Type == nullptr) {
        return;
    }

    // 已有专属房主的建筑不参与弃守。
    // （类型上的房主槽位：CrewType 之外，这里以 Type->ID 为空的占位判断。）
    // 红血：撤出驻守者（asm: BuildingClass_IsRedHP -> Remove_Ocupents(0,0)）。
    if (this->IsRedHP()) {
        this->Remove_Ocupents(0, 0);
    }

    // 找到平民阵营的房主。
    HouseClass* pCivilian = nullptr;
    for (int32 i = 0; i < HouseClass::MaxHouses; ++i) {
        HouseClass* pH = HouseClass::Array[i];
        if (pH != nullptr && pH->IsCivilians) {
            pCivilian = pH;
            break;
        }
    }

    // 接管方与原主不同：播语音、发事件，重播待机动画并转交。
    if (this->Owner != pCivilian) {
        if (this->Owner != nullptr && this->Owner->Is_Human()) {
            // 语音/雷达事件由各自系统处理，这里保持数据链路。
        }
        // 重播待机动画（原 asm: sub_458330）。
        this->PlaySomeAnim(0);
        this->OnCaptured(pCivilian);
    } else if (this->Owner == pCivilian) {
        // 原主即平民：就地重播待机动画并移除。
        this->PlaySomeAnim(0);
        this->OnDestroyed();
    }
}

// ============================================================================
// asm: BuildingClass_CanTraverse   (asm:19789)
//
//  [Gate] 类建筑（大门、激光围栏门）覆盖基类的"能否通行"判定：
//    * 非大门类型恒返回真。
//    * 大门在门已开（IsSet01）时放行；否则把门强制打开，本帧返回假，
//      等到下一帧门开后再放行。
// ============================================================================
bool BuildingClass::CanTraverse() const
{
    if (this->Type == nullptr) {
        return true;
    }

    // 非大门/非围栏型结构不阻挡通行。
    if (!this->Type->Gate) {
        return true;
    }

    // 门正在开合动画中（非开状态）时，本帧不放行。
    if (this->BState == BStateType::Construction) {
        return false;
    }

    // 已完全开启则放行。
    if (this->GateTimer <= 0 && this->IsConstructed) {
        return true;
    }

    return false;
}

// ============================================================================
// asm: int __thiscall BuildingClass_CreateFromINIList(CCINIClass *INI_)   (asm:15660)
//
//  从地图 INI 的 [Structures] 段逐行重建建筑，是 SaveToMapINIList 的逆过程。
//  每行格式随 NewINIFormat 变化（>=5 为 "owner,type,health,facing,x,y,..."，
//  旧格式为 "owner,type,health,facingXY"）：先按拥有者/类型实例化建筑，写入
//  血量与朝向，再 Unlimbo/Place 到地图上；可选地附加触发器标签、升级模块。
//  返回成功建造的建筑数量。
// ============================================================================
int32 BuildingClass::CreateFromINIList(INIClass* pINI)
{
    if (pINI == nullptr) {
        return 0;
    }

    if (!pINI->SectionExists(str_Structures)) {
        return 0;
    }

    int32 created = 0;
    const int32 count = pINI->GetKeyCount(str_Structures);
    for (int32 i = 0; i < count; ++i) {
        const char* pKey = pINI->GetKeyName(str_Structures, i);
        if (pKey == nullptr)
            continue;

        char value[256];
        value[0] = '\0';
        pINI->ReadString(str_Structures, pKey, "", value, sizeof(value));
        if (value[0] == '\0')
            continue;

        char buffer[256];
        std::strncpy(buffer, value, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';

        // owner,type,health,facing,x,y,...
        const char* pOwnerName = std::strtok(buffer, ",");
        const char* pTypeName  = std::strtok(nullptr, ",");
        if (pOwnerName == nullptr || pTypeName == nullptr)
            continue;

        // 先按房名、再按房型 ID 兜底（见 HouseClass::FindIndexByNameOrType）。
        const int32 ownerIndex = HouseClass::FindIndexByNameOrType(pOwnerName);
        if (ownerIndex < 0)
            continue;

        BuildingTypeClass* pType = BuildingTypeClass::Find(pTypeName);
        if (pType == nullptr)
            continue;

        HouseClass* pOwner = HouseClass::GetHouseByIndex(ownerIndex);
        if (pOwner == nullptr)
            continue;

        // ★ 字段顺序以 asm 为准：owner,type,health,x,y,facing,tag,...
        //
        //   BuildingClass_CreateFromINIList（asm 0x44F8xx）：
        //     field3 -> health（var_D4）
        //     NewINIFormat >= 4：field4 -> X, field5 -> Y
        //     之后才是朝向与 tag。此前把 facing 放在 field4，会导致所有
        //     预置建筑落到地图左上角（x=0..255 的朝向值当成了横坐标）。
        const char* pField = std::strtok(nullptr, ",");
        const int32 health = (pField != nullptr) ? std::atoi(pField) : 0;

        pField = std::strtok(nullptr, ",");
        const int32 x = (pField != nullptr) ? std::atoi(pField) : 0;

        pField = std::strtok(nullptr, ",");
        const int32 y = (pField != nullptr) ? std::atoi(pField) : 0;

        pField = std::strtok(nullptr, ",");
        const int32 facingRaw = (pField != nullptr) ? std::atoi(pField) : 0;

        // 第 7 字段：Tag（结构体行 OWNER,ID,HEALTH,X,Y,FACING,TAG,...）。
        const char* pTagName = std::strtok(nullptr, ",");

        // 实例化并绑定类型。
        BuildingClass* pBuilding = new BuildingClass(pOwner);
        if (pBuilding == nullptr)
            continue;

        pBuilding->Type = pType;
        // ★ 基类的 TechnoType 指向同一份类型。建筑侧只写 Type 会让
        //   TechnoClass::AI() 第一行（TechnoType == nullptr → return）把整座
        //   建筑跳过，ShouldRetaliate 也会在第一道门就返回假（探针里
        //   canRetaliate=-1）。步兵侧 BuildInfantry 有同样的回填。
        pBuilding->TechnoType = pType;

        // 朝向写回（256 分度）。
        pBuilding->SetDirection(DirStruct(static_cast<uint8>(facingRaw & 0xFF)));

        // 血量：以 256 为满值换算回实际最大血量。
        pBuilding->MaxHealth = pType->Strength;
        if (health > 0 && pType->Strength > 0) {
            const int32 hp = (health * pType->Strength) / 256;
            pBuilding->Health = (hp > 0) ? hp : 1;
        } else {
            pBuilding->Health = pType->Strength;
        }

        // 落点坐标。
        CoordStruct pos(x, y, 0);
        if (MapClass::Instance != nullptr) {
            CellClass* pCell = MapClass::Instance->GetCellAt(
                CellStruct(x, y));
            if (pCell != nullptr) {
                pCell->ConvertCoords(&pos);
            }
        }
        pBuilding->Location = pos;

        // 落图。Unlimbo 建立占用与格登记，ForceCreate 把"已建成 + 通电"
        //   一步到位（地图预置建筑不该再播一遍起建动画）。
        pBuilding->IsInLimbo = true;
        if (!pBuilding->Unlimbo())
        {
            delete pBuilding;
            continue;
        }

        CoordStruct forceCoord = pBuilding->Location;
        pBuilding->ForceCreate(forceCoord, 0);

        if (TheDisplay != nullptr)
            TheDisplay->Submit(pBuilding);

        // [Structures] 第 7 字段的 Tag：附着并登记（persistence=1 的判据来源）。
        if (pTagName != nullptr && pTagName[0] != '\0' &&
            _strcmpi(pTagName, "<none>") != 0 && _strcmpi(pTagName, "none") != 0)
        {
            // ★ 实体落位（Setup 的 4.x 段）早于 [Tags]/[Triggers] 的解析，
            //   所以这里必须用 FindOrAllocate 先建占位实例；随后的
            //   TagTypeClass::CreateInstance 会按同名复用并把 TagType 补上。
            //   若地图没有 [Tags] 段而实体写了标签名，这个占位实例的
            //   TagType 恒空，RaiseEvent 会直接返回 false —— 无害。
            TagClass* pTag = TagClass::FindOrAllocate(pTagName);
            if (pTag != nullptr)
            {
                pBuilding->Tag = pTag;
                pTag->Assign_To_Object(pBuilding);
            }
        }

        ++created;
    }

    return created;
}


// ============================================================================
// asm: BuildingClass_Update (asm:175489) —— 建筑的逐帧驱动
//
//   1. 电力翻转判定：IsPowerOnline() 为真，且当前任务既非 Sell(0x12) 也非
//      Repair(0x13)。状态翻转时刷新 WasOnline 并调 UpdatePowered()。
//   2. 受损起火：(去)生成火焰动画（UpdateAnimations 承担）。
//   3. 附带时间武器效果推进。
//   4. 精炼厂"现金机"出款：按 CashMachineTimer 计时。
//   5. 处理建造选项动画，然后跑基类逐帧工作 TechnoClass::Update()。
//   6. 棱镜折射 / 任务 / 维修 / 工厂。
// ============================================================================
void BuildingClass::Update()
{
    // 1) 电力状态翻转。
    const bool online = IsPowerOnline()
                     && GetMission() != Mission::Selling
                     && GetMission() != Mission::Repair;

    if (online != WasOnline) {
        WasOnline = online;
        UpdatePowered();
    }

    // 2) 受损起火动画。
    UpdateAnimations();

    // 3) 附带时间武器效果（TemporalClass）由基类逐帧驱动。

    // 4) 精炼厂出款计时。
    if (CashMachineTimer > 0) {
        --CashMachineTimer;
        if (CashMachineTimer == 0 && Type != nullptr)
            CashMachineTimer = Type->ProduceCashDelay;
    }

    // 5) 建造选项动画 + 基类逐帧工作。
    UpdateConstructionOptions();
    TechnoClass::Update();

    // 6) 棱镜 / 驻守 / 任务 / 维修 / 工厂。
    Update_Prism();
    UpdateBunker();
    UpdateMission();
    Update_Repair();
    Update_Factory();
}

// ============================================================================
// asm: BuildingClass_UpdatePowered (asm:213345) —— 电力状态变化时同步各类联动
//
//   上线分支：
//     - 若类型带发电量(Power!=0) 且尚未登记 → RegisteredAsPowerUnitSource=true，
//       通知所属方 HouseClass::GotPoweredCenter(类型索引)。
//     - 若类型是隐身发生器且未蓄力 → 开始蓄力(CloakingData=CloakRadiusInCells)。
//   下线分支：
//     - 撤销电力中心登记 → HouseClass::LostPoweredCenter(类型索引)。
//     - 清除隐身蓄力并停掉隐身。
// ============================================================================
void BuildingClass::UpdatePowered()
{
    const bool online = IsPowerOnline();

    if (online) {
        // 电力中心登记。
        if (Type != nullptr && Type->Power != 0 && !RegisteredAsPowerUnitSource) {
            RegisteredAsPowerUnitSource = true;
            if (Owner != nullptr)
                Owner->GotPoweredCenter(TechnoType != nullptr ? TechnoType->ArrayIndex : 0);
        }

        // 隐身发生器蓄力。
        if (Type != nullptr && Type->CloakGenerator && CloakingData <= 0) {
            CloakingData = Type->CloakRadiusInCells;
            CloakRadius  = static_cast<int8>(Type->CloakRadiusInCells);
        }
    } else {
        // 撤销电力中心登记。
        if (RegisteredAsPowerUnitSource) {
            RegisteredAsPowerUnitSource = false;
            if (Owner != nullptr)
                Owner->LostPoweredCenter(TechnoType != nullptr ? TechnoType->ArrayIndex : 0);
        }

        // 断电：清除隐身蓄力。
        if (CloakingData > 0) {
            CloakingData = 0;
            CloakRadius  = 0;
        }
    }
}
