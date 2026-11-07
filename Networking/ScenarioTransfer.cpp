// ============================================================================
// Networking/ScenarioTransfer.cpp
//
// asm 权威：Negotiate_Scenario_Transfer (0x795CA0，约 750 行)。
// 逐段对照的标签写在每段注释里（loc_795DBC / loc_795F5C / loc_79616E /
// loc_796481 / loc_79651A / loc_79669C ...）。
//
// 依赖落点（原版是引擎里的一组自由函数，本树按真实归属直调）：
//   sub_543080/540C60/541260/541370/5422D0/5424C0/543090 -> IPXManager
//   sub_53EF30 节点地址匹配        -> IPXManagerClass::Match_Node_Address
//   sub_7931A0 取首个连接名        -> IPXManagerClass::First_Active_Name
//   MP_ReadNthPlayerName          -> MP::ReadNthPlayerName
//   wstrcpy_version               -> Wide_From_Narrow（同款 8 槽轮转缓冲）
//   sub_735090 宽转窄拷贝          -> Copy_Wide_To_Narrow
//   WinTimerClass::Get_System_Tick_Count -> SystemTimer::GetTime
//   Game_IsRandomMap              -> Game::IsRandomMap
//   Call_Back                     -> ::Call_Back
//   sub_694760 / sub_6951F0 / Get_File_From_Host / sub_58BC20 / sub_7757E0
//                                 -> 本文件的文件传输层（见各函数注释）
// ============================================================================

#include "Networking/ScenarioTransfer.h"

#include "Network/IPXManagerClass.h"
#include "Network/ConnectionClass.h"
#include "Network/MPModes.h"
#include "Network/SessionClass.h"
#include "Scenario/ScenarioClass.h"
#include "Game/Game.h"
#include "Game/MainLoop.h"
#include "Game/MPTeams.h"
#include "Math/Timer.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

// asm: .data 里的两个全局
ScenarioTransferState g_ScenarioTransfer = {
    "",          // TransferName
    0,           // FileSize
    0,           // HasScenario
    0,           // HeaderFlag
    {0},         // RequestPlayers
    0,           // RequestUnknown
    0,           // RequestCount
    {-1, -1, -1, -1, -1, -1, -1, -1},   // VoteA（asm: rep stosd 8 个 -1）
    {0, 0, 0, 0, 0, 0, 0, 0}            // VoteB
};

const char* g_SecurePlayerName = nullptr;

// asm: dword_A8B244 —— 当前连接形态（0 无 / 1 拨号 / 2 串口 / 3 IPX）
int32 g_ConnectionType = 0;

namespace {

// ---------------------------------------------------------------------------
// 常量（asm 的直接字面量）
// ---------------------------------------------------------------------------
const int32 k_ShortTimeout = 0x12C;    // 300 ms
const int32 k_LongTimeout  = 0x384;    // 900 ms
const int32 k_SendTimeout  = 0x258;    // 600 ms
const int32 k_QueueTimeout = 0x4B0;    // 1200 ms
const int32 k_MinQueueLen  = 0x78;
const int32 k_SendSocket   = 0x2F;
const int32 k_PacketSize   = 0x1C7;    // rep 71h stosd + stosw + stosb
const int32 k_DontGoState  = 2;        // 失败态

// asm: dword_84A1E0 —— .data 里只有 4 项（1,2,0,1）；后续下标在原版
// 会读进相邻的 .data，这里按 4 项周期取值，保持"阈值比较"的语义。
const int32 k_VoteLimit[4] = {1, 2, 0, 1};

// asm: dword_ABED0C —— 传输进度计数（sub_58BC20 清零）
int32 g_TransferProgress = 0;

// asm: 每次收包用的四个全局（dword_A8D638 / unk_A8D800 / dword_A8D804 /
// word_A8D810）。这里以文件内静态量承载，生命周期与函数一致。
int32  g_PacketType = 0;
int32  g_PacketUnknown = 0;
void*  g_PacketData = nullptr;
int16  g_PacketWord = 0;

// ---------------------------------------------------------------------------
// 计时器（asm: WinTimerClass::Get_System_Tick_Count）
//   asm 每个计时点都 lea ecx,[局部计时器] 再取值，返回值和槽位同源。
// ---------------------------------------------------------------------------
inline int32 Tick(int32* pStorage)
{
    const int32 now = SystemTimer::GetTime();
    if (pStorage != nullptr)
        *pStorage = now;
    return now;
}

// asm: __strcmpi —— 名字比较不区分大小写
inline bool Ascii_Icmp_Equal(const char* pA, const char* pB)
{
    if (pA == nullptr || pB == nullptr)
        return pA == pB;
    while (*pA != '\0' && *pB != '\0') {
        char a = *pA;
        char b = *pB;
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b)
            return false;
        ++pA;
        ++pB;
    }
    return *pA == *pB;
}

