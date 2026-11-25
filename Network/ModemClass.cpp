#include "ModemClass.h"

#include "../Game/Game.h"
#include "../Game/Externs.h"
#include "../Game/MainLoop.h"
#include "../Game/GameInit.h"
#include "../IO/FileSystem.h"
#include "../Rules/RulesClass.h"
#include "Networking.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "SessionClass.h"

// ============================================================================
// ModemClass - 拨号对战状态块
//
// 控制流逐段对齐 asm 0x5B7990（Host_Dialog）与 0x5B5700（Guest_ReadDialog）。
// 窗口/对话框层在本项目没有实体，所有落到窗口系统的动作用 protected 虚口
// 承接，默认实现返回"无窗口"（见头文件降级点说明）。
// ============================================================================

namespace
{
// 引擎调试输出通道（原版 WWDebugString）。
void WWDebugString(const char* pFormat, ...)
{
    if (pFormat == nullptr)
        return;
    va_list args;
    va_start(args, pFormat);
    std::vfprintf(stderr, pFormat, args);
    va_end(args);
}

// asm: dword_A8EA70 —— 拨号连接的全局槽。原版是一个 Modem 连接对象的
// 地址；本工程取 Networking 的第 0 号连接（与 Map_Preview_Download 同一做法）。
ConnectionClass* ModemConnection()
{
    NetworkingClass* pNet = NetworkingClass::GetInstance();
    if (pNet == nullptr || pNet->Connections.GetCount() <= 0)
        return nullptr;
    return pNet->Connections[0];
}
} // namespace


ModemClass* ModemClass::Instance = nullptr;      // asm: dword_ABF10C
char  ModemClass::GuestName[0x40] = { 0 };       // asm: word_A8B380
int32 ModemClass::PlayerColor = 0;               // asm: gPlayerColor
int32 ModemClass::GuestColorIndex = 0;           // asm: dword_A8B3AC

ModemClass::ModemClass() noexcept
    : Hwnd(nullptr)
    , DialogResult(-1)
    , SaveNetworkConfig(0)
    , Field09(0)
    , pad_0A{ 0, 0 }
    , StartTick(0)
    , Timeout1(0)
    , StartTick2(0)
    , Timeout2(0)
    , Field1C(0)
    , Field20(0)
    , pad_21(0)
    , Field22(0)
    , pad_24{}
    , Field4C(-1)
    , Field50(-1)
    , Timer54(0)
    , Field58(0)
    , Field5C(0)
{
}

ModemClass::~ModemClass() = default;

// ----------------------------------------------------------------------------
// 窗口 / 对话框层的降级边界
//
//   这一层在原版是 Win32：CreateDialogParam + GetDlgItem + SendMessageA +
//   ShowWindow + SetWindowLongA。本工程没有窗口层，也没有拨号资源脚本
//   （拨号对话框模板不在资源表里），因此下面这几个入口如实返回
//   "没有窗口 / 没有控件"，不伪造任何成功状态。调用方（Host_Dialog /
//   Guest_ReadDialog）按 asm 已有的 `if (!hWnd) goto exit_fail` 分支处理。
//
//   逐个对应 asm 里的直接调用：
//     Dialog_Show       asm: ModemHost_Dialog (0x5B7990) 里
//                            Dialog_Show(0xF1, sub_5B7970, 0)
//     Dialog_End        asm: ModemHost_Dialog+0x14E / ModemHost_SignOff+0x13A
//     Get_Dlg_Item      asm: ModemHost_SignOff (0x5B9F50) 等处的 GetDlgItem
//     Send_Dlg_Message  asm: 各处 SendMessageA
//     Set_Window_Long   asm: ModemHost_Dialog+0x3A 的 SetWindowLongA(hWnd,8,state)
//     Show_Window       asm: 各处 ShowWindow(hWnd, nCmdShow)
// ----------------------------------------------------------------------------
void* ModemClass::Dialog_Show(int32 /*id*/, void* /*pProc*/, void* /*pParam*/)
{
    return nullptr;                 // 没有对话框模板 -> 无窗口
}

void ModemClass::Dialog_End()
{
    // asm: EndDialog / DestroyWindow；无窗口可销毁。
}

void* ModemClass::Get_Dlg_Item(void* /*hDlg*/, int32 /*id*/)
{
    return nullptr;                 // 无窗口 -> 永远取不到控件
}

int32 ModemClass::Send_Dlg_Message(void* /*hWnd*/, int32 /*msg*/,
                                   int32 /*wParam*/, int32 /*lParam*/)
{
    return 0;                       // 无控件 -> 消息无处可投递
}

void ModemClass::Set_Window_Long(void* /*hWnd*/, int32 /*idx*/, void* /*val*/)
{
    // asm: SetWindowLongA(hWnd, 8, state) —— 把状态块挂到窗口用户数据上
}

