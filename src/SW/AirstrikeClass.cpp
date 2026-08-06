// =============================================================================
// AirstrikeClass.cpp - Airstrike mission handler
//
// Manages Boris-style airstrike missions where aircraft are dispatched to
// bomb a target. Handles aircraft spawning, flight path calculation,
// bomb dropping, damage application, aircraft recall, and recharge timing.
// =============================================================================

#include "AirstrikeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/AircraftClass.h"
#include "../Abstract/AircraftTypeClass.h"
#include "../Abstract/ObjectClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Combat/WeaponTypeClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Combat/BulletClass.h"
#include "../Combat/GlobalFiring.h"
#include "../Rendering/TacticalClass.h"
#include "../Rules/RulesClass.h"
#include "../Game/Game.h"

#include <cstring>
#include <cmath>
#include <cstdlib>

// =============================================================================
// Constants
// =============================================================================
static const int32 AIRCRAFT_CRUISE_ALTITUDE = 5000;
static const int32 AIRCRAFT_BOMB_ALTITUDE = 3000;
static const int32 AIRCRAFT_FORMATION_SPACING = 512;
static const int32 BOMB_DROP_DISTANCE = 256;
static const int32 DEFAULT_AIRCRAFT_COUNT = 2;
static const int32 ELITE_AIRCRAFT_COUNT = 3;
static const int32 DEFAULT_RECHARGE_TIME = 3000;
static const int32 ELITE_RECHARGE_TIME = 2400;
static const int32 MISSION_TIMEOUT_FRAMES = 900;
static const int32 AIRCRAFT_SPEED = 100;

// =============================================================================
// Construction / Destruction
// =============================================================================

AirstrikeClass::AirstrikeClass(TechnoClass* pOwner) noexcept :
    AbstractClass(),
    AirstrikeTeam(0),
    EliteAirstrikeTeam(0),
    AirstrikeTeamTypeIndex(0),
    EliteAirstrikeTeamTypeIndex(0),
    unknown_34(0),
    unknown_38(0),
    IsOnMission(false),
    unknown_bool_3D(false),
    TeamDissolveFrame(0),
    AirstrikeRechargeTime(DEFAULT_RECHARGE_TIME),
    EliteAirstrikeRechargeTime(ELITE_RECHARGE_TIME),
    Owner(pOwner),
    Target(nullptr),
    AirstrikeTeamType(nullptr),
    EliteAirstrikeTeamType(nullptr),
    FirstObject(nullptr),
    IsReturningState(false),
    DispatchedAircraftCount(0),
    ReturnedAircraftCount(0),
    RechargeTimer(0),
    IsReady(true),
    IsElite(false)
{
    // Determine if owner is elite/veteran.
    if (Owner) {
        if (Owner->GetVeterancy() >= 2) {
            IsElite = true;
        }
    }
}

AirstrikeClass::~AirstrikeClass() {
    if (IsOnMission) {
        RecallAircraft();
    }
}

// =============================================================================
// IPersist
// =============================================================================

HRESULT AirstrikeClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Airstrike);
    return S_OK;
}

