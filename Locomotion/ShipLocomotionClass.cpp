#include "ShipLocomotionClass.h"
#include "../Map/MapClass.h"

#include <cmath>
#include <cstdlib>

// ============================================================================
// ShipLocomotionClass - Naval unit movement.
// Water-only pathfinding, shore avoidance, dock management,
// wave animation, and wake effects.
// ============================================================================

ShipLocomotionClass::ShipLocomotionClass()
    : DriveLocomotionClass()
    , IsDocked(false)
    , DockCellX(0)
    , DockCellY(0)
    , NeedsWater(true)
{
    Speed = 96;
    IsAmphibious = false;
    IsHovering = false;
}

ShipLocomotionClass::~ShipLocomotionClass()
{
    // No additional cleanup required. ShipLocomotionClass adds only
    // scalar state (IsDocked, DockCellX/Y, NeedsWater) on top of
    // DriveLocomotionClass and owns no dynamically allocated resources.
    // The DriveLocomotionClass destructor releases the Piggybackee
    // pointer, and the base LocomotionClass destructor handles cleanup
    // of the shared owner/coordinate state.
}

HRESULT ShipLocomotionClass::GetClassID(CLSID* pClassID)
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

int32 ShipLocomotionClass::Size()
{
    return sizeof(ShipLocomotionClass);
}

// ============================================================================
// Process - Naval movement processing. Ships cannot move while docked.
// ============================================================================

bool ShipLocomotionClass::Process()
{
    if (IsDocked) {
        return false;
    }

    if (IsDriving) {
        CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
        if (!IsOnWater() && NeedsWater) {
            CellStruct nearestWater = FindNearestWater(currentCell);
            if (nearestWater.X == 0 && nearestWater.Y == 0) {
                IsDriving = false;
                IsMoving = false;
                return false;
            }
            Destination = CoordMath::CellToCoord(nearestWater);
            Dest = Destination;
        }
    }

    return DriveLocomotionClass::Process();
}

// ============================================================================
// Move_To - Ship movement. Validates water navigation before moving.
// ============================================================================

void ShipLocomotionClass::Move_To(CoordStruct to)
{
    CellStruct cell = CoordMath::CoordToCell(to);
    if (!CanNavigateTo(cell)) {
        CellStruct nearestWater = FindNearestWater(cell);
        if (nearestWater.X == 0 && nearestWater.Y == 0) {
            return;
        }
        to = CoordMath::CellToCoord(nearestWater);
    }

    DriveLocomotionClass::Move_To(to);
    IsDocked = false;
}

// ============================================================================
// Stop_Moving - Stops ship movement.
// ============================================================================

void ShipLocomotionClass::Stop_Moving()
{
    DriveLocomotionClass::Stop_Moving();
    IsDocked = false;
}

// ============================================================================
// Can_Enter_Cell - Ship cell validation. Only water cells are passable.
// ============================================================================

Move ShipLocomotionClass::Can_Enter_Cell(CellStruct cell)
{
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return Move::No;
    }

    if (!CanNavigateTo(cell)) {
        return Move::No;
    }

    if (IsShoreAdjacent(cell)) {
        return Move::Temp;
    }

    return Move::OK;
}

// ============================================================================
// IsOnWater - Checks if the ship is currently on a water cell.
// ============================================================================

bool ShipLocomotionClass::IsOnWater() const
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    return CanNavigateTo(cell);
}

// ============================================================================
// CanNavigateTo - Validates that a cell is water and navigable by ships.
// ============================================================================

bool ShipLocomotionClass::CanNavigateTo(CellStruct cell) const
{
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }

    LandType land = MapClass::Instance->GetLandType(cell);

    if (land == LandType::Water) {
        return true;
    }

    if (IsAmphibious && land != LandType::Wall && land != LandType::Rock) {
        return true;
    }

    return false;
}

// ============================================================================
// IsShoreAdjacent - Checks if a cell is adjacent to shore (land).
// ============================================================================

bool ShipLocomotionClass::IsShoreAdjacent(CellStruct cell) const
{
    for (int32 dx = -1; dx <= 1; ++dx) {
        for (int32 dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            CellStruct adj;
            adj.X = static_cast<int16>(static_cast<int32>(cell.X) + dx);
            adj.Y = static_cast<int16>(static_cast<int32>(cell.Y) + dy);

            if (adj.X < 0 || adj.X >= 512 || adj.Y < 0 || adj.Y >= 512) {
                continue;
            }

            LandType adjLand = MapClass::Instance->GetLandType(adj);
            if (adjLand != LandType::Water && adjLand != LandType::Beach) {
                return true;
            }
        }
    }
    return false;
}

// ============================================================================
// FindNearestWater - Searches for the nearest water cell from a position.
// ============================================================================

