#include "DriveLocomotionClass.h"

#include <cmath>
#include <cstdlib>

#include "../Map/MapClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Abstract/InfantryClass.h"

// ============================================================================
// DriveLocomotionClass - Vehicle driving locomotion.
// Track-based movement, speed per terrain type, turret/barrel rotation,
// body rotation, crush logic for infantry, building collision, slope
// handling, and bridge traversal.
// ============================================================================

DriveLocomotionClass::DriveLocomotionClass()
    : LocomotionClass()
    , PreviousRamp(0)
    , CurrentRamp(0)
    , Destination(0, 0, 0)
    , HeadToCoord(0, 0, 0)
    , SpeedAccum(0)
    , MovementSpeed(1.0)
    , TrackNumber(0)
    , TrackIndex(0)
    , IsOnShortTrack(false)
    , IsTurretLockedDown(0)
    , IsRotating(false)
    , IsDriving(false)
    , IsRocking(false)
    , IsLocked(false)
    , Piggybackee(nullptr)
    , TrackType(0)
    , Crusher(false)
    , Destroyer(false)
    , IsHovering(false)
    , IsAmphibious(false)
    , CurrentFacing(0)
    , TargetFacing(0)
    , RotationSpeed(8)
{
    Speed = 128;
}

DriveLocomotionClass::~DriveLocomotionClass()
{
    if (Piggybackee) {
        Piggybackee = nullptr;
    }
}

HRESULT DriveLocomotionClass::GetClassID(CLSID* pClassID)
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

int32 DriveLocomotionClass::Size()
{
    return sizeof(DriveLocomotionClass);
}

// ============================================================================
// Begin_Piggyback / End_Piggyback - Piggyback system for vehicle
// carrying (e.g., transport loading).
// ============================================================================

HRESULT DriveLocomotionClass::Begin_Piggyback(ILocomotion* pointer)
{
    Piggybackee = pointer;
    return S_OK;
}

HRESULT DriveLocomotionClass::End_Piggyback(ILocomotion** pointer)
{
    if (pointer) {
        *pointer = Piggybackee;
    }
    Piggybackee = nullptr;
    return S_OK;
}

// ============================================================================
// Move_To - Sets vehicle destination. Begins driving toward target.
// ============================================================================

void DriveLocomotionClass::Move_To(CoordStruct to)
{
    Destination = to;
    Dest = to;
    IsDriving = true;
    IsMoving = true;
    IsRotating = false;

    TargetFacing = CoordMath::DirectionTo(CurrentCoord, to);
    HeadToCoord = to;
}

// ============================================================================
// Stop_Moving - Halts vehicle movement.
// ============================================================================

void DriveLocomotionClass::Stop_Moving()
{
    IsDriving = false;
    IsMoving = false;
    IsRotating = false;
    IsRocking = false;
    SpeedAccum = 0;
}

// ============================================================================
// Do_Turn - Initiates vehicle body rotation toward a facing direction.
// ============================================================================

void DriveLocomotionClass::Do_Turn(DirStruct coord)
{
    // asm: 载具不做瞬转——把目标朝向登记到 FacingClass，由每帧的
    //   RotateTowards 按 RotationSpeed 逐帧逼近；到位前 Is_Moving_Now
    //   恒为真，其它系统据此知道"车还在转"。
    TargetFacing = coord;
    if (CurrentFacing.Value == coord.Value) {
        IsRotating = false;
        return;
    }
    IsRotating = true;
}

// ============================================================================
// Do_Turret_Turn - Vehicle turret rotation independent of body.
// ============================================================================

void DriveLocomotionClass::Do_Turret_Turn(DirStruct coord)
{
    if (!Owner || !Owner->HasTurret()) {
        return;
    }

    DirStruct turretFacing = Owner->GetTurretFacing();
    int32 diff = static_cast<int32>(coord.Value) - static_cast<int32>(turretFacing.Value);

    if (diff == 0) {
        return;
    }

    int32 turretSpeed = 5;
    int32 step = 0;

    if (diff < -128 || (diff > 0 && diff <= 128)) {
        step = (diff > 0) ? turretSpeed : -turretSpeed;
    } else {
        step = (diff > 0) ? -turretSpeed : turretSpeed;
    }

    int32 newVal = static_cast<int32>(turretFacing.Value) + step;
    if (newVal < 0) {
        newVal += 256;
    }
    if (newVal >= 256) {
        newVal -= 256;
    }

    if (std::abs(newVal - static_cast<int32>(coord.Value)) <= turretSpeed) {
        Owner->SetTurretFacing(coord);
    } else {
        Owner->SetTurretFacing(DirStruct(static_cast<uint8>(newVal)));
    }
}