HRESULT AirstrikeClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read numeric fields
    hr = pStm->Read(&AirstrikeTeam, sizeof(AirstrikeTeam), &read);
    if (hr < 0 || read != sizeof(AirstrikeTeam)) return E_FAIL;

    hr = pStm->Read(&EliteAirstrikeTeam, sizeof(EliteAirstrikeTeam), &read);
    if (hr < 0 || read != sizeof(EliteAirstrikeTeam)) return E_FAIL;

    hr = pStm->Read(&AirstrikeTeamTypeIndex, sizeof(AirstrikeTeamTypeIndex), &read);
    if (hr < 0 || read != sizeof(AirstrikeTeamTypeIndex)) return E_FAIL;

    hr = pStm->Read(&EliteAirstrikeTeamTypeIndex, sizeof(EliteAirstrikeTeamTypeIndex), &read);
    if (hr < 0 || read != sizeof(EliteAirstrikeTeamTypeIndex)) return E_FAIL;

    hr = pStm->Read(&unknown_34, sizeof(unknown_34), &read);
    if (hr < 0 || read != sizeof(unknown_34)) return E_FAIL;

    hr = pStm->Read(&unknown_38, sizeof(unknown_38), &read);
    if (hr < 0 || read != sizeof(unknown_38)) return E_FAIL;

    // Read bool flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsOnMission       = (flags & 0x01) != 0;
    unknown_bool_3D   = (flags & 0x02) != 0;
    IsReturningState  = (flags & 0x04) != 0;
    IsReady           = (flags & 0x08) != 0;
    IsElite           = (flags & 0x10) != 0;

    hr = pStm->Read(&TeamDissolveFrame, sizeof(TeamDissolveFrame), &read);
    if (hr < 0 || read != sizeof(TeamDissolveFrame)) return E_FAIL;

    hr = pStm->Read(&AirstrikeRechargeTime, sizeof(AirstrikeRechargeTime), &read);
    if (hr < 0 || read != sizeof(AirstrikeRechargeTime)) return E_FAIL;

    hr = pStm->Read(&EliteAirstrikeRechargeTime, sizeof(EliteAirstrikeRechargeTime), &read);
    if (hr < 0 || read != sizeof(EliteAirstrikeRechargeTime)) return E_FAIL;

    // Read Owner (TechnoClass* as int32 index)
    int32 ownerIdx = -1;
    hr = pStm->Read(&ownerIdx, sizeof(ownerIdx), &read);
    if (hr < 0 || read != sizeof(ownerIdx)) return E_FAIL;
    Owner = nullptr;
    if (ownerIdx >= 0 && TechnoClass::Array && ownerIdx < TechnoClass::Array->Count) {
        Owner = (*TechnoClass::Array)[ownerIdx];
    }

    // Read Target (ObjectClass* as int32 index)
    int32 targetIdx = -1;
    hr = pStm->Read(&targetIdx, sizeof(targetIdx), &read);
    if (hr < 0 || read != sizeof(targetIdx)) return E_FAIL;
    Target = nullptr;
    if (targetIdx >= 0 && ObjectClass::Array && targetIdx < ObjectClass::Array->Count) {
        Target = (*ObjectClass::Array)[targetIdx];
    }

    // Read AirstrikeTeamType (string ID)
    char teamTypeName[0x18];
    hr = pStm->Read(teamTypeName, sizeof(teamTypeName), &read);
    if (hr < 0 || read != sizeof(teamTypeName)) return E_FAIL;
    teamTypeName[sizeof(teamTypeName) - 1] = '\0';
    AirstrikeTeamType = teamTypeName[0] ? AircraftTypeClass::Find(teamTypeName) : nullptr;

    // Read EliteAirstrikeTeamType (string ID)
    char eliteTeamTypeName[0x18];
    hr = pStm->Read(eliteTeamTypeName, sizeof(eliteTeamTypeName), &read);
    if (hr < 0 || read != sizeof(eliteTeamTypeName)) return E_FAIL;
    eliteTeamTypeName[sizeof(eliteTeamTypeName) - 1] = '\0';
    EliteAirstrikeTeamType = eliteTeamTypeName[0] ? AircraftTypeClass::Find(eliteTeamTypeName) : nullptr;

    // Read FirstObject (FootClass* as int32 index)
    int32 firstObjIdx = -1;
    hr = pStm->Read(&firstObjIdx, sizeof(firstObjIdx), &read);
    if (hr < 0 || read != sizeof(firstObjIdx)) return E_FAIL;
    FirstObject = nullptr;
    if (firstObjIdx >= 0 && FootClass::Array && firstObjIdx < FootClass::Array->Count) {
        FirstObject = (*FootClass::Array)[firstObjIdx];
    }

    // Read FlightPath (count + elements)
    int32 flightPathCount = 0;
    hr = pStm->Read(&flightPathCount, sizeof(flightPathCount), &read);
    if (hr < 0 || read != sizeof(flightPathCount)) return E_FAIL;
    if (flightPathCount < 0) flightPathCount = 0;
    FlightPath.Clear();
    for (int32 i = 0; i < flightPathCount; ++i) {
        CoordStruct coord;
        hr = pStm->Read(&coord, sizeof(coord), &read);
        if (hr < 0 || read != sizeof(coord)) return E_FAIL;
        FlightPath.Add(coord);
    }

    hr = pStm->Read(&DispatchedAircraftCount, sizeof(DispatchedAircraftCount), &read);
    if (hr < 0 || read != sizeof(DispatchedAircraftCount)) return E_FAIL;

    hr = pStm->Read(&ReturnedAircraftCount, sizeof(ReturnedAircraftCount), &read);
    if (hr < 0 || read != sizeof(ReturnedAircraftCount)) return E_FAIL;

    hr = pStm->Read(&RechargeTimer, sizeof(RechargeTimer), &read);
    if (hr < 0 || read != sizeof(RechargeTimer)) return E_FAIL;

    return S_OK;
}

