#include "JumpjetLocomotionClass.h"

#include <cmath>
#include <cstdlib>

#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Scenario/ScenarioClass.h"

// ============================================================================
// JumpjetLocomotionClass - Jumpjet infantry movement.
// Jump arc calculation, landing zone finding, in-air vs ground state
// transitions, parachute deployment if falling, and piggyback support.
// ============================================================================

JumpjetLocomotionClass::JumpjetLocomotionClass()
    : LocomotionClass()
    , TurnRate(8)
    , JumpSpeed(128)
    , Climb(0.5f)
    , CruiseHeight(300.0f)
    , HoverHeight(150.0f)
    , CurrentState(Grounded)
    , IsCrashing(false)
    , JumpDirection(0)
    , MovingDestination(0, 0, 0)
    , Altitude(0)
    , TargetAltitude(0)
    , Piggybackee(nullptr)
    , CrashTimer(0)
    , FlightAltitude(0.0)
    , TargetFlightAltitude(0.0)
    , ClimbRate(4.0f)
    , HoverPhase(0.0)
    , MaxAltitude(300)
{
    Speed = 128;
}

JumpjetLocomotionClass::~JumpjetLocomotionClass()
{
    if (Piggybackee) {
        Piggybackee = nullptr;
    }
}

HRESULT JumpjetLocomotionClass::GetClassID(CLSID* pClassID)
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

int32 JumpjetLocomotionClass::Size()
{
    return sizeof(JumpjetLocomotionClass);
}

// ============================================================================
// Begin_Piggyback / End_Piggyback - Piggyback system for jumpjet carrying.
// ============================================================================

HRESULT JumpjetLocomotionClass::Begin_Piggyback(ILocomotion* pointer)
{
    Piggybackee = pointer;
    return S_OK;
}

HRESULT JumpjetLocomotionClass::End_Piggyback(ILocomotion** pointer)
{
    if (pointer) {
        *pointer = Piggybackee;
    }
    Piggybackee = nullptr;
    return S_OK;
}

// ============================================================================
// Is_Moving - 依据任务状态判定。
// asm: JumpjetLocomotionClass_Is_Moving_Now   (asm:721586)
//   `[4Ch]!=0 && [4Ch]!=2`——状态既非 Grounded 也非 Hovering 即视为在动；
//   Hovering 是悬停待命，不计入移动。
// ============================================================================

bool JumpjetLocomotionClass::Is_Moving()
{
    return CurrentState != Grounded && CurrentState != Hovering;
}

// ============================================================================
// Destination - Returns the jumpjet's current destination.
// ============================================================================

CoordStruct JumpjetLocomotionClass::Destination()
{
    return MovingDestination;
}

// ============================================================================
// Move_To - Sets jumpjet destination. Initiates jump if grounded.
// ============================================================================

void JumpjetLocomotionClass::Move_To(CoordStruct to)
{
    MovingDestination = to;
    Dest = to;
    IsMoving = true;

    MaxAltitude = ResolveCruiseHeight();

    if (CurrentState == Grounded) {
        CurrentState = Ascending;
        TargetFlightAltitude = static_cast<double>(MaxAltitude);
    } else if (CurrentState == Descending) {
        // 下降途中改令 → 重新拉起转巡航。
        CurrentState = Cruising;
        TargetFlightAltitude = static_cast<double>(MaxAltitude);
    } else if (CurrentState == Hovering) {
        CurrentState = Cruising;
    }

    DirStruct facing = CoordMath::DirectionTo(CurrentCoord, to);
    JumpDirection = facing;
}

// ============================================================================
// Stop_Moving - 收令：空中则转入下降，落点由 FindLandingZone 选定。
// asm: Process 里状态机收到 Destination=0 时把目标高度压到 0 并转下降。
// ============================================================================

void JumpjetLocomotionClass::Stop_Moving()
{
    IsMoving = false;

    if (CurrentState == Cruising || CurrentState == Hovering) {
        FindLandingZone();
        CurrentState = Descending;
        TargetFlightAltitude = 0.0;
        TargetAltitude = 0;
    }
}

// ============================================================================
// Do_Turn - Jumpjet turning. Handles rotation in air.
// ============================================================================

