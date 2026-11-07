// ============================================================================
// AsmPalZero.h - ZeroOut_?_pal family (gamemd.exe, YR 1.001)
//
// Five 16-line procs referenced from the palette callback tables in .data
// (00812188/0081219C/008121A0/008121A4).  Each zeroes the 256*3 RGB bytes of
// one palette global:
//   ZeroOut_?_pal     -> GamePalette   (asm 0x40D940-ish, XREF 0081219C)
//   ZeroOut_?_pal_0   -> XXX_PAL_3     (XREF 00812188, listed twice)
//   ZeroOut_?_pal_0b  -> XXX_PAL_3     (duplicate listing)
//   ZeroOut_?_pal_1   -> XXX_PAL       (XREF 008121A0)
//   ZeroOut_?_pal_2   -> XXX_PAL_4     (XREF 008121A4)
// asm shape (identical for all five):
//   mov eax, offset <PAL>.Colors.B   ; Colors.R + 2
//   mov ecx, 100h ; xor dl,dl
//   loop: [eax-2]=dl ; [eax-1]=dl ; [eax]=dl ; eax += 3 ; dec ecx / jnz
//   retn
// The '?' in the dump labels cannot appear in C++ identifiers, hence the
// sanitized names below.
// ============================================================================

void ZeroOut_pal(void);        // GamePalette
void ZeroOut_pal_0(void);      // XXX_PAL_3
void ZeroOut_pal_0b(void);     // XXX_PAL_3 (duplicate proc listing)
void ZeroOut_pal_1(void);      // XXX_PAL
void ZeroOut_pal_2(void);      // XXX_PAL_4

// Bindable palette bases (each must point at Colors.R, i.e. 0x300 bytes).
void AsmPZ_Set_GamePalette(void* p);
void AsmPZ_Set_XXX_PAL_3(void* p);
void AsmPZ_Set_XXX_PAL(void* p);
void AsmPZ_Set_XXX_PAL_4(void* p);
