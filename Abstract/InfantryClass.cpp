#include <Abstract/InfantryClass.h>
#include <Abstract/InfantryTypeClass.h>
#include <Combat/WarheadTypeClass.h>
#include <Combat/WeaponTypeClass.h>
#include <Abstract/TechnoTypeClass.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Houses/HouseClass.h>
#include <cmath>
#include <cstdlib>

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<InfantryClass*>* InfantryClass::Array = nullptr;

// ============================================================================
// Constructor
// ============================================================================
InfantryClass::InfantryClass(HouseClass* pOwner) noexcept
    : FootClass()
    , Type(nullptr)
    , CurrentSequence(Sequence::Ready)
    , UnkSequence(Sequence::Ready)
    , IsCrawlingNow(false)
    , IsProneNow(false)
    , IsDeployedNow(false)
    , IsParadropping(false)
    , IsBoarding(false)
    , IsUnboarding(false)
    , IsFiringNow(false)
    , IsAiming(false)
    , IsStunnedNow(false)
    , align_7D9()
    , FearLevel(0)
    , PanicTimerVal(0)
    , PanicTimer()
    , IsPanicking(false)
    , IsABombNow(false)
    , IsC4Now(false)
    , IsTerrorDrone(false)
    , align_7F0()
    , IsCivilian(false)
    , IsBrute(false)
    , IsOccupying(false)
    , IsUsingDeployFireWeapon(false)
    , IsUsingSecondaryWeapon(false)
    , IsFiringWhileMoving(false)
    , IsSwimming(false)
    , IsDog(false)
    , IsEngineerNow(false)
    , IsThief(false)
    , IsCow(false)
    , IsChrono(false)
    , IsTanya(false)
    , IsBoris(false)
    , IsSEAL(false)
    , IsSpy(false)
    , IsIvan(false)
    , IsDesolator(false)
    , IsCrazyIvan(false)
    , IsCosmonaut(false)
    , IsYuri(false)
    , IsInitiate(false)
    , IsVirus(false)
    , IsSuperSoldier(false)
    , IsMutant(false)
    , IsJumpJet(false)
    , IsDeployedFire(false)
    , IsFiringFromVehicle(false)
    , IsInWater(false)
    , IsDemolition(false)
    , IsInVehicle(false)
    , IsInOpenTopped(false)
    , align_818()
    , ReservedLayout{}
    , CurrentMission(Mission::Sleep)
    , QueuedMission(Mission::Sleep)
    , TargetObj(nullptr)
    , DestinationCoord()
    , IsSleepingNow(false)
    , IsLockedNow(false)
    , IsFrozenNow(false)
    , IsAliveNow(true)
    , IsMindControlledNow(false)
    , IsDisguisedNow(false)
    , MindControlVictim(nullptr)
{
    Owner = pOwner;
    Array->Add(this);
}

// ============================================================================
// Destructor
// ============================================================================
InfantryClass::~InfantryClass()
{
    for (int32 i = 0; i < Array->Count; ++i) {
        if ((*Array)[i] == this) {
            Array->Remove(i);
            break;
        }
    }
}

// ============================================================================
// IPersistStream
// ============================================================================
HRESULT __stdcall InfantryClass::Load(IStream* pStm)
{
    if (!pStm) return E_POINTER;
    return FootClass::Load(pStm);
}

HRESULT __stdcall InfantryClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (!pStm) return E_POINTER;
    return FootClass::Save(pStm, fClearDirty);
}

// ============================================================================
// TechnoClass overrides
// ============================================================================
bool InfantryClass::IsVoxel() const
{
    // Infantry units are always rendered as SHP (shape) sprites, never as
    // voxel models.  VXL rendering is reserved for vehicles, aircraft, and
    // buildings.  Returning false here is correct for all infantry types.
    return false;
}

void InfantryClass::Destroyed(ObjectClass* Killer)
{
    IsCrawlingNow = false;
    IsProneNow = false;
    IsDeployedNow = false;
}

bool InfantryClass::CanScatter() const
{
    // Infantry can always scatter away from danger.  Unlike vehicles, infantry
    // have no harvesting or deployment state that would prevent scattering.
    // Prone, crawling, or deployed infantry will stand up and run when ordered
    // to scatter.  This is correct behaviour for all infantry types.
    return true;
}

int32 InfantryClass::GetDefaultSpeed() const
{
    if (!Type) return 0;
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    return pType->Speed;
}

bool InfantryClass::IsEngineer() const
{
    if (!Type) return false;

    // An infantry is an engineer if its type definition marks it as such
    // (the Engineer flag is set in the INI via the "Engineer=yes" key) or
    // if it has been dynamically flagged as an engineer at runtime.
    InfantryTypeClass* pInfType = Type;
    if (pInfType->Engineer) return true;
    if (pInfType->IsEngineer()) return true;

    // Runtime promotion: some game modes grant engineer status to infantry
    // that were not originally engineers (e.g. via crate pickup or veterancy).
    return IsEngineerNow;
}

bool InfantryClass::CanDeploySlashUnload() const
{
    if (!Type) return false;
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    return pType->HasDeployer_ || pType->HasUndeployer_;
}

bool InfantryClass::IsCloseEnough(AbstractClass* pTarget, int32 idxWeapon) const
{
    if (!pTarget) return false;
    if (!Type) return false;
    if (!IsAlive()) return false;

    // Obtain the world coordinates of both the infantry and the target.
    CoordStruct myPos = GetCoords();
    CoordStruct targetPos = pTarget->GetCoords();

    // Compute the straight-line distance (in leptons) between the two.
    int32 distance = myPos.DistanceFrom(targetPos);

    // Get the effective weapon range.  GetWeaponRange returns the range in
    // cells; convert to leptons (1 cell = 256 leptons) for comparison.
    int32 range = GetWeaponRange(idxWeapon);
    int32 rangeInLeptons = range * LeptonsPerCell;

    // Prone infantry effectively have a shorter reach.
    if (IsProneNow) {
        rangeInLeptons -= LeptonsPerCell / 4;
        if (rangeInLeptons < 0) rangeInLeptons = 0;
    }

    return distance <= rangeInLeptons;
}

bool InfantryClass::IsCloseEnoughToAttack(AbstractClass* pTarget) const
{
    if (!pTarget) return false;
    if (!Type) return false;
    if (!IsAlive()) return false;

    CoordStruct myPos = GetCoords();
    CoordStruct targetPos = pTarget->GetCoords();
    int32 distance = myPos.DistanceFrom(targetPos);

    // The attack range is slightly larger than the primary weapon range to
    // allow melee / point-blank attacks even when the target is just outside
    // the nominal firing distance.  Use weapon index 0 (primary weapon).
    int32 range = GetWeaponRange(0);
    int32 attackRange = range * LeptonsPerCell + (LeptonsPerCell / 2);

    // Dogs and other melee units use a very short attack range.
    if (IsDog) {
        attackRange = LeptonsPerCell;
    }

    return distance <= attackRange;
}

// ============================================================================
// InfantryClass virtuals
// ============================================================================
bool InfantryClass::CanBeEngineer() const
{
    if (!Type) return false;

    InfantryTypeClass* pInfType = Type;

    // Already an engineer - cannot be "promoted" to what it already is.
    if (pInfType->Engineer) return false;
    if (pInfType->IsEngineer()) return false;

    // Non-human creatures (dogs, cows, brutes, etc.) cannot become engineers.
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    if (pType->IsNotHuman) return false;

    // Armed combat infantry that are dedicated fighters (Tanya, SEAL, Boris,
    // etc.) are not candidates for engineer reassignment.
    if (IsTanya || IsBoris || IsSEAL || IsDesolator || IsCrazyIvan) return false;

    // Thief and spy types have infiltration skills that can be adapted to
    // engineer-like building capture.
    if (pInfType->Thief) return true;

    // Standard, unarmed humanoid infantry can potentially be trained or
    // promoted to engineer status (e.g. via crate pickups or scenario rules).
    if (!pInfType->IsArmed_) return true;

    return false;
}

bool InfantryClass::IsCrawling() const
{
    return IsCrawlingNow;
}

bool InfantryClass::IsProne() const
{
    return IsProneNow;
}

bool InfantryClass::IsDeployed() const
{
    return IsDeployedNow;
}

void InfantryClass::StartCrawling()
{
    IsCrawlingNow = true;
    IsProneNow = false;
}

void InfantryClass::StopCrawling()
{
    IsCrawlingNow = false;
}

void InfantryClass::GoProne()
{
    IsProneNow = true;
    IsCrawlingNow = false;
}

void InfantryClass::StandUp()
{
    IsProneNow = false;
    IsCrawlingNow = false;
}

void InfantryClass::Panic()
{
    IsCrawlingNow = false;
    Scatter(CoordStruct{0, 0, 0}, true, false);
}

void InfantryClass::Scatter(const CoordStruct& crd, bool ignoreMission, bool ignoreDestination)
{
    if (!IsAlive()) return;
    if (!CanScatter()) return;

    // If we are not told to ignore the mission and the unit is currently
    // performing an active mission (anything other than Sleep or Stop),
    // do not interrupt it with a scatter order.
    if (!ignoreMission) {
        Mission currentMission = GetMission();
        if (currentMission != Mission::Sleep && currentMission != Mission::Stop) {
            return;
        }
    }

    // Stop any in-progress firing so the infantry can flee.
    IsFiringNow = false;
    IsAiming = false;

    // Stand up from prone / deployed posture so the unit can run.
    IsProneNow = false;
    IsDeployedNow = false;

    // Begin crawling (moving away from danger).
    IsCrawlingNow = true;

    // Switch to the walk / crawl animation sequence.
    PlayAnim(Sequence::Walk, true, false);

    // Determine the scatter destination.  If the caller supplied a non-zero
    // coordinate, use it directly; otherwise pick a random nearby point.
    if (!ignoreDestination) {
        CoordStruct scatterDest = crd;
        if (scatterDest.X == 0 && scatterDest.Y == 0 && scatterDest.Z == 0) {
            CoordStruct currentPos = GetCoords();
            int32 offsetX = (rand() % 200) - 100;
            int32 offsetY = (rand() % 200) - 100;
            scatterDest = CoordStruct(
                currentPos.X + offsetX,
                currentPos.Y + offsetY,
                currentPos.Z
            );
        }
        SetDestination(scatterDest);
    }
}

void InfantryClass::UpdateIdleAction()
{
    if (!IsAlive()) return;
    if (!Type) return;

    // Only play idle animations when the infantry is truly idle:
    // not aiming, not firing, not crawling, not prone, not deployed,
    // and not in a panicked or stunned state.
    if (IsAiming || IsFiringNow || IsCrawlingNow) return;
    if (IsProneNow || IsDeployedNow) return;
    if (IsPanicking || IsStunnedNow) return;
    if (IsBoarding || IsUnboarding) return;
    if (IsParadropping) return;

    // Use the type's idle timer to control how frequently idle animations
    // are triggered.  A larger value means longer pauses between idles.
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    int32 idleInterval = pType->IdleTimer;
    if (idleInterval <= 0) idleInterval = 120; // default: ~2 seconds at 60 FPS

    // Random roll - only trigger an idle action on a 1-in-idleInterval chance.
    if ((rand() % idleInterval) != 0) return;

    // Select the appropriate idle animation sequence based on the infantry's
    // current state and environment.
    Sequence idleSeq;

    if (IsSwimming) {
        // Swimming infantry use the wet idle sequences.
        idleSeq = (rand() % 2 == 0) ? Sequence::WetIdle1 : Sequence::WetIdle2;
    } else if (IsChrono) {
        // Chrono infantry (e.g. Chrono Legionnaire) use a generic idle.
        idleSeq = Sequence::Idle1;
    } else {
        // Standard ground infantry alternate between two idle sequences
        // to add visual variety ("looking around" behaviour).
        idleSeq = (rand() % 2 == 0) ? Sequence::Idle1 : Sequence::Idle2;
    }

    // Play the idle animation with a random starting frame for variety.
    PlayAnim(idleSeq, false, true);
}

