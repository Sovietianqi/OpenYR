#include "CrateClass.h"

#include "../Abstract/OverlayClass.h"
#include "../Abstract/OverlayTypeClass.h"
#include "../Game/Externs.h"
#include "../Game/GameInit.h"
#include "../Map/CellClass.h"
#include "../Map/MapClass.h"
#include "../Rendering/TacticalClass.h"
#include "../Rules/RulesClass.h"

#include <cmath>
#include <cstdlib>

// ============================================================================
// Static data
// ============================================================================

// The original stores the "no crate" sentinel in Crate_Default_CellCoords.
const CellStruct CrateClass::Crate_Default_CellCoords(static_cast<int16>(-1),
                                                      static_cast<int16>(-1));

// The two scaling constants the spawn delay interpolates between.
static constexpr double CRATE_REGEN_HIGH = 1800.0;
static constexpr double CRATE_REGEN_LOW  = 450.0;
static constexpr double CRATE_RAND_MAX   = 2147483647.0;

// ============================================================================
// CrateClass::CrateClass
//
//   The default state marks the spawn as idle and clears the cell.
// ============================================================================
CrateClass::CrateClass()
    : SpawnFrame(-1)
    , SpawnTime(0)
    , SpawnTimeLeft(0)
    , CellCoords(Crate_Default_CellCoords)
{
}

// ============================================================================
 // CrateClass::Remove_It -
//
//   If the tracked cell is not the default, remove the overlay occupying it and
//   reset the cell.  When a spawn was pending (SpawnFrame != -1) the remaining
//   delay is reduced by the elapsed frames; once it does not fit the cycle the
//   spawn is marked idle.
// ============================================================================
bool CrateClass::Remove_It()
{
    if (CellCoords == Crate_Default_CellCoords) {
        return false;
    }

    Get_Crate(CellCoords);
    CellCoords = Crate_Default_CellCoords;

    if (SpawnFrame == -1) {
        return true;
    }

    const int32 elapsed = CurrentFrame - SpawnFrame;
    if (elapsed < SpawnTime) {
        SpawnTimeLeft = SpawnTime - elapsed;
        SpawnFrame = -1;
        return true;
    }

    SpawnTimeLeft = 0;
    SpawnFrame = -1;
    return true;
}

// ============================================================================
 // CrateClass::Create_Crate -
//
//   First consume any still-valid pending spawn (folding the elapsed frames
//   into SpawnTimeLeft), then place the crate at coords.  A successful
//   placement schedules the next spawn: the delay is
//   CrateRegen * lerp(1800, 450, random(0, 0x7FFFFFFE) / 0x7FFFFFFF), floored.
// ============================================================================
bool CrateClass::Create_Crate(const CellStruct& coords)
{
    // Consume a pending spawn whose cycle has not yet finished.
    if (CellCoords != Crate_Default_CellCoords) {
        Get_Crate(CellCoords);
        CellCoords = Crate_Default_CellCoords;
    }

    if (SpawnFrame != -1) {
        const int32 elapsed = CurrentFrame - SpawnFrame;
        if (elapsed < SpawnTime) {
            SpawnTimeLeft = SpawnTime - elapsed;
        } else {
            SpawnTimeLeft = 0;
        }
        SpawnFrame = -1;
    }

    const CellStruct target = coords;

    if (!Put_Crate(target)) {
        return false;
    }

    // Register the cell as newly visible after the placement.
    if (TheTactical != nullptr && MapClass::Instance != nullptr) {
        CellClass* pCell = MapClass::Instance->GetCellAt(target);
        if (pCell != nullptr) {
            TheTactical->RegisterCellAsVisible(pCell);
        }
    }

    CellCoords = target;

    // Schedule the next spawn.  The original draws a 31-bit non-critical random
    // number and maps it onto [450, 1800] scaled by RulesClass.CrateRegen.
    const double regenScale = (TheRules != nullptr) ? TheRules->CrateRegen : 1.0;
    const double high = CRATE_REGEN_HIGH * regenScale;
    const double low  = CRATE_REGEN_LOW * regenScale;
    const double span = high - low;

    const int32 draw = static_cast<int32>(std::rand() & 0x7FFFFFFF);
    const double ratio = static_cast<double>(draw) / CRATE_RAND_MAX;

    SpawnFrame = CurrentFrame;
    SpawnTime = static_cast<int32>(std::floor(low + span * ratio));
    SpawnTimeLeft = SpawnTime;

    if (SpawnFrame == -1) {
        SpawnFrame = CurrentFrame;
    }

    return true;
}

