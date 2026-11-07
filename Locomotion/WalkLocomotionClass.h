#pragma once

#include "LocomotionClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"

class WalkLocomotionClass : public LocomotionClass {
public:
    static constexpr int32 LocoID = LocomotionClass::CLSIDs::Walk;

    virtual ~WalkLocomotionClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual int32 Size() override;

    virtual Layer In_Which_Layer() override { return Layer::Ground; }
    virtual bool Process() override;
    virtual void Move_To(CoordStruct to) override;
    virtual void Move_To(AbstractClass* target);
    virtual void Stop_Moving() override;
    // asm: WalkLocomotionClass_ILocomotion_IsMovingNow   (asm:106)
    //   `Is_Moving() && Owner->SpeedPercentage > 0 && HeadTo != (0,0,0)`。
    virtual bool Is_Moving_Now() override;
    virtual void Do_Turn(DirStruct coord) override;
    virtual Move Can_Enter_Cell(CellStruct cell) override;
    // asm: WalkLocomotionClass_ILocomotion_IsReallyMovingNow   (asm:1418)
    virtual bool Is_Really_Moving_Now() const override;
    virtual void Mark_All_Occupation_Bits(MarkType mark) override;
    virtual void Limbo() override;
    virtual FireError Can_Fire() const override;
    virtual int32 Get_Status() const override;
    virtual int32 Apparent_Speed() const override;
    virtual void Force_New_Slope(int32 ramp) override;
    // asm: WalkLocomotionClass_ILocomotion_HeadToCoord   (asm:232)
    virtual CoordStruct Head_To_Coord() override;
    virtual CoordStruct Head_To_Coord() const;
    // asm: WalkLocomotionClass_ILocomotion_Destination   (asm:168)
    virtual CoordStruct Destination() override;
    // asm: WalkLocomotionClass_ILocomotion_Lock   (asm:1446)
    virtual void Lock() override;
    // asm: WalkLocomotionClass_ILocomotion_ForceImmediateDestination   (asm:549)
    virtual void Force_Immediate_Destination(CoordStruct coord) override;
    // asm: WalkLocomotionClass_ILocomotion_IsMovingHere   (asm:1319)
    virtual bool Is_Moving_Here(CoordStruct to) override;
    // asm: WalkLocomotionClass_ILocomotion_StopMovementAction   (asm:1577)
    virtual void Stop_Movement_Animation() override;

    void UpdatePosition();
    // asm: sub_75AEC0 的坡道/楼梯高度插值段：逐帧把 Z 逼近目标格地面高度，
    //   差值清零时置 JustLanded。返回本帧是否在坡道上。
    bool InterpolateHeight(int32 targetZ);
    // 朝往点是否已被走到（到达判定，距离 <= Rules->CloseEnough）。
    bool IsAtDestination() const;
    // 本帧生效速度：载体 GetCurrentSpeed 与老兵/SpeedPercentage 折合。
    int32 GetCurrentSpeed() const;
    bool MoveOneStep();
    bool MoveToNextCell();
    bool CanMoveToCell(CellStruct cell);
    CellStruct FindAlternativeCell(CellStruct blocked);
    bool ProcessFall();
    bool ProcessJump();
    void BeginCrawl();
    void EndCrawl();
    int32 GetStepSound() const;
    void PlayStepSound();
    void Movement_AI();
    bool Is_To_Have_Moving_Anim() const;
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知步行移位面板补全搭车（IPiggyback）槽位的原版命名
    // 形态：COM 三槽转发，搭车状态以一个 ILocomotion* 槽承载。
    // ------------------------------------------------------------------------
    HRESULT IPiggyback_QueryInterface(REFIID iid, void** ppvObject);
    ULONG IPiggyback_AddRef();
    ULONG IPiggyback_Release();
    bool IPiggyback_IsPiggybacking() const;
    HRESULT IPiggyback_BeginPiggyback(ILocomotion* pLoco);
    HRESULT IPiggyback_EndPiggyback(ILocomotion** ppOut);
    bool IPiggyback_IsOKToEnd();
    HRESULT IPiggyback_PiggybackCLSID(GUID* pGUID);

    // 搭车槽：被驮载的移位对象，空表示未搭车。
    ILocomotion* Piggyback = nullptr;

    WalkLocomotionClass();

protected:
    explicit WalkLocomotionClass(noinit_t) noexcept : LocomotionClass(noinit) {}

public:
    VectorClass<CellStruct> Path;
    CellStruct CurrentCell;
    CellStruct NextCell;
    int32 PathIndex;
    int32 PathLength;
    bool IsFalling;
    bool IsJumping;
    bool IsCrawling;
    int32 MoveStepTimer;
    int32 StepSize;
    // asm: WalkLocomotionClass +0x24 —— 朝往点（HeadTo）。本帧真正要推进到的
    //   坐标，非零时覆盖 Owner 的缓存绘制点。
    CoordStruct HeadTo;
    // asm: WalkLocomotionClass +0x32 —— 本帧是否真的推进了（IsReallyMovingNow）。
    bool IsReallyMovingNow;
    // asm: WalkLocomotionClass +0x34/+0x35/+0x36 —— 三个逐帧布尔：
    //   是否在坡道、是否刚落地、是否本帧被挤压。
    bool OnRamp;
    bool JustLanded;
    bool WasCrushed;
};