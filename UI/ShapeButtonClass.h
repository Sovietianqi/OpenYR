#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class GScreenClass;

// ------------------------------------------------------------------------
// 根据游戏行为，可知形状按钮以贴图编号驱动四态绘制，并支持
// 尺寸改写、提示文案与擦除。
// ------------------------------------------------------------------------
class ShapeButtonClass {
public:
    ShapeButtonClass(int32 id, GScreenClass* pScreen, int32 shapeFrame);
    ~ShapeButtonClass();

    void SetDimensions(int32 x, int32 y, int32 width, int32 height);
    void SetTooltip(const char* pText);
    void Erase();
    bool Draw_Me(bool forced);

    int32 ID;
    int32 X;
    int32 Y;
    int32 Width;
    int32 Height;
    int32 ShapeFrame;
    const char* TooltipText;
    bool IsHovered;
};
