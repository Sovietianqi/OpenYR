#include "JumpjetLocomotionClass.h"

#include <cmath>
#include <cstdlib>

#include "../Map/MapClass.h"

// ============================================================================
// JumpjetLocomotionClass - Jumpjet infantry movement.
// Jump arc calculation, landing zone finding, in-air vs ground state
// transitions, parachute deployment if falling, and piggyback support.
// ============================================================================

JumpjetLocomotionClass::JumpjetLocomotionClass()
    : LocomotionClass()
    , TurnRate(8)
    , JumpSpeed(128)
    , Climb(0.5f)
    , CruiseHeight(300.0f)
    , HoverHeight(150.0f)
    , CurrentState(Grounded)
    , IsCrashing(false)
    , JumpDirection(0)
    , MovingDestination(0, 0, 0)
    , Altitude(0)
    , TargetAltitude(0)
    , Piggybackee(nullptr)
    , CrashTimer(0)
{
    Speed = 128;
}

JumpjetLocomotionClass::~JumpjetLocomotionClass()
{
    if (Piggybackee) {
        Piggybackee = nullptr;
    }
}

HRESULT JumpjetLocomotionClass::GetClassID(CLSID* pClassID)
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

int32 JumpjetLocomotionClass::Size()
{
    return sizeof(JumpjetLocomotionClass);
}

// ============================================================================
// Begin_Piggyback / End_Piggyback - Piggyback system for jumpjet carrying.
// ============================================================================

HRESULT JumpjetLocomotionClass::Begin_Piggyback(ILocomotion* pointer)
{
    Piggybackee = pointer;
    return S_OK;
}

HRESULT JumpjetLocomotionClass::End_Piggyback(ILocomotion** pointer)
{
    if (pointer) {
        *pointer = Piggybackee;
    }
    Piggybackee = nullptr;
    return S_OK;
}

// ============================================================================
// Is_Moving - Jumpjet is moving unless grounded and idle.
// ============================================================================

bool JumpjetLocomotionClass::Is_Moving()
{
    return IsMoving || CurrentState == Cruising || CurrentState == Ascending
        || CurrentState == Descending || CurrentState == Hovering;
}

// ============================================================================
// Destination - Returns the jumpjet's current destination.
// ============================================================================

CoordStruct JumpjetLocomotionClass::Destination()
{
    return MovingDestination;
}

// ============================================================================
// Move_To - Sets jumpjet destination. Initiates jump if grounded.
// ============================================================================

void JumpjetLocomotionClass::Move_To(CoordStruct to)
{
    MovingDestination = to;
    Dest = to;
    IsMoving = true;

    if (CurrentState == Grounded) {
        Jump();
    }

    DirStruct facing = CoordMath::DirectionTo(CurrentCoord, to);
    JumpDirection = facing;
}

// ============================================================================
// Stop_Moving - Stops jumpjet movement. Initiates descent if airborne.
// ============================================================================

void JumpjetLocomotionClass::Stop_Moving()
{
    IsMoving = false;

    if (CurrentState == Cruising || CurrentState == Hovering) {
        FindLandingZone();
        CurrentState = Descending;
        TargetAltitude = 0;
    }
}

// ============================================================================
// Do_Turn - Jumpjet turning. Handles rotation in air.
// ============================================================================

void JumpjetLocomotionClass::Do_Turn(DirStruct coord)
{
    if (Owner) {
        DirStruct currentFacing = Owner->GetFacing();
        int32 diff = static_cast<int32>(coord.Value) - static_cast<int32>(currentFacing.Value);

        if (diff == 0) {
            return;
        }

        int32 step = 0;
        if (diff < -128 || (diff > 0 && diff <= 128)) {
            step = (diff > 0) ? TurnRate : -TurnRate;
        } else {
            step = (diff > 0) ? -TurnRate : TurnRate;
        }

        int32 newVal = static_cast<int32>(currentFacing.Value) + step;
        if (newVal < 0) {
            newVal += 256;
        }
        if (newVal >= 256) {
            newVal -= 256;
        }

        if (std::abs(newVal - static_cast<int32>(coord.Value)) <= TurnRate) {
            Owner->SetFacing(coord);
        } else {
            Owner->SetFacing(DirStruct(static_cast<uint8>(newVal)));
        }
    }
}

// ============================================================================
// Process - Main jumpjet movement loop. State machine for all jumpjet states.
// ============================================================================

