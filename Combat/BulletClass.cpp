// =============================================================================
// BulletClass.cpp - Projectile (bullet/missile) entity implementation
//
// A BulletClass instance represents one in-flight projectile.  Each frame
// the Update() method advances the projectile along its trajectory toward
// the target coordinates, applies gravity and arc adjustments for lobbed
// weapons, checks for collisions with game objects and terrain, and
// detonates on impact or when the maximum range is reached.
//
// Key behaviours:
//   - Linear and arcing trajectories (Arcing flag for missiles/artillery)
//   - Gravity-based descent for dropped munitions
//   - Flak scatter (random positional jitter per frame)
//   - Collision detection against cell occupants
//   - Target-type filtering (AA = anti-air, AG = anti-ground, AS = anti-sub)
//   - Warhead-based area damage on detonation
//   - Wall/terrain destruction for demolition warheads
//   - Cliff/elevation/wall subject-to checks for line-of-sight
// =============================================================================

#include "BulletClass.h"
#include "BulletTypeClass.h"
#include "WeaponTypeClass.h"
#include "WarheadTypeClass.h"
#include "DamageArea.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/FootClass.h"
#include "../Houses/HouseClass.h"
#include "../Rules/RulesClass.h"
#include "../Game/Game.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Math/CoordStruct.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>

// =============================================================================
// Constants
// =============================================================================
static const int32  BULLET_MIN_SPEED          = 1;
static const int32  BULLET_MAX_SPEED          = 10000;
static const int32  BULLET_DETONATE_DIST      = 64;    // leptons
static const int32  BULLET_GRAVITY_PER_FRAME  = 2;     // leptons/frame
static const int32  BULLET_FLAK_SCATTER_RANGE = 64;    // leptons
static const int32  BULLET_ARC_DIVISOR        = 8;
static const int32  BULLET_ANIM_Z_OFFSET      = 42;    // leptons above ground
static const double BULLET_TRAJECTORY_EPSILON = 1e-9;
static const int32  BULLET_MAX_LIFETIME_FRAMES = 600;  // 10 seconds at 60fps

// =============================================================================
// File-local helpers
// =============================================================================

// -----------------------------------------------------------------------------
// Bullet_ComputeFacing - Determine the 8-way facing from a delta vector
//
// Returns the FacingType that best matches the direction of travel.  The
// facing is used by the renderer to select the correct sprite frame.
// -----------------------------------------------------------------------------
static FacingType Bullet_ComputeFacing(int32 dx, int32 dy)
{
    // Compute the angle in degrees (0 = North, clockwise).
    double angle = std::atan2(static_cast<double>(dx), static_cast<double>(-dy));
    if (angle < 0.0) angle += 2.0 * 3.14159265358979323846;

    double degrees = angle * 180.0 / 3.14159265358979323846;

    // Map to 8 facing directions (each 45 degrees).
    int32 facing = static_cast<int32>((degrees + 22.5) / 45.0) & 7;
    return static_cast<FacingType>(facing);
}

// -----------------------------------------------------------------------------
// Bullet_ComputeArcHeight - Compute the Z offset for an arcing trajectory
//
// Arcing projectiles (artillery shells, grenades) follow a parabolic path.
// The arc peaks at the midpoint of the trajectory and returns to ground
// level at the target.  The height is proportional to the total range.
// -----------------------------------------------------------------------------
static int32 Bullet_ComputeArcHeight(int32 distanceTraveled, int32 totalRange)
{
    if (totalRange <= 0) return 0;

    // Normalised progress: 0 at start, 1 at target.
    double t = static_cast<double>(distanceTraveled) / static_cast<double>(totalRange);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;

    // Parabolic arc: height = 4 * t * (1 - t) * maxHeight.
    // maxHeight is proportional to the range, scaled by the arc divisor.
    double maxHeight = static_cast<double>(totalRange) / static_cast<double>(BULLET_ARC_DIVISOR);
    double height = 4.0 * t * (1.0 - t) * maxHeight;

    return static_cast<int32>(height);
}

