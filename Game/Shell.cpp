// ============================================================================
// Shell.cpp —— Win32 外壳（对话框过程 / 崩溃对话框）
//
// 逐句对照 asm；user32 调用在 Windows 构建下直调，Linux 构建下退化为 stderr
// 日志（本工程的可运行目标没有窗口系统）。原版通过 6 个 bind 钩子注入
// user32，现全部删除，改成同一份代码里的条件编译。
// ============================================================================

#include "Game/Shell.h"

#include <cstdio>
#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
    #define OPENYR_SHELL_WIN32 1
    #include <windows.h>
#endif

namespace {

// asm: str_except_txt
const char* s_ExceptText = "";

// ---------------------------------------------------------------------------
// 消息常量（避免在非 Windows 构建里依赖 winuser.h）
// ---------------------------------------------------------------------------
enum : int32 {
    WM_CLOSE      = 0x0010,
    WM_PAINT      = 0x000F,
    WM_COMMAND    = 0x0111,
    WM_INITDIALOG = 0x0110,
    WM_MOVING     = 0x0216
};

// 主菜单控件 ID / 消息
enum : int32 {
    IDC_MENU_PAINT_TARGET = 0x71A,   // WM_PAINT 时接收 0x4F0
    IDC_MENU_VERSION      = 0x71D,   // 版本串显示控件
    WM_MENU_REFRESH       = 0x4F0,
    WM_SET_VERSION_TEXT   = 0x4B2
};

// 崩溃对话框控件 ID
enum : int32 {
    IDC_EXCEPT_TEXT       = 0x484,
    IDC_EXCEPT_SHUTDOWN   = 0x47D,
    IDC_EXCEPT_ABORT      = 0x481
};

// 每次运行最多允许 3 次"紧急关机"提示
const char k_SorryText[] =
    "Sorry, you can only attempt an emergency shut down 3 times.  "
    "Please try again.";

const char k_RecursiveText[] =
    "Error - Recursive exception detected. Program terminating.\n";

void ShellLog(const char* pText)
{
    if (pText == nullptr)
        return;
    std::fputs(pText, stderr);
    std::fflush(stderr);
}

} // namespace

void Set_Except_Text(const char* pText)
{
    s_ExceptText = (pText != nullptr) ? pText : "";
}

// ============================================================================
// asm: DlgProc_MainMenu (0x531FD0, retn 10h) —— 恒返回 0
//
//   if (Dialog_HandleGeneralMessages(hDlg,msg,wParam,lParam)) return 0
//   pResult = GetWindowLongA(hDlg, 8)
//   switch (msg):
//     WM_PAINT (0Fh):   SendMessageA(GetDlgItem(hDlg,71Ah), 4F0h, 0, 0)
//     WM_COMMAND (111h): id = wParam & 0FFFFh
//         3EEh -> 6 / 55Ch -> 5 / 578h -> 3 / 683h -> 1 / 684h -> 2 / 686h -> 4
//     497h: 版本串刷新 —— GetDlgItem(hDlg,71Dh) 收到格式化后的 "GUI:Version"
//   return 0
//
// 降级说明：版本串取自 GetModuleInternalVersion + 字符串表 "GUI:Version"，
// 本工程既没有版本资源也没有 CSF 字符串表（StringTable 只有拆解路径），
// 故 0x497 分支只做控件定位，不再写文本。
// ============================================================================
int32 DlgProc_MainMenu(void* hDlg, int32 msg, int32 wParam, int32 lParam)
{
#if defined(OPENYR_SHELL_WIN32)
    HWND const hWnd = static_cast<HWND>(hDlg);

    // asm: call Dialog_HandleGeneralMessages —— 游戏侧通用对话框消息
    //      （本工程未建模，直接落到各分支）
    int32* const pResult = reinterpret_cast<int32*>(
        static_cast<intptr_t>(GetWindowLongA(hWnd, DWL_USER)));   // asm: push 8

    switch (msg)
    {
    case WM_PAINT:
        // asm: GetDlgItem(hDlg,71Ah) ; SendMessageA(item,4F0h,0,0)
        SendMessageA(GetDlgItem(hWnd, IDC_MENU_PAINT_TARGET),
                     WM_MENU_REFRESH, 0, 0);
        break;

    case WM_COMMAND:
    {
        // asm: mov ecx,wParam / and ecx,0FFFFh
        const int32 id = wParam & 0xFFFF;
        int32       result = 0;
        bool        hit = true;

        switch (id)
        {
        case 0x3EE: result = 6; break;
        case 0x55C: result = 5; break;
        case 0x578: result = 3; break;
        case 0x683: result = 1; break;
        case 0x684: result = 2; break;
        case 0x686: result = 4; break;
        default:    hit = false; break;
        }

        // asm: mov dword ptr [eax], N   (eax = GetWindowLongA 结果)
        if (hit && pResult != nullptr)
            *pResult = result;
        break;
    }

    case 0x497:
        // asm: GetDlgItem(hDlg,71Dh) 之后写版本串；见函数头降级说明
        (void)GetDlgItem(hWnd, IDC_MENU_VERSION);
        break;

    default:
        break;
    }
#else
    (void)hDlg;
    (void)wParam;
    (void)lParam;
    if (msg == WM_COMMAND)
    {
        const int32 id = wParam & 0xFFFF;
        if (id == 0x3EE || id == 0x55C || id == 0x578 ||
            id == 0x683 || id == 0x684 || id == 0x686)
        {
            ShellLog("[SHELL] DlgProc_MainMenu: menu command\n");
        }
    }
#endif
    return 0;   // asm: xor eax,eax / retn 10h
}

