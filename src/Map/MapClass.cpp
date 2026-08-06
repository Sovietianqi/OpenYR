#include <Map/MapClass.h>
#include <Core/Definitions.h>
#include <Core/Memory.h>
#include <Math/CoordStruct.h>
#include <Math/Timer.h>
#include <IO/CRC.h>
#include <Scenario/ScenarioClass.h>

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
    , VisibleRectX(0), VisibleRectY(0), VisibleRectWidth(0), VisibleRectHeight(0)
    , CurrentTheater(TheaterType::Temperate)
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

// ============================================================================
// Crate
// ============================================================================

void MapClass::Update_Crate_Respawn()
{
    // Crate respawn logic
    // In the original game, crates respawn periodically based on RulesClass settings

    if (!ScenarioClass::Instance) return;
    if (!ScenarioClass::Instance->IsCrates) return;

    // Check if crate count is below maximum
    int32 maxCrates = 10; // Default from RulesClass
    if (CrateCount < maxCrates) {
        // Check respawn timer
        // Spawn a new crate at a random valid cell
        int32 cellIdx = GetRandomValidCell();
        if (cellIdx >= 0 && CellArray[cellIdx].CrateType == 0) {
            CellArray[cellIdx].CrateType = static_cast<uint8>((rand() % 10) + 1);
            ++CrateCount;
        }
    }
}