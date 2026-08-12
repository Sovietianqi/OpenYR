// =============================================================================
// GlobalFiring.cpp - Global bullet management and firing system
//
// Manages all active projectiles in the game world. Provides firing helpers,
// trajectory calculation, scatter/inaccuracy, bullet update, impact processing,
// and cleanup of orphaned or detonated bullets.
// =============================================================================

#include "GlobalFiring.h"
#include "BulletClass.h"
#include "BulletTypeClass.h"
#include "WeaponTypeClass.h"
#include "WarheadTypeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/FootClass.h"
#include "../Houses/HouseClass.h"
#include "../Rules/RulesClass.h"
#include "../Game/Game.h"
#include "../Map/MapClass.h"
#include "../Math/CoordStruct.h"

#include <cmath>
#include <cstdlib>

// =============================================================================
// Static instance initialization
// =============================================================================
List<BulletClass*> GlobalFiring::Bullets;

// =============================================================================
// Constants
// =============================================================================
static const int32 MAX_BULLETS = 2048;
static const int32 BARREL_OFFSET_GROUND = 128;
static const int32 BARREL_OFFSET_AIR = 200;
static const int32 BARREL_HEIGHT = 85;
static const int32 ARC_HEIGHT_BASE = 400;
static const int32 ARC_HEIGHT_ARTILLERY = 900;
static const int32 SCATTER_INFANTRY = 16;
static const int32 SCATTER_VEHICLE = 32;
static const int32 SCATTER_AIRCRAFT = 24;
static const int32 SCATTER_BUILDING = 8;
static const int32 BULLET_LIFETIME_FRAMES = 600;
static const int32 BULLET_MAX_DISTANCE = 65535;

// =============================================================================
// UpdateAll - Process every active bullet for one game frame
// =============================================================================
void GlobalFiring::UpdateAll() {
    int32 processedCount = 0;
    BulletClass* pBullet = Bullets.First();
    while (pBullet) {
        BulletClass* pNext = pBullet->Next;

        // Safety valve to prevent infinite loops with pathological bullet counts.
        if (processedCount >= MAX_BULLETS) break;
        ++processedCount;

        if (!pBullet->IsActive()) {
            RemoveBullet(pBullet);
        } else if (pBullet->IsDetonated()) {
            RemoveBullet(pBullet);
        } else {
            // Check if the bullet has exceeded its lifetime.
            int32 currentFrame = Game::GetCurrentFrame();
            int32 age = currentFrame - pBullet->CreationFrame;
            if (age > BULLET_LIFETIME_FRAMES) {
                pBullet->Detonate();
                RemoveBullet(pBullet);
            } else {
                // Check if the bullet has traveled beyond its maximum range.
                if (pBullet->DistanceTraveled > BULLET_MAX_DISTANCE) {
                    pBullet->Detonate();
                    RemoveBullet(pBullet);
                } else {
                    pBullet->Update();
                }
            }
        }

        pBullet = pNext;
    }
}

// =============================================================================
// Fire - Create and launch a single bullet from source to target
// =============================================================================
BulletClass* GlobalFiring::Fire(WeaponTypeClass* pWeapon, CoordStruct source,
                                 CoordStruct target, TechnoClass* pOwner) {
    if (!pWeapon) return nullptr;
    if (!pWeapon->Projectile) return nullptr;
    if (!pWeapon->Warhead) return nullptr;

    // Prevent bullet storms from overwhelming the simulation.
    if (Bullets.GetCount() >= MAX_BULLETS) {
        return nullptr;
    }

    BulletTypeClass* pBulletType = pWeapon->Projectile;

    // Allocate the bullet through the game's memory system.
    BulletClass* pBullet = new BulletClass(pBulletType);
    if (!pBullet) return nullptr;

    // Calculate the actual fire position (barrel offset).
    CoordStruct fireCoords = CalculateFireCoords(source, target, pWeapon);

    // Apply scatter for inaccurate weapons.
    if (!pWeapon->IsLaser && !pWeapon->IsElectric) {
        if (pOwner) {
            fireCoords = ApplyScatter(fireCoords, pWeapon, pOwner);
        }
    }

    // Initialize the bullet with all combat parameters.
    pBullet->Init(pBulletType, fireCoords, target, pOwner,
                  pWeapon->Damage, pWeapon->Warhead, pWeapon);
    pBullet->SetRange(pWeapon->Range * 256);
    pBullet->SetWeaponType(pWeapon);

    // Instant-hit weapons travel at effectively infinite speed.
    if (pWeapon->IsLaser || pWeapon->IsElectric || pWeapon->IsRadBeam) {
        pBullet->SetSpeed(999999);
    } else {
        // Use the projectile's defined speed if available.
        int32 projSpeed = pWeapon->GetProjectileSpeed();
        if (projSpeed > 0) {
            pBullet->SetSpeed(projSpeed);
        }
    }

    // Set altitude flags for anti-air or anti-surface projectiles.
    if (pWeapon->IsRadBeam) {
        pBullet->IsInAir = true;
    }

    // Mark the bullet with its creation frame for lifetime tracking.
    pBullet->CreationFrame = Game::GetCurrentFrame();

    // Add to the global bullet list.
    Bullets.AddTail(pBullet);
    return pBullet;
}

