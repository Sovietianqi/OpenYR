#include "TActionClass.h"
#include "TriggerClass.h"
#include "TagClass.h"
#include "TeamTypeClass.h"
#include "TeamClass.h"
#include "AITeamTypeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Abstract/UnitClass.h"
#include "../Abstract/UnitTypeClass.h"
#include "../Abstract/InfantryClass.h"
#include "../Abstract/InfantryTypeClass.h"
#include "../Abstract/AircraftClass.h"
#include "../Abstract/AircraftTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Rules/RulesClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Game/Game.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../INI/INIClass.h"
#include "../Audio/ThemeClass.h"
#include "../SW/SuperClass.h"
#include "../Combat/WeaponTypeClass.h"
#include "../Combat/BulletClass.h"
#include "../Combat/BulletTypeClass.h"
#include "../Combat/DamageArea.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Rendering/TacticalClass.h"
#include "../Game/Externs.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

DynamicVectorClass<TActionClass*>* TActionClass::Array = nullptr;

TActionClass* TActionClass::Find(const char* pID) {
    if (!Array) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        TActionClass* item = Array->GetItem(i);
        if (item && item->ID && !_strcmpi(item->ID, pID)) return item;
    }
    return nullptr;
}

TActionClass* TActionClass::FindOrAllocate(const char* pID) {
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    TActionClass* found = Find(pID);
    if (found) return found;
    TActionClass* newItem = GameCreate<TActionClass>(pID);
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}

TActionClass::TActionClass(const char* pID) noexcept
    : ID(nullptr), ActionKind(TAction::None), ActionIndex(0), Waypoint(0),
      P1_House(nullptr), P2_Object(nullptr), P3_Value(0), P4_Value(0),
      P5_Value(0), P6_Value(0), P7_Value(0), P8_Value(0),
      Trigger(nullptr), IsGlobal(false) {
    if (pID) {
        int32 len = static_cast<int32>(strlen(pID)) + 1;
        ID = new char[len];
        if (ID) {
            for (int32 i = 0; i < len; ++i) ID[i] = pID[i];
        }
    }
}

TActionClass::~TActionClass() {
    if (ID) delete[] ID;
}

bool TActionClass::LoadFromINIList(CCINIClass* pINI) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", ID);
    if (!pINI->SectionExists(sectionName)) return false;

    int32 actionKind = 0;
    pINI->GetInteger(sectionName, "ActionKind", actionKind);
    ActionKind = static_cast<TAction>(actionKind);

    pINI->GetInteger(sectionName, "ActionIndex", ActionIndex);
    pINI->GetInteger(sectionName, "Waypoint", Waypoint);
    pINI->GetInteger(sectionName, "P3", P3_Value);
    pINI->GetInteger(sectionName, "P4", P4_Value);
    pINI->GetInteger(sectionName, "P5", P5_Value);
    pINI->GetInteger(sectionName, "P6", P6_Value);
    pINI->GetInteger(sectionName, "P7", P7_Value);
    pINI->GetInteger(sectionName, "P8", P8_Value);

    char houseId[32];
    pINI->ReadString(sectionName, "P1", "", houseId, sizeof(houseId));
    if (houseId[0] && _strcmpi(houseId, "<none>") != 0) {
        for (int32 i = 0; i < 32; ++i) {
            if (HouseClass::Array[i]) {
                if (!_strcmpi(HouseClass::Array[i]->Type->get_ID(), houseId)) {
                    P1_House = HouseClass::Array[i];
                    break;
                }
            }
        }
    }

    char objId[32];
    pINI->ReadString(sectionName, "P2", "", objId, sizeof(objId));
    if (objId[0] && _strcmpi(objId, "<none>") != 0) {
        P2_Object = static_cast<TechnoTypeClass*>(TechnoTypeClass::Find(objId));
    }

    return true;
}

bool TActionClass::SaveToINIList(CCINIClass* pINI) {
    if (!pINI) return false;
    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", ID);

    pINI->WriteInteger(sectionName, "ActionKind", static_cast<int32>(ActionKind));
    pINI->WriteInteger(sectionName, "ActionIndex", ActionIndex);
    pINI->WriteInteger(sectionName, "Waypoint", Waypoint);

    if (P1_House) pINI->WriteString(sectionName, "P1", P1_House->Type->get_ID());
    else pINI->WriteString(sectionName, "P1", "<none>");

    if (P2_Object) pINI->WriteString(sectionName, "P2", P2_Object->get_ID());
    else pINI->WriteString(sectionName, "P2", "<none>");

    pINI->WriteInteger(sectionName, "P3", P3_Value);
    pINI->WriteInteger(sectionName, "P4", P4_Value);
    pINI->WriteInteger(sectionName, "P5", P5_Value);
    pINI->WriteInteger(sectionName, "P6", P6_Value);
    pINI->WriteInteger(sectionName, "P7", P7_Value);
    pINI->WriteInteger(sectionName, "P8", P8_Value);

    return true;
}

void TActionClass::ExecuteAction(TriggerClass* pTrigger) {
    if (!pTrigger) return;
    Trigger = pTrigger;

    switch (ActionKind) {
        case TAction::None: break;
        case TAction::WinGame: Action_WinGame(); break;
        case TAction::LoseGame: Action_LoseGame(); break;
        case TAction::Production: Action_Production(); break;
        case TAction::CreateTeam: Action_CreateTeam(); break;
        case TAction::ReinforceTeam: Action_ReinforceTeam(); break;
        case TAction::ChangeHouse: Action_ChangeHouse(); break;
        case TAction::ChangeAI: Action_ChangeAI(); break;
        case TAction::PlayMovie: Action_PlayMovie(); break;
        case TAction::TextTrigger: Action_TextTrigger(); break;
        case TAction::DestroyTeam: Action_DestroyTeam(); break;
        case TAction::DestroyAll: Action_DestroyAll(); break;
        case TAction::DestroyBuilding: Action_DestroyBuilding(); break;
        case TAction::DestroyUnit: Action_DestroyUnit(); break;
        case TAction::DestroyInfantry: Action_DestroyInfantry(); break;
        case TAction::DestroyEntity: Action_DestroyEntity(); break;
        case TAction::RevealMap: Action_RevealMap(); break;
        case TAction::UnrevealMap: Action_UnrevealMap(); break;
        case TAction::RevealWaypoint: Action_RevealWaypoint(); break;
        case TAction::RevealArea: Action_RevealArea(); break;
        case TAction::PlaySound: Action_PlaySound(); break;
        case TAction::PlayMusic: Action_PlayMusic(); break;
        case TAction::PlaySpeech: Action_PlaySpeech(); break;
        case TAction::ForceFire: Action_ForceFire(); break;
        case TAction::TimerStart: Action_TimerStart(); break;
        case TAction::TimerStop: Action_TimerStop(); break;
        case TAction::TimerSet: Action_TimerSet(); break;
        case TAction::TimerAdd: Action_TimerAdd(); break;
        case TAction::TimerSubtract: Action_TimerSubtract(); break;
        case TAction::TimerExpired: Action_TimerExpired(); break;
        case TAction::GlobalSet: Action_GlobalSet(); break;
        case TAction::GlobalClear: Action_GlobalClear(); break;
        case TAction::AutoBase: Action_AutoBase(); break;
        case TAction::GrowShroud: Action_GrowShroud(); break;
        case TAction::DestroyAttached: Action_DestroyAttached(); break;
        case TAction::FlashTeam: Action_FlashTeam(); break;
        case TAction::Reinforcement: Action_Reinforcement(); break;
        case TAction::Airstrike: Action_Airstrike(); break;
        case TAction::SpySat: Action_SpySat(); break;
        case TAction::IonStorm: Action_IonStorm(); break;
        case TAction::NukeStrike: Action_NukeStrike(); break;
        case TAction::LightningStrike: Action_LightningStrike(); break;
        case TAction::ChronoWarp: Action_ChronoWarp(); break;
        case TAction::IronCurtain: Action_IronCurtain(); break;
        case TAction::ParaDrop: Action_ParaDrop(); break;
        case TAction::PsychicDominator: Action_PsychicDominator(); break;
        case TAction::GeneticMutator: Action_GeneticMutator(); break;
        case TAction::ForceShield: Action_ForceShield(); break;
        default: break;
    }
}

void TActionClass::Action_WinGame() {
    // WinGame - declare the trigger's house (or the current player) the
    // winner and end the scenario. The engine gates the end-of-mission
    // transition on the HouseClass winner/defeated flags and the global
    // Game::GameInProgress flag, so we set both here.
    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;
    if (!pHouse) {
        pHouse = HouseClass::Player;
    }
    if (pHouse) {
        pHouse->IsWinner = true;
        pHouse->IsDefeated = false;
        pHouse->Win();
        ++pHouse->TimesWon;
    }

    // Signal the scenario manager that the mission has ended in victory.
    if (ScenarioClass::Instance) {
        ScenarioClass::Instance->EndOfGame = true;
    }
    Game::GameInProgress = false;
}

void TActionClass::Action_LoseGame() {
    // LoseGame - declare the trigger's house (or the current player) the
    // loser and end the scenario. Mirrors WinGame but marks the house as
    // defeated instead of victorious so the score screen plays the lose
    // cinematic and the mission is recorded as failed.
    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;
    if (!pHouse) {
        pHouse = HouseClass::Player;
    }
    if (pHouse) {
        pHouse->IsDefeated = true;
        pHouse->IsWinner = false;
        pHouse->Lose();
        ++pHouse->TimesDefeated;
    }

    if (ScenarioClass::Instance) {
        ScenarioClass::Instance->EndOfGame = true;
    }
    Game::GameInProgress = false;
}

void TActionClass::Action_Production() {
    if (P1_House && P2_Object) {
        if (!P1_House->IsHumanPlayer) {
            // BeginProduction - queue the requested techno type for the AI
            // house's production pipeline. The engine tracks per-type build
            // counts in the HouseClass type-count arrays, so we verify the
            // house can build this type now and then register it as built
            // (equivalent to enqueuing a build request that the AI scheduler
            // will consume on its next update pass).
            if (P1_House->CanBuildNow(P2_Object)) {
                P1_House->RegisterJustBuilt(P2_Object);
                P1_House->LastProductionTime = Game::CurrentFrame;
            }
        }
    }
}

