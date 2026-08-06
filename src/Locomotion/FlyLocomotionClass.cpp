#include "FlyLocomotionClass.h"

#include <cmath>
#include <cstdlib>

#include "../Map/MapClass.h"

// ============================================================================
// FlyLocomotionClass - Aircraft/helicopter movement.
// Altitude changes, landing/takeoff sequences, docking at helipads/airfields,
// formation flying, shadow projection, and speed management.
// ============================================================================

FlyLocomotionClass::FlyLocomotionClass()
    : LocomotionClass()
    , AirportBound(false)
    , MovingDestination(0, 0, 0)
    , XYZ2(0, 0, 0)
    , HasMoveOrder(false)
    , FlightLevel(300)
    , TargetSpeed(200.0)
    , CurrentSpeed(0.0)
    , IsTakingOff(false)
    , IsLanding(false)
    , WasLanding(false)
    , unknown_bool_53(false)
    , unknown_54(0)
    , unknown_58(0)
    , IsElevating(false)
    , unknown_bool_5D(false)
    , unknown_bool_5E(false)
    , unknown_bool_5F(false)
    , Altitude(0)
    , DockTarget(nullptr)
    , LandingDirection(0)
    , TakeoffTimer(0)
    , LandingTimer(0)
{
    Speed = 200;
}

FlyLocomotionClass::~FlyLocomotionClass()
{
    DockTarget = nullptr;
}

HRESULT FlyLocomotionClass::GetClassID(CLSID* pClassID)
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

int32 FlyLocomotionClass::Size()
{
    return sizeof(FlyLocomotionClass);
}

// ============================================================================
// Is_Moving - Aircraft is moving if it has a move order or is transitioning.
// ============================================================================

bool FlyLocomotionClass::Is_Moving()
{
    return HasMoveOrder || IsTakingOff || IsLanding || IsElevating;
}

// ============================================================================
// Destination - Returns the aircraft's current destination.
// ============================================================================

CoordStruct FlyLocomotionClass::Destination()
{
    return MovingDestination;
}

// ============================================================================
// Move_To - Sets aircraft destination. Initiates takeoff if grounded.
// ============================================================================

void FlyLocomotionClass::Move_To(CoordStruct to)
{
    MovingDestination = to;
    HasMoveOrder = true;
    IsMoving = true;
    Dest = to;

    if (!IsTakingOff && !IsLanding && Altitude == 0) {
        TakeOff();
    }
}

// ============================================================================
// Stop_Moving - Halts aircraft movement. May initiate landing.
// ============================================================================

void FlyLocomotionClass::Stop_Moving()
{
    HasMoveOrder = false;
    IsMoving = false;

    if (Altitude > 0 && !IsLanding && !IsTakingOff) {
        Land();
    }
}

// ============================================================================
// Do_Turn - Aircraft turning. Aircraft turn instantly in the air.
// ============================================================================

void FlyLocomotionClass::Do_Turn(DirStruct coord)
{
    if (Owner) {
        Owner->SetFacing(coord);
    }
}

// ============================================================================
// Process - Main aircraft movement loop. Handles takeoff, flight, and landing.
// ============================================================================

bool FlyLocomotionClass::Process()
{
    if (IsTakingOff) {
        return ProcessTakeoff();
    }

    if (IsLanding) {
        return ProcessLanding();
    }

    if (IsElevating) {
        return ProcessElevation();
    }

    if (HasMoveOrder) {
        return ProcessFlight();
    }

    return false;
}

// ============================================================================
// ProcessTakeoff - Handles the takeoff sequence.
// ============================================================================

bool FlyLocomotionClass::ProcessTakeoff()
{
    --TakeoffTimer;

    float progress = 1.0f - static_cast<float>(TakeoffTimer) / 15.0f;
    Altitude = static_cast<int32>(static_cast<float>(FlightLevel) * progress);
    CurrentSpeed = TargetSpeed * 0.3f + (TargetSpeed * 0.7f * progress);

    if (TakeoffTimer <= 0) {
        IsTakingOff = false;
        Altitude = FlightLevel;
        CurrentSpeed = TargetSpeed;
    }

    CurrentCoord.Z = Altitude;

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
    }

    return true;
}

// ============================================================================
// ProcessLanding - Handles the landing sequence.
// ============================================================================

