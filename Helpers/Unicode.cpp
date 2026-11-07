#include "Unicode.h"
#include <cstring>
#include <cwchar>

// ============================================================================
// Unicode_AssignString  (asm: Unicode_AssignString, 0x7B66C0)
//
//   test edi, edi / mov dword [esi], 0 / jz alloc_empty
//   push edi ; call _wcslen ; add esp,4 ; inc eax
//   jmp short widen            ; eax = chars + 1
// alloc_empty:
//   mov eax, 1
// widen:
//   add eax, eax               ; bytes = chars * 2
//   push eax ; call new
//   mov [esi], eax             ; slot = buffer
//   test edi, edi
//   mov word ptr [eax], 0      ; default empty
//   jz done
//   push edi ; push ecx ; call _wcscpy
// done:
//   mov eax, esi ; retn 4
// ============================================================================
wchar_t** Unicode::AssignString(wchar_t** pSlot, const wchar_t* pSource)
{
    if (pSlot == nullptr)
        return nullptr;

    *pSlot = nullptr;                                // asm: mov dword [esi],0

    std::size_t chars = (pSource != nullptr)
                      ? std::wcslen(pSource) + 1     // asm: call _wcslen ; inc eax
                      : 1;                           // asm: mov eax,1

    const std::size_t bytes = chars * 2;             // asm: add eax,eax
    wchar_t* buf = reinterpret_cast<wchar_t*>(new BYTE[bytes]);   // asm: call new
    *pSlot = buf;
    buf[0] = L'\0';                                  // asm: mov word ptr [eax],0

    if (pSource != nullptr) {
        std::wcscpy(buf, pSource);                   // asm: call _wcscpy
    }
    return pSlot;                                    // asm: mov eax,esi
}

// ============================================================================
// Unicode_AssignStringNumeric  (asm: Unicode_AssignStringNumeric, 0x7B7E00)
//
//   sub esp, 40h
//   lea eax,[esp+Source] / push edx / push offset aD_6 / push eax
//   call _swprintf                       ; swprintf(buf, L"%d", edx)
//   mov esi,ecx / mov dword [esi],0
//   test ecx,ecx / jz -> len = 1
//   call _wcslen ; inc eax               ; len = wcslen(buf)+1
//   add eax,eax ; call new               ; new(len*2 bytes)
//   mov [esi],eax ; mov word [eax],0
//   call _wcscpy                         ; wcscpy([esi], buf)
//   mov eax,esi / retn
// ============================================================================
wchar_t** Unicode::AssignStringNumeric(wchar_t** pSlot, int32 number)
{
    wchar_t buf[32];                                     // asm: sub esp,40h

    // asm: push edx / push offset aD_6 / push eax / call _swprintf
    std::swprintf(buf, 32, L"%d", static_cast<int>(number));

    if (pSlot == nullptr)
        return nullptr;                                  // asm: mov eax,esi

    *pSlot = nullptr;                                    // asm: mov dword [esi],0

    // asm: call _wcslen / inc eax
    const std::size_t chars = std::wcslen(buf) + 1;

    // asm: add eax,eax / call new
    wchar_t* pDest = reinterpret_cast<wchar_t*>(new BYTE[chars * 2]);

    *pSlot = pDest;                                      // asm: mov [esi],eax
    pDest[0] = L'\0';                                    // asm: mov word [eax],0

    std::wcscpy(pDest, buf);                             // asm: call _wcscpy
    return pSlot;                                        // asm: mov eax,esi
}
