#include "Rendering/ObjectUI.h"

#include "Rendering/Surface.h"
#include "Math/Rectangle.h"

namespace ObjectUI
{
namespace
{
void Fill(Surface* pSrf, int32 x, int32 y, int32 w, int32 h, uint16 color)
{
    if (pSrf == nullptr || pSrf->Buffer == nullptr || w <= 0 || h <= 0)
        return;
    Rectangle r(x, y, w, h);
    pSrf->FillRectEx(nullptr, &r, static_cast<DWORD>(color));
}
}  // namespace

// ----------------------------------------------------------------------------
// 血条（原版 TechnoClass::Draw_HealthBar）
//
//   背景是一条 1px 深色底 + 3px 高的彩色条；颜色按血量跳档：
//     >= 50%  绿 0x07E0
//     >= 25%  黄 0xFFE0
//     <  25%  红 0xF800
//   条宽随包围盒宽缩放（原版是固定 PIPS 宽，但这里没有 pips.shp，
//   用包围盒宽夹到 [8,40] 是等价的几何表达）。
// ----------------------------------------------------------------------------
void DrawHealthBar(Surface* pSrf, const RectangleStruct& box, int32 healthPct)
{
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return;

    int32 width = box.Width;
    if (width < 8)  width = 8;
    if (width > 40) width = 40;
    const int32 height = 3;

    const int32 x = box.X + (box.Width - width) / 2;
    const int32 y = box.Y - 6;

    if (healthPct < 0)   healthPct = 0;
    if (healthPct > 100) healthPct = 100;

    // 底槽（全黑）
    Fill(pSrf, x - 1, y - 1, width + 2, height + 2, 0x0000);
    // 空槽（深灰）
    Fill(pSrf, x, y, width, height, 0x4208);

    uint16 color = 0x07E0;
    if (healthPct < 25)      color = 0xF800;
    else if (healthPct < 50) color = 0xFFE0;

    const int32 filled = width * healthPct / 100;
    Fill(pSrf, x, y, filled, height, color);

    // 高光：条顶部 1px 提亮
    if (filled > 0)
        Fill(pSrf, x, y, filled, 1, static_cast<uint16>((color >> 1) | 0x8410));
}

// ----------------------------------------------------------------------------
// 选中角标（原版 Draw_Selection_Box 的四角 L 形）
// ----------------------------------------------------------------------------
void DrawSelectionBrackets(Surface* pSrf, const RectangleStruct& box, uint16 color)
{
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return;

    int32 arm = box.Width / 4;
    if (arm < 4)  arm = 4;
    if (arm > 8)  arm = 8;

    const int32 x0 = box.X;
    const int32 y0 = box.Y;
    const int32 x1 = box.X + box.Width  - 1;
    const int32 y1 = box.Y + box.Height - 1;

    // 左上
    Fill(pSrf, x0, y0, arm, 2, color);
    Fill(pSrf, x0, y0, 2, arm, color);
    // 右上
    Fill(pSrf, x1 - arm + 1, y0, arm, 2, color);
    Fill(pSrf, x1 - 1, y0, 2, arm, color);
    // 左下
    Fill(pSrf, x0, y1 - 1, arm, 2, color);
    Fill(pSrf, x0, y1 - arm + 1, 2, arm, color);
    // 右下
    Fill(pSrf, x1 - arm + 1, y1 - 1, arm, 2, color);
    Fill(pSrf, x1 - 1, y1 - arm + 1, 2, arm, color);
}

}  // namespace ObjectUI
