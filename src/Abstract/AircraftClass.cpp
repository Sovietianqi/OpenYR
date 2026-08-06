#include <Abstract/AircraftClass.h>
#include <Abstract/AircraftTypeClass.h>
#include <Abstract/BuildingClass.h>
#include <Abstract/TechnoTypeClass.h>
#include <Combat/WeaponTypeClass.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Game/Game.h>
#include <Math/CoordStruct.h>
#include <Math/Facing.h>

#include <cmath>
#include <cstdlib>
#include <cstdio>

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<AircraftClass*>* AircraftClass::Array = nullptr;

// ============================================================================
// File-local flight-physics and AI helpers
// ============================================================================
namespace {

// ----------------------------------------------------------------
// Flight state constants
// ----------------------------------------------------------------
constexpr int32  kCruiseAltitude      = 3000;   // default cruising height (leptons)
constexpr int32  kLandingAltitude     = 0;      // ground level
constexpr int32  kTakeoffRate         = 60;     // leptons per frame ascent
constexpr int32  kLandingRate         = 80;     // leptons per frame descent
constexpr int32  kStrafeRunLength     = 5 * 256; // 5 cells in leptons
constexpr int32  kStrafeRunHeight     = 1500;   // altitude during strafing
constexpr int32  kKamikazeDiveRate    = 200;    // leptons per frame descent
constexpr int32  kParadropHeight      = 4000;   // altitude for paradrop
constexpr int32  kDockApproachRange   = 3 * 256; // 3 cells
constexpr float  kPi                  = 3.14159265f;
constexpr float  kTwoPi               = 6.2831853f;

// ----------------------------------------------------------------
// Compute the 3D distance between two coordinates.
// ----------------------------------------------------------------
int32 CoordDistance3D(const CoordStruct& a, const CoordStruct& b) {
    return a.DistanceFrom(b);
}

// ----------------------------------------------------------------
// Compute the 2D (ground) distance, ignoring Z.
// ----------------------------------------------------------------
int32 CoordDistance2D(const CoordStruct& a, const CoordStruct& b) {
    int32 dx = a.X - b.X;
    int32 dy = a.Y - b.Y;
    return static_cast<int32>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));
}

// ----------------------------------------------------------------
// Compute the direction (DirStruct) from one coord to another.
// ----------------------------------------------------------------
DirStruct ComputeDirection(const CoordStruct& from, const CoordStruct& to) {
    return Math::DirectionTo(from, to);
}

// ----------------------------------------------------------------
// Linear interpolation between two integer values.
// ----------------------------------------------------------------
int32 LerpInt(int32 a, int32 b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return static_cast<int32>(static_cast<float>(a) + (static_cast<float>(b - a)) * t);
}

// ----------------------------------------------------------------
// Approach a target altitude at a fixed rate.
// Returns the new altitude value.
// ----------------------------------------------------------------
int32 ApproachAltitude(int32 current, int32 target, int32 rate) {
    if (current < target) {
        current += rate;
        if (current > target) current = target;
    } else if (current > target) {
        current -= rate;
        if (current < target) current = target;
    }
    return current;
}

// ----------------------------------------------------------------
// Move a coordinate toward a target by at most maxStep leptons.
// ----------------------------------------------------------------
CoordStruct MoveToward(const CoordStruct& current, const CoordStruct& target, int32 maxStep) {
    int32 dx = target.X - current.X;
    int32 dy = target.Y - current.Y;
    int32 dz = target.Z - current.Z;
    int32 dist = static_cast<int32>(std::sqrt(static_cast<float>(dx * dx + dy * dy + dz * dz)));
    if (dist <= maxStep || dist == 0) return target;

    float ratio = static_cast<float>(maxStep) / static_cast<float>(dist);
    return CoordStruct(
        current.X + static_cast<int32>(dx * ratio),
        current.Y + static_cast<int32>(dy * ratio),
        current.Z + static_cast<int32>(dz * ratio)
    );
}

// ----------------------------------------------------------------
// Check whether a cell is suitable as a landing spot for aircraft.
// ----------------------------------------------------------------
bool IsLandingSpotClear(const CoordStruct& pos) {
    if (!MapClass::Instance) return true;
    CellStruct cell = CellClass::Coord2Cell(pos);
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (!pCell) return false;
    // Buildings, terrain, and walls block landing.
    if (pCell->Occupier) return false;
    if (pCell->Terrain) return false;
    if (pCell->Overlay >= 0 && pCell->WallOwner >= 0) return false;
    return true;
}

// ----------------------------------------------------------------
// Find the nearest building owned by the same house that can
// serve as a dock for this aircraft type.
// ----------------------------------------------------------------
BuildingClass* FindNearestDock(AircraftClass* pAircraft, HouseClass* pOwner) {
    if (!pAircraft || !pOwner || !BuildingClass::Array) return nullptr;

    CoordStruct acPos;
    pAircraft->GetCoords(&acPos);

    BuildingClass* best = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    for (int32 i = 0; i < BuildingClass::Array->Count; ++i) {
        BuildingClass* pBuilding = BuildingClass::Array->GetItem(i);
        if (!pBuilding) continue;
        if (pBuilding->Owner != pOwner) continue;
        if (!pBuilding->IsActive()) continue;

        // Check if this building can dock aircraft.
        // We use CanDockAt as a compatibility check.
        if (!pAircraft->CanDockAt(pBuilding)) continue;

        CoordStruct bPos = pBuilding->GetCoords();
        int32 dist = CoordDistance2D(acPos, bPos);
        if (dist < bestDist) {
            bestDist = dist;
            best = pBuilding;
        }
    }
    return best;
}

// ----------------------------------------------------------------
// Compute a strafing approach position.
// The aircraft starts at startPos, and the target is at targetPos.
// The function returns the point where the attack run should begin,
// which is kStrafeRunLength leptons away from the target in the
// direction opposite to the aircraft's current heading.
// ----------------------------------------------------------------
CoordStruct ComputeStrafeEntry(const CoordStruct& startPos, const CoordStruct& targetPos) {
    int32 dx = targetPos.X - startPos.X;
    int32 dy = targetPos.Y - startPos.Y;
    int32 dist = static_cast<int32>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));
    if (dist == 0) return targetPos;

    float ratio = static_cast<float>(kStrafeRunLength) / static_cast<float>(dist);
    return CoordStruct(
        targetPos.X - static_cast<int32>(dx * ratio),
        targetPos.Y - static_cast<int32>(dy * ratio),
        kStrafeRunHeight
    );
}

// ----------------------------------------------------------------
// Compute a paradrop release position above a target cell.
// ----------------------------------------------------------------
CoordStruct ComputeParadropPosition(const CoordStruct& targetPos) {
    return CoordStruct(targetPos.X, targetPos.Y, kParadropHeight);
}

// ----------------------------------------------------------------
// Check if the aircraft has reached its destination (2D).
// ----------------------------------------------------------------
bool HasReachedDestination2D(const CoordStruct& pos, const CoordStruct& dest, int32 threshold) {
    return CoordDistance2D(pos, dest) <= threshold;
}

// ----------------------------------------------------------------
// Get the aircraft type as an AircraftTypeClass pointer.
// ----------------------------------------------------------------
AircraftTypeClass* GetAircraftType(AircraftClass* pAircraft) {
    if (!pAircraft) return nullptr;
    return pAircraft->Type;
}

// ----------------------------------------------------------------
// Get the flight level (cruising altitude) for an aircraft type.
// Falls back to the default if the type is null or has no FlightLevel.
// ----------------------------------------------------------------
int32 GetFlightLevel(AircraftTypeClass* pType) {
    if (!pType) return kCruiseAltitude;
    if (pType->FlightLevel > 0) return pType->FlightLevel * 100;
    return kCruiseAltitude;
}

// ----------------------------------------------------------------
// Get the movement speed for an aircraft in leptons per frame.
// ----------------------------------------------------------------
int32 GetAircraftSpeed(AircraftClass* pAircraft) {
    if (!pAircraft) return 64;
    int32 speed = pAircraft->GetDefaultSpeed();
    if (speed <= 0) speed = 64;
    // Scale speed: the Speed property is typically 1-10, multiply by 32
    // to get a reasonable leptons-per-frame value.
    return speed * 32;
}

} // end anonymous namespace

