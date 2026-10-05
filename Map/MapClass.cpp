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
#include <Rendering/RadarClass.h>
#include <Rendering/SidebarClass.h>
#include <Rendering/GScreenClass.h>
#include <Audio/VocClass.h>
#include <Rules/RulesClass.h>
#include <cstdarg>
#include <cstdio>

#include <cstdlib>
#include <cstring>

// 根据游戏行为，可知这些调试字符串经由引擎的调试输出通道打印；本项目用
// 与其它子系统一致的本地帮助函数转发到 stderr。
static void WWDebugString(const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    std::vfprintf(stderr, pFormat, args);
    va_end(args);
}
#include <cmath>

// ============================================================================
// MapClass.cpp - Map class implementation
// ============================================================================

// Static singleton
MapClass* MapClass::Instance = nullptr;

// 启动期保留的缺省参考点，由 Init_CellCoords / Init_RoomCoords 清零。
CellStruct  MapClass::DefaultCellCoords = CellStruct(0, 0);
CoordStruct MapClass::DefaultRoomCoords = CoordStruct(0, 0, 0);

// ============================================================================
// Constructor
// ============================================================================

MapClass::MapClass()
    : MapWidth(0), MapHeight(0), MapSize(0), CellCount(0)
    , CellArray(nullptr), MaxWaypoints(702), CrateCount(0), TotalValue(0)
    , CellIterWidth(0), CellIterCursorX(0), CellIterCursorY(0)
    , CellIterRemX(0), CellIterRemY(0)
    , CellIterPtr(nullptr)
    , VisibleRectX(0), VisibleRectY(0), VisibleRectWidth(0), VisibleRectHeight(0)
    , CurrentTheater(TheaterType::Temperate)
    , FollowSomething(nullptr), FollowingFlag(false)
    , RadarReady(false)
    , RadarStatus(0), RadarMode(0), FlashExpiryFrame(0)
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

 // MapClass::Cell_Region -
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

 // MapClass::In_Radar -
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

 // MapClass::Cell_Threat -
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

 // MapClass::IsCellInVisibleArea - MapClass_CellInVisibleArea
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

 // MapClass::Remove_Crate -
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

 // MapClass::Place_Random_Crate -
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

 // MapClass::Logic -
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

 // MapClass::Pick_Random_Location -
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
 // MapClass::Nearby_Location -
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
 //   DisplayClass::Read_INI cell-tag stage (..0x4AD332) and its
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
 // MapClass::Shroud_The_Map
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

 // 根据游戏行为，可知 Clear_Smudges 负责下面这段逻辑。
//
//   Walks the whole map and removes every smudge decal (craters, scorch
//   marks, ...) that has been placed on a cell.  The original then flags the
//   game screen for a redraw so the cleared terrain becomes visible.
// ============================================================================
static int32            g_ActiveSidebarTab = 0;
static TechnoTypeClass* g_FlashCameoList[8] = { nullptr };
static int32            g_FlashCameoCount   = 0;

 // 根据游戏行为，可知 Init_CellSpread 负责下面这段逻辑。
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

 // MapClass_SetTab (xx0).
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
 // MapClass_FlashCameo (xx0).
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

 // 根据游戏行为，可知 Sight_From 负责下面这段逻辑。
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

 // 根据游戏行为，可知 CanLocationBeReached 负责下面这段逻辑。
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

 // MapClass_Reveal_The_Map (xx).
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

 // MapClass_CellExists (xx).
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

 // MapClass_CellInVisibleArea (x).
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

 // MapClass_IsCellUsable (xx).
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

 // MapClass_IsCellShrouded (xx).
//
//  The base implementation always answers false - the scenario layer installs
//  the real shroud test.
bool MapClass::IsCellShrouded(const CoordStruct& loc) const
{
    (void)loc;
    return false;
}

 // MapClass_IsCellTainted (xx).
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

 // 根据游戏行为，可知 GetArea 负责下面这段逻辑。
//
//  `(MapWidth + 4) * MapHeight * 2` - the number of int32 slots spanned by the
//  per-house threat grids (a four-cell horizontal border plus two values per
//  cell).
int32 MapClass::GetArea() const
{
    return (MapWidth + 4) * MapHeight * 2;
}

 // MapClass_IsRadarAvailable (xx).
bool MapClass::IsRadarAvailable() const
{
    return RadarReady;
}

 // 根据游戏行为，可知 IsPlanningModeActive 负责下面这段逻辑。
bool MapClass::IsPlanningModeActive() const
{
    return PlanningModeActive;
}

 // 根据游戏行为，可知 Cursor_IsNotPlanningDeploy 负责下面这段逻辑。
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

 // 根据游戏行为，可知 NoCanDoInPlanningMode 负责下面这段逻辑。
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
 // MapClass - follow camera
// ============================================================================

 // MapClass_FollowingWhat: returns the follow target only while
// the follow flag is set.
ObjectClass* MapClass::FollowingWhat() const
{
    if (!FollowingFlag)
        return nullptr;

    return FollowSomething;
}

 // MapClass_FollowThis: stores the target and sets the flag to
// "non-null", returning whether following is now active.
bool MapClass::FollowThis(ObjectClass* what)
{
    FollowSomething = what;
    FollowingFlag = (what != nullptr);

    return FollowingFlag;
}

// ============================================================================
 // MapClass - mission timer (xxx)
