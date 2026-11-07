#pragma once

// ============================================================================
// ObjectUI.h - 单位/建筑头顶的**引擎 UI**（不是美术资源）
//
// 原版这部分在 TechnoClass::Draw_Extras / Draw_HealthBar 里，全部用
// FillRectEx / DrawLineEx 直接画在战术面上，不依赖任何 SHP。这里保留同样的
// 语义：血条是纯色分段矩形，选中框是四角 L 形亮线。
//
// 注意：这里**不画单位本体**。本体一律走 CC_Draw_Shape（SHP）或体素光栅器。
// ============================================================================

#include "Core/Definitions.h"

class Surface;

namespace ObjectUI
{
// 血条：宽度取包围盒宽（夹到 8..40），3px 高，画在包围盒顶部上方 4px。
//   > 50% 绿、25%..50% 黄、< 25% 红（原版按 Health/MaxHealth 跳档）。
void DrawHealthBar(Surface* pSrf, const RectangleStruct& box, int32 healthPct);

// 选中角标：四角 L 形，每条臂长 = min(6, 宽/4)，亮绿色 0x07E0。
void DrawSelectionBrackets(Surface* pSrf, const RectangleStruct& box, uint16 color);

}  // namespace ObjectUI
