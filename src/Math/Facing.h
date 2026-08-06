#pragma once

#include "../Core/Definitions.h"
#include <utility>

// ============================================================================
// Math/Facing.h - 8-direction facing system
//
//  DirType (the 8-direction enum) and DirStruct (the raw value container)
//  are declared in Core/Definitions.h.  This header provides:
//
//    * Facing      - static utility helpers for converting / rotating
//                    DirStruct values and deriving movement deltas.
//
//    * FacingClass - smooth (time-interpolated) rotation tracker used by
//                    turrets and turning objects.  It remembers the Start
//                    facing, the Target (End) facing, and a Timer that
//                    drives the interpolation toward the target.
//
//  The facing value space is 0..255 (256 discrete steps) where each of the
//  8 cardinal/intercardinal directions occupies a 32-step sector:
//
//      N=0  NE=16  E=32  SE=48  S=64  SW=80  W=96  NW=112
//
//  (matching the DirType enum values in Definitions.h).
// ============================================================================

// ----------------------------------------------------------------------------
// Facing - static utility helpers
// ----------------------------------------------------------------------------
struct Facing {
    static constexpr int32 FacingCount = 8;
    static constexpr int32 FacingStep  = 256 / FacingCount;   // 32

    // Convert a DirType to a DirStruct.
    static DirStruct FromDirType(DirType dt) {
        return DirStruct(static_cast<uint8>(dt));
    }

    // Convert a DirStruct back to the nearest DirType.
    static DirType ToDirType(DirStruct dir) {
        int32 idx = (dir.Value + FacingStep / 2) / FacingStep;
        idx %= FacingCount;
        switch (idx) {
            case 0:  return DirType::N;
            case 1:  return DirType::NE;
            case 2:  return DirType::E;
            case 3:  return DirType::SE;
            case 4:  return DirType::S;
            case 5:  return DirType::SW;
            case 6:  return DirType::W;
            case 7:  return DirType::NW;
            default: return DirType::N;
        }
    }

    // Movement delta (dx, dy) for a facing, used by pathfinding.
    static std::pair<int32, int32> GetMovementDelta(DirStruct dir) {
        static const int32 deltas[8][2] = {
            {0, -1}, {1, -1}, {1, 0}, {1, 1},
            {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}
        };
        int32 idx = dir.Value / FacingStep;
        if (idx >= FacingCount) idx = 0;
        return {deltas[idx][0], deltas[idx][1]};
    }

    // Rotate a facing by a number of 8-direction steps (CW positive).
    static DirStruct Rotate(DirStruct dir, int32 steps) {
        int32 val = static_cast<int32>(dir.Value) + steps * FacingStep;
        while (val < 0)   val += 256;
        while (val >= 256) val -= 256;
        return DirStruct(static_cast<uint8>(val));
    }

    // Shortest signed difference (target - current) in raw units, in range
    // [-128, 127].  Positive = clockwise.
    static int32 Difference(DirStruct from, DirStruct to) {
        int32 diff = static_cast<int32>(to.Value) - static_cast<int32>(from.Value);
        if (diff > 128)  diff -= 256;
        if (diff < -128) diff += 256;
        return diff;
    }
};

// ----------------------------------------------------------------------------
// FacingClass - smooth time-interpolated rotation
//
//  Tracks a rotation from Start toward Target over a number of game frames.
//  Each call to Update() advances the current facing one step along the
//  shortest arc.  When the timer expires the current facing snaps to Target.
//
//  This mirrors the YRpp FacingClass which is used by turrets, infantry
//  body rotation, and vehicle turning.
// ----------------------------------------------------------------------------
class FacingClass {
public:
    // Current (raw) facing value.
    DirStruct Raw;
    // Facing the rotation started from.
    DirStruct Start;
    // Desired end facing.
    DirStruct Target;
    // Remaining frames of rotation (0 = not rotating).
    int32     Timer;

    FacingClass() noexcept
        : Raw(0), Start(0), Target(0), Timer(0) {}

    explicit FacingClass(DirStruct facing) noexcept
        : Raw(facing), Start(facing), Target(facing), Timer(0) {}

    // ------------------------------------------------------------------
    // Accessors
    // ------------------------------------------------------------------

    // Returns the current (possibly mid-rotation) facing.
    DirStruct GetFacing() const { return Raw; }

    // Returns the current facing as a reference (for direct mutation).
    DirStruct& GetFacingRef() { return Raw; }

    // Returns the desired (target) facing.
    DirStruct GetDesiredFacing() const { return Target; }

    // True while a rotation is in progress.
    bool IsRotating() const { return Timer > 0; }

    // True if rotating clockwise.
    bool IsRotatingCW() const {
        if (!IsRotating()) return false;
        return Facing::Difference(Start, Target) > 0;
    }

    // True if rotating counter-clockwise.
    bool IsRotatingCCW() const {
        if (!IsRotating()) return false;
        return Facing::Difference(Start, Target) < 0;
    }

    // Number of 8-direction steps remaining in the rotation.
    int32 RotationSteps() const {
        return Timer;
    }

    // ------------------------------------------------------------------
    // Mutators
    // ------------------------------------------------------------------

    // Instantly set the facing (no rotation).
    void SetFacing(DirStruct facing) {
        Raw    = facing;
        Start  = facing;
        Target = facing;
        Timer  = 0;
    }

    // Begin rotating toward the desired facing over a number of frames.
    void SetDesiredFacing(DirStruct facing, int32 rate) {
        if (rate <= 0) {
            SetFacing(facing);
            return;
        }
        Start  = Raw;
        Target = facing;
        Timer  = rate;
    }

    // Begin rotating toward the desired facing at the default rate.
    void SetDesiredFacing(DirStruct facing) {
        int32 diff = Facing::Difference(Raw, facing);
        int32 steps = diff < 0 ? -diff : diff;
        SetDesiredFacing(facing, steps);
    }

    // ------------------------------------------------------------------
    // Per-frame update
    // ------------------------------------------------------------------

    // Advance the rotation by one frame.  Returns true if still rotating
    // after the update.
    bool Update() {
        if (Timer <= 0) return false;

        int32 diff = Facing::Difference(Raw, Target);
        if (diff == 0) {
            Timer = 0;
            return false;
        }

        // Step size so we reach Target exactly when Timer hits 0.
        int32 step = diff / Timer;
        if (step == 0) step = (diff > 0) ? 1 : -1;

        int32 val = static_cast<int32>(Raw.Value) + step;
        while (val < 0)   val += 256;
        while (val >= 256) val -= 256;
        Raw = DirStruct(static_cast<uint8>(val));

        --Timer;
        if (Timer <= 0) {
            Raw = Target;
            return false;
        }
        return true;
    }
};
