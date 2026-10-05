#include "Storage.h"

static bool Storage_WriteEntry(void* pStorage)
{
    return pStorage != nullptr;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知各写变体在动手前都先解析存储句柄；句柄无效
// 即放弃本次写入。
// ------------------------------------------------------------------------
bool Storage::Write0(void* pStorage, int32 a2, int32 a3)
{
    (void)a2;
    (void)a3;
    return Storage_WriteEntry(pStorage);
}

bool Storage::Write1(void* pStorage, int32 a2, int32 a3, int32 a4, int32 a5, int32 a6, int32 a7)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    return Storage_WriteEntry(pStorage);
}

bool Storage::Write2(void* pStorage, int32 a2, int32 a3, int32 a4, int32 a5, int32 a6, int32 a7)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    return Storage_WriteEntry(pStorage);
}

bool Storage::Write3(void* pStorage, int32 a2, const char* pText, int32 a4)
{
    (void)a2;
    (void)a4;
    if (pText == nullptr) {
        return false;
    }
    return Storage_WriteEntry(pStorage);
}

bool Storage::Write4(void* pStorage, int32 a2, int32 a3, const char* pText, int32 a5, int32 a6, int32 a7, int32 a8)
{
    (void)a2; (void)a3; (void)a5; (void)a6; (void)a7; (void)a8;
    if (pText == nullptr) {
        return false;
    }
    return Storage_WriteEntry(pStorage);
}

bool Storage::Write5(void* pStorage, int32 a2, bool a3, int32 a4, int32 a5, int32 a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return Storage_WriteEntry(pStorage);
}

bool Storage::Write6(void* pStorage, int32 a2, int32 a3)
{
    (void)a2;
    (void)a3;
    return Storage_WriteEntry(pStorage);
}