// ============================================================================
// Is_Moving - 目的地/朝往点任一非零即为在动。
// asm: DriveLocomotionClass_ILocomotion_IsMoving   (asm:372)
//   先看 Destination(+0x34)：非零 → true；否则看 HeadTo(+0x40)：非零且与载体
//   的缓存绘制点（+0x9C）不相等 → true；两者都归零 → false。
// ============================================================================

bool DriveLocomotionClass::Is_Moving()
{
    if (!(Destination == CoordStruct(0, 0, 0))) {
        return true;
    }

    if (HeadToCoord == CoordStruct(0, 0, 0)) {
        return false;
    }

    if (Owner) {
        const CoordStruct cached = Owner->CachedLocation;
        if (cached == HeadToCoord) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Is_Moving_Now - 转身未完成也算在动（原版先看朝向是否到位）。
// asm: DriveLocomotionClass_ILocomotion_IsMovingNow   (asm:467)
//   1) 载体 FacingStruct 尚未到位（IsReady 为假）→ true；
//   2) 否则 Is_Moving() && HeadTo != 0 && 载体当前速度 > 0 → true；
//   3) 其它 → false。
// ============================================================================

bool DriveLocomotionClass::Is_Moving_Now()
{
    if (Owner && IsRotating) {
        return true;
    }

    if (!Is_Moving()) {
        return false;
    }
    if (HeadToCoord == CoordStruct(0, 0, 0)) {
        return false;
    }
    if (Owner && Owner->GetCurrentSpeed() <= 0) {
        return false;
    }
    return true;
}

// ============================================================================
// Process - 载具每帧驱动入口。
// asm: 由 sub_4B0F20 承载（Drive 的每帧仿真）。控制流：
//   1) 被锁（Lock）或不在动 → 直接返回；
//   2) 若在摇车（过断桥坠落）→ 走摇车分支；
//   3) 若在转身 → 朝目标朝向逐帧旋转，转到位后清旗标；
//   4) 若在驾驶 → 地形速度修正、加减速曲线、逐格推进、碾压判定、
//      桥梁检查、坡道俯仰；
//   5) 回传是否仍在活动。
// ============================================================================

bool DriveLocomotionClass::Process()
{
    UpdateDrive();
    return IsDriving || IsRotating || IsRocking;
}

// ============================================================================
// UpdateDrive - 载具每帧驱动主体。
// asm: sub_4B0F20   —— 控制流：
//   1) 被锁（Lock）或不在动 → 直接返回；
//   2) 若在摇车（过断桥坠落）→ 走摇车分支；
//   3) 若在转身 → 朝目标朝向逐帧旋转，转到位后清旗标；
//   4) 若在驾驶 → 地形速度、坡道收敛、到达减速、逐格推进、碾压判定、
//      桥梁检查、坡道俯仰。
// ============================================================================

void DriveLocomotionClass::UpdateDrive()
{
    if (IsLocked) {
        return;
    }

    if (!IsDriving && !IsRotating && !IsRocking) {
        return;
    }

    if (IsRocking) {
        ProcessRocking();
        return;
    }

    // --- 转身（原地或行进中） ---
    if (IsRotating) {
        RotateTowards(TargetFacing);
        if (CurrentFacing.Value == TargetFacing.Value) {
            IsRotating = false;
        }
        if (!IsDriving) {
            return;
        }
    }

    if (!IsDriving) {
        return;
    }

    // --- 地形速度修正 ---
    Movement_AI();

    // --- 坡道速度收敛（爬坡降速、下坡恢复） ---
    UpdateSlopeSpeed();

    // --- 到达减速曲线 ---
    float targetSpeed = static_cast<float>(Speed) * SpeedPercentage
        * static_cast<float>(MovementSpeed);

    int32 distToDest = CurrentCoord.DistanceFrom(Destination);
    const int32 decelDist = (Speed > 0) ? Speed * 4 : 256;
    if (distToDest < decelDist) {
        float factor = static_cast<float>(distToDest) / static_cast<float>(decelDist);
        targetSpeed *= (0.4f + 0.6f * factor);
    }

    int32 effectiveSpeed = static_cast<int32>(targetSpeed);
    if (effectiveSpeed <= 0) {
        effectiveSpeed = 1;
    }

    // --- 坡差：上坡减速、下坡加速 ---
    if (MapClass::Instance != nullptr) {
        CellStruct curCell = CoordMath::CoordToCell(CurrentCoord);
        CellStruct dstCell = CoordMath::CoordToCell(Destination);
        CellClass* pCur = MapClass::Instance->GetCellAt(curCell);
        CellClass* pDst = MapClass::Instance->GetCellAt(dstCell);
        if (pCur != nullptr && pDst != nullptr) {
            int32 heightDelta = pDst->Get_Ground_Height() - pCur->Get_Ground_Height();
            if (heightDelta > 0) {
                effectiveSpeed = (effectiveSpeed * 3) / 4;   // 上坡
            } else if (heightDelta < 0) {
                effectiveSpeed = (effectiveSpeed * 5) / 4;   // 下坡
            }
        }
    }

    SpeedAccum += effectiveSpeed;

    // --- 逐格推进 ---
    StepTowardsDestination(effectiveSpeed);

    if (IsDriving) {
        CheckCrush();
        CheckBridge();

        if (IsOnBridge() && CheckBridge()) {
            SlopeTimer.Update(effectiveSpeed);
        }

        Tilt_Pitch_AI();
    }
}

// ============================================================================
// UpdateSlopeSpeed - 由 CurrentRamp 推目标速度系数并逐帧逼近。
// asm: sub_4B0F20 中 MovementSpeed 由坡度与 SlopeTimer 收敛的段。
// ============================================================================

void DriveLocomotionClass::UpdateSlopeSpeed()
{
    float rampTarget = 1.0f;
    if (CurrentRamp > 0) {
        rampTarget = 1.0f - static_cast<float>(CurrentRamp) * 0.08f;
        if (rampTarget < 0.3f) {
            rampTarget = 0.3f;
        }
    }

    double delta = static_cast<double>(rampTarget) - MovementSpeed;
    MovementSpeed += delta * 0.25;

    if (MovementSpeed < 0.3) {
        MovementSpeed = 0.3;
    }
    if (MovementSpeed > 1.0) {
        MovementSpeed = 1.0;
    }
}

// ============================================================================
// StepTowardsDestination - 按速度累积逐格推进并做到达判定。
// asm: sub_4B0F20 的 SpeedAccum >= 256 步进段（含 UpdatePosition 与履带计数）。
// ============================================================================

void DriveLocomotionClass::StepTowardsDestination(int32 effectiveSpeed)
{
    const int32 stepThreshold = 256;

    while (SpeedAccum >= stepThreshold && IsDriving) {
        SpeedAccum -= stepThreshold;

        UpdatePosition();

        // 每推进一格履带动画前进一帧。
        ++TrackNumber;

        int32 dist = CurrentCoord.DistanceFrom(Destination);
        if (dist <= effectiveSpeed) {
            CurrentCoord = Destination;
            if (Owner) {
                Owner->SetCoords(CurrentCoord);
            }
            IsDriving = false;
            IsMoving = false;
            break;
        }
    }
}

// ============================================================================
// ApplyTerrainSpeed - 按地表类型设置 SpeedPercentage（水面非两栖即停车）。
// asm: sub_4B0F20 的地表速度段。
// ============================================================================

void DriveLocomotionClass::ApplyTerrainSpeed()
{
    if (!IsDriving) {
        return;
    }

    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    LandType land = MapClass::Instance->GetLandType(currentCell);

    float baseMod = 1.0f;
    switch (land) {
        case LandType::Road:
            baseMod = 1.1f;
            break;
        case LandType::Rough:
            baseMod = 0.7f;
            break;
        case LandType::Ice:
            baseMod = 0.75f;
            break;
        case LandType::Weeds:
            baseMod = 0.8f;
            break;
        case LandType::Tiberium:
            baseMod = 0.65f;
            break;
        case LandType::Beach:
            baseMod = 0.9f;
            break;
        case LandType::Water:
            if (IsAmphibious) {
                baseMod = 0.6f;
            } else {
                IsDriving = false;
                IsMoving = false;
                return;
            }
            break;
        default:
            baseMod = 1.0f;
            break;
    }

    SpeedPercentage = baseMod;
}


// ============================================================================
// UpdatePosition - Moves the vehicle toward its destination.
// ============================================================================

void DriveLocomotionClass::UpdatePosition()
{
    if (IsDriving && !IsRotating) {
        int32 effectiveSpeed = static_cast<int32>(
            static_cast<float>(Speed) * SpeedPercentage * MovementSpeed);

        CurrentCoord = VectorMath::MoveTowards(CurrentCoord, Destination, effectiveSpeed);

        // Ground the vehicle on the terrain height below.  Hovering units
        // keep altitude but never sink beneath the terrain.
        if (MapClass::Instance != nullptr) {
            int32 groundZ = MapClass::Instance->GetGroundHeight(CurrentCoord);
            if (!IsHovering) {
                CurrentCoord.Z = groundZ;
            } else if (CurrentCoord.Z < groundZ) {
                CurrentCoord.Z = groundZ;
            }
        }

        if (Owner) {
            Owner->SetCoords(CurrentCoord);
        }
    }
}

// ============================================================================
// RotateTowards - Smoothly rotates the vehicle body toward the target facing.
// ============================================================================

void DriveLocomotionClass::RotateTowards(DirStruct targetDir)
{
    int32 diff = static_cast<int32>(targetDir.Value) - static_cast<int32>(CurrentFacing.Value);
    if (diff == 0) {
        return;
    }

    if (diff < -128 || (diff > 0 && diff <= 128)) {
        int32 step = (diff > 0) ? RotationSpeed : -RotationSpeed;
        int32 newVal = static_cast<int32>(CurrentFacing.Value) + step;
        if (newVal < 0) {
            newVal += 256;
        }
        if (newVal >= 256) {
            newVal -= 256;
        }

        if (std::abs(newVal - static_cast<int32>(targetDir.Value)) <= RotationSpeed) {
            CurrentFacing = targetDir;
        } else {
            CurrentFacing = DirStruct(static_cast<uint8>(newVal));
        }
    } else {
        int32 step = (diff > 0) ? -RotationSpeed : RotationSpeed;
        int32 newVal = static_cast<int32>(CurrentFacing.Value) + step;
        if (newVal < 0) {
            newVal += 256;
        }
        if (newVal >= 256) {
            newVal -= 256;
        }

        if (std::abs(newVal - static_cast<int32>(targetDir.Value)) <= RotationSpeed) {
            CurrentFacing = targetDir;
        } else {
            CurrentFacing = DirStruct(static_cast<uint8>(newVal));
        }
    }

    if (Owner) {
        Owner->SetFacing(CurrentFacing);
    }
}

// ============================================================================
// CheckCrush - Checks if the vehicle is crushing infantry in its path.
// ============================================================================

void DriveLocomotionClass::CheckCrush()
{
    if (!Crusher && !Destroyer) {
        return;
    }

    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);

    for (int32 dx = -1; dx <= 1; ++dx) {
        for (int32 dy = -1; dy <= 1; ++dy) {
            CellStruct cell;
            cell.X = static_cast<int16>(static_cast<int32>(currentCell.X) + dx);
            cell.Y = static_cast<int16>(static_cast<int32>(currentCell.Y) + dy);

            if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
                continue;
            }

            ObjectClass* occupant = MapClass::Instance->GetCellOccupier(cell);
            if (!occupant) {
                continue;
            }

            if (occupant->WhatAmI() == AbstractType::Infantry) {
                InfantryClass* infantry = static_cast<InfantryClass*>(occupant);
                if (Destroyer) {
                    infantry->TakeDamage(10000, static_cast<ObjectClass*>(nullptr), WarheadTypeClass::GetDefault());
                } else if (Crusher) {
                    infantry->TakeDamage(250, Owner, WarheadTypeClass::GetDefault());
                }
            }
        }
    }
}