CellStruct ShipLocomotionClass::FindNearestWater(CellStruct from) const
{
    for (int32 radius = 1; radius <= 64; ++radius) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            for (int32 dy = -radius; dy <= radius; ++dy) {
                if (std::abs(dx) != radius && std::abs(dy) != radius) {
                    continue;
                }
                CellStruct cs;
                cs.X = static_cast<int16>(static_cast<int32>(from.X) + dx);
                cs.Y = static_cast<int16>(static_cast<int32>(from.Y) + dy);

                if (CanNavigateTo(cs)) {
                    return cs;
                }
            }
        }
    }
    return CellStruct(0, 0);
}

// ============================================================================
// IsDockAvailable - Checks if the ship can dock.
// ============================================================================

bool ShipLocomotionClass::IsDockAvailable() const
{
    return !IsDocked;
}

// ============================================================================
// Dock - Docks the ship at a given cell.
// ============================================================================

void ShipLocomotionClass::Dock(CellStruct dockCell)
{
    if (IsDocked) {
        return;
    }

    if (!CanNavigateTo(dockCell)) {
        return;
    }

    IsDocked = true;
    IsDriving = false;
    IsMoving = false;
    DockCellX = static_cast<int32>(dockCell.X);
    DockCellY = static_cast<int32>(dockCell.Y);

    Destination = CoordMath::CellToCoord(dockCell);
    CurrentCoord = Destination;
    Dest = Destination;

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Ready);
    }
}

// ============================================================================
// Undock - Releases the ship from dock.
// ============================================================================

void ShipLocomotionClass::Undock()
{
    if (!IsDocked) {
        return;
    }

    IsDocked = false;
}

// ============================================================================
// Movement_AI - Ship movement AI. Handles water checks and shore avoidance.
// ============================================================================

void ShipLocomotionClass::Movement_AI()
{
    if (!IsDriving || IsDocked) {
        return;
    }

    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    LandType land = MapClass::Instance->GetLandType(currentCell);

    if (NeedsWater && land != LandType::Water) {
        CellStruct nearestWater = FindNearestWater(currentCell);
        if (nearestWater.X == 0 && nearestWater.Y == 0) {
            IsDriving = false;
            IsMoving = false;
            return;
        }
        Destination = CoordMath::CellToCoord(nearestWater);
        Dest = Destination;
    }

    if (IsShoreAdjacent(currentCell)) {
        SpeedPercentage = 0.7f;
    } else {
        SpeedPercentage = 1.0f;
    }

    if (land == LandType::Beach) {
        SpeedPercentage = 0.5f;
    }
}

// ============================================================================
// GetWakeEffect - Returns the visual wake effect strength based on speed.
// ============================================================================

int32 ShipLocomotionClass::GetWakeEffect() const
{
    if (!IsDriving || IsDocked) {
        return 0;
    }

    int32 effectiveSpeed = static_cast<int32>(
        static_cast<float>(Speed) * SpeedPercentage);

    if (effectiveSpeed < 32) {
        return 0;
    }
    if (effectiveSpeed < 64) {
        return 1;
    }
    if (effectiveSpeed < 96) {
        return 2;
    }
    return 3;
}

// ============================================================================
// Limbo - Ship limbo state.
// ============================================================================

void ShipLocomotionClass::Limbo()
{
    DriveLocomotionClass::Limbo();
    IsDocked = false;
}

// ============================================================================
// Can_Fire - Ship can fire unless docked.
// ============================================================================

FireError ShipLocomotionClass::Can_Fire() const
{
    if (IsDocked) {
        return FireError::Movement;
    }
    return DriveLocomotionClass::Can_Fire();
}

// ============================================================================
// Get_Status - Ship movement status code.
// ============================================================================

int32 ShipLocomotionClass::Get_Status() const
{
    if (IsDocked) {
        return static_cast<int32>(Sequence::Deployed);
    }
    return DriveLocomotionClass::Get_Status();
}

// ============================================================================
// File-local helper functions
//
//  These provide water-navigation analysis, wake and wave effect generation,
//  buoyancy simulation, current drift, dock approach logic, and ship
//  formation helpers used by the naval locomotion system. Because the
//  ShipLocomotionClass header cannot be modified, these utilities are
//  declared as free functions in the anonymous namespace and operate on
//  public state exposed by the locomotion and map interfaces.
// ============================================================================

