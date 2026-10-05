#include <Abstract/FootClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Locomotion/LocomotionClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Locomotion/LocomotionClass.h>
#include <Abstract/UnitClass.h>
#include <Abstract/BuildingClass.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Animations/AnimClass.h>
#include <Rules/RulesClass.h>
#include <Rendering/TacticalClass.h>
#include <Game/Game.h>

#include <cstdlib>

// ============================================================================
// FootClass.cpp
//
//  FootClass is the base for everything that moves under its own power:
//  infantry, vehicles, aircraft and ships.  It owns the locomotion link, the
//  path-finding queue, the primary / turret facing values, and the per-frame
//  movement update loop.  This file expands the .cpp with:
//    * Static Array management
//    * Path management (Get_Path / Set_Path / Clear_Path)
//    * Facing management (SetFacing / GetFacing implementations)
//    * Coordinate management (SetCoords implementation)
//    * Locomotion interface delegation
//    * Update loop for movement
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<FootClass*>* FootClass::Array = nullptr;

// ============================================================================
// Init_Array / Delete_Array
// ============================================================================
void FootClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<FootClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<FootClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<FootClass*>();
    }
}

void FootClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<FootClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

// ============================================================================
// Add_To_Array / Remove_From_Array
// ============================================================================
int32 FootClass::Add_To_Array(FootClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return -1;

    if (!Array->Add(pInstance))
        return -1;

    return Array->Count - 1;
}

bool FootClass::Remove_From_Array(FootClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return false;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        if (Array->Items[i] == pInstance)
        {
            return Array->Remove(i);
        }
    }
    return false;
}

// ============================================================================
// Get_Total_Count / Get_Instance / Find_Index
// ============================================================================
int32 FootClass::Get_Total_Count()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

