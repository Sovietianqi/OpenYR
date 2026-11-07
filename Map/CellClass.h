#pragma once

template <class T> class DynamicVectorClass;

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Math/CoordStruct.h>
#include <Math/Facing.h>
#include <Houses/HouseClass.h>

// ============================================================================
// CellClass - Represents a single map cell (tile)
// Inherits AbstractClass in the original, but here we define it as a standalone
// POD-like structure for simplicity, matching the original layout
// ============================================================================

class ObjectClass;
class HouseClass;
class BulletTypeClass;
class BuildingClass;
class BuildingTypeClass;
class TechnoClass;
class OverlayClass;
class SmudgeClass;
class TerrainClass;
class TagClass;
class IStream;
class CRCEngine;

// Cell flags bitfield
enum class CellFlags : uint32 {
    Empty           = 0x0,
    CenterRevealed  = 0x1,
    EdgeRevealed    = 0x2,
    IsWaypoint      = 0x4,
    Explored        = 0x8,
    FlagPresent     = 0x10,
    FlagToShroud    = 0x20,
    IsPlot          = 0x40,
    BridgeOwner     = 0x80,
    BridgeHead      = 0x100,
    HasTiberium     = 0x200,
    BridgeBody      = 0x400,
    BridgeDir       = 0x800,
    PixelFX         = 0x1000,
    IsShrouded      = 0x2000,
    HasOverlay      = 0x4000,
    Veinhole        = 0x8000,
    DrawDarkenIfInAir = 0x10000,
    AnimAttached    = 0x20000,
    Tube            = 0x40000,
    EMPPresent      = 0x80000,
    HorizontalLineEventTag = 0x100000,
    VerticalLineEventTag   = 0x200000,
    Fogged          = 0x400000,

    Revealed        = CenterRevealed | EdgeRevealed,
    Bridge          = BridgeHead | BridgeBody
};

inline CellFlags operator|(CellFlags a, CellFlags b) {
    return static_cast<CellFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
}
inline CellFlags operator&(CellFlags a, CellFlags b) {
    return static_cast<CellFlags>(static_cast<uint32>(a) & static_cast<uint32>(b));
}
inline CellFlags operator~(CellFlags a) {
    return static_cast<CellFlags>(~static_cast<uint32>(a));
}

// Alt cell flags
enum class AltCellFlags : uint32 {
    CellMarked      = 0x1,
    ContainsBuilding    = 0x2,
    HasTerrain      = 0x4,
    Mapped              = 0x8,
    NoFog               = 0x10,
    HasSmudge       = 0x20,
    IsPassable      = 0x40,
    IsIrradiated    = 0x80,
    IsBridged       = 0x100,

    Clear               = Mapped | NoFog
};

inline AltCellFlags operator|(AltCellFlags a, AltCellFlags b) {
    return static_cast<AltCellFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
}
inline AltCellFlags operator&(AltCellFlags a, AltCellFlags b) {
    return static_cast<AltCellFlags>(static_cast<uint32>(a) & static_cast<uint32>(b));
}

// ============================================================================
// CellClass
// ============================================================================
class CellClass {
public:
    static constexpr int32 LeapArray_Step = 256;
    static constexpr int32 CellWidth = 256;
    static constexpr int32 CellHeight = 256;

    CellClass();
    ~CellClass();

    // ========================================================================
    // Coordinate conversion
    // ========================================================================
    static CellStruct Coord2Cell(const CoordStruct& crd);
    static CoordStruct Cell2Coord(const CellStruct& cell);
    static CoordStruct Cell2Coord(int32 x, int32 y);
    static CoordStruct Cell2Coord(int32 cellIndex);

    static int32 Cell2CellIndex(const CellStruct& cell);
    static CellStruct CellIndex2Cell(int32 cellIndex);

    static int32 Coord2CellIndex(const CoordStruct& crd);

    // ========================================================================
    // Terrain checks
    // ========================================================================
    bool IsClear() const;
    bool ContainsWater() const;
    bool IsWater() const;
    bool IsLand() const;
    bool IsRock() const;
    bool IsWall() const;
    bool IsTiberium() const;
    bool IsBridge() const;
    bool IsTunnel() const;
    bool IsRamp() const;
    bool IsRailroad() const;
    bool IsWeeds() const;
    bool IsIce() const;
    bool IsBeach() const;
    bool IsRoad() const;
    bool IsRough() const;
    bool IsOccupied() const;
    bool IsPassable() const;

    bool PassableFor(MovementZone zone) const;
    bool CanEnterTunnelHere() const;

    // ========================================================================
    // Tile-classification predicates (the binary's CellClass_Tile_Is* family)
    //
    // Every one of these reads the cell's isometric tile type index (+0x38)
    // and compares it against the special tile-set ordinals the tileset INI
    // published into the tile_* globals.  A zero-length range means the key
    // was -1 in the theater (tile set absent), in which case the predicate is
    // simply false.
    // ========================================================================
    bool Tile_IsWater() const;
    bool Tile_IsBridge() const;
    bool Tile_IsWoodBridge() const;
    bool Tile_IsCliff() const;
    bool Tile_IsDestroyableCliff() const;
    bool Tile_IsRamp() const;
    bool Tile_IsGreen() const;
    bool Tile_IsBlank() const;
    bool Tile_IsNotWater() const;
    bool Tile_IsShorePieces() const;
    bool Tile_IsWet() const;
    bool Tile_IsMiscPave() const;
    bool Tile_IsPave() const;
    bool Tile_IsDirtRoad() const;
    bool Tile_IsPavedRoad() const;
    bool Tile_IsPavedRoadEnd() const;
    bool Tile_IsPavedRoadSlope() const;
    bool Tile_IsMedian() const;
    bool Tile_IsClearToSandLat() const;
    bool Tile_IsATunnel() const;

 // CellClass::Is_Clear_To_Move - the movement-legal cell
    // test used by the map's "find a nearby spot" and unit-placement paths.
    //
    //   SpeedType      - the mover's speed class (SpeedType::Foot ..).
    //   a3             - clear the low 5 bits of the movement field before the
    //                    zero test (the caller wants to ignore certain flags).
    //   a4             - also clear the "crushable" bits.
    //   a5             - the zone the cell is expected to belong to, or -1.
    //   MovementZone   - the mover's movement zone.
    //   a7             - the expected "land" byte at +0x11B, or -1.
    //   boo            - treat the mover as a bridge-layer.
    bool Is_Clear_To_Move(int32 SpeedType, bool a3, bool a4, int32 a5,
                          MovementZone zone, int32 a7, bool boo) const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 FlagPlaced 负责让某一方把旗帜插到这格上：若本格
    // 还没有旗帜，且这一格对插旗方来说是"可通行且未被占据"的，就置上旗帜
    // 标记位，并把旗帜归属记为该方；否则什么都不做。返回是否插旗成功。
    // ------------------------------------------------------------------------
    bool FlagPlaced(int32 idxHouse);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ShouldDrawObjectsCloaked 负责判断"这一格上的隐形
    // 单位是否应该对观众方显形"：如果这一格正被观众方的隐身发生器覆盖，
    // 且观众方就是发生器拥有者，或者该方的探测计数大于零，则应当显形。
    // ------------------------------------------------------------------------
    bool ShouldDrawObjectsCloaked(int32 idxHouse) const;

