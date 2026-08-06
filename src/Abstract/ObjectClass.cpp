#include <Abstract/ObjectClass.h>

#include <Houses/HouseClass.h>
#include <Core/Memory.h>
#include <Core/Macros.h>

// ============================================================================
// ObjectClass.cpp
//
//  ObjectClass sits below TechnoClass in the inheritance tree and represents
//  anything that can physically exist on the game map - buildings, infantry,
//  vehicles, aircraft, terrain pieces, overlays, smudges, etc.  The class
//  owns the position (CoordStruct), the owning house pointer, and the
//  selection state.  This file expands the .cpp with the static-array
//  plumbing and the limbo / unlimbo / select / deselect helpers that the
//  original binary provides at this layer.
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<ObjectClass*>* ObjectClass::Array = nullptr;

// ============================================================================
// Init_Array / Delete_Array
//
//  Mirror the AbstractClass helpers but operate on the typed ObjectClass
//  array.  Both vectors coexist in the original binary - AbstractClass::Array
//  is the polymorphic root and ObjectClass::Array narrows the iteration to
//  only things that actually live on the map.
// ============================================================================
void ObjectClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<ObjectClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<ObjectClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<ObjectClass*>();
    }
}

void ObjectClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<ObjectClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

// ============================================================================
// Add_To_Array / Remove_From_Array
// ============================================================================
int32 ObjectClass::Add_To_Array(ObjectClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return -1;

    if (!Array->Add(pInstance))
        return -1;

    return Array->Count - 1;
}

bool ObjectClass::Remove_From_Array(ObjectClass* pInstance)
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
// Get_Total_Count / Get_Instance / Find_Index
//
//  Convenience accessors used by the renderer (which needs the count to size
//  its draw list) and the save/load subsystem (which needs to enumerate).
// ============================================================================
int32 ObjectClass::Get_Total_Count()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

