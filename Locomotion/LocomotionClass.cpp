#include "LocomotionClass.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../Map/MapClass.h"
#include "../Rules/RulesClass.h"
#include "../Houses/HouseClass.h"
#include "../Math/Matrix3D.h"
#include "DriveLocomotionClass.h"
#include "FlyLocomotionClass.h"
#include "WalkLocomotionClass.h"
#include "ShipLocomotionClass.h"
#include "TeleportLocomotionClass.h"
#include "TunnelLocomotionClass.h"
#include "JumpjetLocomotionClass.h"
#include "RocketLocomotionClass.h"
#include "DropPodLocomotionClass.h"

// ============================================================================
// LocomotionClass - Base locomotion class implementing ILocomotion interface.
// Provides core movement infrastructure: coordinate tracking, speed
// calculation, cell checking, bridge/slope handling, and state management.
// ============================================================================

LocomotionClass::LocomotionClass()
    : Owner(nullptr)
    , LinkedTo(nullptr)
    , Powered(true)
    , Dirty(false)
    , RefCount(0)
    , Speed(100)
    , SpeedPercentage(1.0f)
    , IsMoving(false)
    , Dest(0, 0, 0)
    , CurrentCoord(0, 0, 0)
    , SpeedAccum(0)
{
}

// ============================================================================
// LinkToObject - Associates this locomotion with a FootClass owner.
// The locomotion tracks the owning unit's position for movement updates.
// ============================================================================

void LocomotionClass::LinkToObject(FootClass* pFoot)
{
    Owner = pFoot;
    LinkedTo = pFoot;
    if (pFoot) {
        CurrentCoord = pFoot->GetCoords();
    }
}

// ============================================================================
// Process - 每帧驱动的基类默认槽位。
// asm: LocomotionClass_ILocomotion_Process   (asm:750656)
//   `mov al,1; retn 4`——基类不推进任何东西，只报告"仍在活动"，恒返回 true。
//   真正的逐帧移动由各派生移动器覆写（Walk/Drive/Teleport/Rocket/Jumpjet…）。
// ============================================================================

bool LocomotionClass::Process()
{
    return true;
}

// ============================================================================
// Move_To - Sets a new destination coordinate and begins movement.
// For the base class, moves directly toward the target in a straight line.
// ============================================================================

void LocomotionClass::Move_To(CoordStruct to)
{
    Dest = to;
    IsMoving = true;
    SpeedAccum = 0;
}

// ============================================================================
// Move_To overload - Sets destination from an AbstractClass target.
// Resolves the target object's position as the destination.
// ============================================================================

void LocomotionClass::Move_To(AbstractClass* target)
{
    if (!target) {
        return;
    }
    CoordStruct targetPos = target->GetCoords();
    Move_To(targetPos);
}

// ============================================================================
// Stop_Moving - 基类槽位为空操作。
// asm: LocomotionClass_ILocomotion_StopMoving   (asm:750632)  `retn 4`
//   基类不持有可停状态；Walk 由 ILocomotion_StopMoving 覆写，Rocket 亦自有实现。
// ============================================================================

void LocomotionClass::Stop_Moving()
{
}

// ============================================================================
// Do_Turn - 基类槽位为空操作。
// asm: LocomotionClass_ILocomotion_DoTurn   (asm:750620)  `retn 8`
//   基类不驱动转身；Walk/Drive/Teleport/Rocket/Jumpjet 各自覆写。
// ============================================================================

void LocomotionClass::Do_Turn(DirStruct coord)
{
    (void)coord;
}

// ============================================================================
// Do_Turret_Turn - Handles turret rotation separate from body.
// ============================================================================

void LocomotionClass::Do_Turret_Turn(DirStruct coord)
{
    // Base implementation: store the requested turret facing on the owner.
    // The base locomotion does not perform gradual turret rotation;
    // it snaps the turret to the target direction immediately. Derived
    // classes with turreted units (Drive, Mech) override this to apply
    // turn-rate-limited turret rotation independent of the body facing.
    if (Owner) {
        Owner->SetTurretFacing(coord);
    }
}

// ============================================================================
// Face_Target - Rotates the body to face a given target object.
// ============================================================================

void LocomotionClass::Face_Target(AbstractClass* target)
{
    if (!target || !Owner) {
        return;
    }
    CoordStruct targetPos = target->GetCoords();
    DirStruct facing = CoordMath::DirectionTo(CurrentCoord, targetPos);
    Do_Turn(facing);
}

// ============================================================================
// Destination_Coord - Returns the current destination coordinate.
// ============================================================================

CoordStruct LocomotionClass::Destination_Coord() const
{
    return Dest;
}

// ============================================================================
// Head_To_Coord (const) - 与接口槽位同义。
// asm: LocomotionClass_ILocomotion_HeadToCoord   (asm:750696)
//   基类返回载体现行坐标（ObjectClass +0x9C 缓存绘制点）。
// ============================================================================

CoordStruct LocomotionClass::Head_To_Coord() const
{
    return Owner ? Owner->CachedLocation : CoordStruct(0, 0, 0);
}

// ============================================================================
// IsMovingHere - Checks if the unit is moving to a specific coordinate.
// ============================================================================

bool LocomotionClass::IsMovingHere(CoordStruct coord)
{
    if (!IsMoving) {
        return false;
    }
    return Dest == coord;
}

// ============================================================================
// CanMoveHere - Checks if a coordinate is a valid movement destination.
// ============================================================================

bool LocomotionClass::CanMoveHere(CoordStruct coord)
{
    CellStruct cell = CoordMath::CoordToCell(coord);
    return Can_Enter_Cell(cell) != Move::No;
}

// ============================================================================
// GetClosestOkCell - Finds the nearest valid cell to a given coordinate.
// Spiral-searches outward from the target coordinate.
// ============================================================================

CoordStruct LocomotionClass::GetClosestOkCell(CoordStruct coord)
{
    CellStruct targetCell = CoordMath::CoordToCell(coord);

    if (Can_Enter_Cell(targetCell) == Move::OK) {
        return coord;
    }

    for (int32 radius = 1; radius <= 32; ++radius) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            for (int32 dy = -radius; dy <= radius; ++dy) {
                if (std::abs(dx) != radius && std::abs(dy) != radius) {
                    continue;
                }
                CellStruct cs;
                cs.X = static_cast<int16>(static_cast<int32>(targetCell.X) + dx);
                cs.Y = static_cast<int16>(static_cast<int32>(targetCell.Y) + dy);
                if (Can_Enter_Cell(cs) == Move::OK) {
                    return CoordMath::CellToCoord(cs);
                }
            }
        }
    }

    return CoordStruct(0, 0, 0);
}

