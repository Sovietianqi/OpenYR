#include "DiscreteDistributionClass.h"

// ============================================================================
// asm: DynamicVectorClass_DiscreteDistribution (0x6F27E0)
// ============================================================================
void* DiscreteDistributionClass::Init(int32 count, void* pPrealloc)
{
    // asm: [edi+4] = 0 / [edi+8] = esi / [edi+0Ch] = 1 / [edi+0Dh] = 0
    Data          = nullptr;
    Count         = count;
    IsInitialized = 1;
    IsAllocated   = 0;
    // asm: mov dword ptr [edi], offset VectorClass<...>::vftable
    VTable        = nullptr;

    // asm: test esi,esi / jz loc_6F288B
    if (count == 0)
        return this;                              // asm: mov eax,edi

    // 清零 count 个 8 字节条目（asm: loc_6F282C mov [eax],0 / add eax,8）。
    // asm 的循环从 [edx+4] 起（edx = data = block+4），即每个条目的
    // **payload** 字段（data+8k+4），weight 字段原版不清。
    auto zeroEntries = [](uint8* pData, int32 n)
    {
        uint8* pCur = pData;
        for (int32 i = 0; i < n; ++i)
        {
            *reinterpret_cast<int32*>(pCur) = 0;
            pCur += 8;
        }
    };

    if (pPrealloc != nullptr)
    {
        // asm: [eax] = esi（count 前缀）/ 清零 / [edi+4] = edx
        uint8* pBlock = static_cast<uint8*>(pPrealloc);
        *reinterpret_cast<int32*>(pBlock) = count;
        zeroEntries(pBlock + 8, count);
        Data = pBlock + 4;                        // asm: mov [edi+4],edx
        return this;
    }

    // asm: lea eax, ds:4[esi*8] / call new
    uint8* pBlock = static_cast<uint8*>(
        YRMemory::Allocate(static_cast<size_t>(count) * 8 + 4));

    if (pBlock == nullptr)
    {
        // asm: loc_6F2882: xor eax,eax / mov [edi+0Dh],1 / mov [edi+4],eax
        IsAllocated = 1;
        Data        = nullptr;
        return this;
    }

    // asm: [eax] = esi / 清零 / mov [edi+0Dh],1 / mov [edi+4],edx
    *reinterpret_cast<int32*>(pBlock) = count;
    zeroEntries(pBlock + 8, count);
    IsAllocated = 1;                              // asm: mov [edi+0Dh],1
    Data        = pBlock + 4;                     // asm: mov [edi+4],edx
    return this;
}

// ============================================================================
// 按权重择一：沿条目累加，命中 target 即返回该条目 payload。
// ============================================================================
void* DiscreteDistributionClass::Random_Pick(int32 target) const
{
    if (Data == nullptr || Count <= 0)
        return nullptr;

    const Entry* pEntries = Data_As_Entries();
    int32 acc = 0;

    for (int32 i = 0; i < Count; ++i)
    {
        acc += pEntries[i].Weight;
        if (target < acc)
            return pEntries[i].Payload;
    }

    // 权重全部为 0 的未初始化分布：回退到首项，避免越界。
    return pEntries[0].Payload;
}
