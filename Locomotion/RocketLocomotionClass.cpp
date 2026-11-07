#include "RocketLocomotionClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/ObjectClass.h"
#include "../Abstract/AircraftTypeClass.h"
#include "../Particles/ParticleClass.h"
#include "../Rules/RulesClass.h"
#include "../Map/MapClass.h"
#include "../Math/Timer.h"

#include <cmath>
#include <cstdlib>

// ============================================================================
// RocketLocomotionClass - V3 / 无畏 / 潜射导弹的火箭移位器。
//
// 权威来源：
//   asm: RocketLocomotionClass_CTOR                       (asm:22)
//   asm: RocketLocomotionClass_ILocomotion_IsMoving       (asm:83)
//   asm: RocketLocomotionClass_ILocomotion_IsMovingNow    (asm:128)
//   asm: RocketLocomotionClass_ILocomotion_Destination    (asm:162)
//   asm: RocketLocomotionClass_ILocomotion_Process        (asm:210)  任务状态机
//   asm: RocketLocomotionClass_DoStuff                    (asm:299)  抵达引爆
//   asm: RocketLocomotionClass_ILocomotion_MoveTo         (asm:564)
//   asm: RocketLocomotionClass_ILocomotion_StopMoving     (asm:668)  空操作
//   asm: RocketLocomotionClass_ILocomotion_InWhichLayer   (asm:789)  恒 3(Air)
//   asm: RocketLocomotionClass_GetSize                    (asm:958)  0x60
//
// 任务状态（[+0x3C]，Process 的跳转表 off_663010 六个入口）：
//   0  待命
//   1  暂停 / 竖直抬升（PauseFrames 内不位移，只把俯仰推到初始角）
//   2  加速脱离（Acceleration 累积速度，沿初始俯仰爬升）
//   3  巡航追踪（朝目标转向，直线飞行）
//   4  末端俯冲（逼近目标时压低俯仰）
//   5  引爆（DoStuff）
//
// Is_Moving_Now 只认状态 3..5（asm:128 的 `cmp 3 / cmp 5`）。
// ============================================================================

namespace {
constexpr double kPi = 3.14159265358979323846;

// 三种火箭参数取哪一枚，看载体类型的 ID。
inline bool SameType(const ObjectTypeClass* a, const AircraftTypeClass* b)
{
    return a != nullptr && b != nullptr && a == static_cast<const ObjectTypeClass*>(b);
}
} // namespace

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
    // asm: RocketLocomotionClass_GetSize   (asm:958) 恒 0x60。
    return 0x60;
}

// ============================================================================
// GetMissionRocket - 按载体类型在三种火箭参数里选定一枚。
// asm: RocketLocomotionClass_ILocomotion_Process 开头 (asm:252..278)
//   载体类型 == Rules->V3RocketType → V3Rocket；== DMislType → DMisl；
//   否则 → CMisl。
// ============================================================================

const RocketStruct* RocketLocomotionClass::GetMissionRocket() const
{
    const ObjectTypeClass* t = Owner ? Owner->GetType() : nullptr;
    if (SameType(t, RulesClass::Instance->V3Rocket.Type)) {
        return &RulesClass::Instance->V3Rocket;
    }
    if (SameType(t, RulesClass::Instance->DMisl.Type)) {
        return &RulesClass::Instance->DMisl;
    }
    return &RulesClass::Instance->CMisl;
}

// ============================================================================
// Is_Moving - 目的地非零即在飞。
// asm: RocketLocomotionClass_ILocomotion_IsMoving   (asm:83)
// ============================================================================

bool RocketLocomotionClass::Is_Moving()
{
    return !(MovingDestination == CoordStruct(0, 0, 0));
}

// ============================================================================
// Is_Moving_Now - 只有状态 3..5 算"此刻真的在飞"。
// asm: RocketLocomotionClass_ILocomotion_IsMovingNow   (asm:128)
// ============================================================================

bool RocketLocomotionClass::Is_Moving_Now()
{
    return MissionState >= 3 && MissionState <= 5;
}

bool RocketLocomotionClass::Is_Really_Moving_Now() const
{
    return MissionState >= 3;
}

CoordStruct RocketLocomotionClass::Destination()
{
    return MovingDestination;
}

