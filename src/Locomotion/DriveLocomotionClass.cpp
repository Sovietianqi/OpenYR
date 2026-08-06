#include "DriveLocomotionClass.h"

#include <cmath>
#include <cstdlib>

#include "../Map/MapClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Abstract/InfantryClass.h"

// ============================================================================
// DriveLocomotionClass - Vehicle driving locomotion.
// Track-based movement, speed per terrain type, turret/barrel rotation,
// body rotation, crush logic for infantry, building collision, slope
// handling, and bridge traversal.
// ============================================================================

DriveLocomotionClass::DriveLocomotionClass()
    : LocomotionClass()
    , PreviousRamp(0)
    , CurrentRamp(0)
    , Destination(0, 0, 0)
    , HeadToCoord(0, 0, 0)
    , SpeedAccum(0)
    , MovementSpeed(1.0)
    , TrackNumber(0)
    , TrackIndex(0)
    , IsOnShortTrack(false)
    , IsTurretLockedDown(0)
    , IsRotating(false)
    , IsDriving(false)
    , IsRocking(false)
    , IsLocked(false)
    , Piggybackee(nullptr)
    , TrackType(0)
    , Crusher(false)
    , Destroyer(false)
    , IsHovering(false)
    , IsAmphibious(false)
    , CurrentFacing(0)
    , TargetFacing(0)
    , RotationSpeed(8)
{
    Speed = 128;
}

DriveLocomotionClass::~DriveLocomotionClass()
{
    if (Piggybackee) {
        Piggybackee = nullptr;
    }
}

HRESULT DriveLocomotionClass::GetClassID(CLSID* pClassID)
{
    if (pClassID) {
        pClassID->Data1 = static_cast<uint32>(LocoID);
        pClassID->Data2 = 0;
        pClassID->Data3 = 0;
        for (int32 i = 0; i < 8; ++i) {
            pClassID->Data4[i] = 0;
        }
        return S_OK;
    }
    return E_FAIL;
}

int32 DriveLocomotionClass::Size()
{
    return sizeof(DriveLocomotionClass);
}

// ============================================================================
// Begin_Piggyback / End_Piggyback - Piggyback system for vehicle
// carrying (e.g., transport loading).
// ============================================================================

HRESULT DriveLocomotionClass::Begin_Piggyback(ILocomotion* pointer)
{
    Piggybackee = pointer;
    return S_OK;
}

HRESULT DriveLocomotionClass::End_Piggyback(ILocomotion** pointer)
{
    if (pointer) {
        *pointer = Piggybackee;
    }
    Piggybackee = nullptr;
    return S_OK;
}

// ============================================================================
// Move_To - Sets vehicle destination. Begins driving toward target.
// ============================================================================

void DriveLocomotionClass::Move_To(CoordStruct to)
{
    Destination = to;
    Dest = to;
    IsDriving = true;
    IsMoving = true;
    IsRotating = false;

    TargetFacing = CoordMath::DirectionTo(CurrentCoord, to);
    HeadToCoord = to;
}

// ============================================================================
// Stop_Moving - Halts vehicle movement.
// ============================================================================

void DriveLocomotionClass::Stop_Moving()
{
    IsDriving = false;
    IsMoving = false;
    IsRotating = false;
    IsRocking = false;
    SpeedAccum = 0;
}

// ============================================================================
// Do_Turn - Initiates vehicle body rotation toward a facing direction.
// ============================================================================

void DriveLocomotionClass::Do_Turn(DirStruct coord)
{
    TargetFacing = coord;
    IsRotating = true;
}

// ============================================================================
// Do_Turret_Turn - Vehicle turret rotation independent of body.
// ============================================================================

void DriveLocomotionClass::Do_Turret_Turn(DirStruct coord)
{
    if (!Owner || !Owner->HasTurret()) {
        return;
    }

    DirStruct turretFacing = Owner->GetTurretFacing();
    int32 diff = static_cast<int32>(coord.Value) - static_cast<int32>(turretFacing.Value);

    if (diff == 0) {
        return;
    }

    int32 turretSpeed = 5;
    int32 step = 0;

    if (diff < -128 || (diff > 0 && diff <= 128)) {
        step = (diff > 0) ? turretSpeed : -turretSpeed;
    } else {
        step = (diff > 0) ? -turretSpeed : turretSpeed;
    }

    int32 newVal = static_cast<int32>(turretFacing.Value) + step;
    if (newVal < 0) {
        newVal += 256;
    }
    if (newVal >= 256) {
        newVal -= 256;
    }

    if (std::abs(newVal - static_cast<int32>(coord.Value)) <= turretSpeed) {
        Owner->SetTurretFacing(coord);
    } else {
        Owner->SetTurretFacing(DirStruct(static_cast<uint8>(newVal)));
    }
}

