#include "ScriptClass.h"
#include "ScriptTypeClass.h"
#include "TeamClass.h"
#include "AITeamClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/FootClass.h"
#include "../Houses/HouseClass.h"
#include "../Game/Game.h"
#include "../Scenario/ScenarioClass.h"
#include "../Map/MapClass.h"
#include "../Math/CoordStruct.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

ScriptClass::ScriptClass(ScriptTypeClass* pScriptType) noexcept
    : ScriptType(pScriptType), CurrentAction(0), CurrentLine(0),
      IsScriptActive(true), IsScriptComplete(false), WaitTimer(0), LoopCount(0),
      RepeatCount(0), ActionTimer(0), Team(nullptr),
      FailedAttempts(0), LastExecutedFrame(0) {
    if (pScriptType && pScriptType->ScriptActions[0].Action == 0x2A) {
        CurrentLine = 1;
    }
}

ScriptClass::ScriptClass(ScriptTypeClass* pScriptType, TeamClass* pTeam) noexcept
    : ScriptType(pScriptType), CurrentAction(0), CurrentLine(0),
      IsScriptActive(true), IsScriptComplete(false), WaitTimer(0), LoopCount(0),
      RepeatCount(0), ActionTimer(0), Team(pTeam),
      FailedAttempts(0), LastExecutedFrame(0) {
    if (pScriptType && pScriptType->ScriptActions[0].Action == 0x2A) {
        CurrentLine = 1;
    }
}

ScriptClass::~ScriptClass() {
}

void ScriptClass::Execute() {
    if (!IsScriptActive || IsScriptComplete) return;
    if (!ScriptType) return;

    if (WaitTimer > 0) {
        --WaitTimer;
        if (WaitTimer > 0) return;
    }

    if (ActionTimer > 0) {
        --ActionTimer;
        if (ActionTimer > 0) return;
    }

    if (CurrentLine >= ScriptType->GetTotalActionCount()) {
        IsScriptComplete = true;
        IsScriptActive = false;
        return;
    }

    int32 action = ScriptType->ScriptActions[CurrentLine].Action;
    int32 argument = ScriptType->ScriptActions[CurrentLine].Argument;

    DispatchAction(action, argument);

    LastExecutedFrame = Game::CurrentFrame;
}

void ScriptClass::DispatchAction(int32 action, int32 argument) {
    switch (action) {
        case 0:  Action_Attack(argument); break;
        case 1:  Action_AttackWaypoint(argument); break;
        case 2:  Action_MoveToWaypoint(argument); break;
        case 3:  Action_MoveToCell(argument); break;
        case 4:  Action_GuardArea(argument); break;
        case 5:  Action_JumpToLine(argument); break;
        case 6:  Action_PlayerCheck(argument); break;
        case 7:  Action_Wait(argument); break;
        case 8:  Action_Unload(argument); break;
        case 9:  Action_Deploy(argument); break;
        case 10: Action_Follow(argument); break;
        case 11: Action_LoadIntoTransport(argument); break;
        case 12: Action_Spy(argument); break;
        case 13: Action_Patrol(argument); break;
        case 14: Action_EnterTunnel(argument); break;
        case 15: Action_ChronoWarp(argument); break;
        case 16: Action_ChronoSphere(argument); break;
        case 17: Action_IronCurtain(argument); break;
        case 18: Action_Sell(argument); break;
        case 19: Action_Repair(argument); break;
        case 20: Action_SelfDestruct(argument); break;
        case 21: Action_ChangeTeam(argument); break;
        case 22: Action_ChangeScript(argument); break;
        case 23: Action_ChangeMission(argument); break;
        case 24: Action_Fear(argument); break;
        case 25: Action_Retreat(argument); break;
        case 26: Action_Scatter(argument); break;
        case 27: Action_Stop(argument); break;
        case 28: Action_Sleep(argument); break;
        case 29: Action_Group(argument); break;
        case 30: Action_Recruit(argument); break;
        case 31: Action_Flash(argument); break;
        case 32: Action_LoadOntoTransports(argument); break;
        case 33: Action_Chronominimum(argument); break;
        case 34: Action_ChronoMaximum(argument); break;
        case 35: Action_ForceMove(argument); break;
        case 36: Action_Circle(argument); break;
        case 37: Action_SearchAndDestroy(argument); break;
        case 38: Action_Harmless(argument); break;
        case 39: Action_Suicide(argument); break;
        case 40: Action_Recycle(argument); break;
        case 41: Action_Repeat(argument); break;
        case 42: Action_Protect(argument); break;
        case 43: Action_Sticky(argument); break;
        case 44: Action_Emergency(argument); break;
        case 45: Action_TakeCover(argument); break;
        case 46: Action_Gibber(argument); break;
        case 47: Action_IronCurtainMe(argument); break;
        case 48: Action_ChronoSphereMe(argument); break;
        case 49: Action_Win(argument); break;
        case 50: Action_Lose(argument); break;
        default:  AdvanceLine(); break;
    }
}