// ============================================================================
 // CrateClass::Put_Crate -
//
//   Reject the placement when the cell is outside the radar (a gameplay-time
//   check, so ScenarioInit is cleared for the duration) or already carries an
//   overlay.  Otherwise build the appropriate overlay (water crate when the
//   land type is Water) and mark the cell area dirty.
// ============================================================================
bool CrateClass::Put_Crate(const CellStruct& coords)
{
    const int32 savedScenarioInit = ScenarioInit;
    ScenarioInit = 0;

    if (MapClass::Instance == nullptr) {
        ScenarioInit = savedScenarioInit;
        return false;
    }

    CellClass* pCell = MapClass::Instance->GetCellAt(coords);
    if (pCell == nullptr) {
        ScenarioInit = savedScenarioInit;
        return false;
    }

    if (pCell->Get_Overlay() != -1) {
        ScenarioInit = savedScenarioInit;
        return false;
    }

    // Choose the crate overlay by the cell's land type.
    OverlayTypeClass* pCrateType = nullptr;
    if (TheRules != nullptr) {
        pCrateType = pCell->IsWater() ? TheRules->WaterCrateImg : TheRules->CrateImg;
    }

    if (pCrateType != nullptr) {
        OverlayClass* pOverlay = new OverlayClass();
        if (pOverlay != nullptr) {
            pOverlay->Type = pCrateType;
            pOverlay->Set_Overlay_Data(-1);
            pOverlay->SetLocation(pCell->Get_Cell_Position());
            pCell->Set_Overlay(pCrateType->GetArrayIndex(), -1);
        }
    }

    // Flag the cell's area as dirty so the new crate is drawn.
    if (TheTactical != nullptr) {
        const CoordStruct crd = pCell->Get_Cell_Position();
        Rectangle dirty(crd.X, crd.Y, CellClass::CellWidth, CellClass::CellHeight);
        TheTactical->RegisterDirtyArea(dirty, false);
    }

    ScenarioInit = savedScenarioInit;
    return true;
}

// ============================================================================
 // CrateClass::Get_Crate -
//
//   Validate that the cell's overlay is one of the three crate images, then
//   remove it: mark the cell area dirty and clear the cell's overlay index and
//   crate flag.
// ============================================================================
bool CrateClass::Get_Crate(const CellStruct& coords)
{
    if (MapClass::Instance == nullptr) {
        return false;
    }

    CellClass* pCell = MapClass::Instance->GetCellAt(coords);
    if (pCell == nullptr) {
        return false;
    }

    const int32 overlayIndex = pCell->Get_Overlay();
    if (overlayIndex == -1) {
        return false;
    }

    OverlayTypeClass* pOverlayType = OverlayTypeClass::FindByIndex(overlayIndex);
    if (pOverlayType == nullptr) {
        return false;
    }

    // Only the wood / generic / water crate images may be harvested.
    const bool isCrate = (TheRules != nullptr) &&
                         (pOverlayType == TheRules->WoodCrateImg ||
                          pOverlayType == TheRules->CrateImg ||
                          pOverlayType == TheRules->WaterCrateImg);
    if (!isCrate) {
        return false;
    }

    if (TheTactical != nullptr) {
        const CoordStruct crd = pCell->Get_Cell_Position();
        Rectangle dirty(crd.X, crd.Y, CellClass::CellWidth, CellClass::CellHeight);
        TheTactical->RegisterDirtyArea(dirty, false);
    }

    pCell->Set_Overlay(-1, 0);
    pCell->CrateType = 0;

    return true;
}
