#include <Abstract/AircraftClass.h>
#include <Abstract/MissionClass.h>
#include <Game/Externs.h>
#include <Game/SaveGameClass.h>
#include <Abstract/AircraftTypeClass.h>
#include <Abstract/BuildingClass.h>
#include <Abstract/TechnoTypeClass.h>
#include <Animations/AnimClass.h>
#include <Animations/AnimTypeClass.h>
#include <Combat/BulletClass.h>
#include <Combat/BulletTypeClass.h>
#include <Scenario/ScenarioClass.h>
#include <Special/TemporalClass.h>
#include <Combat/WeaponTypeClass.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Houses/HouseClass.h>
#include <Rules/RulesClass.h>
#include <Game/Game.h>
#include <INI/INIClass.h>
#include <Math/CoordStruct.h>
#include <Math/Facing.h>
#include <Rendering/Surface.h>
#include <Rendering/ConvertClass.h>
#include <Rendering/TacticalClass.h>
#include <Rendering/UnitArt.h>
#include "Rendering/ObjectUI.h"

#include <cmath>
#include <cstdlib>
#include <cstdio>

// 降级渲染提示只打一次。

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<AircraftClass*>* AircraftClass::Array = nullptr;

// ----------------------------------------------------------------------------
// asm: AircraftClass 飞行状态机的相位写入入口 —— 把阶段值写进状态字 (+0x7F4)。
// ----------------------------------------------------------------------------
void AircraftClass::SetFlightPhase(AircraftClass* pAircraft, int32 phase)
{
    if (!pAircraft)
        return;
    pAircraft->unknown_7F4 = static_cast<DWORD>(phase);
}

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

// 文件级阶段回写辅助：把飞行状态机阶段写回 unknown_7F4。
void SetFlightPhaseValue(AircraftClass* pAircraft, int32 phase) {
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
        SetFlightPhaseValue(pAircraft, Phase_Cruise);
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
        SetFlightPhaseValue(pAircraft, Phase_Docked);
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
            SetFlightPhaseValue(pAircraft, Phase_Approach);
        } else {
            // Look for a dock automatically.
            BuildingClass* pDock = FindNearestDock(pAircraft, pAircraft->Owner);
            if (pDock) {
                pAircraft->DockTarget = pDock;
                SetFlightPhaseValue(pAircraft, Phase_Approach);
            }
        }
    }
}

// ----------------------------------------------------------------
// Update approach: fly toward the dock target and prepare to land.
// ----------------------------------------------------------------
void UpdateApproach(AircraftClass* pAircraft) {
    if (!pAircraft || !pAircraft->DockTarget) {
        SetFlightPhaseValue(pAircraft, Phase_Cruise);
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
        SetFlightPhaseValue(pAircraft, Phase_Landing);
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
        SetFlightPhaseValue(pAircraft, Phase_Cruise);
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
            SetFlightPhaseValue(pAircraft, Phase_StrafeOut);
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
            SetFlightPhaseValue(pAircraft, Phase_Cruise);
            pAircraft->IsStrafe = false;
        }
    }
}

// ----------------------------------------------------------------
// Update kamikaze dive: the aircraft dives directly into the target.
// ----------------------------------------------------------------
void UpdateKamikaze(AircraftClass* pAircraft, AbstractClass* pTarget) {
    if (!pAircraft || !pTarget) {
        SetFlightPhaseValue(pAircraft, Phase_Cruise);
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
        SetFlightPhaseValue(pAircraft, Phase_Crashing);
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
        SetFlightPhaseValue(pAircraft, Phase_Cruise);
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
                SetFlightPhaseValue(pAircraft, Phase_Cruise);
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
    SetFlightPhaseValue(pAircraft, Phase_Takeoff);
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
        SetFlightPhaseValue(pAircraft, Phase_Approach);
    } else {
        // No dock available, just land in place.
        pAircraft->IsLanding = true;
        SetFlightPhaseValue(pAircraft, Phase_Landing);
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
    SetFlightPhaseValue(pAircraft, Phase_StrafeIn);
}

// ----------------------------------------------------------------
// Command the aircraft to perform a kamikaze attack.
// ----------------------------------------------------------------
void CommandKamikaze(AircraftClass* pAircraft, AbstractClass* pTarget) {
    if (!pAircraft || !pTarget) return;
    pAircraft->IsKamikaze = true;
    pAircraft->unknown_7F8 = static_cast<DWORD>(reinterpret_cast<uintptr_t>(pTarget));
    SetFlightPhaseValue(pAircraft, Phase_Kamikaze);
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
    SetFlightPhaseValue(pAircraft, Phase_Paradrop);
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
    SetFlightPhaseValue(pAircraft, Phase_Cruise);
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
    SetFlightPhaseValue(pAircraft, Phase_Approach);
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
    , unknown_810(0), unknown_81C(0)
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
    AircraftClass::SetFlightPhase(this, Phase_Crashing);

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
    AircraftClass::SetFlightPhase(this, Phase_Takeoff);
}

void AircraftClass::Land()
{
    if (!IsFlyingNow) return;
    IsLanding = true;
    IsTakingOff = false;
    AircraftClass::SetFlightPhase(this, Phase_Landing);
}

void AircraftClass::TakeOff()
{
    if (IsFlyingNow && !IsDockedNow) return;
    IsTakingOff = true;
    IsLanding = false;
    IsDockedNow = false;
    IsFlyingNow = true;
    AircraftClass::SetFlightPhase(this, Phase_Takeoff);
}

void AircraftClass::Dock()
{
    if (IsDockedNow) return;
    IsDockedNow = true;
    IsFlyingNow = false;
    IsLanding = false;
    IsTakingOff = false;
    Altitude = kLandingAltitude;
    AircraftClass::SetFlightPhase(this, Phase_Docked);
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
        AircraftClass::SetFlightPhase(this, Phase_Crashing);
        return;
    }
    // Normal crash: lose altitude and play crash animation.
    Health = 0;
    IsCrashing = true;
    IsFlyingNow = false;
    IsLanding = false;
    IsTakingOff = false;
    AircraftClass::SetFlightPhase(this, Phase_Crashing);
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
    AircraftClass::SetFlightPhase(this, Phase_Cruise);
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
    AircraftClass::SetFlightPhase(this, Phase_StrafeIn);
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
        BuildingClass* pDock = ::FindNearestDock(this, Owner);
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
    AircraftClass::SetFlightPhase(this, Phase_Approach);
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
    // FootClass 层那一份同步写：基类的 UpdateTargeting / Mission_Enter 读它。
    FootClass::SetTarget(pTarget);
    unknown_7FC = static_cast<DWORD>(reinterpret_cast<uintptr_t>(pTarget));
}

AbstractClass* AircraftClass::GetTarget() const {
    return reinterpret_cast<AbstractClass*>(static_cast<uintptr_t>(unknown_7FC));
}

void AircraftClass::ClearTarget() {
    FootClass::SetTarget(nullptr);
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
    AircraftClass::SetFlightPhase(this, Phase_Cruise);
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
    AircraftClass::SetFlightPhase(this, Phase_Cruise);
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
            AircraftClass::SetFlightPhase(this, Phase_StrafeIn);
        }
    } else {
        // Out of range: move toward the target.
        if (IsStrafe) {
            // Compute strafe entry point.
            CoordStruct entryPoint = ComputeStrafeEntry(currentPos, targetPos);
            Clear_Path();
            Append_Path(entryPoint);
            AircraftClass::SetFlightPhase(this, Phase_Cruise);
        } else {
            // Non-strafing: fly directly toward the target.
            Clear_Path();
            Append_Path(targetPos);
            AircraftClass::SetFlightPhase(this, Phase_Cruise);
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
    AircraftClass::SetFlightPhase(this, Phase_Cruise);

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
            AircraftClass::SetFlightPhase(this, Phase_Approach);
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
        AircraftClass::SetFlightPhase(this, Phase_Cruise);
    }
}