// ============================================================================
// asm: ExceptDialog_WndProc (0x4C8410, retn 10h)
//
//   110h (WM_INITDIALOG):
//     WWDebugString("WM_INITDIALOG\n")
//     SetDlgItemTextA(hDlg, 484h, str_except_txt)
//     SetFocus(hDlg)
//     if (hWndParent) sub_777080(hDlg)          ; 窗口居中
//     ShowWindow(hDlg, 1) ; sub_53E3C0(hDlg)    ; "对话框已显示"
//   10h (WM_CLOSE):   EndDialog(hDlg,481h) ; sub_53E420(hDlg)
//   111h (WM_COMMAND): 47Dh -> MessageBoxA(...,"Bummer",MB_ICONEXCLAMATION)
//                      481h -> EndDialog(hDlg,481h)
//   216h (WM_MOVING): return sub_776D80(...) & 0xFF
// ============================================================================
int32 ExceptDialog_WndProc(void* hWnd, int32 msg, int32 wParam, int32 lParam)
{
#if defined(OPENYR_SHELL_WIN32)
    HWND const hDlg = static_cast<HWND>(hWnd);

    switch (msg)
    {
    case WM_INITDIALOG:
        ShellLog("WM_INITDIALOG\n");                       // asm: WWDebugString
        SetDlgItemTextA(hDlg, IDC_EXCEPT_TEXT, s_ExceptText);
        SetFocus(hDlg);
        // asm: sub_777080（居中）/ sub_53E3C0（已显示）—— 未建模
        ShowWindow(hDlg, SW_SHOWNORMAL);                   // asm: ShowWindow(hDlg,1)
        return 0;

    case WM_CLOSE:
        EndDialog(hDlg, IDC_EXCEPT_ABORT);                 // asm: EndDialog(hDlg,481h)
        // asm: sub_53E420（已关闭）—— 未建模
        return 0;

    case WM_COMMAND:
    {
        const int32 id = wParam & 0xFFFF;
        if (id == IDC_EXCEPT_SHUTDOWN)
        {
            MessageBoxA(hDlg, k_SorryText, "Bummer",
                        MB_ICONEXCLAMATION);               // asm: 0x30
            return 0;
        }
        if (id == IDC_EXCEPT_ABORT)
        {
            EndDialog(hDlg, IDC_EXCEPT_ABORT);
            return 0;
        }
        return 0;
    }

    case WM_MOVING:
        // asm: push lParam / edx=wParam / ecx=hWnd / call sub_776D80 / and eax,0FFh
        //      （拖动吸附逻辑未建模，等价保留默认移动）
        return 0;

    default:
        return 0;   // asm: loc_4C84BE
    }
#else
    (void)hWnd;
    (void)wParam;
    (void)lParam;
    if (msg == WM_INITDIALOG)
        ShellLog("WM_INITDIALOG\n");
    return 0;
#endif
}

// ============================================================================
// asm: Exception_Dialog (0x4C8530)
//
//   call sub_53E6B0                                  ; 崩溃现场准备
//   switch (dword_820E5C):                           ; 崩溃模式
//     1: AllocConsole（失败先 FreeConsole 再试一次）
//        控制台打印"Error - Recursive exception detected." ;
//        Sleep(4000) ; Console_ReadChar() ; FreeConsole() ; return 481h
//     2: return 481h
//     3: ExitProcess(0) ; return 481h
//     default:
//        ShowCursor(1)
//        res = Fetch_Resource(0F7h, 5)
//        if (res) DialogBoxIndirectParamA(hInstance, res, hWndParent, proc, 0)
//        else     DialogBoxParamA(hInstance, 0F7h, hWndParent, proc, 0)
//        ShowCursor(0) ; return r
//
// 降级说明：0F7h 号对话框模板来自 PE 资源，本工程没有 .rc / 资源段，也没有
// 控制台子系统，故三种模式都只记录日志并返回 0x481（"终止"）。
// ============================================================================
int32 Exception_Dialog()
{
    // asm: call sub_53E6B0
    //      （崩溃现场准备：未建模）

    // asm: mov eax,dword_820E5C / dec / dec / dec
    //      模式由崩溃捕获端设置；本工程只有一个"终止"出口。
    ShellLog(k_RecursiveText);

#if defined(OPENYR_SHELL_WIN32)
    // asm: 模式 3 -> ExitProcess(0)
    //      这里不主动退出进程，交回调用方决定。
#endif

    return 0x481;   // asm: loc_4C85CF mov eax,481h
}