// ============================================================================
// CheckBuildingCollision - Checks for and handles building collisions.
// ============================================================================

bool DriveLocomotionClass::CheckBuildingCollision()
{
    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);

    for (int32 dx = -1; dx <= 1; ++dx) {
        for (int32 dy = -1; dy <= 1; ++dy) {
            CellStruct cell;
            cell.X = static_cast<int16>(static_cast<int32>(currentCell.X) + dx);
            cell.Y = static_cast<int16>(static_cast<int32>(currentCell.Y) + dy);

            if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
                continue;
            }

            if (MapClass::Instance->IsCellOccupied(cell)) {
                ObjectClass* occupant = MapClass::Instance->GetCellOccupier(cell);
                if (occupant && occupant->WhatAmI() == AbstractType::Building) {
                    if (Destroyer) {
                        return true;
                    }
                    return !Crusher;
                }
            }
        }
    }
    return false;
}

// ============================================================================
// IsOnBridge - Checks if the vehicle is currently on a bridge.
// ============================================================================

bool DriveLocomotionClass::IsOnBridge() const
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }
    return MapClass::Instance->IsBridgeCell(cell);
}

// ============================================================================
// CheckBridge - Validates bridge integrity and handles destruction.
// ============================================================================

bool DriveLocomotionClass::CheckBridge()
{
    if (!IsOnBridge()) {
        return false;
    }

    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    if (MapClass::Instance->IsBridgeDestroyed(cell)) {
        IsDriving = false;
        IsMoving = false;
        IsRocking = true;
        return true;
    }

    return false;
}