void ScriptClass::Action_Attack(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Attack);
    }
    AdvanceLine();
    ActionTimer = 30;
}

void ScriptClass::Action_AttackWaypoint(int32 argument) {
    if (Team) {
        CellStruct waypointCell;
        if (ScenarioClass::Instance) {
            waypointCell = ScenarioClass::Instance->GetWaypointCoords(argument);
        }
        CoordStruct targetPos = Math::CellToCoord(waypointCell);
        Team->AssignMissionToAll(Mission::Attack);
    }
    AdvanceLine();
    ActionTimer = 30;
}

void ScriptClass::Action_MoveToWaypoint(int32 argument) {
    if (Team) {
        CellStruct waypointCell;
        if (ScenarioClass::Instance) {
            waypointCell = ScenarioClass::Instance->GetWaypointCoords(argument);
        }
        CoordStruct targetPos = Math::CellToCoord(waypointCell);
        Team->AssignMissionToAll(Mission::Move);
    }
    AdvanceLine();
}

void ScriptClass::Action_MoveToCell(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Move);
    }
    AdvanceLine();
}

void ScriptClass::Action_GuardArea(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::AreaGuard);
        Team->GuardAreaTimer = argument * 10;
    }
    AdvanceLine();
}

void ScriptClass::Action_JumpToLine(int32 argument) {
    if (argument >= 0 && argument < ScriptType->GetTotalActionCount()) {
        CurrentLine = argument;
    } else {
        AdvanceLine();
    }
}

void ScriptClass::Action_PlayerCheck(int32 argument) {
    if (Team) {
        if (Team->Owner && !Team->Owner->IsHumanPlayer) {
            AdvanceLine();
        }
    }
    AdvanceLine();
}

void ScriptClass::Action_Wait(int32 argument) {
    WaitTimer = argument;
    AdvanceLine();
}

void ScriptClass::Action_Unload(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Unload);
    }
    AdvanceLine();
    ActionTimer = 90;
}

void ScriptClass::Action_Deploy(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Deploy);
    }
    AdvanceLine();
}

void ScriptClass::Action_Follow(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Follow);
    }
    AdvanceLine();
}

void ScriptClass::Action_LoadIntoTransport(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Enter);
    }
    AdvanceLine();
}

void ScriptClass::Action_Spy(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Spy);
    }
    AdvanceLine();
}

void ScriptClass::Action_Patrol(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Patrol);
    }
    AdvanceLine();
}

void ScriptClass::Action_EnterTunnel(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::EnterTunnel);
    }
    AdvanceLine();
}

void ScriptClass::Action_ChronoWarp(int32 argument) {
    if (Team) {
        CellStruct targetCell;
        if (ScenarioClass::Instance) {
            targetCell = ScenarioClass::Instance->GetWaypointCoords(argument);
        }
        CoordStruct targetPos = Math::CellToCoord(targetCell);
        // Issue the chrono-warp mission and rebase the team at the target.
        Team->AssignMissionToAll(Mission::ChronoWarp);
        Team->MoveToLocation(targetPos);
    }
    AdvanceLine();
}

void ScriptClass::Action_ChronoSphere(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::ChronoSphere);
    }
    AdvanceLine();
}

void ScriptClass::Action_IronCurtain(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::IronCurtain);
    }
    AdvanceLine();
}

void ScriptClass::Action_Sell(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Selling);
    }
    AdvanceLine();
}

void ScriptClass::Action_Repair(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Repair);
    }
    AdvanceLine();
}

void ScriptClass::Action_SelfDestruct(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::SelfDestruct);
    }
    AdvanceLine();
}

void ScriptClass::Action_ChangeTeam(int32 argument) {
    if (Team && argument >= 0) {
        HouseClass* newHouse = nullptr;
        int32 idx = 0;
        for (int32 i = 0; i < 32; ++i) {
            if (HouseClass::Array[i] && !HouseClass::Array[i]->IsDefeated) {
                if (idx == argument) {
                    newHouse = HouseClass::Array[i];
                    break;
                }
                ++idx;
            }
        }
        if (newHouse && Team->Owner) {
        }
    }
    AdvanceLine();
}

void ScriptClass::Action_ChangeScript(int32 argument) {
    if (Team && ScriptType) {
        char scriptName[0x18];
        snprintf(scriptName, sizeof(scriptName), "%d", argument);
        ScriptTypeClass* newScript = ScriptTypeClass::Find(scriptName);
        if (newScript) {
            ScriptType = newScript;
            CurrentLine = 0;
            WaitTimer = 0;
            ActionTimer = 0;
            return;
        }
    }
    AdvanceLine();
}

void ScriptClass::Action_ChangeMission(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(static_cast<Mission>(argument));
    }
    AdvanceLine();
}

void ScriptClass::Action_Fear(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Retreat);
    }
    AdvanceLine();
}

void ScriptClass::Action_Retreat(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Retreat);
    }
    AdvanceLine();
}

void ScriptClass::Action_Scatter(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Retreat);
    }
    AdvanceLine();
}