// ============================================================================
// Process - Main vehicle movement loop. Handles rotation, driving,
// terrain checks, and crush detection.
// ============================================================================

bool DriveLocomotionClass::Process()
{
    if (IsLocked) {
        return false;
    }

    if (!IsDriving && !IsRotating && !IsRocking) {
        return false;
    }

    if (IsRocking) {
        return ProcessRocking();
    }

    if (IsRotating && !IsDriving) {
        RotateTowards(TargetFacing);
        if (CurrentFacing.Value == TargetFacing.Value) {
            IsRotating = false;
        }
        return true;
    }

    if (IsDriving) {
        if (IsRotating) {
            RotateTowards(TargetFacing);
            if (CurrentFacing.Value == TargetFacing.Value) {
                IsRotating = false;
            }
        }

        if (!IsRotating) {
            CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
            CellStruct destCell = CoordMath::CoordToCell(Destination);

            Movement_AI();

            int32 effectiveSpeed = static_cast<int32>(
                static_cast<float>(Speed) * SpeedPercentage * MovementSpeed);

            if (effectiveSpeed <= 0) {
                effectiveSpeed = 1;
            }

            SpeedAccum += effectiveSpeed;

            int32 stepThreshold = 256;
            while (SpeedAccum >= stepThreshold && IsDriving) {
                SpeedAccum -= stepThreshold;

                UpdatePosition();

                int32 dist = CurrentCoord.DistanceFrom(Destination);
                if (dist <= effectiveSpeed) {
                    CurrentCoord = Destination;
                    if (Owner) {
                        Owner->SetCoords(CurrentCoord);
                    }
                    IsDriving = false;
                    IsMoving = false;
                    break;
                }
            }

            if (IsDriving) {
                CheckCrush();
                CheckBridge();

                if (IsOnBridge() && CheckBridge()) {
                    SlopeTimer.Update(effectiveSpeed);
                }
            }
        }
    }

    if (IsDriving && Owner) {
        Owner->SetSequence(Sequence::Walk);
    }

    return IsDriving || IsRotating || IsRocking;
}

// ============================================================================
// UpdatePosition - Moves the vehicle toward its destination.
// ============================================================================

void DriveLocomotionClass::UpdatePosition()
{
    if (IsDriving && !IsRotating) {
        int32 effectiveSpeed = static_cast<int32>(
            static_cast<float>(Speed) * SpeedPercentage * MovementSpeed);

        CurrentCoord = VectorMath::MoveTowards(CurrentCoord, Destination, effectiveSpeed);

        if (Owner) {
            Owner->SetCoords(CurrentCoord);
        }
    }
}

// ============================================================================
// RotateTowards - Smoothly rotates the vehicle body toward the target facing.
// ============================================================================

void DriveLocomotionClass::RotateTowards(DirStruct targetDir)
{
    int32 diff = static_cast<int32>(targetDir.Value) - static_cast<int32>(CurrentFacing.Value);
    if (diff == 0) {
        return;
    }

    if (diff < -128 || (diff > 0 && diff <= 128)) {
        int32 step = (diff > 0) ? RotationSpeed : -RotationSpeed;
        int32 newVal = static_cast<int32>(CurrentFacing.Value) + step;
        if (newVal < 0) {
            newVal += 256;
        }
        if (newVal >= 256) {
            newVal -= 256;
        }

        if (std::abs(newVal - static_cast<int32>(targetDir.Value)) <= RotationSpeed) {
            CurrentFacing = targetDir;
        } else {
            CurrentFacing = DirStruct(static_cast<uint8>(newVal));
        }
    } else {
        int32 step = (diff > 0) ? -RotationSpeed : RotationSpeed;
        int32 newVal = static_cast<int32>(CurrentFacing.Value) + step;
        if (newVal < 0) {
            newVal += 256;
        }
        if (newVal >= 256) {
            newVal -= 256;
        }

        if (std::abs(newVal - static_cast<int32>(targetDir.Value)) <= RotationSpeed) {
            CurrentFacing = targetDir;
        } else {
            CurrentFacing = DirStruct(static_cast<uint8>(newVal));
        }
    }

    if (Owner) {
        Owner->SetFacing(CurrentFacing);
    }
}

// ============================================================================
// CheckCrush - Checks if the vehicle is crushing infantry in its path.
// ============================================================================

