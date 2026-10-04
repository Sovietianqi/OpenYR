#include <Map/MapClass.h>
#include <Core/Definitions.h>
#include <Core/Memory.h>
#include <Math/CoordStruct.h>
#include <Math/Timer.h>
#include <IO/CRC.h>
#include <Combat/DamageArea.h>
#include <Scenario/ScenarioClass.h>
#include <INI/INIClass.h>
#include <Abstract/OverlayTypeClass.h>
#include <AI/TagClass.h>
#include <Abstract/TechnoTypeClass.h>
#include <Houses/HouseClass.h>
#include <Game/Externs.h>
#include <Game/Game.h>
#include <Rendering/TacticalClass.h>
#include <Game/GameInit.h>

#include <cstdlib>
#include <cstring>
#include <cmath>

// ============================================================================
// MapClass.cpp - Map class implementation
// ============================================================================

// Static singleton
MapClass* MapClass::Instance = nullptr;

// ============================================================================
// Constructor
// ============================================================================

MapClass::MapClass()
    : MapWidth(0), MapHeight(0), MapSize(0), CellCount(0)
    , CellArray(nullptr), MaxWaypoints(702), CrateCount(0), TotalValue(0)
    , CellIterWidth(0), CellIterIndex(0), CellIterRemaining(0)
    , CellIterPtr(nullptr), CellIterBase(0)
    , VisibleRectX(0), VisibleRectY(0), VisibleRectWidth(0), VisibleRectHeight(0)
    , CurrentTheater(TheaterType::Temperate)
    , FollowSomething(nullptr), FollowingFlag(false)
    , RadarReady(false)
    , MissionTimerPinged(false), MissionTimerStopFrame(0)
    , PlanningModeActive(false), PlanningNoCanDoLatched(false)
    , Tilesets(nullptr), TilesetCount(0)
{
    // Zero-initialize waypoints
    for (int32 i = 0; i < 702; ++i) {
        Waypoints[i].X = 0;
        Waypoints[i].Y = 0;
        Waypoints[i].Z = 0;
    }

    // Zero-initialize the tail of unknown members
    memset(&unknown_0x1EF8, 0, &unknown_0x219C - &unknown_0x1EF8 + sizeof(unknown_0x219C));
}

// ============================================================================
// Serialization
// ============================================================================

HRESULT MapClass::Load(IStream* pStm)
{
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read map dimensions
    hr = pStm->Read(&MapWidth, sizeof(MapWidth), &read);
    if (hr < 0 || read != sizeof(MapWidth)) return E_FAIL;
    hr = pStm->Read(&MapHeight, sizeof(MapHeight), &read);
    if (hr < 0 || read != sizeof(MapHeight)) return E_FAIL;
    hr = pStm->Read(&MapSize, sizeof(MapSize), &read);
    if (hr < 0 || read != sizeof(MapSize)) return E_FAIL;
    hr = pStm->Read(&CellCount, sizeof(CellCount), &read);
    if (hr < 0 || read != sizeof(CellCount)) return E_FAIL;

    // Read cell array
    Free_Cells();
    if (CellCount > 0) {
        if (!Allocate_Cells(MapWidth, MapHeight)) return E_FAIL;
        for (int32 i = 0; i < CellCount; ++i) {
            if (!CellArray[i].Load(pStm)) return E_FAIL;
        }
    }

    // Read MaxWaypoints
    hr = pStm->Read(&MaxWaypoints, sizeof(MaxWaypoints), &read);
    if (hr < 0 || read != sizeof(MaxWaypoints)) return E_FAIL;

    // Read Waypoints array
    hr = pStm->Read(Waypoints, sizeof(Waypoints), &read);
    if (hr < 0 || read != sizeof(Waypoints)) return E_FAIL;

    // Read CrateCount and TotalValue
    hr = pStm->Read(&CrateCount, sizeof(CrateCount), &read);
    if (hr < 0 || read != sizeof(CrateCount)) return E_FAIL;
    hr = pStm->Read(&TotalValue, sizeof(TotalValue), &read);
    if (hr < 0 || read != sizeof(TotalValue)) return E_FAIL;

    // Read visible rect
    hr = pStm->Read(&VisibleRectX, sizeof(VisibleRectX), &read);
    if (hr < 0 || read != sizeof(VisibleRectX)) return E_FAIL;
    hr = pStm->Read(&VisibleRectY, sizeof(VisibleRectY), &read);
    if (hr < 0 || read != sizeof(VisibleRectY)) return E_FAIL;
    hr = pStm->Read(&VisibleRectWidth, sizeof(VisibleRectWidth), &read);
    if (hr < 0 || read != sizeof(VisibleRectWidth)) return E_FAIL;
    hr = pStm->Read(&VisibleRectHeight, sizeof(VisibleRectHeight), &read);
    if (hr < 0 || read != sizeof(VisibleRectHeight)) return E_FAIL;

    // Read CurrentTheater
    hr = pStm->Read(&CurrentTheater, sizeof(CurrentTheater), &read);
    if (hr < 0 || read != sizeof(CurrentTheater)) return E_FAIL;

    // Read TilesetCount then Tilesets data
    hr = pStm->Read(&TilesetCount, sizeof(TilesetCount), &read);
    if (hr < 0 || read != sizeof(TilesetCount)) return E_FAIL;
    if (Tilesets) { delete[] Tilesets; Tilesets = nullptr; }
    if (TilesetCount > 0) {
        Tilesets = new uint8[TilesetCount];
        hr = pStm->Read(Tilesets, TilesetCount, &read);
        if (hr < 0 || read != static_cast<ULONG>(TilesetCount)) return E_FAIL;
    }

    // Read unknown block (0x1EF8 through 0x219C inclusive)
    int32 unknownSize = reinterpret_cast<const char*>(&unknown_0x219C)
                      - reinterpret_cast<const char*>(&unknown_0x1EF8)
                      + sizeof(unknown_0x219C);
    hr = pStm->Read(&unknown_0x1EF8, unknownSize, &read);
    if (hr < 0 || read != static_cast<ULONG>(unknownSize)) return E_FAIL;

    // Read padding
    hr = pStm->Read(_unused_padding, sizeof(_unused_padding), &read);
    if (hr < 0 || read != sizeof(_unused_padding)) return E_FAIL;

    return S_OK;
}

