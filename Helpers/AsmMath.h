#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// Helpers/AsmMath.h - free-function primitives recovered from the binary
//
// These are the small standalone procs the IDA dump names without a class
// qualifier; they are collected here so the translation can reference them
// under their original names.
// ============================================================================

// asm: Float_To_Int_Floor (0x7C5F10)
// Converts a double to int64 with the FPU rounding mode forced to "floor"
// (chop toward -inf), caching the control-word flip.  The original keeps
// dword_822D80 as the memoized control word.
int64 Float_To_Int_Floor(double value);

// asm: Mult1_100th (0x54E1F0-ish)
//   fild [esp+arg]      ; int arg
//   fmul ds:num_0_01    ; * 0.01
//   fstp dword ptr [ecx] ; store into *this
// __thiscall store: writes arg * 0.01f through the this pointer.
void Mult1_100th(float* pOut, int32 arg);

// asm: CellCoords_Equal (0x50E470)
// Component compare of two CellStruct values (int16 X/Y each).
bool CellCoords_Equal(const CellStruct* pA, const CellStruct* pB);

// asm: Rect_SetPos (0x5961A0-ish)
//   [this+0] = TopLeft->X ; [this+4] = TopLeft->Y
//   [this+8] = R ; [this+0Ch] = B
// Copies a Point2D into the first two dwords and two scalars into the rest.
void Rect_SetPos(int32* pRect, const int32* pTopLeft, int32 R, int32 B);

// asm: nop_return1 (0x6B0C70-ish) -- constant-true predicate used by WinMain.
bool nop_return1();

// asm: Return_1_0 -- Load_Bootstrap helper returning true.
bool Return_1_0();

// asm: int_GetDropArgument -- returns the DROP event argument verbatim.
int32 int_GetDropArgument(int32 arg_DROP);

// asm: hash_path (0x5DAB50-ish)
//   esi = [ecx] ; eax = (esi >> 31) + esi*2 ; eax += edx ; [ecx] = eax
// Rolling hash update: h = h*2 + sign-bit + input.  ecx points at the
// accumulator, edx is the path character/word being folded in.
void hash_path(int32* pState, int32 input);

// asm: palette_calculator (0x626660)
// Sums (c * 13) % 31 over the NUL-terminated byte string at ecx and returns
// the total -- a palette checksum guard used by ConvertClass loading.
int32 palette_calculator(const BYTE* pBytes);

// asm: bytes_cmp (0x4F4CD0)
// Unsigned byte-string compare of ecx vs val2 up to and including NUL;
// returns 1 on equality, else the sign of (a - b) clamped via sbb.
int32 bytes_cmp(const BYTE* pA, const BYTE* pB);

// asm: unsigned_int_comparator (0x6CF320)
// Classic qsort comparator over two int32 pointers: -1/0/+1.
int32 unsigned_int_comparator(const int32* pA, const int32* pB);

// asm: charPP_GetSecure (0x7B5440)
//   eax = [ecx] ; test ; jz -> return &Null
// Returns *pp or the shared empty string when null.
const char* charPP_GetSecure(char** pp);

// asm: str_zero (0x5A08A0-ish) -- zeroes the leading dword of the object.
void str_zero(void* pObj);

// asm: return_ecx (0x4F8DB0-ish) -- identity thunk returning its this.
void* return_ecx(void* pThis);

// asm: strdup_strupr (0x67F0F0-ish) -- copies the NUL-terminated fname into
// the caller-provided buffer (ecx) and upper-cases it in place.
char* strdup_strupr(char* pDst, const char* fname);

// asm: alloc_str (0x7B4620) -- heap-allocates a copy of str into *pOutSlot
// (or an empty string when str is null) and returns the slot address.
char** alloc_str(char** pOutSlot, const char* str);