void DriveLocomotionClass::CheckCrush()
{
    if (!Crusher && !Destroyer) {
        return;
    }

    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);

    for (int32 dx = -1; dx <= 1; ++dx) {
        for (int32 dy = -1; dy <= 1; ++dy) {
            CellStruct cell;
            cell.X = static_cast<int16>(static_cast<int32>(currentCell.X) + dx);
            cell.Y = static_cast<int16>(static_cast<int32>(currentCell.Y) + dy);

            if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
                continue;
            }

            ObjectClass* occupant = MapClass::Instance->GetCellOccupier(cell);
            if (!occupant) {
                continue;
            }

            if (occupant->WhatAmI() == AbstractType::Infantry) {
                InfantryClass* infantry = static_cast<InfantryClass*>(occupant);
                if (Destroyer) {
                    infantry->TakeDamage(10000, static_cast<ObjectClass*>(nullptr), WarheadTypeClass::GetDefault());
                } else if (Crusher) {
                    infantry->TakeDamage(250, Owner, WarheadTypeClass::GetDefault());
                }
            }
        }
    }
}

// ============================================================================
// CheckBuildingCollision - Checks for and handles building collisions.
// ============================================================================

bool DriveLocomotionClass::CheckBuildingCollision()
{
    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);

    for (int32 dx = -1; dx <= 1; ++dx) {
        for (int32 dy = -1; dy <= 1; ++dy) {
            CellStruct cell;
            cell.X = static_cast<int16>(static_cast<int32>(currentCell.X) + dx);
            cell.Y = static_cast<int16>(static_cast<int32>(currentCell.Y) + dy);

            if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
                continue;
            }

            if (MapClass::Instance->IsCellOccupied(cell)) {
                ObjectClass* occupant = MapClass::Instance->GetCellOccupier(cell);
                if (occupant && occupant->WhatAmI() == AbstractType::Building) {
                    if (Destroyer) {
                        return true;
                    }
                    return !Crusher;
                }
            }
        }
    }
    return false;
}

// ============================================================================
// IsOnBridge - Checks if the vehicle is currently on a bridge.
// ============================================================================

bool DriveLocomotionClass::IsOnBridge() const
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }
    return MapClass::Instance->IsBridgeCell(cell);
}

// ============================================================================
// CheckBridge - Validates bridge integrity and handles destruction.
// ============================================================================

bool DriveLocomotionClass::CheckBridge()
{
    if (!IsOnBridge()) {
        return false;
    }

    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    if (MapClass::Instance->IsBridgeDestroyed(cell)) {
        IsDriving = false;
        IsMoving = false;
        IsRocking = true;
        return true;
    }

    return false;
}

// ============================================================================
// ProcessRocking - Handles vehicle rocking animation after bridge collapse.
// ============================================================================

bool DriveLocomotionClass::ProcessRocking()
{
    int32 rockTimer = 30;
    static int32 rockPhase = 0;

    if (rockPhase < rockTimer) {
        ++rockPhase;
        if (Owner) {
            Owner->SetSequence(Sequence::Tumble);
        }
        return true;
    }

    rockPhase = 0;
    IsRocking = false;
    return false;
}

// ============================================================================
// Can_Enter_Cell - Vehicle-specific cell traversal check.
// ============================================================================

Move DriveLocomotionClass::Can_Enter_Cell(CellStruct cell)
{
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return Move::No;
    }

    LandType land = MapClass::Instance->GetLandType(cell);

    if (IsAmphibious) {
        if (land == LandType::Wall) {
            return Move::No;
        }
        return Move::OK;
    }

    if (IsHovering) {
        if (land == LandType::Wall || land == LandType::Rock) {
            return Move::No;
        }
        return Move::OK;
    }

    if (land == LandType::Water || land == LandType::Wall) {
        return Move::No;
    }

    if (land == LandType::Rock && !Destroyer) {
        return Move::No;
    }

    return Move::OK;
}

// ============================================================================
// Mark_All_Occupation_Bits - Vehicle footprint marking.
// ============================================================================

void DriveLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    bool occupy = (mark == MarkType::Up);

    MapClass::Instance->MarkCellOccupied(cell, occupy);

    for (int32 dx = -1; dx <= 1; ++dx) {
        for (int32 dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            CellStruct adj;
            adj.X = static_cast<int16>(static_cast<int32>(cell.X) + dx);
            adj.Y = static_cast<int16>(static_cast<int32>(cell.Y) + dy);
            if (adj.X >= 0 && adj.X < 512 && adj.Y >= 0 && adj.Y < 512) {
                MapClass::Instance->MarkCellOccupied(adj, occupy);
            }
        }
    }
}

// ============================================================================
// Limbo - Vehicle limbo state.
// ============================================================================

void DriveLocomotionClass::Limbo()
{
    Mark_All_Occupation_Bits(MarkType::Down);
    IsDriving = false;
    IsMoving = false;
    IsRotating = false;
    IsRocking = false;
    SpeedAccum = 0;
}

// ============================================================================
// Force_New_Slope - Vehicle slope handling with speed penalty.
// ============================================================================

