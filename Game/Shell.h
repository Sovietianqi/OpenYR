#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"

// ============================================================================
// Shell.h —— Win32 外壳：对话框过程 + 崩溃对话框
//
// 这三个入口在原版里是 Win32 消息回调（USER32 → 游戏），不属于任何游戏
// 对象；它们原先挂在 Helpers/AsmComplex5.{h,cpp} 里靠 bind 钩子访问 user32，
// 现改为直调 user32（Windows）或退化为日志（Linux 构建）。
//   DlgProc_MainMenu      (0x531FD0, retn 10h)  主菜单对话框过程
//   ExceptDialog_WndProc  (0x4C8410, retn 10h)  崩溃对话框过程
//   Exception_Dialog      (0x4C8530)            崩溃对话框驱动器
// ============================================================================

// asm: DlgProc_MainMenu —— 恒返回 0
int32 DlgProc_MainMenu(void* hDlg, int32 msg, int32 wParam, int32 lParam);

// asm: ExceptDialog_WndProc —— 恒返回 0（WM_MOVING 除外）
int32 ExceptDialog_WndProc(void* hWnd, int32 msg, int32 wParam, int32 lParam);

// asm: Exception_Dialog —— 返回对话框结果（0x481 = "终止"）
int32 Exception_Dialog();

// asm: str_except_txt —— 崩溃对话框正文（由崩溃捕获端填好后调用上面的入口）
void Set_Except_Text(const char* pText);
