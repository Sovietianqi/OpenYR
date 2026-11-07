// ============================================================================
// Networking/ModemHostClass.cpp
//
// asm 权威：ModemHost_ReadDialog (0x5B8CC0, __thiscall, retn 0, AL = 结果)
// 逐句对照见 .h 的函数注释；本文件不再经任何 bind / 函数指针表，
// Win32 调用在 Windows 构建下直调 user32，其余平台退化为"无控件 / 0"
// （与 Game/GameOptionsClass.cpp 的 Dlg_Get_Item / Dlg_Send_Msg 同一约定）。
// ============================================================================

#include "Networking/ModemHostClass.h"

#include "Network/SessionClass.h"
#include "Network/ModemClass.h"
#include "Scenario/ScenarioClass.h"
#include "Game/EngineGlobals.h"
#include "Game/Externs.h"
#include "INI/INIClass.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

#if defined(_WIN32) || defined(_WIN64)
    #define OPENYR_MODEMHOST_WIN32 1
    #include <windows.h>
#endif

// ============================================================================
// 静态成员
// ============================================================================
ModemHostClass* ModemHostClass::Instance = nullptr;   // asm: 当前 ModemHost 实例

ModemHostClass::ModemHostClass() noexcept
    : Hwnd(nullptr)                                  // asm [this+0]
    , Name{}
{
    Instance = this;
}

ModemHostClass::~ModemHostClass()
{
    if (Instance == this)
        Instance = nullptr;
}

namespace {

// ----------------------------------------------------------------------------
// 窗口层薄封装（同 GameOptionsClass 的约定：无窗口系统时退化为"无控件/0"）
// ----------------------------------------------------------------------------
void* Get_Dlg_Item(void* hDlg, int32 id)
{
#if defined(OPENYR_MODEMHOST_WIN32)
    return static_cast<void*>(::GetDlgItem(static_cast<HWND>(hDlg),
                                           static_cast<int>(id)));
#else
    (void)hDlg; (void)id;
    return nullptr;
#endif
}

int32 Send_Dlg_Msg(void* hWnd, int32 msg, int32 wParam, int32 lParam)
{
#if defined(OPENYR_MODEMHOST_WIN32)
    return static_cast<int32>(::SendMessageA(static_cast<HWND>(hWnd),
                                             static_cast<UINT>(msg),
                                             static_cast<WPARAM>(wParam),
                                             static_cast<LPARAM>(lParam)));
#else
    (void)hWnd; (void)msg; (void)wParam; (void)lParam;
    return 0;
#endif
}

// asm: MsgBox(ecx = " " 标题, lParam = 文本, ..., ok) —— 模态消息框。
//   本工程没有窗口系统时退化为输出到 stderr，返回值仍按 MB_OK 处理。
int32 Msg_Box(void* hDlg, const wchar_t* pText, const wchar_t* pCaption)
{
#if defined(OPENYR_MODEMHOST_WIN32)
    return static_cast<int32>(::MessageBoxW(static_cast<HWND>(hDlg), pText,
                                            pCaption, MB_OK));
#else
    (void)hDlg;
    std::fwprintf(stderr, L"[MsgBox] %ls\n", pCaption != nullptr ? pCaption : L"");
    std::fwprintf(stderr, L"%ls\n", pText != nullptr ? pText : L"");
    return 1;
#endif
}

// asm: Get_String(key, 0, file, line, ...) 返回宽串；本工程 CSF 入口是
//   EngineGlobals::Get_String（窄串），这里按需转成宽串喂 _swprintf/MsgBox。
//   译文缺失时（nullptr）退回键名本身，与原版"找不到就返回键名"一致。
const wchar_t* Get_Wide_String(const char* pKey, wchar_t* pBuffer,
                               size_t nBuffer)
{
    const char* pText = EngineGlobals::Get_String(pKey);
    if (pText == nullptr)
        pText = pKey;

    if (nBuffer > 0)
    {
        size_t n = 0;
        for (; pText[n] != '\0' && n + 1 < nBuffer; ++n)
            pBuffer[n] = static_cast<wchar_t>(
                static_cast<unsigned char>(pText[n]));
        pBuffer[n] = L'\0';
    }
    return pBuffer;
}

// asm: sub_735090(dst, src, -1) —— 第三参 -1 表示"拷到 NUL 为止"
void Copy_Wide_String(wchar_t* pDst, const wchar_t* pSrc, size_t nDstCount)
{
    if (pDst == nullptr || pSrc == nullptr)
        return;
    std::wcsncpy(pDst, pSrc, nDstCount - 1);
    pDst[nDstCount - 1] = L'\0';
}

// asm: 无符号则保留旧值 —— GetDlgItem + SendMessageA 的组合。
int32 Probe_Control(void* hDlg, int32 id, int32 msg)
{
    void* h = Get_Dlg_Item(hDlg, id);
    if (h == nullptr)
        return 0;
    return Send_Dlg_Msg(h, msg, 0, 0);
}

// asm: 复选框统一形态
//   item = GetDlgItem(id) ; if (!item) 跳过（全局保留旧值）
//   gX = (SendMessageA(item,0F0h,0,0) == 1)   ; cmp eax,1 / setz
//   WWDebugString("X: %s\n", gX == 1 ? "ON" : "OFF")
void Read_Checkbox(void* hDlg, int32 id, bool& Value, const char* pFormat)
{
    void* h = Get_Dlg_Item(hDlg, id);
    if (h != nullptr)
    {
        const int32 chk = Send_Dlg_Msg(h, 0x00F0 /*BM_GETCHECK*/, 0, 0);
        Value = (chk == 1);                           // asm: cmp eax,1 / setz
    }
    std::printf(pFormat, Value ? "ON" : "OFF");
}

} // namespace