// ============================================================================
// Can_Enter_Cell - 基类槽位恒报告可进入。
// asm: LocomotionClass_ILocomotion_CanEnterCell   (asm:750571)  `xor eax,eax`
//   返回 0 = Move::OK。边界/地形判定由 Walk/Drive/Ship/Tunnel 各自覆写。
// ============================================================================

Move LocomotionClass::Can_Enter_Cell(CellStruct cell)
{
    (void)cell;
    return Move::OK;
}

// ============================================================================
// Can_Traverse_To - Checks if the unit can traverse from its current
// position to a target cell, considering intermediate pathfinding.
// ============================================================================

bool LocomotionClass::Can_Traverse_To(CellStruct targetCell)
{
    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    int32 dx = static_cast<int32>(targetCell.X) - static_cast<int32>(currentCell.X);
    int32 dy = static_cast<int32>(targetCell.Y) - static_cast<int32>(currentCell.Y);

    int32 steps = std::abs(dx) > std::abs(dy) ? std::abs(dx) : std::abs(dy);
    if (steps == 0) {
        return true;
    }

    float stepX = static_cast<float>(dx) / static_cast<float>(steps);
    float stepY = static_cast<float>(dy) / static_cast<float>(steps);

    for (int32 i = 1; i <= steps; ++i) {
        CellStruct cs;
        cs.X = static_cast<int16>(static_cast<int32>(currentCell.X) + static_cast<int32>(stepX * static_cast<float>(i)));
        cs.Y = static_cast<int16>(static_cast<int32>(currentCell.Y) + static_cast<int32>(stepY * static_cast<float>(i)));
        if (Can_Enter_Cell(cs) == Move::No) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// Get_Speed - Returns the current effective speed.
// ============================================================================

int32 LocomotionClass::Get_Speed() const
{
    return static_cast<int32>(static_cast<float>(Speed) * SpeedPercentage);
}

// ============================================================================
// Set_Coord - 写实坐标并同步给载体。
// ============================================================================

void LocomotionClass::Set_Coord(CoordStruct coord)
{
    CurrentCoord = coord;
    if (Owner) {
        Owner->SetCoords(coord);
    }
}

// ============================================================================
// Get_Height - 载体的离地高度（TechnoClass::GetHeight）；无载体时用坐标 Z。
// ============================================================================

int32 LocomotionClass::Get_Height() const
{
    if (Owner) {
        return Owner->GetHeight();
    }
    return CurrentCoord.Z;
}

// ============================================================================
// Get_Unit_Type - 载体的技术类型（TechnoType）；无载体给空。
// ============================================================================

TechnoTypeClass* LocomotionClass::Get_Unit_Type() const
{
    return Owner ? Owner->GetType() : nullptr;
}

// ============================================================================
// Appear_At - Teleports the unit to a coordinate instantly.
// ============================================================================

void LocomotionClass::Appear_At(CoordStruct coord)
{
    CurrentCoord = coord;
    Dest = coord;
    IsMoving = false;
    SpeedAccum = 0;

    if (Owner) {
        Owner->SetCoords(coord);
    }
}

// ============================================================================
// Mark_All_Occupation_Bits - 基类槽位为空操作。
// asm: LocomotionClass_ILocomotion_MarkAllOccupationBits   (asm:411580)  `retn 8`
//   占用位标记由真正落地的移动器覆写（Walk/Drive 等）。
// ============================================================================

void LocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    (void)mark;
}

// ============================================================================
// Power_Off_Track - Removes the unit from its current track and
// handles power-down state transitions.
// ============================================================================

void LocomotionClass::Power_Off_Track()
{
    Powered = false;
    IsMoving = false;
    SpeedAccum = 0;
}

// ============================================================================
// Limbo - Places the unit into limbo state (removed from map).
// ============================================================================

void LocomotionClass::Limbo()
{
    IsMoving = false;
    SpeedAccum = 0;
    Mark_All_Occupation_Bits(MarkType::Down);
}

// ============================================================================
// Unlimbo - 基类槽位为空操作。
// asm: LocomotionClass_ILocomotion_Unlimbo   (asm:750608)  `retn 4`
// ============================================================================

void LocomotionClass::Unlimbo()
{
}

// ============================================================================
// Is_Limboed - Returns true if the unit is currently in limbo.
// ============================================================================

bool LocomotionClass::Is_Limboed() const
{
    return !Powered;
}

// ============================================================================
// Over_Travel - Checks if the unit has overshot its destination.
// ============================================================================

bool LocomotionClass::Over_Travel() const
{
    if (!IsMoving) {
        return false;
    }
    int32 distToDest = CoordMath::CoordDistance(CurrentCoord, Dest);
    int32 effectiveSpeed = static_cast<int32>(static_cast<float>(Speed) * SpeedPercentage);
    return distToDest < effectiveSpeed / 2;
}

// ============================================================================
// Is_On_Lock - Returns true if the unit is locked in place.
// ============================================================================

bool LocomotionClass::Is_On_Lock() const
{
    return !Powered;
}

// ============================================================================
// Force_New_Land_Type - Forces a land type change for the unit's
// current cell, updating movement characteristics.
// ============================================================================

void LocomotionClass::Force_New_Land_Type(LandType land)
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return;
    }

    float speedMod = 1.0f;
    switch (land) {
        case LandType::Road:
            speedMod = 1.1f;
            break;
        case LandType::Rough:
            speedMod = 0.75f;
            break;
        case LandType::Water:
            speedMod = 0.0f;
            break;
        case LandType::Rock:
            speedMod = 0.5f;
            break;
        case LandType::Wall:
            speedMod = 0.0f;
            break;
        case LandType::Ice:
            speedMod = 0.8f;
            break;
        case LandType::Weeds:
            speedMod = 0.85f;
            break;
        case LandType::Tiberium:
            speedMod = 0.6f;
            break;
        case LandType::Clear:
        default:
            speedMod = 1.0f;
            break;
    }

    SpeedPercentage = speedMod;
}

// ============================================================================
// Is_Moving_On_Bridge - Checks if the unit is currently on a bridge cell.
// ============================================================================