    // Radiation
    //
    // RadLevel is the accumulated radiation dose at this cell (a double in the
    // original, at +0xF0); anything above 1.0 makes the cell "radiated" and the
    // RadSiteClass update loop ticks the level down each frame.  RadSite is the
    // owning RadSiteClass pointer (+0xF8).
    void RadLevel_Increase(double amount);
    void RadLevel_Decrease(double amount);
    void Set_Rad_Site(void* pRadSite);
    bool Is_Radiated() const;
    int32 Get_RadLevel() const;

    // Raw per-SpeedType movement cost byte for this cell, taken from the land
    // characteristics table indexed by LandType.
    int32 Get_Movement_Cost(int32 SpeedType) const;

    // The coarse land byte the original stores at +0x11B.  Derived from the
    // land type, since that is what the assembly's table lookups key on.
    int32 Get_Land_Byte() const;

    // Ground and bridge movement bitfields (+0x124 / +0x128).
    uint8 Get_Movement_Field() const;
    uint8 Get_Movement_Field_Bridge() const;

    // Hierarchical pathfinding: route through an 8x8-cell block abstraction
    // first, then refine inside the traversed blocks.  Returns false when no
    // block-level route exists (the caller then falls back to the regular
    // per-cell search).
    static bool Pathfinding_Hierarchical(const CellStruct& from, const CellStruct& to,
                                         DynamicVectorClass<CellStruct>& outPath,
                                         MovementZone zone);

    // ========================================================================
    // Adjacency
    // ========================================================================
    CellClass* AdjacentCell(DirType dir) const;

 // CellClass_ScatterContent.  Moves whatever object is
    // sitting on this cell and its immediate neighbours out of the way, the
    // way a freshly placed structure shoves parked units off its bib.
    void Scatter_Content(int32 a2, int32 a3, bool bIgnoreInfantry);

 // CellClass_AdjustThreat.  Adds `threat` to the cell's
    // accumulated threat for `pFromHouse` (null means "the neutral bucket").
    // FootClass_AddThreatIntoCell / _RemoveThreatFromCell drive it.
    void Adjust_Threat(HouseClass* pFromHouse, int32 threat);

    // ========================================================================
    // Misc checks
    // ========================================================================
    bool Cell_Seems_Ok() const;
    bool Goodie_Check() const;
    int32 GetContainedTiberiumValue() const;

    // ========================================================================
    // 根据游戏行为，可知 BlowUpBridge 炸断本格上的桥：桥体标记翻成已炸断、
    //  地形变成断口、通知重绘并抛出"桥被炸断"事件。
    // ========================================================================
    bool BlowUpBridge();

    // ========================================================================
    // 根据游戏行为，可知格子的颜色分量按"红绿蓝"打包存放在格上：光照与
    //  电磁表现改写它，读侧拆包、写侧重新打包。
    // ========================================================================
    void GetColourComponents(uint8* pR, uint8* pG, uint8* pB) const;
    void SetColourComponents(uint8 r, uint8 g, uint8 b);

    // ========================================================================
    // 根据游戏行为，可知 Clear_Icon_4814F0 清掉本格的地块图标：叠加物一并
    //  清掉，但地面类型与占用情况保持不变。
    // ========================================================================
    void Clear_Icon_4814F0();

    // ========================================================================
    // 根据游戏行为，可知 LAT 给出本格的地表平均类型（贴图分组编号），寻路
    //  与放置判定拿它判断相邻格是否同组。
    // ========================================================================
    int32 LAT() const;
    bool IsShrouded() const;
    bool IsFogged() const;
    bool IsRevealed() const;

    // ========================================================================
    // Flag helpers
    // ========================================================================
    bool HasFlag(CellFlags flag) const {
        return (static_cast<uint32>(Flags) & static_cast<uint32>(flag)) != 0;
    }
    void SetFlag(CellFlags flag, bool value) {
        if (value)
            Flags = static_cast<CellFlags>(static_cast<uint32>(Flags) | static_cast<uint32>(flag));
        else
            Flags = static_cast<CellFlags>(static_cast<uint32>(Flags) & ~static_cast<uint32>(flag));
    }

    bool HasAltFlag(AltCellFlags flag) const {
        return (static_cast<uint32>(AltFlags) & static_cast<uint32>(flag)) != 0;
    }
    void SetAltFlag(AltCellFlags flag, bool value) {
        if (value)
            AltFlags = static_cast<AltCellFlags>(static_cast<uint32>(AltFlags) | static_cast<uint32>(flag));
        else
            AltFlags = static_cast<AltCellFlags>(static_cast<uint32>(AltFlags) & ~static_cast<uint32>(flag));
    }

    // ========================================================================
    // Extended cell management API (implemented in CellClass.cpp)
    // ========================================================================

    // Initialization / state management
    void Init();
    void Recalc_Attributes();

    // Serialization
    bool Load(IStream* pStm);
    bool Save(IStream* pStm) const;
    void Compute_CRC(CRCEngine& crc) const;

    // Coordinate accessors
    CoordStruct Get_CellCoords() const;
    CoordStruct Get_Cell_Position() const;
    Point2D Get_Cell_Screen_Position(int32 originX, int32 originY) const;

    // Map bounds / visibility
    bool Is_On_Map() const;
    bool Is_Visible() const;
    bool Is_Discovered() const;

    // Tiberium management
    int32 Get_Tiberium_Type() const;
    int32 Get_Tiberium_Value() const;
    void Set_Tiberium(int32 type, int32 value);

    // The binary's CellClass_GetContainedTiberiumIndex: resolves the overlay
    // the cell carries into the tiberium type ordinal, or -1.
    int32 Get_Contained_Tiberium_Index() const;

    // CellClass_TiberiumInCell - the ore amount the cell holds: the tiberium
    // type's Value multiplied by (growth stage + 1).  Zero when the cell's
    // overlay is not ore.
    int32 Tiberium_In_Cell() const;