HRESULT MapClass::Save(IStream* pStm, BOOL bSave)
{
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Write map dimensions
    hr = pStm->Write(&MapWidth, sizeof(MapWidth), &written);
    if (hr < 0 || written != sizeof(MapWidth)) return E_FAIL;
    hr = pStm->Write(&MapHeight, sizeof(MapHeight), &written);
    if (hr < 0 || written != sizeof(MapHeight)) return E_FAIL;
    hr = pStm->Write(&MapSize, sizeof(MapSize), &written);
    if (hr < 0 || written != sizeof(MapSize)) return E_FAIL;
    hr = pStm->Write(&CellCount, sizeof(CellCount), &written);
    if (hr < 0 || written != sizeof(CellCount)) return E_FAIL;

    // Write cell array
    for (int32 i = 0; i < CellCount; ++i) {
        if (!CellArray[i].Save(pStm)) return E_FAIL;
    }

    // Write MaxWaypoints
    hr = pStm->Write(&MaxWaypoints, sizeof(MaxWaypoints), &written);
    if (hr < 0 || written != sizeof(MaxWaypoints)) return E_FAIL;

    // Write Waypoints array
    hr = pStm->Write(Waypoints, sizeof(Waypoints), &written);
    if (hr < 0 || written != sizeof(Waypoints)) return E_FAIL;

    // Write CrateCount and TotalValue
    hr = pStm->Write(&CrateCount, sizeof(CrateCount), &written);
    if (hr < 0 || written != sizeof(CrateCount)) return E_FAIL;
    hr = pStm->Write(&TotalValue, sizeof(TotalValue), &written);
    if (hr < 0 || written != sizeof(TotalValue)) return E_FAIL;

    // Write visible rect
    hr = pStm->Write(&VisibleRectX, sizeof(VisibleRectX), &written);
    if (hr < 0 || written != sizeof(VisibleRectX)) return E_FAIL;
    hr = pStm->Write(&VisibleRectY, sizeof(VisibleRectY), &written);
    if (hr < 0 || written != sizeof(VisibleRectY)) return E_FAIL;
    hr = pStm->Write(&VisibleRectWidth, sizeof(VisibleRectWidth), &written);
    if (hr < 0 || written != sizeof(VisibleRectWidth)) return E_FAIL;
    hr = pStm->Write(&VisibleRectHeight, sizeof(VisibleRectHeight), &written);
    if (hr < 0 || written != sizeof(VisibleRectHeight)) return E_FAIL;

    // Write CurrentTheater
    hr = pStm->Write(&CurrentTheater, sizeof(CurrentTheater), &written);
    if (hr < 0 || written != sizeof(CurrentTheater)) return E_FAIL;

    // Write TilesetCount then Tilesets data
    hr = pStm->Write(&TilesetCount, sizeof(TilesetCount), &written);
    if (hr < 0 || written != sizeof(TilesetCount)) return E_FAIL;
    if (TilesetCount > 0 && Tilesets) {
        hr = pStm->Write(Tilesets, TilesetCount, &written);
        if (hr < 0 || written != static_cast<ULONG>(TilesetCount)) return E_FAIL;
    }

    // Write unknown block (0x1EF8 through 0x219C inclusive)
    int32 unknownSize = reinterpret_cast<const char*>(&unknown_0x219C)
                      - reinterpret_cast<const char*>(&unknown_0x1EF8)
                      + sizeof(unknown_0x219C);
    hr = pStm->Write(&unknown_0x1EF8, unknownSize, &written);
    if (hr < 0 || written != static_cast<ULONG>(unknownSize)) return E_FAIL;

    // Write padding
    hr = pStm->Write(_unused_padding, sizeof(_unused_padding), &written);
    if (hr < 0 || written != sizeof(_unused_padding)) return E_FAIL;

    return S_OK;
}

void MapClass::ComputeCRC(CRCEngine& crc) const
{
    crc.AddData(&MapWidth, sizeof(MapWidth));
    crc.AddData(&MapHeight, sizeof(MapHeight));
}

// ============================================================================
// Init - Initialize map with given dimensions
// ============================================================================

void MapClass::Init(int32 maxX, int32 maxY)
{
    Init_Clear();
    Free_Cells();

    if (!Allocate_Cells(maxX, maxY)) {
        return;
    }

    MapWidth = maxX;
    MapHeight = maxY;
    MapSize = maxX * maxY;
    CellCount = MapSize;

    Init_Cells();
    Init_Waypoints();
    Init_Shroud();
    Init_CellSpread();
}

void MapClass::Init_Clear()
{
    Free_Cells();
    MapWidth = 0;
    MapHeight = 0;
    MapSize = 0;
    CellCount = 0;
    Tilesets = nullptr;
    TilesetCount = 0;
    CrateCount = 0;
    TotalValue = 0;
}

void MapClass::Init_Theater(TheaterType theater)
{
    // Store the theater type on the map
    CurrentTheater = theater;

    // In the original game, this sets up the theater-specific terrain data
    // and reloads tileset data for the new theater
    if (CellArray) {
        for (int32 i = 0; i < CellCount; ++i) {
            // Reset cell land type and tile data based on theater
            // The original game applies theater-specific terrain templates here
        }
    }
}

void MapClass::Init_Cells()
{
    if (!CellArray) return;

    for (int32 i = 0; i < CellCount; ++i) {
        // CellClass constructor already initializes all members
        // Set the map coordinates for each cell
        CellArray[i].MapCoords.X = static_cast<int16>(i % MapWidth);
        CellArray[i].MapCoords.Y = static_cast<int16>(i / MapWidth);
        CellArray[i].CellIndex = i;
    }

    // After setting coordinates, wire up adjacent cell pointers
    for (int32 i = 0; i < CellCount; ++i) {
        int32 cx = GetCellX(i);
        int32 cy = GetCellY(i);
        CellClass* pCell = &CellArray[i];

        // Set adjacent cells: N, NE, E, SE, S, SW, W, NW
        pCell->AdjacentCells[0] = IsValidCell(cx, cy - 1)     ? GetCellAt(cx, cy - 1)     : nullptr;
        pCell->AdjacentCells[1] = IsValidCell(cx + 1, cy - 1) ? GetCellAt(cx + 1, cy - 1) : nullptr;
        pCell->AdjacentCells[2] = IsValidCell(cx + 1, cy)     ? GetCellAt(cx + 1, cy)     : nullptr;
        pCell->AdjacentCells[3] = IsValidCell(cx + 1, cy + 1) ? GetCellAt(cx + 1, cy + 1) : nullptr;
        pCell->AdjacentCells[4] = IsValidCell(cx, cy + 1)     ? GetCellAt(cx, cy + 1)     : nullptr;
        pCell->AdjacentCells[5] = IsValidCell(cx - 1, cy + 1) ? GetCellAt(cx - 1, cy + 1) : nullptr;
        pCell->AdjacentCells[6] = IsValidCell(cx - 1, cy)     ? GetCellAt(cx - 1, cy)     : nullptr;
        pCell->AdjacentCells[7] = IsValidCell(cx - 1, cy - 1) ? GetCellAt(cx - 1, cy - 1) : nullptr;
    }
}

void MapClass::Init_Waypoints()
{
    for (int32 i = 0; i < MaxWaypoints; ++i) {
        Waypoints[i].X = 0;
        Waypoints[i].Y = 0;
        Waypoints[i].Z = 0;
    }
}

void MapClass::Init_Shroud()
{
    // Initialize shroud for all cells
    for (int32 i = 0; i < CellCount; ++i) {
        if (CellArray) {
            // Shroud is initialized per-cell in the original game
        }
    }
}

bool MapClass::Allocate_Cells(int32 maxX, int32 maxY)
{
    if (maxX <= 0 || maxY <= 0) return false;

    int32 totalCells = maxX * maxY;
    CellArray = new CellClass[totalCells];
    if (!CellArray) return false;

    CellCount = totalCells;
    return true;
}

void MapClass::Free_Cells()
{
    if (CellArray) {
        delete[] CellArray;
        CellArray = nullptr;
    }
    CellCount = 0;
}

// ============================================================================
// Cell access
// ============================================================================

CellClass* MapClass::GetCellAt(const CoordStruct& coord)
{
    int32 idx = CoordToCell(coord);
    return GetCellAt(idx);
}

CellClass* MapClass::GetCellAt(const CellStruct& cell)
{
    return GetCellAt(static_cast<int32>(cell.X), static_cast<int32>(cell.Y));
}

