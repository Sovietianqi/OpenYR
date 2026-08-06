#include <Objects/HeightMap.h>

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <COM/IUnknown.h>
#include <IO/CRC.h>

#include <cstring>
#include <cmath>

// ============================================================================
// HeightMap.cpp - Height map implementation
//
//  Implements the HeightMap class declared in HeightMap.h.  The height map
//  is a flat 2D grid of int8 values, one per cell, that stores the
//  elevation level (0..14) for every cell on the map.
//
//  In addition to the core get/set/interpolate/coast-detect logic, this file
//  provides a suite of file-local helper functions that perform advanced
//  terrain analysis (slope direction, surface normals, cliff detection),
//  batch region operations (raise/lower/flatten/smooth), erosion simulation,
//  water-level handling, and statistical queries (histograms, min/max).  The
//  helpers are free functions because HeightMap.h is the source of truth and
//  cannot be extended with new member declarations.
// ============================================================================

// ============================================================================
// Construction / Destruction
// ============================================================================

HeightMap::HeightMap() noexcept
    : Data(nullptr)
    , Width(0)
    , Height(0)
{
}

HeightMap::~HeightMap()
{
    Release();
}

// ============================================================================
// Allocation
// ============================================================================

bool HeightMap::Allocate(int32 width, int32 height) noexcept
{
    if (width <= 0 || height <= 0)
        return false;
    if (width > MaxMapWidth || height > MaxMapHeight)
        return false;

    Release();

    int32 total = width * height;
    Data = static_cast<int8*>(YRMemory::Allocate(sizeof(int8) * total));
    if (!Data)
        return false;

    Width = width;
    Height = height;

    // Initialize all heights to 0 (sea level).
    std::memset(Data, 0, sizeof(int8) * total);

    return true;
}

void HeightMap::Release() noexcept
{
    if (Data)
    {
        YRMemory::Deallocate(Data);
        Data = nullptr;
    }
    Width = 0;
    Height = 0;
}

// ============================================================================
// Bounds checking
// ============================================================================

bool HeightMap::IsValidCell(int32 x, int32 y) const noexcept
{
    if (!Data) return false;
    if (x < 0 || x >= Width) return false;
    if (y < 0 || y >= Height) return false;
    return true;
}

// ============================================================================
// Height queries
// ============================================================================

int32 HeightMap::GetHeight(int32 x, int32 y) const noexcept
{
    if (!IsValidCell(x, y))
        return 0;
    return static_cast<int32>(Data[Index(x, y)]);
}

// ============================================================================
// Height mutators
// ============================================================================

void HeightMap::SetHeight(int32 x, int32 y, int32 height) noexcept
{
    if (!IsValidCell(x, y))
        return;
    if (height < 0) height = 0;
    if (height > MaxHeightLevels) height = MaxHeightLevels;
    Data[Index(x, y)] = static_cast<int8>(height);
}

void HeightMap::Raise(int32 x, int32 y) noexcept
{
    SetHeight(x, y, GetHeight(x, y) + 1);
}

void HeightMap::Lower(int32 x, int32 y) noexcept
{
    SetHeight(x, y, GetHeight(x, y) - 1);
}

// ============================================================================
// Interpolation
//
//  Bilinearly interpolates the height between four neighboring cells.
//  This is used by the renderer to draw smooth slopes and by the pathfinder
//  to compute the Z coordinate of a unit at sub-cell positions.
//
//  fx, fy are in [0.0, 1.0) and represent the fractional position within
//  the cell (x, y).  The four samples are taken at (x, y), (x+1, y),
//  (x, y+1) and (x+1, y+1).
// ============================================================================

float HeightMap::GetInterpolatedHeight(int32 x, int32 y,
                                       float fx, float fy) const noexcept
{
    // Clamp the fractional coordinates to [0, 1].
    if (fx < 0.0f) fx = 0.0f;
    if (fx > 1.0f) fx = 1.0f;
    if (fy < 0.0f) fy = 0.0f;
    if (fy > 1.0f) fy = 1.0f;

    // Sample the four corners.
    float h00 = static_cast<float>(GetHeight(x, y));
    float h10 = static_cast<float>(GetHeight(x + 1, y));
    float h01 = static_cast<float>(GetHeight(x, y + 1));
    float h11 = static_cast<float>(GetHeight(x + 1, y + 1));

    // Bilinear interpolation.
    float h0 = h00 + (h10 - h00) * fx;      // Interpolate along X at y
    float h1 = h01 + (h11 - h01) * fx;      // Interpolate along X at y+1
    return h0 + (h1 - h0) * fy;             // Interpolate along Y
}

// ============================================================================
// Terrain analysis
// ============================================================================