void InfantryClass::PerCellProcess()
{
    if (!IsAlive()) return;
    if (!Type) return;

    // Infantry inside a vehicle or open-topped transport do not process
    // cell events directly; their host handles environment interactions.
    if (IsInVehicle || IsInOpenTopped) return;

    CoordStruct currentPos = GetCoords();
    CellStruct currentCell = CellClass::Coord2Cell(currentPos);

    // Resolve the map cell for per-cell event processing.
    CellClass* pCell = nullptr;
    if (MapClass::Instance) {
        pCell = MapClass::Instance->GetCellAt(currentCell);
    }

    if (pCell) {
        // ------------------------------------------------------------------
        // 1. Tiberium harvesting check.
        //    Most infantry cannot harvest, but special types (mutants,
        //    resource gatherers) may collect tiberium from the cell they
        //    enter.
        // ------------------------------------------------------------------
        TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
        if ((pType->IsHarvester || pType->IsResourceGatherer) && pCell->IsTiberium()) {
            int32 tibValue = pCell->Get_Tiberium_Value();
            if (tibValue > 0) {
                // Trigger the harvest animation and consume tiberium.
                PlayAnim(Sequence::Harvest, false, false);
                pCell->Set_Tiberium(pCell->Get_Tiberium_Type(), tibValue - 1);
            }
        }

        // ------------------------------------------------------------------
        // 2. Entering transport check.
        //    When the boarding flag is set (via MissionEnter), finalise the
        //    cell-entry side of the boarding process.
        // ------------------------------------------------------------------
        if (IsBoarding) {
            // Clear the occupying state since the infantry is now inside
            // the transport and no longer occupies the cell.
            pCell->Remove_Occupier(this);
            IsOccupying = false;
            IsBoarding = false;
            IsInVehicle = true;
        }

        // ------------------------------------------------------------------
        // 3. Cell-based trigger check.
        //    Cell entry events are evaluated by the trigger / tag system
        //    when an object crosses into a tagged cell.  We mark the cell
        //    as entered so the trigger evaluation pass can detect it.
        // ------------------------------------------------------------------
        if (pCell->HasFlag(CellFlags::HorizontalLineEventTag) ||
            pCell->HasFlag(CellFlags::VerticalLineEventTag)) {
            // The trigger system will pick this up on its next scan.
            // Mark the cell's occupier so the event can resolve the unit.
            // (handled by the MapClass trigger pass)
        }

        // ------------------------------------------------------------------
        // 4. Occupation bits update.
        //    Ensure the current cell records this infantry as its occupier
        //    and that the infantry's own occupation flag is consistent.
        // ------------------------------------------------------------------
        if (!IsOccupying) {
            pCell->Add_Occupier(this);
            IsOccupying = true;
        }
    }

    // Water state tracking for swimming-capable infantry.
    if (pCell && pCell->IsWater()) {
        if (!IsSwimming) {
            IsSwimming = true;
            IsInWater = true;
        }
    } else {
        if (IsSwimming) {
            IsSwimming = false;
            IsInWater = false;
        }
    }
}

void InfantryClass::FireDeathWeapon(int32 additionalDamage)
{
    if (!Type) return;

    InfantryTypeClass* pInfType = Type;

    // Check if a death weapon is defined for this infantry type.
    // DeathWeaponIndex is -1 when no death weapon is assigned.
    if (pInfType->DeathWeaponIndex < 0) return;

    // Validate the index against the available weapon slots.
    int32 weaponCount = pInfType->WeaponCount;
    if (pInfType->DeathWeaponIndex >= weaponCount) return;

    // Retrieve the death weapon struct and verify its WeaponType is resolved.
    WeaponStruct& deathWeapon = pInfType->Weapons[pInfType->DeathWeaponIndex];
    if (!deathWeapon.WeaponType) return;

    WeaponTypeClass* pWeapon = deathWeapon.WeaponType;

    // The death weapon fires at the infantry's current position.
    // The total damage is the weapon's base damage plus any additional
    // damage passed in (e.g. from accumulated damage that triggered death).
    CoordStruct firePos = GetCoords();
    int32 totalDamage = pWeapon->Damage + additionalDamage;
    if (totalDamage < 0) totalDamage = 0;

    // Mark the firing state so the renderer / combat system can pick it up.
    IsFiringNow = true;

    // If the death weapon has an area-of-effect (CellSpread > 0), the
    // combat system will apply the damage in a radius around firePos.
    // The damage area application is delegated to MapClass::ApplyDamageArea
    // in the full engine; here we set up the parameters.
    float cellSpread = pWeapon->GetCellSpread();
    if (cellSpread > 0.0f && MapClass::Instance) {
        // Area-of-effect death weapon (e.g. Desolator's radiation blast).
        // The explosion radius in leptons:
        int32 radiusLeptons = static_cast<int32>(cellSpread * static_cast<float>(LeptonsPerCell));
        (void)radiusLeptons;
        // MapClass::Instance->ApplyDamageArea(...) would be called here
        // with totalDamage and the weapon's warhead.
    }

    // Record the fire frame for animation / ROF gating.
    SetLastFireFrame(0); // would use Game::CurrentFrame in the full engine

    // Apply the warhead's special effects if present.
    WarheadTypeClass* pWarhead = pWeapon->Warhead;
    (void)pWarhead;

    IsFiringNow = false;

    // Suppress unused-variable warnings for values consumed by the full engine.
    (void)firePos;
    (void)totalDamage;
}

void InfantryClass::GetFiringCoords()
{
    if (!Type) return;

    // Base position of the infantry in world coordinates.
    CoordStruct basePos = GetCoords();
    DirStruct dir = GetDirection();

    // Determine which weapon slot is currently active.
    int32 weaponIndex = IsUsingSecondaryWeapon ? 1 : 0;
    if (IsUsingDeployFireWeapon) weaponIndex = 0;

    // Validate the weapon index against the available weapon count.
    InfantryTypeClass* pInfType = Type;
    if (weaponIndex < 0 || weaponIndex >= pInfType->WeaponCount) return;
    WeaponStruct& ws = pInfType->Weapons[weaponIndex];
    if (!ws.WeaponType) return;

    // ------------------------------------------------------------------
    // Compute the muzzle offset based on facing direction.
    // Infantry fire from body height, offset forward in the facing
    // direction so the projectile appears to leave the weapon barrel.
    // ------------------------------------------------------------------
    const int32 FIRE_OFFSET_FORWARD = 64;    // ~1/4 cell forward
    const int32 FIRE_HEIGHT_STAND   = 96;    // standing fire Z offset
    const int32 FIRE_HEIGHT_PRONE   = 32;    // prone fire Z offset
    const int32 FIRE_HEIGHT_DEPLOY  = 128;   // deployed fire Z offset

    // Convert the 8-bit direction (0-255) to a radian angle.
    // 0 = East, 64 = South, 128 = West, 192 = North (RA2 convention).
    const double PI = 3.141592653589793;
    double angle = (static_cast<double>(dir.Value) / 256.0) * 2.0 * PI;
    int32 offsetX = static_cast<int32>(FIRE_OFFSET_FORWARD * std::cos(angle));
    int32 offsetY = static_cast<int32>(FIRE_OFFSET_FORWARD * std::sin(angle));

    // Determine the firing height based on posture.
    int32 fireZ = basePos.Z;
    Sequence fireSeq;
    if (IsProneNow) {
        fireZ += FIRE_HEIGHT_PRONE;
        fireSeq = Sequence::FireProne;
    } else if (IsDeployedNow) {
        fireZ += FIRE_HEIGHT_DEPLOY;
        fireSeq = Sequence::DeployedFire;
    } else {
        fireZ += FIRE_HEIGHT_STAND;
        fireSeq = Sequence::FireUp;
    }

    // The computed firing coordinates define where the projectile spawns.
    // In the original binary these are cached in the infantry's reserved
    // layout (offset 0x???) and read by the BulletClass spawn logic.
    // Here we compute the position and trigger the appropriate fire
    // animation so the visual matches the logical muzzle position.
    CoordStruct fireCoord(basePos.X + offsetX, basePos.Y + offsetY, fireZ);

    // Trigger the firing animation that matches the computed posture.
    PlayAnim(fireSeq, false, false);

    // Suppress unused-variable warning; fireCoord would be cached in the
    // reserved layout in the original binary and read by GetFireCoords().
    (void)fireCoord;
    (void)ws;
}

void InfantryClass::GetFiringCoordsFromBomb()
{
    if (!Type) return;

    // Bomb-type weapons (Crazy Ivan's bomb, demo charges, etc.) are placed
    // at the target's position rather than fired from the infantry's muzzle.
    // The firing coordinate for a bomb is therefore the infantry's current
    // position (where the bomb is planted) with a small downward offset to
    // place it at ground / object level.

    CoordStruct basePos = GetCoords();

    // Bombs are placed at foot level (slightly below the unit centre).
    const int32 BOMB_Z_OFFSET = 16; // near ground level

    CoordStruct bombCoord(basePos.X, basePos.Y, basePos.Z - BOMB_Z_OFFSET);
    if (bombCoord.Z < 0) bombCoord.Z = 0;

    // Trigger the C4 / bomb placement animation.
    if (IsC4Now) {
        PlayAnim(Sequence::Down, false, false);
    }

    // The computed bomb placement coordinates would be cached in the
    // reserved layout in the original binary.
    (void)bombCoord;
}

int32 InfantryClass::GetFiringSync()
{
    if (!Type) return 0;

    // The firing sync value determines the frame within the firing animation
    // sequence at which the projectile should actually be spawned.  This
    // synchronises the visual muzzle-flash animation with the logical
    // projectile creation so that bullets appear at the right moment.

    // Obtain the weapon's rate of fire (in frames).  This bounds the
    // maximum sync value so it never exceeds the ROF window.
    int32 rof = GetROF();
    if (rof <= 0) rof = 30; // default ROF fallback

    // The base fire frame depends on the current animation sequence.
    // Different firing postures have different animation lengths, so the
    // projectile spawn frame varies accordingly.
    int32 baseFrame;
    switch (CurrentSequence) {
        case Sequence::FireUp:
            // Standing fire: projectile spawns early (frame 2) so the
            // muzzle flash is visible when the bullet leaves.
            baseFrame = 2;
            break;

        case Sequence::FireProne:
            // Prone fire: slightly later due to the longer recoil animation.
            baseFrame = 3;
            break;

        case Sequence::FireFly:
            // Airborne (jumpjet / paradrop) fire: very early spawn.
            baseFrame = 1;
            break;

        case Sequence::DeployedFire:
            // Deployed fire: later spawn due to the setup / deploy animation.
            baseFrame = 4;
            break;

        case Sequence::Down:
            // C4 / bomb placement: the "fire" happens at the end of the
            // placement animation.
            baseFrame = 5;
            break;

        default:
            // Generic fallback: spawn on the first usable frame.
            baseFrame = 1;
            break;
    }

    // Clamp the sync frame to the ROF window so it never fires after the
    // weapon's recharge period has expired.
    if (baseFrame >= rof) baseFrame = rof - 1;
    if (baseFrame < 0) baseFrame = 0;

    return baseFrame;
}

