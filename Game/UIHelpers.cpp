#include "UIHelpers.h"
#include "EngineGlobals.h"
#include "../Houses/HouseClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Containers/DynamicVectorClass.h"

#include <cstdint>

// ============================================================================
// UIHelpers - 对战对话框下拉框辅助
//
//   asm 里这一组是 GDlgSupp.cpp 的 hWnd_* 系列（hWnd_PopulateWithTeamNames /
//   hWnd_PopulateWithCountryNames / hWnd_PopulateWithColourNames /
//   hWnd_SetTextToNone / hWnd_SetTextToRandom / hWnd_SetTextToRandom2 /
//   hWnd_SetTextToObserver / hWnd_FindItem），签名都是
//   (HWND hDlg, int nIDDlgItem)：先 GetDlgItem 取控件，再对控件发一串
//   消息。本工程的接口直接接收控件句柄，故省掉 GetDlgItem 这一步。
//
//   原版这些控件是游戏自绘的（0x4B2 = 写文本、0x498 = 设文字颜色，见
//   Game/Shell.cpp 与 Game/ScoreScreenClass.cpp 的同一套登记），故下面
//   照 asm 立即数逐条发送，不换成标准 CB_* 常量。
// ============================================================================

// asm: ds:SendMessageA / ds:ShowWindow —— 与 Game/GameOptionsClass.cpp、
//   Game/Shell.cpp 同一处理：Windows 构建直调 user32；其余平台没有窗口系统，
//   消息不投递、可见性恒为"隐藏"。
#if defined(_WIN32) || defined(_WIN64)
    #define OPENYR_UIHELPERS_WIN32 1
    #include <windows.h>
#endif

