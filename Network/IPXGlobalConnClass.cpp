#include "IPXGlobalConnClass.h"

#include <Core/Memory.h>

#include <cstdarg>
#include <cstdio>

// ============================================================================
// IPXGlobalConnClass - IPX 广播/全局包通道
// ============================================================================

namespace {

// 引擎的调试输出通道。原版把队列清理过程逐条走 WWDebugString 打出来
// （asm 0x7C5B10）；这里沿用本树其它子系统的做法，落到 stderr。
void WWDebugString(const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    std::vfprintf(stderr, pFormat, args);
    va_end(args);
}

// asm 的 `imul 88888889h / add edx,esi / sar edx,5 / shr-fix`：有符号
// 除以 60，向零取整。用 64 位中间量直接复刻。
inline int32 MagicDiv60Signed(int32 n)
{
    const int64_t prod =
        static_cast<int64_t>(n) * static_cast<int64_t>(0x88888889LL);
    int32 hi = static_cast<int32>(static_cast<uint64_t>(prod) >> 32);
    hi += n;                                            // asm: add edx,esi
    hi >>= 5;                                           // asm: sar edx,5
    hi += static_cast<int32>(static_cast<uint32_t>(hi) >> 31);
    return hi;
}

} // namespace

// ============================================================================
// 构造 / 析构 -- asm IPXGlobalConnClass_CTOR (0x53FB10 一带, retn 14h)
//
//   基类先铺好（IPXConnClass_CTOR），再按队列容量分配队列体与 ack 表。
//   ⚠ 留白：基类构造函数带 5 个参数（网络号/节点/端口等），OpenYR 侧的
//   ConnectionClass 尚未定案成同一套签名，因此这里先只做本类独有的部分；
//   基类参数待 ConnectionClass 对齐后一并接上。
// ============================================================================
IPXGlobalConnClass::IPXGlobalConnClass()
    : Field_B8(0)
    , Field_C4(0)
    , Field_C8(true)
    , AckTable(nullptr)
    , AckSlots(nullptr)
    , QueueCount(0)
    , Field_D8(0)
{
    // asm: mov [esi+0D4h],edi	 -- 队列容量来自构造参数 arg_4。
    // 该参数在 ConnectionClass 定案前不可得，暂留 0；下面的分配随之跳过，
    // 但字段已按 asm 的位置与初值就位（铁律①）。
    if (QueueCount > 0) {
        // asm: lea ecx,[edi*4] / push / call new / mov [esi+0D0h],eax
        AckSlots = static_cast<int32*>(
            YRMemory::Allocate(static_cast<size_t>(QueueCount) * 4u));

        // asm: lea edx,[edi+edi*2] / shl edx,2 / push / call new
        //      （即 QueueCount * 0Ch），逐格调 sub_53ECB0 做零初始化
        AckTable = static_cast<int32*>(
            YRMemory::Allocate(static_cast<size_t>(QueueCount) * 0x0Cu));

        // asm: 逐格写 -1
        for (int32 i = 0; i < QueueCount; ++i) {
            AckSlots[i] = -1;
        }
    }
}

IPXGlobalConnClass::~IPXGlobalConnClass()
{
    // asm 的析构在同一段里释放两张表；此处按「谁申请谁释放」的对应关系
    // 归还。为 0 时 YRMemory::Deallocate 会原样返回。
    if (AckTable != nullptr) {
        YRMemory::Deallocate(AckTable);
        AckTable = nullptr;
    }
    if (AckSlots != nullptr) {
        YRMemory::Deallocate(AckSlots);
        AckSlots = nullptr;
    }
}

// ============================================================================
// StripGlobalPacket -- asm IPXGlobalConnClass_StripGlobalPacket
//                       (0x540110, retn 4)
//
//   ⚠ 留白：asm 里这一层还要经 [+4] 取到「连接对象」，再读它的接收队列
//   （[conn+38h] 计数、sub_48B9E0 取包、sub_48B890 删包），并用到三个
//   计时全局量（dword_887338 / dword_887340 / unk_88733C）。这些在
//   OpenYR 侧的连接层尚未定案，因此本函数当前只保留函数签名与调用点，
//   算法在连接层对齐后再补齐——按铁律①，宁可留白不写错。
//
//   算法要点（供后续迁移）：按 maxAge（秒）遍历接收队列，把
//     pkt->time + maxAge < now
//   的包移除，并把 [+0D0h] ack 表里对应发送者 id 的槽位写成 -1。
// ============================================================================
void IPXGlobalConnClass::StripGlobalPacket(int32 maxAgeSeconds)
{
    // 留白：见上方说明。
    (void)maxAgeSeconds;
}