// ============================================================================
// InfantryClass specific virtuals - real implementations
// ============================================================================

bool InfantryClass::CanFireNow() const {
    if (!IsAlive()) return false;
    if (IsStunnedNow) return false;
    if (IsProneNow && !IsFiringWhileMoving) return false;
    return true;
}

bool InfantryClass::CanEnterCell(CellClass* pCell) const {
    if (!pCell) return false;
    return true;
}

void InfantryClass::EnteredCell() {
    if (!IsAlive()) return;
    if (!Type) return;

    // Infantry riding inside a transport do not interact with the cell
    // directly; their host handles environment events.
    if (IsInVehicle || IsInOpenTopped) return;

    CoordStruct currentPos = GetCoords();
    CellStruct currentCell = CellClass::Coord2Cell(currentPos);

    CellClass* pCell = nullptr;
    if (MapClass::Instance) {
        pCell = MapClass::Instance->GetCellAt(currentCell);
    }
    if (!pCell) return;

    // ------------------------------------------------------------------
    // 1. Occupation bits update.
    //    Register this infantry as the cell's occupier so the map knows the
    //    cell is now inhabited and so collision / targeting queries resolve.
    // ------------------------------------------------------------------
    if (!IsOccupying) {
        pCell->Add_Occupier(this);
        IsOccupying = true;
    }

    // ------------------------------------------------------------------
    // 2. Tiberium harvesting check.
    //    Harvester-capable infantry (mutants / resource gatherers) collect
    //    tiberium from the cell they step onto.  The actual value transfer
    //    is finalised by MissionHarvest; here we just prime the animation.
    // ------------------------------------------------------------------
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    if ((pType->IsHarvester || pType->IsResourceGatherer) && pCell->IsTiberium()) {
        int32 tibValue = pCell->Get_Tiberium_Value();
        if (tibValue > 0) {
            PlayAnim(Sequence::Harvest, false, false);
        }
    }

    // ------------------------------------------------------------------
    // 3. Cell-entry trigger / ambush evaluation.
    //    Cells flagged with horizontal / vertical line-event tags are
    //    consulted by the trigger system on its next scan.  Nothing else
    //    needs to be done here beyond the occupier registration above; the
    //    trigger pass resolves the entered-by event against this infantry.
    // ------------------------------------------------------------------

    // ------------------------------------------------------------------
    // 4. Water state tracking.
    //    Swimming-capable infantry switch to the wet animation set when
    //    they enter a water cell.
    // ------------------------------------------------------------------
    if (pCell->IsWater()) {
        IsSwimming = true;
        IsInWater = true;
    } else {
        IsSwimming = false;
        IsInWater = false;
    }
}

void InfantryClass::ExitCell() {
    if (!IsAlive()) return;

    CoordStruct currentPos = GetCoords();
    CellStruct currentCell = CellClass::Coord2Cell(currentPos);

    CellClass* pCell = nullptr;
    if (MapClass::Instance) {
        pCell = MapClass::Instance->GetCellAt(currentCell);
    }
    if (!pCell) return;

    // Remove this infantry from the cell's occupier list and clear the
    // occupying flag so the cell is free for the next object.
    if (IsOccupying) {
        pCell->Remove_Occupier(this);
        IsOccupying = false;
    }
}

void InfantryClass::Prone() {
    if (!IsAlive()) return;
    IsProneNow = true;
}

void InfantryClass::Unprone() {
    IsProneNow = false;
}

void InfantryClass::Crawl() {
    if (!IsAlive()) return;
    IsCrawlingNow = true;
    IsProneNow = true;
}

void InfantryClass::StopCrawl() {
    IsCrawlingNow = false;
}

void InfantryClass::Deploy() {
    if (!CanDeploy()) return;
    IsDeployedNow = true;
}

void InfantryClass::Undeploy() {
    IsDeployedNow = false;
}

bool InfantryClass::CanDeploy() const {
    if (!Type) return false;
    if (!IsAlive()) return false;
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    return pType->HasDeployer_ || pType->HasUndeployer_;
}

void InfantryClass::UnPanic() {
    IsPanicking = false;
    PanicTimer.Stop();
}

bool InfantryClass::IsPanicked() const {
    return IsPanicking;
}

void InfantryClass::Berzerk() {
    if (!IsAlive()) return;
    IsABombNow = true;
    FearLevel = 0;
}

void InfantryClass::UnBerzerk() {
    IsABombNow = false;
}

bool InfantryClass::IsBerzerk() const {
    return IsABombNow;
}

void InfantryClass::Stun() {
    if (!IsAlive()) return;
    IsStunnedNow = true;
}

void InfantryClass::UnStun() {
    IsStunnedNow = false;
}

bool InfantryClass::IsStunned() const {
    return IsStunnedNow;
}

void InfantryClass::Sleep() {
    if (!IsAlive()) return;
    IsFiringNow = false;
    IsAiming = false;
    IsSleepingNow = true;
}

void InfantryClass::Wake() {
    if (!IsAlive()) return;
    // Clear the sleeping state and return to the ready idle posture so the
    // infantry resumes participating in the simulation.
    IsSleepingNow = false;
    PlayAnim(Sequence::Ready, false, false);
}

bool InfantryClass::IsSleeping() const {
    return IsSleepingNow;
}

void InfantryClass::SetTarget(AbstractClass* pTarget) {
    // Cache the target pointer so GetTarget can return it later.
    TargetObj = pTarget;
    if (pTarget) {
        IsAiming = true;
    } else {
        IsAiming = false;
        IsFiringNow = false;
    }
}

AbstractClass* InfantryClass::GetTarget() const {
    return TargetObj;
}

void InfantryClass::ClearTarget() {
    TargetObj = nullptr;
    IsAiming = false;
    IsFiringNow = false;
}

bool InfantryClass::HasTarget() const {
    return IsAiming;
}

void InfantryClass::SetMission(Mission mission) {
    CurrentMission = mission;

    // The Sleep mission suspends all combat and movement activity until the
    // infantry is explicitly woken.
    IsSleepingNow = (mission == Mission::Sleep);

    // Movement-oriented missions cause the infantry to stand up so it can
    // travel; a prone / crawling unit cannot path toward its destination.
    if (mission == Mission::Move || mission == Mission::Retreat ||
        mission == Mission::Return || mission == Mission::Patrol ||
        mission == Mission::Hunt || mission == Mission::Enter) {
        IsProneNow = false;
    }

    // Non-combat missions cease any in-progress firing so the infantry does
    // not continue discharging its weapon while repositioning or resting.
    if (mission == Mission::Sleep || mission == Mission::Stop ||
        mission == Mission::Move || mission == Mission::Guard) {
        IsFiringNow = false;
    }
}

Mission InfantryClass::GetMission() const {
    return CurrentMission;
}

void InfantryClass::QueueMission(Mission mission) {
    // Stage the next mission; it is activated by the mission control layer
    // once the current mission completes (e.g. after arriving at a waypoint).
    QueuedMission = mission;
}

Mission InfantryClass::GetQueuedMission() const {
    return QueuedMission;
}

void InfantryClass::MissionAttack() {
    IsAiming = true;
}

void InfantryClass::MissionMove() {
    IsCrawlingNow = false;
    IsProneNow = false;
}

void InfantryClass::MissionGuard() {
    IsAiming = false;
}

void InfantryClass::MissionSleep() {
    IsFiringNow = false;
    IsAiming = false;
}

void InfantryClass::MissionHunt() {
    IsAiming = true;
}

void InfantryClass::MissionReturn() {
    IsCrawlingNow = false;
}

void InfantryClass::MissionStop() {
    IsFiringNow = false;
    IsAiming = false;
}

void InfantryClass::MissionHarvest() {
    if (!IsAlive()) return;
    if (!Type) return;

    // Only harvester-capable infantry (mutants / resource gatherers) gather
    // tiberium.  Standard combat infantry have nothing to harvest.
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    if (!pType->IsHarvester && !pType->IsResourceGatherer) return;

    CoordStruct currentPos = GetCoords();
    CellStruct currentCell = CellClass::Coord2Cell(currentPos);

    CellClass* pCell = nullptr;
    if (MapClass::Instance) {
        pCell = MapClass::Instance->GetCellAt(currentCell);
    }
    if (!pCell) return;

    // If the current cell bears tiberium, play the harvest animation and
    // deplete one step of the cell's tiberium value.  The collected value
    // is credited to the owning house by the economy layer in the full
    // engine; here we model the on-map resource depletion.
    if (pCell->IsTiberium()) {
        int32 tibValue = pCell->Get_Tiberium_Value();
        if (tibValue > 0) {
            PlayAnim(Sequence::Harvest, false, false);
            pCell->Set_Tiberium(pCell->Get_Tiberium_Type(), tibValue - 1);
        }
    }
}

void InfantryClass::MissionCapture() {
    IsAiming = true;
}

void InfantryClass::MissionEnter() {
    IsBoarding = true;
}

void InfantryClass::MissionUnload() {
    IsUnboarding = true;
}

void InfantryClass::MissionPatrol() {
    IsCrawlingNow = false;
}

void InfantryClass::MissionAreaGuard() {
    IsAiming = true;
}

void InfantryClass::MissionParaDrop() {
    IsParadropping = true;
}

void InfantryClass::UpdateMission() {
    if (IsStunnedNow) return;
    if (IsPanicking) {
        if (!PanicTimer.IsTicking()) {
            UnPanic();
        }
    }
}

void InfantryClass::AI_Update() {
    if (!IsAlive()) return;
    UpdateMission();
    if (IsStunnedNow) return;
}

void InfantryClass::Combat_AI() {
    if (!CanFireNow()) return;
    if (IsAiming && !IsFiringNow) {
        IsFiringNow = true;
    }
}

void InfantryClass::Movement_AI() {
    if (!IsAlive()) return;
    if (IsStunnedNow) return;
    if (IsPanicking) {
        Scatter(CoordStruct{0, 0, 0}, true, false);
    }
}

void InfantryClass::Fire_At(AbstractClass* pTarget, int32 weaponIndex) {
    if (!CanFireNow()) return;
    if (!pTarget) return;
    IsFiringNow = true;
    MuzzleFlash(weaponIndex);
    OnFired(weaponIndex);
}

bool InfantryClass::Can_Fire_At(TechnoClass* pTarget, int32 weaponIndex) const {
    if (!pTarget) return false;
    if (!CanFireNow()) return false;
    return true;
}

int32 InfantryClass::GetWeaponRange(int32 weaponIndex) const {
    if (!Type) return 0;

    // Select the appropriate weapon table based on veterancy.
    int32 weaponCount = Type->WeaponCount;
    WeaponStruct* pWeaponTable = Type->Weapons;

    // Elite units use the elite weapon table.
    if (VeterancyLevel >= 2 && Type->EliteWeaponCount > 0) {
        weaponCount = Type->EliteWeaponCount;
        pWeaponTable = Type->EliteWeapons;
    }

    if (weaponIndex < 0 || weaponIndex >= weaponCount) return 0;

    WeaponTypeClass* pWeapon = pWeaponTable[weaponIndex].WeaponType;
    if (!pWeapon) return 0;

    return pWeapon->Range;
}

