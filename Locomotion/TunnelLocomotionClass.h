#pragma once

#include "LocomotionClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/Timer.h"
#include "../Math/Facing.h"

class TunnelLocomotionClass : public LocomotionClass {
public:
    static constexpr int32 LocoID = LocomotionClass::CLSIDs::Tunnel;

    virtual ~TunnelLocomotionClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual int32 Size() override;

    virtual Layer In_Which_Layer() override { return IsUnderground ? Layer::Ground : Layer::Surface; }
    virtual bool Is_Moving() override;
    virtual CoordStruct Destination() override;
    virtual bool Process() override;
    virtual void Move_To(CoordStruct to) override;
    virtual void Stop_Moving() override;
    virtual void Do_Turn(DirStruct coord) override;
    virtual void Mark_All_Occupation_Bits(MarkType mark) override;
    virtual void Limbo() override;
    virtual bool Is_Surfacing() override { return IsExitingTunnel; }
    virtual bool Is_Surfacing() const override { return IsExitingTunnel; }
    virtual Move Can_Enter_Cell(CellStruct cell) override;
    virtual int32 Get_Status() const override;
    virtual FireError Can_Fire() const override;
    virtual bool Is_Really_Moving_Now() const override;

    void EnterTunnel(CoordStruct entrance);
    void ExitTunnel(CoordStruct exit);
    bool ProcessEntering();
    bool ProcessUnderground();
    bool ProcessExiting();
    void CheckAttackFromUnderground();
    bool IsValidEntrance(CoordStruct entrance) const;
    bool IsValidExit(CoordStruct exit) const;
    CoordStruct FindNearestTunnelExit(CoordStruct from) const;

    TunnelLocomotionClass();

protected:
    explicit TunnelLocomotionClass(noinit_t) noexcept : LocomotionClass(noinit) {}

public:
    bool IsUnderground;
    CoordStruct TunnelEntrance;
    CoordStruct TunnelExit;
    bool IsEnteringTunnel;
    bool IsExitingTunnel;
    int32 EnterTimer;
    int32 ExitTimer;
    int32 SubterraneanSpeed;
    int32 DiggingTime;
    DirStruct EnterDirection;
};