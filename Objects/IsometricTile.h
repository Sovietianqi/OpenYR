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
#include <Containers/DynamicVectorClass.h>

#include <cstdint>

// ============================================================================
// Forward declarations
// ============================================================================

class IsometricTileType;
class OverlayClass;
class SmudgeClass;
class CellClass;
class AbstractClass;
class CRCEngine;

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

// ============================================================================
// IsometricTileType - one [TileSet%04d] block of isometr(md).ini
//
//  A tile set groups the art files a theater needs for one category of
//  terrain (clear ground, water, cliffs, ...).  Each set owns TilesInSet
//  tiles; every tile has its own ZAdjust/XOffset/YOffset/AttachesTo triplet
//  keyed with the two digit tile number.
// ============================================================================

class IsometricTileType
{
public:
    IsometricTileType() noexcept;
    ~IsometricTileType() noexcept;

    bool LoadFromINI(class CCINIClass* pINI);

    static void CreateFromINIList(class CCINIClass* pINI, bool bTheater);

    static int32 GetTileSetCount();
    static IsometricTileType* GetTileSet(int32 index);

    // IsometricTileTypeClass_FindIndex - the ordinal of the tile set whose
    // SetName matches pID, or -1.  BuildingTypeClass::ToTile uses it to
    // resolve the "ToTile" key of a building section.
    static int32 FindIndex(const char* pID);

    // ========================================================================
    // asm: bool __thiscall IsometricTileTypeClass_CreateSome(
    //          IsometricTileTypeClass *this, WhereType where)   (asm:714657)
    //
    //   push 0B0h / call new                 ; sizeof(IsometricTileClass)
    //   test eax,eax / jz fail
    //   ecx = where ; edx = [esi+294h]        ; 本类型的索引
    //   push where / push idx ; ecx = eax     ; (__thiscall)
    //   call IsometricTileClass_CTOR
    //   test eax,eax / jz fail
    //   mov al,1 ;; fail: xor al,al ; retn 8
    //
    //   即“按本类型创建一块等距瓦片”：以本类型的 ArrayIndex 作为类型号、
    //   以 where 作为锚点格，构造 IsometricTileClass。★ 名字级缺口
    //   `createsome` -> CreateSome。
    // ========================================================================
    bool CreateSome(const CellStruct& where, int32 a3 = 0);

    // ========================================================================
    // asm 原版命名形态（IsometricTileTypeClass 的其余 proc）
    //
    // 下列 proc 在 asm 中挂在 IsometricTileTypeClass 的 vtable / 邻接槽上，
    // OpenYR 的 IsometricTileType 用等价成员还原（未建模的渲染后端按语义
    // 落空，见各实现注释）。
    // ========================================================================

    // asm: IsometricTileTypeClass_544E00(this, int a2) retn 4 (asm:427)
    //   以 [this+2F0h] 为模取余后，沿 [this+2BCh] 的 AttachesTo 链走 a2 步，
    //   返回第 a2 个链节（链头为本类型自身）。OpenYR 用 AttachesToType。
    IsometricTileType* GetTile(int32 index) const;

    // asm: IsometricTileTypeClass_Get_Cell_Color(this, a2, a3) retn 8 (asm:940)
    //   返回 [ [this+2A8h][a2] + a3*4 ] —— 每块的 (子格) 调色索引。
    uint32 GetCellColor(int32 a2, int32 a3) const;

    // asm: IsometricTileTypeClass_GetCRC(this, buf) retn 4 (asm:595)
    //   先把基类 CRC，再把 [294h..2F0h] 的一批字段逐个喂入。
    int32 GetCRC(CRCEngine& crc) const;

    // asm: IsometricTileTypeClass_PointerGotInvalid(this, p, removed) retn 8
    //   (asm:892) —— 若 [2BCh]==p 则清 0；若 [2C8h] 指向的 AnimType==p 则置 -1。
    void PointerGotInvalid(AbstractClass* pInvalid, bool removed);

    // asm: IsometricTileTypeClass_GetAbstractDerivationID -> 0x12 (asm:988)
    int32 GetAbstractDerivationID() const;

    // asm: IsometricTileTypeClass_GetSize -> 0x30Ch (asm:1017)
    int32 GetSize() const;