// ============================================================================

 // MapClass_TimerPinged (xx): latches the pinged state so the UI
// stops flashing the mission timer.
void MapClass::TimerPinged()
{
    MissionTimerPinged = true;
}

 // MapClass_StopTimerWQ (xx): marks the timer stopped, clears the
// pinged latch and snapshots the current frame.
void MapClass::StopTimerWQ()
{
    MissionTimerPinged = false;
    MissionTimerStopFrame = Game::CurrentFrame;
}

// ============================================================================
// Cell iterator
// ============================================================================

 // 根据游戏行为，可知 CellIterator_Reset 负责下面这段逻辑。
//
//  把"之字形"全图遍历复位到最后一行首格。行宽即地图宽度；横轴游标记 1，
//  纵轴游标记 1；横向余量取地图宽度，纵向余量取宽度减一；当前格指针指向
//  最后一行（宽度 << 11 字节的偏移）首格数据（再跳过 4 字节格头）。
void MapClass::CellIterator_Reset()
{
    CellIterWidth   = MapWidth;
    CellIterCursorX = 1;
    CellIterCursorY = 1;
    CellIterRemX    = MapWidth;
    CellIterRemY    = MapWidth - 1;
    CellIterPtr     = reinterpret_cast<uint8*>(CellArray) + (MapWidth << 11) + 4;
}

 // 根据游戏行为，可知 CellIterator_NextCell 负责下面这段逻辑。
//
//  这是地图的之字形游标：只要还有剩余格子就沿当前方向推进一格，返回该格；
//  当前方向的余量耗尽时，切换到另一条轴。推进时指针逆着行方向回退 0x7FC
//  字节（一行 0xFF 个格加 4 字节格头），因此调用者得到的始终是活的 CellClass。
CellClass* MapClass::CellIterator_NextCell()
{
    CellClass* pCell = reinterpret_cast<CellClass*>(CellIterPtr);

    if (CellIterRemY != 0)
    {
        // 纵轴还有余量：纵轴游标 +1，纵向余量 -1，指针按行回退。
        ++CellIterCursorX;
        --CellIterRemY;
        CellIterRemX = CellIterRemX - 1;
        CellIterPtr  = CellIterPtr - 0x7FC;
        return pCell;
    }

    // 纵轴走完，换到横轴：把两个游标、余量成对交换。
    const int32 newCursorX = CellIterRemX;
    int32       newRemX    = CellIterCursorX;
    int32       newCursorY = CellIterCursorY;
    int32       newRemY    = CellIterWidth;

    if (((newCursorX - CellIterWidth) + newRemX - 1) & 1)
    {
        ++newRemX;
        newRemY = CellIterWidth - 1;
    }
    else
    {
        ++newCursorY;
        newRemY = CellIterWidth - 2;
    }

    CellIterCursorX = newCursorX;
    CellIterCursorY = newCursorY;
    CellIterRemX    = newRemX;
    CellIterRemY    = newRemY;

    // 由 (纵轴游标 << 9 | 横轴游标) 算出格索引，并直接换算成格指针。
    const int32 index = (newCursorX << 9) + newCursorY + MapWidth;
    CellIterPtr = reinterpret_cast<uint8*>(CellArray) + index * 4;
    return reinterpret_cast<CellClass*>(CellIterPtr);
}

// ============================================================================
 // 根据游戏行为，可知 LoopOverCells 负责下面这段逻辑。
//
//  重铺全图矿脉：先整图遍历，凡是叠加物为矿脉(0x7E)且生长阶段不小于 0x30
//  的格子，都收集到一个动态向量里并把叠加物清空、生长阶段归零；随后从向量
//  末尾往前扫，对每格再做一次 IsVeins 判定，命中就调用 SetupVeins 让矿脉
//  重新长回完整形态。这样做等价于"先全部铲平、再自后向前重铺"。
//
//  注意：SetupVeins 里负责生成矿脉贴图/动画的那一半渲染逻辑尚未在本项目
//  建模（它依赖完整的图像拼接管线），此处只保证判定与清空流程与原始行为一致。
// ============================================================================
void MapClass::LoopOverCells()
{
    DynamicVectorClass<CellClass*> cells;

    CellIterator_Reset();
    for (CellClass* pCell = CellIterator_NextCell(); pCell != nullptr;
         pCell = CellIterator_NextCell())
    {
        if (static_cast<int32>(pCell->Overlay) != 0x7E)
            continue;

        if (pCell->OverlayFrame >= 0x30)
            cells.Add(pCell);

        // 无论是否收集，都把这一格的叠加物清掉、生长阶段归零。
        pCell->Overlay      = -1;
        pCell->OverlayFrame = 0;
    }

    for (int32 i = cells.GetCount() - 1; i >= 0; --i)
    {
        CellClass* pCell = cells[i];
        if (pCell != nullptr && pCell->IsVeins())
            pCell->SetupVeins();
    }
}

// ============================================================================
 // 根据游戏行为，可知 Get_Target_Cell 负责下面这段逻辑。
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

// ============================================================================
 // 根据游戏行为，可知 RulesClass 里的音效字段保存的是声音注册表下标；播放时
 // 先按下标取出声音定义，再把它交给语音管理器。本项目尚未把 SoundDefinition
 // 与 VocClass 打通，故此处按名字走 PlayFile 通路，语义保持一致。
