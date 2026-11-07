#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// Vector3D / VecFloat3D - 3-component vectors matching the asm layout
//
//   Vector3D    : three int32 components (asm uses dword moves)
//   VecFloat3D  : three double components (asm uses qword FPU loads)
//
// Recovered procedures:
//   Vector3D_Subtract        (this = minuend, vec2 = subtrahend, buffer out)
//   Vector3D_Equal           (this == vec2 ? 1 : 0)
//   VecFloat3D_GetLength     (sqrt(x*x + y*y + z*z) as double)
// ============================================================================

struct Vector3D {
    int32 X;
    int32 Y;
    int32 Z;

    Vector3D() noexcept : X(0), Y(0), Z(0) {}
    Vector3D(int32 x, int32 y, int32 z) noexcept : X(x), Y(y), Z(z) {}

    // asm: Vector3D_Subtract -- writes this - vec2 into pOut.
    //   mov edx, [ecx]      ; this.X
    //   sub edx, esi        ; - vec2.X
    //   ...                 ; same for Y/Z
    //   mov [edi], edx ...  ; store all three
    void Subtract(Vector3D* pOut, const Vector3D* pVec2) const;

    // asm: Vector3D_Equal -- component-wise compare, 1 on full equality.
    //   cmp edx, [eax] / jnz fail  (X)
    //   cmp edx, esi / jnz fail    (Y)
    //   cmp ecx, edx / jnz fail    (Z)
    //   mov eax, 1
    bool Equal(const Vector3D* pOther) const;
};

struct VecFloat3D {
    double X;
    double Y;
    double Z;

    VecFloat3D() noexcept : X(0.0), Y(0.0), Z(0.0) {}
    VecFloat3D(double x, double y, double z) noexcept : X(x), Y(y), Z(z) {}

    // asm: VecFloat3D_GetLength -- sqrt(x^2 + y^2 + z^2), returned as double.
    //   fld qword [ecx+10h] / [ecx+8] / [ecx]  ; push Z, Y, X
    //   fmul chains accumulate x^2 + y^2 + z^2
    //   call float_sqrt
    double GetLength() const;
};