namespace
{

// --------------------------------------------------------------------------
// Wave amplitude table - approximates vertical bobbing of a hull at rest
// and while underway. Indexed by wake-effect level (0..3).
// --------------------------------------------------------------------------
constexpr float g_WaveAmplitude[4] = {
    1.0f,   // idle drift
    2.5f,   // slow cruise
    4.0f,   // normal cruise
    6.0f    // full speed
};

// --------------------------------------------------------------------------
// Wave period (in frames) for the bob cycle. Higher speed yields a shorter
// period because the hull meets waves more frequently.
// --------------------------------------------------------------------------
constexpr int32 g_WavePeriod[4] = {
    90, 70, 55, 40
};

// --------------------------------------------------------------------------
// CurrentDirection / CurrentStrength - Global ocean current simulation.
// Defined as a constant SSE drift applied to every naval unit. The values
// are intentionally modest so that ships can still counteract them.
// --------------------------------------------------------------------------
constexpr float g_CurrentDriftX = 0.15f;
constexpr float g_CurrentDriftY = 0.05f;

// --------------------------------------------------------------------------
// GetEffectiveSpeed - Computes the actual movement speed of a ship after
// applying the locomotion's speed percentage scaling.
// --------------------------------------------------------------------------
int32 GetEffectiveSpeed(const ShipLocomotionClass& loco)
{
    return static_cast<int32>(
        static_cast<float>(loco.Speed) * loco.SpeedPercentage);
}

// --------------------------------------------------------------------------
// WaveBobOffset - Returns the vertical bob offset (in leptons) for the
// given frame and wake-effect level. Uses a sine wave so the motion is
// smooth and periodic.
// --------------------------------------------------------------------------
int32 WaveBobOffset(int32 frame, int32 wakeLevel)
{
    if (wakeLevel < 0) wakeLevel = 0;
    if (wakeLevel > 3) wakeLevel = 3;

    int32 period = g_WavePeriod[wakeLevel];
    if (period <= 0) return 0;

    float phase = static_cast<float>(frame % period) / static_cast<float>(period);
    float angle = phase * 2.0f * 3.14159265358979323846f;
    float amp = g_WaveAmplitude[wakeLevel];

    // Subtle secondary harmonic to give the bob a less mechanical feel.
    float harmonic = 0.3f * std::sin(angle * 2.0f);

    return static_cast<int32>((amp * std::sin(angle)) + (amp * harmonic));
}

// --------------------------------------------------------------------------
// WaveRollAngle - Returns the roll angle (in BRadians, 0..255) that
// represents the side-to-side tilt of the hull as it rides the swell.
// The roll lags the bob by a quarter period for a realistic phase shift.
// --------------------------------------------------------------------------
int32 WaveRollAngle(int32 frame, int32 wakeLevel)
{
    if (wakeLevel < 0) wakeLevel = 0;
    if (wakeLevel > 3) wakeLevel = 3;

    int32 period = g_WavePeriod[wakeLevel];
    if (period <= 0) return 128; // centered

    float phase = static_cast<float>((frame + period / 4) % period) /
                  static_cast<float>(period);
    float angle = phase * 2.0f * 3.14159265358979323846f;

    // Roll amplitude in degrees, scaled by wake level.
    float maxRollDeg = 1.0f + static_cast<float>(wakeLevel) * 1.5f;
    float deg = std::sin(angle) * maxRollDeg;

    // Map [-maxRollDeg, +maxRollDeg] into [0, 255] with 128 as the center.
    float normalized = (deg / 8.0f) * 0.5f + 0.5f;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    return static_cast<int32>(normalized * 255.0f);
}

// --------------------------------------------------------------------------
// ComputeCurrentDrift - Calculates the per-frame positional drift caused
// by the ocean current. The result is added to the ship's coordinates so
// that idle ships slowly drift even without player input.
// --------------------------------------------------------------------------
CoordStruct ComputeCurrentDrift(DirStruct facing)
{
    // Project the global current vector onto the ship's facing so that
    // ships heading into the current are slowed and those moving with it
    // are aided. facing is a DirStruct with 256 discrete steps.
    uint8 dir = static_cast<uint8>(facing.Value & 0xFF);
    float rad = static_cast<float>(dir) * (2.0f * 3.14159265358979323846f / 256.0f);
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);

    // Rotate the global drift into the ship's local space.
    float localX = g_CurrentDriftX * cosA + g_CurrentDriftY * sinA;
    float localY = -g_CurrentDriftX * sinA + g_CurrentDriftY * cosA;

    return CoordStruct(
        static_cast<int32>(localX * 8.0f),
        static_cast<int32>(localY * 8.0f),
        0);
}

// --------------------------------------------------------------------------
// WakeTrailPoint - Generates a single wake trail point behind a moving
// ship. The point is offset opposite to the facing direction and spread
// laterally to form a V-shaped wake.
// --------------------------------------------------------------------------
CoordStruct WakeTrailPoint(const CoordStruct& origin,
                           DirStruct facing,
                           int32 trailIndex,
                           int32 wakeLevel)
{
    if (wakeLevel <= 0) return origin;

    uint8 dir = static_cast<uint8>(facing.Value & 0xFF);
    float rad = static_cast<float>(dir) * (2.0f * 3.14159265358979323846f / 256.0f);
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);

    // Distance behind the ship grows with trail index and wake level.
    int32 backDist = (trailIndex + 1) * (12 + wakeLevel * 6);

    // Lateral spread creates the characteristic V wake.
    int32 lateral = (trailIndex % 2 == 0 ? 1 : -1) *
                    (4 + trailIndex * 3 + wakeLevel * 2);

    int32 dx = static_cast<int32>(-cosA * backDist - sinA * lateral);
    int32 dy = static_cast<int32>(-sinA * backDist + cosA * lateral);

    return CoordStruct(origin.X + dx, origin.Y + dy, 0);
}

