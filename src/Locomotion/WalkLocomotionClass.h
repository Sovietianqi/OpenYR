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
    virtual bool Is_Moving_Now() override { return IsMoving && !IsFalling; }
    virtual void Do_Turn(DirStruct coord) override;
    virtual Move Can_Enter_Cell(CellStruct cell) override;
    virtual bool Is_Really_Moving_Now() const override;
    virtual void Mark_All_Occupation_Bits(MarkType mark) override;
    virtual void Limbo() override;
    virtual FireError Can_Fire() const override;
    virtual int32 Get_Status() const override;
    virtual int32 Apparent_Speed() const override;
    virtual void Force_New_Slope(int32 ramp) override;

    void UpdatePosition();
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
};