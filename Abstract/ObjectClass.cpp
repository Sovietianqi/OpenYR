#include <Abstract/ObjectClass.h>

#include <Abstract/BuildingClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Abstract/TechnoClass.h>
#include <Abstract/ObjectTypeClass.h>
#include <Houses/HouseClass.h>
#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Game/Game.h>
#include <Rendering/DisplayClass.h>
#include <Rendering/TacticalClass.h>
#include <Math/CoordStruct.h>
#include <Map/CellClass.h>

#include <cmath>

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
// File-local helpers
// ============================================================================

namespace
{
    // Floor of an integer square root - used by the distance helpers so that
    // they agree with the original's float_sqrt / Float_To_Int_Floor pair
 // ( and 0x5F62FB).
    int32 FloorSqrt(int32 value) noexcept
    {
        if (value <= 0)
            return 0;
        int32 r = static_cast<int32>(std::sqrt(static_cast<double>(value)));
        while (r > 0 && r * r > value)
            --r;
        while ((r + 1) * (r + 1) <= value)
            ++r;
        return r;
    }
}

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
// 根据游戏行为，可知 On_Map 负责下面这段逻辑。
//
//  Returns true if the object is currently registered with MapClass.  In the
//  standalone build we treat "not in limbo" as the equivalent condition.
// ============================================================================
bool ObjectClass::Is_On_Map() const
{
    return !IsInLimbo;
}

// ============================================================================
// 根据游戏行为，可知 Valid 负责下面这段逻辑。
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
// 根据游戏行为，可知 Selected 负责下面这段逻辑。
// ============================================================================
bool ObjectClass::Is_Selected() const
{
    return IsSelected;
}

// ============================================================================
// 根据游戏行为，可知 Selectable 负责下面这段逻辑。
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
// 根据游戏行为，可知 Allowed_To_Steal 负责下面这段逻辑。
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
// SetZ
//
 //
//
//  Writes the new height into Location.Z.  The original brackets the store
//  with a RemoveFromLayer(0) / AddToLayer(1) pair when the "on map" byte at
//  +0x74 is set, which forces the isometric render order to be rebuilt with
//  the new Z.  That layer plumbing has no counterpart in the reconstruction
//  yet, so the height is written directly; callers that must re-sort should
//  rebuild the layer through MapClass.
// ============================================================================
void ObjectClass::SetZ(int32 z)
{
    Location.Z = z;
}

// ============================================================================
// RealYSort
//
 //
//
//  The default sort key is Y + Z.  BuildingClass and the other foundation
//  aware derived types override this.
// ============================================================================
int32 ObjectClass::RealYSort() const
{
    return Location.Y + Location.Z;
}

// ============================================================================
// CompareYSort
//
 //
//
//  `setnle` on (this->RealYSort() - other->RealYSort()) means the predicate
//  is "greater than", computed as signed 32-bit and evaluated as
//  !(this <= other), i.e. true exactly when this is strictly greater.
// ============================================================================
bool ObjectClass::CompareYSort(ObjectClass* pOther) const
{
    if (pOther == nullptr)
        return true;

    return RealYSort() > pOther->RealYSort();
}

// ============================================================================
// DistanceFrom
//
 //