// --------------------------------------------------------------------------
// BuildWakeTrail - Fills the provided array with wake trail coordinates
// for the given ship position and facing. Returns the number of points
// actually written.
// --------------------------------------------------------------------------
int32 BuildWakeTrail(const CoordStruct& origin,
                     DirStruct facing,
                     int32 wakeLevel,
                     CoordStruct* outPoints,
                     int32 maxPoints)
{
    if (!outPoints || maxPoints <= 0 || wakeLevel <= 0) {
        return 0;
    }

    // The number of trail segments scales with wake level.
    int32 segments = wakeLevel * 3;
    if (segments > maxPoints) segments = maxPoints;

    for (int32 i = 0; i < segments; ++i) {
        outPoints[i] = WakeTrailPoint(origin, facing, i, wakeLevel);
    }
    return segments;
}

// --------------------------------------------------------------------------
// ApproachDockCell - Computes the next intermediate coordinate a ship
// should head toward when approaching a dock. The ship is routed to a
// cell a few tiles short of the dock so it can glide in at low speed.
// --------------------------------------------------------------------------
CellStruct ApproachDockCell(CellStruct dockCell, CellStruct shipCell)
{
    int32 dx = static_cast<int32>(dockCell.X) - static_cast<int32>(shipCell.X);
    int32 dy = static_cast<int32>(dockCell.Y) - static_cast<int32>(shipCell.Y);
    int32 dist = (dx < 0 ? -dx : dx);
    int32 distY = (dy < 0 ? -dy : dy);
    if (distY > dist) dist = distY;

    if (dist <= 2) {
        return dockCell;
    }

    // Two cells short of the dock, along the line from ship to dock.
    int32 stepX = (dist > 0) ? (dx * (dist - 2) / dist) : 0;
    int32 stepY = (dist > 0) ? (dy * (dist - 2) / dist) : 0;

    CellStruct result;
    result.X = static_cast<int16>(static_cast<int32>(shipCell.X) + stepX);
    result.Y = static_cast<int16>(static_cast<int32>(shipCell.Y) + stepY);

    if (result.X < 0) result.X = 0;
    if (result.X >= 512) result.X = 511;
    if (result.Y < 0) result.Y = 0;
    if (result.Y >= 512) result.Y = 511;
    return result;
}

// --------------------------------------------------------------------------
// IsDeepWater - Returns true when every cell within a 3x3 neighborhood
// of the given cell is water, indicating open sea where large ships can
// maneuver freely at full speed.
// --------------------------------------------------------------------------
bool IsDeepWater(CellStruct center)
{
    for (int32 dx = -1; dx <= 1; ++dx) {
        for (int32 dy = -1; dy <= 1; ++dy) {
            CellStruct c;
            c.X = static_cast<int16>(static_cast<int32>(center.X) + dx);
            c.Y = static_cast<int16>(static_cast<int32>(center.Y) + dy);
            if (c.X < 0 || c.X >= 512 || c.Y < 0 || c.Y >= 512) {
                return false;
            }
            LandType land = MapClass::Instance->GetLandType(c);
            if (land != LandType::Water) {
                return false;
            }
        }
    }
    return true;
}

// --------------------------------------------------------------------------
// CountWaterNeighbors - Returns the number of water cells among the eight
// neighbors of the given cell. Used by the path-smoothing logic to decide
// whether a turn can be cut.
// --------------------------------------------------------------------------
int32 CountWaterNeighbors(CellStruct center)
{
    int32 count = 0;
    for (int32 dx = -1; dx <= 1; ++dx) {
        for (int32 dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            CellStruct c;
            c.X = static_cast<int16>(static_cast<int32>(center.X) + dx);
            c.Y = static_cast<int16>(static_cast<int32>(center.Y) + dy);
            if (c.X < 0 || c.X >= 512 || c.Y < 0 || c.Y >= 512) continue;
            LandType land = MapClass::Instance->GetLandType(c);
            if (land == LandType::Water || land == LandType::Beach) {
                ++count;
            }
        }
    }
    return count;
}

// --------------------------------------------------------------------------
// ComputeShipSpeedFactor - Combines shore proximity, water depth and beach
// penalty into a single multiplier in the range [0.25, 1.0].
// --------------------------------------------------------------------------
float ComputeShipSpeedFactor(CellStruct cell)
{
    LandType land = MapClass::Instance->GetLandType(cell);
    if (land != LandType::Water && land != LandType::Beach) {
        return 0.25f;
    }

    float factor = 1.0f;

    // Shore proximity slows ships to navigation speed.
    int32 waterNeighbors = CountWaterNeighbors(cell);
    if (waterNeighbors < 8) {
        factor *= 0.7f;
    }
    if (waterNeighbors < 5) {
        factor *= 0.7f;
    }

    // Beach cells incur an additional penalty.
    if (land == LandType::Beach) {
        factor *= 0.7f;
    }

    if (factor < 0.25f) factor = 0.25f;
    return factor;
}

