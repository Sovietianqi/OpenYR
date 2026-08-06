#pragma once

// ============================================================================
// IsometricTile.h - Isometric tile definition
//
//  Represents a single isometric terrain tile in the game map.  Yuri's
//  Revenge uses an isometric grid where each tile is a diamond shape
//  60 pixels wide and 30 pixels tall.  Each tile carries:
//
//    * A tile-set image index (which sub-tile of the ISO? art to draw)
//    * A terrain height value (0..14 levels, each 104 leptons)
//    * A land-type classification (clear, rough, road, water, etc.)
//    * Ramp/slope information for smooth elevation transitions
//    * Overlay and smudge references
//
//  The tile is the fundamental unit of the map's terrain layer.  Each
//  cell on the map is backed by one IsometricTile.
// ============================================================================

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>

#include <cstdint>

// ============================================================================
// Forward declarations
// ============================================================================

class IsometricTileType;
class OverlayClass;
class SmudgeClass;
class CellClass;

// ============================================================================
// Tile ramp types - define how elevation transitions between tiles
// ============================================================================

enum class TileRampType : int32
{
    None        = 0,   // Flat tile, no ramp
    North       = 1,   // Ramp going up to the north
    South       = 2,   // Ramp going up to the south
    East        = 3,   // Ramp going up to the east
    West        = 4,   // Ramp going up to the west
    NorthEast   = 5,   // Ramp going up to the NE
    NorthWest   = 6,   // Ramp going up to the NW
    SouthEast   = 7,   // Ramp going up to the SE
    SouthWest   = 8,   // Ramp going up to the SW
    CliffN      = 9,   // Cliff facing north
    CliffS      = 10,  // Cliff facing south
    CliffE      = 11,  // Cliff facing east
    CliffW      = 12,  // Cliff facing west
    Water       = 13,  // Water tile (treated as a ramp variant)
    Count       = 14
};

// ============================================================================
// TileFlags - bitfield of per-tile state
// ============================================================================

enum class TileFlags : uint32
{
    None            = 0x00000000,
    Visible         = 0x00000001,   // Currently visible to the player
    Explored        = 0x00000002,   // Previously explored (fog of war)
    Shrouded        = 0x00000004,   // Under shroud
    Passable        = 0x00000008,   // Land units can traverse
    WaterPassable   = 0x00000010,   // Naval units can traverse
    Buildable       = 0x00000020,   // Buildings can be placed here
    HasOverlay      = 0x00000040,   // An overlay (tiberium/wall) is present
    HasSmudge       = 0x00000080,   // A smudge (scorch/crater) is present
    HasTerrainObj   = 0x00000100,   // A terrain object (tree/rock) is present
    IsCoast         = 0x00000200,   // Boundary between land and water
    IsCliff         = 0x00000400,   // Cliff tile (impassable)
    IsBridge        = 0x00000800,   // Bridge tile
    IsTunnel        = 0x00001000,   // Tunnel entrance/interior
    IsIce           = 0x00002000,   // Ice tile (can melt)
    IsRedraw        = 0x00004000,   // Needs redraw next frame
};

inline TileFlags operator|(TileFlags a, TileFlags b) noexcept
{
    return static_cast<TileFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
}
inline TileFlags operator&(TileFlags a, TileFlags b) noexcept
{
    return static_cast<TileFlags>(static_cast<uint32>(a) & static_cast<uint32>(b));
}
inline TileFlags operator~(TileFlags a) noexcept
{
    return static_cast<TileFlags>(~static_cast<uint32>(a));
}

// ============================================================================
// IsometricTile - a single terrain tile
// ============================================================================

class IsometricTile
{
public:
    // ── Constants ───────────────────────────────────────────────────────

    static constexpr int32 MaxHeightLevels = 14;
    static constexpr int32 LeptonsPerLevel = 104;    // Height of one elevation level
    static constexpr int32 TilePixelWidth  = 60;
    static constexpr int32 TilePixelHeight = 30;

    // ── Construction / Destruction ──────────────────────────────────────

    IsometricTile() noexcept;
    IsometricTile(int16 cellX, int16 cellY) noexcept;
    ~IsometricTile() = default;

    // ── Cell position ───────────────────────────────────────────────────

    const CellStruct& GetCell() const noexcept { return Cell; }
    void SetCell(int16 x, int16 y) noexcept { Cell = CellStruct(x, y); }

    int16 GetCellX() const noexcept { return Cell.X; }
    int16 GetCellY() const noexcept { return Cell.Y; }

    // Returns the world coordinate (in leptons) of the tile center.
    CoordStruct GetWorldCoord() const noexcept;

    // ── Tile type / image ───────────────────────────────────────────────

    int32 GetTileSetIndex() const noexcept { return TileSetIndex; }
    void  SetTileSetIndex(int32 index) noexcept { TileSetIndex = index; }

    int32 GetSubTileIndex() const noexcept { return SubTileIndex; }
    void  SetSubTileIndex(int32 index) noexcept { SubTileIndex = index; }