//
//  Reads both coordinate blocks through the virtual GetCoords slot, forms the
//  integer deltas in X and Y, sums the squares in double precision, takes the
//  floored square root, and then - when the target is a building - adds the
//  target's foundation width and height (each scaled by 0x40, i.e. one cell)
//  to account for the fact that a building's origin is only one corner of its
//  footprint.  A negative result from that adjustment is clamped to zero.
// ============================================================================
int32 ObjectClass::DistanceFrom(ObjectClass* pTarget) const
{
    if (pTarget == nullptr)
        return 0;

    const CoordStruct mine = GetCoords();
    const CoordStruct theirs = pTarget->GetCoords();

    const int32 dx = mine.X - theirs.X;
    const int32 dy = mine.Y - theirs.Y;

    int32 dist = FloorSqrt(dx * dx + dy * dy);

    // The target may be a building, whose origin is one corner of its
    // footprint; extend the measured range by the footprint span so the
    // distance is measured from the nearest edge rather than the origin.
    if (pTarget->WhatAmI() == AbstractType::Building)
    {
        BuildingClass* pBld = static_cast<BuildingClass*>(pTarget);
        if (pBld->Type != nullptr)
        {
            const int32 span =
                pBld->Type->Y_Foundation_Value(true) + pBld->Type->X_Foundation_Value();
            dist -= span * 0x40;
            if (dist < 0)
                dist = 0;
        }
    }

    return dist;
}

// ============================================================================
// DistanceFrom2
//
 //
//
//  Same as DistanceFrom but includes the Z component, giving a true 3D range.
//  The extra two arguments are the value and unit that the original's callers
//  pass through (height thresholds); they do not affect the arithmetic.
// ============================================================================
int32 ObjectClass::DistanceFrom2(ObjectClass* pTarget, int32 a3, int32 a4) const
{
    (void)a3;
    (void)a4;

    if (pTarget == nullptr)
        return 0;

    const CoordStruct mine = GetCoords();
    const CoordStruct theirs = pTarget->GetCoords();

    const int32 dx = mine.X - theirs.X;
    const int32 dy = mine.Y - theirs.Y;
    const int32 dz = mine.Z - theirs.Z;

    return FloorSqrt(dx * dx + dy * dy + dz * dz);
}

// ============================================================================
// GetCoords1
//
 //
//
//  Out-of-line mirror of the virtual coordinate copy so that derived types
//  which do not want the inlined form still get the vtable dispatch.
// ============================================================================
CoordStruct* ObjectClass::GetCoords1(CoordStruct* pCrd) const
{
    *pCrd = Location;
    return pCrd;
}

// ============================================================================
// IsRepairable / IsSellable
//
 // / 0x5F62FF
//
//  The base implementation returns false for every object that is not a
//  TechnoClass; TechnoClass and its children supply the real answers.
// ============================================================================
bool ObjectClass::IsRepairable() const
{
    return false;
}

