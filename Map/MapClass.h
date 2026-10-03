#pragma once

#include <Core/Definitions.h>
#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Abstract/AbstractClass.h>
#include <Math/CoordStruct.h>
#include <Map/CellClass.h>
#include <Map/CrateClass.h>

class CRCEngine;
class IStream;
class TechnoTypeClass;

// ============================================================================
// MapClass - The game map manager, singleton
// ============================================================================
class MapClass : public AbstractClass {
public:
    static MapClass* Instance;

    MapClass();
    virtual ~MapClass() noexcept {}

    // AbstractClass overrides
    virtual AbstractType WhatAmI() const { return AbstractType::Map; }
    virtual int32 Size() const { return sizeof(MapClass); }
    virtual int32 GetArrayIndex() const { return 0; }
    virtual bool IsDead() const { return false; }
    virtual HRESULT GetClassID(CLSID* pClassID) override { return 0; }

    // Serialization
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL bSave) override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // Init
    void Init(int32 maxX, int32 maxY);
    void Init_Clear();
    void Init_Theater(TheaterType theater);
    void Init_Cells();
    void Init_Waypoints();
    void Init_Shroud();

    // MapClass::Shroud_The_Map (asm 0x56C3E0).  Pushes every cell back into
    // the shrouded state for the given house, then re-reveals whatever that
    // house is legitimately allowed to see.
    void Shroud_The_Map(HouseClass* pHouse);

    // MapClass_Clear_Smudges (asm 0x588AC0).  Wipes every smudge / crater
    // decal placed on the map and asks the display to repaint.
    void Clear_Smudges();

    // MapClass_FlashCameo (asm 0x4E3xx0).  Lights up the sidebar cameo for
    // `pType` so the player notices a newly available build option.
    void Flash_Cameo(TechnoTypeClass* pType);

    // MapClass_Init_CellSpread (asm 0x561910).  Builds the flat (dx, dy) offset
    // table that every area-of-effect action walks: `CellSpreads` holds the
    // number of live entries and CellSpreadTable[2*i] / [2*i+1] the X / Y cell
    // delta of entry i, ordered so that expanding the walk grows a diamond.
    void Init_CellSpread();

    // MapClass_SetTab (asm 0x4E4xx0).  Switches the sidebar's active tab.
    void Set_Tab(int32 tabIndex);

    // MapClass_Reveal_The_Map (asm 0x56Cxxx): the inverse of Shroud_The_Map -
    // marks every cell as revealed for the given house (null = all houses).
    void Reveal_The_Map(HouseClass* pHouse);
    bool Allocate_Cells(int32 maxX, int32 maxY);
    void Free_Cells();

    // Cell access
    CellClass* GetCellAt(const CoordStruct& coord);
    CellClass* GetCellAt(const CellStruct& cell);
    CellClass* GetCellAt(int32 x, int32 y);
    CellClass* GetCellAt(int32 cellIndex);
    CellClass* TryGetCellAt(int32 x, int32 y);
    bool IsValidCell(int32 x, int32 y) const;
    bool IsValidCell(int32 cellIndex) const;

    // Coordinate conversion
    int32 CoordToCell(const CoordStruct& coord) const;
    CoordStruct CellToCoord(int32 cellIndex) const;
    int32 GetCellX(int32 cellIndex) const;
    int32 GetCellY(int32 cellIndex) const;
    int32 XYToCell(int32 x, int32 y) const;
    CellStruct CellToCellStruct(int32 cellIndex) const;

    // Bounds
    bool IsWithinUsableArea(int32 x, int32 y) const;
    bool IsWithinUsableArea(int32 cellIndex) const;
    bool IsWithinUsableArea(const CoordStruct& coord) const;

    // Waypoints
    CoordStruct GetWaypoint(int32 idx) const;
    void SetWaypoint(int32 idx, const CoordStruct& coord);
    int32 ClosestWaypoint(const CoordStruct& coord) const;

    // Utility
    int32 GetRandomValidCell() const;
    CoordStruct Center_Coord() const;
    bool Is_Placement_Allowed(const CoordStruct& coord) const;
    bool Is_Placement_Allowed(const CellStruct& cell) const;

    // Cell terrain
    LandType GetLandType(const CellStruct& cell) const;
    int32 GetCellSlope(const CellStruct& cell) const;
    int32 GetGroundHeight(const CoordStruct& coord) const;
    void MarkCellOccupied(const CellStruct& cell, bool occupied);
    bool IsCellOccupied(const CellStruct& cell) const;
    ObjectClass* GetCellOccupier(const CellStruct& cell);

    // Bridge
    bool IsBridgeCell(const CellStruct& cell) const;
    bool IsBridgeDestroyed(const CellStruct& cell) const;

    // Damage
    void ApplyDamageArea(const DamageArea& area);
    void CreateCrater(const CellStruct& cell, int32 size);

    // Base zone
    bool Base_Is_Area_Occupied(int32 cellIndex, int32 radius) const;

    // DisplayClass::Read_INI / MapClass_SaveMapToINI cell-tag stage.
    // "CellTags" maps a packed cell index to the name of the tag that owns
    // it.  NewINIFormat >= 4 splits the index as (index / 1000, index % 1000);
    // older maps split it as (index / 128, index % 128).
    void ReadCellTags(CCINIClass* pINI, const char* pSection, int32 newINIFormat);

    // Wall
    void Place_Wall(int32 x, int32 y, int32 overlayIndex);
    void Remove_Wall(int32 x, int32 y);

    // Tiberium
    void Update_Tiberium_Spread();

    // Logic - MapClass::Logic (asm 0x56BBF4).  The map's per-frame tick; the
    // only work it does is expiry-driven crate respawn, gated on a live
    // session and on crates being enabled.
    void Logic();

    // Crate
    void Update_Crate_Respawn();

    // Remove_Crate - MapClass::Remove_Crate (asm 0x56BFB4).  Finds the crate
    // occupying a cell and harvests it.  With a live session the crate table
    // is searched for a matching slot; without one the cell's overlay is
    // checked directly.  The cell must carry a crate overlay, and the overlay
    // must be marked as one the map owns (its Crushable-equivalent "crate"
    // byte).
    bool Remove_Crate(const CellStruct& coords);

    // Place_Random_Crate - MapClass::Place_Random_Crate (asm 0x56BD2C): pick a
    // random valid cell within the map's crate radius and spawn a crate there.
    bool Place_Random_Crate();

    // Nearby_Location - MapClass::Nearby_Location (asm 0x56DD8A): the general
    // "find a free cell around this position" search used by crate spawning,
    // unit placement and start-location scans.
    //
    //   position      - the centre of the search.
    //   SpeedType     - the mover's speed class (SpeedType::Foot ..).
    //   a5            - the expected zone index, or -1 to ignore.
    //   MovementZone  - the mover's movement zone (default Normal).
    //   InAir         - the mover is airborne, so only the X/Y ring is checked.
    //   a8, a9        - extra flags forwarded to the cell test.
    //   a10           - forwarded to the cell test.
    //   a11 / a12 / a13 - cell-test options (see CellClass::Is_Clear_To_Move).
    //   a14           - optional output list (up to 24 candidates).
    //   a15           - restrict the search to a single axis.
    //   a16           - require the candidate to be on screen.
    //
    // Points are returned in cell coordinates.
    CellStruct Nearby_Location(const CellStruct& position, int32 SpeedType,
                               int32 a5, MovementZone zone, bool InAir,
                               int32 a8, int32 a9, int32 a10,
                               bool a11, bool a12, bool a13, bool a15, bool a16);

    // Convenience overload mirroring the binary's most common call shape.
    CellStruct Nearby_Location(const CellStruct& position, int32 SpeedType,
                               MovementZone zone);

    // Pick_Random_Location - MapClass::Pick_Random_Location (asm 0x577A4A):
    // draw a cell uniformly from the published local rect.
    CellStruct Pick_Random_Location();

    // CellInVisibleArea - MapClass_CellInVisibleArea (asm 0x56832A): isometric
    // visible-region test.  A cell is on screen when it falls inside the
    // diamond formed by the viewport's horizontal and vertical extents:
    //
    //     x + y  <= Right        and   |x - y| < Right
    //     x + y  <= Right + 2*Bottom
    bool IsCellInVisibleArea(int32 cellX, int32 cellY) const;

    // Cell_Region - MapClass::Cell_Region (asm 0x56BC40).  Maps a cell to the
    // coarse 4x4 "region" identifier used by the threat grid.  Cells are
    // grouped in blocks of four; the identifier is
    //
    //     region = (y / 4) * 66 + (x / 4) * 2 + 0x83
    //
    // where the flooring divide is the arithmetic shift the binary uses.
    static int32 Cell_Region(const CellStruct& cell);

    // In_Radar - MapClass::In_Radar (asm 0x568326 / 0x56BC40 family).  Tests
    // whether a cell falls inside the radar diamond.  With the local extents
    // R = +0xF4 and B = +0xF8 and cell (x, y):
    //
    //     x + y <= R   and   x - y < R   and   y - x < R
    //     x + y <= R + 2*B
    //
    // `skipRange` is accepted and ignored by this variant (the binary's
    // parameter is unused), matching the original signature.
    bool In_Radar(const CellStruct& cell, bool skipRange) const;

    // Cell_Threat - MapClass::Cell_Threat (asm 0x56BCC0).  Reads the threat
    // value a given house holds for a cell, out of that house's threat grid.
    // The grid is indexed by the same region math as Cell_Region.
    int32 Cell_Threat(const CellStruct& cell, HouseClass* who) const;

    // ========================================================================
    // Visibility / radar / planning probes
    // ========================================================================

    // MapClass_CellExists (asm 0x4AC6xx): true when the cell slot the packed
    // coordinate addresses holds a live CellClass pointer.
    bool CellExists(const CellStruct& cell) const;
    // MapClass_CellInVisibleArea (asm 0x577E5x): true when the world position
    // lies inside the current visible rectangle.
    bool CellInVisibleArea(const CoordStruct& xyz) const;
    // MapClass_IsCellUsable (asm 0x5785xx): the radar-visibility + passability
    // test the cursor code performs before it accepts a click.  `skipRange`
    // short-circuits the range part of MapClass::In_Radar.
    bool IsCellUsable(const CoordStruct& where) const;
    // MapClass_IsCellShrouded (asm 0x5863xx): the base map never reports a
    // shrouded cell; scenario/multiplayer overrides refine this.
    bool IsCellShrouded(const CoordStruct& loc) const;
    // MapClass_IsCellTainted (asm 0x5785xx): true when the cell the coordinate
    // falls in carries the "tainted" (revealed-by-something) marker.
    bool IsCellTainted(const CoordStruct& loc, bool a3) const;
    // MapClass_GetArea (asm 0x5D26C0): (MapWidth + 4) * MapHeight * 2 - the
    // size of the per-cell threat grids, in int32 slots.
    int32 GetArea() const;

    // MapClass_IsRadarAvailable (asm 0x656Bxx): the cached radar-ready byte at
    // +0x14D8.
    bool IsRadarAvailable() const;

    // MapClass_IsPlanningModeActive / NoCanDoInPlanningMode / the cursor
    // predicate (asm 0x637DB6 / 0x63A11E / 0x637DB0): the planning-mode state
    // byte the waypoint planner toggles.
    bool IsPlanningModeActive() const;
    bool Cursor_IsNotPlanningDeploy(int32 cursorType) const;
    void NoCanDoInPlanningMode();

    // ========================================================================
    // Cell iterator
    //
    //  MapClass_CellIterator_Reset (asm 0x578260) primes the iterator over the
    //  whole map; MapClass_CellIterator_NextCell (asm 0x578290) advances to the
    //  next cell and returns it, or null once the walk is exhausted.  The
    //  iterator is a raw pointer walk, not an index walk, so the returned
    //  CellClass* is the live object.
    // ========================================================================
    void       CellIterator_Reset();
    CellClass* CellIterator_NextCell();

    // ========================================================================
    // Follow-camera state (asm 0x4AEB1C / 0x4AEB28)
    // ========================================================================
    ObjectClass* FollowingWhat() const;
    bool FollowThis(ObjectClass* what);

    // ========================================================================
    // Mission timer (asm 0x5E9xxx)
    // ========================================================================
    // MapClass_TimerPinged: marks the mission timer as "pinged" so the UI
    // stops flashing it, without touching the countdown itself.
    void TimerPinged();
    // MapClass_StopTimerWQ: records the "stopped" flag, clears the pinged
    // state and snapshots the current frame into the timer's stop slot.
    void StopTimerWQ();

    // Members
    int32       MapWidth;
    int32       MapHeight;
    int32       MapSize;
    int32       CellCount;

    // ── CellSpread table (asm CellSpreads / CellSpread_Table) ─────────────
    // CellSpreads is the number of live cells in the spread; the table is a
    // flat run of int16 (dx, dy) pairs, 369 entries in the original.
    int32       CellSpreads;
    int16       CellSpreadTable[369 * 2];
    CellClass*  CellArray;

    // ── Cell iterator state (asm +0xF4, +0x10C, +0x110, +0x114, +0x118) ──
    // CellIterator_Reset primes these and CellIterator_NextCell walks them:
    // CellIterWidth (+0xF4) is the row stride, CellIterIndex (+0x10C) the
    // 1-based walk counter, CellIterRemaining (+0x114) counts cells left to
    // visit and CellIterPtr (+0x118) is the current cell pointer.
    int32       CellIterWidth;
    int32       CellIterIndex;
    int32       CellIterRemaining;
    CellClass*  CellIterPtr;
    int32       CellIterBase;
    int32       MaxWaypoints;
    CoordStruct Waypoints[702];
    int32       CrateCount;
    CrateClass  Crate;
    int32       TotalValue;
    int32       VisibleRectX, VisibleRectY, VisibleRectWidth, VisibleRectHeight;
    TheaterType CurrentTheater;

    // The "follow camera" target and its active flag (asm +0x.../anonymous_56
    // and +FollowSomething).
    ObjectClass* FollowSomething;
    bool         FollowingFlag;

    // Cached radar readiness (asm +0x14D8).
    bool         RadarReady;

    // Mission-timer state: the "pinged" latch and the frame the timer was
    // stopped at (asm +MissionTimerIsSomething and +TimerWQ).
    bool         MissionTimerPinged;
    int32        MissionTimerStopFrame;

    // Planning (waypoint) mode latch shared with the cursor code, plus the
    // one-shot warning latch used by NoCanDoInPlanningMode (asm +0xAC4C08).
    bool         PlanningModeActive;
    bool         PlanningNoCanDoLatched;

    uint8*      Tilesets;
    int32       TilesetCount;
    int32       unknown_0x1EF8;
    int32       unknown_0x1EFC;
    int32       unknown_0x1F00;
    int32       unknown_0x1F04;
    int32       unknown_0x1F08;
    int32       unknown_0x1F0C;
    int32       unknown_0x1F10;
    int32       unknown_0x1F14;
    int32       unknown_0x1F18;
    int32       unknown_0x1F1C;
    int32       unknown_0x1F20;
    int32       unknown_0x1F24;
    int32       unknown_0x1F28;
    int32       unknown_0x1F2C;
    int32       unknown_0x1F30;
    int32       unknown_0x1F34;
    int32       unknown_0x1F38;
    int32       unknown_0x1F3C;
    int32       unknown_0x1F40;
    int32       unknown_0x1F44;
    int32       unknown_0x1F48;
    int32       unknown_0x1F4C;
    int32       unknown_0x1F50;
    int32       unknown_0x1F54;
    int32       unknown_0x1F58;
    int32       unknown_0x1F5C;
    int32       unknown_0x1F60;
    int32       unknown_0x1F64;
    int32       unknown_0x1F68;
    int32       unknown_0x1F6C;
    int32       unknown_0x1F70;
    int32       unknown_0x1F74;
    int32       unknown_0x1F78;
    int32       unknown_0x1F7C;
    int32       unknown_0x1F80;
    int32       unknown_0x1F84;
    int32       unknown_0x1F88;
    int32       unknown_0x1F8C;
    int32       unknown_0x1F90;
    int32       unknown_0x1F94;
    int32       unknown_0x1F98;
    int32       unknown_0x1F9C;
    int32       unknown_0x1FA0;
    int32       unknown_0x1FA4;
    int32       unknown_0x1FA8;
    int32       unknown_0x1FAC;
    int32       unknown_0x1FB0;
    int32       unknown_0x1FB4;
    int32       unknown_0x1FB8;
    int32       unknown_0x1FBC;
    int32       unknown_0x1FC0;
    int32       unknown_0x1FC4;
    int32       unknown_0x1FC8;
    int32       unknown_0x1FCC;
    int32       unknown_0x1FD0;
    int32       unknown_0x1FD4;
    int32       unknown_0x1FD8;
    int32       unknown_0x1FDC;
    int32       unknown_0x1FE0;
    int32       unknown_0x1FE4;
    int32       unknown_0x1FE8;
    int32       unknown_0x1FEC;
    int32       unknown_0x1FF0;
    int32       unknown_0x1FF4;
    int32       unknown_0x1FF8;
    int32       unknown_0x1FFC;
    int32       unknown_0x2000;
    int32       unknown_0x2004;
    int32       unknown_0x2008;
    int32       unknown_0x200C;
    int32       unknown_0x2010;
    int32       unknown_0x2014;
    int32       unknown_0x2018;
    int32       unknown_0x201C;
    int32       unknown_0x2020;
    int32       unknown_0x2024;
    int32       unknown_0x2028;
    int32       unknown_0x202C;
    int32       unknown_0x2030;
    int32       unknown_0x2034;
    int32       unknown_0x2038;
    int32       unknown_0x203C;
    int32       unknown_0x2040;
    int32       unknown_0x2044;
    int32       unknown_0x2048;
    int32       unknown_0x204C;
    int32       unknown_0x2050;
    int32       unknown_0x2054;
    int32       unknown_0x2058;
    int32       unknown_0x205C;
    int32       unknown_0x2060;
    int32       unknown_0x2064;
    int32       unknown_0x2068;
    int32       unknown_0x206C;
    int32       unknown_0x2070;
    int32       unknown_0x2074;
    int32       unknown_0x2078;
    int32       unknown_0x207C;
    int32       unknown_0x2080;
    int32       unknown_0x2084;
    int32       unknown_0x2088;
    int32       unknown_0x208C;
    int32       unknown_0x2090;
    int32       unknown_0x2094;
    int32       unknown_0x2098;
    int32       unknown_0x209C;
    int32       unknown_0x20A0;
    int32       unknown_0x20A4;
    int32       unknown_0x20A8;
    int32       unknown_0x20AC;
    int32       unknown_0x20B0;
    int32       unknown_0x20B4;
    int32       unknown_0x20B8;
    int32       unknown_0x20BC;
    int32       unknown_0x20C0;
    int32       unknown_0x20C4;
    int32       unknown_0x20C8;
    int32       unknown_0x20CC;
    int32       unknown_0x20D0;
    int32       unknown_0x20D4;
    int32       unknown_0x20D8;
    int32       unknown_0x20DC;
    int32       unknown_0x20E0;
    int32       unknown_0x20E4;
    int32       unknown_0x20E8;
    int32       unknown_0x20EC;
    int32       unknown_0x20F0;
    int32       unknown_0x20F4;
    int32       unknown_0x20F8;
    int32       unknown_0x20FC;
    int32       unknown_0x2100;
    int32       unknown_0x2104;
    int32       unknown_0x2108;
    int32       unknown_0x210C;
    int32       unknown_0x2110;
    int32       unknown_0x2114;
    int32       unknown_0x2118;
    int32       unknown_0x211C;
    int32       unknown_0x2120;
    int32       unknown_0x2124;
    int32       unknown_0x2128;
    int32       unknown_0x212C;
    int32       unknown_0x2130;
    int32       unknown_0x2134;
    int32       unknown_0x2138;
    int32       unknown_0x213C;
    int32       unknown_0x2140;
    int32       unknown_0x2144;
    int32       unknown_0x2148;
    int32       unknown_0x214C;
    int32       unknown_0x2150;
    int32       unknown_0x2154;
    int32       unknown_0x2158;
    int32       unknown_0x215C;
    int32       unknown_0x2160;
    int32       unknown_0x2164;
    int32       unknown_0x2168;
    int32       unknown_0x216C;
    int32       unknown_0x2170;
    int32       unknown_0x2174;
    int32       unknown_0x2178;
    int32       unknown_0x217C;
    int32       unknown_0x2180;
    int32       unknown_0x2184;
    int32       unknown_0x2188;
    int32       unknown_0x218C;
    int32       unknown_0x2190;
    int32       unknown_0x2194;
    int32       unknown_0x2198;
    int32       unknown_0x219C;

    // Padding to match original binary layout
    // The original MapClass has a large gap of unknown members
    uint8       _unused_padding[0x456C - 0x21A0];
};
// The single global map instance, mirroring `Map` in the original binary.
extern MapClass* TheMap;