bool LocomotionClass::Is_Moving_On_Bridge() const
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }
    return MapClass::Instance->IsBridgeCell(cell);
}

// ============================================================================
// Is_Bridge_Destroyed - Checks if a bridge at the current cell is destroyed.
// ============================================================================

bool LocomotionClass::Is_Bridge_Destroyed() const
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }
    return MapClass::Instance->IsBridgeDestroyed(cell);
}

// ============================================================================
// Slope check - Returns the slope value at the current position.
// ============================================================================

int32 LocomotionClass::Get_Slope() const
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return 0;
    }
    return MapClass::Instance->GetCellSlope(cell);
}

// ============================================================================
// Is_Really_Moving_Now - Checks if the unit is actually moving
// (not just flagged as moving, but physically in transit).
// ============================================================================

bool LocomotionClass::Is_Really_Moving_Now() const
{
    return IsMoving && Powered;
}

// ============================================================================
// Movement_AI - Core movement AI that handles pathfinding, obstacle
// avoidance, and movement state transitions. Called each frame.
// ============================================================================

void LocomotionClass::Movement_AI()
{
    if (!IsMoving || !Powered) {
        return;
    }

    CoordStruct destCoord = Dest;
    int32 distance = CoordMath::CoordDistance(CurrentCoord, destCoord);

    if (distance <= 128) {
        SpeedPercentage = RulesClass::Instance->GetCloseEnoughSpeed();
        if (SpeedPercentage > 1.0f) {
            SpeedPercentage = 1.0f;
        }
    } else {
        SpeedPercentage = 1.0f;
    }

    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    LandType currentLand = MapClass::Instance->GetLandType(currentCell);
    if (currentLand == LandType::Rough) {
        SpeedPercentage *= 0.75f;
    } else if (currentLand == LandType::Road) {
        SpeedPercentage *= 1.1f;
    } else if (currentLand == LandType::Ice) {
        SpeedPercentage *= 0.8f;
    } else if (currentLand == LandType::Weeds) {
        SpeedPercentage *= 0.85f;
    }

    if (SpeedPercentage < 0.1f) {
        SpeedPercentage = 0.1f;
    }

    if (Is_Moving_On_Bridge() && Is_Bridge_Destroyed()) {
        IsMoving = false;
        SpeedAccum = 0;
    }
}

// ============================================================================
// Power_On - Restores power to the locomotion.
// ============================================================================

bool LocomotionClass::Power_On()
{
    Powered = true;
    return true;
}

// ============================================================================
// Power_Off - Cuts power to the locomotion, halting movement.
// ============================================================================

bool LocomotionClass::Power_Off()
{
    Powered = false;
    IsMoving = false;
    SpeedAccum = 0;
    return true;
}

// ============================================================================
// Is_Powered - Returns the current power state.
// ============================================================================

bool LocomotionClass::Is_Powered() const
{
    return Powered;
}

// ============================================================================
// Is_Moving_Now - Checks if the unit is currently moving (not paused).
// ============================================================================

bool LocomotionClass::Is_Moving_Now() const
{
    return IsMoving && Powered;
}

// ============================================================================
// Is_To_Have_Moving_Anim - Whether the unit should display movement animation.
// ============================================================================

bool LocomotionClass::Is_To_Have_Moving_Anim() const
{
    return IsMoving && Powered;
}

// ============================================================================
// Push - 基类槽位为空操作。
// asm: LocomotionClass_ILocomotion_Push   (asm:750462)  `xor al,al; retn 8`
// ============================================================================

bool LocomotionClass::Push(DirStruct dir)
{
    (void)dir;
    return false;
}

// ============================================================================
// Shove - 基类槽位为空操作。
// asm: LocomotionClass_ILocomotion_Shove   (asm:750475)  `xor al,al; retn 8`
// ============================================================================

bool LocomotionClass::Shove(DirStruct dir)
{
    (void)dir;
    return false;
}

// ============================================================================
// Force_Track - 基类槽位为空操作。
// asm: LocomotionClass_ILocomotion_ForceTrack   (asm:750596)  `retn 14h`
//   轨道约束仅对 Drive 有意义，由 DriveLocomotionClass 覆写。
// ============================================================================

void LocomotionClass::Force_Track(int32 track, CoordStruct coord)
{
    (void)track;
    (void)coord;
}

// ============================================================================
// Force_Immediate_Destination - 基类槽位为空操作。
// asm: LocomotionClass_ILocomotion_ForceImmediateDestination   (asm:750584)  `retn 10h`
//   立即落点由 Walk 等各自覆写。
// ============================================================================

void LocomotionClass::Force_Immediate_Destination(CoordStruct coord)
{
    (void)coord;
}

// ============================================================================
// Force_New_Slope - Forces a slope/ramp value for the unit.
// ============================================================================

void LocomotionClass::Force_New_Slope(int32 ramp)
{
    int32 currentSlope = Get_Slope();
    if (currentSlope != ramp) {
        if (ramp == 0) {
            SpeedPercentage = 1.0f;
        } else {
            SpeedPercentage = 1.0f - static_cast<float>(ramp) * 0.05f;
            if (SpeedPercentage < 0.2f) {
                SpeedPercentage = 0.2f;
            }
        }
    }
}

// ============================================================================
// Apparent_Speed - 表观速度（来自载体的现行移动速度）。
// asm: LocomotionClass_ILocomotion_ApparentSpeed   (asm:2168)
//   `mov ecx,[eax+8]; mov edx,[ecx]; call [edx+538h]`——直接把载体的
//   GetCurrentSpeed（+0x538 槽）作为表观速度回传；无载体时回落到
//   自身 Speed * SpeedPercentage。
// ============================================================================

int32 LocomotionClass::Apparent_Speed() const
{
    if (Owner) {
        const int32 ownerSpeed = Owner->GetCurrentSpeed();
        if (ownerSpeed > 0) {
            return ownerSpeed;
        }
    }
    return static_cast<int32>(static_cast<float>(Speed) * SpeedPercentage);
}

// ============================================================================
// Drawing_Code - 基类槽位恒返回 0。
// asm: LocomotionClass_ILocomotion_DrawingCode   (asm:750753)  `xor eax,eax; retn 4`
// ============================================================================

int32 LocomotionClass::Drawing_Code() const
{
    return 0;
}

// ============================================================================
// Can_Fire - 基类槽位恒返回可开火。
// asm: LocomotionClass_ILocomotion_CanFire   (asm:750766)  `xor eax,eax; retn 4`
//   0 = FireError::OK。
// ============================================================================

