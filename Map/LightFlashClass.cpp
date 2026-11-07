#include "LightFlashClass.h"

// ============================================================================
// LightFlashClass implementations
// ============================================================================

// asm: vec_SpotLights -- DynamicVectorClass<LightFlashClass*> in .data
static LightFlashClass** g_SpotLightsData     = nullptr;   // asm vec_SpotLights.data
static int32             g_SpotLightsCount    = 0;         // asm vec_SpotLights.Count
static int32             g_SpotLightsCapacity = 0;         // asm vec_SpotLights.Capacity

void SpotLights_Init()
{
    g_SpotLightsData     = nullptr;
    g_SpotLightsCount    = 0;
    g_SpotLightsCapacity = 0;
}

LightFlashClass** SpotLights_Data()
{
    return g_SpotLightsData;
}

int32 SpotLights_Count()
{
    return g_SpotLightsCount;
}

// ----------------------------------------------------------------------------
// Registration helper -- the original appends through the vector vtable
// ([edx+0Ch] Add path); reproduced with plain growth here.
// ----------------------------------------------------------------------------
void SpotLights_Add(LightFlashClass* pItem)
{
    if (g_SpotLightsCount >= g_SpotLightsCapacity) {
        const int32 newCap = (g_SpotLightsCapacity == 0) ? 10
                                                         : g_SpotLightsCapacity * 2;
        LightFlashClass** pNew = new LightFlashClass*[newCap];
        for (int32 i = 0; i < g_SpotLightsCount; ++i)
            pNew[i] = g_SpotLightsData[i];
        delete[] g_SpotLightsData;
        g_SpotLightsData     = pNew;
        g_SpotLightsCapacity = newCap;
    }
    g_SpotLightsData[g_SpotLightsCount++] = pItem;
}

int32 SpotLights_Find(const LightFlashClass* pItem)
{
    // asm: call [edx+10h] -- DynamicVectorClass::FindIndexOf
    for (int32 i = 0; i < g_SpotLightsCount; ++i) {
        if (g_SpotLightsData[i] == pItem)
            return i;
    }
    return -1;   // asm: mov eax,0FFFFFFFFh
}

// ============================================================================
// LightFlashClass_Unregister  (asm: LightFlashClass_Unregister, 0x5FF2D0)
// ============================================================================
void LightFlashClass::Unregister()
{
    // asm: push ecx (this) / call [edx+10h] (FindIndexOf)
    const int32 idx = SpotLights_Find(this);

    // asm: cmp eax,0FFFFFFFFh / jz done
    if (idx == -1)
        return;

    // asm: mov ecx, vec_SpotLights.Count / cmp eax,ecx / jge done
    if (idx >= g_SpotLightsCount)
        return;

    // asm: dec ecx / cmp eax,ecx / mov vec_SpotLights.Count, ecx / jge done
    const int32 newCount = g_SpotLightsCount - 1;
    g_SpotLightsCount = newCount;
    if (idx >= newCount)
        return;

    // asm: loc_5FF302 shift-down compaction
    //   mov ecx, vec_SpotLights.data / inc eax
    //   mov edx, [ecx+eax*4] / mov [ecx+eax*4-4], edx
    //   mov ecx, vec_SpotLights.Count / cmp eax,ecx / jl loop
    for (int32 i = idx; i < g_SpotLightsCount; ++i)
        g_SpotLightsData[i] = g_SpotLightsData[i + 1];
}
