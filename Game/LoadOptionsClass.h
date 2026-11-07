#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// LoadOptionsClass —— "载入游戏" 对话框（asm 0x55A0xx，vtable 007ED2E0 起）
//
// 原版这一类只在 LoadDlg.CPP 内部使用：四个 GetUIString_* 槽 + Load/Save/
// DeleteMission + 标量删除析构。本批只回收了 GameSaved 这一个槽（其余三个
// GetUIString_Load/Save/Delete 与三个 Mission 操作不在本批范围），所以类里
// 目前只有它一个成员。
//
// asm: LoadOptionsClass_GetUIString_GameSaved (0x55A1A0)
//   push 4D2h                             ; int line
//   push "D:\ra2mdpost\LoadDlg.CPP"       ; const char* file
//   xor  edx, edx                         ; a2 = 0
//   mov  ecx, "TXT_GAME_WAS_SAVED"        ; const char* key
//   call Get_String
//   retn
// 四个槽除了 key / 行号以外完全同形（Load=4C0h、Save=4C6h、Delete=4CCh）。
// ============================================================================
class LoadOptionsClass {
public:
    // asm: LoadOptionsClass_GetUIString_GameSaved —— 无 this，retn
    static const char* Get_UI_String_GameSaved();
};