void ModemClass::Show_Window(int32 /*cmd*/)
{
    // asm: ShowWindow(hWnd, nCmdShow)；0 = 隐藏，5 = 显示
}

void ModemClass::Copy_Guest_Name(void* pDst, const void* pSrc, int32)
{
    // asm: sub_735090(dst, src, -1) —— 第三参 -1 表示"拷到 NUL 为止"
    if (pDst != nullptr && pSrc != nullptr)
        std::snprintf(static_cast<char*>(pDst), sizeof(GuestName), "%s",
                      static_cast<const char*>(pSrc));
}

// ----------------------------------------------------------------------------
// asm: sub_5B82F0 (0x5B82F0) —— ModemHost.InitDialog
//
//   byte_82C938 == 1 时，先把 RulesClass 里的多人默认值灌回会话：
//     gMoney            = Rules->Money
//     gBases            = Rules->Bases
//     gBridgeDestruction= Rules->BridgeDestruction
//     gCrates           = Rules->Crates
//     gAIPlayers        = 0
//     gUnitCount        = Rules->UnitCount
//     Session::ShortGame= 0
//     gSWAllowed        = Rules->SuperWeaponsAllowed
//     gGameSpeed        = Options
//     gBuildOffAlly     = Rules->BuildOffAlly
//     gMultiEngineer    = 0
//   随后是对话框文本填充（名称 0x5C1 / 颜色 0x405 / 国家 0x667 ...），
//   那一段需要控件句柄，属于窗口层（缺口，见上）。
//
//   byte_82C938（"上一次会话退出、需要按规则复位"的旗子）在本工程没有
//   置位点，这里以静态量承接，初值取 1，即首次进入时按 asm 复位一次。
// ----------------------------------------------------------------------------
namespace
{
bool s_ScenarioFlagsRestore = true;     // asm: byte_82C938
} // namespace

void ModemClass::Host_Init()
{
    std::printf("ModemHost.InitDialog(enter)\n");   // asm: aModemhost_init

    if (s_ScenarioFlagsRestore)
    {
        s_ScenarioFlagsRestore = false;
        // asm: nullsub_36(&ScenarioFlagsBackup) —— 场景旗标备份的回收

        RulesClass* pRules = RulesClass::Instance;
        SessionClass* pSession = SessionClass::GetInstance();

        if (pSession != nullptr)
        {
            if (pRules != nullptr)
            {
                pSession->Credits = pRules->Money;                       // gMoney
                pSession->Bases = pRules->Bases;                         // gBases
                pSession->BridgesDestroyable = pRules->BridgeDestruction;
                pSession->Crates = pRules->Crates;                       // gCrates
                pSession->AIPlayers = 0;                                 // gAIPlayers
                pSession->UnitCount = pRules->UnitCount;                 // gUnitCount
                pSession->ShortGame = false;                             // Session::ShortGame
                pSession->SuperWeapons = pRules->SuperWeaponsAllowed;    // gSWAllowed
                gSWAllowed = pSession->SuperWeapons;                     // asm: 同一个全局
                pSession->GameSpeed = pRules->GameSpeed;                 // gGameSpeed
                pSession->BuildOffAlly = pRules->BuildOffAlly;           // gBuildOffAlly
                pSession->MultiEngineer = false;                         // gMultiEngineer
            }
            else
            {
                pSession->AIPlayers = 0;
                pSession->ShortGame = false;
                pSession->MultiEngineer = false;
            }
        }
    }

    // asm: GetDlgItem(hDlg,0x5C1) + WM_LIMITTEXT(0xC5,0x13) + WM_SETTEXT(0x4B2)
    //      + "Name: %ls" 等文本回填。控件层缺失，见上。
    std::printf("Name: %s\n", GuestName);
}

// ----------------------------------------------------------------------------
// asm: sub_622800 (0x622800) —— 给对话框装窗口过程/回调。
//   窗口层缺失，无对象可装。
// ----------------------------------------------------------------------------
void ModemClass::Wnd_Init(void* /*hWnd*/)
{
}

// ----------------------------------------------------------------------------
// asm: sub_623120 (0x623120) —— ModemHost.Dialog 的"是否该退出"查询：
//     Call_Back()                                  ; 心跳
//     if (Session == null || Session == 5 || byte_A8D60E || dword_A8DAB4)
//         { Call_Back() ; return 0 }
//     if (sub_55CBF0()) return 0
//     if (Main_Loop() == true) return 1
//     Call_Back() ; return 0
//
//   本工程的 Main_Loop 是 void，用 GameIsQuitting 承接它"返回真"的语义
//   （GameInit 在退出路径上把它置 1）。
// ----------------------------------------------------------------------------
int32 ModemClass::Is_Quit_Requested()
{
    Call_Back();                                     // asm: call sub_5D4D50 -> Call_Back

    SessionClass* pSession = SessionClass::GetInstance();
    if (pSession == nullptr || pSession->SessionType == 5)
    {
        Call_Back();
        return 0;
    }

    // asm: byte_A8D60E / dword_A8DAB4 两个"会话暂停/已结束"旗标；本工程
    // 分别对应 GameState 的暂停与结束。
    if (pSession->GameState == GameSessionState::Paused ||
        pSession->GameState == GameSessionState::Ended)
    {
        Call_Back();
        return 0;
    }

    Main_Loop();                                     // asm: call Main_Loop

    if (GameIsQuitting)                              // asm: Main_Loop 返回真
        return 1;

    Call_Back();
    return 0;
}

