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
    // 根据游戏行为，可知移位面板的保留查询槽位固定返回 -1。
    virtual int32 ILocomotion_4B6690() const;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override { return S_OK; }
 // 根据游戏行为，可知 GetMaxSize 负责下面这段逻辑。
    //
    //  Serialised size of a locomotion object: the instance's own Size() plus
    //  the four-byte class tag.  A null output pointer yields E_POINTER.
    virtual HRESULT GetSizeMax(uint64* pcbSize) override {
        if (pcbSize == nullptr)
            return E_POINTER;

        *pcbSize = static_cast<uint64>(Size()) + 4u;
        return S_OK;
    }

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

 // 根据游戏行为，可知 HandItOver 负责下面这段逻辑。
    //
    //  COM-style ownership handover: when the caller already holds a
    //  locomotion pointer and is about to overwrite the slot with
    //  `pNew`, the incumbent's reference is released and the incoming
    //  pointer is AddRef'd.  A null incoming pointer simply releases the old
    //  one and clears the slot.  `pSlot` is the holder's locomotion slot;
    //  the function returns it for chaining.
    static ILocomotion** HandItOver(ILocomotion** pSlot, ILocomotion* pNew);

    //========================================================================
    // 根据游戏行为，可知下面一层是 ILocomotion 接口的转发面板：调用者
    // 通过接口名访问时落进这里，再原样转到本类的同义实现。转发层让
    // 接口调用与内部实现解耦，也便于派生移动器只重写实现本身。
    //========================================================================
    virtual HRESULT ILocomotion_QueryInterface(REFIID iid, LPVOID* ppvObject);
    virtual ULONG   ILocomotion_AddRef();
    virtual ULONG   ILocomotion_Release();
    virtual int32   ILocomotion_GetStatus();
    virtual bool    ILocomotion_IsMoving();
    virtual bool    ILocomotion_IsMovingNow();
    virtual bool    ILocomotion_IsReallyMovingNow();
    virtual bool    ILocomotion_IsSurfacing();
    virtual int32   ILocomotion_ApparentSpeed();
    virtual Layer   ILocomotion_InWhichLayer();
    virtual void    ILocomotion_MarkAllOccupationBits(MarkType mark);
    virtual Move    ILocomotion_CanEnterCell(CellStruct cell);
    virtual void    ILocomotion_Lock();
    virtual void    ILocomotion_Unlock();
    virtual void    ILocomotion_TiltPitchAI();
    virtual bool    ILocomotion_Process();
    virtual bool    ILocomotion_IsMovingHere(CoordStruct to);
    virtual CoordStruct ILocomotion_HeadToCoord(CoordStruct to);
    virtual void    ILocomotion_StopMoving();
    virtual bool    ILocomotion_IsIonSensitive();
    virtual int32   ILocomotion_GetTrackIndex();
    virtual void    ILocomotion_ForceNewSlope(int32 ramp);
    virtual void    ILocomotion_StopMovementAction();
    virtual bool    ILocomotion_Shove(DirStruct dir);
    virtual bool    ILocomotion_PowerOn();
    virtual bool    ILocomotion_PowerOff();
    virtual void    ILocomotion_Unlimbo();
    virtual FireError ILocomotion_CanFire();
    virtual void    ILocomotion_ForceImmediateDestination(CoordStruct coord);
    virtual void    ILocomotion_AcquireHunterSeekerTarget();
    virtual HRESULT ILocomotion_LinkToObject(void* pointer);
    virtual bool    ILocomotion_IsToHaveShadow();
    virtual void    ILocomotion_MoveTo(CoordStruct to);
    virtual int32   ILocomotion_GetSpeedAccum();
    virtual void    ILocomotion_ForceTrack(int32 track, CoordStruct coord);
    virtual int32   ILocomotion_DrawingCode();
    virtual int32   ILocomotion_GetTrackNumber();
    virtual bool    ILocomotion_IsPowered();
    virtual CoordStruct ILocomotion_Destination();
    virtual bool    ILocomotion_Push(DirStruct dir);
    virtual bool    ILocomotion_WillJumpTracks();
    virtual void    ILocomotion_DoTurn(DirStruct dir);

    //========================================================================
    // 根据游戏行为，可知下面一组是接口侧的绘制与投影访问器：影子矩阵、
    // 影子落点、高度梯度与 Z 微调都从载具当前状态推算。
    //========================================================================
    virtual Matrix3D* ILocomotion_ShadowMatrix();
    virtual Point2D   ILocomotion_ShadowPoint(const Point2D& point);
    virtual int32     ILocomotion_ZGradient();
    virtual void      ILocomotion_ZAdjust(int32 z);
    virtual int32     ILocomotion_VisualCharacter();
    virtual void      ILocomotion_DrawMatrix(const RectangleStruct& rect);
    virtual void      ILocomotion_DrawPoint(const Point2D& point);

    //========================================================================
    // 根据游戏行为，可知下面是移动器的公用管理入口：按类别号创建对应
    // 派生移动器、确保类别已登记、序列化辅助与 COM 通用引用计数。
    //========================================================================
    static LocomotionClass* CreateInstance(int32 clsid);
    static bool             AssureExists(int32 clsid);
    virtual HRESULT         FillVar(IStream* pStm);
    virtual HRESULT         GetMaxSize(uint64* pcbSize);
    virtual ULONG           ppv_AddRef();
    virtual ULONG           ppv_AddRef2();

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