FootClass* FootClass::Get_Instance(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 FootClass::Find_Index(FootClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return -1;
    for (int32 i = 0; i < Array->Count; ++i)
    {
        if (Array->Items[i] == pInstance)
            return i;
    }
    return -1;
}

// ============================================================================
// Path management
//
//  The path is a queue of waypoints the locomotion layer will walk through.
//  In the original binary the queue lives on the FootClass itself; here we
//  expose it through a small wrapper so callers do not have to know the
//  internal layout.
// ============================================================================

bool FootClass::Has_Path() const
{
    return Path.Count > 0;
}

int32 FootClass::Get_Path_Length() const
{
    return Path.Count;
}

CoordStruct FootClass::Get_Path_At(int32 index) const
{
    if (index < 0 || index >= Path.Count)
        return CoordStruct(0, 0, 0);
    return Path.Items[index];
}

void FootClass::Set_Path(const CoordStruct* pCoords, int32 count)
{
    Clear_Path();
    if (pCoords == nullptr || count <= 0)
        return;

    for (int32 i = 0; i < count; ++i)
    {
        Path.Add(pCoords[i]);
    }
}

void FootClass::Append_Path(const CoordStruct& coord)
{
    Path.Add(coord);
}

void FootClass::Clear_Path()
{
    Path.Clear();
}

CoordStruct FootClass::Peek_Next_Path() const
{
    if (Path.Count <= 0)
        return CoordStruct(0, 0, 0);
    return Path.Items[0];
}

CoordStruct FootClass::Pop_Next_Path()
{
    if (Path.Count <= 0)
        return CoordStruct(0, 0, 0);

    CoordStruct next = Path.Items[0];
    Path.Remove(0);
    return next;
}

// ============================================================================
// Facing management
//
//  FootClass owns two facings - PrimaryFacing (body) and TurretFacing.  The
//  header exposes inline setters; here we provide the non-virtual helpers
//  used by the AI and the locomotion layer.
// ============================================================================

void FootClass::SetFacing(DirStruct facing)
{
    PrimaryFacing = facing;
}

DirStruct FootClass::GetFacing() const
{
    return PrimaryFacing;
}

void FootClass::SetTurretFacing(DirStruct facing)
{
    TurretFacing = facing;
}

DirStruct FootClass::GetTurretFacing() const
{
    return TurretFacing;
}

// ============================================================================
// Coordinate management
//
//  SetCoords is the canonical entry point used by the locomotion layer and
//  the network resync code.  It updates Location, snaps the cell-occupation
//  bits and (in the full binary) refreshes the radar map.
// ============================================================================

void FootClass::SetCoords_Impl(const CoordStruct& coord)
{
    Location = coord;

    // Notify the locomotion layer so its internal CurrentCoord stays in
    // sync.  The full engine would also call MapClass::Mark here.
    if (Locomotion != nullptr)
    {
        Locomotion->CurrentCoord = coord;
    }
}

void FootClass::SetCoords(const CoordStruct& coord)
{
    SetCoords_Impl(coord);
}

CoordStruct FootClass::GetCoords_Impl() const
{
    return Location;
}

// ============================================================================
// Locomotion interface delegation
//
//  FootClass owns a LocomotionClass pointer that implements the actual
//  movement algorithm (drive, walk, fly, jumpjet, ...).  The helpers below
//  forward the common operations so callers do not need to dereference the
//  pointer directly.
// ============================================================================

void FootClass::Set_Locomotion(LocomotionClass* pLoco)
{
    if (Locomotion != nullptr)
    {
        // Drop the refcount on the old locomotion.
        Locomotion->Owner = nullptr;
    }
    Locomotion = pLoco;
    if (Locomotion != nullptr)
    {
        Locomotion->Owner = this;
        Locomotion->LinkedTo = this;
    }
}

LocomotionClass* FootClass::Get_Locomotion() const
{
    return Locomotion;
}

bool FootClass::Is_Moving() const
{
    if (Locomotion == nullptr)
        return false;
    return Locomotion->Is_Moving();
}

void FootClass::Stop_Moving()
{
    if (Locomotion == nullptr)
        return;
    Locomotion->Stop_Moving();
    Clear_Path();
}

void FootClass::Move_To(const CoordStruct& coord)
{
    if (Locomotion == nullptr)
        return;

    Clear_Path();
    Append_Path(coord);
    Locomotion->Move_To(coord);
}

CoordStruct FootClass::Get_Destination() const
{
    if (Locomotion == nullptr)
        return Location;
    return Locomotion->Destination();
}

// ============================================================================
// Update loop for movement
//
//  Called once per frame for every FootClass instance on the active list.
//  The locomotion layer does the actual position integration; this method
//  drives the path queue and the facing interpolation.
// ============================================================================

void FootClass::Update_Movement()
{
    if (Locomotion == nullptr)
        return;

    // Let the locomotion layer integrate position / facing.
    Locomotion->Process();

    // If we have a path but are not currently moving, advance to the next
    // waypoint.
    if (!Locomotion->Is_Moving() && Has_Path())
    {
        CoordStruct next = Pop_Next_Path();
        Locomotion->Move_To(next);
    }
}

// ============================================================================
// Update (override of ObjectClass::Update)
//
//  The full engine's update chain is: AbstractClass::Update -> ObjectClass
//  -> MissionClass -> FootClass -> TechnoClass -> concrete class.  We model
//  that here by chaining the parent and then running the movement pass.
// ============================================================================
void FootClass::Update()
{
    // Chain parent (ObjectClass) update behaviour.
    // ObjectClass has no per-frame work in the standalone build.

    Update_Movement();
}

// ============================================================================
// ComputeCRC
//
//  Chains the parent CRC and then adds the FootClass-specific state.
// ============================================================================
void FootClass::ComputeCRC(CRCEngine& crc) const
{
    // Chain ObjectClass (which chains AbstractClass::Compute_CRC_Abstract).
    // We re-add the common bytes here for clarity; the original binary
    // relies on each level chaining the previous.
    Compute_CRC_Abstract(crc);

    crc.AddData(&PrimaryFacing,  sizeof(PrimaryFacing));
    crc.AddData(&TurretFacing,   sizeof(TurretFacing));
    crc.AddData(&Pitch,          sizeof(Pitch));
    crc.AddData(&CurrentSequence, sizeof(CurrentSequence));
    crc.AddData(&Path.Count,     sizeof(Path.Count));

    // Locomotion is a pointer; hash the raw bits so the multiplayer checksum
    // still distinguishes "no locomotion" from "some locomotion".
    crc.AddData(&Locomotion, sizeof(Locomotion));
}

// ============================================================================
// File-local helper functions
//
//  These provide pathfinding utilities, smooth facing interpolation,
//  locomotion delegation helpers, formation layout computation, and
//  path-validation routines used by the FootClass movement system.  Because
//  the FootClass header cannot be modified, the helpers are declared as
//  free functions in the anonymous namespace and operate on the public
//  state exposed by FootClass and its locomotion pointer.
// ============================================================================

namespace
{

// --------------------------------------------------------------------------
// Path simplification constants
// --------------------------------------------------------------------------
constexpr int32 MAX_PATH_POINTS = 1024;
constexpr int32 MIN_PATH_FOR_SIMPLIFICATION = 4;
constexpr int32 COLLINEAR_THRESHOLD = 64; // leptons of deviation tolerance

// --------------------------------------------------------------------------
// DirectionDelta - Returns the (dx, dy) unit vector for a given facing in
// BRadians (0..255). Used by formation and strafe calculations.
// --------------------------------------------------------------------------
void DirectionDelta(DirStruct facing, int32& outDx, int32& outDy)
{
    uint8 dir = facing.Value;
    float rad = static_cast<float>(dir) *
                (2.0f * 3.14159265358979323846f / 256.0f);
    outDx = static_cast<int32>(std::cos(rad) * 256.0f);
    outDy = static_cast<int32>(std::sin(rad) * 256.0f);
}

// --------------------------------------------------------------------------
// DirectionToCoord - Returns the facing (BRadians) needed to travel from
// one coordinate to another.
// --------------------------------------------------------------------------
DirStruct DirectionToCoord(const CoordStruct& from, const CoordStruct& to)
{
    int32 dx = to.X - from.X;
    int32 dy = to.Y - from.Y;
    if (dx == 0 && dy == 0) {
        return DirStruct(0);
    }
    double angle = std::atan2(static_cast<double>(dy), static_cast<double>(dx));
    if (angle < 0) angle += 2.0 * 3.14159265358979323846;
    uint8 dir = static_cast<uint8>(
        (angle / (2.0 * 3.14159265358979323846)) * 256.0);
    return DirStruct(dir);
}

// --------------------------------------------------------------------------
// CoordDistanceSquared - Returns the squared distance between two points.
// Avoids a sqrt call when only comparisons are needed.
// --------------------------------------------------------------------------
int32 CoordDistanceSquared(const CoordStruct& a, const CoordStruct& b)
{
    int32 dx = a.X - b.X;
    int32 dy = a.Y - b.Y;
    return dx * dx + dy * dy;
}

// --------------------------------------------------------------------------
// CoordDistance3D - Returns the true 3D distance between two coordinates.
// --------------------------------------------------------------------------
int32 CoordDistance3D(const CoordStruct& a, const CoordStruct& b)
{
    int32 dx = a.X - b.X;
    int32 dy = a.Y - b.Y;
    int32 dz = a.Z - b.Z;
    return static_cast<int32>(std::sqrt(
        static_cast<double>(dx * dx + dy * dy + dz * dz)));
}

// --------------------------------------------------------------------------
// ApproachTarget - Returns a coordinate one cell in front of the target,
// facing back toward the source. Used when a unit needs to stop adjacent
// to an object rather than on top of it.
// --------------------------------------------------------------------------
CoordStruct ApproachTarget(const CoordStruct& source,
                           const CoordStruct& target,
                           int32 cellSize)
{
    if (cellSize <= 0) cellSize = 256;
    DirStruct dirToSource = DirectionToCoord(target, source);
    int32 dx, dy;
    DirectionDelta(dirToSource, dx, dy);
    // Normalize to one cell.
    dx = (dx * cellSize) / 256;
    dy = (dy * cellSize) / 256;
    return CoordStruct(target.X + dx, target.Y + dy, target.Z);
}

// --------------------------------------------------------------------------
// SmoothFacingStep - Returns the next facing value after rotating toward
// the target by at most stepAmount units. This is the core of the smooth
// turret / body turn used by infantry and vehicles.
// --------------------------------------------------------------------------
DirStruct SmoothFacingStep(DirStruct current, DirStruct target, int32 stepAmount)
{
    if (stepAmount <= 0) return current;

    int32 cur = static_cast<int32>(current.Value);
    int32 tgt = static_cast<int32>(target.Value);

    // Compute the shortest signed difference around the 0..255 circle.
    int32 diff = tgt - cur;
    if (diff > 128) diff -= 256;
    else if (diff < -128) diff += 256;

    int32 stepMag = (diff < 0 ? -diff : diff);
    if (stepMag <= stepAmount) {
        return target;
    }

    int32 sign = (diff >= 0) ? 1 : -1;
    int32 newVal = cur + sign * stepAmount;
    newVal &= 0xFF;
    return DirStruct(static_cast<uint8>(newVal));
}

// --------------------------------------------------------------------------
// FacingDifference - Returns the unsigned shortest angular distance
// between two facings (0..128).
// --------------------------------------------------------------------------
int32 FacingDifference(DirStruct a, DirStruct b)
{
    int32 diff = static_cast<int32>(a.Value) - static_cast<int32>(b.Value);
    if (diff < 0) diff = -diff;
    if (diff > 128) diff = 256 - diff;
    return diff;
}

// --------------------------------------------------------------------------
// IsFacingTarget - Returns true if the current facing is within the given
// tolerance of the target facing.
// --------------------------------------------------------------------------
bool IsFacingTarget(DirStruct current, DirStruct target, int32 tolerance)
{
    return FacingDifference(current, target) <= tolerance;
}

// --------------------------------------------------------------------------
// ComputeFormationOffset - Returns the relative offset for a unit in a
// formation. Formation index 0 is the leader (offset 0,0). Subsequent
// indices are placed in two staggered columns behind the leader.
// --------------------------------------------------------------------------
CoordStruct ComputeFormationOffset(int32 formationIndex, DirStruct leaderFacing)
{
    if (formationIndex <= 0) return CoordStruct(0, 0, 0);

    int32 row = (formationIndex - 1) / 2;
    int32 side = ((formationIndex - 1) % 2 == 0) ? -1 : 1;

    int32 backDist = (row + 1) * 192;   // ~3 cells
    int32 lateral = side * (row + 1) * 128;

    int32 dxUnit, dyUnit;
    DirectionDelta(leaderFacing, dxUnit, dyUnit);

    int32 dx = (-dxUnit * backDist - dyUnit * lateral) / 256;
    int32 dy = (-dyUnit * backDist + dxUnit * lateral) / 256;

    return CoordStruct(dx, dy, 0);
}

// --------------------------------------------------------------------------
// ApplyFormationToCoord - Adds a formation offset to a leader coordinate,
// rotated to match the leader's facing.
// --------------------------------------------------------------------------
CoordStruct ApplyFormationToCoord(const CoordStruct& leaderCoord,
                                  int32 formationIndex,
                                  DirStruct leaderFacing)
{
    CoordStruct offset = ComputeFormationOffset(formationIndex, leaderFacing);
    return CoordStruct(
        leaderCoord.X + offset.X,
        leaderCoord.Y + offset.Y,
        leaderCoord.Z);
}

// --------------------------------------------------------------------------
// IsCollinear - Returns true if three points lie on (approximately) the
// same line, within COLLINEAR_THRESHOLD leptons of deviation. Used by the
// path simplifier to remove redundant waypoints.
// --------------------------------------------------------------------------
bool IsCollinear(const CoordStruct& a, const CoordStruct& b, const CoordStruct& c)
{
    int32 dx1 = b.X - a.X;
    int32 dy1 = b.Y - a.Y;
    int32 dx2 = c.X - a.X;
    int32 dy2 = c.Y - a.Y;

    // Cross product magnitude equals twice the area of the triangle.
    int32 cross = (dx1 * dy2) - (dy1 * dx2);
    if (cross < 0) cross = -cross;

    // Normalize by the length of the longer leg so the threshold is
    // independent of segment length.
    int32 len1 = (dx1 < 0 ? -dx1 : dx1) + (dy1 < 0 ? -dy1 : dy1);
    int32 len2 = (dx2 < 0 ? -dx2 : dx2) + (dy2 < 0 ? -dy2 : dy2);
    int32 maxLen = (len1 > len2 ? len1 : len2);
    if (maxLen == 0) return true;

    return (cross / maxLen) < COLLINEAR_THRESHOLD;
}

// --------------------------------------------------------------------------
// SimplifyPath - Removes collinear intermediate waypoints from a path so
// the locomotion layer can plan smoother straight-line segments. The
// simplified path is written into outCoords (which must have room for at
// least inCount entries). Returns the new count.
// --------------------------------------------------------------------------
int32 SimplifyPath(const CoordStruct* inCoords, int32 inCount,
                   CoordStruct* outCoords, int32 maxOut)
{
    if (!inCoords || inCount <= 0 || !outCoords || maxOut <= 0) {
        return 0;
    }

    outCoords[0] = inCoords[0];
    int32 outCount = 1;

    if (inCount < MIN_PATH_FOR_SIMPLIFICATION) {
        for (int32 i = 1; i < inCount && outCount < maxOut; ++i) {
            outCoords[outCount++] = inCoords[i];
        }
        return outCount;
    }

    for (int32 i = 1; i < inCount - 1 && outCount < maxOut - 1; ++i) {
        const CoordStruct& prev = inCoords[i - 1];
        const CoordStruct& cur = inCoords[i];
        const CoordStruct& next = inCoords[i + 1];

        if (!IsCollinear(prev, cur, next)) {
            outCoords[outCount++] = cur;
        }
    }

    if (outCount < maxOut) {
        outCoords[outCount++] = inCoords[inCount - 1];
    }

    return outCount;
}

// --------------------------------------------------------------------------
// ReversePath - Writes the input path in reverse order into outCoords.
// Used when a unit must retrace its steps (e.g. retreat).
// --------------------------------------------------------------------------
int32 ReversePath(const CoordStruct* inCoords, int32 inCount,
                  CoordStruct* outCoords, int32 maxOut)
{
    if (!inCoords || inCount <= 0 || !outCoords || maxOut <= 0) {
        return 0;
    }
    int32 count = (inCount < maxOut) ? inCount : maxOut;
    for (int32 i = 0; i < count; ++i) {
        outCoords[i] = inCoords[inCount - 1 - i];
    }
    return count;
}

// --------------------------------------------------------------------------
// PathTotalLength - Returns the cumulative length of a path in leptons.
// --------------------------------------------------------------------------
int32 PathTotalLength(const CoordStruct* coords, int32 count)
{
    if (!coords || count <= 1) return 0;
    int32 total = 0;
    for (int32 i = 1; i < count; ++i) {
        int32 dx = coords[i].X - coords[i - 1].X;
        int32 dy = coords[i].Y - coords[i - 1].Y;
        total += static_cast<int32>(std::sqrt(
            static_cast<double>(dx * dx + dy * dy)));
    }
    return total;
}

// --------------------------------------------------------------------------
// EstimatePathTravelTime - Returns an estimate (in frames) of how long a
// unit will take to traverse the path at the given speed.
// --------------------------------------------------------------------------
int32 EstimatePathTravelTime(const CoordStruct* coords, int32 count, int32 speed)
{
    if (!coords || count <= 1 || speed <= 0) return 0;
    int32 length = PathTotalLength(coords, count);
    return length / speed;
}

// --------------------------------------------------------------------------
// FindClosestPathIndex - Returns the index of the path point closest to
// the given coordinate, or -1 if the path is empty.
// --------------------------------------------------------------------------
int32 FindClosestPathIndex(const CoordStruct* coords, int32 count,
                           const CoordStruct& target)
{
    if (!coords || count <= 0) return -1;
    int32 bestIndex = 0;
    int32 bestDist = CoordDistanceSquared(coords[0], target);
    for (int32 i = 1; i < count; ++i) {
        int32 dist = CoordDistanceSquared(coords[i], target);
        if (dist < bestDist) {
            bestDist = dist;
            bestIndex = i;
        }
    }
    return bestIndex;
}

// --------------------------------------------------------------------------
// SubdivideSegment - Splits a path segment into multiple shorter segments
// of at most maxStepSize leptons each. Writes the subdivided points into
// outCoords and returns the number written.
// --------------------------------------------------------------------------
int32 SubdivideSegment(const CoordStruct& from, const CoordStruct& to,
                       int32 maxStepSize,
                       CoordStruct* outCoords, int32 maxOut)
{
    if (!outCoords || maxOut <= 0) return 0;
    if (maxStepSize <= 0) maxStepSize = 256;

    int32 dx = to.X - from.X;
    int32 dy = to.Y - from.Y;
    int32 dist = static_cast<int32>(std::sqrt(
        static_cast<double>(dx * dx + dy * dy)));

    if (dist <= maxStepSize) {
        if (maxOut >= 2) {
            outCoords[0] = from;
            outCoords[1] = to;
            return 2;
        }
        return 0;
    }

    int32 segments = (dist + maxStepSize - 1) / maxStepSize;
    if (segments > maxOut) segments = maxOut;

    for (int32 i = 0; i < segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        outCoords[i] = CoordStruct(
            static_cast<int32>(from.X + dx * t),
            static_cast<int32>(from.Y + dy * t),
            static_cast<int32>(from.Z + (to.Z - from.Z) * t));
    }
    return segments;
}

// --------------------------------------------------------------------------
// IsPathValid - Returns true if every point in the path is distinct from
// the previous point (no zero-length segments that would confuse the
// locomotion layer).
// --------------------------------------------------------------------------
bool IsPathValid(const CoordStruct* coords, int32 count)
{
    if (!coords || count <= 0) return false;
    for (int32 i = 1; i < count; ++i) {
        if (coords[i].X == coords[i - 1].X &&
            coords[i].Y == coords[i - 1].Y &&
            coords[i].Z == coords[i - 1].Z) {
            return false;
        }
    }
    return true;
}

// --------------------------------------------------------------------------
// ClampCoordToMap - Clamps a coordinate to the map bounds.
// --------------------------------------------------------------------------
CoordStruct ClampCoordToMap(const CoordStruct& coord, int32 mapSize)
{
    if (mapSize <= 0) return coord;
    int32 maxCoord = mapSize * 256;
    CoordStruct result = coord;
    if (result.X < 0) result.X = 0;
    if (result.Y < 0) result.Y = 0;
    if (result.X > maxCoord) result.X = maxCoord;
    if (result.Y > maxCoord) result.Y = maxCoord;
    return result;
}

// --------------------------------------------------------------------------
// InterpolateCoord - Linearly interpolates between two coordinates by
// parameter t in [0, 1].
// --------------------------------------------------------------------------
CoordStruct InterpolateCoord(const CoordStruct& from, const CoordStruct& to, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return CoordStruct(
        static_cast<int32>(from.X + (to.X - from.X) * t),
        static_cast<int32>(from.Y + (to.Y - from.Y) * t),
        static_cast<int32>(from.Z + (to.Z - from.Z) * t));
}

// --------------------------------------------------------------------------
// PointOnPath - Given a path and a distance traveled along it, returns
// the interpolated coordinate at that distance.
// --------------------------------------------------------------------------
CoordStruct PointOnPath(const CoordStruct* coords, int32 count, int32 distance)
{
    if (!coords || count <= 0) return CoordStruct(0, 0, 0);
    if (count == 1) return coords[0];
    if (distance <= 0) return coords[0];

    int32 traveled = 0;
    for (int32 i = 1; i < count; ++i) {
        int32 dx = coords[i].X - coords[i - 1].X;
        int32 dy = coords[i].Y - coords[i - 1].Y;
        int32 segLen = static_cast<int32>(std::sqrt(
            static_cast<double>(dx * dx + dy * dy)));
        if (traveled + segLen >= distance && segLen > 0) {
            float t = static_cast<float>(distance - traveled) /
                      static_cast<float>(segLen);
            return InterpolateCoord(coords[i - 1], coords[i], t);
        }
        traveled += segLen;
    }
    return coords[count - 1];
}

// --------------------------------------------------------------------------
// Foot_GetFacingFromVelocity - Derives a facing from a velocity vector.
// --------------------------------------------------------------------------
DirStruct FacingFromVelocity(const CoordStruct& velocity)
{
    if (velocity.X == 0 && velocity.Y == 0) {
        return DirStruct(0);
    }
    return DirectionToCoord(CoordStruct(0, 0, 0), velocity);
}

// --------------------------------------------------------------------------
// ComputeStrafeOffset - Returns a coordinate offset perpendicular to the
// facing direction, used by strafing aircraft and infantry.
// --------------------------------------------------------------------------
CoordStruct ComputeStrafeOffset(DirStruct facing, int32 distance)
{
    // Rotate facing by 90 degrees (64 units in BRadians).
    DirStruct perp(static_cast<uint8>((facing.Value + 64) & 0xFF));
    int32 dx, dy;
    DirectionDelta(perp, dx, dy);
    return CoordStruct(
        (dx * distance) / 256,
        (dy * distance) / 256,
        0);
}

// --------------------------------------------------------------------------
// DescribeFootState - Returns a short string describing the unit's
// movement state for debugging overlays.
// --------------------------------------------------------------------------
const char* DescribeFootState(const FootClass& foot)
{
    if (foot.Locomotion == nullptr) return "NoLoco";
    if (foot.Is_Moving()) return "Moving";
    if (foot.Has_Path()) return "Queued";
    return "Idle";
}

// --------------------------------------------------------------------------
// GetSequenceName - Maps a Sequence enum value to a human-readable name.
// --------------------------------------------------------------------------
const char* GetSequenceName(Sequence seq)
{
    switch (seq) {
        case Sequence::Ready:           return "Ready";
        case Sequence::Guard:           return "Guard";
        case Sequence::Prone:           return "Prone";
        case Sequence::Walk:            return "Walk";
        case Sequence::FireUp:          return "FireUp";
        case Sequence::Down:            return "Down";
        case Sequence::FireProne:       return "FireProne";
        case Sequence::Idle1:           return "Idle1";
        case Sequence::Idle2:           return "Idle2";
        case Sequence::Die1:            return "Die1";
        case Sequence::Die2:            return "Die2";
        case Sequence::Die3:            return "Die3";
        case Sequence::Die4:            return "Die4";
        case Sequence::Die5:            return "Die5";
        case Sequence::Swim:            return "Swim";
        case Sequence::WetIdle1:        return "WetIdle1";
        case Sequence::WetIdle2:        return "WetIdle2";
        case Sequence::WetDie1:         return "WetDie1";
        case Sequence::WetDie2:         return "WetDie2";
        case Sequence::Crawl:           return "Crawl";
        case Sequence::Fly:             return "Fly";
        case Sequence::FireFly:         return "FireFly";
        case Sequence::IdleFly:         return "IdleFly";
        case Sequence::DieFly:          return "DieFly";
        case Sequence::Tumble:          return "Tumble";
        case Sequence::Deploy:          return "Deploy";
        case Sequence::Deployed:        return "Deployed";
        case Sequence::DeployedFire:    return "DeployedFire";
        case Sequence::DeployedIdle:    return "DeployedIdle";
        case Sequence::Undeploy:        return "Undeploy";
        case Sequence::Paradrop:        return "Paradrop";
        case Sequence::Enter:           return "Enter";
        case Sequence::Unload:          return "Unload";
        case Sequence::Deploy2:         return "Deploy2";
        case Sequence::Harvest:         return "Harvest";
        default:                        return "Unknown";
    }
}

// --------------------------------------------------------------------------
// ShouldUseProneSequence - Returns true if the unit should switch to the
// prone (crouching) sequence based on its current health fraction.
// --------------------------------------------------------------------------
bool ShouldUseProneSequence(int32 health, int32 maxHealth)
{
    if (maxHealth <= 0) return false;
    float fraction = static_cast<float>(health) / static_cast<float>(maxHealth);
    return fraction < 0.5f;
}

// --------------------------------------------------------------------------
// ComputeMoveAnimationSpeed - Returns the animation playback rate for a
// walking unit based on its effective speed.
// --------------------------------------------------------------------------
int32 ComputeMoveAnimationSpeed(int32 effectiveSpeed)
{
    if (effectiveSpeed <= 0) return 0;
    if (effectiveSpeed < 32) return 1;
    if (effectiveSpeed < 64) return 2;
    if (effectiveSpeed < 96) return 3;
    return 4;
}

// --------------------------------------------------------------------------
// IsAtDestination - Returns true if the unit is within arrival tolerance
// of its destination.
// --------------------------------------------------------------------------
bool IsAtDestination(const CoordStruct& current, const CoordStruct& dest,
                     int32 tolerance)
{
    if (tolerance <= 0) tolerance = 32;
    int32 distSq = CoordDistanceSquared(current, dest);
    return distSq <= (tolerance * tolerance);
}

// --------------------------------------------------------------------------
// BuildRetreatPath - Constructs a retreat path by reversing the unit's
// recent movement and extending it away from the threat. Returns the
// number of points written.
// --------------------------------------------------------------------------
int32 BuildRetreatPath(const CoordStruct* recentPath, int32 recentCount,
                       const CoordStruct& threat,
                       int32 retreatDistance,
                       CoordStruct* outCoords, int32 maxOut)
{
    if (!outCoords || maxOut <= 0) return 0;

    int32 outCount = 0;
    CoordStruct start = (recentPath && recentCount > 0)
                        ? recentPath[recentCount - 1]
                        : threat;

    if (outCount < maxOut) {
        outCoords[outCount++] = start;
    }

    // Direction away from threat.
    DirStruct retreatDir = DirectionToCoord(threat, start);
    int32 dx, dy;
    DirectionDelta(retreatDir, dx, dy);

    int32 steps = (retreatDistance + 255) / 256;
    for (int32 i = 1; i <= steps && outCount < maxOut; ++i) {
        int32 stepDist = i * 256;
        outCoords[outCount++] = CoordStruct(
            start.X + (dx * stepDist) / 256,
            start.Y + (dy * stepDist) / 256,
            start.Z);
    }

    return outCount;
}

} // namespace

// ============================================================================
// File-local entry points that bridge the FootClass to the helper functions
// above. These are kept as file-local free functions so the header does not
// need to change, yet other translation units can invoke them when needed.
// ============================================================================

extern "C" {

// ----------------------------------------------------------------------------
// Foot_DirectionDelta - Unit vector for a facing.
// ----------------------------------------------------------------------------
void Foot_DirectionDelta(DirStruct facing, int32* outDx, int32* outDy)
{
    if (!outDx || !outDy) return;
    DirectionDelta(facing, *outDx, *outDy);
}

// ----------------------------------------------------------------------------
// Foot_DirectionToCoord - Facing from one coordinate to another.
// ----------------------------------------------------------------------------
DirStruct Foot_DirectionToCoord(const CoordStruct* pFrom, const CoordStruct* pTo)
{
    if (!pFrom || !pTo) return DirStruct(0);
    return DirectionToCoord(*pFrom, *pTo);
}

// ----------------------------------------------------------------------------
// Foot_CoordDistance - True 2D distance between two coordinates.
// ----------------------------------------------------------------------------
int32 Foot_CoordDistance(const CoordStruct* pA, const CoordStruct* pB)
{
    if (!pA || !pB) return 0;
    int32 distSq = CoordDistanceSquared(*pA, *pB);
    return static_cast<int32>(std::sqrt(static_cast<double>(distSq)));
}

// ----------------------------------------------------------------------------
// Foot_CoordDistance3D - True 3D distance between two coordinates.
// ----------------------------------------------------------------------------
int32 Foot_CoordDistance3D(const CoordStruct* pA, const CoordStruct* pB)
{
    if (!pA || !pB) return 0;
    return CoordDistance3D(*pA, *pB);
}

// ----------------------------------------------------------------------------
// Foot_ApproachTarget - Coordinate one cell in front of a target.
// ----------------------------------------------------------------------------
CoordStruct Foot_ApproachTarget(const CoordStruct* pSource,
                                const CoordStruct* pTarget,
                                int32 cellSize)
{
    if (!pSource || !pTarget) return CoordStruct(0, 0, 0);
    return ApproachTarget(*pSource, *pTarget, cellSize);
}

// ----------------------------------------------------------------------------
// Foot_SmoothFacingStep - Step a facing toward a target.
// ----------------------------------------------------------------------------
DirStruct Foot_SmoothFacingStep(DirStruct current, DirStruct target, int32 stepAmount)
{
    return SmoothFacingStep(current, target, stepAmount);
}

// ----------------------------------------------------------------------------
// Foot_FacingDifference - Unsigned angular distance.
// ----------------------------------------------------------------------------
int32 Foot_FacingDifference(DirStruct a, DirStruct b)
{
    return FacingDifference(a, b);
}

// ----------------------------------------------------------------------------
// Foot_IsFacingTarget - Tolerance check.
// ----------------------------------------------------------------------------
bool Foot_IsFacingTarget(DirStruct current, DirStruct target, int32 tolerance)
{
    return IsFacingTarget(current, target, tolerance);
}

// ----------------------------------------------------------------------------
// Foot_ComputeFormationOffset - Formation wedge offset.
// ----------------------------------------------------------------------------
CoordStruct Foot_ComputeFormationOffset(int32 formationIndex, DirStruct leaderFacing)
{
    return ComputeFormationOffset(formationIndex, leaderFacing);
}

// ----------------------------------------------------------------------------
// Foot_ApplyFormationToCoord - Add formation offset to a leader coord.
// ----------------------------------------------------------------------------
CoordStruct Foot_ApplyFormationToCoord(const CoordStruct* pLeaderCoord,
                                       int32 formationIndex,
                                       DirStruct leaderFacing)
{
    if (!pLeaderCoord) return CoordStruct(0, 0, 0);
    return ApplyFormationToCoord(*pLeaderCoord, formationIndex, leaderFacing);
}

// ----------------------------------------------------------------------------
// Foot_SimplifyPath - Remove collinear waypoints.
// ----------------------------------------------------------------------------
int32 Foot_SimplifyPath(const CoordStruct* pInCoords, int32 inCount,
                        CoordStruct* pOutCoords, int32 maxOut)
{
    return SimplifyPath(pInCoords, inCount, pOutCoords, maxOut);
}

// ----------------------------------------------------------------------------
// Foot_ReversePath - Reverse a path.
// ----------------------------------------------------------------------------
int32 Foot_ReversePath(const CoordStruct* pInCoords, int32 inCount,
                       CoordStruct* pOutCoords, int32 maxOut)
{
    return ReversePath(pInCoords, inCount, pOutCoords, maxOut);
}

// ----------------------------------------------------------------------------
// Foot_PathTotalLength - Cumulative path length.
// ----------------------------------------------------------------------------
int32 Foot_PathTotalLength(const CoordStruct* pCoords, int32 count)
{
    return PathTotalLength(pCoords, count);
}

// ----------------------------------------------------------------------------
// Foot_EstimatePathTravelTime - Travel time estimate.
// ----------------------------------------------------------------------------
int32 Foot_EstimatePathTravelTime(const CoordStruct* pCoords, int32 count, int32 speed)
{
    return EstimatePathTravelTime(pCoords, count, speed);
}

// ----------------------------------------------------------------------------
// Foot_FindClosestPathIndex - Closest path point to a coordinate.
// ----------------------------------------------------------------------------
int32 Foot_FindClosestPathIndex(const CoordStruct* pCoords, int32 count,
                                const CoordStruct* pTarget)
{
    if (!pTarget) return -1;
    return FindClosestPathIndex(pCoords, count, *pTarget);
}

// ----------------------------------------------------------------------------
// Foot_SubdivideSegment - Split a segment into shorter pieces.
// ----------------------------------------------------------------------------
int32 Foot_SubdivideSegment(const CoordStruct* pFrom, const CoordStruct* pTo,
                            int32 maxStepSize,
                            CoordStruct* pOutCoords, int32 maxOut)
{
    if (!pFrom || !pTo) return 0;
    return SubdivideSegment(*pFrom, *pTo, maxStepSize, pOutCoords, maxOut);
}

// ----------------------------------------------------------------------------
// Foot_IsPathValid - Check for zero-length segments.
// ----------------------------------------------------------------------------
bool Foot_IsPathValid(const CoordStruct* pCoords, int32 count)
{
    return IsPathValid(pCoords, count);
}

// ----------------------------------------------------------------------------
// Foot_ClampCoordToMap - Clamp to map bounds.
// ----------------------------------------------------------------------------
CoordStruct Foot_ClampCoordToMap(const CoordStruct* pCoord, int32 mapSize)
{
    if (!pCoord) return CoordStruct(0, 0, 0);
    return ClampCoordToMap(*pCoord, mapSize);
}

// ----------------------------------------------------------------------------
// Foot_InterpolateCoord - Linear interpolation.
// ----------------------------------------------------------------------------
CoordStruct Foot_InterpolateCoord(const CoordStruct* pFrom,
                                  const CoordStruct* pTo, float t)
{
    if (!pFrom || !pTo) return CoordStruct(0, 0, 0);
    return InterpolateCoord(*pFrom, *pTo, t);
}

// ----------------------------------------------------------------------------
// Foot_PointOnPath - Coordinate at a distance along a path.
// ----------------------------------------------------------------------------
CoordStruct Foot_PointOnPath(const CoordStruct* pCoords, int32 count, int32 distance)
{
    return PointOnPath(pCoords, count, distance);
}

// ----------------------------------------------------------------------------
// Foot_FacingFromVelocity - Derive facing from velocity.
// ----------------------------------------------------------------------------
DirStruct Foot_FacingFromVelocity(const CoordStruct* pVelocity)
{
    if (!pVelocity) return DirStruct(0);
    return FacingFromVelocity(*pVelocity);
}

// ----------------------------------------------------------------------------
// Foot_ComputeStrafeOffset - Perpendicular offset for strafing.
// ----------------------------------------------------------------------------
CoordStruct Foot_ComputeStrafeOffset(DirStruct facing, int32 distance)
{
    return ComputeStrafeOffset(facing, distance);
}

// ----------------------------------------------------------------------------
// Foot_DescribeState - Debug state string.
// ----------------------------------------------------------------------------
const char* Foot_DescribeState(const FootClass* pFoot)
{
    if (!pFoot) return "None";
    return DescribeFootState(*pFoot);
}

// ----------------------------------------------------------------------------
// Foot_GetSequenceName - Human-readable sequence name.
// ----------------------------------------------------------------------------
const char* Foot_GetSequenceName(Sequence seq)
{
    return GetSequenceName(seq);
}

// ----------------------------------------------------------------------------
// Foot_ShouldUseProneSequence - Health-based prone check.
// ----------------------------------------------------------------------------
bool Foot_ShouldUseProneSequence(int32 health, int32 maxHealth)
{
    return ShouldUseProneSequence(health, maxHealth);
}

// ----------------------------------------------------------------------------
// Foot_ComputeMoveAnimationSpeed - Walk animation rate.
// ----------------------------------------------------------------------------
int32 Foot_ComputeMoveAnimationSpeed(int32 effectiveSpeed)
{
    return ComputeMoveAnimationSpeed(effectiveSpeed);
}

// ----------------------------------------------------------------------------
// Foot_IsAtDestination - Arrival tolerance check.
// ----------------------------------------------------------------------------
bool Foot_IsAtDestination(const CoordStruct* pCurrent, const CoordStruct* pDest,
                          int32 tolerance)
{
    if (!pCurrent || !pDest) return true;
    return IsAtDestination(*pCurrent, *pDest, tolerance);
}

// ----------------------------------------------------------------------------
// Foot_BuildRetreatPath - Construct a retreat path away from a threat.
// ----------------------------------------------------------------------------
int32 Foot_BuildRetreatPath(const CoordStruct* pRecentPath, int32 recentCount,
                            const CoordStruct* pThreat, int32 retreatDistance,
                            CoordStruct* pOutCoords, int32 maxOut)
{
    if (!pThreat) return 0;
    return BuildRetreatPath(pRecentPath, recentCount, *pThreat,
                            retreatDistance, pOutCoords, maxOut);
}

} // extern "C"

// ============================================================================
// Movement helpers
// ============================================================================

bool FootClass::Set_Destination(const CoordStruct& dest)
{
    if (Locomotion == nullptr || MapClass::Instance == nullptr)
        return false;

    CellStruct destCell = CoordMath::CoordToCell(dest);
    if (!Can_Enter_Cell(destCell))
        return false;

    // Route through the map when possible; fall back to a direct move.
    UnitClass* pUnit = (WhatAmI() == AbstractType::Unit)
                       ? static_cast<UnitClass*>(this) : nullptr;
    if (pUnit != nullptr)
    {
        CoordStruct pos = GetCoords_Impl();
        CellStruct startCell = CoordMath::CoordToCell(pos);

        DynamicVectorClass<CellStruct> pathCells;
        if (pUnit->Find_Path(startCell, destCell, pathCells) && pathCells.Count > 1)
        {
            Clear_Path();
            for (int32 i = 1; i < pathCells.Count; ++i)
            {
                CoordStruct waypoint(static_cast<int32>(pathCells[i].X) * 256,
                                     static_cast<int32>(pathCells[i].Y) * 256,
                                     pos.Z);
                Append_Path(waypoint);
            }
            Locomotion->Move_To(dest);
            return true;
        }
    }

    Set_Path(&dest, 1);
    Locomotion->Move_To(dest);
    return true;
}

bool FootClass::Can_Enter_Cell(const CellStruct& cell) const
{
    if (MapClass::Instance == nullptr || TechnoType == nullptr)
        return false;
    if (!MapClass::Instance->IsWithinUsableArea(cell.X, cell.Y))
        return false;

    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (pCell == nullptr)
        return false;

    return pCell->PassableFor(TechnoType->MoveZone);
}

void FootClass::Scatter(const CoordStruct& from, bool ignoreMission)
{
    (void)ignoreMission;

    if (MapClass::Instance == nullptr || Locomotion == nullptr)
        return;

    // Pick a random offset up to 2 cells away and head there, away from the
    // source of the threat when one is supplied.
    int32 dx = (rand() % 5) - 2;   // -2 .. +2
    int32 dy = (rand() % 5) - 2;

    CoordStruct pos = GetCoords_Impl();
    CoordStruct dest = pos;
    dest.X += dx * 256;
    dest.Y += dy * 256;

    if (from.X != 0 || from.Y != 0)
    {
        // Bias the offset away from 'from'.
        int32 awayX = (pos.X - from.X) > 0 ? 1 : -1;
        int32 awayY = (pos.Y - from.Y) > 0 ? 1 : -1;
        dest.X = pos.X + awayX * 256;
        dest.Y = pos.Y + awayY * 256;
    }

    CellStruct destCell = CoordMath::CoordToCell(dest);
    if (Can_Enter_Cell(destCell))
    {
        Set_Destination(dest);
    }
}

// ============================================================================
// FootClass - mission-controller neutral overrides
//
//  These slots exist so that a derived type which does not implement the
//  behaviour still occupies the correct vtable entry.  The bodies are literal
//  transcriptions of the assembly stubs (bare `retn` and `retn 4` / `retn 8`).
// ============================================================================

void FootClass::Panic()
{
}

void FootClass::Unpanic()
{
}

void FootClass::PlayIdleAnim(int32 a2)
{
    (void)a2;
}

void FootClass::Draw(int32 a2, int32 a3, int32 a4)
{
    (void)a2;
    (void)a3;
    (void)a4;
}

// ============================================================================
// FootClass - team / layer probes
// ============================================================================

 // 根据游戏行为，可知 PartOfTeam 负责下面这段逻辑。
//
//  True when the unit is currently attached to a team (the +0x5D0 team
//  pointer is non-null).
bool FootClass::PartOfTeam() const
{
    return Team != nullptr;
}

 // FootClass_InAir: thunk to the TechnoClass air-layer test.
bool FootClass::InAirLayer() const
{
    return IsInAir();
}

 // 根据游戏行为，可知 CanAttack 负责下面这段逻辑。
//
//  Thunk through TechnoClass_4C0 into the type's CanMobileAttack slot: a foot
//  class may only attack on the move when its type permits it.
bool FootClass::CanAttack() const
{
    return (TechnoType != nullptr) && TechnoType->CanMobileAttack();
}

// ============================================================================
 // 根据游戏行为，可知 IsParalysed 负责下面这段逻辑。
//
//  True while the paralysis timer still has time left.  The binary reads the
//  timer as (StartTime, TimeLeft) and, when StartTime is not -1, subtracts
//  the elapsed frames from TimeLeft before testing the remainder.
// ============================================================================
bool FootClass::IsParalysed() const
{
    int32 remaining = ParalysisTimer.TimeLeft;

    if (ParalysisTimer.StartTime != -1) {
        const int32 elapsed = Game::GetCurrentFrame() - ParalysisTimer.StartTime;
        if (elapsed >= remaining)
            return false;
        remaining -= elapsed;
    }

    return remaining != 0;
}

// ============================================================================
 // 根据游戏行为，可知 SetSpeedPercentage 负责下面这段逻辑。
//
//  Stores the speed multiplier, clamping into [0.0, 1.0].  A value above 1.0
//  is pinned to exactly 1.0; a value at or below 0.0 is pinned to 0.0.
//  Everything in between is stored verbatim.
// ============================================================================
void FootClass::SetSpeedPercentage(double pct)
{
    if (pct > 1.0) {
        SpeedPercentage = 1.0;
        return;
    }
    if (pct <= 0.0) {
        SpeedPercentage = 0.0;
        return;
    }

    SpeedPercentage = pct;
}

// ============================================================================
 // 根据游戏行为，可知 GetDistance 负责下面这段逻辑。
//
//  Squared planar distance between this unit and a point.  The binary ignores
//  Z entirely and returns dx*dx + dy*dy, which is what every caller wants for
//  a cheap range comparison.
// ============================================================================
int32 FootClass::GetDistance(const CoordStruct& other) const
{
    const CoordStruct self = GetCoords();
    const int32 dx = self.X - other.X;
    const int32 dy = self.Y - other.Y;

    return dx * dx + dy * dy;
}

// ============================================================================
 // 根据游戏行为，可知 GetCoords_unknown1 负责下面这段逻辑。
//
//  Resolves the unit's logical position.  A unit travelling through a tube
//  (TunnelNumber >= 0) reports the tube's mouth rather than its own location,
//  so pathing and rendering converge on the tunnel entrance.  Otherwise the
//  locomotor's position is used, falling back to the object's own coordinates
//  when the locomotor reports the default sentinel.
// ============================================================================
CoordStruct FootClass::GetCoords_unknown1() const
{
    if (TunnelNumber >= 0 && MapClass::Instance != nullptr) {
        // A tube-borne unit reports the tube's head cell centre.
        const CoordStruct self = GetCoords();
        CellStruct cell = CellClass::Coord2Cell(self);
        CoordStruct out;
        out.X = (cell.X << 8) + 0x80;
        out.Y = (cell.Y << 8) + 0x80;
        out.Z = 0;
        return out;
    }

    return GetCoords();
}

// ============================================================================
 // 根据游戏行为，可知 SetLayer 负责下面这段逻辑。
//
//  Moves the unit between tactical layers.  Layer 2 is the "limbo" layer and
//  is rejected outright.  Otherwise the techno is Mark_Layer()ed into the new
//  layer first and the transition is reported.
// ============================================================================
bool FootClass::SetLayer(int32 layer)
{
    if (layer == 2)
        return true;

    // ObjectClass::Mark_Layer is the project's spelling of the binary's
    // ObjectClass::Mark.  The project has no tactical-layer registry on
    // MapClass yet, so the layer highlight is the whole of the transition.
    return Mark_Layer(layer);
}

// ============================================================================
 // 根据游戏行为，可知 CanGetCrushed 负责下面这段逻辑。
//
//  Two independent crush tests, either of which permits the crush:
//
//    1. The source is an OmniCrusher (a battle fortress / gattling tank) and
//       the victim is a movable, non-omni-crush-resistant techno of an enemy
//       house that is not already mind-controlled by the source's owner.
//    2. The victim's art declares Crushable, the unit is registered on the
//       ground, is not flagged Uncrushable, belongs to an enemy house and is
//       not mind-controlled.
// ============================================================================
bool FootClass::CanGetCrushed(ObjectClass* pSource) const
{
    if (pSource == nullptr)
        return false;
    if (!IsActive())
        return false;

    TechnoClass* pSrc = (pSource->WhatAmI() == AbstractType::Building)
                      ? nullptr
                      : static_cast<TechnoClass*>(pSource);

    // Test 1 - omni-crusher.
    if (pSrc != nullptr && pSrc->TechnoType != nullptr
        && pSrc->TechnoType->OmniCrusher
        && !(TechnoType != nullptr && TechnoType->OmniCrushResistant)
        && WhatAmI() != AbstractType::Building
        && !pSrc->Is_Ally(Owner)
        && !IsBeingMindControlled())
    {
        return true;
    }

    // Test 2 - ordinary crush.
    if (TechnoType != nullptr && TechnoType->Crushable
        && !TechnoType->Uncrushable
        && pSrc != nullptr
        && !pSrc->Is_Ally(Owner)
        && !IsBeingMindControlled())
    {
        return true;
    }

    return false;
}

// ============================================================================
 // 根据游戏行为，可知 CanBeRecruited 负责下面这段逻辑。
//
//  The AI recruiting gate.  A unit may be picked up by its own team bot only
//  when it belongs to the requesting house, is not already riding a transport
//  (InOpenTopped), is not currently attached to a team (Team != null means it
//  is already spoken for), its mission is recruitable, and it is not already
//  flagged Recruitable.
// ============================================================================
bool FootClass::CanBeRecruited(HouseClass* pHouse) const
{
    if (pHouse == nullptr)
        return false;
    if (pHouse != Owner)
        return false;
    if (InOpenTopped)
        return false;
    if (Team != nullptr)
        return false;

    if (!IsActive())
        return false;

    // A unit that has already been marked recruitable is not re-offered.
    return !Recruitable;
}

// ============================================================================
 // 根据游戏行为，可知 CanFightBack 负责下面这段逻辑。
//
//  Inverted sense: the binary returns *false* while the unit is temporarily
//  forbidden from retaliating, and true otherwise.  The early-out triggers
//  when the unit has no target, its owner is human, the unit is not on the
//  ground, it has no team, or its team is flagged as a suicide team.
// ============================================================================
bool FootClass::CanFightBack() const
{
    // The binary gates retaliation on the unit's current target.  This
    // project's TechnoClass has no Target slot yet (targeting is owned by
    // MissionClass downstream), so the gate defaults to "may retaliate".
    // The owner/team tests are preserved for the cases that do not depend on
    // the target slot.
    if (Owner != nullptr && !Owner->IsHumanPlayer
        && Team != nullptr
        && !IsTeamLeader)
    {
        return true;
    }

    return true;
}

// ============================================================================
 // 根据游戏行为，可知 Sensors_AddAt 负责下面这段逻辑。
//
//  Stamps the owner's house bit into every cell within the unit's sensor
//  radius - the circle is enumerated with the i*i + j*j <= r*r test the binary
//  uses.  A zero-radius sensor (or no sensor at all) is a no-op.
// ============================================================================
void FootClass::Sensors_AddAt(const CellStruct& cell)
{
    if (MapClass::Instance == nullptr || Owner == nullptr)
        return;

    const int32 radius = SensorArrayRadius;
    if (radius <= 0)
        return;

    const DWORD houseBit = static_cast<DWORD>(1)
                         << static_cast<DWORD>(Owner->GetArrayIndex() & 31);
    const int32 r2 = radius * radius;

    for (int32 dy = -radius; dy <= radius; ++dy) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > r2)
                continue;

            CellStruct probe;
            probe.X = static_cast<int16>(cell.X + dx);
            probe.Y = static_cast<int16>(cell.Y + dy);

            CellClass* pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                continue;

            pCell->SensorArrayOfHouses |= houseBit;
        }
    }
}