// asm: sub_735090 —— 宽字符 -> 窄字符定长拷贝（ecx=dst, edx=src, 上限）
void Copy_Wide_To_Narrow(const wchar_t* pSrc, char* pDst, int32 limit)
{
    int32 i = 0;
    if (pSrc != nullptr) {
        while (pSrc[i] != 0 && (limit < 0 || i < limit)) {
            pDst[i] = static_cast<char>(pSrc[i] & 0xFF);
            ++i;
        }
    }
    pDst[i] = '\0';
}

// asm: wstrcpy_version —— 窄 -> 宽，8 槽轮转缓冲，单槽上限 0x400 字节。
wchar_t* Wide_From_Narrow(const char* pSrc)
{
    static wchar_t s_Buffers[8][0x400];
    static int32   s_Index = 0;

    if (pSrc == nullptr)
        return nullptr;

    wchar_t* const pDst = s_Buffers[s_Index];
    int32 n = 0;
    while (pSrc[n] != 0 && n < 0x3FF) {  // asm: 上限 400h，逐字符搬运
        pDst[n] = static_cast<wchar_t>(static_cast<unsigned char>(pSrc[n]));
        ++n;
    }
    pDst[n] = 0;                         // asm: word [edx] = 0
    pDst[0x3FF] = 0;                     // asm: word [eax+7FEh] = 0

    s_Index = (s_Index + 1) & 7;
    return pDst;
}

// asm: sub_7757E0 —— 模态等待窗口的创建/切换。
//   本工程没有窗口系统（同 ModemClass 的窗口层降级约定），这里只推进
//   一次引擎回调并报告"已空闲"，调用点的 while(...==1) 因而直接退出。
int32 Wait_For_Idle()
{
    Call_Back();
    return 0;
}

// asm: sub_58BC20 —— dword_ABED0C = 0
void Transfer_Reset_Progress()
{
    g_TransferProgress = 0;
}

// asm: sub_6951F0 —— 读本地剧本头，返回"本地有这份剧本"。
//   ecx = 剧本名，edx = 文件长度，arg0 = 头标志字节指针，arg4 = 已有副本
//   标志，arg8 = 1。名字是 RandMap.Sed 时直接失败（随机地图不参与交换）。
int32 Read_Scenario_Header(const char* pName, int32 size, int32* pHeaderFlag,
                           int32 hasScenario, int32 a5)
{
    (void)a5;

    if (pName == nullptr || pName[0] == '\0')
        return 0;
    if (Ascii_Icmp_Equal(pName, "RandMap.Sed"))
        return 0;

    std::FILE* pFile = std::fopen(pName, "rb");
    if (pFile == nullptr)
        return 0;

    int32 length = 0;
    if (std::fseek(pFile, 0, SEEK_END) == 0)
        length = static_cast<int32>(std::ftell(pFile));
    std::fclose(pFile);

    if (length <= 0)
        return 0;

    g_ScenarioTransfer.FileSize = (size > 0) ? size : length;
    g_ScenarioTransfer.HasScenario = (hasScenario != 0) ? hasScenario : 1;
    if (pHeaderFlag != nullptr)
        *pHeaderFlag = 1;
    return 1;
}

