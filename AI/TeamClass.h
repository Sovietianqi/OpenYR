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
    // TeamClass::Remove (asm 0x6EA87A): detaches Unit from the team, decrements
    // the per-slot counter for the task-force slot the unit occupied (unless a
    // slot index is supplied in idx), updates TotalObjects/totalStrength and
    // clears the unit's team links.  Returns true when the unit was a member.
    bool Remove(FootClass* pUnit, int32 idx, bool count);
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
    // ========================================================================
    // Script-action handlers (asm 0x6EDBxx..0x6EE1xx)
    //
    //  These are the TeamClass half of the script-action table: the script VM
    //  invokes one of them per action line, with `this` in ECX and the action's
    //  parameters on the stack.  Each stamps the "action executed" byte at
    //  +0x80 so the script advances.
    // ========================================================================
    void SetGlobal(void* pParam);
    void ClearGlobal(void* pParam);
    void SetLocal(void* pParam);
    void ClearLocal(void* pParam);
    void Panic(void* pParam);
    void Unpanic(void* pParam);
    void Win(void* pParam);
    void Lose(void* pParam);
    void Dud(void* pParam);
    void PlayEVA(void* pParam);
    void PlayMovie(void* pParam);
    void PlayTheme(void* pParam);
    void ReduceTiberium(void* pParam);
    void EnableHouseProduction(void* pParam);
    void ForceSale(void* pParam);
    void Suicide(void* pParam);
    void StartLStorm(void* pParam);
    void StopLStorm(void* pParam);
    void ShroudMap(void* pParam);
    void UnshroudMap(void* pParam);
    void AchieveSuccess(void* pParam);
    // TeamClass_GoToScriptAction (asm 0x6F1B4C): jumps the team's script to
    // the line the script-action argument names (argument - 2) and marks the
    // action executed.
    void GoToScriptAction(void* pParam);
    // TeamClass_RecruitThisUnit (asm 0x6EBFB0): forwards to RecruitUnit with
    // the "recruit extras" flag set.
    void RecruitThisUnit(FootClass* pUnit);
    // TeamClass_RecruitUnit (asm 0x6EA509).
    void RecruitUnit(FootClass* pUnit, bool canRecruitExtras);
    // TeamClass_CanRecruitUnit (asm 0x6EA8D1).
    bool CanRecruitUnit(FootClass* pUnit, int32* idxInTask, bool canRecruitExtras);

    // ── Queries ───────────────────────────────────────────────────────────
    // TeamClass_UnitInTeam (asm 0x6EC217): walks the member list +0x5D8 chain.
    bool UnitInTeam(TechnoClass* pUnit) const;
    // TeamClass_FindLeader (asm 0x6EC3D0): the member with the highest
    // LeadershipRating.
    TechnoClass* FindLeader() const;
    // TeamClass_CanAnyMembersAttack (asm 0x6F03F0).
    bool CanAnyMembersAttack() const;
    // TeamClass_DoesTeamHaveTransportAircraft (asm 0x6EF470).
    bool DoesTeamHaveTransportAircraft() const;
    // TeamClass_GetStrayDistance (asm 0x6F03B0).
    int32 GetStrayDistance() const;
    // TeamClass_GetSize (asm 0x6F0496): the fixed byte size of a TeamClass.
    int32 GetSize() const;
    // TeamClass_GetAbstractDerivationID (asm 0x6F04A2): AbstractType::Team.
    int32 GetAbstractDerivationID() const;
    // TeamClass_TargetTypeToFlags (asm 0x645BB0): maps the script's target
    // enumeration into the team member flag mask.
    int32 TargetTypeToFlags(int32 targetType) const;


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

    // The "action executed" byte the script VM stamps at +0x80 after each
    // script-action handler runs, so the script can advance past the line.
    bool ActionExecuted;

    // The "action achieved success" byte at +0xA1 that the script engine reads
    // when an action reports success.
    bool AchievedSuccess;

    // Per-task-force-slot unit counters (asm +0x88, six dwords: cntObjects).
    // RecruitUnit bumps the slot the new member filled; the AI uses these to
    // decide whether a slot still needs more units.
    static constexpr int32 MaxTaskForceSlots = 6;
    int32 TeamCounts[MaxTaskForceSlots];
};