// ----------------------------------------------------------------------------
// asm: sub_5B9F30 (0x5B9F30) —— ModemHost 空闲服务
//
//   sub_5F1C60(conn)                                ; 服务收发队列
//   if (byte[this+9] == 1) {                        ; 刚进/刚刷新
//       if (now - [this+0Ch] > 0x1E) {
//           sub_5BA660(this)                        ; 重画状态
//           [this+0Ch] = now ; byte[this+9] = 0
//       }
//   }
//   if (now - [this+1Ch] > 0x28) {                  ; 每 40ms 一次心跳
//       pkt[0xFA] 清零 ; *(u32*)pkt = 0x69         
//       pkt[0x18] = byte(dword_A8DB30)              ; 本机席位
//       *(u32*)(pkt+0x9A) = sub_5F1EE0(conn)        ; 序号
//       sub_5F1BF0(conn, pkt, 0xFA, 0)              ; 发送
//       [this+1Ch] = now
//   }
// ----------------------------------------------------------------------------
void ModemClass::Process_Host()
{
    ConnectionClass* pConn = ModemConnection();

    if (pConn != nullptr)
        pConn->Service_Send_Queue();                 // asm: sub_5F1C60

    const int32 now = static_cast<int32>(Game::GetTickCount());

    if (Field09 == 1)
    {
        if (now - StartTick > 0x1E)
        {
            // asm: sub_5BA660(this) —— 状态面板重绘（窗口层缺失）
            StartTick = now;
            Field09 = 0;
        }
    }

    if (now - Field1C > 0x28)
    {
        uint8 packet[0xFA];
        std::memset(packet, 0, sizeof(packet));
        *reinterpret_cast<uint32*>(packet) = 0x69;   // asm: mov [esp+var_FC],69h
        packet[0x18] = static_cast<uint8>(Field20);   // asm: var_E4 <- byte A8DB30
        const uint32 seq = (pConn != nullptr)
            ? pConn->SequenceNumber : 0;              // asm: sub_5F1EE0
        std::memcpy(packet + 0x9A, &seq, sizeof(seq));// asm: var_62

        if (pConn != nullptr)
            pConn->SendPacket(packet, 0xFA);          // asm: sub_5F1BF0

        Field1C = now;
    }
}

// ----------------------------------------------------------------------------
// asm: sub_5BA060 (0x5BA060) —— ModemHost 收包处理
//
//   循环 sub_5F1C30(conn, &pkt, &len)：收到包就按首字节分派——
//   0x64('d') / 0x68('h') / 0x6D('m')，并对"自己是发送者"的回环包打折；
//   任一分支要求退出时返回 1。等待超时会弹 TXT_SYSTEM_NOT_RESPONDING。
//
//   本工程没有拨号传输层，也没有那三类包的载荷结构；这里如实做的是
//   "把连接上已到达的包全部取走并计数"，不伪造分派结果，返回 0（不退出）。
// ----------------------------------------------------------------------------
int32 ModemClass::Process_Host2()
{
    ConnectionClass* pConn = ModemConnection();
    if (pConn == nullptr)
        return 0;

    uint8 buffer[0x400];
    int32 received = 0;
    for (;;)
    {
        const int32 len = pConn->ReceivePacket(buffer, sizeof(buffer));
        if (len <= 0)
            break;
        ++received;

        // asm: cmp [pkt], 64h/68h/6Dh —— 三个包型的分派体未在本层还原
        //   （载荷结构属于 ModemHost/ModemGuest 的会话协议）。
        WWDebugString("ModemHost: received %d-byte packet (type %02Xh)\n",
                      len, len > 0 ? buffer[0] : 0);
    }

    (void)received;
    return 0;
}

// ----------------------------------------------------------------------------
// asm: sub_532100 (0x532100) —— 对话框重绘
//     if (!FocusGained && !arg) return;
//     FocusGained = 0; sub_52FEC0(0, 1); GScreenClass::Do_Blit(DSurface_Hidden,...)
//   本工程有 TheGScreen，但没有"隐藏表面"这一层；这里保留焦点判定与
//   焦点位的清除，blit 本身因目标表面缺失而跳过（如实不做）。
// ----------------------------------------------------------------------------
namespace
{
bool s_FocusGained = false;      // asm: FocusGained
} // namespace

