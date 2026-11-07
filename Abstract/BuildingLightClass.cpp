#include "BuildingLightClass.h"

#include <Abstract/BuildingClass.h>
#include <Abstract/FootClass.h>
#include <Houses/HouseClass.h>
#include <Map/CellClass.h>
#include <Map/MapClass.h>

// ============================================================================
// BuildingLightClass::SetBehaviour (asm 0x436C40)
//
//  See the header for the full contract.  The interesting part is the tracking
//  scan; the entry side is three stores.
// ============================================================================
void BuildingLightClass::SetBehaviour(int32 behaviour)
{
    // asm entry: clear the two transient fields, latch the mode.
    field_B0      = 0;
    BehaviourMode = behaviour;
    field_B4      = 0;

    if (behaviour != Behaviour_Track)
        return;

    // The scan centre comes from the rig's own reported position.  In the
    // original this is the vt+0x1B8 slot; in OpenYR the light is a
    // first-class ObjectClass, so its world position is simply Location.
    const CoordStruct centre = Location;

    // OwnerBuilding->Owner -- the house whose alliance test gates candidates.
    HouseClass* ownerHouse = nullptr;
    if (OwnerBuilding != nullptr)
        ownerHouse = OwnerBuilding->Owner;

    ObjectClass* bestObject = nullptr;
    int32        bestDistance = 0x98967F;   // asm: initial "infinity"

    // asm loops both counters from -1 while (counter + 1) < 2, i.e. {-1, 0}.
    // Reproduced verbatim -- see the NOTE in the header.
    for (int32 dy = -1; dy < 1; ++dy) {
        for (int32 dx = -1; dx < 1; ++dx) {
            const CellStruct cell(
                static_cast<int16>(centre.X / LeptonsPerCell + dx),
                static_cast<int16>(centre.Y / LeptonsPerCell + dy));

            CellClass* pCell = MapClass::Instance != nullptr
                ? MapClass::Instance->TryGetCellAt(cell.X, cell.Y)
                : nullptr;
            if (pCell == nullptr)
                continue;

            // The cell's ground-object list.  asm walks [cell+0xE4] and
            // follows ObjectClass::NextObject ([obj+0x30]).
            for (ObjectClass* pObj = pCell->FirstObject; pObj != nullptr; ) {
                // The link must be read *before* any early exit because the
                // loop tail is shared by all branches in the original.
                ObjectClass* pNext = pObj->NextObject;

                const AbstractType what = pObj->WhatAmI();
                if (what == AbstractType::Infantry || what == AbstractType::Unit) {
                    // asm: HouseClass::Is_Ally(ownerHouse, obj) -- true when the
                    // candidate belongs to a house allied with the owner.
                    HouseClass* pObjHouse = pObj->Owner;
                    const bool ally = (ownerHouse != nullptr)
                        && (pObjHouse != nullptr)
                        && ownerHouse->IsAlliedWith(pObjHouse);

                    if (!ally) {
                        const CoordStruct objPos = pObj->GetCoords();
                        const int32 ddx = objPos.X - centre.X;
                        const int32 ddy = objPos.Y - centre.Y;
                        const int32 ddz = objPos.Z - centre.Z;
                        const double sq = static_cast<double>(ddx) * ddx
                                        + static_cast<double>(ddy) * ddy
                                        + static_cast<double>(ddz) * ddz;
                        const int32 dist = static_cast<int32>(std::sqrt(sq));  // asm: floor after sqrt

                        if (dist < bestDistance) {
                            bestDistance = dist;
                            bestObject   = pObj;
                        }
                    }
                }

                pObj = pNext;
            }
        }
    }

    if (bestObject != nullptr)
        TrackingObject = bestObject;
}
