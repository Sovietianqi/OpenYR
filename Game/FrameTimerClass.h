#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Containers/DynamicVectorClass.h"

class HouseClass;

// ============================================================================
// FrameTimerClass - frame-quantized countdown used by animations & missions
//
// asm layout recovered from FrameTimerClass_Get_Remaining (0x426620):
//   +0x00  int32  StartFrame   -- frame the timer was armed (-1 = inactive)
//   +0x08  int32  Duration     -- total frames the timer spans
// ============================================================================
class FrameTimerClass {
public:
    FrameTimerClass() noexcept : StartFrame(-1), Duration(0) {}
    FrameTimerClass(int32 duration) noexcept : StartFrame(-1), Duration(duration) {}

    void Set(int32 duration);   // implemented in cpp (needs Game::CurrentFrame)
    void Arm();                 // implemented in cpp (needs Game::CurrentFrame)
    void Disarm() { StartFrame = -1; }

    // asm: FrameTimerClass_Get_Remaining (0x426620)
    //   edx = [ecx] ; eax = [ecx+8]
    //   cmp edx, -1 / jz return0        ; inactive -> 0
    //   ecx = Frame - edx               ; elapsed
    //   cmp ecx, eax / jge zero         ; elapsed >= duration -> 0
    //   eax -= ecx                      ; remaining
    int32 Get_Remaining() const;

    bool Has_Expired() const { return Get_Remaining() == 0; }

    int32 StartFrame;   // asm +0x00
    int32 Duration;     // asm +0x08
};