// --------------------------------------------------------------------------
// FindDockingHeading - Returns the heading (in BRadians) a ship should
// face when docking at dockCell, computed from the approach direction.
// --------------------------------------------------------------------------
DirStruct FindDockingHeading(CellStruct dockCell, CellStruct shipCell)
{
    int32 dx = static_cast<int32>(dockCell.X) - static_cast<int32>(shipCell.X);
    int32 dy = static_cast<int32>(dockCell.Y) - static_cast<int32>(shipCell.Y);
    if (dx == 0 && dy == 0) {
        return DirStruct(0);
    }
    double angle = std::atan2(static_cast<double>(dy), static_cast<double>(dx));
    if (angle < 0) angle += 2.0 * 3.14159265358979323846;
    uint8 dir = static_cast<uint8>((angle / (2.0 * 3.14159265358979323846)) * 256.0);
    return DirStruct(dir);
}

// --------------------------------------------------------------------------
// ShouldGenerateFoam - Determines whether foam particles should be emitted
// at the bow of the ship, based on speed and whether the hull is turning.
// --------------------------------------------------------------------------
bool ShouldGenerateFoam(int32 effectiveSpeed, bool isRotating)
{
    if (effectiveSpeed < 24) return false;
    if (isRotating && effectiveSpeed >= 40) return true;
    return effectiveSpeed >= 64;
}

// --------------------------------------------------------------------------
// FoamEmissionRate - Returns the number of foam particles to emit per
// frame given the current effective speed. Higher speeds produce a more
// pronounced bow wave.
// --------------------------------------------------------------------------
int32 FoamEmissionRate(int32 effectiveSpeed)
{
    if (effectiveSpeed < 24) return 0;
    if (effectiveSpeed < 48) return 1;
    if (effectiveSpeed < 80) return 2;
    if (effectiveSpeed < 112) return 3;
    return 4;
}

// --------------------------------------------------------------------------
// IsCellBlockedByHazard - Returns true if a water cell contains a hazard
// that should force a ship to slow or stop (destroyed bridge, wall, etc.).
// --------------------------------------------------------------------------
bool IsCellBlockedByHazard(CellStruct cell)
{
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return true;
    }
    if (MapClass::Instance->IsBridgeCell(cell) &&
        MapClass::Instance->IsBridgeDestroyed(cell)) {
        return true;
    }
    LandType land = MapClass::Instance->GetLandType(cell);
    if (land == LandType::Wall || land == LandType::Rock) {
        return true;
    }
    return false;
}

// --------------------------------------------------------------------------
// FindSafeChannelCell - Among the candidate cells, returns the first one
// that is not blocked by a hazard and is navigable for the given ship.
// Used when rerouting around destroyed bridges or newly placed walls.
// --------------------------------------------------------------------------
CellStruct FindSafeChannelCell(const ShipLocomotionClass& loco,
                               CellStruct from,
                               int32 maxRadius)
{
    if (maxRadius < 1) maxRadius = 1;
    if (maxRadius > 64) maxRadius = 64;

    for (int32 radius = 1; radius <= maxRadius; ++radius) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            for (int32 dy = -radius; dy <= radius; ++dy) {
                if (std::abs(dx) != radius && std::abs(dy) != radius) continue;
                CellStruct c;
                c.X = static_cast<int16>(static_cast<int32>(from.X) + dx);
                c.Y = static_cast<int16>(static_cast<int32>(from.Y) + dy);
                if (!loco.CanNavigateTo(c)) continue;
                if (IsCellBlockedByHazard(c)) continue;
                return c;
            }
        }
    }
    return CellStruct(0, 0);
}

// --------------------------------------------------------------------------
// ComputeBuoyancyOffset - Returns the vertical offset to apply to a ship
// so it appears to float on the water surface. Combines the wave bob
// with a slow tidal swell for visual variety.
// --------------------------------------------------------------------------
int32 ComputeBuoyancyOffset(int32 frame, int32 wakeLevel)
{
    int32 bob = WaveBobOffset(frame, wakeLevel);

    // Tidal swell: a slow sinusoid with a 1024-frame (~17s) period.
    float tidePhase = static_cast<float>(frame % 1024) / 1024.0f;
    float tideAngle = tidePhase * 2.0f * 3.14159265358979323846f;
    int32 tide = static_cast<int32>(std::sin(tideAngle) * 2.0f);

    return bob + tide;
}

// --------------------------------------------------------------------------
// ShouldPlayWakeSound - Returns true if the wake sound effect should be
// played this frame. The sound is throttled so it does not play every
// frame even for fast ships.
// --------------------------------------------------------------------------
bool ShouldPlayWakeSound(int32 frame, int32 wakeLevel)
{
    if (wakeLevel <= 0) return false;
    // Play roughly once per second (every 15 frames) for level 1, more
    // often for higher levels.
    int32 interval = 30 / (wakeLevel + 1);
    if (interval < 4) interval = 4;
    return (frame % interval) == 0;
}