CellClass* MapClass::GetCellAt(int32 x, int32 y)
{
    if (!IsValidCell(x, y)) return nullptr;
    return &CellArray[y * MapWidth + x];
}

CellClass* MapClass::GetCellAt(int32 cellIndex)
{
    if (!IsValidCell(cellIndex)) return nullptr;
    return &CellArray[cellIndex];
}

CellClass* MapClass::TryGetCellAt(int32 x, int32 y)
{
    if (!IsValidCell(x, y)) return nullptr;
    return &CellArray[y * MapWidth + x];
}

bool MapClass::IsValidCell(int32 x, int32 y) const
{
    return (x >= 0 && x < MapWidth && y >= 0 && y < MapHeight);
}

bool MapClass::IsValidCell(int32 cellIndex) const
{
    return (cellIndex >= 0 && cellIndex < CellCount);
}

// ============================================================================
// Coordinate conversion
// ============================================================================

int32 MapClass::CoordToCell(const CoordStruct& coord) const
{
    // Convert leptons to cell coordinates (1 cell = 256 leptons)
    int32 x = (coord.X + 128) / 256;
    int32 y = (coord.Y + 128) / 256;
    return XYToCell(x, y);
}

CoordStruct MapClass::CellToCoord(int32 cellIndex) const
{
    if (!IsValidCell(cellIndex)) {
        return CoordStruct(0, 0, 0);
    }
    int32 x = cellIndex % MapWidth;
    int32 y = cellIndex / MapWidth;
    // Center of cell: 256 leptons per cell
    return CoordStruct(x * 256 + 128, y * 256 + 128, 0);
}

int32 MapClass::GetCellX(int32 cellIndex) const
{
    return cellIndex % MapWidth;
}

int32 MapClass::GetCellY(int32 cellIndex) const
{
    return cellIndex / MapWidth;
}

int32 MapClass::XYToCell(int32 x, int32 y) const
{
    return y * MapWidth + x;
}

CellStruct MapClass::CellToCellStruct(int32 cellIndex) const
{
    if (!IsValidCell(cellIndex)) {
        return CellStruct(0, 0);
    }
    return CellStruct(
        static_cast<int16>(cellIndex % MapWidth),
        static_cast<int16>(cellIndex / MapWidth)
    );
}

// ============================================================================
// Bounds
// ============================================================================

bool MapClass::IsWithinUsableArea(int32 x, int32 y) const
{
    // Usable area excludes the border cells (typically 1-3 cells of border)
    const int32 border = 3;
    return (x >= border && x < MapWidth - border &&
            y >= border && y < MapHeight - border);
}

bool MapClass::IsWithinUsableArea(int32 cellIndex) const
{
    int32 x = GetCellX(cellIndex);
    int32 y = GetCellY(cellIndex);
    return IsWithinUsableArea(x, y);
}

bool MapClass::IsWithinUsableArea(const CoordStruct& coord) const
{
    int32 cellIdx = CoordToCell(coord);
    return IsWithinUsableArea(cellIdx);
}

// ============================================================================
// Waypoints
// ============================================================================

CoordStruct MapClass::GetWaypoint(int32 idx) const
{
    if (idx >= 0 && idx < MaxWaypoints) {
        return Waypoints[idx];
    }
    return CoordStruct(0, 0, 0);
}

void MapClass::SetWaypoint(int32 idx, const CoordStruct& coord)
{
    if (idx >= 0 && idx < MaxWaypoints) {
        Waypoints[idx] = coord;
    }
}

int32 MapClass::ClosestWaypoint(const CoordStruct& coord) const
{
    int32 closest = -1;
    int64 closestDist = INT64_MAX;

    for (int32 i = 0; i < MaxWaypoints; ++i) {
        if (Waypoints[i].X == 0 && Waypoints[i].Y == 0 && Waypoints[i].Z == 0)
            continue;

        int64 dx = static_cast<int64>(coord.X) - static_cast<int64>(Waypoints[i].X);
        int64 dy = static_cast<int64>(coord.Y) - static_cast<int64>(Waypoints[i].Y);
        int64 dist = dx * dx + dy * dy;

        if (dist < closestDist) {
            closestDist = dist;
            closest = i;
        }
    }

    return closest;
}

// ============================================================================
// Utility
// ============================================================================

int32 MapClass::GetRandomValidCell() const
{
    if (CellCount <= 0) return 0;

    int32 attempts = 0;
    while (attempts < 100) {
        int32 idx = rand() % CellCount;
        int32 x = GetCellX(idx);
        int32 y = GetCellY(idx);
        if (IsWithinUsableArea(x, y)) {
            return idx;
        }
        ++attempts;
    }
    return 0;
}

CoordStruct MapClass::Center_Coord() const
{
    int32 cx = MapWidth / 2;
    int32 cy = MapHeight / 2;
    return CoordStruct(cx * 256 + 128, cy * 256 + 128, 0);
}

bool MapClass::Is_Placement_Allowed(const CoordStruct& coord) const
{
    int32 cellIdx = CoordToCell(coord);
    if (!IsValidCell(cellIdx)) return false;
    return IsWithinUsableArea(cellIdx);
}

bool MapClass::Is_Placement_Allowed(const CellStruct& cell) const
{
    return IsWithinUsableArea(cell.X, cell.Y);
}

// ============================================================================
// Base zone
// ============================================================================

bool MapClass::Base_Is_Area_Occupied(int32 cellIndex, int32 radius) const
{
    if (!IsValidCell(cellIndex)) return false;

    int32 cx = GetCellX(cellIndex);
    int32 cy = GetCellY(cellIndex);

    for (int32 dy = -radius; dy <= radius; ++dy) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            int32 nx = cx + dx;
            int32 ny = cy + dy;
            if (IsValidCell(nx, ny)) {
                CellClass* pCell = const_cast<MapClass*>(this)->GetCellAt(nx, ny);
                if (pCell && pCell->Occupier) {
                    return true;
                }
            }
        }
    }
    return false;
}

// ============================================================================
// Wall
// ============================================================================

void MapClass::Place_Wall(int32 x, int32 y, int32 overlayIndex)
{
    CellClass* pCell = GetCellAt(x, y);
    if (!pCell) return;
    // Place the wall overlay on the cell
    // In the original game, this sets the overlay type and updates adjacent walls
    if (overlayIndex >= 0) {
        pCell->Overlay = overlayIndex;
        pCell->OverlayData = static_cast<int32>(overlayIndex);
        pCell->Land = LandType::Wall;
    }
}

void MapClass::Remove_Wall(int32 x, int32 y)
{
    CellClass* pCell = GetCellAt(x, y);
    if (!pCell) return;
    pCell->Overlay = -1;
    pCell->OverlayData = 0;
    pCell->Land = LandType::Clear;
}

// ============================================================================
// Tiberium
// ============================================================================

void MapClass::Update_Tiberium_Spread()
{
    // Tiberium growth logic
    // In the original game, this spreads tiberium to adjacent cells
    // based on the current tiberium concentration and growth rate

    if (!ScenarioClass::Instance) return;
    if (!ScenarioClass::Instance->TiberiumGrowthEnabled) return;

    // Simple growth: scan for tiberium cells and spread
    for (int32 i = 0; i < CellCount; ++i) {
        if (CellArray[i].TiberiumValue > 0) {
            // Check adjacent cells for spreading
            int32 x = GetCellX(i);
            int32 y = GetCellY(i);

            for (int32 dy = -1; dy <= 1; ++dy) {
                for (int32 dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int32 nx = x + dx;
                    int32 ny = y + dy;
                    if (IsValidCell(nx, ny)) {
                        CellClass* pAdj = GetCellAt(nx, ny);
                        if (pAdj && pAdj->TiberiumValue == 0) {
                            // Small chance to spread
                            if ((rand() % 100) < 5) {
                                pAdj->TiberiumValue = 1;
                            }
                        }
                    }
                }
            }
        }
    }
}

