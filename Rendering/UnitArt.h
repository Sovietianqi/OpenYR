// =============================================================================
// UnitArt.h - 引擎侧的颜色 / 朝向工具（不含任何图形绘制）
//
//   单位、步兵、建筑、飞机一律由 SHP（CC_Draw_Shape）或 VXL（体素光栅器）
//   绘制。这里只提供调用方需要的纯函数。
// =============================================================================
#pragma once

#include "Core/Definitions.h"

class Surface;
class HouseClass;

namespace UnitArt
{
    // 24 位 RGB -> 16bpp 565。
    uint16 MakeColor16(int32 r, int32 g, int32 b);

    // 按 num/den 的比例压暗（通道向下取整，钳到该通道位宽）。
    uint16 Darken16(uint16 color, int32 num, int32 den);

    // 按 num/den 的比例朝白色提亮。
    uint16 Lighten16(uint16 color, int32 num, int32 den);

    // 取阵营色：HouseClass::ColorSchemeIndex -> ColorScheme::BaseColor。
    //   查不到就返回 fallback。
    uint16 GetHouseColor16(const HouseClass* pOwner, uint16 fallback);

    // DirStruct 原始字节（0..255）-> 8 方向序号（0..7）。
    int32 FacingToDir8(uint8 facing);

    // 本帧战术层的绘制目标面（TacticalClass::Instance->Target_Surface()）。
    Surface* GetTacticalSurface();
}