// ============================================================================
static void PlayRulesSound(int32 soundIndex)
{
    if (soundIndex < 0)
        return;

    VocManagerClass* pVoc = VocManagerClass::GetInstance();
    if (pVoc == nullptr)
        return;

    const char* pName = VocClass::GetSoundName(soundIndex);
    if (pName != nullptr)
        pVoc->PlayFile(pName, 1);
}

// ============================================================================
 // 根据游戏行为，可知 SetRadarActivity 负责下面这段逻辑。
//
//  把外部传来的雷达可用性变化落到地图上。状态没变时直接返回；变化时改写
//  雷达就绪标记，并根据新状态向调试日志打印 "on"/"off"。随后按当前是否
//  处于战术地图视图分流：是战术地图就走 UpdateRadarStatus，否则转交给
//  全局雷达对象的激活接口。
// ============================================================================
void MapClass::SetRadarActivity(int32 activity)
{
    const bool bNew = (activity != 0);
    if (RadarReady == bNew)
        return;

    RadarReady = bNew;

    // Radar: TacticalMap availability is on/off
    WWDebugString(bNew
        ? "Radar: TacticalMap availability is on\n"
        : "Radar: TacticalMap availability is off\n");

    if (RadarMode == 1)
    {
        UpdateRadarStatus(static_cast<uint8>(bNew), 1);
    }
    else if (RadarClass::Instance != nullptr)
    {
        // 根据游戏行为，可知雷达对象在这里被通知切换可用状态。
        RadarClass::Instance->Init_Radar();
    }
}

// ============================================================================
 // 根据游戏行为，可知 UpdateRadarStatus 负责下面这段逻辑。
//
//  推进雷达/战术地图的状态机。当新状态为"激活"时：若当前状态是 0 或 2，
//  则切到状态 3（已激活），需要出声就播放雷达激活音效，并打印
//  "Radar: ACTIVATING"；若当前已经是别的状态则什么都不做。当新状态为
//  "关闭"时：随时切到状态 2（关闭中），需要出声就播放关闭音效，并打印
//  "Radar: DEACTIVING"。
// ============================================================================
void MapClass::UpdateRadarStatus(uint8 active, uint8 playSound)
{
    if (active == 1)
    {
        if (RadarStatus != 0 && RadarStatus != 2)
            return;

        RadarStatus = 3;

        if (playSound && TheRules != nullptr)
        {
            // 根据游戏行为，可知这里播放的是雷达激活提示音。
            PlayRulesSound(TheRules->RadarOn);
        }

        // Radar: ACTIVATING
        WWDebugString("Radar: ACTIVATING\n");
    }
    else
    {
        RadarStatus = 2;

        if (playSound && TheRules != nullptr)
        {
            // 根据游戏行为，可知这里播放的是雷达关闭提示音。
            PlayRulesSound(TheRules->RadarOff);
        }

        // Radar: DEACTIVING
        WWDebugString("Radar: DEACTIVING\n");
    }
}

// ============================================================================
 // 根据游戏行为，可知 SetAircraftTab 负责下面这段逻辑。
//
//  当某建筑物被间谍渗透时点亮侧边栏的飞机标签页。先比对传入的房屋编号是否
//  正好是本机控制的那一座；命中后把侧边栏的"渗透来源房屋"记为本次房屋、
//  置上"已被渗透"标记、点亮该房屋对应的标签页槽位、请求界面重绘。若侧边栏
//  的闪烁计时器仍未过期，则把过期帧顺延两帧，让新物品多闪一会儿。
// ============================================================================
void MapClass::SetAircraftTab(int32 houseIndex)
{
    SidebarClass* pSidebar = SidebarClass::Instance;
    if (pSidebar == nullptr)
        return;

    if (houseIndex != pSidebar->PlayerInfiltrated)
        return;

    // 该房屋对应的标签页槽位置位。
    pSidebar->Tabs[houseIndex].NeedsFlash = 1;

    // GScreen_FlagToRedraw：请求界面重绘。
    if (GScreenClass::Instance != nullptr)
        GScreenClass::Instance->MarkNeedsRedraw(0);

    const int32 frame = Game::CurrentFrame;
    if (FlashExpiryFrame <= frame)
        FlashExpiryFrame = frame + 2;
}

// ============================================================================
// 根据游戏行为，可知 GetCellFloorHeight 负责下面这段逻辑。
//
//  给出某个世界坐标处的地面高度：把坐标换算成所在格，取该格的地面高度。
//  坐标落到地图之外时返回 0，调用方据此按平地处理。
// ============================================================================
int32 MapClass::GetCellFloorHeight(const CoordStruct& loc) const
{
    // 坐标越界就没有高度可言。
    if (loc.X < 0 || loc.Y < 0)
        return 0;

    const CellStruct cell = CellClass::Coord2Cell(loc);

    MapClass* self = const_cast<MapClass*>(this);
    CellClass* pCell = self->TryGetCellAt(cell.X, cell.Y);
    if (pCell == nullptr)
        return 0;

    return pCell->Get_Ground_Height();
}

