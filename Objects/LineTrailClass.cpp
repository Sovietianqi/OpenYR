#include "LineTrailClass.h"

// ============================================================================
// LineTrailClass globals
// ============================================================================
int32 g_LineTrailFastFlag = 0;    // asm: dword_A8EB78 (0 = fast machine)

// ============================================================================
// LineTrailClass_SetDecrement  (asm: LineTrailClass_SetDecrement, 0x556B40)
//
//   mov eax, dword_A8EB78
//   test eax, eax
//   mov eax, [esp+decrement]
//   jnz loc_556B5F
//   shl eax, 1                    ; fast path: twice the fade step
// loc_556B5F:
//   mov [ecx+8], eax              ; trail->Decrement = step
// ============================================================================
void LineTrailClass::SetDecrement(LineTrailClass* pTrail, int32 decrement)
{
    if (pTrail == nullptr)
        return;

    int32 step = decrement;
    if (g_LineTrailFastFlag == 0)          // asm: test eax,eax ; jnz keep
        step = decrement * 2;              // asm: shl eax, 1

    pTrail->Decrement = step;              // asm: mov [ecx+8], eax
}