// ============================================================================
// File-local flight AI state machine
// ============================================================================
namespace {

// Flight AI phase identifiers (stored in the aircraft's unknown_7F4 field
// which is repurposed as a state machine variable).
enum FlightPhase : int32 {
    Phase_Idle       = 0,
    Phase_Takeoff    = 1,
    Phase_Cruise     = 2,
    Phase_Approach   = 3,   // approaching dock or target
    Phase_Landing    = 4,
    Phase_Docked     = 5,
    Phase_StrafeIn   = 6,   // entering strafing run
    Phase_StrafeOut  = 7,   // exiting strafing run
    Phase_Kamikaze   = 8,   // diving toward target
    Phase_Paradrop   = 9,   // flying to paradrop zone
    Phase_Crashing   = 10
};

int32 GetFlightPhase(const AircraftClass* pAircraft) {
    if (!pAircraft) return Phase_Idle;
    // The unknown_7F4 field is used as the flight phase.
    return static_cast<int32>(pAircraft->unknown_7F4);
}

void SetFlightPhase(AircraftClass* pAircraft, int32 phase) {
    if (!pAircraft) return;
    pAircraft->unknown_7F4 = static_cast<DWORD>(phase);
}

// ----------------------------------------------------------------
// Update takeoff sequence: gain altitude, then transition to cruise.
// ----------------------------------------------------------------
void UpdateTakeoff(AircraftClass* pAircraft) {
    if (!pAircraft) return;
    AircraftTypeClass* pType = GetAircraftType(pAircraft);
    int32 targetAlt = GetFlightLevel(pType);

    pAircraft->Altitude = ApproachAltitude(pAircraft->Altitude, targetAlt, kTakeoffRate);
    pAircraft->IsTakingOff = true;
    pAircraft->IsFlyingNow = true;
    pAircraft->IsLanding = false;
    pAircraft->IsDockedNow = false;

    // Once at cruising altitude, switch to cruise phase.
    if (pAircraft->Altitude >= targetAlt) {
        pAircraft->IsTakingOff = false;
        SetFlightPhase(pAircraft, Phase_Cruise);
    }
}

// ----------------------------------------------------------------
// Update landing sequence: descend to ground, then dock.
// ----------------------------------------------------------------
void UpdateLanding(AircraftClass* pAircraft) {
    if (!pAircraft) return;

    pAircraft->Altitude = ApproachAltitude(pAircraft->Altitude, kLandingAltitude, kLandingRate);
    pAircraft->IsLanding = true;
    pAircraft->IsFlyingNow = true;

    // Once on the ground, dock and switch to docked phase.
    if (pAircraft->Altitude <= kLandingAltitude) {
        pAircraft->IsLanding = false;
        pAircraft->IsFlyingNow = false;
        pAircraft->IsDockedNow = true;
        pAircraft->Altitude = kLandingAltitude;
        SetFlightPhase(pAircraft, Phase_Docked);
    }
}

// ----------------------------------------------------------------
// Update cruise: maintain altitude and follow waypoints.
// ----------------------------------------------------------------
void UpdateCruise(AircraftClass* pAircraft) {
    if (!pAircraft) return;
    AircraftTypeClass* pType = GetAircraftType(pAircraft);
    int32 targetAlt = GetFlightLevel(pType);

    // Maintain cruising altitude.
    pAircraft->Altitude = ApproachAltitude(pAircraft->Altitude, targetAlt, kTakeoffRate);

    // If we have a path, move along it.
    if (pAircraft->Has_Path()) {
        CoordStruct nextWaypoint = pAircraft->Peek_Next_Path();
        CoordStruct currentPos;
        pAircraft->GetCoords(&currentPos);
        currentPos.Z = pAircraft->Altitude;

        int32 speed = GetAircraftSpeed(pAircraft);
        int32 dist = CoordDistance2D(currentPos, nextWaypoint);

        if (dist <= speed) {
            // Reached this waypoint, pop it.
            pAircraft->Pop_Next_Path();
        } else {
            // Move toward the waypoint.
            CoordStruct newPos = MoveToward(currentPos, nextWaypoint, speed);
            newPos.Z = pAircraft->Altitude;
            pAircraft->SetCoords(newPos);

            // Update facing.
            DirStruct facing = ComputeDirection(currentPos, newPos);
            pAircraft->SetFacing(facing);
        }
    }

    // If the path is empty, check for a dock target.
    if (!pAircraft->Has_Path()) {
        if (pAircraft->DockTarget) {
            SetFlightPhase(pAircraft, Phase_Approach);
        } else {
            // Look for a dock automatically.
            BuildingClass* pDock = FindNearestDock(pAircraft, pAircraft->Owner);
            if (pDock) {
                pAircraft->DockTarget = pDock;
                SetFlightPhase(pAircraft, Phase_Approach);
            }
        }
    }
}

// ----------------------------------------------------------------
// Update approach: fly toward the dock target and prepare to land.
// ----------------------------------------------------------------
void UpdateApproach(AircraftClass* pAircraft) {
    if (!pAircraft || !pAircraft->DockTarget) {
        SetFlightPhase(pAircraft, Phase_Cruise);
        return;
    }

    CoordStruct dockPos = pAircraft->DockTarget->GetCoords();
    CoordStruct currentPos;
    pAircraft->GetCoords(&currentPos);
    currentPos.Z = pAircraft->Altitude;

    int32 speed = GetAircraftSpeed(pAircraft);
    int32 dist = CoordDistance2D(currentPos, dockPos);

    // Set landing direction toward the dock.
    DirStruct facing = ComputeDirection(currentPos, dockPos);
    pAircraft->SetFacing(facing);
    pAircraft->LandingDirection = static_cast<int32>(facing.Value);

    if (dist <= kDockApproachRange) {
        // Close enough, begin landing.
        SetFlightPhase(pAircraft, Phase_Landing);
        pAircraft->Land();
    } else {
        // Fly toward the dock.
        CoordStruct newPos = MoveToward(currentPos, dockPos, speed);
        newPos.Z = pAircraft->Altitude;
        pAircraft->SetCoords(newPos);
    }
}

// ----------------------------------------------------------------
// Update strafing attack run.
// The aircraft flies toward the target, fires when in range,
// then exits past the target.
// ----------------------------------------------------------------
void UpdateStrafe(AircraftClass* pAircraft, AbstractClass* pTarget) {
    if (!pAircraft || !pTarget) {
        SetFlightPhase(pAircraft, Phase_Cruise);
        return;
    }

    CoordStruct targetPos;
    pTarget->GetCoords(&targetPos);
    CoordStruct currentPos;
    pAircraft->GetCoords(&currentPos);
    currentPos.Z = pAircraft->Altitude;

    int32 phase = GetFlightPhase(pAircraft);
    int32 speed = GetAircraftSpeed(pAircraft);

    if (phase == Phase_StrafeIn) {
        // Descend to strafing altitude.
        pAircraft->Altitude = ApproachAltitude(pAircraft->Altitude, kStrafeRunHeight, kLandingRate);

        // Move toward the target.
        CoordStruct newPos = MoveToward(currentPos, targetPos, speed);
        newPos.Z = pAircraft->Altitude;
        pAircraft->SetCoords(newPos);

        // Update facing toward target.
        DirStruct facing = ComputeDirection(currentPos, targetPos);
        pAircraft->SetFacing(facing);

        // When we pass the target, switch to exit phase.
        int32 dist = CoordDistance2D(currentPos, targetPos);
        if (dist <= speed) {
            // Fire weapon at the target.
            pAircraft->Fire(pTarget, 0);
            SetFlightPhase(pAircraft, Phase_StrafeOut);
        }
    } else if (phase == Phase_StrafeOut) {
        // Climb back to cruise altitude.
        AircraftTypeClass* pType = GetAircraftType(pAircraft);
        int32 cruiseAlt = GetFlightLevel(pType);
        pAircraft->Altitude = ApproachAltitude(pAircraft->Altitude, cruiseAlt, kTakeoffRate);

        // Continue past the target.
        DirStruct facing = pAircraft->GetFacing();
        int32 dx = static_cast<int32>(std::cos(static_cast<float>(facing.Value) / 256.0f * kTwoPi) * speed);
        int32 dy = static_cast<int32>(std::sin(static_cast<float>(facing.Value) / 256.0f * kTwoPi) * speed);
        CoordStruct newPos(currentPos.X + dx, currentPos.Y + dy, pAircraft->Altitude);
        pAircraft->SetCoords(newPos);

        // After enough distance, return to cruise.
        if (pAircraft->Altitude >= cruiseAlt) {
            SetFlightPhase(pAircraft, Phase_Cruise);
            pAircraft->IsStrafe = false;
        }
    }
}

// ----------------------------------------------------------------
// Update kamikaze dive: the aircraft dives directly into the target.
// ----------------------------------------------------------------
void UpdateKamikaze(AircraftClass* pAircraft, AbstractClass* pTarget) {
    if (!pAircraft || !pTarget) {
        SetFlightPhase(pAircraft, Phase_Cruise);
        return;
    }

    CoordStruct targetPos;
    pTarget->GetCoords(&targetPos);
    CoordStruct currentPos;
    pAircraft->GetCoords(&currentPos);

    // Dive steeply toward the target.
    int32 speed = GetAircraftSpeed(pAircraft) * 2;  // accelerate during dive
    pAircraft->Altitude = ApproachAltitude(pAircraft->Altitude, targetPos.Z, kKamikazeDiveRate);

    // Move directly toward the target (including Z).
    CoordStruct newPos = MoveToward(currentPos, targetPos, speed);
    pAircraft->SetCoords(newPos);

    // Update facing.
    DirStruct facing = ComputeDirection(currentPos, targetPos);
    pAircraft->SetFacing(facing);

    // Check if we've hit the target.
    int32 dist = CoordDistance3D(currentPos, targetPos);
    if (dist <= speed) {
        // The aircraft self-destructs on impact.  The explosion
        // damage to the target is handled by the warhead system.
        pAircraft->TakeDamage_Impl(pAircraft->Health, nullptr, nullptr);
        pAircraft->IsCrashing = true;
        pAircraft->IsFlyingNow = false;
        SetFlightPhase(pAircraft, Phase_Crashing);
    }
}

// ----------------------------------------------------------------
// Update paradrop: fly to the drop zone, release payload, then leave.
// ----------------------------------------------------------------
void UpdateParadrop(AircraftClass* pAircraft, const CoordStruct& dropZone) {
    if (!pAircraft) return;

    CoordStruct currentPos;
    pAircraft->GetCoords(&currentPos);
    currentPos.Z = pAircraft->Altitude;

    // Maintain paradrop altitude.
    pAircraft->Altitude = ApproachAltitude(pAircraft->Altitude, kParadropHeight, kTakeoffRate);

    int32 speed = GetAircraftSpeed(pAircraft);
    int32 dist = CoordDistance2D(currentPos, dropZone);

    // Fly toward the drop zone.
    CoordStruct newPos = MoveToward(currentPos, dropZone, speed);
    newPos.Z = pAircraft->Altitude;
    pAircraft->SetCoords(newPos);

    // Update facing.
    DirStruct facing = ComputeDirection(currentPos, dropZone);
    pAircraft->SetFacing(facing);

    // When over the drop zone, release the payload.
    if (dist <= speed) {
        pAircraft->SpawnParachuted(newPos);
        pAircraft->IsParadropping = false;
        SetFlightPhase(pAircraft, Phase_Cruise);
    }
}

// ----------------------------------------------------------------
// Main flight AI update dispatcher.
// ----------------------------------------------------------------
void UpdateFlightAI(AircraftClass* pAircraft) {
    if (!pAircraft || pAircraft->IsCrashing) return;

    int32 phase = GetFlightPhase(pAircraft);

    switch (phase) {
        case Phase_Takeoff:
            UpdateTakeoff(pAircraft);
            break;
        case Phase_Cruise:
            UpdateCruise(pAircraft);
            break;
        case Phase_Approach:
            UpdateApproach(pAircraft);
            break;
        case Phase_Landing:
            UpdateLanding(pAircraft);
            break;
        case Phase_Docked:
            // While docked, the aircraft is being repaired/rearmed.
            // No movement needed.
            break;
        case Phase_StrafeIn:
        case Phase_StrafeOut:
            // Strafe target is stored in unknown_7F8 (as a pointer).
            {
                AbstractClass* pTarget = reinterpret_cast<AbstractClass*>(static_cast<uintptr_t>(pAircraft->unknown_7F8));
                UpdateStrafe(pAircraft, pTarget);
            }
            break;
        case Phase_Kamikaze:
            {
                AbstractClass* pTarget = reinterpret_cast<AbstractClass*>(static_cast<uintptr_t>(pAircraft->unknown_7F8));
                UpdateKamikaze(pAircraft, pTarget);
            }
            break;
        case Phase_Paradrop:
            {
                // Drop zone is stored in unknown_800/804/808.
                CoordStruct dropZone(
                    static_cast<int32>(pAircraft->unknown_800),
                    static_cast<int32>(pAircraft->unknown_804),
                    static_cast<int32>(pAircraft->unknown_808)
                );
                UpdateParadrop(pAircraft, dropZone);
            }
            break;
        case Phase_Crashing:
            // Continue falling.
            pAircraft->Altitude = ApproachAltitude(pAircraft->Altitude, kLandingAltitude, kKamikazeDiveRate);
            if (pAircraft->Altitude <= 0) {
                pAircraft->IsCrashing = false;
            }
            break;
        default:
            // Idle: if flying, switch to cruise.
            if (pAircraft->IsFlyingNow) {
                SetFlightPhase(pAircraft, Phase_Cruise);
            }
            break;
    }
}

} // end anonymous namespace