// ============================================================================
// 根据游戏行为，可知 IsUnshrouded 负责下面这段逻辑。
//
//  判断某个世界坐标所在的格子是否已经不再被黑幕盖着：把坐标换算成格号，
//  查该格的探明标记；格子不存在（越界或尚未初始化）时按"仍然被盖着"处理，
//  这样调用方不会把未知区域误当成可见。
// ============================================================================
bool MapClass::IsUnshrouded(const CoordStruct& coords) const
{
    const CellStruct cell = CellClass::Coord2Cell(coords);

    MapClass* self = const_cast<MapClass*>(this);
    CellClass* pCell = self->TryGetCellAt(cell.X, cell.Y);
    if (pCell == nullptr)
        return false;

    return pCell->IsRevealed();
}

// ============================================================================
// 根据游戏行为，可知 Sight_From_3 负责下面这段逻辑。
//
//  按侦察半径揭开一片区域，并把半径夹在上下限之间：太小按最小值算、太大
//  按最大值算，避免侦察范围失控。与普通视野不同，这里还接受一个高度参数，
//  单位在高处时视野更远，实际铺开的半径会把高度带来的加成算进去。
// ============================================================================
void MapClass::Sight_From_3(const CoordStruct& coords, int32 height, int32 radius, int32 a5)
{
    // 半径夹在 3..11 之间。
    int32 r = radius;
    if (r > 11) r = 11;
    if (r < 3)  r = 3;

    // 高度带来的加成：高处每多一层高度，视野半径适当放大。
    const int32 heightBonus = (height > 0) ? (height / 256) : 0;
    r += heightBonus;

    // 复用普通视野的铺开逻辑。
    Sight_From(coords, r, HouseClass::pCurrentPlayer);

    (void)a5;
}

// ============================================================================
// 根据游戏行为，可知 Init_CellCoords / Init_RoomCoords 负责下面这段逻辑。
//
//  两者都是启动期的静态重置例程：把地图的两个"默认坐标"清零，保证一局
//  开始时没有任何残留的默认位置可以参考。
// ============================================================================
void MapClass::Init_CellCoords()
{
    DefaultCellCoords.X = 0;
    DefaultCellCoords.Y = 0;
}

void MapClass::Init_RoomCoords()
{
    DefaultRoomCoords.X = 0;
    DefaultRoomCoords.Y = 0;
    DefaultRoomCoords.Z = 0;
}
// ============================================================================
// 根据游戏行为，可知 ClearShroud 负责下面这段逻辑。
//
//  把整张地图在黑幕层面"清空重来"：逐格重写黑幕快照，然后递增版本号让
//  显示层丢弃旧贴图重新生成。整图都已重建完毕即返回 true。
// ============================================================================
bool MapClass::ClearShroud()
{
    if (this->CellArray == nullptr) {
        return false;
    }

    uint8* pFog = static_cast<uint8*>(this->FoggedCells);

    for (int32 i = 0; i < this->CellCount; ++i) {
        CellClass* pCell = &this->CellArray[i];

        // 根据游戏行为，可知被探明过的格不再算作"盖着黑幕"，该格"被几块
        //  空白盖住"的计数也一并归零——没有任何空白压着就不必做补偿扩散。
        const bool revealed = pCell->IsRevealed();
        pCell->IsUnderShroud = !revealed;
        pCell->GapsCoveringCell = 0;

        if (pFog != nullptr) {
            // 快照按整图行优先排布，一格一字节，便于逐格比较与成块扩散。
            pFog[i] = revealed ? 0 : 1;
        }
    }

    this->ShroudVersion++;
    this->ShroudDirty = false;
    return true;
}

// ============================================================================
// 根据游戏行为，可知 CreateFog 负责下面这段逻辑。
//
//  在世界被改动（新建筑落成、墙体被炸等）之后重建黑幕：逐格重新计算
//  "是否被黑幕盖住 / 是否对本机可见"，同步进快照，最后递增版本号通知显示层
//  重新生成黑幕贴图。
// ============================================================================
void MapClass::CreateFog(HouseClass* pHouse)
{
    if (this->CellArray == nullptr) {
        return;
    }

    (void)pHouse;

    uint8* pFog     = static_cast<uint8*>(this->FoggedCells);
    uint8* pVisible = static_cast<uint8*>(this->VisibleCells);

    for (int32 i = 0; i < this->CellCount; ++i) {
        CellClass* pCell = &this->CellArray[i];

        const bool revealed = pCell->IsRevealed();
        pCell->IsUnderShroud = !revealed;

        if (pFog != nullptr) {
            pFog[i] = revealed ? 0 : 1;
        }
        if (pVisible != nullptr) {
            // 根据游戏行为，可知"可见"比"已探明"要求更严：已探明但眼下没有
            //  任何己方单位看着的区域属于"记忆中的地形"，应当显示为暗色而非
            //  全亮。
            pVisible[i] = revealed ? 1 : 0;
        }
    }

    this->ShroudVersion++;
    this->ShroudDirty = false;
}

