// ============================================================================
// AsmPalZero.cpp - ZeroOut_?_pal family (see AsmPalZero.h for the asm map).
// ============================================================================

#include "AsmPalZero.h"

#include <cstdint>

namespace {

typedef uint8_t u8;

u8* s_PZGamePal = nullptr;      // GamePalette.Colors.R
u8* s_PZXXX3 = nullptr;         // XXX_PAL_3.Colors.R
u8* s_PZXXX = nullptr;          // XXX_PAL.Colors.R
u8* s_PZXXX4 = nullptr;         // XXX_PAL_4.Colors.R

// asm: mov eax,offset <PAL>.Colors.B ; mov ecx,100h ; xor dl,dl ;
//      loc: [eax-2],dl / [eax-1],dl / [eax],dl ; add eax,3 ; dec ecx / jnz.
// The B-based pointer with -2/-1/0 offsets writes R,G,B in order, i.e. the
// full 0x300-byte palette body.
inline void PalZero13(u8* pal)
{
    if (pal == nullptr)
        return;                    // unbound palette: nothing to zero
    u8* p = pal;                   // asm: eax = &Colors.B - 2 == &Colors.R
    for (uint32_t i = 0x100; i != 0; --i) {
        p[0] = 0;                  // asm: mov [eax-2],dl
        p[1] = 0;                  // asm: mov [eax-1],dl
        p[2] = 0;                  // asm: mov [eax],dl
        p += 3;                    // asm: add eax,3
    }
}

}  // namespace

void ZeroOut_pal(void)   { PalZero13(s_PZGamePal); }
void ZeroOut_pal_0(void) { PalZero13(s_PZXXX3); }
void ZeroOut_pal_0b(void) { PalZero13(s_PZXXX3); }
void ZeroOut_pal_1(void) { PalZero13(s_PZXXX); }
void ZeroOut_pal_2(void) { PalZero13(s_PZXXX4); }

void AsmPZ_Set_GamePalette(void* p) { s_PZGamePal = static_cast<u8*>(p); }
void AsmPZ_Set_XXX_PAL_3(void* p)   { s_PZXXX3 = static_cast<u8*>(p); }
void AsmPZ_Set_XXX_PAL(void* p)     { s_PZXXX = static_cast<u8*>(p); }
void AsmPZ_Set_XXX_PAL_4(void* p)   { s_PZXXX4 = static_cast<u8*>(p); }
