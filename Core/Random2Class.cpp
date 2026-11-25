#include "Random2Class.h"

// ============================================================================
// Random2Class.cpp - 250-slot lagged XOR generator
//
//   序列完全由种子决定：Random_Init 用固定常数表 + 种子把 250 个槽填满，
//   之后每次取值把两个槽异或回写。没有时间、没有指针、没有 std::rand。
// ============================================================================

// The non-critical generator is a plain global object, mirroring the
// binary's "NonCriticalRandomNumber" instance at 0x87F2A0.
Random2Class NonCriticalRandomNumber;

namespace {

// asm: dword_839644[0..4]（.data:0x839644，16 项，Random_Init 只用前 5 项）
const uint32 kSeedTable[5] = {
    0xBAA96887u, 0x1E17D32Cu, 0x0BCDC3Cu, 0xF33D1B2u, 0x76A6491Du
};

// asm: dword_839690[1..5]（Random_Init 先 add ebx,4 再取表，故从下标 1 起）
const uint32 kMixTable[6] = {
    0x48AAD7E4u, 0x4B0F3B58u, 0xE874F0C3u, 0x6955C5A6u, 0x55A7CA46u, 0x4D9A9D86u
};

// asm: sar edx, 16 —— 算术右移（GCC/Clang 对 int32 的 >> 即算术右移）。
inline int32 Asr16(int32 v)
{
    return static_cast<int32>(static_cast<uint32>(v) >> 16)
         | (v < 0 ? static_cast<int32>(0xFFFF0000u) : 0);
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Random2Class::Init
//
//   asm Random_Init @0x65C6D0，逐条对应。全程按 32 位无符号做乘法，低半部
//   与 x86 的 imul 一致，也避免有符号溢出的未定义行为。
// ----------------------------------------------------------------------------
void Random2Class::Init(int32 seed)
{
    IndexA = 0;
    IndexB = 0x67;                 // asm: mov dword ptr [ecx+8], 67h

    const uint32 useed = static_cast<uint32>(seed);

    for (int32 i = 0; i < RANDOM2_TABLE_SIZE; ++i)
    {
        uint32 esi = static_cast<uint32>(i);
        uint32 ebp = useed;

        for (int32 j = 0; j < 5; ++j)
        {
            const uint32 eax = esi;
            esi ^= kSeedTable[j];                       // asm: xor edx, esi / mov esi, edx

            const uint32 hi = static_cast<uint32>(Asr16(static_cast<int32>(esi)));
            const uint32 lo = esi & 0xFFFFu;

            uint32 v = ~(hi * hi);                      // asm: imul edx, edi / not edx
            v += lo * lo;                               // asm: add edx, ecx
            v = (static_cast<uint32>(Asr16(static_cast<int32>(v))) | (v << 16));
            v ^= kMixTable[j + 1];
            v += hi * lo;
            v ^= ebp;

            esi = v;
            ebp = eax;                                  // asm: mov ebp, eax
        }

        Table[i] = static_cast<int32>(esi);
    }

    Initialized = true;            // asm 写 0 表示"可用"，本工程用 true 表示已建表
}

// ----------------------------------------------------------------------------
// Random2Class::operator()
//
//   asm @0x65C770：v = Table[A] ^ Table[B]; Table[A] = v; return v;
//   游标在写完表**之后**才推进（返回值用的是推进前的 A 槽）。
// ----------------------------------------------------------------------------
int32 Random2Class::operator()()
{
    if (!Initialized) {
        Init(0);                   // asm: Random_Init(this+0x218, 0) —— 种子固定 0
    }

    const int32 a = IndexA;
    const int32 b = IndexB;

    const int32 v = Table[a] ^ Table[b];
    Table[a] = v;

    int32 na = a + 1;
    int32 nb = b + 1;
    if (na >= RANDOM2_TABLE_SIZE) na = 0;
    if (nb >= RANDOM2_TABLE_SIZE) nb = 0;

    IndexA = na;
    IndexB = nb;

    return v;
}

// ----------------------------------------------------------------------------
// Random2Class::operator()(min, max)
//
//   asm @0x65C7E0。要点：
//     * min == max → 直接返回；
//     * 参数倒序时交换；
//     * range = hi - lo，mask = 不小于 range 的 (2^k - 1)；
//     * 取值 v &= mask，v > range 则重抽（拒绝采样），**不是** v % (range+1)；
//     * 返回 v + lo。
// ----------------------------------------------------------------------------
int32 Random2Class::operator()(int32 min, int32 max)
{
    if (min == max) {
        return min;
    }

    int32 lo = min;
    int32 hi = max;
    if (hi < lo) {
        const int32 t = lo;
        lo = hi;
        hi = t;
    }

    const uint32 range = static_cast<uint32>(hi) - static_cast<uint32>(lo);

    // 找 range 的最高有效位（asm 用 ecx 从 31 往下试）。
    uint32 bit = 0;
    if (range == 0) {
        bit = 0;
    } else {
        uint32 probe = 0x80000000u;
        bit = 31;
        while (bit > 0 && (range & probe) == 0) {
            probe >>= 1;
            --bit;
        }
    }

    // asm: inc ecx / or ebp,-1 / shl ebp,cl / not ebp。
    // cl 到 32 时 x86 把移位量按 5 位截断（等价于不移位），not 之后掩码为 0。
    const uint32 maskBits = bit + 1;
    const uint32 mask = (maskBits >= 32) ? 0u
                                         : ((1u << maskBits) - 1u);

    int32 v = 0;
    do {
        v = (*this)() & static_cast<int32>(mask);
    } while (v > static_cast<int32>(range));

    return lo + v;
}