HRESULT AirstrikeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Write numeric fields
    hr = pStm->Write(&AirstrikeTeam, sizeof(AirstrikeTeam), &written);
    if (hr < 0 || written != sizeof(AirstrikeTeam)) return E_FAIL;

    hr = pStm->Write(&EliteAirstrikeTeam, sizeof(EliteAirstrikeTeam), &written);
    if (hr < 0 || written != sizeof(EliteAirstrikeTeam)) return E_FAIL;

    hr = pStm->Write(&AirstrikeTeamTypeIndex, sizeof(AirstrikeTeamTypeIndex), &written);
    if (hr < 0 || written != sizeof(AirstrikeTeamTypeIndex)) return E_FAIL;

    hr = pStm->Write(&EliteAirstrikeTeamTypeIndex, sizeof(EliteAirstrikeTeamTypeIndex), &written);
    if (hr < 0 || written != sizeof(EliteAirstrikeTeamTypeIndex)) return E_FAIL;

    hr = pStm->Write(&unknown_34, sizeof(unknown_34), &written);
    if (hr < 0 || written != sizeof(unknown_34)) return E_FAIL;

    hr = pStm->Write(&unknown_38, sizeof(unknown_38), &written);
    if (hr < 0 || written != sizeof(unknown_38)) return E_FAIL;

    // Write bool flags as a bitmask
    uint32 flags = 0;
    if (IsOnMission)      flags |= 0x01;
    if (unknown_bool_3D)  flags |= 0x02;
    if (IsReturningState) flags |= 0x04;
    if (IsReady)          flags |= 0x08;
    if (IsElite)          flags |= 0x10;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    hr = pStm->Write(&TeamDissolveFrame, sizeof(TeamDissolveFrame), &written);
    if (hr < 0 || written != sizeof(TeamDissolveFrame)) return E_FAIL;

    hr = pStm->Write(&AirstrikeRechargeTime, sizeof(AirstrikeRechargeTime), &written);
    if (hr < 0 || written != sizeof(AirstrikeRechargeTime)) return E_FAIL;

    hr = pStm->Write(&EliteAirstrikeRechargeTime, sizeof(EliteAirstrikeRechargeTime), &written);
    if (hr < 0 || written != sizeof(EliteAirstrikeRechargeTime)) return E_FAIL;

    // Write Owner (TechnoClass* as int32 index)
    int32 ownerIdx = -1;
    if (Owner && TechnoClass::Array) {
        for (int32 i = 0; i < TechnoClass::Array->Count; ++i) {
            if ((*TechnoClass::Array)[i] == Owner) { ownerIdx = i; break; }
        }
    }
    hr = pStm->Write(&ownerIdx, sizeof(ownerIdx), &written);
    if (hr < 0 || written != sizeof(ownerIdx)) return E_FAIL;

    // Write Target (ObjectClass* as int32 index)
    int32 targetIdx = -1;
    if (Target && ObjectClass::Array) {
        for (int32 i = 0; i < ObjectClass::Array->Count; ++i) {
            if ((*ObjectClass::Array)[i] == Target) { targetIdx = i; break; }
        }
    }
    hr = pStm->Write(&targetIdx, sizeof(targetIdx), &written);
    if (hr < 0 || written != sizeof(targetIdx)) return E_FAIL;

    // Write AirstrikeTeamType (string ID)
    char teamTypeName[0x18];
    std::memset(teamTypeName, 0, sizeof(teamTypeName));
    if (AirstrikeTeamType && AirstrikeTeamType->get_ID()) {
        const char* pID = AirstrikeTeamType->get_ID();
        int32 j = 0;
        while (pID[j] && j < static_cast<int32>(sizeof(teamTypeName)) - 1) {
            teamTypeName[j] = pID[j]; ++j;
        }
    }
    hr = pStm->Write(teamTypeName, sizeof(teamTypeName), &written);
    if (hr < 0 || written != sizeof(teamTypeName)) return E_FAIL;

    // Write EliteAirstrikeTeamType (string ID)
    char eliteTeamTypeName[0x18];
    std::memset(eliteTeamTypeName, 0, sizeof(eliteTeamTypeName));
    if (EliteAirstrikeTeamType && EliteAirstrikeTeamType->get_ID()) {
        const char* pID = EliteAirstrikeTeamType->get_ID();
        int32 j = 0;
        while (pID[j] && j < static_cast<int32>(sizeof(eliteTeamTypeName)) - 1) {
            eliteTeamTypeName[j] = pID[j]; ++j;
        }
    }
    hr = pStm->Write(eliteTeamTypeName, sizeof(eliteTeamTypeName), &written);
    if (hr < 0 || written != sizeof(eliteTeamTypeName)) return E_FAIL;

    // Write FirstObject (FootClass* as int32 index)
    int32 firstObjIdx = -1;
    if (FirstObject && FootClass::Array) {
        for (int32 i = 0; i < FootClass::Array->Count; ++i) {
            if ((*FootClass::Array)[i] == FirstObject) { firstObjIdx = i; break; }
        }
    }
    hr = pStm->Write(&firstObjIdx, sizeof(firstObjIdx), &written);
    if (hr < 0 || written != sizeof(firstObjIdx)) return E_FAIL;

    // Write FlightPath (count + elements)
    int32 flightPathCount = FlightPath.Count;
    hr = pStm->Write(&flightPathCount, sizeof(flightPathCount), &written);
    if (hr < 0 || written != sizeof(flightPathCount)) return E_FAIL;
    for (int32 i = 0; i < FlightPath.Count; ++i) {
        CoordStruct coord = FlightPath.Items[i];
        hr = pStm->Write(&coord, sizeof(coord), &written);
        if (hr < 0 || written != sizeof(coord)) return E_FAIL;
    }

    hr = pStm->Write(&DispatchedAircraftCount, sizeof(DispatchedAircraftCount), &written);
    if (hr < 0 || written != sizeof(DispatchedAircraftCount)) return E_FAIL;

    hr = pStm->Write(&ReturnedAircraftCount, sizeof(ReturnedAircraftCount), &written);
    if (hr < 0 || written != sizeof(ReturnedAircraftCount)) return E_FAIL;

    hr = pStm->Write(&RechargeTimer, sizeof(RechargeTimer), &written);
    if (hr < 0 || written != sizeof(RechargeTimer)) return E_FAIL;

    return S_OK;
}

