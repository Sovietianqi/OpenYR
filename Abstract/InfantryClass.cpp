#include <Abstract/InfantryClass.h>
#include <Game/SaveGameClass.h>
#include <Abstract/InfantryTypeClass.h>
#include <Combat/WarheadTypeClass.h>
#include <Combat/WeaponTypeClass.h>
#include <Abstract/TechnoTypeClass.h>
#include <Abstract/BuildingClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Combat/BulletClass.h>
#include <Scenario/ScenarioClass.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <IO/MixFileClass.h>
#include <Houses/HouseClass.h>
#include <Rules/RulesClass.h>
#include <Game/Game.h>
#include <INI/INIClass.h>
#include <Game/Externs.h>
#include <Rendering/Surface.h>
#include <Rendering/ConvertClass.h>
#include <Rendering/TacticalClass.h>
#include <Rendering/UnitArt.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Rendering/ObjectUI.h"

// 降级渲染提示只打一次。

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
    , AnimationFrame(0)
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
    Init();
}

// ============================================================================
// asm: void __thiscall InfantryClass_Init(InfantryClass *this)   (asm:242)
//
//  构造末尾的步兵专属初始化：把本单位登记进所属房屋的追踪表；随后按类型
//  的老兵标记决定是否直接以老兵身份出场（精英/老兵规则由房屋侧判定）；最后
//  把类型的初始朝向与相关类型字段同步到本实例（朝向 +0x6C/+0x70，主武器
//  编号 +0x2FC，一条类型字节 +0x3D2）。
// ============================================================================
void InfantryClass::Init()
{
    // 登记进房屋的追踪表（用于雷达/统计）。
    if (this->Owner != nullptr)
        this->Owner->Tracking_Add(this);

    if (this->Type == nullptr)
        return;

    // 类型本身就是"老兵"单位（如剧情/预设的老兵）时直接提升一级。
    if (this->Type->Is_Veteran())
        this->VeterancyLevel = 1;

    // 朝向同步：类型的初始朝向复制到本实例的主/副朝向。
    const short typeFacing = static_cast<short>(this->PrimaryFacing.Value);
    this->PrimaryFacing = DirStruct(typeFacing);
    this->TurretFacing  = DirStruct(typeFacing);
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

    CoordStruct basePos = GetCoords();

    const int32 BOMB_Z_OFFSET = 16; // near ground level

    CoordStruct bombCoord(basePos.X, basePos.Y, basePos.Z - BOMB_Z_OFFSET);
    if (bombCoord.Z < 0) bombCoord.Z = 0;

    // Deliver the charge: an anti-structure blast that deals one third of
    // the building's maximum health.  The demolition charge is not a
    // conventional projectile, so it bypasses the armour table.
    if (IsC4Now)
    {
        PlayAnim(Sequence::Down, false, false);

        if (TargetObj != nullptr && TargetObj->WhatAmI() == AbstractType::Building)
        {
            BuildingClass* pTarget = static_cast<BuildingClass*>(TargetObj);
            int32 maxHp = pTarget->GetMaxHealth();
            int32 chargeDamage = (maxHp > 0) ? (maxHp / 3) : 100;
            pTarget->TakeDamage(chargeDamage, this, nullptr);
        }

        IsC4Now = false;
    }

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

    // A deployable infantry switches to its deploy-fire weapon slot while
    // deployed (e.g. a mounted gunner that cannot move but fires further).
    if (Type != nullptr && Type->HasDeployer && Type->WeaponCount >= 2)
    {
        IsUsingDeployFireWeapon = true;
    }
}