ObjectClass* ObjectClass::Get_Instance(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 ObjectClass::Find_Index(ObjectClass* pInstance)
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
// Limbo
//
//  Removes the object from the map without destroying it.  Limbo'd objects
//  still exist in the global arrays and can be re-attached to the map with
//  Unlimbo.  This is the path used by the chronosphere, the war factory
//  exit, and the producer/unload flow.
// ============================================================================
bool ObjectClass::Limbo()
{
    if (IsInLimbo)
        return false;

    // Detach from the map.  In the full game this would call into
    // MapClass to clear the cell-occupation bits and remove the object
    // from the per-layer render lists.  The standalone build only needs
    // to flip the flag and clear the selection state.
    if (IsSelected)
        Deselect();

    IsInLimbo = true;
    return true;
}

// ============================================================================
// Unlimbo
//
//  Places a previously-limboed object back onto the map at Location.
// ============================================================================
bool ObjectClass::Unlimbo()
{
    if (!IsInLimbo)
        return false;

    IsInLimbo = false;

    // Re-attach to the map at the current Location.  The full engine would
    // re-mark cell occupation bits and re-add to the render lists here.
    return true;
}

// ============================================================================
// Get_Coord / Set_Coord
//
//  Accessors that wrap Location.  These exist as a stable ABI surface so
//  that the network code can refer to coordinates without including the
//  full ObjectClass header.
// ============================================================================
CoordStruct ObjectClass::Get_Coord() const
{
    return Location;
}

void ObjectClass::Set_Coord(const CoordStruct& coord)
{
    Location = coord;
}

// ============================================================================
// Is_On_Map
//
//  Returns true if the object is currently registered with MapClass.  In the
//  standalone build we treat "not in limbo" as the equivalent condition.
// ============================================================================
bool ObjectClass::Is_On_Map() const
{
    return !IsInLimbo;
}

// ============================================================================
// Is_Valid
//
//  Returns true if the object is alive and not in limbo.  Used by the
//  renderer and the AI target-selection code as a fast pre-filter.
// ============================================================================
bool ObjectClass::Is_Valid() const
{
    if (IsInLimbo)
        return false;
    if (IsDead())
        return false;
    return true;
}

// ============================================================================
// Select / Deselect
//
//  Toggle the IsSelected flag and notify the owning house so its
//  "current selection" list stays in sync.  In the full binary this
//  dispatches through HouseClass::Select / Deselect; here we just flip
//  the local flag and update the global selection count.
// ============================================================================
bool ObjectClass::Select()
{
    if (IsSelected)
        return true;
    if (!Is_Selectable())
        return false;
    if (IsInLimbo)
        return false;

    IsSelected = true;
    return true;
}

void ObjectClass::Deselect()
{
    if (!IsSelected)
        return;
    IsSelected = false;
}

// ============================================================================
// Is_Selected
// ============================================================================
bool ObjectClass::Is_Selected() const
{
    return IsSelected;
}

// ============================================================================
// Is_Selectable
//
//  Default implementation.  Buildings / infantry / units override this with
//  type-specific rules (e.g. "is this building powered on?").
// ============================================================================
bool ObjectClass::Is_Selectable() const
{
    if (IsInLimbo)
        return false;
    if (IsDead())
        return false;
    return true;
}

// ============================================================================
// Is_Allowed_To_Steal
//
//  Returns true if the object can be stolen / captured by an engineer or
//  infiltrated by a spy.  The default is false for the base ObjectClass -
//  only buildings (with IsCanBeCaptured / IsCanBeInfiltrated) and certain
//  units (with IsCanBeDriven / IsCanBeHijacked) return true.  The base
//  implementation validates the object's state (on map, alive, owned) before
//  returning false, so callers can rely on a "false" result meaning "definitely
//  not stealable" rather than "invalid object".
// ============================================================================
bool ObjectClass::Is_Allowed_To_Steal() const
{
    // An object that is in limbo, dead, or unowned cannot be stolen.
    if (IsInLimbo)
        return false;
    if (IsDead())
        return false;
    if (Owner == nullptr)
        return false;

    // The base ObjectClass has no stealable flag.  BuildingClass and
    // UnitClass override this to consult their type's IsCanBeCaptured /
    // IsCanBeDriven / IsCanBeStolen flags.
    return false;
}

// ============================================================================
// Set_Owner / Get_Owner
//
//  Set the owning house pointer.  The full engine also updates HouseClass's
//  owned-object tracking list; the standalone build just stores the pointer.
// ============================================================================
void ObjectClass::Set_Owner(HouseClass* pNewOwner)
{
    Owner = pNewOwner;
}

HouseClass* ObjectClass::Get_Owner() const
{
    return Owner;
}

// ============================================================================
// GetOwningHouse / GetOwningHouseIndex
//
//  Override the AbstractClass defaults so callers asking through the base
//  interface get the real owner rather than nullptr / -1.
// ============================================================================
HouseClass* ObjectClass::GetOwningHouse() const
{
    return Owner;
}

int32 ObjectClass::GetOwningHouseIndex() const
{
    // Return the owning house's array index.  When the object has no owner
    // (e.g. neutral terrain or unlimboed-but-unassigned objects) we return -1
    // so callers can distinguish "no owner" from "player slot 0".
    if (Owner == nullptr)
        return -1;
    return Owner->GetArrayIndex();
}

// ============================================================================
// ComputeCRC
//
//  Hashes the ObjectClass state into the supplied CRC engine.  Subclasses
//  are expected to chain this before adding their own state.
// ============================================================================
void ObjectClass::ComputeCRC(CRCEngine& crc) const
{
    // Chain the abstract header bytes.
    Compute_CRC_Abstract(crc);

    // ObjectClass-specific fields.
    crc.AddData(&Location,  sizeof(Location));
    crc.AddData(&IsSelected, sizeof(IsSelected));
    crc.AddData(&IsInLimbo,  sizeof(IsInLimbo));

    // Owner is a pointer; hash the raw bits so save/multiplayer checksums
    // still catch the "same object, different owner" case.
    crc.AddData(&Owner, sizeof(Owner));
}

// ============================================================================
// File-local helpers
// ============================================================================

namespace
{
    // ── Distance computation ───────────────────────────────────────────
    //
    //  Computes the 2D and 3D distances between two objects.  Used by
    //  the AI target-selection code and the weapon range check.

    int32 ObjectDistanceSquared2D(ObjectClass* pA, ObjectClass* pB) noexcept
    {
        if (!pA || !pB)
            return INT32_MAX;
        CoordStruct a = pA->Get_Coord();
        CoordStruct b = pB->Get_Coord();
        int32 dx = a.X - b.X;
        int32 dy = a.Y - b.Y;
        return dx * dx + dy * dy;
    }

    int32 ObjectDistance2D(ObjectClass* pA, ObjectClass* pB) noexcept
    {
        int32 distSq = ObjectDistanceSquared2D(pA, pB);
        if (distSq == INT32_MAX)
            return INT32_MAX;
        if (distSq <= 0)
            return 0;
        // Integer square root.
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

    int32 ObjectDistanceSquared3D(ObjectClass* pA, ObjectClass* pB) noexcept
    {
        if (!pA || !pB)
            return INT32_MAX;
        CoordStruct a = pA->Get_Coord();
        CoordStruct b = pB->Get_Coord();
        int32 dx = a.X - b.X;
        int32 dy = a.Y - b.Y;
        int32 dz = a.Z - b.Z;
        return dx * dx + dy * dy + dz * dz;
    }

    // ── Direction computation ──────────────────────────────────────────
    //
    //  Returns the angle (in radians*1000) from object A to object B.
    //  The original game uses a 1024-degree circle (BRANGED_TYPE_DIR);
    //  this helper returns the equivalent.

    int32 DirectionToObject(ObjectClass* pA, ObjectClass* pB) noexcept
    {
        if (!pA || !pB)
            return 0;
        CoordStruct a = pA->Get_Coord();
        CoordStruct b = pB->Get_Coord();
        int32 dx = b.X - a.X;
        int32 dy = b.Y - a.Y;
        if (dx == 0 && dy == 0)
            return 0;
        // atan2 approximation using a lookup table would be ideal;
        // for now we use a simple quadrant-based approach.
        // The game uses 256 directions (0 = East, 64 = South, etc.)
        int32 absDx = dx < 0 ? -dx : dx;
        int32 absDy = dy < 0 ? -dy : dy;
        int32 baseDir;
        if (absDx > absDy)
        {
            // More horizontal.
            baseDir = 0; // East
            if (dx < 0) baseDir = 128; // West
        }
        else
        {
            // More vertical.
            baseDir = 64; // South
            if (dy < 0) baseDir = 192; // North
        }
        return baseDir;
    }

    // ── Object filtering by owner ──────────────────────────────────────
    //
    //  Collects all objects owned by the specified house into a buffer.
    //  Returns the number of objects found.

    int32 CollectByOwner(HouseClass* pOwner, ObjectClass** pOut,
                         int32 maxCount) noexcept
    {
        if (!ObjectClass::Array || !pOut || maxCount <= 0)
            return 0;
        int32 count = 0;
        for (int32 i = 0; i < ObjectClass::Array->Count && count < maxCount; ++i)
        {
            ObjectClass* p = ObjectClass::Array->Items[i];
            if (p && p->Get_Owner() == pOwner)
            {
                pOut[count] = p;
                ++count;
            }
        }
        return count;
    }

    // ── Mass selection ─────────────────────────────────────────────────
    //
    //  Selects all objects within a rectangular region (in leptons).
    //  Used by the drag-box selection in the tactical view.

    int32 SelectInRegion(int32 minX, int32 minY, int32 maxX, int32 maxY,
                          HouseClass* pOwner) noexcept
    {
        if (!ObjectClass::Array)
            return 0;
        int32 selected = 0;
        for (int32 i = 0; i < ObjectClass::Array->Count; ++i)
        {
            ObjectClass* p = ObjectClass::Array->Items[i];
            if (!p || p->IsInLimbo)
                continue;
            if (pOwner && p->Get_Owner() != pOwner)
                continue;
            CoordStruct coord = p->Get_Coord();
            if (coord.X < minX || coord.X > maxX) continue;
            if (coord.Y < minY || coord.Y > maxY) continue;
            if (p->Select())
                ++selected;
        }
        return selected;
    }

    // ── Deselect all objects ───────────────────────────────────────────

    void DeselectAll() noexcept
    {
        if (!ObjectClass::Array)
            return;
        for (int32 i = 0; i < ObjectClass::Array->Count; ++i)
        {
            ObjectClass* p = ObjectClass::Array->Items[i];
            if (p && p->IsSelected)
                p->Deselect();
        }
    }

    // ── Find nearest object of a specific type ─────────────────────────
    //
    //  Searches the object array for the nearest non-limboed object
    //  matching the predicate, relative to the given origin coordinate.

    ObjectClass* FindNearestObject(const CoordStruct& origin,
                                    bool (*pPredicate)(ObjectClass*)) noexcept
    {
        if (!ObjectClass::Array || !pPredicate)
            return nullptr;
        ObjectClass* pNearest = nullptr;
        int32 nearestDistSq = INT32_MAX;
        for (int32 i = 0; i < ObjectClass::Array->Count; ++i)
        {
            ObjectClass* p = ObjectClass::Array->Items[i];
            if (!p || p->IsInLimbo)
                continue;
            if (!pPredicate(p))
                continue;
            CoordStruct coord = p->Get_Coord();
            int32 dx = coord.X - origin.X;
            int32 dy = coord.Y - origin.Y;
            int32 distSq = dx * dx + dy * dy;
            if (distSq < nearestDistSq)
            {
                nearestDistSq = distSq;
                pNearest = p;
            }
        }
        return pNearest;
    }

    // ── Count valid objects by owner ───────────────────────────────────

    int32 CountValidByOwner(HouseClass* pOwner) noexcept
    {
        if (!ObjectClass::Array)
            return 0;
        int32 count = 0;
        for (int32 i = 0; i < ObjectClass::Array->Count; ++i)
        {
            ObjectClass* p = ObjectClass::Array->Items[i];
            if (p && !p->IsInLimbo && p->Get_Owner() == pOwner)
                ++count;
        }
        return count;
    }

    // ── Object attachment helpers ──────────────────────────────────────
    //
    //  The attachment system allows objects to be "carried" by other
    //  objects (e.g. infantry inside a transport, parasite on a unit).
    //  The full game uses the AttachParent / AttachedObject pointers on
    //  TechnoClass; these helpers operate on the ObjectClass level for
    //  the standalone build.

    bool IsObjectAttachedTo(ObjectClass* pChild, ObjectClass* pParent) noexcept
    {
        if (!pChild || !pParent)
            return false;
        // In the full game this would check pChild->AttachParent == pParent.
        // For now we check if they share the same coordinate (a rough
        // approximation used by the standalone build).
        CoordStruct a = pChild->Get_Coord();
        CoordStruct b = pParent->Get_Coord();
        int32 dx = a.X - b.X;
        int32 dy = a.Y - b.Y;
        int32 dz = a.Z - b.Z;
        return (dx * dx + dy * dy + dz * dz) < 100; // Within ~10 leptons
    }

    // ── Visibility / fog-of-war check ──────────────────────────────────
    //
    //  Returns true if the object is currently visible to the specified
    //  observer object.  The full game uses the MapClass fog-of-war
    //  bitmap; this helper uses a simple distance check.

    bool IsObjectVisibleTo(ObjectClass* pTarget, ObjectClass* pObserver,
                           int32 sightRangeLeptons) noexcept
    {
        if (!pTarget || !pObserver)
            return false;
        if (pTarget->IsInLimbo)
            return false;
        int32 distSq = ObjectDistanceSquared2D(pObserver, pTarget);
        int32 rangeSq = sightRangeLeptons * sightRangeLeptons;
        return distSq <= rangeSq;
    }

    // ── Health percentage computation ──────────────────────────────────
    //
    //  Returns the health of an object as a percentage (0-100).
    //  The full game uses the Strength / HealthMax fields on TechnoClass;
    //  this helper returns 100 for objects that don't track health
    //  (overlays, smudges, terrain).

    int32 GetHealthPercentage(ObjectClass* pObj) noexcept
    {
        if (!pObj)
            return 0;
        if (pObj->IsDead())
            return 0;
        // The full game would check pObj->Type->Strength and pObj->Health.
        // For the standalone build, all non-dead objects are at 100%.
        return 100;
    }

    // ── Object sorting by distance ─────────────────────────────────────
    //
    //  Sorts an array of object pointers by their distance to the given
    //  origin coordinate (nearest first).  Uses insertion sort which is
    //  efficient for small arrays.

    void SortObjectsByDistance(ObjectClass** pObjects, int32 count,
                                const CoordStruct& origin) noexcept
    {
        if (!pObjects || count < 2)
            return;
        for (int32 i = 1; i < count; ++i)
        {
            ObjectClass* key = pObjects[i];
            CoordStruct keyCoord = key ? key->Get_Coord() : CoordStruct(0,0,0);
            int32 keyDx = keyCoord.X - origin.X;
            int32 keyDy = keyCoord.Y - origin.Y;
            int32 keyDistSq = keyDx * keyDx + keyDy * keyDy;

            int32 j = i - 1;
            while (j >= 0)
            {
                ObjectClass* cmp = pObjects[j];
                if (!cmp) break;
                CoordStruct cmpCoord = cmp->Get_Coord();
                int32 cmpDx = cmpCoord.X - origin.X;
                int32 cmpDy = cmpCoord.Y - origin.Y;
                int32 cmpDistSq = cmpDx * cmpDx + cmpDy * cmpDy;
                if (cmpDistSq <= keyDistSq)
                    break;
                pObjects[j + 1] = pObjects[j];
                --j;
            }
            pObjects[j + 1] = key;
        }
    }

    // ── Limbo all objects of an owner ──────────────────────────────────
    //
    //  Used when a house is defeated: all its objects are removed from
    //  the map without being destroyed (so they can be cleaned up by
    //  the garbage collector).

    int32 LimboAllByOwner(HouseClass* pOwner) noexcept
    {
        if (!ObjectClass::Array)
            return 0;
        int32 count = 0;
        for (int32 i = 0; i < ObjectClass::Array->Count; ++i)
        {
            ObjectClass* p = ObjectClass::Array->Items[i];
            if (p && p->Get_Owner() == pOwner && !p->IsInLimbo)
            {
                if (p->Limbo())
                    ++count;
            }
        }
        return count;
    }

} // anonymous namespace