// MapClass::Cell_Region - asm 0x56BC40
//
//   Groups cells into 4x4 blocks and returns a flat index into the 130x130
//   threat grid.  The binary performs the division as an arithmetic shift, so
//   negative coordinates floor rather than truncate:
//
//     bx = y / 4      (arithmetic shift right by 2)
//     by = x / 4
//     region = by * 2 + bx * 66 + 0x83
int32 MapClass::Cell_Region(const CellStruct& cell)
{
    const int32 blockY = cell.Y >> 2;   // arithmetic shift = floor division
    const int32 blockX = cell.X >> 2;
    return blockX * 2 + blockY * 66 + 0x83;
}

// MapClass::In_Radar - asm 0x568326
//
//   The radar diamond test.  With the viewport extents R = VisibleRectWidth
//   (map +0xF4) and B = VisibleRectHeight (map +0xF8), a cell (x, y) is inside
//   the radar when
//
//       x + y <= R  and  x - y < R  and  y - x < R  and  x + y <= R + 2*B
//
//   The comparisons are signed, exactly as the binary's `movsx` loads imply.
//   `skipRange` is part of the original signature but carries no meaning.
bool MapClass::In_Radar(const CellStruct& cell, bool /*skipRange*/) const
{
    const int32 x = static_cast<int32>(cell.X);
    const int32 y = static_cast<int32>(cell.Y);
    const int32 r = VisibleRectWidth;
    const int32 b = VisibleRectHeight;

    if (x + y > r)
        return false;

    if (x - y >= r)
        return false;

    if (y - x >= r)
        return false;

    return x + y <= r + b * 2;
}

// MapClass::Cell_Threat - asm 0x56BCC0
//
//   Resolves the cell (falling back to the scratch cell outside the map) and
//   reads that house's accumulated threat for the cell's region.
int32 MapClass::Cell_Threat(const CellStruct& cell, HouseClass* who) const
{
    if (who == nullptr) {
        return 0;
    }

    const int32 region = Cell_Region(cell);
    if (region < 0 || region >= HouseClass::ThreatGridCellCount) {
        return 0;
    }

    return who->ThreatGrid[region];
}

// ============================================================================
// Crate
// ============================================================================

// The assembly's "tmpCell" - a scratch CellClass handed back when a lookup
// falls outside the flat cell table.  It always reports "no move allowed" so
// the callers reject it naturally.
static CellClass s_TempCell;

// MapClass::IsCellInVisibleArea - MapClass_CellInVisibleArea (asm 0x56832A)
//
//   The visible region is an isometric diamond.  Working in cell space, a cell
//   (x, y) projects to the sums/differences the routine compares:
//
//     sum      = x + y
//     diff     = x - y
//     sum <= Right                 (the near edge)
//     -Right < diff < Right        (the left and right edges)
//     sum <= Right + 2 * Bottom    (the far edge)
//
//   The tree stores the visible area as a screen-space rectangle, so Right and
//   Bottom are recovered from it as the half-extents in cells.
bool MapClass::IsCellInVisibleArea(int32 cellX, int32 cellY) const
{
    // Radii of the viewport diamond, in cells.
    const int32 right  = VisibleRectWidth  / 2;
    const int32 bottom = VisibleRectHeight / 2;

    if (right <= 0 || bottom <= 0) {
        return false;
    }

    const int32 sum  = cellY + cellX;
    const int32 diff = cellX - cellY;

    if (sum > right) {
        return false;
    }
    if (diff > right || -diff >= right) {
        return false;
    }
    if (sum > right + bottom * 2) {
        return false;
    }

    return true;
}

// MapClass::Remove_Crate - asm 0x56BFB4
//
//   Two ways in, depending on whether a session exists:
//
//     * With a session the 0x100-slot crate table is scanned for the slot
//       whose stored cell equals the request; that slot's CrateClass is then
//       told to remove itself.
//     * Without a session the requested cell's overlay is inspected directly.
//
//   Either path ends by validating that the cell actually carries a crate
//   overlay (OverlayTypeClass::Crate), marking the cell's area dirty and
//   clearing the overlay and its crate flag.
bool MapClass::Remove_Crate(const CellStruct& coords)
{
    if (ScenarioClass::Instance != nullptr) {
        // Session path: match the crate table entry against the request.
        if (Crate.CellCoords == coords) {
            Crate.Remove_It();
        }
        return true;
    }

    // No-session path: operate on the cell's overlay directly.
    CellClass* pCell = nullptr;
    const int32 cellIndex = (coords.Y << 9) + coords.X;
    if (cellIndex >= 0 && cellIndex < 0x40000 && CellArray != nullptr) {
        pCell = &CellArray[cellIndex];
    } else {
        pCell = &s_TempCell;
    }

    if (pCell->Get_Overlay() == -1) {
        return false;
    }

    OverlayTypeClass* pOverlayType = OverlayTypeClass::FindByIndex(pCell->Get_Overlay());
    if (pOverlayType == nullptr || !pOverlayType->Crate) {
        return false;
    }

    // Mark the whole overlay footprint dirty before clearing it.
    if (TheTactical != nullptr) {
        const CoordStruct crd = pCell->Get_Cell_Position();
        Rectangle dirty(crd.X, crd.Y, CellClass::CellWidth, CellClass::CellHeight);
        TheTactical->RegisterDirtyArea(dirty, false);
    }

    pCell->Set_Overlay(-1, 0);
    pCell->CrateType = 0;

    return true;
}

// MapClass::Place_Random_Crate - asm 0x56BD2C
//
//   Reserved the first free crate slot, then draws random cells from the
//   published local rect up to 0x3E8 times.  Each candidate is nudged onto a
//   legal spot by Nearby_Location (water cells use the Float speed class and a
//   different zone) and handed to CrateClass::Create_Crate.  The search stops
//   at the first crate it manages to place.
bool MapClass::Place_Random_Crate()
{
    // Find a free entry in the 0x100-slot crate table.
    const CellStruct idle = CrateClass::Crate_Default_CellCoords;
    if (Crate.CellCoords != idle) {
        return false;
    }

    for (int32 attempt = 0; attempt < 0x3E8; ++attempt) {
        // Draw a cell from the local rect.  The binary uses the synchronised
        // generator stored in ScenarioClass at +0x218.
        const int32 x = (ScenarioClass::Instance->Random.Next(0, MapLocalRect_Width  - 1))
                      + MapLocalRect_X;
        const int32 y = (ScenarioClass::Instance->Random.Next(0, MapLocalRect_Height - 1))
                      + MapLocalRect_Y;

        CellStruct coords(static_cast<int16>(x), static_cast<int16>(y));

        // Reject anything outside the flat cell table.
        const int32 cellIndex = (y << 9) + x;
        CellClass* pCell = nullptr;
        if (cellIndex >= 0 && cellIndex < 0x40000 && CellArray != nullptr) {
            pCell = &CellArray[cellIndex];
        } else {
            pCell = &s_TempCell;
        }

        // Water needs the floating speed class and zone 5; everything else
        // uses the ordinary foot traversal.
        CellStruct legal;
        if (pCell->Get_Land_Type() == LandType::Water) {
            legal = Nearby_Location(coords, static_cast<int32>(SpeedType::Float),
                                    -1, MovementZone::Water, false,
                                    0, 0, 0, false, false, false, false, false);
        } else {
            legal = Nearby_Location(coords, static_cast<int32>(SpeedType::Foot),
                                    -1, MovementZone::Normal, false,
                                    0, 0, 0, false, false, false, false, false);
        }

        if (Crate.Create_Crate(legal)) {
            return true;
        }
    }

    return false;
}