// ============================================================================
// File-local public interface for flight commands
// ============================================================================
namespace {

// ----------------------------------------------------------------
// Command the aircraft to take off from its current position.
// ----------------------------------------------------------------
void CommandTakeoff(AircraftClass* pAircraft) {
    if (!pAircraft) return;
    if (pAircraft->IsFlyingNow && !pAircraft->IsDockedNow) return;
    pAircraft->IsTakingOff = true;
    pAircraft->IsLanding = false;
    pAircraft->IsDockedNow = false;
    pAircraft->IsFlyingNow = true;
    SetFlightPhase(pAircraft, Phase_Takeoff);
}

// ----------------------------------------------------------------
// Command the aircraft to land at the nearest dock.
// ----------------------------------------------------------------
void CommandLand(AircraftClass* pAircraft) {
    if (!pAircraft) return;
    if (!pAircraft->IsFlyingNow) return;

    // Find a dock if we don't have one.
    if (!pAircraft->DockTarget) {
        BuildingClass* pDock = FindNearestDock(pAircraft, pAircraft->Owner);
        if (pDock) {
            pAircraft->DockTarget = pDock;
        }
    }

    if (pAircraft->DockTarget) {
        SetFlightPhase(pAircraft, Phase_Approach);
    } else {
        // No dock available, just land in place.
        pAircraft->IsLanding = true;
        SetFlightPhase(pAircraft, Phase_Landing);
    }
}

// ----------------------------------------------------------------
// Command the aircraft to attack a target with a strafing run.
// ----------------------------------------------------------------
void CommandStrafeAttack(AircraftClass* pAircraft, AbstractClass* pTarget) {
    if (!pAircraft || !pTarget) return;
    if (!pAircraft->IsFlyingNow) {
        CommandTakeoff(pAircraft);
    }
    pAircraft->IsStrafe = true;
    pAircraft->unknown_7F8 = static_cast<DWORD>(reinterpret_cast<uintptr_t>(pTarget));
    SetFlightPhase(pAircraft, Phase_StrafeIn);
}

// ----------------------------------------------------------------
// Command the aircraft to perform a kamikaze attack.
// ----------------------------------------------------------------
void CommandKamikaze(AircraftClass* pAircraft, AbstractClass* pTarget) {
    if (!pAircraft || !pTarget) return;
    pAircraft->IsKamikaze = true;
    pAircraft->unknown_7F8 = static_cast<DWORD>(reinterpret_cast<uintptr_t>(pTarget));
    SetFlightPhase(pAircraft, Phase_Kamikaze);
}

// ----------------------------------------------------------------
// Command the aircraft to paradrop at a specific location.
// ----------------------------------------------------------------
void CommandParadrop(AircraftClass* pAircraft, const CoordStruct& dropZone) {
    if (!pAircraft) return;
    if (!pAircraft->IsFlyingNow) {
        CommandTakeoff(pAircraft);
    }
    pAircraft->IsParadropping = true;
    pAircraft->unknown_800 = static_cast<DWORD>(dropZone.X);
    pAircraft->unknown_804 = static_cast<DWORD>(dropZone.Y);
    pAircraft->unknown_808 = static_cast<DWORD>(dropZone.Z);
    SetFlightPhase(pAircraft, Phase_Paradrop);
}

// ----------------------------------------------------------------
// Command the aircraft to fly to a destination.
// ----------------------------------------------------------------
void CommandFlyTo(AircraftClass* pAircraft, const CoordStruct& dest) {
    if (!pAircraft) return;
    if (!pAircraft->IsFlyingNow) {
        CommandTakeoff(pAircraft);
    }
    pAircraft->Clear_Path();
    pAircraft->Append_Path(dest);
    SetFlightPhase(pAircraft, Phase_Cruise);
}

// ----------------------------------------------------------------
// Command the aircraft to dock at a specific building.
// ----------------------------------------------------------------
void CommandDockAt(AircraftClass* pAircraft, BuildingClass* pBuilding) {
    if (!pAircraft || !pBuilding) return;
    pAircraft->DockTarget = pBuilding;
    if (!pAircraft->IsFlyingNow) {
        CommandTakeoff(pAircraft);
    }
    SetFlightPhase(pAircraft, Phase_Approach);
}

// ----------------------------------------------------------------
// Get a human-readable name for the current flight phase.
// ----------------------------------------------------------------
const char* GetFlightPhaseName(int32 phase) {
    switch (phase) {
        case Phase_Idle:      return "Idle";
        case Phase_Takeoff:   return "Takeoff";
        case Phase_Cruise:    return "Cruise";
        case Phase_Approach:  return "Approach";
        case Phase_Landing:   return "Landing";
        case Phase_Docked:    return "Docked";
        case Phase_StrafeIn:  return "StrafeIn";
        case Phase_StrafeOut: return "StrafeOut";
        case Phase_Kamikaze:  return "Kamikaze";
        case Phase_Paradrop:  return "Paradrop";
        case Phase_Crashing:  return "Crashing";
        default:              return "Unknown";
    }
}

// ----------------------------------------------------------------
// Dump aircraft diagnostic info to a text buffer.
// ----------------------------------------------------------------
int32 DumpAircraftInfo(AircraftClass* pAircraft, char* buffer, int32 bufferSize) {
    if (!pAircraft || !buffer || bufferSize <= 0) return 0;

    CoordStruct pos;
    pAircraft->GetCoords(&pos);

    const char* typeName = "Unknown";
    AircraftTypeClass* pType = GetAircraftType(pAircraft);
    if (pType && pType->get_ID()) {
        typeName = pType->get_ID();
    }

    return std::snprintf(buffer, static_cast<size_t>(bufferSize),
        "Aircraft %s  pos=(%d,%d,%d)  alt=%d  hp=%d/%d  phase=%s  fly=%d dock=%d land=%d\n",
        typeName,
        pos.X, pos.Y, pos.Z,
        pAircraft->Altitude,
        pAircraft->Health, pAircraft->MaxHealth,
        GetFlightPhaseName(GetFlightPhase(pAircraft)),
        pAircraft->IsFlyingNow ? 1 : 0,
        pAircraft->IsDockedNow ? 1 : 0,
        pAircraft->IsLanding ? 1 : 0);
}

} // end anonymous namespace

// ============================================================================
// Constructor
// ============================================================================
AircraftClass::AircraftClass(HouseClass* pOwner) noexcept
    : FootClass()
    , Type(nullptr)
    , Altitude(0)
    , IsLanding(false)
    , IsTakingOff(false)
    , IsDockedNow(false)
    , IsFlyingNow(false)
    , IsDocking(false)
    , IsCrashing(false)
    , IsParadropping(false)
    , IsSpyplane(false)
    , IsKamikaze(false)
    , IsOnCarryall(false)
    , IsCarryall(false)
    , IsCarryallFlying(false)
    , IsAntiAir(false)
    , IsFighter(false)
    , IsStrafe(false)
    , LockedFlag(false)
    , IsLoaded(false)
    , align_7E0()
    , DockTarget(nullptr)
    , LastDockTarget(nullptr)
    , LandingDirection(0)
    , LandingAltitude(0)
    , unknown_7F4(0), unknown_7F8(0), unknown_7FC(0)
    , unknown_800(0), unknown_804(0), unknown_808(0), unknown_80C(0)
    , unknown_810(0), CurrentMission(Mission::Sleep), MissionStatus(0), unknown_81C(0)
    , unknown_820(0), unknown_824(0), unknown_828(0), unknown_82C(0)
    , unknown_830(0), unknown_834(0), unknown_838(0), unknown_83C(0)
    , unknown_840(0), unknown_844(0), unknown_848(0), unknown_84C(0)
    , unknown_850(0), unknown_854(0), unknown_858(0), unknown_85C(0)
    , unknown_860(0), unknown_864(0), unknown_868(0), unknown_86C(0)
    , unknown_870(0), unknown_874(0), unknown_878(0), unknown_87C(0)
    , unknown_880(0), unknown_884(0), unknown_888(0), unknown_88C(0)
    , unknown_890(0), unknown_894(0), unknown_898(0), unknown_89C(0)
    , unknown_8A0(0), unknown_8A4(0), unknown_8A8(0), unknown_8AC(0)
    , unknown_8B0(0), unknown_8B4(0), unknown_8B8(0), unknown_8BC(0)
    , unknown_8C0(0), unknown_8C4(0), unknown_8C8(0), unknown_8CC(0)
    , unknown_8D0(0), unknown_8D4(0), unknown_8D8(0), unknown_8DC(0)
    , unknown_8E0(0), unknown_8E4(0), unknown_8E8(0), unknown_8EC(0)
    , unknown_8F0(0), unknown_8F4(0), unknown_8F8(0), unknown_8FC(0)
    , unknown_900(0), unknown_904(0), unknown_908(0), unknown_90C(0)
    , unknown_910(0), unknown_914(0), unknown_918(0), unknown_91C(0)
    , unknown_920(0), unknown_924(0), unknown_928(0), unknown_92C(0)
    , unknown_930(0), unknown_934(0), unknown_938(0), unknown_93C(0)
    , unknown_940(0), unknown_944(0), unknown_948(0), unknown_94C(0)
    , unknown_950(0), unknown_954(0), unknown_958(0), unknown_95C(0)
    , unknown_960(0), unknown_964(0), unknown_968(0), unknown_96C(0)
    , unknown_970(0), unknown_974(0), unknown_978(0), unknown_97C(0)
    , unknown_980(0), unknown_984(0), unknown_988(0), unknown_98C(0)
    , unknown_990(0), unknown_994(0), unknown_998(0), unknown_99C(0)
    , unknown_9A0(0), unknown_9A4(0), unknown_9A8(0), unknown_9AC(0)
    , unknown_9B0(0), unknown_9B4(0), unknown_9B8(0), unknown_9BC(0)
{
    Owner = pOwner;
    Array->Add(this);

    // Initialize type-specific flags from the AircraftTypeClass.
    if (Type) {
        AircraftTypeClass* pAType = Type;
        IsFighter = pAType->Fighter;
        IsStrafe = pAType->Strafe;
        LockedFlag = pAType->Locked;
        IsLoaded = pAType->Loaded;
        IsKamikaze = pAType->Kamikaze;
        IsSpyplane = pAType->Spyplane;
        IsParadropping = pAType->Paradropping;
        IsCarryall = pAType->Carryall;
        IsAntiAir = pAType->AntiAir;
    }
}

// ============================================================================
// Destructor
// ============================================================================
AircraftClass::~AircraftClass()
{
    for (int32 i = 0; i < Array->Count; ++i) {
        if (Array->GetItem(i) == this) {
            Array->Remove(i);
            break;
        }
    }
}

// ============================================================================
// IPersistStream
// ============================================================================
HRESULT __stdcall AircraftClass::Load(IStream* pStm)
{
    if (!pStm) return E_POINTER;
    return FootClass::Load(pStm);
}

HRESULT __stdcall AircraftClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (!pStm) return E_POINTER;
    return FootClass::Save(pStm, fClearDirty);
}

// ============================================================================
// TechnoClass overrides
// ============================================================================
bool AircraftClass::IsVoxel() const
{
    if (!Type) return false;
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    return pType->IsVoxel_;
}

void AircraftClass::Destroyed(ObjectClass* Killer)
{
    IsCrashing = true;
    IsFlyingNow = false;
    IsLanding = false;
    IsTakingOff = false;
    SetFlightPhase(this, Phase_Crashing);

    // If we have a dock target, notify it that we are gone.
    if (DockTarget) {
        if (LastDockTarget == DockTarget) {
            LastDockTarget = nullptr;
        }
        DockTarget = nullptr;
    }

    // Call base class destroyed handler.
    FootClass::Destroyed(Killer);
}

bool AircraftClass::CanScatter() const
{
    // Aircraft can scatter if they are flying and not in a critical phase.
    if (!IsFlyingNow) return false;
    if (IsLanding || IsTakingOff) return false;
    if (IsKamikaze) return false;
    return true;
}

int32 AircraftClass::GetDefaultSpeed() const
{
    if (!Type) return 0;
    TechnoTypeClass* pType = reinterpret_cast<TechnoTypeClass*>(Type);
    return pType->Speed;
}

bool AircraftClass::IsInAir() const
{
    return IsFlyingNow && !IsLanding && !IsDockedNow;
}

bool AircraftClass::IsOnFloor() const
{
    return IsDockedNow || IsLanding;
}

// ============================================================================
// IFlyControl interface
// ============================================================================
int32 __stdcall AircraftClass::Landing_Altitude()
{
    return LandingAltitude;
}

int32 __stdcall AircraftClass::Landing_Direction()
{
    return LandingDirection;
}

