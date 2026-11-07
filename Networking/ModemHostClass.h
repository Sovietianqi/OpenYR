#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// ModemHostClass —— 调制解调器（拨号）主机端对话框控制器
//
// asm 依据：ModemHost_ReadDialog (0x5B8CC0, __thiscall, retn 0, 结果在 AL)
//   ecx = ModemHostClass*，对象首字段 [this+0] 就是对话框句柄：
//     mov esi, ecx
//     ...
//     mov eax, [esi]        ; +0x00 = hDlg
//     push 552h / push eax / call GetDlgItem
//   该函数只在对话框存活期间使用本对象，除 hDlg 外不读其它成员；读回的
//   各项设置按 asm 写进**全局**（gUnitCount / gTechLevel / gMoney /
//   gAIPlayers / gGameSpeed / gAIDifficulty / gMCVRedeploy / gBases /
//   gCrates / gFogOfWar / gBridgeDestruction / gHarvesterTruce /
//   SessionClass::ShortGame / gSWAllowed / gBuildOffAlly / gMultiEngineer /
//   gPlayerColor / dword_A8B3AC），本工程里这些全局的真实归属是
//   SessionClass（多人数会话状态块）与 ModemClass（拨号共享的颜色槽），
//   故此处直调它们的真实成员，不再另立一份影子变量。
//
// 字段：
//   +0x00  Hwnd   对话框句柄（asm [this+0]，也是本函数唯一读的对象字段）
//          Name   asm word_A8B380 —— 主机名宽串缓冲（WM_GETTEXT 20 字符后
//                 经 sub_735090(wcscpy) 整体覆盖）
// ============================================================================
class ModemHostClass
{
public:
    ModemHostClass() noexcept;
    virtual ~ModemHostClass();

    // asm: dword_ABF10C 之外的 ModemHost 实例句柄（对话框存活期有效）
    static ModemHostClass* Instance;

    // ------------------------------------------------------------------------
    // asm: ModemHost_ReadDialog (0x5B8CC0)
    //
    //   WWDebugString("ModemHost.ReadDialog(enter)\n")
    //   nMax = sub_5E6520(gScenarioIndex)            ; 剧本可用玩家数
    //   edi = 2
    //   item = GetDlgItem(hDlg,552h)
    //   if (item) edi = SendMessageA(item,400h,0,0) + 2
    //   if (nMax < edi) {                            ; 剧本太小
    //       Get_String("TXT_SCENARIO_TOO_SMALL",0,"D:\ra2mdpost\ModemHst.cpp",2F7h,nMax)
    //       _swprintf(buf, fmt, nMax)
    //       Get_String("TXT_OK",0,"D:\ra2mdpost\ModemHst.cpp",2F8h,0,0,0)
    //       MsgBox(wstr_space, buf, buf, ok) ; return 0
    //   }
    //   名字 5C1h -> WM_GETTEXT(4B3h,14h) -> wcscpy(word_A8B380,...)
    //   颜色 405h -> 147h -> gPlayerColor
    //   国家 667h -> 147h -> 150h -> dword_A8B3AC
    //   单位数 669h / 科技等级 550h / 资金 402h / AI 数 552h / AI 难度 553h
    //   游戏速度 529h -> 6 - 文本长度（控件缺失时保留旧值）
    //   10 个复选框（BM_GETCHECK=F0h），cmp eax,1 / setz
    //   return 1
    // ------------------------------------------------------------------------
    uint8 ReadDialog();

    // ------------------------------------------------------------------------
    // asm: sub_5E6520 —— 剧本可用玩家数
    //
    //   if (idx < 0 || idx >= dword_A8B8D8) return 0
    //   ini = CCINIClass(ScenarioName(idx) + 58h) ; if (!ini.Load()) return 0
    //   n = 0
    //   for (i = 0 ; i < 8 ; ++i)
    //       if (INI_GetInteger("Waypoints", sprintf("%d",i), -1) != -1) ++n
    //   if (n == 0) { n = INI_GetInteger("RandomMap","NumPlayers",0) ;
    //                 if (n == 0) n = 8 ; }
    //   return n
    // ------------------------------------------------------------------------
    static int32 Scenario_Player_Count(int32 nScenarioIndex);

    // asm [this+0] —— 对话框句柄
    void*   Hwnd;
    // asm word_A8B380 —— 主机名（宽串，WM_GETTEXT 最多 14h 个字符）
    wchar_t Name[0x40];
};

// asm: word_A8B380 / gPlayerColor / dword_A8B3AC 与拨号模块共享，
// Guest_ReadDialog 用的是同一批全局（见 Network/ModemClass.h），
// 这里只做可读性别名，不重复定义。
