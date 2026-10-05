#include "ShapeButtonClass.h"

ShapeButtonClass::ShapeButtonClass(int32 id, GScreenClass* pScreen, int32 shapeFrame)
    : ID(id)
    , X(0)
    , Y(0)
    , Width(0)
    , Height(0)
    , ShapeFrame(shapeFrame)
    , TooltipText(nullptr)
    , IsHovered(false)
{
    (void)pScreen;
}

ShapeButtonClass::~ShapeButtonClass()
{
    TooltipText = nullptr;
}

void ShapeButtonClass::SetDimensions(int32 x, int32 y, int32 width, int32 height)
{
    X = x;
    Y = y;
    Width = width;
    Height = height;
}

void ShapeButtonClass::SetTooltip(const char* pText)
{
    TooltipText = pText;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知擦除只在按钮可见时进行，避免无谓的表面操作。
// ------------------------------------------------------------------------
void ShapeButtonClass::Erase()
{
    IsHovered = false;
}

bool ShapeButtonClass::Draw_Me(bool forced)
{
    return forced || IsHovered;
}
