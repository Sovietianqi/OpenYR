#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

template<typename T>
class VectorClass {
public:
    T* Items;
    int32 Count;
    int32 Capacity;

    VectorClass() : Items(nullptr), Count(0), Capacity(0) {}
    ~VectorClass() {
        if (Items) {
            std::free(Items);
            Items = nullptr;
        }
    }

    T& operator[](int32 index) { return Items[index]; }
    const T& operator[](int32 index) const { return Items[index]; }

    bool Add(const T& item) {
        if (Count >= Capacity) {
            int32 newCap = Capacity == 0 ? 8 : Capacity * 2;
            T* newItems = static_cast<T*>(std::realloc(Items, newCap * sizeof(T)));
            if (!newItems) return false;
            Items = newItems;
            Capacity = newCap;
        }
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
    int32 GetCapacity() const { return Capacity; }
};