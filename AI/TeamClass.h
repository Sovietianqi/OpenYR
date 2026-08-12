#pragma once

#include "../Abstract/AbstractClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"
#include "../Math/CoordStruct.h"

class TeamClass {
public:
    static DynamicVectorClass<TeamClass*>* Array;

    TeamClass(TeamTypeClass* pType, HouseClass* pOwner, int32 nFlags) noexcept;
    virtual ~TeamClass();

    void Update();
    bool AddMember(TechnoClass* pTechno, bool isLeader);
    bool RemoveMember(int32 index);
    int32 GetMemberCount() const;
    TechnoClass* GetMember(int32 index) const;
    void AssignMissionToAll(Mission mission);
    void AssignTargetToAll(AbstractClass* pTarget);
    void Form();
    void Disband();
    void MoveToWaypoint(int32 waypointIndex);
    void MoveToLocation(CoordStruct location);
    void AttackTarget(AbstractClass* pTarget);
    void GuardArea(CoordStruct location, int32 radius);
    void GuardTarget(AbstractClass* pTarget);
    void PatrolArea(CoordStruct toLocation);
    void HandleMemberDeath(TechnoClass* pTechno);
    bool DoesTeamStillExist() const;
    bool CanRecruit() const;
    int32 GetTotalStrength() const;
    bool IsTeamFull() const;
    void ReinforceTeam(int32 nUnits);
    void ReGroup();
    void SortByThreatValue();
    void UpdateRecruitTimer();
    void SetRecruitTimer(int32 frames);
    bool IsRecruitTimerExpired() const;
    int32 GetThreatValue(TechnoClass* pTechno) const;

private:
    void CleanupDeadMembers();
    void DoDisappear();
    CoordStruct ComputeFormationCenter();

public:
    TeamTypeClass* Type;
    HouseClass* Owner;
    int32 CreationFrame;
    bool IsTransient;
    bool IsFullStrength;
    bool NeedsToDisappear;
    bool JustDisappeared;
    int32 Value;
    int32 RecruitRadius;
    int32 RecruitTimer;
    ScriptClass* Script;
    DynamicVectorClass<TechnoClass*> Members;
    AbstractClass* ITarget;
    TeamClass* NextTeam;
    TeamClass* PrevTeam;
    int32 GuardAreaTimer;
    int32 CurrentMission;
    int32 TotalThreatValue;
    int32 totalStrength;
    int32 idxTeam;
};