void ModemClass::Blit_Dialog()
{
    if (!s_FocusGained)
        return;                  // asm: test al,al / jz retn

    s_FocusGained = false;       // asm: mov FocusGained,0
    // asm: sub_52FEC0(0,1) —— 焦点切换的电影/界面服务，本工程无对应入口
    // asm: GScreenClass::Do_Blit(DSurface_Hidden, 0, 0) —— 隐藏表面未建模
}

// ----------------------------------------------------------------------------
// asm: ModemHost_StartGame (0x5BAC40)
//
//   依次：检查剧本可用人数（sub_5E6520(gScenarioIndex)）够不够；
//   为每个玩家建 0x85 的名牌节点；拼 "go" 包（0xFA 字节）并发送；
//   弹 TXT_SENDING_GO；随后 0x708 tick 内等待对端 GO 回执，
//   期间 Call_Back + 会话服务；失败弹 TXT_WAITING_GO。
//
//   缺口：这段握手要求"对话框里已填好的玩家表"（0x85 节点数组）与
//   "GO/回执"的载荷结构，两者都在未建模的拨号会话协议里；本工程也没有
//   拨号传输层可发。函数如实返回 false（开局未完成），Host_Dialog 会按
//   asm 原样回到等待循环。
// ----------------------------------------------------------------------------
bool ModemClass::Host_Start_Game()
{
    std::printf("ModemHost.StartGame(enter)\n");     // asm: aModemhost_start
    return false;
}

// ----------------------------------------------------------------------------
// asm: ModemHost_SignOff (0x5B9F50)
//
//   1) pkt[0xFA] 清零 ; *(u32*)pkt = 0x66
//      pkt[0x18] = byte(dword_A8DB30) ; pkt[0x1D] = byte(gPlayerColor)
//      sub_5F1BF0(conn, pkt, 0xFA, 0)              ; 广播挂断
//   2) Get_String("TXT_SIGNING_OFF") -> MsgBox（模态提示，窗口层缺失）
//   3) 0x12C tick 内循环 Call_Back + sub_5D4D50 + sub_5F1C60 + sub_5F1EA0，
//      直到收到同一席位回的 0x66 包
//   4) sub_5F0040（会话清理）; Dialog_End
// ----------------------------------------------------------------------------
void ModemClass::SignOff()
{
    ConnectionClass* pConn = ModemConnection();

    uint8 packet[0xFA];
    std::memset(packet, 0, sizeof(packet));
    *reinterpret_cast<uint32*>(packet) = 0x66;       // asm: mov [esp+var_1F8],66h
    packet[0x18] = static_cast<uint8>(Field20);       // asm: var_1E0
    packet[0x1D] = static_cast<uint8>(PlayerColor);   // asm: var_1DB

    if (pConn != nullptr)
        pConn->SendPacket(packet, 0xFA);              // asm: sub_5F1BF0

    // asm: Get_String("TXT_SIGNING_OFF") + MsgBox —— 模态提示（窗口层缺失）

    const int32 start = static_cast<int32>(Game::GetTickCount());
    if (pConn != nullptr)
    {
        uint8 reply[0x400];
        for (;;)
        {
            const int32 elapsed = static_cast<int32>(Game::GetTickCount()) - start;
            if (elapsed >= 0x12C)                     // asm: cmp esi,12Ch / jnb
                break;

            Call_Back();                              // asm: call Call_Back
            pConn->Service_Send_Queue();              // asm: sub_5D4D50 / 5F1C60

            const int32 len = pConn->ReceivePacket(reply, sizeof(reply));
            if (len <= 0)
                continue;

            if (len >= 4 && *reinterpret_cast<uint32*>(reply) == 0x66)
            {
                if (reply[0x18] == packet[0x18])      // 同一席位回的挂断包
                    break;
            }
        }
    }

    // asm: sub_5F0040 —— 会话清理（本工程无拨号会话对象）
    Dialog_End();                                     // asm: call Dialog_End
}

