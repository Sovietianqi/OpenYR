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
    virtual void Move_To(CoordStruct to) override;
    virtual void Stop_Moving() override;
    virtual bool Is_Moving_Now() override { return IsDriving; }
    virtual void Do_Turn(DirStruct coord) override;

    virtual HRESULT Begin_Piggyback(ILocomotion* pointer) override;
    virtual HRESULT End_Piggyback(ILocomotion** pointer) override;
    virtual bool Is_Ok_To_End() override { return !IsDriving; }
    virtual HRESULT Piggyback_CLSID(GUID* classid) override { return S_OK; }
    virtual bool Is_Piggybacking() override { return Piggybackee != nullptr; }

    void UpdatePosition();
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