int32 HeightMap::GetMaxNeighborDelta(int32 x, int32 y) const noexcept
{
    int32 center = GetHeight(x, y);
    int32 maxDelta = 0;

    // Check all four orthogonal neighbors.
    int32 neighbors[4];
    neighbors[0] = GetHeight(x - 1, y);
    neighbors[1] = GetHeight(x + 1, y);
    neighbors[2] = GetHeight(x, y - 1);
    neighbors[3] = GetHeight(x, y + 1);

    for (int32 i = 0; i < 4; ++i)
    {
        int32 delta = neighbors[i] - center;
        if (delta < 0) delta = -delta;
        if (delta > maxDelta) maxDelta = delta;
    }

    return maxDelta;
}

bool HeightMap::IsCoastCell(int32 x, int32 y) const noexcept
{
    int32 center = GetHeight(x, y);
    if (center <= 0)
        return false;  // Water or sea-level tile is not a coast.

    // A coast cell is a land tile adjacent to a water (height 0) tile.
    if (GetHeight(x - 1, y) <= 0) return true;
    if (GetHeight(x + 1, y) <= 0) return true;
    if (GetHeight(x, y - 1) <= 0) return true;
    if (GetHeight(x, y + 1) <= 0) return true;

    return false;
}

bool HeightMap::IsFlatCell(int32 x, int32 y) const noexcept
{
    int32 center = GetHeight(x, y);
    if (GetHeight(x - 1, y) != center) return false;
    if (GetHeight(x + 1, y) != center) return false;
    if (GetHeight(x, y - 1) != center) return false;
    if (GetHeight(x, y + 1) != center) return false;
    return true;
}

// ============================================================================
// Bulk operations
// ============================================================================

void HeightMap::Clear(int32 height) noexcept
{
    if (!Data) return;
    if (height < 0) height = 0;
    if (height > MaxHeightLevels) height = MaxHeightLevels;
    int8 val = static_cast<int8>(height);
    std::memset(Data, val, sizeof(int8) * Width * Height);
}

bool HeightMap::CopyFrom(const HeightMap& other) noexcept
{
    if (Width != other.Width || Height != other.Height)
        return false;
    if (!Data || !other.Data)
        return false;
    std::memcpy(Data, other.Data, sizeof(int8) * Width * Height);
    return true;
}

// ============================================================================
// CRC computation
//
//  Feeds the entire height grid into the CRC engine.  This is part of the
//  multiplayer sync check - if two players have different height maps, the
//  CRC will differ and the game will flag a desync.
// ============================================================================

void HeightMap::ComputeCRC(CRCEngine& crc) const
{
    crc.AddData(&Width, sizeof(Width));
    crc.AddData(&Height, sizeof(Height));
    if (Data && Width > 0 && Height > 0)
    {
        crc.AddData(Data, sizeof(int8) * Width * Height);
    }
}

// ============================================================================
// Serialization
//
//  The save format stores the dimensions followed by the raw height data.
// ============================================================================

bool HeightMap::Save(IStream* pStm) const
{
    if (!pStm) return false;
    if (!Data) return false;

    uint32 written = 0;

    // Write dimensions.
    HRESULT hr = pStm->Write(&Width, sizeof(Width), &written);
    if (hr < 0 || written != sizeof(Width)) return false;

    hr = pStm->Write(&Height, sizeof(Height), &written);
    if (hr < 0 || written != sizeof(Height)) return false;

    // Write height data.
    int32 total = Width * Height;
    hr = pStm->Write(Data, sizeof(int8) * total, &written);
    if (hr < 0 || static_cast<int32>(written) != sizeof(int8) * total)
        return false;

    return true;
}

bool HeightMap::Load(IStream* pStm)
{
    if (!pStm) return false;

    Release();

    uint32 read = 0;

    // Read dimensions.
    HRESULT hr = pStm->Read(&Width, sizeof(Width), &read);
    if (hr < 0 || read != sizeof(Width)) return false;

    hr = pStm->Read(&Height, sizeof(Height), &read);
    if (hr < 0 || read != sizeof(Height)) return false;

    // Validate dimensions.
    if (Width <= 0 || Height <= 0 ||
        Width > MaxMapWidth || Height > MaxMapHeight)
    {
        Width = 0;
        Height = 0;
        return false;
    }

    // Allocate and read height data.
    int32 total = Width * Height;
    Data = static_cast<int8*>(YRMemory::Allocate(sizeof(int8) * total));
    if (!Data)
    {
        Width = 0;
        Height = 0;
        return false;
    }

    hr = pStm->Read(Data, sizeof(int8) * total, &read);
    if (hr < 0 || static_cast<int32>(read) != sizeof(int8) * total)
    {
        YRMemory::Deallocate(Data);
        Data = nullptr;
        Width = 0;
        Height = 0;
        return false;
    }

    return true;
}

// ============================================================================
// File-local helper functions
//
//  These implement advanced terrain analysis, batch region operations,
//  erosion simulation, water-level handling, and statistical queries that
//  the original engine performs on the height grid.  They are free functions
//  operating on a HeightMap* because HeightMap.h cannot be extended with new
//  member declarations.
// ============================================================================