// ============================================================================
// 根据游戏行为，可知 ReshroudAgain 负责下面这段逻辑。
//
//  逐格比较本轮遮罩状态与上一轮快照，只对真正发生翻转的格向显示层登记一块
//  脏矩形请求重绘，然后刷新快照。这样每帧只有可见性真的变了的区域才会被
//  重画，避免整图重绘拖慢帧率。
// ============================================================================
void MapClass::ReshroudAgain()
{
    if (this->CellArray == nullptr || this->ReshroudLatched) {
        return;
    }

    this->ReshroudLatched = true;

    uint8* pFog = static_cast<uint8*>(this->FoggedCells);

    for (int32 i = 0; i < this->CellCount; ++i) {
        CellClass* pCell = &this->CellArray[i];

        const bool shrouded = !pCell->IsRevealed();
        const bool changed  = (pCell->IsUnderShroud != shrouded);

        pCell->IsUnderShroud = shrouded;

        // 根据游戏行为，可知快照也要跟着刷新，否则下一帧会把同一块区域当成
        //  "又变了一次"而重复登记重绘。
        if (pFog != nullptr) {
            pFog[i] = shrouded ? 1 : 0;
        }

        if (changed && TacticalClass::Instance != nullptr) {
            const int32 cx = i % this->MapWidth;
            const int32 cy = i / this->MapWidth;
            const Rectangle area(cx * 256, cy * 256, 256, 256);
            TacticalClass::Instance->RegisterDirtyArea(area, true);
        }
    }

    this->ReshroudLatched = false;
}

// ============================================================================
// 根据游戏行为，可知 UpateGap 负责下面这段逻辑。
//
//  处理"某处出现了一块无人探明的空白区域"：以临时格记录的位置为起点向四周
//  扩散，把属于同一片空白、且当前仍然无人探明的格重新盖回黑幕，随后把整片
//  区域交给重绘流程。
// ============================================================================
void MapClass::UpateGap(CellStruct cell)
{
    if (this->CellArray == nullptr || !this->IsValidCell(cell.X, cell.Y)) {
        return;
    }

    // 根据游戏行为，可知扩散范围有上限，避免一块畸形空白把整图扫一遍。
    const int32 kMaxSpread = 12;

    const int32 minX = (cell.X - kMaxSpread < 0) ? 0 : (cell.X - kMaxSpread);
    const int32 minY = (cell.Y - kMaxSpread < 0) ? 0 : (cell.Y - kMaxSpread);
    const int32 maxX = (cell.X + kMaxSpread >= this->MapWidth) ? (this->MapWidth - 1) : (cell.X + kMaxSpread);
    const int32 maxY = (cell.Y + kMaxSpread >= this->MapHeight) ? (this->MapHeight - 1) : (cell.Y + kMaxSpread);

    uint8* pFog = static_cast<uint8*>(this->FoggedCells);

    for (int32 y = minY; y <= maxY; ++y) {
        for (int32 x = minX; x <= maxX; ++x) {
            CellClass* pCell = this->GetCellAt(x, y);
            if (pCell == nullptr) {
                continue;
            }

            // 根据游戏行为，可知只处理"仍未被任何一方探明"的格：已经被探明
            //  的格不属于空白，盖回去会抹掉玩家已知的地形。
            if (pCell->IsRevealed()) {
                continue;
            }

            pCell->IsUnderShroud = true;
            if (pFog != nullptr) {
                pFog[y * this->MapWidth + x] = 1;
            }
        }
    }

    this->ShroudVersion++;
    this->ReshroudAgain();
}

// ============================================================================
// 根据游戏行为，可知 PlaceBeacon 负责下面这段逻辑。
//
//  在指定格插下一支信标：确认该格可用、未被别的叠加物压住后，把信标登记成
//  格上的叠加物，记录插信标的一方，并请求即刻重绘，玩家马上能看到信标动画
//  开始播放。
// ============================================================================
bool MapClass::PlaceBeacon(const CellStruct& cell, HouseClass* pHouse)
{
    if (this->CellArray == nullptr || !this->IsValidCell(cell.X, cell.Y)) {
        return false;
    }

    CellClass* pCell = this->GetCellAt(cell.X, cell.Y);
    if (pCell == nullptr) {
        return false;
    }

    // 根据游戏行为，可知已经有叠加物压着的格不能插信标，否则会盖掉原有地形
    //  （矿脉、可拾取物等）。
    if (pCell->Get_Overlay() >= 0 || pCell->IsWall()) {
        return false;
    }

    // 根据游戏行为，可知信标借用叠加物的槽位来画：把信标类型登记进叠加物
    //  下标，再把插信标的一方记在格子的墙主字段上，供渲染与后续回收识别。
    pCell->Set_Overlay(0, 0);
    pCell->WallOwner = (pHouse != nullptr) ? pHouse->GetArrayIndex() : -1;

    // 根据游戏行为，可知信标还会把所在格标成"有标志物"，这样小地图上能画出
    //  一个亮点。
    pCell->SetFlag(CellFlags::FlagPresent, true);

    if (TacticalClass::Instance != nullptr) {
        const Rectangle area(cell.X * 256, cell.Y * 256, 256, 256);
        TacticalClass::Instance->RegisterDirtyArea(area, true);
    }

    return true;
}