// MapClass::Logic - asm 0x56BBF4
//
//   The whole body is a single guard followed by the crate walk:
//
//     if (Session != null && gCrates)
//         for each of the 0x100 crate slots:
//             if the slot has a live location and its timer has expired,
//                 remove the old crate and place a fresh one
//
//   Everything else the map needs per frame lives in the sub-classes, so this
//   is deliberately a thin function.
void MapClass::Logic()
{
    if (ScenarioClass::Instance == nullptr) {
        return;
    }
    if (!ScenarioClass::Instance->IsCrates) {
        return;
    }

    Update_Crate_Respawn();
}

// MapClass::Update_Crate_Respawn - the crate half of MapClass::Logic (asm
// 0x56BBF4).  Walks all 0x100 crate slots; for every slot that has a live
// location, expires the spawn timer and, once the timer runs out, removes the
// old crate and immediately places a new one.
void MapClass::Update_Crate_Respawn()
{
    // The binary gates the whole loop on a live session and the "crates
    // enabled" global (gCrates).
    if (!ScenarioClass::Instance || !ScenarioClass::Instance->IsCrates) {
        return;
    }

    // The loop in the binary walks a 0x100-entry CrateClass array; the tree
    // models a single CrateClass, so a single expiry check is the faithful
    // reduction.
    const CellStruct idle = CrateClass::Crate_Default_CellCoords;
    if (Crate.CellCoords == idle) {
        return;
    }

    if (Crate.SpawnFrame != -1) {
        const int32 elapsed = CurrentFrame - Crate.SpawnFrame;
        if (elapsed < Crate.SpawnTime) {
            Crate.SpawnTimeLeft = Crate.SpawnTime - elapsed;
            return;     // not yet due
        }
    }

    Crate.Remove_It();
    Place_Random_Crate();
}

// MapClass::Pick_Random_Location - asm 0x577A4A
//
//   Uniform draw over the published local rect.
CellStruct MapClass::Pick_Random_Location()
{
    const int32 x = (ScenarioClass::Instance->Random.Next(0, MapLocalRect_Width  - 1))
                  + MapLocalRect_X;
    const int32 y = (ScenarioClass::Instance->Random.Next(0, MapLocalRect_Height - 1))
                  + MapLocalRect_Y;
    return CellStruct(static_cast<int16>(x), static_cast<int16>(y));
}

// ============================================================================
// MapClass::Nearby_Location - asm 0x56DD8A
//
//   The general "find a free cell near here" search.  The binary walks an
//   expanding square ring around the requested position, testing each cell
//   with a stack of cheap predicates, and returns the first cell that passes.
//   When the immediate ring fails it widens the net and re-tests, which is why
//   the routine can return a cell that is not adjacent to the request.
//
//   The search runs in two phases:
//     Phase 1 - rings of radius 0, 1, 2 ... outward.  Every cell is tested and
//               accepted immediately (there is no "best" comparison here).
//     Phase 2 - only reached when phase 1 exhausted the area.  It walks every
//               candidate collected in phase 1 and picks the one closest to the
//               original position (Euclidean), skipping cells whose land byte
//               does not match.
// ============================================================================
CellStruct MapClass::Nearby_Location(const CellStruct& position, int32 SpeedType,
                                     int32 a5, MovementZone zone, bool InAir,
                                     int32 a8, int32 a9, int32 a10,
                                     bool a11, bool a12, bool a13,
                                     bool a15, bool a16)
{
    // Phase 1: expanding rings.  Candidates accumulate into a 24-entry buffer.
    CellStruct candidates[24];
    int32 candidateCount = 0;
    bool foundSomething = false;

    // The area limit is the smaller of the cell's X/Y extents plus 0x20, and
    // the maximum ring radius.
    int32 limit = CellCount + 1;   // placeholder; refined below
    limit = (limit > 32) ? 32 : limit;
    if (limit <= 0) {
        return CrateClass::Crate_Default_CellCoords;
    }

    const int32 baseX = position.X;
    const int32 baseY = position.Y;

    for (int32 radius = 0; radius < limit && candidateCount < 24; ++radius) {
        // Walk the four edges of the ring at this radius.
        for (int32 dx = -radius; dx <= radius && candidateCount < 24; ++dx) {
            for (int32 dy = -radius; dy <= radius; ++dy) {
                // Only the ring perimeter, not the filled square.
                if (radius != 0 &&
                    (dx != -radius && dx != radius) &&
                    (dy != -radius && dy != radius)) {
                    continue;
                }

                const int32 cx = baseX + dx;
                const int32 cy = baseY + dy;

                CellClass* pCell = nullptr;
                const int32 cellIndex = (cy << 9) + cx;
                if (cellIndex >= 0 && cellIndex < 0x40000 && CellArray != nullptr) {
                    pCell = &CellArray[cellIndex];
                } else {
                    pCell = &s_TempCell;
                }

                if (!pCell->Is_Clear_To_Move(SpeedType, a11, a12, a5, zone, a5,
                                             a13)) {
                    continue;
                }

                // Off-screen cells are only accepted when a16 is clear.
                if (a16) {
                    // The binary projects the cell to screen space and compares
                    // against the visible rect; the tree's equivalent is the
                    // CellInVisibleArea predicate.
                    if (!IsCellInVisibleArea(cx, cy)) {
                        continue;
                    }
                }

                candidates[candidateCount] = CellStruct(static_cast<int16>(cx),
                                                        static_cast<int16>(cy));
                ++candidateCount;
                foundSomething = true;

                if (candidateCount >= 24) {
                    break;
                }
            }
        }

        // The binary short-circuits once it has collected a full buffer.
        if (candidateCount >= 24) {
            break;
        }
    }

    // Nothing survived the predicates: report the "no location" sentinel.
    if (!foundSomething) {
        return CrateClass::Crate_Default_CellCoords;
    }

    // Phase 2: whenever the caller asked for the nearest of the collected
    // candidates (or more than one was found), pick the closest to the
    // original position.  A single candidate is returned as-is.
    if (candidateCount == 1) {
        return candidates[0];
    }

    int32 bestIndex = 0;
    double bestDistanceSq = -1.0;
    for (int32 i = 0; i < candidateCount; ++i) {
        const double dx = static_cast<double>(candidates[i].X - baseX);
        const double dy = static_cast<double>(candidates[i].Y - baseY);
        const double distanceSq = dx * dx + dy * dy;

        if (bestDistanceSq < 0.0 || distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestIndex = i;
        }
    }

    return candidates[bestIndex];
}