// asm: sub_694760 —— "Send file %s to clients\n"，把剧本按块发给所有连接。
//   原版带进度框与重试窗口；本树没有进度框，保留同样的分块发送与
//   0x258 ms 重试窗口，失败即返回 0（调用方会置成放弃态）。
int32 Send_File_To_Clients(const char* pName, int32 a2, int32 a3, int32 a4)
{
    (void)a2;
    (void)a3;
    (void)a4;

    std::printf("Send file %s to clients\n",
                (pName != nullptr) ? pName : "");

    if (pName == nullptr)
        return 0;

    std::FILE* pFile = std::fopen(pName, "rb");
    if (pFile == nullptr)
        return 0;

    ConnectionManager* pMgr = ConnectionManager::GetInstance();
    uint8 chunk[0x100];
    bool  ok = true;

    for (;;) {
        const size_t n = std::fread(chunk, 1, sizeof(chunk), pFile);
        if (n == 0)
            break;

        const int32 count = (pMgr != nullptr) ? pMgr->GetConnectionCount() : 0;
        for (int32 i = 0; i < count; ++i) {
            ConnectionClass* pConn = pMgr->GetConnection(i);
            if (pConn == nullptr || !pConn->Connected)
                continue;

            wchar_t* const pWideName = Wide_From_Narrow(pConn->PeerName);
            if (pWideName == nullptr)
                continue;

            int32 startTick = 0;
            int32 const start = Tick(&startTick);
            for (;;) {
                Call_Back();
                if (IPXManager.Send(chunk, k_SendSocket, 1, pWideName,
                                    static_cast<int32>(n)) != 0)
                    break;

                const int32 elapsed = Tick(&startTick) - start;
                if (elapsed >= k_SendTimeout) {
                    ok = false;
                    break;
                }
            }
            if (!ok)
                break;
        }
        if (!ok)
            break;
        g_TransferProgress += static_cast<int32>(n);
    }

    std::fclose(pFile);
    return ok ? 1 : 0;
}

// asm: Get_File_From_Host —— 从房主收一块文件数据，超时返回 0。
int32 Get_File_From_Host(void* pBuffer, int32 size, int32 a3)
{
    (void)a3;

    if (pBuffer == nullptr || size <= 0)
        return 0;

    int32 startTick = 0;
    const int32 start = Tick(&startTick);

    for (;;) {
        Call_Back();

        if (IPXManager.Receive(&g_PacketType, &g_PacketUnknown, &g_PacketData,
                               &g_PacketWord) != 0 &&
            g_PacketWord == static_cast<int16>(NET_YR_PACKET_WORD)) {
            if (IPXManager.PacketLength > 0) {
                int32 copy = IPXManager.PacketLength;
                if (copy > size)
                    copy = size;
                std::memcpy(pBuffer, IPXManager.PacketPayload,
                            static_cast<size_t>(copy));
                return 1;
            }
        }

        const int32 elapsed = Tick(&startTick) - start;
        if (elapsed >= k_QueueTimeout)
            return 0;
    }
}

// asm: dword_84A1E0[esi] —— 见 k_VoteLimit 的说明
inline int32 Vote_Limit(int32 index)
{
    return k_VoteLimit[index & 3];
}

// ---------------------------------------------------------------------------
// 连接表遍历（asm: dword_B77900 / dword_B77908，每项 0x6C 字节）
// ---------------------------------------------------------------------------
inline int32 Connection_Count()
{
    ConnectionManager* pMgr = ConnectionManager::GetInstance();
    return (pMgr != nullptr) ? pMgr->GetConnectionCount() : 0;
}

inline ConnectionClass* Connection_At(int32 index)
{
    ConnectionManager* pMgr = ConnectionManager::GetInstance();
    return (pMgr != nullptr) ? pMgr->GetConnection(index) : nullptr;
}

// 发送到"第一个活动连接"（asm loc_79620F / loc_796366 的 rep movsd + wstrcpy）
bool Send_To_First_Active(const uint8* pPacket, int32 length)
{
    const int32 count = Connection_Count();
    for (int32 i = 0; i < count; ++i) {
        ConnectionClass* pConn = Connection_At(i);
        if (pConn == nullptr || !pConn->Connected)          // asm: test byte [entry],1
            continue;

        wchar_t* const pName = Wide_From_Narrow(pConn->PeerName);
        if (pName == nullptr)
            continue;

        IPXManager.Send(pPacket, k_SendSocket, 1, pName, length);
        return true;
    }
    return false;
}