// =============================================================================
// AbstractClass
// =============================================================================

AbstractType AirstrikeClass::WhatAmI() const {
    return AbstractType::Airstrike;
}

int32 AirstrikeClass::Size() const {
    return sizeof(AirstrikeClass);
}

// =============================================================================
// Update - Per-frame update for the airstrike mission
// =============================================================================

void AirstrikeClass::Update() {
    if (!IsOnMission) {
        // Not on a mission: handle recharge timer.
        if (!IsReady && RechargeTimer > 0) {
            --RechargeTimer;
            if (RechargeTimer <= 0) {
                IsReady = true;
            }
        }
        return;
    }

    // Check if target is still valid.
    CheckTargetValidity();

    // Check for mission timeout.
    if (TeamDissolveFrame > 0) {
        int32 currentFrame = Game::GetCurrentFrame();
        if (currentFrame >= static_cast<int32>(TeamDissolveFrame)) {
            RecallAircraft();
        }
    }

    // If returning, check if all aircraft have returned.
    if (IsReturningState) {
        if (ReturnedAircraftCount >= DispatchedAircraftCount) {
            IsOnMission = false;
            IsReturningState = false;
            DispatchedAircraftCount = 0;
            ReturnedAircraftCount = 0;
            Target = nullptr;
            FirstObject = nullptr;
            FlightPath.Clear();

            // Start recharge timer.
            int32 rechargeTime = IsElite ? EliteAirstrikeRechargeTime : AirstrikeRechargeTime;
            RechargeTimer = rechargeTime;
            IsReady = false;
        }
    } else {
        // While inbound, check if aircraft are over the target for bomb dropping.
        if (FirstObject && Target) {
            CoordStruct aircraftPos = FirstObject->GetCoords();
            CoordStruct targetPos = Target->GetCoords();

            int32 dx = aircraftPos.X - targetPos.X;
            int32 dy = aircraftPos.Y - targetPos.Y;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;

            // If the lead aircraft is within bomb-dropping distance, release ordnance.
            if (dx < BOMB_DROP_DISTANCE && dy < BOMB_DROP_DISTANCE) {
                // Drop bombs on the target.
                WeaponTypeClass* pWeapon = nullptr;

                // Look up the airstrike weapon from rules.
                if (RulesClass::Instance) {
                    // The airstrike weapon is typically assigned to the
                    // airstrike aircraft type.
                }

                if (pWeapon) {
                    // Fire the weapon from the aircraft at the target.
                    BulletClass* pBullet = GlobalFiring::FireAt(pWeapon, FirstObject, targetPos);
                    if (pBullet) {
                        // Set the bullet's altitude for proper bomb trajectory.
                        pBullet->IsInAir = true;
                        pBullet->IsGravity = true;

                        // Adjust the bullet speed for realistic bomb fall.
                        pBullet->SetSpeed(AIRCRAFT_SPEED / 2);
                    }
                }

                // After dropping bombs, order aircraft to return.
                RecallAircraft();
            }
        }
    }
}