// ============================================================================
// asm: ModemHost_Dialog (0x5B7990)
// ============================================================================
bool ModemClass::Host_Dialog()
{
    uint8 result = 0;

    std::printf("ModemHost.Dialog(enter)\n");          // asm: WWDebugString
    DialogResult = -1;                                  // asm: mov [state+4],-1

    // asm: Dialog_Show(0xF1, sub_5B7970, 0) ; test eax,eax ; jz exit_fail
    Hwnd = Dialog_Show(0xF1, nullptr, nullptr);
    if (Hwnd == nullptr)
        goto exit_fail;

    Set_Window_Long(Hwnd, 8, this);                     // asm: SetWindowLongA(hWnd,8,state)
    Host_Init();                                        // asm: sub_5B82F0(state)
    Wnd_Init(Hwnd);                                     // asm: sub_622800(hWnd)

    SaveNetworkConfig = 0;                              // asm: mov byte [state+8],0
    Field09 = 1;                                        // asm: mov byte [state+9],1

    // asm: 两次取 tick（dword_887340 [+ (Tick()-dword_887338)]）
    // 降级点：原版把"累计游戏时间 + 自启动以来的增量"两段拆开存；本项目
    // 只有单调毫秒时基，故两处都取 Game::GetTickCount()。
    StartTick = static_cast<int32>(Game::GetTickCount());
    Timeout1 = 0x2710;                                  // asm: mov [state+10h],2710h
    StartTick2 = static_cast<int32>(Game::GetTickCount());
    Timeout2 = 0x4B0;                                   // asm: mov [state+18h],4B0h

    Field1C = 0;                                        // asm: mov [state+1Ch],0
    Field20 = 0;                                        // asm: mov byte [state+20h],0
    Field22 = 0;                                        // asm: mov word [state+22h],0
    Field4C = -1;                                       // asm: mov [state+4Ch],-1
    Field50 = -1;                                       // asm: mov [state+50h],-1

    Instance = this;                                    // asm: mov dword_ABF10C, state

    Timer54 = 0;                                        // asm: sub_679D90()
    // asm: Game_Load_ARTMD_INI()
    //   artmd.ini 在本项目的等价入口是 CCINIClass::GetArtINI()，规则装载阶段
    //   已完成，这里不再重复装载。
    Field58 = 0;                                        // asm: unknown_libname_29()
    Field5C = 0;                                        // asm: unknown_libname_30()

loop:
    DialogResult = -1;                                  // asm: mov [state+4],-1
    for (;;) {
        if (Is_Quit_Requested() == 1)                   // asm: sub_623120()
            goto check_exit;
        Process_Host();                                 // asm: sub_5B9F30(state,0)
        if (Process_Host2() == 1)                       // asm: sub_5BA060(state,0)
            goto check_exit;
        Blit_Dialog();                                  // asm: sub_532100(0)
        if (DialogResult >= 0)                          // asm: cmp [state+4],0 ; jge
            goto check_exit;
    }

check_exit:
    if (DialogResult == 1) {
        Show_Window(0);                                 // asm: ShowWindow(state->[0], 0)
        if (!Host_Start_Game()) {
            Show_Window(5);                             // asm: ShowWindow(state->[0], 5)
            goto loop;
        }
        result = 1;
    }

    Dialog_End();                                       // asm: Dialog_End(state->[0])
    Instance = nullptr;                                 // asm: mov dword_ABF10C, 0

    if (SaveNetworkConfig == 1) {
        // asm: Game_SaveNetworkConfig(Session)
        //   项目的 SessionClass 目前没有独立的"写回网络配置"入口，规则写在
        //   SaveGame/WriteINI 路径上；此处保留判据不动（降级点）。
    }
    if (DialogResult == 2)                              // asm: cmp [state+4],2
        SignOff();

    std::printf("ModemHost.Dialog(exit %s)\n", result ? "true" : "false");
    return result != 0;

exit_fail:
    std::printf("ModemHost.Dialog(exit %s)\n", "false");
    return false;
}

// ============================================================================
// asm: ModemGuest_ReadDialog (0x5B5700)
// ============================================================================
void ModemClass::Guest_ReadDialog()
{
    std::printf("ModemGuest.ReadDialog(enter)\n");      // asm: WWDebugString

    void* hDlg = Hwnd;                                  // asm: mov eax,[state]

    // ---- 名字 --------------------------------------------------------------
    char lParam[0x40] = { 0 };
    void* item = Get_Dlg_Item(hDlg, 0x5C1);             // asm: GetDlgItem(hDlg,5C1h)
    if (item != nullptr) {
        // asm: SendMessageA(item, 4B3h /*WM_GETTEXT*/, 14h, &lParam)
        Send_Dlg_Message(item, 0x4B3, 0x14,
                         static_cast<int32>(reinterpret_cast<uintptr_t>(lParam)));
        Copy_Guest_Name(GuestName, lParam, -1);         // asm: sub_735090
    }
    std::printf("Name: %s\n", GuestName);               // asm: WWDebugString

    // ---- 玩家颜色 ----------------------------------------------------------
    item = Get_Dlg_Item(hDlg, 0x405);                   // asm: GetDlgItem(hDlg,405h)
    if (item != nullptr) {
        // asm: SendMessageA(item, 147h /*CB_GETCURSEL*/, 0, 0)
        PlayerColor = Send_Dlg_Message(item, 0x147, 0, 0);
    }
    std::printf("Color: %d\n", PlayerColor);            // asm: WWDebugString

    // ---- 颜色槽（第二个下拉） ----------------------------------------------
    item = Get_Dlg_Item(hDlg, 0x667);                   // asm: GetDlgItem(hDlg,667h)
    if (item != nullptr) {
        const int32 sel = Send_Dlg_Message(item, 0x147, 0, 0);
        Send_Dlg_Message(item, 0x150, sel, 0);          // asm: CB_SETCURSEL
        GuestColorIndex = sel;                          // asm: mov dword_A8B3AC,sel
    }

    std::printf("ModemGuest.ReadDialog(exit)\n");       // asm: WWDebugString
}

