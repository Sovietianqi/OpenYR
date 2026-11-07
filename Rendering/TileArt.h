#pragma once

// =============================================================================
// TileArt.h - 地形层的几何工具（不含贴图内容）
//
// 地块贴图来自 theater 的 IsoTile 图集（SHP/TMP），经 CC_Draw_Shape 走素材
// 合成。本命名空间只提供与素材无关的像素原语与图集句柄表：
//   * 等距菱形填充 / 轮廓（shroud、调试网格）
//   * Tile_Shape_For()：按 IsoTileTypeIndex 取图集句柄
// =============================================================================

#include "Core/Definitions.h"

class Surface;
struct SHPStruct;

namespace TileArt
{
// 等距地块尺寸（与原版 IsoTileClass 一致，同 TacticalClass 的同名常量）
constexpr int32 IsoTileWidth  = 60;
constexpr int32 IsoTileHeight = 30;
constexpr int32 IsoHalfWidth  = 30;
constexpr int32 IsoHalfHeight = 15;

// 菱形第 row 行（0..29）的半宽像素数。每行左右各收 2 像素：
// 第 0 行与第 29 行为 2，第 15 行最宽为 30。
inline int32 IsoRowHalfWidth(int32 row) noexcept
{
    if (row < 0 || row >= IsoTileHeight)
        return 0;
    const int32 d = (row < IsoHalfHeight) ? (row + 1) : (IsoTileHeight - row);
    const int32 half = d * 2;
    return (half > IsoHalfWidth) ? IsoHalfWidth : half;
}

// 该面是否确实是 16bpp 565 缓冲。
//
// Surface::GetBytesPerPixel() 在 DSurface 上没有覆写，仍返回基类默认值 1，
// 而 DSurface::Allocate 把 Pitch 设成 Width*2。所以判定要同时看两处。
bool Surface_Is_16bpp(Surface* pSrf);

// 实心等距菱形：逐行算宽度，每行一条 FillRectEx。
void DrawIsoDiamond(Surface* pSrf, int32 left, int32 top, uint16 color);

// 菱形轮廓（网格线）：逐行画左右两个端点像素，1px 宽。
void DrawIsoTileOutline(Surface* pSrf, int32 left, int32 top, uint16 color);

// -----------------------------------------------------------------------------
// tile 图集分流入口
//
// Draw_Overlay 先问这里要该 IsoTileTypeIndex 的贴图句柄：非空走
// CC_Draw_Shape（theater 图集），空则不画。
//
// 原版这一步是 CellClass::Get_Tile_Image() →
// IsometricTileTypeClass::GetImageData()；本表是它在渲染层的最小投影，
// 由地形装载器（TMP/theater 解析）写入。
// -----------------------------------------------------------------------------
// [TileSet%04d] 的编号空间。原版 tile set 可达数千套，按 256 截断会让
// 高编号瓦片集取不到句柄（表现为"后半个地图不画"）。
constexpr int32 TileShapeTableSize = 8192;

SHPStruct* Tile_Shape_For(int32 tileType);
void Set_Tile_Shape(int32 tileType, SHPStruct* pShape);

}  // namespace TileArt
