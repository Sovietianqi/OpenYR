#include "FacingStruct.h"

// Game frame counter (same global the original `Frame` symbol resolves to).
#include "../Game/Game.h"

// ============================================================================
// FacingStruct_InitTurnable  (asm: FacingStruct_InitTurnable, 0x4C9680)
//
//   mov eax, dword ptr [esp+ROT]
//   cmp eax, 7Fh
//   jl  short loc_4C968E
//   mov eax, 7Fh
// loc_4C968E:
//   xor edx, edx
//   mov dh, al                 ; zero-extend the low byte
//   mov [ecx+14h], dx          ; store as the 16-bit turn rate
//   retn 4
//
// Called from AircraftClass_Init and friends to (re)arm the turn machinery.
// ============================================================================
void FacingStruct::InitTurnable(int32 ROT)
{
    int32 rate = ROT;
    if (rate >= 0x7F)          // asm: cmp eax, 7Fh ; signed compare
        rate = 0x7F;           // asm: mov eax, 7Fh

    TurnRate = static_cast<int16>(static_cast<uint8>(rate)); // asm: mov dh,al
}

// ============================================================================
// FacingStruct_IsSet  (asm: FacingStruct_IsSet, 0x4C94B0)
//
// Reports whether the owner still has an in-progress, forward-facing rotation.
// The tail compares (FacingTarget - FacingCurrent) as a signed 16-bit value
// and keeps the "set" verdict only while that delta is strictly positive
// (asm: setnle -- sign-flag of `> 0`).
// ============================================================================
bool FacingStruct::IsSet() const
{
    if (TurnRate <= 0)                       // asm: cmp word [ecx+14h],0 ; jle fail
        return false;

    int32 remaining = TurnDuration;
    if (StartFrame != -1) {                  // asm: cmp esi, 0FFFFFFFFh ; jz check
        const int32 elapsed = Game::CurrentFrame - StartFrame;  // asm: sub edx, esi
        if (elapsed >= TurnDuration)         // asm: cmp edx, eax ; jge fail
            return false;
        remaining = TurnDuration - elapsed;  // asm: sub eax, edx
    }

    if (remaining == 0)                      // asm: test eax, eax ; jz fail
        return false;

    const int16 delta = static_cast<int16>(FacingTarget - FacingCurrent);
    return delta > 0;                        // asm: setnle cl
}
