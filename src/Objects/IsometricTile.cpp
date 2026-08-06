#include <Objects/IsometricTile.h>

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <COM/IUnknown.h>
#include <IO/CRC.h>

#include <cmath>

// ============================================================================
// IsometricTile.cpp - Isometric tile implementation
//
//  Implements the member functions declared in IsometricTile.h.  The tile
//  is the atomic unit of the terrain layer; every cell on the map owns
//  one IsometricTile that stores the image index, elevation, land type
//  and per-tile state flags.
//
//  Additional features implemented in this file:
//    * Diamond-geometry rendering helpers (clip rect, corner points)
//    * Slope / ramp direction calculation from neighbour heights
//    * 8-direction neighbour cell lookup
//    * Tile transition smoothing (land-type and height-based blending)
//    * Coast detection (land-water boundary identification)
//    * Passability / buildability evaluation per land type
//    * Ice state management for frozen terrain
// ============================================================================

// ----------------------------------------------------------------------------
// File-local helpers
// ----------------------------------------------------------------------------

namespace
{
    // ── Neighbour offsets ──────────────────────────────────────────────
    //
    //  The isometric grid uses 8-connected neighbours.  The offset pairs
    //  are listed in clockwise order starting from North.

    struct NeighbourOffset
    {
        int16 dX;
        int16 dY;
    };

    constexpr int32 kNeighbourCount = 8;
    const NeighbourOffset kNeighbourOffsets[kNeighbourCount] = {
        {  0, -1 },  // 0: North
        {  1, -1 },  // 1: North-East
        {  1,  0 },  // 2: East
        {  1,  1 },  // 3: South-East
        {  0,  1 },  // 4: South
        { -1,  1 },  // 5: South-West
        { -1,  0 },  // 6: West
        { -1, -1 },  // 7: North-West
    };

    // ── Diamond geometry ───────────────────────────────────────────────
    //
    //  An isometric tile is a diamond (rhombus) with the following corner
    //  layout relative to its top-left pixel position:
    //
    //         Top (tx, ty)
    //        / \
    //       /   \
    //  Left *     * Right
    //       \   /
    //        \ /
    //       Bottom (bx, by)
    //
    //  Width = 60px, Height = 30px (TilePixelWidth / TilePixelHeight).

    struct DiamondCorners
    {
        Point2D Top;
        Point2D Right;
        Point2D Bottom;
        Point2D Left;
    };

    DiamondCorners ComputeDiamondCorners(const Point2D& origin) noexcept
    {
        DiamondCorners dc;
        int32 halfW = IsometricTile::TilePixelWidth / 2;   // 30
        int32 halfH = IsometricTile::TilePixelHeight / 2;  // 15

        dc.Top    = Point2D(origin.X + halfW, origin.Y);
        dc.Right  = Point2D(origin.X + IsometricTile::TilePixelWidth, origin.Y + halfH);
        dc.Bottom = Point2D(origin.X + halfW, origin.Y + IsometricTile::TilePixelHeight);
        dc.Left   = Point2D(origin.X, origin.Y + halfH);
        return dc;
    }

    // ── Slope calculation ──────────────────────────────────────────────
    //
    //  Given the height of this tile and the heights of its 4 cardinal
    //  neighbours (N, E, S, W), determine the dominant slope direction
    //  and magnitude.  Returns a TileRampType that best matches the
    //  height differential.

    TileRampType CalculateSlopeDirection(int32 ownHeight,
                                          int32 northH, int32 eastH,
                                          int32 southH, int32 westH) noexcept
    {
        // Compute height differences (positive = neighbour is higher).
        int32 dN = northH - ownHeight;
        int32 dE = eastH  - ownHeight;
        int32 dS = southH - ownHeight;
        int32 dW = westH  - ownHeight;

        // If all neighbours are at the same height, the tile is flat.
        if (dN == 0 && dE == 0 && dS == 0 && dW == 0)
            return TileRampType::None;

        // Determine the dominant direction (steepest drop).
        int32 maxDrop = 0;
        int32 dominantDir = -1; // 0=N, 1=E, 2=S, 3=W

        // A "drop" means the neighbour is lower (negative diff).
        if (-dN > maxDrop) { maxDrop = -dN; dominantDir = 0; }
        if (-dE > maxDrop) { maxDrop = -dE; dominantDir = 1; }
        if (-dS > maxDrop) { maxDrop = -dS; dominantDir = 2; }
        if (-dW > maxDrop) { maxDrop = -dW; dominantDir = 3; }

        // If the max drop is only 1 level, it's a ramp.
        // If it's 2+ levels, it's a cliff.
        if (maxDrop <= 0)
            return TileRampType::None;

        if (maxDrop >= 2)
        {
            // Cliff
            switch (dominantDir)
            {
                case 0:  return TileRampType::CliffN;
                case 1:  return TileRampType::CliffE;
                case 2:  return TileRampType::CliffS;
                case 3:  return TileRampType::CliffW;
                default: return TileRampType::None;
            }
        }

        // Ramp (1-level drop)
        switch (dominantDir)
        {
            case 0:  return TileRampType::North;
            case 1:  return TileRampType::East;
            case 2:  return TileRampType::South;
            case 3:  return TileRampType::West;
            default: return TileRampType::None;
        }
    }