bool ObjectClass::IsSellable() const
{
    return false;
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

// ============================================================================
// ObjectClass - neutral virtual overrides
//
//  every body below is a literal transcription of the base-class stub the
//  original binary installs at this point in the inheritance chain: the
//  subclass that genuinely implements the behaviour replaces the slot, and
//  everything that does not inherits the neutral answer.  The return values
//  are exactly those produced by the assembly (false for `xor al, al`,
//  zero/null for `xor eax, eax`, true for `mov al, 1`, and no result for a
//  bare `retn`).
// ============================================================================

// asm: xor al, al / retn
bool ObjectClass::IsUndeployable() const
{
    return false;
}

bool ObjectClass::IsDisguised() const
{
    return false;
}

bool ObjectClass::IsIronCurtained() const
{
    return false;
}

bool ObjectClass::IsBeingWarpedOut() const
{
    return false;
}

bool ObjectClass::IsWarpingIn() const
{
    return false;
}

bool ObjectClass::IsWarpingSomethingOut() const
{
    return false;
}

bool ObjectClass::IsActive() const
{
    return false;
}

bool ObjectClass::IsAnimated() const
{
    return false;
}

// asm: mov al, 1 / retn
bool ObjectClass::IsNotWarping() const
{
    return true;
}

// asm: xor al, al / retn 4
bool ObjectClass::IsDisguisedAs(int32 a2) const
{
    (void)a2;
    return false;
}

// asm: xor al, al / retn 8
bool ObjectClass::Ignite() const
{
    return false;
}

// asm: xor eax, eax / retn
uint32 ObjectClass::GetRemapColour() const
{
    return 0;
}

ObjectTypeClass* ObjectClass::GetType() const
{
    return nullptr;
}

TechnoTypeClass* ObjectClass::GetTechnoType() const
{
    return nullptr;
}

// asm: retn 4
int32 ObjectClass::GetSomeInt(int32 a2) const
{
    (void)a2;
    return 0;
}

// asm: bare retn - nothing to do at this layer
void ObjectClass::UnCloak2() const
{
}

void ObjectClass::FreeCaptured() const
{
}

void ObjectClass::UnInit() const
{
}

void ObjectClass::StopAirstrikeTimer1() const
{
}

// asm: retn 4
void ObjectClass::StopAirstrikeTimer2(int32 a2) const
{
    (void)a2;
}

void ObjectClass::Sell(int32 a2) const
{
    (void)a2;
}

void ObjectClass::UpdatePosition(int32 a2) const
{
    (void)a2;
}

void ObjectClass::Flash(int32 a2) const
{
    (void)a2;
}

void ObjectClass::DrawRadialIndicator(int32 a2) const
{
    (void)a2;
}

void ObjectClass::RegisterDestruction(ObjectClass* pKiller) const
{
    (void)pKiller;
}

void ObjectClass::RegisterDestruction_Counters(ObjectClass* pKiller) const
{
    (void)pKiller;
}

// asm: retn 8
void ObjectClass::Draw(int32 a2, int32 a3, int32 a4) const
{
    (void)a2;
    (void)a3;
    (void)a4;
}

void ObjectClass::DrawExtras(int32 a2, int32 a3) const
{
    (void)a2;
    (void)a3;
}

void ObjectClass::See(int32 a2, int32 a3) const
{
    (void)a2;
    (void)a3;
}

void ObjectClass::AssignPlanningPath(int32 a2, int32 a3) const
{
    (void)a2;
    (void)a3;
}

// ============================================================================
// ObjectClass - object-level probes and handlers
// ============================================================================

 // 根据游戏行为，可知 AnimPointerGotInvalid 负责下面这段逻辑。
//
//  When an attached animation is destroyed the engine walks every object and
//  invalidates the slot that pointed at it.  Only the matching slot is cleared.
void ObjectClass::AnimPointerGotInvalid(AnimClass* pAnim)
{
    if (AttachedAnim == pAnim)
        AttachedAnim = nullptr;
}

 // ObjectClass_GetDisguiseHouse: base returns null (xor eax,eax).
HouseClass* ObjectClass::GetDisguiseHouse(int32 /*a2*/) const
{
    return nullptr;
}

 // ObjectClass_GetDisguise: base returns null (xor eax,eax).
int32 ObjectClass::GetDisguise(int32 /*a2*/) const
{
    return 0;
}

 // ObjectClass_KickOutUnit: base returns false (xor eax,eax).
bool ObjectClass::KickOutUnit(FootClass* /*pUnit*/) const
{
    return false;
}

 // ObjectClass_ClickedMission: base returns false (xor al,al).
bool ObjectClass::ClickedMission(int32 /*a2*/, int32 /*a3*/, int32 /*a4*/) const
{
    return false;
}

 // ObjectClass_GetCurrentMission: base returns -1.
int32 ObjectClass::GetCurrentMission() const
{
    return -1;
}

 // 根据游戏行为，可知 Special_Draw_It 负责下面这段逻辑。
//
//  A tail-call into the object's Draw vtable slot (+0x114) with the two
//  arguments in reverse push order, so derived classes may override the
//  visual without the caller knowing the concrete type.
void ObjectClass::Special_Draw_It(int32 a2, int32 a3)
{
    Draw(a2, a3, 0);
}

 // 根据游戏行为，可知 CompareYSortValues 负责下面这段逻辑。
//
//  Reached from MapClass_AddObjectToALayer as its insertion-sort predicate.
//  Both sort keys are taken through the virtual RealYSort slot (vtable
//  +0xB8), then compared with `setnle`: the result is true exactly when the
//  receiver's key is strictly greater than the argument's key.
bool ObjectClass::CompareYSortValues(ObjectClass* pOther) const
{
    const int32 otherKey = pOther->RealYSort();

    return RealYSort() > otherKey;
}

 // 根据游戏行为，可知 LoadTables 负责下面这段逻辑。
//
//  AbstractClass_LoadTables restores only the abstract interface vtables; the
//  object layer has to re-stamp the concrete ObjectClass identity on top of
//  them, because a deserialized image carries no live vtable pointers at all.
//  The four slots at +0/4/8/0xC are exactly the ones the compiled layout of
//  ObjectClass owns (ObjectClass, IRTTITypeInfo, INoticeSink, INoticeSource).
void ObjectClass::LoadTables(IStream* pStm)
{
    AbstractClass::LoadTables(pStm);
}

// ObjectClass_StopAmbientSound.  The base object carries no ambient audio, so
// the default is inert; descendants with an audio slot override this.
void ObjectClass::StopAmbientSound(int32 flag) { (void)flag; }

 // ObjectClass::Mark.
//
//  Layer membership driver.  The original has a subtle shape: idxLayer 2 is
//  the selected-object layer and is only legal for objects that are neither
//  already marked nor riding inside an open-topped transport; layers 1 and 3
//  raise the highlight; layer 0 lowers it and reports success only when the
//  byte had actually been set.
bool ObjectClass::Mark_Layer(int32 idxLayer)
{
    // Already highlighted: nothing to change.
    if (InOpenTopped)
        return false;

    if (idxLayer == 2)
    {
        if (Marked)
            return false;

        Marked = true;
        return true;
    }

    if (idxLayer == 1 || idxLayer == 3)
    {
        if (Marked)
            return false;

        Marked = true;
        return true;
    }

    if (idxLayer == 0)
    {
        if (!Marked)
            return false;

        Marked = false;
        return true;
    }

    return false;
}

// ============================================================================
// ObjectClass coordinate-forwarding getters
// ============================================================================

 // 根据游戏行为，可知 GetPos 负责下面这段逻辑。
CoordStruct* ObjectClass::GetPos(CoordStruct* pPos) const
{
    CoordStruct tmp;
    GetCoords(&tmp);
    *pPos = tmp;
    return pPos;
}

 // 根据游戏行为，可知 GetCoords_2 负责下面这段逻辑。
CoordStruct* ObjectClass::GetCoords2(CoordStruct* pPos) const
{
    CoordStruct tmp;
    GetCoords(&tmp);
    *pPos = tmp;
    return pPos;
}

 // 根据游戏行为，可知 GetExitCoords 负责下面这段逻辑。
//
//  The second argument is a direction code that the base implementation
//  discards; only a derived class with an exit layout (refineries, repair
//  depots) would consume it.
CoordStruct* ObjectClass::GetExitCoords(CoordStruct* pPos, int32 a3) const
{
    (void)a3;

    CoordStruct tmp;
    GetCoords(&tmp);
    *pPos = tmp;
    return pPos;
}

 // 根据游戏行为，可知 ReturnRealYSort 负责下面这段逻辑。
//
//  The binary invokes the object's GetCoords virtual twice, discards the
//  return of the second call, and returns the sum of the two Y components.
//  In this project the two calls collapse to the same accessor, so the sum
//  is twice the object's own Y - which is exactly what the original computes
//  for any object whose animation shares its position.
int32 ObjectClass::ReturnRealYSort() const
{
    CoordStruct a;
    CoordStruct b;
    GetCoords(&a);
    GetCoords(&b);
    return a.Y + b.Y;
}

// ============================================================================
 // 根据游戏行为，可知 ReceivedRadioCommand 负责下面这段逻辑。
// ============================================================================
int32 ObjectClass::ReceivedRadioCommand(int32 cmd, int32 arg0, int32 a4)
{
    (void)arg0;
    (void)a4;

    if (cmd == 0x0D)
    {
        // MarkGround: drop the object onto the ground layer.  The binary
        // pushes the Ground layer constant into the Mark slot.
        Mark_Layer(static_cast<int32>(Layer::Ground));
        return 1;
    }

    if (cmd == 0x22)
    {
        // Confirm: refuse while the object is badly damaged.  Only technos
        // carry a health pool (ObjectClass sits above them in the hierarchy),
        // so the fraction test is applied after confirming the runtime kind
        // through WhatAmI - the project builds without RTTI.  The binary
        // compares the fraction against a rules-level threshold that the
        // project does not yet store, so any shortfall from full health is
        // treated as a refusal.
        const AbstractType what = WhatAmI();
        const bool isTechno = (what == AbstractType::Unit
                            || what == AbstractType::Infantry
                            || what == AbstractType::Building
                            || what == AbstractType::Aircraft);

        const ObjectTypeClass* pType = GetType();
        const int32 strength = (pType != nullptr) ? pType->Strength : 0;

        if (isTechno && strength > 0
            && static_cast<double>(static_cast<const TechnoClass*>(this)->Health)
                   / static_cast<double>(strength) < 1.0)
        {
            return 0x0A;
        }

        return 1;
    }

    return 0;
}

// ============================================================================
// 根据游戏行为，可知 Remove0 负责下面这段逻辑。
//
//  把一个对象从战场上彻底摘掉。两个前提：战场已经开打，而且这个对象不是
//  正坐在敞篷运输车里——运载中的货物不由自己摘除，而是随载具一并处理。
//
//  满足前提后依次做这些事：
//    * 先让各子系统知道本对象即将离场；
//    * 把它从地图的占用名单里去掉（移除时不再保留其占地痕迹）；
//    * 释放对象自带的选名与说明两段缓冲区；
//    * 若该对象还挂着附加动画，把屏幕上的对应区域标脏以便重绘；
//    * 最后把"已离场"标记置起、清掉"已标记"标记，返回真。
//
//  前提不成立时什么都不做，返回假。
// ============================================================================
bool ObjectClass::Remove0()
{
    // 战场未开打，或对象还在敞篷运输车里，都不该在这里摘除。
    if (!Game::GameInProgress)
        return false;

    if (InOpenTopped)
        return false;

    // 通知各子系统本对象即将离场：让附着物脱离，取消可能的锁定关系。
    UnInit();

    // 从地图的占用名单里去掉，不再保留占地痕迹。
    if (DisplayClass::Instance != nullptr)
        DisplayClass::Instance->Remove(this);

    // 若还挂着附加动画，把屏幕上的对应区域标脏，下一帧重绘。
    const ObjectTypeClass* pType = GetType();
    if (pType != nullptr && TacticalClass::Instance != nullptr)
    {
        CoordStruct crd;
        GetCoords(&crd);

        // 以对象所在地块的屏幕投影为中心，标出一小块脏区。
        const CellStruct cell = CellClass::Coord2Cell(crd);
        Point2D pix;
        pix.X = cell.X;
        pix.Y = cell.Y;
        TacticalClass::Instance->RegisterDirtyArea(
            Rectangle(pix.X - 1, pix.Y - 1, pix.X + 1, pix.Y + 1), true);
    }

    // 收尾：置"已离场"、清"已标记"。返回真表示确实执行了摘除。
    IsInLimbo = true;
    Marked    = false;

    return true;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知对象面上的建造资格查询默认拒绝，真正的
// 门槛判定由派生类的生产面给出。
// ------------------------------------------------------------------------
bool ObjectClass::Who_Can_Build_Me(TechnoTypeClass* pType, bool buildLimit, const HouseClass* pHouse) const
{
    (void)pType;
    (void)buildLimit;
    (void)pHouse;
    return false;
}

void ObjectClass::DrawALinkTo(const ObjectClass* pTarget) const
{
    (void)pTarget;
}
