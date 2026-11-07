#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Containers/DynamicVectorClass.h"

class TechnoClass;

// ============================================================================
// BombListClass - per-building tracker of attached demolition charges
// (C4 / bomb effects) so detectors can find and clear them.
//
//  Original layout recovered from BombListClass_AddDetector (0x439080):
//    +0x04  ObjectClass** Items   (dynamic vector data)
//    +0x0D  byte         IsFull   (accepts no more entries when nonzero)
//    +0x10  int32        Count
//    +0x18  vector head  (vtable at +0x18, used via [eax+8]/[eax+10h])
//    +0x20  int32        Capacity
//    +0x28  int32        TotalAdded (monotonic insert counter)
// ============================================================================

class BombListClass {
public:
    BombListClass() noexcept;

    // ------------------------------------------------------------------------
    // asm: BombListClass_AddDetector (0x439080)
    //
    //   eax = [ecx+28h] ; ecx = [ecx+20h]         ; TotalAdded vs Capacity
    //   cmp eax, ecx / jl grow_path               ; room left -> append
    //   al = [esi+0Dh] / test al,al / jnz fail    ; IsFull -> reject
    //   test ecx,ecx / jnz try_grow               ; Capacity>0 -> try grow
    //   eax = [esi+14h] / test / jle fail         ; Growth<=0 -> reject
    //   edx = [esi] (vtable) / eax += ecx         ; newCapacity = Count+Growth
    //   push 0 / push newCapacity / ecx=esi
    //   call [edx+8]                              ; vector Grow()
    //   test al,al / jz fail
    // grow_path:
    //   eax = [esi+10h] (Count) / edx = [esi+4] (Items)
    //   Items[Count] = techno ; ++Count
    // ------------------------------------------------------------------------
    void Add_Detector(TechnoClass* pTechno);

    // ------------------------------------------------------------------------
    // asm: BombListClass_RemoveDetector (0x4390C8)
    //
    //   eax = [ecx+18h] (vector head) / push &techno / ecx = esi (list)
    //   call [eax+10h]                            ; vector FindIndexOf
    //   cmp eax,-1 / jz done                      ; not found -> done
    //   ecx = [esi+10h] / cmp eax,ecx / jge done  ; idx >= Count -> done
    //   dec ecx / cmp eax,ecx / [esi+10h]=ecx
    //   jge done                                  ; idx == last -> done
    // shift:
    //   Items[i] = Items[i+1]  for i in [idx, Count)
    // ------------------------------------------------------------------------
    void Remove_Detector(TechnoClass* pTechno);

    int32 Get_Count() const { return Count; }

private:
    // The vector head mirrors the original's DynamicVectorClass<...> at +18h:
    // vtable at +0x18, Items at +0x1C, Count at +0x28... but the original
    // BombListClass embeds its own counters (Count at +10h, IsFull at +0Dh),
    // so we keep the public counters here and mirror the vector semantics
    // through DynamicVectorClass for storage.
    TechnoClass** ItemsRaw;   // asm +0x04
    int32         Count;      // asm +0x10
    int32         Capacity;   // asm +0x20
    int32         Growth;     // asm +0x14
    int32         TotalAdded; // asm +0x28
    bool          IsFull;     // asm +0x0D
};