    // CellClass_ContainsTiberium - the bare land-type test the harvest code
    // performs before it bothers resolving the ore type.
    bool Contains_Tiberium() const;

    // ------------------------------------------------------------------------
    // CellClass_Reduce_Tiberium (原版子程序，被 Damage_At_Cell 调用)
    //   把格子上 tiberium 的生长阶段（OverlayFrame）按 amount 递减到 0；
    //   归零时清除格子上的 tiberium overlay 与相关标记。
    // ------------------------------------------------------------------------
    void Reduce_Tiberium(int32 amount);

    // ------------------------------------------------------------------------
    // CellClass_Reduce_Wall (原版子程序，被 Damage_At_Cell 调用)
    //   amount < 0 表示"绝对摧毁"（无视强度直接清墙）；否则按 amount 扣减
    //   墙的耐久，扣完即清除墙 overlay 并把该格标记为可通行。
    // ------------------------------------------------------------------------
    void Reduce_Wall(int32 amount);

    // Overlay management
    int32 Get_Overlay() const;
    int32 Get_Overlay_Type() const;
    void Set_Overlay(int32 overlayIndex, int32 overlayData = 0);

    // Smudge management
    int32 Get_Smudge() const;
    int32 Get_Smudge_Type() const;
    void Set_Smudge(int32 smudgeIndex, int32 smudgeData = 0);

    // Terrain object management
    TerrainClass* Get_Terrain() const;

 // CellClass_StopAmbientSound.  Recursively silences the
    // ambient sound of whatever sits on this cell - the building first, and
    // failing that the terrain object.
    void Silence_Attached_Ambient();
    void Set_Terrain(TerrainClass* pTerrain);
    void Clear_Terrain();

    // Land type accessors
    ::LandType Get_Land_Type() const;
    void Set_Land_Type(::LandType land);

    // Height accessors
    int32 Get_Ground_Height() const;
    int32 Get_Z_Height() const;

    // ------------------------------------------------------------------------
    // asm: int __thiscall CellClass_GetHeight(CellClass *this)
    //
    //   The cell's *level number* the gameplay code compares when it asks
    //   whether two cells share a height band (cliffs, ramps, projectile
    //   passes).  It is the signed byte at +0x11B plus, for bridge cells, a
    //   four-level raise so a bridge deck sits above the water beneath it:
    //     level = (signed char)[this+0x11B] + ((flags & 0x80) ? 4 : 0)
    //   The 0x80 bit of the flag word at +0x140 is CellFlags::BridgeOwner.
    // ------------------------------------------------------------------------
    int32 Get_Height() const;

    // ------------------------------------------------------------------------
    // asm: dwXYZ *__thiscall CellClass_GetCoords(CellClass *this, dwXYZ *out)
    //      (vtable slot +0x48)
    //
    //   The cell's own world position: the map coordinate shifted left eight
    //   (one cell) and offset by half a cell (0x80) to reach the cell centre,
    //   with Z taken from GetFloorHeight so a ramp interpolates smoothly.
    //   Writes into the caller's buffer and returns it.
    // ------------------------------------------------------------------------
    CoordStruct* GetCoords(CoordStruct* pOut) const;

    // ------------------------------------------------------------------------
    // asm: byte __thiscall CellClass_IsHidden(CellClass *this)   (asm:309932)
    //
    //   Answers "does this cell conceal a loose techno".  While the game is
    //   running it walks the ground content chain; if a building sits on the
    //   cell the answer is false unless the content count is anything other
    //   than exactly one.  With no building, the answer is simply whether the
    //   content counter at +0x100 is non-zero.
    // ------------------------------------------------------------------------
    bool IsHidden() const;

    // ------------------------------------------------------------------------
    // asm: void *__thiscall CellClass_GetRadSite(CellClass *this)
    //
    //   The owning RadSiteClass pointer stored at +0xF8, or null.
    // ------------------------------------------------------------------------
    void* Get_Rad_Site() const;

    // ------------------------------------------------------------------------
    // asm: wchar_t *__thiscall CellClass_GetUIName(CellClass *this)
    //
    //   The localised display name of the tiberium this cell's overlay is, or
    //   null when the overlay is not ore.  Resolves the overlay through
    //   Is_Overlay_Idx_Tiberium and returns the tiberium type's UIName.
    // ------------------------------------------------------------------------
    const wchar_t* GetUIName() const;

    // ------------------------------------------------------------------------
    // asm: int __thiscall CellClass_GetAbstractDerivationID(CellClass *this)
    //
    //   The fixed AbstractType derivation ordinal every cell reports (0x0B,
    //   AbstractType::Cell).
    // ------------------------------------------------------------------------
    int32 Get_AbstractDerivationID() const;

    // ------------------------------------------------------------------------
    // asm: int __thiscall CellClass_GetSize(CellClass *this)
    //
    //   The serialised size of a CellClass (0x148 bytes).
    // ------------------------------------------------------------------------
    int32 Get_Size() const;

    // ------------------------------------------------------------------------
    // asm: char __fastcall CellClass_486FF0(CellClass *this)   (asm:308079)
    //
    //   The subterranean-movement cell gate.  A burrowing unit asks whether it
    //   may surface here; the answer is true only when the cell is not map-
    //   tainted, its isometric tile permits burrowing (+0x305 AllowBurrowing),
    //   it has no overlay frame, its flag word carries neither bit 0x100 nor
    //   0x500, and neither a building nor a terrain object occupies it.
    // ------------------------------------------------------------------------
    bool Is_Clear_For_Subterranean() const;

    // ========================================================================
    // 投射物落点判定
    //
    //  根据游戏行为，可知 CanTarget 判断一枚飞来的投射物能不能在这个格子上
    //  生效，返回真正生效的那一格。判定分两类：
    //    * 会被悬崖挡住的投射物：比较落点与入射格的高度层号，落差过大时拒绝。
    //    * 会被墙/障碍挡住的投射物：比较层号、阻挡方归属，同阵营的墙不挡自己。
    //  另外还要考虑"两格宽"的投射物在台阶处的连通性；全部通过则返回落点格，
    //  否则返回空。
    // ========================================================================
    static CellClass* CanTarget(
        CellClass* pSourceCell, int32 x, int32 y, int32 z,
        BulletTypeClass* pProjType, HouseClass* pHouseOwner);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 Get_Containing_Rect 负责返回"这一格叠加物在屏幕上
    // 占据的矩形"。叠加物缺省(-1)时返回空矩形；否则按叠加物类型记录里的
    // 尺寸字段与当前叠加物帧换算出一个以格右上角为基准的矩形。泰矿/矿脉
    // 等分块叠加物会按其帧序切成若干小格，因此矩形可能只覆盖格子的一部分。
    // 本项目暂只还原"叠加物缺省返回空矩形、以及常规叠加物按其尺寸+帧序
    // 换算"的主干，矿脉分块的细分贴图索引留待渲染管线建模后补齐。
    // ------------------------------------------------------------------------
    RectangleStruct Get_Containing_Rect() const;

