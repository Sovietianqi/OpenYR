// =============================================================================
// UnitArt.cpp - 引擎侧的**颜色/朝向工具**（不含任何图形绘制）
//
//   原版的单位 / 步兵 / 建筑 / 飞机都是 .shp 或 .vxl 素材，绘制一律走
//   CC_Draw_Shape（SHP）或体素光栅器（VXL）。这里只保留调用方需要的
//   纯函数：24bit -> RGB565 打包、明暗调整、阵营色查询、朝向换算、取面。
//
//   早期这个文件里还有一整套“基向量光栅化的坦克/步兵/建筑/飞机”。那是
//   程序化替代美术，导入真实素材后会与 SHP/VXL 重叠。已整体删除。
// =============================================================================

#include "Rendering/UnitArt.h"

#include "Rendering/Surface.h"
#include "Rendering/ConvertClass.h"
#include "Rendering/TacticalClass.h"
#include "Houses/HouseClass.h"
#include "Houses/HouseTypeClass.h"

namespace UnitArt
{

// =============================================================================
// 颜色工具
// =============================================================================
uint16 MakeColor16(int32 r, int32 g, int32 b)
{
    r = (r < 0) ? 0 : ((r > 255) ? 255 : r);
    g = (g < 0) ? 0 : ((g > 255) ? 255 : g);
    b = (b < 0) ? 0 : ((b > 255) ? 255 : b);
    return static_cast<uint16>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

uint16 Darken16(uint16 color, int32 num, int32 den)
{
    if (den <= 0) return color;
    int32 r = (color >> 11) & 0x1F;
    int32 g = (color >>  5) & 0x3F;
    int32 b =  color        & 0x1F;
    r = r * num / den;
    g = g * num / den;
    b = b * num / den;
    r = (r < 0) ? 0 : ((r > 31) ? 31 : r);
    g = (g < 0) ? 0 : ((g > 63) ? 63 : g);
    b = (b < 0) ? 0 : ((b > 31) ? 31 : b);
    return static_cast<uint16>((r << 11) | (g << 5) | b);
}

uint16 Lighten16(uint16 color, int32 num, int32 den)
{
    if (den <= 0) return color;
    int32 r = (color >> 11) & 0x1F;
    int32 g = (color >>  5) & 0x3F;
    int32 b =  color        & 0x1F;
    r = r + (31 - r) * num / den;
    g = g + (63 - g) * num / den;
    b = b + (31 - b) * num / den;
    if (r > 31) r = 31;
    if (g > 63) g = 63;
    if (b > 31) b = 31;
    return static_cast<uint16>((r << 11) | (g << 5) | b);
}

// 阵营色：原版走 HouseClass::ColorScheme -> ColorScheme::BaseColor。
//   这里返回 565 版本，供 UI/调试使用；素材本身的阵营色由 ColorScheme 的
//   remap 表 + ConvertClass 完成，不走这条。
uint16 GetHouseColor16(const HouseClass* pOwner, uint16 fallback)
{
    if (pOwner == nullptr) return fallback;

    int32 idx = pOwner->ColorSchemeIndex;
    if (idx < 0 && pOwner->Type != nullptr)
        idx = pOwner->Type->ColorSchemeIndex;

    if (idx < 0) return fallback;
    if (ColorScheme::Array == nullptr) return fallback;
    if (idx >= ColorScheme::Array->Count) return fallback;

    const ColorScheme* pScheme = (*ColorScheme::Array)[idx];
    if (pScheme == nullptr) return fallback;

    return MakeColor16(pScheme->BaseColor.R,
                       pScheme->BaseColor.G,
                       pScheme->BaseColor.B);
}

// DirStruct 的高 3 位就是 8 方向序号（原版 DirStruct::GetFacing8）。
int32 FacingToDir8(uint8 facing)
{
    return static_cast<int32>((facing >> 5) & 0x07);
}

Surface* GetTacticalSurface()
{
    if (TacticalClass::Instance == nullptr) return nullptr;
    return TacticalClass::Instance->Target_Surface();
}

}  // namespace UnitArt