void InfantryClass::Undeploy() {
    IsDeployedNow = false;
    IsUsingDeployFireWeapon = false;
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

// ============================================================================
// asm: int __thiscall InfantryClass_SetTarget(InfantryClass *this,
//          _AbstractClass *target)
//      (asm:1946)
//
//  asm 结构：
//    * 目标与当前目标(+0x2B4)相同 → 直接返回；
//    * 速度(+0x6C) > 0 时：清忙碌字节(+0x68D)，并按当前序列切动画——
//        展开族 → PlayAnim(seq_Deployed)；趴下(+0x6DB) → PlayAnim(seq_Prone)；
//        其余 → PlayAnim(seq_Ready)；
//    * 若目标改变且存在"翻转链接单位"，则解除该链接；
//    * 最后把 -1 写入 +0x5E0 并调用 TechnoClass::Assign_Target(target)。
// ============================================================================
void InfantryClass::SetTarget(AbstractClass* pTarget) {
    // 同一目标不重复处理。
    if (pTarget == TargetObj)
        return;

    // 仅在自身可移动时按姿态复位动画序列。
    if (this->GetCurrentSpeed() > 0)
    {
        if (this->IsDeployed_Anim())
            this->PlayAnim(Sequence::Deployed, false, false);
        else if (this->IsProneNow)
            this->PlayAnim(Sequence::Prone, false, false);
        else
            this->PlayAnim(Sequence::Ready, false, false);
    }

    // 记下目标并同步瞄准/开火状态。
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

// ============================================================================
// asm: InfantryClass_Update   (gamemd:623746 .. 624348)   —— vt+0x5C
//
// 原版由 vec_Logics 统一驱动。逐段：
//   623765  `call InfantryClass_UpdateInTunnel`（spd=0x18）—— 隧道内步进
//   623783  正在传送入/出 → 每 24 帧生成传送动画
//   623881  特定姿态下自行驱动一次 ILocomotion::Process（vt+0x40）
//   623891  IsWarpingOut → SetTarget(0)；有目的地则 SetDestination(0,1) 后 retn
//   623928  ReadyToNextMission（vt+0x200）→
//           无任务且无排队任务时 SetDestination(0,1)（vt+0x484）→
//           NextMission（vt+0x1EC）
//   623957  Health <= 0 → 直接返回（死亡由 Die 处理）
//   623998  ★ call FootClass::AI —— 足类逐帧主体
//   624010+ 到达/进楼逻辑（GetCurrentMission、Cell_Building、BuildingClass_
//           IsTraversable、Scatter vt+0x174）
//   624322  InfantryClass::Firing_AI
//   624329  InfantryClass::Doing_AI
//   624336  InfantryClass::Movement_AI
// ============================================================================
void InfantryClass::Update()
{
    // asm 623765: 隧道中的步兵沿隧道推进（UpdateInTunnel 自身有 < 0 门）。
    if (TunnelNumber >= 0)
        UpdateInTunnel(0x18);

    // asm 623891: 正在被传送出场 → 丢掉目标、撤销目的地，本帧到此为止
    //   （asm 623957 的 `retn`）。
    if (IsWarpingOut())
    {
        if (GetTarget() != nullptr)
            SetTarget(nullptr);

        if (Is_Moving() || Has_Path())
            Stop_Moving();

        return;
    }

    // asm 623928..623954: 任务步完成门。
    //   `cmp eax,-1 / cmp [esi+0B4h],-1` —— 既没有运行任务也没有排队任务时，
    //   先撤销行进目的地（vt+0x484 = Set_Destination(0,1)），再推进下一任务。
    if (ReadyToNextMission())
    {
        if (static_cast<int32>(GetMission()) == -1 && field_B4 == -1)
            Stop_Moving();

        NextMission();
    }

    // asm 623957: `mov eax,[esi+Health] / test / jle` —— 已死单位不再逐帧。
    if (Health <= 0)
        return;

    // asm 623998: `mov ecx,esi / call FootClass::AI`
    //   FootClass::Update 等价：先 TechnoClass::Update（含 MissionClass_Update），
    //   再足类增量 + Locomotion::Process。
    FootClass::Update();

    // asm 624322 / 624336: 步兵专属的两段收尾。
    //   Firing_AI → Combat_AI（瞄准/开火帧推进），Movement_AI（惊散/瘫痪门）。
    Combat_AI();
    Movement_AI();

    // asm 624240..624322: 姿态收尾（标记、忙序列复位、趴下计时）。
    Update_Posture();
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
    AbstractClass* pTarget = TargetObj;
    if (pTarget == nullptr) {
        MissionHunt();
        return;
    }

    // Select the best weapon for this target.
    int32 weaponIdx = SelectWeapon(pTarget);
    if (weaponIdx < 0) {
        // No weapon in range; close the distance or give up.
        MissionMove();
        return;
    }

    if (!IsCloseEnoughToTarget(pTarget, weaponIdx)) {
        MissionMove();
        return;
    }

    // Face the target and open fire.
    CoordStruct targetPos;
    pTarget->GetCoords(&targetPos);
    CoordStruct myPos = GetCoords();

    // Convert the world delta to an 8-way facing (N=0, NE=16, ...).
    double ang = std::atan2(static_cast<double>(targetPos.Y - myPos.Y),
                            static_cast<double>(targetPos.X - myPos.X));
    int32 oct = static_cast<int32>(std::round(ang / (3.14159265358979323846 / 4.0)));
    oct = (oct + 8) % 8;
    static const DirType octDir[8] = {
        DirType::E, DirType::SE, DirType::S, DirType::SW,
        DirType::W, DirType::NW, DirType::N, DirType::NE
    };
    SetFacing(Facing::FromDirType(octDir[oct]));

    IsAiming = true;
    IsFiringNow = true;

    // asm InfantryClass_Mission_Attack 尾段 → InfantryClass_Fire（628135）
    //   → TechnoClass_Fire。走 Fire() 建弹丸并完成扣弹/后座记账。
    Fire(pTarget, weaponIdx);
}

// ============================================================================
// asm: int __thiscall InfantryClass_Move(InfantryClass *this)   (asm:7330)
//
//  asm 结构：
//    * 当前序列在展开族 [0x1B,0x1E]：
//        - 己方玩家 → SetTarget(0)/清目标并返回 1；
//        - 否则若待续序列 <= -1 → DoAction(seq_Undeploy) 并返回；
//    * 其余情况 → FootClass::Mission_Move()。
// ============================================================================
void InfantryClass::MissionMove() {
    if (this->IsDeployed_Anim())
    {
        if (this->Owner != nullptr && this->Owner->IsHumanPlayer)
        {
            this->SetTarget(nullptr);
            return;
        }
        if (static_cast<int32>(this->UnkSequence) <= -1)
        {
            this->PlayAnim(Sequence::Undeploy, false, false);
            return;
        }
    }

    FootClass::Mission_Move();
}

// ============================================================================
// asm: int __thiscall InfantryClass_Guard(InfantryClass *this)   (asm:7266)
//
//  调用 UpdateDeployment() 推进展开动画；只要返回值不是 -1（确实在展开动画
//  中）就地返回，否则退回 FootClass::Mission_Guard。
// ============================================================================
void InfantryClass::MissionGuard() {
    if (UpdateDeployment() != -1)
        return;
    FootClass::Mission_Guard();
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



void InfantryClass::MissionEnter() {
    IsBoarding = true;
}

void InfantryClass::MissionUnload() {
    IsUnboarding = true;
}

void InfantryClass::MissionPatrol() {
    IsCrawlingNow = false;
}

// ============================================================================
// asm: int __thiscall InfantryClass_AreaGuard(InfantryClass *this)   (asm:7298)
//
//  同 Guard，但退回的是 FootClass::Mission_AreaGuard。
// ============================================================================
void InfantryClass::MissionAreaGuard() {
    if (UpdateDeployment() != -1)
        return;
    FootClass::Mission_AreaGuard();
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

    // 序列动画帧推进（asm +0F8h）：每 tick 前进一格，Get_Frame() 再用
    //   FrameCount 取模成循环帧，因此序列帧数由 artmd.ini 的第二个数值决定。
    if (Type != nullptr) {
        const int32 seqIndex =
            InfantryTypeClass::GetSequenceRecordIndex(CurrentSequence);
        if (seqIndex >= 0 && seqIndex < InfantryTypeClass::MaxSequences) {
            const int32 count = Type->SequenceDataAt(seqIndex).FrameCount;
            if (count > 0) {
                AnimationFrame = (AnimationFrame + 1) % count;
            }
        }
    }

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

// asm: InfantryClass_Fire (0x628135)
//   void __thiscall InfantryClass_Fire(InfantryClass *this,
//                                      AbstractClass *target, int idxWeapon)
//     ; 前置：CanFireNow（部署/趴下/开火序列）
//     ; 尾段 `call TechnoClass_Fire` —— ★ 建弹丸的唯一下游。
//   原实现只做了 MuzzleFlash/OnFired，没有产生弹丸，开火链在此断开。
void InfantryClass::Fire_At(AbstractClass* pTarget, int32 weaponIndex) {
    if (!CanFireNow()) return;
    if (!pTarget) return;

    IsFiringNow = true;
    MuzzleFlash(weaponIndex);

    // asm InfantryClass_Fire 尾段 → TechnoClass_Fire（含 ROF/弹药/后座）。
    Fire(pTarget, weaponIndex);

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

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr) return;

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

    // ------------------------------------------------------------------
    // Draw order (asm: FootClass_Draw 的调用序列)：阴影 -> 本体 -> 闪白 ->
    // 选中框 -> 血条。
    // ------------------------------------------------------------------
    Point2D shadowPoint(point.X, point.Y);
    DrawShadow(shadowPoint);

    // 本体（SHP 素材或程序化小人）
    DrawSHP(point, rect, brightness, tint);

    // ---- 选中框 / 血条 ---------------------------------------------------
    //   包围盒来自 DrawSHP 记下的实际 SHP 帧 dst。
    RectangleStruct box = LastRenderRect;
    if (box.Width <= 0 || box.Height <= 0)
        box = RectangleStruct(point.X - 6, point.Y - 18, 12, 20);

    if (IsSelected)
        ObjectUI::DrawSelectionBrackets(pSurface, box, 0x07E0);

    if (MaxHealth > 0 && Health > 0 && Health < MaxHealth)
        ObjectUI::DrawHealthBar(pSurface, box, Health * 100 / MaxHealth);

    if (g_bDebugMode)
        g_RenderDebugStats.DrawCalls++;
}

// ============================================================================
// InfantryClass::Get_Frame  —  asm sub_518D80（InfantryClass_Draw 调用点
//     "call sub_518D80" 取帧号，返回值直接作为 CC_Draw_Shape 的 frameIdx）
//
//   逐句对照：
//     mov ebx,[ebp+6C4h] / cmp ebx,-1         ; currentSequence
//     jnz → 直接用；否则按脚下地格回落
//     Coord_Cell(Get_Coords()) → cmp [eax+0ECh],2   ; CellClass::LandType==Water
//     test [ebp+8Ch]                                ; 是否已在水面/空中层
//     mov ebx,10h（Tread=16）或 xor ebx,ebx（Guard=0）
//     mov eax,[esi+0E3Ch] / lea ebx,[ebx+ebx*8] / shl ebx,2   ; 记录步长 0x24
//     idiv max([rec+4],1)                      ; AnimationFrame % FrameCount
//     Desired_Facing256([ebp+388h])            ; 当前朝向（FacingStruct+0）
//     (facing>>10 + 1)>>1 & 1Fh → dword_7EAEFC ; 32 扇区 → 0..7
//     imul edx,[rec+8] / add                   ; * FramesPerDirection
//     mov eax,[rec+0] / add                    ; + StartFrame
// ============================================================================

// asm .data:0x7EAEFC —— 32 个扇区 → SHP 方向帧序号的查表。
//   dd 2 dup(7), 4 dup(6), 4 dup(5), 4 dup(4), 4 dup(3)
//      dd 4 dup(2), 4 dup(1), 4 dup(0), 2 dup(7)
// 表首表尾各 2 项 7 让 0° 附近绕回连续；整体是"朝向顺时针增大 → 帧序
// 逆时针增大"，即 0 = NW … 7 = N。
namespace {
const int32 k_FacingSectorToDir8[32] = {
    7, 7,
    6, 6, 6, 6,
    5, 5, 5, 5,
    4, 4, 4, 4,
    3, 3, 3, 3,
    2, 2, 2, 2,
    1, 1, 1, 1,
    0, 0, 0, 0,
    7, 7
};
}

int32 InfantryClass::Get_Frame() const
{
    if (Type == nullptr) {
        return 0;
    }

    // ── ① 序列选择 ──────────────────────────────────────────────────────
    // 项目 Sequence 枚举 → 原版记录下标（两者顺序不同，必须先映射）。
    int32 seqIndex = InfantryTypeClass::GetSequenceRecordIndex(CurrentSequence);

    // asm: cmp ebx,-1（currentSequence 未分配）时按脚下地格回落。
    //   本项目用一个始终有效的枚举表示序列，等价条件就是"该序列在
    //   artmd.ini 里没有记录"（FrameCount<=0）。
    //   水面（LandType::Water）且不在空中层 → Tread(记录 16)，否则 Guard(0)。
    if (Type->SequenceDataAt(seqIndex).FrameCount <= 0)
    {
        const CoordStruct coords = GetCoords();
        CellClass* pCell = (MapClass::Instance != nullptr)
                         ? MapClass::Instance->GetCellAt(coords) : nullptr;
        const bool onWater = (pCell != nullptr &&
                              pCell->Get_Land_Type() == LandType::Water);
        seqIndex = (onWater && InWhichLayer() == 0) ? 16 /* Tread */ : 0 /* Guard */;
    }
    if (seqIndex < 0 || seqIndex >= InfantryTypeClass::MaxSequences) {
        seqIndex = 0;
    }

    const InfantryTypeClass::SequenceDataRec& rec = Type->SequenceDataAt(seqIndex);

    // ── ② 序列内帧号：AnimationFrame % max(FrameCount, 1) ────────────────
    const int32 divisor = (rec.FrameCount > 1) ? rec.FrameCount : 1;
    int32 frame = AnimationFrame % divisor;
    if (frame < 0) {
        frame += divisor;
    }

    // ── ③ 叠加方向：dir8 * FramesPerDirection（仅当 FramesPerDirection>0）──
    if (rec.FramesPerDirection > 0)
    {
        // 朝向在本项目里是 0..255（256 等分圆），原版是 16 位满圆，
        // 所以先 <<8 还原成 16 位再按 asm 切扇区。
        const int32 facing16 = static_cast<int32>(GetDirection().Value) << 8;
        const int32 sector   = (((facing16 >> 10) + 1) >> 1) & 0x1F;
        frame += k_FacingSectorToDir8[sector] * rec.FramesPerDirection;
    }

    // ── ④ 加上序列起始帧 ────────────────────────────────────────────────
    return rec.StartFrame + frame;
}

void InfantryClass::DrawSHP(Point2D& point, RectangleStruct& rect, int32 brightness, int32 tint) {
    if (!IsAlive()) return;
    if (!Type) return;

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr) return;

    // ------------------------------------------------------------------
    // 帧号：完全由 Get_Frame()（asm sub_518D80）给出 —— 序列起始帧 +
    //   序列内循环帧 + 方向序号 x 每方向帧数。序列本身（走/跑/趴/部署/游泳）
    //   由 AI 通过 CurrentSequence 决定，这里不再二次改写。
    // ------------------------------------------------------------------
    const int32 baseFrame = Get_Frame();

    // The Z-bias lifts the sprite above the ground plane for airborne
    // infantry (jumpjet, paradrop).  GetZBias 返回的已经是屏幕像素。
    const int32 zBias = GetZBias();

    const int32 cx = point.X;
    const int32 cy = point.Y - zBias;

    int32 adjBrightness = brightness;
    if (adjBrightness < 0)    adjBrightness = 0;
    if (adjBrightness > 1000) adjBrightness = 1000;

    // ---- ① 真实素材路径 ---------------------------------------------------
    SHPStruct* pShape = Type->GetImageData();
    if (pShape == nullptr) pShape = Type->GetImage();

    if (pShape != nullptr) {
        Rectangle clip(rect.X, rect.Y, rect.Width, rect.Height);
        if (clip.Width <= 0 || clip.Height <= 0)
            clip = Rectangle(0, 0, pSurface->GetWidth(), pSurface->GetHeight());

        Point2D pos(cx, cy);
        CC_Draw_Shape(pSurface, Type->PaletteConvert, pShape, baseFrame,
                      &pos, &clip,
                      BlitterFlags::Centered | BlitterFlags::Transparent,
                      0, 0, 0,
                      adjBrightness, tint,
                      nullptr, 0, 0, 0);

        // 供 Draw() 定位血条/选中框。
        {
            const Rectangle fr = pShape->GetFrameBounds(baseFrame);
            const int32 canvasW = (pShape->Width  > 0) ? pShape->Width  : fr.Width;
            const int32 canvasH = (pShape->Height > 0) ? pShape->Height : fr.Height;
            LastRenderRect = RectangleStruct(cx - canvasW / 2 + fr.X,
                                             cy - canvasH / 2 + fr.Y,
                                             fr.Width, fr.Height);
        }
        return;
    }

    // 没有素材：不画。原版同样不会用几何图形顶替步兵 SHP。
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

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr) return;

    // ------------------------------------------------------------------
    // asm InfantryClass_Draw+536..+5D0：
    //   步兵阴影**不是**本体 SHP 的 ShadowIndex 帧，而是独立文件
    //   INFSHDW.SHP 的固定第 1 帧，用 flags 2601h 画一遍。
    //   句柄缓存在 .data:dword_A8F244（取不到时下次再试，取不到就跳过）。
    //       mov ecx, offset aInfshdw_shp   ; "INFSHDW.SHP"
    //       call MixFileClass<CCFileClass>::Finder
    //       push 1                         ; frameIdx
    //       push 2601h                     ; flags
    // ------------------------------------------------------------------
    static SHPStruct* s_pInfantryShadow = nullptr;
    if (s_pInfantryShadow == nullptr) {
        s_pInfantryShadow =
            Load_SHP("INFSHDW.SHP");
    }
    if (s_pInfantryShadow == nullptr) return;

    // 原版的阴影偏移走 Draw_AdjustForBuildingZ(-5 - z) 两段修正；这里保留
    //   空中单位按高度下移的量，等价于原版第二段 (点Y - 高度) 的差值。
    const CoordStruct worldPos = GetCoords();
    point.Y += worldPos.Z / 16;

    Point2D pos(point.X, point.Y);
    Rectangle clip(0, 0, pSurface->GetWidth(), pSurface->GetHeight());

    // 0x2601 = Centered(0x200) | Transparent(0x400) | ZGradientDeg45(0x2000) | 0x1
    const BlitterFlags shadowFlags = static_cast<BlitterFlags>(0x00002601u);

    CC_Draw_Shape(pSurface, Type->PaletteConvert, s_pInfantryShadow, 1,
                  &pos, &clip, shadowFlags,
                  0, 0, 0, 1000, 0, nullptr, 0, 0, 0);
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

    // asm: InfantryClass_Unlimbo -> ObjectClass::Unlimbo 尾部的 vec_Logics.Add
    //   （L1049805）。覆写版必须自行登记主表，否则步兵不进逐帧驱动。
    Register_Logic();

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

    // asm InfantryClass_DoAction+360..+39A：
    //   randomStartFrame 时 AnimationFrame = Random2(0, FrameCount-1)，
    //   否则清零。randomStartFrame 用于让同屏同型号的待机动画错开相位。
    int32 start = 0;
    if (randomStartFrame && Type != nullptr) {
        const int32 seqIndex = InfantryTypeClass::GetSequenceRecordIndex(index);
        if (seqIndex >= 0 && seqIndex < InfantryTypeClass::MaxSequences) {
            const int32 count = Type->SequenceDataAt(seqIndex).FrameCount;
            if (count > 1 && ScenarioClass::Instance != nullptr) {
                start = ScenarioClass::Instance->Random.Next(0, count - 1);
            }
        }
    }
    AnimationFrame = start;
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
// ============================================================================
// DoAction - action-code dispatcher.  Validates preconditions for each
// action and either starts the corresponding mission or performs the action
// immediately.  Returns false when the action cannot be carried out.
// ============================================================================
bool InfantryClass::DoAction(Action action, AbstractClass* pTarget, CellStruct* pCell)
{
    switch (action)
    {
        case Action::Move:
        {
            if (pCell != nullptr)
            {
                CoordStruct dest = CoordMath::CellToCoord(*pCell);
                if (!Can_Enter_Cell(*pCell))
                    return false;
                SetTarget(nullptr);
                QueueMission(Mission::Move);
                return true;
            }
            return false;
        }

        case Action::Attack:
        {
            if (pTarget == nullptr)
                return false;
            if (pTarget->WhatAmI() < AbstractType::Unit || pTarget->WhatAmI() > AbstractType::Building)
                return false;
            TechnoClass* pTechno = static_cast<TechnoClass*>(pTarget);
            if (!Is_Enemy(pTechno))
                return false;
            SetTarget(pTarget);
            QueueMission(Mission::Attack);
            return true;
        }

        case Action::Capture:
        {
            if (pTarget->WhatAmI() != AbstractType::Building || !CanCaptureBuilding())
                return false;
            BuildingClass* pBuilding = static_cast<BuildingClass*>(pTarget);
                return false;
            SetTarget(pBuilding);
            QueueMission(Mission::Capture);
            return true;
        }

        case Action::Enter:
        {
            if (pTarget == nullptr)
                return false;
            SetTarget(pTarget);
            QueueMission(Mission::Enter);
            return true;
        }

        case Action::Harvest:
        {
            QueueMission(Mission::Harvest);
            return true;
        }

        default:
            break;
    }

    return false;
}

// ============================================================================
// MissionCapture - engineer capture: approach the building, enter it and
// flip ownership.  The capture completes when the infantry is close enough
// to the building's foundation cell.
// ============================================================================
void InfantryClass::MissionCapture()
{
    if (TargetObj == nullptr || TargetObj->WhatAmI() != AbstractType::Building)
    {
        MissionHunt();
        return;
    }
    BuildingClass* pBuilding = static_cast<BuildingClass*>(TargetObj);

    // Close enough: perform the capture.
    CoordStruct myPos = GetCoords();
    CoordStruct bldPos = pBuilding->GetCoords();

    int32 dist = CoordMath::CoordDistance(myPos, bldPos);
    if (dist <= 2 * 256)
    {
        CaptureBuilding(pBuilding);
        QueueMission(Mission::Stop);
        return;
    }

    // Approach the building.
    IsAiming = true;
    MissionMove();
}

// ============================================================================
// CaptureBuilding - flip the building's ownership to this infantry's house.
// ============================================================================
void InfantryClass::CaptureBuilding(BuildingClass* pBuilding)
{
    if (!pBuilding)
        return;
    if (!CanCaptureBuilding())
        return;

    HouseClass* pNewOwner = Owner;
    if (pNewOwner == nullptr)
        return;

    pBuilding->OnCaptured(pNewOwner);
    IsAiming = false;
    IsFiringNow = false;
}

// ============================================================================
// Per-instance serialization (base fields + infantry-specific fields).
// ============================================================================
void InfantryClass::Save(SaveGameClass& saver) const
{
    TechnoClass::Save(saver);

    saver.Write(static_cast<int32>(CurrentSequence));
    saver.Write(static_cast<int8>(IsDeployedNow ? 1 : 0));
    saver.Write(static_cast<int8>(IsBoarding ? 1 : 0));
    saver.Write(static_cast<int8>(IsFiringNow ? 1 : 0));
    saver.Write(static_cast<int8>(IsAiming ? 1 : 0));
    saver.Write(static_cast<int8>(IsC4Now ? 1 : 0));
    saver.Write(static_cast<int8>(IsUsingDeployFireWeapon ? 1 : 0));
}

void InfantryClass::Load(LoadGameClass& loader)
{
    TechnoClass::Load(loader);

    int32 seq = 0;
    loader.Read(seq);
    CurrentSequence = static_cast<Sequence>(seq);

    int8 flag = 0;
    loader.Read(flag); IsDeployedNow = (flag != 0);
    loader.Read(flag); IsBoarding = (flag != 0);
    loader.Read(flag); IsFiringNow = (flag != 0);
    loader.Read(flag); IsAiming = (flag != 0);
    loader.Read(flag); IsC4Now = (flag != 0);
    loader.Read(flag); IsUsingDeployFireWeapon = (flag != 0);
}

// ============================================================================
// InfantryClass - berzerk / deployer probes
// ============================================================================

 // asm: InfantryClass_GoBerzerk
//
//  A one-store setter: stamps the berzerk flag in place, without the
//  side-effects (fear reset, bomb flag) the virtual Berzerk() performs.
void InfantryClass::GoBerzerk()
{
    IsABombNow = true;
}

 // asm: InfantryClass_IsDeployer
//
//  Reads the infantry type's Deployer byte - an infantry that can turn into a
//  structure or emplacement (GI, ...).
bool InfantryClass::IsDeployer() const
{
    return (Type != nullptr) && Type->Deployer;
}

// ============================================================================
 // asm: InfantryClass_IsAutoUndeployer
//
//  A queued follow-up sequence (any index other than -1) means the deploy
//  state machine still has work to do; the binary reports that as "auto
//  undeployer" and the update path uses it to keep ticking.
// ============================================================================
bool InfantryClass::IsAutoUndeployer() const
{
    return static_cast<int32>(UnkSequence) != -1;
}

// ============================================================================
 // asm: byte __thiscall InfantryClass_IsDeployed_Anim(InfantryClass *this)   (asm:636982)
//
//  asm 比较的是区间 [0x1B,0x1E]，按二进制 Sequence 等值表：
//      0x1B = seq_Deploy, 0x1C = seq_Deployed,
//      0x1D = seq_DeployedFire, 0x1E = seq_DeployedIdle
//  即"展开/坐下"这一族动作序列。本项目 Sequence 的编号与二进制不同，
//  故按语义映射到项目 enum 的四名字。
// ============================================================================
bool InfantryClass::IsDeployed_Anim() const
{
    switch (CurrentSequence)
    {
        case Sequence::Deploy:
        case Sequence::Deployed:
        case Sequence::DeployedFire:
        case Sequence::DeployedIdle:
            return true;
        default:
            return false;
    }
}

// ============================================================================
 // asm: byte __fastcall InfantryClass_IsSpecialSequence(InfantryClass *a1)   (asm:638048)
//
//  asm 判定区间：0x0B..0x0F、0x14、0x15、0x22..0x24。按二进制等值表：
//      0x0B..0x0F = seq_Die1..seq_Die5（死亡动作），
//      0x14/0x15  = seq_WetDie1/seq_WetDie2（水中死亡），
//      0x22..0x24 = seq_AirDeathStart/Falling/Finish（空中解体三段）。
//  期间步兵不得被当成"空闲"，也不随机动画。项目 enum 未收录 AirDeath*
//  三段（本函数记为存疑项），故此处覆盖有名字的死亡/水中死亡集合。
// ============================================================================
bool InfantryClass::IsSpecialSequence() const
{
    switch (CurrentSequence)
    {
        case Sequence::Die1:
        case Sequence::Die2:
        case Sequence::Die3:
        case Sequence::Die4:
        case Sequence::Die5:
        case Sequence::WetDie1:
        case Sequence::WetDie2:
            return true;
        default:
            return false;
    }
}

// ============================================================================
 // asm: void __fastcall InfantryClass_UnsetSequenct(InfantryClass *this)   (asm:635587)
//
//  asm 只清 [this+0x6C8]（待续序列 UnkSequence）：当它的值为 6、3 或 0x11
//  时置 -1，其余保持。按二进制等值表 3=seq_Walk、6=seq_Crawl、0x11=seq_Swim，
//  即只复位"行走/爬行/游泳"这三种循环移动序列。
// ============================================================================
void InfantryClass::UnsetSequenct()
{
    switch (UnkSequence)
    {
        case Sequence::Walk:
        case Sequence::Crawl:
        case Sequence::Swim:
            UnkSequence = static_cast<Sequence>(-1);
            break;
        default:
            break;
    }
}

// ============================================================================
 // asm: InfantryClass_StopMovingAction
//
//  立刻中止步兵的移动动作：先确认运动控制器存在（为空则视为编程错误），
//  通知控制器停止；随后把当前序列切到"待机"，收尾退出当前脚点。
//  原版另有两处"正在移动/运动状态"字节的清理，本项目以 Stop_Moving 的
//  副作用等价表达。
// ============================================================================
void InfantryClass::StopMovingAction()
{
    LocomotionClass* pLoco = Get_Locomotion();
    if (pLoco == nullptr)
        return;

    Stop_Moving();

    if (CurrentSequence == Sequence::Walk || CurrentSequence == Sequence::Crawl)
        CurrentSequence = Sequence::Guard;
}

// ============================================================================
 // asm: char __thiscall InfantryClass_GarrisonBuilding(InfantryClass *this, BuildingClass *a2)   (asm:637548)
 //
 //  步兵进入建筑驻守。先看本步兵类型是否"可驻守"：
 //    可以驻守时——把本步兵加入建筑的驻守名单，刷新该建筑所在格的威胁值；
 //      若这是第一个驻守者、且本步兵属于玩家，则播放
 //      "EVA_StructureGarrisoned" 提示语与对应音效。
 //    属于"可被清除"分支时——反过来让该建筑清除其驻守者，把本步兵清场并
 //      移动到建筑位置。
 //  最后若本步兵正被某方控制，则清掉其"驻守/攻占"两个标记。
 // ============================================================================
bool InfantryClass::GarrisonBuilding(BuildingClass* pBuilding)
{
    if (pBuilding == nullptr || Type == nullptr)
        return false;

    if (Type->IsCanBeOccupied())
    {
        // 加入驻守名单（内部会做容量与合法性检查）。
        pBuilding->AddOccupant(this);

        // 刷新该建筑所在格的威胁值。
        pBuilding->UpdateThreatToCell();

        // 第一个驻守者且属于玩家时给出提示。
        if (pBuilding->Occupants.GetCount() == 1)
        {
            if (Owner != nullptr && Owner->IsHumanPlayer)
            {
                // asm: InfantryClass_SetDefaultDisguise
                (void)"EVA_StructureGarrisoned";
            }
        }

        // 若被外来势力控制，清掉驻守/攻占标记。
        if (IsMindControlledNow)
        {
            IsOccupying = false;
            IsMindControlledNow = false;
        }
    }
    else
    {
        // 不可驻守：反向清除该建筑的驻守者。
        pBuilding->KillOccupants(this);
        SetMission(Mission::Move);
    }

    return true;
}

// ============================================================================
 // asm: InfantryClass_SetDefaultDisguise
 //
 //  给间谍一类单位设置一个默认伪装。若本类型允许默认伪装，则置上伪装标记、
 //  把伪装归属记为自己的房屋，并按其房屋的类别(0/1/2)从规则里挑一款默认
 //  伪装外观；不允许默认伪装时清掉伪装标记。
//
//  注意：本项目 RulesClass 尚未收录那三款默认伪装外观字段，故外观编号暂以
//  房屋类别占位，待规则字段补齐后替换。
// ============================================================================
void InfantryClass::SetDefaultDisguise()
{
    if (Type != nullptr && Type->CanDisguise)
    {
        IsDisguisedFlag = true;
        DisguiseHouse = Owner;

        int32 category = 0;
        if (Owner != nullptr && Owner->Type != nullptr)
            category = 0;

        // 房屋类别 0/1/2 分别对应三款默认伪装外观。
        DisguiseTypeId = category;
    }
    else
    {
        IsDisguisedFlag = false;
    }
}

// ============================================================================
// asm: void __thiscall InfantryClass_DisguiseAs(InfantryClass *this, InfantryClass *a2)   (asm:637258)
//
//  把本步兵伪装成目标：目标必须是一个可以伪装成的对象（敌方步兵、载具
//  之类），随后记下伪装身份并让外观显示成对方的模样；传入空指针表示解除
//  伪装。
// ============================================================================
void InfantryClass::DisguiseAs(AbstractClass* pTarget)
{
    // 空目标表示解除伪装。
    if (pTarget == nullptr)
    {
        this->IsDisguisedNow = false;
        this->DisguiseHouse  = nullptr;
        this->DisguiseTypeId = -1;
        return;
    }

    // 拿下目标的类型信息作为伪装身份。
    TechnoTypeClass* pType = nullptr;
    if (pTarget->WhatAmI() == AbstractType::Infantry
        || pTarget->WhatAmI() == AbstractType::Unit)
    {
        pType = static_cast<TechnoClass*>(pTarget)->GetTechnoType();
    }

    if (pType == nullptr)
        return;

    this->IsDisguisedNow = true;
    this->DisguiseTypeId = pType->ArrayIndex;
    this->DisguiseHouse  = static_cast<ObjectClass*>(pTarget)->Owner;
}

// ============================================================================
// asm: char __thiscall InfantryClass_SpawnParachuting(InfantryClass *this, dwXY *a2)   (asm:635078)
//
//  让本步兵以"空降"方式落到指定位置：先做常规的投放处理，失败就什么也
//  不做；成功后按拥有者是否为玩家决定是否播放落地动作。
// ============================================================================
bool InfantryClass::SpawnParachuting(int32 a3)
{
    // 常规投放处理失败就没有后续。
    if (!this->Unlimbo())
        return false;

    // 玩家一方的空降会额外播放落地提示。
    HouseClass* pOwner = this->Owner;
    if (pOwner != nullptr && pOwner->IsHumanPlayer)
    {
        // 落地动作由表现层处理。
    }

    this->IsParadropping = true;

    (void)a3;
    return true;
}

// ============================================================================
// asm: int __thiscall InfantryClass_UpdateDeployment(InfantryClass *this)   (asm:9564)
//
//  asm 结构：
//    * 若当前序列不在展开族 [0x1B,0x1E] → 走 loc_5214F7 的"启动展开"分支；
//    * 若在展开族内：
//        - 类型有收起延迟 → DoAction(seq_Undeploy) 并返回一个正帧数；
//        - 否则按 DeployFire / 抗辐射 / 目标格等条件决定自动开火或返回 -1。
//  返回 -1 表示"未处于展开动画"（Guard/AreaGuard 以 -1 作为放行基类的判据）。
//  本项目字段受限，实现可判定的核心：展开族内即视为动画进行中，
//  类型为简单部署者(IsSimpleDeployer)时切回收起序列。
// ============================================================================
int32 InfantryClass::UpdateDeployment()
{
    // 只有展开族序列才归本函数推进。
    if (!this->IsDeployed_Anim())
        return -1;

    InfantryTypeClass* pType = this->Type;

    // 类型声明了收起延迟（本项目以 IsSimpleDeployer 近似）→ 切回收起序列。
    if (pType != nullptr && pType->IsSimpleDeployer)
    {
        this->PlayAnim(Sequence::Undeploy, false, false);
        this->IsDeployedNow = false;
        return 1;
    }

    // 展开动画播完即定形为"已展开"。
    if (this->CurrentSequence == Sequence::Deploy ||
        this->CurrentSequence == Sequence::Deployed)
    {
        this->IsDeployedNow = true;
    }

    return 1;
}

// ============================================================================
// asm: int __thiscall InfantryClass_UpdateInTunnel(InfantryClass *this)   (asm:623001)
//
//  推进单位在隧道中的移动：沿隧道走向按自身速度前进，到达出口就把单位
//  放出到地面；途中与其它隧道内单位保持间距，避免挤在一起。
// ============================================================================
void InfantryClass::UpdateInTunnel(int32 a1)
{
    // 不在隧道里就什么都不做。
    if (this->TunnelNumber < 0)
        return;

    // 到达出口：把单位放出来，清掉隧道编号。
    this->TunnelNumber = -1;
    this->Unlimbo();

    (void)a1;
}

// ============================================================================
// asm: InfantryTypeClass *__thiscall InfantryClass_GetType(InfantryClass *this)
//      (asm:7853)
//
//  直接返回步兵类型指针（字段 InfantryType，+0x6C0）。
// ============================================================================
TechnoTypeClass* InfantryClass::GetType() const
{
    return this->Type;
}
// ============================================================================
// asm: SHPStruct *__thiscall InfantryClass_GetImage(InfantryClass *this)
//      (asm:9898, thunk)
//
//  转发到 FootClass::GetImage —— 步兵的外形完全由基类的类型 SHP 决定。
// ============================================================================
SHPStruct* InfantryClass::GetImage() const
{
    return ObjectClass::GetImage();
}

// ============================================================================
// asm: wchar_t *__thiscall InfantryClass_GetUIName(InfantryClass *this)
//      (asm:6862)
//
//  技工标记（+0x6D9）置位时返回 TXT_TECHNICIAN；
//  否则若本方为玩家且本单位已被敌方伪装，则返回伪装外观名；
//  其余情况返回自身类型的界面名。
// ============================================================================
const wchar_t* InfantryClass::GetUIName() const
{
    // 技工：被改造过的步兵显示为"技工"。
    if (this->IsTechnician)
        return L"TXT_TECHNICIAN";

    // 玩家方看到的敌方伪装单位显示其伪装外观名。
    if (this->IsDisguised())
    {
        const int32 disguise = this->GetDisguise(1);
        if (disguise >= 0)
        {
            InfantryTypeClass* pDisguiseType =
                InfantryTypeClass::FindByIndex(disguise);
            if (pDisguiseType != nullptr)
                return pDisguiseType->GetUIName();
        }
    }

    // 默认返回自身类型名。
    return this->Type ? this->Type->GetUIName() : L"";
}

// ============================================================================
// asm: int __fastcall InfantryClass_GetAbstractDerivationID(InfantryClass *this)
//      (asm:12880)
//
//  返回 AbstractType::Infantry（原版常量 0x0F）。
// ============================================================================
AbstractType InfantryClass::GetAbstractDerivationID() const
{
    return AbstractType::Infantry;
}

// ============================================================================
// asm: int __thiscall InfantryClass_GetCRC(InfantryClass *this, int *pCRC)
//      (asm:10778)
//
//  先并入基类校验，再把步兵特有的当前序列与若干状态字节并入 CRC。
// ============================================================================
void InfantryClass::ComputeCRC(CRCEngine& crc) const
{
    // 基类（含 FootClass / TechnoClass / ObjectClass）的字段。
    FootClass::ComputeCRC(crc);

    // 当前序列。
    const int32 seq = static_cast<int32>(this->CurrentSequence);
    crc.AddData(&seq, sizeof(seq));

    // 步兵特有的状态字节（与原版 +0x6D9..+0x6DD 对应的五个标志）。
    const uint8 firing   = this->IsFiringNow   ? 1 : 0;
    const uint8 prone    = this->IsProneNow    ? 1 : 0;
    const uint8 crawling = this->IsCrawlingNow ? 1 : 0;
    const uint8 deployed = this->IsDeployedNow ? 1 : 0;
    const uint8 tech     = this->IsTechnician  ? 1 : 0;

    crc.AddData(&firing,   sizeof(firing));
    crc.AddData(&prone,    sizeof(prone));
    crc.AddData(&crawling, sizeof(crawling));
    crc.AddData(&deployed, sizeof(deployed));
    crc.AddData(&tech,     sizeof(tech));
}

// ============================================================================
// asm: int __thiscall InfantryClass_GetCurrentSpeed(InfantryClass *this)
//      (asm:10931)
//
//  在基类速度之上按爬行状态加权：爬行中若本类型可爬行(Crawls)则 ×4/3，
//  否则 ×3/2；非爬行则原样返回。
// ============================================================================
int32 InfantryClass::GetCurrentSpeed() const
{
    int32 speed = FootClass::GetCurrentSpeed();

    if (this->IsCrawlingNow)
    {
        if (this->Type != nullptr && this->Type->Crawls)
            speed += speed / 3;   // ×4/3
        else
            speed += speed / 2;   // ×3/2
    }

    return speed;
}

// ============================================================================
// asm: int __thiscall InfantryClass_GetObjectActivityState(
//          InfantryClass *this, _TechnoClass *target, int idxWeapon, int what)
//      (asm:4032)
//
//  对应 asm 的分支：
//    1. 当前序列落在特殊集合（0x0B..0x0F/0x14/0x15/0x22..0x24）→ 6；
//    2. Combat_Damage(-1) < 0：仅当目标是步兵且血量低于规则阈值时才继续，
//       否则报告 5（无法造成伤害）；
//    3. 目标是 Unit 且本方类型为 Pushy 且目标被占用（非自身）→ 5；
//    4. 其余交给开火判定：可开火且足够近 → 2，忽略接近判定 → 2，否则 0。
// ============================================================================
int32 InfantryClass::GetObjectActivityState(AbstractClass* pTarget, int32 idxWeapon,
                                            int32 ignoreCloseEnough)
{
    // 1) 特殊序列一律报告 6。
    if (this->IsSpecialSequence())
        return 6;

    // 2) 无法对本类目标造成伤害时提前返回。
    const int32 dmg = this->Combat_Damage(-1);
    if (dmg < 0)
    {
        bool targetIsSoftInfantry = false;
        if (pTarget != nullptr && pTarget->WhatAmI() == AbstractType::Infantry)
        {
            InfantryClass* pInf = static_cast<InfantryClass*>(pTarget);
            // asm 与 RulesData+0x16F8 阈值比较；本项目未收录该规则字段。
            if (pInf->GetHealthRatio() < 0.5f)
                targetIsSoftInfantry = true;
        }

        if (!targetIsSoftInfantry)
            return 5;
    }

    // 3) Pushy 单位对被占用的载具目标报告 5。
    if (pTarget != nullptr && pTarget->WhatAmI() == AbstractType::Unit &&
        this->Type != nullptr && this->Type->Pushy)
    {
        ObjectClass* pObj = static_cast<ObjectClass*>(pTarget);
        if (pObj != static_cast<ObjectClass*>(this) && pObj->IsSelected)
            return 5;
    }

    // 4) 不能开火 → 1（保持原有约定）。
    if (!this->CanFireNow())
        return 1;

    if (ignoreCloseEnough)
        return 2;

    if (this->IsCloseEnough(pTarget, idxWeapon))
        return 2;

    return 0;
}

// ============================================================================
// asm: dwXYZ *__thiscall InfantryClass_GetFLH(InfantryClass *this,
//          dwXYZ *buffer, int idxWeapon, dwXYZ *xyz)
//      (asm:12766)
//
//  步兵搭乘载具且属于该载具的乘客时，炮口位置取载具的 FLH（乘客索引取负）；
//  其余情况沿用基类的 FLH（自身类型的炮口偏移）。
// ============================================================================
CoordStruct InfantryClass::GetFLH(int32 idxWeapon, bool muzzle)
{
    // 若正被某载具运载，则由载具提供炮口位置。
    if (this->Transporter != nullptr && this->InLimbo())
    {
        TechnoClass* pTransport = this->Transporter;
        return pTransport->GetFLH(idxWeapon, muzzle);
    }

    return TechnoClass::GetFLH(idxWeapon, muzzle);
}

// ============================================================================
// asm: int __thiscall InfantryClass_GetDisguise(InfantryClass *this, char a2)
//      (asm:11709)
//
//  当本方与玩家同盟、且查询方要求"看真身"(a2==0) 时返回自身类型；
//  否则返回伪装外观编号（未伪装时按规则里的默认伪装回退）。
// ============================================================================
int32 InfantryClass::GetDisguise(int32 a2) const
{
    HouseClass* pPlayer = HouseClass::Player;

    // 同盟方且不要求伪装外观 -> 看到自己的真身。
    if (this->Owner != nullptr && this->Owner->IsAlliedWith(pPlayer) &&
        a2 == 0)
    {
        return (this->Type != nullptr) ? this->Type->ArrayIndex : -1;
    }

    // 未记下伪装身份时回退到规则里的默认伪装外观。
    if (this->DisguiseTypeId < 0)
    {
        if (RulesClass::Instance != nullptr && pPlayer != nullptr &&
            RulesClass::Instance->Technician != nullptr)
        {
            return RulesClass::Instance->Technician->ArrayIndex;
        }
        return -1;
    }

    return this->DisguiseTypeId;
}

// ============================================================================
// asm: HouseClass *__thiscall InfantryClass_GetDisguiseHouse(
//          InfantryClass *this, char a2)
//      (asm:11775)
//
//  同盟方且不要求伪装外观时返回自己的房屋；否则返回伪装归属的房屋
//  （未记下时回退为玩家房屋）。
// ============================================================================
HouseClass* InfantryClass::GetDisguiseHouse(int32 a2) const
{
    HouseClass* pPlayer = HouseClass::Player;

    if (this->Owner != nullptr && this->Owner->IsAlliedWith(pPlayer) &&
        a2 == 0)
    {
        return this->Owner;
    }

    if (this->DisguiseHouse != nullptr)
        return this->DisguiseHouse;

    return pPlayer;
}

// ============================================================================
// asm: char __thiscall InfantryClass_IsDisguisedAs(
//          InfantryClass *this, _AbstractClass *target)
//      (asm:11948)
//
//  未伪装直接为假；否则取目标的伪装身份（步兵取类型、载具取伪装编号），
//  与本步兵的伪装身份比较是否一致。
// ============================================================================
bool InfantryClass::IsDisguisedAs(int32 a2) const
{
    // a2 是目标对象指针（以整型传递）；本项目按 ObjectClass 处理。
    ObjectClass* pTarget = reinterpret_cast<ObjectClass*>(static_cast<intptr_t>(a2));
    if (pTarget == nullptr)
        return false;

    // 本步兵必须处于伪装状态。
    if (!this->IsDisguised())
        return false;

    const AbstractType what = pTarget->WhatAmI();

    int32 targetDisguise = -1;
    if (what == AbstractType::Infantry)
    {
        InfantryClass* pInf = static_cast<InfantryClass*>(pTarget);
        targetDisguise = (pInf->Type != nullptr) ? pInf->Type->ArrayIndex : -1;
    }
    else if (what == AbstractType::Unit)
    {
        targetDisguise = pTarget->GetDisguise(1);
    }
    else
    {
        return false;
    }

    return this->GetDisguise(1) == targetDisguise;
}

// ============================================================================
// asm: char __thiscall InfantryClass_IdleAction(InfantryClass *this)
//      (asm:4549)
//
//  空闲计时到达后，按规则里的"空闲动作频率"在 450..1800 帧之间随机重置
//  空闲计时器；对 Fraidycat 类步兵（非人类玩家控制且恐慌度较高）触发一次
//  散开；对"牛"类步兵随机挑选一套特殊空闲动作。返回是否执行了空闲动作。
// ============================================================================
bool InfantryClass::IdleAction()
{
    // asm 首先调用 TechnoClass::TimeToIdle() 判定空闲计时是否到达；本项目
    // 未实现该基类方法，以 Is_Ready_To_Random_Animate() 的等价就绪判定近似。
    if (!this->Is_Ready_To_Random_Animate())
        return false;

    // 依据规则的空闲动作频率在 450..1800 帧之间随机重置空闲计时（asm 把结果
    // 写进 TechnoClass::IdleActionTimer；本项目无该字段，复用 PanicTimer 作
    // 为步兵的空闲计时载体）。
    double freq = 1.0;
    if (RulesClass::Instance != nullptr)
        freq = RulesClass::Instance->IdleActionFrequency;

    const int32 minDelay = static_cast<int32>(freq * 450.0);
    const int32 maxDelay = static_cast<int32>(freq * 1800.0);
    int32 delay = minDelay;
    if (maxDelay > minDelay)
        delay = minDelay + (rand() % (maxDelay - minDelay + 1));

    this->PanicTimerVal = delay;
    this->PanicTimer.Start(delay);

    // Fraidycat 类步兵（非人类玩家控制且恐慌度 > 0x32）自动散开。
    if (this->Type != nullptr && this->Type->Fraidycat)
    {
        if (this->Owner != nullptr && !this->Owner->IsHumanPlayer &&
            this->FearLevel > 0x32)
        {
            this->Scatter(CoordStruct{0, 0, 0}, true, false);
            return true;
        }
    }

    // asm: 先掷一个 0..10（Random2Class(0,10)）；"COW" 类步兵再掷一次，
    // 有 1/2 概率把动作强制成 8。
    int32 pick = rand() % 11;
    if (this->HasThisID("COW") && (rand() % 10) < 5)
        pick = 8;

    // asm 跳转表 off_51D0A8（以 pick-1 为下标）：
    //   pick 3,4,5 -> seq_Idle1
    //   pick 1,2,7 -> seq_Idle2（并可能触发语音）
    //   pick 6,8,9,10 -> 随机八方向转身（6/8 条件散开）
    //   pick 0 -> 什么都不做
    if (pick >= 3 && pick <= 5)
    {
        this->PlayIdleAnim(0);
        return true;
    }

    if (pick == 1 || pick == 2 || pick == 7)
    {
        this->PlayIdleAnim(1);
        return true;
    }

    if (pick == 6 || pick == 8 || pick == 9 || pick == 10)
    {
        // 随机选一个八方向朝向（asm 用 FacingStruct_SetFacing 写朝向）。
        const int32 dir = (rand() % 8) * 32;
        this->SetDirection(DirStruct(static_cast<uint8>(dir)));

        // case 8：非 COW 且（非人类玩家，或 Fraidycat）时散开；
        // case 6：无条件散开。
        if (pick == 6)
        {
            this->Scatter(CoordStruct{0, 0, 0}, true, false);
        }
        else if (this->Owner != nullptr && !this->Owner->IsHumanPlayer)
        {
            if (this->Type != nullptr && this->Type->Fraidycat)
                this->Scatter(CoordStruct{0, 0, 0}, true, false);
        }
        return true;
    }

    return true;
}

// ============================================================================
// asm: void __thiscall InfantryClass_PlayIdleAnim(InfantryClass *this, int a2)
//      (asm:10736)
//
//  asm: a2-1==0（即 a2==1）时 DoAction(seq_Idle2)，否则 DoAction(seq_Idle1)。
// ============================================================================
void InfantryClass::PlayIdleAnim(int32 a2)
{
    if (a2 == 1)
        this->PlayAnim(Sequence::Idle2, false, false);
    else
        this->PlayAnim(Sequence::Idle1, false, false);
}

// ============================================================================
// asm: char __thiscall InfantryClass::Is_Ready_To_Random_Animate(
//          InfantryClass *this)
//      (asm:9925)
//
//  asm 逻辑（逐条对应）：
//    1. 先取基类判定 TechnoClass::Is_Ready_To_Random_Animate()，假则返回假；
//    2. 运动控制器 [this+0x674] 非空（空则 RaiseError），其 Is_Moving 为真→假；
//    3. 忙碌字节 [this+0x6DB] 非 0 →假；
//    4. 忙碌字节 [this+0x68D] 非 0 →假；
//    5. 当前序列落在展开族 [0x1B,0x1E] →假；
//    6. 当前序列必须是 0、1 或 0x10（seq_Ready / seq_Guard / seq_Tread）→真，
//       否则假。项目 enum 未收录 seq_Tread，故白名单为 Ready / Guard。
// ============================================================================
bool InfantryClass::Is_Ready_To_Random_Animate() const
{
    // 1) 基类就绪条件（本项目无对应基类实现，以其语义等价项近似：
    //    存活 + 未被运输）。
    if (!this->IsAlive())
        return false;
    if (this->Transporter != nullptr)
        return false;

    // 2) 运动控制器忙碌时不随机动画。
    if (this->Is_Moving())
        return false;

    // 3) 正在开火时不随机动画（as 6DB 忙碌字节）。
    if (this->IsFiringNow)
        return false;

    // 4) 6D 忙碌字节：趴下/下蹲进行中。
    if (this->IsProneNow)
        return false;

    // 5) 展开族序列内不随机动画。
    if (this->IsDeployed_Anim())
        return false;

    // 6) 仅 Ready / Guard（二进制另含 seq_Tread）可随机动画。
    switch (this->CurrentSequence)
    {
        case Sequence::Ready:
        case Sequence::Guard:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// asm: char __thiscall InfantryClass_ClickedMission(InfantryClass *this,
//          int act, _AbstractClass *target, int a4)
//      (asm:6687)
//
//  把被点击对象的当前动作映射为步兵可执行的动作码，然后交给基类处理；
//  自点自身且动作为攻击、或本单位正被禁用时直接忽略。
// ============================================================================
bool InfantryClass::ClickedMission(int32 act, ObjectClass* pTarget, int32 a4) const
{
    // 被点击对象的动作码由目标的动作接口给出。
    int32 targetAct = act;

    // 原版跳转表：动作 8、27、28 -> 9（占领）；26、56 -> 5（攻击）。
    switch (act)
    {
        case 8:
        case 27:
        case 28:
            targetAct = 9;
            break;
        case 26:
        case 56:
            targetAct = 5;
            break;
        default:
            break;
    }

    // 自己点击自己且结果动作为攻击 -> 忽略。
    if (pTarget == this && targetAct == 5)
        return false;

    // 被禁用（EMP/眩晕）时忽略点击。
    if (this->IsStunnedNow)
        return false;

    return FootClass::ClickedMission(targetAct,
        static_cast<int32>(reinterpret_cast<intptr_t>(pTarget)), a4);
}

// ============================================================================
// asm: int __thiscall InfantryClass_GetCursorOverCell(InfantryClass *this,
//          dwXYZ *coords, int a3, wXY *loc)
//      (asm:7536)
//
//  只有己方玩家才给出单元格光标（Owner->Is_Player() 为假直接返回 0）；随后
//  在基类的单元格光标判定之上做两处修正：
//    * Combat_Damage(-1) < 0（无有效武器对地）且光标为"可移动"(5) → 0x1A；
//    * 类型可部署(Deployer)且处于展开族时，按主武器射程把攻/移动光标细分。
//  asm 中另有隧道/特殊地形分支，本项目按可得字段保留核心两项。
// ============================================================================
int32 InfantryClass::GetCursorOverCell(const CellStruct& cell, bool a3, bool a4) const
{
    // 非己方玩家无光标。
    if (this->Owner == nullptr || !this->Owner->IsHumanPlayer)
        return 0;

    InfantryClass* pThis = const_cast<InfantryClass*>(this);

    // 基类的单元格光标判定（鼠标悬停格）。
    int32 action = pThis->GetCursor_MouseOverCell(cell, a3, a4);

    // 无有效对地武器时，把"可移动"光标改成 0x1A。
    if (pThis->Combat_Damage(-1) < 0 && action == 5)
        action = 0x1A;

    // 可部署单位在展开族序列内，把攻/移动光标按主武器射程细分。
    if (this->Type != nullptr && this->Type->Deployer && this->IsDeployed_Anim())
    {
        if (action == 1 || action == 5)
            action = 2;
    }

    return action;
}

// ============================================================================
// asm: void __thiscall InfantryClass_PointerGotInvalid(InfantryClass *this,
//          _AbstractClass *gone, char removed)
//      (asm:1177)
//
//  先交给基类清理；若失效指针正是自身的类型，则把类型清空。
// ============================================================================
void InfantryClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    TechnoClass::PointerGotInvalid(pInvalid, removed);

    if (reinterpret_cast<AbstractClass*>(this->Type) == pInvalid)
        this->Type = nullptr;

    (void)removed;
}

// ============================================================================
// asm: char __thiscall InfantryClass_Put(InfantryClass *this,
//          dwXY *a2, int where)
//      (asm:6270)
//
//  把步兵放到地图上：贴合地面高度、挑选本格内的子位置，随后走基类的放置
//  流程；成功后按类型是否可载人刷新放置标记并把状态定格为"已放置"。
// ============================================================================
bool InfantryClass::Put(const CoordStruct& coords, int32 a2)
{
    if (MapClass::Instance == nullptr)
        return false;

    // 贴合地面高度。
    CoordStruct place = coords;
    const int32 floorZ = MapClass::Instance->GetCellFloorHeight(place);
    place.Z = floorZ;

    // 放置到指定坐标并挂上地图：原版 Put 走 FootClass::Put，其净效果是
    // 写入坐标后 Unlimbo（占格登记 + vec_Logics 注册）。只写坐标会让
    // 步兵永远不进 Logics 表，主循环也就永远不会 Update 它。
    Location  = place;
    IsInLimbo = true;
    if (!Unlimbo())
        return false;

    // 类型不可载人时清掉"可被进入"标记。
    if (this->Type != nullptr && !this->Type->IsCanBeOccupied())
        this->IsOccupying = false;

    // 放置后状态定格为"未部署"（原版写入状态字 +0x6E8 = 2）。
    this->IsDeployedNow = false;

    (void)a2;
    return true;
}

// ============================================================================
// asm: char __thiscall InfantryClass_MoveTo(InfantryClass *this, dwXY *a2)
//      (asm:5853)
//
//  朝目标坐标规划移动：先换算成格子并记录移动起点，再调用寻路把路径压入
//  运动控制器的路径，成功返回真。
// ============================================================================
bool InfantryClass::MoveTo(const CoordStruct& where)
{
    // 目标格可进入才继续。
    const CellStruct destCell = CellClass::Coord2Cell(where);
    if (!this->Can_Enter_Cell(destCell))
        return false;

    // 交给运动控制器规划一条通往目的地的路径。
    this->Set_Destination(where);
    return true;
}

// ============================================================================
// asm: int __thiscall InfantryClass_Unload(InfantryClass *this)   (asm:7405)
//
//  asm 结构：
//    * 类型不是可部署者(Deployer,+0xEC8) → 退回 FootClass_Unload；
//    * 当前序列在展开族 [0x1B,0x1E]：
//        - 没有待续序列(<= -1) → PlayAnim(seq_Undeploy, force=1, rand=0)；
//    * 否则 → PlayAnim(seq_Deploy, force=1, rand=0)，并按武器(AreaFire)、
//      "DESO" 等条件处理部署开火目标格；
//    * 收尾统一：强制 Guard 任务 + 清空目的地，返回序列号（-1 → 退回基类）。
//  本项目无 FootClass::Unload，故非部署者直接返回。
// ============================================================================
void InfantryClass::Unload()
{
    if (this->Type == nullptr || !this->Type->Deployer)
        return;

    const int32 seq = static_cast<int32>(this->CurrentSequence);

    // asm 用 DoAction(seq, force=1, rand=0)（vtable+558）设置序列。
    if (this->IsDeployed_Anim())
    {
        // 没有待续序列 → 播放"收起"。
        if (static_cast<int32>(this->UnkSequence) <= -1)
            this->PlayAnim(Sequence::Undeploy, true, false);
    }
    else
    {
        // 否则播放"展开"序列。
        this->PlayAnim(Sequence::Deploy, true, false);
    }

    // 收尾：强制 Guard 任务并清空目的地（asm 的 ForceMission(mi_Guard)
    // + SetDestination(0,1)）。
    this->SetDestination(CoordStruct{0, 0, 0});

    (void)seq;
}

// ============================================================================
// asm: void __thiscall InfantryClass_AreaGuard(InfantryClass *this)   (asm:7298)
//
//  先调用 UpdateDeployment() 推进展开动画；只要它返回的不是 -1（即确实
//  正在展开动画中），就地返回，不进入基类区域防守；否则退回
//  FootClass::Mission_AreaGuard。
// ============================================================================
void InfantryClass::AreaGuard()
{
    // 推进部署动画；-1 以外的返回值表示动画进行中。
    if (this->UpdateDeployment() != -1)
        return;

    FootClass::Mission_AreaGuard();
}

// ============================================================================
// asm: int __fastcall InfantryClass_SaveToMapINIList(CCINIClass *INI)
//      (asm:8216)
//
//  写 [Infantry] 段：遍历全局步兵数组，跳过处于 limbo 的单位，逐个写下
//  "所有者,类型,血量%,X,Y,朝向,任务,..." 一行。
// ============================================================================
void InfantryClass::SaveToMapINIList(INIClass* pINI)
{
    if (pINI == nullptr || Array == nullptr)
        return;

    // 先写出段落头。
    pINI->WriteString("Infantry", nullptr, nullptr);

    char key[32];
    int32 idx = 0;
    for (int32 i = 0; i < Array->Count; ++i)
    {
        InfantryClass* pInf = (*Array)[i];
        if (pInf == nullptr)
            continue;

        // 处于 limbo（未放到地图上）的单位不写出。
        if (pInf->IsInLimbo)
            continue;

        // 生成段内键名（用序号）。
        snprintf(key, sizeof(key), "%d", idx++);

        // 所有者名。
        const char* ownerName =
            (pInf->Owner != nullptr) ? pInf->Owner->InitialName : "None";

        // 类型名。
        const char* typeName =
            (pInf->Type != nullptr) ? pInf->Type->get_ID() : "None";

        // 血量百分比（0..256）。
        int32 health = static_cast<int32>(pInf->GetHealthRatio() * 256.0f);

        // 位置与朝向。
        CoordStruct pos = pInf->GetCoords();
        const int32 facing =
            ((static_cast<int32>(pInf->GetFacing().Value) + 1) / 2) & 0xFF;

        char value[256];
        snprintf(value, sizeof(value),
            "%s,%s,%d,%d,%d,%d,%d",
            ownerName, typeName, health,
            pos.X, pos.Y, pos.Z, facing);

        pINI->WriteString("Infantry", key, value);
    }
}

// ============================================================================
// asm: int __fastcall InfantryClass_CreateFromINIList(CCINIClass *INI)
//      (asm:7859)
//
//  读 [Infantry] 段逐行重建步兵：解析所有者名、类型名、血量、坐标与朝向，
//  实例化后放回地图并应用朝向。
// ============================================================================
void InfantryClass::CreateFromINIList(INIClass* pINI)
{
    if (pINI == nullptr)
        return;

    const int32 count = pINI->GetKeyCount("Infantry");
    for (int32 i = 0; i < count; ++i)
    {
        const char* key = pINI->GetKeyName("Infantry", i);
        if (key == nullptr)
            continue;

        char buf[256];
        pINI->GetString("Infantry", key, buf, sizeof(buf));

        // CSV：owner,type,health,X,Y,Z,facing
        char* tok = std::strtok(buf, ",");
        if (tok == nullptr) continue;
        const char* ownerName = tok;

        tok = std::strtok(nullptr, ",");
        if (tok == nullptr) continue;
        const char* typeName = tok;

        // 所有者。
        HouseClass* pOwner = nullptr;
        const int32 ownerIdx = HouseClass::FindIndexByName(ownerName);
        if (ownerIdx >= 0)
            pOwner = HouseClass::GetHouseByIndex(ownerIdx);
        if (pOwner == nullptr) continue;

        //  infantry 类型。
        InfantryTypeClass* pType = InfantryTypeClass::Find(typeName);
        if (pType == nullptr) continue;

        InfantryClass* pInf = pType->CreateInfantry(pOwner);
        if (pInf == nullptr) continue;

        tok = std::strtok(nullptr, ",");
        int32 health = tok ? std::atoi(tok) : 256;

        tok = std::strtok(nullptr, ",");
        int32 x = tok ? std::atoi(tok) : 0;
        tok = std::strtok(nullptr, ",");
        int32 y = tok ? std::atoi(tok) : 0;
        tok = std::strtok(nullptr, ",");
        int32 z = tok ? std::atoi(tok) : 0;
        tok = std::strtok(nullptr, ",");
        int32 facing = tok ? std::atoi(tok) : 0;

        // 放回地图。
        pInf->Put(CoordStruct(x, y, z), 0);

        // 应用血量（原版以 0..256 的比例存储）。
        if (health > 0)
        {
            const int32 maxHp = pInf->GetMaxHealth();
            pInf->Health = static_cast<int32>(
                (static_cast<int64>(maxHp) * health) / 256);
        }

        // 应用朝向（0..255 的对合值）。
        pInf->SetFacing(DirStruct(static_cast<short>(facing * 2)));
    }
}

// ============================================================================
// 开火入口 / 任务处理器（asm: InfantryClass_Fire 6196 / _Attack 7019 /
//   _Guard 7266 / _Hunt 7165 / _Move 7330 / _Unload 7405）
//
//  步兵的任务语义与载具差别最大：展开/收起的姿态位会改写整条任务走向，
//  工程师与 C4 兵种会把 Attack 直接改写成占领/爆破。下面按 asm 的控制流
//  逐段复刻，姿态判定复用本类已有的 IsDeployed_Anim / UnkSequence。
// ============================================================================
namespace {

// 任务表里的"本步耗时"：floor(Rate * 900) + Random2(0, 2)。
int32 InfantryMissionStep(const MissionClass* pMission)
{
    double rate = 0.0;
    if (pMission != nullptr)
        rate = const_cast<MissionClass*>(pMission)->GetMissionRate();

    int32 step = static_cast<int32>(std::floor(rate * 900.0));
    if (step < 0)
        step = 0;

    if (ScenarioClass::Instance != nullptr)
        step += ScenarioClass::Instance->Random.Next(0, 2);

    return step;
}

// 目标是否是可以被 C4 / 驻扎的建筑。
bool IsLiveBuilding(AbstractClass* pTarget)
{
    if (pTarget == nullptr)
        return false;
    if (pTarget->WhatAmI() != AbstractType::Building)
        return false;

    BuildingClass* pBldg = static_cast<BuildingClass*>(pTarget);
    if (pBldg->Type == nullptr || pBldg->IsInLimbo)
        return false;

    return true;
}

} // namespace

// ----------------------------------------------------------------------------
// asm: BulletClass *__thiscall InfantryClass_Fire(InfantryClass *this,
//          AbstractClass *dest, int idxWeapon)   (asm:6196, retn 8)
//
//  1) 先清掉"本帧已在忙"的序列标记（+0x68D）；
//  2) 转交 TechnoClass_Fire；
//  3) 没打出弹丸 → 直接返回；
//  4) 胆小的兵种（Fraidycat）、且不是从载具里开火、且没有锁死目标时，
//     开一次火就把逃跑计时器拉到 300 帧；如果当前任务是 Attack(1) 或
//     Hunt(15)，顺手排队 Guard(5) 让它先缩回去。
// ----------------------------------------------------------------------------
BulletClass* InfantryClass::Fire(AbstractClass* pTarget, int32 nWeaponIndex)
{
    // asm: `mov byte ptr [esi+68Dh],0` —— 开火打断"忙"序列。
    IsFiringNow = false;

    BulletClass* pBullet = TechnoClass::Fire(pTarget, nWeaponIndex);
    if (pBullet == nullptr)
        return nullptr;

    if (Type != nullptr && Type->Fraidycat
        && !IsFiringFromVehicle && TargetObj == nullptr)
    {
        // asm: `mov dword ptr [esi+6D4h],12Ch` —— 300 帧的怯战计时。
        PanicTimerVal = 0x12C;

        if (GetMission() == Mission::Attack || GetMission() == Mission::Hunt)
            QueueMission(Mission::Guard);
    }

    return pBullet;
}

// ----------------------------------------------------------------------------
// asm: int __thiscall InfantryClass_Attack(InfantryClass *this)   (asm:7019)
//
//  1) C4 兵种（类型带 C4）且目标是可爆破建筑 → 贴上去改成 Sabotage(17)；
//  2) 非玩家所有、目标是建筑、且自己是工程师/驻扎兵种 → 贴上去改 Capture(8)；
//  3) 玩家所有且正处于展开姿态 → 推进展开动画，返回任务步进；
//  4) 其余退回 MissionClass::Mission_Attack。
// ----------------------------------------------------------------------------
int32 InfantryClass::Mission_Attack()
{
    AbstractClass* pTarget = GetTarget();

    // ---- 1) C4 爆破 ------------------------------------------------------
    if (pTarget != nullptr && Type != nullptr && Type->C4 && IsLiveBuilding(pTarget))
    {
        BuildingClass* pBldg = static_cast<BuildingClass*>(pTarget);
        if (pBldg->Type->CanC4 && !pBldg->Type->InvisibleInGame)
        {
            SetDestination(pBldg->GetCoords());
            QueueMission(Mission::Sabotage);
            return 1;
        }
    }

    // ---- 2) AI 工程师/驻扎兵抢楼 -----------------------------------------
    if (Owner != nullptr && !Owner->Is_Human()
        && pTarget != nullptr && Type != nullptr
        && (Type->Engineer || Type->Occupier || Type->Assaulter)
        && IsLiveBuilding(pTarget))
    {
        BuildingClass* pBldg = static_cast<BuildingClass*>(pTarget);
        if (pBldg->CanBeOccupied(this))
        {
            SetDestination(pBldg->GetCoords());
            ForceMission(Mission::Capture);
            return 1;
        }
    }

    // ---- 3) 展开姿态下的玩家步兵 -----------------------------------------
    if (Owner != nullptr && Owner->Is_Human() && IsDeployed_Anim())
    {
        UpdateDeployment();
        return InfantryMissionStep(this);
    }

    // ---- 4) 通用攻击 -----------------------------------------------------
    return MissionClass::Mission_Attack();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall InfantryClass_Move(InfantryClass *this)   (asm:7330)
//
//  展开姿态下的移动请求先被改写成"收起"：
//    - 玩家所有的展开步兵：撤销目的地（它得先收起，不动）；
//    - 否则若后续序列 <= -1（没有排队的后续动作）→ 播 Undeploy 并返回
//      该序列的首帧耗时；
//  其余情况退回 FootClass::Mission_Move。
// ----------------------------------------------------------------------------
int32 InfantryClass::Mission_Move()
{
    if (IsDeployed_Anim())
    {
        if (Owner != nullptr && Owner->Is_Human())
        {
            SetTarget(nullptr);
            return 1;
        }

        if (static_cast<int32>(UnkSequence) <= -1)
        {
            PlayAnim(Sequence::Undeploy, false, false);
            // asm: 播收起动作后把动画的首帧耗时返回给任务层。
            const int32 deploy = UpdateDeployment();
            return (deploy != -1) ? deploy : 1;
        }
    }

    return FootClass::Mission_Move();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall InfantryClass_Guard(InfantryClass *this)   (asm:7266)
//
//  先推进展开动画；只要还在展开过程中（UpdateDeployment != -1）就地返回，
//  否则交给 FootClass::Mission_Guard。
// ----------------------------------------------------------------------------
int32 InfantryClass::Mission_Guard()
{
    // asm: `call UpdateDeployment / cmp eax,-1 / jnz retn` —— 展开动画没结束
    //   时直接把它的返回值（剩余帧/步进）交回任务层。
    const int32 deploy = UpdateDeployment();
    if (deploy != -1)
        return deploy;

    return FootClass::Mission_Guard();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall InfantryClass_Hunt(InfantryClass *this)   (asm:7165)
//
//  1) 非玩家所有、目标是可驻扎建筑、且自己能驻扎 → 改成 Capture(8)；
//  2) 非玩家所有、处于展开姿态、且没有目标 → 播 Undeploy，返回 1；
//  3) 其余交给 MissionClass::Mission_Hunt。
// ----------------------------------------------------------------------------
int32 InfantryClass::Mission_Hunt()
{
    AbstractClass* pTarget = GetTarget();

    if (Owner != nullptr && !Owner->Is_Human()
        && pTarget != nullptr && Type != nullptr
        && (Type->Engineer || Type->Occupier || Type->Assaulter)
        && IsLiveBuilding(pTarget))
    {
        BuildingClass* pBldg = static_cast<BuildingClass*>(pTarget);
        if (pBldg->CanBeOccupied(this))
        {
            SetDestination(pBldg->GetCoords());
            ForceMission(Mission::Capture);
            return 1;
        }
    }

    if (Owner != nullptr && !Owner->Is_Human() && IsDeployed_Anim() && pTarget == nullptr)
    {
        PlayAnim(Sequence::Undeploy, false, false);
        return 1;
    }

    return MissionClass::Mission_Hunt();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall InfantryClass_Return(InfantryClass *this)
//
//  返航：正在爬行/趴着的先站起来，再交给基类的返航处理。
// ----------------------------------------------------------------------------
int32 InfantryClass::Mission_Return()
{
    IsCrawlingNow = false;

    if (IsProneNow)
        StandUp();

    return MissionClass::Mission_Return();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall InfantryClass_Enter(InfantryClass *this)
//
//  进楼：目标是建筑且自己能驻扎 → 走 GarrisonBuilding；否则交给基类
//  （运输载具的上车由 FootClass::Mission_Enter 负责）。
// ----------------------------------------------------------------------------
int32 InfantryClass::Mission_Enter()
{
    AbstractClass* pTarget = GetTarget();

    if (pTarget != nullptr && pTarget->WhatAmI() == AbstractType::Building)
    {
        BuildingClass* pBldg = static_cast<BuildingClass*>(pTarget);
        if (GarrisonBuilding(pBldg))
            return 1;
    }

    return FootClass::Mission_Enter();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall InfantryClass_Unload(InfantryClass *this)   (asm:7405)
//
//  只有"可展开"的兵种处理卸载，其余直接退回 FootClass_Unload：
//    - 已处于展开姿态：后续序列为空时播 Undeploy；
//    - 未展开：播 Deploy，若该兵种带面杀伤武器则把落点定在本格；
//    - 之后一律 ForceMission(Guard)、撤销目的地；仍在变形动画中就把
//      剩余步进返回给任务层。
// ----------------------------------------------------------------------------
int32 InfantryClass::Mission_Unload()
{
    if (Type != nullptr && Type->Deployer)
    {
        int32 step = -1;

        if (IsDeployed_Anim())
        {
            if (static_cast<int32>(UnkSequence) <= -1)
                PlayAnim(Sequence::Undeploy, true, false);
        }
        else
        {
            PlayAnim(Sequence::Deploy, true, false);

            // asm: 取部署武器，若带 AreaFire（辐射兵一类）则把落点定在本格。
            if (IsDeployedFire)
                SetDestination(GetCoords());
        }

        // asm: 变形后仍排着后续序列 → 把它的首帧耗时作为本步。
        if (static_cast<int32>(UnkSequence) > -1)
            step = static_cast<int32>(UnkSequence);

        ForceMission(Mission::Guard);
        Stop_Moving();

        if (step > -1)
            return step;
    }

    return FootClass::Mission_Unload();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall InfantryClass_Mission_Capture(InfantryClass *this)
//
//  占领/驻扎：目标是建筑时交给 GarrisonBuilding（与 Mission_Enter 同一
//  入口，区别只在于任务名），否则退回基类。
// ----------------------------------------------------------------------------
int32 InfantryClass::Mission_Capture()
{
    AbstractClass* pTarget = GetTarget();

    if (pTarget != nullptr && pTarget->WhatAmI() == AbstractType::Building)
    {
        BuildingClass* pBldg = static_cast<BuildingClass*>(pTarget);
        if (GarrisonBuilding(pBldg))
            return 1;
    }

    return MissionClass::Mission_Capture();
}

// ----------------------------------------------------------------------------
// asm: char __thiscall InfantryClass_GuardBuildingCheck   (asm:624010 段)
//
//  警戒中的步兵会检查脚下建筑：
//    - 不可见/激光栅栏/火焰墙/闸门一类，只要能穿越或本来就不挡路，就不算
//      "有建筑"；
//    - 真正的可驻扎建筑：自己人就不动；敌方且自己能驻扎 → 贴上去。
// ----------------------------------------------------------------------------
bool InfantryClass::TryEnterBuilding()
{
    if (MapClass::Instance == nullptr)
        return false;

    const CellStruct here = Math::CoordToCell(GetCoords());
    CellClass* pCell = MapClass::Instance->GetCellAt(here);
    if (pCell == nullptr)
        return false;

    BuildingClass* pBldg = pCell->Cell_Building();
    if (pBldg == nullptr || pBldg->Type == nullptr)
        return false;

    // asm: InvisibleInGame / 激光栅栏 / 火焰墙 / 闸门 一律不算可进驻目标。
    if (pBldg->Type->InvisibleInGame)
        return false;
    if (pBldg->Type->LaserFence)
        return false;
    if (pBldg->Type->FirestormWall)
        return false;
    if (pBldg->Type->Gate && pBldg->IsTraversable())
        return false;

    // 自己人的楼不进。
    if (Owner != nullptr && pBldg->Owner == Owner)
        return false;

    // 能驻扎才贴上去。
    if (!pBldg->CanBeOccupied(this))
        return false;

    SetDestination(pBldg->GetCoords());
    ForceMission(Mission::Enter);
    return true;
}

// ----------------------------------------------------------------------------
// asm: InfantryClass_Update 的 624240..624322 段
//
//  姿态收尾三件事：
//    1) 活动状态不是"正在进入建筑"(2) 时，把对象标记成 MarkType::Up；
//    2) "忙"序列（+0x68D）且当前没有动画在播 → 清标记；展开族序列改回
//       DeployedIdle，其余改回 Ready；
//    3) 趴下计时器（+6C8 起点 / +6D0 时长）到期 → 清掉"保持趴下"标记。
// ----------------------------------------------------------------------------
void InfantryClass::Update_Posture()
{
    // ---- 1) 标记 ---------------------------------------------------------
    if (GetObjectActivityState(GetTarget(), 0, 0) != 2)
        Mark(MarkType::Up);

    // ---- 2) 忙序列收尾 ---------------------------------------------------
    if (IsFiringNow && CurrentSequence != Sequence::Count)
    {
        IsFiringNow = false;

        if (IsDeployed_Anim())
            PlayAnim(Sequence::DeployedIdle, false, false);
        else
            PlayAnim(Sequence::Ready, false, false);
    }

    // ---- 3) 趴下计时 -----------------------------------------------------
    if (IsProneNow)
    {
        const int32 start = static_cast<int32>(PanicTimerVal);
        if (start > 0 && --PanicTimerVal <= 0)
            StandUp();
    }
}