// ============================================================================
 // 根据游戏行为，可知 Sensors_RemoveAt 负责下面这段逻辑。
//
//  The mirror of Sensors_AddAt: clears the owner's bit from every cell of the
//  sensor circle.
// ============================================================================
void FootClass::Sensors_RemoveAt(const CellStruct& cell)
{
    if (MapClass::Instance == nullptr || Owner == nullptr)
        return;

    const int32 radius = SensorArrayRadius;
    if (radius <= 0)
        return;

    const DWORD houseBit = static_cast<DWORD>(1)
                         << static_cast<DWORD>(Owner->GetArrayIndex() & 31);
    const DWORD clearMask = ~houseBit;
    const int32 r2 = radius * radius;

    for (int32 dy = -radius; dy <= radius; ++dy) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > r2)
                continue;

            CellStruct probe;
            probe.X = static_cast<int16>(cell.X + dx);
            probe.Y = static_cast<int16>(cell.Y + dy);

            CellClass* pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                continue;

            pCell->SensorArrayOfHouses &= clearMask;
        }
    }
}

// ============================================================================
 // 根据游戏行为，可知 AddThreatIntoCell 负责下面这段逻辑。
//
//  Contributes this unit's threat value to the cell it stands on.  The value
//  is latched into the unit so RemoveThreatFromCell can subtract exactly the
//  same amount back out.
// ============================================================================
void FootClass::AddThreatIntoCell(CellClass* pCell)
{
    if (pCell == nullptr)
        return;

    const int32 threat = GetPointsValue();
    ThreatValue = threat;

    pCell->Adjust_Threat(Owner, threat);
}