bool JumpjetLocomotionClass::Process()
{
    if (IsCrashing) {
        return ProcessCrash();
    }

    switch (CurrentState) {
        case Grounded:
            return ProcessGrounded();
        case Ascending:
            return ProcessAscending();
        case Cruising:
            return ProcessCruising();
        case Hovering:
            return ProcessHovering();
        case Descending:
            return ProcessDescending();
        case Crashing:
            return ProcessCrash();
        default:
            break;
    }

    return IsMoving || CurrentState != Grounded;
}

// ============================================================================
// ProcessGrounded - Jumpjet is on the ground.
// ============================================================================

bool JumpjetLocomotionClass::ProcessGrounded()
{
    Altitude = 0;
    CurrentCoord.Z = 0;

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Ready);
    }

    return false;
}

// ============================================================================
// ProcessAscending - Jumpjet is climbing to cruising altitude.
// ============================================================================

bool JumpjetLocomotionClass::ProcessAscending()
{
    int32 climbRate = static_cast<int32>(Climb * 50.0f);
    Altitude += climbRate;

    if (Altitude >= static_cast<int32>(CruiseHeight)) {
        Altitude = static_cast<int32>(CruiseHeight);
        TargetAltitude = Altitude;
        CurrentState = Cruising;
    }

    CurrentCoord.Z = Altitude;

    if (Owner) {
        Do_Turn(JumpDirection);
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Fly);
    }

    return true;
}

// ============================================================================
// ProcessCruising - Jumpjet is flying at cruising altitude.
// ============================================================================

bool JumpjetLocomotionClass::ProcessCruising()
{
    if (!IsMoving) {
        CurrentState = Hovering;
        return true;
    }

    int32 moveSpeed = JumpSpeed;
    int32 effectiveSpeed = static_cast<int32>(static_cast<float>(moveSpeed) * SpeedPercentage);

    if (Owner) {
        DirStruct facing = CoordMath::DirectionTo(CurrentCoord, MovingDestination);
        Do_Turn(facing);
    }

    CurrentCoord = VectorMath::MoveTowards(CurrentCoord, MovingDestination, effectiveSpeed);
    CurrentCoord.Z = Altitude;

    int32 dist = CurrentCoord.DistanceFrom(MovingDestination);
    if (dist <= effectiveSpeed) {
        CurrentCoord = MovingDestination;
        CurrentCoord.Z = Altitude;
        CurrentState = Hovering;
        IsMoving = false;
    }

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Fly);
    }

    return true;
}

// ============================================================================
// ProcessHovering - Jumpjet is hovering in place.
// ============================================================================

bool JumpjetLocomotionClass::ProcessHovering()
{
    if (IsMoving) {
        CurrentState = Cruising;
        return true;
    }

    if (Altitude != static_cast<int32>(HoverHeight)) {
        if (Altitude > static_cast<int32>(HoverHeight)) {
            Altitude -= 5;
            if (Altitude < static_cast<int32>(HoverHeight)) {
                Altitude = static_cast<int32>(HoverHeight);
            }
        } else {
            Altitude += 5;
            if (Altitude > static_cast<int32>(HoverHeight)) {
                Altitude = static_cast<int32>(HoverHeight);
            }
        }
    }

    CurrentCoord.Z = Altitude;

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::IdleFly);
    }

    return true;
}

// ============================================================================
// ProcessDescending - Jumpjet is descending to ground.
// ============================================================================

bool JumpjetLocomotionClass::ProcessDescending()
{
    int32 descendRate = static_cast<int32>(Climb * 50.0f);
    Altitude -= descendRate;

    if (Altitude <= 0) {
        Altitude = 0;
        CurrentCoord.Z = 0;
        CurrentState = Grounded;

        if (!IsValidLandingCell()) {
            Crash();
            return true;
        }

        if (Owner) {
            Owner->SetCoords(CurrentCoord);
            Owner->SetSequence(Sequence::Ready);
        }

        return false;
    }

    CurrentCoord.Z = Altitude;

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Fly);
    }

    return true;
}

// ============================================================================
// ProcessCrash - Jumpjet is crashing.
// ============================================================================

bool JumpjetLocomotionClass::ProcessCrash()
{
    --CrashTimer;

    Altitude -= 30;
    if (Altitude < 0) {
        Altitude = 0;
    }

    CurrentCoord.Z = Altitude;

    if (CrashTimer <= 0) {
        IsCrashing = false;
        CurrentState = Grounded;
        IsMoving = false;
        Altitude = 0;
        CurrentCoord.Z = 0;

        if (Owner) {
            Owner->SetCoords(CurrentCoord);
            Owner->SetSequence(Sequence::Prone);
        }

        return false;
    }

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Tumble);
    }

    return true;
}

// ============================================================================
// IsValidLandingCell - Checks if the current cell is valid for landing.
// ============================================================================

