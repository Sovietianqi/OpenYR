#pragma once

#include "LocomotionClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/Facing.h"

class FlyLocomotionClass : public LocomotionClass {
public:
    static constexpr int32 LocoID = LocomotionClass::CLSIDs::Fly;

    virtual ~FlyLocomotionClass();

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

    void Fly();
    void Land();
    void TakeOff();
    void SetFlightLevel(int32 level);
    bool CanLand() const;
    CoordStruct GetLandingPos() const;

    bool ProcessTakeoff();
    bool ProcessLanding();
    bool ProcessElevation();
    bool ProcessFlight();
    void UpdateShadowProjection();
    void DockAtTarget();
    void SetDockTarget(TechnoClass* target);

    int32 GetAltitude() const;
    bool Is_In_Air() const;
    CoordStruct GetShadowPos() const;
    bool Is_To_Have_Shadow() const;
    Move Can_Enter_Cell(CellStruct cell);
    int32 Get_Status() const;
    FireError Can_Fire() const;
    void SetTargetAltitude(int32 targetAlt);
    bool Is_Really_Moving_Now() const;

    FlyLocomotionClass();

protected:
    explicit FlyLocomotionClass(noinit_t) noexcept : LocomotionClass(noinit) {}

public:
    bool AirportBound;
    CoordStruct MovingDestination;
    CoordStruct XYZ2;
    bool HasMoveOrder;
    int32 FlightLevel;
    double TargetSpeed;
    double CurrentSpeed;
    bool IsTakingOff;
    bool IsLanding;
    bool WasLanding;
    bool unknown_bool_53;
    uint32 unknown_54;
    uint32 unknown_58;
    bool IsElevating;
    bool unknown_bool_5D;
    bool unknown_bool_5E;
    bool unknown_bool_5F;
    int32 Altitude;
    int32 TargetAltitude;
    TechnoClass* DockTarget;
    DirStruct LandingDirection;
    int32 TakeoffTimer;
    int32 LandingTimer;
};