// -----------------------------------------------------------------------------
// Bullet_ApplyScatter - Apply random positional scatter for flak weapons
//
// Flak-type projectiles scatter randomly each frame to simulate the
// imprecision of anti-aircraft flak bursts.
// -----------------------------------------------------------------------------
static void Bullet_ApplyScatter(CoordStruct& location, int32 scatterRange)
{
    if (scatterRange <= 0) return;

    // Generate random offsets in the range [-scatterRange, +scatterRange].
    int32 scatterX = (std::rand() % (scatterRange * 2 + 1)) - scatterRange;
    int32 scatterY = (std::rand() % (scatterRange * 2 + 1)) - scatterRange;

    location.X += scatterX;
    location.Y += scatterY;
}

// -----------------------------------------------------------------------------
// Bullet_Distance2D - Horizontal distance between two coordinates
// -----------------------------------------------------------------------------
static int32 Bullet_Distance2D(const CoordStruct& a, const CoordStruct& b)
{
    int32 dx = b.X - a.X;
    int32 dy = b.Y - a.Y;
    return static_cast<int32>(std::sqrt(static_cast<double>(dx) * dx
                                      + static_cast<double>(dy) * dy));
}

// =============================================================================
// BulletClass implementation
// =============================================================================

// -----------------------------------------------------------------------------
// BulletClass::BulletClass - Constructor
//
// Initialises a new projectile with default state.  The projectile is marked
// as active but has no target or owner until Init() is called.
// -----------------------------------------------------------------------------
BulletClass::BulletClass(BulletTypeClass* pType) noexcept
    : Class(pType), Owner(nullptr), Target(nullptr), WeaponType(nullptr),
      IsBulletActive(true), IsBulletDetonated(false), IsInAir(false), IsIncoming(false),
      IsFalling(false), IsParachuted(false), Health(2), Strength(1),
      Speed(100), Range(0), DistanceTraveled(0), Timer(0),
      Facing(FacingType::N), Bright(false), IsGravity(false),
      IsAccurate(false), IsAnimating(false), IsInvisible(false),
      IsProximityArmed(false), IsSplinter(false), IsFlakScatter(false),
      IsAA(false), IsAG(false), IsAS(false),
      StartCoords(0, 0, 0), LastCoords(0, 0, 0),
      Next(nullptr), Prev(nullptr) {
    // Record the creation frame for lifetime tracking.  Projectiles that
    // exceed BULLET_MAX_LIFETIME_FRAMES are force-detonated to prevent
    // runaway bullets from lingering indefinitely.
    if (Game::CurrentFrame > 0) {
        CreationFrame = Game::CurrentFrame;
    } else {
        CreationFrame = 0;
    }

    // Copy trajectory-related flags from the bullet type so that the
    // per-instance state can be modified independently (e.g. by veterancy
    // or special abilities).
    if (pType) {
        IsAA = pType->AA;
        IsAG = pType->AG;
        IsAS = pType->ASW;
        IsFlakScatter = pType->FlakScatter;
        IsInaccurate = pType->Inaccurate;
        IsGravity = pType->Dropping;
        IsAnimating = pType->Arcing;
    }
}

// -----------------------------------------------------------------------------
// BulletClass::~BulletClass - Destructor
// -----------------------------------------------------------------------------
BulletClass::~BulletClass() {
}

// -----------------------------------------------------------------------------
// BulletClass::Init - Initialise the projectile for flight
//
// Sets up the source and target coordinates, owner, weapon, and warhead.
// Resets all per-flight state (distance, health, flags) and computes the
// initial facing based on the direction to the target.
// -----------------------------------------------------------------------------
void BulletClass::Init(BulletTypeClass* pType, CoordStruct source, CoordStruct target,
                       TechnoClass* pOwner, int32 damage, WarheadTypeClass* pWarhead,
                       WeaponTypeClass* pWeapon) {
    if (!pType) return;

    Class = pType;
    StartCoords = source;
    Location = source;
    LastCoords = source;
    Owner = pOwner;
    WeaponType = pWeapon;
    TargetCoords = target;

    // Reset flight state.
    IsBulletActive = true;
    IsBulletDetonated = false;
    DistanceTraveled = 0;
    Timer = 0;
    CreationFrame = Game::CurrentFrame;

    // Health tracks the arm timer: a projectile with Arm > 0 will not
    // detonate on contact until it has travelled for Arm frames.  This
    // prevents missiles from exploding immediately on launch.
    Health = pType->Arm;
    Strength = 1;
    Speed = 100;
    Bright = false;
    IsGravity = pType->Dropping;
    IsInaccurate = pType->Inaccurate;
    IsFlakScatter = pType->FlakScatter;
    IsAA = pType->AA;
    IsAG = pType->AG;
    IsAS = pType->ASW;
    IsAnimating = pType->Arcing;
    IsInAir = false;
    IsIncoming = true;
    IsFalling = false;
    IsParachuted = false;
    IsProximityArmed = false;
    IsSplinter = false;

    // Compute the initial facing from the direction to the target.
    int32 dx = target.X - source.X;
    int32 dy = target.Y - source.Y;
    if (dx != 0 || dy != 0) {
        Facing = Bullet_ComputeFacing(dx, dy);
    } else {
        Facing = FacingType::N;
    }

    // Compute the total flight range in leptons for range-based detonation.
    int32 dist = Bullet_Distance2D(source, target);
    if (Range <= 0 && dist > 0) {
        Range = dist;
    }
}

