#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// LineTrailClass - fading trail segments behind flying projectiles
//
// asm: LineTrailClass_SetDecrement (0x556B40) writes the per-frame fade step
// into [+8] of the trail state.  When the global dword_A8EB78 (the
// "slow-machine" detail flag) is clear the requested decrement is doubled so
// the trail burns twice as fast.
// ============================================================================
class LineTrailClass {
public:
    LineTrailClass() noexcept : Volume(0), Decrement(0), Color(0) {}

    // asm: LineTrailClass_SetDecrement
    //   eax = dword_A8EB78 ; test ; jnz keep
    //   shl eax, 1                 ; double the step on fast machines
    // keep:
    //   mov [ecx+8], eax
    static void SetDecrement(LineTrailClass* pTrail, int32 decrement);

    int32 Volume;       // asm +0x00: remaining trail life
    int32 Decrement;    // asm +0x08: fade step per frame
    int32 Color;        // trail color index
};

// Global detail flag consulted by SetDecrement (asm dword_A8EB78).
extern int32 g_LineTrailFastFlag;
