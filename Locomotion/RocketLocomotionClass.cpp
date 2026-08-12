#include "RocketLocomotionClass.h"

#include <cmath>
#include <cstdlib>

// ============================================================================
// RocketLocomotionClass - Rocket/projectile locomotion.
// Ballistic trajectory calculation, acceleration, max speed, impact
// detection, target tracking, and homing logic.
// ============================================================================

RocketLocomotionClass::RocketLocomotionClass()
    : LocomotionClass()
    , MovingDestination(0, 0, 0)
    , MissionState(0)
    , unknown_44(0)
    , CurrentSpeed(0.0)
    , unknown_bool_4C(false)
    , SpawnerIsElite(false)
    , CurrentPitch(0.0f)
    , unknown_58(0)
    , unknown_5C(0)
    , Thrust(10.0)
    , MaxSpeed(300.0)
    , Acceleration(5.0)
    , IsAccelerating(false)
    , IsDecelerating(false)
{
    Speed = 300;
}

RocketLocomotionClass::~RocketLocomotionClass()
{
    // No additional cleanup required. RocketLocomotionClass owns no
    // dynamically allocated resources: its state consists of scalar
    // trajectory fields (CurrentSpeed, Thrust, MaxSpeed, Acceleration,
    // CurrentPitch, etc.) and two POD timers (RateTimer MissionTimer
    // and CDTimerClass TrailerTimer) that perform no heap allocation.
    // The base LocomotionClass destructor handles cleanup of the
    // shared owner/coordinate state.
}

HRESULT RocketLocomotionClass::GetClassID(CLSID* pClassID)
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

int32 RocketLocomotionClass::Size()
{
    return sizeof(RocketLocomotionClass);
}

// ============================================================================
// Is_Moving - Rocket is moving if it has thrust or is in flight.
// ============================================================================

bool RocketLocomotionClass::Is_Moving()
{
    return IsMoving || IsAccelerating || IsDecelerating || CurrentSpeed > 0;
}

// ============================================================================
// Destination - Returns the rocket's current destination.
// ============================================================================

CoordStruct RocketLocomotionClass::Destination()
{
    return MovingDestination;
}

// ============================================================================
// Move_To - Sets rocket destination. Initiates acceleration.
// ============================================================================

void RocketLocomotionClass::Move_To(CoordStruct to)
{
    MovingDestination = to;
    Dest = to;
    IsMoving = true;
    Accelerate();
}

// ============================================================================
// Stop_Moving - Stops rocket movement. Initiates deceleration.
// ============================================================================

void RocketLocomotionClass::Stop_Moving()
{
    IsMoving = false;
    Decelerate();
}

// ============================================================================
// Do_Turn - Rocket turning. Handles smooth trajectory curvature.
// ============================================================================

void RocketLocomotionClass::Do_Turn(DirStruct coord)
{
    if (Owner) {
        DirStruct currentFacing = Owner->GetFacing();
        int32 diff = static_cast<int32>(coord.Value) - static_cast<int32>(currentFacing.Value);

        if (diff == 0) {
            return;
        }

        int32 turnRate = 3;
        int32 step = 0;

        if (diff < -128 || (diff > 0 && diff <= 128)) {
            step = (diff > 0) ? turnRate : -turnRate;
        } else {
            step = (diff > 0) ? -turnRate : turnRate;
        }

        int32 newVal = static_cast<int32>(currentFacing.Value) + step;
        if (newVal < 0) {
            newVal += 256;
        }
        if (newVal >= 256) {
            newVal -= 256;
        }

        if (std::abs(newVal - static_cast<int32>(coord.Value)) <= turnRate) {
            Owner->SetFacing(coord);
        } else {
            Owner->SetFacing(DirStruct(static_cast<uint8>(newVal)));
        }
    }
}

// ============================================================================
// Process - Main rocket movement loop. Handles acceleration, flight,
// deceleration, and impact detection.
// ============================================================================