void DriveLocomotionClass::Force_New_Slope(int32 ramp)
{
    PreviousRamp = CurrentRamp;
    CurrentRamp = static_cast<uint32>(ramp);

    if (PreviousRamp != CurrentRamp) {
        if (ramp > 0) {
            SlopeTimer.SetRate(60);
            MovementSpeed = 1.0 - static_cast<double>(ramp) * 0.08;
            if (MovementSpeed < 0.3) {
                MovementSpeed = 0.3;
            }
        } else {
            MovementSpeed = 1.0;
        }
    }
}

// ============================================================================
// Movement_AI - Vehicle movement AI with terrain and slope awareness.
// ============================================================================

void DriveLocomotionClass::Movement_AI()
{
    if (!IsDriving) {
        return;
    }

    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    LandType land = MapClass::Instance->GetLandType(currentCell);

    float baseMod = 1.0f;
    switch (land) {
        case LandType::Road:
            baseMod = 1.1f;
            break;
        case LandType::Rough:
            baseMod = 0.7f;
            break;
        case LandType::Ice:
            baseMod = 0.75f;
            break;
        case LandType::Weeds:
            baseMod = 0.8f;
            break;
        case LandType::Tiberium:
            baseMod = 0.65f;
            break;
        case LandType::Beach:
            baseMod = 0.9f;
            break;
        case LandType::Water:
            if (IsAmphibious) {
                baseMod = 0.6f;
            } else {
                IsDriving = false;
                IsMoving = false;
                return;
            }
            break;
        default:
            baseMod = 1.0f;
            break;
    }

    SpeedPercentage = baseMod;

    int32 slope = MapClass::Instance->GetCellSlope(currentCell);
    if (slope > 0) {
        SpeedPercentage *= 1.0f - static_cast<float>(slope) * 0.05f;
        if (SpeedPercentage < 0.2f) {
            SpeedPercentage = 0.2f;
        }
    }

    if (Is_Moving_On_Bridge() && Is_Bridge_Destroyed()) {
        IsDriving = false;
        IsMoving = false;
    }
}

// ============================================================================
// Get_Status - Vehicle movement status code.
// ============================================================================

int32 DriveLocomotionClass::Get_Status() const
{
    if (IsRocking) {
        return static_cast<int32>(Sequence::Tumble);
    }
    if (IsRotating) {
        return static_cast<int32>(Sequence::Ready);
    }
    if (IsDriving) {
        return static_cast<int32>(Sequence::Walk);
    }
    return static_cast<int32>(Sequence::Ready);
}

// ============================================================================
// Is_Really_Moving_Now - Vehicle is moving if actually driving.
// ============================================================================

bool DriveLocomotionClass::Is_Really_Moving_Now() const
{
    return IsDriving && !IsRotating && !IsRocking;
}

// ============================================================================
// Can_Fire - Vehicle can fire unless rotating (turret lock).
// ============================================================================

FireError DriveLocomotionClass::Can_Fire() const
{
    if (IsTurretLockedDown > 0) {
        return FireError::ROT;
    }
    if (IsRocking) {
        return FireError::Movement;
    }
    return FireError::OK;
}

// ============================================================================
// Force_Track - Forces the vehicle onto a specific track.
// ============================================================================

void DriveLocomotionClass::Force_Track(int32 track, CoordStruct coord)
{
    TrackNumber = static_cast<uint32>(track);
    TrackIndex = track;
    IsOnShortTrack = (track < 0);

    Dest = coord;
    Destination = coord;
    IsDriving = true;
    IsMoving = true;
    SpeedAccum = 0;
}

// ============================================================================
// Get_Track_Number - Returns the current track number.
// ============================================================================

int32 DriveLocomotionClass::Get_Track_Number() const
{
    return static_cast<int32>(TrackNumber);
}

// ============================================================================
// Get_Track_Index - Returns the current track index.
// ============================================================================

int32 DriveLocomotionClass::Get_Track_Index() const
{
    return TrackIndex;
}

// ============================================================================
// Lock / Unlock - Vehicle lock state management.
// ============================================================================

void DriveLocomotionClass::Lock()
{
    IsLocked = true;
    IsDriving = false;
    IsMoving = false;
}

void DriveLocomotionClass::Unlock()
{
    IsLocked = false;
}

// ============================================================================
// Tilt_Pitch_AI - Vehicle pitch adjustment for slopes.
// ============================================================================

void DriveLocomotionClass::Tilt_Pitch_AI()
{
    int32 slope = MapClass::Instance->GetCellSlope(CoordMath::CoordToCell(CurrentCoord));
    if (slope == 0) {
        return;
    }

    if (Owner) {
        int32 pitch = slope * 3;
        if (pitch > 20) {
            pitch = 20;
        }
        if (pitch < -20) {
            pitch = -20;
        }
        Owner->SetPitch(pitch);
    }
}