bool FlyLocomotionClass::ProcessLanding()
{
    --LandingTimer;

    float progress = static_cast<float>(LandingTimer) / 30.0f;
    Altitude = static_cast<int32>(static_cast<float>(FlightLevel) * progress);
    CurrentSpeed = TargetSpeed * 0.5f * progress;

    if (LandingTimer <= 0) {
        IsLanding = false;
        Altitude = 0;
        CurrentSpeed = 0;
        HasMoveOrder = false;
        IsMoving = false;

        if (DockTarget) {
            DockAtTarget();
        }
    }

    CurrentCoord.Z = Altitude;

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
    }

    return true;
}

// ============================================================================
// ProcessFlight - Handles normal flight movement.
// ============================================================================

bool FlyLocomotionClass::ProcessFlight()
{
    if (CurrentSpeed < TargetSpeed) {
        CurrentSpeed += TargetSpeed * 0.05;
        if (CurrentSpeed > TargetSpeed) {
            CurrentSpeed = TargetSpeed;
        }
    }

    int32 moveSpeed = static_cast<int32>(CurrentSpeed);

    if (Owner) {
        DirStruct facing = CoordMath::DirectionTo(CurrentCoord, MovingDestination);
        Owner->SetFacing(facing);
    }

    CurrentCoord = VectorMath::MoveTowards(CurrentCoord, MovingDestination, moveSpeed);
    CurrentCoord.Z = Altitude;

    int32 dist = CurrentCoord.DistanceFrom(MovingDestination);
    if (dist <= moveSpeed) {
        CurrentCoord = MovingDestination;
        CurrentCoord.Z = Altitude;

        if (Altitude > 0 && !IsTakingOff && !IsLanding) {
            if (CanLand()) {
                Land();
            } else {
                HasMoveOrder = false;
                IsMoving = false;
            }
        } else {
            HasMoveOrder = false;
            IsMoving = false;
        }
    }

    UpdateShadowProjection();

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
    }

    return true;
}

// ============================================================================
// ProcessElevation - Handles altitude change without horizontal movement.
// ============================================================================

bool FlyLocomotionClass::ProcessElevation()
{
    if (Altitude < TargetAltitude) {
        Altitude += 10;
        if (Altitude >= TargetAltitude) {
            Altitude = TargetAltitude;
            IsElevating = false;
        }
    } else if (Altitude > TargetAltitude) {
        Altitude -= 10;
        if (Altitude <= TargetAltitude) {
            Altitude = TargetAltitude;
            IsElevating = false;
        }
    }

    CurrentCoord.Z = Altitude;

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
    }

    return IsElevating;
}

// ============================================================================
// UpdateShadowProjection - Updates the aircraft's shadow position on the ground.
// ============================================================================

void FlyLocomotionClass::UpdateShadowProjection()
{
    XYZ2 = CurrentCoord;
    int32 groundZ = MapClass::Instance->GetGroundHeight(CurrentCoord);
    XYZ2.Z = groundZ;
}

// ============================================================================
// DockAtTarget - Docks the aircraft at its target after landing.
// ============================================================================

void FlyLocomotionClass::DockAtTarget()
{
    if (!DockTarget) {
        return;
    }

    CoordStruct dockPos = DockTarget->GetCoords();
    CurrentCoord = dockPos;

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Deployed);
    }
}

// ============================================================================
// Mark_All_Occupation_Bits - Aircraft don't occupy ground cells while flying.
// ============================================================================

void FlyLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    if (Altitude == 0 && !IsTakingOff) {
        CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
        bool occupy = (mark == MarkType::Up);
        MapClass::Instance->MarkCellOccupied(cell, occupy);
    }
}

// ============================================================================
// Limbo - Aircraft limbo state.
// ============================================================================

void FlyLocomotionClass::Limbo()
{
    HasMoveOrder = false;
    IsMoving = false;
    IsTakingOff = false;
    IsLanding = false;
    IsElevating = false;
    Altitude = 0;
    CurrentSpeed = 0;
}

// ============================================================================
// Fly - Initiates flight from the ground.
// ============================================================================

void FlyLocomotionClass::Fly()
{
    if (!IsTakingOff && Altitude == 0) {
        TakeOff();
    }
}

// ============================================================================
// Land - Initiates landing sequence.
// ============================================================================

void FlyLocomotionClass::Land()
{
    if (IsTakingOff || IsLanding) {
        return;
    }

    IsLanding = true;
    LandingTimer = 30;
    CurrentSpeed = TargetSpeed * 0.5;
    Altitude = FlightLevel / 2;
}

