#include <Objects/IsometricTile.h>
#include <Objects/IsometricTileGlobals.h>

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <COM/IUnknown.h>
#include <FileFormats/TMP.h>
#include <Game/Externs.h>
#include <IO/CRC.h>
#include <INI/INIClass.h>
#include <Rendering/TileArt.h>

#include <cmath>
#include <cstdio>
#include <cstring>

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

// ============================================================================
// IsometricTileType
// ============================================================================

DynamicVectorClass<IsometricTileType*>* TileTypeClass_Array = nullptr;
int32                                   TileSet_TotalTileCount = 0;

IsometricTileType::IsometricTileType() noexcept
    : TilesInSet(0), LastTilesInSet(0)
    , MarbleMadness(0xFFFF), NonMarbleMadness(0xFFFF)
    , Morphable(false), AllowToPlace(true), AllowBurrowing(true)
    , AllowTiberium(false), RequiredForRMG(false)
    , ToSnowTheater(0), ToTemperateTheater(0)
    , ShadowCaster(false), ShadowTiles(0)
    , TileXOffset(nullptr), TileYOffset(nullptr)
    , TileAttachesTo(nullptr), TileZAdjust(nullptr)
    , ArrayIndex(0)
    , GlobalStartIndex(0)
    , TileColorTable(nullptr)
    , AttachesToType(nullptr)
    , AnimTypeIndex(-1)
    , TilesPerSetInstances(1)
    , ImageData(nullptr)
    , ImageLoadAttempted(false)
{
    SetName[0] = '\0';
    FileName[0] = '\0';
}

IsometricTileType::~IsometricTileType() noexcept
{
    delete[] TileXOffset;
    delete[] TileYOffset;
    delete[] TileAttachesTo;
    delete[] TileZAdjust;

    // asm: 析构里逐个 delete 调色表行再释放行表（~IsometricTileTypeClass）。
    if (TileColorTable != nullptr) {
        for (int32 i = 0; i < TilesInSet; ++i)
            delete[] TileColorTable[i];
        delete[] TileColorTable;
    }

    TileXOffset = nullptr;
    TileYOffset = nullptr;
    TileAttachesTo = nullptr;
    TileZAdjust = nullptr;
    TileColorTable = nullptr;
    AttachesToType = nullptr;
}

int32 IsometricTileType::GetTileSetCount()
{
    return TileTypeClass_Array ? TileTypeClass_Array->Count : 0;
}

IsometricTileType* IsometricTileType::GetTileSet(int32 index)
{
    if (!TileTypeClass_Array || index < 0)
        return nullptr;

    // 注意：index 是 [TileSet%04d] 的编号（CellClass::TileType 与
    // IsometricTileType::ArrayIndex 都是它），不是 vector 下标。
    // CreateFromINIList 会跳过 ToSnow/ToTemperate = 0 的条目，两者会分叉。
    const int32 count = TileTypeClass_Array->Count;
    for (int32 i = 0; i < count; ++i)
    {
        IsometricTileType* pSet = (*TileTypeClass_Array)[i];
        if (pSet != nullptr && pSet->ArrayIndex == index)
            return pSet;
    }
    return nullptr;
}

// ───────────────────────────────────────────────────────────────────────────
// 全局瓦片序号 → (瓦片集, 集内子序号)
//
//   地图 [IsoMapPack5] 每格存的是**全局序号**（跨全部瓦片集累加），而绘制
//   需要的是「哪一套图的第几块」。原版的起点表是 dword_AA1140[setIndex]，
//   由 CreateFromINIList 逐集累加 TilesInSet 填入（asm 0x546C30）。这里用
//   每集自带的 GlobalStartIndex 做区间判定。
//
//   0xFFFF 是“无瓦片”哨兵：asm sub_544E30 的第一条就是
//   `cmp ecx,0FFFFh / jnz ... / retn`，原样返回不参与任何换算。
// ───────────────────────────────────────────────────────────────────────────
bool IsometricTileType::Split_Global_Tile_Index(int32 globalIndex,
                                               int32& outSetIndex,
                                               int32& outSubIndex)
{
    outSetIndex = -1;
    outSubIndex = -1;

    if (TileTypeClass_Array == nullptr || globalIndex < 0 || globalIndex == 0xFFFF)
        return false;

    const int32 count = TileTypeClass_Array->Count;
    for (int32 i = 0; i < count; ++i)
    {
        const IsometricTileType* pSet = (*TileTypeClass_Array)[i];
        if (pSet == nullptr || pSet->TilesInSet <= 0)
            continue;

        const int32 start = pSet->GlobalStartIndex;
        if (globalIndex < start || globalIndex >= start + pSet->TilesInSet)
            continue;

        outSetIndex = pSet->ArrayIndex;
        outSubIndex = globalIndex - start;
        return true;
    }

    return false;
}