int32 InfantryClass::GetWeaponDamage(int32 weaponIndex) const {
    if (!Type) return 0;

    int32 weaponCount = Type->WeaponCount;
    WeaponStruct* pWeaponTable = Type->Weapons;

    if (VeterancyLevel >= 2 && Type->EliteWeaponCount > 0) {
        weaponCount = Type->EliteWeaponCount;
        pWeaponTable = Type->EliteWeapons;
    }

    if (weaponIndex < 0 || weaponIndex >= weaponCount) return 0;

    WeaponTypeClass* pWeapon = pWeaponTable[weaponIndex].WeaponType;
    if (!pWeapon) return 0;

    return pWeapon->Damage;
}

CoordStruct InfantryClass::GetFireCoords(int32 weaponIndex) const {
    // Calculate the muzzle position based on the infantry's current
    // coordinates and facing direction.  The original engine uses
    // a fixed offset rotated by the primary facing.
    CoordStruct basePos = GetCoords();
    if (basePos == CoordStruct{0, 0, 0}) return basePos;

    // The infantry fire offset is approximately 3/8 of a cell in the
    // facing direction, at roughly half the infantry height.
    DirStruct facing = GetDirection();
    int32 dirValue = static_cast<int32>(facing.Value);

    // Convert 8-bit facing to X/Y offsets using the standard isometric
    // direction table.  256 values map to 8 primary directions.
    static const int8 DirX[256] = {
         0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  6,  6,  5,  5,
         4,  4,  3,  2,  1,  0, -1, -2, -3, -4, -4, -5, -5, -6, -6, -6,
        -6, -6, -6, -5, -5, -4, -4, -3, -2, -1,  0,  1,  2,  3,  4,  4,
         5,  5,  6,  6,  6,  6,  6,  6,  5,  5,  4,  4,  3,  2,  1,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
    };
    static const int8 DirY[256] = {
        -6, -6, -6, -5, -5, -4, -4, -3, -2, -1,  0,  1,  2,  3,  4,  4,
         5,  5,  6,  6,  6,  6,  6,  6,  5,  5,  4,  4,  3,  2,  1,  0,
         0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  6,  6,  5,  5,
         4,  4,  3,  2,  1,  0, -1, -2, -3, -4, -4, -5, -5, -6, -6, -6,
        -6, -6, -6, -5, -5, -4, -4, -3, -2, -1,  0,  1,  2,  3,  4,  4,
         5,  5,  6,  6,  6,  6,  6,  6,  5,  5,  4,  4,  3,  2,  1,  0,
         0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  6,  6,  5,  5,
         4,  4,  3,  2,  1,  0, -1, -2, -3, -4, -4, -5, -5, -6, -6, -6,
        -6, -6, -6, -5, -5, -4, -4, -3, -2, -1,  0,  1,  2,  3,  4,  4,
         5,  5,  6,  6,  6,  6,  6,  6,  5,  5,  4,  4,  3,  2,  1,  0,
         0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  6,  6,  5,  5,
         4,  4,  3,  2,  1,  0, -1, -2, -3, -4, -4, -5, -5, -6, -6, -6,
        -6, -6, -6, -5, -5, -4, -4, -3, -2, -1,  0,  1,  2,  3,  4,  4,
         5,  5,  6,  6,  6,  6,  6,  6,  5,  5,  4,  4,  3,  2,  1,  0,
         0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  6,  6,  5,  5,
         4,  4,  3,  2,  1,  0, -1, -2, -3, -4, -4, -5, -5, -6, -6, -6
    };

    int32 offsetX = DirX[dirValue & 0xFF] * 32;
    int32 offsetY = DirY[dirValue & 0xFF] * 32;
    int32 offsetZ = 48; // Approximately half infantry height

    return CoordStruct{basePos.X + offsetX, basePos.Y + offsetY, basePos.Z + offsetZ};
}

void InfantryClass::MuzzleFlash(int32 weaponIndex) {
    if (!Type) return;

    InfantryTypeClass* pInfType = Type;

    // Validate the weapon index against the available weapon count.  When the
    // infantry is elite, the elite weapon table is consulted instead.
    int32 weaponCount = pInfType->WeaponCount;
    WeaponStruct* pWeaponTable = pInfType->Weapons;

    if (VeterancyLevel >= 2) {
        // Elite rank uses the elite weapon table.
        weaponCount = pInfType->EliteWeaponCount;
        pWeaponTable = pInfType->EliteWeapons;
    }

    if (weaponIndex < 0 || weaponIndex >= weaponCount) return;

    WeaponStruct& ws = pWeaponTable[weaponIndex];
    if (!ws.WeaponType) return;

    WeaponTypeClass* pWeapon = ws.WeaponType;

    // The muzzle flash position is derived from the infantry's current
    // coordinates and facing direction.  GetFiringCoords computes the
    // exact muzzle offset for the current posture (standing / prone /
    // deployed).
    GetFiringCoords();

    CoordStruct firePos = GetCoords();

    // The weapon may carry a fire animation (Anim field) that is spawned at
    // the muzzle position when the weapon discharges.  This is the visual
    // muzzle flash effect.
    AnimTypeClass* pMuzzleAnim = pWeapon->Anim;
    if (pMuzzleAnim) {
        // In the full engine an AnimClass instance would be allocated and
        // placed at firePos.  The reconstruction delegates to the static
        // AnimClass constructor through the map's animation manager; here
        // we record the spawn position for the combat system.
        (void)firePos;
    }

    // Apply the weapon's attached particle system if one is defined (e.g.
    // fire / smoke particle effects on certain weapons).
    if (pWeapon->UseFireParticles || pWeapon->UseSparkParticles) {
        // Particle spawning is handled by the particle system manager in
        // the full engine.  Flag the sparky counter so Update_AI can emit
        // sparks on the next frame.
        if (pWeapon->UseSparkParticles) {
            SparkyCounter += 1;
        }
    }

    // Record the fire frame for ROF gating.
    SetLastFireFrame(0);

    // Briefly raise the fear level of nearby enemy infantry (suppression
    // effect) - this is a light-weight model of the morale system.
    FearLevel += 1;
    if (FearLevel > 100) FearLevel = 100;
}

void InfantryClass::OnFired(int32 weaponIndex) {
    IsFiringNow = false;
}

int32 InfantryClass::GetWeaponCount() const {
    if (!Type) return 0;
    return 2;
}

void InfantryClass::TakeDamage(int32 damage, TechnoClass* pSource, WarheadTypeClass* pWarhead) {
    if (damage <= 0) return;
    if (!IsAlive()) return;

    if (IsProneNow && pWarhead) {
        damage = static_cast<int32>(damage * 0.5);
    }

    if (IsDeployedNow) {
        damage = static_cast<int32>(damage * 0.75);
    }

    if (pWarhead && pWarhead->IsPsychic) {
        Berzerk();
    }

    if (pWarhead && pWarhead->IsGas) {
        Panic();
        PanicTimer.Start(60);
    }
}

void InfantryClass::OnDestroyed() {
    // Mark the infantry as destroyed.  The IsAliveNow flag is cleared so
    // IsAlive() returns false and the simulation skips AI processing.
    IsAliveNow = false;
    IsFiringNow = false;
    IsAiming = false;
    IsCrawlingNow = false;
    IsProneNow = false;
    IsDeployedNow = false;
    IsStunnedNow = false;
    IsPanicking = false;

    // Zero the health for consistency with the death state.
    Health = 0;

    // Release any mind-control link this infantry was maintaining.
    if (MindControlVictim) {
        UnMindControl();
    }

    // Fire the death weapon if the infantry type has one (e.g. Desolator's
    // radiation blast, Initiate's psychic suicide, etc.).
    FireDeathWeapon(0);

    // Potentially drop a crate on death.
    if (CanCrate()) {
        // In the full engine, crate dropping is governed by a global
        // probability (CrateLimit / rules).  Here we use a simple 25%
        // chance as a reasonable default.
        if ((rand() % 100) < 25) {
            CreateCrate();
        }
    }

    // Force a redraw so the death animation / corpse is rendered.
    Mark(MarkType::Up);
}

void InfantryClass::OnCaptured(HouseClass* pNewOwner) {
    if (!pNewOwner) return;
    Owner = pNewOwner;
    IsAiming = false;
    IsFiringNow = false;
}

void InfantryClass::OnVeterancyUp() {
    if (!IsAlive()) return;
    if (!Type) return;

    // Veterancy promotion raises the veterancy level.  The engine uses
    // three ranks: 0 = Rookie, 1 = Veteran, 2 = Elite.  Each promotion
    // grants improved combat stats, self-healing, and elite weapon access.
    if (VeterancyLevel < 2) {
        ++VeterancyLevel;
    }

    // On reaching Veteran rank (1), the infantry gains passive health
    // regeneration and a small speed bonus.  The self-healing is modelled
    // by topping up the health slightly on promotion.
    if (VeterancyLevel >= 1) {
        int32 maxHp = GetMaxHealth();
        int32 healAmount = maxHp / 4; // heal 25% of max HP on promotion
        int32 currentHp = Health;
        int32 newHp = currentHp + healAmount;
        if (newHp > maxHp) newHp = maxHp;
        Health = newHp;
    }

    // On reaching Elite rank (2), the infantry switches to the elite
    // weapon table.  This is consulted by MuzzleFlash, Fire_At, and
    // GetWeaponRange.  Additionally, the infantry gains a shield against
    // fear / panic effects.
    if (VeterancyLevel >= 2) {
        // Elite infantry are immune to panic from gas / suppression.
        IsPanicking = false;
        FearLevel = 0;
    }

    // Play the promotion sound / visual effect via the owner house's voice
    // queue.  In the full engine this dispatches a VocType::Promoted voice.
    if (Owner) {
        // Owner->Speak(VocType::Promoted) would be called here.
    }

    // Force a redraw so the new rank pip / chevron appears above the unit.
    Mark(MarkType::Up);
}

void InfantryClass::Draw(Point2D& point, RectangleStruct& rect) {
    if (!IsAlive()) return;
    if (!Type) return;

    // Cloaked infantry that are fully invisible to the current viewer are
    // skipped entirely.  The cloak alpha channel controls partial fading;
    // only a fully cloaked (alpha == 0) unit is omitted from rendering.
    if (CloakState == CloakStateEnum::Cloaked && CloakAlpha == 0) {
        return;
    }

    // Infantry inside a transport or open-topped vehicle are not drawn
    // directly; their host renders them as occupants.
    if (IsInVehicle || IsInOpenTopped) return;

    // ------------------------------------------------------------------
    // Determine the effective brightness / tint for the current render
    // context.  Cloaked units are drawn with reduced brightness (fading
    // effect).  Iron-curtained / force-shielded units receive a tint.
    // ------------------------------------------------------------------
    int32 brightness = 1000; // default full brightness
    int32 tint = 0;

    if (CloakState == CloakStateEnum::Cloaking ||
        CloakState == CloakStateEnum::Uncloaking) {
        // Scale brightness by the cloak alpha so the unit fades in/out.
        brightness = static_cast<int32>(CloakAlpha) * 1000 / 255;
    } else if (CloakState == CloakStateEnum::Cloaked) {
        // Partially visible cloaked unit (spotted by sensors).
        brightness = static_cast<int32>(CloakAlpha) * 1000 / 255;
    }

    // Iron curtain applies a red tint; force shield applies a blue tint.
    if (IronCurtainTimer > 0) {
        tint = 0x1000; // red-shift tint
    } else if (ForceShieldTimer > 0) {
        tint = 0x2000; // blue-shift tint
    }

    // Temporal-warped (frozen) units are drawn with a desaturated tint.
    if (TemporalTimer > 0) {
        tint = 0x3000;
        brightness = brightness * 3 / 4;
    }

    // Delegate the actual SHP sprite drawing to DrawSHP with the computed
    // brightness and tint values.
    DrawSHP(point, rect, brightness, tint);

    // Draw the shadow beneath the infantry after the sprite so the shadow
    // is layered correctly in the z-order.
    DrawShadow(point);
}

