// =============================================================================
// TileArt.cpp - 地形层的**几何**工具（不含任何贴图内容）
//
// 地形贴图本身来自 theater 的 TMP / IsoTile 图集，由 CC_Draw_Shape 走
// 素材 blit；本文件只保留三类与素材无关的像素原语：
//
//   1) 16bpp 落笔（PutPixel565 / FillRow565）—— 供阴影、雾、网格使用；
//   2) 等距菱形填充（DrawIsoDiamond）—— 用于 shroud 黑菱形；
//   3) 菱形轮廓（DrawIsoTileOutline）—— 用于调试网格。
//
// 几何与原版一致：
//   格子 (cx,cy) 的屏幕中心 = ((cx-cy)*30, (cx+cy)*15)
//   菱形包围盒左上角       = 中心 - (30,15)
//
// 早期这里还有一整套“程序化草地/水波/矿簇/树冠”。那不是复刻——原版地形
// 是美术资源，任何程序化替代都会在导入真实素材后变成两层互相打架的图。
// 已整体删除；地形层只认 Tile_Shape_For() 返回的图集句柄。
// =============================================================================

#include "Rendering/TileArt.h"

#include "Rendering/Surface.h"
#include "Math/Rectangle.h"
#include "Core/Definitions.h"

namespace
{
// -----------------------------------------------------------------------------
// 单像素 / 单行落笔（16bpp XRGB565）
// -----------------------------------------------------------------------------
bool Use_Interface(Surface* pSrf)
{
    return pSrf->GetBytesPerPixel() == 2;
}

void PutPixel565(Surface* pSrf, int32 x, int32 y, uint16 color)
{
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return;
    if (x < 0 || x >= pSrf->GetWidth() || y < 0 || y >= pSrf->GetHeight())
        return;

    if (Use_Interface(pSrf))
    {
        Point2D pt(x, y);
        pSrf->SetPixel(&pt, static_cast<DWORD>(color));
        return;
    }

    uint16* line = reinterpret_cast<uint16*>(
        static_cast<BYTE*>(pSrf->Buffer) + y * pSrf->GetPitch());
    line[x] = color;
}

void FillRow565(Surface* pSrf, int32 x, int32 y, int32 width, uint16 color)
{
    if (pSrf == nullptr || pSrf->Buffer == nullptr || width <= 0)
        return;

    const int32 srfW = pSrf->GetWidth();
    if (y < 0 || y >= pSrf->GetHeight())
        return;
    if (x < 0)
    {
        width += x;
        x = 0;
    }
    if (x + width > srfW)
        width = srfW - x;
    if (width <= 0)
        return;

    if (Use_Interface(pSrf))
    {
        Rectangle line(x, y, width, 1);
        pSrf->FillRectEx(nullptr, &line, static_cast<DWORD>(color));
        return;
    }

    uint16* dst = reinterpret_cast<uint16*>(
        static_cast<BYTE*>(pSrf->Buffer) + y * pSrf->GetPitch()) + x;
    for (int32 i = 0; i < width; ++i)
        dst[i] = color;
}

// tile 图集句柄表。装载器把 theater 的 IsoTile SHP 按类型索引写进来；
// 绘制层只读。原版这一步是 IsometricTileTypeClass::Get_ImageData()，
// 本表是它在渲染层的最小投影。
// 表大小按 [TileSet%04d] 的实际编号空间取（原版可达数千套）。
SHPStruct* g_TileShapes[TileArt::TileShapeTableSize] = {};

}  // namespace

namespace TileArt
{

bool Surface_Is_16bpp(Surface* pSrf)
{
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return false;
    if (pSrf->GetBytesPerPixel() == 2)
        return true;
    // DSurface 的 GetBytesPerPixel 未覆写时仍返回 1，但 Pitch 已是 Width*2。
    return pSrf->GetPitch() == pSrf->GetWidth() * 2;
}

void DrawIsoDiamond(Surface* pSrf, int32 left, int32 top, uint16 color)
{
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return;

    for (int32 row = 0; row < IsoTileHeight; ++row)
    {
        const int32 half = IsoRowHalfWidth(row);
        if (half <= 0)
            continue;
        FillRow565(pSrf, left + IsoHalfWidth - half, top + row, half * 2, color);
    }
}

void DrawIsoTileOutline(Surface* pSrf, int32 left, int32 top, uint16 color)
{
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return;

    for (int32 row = 0; row < IsoTileHeight; ++row)
    {
        const int32 half = IsoRowHalfWidth(row);
        if (half <= 0)
            continue;
        PutPixel565(pSrf, left + IsoHalfWidth - half,     top + row, color);
        PutPixel565(pSrf, left + IsoHalfWidth + half - 1, top + row, color);
    }
}

SHPStruct* Tile_Shape_For(int32 tileType)
{
    if (tileType < 0 || tileType >= TileArt::TileShapeTableSize)
        return nullptr;
    return g_TileShapes[tileType];
}

void Set_Tile_Shape(int32 tileType, SHPStruct* pShape)
{
    if (tileType < 0 || tileType >= TileArt::TileShapeTableSize)
        return;
    g_TileShapes[tileType] = pShape;
}

}  // namespace TileArt