void JumpjetLocomotionClass::Do_Turn(DirStruct coord)
{
    if (Owner) {
        DirStruct currentFacing = Owner->GetFacing();
        int32 diff = static_cast<int32>(coord.Value) - static_cast<int32>(currentFacing.Value);

        if (diff == 0) {
            return;
        }

        int32 step = 0;
        if (diff < -128 || (diff > 0 && diff <= 128)) {
            step = (diff > 0) ? TurnRate : -TurnRate;
        } else {
            step = (diff > 0) ? -TurnRate : TurnRate;
        }

        int32 newVal = static_cast<int32>(currentFacing.Value) + step;
        if (newVal < 0) {
            newVal += 256;
        }
        if (newVal >= 256) {
            newVal -= 256;
        }

        if (std::abs(newVal - static_cast<int32>(coord.Value)) <= TurnRate) {
            Owner->SetFacing(coord);
        } else {
            Owner->SetFacing(DirStruct(static_cast<uint8>(newVal)));
        }
    }
}

// ============================================================================
// TickCommon - 每帧公共段（所有状态前先跑）。
// asm: sub_54D0F0   (asm 主库 721618)
//   1) 高度插值：目标高度 78h > 当前 70h 时按 30h 爬升率上抬，并以 20h 为上限；
//      目标 < 当前时按 30h*1.5 下沉，落到 0 为止。
//   2) 悬停摆动相位 88h 按 34h 阻尼推进（非悬停/巡航时清零）。
//   3) 离子风暴：飞行中的跳跃喷气遇风暴激活即坠毁（Crash）。
// ============================================================================

void JumpjetLocomotionClass::TickCommon()
{
    // --- 1) 高度插值 ---
    if (TargetFlightAltitude > FlightAltitude) {
        FlightAltitude += static_cast<double>(ClimbRate);
        if (FlightAltitude > static_cast<double>(MaxAltitude)) {
            FlightAltitude = static_cast<double>(MaxAltitude);
        }
    } else if (TargetFlightAltitude < FlightAltitude) {
        FlightAltitude -= static_cast<double>(ClimbRate) * 1.5;
        if (FlightAltitude < 0.0) {
            FlightAltitude = 0.0;
        }
    }

    Altitude = static_cast<int32>(FlightAltitude);
    CurrentCoord.Z = Altitude;

    // --- 3) 悬停摆动相位 ---
    const bool suspended = (CurrentState == Hovering || CurrentState == Cruising);
    if (suspended && !IsMoving) {
        // asm:dbl_7ECE60 / dbl_7E3CC0 是两个衰减常数，此处以固定步进近似同一相位推进。
        HoverPhase += 0.05;
        if (HoverPhase > 6.283185307179586) {
            HoverPhase -= 6.283185307179586;
        }
    } else {
        HoverPhase = 0.0;
    }

    // --- 4) 离子风暴坠毁 ---
    if (Owner && CurrentState != Grounded && CurrentState != Crashing) {
        if (ScenarioClass::Instance->SpecialFlags.IonStorms()) {
            Crash();
        }
    }
}

// ============================================================================
// Process - 跳跃喷气每帧驱动入口。
// asm: JumpjetLocomotionClass_Process   (asm:105)
//   先跑 TickCommon，再按 [4Ch]（任务状态）做 7 路分派：
//     0 → 落地待命  1 → 爬升  2 → 悬停  3 → 巡航  4 → 下降  5 → 触地  6 → 收尾
// ============================================================================

bool JumpjetLocomotionClass::Process()
{
    if (!Owner) {
        return false;
    }

    TickCommon();

    if (IsCrashing) {
        return ProcessCrash();
    }

    switch (CurrentState) {
        case Grounded:
            return ProcessLanded();
        case Ascending:
            return ProcessClimb();
        case Hovering:
            return ProcessHover();
        case Cruising:
            return ProcessCruise();
        case Descending:
            return ProcessDescent();
        case Crashing:
            return ProcessCrash();
        default:
            break;
    }

    return IsMoving || CurrentState != Grounded;
}

// ============================================================================
// ResolveCruiseHeight - 巡航高度（上限）。
// asm: sub_54D0F0 读 [esi+20h]；该值在 Process 起始由机型的高度字段算出
//   （Rules->JumpJetHeight / 类型默认），缺省回落到本类的 CruiseHeight。
// ============================================================================

int32 JumpjetLocomotionClass::ResolveCruiseHeight() const
{
    int32 height = MaxAltitude;
    if (height <= 0) {
        height = static_cast<int32>(CruiseHeight);
    }
    if (height <= 0) {
        height = 300;
    }
    return height;
}

// ============================================================================
// FaceDestination - 把"朝往点方向"交给载体的 FacingClass。
// asm: sub_54BA30 的 Desired_Facing256 + FacingClass::Set_Desired。
// ============================================================================