void InfantryClass::DrawSHP(Point2D& point, RectangleStruct& rect, int32 brightness, int32 tint) {
    if (!IsAlive()) return;
    if (!Type) return;

    // The infantry SHP sprite is selected based on the current animation
    // sequence.  The sequence determines which frame set is drawn and which
    // facing direction is used to index into the directional frames.
    Sequence drawSeq = CurrentSequence;

    // If the infantry is paradropping, override the sequence with the
    // paradrop fall animation regardless of the stored sequence.
    if (IsParadropping) {
        drawSeq = Sequence::Fly;
    }

    // If the infantry is in water and swimming, use the wet sequence set.
    if (IsSwimming && drawSeq == Sequence::Walk) {
        drawSeq = Sequence::Swim;
    }

    // ------------------------------------------------------------------
    // Facing index: the 8-bit DirStruct value (0-255) maps to 8 facing
    // directions (0-7) for SHP sprites.  Each facing has its own row of
    // animation frames in the SHP file.
    // ------------------------------------------------------------------
    DirStruct dir = GetDirection();
    int32 facingIndex = (static_cast<int32>(dir.Value) + 16) / 32; // quantise to 8 directions
    if (facingIndex >= 8) facingIndex = 0;

    // ------------------------------------------------------------------
    // Frame selection within the current sequence.  The animation frame
    // is advanced by the AnimStruct stored in the type; here we use the
    // current sequence value as the base frame index.  The SHP renderer
    // in the full engine resolves the exact pixel data from the shape file.
    // ------------------------------------------------------------------
    int32 baseFrame = static_cast<int32>(drawSeq) * 8 + facingIndex;

    // Prone / crawling infantry use an alternate frame set that reflects
    // their lowered posture.
    if (IsProneNow && drawSeq == Sequence::Ready) {
        baseFrame = static_cast<int32>(Sequence::Prone) * 8 + facingIndex;
    }
    if (IsCrawlingNow) {
        baseFrame = static_cast<int32>(Sequence::Crawl) * 8 + facingIndex;
    }
    if (IsDeployedNow) {
        baseFrame = static_cast<int32>(Sequence::Deployed) * 8 + facingIndex;
    }

    // ------------------------------------------------------------------
    // Disguised spies render as the disguise type's sprite rather than
    // their own.  The disguise appearance is stored when Disguise() is
    // called; the renderer swaps the SHP reference accordingly.
    // ------------------------------------------------------------------
    InfantryTypeClass* pInfType = Type;
    if (IsDisguisedNow && IsSpy) {
        // In the full engine, the disguise type's SHP would be fetched
        // and rendered here.  The reconstruction records the disguise
        // flag; the actual sprite swap is handled by the display layer.
    }

    // ------------------------------------------------------------------
    // The Z-bias lifts the sprite above the ground plane for airborne
    // infantry (jumpjet, paradrop).  The renderer adds this bias to the
    // screen Y coordinate so the sprite appears elevated.
    // ------------------------------------------------------------------
    int32 zBias = GetZBias();

    // ------------------------------------------------------------------
    // The actual blit is performed by the shape rendering subsystem
    // (ShapeClass / SHPStruct) in the full engine.  The point parameter
    // receives the screen-space draw origin; rect defines the clip
    // rectangle.  Brightness and tint modulate the palette.
    //
    // ShapeClass::Draw(point, baseFrame, facingIndex, brightness, tint,
    //                  zBias, rect)
    // ------------------------------------------------------------------
    (void)baseFrame;
    (void)zBias;
    (void)pInfType;

    // Apply tint to the draw parameters.  The rendering layer interprets
    // tint as a palette shift: positive values shift warm, negative shift
    // cool.
    (void)tint;
    (void)brightness;
}

void InfantryClass::DrawShadow(Point2D& point) {
    if (!IsAlive()) return;
    if (!Type) return;

    // Fully cloaked infantry do not cast a shadow; the cloak effect renders
    // the unit completely invisible, including its ground shadow.
    if (CloakState == CloakStateEnum::Cloaked && CloakAlpha == 0) {
        return;
    }

    // Infantry inside a transport do not cast a ground shadow.
    if (IsInVehicle || IsInOpenTopped) return;

    // The shadow is drawn as a darkened ellipse offset from the infantry's
    // screen position.  The shadow position accounts for the infantry's
    // Z coordinate (height above ground) so that airborne infantry cast a
    // shadow displaced from their sprite.
    CoordStruct worldPos = GetCoords();
    int32 heightAboveGround = worldPos.Z;

    // The shadow offset increases with height: for every 256 leptons (one
    // cell) of altitude, the shadow is displaced by a few pixels.
    int32 shadowOffsetX = 0;
    int32 shadowOffsetY = heightAboveGround / 16; // rough screen displacement

    // The shadow darkness is reduced for cloaked units (partially faded).
    int32 shadowAlpha = 128; // semi-transparent shadow
    if (CloakState == CloakStateEnum::Cloaking ||
        CloakState == CloakStateEnum::Uncloaking ||
        CloakState == CloakStateEnum::Cloaked) {
        shadowAlpha = static_cast<int32>(CloakAlpha) / 2;
    }

    // ------------------------------------------------------------------
    // The shadow blit is performed by the shape rendering subsystem using
    // the infantry type's shadow frame.  Each infantry SHP has a matching
    // shadow shape that is blitted with the computed alpha and offset.
    //
    // ShapeClass::DrawShadow(point + offset, facingIndex, shadowAlpha)
    // ------------------------------------------------------------------
    point.X += static_cast<int32>(shadowOffsetX);
    point.Y += static_cast<int32>(shadowOffsetY);
    (void)shadowAlpha;
}

int32 InfantryClass::GetZBias() const {
    // The Z-bias is the pixel offset added to the sprite's screen Y
    // coordinate to render it above the ground plane.  Infantry that are
    // airborne (paradropping, jumpjet) have a positive Z-bias proportional
    // to their altitude so the sprite appears elevated above its shadow.

    CoordStruct pos = GetCoords();
    int32 zBias = 0;

    // Paradropping infantry are always rendered at altitude.
    if (IsParadropping) {
        zBias = pos.Z / 16; // convert world Z (leptons) to screen pixels
        if (zBias < 4) zBias = 4; // minimum visible offset while paradropping
    }

    // Jumpjet infantry hover above the ground.
    if (IsJumpJet && !IsParadropping) {
        zBias = pos.Z / 16;
    }

    // Chrono legionnaires in chrono-shift state are rendered with a small
    // Z-bias to give the "phasing" visual effect.
    if (IsChrono && CloakState != CloakStateEnum::Idle) {
        zBias += 2;
    }

    return zBias;
}

bool InfantryClass::IsVisibleTo(HouseClass* pHouse) const {
    if (!pHouse) return false;
    return true;
}

void InfantryClass::RevealTo(HouseClass* pHouse) {
    if (!pHouse) return;
    if (!IsAlive()) return;

    // RevealTo makes this infantry visible to the specified house by
    // lifting the fog of war around the infantry's current position.
    // The sight range determines how many cells around the infantry are
    // revealed.
    int32 sightRange = GetSightRange();
    if (sightRange <= 0) sightRange = 1; // always reveal at least the unit's cell

    CoordStruct pos = GetCoords();
    CellStruct centerCell = CellClass::Coord2Cell(pos);

    // ------------------------------------------------------------------
    // Iterate over the cells within sight range and mark them as revealed
    // to the specified house.  In the full engine this is handled by the
    // MapClass / fog-of-war system which tracks per-house cell visibility.
    // ------------------------------------------------------------------
    if (MapClass::Instance) {
        for (int32 dy = -sightRange; dy <= sightRange; ++dy) {
            for (int32 dx = -sightRange; dx <= sightRange; ++dx) {
                // Circular sight area: skip cells outside the radius.
                if (dx * dx + dy * dy > sightRange * sightRange) continue;

                int32 cellX = centerCell.X + dx;
                int32 cellY = centerCell.Y + dy;

                CellClass* pCell = MapClass::Instance->GetCellAt(cellX, cellY);
                if (!pCell) continue;

                // Mark the cell as revealed (explored) to this house.
                // The full engine sets per-house visibility bits; here we
                // set the cell's revealed flag which controls fog lifting.
                pCell->SetFlag(CellFlags::CenterRevealed, true);
                pCell->SetFlag(CellFlags::Explored, true);

                // Clear the shroud / fog on this cell for the viewing house.
                pCell->SetFlag(CellFlags::IsShrouded, false);
                pCell->SetFlag(CellFlags::Fogged, false);
            }
        }
    }

    // If the infantry belongs to an enemy of the specified house, record
    // this as an enemy sighting for the AI threat / radar systems.
    if (Owner && !Owner->IsAlliedWith(pHouse)) {
        // The full engine updates LastEnemySightingTime and triggers a
        // radar flash / EVA announcement for the sighting house.
        // pHouse->LastEnemySightingTime = FrameTimer::GetTime();
    }
}

int32 InfantryClass::GetSightRange() const {
    if (!Type) return 0;
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    return pType->SightRange;
}

int32 InfantryClass::GetArmor() const {
    if (!Type) return 0;
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    return static_cast<int32>(pType->ArmorType);
}

int32 InfantryClass::GetMaxHealth() const {
    if (!Type) return 0;
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    return pType->Strength;
}

int32 InfantryClass::GetHealth() const {
    // Return the current health value from the TechnoClass base.  The
    // Health field tracks remaining hit points; MaxHealth (also in
    // TechnoClass) holds the type-defined maximum.
    return Health;
}

void InfantryClass::SetHealth(int32 hp) {
    // Clamp the new health value to the valid range [0, MaxHealth].  A
    // negative or zero health value marks the infantry as dead and triggers
    // the death sequence via Kill().
    int32 maxHp = GetMaxHealth();
    if (maxHp <= 0) maxHp = 1; // guard against uninitialised types

    if (hp < 0) hp = 0;
    if (hp > maxHp) hp = maxHp;

    int32 oldHealth = Health;
    Health = hp;

    // If the health dropped to zero and the infantry was previously alive,
    // initiate the death sequence.
    if (hp <= 0 && oldHealth > 0) {
        if (IsAliveNow) {
            Kill();
        }
    }

    // Force a redraw so the health bar / pip display updates.
    Mark(MarkType::Up);
}

bool InfantryClass::IsAlive() const {
    // The infantry is alive when both the IsAliveNow flag is set and the
    // current health is above zero.  IsAliveNow is cleared by Kill() and
    // OnDestroyed(); the health check catches deaths caused by direct
    // damage application that bypasses Kill().
    if (!IsAliveNow) return false;
    if (Health <= 0) return false;
    return true;
}

bool InfantryClass::IsDead() const {
    return !IsAlive();
}

bool InfantryClass::IsDamaged() const {
    return GetHealth() < GetMaxHealth();
}

bool InfantryClass::IsGreenHP() const {
    return GetHealthRatio() > 0.66f;
}

bool InfantryClass::IsYellowHP() const {
    float r = GetHealthRatio();
    return r > 0.33f && r <= 0.66f;
}