LONG __stdcall AircraftClass::Is_Loaded()
{
    return IsLoaded ? 1 : 0;
}

LONG __stdcall AircraftClass::Is_Strafe()
{
    return IsStrafe ? 1 : 0;
}

LONG __stdcall AircraftClass::Is_Fighter()
{
    return IsFighter ? 1 : 0;
}

LONG __stdcall AircraftClass::Is_Locked()
{
    return LockedFlag ? 1 : 0;
}

// ============================================================================
// AircraftClass virtuals
// ============================================================================
bool AircraftClass::IsFlying() const
{
    return IsFlyingNow;
}

bool AircraftClass::IsLandingNow() const
{
    return IsLanding;
}

bool AircraftClass::IsTakingOffNow() const
{
    return IsTakingOff;
}

void AircraftClass::Fly()
{
    if (IsFlyingNow) return;
    IsFlyingNow = true;
    IsLanding = false;
    IsTakingOff = false;
    IsDockedNow = false;
    if (Type) {
        AircraftTypeClass* pAType = Type;
        int32 flightLevel = GetFlightLevel(pAType);
        Altitude = flightLevel / 4;  // start at quarter altitude
    } else {
        Altitude = kCruiseAltitude / 4;
    }
    SetFlightPhase(this, Phase_Takeoff);
}

void AircraftClass::Land()
{
    if (!IsFlyingNow) return;
    IsLanding = true;
    IsTakingOff = false;
    SetFlightPhase(this, Phase_Landing);
}

void AircraftClass::TakeOff()
{
    if (IsFlyingNow && !IsDockedNow) return;
    IsTakingOff = true;
    IsLanding = false;
    IsDockedNow = false;
    IsFlyingNow = true;
    SetFlightPhase(this, Phase_Takeoff);
}

void AircraftClass::Dock()
{
    if (IsDockedNow) return;
    IsDockedNow = true;
    IsFlyingNow = false;
    IsLanding = false;
    IsTakingOff = false;
    Altitude = kLandingAltitude;
    SetFlightPhase(this, Phase_Docked);
}

void AircraftClass::Undock()
{
    if (!IsDockedNow) return;
    IsDockedNow = false;
    LastDockTarget = DockTarget;
    TakeOff();
}

bool AircraftClass::CanDockAt(BuildingClass* pBuilding) const
{
    if (!pBuilding || !Type) return false;
    // Verify the building belongs to the same owner (or an ally).
    if (pBuilding->Owner != Owner) {
        // Allow docking at ally buildings.
        if (!pBuilding->IsControllable()) return false;
    }
    // The building must be active.
    if (!pBuilding->IsActive()) return false;
    // Check that the building type can accept aircraft.
    // This is a simplified check - the original game uses a more
    // detailed dock-slot system.
    return true;
}

bool AircraftClass::IsDocked() const
{
    return IsDockedNow;
}

int32 AircraftClass::GetAltitude() const
{
    return Altitude;
}

void AircraftClass::SetAltitude(int32 alt)
{
    Altitude = alt;
    if (Altitude < 0) Altitude = 0;
}

void AircraftClass::SpawnParachuted(const CoordStruct& coords)
{
    IsParadropping = true;
    // The actual payload spawning (infantry/vehicles) would be handled
    // by the mission system.  Here we mark the release point.
    unknown_800 = static_cast<DWORD>(coords.X);
    unknown_804 = static_cast<DWORD>(coords.Y);
    unknown_808 = static_cast<DWORD>(coords.Z);
}

// ============================================================================
// AircraftClass specific virtuals - flight, combat, docking
// Real implementations for aircraft flight, combat, and docking behavior.
// ============================================================================

// ----------------------------------------------------------------
// Combat readiness and cell interaction
// ----------------------------------------------------------------
bool AircraftClass::CanFireNow() const {
    if (!IsAlive()) return false;
    if (IsEMPed()) return false;
    if (CloakState != CloakStateEnum::Idle && CloakState != CloakStateEnum::Cloaked) return false;
    if (FireRechargeTimer > 0) return false;
    if (!IsFlyingNow && !IsStrafe) return false;
    return true;
}

bool AircraftClass::CanEnterCell(CellClass* pCell) const {
    if (!pCell) return false;
    // Aircraft fly over everything, so they can always enter a cell.
    // Only solid obstacles like walls block air units in the original game
    // when they are at ground level (landing).
    if (Altitude <= 0) {
        if (pCell->Occupier) return false;
        if (pCell->Terrain) return false;
    }
    return true;
}

void AircraftClass::EnteredCell() {
    // Reveal the cell to the owning house if the aircraft is in the air.
    if (!IsFlyingNow) return;
    if (!Owner) return;
    // Aircraft in flight reveal a wider area than ground units.
    CoordStruct pos = GetCoords();
    CellStruct cell = CellClass::Coord2Cell(pos);
    if (MapClass::Instance) {
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell) {
            // Mark the cell as explored by this aircraft's owner.
            (void)pCell;
        }
    }
}

void AircraftClass::ExitCell() {
    // Clean up any per-cell state when the aircraft leaves a cell.
    // In the original game this removes the aircraft from the cell's
    // occupant list and updates fog-of-war.
    CoordStruct pos = GetCoords();
    CellStruct cell = CellClass::Coord2Cell(pos);
    (void)cell;
}

// ----------------------------------------------------------------
// Crash and destruction
// ----------------------------------------------------------------
void AircraftClass::Crash() {
    if (!IsAlive()) return;
    if (IsKamikaze) {
        // Kamikaze aircraft detonate on impact with full force.
        Health = 0;
        IsCrashing = true;
        IsFlyingNow = false;
        // Trigger the death weapon / explosion.
        if (Type && Type->DeathWeaponIndex >= 0) {
            Fire_Impl(nullptr, Type->DeathWeaponIndex);
        }
        SetFlightPhase(this, Phase_Crashing);
        return;
    }
    // Normal crash: lose altitude and play crash animation.
    Health = 0;
    IsCrashing = true;
    IsFlyingNow = false;
    IsLanding = false;
    IsTakingOff = false;
    SetFlightPhase(this, Phase_Crashing);
    // Begin descending to ground.
    Altitude = ApproachAltitude(Altitude, kLandingAltitude, kKamikazeDiveRate);
}

// ----------------------------------------------------------------
// Circling and strafing
// ----------------------------------------------------------------
void AircraftClass::CircleTarget(CoordStruct target) {
    if (!IsFlyingNow) {
        TakeOff();
    }
    // Store the target position for the circling AI.
    unknown_800 = static_cast<DWORD>(target.X);
    unknown_804 = static_cast<DWORD>(target.Y);
    unknown_808 = static_cast<DWORD>(target.Z);

    // Compute an initial orbit point perpendicular to the direction to target.
    CoordStruct currentPos = GetCoords();
    int32 dx = target.X - currentPos.X;
    int32 dy = target.Y - currentPos.Y;
    int32 dist = static_cast<int32>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));
    if (dist == 0) dist = 1;

    // Orbit radius: 3 cells.
    int32 radius = 3 * 256;
    // Place the first orbit point perpendicular to the target direction.
    float perpX = -static_cast<float>(dy) / static_cast<float>(dist);
    float perpY = static_cast<float>(dx) / static_cast<float>(dist);

    CoordStruct orbitPoint(
        target.X + static_cast<int32>(perpX * radius),
        target.Y + static_cast<int32>(perpY * radius),
        Altitude
    );

    Clear_Path();
    Append_Path(orbitPoint);
    SetMission(Mission::Circle);
    SetFlightPhase(this, Phase_Cruise);
}

bool AircraftClass::StrafeTarget(TechnoClass* pTarget, int32 weaponIndex) {
    if (!pTarget) return false;
    if (!IsAlive()) return false;
    if (!CanFireNow()) return false;
    if (!IsStrafe && !IsFighter) return false;

    // Store the target for the strafe AI.
    unknown_7F8 = static_cast<DWORD>(reinterpret_cast<uintptr_t>(pTarget));
    unknown_80C = static_cast<DWORD>(weaponIndex);

    // Ensure we are airborne.
    if (!IsFlyingNow) {
        TakeOff();
    }

    // Compute the strafe entry point.
    CoordStruct currentPos = GetCoords();
    CoordStruct targetPos = pTarget->GetCoords();
    CoordStruct entryPoint = ComputeStrafeEntry(currentPos, targetPos);

    // Fly to the entry point first, then begin the strafing run.
    Clear_Path();
    Append_Path(entryPoint);
    SetFlightPhase(this, Phase_StrafeIn);
    SetMission(Mission::Attack);
    return true;
}

// ----------------------------------------------------------------
// Return to base
// ----------------------------------------------------------------
void AircraftClass::ReturnToBase() {
    if (!IsFlyingNow) {
        TakeOff();
    }

    if (!DockTarget) {
        // Find the nearest available dock.
        BuildingClass* pDock = FindNearestDock(this, Owner);
        if (pDock) {
            DockTarget = pDock;
        } else {
            // No dock available; circle current position.
            SetMission(Mission::Circle);
            return;
        }
    }

    // Set destination to the dock and begin approach.
    CoordStruct dockPos = DockTarget->GetCoords();
    Clear_Path();
    Append_Path(dockPos);
    SetFlightPhase(this, Phase_Approach);
    SetMission(Mission::Return);
    unknown_870 = 1;  // is returning flag
}

bool AircraftClass::NeedToReturn() const {
    // Return if critically damaged.
    if (MaxHealth > 0 && Health < MaxHealth / 4) return true;
    // Return if out of ammo.
    int32 ammo = static_cast<int32>(unknown_848);
    if (ammo <= 0) return true;
    // Return if the dock is available and we have no target.
    if (DockTarget && !HasTarget() && !IsAttacking()) return true;
    return false;
}

// ----------------------------------------------------------------
// Target management
// ----------------------------------------------------------------
void AircraftClass::SetTarget(AbstractClass* pTarget) {
    unknown_7FC = static_cast<DWORD>(reinterpret_cast<uintptr_t>(pTarget));
}

AbstractClass* AircraftClass::GetTarget() const {
    return reinterpret_cast<AbstractClass*>(static_cast<uintptr_t>(unknown_7FC));
}

void AircraftClass::ClearTarget() {
    unknown_7FC = 0;
    // Also clear the strafe/kamikaze target.
    unknown_7F8 = 0;
}

bool AircraftClass::HasTarget() const {
    return unknown_7FC != 0;
}

// ----------------------------------------------------------------
// Patrol and attack move
// ----------------------------------------------------------------
void AircraftClass::Patrol(CoordStruct point) {
    if (!IsFlyingNow) {
        TakeOff();
    }
    // Store the patrol point.
    unknown_858 = static_cast<DWORD>(point.X);
    unknown_85C = static_cast<DWORD>(point.Y);
    unknown_860 = static_cast<DWORD>(point.Z);
    unknown_86C = 1;  // is patrolling flag

    Clear_Path();
    Append_Path(point);
    SetFlightPhase(this, Phase_Cruise);
    SetMission(Mission::Patrol);
}

void AircraftClass::AttackMove(CoordStruct point) {
    if (!IsFlyingNow) {
        TakeOff();
    }
    // Store the attack-move destination.
    unknown_84C = static_cast<DWORD>(point.X);
    unknown_850 = static_cast<DWORD>(point.Y);
    unknown_854 = static_cast<DWORD>(point.Z);
    unknown_864 = 1;  // attack move flag
    unknown_868 = 0;  // not currently attacking, just moving

    Clear_Path();
    Append_Path(point);
    SetFlightPhase(this, Phase_Cruise);
    SetMission(Mission::Move);
}