// asm: loc_796236 / loc_79638D —— 等发送队列排空
void Wait_Send_Queue_Drain(bool joiner)
{
    int32 startTick = 0;
    int32 timeout = 0;
    int32 const start = Tick(&startTick);

    timeout = joiner ? k_ShortTimeout : k_QueueTimeout;

    if (IPXManager.Get_Receive_Count() <= 0)
        return;

    for (;;) {
        const int32 elapsed = Tick(&startTick) - start;
        if (elapsed >= timeout)
            return;

        Call_Back();

        if (IPXManager.Get_Receive_Count() <= 0)
            return;
    }
}

}  // namespace

// ============================================================================
// asm: Negotiate_Scenario_Transfer (0x795CA0)
// ============================================================================
int32 Negotiate_Scenario_Transfer(int32 clFlag)
{
    // asm: test cl,cl / setz var_216 —— clFlag == 0 时 var_216 = 1
    bool v216 = (clFlag == 0);
    bool v215 = false;                       // var_215
    int32 v214 = 0;                          // var_214

    // 计时器槽位（var_200 / var_204 / var_1FC 与 var_20C / var_210 / var_208）
    int32 tStartA = 0;
    int32 tTickA = 0;
    int32 tTimeoutA = 0;
    int32 tStartB = 0;
    int32 tTickB = 0;
    int32 tTimeoutB = 0;

    uint8 packet[k_PacketSize];              // asm: var_1C8

    // asm: sub_543080 —— IPX Manager 初始化（停止收发）
    IPXManager.Stop_Listening();

    // asm: sub_540C60(0x32, -1, 0x1388, 1)
    IPXManager.Set_Timing(0x32, -1, 0x1388, 1);

    // asm: sub_7931A0 —— 取第一个活动连接的名字，与本地安全玩家名比对
    const char* const pConnName = IPXManager.First_Active_Name();
    const char* const pSecureName =
        (g_SecurePlayerName != nullptr) ? g_SecurePlayerName : "";

    const bool isOwner =
        (pConnName != nullptr && Ascii_Icmp_Equal(pConnName, pSecureName));

    if (isOwner) {
        // ====================================================================
        // Path A：本机是房主（asm loc_795D.. 起）
        // ====================================================================
        int32 votes[8] = {0, 0, 0, 0, 0, 0, 0, 0};   // asm: var_1E4 起
        std::memset(packet, 0, k_PacketSize);        // rep 71h stosd + stosw + stosb

        std::printf("About to wait for 'GO' response.\n");   // asm: WWDebugString

        int32 readyCount = 0;                    // asm: ebp
        bool  gotRequest = false;                // asm: bl

        // asm: loc_795DBC 之前的计时器装填（900 ms；var_216 时 300 ms）
        tTickA = Tick(&tStartA);
        tTimeoutA = v216 ? k_ShortTimeout : k_LongTimeout;

        // asm: loc_795DBC —— 等其余玩家投票
        for (;;) {
            Call_Back();

            if (IPXManager.Receive(&g_PacketType, &g_PacketUnknown,
                                   &g_PacketData, &g_PacketWord) != 0 &&
                g_PacketWord == static_cast<int16>(NET_YR_PACKET_WORD)) {
                std::printf("Packet is YR packet\n");

                const int32 nodeCount = NameNodeTypes_Length();
                if (nodeCount > 1) {
                    // asm: loc_795E17 —— 从 1 号玩家开始逐位核对
                    for (int32 edi = 1; edi < nodeCount && edi <= 8; ++edi) {
                        // asm: sub_53EF30(node+28h, &dword_A8D804)
                        if (!IPXManager.Match_Node_Address(edi, g_PacketData))
                            continue;

                        // asm: MP_ReadNthPlayerName(NameNodeTypes.data[edi])
                        const char* const pNodeName =
                            MP::ReadNthPlayerName(MP::PlayerNames[edi]);
                        // asm: __strcmpi(name, byte_A8D63C)
                        if (!Ascii_Icmp_Equal(pNodeName, IPXManager.PacketName))
                            continue;

                        int32& vote = votes[edi - 1];
                        if (vote != 0)
                            continue;

                        const int32 type = g_PacketType;
                        if (type == NET_REQ_SCENARIO) {
                            std::printf("Packet type is NET_REQ_SCENARIO\n");
                            vote = type;
                            gotRequest = true;
                            ++readyCount;
                        }
                        if (type == NET_READY_TO_GO) {
                            std::printf("Packet type is NET_READY_TO_GO\n");
                            vote = type;
                            ++readyCount;
                        }
                        if (type == NET_DONT_GO) {
                            std::printf("Packet type is NET_DONT_GO\n");
                            ++readyCount;
                            vote = type;
                            v216 = true;
                            if (nodeCount > edi &&
                                g_ScenarioTransfer.VoteB[edi - 1] <
                                    Vote_Limit(edi - 1)) {
                                g_ScenarioTransfer.VoteB[edi - 1] = 0;
                            }
                        }
                        if (type == NET_NO_SCENARIO) {
                            std::printf("Packet type is NET_NO_SCENARIO\n");
                            vote = type;
                            v214 = k_DontGoState;
                            v216 = true;
                            v215 = true;
                            ++readyCount;
                        }
                    }
                }
            }

            // asm: loc_795F29 —— 全员到齐即可结束等待
            if (readyCount >= NameNodeTypes_Length() - 1)
                break;

            int32 remain = tTimeoutA;
            if (tTickA != -1) {
                const int32 elapsed = Tick(&tStartA) - tTickA;
                if (elapsed >= tTimeoutA)
                    break;                       // asm: loc_795F5C
                remain = tTimeoutA - elapsed;
            }
            if (remain == 0)
                break;                           // asm: loc_795F5C
        }

        // asm: loc_795F5C —— 统计结果
        if (v215)
            v214 = k_DontGoState;

        if (!v216 && !v215 && gotRequest &&
            readyCount == NameNodeTypes_Length() - 1) {
            // asm: 等待系统空闲（sub_7757E0(0,0) 循环）
            if (Wait_For_Idle() == 1) {
                while (Wait_For_Idle() == 1) {
                }
            }

            // asm: loc_795FB2 —— 清空需求表并收集 NET_REQ_SCENARIO 的玩家
            g_ScenarioTransfer.RequestUnknown = 0;
            g_ScenarioTransfer.RequestCount = 0;
            std::memset(g_ScenarioTransfer.RequestPlayers, 0,
                        sizeof(g_ScenarioTransfer.RequestPlayers));

            int32 index = 1;
            for (; index < NameNodeTypes_Length() && index <= 8; ++index) {
                if (votes[index - 1] == NET_REQ_SCENARIO) {
                    g_ScenarioTransfer.RequestPlayers[
                        g_ScenarioTransfer.RequestCount] =
                        static_cast<uint8>(index);
                    ++g_ScenarioTransfer.RequestCount;
                }
            }

            // asm: loc_796002 —— 发剧本（必要时再发随机地图）
            ScenarioClass* const pScenario = ScenarioClass::Instance;
            const char* const pScenName =
                (pScenario != nullptr) ? pScenario->MapFileName : "";
            if (Send_File_To_Clients(pScenName, 4, 0, 1) == 0) {
                v214 = k_DontGoState;
                v216 = false;                    // asm: mov [var_216], al
            }
            if (Game::IsRandomMap() != 0) {
                if (Send_File_To_Clients("RandMap.img", 4, 0, 1) == 0) {
                    v214 = k_DontGoState;
                    v216 = false;
                }
            }
        }

        // asm: loc_796054 —— 组装 GO / DONT_GO 广播包
        std::memset(packet, 0, k_PacketSize);
        if (!v216 && readyCount == NameNodeTypes_Length() - 1 && !v215) {
            *reinterpret_cast<int32*>(packet) = NET_GO;
        } else {
            *reinterpret_cast<int32*>(packet) = NET_DONT_GO;
            v216 = true;
            if (v214 == 0)
                v214 = 1;
        }

        // asm: loc_7960A6 —— 目标设为 0 号节点（sub_735090 的宽转窄拷贝，
        // 原版写完就不再读取，这里保留同一动作）
        {
            char nodeName[0x100];
            Copy_Wide_To_Narrow(MP::PlayerNames[0], nodeName, 0xFF);
        }

        // asm: loc_7960D3 —— 逐个玩家发送
        for (int32 edi = 1; edi < NameNodeTypes_Length() && edi <= 8; ++edi) {
            tTickB = Tick(&tStartB);
            tTimeoutB = k_SendTimeout;
            if (v216) {
                tTickB = Tick(&tStartB);
                tTimeoutB = k_ShortTimeout;
            }

            // asm: loc_796112 —— 重试直到成功或超时
            for (;;) {
                Call_Back();

                if (IPXManager.Send(packet, k_SendSocket, 1,
                                    MP::PlayerNames[edi],
                                    static_cast<int32>(sizeof(packet))) != 0)
                    break;                       // asm: loc_79615B

                int32 remain = tTimeoutB;
                if (tTickB != -1) {
                    const int32 elapsed = Tick(&tStartB) - tTickB;
                    if (elapsed >= tTimeoutB)
                        break;                   // asm: loc_79615B
                    remain = tTimeoutB - elapsed;
                }
                if (remain == 0)
                    break;
            }
        }
    } else {
        // ====================================================================
        // Path B：加入方 / 无匹配连接（asm loc_79616E 起）
        // ====================================================================
        const int32 headerOk = Read_Scenario_Header(
            g_ScenarioTransfer.TransferName, g_ScenarioTransfer.FileSize,
            &g_ScenarioTransfer.HeaderFlag, g_ScenarioTransfer.HasScenario, 1);

        const int32 ebx = 2;

        if (headerOk != 0) {
            // asm: loc_7961C1 —— 本地已有剧本，广播 READY_TO_GO / DONT_GO
            std::memset(packet, 0, k_PacketSize);
            *reinterpret_cast<int32*>(packet) =
                v216 ? NET_DONT_GO : NET_READY_TO_GO;

            {
                char nodeName[0x100];
                Copy_Wide_To_Narrow(MP::PlayerNames[0], nodeName, 0xFF);
            }

            // asm: loc_7961E6 —— 发给第一个活动连接
            Send_To_First_Active(packet, static_cast<int32>(sizeof(packet)));

            // asm: loc_796236 —— 等队列排空
            Wait_Send_Queue_Drain(v216);
        } else {
            // asm: loc_7962E1 —— 头读取失败：拒绝，或向房主要剧本
            const int32 connType = g_ConnectionType;
            const bool refuse =
                (connType == 1 || connType == ebx) ||
                (g_ScenarioTransfer.HasScenario != 0 &&
                 Game::IsRandomMap() == 0);

            if (!refuse) {
                // asm: loc_79641A —— 拉取剧本（必要时再拉随机地图）
                if (Wait_For_Idle() == 1) {
                    while (Wait_For_Idle() == 1) {
                    }
                }
                Transfer_Reset_Progress();

                if (Get_File_From_Host(
                        g_ScenarioTransfer.TransferName,
                        static_cast<int32>(sizeof(g_ScenarioTransfer.TransferName)),
                        1) == 0) {
                    v214 = ebx;                  // asm: loc_796478
                    v216 = true;
                } else if (!v216 && Game::IsRandomMap() != 0) {
                    if (Get_File_From_Host(&tStartA, 4, 1) == 0) {
                        v214 = ebx;
                        v216 = true;
                    }
                }
            } else {
                // asm: loc_796313 —— 广播 NET_NO_SCENARIO
                std::memset(packet, 0, k_PacketSize);
                *reinterpret_cast<int32*>(packet) = NET_NO_SCENARIO;

                Send_To_First_Active(packet,
                                     static_cast<int32>(sizeof(packet)));

                std::printf("Sent NET_NO_SCENARIO\n");

                // asm: loc_79638D —— 等队列排空（1200 ms）
                Wait_Send_Queue_Drain(false);

                v214 = ebx;                      // asm: loc_796478
                v216 = true;
            }
        }

        // asm: loc_796481 —— 装好剧本名，等房主的 GO / DONT_GO
        {
            ScenarioClass* const pScenario = ScenarioClass::Instance;
            if (pScenario != nullptr) {
                // asm: repne scasb + rep movsd/movsb —— 整串拷贝
                std::strncpy(pScenario->MapFileName,
                             g_ScenarioTransfer.TransferName,
                             sizeof(pScenario->MapFileName) - 1);
                pScenario->MapFileName[sizeof(pScenario->MapFileName) - 1] =
                    '\0';
            }
        }

        std::printf("About to wait for 'GO' from host.\n");

        // asm: 计时器装填（默认 1200 ms，v216 时 300 ms）
        tTickA = Tick(&tStartA);
        tTimeoutA = v216 ? k_ShortTimeout : k_QueueTimeout;

        // asm: loc_79651A —— 等房主的 NET_GO / NET_DONT_GO
        for (;;) {
            Call_Back();

            bool handled = false;

            if (IPXManager.Receive(&g_PacketType, &g_PacketUnknown,
                                   &g_PacketData, &g_PacketWord) != 0 &&
                g_PacketWord == static_cast<int16>(NET_YR_PACKET_WORD)) {
                std::printf("Packet is YR packet\n");

                const int32 count = Connection_Count();
                int32 type = 0;
                bool matched = false;

                // asm: loc_796569 —— 找到与包内名字一致的连接项
                for (int32 i = 0; i < count; ++i) {
                    ConnectionClass* pConn = Connection_At(i);
                    if (pConn == nullptr || !pConn->Connected)
                        continue;
                    if (Ascii_Icmp_Equal(IPXManager.PacketName,
                                         pConn->PeerName)) {
                        matched = true;
                        type = g_PacketType;
                        break;
                    }
                }

                if (matched) {
                    if (type == NET_GO) {
                        std::printf("Got NET_GO from host\n");
                        goto finish;             // asm: loc_79669C
                    }
                    if (type == NET_DONT_GO) {
                        std::printf("Got NET_DONT_GO from host\n");
                        v216 = true;

                        // asm: loc_796620 —— 按名字找回节点下标
                        const int32 nodeCount = NameNodeTypes_Length();
                        int32 esi = 0;
                        for (; esi < nodeCount && esi < 8; ++esi) {
                            const char* const pNodeName =
                                MP::ReadNthPlayerName(MP::PlayerNames[esi]);
                            if (Ascii_Icmp_Equal(pNodeName,
                                                 IPXManager.PacketName))
                                break;
                        }
                        if (esi < nodeCount &&
                            g_ScenarioTransfer.VoteA[esi] < Vote_Limit(esi)) {
                            g_ScenarioTransfer.VoteA[esi] = 3;
                        }
                        handled = true;          // asm: loc_796673
                    }
                }
            }

            // asm: loc_7965C0 —— 第一次超时判定；仍有时间就继续等
            if (!handled) {
                int32 remain = tTimeoutA;
                if (tTickA != -1) {
                    const int32 elapsed = Tick(&tStartA) - tTickA;
                    remain = (elapsed >= tTimeoutA) ? 0
                                                    : (tTimeoutA - elapsed);
                }
                if (remain != 0)
                    continue;                    // asm: loc_79651A
            }

            // asm: loc_796673 —— 第二次判定决定是否置放弃标志
            {
                int32 remain = tTimeoutA;
                if (tTickA != -1) {
                    const int32 elapsed = Tick(&tStartA) - tTickA;
                    remain = (elapsed >= tTimeoutA) ? 0
                                                    : (tTimeoutA - elapsed);
                }
                if (remain != 0)
                    break;                       // asm: loc_79669C（保持标志）
                v216 = true;                     // asm: loc_796697
                break;
            }
        }
    }

finish:
    // asm: loc_79669C —— 重新装填 IPX Manager 的节拍参数
    {
        int32 queueLen = IPXManager.Get_Send_Queue_Length() * 8;
        if (queueLen < k_MinQueueLen) {
            queueLen = k_MinQueueLen;
        } else {
            queueLen = IPXManager.Get_Send_Queue_Length() * 8;   // asm: 再取一次
        }

        // asm: 栈上先压 (1, queueLen, -1)，取队列长度 +2 作为第一个参数
        const int32 retryDelta = IPXManager.Get_Send_Queue_Length() + 2;
        IPXManager.Set_Timing(retryDelta, -1, queueLen, 1);

        IPXManager.Start_Listening();            // asm: sub_543090
    }

    if (v214 == k_DontGoState)
        return k_DontGoState;
    if (v216)
        return 1;
    return v214;
}