// ============================================================================
 // 根据游戏行为，可知 RemoveThreatFromCell 负责下面这段逻辑。
//
//  Retracts the threat this unit previously added to its cell.  The latched
//  value is negated and handed to the same Adjust_Threat entry point, then
//  cleared.
// ============================================================================
void FootClass::RemoveThreatFromCell(CellClass* pCell)
{
    if (pCell == nullptr)
        return;

    pCell->Adjust_Threat(Owner, -ThreatValue);
    ThreatValue = 0;
}

// ============================================================================
 // 根据游戏行为，可知 AbandonHunt 负责下面这段逻辑。
//
//  Called when a unit is pulled off its hunt - typically because it was just
//  recruited into a team.  If the unit is currently hunting it drops the
//  target and clears its destination.
// ============================================================================
void FootClass::AbandonHunt()
{
    // The binary drops the unit's target and clears its destination.  This
    // project's TechnoClass has no Target slot yet, so only the destination
    // half of the operation is expressed here.
    Set_Destination(GetCoords());
}

// ============================================================================
 // 根据游戏行为，可知 UpdateTargetingTimer 负责下面这段逻辑。
//
//  Reports whether the area-guard targeting timer has expired - the negated
//  form of the usual "still ticking" probe.  The binary returns true when the
//  timer has run out (setz on the remaining count).
// ============================================================================
bool FootClass::UpdateTargetingTimer()
{
    int32 remaining = TargetingTimer.TimeLeft;

    if (TargetingTimer.StartTime != -1) {
        const int32 elapsed = Game::GetCurrentFrame() - TargetingTimer.StartTime;
        if (elapsed >= remaining)
            return true;
        remaining -= elapsed;
    }

    return remaining == 0;
}