// ============================================================================
// Move_To - 登记目标点并据火箭参数起任务。
// asm: RocketLocomotionClass_ILocomotion_MoveTo   (asm:564)
//   若目的地尚为零：按火箭参数决定 MissionState（PauseFrames 非零 → 1，
//   否则 → 2），起 MissionTimer（长度取 TiltFrames），把初始俯仰置为
//   PitchInitial * PI/2，再写入目标点。
// ============================================================================

void RocketLocomotionClass::Move_To(CoordStruct to)
{
    // asm: RocketLocomotionClass_ILocomotion_MoveTo   (asm:564)
    //   目的地点入库；若尚未起飞（MissionState 仍为 0）则把任务态置 0，
    //   由 ProcessIdle 依火箭参数（PauseFrames）决定进入抬升或直接点火。
    MovingDestination = to;
    Dest = to;
    IsMoving = true;

    if (MissionState == 0) {
        const RocketStruct* rocket = GetMissionRocket();
        if (rocket) {
            CurrentPitch = rocket->PitchInitial * static_cast<float>(kPi / 2.0);
            CurrentSpeed = 0.0;
            MaxSpeed = static_cast<double>(rocket->RaiseRate);
            Acceleration = static_cast<double>(rocket->Acceleration);
            MissionTimer.SetRate((rocket->PauseFrames != 0)
                ? rocket->PauseFrames : rocket->TiltFrames);
            MissionTimer.Reset();
        }
    }
}

void RocketLocomotionClass::Stop_Moving()
{
    // asm: RocketLocomotionClass_ILocomotion_StopMoving   (asm:668) 空操作。
}

void RocketLocomotionClass::Do_Turn(DirStruct coord)
{
    // 火箭在巡航阶段按 TurnRate 限速转向（asm 的 Desired_Facing256 + 限速）。
    if (!Owner) {
        return;
    }

    DirStruct currentFacing = Owner->GetFacing();
    const int32 diff = Facing::Difference(currentFacing, coord);
    if (diff == 0) {
        return;
    }

    int32 turnRate = 3;
    const RocketStruct* rocket = GetMissionRocket();
    if (rocket && rocket->TurnRate > 0.0f) {
        turnRate = static_cast<int32>(rocket->TurnRate);
        if (turnRate < 1) {
            turnRate = 1;
        }
    }

    int32 step = (diff > 0) ? turnRate : -turnRate;
    if (std::abs(diff) <= turnRate) {
        Owner->SetFacing(coord);
    } else {
        Owner->SetFacing(Facing::Rotate(currentFacing, (step > 0) ? 1 : -1));
    }
}

// ============================================================================
// ResolveSpawnerElite - 判定发射者是否精英，决定弹头/伤害档次。
// asm: Process 状态 0/5 开头 `Veterancy_IsElite` 段。
// ============================================================================

void RocketLocomotionClass::ResolveSpawnerElite()
{
    SpawnerIsElite = (Owner != nullptr) && (Owner->GetVeterancy() >= 2);
}

// ============================================================================
// ClampSpeedToMax - 把当前速度夹到火箭参数给出的上限。
// asm: 状态 2/3 的 `cmp ... VehicleType+0x678` 段。
// ============================================================================

void RocketLocomotionClass::ClampSpeedToMax(const RocketStruct* rocket)
{
    double limit = MaxSpeed;
    if (limit <= 0.0 && rocket != nullptr && rocket->RaiseRate > 0) {
        limit = static_cast<double>(rocket->RaiseRate);
    }
    if (limit > 0.0 && CurrentSpeed > limit) {
        CurrentSpeed = limit;
    }
    if (CurrentSpeed < 0.0) {
        CurrentSpeed = 0.0;
    }
}

// ============================================================================
// UpdatePitchToTarget - 按弹体到目标的 (dz, 水平距离) 推俯仰角。
// asm: 状态 3 的 arctanfoo 段（dz / 水平距离 → atan → 转 16 位朝向）。
// ============================================================================

void RocketLocomotionClass::UpdatePitchToTarget()
{
    const int32 dx = MovingDestination.X - CurrentCoord.X;
    const int32 dy = MovingDestination.Y - CurrentCoord.Y;
    const int32 dz = MovingDestination.Z - CurrentCoord.Z;

    const double horiz = std::sqrt(static_cast<double>(dx) * dx
        + static_cast<double>(dy) * dy);
    if (horiz > 0.0001) {
        CurrentPitch = static_cast<float>(std::atan2(static_cast<double>(dz), horiz));
    } else {
        CurrentPitch = static_cast<float>(kPi / 2.0);
    }
}

