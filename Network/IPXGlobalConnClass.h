#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "ConnectionClass.h"

// ============================================================================
// IPXGlobalConnClass - IPX 广播/全局包通道
//
//   asm: IPXGlobalConnClass_CTOR (0x53FB10 一带，retn 14h)
//        IPXGlobalConnClass_StripGlobalPacket (0x540110, retn 4)
//
//   在 IPXConnClass 之上再加一层「全局包」管理：一条按序号的接收队列，
//   外加一张与队列一一对应的 ack 表（每格 4 字节，存发送者 id，-1 表示
//   空闲）。发送方把广播包按序号排队，接收方按年龄清理过期包并把对应
//   的 ack 槽位腾空。
//
//   构造函数关键动作（ecx = this）：
//     1. 调 IPXConnClass_CTOR 铺好基类（参数见 asm arg_0..arg_10）
//     2. 换 vftable 为 IPXGlobalConnClass::vftable
//     3. [+0B8h] = arg_C（16 位）
//     4. [+0C4h] = 0
//     5. [+0C8h] = 1                     （byte）
//     6. [+0D4h] = arg_4                 （队列容量，即「连接数」）
//     7. [+0D0h] = new(arg_4 * 4)        ack 表
//     8. [+0CCh] = new(arg_4 * 0Ch)      队列体；每格 0x0C 字节，
//        逐格调 sub_53ECB0 做零初始化
//     9. ack 表逐格写 -1
//    10. [+0D8h] = 0
//
//   ⚠ 留白：asm 里 IPXConnClass 的基类布局（[+4Ch] 的地址结构、[+68h] 的名称缓冲
//   等）OpenYR 侧的 ConnectionClass 尚未按字节对齐，因此下面的偏移字段以
//   具名成员的形式记录。StripGlobalPacket 用到的接收队列（原版挂在连接对象
//   +0x38 上）改由本类的 ReceiveQueue 持有，其余逻辑逐句对齐 asm。
// ============================================================================
class IPXGlobalConnClass : public IPXConnectionClass {
public:
    // asm: IPXGlobalConnClass_CTOR
    IPXGlobalConnClass();
    virtual ~IPXGlobalConnClass();

    // ------------------------------------------------------------------------
    // asm: IPXGlobalConnClass_StripGlobalPacket (0x540110, retn 4)
    //
    //   清理接收队列里「年龄超过 maxAge 秒」的全局包。asm 逐段：
    //     num = [conn+38h]（接收数）
    //     WWDebugString("Stripping queue entries older than %d seconds. "
    //                   "Num receive = %d", maxAge / 60, num)
    //     for i in 0..num-1:
    //       pkt = sub_48B9E0(i) ; if (!pkt) continue
    //       now = dword_887340 + (tick - dword_887338)     (887338 != -1 时)
    //       if (pkt->time + maxAge >= now) continue        （还新鲜，保留）
    //       WWDebugString("Stripping global packet due to age of %d seconds",
    //                     (now - pkt->time) / 60)
    //       把发送者 id 对应的 ack 槽位写成 -1
    //       sub_48B890(..., i, ...)  移除该包
    //   本函数是虚表项，被 Networking 层按虚派发调用。
    // ------------------------------------------------------------------------
    virtual void StripGlobalPacket(int32 maxAgeSeconds);

    // ------------------------------------------------------------------
    // 全局包接收队列。
    //
    //   原版这条队列挂在「连接对象」上（[this+4] -> +0x38 计数，
    //   sub_48B9E0 取第 i 个包、sub_48B890 删除第 i 个包），包体是
    //   { +04 接收时刻, +0C -> +04 发送者 id }。本工程 ConnectionClass
    //   没有这层结构，队列改由本类持有，字段含义与 asm 一一对应。
    // ------------------------------------------------------------------
    struct GlobalPacket
    {
        int32 Time;      // 包 +0x04 —— 接收时刻（tick）
        int32 Sender;    // 包 +0x0C -> +0x04 —— 发送者 id
    };
    VectorClass<GlobalPacket> ReceiveQueue;   // asm: [conn+0x38]

    // ------------------------------------------------------------------
    // 布局（偏移取自 asm 构造函数）
    // ------------------------------------------------------------------
    uint16  Field_B8;        // +0x0B8  asm arg_C（16 位）
    uint8   _padBA[0x0A];    // +0x0BA
    int32   Field_C4;        // +0x0C4  构造时清 0
    bool    Field_C8;        // +0x0C8  构造时置 1
    uint8   _padC9[0x03];    // +0x0C9
    int32*  AckTable;        // +0x0CC  new(队列容量 * 0Ch)，每格 0x0C 字节
    int32*  AckSlots;        // +0x0D0  new(队列容量 * 4)，每格一个发送者 id
    int32   QueueCount;      // +0x0D4  队列容量（构造参数 arg_4）
    int32   Field_D8;        // +0x0D8  构造时清 0
};