// =============================================================================
// FireAt - Fire a weapon from an owner at a target coordinate
// =============================================================================
BulletClass* GlobalFiring::FireAt(WeaponTypeClass* pWeapon, TechnoClass* pOwner,
                                   CoordStruct target) {
    if (!pWeapon || !pOwner) return nullptr;
    if (pOwner->IsDead()) return nullptr;

    CoordStruct source = pOwner->GetCoords();
    return Fire(pWeapon, source, target, pOwner);
}

// =============================================================================
// FireBurst - Fire multiple bullets in a burst pattern
// =============================================================================
BulletClass* GlobalFiring::FireBurst(WeaponTypeClass* pWeapon, TechnoClass* pOwner,
                                      CoordStruct target, int32 burstCount) {
    if (!pWeapon || !pOwner) return nullptr;
    if (pOwner->IsDead()) return nullptr;

    if (burstCount <= 0) burstCount = pWeapon->Burst;
    if (burstCount <= 0) burstCount = pWeapon->GetBurstCount();
    if (burstCount <= 0) burstCount = 1;

    BulletClass* lastBullet = nullptr;
    CoordStruct source = pOwner->GetCoords();

    for (int32 i = 0; i < burstCount; ++i) {
        // Each burst shot gets its own scatter to create a spread pattern.
        lastBullet = Fire(pWeapon, source, target, pOwner);
    }

    return lastBullet;
}

// =============================================================================
// CalculateFireCoords - Determine the actual fire origin with barrel offset
// =============================================================================
CoordStruct GlobalFiring::CalculateFireCoords(CoordStruct source, CoordStruct target,
                                               WeaponTypeClass* pWeapon) {
    if (!pWeapon) return source;

    CoordStruct fireCoords = source;

    // Instant-hit weapons fire directly from the body, no barrel offset.
    if (pWeapon->IsLaser || pWeapon->IsElectric) {
        return fireCoords;
    }

    // Calculate the direction from source to target.
    int32 dx = target.X - source.X;
    int32 dy = target.Y - source.Y;
    double direction = atan2((double)dy, (double)dx);

    // Determine barrel offset based on weapon characteristics.
    double barrelOffset = (double)BARREL_OFFSET_GROUND;

    // Artillery weapons have a higher launch point for arc trajectories.
    if (pWeapon->CellSpread > 0) {
        barrelOffset = (double)BARREL_OFFSET_AIR;
    }

    // Apply the horizontal barrel offset along the fire direction.
    fireCoords.X += (int32)(cos(direction) * barrelOffset);
    fireCoords.Y += (int32)(sin(direction) * barrelOffset);

    // Apply vertical offset based on weapon type.
    if (pWeapon->IsRadBeam) {
        // Radiation beams fire from a higher vantage point.
        fireCoords.Z += BARREL_HEIGHT * 2;
    } else if (pWeapon->CellSpread > 0) {
        // Artillery fires upward for arc trajectory.
        fireCoords.Z += ARC_HEIGHT_ARTILLERY;
    } else if (pWeapon->IsSonic) {
        // Sonic weapons have a moderate launch height.
        fireCoords.Z += BARREL_HEIGHT + 40;
    } else {
        // Standard weapons have a small barrel height offset.
        fireCoords.Z += BARREL_HEIGHT;
    }

    return fireCoords;
}