bool JumpjetLocomotionClass::IsValidLandingCell() const
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);

    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }

    LandType land = MapClass::Instance->GetLandType(cell);
    if (land == LandType::Water || land == LandType::Wall || land == LandType::Rock) {
        return false;
    }

    return true;
}

// ============================================================================
// FindLandingZone - Finds the closest valid landing zone.
// ============================================================================

void JumpjetLocomotionClass::FindLandingZone()
{
    if (IsValidLandingCell()) {
        return;
    }

    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    for (int32 radius = 1; radius <= 16; ++radius) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            for (int32 dy = -radius; dy <= radius; ++dy) {
                if (std::abs(dx) != radius && std::abs(dy) != radius) {
                    continue;
                }
                CellStruct cs;
                cs.X = static_cast<int16>(static_cast<int32>(currentCell.X) + dx);
                cs.Y = static_cast<int16>(static_cast<int32>(currentCell.Y) + dy);

                if (cs.X < 0 || cs.X >= 512 || cs.Y < 0 || cs.Y >= 512) {
                    continue;
                }

                LandType land = MapClass::Instance->GetLandType(cs);
                if (land != LandType::Water && land != LandType::Wall && land != LandType::Rock) {
                    MovingDestination = CoordMath::CellToCoord(cs);
                    CurrentCoord = MovingDestination;
                    return;
                }
            }
        }
    }
}

// ============================================================================
// Mark_All_Occupation_Bits - Jumpjet occupation marking.
// ============================================================================

void JumpjetLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    if (CurrentState == Grounded || CurrentState == Descending) {
        CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
        bool occupy = (mark == MarkType::Up);
        MapClass::Instance->MarkCellOccupied(cell, occupy);
    }
}

// ============================================================================
// Limbo - Jumpjet limbo state.
// ============================================================================

void JumpjetLocomotionClass::Limbo()
{
    IsMoving = false;
    CurrentState = Grounded;
    Altitude = 0;
    IsCrashing = false;
    TargetAltitude = 0;
}

// ============================================================================
// Hover - Puts the jumpjet into hovering state.
// ============================================================================

void JumpjetLocomotionClass::Hover()
{
    if (CurrentState == Grounded) {
        Jump();
    }
    if (CurrentState == Cruising) {
        CurrentState = Hovering;
    }
}

// ============================================================================
// Jump - Initiates the jump sequence from ground to air.
// ============================================================================

void JumpjetLocomotionClass::Jump()
{
    if (CurrentState != Grounded) {
        return;
    }

    CurrentState = Ascending;
    TargetAltitude = static_cast<int32>(CruiseHeight);
    Altitude = 0;
}

// ============================================================================
// Crash - Forces the jumpjet into crash state.
// ============================================================================

void JumpjetLocomotionClass::Crash()
{
    IsCrashing = true;
    CrashTimer = 60;
    CurrentState = Crashing;
}

// ============================================================================
// DeployParachute - Deploys parachute if falling (safety feature).
// ============================================================================

void JumpjetLocomotionClass::DeployParachute()
{
    if (CurrentState == Descending || CurrentState == Crashing) {
        Climb = 0.25f;
        if (Owner) {
            Owner->SetSequence(Sequence::Paradrop);
        }
    }
}

// ============================================================================
// Get_Status - Jumpjet movement status code.
// ============================================================================

int32 JumpjetLocomotionClass::Get_Status() const
{
    switch (CurrentState) {
        case Ascending:
        case Cruising:
            return static_cast<int32>(Sequence::Fly);
        case Hovering:
            return static_cast<int32>(Sequence::IdleFly);
        case Descending:
            return static_cast<int32>(Sequence::Fly);
        case Crashing:
            return static_cast<int32>(Sequence::Tumble);
        case Grounded:
        default:
            return static_cast<int32>(Sequence::Ready);
    }
}

// ============================================================================
// Can_Fire - Jumpjet can fire in most states except crashing.
// ============================================================================

FireError JumpjetLocomotionClass::Can_Fire() const
{
    if (IsCrashing || CurrentState == Crashing) {
        return FireError::Movement;
    }
    return FireError::OK;
}

// ============================================================================
// Is_Really_Moving_Now - Jumpjet is moving if airborne.
// ============================================================================

bool JumpjetLocomotionClass::Is_Really_Moving_Now() const
{
    return CurrentState != Grounded && !IsCrashing;
}

// ============================================================================
// GetAltitude - Returns the current altitude of the jumpjet.
// ============================================================================

int32 JumpjetLocomotionClass::GetAltitude() const
{
    return Altitude;
}