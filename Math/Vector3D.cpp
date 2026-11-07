#include "Vector3D.h"
#include <cmath>

// ============================================================================
// Vector3D_Subtract  (asm: Vector3D_Subtract)
//
//   mov eax, [esp+vec2]
//   mov edx, [ecx]        ; this.X
//   mov esi, [eax]        ; vec2.X
//   mov edi, [eax+4]      ; vec2.Y
//   sub edx, esi          ; X = this.X - vec2.X
//   mov esi, [ecx+4]      ; this.Y
//   mov ecx, [ecx+8]      ; this.Z
//   mov edi, [eax+8]      ; vec2.Z
//   sub esi, edi          ; Y
//   mov eax, [esp+buffer]
//   sub ecx, edi          ; Z (asm reuses edi; net effect identical)
//   mov [eax], edx / [eax+4], esi / [eax+8], ecx
//
// pOut may alias neither vector in the original call sites.
// ============================================================================
void Vector3D::Subtract(Vector3D* pOut, const Vector3D* pVec2) const
{
    if (!pOut || !pVec2)
        return;

    pOut->X = X - pVec2->X;
    pOut->Y = Y - pVec2->Y;
    pOut->Z = Z - pVec2->Z;
}

// ============================================================================
// Vector3D_Equal  (asm: Vector3D_Equal)
//
//   mov eax, [esp+a2]
//   cmp edx, [eax]  / jnz fail   ; X
//   cmp edx, esi    / jnz fail   ; Y
//   cmp ecx, edx    / jnz fail   ; Z
//   mov eax, 1 / retn 4
// fail:
//   xor eax, eax
// ============================================================================
bool Vector3D::Equal(const Vector3D* pOther) const
{
    if (pOther == nullptr)
        return false;

    if (X != pOther->X) return false;   // asm: jnz loc_459F88
    if (Y != pOther->Y) return false;
    if (Z != pOther->Z) return false;

    return true;                        // asm: mov eax, 1
}

// ============================================================================
// VecFloat3D_GetLength  (asm: VecFloat3D_GetLength)
//
//   fld qword ptr [ecx+10h]   ; Z   (pushed first, ends deepest)
//   fld qword ptr [ecx+8]     ; Y
//   fld qword ptr [ecx]       ; X
//   fld st                    ; dup X
//   fmul st, st(1)            ; X^2
//   fld st(2)                 ; Y
//   fmul st, st(3)            ; Y^2
//   faddp st(1), st           ; X^2 + Y^2
//   fld st(3)                 ; Z
//   fmul st, st(4)            ; Z^2
//   faddp st(1), st           ; X^2 + Y^2 + Z^2
//   call float_sqrt
//
// The original returns the length in st(0) -- a double here.
// ============================================================================
double VecFloat3D::GetLength() const
{
    return std::sqrt(X * X + Y * Y + Z * Z);
}
