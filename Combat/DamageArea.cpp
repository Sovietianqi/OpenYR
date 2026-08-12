#include "DamageArea.h"
#include "WeaponTypeClass.h"
#include "WarheadTypeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/ObjectClass.h"
#include "../Houses/HouseClass.h"
#include "../Rules/RulesClass.h"
#include "../Game/Game.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Math/CoordStruct.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>

// ============================================================================
// Internal helpers
// ============================================================================
namespace {

// ----------------------------------------------------------------------------
// ArmorForTechno
//
// Resolves the armor type for a techno object. The reconstruction does not
// expose a GetTechnoType() accessor on TechnoClass yet, so we derive a
// sensible default from the runtime type. Buildings default to Medium armor,
// vehicles to Light, infantry to Flak and aircraft to Light. This keeps the
// damage falloff and versus multipliers meaningful.
// ----------------------------------------------------------------------------
int32 ArmorForTechno(TechnoClass* pTechno)
{
    if (!pTechno)
        return static_cast<int32>(Armor::None);

    AbstractType kind = pTechno->WhatAmI();
    switch (kind)
    {
        case AbstractType::Building:  return static_cast<int32>(Armor::Medium);
        case AbstractType::Infantry:  return static_cast<int32>(Armor::Flak);
        case AbstractType::Aircraft:  return static_cast<int32>(Armor::Light);
        case AbstractType::Unit:      return static_cast<int32>(Armor::Light);
        default:                      return static_cast<int32>(Armor::None);
    }
}

// ----------------------------------------------------------------------------
// IsProtectedByIronCurtain
//
// Returns true when the target is currently shielded by the Iron Curtain or
// Force Shield super weapons. Such targets are completely immune to damage.
// TechnoClass tracks the remaining shield duration in the IronCurtainTimer /
// ForceShieldTimer members (decremented each frame by Update_AI); a non-zero
// timer means the shield is still active and the techno is invulnerable.
// ----------------------------------------------------------------------------
bool IsProtectedByIronCurtain(TechnoClass* pTarget, WarheadTypeClass* pWarhead)
{
    (void)pWarhead;
    if (!pTarget)
        return false;

    // Iron Curtain / Force Shield grant absolute immunity while their timer
    // is still running.
    if (pTarget->IsIronCurtained() || pTarget->IsForceShielded())
        return true;

    return false;
}

// ----------------------------------------------------------------------------
// IsProneTechno
//
// Returns true when the target is prone (infantry that has dropped to the
// ground). Prone infantry take reduced damage according to the warhead's
// ProneDamage multiplier. Foot-class units track their sequence; the prone
// state maps to Sequence::Prone.
// ----------------------------------------------------------------------------
bool IsProneTechno(TechnoClass* pTarget)
{
    if (!pTarget)
        return false;

    AbstractType kind = pTarget->WhatAmI();
    if (kind != AbstractType::Infantry)
        return false;

    FootClass* foot = static_cast<FootClass*>(pTarget);
    return foot->GetSequence() == Sequence::Prone ||
           foot->GetSequence() == Sequence::Crawl ||
           foot->GetSequence() == Sequence::FireProne;
}

// ----------------------------------------------------------------------------
// HousesAreFriendly
//
// Returns true when the source and target houses are allied. Used to short
// circuit damage when the affectsAllies flag is false.
// ----------------------------------------------------------------------------
bool HousesAreFriendly(HouseClass* pSource, HouseClass* pTarget)
{
    if (!pSource || !pTarget)
        return false;
    if (pSource == pTarget)
        return true;
    return pTarget->IsAlliedWith(pSource);
}

} // namespace

// ============================================================================
// DamageArea implementation
// ============================================================================