void AircraftClass::MissionReturn() {
    unknown_870 = 1;  // returning flag
    if (!DockTarget) {
        BuildingClass* pDock = ::FindNearestDock(this, Owner);
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
        AircraftClass::SetFlightPhase(this, Phase_Landing);
        Land();
    } else {
        // Fly toward the dock.
        Clear_Path();
        Append_Path(dockPos);
        AircraftClass::SetFlightPhase(this, Phase_Approach);
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
        AircraftClass::SetFlightPhase(this, Phase_Cruise);
    } else if (dist < targetRadius - 256) {
        // Too close: move away.
        CoordStruct awayPoint(
            currentPos.X + (currentPos.X - targetPos.X),
            currentPos.Y + (currentPos.Y - targetPos.Y),
            Altitude
        );
        Clear_Path();
        Append_Path(awayPoint);
        AircraftClass::SetFlightPhase(this, Phase_Cruise);
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
        AircraftClass::SetFlightPhase(this, Phase_Cruise);
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
        AircraftClass::SetFlightPhase(this, Phase_StrafeIn);
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
        AircraftClass::SetFlightPhase(this, Phase_Paradrop);
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
        AircraftClass::SetFlightPhase(this, Phase_Cruise);
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
    // 默认 5 格（lepton）。
    const int32 defaultRange = 5 * 256;
    if (weaponIndex < 0 || weaponIndex >= Type->WeaponCount) return defaultRange;
    // 有真实武器槽时用武器射程（lepton）；否则退回默认值。
    const WeaponStruct& ws = Type->Weapons[weaponIndex];
    if (ws.WeaponType != nullptr && ws.WeaponType->Range > 0)
        return ws.WeaponType->Range;
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
    LastFireFrame = Game::CurrentFrame;
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
    AircraftClass::SetFlightPhase(this, Phase_Crashing);
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
//
//   顺序（asm AircraftClass_Draw）：阴影(FootClass_DrawVXLShadow) ->
//   本体(vt[0x510] Draw) -> 命中闪白 -> 选中框 -> 血条。
//   point 是本帧换算好的屏幕坐标，rect 是视口/裁剪框。
// ----------------------------------------------------------------
void AircraftClass::Draw(Point2D& point, RectangleStruct& rect) {
    if (!Type) return;
    if (!IsAlive()) return;

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr) return;

    // 高度越高，机体在屏幕上越靠上。
    const int32 altitudeOffset = Altitude / 8;

    const int32 screenX = point.X;
    const int32 screenY = point.Y - altitudeOffset;

    // ---- ① 阴影：落在地面（不随高度上移），并随高度往右下偏移 ----------
    Point2D shadowPoint(screenX, screenY);
    DrawShadow(shadowPoint);

    // ---- ② 本体 ----------------------------------------------------------
    Point2D bodyPoint(screenX, screenY);
    int32 brightness = 1000;
    int32 tint = 0;
    if (IsCloaked()) brightness = 520;

    DrawVoxel(bodyPoint, rect, brightness, tint);

    // ---- ③ 选中框 / 血条 -------------------------------------------------
    RectangleStruct box = LastRenderRect;
    if (box.Width <= 0 || box.Height <= 0)
        box = RectangleStruct(screenX - 10, screenY - 10, 20, 20);

    if (IsSelected)
        ObjectUI::DrawSelectionBrackets(pSurface, box, 0x07E0);

    if (MaxHealth > 0 && Health > 0 && Health < MaxHealth)
        ObjectUI::DrawHealthBar(pSurface, box, Health * 100 / MaxHealth);

    // 原版 TechnoClass::DrawExtras：话泡等附加层（本树 Draw 不走 vtable）。
    //   机体在高处，话泡要跟着机身走，所以传 bodyPoint 而不是地面 point。
    DrawExtras(&bodyPoint, &rect);

    if (g_bDebugMode)
        g_RenderDebugStats.DrawCalls++;

    (void)rect;
}

void AircraftClass::DrawVoxel(Point2D& point, RectangleStruct& rect, int32 brightness, int32 tint) {
    if (!Type) return;

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr) return;

    int32 adjBrightness = brightness;
    if (adjBrightness < 0)    adjBrightness = 0;
    if (adjBrightness > 1000) adjBrightness = 1000;

    const int32 facing8 = UnitArt::FacingToDir8(GetFacing().Value);
    const int32 cx = point.X;
    const int32 cy = point.Y;

    // ---- ① 真实素材路径 --------------------------------------------------
    // 飞机的体素/素材句柄同样来自 ObjectTypeClass::ImageSHP；本构建没有
    // VoxLib，能拿到的只有 SHP，有就走 CC_Draw_Shape。
    SHPStruct* pShape = Type->GetImageData();
    if (pShape == nullptr) pShape = Type->GetImage();

    if (pShape != nullptr) {
        Rectangle clip(rect.X, rect.Y, rect.Width, rect.Height);
        if (clip.Width <= 0 || clip.Height <= 0)
            clip = Rectangle(0, 0, pSurface->GetWidth(), pSurface->GetHeight());

        const int32 frameIndex = facing8;
        Point2D pos(cx, cy);
        CC_Draw_Shape(pSurface, Type->PaletteConvert, pShape, frameIndex,
                      &pos, &clip,
                      BlitterFlags::Centered | BlitterFlags::Transparent,
                      0, 0, 0,
                      adjBrightness, tint,
                      nullptr, 0, 0, 0);

        const Rectangle fr = pShape->GetFrameBounds(frameIndex);
        const int32 canvasW = (pShape->Width  > 0) ? pShape->Width  : fr.Width;
        const int32 canvasH = (pShape->Height > 0) ? pShape->Height : fr.Height;
        LastRenderRect = RectangleStruct(cx - canvasW / 2 + fr.X,
                                         cy - canvasH / 2 + fr.Y,
                                         fr.Width, fr.Height);
        return;
    }

    // 没有素材：不画。原版同样不会用几何图形顶替 VXL/SHP。
}

void AircraftClass::DrawShadow(Point2D& point) {
    // 阴影落在地面：只有真的离地才画。
    if (!IsFlyingNow && Altitude <= 0) return;

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr) return;

    // 高度越高，投影越靠屏幕下方（与机体的上移相反）。
    point.Y += Altitude / 8;

    SHPStruct* pShape = Type->GetImageData();
    if (pShape == nullptr) pShape = Type->GetImage();
    if (pShape == nullptr) return;

    Point2D pos(point.X, point.Y);
    Rectangle clip(0, 0, pSurface->GetWidth(), pSurface->GetHeight());

    CC_Draw_Shape(pSurface, Type->PaletteConvert, pShape,
                  Type->ShadowIndex, &pos, &clip,
                  BlitterFlags::Centered | BlitterFlags::Transparent | BlitterFlags::Darken,
                  0, 0, 0, 1000, 0, nullptr, 0, 0, 0);
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
    AircraftClass::SetFlightPhase(this, Phase_Docked);
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
    AircraftClass::SetFlightPhase(this, Phase_Cruise);
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
    Init_Locomotion();          // 按类型装配行走逻辑（TechnoClass_Init 段）
    Start_Default_Mission();    // 落位后启动默认 Guard 任务
    IsInLimbo = false;
    TakeOff();
    // asm: AircraftClass_Unlimbo -> ObjectClass::Unlimbo 尾部的收尾（L1049805）：
    //   置 Alive、登记主表、交显示层。
    FinalizeUnlimbo();
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
        AircraftClass::SetFlightPhase(this, Phase_Landing);
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
        AircraftClass::SetFlightPhase(this, Phase_Landing);
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


// ============================================================================
// Per-instance serialization (base fields + aircraft-specific fields).
// ============================================================================
void AircraftClass::Save(SaveGameClass& saver) const
{
    TechnoClass::Save(saver);

    saver.Write(Altitude);
    saver.Write(static_cast<int8>(IsLanding ? 1 : 0));
    saver.Write(static_cast<int8>(IsDocking ? 1 : 0));
    saver.Write(static_cast<int8>(IsLoaded ? 1 : 0));
    saver.Write(LandingAltitude);
    saver.Write(static_cast<int32>(CurrentMission));
    saver.Write(MissionStatus);
}

void AircraftClass::Load(LoadGameClass& loader)
{
    TechnoClass::Load(loader);

    loader.Read(Altitude);
    int8 flag = 0;
    loader.Read(flag); IsLanding = (flag != 0);
    loader.Read(flag); IsDocking = (flag != 0);
    loader.Read(flag); IsLoaded = (flag != 0);
    loader.Read(LandingAltitude);

    int32 mission = 0;
    loader.Read(mission);
    CurrentMission = static_cast<Mission>(mission);
    loader.Read(MissionStatus);
}

// ============================================================================
// AircraftClass - pose / ground-state probes
// ============================================================================

 // asm: AircraftClass_GetPoseDir
//
//  The global RulesData pose direction, consulted when a parked aircraft is
//  oriented to the map.  A bare load-and-return in the original.
int32 AircraftClass::GetPoseDir() const
{
    return (TheRules != nullptr) ? TheRules->PoseDir : 0;
}

 // asm: AircraftClass_IsGroundUnit
//
//  Tail-call into the OnFloor vtable slot: an aircraft counts as a ground
//  unit exactly when its locomotion layer reports it on the floor.
bool AircraftClass::IsGroundUnit() const
{
    return IsOnFloor();
}

// asm: AircraftClass_Get_Aircraft_Type
//
//  返回机体类型的具体类型指针。与 GetType（TechnoTypeClass* 基类形态）是
//  同一份数据的强类型视图。
AircraftTypeClass* AircraftClass::Get_Aircraft_Type() const
{
    return this->Type;
}

// asm: AircraftClass_IsGroundUnit_Impl
//
//  地面单位判定的虚槽实现：与 IsGroundUnit 同义，转发到 OnFloor 槽位。
bool AircraftClass::IsGroundUnit_Impl() const
{
    return IsOnFloor();
}

// ============================================================================
// asm: AircraftClass_Mi_Scatter(int a2, int a3, int a4)   (asm:8522)
//
//   Get_Mission_Control_Data(); al = ctrl[+9h]（即 MissionControlClass::Scatter）
//   若该任务配置允许散开，则调用 Scatter 槽位；否则原样返回。
//   原实现错误地用 MissionControlClass::Find(Mission::Sticky) 取配置表，
//   与 asm 的"读当前任务自己的配置"不符。
// ============================================================================
void AircraftClass::Mi_Scatter(void* pParam)
{
    MissionControlClass* pControl = MissionControlClass::Find(this->CurrentMission);
    if (pControl != nullptr && pControl->Scatter)
    {
        this->Scatter();
    }

    (void)pParam;
}

// ============================================================================
// 飞机任务处理器
//
//  下面这一组是飞机对基础任务的重写入口。飞机的行动方式与地面单位不同：
//  它要在空域之间往返、决定悬停还是掠过、并按需让乘员跳伞。这里统一采用
//  "先把本机当前任务对齐，再交给本类已有的专用任务实现推进" 的做法，返回
//  任务步进值（0 表示继续）。
// ============================================================================

