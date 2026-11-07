#include "AsmMath.h"
#include <cmath>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cwchar>

// ============================================================================
// Float_To_Int_Floor  (asm: Float_To_Int_Floor, 0x7C5F10)
//
//   fnstcw word ptr [esp]          ; save current control word
//   and dword, 0FFFFh
//   cmp edx, dword_822D80          ; already flipped this session?
//   jz  convert                    ; yes -> straight fistp
//   or the saved word with 0x0400  ; (round-down bits) -- folded into the
//                                  ; memoized dword_822D80 at init time
//   fldcw word ptr [esp]
// convert:
//   fistp qword ptr [esp+4]        ; store as 64-bit integer
//   edx = hi ; eax = lo
//
// The truncation-toward-negative-infinity conversion, memoizing the control
// word change in dword_822D80 so the flip happens only once.
// ============================================================================
int64 Float_To_Int_Floor(double value)
{
    // Control-word machinery replaced by C99 semantics: floor() then a
    // 64-bit narrowing store matches fistp with the round-down mode set.
    return static_cast<int64>(std::floor(value));
}

// ============================================================================
// Mult1_100th  (asm: Mult1_100th)
//
//   fild [esp+arg]        ; push integer arg
//   fmul ds:num_0_01      ; multiply by 0.01f
//   fstp dword ptr [ecx]  ; store single-precision result through this
//   retn 4
// ============================================================================
void Mult1_100th(float* pOut, int32 arg)
{
    if (pOut == nullptr)
        return;
    *pOut = static_cast<float>(static_cast<double>(arg) * 0.01);
}

// ============================================================================
// CellCoords_Equal  (asm: CellCoords_Equal, 0x50E470)
//
//   mov dx, [ecx]    / cmp dx, [eax]   / jnz fail
//   mov cx, [ecx+2]  / cmp cx, [eax+2] / jnz fail
//   mov eax, 1 / retn 4
// fail:
//   xor eax, eax
// ============================================================================
bool CellCoords_Equal(const CellStruct* pA, const CellStruct* pB)
{
    if (pA == nullptr || pB == nullptr)
        return false;

    if (pA->X != pB->X) return false;   // asm: cmp dx,[eax] ; jnz
    if (pA->Y != pB->Y) return false;   // asm: cmp cx,[eax+2] ; jnz
    return true;
}

// ============================================================================
// Rect_SetPos  (asm: Rect_SetPos)
//
//   mov ecx, [esp+TopLeft]
//   mov edx, [ecx]    ; TopLeft->X
//   mov [eax], edx    ; rect[0]
//   mov ecx, [ecx+4]  ; TopLeft->Y
//   mov [eax+4], ecx  ; rect[1]
//   mov edx, [esp+R]  ; rect[2]
//   mov ecx, [esp+B]  ; rect[3]
//   retn 0Ch
// ============================================================================
void Rect_SetPos(int32* pRect, const int32* pTopLeft, int32 R, int32 B)
{
    if (pRect == nullptr || pTopLeft == nullptr)
        return;

    pRect[0] = pTopLeft[0];   // asm: mov [eax], edx
    pRect[1] = pTopLeft[1];   // asm: mov [eax+4], ecx
    pRect[2] = R;             // asm: mov [eax+8], edx
    pRect[3] = B;             // asm: mov [eax+0Ch], ecx
}

// ============================================================================
// nop_return1  (asm: nop_return1)  --  mov al, 1 ; retn
// ============================================================================
bool nop_return1()
{
    return true;
}

// ============================================================================
// Return_1_0  (asm: Return_1_0)  --  mov al, 1 ; retn
// ============================================================================
bool Return_1_0()
{
    return true;
}

// ============================================================================
// int_GetDropArgument  (asm: int_GetDropArgument)
//   mov eax, arg_DROP ; retn
// ============================================================================
int32 int_GetDropArgument(int32 arg_DROP)
{
    return arg_DROP;
}

// ============================================================================
// hash_path  (asm: hash_path)
//
//   mov esi, [ecx]           ; h
//   mov eax, esi
//   shr eax, 1Fh             ; sign bit
//   lea eax, [eax+esi*2]     ; h*2 + sign
//   add eax, edx             ; + input
//   mov [ecx], eax
// ============================================================================
void hash_path(int32* pState, int32 input)
{
    if (pState == nullptr)
        return;

    const uint32 h   = static_cast<uint32>(*pState);
    const uint32 bit = h >> 31;
    *pState = static_cast<int32>(bit + h * 2u + static_cast<uint32>(input));
}