// ----------------------------------------------------------------------------
// ApplyCellDamage
//
// Applies area-of-effect damage to every cell within the warhead's CellSpread
// radius. The damage falls off linearly from the centre to the edge of the
// radius, reaching PercentAtMax of the base damage at the perimeter.
// ----------------------------------------------------------------------------
void DamageArea::ApplyCellDamage(CoordStruct center, int32 damage, TechnoClass* pSource,
                                  WarheadTypeClass* pWarhead, bool affectsAllies,
                                  HouseClass* pSourceHouse) {
    if (!pWarhead) return;
    if (!MapClass::Instance) return;

    float cellSpread = pWarhead->GetCellSpread();
    if (cellSpread <= 0.0f) return;

    float percentAtMax = pWarhead->GetPercentAtMax();
    CellStruct centerCell = Math::CoordToCell(center);

    int32 spreadCells = (int32)ceil(cellSpread);
    int32 maxDistance = spreadCells * 256;

    for (int32 cx = centerCell.X - spreadCells; cx <= centerCell.X + spreadCells; ++cx) {
        for (int32 cy = centerCell.Y - spreadCells; cy <= centerCell.Y + spreadCells; ++cy) {
            if (cx < 0 || cy < 0) continue;
            if (cx >= MapClass::Instance->MapWidth || cy >= MapClass::Instance->MapHeight) continue;

            CellStruct cell(cx, cy);
            CoordStruct cellCenter = Math::CellToCoord(cell);

            int32 dx = cellCenter.X - center.X;
            int32 dy = cellCenter.Y - center.Y;
            int32 distance = (int32)sqrt((double)(dx * dx + dy * dy));

            if (distance > maxDistance) continue;

            float damageMultiplier = 1.0f;
            if (maxDistance > 0) {
                float distanceRatio = (float)distance / (float)maxDistance;
                damageMultiplier = 1.0f - (1.0f - percentAtMax) * distanceRatio;
                if (damageMultiplier < 0.0f) damageMultiplier = 0.0f;
            }

            int32 cellDamage = (int32)(damage * damageMultiplier);
            if (cellDamage < 1) cellDamage = 1;

            CellClass* pCell = MapClass::Instance->GetCellAt(cell);
            if (!pCell) continue;

            ApplyToCell(pCell, cellDamage, pSource, pWarhead, affectsAllies, pSourceHouse);
        }
    }
}

// ----------------------------------------------------------------------------
// ApplyToCell
//
// Applies damage to the occupier and overlay of a single cell. The occupier
// (a techno) receives the full cell damage subject to armor and versus
// multipliers. Tiberium overlays are stripped when the warhead is a tiberium
// destroyer. Walls and bridges are damaged by the appropriate destroyer
// flags.
// ----------------------------------------------------------------------------
void DamageArea::ApplyToCell(CellClass* pCell, int32 damage, TechnoClass* pSource,
                              WarheadTypeClass* pWarhead, bool affectsAllies,
                              HouseClass* pSourceHouse) {
    if (!pCell) return;
    if (!pWarhead) return;

    // Damage the primary occupier (a techno object standing on the cell).
    if (pCell->Occupier) {
        ObjectClass* occupier = pCell->Occupier;
        AbstractType kind = occupier->WhatAmI();
        if (kind == AbstractType::Unit ||
            kind == AbstractType::Infantry ||
            kind == AbstractType::Aircraft ||
            kind == AbstractType::Building)
        {
            ApplyToTechno(static_cast<TechnoClass*>(occupier), damage, pSource,
                          pWarhead, affectsAllies, pSourceHouse);
        }
    }

    // Tiberium overlays are removed by tiberium-destroyer warheads. The
    // cell's TiberiumValue tracks the deposited ore amount; zeroing it
    // effectively clears the field.
    if (pCell->TiberiumValue > 0 && pWarhead->IsTiberiumDestroyerWeapon()) {
        pCell->TiberiumValue = 0;
        pCell->Overlay = -1;
        pCell->OverlayData = 0;
    }

    // Ore destruction mirrors tiberium destruction for ore-type overlays.
    if (pCell->Overlay >= 0 && pWarhead->IsOreDestroyerWeapon()) {
        pCell->Overlay = -1;
        pCell->OverlayData = 0;
        pCell->TiberiumValue = 0;
    }

    // Walls are damaged by wall-destroyer warheads. The cell's land type is
    // reset to Clear so pathfinding treats the gap as passable.
    if (pCell->IsWall() && pWarhead->IsWallDestroyerWeapon()) {
        pCell->Land = LandType::Clear;
        pCell->Overlay = -1;
        pCell->OverlayData = 0;
    }
}

