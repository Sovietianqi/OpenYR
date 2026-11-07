#include "TurretControl.h"

// ============================================================================
// TurretControl_FireWithRecoil  (asm: TurretControl_FireWithRecoil, 0x70EC50)
//
// Called by BuildingClass_Mi_Missile (and the shared TechnoClass fire path)
// the moment a turret weapon discharges: arms the recoil animation by
// staging the compression and computing the per-frame recoil increment.
//
//   var_4 = Travel
//   if (Travel == 0) return
//   Stage        = 1
//   ActiveFrames = (CompressFrames >= 1) ? CompressFrames : 1
//   RecoilRate   = (double)Travel / ActiveFrames
//
// Note the original divides through the FPU (fild + fidiv), so the division
// is performed in double precision before storing back into field_10.
// ============================================================================
void TurretControl::FireWithRecoil()
{
    if (Travel == 0)                                   // asm: test eax,eax ; jz done
        return;

    Stage = 1;                                         // asm: mov [ecx+field_18], edx (edx=1)
    ActiveFrames = CompressFrames;                     // asm: mov [ecx+field_1C], eax
    if (ActiveFrames < 1)                              // asm: cmp eax, edx ; jge fpu
        ActiveFrames = 1;                              // asm: mov [ecx+field_1C], edx

    // asm: fild var_4 ; fidiv [ecx+field_1C] ; fstp [ecx+field_10]
    RecoilRate = static_cast<double>(Travel) / static_cast<double>(ActiveFrames);
}
