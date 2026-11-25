#include "IPXGlobalConnClass.h"

#include <Core/Memory.h>

#include "../Game/Game.h"

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

// asm 的时间基准：dword_887338（基准 tick，-1 表示未设）与 dword_887340
// （累计毫秒）。Now() 复刻 asm 每次的 `mov eax,887338 / cmp eax,-1 /
// Get_System_Tick_Count / sub eax,887338 / add edi,eax`。
int32 s_TimeBaseTick = -1;      // asm: dword_887338
int32 s_TimeAccum = 0;          // asm: dword_887340

inline int32 Now()
{
    int32 now = s_TimeAccum;
    if (s_TimeBaseTick != -1)
        now += static_cast<int32>(Game::GetTickCount()) - s_TimeBaseTick;
    return now;
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
//   asm 逐句：
//     num = [conn+38h]（接收队列条数）
//     WWDebugString("Stripping queue entries older than %d seconds. "
//                   "Num receive = %d", maxAge/60, num)
//     for (i = 0 ; i < num ; ++i):
//       pkt = sub_48B9E0(i)                     ; 第 i 个包
//       WWDebugString("i = %d\n", i)
//       if (!pkt) continue
//       now = dword_887340 + (Tick - dword_887338)       ; 887338 == -1 时取 887340
//       if (pkt->time + maxAge >= now)          ; 还新鲜
//           打 "Leaving global packet due to age of %d seconds"
//           continue
//       打 "Stripping global packet due to age of %d seconds"
//       for (j = 1..[this+0D4h]):
//           if (AckSlots[j] == pkt->sender) AckSlots[j] = -1
//       sub_48B890(..., i, ...)                 ; 移除第 i 个包，i 不前进
//     WWDebugString("Done stripping. Num receive = %d\n", [conn+38h])
//
//   时间基准：原版 dword_887338（基准 tick，-1 表示未设）与 dword_887340
//   （累计毫秒）在这里用文件内静态量复刻，Tick 取 Game::GetTickCount()。
//   队列本身由本类的 ReceiveQueue 持有（原版挂在连接对象上，见头文件）。
// ============================================================================
void IPXGlobalConnClass::StripGlobalPacket(int32 maxAgeSeconds)
{
    // asm: mov eax,[esi+4] / mov ecx,[eax+38h]
    const int32 num = ReceiveQueue.GetCount();

    WWDebugString("Stripping queue entries older than %d seconds. "
                  "Num receive = %d\n",
                  MagicDiv60Signed(maxAgeSeconds), num);

    for (int32 i = 0; i < ReceiveQueue.GetCount(); )
    {
        // asm: push ebp / call sub_48B9E0
        const GlobalPacket& pkt = ReceiveQueue[i];
        WWDebugString("i = %d\n", i);

        // asm: mov eax,dword_887338 / mov esi,dword_887340 / cmp eax,-1
        const int32 now = Now();

        // asm: mov ecx,[edi+4] / add ecx,arg / cmp ecx,ebx / jge 保留
        if (pkt.Time + maxAgeSeconds >= now)
        {
            WWDebugString("Leaving global packet due to age of %d seconds\n",
                          MagicDiv60Signed(now - pkt.Time));
            ++i;
            continue;
        }

        WWDebugString("Stripping global packet due to age of %d seconds\n",
                      MagicDiv60Signed(now - pkt.Time));

        // asm: mov ecx,[edi+0Ch] / mov esi,[ecx+4]  —— 包里的发送者 id
        const int32 sender = pkt.Sender;

        // asm: mov ecx,[ebx+0D4h] / mov edi,[ebx+0D0h] / 线性找同值槽 -> -1
        if (AckSlots != nullptr)
        {
            for (int32 j = 0; j < QueueCount; ++j)
            {
                if (AckSlots[j] == sender)
                {
                    AckSlots[j] = -1;
                    break;
                }
            }
        }

        // asm: push 0 / push 0 / push ebp / push 0 / push 0 / call sub_48B890
        ReceiveQueue.Remove(i);
        // asm: dec ebp / jmp  —— 移除后下标不动，继续看同一个位置
    }

    // asm: mov ecx,[esi+4] / mov edx,[ecx+38h] / push edx
    WWDebugString("Done stripping. Num receive = %d\n",
                  ReceiveQueue.GetCount());
}