    // asm: sub_54A160 -> [this+294h]（本类型的索引）
    int32 GetArrayIndex() const;

    // asm: sub_544CB0(this) (asm:349) —— 惰性取回本类型的图像数据指针。
    void* GetImageData();

    // asm: sub_544D30(this, out) retn 4 (asm:406) —— 把脚印偏移表哨兵
    //   0x7FFF7FFF 写进 out，返回全局表头。
    const CellStruct* GetFoundationData(CellStruct* pOut) const;

    // asm: sub_549B50(dst, src) retn 8 (asm:578) —— 拷贝 3 个 dword。
    static void Copy3Dwords(void* pDst, const void* pSrc);

    // asm: IsometricTileTypeClass_ReturnFalse (asm:479) —— 恒假槽。
    static bool ReturnFalse();

    // IsometricTileTypeClass_CreateFromINIList - the tail of the walk: after a
    // tile set has been created, every [General] special-tile key whose value
    // equals this set's SetName has its ordinal updated to the running tile
    // count.  bTheater selects the [General] block that was read (the snow
    // theater fills from the MD.INI, the temperate one from the main INI).
    static void Apply_Special_Tile_Indices(CCINIClass* pINI, bool bTheater,
                                           const char* pSetName, int32 globalStart);

    // ── 全局瓦片序号 → (瓦片集, 集内子序号) ────────────────────────────────
    //
    // asm: IsometricTileTypeClass_CreateFromINIList 维护一张
    //      `dword_AA1140[setIndex] = 该集在全局序列中的起始号`
    //      （asm 0x546C30 附近的 `mov dword_AA1140[eax*4], ebx`，
    //        ebx 就是逐集累加 TilesInSet 得到的起点）。
    //
    // 地图文件 [IsoMapPack5] 里存的正是这个**全局序号**；原版在
    // CellClass 里把它叫 IsoTileTypeIndex(+0x38)，判定形如
    // `tile_BridgeSet <= idx < tile_BridgeSet + 0x10`，可见它跨越全部集。
    // 绘制时必须先拆成“集”和“集内帧”，否则整张地图只会画第 0 套。
    static bool Split_Global_Tile_Index(int32 globalIndex,
                                        int32& outSetIndex,
                                        int32& outSubIndex);

    // ---- [TileSet%04d] scalars ----
    int32       TilesInSet;
    int32       LastTilesInSet;
    char        SetName[0x40];
    char        FileName[0x40];
    int32       MarbleMadness;
    int32       NonMarbleMadness;
    bool        Morphable;
    bool        AllowToPlace;
    bool        AllowBurrowing;
    bool        AllowTiberium;
    bool        RequiredForRMG;
    int32       ToSnowTheater;
    int32       ToTemperateTheater;
    bool        ShadowCaster;
    int32       ShadowTiles;

    // ---- per tile arrays (indexed by tile number) ----
    int32*      TileXOffset;
    int32*      TileYOffset;
    int32*      TileAttachesTo;
    int32*      TileZAdjust;

    int32       ArrayIndex;

    // 本瓦片集在**全局瓦片序列**中的起始号（asm dword_AA1140[ArrayIndex]）。
    // 由 CreateFromINIList 按 TilesInSet 累加填入；运行期派生，不进存档。
    int32       GlobalStartIndex;

    // ---- asm 字段（+2A8h 起，供 GetTile / GetCellColor / PointerGotInvalid）----
    // 每块的调色表（asm [this+2A8h]）：外层是 TilesInSet 行的指针数组，
    // 每行指向 4 个子格的 32 位色（asm [row][a3]）。
    uint32**    TileColorTable;
    // AttachesTo 链下一节（asm [this+2BCh]）。 */
    IsometricTileType* AttachesToType;
    // 动画类型索引（asm [this+2C8h]，-1 表示无）。 */
    int32       AnimTypeIndex;
    // 每块实例数（asm [this+2F0h]，默认 1）。 */
    int32       TilesPerSetInstances;
    // 惰性图像数据指针（asm [this+0A4h]）。 */
    void*       ImageData;
    // 图像加载只尝试一次（素材缺失时避免每帧 fopen 失败重试）。
    bool        ImageLoadAttempted;
};

extern DynamicVectorClass<IsometricTileType*>* TileTypeClass_Array;
extern int32                                    TileSet_TotalTileCount;