// ============================================================================
// asm: ModemHost_SelectMultiMap (0x5B9B00)
//
//  降级点（本工程未建模的外部入口，逐个对应 asm 里的直接调用，不再经 bind）：
//    sub_5E68A0  地图选择对话框    -> s_SelectMapDialog()
//    investigation(索引)           -> s_Investigation()
//    sub_5E74E0  隐藏对话框        -> 走基类 Show_Window(0)
//    sub_641DB0  装载 RandMap.img  -> s_LoadRandMapImg()
//  这些入口在原版里是对话框/绘图层的实体，本项目没有窗口系统，故各自退化为
//  "无对话框 / 无侦查"的默认值，控制流与判定顺序保持不变。
// ============================================================================
namespace {

// asm: byte_A8B8E0 —— 主机名（窄串）
char    s_HostNameA8B8E0[0x40] = { 0 };
// asm: word_A8B322 —— 主机名（宽串）
wchar_t s_HostNameWideA8B322[0x40] = { 0 };
// asm: InvalidateRect(hWnd,0,0) —— 窗口层在本工程未建模（与 Show_Window /
//   Get_Dlg_Item 等 protected 虚口同一处理方式），这里退化为空操作。
void InvalidateRect_Raw(void* /*hWnd*/, const void* /*pRect*/, int32 /*erase*/) {}

// asm: dword_A8B8CC —— 地图条目数组；条目本身 +0x58 处是文件名
void**  s_NameArrayA8B8CC = nullptr;
// asm: hWndParent —— 地图选择对话框的父窗口
void*   s_HwndParentA8B8CC = nullptr;

constexpr int32 MAPDLG_OK = 2;      // asm: cmp eax,2

// asm: sub_5E68A0 —— 地图选择对话框；返回 2 表示按下"确定"
int32 s_SelectMapDialog(void* /*hParent*/) { return 0; }

// asm: investigation —— 侦查（读回选中的地图信息）；返回 1 表示有效
uint8 s_Investigation(int32 /*idx*/) { return 0; }

// asm: sub_641DB0 —— 把地图预览 SHP 装进给定表面槽
void s_LoadRandMapImg(void* /*pSurface*/, const char* /*pName*/) {}

// asm: dword_AC1154 —— 地图预览表面槽（原版是 DSurface* 全局）
void* s_MapPreviewSurfaceAC1154 = nullptr;

// 共用的 RandMap.Sed 预览重建段（两处调用点同一份）
void s_RebuildRandMapPreview()
{
    if (s_MapPreviewSurfaceAC1154 != nullptr) {         // asm: test esi,esi ; jz
        delete static_cast<uint8*>(s_MapPreviewSurfaceAC1154);
        s_MapPreviewSurfaceAC1154 = nullptr;
    }
    s_MapPreviewSurfaceAC1154 = new uint8[4];           // asm: push 4 ; call new
    s_LoadRandMapImg(s_MapPreviewSurfaceAC1154, "RandMap.img");
}

} // namespace

