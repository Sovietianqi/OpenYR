#pragma once

#include "LocomotionClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/Facing.h"

class JumpjetLocomotionClass : public LocomotionClass, public IPiggyback {
public:
    static constexpr int32 LocoID = LocomotionClass::CLSIDs::Jumpjet;

    enum State {
        Grounded = 0, Ascending = 1, Hovering = 2,
        Cruising = 3, Descending = 4, Crashing = 5, Unknown = 6
    };

    virtual ~JumpjetLocomotionClass();

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

    virtual HRESULT Begin_Piggyback(ILocomotion* pointer) override;
    virtual HRESULT End_Piggyback(ILocomotion** pointer) override;
    virtual bool Is_Ok_To_End() override { return !IsMoving; }
    virtual HRESULT Piggyback_CLSID(GUID* classid) override { return S_OK; }
    virtual bool Is_Piggybacking() override { return Piggybackee != nullptr; }

    void Hover();
    void Jump();
    void Crash();
    void DeployParachute();
    int32 Get_Status() const;
    FireError Can_Fire() const;
    bool Is_Really_Moving_Now() const;
    int32 GetAltitude() const;

    bool ProcessGrounded();
    bool ProcessAscending();
    bool ProcessCruising();
    bool ProcessHovering();
    bool ProcessDescending();
    bool ProcessCrash();
    void FindLandingZone();
    bool IsValidLandingCell() const;

    JumpjetLocomotionClass();

protected:
    explicit JumpjetLocomotionClass(noinit_t) noexcept : LocomotionClass(noinit) {}

public:
    int32 TurnRate;
    int32 JumpSpeed;
    float Climb;
    float CruiseHeight;
    float HoverHeight;
    State CurrentState;
    bool IsCrashing;
    DirStruct JumpDirection;
    CoordStruct MovingDestination;
    int32 Altitude;
    int32 TargetAltitude;
    ILocomotion* Piggybackee;
    int32 CrashTimer;
    CDTimerClass StateTimer;
};