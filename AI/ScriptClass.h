#pragma once

#include "../Abstract/AbstractClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class ScriptTypeClass;
class TeamClass;

class ScriptClass {
public:
    static DynamicVectorClass<ScriptClass*>* Array;
    ScriptClass(ScriptTypeClass* pScriptType) noexcept;
    ScriptClass(ScriptTypeClass* pScriptType, TeamClass* pTeam) noexcept;
    ~ScriptClass();

    void Execute();
    void Reset();
    bool IsComplete() const;
    bool IsActive() const;
    void SetActive(bool active);
    void SetTeam(TeamClass* pTeam);
    TeamClass* GetTeam() const;
    int32 GetAction() const;
    int32 GetLine() const;

private:
    void DispatchAction(int32 action, int32 argument);
    void AdvanceLine();

    void Action_Attack(int32 argument);
    void Action_AttackWaypoint(int32 argument);
    void Action_MoveToWaypoint(int32 argument);
    void Action_MoveToCell(int32 argument);
    void Action_GuardArea(int32 argument);
    void Action_JumpToLine(int32 argument);
    void Action_PlayerCheck(int32 argument);
    void Action_Wait(int32 argument);
    void Action_Unload(int32 argument);
    void Action_Deploy(int32 argument);
    void Action_Follow(int32 argument);
    void Action_LoadIntoTransport(int32 argument);
    void Action_Spy(int32 argument);
    void Action_Patrol(int32 argument);
    void Action_EnterTunnel(int32 argument);
    void Action_ChronoWarp(int32 argument);
    void Action_ChronoSphere(int32 argument);
    void Action_IronCurtain(int32 argument);
    void Action_Sell(int32 argument);
    void Action_Repair(int32 argument);
    void Action_SelfDestruct(int32 argument);
    void Action_ChangeTeam(int32 argument);
    void Action_ChangeScript(int32 argument);
    void Action_ChangeMission(int32 argument);
    void Action_Fear(int32 argument);
    void Action_Retreat(int32 argument);
    void Action_Scatter(int32 argument);
    void Action_Stop(int32 argument);
    void Action_Sleep(int32 argument);
    void Action_Group(int32 argument);
    void Action_Recruit(int32 argument);
    void Action_Flash(int32 argument);
    void Action_LoadOntoTransports(int32 argument);
    void Action_Chronominimum(int32 argument);
    void Action_ChronoMaximum(int32 argument);
    void Action_ForceMove(int32 argument);
    void Action_Circle(int32 argument);
    void Action_SearchAndDestroy(int32 argument);
    void Action_Harmless(int32 argument);
    void Action_Suicide(int32 argument);
    void Action_Recycle(int32 argument);
    void Action_Repeat(int32 argument);
    void Action_Protect(int32 argument);
    void Action_Sticky(int32 argument);
    void Action_Emergency(int32 argument);
    void Action_TakeCover(int32 argument);
    void Action_Gibber(int32 argument);
    void Action_IronCurtainMe(int32 argument);
    void Action_ChronoSphereMe(int32 argument);
    void Action_Win(int32 argument);
    void Action_Lose(int32 argument);

public:
    ScriptTypeClass* ScriptType;
    int32 CurrentAction;
    int32 CurrentLine;
    bool IsScriptActive;
    bool IsScriptComplete;
    int32 WaitTimer;
    int32 LoopCount;
    int32 RepeatCount;
    int32 ActionTimer;
    TeamClass* Team;
    int32 FailedAttempts;
    int32 LastExecutedFrame;
};