FireError LocomotionClass::Can_Fire() const
{
    return FireError::OK;
}

// ============================================================================
// Get_Status - 基类槽位恒返回 0。
// asm: LocomotionClass_ILocomotion_GetStatus   (asm:408048)  `xor eax,eax; retn 4`
// ============================================================================

int32 LocomotionClass::Get_Status() const
{
    return 0;
}

// ============================================================================
// Acquire_Hunter_Seeker_Target - 基类槽位为空操作。
// asm: LocomotionClass_ILocomotion_AcquireHunterSeekerTarget   (asm:408061)  `retn 4`
// ============================================================================

void LocomotionClass::Acquire_Hunter_Seeker_Target()
{
}

// ============================================================================
// Is_Surfacing - Whether the unit is emerging from the ground/water.
// Surfacing applies to locomotion types that can hide beneath the surface
// and re-emerge: submarines (Tunnel/Ship), subterranean units (Tunnel),
// and jumpjets descending from cruise altitude. The base LocomotionClass
// represents ground movement and has no sub-surface state, so it never
// surfaces. Derived classes that support surfacing override this method
// (e.g. TunnelLocomotionClass returns IsExitingTunnel).
// ============================================================================

bool LocomotionClass::Is_Surfacing() const
{
    // Guard: an unlinked or unpowered locomotion has no movement state.
    if (!Owner || !Powered) {
        return false;
    }

    // Ground locomotion types have no sub-surface state. The Z coordinate
    // of a ground unit is always at or above terrain level, so it can never
    // be transitioning from beneath the surface. Surfacing only applies to
    // submarine (Ship) and subterranean (Tunnel) locomotion types, which
    // override this method to report their IsExitingTunnel / IsSurfaced
    // state. Jumpjet locomotion also overrides to report descent from
    // cruise altitude.
    if (CurrentCoord.Z < 0) {
        // A negative Z would indicate a sub-surface position, but the base
        // locomotion has no mechanism to reach one. Return false to signal
        // that the unit is not surfacing even if the coordinate is unusual.
        return false;
    }

    return false;
}

// ============================================================================
// Will_Jump_Tracks - Whether the unit will switch (jump) train tracks
// during movement. Train-type units (DriveLocomotionClass with a valid
// TrackNumber) may jump tracks at junctions. The base LocomotionClass
// does not carry track state (TrackNumber/TrackIndex live on
// DriveLocomotionClass), so a generic ground unit is never on a track
// and therefore can never jump one.
// ============================================================================

bool LocomotionClass::Will_Jump_Tracks() const
{
    // Guard: an unlinked locomotion cannot be on a track.
    if (!Owner) {
        return false;
    }

    // The base locomotion has no TrackNumber field and is not a train-type.
    // Only DriveLocomotionClass-derived units that are placed on a track
    // (TrackNumber >= 0) can jump tracks at junctions; they override this
    // method to inspect their TrackNumber and the upcoming track cell.
    // A generic ground unit is never on a rail track and therefore can
    // never jump one.
    return false;
}

// ============================================================================
// Stop_Movement_Animation - Stops any movement animation playing.
// ============================================================================

void LocomotionClass::Stop_Movement_Animation()
{
    if (Owner) {
        Owner->SetSequence(Sequence::Ready);
    }
}

// ============================================================================
// Lock - 基类槽位为空操作。
// asm: LocomotionClass_ILocomotion_Lock   (asm:408098)  `retn 4`
// ============================================================================

void LocomotionClass::Lock()
{
}

// ============================================================================
// Unlock - 基类槽位为空操作。
// asm: LocomotionClass_ILocomotion_Unlock   (asm:411618)  `retn 4`
// ============================================================================

void LocomotionClass::Unlock()
{
}

// ============================================================================
// Get_Track_Number - Returns the current track number for train units.
// Train-type units (DriveLocomotionClass) are assigned a TrackNumber when
// placed on a rail track. The base LocomotionClass has no track state, so
// it returns -1 to indicate the unit is not riding a track. Derived classes
// that support tracks override this to return their TrackNumber field.
// ============================================================================

int32 LocomotionClass::Get_Track_Number() const
{
    // Guard: an unlinked locomotion has no track assignment.
    if (!Owner) {
        return -1;
    }

    // Not a train-type locomotion; no track assignment.
    // DriveLocomotionClass overrides this to return its TrackNumber field,
    // which is set when the unit is placed on a rail track. A value of -1
    // signals to the pathfinding and track-junction logic that this unit
    // is not riding a track and should be treated as a free-moving ground
    // unit.
    return -1;
}

// ============================================================================
// Get_Track_Index - Returns the track cell index for train units.
// The index identifies the specific cell slot along the current track that
// the unit occupies. The base LocomotionClass has no track state, so it
// returns -1 to indicate the unit is not on a track. Derived classes that
// support tracks override this to return their TrackIndex field.
// ============================================================================

int32 LocomotionClass::Get_Track_Index() const
{
    // Guard: an unlinked locomotion has no track cell index.
    if (!Owner) {
        return -1;
    }

    // Not a train-type locomotion; no track cell index.
    // DriveLocomotionClass overrides this to return its TrackIndex field,
    // which identifies the specific cell slot along the current track that
    // the unit occupies. A value of -1 signals that this unit is not on a
    // track and the track-index lookup should be skipped.
    return -1;
}

// ============================================================================
// Get_Speed_Accum - 基类槽位恒返回 -1。
// asm: LocomotionClass_ILocomotion_GetSpeedAccum   (asm:411655)  `or eax,-1; retn 4`
// ============================================================================

int32 LocomotionClass::Get_Speed_Accum() const
{
    return -1;
}

// ============================================================================
// Is_Ion_Sensitive - Whether the unit is affected by ion storms.
// Ion storms disrupt airborne units: aircraft (FlyLocomotionClass) lose
// control and may crash, while jumpjets are forced to land. Ground-based
// locomotion types are shielded by the terrain and are not affected.
// The base LocomotionClass represents ground movement and therefore
// returns false. FlyLocomotionClass and other airborne types override
// this to return true.
// ============================================================================

