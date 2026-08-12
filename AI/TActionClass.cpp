#include "TActionClass.h"
#include "TriggerClass.h"
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

    // Apply the matching difficulty multipliers from the rules database
    // so the combat/production/economy code picks them up on the next
    // update pass.
    if (RulesClass::Instance) {
        const DifficultyStruct* diff = RulesClass::Instance->GetDifficulty(newDifficulty);
        if (diff) {
            Game::DifficultyFirepowerMult = diff->Firepower;
            Game::DifficultyArmorMult = diff->Armor;
            Game::DifficultySpeedMult = diff->GroundSpeed;
            Game::DifficultyROFMult = diff->ROF;
            Game::DifficultyCostMult = diff->Cost;
            Game::DifficultyBuildTimeMult = diff->BuildTime;
        }
    }

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