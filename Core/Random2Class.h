#pragma once

#include "Definitions.h"

// ============================================================================
// Random2Class - the game's pseudo-random generator
//
//   asm: RandomizerClass struc (sizeof=0x3F4)
//      +0x00  byte  Initialized                （0 = 可用，见 Random_Init 结尾）
//      +0x04  int   IndexA
//      +0x08  int   IndexB
//      +0x0C  int   Table[250]
//
//   asm: Random_Init(RandomizerClass* this, int seed)  @0x65C6D0
//      IndexA = 0; IndexB = 0x67;
//      for (i = 0; i < 250; ++i) {
//          esi = i; ebp = seed;
//          for (j = 0; j < 5; ++j) {
//              eax  = esi;
//              esi ^= k_seedTable[j];                 // 先异或常数
//              hi   = (int32)esi >> 16;
//              lo   = esi & 0xFFFF;
//              v    = ~(hi * hi) + lo * lo;           // 32 位低半部
//              v    = (((int32)v >> 16) | (v << 16));
//              v   ^= k_mixTable[j + 1];
//              v   += hi * lo;
//              v   ^= ebp;
//              esi  = v;
//              ebp  = eax;
//          }
//          Table[i] = esi;
//      }
//      Initialized = 0;                              // 0 = 已建表、可取值
//
//   asm: Random2Class::operator()()  @0x65C770 —— 250 槽滞后异或：
//      v = Table[IndexA] ^ Table[IndexB]; Table[IndexA] = v; return v;
//      两个游标各自 +1，到 250 归零。
//
//   asm: Random2Class::operator()(min, max)  @0x65C7E0 —— 掩码 + 拒绝采样，
//      不是取模。
//
//   这两张常数表（k_seedTable / k_mixTable）是原版 .data 里的固定内容，
//   与种子无关；只要种子相同，整张表和整条序列就逐位相同。
// ============================================================================

class Random2Class
{
public:
    static constexpr int32 RANDOM2_TABLE_SIZE = 250;  // 0xFA

    bool  Initialized;                  // +0x00
    int32 IndexA;                       // +0x04
    int32 IndexB;                       // +0x08
    int32 Table[RANDOM2_TABLE_SIZE];    // +0x0C

    // ⚠ 不要在这里加成员。原版 RandomizerClass 的 sizeof 是 0x3F4
    //   （4 + 4 + 4 + 250*4），ScenarioClass 把实例放在 +0x218，后面的
    //   全部字段都按这个尺寸排。多一个 int32 就会让 ScenarioClass 整段
    //   错位（实测：加 8 字节后主视口地形全黑、nonblack 246392 → 57592）。

    Random2Class() noexcept
        : Initialized(false)
        , IndexA(0)
        , IndexB(0)
    {
        for (int32 i = 0; i < RANDOM2_TABLE_SIZE; ++i) {
            Table[i] = 0;
        }
    }

    // asm: Random_Init —— 按种子填满 250 个槽并复位游标。
    void Init(int32 seed);

    // 原始取值。表未建时先按种子 0 建表（asm: Random_Init(..., 0)）。
    int32 operator()();

    // 闭区间 [min, max] 取值（掩码 + 拒绝采样）。
    int32 operator()(int32 min, int32 max);
};

// The game keeps two independent generators: the synchronised one used by
// gameplay code (stored inside ScenarioClass at +0x218) and the so-called
// non-critical generator used by purely cosmetic code paths.
extern Random2Class NonCriticalRandomNumber;