// ============================================================================
// ProcessRocking - Handles vehicle rocking animation after bridge collapse.
// ============================================================================

bool DriveLocomotionClass::ProcessRocking()
{
    int32 rockTimer = 30;
    static int32 rockPhase = 0;

    if (rockPhase < rockTimer) {
        ++rockPhase;
        if (Owner) {
            Owner->SetSequence(Sequence::Tumble);
        }
        return true;
    }

    rockPhase = 0;
    IsRocking = false;
    return false;
}

// ============================================================================
// Can_Enter_Cell - Vehicle-specific cell traversal check.
// ============================================================================

Move DriveLocomotionClass::Can_Enter_Cell(CellStruct cell)
{
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return Move::No;
    }

    LandType land = MapClass::Instance->GetLandType(cell);

    if (IsAmphibious) {
        if (land == LandType::Wall) {
            return Move::No;
        }
        return Move::OK;
    }

    if (IsHovering) {
        if (land == LandType::Wall || land == LandType::Rock) {
            return Move::No;
        }
        return Move::OK;
    }

    if (land == LandType::Water || land == LandType::Wall) {
        return Move::No;
    }

    if (land == LandType::Rock && !Destroyer) {
        return Move::No;
    }

    return Move::OK;
}

// ============================================================================
// Mark_All_Occupation_Bits - Vehicle footprint marking.
// ============================================================================

void DriveLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    bool occupy = (mark == MarkType::Up);

    MapClass::Instance->MarkCellOccupied(cell, occupy);

    for (int32 dx = -1; dx <= 1; ++dx) {
        for (int32 dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            CellStruct adj;
            adj.X = static_cast<int16>(static_cast<int32>(cell.X) + dx);
            adj.Y = static_cast<int16>(static_cast<int32>(cell.Y) + dy);
            if (adj.X >= 0 && adj.X < 512 && adj.Y >= 0 && adj.Y < 512) {
                MapClass::Instance->MarkCellOccupied(adj, occupy);
            }
        }
    }
}

// ============================================================================
// Limbo - Vehicle limbo state.
// ============================================================================

void DriveLocomotionClass::Limbo()
{
    Mark_All_Occupation_Bits(MarkType::Down);
    IsDriving = false;
    IsMoving = false;
    IsRotating = false;
    IsRocking = false;
    SpeedAccum = 0;
}

// ============================================================================
// Force_New_Slope - Vehicle slope handling with speed penalty.
// ============================================================================

void DriveLocomotionClass::Force_New_Slope(int32 ramp)
{
    PreviousRamp = CurrentRamp;
    CurrentRamp = static_cast<uint32>(ramp);

    if (PreviousRamp != CurrentRamp) {
        if (ramp > 0) {
            SlopeTimer.SetRate(60);
            MovementSpeed = 1.0 - static_cast<double>(ramp) * 0.08;
            if (MovementSpeed < 0.3) {
                MovementSpeed = 0.3;
            }
        } else {
            MovementSpeed = 1.0;
        }
    }
}

