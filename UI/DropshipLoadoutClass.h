#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class DSurface;
class TechnoTypeClass;

// ============================================================================
// DropshipLoadoutClass —— 运兵船装载清单画面里的"单个单位条目"
//
// asm 形态：`DropshipLoadout_WriteUnit`（thiscall：ecx = 条目上下文，
//   edx = 目标 DC，[esp+4] = 单位类型，[esp+8] = showCost 字节，retn 8）。
//   把单位造型在两块表面之间居中后贴到隐藏表面，再按 Green（显示造价）或
//   Red 配色、以 12Ch / 1C2h 的步距画出 NAME / COST / ARMAMENT / ARMOR 四行
//   说明文字。
// ============================================================================
class DropshipLoadoutClass
{
public:
    // ------------------------------------------------------------------------
    // asm: DropshipLoadout_WriteUnit (0x4B9A80 区)
    //   pShape: 被贴的造型对象（asm 用 vt+80h/7Ch 取宽高）
    //   pDc:    目标 DC
    //   pUnit:  单位类型（名字 / 造价 / 武器 / 护甲都从它读）
    //   showCost: 非零用 Green 配色并画造价，否则用 Red 且写 COST N/A
    // ------------------------------------------------------------------------
    void Write_Unit(DSurface* pShape, DSurface* pDc,
                    const TechnoTypeClass* pUnit, bool showCost);

    // asm: DSurface_Hidden —— 造型先贴到这块隐藏表面再整体翻到屏幕
    static DSurface* HiddenSurface;
};
