#pragma once

#include "LocomotionClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/Facing.h"

class JumpjetLocomotionClass : public LocomotionClass, public IPiggyback {
public:
    static constexpr int32 LocoID = LocomotionClass::CLSIDs::Jumpjet;

    enum State {
        Grounded = 0, Ascending = 1, Hovering = 2,
        Cruising = 3, Descending = 4, Crashing = 5, Unknown = 6
    };

    virtual ~JumpjetLocomotionClass();

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

    virtual HRESULT Begin_Piggyback(ILocomotion* pointer) override;
    virtual HRESULT End_Piggyback(ILocomotion** pointer) override;
    virtual bool Is_Ok_To_End() override { return !IsMoving; }
    virtual HRESULT Piggyback_CLSID(GUID* classid) override { return S_OK; }
    virtual bool Is_Piggybacking() override { return Piggybackee != nullptr; }

    void Hover();
    void Jump();
    void Crash();
    void DeployParachute();
    int32 Get_Status() const;
    FireError Can_Fire() const;
    bool Is_Really_Moving_Now() const;
    int32 GetAltitude() const;

    bool ProcessCrash();
    void FindLandingZone();
    // 根据游戏行为，可知跳跃喷气移位在挪位前确认目标点上空无占用。
    static bool LocationClear(const CoordStruct& coord);
    bool IsValidLandingCell() const;

    // asm: sub_54D0F0  (asm 主库 721618) —— 每帧公共段：高度插值（70h 当前 /
    //   78h 目标 / 30h 爬升率 / 20h 上限）、载体的飞行高度比例（vtbl+544）、
    //   悬停摆动的正弦推进，以及离子风暴坠毁判定。
    void TickCommon();
    // asm: sub_54B980  (asm 主库 718999) —— case 0：已落地/待命，收到移动指令即起飞。
    bool ProcessLanded();
    // asm: sub_54BA30  (asm 主库 719081) —— case 1：爬升到巡航高度后转巡航。
    bool ProcessClimb();
    // asm: jumpjet2     (asm 主库 719714) —— case 3：巡航平飞，按住朝往点推进。
    bool ProcessCruise();
    // asm: sub_54BD30   (asm 主库 719414) —— case 2：悬停保持巡航高度。
    bool ProcessHover();
    // asm: sub_54C550   (asm 主库 720281) —— case 4：下降进场。
    bool ProcessDescent();
    // asm: sub_54CA90   (asm 主库 720843) —— case 5：触地落位。
    bool ProcessTouchdown();
    // 巡航高度取自 Rules 的类型级 JumpJetHeight（缺省按类型回退）。
    int32 ResolveCruiseHeight() const;
    // 朝往点存在时把朝向交给载体 FacingClass（Desired_Facing256 语义）。
    bool FaceDestination();

    JumpjetLocomotionClass();

protected:
    explicit JumpjetLocomotionClass(noinit_t) noexcept : LocomotionClass(noinit) {}

public:
    int32 TurnRate;
    int32 JumpSpeed;
    float Climb;
    float CruiseHeight;
    float HoverHeight;
    State CurrentState;
    bool IsCrashing;
    DirStruct JumpDirection;
    CoordStruct MovingDestination;
    int32 Altitude;
    int32 TargetAltitude;
    ILocomotion* Piggybackee;
    int32 CrashTimer;
    CDTimerClass StateTimer;

    // asm: +0x70/+0x78 —— 当前 / 目标飞行高度（双精度 leptons）。
    double FlightAltitude;
    double TargetFlightAltitude;
    // asm: +0x30 —— 每帧爬升率（爬升用原值，下降用 1.5 倍）。
    float ClimbRate;
    // asm: +0x88 —— 悬停摆动的相位累积量（每帧按悬浮阻尼推进）。
    double HoverPhase;
    // asm: +0x20 —— 本机型的高度上限（巡航高度）。
    int32 MaxAltitude;
};