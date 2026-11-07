#pragma once

#include "LocomotionClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/Timer.h"

class TeleportLocomotionClass : public LocomotionClass {
public:
    static constexpr int32 LocoID = LocomotionClass::CLSIDs::Teleport;

    virtual ~TeleportLocomotionClass();

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

    void TeleportTo(CoordStruct coord);
    bool IsTeleporting() const;
    void ChronoWarpTo(CoordStruct coord);
    int32 GetWarpPhase() const;

    bool ProcessWarpOut();
    bool ProcessWarpTransition();
    bool ProcessWarpIn();
    // asm: Process 开头的"外部传送相位同步"段（7192E0..719351）：当载体的
    //   IsWarpingOut（+0x74）或外部相位槽（+0x280）在动时，把本机相位与之同步
    //   并当帧不推进。返回 true 表示已被外部相位接管。
    bool HandleOuterWarp();
    // asm: Process 的"被抹除"分支（loc_7197CC）：置载体的抹除旗标并起蓄能计时。
    void BeginCharge();
    // asm: Process 里把相位点所在格是否为桥面写入载体的标记。
    bool TargetCellOnBridge() const;
    // asm: Process 的 719803..719BE2 相位分派与起始段。
    bool ProcessPhase();
    void BeginWarpOut();
    bool FinishWarp();
    void PlayChronoOutSound();
    void PlayChronoInSound();
    bool IsValidDestination() const;
    bool IsValidDestinationForCoord(CoordStruct coord) const;
    void RevertTeleport();
    CoordStruct GetOriginalPosition() const;
    void PlayWarpOutAnimation();
    void PlayWarpInAnimation();
    void CheckChronoVortex();
    void SpawnChronoVortex();
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知传送移位面板补全 ILocomotion 槽位的原版命名形态。
    // ------------------------------------------------------------------------
    bool ILocomotion_718080() const;
    void ILocomotion_7180A0(CoordStruct* pOut) const;
    void ILocomotion_718100(const CoordStruct& to);
    void ILocomotion_718230();
    void ILocomotion_7192C0(DirStruct dir);
    int32 ILocomotion_719E20() const;
    void ILocomotion_71A090(CoordStruct* pCoord);
    HRESULT ILocomotion_71A160(REFIID iid, void** ppvObject);
    ULONG ILocomotion_71A170();
    ULONG ILocomotion_71A180();

    TeleportLocomotionClass();

protected:
    explicit TeleportLocomotionClass(noinit_t) noexcept : LocomotionClass(noinit) {}

public:
    CDTimerClass TeleportTimer;
    bool IsTeleportingNow;
    bool HasArrived;
    AnimTypeClass* WarpOutAnim;
    AnimTypeClass* WarpInAnim;
    // asm: TeleportLocomotionClass +0x24 —— 朝往点（HeadTo）。目的地复用基类 Dest。
    CoordStruct HeadTo;
    CoordStruct TargetCell;
    int32 WarpPhase;
    int32 WarpDelay;
    // asm: 相位点所在格是否为桥面（写回载体的 +0x8C 标记）。
    bool TargetOnBridge;
};