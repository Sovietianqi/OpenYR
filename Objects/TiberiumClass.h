#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// TiberiumClass helpers - original layout mirrors
//
//  The original binary keeps the four loaded Tiberium types' "image frames
//  loaded" amounts as a contiguous float table.  Tiberium_FindFirstLoadedType
//  scans that table for the first entry != 0.0 and returns its index.
// ============================================================================

namespace Tiberium {

// ----------------------------------------------------------------------------
// asm: Tiberium_FindFirstLoadedType (0x6C9810) -- called from
// BuildingClass::Destroy when a silo/gem generator must pick a visible ore
// type.  ecx walks the table of 4 floats:
//
//   xor edx, edx
// loop_6C9822:
//   fld  [ecx]          ; amount
//   fcomp num_0_0       ; vs 0.0
//   fnstsw ax / test ah,41h / jz found   ; amount != 0.0 (ne or unordered
//                                        ; never both here) -> index in edx
//   inc edx / add ecx,4 / cmp edx,4 / jl loop
//   or eax,-1           ; none loaded
//   retn
// found:
//   mov eax, edx / retn
//
// NOTE the original test is `test ah,41h` (C3 / C0): 只有 "既不等也不小于"
// 才算命中，也就是 **amount > 0.0**；NaN（C3=C2=C0 全 1）不命中，负数不命中。
// ----------------------------------------------------------------------------
inline int32 Find_First_Loaded_Type(const float* pAmounts, int32 count = 4)
{
    for (int32 idx = 0; idx < count; ++idx) {       // asm: cmp edx,4 / jl
        if (pAmounts[idx] > 0.0f)                   // asm: fcomp num_0_0 / test ah,41h / jz found
            return idx;                             // asm: mov eax,edx
    }
    return -1;                                      // asm: or eax,0FFFFFFFFh
}

} // namespace Tiberium