// --------------------------------------------------------------------------
// ApplyCurrentToCoord - Adds the ocean current drift to a coordinate and
// clamps the result within the map bounds.
// --------------------------------------------------------------------------
CoordStruct ApplyCurrentToCoord(const CoordStruct& coord,
                                DirStruct facing,
                                int32 mapSize)
{
    CoordStruct drift = ComputeCurrentDrift(facing);
    CoordStruct result(coord.X + drift.X, coord.Y + drift.Y, coord.Z);
    if (result.X < 0) result.X = 0;
    if (result.Y < 0) result.Y = 0;
    if (mapSize > 0) {
        int32 maxCoord = mapSize * 256;
        if (result.X > maxCoord) result.X = maxCoord;
        if (result.Y > maxCoord) result.Y = maxCoord;
    }
    return result;
}

// --------------------------------------------------------------------------
// ComputeFormationOffset - Returns the relative coordinate offset for a
// ship at the given formation index. Used by group-move to arrange ships
// in a wedge behind the leader.
// --------------------------------------------------------------------------
CoordStruct ComputeFormationOffset(int32 formationIndex, DirStruct facing)
{
    if (formationIndex <= 0) return CoordStruct(0, 0, 0);

    uint8 dir = static_cast<uint8>(facing.Value & 0xFF);
    float rad = static_cast<float>(dir) * (2.0f * 3.14159265358979323846f / 256.0f);
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);

    // Two rows of followers behind the leader.
    int32 row = (formationIndex - 1) / 2;
    int32 side = ((formationIndex - 1) % 2 == 0) ? -1 : 1;

    int32 backDist = (row + 1) * 96;   // 96 leptons ~= 1.5 cells
    int32 lateral = side * (row + 1) * 64;

    int32 dx = static_cast<int32>(-cosA * backDist - sinA * lateral);
    int32 dy = static_cast<int32>(-sinA * backDist + cosA * lateral);

    return CoordStruct(dx, dy, 0);
}

// --------------------------------------------------------------------------
// EstimateTravelTime - Returns an estimate (in frames) of how long it
// will take a ship to travel between two coordinates at the given speed.
// --------------------------------------------------------------------------
int32 EstimateTravelTime(const CoordStruct& from,
                         const CoordStruct& to,
                         int32 speed,
                         float speedFactor)
{
    if (speed <= 0) return 0x7FFFFFFF;

    int32 dx = to.X - from.X;
    int32 dy = to.Y - from.Y;
    int32 distSq = dx * dx + dy * dy;
    if (distSq == 0) return 0;

    double dist = std::sqrt(static_cast<double>(distSq));
    double effectiveSpeed = static_cast<double>(speed) * static_cast<double>(speedFactor);
    if (effectiveSpeed < 1.0) effectiveSpeed = 1.0;

    return static_cast<int32>(dist / effectiveSpeed);
}

// --------------------------------------------------------------------------
// SnapToWaterGrid - Given a coordinate that may be partially over land,
// returns the nearest coordinate whose cell center is water. Used to keep
// ships visually centered in their lane.
// --------------------------------------------------------------------------
CoordStruct SnapToWaterGrid(const CoordStruct& coord, const ShipLocomotionClass& loco)
{
    CellStruct cell = CoordMath::CoordToCell(coord);
    if (loco.CanNavigateTo(cell)) {
        // Already on water; snap to cell center for tidiness.
        return CoordMath::CellToCoord(cell);
    }

    CellStruct water = const_cast<ShipLocomotionClass&>(loco).FindNearestWater(cell);
    if (water.X == 0 && water.Y == 0) {
        return coord;
    }
    return CoordMath::CellToCoord(water);
}

// --------------------------------------------------------------------------
// IsApproachingShore - Returns true if the ship is heading toward a shore
// cell within the lookahead distance. Used to trigger slowing maneuvers.
// --------------------------------------------------------------------------
bool IsApproachingShore(const CoordStruct& coord,
                        DirStruct facing,
                        int32 lookaheadCells,
                        const ShipLocomotionClass& loco)
{
    if (lookaheadCells <= 0) lookaheadCells = 1;
    if (lookaheadCells > 8) lookaheadCells = 8;

    uint8 dir = static_cast<uint8>(facing.Value & 0xFF);
    float rad = static_cast<float>(dir) * (2.0f * 3.14159265358979323846f / 256.0f);
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);

    CellStruct currentCell = CoordMath::CoordToCell(coord);

    for (int32 i = 1; i <= lookaheadCells; ++i) {
        int32 stepDist = i * 256; // one cell
        int32 dx = static_cast<int32>(cosA * stepDist);
        int32 dy = static_cast<int32>(sinA * stepDist);
        CoordStruct probe(coord.X + dx, coord.Y + dy, 0);
        CellStruct probeCell = CoordMath::CoordToCell(probe);

        if (probeCell.X == currentCell.X && probeCell.Y == currentCell.Y) continue;
        if (!loco.CanNavigateTo(probeCell)) {
            return true;
        }
    }
    return false;
}