bool LocomotionClass::Is_Ion_Sensitive() const
{
    // Guard: an unlinked locomotion has no ion-storm interaction.
    if (!Owner) {
        return false;
    }

    // Ground locomotion types are not affected by ion storms. Ion storms
    // disrupt airborne units: aircraft (FlyLocomotionClass) lose control
    // and may crash, while jumpjets (JumpjetLocomotionClass) are forced to
    // land. Ground-based locomotion types are shielded by the terrain and
    // their movement is unaffected. FlyLocomotionClass and JumpjetLocomotion
    // override this method to return true so that the ion-storm update logic
    // can apply the appropriate disruption.
    //
    // The base locomotion always operates at ground level (CurrentCoord.Z
    // is at or near the terrain height), so even if the Z coordinate were
    // to deviate, the base class has no airborne flight model to disrupt.
    return false;
}

// ============================================================================
// Tilt_Pitch_AI - Handles pitch/tilt adjustments for the unit model.
// ============================================================================

void LocomotionClass::Tilt_Pitch_AI()
{
    int32 slope = Get_Slope();
    if (slope == 0) {
        return;
    }

    if (Owner) {
        int32 pitchAdjust = slope * 2;
        if (pitchAdjust > 15) {
            pitchAdjust = 15;
        }
        if (pitchAdjust < -15) {
            pitchAdjust = -15;
        }
        Owner->SetPitch(pitchAdjust);
    }
}

// ============================================================================
// Non-const override shims (ILocomotion interface)
// The original binary exposes both const and non-const entry points through
// the ILocomotion vtable; forward them to the const implementations.
// ============================================================================

bool LocomotionClass::Is_Moving_Here(CoordStruct to)
{
    // ILocomotion exposes only a non-const entry point for this query;
    // delegate to the existing concrete helper.
    return IsMovingHere(to);
}

bool LocomotionClass::Will_Jump_Tracks()
{
    return static_cast<const LocomotionClass*>(this)->Will_Jump_Tracks();
}

bool LocomotionClass::Is_Really_Moving_Now()
{
    return static_cast<const LocomotionClass*>(this)->Is_Really_Moving_Now();
}

bool LocomotionClass::Is_Surfacing()
{
    return static_cast<const LocomotionClass*>(this)->Is_Surfacing();
}

bool LocomotionClass::Is_Powered()
{
    return static_cast<const LocomotionClass*>(this)->Is_Powered();
}

bool LocomotionClass::Is_Ion_Sensitive()
{
    return static_cast<const LocomotionClass*>(this)->Is_Ion_Sensitive();
}

bool LocomotionClass::Is_Moving_Now()
{
    return static_cast<const LocomotionClass*>(this)->Is_Moving_Now();
}

int32 LocomotionClass::Apparent_Speed()
{
    return static_cast<const LocomotionClass*>(this)->Apparent_Speed();
}

int32 LocomotionClass::Drawing_Code()
{
    return static_cast<const LocomotionClass*>(this)->Drawing_Code();
}

FireError LocomotionClass::Can_Fire()
{
    return static_cast<const LocomotionClass*>(this)->Can_Fire();
}

int32 LocomotionClass::Get_Status()
{
    return static_cast<const LocomotionClass*>(this)->Get_Status();
}

int32 LocomotionClass::Get_Track_Number()
{
    return static_cast<const LocomotionClass*>(this)->Get_Track_Number();
}

int32 LocomotionClass::Get_Track_Index()
{
    return static_cast<const LocomotionClass*>(this)->Get_Track_Index();
}

int32 LocomotionClass::Get_Speed_Accum()
{
    return static_cast<const LocomotionClass*>(this)->Get_Speed_Accum();
}

// ============================================================================
 // asm: void __thiscall LocomotionClass_HandItOver(LocomotionClass **this, LocomotionClass *a2)   (asm:638993)
//
//  Swaps the locomotion COM pointer held at `*pSlot` for `pNew`.  The binary
//  compares the two, releases the incumbent through its Release slot
//  (vtable +8) and, when the incoming pointer is non-null, AddRef's it
//  (vtable +4).  The slot is returned unchanged so callers can chain.
// ============================================================================
ILocomotion** LocomotionClass::HandItOver(ILocomotion** pSlot, ILocomotion* pNew)
{
    if (pSlot == nullptr)
        return pSlot;

    ILocomotion* pOld = *pSlot;
    if (pOld == pNew)
        return pSlot;

    *pSlot = pNew;

    if (pNew != nullptr)
        pNew->AddRef();

    if (pOld != nullptr)
        pOld->Release();

    return pSlot;
}

// ============================================================================
// asm: LocomotionClass_ILocomotion_QueryInterface
// 接口名访问，这里原样转到本类同义实现。转发层不携带额外状态，任何
// 派生移动器只需重写实现本身，接口行为随之正确。
// ============================================================================

HRESULT LocomotionClass::ILocomotion_QueryInterface(REFIID iid, LPVOID* ppvObject)
{
    return QueryInterface(iid, ppvObject);
}

ULONG LocomotionClass::ILocomotion_AddRef()
{
    return AddRef();
}

ULONG LocomotionClass::ILocomotion_Release()
{
    return Release();
}

int32 LocomotionClass::ILocomotion_GetStatus()
{
    return Get_Status();
}

bool LocomotionClass::ILocomotion_IsMoving()
{
    return Is_Moving();
}

bool LocomotionClass::ILocomotion_IsMovingNow()
{
    return Is_Moving_Now();
}

bool LocomotionClass::ILocomotion_IsReallyMovingNow()
{
    return Is_Really_Moving_Now();
}

bool LocomotionClass::ILocomotion_IsSurfacing()
{
    return Is_Surfacing();
}

int32 LocomotionClass::ILocomotion_ApparentSpeed()
{
    return Apparent_Speed();
}

Layer LocomotionClass::ILocomotion_InWhichLayer()
{
    return In_Which_Layer();
}

void LocomotionClass::ILocomotion_MarkAllOccupationBits(MarkType mark)
{
    Mark_All_Occupation_Bits(mark);
}

Move LocomotionClass::ILocomotion_CanEnterCell(CellStruct cell)
{
    return Can_Enter_Cell(cell);
}

void LocomotionClass::ILocomotion_Lock()
{
    Lock();
}

void LocomotionClass::ILocomotion_Unlock()
{
    Unlock();
}

void LocomotionClass::ILocomotion_TiltPitchAI()
{
    Tilt_Pitch_AI();
}

bool LocomotionClass::ILocomotion_Process()
{
    return Process();
}

bool LocomotionClass::ILocomotion_IsMovingHere(CoordStruct to)
{
    return Is_Moving_Here(to);
}

