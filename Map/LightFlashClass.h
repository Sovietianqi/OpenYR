#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// LightFlashClass - transient bright flash (nukes, ion storms) registered in
// the vec_SpotLights dynamic vector.  Original layout keeps the vector as a
// classic DynamicVectorClass {vtable, data, count, capacity} and Unregister
// removes this pointer with the standard shift-down compaction.
// ============================================================================

class LightFlashClass {
public:
    LightFlashClass() noexcept : IsRegistered(false) {}

    // ------------------------------------------------------------------------
    // asm: LightFlashClass_Unregister (0x5FF2D0)
    //
    //   push ecx (this)
    //   edx = vec_SpotLights.vt / eax = &local(this)
    //   ecx = offset vec_SpotLights
    //   call [edx+10h]               ; FindIndexOf(this)
    //   cmp eax,0FFFFFFFFh / jz done
    //   ecx = vec_SpotLights.Count
    //   cmp eax,ecx / jge done       ; idx >= Count -> nothing to do
    //   dec ecx / cmp eax,ecx / mov Count,ecx
    //   jge done                     ; idx == last -> just Count--
    // loop_5FF302:
    //   data[i] = data[i+1]  while (i < Count)
    // ------------------------------------------------------------------------
    void Unregister();

    bool IsRegistered;
};

// ----------------------------------------------------------------------------
// asm global: vec_SpotLights -- {LightFlashClass** data; int32 count;
// int32 capacity}.  Exposed here so building/weapon code can register.
// ----------------------------------------------------------------------------
void           SpotLights_Init();
int32          SpotLights_Find(const LightFlashClass* pItem);
void           SpotLights_Add(LightFlashClass* pItem);
LightFlashClass** SpotLights_Data();
int32          SpotLights_Count();