// =============================================================================
// ApplyScatter - Apply inaccuracy scatter to a coordinate
// =============================================================================
CoordStruct GlobalFiring::ApplyScatter(CoordStruct coords, WeaponTypeClass* pWeapon,
                                        TechnoClass* pOwner) {
    if (!pWeapon || !pOwner) return coords;

    // Instant-hit weapons have zero scatter.
    if (pWeapon->IsLaser || pWeapon->IsElectric) {
        return coords;
    }

    // Determine the base scatter amount based on the owner type.
    int32 scatterAmount = 0;
    AbstractType ownerType = pOwner->WhatAmI();

    switch (ownerType) {
        case AbstractType::Unit:
            scatterAmount = SCATTER_VEHICLE;
            break;
        case AbstractType::Infantry:
            scatterAmount = SCATTER_INFANTRY;
            break;
        case AbstractType::Aircraft:
            scatterAmount = SCATTER_AIRCRAFT;
            break;
        case AbstractType::Building:
            scatterAmount = SCATTER_BUILDING;
            break;
        default:
            scatterAmount = SCATTER_INFANTRY;
            break;
    }

    // Artillery weapons with cell spread have larger scatter to simulate
    // the inherent inaccuracy of indirect fire.
    if (pWeapon->CellSpread > 0) {
        scatterAmount += pWeapon->CellSpread * 64;
    }

    // Inaccurate projectiles get additional scatter.
    if (!pWeapon->IsInstantHit()) {
        scatterAmount += 8;
    }

    // Apply the scatter with a random offset in both axes.
    if (scatterAmount > 0) {
        int32 range = scatterAmount * 2 + 1;
        int32 offsetX = (rand() % range) - scatterAmount;
        int32 offsetY = (rand() % range) - scatterAmount;
        coords.X += offsetX;
        coords.Y += offsetY;
    }

    return coords;
}

// =============================================================================
// RemoveBullet - Remove a single bullet from the list and free its memory
// =============================================================================
void GlobalFiring::RemoveBullet(BulletClass* pBullet) {
    if (!pBullet) return;
    Bullets.Remove(pBullet);
    delete pBullet;
}

// =============================================================================
// RemoveAllBullets - Clear all bullets from the simulation
// =============================================================================
void GlobalFiring::RemoveAllBullets() {
    BulletClass* pBullet = Bullets.First();
    while (pBullet) {
        BulletClass* pNext = pBullet->Next;
        Bullets.Remove(pBullet);
        delete pBullet;
        pBullet = pNext;
    }
}

// =============================================================================
// GetBulletCount - Return the total number of active bullets
// =============================================================================
int32 GlobalFiring::GetBulletCount() {
    return Bullets.GetCount();
}

// =============================================================================
// GetFirstBullet - Return the head of the bullet list
// =============================================================================
BulletClass* GlobalFiring::GetFirstBullet() {
    return Bullets.First();
}

// =============================================================================
// GetNextBullet - Return the next bullet in the list
// =============================================================================
BulletClass* GlobalFiring::GetNextBullet(BulletClass* pBullet) {
    if (!pBullet) return nullptr;
    return pBullet->Next;
}

// =============================================================================
// ProcessBulletImpacts - Process all bullets that have detonated
// =============================================================================
void GlobalFiring::ProcessBulletImpacts() {
    BulletClass* pBullet = Bullets.First();
    while (pBullet) {
        BulletClass* pNext = pBullet->Next;
        if (pBullet->IsDetonated()) {
            // The bullet has already detonated; trigger any remaining
            // impact effects before removal.
            if (pBullet->WeaponType && pBullet->WeaponType->Anim) {
                // The animation is spawned by the detonation logic in
                // BulletClass::Detonate, so we just clean up here.
            }
            RemoveBullet(pBullet);
        }
        pBullet = pNext;
    }
}

