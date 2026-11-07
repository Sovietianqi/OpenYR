#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// StorageClass - tiberium / ore silo ledger (float amounts per type)
//
// asm: StorageClass_Get_Amount_From (0x4A2DE0)
//   eax = [esp+a2] ; fld dword ptr [ecx+eax*4] ; retn 4
// The ledger keeps one 32-bit float per ore/tiberium type; the getter loads
// the requested slot directly onto the FPU stack.
// ============================================================================
class StorageClass {
public:
    StorageClass() noexcept { for (int32 i = 0; i < 4; ++i) Amounts[i] = 0.0f; }

    // asm: StorageClass_Get_Amount_From -- returns Amounts[type] as a float.
    float Get_Amount_From(int32 type) const
    {
        if (type < 0 || type >= 4)
            return 0.0f;                  // asm: out-of-range reads are UB; guarded
        return Amounts[type];             // asm: fld dword ptr [ecx+eax*4]
    }

    float Amounts[4];    // asm +0x00: one float slot per tiberium type
};