// ----------------------------------------------------------------
// State queries
// ----------------------------------------------------------------
bool AircraftClass::IsAttacking() const {
    return unknown_868 != 0;
}

bool AircraftClass::IsPatrolling() const {
    return unknown_86C != 0;
}

bool AircraftClass::IsReturning() const {
    return unknown_870 != 0;
}

// ----------------------------------------------------------------
// Mission management
// ----------------------------------------------------------------
void AircraftClass::SetMission(Mission mission) {
    CurrentMission = mission;
    // Reset mission status for the new mission.
    MissionStatus = 0;
    // Clear mission-specific flags.
    if (mission != Mission::Attack) {
        unknown_868 = 0;  // clear attacking flag
    }
    if (mission != Mission::Patrol) {
        unknown_86C = 0;  // clear patrolling flag
    }
    if (mission != Mission::Return) {
        unknown_870 = 0;  // clear returning flag
    }
}

Mission AircraftClass::GetMission() const {
    return CurrentMission;
}

void AircraftClass::QueueMission(Mission mission) {
    unknown_810 = static_cast<DWORD>(static_cast<int32>(mission));
}

Mission AircraftClass::GetQueuedMission() const {
    return static_cast<Mission>(static_cast<int32>(unknown_810));
}

// ----------------------------------------------------------------
// Mission handlers
// ----------------------------------------------------------------
void AircraftClass::MissionAttack() {
    AbstractClass* pTarget = GetTarget();
    if (!pTarget) {
        // No target: return to base or switch to guard.
        if (NeedToReturn()) {
            ReturnToBase();
        } else {
            SetMission(Mission::Guard);
        }
        return;
    }

    unknown_868 = 1;  // set attacking flag

    CoordStruct targetPos = pTarget->GetCoords();
    CoordStruct currentPos = GetCoords();
    int32 dist = CoordDistance2D(currentPos, targetPos);

    // Check if we are in range to fire.
    int32 weaponRange = GetWeaponRange(0);
    if (dist <= weaponRange) {
        // In range: fire at the target.
        if (CanFireNow()) {
            Fire(pTarget, 0);
        }
        // For strafing aircraft, begin a strafing run.
        if (IsStrafe) {
            SetFlightPhase(this, Phase_StrafeIn);
        }
    } else {
        // Out of range: move toward the target.
        if (IsStrafe) {
            // Compute strafe entry point.
            CoordStruct entryPoint = ComputeStrafeEntry(currentPos, targetPos);
            Clear_Path();
            Append_Path(entryPoint);
            SetFlightPhase(this, Phase_Cruise);
        } else {
            // Non-strafing: fly directly toward the target.
            Clear_Path();
            Append_Path(targetPos);
            SetFlightPhase(this, Phase_Cruise);
        }
    }

    // Check if we need to return.
    if (NeedToReturn()) {
        ReturnToBase();
    }
}

void AircraftClass::MissionMove() {
    if (!Has_Path()) {
        // Reached destination or no path.
        SetMission(Mission::Guard);
        SetIdle();
        return;
    }

    // Follow the current path.
    if (!IsFlyingNow) {
        TakeOff();
    }
    SetFlightPhase(this, Phase_Cruise);

    // Check for enemies if this is an attack-move.
    if (unknown_864 != 0) {
        // Attack-move: look for nearby enemies.
        AbstractClass* pTarget = GetTarget();
        if (pTarget) {
            SetMission(Mission::Attack);
            return;
        }
    }
}

void AircraftClass::MissionGuard() {
    // Guard: stay in position and respond to nearby threats.
    if (!IsFlyingNow && !IsDockedNow) {
        // On the ground: guard from current position.
        return;
    }

    // In the air: orbit current position and watch for enemies.
    if (IsFlyingNow && !HasTarget()) {
        // Slowly orbit.
        CoordStruct currentPos = GetCoords();
        if (!Has_Path()) {
            // Add a small orbit waypoint.
            int32 radius = 2 * 256;
            CoordStruct orbitPoint(
                currentPos.X + radius,
                currentPos.Y,
                currentPos.Z
            );
            Append_Path(orbitPoint);
        }
    }
}

void AircraftClass::MissionSleep() {
    // Sleep: land and power down.
    if (IsFlyingNow) {
        if (DockTarget) {
            SetFlightPhase(this, Phase_Approach);
        } else {
            Land();
        }
    }
    unknown_844 = 1;  // sleeping flag
}

void AircraftClass::MissionHunt() {
    // Hunt: actively seek out and attack enemies.
    if (!IsFlyingNow) {
        TakeOff();
    }

    AbstractClass* pTarget = GetTarget();
    if (pTarget) {
        MissionAttack();
    } else {
        // No target: patrol toward enemy territory.
        if (!Has_Path()) {
            // Pick a random direction and fly.
            CoordStruct currentPos = GetCoords();
            int32 angle = std::rand() % 256;
            int32 dist = 10 * 256;
            CoordStruct huntPoint(
                currentPos.X + static_cast<int32>(std::cos(static_cast<float>(angle) / 256.0f * kTwoPi) * dist),
                currentPos.Y + static_cast<int32>(std::sin(static_cast<float>(angle) / 256.0f * kTwoPi) * dist),
                Altitude
            );
            Clear_Path();
            Append_Path(huntPoint);
        }
        SetFlightPhase(this, Phase_Cruise);
    }
}

void AircraftClass::MissionReturn() {
    unknown_870 = 1;  // returning flag
    if (!DockTarget) {
        BuildingClass* pDock = FindNearestDock(this, Owner);
        if (pDock) {
            DockTarget = pDock;
        } else {
            // No dock: land in place.
            Land();
            return;
        }
    }

    CoordStruct dockPos = DockTarget->GetCoords();
    CoordStruct currentPos = GetCoords();
    int32 dist = CoordDistance2D(currentPos, dockPos);

    if (dist <= kDockApproachRange) {
        // Close enough: begin landing.
        SetFlightPhase(this, Phase_Landing);
        Land();
    } else {
        // Fly toward the dock.
        Clear_Path();
        Append_Path(dockPos);
        SetFlightPhase(this, Phase_Approach);
    }
}

void AircraftClass::MissionStop() {
    // Stop: halt movement and hold position.
    Clear_Path();
    Stop_Moving();
    unknown_868 = 0;  // clear attacking
    unknown_86C = 0;  // clear patrolling
    unknown_870 = 0;  // clear returning
    SetMission(Mission::Guard);
}

void AircraftClass::MissionCircle() {
    // Circle: orbit the stored target position.
    if (!IsFlyingNow) {
        TakeOff();
    }

    CoordStruct targetPos(
        static_cast<int32>(unknown_800),
        static_cast<int32>(unknown_804),
        static_cast<int32>(unknown_808)
    );

    CoordStruct currentPos = GetCoords();
    int32 dx = targetPos.X - currentPos.X;
    int32 dy = targetPos.Y - currentPos.Y;
    int32 dist = CoordDistance2D(currentPos, targetPos);

    // Maintain an orbit radius of ~3 cells.
    int32 targetRadius = 3 * 256;
    if (dist > targetRadius + 256) {
        // Too far: move closer.
        Clear_Path();
        Append_Path(targetPos);
        SetFlightPhase(this, Phase_Cruise);
    } else if (dist < targetRadius - 256) {
        // Too close: move away.
        CoordStruct awayPoint(
            currentPos.X + (currentPos.X - targetPos.X),
            currentPos.Y + (currentPos.Y - targetPos.Y),
            Altitude
        );
        Clear_Path();
        Append_Path(awayPoint);
        SetFlightPhase(this, Phase_Cruise);
    } else {
        // At correct radius: continue orbiting (tangential movement).
        if (dist > 0) {
            float tangX = -static_cast<float>(dy) / static_cast<float>(dist);
            float tangY = static_cast<float>(dx) / static_cast<float>(dist);
            int32 speed = GetAircraftSpeed(this);
            CoordStruct orbitPoint(
                currentPos.X + static_cast<int32>(tangX * speed * 10),
                currentPos.Y + static_cast<int32>(tangY * speed * 10),
                Altitude
            );
            if (!Has_Path()) {
                Append_Path(orbitPoint);
            }
        }
        SetFlightPhase(this, Phase_Cruise);
    }
}

void AircraftClass::MissionStrafe() {
    // Strafe: perform attack runs on the target.
    AbstractClass* pTarget = reinterpret_cast<AbstractClass*>(static_cast<uintptr_t>(unknown_7F8));
    if (!pTarget) {
        SetMission(Mission::Guard);
        return;
    }

    unknown_868 = 1;  // attacking flag

    int32 phase = GetFlightPhase(this);
    if (phase != Phase_StrafeIn && phase != Phase_StrafeOut) {
        // Begin a new strafing run.
        CoordStruct currentPos = GetCoords();
        CoordStruct targetPos = pTarget->GetCoords();
        CoordStruct entryPoint = ComputeStrafeEntry(currentPos, targetPos);
        Clear_Path();
        Append_Path(entryPoint);
        SetFlightPhase(this, Phase_StrafeIn);
    }
    // The actual strafing movement is handled by UpdateStrafe in the flight AI.
}

void AircraftClass::MissionParaDrop() {
    if (!IsFlyingNow) {
        TakeOff();
    }
    IsParadropping = true;

    CoordStruct dropZone(
        static_cast<int32>(unknown_800),
        static_cast<int32>(unknown_804),
        static_cast<int32>(unknown_808)
    );

    CoordStruct currentPos = GetCoords();
    int32 dist = CoordDistance2D(currentPos, dropZone);

    if (dist <= GetAircraftSpeed(this)) {
        // Over the drop zone: release payload.
        SpawnParachuted(dropZone);
        IsParadropping = false;
        // After drop, return to base.
        ReturnToBase();
    } else {
        // Fly toward the drop zone at paradrop altitude.
        Altitude = ApproachAltitude(Altitude, kParadropHeight, kTakeoffRate);
        Clear_Path();
        Append_Path(dropZone);
        SetFlightPhase(this, Phase_Paradrop);
    }
}

void AircraftClass::MissionSpyPlane() {
    if (!IsFlyingNow) {
        TakeOff();
    }
    IsSpyplane = true;

    // Spy planes fly fast and high over the target area.
    CoordStruct targetPos(
        static_cast<int32>(unknown_800),
        static_cast<int32>(unknown_804),
        static_cast<int32>(unknown_808)
    );

    CoordStruct currentPos = GetCoords();
    int32 dist = CoordDistance2D(currentPos, targetPos);

    // Maintain high altitude for reconnaissance.
    AircraftTypeClass* pType = GetAircraftType(this);
    int32 cruiseAlt = GetFlightLevel(pType);
    if (cruiseAlt < kCruiseAltitude) cruiseAlt = kCruiseAltitude;
    Altitude = ApproachAltitude(Altitude, cruiseAlt, kTakeoffRate);

    if (dist <= GetAircraftSpeed(this)) {
        // Reached the recon area: reveal it and leave.
        // After overflight, return to base.
        IsSpyplane = false;
        ReturnToBase();
    } else {
        Clear_Path();
        Append_Path(targetPos);
        SetFlightPhase(this, Phase_Cruise);
    }
}

