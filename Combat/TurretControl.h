#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// TurretControl - turret recoil state machine (recovered sub-structure)
//
// The original embeds this block inside BuildingClass/TechnoClass state and
// drives it from BuildingClass_Mi_Missile and the standard weapon-fire path.
// Field offsets recovered from TurretControl_FireWithRecoil:
//
//   +0x10  double  RecoilRate    -- leptons of recoil applied per frame
//   +0x??  int32   Travel        -- total recoil travel requested on fire
//   +0x??  int32   CompressFrames-- frames over which the travel compresses
//   +0x18  int32   Stage         -- recoil phase flag (1 = compressing)
//   +0x1C  int32   ActiveFrames  -- clamped compression duration (>= 1)
// ============================================================================

// Forward declaration of the embedding class is unnecessary: the original
// procedures operate purely on the sub-structure, so the struct is standalone
// and embedding classes simply hold it by value.
struct TurretControl {
    double RecoilRate;      // asm field_10 (double)
    int32  Travel;          // requested recoil travel
    int32  CompressFrames;  // authored compression duration
    int32  Stage;           // asm field_18
    int32  ActiveFrames;    // asm field_1C

    TurretControl() noexcept
        : RecoilRate(0.0), Travel(0)
        , CompressFrames(0), Stage(0), ActiveFrames(0) {}

    // asm: TurretControl_FireWithRecoil (0x70EC50)
    //
    //   mov eax, [ecx+Travel]
    //   test eax, eax
    //   jz  done                     ; zero travel -> nothing to animate
    //   mov var_4, eax               ; spill travel for the FPU load
    //   mov eax, [ecx+CompressFrames]
    //   mov edx, 1
    //   mov [ecx+field_18], edx      ; Stage = 1 (compressing)
    //   mov [ecx+field_1C], eax      ; ActiveFrames = CompressFrames
    //   cmp eax, edx
    //   jge fpu                      ; >= 1 keeps the authored value
    //   mov [ecx+field_1C], edx      ; clamp to 1 frame minimum
    // fpu:
    //   fild var_4                   ; (double)Travel
    //   fidiv [ecx+field_1C]         ; / ActiveFrames
    //   fstp [ecx+field_10]          ; RecoilRate = Travel / ActiveFrames
    // done:
    //   retn
    void FireWithRecoil();
};