bool JumpjetLocomotionClass::FaceDestination()
{
    if (!Owner) {
        return false;
    }

    CoordStruct target = !(MovingDestination == CoordStruct(0, 0, 0))
        ? MovingDestination : Dest;
    if (target == CoordStruct(0, 0, 0)) {
        return false;
    }

    DirStruct facing = CoordMath::DirectionTo(CurrentCoord, target);
    JumpDirection = facing;
    Do_Turn(facing);
    return true;
}

// ============================================================================
// ProcessLanded - case 0：落地/待命。
// asm: sub_54B980   (asm 主库 718999)
//   若已有移动指令 → 把朝向对向目的地、清空坠落相关计数、把巡航高度上限写入
//   本机，随后转入爬升态；离子风暴激活时不起飞。否则保持 Ready 姿态。
// ============================================================================

bool JumpjetLocomotionClass::ProcessLanded()
{
    FlightAltitude = 0.0;
    Altitude = 0;
    CurrentCoord.Z = 0;

    const bool hasOrder = IsMoving
        || !(MovingDestination == CoordStruct(0, 0, 0));

    if (hasOrder && Owner) {
        MaxAltitude = ResolveCruiseHeight();

        if (ScenarioClass::Instance->SpecialFlags.IonStorms()) {
            if (Owner) {
                Owner->SetCoords(CurrentCoord);
                Owner->SetSequence(Sequence::Ready);
            }
            return false;
        }

        FaceDestination();
        CrashTimer = 0;
        HoverPhase = 0.0;
        CurrentState = Ascending;
        TargetFlightAltitude = static_cast<double>(MaxAltitude);

        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Fly);
        return true;
    }

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Ready);
    }
    return false;
}

// ============================================================================
// ProcessClimb - case 1：爬升到巡航高度。
// asm: sub_54BA30   (asm 主库 719081)
//   高度未到上限时朝目的地转向并持续爬升；到达后若目的地格净空则转入巡航，
//   否则先在原地悬停（保持高度），等待下一帧重试。
// ============================================================================

bool JumpjetLocomotionClass::ProcessClimb()
{
    if (!Owner) {
        return false;
    }

    TargetFlightAltitude = static_cast<double>(ResolveCruiseHeight());
    FaceDestination();

    const bool atCruise = (Altitude >= ResolveCruiseHeight());
    if (atCruise) {
        // 到顶：目的地格净空则转巡航，否则悬停原地。
        const CoordStruct head = !(MovingDestination == CoordStruct(0, 0, 0))
            ? MovingDestination : Dest;
        if (LocationClear(head)) {
            CurrentState = Cruising;
        } else {
            CurrentState = Hovering;
        }
    }

    Owner->SetCoords(CurrentCoord);
    Owner->SetSequence(Sequence::Fly);
    return true;
}

// ============================================================================
// ProcessCruise - case 3：巡航平飞。
// asm: jumpjet2   (asm 主库 719714)
//   保持巡航高度，朝朝往点逐帧推进；到位即转悬停。推进速度取载体现行速度
//   与 SpeedPercentage 的折合。
// ============================================================================

bool JumpjetLocomotionClass::ProcessCruise()
{
    if (!Owner) {
        return false;
    }

    TargetFlightAltitude = static_cast<double>(ResolveCruiseHeight());

    if (!IsMoving && (MovingDestination == CoordStruct(0, 0, 0))) {
        CurrentState = Hovering;
        return true;
    }

    FaceDestination();

    const int32 baseSpeed = Owner->GetCurrentSpeed() > 0
        ? Owner->GetCurrentSpeed() : JumpSpeed;
    int32 step = static_cast<int32>(static_cast<float>(baseSpeed) * SpeedPercentage);
    if (step <= 0) {
        step = 1;
    }

    const int32 dist = CurrentCoord.DistanceFrom(MovingDestination);
    if (dist <= step) {
        CurrentCoord = MovingDestination;
        CurrentCoord.Z = Altitude;
        CurrentState = Hovering;
        IsMoving = false;
    } else {
        CurrentCoord = VectorMath::MoveTowards(CurrentCoord, MovingDestination, step);
        CurrentCoord.Z = Altitude;
    }

    Owner->SetCoords(CurrentCoord);
    Owner->SetSequence(Sequence::Fly);
    return true;
}

// ============================================================================
// ProcessHover - case 2：原地悬停保持巡航高度。
// asm: sub_54BD30   (asm 主库 719414)
//   悬停期间插值把高度拉回巡航上限；一旦重新收到移动指令即转巡航。
// ============================================================================