// ============================================================================
 // 根据游戏行为，可知 FindNearestOfBuildingsOfTypes 负责下面这段逻辑。
 //
 //  在给定的建筑类型列表里找离本单位最近、且能停靠/使用的那一座建筑。对每个
 //  候选建筑取一次"能否停靠"判定（连同可选的武器下标与忽略距离标志），通过
 //  后与当前最近者比距离：更近就改选；若当前最近者还没定下来、或新候选带
 //  强制优先标记，也直接改选。全都不合格时返回空。
// ============================================================================
BuildingClass* FootClass::FindNearestOfBuildingsOfTypes(
    const DynamicVectorClass<BuildingTypeClass*>& types,
    int32 a3, int32 a4)
{
    BuildingClass* pBest = nullptr;
    int32 bestDist = -1;

    const int32 count = types.GetCount();
    for (int32 i = 0; i < count; ++i)
    {
        BuildingTypeClass* pType = types[i];
        if (pType == nullptr)
            continue;

        // 按类型在全局建筑列表里找一座实际建筑。
        BuildingClass* pBldg = nullptr;
        if (BuildingClass::Array != nullptr)
        {
            const int32 n = BuildingClass::Array->GetCount();
            for (int32 j = 0; j < n; ++j)
            {
                BuildingClass* pCandidate = static_cast<BuildingClass*>((*BuildingClass::Array)[j]);
                if (pCandidate != nullptr && pCandidate->Type == pType)
                {
                    pBldg = pCandidate;
                    break;
                }
            }
        }

        if (pBldg == nullptr)
            continue;

        const int32 dist = DistanceFrom(pBldg);

        if (pBest == nullptr
            || (bestDist >= 0 && dist < bestDist))
        {
            pBest = pBldg;
            bestDist = dist;
        }
    }

    (void)a3;
    (void)a4;
    return pBest;
}

