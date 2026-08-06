#pragma once

#include "../Abstract/AbstractTypeClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"

class AITeamTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::TeamType;

    static DynamicVectorClass<AITeamTypeClass*>* Array;

    static AITeamTypeClass* Find(const char* pID);
    static AITeamTypeClass* FindOrAllocate(const char* pID);

    virtual ~AITeamTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    bool LoadFromINIList(CCINIClass* pINI, bool IsGlobal);

    TeamClass* CreateTeam(HouseClass* pHouse);
    FootClass* SpawnUnit(TechnoTypeClass* pType, HouseClass* pHouse);
    bool HasEnoughUnitsForCreation(HouseClass* pHouse) const;
    void DestroyAllInstances();
    int32 GetGroup() const;
    CellStruct* GetWaypoint(CellStruct* buffer) const;
    CellStruct* GetTransportWaypoint(CellStruct* buffer) const;
    bool CanRecruitUnit(FootClass* pUnit, HouseClass* pOwner) const;
    void FlashAllInstances(int32 Duration);
    TeamClass* FindFirstInstance() const;
    void ProcessTaskforce();
    static void ProcessAllTaskforces();
    HouseClass* GetHouse() const;
    void RegisterSuccess() { ++cntInstances; }
    int32 GetActiveInstanceCount() const;
    bool IsValidDifficulty() const;
    bool IsAvailableForCurrentMission() const;

    void ParseTaskForceMembers(CCINIClass* pINI);

    AITeamTypeClass(const char* pID) noexcept;

protected:
    explicit AITeamTypeClass(noinit_t) noexcept : AbstractTypeClass(noinit) {}

public:
    int32 ArrayIndex;
    int32 Group;
    int32 VeteranLevel;
    bool Loadable;
    bool Full;
    bool Annoyance;
    bool GuardSlower;
    bool Recruiter;
    bool Autocreate;
    bool Prebuild;
    bool Reinforce;
    bool Whiner;
    bool Aggressive;
    bool LooseRecruit;
    bool Suicide;
    bool Droppod;
    bool UseTransportOrigin;
    bool DropshipLoadout;
    bool OnTransOnly;
    int32 Priority;
    int32 Max;
    int32 MaxInstances;
    int32 MindControlDecision;
    HouseClass* Owner;
    int32 idxHouse;
    int32 TechLevel;
    TagClass* Tag;
    int32 Waypoint;
    int32 TransportWaypoint;
    int32 cntInstances;
    ScriptTypeClass* ScriptType;
    TaskForceClass* TaskForce;
    int32 IsGlobal;
    int32 field_EC;
    bool field_F0;
    bool field_F1;
    bool AvoidThreats;
    bool IonImmune;
    bool TransportsReturnOnUnload;
    bool AreTeamMembersRecruitable;
    bool IsBaseDefense;
    bool OnlyTargetHouseEnemy;
    DynamicVectorClass<TechnoTypeClass*> TaskForceMembers;
    DynamicVectorClass<int32> TaskForceCounts;
};