bool JumpjetLocomotionClass::ProcessHover()
{
    if (!Owner) {
        return false;
    }

    if (IsMoving && !(MovingDestination == CoordStruct(0, 0, 0))) {
        CurrentState = Cruising;
        return true;
    }

    TargetFlightAltitude = static_cast<double>(ResolveCruiseHeight());

    Owner->SetCoords(CurrentCoord);
    Owner->SetSequence(Sequence::IdleFly);
    return true;
}

// ============================================================================
// ProcessDescent - case 4：下降进场。
// asm: sub_54C550   (asm 主库 720281)
//   一边朝落点水平推进，一边把目标高度压到 0；触地后转触地处理。
// ============================================================================

bool JumpjetLocomotionClass::ProcessDescent()
{
    if (!Owner) {
        return false;
    }

    TargetFlightAltitude = 0.0;

    CoordStruct target = !(MovingDestination == CoordStruct(0, 0, 0))
        ? MovingDestination : CurrentCoord;

    if (!(target == CurrentCoord)) {
        FaceDestination();
        const int32 baseSpeed = Owner->GetCurrentSpeed() > 0
            ? Owner->GetCurrentSpeed() : JumpSpeed;
        const int32 step = static_cast<int32>(
            static_cast<float>(baseSpeed) * SpeedPercentage);
        if (step > 0) {
            CoordStruct horiz = CurrentCoord;
            horiz.Z = 0;
            CoordStruct tgt = target;
            tgt.Z = 0;
            const int32 hdist = horiz.DistanceFrom(tgt);
            if (hdist > 0) {
                int32 use = (hdist < step) ? hdist : step;
                CurrentCoord = VectorMath::MoveTowards(CurrentCoord, target, use);
            }
        }
    }

    if (Altitude <= 0) {
        CurrentCoord.Z = 0;
        return ProcessTouchdown();
    }

    CurrentCoord.Z = Altitude;
    Owner->SetCoords(CurrentCoord);
    Owner->SetSequence(Sequence::Fly);
    return true;
}

// ============================================================================
// ProcessTouchdown - case 5：触地落位。
// asm: sub_54CA90   (asm 主库 720843)
//   落点不可用时转入坠毁；否则落位到地面、切 Ready 姿态、恢复占用位。
// ============================================================================

bool JumpjetLocomotionClass::ProcessTouchdown()
{
    if (!Owner) {
        return false;
    }

    if (!IsValidLandingCell()) {
        Crash();
        return true;
    }

    Altitude = 0;
    FlightAltitude = 0.0;
    CurrentCoord.Z = 0;
    CurrentState = Grounded;
    IsMoving = false;
    MovingDestination = CoordStruct(0, 0, 0);
    Dest = CoordStruct(0, 0, 0);
    TargetFlightAltitude = 0.0;

    Mark_All_Occupation_Bits(MarkType::Up);
    Owner->SetCoords(CurrentCoord);
    Owner->SetSequence(Sequence::Ready);
    return false;
}

// ============================================================================
// ProcessCrash - 坠毁：每帧下沉，计时结束后趴地。
// asm: Process 崩溃分支（降落/坠毁姿态切换 Prone/Tumble）。
// ============================================================================

bool JumpjetLocomotionClass::ProcessCrash()
{
    --CrashTimer;

    Altitude -= 30;
    if (Altitude < 0) {
        Altitude = 0;
    }
    FlightAltitude = static_cast<double>(Altitude);
    CurrentCoord.Z = Altitude;

    if (CrashTimer <= 0) {
        IsCrashing = false;
        CurrentState = Grounded;
        IsMoving = false;
        Altitude = 0;
        FlightAltitude = 0.0;
        CurrentCoord.Z = 0;

        if (Owner) {
            Owner->SetCoords(CurrentCoord);
            Owner->SetSequence(Sequence::Prone);
        }

        return false;
    }

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Tumble);
    }

    return true;
}

// ============================================================================
// IsValidLandingCell - Checks if the current cell is valid for landing.
// ============================================================================

bool JumpjetLocomotionClass::IsValidLandingCell() const
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);

    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }

    LandType land = MapClass::Instance->GetLandType(cell);
    if (land == LandType::Water || land == LandType::Wall || land == LandType::Rock) {
        return false;
    }

    return true;
}

// ============================================================================
// FindLandingZone - Finds the closest valid landing zone.
// ============================================================================

