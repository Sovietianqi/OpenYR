#pragma once

#include "../Abstract/AbstractClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"

// ============================================================================
// Forward declarations
// ============================================================================

class AircraftTypeClass;
class ObjectClass;
class TechnoClass;
class FootClass;
class HouseClass;

// ============================================================================
// AirstrikeClass - handles Boris-style airstrike missions
// ============================================================================

class NOVTABLE AirstrikeClass : public AbstractClass {
public:
    static const AbstractType AbsID = AbstractType::Airstrike;

    AirstrikeClass(TechnoClass* pOwner) noexcept;
    virtual ~AirstrikeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual void Update() override;

    // Core functionality
    void StartMission(ObjectClass* pTarget);
    bool CanFire() const;
    void Start();
    void Fire();
    void RemoveAmmo();

    // ========================================================================
    // asm 权威重载（字段偏移与 asm 对齐：+3Ch=IsOnMission / +3Dh=unknown_bool_3D
    //   / +40h=TeamDissolveFrame / +44h=AirstrikeRechargeTime /
    //   +48h=EliteAirstrikeRechargeTime / +4Ch=Owner / +50h=Target）。
    //
    //   CanFire(target) (asm: AirstrikeClass_CanFire, retn 4):
    //      recharge = ELITE(owner) ? [48h] : [44h]
    //      if ([3Ch]==0 && [40h]+recharge <= Frame) return true
    //      else return target != [50h]
    //   Start(target) (asm: AirstrikeClass_Start, retn 4):
    //      if ([3Ch]==0) Fire(target) else if (target != [50h]) SetNewTarget(target)
    //   SetNewTarget(target) (asm: AirstrikeClass_SetNewTarget, retn 4):
    //      遍历目标航空器的 [2FCh] 链；命中不同 owner 则 RemoveAmmo；再逐个
    //      把空袭节点重定向到新目标。
    // ========================================================================
    bool CanFire(ObjectClass* pTarget) const;
    void Start(ObjectClass* pTarget);
    void SetNewTarget(ObjectClass* pTarget);

    // asm: AirstrikeClass_Fire (retn 4: tgt) —— 置位 [3Ch]/[3Dh]，清 [34h]，
    //   取 Rules 空袭队类型经 HouseClass::CreateAirstrike 派机，写 [50h]=tgt。
    void Fire(ObjectClass* pTarget);

    // ------------------------------------------------------------------------
    // asm: AirstrikeClass_RemoveNode (0x41DC80)
    //
    //   void __thiscall AirstrikeClass_RemoveNode(AirstrikeClass *this, void *pNode)
    //
    //   从空袭的节点链表里摘掉一个节点。asm 触及的偏移：
    //     +0x5C  void*  Head          链表头
    //     +0x30  int32  Count         节点计数
    //     +0x3C  BYTE   flagA         Count 归零时清 0
    //     +0x3D  BYTE   flagB         Count 归零时清 0
    //     +0x40  int32  Frame         Count 归零时写当前帧
    //   节点布局：+0x5D8 next，+0x294 owner 反向引用。
    //
    //   遍历上限 0x64 (100) 跳，与原版一致（防止环形链表死循环）。
    // ------------------------------------------------------------------------
    void Remove_Node(void* pNode);

    // State
    bool IsInbound() const { return IsOnMission && !IsReturningState; }
    bool IsReturning() const { return IsReturningState; }
    bool IsOnMissionCount() const { return IsOnMission; }

    // Properties
    TechnoClass* GetOwner() const { return Owner; }
    ObjectClass* GetTarget() const { return Target; }
    int32 GetAirstrikeTeam() const { return AirstrikeTeam; }
    int32 GetRechargeTime() const { return AirstrikeRechargeTime; }

    void SetAirstrikeTeam(int32 team) { AirstrikeTeam = team; }
    void SetEliteAirstrikeTeam(int32 team) { EliteAirstrikeTeam = team; }

protected:
    explicit AirstrikeClass(noinit_t) noexcept : AbstractClass(noinit) {}

    void DispatchAircraft();
    void RecallAircraft();
    void CheckTargetValidity();
    bool IsTargetValid() const;

public:
    int32 AirstrikeTeam;
    int32 EliteAirstrikeTeam;
    int32 AirstrikeTeamTypeIndex;
    int32 EliteAirstrikeTeamTypeIndex;
    uint32 unknown_34;
    uint32 unknown_38;
    bool IsOnMission;
    bool unknown_bool_3D;
    uint32 TeamDissolveFrame;
    int32 AirstrikeRechargeTime;
    int32 EliteAirstrikeRechargeTime;
    TechnoClass* Owner;
    ObjectClass* Target;
    AircraftTypeClass* AirstrikeTeamType;
    AircraftTypeClass* EliteAirstrikeTeamType;
    FootClass* FirstObject;

    // Additional runtime data
    DynamicVectorClass<CoordStruct> FlightPath;
    bool IsReturningState;
    int32 DispatchedAircraftCount;
    int32 ReturnedAircraftCount;
    int32 RechargeTimer;
    bool IsReady;
    bool IsElite;
};