void ScriptClass::Action_Stop(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Stop);
    }
    AdvanceLine();
}

void ScriptClass::Action_Sleep(int32 argument) {
    WaitTimer = argument;
    AdvanceLine();
}

void ScriptClass::Action_Group(int32 argument) {
    if (Team) {
        Team->ReGroup();
    }
    AdvanceLine();
}

void ScriptClass::Action_Recruit(int32 argument) {
    if (Team && Team->CanRecruit()) {
        Team->ReinforceTeam(argument);
    }
    AdvanceLine();
}

void ScriptClass::Action_Flash(int32 argument) {
    if (Team) {
        for (int32 i = 0; i < 5; ++i) {
        }
    }
    AdvanceLine();
}

void ScriptClass::Action_LoadOntoTransports(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Enter);
    }
    AdvanceLine();
}

void ScriptClass::Action_Chronominimum(int32 argument) {
    if (Team) {
        // Chrono minimum range behaviour - use the ChronoSphere mission as
        // the closest available chrono-related mission enum value.
        Team->AssignMissionToAll(Mission::ChronoSphere);
    }
    AdvanceLine();
}

void ScriptClass::Action_ChronoMaximum(int32 argument) {
    if (Team) {
        // Chrono maximum range behaviour - use the ChronoSphere mission as
        // the closest available chrono-related mission enum value.
        Team->AssignMissionToAll(Mission::ChronoSphere);
    }
    AdvanceLine();
}

void ScriptClass::Action_ForceMove(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Move);
    }
    AdvanceLine();
}

void ScriptClass::Action_Circle(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Circle);
    }
    AdvanceLine();
}

void ScriptClass::Action_SearchAndDestroy(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Hunt);
    }
    AdvanceLine();
}

void ScriptClass::Action_Harmless(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Harmless);
    }
    AdvanceLine();
}

void ScriptClass::Action_Suicide(int32 argument) {
    if (Team) {
        // Suicide mission: a one-shot kamikaze attack using the dedicated
        // SelfDestruct mission enum value.
        Team->AssignMissionToAll(Mission::SelfDestruct);
    }
    AdvanceLine();
}

void ScriptClass::Action_Recycle(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Recycle);
    }
    AdvanceLine();
}

void ScriptClass::Action_Repeat(int32 argument) {
    if (RepeatCount < ScriptType->GetTotalActionCount()) {
        ++RepeatCount;
        CurrentLine = argument;
        return;
    }
    RepeatCount = 0;
    AdvanceLine();
}

void ScriptClass::Action_Protect(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Guard);
    }
    AdvanceLine();
}

void ScriptClass::Action_Sticky(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Sticky);
    }
    AdvanceLine();
}

void ScriptClass::Action_Emergency(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Emergency);
    }
    AdvanceLine();
}

void ScriptClass::Action_TakeCover(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::TakeCover);
    }
    AdvanceLine();
}

void ScriptClass::Action_Gibber(int32 argument) {
    if (Team) {
        Team->AssignMissionToAll(Mission::Gibber);
    }
    AdvanceLine();
}

void ScriptClass::Action_IronCurtainMe(int32 argument) {
    if (Team) {
        // Apply the IronCurtain mission to the calling team itself.
        Team->AssignMissionToAll(Mission::IronCurtain);
    }
    AdvanceLine();
}

void ScriptClass::Action_ChronoSphereMe(int32 argument) {
    if (Team) {
        // Apply the ChronoSphere mission to the calling team itself.
        Team->AssignMissionToAll(Mission::ChronoSphere);
    }
    AdvanceLine();
}

void ScriptClass::Action_Win(int32 argument) {
    IsScriptComplete = true;
    IsScriptActive = false;
}

void ScriptClass::Action_Lose(int32 argument) {
    IsScriptComplete = true;
    IsScriptActive = false;
}

void ScriptClass::AdvanceLine() {
    ++CurrentLine;
    if (CurrentLine >= ScriptType->GetTotalActionCount()) {
        CurrentLine = ScriptType->GetTotalActionCount();
        IsScriptComplete = true;
    }
}

void ScriptClass::Reset() {
    CurrentLine = 0;
    CurrentAction = 0;
    IsScriptActive = true;
    IsScriptComplete = false;
    WaitTimer = 0;
    ActionTimer = 0;
    RepeatCount = 0;
    LoopCount = 0;
    FailedAttempts = 0;
}

bool ScriptClass::IsComplete() const {
    return IsScriptComplete;
}

void ScriptClass::SetTeam(TeamClass* pTeam) {
    Team = pTeam;
}

TeamClass* ScriptClass::GetTeam() const {
    return Team;
}

int32 ScriptClass::GetAction() const {
    return CurrentAction;
}

int32 ScriptClass::GetLine() const {
    return CurrentLine;
}

bool ScriptClass::IsActive() const {
    return IsScriptActive;
}

void ScriptClass::SetActive(bool active) {
    IsScriptActive = active;
}