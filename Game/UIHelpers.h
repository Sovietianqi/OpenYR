#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// UIHelpers - dialog list box helpers
//
//  根据游戏行为，可知对战设置对话框的几个下拉框由一组辅助函数维护：
//  填充队名/国家名/颜色名三张表，并提供“无”“随机”“观察者”三个
//  特殊默认项的设置与项查找。
// ============================================================================

class UIHelpers {
public:
    static int32 FindItem(void* pListBox, const wchar_t* pText);
    static void PopulateWithTeamNames(void* pListBox);
    static void PopulateWithCountryNames(void* pListBox);
    static void PopulateWithColourNames(void* pListBox);
    static void SetTextToNone(void* pListBox);
    static void SetTextToRandom(void* pListBox);
    static void SetTextToRandom2(void* pListBox);
    static void SetTextToObserver(void* pListBox);
};
