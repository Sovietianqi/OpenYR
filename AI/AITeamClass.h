#pragma once

#include "../Abstract/AbstractClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"
#include "../Math/Timer.h"
#include "../Math/CoordStruct.h"

class AITeamClass : public AbstractClass {
public:
    static const AbstractType AbsID = AbstractType::AITrigger;

    static DynamicVectorClass<AITeamClass*>* Array;

    virtual ~AITeamClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    void Update();
    bool AddMember(TechnoClass* pTechno);
    bool RemoveMember(TechnoClass* pTechno);
    TechnoClass* FindUnitToFollow();
    void ExecuteScript();
    bool IsTeamFull() const;
    bool IsActive() const { return Active; }
    void SetActive(bool active) { Active = active; }
    void Suspend(int32 duration);
    void Resume();
    void ReGroup();
    void FormUp();
    void CleanupDeadMembers();
    void DestroyAllMembers();
    void Disband();
    void HandleMemberDeath(TechnoClass* pTechno);
    void AssignMissionToAll(Mission mission);
    void AssignTargetToAll(AbstractClass* pTarget);
    CoordStruct GetTeamCenter() const;
    CellStruct GetTeamCell() const;
    bool CanRecruit() const;
    bool ReinforceTeam(int32 nUnits);
    int32 GetTotalStrength() const;
    int32 GetActiveMemberCount() const;
    bool IsTeamCombatReady() const;
    CoordStruct ComputeFormationCenter() const;

    AITeamClass(AITeamTypeClass* pType, HouseClass* pOwner) noexcept;

protected:
    explicit AITeamClass(noinit_t) noexcept : AbstractClass(noinit) {}

public:
    AITeamTypeClass* TeamType;
    HouseClass* Owner;
    ScriptClass* Script;
    DynamicVectorClass<TechnoClass*> Members;
    int32 CurrentAction;
    bool Active;
    int32 CurrentScriptLine;
    bool IsFormed;
    int32 CreationFrame;
    int32 TotalThreatValue;
    CDTimerClass GuardAreaTimer;
    CDTimerClass SuspendTimer;
    bool IsSuspended;
    bool IsReforming;
    bool NeedsReGrouping;
    CoordStruct FormationTarget;
};