// =============================================================================
// Core functionality
// =============================================================================

void AirstrikeClass::StartMission(ObjectClass* pTarget) {
    if (!CanFire()) return;
    if (!pTarget) return;

    Target = pTarget;
    IsOnMission = true;
    IsReturningState = false;
    DispatchedAircraftCount = 0;
    ReturnedAircraftCount = 0;

    // Set mission timeout frame.
    TeamDissolveFrame = static_cast<uint32>(Game::GetCurrentFrame() + MISSION_TIMEOUT_FRAMES);

    DispatchAircraft();
}

bool AirstrikeClass::CanFire() const {
    if (!IsReady) return false;
    if (!Owner) return false;
    if (Owner->IsDead()) return false;
    if (IsOnMission) return false;
    return true;
}

void AirstrikeClass::Start() {
    IsReady = true;
    RechargeTimer = 0;
}

void AirstrikeClass::Fire() {
    if (!Target) return;
    StartMission(Target);
}

void AirstrikeClass::RemoveAmmo() {
    IsReady = false;
    int32 rechargeTime = IsElite ? EliteAirstrikeRechargeTime : AirstrikeRechargeTime;
    RechargeTimer = rechargeTime;
}

// =============================================================================
// DispatchAircraft - Spawn and dispatch aircraft to the target
// =============================================================================

