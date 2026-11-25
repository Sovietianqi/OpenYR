#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// ModemClass - 拨号对战的状态块（原版 ModemHost / ModemGuest 共用同一个
// 状态结构，dword_ABF10C 持有当前实例）。
//
// asm 恢复自：
//   * ModemHost_Dialog       0x5B7990  (__thiscall, retn 0, 结果在 AL)
//   * ModemGuest_ReadDialog  0x5B5700  (__thiscall, retn 0)
//
// 字段偏移全部来自 asm 的直接寻址，未做任何重排：
//   +0x00  Hwnd                 对话框窗口（也作为本对象首字段被 ShowWindow 用）
//   +0x04  DialogResult         -1 运行中 / 1 开始游戏 / 2 挂断
//   +0x08  SaveNetworkConfig    退出时是否写回网络配置
//   +0x09  Field09              进入时置 1
//   +0x0C  StartTick            计时基准（Tick 展开后的绝对时间）
//   +0x10  Timeout1             0x2710
//   +0x14  StartTick2           第二个计时基准
//   +0x18  Timeout2             0x4B0
//   +0x1C  Field1C              0
//   +0x20  Field20              0
//   +0x22  Field22              0（word）
//   +0x4C  Field4C              -1
//   +0x50  Field50              -1
//   +0x54  Timer54              sub_679D90() 的返回值
//   +0x58  Field58              unknown_libname_29()
//   +0x5C  Field5C              unknown_libname_30()
//
// 降级点（本项目没有 Win32 资源对话框与拨号传输层）：
//   Dialog_Show / GetDlgItem / SendMessageA / SetWindowLongA / ShowWindow /
//   EnterCriticalSection 这一层在本项目没有实体。所有落到窗口系统的动作
//   都收敛到下方几个 protected 虚口，默认实现返回"无窗口"（nullptr / 0），
//   Host_Dialog 因此会走 asm 里 `if (!hWnd) goto exit_fail` 的既存分支，
//   返回 false。状态机与字段写入顺序仍与 asm 完全一致。
// ============================================================================
class ModemClass
{
public:
    ModemClass() noexcept;
    virtual ~ModemClass();

    // asm: dword_ABF10C —— 当前活动的 Modem 状态块
    static ModemClass* Instance;

    // asm: word_A8B380 / gPlayerColor / dword_A8B3AC
    static char  GuestName[0x40];
    static int32 PlayerColor;
    static int32 GuestColorIndex;

    // ------------------------------------------------------------------------
    // asm: ModemHost_Dialog (0x5B7990)
    //
    //   WWDebugString("ModemHost.Dialog(enter)\n")
    //   state->[+4] = -1
    //   hWnd = Dialog_Show(0xF1, sub_5B7970, 0) ; if (!hWnd) goto exit_fail
    //   SetWindowLongA(hWnd, 8, state)
    //   sub_5B82F0(state) ; sub_622800(hWnd)
    //   state->[+8] = 0 ; state->[+9] = 1
    //   state->[+0Ch] = tick() ; state->[+10h] = 0x2710
    //   state->[+14h] = tick() ; state->[+18h] = 0x4B0
    //   state->[+1Ch] = 0 ; state->[+20h] = 0 ; state->[+22h] = 0
    //   state->[+4Ch] = -1 ; state->[+50h] = -1
    //   dword_ABF10C = state
    //   state->[+54h] = sub_679D90() ; Game_Load_ARTMD_INI()
    //   state->[+58h] = unknown_libname_29() ; state->[+5Ch] = unknown_libname_30()
    // loop:
    //   state->[+4] = -1
    // inner:
    //   if (sub_623120() == 1) goto check_exit
    //   sub_5B9F30(state, 0) ; if (sub_5BA060(state, 0) == 1) goto check_exit
    //   sub_532100(0)
    //   if (state->[+4] >= 0) goto check_exit
    //   goto inner
    // check_exit:
    //   if (state->[+4] == 1):
    //       ShowWindow(state->[0], 0)
    //       if (!Host_Start_Game()) { ShowWindow(state->[0], 5) ; goto loop }
    //       var = 1
    //   Dialog_End(state->[0]) ; dword_ABF10C = 0
    //   if (state->[+8] == 1) Game_SaveNetworkConfig(Session)
    //   if (state->[+4] == 2) SignOff()
    //   WWDebugString("ModemHost.Dialog(exit %s)\n", var ? "true" : "false")
    //   return var
    // exit_fail:
    //   WWDebugString("ModemHost.Dialog(exit %s)\n", "false") ; return 0
    // ------------------------------------------------------------------------
    bool Host_Dialog();

