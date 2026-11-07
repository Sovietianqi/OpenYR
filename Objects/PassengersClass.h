#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class TechnoClass;

// ============================================================================
// PassengersClass - passenger chain wrapper (asm sub-object at techno+...)
//
// The original keeps a pointer to the first passenger at [this+4]; the chain
// runs through the object's Next slot (+0x30) and each hop validates the
// candidate's low type flags byte (+0x14) for bit 2 ("can carry/be carried").
// asm: Passengers_IndexOf (0x4734F0) returns the 1-based index of the
// passenger in the chain, or 0 when absent.
// ============================================================================
class PassengersClass {
public:
    PassengersClass() noexcept : Capacity(0), First(nullptr) {}

    // asm: Passengers_IndexOf (0x4734F0)
    //   ecx = [this+4]            ; chain head
    //   xor eax, eax              ; index = 0
    //   test ecx, ecx / jz ret0
    // loop (loc_47350E):
    //   inc eax                   ; 1-based ordinal
    //   cmp ecx, passenger / jz found
    //   edx = [ecx+30h]           ; Next
    //   test edx, edx / jz endcheck
    //   test byte [edx+14h], 4 / jz ret0    ; next hop not carryable
    //   ecx = edx
    // endcheck:
    //   test ecx, ecx / jnz loop
    // found:
    //   test ecx, ecx / jnz ret_idx / ret0
    int32 IndexOf(TechnoClass* passenger) const;

    int32          Capacity;   // asm +0x00: max carried units
    TechnoClass*   First;      // asm +0x04: passenger chain head
};