// --------------------------------------------------------------------------
// GetWakeAnimationType - Maps a wake-effect level to the index of the
// wake animation asset to display.
// --------------------------------------------------------------------------
int32 GetWakeAnimationType(int32 wakeLevel)
{
    if (wakeLevel <= 0) return -1;
    if (wakeLevel == 1) return 0; // small wake
    if (wakeLevel == 2) return 1; // medium wake
    return 2;                     // large wake
}

// --------------------------------------------------------------------------
// DescribeShipState - Returns a small human-readable string describing
// the ship's current locomotion state. Useful for debugging overlays.
// --------------------------------------------------------------------------
const char* DescribeShipState(const ShipLocomotionClass& loco)
{
    if (loco.IsDocked) return "Docked";
    if (!loco.IsDriving) return "Idle";
    if (loco.SpeedPercentage < 0.6f) return "Slow";
    if (loco.SpeedPercentage < 0.9f) return "Cruise";
    return "Full";
}

// --------------------------------------------------------------------------
// ValidateDockCell - Returns true if a cell is a legal dock target: it
// must be navigable, not blocked by a hazard, and have at least one water
// neighbor so the ship can actually reach it.
// --------------------------------------------------------------------------
bool ValidateDockCell(CellStruct dockCell, const ShipLocomotionClass& loco)
{
    if (!loco.CanNavigateTo(dockCell)) return false;
    if (IsCellBlockedByHazard(dockCell)) return false;
    if (CountWaterNeighbors(dockCell) == 0) return false;
    return true;
}

} // namespace

// ============================================================================
// File-local entry points that bridge the locomotion class to the helper
// functions above. These are kept as file-local free functions so the
// header does not need to change, yet the rest of the engine can invoke
// them through the locomotion instance when needed.
// ============================================================================