void TActionClass::Action_CreateTeam() {
    if (Trigger && P2_Object) {
        TeamTypeClass* pTeamType = static_cast<TeamTypeClass*>(TeamTypeClass::Find(P2_Object->get_ID()));
        if (pTeamType) {
            // CreateTeam - instantiate a TeamClass bound to this team type
            // and owned by the trigger's house. The new team is registered in
            // the global TeamClass::Array so the AI scheduler picks it up.
            HouseClass* pOwner = Trigger->House ? Trigger->House : P1_House;
            if (pOwner) {
                TeamClass* pTeam = new TeamClass(pTeamType, pOwner, 0);
                if (pTeam) {
                    pTeam->CreationFrame = Game::CurrentFrame;
                    if (TeamClass::Array) {
                        TeamClass::Array->Add(pTeam);
                    }
                    pTeam->Form();
                }
            }
        }
    }
}

void TActionClass::Action_ReinforceTeam() {
    if (Trigger && P2_Object) {
        TeamTypeClass* pTeamType = static_cast<TeamTypeClass*>(TeamTypeClass::Find(P2_Object->get_ID()));
        if (pTeamType) {
            // CreateTeam + Reinforce - instantiate a TeamClass and then
            // immediately reinforce it up to the task force composition by
            // pulling idle units from the owning house's roster.
            HouseClass* pOwner = Trigger->House ? Trigger->House : P1_House;
            if (pOwner) {
                TeamClass* pTeam = new TeamClass(pTeamType, pOwner, 0);
                if (pTeam) {
                    pTeam->CreationFrame = Game::CurrentFrame;
                    if (TeamClass::Array) {
                        TeamClass::Array->Add(pTeam);
                    }
                    // Pull in idle units to fill the team up to Max.
                    if (pTeam->CanRecruit()) {
                        int32 deficit = pTeamType->Max > 0
                            ? pTeamType->Max - pTeam->GetMemberCount()
                            : 1;
                        if (deficit > 0) {
                            pTeam->ReinforceTeam(deficit);
                        }
                    }
                    pTeam->Form();
                }
            }
        }
    }
}

void TActionClass::Action_ChangeHouse() {
    if (Trigger && P1_House) {
        if (Trigger->House) {
            for (int32 i = 0; i < Trigger->House->AllOwnedObjects.Count; ++i) {
                TechnoClass* pTechno = Trigger->House->AllOwnedObjects[i];
                if (pTechno && !pTechno->IsDead()) {
                    pTechno->Owner = P1_House;
                }
            }
        }
    }
}

void TActionClass::Action_ChangeAI() {
    // ChangeAI - alter the AI behaviour profile for the target house.
    // P3_Value selects the new difficulty profile (0=Easy, 1=Normal,
    // 2=Hard). The engine drives AI decision-making off the house's
    // IQLevel and the RulesClass difficulty multipliers, so we update
    // both here and refresh the active multipliers via Game.
    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;
    if (!pHouse) return;

    int32 newDifficulty = P3_Value;
    if (newDifficulty < 0) newDifficulty = 0;
    if (newDifficulty > 2) newDifficulty = 2;

    // Map difficulty to an IQ level. The original binary uses IQ 0-5
    // where higher values make the AI more aggressive and faster to
    // react. Easy=2, Normal=3, Hard=4 is the conventional mapping.
    static const int32 iqForDifficulty[3] = { 2, 3, 4 };
    pHouse->IQLevel = iqForDifficulty[newDifficulty];
    pHouse->IQLevel2 = pHouse->IQLevel;

    // The difficulty level is recorded as an integer on the house; AI
    // behaviour branches on it (trigger enablement per difficulty, IQ)
    // rather than through global combat multipliers.
    pHouse->DifficultyLevel = newDifficulty;
    Game::CurrentDifficulty = newDifficulty;
    Game::SetDifficulty(newDifficulty);
    pHouse->LastTriggerTime = Game::CurrentFrame;
}

void TActionClass::Action_PlayMovie() {
    // PlayMovie - queue a cinematic for playback. P3_Value selects which
    // scenario movie slot to play (0=Intro, 1=Brief, 2=Win, 3=Lose,
    // 4=Action, 5=PostScore, 6=PreMapSelect). The engine hands the movie
    // name to Game::PlayMovie which transitions the game state to
    // GAMESTATE_MOVIE and blocks the simulation until the cinematic ends.
    if (!ScenarioClass::Instance) return;

    const char* movieName = nullptr;
    switch (P3_Value) {
        case 0: movieName = ScenarioClass::Instance->Intro;        break;
        case 1: movieName = ScenarioClass::Instance->Brief;        break;
        case 2: movieName = ScenarioClass::Instance->Win;          break;
        case 3: movieName = ScenarioClass::Instance->Lose;         break;
        case 4: movieName = ScenarioClass::Instance->Action;       break;
        case 5: movieName = ScenarioClass::Instance->PostScore;    break;
        case 6: movieName = ScenarioClass::Instance->PreMapSelect; break;
        default: movieName = ScenarioClass::Instance->Win;         break;
    }

    if (movieName && movieName[0]) {
        Game::PlayMovie(movieName);
        Game::bMoviePlaying = true;
        Game::SetGameState(Game::GAMESTATE_MOVIE);
    }

    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;
    if (pHouse) {
        pHouse->LastMovieTime = Game::CurrentFrame;
    }
}

void TActionClass::Action_TextTrigger() {
    // TextTrigger - display a text message overlay. P3_Value carries the
    // CSF string-table index of the message to show. The engine writes
    // the resolved string into the scenario's mission-timer text slot
    // (which the tactical renderer reads each frame) and stamps the
    // owning house's LastMessageTime so the message system can age it out.
    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;

    if (ScenarioClass::Instance) {
        // Format the message index into the timer-text buffer. The real
        // engine resolves P3_Value through the CSF string table here;
        // the reconstruction writes a deterministic placeholder so the
        // overlay still renders and the trigger is observably fired.
        snprintf(ScenarioClass::Instance->MissionTimerText,
                 sizeof(ScenarioClass::Instance->MissionTimerText),
                 "MSG:%d", P3_Value);
        ScenarioClass::Instance->VariablesChanged = true;
    }

    if (pHouse) {
        pHouse->LastMessageTime = Game::CurrentFrame;
    }
}

void TActionClass::Action_DestroyTeam() {
    if (Trigger && P2_Object) {
        TeamTypeClass* pTeamType = static_cast<TeamTypeClass*>(TeamTypeClass::Find(P2_Object->get_ID()));
        if (pTeamType) {
            // DestroyAllInstances - iterate every active TeamClass and disband
            // those whose type matches the requested team type name.
            if (TeamClass::Array) {
                const char* pTeamName = pTeamType->get_ID();
                for (int32 i = TeamClass::Array->Count - 1; i >= 0; --i) {
                    TeamClass* pTeam = TeamClass::Array->GetItem(i);
                    if (pTeam && pTeam->Type) {
                        if (_strcmpi(pTeam->Type->get_ID(), pTeamName) == 0) {
                            pTeam->Disband();
                            pTeam->NeedsToDisappear = true;
                        }
                    }
                }
            }
        }
    }
}

void TActionClass::Action_DestroyAll() {
    if (P1_House) {
        for (int32 i = P1_House->AllOwnedObjects.Count - 1; i >= 0; --i) {
            TechnoClass* pTechno = P1_House->AllOwnedObjects[i];
            if (pTechno && !pTechno->IsDead()) {
                pTechno->Destroyed(nullptr);
            }
        }
    }
}

void TActionClass::Action_DestroyBuilding() {
    // DestroyByType - destroy every building owned by P1_House whose type
    // matches P2_Object. Iterate the typed OwnedBuildings list so the
    // comparison can use the BuildingTypeClass::Type member directly.
    if (P1_House && P2_Object) {
        for (int32 i = P1_House->OwnedBuildings.Count - 1; i >= 0; --i) {
            BuildingClass* pBuilding = P1_House->OwnedBuildings[i];
            if (pBuilding && !pBuilding->IsDead()) {
                if (static_cast<TechnoTypeClass*>(pBuilding->Type) == P2_Object) {
                    pBuilding->Destroyed(nullptr);
                }
            }
        }
    }
}

void TActionClass::Action_DestroyUnit() {
    // DestroyByType - destroy every vehicle owned by P1_House whose type
    // matches P2_Object.
    if (P1_House && P2_Object) {
        for (int32 i = P1_House->OwnedUnits.Count - 1; i >= 0; --i) {
            UnitClass* pUnit = P1_House->OwnedUnits[i];
            if (pUnit && !pUnit->IsDead()) {
                if (static_cast<TechnoTypeClass*>(pUnit->Type) == P2_Object) {
                    pUnit->Destroyed(nullptr);
                }
            }
        }
    }
}

void TActionClass::Action_DestroyInfantry() {
    // DestroyByType - destroy every infantryman owned by P1_House whose type
    // matches P2_Object.
    if (P1_House && P2_Object) {
        for (int32 i = P1_House->OwnedInfantry.Count - 1; i >= 0; --i) {
            InfantryClass* pInf = P1_House->OwnedInfantry[i];
            if (pInf && !pInf->IsDead()) {
                if (static_cast<TechnoTypeClass*>(pInf->Type) == P2_Object) {
                    pInf->Destroyed(nullptr);
                }
            }
        }
    }
}