// ============================================================================
// Process - 火箭任务状态机。
// asm: RocketLocomotionClass_ILocomotion_Process   (asm:210)
//   跳转表 off_663010（六个入口，index 0..6）：
//     0 待命初始化 / 1 竖直抬升 / 2 点火加速 / 3 巡航追踪 / 4 末端俯冲 /
//     5 引爆 / 6 收尾。
// ============================================================================

bool RocketLocomotionClass::Process()
{
    if (!Owner) {
        return false;
    }

    switch (MissionState) {
        case 0:  return ProcessIdle();
        case 1:  return ProcessPauseRaise();
        case 2:  return ProcessIgnite();
        case 3:  return ProcessTrack();
        case 4:  return ProcessTerminal();
        case 5:  ProcessImpact(); return false;
        default: return false;
    }
}

// ============================================================================
// ProcessIdle - 状态 0：待命初始化。
// asm: Process 跳转表 case 0（loc_662326）
//   清速度，判定发射者老兵，按火箭参数决定初俯仰；PauseFrames 非零先进抬升
//   态（1），否则直接点火（2）。
// ============================================================================

bool RocketLocomotionClass::ProcessIdle()
{
    const RocketStruct* rocket = GetMissionRocket();
    if (!rocket) {
        return false;
    }

    CurrentSpeed = 0.0;
    unknown_44 = 0;
    ResolveSpawnerElite();

    CurrentPitch = rocket->PitchInitial * static_cast<float>(kPi / 2.0);
    MaxSpeed = static_cast<double>(rocket->RaiseRate);
    Acceleration = static_cast<double>(rocket->Acceleration);
    Thrust = rocket->Acceleration;

    if (rocket->PauseFrames != 0) {
        MissionState = 1;
        MissionTimer.SetRate(rocket->PauseFrames);
    } else {
        MissionState = 2;
        MissionTimer.SetRate(rocket->TiltFrames);
    }
    MissionTimer.Reset();

    if (Owner) {
        Owner->SetPitch(static_cast<int32>(CurrentPitch * 180.0 / kPi));
        Owner->SetCoords(CurrentCoord);
    }
    return true;
}

// ============================================================================
// ProcessPauseRaise - 状态 1：竖直抬升（暂停段）。
// asm: Process 跳转表 case 1（loc_662720）
//   保持初俯仰抬升，计时到点后转入巡航追踪（3）。
// ============================================================================

bool RocketLocomotionClass::ProcessPauseRaise()
{
    const RocketStruct* rocket = GetMissionRocket();
    if (!rocket) {
        return false;
    }

    CurrentPitch = rocket->PitchInitial * static_cast<float>(kPi / 2.0);
    CurrentCoord.Z += (rocket->RaiseRate > 0) ? rocket->RaiseRate : 1;

    if (Owner) {
        Owner->SetPitch(static_cast<int32>(CurrentPitch * 180.0 / kPi));
        Owner->SetCoords(CurrentCoord);
    }

    if (MissionTimer.Update(1)) {
        MissionState = 3;
    }
    return true;
}

// ============================================================================
// ProcessIgnite - 状态 2：点火加速脱离。
// asm: Process 跳转表 case 2（loc_6628F0）
//   速度按 Acceleration 累加并夹到上限；高度按 RaiseRate 抬升，到达火箭参数
//   Altitude 后转巡航追踪（3）。
// ============================================================================

bool RocketLocomotionClass::ProcessIgnite()
{
    const RocketStruct* rocket = GetMissionRocket();
    if (!rocket) {
        return false;
    }

    CurrentSpeed += Acceleration;
    ClampSpeedToMax(rocket);

    const int32 raise = (rocket->RaiseRate > 0)
        ? rocket->RaiseRate
        : static_cast<int32>(CurrentSpeed);
    CurrentCoord.Z += (raise > 0) ? raise : 1;

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetPitch(static_cast<int32>(CurrentPitch * 180.0 / kPi));
    }

    if (rocket->Altitude > 0 && CurrentCoord.Z >= rocket->Altitude) {
        MissionState = 3;
    }
    return true;
}

