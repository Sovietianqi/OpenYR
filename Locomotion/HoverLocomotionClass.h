#pragma once

#include "LocomotionClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

// ============================================================================
// HoverLocomotionClass - 悬浮移位面板
//
// 根据游戏行为，可知悬浮移位把当前位置记在自己的坐标槽里：槽值等于
// 默认值视为"未落位"，取坐标时回落到载体现行坐标。
// ============================================================================
class HoverLocomotionClass : public LocomotionClass {
public:
    static constexpr int32 LocoID = LocomotionClass::CLSIDs::Hover;

    // 默认坐标槽值（未落位哨兵）。
    static CoordStruct Default_Pos;

    HoverLocomotionClass();
    virtual ~HoverLocomotionClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual int32 Size() override;

    virtual Layer In_Which_Layer() override { return Layer::Ground; }
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

    // 根据游戏行为，可知取坐标分两路：坐标槽已离开默认值给槽值，否则
    // 回落给载体现行坐标。
    void ILocomotion_GetCoords(CoordStruct* pOut) const;

public:
    CoordStruct Position;
    int32 HoverHeight;
    bool IsHovering;
};