namespace {

// asm 立即数（逐条与反汇编对应）
enum : int32 {
    kMsgItemCount      = 0x146,   // 取项数（Populate 开头的可见性判定）
    kMsgResetContent   = 0x14B,   // 清空
    kMsgSetCurSel      = 0x14E,   // 设置当前选中项
    kMsgGetItemData    = 0x150,   // hWnd_FindItem 的比对来源
    kMsgSetItemData    = 0x151,   // 项数据
    kMsgAddString      = 0x4C2,   // 追加字符串，返回值 = 新项下标
    kMsgSetColumnWidth = 0x4DD,
    kMsgSetHScroll     = 0x4DE,
    kMsgSetItemColor   = 0x498,   // lParam = -1 表示默认色
    kMsgFinalize       = 0x4F1
};

// asm: MenuColors_WCharPtr 的 8 个名字槽，由 sub_4E43C0 用 Get_String 填：
//   GUI:GoldAbbrev / RedAbbrev / BlueAbbrev / GreenAbbrev / OrangeAbbrev /
//   SkyBlueAbbrev / PurpleAbbrev / PinkAbbrev（第 9 槽 GUI:GrayAbbrev 不参与
//   本循环，循环上界是 selectedIndex+60h）。
const char* const kTeamNameKeys[8] = {
    "GUI:GoldAbbrev",
    "GUI:RedAbbrev",
    "GUI:BlueAbbrev",
    "GUI:GreenAbbrev",
    "GUI:OrangeAbbrev",
    "GUI:SkyBlueAbbrev",
    "GUI:PurpleAbbrev",
    "GUI:PinkAbbrev"
};

// asm: stru_8B3F30 的 8 个名字槽（__cfltcvt_init 里直接写死的 "1".."8"，
//   第 9 槽是 "0"）。
const wchar_t* const kColourNames[8] = {
    L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8"
};

int32 Dlg_Send(void* hWnd, int32 msg, int32 wParam, intptr_t lParam)
{
#if defined(OPENYR_UIHELPERS_WIN32)
    return static_cast<int32>(SendMessageA(static_cast<HWND>(hWnd),
                                           static_cast<UINT>(msg),
                                           static_cast<WPARAM>(wParam),
                                           static_cast<LPARAM>(lParam)));
#else
    (void)hWnd; (void)msg; (void)wParam; (void)lParam;
    return 0;
#endif
}

// asm: ds:ShowWindow —— 返回"调用前的可见性"。
bool Dlg_Show(void* hWnd, int32 nCmdShow)
{
#if defined(OPENYR_UIHELPERS_WIN32)
    return ShowWindow(static_cast<HWND>(hWnd), nCmdShow) != 0;
#else
    (void)hWnd; (void)nCmdShow;
    return false;
#endif
}

// asm: Get_String —— 取 CSF 键的文本。本树的 EngineGlobals::Get_String 给
//   窄串，而原版 Get_String 给宽串（控件按宽串接收），故这里逐字节拓宽到
//   调用方缓冲后再作为 lParam 传下去（同 ScoreScreenClass::Score_Wide）。
const wchar_t* UI_Wide(const char* pKey, wchar_t* pBuffer, size_t nCapacity)
{
    const char* pText = EngineGlobals::Get_String(pKey);
    size_t i = 0;
    for (; pText != nullptr && pText[i] != 0 && i + 1 < nCapacity; ++i)
        pBuffer[i] = static_cast<wchar_t>(static_cast<unsigned char>(pText[i]));
    pBuffer[i] = 0;
    return pBuffer;
}

// asm: SendMessage(CB_ADDSTRING 等价消息) —— lParam 为宽串指针，返回值是新项
//   下标（后续 0x151 / 0x498 都以它为 wParam）。
int32 AddString(void* pListBox, const wchar_t* pText)
{
    return Dlg_Send(pListBox, kMsgAddString, 0,
                    reinterpret_cast<intptr_t>(pText));
}

// asm: hWnd_SetTextTo{None,Random,Random2,Observer} 的公共骨架：
//   隐藏（并记住原可见性）→ 清空 → 列宽/横滚 → 追加一条带项数据的条目 →
//   选中 0 号 → 收尾 → 原来可见才恢复显示。
//
//   nColumnWidth < 0 表示该函数在原版里没有 0x4DD 这一次发送
//   （hWnd_SetTextToObserver 只发 0x4DE）。
void SetTextToSingle(void* pListBox, const char* pCsKey, int32 nItemData,
                     int32 nColumnWidth, int32 nHScroll, bool bSetColor)
{
    if (pListBox == nullptr)
        return;

    const bool bWasVisible = Dlg_Show(pListBox, 0 /* SW_HIDE */);

    Dlg_Send(pListBox, kMsgResetContent, 0, 0);
    if (nColumnWidth >= 0)
        Dlg_Send(pListBox, kMsgSetColumnWidth, 0, nColumnWidth);
    Dlg_Send(pListBox, kMsgSetHScroll, 0, nHScroll);

    wchar_t text[0x100];
    UI_Wide(pCsKey, text, sizeof(text) / sizeof(text[0]));

    const int32 idx = Dlg_Send(pListBox, kMsgAddString, 0,
                               reinterpret_cast<intptr_t>(text));
    Dlg_Send(pListBox, kMsgSetItemData, idx, nItemData);
    if (bSetColor)
        Dlg_Send(pListBox, kMsgSetItemColor, idx, -1);

    Dlg_Send(pListBox, kMsgSetCurSel, 0, 0);
    Dlg_Send(pListBox, kMsgFinalize, 0, 1);

    if (bWasVisible)
        Dlg_Show(pListBox, 5 /* SW_SHOW */);
}

}  // namespace

// ============================================================================
// asm: int __usercall hWnd_FindItem(HWND hWnd, int startIdx, int key)
//
//   取项数，从下标 0 起逐项比较"项数据"，命中返回下标，否则 -1。
//
//   注：原版的 key 是 LPARAM 项数据（调用点传的是国家号一类的整数，见
//   ModemGuest_InitDialog+250 的 dword_A8B3AC）。本工程头文件把该参数写成
//   const wchar_t*，但比对口径必须与 asm 一致——按原值比较，类型只是承载。
// ============================================================================
int32 UIHelpers::FindItem(void* pListBox, const wchar_t* pText)
{
    if (pListBox == nullptr)
        return -1;

    const int32 nCount = Dlg_Send(pListBox, kMsgItemCount, 0, 0);
    const int32 nKey = static_cast<int32>(reinterpret_cast<intptr_t>(pText));

    for (int32 i = 0; i < nCount; ++i) {
        if (Dlg_Send(pListBox, kMsgGetItemData, i, 0) == nKey)
            return i;
    }
    return -1;
}

