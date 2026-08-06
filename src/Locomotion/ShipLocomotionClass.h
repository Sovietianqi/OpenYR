#pragma once

#include "DriveLocomotionClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class ShipLocomotionClass : public DriveLocomotionClass {
public:
    static constexpr int32 LocoID = LocomotionClass::CLSIDs::Ship;

    virtual ~ShipLocomotionClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual int32 Size() override;

    virtual Layer In_Which_Layer() override { return Layer::Surface; }
    virtual bool Process() override;
    virtual void Move_To(CoordStruct to) override;
    virtual void Stop_Moving() override;
    virtual Move Can_Enter_Cell(CellStruct cell) override;
    virtual void Limbo() override;
    virtual FireError Can_Fire() const override;
    virtual int32 Get_Status() const override;

    bool IsOnWater() const;
    bool CanNavigateTo(CellStruct cell) const;
    bool IsDockAvailable() const;
    bool IsShoreAdjacent(CellStruct cell) const;
    CellStruct FindNearestWater(CellStruct from) const;
    void Dock(CellStruct dockCell);
    void Undock();
    void Movement_AI();
    int32 GetWakeEffect() const;

    ShipLocomotionClass();

protected:
    explicit ShipLocomotionClass(noinit_t) noexcept : DriveLocomotionClass(noinit) {}

public:
    bool IsDocked;
    int32 DockCellX;
    int32 DockCellY;
    bool NeedsWater;
};