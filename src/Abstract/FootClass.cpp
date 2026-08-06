#include <Abstract/FootClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Locomotion/LocomotionClass.h>

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