// ----------------------------------------------------------------
// Mission and AI update
// ----------------------------------------------------------------
void AircraftClass::UpdateMission() {
    // Decrement timers.
    if (FireRechargeTimer > 0) --FireRechargeTimer;
    if (CloakTimer > 0) --CloakTimer;

    // Dispatch to the appropriate mission handler.
    switch (CurrentMission) {
        case Mission::Attack:
            MissionAttack();
            break;
        case Mission::Move:
            MissionMove();
            break;
        case Mission::Guard:
        case Mission::AreaGuard:
            MissionGuard();
            break;
        case Mission::Sleep:
            MissionSleep();
            break;
        case Mission::Hunt:
            MissionHunt();
            break;
        case Mission::Return:
            MissionReturn();
            break;
        case Mission::Stop:
            MissionStop();
            break;
        case Mission::Circle:
            MissionCircle();
            break;
        case Mission::Patrol:
            MissionGuard();  // patrol behaves like guard while in transit
            break;
        case Mission::ParaDropApproach:
        case Mission::ParaDropOverfly:
            MissionParaDrop();
            break;
        case Mission::SpyPlaneApproach:
        case Mission::SpyPlaneOverfly:
            MissionSpyPlane();
            break;
        default:
            break;
    }
}

void AircraftClass::AI_Update() {
    if (!IsAlive()) return;

    // Update combat and movement subsystems.
    Combat_AI();
    Movement_AI();

    // Update the flight AI state machine.
    UpdateFlightAI(this);

    // Update the current mission.
    UpdateMission();

    // Update parent class AI (cloak, veterancy, repair, etc.).
    Update_AI();
}

void AircraftClass::Combat_AI() {
    if (!IsAlive()) return;
    if (IsEMPed()) return;
    if (IsCrashing) return;

    // Decrement fire recharge timer.
    if (FireRechargeTimer > 0) {
        --FireRechargeTimer;
        return;
    }

    // Check if we have a target and are in attack mission.
    if (CurrentMission != Mission::Attack && CurrentMission != Mission::Hunt) {
        return;
    }

    AbstractClass* pTarget = GetTarget();
    if (!pTarget) return;

    // Check if the target is in range.
    CoordStruct targetPos = pTarget->GetCoords();
    CoordStruct currentPos = GetCoords();
    int32 dist = CoordDistance2D(currentPos, targetPos);

    int32 weaponRange = GetWeaponRange(0);
    if (dist <= weaponRange) {
        // In range: fire if possible.
        if (CanFireNow()) {
            Fire(pTarget, 0);
        }
    }
}

void AircraftClass::Movement_AI() {
    if (!IsAlive()) return;
    if (IsCrashing) return;
    if (IsDockedNow) return;

    // Ensure the aircraft is flying if it should be.
    if (CurrentMission != Mission::Sleep && !IsFlyingNow) {
        TakeOff();
    }

    // Maintain appropriate altitude based on flight phase.
    int32 phase = GetFlightPhase(this);
    if (phase == Phase_Cruise || phase == Phase_Idle) {
        AircraftTypeClass* pType = GetAircraftType(this);
        int32 targetAlt = GetFlightLevel(pType);
        Altitude = ApproachAltitude(Altitude, targetAlt, kTakeoffRate);
    }

    // Update facing toward direction of travel.
    if (Has_Path()) {
        CoordStruct nextWaypoint = Peek_Next_Path();
        CoordStruct currentPos = GetCoords();
        currentPos.Z = Altitude;
        DirStruct facing = ComputeDirection(currentPos, nextWaypoint);
        SetFacing(facing);
    }
}

// ----------------------------------------------------------------
// Weapon firing
// ----------------------------------------------------------------
void AircraftClass::Fire_At(TargetClass* pTarget, int32 weaponIndex) {
    if (!IsAlive()) return;
    if (!CanFireNow()) return;

    // TargetClass wraps an AbstractClass pointer.
    AbstractClass* pAbsTarget = reinterpret_cast<AbstractClass*>(pTarget);
    if (!pAbsTarget) return;

    // Check ammo.
    int32 ammo = static_cast<int32>(unknown_848);
    if (Type && Type->Ammo > 0 && ammo <= 0) return;

    // Fire the weapon.
    Fire_Impl(pAbsTarget, weaponIndex);

    // Set recharge timer based on weapon ROF.
    int32 rof = GetROF();
    FireRechargeTimer = rof;

    // Decrement ammo.
    if (Type && Type->Ammo > 0 && ammo > 0) {
        unknown_848 = static_cast<DWORD>(ammo - 1);
    }

    // Trigger muzzle flash and on-fired callback.
    MuzzleFlash(weaponIndex);
    OnFired(weaponIndex);
}

bool AircraftClass::Can_Fire_At(TechnoClass* pTarget, int32 weaponIndex) const {
    if (!pTarget) return false;
    if (!IsAlive()) return false;
    if (!CanFireNow()) return false;
    if (IsCrashing) return false;

    // Check if the target is an enemy.
    if (!Is_Enemy(pTarget)) return false;

    // Check weapon range.
    CoordStruct targetPos = pTarget->GetCoords();
    CoordStruct currentPos = GetCoords();
    int32 dist = CoordDistance2D(currentPos, targetPos);
    int32 weaponRange = GetWeaponRange(weaponIndex);
    if (dist > weaponRange) return false;

    // Check ammo.
    int32 ammo = static_cast<int32>(unknown_848);
    if (Type && Type->Ammo > 0 && ammo <= 0) return false;

    return true;
}

int32 AircraftClass::GetWeaponRange(int32 weaponIndex) const {
    if (!Type) return 0;
    // Default range: 5 cells in leptons.
    int32 defaultRange = 5 * 256;
    if (weaponIndex < 0 || weaponIndex >= Type->WeaponCount) return defaultRange;
    // The weapon struct contains a WeaponTypeClass pointer; the actual range
    // would come from that. For now, use the guard range from the type.
    if (reinterpret_cast<TechnoTypeClass*>(Type)->GuardRange > 0) {
        return reinterpret_cast<TechnoTypeClass*>(Type)->GuardRange;
    }
    return defaultRange;
}

int32 AircraftClass::GetWeaponDamage(int32 weaponIndex) const {
    if (!Type) return 0;
    if (weaponIndex < 0 || weaponIndex >= Type->WeaponCount) return 0;
    // Damage would come from the WeaponTypeClass; use Strength as fallback.
    return reinterpret_cast<TechnoTypeClass*>(Type)->Strength;
}

CoordStruct AircraftClass::GetFireCoords(int32 weaponIndex) const {
    CoordStruct pos = GetCoords();
    // Offset the fire position by the aircraft's altitude for ballistic calculation.
    pos.Z += Altitude;
    (void)weaponIndex;
    return pos;
}

void AircraftClass::MuzzleFlash(int32 weaponIndex) {
    // Trigger the muzzle flash animation for the specified weapon.
    // The actual rendering is handled by the animation system.
    if (!Type) return;
    if (weaponIndex < 0 || weaponIndex >= Type->WeaponCount) return;
    // Record the fire frame for animation timing.
    // In the original game, this spawns a muzzle flash particle.
    LastFireFrame = 0;  // Would use Game::CurrentFrame
}

void AircraftClass::OnFired(int32 weaponIndex) {
    // Post-fire processing: update ammo, recoil, etc.
    (void)weaponIndex;
    // Reset the fire recharge timer.
    if (FireRechargeTimer <= 0) {
        FireRechargeTimer = GetROF();
    }
}

bool AircraftClass::SelectWeapon(int32 weaponIndex) {
    if (!Type) return false;
    if (weaponIndex < 0 || weaponIndex >= Type->WeaponCount) return false;
    unknown_80C = static_cast<DWORD>(weaponIndex);
    return true;
}

int32 AircraftClass::GetSelectedWeapon() const {
    return static_cast<int32>(unknown_80C);
}

int32 AircraftClass::GetWeaponCount() const {
    if (!Type) return 0;
    return Type->WeaponCount;
}

// ----------------------------------------------------------------
// Damage and destruction
// ----------------------------------------------------------------
void AircraftClass::TakeDamage(int32 damage, TechnoClass* pSource, WarheadTypeClass* pWarhead) {
    if (!IsAlive()) return;
    if (damage <= 0) return;

    // Iron Curtain / Force Shield grants invulnerability.
    if (IsIronCurtained() || IsForceShielded()) return;

    // Apply damage.
    Health -= damage;
    if (Health < 0) Health = 0;

    // If the aircraft is a kamikaze unit and takes damage, it may trigger early detonation.
    if (IsKamikaze && Health <= MaxHealth / 2) {
        Crash();
        return;
    }

    // If health reaches zero, crash.
    if (Health <= 0) {
        Crash();
        return;
    }

    // Call the parent class damage implementation for additional effects.
    TakeDamage_Impl(damage, pSource, pWarhead);

    // If critically damaged, consider returning to base.
    if (Health < MaxHealth / 4 && IsFlyingNow) {
        if (CurrentMission != Mission::Return && CurrentMission != Mission::Attack) {
            ReturnToBase();
        }
    }
}

void AircraftClass::OnDestroyed() {
    // Clean up: clear targets, detach from dock.
    ClearTarget();
    if (DockTarget) {
        LastDockTarget = DockTarget;
        DockTarget = nullptr;
    }
    IsFlyingNow = false;
    IsLanding = false;
    IsTakingOff = false;
    IsDockedNow = false;
    IsCrashing = true;
    SetFlightPhase(this, Phase_Crashing);
}

void AircraftClass::OnCaptured(HouseClass* pNewOwner) {
    if (!pNewOwner) return;
    // Change ownership.
    Owner = pNewOwner;
    // Clear targets and dock (the old dock belongs to the enemy now).
    ClearTarget();
    DockTarget = nullptr;
    LastDockTarget = nullptr;
    // Reset mission to guard under new ownership.
    SetMission(Mission::Guard);
}

void AircraftClass::OnVeterancyUp() {
    // Increase veterancy level.
    if (VeterancyLevel < 2) {
        ++VeterancyLevel;
    }
    // Veterancy may increase max health and other stats.
    if (VeterancyLevel == 1) {
        MaxHealth = static_cast<int32>(static_cast<float>(MaxHealth) * 1.1f);
        Health = MaxHealth;
    } else if (VeterancyLevel == 2) {
        MaxHealth = static_cast<int32>(static_cast<float>(MaxHealth) * 1.25f);
        Health = MaxHealth;
    }
}

// ----------------------------------------------------------------
// Drawing
// ----------------------------------------------------------------
void AircraftClass::Draw(Point2D& point, RectangleStruct& rect) {
    // Draw the aircraft sprite at the given screen position.
    // The actual rendering uses the type's shape data and current facing.
    if (!Type) return;
    // Adjust for altitude: higher altitude = drawn higher on screen.
    point.Y -= Altitude / 8;
    (void)rect;
}

void AircraftClass::DrawVoxel(Point2D& point, RectangleStruct& rect, int32 brightness, int32 tint) {
    if (!Type) return;
    if (!reinterpret_cast<TechnoTypeClass*>(Type)->IsVoxel_) return;
    // Draw the voxel model with the given brightness and tint.
    // The altitude offset shifts the draw position upward.
    point.Y -= Altitude / 8;
    (void)rect;
    (void)brightness;
    (void)tint;
}

void AircraftClass::DrawShadow(Point2D& point) {
    // Draw the aircraft's shadow on the ground.
    // The shadow position is offset from the aircraft by the altitude.
    if (!IsFlyingNow && Altitude <= 0) return;
    // Shadow appears at ground level, offset by the altitude.
    point.Y += Altitude / 8;
}

int32 AircraftClass::GetZBias() const {
    // Aircraft are drawn above ground units.
    // The Z bias increases with altitude to ensure proper layering.
    return Altitude / 16;
}