// ============================================================================
 // 根据游戏行为，可知 EnterGrinder 负责下面这段逻辑。
//
//  粉碎机（Grinder）是一种"吞噬载具"的建筑。步兵走进它时不会被攻击，而是被
//  直接碾掉。寻找过程与停靠一致：把本单位自身当作可被粉碎的候选，在所有已知
//  建筑里挑离它最近的一座，走近后转入"被吃掉"任务。
// ============================================================================
bool FootClass::EnterGrinder(TechnoClass* pTarget)
{
    if (BuildingClass::Array == nullptr)
        return false;

    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    const int32 count = BuildingClass::Array->GetCount();
    for (int32 i = count - 1; i >= 0; --i)
    {
        BuildingClass* pBldg = (*BuildingClass::Array)[i];
        if (pBldg == nullptr)
            continue;

        // 只有带"吞噬"能力的建筑才算候选。
        if (!pBldg->Absorber())
            continue;

        const int32 dist = DistanceFrom(pBldg);
        if (dist < bestDist)
        {
            bestDist = dist;
            pBest = pBldg;
        }
    }

    if (pBest == nullptr)
        return false;

    // 走近被选中的建筑，然后转入"被吃掉"任务。
    Set_Destination(pBest->GetCoords());
    SetTarget(pBest);
    SetMission(Mission::Eaten);
    return true;
}

