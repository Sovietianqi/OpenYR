#pragma once

#include <Abstract/TechnoClass.h>
#include <Core/Definitions.h>

class BuildingClass;
class HouseClass;
class ObjectClass;

// ============================================================================
// BuildingLightClass (asm sizeof = 0xE8, mappedto_3696)
//
//  The searchlight / spotlight rig a defensive structure mounts.  A building
//  owns at most one of these (BuildingClass::Spotlight) and drives it through
//  SetBehaviour whenever the structure changes between idle, sweeping and
//  tracking modes.
//
//  asm layout (IDA, for reference only -- this port keeps OpenYR's own field
//  ordering, the semantics are what matter):
//    +0x00  vt                     vtable (slot +0x1B8 = GetCoords-ish)
//    +0x04  o(ObjectClass)         the ObjectClass base sub-object
//    +0xA8  field_A8
//    +0xAC  field_AC
//    +0xB0  field_B0               cleared on every SetBehaviour entry
//    +0xB4  field_B4               cleared on every SetBehaviour entry
//    +0xB8  field_B8..field_BB     4 bytes
//    +0xD0  field_D0
//    +0xD4  field_D4
//    +0xD8  field_D8..field_DB     4 bytes
//    +0xDC  BehaviourMode          latched behaviour passed to SetBehaviour
//    +0xE0  TrackingObject         the object the light is currently locked on
//    +0xE4  OwnerBuilding          the BuildingClass that owns this rig
//
//  The base sub-object at +0x04 is fully-fledged ObjectClass: the tracking
//  loop reaches the owner's house through OwnerBuilding->Owner, and the light
//  itself is registered in the map's object lists, so everything the asm does
//  with the +0x04 fields is already covered by inheriting from ObjectClass.
// ============================================================================
class BuildingLightClass : public ObjectClass {
public:
    static const AbstractType AbsID = AbstractType::BuildingLight;

    static DynamicVectorClass<BuildingLightClass*>* Array;

    // asm: SetBehaviour entry constants.  The routine only performs the
    // 3x3 neighbourhood scan in the "track enemies" mode; every other mode
    // just latches the new mode and clears the two transient fields.
    enum BehaviourModeEnum : int32 {
        Behaviour_None    = 0,
        Behaviour_Sweep   = 1,
        Behaviour_Idle    = 2,
        Behaviour_Track   = 3
    };

    BuildingLightClass() noexcept
        : ObjectClass()
        , field_A8(0)
        , field_AC(0)
        , field_B0(0)
        , field_B4(0)
        , field_D0(0)
        , field_D4(0)
        , BehaviourMode(Behaviour_None)
        , TrackingObject(nullptr)
        , OwnerBuilding(nullptr)
    {}
    virtual ~BuildingLightClass() {}

    virtual AbstractType WhatAmI() const override { return AbstractType::BuildingLight; }
    virtual int32 Size() const override { return sizeof(BuildingLightClass); }

    // ========================================================================
    // SetBehaviour (asm 0x436C40, __thiscall, retn 4)
    //
    //   void SetBehaviour(int behaviour)
    //
    //   Entry (every call):
    //     field_B0     = 0;
    //     BehaviourMode = behaviour;
    //     field_B4     = 0;
    //
    //   If behaviour != 3 the routine returns right there.
    //
    //   behaviour == 3 ("track enemies"):
    //     * Ask the light rig for the world coords it should scan around
    //       (vt+0x1B8).  The returned position feeds MapClass::Coord_Cell as
    //       the scan centre; the ±1 offsets then walk a 3x3 cell neighbourhood
    //       (the two loop counters run -1..0 inclusive in the original -- see
    //       NOTE below).
    //     * ownerHouse = OwnerBuilding->Owner (asm [OwnerBuilding+0x21C]).
    //     * For every cell, walk its ground-object list ([cell+0xE4] ->
    //       ObjectClass::NextObject) and consider each entry whose WhatAmI()
    //       is Infantry (0xF) or Unit (1) and which the owner does NOT regard
    //       as an ally.
    //     * For each candidate compute the 3D distance between the light's own
    //       position (its coord slots) and the candidate's position
    //       (ObjectClass vt+0x48), floor it, and keep the smallest.
    //     * If any candidate was found, store it into TrackingObject.
    //
    //   NOTE on the scan bounds: the original initialises both counters to
    //   -1 and loops while "counter + 1 < 2", i.e. counter in {-1, 0} -- a
    //   2x2 block anchored one cell up-left of the centre, not a symmetric
    //   3x3.  That is reproduced verbatim here; it is legacy behaviour, not
    //   a decompiler artifact (the counters really do start at -1 and the
    //   `inc`/`cmp 2`/`jl` tail is symmetric on both loops).
    // ========================================================================
    void SetBehaviour(int32 behaviour);

    // +0xA8..+0xAC -- the light's mounted offset (unused by SetBehaviour but
    //   part of the rig's serialized state).
    int32 field_A8;
    int32 field_AC;

    // +0xB0/+0xB4 -- transient flags cleared on every SetBehaviour entry.
    int32 field_B0;
    int32 field_B4;

    // +0xD0/+0xD4 -- reserved rig state.
    int32 field_D0;
    int32 field_D4;

    // +0xDC -- the behaviour mode latched by the last SetBehaviour call.
    int32 BehaviourMode;

    // +0xE0 -- the object the beam is currently locked onto, or null.
    ObjectClass* TrackingObject;

    // +0xE4 -- the structure that owns this rig (its Owner is the house the
    //   tracking scan tests allies against).
    BuildingClass* OwnerBuilding;
};