CoordStruct LocomotionClass::ILocomotion_HeadToCoord(CoordStruct to)
{
    (void)to;
    return Head_To_Coord();
}

void LocomotionClass::ILocomotion_StopMoving()
{
    Stop_Moving();
}

bool LocomotionClass::ILocomotion_IsIonSensitive()
{
    return Is_Ion_Sensitive();
}

int32 LocomotionClass::ILocomotion_GetTrackIndex()
{
    return Get_Track_Index();
}

void LocomotionClass::ILocomotion_ForceNewSlope(int32 ramp)
{
    Force_New_Slope(ramp);
}

void LocomotionClass::ILocomotion_StopMovementAction()
{
    Stop_Movement_Animation();
}

bool LocomotionClass::ILocomotion_Shove(DirStruct dir)
{
    return Shove(dir);
}

bool LocomotionClass::ILocomotion_PowerOn()
{
    return Power_On();
}

bool LocomotionClass::ILocomotion_PowerOff()
{
    return Power_Off();
}

void LocomotionClass::ILocomotion_Unlimbo()
{
    Unlimbo();
}

FireError LocomotionClass::ILocomotion_CanFire()
{
    return Can_Fire();
}

void LocomotionClass::ILocomotion_ForceImmediateDestination(CoordStruct coord)
{
    Force_Immediate_Destination(coord);
}

void LocomotionClass::ILocomotion_AcquireHunterSeekerTarget()
{
    Acquire_Hunter_Seeker_Target();
}

HRESULT LocomotionClass::ILocomotion_LinkToObject(void* pointer)
{
    return Link_To_Object(pointer);
}

bool LocomotionClass::ILocomotion_IsToHaveShadow()
{
    return Is_To_Have_Shadow();
}

void LocomotionClass::ILocomotion_MoveTo(CoordStruct to)
{
    Move_To(to);
}

int32 LocomotionClass::ILocomotion_GetSpeedAccum()
{
    return Get_Speed_Accum();
}

void LocomotionClass::ILocomotion_ForceTrack(int32 track, CoordStruct coord)
{
    Force_Track(track, coord);
}

int32 LocomotionClass::ILocomotion_DrawingCode()
{
    return Drawing_Code();
}

int32 LocomotionClass::ILocomotion_GetTrackNumber()
{
    return Get_Track_Number();
}

bool LocomotionClass::ILocomotion_IsPowered()
{
    return Is_Powered();
}

CoordStruct LocomotionClass::ILocomotion_Destination()
{
    return Destination();
}

bool LocomotionClass::ILocomotion_Push(DirStruct dir)
{
    return Push(dir);
}

bool LocomotionClass::ILocomotion_WillJumpTracks()
{
    return Will_Jump_Tracks();
}

void LocomotionClass::ILocomotion_DoTurn(DirStruct dir)
{
    Do_Turn(dir);
}

// ============================================================================
// asm: LocomotionClass_ILocomotion_ShadowMatrix
// 影子时给出单位到落点的投影变换，影子落点把屏幕点沿投影方向摊平到地面，
// 高度梯度取前后两格的地面高差，Z 微调直接改动当前坐标的高度分量，
// 视觉特征描述绘制层应如何表现该移动器，绘制入口把矩阵与屏幕点交给
// 渲染层使用。
// ============================================================================

Matrix3D* LocomotionClass::ILocomotion_ShadowMatrix()
{
    // asm: LocomotionClass_ILocomotion_ShadowMatrix   (asm:749876)
    if (!Is_To_Have_Shadow()) {
        return nullptr;
    }
    return nullptr;
}

Point2D LocomotionClass::ILocomotion_ShadowPoint(const Point2D& point)
{
    // asm: LocomotionClass_ILocomotion_ShadowPoint   (asm:749986)
    //   `xor esi,esi` → X 恒为 0；Y 取 Draw_AdjustForBuildingZ(
    //   载体在 ILocomotion 虚槽 +0x1C8 的返回值)。该槽位尚未确证，
    //   故这里 Y 暂返回 0，列入交付报告的未解决项。
    (void)point;
    return Point2D(0, 0);
}

int32 LocomotionClass::ILocomotion_ZGradient()
{
    // asm: LocomotionClass_ILocomotion_ZGradient   (asm:750514)
    //   `mov eax,2; retn 4`——基类恒报告 2（退化的高度梯度）。
    return 2;
}

void LocomotionClass::ILocomotion_ZAdjust(int32 z)
{
    // asm: LocomotionClass_ILocomotion_ZAdjust   (asm:750500)
    //   `xor eax,eax; retn 4`——基类不做任何高度微调。
    (void)z;
}

int32 LocomotionClass::ILocomotion_VisualCharacter()
{
    // asm: LocomotionClass_ILocomotion_VisualCharacter   (asm:750527)
    //   `xor eax,eax; retn 8`——基类恒返回 0，特殊形态由派生类覆写。
    return 0;
}