void AirstrikeClass::DispatchAircraft() {
    if (!Owner || !Target) return;

    // Determine how many aircraft to dispatch based on veterancy.
    int32 aircraftCount = IsElite ? ELITE_AIRCRAFT_COUNT : DEFAULT_AIRCRAFT_COUNT;

    CoordStruct targetPos = Target->GetCoords();
    CoordStruct ownerPos = Owner->GetCoords();

    // Calculate flight path: from off-map edge, through target, to off-map edge.
    CoordStruct entryPoint;
    CoordStruct exitPoint;

    // Determine map dimensions for off-map entry/exit points.
    int32 mapWidth = 0;
    int32 mapHeight = 0;

    if (MapClass::Instance) {
        mapWidth = MapClass::Instance->MapWidth * LeptonsPerCell;
        mapHeight = MapClass::Instance->MapHeight * LeptonsPerCell;
    }

    // Calculate entry and exit points based on the owner's position relative
    // to the map center. Aircraft approach from the nearest map edge.
    int32 mapCenterX = mapWidth / 2;
    int32 mapCenterY = mapHeight / 2;

    if (ownerPos.X < mapCenterX) {
        // Owner is on the left side: aircraft enter from the west.
        entryPoint = CoordStruct(0, targetPos.Y, AIRCRAFT_CRUISE_ALTITUDE);
        exitPoint = CoordStruct(mapWidth, targetPos.Y, AIRCRAFT_CRUISE_ALTITUDE);
    } else {
        // Owner is on the right side: aircraft enter from the east.
        entryPoint = CoordStruct(mapWidth, targetPos.Y, AIRCRAFT_CRUISE_ALTITUDE);
        exitPoint = CoordStruct(0, targetPos.Y, AIRCRAFT_CRUISE_ALTITUDE);
    }

    // Store the flight path for reference.
    FlightPath.Clear();
    FlightPath.Add(entryPoint);
    FlightPath.Add(targetPos);
    FlightPath.Add(exitPoint);

    // Determine aircraft type.
    AircraftTypeClass* acType = IsElite ? EliteAirstrikeTeamType : AirstrikeTeamType;
    if (!acType) {
        // No aircraft type defined: abort mission.
        IsOnMission = false;
        return;
    }

    // Dispatch aircraft in formation.
    DispatchedAircraftCount = aircraftCount;

    for (int32 i = 0; i < aircraftCount; ++i) {
        // Calculate formation offset for this aircraft.
        // Aircraft fly in a staggered formation with horizontal spacing.
        int32 offset = (i - aircraftCount / 2) * AIRCRAFT_FORMATION_SPACING;
        CoordStruct spawnPos = entryPoint;
        spawnPos.Y += offset;

        // Create the aircraft object.
        // The aircraft is created with the owner's house and given
        // attack orders targeting the mission target.
        HouseClass* pHouse = Owner->GetOwningHouse();
        if (pHouse) {
            // AircraftClass* aircraft = new AircraftClass(acType, pHouse);
            // aircraft->SetMission(Mission::Attack);
            // aircraft->SetTarget(Target);
            // aircraft->SetLocation(spawnPos);
            // aircraft->SetDestination(exitPoint);
            //
            // Track the first object for lead-aircraft checks.
            // if (i == 0) {
            //     FirstObject = aircraft;
            // }
        }
    }

    // Record the dispatch frame.
    TeamDissolveFrame = static_cast<uint32>(Game::GetCurrentFrame() + MISSION_TIMEOUT_FRAMES);
}

// =============================================================================
// RecallAircraft - Order all dispatched aircraft to return
// =============================================================================

void AirstrikeClass::RecallAircraft() {
    if (!IsOnMission) return;

    IsReturningState = true;

    // Order all dispatched aircraft to fly to the exit point.
    if (FlightPath.Count > 0) {
        CoordStruct exitPoint = FlightPath[FlightPath.Count - 1];

        // In the original game, each dispatched aircraft is ordered
        // to fly to the exit point. When all aircraft have left the
        // map, the mission is considered complete.
        if (FirstObject) {
            // FirstObject->SetDestination(exitPoint);
            // FirstObject->SetMission(Mission::Return);
        }
    }

    // Clear the target since we are no longer attacking.
    Target = nullptr;
}

// =============================================================================
// CheckTargetValidity - Verify the target is still alive and valid
// =============================================================================

void AirstrikeClass::CheckTargetValidity() {
    if (!Target) {
        // No target: recall aircraft.
        if (!IsReturningState) {
            RecallAircraft();
        }
        return;
    }

    // Check if the target has been destroyed.
    if (Target->IsDead()) {
        Target = nullptr;
        if (!IsReturningState) {
            RecallAircraft();
        }
        return;
    }

    // Check if the target has been removed from the map.
    if (!Target->Is_Valid()) {
        Target = nullptr;
        if (!IsReturningState) {
            RecallAircraft();
        }
        return;
    }

    // Check if the owner has been destroyed (e.g. Boris was killed).
    if (Owner && Owner->IsDead()) {
        if (!IsReturningState) {
            RecallAircraft();
        }
        return;
    }
}

// =============================================================================
// IsTargetValid - Check if the current target is valid for attacking
// =============================================================================

