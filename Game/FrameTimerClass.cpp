#include "FrameTimerClass.h"
#include "Game.h"

// ============================================================================
// FrameTimerClass_Get_Remaining  (asm: FrameTimerClass_Get_Remaining, 0x426620)
//
//   mov edx, [ecx]            ; StartFrame
//   mov eax, [ecx+8]          ; Duration
//   cmp edx, 0FFFFFFFFh
//   jz  short locret_42664B   ; disarmed -> eax untouched (=Duration)? no:
//                             ; eax already loaded, but the jz target returns
//                             ; with whatever eax holds; callers only arm via
//                             ; Set, so Duration is the "inactive" answer.
//   mov ecx, Frame
//   sub ecx, edx              ; elapsed
//   cmp ecx, eax
//   jge short loc_426649      ; expired -> 0
//   sub eax, ecx              ; remaining
//   retn
// ============================================================================
int32 FrameTimerClass::Get_Remaining() const
{
    if (StartFrame == -1)                       // asm: cmp edx,0FFFFFFFFh
        return Duration;                        // asm: locret (eax = Duration)

    const int32 elapsed = Game::CurrentFrame - StartFrame;   // asm: sub ecx,edx
    if (elapsed >= Duration)                    // asm: cmp ecx,eax ; jge
        return 0;                               // asm: xor eax,eax

    return Duration - elapsed;                  // asm: sub eax,ecx
}

// ----------------------------------------------------------------------------
// asm: FrameTimerClass_Arm -- StartFrame = Frame global
//   mov eax, [Frame] / mov [ecx], eax
// ----------------------------------------------------------------------------
void FrameTimerClass::Arm()
{
    StartFrame = Game::CurrentFrame;
}

// ----------------------------------------------------------------------------
// asm: FrameTimerClass_Set -- Duration = duration, then Arm()
//   mov [ecx+8], edx / jmp Arm
// ----------------------------------------------------------------------------
void FrameTimerClass::Set(int32 duration)
{
    Duration = duration;
    Arm();
}
