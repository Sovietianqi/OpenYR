#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Core/Memory.h"

#include <cstring>
#include <functional>
#include <utility>

//========================================================================
// PriorityQueueClass<T, Pr>
//
// A heap-based priority queue matching the original game's implementation.
// The game uses this for pathfinding (A*), AI decision-making, and other
// ordered processing tasks.
//
// Template parameters:
//   T  - The element type stored as pointers
//   Pr - Comparison predicate (default: std::less<T> for min-heap)
//
// Binary layout:
//   Capacity, Count, Nodes, LMost, RMost
//========================================================================

template <typename T, typename Pr = std::less<T>>
class PriorityQueueClass
{
public:
    //========================================================================
    // Construction / Destruction
    //========================================================================

    PriorityQueueClass() noexcept
        : Capacity(0)
        , Count(0)
        , Nodes(nullptr)
        , LMost(nullptr)
        , RMost(nullptr)
    {
        LMost = reinterpret_cast<T*>(0);
        RMost = reinterpret_cast<T*>(static_cast<uintptr_t>(0xFFFFFFFF));
    }

    explicit PriorityQueueClass(int32 capacity) noexcept
        : Capacity(capacity)
        , Count(0)
        , Nodes(nullptr)
        , LMost(nullptr)
        , RMost(nullptr)
    {
        // Allocate Nodes array with one extra slot for 1-based indexing
        Nodes = static_cast<T**>(YRMemory::Allocate(sizeof(T*) * static_cast<size_t>(capacity + 1)));

        if (Nodes)
        {
            ClearAll();
        }
        else
        {
            Capacity = 0;
        }

        // Sentinel values for range tracking
        LMost = reinterpret_cast<T*>(0);
        RMost = reinterpret_cast<T*>(static_cast<uintptr_t>(0xFFFFFFFF));
    }

    ~PriorityQueueClass() noexcept
    {
        if (Nodes)
        {
            YRMemory::Deallocate(Nodes);
            Nodes = nullptr;
        }
    }

    // Non-copyable, non-movable (matches original game behavior)
    PriorityQueueClass(const PriorityQueueClass&) = delete;
    PriorityQueueClass& operator=(const PriorityQueueClass&) = delete;
    PriorityQueueClass(PriorityQueueClass&&) = delete;
    PriorityQueueClass& operator=(PriorityQueueClass&&) = delete;

    //========================================================================
    // Public interface
    //========================================================================

    void Clear() noexcept
    {
        if (Nodes)
            std::memset(Nodes, 0, sizeof(T*) * static_cast<size_t>(Count + 1));
        Count = 0;
    }

    T* Top() const noexcept
    {
        return (Count == 0 || !Nodes) ? nullptr : Nodes[1];
    }

    bool Empty() const noexcept
    {
        return Count == 0;
    }

    //========================================================================
    // Push operation
    //========================================================================

    bool Push(T* pValue) noexcept
    {
        if (!Nodes || Count >= Capacity)
            return false;

        if (!pValue)
            return false;

        Nodes[++Count] = pValue;
        int32 now = Count;

        while (now != 1)
        {
            int32 next = now / 2;

            if (!Comp(Nodes[now], Nodes[next]))
                break;

            std::swap(Nodes[now], Nodes[next]);
            now = next;
        }

        return true;
    }

    //========================================================================
    // Pop operation
    //========================================================================

    bool Pop() noexcept
    {
        if (Count == 0 || !Nodes)
            return false;

        Nodes[1] = Nodes[Count--];
        int32 now = 1;

        while (now * 2 <= Count)
        {
            int32 next = now * 2;

            if (next < Count && Comp(Nodes[next + 1], Nodes[next]))
                ++next;

            if (Comp(Nodes[now], Nodes[next]))
                break;

            std::swap(Nodes[now], Nodes[next]);
            now = next;
        }

        return true;
    }

    //========================================================================
    // Westwood-style push/pop with pointer range tracking
    // Used by the original game for debugging / validation
    //========================================================================

    bool WWPush(T* pValue) noexcept
    {
        if (Push(pValue))
        {
            WWPointerUpdate(pValue);
            return true;
        }
        return false;
    }

    bool WWPop() noexcept
    {
        if (Pop())
        {
            for (int32 i = 1; i <= Count; ++i)
                WWPointerUpdate(Nodes[i]);
            return true;
        }
        return false;
    }

    //========================================================================
    // Data members
    //========================================================================

    int32 Capacity;
    int32 Count;
    T** Nodes;
    T* LMost;
    T* RMost;

private:
    //========================================================================
    // Internal helpers
    //========================================================================

    void ClearAll() noexcept
    {
        if (Nodes)
            std::memset(Nodes, 0, sizeof(T*) * static_cast<size_t>(Capacity + 1));
        Count = 0;
    }

    bool Comp(T* p1, T* p2) const noexcept
    {
        return Pr()(*p1, *p2);
    }

    void WWPointerUpdate(T* pValue) noexcept
    {
        if (pValue > RMost)
            RMost = pValue;
        if (pValue < LMost)
            LMost = pValue;
    }
};

//========================================================================
// Validate binary layout (32-bit x86, matching original game)
// On 32-bit: Capacity(4) + Count(4) + Nodes(4) + LMost(4) + RMost(4) = 20
//========================================================================
#ifdef __i386__
static_assert(sizeof(PriorityQueueClass<int32>) == 20,
    "PriorityQueueClass layout must match original 32-bit game binary");
#endif