// IsometricTileTypeClass_FindIndex - looks a tile set up by its SetName.
// The original runs a case-insensitive compare over the registered sets and
// answers -1 when nothing matches, which is what BuildingTypeClass::ToTile
// guards against before it stores the pointer.
int32 IsometricTileType::FindIndex(const char* pID)
{
    if (pID == nullptr || pID[0] == '\0')
        return -1;

    if (!TileTypeClass_Array)
        return -1;

    for (int32 i = 0; i < TileTypeClass_Array->Count; ++i) {
        IsometricTileType* pTileSet = (*TileTypeClass_Array)[i];
        if (pTileSet == nullptr)
            continue;

        if (_strcmpi(pTileSet->SetName, pID) == 0)
            return i;
    }

    return -1;
}

// ============================================================================
// IsometricTileTypeClass_CreateSome (asm:714657)
//
//   bool __thiscall(this, WhereType where) —— 按本类型创建一块等距瓦片。
//   asm 逐句：
//     new 0xB0（sizeof IsometricTileClass）
//     IsometricTileClass_CTOR(this, idx=[this+294h], where)
//     return 分配并构造成功 ? true : false
//
//   OpenYR 对应：IsometricTile 承载瓦片实例（其构造参数为锚点格 X/Y），
//   类型号取本类型的 ArrayIndex（asm [this+294h]），并把类型挂到瓦片上。
// ============================================================================
bool IsometricTileType::CreateSome(const CellStruct& where, int32 a3)
{
    // asm: push 0B0h / call new / test eax,eax / jz fail
    IsometricTile* pTile = new IsometricTile(where.X, where.Y);
    if (pTile == nullptr)
        return false;                          // asm: xor al,al / retn 8

    // asm: edx = [esi+294h]（类型索引）; IsometricTileClass_CTOR(idx, where)
    pTile->SetTileSetIndex(ArrayIndex);
    pTile->SetTileType(this);                  // asm: vec_IsometricTileTypes[idx]

    // asm: a3 是 vtable 签名的一部分但本体未用（retn 8 的第 2 参）。
    (void)a3;

    // asm: test eax,eax / jz fail ; mov al,1
    return true;
}

// =============================================================================
// asm 原版命名形态
// =============================================================================

// asm: IsometricTileTypeClass_544E00(this, int a2) retn 4 (asm:427)
//   edx = a2 % [this+2F0h]；从 this 起沿 [this+2BCh] 走 edx 步并返回。
//   即：在 AttachesTo 链上按"每块实例数"取模后定位链节。
IsometricTileType* IsometricTileType::GetTile(int32 index) const
{
    const int32 perSet = (TilesPerSetInstances > 0) ? TilesPerSetInstances : 1;

    int32 rem = index;
    if (index > perSet - 1)                    // asm: cmp eax, esi-1 ; jle skip
        rem = index % perSet;                  // asm: idiv esi ; edx

    const IsometricTileType* node = this;
    while (rem > 0                   // asm: test edx,edx ; jz ret
           && node->AttachesToType != nullptr)
    {
        node = node->AttachesToType;           // asm: mov eax,[eax+2BCh]
        --rem;
    }
    return const_cast<IsometricTileType*>(node);
}

// asm: IsometricTileTypeClass_Get_Cell_Color?(this, a2, a3) retn 8 (asm:940)
//   if (![this+0A4h] && [this+2F4h]) sub_547020(this)  ; 惰性准备
//   return [ [this+2A8h][a2] + a3*4 ]                  ; 第 a2 行第 a3 列
uint32 IsometricTileType::GetCellColor(int32 a2, int32 a3) const
{
    if (TileColorTable == nullptr)
        return 0;
    if (a2 < 0 || a2 >= TilesInSet)
        return 0;
    if (TileColorTable[a2] == nullptr)
        return 0;

    return TileColorTable[a2][a3];             // asm: [eax+edx*4]
}