namespace
{

// --------------------------------------------------------------------------
// ClampToMap - clamps a coordinate to the valid grid range
// --------------------------------------------------------------------------
void ClampToMap(const HeightMap& map, int32& x, int32& y) noexcept
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= map.GetWidth())  x = map.GetWidth()  - 1;
    if (y >= map.GetHeight()) y = map.GetHeight() - 1;
}

// --------------------------------------------------------------------------
// GetAverageHeightInRegion
//
//  Returns the arithmetic mean of all height values inside the rectangular
//  region [minX, maxX] x [minY, maxY].  Out-of-range cells are clamped to
//  the grid boundary, mirroring how the renderer samples edge tiles.
// --------------------------------------------------------------------------
float GetAverageHeightInRegion(const HeightMap& map,
                               int32 minX, int32 minY,
                               int32 maxX, int32 maxY) noexcept
{
    if (!map.IsValid())
        return 0.0f;

    ClampToMap(map, minX, minY);
    ClampToMap(map, maxX, maxY);
    if (minX > maxX) { int32 t = minX; minX = maxX; maxX = t; }
    if (minY > maxY) { int32 t = minY; minY = maxY; maxY = t; }

    int64 sum = 0;
    int32 count = 0;
    for (int32 y = minY; y <= maxY; ++y)
    {
        for (int32 x = minX; x <= maxX; ++x)
        {
            sum += map.GetHeight(x, y);
            ++count;
        }
    }
    if (count == 0)
        return 0.0f;
    return static_cast<float>(sum) / static_cast<float>(count);
}

// --------------------------------------------------------------------------
// GetMinMaxHeight
//
//  Scans the entire grid and returns the minimum and maximum height levels
//  present.  Used by the map editor to scale the elevation palette and by
//  the water-fill routine to determine the flood threshold.
// --------------------------------------------------------------------------
void GetMinMaxHeight(const HeightMap& map, int32& outMin, int32& outMax) noexcept
{
    outMin = HeightMap::MaxHeightLevels;
    outMax = 0;
    if (!map.IsValid())
    {
        outMin = 0;
        return;
    }
    for (int32 y = 0; y < map.GetHeight(); ++y)
    {
        for (int32 x = 0; x < map.GetWidth(); ++x)
        {
            int32 h = map.GetHeight(x, y);
            if (h < outMin) outMin = h;
            if (h > outMax) outMax = h;
        }
    }
}

// --------------------------------------------------------------------------
// GetHeightHistogram
//
//  Fills the supplied array (which must have at least
//  HeightMap::MaxHeightLevels + 1 entries) with the number of cells at each
//  elevation level.  Returns the total number of cells counted.
// --------------------------------------------------------------------------
int32 GetHeightHistogram(const HeightMap& map, int32* pHistogram) noexcept
{
    if (!map.IsValid() || !pHistogram)
        return 0;

    for (int32 i = 0; i <= HeightMap::MaxHeightLevels; ++i)
        pHistogram[i] = 0;

    int32 total = 0;
    for (int32 y = 0; y < map.GetHeight(); ++y)
    {
        for (int32 x = 0; x < map.GetWidth(); ++x)
        {
            int32 h = map.GetHeight(x, y);
            if (h < 0) h = 0;
            if (h > HeightMap::MaxHeightLevels) h = HeightMap::MaxHeightLevels;
            ++pHistogram[h];
            ++total;
        }
    }
    return total;
}

// --------------------------------------------------------------------------
// CountCellsAtLevel
//
//  Returns the number of cells whose height exactly matches the supplied
//  level.  Used by the water renderer to count submerged tiles.
// --------------------------------------------------------------------------
int32 CountCellsAtLevel(const HeightMap& map, int32 level) noexcept
{
    if (!map.IsValid())
        return 0;
    int32 count = 0;
    for (int32 y = 0; y < map.GetHeight(); ++y)
    {
        for (int32 x = 0; x < map.GetWidth(); ++x)
        {
            if (map.GetHeight(x, y) == level)
                ++count;
        }
    }
    return count;
}

// --------------------------------------------------------------------------
// CountWaterCells
//
//  Counts all cells at or below sea level (height 0).  These are the cells
//  the renderer treats as water.
// --------------------------------------------------------------------------
int32 CountWaterCells(const HeightMap& map) noexcept
{
    return CountCellsAtLevel(map, 0);
}

// --------------------------------------------------------------------------
// CountCoastCells
//
//  Walks every cell and counts how many are coast tiles (land adjacent to
//  water).  The renderer uses this to size its shoreline overlay buffer.
// --------------------------------------------------------------------------
int32 CountCoastCells(const HeightMap& map) noexcept
{
    if (!map.IsValid())
        return 0;
    int32 count = 0;
    for (int32 y = 0; y < map.GetHeight(); ++y)
    {
        for (int32 x = 0; x < map.GetWidth(); ++x)
        {
            if (map.IsCoastCell(x, y))
                ++count;
        }
    }
    return count;
}