// ============================================================================
// TakeOff - Initiates takeoff sequence.
// ============================================================================

void FlyLocomotionClass::TakeOff()
{
    if (IsTakingOff || IsLanding) {
        return;
    }

    IsTakingOff = true;
    TakeoffTimer = 15;
    CurrentSpeed = TargetSpeed * 0.3;
    Altitude = 0;
}

// ============================================================================
// SetFlightLevel - Sets the cruising altitude.
// ============================================================================

void FlyLocomotionClass::SetFlightLevel(int32 level)
{
    FlightLevel = level;
    if (!IsTakingOff && !IsLanding && Altitude > 0) {
        Altitude = level;
    }
}

// ============================================================================
// CanLand - Checks if the aircraft can land at its current position.
// ============================================================================

bool FlyLocomotionClass::CanLand() const
{
    if (IsTakingOff || IsLanding) {
        return false;
    }

    if (Altitude <= 0) {
        return false;
    }

    CellStruct cell = CoordMath::CoordToCell(MovingDestination);
    LandType land = MapClass::Instance->GetLandType(cell);

    if (land == LandType::Water || land == LandType::Wall || land == LandType::Rock) {
        return false;
    }

    return true;
}

// ============================================================================
// GetLandingPos - Returns the landing position.
// ============================================================================

CoordStruct FlyLocomotionClass::GetLandingPos() const
{
    return MovingDestination;
}

// ============================================================================
// SetDockTarget - Sets the docking target (helipad/airfield).
// ============================================================================

void FlyLocomotionClass::SetDockTarget(TechnoClass* target)
{
    DockTarget = target;
    AirportBound = (target != nullptr);

    if (target) {
        CoordStruct dockPos = target->GetCoords();
        Move_To(dockPos);
    }
}

// ============================================================================
// GetAltitude - Returns the current altitude.
// ============================================================================

int32 FlyLocomotionClass::GetAltitude() const
{
    return Altitude;
}

// ============================================================================
// Is_In_Air - Returns true if the aircraft is airborne.
// ============================================================================

bool FlyLocomotionClass::Is_In_Air() const
{
    return Altitude > 0 || IsTakingOff || IsLanding;
}

// ============================================================================
// GetShadowPos - Returns the shadow position on the ground.
// ============================================================================

CoordStruct FlyLocomotionClass::GetShadowPos() const
{
    return XYZ2;
}

// ============================================================================
// Is_To_Have_Shadow - Aircraft always have a shadow when airborne.
// ============================================================================

bool FlyLocomotionClass::Is_To_Have_Shadow() const
{
    return Is_In_Air();
}

// ============================================================================
// Can_Enter_Cell - Aircraft can enter any non-water cell.
// ============================================================================

Move FlyLocomotionClass::Can_Enter_Cell(CellStruct cell)
{
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return Move::No;
    }

    if (Altitude <= 0 && !IsTakingOff) {
        LandType land = MapClass::Instance->GetLandType(cell);
        if (land == LandType::Water || land == LandType::Wall) {
            return Move::No;
        }
    }

    return Move::OK;
}

// ============================================================================
// Get_Status - Aircraft movement status code.
// ============================================================================

int32 FlyLocomotionClass::Get_Status() const
{
    if (IsTakingOff) {
        return static_cast<int32>(Sequence::Fly);
    }
    if (IsLanding) {
        return static_cast<int32>(Sequence::Fly);
    }
    if (HasMoveOrder) {
        return static_cast<int32>(Sequence::Fly);
    }
    if (Altitude == 0) {
        return static_cast<int32>(Sequence::Ready);
    }
    return static_cast<int32>(Sequence::IdleFly);
}

// ============================================================================
// Can_Fire - Aircraft can fire while airborne.
// ============================================================================

FireError FlyLocomotionClass::Can_Fire() const
{
    if (IsTakingOff || IsLanding) {
        return FireError::Movement;
    }
    return FireError::OK;
}

// ============================================================================
// TargetAltitude - Sets a target altitude without horizontal movement.
// ============================================================================

void FlyLocomotionClass::SetTargetAltitude(int32 targetAlt)
{
    TargetAltitude = targetAlt;
    if (targetAlt != Altitude) {
        IsElevating = true;
    }
}

// ============================================================================
// Is_Really_Moving_Now - Aircraft is moving if in flight.
// ============================================================================

bool FlyLocomotionClass::Is_Really_Moving_Now() const
{
    return HasMoveOrder || IsTakingOff || IsLanding;
}