// ============================================================================
// asm: sub_5E6520 —— 剧本可用玩家数
// ============================================================================
int32 ModemHostClass::Scenario_Player_Count(int32 nScenarioIndex)
{
    SessionClass* pSession = SessionClass::GetInstance();
    const int32 nCurrent = (pSession != nullptr) ? pSession->ScenarioIndex : 0;

    // asm: cmp ecx,0 / jl  -> return 0 ; cmp ecx,dword_A8B8D8 / jge -> return 0
    //   dword_A8B8CC 是本工程的剧本条目表；条目 +0x58 处即文件名。本工程
    //   尚未建立该目录表，故"索引命中当前已装载剧本"时等价取
    //   ScenarioClass::ScenarioFileName（同一条目里的同一个字符串）。
    if (nScenarioIndex < 0 || nScenarioIndex != nCurrent)
        return 0;
    if (TheScenario == nullptr || TheScenario->ScenarioFileName == nullptr)
        return 0;

    // asm: CCFileClass_CTOR(filename) -> INIClass_ReadCCFile ; 失败 return 0
    CCINIClass* pINI = CCINIClass::LoadINIFile(TheScenario->ScenarioFileName);
    if (pINI == nullptr)
        return 0;

    // asm: for (esi = 0 ; esi < 8 ; ++esi)
    //         sprintf(key, "%d", esi)
    //         if (INI_GetInteger("Waypoints", key, -1) != -1) ++edi
    int32 nCount = 0;
    for (int32 i = 0; i < 8; ++i)
    {
        char key[16];
        std::snprintf(key, sizeof(key), "%d", static_cast<int>(i));

        int32 nWaypoint = -1;                          // asm: push 0FFFFFFFFh
        pINI->GetInteger("Waypoints", key, nWaypoint);
        if (nWaypoint != -1)
            ++nCount;
    }

    // asm: if (edi == 0) { edi = INI_GetInteger("RandomMap","NumPlayers",0) ;
    //                      if (edi == 0) edi = 8 ; }
    if (nCount == 0)
    {
        int32 nPlayers = 0;
        pINI->GetInteger("RandomMap", "NumPlayers", nPlayers);
        nCount = (nPlayers != 0) ? nPlayers : 8;
    }

    CCINIClass::UnloadINIFile(pINI);
    return nCount;
}