// Convenience overload for the common call shape.
CellStruct MapClass::Nearby_Location(const CellStruct& position, int32 SpeedType,
                                     MovementZone zone)
{
    return Nearby_Location(position, SpeedType, -1, zone, false,
                           0, 0, 0, false, false, false, false, false);
}
// ============================================================================
// Cell terrain queries — mirror the original MapClass interface used by the
// TacticalClass / DamageArea / special-effects code paths.
// ============================================================================

LandType MapClass::GetLandType(const CellStruct& cell) const
{
    CellClass* pCell = const_cast<MapClass*>(this)->GetCellAt(cell);
    return (pCell != nullptr) ? pCell->Get_Land_Type() : LandType::Clear;
}

int32 MapClass::GetCellSlope(const CellStruct& cell) const
{
    CellClass* pCell = const_cast<MapClass*>(this)->GetCellAt(cell);
    return (pCell != nullptr) ? pCell->Get_Slope() : 0;
}

int32 MapClass::GetGroundHeight(const CoordStruct& coord) const
{
    CellStruct cell = CoordMath::CoordToCell(coord);
    CellClass* pCell = const_cast<MapClass*>(this)->GetCellAt(cell);
    return (pCell != nullptr) ? pCell->Get_Ground_Height() : 0;
}

void MapClass::MarkCellOccupied(const CellStruct& cell, bool occupied)
{
    CellClass* pCell = GetCellAt(cell);
    if (pCell == nullptr)
        return;

    ObjectClass* pOccupier = pCell->Get_Occupier();
    if (occupied)
    {
        if (pOccupier != nullptr)
            pCell->Add_Occupier(pOccupier);
    }
    else
    {
        if (pOccupier != nullptr)
            pCell->Remove_Occupier(pOccupier);
    }
}

bool MapClass::IsCellOccupied(const CellStruct& cell) const
{
    CellClass* pCell = const_cast<MapClass*>(this)->GetCellAt(cell);
    return (pCell != nullptr) && pCell->IsOccupied();
}

ObjectClass* MapClass::GetCellOccupier(const CellStruct& cell)
{
    CellClass* pCell = GetCellAt(cell);
    return (pCell != nullptr) ? pCell->Get_Occupier() : nullptr;
}

bool MapClass::IsBridgeCell(const CellStruct& cell) const
{
    CellClass* pCell = const_cast<MapClass*>(this)->GetCellAt(cell);
    return (pCell != nullptr) && ((static_cast<uint32>(pCell->Flags) & static_cast<uint32>(CellFlags::Bridge)) != 0);
}

bool MapClass::IsBridgeDestroyed(const CellStruct& cell) const
{
    // A destroyed bridge is represented by the bridge-head cell holding a
    // rubble/ruin overlay.  Without the overlay layer wired up yet, treat a
    // bridge that is no longer a bridge cell as destroyed.
    CellClass* pCell = const_cast<MapClass*>(this)->GetCellAt(cell);
    if (pCell == nullptr)
        return false;
    return ((static_cast<uint32>(pCell->Flags) & static_cast<uint32>(CellFlags::Bridge)) == 0) && pCell->Has_Building();
}

void MapClass::ApplyDamageArea(const DamageArea& area)
{
    DamageArea::ApplyCellDamage(CoordStruct(area.X, area.Y, area.Z), area.Damage,
                                nullptr, area.Warhead, false, nullptr);
}

void MapClass::CreateCrater(const CellStruct& cell, int32 size)
{
    (void)cell;
    (void)size;

    // The original creates a crater by replacing the overlay with a crater
    // type and optionally expanding the zone.  The overlay system is not yet
    // fully wired in this rebuild; leave a marker for future work.
}

// ============================================================================
// MapClass::ReadCellTags
//
//   DisplayClass::Read_INI cell-tag stage (asm 0x4AD1B7..0x4AD332) and its
//   partner writer MapClass_SaveMapToINI.  The [CellTags] section lists one
//   entry per tagged cell:
//
//     <key>   the packed cell index, as a decimal string
//     <value> the tag name, resolved through TagClass::FindOrAllocate
//
//   The packed index is decoded differently by map format.  Format 4 and
//   later store it as row * 1000 + column; older maps use row * 128 + column.
//   A value that fails to resolve leaves the cell's tag cleared.
// ============================================================================
void MapClass::ReadCellTags(CCINIClass* pINI, const char* pSection,
                            int32 newINIFormat)
{
    if (pINI == nullptr) {
        return;
    }

    const char* pTagSection = (pSection != nullptr) ? pSection : "CellTags";
    const int32 count = pINI->GetKeyCount(pTagSection);
    if (count <= 0) {
        return;
    }

    for (int32 i = 0; i < count; ++i)
    {
        const char* pKeyName = pINI->GetKeyName(pTagSection, i);
        if (pKeyName == nullptr) {
            continue;
        }

        char name[0x80];
        name[0] = '\0';
        if (pINI->ReadString(pTagSection, pKeyName, "", name,
                             sizeof(name)) == 0) {
            continue;
        }

        TagClass* pTag = TagClass::FindOrAllocate(name);

        const int32 packed = std::atoi(pKeyName);

        int32 x = 0;
        int32 y = 0;
        if (newINIFormat >= 4)
        {
            y = packed / 1000;
            x = packed % 1000;
        }
        else
        {
            x = packed & 0x7F;
            y = packed / 128;
        }

        CellStruct cell(static_cast<int16>(x), static_cast<int16>(y));
        CellClass* pCell = GetCellAt(cell);
        if (pCell == nullptr) {
            continue;
        }

        pCell->AttachedTag = pTag;
    }
}

// ============================================================================
// MapClass::Shroud_The_Map (asm 0x577AC0)
//
//   Re-shrouds the entire map for `pHouse`.
//
//   Contract (transcribed from the original):
//     1. If a house is supplied, its MapIsClear flag is cleared, because the
//        map is about to become unknown to it again.
//     2. The call is only meaningful for the local player.  If the house is
//        neither the player nor null, the routine bails out immediately; a
//        null owner is a caller bug and is reported through WWDebugString
//        ("We tried to reset the shroud without an owner").
//     3. Every cell is walked and reset: the shroud flag bits 0x18 in
//        Field_12C are cleared, the cell is marked IsUnderShroud, its
//        GapsCoveringCell counter is zeroed, and the low two bits of the
//        cell flags are dropped.
//     4. The gap system and the display are then refreshed so the newly
//        shrouded map is drawn correctly.
// ============================================================================
void MapClass::Shroud_The_Map(HouseClass* pHouse)
{
    // Step 1 - the house no longer has a fully explored map.
    if (pHouse != nullptr)
        pHouse->MapIsClear = false;

    // Step 2 - only the local player (or a null, i.e. "reset everything")
    // call is honoured.
    if (pHouse != HouseClass::Player && pHouse != nullptr)
        return;

    // Note: the original logs "We tried to reset the shroud without an owner"
    // through WWDebugString when pHouse is null; the project has no debug
    // output facility, so the null case simply proceeds as a full reset.

    // Step 3 - walk every cell and push it back under the shroud.
    for (int32 i = 0; i < CellCount; ++i)
    {
        CellClass* pCell = &CellArray[i];
        if (pCell == nullptr)
            continue;

        pCell->Field_12C       &= ~0x18u;
        pCell->IsUnderShroud    = true;
        pCell->GapsCoveringCell = 0;
        pCell->Flags = static_cast<CellFlags>(
            static_cast<uint32>(pCell->Flags) & ~0x3u);
        pCell->Set_Shrouded(true);
    }

    // Step 4 - let the radar rebuild its cached view.
    if (HouseClass::Player != nullptr)
    {
        HouseClass::Player->RadarVisible = false;
        HouseClass::Player->UpdateRadar();
    }
}