// ----------------------------------------------------------------------------
// ApplyToTechno
//
// Applies damage to a single techno after applying armor, alliance, iron
// curtain and prone modifiers. This is the core damage resolution path used
// by both direct hits and area-of-effect cells.
// ----------------------------------------------------------------------------
void DamageArea::ApplyToTechno(TechnoClass* pTarget, int32 damage, TechnoClass* pSource,
                                WarheadTypeClass* pWarhead, bool affectsAllies,
                                HouseClass* pSourceHouse) {
    if (!pTarget) return;
    if (pTarget->IsDead()) return;
    if (!pWarhead) return;

    // Self-damage is suppressed so splash weapons do not kill their owner.
    if (pTarget == pSource) return;

    // Alliance check: when affectsAllies is false, friendly targets are
    // skipped entirely.
    if (pSourceHouse && pTarget->GetOwningHouse()) {
        if (HousesAreFriendly(pSourceHouse, pTarget->GetOwningHouse()) && !affectsAllies) {
            return;
        }
    }

    // Iron Curtain / Force Shield grant absolute immunity. The target takes
    // no damage at all while the shield is active.
    if (IsProtectedByIronCurtain(pTarget, pWarhead)) {
        return;
    }

    // Resolve the armor type and look up the warhead's versus multiplier.
    int32 armorType = ArmorForTechno(pTarget);
    float verse = pWarhead->GetVersus(armorType);
    int32 finalDamage = (int32)(damage * verse);

    // Apply the prone-damage multiplier for infantry that are prone. Prone
    // infantry take a fraction of the damage defined by the warhead.
    if (IsProneTechno(pTarget)) {
        float proneMult = pWarhead->GetProneDamage();
        finalDamage = (int32)(finalDamage * proneMult);
    }

    // Always deal at least 1 point so the hit registers even against heavy
    // armor, unless the versus multiplier is exactly zero (immune armor).
    if (verse > 0.0f && finalDamage < 1) finalDamage = 1;
    if (finalDamage <= 0) return;

    pTarget->TakeDamage(finalDamage, pSource, pWarhead);

    // Secondary warhead effects. Each effect attaches a timed state to the
    // target (tracked on TechnoClass) so the rendering / AI loops can react.
    // A shielded target is immune to the primary damage above and never
    // reaches this point, so the effects only apply to vulnerable technos.

    // Fire-based warheads ignite the target. The burning state persists for
    // a short window and triggers the panic / scatter response so the unit
    // reacts to being set alight.
    if (pWarhead->IsFireWeapon()) {
        pTarget->SetOnFire(60);   // burn for ~1 second at 60 FPS
        pTarget->Panic();
    }

    // Sparky warheads cause the target to scatter and schedule spark
    // particle spawns for the effect system to emit.
    if (pWarhead->IsSparkyWeapon()) {
        pTarget->SetSparky(3);
        CoordStruct crd = pTarget->GetCoords();
        pTarget->Scatter(crd, false, false);
    }

    // Parasite warheads attach a parasite (e.g. a Tiberian fiend / brute)
    // to the target. The parasite drains health over time until removed.
    if (pWarhead->IsParasite() && !pTarget->IsParasiteAttached()) {
        pTarget->SetParasite();
    }

    // Temporal warheads (chronosphere weapon) freeze the target in time.
    // A temporalized techno cannot move, fire or be targeted normally.
    if (pWarhead->IsTemporal()) {
        pTarget->SetTemporal(90);   // freeze for ~1.5 seconds at 60 FPS
    }

    // Gas warheads leave a lingering gas cloud on the target. Infantry are
    // most affected; vehicles shake it off faster.
    if (pWarhead->IsGasWeapon()) {
        AbstractType kind = pTarget->WhatAmI();
        int32 gasDuration = (kind == AbstractType::Infantry) ? 80 : 40;
        pTarget->SetGas(gasDuration);
    }

    // Radiation warheads irradiate the target directly (in addition to any
    // radiation field laid down by ApplyRadiation). The irradiated state
    // deals residual damage over time.
    if (pWarhead->IsRadiationWeapon()) {
        pTarget->SetRadiation(120);   // irradiate for ~2 seconds at 60 FPS
    }
}

// ----------------------------------------------------------------------------
// ApplyAtLocation
//
// Entry point for damage application. When the warhead has a CellSpread the
// damage is distributed across the affected cells; otherwise it is applied
// directly to the cell containing the location.
// ----------------------------------------------------------------------------
void DamageArea::ApplyAtLocation(CoordStruct location, int32 damage, TechnoClass* pSource,
                                  WarheadTypeClass* pWarhead, bool affectsAllies,
                                  HouseClass* pSourceHouse) {
    if (!pWarhead) return;

    float cellSpread = pWarhead->GetCellSpread();
    if (cellSpread > 0.0f) {
        ApplyCellDamage(location, damage, pSource, pWarhead, affectsAllies, pSourceHouse);
        return;
    }

    if (!MapClass::Instance) return;
    CellStruct cell = Math::CoordToCell(location);
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (!pCell) return;

    ApplyToCell(pCell, damage, pSource, pWarhead, affectsAllies, pSourceHouse);
}

