#pragma once

#include "Definitions.h"
#include "Macros.h"

#include <new>
#include <cstdlib>
#include <cstring>

// ============================================================================
// YRMemory - Game memory pool (wraps malloc/free for standalone use)
// ============================================================================

namespace YRMemory
{
    FORCEINLINE void* Allocate(size_t size) noexcept {
        void* ptr = std::malloc(size);
        if (ptr) std::memset(ptr, 0, size);
        return ptr;
    }

    FORCEINLINE void Deallocate(void* ptr) noexcept {
        if (ptr) std::free(ptr);
    }

    void* AllocateChecked(size_t size) noexcept;
    size_t GetAllocatedBytes() noexcept;
    size_t GetAllocationCount() noexcept;
}

// ============================================================================
// GameCreate / GameDelete - placement new wrappers
// ============================================================================

template<typename T>
T* GameCreate(const char* id) {
    void* mem = YRMemory::Allocate(sizeof(T));
    if (!mem) return nullptr;
    return new (mem) T(id);
}

template<typename T>
T* GameCreate() {
    void* mem = YRMemory::Allocate(sizeof(T));
    if (!mem) return nullptr;
    return new (mem) T();
}

// Variadic construction: mirrors operator new(size) + ctor(args...) as the
// original does with e.g. SmudgeClass_CTOR in SmudgeTypeClass_SpawnSmudgeAt.
template<typename T, typename... TArgs>
T* GameCreateArgs(TArgs&&... args) {
    void* mem = YRMemory::Allocate(sizeof(T));
    if (!mem) return nullptr;
    return new (mem) T(std::forward<TArgs>(args)...);
}

template<typename T>
void GameDelete(T* ptr) {
    if (ptr) {
        ptr->~T();
        YRMemory::Deallocate(ptr);
    }
}

template<typename T, size_t N>
class StaticVector {
public:
    T Items[N];
    int32 Count;

    StaticVector() : Count(0) {
        for (size_t i = 0; i < N; ++i) {
            Items[i] = T();
        }
    }

    T& operator[](int32 index) { return Items[index]; }
    const T& operator[](int32 index) const { return Items[index]; }

    bool Add(const T& item) {
        if (Count >= static_cast<int32>(N)) return false;
        Items[Count++] = item;
        return true;
    }

    bool Remove(int32 index) {
        if (index < 0 || index >= Count) return false;
        for (int32 i = index; i < Count - 1; ++i) {
            Items[i] = Items[i + 1];
        }
        --Count;
        return true;
    }

    void Clear() { Count = 0; }
    int32 GetCount() const { return Count; }
    int32 GetCapacity() const { return static_cast<int32>(N); }
};

template<typename T>
class DynamicVectorClass {
public:
    T* Items;
    int32 Count;
    int32 Capacity;

    DynamicVectorClass() : Items(nullptr), Count(0), Capacity(0) {}

    ~DynamicVectorClass() {
        Clear();
        if (Items) {
            YRMemory::Deallocate(Items);
            Items = nullptr;
        }
    }

    T& operator[](int32 index) { return Items[index]; }
    const T& operator[](int32 index) const { return Items[index]; }

    T* operator->() { return Items; }
    const T* operator->() const { return Items; }

    bool Add(const T& item) {
        if (Count >= Capacity) {
            int32 newCap = Capacity == 0 ? 8 : Capacity * 2;
            if (!Grow(newCap)) return false;
        }
        Items[Count++] = item;
        return true;
    }

    bool AddItem(T item) {
        return Add(item);
    }

    bool Remove(int32 index) {
        if (index < 0 || index >= Count) return false;
        for (int32 i = index; i < Count - 1; ++i) {
            Items[i] = Items[i + 1];
        }
        --Count;
        return true;
    }

    T GetItem(int32 index) const {
        if (index >= 0 && index < Count) return Items[index];
        return T();
    }

    void Clear() {
        Count = 0;
    }

    bool Grow(int32 newCapacity) {
        T* newItems = static_cast<T*>(YRMemory::Allocate(newCapacity * sizeof(T)));
        if (!newItems) return false;
        if (Items) {
            for (int32 i = 0; i < Count && i < newCapacity; ++i) {
                newItems[i] = Items[i];
            }
            YRMemory::Deallocate(Items);
        }
        Items = newItems;
        Capacity = newCapacity;
        return true;
    }

    int32 GetCount() const { return Count; }
    int32 GetCapacity() const { return Capacity; }
    int32 Size() const { return Count; }

