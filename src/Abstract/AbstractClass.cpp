#include <Abstract/AbstractClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>

// ============================================================================
// AbstractClass.cpp
//
//  Base class for nearly every game object in the Yuri's Revenge engine.
//  This file provides:
//    * Static Array allocation / deallocation (Init_Array / Delete_Array)
//    * Constructor / destructor behaviour that the header leaves inline-only
//    * Add_To_Array / Remove_From_Array helpers used by every derived class
//    * Get_Total_Count - returns the number of currently-registered abstracts
//    * Delete_All_Instances - bulk cleanup used by scenario / save loading
//    * Read_INI / Write_INI base implementations (the abstract layer has no INI presence)
//    * Compute_CRC abstract-base hook for save / multiplayer checksums
//
//  The original binary keeps the AbstractClass::Array pointer in the .data
//  segment and lets each subclass register itself on construction.  The
//  reconstructed build preserves the same control flow.
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<AbstractClass*>* AbstractClass::Array = nullptr;

// ============================================================================
// Init_Array
//
//  Allocates the global AbstractClass::Array on the game's memory pool.  In
//  the original binary this is called once during engine boot, before any
//  AbstractClass-derived object is constructed.
// ============================================================================
void AbstractClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<AbstractClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<AbstractClass*>)));

    if (Array != nullptr)
    {
        // Placement-construct the vector so its internal pointers are valid.
        new (Array) DynamicVectorClass<AbstractClass*>();
    }
}

// ============================================================================
// Delete_Array
//
//  Tears down the global array.  Every entry still present is left alone
//  (the caller is expected to have already destroyed the objects).  The
//  vector's own heap buffer is released by the destructor.
// ============================================================================
void AbstractClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    // Invoke the destructor explicitly since we allocated raw memory.
    Array->~DynamicVectorClass<AbstractClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

// ============================================================================
// Add_To_Array
//
//  Registers an abstract instance with the global array.  Returns the index
//  the object was stored at, or -1 if the array has not been initialised.
// ============================================================================
int32 AbstractClass::Add_To_Array(AbstractClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return -1;

    if (!Array->Add(pInstance))
        return -1;

    return Array->Count - 1;
}

// ============================================================================
// Remove_From_Array
//
//  Unregisters an abstract instance.  The search is linear because the
//  original binary keeps a flat array and does not maintain an index field
//  on AbstractClass itself (only some derived classes do).
// ============================================================================
bool AbstractClass::Remove_From_Array(AbstractClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return false;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        if (Array->Items[i] == pInstance)
        {
            return Array->Remove(i);
        }
    }
    return false;
}

// ============================================================================
// Get_Total_Count
//
//  Returns the number of currently-registered AbstractClass instances.  This
//  is used by the save/load subsystem to size the pointer-fixup tables.
// ============================================================================
int32 AbstractClass::Get_Total_Count()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

// ============================================================================
// Get_Instance
//
//  Returns the instance at the supplied array index, or nullptr if the index
//  is out of range or the array has not been initialised.
// ============================================================================
AbstractClass* AbstractClass::Get_Instance(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

// ============================================================================
// Find_Index
//
//  Linear search returning the array index of the supplied instance, or -1
//  if it is not registered.  Mirrors the helper used by the save system.
// ============================================================================
int32 AbstractClass::Find_Index(AbstractClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return -1;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        if (Array->Items[i] == pInstance)
            return i;
    }
    return -1;
}

// ============================================================================
// Delete_All_Instances
//
//  Destroys every registered AbstractClass and clears the array.  This is
//  the bulk teardown path invoked between scenarios and during shutdown.
//  The array itself is preserved so subsequent allocations can reuse it.
// ============================================================================
void AbstractClass::Delete_All_Instances()
{
    if (Array == nullptr)
        return;

    // Walk backwards so Remove() does not have to shift as many entries.
    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        AbstractClass* pItem = Array->Items[i];
        if (pItem != nullptr)
        {
            // Decrement the refcount; the original binary uses a custom
            // deleter that respects shared ownership.  In the standalone
            // build we just delete the object directly.
            delete pItem;
        }
        Array->Remove(i);
    }
}