void JumpjetLocomotionClass::FindLandingZone()
{
    if (IsValidLandingCell()) {
        return;
    }

    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    for (int32 radius = 1; radius <= 16; ++radius) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            for (int32 dy = -radius; dy <= radius; ++dy) {
                if (std::abs(dx) != radius && std::abs(dy) != radius) {
                    continue;
                }
                CellStruct cs;
                cs.X = static_cast<int16>(static_cast<int32>(currentCell.X) + dx);
                cs.Y = static_cast<int16>(static_cast<int32>(currentCell.Y) + dy);

                if (cs.X < 0 || cs.X >= 512 || cs.Y < 0 || cs.Y >= 512) {
                    continue;
                }

                LandType land = MapClass::Instance->GetLandType(cs);
                if (land != LandType::Water && land != LandType::Wall && land != LandType::Rock) {
                    MovingDestination = CoordMath::CellToCoord(cs);
                    CurrentCoord = MovingDestination;
                    return;
                }
            }
        }
    }
}

// ============================================================================
// Mark_All_Occupation_Bits - Jumpjet occupation marking.
// ============================================================================

void JumpjetLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    if (CurrentState == Grounded || CurrentState == Descending) {
        CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
        bool occupy = (mark == MarkType::Up);
        MapClass::Instance->MarkCellOccupied(cell, occupy);
    }
}

// ============================================================================
// Limbo - Jumpjet limbo state.
// ============================================================================

void JumpjetLocomotionClass::Limbo()
{
    IsMoving = false;
    CurrentState = Grounded;
    Altitude = 0;
    IsCrashing = false;
    TargetAltitude = 0;
}

// ============================================================================
// Hover - Puts the jumpjet into hovering state.
// ============================================================================

void JumpjetLocomotionClass::Hover()
{
    if (CurrentState == Grounded) {
        Jump();
    }
    if (CurrentState == Cruising) {
        CurrentState = Hovering;
    }
}

// ============================================================================
// Jump - Initiates the jump sequence from ground to air.
// ============================================================================

void JumpjetLocomotionClass::Jump()
{
    if (CurrentState != Grounded) {
        return;
    }

    MaxAltitude = ResolveCruiseHeight();
    CurrentState = Ascending;
    TargetFlightAltitude = static_cast<double>(MaxAltitude);
    TargetAltitude = MaxAltitude;
    Altitude = 0;
    FlightAltitude = 0.0;
}

// ============================================================================
// Crash - Forces the jumpjet into crash state.
// ============================================================================

void JumpjetLocomotionClass::Crash()
{
    IsCrashing = true;
    CrashTimer = 60;
    CurrentState = Crashing;
}

// ============================================================================
// DeployParachute - Deploys parachute if falling (safety feature).
// ============================================================================

void JumpjetLocomotionClass::DeployParachute()
{
    if (CurrentState == Descending || CurrentState == Crashing) {
        Climb = 0.25f;
        if (Owner) {
            Owner->SetSequence(Sequence::Paradrop);
        }
    }
}

// ============================================================================
// Get_Status - Jumpjet movement status code.
// ============================================================================

int32 JumpjetLocomotionClass::Get_Status() const
{
    switch (CurrentState) {
        case Ascending:
        case Cruising:
            return static_cast<int32>(Sequence::Fly);
        case Hovering:
            return static_cast<int32>(Sequence::IdleFly);
        case Descending:
            return static_cast<int32>(Sequence::Fly);
        case Crashing:
            return static_cast<int32>(Sequence::Tumble);
        case Grounded:
        default:
            return static_cast<int32>(Sequence::Ready);
    }
}

// ============================================================================
// Can_Fire - Jumpjet can fire in most states except crashing.
// ============================================================================

FireError JumpjetLocomotionClass::Can_Fire() const
{
    if (IsCrashing || CurrentState == Crashing) {
        return FireError::Movement;
    }
    return FireError::OK;
}

// ============================================================================
// Is_Really_Moving_Now - Jumpjet is moving if airborne.
// ============================================================================

bool JumpjetLocomotionClass::Is_Really_Moving_Now() const
{
    return CurrentState != Grounded && !IsCrashing;
}

// ============================================================================
// GetAltitude - Returns the current altitude of the jumpjet.
// ============================================================================

int32 JumpjetLocomotionClass::GetAltitude() const
{
    return Altitude;
}
bool JumpjetLocomotionClass::LocationClear(const CoordStruct& coord)
{
    // 根据游戏行为，可知跳跃喷气移位在挪位前要确认目标点上空没有其他
    // 载具占用：以目标格的占用旗标为依据。
    CellClass* pCell = TheMap->GetCellAt(coord.X >> 8, coord.Y >> 8);
    return pCell && !pCell->IsOccupied();
}