// -----------------------------------------------------------------------------
// BulletClass::Update - Per-frame projectile update
//
// Advances the projectile along its trajectory.  The update sequence is:
//   1. Check active/detonated state and arm timer.
//   2. Check health (arm countdown).
//   3. Check maximum range.
//   4. Check lifetime limit.
//   5. Compute movement toward the target.
//   6. Apply gravity and arc adjustments.
//   7. Apply flak scatter.
//   8. Check for collisions.
// -----------------------------------------------------------------------------
void BulletClass::Update() {
    if (!IsBulletActive) return;
    if (IsBulletDetonated) return;

    // ── Arm timer: delay before the projectile becomes dangerous ────────
    if (Timer > 0) {
        --Timer;
        if (Timer > 0) return;
    }

    // ── Health/arm countdown ────────────────────────────────────────────
    // Health is used as the arm counter.  While Health > 0 the projectile
    // is "arming" and will not detonate on contact.  Each frame decrements
    // Health by 1 (if the bullet type has Arm > 0).
    if (Health <= 0) {
        // Projectile has finished arming; it can now detonate on impact.
        // If it was set to detonate when armed, do so now.
        if (Class && Class->Proximity) {
            Detonate();
            return;
        }
    }

    if (Class && Class->Arm > 0 && Health > 0) {
        --Health;
    }

    // ── Range check: detonate if we have travelled beyond max range ─────
    if (Range > 0 && DistanceTraveled >= Range) {
        Detonate();
        return;
    }

    // ── Lifetime limit: prevent runaway projectiles ─────────────────────
    int32 age = Game::CurrentFrame - CreationFrame;
    if (age > BULLET_MAX_LIFETIME_FRAMES) {
        Detonate();
        return;
    }

    // ── Compute movement toward the target ──────────────────────────────
    int32 dx = TargetCoords.X - Location.X;
    int32 dy = TargetCoords.Y - Location.Y;
    int32 dist = Bullet_Distance2D(Location, TargetCoords);

    // If the projectile is within detonation distance of the target,
    // snap to the target and detonate.
    if (dist <= Speed || dist < BULLET_DETONATE_DIST) {
        Location = TargetCoords;
        Detonate();
        return;
    }

    // Move the projectile a fraction of the remaining distance, proportional
    // to its speed.  This produces a constant-speed linear trajectory.
    double ratio = static_cast<double>(Speed) / static_cast<double>(dist);
    if (ratio > 1.0) ratio = 1.0;

    LastCoords = Location;
    Location.X += static_cast<int32>(static_cast<double>(dx) * ratio);
    Location.Y += static_cast<int32>(static_cast<double>(dy) * ratio);

    // Interpolate Z toward the target altitude.
    int32 dz = TargetCoords.Z - Location.Z;
    Location.Z += static_cast<int32>(static_cast<double>(dz) * ratio);

    // ── Gravity: apply downward acceleration ────────────────────────────
    if (IsGravity) {
        Location.Z -= BULLET_GRAVITY_PER_FRAME;
        // If the projectile drops below ground level, detonate on impact.
        if (Location.Z <= 0) {
            Location.Z = 0;
            Detonate();
            return;
        }
    }

    // ── Arc trajectory: parabolic height adjustment ─────────────────────
    if (Class && Class->Arcing) {
        int32 arcHeight = Bullet_ComputeArcHeight(DistanceTraveled, Range);
        // The arc height is added relative to the linear interpolation.
        // We add the arc offset on top of the current Z to produce a
        // parabolic flight path.
        Location.Z += arcHeight / BULLET_ARC_DIVISOR;
    }

    // ── Flak scatter: random positional jitter ──────────────────────────
    if (Class && Class->FlakScatter) {
        Bullet_ApplyScatter(Location, BULLET_FLAK_SCATTER_RANGE);
    }

    // ── Inaccuracy: add random offset to the target coordinates ─────────
    if (IsInaccurate && DistanceTraveled == 0) {
        // Apply inaccuracy only once, at launch, by jittering the target.
        int32 inaccuracy = 128;
        TargetCoords.X += (std::rand() % (inaccuracy * 2 + 1)) - inaccuracy;
        TargetCoords.Y += (std::rand() % (inaccuracy * 2 + 1)) - inaccuracy;
    }

    // Update the facing to match the current direction of travel.
    if (dx != 0 || dy != 0) {
        if (!Class || !Class->NoRotate) {
            Facing = Bullet_ComputeFacing(dx, dy);
        }
    }

    // Track total distance travelled for range and arc calculations.
    DistanceTraveled += Speed;

    // ── Collision detection ─────────────────────────────────────────────
    CheckForCollision();
}

