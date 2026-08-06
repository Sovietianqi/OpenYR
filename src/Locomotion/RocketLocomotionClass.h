#pragma once

#include "LocomotionClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/Timer.h"

class RocketLocomotionClass : public LocomotionClass {
public:
    static constexpr int32 LocoID = LocomotionClass::CLSIDs::Rocket;

    virtual ~RocketLocomotionClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual int32 Size() override;

    virtual Layer In_Which_Layer() override { return Layer::Air; }
    virtual bool Is_Moving() override;
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
    void ProcessAcceleration();
    void ProcessDeceleration();
    void ProcessCruise();
    void UpdateTrajectory();
    void GetTargetTracking(CoordStruct targetPos);
    void CalculateBallisticTrajectory(CoordStruct target);
    bool CheckImpact();
    void SetThrust(double thrust, double maxSpeed, double accel);
    double GetCurrentSpeed() const;
    float GetPitch() const;

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