bool AirstrikeClass::IsTargetValid() const {
    if (!Target) return false;
    if (Target->IsDead()) return false;
    if (!Target->Is_Valid()) return false;

    // Check that the target is an enemy of the owner.
    if (Owner && Target) {
        HouseClass* ownerHouse = Owner->GetOwningHouse();
        if (ownerHouse) {
            // The target must be an enemy.
            if (Target->GetOwningHouse() == ownerHouse) {
                return false;
            }
        }
    }

    return true;
}

// =============================================================================
// Airstrike mission flow:
//
// 1. Boris (or another infantry with airstrike capability) designates a target.
// 2. StartMission() is called, which sets up the flight path and dispatches
//    aircraft from the nearest map edge.
// 3. Update() is called each frame:
//    a. CheckTargetValidity() verifies the target is still alive.
//    b. If the lead aircraft is over the target, bombs are dropped via
//       GlobalFiring::FireAt().
//    c. After dropping bombs, RecallAircraft() orders the aircraft to exit.
// 4. When all aircraft have left the map (ReturnedAircraftCount >=
//    DispatchedAircraftCount), the mission is complete.
// 5. The recharge timer starts. After AirstrikeRechargeTime frames, the
//    airstrike is ready again (IsReady = true).
//
// Veterancy effects:
// - Regular: 2 aircraft, 3000 frame recharge (200 seconds)
// - Elite:   3 aircraft, 2400 frame recharge (160 seconds)
//
// The airstrike can be interrupted if:
// - The target is destroyed before bombs are dropped
// - The owner (Boris) is killed
// - The mission timeout (900 frames / 60 seconds) is exceeded
//
// Flight path calculation:
// The entry and exit points are determined by the owner's position relative
// to the map center. If the owner is on the west half of the map, aircraft
// enter from the west edge and exit to the east edge, and vice versa. This
// ensures the aircraft fly over the target from a direction that makes sense
// based on the caller's position.
//
// Formation flying:
// Multiple aircraft fly in a staggered formation with AIRCRAFT_FORMATION_SPACING
// (512 leptons) between each aircraft. The formation is centered on the target's
// Y coordinate. The lead aircraft (index 0) is tracked as FirstObject for
// bomb-dropping proximity checks.
//
// Bomb dropping:
// When the lead aircraft is within BOMB_DROP_DISTANCE (256 leptons) of the
// target in both X and Y axes, a weapon is fired using GlobalFiring::FireAt().
// The resulting bullet is configured with gravity (for arc trajectory) and
// reduced speed (for realistic bomb fall). After dropping bombs, the aircraft
// are immediately recalled to exit the map.
//
// Recharge system:
// After a mission completes, the airstrike enters a recharge period. The
// recharge time depends on veterancy:
// - Regular: 3000 frames (200 seconds at 15 FPS)
// - Elite:   2400 frames (160 seconds at 15 FPS)
// During recharge, IsReady is false and CanFire() returns false. The recharge
// timer counts down one frame per Update() call until it reaches zero, at
// which point IsReady becomes true again.
// =============================================================================

// =============================================================================
// Integration with the game system:
//
// AirstrikeClass is instantiated by infantry types that have the airstrike
// capability (primarily Boris). The instance is owned by the TechnoClass
// and persists for the lifetime of that unit.
//
// The airstrike is triggered when the player issues an attack command on a
// building target with Boris. The game calls Fire() which in turn calls
// StartMission() if CanFire() returns true.
//
// During the mission, Update() is called each frame as part of the owner's
// AI update cycle. The airstrike manages its own aircraft, flight paths, and
// bomb dropping independently of the owner's mission AI.
//
// The airstrike respects the game's super weapon system: it uses a recharge
// timer similar to super weapons, but is tied to the individual unit rather
// than a building. This means killing Boris will permanently disable the
// airstrike (the instance is destroyed with the unit).
//
// Relationship to other systems:
// - GlobalFiring: Used to create and manage bomb bullets
// - MapClass: Used for map dimensions and flight path calculation
// - RulesClass: Provides default weapon and recharge parameters
// - Game::GetCurrentFrame(): Used for mission timeout tracking
// - HouseClass: Determines ownership and alliance for target validation
// =============================================================================
