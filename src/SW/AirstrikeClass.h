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