#pragma once

#include "LocomotionClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/Timer.h"

class DropPodLocomotionClass : public LocomotionClass {
public:
    static constexpr int32 LocoID = LocomotionClass::CLSIDs::Droppod;

    virtual ~DropPodLocomotionClass();

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

    void DropTo(CoordStruct coord);
    void Impact();

    bool ProcessDescent();
    bool ProcessImpact();
    void OnImpact();
    void GenerateSmokeTrail();
    void DeployParachute();
    void GenerateDustCloud();
    void DeployUnits();
    void GenerateImpactCrater();
    bool IsValidDropPoint(CoordStruct coord) const;
    CoordStruct FindNearestDropPoint(CoordStruct from) const;
    int32 Get_Status() const;
    FireError Can_Fire() const;
    bool Is_Really_Moving_Now() const;
    Move Can_Enter_Cell(CellStruct cell);
    int32 GetCurrentHeight() const;
    float GetDropProgress() const;

    DropPodLocomotionClass();

protected:
    explicit DropPodLocomotionClass(noinit_t) noexcept : LocomotionClass(noinit) {}

public:
    int32 DropHeight;
    bool IsDropping;
    CoordStruct ImpactPoint;
    int32 DescentSpeed;
    int32 CurrentHeight;
    bool HasImpacted;
    int32 ImpactDelay;
    CDTimerClass ImpactTimer;
};