    // ------------------------------------------------------------------------
    // asm: CellClass_Update_Cell?? (真实入口 0x47FDFB，retn 4)
    //
    //   把"这一格叠加物图标在屏幕上的四个锚点"算出来写进 arg_0（16 字节，
    //   四个 int32），并把 arg_0 原样返回。叠加物缺省或该类型拿不到图像时
    //   写全零。
    //
    //   逐句（asm 行 294174-294345）：
    //     idx = [cell+44h]                         (Overlay)
    //     if (idx == -1) -> 写 {0,0,0,0} 返回
    //     ovt = OverlayTypeClass[idx]
    //     shape = ovt->[vt+9Ch](ovt)               (取 SHP 图像)
    //     if (!shape) -> 写 {0,0,0,0} 返回
    //     level = (uint8)[cell+11Eh]               (OverlayFrame)
    //     if (ovt->Crate) level = 0
    //     slotCoord = Coord_Move({
    //         X = ((cx*256 + 0x80 + sign*0xFF) >> 8) << 8 ,
    //         Y = ((cy*256 + 0x80 + sign*0xFF) >> 8) << 8 , Z = 0 })
    //         其中 cx = (int16)[cell+24h], cy = (int16)[cell+26h]
    //     Tactical::Coord_To_Pixel(TacticalMap, &slotCoord, &pix)
    //     sub_480110(cell, &slotCoord)             (叠加物帧偏移)
    //     pix.x += slotCoord.x - 0x1E ; pix.y += slotCoord.y
    //     if ([cell+140h] & 0x80 && 9 <= level <= 0x11):
    //         pix.x -= 0x0F ; pix.y += 7
    //     w = (int16)[shape+6] ; h = (int16)[shape+2] ; dpt = (int16)[shape+4]
    //     half = (w + (w<0)) / 2 + level           (向零取整)
    //     pix.x -= (h + (h<0)) / 2
    //     pix.y -= (dpt + (dpt<0)) / 2
    //     sub_69E7E0(shape, &tmp, half)            (返回 4 个 int32)
    //     out[i] = tmp[i] + {pix.x, pix.y, 0, 0}
    //
    //   注 1：+0x140 是格标志字（本项目 CellClass::Flags），asm 测的 bit0x80
    //         在 CellFlags 里对应 BridgeOwner。
    //   注 2：asm 的输出是 4 个 int32 的连续缓冲（局部 Point3D，但装的是
    //         整数像素偏移，第 4 个 dword 恒为 0）。本项目用 CoordStruct
    //         （三个 int32）承接 —— 语义等价，且不用把 float 当 int 用。
    //   注 3：sub_480110（叠加物帧偏移）已按 asm 落地，见
    //         Get_Overlay_Draw_Offset()；sub_69E7E0（SHP 帧顶点查询）
    //         语义等于 SHPStruct::GetFrameBounds（取帧记录的 x/y/w/h）。
    // ------------------------------------------------------------------------
    CoordStruct* Update_Cell(CoordStruct* pOut);

    // ------------------------------------------------------------------------
    // asm: sub_480110 (0x480110) + 它内部调的 sub_5FDCC0 (0x5FDCC0)
    //
    //   sub_5FDCC0(overlayIdx)  —— 覆盖物类型自带的纵向偏移：
    //       esi = OverlayTypeClass[idx]; off = 0
    //       if (esi->Tiberium(+0x2A9) || esi->Wall(+0x2A8) ||
    //           esi->HeapID(+0x294) == 0x7E) off = -0x0C
    //       if (esi->LandValue(+0x298) == 9) --off
    //       if (idx == 0x7E) --off
    //       返回 (0, off)
    //
    //   sub_480110(cell) —— 在它之上再叠三项：
    //       if ([cell+140h] & 0x80)  { off -= 0x10
    //                                  if (9 <= Powerup(+0x11E) <= 0x11) off -= 0x0F }
    //       else if (Overlay == 0xEF) off -= 0x0F
    //       off += rect_mouseclass_vp.L - Level(+0x11B) * 15
    //       off += 0x0F
    //       返回 (0x1E, off)            // 调用方再减 0x1E，净 X 偏移恒为 0
    //
    //   本函数返回**去掉 (0x1E, 0x0F) 尾项**的净偏移：那两项由
    //   OverlayTypeClass::Draw 自己加（它内部就是 +0x1E/+0x0F），
    //   Update_Cell 里则单独处理，避免重复。
    // ------------------------------------------------------------------------
    Point2D Get_Overlay_Draw_Offset() const;

 // 根据游戏行为，可知 GetCoords_unknown2 负责下面这段逻辑。
    //
    //  Coordinate getter used by the vtable slot at +0xA4.  It forwards the
    //  cell's own GetCoords and, when the cell flag word (+0x140) has bit
    //  0x100 set, raises the returned Z by the global cell-height bias
    //  (dword_89E7B4).  The bias is a runtime-tuned constant published when
    //  the isometric tables are built; the project stores it as the nominal
    //  level height.
    CoordStruct* GetCoordsUnknown2(CoordStruct* pCoords) const;

 // 根据游戏行为，可知 IsVeins 负责下面这段逻辑。
    //
    //  True while the cell can grow vein overlay: the overlay frame byte
    //  (+0x11C) must be at most 4, the land byte (+0x117) must not be one of
    //  the four vein-hostile land types (2, 3, 6, 8), and the overlay at
    //  +0x44 must either be absent (-1) or a vein-type overlay.  The binary
    //  reads the "is vein" byte out of the OverlayTypeClass record at +0x2AE.
    bool IsVeins() const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ActivateVeins 负责给踩在矿脉上的单位"放血"：
    //  它对格子自身的条件(叠加物必须是矿脉、生长阶段达标、叠加物帧未满、
    //  且格子上还没有激活标记)做一次筛选，然后顺着该格的对象链遍历；
    //  凡是在地面上、且其类型没有"免疫矿脉"、也没有 VEIN_PROOF 能力的单位，
    //  都会在格心生成一个矿脉攻击动画；只要本格处理过，就给格子打上
    //  "已激活"标记，避免重复触发。
    // ------------------------------------------------------------------------
    void ActivateVeins();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 SetupVeins 负责把一格矿脉重新铺设成完整形态：
    //  先确认本格确实是矿脉宿主，再取其包围矩形与一格尺寸做裁剪，然后按
    //  包围范围从后往前逐格重铺；最后再跑一遍激活，让站在矿脉上的单位
    //  重新吃到矿脉伤害。
    // ------------------------------------------------------------------------
    void SetupVeins();

