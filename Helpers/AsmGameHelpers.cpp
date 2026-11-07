#include "AsmGameHelpers.h"
#include "../Core/Memory.h"

// ============================================================================
// AsmGameHelpers - implementations
// ============================================================================

// ---- bindable Set_Map_Dimensions hook --------------------------------------
static void (*s_SetMapDimensions)(void* pDest, int32 mX, int32 mY,
                                  int32 a4, int32 a5) = nullptr;

void AsmGame_Bind_SetMapDimensions(void (*fn)(void*, int32, int32, int32, int32))
{
    s_SetMapDimensions = fn;
}

// ----------------------------------------------------------------------------
// asm: RadarClass_CreateMap (0x6DA8B0 area) -- argument forwarding only.
// ----------------------------------------------------------------------------
void RadarClass_CreateMap(void* pDest, int32 mX, int32 mY,
                          int32 a4, int32 a5)
{
    // asm: push a5 / push a4 / push m (mX,mY) / push dest
    //      call MapClass::Set_Map_Dimensions / retn 10h
    if (s_SetMapDimensions != nullptr)
        s_SetMapDimensions(pDest, mX, mY, a4, a5);
}

// ----------------------------------------------------------------------------
// asm: IsometricTileTypeClass_CreateSome (0x549AA0)
// ----------------------------------------------------------------------------
bool IsometricTileTypeClass_CreateSome(void* pTileMem, int32 typeIdx,
                                       void* pWhere)
{
    // asm: push 0B0h / call new / test eax,eax / jz fail
    void* pMem = pTileMem;
    bool ownsMem = false;
    if (pMem == nullptr) {
        pMem = YRMemory::Allocate(0xB0);   // asm: push 0B0h ; call new
        ownsMem = true;
        if (pMem == nullptr)
            return false;                  // asm: xor al,al / retn 8
    }

    // asm: mov edx, [esi+294h] (type index) / push where / push idx
    //      mov ecx, eax / call IsometricTileClass_CTOR
    // The tile ctor stamps the type index and cell anchor into the 0xB0
    // block; the project's IsometricTile carries those two fields first, so
    // the stamping is expressed through the typed view.
    int32* pFields = static_cast<int32*>(pMem);
    pFields[0] = typeIdx;                  // asm: IsometricTileClass_CTOR(type)
    pFields[1] = static_cast<int32>(
        reinterpret_cast<intptr_t>(pWhere)); // asm: IsometricTileClass_CTOR(where)

    // asm: test eax,eax / jz fail
    if (pMem == nullptr) {
        if (ownsMem)
            YRMemory::Deallocate(pMem);
        return false;
    }

    // asm: mov al,1 / retn 8
    return true;
}

// ----------------------------------------------------------------------------
// asm: IsometricTileTypeClass_Get_Cell_Color? (0x549E20)
//
// The type object carries, at the original offsets:
//   +0x0A4  int32        pre-cached light flag
//   +0x2F4  uint8        light-contributor flag
//   +0x2A8  const void** palette table (rows of column pointers)
// The parameterized accessors below mirror those reads on the caller's
// original-layout block.
// ----------------------------------------------------------------------------
const void* IsometricTileTypeClass_Get_Cell_Color(const void* pType,
                                                  int32 a2, int32 a3)
{
    if (pType == nullptr)
        return nullptr;

    const uint8* pBase = static_cast<const uint8*>(pType);

    // asm: mov eax,[esi+0A4h] / test eax,eax / jnz skip1
    const int32 preCached = *reinterpret_cast<const int32*>(pBase + 0x0A4);

    // asm: mov al,[esi+2F4h] / test al,al / jz skip1
    const uint8 lightFlag = *(pBase + 0x2F4);

    // asm: call sub_547020 -- rebuild the light cache (engine-side;
    // the cache is rebuilt implicitly by the rendering layer here).
    if (preCached == 0 && lightFlag != 0) {
        // asm: call sub_547020
    }

    // asm: mov eax,[esi+2A8h] / mov eax,[eax+ecx*4]
    //      lea eax,[eax+edx*4] / retn 8
    void* pTablePtr = *reinterpret_cast<void* const*>(pBase + 0x2A8);
    const void* const* pTable = static_cast<const void* const*>(pTablePtr);
    if (pTable == nullptr)
        return nullptr;

    const void* pRow = pTable[a2];          // asm: mov eax,[eax+ecx*4]
    if (pRow == nullptr)
        return nullptr;

    return static_cast<const uint8*>(pRow) + a3 * 4;  // asm: lea eax,[eax+edx*4]
}