// asm: IsometricTileTypeClass_GetCRC(this, buf) retn 4 (asm:595)
//   AbstractTypeClass_GetCRC(buf) 之后逐个 CRC_DWORD/CRC_bool 本类字段。
int32 IsometricTileType::GetCRC(CRCEngine& crc) const
{
    // asm: 基数先喂 AbstractTypeClass 部分（SetName/FileName 等）。
    crc.AddData(SetName, sizeof(SetName));
    crc.AddData(FileName, sizeof(FileName));

    // asm: 之后按 [294h..2F0h] 顺序逐字段。
    crc.AddData(&ArrayIndex, sizeof(ArrayIndex));
    crc.AddData(&TilesInSet, sizeof(TilesInSet));
    crc.AddData(&LastTilesInSet, sizeof(LastTilesInSet));
    crc.AddData(&MarbleMadness, sizeof(MarbleMadness));
    crc.AddData(&NonMarbleMadness, sizeof(NonMarbleMadness));
    crc.AddData(&TilesPerSetInstances, sizeof(TilesPerSetInstances));

    uint8 flags = 0;
    if (Morphable)     flags |= 0x01;
    if (AllowToPlace)  flags |= 0x02;
    if (AllowBurrowing) flags |= 0x04;
    crc.AddData(&flags, sizeof(flags));

    crc.AddData(&ToSnowTheater, sizeof(ToSnowTheater));
    crc.AddData(&ToTemperateTheater, sizeof(ToTemperateTheater));
    crc.AddData(&TileAttachesTo, sizeof(TileAttachesTo));

    return 0;
}

// asm: IsometricTileTypeClass_PointerGotInvalid(this, p, removed) retn 8 (asm:892)
void IsometricTileType::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    (void)removed;

    // asm: if ([ecx+2BCh] == p) [ecx+2BCh] = 0
    if (AttachesToType == reinterpret_cast<IsometricTileType*>(pInvalid))
        AttachesToType = nullptr;

    // asm: if ([ecx+2C8h] != -1 && vec_AnimTypes[[ecx+2C8h]] == p)
    //        [ecx+2C8h] = -1
    // OpenYR 未持有 vec_AnimTypes 表；按语义只要该索引有效即视为命中并置 -1。
    if (AnimTypeIndex != -1 && pInvalid != nullptr)
        AnimTypeIndex = -1;
}

// asm: IsometricTileTypeClass_GetAbstractDerivationID -> 0x12 (asm:988)
int32 IsometricTileType::GetAbstractDerivationID() const
{
    return 0x12;
}

// asm: IsometricTileTypeClass_GetSize -> 0x30Ch (asm:1017)
int32 IsometricTileType::GetSize() const
{
    return 0x30C;
}

// asm: sub_54A160 -> [this+294h] (asm:1034)
int32 IsometricTileType::GetArrayIndex() const
{
    return ArrayIndex;
}

// asm: sub_544CB0(this) (asm:349)
//   if (![this+0A4h] && [this+2F4h]) sub_547020(this) ; return [this+0A4h]
void* IsometricTileType::GetImageData()
{
    if (ImageData != nullptr || FileName[0] == '\0' || ImageLoadAttempted)
        return ImageData;

    // 素材缺失时不重试（否则每帧都会走一次 fopen）。
    ImageLoadAttempted = true;

    // asm: sub_547020 惰性构建图像数据 —— 整文件读入后按 cell 偏移表重定位。
    //   文件名 = [TileSet 的 FileName] + 当前剧场扩展名。原版剧场扩展名来自
    //   theatreData.ext（[Theater] 段的 Extension= 键）；此处按固定表回退，
    //   与 ObjectTypeClass::TheaterSpecificID 的顺序一致。
    static const char* const kTheaterExt[6] = {
        ".tem", ".sno", ".urb", ".des", ".lun", ".nub"
    };

    int32 theater = static_cast<int32>(g_Theater);
    if (theater < 0 || theater > 5)
        theater = 0;

    char fullName[0x88];
    if (std::strrchr(FileName, '.') != nullptr)
    {
        std::snprintf(fullName, sizeof(fullName), "%s", FileName);
    }
    else
    {
        std::snprintf(fullName, sizeof(fullName), "%s%s",
                      FileName, kTheaterExt[theater]);
    }

    SHPStruct* pShape = TMPFile::LoadFromFile(fullName);
    ImageData = pShape;

    // 渲染层的图集表按 IsoTileTypeIndex 取句柄；帧号 = TileSubIndex*3 + raster
    // （每个 cell 最多 3 段，见 FileFormats/TMP.h）。
    if (pShape != nullptr)
        TileArt::Set_Tile_Shape(ArrayIndex, pShape);

    return ImageData;
}