bool RocketLocomotionClass::Process()
{
    if (IsAccelerating) {
        ProcessAcceleration();
    }

    if (IsDecelerating) {
        ProcessDeceleration();
    }

    if (IsMoving && !IsAccelerating && !IsDecelerating) {
        ProcessCruise();
    }
    else if (IsMoving && IsAccelerating) {
        ProcessCruise();
    }

    if (IsMoving && CurrentSpeed > 0) {
        UpdateTrajectory();
    }

    if (IsMoving && CurrentSpeed <= 0 && !IsAccelerating && !IsDecelerating) {
        IsMoving = false;
    }

    return IsMoving || IsAccelerating || IsDecelerating;
}

// ============================================================================
// ProcessAcceleration - Handles rocket acceleration phase.
// ============================================================================

void RocketLocomotionClass::ProcessAcceleration()
{
    CurrentSpeed += Acceleration;
    if (CurrentSpeed >= MaxSpeed) {
        CurrentSpeed = MaxSpeed;
        IsAccelerating = false;
    }
}

// ============================================================================
// ProcessDeceleration - Handles rocket deceleration phase.
// ============================================================================

void RocketLocomotionClass::ProcessDeceleration()
{
    CurrentSpeed -= Acceleration * 2.0;
    if (CurrentSpeed <= 0) {
        CurrentSpeed = 0;
        IsDecelerating = false;
        IsMoving = false;
    }
}

// ============================================================================
// ProcessCruise - Handles rocket cruising at full speed.
// ============================================================================

void RocketLocomotionClass::ProcessCruise()
{
    int32 moveSpeed = static_cast<int32>(CurrentSpeed);

    CurrentCoord = VectorMath::MoveTowards(CurrentCoord, MovingDestination, moveSpeed);

    int32 dist = CurrentCoord.DistanceFrom(MovingDestination);
    if (dist <= moveSpeed) {
        CurrentCoord = MovingDestination;
        IsMoving = false;
        Decelerate();
    }

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
    }
}

// ============================================================================
// UpdateTrajectory - Updates the rocket's trajectory, handling homing
// and ballistic arc calculations.
// ============================================================================

void RocketLocomotionClass::UpdateTrajectory()
{
    if (Owner) {
        DirStruct facing = CoordMath::DirectionTo(CurrentCoord, MovingDestination);
        Do_Turn(facing);
    }

    int32 dist = CurrentCoord.DistanceFrom(MovingDestination);
    if (dist > 0) {
        float heightRatio = static_cast<float>(MovingDestination.Z - CurrentCoord.Z)
            / static_cast<float>(dist);
        CurrentPitch = std::atan2(heightRatio, 1.0f) * (180.0f / 3.141592653589793f);
    }

    if (Owner && CurrentPitch != 0.0f) {
        Owner->SetPitch(static_cast<int32>(CurrentPitch));
    }
}

// ============================================================================
// GetTargetTracking - Calculates homing trajectory toward a moving target.
// ============================================================================

void RocketLocomotionClass::GetTargetTracking(CoordStruct targetPos)
{
    MovingDestination = targetPos;

    int32 dist = CurrentCoord.DistanceFrom(targetPos);
    if (dist > 0) {
        float homingFactor = 0.1f;
        CoordStruct adjusted;
        adjusted.X = CurrentCoord.X + static_cast<int32>(
            static_cast<float>(targetPos.X - CurrentCoord.X) * homingFactor);
        adjusted.Y = CurrentCoord.Y + static_cast<int32>(
            static_cast<float>(targetPos.Y - CurrentCoord.Y) * homingFactor);
        adjusted.Z = CurrentCoord.Z + static_cast<int32>(
            static_cast<float>(targetPos.Z - CurrentCoord.Z) * homingFactor);

        MovingDestination = adjusted;
    }
}

// ============================================================================
// CalculateBallisticTrajectory - Computes a ballistic arc path.
// ============================================================================

void RocketLocomotionClass::CalculateBallisticTrajectory(CoordStruct target)
{
    int32 dx = target.X - CurrentCoord.X;
    int32 dy = target.Y - CurrentCoord.Y;
    int32 dz = target.Z - CurrentCoord.Z;

    int32 horizontalDist = static_cast<int32>(std::sqrt(
        static_cast<double>(dx * dx + dy * dy)));

    double launchAngle = std::atan2(static_cast<double>(dz), static_cast<double>(horizontalDist));

    CurrentPitch = static_cast<float>(launchAngle * (180.0 / 3.141592653589793));

    if (horizontalDist > 0) {
        int32 arcHeight = horizontalDist / 4;
        MovingDestination = target;
        MovingDestination.Z += arcHeight;
    }

    if (Owner) {
        Owner->SetPitch(static_cast<int32>(CurrentPitch));
    }
}

