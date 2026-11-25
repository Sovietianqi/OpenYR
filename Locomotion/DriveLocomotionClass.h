#pragma once

#include "LocomotionClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/Timer.h"
#include "../Math/Facing.h"

class DriveLocomotionClass : public LocomotionClass, public IPiggyback {
public:
    static constexpr int32 LocoID = LocomotionClass::CLSIDs::Drive;

    virtual ~DriveLocomotionClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual int32 Size() override;

    virtual Layer In_Which_Layer() override { return Layer::Ground; }
    virtual bool Process() override;
    // asm: sub_4B0F20 —— 载具每帧驱动主体（转身 / 摇车 / 地形速度 / 逐格推进）。
    void UpdateDrive();
    // asm: sub_4B0F20 的地表速度段：按格的地表类型调 SpeedPercentage。
    void ApplyTerrainSpeed();
    // asm: sub_4B0F20 的坡道段：按 CurrentRamp 与 SlopeTimer 收敛 MovementSpeed。
    void UpdateSlopeSpeed();
    // asm: sub_4B0F20 的逐格推进段：按 SpeedAccum>=256 推进并做到达判定。
    void StepTowardsDestination(int32 effectiveSpeed);
    virtual void Move_To(CoordStruct to) override;
    virtual void Stop_Moving() override;
    // asm: DriveLocomotionClass_ILocomotion_IsMoving        (asm:372)
    virtual bool Is_Moving() override;
    // asm: DriveLocomotionClass_ILocomotion_IsMovingNow     (asm:467)
    virtual bool Is_Moving_Now() override;
    virtual void Do_Turn(DirStruct coord) override;

    virtual HRESULT Begin_Piggyback(ILocomotion* pointer) override;
    virtual HRESULT End_Piggyback(ILocomotion** pointer) override;
    virtual bool Is_Ok_To_End() override { return !IsDriving; }
    virtual HRESULT Piggyback_CLSID(GUID* classid) override { return S_OK; }
    virtual bool Is_Piggybacking() override { return Piggybackee != nullptr; }

    void UpdatePosition();
    // 载具所在格的占用登记同步（CellClass::Occupier 槽 + 地面 FirstObject 链）。
    //   只在格号变化时做"旧格注销 + 新格登记"，每帧最多一次旁表查询。
    //   不动类布局：上次登记的格存在 DriveLocomotionClass.cpp 内的文件级旁表里。
    void SyncCellOccupation();
    // 彻底摘除本移动器的占用登记（载体离场 / 销毁 / Limbo）。
    void ReleaseCellOccupation();
    void RotateTowards(DirStruct targetDir);
    bool IsOnBridge() const;
    bool CheckBridge();
    bool ProcessRocking();
    void Movement_AI();
    void Do_Turret_Turn(DirStruct coord);
    void CheckCrush();
    bool CheckBuildingCollision();
    Move Can_Enter_Cell(CellStruct cell);
    void Mark_All_Occupation_Bits(MarkType mark);
    void Limbo();
    void Force_New_Slope(int32 ramp);
    int32 Get_Status() const;
    bool Is_Really_Moving_Now() const;
    FireError Can_Fire() const;
    void Force_Track(int32 track, CoordStruct coord);
    int32 Get_Track_Number() const;
    int32 Get_Track_Index() const;
    void Lock();
    void Unlock();
    void Tilt_Pitch_AI();

    DriveLocomotionClass();

protected:
    explicit DriveLocomotionClass(noinit_t) noexcept : LocomotionClass(noinit) {}

public:
    uint32 PreviousRamp;
    uint32 CurrentRamp;
    RateTimer SlopeTimer;
    CoordStruct Destination;
    CoordStruct HeadToCoord;
    int32 SpeedAccum;
    double MovementSpeed;
    uint32 TrackNumber;
    int32 TrackIndex;
    bool IsOnShortTrack;
    uint8 IsTurretLockedDown;
    bool IsRotating;
    bool IsDriving;
    bool IsRocking;
    bool IsLocked;
    ILocomotion* Piggybackee;
    int32 TrackType;
    bool Crusher;
    bool Destroyer;
    bool IsHovering;
    bool IsAmphibious;
    DirStruct CurrentFacing;
    DirStruct TargetFacing;
    int32 RotationSpeed;
};