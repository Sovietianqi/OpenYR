#include "LoadOptionsClass.h"

#include "EngineGlobals.h"

// ============================================================================
// LoadOptionsClass
// ============================================================================

// ----------------------------------------------------------------------------
// asm: LoadOptionsClass_GetUIString_GameSaved (0x55A1A0)
//   ecx = "TXT_GAME_WAS_SAVED" / edx = 0
//   push "D:\ra2mdpost\LoadDlg.CPP" / push 4D2h / call Get_String
//
// 原版 Get_String 带 (key, a2, file, line) 四参，其中 a2/file/line 只用于
// 缺键告警；本工程的字符串表入口 EngineGlobals::Get_String 只取 key，缺键
// 时回退返回 key 本身 —— 语义等价（不产生空指针）。
// ----------------------------------------------------------------------------
const char* LoadOptionsClass::Get_UI_String_GameSaved()
{
    return EngineGlobals::Get_String("TXT_GAME_WAS_SAVED");
}
