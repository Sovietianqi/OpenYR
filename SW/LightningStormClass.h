#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"

// ------------------------------------------------------------------------
// 根据游戏行为，可知闪电风暴的运行面是：延迟期查询、落雷与
// 屏幕播报三件事。
// ------------------------------------------------------------------------
class LightningStormClass {
public:
    static bool HasDeferment();
    static void Strike(const CoordStruct& pos);
    static void PrintMessage(int32 houseIndex);
};
