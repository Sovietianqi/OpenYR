#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// Networking/ScenarioTransfer.h —— 联机开局前的剧本/会话参数交换
//
// asm 权威：Negotiate_Scenario_Transfer (0x795CA0, __usercall: cl = 是否房主)
//
//   房主（cl == 0）先等其余玩家对剧本投票（NET_REQ_SCENARIO /
//   NET_READY_TO_GO / NET_DONT_GO / NET_NO_SCENARIO），然后广播
//   NET_GO / NET_DONT_GO 并把剧本文件发给要剧本的玩家；加入方（cl != 0）
//   读本地剧本头，向房主回 NET_READY_TO_GO 或直接索要剧本
//   （NET_NO_SCENARIO），最后等房主的 NET_GO / NET_DONT_GO。
//
// 本文件只做"状态与入口"的归属；协议细节见 .cpp 的逐段注释。
// ============================================================================

// asm: .data 里的包类型码
enum : int32 {
    NET_GO           = 0x0B,
    NET_REQ_SCENARIO = 0x10,
    NET_READY_TO_GO  = 0x13,
    NET_NO_SCENARIO  = 0x14,
    NET_DONT_GO      = 0x2A
};

// asm: word_A8D810 —— YR 包标记
const int32 NET_YR_PACKET_WORD = 0xAA04;

// asm: 交换期间用到的全局状态块
//   byte_A8B8E0     待交换/已收到的剧本文件名缓冲
//   dword_A8BB04    剧本文件长度
//   dword_A8BB08    "本地已有剧本"标志
//   byte_A8BAE2     剧本头标志字节
//   dword_A8BB10    需要剧本的玩家编号字节表
//   dword_A8BB14/18 该表的保留字段/条目数
//   dword_B73BB4/B8 每位玩家的投票状态（-1 = 未投）
struct ScenarioTransferState {
    char   TransferName[0x100];   // byte_A8B8E0
    int32  FileSize;              // dword_A8BB04
    int32  HasScenario;           // dword_A8BB08
    int32  HeaderFlag;            // byte_A8BAE2
    uint8  RequestPlayers[8];     // dword_A8BB10
    int32  RequestUnknown;        // dword_A8BB14
    int32  RequestCount;          // dword_A8BB18
    int32  VoteA[8];              // dword_B73BB4
    int32  VoteB[8];              // dword_B73BB8
};

extern ScenarioTransferState g_ScenarioTransfer;

// asm: dword_B73E40 —— 本地安全玩家名（charPP_GetSecure 取 *pp，空则空串）
extern const char* g_SecurePlayerName;

// asm: dword_A8B244 —— 当前连接形态（1 拨号 / 2 串口 / 3 IPX），
// 由会话建立方写入；Negotiate 用它决定"拒绝交换"还是"向房主要剧本"。
extern int32 g_ConnectionType;

// asm: Negotiate_Scenario_Transfer (0x795CA0)
//   clFlag == 0 表示本机是房主（setz 写 var_216 的反面基准），返回值
//   1 = 继续 / 2 = 放弃 / 其它 = 原样透传状态。
int32 Negotiate_Scenario_Transfer(int32 clFlag);