// ============================================================================
// asm: hWnd_SetTextToNone —— GUI:NoneAsSymbols，项数据 -2，默认色。
// ============================================================================
void UIHelpers::SetTextToNone(void* pListBox)
{
    SetTextToSingle(pListBox, "GUI:NoneAsSymbols", -2,
                    1, 9, true);
}

// ============================================================================
// asm: hWnd_SetTextToRandom —— GUI:RandomAsSymbols，项数据 -2，默认色。
// ============================================================================
void UIHelpers::SetTextToRandom(void* pListBox)
{
    SetTextToSingle(pListBox, "GUI:RandomAsSymbols", -2,
                    1, 9, true);
}

// ============================================================================
// asm: hWnd_SetTextToRandom2 —— 与 SetTextToRandom 完全同构（同一 CSF 键、
//   同一项数据 -2、同一组消息），asm 里只有 Get_String 的行号不同。
// ============================================================================
void UIHelpers::SetTextToRandom2(void* pListBox)
{
    SetTextToSingle(pListBox, "GUI:RandomAsSymbols", -2,
                    1, 9, true);
}

// ============================================================================
// asm: hWnd_SetTextToObserver —— GUI:Observer，项数据 -3；这一支没有
//   0x4DD（列宽）也没有 0x498（文字颜色），横滚量是 7 而不是 9。
// ============================================================================
void UIHelpers::SetTextToObserver(void* pListBox)
{
    SetTextToSingle(pListBox, "GUI:Observer", -3,
                    -1, 7, false);
}

// ============================================================================
// asm: hWnd_PopulateWithTeamNames
//
//   隐藏 → 清空 → 列宽 1 / 横滚 9 → 先加一条 GUI:RandomAsSymbols（项数据
//   -2、文字色 -1）→ 8 个队伍名逐条加入并把下标写成项数据 → 选中 → 收尾 →
//   原来可见才恢复显示。
//
//   选择项：原版由 nIDDlgItem 推出"本控件对应的玩家槽位"再在表里找同值项
//   （表初值全为 -1，即"未占用"）。本工程接口只给控件句柄、不带控件 id，
//   取不到槽位，故按"没有匹配项"时的结果选中 0 号（与 asm 的 wParam 初值
//   一致）。
// ============================================================================
void UIHelpers::PopulateWithTeamNames(void* pListBox)
{
    if (pListBox == nullptr)
        return;

    const bool bWasVisible = Dlg_Show(pListBox, 0 /* SW_HIDE */);

    Dlg_Send(pListBox, kMsgResetContent, 0, 0);
    Dlg_Send(pListBox, kMsgSetColumnWidth, 0, 1);
    Dlg_Send(pListBox, kMsgSetHScroll, 0, 9);

    wchar_t text[0x100];
    UI_Wide("GUI:RandomAsSymbols", text, sizeof(text) / sizeof(text[0]));
    int32 idx = AddString(pListBox, text);
    Dlg_Send(pListBox, kMsgSetItemData, idx, -2);
    Dlg_Send(pListBox, kMsgSetItemColor, idx, -1);

    for (int32 i = 0; i < 8; ++i) {
        UI_Wide(kTeamNameKeys[i], text, sizeof(text) / sizeof(text[0]));
        idx = AddString(pListBox, text);
        Dlg_Send(pListBox, kMsgSetItemData, idx, i);
    }

    Dlg_Send(pListBox, kMsgSetCurSel, 0, 0);
    Dlg_Send(pListBox, kMsgFinalize, 0, 0);

    if (bWasVisible)
        Dlg_Show(pListBox, 5 /* SW_SHOW */);
}