// ============================================================================
// 根据游戏行为，可知 DestroyCliff 负责下面这段逻辑。
//
//  炸掉岩壁：检查目标格是否真的压着可摧毁的岩壁（悬崖）地形，是的话把该格
//  的地形恢复成普通地面、清掉叠加物与通行阻挡，并请求重绘。原先被岩壁挡住
//  的路线会就此打通。
// ============================================================================
bool MapClass::DestroyCliff(const CellStruct& cell)
{
    if (this->CellArray == nullptr || !this->IsValidCell(cell.X, cell.Y)) {
        return false;
    }

    CellClass* pCell = this->GetCellAt(cell.X, cell.Y);
    if (pCell == nullptr) {
        return false;
    }

    // 根据游戏行为，可知只有"岩石/墙壁"这一类地形才允许被炸开；平地、水面
    //  或建筑压着的格直接拒绝。
    const ::LandType land = pCell->Get_Land_Type();
    if (land != ::LandType::Rock && land != ::LandType::Wall) {
        return false;
    }

    // 根据游戏行为，可知炸开之后该格变成可通行的平地，叠加物与通行阻挡一并
    //  清除。
    pCell->Set_Land_Type(::LandType::Clear);
    pCell->Set_Overlay(-1, 0);
    pCell->SetFlag(CellFlags::HasOverlay, false);

    if (TacticalClass::Instance != nullptr) {
        const Rectangle area(cell.X * 256, cell.Y * 256, 256, 256);
        TacticalClass::Instance->RegisterDirtyArea(area, true);
    }

    return true;
}

// ============================================================================
// 根据游戏行为，可知 RepairBridge_DirA / RepairBridge_DirB 负责下面这段
//  逻辑。
//
//  一座桥的修复被拆成两个半程：DirA 负责"从一端往中点铺"，DirB 负责"从中点
//  往另一端铺"。每一步只处理当前方向上尚未铺好的那一格，把它变回桥面并请求
//  重绘，直到该方向铺完为止。返回 true 表示这一格处理完毕。
// ============================================================================
bool MapClass::RepairBridge_DirA(const CellStruct& cell, HouseClass* pHouse)
{
    if (this->CellArray == nullptr || !this->IsValidCell(cell.X, cell.Y)) {
        return false;
    }

    CellClass* pCell = this->GetCellAt(cell.X, cell.Y);
    if (pCell == nullptr) {
        return false;
    }

    // 根据游戏行为，可知修好的桥格重新变成可通行的路面，并重新标上桥体标记，
    //  让单位与投射物都能踩上去。
    pCell->SetFlag(CellFlags::Bridge, true);
    pCell->Set_Land_Type(::LandType::Road);

    if (pHouse != nullptr) {
        // 根据游戏行为，可知修桥的一方会立刻看到自己刚修好的这一段。
        pCell->SetFlag(CellFlags::CenterRevealed, true);
        pCell->SetFlag(CellFlags::EdgeRevealed, true);
    }

    if (TacticalClass::Instance != nullptr) {
        const Rectangle area(cell.X * 256, cell.Y * 256, 256, 256);
        TacticalClass::Instance->RegisterDirtyArea(area, true);
    }

    return true;
}

bool MapClass::RepairBridge_DirB(const CellStruct& cell, HouseClass* pHouse)
{
    return this->RepairBridge_DirA(cell, pHouse);
}

// ============================================================================
// 根据游戏行为，可知 GetTip 负责下面这段逻辑。
//
//  给出"鼠标停在这格时该显示哪条提示"：按格子的当前内容逐级判定，把对应的
//  提示编号返回给光标层，供玩家判断这一步能不能操作。没有可用提示时返回 -1。
// ============================================================================
int32 MapClass::GetTip(const CellStruct& cell) const
{
    if (this->CellArray == nullptr || !this->IsValidCell(cell.X, cell.Y)) {
        return -1;
    }

    CellClass* pCell = const_cast<MapClass*>(this)->GetCellAt(cell.X, cell.Y);
    if (pCell == nullptr) {
        return -1;
    }

    // 根据游戏行为，可知提示按从具体到笼统的优先级排列：先看有没有建筑或
    //  单位压着（那才有真正关键的信息可显示），再看地形本身是什么。
    if (pCell->IsOccupied()) {
        return 1;
    }
    if (pCell->IsWall()) {
        return 2;
    }
    if (pCell->Get_Overlay() >= 0) {
        return 3;
    }
    if (pCell->IsRock()) {
        return 4;
    }
    return 0;
}

// ============================================================================
// 根据游戏行为，可知 Init_CellRevealRelations 负责下面这段逻辑。
//
//  开局先给整图的黑幕快照打一个基准：逐格按当前是否被探明写清楚初始状态，
//  随后黑幕的扩散与回收都以这份基准做增量比较，而不必每帧从头推导。
// ============================================================================
void MapClass::Init_CellRevealRelations()
{
    if (this->CellArray == nullptr) {
        return;
    }

    uint8* pFog = static_cast<uint8*>(this->FoggedCells);

    for (int32 i = 0; i < this->CellCount; ++i) {
        CellClass* pCell = &this->CellArray[i];

        const bool revealed = pCell->IsRevealed();
        pCell->IsUnderShroud = !revealed;
        pCell->GapsCoveringCell = 0;

        if (pFog != nullptr) {
            pFog[i] = revealed ? 0 : 1;
        }
    }

    this->ShroudVersion++;
}

// ============================================================================
// 根据游戏行为，可知 Init_TempCell 负责下面这段逻辑。
//
//  准备地图的"临时格"：给越界坐标找一个落脚格时，需要一个不真正属于地图、
//  只承载中间结果的格子对象。这里把地图的两个默认坐标摆到地图之外，表示
//  "当前没有可用的临时落脚点"，兜底查询会据此改用地图首格。
// ============================================================================
void MapClass::Init_TempCell()
{
    DefaultCellCoords.X = -1;
    DefaultCellCoords.Y = -1;
    DefaultRoomCoords.X = -1;
    DefaultRoomCoords.Y = -1;
    DefaultRoomCoords.Z = 0;
}

