#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

// ------------------------------------------------------------------------
// 根据游戏行为，可知存档写面由一组同型变体构成：它们都持有存储
// 句柄并按各自参数组合把键值块写入子存储。
// ------------------------------------------------------------------------
class Storage {
public:
    static bool Write0(void* pStorage, int32 a2, int32 a3);
    static bool Write1(void* pStorage, int32 a2, int32 a3, int32 a4, int32 a5, int32 a6, int32 a7);
    static bool Write2(void* pStorage, int32 a2, int32 a3, int32 a4, int32 a5, int32 a6, int32 a7);
    static bool Write3(void* pStorage, int32 a2, const char* pText, int32 a4);
    static bool Write4(void* pStorage, int32 a2, int32 a3, const char* pText, int32 a5, int32 a6, int32 a7, int32 a8);
    static bool Write5(void* pStorage, int32 a2, bool a3, int32 a4, int32 a5, int32 a6);
    static bool Write6(void* pStorage, int32 a2, int32 a3);
};