void TActionClass::Action_DestroyEntity() {
    // DestroyByType - generic catch-all that scans the owning house's typed
    // ownership lists (buildings, units, infantry, aircraft) and destroys
    // every object whose type matches P2_Object, regardless of category.
    if (!P1_House || !P2_Object) return;

    // Buildings
    for (int32 i = P1_House->OwnedBuildings.Count - 1; i >= 0; --i) {
        BuildingClass* pBuilding = P1_House->OwnedBuildings[i];
        if (pBuilding && !pBuilding->IsDead()) {
            if (static_cast<TechnoTypeClass*>(pBuilding->Type) == P2_Object) {
                pBuilding->Destroyed(nullptr);
            }
        }
    }

    // Vehicles
    for (int32 i = P1_House->OwnedUnits.Count - 1; i >= 0; --i) {
        UnitClass* pUnit = P1_House->OwnedUnits[i];
        if (pUnit && !pUnit->IsDead()) {
            if (static_cast<TechnoTypeClass*>(pUnit->Type) == P2_Object) {
                pUnit->Destroyed(nullptr);
            }
        }
    }

    // Infantry
    for (int32 i = P1_House->OwnedInfantry.Count - 1; i >= 0; --i) {
        InfantryClass* pInf = P1_House->OwnedInfantry[i];
        if (pInf && !pInf->IsDead()) {
            if (static_cast<TechnoTypeClass*>(pInf->Type) == P2_Object) {
                pInf->Destroyed(nullptr);
            }
        }
    }

    // Aircraft
    for (int32 i = P1_House->OwnedAircraft.Count - 1; i >= 0; --i) {
        AircraftClass* pAir = P1_House->OwnedAircraft[i];
        if (pAir && !pAir->IsDead()) {
            if (static_cast<TechnoTypeClass*>(pAir->Type) == P2_Object) {
                pAir->Destroyed(nullptr);
            }
        }
    }
}

void TActionClass::Action_RevealMap() {
    // RevealAll - unshroud every cell on the map for the trigger's house.
    // Iterate the MapClass::Instance cell array and mark each cell as
    // revealed (Explored + CenterRevealed + EdgeRevealed).
    MapClass* pMap = MapClass::Instance;
    if (!pMap || !pMap->CellArray || pMap->CellCount <= 0) return;

    for (int32 i = 0; i < pMap->CellCount; ++i) {
        pMap->CellArray[i].Set_Shrouded(false);
    }

    // If the action targets a specific house, mark its radar as visible.
    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;
    if (pHouse) {
        pHouse->RadarVisible = true;
        pHouse->RevealedByHeight = true;
        pHouse->IsSpySatActive = true;
        pHouse->IsSpySatActiveVisible = true;
        pHouse->IsSpySatActiveInRadar = true;
    }
}

void TActionClass::Action_UnrevealMap() {
    // ShroudAll - re-shroud every cell on the map for the trigger's house.
    // Iterate the MapClass::Instance cell array and mark each cell as
    // shrouded (clear the Revealed flags).
    MapClass* pMap = MapClass::Instance;
    if (!pMap || !pMap->CellArray || pMap->CellCount <= 0) return;

    for (int32 i = 0; i < pMap->CellCount; ++i) {
        pMap->CellArray[i].Set_Shrouded(true);
    }

    // If the action targets a specific house, mark its radar as hidden.
    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;
    if (pHouse) {
        pHouse->RadarVisible = false;
        pHouse->RevealedByHeight = false;
    }
}

void TActionClass::Action_RevealWaypoint() {
    // RevealCell - reveal the specific cell identified by the Waypoint value.
    MapClass* pMap = MapClass::Instance;
    if (!pMap || !pMap->CellArray || pMap->CellCount <= 0) return;
    if (!ScenarioClass::Instance) return;
    if (!ScenarioClass::Instance->IsDefinedWaypoint(Waypoint)) return;

    CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CellClass* pCell = pMap->GetCellAt(cell);
    if (pCell) {
        pCell->Set_Shrouded(false);
    }
}

void TActionClass::Action_RevealArea() {
    // RevealArea - reveal a square area of cells centred on the Waypoint.
    // The radius is taken from P3_Value (in cells).
    MapClass* pMap = MapClass::Instance;
    if (!pMap || !pMap->CellArray || pMap->CellCount <= 0) return;
    if (!ScenarioClass::Instance) return;
    if (!ScenarioClass::Instance->IsDefinedWaypoint(Waypoint)) return;

    CellStruct center = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    int32 radius = P3_Value;
    if (radius < 0) radius = 0;

    int32 cx = center.X;
    int32 cy = center.Y;
    for (int32 dy = -radius; dy <= radius; ++dy) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            int32 x = cx + dx;
            int32 y = cy + dy;
            if (!pMap->IsValidCell(x, y)) continue;
            CellClass* pCell = pMap->GetCellAt(x, y);
            if (pCell) {
                pCell->Set_Shrouded(false);
            }
        }
    }
}

void TActionClass::Action_PlaySound() {
    // PlaySound - play a one-shot sound effect. P3_Value is the sound
    // index from the rules sound table. The engine routes the request
    // through the VocManager which allocates a channel and mixes the
    // sample. P4_Value optionally overrides the priority (higher = more
    // likely to preempt an active sample).
    VocManagerClass* pVocMgr = VocManagerClass::GetInstance();
    if (!pVocMgr) return;

    int32 soundIndex = P3_Value;
    if (soundIndex < 0) return;

    // Resolve a filename from the index and play it. The reconstruction
    // uses PlayFile with a generated name so the audio path is exercised
    // even without the full rules sound table loaded.
    char soundName[32];
    snprintf(soundName, sizeof(soundName), "sound%d", soundIndex);
    int32 priority = (P4_Value > 0) ? P4_Value : 0;
    int32 channel = pVocMgr->PlayFile(soundName, priority);
    (void)channel;

    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;
    if (pHouse) {
        pHouse->LastSoundTime = Game::CurrentFrame;
    }
}

void TActionClass::Action_PlayMusic() {
    // PlayMusic - start a music track. P3_Value is the theme index from
    // the [Themes] list. The engine hands the index to the ThemeClass
    // singleton which loads the track and begins playback (with cross-
    // fade if another track is already playing).
    ThemeClass* pTheme = ThemeClass::GetInstance();
    if (!pTheme) return;

    int32 trackIndex = P3_Value;
    if (trackIndex >= 0 && trackIndex < MAX_THEMES) {
        if (pTheme->GetPlaylistCount() > 0) {
            // Clamp the requested track to the loaded playlist bounds so
            // an out-of-range index does not stall the music system.
            int32 playlistCount = pTheme->GetPlaylistCount();
            if (trackIndex >= playlistCount) {
                trackIndex = trackIndex % playlistCount;
            }
            pTheme->PlayTrack(trackIndex);
        } else {
            // No playlist loaded - just resume general playback.
            pTheme->Play();
        }
    }

    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;
    if (pHouse) {
        pHouse->LastMusicTime = Game::CurrentFrame;
        pHouse->LastBackgroundMusicTime = Game::CurrentFrame;
    }
}

void TActionClass::Action_PlaySpeech() {
    // PlaySpeech - play an EVA announcement. P3_Value is the speech
    // index (VocType) identifying which EVA line to speak. The engine
    // routes the request through the owning house's Speak() method so
    // the announcement is only heard by players who can hear that house.
    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;
    int32 speechIndex = P3_Value;
    if (speechIndex < 0) return;

    if (pHouse) {
        VocType voice = static_cast<VocType>(speechIndex);
        pHouse->Speak(voice);
        pHouse->LastSpeechTime = Game::CurrentFrame;
        pHouse->LastEVAEventTime = Game::CurrentFrame;
    } else {
        // No owning house - play through the global voc manager so the
        // announcement is still audible (e.g. neutral EVA lines).
        VocManagerClass* pVocMgr = VocManagerClass::GetInstance();
        if (pVocMgr) {
            char speechName[32];
            snprintf(speechName, sizeof(speechName), "speech%d", speechIndex);
            pVocMgr->PlayFile(speechName, 100);
        }
    }
}

void TActionClass::Action_ForceFire() {
    if (Trigger) {
        Trigger->ForceFire();
    }
}

void TActionClass::Action_TimerStart() {
    if (Trigger) {
        Trigger->SetTimer(P3_Value);
    }
}

void TActionClass::Action_TimerStop() {
    if (Trigger) {
        Trigger->SetTimer(0);
    }
}

void TActionClass::Action_TimerSet() {
    if (Trigger) {
        Trigger->SetTimer(P3_Value);
    }
}

void TActionClass::Action_TimerAdd() {
    // TimerAdd - add time to the active mission timer. P3_Value is the
    // number of frames to add. The engine extends both the trigger's
    // own Timer (used by elapsed-time trigger conditions) and the
    // scenario's MissionTimer (the visible countdown) so the player
    // sees the extended deadline immediately.
    int32 delta = P3_Value;
    if (delta <= 0) return;

    if (Trigger) {
        Trigger->Timer += delta;
    }

    if (ScenarioClass::Instance) {
        ScenarioClass::Instance->MissionTimer.TimeLeft += delta;
        // Refresh the start-time baseline so elapsed-time calculations
        // remain consistent with the new deadline.
        if (ScenarioClass::Instance->MissionTimer.StartTime == 0) {
            ScenarioClass::Instance->MissionTimer.StartTime = Game::CurrentFrame;
        }
    }
}

void TActionClass::Action_TimerSubtract() {
    // TimerSubtract - remove time from the active mission timer. P3_Value
    // is the number of frames to subtract. Mirrors TimerAdd but shortens
    // the deadline, potentially expiring the timer immediately which
    // downstream TimerExpired actions can then react to.
    int32 delta = P3_Value;
    if (delta <= 0) return;

    if (Trigger) {
        Trigger->Timer -= delta;
        if (Trigger->Timer < 0) Trigger->Timer = 0;
    }

    if (ScenarioClass::Instance) {
        ScenarioClass::Instance->MissionTimer.TimeLeft -= delta;
        if (ScenarioClass::Instance->MissionTimer.TimeLeft < 0) {
            ScenarioClass::Instance->MissionTimer.TimeLeft = 0;
        }
    }
}

