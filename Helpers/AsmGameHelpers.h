#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// AsmGameHelpers - remaining original-layout gameplay helpers recovered from
// the binary (PlanningToken, PixelFX, RadarClass map sizing, isometric tile
// type factories).  Each function cites the asm label it mirrors.
// ============================================================================

// ----------------------------------------------------------------------------
// asm: PlanningToken_GetNodeCount (0x6F0A50 area)
//   mov eax, [ecx+14h] / retn
// The planning token keeps its node count at +14h; parameterized here.
// ----------------------------------------------------------------------------
inline int32 PlanningToken_GetNodeCount(const void* pToken)
{
    if (pToken == nullptr)
        return 0;
    return *reinterpret_cast<const int32*>(
        static_cast<const uint8*>(pToken) + 0x14);      // asm: mov eax,[ecx+14h]
}

// ----------------------------------------------------------------------------
// PixelFXClass - the per-frame twinkle effect cell (asm 0x631F10 / 0x631EE0)
//   +0x30  int32 X     (Set_Something arg_0)
//   +0x34  int32 Y     (Set_Something arg_4)
//   +0x38  int32 Life  (Is_Something decrements; elapsed counter)
// ----------------------------------------------------------------------------
class PixelFXClass {
public:
    // asm: PixelFXClass_Set_Something_631F10
    //   mov eax,[esp+arg_0] / mov edx,[esp+arg_4]
    //   mov [ecx+30h],eax / mov [ecx+34h],edx / retn 8
    void Set_Something(int32 a, int32 b)
    {
        Field_30 = a;                    // asm: mov [ecx+30h],eax
        Field_34 = b;                    // asm: mov [ecx+34h],edx
    }

    // asm: PixelFXClass_Is_Something_631EE0
    //   eax = [esp+arg_0] / edx = [ecx+38h] / sub edx,eax
    //   [ecx+38h] = edx / test eax,eax / jg keep
    //   mov dword ptr [ecx+38h],0 / mov al,1 / retn    ; expired
    // keep: xor al,al / retn                          ; still alive
    bool Is_Something(int32 delta)
    {
        const int32 remaining = Field_38 - delta;        // asm: sub edx,eax
        Field_38 = remaining;                            // asm: mov [ecx+38h],edx
        if (remaining > 0)                               // asm: test/jg
            return false;                                // asm: xor al,al
        Field_38 = 0;                                    // asm: mov [ecx+38h],0
        return true;                                     // asm: mov al,1
    }

    int32 Field_30;   // asm +0x30
    int32 Field_34;   // asm +0x34
    int32 Field_38;   // asm +0x38
};

// ----------------------------------------------------------------------------
// asm: RadarClass_CreateMap (0x6DA8B0 area)
//
//   eax = dword [esp+a5] / edx = dword [esp+a4]
//   push a5 / push a4
//   eax = dword [esp+m.X] / edx = [esp+dest]
//   push m (2 dwords) / push dest
//   call MapClass::Set_Map_Dimensions / retn 10h
//
// Pure argument forwarding to the map sizing entry -- reproduced 1:1.
// ----------------------------------------------------------------------------
void RadarClass_CreateMap(void* pDest, int32 mX, int32 mY,
                          int32 a4, int32 a5);

// ----------------------------------------------------------------------------
// asm: MapClass::Set_Map_Dimensions -- the sizing entry RadarClass_CreateMap
// forwards to.  Engine-side on the original; modeled as a bindable hook with
// a local recorder so call flow stays observable.
// ----------------------------------------------------------------------------
void AsmGame_Bind_SetMapDimensions(void (*fn)(void* pDest, int32 mX, int32 mY,
                                              int32 a4, int32 a5));

// ----------------------------------------------------------------------------
// asm: IsometricTileTypeClass_CreateSome (0x549AA0)
//
//   push 0B0h / call new            ; sizeof(IsometricTileClass) == 0xB0
//   test eax,eax / jz fail
//   ecx = [esp+where] / edx = [esi+294h]   ; type index
//   push where / push idx / ecx = eax
//   call IsometricTileClass_CTOR
//   test eax,eax / jz fail
//   mov al,1 / retn 8
// fail: xor al,al / retn 8
//
// `pTypeBase` plays the role of `this` with the original's +294h type-index
// slot; `pTileBase` the 0xB0 tile memory when the caller owns it (null here
// allocates through the project heap).
// ----------------------------------------------------------------------------
bool IsometricTileTypeClass_CreateSome(void* pTileMem, int32 typeIdx,
                                       void* pWhere);

// ----------------------------------------------------------------------------
// asm: IsometricTileTypeClass_Get_Cell_Color? (0x549E20)
//
//   esi = this
//   eax = [esi+0A4h] / test / jnz skip1      ; pre-cached set?
//   al = [esi+2F4h] / test / jz skip1        ; light contributor?
//   call sub_547020                           ; rebuild light cache
// skip1:
//   eax = [esi+2A8h]                          ; palette table
//   ecx = [esp+a2] / edx = [esp+a3]
//   eax = [eax+ecx*4] / lea eax, [eax+edx*4]  ; row + column offset
//   retn 8
// ----------------------------------------------------------------------------
const void* IsometricTileTypeClass_Get_Cell_Color(const void* pType,
                                                  int32 a2, int32 a3);

// ----------------------------------------------------------------------------
// asm: IsometricTileTypeClass_ReturnFalse (0x549E60 area) -- xor al,al / retn
// ----------------------------------------------------------------------------
inline bool IsometricTileTypeClass_ReturnFalse()
{
    return false;
}
