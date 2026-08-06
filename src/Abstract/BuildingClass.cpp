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

#include <Abstract/BuildingClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Abstract/TechnoTypeClass.h>
#include <Houses/HouseClass.h>
#include <Game/Game.h>
#include <Core/Definitions.h>

#include <cmath>

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<BuildingClass*>* BuildingClass::Array = nullptr;

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
    , FiringSWType(0)
    , Spotlight(nullptr)
    , GateTimer(0)
    , LightSource(nullptr)
    , HasPower(false)
    , RegisteredAsPoweredUnitSource(false)
    , SupportingPrisms(0)
    , HasExtraPowerBonus(false)
    , HasExtraPowerDrain(false)
    , FiringOccupantIndex(0)
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
    , IsPrimaryFactory(false)
    , BunkerState(0)
    , PrismStage(0)
    , PrismTargetCoords{}
    , DelayBeforeFiring(0)
    , SecretProduction(nullptr)
    , StorageFilledSlots(0)
    , RallyPoint{}
    , Target(nullptr)
    , CurrentMission(Mission::Sleep)
    , QueuedMission(Mission::Sleep)
    , ReservedLayout_538{}
    , IsTentativelyOccupied(false)
    , IsCurrentlyOccupied(false)
    , IsStateChanging(false)
    , IsBeingSabotaged(false)
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
// GetRefund - compute the refund value if the building were sold now.  The
// refund is a fraction of the original cost proportional to remaining health.
// ============================================================================
int32 BuildingClass::GetRefund() const
{
    if (!Type) return 0;
    if (!IsConstructed) return 0;

    int32 baseCost = Type->Cost;
    if (baseCost <= 0) return 0;

    // Health ratio determines the refund fraction.
    double healthRatio = 1.0;
    if (MaxHealth > 0) {
        healthRatio = static_cast<double>(Health) /
                      static_cast<double>(MaxHealth);
        if (healthRatio < 0.0) healthRatio = 0.0;
        if (healthRatio > 1.0) healthRatio = 1.0;
    }

    int32 refund = static_cast<int32>(
        static_cast<double>(baseCost) * (SELL_REFUND_PERCENT / 100.0) * healthRatio);
    if (refund < 0) refund = 0;
    return refund;
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
// Sell - begin the sell process.  The building is marked as not constructed
// and the owner receives a refund proportional to remaining health.
// ============================================================================
void BuildingClass::Sell(DWORD dwUnk)
{
    if (!IsConstructed) return;
    if (!CanBeSold()) return;

    // Compute and grant the refund.
    int32 refund = GetRefund();
    if (refund > 0 && Owner) {
        Owner->GiveMoney(refund);
    }

    // Go offline to stop power contributions immediately.
    GoOffline();

    IsConstructed = false;

    // Remove from the owner's tracking list.
    if (Owner) {
        Owner->Tracking_Remove(this);
    }

    (void)dwUnk; // unused parameter preserved for ABI compatibility.
}

// ============================================================================
// CanBeSold - true if the building type allows selling and the building is
// constructed.
// ============================================================================
bool BuildingClass::CanBeSold() const
{
    if (!Type) return false;
    if (!IsConstructed) return false;
    // Use the BuildingTypeClass method if available, fall back to TechnoType.
    if (Type->IsSellable) return true;
    TechnoTypeClass* pType = static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    return pType->IsSellable && !pType->IsUnsellable;
}

// ============================================================================
// CanBeRepaired - true if the building type allows repair and the building
// is constructed and damaged.
// ============================================================================
bool BuildingClass::CanBeRepaired() const
{
    if (!Type) return false;
    if (!IsConstructed) return false;
    // Only damaged buildings need repair.
    if (MaxHealth > 0 && Health >= MaxHealth) return false;
    TechnoTypeClass* pType = static_cast<TechnoTypeClass*>(static_cast<void*>(Type));
    return pType->IsRepairable;
}

// ============================================================================
// RepairWithMoney - restore HP using the owner's credits.  Each call consumes
// 'money' credits and restores REPAIR_HP_PER_CREDIT * money HP, subject to
// the MaxHealth cap.  The repair is throttled by the FireRechargeTimer field
// to prevent instant healing.
// ============================================================================
void BuildingClass::RepairWithMoney(int32 money)
{
    if (!Type || money <= 0) return;
    if (!IsConstructed) return;
    if (MaxHealth <= 0) return;
    if (Health >= MaxHealth) return;

    // Throttle repair ticks.
    if (FireRechargeTimer > 0) return;

    // Consume credits from the owner.
    if (Owner) {
        if (Owner->Credits < money) {
            // Not enough money; repair what we can afford.
            money = Owner->Credits;
            if (money <= 0) return;
        }
        Owner->SpendMoney(money);
    }

    // Restore health.
    int32 hpRestored = money * REPAIR_HP_PER_CREDIT;
    Health += hpRestored;
    if (Health > MaxHealth) {
        Health = MaxHealth;
    }

    // Update the damaged animation state.
    double healthRatio = static_cast<double>(Health) /
                         static_cast<double>(MaxHealth);
    Animation.IsDamaged = (healthRatio < DAMAGED_THRESHOLD);

    // Set the recharge timer to throttle the next repair tick.
    FireRechargeTimer = REPAIR_TICK_INTERVAL;
}

// ============================================================================
// Repair - alias for RepairWithMoney.
// ============================================================================
void BuildingClass::Repair(int32 money)
{
    RepairWithMoney(money);
}

// ============================================================================
// GoOnline - bring the building online.  Power plants begin producing power;
// factories become available for production.
// ============================================================================
void BuildingClass::GoOnline()
{
    IsOnline = true;
    HasPower = true;
    WasOnline = true;

    // Determine power contribution from the type.
    if (Type) {
        if (Type->Power > 0) {
            IsPowerPlant = true;
            PowerOutput = Type->Power;
            // Low-power state reduces output.
            if (IsOverpowered) {
                PowerOutput = static_cast<int32>(
                    static_cast<double>(PowerOutput) * LOW_POWER_OUTPUT_MULT);
            }
        }
        if (Type->PowerDrain > 0) {
            PowerDrain = Type->PowerDrain;
        }
    }

    // Refresh construction options so the build menu updates.
    UpdateConstructionOptions();
    EnableStuff();
    UpdateAnimations();
}

// ============================================================================
// GoOffline - take the building offline.  Power output drops to zero and
// factories stop accepting production orders.
// ============================================================================
void BuildingClass::GoOffline()
{
    IsOnline = false;
    HasPower = false;
    WasOnline = false;
    IsPowerPlant = false;
    PowerOutput = 0;
    PowerDrain = 0;

    // Stop the animation.
    Animation.IsAnimating = false;
    DisableStuff();
}

// ============================================================================
// GetPowerOutput - report the current power output (0 if offline).
// ============================================================================
int32 BuildingClass::GetPowerOutput() const
{
    if (!HasPower) return 0;
    if (!IsOnline || !IsConstructed) return 0;
    if (IsBeingDrained_) return 0;
    return PowerOutput;
}

// ============================================================================
// GetPowerDrain - report the current power drain (0 if offline).
// ============================================================================
int32 BuildingClass::GetPowerDrain() const
{
    if (!IsConstructed) return 0;
    return PowerDrain;
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
// ClearBunker - evict all garrisoned infantry from the building and reset the
// garrison state flags.  The occupant list is emptied in place; the garrison
// subsystem relocates each ejected infantry to a surrounding cell.
// ============================================================================
void BuildingClass::ClearBunker()
{
    IsCurrentlyOccupied = false;
    IsTentativelyOccupied = false;
    Occupants.Clear();
    FiringOccupantIndex = 0;
    BunkerState = 0;
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

    // Delegate to the TechnoClass fire implementation.
    Fire_Impl(static_cast<AbstractClass*>(static_cast<void*>(pTarget)),
              weaponIndex);

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

    // The WeaponStruct holds a WeaponTypeClass pointer; its range is accessed
    // via the type's Range field.  Here we use the type's GuardRange as a
    // fallback for the firing range.
    if (pType->GuardRange > 0) return pType->GuardRange;
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

// ----------------------------------------------------------------------------
// AddOccupant - add an infantry unit to the building's garrison.  Fails if the
// building is not occupiable, is full, or the infantry is null.
// ----------------------------------------------------------------------------
bool BuildingClass::AddOccupant(InfantryClass* pInfantry)
{
    if (!pInfantry) return false;
    if (!Type) return false;
    if (!Type->IsCanBeOccupied) return false;
    if (Occupants.Count >= Type->OccupyCount) return false;

    Occupants.Add(pInfantry);
    IsCurrentlyOccupied = (Occupants.Count > 0);
    IsTentativelyOccupied = (Occupants.Count > 0);

    // Update animations to show garrisoned state.
    if (IsCurrentlyOccupied) {
        UpdateAnimations();
    }
    return true;
}

// ----------------------------------------------------------------------------
// RemoveOccupant - remove the specified infantry from the garrison.
// Returns false if the infantry was not found.
// ----------------------------------------------------------------------------
bool BuildingClass::RemoveOccupant(InfantryClass* pInfantry)
{
    if (!pInfantry) return false;
    if (Occupants.Count == 0) return false;

    for (int32 i = 0; i < Occupants.Count; ++i) {
        if (Occupants[i] == pInfantry) {
            Occupants.Remove(i);
            // Adjust firing index if needed.
            if (FiringOccupantIndex >= Occupants.Count) {
                FiringOccupantIndex = 0;
            }
            IsCurrentlyOccupied = (Occupants.Count > 0);
            IsTentativelyOccupied = (Occupants.Count > 0);
            UpdateAnimations();
            return true;
        }
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

// ----------------------------------------------------------------------------
// UpdateBunker - per-frame update for garrison state.  Advances the firing
// occupant cycle and checks for occupant ejection.
// ----------------------------------------------------------------------------
bool BuildingClass::UpdateBunker()
{
    if (!Type) return false;
    if (!Type->IsCanBeOccupied) return false;

    if (Occupants.Count == 0) {
        BunkerState = 0;
        return false;
    }

    // Advance the bunker state machine.
    BunkerState = (BunkerState + 1) % 4;
    return true;
}

// ----------------------------------------------------------------------------
// UnloadBunker - eject all occupants to surrounding cells.  The garrison
// subsystem handles the actual relocation; here we clear the tracking state.
// ----------------------------------------------------------------------------
void BuildingClass::UnloadBunker()
{
    if (Occupants.Count == 0) return;

    // Mark all occupants for ejection.  The map subsystem relocates them.
    Occupants.Clear();
    FiringOccupantIndex = 0;
    BunkerState = 0;
    IsCurrentlyOccupied = false;
    IsTentativelyOccupied = false;
    UpdateAnimations();
}

// ----------------------------------------------------------------------------
// EmptyBunker - forcefully empty the garrison without relocation.  Used when
// the building is captured or destroyed.
// ----------------------------------------------------------------------------
void BuildingClass::EmptyBunker()
{
    Occupants.Clear();
    FiringOccupantIndex = 0;
    BunkerState = 0;
    IsCurrentlyOccupied = false;
    IsTentativelyOccupied = false;
}

// ----------------------------------------------------------------------------
// KillOccupants - kill all garrisoned infantry.  Called when the building is
// destroyed by an assaulter.
// ----------------------------------------------------------------------------
void BuildingClass::KillOccupants(TechnoClass* pAssaulter)
{
    (void)pAssaulter;
    if (Occupants.Count == 0) return;

    // In the full binary this calls Kill() on each occupant.  Here we clear
    // the list and let the infantry subsystem handle the death animations.
    Occupants.Clear();
    FiringOccupantIndex = 0;
    BunkerState = 0;
    IsCurrentlyOccupied = false;
    IsTentativelyOccupied = false;
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

// ----------------------------------------------------------------------------
// AfterDestruction - called after the building has been fully destroyed.
// Cleans up power registration, sensor arrays, and garrison.
// ----------------------------------------------------------------------------
void BuildingClass::AfterDestruction()
{
    // Unregister from the power system.
    if (RegisteredAsPoweredUnitSource) {
        RegisteredAsPoweredUnitSource = false;
    }

    // Deactivate sensors and detectors.
    IsSensorActive = false;
    IsDetectorActive = false;

    // Empty the garrison.
    EmptyBunker();

    // Stop all animations.
    for (int32 i = 0; i < BUILDING_ANIM_SLOT_COUNT; ++i) {
        Anims[i] = nullptr;
        AnimStates[i] = false;
    }
}

// ----------------------------------------------------------------------------
// DestroyNthAnim - destroy the animation in the specified slot and free its
// resources.
// ----------------------------------------------------------------------------
void BuildingClass::DestroyNthAnim(BuildingAnimSlot Slot)
{
    int32 idx = static_cast<int32>(Slot);
    if (idx < 0 || idx >= BUILDING_ANIM_SLOT_COUNT) return;

    if (Anims[idx]) {
        Anims[idx] = nullptr;
        AnimStates[idx] = false;
    }
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

// ----------------------------------------------------------------------------
// ToggleDamagedAnims - enable or disable the damaged animation state.
// ----------------------------------------------------------------------------
void BuildingClass::ToggleDamagedAnims(bool isDamaged)
{
    Animation.IsDamaged = isDamaged;
    UpdateAnimations();
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

// ----------------------------------------------------------------------------
// GetSecretProduction - return the secret lab bonus type, if this building is
// a secret lab.
// ----------------------------------------------------------------------------
TechnoTypeClass* BuildingClass::GetSecretProduction() const
{
    if (!Type) return nullptr;
    if (!Type->IsSecretLab) return nullptr;
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

// ----------------------------------------------------------------------------
// MissionConstruction - handle the construction animation and state.
// ----------------------------------------------------------------------------
void BuildingClass::MissionConstruction()
{
    CurrentMission = Mission::Construction;
    BState = BStateType::Construction;
    Animation.IsAnimating = true;
    UpdateAnimations();
}

// ----------------------------------------------------------------------------
// MissionSelling - handle the sell animation and process the refund.
// ----------------------------------------------------------------------------
void BuildingClass::MissionSelling()
{
    CurrentMission = Mission::Selling;
    BState = BStateType::Aux1;
    Sell(0);
    UpdateAnimations();
}

// ----------------------------------------------------------------------------
// MissionRepair - handle the repair mission.  Buildings being repaired gain
// HP over time.
// ----------------------------------------------------------------------------
void BuildingClass::MissionRepair()
{
    CurrentMission = Mission::Repair;
    if (MaxHealth > 0 && Health < MaxHealth) {
        int32 healAmount = 1;
        Health += healAmount;
        if (Health > MaxHealth) Health = MaxHealth;
        UpdateAnimations();
    }
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
    // If a mission is queued, switch to it.
    if (QueuedMission != CurrentMission) {
        CurrentMission = QueuedMission;
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

// ----------------------------------------------------------------------------
// Limbo - remove the building from the map without destroying it.  Used by
// the chronosphere and similar mechanics.
// ----------------------------------------------------------------------------
bool BuildingClass::Limbo()
{
    if (IsInLimbo) return false;
    IsInLimbo = true;
    GoOffline();
    return true;
}

// ----------------------------------------------------------------------------
// Unlimbo - restore a limboed building to the map.
// ----------------------------------------------------------------------------
bool BuildingClass::Unlimbo()
{
    if (!IsInLimbo) return false;
    IsInLimbo = false;
    GoOnline();
    return true;
}

// ----------------------------------------------------------------------------
// InLimbo - return true if the building is currently in limbo.
// ----------------------------------------------------------------------------
bool BuildingClass::InLimbo() const
{
    return IsInLimbo;
}

// ----------------------------------------------------------------------------
// Mark - mark the building's cells on the map (for occupancy tracking).
// ----------------------------------------------------------------------------
void BuildingClass::Mark(MarkType mark)
{
    (void)mark;
    // The full binary updates the map cell occupancy array.
}

// ----------------------------------------------------------------------------
// Unmark - unmark the building's cells from the map.  Clears the cell-
// occupancy tracking so the footprint is freed for other objects.  Also
// resets the redraw mark so the renderer refreshes the terrain beneath the
// former footprint.  Only buildings that are currently on the map (not in
// limbo, not already unmarked) need processing.
// ----------------------------------------------------------------------------
void BuildingClass::Unmark()
{
    if (!IsConstructed) return;
    if (IsInLimbo) return;

    // Clear the occupancy flags that Mark() set.  The full binary walks
    // the building's foundation cells and clears each cell's building-
    // occupancy bit in the MapClass cell array.
    IsCurrentlyOccupied = false;
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
// Draw - draw the building at the specified screen position.  The full binary
// renders the voxel/sprite; here we update the animation state.
// ----------------------------------------------------------------------------
void BuildingClass::Draw(const Point2D& point, const RectangleStruct& rect)
{
    (void)point;
    (void)rect;
    // Advance the animation for rendering.
    UpdateAnimations();
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

// ----------------------------------------------------------------------------
// FindExitCell - find a cell for newly produced units to exit to.  Returns
// an empty cell struct if no suitable cell exists.
// ----------------------------------------------------------------------------
CellStruct BuildingClass::FindExitCell(DWORD dwUnk, DWORD dwUnk2) const
{
    (void)dwUnk;
    (void)dwUnk2;
    CellStruct cell;
    // The full binary searches for a free cell adjacent to the building's
    // dock point.  Here we return the rally point if set.
    return RallyPoint;
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