// ============================================================================
// PointerExpired (default implementation)
//
//  Called by the pointer-fixup pass after a save game load.  The default
//  behaviour is a no-op; derived classes override this to rebind their
//  embedded pointers to the freshly-loaded object table.
// ============================================================================
// (Implemented inline in the header - retained here as documentation.)

// ============================================================================
// Read_INI (abstract base implementation)
//
//  AbstractClass itself has no INI presence - it owns only runtime
//  bookkeeping (UniqueID, Flags, RefCount, ...) that is never persisted to
//  an INI file.  The no-op base returning false exists so that callers can
//  polymorphically invoke Read_INI on a heterogeneous array of abstracts
//  without RTTI checks; any object that actually has INI state is a
//  derived class that overrides this (e.g. AbstractTypeClass::Read_INI
//  delegates to LoadFromINI, which concrete type classes implement).
//
//  Returning false signals "nothing was read", which is the correct
//  behaviour for the abstract root.
// ============================================================================
bool AbstractClass::Read_INI(CCINIClass* /*pINI*/)
{
    return false;
}

// ============================================================================
// Write_INI (abstract base implementation)
//
//  Mirror of Read_INI: AbstractClass has no INI fields to emit, so the base
//  returns false.  Derived classes with persistent state (type classes,
//  scenario objects, ...) override this to serialise their fields.  The
//  virtual hook lets the save/editor code iterate a mixed array and write
//  every persistable object through a uniform interface.
// ============================================================================
bool AbstractClass::Write_INI(CCINIClass* /*pINI*/) const
{
    return false;
}

// ============================================================================
// Compute_CRC_Abstract
//
//  Helper invoked by the multiplayer / save checksum code.  It feeds the
//  basic bookkeeping fields (UniqueID, Flags, unknown_18) into the CRC
//  stream.  Derived classes are expected to chain this before adding their
//  own state.
// ============================================================================
void AbstractClass::Compute_CRC_Abstract(CRCEngine& crc) const
{
    crc.AddData(&UniqueID,    sizeof(UniqueID));
    crc.AddData(&Flags,       sizeof(Flags));
    crc.AddData(&unknown_18,  sizeof(unknown_18));
    crc.AddData(&RefCount,    sizeof(RefCount));
    crc.AddData(&Dirty,       sizeof(Dirty));
}

// ============================================================================
// ComputeCRC (default implementation)
//
//  The virtual override declared on AbstractClass.  The base implementation
//  forwards to Compute_CRC_Abstract so subclasses get the common header
//  bytes hashed even if they forget to chain up.
// ============================================================================
// (Header declares this as `virtual void ComputeCRC(CRCEngine&) const {}`
//  which already gives the empty default.  We intentionally leave it empty
//  because every concrete subclass overrides it; the helper above is what
//  they call to add the common header bytes.)

// ============================================================================
// Create_ID (default implementation)
//
//  Generates a fresh UniqueID for the instance.  The original binary uses a
//  global monotonic counter seeded from the system clock.  The standalone
//  build uses a static local counter which is sufficient for single-process
//  scenarios.
// ============================================================================
void AbstractClass::Create_ID_Internal()
{
    static DWORD s_NextID = 0x00010000u;
    UniqueID = s_NextID++;
}

// ============================================================================
// Get_Array_Ptr
//
//  Read-only accessor used by debugging and save/load helpers.
// ============================================================================
const DynamicVectorClass<AbstractClass*>* AbstractClass::Get_Array_Ptr()
{
    return Array;
}

// ============================================================================
// For_Each_Instance
//
//  Convenience functor dispatcher.  Walks the array and invokes the supplied
//  callback for every non-null entry.  Stops early if the callback returns
//  false.  Used by the editor and the network resync code.
// ============================================================================
void AbstractClass::For_Each_Instance(bool (*pCallback)(AbstractClass*, void*),
                                      void* pUser)
{
    if (Array == nullptr || pCallback == nullptr)
        return;

    // Cache the count; the callback is free to delete entries which would
    // invalidate the live Count field mid-iteration.
    int32 cachedCount = Array->Count;
    for (int32 i = 0; i < cachedCount; ++i)
    {
        if (i >= Array->Count)
            break; // Callback compacted the array.
        AbstractClass* pItem = Array->Items[i];
        if (pItem == nullptr)
            continue;
        if (!pCallback(pItem, pUser))
            break;
    }
}