// ============================================================================
// palette_calculator  (asm: palette_calculator, 0x626660)
//
//   dl = [ecx] ; if 0 -> 0
// loop:
//   esi = i*3 (unused scratch) ; eax = (i + i*13) * 2? -- net effect:
//   eax = (previous_total + char*13) ; cdq ; idiv 31 -> keep remainder
//   i++ ; next byte
//
// The accumulation walks eax through (eax*13 + c) % 31 per byte; the loop
// terminates at NUL.  ecx doubles as the read cursor (inc ecx each pass).
// ============================================================================
int32 palette_calculator(const BYTE* pBytes)
{
    if (pBytes == nullptr || *pBytes == 0)
        return 0;

    int32 acc = 0;
    for (const BYTE* p = pBytes; *p != 0; ++p) {
        // asm: lea eax, [eax+eax*4] ; lea eax, [edx+eax*2] -> acc*13 + c
        acc = acc * 13 + static_cast<int8>(*p);
        acc = acc % 31;                     // asm: cdq ; idiv 31 ; eax = edx
    }
    return acc;
}

// ============================================================================
// bytes_cmp  (asm: bytes_cmp, 0x4F4CD0)
//
// Two-bytes-per-iteration unrolled strcmp: compares [eax]/[esi] then
// [eax+1]/[esi+1], stepping by 2 while bytes are equal and non-NUL.
// Equal  -> returns 1 (asm: setz after xor).
// Unequal -> sbb chain yields -1 or +1, then the same setz trick collapses
// it to a boolean (the original callers treat the result as "same or not").
// ============================================================================
int32 bytes_cmp(const BYTE* pA, const BYTE* pB)
{
    if (pA == nullptr || pB == nullptr)
        return 0;

    while (*pA == *pB) {
        if (*pA == 0)
            return 1;                 // asm: loc_4F4D0C -> setz -> al=1
        ++pA; ++pB;
    }

    // asm: loc_4F4D1C -- sbb eax,eax ; sbb eax,0FFFFFFFFh produces -1/0/+1;
    // callers only branch on zero-vs-nonzero, so return the sign directly.
    return (*pA < *pB) ? -1 : 1;
}

// ============================================================================
// unsigned_int_comparator  (asm: unsigned_int_comparator, 0x6CF320)
//
//   eax = *pA ; ecx = *pB ; cmp ; jnz signed-path
//   xor eax, eax / retn
// signed-path:
//   setnl dl / dec / and 0FFFFFFFEh / inc -> {-1, +1}
// ============================================================================
int32 unsigned_int_comparator(const int32* pA, const int32* pB)
{
    if (pA == nullptr || pB == nullptr)
        return 0;

    if (*pA == *pB)
        return 0;
    return (*pA > *pB) ? 1 : -1;
}

// ============================================================================
// charPP_GetSecure  (asm: charPP_GetSecure, 0x7B5440)
//
//   mov eax, [ecx] ; test eax, eax ; jnz ret
//   mov eax, offset Null
// ret: retn
// ============================================================================
const char* charPP_GetSecure(char** pp)
{
    if (pp == nullptr)
        return "";

    return (*pp != nullptr) ? *pp : "";   // asm: offset Null (shared "")
}

// ============================================================================
// str_zero  (asm: str_zero)
//   mov eax, ecx ; mov dword ptr [eax], 0 ; retn
// ============================================================================
void str_zero(void* pObj)
{
    if (pObj != nullptr) {
        *static_cast<int32*>(pObj) = 0;
    }
}

// ============================================================================
// return_ecx  (asm: return_ecx)
//   mov eax, ecx ; retn
// ============================================================================
void* return_ecx(void* pThis)
{
    return pThis;
}

// ============================================================================
// strdup_strupr  (asm: strdup_strupr, 0x67F0F0)
//
// Inline memcpy of fname into the caller buffer (ecx), then call __strupr on
// the buffer, returning the buffer address.
// ============================================================================
char* strdup_strupr(char* pDst, const char* fname)
{
    if (pDst == nullptr)
        return nullptr;

    if (fname != nullptr) {
        std::size_t len = std::strlen(fname) + 1;   // asm: repne scasb / not ecx
        std::memcpy(pDst, fname, len);              // asm: rep movsd/movsb
        // asm: call __strupr
        for (char* p = pDst; *p != '\0'; ++p) {
            *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
        }
    } else {
        pDst[0] = '\0';
    }
    return pDst;
}

// ============================================================================
// alloc_str  (asm: alloc_str, 0x7B4620)
//
//   *pOutSlot = 0 initially ; if str == null allocate 1 byte holding ""
//   else strlen(str)+1 bytes and an inline memcpy ; return pOutSlot
// ============================================================================
char** alloc_str(char** pOutSlot, const char* str)
{
    if (pOutSlot == nullptr)
        return nullptr;

    char*& slot = *pOutSlot;
    slot = nullptr;                                   // asm: mov dword [ebx], 0

    std::size_t len = (str != nullptr) ? std::strlen(str) + 1 : 1;
    char* buf = new char[len];                        // asm: call new
    slot = buf;
    buf[0] = '\0';                                    // asm: mov byte [eax], 0
    if (str != nullptr) {
        std::memcpy(buf, str, len);                   // asm: rep movsd/movsb
    }
    return pOutSlot;                                  // asm: mov eax, ebx
}