// --------------------------------------------------------------------------
// CountCliffCells
//
//  A cliff cell is one whose maximum neighbor delta exceeds the supplied
//  threshold (typically 2 levels).  Cliff cells use special overlay art in
//  the original engine.
// --------------------------------------------------------------------------
int32 CountCliffCells(const HeightMap& map, int32 threshold) noexcept
{
    if (!map.IsValid())
        return 0;
    int32 count = 0;
    for (int32 y = 0; y < map.GetHeight(); ++y)
    {
        for (int32 x = 0; x < map.GetWidth(); ++x)
        {
            if (map.GetMaxNeighborDelta(x, y) >= threshold)
                ++count;
        }
    }
    return count;
}

// --------------------------------------------------------------------------
// IsCliffCell
//
//  Convenience predicate: returns true if the cell's steepest neighbor delta
//  is at or above the cliff threshold.
// --------------------------------------------------------------------------
bool IsCliffCell(const HeightMap& map, int32 x, int32 y,
                 int32 threshold) noexcept
{
    if (!map.IsValidCell(x, y))
        return false;
    return map.GetMaxNeighborDelta(x, y) >= threshold;
}

// --------------------------------------------------------------------------
// GetSlopeDirection
//
//  Computes the direction (in facing units, 0..255) of the steepest
//  downhill slope at the given cell.  This is used by the water-flow
//  simulation to determine which way runoff travels.  Returns -1 if the
//  cell is flat (no discernible slope).
// --------------------------------------------------------------------------
int32 GetSlopeDirection(const HeightMap& map, int32 x, int32 y) noexcept
{
    if (!map.IsValidCell(x, y))
        return -1;

    int32 center = map.GetHeight(x, y);

    // Sample the eight neighbors.  The offsets are ordered so that index 0
    // is north and they proceed clockwise.
    struct Offset { int32 dx; int32 dy; };
    static const Offset offsets[8] = {
        {  0, -1 },  // N
        {  1, -1 },  // NE
        {  1,  0 },  // E
        {  1,  1 },  // SE
        {  0,  1 },  // S
        { -1,  1 },  // SW
        { -1,  0 },  // W
        { -1, -1 },  // NW
    };

    int32 bestDir = -1;
    int32 bestDrop = 0;
    for (int32 i = 0; i < 8; ++i)
    {
        int32 nx = x + offsets[i].dx;
        int32 ny = y + offsets[i].dy;
        int32 neighbor = map.GetHeight(nx, ny);
        int32 drop = center - neighbor;
        if (drop > bestDrop)
        {
            bestDrop = drop;
            bestDir  = i;
        }
    }

    if (bestDrop <= 0)
        return -1;  // Flat or uphill in all directions.

    // Convert the 8-way direction index into a 256-way facing value.
    // Each octant covers 32 facing units.
    return bestDir * 32;
}

// --------------------------------------------------------------------------
// GetSurfaceNormal
//
//  Approximates the surface normal at the given cell using the height
//  differences to the east and south neighbors.  The result is written into
//  the supplied float triple (nx, ny, nz).  The normal is not normalized;
//  the caller should normalize it if a unit vector is required.
// --------------------------------------------------------------------------
void GetSurfaceNormal(const HeightMap& map, int32 x, int32 y,
                      float& nx, float& ny, float& nz) noexcept
{
    if (!map.IsValidCell(x, y))
    {
        nx = 0.0f;
        ny = 0.0f;
        nz = 1.0f;
        return;
    }

    float scale = static_cast<float>(HeightMap::LeptonsPerLevel);

    float h  = static_cast<float>(map.GetHeight(x, y))     * scale;
    float he = static_cast<float>(map.GetHeight(x + 1, y)) * scale;
    float hs = static_cast<float>(map.GetHeight(x, y + 1)) * scale;

    // The cell is assumed to be one world-unit wide.  The gradient along X
    // is (he - h) and along Y is (hs - h).  The normal is the cross product
    // of the two tangent vectors (1, 0, he-h) and (0, 1, hs-h).
    nx = -(he - h);
    ny = -(hs - h);
    nz = 1.0f;

    // Normalize to a unit vector.
    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 0.0001f)
    {
        nx /= len;
        ny /= len;
        nz /= len;
    }
    else
    {
        nx = 0.0f;
        ny = 0.0f;
        nz = 1.0f;
    }
}