// ============================================================================
// Movement_AI - Vehicle movement AI with terrain and slope awareness.
// ============================================================================

void DriveLocomotionClass::Movement_AI()
{
    if (!IsDriving) {
        return;
    }

    // 地表速度（水面非两栖会直接停车）。
    ApplyTerrainSpeed();
    if (!IsDriving) {
        return;
    }

    // 坡道降速。
    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    int32 slope = MapClass::Instance->GetCellSlope(currentCell);
    if (slope > 0) {
        SpeedPercentage *= 1.0f - static_cast<float>(slope) * 0.05f;
        if (SpeedPercentage < 0.2f) {
            SpeedPercentage = 0.2f;
        }
    }

    // 断桥处停车（后续转摇车分支）。
    if (Is_Moving_On_Bridge() && Is_Bridge_Destroyed()) {
        IsDriving = false;
        IsMoving = false;
    }
}

// ============================================================================
// Get_Status - Vehicle movement status code.
// ============================================================================

int32 DriveLocomotionClass::Get_Status() const
{
    if (IsRocking) {
        return static_cast<int32>(Sequence::Tumble);
    }
    if (IsRotating) {
        return static_cast<int32>(Sequence::Ready);
    }
    if (IsDriving) {
        return static_cast<int32>(Sequence::Walk);
    }
    return static_cast<int32>(Sequence::Ready);
}

// ============================================================================
// Is_Really_Moving_Now - Vehicle is moving if actually driving.
// ============================================================================

bool DriveLocomotionClass::Is_Really_Moving_Now() const
{
    return IsDriving && !IsRotating && !IsRocking;
}

// ============================================================================
// Can_Fire - Vehicle can fire unless rotating (turret lock).
// ============================================================================

FireError DriveLocomotionClass::Can_Fire() const
{
    if (IsTurretLockedDown > 0) {
        return FireError::ROT;
    }
    if (IsRocking) {
        return FireError::Movement;
    }
    return FireError::OK;
}

// ============================================================================
// Force_Track - Forces the vehicle onto a specific track.
// ============================================================================

void DriveLocomotionClass::Force_Track(int32 track, CoordStruct coord)
{
    TrackNumber = static_cast<uint32>(track);
    TrackIndex = track;
    IsOnShortTrack = (track < 0);

    Dest = coord;
    Destination = coord;
    IsDriving = true;
    IsMoving = true;
    SpeedAccum = 0;
}

// ============================================================================
// Get_Track_Number - Returns the current track number.
// ============================================================================

int32 DriveLocomotionClass::Get_Track_Number() const
{
    return static_cast<int32>(TrackNumber);
}

// ============================================================================
// Get_Track_Index - Returns the current track index.
// ============================================================================

int32 DriveLocomotionClass::Get_Track_Index() const
{
    return TrackIndex;
}

// ============================================================================
// Lock / Unlock - Vehicle lock state management.
// ============================================================================

void DriveLocomotionClass::Lock()
{
    IsLocked = true;
    IsDriving = false;
    IsMoving = false;
}

void DriveLocomotionClass::Unlock()
{
    IsLocked = false;
}

// ============================================================================
// Tilt_Pitch_AI - Vehicle pitch adjustment for slopes.
// ============================================================================

void DriveLocomotionClass::Tilt_Pitch_AI()
{
    int32 slope = MapClass::Instance->GetCellSlope(CoordMath::CoordToCell(CurrentCoord));
    if (slope == 0) {
        return;
    }

    if (Owner) {
        int32 pitch = slope * 3;
        if (pitch > 20) {
            pitch = 20;
        }
        if (pitch < -20) {
            pitch = -20;
        }
        Owner->SetPitch(pitch);
    }
}