void ModemClass::Host_Select_Multi_Map()
{
    void* hWnd = Hwnd;                                  // asm: mov ebx,ecx ; [ebx]

    // asm: repne scasb(strlen) + rep movsd/movsb —— 把窄主机名拷进 Dest
    wchar_t Dest[0x100] = { 0 };
    {
        const size_t n = std::strlen(s_HostNameA8B8E0);
        const size_t bytes = (n + 1 < sizeof(Dest)) ? (n + 1) : sizeof(Dest);
        std::memcpy(Dest, s_HostNameA8B8E0, bytes);
    }
    const int32 savedIdx = (SessionClass::GetInstance() != nullptr)
        ? SessionClass::GetInstance()->ScenarioIndex : 0;    // asm: mov ebp,gScenarioIndex
    {
        // asm: push offset word_A8B322 / call _wcscpy —— 宽串覆盖上面那份拷贝，
        // 之后 Dest 再没被读取（原版行为，保留同一份拷贝动作）。
        std::wcsncpy(Dest, s_HostNameWideA8B322, 0x100 - 1);
    }
    (void)Dest;

    std::printf("ModemHost.SelectMultiMap(enter)\n");    // asm: WWDebugString

    Show_Window(0);                                     // asm: ShowWindow(hWnd,0)
    const int32 dlgrc = s_SelectMapDialog(s_HwndParentA8B8CC);

    if (dlgrc != MAPDLG_OK)                             // asm: cmp eax,2 ; jnz 5B9B9F
        goto cancel_path;

    // ── OK 路径 ────────────────────────────────────────────────────────────
    if (SessionClass::GetInstance() != nullptr)
        SessionClass::GetInstance()->ScenarioIndex = savedIdx;   // asm: mov gScenarioIndex,ebp
    s_Investigation(savedIdx);                          // asm: investigation(ecx=ebp)
    Show_Window(0);                                     // asm: sub_5E74E0(hWnd)
    Show_Window(5);                                     // asm: ShowWindow(hWnd,5)
    {
        void* pEntry = (s_NameArrayA8B8CC != nullptr)
            ? s_NameArrayA8B8CC[savedIdx] : nullptr;
        const int32 r = (pEntry != nullptr)
            ? _strcmpi("RandMap.Sed",
                       static_cast<const char*>(pEntry) + 0x58)
            : 1;
        if (r == 0) {                                   // asm: loc_5B9B4x
            s_RebuildRandMapPreview();
            if (s_MapPreviewSurfaceAC1154 != nullptr &&
                *reinterpret_cast<int32*>(s_MapPreviewSurfaceAC1154) == 0)
                Show_Window(0);                         // asm: sub_5E74E0(hWnd)
            // asm: loc_5B9B73 —— 两次 InvalidateRect(hWnd,0,0)
            if (Hwnd != nullptr) {
                InvalidateRect_Raw(Hwnd, nullptr, 0);
                InvalidateRect_Raw(Hwnd, nullptr, 0);
            }
        } else {                                        // asm: loc_5B9B8A
            Show_Window(0);
            if (Hwnd != nullptr)
                InvalidateRect_Raw(Hwnd, nullptr, 0);
        }
    }
    goto end;

cancel_path:                                            // asm: loc_5B9B9F
    Show_Window(5);                                     // asm: ShowWindow(hWnd,5)
    {
        const uint8 ok = (SessionClass::GetInstance() != nullptr)
            ? s_Investigation(SessionClass::GetInstance()->ScenarioIndex) : 0;
        if (ok != 1) {                                  // asm: cmp al,1 ; jnz
            if (SessionClass::GetInstance() != nullptr)
                SessionClass::GetInstance()->ScenarioIndex = savedIdx;
            goto end;
        }
        void* item = Get_Dlg_Item(hWnd, 0x5A8);         // asm: GetDlgItem(hWnd,5A8h)
        if (item != nullptr)
            Send_Dlg_Message(item, 0x4B2, 0,            // asm: WM_SETTEXT
                             static_cast<int32>(reinterpret_cast<uintptr_t>(
                                 s_HostNameWideA8B322)));
        void* pEntry = (s_NameArrayA8B8CC != nullptr &&
                        SessionClass::GetInstance() != nullptr)
            ? s_NameArrayA8B8CC[SessionClass::GetInstance()->ScenarioIndex]
            : nullptr;
        const int32 r = (pEntry != nullptr)
            ? _strcmpi("RandMap.Sed", static_cast<const char*>(pEntry) + 0x58)
            : 1;
        if (r == 0) {
            s_RebuildRandMapPreview();
            if (s_MapPreviewSurfaceAC1154 != nullptr &&
                *reinterpret_cast<int32*>(s_MapPreviewSurfaceAC1154) == 0)
                Show_Window(0);
            if (Hwnd != nullptr)
                InvalidateRect_Raw(Hwnd, nullptr, 0);
        } else {
            Show_Window(0);
        }
    }

end:                                                    // asm: loc_5B9C71
    Field09 = 1;                                        // asm: mov byte[ebx+9],1
    std::printf("ModemHost.SelectMultiMap(exit)\n");    // asm: WWDebugString
}