// ============================================================================
// CheckImpact - Checks if the rocket has reached its target and should detonate.
// ============================================================================

bool RocketLocomotionClass::CheckImpact()
{
    int32 dist = CurrentCoord.DistanceFrom(MovingDestination);
    if (dist <= 64) {
        IsMoving = false;
        CurrentSpeed = 0;
        IsAccelerating = false;
        IsDecelerating = false;
        return true;
    }
    return false;
}

// ============================================================================
// SetThrust - Configures rocket thrust parameters.
// ============================================================================

void RocketLocomotionClass::SetThrust(double thrust, double maxSpeed, double accel)
{
    Thrust = thrust;
    MaxSpeed = maxSpeed;
    Acceleration = accel;
}

// ============================================================================
// Mark_All_Occupation_Bits - Rockets don't occupy ground cells.
// ============================================================================

void RocketLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    // Rockets are airborne projectiles that traverse the air layer
    // (Layer::Air). While in flight they must not occupy ground cells,
    // otherwise they would block ground-based pathfinding and trigger
    // spurious collision checks against ground units. This mirrors the
    // behaviour of FlyLocomotionClass, which only marks a ground cell
    // when the aircraft is on the runway.
    //
    // A rocket is considered in flight when any of the following hold:
    //   - IsMoving        (a destination is active)
    //   - IsAccelerating  (thrust phase)
    //   - IsDecelerating  (braking phase)
    //   - CurrentSpeed > 0 (residual velocity)
    // In all of these cases the function returns early and no ground
    // cells are touched.
    //
    // Only when the rocket is completely stationary - before launch or
    // after impact - does it occupy a ground cell. In that state the
    // base class Mark_All_Occupation_Bits handles updating the map's
    // occupation grid for the current cell and its neighbours.
    bool inFlight = IsMoving || IsAccelerating || IsDecelerating || CurrentSpeed > 0.0;
    if (inFlight) {
        return;
    }

    LocomotionClass::Mark_All_Occupation_Bits(mark);
}

// ============================================================================
// Limbo - Rocket limbo state.
// ============================================================================

void RocketLocomotionClass::Limbo()
{
    IsMoving = false;
    IsAccelerating = false;
    IsDecelerating = false;
    CurrentSpeed = 0;
    CurrentPitch = 0.0f;
}

// ============================================================================
// Accelerate - Begins the acceleration phase.
// ============================================================================

void RocketLocomotionClass::Accelerate()
{
    IsDecelerating = false;
    IsAccelerating = true;
}

// ============================================================================
// Decelerate - Begins the deceleration phase.
// ============================================================================

void RocketLocomotionClass::Decelerate()
{
    IsAccelerating = false;
    IsDecelerating = true;
}

// ============================================================================
// Get_Status - Rocket movement status code.
// ============================================================================

int32 RocketLocomotionClass::Get_Status() const
{
    if (IsAccelerating) {
        return static_cast<int32>(Sequence::Fly);
    }
    if (IsMoving) {
        return static_cast<int32>(Sequence::Fly);
    }
    return static_cast<int32>(Sequence::Ready);
}

// ============================================================================
// Can_Fire - Rockets can always fire (they are projectiles).
// ============================================================================

FireError RocketLocomotionClass::Can_Fire() const
{
    return FireError::OK;
}

// ============================================================================
// Is_Really_Moving_Now - Rocket is moving if in flight.
// ============================================================================

bool RocketLocomotionClass::Is_Really_Moving_Now() const
{
    return IsMoving || IsAccelerating || IsDecelerating;
}

// ============================================================================
// GetCurrentSpeed - Returns the current rocket speed.
// ============================================================================

double RocketLocomotionClass::GetCurrentSpeed() const
{
    return CurrentSpeed;
}

// ============================================================================
// GetPitch - Returns the current pitch angle.
// ============================================================================

float RocketLocomotionClass::GetPitch() const
{
    return CurrentPitch;
}