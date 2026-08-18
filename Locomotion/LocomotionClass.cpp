#include "LocomotionClass.h"

#include <cmath>
#include <cstdlib>

#include "../Map/MapClass.h"
#include "../Rules/RulesClass.h"
#include "../Houses/HouseClass.h"

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
// Process - Main movement loop. Called each game frame.
// In the base class, this handles the core movement logic:
// accumulating speed toward the destination with terrain-aware checks.
// Derived classes override this for specific movement patterns.
// ============================================================================

bool LocomotionClass::Process()
{
    if (!IsMoving) {
        return false;
    }

    if (!Powered) {
        IsMoving = false;
        return false;
    }

    int32 effectiveSpeed = static_cast<int32>(static_cast<float>(Speed) * SpeedPercentage);
    if (effectiveSpeed <= 0) {
        effectiveSpeed = 1;
    }

    SpeedAccum += effectiveSpeed;

    int32 stepThreshold = 256;
    if (SpeedAccum < stepThreshold) {
        return true;
    }

    while (SpeedAccum >= stepThreshold && IsMoving) {
        SpeedAccum -= stepThreshold;

        CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
        CellStruct destCell = CoordMath::CoordToCell(Dest);

        if (Can_Enter_Cell(destCell) == Move::No) {
            CoordStruct nearest = GetClosestOkCell(Dest);
            if (nearest == CoordStruct(0, 0, 0)) {
                IsMoving = false;
                return false;
            }
            Dest = nearest;
            destCell = CoordMath::CoordToCell(nearest);
        }

        int32 totalDist = CoordMath::CoordDistance(CurrentCoord, Dest);
        if (totalDist <= effectiveSpeed) {
            CurrentCoord = Dest;
            IsMoving = false;
            SpeedAccum = 0;
            return false;
        }

        CurrentCoord = VectorMath::MoveTowards(CurrentCoord, Dest, effectiveSpeed);
    }

    return IsMoving;
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
// Stop_Moving - Halts all movement immediately.
// ============================================================================

void LocomotionClass::Stop_Moving()
{
    IsMoving = false;
    SpeedAccum = 0;
}

// ============================================================================
// Do_Turn - Handles body rotation toward a facing direction.
// Base implementation is a no-op; derived classes handle rotation.
// ============================================================================

void LocomotionClass::Do_Turn(DirStruct coord)
{
    // Base implementation: store the requested facing on the owner so
    // that derived classes which track body rotation (Drive, Walk, Ship)
    // can interpolate from the current value. The base class performs an
    // immediate snap rather than a gradual turn; derived classes override
    // this to apply turn-rate-limited rotation.
    if (Owner) {
        Owner->SetFacing(coord);
    }
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
// Head_To_Coord - Returns the coordinate the unit is heading toward.
// ============================================================================

CoordStruct LocomotionClass::Head_To_Coord() const
{
    return Dest;
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
// Can_Enter_Cell - Checks if a cell is passable. Base returns always OK.
// Derived classes override with terrain/occupancy checks.
// ============================================================================

Move LocomotionClass::Can_Enter_Cell(CellStruct cell)
{
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return Move::No;
    }
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
// Mark_All_Occupation_Bits - Marks or clears occupation bits for the
// unit's footprint on the map. Used for pathfinding and collision.
// ============================================================================

void LocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    if (!Owner) {
        return;
    }

    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    if (mark == MarkType::Up) {
        MapClass::Instance->MarkCellOccupied(cell, true);

        for (int32 dx = -1; dx <= 1; ++dx) {
            for (int32 dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                CellStruct adj;
                adj.X = static_cast<int16>(static_cast<int32>(cell.X) + dx);
                adj.Y = static_cast<int16>(static_cast<int32>(cell.Y) + dy);
                if (adj.X >= 0 && adj.X < 512 && adj.Y >= 0 && adj.Y < 512) {
                    MapClass::Instance->MarkCellOccupied(adj, true);
                }
            }
        }
    } else {
        MapClass::Instance->MarkCellOccupied(cell, false);

        for (int32 dx = -1; dx <= 1; ++dx) {
            for (int32 dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                CellStruct adj;
                adj.X = static_cast<int16>(static_cast<int32>(cell.X) + dx);
                adj.Y = static_cast<int16>(static_cast<int32>(cell.Y) + dy);
                if (adj.X >= 0 && adj.X < 512 && adj.Y >= 0 && adj.Y < 512) {
                    MapClass::Instance->MarkCellOccupied(adj, false);
                }
            }
        }
    }
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
// Unlimbo - Restores the unit from limbo state.
// ============================================================================

void LocomotionClass::Unlimbo()
{
    Mark_All_Occupation_Bits(MarkType::Up);
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
// Push - Attempts to push the unit in a direction (for physics collisions).
// ============================================================================

bool LocomotionClass::Push(DirStruct dir)
{
    if (!Owner) {
        return false;
    }

    auto delta = Facing::GetMovementDelta(dir);
    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    CellStruct targetCell;
    targetCell.X = static_cast<int16>(static_cast<int32>(currentCell.X) + delta.first);
    targetCell.Y = static_cast<int16>(static_cast<int32>(currentCell.Y) + delta.second);

    if (Can_Enter_Cell(targetCell) == Move::OK) {
        CoordStruct newCoord = CoordMath::CellToCoord(targetCell);
        CurrentCoord = newCoord;
        if (Owner) {
            Owner->SetCoords(newCoord);
        }
        return true;
    }

    return false;
}

// ============================================================================
// Shove - Stronger push that ignores some terrain restrictions.
// ============================================================================

bool LocomotionClass::Shove(DirStruct dir)
{
    if (!Owner) {
        return false;
    }

    auto delta = Facing::GetMovementDelta(dir);
    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    CellStruct targetCell;
    targetCell.X = static_cast<int16>(static_cast<int32>(currentCell.X) + delta.first * 2);
    targetCell.Y = static_cast<int16>(static_cast<int32>(currentCell.Y) + delta.second * 2);

    if (targetCell.X >= 0 && targetCell.X < 512 && targetCell.Y >= 0 && targetCell.Y < 512) {
        CoordStruct newCoord = CoordMath::CellToCoord(targetCell);
        CurrentCoord = newCoord;
        if (Owner) {
            Owner->SetCoords(newCoord);
        }
        return true;
    }

    return false;
}

// ============================================================================
// Force_Track - Forces the unit onto a specific track at a coordinate.
// ============================================================================

void LocomotionClass::Force_Track(int32 track, CoordStruct coord)
{
    Dest = coord;
    IsMoving = true;
    SpeedAccum = 0;
}

// ============================================================================
// Force_Immediate_Destination - Instantly sets and snaps to destination.
// ============================================================================

void LocomotionClass::Force_Immediate_Destination(CoordStruct coord)
{
    Dest = coord;
    CurrentCoord = coord;
    IsMoving = false;
    SpeedAccum = 0;

    if (Owner) {
        Owner->SetCoords(coord);
    }
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
// Apparent_Speed - Returns the speed as it appears to the game logic.
// ============================================================================

int32 LocomotionClass::Apparent_Speed() const
{
    return static_cast<int32>(static_cast<float>(Speed) * SpeedPercentage);
}

// ============================================================================
// Drawing_Code - Returns the drawing code for the locomotion type.
// The code controls which render state the voxel/shape drawer uses:
//   0 = static (idle, no movement)
//   1 = moving (translating toward a destination)
//   2 = turning (rotating in place without translation)
// The base class only tracks the IsMoving flag, so it can distinguish
// between static and moving. Turning (code 2) is reported by derived
// classes that track body rotation (e.g. DriveLocomotionClass::IsRotating);
// the base class has no turn state and therefore never reports 2.
// ============================================================================

int32 LocomotionClass::Drawing_Code() const
{
    if (!Powered) {
        return 0;
    }

    if (IsMoving) {
        return 1;
    }

    return 0;
}

// ============================================================================
// Can_Fire - Checks if the unit can fire while moving.
// ============================================================================

FireError LocomotionClass::Can_Fire() const
{
    if (!Powered) {
        return FireError::Ill;
    }
    return FireError::OK;
}

// ============================================================================
// Get_Status - Returns the movement/status code for display.
// ============================================================================

int32 LocomotionClass::Get_Status() const
{
    if (!Powered) {
        return static_cast<int32>(Sequence::Ready);
    }
    if (IsMoving) {
        return static_cast<int32>(Sequence::Walk);
    }
    return static_cast<int32>(Sequence::Ready);
}

// ============================================================================
// Acquire_Hunter_Seeker_Target - AI targeting for hunter-seeker behavior.
// ============================================================================

void LocomotionClass::Acquire_Hunter_Seeker_Target()
{
    if (!Owner) {
        return;
    }

    HouseClass* ownerHouse = Owner->GetOwningHouse();
    if (!ownerHouse) {
        return;
    }

    CoordStruct bestTarget = CurrentCoord;
    int64 bestDistSq = 0;
    bool found = false;

    for (int32 h = 0; h < MAX_HOUSES; ++h) {
        HouseClass* enemyHouse = HouseClass::GetHouseByIndex(h);
        if (!enemyHouse || enemyHouse == ownerHouse || enemyHouse->IsAlliedWith(ownerHouse)) {
            continue;
        }

        DynamicVectorClass<TechnoClass*>* enemyUnits = enemyHouse->GetTechnos();
        if (!enemyUnits) {
            continue;
        }

        for (int32 i = 0; i < enemyUnits->Count; ++i) {
            TechnoClass* enemy = (*enemyUnits)[i];
            if (!enemy || !enemy->IsActive() || enemy->IsInAir()) {
                continue;
            }

            CoordStruct enemyPos = enemy->GetCoords();
            int64 distSq = CurrentCoord.DistanceSquaredFrom(enemyPos);

            if (!found || distSq < bestDistSq) {
                bestDistSq = distSq;
                bestTarget = enemyPos;
                found = true;
            }
        }
    }

    if (found) {
        Move_To(bestTarget);
    }
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
// Lock - Locks the unit in place.
// ============================================================================

void LocomotionClass::Lock()
{
    IsMoving = false;
    SpeedAccum = 0;
}

// ============================================================================
// Unlock - Unlocks the unit, allowing movement to resume.
// ============================================================================

void LocomotionClass::Unlock()
{
    // Reverse the effect of Lock(): reset the speed accumulator and
    // resume movement toward the pending destination if the unit has
    // not yet reached it. If the unit is already at its destination
    // (or has no destination), it remains stationary.
    SpeedAccum = 0;

    int32 distance = CoordMath::CoordDistance(CurrentCoord, Dest);
    if (distance > 0 && Powered) {
        IsMoving = true;
    }
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
// Get_Speed_Accum - Returns the speed accumulator value.
// ============================================================================

int32 LocomotionClass::Get_Speed_Accum() const
{
    return SpeedAccum;
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
