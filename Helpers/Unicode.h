#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// Unicode helpers (asm: Unicode_AssignString, 0x7B66C0)
// ============================================================================
namespace Unicode {
    // asm: Unicode_AssignString
    //   esi = this (wchar_t** slot) ; edi = Source
    //   [esi] = 0 initially
    //   if (edi): len = wcslen(edi)+1 ; bytes = len*2 ; new ; wcscpy
    //   else:     bytes = 2          ; new ; word [new] = 0
    //   returns the slot address.
    wchar_t** AssignString(wchar_t** pSlot, const wchar_t* pSource);

    // asm: Unicode_AssignStringNumeric (0x7B7E00)
    //   this = ecx -> esi (a wide-string slot object whose only field is +0)
    //   edx        = the numeric value to render
    //
    //   wchar_t buf[32];  swprintf(buf, L"%d", edx)
    //   [esi] = 0
    //   len = wcslen(buf)+1
    //   mem = new BYTE[len*2]        (asm: add eax,eax ; call new)
    //   [esi] = (wchar_t*)mem ; mem[0] = 0
    //   wcscpy([esi], buf)
    //   return esi
    wchar_t** AssignStringNumeric(wchar_t** pSlot, int32 number);
}
