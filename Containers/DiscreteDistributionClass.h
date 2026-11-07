#pragma once

#include "../Core/Definitions.h"
#include "../Core/Memory.h"

// ============================================================================
// DiscreteDistributionClass
//
// asm 结构体（权威布局，来自 gamemd 的 struc 定义）：
//   struct VecDistributionObject {            sizeof = 0x18
//       void*  vt;                // +0x00
//       void*  data;              // +0x04   entries；[data-4] 存 count
//       int32  count;             // +0x08
//       uint8  IsInitialized;     // +0x0C
//       uint8  IsAllocated;       // +0x0D
//       uint8  field_E;           // +0x0E
//       uint8  field_F;           // +0x0F
//       int32  length;            // +0x10
//       int32  incrementAmount;   // +0x14
//   }
//
// 每个 entry 8 字节：{ int32 weight; void* payload; }，weight 初值为 0。
//
// asm: DynamicVectorClass_DiscreteDistribution (0x6F27E0)
// ============================================================================

class DiscreteDistributionClass
{
public:
    // asm: VectorClass<...>::vftable —— C++ 里由类型系统承担
    void* VTable;            // +0x00
    void* Data;              // +0x04  entries；[data-4] 存 count
    int32 Count;             // +0x08
    uint8 IsInitialized;     // +0x0C
    uint8 IsAllocated;       // +0x0D
    uint8 field_E;           // +0x0E
    uint8 field_F;           // +0x0F
    int32 Length;            // +0x10
    int32 IncrementAmount;   // +0x14

    // ------------------------------------------------------------------------
    // asm: DynamicVectorClass_DiscreteDistribution (0x6F27E0)
    //
    //   void* __fastcall DiscreteDistribution_Init(DiscreteDistribution *this,
    //                                              int32 count, void *pPrealloc)
    //
    //   构造一个 count 项的离散分布：
    //     [this+04] = data ; [this+08] = count ; [+0C] = 1（已初始化）
    //     [+0D] = 0（未分配）
    //     pPrealloc != 0 -> 直接用（count dword 前缀 + count*8 字节条目）
    //     pPrealloc == 0 -> new (count*8 + 4)；成功则 [+0D] = 1
    //                      失败时 data = 0 但 [+0D] 仍 = 1（原版如此，保留这个怪癖）
    //   返回 this（asm: mov eax,edi）。
    // ------------------------------------------------------------------------
    void* Init(int32 count, void* pPrealloc);

    // 条目访问（每条 8 字节：weight + payload）
    struct Entry
    {
        int32 Weight;        // +0x00
        void* Payload;       // +0x04
    };

    Entry* Data_As_Entries() const
    {
        return static_cast<Entry*>(Data);
    }

    // ------------------------------------------------------------------------
    // 按权重随机取一个条目：累计权重遇到 target 就返回。
    // 权重全为 0（未初始化）时回退到下标 0，与原版行为一致。
    // ------------------------------------------------------------------------
    void* Random_Pick(int32 target) const;
};