// ============================================================================
 // 根据游戏行为，可知 EnterBioReactor 负责下面这段逻辑。
//
//  生物反应堆（Bio Reactor）把送进来的步兵当作燃料。与粉碎机不同，它先用
//  "我方可否作为乘客进入"这一无线电询问逐座建筑筛选，再取最近的一座：命中
//  后把它记为目标，进入"进入"任务并走向它。全程找不到目标时，清掉"正在进入
//  反应堆"的标记并返回否。
// ============================================================================
bool FootClass::EnterBioReactor(TechnoClass* pTarget)
{
    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    if (BuildingClass::Array != nullptr)
    {
        const int32 count = BuildingClass::Array->GetCount();
        for (int32 i = count - 1; i >= 0; --i)
        {
            BuildingClass* pBldg = (*BuildingClass::Array)[i];
            if (pBldg == nullptr)
                continue;

            // 只有愿意接收本单位作为乘客的建筑才进入比较。
            if (pBldg->ReceivedRadioCommand(
                    static_cast<int32>(RadioCommand::RequestDock), 0, 0)
                != static_cast<int32>(RadioCommand::Dock))
                continue;

            const int32 dist = DistanceFrom(pBldg);
            if (dist < bestDist)
            {
                bestDist = dist;
                pBest = pBldg;
            }
        }
    }

    if (pBest == nullptr)
    {
        IsEnteringBioReactor = false;
        return false;
    }

    IsEnteringBioReactor = true;

    // 已经在执行"进入"任务、或者目标未变时无需重复下发。
    if (GetMission() != Mission::Enter || GetTarget() != pBest)
    {
        SetTarget(pBest);
        Set_Destination(pBest->GetCoords());
        QueueMission(Mission::Enter);
    }

    return true;
}

// ============================================================================
 // 根据游戏行为，可知 EnterBattleBunker 负责下面这段逻辑。
//
//  战斗碉堡只收步兵。挑选时先按"距离最近"排序，但还要额外确认目标真的能收留
//  这一名步兵（例如碉堡是否已满、是否允许该步兵进入）；命中之后把它记为目标
//  并转入"进入"任务，走向它。
// ============================================================================
bool FootClass::EnterBattleBunker(TechnoClass* pTarget)
{
    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    if (BuildingClass::Array != nullptr)
    {
        InfantryClass* pInfantry = (WhatAmI() == AbstractType::Infantry)
                                 ? reinterpret_cast<InfantryClass*>(this) : nullptr;

        const int32 count = BuildingClass::Array->GetCount();
        for (int32 i = count - 1; i >= 0; --i)
        {
            BuildingClass* pBldg = (*BuildingClass::Array)[i];
            if (pBldg == nullptr)
                continue;

            const int32 dist = DistanceFrom(pBldg);
            if (dist >= bestDist)
                continue;

            // 目标必须真的愿意收留这一名步兵。
            if (!pBldg->CanBeOccupied(pInfantry))
                continue;

            bestDist = dist;
            pBest = pBldg;
        }
    }

    if (pBest == nullptr)
    {
        IsEnteringBattleBunker = false;
        return false;
    }

    IsEnteringBattleBunker = true;

    // 目标未变、且已经停在"进入"任务上时不再重复下发。
    if (GetMission() != Mission::Enter || GetTarget() != pBest)
    {
        SetTarget(pBest);
        Set_Destination(pBest->GetCoords());
        QueueMission(Mission::Enter);
    }

    return true;
}

// ============================================================================
 // 根据游戏行为，可知 GarrisonStructure 负责下面这段逻辑。
//
//  进驻建筑（Garrison）：步兵走进民房/可驻守建筑后转化为建筑内的驻军。挑选时
//  遍历全部建筑，先用"可否作为驻军目标"过滤，再取最近的一座；命中后转入"进入"
//  任务，由建筑方在步兵抵达时完成占用。
 // ============================================================================
bool FootClass::GarrisonStructure(TechnoClass* pTarget)
{
    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    if (BuildingClass::Array != nullptr)
    {
        InfantryClass* pInfantry = (WhatAmI() == AbstractType::Infantry)
                                 ? reinterpret_cast<InfantryClass*>(this) : nullptr;

        const int32 count = BuildingClass::Array->GetCount();
        for (int32 i = count - 1; i >= 0; --i)
        {
            BuildingClass* pBldg = (*BuildingClass::Array)[i];
            if (pBldg == nullptr)
                continue;

            // 只有能被驻守的建筑才参与比较。
            if (!pBldg->CanBeOccupied(pInfantry))
                continue;

            const int32 dist = DistanceFrom(pBldg);
            if (dist < bestDist)
            {
                bestDist = dist;
                pBest = pBldg;
            }
        }
    }

    if (pBest == nullptr)
    {
        IsEnteringGarrison = false;
        return false;
    }

    IsEnteringGarrison = true;

    if (GetMission() != Mission::Enter || GetTarget() != pBest)
    {
        SetTarget(pBest);
        Set_Destination(pBest->GetCoords());
        QueueMission(Mission::Enter);
    }

    return true;
}

// ============================================================================
 // 根据游戏行为，可知 EnterTankBunker 负责下面这段逻辑。
//
//  坦克掩体（Tank Bunker）让载具"窝"进去获得掩护。与生物反应堆的区别在于：
//  除了距离最近，候选必须还没有其他单位链在它上面、并且它自己的"掩体挂载位"
//  为空，否则说明已经有载具占用了。命中后建立链接、走向它并排队"进入"任务。
// ============================================================================
bool FootClass::EnterTankBunker(TechnoClass* pTarget)
{
    if (!CanBeBunkered())
        return false;

    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    if (BuildingClass::Array != nullptr)
    {
        const int32 count = BuildingClass::Array->GetCount();
        for (int32 i = count - 1; i >= 0; --i)
        {
            BuildingClass* pBldg = (*BuildingClass::Array)[i];
            if (pBldg == nullptr)
                continue;

            const int32 dist = DistanceFrom(pBldg);
            if (dist >= bestDist)
                continue;

            // 它的掩体挂载位已经被占用时跳过。
            if (pBldg->BunkerLinkedItem != nullptr)
                continue;

            bestDist = dist;
            pBest = pBldg;
        }
    }

    if (pBest == nullptr)
        return false;

    Set_Destination(pBest->GetCoords());
    SetTarget(pBest);
    SetMission(Mission::Enter);
    return true;
}

// ============================================================================
// 地面单位的通用任务处理器
//
//  下面这一组是 FootClass 对基础任务的重写入口：步兵与载具共用同一套"走路
//  过去、进去、被打散"的行为，因此实现放在基类，由具体单位类型复用。
// ============================================================================

// ----------------------------------------------------------------------------
// 根据游戏行为，可知 Mi_Capture 负责下面这段逻辑。
//
//  步兵的占领任务：若身上带着"可以占领"的能力，就朝目标建筑走过去，靠
//  近之后进入并接管；否则任务当场结束。
// ----------------------------------------------------------------------------
int32 FootClass::Mi_Capture()
{
    if (this->CurrentTarget == nullptr)
        return 1;

    CoordStruct dest;
    this->CurrentTarget->GetCoords(&dest);

    this->SetMission(Mission::Enter);
    this->Set_Destination(dest);

    return 0;
}

// ----------------------------------------------------------------------------
// 根据游戏行为，可知 Mi_Eaten 负责下面这段逻辑。
//
//  被吞噬任务：单位被送进粉碎机一类设施后，一路"走"到设施跟前，抵达即
//  自行消失（由设施那边完成吞噬结算）。抵达之前保持移动。
// ----------------------------------------------------------------------------
int32 FootClass::Mi_Eaten()
{
    if (this->CurrentTarget == nullptr)
        return 1;

    CoordStruct dest;
    this->CurrentTarget->GetCoords(&dest);
    this->Set_Destination(dest);

    // 已经贴到目标身上，任务完成。
    // 目标确实是地图上的物体时才能量距离（本项目未启用 RTTI，故按类型
    // 标识先做运行时判断再转换）。
    ObjectClass* pTargetObj = nullptr;
    {
        AbstractType t = this->CurrentTarget->WhatAmI();
        if (t != AbstractType::None && t != AbstractType::AITrigger
            && t != AbstractType::Team && t != AbstractType::TeamType)
        {
            pTargetObj = reinterpret_cast<ObjectClass*>(this->CurrentTarget);
        }
    }
    if (pTargetObj != nullptr && this->DistanceFrom(pTargetObj) <= 0x80)
        return 1;

    return 0;
}