    // ------------------------------------------------------------------------
    // asm: ModemGuest_ReadDialog (0x5B5700)
    //
    //   WWDebugString("ModemGuest.ReadDialog(enter)\n")
    //   hDlg = [state]
    //   item = GetDlgItem(hDlg, 5C1h)
    //   if (item): SendMessageA(item, 4B3h, 14h, &lParam)
    //              sub_735090(&nameBuf, &lParam, -1)
    //   WWDebugString("Name: %s\n", nameBuf)
    //   item = GetDlgItem(hDlg, 405h)
    //   if (item): gPlayerColor = SendMessageA(item, 147h, 0, 0)
    //   WWDebugString("Color: %d\n", gPlayerColor)
    //   item = GetDlgItem(hDlg, 667h)
    //   if (item): sel = SendMessageA(item, 147h, 0, 0)
    //              SendMessageA(item, 150h, sel, 0)
    //              dword_A8B3AC = sel
    //   WWDebugString("ModemGuest.ReadDialog(exit)\n")
    // ------------------------------------------------------------------------
    void Guest_ReadDialog();

    // ------------------------------------------------------------------------
    // asm: ModemHost_SelectMultiMap (0x5B9B00)
    //
    //  这是 ModemHost 对话框里"选地图"按钮的处理体（sub_5B7970+741 调用）：
    //   1) 把全局主机名（byte_A8B8E0 窄串 / word_A8B322 宽串）拷进栈上的
    //      Dest[0x100]，随后这份拷贝**再没被读过**（原版就这样）；
    //   2) 记下 gScenarioIndex（ebp）；
    //   3) ShowWindow(hWnd,0) 隐藏主机窗口，跑地图选择对话框（返回值 2 = OK）；
    //   4) OK 路径：恢复 gScenarioIndex -> investigation -> 隐藏对话框 ->
    //      ShowWindow(5)；若选中的是 "RandMap.Sed" 则重建 RandMap.img 预览面
    //      并连发两次 InvalidateRect，否则只隐藏 + 一次 InvalidateRect；
    //   5) 取消路径：ShowWindow(5) -> investigation(gScenarioIndex) == 1 时
    //      把宽主机名塞进 0x5A8 控件并重复第 4 步的 RandMap 判定；
    //   6) 出口统一 byte[this+9] = 1。
    // ------------------------------------------------------------------------
    void Host_Select_Multi_Map();

    // ------------------------------------------------------------------------
    // asm: MapPreview_Download (0x5B7090)
    //
    //  ModemGuest 收到主机广播后拉取地图预览图：
    //   ack 包 0xFA 字节、首字节 'v'；0x258 tick 超时窗口内反复
    //   Call_Back + 收包检查；随后 Get_File_From_Host 取回压缩预览，
    //   RawFile -> 解压 -> 建面 -> 换掉 dword_AC1154 -> InvalidateRect，
    //   最后把栈上四个临时对象逐个拆掉。
    // ------------------------------------------------------------------------
    void Map_Preview_Download();

