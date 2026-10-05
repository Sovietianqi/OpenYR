#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

// ------------------------------------------------------------------------
// 根据游戏行为，可知浮点三角面为渲染插值提供单精度入口。
// ------------------------------------------------------------------------
class FloatMath {
public:
    static float tan(float radians);
    static float arcsin(float value);
    static float arccos(float value);
    static float arctan(float value);
};
