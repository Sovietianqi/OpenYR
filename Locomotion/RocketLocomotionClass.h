#pragma once

#include "LocomotionClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/Timer.h"

struct RocketStruct;

class RocketLocomotionClass : public LocomotionClass {
public:
    static constexpr int32 LocoID = LocomotionClass::CLSIDs::Rocket;

    virtual ~RocketLocomotionClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual int32 Size() override;

    virtual Layer In_Which_Layer() override { return Layer::Air; }
    virtual bool Is_Moving() override;
    virtual bool Is_Moving_Now() override;
    virtual CoordStruct Destination() override;
    virtual bool Process() override;
    virtual void Move_To(CoordStruct to) override;
    virtual void Stop_Moving() override;
    virtual void Do_Turn(DirStruct coord) override;
    virtual void Mark_All_Occupation_Bits(MarkType mark) override;
    virtual void Limbo() override;
    virtual int32 Get_Status() const override;
    virtual FireError Can_Fire() const override;
    virtual bool Is_Really_Moving_Now() const override;

    void Accelerate();
    void Decelerate();
    void UpdateTrajectory();
    void GetTargetTracking(CoordStruct targetPos);
    void CalculateBallisticTrajectory(CoordStruct target);
    bool CheckImpact();
    // asm: Process 开头的取规则段（按载体类型在 V3Rocket/DMisl/CMisl 三者中
    //   选定一枚火箭参数）。
    const RocketStruct* GetMissionRocket() const;
    // asm: Process 的跳转表 off_663010 —— 任务态 0..6 的逐帧处理体。
    bool ProcessIdle();
    bool ProcessPauseRaise();
    bool ProcessIgnite();
    bool ProcessTrack();
    bool ProcessTerminal();
    void ProcessImpact();
    // asm: 速度上限夹取（状态 2/3 都把速度夹到载体类型上限，VehicleType+0x678）。
    void ClampSpeedToMax(const RocketStruct* rocket);
    // asm: 发射者的老兵判定（Veterancy_IsElite）决定用普通/精英弹头与伤害。
    void ResolveSpawnerElite();
    // asm: 巡航/追踪段按 (dz, 水平距离) 的 arctan 推俯仰（arctanfoo 段）。
    void UpdatePitchToTarget();
    // asm: RocketLocomotionClass_DoStuff   (asm:299) —— 抵达引爆。
    void Detonate();
    void SetThrust(double thrust, double maxSpeed, double accel);
    double GetCurrentSpeed() const;
    float GetPitch() const;
    // 根据游戏行为，可知火箭的每帧推进入口。
    void DoStuff();

    RocketLocomotionClass();

protected:
    explicit RocketLocomotionClass(noinit_t) noexcept : LocomotionClass(noinit) {}

public:
    CoordStruct MovingDestination;
    RateTimer MissionTimer;
    CDTimerClass TrailerTimer;
    int32 MissionState;
    uint32 unknown_44;
    double CurrentSpeed;
    bool unknown_bool_4C;
    bool SpawnerIsElite;
    float CurrentPitch;
    uint32 unknown_58;
    uint32 unknown_5C;
    double Thrust;
    double MaxSpeed;
    double Acceleration;
    bool IsAccelerating;
    bool IsDecelerating;
};