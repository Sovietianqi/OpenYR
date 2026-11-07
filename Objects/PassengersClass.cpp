#include "PassengersClass.h"
#include "../Abstract/TechnoClass.h"

// ============================================================================
// Passengers_IndexOf  (asm: Passengers_IndexOf, 0x4734F0)
//
// Walks the passenger chain (linked through ObjectClass::NextObject in this
// layout) counting positions from 1.  The +0x14 flag-byte bit-2 test in the
// original filters nodes whose abstract type does not participate in
// carrying; here the same bit of the runtime type id is checked.
// ============================================================================
int32 PassengersClass::IndexOf(TechnoClass* passenger) const
{
    const ObjectClass* pCur = First;                 // asm: mov ecx,[ecx+4]
    int32 index = 0;                                 // asm: xor eax,eax

    while (pCur != nullptr) {
        ++index;                                     // asm: inc eax
        if (static_cast<const void*>(pCur) == static_cast<const void*>(passenger))
            return index;                            // asm: jz loc_473528

        const ObjectClass* pNext = pCur->NextObject; // asm: mov edx,[ecx+30h]
        if (pNext == nullptr)
            break;                                   // asm: jz loc_473522

        // asm: test byte ptr [edx+14h], 4 ; jz ret0
        const uint8 typeLow = static_cast<uint8>(pNext->WhatAmI());
        if ((typeLow & 4) == 0)
            return 0;

        pCur = pNext;                                // asm: mov ecx,edx
    }

    return 0;                                        // asm: xor eax,eax
}