// =============================================================================
// UpdateBulletAnimations - Ensure all active bullets have animation enabled
// =============================================================================
void GlobalFiring::UpdateBulletAnimations() {
    BulletClass* pBullet = Bullets.First();
    while (pBullet) {
        if (pBullet->IsActive() && !pBullet->IsDetonated()) {
            pBullet->IsAnimating = true;

            // Bright flag makes the bullet render with increased luminosity,
            // which is used for tracer rounds at night.
            if (pBullet->WeaponType) {
                if (pBullet->WeaponType->IsLaser || pBullet->WeaponType->IsElectric) {
                    pBullet->Bright = true;
                }
            }
        }
        pBullet = pBullet->Next;
    }
}

// =============================================================================
// CleanupOrphanedBullets - Remove bullets whose owners are gone or dead
// =============================================================================
void GlobalFiring::CleanupOrphanedBullets() {
    BulletClass* pBullet = Bullets.First();
    while (pBullet) {
        BulletClass* pNext = pBullet->Next;

        bool shouldRemove = false;

        // Bullets without an owner are orphaned.
        if (!pBullet->GetOwner()) {
            shouldRemove = true;
        } else if (pBullet->GetOwner()->IsDead()) {
            shouldRemove = true;
        } else if (!pBullet->GetOwner()->IsActive()) {
            // Owner has been deactivated (e.g. sold or destroyed).
            shouldRemove = true;
        }

        // Also remove bullets that have somehow become inactive without detonating.
        if (!shouldRemove && !pBullet->IsActive() && !pBullet->IsDetonated()) {
            shouldRemove = true;
        }

        if (shouldRemove) {
            RemoveBullet(pBullet);
        }

        pBullet = pNext;
    }
}

// =============================================================================
// CanFireAt - Check if a weapon can be fired at a target from an owner
// =============================================================================
bool GlobalFiring::CanFireAt(WeaponTypeClass* pWeapon, TechnoClass* pOwner,
                              TechnoClass* pTarget) {
    if (!pWeapon || !pOwner || !pTarget) return false;
    if (pTarget->IsDead()) return false;
    if (pOwner->IsDead()) return false;
    if (!pTarget->IsActive()) return false;
    if (!pOwner->IsActive()) return false;

    CoordStruct source = pOwner->GetCoords();
    CoordStruct target = pTarget->GetCoords();

    // Check if the target is within range.
    if (!pWeapon->CanFire(source, target)) {
        return false;
    }

    // Check minimum range constraint.
    if (!pWeapon->IsAboveMinimumRange(source, target)) {
        return false;
    }

    // Verify the target is a valid combat target.
    if (!IsValidTarget(pTarget, pOwner)) {
        return false;
    }

    return true;
}

// =============================================================================
// IsValidTarget - Check if a techno is a valid target for an owner
// =============================================================================
bool GlobalFiring::IsValidTarget(TechnoClass* pTarget, TechnoClass* pOwner) {
    if (!pTarget || !pOwner) return false;
    if (pTarget->IsDead()) return false;
    if (!pTarget->IsActive()) return false;

    // Cannot target self.
    if (pTarget == pOwner) return false;

    // Check ownership: same house is not a valid target (no friendly fire).
    HouseClass* targetHouse = pTarget->GetOwningHouse();
    HouseClass* ownerHouse = pOwner->GetOwningHouse();

    if (targetHouse && ownerHouse) {
        if (targetHouse == ownerHouse) return false;

        // Check alliance status.
        if (targetHouse->IsAlliedWith(ownerHouse)) return false;
    }

    // Cloaked targets are only valid if the owner can detect them.
    // A fully cloaked target that is not detected cannot be targeted.
    if (pTarget->Is_Cloaked()) {
        // The owner's sensor/detector capability is checked elsewhere.
        // For now, allow targeting cloaked units if they are allies of
        // an ally or detected by sensors. The IsClearlyVisibleTo check
        // handles visibility determination.
        if (!pTarget->IsClearlyVisibleTo(ownerHouse)) {
            return false;
        }
    }

    return true;
}