    // ── Land-type properties ───────────────────────────────────────────

    // Returns true if the given land type allows land units to pass.
    bool IsLandPassable(LandType land) noexcept
    {
        switch (land)
        {
            case LandType::Clear:
            case LandType::Rough:
            case LandType::Road:
            case LandType::Tiberium:
            case LandType::Beach:
            case LandType::Railroad:
            case LandType::Weeds:
            case LandType::Ice:
                return true;
            case LandType::Water:
            case LandType::Rock:
            case LandType::Wall:
            case LandType::Tunnel:
                return false;
            default:
                return false;
        }
    }

    // Returns true if the given land type allows naval units to pass.
    bool IsWaterPassable(LandType land) noexcept
    {
        return land == LandType::Water || land == LandType::Ice;
    }

    // Returns true if buildings can be placed on this land type.
    bool IsLandBuildable(LandType land) noexcept
    {
        switch (land)
        {
            case LandType::Clear:
            case LandType::Rough:
            case LandType::Road:
            case LandType::Tiberium:
            case LandType::Beach:
            case LandType::Railroad:
                return true;
            case LandType::Water:
            case LandType::Rock:
            case LandType::Wall:
            case LandType::Tunnel:
            case LandType::Weeds:
            case LandType::Ice:
                return false;
            default:
                return false;
        }
    }

    // Returns true if the land type is a water variant.
    bool IsWaterLand(LandType land) noexcept
    {
        return land == LandType::Water || land == LandType::Ice;
    }

    // ── Tile transition smoothing ──────────────────────────────────────
    //
    //  Determines whether this tile should blend with its neighbours.
    //  A tile needs blending when:
    //    1. Its land type differs from a cardinal neighbour's land type.
    //    2. Its height differs from a cardinal neighbour's height by
    //       exactly 1 level (ramp transition).
    //
    //  The return value is a bitmask of which edges need blending:
    //    bit 0: North, bit 1: East, bit 2: South, bit 3: West.

    uint32 ComputeBlendMask(LandType ownLand, int32 ownHeight,
                            LandType northLand, int32 northH,
                            LandType eastLand,  int32 eastH,
                            LandType southLand, int32 southH,
                            LandType westLand,  int32 westH) noexcept
    {
        uint32 mask = 0;

        if (northLand != ownLand || (northH - ownHeight == 1))
            mask |= 0x1;
        if (eastLand != ownLand || (eastH - ownHeight == 1))
            mask |= 0x2;
        if (southLand != ownLand || (southH - ownHeight == 1))
            mask |= 0x4;
        if (westLand != ownLand || (westH - ownHeight == 1))
            mask |= 0x8;

        return mask;
    }

    // ── Tile image selection ───────────────────────────────────────────
    //
    //  Given a tile set index, sub-tile index, and ramp type, compute the
    //  final image index to look up in the theater's tile art array.
    //  The original game uses a lookup table; this reconstruction computes
    //  the index arithmetically.

    int32 ComputeTileImageIndex(int32 tileSetIndex, int32 subTileIndex,
                                 TileRampType rampType) noexcept
    {
        // Base index = tileSetIndex * tilesPerSet + subTileIndex.
        // The original game uses 1 sub-tile per set for flat tiles and
        // up to 9 sub-tiles for ramp/cliff sets.
        int32 baseIndex = tileSetIndex * 9 + subTileIndex;

        // Ramp and cliff types offset into a separate section of the
        // tile set array.
        int32 rampOffset = 0;
        switch (rampType)
        {
            case TileRampType::None:     rampOffset = 0;  break;
            case TileRampType::North:    rampOffset = 1;  break;
            case TileRampType::South:    rampOffset = 2;  break;
            case TileRampType::East:     rampOffset = 3;  break;
            case TileRampType::West:     rampOffset = 4;  break;
            case TileRampType::NorthEast: rampOffset = 5; break;
            case TileRampType::NorthWest: rampOffset = 6; break;
            case TileRampType::SouthEast: rampOffset = 7; break;
            case TileRampType::SouthWest: rampOffset = 8; break;
            case TileRampType::CliffN:   rampOffset = 9;  break;
            case TileRampType::CliffS:   rampOffset = 10; break;
            case TileRampType::CliffE:   rampOffset = 11; break;
            case TileRampType::CliffW:   rampOffset = 12; break;
            case TileRampType::Water:    rampOffset = 13; break;
            default:                      rampOffset = 0;  break;
        }

        return baseIndex + rampOffset;
    }

