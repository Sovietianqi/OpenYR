#pragma once

// ============================================================================
// HeightMap.h - Height map for terrain elevation
//
//  Stores the per-cell elevation data for the entire map.  The isometric
//  terrain in Yuri's Revenge supports up to 14 elevation levels, each
//  representing 104 leptons of vertical height.  The HeightMap provides:
//
//    * O(1) get/set of a cell's height
//    * Smooth interpolation between adjacent tiles for rendering
//    * Height queries at sub-cell resolution (for unit placement)
//    * Coast detection (where height transitions from land to water)
//    * CRC checksum for multiplayer sync
//
//  The height map is allocated as a 2D grid of int8 values (one per cell)
//  with dimensions matching the map size.
// ============================================================================

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>

#include <cstdint>

// ============================================================================
// HeightMap - per-cell elevation grid
// ============================================================================

class HeightMap
{
public:
    // ── Constants ───────────────────────────────────────────────────────

    static constexpr int32 MaxHeightLevels = 14;
    static constexpr int32 LeptonsPerLevel = 104;
    static constexpr int32 MaxMapWidth     = 256;
    static constexpr int32 MaxMapHeight    = 256;

    // ── Construction / Destruction ──────────────────────────────────────

    HeightMap() noexcept;
    ~HeightMap();

    // Allocate the height grid for the given dimensions.  Returns false if
    // the dimensions are out of range or allocation fails.
    bool Allocate(int32 width, int32 height) noexcept;

    // Release the allocated grid.
    void Release() noexcept;

    // ── Queries ─────────────────────────────────────────────────────────

    int32 GetWidth() const noexcept { return Width; }
    int32 GetHeight() const noexcept { return Height; }

    bool IsValid() const noexcept { return Data != nullptr; }
    bool IsValidCell(int32 x, int32 y) const noexcept;

    // Returns the height level (0..14) at the given cell.  Returns 0 for
    // out-of-range cells.
    int32 GetHeight(int32 x, int32 y) const noexcept;

    // Returns the height level at the given CellStruct.
    int32 GetHeight(const CellStruct& cell) const noexcept
    {
        return GetHeight(cell.X, cell.Y);
    }

    // Returns the Z coordinate (in leptons) at the given cell.
    int32 GetZ(int32 x, int32 y) const noexcept
    {
        return GetHeight(x, y) * LeptonsPerLevel;
    }

    int32 GetZ(const CellStruct& cell) const noexcept
    {
        return GetZ(cell.X, cell.Y);
    }

    // ── Mutators ────────────────────────────────────────────────────────

    // Set the height level at the given cell.  The value is clamped to
    // [0, MaxHeightLevels].
    void SetHeight(int32 x, int32 y, int32 height) noexcept;

    void SetHeight(const CellStruct& cell, int32 height) noexcept
    {
        SetHeight(cell.X, cell.Y, height);
    }

    // Raise / lower the height by one level.
    void Raise(int32 x, int32 y) noexcept;
    void Lower(int32 x, int32 y) noexcept;

    // ── Interpolation ───────────────────────────────────────────────────

    // Bilinearly interpolate the height at sub-cell coordinates.
    // fx and fy are in [0.0, 1.0) relative to the cell origin.
    float GetInterpolatedHeight(int32 x, int32 y, float fx, float fy) const noexcept;

    // Returns the interpolated Z (in leptons) at sub-cell coordinates.
    float GetInterpolatedZ(int32 x, int32 y, float fx, float fy) const noexcept
    {
        return GetInterpolatedHeight(x, y, fx, fy) * static_cast<float>(LeptonsPerLevel);
    }

    // ── Terrain analysis ────────────────────────────────────────────────

    // Returns the maximum height difference between this cell and its 4
    // orthogonal neighbors.  A large difference indicates a cliff.
    int32 GetMaxNeighborDelta(int32 x, int32 y) const noexcept;

    // True if the cell is at a coast (adjacent to a height-0 / water cell
    // while itself being above height 0).
    bool IsCoastCell(int32 x, int32 y) const noexcept;

    // True if the cell is flat (same height as all 4 neighbors).
    bool IsFlatCell(int32 x, int32 y) const noexcept;

    // ── Bulk operations ─────────────────────────────────────────────────

    // Set all cells to the given height.
    void Clear(int32 height = 0) noexcept;

    // Copy height data from another HeightMap.  Returns false if the
    // dimensions do not match.
    bool CopyFrom(const HeightMap& other) noexcept;

    // ── CRC ─────────────────────────────────────────────────────────────

    void ComputeCRC(class CRCEngine& crc) const;

    // ── Serialization ───────────────────────────────────────────────────

    bool Save(class IStream* pStm) const;
    bool Load(class IStream* pStm);

private:
    // Convert (x, y) to a linear index.  Does not bounds-check.
    int32 Index(int32 x, int32 y) const noexcept
    {
        return y * Width + x;
    }

    int8*  Data;     // Height values, one int8 per cell
    int32  Width;    // Grid width in cells
    int32  Height;   // Grid height in cells
};
