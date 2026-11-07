#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "ConnectionClass.h"

// ============================================================================
// Network/IPXManagerClass.h —— 联机 IPX 管理器
//
// asm 权威（0x5422D0 / 0x5424C0 / 0x540C60 / 0x541260 / 0x541370 /
// 0x543080 / 0x543090，全部以全局对象 `IPXManager` 为 this）：
//
//   [+0x04]  Is_Open     收/发前的第一道开关
//   [+0x05]  Is_Service  收/发前的第二道开关
//   [+0x44]  连接数       Set_Timing 逐个刷新
//   [+0x48]  传输对象     vtable +2Ch = 发包，+34h = 收包
//   [+0x50]  单连接对象   Set_Timing 单独刷新
//   [+0x58]  RetryDelta   Set_Timing 第一参
//   [+0x5C]  RetryTimeout Set_Timing 第二参
//   [+0x60]  MaxAhead     Set_Timing 第三参
//   [+0xA0]  发送溢出计数 发包失败时自增并打印 "Send overflow %d\n"
//
// 本树的传输层是 Network/ConnectionClass.h 的 IPXConnectionClass（Winsock
// 之上的 IPX/SPX 兼容实现），故 asm 里 [this+0x48]->vtable[+2Ch/+34h] 的
// 虚派发直接落成 Connection->SendPacket / ReceivePacket 两个真实方法。
// ============================================================================

// asm: NameNodeTypes 的元素。+0 是宽字符玩家名（MP_ReadNthPlayerName 的
// 入参），+0x28 是 12 字节 IPX 地址（网络/节点/套接字）。
struct NodeNameType {
    wchar_t Name[20];      // +0x00
    uint8_t Address[12];   // +0x28
};

class IPXManagerClass
{
public:
    // asm: 全局对象 IPXManager
    static IPXManagerClass* Instance;

    IPXManagerClass();
    ~IPXManagerClass();

    // asm: sub_543080 —— [conn+0C8h] = 0（停止收发）
    void Stop_Listening();
    // asm: sub_543090 —— [conn+0C8h] = 1（开始收发）
    void Start_Listening();

    // asm: sub_540C60 —— 设置重试参数并在 apply 时下发到传输与各连接
    void Set_Timing(int32 retryDelta, int32 timeout, int32 maxAhead, int32 apply);

    // asm: sub_541260 —— 按节点名寻址后发包；成功返回传输层的返回值
    int32 Send(const void* pPacket, int32 socket, int32 flags,
               const wchar_t* pNodeName, int32 length = 0x1C7);

    // asm: sub_541370 —— 收包，四个出参对应 dword_A8D638 / unk_A8D800 /
    //                    dword_A8D804 / word_A8D810
    int32 Receive(int32* pType, int32* pUnknown, void** ppData, int16* pWord);

    // asm: sub_5424C0 —— 发送队列长度
    int32 Get_Send_Queue_Length();
    // asm: sub_5422D0 —— 接收队列长度
    int32 Get_Receive_Count();

    // asm: sub_53EF30 —— 节点地址匹配（比较 +4 起的 4/6 字节地址）
    bool Match_Node_Address(int32 nodeIndex, const void* pAddress);

    // asm: sub_7931A0 —— 第一个"活动"连接项的名字（+0x24），无则返回 nullptr
    const char* First_Active_Name();

public:
    IPXConnectionClass* Connection;   // +0x48
    bool  Is_Open;                    // +0x04
    bool  Is_Service;                 // +0x05
    int32 ConnectionCount;            // +0x44
    int32 RetryDelta;                 // +0x58
    int32 RetryTimeout;               // +0x5C
    int32 MaxAhead;                   // +0x60
    int32 SendOverflow;               // +0xA0
    int32 AddressLength;              // asm: dword_828140 —— 4 或 6

    // asm: dword_A8D804 指向的发送方地址块 / byte_A8D63C 的包内玩家名
    uint8 PacketAddress[12];
    char  PacketName[0x40];

    // 最近一次收包的包体（文件传输按块取用）
    uint8 PacketPayload[0x1C7];
    int32 PacketLength;
};

// asm: IPXManager —— .data 里的全局实例
extern IPXManagerClass IPXManager;