// ============================================================================
// asm: ModemHost_ReadDialog (0x5B8CC0)
// ============================================================================
uint8 ModemHostClass::ReadDialog()
{
    std::printf("ModemHost.ReadDialog(enter)\n");      // asm: WWDebugString

    SessionClass* pSession = SessionClass::GetInstance();
    if (pSession == nullptr)
        return 0;

    // asm: mov ecx,gScenarioIndex / call sub_5E6520
    const int32 nMaxPlayers = Scenario_Player_Count(pSession->ScenarioIndex);
    void* hDlg = Hwnd;                                 // asm: mov eax,[esi]

    // asm: item = GetDlgItem(hDlg,552h) ; edi = 2
    //      if (item) edi = SendMessageA(item,400h,0,0) + 2
    int32 nTextLen = 2;
    {
        void* h = Get_Dlg_Item(hDlg, 0x552);
        if (h != nullptr)
            nTextLen = Send_Dlg_Msg(h, 0x400, 0, 0) + 2;
    }

    // asm: cmp eax(nMax), edi ; jge loc_5B8DA5
    if (nMaxPlayers < nTextLen)
    {
        // ---- 剧本太小（asm 0x5B8D40..0x5B8DA4）----------------------------
        wchar_t fmt[0x100];                            // asm: lParam(word ptr -200h)
        wchar_t okStr[0x100];

        // Get_String("TXT_SCENARIO_TOO_SMALL", 0, "D:\ra2mdpost\ModemHst.cpp",2F7h,nMax)
        Get_Wide_String("TXT_SCENARIO_TOO_SMALL", fmt, 0x100);

        wchar_t text[0x100];                           // asm: _swprintf(ecx=lParam)
        std::swprintf(text, 0x100, fmt, static_cast<int>(nMaxPlayers));

        // Get_String("TXT_OK", 0, "D:\ra2mdpost\ModemHst.cpp",2F8h,0,0,0)
        Get_Wide_String("TXT_OK", okStr, 0x100);

        // asm: var_22C = wstr_space ; MsgBox(caption, text, text, ok)
        Msg_Box(hDlg, text, L" ");
        return 0;                                      // asm: xor al,al
    }

    // ---- 名字（asm 0x5B8DA5..0x5B8DD1）------------------------------------
    {
        void* h = Get_Dlg_Item(hDlg, 0x5C1);
        if (h != nullptr)
        {
            wchar_t nameTmp[0x14] = { 0 };             // asm: var_228（14h 个字符）
            Send_Dlg_Msg(h, 0x4B3 /*取文本*/, 0x14,
                         static_cast<int32>(
                             reinterpret_cast<intptr_t>(nameTmp)));
            Copy_Wide_String(Name, nameTmp, 0x40);      // asm: sub_735090
        }
    }
    std::printf("Name: %ls\n", Name);                   // asm: WWDebugString

    // ---- 颜色（asm 0x5B8DD2..0x5B8E02）------------------------------------
    {
        void* h = Get_Dlg_Item(hDlg, 0x405);
        if (h != nullptr)
            ModemClass::PlayerColor = Send_Dlg_Msg(h, 0x147, 0, 0);   // gPlayerColor
    }
    std::printf("Color: %d\n", ModemClass::PlayerColor);

    // ---- 国家槽（asm 0x5B8E03..0x5B8E41）----------------------------------
    {
        void* h = Get_Dlg_Item(hDlg, 0x667);
        if (h != nullptr)
        {
            const int32 sel = Send_Dlg_Msg(h, 0x147, 0, 0);
            ModemClass::GuestColorIndex = Send_Dlg_Msg(h, 0x150, sel, 0);
        }
    }

    // ---- 单位数（asm 0x5B8E42..0x5B8E60）----------------------------------
    {
        void* h = Get_Dlg_Item(hDlg, 0x669);
        if (h != nullptr)
            pSession->UnitCount = Send_Dlg_Msg(h, 0x400, 0, 0);       // gUnitCount
    }
    std::printf("UnitCount: %d\n", pSession->UnitCount);

    // ---- 科技等级（asm 0x5B8E61..0x5B8E92）--------------------------------
    {
        void* h = Get_Dlg_Item(hDlg, 0x550);
        if (h != nullptr)
            pSession->TechLevel = Send_Dlg_Msg(h, 0x400, 0, 0);       // gTechLevel
    }
    std::printf("TechLevel: %d\n", pSession->TechLevel);

    // ---- 资金（asm 0x5B8E93..0x5B8EC5）------------------------------------
    {
        void* h = Get_Dlg_Item(hDlg, 0x402);
        if (h != nullptr)
            pSession->Credits = Send_Dlg_Msg(h, 0x400, 0, 0);         // gMoney
    }
    std::printf("Credits: %d\n", pSession->Credits);

    // ---- AI 玩家数（asm 0x5B8EC6..0x5B8EF8）-------------------------------
    {
        void* h = Get_Dlg_Item(hDlg, 0x552);
        if (h != nullptr)
            pSession->AIPlayers = Send_Dlg_Msg(h, 0x400, 0, 0);       // gAIPlayers
    }
    std::printf("AIPlayers: %d\n", pSession->AIPlayers);

    // ---- 游戏速度（asm 0x5B8EF9..0x5B8F3A）--------------------------------
    //   if (item) { gGameSpeed = 6 - SendMessageA(item,400h,0,0) ; jmp print }
    //   else      { ecx = gGameSpeed ; jmp print }      —— 打印的是这个 ecx
    {
        void* h = Get_Dlg_Item(hDlg, 0x529);
        if (h != nullptr)
            pSession->GameSpeed = 6 - Send_Dlg_Msg(h, 0x400, 0, 0);
    }
    std::printf("Game Speed: %d\n", pSession->GameSpeed);

    // ---- AI 难度（asm 0x5B8F3B..0x5B8F67）---------------------------------
    {
        void* h = Get_Dlg_Item(hDlg, 0x553);
        if (h != nullptr)
            pSession->AIDifficulty = Send_Dlg_Msg(h, 0x400, 0, 0);    // gAIDifficulty
    }
    std::printf("AIDifficulty: %d\n", pSession->AIDifficulty);

    // ---- 10 个复选框（asm 0x5B8F68..0x5B9255）-----------------------------
    Read_Checkbox(hDlg, 0x66B, pSession->MCVRepacks,      "MCVRedeploy: %s\n");
    Read_Checkbox(hDlg, 0x66A, pSession->Bases,           "Bases: %s\n");
    Read_Checkbox(hDlg, 0x54C, pSession->Crates,          "Crates: %s\n");
    Read_Checkbox(hDlg, 0x54D, pSession->FogOfWar,        "FogOfWar: %s\n");
    Read_Checkbox(hDlg, 0x624, pSession->BridgesDestroyable,
                  "BridgeDestroy: %s\n");
    Read_Checkbox(hDlg, 0x596, pSession->HarvesterTruce,  "Harvester Truce: %s\n");
    Read_Checkbox(hDlg, 0x54E, pSession->ShortGame,       "Short Game: %s\n");

    // asm 0x5B9155..0x5B91B0：0x69Ah -> gSWAllowed，但紧接着的调试打印读的
    // 仍是 SessionClass::ShortGame（原版就这样，逐字节保留）。
    {
        void* h = Get_Dlg_Item(hDlg, 0x69A);
        if (h != nullptr)
            pSession->SuperWeapons =
                (Send_Dlg_Msg(h, 0x00F0, 0, 0) == 1);     // cmp eax,1 / setz al
        std::printf("Short Game: %s\n", pSession->ShortGame ? "ON" : "OFF");
    }

    // asm 0x5B91B1..0x5B920C：0x69Dh -> gBuildOffAlly，打印同样读 ShortGame。
    {
        void* h = Get_Dlg_Item(hDlg, 0x69D);
        if (h != nullptr)
            pSession->BuildOffAlly =
                (Send_Dlg_Msg(h, 0x00F0, 0, 0) == 1);     // cmp eax,1 / setz dl
        std::printf("Short Game: %s\n", pSession->ShortGame ? "ON" : "OFF");
    }

    Read_Checkbox(hDlg, 0x551, pSession->MultiEngineer,   "Crap Engineers: %s\n");

    std::printf("ModemHost.ReadDialog(exit)\n");        // asm: WWDebugString
    return 1;                                          // asm: mov al,1
}