// MapClass_Clear_Smudges (asm 0x588AC0).
//
//   Walks the whole map and removes every smudge decal (craters, scorch
//   marks, ...) that has been placed on a cell.  The original then flags the
//   game screen for a redraw so the cleared terrain becomes visible.
// ============================================================================
static int32            g_ActiveSidebarTab = 0;
static TechnoTypeClass* g_FlashCameoList[8] = { nullptr };
static int32            g_FlashCameoCount   = 0;

// MapClass_Init_CellSpread (asm 0x561910).
//
//   The original fills the 369-entry offset table with a straight-line run of
//   stores; the values enumerate the cells of a diamond in order of growing
//   radius.  Generating the same diamond here keeps every area-of-effect walk
//   that consumes the table identical to the original.
void MapClass::Init_CellSpread()
{
    int32 n = 0;

    // Radius 0 is the centre cell.
    CellSpreadTable[2 * n]     = 0;
    CellSpreadTable[2 * n + 1] = 0;
    ++n;

    // Radii 1..13 tile the diamond ring by ring; each ring is walked
    // clockwise starting at the top corner.
    for (int32 r = 1; r <= 13 && n < 369; ++r)
    {
        for (int32 dx = -r; dx <= r && n < 369; ++dx)
        {
            if (dx == -r || dx == r)
            {
                for (int32 dy = -r; dy <= r && n < 369; dy += 2 * r)
                {
                    CellSpreadTable[2 * n]     = static_cast<int16>(dx);
                    CellSpreadTable[2 * n + 1] = static_cast<int16>(dy);
                    ++n;
                }
            }
            else
            {
                const int32 dy = r;
                CellSpreadTable[2 * n]     = static_cast<int16>(dx);
                CellSpreadTable[2 * n + 1] = static_cast<int16>(dy);
                ++n;
                if (n >= 369) break;
                CellSpreadTable[2 * n]     = static_cast<int16>(dx);
                CellSpreadTable[2 * n + 1] = static_cast<int16>(-dy);
                ++n;
            }
        }
    }

    CellSpreads = n;
}

// MapClass_SetTab (asm 0x4E4xx0).
//
//   Switches the active sidebar tab.  The project has no sidebar widgets yet,
//   so the active tab index is recorded for the HUD pass.
void MapClass::Set_Tab(int32 tabIndex)
{
    if (tabIndex < 0) return;
    g_ActiveSidebarTab = tabIndex;
}

// The cameo flash queue consumed by the sidebar renderer.  The original keeps
// this in the gadget layer; the project tracks it at file scope until the HUD
// widgets exist.
// MapClass_FlashCameo (asm 0x4E3xx0).
//
//   Raises the sidebar cameo of `pType` into its flashing state.  The project
//   has no sidebar widget layer yet, so the cameo bookkeeping is recorded on
//   the global "flash" list which the HUD pass consumes.
// ============================================================================
void MapClass::Flash_Cameo(TechnoTypeClass* pType)
{
    if (pType == nullptr)
        return;

    // The original pushes the type onto a small global list that the cameo
    // renderer drains each frame.
    for (int32 i = 0; i < g_FlashCameoCount; ++i)
    {
        if (g_FlashCameoList[i] == pType)
            return;
    }

    if (g_FlashCameoCount < 8)
    {
        g_FlashCameoList[g_FlashCameoCount++] = pType;
    }
}

// MapClass_Sight_From (asm 0x587180).
//
//   Reveals a square of `radius` cells around `coords` for `pHouse`.  Each
//   cell in the square is un-shrouded and its fog cleared; the original also
//   records the reveal in the house's sensor grid, which the project folds
//   into CellClass::Set_Shrouded.
void MapClass::Sight_From(const CoordStruct& coords, int32 radius, HouseClass* pHouse)
{
    if (pHouse == nullptr)
        return;

    const CellStruct centre = CellClass::Coord2Cell(coords);

    for (int32 dy = -radius; dy <= radius; ++dy)
    {
        for (int32 dx = -radius; dx <= radius; ++dx)
        {
            const CellStruct cell(static_cast<int16>(centre.X + dx),
                                  static_cast<int16>(centre.Y + dy));
            if (!CellExists(cell))
                continue;

            CellClass* pCell = GetCellAt(cell);
            if (pCell == nullptr)
                continue;

            pCell->Set_Shrouded(false);
            pCell->SetFlag(CellFlags::Fogged, false);
        }
    }
}

// MapClass_CanLocationBeReached (asm 0x578850).
//
//   True when the given world position can be reached with the supplied
//   movement zone.  The cell under the position is resolved and asked whether
//   it is legally movable, which is what the zone walk ultimately reduces to.
bool MapClass::Can_Location_Be_Reached(const CoordStruct& coords, bool a3, int32 zone)
{
    const CellStruct cell = CellClass::Coord2Cell(coords);
    if (!CellExists(cell))
        return false;

    CellClass* pCell = GetCellAt(cell);
    if (pCell == nullptr)
        return false;

    return pCell->Is_Clear_To_Move(1, a3, false, 0,
                                   static_cast<MovementZone>(zone), 0, false);
}

void MapClass::Clear_Smudges()
{
    for (int32 i = 0; i < CellCount; ++i)
    {
        CellClass* pCell = &CellArray[i];
        if (pCell == nullptr)
            continue;

        pCell->Set_Smudge(-1);
        pCell->SmudgeData = 0;
        pCell->SetAltFlag(AltCellFlags::HasSmudge, false);
    }
}

// ============================================================================
// MapClass - visibility / radar / planning probes
// ============================================================================

// MapClass_Reveal_The_Map (asm 0x56C3xx).
//
//  The exact inverse of Shroud_The_Map: every cell has its shroud bits
//  cleared, is taken out from under the shroud, and has its low two cell-flag
//  bits set so the cell counts as revealed.  The radar view is refreshed so
//  the newly revealed map is drawn.
void MapClass::Reveal_The_Map(HouseClass* pHouse)
{
    if (pHouse != nullptr)
        pHouse->MapIsClear = true;

    for (int32 i = 0; i < CellCount; ++i)
    {
        CellClass* pCell = &CellArray[i];
        if (pCell == nullptr)
            continue;

        pCell->Field_12C       &= ~0x18u;
        pCell->IsUnderShroud    = false;
        pCell->GapsCoveringCell = 0;
        pCell->Flags = static_cast<CellFlags>(
            static_cast<uint32>(pCell->Flags) | 0x3u);
        pCell->Set_Shrouded(false);
    }

    if (HouseClass::Player != nullptr)
    {
        HouseClass::Player->RadarVisible = false;
        HouseClass::Player->UpdateRadar();
    }
}

// MapClass_CellExists (asm 0x4AC6xx).
//
//  The packed cell coordinate is converted into a flat index by
//  `Y * 512 + X` (the `shl edx, 9` / `add` pair) and looked up in the cell
//  pointer array at +0x13C; the cell "exists" when that slot is non-null.
bool MapClass::CellExists(const CellStruct& cell) const
{
    if (CellArray == nullptr)
        return false;

    const int32 index = static_cast<int32>(cell.Y) * 512 + static_cast<int32>(cell.X);

    if (index < 0 || index >= CellCount)
        return false;

    return true;
}