    // ------------------------------------------------------------------------
    // asm: sub_485390 —— 四个正交邻格里有几格还是"矿脉"（返回 4 位掩码，
    //   bit i 对应方向 2i）。判据（逐句 304290-304365）：
    //     起点 = 方向 2i 的邻格（i*2 >= 8 时退回本格）；
    //     邻格 Overlay == -1                                   -> 该位不置
    //     邻格 +0x11C != 0 且 OverlayType[Overlay].IsVeins != 0 -> 该位不置
    //     Overlay != 7Eh 且 OverlayType[Overlay].IsVeins == 0  -> 该位不置
    //     Overlay == 7Eh 且 OverlayFrame < 0x30                -> 该位不置
    //   其余情况置 bit i。结果作为 +0x11E 的随机生长阶段的乘数使用。
    // ------------------------------------------------------------------------
    uint8 Vein_Neighbor_Mask() const;

    // ------------------------------------------------------------------------
    // asm: sub_485590 —— 一格矿脉叠加物的"重接"：把本格的叠加物帧清零，按
    //   邻格矿脉掩码重新掷一个生长阶段（掩码为 0 或本格已不可长矿脉时把
    //   叠加物整格撤掉），刷新该格并重画；随后对每个正交邻格做同样的重接
    //   （只处理 +0x11C == 0、仍是 7Eh、阶段 < 0x30 的邻格）。
    //   asm 里同时算的两组脏矩形（Update_Cell / GetContainingRect /
    //   TacticalMap_RegisterDirtyArea）在本工程由 RegisterDirtyArea 一次完成。
    // ------------------------------------------------------------------------
    void Relink_Vein_Overlay();
    // ------------------------------------------------------------------------
    // asm: char __thiscall CellClass::Spread_Tiberium(CellClass *this, char a2)   (asm:6763)
    //
    //   Grows the ore this cell carries onto a random neighbouring cell.  When
    //   a2 is clear the cell must already hold a growth stage above half the
    //   ore type's ordinal, be flat (slope index 0), carry a non-trivial ore
    //   type and have an empty content chain; the search then walks all eight
    //   neighbours (random start) and seeds the first one CanAddTiberium
    //   accepts.  Returns whether a seed was placed.
    // ------------------------------------------------------------------------
    bool Spread_Tiberium(bool a2);

    // Occupier management
    int32 Get_Occupier_Count() const;
    ObjectClass* Get_Occupier() const;
    void Add_Occupier(ObjectClass* pObj);
    void Remove_Occupier(ObjectClass* pObj);

    // ------------------------------------------------------------------------
    // asm: void __thiscall CellClass::Occupy_Down(CellClass *this, ObjectClass *Content, char bridge)   (asm:3559)
    //
    //   Places `Content` onto the cell's ground (or, when `bridge` is set, its
    //   altitude) occupancy chain: a building is appended to the tail so it is
    //   never hidden by loose units, everything else is prepended.  When the
    //   cell is already revealed the object is told it entered the map
    //   (vtable +0xC0 / +0xF0 placement notification).
    // ------------------------------------------------------------------------
    void Occupy_Down(ObjectClass* pContent, bool bridge);

    // Occupier type checks
    bool Has_Unit() const;
    bool Has_Building() const;
    bool Has_Infantry() const;

    // ------------------------------------------------------------------------
    // The building sitting on this cell, or null.  Walks the ground content
    // chain looking for the AbstractType::Building entry, the same way
    // GetUnit / GetInfantry / GetAircraft select their type.  Used by trigger
    // actions that sell or toggle the power of a building by coordinate.
    // ------------------------------------------------------------------------
    BuildingClass* Get_Building() const;

    // ========================================================================
    // Occupancy-list lookups
    //
    //  The binary threads every object on a cell onto one of two singly-linked
    //  lists through ObjectClass::NextObject (+0x30): the ground list (+0xE4)
    //  and the altitude list (+0xE8).  The Get* helpers pick a list from their
    //  bool argument and return the first entry whose WhatAmI() matches.
    // ========================================================================
    ObjectClass* First_Object(bool alt = false) const;
    ObjectClass* GetUnit(bool alt = false) const;
    ObjectClass* GetAircraft(bool alt = false) const;
    ObjectClass* GetInfantry(bool alt = false) const;

    // ------------------------------------------------------------------------
    // The binary's CellClass::Cell_* typed-chain lookups (asm:1609 / :1740 /
    // :1802).  Each walks one of the two occupancy chains (selected by the
    // `alt` flag) and returns the first entry of the requested kind.
    //
    //   Cell_Building()  - first BuildingClass on the ground chain.
    //   Cell_Abstract()  - first entry whose WhatAmI() == objId.
    //   Cell_Techno()    - the *nearest* techno object to pCoords, measured in
    //                      the low byte of the object's world offset; the
    //                      squared distance is square-rooted and floored, the
    //                      smallest wins.  pSkip (if non-null) is excluded.
    // ------------------------------------------------------------------------
    BuildingClass* Cell_Building() const;
    ObjectClass*   Cell_Abstract(int32 objId, bool alt) const;
    ObjectClass*   Cell_Techno(CellStruct* pCoords, bool alt, ObjectClass* pSkip) const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 GetAircraftOrTerrain 负责为需要"落点"的对象找一个
    //  可用目标：先把格子上非空的飞机挑出来（非地面层里的飞机），没有的话
    //  再按"是否允许地形"回退到格子上可被占用的地形物；两者都没有就返回空。
    // ------------------------------------------------------------------------
    ObjectClass* GetAircraftOrTerrain(int32 a2, bool a3);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 RemoveContent 负责把某个对象从本格的对象链上摘除：
    //  按 alt 标志选择要操作的链（地面链/空中链），命中链首时直接换链首，
    //  否则沿链找到前驱并跳过目标；随后按目标类型做一些收尾（例如步兵离开
    //  时通知其所在位置更新）。目标不属于本格或为空则什么都不做。
    // ------------------------------------------------------------------------
    void RemoveContent(ObjectClass* pObj, bool alt);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 SetWallOwner 负责在格子上出现"墙类叠加物"时，找出
    //  附近最近的一座、由本格可见的墙归属建筑，把它记为墙的拥有者；找不到
    //  合适的建筑则清空归属。
    // ------------------------------------------------------------------------
    void SetWallOwner();

    // ========================================================================
    // Terrain / tiberium helpers
    // ========================================================================
    bool  CanAddTiberium() const;