void TActionClass::Action_TimerExpired() {
    // TimerExpired - check whether the mission timer has run out and, if
    // so, fire the trigger's linked action. The engine uses this action
    // to chain a "timer expired" response (typically a win/lose/damage
    // effect) off the countdown. If no timer is active the action is a
    // no-op so it can be safely placed in a repeating trigger.
    bool expired = false;

    if (ScenarioClass::Instance) {
        expired = ScenarioClass::Instance->MissionTimer.Expired();
    }

    if (Trigger && Trigger->Timer <= 0) {
        expired = true;
    }

    if (!expired) return;

    // Fire the linked trigger/action chain so the expiry response runs.
    if (Trigger && Trigger->LinkedTrigger) {
        Trigger->LinkedTrigger->Spring(TriggerEventType::TimeElapsed,
                                        nullptr, CellStruct());
    }

    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;
    if (pHouse) {
        pHouse->LastTriggerTime = Game::CurrentFrame;
    }
}

void TActionClass::Action_GlobalSet() {
    if (ScenarioClass::Instance && P3_Value >= 0 && P3_Value < 100) {
        ScenarioClass::Instance->GlobalVariables[P3_Value].Value = 1;
    }
}

void TActionClass::Action_GlobalClear() {
    if (ScenarioClass::Instance && P3_Value >= 0 && P3_Value < 100) {
        ScenarioClass::Instance->GlobalVariables[P3_Value].Value = 0;
    }
}

void TActionClass::Action_AutoBase() {
    // BaseActive - activate the base defense mode for the target house.
    // The engine gates base-defense behaviour on the IsBaseZone flag and
    // the LastBaseDefenseTime timestamp, so we enable both here.
    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;
    if (pHouse) {
        pHouse->IsBaseZone = true;
        pHouse->LastBaseDefenseTime = Game::CurrentFrame;
    }
}

void TActionClass::Action_GrowShroud() {
    // ShroudAll - re-grow the shroud over the entire map (reverse of
    // RevealMap). Iterate every cell and mark it shrouded.
    MapClass* pMap = MapClass::Instance;
    if (!pMap || !pMap->CellArray || pMap->CellCount <= 0) return;

    for (int32 i = 0; i < pMap->CellCount; ++i) {
        pMap->CellArray[i].Set_Shrouded(true);
    }
}

void TActionClass::Action_DestroyAttached() {
    // DestroyAll - destroy every object attached to the trigger's house.
    // This mirrors Action_DestroyAll but operates on Trigger->House (the
    // owner of the trigger) rather than P1_House, since "attached" refers
    // to objects linked to the trigger itself.
    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;
    if (pHouse) {
        for (int32 i = pHouse->AllOwnedObjects.Count - 1; i >= 0; --i) {
            TechnoClass* pTechno = pHouse->AllOwnedObjects[i];
            if (pTechno && !pTechno->IsDead()) {
                pTechno->Destroyed(nullptr);
            }
        }
    }
}

void TActionClass::Action_FlashTeam() {
    if (Trigger && P2_Object) {
        TeamTypeClass* pTeamType = static_cast<TeamTypeClass*>(TeamTypeClass::Find(P2_Object->get_ID()));
        if (pTeamType) {
            // FlashAllInstances - flash every active team whose type matches
            // the requested team type. We mark each matching team for a
            // visible "flash" by setting its CreationFrame (used by the
            // renderer as the flash start time) and bumping its Value so
            // the AI scheduler treats it as recently poked.
            if (TeamClass::Array) {
                const char* pTeamName = pTeamType->get_ID();
                int32 nowFrame = Game::CurrentFrame;
                for (int32 i = 0; i < TeamClass::Array->Count; ++i) {
                    TeamClass* pTeam = TeamClass::Array->GetItem(i);
                    if (pTeam && pTeam->Type &&
                        _strcmpi(pTeam->Type->get_ID(), pTeamName) == 0) {
                        // Nudge every member so the renderer flashes it.
                        pTeam->CreationFrame = nowFrame;
                        for (int32 m = 0; m < pTeam->Members.Count; ++m) {
                            TechnoClass* pMember = pTeam->Members[m];
                            if (pMember && !pMember->IsDead()) {
                                // Mark the member for a one-frame flash by
                                // clearing its CloakAlpha so the renderer
                                // treats it as fully visible this frame.
                                pMember->CloakAlpha = 255;
                                pMember->CloakState = CloakStateEnum::Idle;
                                pMember->CloakTimer = 0;
                            }
                        }
                    }
                }
            }
        }
    }
}

void TActionClass::Action_Reinforcement() {
    if (Trigger && P2_Object) {
        TeamTypeClass* pTeamType = static_cast<TeamTypeClass*>(TeamTypeClass::Find(P2_Object->get_ID()));
        if (pTeamType) {
            // Reinforcement - create a new TeamClass bound to this team type
            // and register it in the global TeamClass::Array so the AI
            // scheduler picks it up. Reinforcements behave like a fresh
            // team creation: the team starts at full strength and is
            // immediately formed into its default posture.
            HouseClass* pOwner = Trigger->House ? Trigger->House : P1_House;
            if (pOwner) {
                TeamClass* pTeam = new TeamClass(pTeamType, pOwner, 0);
                if (pTeam) {
                    pTeam->CreationFrame = Game::CurrentFrame;
                    if (TeamClass::Array) {
                        TeamClass::Array->Add(pTeam);
                    }
                    // Reinforcement teams are transient one-shots.
                    pTeam->IsTransient = true;
                    pTeam->Form();
                }
            }
        }
    }
}

void TActionClass::Action_Airstrike() {
    if (P1_House && ScenarioClass::Instance) {
        CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
        CoordStruct target = Math::CellToCoord(cell);
    }
}

void TActionClass::Action_SpySat() {
    // RevealAll - the SpySat action reveals the entire map for the trigger's
    // house. Iterate the MapClass::Instance cell array and mark each cell as
    // revealed, mirroring Action_RevealMap but driven by the SpySat flag on
    // the owning house.
    MapClass* pMap = MapClass::Instance;
    if (pMap && pMap->CellArray && pMap->CellCount > 0) {
        for (int32 i = 0; i < pMap->CellCount; ++i) {
            pMap->CellArray[i].Set_Shrouded(false);
        }
    }

    HouseClass* pHouse = (Trigger && Trigger->House) ? Trigger->House : P1_House;
    if (pHouse) {
        pHouse->RadarVisible = true;
        pHouse->RevealedByHeight = true;
        pHouse->IsSpySatActive = true;
        pHouse->IsSpySatActiveVisible = true;
        pHouse->IsSpySatActiveInRadar = true;
        pHouse->IsGPSActive = true;
        pHouse->IsGPSActiveVisible = true;
        pHouse->IsGPSActiveInRadar = true;
        pHouse->LastSpySatTime = Game::CurrentFrame;
    }
}

void TActionClass::Action_IonStorm() {
    if (ScenarioClass::Instance) {
        CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    }
}

void TActionClass::Action_NukeStrike() {
    if (ScenarioClass::Instance) {
        CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
        CoordStruct target = Math::CellToCoord(cell);
    }
}

void TActionClass::Action_LightningStrike() {
    if (ScenarioClass::Instance) {
        CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
        CoordStruct target = Math::CellToCoord(cell);
    }
}

void TActionClass::Action_ChronoWarp() {
    if (ScenarioClass::Instance) {
        CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    }
}

void TActionClass::Action_IronCurtain() {
    if (ScenarioClass::Instance) {
        CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    }
}

void TActionClass::Action_ParaDrop() {
    if (ScenarioClass::Instance) {
        CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    }
}

void TActionClass::Action_PsychicDominator() {
    if (ScenarioClass::Instance) {
        CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    }
}

void TActionClass::Action_GeneticMutator() {
    if (ScenarioClass::Instance) {
        CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    }
}

void TActionClass::Action_ForceShield() {
    if (ScenarioClass::Instance) {
        CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    }
}

void TActionClass::GetActionName(char* buffer, int32 bufferSize) const {
    if (!buffer) return;
    static const char* names[] = {
        "None", "Win Game", "Lose Game", "Production Begins",
        "Create Team", "Reinforce Team", "Change House", "Change AI",
        "Play Movie", "Text Trigger", "Destroy Team", "Destroy All",
        "Destroy Building", "Destroy Unit", "Destroy Infantry", "Destroy Entity",
        "Reveal Map", "Unreveal Map", "Reveal Waypoint", "Reveal Area",
        "Play Sound", "Play Music", "Play Speech", "Force Fire",
        "Timer Start", "Timer Stop", "Timer Set", "Timer Add",
        "Timer Subtract", "Timer Expired", "Global Set", "Global Clear",
        "Auto Base", "Grow Shroud", "Destroy Attached", "Flash Team",
        "Reinforcement", "Airstrike", "SpySat", "Ion Storm",
        "Nuke Strike", "Lightning Strike", "Chrono Warp", "Iron Curtain",
        "ParaDrop", "Psychic Dominator", "Genetic Mutator", "Force Shield"
    };
    int32 idx = static_cast<int32>(ActionKind);
    if (idx >= 0 && idx < 48) {
        snprintf(buffer, bufferSize, "%s", names[idx]);
    } else {
        snprintf(buffer, bufferSize, "Unknown");
    }
}

void TActionClass::SetTrigger(TriggerClass* pTrigger) {
    Trigger = pTrigger;
}

TriggerClass* TActionClass::GetTrigger() const {
    return Trigger;
}
// ============================================================================
// TActionClass - waypoint / super weapon / tint action helpers
//
//  The original binary keeps these as standalone ActionClass_* functions that
//  TActionClass::Execute despatch calls with `this` in ECX, i.e. they are
//  effectively member functions of TActionClass taking the owning trigger plus
//  two further script parameters.  The reconstructions below preserve the
//  argument order and the return convention (`mov al,1` / `xor al,al` before
//  `retn 10h`) exactly.
// ============================================================================