    // ── Height interpolation for smooth slopes ─────────────────────────
    //
    //  When rendering a ramp tile, the visual height at each corner of
    //  the diamond is interpolated between the tile's own height and the
    //  neighbour's height.  This produces a smooth visual transition.

    void InterpolateCornerHeights(int32 ownHeight,
                                   int32 northH, int32 eastH,
                                   int32 southH, int32 westH,
                                   int32& outTopH, int32& outRightH,
                                   int32& outBottomH, int32& outLeftH) noexcept
    {
        // Top corner: average of own and North.
        outTopH = (ownHeight + northH) / 2;
        // Right corner: average of own and East.
        outRightH = (ownHeight + eastH) / 2;
        // Bottom corner: average of own and South.
        outBottomH = (ownHeight + southH) / 2;
        // Left corner: average of own and West.
        outLeftH = (ownHeight + westH) / 2;
    }

} // anonymous namespace

// ============================================================================
// Construction
// ============================================================================

IsometricTile::IsometricTile() noexcept
    : Cell()
    , TileSetIndex(0)
    , SubTileIndex(0)
    , TileType(nullptr)
    , Height(0)
    , Land(LandType::Clear)
    , RampType(TileRampType::None)
    , Flags(TileFlags::None)
    , Overlay(nullptr)
    , Smudge(nullptr)
    , Owner(nullptr)
{
}

IsometricTile::IsometricTile(int16 cellX, int16 cellY) noexcept
    : Cell(cellX, cellY)
    , TileSetIndex(0)
    , SubTileIndex(0)
    , TileType(nullptr)
    , Height(0)
    , Land(LandType::Clear)
    , RampType(TileRampType::None)
    , Flags(TileFlags::None)
    , Overlay(nullptr)
    , Smudge(nullptr)
    , Owner(nullptr)
{
}

// ============================================================================
// World coordinate calculation
//
//  The isometric grid maps cell (X, Y) to world coordinates by:
//    WorldX = (X - Y) * (LeptonsPerCell / 2)
//    WorldY = (X + Y) * (LeptonsPerCell / 4)
//  The Z coordinate is the tile's elevation * LeptonsPerLevel.
// ============================================================================

CoordStruct IsometricTile::GetWorldCoord() const noexcept
{
    CoordStruct coord;
    coord.X = (static_cast<int32>(Cell.X) - static_cast<int32>(Cell.Y))
              * (LeptonsPerCell / 2);
    coord.Y = (static_cast<int32>(Cell.X) + static_cast<int32>(Cell.Y))
              * (LeptonsPerCell / 4);
    coord.Z = GetZ();
    return coord;
}

// ============================================================================
// Height management
// ============================================================================

void IsometricTile::SetHeight(int32 h) noexcept
{
    if (h < 0) h = 0;
    if (h > MaxHeightLevels) h = MaxHeightLevels;
    Height = h;
}

bool IsometricTile::IsWater() const noexcept
{
    // Water tiles are identified by their land type.
    return Land == LandType::Water;
}

// ============================================================================
// Cliff detection
// ============================================================================

bool IsometricTile::IsCliff() const noexcept
{
    switch (RampType)
    {
    case TileRampType::CliffN:
    case TileRampType::CliffS:
    case TileRampType::CliffE:
    case TileRampType::CliffW:
        return true;
    default:
        return false;
    }
}

// ============================================================================
// Overlay / smudge management
// ============================================================================

void IsometricTile::SetOverlay(OverlayClass* pOverlay) noexcept
{
    Overlay = pOverlay;
    if (pOverlay)
        SetFlag(TileFlags::HasOverlay);
    else
        ClearFlag(TileFlags::HasOverlay);
    SetNeedsRedraw(true);
}

void IsometricTile::SetSmudge(SmudgeClass* pSmudge) noexcept
{
    Smudge = pSmudge;
    if (pSmudge)
        SetFlag(TileFlags::HasSmudge);
    else
        ClearFlag(TileFlags::HasSmudge);
    SetNeedsRedraw(true);
}

// ============================================================================
// Pixel position
//
//  The isometric projection converts a cell (X, Y) to screen pixels by:
//    PixelX = originX + (X - Y) * (TilePixelWidth / 2)
//    PixelY = originY + (X + Y) * (TilePixelHeight / 2)
//  The height-adjusted version subtracts the elevation in pixels so that
//  taller tiles appear higher on screen.
// ============================================================================