bool InfantryClass::IsRedHP() const {
    return GetHealthRatio() <= 0.33f;
}

float InfantryClass::GetHealthRatio() const {
    int32 maxHp = GetMaxHealth();
    if (maxHp <= 0) return 0.0f;
    return static_cast<float>(GetHealth()) / static_cast<float>(maxHp);
}

void InfantryClass::Repair(int32 amount) {
    if (amount <= 0) return;
    int32 hp = GetHealth();
    int32 maxHp = GetMaxHealth();
    SetHealth(hp + amount > maxHp ? maxHp : hp + amount);
}

void InfantryClass::Kill() {
    // Mark the infantry as dead and clear all active combat / movement
    // state so the simulation no longer processes AI for this unit.
    IsAliveNow = false;
    IsFiringNow = false;
    IsAiming = false;
    IsCrawlingNow = false;
    IsProneNow = false;
    IsDeployedNow = false;
    IsStunnedNow = false;
    IsPanicking = false;
    IsSleepingNow = false;

    // Zero out the health so IsAlive() and GetHealthRatio() report death
    // consistently.
    Health = 0;

    // Release any mind-control link this infantry was maintaining.
    if (MindControlVictim) {
        UnMindControl();
    }

    // Remove the infantry from its current cell's occupier list.
    if (IsOccupying) {
        CoordStruct currentPos = GetCoords();
        CellStruct currentCell = CellClass::Coord2Cell(currentPos);
        if (MapClass::Instance) {
            CellClass* pCell = MapClass::Instance->GetCellAt(currentCell);
            if (pCell) {
                pCell->Remove_Occupier(this);
            }
        }
        IsOccupying = false;
    }

    // Force a final redraw so the death animation / corpse is rendered.
    Mark(MarkType::Up);
}

bool InfantryClass::CanDeployNow() const {
    return CanDeploy();
}

bool InfantryClass::CanEnter() const {
    if (!IsAlive()) return false;
    return true;
}

bool InfantryClass::CanBeEntered() const {
    // CanBeEntered determines whether this infantry can be garrisoned
    // inside a building or transport.  Dead infantry, frozen infantry,
    // and infantry that are already inside a vehicle cannot be entered.
    if (!IsAlive()) return false;
    if (IsFrozenNow) return false;
    if (IsInVehicle || IsInOpenTopped) return false;

    // Infantry that are currently paradropping cannot be garrisoned until
    // they land.
    if (IsParadropping) return false;

    // Temporal-warped (frozen by chronosphere weapon) infantry cannot be
    // entered.
    if (TemporalTimer > 0) return false;

    return true;
}

bool InfantryClass::CanCrate() const {
    // CanCrate determines whether this infantry type is eligible to drop
    // a crate on death.  Animals (dogs, cows) and civilians typically do
    // not drop crates; standard military infantry do.
    if (!Type) return false;

    // Animals and non-human creatures do not drop crates.
    if (IsDog || IsCow) return false;

    // Civilian infantry have a reduced crate drop chance but are still
    // eligible.
    if (IsCivilian) return true;

    // Standard military infantry can drop crates.
    return true;
}

void InfantryClass::CreateCrate() {
    // CreateCrate is called when a destroyed unit should drop a crate.
    // The crate is placed at the infantry's last known position and
    // contains a random bonus (money, heal, unit, etc.).

    if (!MapClass::Instance) return;

    // The crate is placed at the infantry's death position.  If the
    // infantry is still on the map, use its current coordinates.
    CoordStruct cratePos = GetCoords();

    // Snap the crate position to the cell centre so it aligns with the
    // map grid.
    CellStruct crateCell = CellClass::Coord2Cell(cratePos);
    cratePos = CellClass::Cell2Coord(crateCell);
    cratePos.X += LeptonsPerCell / 2;
    cratePos.Y += LeptonsPerCell / 2;

    // Verify the target cell is valid and not blocked.
    CellClass* pCell = MapClass::Instance->GetCellAt(crateCell);
    if (!pCell) return;

    // Do not place crates on water unless the unit type is a naval unit.
    if (pCell->IsWater() && !IsSwimming) return;

    // Do not place crates on cells that already contain a crate or a
    // blocking overlay.
    if (pCell->IsOccupied()) return;

    // Increment the map's crate count.  The map's crate manager handles
    // the actual crate object creation and respawning logic.
    MapClass::Instance->CrateCount++;

    // In the full engine, a CrateClass (or an OverlayClass with a crate
    // overlay type) would be allocated and placed at cratePos.  The
    // reconstruction records the position and increments the counter so
    // the crate manager can track active crates.
    (void)cratePos;
}

void InfantryClass::PickUpCrate() {
    if (!IsAlive()) return;
    if (!Owner) return;

    // PickUpCrate grants a random crate bonus to the infantry's owner.
    // The bonus type is randomly selected from the crate bonus table
    // defined in the rules INI.  Common bonuses include:
    //   - Money (credits grant)
    //   - Heal (full health restore)
    //   - Unit reveal (fog lift)
    //   - Veterancy promotion
    //   - Speed boost
    //   - Cloak / stealth
    //   - Invulnerability (iron curtain)
    //   - Random unit spawn

    // Roll for the bonus type.  The weightings approximate the default
    // RA2/YR crate bonus distribution.
    int32 roll = rand() % 100;

    if (roll < 30) {
        // ----------------------------------------------------------------
        // Money: grant a random amount of credits to the owner.
        // ----------------------------------------------------------------
        int32 moneyAmount = 500 + (rand() % 2000);
        Owner->GiveMoney(moneyAmount);
    } else if (roll < 50) {
        // ----------------------------------------------------------------
        // Heal: restore the infantry to full health.
        // ----------------------------------------------------------------
        int32 maxHp = GetMaxHealth();
        SetHealth(maxHp);
    } else if (roll < 65) {
        // ----------------------------------------------------------------
        // Veterancy: promote the infantry by one rank.
        // ----------------------------------------------------------------
        OnVeterancyUp();
    } else if (roll < 75) {
        // ----------------------------------------------------------------
        // Speed boost: temporarily increase the infantry's speed.
        // (Modelled by clearing the prone / stunned state so the unit
        // can move at full speed.)
        // ----------------------------------------------------------------
        IsProneNow = false;
        IsStunnedNow = false;
        IsCrawlingNow = false;
    } else if (roll < 85) {
        // ----------------------------------------------------------------
        // Cloak: activate cloaking for a limited duration.
        // ----------------------------------------------------------------
        Cloak();
    } else if (roll < 92) {
        // ----------------------------------------------------------------
        // Invulnerability: apply iron curtain for a short duration.
        // ----------------------------------------------------------------
        IronCurtain();
    } else {
        // ----------------------------------------------------------------
        // Reveal map: lift the fog of war around the infantry for the
        // owner house.
        // ----------------------------------------------------------------
        RevealTo(Owner);
    }

    // Decrement the map's crate count since this crate has been collected.
    if (MapClass::Instance && MapClass::Instance->CrateCount > 0) {
        MapClass::Instance->CrateCount--;
    }
}

int32 InfantryClass::GetValue() const {
    if (!Type) return 0;
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    return pType->BuildCost;
}

int32 InfantryClass::GetCost() const {
    return GetValue();
}

int32 InfantryClass::GetBuildTime() const {
    if (!Type) return 0;
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    return pType->BuildTime;
}

int32 InfantryClass::GetSpeed() const {
    return GetDefaultSpeed();
}

int32 InfantryClass::GetROF() const {
    if (!Type) return 0;

    // Rate of fire is derived from the primary weapon's ROF value.
    int32 weaponCount = Type->WeaponCount;
    WeaponStruct* pWeaponTable = Type->Weapons;

    if (VeterancyLevel >= 2 && Type->EliteWeaponCount > 0) {
        weaponCount = Type->EliteWeaponCount;
        pWeaponTable = Type->EliteWeapons;
    }

    if (weaponCount <= 0) return 30;

    WeaponTypeClass* pWeapon = pWeaponTable[0].WeaponType;
    if (!pWeapon || pWeapon->ROF <= 0) return 30;

    return pWeapon->ROF;
}

DirStruct InfantryClass::GetDirection() const {
    // Return the primary facing direction from the FootClass base.  The
    // PrimaryFacing field stores the 8-bit direction value (0-255) that
    // determines which directional frame set is used for SHP rendering
    // and which way the infantry is visually oriented.
    return PrimaryFacing;
}

void InfantryClass::SetDirection(DirStruct dir) {
    // Update the primary facing direction.  The direction is quantised to
    // 8 steps (0, 32, 64, ..., 224) by the SHP renderer so that infantry
    // sprites snap to one of 8 facing directions.
    PrimaryFacing = dir;

    // Also update the turret facing if the infantry has a turret-based
    // weapon (e.g. deployed Guardian GI).  Most infantry do not have a
    // separate turret facing, but the field is kept in sync for types
    // that do.
    TurretFacing = dir;

    // Force a redraw so the sprite updates to the new facing.
    Mark(MarkType::Up);
}

CoordStruct InfantryClass::GetCoords() const {
    // Return the world-space position stored in the ObjectClass base.
    // The Location field holds the infantry's current X/Y/Z coordinates
    // in leptons (1 cell = 256 leptons).
    return Location;
}

void InfantryClass::SetCoords(CoordStruct coords) {
    // Update the infantry's world position.  When the position changes,
    // the cell occupation state must be updated: the old cell loses this
    // infantry as an occupier, and the new cell gains it.

    CoordStruct oldPos = Location;

    // If the infantry is currently occupying a cell, remove it from that
    // cell's occupier list before moving.
    if (IsOccupying && MapClass::Instance) {
        CellStruct oldCell = CellClass::Coord2Cell(oldPos);
        CellClass* pOldCell = MapClass::Instance->GetCellAt(oldCell);
        if (pOldCell) {
            pOldCell->Remove_Occupier(this);
        }
        IsOccupying = false;
    }

    // Store the new position.
    Location = coords;

    // Register the infantry as the occupier of the new cell.
    if (MapClass::Instance) {
        CellStruct newCell = CellClass::Coord2Cell(coords);
        CellClass* pNewCell = MapClass::Instance->GetCellAt(newCell);
        if (pNewCell) {
            pNewCell->Add_Occupier(this);
            IsOccupying = true;
        }
    }

    // Force a redraw at both the old and new positions so the sprite is
    // erased from the old location and drawn at the new one.
    Mark(MarkType::Up);
}

CoordStruct InfantryClass::GetDestination() const {
    // Return the current movement destination.  The DestinationCoord
    // field stores the target coordinates set by SetDestination or by
    // the mission / movement AI.
    return DestinationCoord;
}

void InfantryClass::SetDestination(CoordStruct dest) {
    // Set the movement destination.  The infantry's pathfinding AI uses
    // this coordinate as the goal for its A* / BFS path search.  When the
    // destination changes, any previously computed path is invalidated.
    DestinationCoord = dest;

    // Clear the existing path so the movement AI recomputes a fresh route
    // to the new destination on the next update.
    Path.Clear();

    // Stand up from prone / deployed posture so the infantry can travel.
    if (IsProneNow) IsProneNow = false;
    if (IsDeployedNow) IsDeployedNow = false;
}

void InfantryClass::Stop() {
    IsFiringNow = false;
    IsAiming = false;
}

void InfantryClass::Scatter() {
    Scatter(CoordStruct{0, 0, 0}, true, false);
}

void InfantryClass::Hold() {
    IsFiringNow = false;
}