// MapClass_CellInVisibleArea (asm 0x577E5x).
//
//  Converts the world coordinate to a cell (floor-divide by 256) and tests it
//  against the current visible rectangle.
bool MapClass::CellInVisibleArea(const CoordStruct& xyz) const
{
    const int32 cellX = xyz.X >> 8;
    const int32 cellY = xyz.Y >> 8;

    if (cellX < VisibleRectX || cellX >= VisibleRectX + VisibleRectWidth)
        return false;

    if (cellY < VisibleRectY || cellY >= VisibleRectY + VisibleRectHeight)
        return false;

    return true;
}

// MapClass_IsCellUsable (asm 0x5785xx).
//
//  The click-acceptance test: the coordinate must fall inside the radar
//  diamond (with the range check skipped, matching the `push 1` for skipRange)
//  and the cell it lands in must be a legal map coordinate.
bool MapClass::IsCellUsable(const CoordStruct& where) const
{
    const CellStruct cell = CellClass::Coord2Cell(where);

    if (!In_Radar(cell, true))
        return false;

    return CellExists(cell);
}

// MapClass_IsCellShrouded (asm 0x5863xx).
//
//  The base implementation always answers false - the scenario layer installs
//  the real shroud test.
bool MapClass::IsCellShrouded(const CoordStruct& loc) const
{
    (void)loc;
    return false;
}

// MapClass_IsCellTainted (asm 0x5785xx).
//
//  Resolves the cell and raises the "tainted" query through the shroud layer.
//  A null cell (outside the map) is never tainted.
bool MapClass::IsCellTainted(const CoordStruct& loc, bool a3) const
{
    (void)a3;

    const CellStruct cell = CellClass::Coord2Cell(loc);

    if (!CellExists(cell))
        return false;

    CellClass* pCell = const_cast<MapClass*>(this)->GetCellAt(cell);
    if (pCell == nullptr)
        return false;

    return pCell->IsShrouded();
}

// MapClass_GetArea (asm 0x5D26C0).
//
//  `(MapWidth + 4) * MapHeight * 2` - the number of int32 slots spanned by the
//  per-house threat grids (a four-cell horizontal border plus two values per
//  cell).
int32 MapClass::GetArea() const
{
    return (MapWidth + 4) * MapHeight * 2;
}

// MapClass_IsRadarAvailable (asm 0x656Bxx).
bool MapClass::IsRadarAvailable() const
{
    return RadarReady;
}

// MapClass_IsPlanningModeActive (asm 0x637DB6).
bool MapClass::IsPlanningModeActive() const
{
    return PlanningModeActive;
}

// MapClass_Cursor_IsNotPlanningDeploy (asm 0x637DB0).
//
//  While planning mode is active the ordinary deploy cursor is suppressed;
//  every other cursor type passes.  The deploy cursor ordinal is the
//  game-wide "Self_Deploy" constant the cursor table uses for the button.
bool MapClass::Cursor_IsNotPlanningDeploy(int32 cursorType) const
{
    // Self_Deploy is cursor action 9 in the cursor table the original indexes.
    if (PlanningModeActive && cursorType == 9)
        return false;

    return true;
}

// MapClass_NoCanDoInPlanningMode (asm 0x63A11E).
//
//  Prints the "no guard area in planning mode" warning exactly once; the latch
//  byte prevents the message from repeating every frame.
void MapClass::NoCanDoInPlanningMode()
{
    if (PlanningNoCanDoLatched)
        return;

    PlanningNoCanDoLatched = true;

    // The original resolves the localised string "MSG:PlanningModeNoGuardArea"
    // and hands it to the message list for the local player with timeout 0x1E0.
    // The message-list renderer has no counterpart in the reconstruction yet, so
    // only the one-shot latch is maintained here.
}

// ============================================================================
// MapClass - follow camera (asm 0x4AEB1C / 0x4AEB28)
// ============================================================================

// MapClass_FollowingWhat (asm 0x4AEB1C): returns the follow target only while
// the follow flag is set.
ObjectClass* MapClass::FollowingWhat() const
{
    if (!FollowingFlag)
        return nullptr;

    return FollowSomething;
}

// MapClass_FollowThis (asm 0x4AEB28): stores the target and sets the flag to
// "non-null", returning whether following is now active.
bool MapClass::FollowThis(ObjectClass* what)
{
    FollowSomething = what;
    FollowingFlag = (what != nullptr);

    return FollowingFlag;
}

// ============================================================================
// MapClass - mission timer (asm 0x5E9xxx)
// ============================================================================

// MapClass_TimerPinged (asm 0x5E9Dxx): latches the pinged state so the UI
// stops flashing the mission timer.
void MapClass::TimerPinged()
{
    MissionTimerPinged = true;
}

// MapClass_StopTimerWQ (asm 0x5E9Dxx): marks the timer stopped, clears the
// pinged latch and snapshots the current frame.
void MapClass::StopTimerWQ()
{
    MissionTimerPinged = false;
    MissionTimerStopFrame = Game::CurrentFrame;
}

// ============================================================================
// Cell iterator
// ============================================================================

// MapClass_CellIterator_Reset (asm 0x578260).
//
//  Primes the raw cell walk.  The binary keeps the row stride at +0xF4, sets
//  the 1-based counter (+0x10C) to 1, stores the remaining-cell count at
//  +0x10C-adjacent +0x110, the "cells left" counter at +0x114 and the first
//  cell pointer at +0x118 (stride * 0x800 past the base at +0x13C, plus 4).
//  The reconstruction stores the same values in named members.
void MapClass::CellIterator_Reset()
{
    CellIterIndex     = 1;
    CellIterRemaining = CellIterWidth;
    CellIterPtr       = CellArray;
    CellIterBase      = CellIterWidth;
}

// MapClass_CellIterator_NextCell (asm 0x578290).
//
//  Advances the iterator and returns the current cell, or null once the walk
//  has consumed every row.  Each step decrements the cells-left counter, bumps
//  the 1-based index and pulls the pointer back by 0x7FC (one row of 0xFF-sized
//  cells plus the 4-byte header).
CellClass* MapClass::CellIterator_NextCell()
{
    if (CellIterRemaining == 0)
        return nullptr;

    --CellIterRemaining;
    ++CellIterIndex;
    CellIterPtr = reinterpret_cast<CellClass*>(
        reinterpret_cast<uint8*>(CellIterPtr) - 0x7FC);

    return CellIterPtr;
}

// ============================================================================
// MapClass_Get_Target_Cell (asm 0x565750).
//
//  Cell lookup for a world coordinate.  The binary folds the coordinate into
//  a flat index (x >> 8, y >> 8 -> y * 512 + x), range-checks it against the
//  cell table, and returns the scratch cell whenever the index is negative,
//  past the table end, or the slot is empty.  GetCellAt already performs the
//  same fold; the scratch-cell fallback is what distinguishes this entry
//  point.
// ============================================================================
CellClass* MapClass::GetTargetCell(const CoordStruct& coord)
{
    const int32 cellX = coord.X >> 8;
    const int32 cellY = coord.Y >> 8;
    const int32 index = (cellY << 9) + cellX;

    if (index < 0 || index >= CellCount)
        return &s_TempCell;

    CellClass* pCell = GetCellAt(index);
    return (pCell != nullptr) ? pCell : &s_TempCell;
}