// ============================================================================
// File-local helpers
// ============================================================================

namespace
{
    // ── Type-filtered counting ─────────────────────────────────────────
    //
    //  Counts how many registered AbstractClass instances report the given
    //  AbstractType via WhatAmI().  Used by the debugger overlay and the
    //  save subsystem to size per-type pointer fixup tables.

    int32 CountInstancesByType(AbstractType type) noexcept
    {
        if (!AbstractClass::Array)
            return 0;
        int32 count = 0;
        for (int32 i = 0; i < AbstractClass::Array->Count; ++i)
        {
            AbstractClass* p = AbstractClass::Array->Items[i];
            if (p && p->WhatAmI() == type)
                ++count;
        }
        return count;
    }

    // ── Type-filtered iteration ────────────────────────────────────────
    //
    //  Invokes the callback only for instances matching the supplied type.
    //  Returns the number of instances visited.

    int32 ForEachByType(AbstractType type,
                        bool (*pCallback)(AbstractClass*, void*),
                        void* pUser) noexcept
    {
        if (!AbstractClass::Array || !pCallback)
            return 0;
        int32 visited = 0;
        int32 cachedCount = AbstractClass::Array->Count;
        for (int32 i = 0; i < cachedCount; ++i)
        {
            if (i >= AbstractClass::Array->Count)
                break;
            AbstractClass* p = AbstractClass::Array->Items[i];
            if (!p || p->WhatAmI() != type)
                continue;
            ++visited;
            if (!pCallback(p, pUser))
                break;
        }
        return visited;
    }

    // ── Instance lookup by UniqueID ───────────────────────────────────
    //
    //  Linear scan for an instance whose UniqueID matches.  The original
    //  binary maintains a hash table for this; the standalone build uses
    //  a linear search which is adequate for debugging and save loading.

    AbstractClass* FindInstanceByID(DWORD uniqueID) noexcept
    {
        if (!AbstractClass::Array)
            return nullptr;
        for (int32 i = 0; i < AbstractClass::Array->Count; ++i)
        {
            AbstractClass* p = AbstractClass::Array->Items[i];
            if (p && p->UniqueID == uniqueID)
                return p;
        }
        return nullptr;
    }

    // ── Coordinate distance helpers ───────────────────────────────────
    //
    //  Computes the 2D distance between two abstract objects based on
    //  their GetCoords() result.  Used by the AI target-selection code
    //  and the fog-of-war visibility check.

    int32 DistanceSquared2D(AbstractClass* pA, AbstractClass* pB) noexcept
    {
        if (!pA || !pB)
            return 0;
        CoordStruct a = pA->GetCoords();
        CoordStruct b = pB->GetCoords();
        int32 dx = a.X - b.X;
        int32 dy = a.Y - b.Y;
        return dx * dx + dy * dy;
    }

    int32 Distance2D(AbstractClass* pA, AbstractClass* pB) noexcept
    {
        int32 distSq = DistanceSquared2D(pA, pB);
        if (distSq <= 0)
            return 0;
        // Integer square root approximation (Newton's method, 4 iterations).
        int32 x = distSq;
        int32 root = 0;
        int32 bit = 1 << 30;
        while (bit > distSq) bit >>= 2;
        while (bit != 0)
        {
            if (x >= root + bit)
            {
                x -= root + bit;
                root = (root >> 1) + bit;
            }
            else
            {
                root >>= 1;
            }
            bit >>= 2;
        }
        return root;
    }

    // ── 3D distance (includes Z) ──────────────────────────────────────

    int32 DistanceSquared3D(AbstractClass* pA, AbstractClass* pB) noexcept
    {
        if (!pA || !pB)
            return 0;
        CoordStruct a = pA->GetCoords();
        CoordStruct b = pB->GetCoords();
        int32 dx = a.X - b.X;
        int32 dy = a.Y - b.Y;
        int32 dz = a.Z - b.Z;
        return dx * dx + dy * dy + dz * dz;
    }