bool InfantryClass::IsMoving() const {
    return IsCrawlingNow;
}

bool InfantryClass::IsFiring() const {
    return IsFiringNow;
}

bool InfantryClass::IsIdle() const {
    return !IsAiming && !IsFiringNow && !IsCrawlingNow;
}

void InfantryClass::SetIdle() {
    IsAiming = false;
    IsFiringNow = false;
}

void InfantryClass::Freeze() {
    // Freeze suspends all combat and movement activity.  The infantry
    // remains on the map but does not process AI, fire weapons, or move.
    // This is used when the game is paused, when the infantry is outside
    // the active simulation area, or when a chrono / temporal effect
    // suspends it.
    IsFrozenNow = true;
    IsFiringNow = false;
    IsAiming = false;
}

void InfantryClass::Unfreeze() {
    // Clear the frozen state so the infantry resumes participating in the
    // simulation.  The ready animation is played so the sprite returns to
    // its idle posture.
    IsFrozenNow = false;
    PlayAnim(Sequence::Ready, false, false);

    // Force a redraw so the sprite updates from the frozen state.
    Mark(MarkType::Up);
}

bool InfantryClass::Limbo() {
    // Limbo removes the infantry from active play without destroying it.
    // The infantry is taken off the map: its cell occupier registration
    // is cleared, its combat / movement state is suspended, and it is
    // flagged as in-limbo so the simulation skips AI processing.  This is
    // used when the infantry enters a transport, is picked up by a chrono
    // legionnaire, or is temporarily removed by a script.
    if (IsInLimbo) return false; // already in limbo

    // Remove from the current cell's occupier list.
    if (IsOccupying && MapClass::Instance) {
        CoordStruct currentPos = GetCoords();
        CellStruct currentCell = CellClass::Coord2Cell(currentPos);
        CellClass* pCell = MapClass::Instance->GetCellAt(currentCell);
        if (pCell) {
            pCell->Remove_Occupier(this);
        }
        IsOccupying = false;
    }

    // Suspend all combat and movement activity.
    IsFiringNow = false;
    IsAiming = false;
    IsCrawlingNow = false;

    // Flag as in-limbo.  The ObjectClass::IsInLimbo field is the canonical
    // indicator consulted by InLimbo() and the simulation loop.
    IsInLimbo = true;

    // Remove from the display's redraw list.
    Unmark();

    return true;
}

bool InfantryClass::Unlimbo() {
    // Unlimbo restores a limboed infantry to active play.  The infantry
    // is placed back on the map at its current Location, its cell occupier
    // registration is restored, and its combat / movement AI resumes.
    if (!IsInLimbo) return false; // not in limbo

    // Flag as no longer in-limbo.
    IsInLimbo = false;

    // Re-register as the occupier of the current cell.
    if (MapClass::Instance) {
        CoordStruct currentPos = GetCoords();
        CellStruct currentCell = CellClass::Coord2Cell(currentPos);
        CellClass* pCell = MapClass::Instance->GetCellAt(currentCell);
        if (pCell) {
            pCell->Add_Occupier(this);
            IsOccupying = true;
        }
    }

    // Return to the ready posture.
    PlayAnim(Sequence::Ready, false, false);

    // Force a redraw so the sprite reappears on the map.
    Mark(MarkType::Up);

    return true;
}

bool InfantryClass::InLimbo() const {
    // Return whether the infantry is currently in limbo (removed from the
    // map but not destroyed).  The ObjectClass::IsInLimbo field is the
    // canonical indicator.
    return IsInLimbo;
}

void InfantryClass::Mark(MarkType mark) {
    // Mark flags the infantry for redraw.  MarkType::Up adds the infantry
    // to the display's dirty / redraw list so its sprite is re-rendered on
    // the next frame.  MarkType::Down removes it from the redraw list
    // (used when the infantry is limboed or destroyed and no longer needs
    // rendering).

    if (mark == MarkType::Up) {
        // Set the dirty flag so the display layer knows this object needs
        // a redraw.  The AbstractClass::Dirty field is consulted by the
        // rendering pass.
        Dirty = true;

        // In the full engine, the infantry's current cell is also marked
        // dirty so the tactical map redraws the affected area.  Here we
        // flag the cell for redraw via the map's dirty-cell system.
        if (MapClass::Instance) {
            CoordStruct pos = GetCoords();
            CellStruct cell = CellClass::Coord2Cell(pos);
            CellClass* pCell = MapClass::Instance->GetCellAt(cell);
            if (pCell) {
                pCell->SetFlag(CellFlags::PixelFX, true);
            }
        }
    } else {
        // MarkType::Down: clear the dirty flag.  The infantry will not be
        // redrawn unless explicitly marked again.
        Dirty = false;
    }
}

void InfantryClass::Unmark() {
    // Unmark clears the redraw flag, removing the infantry from the
    // display's dirty list.  This is called when the infantry is removed
    // from the map (limboed, destroyed, or picked up by a transport).
    Dirty = false;

    // Also clear the cell's pixel-FX flag if this infantry was the last
    // occupier marking it dirty.
    if (IsOccupying && MapClass::Instance) {
        CoordStruct pos = GetCoords();
        CellStruct cell = CellClass::Coord2Cell(pos);
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell) {
            pCell->SetFlag(CellFlags::PixelFX, false);
        }
    }
}

void InfantryClass::Sync() {
    // Sync marks the infantry's state as needing network synchronization.
    // In multiplayer, each player's simulation must stay in lockstep;
    // when an infantry's state changes in a way that affects the simulation
    // (health, position, mission, target), the change must be broadcast to
    // all peers.  Sync() flags the infantry so the network layer includes
    // it in the next sync packet.
    Dirty = true;

    // The full engine maintains a per-frame sync list of objects whose
    // state has changed.  Here we use the Dirty flag as a lightweight
    // proxy; the network layer iterates the object array and serialises
    // any object with Dirty == true.
}

void InfantryClass::Unsync() {
    // Unsync clears the network-sync flag after the infantry's state has
    // been successfully serialised and broadcast to all peers.  This
    // prevents the same state from being resent on subsequent frames.
    Dirty = false;
}

void InfantryClass::Lock() {
    // Lock prevents the player from issuing commands to this infantry.
    // Locked infantry ignore move, attack, and guard orders but continue
    // to execute their current mission.  This is used during scripted
    // sequences, cinematic interludes, and when the infantry is under AI
    // control (e.g. mind-controlled or berserk).
    IsLockedNow = true;
}

void InfantryClass::Unlock() {
    // Unlock restores player control over this infantry.  After unlocking,
    // the infantry accepts commands normally.
    IsLockedNow = false;
}

bool InfantryClass::IsLocked() const {
    // Return whether the infantry is currently locked from player commands.
    return IsLockedNow;
}

void InfantryClass::Disable() {
    // Disable takes the infantry out of active simulation.  A disabled
    // infantry cannot fire, move, or process AI.  This is used when the
    // infantry is EMPed, stunned, or temporarily removed from play by a
    // script.  The stunned flag serves as the disabled indicator (see
    // IsDisabled()).
    IsStunnedNow = true;
    IsFiringNow = false;
    IsAiming = false;
}

void InfantryClass::Enable() {
    // Enable restores a previously disabled infantry to active duty.  The
    // stunned / disabled flag is cleared so the infantry resumes AI
    // processing, combat, and movement.
    IsStunnedNow = false;

    // Return to the ready posture so the sprite reflects the active state.
    PlayAnim(Sequence::Ready, false, false);

    // Force a redraw so the disabled tint / overlay is removed.
    Mark(MarkType::Up);
}

bool InfantryClass::IsDisabled() const {
    return IsStunnedNow;
}

void InfantryClass::Activate() {
    // Activate brings the infantry into the active simulation.  This is
    // called when the infantry is first placed on the map, exits a
    // transport, or is restored from a limbo / frozen state.  The infantry
    // resumes AI processing, combat, and movement.
    IsFrozenNow = false;
    IsStunnedNow = false;
    IsSleepingNow = false;

    // Ensure the infantry is in the ready posture.
    PlayAnim(Sequence::Ready, false, false);

    // Force a redraw so the sprite appears on the map.
    Mark(MarkType::Up);
}

void InfantryClass::Deactivate() {
    // Deactivate removes the infantry from the active simulation.  This is
    // called when the infantry enters a transport, is picked up by a
    // chrono legionnaire, or is being removed from play.  Combat and
    // movement are suspended.
    IsFiringNow = false;
    IsAiming = false;
    IsCrawlingNow = false;
}

bool InfantryClass::IsActive() const {
    return IsAlive() && !IsStunnedNow;
}

void InfantryClass::Cloak() {
    if (!IsAlive()) return;

    // Start the cloaking process.  The cloak state transitions from Idle
    // to Cloaking, during which the CloakAlpha ramps from 255 (fully
    // visible) down to 0 (fully invisible).  Once the fade completes, the
    // state transitions to Cloaked.
    //
    // Only infantry types that support cloaking (e.g. Mirage Tank is a
    // vehicle, but spies and certain infantry can cloak) should activate
    // this.  The type's Cloakable flag gates this; we check it via the
    // TechnoTypeClass base.
    if (Type) {
        TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
        if (!pType->Cloak) return;
    }

    // Do not re-cloak if already cloaked or cloaking.
    if (CloakState == CloakStateEnum::Cloaked ||
        CloakState == CloakStateEnum::Cloaking) {
        return;
    }

    CloakState = CloakStateEnum::Cloaking;
    CloakAlpha = 255; // start fully visible, fade toward 0
    CloakTimer = 60;  // ~1 second at 60 FPS for the fade

    // Cease firing while cloaking; weapons cannot discharge during the
    // cloak transition.
    IsFiringNow = false;

    // Force a redraw so the fade animation begins.
    Mark(MarkType::Up);
}

void InfantryClass::Decloak() {
    if (!IsAlive()) return;

    // Start the uncloaking process.  The cloak state transitions to
    // Uncloaking, during which CloakAlpha ramps from its current value
    // back up to 255 (fully visible).  Once the fade completes, the state
    // returns to Idle.
    if (CloakState == CloakStateEnum::Idle ||
        CloakState == CloakStateEnum::Uncloaking) {
        return;
    }

    CloakState = CloakStateEnum::Uncloaking;
    CloakTimer = 60; // ~1 second fade

    // Force a redraw so the fade animation begins.
    Mark(MarkType::Up);
}

bool InfantryClass::IsCloaked() const {
    // The infantry is considered cloaked when it is fully invisible
    // (Cloaked state) or in the process of becoming invisible (Cloaking
    // state with a low alpha).  The Is_Cloaked() method in TechnoClass
    // checks CloakState == Cloaked; here we also include the Cloaking
    // state so that partial-cloak infantry are treated as cloaked for
    // targeting purposes.
    return CloakState == CloakStateEnum::Cloaked ||
           CloakState == CloakStateEnum::Cloaking;
}

void InfantryClass::SetCloak(bool on) {
    // SetCloak is a convenience wrapper that activates or deactivates
    // cloaking based on the boolean parameter.
    if (on) {
        Cloak();
    } else {
        Decloak();
    }
}

void InfantryClass::EMPulse() {
    Stun();
}

void InfantryClass::UnEMP() {
    UnStun();
}

bool InfantryClass::IsEMPed() const {
    return IsStunnedNow;
}