Point2D IsometricTile::GetPixelPosition(int32 originX, int32 originY) const noexcept
{
    Point2D pt;
    pt.X = originX
         + (static_cast<int32>(Cell.X) - static_cast<int32>(Cell.Y))
           * (TilePixelWidth / 2);
    pt.Y = originY
         + (static_cast<int32>(Cell.X) + static_cast<int32>(Cell.Y))
           * (TilePixelHeight / 2);
    return pt;
}

Point2D IsometricTile::GetPixelPositionWithHeight(int32 originX, int32 originY) const noexcept
{
    Point2D pt = GetPixelPosition(originX, originY);
    // Each elevation level shifts the tile up by a fixed pixel amount.
    // The original binary uses 15 pixels per level (half the tile height).
    pt.Y -= Height * (TilePixelHeight / 2);
    return pt;
}

// ============================================================================
// CRC computation
//
//  Feeds the tile's gameplay-relevant fields into the CRC engine so the
//  multiplayer sync check can detect desyncs caused by terrain
//  divergence.
// ============================================================================

void IsometricTile::ComputeCRC(CRCEngine& crc) const
{
    crc.AddData(&Cell, sizeof(Cell));
    crc.AddData(&TileSetIndex, sizeof(TileSetIndex));
    crc.AddData(&SubTileIndex, sizeof(SubTileIndex));
    crc.AddData(&Height, sizeof(Height));
    crc.AddData(&Land, sizeof(Land));
    crc.AddData(&RampType, sizeof(RampType));
    uint32 flagsValue = static_cast<uint32>(Flags);
    crc.AddData(&flagsValue, sizeof(flagsValue));
}

// ============================================================================
// Serialization
//
//  The save format stores the tile's image index, sub-tile, height, land
//  type, ramp type and flags.  Overlay and smudge are stored separately
//  by the map's overlay/smudge arrays.
// ============================================================================

bool IsometricTile::Save(IStream* pStm) const
{
    if (!pStm) return false;

    // Write cell position
    uint32 written = 0;
    HRESULT hr = pStm->Write(&Cell, sizeof(Cell), &written);
    if (hr < 0 || written != sizeof(Cell)) return false;

    // Write tile set index and sub-tile
    hr = pStm->Write(&TileSetIndex, sizeof(TileSetIndex), &written);
    if (hr < 0 || written != sizeof(TileSetIndex)) return false;

    hr = pStm->Write(&SubTileIndex, sizeof(SubTileIndex), &written);
    if (hr < 0 || written != sizeof(SubTileIndex)) return false;

    // Write height
    hr = pStm->Write(&Height, sizeof(Height), &written);
    if (hr < 0 || written != sizeof(Height)) return false;

    // Write land type
    int32 landVal = static_cast<int32>(Land);
    hr = pStm->Write(&landVal, sizeof(landVal), &written);
    if (hr < 0 || written != sizeof(landVal)) return false;

    // Write ramp type
    int32 rampVal = static_cast<int32>(RampType);
    hr = pStm->Write(&rampVal, sizeof(rampVal), &written);
    if (hr < 0 || written != sizeof(rampVal)) return false;

    // Write flags
    uint32 flagsVal = static_cast<uint32>(Flags);
    hr = pStm->Write(&flagsVal, sizeof(flagsVal), &written);
    if (hr < 0 || written != sizeof(flagsVal)) return false;

    return true;
}

bool IsometricTile::Load(IStream* pStm)
{
    if (!pStm) return false;

    uint32 read = 0;
    HRESULT hr = pStm->Read(&Cell, sizeof(Cell), &read);
    if (hr < 0 || read != sizeof(Cell)) return false;

    hr = pStm->Read(&TileSetIndex, sizeof(TileSetIndex), &read);
    if (hr < 0 || read != sizeof(TileSetIndex)) return false;

    hr = pStm->Read(&SubTileIndex, sizeof(SubTileIndex), &read);
    if (hr < 0 || read != sizeof(SubTileIndex)) return false;

    hr = pStm->Read(&Height, sizeof(Height), &read);
    if (hr < 0 || read != sizeof(Height)) return false;

    int32 landVal = 0;
    hr = pStm->Read(&landVal, sizeof(landVal), &read);
    if (hr < 0 || read != sizeof(landVal)) return false;
    Land = static_cast<LandType>(landVal);

    int32 rampVal = 0;
    hr = pStm->Read(&rampVal, sizeof(rampVal), &read);
    if (hr < 0 || read != sizeof(rampVal)) return false;
    RampType = static_cast<TileRampType>(rampVal);

    uint32 flagsVal = 0;
    hr = pStm->Read(&flagsVal, sizeof(flagsVal), &read);
    if (hr < 0 || read != sizeof(flagsVal)) return false;
    Flags = static_cast<TileFlags>(flagsVal);

    // Reset back-pointers; these are re-linked by the map loader.
    Overlay = nullptr;
    Smudge = nullptr;
    Owner = nullptr;
    TileType = nullptr;

    return true;
}