// ============================================================================
// ProcessTrack - 状态 3：巡航追踪。
// asm: Process 跳转表 case 3（loc_6629C7）
//   速度抬到上限；朝目标转向并按俯仰直线逼近；到达目标或逼近阈值后转末端
//   俯冲（4）；若已抵达则直接引爆（5）。
// ============================================================================

bool RocketLocomotionClass::ProcessTrack()
{
    const RocketStruct* rocket = GetMissionRocket();
    if (!rocket) {
        return false;
    }

    CurrentSpeed += Acceleration;
    ClampSpeedToMax(rocket);

    const int32 dist = CurrentCoord.DistanceFrom(MovingDestination);

    // 到达目标 → 引爆。
    if (dist <= 0) {
        ProcessImpact();
        return false;
    }

    Do_Turn(CoordMath::DirectionTo(CurrentCoord, MovingDestination));
    UpdatePitchToTarget();

    int32 step = static_cast<int32>(CurrentSpeed);
    if (step < 1) {
        step = 1;
    }

    if (dist <= step) {
        CurrentCoord = MovingDestination;
        MissionState = 5;
        if (Owner) {
            Owner->SetCoords(CurrentCoord);
        }
        return true;
    }

    CurrentCoord = VectorMath::MoveTowards(CurrentCoord, MovingDestination, step);

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetPitch(static_cast<int32>(CurrentPitch * 180.0 / kPi));
    }

    // 逼近末端 → 转俯冲段。
    if (dist < step * 3) {
        MissionState = 4;
    }
    return true;
}

// ============================================================================
// ProcessTerminal - 状态 4：末端俯冲。
// asm: Process 跳转表 case 4（loc_662CAC）：直扑目标，抵达即引爆。
// ============================================================================

bool RocketLocomotionClass::ProcessTerminal()
{
    const int32 dist = CurrentCoord.DistanceFrom(MovingDestination);
    int32 step = static_cast<int32>(CurrentSpeed);
    if (step < 1) {
        step = 1;
    }

    if (dist <= step) {
        CurrentCoord = MovingDestination;
        MissionState = 5;
        if (Owner) {
            Owner->SetCoords(CurrentCoord);
        }
        return true;
    }

    CurrentCoord = VectorMath::MoveTowards(CurrentCoord, MovingDestination, step);
    if (Owner) {
        Owner->SetCoords(CurrentCoord);
    }
    return true;
}

// ============================================================================
// ProcessImpact - 状态 5：引爆收尾。
// asm: Process 跳转表 case 5（loc_6624C8）→ DoStuff（落点伤害 + 表现）。
// ============================================================================

void RocketLocomotionClass::ProcessImpact()
{
    ResolveSpawnerElite();
    Detonate();
    MissionState = 6;
}


// ============================================================================
// DoStuff - 抵达后的每帧收尾（尾部烟雾粒子）。
// asm: RocketLocomotionClass_DoStuff   (asm:299) 的粒子/表现段。
// ============================================================================

void RocketLocomotionClass::DoStuff()
{
    GetTargetTracking(MovingDestination);

    switch (MissionState) {
        case 0:  ProcessIdle();       break;
        case 1:  ProcessPauseRaise(); break;
        case 2:  ProcessIgnite();     break;
        case 3:  ProcessTrack();      break;
        case 4:  ProcessTerminal();   break;
        case 5:  ProcessImpact();     break;
        default: break;
    }

    if (LinkedTo && (FrameTimer::GetTime() % 3) == 0) {
        CoordStruct pos = LinkedTo->GetCoords();
        ParticleClass::CreateSmokeParticle(pos, CoordStruct(0, 0, 5));
    }
}

// ============================================================================
// Detonate - 抵达引爆：在落点结算伤害并移除烟雾。
// asm: RocketLocomotionClass_DoStuff   (asm:299)
//   按当前俯仰/朝向的 cos/sin 算出爆点，取火箭参数的 Damage / EliteDamage
//   作为伤害值，落到 MapClass::Damage_At_Cell 与 Flashbang 表现。
// ============================================================================

