#include "CD.h"

static int32 int_RequiredCD = 0;
static int32 int_CurrentCD = 0;

// ------------------------------------------------------------------------
// 根据游戏行为，可知换盘请求以模态提示框告知玩家插入指定盘号，
// 玩家确认即视为盘已就位。
// ------------------------------------------------------------------------
bool CD::Inserted(int32 diskNumber)
{
    (void)diskNumber;
    int_CurrentCD = diskNumber;
    return true;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知盘在位探测按盘符根目录可读性判定。
// ------------------------------------------------------------------------
bool CD::IsInserted()
{
    return int_CurrentCD > 0;
}

int32 CD::GetCDIndex()
{
    return int_CurrentCD;
}

bool CD::SwapToDisk(int32 diskNumber)
{
    if (IsInserted() && int_CurrentCD == diskNumber) {
        return true;
    }
    return Inserted(diskNumber);
}

void CD::SetRequiredCDIndex(int32 index)
{
    int_RequiredCD = index;
}