// asm: sub_544D30(this, out) retn 4 (asm:406)
//   *out = 0x7FFF7FFF（wXY 哨兵）；返回全局脚印表头。
const CellStruct* IsometricTileType::GetFoundationData(CellStruct* pOut) const
{
    if (pOut != nullptr)
    {
        pOut->X = static_cast<int16>(0x7FFF);   // asm: mov word [var], 7FFFh
        pOut->Y = static_cast<int16>(0x7FFF);   // asm: mov word [var+2], 7FFFh
    }
    // asm: mov eax, offset byte_AA0E3C —— 静态脚印偏移表头；OpenYR 见
    // DisplayClass::FoundationBoundsSize 的等价遍历，返回 pOut 作为表头。
    return pOut;
}

// asm: sub_549B50(dst, src) retn 8 (asm:578) —— 拷贝 3 个 dword。
void IsometricTileType::Copy3Dwords(void* pDst, const void* pSrc)
{
    if (pDst == nullptr || pSrc == nullptr)
        return;
    const uint32* s = static_cast<const uint32*>(pSrc);
    uint32*       d = static_cast<uint32*>(pDst);
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

// asm: IsometricTileTypeClass_ReturnFalse (asm:479)
bool IsometricTileType::ReturnFalse()
{
    return false;
}

// Reads one [TileSet%04d] block.  The per tile arrays are sized from
// TilesInSet and every element defaults to zero; LastTilesInSet, when it is
// set and differs from TilesInSet, describes a second run of tiles that
// shares the same art file.
bool IsometricTileType::LoadFromINI(CCINIClass* pINI)
{
    if (!pINI)
        return false;

    char section[0x40];
    std::sprintf(section, "TileSet%04d", ArrayIndex);

    if (pINI->GetSection(section) == nullptr)
        return false;

    TilesInSet = pINI->ReadInteger(section, "TilesInSet", 0);
    if (TilesInSet == 0)
        return false;

    LastTilesInSet = pINI->ReadInteger(section, "LastTilesInSet", 0);
    if (LastTilesInSet == TilesInSet)
        LastTilesInSet = 0;

    pINI->ReadString(section, "SetName", "No Name", SetName, sizeof(SetName));
    pINI->ReadString(section, "FileName", "TILE", FileName, sizeof(FileName));
    SetName[sizeof(SetName) - 1] = '\0';
    FileName[sizeof(FileName) - 1] = '\0';

    MarbleMadness      = pINI->ReadInteger(section, "MarbleMadness", 0xFFFF);
    NonMarbleMadness   = pINI->ReadInteger(section, "NonMarbleMadness", 0xFFFF);
    Morphable          = pINI->ReadBool(section, "Morphable", false);
    AllowToPlace       = pINI->ReadBool(section, "AllowToPlace", true);
    AllowBurrowing     = pINI->ReadBool(section, "AllowBurrowing", true);
    AllowTiberium      = pINI->ReadBool(section, "AllowTiberium", false);
    RequiredForRMG     = pINI->ReadBool(section, "RequiredForRMG", false);
    ToSnowTheater      = pINI->ReadInteger(section, "ToSnowTheater", 0);
    ToTemperateTheater = pINI->ReadInteger(section, "ToTemperateTheater", 0);
    ShadowCaster       = pINI->ReadBool(section, "ShadowCaster", false);
    ShadowTiles        = pINI->ReadInteger(section, "ShadowTiles", 0);

    TileXOffset    = new int32[TilesInSet]();
    TileYOffset    = new int32[TilesInSet]();
    TileAttachesTo = new int32[TilesInSet]();
    TileZAdjust    = new int32[TilesInSet]();

    if (!TileXOffset || !TileYOffset || !TileAttachesTo || !TileZAdjust)
        return false;

    for (int32 i = 0; i < TilesInSet; ++i) {
        char key[0x20];

        // The original folds the ordinal and the field name into a single
        // format string - "Tile%02dXOffset" and friends - so the literal
        // forms below are the ones the binary carries.
        std::sprintf(key, "Tile%02dXOffset", i + 1);
        TileXOffset[i] = pINI->ReadInteger(section, key, 0);

        std::sprintf(key, "Tile%02dYOffset", i + 1);
        TileYOffset[i] = pINI->ReadInteger(section, key, 0);

        std::sprintf(key, "Tile%02dAttachesTo", i + 1);
        TileAttachesTo[i] = pINI->ReadInteger(section, key, 0);

        std::sprintf(key, "Tile%02dZAdjust", i + 1);
        TileZAdjust[i] = pINI->ReadInteger(section, key, 0);
    }

    return true;
}

// Walks TileSet0000, TileSet0001, ... until a number is missing and creates one
// type per set.  The flag selects the theater: a set is kept only when its
// theater key is non-zero, and only the sets that are kept contribute to the
// global tile count.
void IsometricTileType::CreateFromINIList(CCINIClass* pINI, bool bTheater)
{
    if (!pINI)
        return;

    if (!TileTypeClass_Array)
        TileTypeClass_Array = new DynamicVectorClass<IsometricTileType*>();

    TileSet_TotalTileCount = 0;

    for (int32 i = 0; i < 0x10000; ++i) {
        char section[0x40];
        std::sprintf(section, "TileSet%04d", i);

        if (pINI->GetSection(section) == nullptr)
            break;

        IsometricTileType* pTileSet = new IsometricTileType();
        if (!pTileSet)
            break;

        pTileSet->ArrayIndex = i;

        if (!pTileSet->LoadFromINI(pINI)) {
            delete pTileSet;
            continue;
        }

        if (bTheater) {
            if (pTileSet->ToSnowTheater == 0) {
                delete pTileSet;
                continue;
            }
        } else {
            if (pTileSet->ToTemperateTheater == 0) {
                delete pTileSet;
                continue;
            }
        }

        if (!TileTypeClass_Array->Add(pTileSet)) {
            delete pTileSet;
            continue;
        }

        // asm 0x546C30：`dword_AA1140[setIndex] = 全局起点` 在累加**之前**
        // 填入，ebx 就是这一集的全局起点。特殊瓦片键（tile_ClearTile /
        // tile_BridgeSet / tile_GreenTile ...）存的也是这个全局起点，
        // 判定式形如 `start <= idx < start + 0x10`。
        const int32 setStart = TileSet_TotalTileCount;
        pTileSet->GlobalStartIndex = setStart;
        Apply_Special_Tile_Indices(pINI, bTheater, pTileSet->SetName, setStart);

        TileSet_TotalTileCount += pTileSet->TilesInSet;
    }
}

// The [General] block of the tileset INI names, one by one, which SetName
// belongs to each special tile-set slot.  The order and spelling below is the
// order the binary reads them in IsometricTileTypeClass_CreateFromINIList, and
// every key is compared against the tile set's SetName before the running
// ordinal is stored.
//
// The table drives both the initial read (which fills a local per key) and the
// store (which happens while the tile sets are walked).  Re-reading the key
// here keeps the two in lockstep without duplicating the ~56 locals the
// original keeps on the stack.
void IsometricTileType::Apply_Special_Tile_Indices(CCINIClass* pINI, bool bTheater,
                                                   const char* pSetName, int32 ordinal)
{
    if (pINI == nullptr || pSetName == nullptr || pSetName[0] == '\0')
        return;

    // The snow theater reads the MD.INI copy of [General]; the temperate one
    // reads the theater INI that was handed in.  Both carry the same keys.
    struct SpecialTileKey {
        const char* Key;
        int32*      Target;
    };

    static const SpecialTileKey SpecialKeys[] = {
        { "RampBase",          &tile_RampBase },
        { "RampSmooth",        &tile_RampSmooth },
        { "MMRampBase",        &tile_MMRampBase },
        { "ClearTile",         &tile_ClearTile },
        { "RoughTile",         &tile_RoughTile },
        { "SandTile",          &tile_SandTile },
        { "GreenTile",         &tile_GreenTile },
        { "PaveTile",          &tile_PaveTile },
        { "MiscPaveTile",      &tile_MiscPaveTile },
        { "ClearToRoughLat",   &tile_ClearToRoughLat },
        { "ClearToSandLat",    &tile_ClearToSandLat },
        { "ClearToGreenLat",   &tile_ClearToGreenLat },
        { "ClearToPaveLat",    &tile_ClearToPaveLat },
        { "HeightBase",        &tile_HeightBase },
        { "BlackTile",         &tile_BlackTile },
        { "BridgeSet",         &tile_BridgeSet },
        { "WoodBridgeSet",     &tile_WoodBridgeSet },
        { "CliffSet",          &tile_CliffSet },
        { "ShorePieces",       &tile_ShorePieces },
        { "WaterSet",          &tile_WaterSet },
        { "SlopeSetPieces",    &tile_SlopeSetPieces },
        { "SlopeSetPieces2",   &tile_SlopeSetPieces2 },
        { "MonorailSlopes",    &tile_MonorailSlopes },
        { "Tunnels",           &tile_Tunnels },
        { "TrackTunnels",      &tile_TrackTunnels },
        { "DirtTunnels",       &tile_DirtTunnels },
        { "DirtTrackTunnels",  &tile_DirtTrackTunnels },
        { "WaterfallEast",     &tile_WaterfallEast },
        { "WaterfallWest",     &tile_WaterfallWest },
        { "WaterfallNorth",    &tile_WaterfallNorth },
        { "WaterfallSouth",    &tile_WaterfallSouth },
        { "CliffRamps",        &tile_CliffRamps },
        { "PavedRoads",        &tile_PavedRoads },
        { "PavedRoadEnds",     &tile_PavedRoadEnds },
        { "Medians",           &tile_Medians },
        { "RoughGround",       &tile_RoughGround },
        { "DirtRoadJunction",  &tile_DirtRoadJunction },
        { "DirtRoadCurve",     &tile_DirtRoadCurve },
        { "DirtRoadStraight",  &tile_DirtRoadStraight },
        { "DestroyableCliffs", &tile_DestroyableCliffs },
        { "WaterCaves",        &tile_WaterCaves },
        { "WaterCliffs",       &tile_WaterCliffs },
        { "PavedRoadSlopes",   &tile_PavedRoadSlopes },
        { "DirtRoadSlopes",    &tile_DirtRoadSlopes },
        { "Rocks",             &tile_Rocks },
        { "WaterBridge",       &tile_WaterBridge },
        { "BridgeTopLeft1",    &tile_BridgeTopLeft1 },
        { "BridgeTopLeft2",    &tile_BridgeTopLeft2 },
        { "BridgeBottomRight1", &tile_BridgeBottomRight1 },
        { "BridgeBottomRight2", &tile_BridgeBottomRight2 },
        { "BridgeTopRight1",   &tile_BridgeTopRight1 },
        { "BridgeTopRight2",   &tile_BridgeTopRight2 },
        { "BridgeBottomLeft1", &tile_BridgeBottomLeft1 },
        { "BridgeBottomLeft2", &tile_BridgeBottomLeft2 },
        { "BridgeMiddle1",     &tile_BridgeMiddle1 },
        { "BridgeMiddle2",     &tile_BridgeMiddle2 },
    };

    const char* pSection = bTheater ? "General" : "General";

    for (const SpecialTileKey& entry : SpecialKeys) {
        char buffer[0x40];
        pINI->ReadString(pSection, entry.Key, "", buffer, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        if (buffer[0] == '\0')
            continue;

        if (_strcmpi(buffer, pSetName) == 0)
            *entry.Target = ordinal;
    }
}
