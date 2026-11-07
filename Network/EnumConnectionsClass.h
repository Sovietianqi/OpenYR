#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// EnumConnectionsClass - DirectPlay session enumeration callback state
//
// asm procs (both are stdcall COM callbacks, DATA XREF in .rdata):
//   EnumConnectionsClass_ResetCounter (0x4F8CA0): [this+1Ch] = 0
//   EnumConnectionsClass_AddToCounter (0x4F8CB0): [this+1Ch] += a2
// ============================================================================
class EnumConnectionsClass {
public:
    EnumConnectionsClass() noexcept : Counter(0) {}

    // asm: EnumConnectionsClass_ResetCounter
    //   mov eax, [esp+this] / mov dword ptr [eax+1Ch], 0 / xor eax,eax / retn 4
    static int32 ResetCounter(EnumConnectionsClass* pThis)
    {
        if (pThis != nullptr)
            pThis->Counter = 0;
        return 0;                                // asm: xor eax,eax
    }

    // asm: EnumConnectionsClass_AddToCounter
    //   edx = [eax+1Ch] ; edx += ecx ; [eax+1Ch] = edx ; xor eax,eax / retn 8
    static int32 AddToCounter(EnumConnectionsClass* pThis, int32 amount)
    {
        if (pThis != nullptr)
            pThis->Counter += amount;
        return 0;
    }

    int32 Counter;   // asm +0x1C
};