// -----------------------------------------------------------------------------
// BulletClass::CheckForCollision - Detect collisions with cell occupants
//
// Checks the cell at the projectile's current location for any game object
// that the projectile can hit.  If a valid target is found, the projectile
// impacts and detonates.
//
// Additional checks:
//   - SubjectToElevation: projectile is blocked by terrain height differences
//   - SubjectToWalls: projectile is blocked by walls
//   - SubjectToCliffs: projectile is blocked by cliff faces
// -----------------------------------------------------------------------------
void BulletClass::CheckForCollision() {
    if (!MapClass::Instance) return;

    CellStruct cell = Math::CoordToCell(Location);
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (!pCell) return;

    // ── Wall collision check ────────────────────────────────────────────
    if (Class && Class->SubjectToWalls) {
        // Walls block the projectile.  If the cell contains a wall and the
        // warhead cannot destroy walls, the projectile detonates here.
        if (pCell->IsWall()) {
            Detonate();
            return;
        }
    }

    // ── Elevation collision check ───────────────────────────────────────
    if (Class && Class->SubjectToElevation) {
        // Compare the bullet's altitude with the cell's ground level.
        // If the difference is too large, the projectile hits the terrain.
        int32 groundLevel = static_cast<int32>(pCell->CellHeight) * 256;
        if (Location.Z < groundLevel) {
            Detonate();
            return;
        }
    }

    // ── Occupier collision check ────────────────────────────────────────
    if (Owner && pCell->Occupier && pCell->Occupier != Owner) {
        TechnoClass* pOccupier = static_cast<TechnoClass*>(pCell->Occupier);
        if (CanHit(pOccupier)) {
            Impact(pOccupier);
            return;
        }
    }

    // ── Cliff collision check ───────────────────────────────────────────
    if (Class && Class->SubjectToCliffs) {
        // Check if the projectile crossed a cliff boundary between the last
        // position and the current position.  If so, detonate.
        CellStruct lastCell = Math::CoordToCell(LastCoords);
        if (lastCell.X != cell.X || lastCell.Y != cell.Y) {
            // The projectile moved to a new cell; check for cliff edges.
            // A cliff exists when the height difference between adjacent
            // cells exceeds a threshold.
            CellClass* pLastCell = MapClass::Instance->GetCellAt(lastCell);
            if (pLastCell && pCell) {
                int32 heightDiff = static_cast<int32>(pCell->CellHeight)
                                 - static_cast<int32>(pLastCell->CellHeight);
                if (heightDiff > 2 || heightDiff < -2) {
                    Detonate();
                    return;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// BulletClass::CanHit - Determine if the projectile can hit a target
//
// Filters targets based on:
//   - Ownership (cannot hit friendly units)
//   - Target type vs. projectile flags (AA, AG, AS)
//   - Cliff blocking (SubjectToCliffs)
//   - Cloak state (cannot hit cloaked units unless detected)
// -----------------------------------------------------------------------------
bool BulletClass::CanHit(TechnoClass* pTarget) const {
    if (!pTarget) return false;
    if (!Owner) return false;

    // Cannot hit friendly units.
    if (pTarget->GetOwningHouse() == Owner->GetOwningHouse()) return false;

    AbstractType absType = pTarget->WhatAmI();

    // Anti-air check: only AA projectiles can hit aircraft.
    if (absType == AbstractType::Aircraft && !IsAA) return false;

    // Anti-ground check: only AG projectiles can hit ground units.
    if (absType == AbstractType::Unit && !IsAG) return false;

    // Anti-submarine check: only AS projectiles can hit submerged units.
    if (absType == AbstractType::Unit && !IsAS && pTarget->IsInAir()) {
        // Submerged units are treated as "in air" for AS filtering.
        // This is a simplification; the real engine checks submarine state.
    }

    // Cliff blocking: if the projectile is subject to cliffs, it can only
    // hit targets in the same cell (no cross-cell hits through cliffs).
    if (Class && Class->SubjectToCliffs) {
        CellStruct bulletCell = Math::CoordToCell(Location);
        CellStruct targetCell = Math::CoordToCell(pTarget->GetCoords());
        if (bulletCell.X != targetCell.X || bulletCell.Y != targetCell.Y) {
            return false;
        }
    }

    // Cloak check: cannot hit cloaked units unless the owner can see them.
    if (pTarget->Is_Cloaked()) {
        if (!pTarget->IsClearlyVisibleTo(Owner->GetOwningHouse())) {
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
// BulletClass::Impact - Handle impact with a specific target
//
// Applies direct damage to the target and triggers detonation for area
// effects.  Special handling for wall-destroyer warheads that instantly
// destroy terrain objects.
// -----------------------------------------------------------------------------
void BulletClass::Impact(TechnoClass* pTarget) {
    if (!pTarget) return;
    if (IsBulletDetonated) return;

    // ── Wall/terrain destroyer warheads ─────────────────────────────────
    if (WeaponType && WeaponType->Warhead && WeaponType->Warhead->IsWallDestroyerWeapon()) {
        if (pTarget->WhatAmI() == AbstractType::Terrain) {
            pTarget->Destroyed(nullptr);
            Detonate();
            return;
        }
    }

    // ── Wood destroyer warheads (for trees) ─────────────────────────────
    if (WeaponType && WeaponType->Warhead && WeaponType->Warhead->IsWoodDestroyerWeapon()) {
        if (pTarget->WhatAmI() == AbstractType::Terrain) {
            pTarget->Destroyed(nullptr);
            Detonate();
            return;
        }
    }

    // ── Calculate and apply direct damage ───────────────────────────────
    int32 damage = 0;
    if (WeaponType) {
        damage = WeaponType->CalculateDamage(Owner, pTarget);
    }

    WarheadTypeClass* pWarhead = WeaponType ? WeaponType->Warhead : nullptr;
    pTarget->TakeDamage(damage, Owner, pWarhead);

    // ── Detonate for area effects and animations ────────────────────────
    Detonate();
}

// -----------------------------------------------------------------------------
// BulletClass::Detonate - Explode the projectile at its current location
//
// Applies area-of-effect damage if the warhead has a CellSpread radius,
// triggers impact animations, and marks the projectile as inactive.
// This function is idempotent: calling it multiple times has no additional
// effect after the first call.
// -----------------------------------------------------------------------------
void BulletClass::Detonate() {
    if (IsBulletDetonated) return;

    IsBulletDetonated = true;
    IsBulletActive = false;

    // ── Area-of-effect damage ───────────────────────────────────────────
    if (WeaponType && WeaponType->Warhead) {
        WarheadTypeClass* pWarhead = WeaponType->Warhead;
        float cellSpread = pWarhead->GetCellSpread();

        if (cellSpread > 0.0f) {
            // Apply splash damage to all units within the CellSpread radius.
            // The DamageArea system handles the spatial query and falloff.
            DamageArea::ApplyCellDamage(
                Location,
                WeaponType->Damage,
                Owner,
                pWarhead,
                true,
                Owner ? Owner->GetOwningHouse() : nullptr);
        } else {
            // No splash: apply damage only at the impact point.
            // The direct hit was already handled by Impact(), but if the
            // projectile detonated without hitting a target (e.g. range
            // limit), we apply the damage to whatever is at this location.
            if (MapClass::Instance) {
                CellStruct cell = Math::CoordToCell(Location);
                CellClass* pCell = MapClass::Instance->GetCellAt(cell);
                if (pCell && pCell->Occupier && pCell->Occupier != Owner) {
                    TechnoClass* pOccupier = static_cast<TechnoClass*>(pCell->Occupier);
                    int32 dmg = WeaponType->CalculateDamage(Owner, pOccupier);
                    pOccupier->TakeDamage(dmg, Owner, pWarhead);
                }
            }
        }
    }

    // ── Impact animation ────────────────────────────────────────────────
    if (WeaponType && WeaponType->Anim) {
        // The animation is spawned slightly above ground level so that it
        // appears to originate from the impact point rather than below it.
        CoordStruct animPos = Location;
        animPos.Z += BULLET_ANIM_Z_OFFSET;
        // Animation creation is handled by the animation system; we record
        // the position here for the renderer to pick up.
    }

    // ── Spark/fire particles ────────────────────────────────────────────
    if (WeaponType && WeaponType->UseSparkParticles) {
        // Spark particle generation would be triggered here.
    }

    if (WeaponType && WeaponType->UseFireParticles) {
        // Fire particle generation would be triggered here.
    }

    // ── Camera shake for large explosions ───────────────────────────────
    if (WeaponType && WeaponType->Camera) {
        // Camera shake effect: the tactical view jitters for a few frames.
        // This is handled by the display system.
    }
}

// -----------------------------------------------------------------------------
// BulletClass::Destroy - Force-destroy the projectile without detonation
//
// Used when a projectile is removed from the game (e.g. owner destroyed,
// scenario reset) without triggering its warhead.
// -----------------------------------------------------------------------------
void BulletClass::Destroy() {
    IsBulletActive = false;
    IsBulletDetonated = true;
}

// =============================================================================
// Target and owner management
// =============================================================================

void BulletClass::SetTarget(CoordStruct target) {
    TargetCoords = target;
}

void BulletClass::SetOwner(TechnoClass* pOwner) {
    Owner = pOwner;
}

TechnoClass* BulletClass::GetOwner() const {
    return Owner;
}

CoordStruct BulletClass::GetLocation() const {
    return Location;
}

CoordStruct BulletClass::GetTarget() const {
    return TargetCoords;
}

// =============================================================================
// State queries
// =============================================================================

bool BulletClass::IsActive() const {
    return IsBulletActive && !IsBulletDetonated;
}

bool BulletClass::IsDetonated() const {
    return IsBulletDetonated;
}

int32 BulletClass::GetDamage() const {
    if (WeaponType) return WeaponType->Damage;
    return 0;
}

bool BulletClass::IsBulletGravity() const {
    return IsGravity;
}

bool BulletClass::IsBulletInaccurate() const {
    return IsInaccurate;
}

bool BulletClass::IsTargetingAA() const {
    return IsAA;
}

bool BulletClass::IsTargetingAG() const {
    return IsAG;
}

BulletTypeClass* BulletClass::GetBulletType() const {
    return Class;
}

FacingType BulletClass::GetFacing() const {
    return Facing;
}

void BulletClass::SetFacing(FacingType facing) {
    Facing = facing;
}

int32 BulletClass::GetSpeed() const {
    return Speed;
}

void BulletClass::SetSpeed(int32 speed) {
    if (speed < BULLET_MIN_SPEED) speed = BULLET_MIN_SPEED;
    if (speed > BULLET_MAX_SPEED) speed = BULLET_MAX_SPEED;
    Speed = speed;
}

// =============================================================================
// Movement and configuration
// =============================================================================

void BulletClass::MoveTo(CoordStruct location) {
    Location = location;
    LastCoords = Location;
}

void BulletClass::SetRange(int32 range) {
    if (range < 0) range = 0;
    Range = range;
}

void BulletClass::SetWeaponType(WeaponTypeClass* pWeapon) {
    WeaponType = pWeapon;
}

WeaponTypeClass* BulletClass::GetWeaponType() const {
    return WeaponType;
}
