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
};