// --------------------------------------------------------------------------
// GetSlopeAngle
//
//  Returns the slope angle in radians at the given cell, computed from the
//  surface normal.  A flat cell returns 0; a vertical cliff approaches
//  pi/2.
// --------------------------------------------------------------------------
float GetSlopeAngle(const HeightMap& map, int32 x, int32 y) noexcept
{
    float nx, ny, nz;
    GetSurfaceNormal(map, x, y, nx, ny, nz);
    // The angle from horizontal is arccos(nz) when the normal is unit length.
    if (nz > 1.0f) nz = 1.0f;
    if (nz < -1.0f) nz = -1.0f;
    return std::acos(nz);
}

// --------------------------------------------------------------------------
// GetSteepestDrop
//
//  Returns the largest single-step height decrease from the center cell to
//  any of its four orthogonal neighbors.  A positive value means the
//  neighbor is lower; zero means no downhill step.
// --------------------------------------------------------------------------
int32 GetSteepestDrop(const HeightMap& map, int32 x, int32 y) noexcept
{
    if (!map.IsValidCell(x, y))
        return 0;
    int32 center = map.GetHeight(x, y);
    int32 maxDrop = 0;

    int32 neighbors[4];
    neighbors[0] = map.GetHeight(x - 1, y);
    neighbors[1] = map.GetHeight(x + 1, y);
    neighbors[2] = map.GetHeight(x, y - 1);
    neighbors[3] = map.GetHeight(x, y + 1);

    for (int32 i = 0; i < 4; ++i)
    {
        int32 drop = center - neighbors[i];
        if (drop > maxDrop) maxDrop = drop;
    }
    return maxDrop;
}

// --------------------------------------------------------------------------
// GetSteepestRise
//
//  Returns the largest single-step height increase from the center cell to
//  any of its four orthogonal neighbors.
// --------------------------------------------------------------------------
int32 GetSteepestRise(const HeightMap& map, int32 x, int32 y) noexcept
{
    if (!map.IsValidCell(x, y))
        return 0;
    int32 center = map.GetHeight(x, y);
    int32 maxRise = 0;

    int32 neighbors[4];
    neighbors[0] = map.GetHeight(x - 1, y);
    neighbors[1] = map.GetHeight(x + 1, y);
    neighbors[2] = map.GetHeight(x, y - 1);
    neighbors[3] = map.GetHeight(x, y + 1);

    for (int32 i = 0; i < 4; ++i)
    {
        int32 rise = neighbors[i] - center;
        if (rise > maxRise) maxRise = rise;
    }
    return maxRise;
}

// --------------------------------------------------------------------------
// RaiseRegion
//
//  Increments the height of every cell inside the rectangular region by the
//  supplied amount.  Values are clamped to the valid range.  Returns the
//  number of cells actually modified.
// --------------------------------------------------------------------------
int32 RaiseRegion(HeightMap& map,
                  int32 minX, int32 minY, int32 maxX, int32 maxY,
                  int32 amount) noexcept
{
    if (!map.IsValid() || amount == 0)
        return 0;

    ClampToMap(map, minX, minY);
    ClampToMap(map, maxX, maxY);
    if (minX > maxX) { int32 t = minX; minX = maxX; maxX = t; }
    if (minY > maxY) { int32 t = minY; minY = maxY; maxY = t; }

    int32 modified = 0;
    for (int32 y = minY; y <= maxY; ++y)
    {
        for (int32 x = minX; x <= maxX; ++x)
        {
            int32 oldH = map.GetHeight(x, y);
            int32 newH = oldH + amount;
            if (newH < 0) newH = 0;
            if (newH > HeightMap::MaxHeightLevels)
                newH = HeightMap::MaxHeightLevels;
            if (newH != oldH)
            {
                map.SetHeight(x, y, newH);
                ++modified;
            }
        }
    }
    return modified;
}

// --------------------------------------------------------------------------
// LowerRegion
//
//  Decrements the height of every cell inside the rectangular region by the
//  supplied amount.  Returns the number of cells modified.
// --------------------------------------------------------------------------
int32 LowerRegion(HeightMap& map,
                  int32 minX, int32 minY, int32 maxX, int32 maxY,
                  int32 amount) noexcept
{
    return RaiseRegion(map, minX, minY, maxX, maxY, -amount);
}

// --------------------------------------------------------------------------
// FlattenRegion
//
//  Sets every cell inside the rectangular region to the supplied uniform
//  height.  Returns the number of cells that changed.
// --------------------------------------------------------------------------
int32 FlattenRegion(HeightMap& map,
                    int32 minX, int32 minY, int32 maxX, int32 maxY,
                    int32 height) noexcept
{
    if (!map.IsValid())
        return 0;

    ClampToMap(map, minX, minY);
    ClampToMap(map, maxX, maxY);
    if (minX > maxX) { int32 t = minX; minX = maxX; maxX = t; }
    if (minY > maxY) { int32 t = minY; minY = maxY; maxY = t; }

    if (height < 0) height = 0;
    if (height > HeightMap::MaxHeightLevels)
        height = HeightMap::MaxHeightLevels;

    int32 modified = 0;
    for (int32 y = minY; y <= maxY; ++y)
    {
        for (int32 x = minX; x <= maxX; ++x)
        {
            if (map.GetHeight(x, y) != height)
            {
                map.SetHeight(x, y, height);
                ++modified;
            }
        }
    }
    return modified;
}