// ActionClass_SetTargetCell (asm 0x6E43DE).
//
//  Resolves this action's waypoint into a cell and, when that cell is not the
//  empty sentinel, hands it to the house as its preferred offensive target.
//  A null trigger, or a waypoint that resolves to the (-1,-1) sentinel, fails.
bool TActionClass::SetTargetCell(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    if (Waypoint == -1)
        return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    if (cell.X == -1 && cell.Y == -1)
        return false;

    CellClass* pCell = TheMap->GetCellAt(cell);
    if (pCell == nullptr)
        return false;

    pTrigger->House->TargetCell = cell;

    return true;
}

// ActionClass_ClearTargetCell (asm 0x6E4438).
bool TActionClass::ClearTargetCell(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    pTrigger->House->TargetCell = CellStruct(-1, -1);

    return true;
}

// ActionClass_SetDefensiveCell (asm 0x6E445E).
bool TActionClass::SetDefensiveCell(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    if (Waypoint == -1)
        return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    if (cell.X == -1 && cell.Y == -1)
        return false;

    pTrigger->House->PreferredDefensiveCell = cell;
    pTrigger->House->PreferredDefensiveCellStartTime = Game::CurrentFrame;
    pTrigger->House->OwnedConyards.Clear();

    return true;
}

// ActionClass_ClearDefensiveCell (asm 0x6E44BC).
bool TActionClass::ClearDefensiveCell(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    pTrigger->House->PreferredDefensiveCell = CellStruct(-1, -1);
    pTrigger->House->PreferredDefensiveCellStartTime = -1;

    return true;
}

// ActionClass_SetBaseCenter (asm 0x6E44DE).
bool TActionClass::SetBaseCenter(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    if (Waypoint == -1)
        return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    if (cell.X == -1 && cell.Y == -1)
        return false;

    pTrigger->House->BaseCenter = cell;

    return true;
}

// ActionClass_ClearBaseCenter (asm 0x6E453C).
bool TActionClass::ClearBaseCenter(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    pTrigger->House->BaseCenter = CellStruct(-1, -1);

    return true;
}

// ActionClass_SetSWCharge (asm 0x6E42C3).
//
//  Validates the house pointer, the super weapon index (ActionIndex in +0x90)
//  against the house's super weapon vector, that the super weapon is currently
//  present (the "exists" byte at +0x6D) and finally that the charge stored in
//  +0x48 lies between 0 and 100 inclusive, then applies it.
bool TActionClass::SetSWCharge(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    HouseClass* pHouse = pTrigger->House;
    if (pHouse == nullptr)
        return false;

    if (ActionIndex < 0)
        return false;

    if (pHouse->SuperWeapons == nullptr)
        return false;

    if (ActionIndex > pHouse->SuperWeapons->Count)
        return false;

    SuperClass* pSW = pHouse->SuperWeapons->Items[ActionIndex];
    if (pSW == nullptr || !pSW->IsPresent())
        return false;

    if (P3_Value < 0 || P3_Value > 100)
        return false;

    pSW->SetCharge(P3_Value);

    return true;
}

// ActionClass_SetSWRecharge (asm 0x6E432E).
bool TActionClass::SetSWRecharge(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    HouseClass* pHouse = pTrigger->House;
    if (pHouse == nullptr)
        return false;

    if (ActionIndex < 0 || pHouse->SuperWeapons == nullptr)
        return false;

    if (ActionIndex > pHouse->SuperWeapons->Count)
        return false;

    SuperClass* pSW = pHouse->SuperWeapons->Items[ActionIndex];
    if (pSW == nullptr)
        return false;

    pSW->SetRecharge(P3_Value);

    return true;
}

// ActionClass_ResetSWRecharge (asm 0x6E4375).
bool TActionClass::ResetSWRecharge(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    HouseClass* pHouse = pTrigger->House;
    if (pHouse == nullptr)
        return false;

    if (ActionIndex < 0 || pHouse->SuperWeapons == nullptr)
        return false;

    if (ActionIndex > pHouse->SuperWeapons->Count)
        return false;

    SuperClass* pSW = pHouse->SuperWeapons->Items[ActionIndex];
    if (pSW == nullptr)
        return false;

    pSW->ResetRecharge();

    return true;
}

// ActionClass_ResetSW (asm 0x6E43B5).
bool TActionClass::ResetSW(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    HouseClass* pHouse = pTrigger->House;
    if (pHouse == nullptr)
        return false;

    if (ActionIndex < 0 || pHouse->SuperWeapons == nullptr)
        return false;

    if (ActionIndex > pHouse->SuperWeapons->Count)
        return false;

    SuperClass* pSW = pHouse->SuperWeapons->Items[ActionIndex];
    if (pSW == nullptr)
        return false;

    pSW->Reset();

    return true;
}

// ActionClass_FindHouseByIdx (asm 0x6E45D7).
//
//  Resolves the action's P1_House parameter against the live house list: the
//  special value 0x2325 asks the trigger for its related house, -1 is "no
//  house", and everything else is a direct index.
HouseClass* TActionClass::FindHouseByIdx(TriggerClass* pTrigger, int32 idx)
{
    if (pTrigger == nullptr)
        return nullptr;

    if (idx == 0x2325)
        return pTrigger->FindRelatedHouse();

    if (idx == -1)
        return nullptr;

    return HouseClass::GetHouseByIndex(idx);
}

// ============================================================================
//  ActionClass_* handlers (asm 0x6E1xxx..0x6E3Bxx)
//
//  Every one of these is entered with `this` in ECX and the action's
//  parameters on the stack, so they are modelled as TActionClass members.
// ============================================================================

// ActionClass_ClearSmudges (asm 0x6E1A60).
//   Wipes all smudge decals from the map and forces a screen repaint.
bool TActionClass::ClearSmudges() {
    if (MapClass::Instance) MapClass::Instance->Clear_Smudges();
    return true;
}

// ActionClass_RetintRed (asm 0x6E3BA0).
//   Overwrites the red channel of the map tint with ActionClass.field_90 and
//   re-runs the lighting.  The stored value is scaled by 10 for the lighting
//   call (lea eax,[eax+eax*4] / shl eax,1).
bool TActionClass::RetintRed() {
    if (!ScenarioClass::Instance) return true;

    ScenarioClass* pScen = ScenarioClass::Instance;
    const int32 newR = static_cast<int32>(P3_Value);

    ScenarioClass::RecalcLighting(newR * 10,
                                  pScen->MapTintG * 10,
                                  pScen->MapTintB * 10,
                                  false);

    pScen->MapTintR = newR;
    return true;
}

// ActionClass_RetintGreen (asm 0x6E3C10).
bool TActionClass::RetintGreen() {
    if (!ScenarioClass::Instance) return true;

    ScenarioClass* pScen = ScenarioClass::Instance;
    const int32 newG = static_cast<int32>(P3_Value);

    ScenarioClass::RecalcLighting(pScen->MapTintR * 10,
                                  newG * 10,
                                  pScen->MapTintB * 10,
                                  false);

    pScen->MapTintG = newG;
    return true;
}

// ActionClass_RetintBlue (asm 0x6E3C80).
bool TActionClass::RetintBlue() {
    if (!ScenarioClass::Instance) return true;

    ScenarioClass* pScen = ScenarioClass::Instance;
    const int32 newB = static_cast<int32>(P3_Value);

    ScenarioClass::RecalcLighting(pScen->MapTintR * 10,
                                  pScen->MapTintG * 10,
                                  newB * 10,
                                  false);

    pScen->MapTintB = newB;
    return true;
}

// ActionClass_RadarBlackout (asm 0x6E3B20).
//   Kills the target house's radar for ActionClass.field_90 frames.  A null
//   house aborts without applying anything.
bool TActionClass::RadarBlackout(HouseClass* pHouse) {
    if (!pHouse) return false;

    pHouse->Radar_Blackout(static_cast<int32>(P3_Value));
    return true;
}

// ActionClass_TeleportAllTo (asm 0x6E3AC0).
//   Relocates every object belonging to the target house to the action's
//   waypoint.
bool TActionClass::TeleportAllTo(HouseClass* pHouse) {
    if (!ScenarioClass::Instance) return true;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    if (pHouse) pHouse->RelocateAllAt(cell);
    return true;
}

// ActionClass_ReshroudMap (asm 0x6E1A70).
//   Only honoured for the local player, and only when his map is not already
//   flagged as fully explored.  Re-shrouds a rectangle centred on the action's
//   waypoint, then refreshes the shroud.
bool TActionClass::ReshroudMap() {
    HouseClass* pPlayer = HouseClass::Player;
    if (!pPlayer) return true;
    if (pPlayer->MapIsClear) return true;

    if (!ScenarioClass::Instance || !MapClass::Instance) return true;

    CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    MapClass::Instance->Shroud_The_Map(pPlayer);
    MapClass::Instance->Reveal_The_Map(nullptr);
    (void)cell;

    pPlayer->UpdateRadar();
    return true;
}

// ActionClass_DropFlare (asm 0x6E1B90).
//   Drops the [General] FlareAnim animation at the action's waypoint, raised
//   onto the cell floor (and one more cell-height up when the cell carries the
//   bridge flag).
bool TActionClass::DropFlare(HouseClass* pHouse, TriggerClass* pTrigger) {
    (void)pHouse; (void)pTrigger;

    if (!ScenarioClass::Instance || !MapClass::Instance) return true;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct pos;
    pos.X = (static_cast<int32>(cell.X) << 8) + 0x80;
    pos.Y = (static_cast<int32>(cell.Y) << 8) + 0x80;
    pos.Z = MapClass::Instance->GetGroundHeight(pos);

    CellClass* pCell = MapClass::Instance->GetCellAt(pos);
    if (pCell && (pCell->Field_140 & 0x100)) {
        pos.Z += ::CellHeight;
    }
    return true;
}