    // Foundation
    bool Is_Foundation() const;
    bool Get_Foundation() const;

    // Radar color
    int32 Cell_Color() const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 Cell_Color_2 负责给出"地板/地面"这一层在雷达图上
    //  的配色：地图上凡是画着地表的格子，雷达上统一用一套偏灰的深色调表示，
    //  这样即便地表具体类型各异，雷达看起来也是一致的。给定一个缓冲，函数
    //  把颜色写进去并返回。
    //  没有地表的格子返回假，不写任何颜色。
    // ------------------------------------------------------------------------
    bool Cell_Color_2(uint8* pColorOut, int32 a3) const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ConvertCoords 负责把"格子坐标"换算成该格中心点的
    //  世界坐标：格号左移 8 位得到格子左上角，再加半格（128）即得中心；
    //  Z 取该格的地面高度。返回填好的坐标指针。
    // ------------------------------------------------------------------------
    CoordStruct* ConvertCoords(CoordStruct* pOut) const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 GetFloorHeight 负责给出指定世界坐标处的地面高度，
    //  结果带有小数部分以便坡道平滑：先把坐标换算成格号，取该格的地面高度，
    //  再按格内的像素偏移做一次线性插值，使斜面上移动的单位高度连续变化。
    // ------------------------------------------------------------------------
    int32 GetFloorHeight(const CoordStruct& coords) const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 GetFogged 负责判断本格在某个拥有者的视野里是否仍
    //  被战争迷雾遮住：逐个检查该拥有者仍然"记得"的格位快照，只要有一处
    //  与本格相符就认为已探明；一处都对不上则说明还是黑的。
    //  战场未处于迷雾规则下时，一律报告未被遮住。
    // ------------------------------------------------------------------------
    bool GetFogged(HouseClass* pHouse) const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 PickInfantrySublocation 负责在一格之内给新生成的
    //  步兵挑一个落脚点：把整格再细分成若干小位置，逐个试探哪些还空着、且
    //  允许步兵站立，从中选一个可用的返回；全都被占满就报告失败。
    // ------------------------------------------------------------------------
    bool PickInfantrySublocation(CoordStruct* pOut, const CoordStruct& coords,
                                 uint8 a4, uint8 a5, uint8 a6) const;

    // Buildability
    bool Can_Build_On() const;

    // ------------------------------------------------------------------------
    // asm: char __thiscall CellClass::Is_Clear_To_Build(CellClass *this, int a2,
    //      BuildingTypeClass *a3, HouseClass *a4)   (asm:1990, retn 0Ch)
    //
    //   The authoritative "may a structure be placed here" test.  During
    //   scenario init a debug-map cell that is off-radar rejects; the real
    //   rules are:
    //     * a friendly building on the chain blocks unless it is a laser-fence;
    //     * existing terrain objects block;
    //     * a special "no-block" tile (BuildingTypeClass::ToTile / AllowBurrow)
    //       has its own branch;
    //     * the overlay's wall / tiberium ownership decides whether the wall
    //       belongs to the requesting house (own walls never block);
    //     * finally the crushed movement bits (0x3F of +0x124) and the land's
    //       movement cost are consulted.
    // ------------------------------------------------------------------------
    bool Is_Clear_To_Build(int32 a2, BuildingTypeClass* a3, HouseClass* a4) const;
    bool Is_Buildable() const;
    bool Is_Concrete() const;
    bool Is_Cliff() const;

    // Slope
    int32 Get_Slope() const;
    bool Is_Sloped() const;

    // Damage
    void Apply_Damage(int32 damage, int32 warheadType);

    // Rendering
    void Draw_It(int32 originX, int32 originY) const;

    // Shroud management
    void Set_Shrouded(bool shrouded);
    void Unshroud();

 // CellClass_Setup.  Re-derives this cell's shroud state
    // from the map's current view rectangle: the cell is checked against the
    // visible rect and either re-shrouded (`flag` < 0) or marked fogged.
    // Called for every cell when the player's view is resized.
    void Setup(int32 flag);

    // ------------------------------------------------------------------------
    // Trigger-action helpers (TActionClass call sites).
    //
    //   The original TalkBubble action routes through TechnoClass and the
    //   radar-event action through RadarEventClass; the reconstruction folds
    //   the two per-cell side effects onto CellClass so the trigger layer can
    //   reach them without dragging in the whole event/balloon pipeline:
    //
    //     Show_Talk_Bubble  - raise a dialogue balloon carrying the text over
    //                         whatever techno occupies this cell.
    //     Make_Occupiers_Berzerk - drive every ground-layer techno on the cell
    //                         berserk (the "GoBerzerk" trigger action).
    //     LastRadarEventTime - frame stamp of the last radar event raised here.
    // ------------------------------------------------------------------------
    void  Show_Talk_Bubble(const char* pText);
    void  Make_Occupiers_Berzerk();

    // ========================================================================
    // Per-house sensor / cloak-generator bookkeeping
    //
    //  Every cell tracks, for each house index, whether that house has a
    //  cloak generator covering it (a 32-bit bitmask, asm +0x78), how many of
    //  that house's sensor structures reveal it (a 16-bit counter at
    //  +0x7C + idx*2) and how many of that house's disguise sensors cover it
    //  (a 16-bit counter at +0xAC + idx*2).  The building update code adds and
    //  removes these contributions as structures power on and off.
    // ========================================================================
    void CloakGen_AddHouse(int32 idxHouse);
    void CloakGen_RemHouse(int32 idxHouse);
    bool CloakGen_HasHouse(int32 idxHouse) const;
    void Sensors_AddHouse(int32 idxHouse);
    void Sensors_RemHouse(int32 idxHouse);
    bool Sensors_HasHouse(int32 idxHouse) const;
    void DisguiseSensors_AddHouse(int32 idxHouse);
    void DisguiseSensors_RemHouse(int32 idxHouse);
    bool DisguiseSensors_HasHouse(int32 idxHouse) const;

    // ========================================================================
    // Tunnel / coordinate / identity helpers
    // ========================================================================
 // CellClass_GetTunnel: resolves TubeIndex against the
    // tunnel table and returns the tunnel cell, or null when the index is
    // negative or out of range.
    CellClass* Get_Tunnel() const;
 // CellClass_SetMapCoords: copies the four-byte packed
    // X/Y pair into the cell's +0x24 coordinate slot.
    void Set_Map_Coords(const CellStruct& coords);
 // CellClass_GetAbstractID (xx): the fixed AbstractType ordinal
    // every cell reports (AbstractType::Cell).
    int32 Get_AbstractID() const;