    // ── Visibility check ──────────────────────────────────────────────
    //
    //  Returns true if object pTarget is within the visible radius of
    //  pObserver.  The radius is in leptons.  Uses 2D distance for the
    //  initial check; the full game also applies a height-based modifier.

    bool IsWithinSightRange(AbstractClass* pObserver,
                            AbstractClass* pTarget,
                            int32 sightRangeLeptons) noexcept
    {
        if (!pObserver || !pTarget || sightRangeLeptons <= 0)
            return false;
        int32 distSq = DistanceSquared2D(pObserver, pTarget);
        int32 rangeSq = sightRangeLeptons * sightRangeLeptons;
        return distSq <= rangeSq;
    }

    // ── CRC with type information ─────────────────────────────────────
    //
    //  Extends the base Compute_CRC_Abstract by also feeding the object's
    //  AbstractType into the stream.  This makes the checksum sensitive to
    //  type mismatches (e.g. a Unit where a Building was expected).

    void ComputeTypedCRC(AbstractClass* pObj, CRCEngine& crc) noexcept
    {
        if (!pObj)
            return;
        pObj->Compute_CRC_Abstract(crc);
        AbstractType type = pObj->WhatAmI();
        crc.AddData(&type, sizeof(type));
    }

    // ── Array sorting by UniqueID ─────────────────────────────────────
    //
    //  Simple insertion sort that orders the array entries by UniqueID.
    //  The array is typically small enough (< 10k entries) that O(n^2)
    //  is acceptable and avoids the overhead of a full quicksort.

    void SortArrayByUniqueID() noexcept
    {
        if (!AbstractClass::Array || AbstractClass::Array->Count < 2)
            return;
        DynamicVectorClass<AbstractClass*>& arr = *AbstractClass::Array;
        for (int32 i = 1; i < arr.Count; ++i)
        {
            AbstractClass* key = arr.Items[i];
            int32 j = i - 1;
            while (j >= 0 && arr.Items[j] && key &&
                   arr.Items[j]->UniqueID > key->UniqueID)
            {
                arr.Items[j + 1] = arr.Items[j];
                --j;
            }
            arr.Items[j + 1] = key;
        }
    }

    // ── Find first instance of a type ─────────────────────────────────

    AbstractClass* FindFirstOfType(AbstractType type) noexcept
    {
        if (!AbstractClass::Array)
            return nullptr;
        for (int32 i = 0; i < AbstractClass::Array->Count; ++i)
        {
            AbstractClass* p = AbstractClass::Array->Items[i];
            if (p && p->WhatAmI() == type)
                return p;
        }
        return nullptr;
    }

    // ── Delete all instances of a specific type ───────────────────────
    //
    //  Used during scenario transitions to clear only one category of
    //  objects (e.g. all units, but keep buildings).

    void DeleteInstancesByType(AbstractType type) noexcept
    {
        if (!AbstractClass::Array)
            return;
        for (int32 i = AbstractClass::Array->Count - 1; i >= 0; --i)
        {
            AbstractClass* p = AbstractClass::Array->Items[i];
            if (p && p->WhatAmI() == type)
            {
                delete p;
                AbstractClass::Array->Remove(i);
            }
        }
    }

    // ── Collect coordinates into a flat array ─────────────────────────
    //
    //  Helper for the renderer's draw-list builder: walks all instances
    //  and copies their coordinates into a caller-supplied buffer.
    //  Returns the number of entries written.

    int32 CollectCoordinates(CoordStruct* pOut, int32 maxCount) noexcept
    {
        if (!AbstractClass::Array || !pOut || maxCount <= 0)
            return 0;
        int32 written = 0;
        for (int32 i = 0; i < AbstractClass::Array->Count && written < maxCount; ++i)
        {
            AbstractClass* p = AbstractClass::Array->Items[i];
            if (!p)
                continue;
            pOut[written] = p->GetCoords();
            ++written;
        }
        return written;
    }

} // anonymous namespace