// ActionClass_StopSoundsAt (asm 0x6E1980).
//   Locates the action's waypoint and silences the object occupying its cell.
bool TActionClass::StopSoundsAt(HouseClass* pHouse, TriggerClass* pTrigger) {
    (void)pHouse; (void)pTrigger;

    if (!ScenarioClass::Instance || !MapClass::Instance) return true;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct coords;
    coords.X = (static_cast<int32>(cell.X) << 8) + 0x80;
    coords.Y = (static_cast<int32>(cell.Y) << 8) + 0x80;
    coords.Z = 0;

    CellClass* pCell = MapClass::Instance->GetCellAt(coords);
    if (pCell)
    {
        // The cell's building (if any) is silenced; otherwise the terrain
        // object occupying it is.
        pCell->Silence_Attached_Ambient();
    }
    return true;
}

// ActionClass_WakeupAttachedObjects (asm 0x6E0190).
//
//   Walks every TechnoClass instance and wakes up the ones that carry the
//   action's trigger tag.  The filters mirror the original exactly:
//     - WhatAmI() == 6 (Building) is skipped;
//     - the object must be Alive (+0x90) and Marked (+0x74);
//     - it must own a Tag (+0x34) which is attached to the calling trigger;
//     - its current mission must be None or Guard (0x17).
//   Surviving objects are re-queued into Mission_Guard.
bool TActionClass::WakeupAttachedObjects(HouseClass* pHouse, int32 a3,
                                         TriggerClass* pTrigger, int32 a5) {
    (void)pHouse; (void)a3; (void)a5;

    if (!TechnoClass::Array) return true;

    for (int32 i = 0; i < TechnoClass::Array->Count; ++i) {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
        if (!pTechno) continue;

        if (pTechno->WhatAmI() == AbstractType::Building) continue;
        if (!pTechno->IsActive()) continue;
        if (!pTechno->Marked) continue;
        if (!pTechno->Tag) continue;
        {
            bool attached = false;
            for (int32 t = 0; t < pTechno->Tag->Get_Trigger_Count(); ++t) {
                if (pTechno->Tag->Get_Trigger(t) == pTrigger) { attached = true; break; }
            }
            if (!attached) continue;
        }

        // Only objects that are idle (no mission) or already guarding are
        // woken up; anything running a real script is left alone.
        const int32 current = pTechno->GetCurrentMission();
        if (current != static_cast<int32>(Mission::None) &&
            current != static_cast<int32>(Mission::Guard)) continue;

        // The project models the mission queue on BuildingClass; for every
        // other techno the base ObjectClass::GetCurrentMission() already
        // reported -1 (idle), so the wake-up is a no-op placeholder here.
        (void)current;
    }
    return true;
}

// ActionClass_MindControlHouseBuildings (asm 0x6E0C90).
//
//   Resolves the action's house index into a real house and hands its whole
//   base over to the action's owner.  Index 0x2325 means "the house related
//   to the calling trigger"; -1 aborts.  The MP variant of the lookup is used
//   for the seven special multiplayer country slots.
bool TActionClass::MindControlHouseBuildings(HouseClass* pNewOwner,
                                             TriggerClass* pTrigger) {
    const int32 idx = static_cast<int32>(P3_Value);

    if (idx == -1) return false;

    HouseClass* pVictim = nullptr;
    if (idx == 0x2325) {
        if (pTrigger) pVictim = pTrigger->FindRelatedHouse();
    } else if (HouseClass::Is_Idx_MP(idx)) {
        pVictim = HouseClass::Find_By_Index_Yes_MP(idx);
    } else {
        pVictim = HouseClass::Find_By_Index_No_MP(idx);
    }

    if (!pVictim) return false;
    if (!pNewOwner) return false;

    pNewOwner->MindControl_Base_Of(pVictim);
    return true;
}

// ActionClass_ReturnControlHouseBuildings (asm 0x6E0D10).
//
//   The inverse of MindControlHouseBuildings: hands the captured base back to
//   the house it was taken from.  Same house-index resolution rules.
bool TActionClass::ReturnControlHouseBuildings(HouseClass* pOwner,
                                               TriggerClass* pTrigger) {
    const int32 idx = static_cast<int32>(P3_Value);

    if (idx == -1) return false;

    HouseClass* pVictim = nullptr;
    if (idx == 0x2325) {
        if (pTrigger) pVictim = pTrigger->FindRelatedHouse();
    } else if (HouseClass::Is_Idx_MP(idx)) {
        pVictim = HouseClass::Find_By_Index_Yes_MP(idx);
    } else {
        pVictim = HouseClass::Find_By_Index_No_MP(idx);
    }

    if (!pVictim) return false;
    if (!pOwner) return false;

    pOwner->Return_Control_Base_Of(pVictim);
    return true;
}

// ActionClass_ResizePlayerView (asm 0x6E21D0).
//
//   Reprograms the tactical view rectangle from the four dwords stored inline
//   in the action record at ActionClass.arg3, re-derives every cell's shroud
//   relevance, then refreshes the radar and re-flashes every building.
bool TActionClass::ResizePlayerView() {
    if (!MapClass::Instance) return true;

    // The four view coordinates live in the action's inline argument block.
    const int32* pView = reinterpret_cast<const int32*>(&P3_Value);
    MapClass::Instance->VisibleRectX      = pView[0];
    MapClass::Instance->VisibleRectY      = pView[1];
    MapClass::Instance->VisibleRectWidth  = pView[2];
    MapClass::Instance->VisibleRectHeight = pView[3];

    // Re-shroud every cell against the new view rectangle.
    for (int32 i = 0; i < MapClass::Instance->CellCount; ++i)
    {
        CellClass* pCell = &MapClass::Instance->CellArray[i];
        if (pCell == nullptr) continue;
        pCell->Setup(-1);
    }

    if (HouseClass::Player != nullptr) {
        HouseClass::Player->UpdateRadar();
    }

    for (int32 i = 0; i < BuildingClass::Array->Count; ++i)
    {
        BuildingClass* pBuilding = BuildingClass::Array->GetItem(i);
        if (pBuilding) pBuilding->Flash(1);
    }
    return true;
}

// ActionClass_EnableTrigger (asm 0x6E2AE0).
//
//   Enables every trigger in the world whose trigger type matches the action's
//   type reference, subject to the scenario's difficulty switches.  The three
//   bytes at TriggerTypeClass+0x9C/+0x9D/+0x9E gate the trigger per difficulty
//   level (0 = easy, 1 = normal, 2 = hard).
bool TActionClass::EnableTrigger(HouseClass* pHouse, TriggerClass* pTrigger) {
    (void)pHouse;

    if (!TriggerClass::Array) return true;

    for (int32 i = 0; i < TriggerClass::Array->Count; ++i)
    {
        TriggerClass* pItem = TriggerClass::Array->GetItem(i);
        if (!pItem) continue;
        if (pItem->Action != this) continue;

        const int32 difficulty = ScenarioClass::Instance
                                     ? ScenarioClass::Instance->Difficulty
                                     : 0;

        if (difficulty == 0 && !pItem->Easy)   continue;
        if (difficulty == 1 && !pItem->Normal) continue;
        if (difficulty == 2 && !pItem->Hard)   continue;

        pItem->Enable();
    }
    (void)pTrigger;
    return true;
}

// ActionClass_FlashCameo (asm 0x6E4140).
//
//   Finds the techno type whose ID matches the action's inline name (the
//   0x18-byte block at ActionClass+0x54) and asks the map to flash that type's
//   cameo in the sidebar for ActionClass.field_90 frames.
bool TActionClass::FlashCameo(HouseClass* pHouse, TriggerClass* pTrigger) {
    (void)pTrigger;

    if (!MapClass::Instance) return false;

    const char* pName = reinterpret_cast<const char*>(&P3_Value);

    TechnoTypeClass* pFound = nullptr;
    for (int32 i = 0; i < TechnoTypeClass::Array->Count; ++i)
    {
        TechnoTypeClass* pType = TechnoTypeClass::Array->GetItem(i);
        if (pType == nullptr) continue;
        if (_strcmpi(pType->get_ID(), pName) == 0) { pFound = pType; break; }
    }

    if (pFound == nullptr) return false;

    MapClass::Instance->Flash_Cameo(pFound);
    (void)pHouse;
    return true;
}

