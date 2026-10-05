#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

// ------------------------------------------------------------------------
// 根据游戏行为，可知光盘面负责探测盘符、请求换盘与记录所需盘号。
// ------------------------------------------------------------------------
class CD {
public:
    static bool Inserted(int32 diskNumber);
    static bool IsInserted();
    static int32 GetCDIndex();
    static bool SwapToDisk(int32 diskNumber);
    static void SetRequiredCDIndex(int32 index);
};