// --------------------------------------------------------------------------
// SmoothRegion
//
//  Applies a 3x3 box-blur averaging filter to the region.  Each cell's new
//  height is the integer-rounded average of itself and its eight neighbors.
//  The operation is performed on a scratch copy so that already-updated
//  values do not influence subsequent samples.  Returns the number of cells
//  whose height changed.
// --------------------------------------------------------------------------
int32 SmoothRegion(HeightMap& map,
                   int32 minX, int32 minY, int32 maxX, int32 maxY) noexcept
{
    if (!map.IsValid())
        return 0;

    ClampToMap(map, minX, minY);
    ClampToMap(map, maxX, maxY);
    if (minX > maxX) { int32 t = minX; minX = maxX; maxX = t; }
    if (minY > maxY) { int32 t = minY; minY = maxY; maxY = t; }

    int32 w = map.GetWidth();
    int32 h = map.GetHeight();

    // Allocate a scratch buffer for the smoothed region.
    int32 regionW = maxX - minX + 1;
    int32 regionH = maxY - minY + 1;
    if (regionW <= 0 || regionH <= 0)
        return 0;

    int8* pScratch = static_cast<int8*>(
        YRMemory::Allocate(sizeof(int8) * regionW * regionH));
    if (!pScratch)
        return 0;

    int32 modified = 0;
    for (int32 ry = 0; ry < regionH; ++ry)
    {
        for (int32 rx = 0; rx < regionW; ++rx)
        {
            int32 cx = minX + rx;
            int32 cy = minY + ry;

            int32 sum = 0;
            int32 count = 0;
            for (int32 dy = -1; dy <= 1; ++dy)
            {
                for (int32 dx = -1; dx <= 1; ++dx)
                {
                    int32 nx = cx + dx;
                    int32 ny = cy + dy;
                    if (nx < 0) nx = 0;
                    if (ny < 0) ny = 0;
                    if (nx >= w) nx = w - 1;
                    if (ny >= h) ny = h - 1;
                    sum += map.GetHeight(nx, ny);
                    ++count;
                }
            }
            int32 avg = (count > 0) ? (sum + count / 2) / count : 0;
            if (avg < 0) avg = 0;
            if (avg > HeightMap::MaxHeightLevels)
                avg = HeightMap::MaxHeightLevels;
            pScratch[ry * regionW + rx] = static_cast<int8>(avg);
        }
    }

    // Write back the smoothed values.
    for (int32 ry = 0; ry < regionH; ++ry)
    {
        for (int32 rx = 0; rx < regionW; ++rx)
        {
            int32 cx = minX + rx;
            int32 cy = minY + ry;
            int32 oldH = map.GetHeight(cx, cy);
            int32 newH = static_cast<int32>(pScratch[ry * regionW + rx]);
            if (newH != oldH)
            {
                map.SetHeight(cx, cy, newH);
                ++modified;
            }
        }
    }

    YRMemory::Deallocate(pScratch);
    return modified;
}

// --------------------------------------------------------------------------
// FillWater
//
//  Sets every cell whose height is at or below the water threshold to 0
//  (submerged).  This is the core of the map editor's "flood" tool.  Returns
//  the number of cells that were lowered to water.
// --------------------------------------------------------------------------
int32 FillWater(HeightMap& map, int32 waterThreshold) noexcept
{
    if (!map.IsValid())
        return 0;
    int32 modified = 0;
    for (int32 y = 0; y < map.GetHeight(); ++y)
    {
        for (int32 x = 0; x < map.GetWidth(); ++x)
        {
            if (map.GetHeight(x, y) <= waterThreshold)
            {
                if (map.GetHeight(x, y) != 0)
                {
                    map.SetHeight(x, y, 0);
                    ++modified;
                }
            }
        }
    }
    return modified;
}