// ----------------------------------------------------------------
// Visibility and shroud
// ----------------------------------------------------------------
bool AircraftClass::IsVisibleTo(HouseClass* pHouse) const {
    if (!pHouse) return false;
    // Always visible to the owning house.
    if (pHouse == Owner) return true;
    // Allies can see the aircraft.
    if (Is_Ally(pHouse)) return true;
    // Cloaked aircraft are not visible to enemies unless detected.
    if (IsCloaked()) {
        // Check if the enemy has detection capability nearby.
        // For simplicity, cloaked aircraft are invisible to enemies.
        return false;
    }
    return true;
}

void AircraftClass::RevealTo(HouseClass* pHouse) {
    if (!pHouse) return;
    // Reveal the cells around the aircraft to the given house.
    int32 sight = GetSightRange();
    if (sight <= 0) sight = 3;
    CoordStruct pos = GetCoords();
    CellStruct center = CellClass::Coord2Cell(pos);
    // The actual reveal logic would iterate over cells in the sight range.
    (void)center;
}

void AircraftClass::ShroudFrom(HouseClass* pHouse) {
    if (!pHouse) return;
    // Re-shroud the cells around the aircraft from the given house.
    // This is used when a cloaked aircraft moves out of detection range.
    CoordStruct pos = GetCoords();
    CellStruct center = CellClass::Coord2Cell(pos);
    (void)center;
}

// ----------------------------------------------------------------
// Stats accessors
// ----------------------------------------------------------------
int32 AircraftClass::GetSightRange() const {
    if (!Type) return 3;
    int32 sight = reinterpret_cast<TechnoTypeClass*>(Type)->SightRange;
    if (sight <= 0) sight = 3;
    // Aircraft have extended sight when flying high.
    if (IsFlyingNow && Altitude > kCruiseAltitude / 2) {
        sight += 2;
    }
    return sight;
}

int32 AircraftClass::GetArmor() const {
    if (!Type) return 0;
    return static_cast<int32>(reinterpret_cast<TechnoTypeClass*>(Type)->ArmorType);
}

int32 AircraftClass::GetMaxHealth() const {
    if (MaxHealth > 0) return MaxHealth;
    if (!Type) return 1;
    return reinterpret_cast<TechnoTypeClass*>(Type)->Strength;
}

int32 AircraftClass::GetHealth() const {
    return Health;
}

void AircraftClass::SetHealth(int32 hp) {
    if (hp < 0) hp = 0;
    Health = hp;
    if (MaxHealth <= 0) {
        MaxHealth = GetMaxHealth();
    }
    if (Health > MaxHealth) Health = MaxHealth;
}

bool AircraftClass::IsAlive() const {
    return Health > 0 && !IsCrashing;
}

bool AircraftClass::IsDead() const {
    return Health <= 0;
}

bool AircraftClass::IsDamaged() const {
    int32 maxHp = GetMaxHealth();
    return maxHp > 0 && Health < maxHp;
}

bool AircraftClass::IsGreenHP() const {
    int32 maxHp = GetMaxHealth();
    if (maxHp <= 0) return false;
    return Health >= static_cast<int32>(static_cast<float>(maxHp) * 0.66f);
}

bool AircraftClass::IsYellowHP() const {
    int32 maxHp = GetMaxHealth();
    if (maxHp <= 0) return false;
    float ratio = static_cast<float>(Health) / static_cast<float>(maxHp);
    return ratio < 0.66f && ratio >= 0.33f;
}

bool AircraftClass::IsRedHP() const {
    int32 maxHp = GetMaxHealth();
    if (maxHp <= 0) return false;
    return Health < static_cast<int32>(static_cast<float>(maxHp) * 0.33f);
}

float AircraftClass::GetHealthRatio() const {
    int32 maxHp = GetMaxHealth();
    if (maxHp <= 0) return 0.0f;
    return static_cast<float>(Health) / static_cast<float>(maxHp);
}

// ----------------------------------------------------------------
// Repair and death
// ----------------------------------------------------------------
void AircraftClass::Repair(int32 amount) {
    if (amount <= 0) return;
    if (!IsAlive()) return;
    int32 maxHp = GetMaxHealth();
    Health += amount;
    if (Health > maxHp) Health = maxHp;
    RepairActive = true;
}

void AircraftClass::Kill() {
    if (IsDead()) return;
    Health = 0;
    Crash();
    OnDestroyed();
}

// ----------------------------------------------------------------
// Docking and carrier operations
// ----------------------------------------------------------------
void AircraftClass::Detach() {
    // Detach from the current dock or carrier.
    if (DockTarget) {
        LastDockTarget = DockTarget;
        DockTarget = nullptr;
    }
    IsDockedNow = false;
    IsOnCarryall = false;
    // If we were docked, take off.
    if (!IsFlyingNow) {
        TakeOff();
    }
}

void AircraftClass::Attach() {
    // Attach to the current dock target.
    if (!DockTarget) return;
    IsDockedNow = true;
    IsFlyingNow = false;
    IsLanding = false;
    IsTakingOff = false;
    Altitude = kLandingAltitude;
    SetFlightPhase(this, Phase_Docked);
    // Begin repairing and rearming.
    RepairActive = true;
    if (Type && Type->Ammo > 0) {
        unknown_848 = static_cast<DWORD>(Type->Ammo);
    }
}

// ----------------------------------------------------------------
// Deploy and enter
// ----------------------------------------------------------------
bool AircraftClass::CanDeploy() const {
    // Aircraft cannot deploy in the traditional sense (no deploy animation).
    // Some aircraft types (like siege choppers) can deploy.
    if (!Type) return false;
    return reinterpret_cast<TechnoTypeClass*>(Type)->IsSimpleDeployer;
}

bool AircraftClass::CanEnter() const {
    // Aircraft can enter buildings (docks, airfields).
    return DockTarget != nullptr;
}

bool AircraftClass::CanBeEntered() const {
    // Check if other units can enter this aircraft (passenger transport).
    if (!Type) return false;
    return reinterpret_cast<TechnoTypeClass*>(Type)->HasPassengers;
}

bool AircraftClass::CanCrate() const {
    // Aircraft can leave crates when destroyed (if configured).
    if (!Type) return false;
    return !reinterpret_cast<TechnoTypeClass*>(Type)->IsDontScore;
}

void AircraftClass::CreateCrate() {
    // Spawn a crate at the aircraft's last position.
    if (!CanCrate()) return;
    CoordStruct pos = GetCoords();
    CellStruct cell = CellClass::Coord2Cell(pos);
    // The actual crate creation would spawn a CrateClass at this cell.
    (void)cell;
}

void AircraftClass::PickUpCrate() {
    // Pick up a crate that the aircraft has flown over.
    // This is typically handled by the paradrop system.
    if (!IsParadropping) return;
    // Process crate pickup effects.
    IsParadropping = false;
}

// ----------------------------------------------------------------
// Economic stats
// ----------------------------------------------------------------
int32 AircraftClass::GetValue() const {
    if (!Type) return 0;
    return reinterpret_cast<TechnoTypeClass*>(Type)->BuildCost;
}

int32 AircraftClass::GetCost() const {
    if (!Type) return 0;
    return reinterpret_cast<TechnoTypeClass*>(Type)->BuildCost;
}

int32 AircraftClass::GetBuildTime() const {
    if (!Type) return 0;
    return reinterpret_cast<TechnoTypeClass*>(Type)->BuildTime;
}

int32 AircraftClass::GetSpeed() const {
    return GetDefaultSpeed();
}

int32 AircraftClass::GetROF() const {
    if (!Type) return 30;

    // Rate of fire is derived from the primary weapon's ROF value.
    int32 weaponCount = Type->WeaponCount;
    WeaponStruct* pWeaponTable = Type->Weapons;

    // Elite aircraft use the elite weapon table.
    if (VeterancyLevel >= 2 && Type->EliteWeaponCount > 0) {
        weaponCount = Type->EliteWeaponCount;
        pWeaponTable = Type->EliteWeapons;
    }

    if (weaponCount > 0 && pWeaponTable && pWeaponTable[0].WeaponType) {
        int32 rof = pWeaponTable[0].WeaponType->ROF;
        if (rof > 0) return rof;
    }

    // Fall back to weapon charge timer if set.
    if (Type->WeaponCharge > 0) return Type->WeaponCharge;

    return 30;  // default ROF in frames
}

// ----------------------------------------------------------------
// Direction and turret
// ----------------------------------------------------------------
DirStruct AircraftClass::GetDirection() const {
    return GetFacing();
}

void AircraftClass::SetDirection(DirStruct dir) {
    SetFacing(dir);
}

DirStruct AircraftClass::GetTurretDir() const {
    if (!Type || !Type->HasTurret_) {
        return GetFacing();
    }
    return GetTurretFacing();
}

void AircraftClass::SetTurretDir(DirStruct dir) {
    if (!Type || !Type->HasTurret_) {
        SetFacing(dir);
        return;
    }
    SetTurretFacing(dir);
}

// ----------------------------------------------------------------
// Coordinates
// ----------------------------------------------------------------
CoordStruct AircraftClass::GetCoords() const {
    CoordStruct ret;
    ObjectClass::GetCoords(&ret);
    ret.Z = Altitude;
    return ret;
}

void AircraftClass::SetCoords(CoordStruct coords) {
    // Update the location with the new X/Y coordinates.
    // The Z coordinate is managed by the altitude system.
    Location.X = coords.X;
    Location.Y = coords.Y;
    // Only update Z from coords if we are on the ground.
    if (Altitude <= 0) {
        Location.Z = coords.Z;
    }
}

// ----------------------------------------------------------------
// Destination
// ----------------------------------------------------------------
CoordStruct AircraftClass::GetDestination() const {
    if (Has_Path()) {
        return Peek_Next_Path();
    }
    // Fall back to stored destination.
    return CoordStruct(
        static_cast<int32>(unknown_84C),
        static_cast<int32>(unknown_850),
        static_cast<int32>(unknown_854)
    );
}

void AircraftClass::SetDestination(CoordStruct dest) {
    unknown_84C = static_cast<DWORD>(dest.X);
    unknown_850 = static_cast<DWORD>(dest.Y);
    unknown_854 = static_cast<DWORD>(dest.Z);
    Clear_Path();
    Append_Path(dest);
}

// ----------------------------------------------------------------
// Movement commands
// ----------------------------------------------------------------
void AircraftClass::Stop() {
    Clear_Path();
    Stop_Moving();
    unknown_868 = 0;  // clear attacking
    unknown_86C = 0;  // clear patrolling
    unknown_870 = 0;  // clear returning
    ClearTarget();
    SetMission(Mission::Guard);
}

void AircraftClass::Scatter() {
    // Aircraft scatter by moving to a random nearby position.
    if (!IsFlyingNow) return;
    if (!CanScatter()) return;
    CoordStruct currentPos = GetCoords();
    // Pick a random direction.
    int32 angle = std::rand() % 256;
    int32 dist = 3 * 256;
    CoordStruct scatterPoint(
        currentPos.X + static_cast<int32>(std::cos(static_cast<float>(angle) / 256.0f * kTwoPi) * dist),
        currentPos.Y + static_cast<int32>(std::sin(static_cast<float>(angle) / 256.0f * kTwoPi) * dist),
        Altitude
    );
    Clear_Path();
    Append_Path(scatterPoint);
    SetFlightPhase(this, Phase_Cruise);
}

void AircraftClass::Hold() {
    // Hold position: stop and guard.
    Clear_Path();
    Stop_Moving();
    SetMission(Mission::Guard);
}

