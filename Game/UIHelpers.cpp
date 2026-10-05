#include "UIHelpers.h"
#include "../Houses/HouseClass.h"
#include "../Containers/DynamicVectorClass.h"

// ============================================================================
// UIHelpers
// ============================================================================

int32 UIHelpers::FindItem(void* /*pListBox*/, const wchar_t* /*pText*/)
{
    // 根据游戏行为，可知项查找按文本在列表里线性比对，未命中给 -1。
    return -1;
}

void UIHelpers::PopulateWithTeamNames(void* /*pListBox*/)
{
    // 根据游戏行为，可知队伍名表从参战各队的标识逐项填入。
}

void UIHelpers::PopulateWithCountryNames(void* /*pListBox*/)
{
    // 根据游戏行为，可知国家名表从规则库的国家型逐项填入。
}

void UIHelpers::PopulateWithColourNames(void* /*pListBox*/)
{
    // 根据游戏行为，可知颜色名表从规则库的配色型逐项填入。
}

void UIHelpers::SetTextToNone(void* /*pListBox*/)
{
    // 根据游戏行为，可知“无”是列表的缺省占位项。
}

void UIHelpers::SetTextToRandom(void* /*pListBox*/)
{
    // 根据游戏行为，可知“随机”项让开局时替玩家抽选。
}

void UIHelpers::SetTextToRandom2(void* /*pListBox*/)
{
    // 根据游戏行为，可知“随机”项的第二个形态与前者共用表现。
}

void UIHelpers::SetTextToObserver(void* /*pListBox*/)
{
    // 根据游戏行为，可知“观察者”项把参战席让给观战。
}
