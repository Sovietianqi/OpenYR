#pragma once

#include "../Core/Definitions.h"

// ============================================================================
// CrateClass
//
//   The crate spawner attached to MapClass.  At most one crate is active at a
//   time; the record tracks the scheduled spawn and the cell the crate lives
//   in.
//
//   Layout (matches the original):
//     +0x00  int32      SpawnFrame    frame the next spawn is due (-1 = idle)
//     +0x04  int32      SpawnTime     total delay for the current cycle
//     +0x08  int32      SpawnTimeLeft remaining delay after a mid-cycle split
//     +0x0C  CellStruct CellCoords    cell the crate occupies
// ============================================================================
class CrateClass
{
public:
    // The "no crate here" cell the original compares against, and the sentinel
    // it copies when a crate is removed.
    static const CellStruct Crate_Default_CellCoords;

    CrateClass();
    CrateClass(noinit_t) noexcept {}
    ~CrateClass() {}

    // CrateClass::Remove_It (asm 0x4A174D): if a crate is present, tear down
    // its overlay, reset the cell and (when the spawn is due) mark it done.
    // Returns false when there was nothing to remove.
    bool Remove_It();

    // CrateClass::Create_Crate (asm 0x4A17DD): consume any pending spawn,
    // place a crate at the requested cell and schedule the next one using
    // RulesClass.CrateRegen * {1800..450}.
    bool Create_Crate(const CellStruct& coords);

    // CrateClass::Put_Crate (asm 0x4A191F): create the overlay on the target
    // cell when the terrain allows it, then flag the area dirty.
    bool Put_Crate(const CellStruct& coords);

    // CrateClass::Get_Crate (asm 0x4A1A9D): validate the overlay on the cell
    // as a crate image, remove it and flag the area dirty.
    bool Get_Crate(const CellStruct& coords);

    // ========================================================================
    // State
    // ========================================================================
    int32       SpawnFrame;      // +0x00
    int32       SpawnTime;       // +0x04
    int32       SpawnTimeLeft;   // +0x08
    CellStruct  CellCoords;      // +0x0C
};