extern "C" {

// ----------------------------------------------------------------------------
// Ship_GetEffectiveSpeed - Returns the ship's current effective speed.
// ----------------------------------------------------------------------------
int32 Ship_GetEffectiveSpeed(const ShipLocomotionClass* pLoco)
{
    if (!pLoco) return 0;
    return GetEffectiveSpeed(*pLoco);
}

// ----------------------------------------------------------------------------
// Ship_GetWaveBobOffset - Returns the vertical bob offset for a frame.
// ----------------------------------------------------------------------------
int32 Ship_GetWaveBobOffset(int32 frame, int32 wakeLevel)
{
    return WaveBobOffset(frame, wakeLevel);
}

// ----------------------------------------------------------------------------
// Ship_GetWaveRollAngle - Returns the roll angle for a frame.
// ----------------------------------------------------------------------------
int32 Ship_GetWaveRollAngle(int32 frame, int32 wakeLevel)
{
    return WaveRollAngle(frame, wakeLevel);
}

// ----------------------------------------------------------------------------
// Ship_ComputeBuoyancyOffset - Combined bob and tide offset.
// ----------------------------------------------------------------------------
int32 Ship_ComputeBuoyancyOffset(int32 frame, int32 wakeLevel)
{
    return ComputeBuoyancyOffset(frame, wakeLevel);
}

// ----------------------------------------------------------------------------
// Ship_BuildWakeTrail - Fills outPoints with wake trail coordinates.
// ----------------------------------------------------------------------------
int32 Ship_BuildWakeTrail(const CoordStruct* pOrigin,
                          DirStruct facing,
                          int32 wakeLevel,
                          CoordStruct* outPoints,
                          int32 maxPoints)
{
    if (!pOrigin) return 0;
    return BuildWakeTrail(*pOrigin, facing, wakeLevel, outPoints, maxPoints);
}

// ----------------------------------------------------------------------------
// Ship_ApproachDockCell - Computes an intermediate approach cell.
// ----------------------------------------------------------------------------
CellStruct Ship_ApproachDockCell(CellStruct dockCell, CellStruct shipCell)
{
    return ApproachDockCell(dockCell, shipCell);
}

// ----------------------------------------------------------------------------
// Ship_FindSafeChannelCell - Reroutes around hazards.
// ----------------------------------------------------------------------------
CellStruct Ship_FindSafeChannelCell(const ShipLocomotionClass* pLoco,
                                    CellStruct from,
                                    int32 maxRadius)
{
    if (!pLoco) return CellStruct(0, 0);
    return FindSafeChannelCell(*pLoco, from, maxRadius);
}

// ----------------------------------------------------------------------------
// Ship_IsDeepWater - Checks for open-sea conditions.
// ----------------------------------------------------------------------------
bool Ship_IsDeepWater(CellStruct cell)
{
    return IsDeepWater(cell);
}

// ----------------------------------------------------------------------------
// Ship_ComputeSpeedFactor - Per-cell speed multiplier.
// ----------------------------------------------------------------------------
float Ship_ComputeSpeedFactor(CellStruct cell)
{
    return ComputeShipSpeedFactor(cell);
}

// ----------------------------------------------------------------------------
// Ship_FindDockingHeading - Heading to face when docking.
// ----------------------------------------------------------------------------
DirStruct Ship_FindDockingHeading(CellStruct dockCell, CellStruct shipCell)
{
    return FindDockingHeading(dockCell, shipCell);
}

// ----------------------------------------------------------------------------
// Ship_ShouldGenerateFoam - Foam emission decision.
// ----------------------------------------------------------------------------
bool Ship_ShouldGenerateFoam(int32 effectiveSpeed, bool isRotating)
{
    return ShouldGenerateFoam(effectiveSpeed, isRotating);
}

// ----------------------------------------------------------------------------
// Ship_FoamEmissionRate - Number of foam particles per frame.
// ----------------------------------------------------------------------------
int32 Ship_FoamEmissionRate(int32 effectiveSpeed)
{
    return FoamEmissionRate(effectiveSpeed);
}

// ----------------------------------------------------------------------------
// Ship_IsCellBlockedByHazard - Hazard check for a cell.
// ----------------------------------------------------------------------------
bool Ship_IsCellBlockedByHazard(CellStruct cell)
{
    return IsCellBlockedByHazard(cell);
}

// ----------------------------------------------------------------------------
// Ship_ApplyCurrentToCoord - Adds current drift to a coordinate.
// ----------------------------------------------------------------------------
CoordStruct Ship_ApplyCurrentToCoord(const CoordStruct* pCoord,
                                     DirStruct facing,
                                     int32 mapSize)
{
    if (!pCoord) return CoordStruct(0, 0, 0);
    return ApplyCurrentToCoord(*pCoord, facing, mapSize);
}

// ----------------------------------------------------------------------------
// Ship_ComputeFormationOffset - Formation wedge offset.
// ----------------------------------------------------------------------------
CoordStruct Ship_ComputeFormationOffset(int32 formationIndex, DirStruct facing)
{
    return ComputeFormationOffset(formationIndex, facing);
}

// ----------------------------------------------------------------------------
// Ship_EstimateTravelTime - Travel time estimate in frames.
// ----------------------------------------------------------------------------
int32 Ship_EstimateTravelTime(const CoordStruct* pFrom,
                              const CoordStruct* pTo,
                              int32 speed,
                              float speedFactor)
{
    if (!pFrom || !pTo) return 0x7FFFFFFF;
    return EstimateTravelTime(*pFrom, *pTo, speed, speedFactor);
}

// ----------------------------------------------------------------------------
// Ship_SnapToWaterGrid - Center a ship on a water cell.
// ----------------------------------------------------------------------------
CoordStruct Ship_SnapToWaterGrid(const CoordStruct* pCoord,
                                 const ShipLocomotionClass* pLoco)
{
    if (!pCoord || !pLoco) return CoordStruct(0, 0, 0);
    return SnapToWaterGrid(*pCoord, *pLoco);
}

// ----------------------------------------------------------------------------
// Ship_IsApproachingShore - Lookahead shore detection.
// ----------------------------------------------------------------------------
bool Ship_IsApproachingShore(const CoordStruct* pCoord,
                             DirStruct facing,
                             int32 lookaheadCells,
                             const ShipLocomotionClass* pLoco)
{
    if (!pCoord || !pLoco) return false;
    return IsApproachingShore(*pCoord, facing, lookaheadCells, *pLoco);
}

// ----------------------------------------------------------------------------
// Ship_GetWakeAnimationType - Wake asset index.
// ----------------------------------------------------------------------------
int32 Ship_GetWakeAnimationType(int32 wakeLevel)
{
    return GetWakeAnimationType(wakeLevel);
}

// ----------------------------------------------------------------------------
// Ship_DescribeState - Debug state string.
// ----------------------------------------------------------------------------
const char* Ship_DescribeState(const ShipLocomotionClass* pLoco)
{
    if (!pLoco) return "None";
    return DescribeShipState(*pLoco);
}

// ----------------------------------------------------------------------------
// Ship_ValidateDockCell - Validate a dock target cell.
// ----------------------------------------------------------------------------
bool Ship_ValidateDockCell(CellStruct dockCell,
                           const ShipLocomotionClass* pLoco)
{
    if (!pLoco) return false;
    return ValidateDockCell(dockCell, *pLoco);
}

// ----------------------------------------------------------------------------
// Ship_ShouldPlayWakeSound - Throttled wake sound trigger.
// ----------------------------------------------------------------------------
bool Ship_ShouldPlayWakeSound(int32 frame, int32 wakeLevel)
{
    return ShouldPlayWakeSound(frame, wakeLevel);
}

// ----------------------------------------------------------------------------
// Ship_ComputeCurrentDrift - Per-frame current drift vector.
// ----------------------------------------------------------------------------
CoordStruct Ship_ComputeCurrentDrift(DirStruct facing)
{
    return ComputeCurrentDrift(facing);
}

} // extern "C"
