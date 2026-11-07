#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// FacingStruct - smooth-turn state block used by aircraft / turrets
//
// Recovered layout (offsets from the asm procs):
//   +0x00  int16  FacingTarget   -- destination facing (256-step space)
//   +0x04  int16  FacingCurrent  -- facing this frame
//   +0x08  int32  StartFrame     -- frame the turn began (-1 = not turning)
//   +0x10  int32  TurnDuration   -- frames the whole turn spans
//   +0x14  int16  TurnRate       -- ROT clamped to <= 0x7F (127)
//
// Recovered procedures:
//   FacingStruct_InitTurnable (0x4C9680): clamps the ROT argument to 127 and
//     stores it as the turn rate word.
//   FacingStruct_IsSet (0x4C94B0): true while a turn is in progress and the
//     remaining rotation delta is still positive.
// ============================================================================
struct FacingStruct {
    int16 FacingTarget;
    int16 FacingCurrent;
    int32 StartFrame;
    int32 TurnDuration;
    int16 TurnRate;
    int16 pad_16;

    FacingStruct() noexcept
        : FacingTarget(0), FacingCurrent(0)
        , StartFrame(-1), TurnDuration(0)
        , TurnRate(0), pad_16(0) {}

    // asm: FacingStruct_InitTurnable
    //   mov eax, [esp+ROT]      ; ROT arrives in the low byte
    //   cmp eax, 7Fh
    //   jl  clamp_done
    //   mov eax, 7Fh            ; cap at 127
    // clamp_done:
    //   mov dh, al / mov [ecx+14h], dx
    void InitTurnable(int32 ROT);

    // asm: FacingStruct_IsSet
    //   cmp word ptr [ecx+14h], 0 / jle fail   ; rate must be positive
    //   mov esi, [ecx+8]                        ; StartFrame
    //   mov eax, [ecx+10h]                      ; TurnDuration
    //   cmp esi, -1 / jz check_delta            ; turn not started -> use full span
    //   mov edx, Frame / sub edx, esi           ; elapsed = Frame - StartFrame
    //   cmp edx, eax / jge fail                 ; elapsed >= duration -> done
    //   sub eax, edx                            ; remaining = duration - elapsed
    // check_delta:
    //   test eax, eax / jz fail                 ; no frames left
    //   mov ax, [ecx] / sub ax, [ecx+4]         ; delta = target - current
    //   setnle cl                               ; (int16)delta > 0
    bool IsSet() const;
};