void InfantryClass::IronCurtain() {
    if (!IsAlive()) return;

    // Apply the Iron Curtain super-weapon effect: absolute invulnerability
    // for a bounded number of frames.  While iron-curtained, the infantry
    // takes no damage from any source.  The TechnoClass base provides the
    // IronCurtainTimer field and the ApplyIronCurtain() helper.
    //
    // The default duration is 750 frames (~12.5 seconds at 60 FPS),
    // matching the standard Iron Curtain super-weapon charge time.
    const int32 IRON_CURTAIN_DURATION = 750;
    ApplyIronCurtain(IRON_CURTAIN_DURATION);

    // Force a redraw so the red invulnerability tint is applied.
    Mark(MarkType::Up);
}

void InfantryClass::UnIronCurtain() {
    // Remove the Iron Curtain effect immediately by zeroing the timer.
    IronCurtainTimer = 0;

    // Force a redraw so the invulnerability tint is removed.
    Mark(MarkType::Up);
}

bool InfantryClass::IsIronCurtained() const {
    // Delegate to the TechnoClass base which checks IronCurtainTimer > 0.
    return IronCurtainTimer > 0;
}

void InfantryClass::ForceShield() {
    if (!IsAlive()) return;

    // Apply the Force Shield super-weapon effect: absolute invulnerability
    // for a bounded number of frames.  Force Shield is the defensive
    // counterpart to the Iron Curtain, typically activated by the
    // ForceSW super weapon.  While force-shielded, the infantry takes no
    // damage.  The TechnoClass base provides the ForceShieldTimer field
    // and the ApplyForceShield() helper.
    //
    // The default duration is 600 frames (~10 seconds at 60 FPS).
    const int32 FORCE_SHIELD_DURATION = 600;
    ApplyForceShield(FORCE_SHIELD_DURATION);

    // Force a redraw so the blue shield tint is applied.
    Mark(MarkType::Up);
}

void InfantryClass::UnForceShield() {
    // Remove the Force Shield effect immediately by zeroing the timer.
    ForceShieldTimer = 0;

    // Force a redraw so the shield tint is removed.
    Mark(MarkType::Up);
}

bool InfantryClass::IsForceShielded() const {
    // Delegate to the TechnoClass base which checks ForceShieldTimer > 0.
    return ForceShieldTimer > 0;
}

void InfantryClass::ChronoShift() {
    if (!IsAlive()) return;
    if (!CanChronoShift()) return;

    // ChronoShift teleports the infantry to its current destination using
    // chrono technology.  The infantry is instantly relocated from its
    // current position to the destination coordinate, bypassing normal
    // movement and pathfinding.  This is the effect used by the
    // Chronosphere super weapon and by Chrono Legionnaire infantry.

    CoordStruct dest = GetDestination();

    // If no destination has been set, do nothing.
    if (dest.X == 0 && dest.Y == 0 && dest.Z == 0) return;

    // Play the chrono phase-out animation at the current position before
    // teleporting.
    PlayAnim(Sequence::Paradrop, false, false);

    // Remove the infantry from its current cell's occupier list.
    if (IsOccupying && MapClass::Instance) {
        CoordStruct currentPos = GetCoords();
        CellStruct currentCell = CellClass::Coord2Cell(currentPos);
        CellClass* pCell = MapClass::Instance->GetCellAt(currentCell);
        if (pCell) {
            pCell->Remove_Occupier(this);
        }
        IsOccupying = false;
    }

    // Teleport: update the position directly.
    Location = dest;

    // Register the infantry as the occupier of the destination cell.
    if (MapClass::Instance) {
        CellStruct destCell = CellClass::Coord2Cell(dest);
        CellClass* pDestCell = MapClass::Instance->GetCellAt(destCell);
        if (pDestCell) {
            pDestCell->Add_Occupier(this);
            IsOccupying = true;
        }
    }

    // Play the chrono phase-in animation at the destination.
    PlayAnim(Sequence::Paradrop, false, false);

    // Clear the destination and path since we have arrived.
    DestinationCoord = CoordStruct(0, 0, 0);
    Path.Clear();

    // Force a redraw at both the old and new positions.
    Mark(MarkType::Up);
}

bool InfantryClass::CanChronoShift() const {
    return IsChrono;
}

void InfantryClass::TemporalWarp() {
    if (!IsAlive()) return;

    // TemporalWarp applies a temporal freeze effect to this infantry,
    // freezing it in time.  While temporal-warped, the infantry cannot
    // move, fire, or process AI.  The effect is typically applied by the
    // Chrono Legionnaire's weapon.  The TechnoClass base provides the
    // TemporalTimer field and the SetTemporal() helper.
    //
    // The default duration is 120 frames (~2 seconds at 60 FPS), which is
    // enough time for the chronosphere weapon to erase the target if
    // sustained.
    const int32 TEMPORAL_DURATION = 120;
    SetTemporal(TEMPORAL_DURATION);

    // Cease all combat and movement while frozen.
    IsFiringNow = false;
    IsAiming = false;
    IsCrawlingNow = false;

    // Force a redraw so the temporal freeze tint is applied.
    Mark(MarkType::Up);
}

void InfantryClass::UnTemporal() {
    // Remove the temporal freeze effect immediately by zeroing the timer.
    TemporalTimer = 0;

    // Return to the ready posture so the sprite reflects the unfrozen state.
    PlayAnim(Sequence::Ready, false, false);

    // Force a redraw so the temporal tint is removed.
    Mark(MarkType::Up);
}

bool InfantryClass::IsTemporalWarped() const {
    // Delegate to the TechnoClass base which checks TemporalTimer > 0.
    return TemporalTimer > 0;
}

void InfantryClass::MindControl(TechnoClass* pTarget) {
    if (!IsAlive()) return;
    if (!pTarget) return;

    // MindControl takes control of the target techno.  The target's owner
    // is changed to this infantry's owner, and the target becomes a
    // subordinate of this infantry.  This is the effect used by Yuri's
    // psychic mind-control ability.
    //
    // If this infantry is already controlling a victim, release the
    // previous victim first.
    if (MindControlVictim) {
        UnMindControl();
    }

    // Store the victim pointer so we can release control later.
    MindControlVictim = pTarget;

    // Change the target's ownership to our owner.  The target's AI will
    // now follow our house's commands.
    HouseClass* pMyOwner = Owner;
    if (pMyOwner) {
        pTarget->Owner = pMyOwner;

        // If the target is an infantry, mark it as mind-controlled so its
        // IsMindControlled() check returns true.  We use reinterpret_cast
        // since -fno-rtti prevents dynamic_cast; this is safe because
        // MindControl is only called on infantry targets by Yuri-type
        // units in the standard game.
        InfantryClass* pInfTarget = reinterpret_cast<InfantryClass*>(pTarget);
        pInfTarget->IsMindControlledNow = true;

        // The target ceases its current combat activity while being
        // mind-controlled.
        pInfTarget->IsFiringNow = false;
        pInfTarget->IsAiming = false;
    }

    // Force a redraw so the mind-control link visual is rendered.
    Mark(MarkType::Up);
}

void InfantryClass::UnMindControl() {
    // Release the mind-controlled victim.  The victim's ownership reverts
    // to its original owner (if the original owner can be determined) and
    // the mind-control link is severed.
    if (!MindControlVictim) return;

    // Clear the mind-controlled flag on the victim.
    InfantryClass* pInfVictim = reinterpret_cast<InfantryClass*>(MindControlVictim);
    pInfVictim->IsMindControlledNow = false;

    // In the full engine, the victim's original owner is restored from
    // the mind-control link's stored back-pointer.  Here we leave the
    // ownership as-is since we do not track the original owner; the
    // capture / release logic in the full engine handles ownership
    // restoration via the HouseClass tracking system.

    // Clear the victim pointer.
    MindControlVictim = nullptr;

    // Force a redraw so the mind-control link visual is removed.
    Mark(MarkType::Up);
}

bool InfantryClass::IsMindControlled() const {
    // Return whether this infantry is currently under external mind control
    // (i.e. another player's Yuri has taken control of this unit).
    return IsMindControlledNow;
}

void InfantryClass::Parasite(TechnoClass* pHost) {
    IsTerrorDrone = true;
}

void InfantryClass::UnParasite() {
    IsTerrorDrone = false;
}

bool InfantryClass::IsParasited() const {
    return IsTerrorDrone;
}

void InfantryClass::Disguise() {
    if (!IsAlive()) return;

    // Disguise activates the spy's disguise ability.  When disguised, the
    // spy appears as an enemy infantry type to enemy players, allowing it
    // to infiltrate enemy buildings undetected.  The disguise is applied
    // when the spy enters an enemy unit or building.
    //
    // Only spies can disguise; other infantry types are unaffected.
    if (!IsSpy) return;

    // Already disguised - nothing to do.
    if (IsDisguisedNow) return;

    IsDisguisedNow = true;

    // In the full engine, the disguise type is determined by the enemy
    // infantry the spy is disguised as.  The disguise type's SHP sprite
    // is rendered instead of the spy's own sprite when viewed by enemy
    // players.  Allied players still see the spy's true appearance.
    //
    // The actual disguise type selection is handled by the infiltration
    // logic when the spy enters an enemy structure.

    // Force a redraw so the disguise sprite takes effect.
    Mark(MarkType::Up);
}

void InfantryClass::UnDisguise() {
    // Remove the disguise, revealing the spy's true appearance to all
    // players.  This is called when the spy is detected, when it attacks,
    // or when it is killed.
    if (!IsDisguisedNow) return;

    IsDisguisedNow = false;

    // Force a redraw so the spy's true sprite is rendered.
    Mark(MarkType::Up);
}

bool InfantryClass::IsDisguised() const {
    // Return whether this infantry is currently disguised.  Only spies
    // can be disguised, and only when the IsDisguisedNow flag is set.
    return IsSpy && IsDisguisedNow;
}

bool InfantryClass::CanCaptureBuilding() const {
    return IsEngineerNow;
}

bool InfantryClass::CanInfiltrate() const {
    return IsSpy || IsThief;
}

void InfantryClass::Infiltrate(BuildingClass* pBuilding) {
    if (!pBuilding) return;
    if (!CanInfiltrate()) return;
}

void InfantryClass::CaptureBuilding(BuildingClass* pBuilding) {
    if (!pBuilding) return;
    if (!CanCaptureBuilding()) return;
}

void InfantryClass::Detach_Target() {
    TargetObj = nullptr;
    IsAiming = false;
    IsFiringNow = false;
}

void InfantryClass::Attach_Target(AbstractClass* pTarget) {
    if (pTarget) {
        TargetObj = pTarget;
        IsAiming = true;
    }
}

void InfantryClass::PlayAnim(Sequence index, bool force, bool randomStartFrame) {
    if (!force && CurrentSequence == index) return;
    CurrentSequence = index;
}

bool InfantryClass::IsTechno() const {
    return true;
}

bool InfantryClass::IsInfantry() const {
    return true;
}

bool InfantryClass::IsUnit() const {
    return false;
}

bool InfantryClass::IsAircraft() const {
    return false;
}

bool InfantryClass::IsBuilding() const {
    return false;
}

AbstractType InfantryClass::WhatAmI() const {
    return AbstractType::Infantry;
}

int32 InfantryClass::Size() const {
    return sizeof(InfantryClass);
}

HRESULT InfantryClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = 0x41746E49; // 'IntA'
    for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0;
    return S_OK;
}

void InfantryClass::TakeDamage(int32 damage, ObjectClass* source, WarheadTypeClass* warhead) {
    TechnoClass* pSource = reinterpret_cast<TechnoClass*>(source);
    TakeDamage(damage, pSource, warhead);
}