// ============================================================================
// asm: hWnd_PopulateWithColourNames
//
//   与队伍名同构，只是名字表换成 "1".."8"（stru_8B3F30），先加的随机项是
//   GUI:RandomAsSymbols。表里的"占用"标记是 -1（空闲）/ -2（被别的玩家占），
//   永远不等于槽位号，因此 asm 里那支"命中即选中"分支不会触发，选中项恒为
//   0 —— 本工程直接按 0 处理。
// ============================================================================
void UIHelpers::PopulateWithColourNames(void* pListBox)
{
    if (pListBox == nullptr)
        return;

    const bool bWasVisible = Dlg_Show(pListBox, 0 /* SW_HIDE */);

    Dlg_Send(pListBox, kMsgResetContent, 0, 0);
    Dlg_Send(pListBox, kMsgSetColumnWidth, 0, 1);
    Dlg_Send(pListBox, kMsgSetHScroll, 0, 9);

    wchar_t text[0x100];
    UI_Wide("GUI:RandomAsSymbols", text, sizeof(text) / sizeof(text[0]));
    int32 idx = AddString(pListBox, text);
    Dlg_Send(pListBox, kMsgSetItemData, idx, -2);
    Dlg_Send(pListBox, kMsgSetItemColor, idx, -1);

    for (int32 i = 0; i < 8; ++i) {
        idx = AddString(pListBox, kColourNames[i]);
        Dlg_Send(pListBox, kMsgSetItemData, idx, i);
    }

    Dlg_Send(pListBox, kMsgSetCurSel, 0, 0);
    Dlg_Send(pListBox, kMsgFinalize, 0, 0);

    if (bWasVisible)
        Dlg_Show(pListBox, 5 /* SW_SHOW */);
}

// ============================================================================
// asm: hWnd_PopulateWithCountryNames
//
//   隐藏 → 清空 → 横滚 7（没有 0x4DD）→ 先加 GUI:RandomEx（项数据 -2）→
//   遍历 vec_HouseTypes：
//     * [+1A5h]（本树 HouseTypeClass::Multiplay）非零；
//     * [+0B8h] 也就是数组下标落在 [-2, 9]；
//     * 名字（+60h，本树 UIName）非空；
//   命中则加入并把下标写成项数据。最后 0x4F1(0,0) → 0x14E(0,0) → 恢复显示。
//   两支的先后顺序与队伍名相反（asm 里先 4F1 后 14E）。
// ============================================================================
void UIHelpers::PopulateWithCountryNames(void* pListBox)
{
    if (pListBox == nullptr)
        return;

    const bool bWasVisible = Dlg_Show(pListBox, 0 /* SW_HIDE */);

    Dlg_Send(pListBox, kMsgResetContent, 0, 0);
    Dlg_Send(pListBox, kMsgSetHScroll, 0, 7);

    wchar_t text[0x100];
    UI_Wide("GUI:RandomEx", text, sizeof(text) / sizeof(text[0]));
    int32 idx = AddString(pListBox, text);
    Dlg_Send(pListBox, kMsgSetItemData, idx, -2);

    for (int32 i = 0; i < HouseTypeClass::ArrayCount; ++i)
    {
        HouseTypeClass* pType = HouseTypeClass::Array[i];
        if (pType == nullptr)
            continue;
        if (!pType->Multiplay)
            continue;
        // asm: 下标 -2..9（cmp -3 / jle 跳过，cmp 9 / jg 跳过）
        if (i < -2 || i > 9)
            continue;
        const wchar_t* pName = pType->GetUIName();
        if (pName == nullptr || pName[0] == 0)
            continue;

        idx = AddString(pListBox, pName);
        Dlg_Send(pListBox, kMsgSetItemData, idx, i);
    }

    Dlg_Send(pListBox, kMsgFinalize, 0, 0);
    Dlg_Send(pListBox, kMsgSetCurSel, 0, 0);

    if (bWasVisible)
        Dlg_Show(pListBox, 5 /* SW_SHOW */);
}