// --------------------------------------------------------------------------
// Erode
//
//  Performs one iteration of a simple thermal-erosion pass.  For every cell
//  that has a neighbor more than "talus" levels below it, material is moved
//  from the higher cell to the lower cell.  This simulates the natural
//  settling of steep terrain.  The operation is performed on a scratch copy
//  so that mid-pass updates do not affect the result.  Returns the number
//  of cells that changed height.
// --------------------------------------------------------------------------
int32 Erode(HeightMap& map, int32 talus) noexcept
{
    if (!map.IsValid() || talus <= 0)
        return 0;

    int32 w = map.GetWidth();
    int32 h = map.GetHeight();
    int32 total = w * h;

    int8* pScratch = static_cast<int8*>(
        YRMemory::Allocate(sizeof(int8) * total));
    if (!pScratch)
        return 0;

    // Snapshot the current state.
    for (int32 y = 0; y < h; ++y)
    {
        for (int32 x = 0; x < w; ++x)
        {
            pScratch[y * w + x] = static_cast<int8>(map.GetHeight(x, y));
        }
    }

    int32 modified = 0;
    for (int32 y = 0; y < h; ++y)
    {
        for (int32 x = 0; x < w; ++x)
        {
            int32 center = static_cast<int32>(pScratch[y * w + x]);

            // Find the lowest neighbor.
            int32 lowest = center;
            int32 lowNX = x;
            int32 lowNY = y;

            int32 neighbors[4][2] = {
                { x - 1, y     },
                { x + 1, y     },
                { x,     y - 1 },
                { x,     y + 1 },
            };

            for (int32 i = 0; i < 4; ++i)
            {
                int32 nx = neighbors[i][0];
                int32 ny = neighbors[i][1];
                if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                    continue;
                int32 nv = static_cast<int32>(pScratch[ny * w + nx]);
                if (nv < lowest)
                {
                    lowest = nv;
                    lowNX = nx;
                    lowNY = ny;
                }
            }

            // If the drop exceeds the talus threshold, move one unit of
            // material from the center to the lowest neighbor.
            if (lowest < center && (center - lowest) > talus)
            {
                int32 newCenter = center - 1;
                int32 newNeighbor = lowest + 1;
                pScratch[y * w + x] = static_cast<int8>(newCenter);
                pScratch[lowNY * w + lowNX] = static_cast<int8>(newNeighbor);
                // Mark both cells as modified (deduplication happens below).
                ++modified;
            }
        }
    }

    // Write back the eroded values and count actual changes.
    int32 actualChanges = 0;
    for (int32 y = 0; y < h; ++y)
    {
        for (int32 x = 0; x < w; ++x)
        {
            int32 oldH = map.GetHeight(x, y);
            int32 newH = static_cast<int32>(pScratch[y * w + x]);
            if (newH != oldH)
            {
                map.SetHeight(x, y, newH);
                ++actualChanges;
            }
        }
    }

    YRMemory::Deallocate(pScratch);
    return actualChanges;
}

// --------------------------------------------------------------------------
// BlendHeightMaps
//
//  Blends two height maps into a destination using a linear weight.  For
//  each cell: dest = a * weight + b * (1 - weight).  The weight is clamped
//  to [0, 1].  All three maps must have identical dimensions.  Returns true
//  on success.
// --------------------------------------------------------------------------
bool BlendHeightMaps(HeightMap& dest, const HeightMap& a,
                     const HeightMap& b, float weight) noexcept
{
    if (!dest.IsValid() || !a.IsValid() || !b.IsValid())
        return false;
    if (dest.GetWidth()  != a.GetWidth()  || dest.GetHeight() != a.GetHeight())
        return false;
    if (dest.GetWidth()  != b.GetWidth()  || dest.GetHeight() != b.GetHeight())
        return false;

    if (weight < 0.0f) weight = 0.0f;
    if (weight > 1.0f) weight = 1.0f;

    for (int32 y = 0; y < dest.GetHeight(); ++y)
    {
        for (int32 x = 0; x < dest.GetWidth(); ++x)
        {
            float ha = static_cast<float>(a.GetHeight(x, y));
            float hb = static_cast<float>(b.GetHeight(x, y));
            int32 blended = static_cast<int32>(ha * weight + hb * (1.0f - weight) + 0.5f);
            dest.SetHeight(x, y, blended);
        }
    }
    return true;
}

// --------------------------------------------------------------------------
// ValidateHeights
//
//  Scans the entire grid and clamps any out-of-range values to the valid
//  [0, MaxHeightLevels] interval.  Returns the number of cells that were
//  corrected.  This is called after loading a save file from an untrusted
//  source.
// --------------------------------------------------------------------------
int32 ValidateHeights(HeightMap& map) noexcept
{
    if (!map.IsValid())
        return 0;
    int32 corrected = 0;
    for (int32 y = 0; y < map.GetHeight(); ++y)
    {
        for (int32 x = 0; x < map.GetWidth(); ++x)
        {
            int32 h = map.GetHeight(x, y);
            if (h < 0 || h > HeightMap::MaxHeightLevels)
            {
                map.SetHeight(x, y, h < 0 ? 0 : HeightMap::MaxHeightLevels);
                ++corrected;
            }
        }
    }
    return corrected;
}

