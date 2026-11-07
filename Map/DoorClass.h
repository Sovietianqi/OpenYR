#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// DoorClass - cargo door / hatch state machine (building sub-structure)
//
// asm: DoorClass_Open_Door (0x4A51D0) reads the two state bytes at +0x18 and
// +0x19 (opening / closing flags).  A door is "openable" only when neither
// flag is set.
// ============================================================================
class DoorClass {
public:
    DoorClass() noexcept : IsOpening(false), IsClosing(false), Progress(0), Stage(0) {}

    // asm: DoorClass_Open_Door_4A51D0
    //   al = [ecx+18h] ; test ; jnz busy
    //   al = [ecx+19h] ; test ; jnz busy
    //   mov al, 1          ; idle -> may open
    // busy:
    //   xor al, al
    bool Open_Door() const
    {
        if (IsOpening || IsClosing)      // asm: jnz loc_4A51E1
            return false;
        return true;                     // asm: mov al, 1
    }

    bool  IsOpening;    // asm +0x18
    bool  IsClosing;    // asm +0x19
    int32 Progress;     // door animation progress
    int32 Stage;        // staged animation step
};
