#pragma once

#include "../COM/IUnknown.h"
#include "../Abstract/FootClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Math/Facing.h"
#include "../Math/Timer.h"
#include "../Math/VectorMath.h"

class LocomotionClass : public IPersistStream, public ILocomotion {
public:
    struct CLSIDs {
        static constexpr int32 Drive = 0;
        static constexpr int32 Hover = 1;
        static constexpr int32 Tunnel = 2;
        static constexpr int32 Walk = 3;
        static constexpr int32 Droppod = 4;
        static constexpr int32 Fly = 5;
        static constexpr int32 Teleport = 6;
        static constexpr int32 Mech = 7;
        static constexpr int32 Ship = 8;
        static constexpr int32 Jumpjet = 9;
        static constexpr int32 Rocket = 10;
    };

    virtual HRESULT QueryInterface(REFIID iid, LPVOID* ppvObject) override { return E_FAIL; }
    virtual ULONG AddRef() override { ++RefCount; return RefCount; }
    virtual ULONG Release() override { if (RefCount > 0) --RefCount; return RefCount; }

    virtual HRESULT GetClassID(CLSID* pClassID) override = 0;
    virtual HRESULT IsDirty() override { return 0; }
    virtual HRESULT Load(IStream* pStm) override { return S_OK; }
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override { return S_OK; }
    virtual HRESULT GetSizeMax(uint64* pcbSize) override { return 0; }

    virtual ~LocomotionClass() = default;
    virtual int32 Size() = 0;

    virtual HRESULT Link_To_Object(void* pointer) override { return S_OK; }
    virtual bool Is_Moving() override { return IsMoving; }
    virtual CoordStruct Destination() override { return Dest; }
    virtual CoordStruct Head_To_Coord() override { return Dest; }
    virtual CoordStruct Head_To_Coord() const;
    virtual bool Is_To_Have_Shadow() override { return true; }

    virtual bool Process() override;
    virtual void Move_To(CoordStruct to) override;
    virtual void Move_To(AbstractClass* target);
    virtual void Stop_Moving() override;
    virtual void Do_Turn(DirStruct coord) override;
    virtual Move Can_Enter_Cell(CellStruct cell) override;
    virtual bool Is_Moving_Here(CoordStruct to) override;
    virtual bool Will_Jump_Tracks() override;
    virtual bool Will_Jump_Tracks() const;
    virtual bool Is_Really_Moving_Now() override;
    virtual bool Is_Really_Moving_Now() const;
    virtual bool Is_Surfacing() override;
    virtual bool Is_Surfacing() const;
    virtual void Mark_All_Occupation_Bits(MarkType mark) override;
    virtual void Limbo() override;
    virtual void Unlimbo() override;
    virtual void Lock() override;
    virtual void Unlock() override;
    virtual void Tilt_Pitch_AI() override;
    virtual bool Power_On() override;
    virtual bool Power_Off() override;
    virtual bool Is_Powered() override;
    virtual bool Is_Powered() const;
    virtual bool Is_Ion_Sensitive() override;
    virtual bool Is_Ion_Sensitive() const;
    virtual bool Push(DirStruct dir) override;
    virtual bool Shove(DirStruct dir) override;
    virtual void Force_Track(int32 track, CoordStruct coord) override;
    virtual Layer In_Which_Layer() override = 0;
    virtual void Force_Immediate_Destination(CoordStruct coord) override;
    virtual void Force_New_Slope(int32 ramp) override;
    virtual bool Is_Moving_Now() override;
    virtual bool Is_Moving_Now() const;
    virtual int32 Apparent_Speed() override;
    virtual int32 Apparent_Speed() const;
    virtual int32 Drawing_Code() override;
    virtual int32 Drawing_Code() const;
    virtual FireError Can_Fire() override;
    virtual FireError Can_Fire() const;
    virtual int32 Get_Status() override;
    virtual int32 Get_Status() const;
    virtual void Acquire_Hunter_Seeker_Target() override;
    virtual void Stop_Movement_Animation() override;
    virtual int32 Get_Track_Number() override;
    virtual int32 Get_Track_Number() const;
    virtual int32 Get_Track_Index() override;
    virtual int32 Get_Track_Index() const;
    virtual int32 Get_Speed_Accum() override;
    virtual int32 Get_Speed_Accum() const;

    void LinkToObject(FootClass* pFoot);
    bool IsMovingHere(CoordStruct coord);
    bool CanMoveHere(CoordStruct coord);
    CoordStruct GetClosestOkCell(CoordStruct coord);
    int32 GetSpeed() const { return Speed; }
    void SetSpeed(int32 speed) { Speed = speed; }
    void Movement_AI();
    void Do_Turret_Turn(DirStruct coord);
    void Face_Target(AbstractClass* target);
    CoordStruct Destination_Coord() const;
    bool Can_Traverse_To(CellStruct targetCell);
    int32 Get_Speed() const;
    void Appear_At(CoordStruct coord);
    void Power_Off_Track();
    bool Is_Limboed() const;
    bool Over_Travel() const;
    bool Is_On_Lock() const;
    void Force_New_Land_Type(LandType land);
    bool Is_Moving_On_Bridge() const;
    bool Is_Bridge_Destroyed() const;
    int32 Get_Slope() const;
    bool Is_To_Have_Moving_Anim() const;

    LocomotionClass();

protected:
    explicit LocomotionClass(noinit_t) noexcept {}

public:
    FootClass* Owner;
    FootClass* LinkedTo;
    bool Powered;
    bool Dirty;
    int32 RefCount;
    int32 Speed;
    float SpeedPercentage;
    bool IsMoving;
    CoordStruct Dest;
    CoordStruct CurrentCoord;
    int32 SpeedAccum;
};