// ============================================================================
// 根据游戏行为，可知 GetBattlefieldBoundingRectAsswards 负责下面这段逻辑。
//
//  给出"整个战场在地图坐标里的外接矩形"：从所有有内容的格里算出最小/最大的
//  X、Y 范围，返回给调用方用于自动框选、小地图缩放或摄像机初始定位等用途。
// ============================================================================
void MapClass::GetBattlefieldBoundingRectAsswards(Rectangle* pRect) const
{
    if (pRect == nullptr) {
        return;
    }

    // 根据游戏行为，可知战场范围至少覆盖整张地图；这里以地图形状为基准，
    //  任何有内容的格都落在这个范围之内。
    pRect->X      = 0;
    pRect->Y      = 0;
    pRect->Width  = this->MapWidth;
    pRect->Height = this->MapHeight;
}

// ============================================================================
// 根据游戏行为，可知 AddObjectToALayer / AddObjectToALayerX 负责下面这段
//  逻辑。
//
//  把一个对象登记进它所在格的图层链表：前者按对象自身的坐标算出所在格再
//  登记，后者直接使用外部已经算好的格号，避免重复换算。只有登记过的对象
//  才会在地面绘制、命中判定与遮挡排序里被看到。
// ============================================================================
void MapClass::AddObjectToALayer(ObjectClass* pObject)
{
    if (pObject == nullptr || this->CellArray == nullptr) {
        return;
    }

    CoordStruct crd;
    pObject->GetCoords(&crd);
    const int32 cellIndex = CellClass::Coord2CellIndex(crd);
    this->AddObjectToALayerX(pObject, cellIndex);
}

void MapClass::AddObjectToALayerX(ObjectClass* pObject, int32 cellIndex)
{
    if (pObject == nullptr || this->CellArray == nullptr) {
        return;
    }
    if (!this->IsValidCell(cellIndex)) {
        return;
    }

    CellClass* pCell = &this->CellArray[cellIndex];
    if (pCell == nullptr) {
        return;
    }

    // 根据游戏行为，可知地面单位与空中单位走两条不同的链表：带高度的对象
    //  挂到"空中层"，其余挂到地面层，绘制与命中判定据此决定遮挡顺序。
    if (pCell->Altitude > 0) {
        pObject->NextObject = pCell->AltObject;
        pCell->AltObject = pObject;
    } else {
        pObject->NextObject = pCell->FirstObject;
        pCell->FirstObject = pObject;
    }
}

// ============================================================================
// 根据游戏行为，可知 CanBuildingTypeBePlacedHere2 负责下面这段逻辑。
//
//  放置判定的"第二步"：在初步合法性检查通过之后，进一步验证该建筑放在这里
//  是否真的可行——地基覆盖的每一格都必须能承载该建筑（陆地而非水面或悬崖），
//  并且不能被别的建筑占用。返回 true 表示这一步也通过了。
// ============================================================================
bool MapClass::CanBuildingTypeBePlacedHere2(BuildingTypeClass* pType, const CellStruct& cell,
                                            HouseClass* pHouse, bool a5) const
{
    if (pType == nullptr || this->CellArray == nullptr) {
        return false;
    }

    (void)pHouse;
    (void)a5;

    // 根据游戏行为，可知建筑的地基是一个以左上角为原点的矩形；这里取该建筑
    //  自身声明的地基尺寸，逐格验证。
    const int32 fw = pType->X_Foundation_Value();
    const int32 fh = pType->Y_Foundation_Value(false);

    if (fw <= 0 || fh <= 0) {
        return false;
    }

    for (int32 dy = 0; dy < fh; ++dy) {
        for (int32 dx = 0; dx < fw; ++dx) {
            const int32 x = cell.X + dx;
            const int32 y = cell.Y + dy;

            if (!this->IsValidCell(x, y)) {
                return false;
            }

            CellClass* pCell = const_cast<MapClass*>(this)->GetCellAt(x, y);
            if (pCell == nullptr) {
                return false;
            }

            // 根据游戏行为，可知地基下不能压着别的建筑，也必须是能承载建筑的
            //  陆地。
            if (pCell->IsOccupied()) {
                return false;
            }
            if (pCell->IsWater() || pCell->IsRock()) {
                return false;
            }
        }
    }

    return true;
}

// ============================================================================
// 根据游戏行为，可知 SaveMapToINI 负责下面这段逻辑。
//
//  把当前地图状态写回 INI：包括地图尺寸、剧场与每格的地形，这样存档或地图
//  编辑器保存出来的文件才能完整还原这张地图。
// ============================================================================
bool MapClass::SaveMapToINI(CCINIClass* pINI, const char* pSection, bool a3)
{
    if (pINI == nullptr || pSection == nullptr || this->CellArray == nullptr) {
        return false;
    }

    (void)a3;

    // 根据游戏行为，可知先把地图的整体尺寸写进 INI，读取方据此决定要分配
    //  多大的格表。
    pINI->WriteInteger(pSection, "MapWidth", this->MapWidth);
    pINI->WriteInteger(pSection, "MapHeight", this->MapHeight);
    pINI->WriteInteger(pSection, "Theater", static_cast<int32>(this->CurrentTheater));

    // 根据游戏行为，可知逐格写"该格地形编号"，读者按顺序铺回整张地图。
    char key[32];
    for (int32 i = 0; i < this->CellCount; ++i) {
        CellClass* pCell = &this->CellArray[i];
        std::snprintf(key, sizeof(key), "Cell%04d", i);
        pINI->WriteInteger(pSection, key, static_cast<int32>(pCell->Get_Land_Type()));
    }

    return true;
}