    // asm: ModemHost_StartGame (0x5BAC40) —— 点"开始"之后的建场入口
    //   缺口：握手要求对话框里已填好的玩家表（0x85 节点数组）与 GO/回执
    //   载荷结构，两者都在未建模的拨号会话协议里；如实返回 false。
    virtual bool Host_Start_Game();
    // asm: ModemHost_SignOff (0x5B9F50) —— 已落地：发 0x66 挂断包（0xFA 字节）、
    //   0x12C tick 内等同一席位回执；TXT_SIGNING_OFF 的模态提示与
    //   sub_5F0040 的会话清理属窗口层/拨号会话（缺口）。
    virtual void SignOff();

protected:
    // --- 窗口系统（无实体，默认"无窗口"） -----------------------------------
    //   asm 依据与缺口逐条列在 ModemClass.cpp 的实现处：
    //     Dialog_Show       asm: ModemHost_Dialog (0x5B7990) 的
    //                         Dialog_Show(0xF1, sub_5B7970, 0)
    //     Dialog_End        asm: ModemHost_Dialog (0x5B7990) +0x14E
    //     Get_Dlg_Item      asm: ModemHost_SignOff (0x5B9F50) 等处的 GetDlgItem
    //     Send_Dlg_Message  asm: 各处 SendMessageA
    //     Set_Window_Long   asm: ModemHost_Dialog (0x5B7990) +0x3A
    //     Show_Window       asm: 各处 ShowWindow(hWnd, nCmdShow)
    //   缺口：本工程没有 Win32 窗口层与拨号对话框资源（模板 id 0xF1 不存在），
    //   因此这六个入口如实返回"无窗口 / 无控件"，不伪造成功。
    virtual void* Dialog_Show(int32 id, void* pProc, void* pParam);
    virtual void  Dialog_End();
    virtual void* Get_Dlg_Item(void* hDlg, int32 id);
    virtual int32 Send_Dlg_Message(void* hWnd, int32 msg, int32 wParam, int32 lParam);
    virtual void  Set_Window_Long(void* hWnd, int32 idx, void* val);
    virtual void  Show_Window(int32 cmd);
    virtual void  Copy_Guest_Name(void* pDst, const void* pSrc, int32 a3);

    // --- 对话框内部处理器 ---------------------------------------------------
    //   Host_Init          asm: sub_5B82F0 (0x5B82F0) —— 已落地：RulesClass
    //                      多人默认值灌回会话；对话框文本段属窗口层（缺口）。
    //   Wnd_Init           asm: sub_622800 (0x622800) —— 装窗口过程；无窗口（缺口）。
    //   Is_Quit_Requested  asm: sub_623120 (0x623120) —— 已落地：Call_Back +
    //                      Main_Loop 泵，退出判据用 GameIsQuitting 承接。
    //   Process_Host       asm: sub_5B9F30 (0x5B9F30) —— 已落地：服务收发队列 +
    //                      每 0x28ms 发一次 0xFA 字节心跳包（首双字 0x69）。
    //   Process_Host2      asm: sub_5BA060 (0x5BA060) —— 已落地"把已到达的包
    //                      全部取走"；0x64/0x68/0x6D 三个包型的分派体属于
    //                      拨号会话协议（缺口），因此一律返回 0（不退出）。
    //   Blit_Dialog        asm: sub_532100 (0x532100) —— 已落地焦点判定；
    //                      GScreen 隐藏表面的 blit 未建模（缺口）。
    virtual void Host_Init();
    virtual void Wnd_Init(void* hWnd);
    virtual int32 Is_Quit_Requested();
    virtual void Process_Host();
    virtual int32 Process_Host2();
    virtual void Blit_Dialog();

public:
    void*  Hwnd;                 // +0x00
    int32  DialogResult;         // +0x04
    uint8  SaveNetworkConfig;    // +0x08
    uint8  Field09;              // +0x09
    uint8  pad_0A[2];
    int32  StartTick;            // +0x0C
    int32  Timeout1;             // +0x10
    int32  StartTick2;           // +0x14
    int32  Timeout2;             // +0x18
    int32  Field1C;              // +0x1C
    uint8  Field20;              // +0x20
    uint8  pad_21;
    uint16 Field22;              // +0x22
    uint8  pad_24[0x28];
    int32  Field4C;              // +0x4C
    int32  Field50;              // +0x50
    int32  Timer54;              // +0x54
    int32  Field58;              // +0x58
    int32  Field5C;              // +0x5C
};