    // ========================================================================
    // Flag / shroud flag helpers
    // ========================================================================
 // CellClass_FlagPickedUp (xx): clears the "flag placed" bit
    // (0x10) of the cell's +0x140 flag word and resets the attached object
    // index at +0x50 to -1.  Returns true when the bit had been set.
    bool Flag_Picked_Up();
 // CellClass_Smth0: decrements the gap counter at +0x130,
    // wrapping a count of exactly 1 back down through zero.
    void Smth0();
 // CellClass_Smth2 (xx): sets bits 0x18 of the shroud flag word
    // and, when the gap counter is still positive, tags 0x20 onto +0x140.
    void Smth2();

    // ========================================================================
    // Properties
    // ========================================================================
    CellStruct      MapCoords;
    int32           CellIndex;
    CellFlags       Flags;
    AltCellFlags    AltFlags;
    // BaseSpacerOfHouses (asm CellClass.BaseSpacerOfHouses): a per-house
    // bitmask recording which houses have marked this cell as part of their
    // base's "spacer" ring (the buildable perimeter a base is allowed to
    // expand into).  BuildingClass_MarkBaseSpace sets the bit for its owner;
    // BuildingClass_UnmarkBaseSpace clears it.
    DWORD           BaseSpacerOfHouses;
    // SensorArrayOfHouses (asm CellClass.SensorArrayOfHouses): the same
    // per-house bitmask scheme, but for sensor coverage.  FootClass's
    // Sensors_AddAt / Sensors_RemoveAt stamp and clear the owner's bit over
    // the unit's sensor circle.
    DWORD           SensorArrayOfHouses;
    ::LandType      Land;
    int32           TileType;
    int32           TileSubIndex;
    int32           Overlay;
    int32           OverlayData;
    int32           Smudge;
    int32           SmudgeData;
    ObjectClass*    Occupier;
    TerrainClass*   Terrain;
    TagClass*       AttachedTag;
    // ── Cell occupancy lists (+0xE4 / +0xE8) ──────────────────────────────
    // The binary threads every object standing on a cell onto one of two
    // singly-linked lists through ObjectClass::NextObject (+0x30): the ground
    // list at +0xE4 and the altitude ("flying") list at +0xE8.  GetUnit /
    // GetAircraft / GetInfantry pick a list via their bool argument and walk
    // it looking for the matching abstract type.
    ObjectClass*    FirstObject;
    ObjectClass*    AltObject;
    int32           CellColor;
    int32           Altitude;
    int32           Slope;

    // 根据游戏行为，可知 Level 表示格子地形的高低"层号"，与高度值不同：层号
    // 是整数台阶，用来比较两个格子是不是处于同一高度层（比如判断悬崖、坡道，
    // 或者投射物能否从一侧越过)。高度是连续的绘图用值，层号则用于玩法判定。
    uint8           Level;
    int32           TiberiumValue;
    double          RadLevel;           // +0xF0
    void*           RadSite;            // +0xF8
    int32           WallOwner;
    int32           CrateType;
    // ContentCount (asm CellClass.field_100): the number of objects the cell
    // currently hosts on its ground chain, maintained by AddContent /
    // RemoveContent.  CellClass_IsHidden tests it for non-zero.
    int32           ContentCount;
    DWORD           unknown_38;
    DWORD           unknown_3C;
    DWORD           unknown_40;
    DWORD           unknown_44;

    // ── Movement / zone state ─────────────────────────────────────────────
    // These back the assembly's per-cell bytes consulted by
    // CellClass::Is_Clear_To_Move: the coarse "passability" byte (+0x11A), the
    // "land" byte (+0x11B), the ground and bridge movement bitfields
    // (+0x124 / +0x128) and the pathfinding zone index (+0xE4).
    uint8           Passability;        // +0x11A
    uint8           LandByte;           // +0x11B
    uint8           OverlayFrame;       // +0x11E (overlay growth stage / frame)
    // VeinStage (+0x11C): the per-cell "vein growth counter" the veinhole code
    // reads.  Non-zero means the cell may no longer host a vein overlay
    // (IsVeins allows it at most 4); CellClass::Relink_Vein_Overlay clears it.
    uint8           VeinStage;
    uint8           MovementField;      // +0x124
    uint8           MovementFieldBridge;// +0x128
    int32           ZoneIndex;          // +0xE4
    int16           TubeIndex;          // +0x118 (index into vec_Tubes, -1 = none)

    // ── Shroud bookkeeping ────────────────────────────────────────────────
    // IsUnderShroud / GapsCoveringCell / Field_12C are the three slots
    // MapClass::Shroud_The_Map clears and re-seeds for every cell on the map.
    bool            IsUnderShroud;      // +0x12C+1 - cell hidden by the shroud
    int32           GapsCoveringCell;   // +0x130   - number of gap generators covering it
    uint32          Field_12C;          // +0x12C   - shroud flag bits (bit 0x18 cleared on reset)

    // ── Per-house sensor / cloak-generator state ──────────────────────────
    // CloakGenMask: bit N set when house N has a cloak generator covering
    // this cell (asm +0x78).  SensedByHouses[idx]: how many of house idx's
    // sensor structures reveal this cell (asm +0x7C).  DisguiseSensedByHouses
    // [idx]: how many of house idx's disguise sensors cover it (asm +0xAC).
    uint32          CloakGenMask;
    uint16          SensedByHouses[HouseClass::MaxHouses];
    uint16          DisguiseSensedByHouses[HouseClass::MaxHouses];
    uint8           pad_Sensors[2];

    // ── Flag / object-index slots ─────────────────────────────────────────
    // Field_50 is the 32-bit "attached object" index the flag code resets to
    // -1 (asm +0x50).  Field_140 is the cell flag word whose bit 0x10 records
    // that a flag is planted here (asm +0x140).
    int32           Field_50;
    uint32          Field_140;

    // LastRadarEventTime - frame stamp of the most recent radar event raised
    // on this cell (TActionClass::Action_CreateRadarEvent).  The original
    // stores this on RadarEventClass; the reconstruction keeps it per cell.
    int32           LastRadarEventTime;

    CellClass*      AdjacentCells[8]; // N, NE, E, SE, S, SW, W, NW
};

// The tunnel table the tunnel predicates index.  CellClass_Tile_IsATunnel
// bounds its tube index against this vector's length before it consults the
// cell's land type.
extern int32            TubeCount;
extern DynamicVectorClass<CellClass*>* vec_Tubes;

// ============================================================================
// Coordinate conversion implementations
// ============================================================================
inline CellStruct CellClass::Coord2Cell(const CoordStruct& crd) {
    return CellStruct(
        static_cast<int16>(crd.X / LeapArray_Step),
        static_cast<int16>(crd.Y / LeapArray_Step)
    );
}

