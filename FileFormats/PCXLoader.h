#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class PCXClass;

// ============================================================================
// PCX engine-side loader wrappers (asm: PCX_LoadFile group)
//
//  The original exposes PCX_LoadFile(src, a2, a3) where the two flags steer
//  palette conversion and buffer ownership.  PCX_LoadFile11 (0x6B68E0) is
//  the "load file, build palette" convenience thunk used by Game::LoadPCXFiles:
//
//    mov eax, [esp+Src]
//    push 1 / push 1 / push eax
//    call PCX_LoadFile
//    retn 4
//
//  i.e. it always loads with both flags = 1.
// ============================================================================

// ----------------------------------------------------------------------------
// asm: PCX_LoadFile11 (0x6B68E0) - forwards (src, 1, 1) to PCX_LoadFile.
// Returns the loaded PCXClass or nullptr on failure.
// ----------------------------------------------------------------------------
PCXClass* PCX_LoadFile11(const char* pSrc);