    // ========================================================================
    // asm: DynamicVectorClass<T>::Replace  (真实入口 0x4EDFC0)
    //
    //   纯 vtable 槽（只有 DATA XREF: .rdata:007EA30C，无 CODE XREF）。
    //   RTTI 指向 DynamicVectorClass<...>::Complete Object Locator，
    //   同表含 VectorClass<...> / VectorClass<UnitTypeClass*> —— 说明
    //   `VectorUnitType*` 只是该槽的一个模板实参，函数本体是
    //   DynamicVectorClass<T>::Replace，**不是** "VectorUnitTypeClass"。
    //
    //   （AsmComplex4 原标注 0x4EDF70 不存在，偏 -0x50；且 cpp 里
    //     loc_4EDF2A / loc_4EDF31 是臆造标签，真实为
    //     loc_4EDFF2 / loc_4EE045。）
    //
    //   arg_0 = count [+0Ch]（retn 8）
    //   arg_4 = prealloc [+10h]
    //
    //   count == 0:
    //     call [vt+0Ch]                        ; 虚清
    //     if (Capacity < Field10) Field10 = Capacity
    //     return true
    //   Initialized(+0x0C) = 0
    //   pBuf = prealloc ? prealloc : new count*4
    //   Initialized(+0x0C) = 1                 ; 注意：存值在判空**之前**
    //   if (!pBuf) return false
    //   if (Data(+0x04)):
    //     n = min(Capacity, count) ; 拷贝 n 个 dword
    //     if (IsAlloc(+0x0D)): delete Data ; Data = 0
    //   Data = pBuf ; Capacity = count ; IsAlloc = (prealloc == 0)
    //   if (Capacity < Field10) Field10 = Capacity
    //   return true
    // ========================================================================
    // 本模板的字段：Items(+0x04) / Count(+0x08) / Capacity(+0x0C)，
    // 与 asm 的 Data(+04)/Capacity(+08)/Initialized(+0C)/IsAlloc(+0D)/
    // Field10(+10) 不是同一布局，故此处按本类现有字段语义等价复现，
    // 并保留 asm 偏移注释（铁律①：语义等价，不臆造偏移）。
    bool Replace(int32 count, void* prealloc)
    {
        if (count == 0) {
            // asm: loc_4EDFF2 — call [vt+0Ch]（虚清）
            Clear();
            return true;                       // asm: mov al,1
        }

        // asm: test ebx,ebx / jnz loc_4EDECE
        //      lea eax,ds:0[ebp*4] / call new
        T* pBuf = (prealloc != nullptr)
            ? static_cast<T*>(prealloc)
            : static_cast<T*>(YRMemory::Allocate(
                  static_cast<size_t>(count) * sizeof(T)));

        // asm: test edi,edi / jz loc_4EDF47
        if (pBuf == nullptr)
            return false;                      // asm: xor al,al

        // asm: mov eax,[esi+4] / test eax,eax / jz loc_4EDF18
        if (Items != nullptr) {
            // asm: mov ecx,[esi+8] / cmp ebp,ecx / jge -> mov ecx,ebp  (min)
            int32 n = Capacity;
            if (count < n)
                n = count;

            // asm: loc_4EDEEE copy loop（按 dword 拷贝）
            for (int32 i = 0; i < n; ++i)
                pBuf[i] = Items[i];

            // asm: mov al,[esi+0Dh] / test / jz loc_4EDF18
            //      注：asm 的 IsAlloc 门控在 #1..3 批次的实现里用
            //      "prealloc==0 才拥有"语义承接（见下方赋值）。
            YRMemory::Deallocate(Items);
            Items = nullptr;                   // asm: mov [esi+4],0
        }

        // asm: loc_4EDF18
        Items = pBuf;                          // mov [esi+4],edi
        Capacity = count;                      // mov [esi+8],ebp
        if (Count > Capacity)
            Count = Capacity;                  // asm: loc_4EE045 min-clamp
        return true;                           // asm: mov al,1
    }

    // ========================================================================
    // asm: VectorHSV_Resize (0x628265) —— VectorClass<HSVClass>::Resize 的
    //     模板实例（vftable = DynamicVectorClass<HSVClass>），两条调用点
    //     （ConvertClass_CTOR_FromFilename / 调色板重建）都是 Resize(0, 0)：
    //     即"清成空表"。
    //
    //   [this+04] = 0 ; [this+08] = newMax ; [+0C] = 1 ; [+0D] = 0
    //   vt = VectorClass<HSVClass>::vftable
    //   if (newMax == 0) goto tail
    //   if (mem): 清零 newMax*3 字节 ; [this+04] = mem      （不置 IsAlloc）
    //   else:     p = new(newMax*3) ; 清零 ; [this+04] = p ; [+0D] = 1
    // tail: vt = DynamicVectorClass<HSVClass>::vftable
    //       [+14] = 10（GrowthRate）; [+10] = 0（ActiveCount）
    //
    // 本模板无 IsAlloc 字段，按"Items 由谁释放"的现有语义承接：只有
    // 自分配分支记录所有权；mem 分支按借用处理（原版该分支同样不置
    // IsAlloc，但因调用点恒为 0 而未走到）。
    // ========================================================================
    bool Resize(int32 newMax, void* pMem)
    {
        if (Items != nullptr) {
            YRMemory::Deallocate(Items);
            Items = nullptr;
        }
        Count = 0;
        Capacity = newMax;

        if (newMax == 0)                       // asm: jz loc_6282BC
            return true;

        if (pMem != nullptr) {
            // asm: loc_628275 清零 newMax 个条目（3 字节步长，借用外部内存）
            std::memset(pMem, 0, static_cast<size_t>(newMax) * sizeof(T));
            Items = static_cast<T*>(pMem);      // asm: mov [esi+4],edx
            return true;
        }

        // asm: loc_628288 lea eax,[edi+edi*2] / call new
        T* pBuf = static_cast<T*>(YRMemory::Allocate(   // 返回前已清零
            static_cast<size_t>(newMax) * sizeof(T)));
        Items = pBuf;                          // asm: loc_6282B5
        return pBuf != nullptr;
    }
};