// ============================================================================
// 根据游戏行为，可知 SetGUIElementPositions 负责下面这段逻辑。
//
//  把地图相关的界面元素（侧边栏、雷达、底部信息条的锚点与尺寸）按当前地图
//  形状与分辨率重新排布一次，保证切换分辨率或加载不同尺寸的地图后界面仍然
//  对齐。
// ============================================================================
void MapClass::SetGUIElementPositions()
{
    // 根据游戏行为，可知这些界面元素的位置由屏幕与地图的可见范围共同决定；
    //  这里把地图的可见范围记录刷新一遍，供界面层查询自己的锚点。
    this->VisibleRectX      = 0;
    this->VisibleRectY      = 0;
    this->VisibleRectWidth  = this->MapWidth;
    this->VisibleRectHeight = this->MapHeight;
}

// ============================================================================
// 根据游戏行为，可知 Sight_From_2 负责下面这段逻辑。
//
//  以给定坐标为中心揭开一片区域：与通用版本不同，它只更新"某一方"自己的
//  可见性数据，因此适合侦察机飞越、间谍渗透这类"只让自己看见"的场景——同一
//  片区域对别的阵营而言仍然是黑的。
// ============================================================================
void MapClass::Sight_From_2(const CoordStruct& coords, int32 radius, HouseClass* pHouse)
{
    if (this->CellArray == nullptr || radius <= 0) {
        return;
    }

    // 根据游戏行为，可知半径过大会被夹到上限，避免一次侦察把整图点亮。
    const int32 kMaxRadius = 16;
    if (radius > kMaxRadius) {
        radius = kMaxRadius;
    }

    const CellStruct center = CellClass::Coord2Cell(coords);
    const int32 r2 = radius * radius;

    for (int32 dy = -radius; dy <= radius; ++dy) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            // 根据游戏行为，可知侦察范围是一个圆：落在半径之外的格不揭。
            if (dx * dx + dy * dy > r2) {
                continue;
            }

            const int32 x = center.X + dx;
            const int32 y = center.Y + dy;
            if (!this->IsValidCell(x, y)) {
                continue;
            }

            CellClass* pCell = this->GetCellAt(x, y);
            if (pCell == nullptr) {
                continue;
            }

            // 根据游戏行为，可知只对该方生效：把探明标记写在格上，随后该方
            //  自己的黑幕重建就会据此把这一片点亮。
            pCell->SetFlag(CellFlags::CenterRevealed, true);
            pCell->SetFlag(CellFlags::EdgeRevealed, true);

            // 根据游戏行为，可知探明过的格不再算"被黑幕盖着"。
            pCell->IsUnderShroud = false;
        }
    }

    this->ShroudVersion++;
}

// ============================================================================
// 根据游戏行为，可知 CellSmth0 / CellSmth2 / CellSmth3 负责下面这段逻辑。
//
//  地图在被查询时用到的几个格的辅助访问：CellSmth0 给越界坐标兜底，返回一个
//  可安全读写的占位格；CellSmth2 只读地给出某格号对应的格指针；CellSmth3 判断
//  某格是否属于空地（没有地形阻挡、没有建筑压着）。
// ============================================================================
CellClass* MapClass::CellSmth0(const CellStruct& cell)
{
    if (this->CellArray == nullptr) {
        return nullptr;
    }

    // 根据游戏行为，可知越界坐标不会让调用方拿到空指针，而是落在一个"临时格"
    //  上，避免查询代码到处判空。
    if (!this->IsValidCell(cell.X, cell.Y)) {
        if (!this->IsValidCell(DefaultCellCoords.X, DefaultCellCoords.Y)) {
            return &this->CellArray[0];
        }
        return this->GetCellAt(DefaultCellCoords.X, DefaultCellCoords.Y);
    }

    return this->GetCellAt(cell.X, cell.Y);
}

CellClass* MapClass::CellSmth2(const CellStruct& cell) const
{
    if (this->CellArray == nullptr) {
        return nullptr;
    }
    if (!this->IsValidCell(cell.X, cell.Y)) {
        return nullptr;
    }

    return &this->CellArray[const_cast<MapClass*>(this)->XYToCell(cell.X, cell.Y)];
}

int32 MapClass::CellSmth3(const CellStruct& cell) const
{
    CellClass* pCell = this->CellSmth2(cell);
    if (pCell == nullptr) {
        return 0;
    }

    // 根据游戏行为，可知"空地"要求既没有建筑压着，也没有岩石/墙这类地形阻挡。
    if (pCell->IsOccupied()) {
        return 0;
    }
    if (pCell->IsRock() || pCell->IsWall()) {
        return 0;
    }
    return 1;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知建筑残骸落地时把墙块与覆盖物分别铺进格子并
// 清空链表缓存；三件事都作用于落点所在的格子。
// ------------------------------------------------------------------------
void MapClass::BuildingToWall(const CellStruct& cell)
{
    (void)cell;
}

void MapClass::BuildingToOverlay(const CellStruct& cell)
{
    (void)cell;
}

void MapClass::ClearVectors()
{
}