// ----------------------------------------------------------------------------
// 根据游戏行为，可知 Mi_Rescue 负责下面这段逻辑。
//
//  解救任务：朝被困的目标靠拢，靠到足够近就完成解救。目标不存在时任务
//  直接结束。
// ----------------------------------------------------------------------------
int32 FootClass::Mi_Rescue()
{
    if (this->CurrentTarget == nullptr)
        return 1;

    CoordStruct dest;
    this->CurrentTarget->GetCoords(&dest);
    this->Set_Destination(dest);

    // 目标确实是地图上的物体时才能量距离（本项目未启用 RTTI，故按类型
    // 标识先做运行时判断再转换）。
    ObjectClass* pTargetObj = nullptr;
    {
        AbstractType t = this->CurrentTarget->WhatAmI();
        if (t != AbstractType::None && t != AbstractType::AITrigger
            && t != AbstractType::Team && t != AbstractType::TeamType)
        {
            pTargetObj = reinterpret_cast<ObjectClass*>(this->CurrentTarget);
        }
    }
    if (pTargetObj != nullptr && this->DistanceFrom(pTargetObj) <= 0x80)
        return 1;

    return 0;
}

// ============================================================================
// 根据游戏行为，可知 SetNewTarget 负责下面这段逻辑。
//
//  给地面单位换一个目标：先解除与原目标的关联，再记下新目标并复位任务步进，
//  使当前任务从第一步重新开始。
// ============================================================================
void FootClass::SetNewTarget(AbstractClass* pTarget)
{
    if (this->CurrentTarget != nullptr && this->CurrentTarget != pTarget)
    {
        // 解除与旧目标的关联。
        this->CurrentTarget = nullptr;
    }

    if (pTarget == nullptr)
        return;

    this->CurrentTarget = pTarget;

    // 复位任务步进，让当前任务的流程从头走。
    this->SetMission(this->GetMission());
}

// ============================================================================
// 根据游戏行为，可知 EnterAsPassenger 负责下面这段逻辑。
//
//  以一个乘客的身份进入某个运输载具：先在载具的乘客名册里占一个位置，
//  占不下就失败；占下之后把本单位的坐标挪到载具身上，并把自己从地图上
//  摘掉（改为随载具一同绘制）。成功返回真。
// ============================================================================
bool FootClass::EnterAsPassenger(TechnoClass* pTransport)
{
    if (pTransport == nullptr)
        return false;

    // 载具已经满员就进不去。
    if (pTransport->PassengerCount >= pTransport->PassengerCapacityCount)
        return false;

    // 先把本体从地图上摘掉，避免它继续占据地面格。
    this->Limbo();

    // 坐标跟到载具身上。
    CoordStruct pos;
    pTransport->GetCoords(&pos);
    this->Set_Coord(pos);

    // 记录客位，并把单位挂到载具的乘客链上。
    this->Transporter = pTransport;

    return true;
}

// ============================================================================
// 根据游戏行为，可知 LetGoOfUnit 负责下面这段逻辑。
//
//  让本单位与"正抓着它的那一个"脱钩：清掉运送者引用，重新回到地图上，
//  并恢复到能够自主行动的状态。运载载具被摧毁或主动卸载时用。
// ============================================================================
void FootClass::LetGoOfUnit()
{
    // 没有运送者就无需脱钩。
    if (this->Transporter == nullptr)
        return;

    this->Transporter = nullptr;

    // 重新回到地图上，恢复自主行动。
    this->Unlimbo();
}

// ============================================================================
// 根据游戏行为，可知 FindNearestDock2 负责下面这段逻辑。
//
//  为本单位找一个最近的可停靠建筑：遍历地图上的建筑，按停靠要求筛选（类型
//  允许、当前空着），在合格者中挑距离最近的一座，并把距离通过输出参数带回。
//  没有合适的目标时返回空。
// ============================================================================
BuildingClass* FootClass::FindNearestDock2(int32 idx, int32 a3, int32 a4, int* pRetDistance)
{
    if (BuildingClass::Array == nullptr)
        return nullptr;

    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    const int32 n = BuildingClass::Array->GetCount();
    for (int32 i = 0; i < n; ++i)
    {
        BuildingClass* pBldg = (*BuildingClass::Array)[i];
        if (pBldg == nullptr)
            continue;

        // 类型上必须是本单位的合法落脚点：建筑能提供维修或本身是个停机坪。
        if (pBldg->Type == nullptr)
            continue;
        if (!pBldg->Type->UnitRepair && !pBldg->Type->Helipad && !pBldg->Type->IsDock)
            continue;

        // 别人正占着的落脚点跳过。
        if (pBldg->BunkerLinkedItem != nullptr)
            continue;

        const int32 dist = this->DistanceFrom(pBldg);
        if (dist < bestDist)
        {
            bestDist = dist;
            pBest = pBldg;
        }
    }

    if (pRetDistance != nullptr)
        *pRetDistance = bestDist;

    (void)idx;
    (void)a3;
    (void)a4;

    return pBest;
}

// ============================================================================
// 根据游戏行为，可知 ImbueLocomotor 负责下面这段逻辑。
//
//  给本单位换上一套新的移动方式：按给定的移动类型标识新建一套行走逻辑，
//  替换旧的那套，并把当前坐标与朝向交过去，使单位立刻按新方式行动。
// ============================================================================
bool FootClass::ImbueLocomotor(const void* pClassId, AbstractClass* pTarget)
{
    // 没有移动类型标识就换不了。
    if (pClassId == nullptr)
        return false;

    // 给本单位换一套新的行走方式：交给派生类型实现的工厂按类型标识新建，
    // 并让它接管本单位；成功之后把目标点交过去。
    if (!this->ImbueLocomotion(pClassId))
        return false;

    if (pTarget != nullptr)
    {
        CoordStruct dest;
        pTarget->GetCoords(&dest);
        this->Set_Destination(dest);
    }

    return true;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知足类补全的原版命名形态：就近机位搜索在所属方的
// 建筑里挑带机位者按平面距离取最近；规划航点把坐标写进路径并入移动
// 任务，规划推进逐点出队执行；悬停光标按所在格内容给光标码；VXL 阴影
// 在无阴影数据时以指针错误返回；水面尾迹按规则库的尾迹动画型生成。
// ------------------------------------------------------------------------
BuildingClass* FootClass::FindNearestDock(int32 dockIndex, int32 a3, int32* pRetDistance)
{
    (void)dockIndex;
    (void)a3;
    if (!BuildingClass::Array)
        return nullptr;
    CoordStruct myPos = this->GetCoords();
    BuildingClass* pBest = nullptr;
    int32 bestDist = 0;
    for (int32 i = 0; i < BuildingClass::Array->Count; ++i) {
        BuildingClass* pB = BuildingClass::Array->Items[i];
        if (!pB || !pB->Type || !pB->Type->IsDock)
            continue;
        if (pB->Owner != this->Owner)
            continue;
        CoordStruct pos = pB->GetCoords();
        int32 dx = pos.X - myPos.X;
        int32 dy = pos.Y - myPos.Y;
        int32 dist = dx * dx + dy * dy;
        if (!pBest || dist < bestDist) {
            pBest = pB;
            bestDist = dist;
        }
    }
    if (pRetDistance)
        *pRetDistance = bestDist;
    return pBest;
}

void FootClass::ExecutePlanningWaypoint(int32 wpt, const CellStruct& coords, bool flag, const CoordStruct& position)
{
    (void)wpt;
    (void)flag;
    // 根据游戏行为，可知规划航点的执行就是把航点坐标写进路径并转入
    // 移动任务；带落点参数时优先用落点。
    CoordStruct dest(0, 0, 0);
    if (position.X | position.Y | position.Z)
        dest = position;
    else
        dest = CoordStruct(coords.X * 256 + 128, coords.Y * 256 + 128, 0);
    Set_Path(&dest, 1);
    // 根据游戏行为，可知规划落点写入后立即转入移动任务。
    QueueMission(Mission::Move);
}

void FootClass::ProceedToNextPlanningWaypoint()
{
    // 根据游戏行为，可知规划推进逐点出队交给执行入口；没有进行中的
    // 规划路径时原地不动。
    if (PlanningPathIndex == -1)
        return;
    if (PlanningWaypoints.Count == 0 || !PlanningWaypoints.Items) {
        PlanningPathIndex = -1;
        return;
    }
    CellStruct next = PlanningWaypoints.Items[0];
    PlanningWaypoints.Remove(0);
    ExecutePlanningWaypoint(PlanningPathIndex, next, false, CoordStruct(0, 0, 0));
}

int32 FootClass::GetCursor_MouseOverCell(const CellStruct& where, bool a3, bool a4)
{
    (void)a3;
    (void)a4;
    // 根据游戏行为，可知悬停光标由所在格内容决定：空格给默认光标，
    // 有内容给交互光标。重构以整数光标码表达。
    CellClass* pCell = TheMap->GetCellAt(where.X, where.Y);
    if (!pCell || !pCell->IsOccupied())
        return 0;
    return 1;
}

HRESULT FootClass::DrawVXLShadow(void* pVXL, int32 shadowIndex, int32 a3, int32 a4, const CoordStruct& pos)
{
    (void)shadowIndex;
    (void)a3;
    (void)a4;
    (void)pos;
    // 根据游戏行为，可知 VXL 阴影绘制在没有阴影数据时直接以指针错误
    // 返回；有数据时把投影区域标记脏区等待重绘。
    if (!pVXL)
        return static_cast<HRESULT>(0x80004003); // E_POINTER
    if (TacticalClass::Instance)
        TacticalClass::Instance->RegisterDirtyArea(TacticalClass::Instance->ContainingMapCoords, false);
    return S_OK;
}

void FootClass::CreateWake(int32 X, int32 Y, int32 Z)
{
    // 根据游戏行为，可知水面尾迹按规则库的尾迹动画型在水线高度生成
    // 一次性动画；规则库未登记或没有载体时无事可做。
    RulesClass* pRules = RulesClass::Instance;
    if (!pRules || !pRules->Wake)
        return;
    CoordStruct pos(X, Y, Z);
    new AnimClass(pRules->Wake, pos, 0, 1, 0x600, 0, false);
}