// ----------------------------------------------------------------------------
// asm: AircraftClass_Mi_Attack
//
//  飞机的攻击任务：如果机上载着伞兵，任务就变成"飞到投放点上空、把乘员
//  放下去"，也就是转入空投流程；否则按普通空中攻击推进——朝目标所在空域
//  飞近，进入射程后开火。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mi_Attack()
{
    // 载着乘员的运输机：攻击任务退化为空投。
    if (this->IsLoaded)
    {
        this->MissionParaDrop();
        return 0;
    }

    this->MissionAttack();
    return 0;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_Mi_Guard   (asm:8563)
//   若已有停靠建筑（[+0x6C4]）：
//     - 小队存在时：机上有目标则置停机标记（byte[+0x6D4]=1）并转停机槽
//       vtable[0x1E8](2,0)，再按 MissionControl.Rate*900 返回延时；
//     - 无小队时：核对落点，必要时设目标格并转停机，返回 1。
//   否则退回足类守备处理器 FootClass_Guard。
//   原实现只是"清目标 + 转 MissionGuard()"，丢掉了停靠/小队/任务率逻辑。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mi_Guard()
{
    if (this->DockTarget != nullptr)
    {
        if (this->Team != nullptr)
        {
            if (this->GetTarget() != nullptr)
            {
                this->IsLoaded = true;              // asm byte[+0x6D4]=1
                this->SetMission(Mission::Return);  // asm vtable[0x1E8](2,0)
            }

            int32 delay = 0;
            MissionControlClass* pControl = MissionControlClass::Find(this->CurrentMission);
            if (pControl != nullptr)
                delay = static_cast<int32>(pControl->Rate * 900.0);
            return delay;
        }

        // 无小队：无目标时把落点设到自身格并转停机。
        if (this->GetTarget() == nullptr)
        {
            this->Set_Destination(this->Get_Coord());
            this->SetMission(Mission::Return);
            return 1;
        }

        this->IsLoaded = true;
        this->SetMission(Mission::Guard);
        return 1;
    }

    // 无停靠建筑：交由足类守备处理器。
    return FootClass::Mission_Guard();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall AircraftClass_Mi_Hunt(AircraftClass *this)   (asm:1301)
//   if ([+0x2FC] == 0) {                        // 无弹药/无武装
//       if (Team) TeamClass::Remove(this, -1, 0);
//       call vtable[0x484](0,1);                // 复位任务/AI
//       return floor(MissionControl.Rate*900) + Random(0,2);
//   }
//   if ([+0x2B4] == 0) { ... 逐条刷新目标/小队指令 ... }
//   call vtable[0x1E8](1,0);                    // 转攻击流程
//   return 1;
//   原实现只是无脑转调 MissionHunt()，完全没有小队脱离/任务率延时/攻击转场。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mi_Hunt()
{
    // 无武装余量（asm [+0x2FC]==0）：脱离小队并按任务率随机延时。
    if (this->CurrentAmmo == 0)
    {
        if (this->Team != nullptr)
            this->Team->Remove(this, -1, false);

        // asm vtable[0x484](0,1)：复位本机任务/AI 状态。
        this->SetMission(Mission::Hunt);

        int32 delay = 0;
        MissionControlClass* pControl = MissionControlClass::Find(Mission::Hunt);
        if (pControl != nullptr)
            delay = static_cast<int32>(pControl->Rate * 900.0);
        delay += std::rand() % 3;   // Random2Class::operator()(0,2)
        return delay;
    }

    // 有目标：转入攻击流程（asm vtable[0x1E8](1,0)）。
    if (this->GetTarget() != nullptr)
        this->MissionAttack();

    return 1;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_Mi_Move
//
//  飞机的移动任务：朝指定的空域飞去。若机上载着乘员，移动任务改成"把
//  乘员运到投放点再放下"的空投流程；否则就是纯飞行，抵达后任务结束。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mi_Move()
{
    // 载着乘员的运输机：移动退化为空投。
    if (this->IsLoaded)
    {
        this->MissionParaDrop();
        return 0;
    }

    this->MissionMove();
    return 0;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_Mi_Retreat   (asm:2832)
//   eax = [esi+5A4h] (TechnoClass::Target)
//   if (Target == 0) {
//       ecx = [esi+21Ch] (Owner); edge = Owner->[1E0h] (Get_Edge)；
//       edge 越界则重算 HouseClass_GetEdge_；
//       DisplayClass::Calculated_Cell(edge, ...) 求战场边缘落点 cell；
//       若落点有效 -> Set_Destination(cell)；
//       retn 3;            // 撤退任务单步完成
//   }
//   // 有目标：若目标即导航目的点，则清空目的点
//   if (Target == Get_Destination()) Set_Destination(null);
//   retn 3;
//   原实现错误地改调 MissionMove() 并返回 0，且清的是 DockTarget 而非导航目的点。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mi_Retreat()
{
    if (this->GetTarget() == nullptr)
    {
        HouseClass* pOwner = this->Owner;
        if (pOwner == nullptr)
            return 3;

        int32 edge = pOwner->Get_Edge();
        if (edge < 0 || edge > 3)
            edge = pOwner->Get_Edge_Inverse();

        // asm 用 DisplayClass::Calculated_Cell(edge,...) 求战场边界落点；
        // 本项目无该接口，按 edge 取地图四侧边缘空域作为等效撤退点。
        const CoordStruct here = this->Get_Coord();
        CoordStruct dest = here;
        switch (edge)
        {
            case 0: dest = CoordStruct(here.X, 0, here.Z); break;      // 北
            case 1: dest = CoordStruct(here.X, 0x4000, here.Z); break; // 南
            case 2: dest = CoordStruct(0, here.Y, here.Z); break;      // 西
            case 3: dest = CoordStruct(0x4000, here.Y, here.Z); break; // 东
            default: break;
        }
        this->Set_Destination(dest);
        return 3;
    }

    // 有目标：目标即当前导航目的点时清掉目的点。
    this->Set_Destination(this->Get_Coord());
    return 3;
}

// ----------------------------------------------------------------------------
// asm: signed int __thiscall AircraftClass_Mi_Enter(AircraftClass *this)   (asm:7597)
//
//   1) if (GetTarget()==0 && DockTarget!=0 && Get_Cell().Cell_Building()!=DockTarget) {
//          PreferredDockingBuilding = (DockTarget->WhatAmI()==6 ? DockTarget : 0);
//          QueueMission(3);            // vtable[0x274](3)
//          SetMission(2,1);            // vtable[0x1E8](2,1)  停机流程
//          retn 1;
//      }
//   2) 若已将目标建筑落到 +0x6CC，则刷新优先停靠建筑（+0x6CC = DockTarget）。
//   3) 无停靠建筑且本机判断非"进近"（WhatAmI()!=2）且 ReplaceDestination()
//      未受理时，判任务完成（call vtable[0x484](0,1); retn 1）。
//   原实现只无条件调用 MissionReturn()，完全没有停靠建筑簿记与收尾。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mi_Enter()
{
    // 情况 1：尚无攻击目标，但已锁定一座停靠建筑，且本机不在该建筑格上。
    //   -> 记下优先停靠建筑并转入停机流程，返回 1（本步完成）。
    if (this->GetTarget() == nullptr && this->DockTarget != nullptr
        && this->LastDockTarget != this->DockTarget)
    {
        // DockTarget->WhatAmI()==AbstractType::Building(6) 时才作为有效停靠点。
        if (this->DockTarget->WhatAmI() == AbstractType::Building)
            this->LastDockTarget = this->DockTarget;   // +0x6CC PreferredDockingBuilding

        this->SetMission(Mission::Return);             // 等价 asm vtable[0x274](3)
        return 1;
    }

    // 情况 2：把当前停靠建筑同步进优先停靠建筑簿记。
    if (this->DockTarget != nullptr && this->DockTarget != this->LastDockTarget)
        this->LastDockTarget = this->DockTarget;

    // 情况 3：没有停靠建筑 -> 退回返航/进入流程。
    if (this->DockTarget == nullptr)
    {
        this->MissionReturn();
        return 0;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_Mi_Unload   (asm:2002)
//   以一个 0..4 的状态字（asm [esi+0BCh]，本项目 MissionStatus）驱动的多段卸载流程：
//     0: 无目标且已就位 -> 置状态 3（开始卸载）;
//     1: 等待抵达停机位，未停靠则状态 2，否则回 0;
//     2: 停机完成 -> 状态 3 并返回 1;
//     3: 卸载乘员（Drop_Off_Cargo），乘客放空后状态归 0;
//     4: 收尾，状态归 0 并返回 1;
//     默认: 返回 floor(MissionControl.Rate*900)+Random(0,2)。
//   原实现仅在 IsLoaded 时转 MissionParaDrop()，状态机完全缺失。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mi_Unload()
{
    switch (this->MissionStatus)
    {
    case 0:
        // 到位判定：无目标且已挂停靠建筑 -> 进入卸载段。
        if (this->GetTarget() == nullptr && this->DockTarget != nullptr)
            this->MissionStatus = 3;
        break;

    case 1:
        // 等待抵达停机位。
        this->MissionStatus = (this->DockTarget == nullptr) ? 3 : 2;
        break;

    case 2:
        // 停机完成，转入卸载。
        this->MissionStatus = 3;
        return 1;

    case 3:
        // 卸载乘员。
        this->Drop_Off_Cargo();
        if (this->Passenger == nullptr)
            this->MissionStatus = 0;
        break;

    case 4:
        this->MissionStatus = 0;
        return 1;

    default:
        break;
    }

    int32 delay = 0;
    MissionControlClass* pControl = MissionControlClass::Find(Mission::Unload);
    if (pControl != nullptr)
        delay = static_cast<int32>(pControl->Rate * 900.0);
    delay += std::rand() % 3;   // Random2Class::operator()(0,2)
    return delay;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_Mi_Patrol
//
//  飞机的巡逻任务：在当前空域与给定的另一处空域之间来回飞行，来回一次算
//  一轮；只要没被打断就持续往返。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mi_Patrol()
{
    this->MissionCircle();
    return 0;
}

// ----------------------------------------------------------------------------
// asm: signed int __thiscall AircraftClass_Mi_AreaGuard(AircraftClass *this)   (asm:8888)
//   若已有停靠建筑（[+0x6C4]）：不在小队（[+0x5D4]==0）则 vtable[0x484](0,1)
//     复位任务；返回 1。
//   否则：若 [2FCh]==0 且 vtable[0x2AC]() 成立且无无线电链路，则复位任务；
//     若 [+0x2B4] 非 0 则转攻击 vtable[0x1E8](1,0) 并返回 1；
//     否则 FootClass_AreaGuard()。
//   原实现只是无条件转 MissionCircle()，与 asm 不符。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mi_AreaGuard()
{
    if (this->DockTarget != nullptr)
    {
        if (this->Team == nullptr)
            this->SetMission(Mission::AreaGuard);   // asm vtable[0x484](0,1)

        return 1;
    }

    // 无武装余量且无小队/指令：复位区域守备任务。
    if (this->CurrentAmmo == 0 && this->Team == nullptr)
        this->SetMission(Mission::AreaGuard);

    // 有目标（asm [+0x2B4]）：转攻击。
    if (this->GetTarget() != nullptr)
    {
        this->SetMission(Mission::Attack);          // asm vtable[0x1E8](1,0)
        return 1;
    }

    return FootClass::Mission_AreaGuard();
}

// ----------------------------------------------------------------------------
// asm: int __fastcall AircraftClass_Mi_ParaDropApproach(AircraftClass *a1)   (asm:90022)
//
//  空投的"接近"阶段：运输机朝投放点飞去。抵达投放点上空就切换到投放段，
//  这一段只负责飞、不负责放人。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mi_ParaDropApproach()
{
    this->MissionParaDrop();
    return 0;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_Mi_ParaDropOverfly
//
//  空投的"掠过"阶段：运输机保持当前航向从投放点上空掠过，同时把乘员依次
//  放下去；乘员放空后本段结束、交给下一段。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mi_ParaDropOverfly()
{
    this->MissionParaDrop();

    // 乘员已经放空，本段完成。
    if (!this->IsLoaded)
        return 1;

    return 0;
}

// ----------------------------------------------------------------------------
// asm: int __fastcall AircraftClass_Mi_SpyPlaneApproach(AircraftClass *this)   (asm:89697)
//
//  侦察机的"接近"阶段：朝侦察目标区域飞去，抵达后切入侦察段。这一段只
//  负责飞行。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mi_SpyPlaneApproach()
{
    this->MissionSpyPlane();
    return 0;
}

// ----------------------------------------------------------------------------
// asm: int __fastcall AircraftClass_Mi_SpyPlaneOverfly(AircraftClass *a1)   (asm:89897)
//
//  侦察机的"掠过"阶段：从目标区域上空掠过，沿途揭开迷雾；飞过去之后本段
//  结束，侦察机转入返航。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mi_SpyPlaneOverfly()
{
    this->MissionSpyPlane();
    return 0;
}

// ============================================================================
// asm: AircraftClass_ReplaceDestination
//
//  给飞机换一个新的目的地：若新点与当前目的地相同就什么都不做；否则记下
//  新点，并把"已到达"标记清掉，好让飞机重新朝新点飞去。
// ============================================================================
void AircraftClass::ReplaceDestination(AbstractClass* pDest)
{
    if (pDest == nullptr)
        return;

    CoordStruct dest;
    pDest->GetCoords(&dest);

    // 目标点没变就免动。
    const CoordStruct cur = this->GetDestination();
    if (cur.X == dest.X && cur.Y == dest.Y && cur.Z == dest.Z)
        return;

    this->SetDestination(dest);

    // 目的地变了，复位"已到达"状态，重新开始飞。
    this->SetMission(Mission::Move);
}

// ============================================================================
// asm: AircraftClass_SetNewTarget
//
//  给飞机换一个攻击目标：先把与原目标的关联解除，再记下新目标并复位任务
//  步进，使攻击任务从第一步重新开始。
// ============================================================================
void AircraftClass::SetNewTarget(AbstractClass* pTarget)
{
    // 解除与旧目标的关联。
    if (this->HasTarget())
        this->ClearTarget();

    if (pTarget == nullptr)
        return;

    this->SetTarget(pTarget);

    // 复位任务步进，让攻击流程从头走。
    this->SetMission(Mission::Attack);
}

// ------------------------------------------------------------------------
// asm: AircraftClass_FlasherClass_QueryInterface
// 机型查询读型表旗标，锁定读自身锁定旗标，载荷看乘客槽，扫射由目标
// 是否为载具单元决定，着陆高度对运机取链路处高度，着陆朝向抄第一个
// 链路的朝向。
// ------------------------------------------------------------------------
HRESULT AircraftClass::FlasherClass_QueryInterface(REFIID iid, void** ppvObject)
{
    return AbstractClass::QueryInterface(iid, ppvObject);
}

ULONG AircraftClass::FlasherClass_AddRef()
{
    return AbstractClass::AddRef();
}

ULONG AircraftClass::FlasherClass_Release()
{
    return AbstractClass::Release();
}

bool AircraftClass::FlasherClass_IsFighter() const
{
    return Type ? Type->Fighter : false;
}

bool AircraftClass::FlasherClass_IsLocked() const
{
    return LockedFlag;
}

bool AircraftClass::FlasherClass_IsLoaded() const
{
    return Passenger != nullptr;
}

bool AircraftClass::FlasherClass_IsStrafe() const
{
    // asm: AircraftClass_FlasherClass_LandingAltitude
    // 才进入扫射。
    AbstractClass* pTarget = GetTarget();
    if (!pTarget)
        return false;
    return pTarget->WhatAmI() == AbstractType::Unit;
}

int32 AircraftClass::FlasherClass_LandingAltitude() const
{
    // asm: AircraftClass_FlasherClass_LandingDirection
    // 目标处的高度为着陆高度，其余用自身记录的着陆高度。
    if (Type && Type->Carryall && !Passenger && DockTarget)
        return DockTarget->Location.Z;
    return LandingAltitude;
}

void AircraftClass::FlasherClass_LandingDirection(DirStruct* pOut) const
{
    // 根据游戏行为，可知着陆朝向取停靠目标的朝向：有停靠目标抄目标，
    // 没有保持当前朝向。
    if (!pOut)
        return;
    if (DockTarget)
        *pOut = DockTarget->PrimaryFacing;
}

// ============================================================================
// 类型信息 / 反射覆写
// ============================================================================

// ----------------------------------------------------------------------------
// asm: AircraftClass_GetType —— 直接返回机体类型指针。
// ----------------------------------------------------------------------------
TechnoTypeClass* AircraftClass::GetType() const
{
    // AircraftTypeClass 派生自 TechnoTypeClass，无需转换。
    return this->Type;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_GetSize —— 原版 AircraftClass 结构体字节数 0x6D8。
//   本复刻沿用同一逻辑尺寸作为序列化/Swizzle 的步进口径。
// ----------------------------------------------------------------------------
int32 AircraftClass::Size() const
{
    return 0x6D8;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_GetAbstractDerivationID —— 返回 AbstractType::Aircraft(2)。
// ----------------------------------------------------------------------------
AbstractType AircraftClass::GetAbstractDerivationID() const
{
    return AbstractType::Aircraft;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_GetClassID —— 把本类 CLSID 写进出参；空指针返回 E_POINTER。
// ----------------------------------------------------------------------------
HRESULT AircraftClass::GetClassID(CLSID* pClassID)
{
    if (!pClassID)
        return E_POINTER;

    static const GUID kAircraftClassID =
        { 0x1B3A6E40, 0x8D2C, 0x11D1, { 0xA5, 0x2E, 0x00, 0x60, 0x97, 0x5A, 0x00, 0x01 } };
    *pClassID = kAircraftClassID;
    return S_OK;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_GetUIName —— 取机体类型的界面名。
// ----------------------------------------------------------------------------
const wchar_t* AircraftClass::GetUIName() const
{
    return this->Type ? this->Type->GetUIName() : L"";
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_InAir —— 机场绑定/悬停类机体改由载荷接口判断是否在空中，
//   其余退回 FootClass 的基础判定。
// ----------------------------------------------------------------------------
bool AircraftClass::InAir() const
{
    if (this->Type && (this->Type->AirportBound || this->Type->IsBalloonHover))
    {
        // 由飞行控制载荷上报：未停靠即视为在空中。
        return !this->IsDockedNow;
    }
    return FootClass::InAir();
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_OnFloor —— 与 InAir 对偶：机场绑定/悬停类检视停靠状态，
//   其余退回 TechnoClass 的基础判定。
// ----------------------------------------------------------------------------
bool AircraftClass::OnFloor() const
{
    if (this->Type && (this->Type->AirportBound || this->Type->IsBalloonHover))
    {
        return this->IsDockedNow;
    }
    return TechnoClass::OnFloor();
}

// ============================================================================
// 通用生命周期
// ============================================================================

// ----------------------------------------------------------------------------
// asm: AircraftClass_Init —— 先跑 TechnoClass::Init，再补齐飞机的朝向结构、
//   退伍/精英判定、飞行控制高度以及所属方的追踪登记。
// ----------------------------------------------------------------------------
void AircraftClass::Init()
{
    TechnoClass::Init();

    if (this->Type)
    {
        // 朝向初始化为"面向目标点"，无目标时用本体朝向。
        DirStruct pose = this->GetFacing();
        this->SetFacing(pose);

        // 默认巡航高度取类型 FlightLevel；未配置时保持 0。
        if (this->Type->FlightLevel > 0)
            this->Altitude = this->Type->FlightLevel * 100;
    }

    // 登记进所属方的追踪表。
    if (this->Owner)
        this->Owner->Tracking_Add(this);
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_Update   (asm:1439 .. 2000)   —— vt+0x5C
//
//  飞机主循环。asm 分段与这里的段落一一对应：
//    A (+0..+1D)    任务分派表：除 0/26/29/30 之外的任务清掉"本帧着陆请求"。
//    B (+1D..+33)   TemporalTargettingMe（时间武器）逐帧。
//    C (+46..+78)   传送入/出时每 36 帧生成一次传送火花。
//    D (+E0..+140)  传送中：清目标；有目的地则撤销并本帧返回。
//    E (+184..+1A1) 目标合法性：目标升空/敌方隐身且看不见 → 丢目标。
//    F (+1A1)       FootClass::AI（父类逐帧）。
//    G (+20E..+2D0) 坠毁：每帧下沉 5，低于 -400 登记摧毁并 UnInit；
//                   每 4 帧冒一次尾烟。
//    H (+34E..+380) 类型尾迹（Trailer）按 SpawnDelay 生成。
//    I (+40D..+46C) 到达临时点：清目的地/目标并进入待机。
//    J (+4A2)       ReadyToNextMission → NextMission。
//    K (+4B6)       开火计数：非 Attack 任务里扣一发弹药。
//    L (+4ED..+52C) 重伤且仍在空中 → 按血量概率冒灰烟。
//    M (+55B..+5B5) Carryall：把吊挂的乘客同步到机体坐标/朝向。
// ----------------------------------------------------------------------------
void AircraftClass::Update()
{
    const int32 frame = Game::GetCurrentFrame();
    RulesClass* pRules = RulesClass::Instance;

    // ---- 段 A：任务分派表 ------------------------------------------------
    switch (GetMission())
    {
        case Mission::Sleep:
        case Mission::ParaDropApproach:
        case Mission::Wait:
        case Mission::AttackMove:
            break;      // 这四个任务保留着陆请求
        default:
            unknown_858 = 0;   // asm field_6D2：本帧着陆请求
            break;
    }

    // ---- 段 B：时间武器 --------------------------------------------------
    if (TemporalTargettingMe != nullptr)
        TemporalTargettingMe->Update();

    // ---- 段 C：传送火花 --------------------------------------------------
    const bool warpingIn  = !IsNotWarpingIn();
    const bool warpingOut = IsWarpingOut();

    if ((warpingIn || warpingOut) && (frame % 0x24) == 0
        && pRules != nullptr && pRules->ChronoSparkle1 != nullptr)
    {
        const CoordStruct pos = GetCoords();
        new AnimClass(pRules->ChronoSparkle1, pos, 0, 1, 0x600, 0, false);
    }

    // ---- 段 D：传送中清理目标/目的地 --------------------------------------
    if ((warpingIn && TemporalTargettingMe != nullptr) || warpingOut)
    {
        if (HasTarget())
            SetTarget(nullptr);

        if (Is_Moving() || Has_Path())
        {
            Stop_Moving();
            return;
        }
    }

    // ---- 段 E：目标合法性 -------------------------------------------------
    if (HasTarget())
    {
        AbstractClass* pTarget = GetTarget();
        if (pTarget != nullptr)
        {
            if (pTarget->WhatAmI() != AbstractType::None && pTarget->IsInAir())
            {
                // asm: target->InAir() 为真 → 打不到，直接丢。
                SetTarget(nullptr);
            }
            else if (pTarget->WhatAmI() == AbstractType::Unit
                  || pTarget->WhatAmI() == AbstractType::Infantry
                  || pTarget->WhatAmI() == AbstractType::Aircraft
                  || pTarget->WhatAmI() == AbstractType::Building)
            {
                TechnoClass* pTechno = static_cast<TechnoClass*>(pTarget);
                if (Owner != nullptr && Is_Enemy(pTechno->Get_Owner())
                    && pTechno->CloakState == CloakStateEnum::Cloaked)
                {
                    // 隐身目标且己方没有探测器 → 看不见，丢目标。
                    SetTarget(nullptr);
                }
            }
        }
    }

    // ---- 段 F：父类逐帧 ---------------------------------------------------
    FootClass::Update();

    if (!IsAlive())
        return;

    // ---- 段 G：坠毁 -------------------------------------------------------
    if (IsCrashing)
    {
        CoordStruct pos = GetCoords();
        pos.Z -= 5;
        SetCoords(pos);

        if (GetHeight() < -400)
        {
            RegisterDestruction();
            UnInit();
            return;
        }

        if ((frame & 3) == 0 && pRules != nullptr && pRules->Wake != nullptr)
        {
            CoordStruct smoke = pos;
            if (ScenarioClass::Instance != nullptr) {
                smoke.X += ScenarioClass::Instance->Random.Next(-170, 170);
                smoke.Y += ScenarioClass::Instance->Random.Next(-170, 170);
            }
            smoke.Z += 0;
            new AnimClass(pRules->Wake, smoke, 0, 1, 0x600, 0, false);
        }
    }

    // ---- 段 H：尾迹 -------------------------------------------------------
    if (Type != nullptr && Type->Trailer != nullptr && Type->SpawnDelay > 0
        && (frame % Type->SpawnDelay) == 0)
    {
        const CoordStruct pos = GetCoords();
        new AnimClass(Type->Trailer, pos, 0, 1, 0x600, 0, false);
    }

    // ---- 段 I：到达临时点 -------------------------------------------------
    //  asm 只在"目的地仍是 tmpCell 哨兵"时清目的地/目标并进待机；本项目没有
    //  该哨兵，等价地要求"没有路径、没在动、也没有目标"才进待机，避免把
    //  仍有作战目标的滞空机误判成待机。
    if (!Has_Path() && !Is_Moving() && !HasTarget())
        Enter_Idle_Mode(0, 1);

    // ---- 段 J：任务推进 ---------------------------------------------------
    if (ReadyToNextMission())
        NextMission();

    // ---- 段 K：开火计数扣弹 -----------------------------------------------
    if (unknown_860 != 0)
    {
        unknown_860 = 0;
        if (GetMission() != Mission::Attack && CurrentAmmo > 0)
            --CurrentAmmo;
    }

    // ---- 段 L：重伤冒烟 ---------------------------------------------------
    if (pRules != nullptr && GetHealthRatio() < 0.5f && GetHeight() > 0
        && (frame & 3) == 0)
    {
        // asm: 血量越低概率越高（0..99 与本机血量比比较）。
        const int32 chance = 80 - static_cast<int32>(GetHealthRatio() * 70.0f);
        const int32 roll = (ScenarioClass::Instance != nullptr)
                         ? ScenarioClass::Instance->Random.Next(0, 99) : 0;
        if (roll < chance)
        {
            AnimTypeClass* pSmoke = AnimTypeClass::Find("SGRYSMK1");
            if (pSmoke != nullptr)
            {
                const CoordStruct pos = GetCoords();
                new AnimClass(pSmoke, pos, 0, 1, 0x600, 0, false);
            }
        }
    }

    // ---- 段 M：Carryall 吊挂同步 ------------------------------------------
    if (Passenger != nullptr && Type != nullptr && Type->Carryall)
    {
        Passenger->Set_Coord(GetCoords());
        Passenger->SetFacing(GetFacing());
    }

    // ---- 飞行状态机与任务调度 --------------------------------------------
    UpdateFlightAI(this);
    this->UpdateMission();
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_Draw —— 机体绘制入口：按高度做屏幕偏移后交给渲染层。
//
//   原版这个三参入口（屏幕 X / 屏幕 Y / 帧序号）就是把机体画到战术面上：
//   先取面，再按高度换算落点，然后走与两参 Draw 完全相同的绘制链。
// ----------------------------------------------------------------------------
void AircraftClass::Draw(int32 a2, int32 a3, int32 a4)
{
    if (!this->Type)
        return;
    if (!this->IsAlive())
        return;

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr)
        return;

    // a2/a3 是屏幕坐标；a4 是原版的帧/标志位，绘制链本身按朝向取帧。
    Point2D screen(a2, a3);

    RectangleStruct viewRect(0, 0, pSurface->GetWidth(), pSurface->GetHeight());
    if (TheDisplay != nullptr) {
        viewRect.Width  = TheDisplay->ScreenWidth;
        viewRect.Height = TheDisplay->ScreenHeight;
    }

    (void)a4;
    this->Draw(screen, viewRect);
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_GetCRC —— 先累积 FootClass 的 CRC，再并入本类若干个
//   状态字节（停靠/着陆/起飞标志与朝向），保持存档校验一致。
// ----------------------------------------------------------------------------
int32 AircraftClass::GetCRC(int32* pCRC) const
{
    if (!pCRC)
        return 0;

    int32 crc = *pCRC;
    crc ^= (this->IsDockedNow ? 0x11 : 0);
    crc ^= (this->IsLanding ? 0x22 : 0);
    crc ^= (this->IsTakingOff ? 0x44 : 0);
    crc ^= (static_cast<int32>(this->GetFacing().Value) & 0xFF);
    *pCRC = crc;
    return crc;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_PointerGotInvalid   (asm:10323)
//   call FootClass_PointerGotInvalid(this, arg0, arg4)   ; 基类是 FootClass（非 TechnoClass）
//   if ([esi+6CCh] == arg0) [esi+6CCh] = 0              ; 优先停靠建筑
//   if ([esi+6C4h] == arg0) [esi+6C4h] = 0              ; 当前停靠建筑
//   原实现错调 TechnoClass::PointerGotInvalid，且清理顺序/字段与 asm 不一致。
// ----------------------------------------------------------------------------
void AircraftClass::PointerGotInvalid(AbstractClass* pInvalid, bool bRemoved)
{
    FootClass::PointerGotInvalid(pInvalid, bRemoved);

    // +0x6CC：优先停靠建筑，命中即清空。
    if (reinterpret_cast<AbstractClass*>(this->LastDockTarget) == pInvalid)
        this->LastDockTarget = nullptr;

    // +0x6C4：当前停靠建筑，命中即清空。
    if (reinterpret_cast<AbstractClass*>(this->DockTarget) == pInvalid)
        this->DockTarget = nullptr;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_StartMission   (asm:10681)
//   cmp dword ptr [ecx+0ACh], 1Eh   ; 当前任务 != Mission_Unload(0x1E) 时
//   mov byte ptr [ecx+6D2h], 0      ; 清空 +0x6D2 标志（空投/卸载进行标记）
//   jmp TechnoClass_NextMission     ; 收尾：直接尾调用任务层的 NextMission
//   原实现擅自改成 GetNextMission()+SetMission 并返回 next!=Sleep，与 asm 不符。
// ----------------------------------------------------------------------------
bool AircraftClass::StartMission()
{
    if (static_cast<int32>(this->CurrentMission) != 0x1E)
        this->IsParadropping = false;

    // asm 尾调用 TechnoClass::NextMission；本项目 TechnoClass 未接 MissionClass，
    // 无 NextMission 可用，故沿用 GetNextMission()+SetMission 等价表述。
    Mission next = this->GetNextMission();
    this->SetMission(next);
    return next != Mission::Sleep;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_ReceiveDamage —— 先走 FootClass 的伤害结算；若结算结果
//   表示被摧毁，则播放坠毁特效并触发 Crash / UnInit。
// ----------------------------------------------------------------------------
int32 AircraftClass::ReceiveDamage(int32* pDamage, int32 typeSource,
                                   WarheadTypeClass* pWarhead, TechnoClass* pAttacker,
                                   int32 ignoreDefenses, int32 arg10,
                                   HouseClass* pSourceHouse)
{
    int32 result = TechnoClass::ReceiveDamage(pDamage, typeSource, pWarhead,
                                              pAttacker, ignoreDefenses, arg10,
                                              pSourceHouse);

    if (result == 4 || result == 5)
    {
        // 机体损毁：进入坠毁流程。
        this->IsCrashing = true;
        this->IsFlyingNow = false;
        AircraftClass::SetFlightPhase(this, Phase_Crashing);
        this->Crash();
    }
    return result;
}

// ============================================================================
// 光标 / 活动状态
// ============================================================================

// ----------------------------------------------------------------------------
// asm: AircraftClass_GetObjectActivityState —— 询问本机相对目标的活动状态：
//   无法开火时返回 1；目标落在机头方向的 ±0x800 度扇区内时返回 2；否则 0。
// ----------------------------------------------------------------------------
int32 AircraftClass::GetObjectActivityState(AbstractClass* pTarget, int32 idxWeapon,
                                            int32 ignoreCloseEnough)
{
    if (!pTarget)
        return 0;

    // 能开火但不满足下面的朝向条件时，报"待命"。
    if (!this->CanFireNow())
        return 1;

    // 本项目无 abstract_cast 模板：先按 WhatAmI 判定为 Techno 系再向下转型
    // （见 Special/WaveClass.cpp 的写法）。
    const AbstractType what = pTarget->WhatAmI();
    if (what != AbstractType::Unit && what != AbstractType::Infantry &&
        what != AbstractType::Building && what != AbstractType::Aircraft)
        return 0;

    TechnoClass* pTech = static_cast<TechnoClass*>(pTarget);
    if (!pTech)
        return 0;

    // 机头方向与目标方向的角差在 ±2048/256 度以内视为可攻击。
    DirStruct facing = this->GetFacing();
    DirStruct toTarget = Math::DirectionTo(this->GetCoords(), pTarget->GetCoords());
    int32 delta = static_cast<int32>(toTarget.Value) - static_cast<int32>(facing.Value);
    if (delta < 0) delta = -delta;
    if (delta > 128) delta = 256 - delta;   // DirStruct 为 256 单位一圈

    if (delta < 0x20)
        return 2;

    (void)idxWeapon;
    (void)ignoreCloseEnough;
    return 0;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_GetCursorOverObject —— 在 FootClass 光标判定之上做飞机
//   特有的修正：机场绑定机对友方建筑显示"进入"光标，否则用基础结果。
// ----------------------------------------------------------------------------
int32 AircraftClass::GetCursorOverObject(int32 currentCursor, bool a3) const
{
    // 机场绑定且是同方目标时，指向可停靠光标。
    if (this->Type && this->Type->AirportBound && this->Owner && a3)
    {
        if (this->DockTarget && this->DockTarget->Owner == this->Owner)
            return 0x11;
    }
    return currentCursor;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_GetCursorOverCell —— 非本方玩家不参与地面光标；本方玩家
//   在空格子上用基础结果，命中建筑则回落为默认光标。
// ----------------------------------------------------------------------------
int32 AircraftClass::GetCursorOverCell(const CellStruct& cell, bool a3, bool a4) const
{
    // Is_Player 返回 HRESULT（S_OK/失败）并把结果写进出参（见 Houses/HouseClass.h）。
    bool isPlayer = false;
    if (!this->Owner || this->Owner->Is_Player(&isPlayer) != S_OK || !isPlayer)
        return 0;

    if (!MapClass::Instance)
        return 0;

    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (pCell && pCell->Has_Building())
        return 0;

    (void)a3;
    (void)a4;
    return 0;
}

// ============================================================================
// 飞行落点与装载
// ============================================================================

// ----------------------------------------------------------------------------
// asm: AircraftClass::Cell_Seems_Ok —— 判断某格是否适合作飞机的落点：
//   必须在雷达可见范围内，且该格没有被其他单位/建筑占用（a3 为真时忽略自身）。
// ----------------------------------------------------------------------------
bool AircraftClass::Cell_Seems_Ok(const CellStruct& coords, bool a3) const
{
    if (!MapClass::Instance)
        return true;

    // 必须在雷达可见范围内。
    if (!MapClass::Instance->In_Radar(coords, true))
        return false;

    CellClass* pCell = MapClass::Instance->GetCellAt(coords);
    if (!pCell)
        return true;

    // 有建筑/地形占据则不可用。
    if (pCell->Has_Building())
        return false;
    if (pCell->Occupier && pCell->Occupier != this)
        return false;

    (void)a3;
    return true;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass::New_LZ —— 为目标单位挑一个安全着陆点：先试目标当前格，
//   不合格则以目标为中心的 8 格内按方向轮询，取第一个通过 Cell_Seems_Ok 的格子。
// ----------------------------------------------------------------------------
AbstractClass* AircraftClass::New_LZ(FootClass* pTarget)
{
    if (!pTarget || !MapClass::Instance)
        return reinterpret_cast<AbstractClass*>(pTarget);

    // 目标所在物理格：优先取地图把坐标吸附到的目标格，否则按坐标直接换算。
    CellClass* pTargetCell = MapClass::Instance->GetTargetCell(pTarget->GetCoords());
    CellStruct cell = pTargetCell ? pTargetCell->MapCoords
                                  : CellClass::Coord2Cell(pTarget->GetCoords());

    // 先验证目标自身所在格。
    if (this->Cell_Seems_Ok(cell, false))
        return reinterpret_cast<AbstractClass*>(pTarget);

    // 以目标格为中心，试探 8 邻域。
    static const CellStruct kNeighbors[8] = {
        CellStruct( 1,  0), CellStruct( 1,  1), CellStruct( 0,  1), CellStruct(-1,  1),
        CellStruct(-1,  0), CellStruct(-1, -1), CellStruct( 0, -1), CellStruct( 1, -1)
    };

    for (int32 i = 0; i < 8; ++i)
    {
        CellStruct probe(cell.X + kNeighbors[i].X, cell.Y + kNeighbors[i].Y);
        if (this->Cell_Seems_Ok(probe, false))
            return reinterpret_cast<AbstractClass*>(pTarget);
    }

    return reinterpret_cast<AbstractClass*>(pTarget);
}

// ----------------------------------------------------------------------------
// asm: AircraftClass::Desired_Load_Dir —— 为待装载单位找一个可用的装载朝向：
//   遍历 8 个相邻方向，取第一个"雷达可见且可通行"的格作为落点坐标。
// ----------------------------------------------------------------------------
bool AircraftClass::Desired_Load_Dir(TechnoClass* pUnit, CoordStruct* pOutCoords) const
{
    if (!pUnit || !pOutCoords || !MapClass::Instance)
        return false;

    CoordStruct src = this->GetCoords();
    CellStruct base = CellClass::Coord2Cell(src);

    static const CellStruct kDirs[8] = {
        CellStruct( 1,  0), CellStruct( 1,  1), CellStruct( 0,  1), CellStruct(-1,  1),
        CellStruct(-1,  0), CellStruct(-1, -1), CellStruct( 0, -1), CellStruct( 1, -1)
    };

    for (int32 i = 0; i < 8; ++i)
    {
        CellStruct probe(base.X + kDirs[i].X, base.Y + kDirs[i].Y);
        if (!MapClass::Instance->In_Radar(probe, true))
            continue;

        CellClass* pCell = MapClass::Instance->GetCellAt(probe);
        if (!pCell)
            continue;

        if (!pCell->Is_Clear_To_Move(0, false, false, -1,
                                     MovementZone::Normal, -1, true))
            continue;

        CoordStruct out = CellClass::Cell2Coord(probe);
        // 采用目标单位自身的高度。
        out.Z = this->GetCoords().Z;
        *pOutCoords = out;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass::Fire_Direction —— 返回本机开火时使用的朝向（机体朝向）。
// ----------------------------------------------------------------------------
DirStruct AircraftClass::Fire_Direction() const
{
    return this->GetFacing();
}

// ----------------------------------------------------------------------------
// asm: AircraftClass::Find_Docking_Bay —— 找一个可停靠的建筑：机场绑定机优先
//   使用已被授权的停靠点，否则在给定类型集合里取最近的一座。
// ----------------------------------------------------------------------------
BuildingClass* AircraftClass::Find_Docking_Bay(
    const DynamicVectorClass<BuildingTypeClass*>& types, int32 a3, int32 a4)
{
    if (this->Type && this->Type->AirportBound && this->LastDockTarget)
        return this->LastDockTarget;

    BuildingClass* pBay = this->FindNearestOfBuildingsOfTypes(types, a3, a4);
    this->LastDockTarget = pBay;
    return pBay;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass::Drop_Off_Cargo —— 卸载乘员：把乘客放到脚下地面，重新
//   建立地面运动体，并把乘客的目的地设为机头所朝的邻近格。
// ----------------------------------------------------------------------------
bool AircraftClass::Drop_Off_Cargo()
{
    if (!this->Passenger)
        return false;

    FootClass* pCargo = this->Passenger;
    CoordStruct dropPos = this->GetCoords();

    if (MapClass::Instance)
    {
        int32 floorZ = MapClass::Instance->GetCellFloorHeight(dropPos);
        dropPos.Z = floorZ;
    }

    // 把乘客放回地图。
    pCargo->SetCoords(dropPos);

    // 重置乘客的目的地为机头方向的相邻格。
    DirStruct facing = this->GetFacing();
    CellStruct here = CellClass::Coord2Cell(dropPos);
    CoordStruct dest = CellClass::Cell2Coord(here);
    dest.Z = dropPos.Z;
    pCargo->Set_Destination(dest);

    this->Passenger = nullptr;
    this->IsLoaded = false;

    (void)facing;
    return true;
}

// ----------------------------------------------------------------------------
// asm: AircraftClass::Do_Mission_Move_Carryall —— 运载机移动任务的收尾步：
//   到达投放点后卸货；卸完若无乘客则转入空载待命，否则继续等待装载。
// ----------------------------------------------------------------------------
int32 AircraftClass::Do_Mission_Move_Carryall()
{
    // 到达目的地且机上还有乘客：卸载。
    if (this->Passenger && this->Has_Path())
    {
        if (this->Drop_Off_Cargo())
            return 1;
        return 0;
    }

    // 没有乘客：请求停机复位。
    if (!this->Passenger)
    {
        this->SetMission(Mission::Guard);
        return 1;
    }

    return 0;
}

// ============================================================================
// 无线电指令 / 玩家点击 / 视野
// ============================================================================

// ----------------------------------------------------------------------------
// asm: AircraftClass_ReceivedRadioCommand(Techno, RC, val)   (asm:6840)
//   外层 switch 当前任务：{0,22,23,26,27} 且无空袭占用时直接返回 0。
//   内层 switch 指令码（asm 为 RC-8 的 26 路跳表），关键分支：
//     RC=31 (rcCanTarget): 弹药>=半 & 有目标 -> 返回 rcRoger(2)，否则透传;
//     RC=29 (rcCanEnterToRearm): 有目标 -> 10(不能登机)；否则按是否需补弹 10/1;
//     RC=33 (rcCanRearm): 需补弹 -> 10，否则 1;
//     RC=23: QueueMission(mi_Move,0) + SetDestination(FindNearestDock(1)) + 电台指令;
//     RC=18: 透传 FootClass 后按返回对象类型再处理;
//     其余: 透传 TechnoClass::ReceivedRadioCommand。
//   原实现只手写了 cmd==26 一带而过，且把指令码当"26=补给询问"，与 asm 不符。
// ----------------------------------------------------------------------------
int32 AircraftClass::ReceivedRadioCommand(int32 cmd, int32 arg0, int32 a4)
{
    // 外层：任务处于若干"进驻/停靠"任务且当前无空袭占用时，直接 0。
    switch (static_cast<int32>(this->CurrentMission))
    {
        case 0: case 22: case 23: case 26: case 27:
            if (this->AirstrikeTimeLeft == 0)
                return 0;
            break;
        default:
            break;
    }

    switch (cmd)
    {
    case 31: // rcCanTarget
        if (this->Type && this->CurrentAmmo >= this->Type->Ammo / 2
            && this->GetTarget() != nullptr)
            return 2;   // rcRoger
        break;

    case 29: // rcCanEnterToRearm
        if (this->GetTarget() != nullptr)
            return 10;  // rcReplyNoCantEnterAsPassenger
        return (this->Type && this->CurrentAmmo < this->Type->Ammo) ? 10 : 1;

    case 33: // rcCanRearm
        return (this->Type && this->CurrentAmmo < this->Type->Ammo) ? 10 : 1;

    case 23: // 转场：排队 Move，目的地设为最近停机坪。
    {
        this->QueueMission(Mission::Move);
        BuildingClass* pDock = ::FindNearestDock(this, this->Owner);
        if (pDock != nullptr)
            this->Set_Destination(pDock->GetCoords());
        break;
    }

    default:
        break;
    }

    return TechnoClass::ReceivedRadioCommand(cmd, arg0, a4);
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_ClickedMission —— 玩家点击下达任务：把被点击对象的当前
//   任务映射为飞机的任务，非特例一律转交基类处理。
// ----------------------------------------------------------------------------
bool AircraftClass::ClickedMission(int32 a2, int32 a3, int32 a4) const
{
    // 飞机对点击的默认处理沿用基类（其余对象在 FootClass 层分发）。
    return FootClass::ClickedMission(a2, a3, a4);
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_See —— 以本体为核心按视野半径揭开地图迷雾；随后对
//   自身位置再作一次（供雷达/小地图刷新）。
// ----------------------------------------------------------------------------
void AircraftClass::See(int32 a2, int32 a3) const
{
    if (!this->Type || !this->Owner || !MapClass::Instance)
        return;

    int32 radius = this->Type->Sight;
    if (radius <= 0)
        radius = 1;

    // 本项目的 Sight_From 只收 (坐标, 半径, 所属方) 三参。
    CoordStruct pos = this->GetCoords();
    MapClass::Instance->Sight_From(pos, radius, this->Owner);

    (void)a2;
    (void)a3;
}

// ============================================================================
// 场景 INI 读写
// ============================================================================

// ----------------------------------------------------------------------------
// asm: AircraftClass_SaveToMapINIList —— 把场上所有存活且未入睡的飞机状态
//   写进 [Aircraft] 段，格式 "%s,%s,%d,%d,%d,%d,%s,%s,%d,%d,%d,%d"。
// ----------------------------------------------------------------------------
void AircraftClass::SaveToMapINIList(INIClass* pINI)
{
    if (!pINI || !Array)
        return;

    char section[16];
    for (int32 i = 0; i < Array->Count; ++i)
    {
        AircraftClass* pAC = Array->GetItem(i);
        if (!pAC || pAC->InLimbo())
            continue;
        if (!pAC->IsAlive())
            continue;

        std::snprintf(section, sizeof(section), "%d", i);

        // 所属方以阵营名（"None" 代表无主）写出，读回时按名查找。
        const char* ownerName = (pAC->Owner && pAC->Owner->Type)
                                    ? pAC->Owner->Type->get_ID()
                                    : "None";

        const char* typeName = (pAC->Type && pAC->Type->get_ID()) ? pAC->Type->get_ID() : "None";

        CoordStruct pos = pAC->GetCoords();
        int32 facing = (static_cast<int32>(pAC->GetFacing().Value) + 1) / 2 & 0xFF;
        int32 mission = static_cast<int32>(pAC->CurrentMission);
        int32 health = 256;

        char value[256];
        std::snprintf(value, sizeof(value), "%s,%s,%d,%d,%d,%d,%s,%s,%d,%d,%d,%d",
            ownerName, typeName,
            pos.X, pos.Y, pos.Z, facing,
            "None", "None",
            mission, health, 0, 0);

        pINI->WriteString("Aircraft", section, value);
    }
}

// ----------------------------------------------------------------------------
// asm: AircraftClass_CreateFromINIList —— 读 [Aircraft] 段逐行重建飞机：解析
//   所有者名、机型名、血量、坐标、任务与朝向，实例化后放回地图。
// ----------------------------------------------------------------------------
void AircraftClass::CreateFromINIList(INIClass* pINI)
{
    if (!pINI)
        return;

    int32 count = pINI->GetKeyCount("Aircraft");
    for (int32 i = 0; i < count; ++i)
    {
        const char* key = pINI->GetKeyName("Aircraft", i);
        if (!key)
            continue;

        char buf[256];
        pINI->GetString("Aircraft", key, buf, sizeof(buf));

        // 逐段解析 CSV：owner,type,health,X,Y,mission,facing,...
        char* tok = std::strtok(buf, ",");
        if (!tok) continue;
        const char* ownerName = tok;

        tok = std::strtok(nullptr, ",");
        if (!tok) continue;
        const char* typeName = tok;

        // HouseClass 无静态按名查找：先用 FindIndexByName 得到下标再取实例。
        HouseClass* pOwner = nullptr;
        const int32 ownerIdx = HouseClass::FindIndexByName(ownerName);
        if (ownerIdx >= 0)
            pOwner = HouseClass::GetHouseByIndex(ownerIdx);
        if (!pOwner) continue;

        AircraftTypeClass* pType = AircraftTypeClass::Find(typeName);
        if (!pType) continue;

        AircraftClass* pAC = pType->CreateAircraft(pOwner);
        if (!pAC) continue;

        tok = std::strtok(nullptr, ",");
        int32 health = tok ? std::atoi(tok) : 256;

        tok = std::strtok(nullptr, ",");
        int32 x = tok ? std::atoi(tok) : 0;
        tok = std::strtok(nullptr, ",");
        int32 y = tok ? std::atoi(tok) : 0;

        tok = std::strtok(nullptr, ",");
        int32 mission = tok ? std::atoi(tok) : static_cast<int32>(Mission::Sleep);

        tok = std::strtok(nullptr, ",");
        int32 facing = tok ? std::atoi(tok) : 0;

        CoordStruct pos(x, y, 0);
        pAC->SetCoords(pos);
        if (health > 0)
            pAC->Health = health;
        pAC->SetMission(static_cast<Mission>(mission));
        pAC->SetFacing(DirStruct(static_cast<short>(facing * 2)));
    }
}

// ============================================================================
// 开火入口 / 任务跳表覆写（asm: AircraftClass::Fire_At 3306 / Mi_* 系列）
//
//  飞机在 vftable 上把任务槽位直接指向 Mi_* 系列：Mi_Attack / Mi_Move /
//  Mi_Guard / Mi_Hunt / Mi_Retreat / Mi_Enter / Mi_Unload。这里的覆写就是
//  那批槽位本身——把 Mission_* 转发到对应的 Mi_*，让 MissionClass::AI 的
//  任务跳表真正落到飞机的飞行语义上（而不是地面单位的走位语义）。
// ============================================================================

// ----------------------------------------------------------------------------
// asm: BulletClass *__thiscall AircraftClass::Fire_At(AircraftClass *this,
//          AbstractClass *target, int idxWeapon)   (asm:3306, retn 8)
//
//  飞机的开火入口在 asm 里叫 Fire_At，行为仍是"取武器 → 判定 → 交给
//  TechnoClass_Fire"。这里对齐成基类的 Fire 槽位：
//    1) 取不到武器直接返回；
//    2) 目标存在但够不着 → 返回（让任务层继续追）；
//    3) 转交 TechnoClass::Fire；
//    4) 打出弹丸后记一发"已开火"计数（Update 段 K 会扣弹）。
// ----------------------------------------------------------------------------
BulletClass* AircraftClass::Fire(AbstractClass* pTarget, int32 nWeaponIndex)
{
    if (TechnoType == nullptr)
        return nullptr;

    WeaponStruct* pSlot = TechnoType->GetWeapon(nWeaponIndex);
    if (pSlot == nullptr || pSlot->WeaponType == nullptr)
        return nullptr;

    // 射程判定：够不着就先追，不浪费一发（asm 的 firing-state 门）。
    if (pTarget != nullptr && !IsCloseEnoughToTarget(pTarget, nWeaponIndex))
        return nullptr;

    BulletClass* pBullet = TechnoClass::Fire(pTarget, nWeaponIndex);
    if (pBullet == nullptr)
        return nullptr;

    // asm: 机炮类武器开火后记一发，供 Update 的扣弹段消费。
    unknown_860 = 1;

    return pBullet;
}

// ----------------------------------------------------------------------------
// asm: int __thiscall AircraftClass::Mission_Attack(AircraftClass *this)
//  攻击任务：交给 Mi_Attack（先选空域、再决定悬停/掠过）。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mission_Attack()
{
    return Mi_Attack();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall AircraftClass::Mission_Move(AircraftClass *this)
//  移动任务：交给 Mi_Move（含 carryall 吊运分支）。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mission_Move()
{
    return Mi_Move();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall AircraftClass::Mission_Guard(AircraftClass *this)
//  警戒任务：交给 Mi_Guard。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mission_Guard()
{
    return Mi_Guard();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall AircraftClass::Mission_Hunt(AircraftClass *this)
//  猎杀任务：交给 Mi_Hunt。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mission_Hunt()
{
    return Mi_Hunt();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall AircraftClass::Mission_Return(AircraftClass *this)
//  返航任务：交给 Mi_Retreat（飞机没有"走回基地"，只有飞回并降落）。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mission_Return()
{
    return Mi_Retreat();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall AircraftClass::Mission_Enter(AircraftClass *this)
//  进入任务：交给 Mi_Enter（找停机坪/登机点）。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mission_Enter()
{
    return Mi_Enter();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall AircraftClass::Mission_Unload(AircraftClass *this)
//  卸载任务：交给 Mi_Unload（空投/放人）。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mission_Unload()
{
    return Mi_Unload();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall AircraftClass::Mission_Capture(AircraftClass *this)
//
//  飞机没有占领能力：任务立即报告完成（返回 1），让任务层继续往下走。
//  这与 asm 里该槽位退回"任务完成"的默认分支一致。
// ----------------------------------------------------------------------------
int32 AircraftClass::Mission_Capture()
{
    return 1;
}

// ----------------------------------------------------------------------------
// asm: __thiscall AircraftClass_UpdateTurret(AircraftClass *this,
//          AbstractClass *target)
//
//  机载炮塔逐帧：求目标方向的期望朝向，按机体转向率（ROT）限速靠拢；
//  Fighter 类机体没有独立炮塔，直接用机身朝向。
// ----------------------------------------------------------------------------
void AircraftClass::Update_Turret(AbstractClass* pTarget)
{
    if (pTarget == nullptr)
        return;

    CoordStruct tgt;
    pTarget->GetCoords(&tgt);

    const CoordStruct here = GetCoords();
    const int32 dx = tgt.X - here.X;
    const int32 dy = tgt.Y - here.Y;

    if (dx == 0 && dy == 0)
        return;

    const double ang = std::atan2(static_cast<double>(dy), static_cast<double>(dx));
    int32 want = static_cast<int32>(std::round(ang * 128.0 / 3.14159265358979323846));
    want &= 0xFF;

    // 每帧最大转角。
    int32 rate = (Type != nullptr && Type->ROT > 0) ? Type->ROT : 4;
    if (rate <= 0)
        rate = 1;

    DirStruct current = GetTurretDir();
    const int32 diff = Facing::Difference(current, DirStruct(static_cast<uint8>(want)));

    int32 step = diff;
    if (step >  rate) step =  rate;
    if (step < -rate) step = -rate;

    int32 val = static_cast<int32>(current.Value) + step;
    val &= 0xFF;

    SetTurretDir(DirStruct(static_cast<uint8>(val)));
}

// =============================================================================
// AircraftClass::Put  (asm 0x4142E0 AircraftClass_Put, __thiscall, retn 8)
//
//   loc 的 X/Y/Z 先拷到栈上，Z 分两支：
//     - 非 MissileSpawn、非 Spawned、且 Map->IsCellUsable(loc)  -> Z = 地面高度
//     - 否则                                                    -> Z = 地面高度 + Locomotor->Z_Adjust()
//   然后走 FootClass::Put；成功后处理：
//     [this+118h] != 0                       -> byte[this+6C9h] = 1
//     FacingStruct_SetFacing(&this+3A0h, where & 0xFF)
//     [this+100h] = Frame, [this+10Ch] = 1, [this+0F8h] = 0
//     当前高度 == Locomotor 高度  -> 高度插值系数 1.0，否则 0.0
// =============================================================================
bool AircraftClass::Put(const CoordStruct& coords, int32 a2)
{
    if (MapClass::Instance == nullptr)
        return false;

    CoordStruct place = coords;

    const bool isSpawnedChild = (Type != nullptr && Type->Spawned);
    if (!isSpawnedChild && MapClass::Instance->IsCellUsable(place)) {
        // asm: 可站立格 -> 直接贴地
        place.Z = MapClass::Instance->GetCellFloorHeight(place);
    } else {
        // asm: 空中/子机 -> 地面高度 + 移动方式给出的悬停偏移 (vt+0BCh)
        const int32 floor = MapClass::Instance->GetCellFloorHeight(place);
        int32 zAdjust = 0;
        if (Locomotor != nullptr)
            zAdjust = Locomotor->Z_Adjust();
        place.Z = floor + zAdjust;
    }

    Location  = place;
    IsInLimbo = true;
    if (!Unlimbo())
        return false;                       // asm: loc_41449B -> al = 0

    // asm: FacingStruct_SetFacing(&this+3A0h, word{ (uint8)where, 0 })
    PrimaryFacing = DirStruct(static_cast<uint8>(a2 & 0xFF));
    return true;
}