// ============================================================================
// asm: MapPreview_Download (0x5B7090)
//
//  栈上四个临时对象（原版尺寸）：ack 0xFA / RawFile 0x148 / 文件名 0x100 /
//  12 字节 straw 头。流程：
//    tick 记 var_280 ; WWDebugString("Starting map preview download")
//    memset(ack,0,0xFA) ; ack[0]='v' ; "Sending preview mode ack"
//    SendPacket(conn, ack, 0xFA, 1)
//    tick 记 var_250 ; 超时 0x258 tick 的等待环：Call_Back + 收包检查
//    "Preview mode ack sent" / "Calling Get_File_From_Host ..."
//    Get_File_From_Host(filename,1,0) ; 0 -> "Download failed" 直接返回
//    RawFile(name) -> size -> 整块读入 -> 解压 -> 换掉 dword_AC1154 ->
//    InvalidateRect(hWnd,0,0) -> 释放 wide/comp、拆 straw/Buffer/RawFile
//
//  降级点：
//    * dword_A8EA70（连接槽）在本工程取 Networking 的第 0 号连接；
//    * Get_File_From_Host 的"等主机把文件发过来"会合流程尚未建模，这里返回
//      0，因而走 asm 的 "got_file is false. Download failed" 出口；
//    * LZOStraw 在本工程没有对应类（IO/Straws 只有 LCW/Base64/Buffer/PK），
//      解压那一段按原版的调用顺序保留结构，实际解压入口缺失。
// ============================================================================
void ModemClass::Map_Preview_Download()
{
    uint8 ack[0xFA];                                    // asm: a1.cd.io.field_4C 块
    std::memset(ack, 0, sizeof(ack));                   // asm: mov ecx,3Eh ; rep stosd
    ack[0] = 'v';                                       // asm: stosw ; field_4C='v'

    int32 startTick = static_cast<int32>(Game::GetTickCount());   // var_280
    std::printf("Starting map preview download\n");     // asm: WWDebugString

    // asm: sub_5F1BF0(conn, ack, 0FAh, 1)
    ConnectionClass* pConn = nullptr;
    NetworkingClass* pNet = NetworkingClass::GetInstance();
    if (pNet != nullptr && pNet->Connections.GetCount() > 0)
        pConn = pNet->Connections[0];                   // asm: dword_A8EA70

    std::printf("Sending preview mode ack\n");
    if (pConn != nullptr)
        pConn->SendPacket(ack, 0xFA);

    // ── 等待环（asm: loc_5B73FC）──────────────────────────────────────────
    startTick = static_cast<int32>(Game::GetTickCount());
    int32 remaining = 0x258;                            // asm: mov var_278,258h
    if (pConn != nullptr) {
        uint8 scratch[0x400];
        int32 avail = pConn->ReceivePacket(scratch, sizeof(scratch));   // asm: sub_5F1EA0
        if (avail > 0) {
            const int32 kNoStart = -1;                  // asm: or edi,-1
            for (;;) {
                if (startTick != kNoStart) {            // asm: cmp eax,edi ; jz
                    const int32 now = static_cast<int32>(Game::GetTickCount());
                    const int32 elapsed = now - startTick;
                    if (elapsed >= remaining)           // asm: cmp eax,esi ; jge
                        break;
                    remaining -= elapsed;               // asm: sub esi,eax
                }
                if (remaining == 0)                     // asm: cmp esi,ebp ; jz
                    break;
                // asm: call Call_Back —— 主循环心跳（本工程由 Main_Loop 承担）
                avail = pConn->ReceivePacket(scratch, sizeof(scratch));
                if (avail <= 0)                         // asm: test eax,eax ; jg loop
                    break;
            }
        }
    }

    std::printf("Preview mode ack sent\n");
    std::printf("Calling Get_File_From_Host to receive the file download\n");

    // asm: Get_File_From_Host(filename, 1, 0) —— 见上面的降级说明
    char filename[0x100] = { 0 };
    const int32 gotFile = 0;
    if (gotFile == 0) {                                 // asm: test al,al ; jnz
        std::printf("got_file is false. Download failed\n");
        return;
    }

    // 以下分支在原版是"已拿到预览文件"之后的处理；保留恢复出来的调用顺序。
    std::printf("Loading the compressed preview image\n");
    RawFileClass rawFile(filename);                     // asm: RawFileClass(name)
    const int32 size = rawFile.GetSize();               // asm: vt 调用取长度
    uint8* comp = (size > 0) ? new uint8[static_cast<size_t>(size)] : nullptr;
    if (comp != nullptr)
        rawFile.Read(comp, size);
    const int32 rawSize = (comp != nullptr)
        ? *reinterpret_cast<const int32*>(comp) : 0;    // asm: mov ebx,[edi]

    std::printf("Decompressing the preview image\n");
    // asm: straw 头 {vt,0,0} -> CCMemoryBuffer 接管 comp+4
    //        -> Base64Straw 换 vtable -> sub_55C720(raw+24h,1,2000h)
    //        -> Straw::Get_From -> LZOStraw::Get(wide = new(rawSize*2))
    // 降级点：LZOStraw 未建模，wide 缓冲保留分配但无解压入口。
    uint8* wide = (rawSize > 0)
        ? new uint8[static_cast<size_t>(rawSize) * 2] : nullptr;

    std::printf("Creating the new preview surface\n");
    if (s_MapPreviewSurfaceAC1154 != nullptr) {         // asm: mov eax,dword_AC1154
        delete static_cast<uint8*>(s_MapPreviewSurfaceAC1154);
        s_MapPreviewSurfaceAC1154 = nullptr;
    }
    s_MapPreviewSurfaceAC1154 = new uint8[4];           // asm: push 4 ; call new
    // asm: sub_6425F0(surface, wide) —— 建面入口未建模

    InvalidateRect_Raw(Hwnd, nullptr, 0);               // asm: [this] 即 hWnd

    std::printf("Cleaning up the temporary decompression buffers\n");
    delete[] wide;                                      // asm: delete wide
    delete[] comp;                                      // asm: delete comp
    rawFile.Close();                                    // asm: RawFileClass_DTOR
}