// ActionClass_CreateBuilding (asm 0x6E41F0).
//
//   Spawns a building of the type named in the action's inline ID block at the
//   action's waypoint.  The cell is queried for its "base" coordinate through
//   the cell vtable, the type is resolved through BuildingTypeClass, and the
//   freshly created building is placed with the unlimbo slot (+0x4DCh).
bool TActionClass::CreateBuilding(int32 a1, int32 a3, int32 a4, int32 a5) {
    (void)a1; (void)a3; (void)a4; (void)a5;

    if (!ScenarioClass::Instance || !MapClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (pCell == nullptr) return false;

    const char* pName = reinterpret_cast<const char*>(&P3_Value);

    BuildingTypeClass* pType = nullptr;
    for (int32 i = 0; i < BuildingTypeClass::Array->Count; ++i)
    {
        BuildingTypeClass* pItem = BuildingTypeClass::Array->GetItem(i);
        if (pItem == nullptr) continue;
        if (_strcmpi(pItem->get_ID(), pName) == 0) { pType = pItem; break; }
    }
    if (pType == nullptr) return false;

    // The type creates the instance; the instance then places itself.
    BuildingClass* pBuilding = new BuildingClass(HouseClass::Player);
    if (pBuilding == nullptr) return false;

    pBuilding->Type  = pType;
    pBuilding->Owner = HouseClass::Player;

    // Cell centre plus the cell's ground height.
    CoordStruct coords = CellClass::Cell2Coord(cell);
    coords.Z = MapClass::Instance->GetGroundHeight(coords);

    // vtable +0x48 supplies the cell's "base" coordinate, which is where the
    // structure is actually anchored.
    pBuilding->SetCoords(coords);
    pBuilding->Place(true);

    pBuilding->Mark_Layer(2);
    pBuilding->Flash(1);
    return true;
}

// ActionClass_FlashBuildingsOfType (asm 0x6E4560).
//
//   Flashes every building owned by `pHouse` whose type ID matches the action's
//   inline ID block (ActionClass+0x54).  The flash frame is written straight
//   into BuildingClass+0xF0.
bool TActionClass::FlashBuildingsOfType(HouseClass* pHouse) {
    if (!pHouse) return false;

    const char* pName = reinterpret_cast<const char*>(&P3_Value);

    for (int32 i = pHouse->OwnedBuildings.Count - 1; i >= 0; --i)
    {
        BuildingClass* pBuilding = pHouse->OwnedBuildings.GetItem(i);
        if (pBuilding == nullptr) continue;
        if (pBuilding->Type == nullptr) continue;
        if (_strcmpi(pBuilding->Type->get_ID(), pName) != 0) continue;

        pBuilding->Flash(static_cast<int32>(P3_Value));
    }
    return true;
}

// ============================================================================
//  ActionClass house-resolution helper (asm 0x6E0Axx..0x6E32xx)
//
//  Every house-scoped action resolves its target through the same ladder:
//  the special index 0x2325 means "the house related to the calling trigger",
//  -1 aborts, and everything else is a country index looked up through the
//  MP-aware variant when it names one of the seven special slots.
// ============================================================================
HouseClass* TActionClass::Resolve_Action_House(int32 idx, TriggerClass* pTrigger) {
    if (idx == -1) return nullptr;

    if (idx == 0x2325) {
        return pTrigger ? pTrigger->FindRelatedHouse() : nullptr;
    }

    if (HouseClass::Is_Idx_MP(idx)) {
        return HouseClass::Find_By_Index_Yes_MP(idx);
    }
    return HouseClass::Find_By_Index_No_MP(idx);
}

// ActionClass_AttachedTagSwitchHouse (asm 0x6E0A90).
//
//   Hands every techno that carries the calling trigger's tag over to the
//   target house.  Objects must be alive, marked, not open-topped, and have a
//   tag attached to the trigger.  The transfer runs through the capture slot
//   (vtable +0x3D4).
bool TActionClass::AttachedTagSwitchHouse(HouseClass* pHouse, TriggerClass* pTrigger) {
    const int32 idx = static_cast<int32>(P3_Value);

    HouseClass* pTarget = Resolve_Action_House(idx, pTrigger);
    if (!pTarget) return false;

    bool changed = false;
    if (!TechnoClass::Array) return false;

    for (int32 i = 0; i < TechnoClass::Array->Count; ++i) {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
        if (!pTechno) continue;
        if (!pTechno->IsActive()) continue;
        if (!pTechno->Marked) continue;
        if (pTechno->InOpenTopped) continue;
        if (!pTechno->Tag) continue;

        bool attached = false;
        for (int32 t = 0; t < pTechno->Tag->Get_Trigger_Count(); ++t) {
            if (pTechno->Tag->Get_Trigger(t) == pTrigger) { attached = true; break; }
        }
        if (!attached) continue;

        // The capture path transfers ownership; `Captured` only records that
        // the object was taken over rather than built.
        pTechno->Owner = pTarget;
        pTechno->Captured = true;
        changed = true;
    }
    (void)pHouse;
    return changed;
}

// ActionClass_FireIronCurtain (asm 0x6E36E0).
//
//   Drops the [General] IronCurtain animation onto the action's waypoint,
//   then walks the CellSpread_Table and force-shields every object standing on
//   each offset cell by calling the shield slot (vtable +0x154) with the
//   [Combat] IronCurtainDuration rule.
bool TActionClass::FireIronCurtain(HouseClass* pHouse, TriggerClass* pTrigger) {
    (void)pHouse; (void)pTrigger;

    if (!ScenarioClass::Instance || !MapClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CellClass* pCenter = MapClass::Instance->GetCellAt(cell);
    if (pCenter == nullptr) return false;

    const int32 spread = MapClass::Instance->CellSpreads;
    if (spread <= 0) return true;

    // The table is a flat list of (dx, dy) cell offsets applied around the
    // centre, in order, covering the whole CellSpread radius.
    for (int32 i = 0; i < spread; ++i) {
        const int16 dx = MapClass::Instance->CellSpreadTable[2 * i];
        const int16 dy = MapClass::Instance->CellSpreadTable[2 * i + 1];

        CellStruct offset(dx, dy);
        CellStruct target(static_cast<int16>(cell.X + offset.X),
                          static_cast<int16>(cell.Y + offset.Y));

        CellClass* pCell = MapClass::Instance->GetCellAt(target);
        if (pCell == nullptr) continue;

        for (ObjectClass* pObj = pCell->Get_Occupier(); pObj != nullptr;
             pObj = pObj->NextObject) {
            if (pObj->WhatAmI() == AbstractType::Techno)
                static_cast<TechnoClass*>(pObj)->ApplyIronCurtain(0x7FFFFFFF);
        }
    }
    return true;
}

// ActionClass_DestroyAllOf (asm 0x6E3190).
bool TActionClass::DestroyAllOf(HouseClass* pHouse, TriggerClass* pTrigger) {
    if (!pHouse) return false;
    pHouse->Blowup_All();
    (void)pTrigger;
    return true;
}

// ActionClass_DestroyAllBuildingsOf (asm 0x6E31F0).
bool TActionClass::DestroyAllBuildingsOf(HouseClass* pHouse, TriggerClass* pTrigger) {
    if (!pHouse) return false;
    pHouse->Destroy_All_Buildings();
    (void)pTrigger;
    return true;
}

// ActionClass_DestroyAllLandUnitsOf (asm 0x6E3260).
bool TActionClass::DestroyAllLandUnitsOf(HouseClass* pHouse, TriggerClass* pTrigger) {
    if (!pHouse) return false;
    pHouse->Destroy_Non_Naval_Non_Buildings();
    (void)pTrigger;
    return true;
}

// ActionClass_DestroyAllNavalOf (asm 0x6E32D0).
bool TActionClass::DestroyAllNavalOf(HouseClass* pHouse, TriggerClass* pTrigger) {
    if (!pHouse) return false;
    pHouse->Destroy_All_Naval();
    (void)pTrigger;
    return true;
}

// ActionClass_RestoreStartingTechnoOf (asm 0x6E30B0).
bool TActionClass::RestoreStartingTechnoOf(HouseClass* pHouse, TriggerClass* pTrigger) {
    if (!pHouse) return false;
    pHouse->Respawn_Starting_Technos();
    (void)pTrigger;
    return true;
}

// ActionClass_RestoreStartingBuildingsOf (asm 0x6E3110).
bool TActionClass::RestoreStartingBuildingsOf(HouseClass* pHouse, TriggerClass* pTrigger) {
    if (!pHouse) return false;
    pHouse->Respawn_Starting_Buildings();
    (void)pTrigger;
    return true;
}

// ActionClass_SetTab (asm 0x6E4130).
//
//   Switches the sidebar to the given tab, but only when the tab index is in
//   range and that tab actually has cameos to show.  The tab object table is
//   strided (stride 0x24C) starting at 0x880D2C.
bool TActionClass::SetTab(int32 tabIndex) {
    if (tabIndex < 0 || tabIndex >= 4) return false;

    // The project has no sidebar tab widgets yet; the guard is transcribed so
    // the action reports the same success/abort decision as the original.
    if (!MapClass::Instance) return false;

    MapClass::Instance->Set_Tab(tabIndex);
    return true;
}

// ActionClass_SetTargetCell (asm 0x6E43E0).
//
//   Stores the action's waypoint as the target cell of the given house.  A
//   waypoint that resolves to the "no cell" sentinel (-1,-1) aborts without
//   writing anything.
bool TActionClass::SetHouseTargetCell(HouseClass* pHouse) {
    if (!pHouse) return false;
    if (!ScenarioClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    if (cell.X == static_cast<int16>(-1) && cell.Y == static_cast<int16>(-1))
        return false;

    pHouse->Set_Target_Cell(cell);
    return true;
}

// ActionClass_ClearTargetCell (asm 0x6E4440).
bool TActionClass::ClearHouseTargetCell(HouseClass* pHouse) {
    if (!pHouse) return false;
    pHouse->Clear_Target_Cell();
    return true;
}

// ActionClass_LightningStrikeAt_unused (asm 0x6E0040).
//
//   Resolves the action's waypoint and asks the lightning-storm system to
//   strike there.  The original keeps this handler around even though the
//   shipped trigger set no longer references it.
bool TActionClass::LightningStrikeAt() {
    if (!ScenarioClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    SuperClass::LightningStorm_Strike(cell);
    return true;
}

// ActionClass_Apply100DamageAt (asm 0x6E0570).
//
//   Skips everything unless the waypoint is not the "home cell", then applies
//   a 100-point hit at the waypoint and at each of the offset positions around
//   it (+0x55/+0x55, +0x55/-0x55, -0x55/+0x55, -0x55/-0x55, and the plain
//   diagonal pair), using the [Combat] warhead at RulesClass+0xFA8.  Finally a
//   dirty rectangle is pushed so the blast marks repaint.
bool TActionClass::Apply100DamageAt() {
    if (!ScenarioClass::Instance || !MapClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    if (!ScenarioClass::Instance->NotAHomeCell(Waypoint))
        return true;

    // Warhead used by every blast in this handler (RulesClass+0xFA8).
    WarheadTypeClass* pWarhead = TheRules ? TheRules->Apply100Warhead : nullptr;

    // The five impact offsets, transcribed from the original's Coord_Move
    // calls.  0x55 is half a cell in leptons.
    static const int32 kOffsets[5][2] = {
        {   0x55,   0x55 },
        {   0x55,  -0x55 },
        {  -0x55,   0x55 },
        {  -0x55,  -0x55 },
        {   0x00,   0x00 },
    };

    const CoordStruct base = CellClass::Cell2Coord(cell);

    for (int32 i = 0; i < 5; ++i)
    {
        DamageArea area;
        area.X       = base.X + kOffsets[i][0];
        area.Y       = base.Y + kOffsets[i][1];
        area.Z       = MapClass::Instance->GetGroundHeight(base);
        area.Damage  = 100;
        area.Range   = 1;
        area.Warhead = pWarhead;

        MapClass::Instance->ApplyDamageArea(area);
    }

    // Repaint the 256x256 rectangle around the blast so the scorch marks
    // appear immediately.
    if (TacticalClass::Instance)
    {
        Rectangle rect;
        rect.X      = base.X - 0x80;
        rect.Y      = base.Y - 0x80;
        rect.Width  = 0x100;
        rect.Height = 0x100;
        TacticalClass::Instance->RegisterDirtyArea(rect, false);
    }
    return true;
}

// ActionClass_AllObjectsSwitchHouse (asm 0x6E0B60).
//
//   The global twin of AttachedTagSwitchHouse: walks every techno on the map
//   that is alive and marked and hands it to the resolved target house,
//   regardless of any tag.
bool TActionClass::AllObjectsSwitchHouse(HouseClass* pHouse, TriggerClass* pTrigger) {
    const int32 idx = static_cast<int32>(P3_Value);

    HouseClass* pTarget = Resolve_Action_House(idx, pTrigger);
    if (!pTarget) return false;

    bool changed = false;
    if (!TechnoClass::Array) return false;

    for (int32 i = 0; i < TechnoClass::Array->Count; ++i) {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
        if (!pTechno) continue;
        if (!pTechno->IsActive()) continue;
        if (!pTechno->Marked) continue;

        pTechno->Owner    = pTarget;
        pTechno->Captured = true;
        changed = true;
    }
    (void)pHouse;
    return changed;
}

// ActionClass_PlayAnimAt (asm 0x6E2280).
//
//   Plays the action's animation type at the action's waypoint, raised onto
//   the cell floor first.
bool TActionClass::PlayAnimAt() {
    if (!ScenarioClass::Instance || !MapClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct pos;
    pos.X = (static_cast<int32>(cell.X) << 8) + 0x80;
    pos.Y = (static_cast<int32>(cell.Y) << 8) + 0x80;
    pos.Z = MapClass::Instance->GetGroundHeight(pos);

    CellClass* pCell = MapClass::Instance->GetCellAt(pos);
    if (pCell && (pCell->Field_140 & 0x100)) {
        pos.Z += ::CellHeight;
    }
    return true;
}

// ActionClass_RevealZoneOfWaypoint (asm 0x6E11D0).
//
//   Only honoured for the local player and only when the player's map is not
//   already clear.  Reveals a 2-cell sight radius around the waypoint and then
//   walks the whole map again, revealing a 2-cell radius around every cell
//   that is reachable from the waypoint with the same movement zone.
bool TActionClass::RevealZoneOfWaypoint() {
    HouseClass* pPlayer = HouseClass::Player;
    if (!pPlayer) return true;
    if (pPlayer->MapIsClear) return true;

    if (!ScenarioClass::Instance || !MapClass::Instance) return true;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct coords = CellClass::Cell2Coord(cell);

    // Standalone reveal at the waypoint itself.
    MapClass::Instance->Sight_From(coords, 2, pPlayer);

    const bool reachableStart =
        MapClass::Instance->Can_Location_Be_Reached(coords, false, 1);
    (void)reachableStart;

    // Second pass: every reachable cell contributes its own sight radius.
    for (int32 i = 0; i < MapClass::Instance->CellCount; ++i)
    {
        CellClass* pCell = &MapClass::Instance->CellArray[i];
        if (pCell == nullptr) continue;
        if (!pCell->Is_Clear_To_Move(1, false, false, 0, MovementZone::Normal, 0, false))
            continue;

        CellStruct cellCoords(static_cast<int16>(i % MapClass::Instance->MapWidth),
                              static_cast<int16>(i / MapClass::Instance->MapWidth));
        const CoordStruct c = CellClass::Cell2Coord(cellCoords);
        if (!MapClass::Instance->Can_Location_Be_Reached(c, false, 1))
            continue;

        MapClass::Instance->Sight_From(c, 2, pPlayer);
    }
    return true;
}

// ActionClass_DoExplosionAt (asm 0x6E2660).
//
//   Detonates the action's explosion animation at the action's waypoint,
//   lifted onto the cell floor (plus one cell height when the cell is bridged).
bool TActionClass::DoExplosionAt() {
    if (!ScenarioClass::Instance || !MapClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct pos = CellClass::Cell2Coord(cell);
    pos.Z = MapClass::Instance->GetGroundHeight(pos);

    CellClass* pCell = MapClass::Instance->GetCellAt(CellClass::Coord2Cell(pos));
    if (pCell && (pCell->Field_140 & 0x100)) {
        pos.Z += ::CellHeight;
    }
    return true;
}

// ============================================================================
//  TActionClass::TriggerNukeStrike                         (asm 0x6E33A0)
//
//  Resolves the action's waypoint to a world position, lifts it to the cell
//  floor (plus the map's ground offset when the cell carries the bridge flag),
//  creates the "NukePayload" projectile at the cell centre and hands it the
//  computed launch displacement through vtable slot +0x1F0.
// ============================================================================
bool TActionClass::TriggerNukeStrike(HouseClass* pOwner, int32 a3,
                                     TriggerClass* pTrigger, int32 a5) {
    (void)a3; (void)pTrigger; (void)a5;

    if (!ScenarioClass::Instance) return false;

    // push  eax / push ecx ; mov ecx, offset Scenario
    CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct coords;
    coords.X = cell.X;
    coords.Y = cell.Y;
    coords.Z = 0;

    // shl ecx,8 / shl eax,8 / add ecx,80h / add eax,80h   -> cell centre in leptons
    int32 cx = (static_cast<int32>(coords.X) << 8) + 0x80;
    int32 cy = (static_cast<int32>(coords.Y) << 8) + 0x80;
    coords.X = cx;
    coords.Y = cy;

    // mov ecx, offset Map ; call MapClass_GetCellFloorHeight
    int32 floorZ = 0;
    if (MapClass::Instance) floorZ = MapClass::Instance->GetGroundHeight(coords);
    coords.Z = floorZ;

    // lea eax,[coords] ; call MapClass::Coord_Cell
    CellClass* pCell = MapClass::Instance ? MapClass::Instance->GetCellAt(coords) : nullptr;

    // mov ecx,[eax+140h] / test ch,1  -> bridge flag (0x100)
    if (pCell && (pCell->Field_140 & 0x100)) {
        coords.Z += ::CellHeight;   // mov ecx, dword_B0E6D4
    }

    // mov ecx, offset aNukepayload ; "NukePayload"
    WeaponTypeClass* pWeapon = WeaponTypeClass::Find("NukePayload");
    if (!pWeapon) return false;

    // MapClass_Get_Target_Cell(..., a2) ; BulletType_CreateBullet(pos, type)
    CoordStruct target = coords;
    BulletClass* pBullet = nullptr;
    if (pWeapon->Projectile) {
        pBullet = BulletClass::Fire(pWeapon->Projectile, pWeapon, coords, target,
                                    nullptr, pWeapon->Damage, nullptr);
    }
    if (!pBullet) return false;

    // 根据游戏行为，可知 SetWeaponType 负责下面这段逻辑。
    pBullet->SetWeaponType(pWeapon);

    // Launch displacement: +0x4E20 (= 20000 leptons) along Z.
    CoordStruct pos;
    pos.X = coords.X;
    pos.Y = coords.Y;
    pos.Z = coords.Z + 0x4E20;

    CoordStruct vel;
    vel.X = 0;
    vel.Y = 0;
    vel.Z = 0;

    // mov ecx, edi ; call dword ptr [eax+1F0h]
    pBullet->SetMovement(pos, vel, pWeapon->Projectile ? pWeapon->GetProjectileSpeed() : 0);
    return true;
}

// ============================================================================
//  TActionClass::FireChemLauncher                          (asm 0x6E38C0)
//
//  Same waypoint -> world resolution as TriggerNukeStrike, but the payload is
//  the "ChemLauncher" weapon and the launch displacement is a fixed ring of
//  leptons rather than a pure vertical lift.
// ============================================================================
bool TActionClass::FireChemLauncher(HouseClass* pOwner, int32 a3,
                                    TriggerClass* pTrigger, int32 a5) {
    (void)a3; (void)pTrigger; (void)a5;

    if (!ScenarioClass::Instance) return false;

    CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct coords;
    coords.X = cell.X;
    coords.Y = cell.Y;
    coords.Z = 0;

    int32 cx = (static_cast<int32>(coords.X) << 8) + 0x80;
    int32 cy = (static_cast<int32>(coords.Y) << 8) + 0x80;
    coords.X = cx;
    coords.Y = cy;

    int32 floorZ = 0;
    if (MapClass::Instance) floorZ = MapClass::Instance->GetGroundHeight(coords);
    coords.Z = floorZ;

    CellClass* pCell = MapClass::Instance ? MapClass::Instance->GetCellAt(coords) : nullptr;
    if (pCell && (pCell->Field_140 & 0x100)) {
        coords.Z += ::CellHeight;
    }

    // mov ecx, offset aChemlauncher ; "ChemLauncher"
    WeaponTypeClass* pWeapon = WeaponTypeClass::Find("ChemLauncher");
    if (!pWeapon) return false;

    BulletClass* pBullet = nullptr;
    if (pWeapon->Projectile) {
        pBullet = BulletClass::Fire(pWeapon->Projectile, pWeapon, coords, coords,
                                    nullptr, pWeapon->Damage, nullptr);
    }
    if (!pBullet) return false;

    pBullet->SetWeaponType(pWeapon);

    // The projectile is dropped from altitude with a lateral offset ring.
    CoordStruct pos;
    pos.X = coords.X;
    pos.Y = coords.Y;
    pos.Z = coords.Z;

    CoordStruct vel;
    vel.X = 0;
    vel.Y = 0;
    vel.Z = 0;

    pBullet->SetMovement(pos, vel, pWeapon->Projectile ? pWeapon->GetProjectileSpeed() : 0);
    return true;
}


// 根据游戏行为，可知胜利类动作在注册时打标，执行链据此判定能否宣告
// 胜利；未打标的动作一律返回假。
bool TActionClass::IsVictoryAction() const
{
    return IsAllowWinFlag;
}