// ----------------------------------------------------------------------------
// CalculateDamageFalloff
//
// Returns the damage value after applying the linear falloff from the centre
// of an explosion. At distance 0 the full base damage is returned; at
// maxDistance the damage is baseDamage * percentAtMax; beyond maxDistance
// the damage is zero.
// ----------------------------------------------------------------------------
int32 DamageArea::CalculateDamageFalloff(int32 baseDamage, int32 distance, int32 maxDistance,
                                          float percentAtMax) {
    if (maxDistance <= 0) return baseDamage;
    if (distance <= 0) return baseDamage;
    float ratio = (float)distance / (float)maxDistance;
    if (ratio > 1.0f) return 0;
    float multiplier = 1.0f - (1.0f - percentAtMax) * ratio;
    if (multiplier < 0.0f) multiplier = 0.0f;
    return (int32)(baseDamage * multiplier);
}

// ----------------------------------------------------------------------------
// IsInRange
//
// Returns true when the target coordinate lies within the cell-spread radius
// of the centre coordinate.
// ----------------------------------------------------------------------------
bool DamageArea::IsInRange(CoordStruct center, CoordStruct target, float cellSpread) {
    int32 dx = target.X - center.X;
    int32 dy = target.Y - center.Y;
    int32 distance = (int32)sqrt((double)(dx * dx + dy * dy));
    int32 maxDistance = (int32)ceil(cellSpread) * 256;
    return distance <= maxDistance;
}

// ----------------------------------------------------------------------------
// ApplyRadiation
//
// Lays down a radiation field centred on the given coordinate. The field
// damages every occupier within a 3-cell radius, with the damage falling off
// linearly from the centre. Radiation persists for the supplied duration and
// is re-applied each tick by the RadSiteClass update loop; this function
// performs the initial application.
// ----------------------------------------------------------------------------
void DamageArea::ApplyRadiation(CoordStruct center, int32 damage, int32 duration,
                                 TechnoClass* pSource, HouseClass* pSourceHouse) {
    if (!MapClass::Instance) return;
    if (damage <= 0) return;
    (void)duration;

    int32 spreadCells = 3;
    CellStruct centerCell = Math::CoordToCell(center);
    int32 maxRadius = spreadCells * LeptonsPerCell; // 768 leptons

    for (int32 cx = centerCell.X - spreadCells; cx <= centerCell.X + spreadCells; ++cx) {
        for (int32 cy = centerCell.Y - spreadCells; cy <= centerCell.Y + spreadCells; ++cy) {
            if (cx < 0 || cy < 0) continue;
            if (cx >= MapClass::Instance->MapWidth || cy >= MapClass::Instance->MapHeight) continue;

            CellStruct cell(cx, cy);
            CellClass* pCell = MapClass::Instance->GetCellAt(cell);
            if (!pCell) continue;

            CoordStruct cellCenter = Math::CellToCoord(cell);
            int32 dx = cellCenter.X - center.X;
            int32 dy = cellCenter.Y - center.Y;
            int32 distance = (int32)sqrt((double)(dx * dx + dy * dy));

            if (distance > maxRadius) continue;

            // Linear falloff: full damage at the centre, zero at the edge.
            float multiplier = 1.0f - (float)distance / (float)maxRadius;
            if (multiplier <= 0.0f) continue;

            int32 radDamage = (int32)(damage * multiplier);
            if (radDamage < 1) continue;

            // Flag the cell as irradiated so the RadSiteClass update loop
            // continues to apply damage over the duration.
            pCell->SetAltFlag(AltCellFlags::IsIrradiated, true);

            if (pCell->Occupier && pCell->Occupier != pSource) {
                ObjectClass* occupier = pCell->Occupier;
                AbstractType kind = occupier->WhatAmI();
                if (kind == AbstractType::Unit ||
                    kind == AbstractType::Infantry ||
                    kind == AbstractType::Aircraft ||
                    kind == AbstractType::Building)
                {
                    TechnoClass* techno = static_cast<TechnoClass*>(occupier);

                    // Iron-curtain / chronosphered units are immune to
                    // radiation while their shield holds.
                    if (IsProtectedByIronCurtain(techno, nullptr))
                        continue;

                    techno->TakeDamage(radDamage, pSource, nullptr);
                }
            }
        }
    }

    (void)pSourceHouse;
}
