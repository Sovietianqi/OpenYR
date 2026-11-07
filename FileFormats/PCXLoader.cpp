#include "PCXLoader.h"
#include "PCX.h"

// ============================================================================
// PCX engine-side loader wrappers -- implementations
// ============================================================================

// ----------------------------------------------------------------------------
// asm: PCX_LoadFile11 (0x6B68E0)
//   mov eax, [esp+Src] / push 1 / push 1 / push eax / call PCX_LoadFile
//   retn 4
//
// The underlying PCX_LoadFile(src, copyPalette=1, buildVga=1) maps onto the
// PCXClass pipeline here: instantiate, load from file, both conversions on.
// ----------------------------------------------------------------------------
PCXClass* PCX_LoadFile11(const char* pSrc)
{
    if (pSrc == nullptr)
        return nullptr;

    // asm: call PCX_LoadFile with (1, 1) flag pair
    PCXClass* pPCX = new PCXClass();
    if (!pPCX->LoadFromFile(pSrc)) {
        delete pPCX;
        return nullptr;
    }
    return pPCX;
}