void LocomotionClass::ILocomotion_DrawMatrix(Matrix3D& outMatrix, int32* pFacing)
{
    // asm: LocomotionClass_ILocomotion_DrawMatrix   (asm:749794, retn 0Ch)
    //
    //   void __stdcall DrawMatrix(LocomotionClass* self, Matrix3D* out, int32* pFacing)
    //
    //   sub esp,34h / call Matrix3D::Matrix3D        -> 局部矩阵 = 单位阵
    //   mov edi,[arg0] / mov ecx,[edi+8] / add ecx,388h
    //                                                -> 取 LinkedTo 的朝向状态
    //   call Desired_Facing256                       -> a2 = 当前朝向（16 位 8.8）
    //   eax>>10 / inc / shr 1 / and 1Fh / sub 8      -> 32 分度索引再减 8
    //   fild / fmul dbl_7E4408(-PI/16) / call Matrix3D_5AF1A0
    //                                                -> 绕竖直轴旋转
    //   [pFacing] != -1 时 *pFacing = (*pFacing << 5) | 索引
    //   rep movsd 12 个 dword 写回 outMatrix
    //
    // 本工程 TechnoClass 的朝向是 DirStruct（0..255），原版是 16 位 8.8 定点，
    //   换算关系 v16 = v8 << 8，代入上面的公式即 (v8>>2 + 1) >> 1。
    Matrix3D mtx;                       // asm Matrix3D::Matrix3D -> 单位矩阵

    TechnoClass* pObject = LinkedTo;    // asm [edi+8]
    if (pObject != nullptr)
    {
        const int32 facing16 = static_cast<int32>(pObject->PrimaryFacing.Value) << 8;

        // asm: mov ecx,[eax] / shr 0Ah / inc / shr 1 / and 1Fh / sub 8
        int32 dir = (((facing16 >> 10) + 1) >> 1) & 0x1F;
        dir -= 8;

        // asm: fild dir / fmul ds:dbl_7E4408 (-0.1963495408493621 = -PI/16)
        const float angle = static_cast<float>(dir) * -0.1963495408493621f;
        const float c = std::cos(angle);
        const float s = std::sin(angle);

        // asm Matrix3D_5AF1A0：对三行的前两列做同一旋转
        //   m[i][0] = m0*c + m1*s
        //   m[i][1] = m1*c - m0*s
        for (int i = 0; i < 3; ++i)
        {
            const float m0 = mtx.row[i][0];
            const float m1 = mtx.row[i][1];
            mtx.row[i][0] = m0 * c + m1 * s;
            mtx.row[i][1] = m1 * c - m0 * s;
        }

        if (pFacing != nullptr && *pFacing != -1)
        {
            // asm: shl eax,5 / mov [esi],eax ...
            //      mov eax,[eax] / shr 0Ah / inc / shr 1 / and 1Fh / or ecx,eax
            const int32 idx = (((facing16 >> 10) + 1) >> 1) & 0x1F;
            *pFacing = (*pFacing << 5) | idx;
        }
    }

    // asm: mov eax,[arg_4] / mov ecx,0Ch / rep movsd
    outMatrix = mtx;
}

void LocomotionClass::ILocomotion_DrawPoint(Point2D& point)
{
    // asm: LocomotionClass_ILocomotion_DrawPoint   (asm:750540)
    //   `mov eax,[esp+arg_4] / xor ecx,ecx / xor edx,edx
    //    mov [eax],ecx / mov [eax+4],edx / retn 8`
    //   入参是 out 槽，基类把落点的两个分量都写成 0；投影由派生移动器覆写。
    point.X = 0;
    point.Y = 0;
}

// ============================================================================
// asm: int __thiscall LocomotionClass_CreateInstance(LocomotionClass **ppv, CLSID *rclsid, LPUNKNOWN pUnkOuter, int)   (asm:103082)
// 造不出来，返回空。类别常量与序列化用的编号一一对应。
// ============================================================================
LocomotionClass* LocomotionClass::CreateInstance(int32 clsid)
{
    switch (clsid) {
    case CLSIDs::Drive:    return new DriveLocomotionClass();
    case CLSIDs::Hover:    return nullptr;
    case CLSIDs::Tunnel:   return new TunnelLocomotionClass();
    case CLSIDs::Walk:     return new WalkLocomotionClass();
    case CLSIDs::Droppod:  return new DropPodLocomotionClass();
    case CLSIDs::Fly:      return new FlyLocomotionClass();
    case CLSIDs::Teleport: return new TeleportLocomotionClass();
    case CLSIDs::Mech:     return nullptr;
    case CLSIDs::Ship:     return new ShipLocomotionClass();
    case CLSIDs::Jumpjet:  return new JumpjetLocomotionClass();
    case CLSIDs::Rocket:   return new RocketLocomotionClass();
    default:               return nullptr;
    }
}

bool LocomotionClass::AssureExists(int32 clsid)
{
    // asm: LocomotionClass_FillVar
    switch (clsid) {
    case CLSIDs::Drive:
    case CLSIDs::Tunnel:
    case CLSIDs::Walk:
    case CLSIDs::Droppod:
    case CLSIDs::Fly:
    case CLSIDs::Teleport:
    case CLSIDs::Ship:
    case CLSIDs::Jumpjet:
    case CLSIDs::Rocket:
        return true;
    default:
        return false;
    }
}

HRESULT LocomotionClass::FillVar(IStream* pStm)
{
    // asm: vt_LocomotionClass.GetMaxSize   (asm:?)
    // 先速度，再速度累积，最后目的地。
    if (!pStm) {
        return E_POINTER;
    }

    ULONG written = 0;
    if (pStm->Write(&Speed, sizeof(Speed), &written) != S_OK) return E_FAIL;
    if (pStm->Write(&SpeedAccum, sizeof(SpeedAccum), &written) != S_OK) return E_FAIL;
    if (pStm->Write(&Dest, sizeof(Dest), &written) != S_OK) return E_FAIL;
    return S_OK;
}

// ----------------------------------------------------------------------------
// asm: LocomotionClass_GetMaxSize   (asm:750425, retn 8)
//
//   HRESULT __stdcall GetMaxSize(LocomotionClass* this, uint64* pcbSize)
//   {
//       if (!pcbSize) return E_POINTER;          // mov eax,80004003h
//       int32 n = this->vtbl[0x24/4]();          // 实例字节数
//       *pcbSize = (uint64)(n + 4);              // 末尾四字节类别标记
//       return S_OK;                             // xor eax,eax
//   }
//
//   本树的 vtbl[+0x24] 对应 Size()（各派生类返回自己的 sizeof），故这里
//   直接复用 GetSizeMax：`*pcbSize = Size() + 4`，判空返回 E_POINTER。
// ----------------------------------------------------------------------------
HRESULT LocomotionClass::GetMaxSize(uint64* pcbSize)
{
    return GetSizeMax(pcbSize);
}

ULONG LocomotionClass::ppv_AddRef()
{
    // 根据游戏行为，可知 COM 通用引用计数只增不减其返回值语义：
    // 返回的总是递增后的计数。
    return ++RefCount;
}

ULONG LocomotionClass::ppv_AddRef2()
{
    // asm: LocomotionClass_ILocomotion_4B6690
    return ++RefCount;
}

int32 LocomotionClass::ILocomotion_4B6690() const
{
    // asm: LocomotionClass_ILocomotion_4B6690   (asm:411668)
    //   `or eax,0FFFFFFFFh; retn 4` —— 保留槽固定 -1（不是 0）。
    return -1;
}

