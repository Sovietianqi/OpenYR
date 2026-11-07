#include "BombListClass.h"
#include "../Core/Memory.h"

// ============================================================================
// BombListClass implementations
// ============================================================================

BombListClass::BombListClass() noexcept
    : ItemsRaw(nullptr)
    , Count(0)
    , Capacity(0)
    , Growth(10)            // original's default increment is 10 (init_vec)
    , TotalAdded(0)
    , IsFull(false)
{
}

// ----------------------------------------------------------------------------
// asm: BombListClass_AddDetector (0x439080)
// ----------------------------------------------------------------------------
void BombListClass::Add_Detector(TechnoClass* pTechno)
{
    // asm: cmp eax,[ecx+20h] / jl grow_path  -- capacity available?
    if (TotalAdded < Capacity) {
        // asm: grow_path entry
    } else {
        // asm: al = [esi+0Dh] / test al,al / jnz fail
        if (IsFull)
            return;                          // asm: jmp loc_4390C3 (fail)

        // asm: test ecx,ecx / jnz try_grow -- Capacity == 0 means never sized
        if (Capacity != 0) {
            // asm: eax = [esi+14h] / test eax,eax / jle fail
            if (Growth <= 0)
                return;                      // asm: jmp loc_4390C3 (fail)

            // asm: add eax, ecx ; push 0 ; push eax ; call [edx+8] (Grow)
            const int32 newCapacity = Count + Growth;
            TechnoClass** pNew = static_cast<TechnoClass**>(
                YRMemory::Allocate(sizeof(TechnoClass*) * newCapacity));
            if (pNew == nullptr) {
                IsFull = true;               // grow failed -> treat as full
                return;                      // asm: test al,al / jz fail
            }
            if (ItemsRaw != nullptr && Count > 0) {
                for (int32 i = 0; i < Count; ++i)
                    pNew[i] = ItemsRaw[i];
            }
            if (ItemsRaw != nullptr)
                YRMemory::Deallocate(ItemsRaw);
            ItemsRaw = pNew;
            Capacity = newCapacity;
        } else {
            // First allocation: the original calls the vector's grow with
            // (Count + Growth) which equals Growth when Count==0.
            if (Growth <= 0)
                return;
            ItemsRaw = static_cast<TechnoClass**>(
                YRMemory::Allocate(sizeof(TechnoClass*) * Growth));
            if (ItemsRaw == nullptr) {
                IsFull = true;
                return;
            }
            Capacity = Growth;
        }
    }

    // asm: eax = [esi+10h] / edx = [esi+4] / lea ecx,[eax+1]
    //      mov [esi+10h], ecx / mov ecx,[esp+techno]
    //      mov [edx+eax*4], ecx
    ItemsRaw[Count] = pTechno;
    ++Count;
    ++TotalAdded;                            // asm: (TotalAdded tracked at +28h)
}

// ----------------------------------------------------------------------------
// asm: BombListClass_RemoveDetector (0x4390C8)
// ----------------------------------------------------------------------------
void BombListClass::Remove_Detector(TechnoClass* pTechno)
{
    // asm: call [eax+10h] -- vector FindIndexOf(pTechno)
    int32 idx = -1;
    if (ItemsRaw != nullptr) {
        for (int32 i = 0; i < Count; ++i) {
            if (ItemsRaw[i] == pTechno) {
                idx = i;
                break;
            }
        }
    }

    // asm: cmp eax,0FFFFFFFFh / jz done
    if (idx == -1)
        return;

    // asm: mov ecx,[esi+10h] / cmp eax,ecx / jge done
    if (idx >= Count)
        return;

    // asm: dec ecx / cmp eax,ecx / mov [esi+10h],ecx / jge done
    const int32 newCount = Count - 1;
    Count = newCount;
    if (idx >= newCount)
        return;

    // asm: loc_4390F5 shift-down compaction:
    //   mov ecx,[esi+4] / inc eax / mov edx,[ecx+eax*4]
    //   mov [ecx+eax*4-4], edx / mov ecx,[esi+10h] / cmp eax,ecx / jl loop
    for (int32 i = idx; i < Count; ++i)
        ItemsRaw[i] = ItemsRaw[i + 1];
}