// --------------------------------------------------------------------------
// FindNearestWaterCell
//
//  Searches outward from the given starting cell for the nearest cell at
//  height 0 (water).  The search uses an expanding square ring so that
//  cells at the same Manhattan distance are checked together.  Returns
//  true and fills outWaterX/outWaterY if water is found within the max
//  radius; returns false otherwise.
// --------------------------------------------------------------------------
bool FindNearestWaterCell(const HeightMap& map, int32 startX, int32 startY,
                          int32 maxRadius,
                          int32& outWaterX, int32& outWaterY) noexcept
{
    if (!map.IsValidCell(startX, startY))
        return false;

    // Check the start cell first.
    if (map.GetHeight(startX, startY) <= 0)
    {
        outWaterX = startX;
        outWaterY = startY;
        return true;
    }

    for (int32 r = 1; r <= maxRadius; ++r)
    {
        // Walk the perimeter of the square ring at radius r.
        for (int32 dx = -r; dx <= r; ++dx)
        {
            // Top edge.
            int32 nx = startX + dx;
            int32 ny = startY - r;
            if (map.IsValidCell(nx, ny) && map.GetHeight(nx, ny) <= 0)
            {
                outWaterX = nx;
                outWaterY = ny;
                return true;
            }
            // Bottom edge (skip corners already checked on top edge when
            // dx == -r or dx == r to avoid double-checking).
            if (dx != -r && dx != r)
            {
                ny = startY + r;
                if (map.IsValidCell(nx, ny) && map.GetHeight(nx, ny) <= 0)
                {
                    outWaterX = nx;
                    outWaterY = ny;
                    return true;
                }
            }
        }
        for (int32 dy = -r + 1; dy <= r - 1; ++dy)
        {
            // Left edge.
            int32 nx = startX - r;
            int32 ny = startY + dy;
            if (map.IsValidCell(nx, ny) && map.GetHeight(nx, ny) <= 0)
            {
                outWaterX = nx;
                outWaterY = ny;
                return true;
            }
            // Right edge.
            nx = startX + r;
            if (map.IsValidCell(nx, ny) && map.GetHeight(nx, ny) <= 0)
            {
                outWaterX = nx;
                outWaterY = ny;
                return true;
            }
        }
    }
    return false;
}

// --------------------------------------------------------------------------
// GetHeightAtWorld
//
//  Samples the height at a world coordinate (in leptons).  The world
//  coordinate is converted to cell + fractional offset and then bilinearly
//  interpolated.  This is the high-level entry point used by unit placement
//  and the camera.
// --------------------------------------------------------------------------
float GetHeightAtWorld(const HeightMap& map,
                       float worldX, float worldY) noexcept
{
    if (!map.IsValid())
        return 0.0f;

    // Convert world leptons to cell coordinates.
    float cellX = worldX / static_cast<float>(HeightMap::LeptonsPerLevel);
    float cellY = worldY / static_cast<float>(HeightMap::LeptonsPerLevel);

    int32 ix = static_cast<int32>(cellX);
    int32 iy = static_cast<int32>(cellY);
    float fx = cellX - static_cast<float>(ix);
    float fy = cellY - static_cast<float>(iy);

    return map.GetInterpolatedHeight(ix, iy, fx, fy);
}

// --------------------------------------------------------------------------
// GetZAtWorld
//
//  Like GetHeightAtWorld but returns the result in leptons (the actual Z
//  world coordinate).  Used by the renderer to place sprites at the correct
//  depth.
// --------------------------------------------------------------------------
float GetZAtWorld(const HeightMap& map, float worldX, float worldY) noexcept
{
    return GetHeightAtWorld(map, worldX, worldY) *
           static_cast<float>(HeightMap::LeptonsPerLevel);
}

// --------------------------------------------------------------------------
// CountCellsAboveLevel
//
//  Returns the number of cells whose height strictly exceeds the supplied
//  level.  Used to determine how much land is above a flood threshold.
// --------------------------------------------------------------------------
int32 CountCellsAboveLevel(const HeightMap& map, int32 level) noexcept
{
    if (!map.IsValid())
        return 0;
    int32 count = 0;
    for (int32 y = 0; y < map.GetHeight(); ++y)
    {
        for (int32 x = 0; x < map.GetWidth(); ++x)
        {
            if (map.GetHeight(x, y) > level)
                ++count;
        }
    }
    return count;
}

// --------------------------------------------------------------------------
// GetDominantHeight
//
//  Returns the most frequently occurring height level in the grid.  Ties
//  are broken toward the lower level.  Used by the map loader to pick a
//  default base terrain.
// --------------------------------------------------------------------------
int32 GetDominantHeight(const HeightMap& map) noexcept
{
    int32 histogram[HeightMap::MaxHeightLevels + 1];
    int32 total = GetHeightHistogram(map, histogram);
    if (total == 0)
        return 0;

    int32 bestLevel = 0;
    int32 bestCount = 0;
    for (int32 i = 0; i <= HeightMap::MaxHeightLevels; ++i)
    {
        if (histogram[i] > bestCount)
        {
            bestCount = histogram[i];
            bestLevel = i;
        }
    }
    return bestLevel;
}

} // end anonymous namespace