// ============================================================================
// LocomotionClass::CLSIDs —— 真实 COM 类标识（asm 0x7E9A74 起连续 11 个 GUID）
//
//  asm 里每个 GetClassID 都是“判空 → 把 16 字节 GUID 原样拷进出参 → S_OK”，
//  空指针返回 0x80004003 (E_POINTER)。下表逐字段抄自 .data 段：
//      Drive    4A582741-9839-11D1-B709-00A024DDAFD1
//      Hover    4A582742-...（Data2/3/4 与 Drive 相同）
//      Tunnel   4A582743 / Walk 4A582744 / DropPod 4A582745
//      Fly      4A582746 / Teleport 4A582747
//      Mech     55D141B8-DB94-11D1-AC98-006008055BB5
//      Ship     2BEA74E1-7CCA-11D3-BE14-00104B62A16C
//      Jumpjet  92612C46-F71F-11D1-AC9F-006008055BB5
//      Rocket   7B49766-E576-11D3-9BD9-00104B972FE8
// ============================================================================
namespace {
struct GuidData {
    uint32 Data1;
    uint16 Data2;
    uint16 Data3;
    uint8  Data4[8];
};

const GuidData kLocoClsidTable[LocomotionClass::CLSIDs::Rocket + 1] = {
    { 0x4A582741u, 0x9839u, 0x11D1u, { 0xB7, 0x09, 0x00, 0xA0, 0x24, 0xDD, 0xAF, 0xD1 } }, // Drive
    { 0x4A582742u, 0x9839u, 0x11D1u, { 0xB7, 0x09, 0x00, 0xA0, 0x24, 0xDD, 0xAF, 0xD1 } }, // Hover
    { 0x4A582743u, 0x9839u, 0x11D1u, { 0xB7, 0x09, 0x00, 0xA0, 0x24, 0xDD, 0xAF, 0xD1 } }, // Tunnel
    { 0x4A582744u, 0x9839u, 0x11D1u, { 0xB7, 0x09, 0x00, 0xA0, 0x24, 0xDD, 0xAF, 0xD1 } }, // Walk
    { 0x4A582745u, 0x9839u, 0x11D1u, { 0xB7, 0x09, 0x00, 0xA0, 0x24, 0xDD, 0xAF, 0xD1 } }, // DropPod
    { 0x4A582746u, 0x9839u, 0x11D1u, { 0xB7, 0x09, 0x00, 0xA0, 0x24, 0xDD, 0xAF, 0xD1 } }, // Fly
    { 0x4A582747u, 0x9839u, 0x11D1u, { 0xB7, 0x09, 0x00, 0xA0, 0x24, 0xDD, 0xAF, 0xD1 } }, // Teleport
    { 0x55D141B8u, 0xDB94u, 0x11D1u, { 0xAC, 0x98, 0x00, 0x60, 0x08, 0x05, 0x5B, 0xB5 } }, // Mech
    { 0x2BEA74E1u, 0x7CCAu, 0x11D3u, { 0xBE, 0x14, 0x00, 0x10, 0x4B, 0x62, 0xA1, 0x6C } }, // Ship
    { 0x92612C46u, 0xF71Fu, 0x11D1u, { 0xAC, 0x9F, 0x00, 0x60, 0x08, 0x05, 0x5B, 0xB5 } }, // Jumpjet
    { 0x0B7B4976u, 0xE576u, 0x11D3u,{ 0x9B, 0xD9, 0x00, 0x10, 0x4B, 0x97, 0x2F, 0xE8 } }, // Rocket
};

const GuidData kZeroGuid = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
} // namespace

const GUID& LocomotionClass::CLSIDs::Get(int32 id)
{
    static GUID s_guid;
    const GuidData& src = (id >= 0 && id <= Rocket) ? kLocoClsidTable[id] : kZeroGuid;
    s_guid.Data1 = src.Data1;
    s_guid.Data2 = src.Data2;
    s_guid.Data3 = src.Data3;
    for (int32 i = 0; i < 8; ++i)
        s_guid.Data4[i] = src.Data4[i];
    return s_guid;
}

// ============================================================================
// CLSIDs::FromString —— rules(md).ini 的 Locomotor= 解析
//
//   Type 类（[MTNK] 等）里写的是 COM 类标识，形如
//     Locomotor={4A582741-9839-11D1-B709-00A024DDAFD1}
//   也可能被写成裸名字。这里两种都收：先做 GUID 规范化比对，再退回名字表。
//   解析失败返回 -1（调用方据此不装配行走逻辑）。
// ============================================================================
int32 LocomotionClass::CLSIDs::FromString(const char* nameOrGuid)
{
    if (nameOrGuid == nullptr || *nameOrGuid == '\0')
        return -1;

    // ---- 裸名字表（rules 里偶见，也方便内置兜底）--------------------------
    static const char* const kNames[] = {
        "Drive", "Hover", "Tunnel", "Walk", "DropPod",
        "Fly", "Teleport", "Mech", "Ship", "Jumpjet", "Rocket"
    };
    for (int32 i = 0; i <= Rocket; ++i)
    {
        const char* a = nameOrGuid;
        const char* b = kNames[i];
        while (*a != '\0' && *b != '\0')
        {
            char ca = *a, cb = *b;
            if (ca >= 'a' && ca <= 'z') ca = static_cast<char>(ca - 32);
            if (cb >= 'a' && cb <= 'z') cb = static_cast<char>(cb - 32);
            if (ca != cb)
                break;
            ++a; ++b;
        }
        if (*a == '\0' && *b == '\0')
            return i;
    }

    // ---- GUID 串比对：只比十六进制数字，忽略花括号/连字符/空白 -------------
    if (*nameOrGuid != '{' && *nameOrGuid != '(')
    {
        // 既不是名字也不是 GUID 包裹形式
        return -1;
    }

    char hex[33];
    int32 n = 0;
    for (const char* p = nameOrGuid; *p != '\0' && n < 32; ++p)
    {
        const char c = *p;
        const bool isHex = (c >= '0' && c <= '9')
                        || (c >= 'a' && c <= 'f')
                        || (c >= 'A' && c <= 'F');
        if (!isHex)
            continue;
        char u = c;
        if (u >= 'a' && u <= 'f') u = static_cast<char>(u - 32);
        hex[n++] = u;
    }
    hex[n] = '\0';
    if (n != 32)
        return -1;

    for (int32 i = 0; i <= Rocket; ++i)
    {
        const GuidData& g = kLocoClsidTable[i];
        char ref[33];
        std::snprintf(ref, sizeof(ref), "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
                      static_cast<unsigned>(g.Data1),
                      static_cast<unsigned>(g.Data2),
                      static_cast<unsigned>(g.Data3),
                      g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
                      g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
        if (std::strcmp(ref, hex) == 0)
            return i;
    }

    return -1;
}