void RocketLocomotionClass::Detonate()
{
    const RocketStruct* rocket = GetMissionRocket();
    const int32 damage = SpawnerIsElite ? rocket->EliteDamage : rocket->Damage;

    // 爆点：由弹体朝向/俯仰推出（原版用 cos/sin 组合）。
    const double yaw = Owner
        ? (static_cast<double>(Owner->GetFacing().Value) * (2.0 * kPi / 256.0) - kPi / 2.0)
        : 0.0;
    const double pitch = static_cast<double>(CurrentPitch);
    const int32 bodyLen = rocket->BodyLength;

    const int32 ox = static_cast<int32>(std::cos(yaw) * std::cos(pitch) * bodyLen);
    const int32 oy = static_cast<int32>(std::sin(yaw) * std::cos(pitch) * bodyLen);
    const int32 oz = static_cast<int32>(std::sin(pitch) * bodyLen);

    CoordStruct impact(CurrentCoord.X + ox, CurrentCoord.Y + oy, CurrentCoord.Z + oz);

    if (damage > 0) {
        WarheadTypeClass* pWarhead = SpawnerIsElite
            ? RulesClass::Instance->V3EliteWarhead
            : RulesClass::Instance->V3Warhead;
        MapClass::Instance->Damage_At_Cell(impact, damage,
            Owner, pWarhead, false, nullptr);
    }

    MissionState = 6;
    IsMoving = false;
    IsAccelerating = false;
    IsDecelerating = false;
    CurrentSpeed = 0.0;
    MovingDestination = CoordStruct(0, 0, 0);

    if (Owner) {
        Owner->SetCoords(impact);
    }
}

void RocketLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    // 火箭全程处于 Air 层，不占用地面格；落地后由弹体逻辑自行销毁。
    (void)mark;
}

void RocketLocomotionClass::Limbo()
{
    IsMoving = false;
    IsAccelerating = false;
    IsDecelerating = false;
    CurrentSpeed = 0;
    CurrentPitch = 0.0f;
    MissionState = 0;
    MovingDestination = CoordStruct(0, 0, 0);
}

void RocketLocomotionClass::Accelerate()
{
    IsDecelerating = false;
    IsAccelerating = true;
}

void RocketLocomotionClass::Decelerate()
{
    IsAccelerating = false;
    IsDecelerating = true;
}

void RocketLocomotionClass::GetTargetTracking(CoordStruct targetPos)
{
    MovingDestination = targetPos;
}

void RocketLocomotionClass::CalculateBallisticTrajectory(CoordStruct target)
{
    const int32 dx = target.X - CurrentCoord.X;
    const int32 dy = target.Y - CurrentCoord.Y;
    const int32 dz = target.Z - CurrentCoord.Z;

    const int32 horizontalDist = static_cast<int32>(std::sqrt(
        static_cast<double>(dx) * dx + static_cast<double>(dy) * dy));

    const double launchAngle = std::atan2(static_cast<double>(dz),
        static_cast<double>(horizontalDist));

    CurrentPitch = static_cast<float>(launchAngle);

    if (Owner) {
        Owner->SetPitch(static_cast<int32>(CurrentPitch * 180.0 / kPi));
    }

    MovingDestination = target;
}

void RocketLocomotionClass::UpdateTrajectory()
{
    if (Owner) {
        Do_Turn(CoordMath::DirectionTo(CurrentCoord, MovingDestination));
    }
    CalculateBallisticTrajectory(MovingDestination);
}

bool RocketLocomotionClass::CheckImpact()
{
    const int32 dist = CurrentCoord.DistanceFrom(MovingDestination);
    if (dist <= 64) {
        MissionState = 5;
        return true;
    }
    return false;
}

void RocketLocomotionClass::SetThrust(double thrust, double maxSpeed, double accel)
{
    Thrust = thrust;
    MaxSpeed = maxSpeed;
    Acceleration = accel;
}

int32 RocketLocomotionClass::Get_Status() const
{
    // 火箭无自有 Get_Status，沿用基类槽位（恒 0）。
    return 0;
}

FireError RocketLocomotionClass::Can_Fire() const
{
    return FireError::OK;
}

double RocketLocomotionClass::GetCurrentSpeed() const
{
    return CurrentSpeed;
}

float RocketLocomotionClass::GetPitch() const
{
    return CurrentPitch;
}