    const IsometricTileType* GetTileType() const noexcept { return TileType; }
    void  SetTileType(const IsometricTileType* pType) noexcept { TileType = pType; }

    // ── Height / elevation ──────────────────────────────────────────────

    int32 GetHeight() const noexcept { return Height; }
    void  SetHeight(int32 h) noexcept;

    // Returns the Z-coordinate (in leptons) of the tile's top surface.
    int32 GetZ() const noexcept { return Height * LeptonsPerLevel; }

    // True if this tile is at ground level (height == 0).
    bool IsGroundLevel() const noexcept { return Height == 0; }

    // True if this tile is a water tile (height < 0 in some representations,
    // or flagged as water).
    bool IsWater() const noexcept;

    // ── Land type ───────────────────────────────────────────────────────

    LandType GetLandType() const noexcept { return Land; }
    void     SetLandType(LandType land) noexcept { Land = land; }

    // ── Ramp / slope ────────────────────────────────────────────────────

    TileRampType GetRampType() const noexcept { return RampType; }
    void         SetRampType(TileRampType ramp) noexcept { RampType = ramp; }

    bool IsRamp() const noexcept { return RampType != TileRampType::None; }
    bool IsCliff() const noexcept;
    bool IsFlat() const noexcept { return RampType == TileRampType::None; }

    // ── Flags ───────────────────────────────────────────────────────────

    bool HasFlag(TileFlags flag) const noexcept
    {
        return (static_cast<uint32>(Flags) & static_cast<uint32>(flag)) != 0;
    }

    void SetFlag(TileFlags flag) noexcept
    {
        Flags = Flags | flag;
    }

    void ClearFlag(TileFlags flag) noexcept
    {
        Flags = Flags & ~flag;
    }

    void ToggleFlag(TileFlags flag) noexcept
    {
        Flags = static_cast<TileFlags>(
            static_cast<uint32>(Flags) ^ static_cast<uint32>(flag));
    }

    // Convenience flag accessors
    bool IsVisible() const noexcept    { return HasFlag(TileFlags::Visible); }
    bool IsExplored() const noexcept   { return HasFlag(TileFlags::Explored); }
    bool IsShrouded() const noexcept   { return HasFlag(TileFlags::Shrouded); }
    bool IsPassable() const noexcept   { return HasFlag(TileFlags::Passable); }
    bool IsBuildable() const noexcept  { return HasFlag(TileFlags::Buildable); }
    bool NeedsRedraw() const noexcept  { return HasFlag(TileFlags::IsRedraw); }

    void SetVisible(bool v)    { v ? SetFlag(TileFlags::Visible) : ClearFlag(TileFlags::Visible); }
    void SetExplored(bool v)   { v ? SetFlag(TileFlags::Explored) : ClearFlag(TileFlags::Explored); }
    void SetShrouded(bool v)   { v ? SetFlag(TileFlags::Shrouded) : ClearFlag(TileFlags::Shrouded); }
    void SetPassable(bool v)   { v ? SetFlag(TileFlags::Passable) : ClearFlag(TileFlags::Passable); }
    void SetNeedsRedraw(bool v){ v ? SetFlag(TileFlags::IsRedraw) : ClearFlag(TileFlags::IsRedraw); }

    // ── Overlay / smudge / terrain object ───────────────────────────────

    OverlayClass* GetOverlay() const noexcept { return Overlay; }
    void SetOverlay(OverlayClass* pOverlay) noexcept;

    SmudgeClass* GetSmudge() const noexcept { return Smudge; }
    void SetSmudge(SmudgeClass* pSmudge) noexcept;

    // ── Drawing helpers ─────────────────────────────────────────────────

    // Returns the pixel position of this tile's top-left corner for the
    // given map origin.
    Point2D GetPixelPosition(int32 originX, int32 originY) const noexcept;

    // Returns the pixel position adjusted for the tile's height.
    Point2D GetPixelPositionWithHeight(int32 originX, int32 originY) const noexcept;

    // ── CRC ─────────────────────────────────────────────────────────────

    void ComputeCRC(class CRCEngine& crc) const;

    // ── Serialization ───────────────────────────────────────────────────

    // Save/Load to/from a stream (serializes the tile's cell, image index,
    // sub-tile, height, land type, ramp type and flags).
    bool Save(IStream* pStm) const;
    bool Load(IStream* pStm);

private:
    CellStruct                 Cell;           // Grid position
    int32                      TileSetIndex;   // Index into the tile set
    int32                      SubTileIndex;   // Sub-tile within the set
    const IsometricTileType*   TileType;       // Pointer to the tile type
    int32                      Height;         // Elevation level (0..14)
    LandType                   Land;           // Land type classification
    TileRampType               RampType;       // Slope / ramp type
    TileFlags                  Flags;          // State flags
    OverlayClass*              Overlay;        // Overlay object (tiberium, walls)
    SmudgeClass*               Smudge;         // Smudge object (scorch, crater)
    CellClass*                 Owner;          // Owning CellClass (back-pointer)
};