inline CoordStruct CellClass::Cell2Coord(const CellStruct& cell) {
    return CoordStruct(
        static_cast<int32>(cell.X) * LeapArray_Step,
        static_cast<int32>(cell.Y) * LeapArray_Step,
        0
    );
}

inline CoordStruct CellClass::Cell2Coord(int32 x, int32 y) {
    return CoordStruct(x * LeapArray_Step, y * LeapArray_Step, 0);
}

inline CoordStruct CellClass::Cell2Coord(int32 cellIndex) {
    return Cell2Coord(CellIndex2Cell(cellIndex));
}

inline int32 CellClass::Cell2CellIndex(const CellStruct& cell) {
    return (static_cast<int32>(cell.Y) << 9) + static_cast<int32>(cell.X);
}

inline CellStruct CellClass::CellIndex2Cell(int32 cellIndex) {
    return CellStruct(
        static_cast<int16>(cellIndex & 0x1FF),
        static_cast<int16>((cellIndex >> 9) & 0x1FF)
    );
}

inline int32 CellClass::Coord2CellIndex(const CoordStruct& crd) {
    return Cell2CellIndex(Coord2Cell(crd));
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
inline CellClass::CellClass()
    : MapCoords(0, 0)
    , CellIndex(-1)
    , Flags(CellFlags::Empty)
    , AltFlags(AltCellFlags::Clear)
    , BaseSpacerOfHouses(0)
    , SensorArrayOfHouses(0)
    , Land(::LandType::Clear)
    , TileType(0)
    , TileSubIndex(0)
    , Overlay(-1)
    , OverlayData(0)
    , Smudge(-1)
    , SmudgeData(0)
    , Occupier(nullptr)
    , Terrain(nullptr)
    , AttachedTag(nullptr)
    , FirstObject(nullptr)
    , AltObject(nullptr)
    , CellColor(0)
    , Altitude(0)
    , Slope(0)
    , TiberiumValue(0)
    , RadLevel(0.0)
    , RadSite(nullptr)
    , WallOwner(-1)
    , CrateType(0)
    , ContentCount(0)
    , unknown_38(0)
    , unknown_3C(0)
    , unknown_40(0)
    , unknown_44(0)
    , Passability(0)
    , LandByte(0)
    , OverlayFrame(0)
    , VeinStage(0)
    , MovementField(0)
    , MovementFieldBridge(0)
    , ZoneIndex(-1)
    , TubeIndex(-1)
    , CloakGenMask(0)
    , Field_50(-1)
    , Field_140(0)
    , LastRadarEventTime(0)
{
    for (int32 i = 0; i < 8; ++i) AdjacentCells[i] = nullptr;
    for (int32 i = 0; i < HouseClass::MaxHouses; ++i)
    {
        SensedByHouses[i] = 0;
        DisguiseSensedByHouses[i] = 0;
    }
}

inline CellClass::~CellClass() {
}

// ========================================================================
// Terrain checks
// ========================================================================
inline bool CellClass::IsClear() const {
    return Land == ::LandType::Clear;
}

inline bool CellClass::ContainsWater() const {
    return Land == ::LandType::Water;
}

inline bool CellClass::IsWater() const {
    return Land == ::LandType::Water;
}

inline bool CellClass::IsLand() const {
    return Land != ::LandType::Water && Land != ::LandType::Rock;
}

inline bool CellClass::IsRock() const {
    return Land == ::LandType::Rock;
}

inline bool CellClass::IsWall() const {
    return Land == ::LandType::Wall;
}

inline bool CellClass::IsTiberium() const {
    return Land == ::LandType::Tiberium;
}

inline bool CellClass::IsBridge() const {
    return HasFlag(CellFlags::Bridge);
}

inline bool CellClass::IsTunnel() const {
    return Land == ::LandType::Tunnel;
}

inline bool CellClass::IsRamp() const {
    return Slope > 0;
}

inline bool CellClass::IsRailroad() const {
    return Land == ::LandType::Railroad;
}

inline bool CellClass::IsWeeds() const {
    return Land == ::LandType::Weeds;
}

inline bool CellClass::IsIce() const {
    return Land == ::LandType::Ice;
}

inline bool CellClass::IsBeach() const {
    return Land == ::LandType::Beach;
}

inline bool CellClass::IsRoad() const {
    return Land == ::LandType::Road;
}

inline bool CellClass::IsRough() const {
    return Land == ::LandType::Rough;
}

inline bool CellClass::IsOccupied() const {
    return Occupier != nullptr || HasAltFlag(AltCellFlags::ContainsBuilding);
}

inline bool CellClass::IsPassable() const {
    return !IsOccupied() && !IsWall() && !IsRock();
}

inline bool CellClass::PassableFor(MovementZone zone) const {
    if (IsWall() || IsRock()) return false;
    if (zone == MovementZone::Water || zone == MovementZone::WaterBeach) {
        return IsWater() || IsBeach();
    }
    if (zone == MovementZone::Amphibious || zone == MovementZone::AmphibiousCrusher ||
        zone == MovementZone::AmphibiousDestroyer) {
        return true;
    }
    if (zone == MovementZone::Fly) return true;
    return !IsWater();
}

inline bool CellClass::CanEnterTunnelHere() const {
    // asm CellClass_CanEnterTunnelHere (asm:7765): the cell grants tunnel
    // access when its tube index is a live entry of vec_Tubes and its land
    // byte is the Tunnel classification.  The binary then walks the eight
    // neighbours looking for an adjoining tunnel; the direct hit is returned
    // first here.
    if (TubeIndex < 0)
        return false;
    if (TubeIndex >= TubeCount)
        return false;
    return Land == ::LandType::Tunnel;
}

// ========================================================================
// Adjacency
// ========================================================================
inline CellClass* CellClass::AdjacentCell(DirType dir) const {
    int32 idx = static_cast<int32>(static_cast<uint8>(dir) >> 5);
    if (idx < 0 || idx > 7) return nullptr;
    return AdjacentCells[idx];
}

// ========================================================================
// Misc checks
// ========================================================================
inline bool CellClass::Cell_Seems_Ok() const {
    return CellIndex >= 0 && !IsWall();
}

inline bool CellClass::Goodie_Check() const {
    return IsClear() && !IsOccupied();
}

inline int32 CellClass::GetContainedTiberiumValue() const {
    return TiberiumValue;
}

inline bool CellClass::IsShrouded() const {
    return !HasFlag(CellFlags::Revealed);
}

inline bool CellClass::IsFogged() const {
    return HasFlag(CellFlags::Fogged);
}

inline bool CellClass::IsRevealed() const {
    return HasFlag(CellFlags::Revealed);
}