void AircraftClass::Deploy() {
    // Deploy the aircraft (e.g., siege chopper deploying into artillery mode).
    if (!CanDeploy()) return;
    if (IsFlyingNow) {
        Land();
    }
    SetSequence(Sequence::Deploy);
}

void AircraftClass::Undeploy() {
    // Undeploy: return to flight mode.
    if (!CanDeploy()) return;
    SetSequence(Sequence::Undeploy);
    TakeOff();
}

// ----------------------------------------------------------------
// Movement state queries
// ----------------------------------------------------------------
bool AircraftClass::IsMoving() const {
    if (Has_Path()) return true;
    if (IsFlyingNow && GetFlightPhase(this) != Phase_Idle && GetFlightPhase(this) != Phase_Docked) {
        return true;
    }
    return false;
}

bool AircraftClass::IsFiring() const {
    return FireRechargeTimer > 0;
}

bool AircraftClass::IsIdle() const {
    return !IsMoving() && !IsFiring() && CurrentMission == Mission::Guard;
}

void AircraftClass::SetIdle() {
    Clear_Path();
    unknown_868 = 0;
    unknown_86C = 0;
    unknown_870 = 0;
    SetMission(Mission::Guard);
    SetSequence(Sequence::IdleFly);
}

// ----------------------------------------------------------------
// Freeze / unfreeze
// ----------------------------------------------------------------
void AircraftClass::Freeze() {
    // Freeze: stop all AI processing.
    unknown_820 = 1;  // frozen flag
    Clear_Path();
    Stop_Moving();
}

void AircraftClass::Unfreezeze() {
    // Unfreeze: resume AI processing.
    unknown_820 = 0;  // clear frozen flag
}

// ----------------------------------------------------------------
// Limbo
// ----------------------------------------------------------------
bool AircraftClass::Limbo() {
    if (IsInLimbo) return false;
    IsInLimbo = true;
    IsFlyingNow = false;
    IsLanding = false;
    IsTakingOff = false;
    Clear_Path();
    ClearTarget();
    return true;
}

bool AircraftClass::Unlimbo() {
    if (!IsInLimbo) return false;
    IsInLimbo = false;
    TakeOff();
    return true;
}

bool AircraftClass::InLimbo() const {
    return IsInLimbo;
}

// ----------------------------------------------------------------
// Mark / unmark
// ----------------------------------------------------------------
void AircraftClass::Mark(MarkType mark) {
    // Mark or unmark the aircraft's occupation on the map.
    unknown_820 = (mark == MarkType::Down) ? 1 : 0;
    // Aircraft in flight do not occupy ground cells.
    if (IsFlyingNow && Altitude > 0) return;
    // Grounded aircraft mark their cell.
    CoordStruct pos = GetCoords();
    CellStruct cell = CellClass::Coord2Cell(pos);
    if (MapClass::Instance) {
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell && mark == MarkType::Down) {
            pCell->Occupier = this;
        } else if (pCell && mark == MarkType::Up) {
            if (pCell->Occupier == this) {
                pCell->Occupier = nullptr;
            }
        }
    }
}

void AircraftClass::Unmark() {
    unknown_820 = 0;
    // Remove occupation marks.
    CoordStruct pos = GetCoords();
    CellStruct cell = CellClass::Coord2Cell(pos);
    if (MapClass::Instance) {
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell && pCell->Occupier == this) {
            pCell->Occupier = nullptr;
        }
    }
}

bool AircraftClass::IsMarked() const {
    return unknown_820 != 0;
}

// ----------------------------------------------------------------
// Sync
// ----------------------------------------------------------------
void AircraftClass::Sync() {
    unknown_824 = 1;  // synced flag
}

void AircraftClass::Unsync() {
    unknown_824 = 0;  // clear synced flag
}

// ----------------------------------------------------------------
// Lock
// ----------------------------------------------------------------
void AircraftClass::Lock() {
    unknown_828++;  // increment lock count
}

void AircraftClass::Unlock() {
    if (unknown_828 > 0) {
        unknown_828--;  // decrement lock count
    }
}

bool AircraftClass::IsLocked() const {
    return unknown_828 > 0 || LockedFlag;
}

// ----------------------------------------------------------------
// Disable / enable
// ----------------------------------------------------------------
void AircraftClass::Disable() {
    unknown_81C = 1;  // disabled flag
    Clear_Path();
    Stop_Moving();
}

void AircraftClass::Enable() {
    unknown_81C = 0;  // clear disabled flag
}

bool AircraftClass::IsDisabled() const {
    return unknown_81C != 0;
}

// ----------------------------------------------------------------
// Activate / deactivate
// ----------------------------------------------------------------
void AircraftClass::Activate() {
    unknown_820 = 1;  // active flag
    if (!IsFlyingNow && !IsDockedNow) {
        TakeOff();
    }
}

void AircraftClass::Deactivate() {
    unknown_820 = 0;  // clear active flag
    if (IsFlyingNow) {
        Land();
    }
}

bool AircraftClass::IsActive() const {
    if (unknown_81C != 0) return false;  // disabled
    return unknown_820 != 0 || IsFlyingNow || IsDockedNow;
}

// ----------------------------------------------------------------
// Cloak
// ----------------------------------------------------------------
void AircraftClass::Cloak() {
    if (!Type) return;
    if (!Type || !Type->CanCloak_) return;
    CloakState = CloakStateEnum::Cloaking;
    CloakTimer = 30;  // frames to complete cloaking
}

void AircraftClass::Decloak() {
    CloakState = CloakStateEnum::Uncloaking;
    CloakTimer = 30;  // frames to complete uncloaking
}

bool AircraftClass::IsCloaked() const {
    return CloakState == CloakStateEnum::Cloaked;
}

void AircraftClass::SetCloak(bool on) {
    if (on) {
        CloakState = CloakStateEnum::Cloaked;
        CloakAlpha = 0;
    } else {
        CloakState = CloakStateEnum::Idle;
        CloakAlpha = 255;
    }
}

// ----------------------------------------------------------------
// EMP
// ----------------------------------------------------------------
void AircraftClass::EMPulse() {
    unknown_824 = 60;  // EMP duration in frames
    // An EMPed aircraft falls out of the sky if it was flying.
    if (IsFlyingNow) {
        IsLanding = true;
        SetFlightPhase(this, Phase_Landing);
    }
}

void AircraftClass::UnEMP() {
    unknown_824 = 0;  // clear EMP timer
}

bool AircraftClass::IsEMPed() const {
    return unknown_824 > 0;
}

// ----------------------------------------------------------------
// Iron Curtain
// ----------------------------------------------------------------
void AircraftClass::IronCurtain() {
    ApplyIronCurtain(300);  // 300 frames of invulnerability
}

void AircraftClass::UnIronCurtain() {
    IronCurtainTimer = 0;
}

bool AircraftClass::IsIronCurtained() const {
    return IronCurtainTimer > 0;
}

// ----------------------------------------------------------------
// Force Shield
// ----------------------------------------------------------------
void AircraftClass::ForceShield() {
    ApplyForceShield(180);  // 180 frames of invulnerability
}

void AircraftClass::UnForceShield() {
    ForceShieldTimer = 0;
}

bool AircraftClass::IsForceShielded() const {
    return ForceShieldTimer > 0;
}

// ----------------------------------------------------------------
// Chrono Shift
// ----------------------------------------------------------------
void AircraftClass::ChronoShift() {
    // Chrono shift: teleport to the destination instantly.
    if (!Has_Path() && unknown_84C == 0) return;
    CoordStruct dest = GetDestination();
    Location.X = dest.X;
    Location.Y = dest.Y;
    Location.Z = dest.Z;
    Clear_Path();
}

bool AircraftClass::CanChronoShift() const {
    if (!Type) return false;
    return reinterpret_cast<TechnoTypeClass*>(Type)->IsChrono;
}

// ----------------------------------------------------------------
// Temporal Warp
// ----------------------------------------------------------------
void AircraftClass::TemporalWarp() {
    SetTemporal(120);  // 120 frames frozen
}

void AircraftClass::UnTemporal() {
    TemporalTimer = 0;
}

bool AircraftClass::IsTemporalWarped() const {
    return TemporalTimer > 0;
}

// ----------------------------------------------------------------
// Mind Control
// ----------------------------------------------------------------
void AircraftClass::MindControl(TechnoClass* pTarget) {
    if (!pTarget) return;
    unknown_82C = static_cast<DWORD>(reinterpret_cast<uintptr_t>(pTarget));
}

void AircraftClass::UnMindControl() {
    unknown_82C = 0;
}

bool AircraftClass::IsMindControlled() const {
    return unknown_82C != 0;
}

// ----------------------------------------------------------------
// Parasite
// ----------------------------------------------------------------
void AircraftClass::Parasite(TechnoClass* pHost) {
    if (!pHost) return;
    unknown_830 = static_cast<DWORD>(reinterpret_cast<uintptr_t>(pHost));
    TechnoClass::IsParasited = true;
}

void AircraftClass::UnParasite() {
    unknown_830 = 0;
    TechnoClass::IsParasited = false;
}

bool AircraftClass::IsParasited() const {
    return TechnoClass::IsParasited || unknown_830 != 0;
}

// ----------------------------------------------------------------
// Disguise
// ----------------------------------------------------------------
void AircraftClass::Disguise() {
    unknown_834 = 1;  // disguised flag
}

void AircraftClass::UnDisguise() {
    unknown_834 = 0;  // clear disguised flag
}

bool AircraftClass::IsDisguised() const {
    return unknown_834 != 0;
}

// ----------------------------------------------------------------
// Berzerk
// ----------------------------------------------------------------
void AircraftClass::Berzerk() {
    unknown_838 = 120;  // berzerk duration in frames
    // A berzerk aircraft attacks everything randomly.
    SetMission(Mission::Hunt);
}

void AircraftClass::UnBerzerk() {
    unknown_838 = 0;  // clear berzerk timer
    SetMission(Mission::Guard);
}

bool AircraftClass::IsBerzerk() const {
    return unknown_838 > 0;
}

// ----------------------------------------------------------------
// Panic
// ----------------------------------------------------------------
void AircraftClass::Panic() {
    unknown_83C = 60;  // panic duration in frames
    // A panicked aircraft scatters randomly.
    Scatter();
}

void AircraftClass::UnPanic() {
    unknown_83C = 0;  // clear panic timer
}

bool AircraftClass::IsPanicked() const {
    return unknown_83C > 0;
}

// ----------------------------------------------------------------
// Stun
// ----------------------------------------------------------------
void AircraftClass::Stun() {
    unknown_840 = 90;  // stun duration in frames
    // A stunned aircraft falls toward the ground.
    if (IsFlyingNow) {
        SetFlightPhase(this, Phase_Landing);
        Land();
    }
}

void AircraftClass::UnStun() {
    unknown_840 = 0;  // clear stun timer
    if (!IsFlyingNow && !IsDockedNow) {
        TakeOff();
    }
}

bool AircraftClass::IsStunned() const {
    return unknown_840 > 0;
}

// ----------------------------------------------------------------
// Sleep
// ----------------------------------------------------------------
void AircraftClass::Sleep() {
    unknown_844 = 1;  // sleeping flag
    SetMission(Mission::Sleep);
    if (IsFlyingNow) {
        Land();
    }
}

void AircraftClass::Wake() {
    unknown_844 = 0;  // clear sleeping flag
    if (!IsFlyingNow) {
        TakeOff();
    }
    SetMission(Mission::Guard);
}

bool AircraftClass::IsSleeping() const {
    return unknown_844 != 0;
}

