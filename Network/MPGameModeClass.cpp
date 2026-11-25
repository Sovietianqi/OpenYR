#include "MPGameModeClass.h"
#include "Game/MPTeams.h"
#include <cmath>
#include <cstring>
#include <cwchar>
#include "SessionClass.h"
#include "../Game/Externs.h"
#include "../Scenario/ScenarioClass.h"
#include "../Rules/RulesClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"
#include "../INI/INIClass.h"
#include "../Abstract/UnitClass.h"
#include "../UI/StartSpotClass.h"

#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <new>

// 原版的调试输出通道（WWDebugString, 0x7C5B10）。与其它子系统一致落到
// stderr；开局分配相关的几条原文在 asm 里就是走这条通道。
static void WWDebugString(const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    std::vfprintf(stderr, pFormat, args);
    va_end(args);
}


// ============================================================
// MPGameModeClass
// ============================================================

static MPGameModeClass* g_MPGameModeInstance = nullptr;

DynamicVectorClass<MPGameModeClass*>* MPGameModeClass::Array = nullptr;

MPGameModeClass::MPGameModeClass()
    : Field_28(0)
    , GameMode(MultiplayerGameMode::FreeForAll)
    , MaxPlayers(8), MinPlayers(2)
    , StartingCredits(10000), StartingUnits(0)
    , MapRevealed(false), AlliesRevealed(false)
    , ScoreLimit(0), TimeLimit(0), GameTime(0)
    , AllianceLocked(false), AllianceLockedAfter(0)
    , RandomStartingPositions(true)
    , AllowObservers(true), ObserverCount(0)
    , PreBuiltBase(false), BaseTemplateIndex(-1)
    , OneVsOne(false), Ranked(false)
    , DedicatedServer(false), BattleLAN(false)
    , VictoryCondition(VictoryType::DestroyAll)
    , TeamVictory(false), SharedTech(false)
    , UseMapReveal(false), RevealRadius(0)
    , NoSuperWeapons(false), NoMCV(false)
    , NoInfantry(false), NoVehicles(false)
    , NoNavy(false), NoAircraft(false)
    , NoBuildings(false), NoDefenses(false)
    , WonlineTournamentAllowed(true)
    , WonlineClanTournamentAllowed(true)
    , AlliesAllowedFlag(true), AIAllowedFlag(true)
    , MustAlly(false)
{
    for (int32 i = 0; i < MAX_MP_TEAMS; ++i) {
        TeamScores[i] = 0;
        TeamUnits[i] = 0;
        TeamBuildings[i] = 0;
        TeamKills[i] = 0;
        TeamLosses[i] = 0;
        TeamAlive[i] = true;
    }
    for (int32 i = 0; i < MAX_MP_PLAYERS; ++i) {
        PlayerScores[i] = 0;
        PlayerKills[i] = 0;
        PlayerLosses[i] = 0;
        PlayerEconomy[i] = 0;
        PlayerUnits[i] = 0;
        PlayerBuildings[i] = 0;
        PlayerAlive[i] = false;
        PlayerTeam[i] = -1;
        PlayerEliminated[i] = false;
    }
}

MPGameModeClass::~MPGameModeClass() {
}

MPGameModeClass* MPGameModeClass::GetInstance() {
    if (!g_MPGameModeInstance) {
        g_MPGameModeInstance = new MPGameModeClass();
    }
    return g_MPGameModeInstance;
}

void MPGameModeClass::SetGameMode(MultiplayerGameMode mode) {
    GameMode = mode;
    ApplyModeDefaults();
}

void MPGameModeClass::ApplyModeDefaults() {
    switch (GameMode) {
        case MultiplayerGameMode::FreeForAll:
            MaxPlayers = 8;
            AllianceLocked = false;
            AllianceLockedAfter = 0;
            TeamVictory = false;
            VictoryCondition = VictoryType::DestroyAll;
            break;
        case MultiplayerGameMode::Cooperative:
            MaxPlayers = 4;
            AllianceLocked = true;
            AllianceLockedAfter = 0;
            TeamVictory = true;
            VictoryCondition = VictoryType::Campaign;
            break;
        case MultiplayerGameMode::TeamGame:
            MaxPlayers = 8;
            AllianceLocked = true;
            AllianceLockedAfter = 0;
            TeamVictory = true;
            VictoryCondition = VictoryType::DestroyAll;
            break;
        case MultiplayerGameMode::Battle:
            MaxPlayers = 2;
            AllianceLocked = true;
            AllianceLockedAfter = 0;
            OneVsOne = true;
            VictoryCondition = VictoryType::DestroyAll;
            break;
        case MultiplayerGameMode::Tournament:
            MaxPlayers = 2;
            AllianceLocked = true;
            AllianceLockedAfter = 0;
            Ranked = true;
            VictoryCondition = VictoryType::DestroyAll;
            break;
        case MultiplayerGameMode::Megawealth:
            MaxPlayers = 8;
            VictoryCondition = VictoryType::Score;
            ScoreLimit = 200000;
            break;
        case MultiplayerGameMode::Duel:
            MaxPlayers = 2;
            AllianceLocked = true;
            OneVsOne = true;
            VictoryCondition = VictoryType::DestroyAll;
            break;
        case MultiplayerGameMode::MeatGrind:
            MaxPlayers = 4;
            VictoryCondition = VictoryType::Score;
            ScoreLimit = 100000;
            break;
        case MultiplayerGameMode::NavalWar:
            MaxPlayers = 4;
            NoVehicles = true;
            NoInfantry = true;
            VictoryCondition = VictoryType::DestroyAll;
            break;
        case MultiplayerGameMode::AirWar:
            MaxPlayers = 4;
            NoNavy = true;
            NoInfantry = true;
            NoVehicles = true;
            VictoryCondition = VictoryType::DestroyAll;
            break;
        case MultiplayerGameMode::FFA:
            MaxPlayers = 8;
            AllianceLocked = false;
            TeamVictory = false;
            VictoryCondition = VictoryType::DestroyAll;
            break;
        case MultiplayerGameMode::Coop:
            MaxPlayers = 4;
            AllianceLocked = true;
            TeamVictory = true;
            VictoryCondition = VictoryType::Campaign;
            break;
        default:
            break;
    }
}

MultiplayerGameMode MPGameModeClass::GetGameMode() const {
    return GameMode;
}

void MPGameModeClass::SetAlliance(int32 player1, int32 player2, bool allied) {
    if (player1 < 0 || player1 >= MAX_MP_PLAYERS) return;
    if (player2 < 0 || player2 >= MAX_MP_PLAYERS) return;
    if (player1 == player2) return;
    if (AllianceLocked) return;

    Alliances[player1][player2] = allied;
    Alliances[player2][player1] = allied;
}

bool MPGameModeClass::IsAllied(int32 player1, int32 player2) const {
    if (player1 < 0 || player1 >= MAX_MP_PLAYERS) return false;
    if (player2 < 0 || player2 >= MAX_MP_PLAYERS) return false;
    if (player1 == player2) return true;
    return Alliances[player1][player2];
}

void MPGameModeClass::LockAlliances() {
    AllianceLocked = true;
}

void MPGameModeClass::UnlockAlliances() {
    if (AllianceLockedAfter <= 0) {
        AllianceLocked = false;
    }
}

void MPGameModeClass::SetAllianceLockTime(int32 minutes) {
    AllianceLockedAfter = minutes * 60 * 60;
}

void MPGameModeClass::UpdateAlliances() {
    if (AllianceLockedAfter > 0) {
        if (GameTime >= AllianceLockedAfter) {
            AllianceLocked = true;
        }
    }
}

void MPGameModeClass::SetTeam(int32 playerID, int32 team) {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return;
    if (team < 0 || team >= MAX_MP_TEAMS) return;
    PlayerTeam[playerID] = team;
}

int32 MPGameModeClass::GetTeam(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return -1;
    return PlayerTeam[playerID];
}

void MPGameModeClass::UpdateScores() {
    for (int32 i = 0; i < MAX_MP_TEAMS; ++i) {
        TeamScores[i] = 0;
        TeamUnits[i] = 0;
        TeamBuildings[i] = 0;
        TeamKills[i] = 0;
        TeamLosses[i] = 0;
    }

    for (int32 i = 0; i < MAX_MP_PLAYERS; ++i) {
        if (!PlayerAlive[i]) continue;
        PlayerScores[i] = PlayerEconomy[i] + (PlayerKills[i] * 100) - (PlayerLosses[i] * 50);
        if (PlayerScores[i] < 0) PlayerScores[i] = 0;

        int32 team = PlayerTeam[i];
        if (team >= 0 && team < MAX_MP_TEAMS) {
            TeamScores[team] += PlayerScores[i];
            TeamUnits[team] += PlayerUnits[i];
            TeamBuildings[team] += PlayerBuildings[i];
            TeamKills[team] += PlayerKills[i];
            TeamLosses[team] += PlayerLosses[i];
        }
    }
}

int32 MPGameModeClass::GetPlayerScore(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return 0;
    return PlayerScores[playerID];
}

int32 MPGameModeClass::GetTeamScore(int32 team) const {
    if (team < 0 || team >= MAX_MP_TEAMS) return 0;
    return TeamScores[team];
}

void MPGameModeClass::AddPlayerKill(int32 playerID) {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return;
    ++PlayerKills[playerID];
}

void MPGameModeClass::AddPlayerLoss(int32 playerID) {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return;
    ++PlayerLosses[playerID];
}

void MPGameModeClass::AddPlayerEconomy(int32 playerID, int32 amount) {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return;
    PlayerEconomy[playerID] += amount;
}

void MPGameModeClass::UpdatePlayerUnits(int32 playerID, int32 count) {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return;
    PlayerUnits[playerID] = count;
}

void MPGameModeClass::UpdatePlayerBuildings(int32 playerID, int32 count) {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return;
    PlayerBuildings[playerID] = count;
}

void MPGameModeClass::SetPlayerAlive(int32 playerID, bool alive) {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return;
    PlayerAlive[playerID] = alive;
    if (!alive) {
        PlayerEliminated[playerID] = true;
    }
}

bool MPGameModeClass::IsPlayerAlive(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return false;
    return PlayerAlive[playerID];
}

bool MPGameModeClass::IsPlayerEliminated(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return false;
    return PlayerEliminated[playerID];
}

int32 MPGameModeClass::CheckWinCondition() {
    switch (VictoryCondition) {
        case VictoryType::DestroyAll:
            return CheckDestroyAllWin();
        case VictoryType::Score:
            return CheckScoreWin();
        case VictoryType::Time:
            return CheckTimeWin();
        case VictoryType::Campaign:
            return CheckCampaignWin();
        case VictoryType::Custom:
            return CheckCustomWin();
        default:
            return -1;
    }
}

int32 MPGameModeClass::CheckDestroyAllWin() {
    int32 aliveTeam = -1;
    int32 alivePlayer = -1;
    int32 aliveCount = 0;

    for (int32 i = 0; i < MAX_MP_PLAYERS; ++i) {
        if (PlayerAlive[i] && !PlayerEliminated[i]) {
            ++aliveCount;
            int32 team = PlayerTeam[i];
            if (TeamVictory) {
                if (aliveTeam < 0) aliveTeam = team;
                else if (aliveTeam != team) return -1;
            } else {
                alivePlayer = i;
            }
        }
    }

    if (aliveCount <= 1) {
        if (TeamVictory) return aliveTeam;
        return alivePlayer;
    }
    return -1;
}

int32 MPGameModeClass::CheckScoreWin() {
    if (ScoreLimit <= 0) return -1;

    for (int32 i = 0; i < MAX_MP_PLAYERS; ++i) {
        if (PlayerAlive[i] && PlayerScores[i] >= ScoreLimit) {
            if (TeamVictory) return PlayerTeam[i];
            return i;
        }
    }
    return -1;
}

int32 MPGameModeClass::CheckTimeWin() {
    if (TimeLimit <= 0) return -1;
    if (GameTime < TimeLimit) return -1;

    int32 bestPlayer = -1;
    int32 bestScore = -1;
    for (int32 i = 0; i < MAX_MP_PLAYERS; ++i) {
        if (PlayerAlive[i] && PlayerScores[i] > bestScore) {
            bestScore = PlayerScores[i];
            bestPlayer = i;
        }
    }
    return bestPlayer;
}

int32 MPGameModeClass::CheckCampaignWin() {
    int32 aliveCount = 0;
    for (int32 i = 0; i < MAX_MP_PLAYERS; ++i) {
        if (PlayerAlive[i] && !PlayerEliminated[i]) {
            ++aliveCount;
        }
    }
    if (aliveCount == 0) return -1;
    return aliveCount > 0 ? 0 : -1;
}

int32 MPGameModeClass::CheckCustomWin() {
    // CheckCustomWin - evaluate custom multiplayer win conditions.
    // The custom victory type is used by mod maps and special game modes
    // that define their own win criteria beyond the standard destroy-all,
    // score, and time-based conditions. The engine checks multiple criteria
    // in priority order: score limit, time limit, then elimination.

    // 1. Score-based win: if a score limit is set, check whether any
    //    alive player has reached it.
    if (ScoreLimit > 0) {
        for (int32 i = 0; i < MAX_MP_PLAYERS; ++i) {
            if (PlayerAlive[i] && PlayerScores[i] >= ScoreLimit) {
                if (TeamVictory) return PlayerTeam[i];
                return i;
            }
        }
    }

    // 2. Time-based win: if the time limit has elapsed, the player or team
    //    with the highest score wins.
    if (TimeLimit > 0 && GameTime >= TimeLimit) {
        int32 bestPlayer = -1;
        int32 bestScore = -1;
        for (int32 i = 0; i < MAX_MP_PLAYERS; ++i) {
            if (PlayerAlive[i] && PlayerScores[i] > bestScore) {
                bestScore = PlayerScores[i];
                bestPlayer = i;
            }
        }
        if (bestPlayer >= 0) {
            if (TeamVictory) return PlayerTeam[bestPlayer];
            return bestPlayer;
        }
    }

    // 3. Elimination win: if only one player (or one team) remains alive,
    //    they are the winner.
    int32 aliveCount = 0;
    int32 lastAlive = -1;
    int32 aliveTeam = -1;
    for (int32 i = 0; i < MAX_MP_PLAYERS; ++i) {
        if (PlayerAlive[i] && !PlayerEliminated[i]) {
            ++aliveCount;
            lastAlive = i;
            if (TeamVictory) {
                if (aliveTeam < 0) {
                    aliveTeam = PlayerTeam[i];
                } else if (aliveTeam != PlayerTeam[i]) {
                    // Multiple teams still alive - no winner yet.
                    return -1;
                }
            }
        }
    }

    if (aliveCount <= 1) {
        if (TeamVictory) return aliveTeam;
        return lastAlive;
    }

    return -1;
}

void MPGameModeClass::SetScoreLimit(int32 limit) {
    ScoreLimit = limit;
}

void MPGameModeClass::SetTimeLimit(int32 minutes) {
    TimeLimit = minutes * 60 * 60;
}

void MPGameModeClass::SetStartingCredits(int32 credits) {
    StartingCredits = credits;
    if (StartingCredits < 0) StartingCredits = 0;
    if (StartingCredits > 100000) StartingCredits = 100000;
}

int32 MPGameModeClass::GetStartingCredits() const {
    return StartingCredits;
}

void MPGameModeClass::SetStartingUnits(int32 count) {
    StartingUnits = count;
    if (StartingUnits < 0) StartingUnits = 0;
    if (StartingUnits > 50) StartingUnits = 50;
}

int32 MPGameModeClass::GetStartingUnits() const {
    return StartingUnits;
}

void MPGameModeClass::SetMapRevealed(bool revealed) {
    MapRevealed = revealed;
}

void MPGameModeClass::SetAlliesRevealed(bool revealed) {
    AlliesRevealed = revealed;
}

void MPGameModeClass::SetNoSuperWeapons(bool disabled) {
    NoSuperWeapons = disabled;
}

void MPGameModeClass::SetNoMCV(bool disabled) {
    NoMCV = disabled;
}

void MPGameModeClass::SetNoInfantry(bool disabled) {
    NoInfantry = disabled;
}

void MPGameModeClass::SetNoVehicles(bool disabled) {
    NoVehicles = disabled;
}

void MPGameModeClass::SetNoNavy(bool disabled) {
    NoNavy = disabled;
}

void MPGameModeClass::SetNoAircraft(bool disabled) {
    NoAircraft = disabled;
}

void MPGameModeClass::SetNoBuildings(bool disabled) {
    NoBuildings = disabled;
}

void MPGameModeClass::SetNoDefenses(bool disabled) {
    NoDefenses = disabled;
}

void MPGameModeClass::SetPreBuiltBase(bool enabled) {
    PreBuiltBase = enabled;
}

void MPGameModeClass::SetBaseTemplateIndex(int32 index) {
    BaseTemplateIndex = index;
}

void MPGameModeClass::SetRandomStartingPositions(bool random) {
    RandomStartingPositions = random;
}

void MPGameModeClass::SetAllowObservers(bool allow) {
    AllowObservers = allow;
}

void MPGameModeClass::SetRanked(bool ranked) {
    Ranked = ranked;
}

void MPGameModeClass::SetDedicatedServer(bool dedicated) {
    DedicatedServer = dedicated;
}

void MPGameModeClass::SetBattleLAN(bool lan) {
    BattleLAN = lan;
}

void MPGameModeClass::SetSharedTech(bool shared) {
    SharedTech = shared;
}

void MPGameModeClass::SetRevealRadius(int32 radius) {
    RevealRadius = radius;
    if (RevealRadius < 0) RevealRadius = 0;
    if (RevealRadius > 256) RevealRadius = 256;
}

void MPGameModeClass::SetUseMapReveal(bool use) {
    UseMapReveal = use;
}

void MPGameModeClass::UpdateGameTime() {
    ++GameTime;
}

int32 MPGameModeClass::GetGameTime() const {
    return GameTime;
}

int32 MPGameModeClass::GetGameTimeMinutes() const {
    return GameTime / (60 * 60);
}

int32 MPGameModeClass::GetGameTimeSeconds() const {
    return (GameTime / 60) % 60;
}

bool MPGameModeClass::IsMapRevealed() const {
    return MapRevealed;
}

bool MPGameModeClass::IsAlliesRevealed() const {
    return AlliesRevealed;
}

bool MPGameModeClass::IsAllianceLocked() const {
    return AllianceLocked;
}

bool MPGameModeClass::IsTeamVictory() const {
    return TeamVictory;
}

bool MPGameModeClass::IsNoSuperWeapons() const {
    return NoSuperWeapons;
}

bool MPGameModeClass::IsNoMCV() const {
    return NoMCV;
}

bool MPGameModeClass::IsNoInfantry() const {
    return NoInfantry;
}

bool MPGameModeClass::IsNoVehicles() const {
    return NoVehicles;
}

bool MPGameModeClass::IsNoNavy() const {
    return NoNavy;
}

bool MPGameModeClass::IsNoAircraft() const {
    return NoAircraft;
}

bool MPGameModeClass::IsNoBuildings() const {
    return NoBuildings;
}

bool MPGameModeClass::IsNoDefenses() const {
    return NoDefenses;
}

bool MPGameModeClass::IsPreBuiltBase() const {
    return PreBuiltBase;
}

int32 MPGameModeClass::GetBaseTemplateIndex() const {
    return BaseTemplateIndex;
}

bool MPGameModeClass::IsRandomStartingPositions() const {
    return RandomStartingPositions;
}

bool MPGameModeClass::IsAllowObservers() const {
    return AllowObservers;
}

bool MPGameModeClass::IsRanked() const {
    return Ranked;
}

bool MPGameModeClass::IsDedicatedServer() const {
    return DedicatedServer;
}

bool MPGameModeClass::IsBattleLAN() const {
    return BattleLAN;
}

bool MPGameModeClass::IsSharedTech() const {
    return SharedTech;
}

int32 MPGameModeClass::GetRevealRadius() const {
    return RevealRadius;
}

bool MPGameModeClass::IsUseMapReveal() const {
    return UseMapReveal;
}

void MPGameModeClass::ResetGame() {
    GameTime = 0;
    for (int32 i = 0; i < MAX_MP_PLAYERS; ++i) {
        PlayerScores[i] = 0;
        PlayerKills[i] = 0;
        PlayerLosses[i] = 0;
        PlayerEconomy[i] = 0;
        PlayerUnits[i] = 0;
        PlayerBuildings[i] = 0;
        PlayerAlive[i] = true;
        PlayerEliminated[i] = false;
    }
    for (int32 i = 0; i < MAX_MP_TEAMS; ++i) {
        TeamScores[i] = 0;
        TeamUnits[i] = 0;
        TeamBuildings[i] = 0;
        TeamKills[i] = 0;
        TeamLosses[i] = 0;
        TeamAlive[i] = true;
    }
    for (int32 i = 0; i < MAX_MP_PLAYERS; ++i) {
        for (int32 j = 0; j < MAX_MP_PLAYERS; ++j) {
            Alliances[i][j] = false;
        }
        Alliances[i][i] = true;
    }
}

int32 MPGameModeClass::GetMaxPlayers() const {
    return MaxPlayers;
}

int32 MPGameModeClass::GetMinPlayers() const {
    return MinPlayers;
}

void MPGameModeClass::SetMaxPlayers(int32 count) {
    MaxPlayers = count;
    if (MaxPlayers < 2) MaxPlayers = 2;
    if (MaxPlayers > 8) MaxPlayers = 8;
}

void MPGameModeClass::SetMinPlayers(int32 count) {
    MinPlayers = count;
    if (MinPlayers < 1) MinPlayers = 1;
    if (MinPlayers > MaxPlayers) MinPlayers = MaxPlayers;
}

int32 MPGameModeClass::GetPlayerKills(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return 0;
    return PlayerKills[playerID];
}

int32 MPGameModeClass::GetPlayerLosses(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return 0;
    return PlayerLosses[playerID];
}

int32 MPGameModeClass::GetPlayerEconomy(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return 0;
    return PlayerEconomy[playerID];
}

int32 MPGameModeClass::GetPlayerUnitCount(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return 0;
    return PlayerUnits[playerID];
}

int32 MPGameModeClass::GetPlayerBuildingCount(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return 0;
    return PlayerBuildings[playerID];
}

int32 MPGameModeClass::GetTeamKills(int32 team) const {
    if (team < 0 || team >= MAX_MP_TEAMS) return 0;
    return TeamKills[team];
}

int32 MPGameModeClass::GetTeamLosses(int32 team) const {
    if (team < 0 || team >= MAX_MP_TEAMS) return 0;
    return TeamLosses[team];
}

int32 MPGameModeClass::GetTeamUnitCount(int32 team) const {
    if (team < 0 || team >= MAX_MP_TEAMS) return 0;
    return TeamUnits[team];
}

int32 MPGameModeClass::GetTeamBuildingCount(int32 team) const {
    if (team < 0 || team >= MAX_MP_TEAMS) return 0;
    return TeamBuildings[team];
}

int32 MPGameModeClass::GetScoreLimit() const {
    return ScoreLimit;
}

int32 MPGameModeClass::GetTimeLimit() const {
    return TimeLimit;
}

VictoryType MPGameModeClass::GetVictoryCondition() const {
    return VictoryCondition;
}

void MPGameModeClass::SetVictoryCondition(VictoryType type) {
    VictoryCondition = type;
}

void MPGameModeClass::SetTeamVictory(bool team) {
    TeamVictory = team;
}

int32 MPGameModeClass::GetObserverCount() const {
    return ObserverCount;
}

void MPGameModeClass::AddObserver() {
    ++ObserverCount;
}

void MPGameModeClass::RemoveObserver() {
    if (ObserverCount > 0) --ObserverCount;
}
// ============================================================
// Global mode list
//
// Mirrors vec_MPGameModes / MPGameMode_ResetList / vec_MPGameModes_Find.
// ============================================================

void MPGameModeClass::ResetList()
{
    if (Array == nullptr) {
        Array = new DynamicVectorClass<MPGameModeClass*>();
    }
    for (int32 i = 0; i < Array->Count; ++i) {
        delete Array->Items[i];
        Array->Items[i] = nullptr;
    }
    Array->Clear();
}

int32 MPGameModeClass::FindIndex(int32 idx)
{
    if (Array == nullptr) {
        return 0;
    }
    // vec_MPGameModes_Find: linear scan comparing the identifier at +0x28.
    for (int32 i = 0; i < Array->Count; ++i) {
        MPGameModeClass* pMode = Array->Items[i];
        if (pMode != nullptr && pMode->Field_28 == idx) {
            return i;
        }
    }
    return 0;
}

MPGameModeClass* MPGameModeClass::Find(int32 idx)
{
    if (Array == nullptr || Array->Count <= 0) {
        ResetList();
        return (Array != nullptr && Array->Count > 0) ? Array->Items[0] : nullptr;
    }
    const int32 index = FindIndex(idx);
    if (Array->Count > 0) {
        return Array->Items[index];
    }
    return nullptr;
}

void MPGameModeClass::Register(MPGameModeClass* pMode)
{
    if (pMode == nullptr) {
        return;
    }
    if (Array == nullptr) {
        Array = new DynamicVectorClass<MPGameModeClass*>();
    }
    Array->Add(pMode);
}

// ============================================================
// MPGameModeClass::ReadFromINI
//
// Mirrors the tournament/alliance flags read from the mode's own INI file.
// WonlineTournamentAllowed and WonlineClanTournamentAllowed live in the
// same section as the mode definition; AlliesAllowed and MustAlly describe
// whether the mode permits player chosen alliances.
// ============================================================

void MPGameModeClass::ReadFromINI(CCINIClass* pINI, const char* pSection)
{
    if (pINI == nullptr || pSection == nullptr) {
        return;
    }

    WonlineTournamentAllowed =
        pINI->ReadBool(pSection, "WonlineTournamentAllowed",
                       WonlineTournamentAllowed);
    WonlineClanTournamentAllowed =
        pINI->ReadBool(pSection, "WonlineClanTournamentAllowed",
                       WonlineClanTournamentAllowed);
    AlliesAllowedFlag = pINI->ReadBool(pSection, "AlliesAllowed", AlliesAllowedFlag);
    MustAlly      = pINI->ReadBool(pSection, "MustAlly",      MustAlly);
}

// ============================================================================
// 应答桩与选人界面
// 根据游戏行为，可知各模式对"是否允许某选项"的应答在基类是常量桩：
// 派生模式需要不同答案时按需覆写。返回负值的两档表示"无效/未找到"。
// ============================================================================

int32 MPGameModeClass::ret1()   { return 1; }
int32 MPGameModeClass::ret1_1() { return 1; }
int32 MPGameModeClass::ret1_3() { return 1; }
int32 MPGameModeClass::ret1_6() { return 1; }
int32 MPGameModeClass::ret1_7() { return 1; }
int32 MPGameModeClass::ret1_8() { return 1; }
int32 MPGameModeClass::ret0()   { return 0; }
int32 MPGameModeClass::ret0_0() { return 0; }
int32 MPGameModeClass::ret0_1() { return 0; }
int32 MPGameModeClass::ret0_4() { return 0; }
int32 MPGameModeClass::retm1()  { return -1; }
int32 MPGameModeClass::retm2()  { return -2; }

int32 MPGameModeClass::Selected(int32 slot) const
{
    // asm: vt_MPGameModeClass.FillTeamSelector   (asm:?)
    return (slot >= 0 && slot < MAX_MP_PLAYERS) ? 1 : 0;
}

void MPGameModeClass::FillTeamSelector()
{
    // asm: vt_MPGameModeClass.FillTeamSelectorForSlot   (asm:?)
    // 不进列表。
}

void MPGameModeClass::FillTeamSelectorForSlot(int32 slot)
{
    // asm: vt_MPGameModeClass.SpawnBaseUnit   (asm:?)
    (void)slot;
}

// ----------------------------------------------------------------------------
// asm: MPUnholy_CreateBaseUnit
//
//   UnholyAlliance 模式的开局基地单位生成。原版把这段挂在 UnholyAlliance
//   的 vftable 上，槽位与本类的 SpawnBaseUnit 对应；原版签名是
//   __stdcall(int Owner, int _20) 两栈参、ecx 不参与，第二个栈参全程未被
//   读取（见 asm 全文），所以这里只需 houseIndex 一个参数。
//
//   for (i = Rules.BaseUnit.Count - 1 ; i >= 0 ; --i):
//     type = Rules.BaseUnit[i]
//     if (!type)                                { failed = 1 ; continue }
//     unit = UnitClass_CTOR(new UnitClass, type, Owner)
//     if (!unit)                                { failed = 1 ; continue }
//     home = Owner->GetHomeCell()
//     home.X += 3 ; home.Y += 3
//     if (i == 0):                              首轮先试 lepton 直接落位
//       coords = { (home.X<<8)+0x80, (home.Y<<8)+0x80, 0 }
//       if (unit->[vt+0xD8](unit, &coords, 0))  continue
//     if (Scan_Place_Object(unit, Owner->GetHomeCell(), 3))  continue
//     unit->[vt+0x20](unit, 1)                  扫描不到 → 就地销毁
//     failed = 1
//   返回 !failed
//
//   【留白（铁律①）】
//   vt+0xD8 与 vt+0x20 两个虚槽在项目里没有等价声明：前者是原版
//   TechnoClass 的"按给定 lepton 坐标直接落到地图"重载，后者是
//   "就地销毁"重载；本项目的 Unlimbo()/析构语义并不逐槽对应，
//   强行挪用会让链接期选错槽位，所以此处不写死槽位号，只保留控制流与
//   调用点，等这两槽在真实类里补出来后再接上。
//   Scan_Place_Object（__fastcall(TechnoClass*, wXY*, int)，原版 0x688F00
//   一带）是本项目尚未移植的地图扫描放置设施，同样只保留调用点。
//
//   原版返回值在本项目里没有调用方使用（见 CreateStartingUnits）。
// ----------------------------------------------------------------------------
void MPGameModeClass::SpawnBaseUnit(int32 houseIndex)
{
    if (RulesClass::Instance == nullptr) {
        return;
    }

    // 在项目里，houseIndex 是全局房屋表 Houses[] 的下标。
    if (houseIndex < 0 || houseIndex >= MAX_HOUSES) {
        return;
    }
    HouseClass* pOwner = Houses[houseIndex];
    if (pOwner == nullptr) {
        return;
    }

    DynamicVectorClass<UnitTypeClass*>& baseUnits = RulesClass::Instance->BaseUnit;

    for (int32 i = baseUnits.Count - 1; i >= 0; --i) {
        UnitTypeClass* pType = baseUnits[i];
        if (pType == nullptr) {
            continue;
        }

        // asm: push size UnitClass / call new / push Owner / push type /
        //      mov ecx, eax / call UnitClass_CTOR
        //      —— 类型由构造函数第二参数写入，不经由外部赋值。
        UnitClass* pUnit = new (std::nothrow) UnitClass(pOwner);
        if (pUnit == nullptr) {
            continue;
        }
        pUnit->Type = pType;

        // asm: HouseClass_GetHomeCell(Owner, &out) / add cx,3 / add ax,3
        CellStruct home = pOwner->GetHomeCell();
        home.X = static_cast<int16>(home.X + 3);
        home.Y = static_cast<int16>(home.Y + 3);

        if (i == 0) {
            // 首轮用格心的 lepton 坐标直接落位，成功就不再走扫描。
            const CoordStruct coords(
                (static_cast<int32>(home.X) << 8) + 0x80,
                (static_cast<int32>(home.Y) << 8) + 0x80,
                0);
            if (this->Place_Base_Unit_At_Lepton(*pUnit, coords)) {
                continue;
            }
        }

        // asm: HouseClass_GetHomeCell(Owner, &a2) / Scan_Place_Object(unit, eax, 3)
        if (this->Scan_Place_Object(*pUnit, pOwner->GetHomeCell(), 3)) {
            continue;
        }

        // asm: push 1 / call [eax+20h]  (就地销毁)
        delete pUnit;
    }
}

// ----------------------------------------------------------------------------
// 留白（铁律①）：SpawnBaseUnit 依赖的两个原版设施尚未移植。
//
//   Place_Base_Unit_At_Lepton  —— 原版 UnitClass 虚表 +0xD8 槽。asm 调用点：
//       mov eax, [esi] / push edi(0) / push &coords / mov ecx, esi /
//       call dword ptr [eax+0D8h] / test al, al
//     即"按给定 lepton 坐标落位"，返回是否成功。
//
//   Scan_Place_Object —— 原版 __fastcall(TechnoClass* this, wXY* where,
//     int mode)，入口 0x688F00 一带（符号 Scan_Place_Object，被
//     MPSiege_SpawnBaseUnit / MPUnholy_CreateBaseUnit 等共同引用）。
//
// 这两个设施一旦在真实类里补出，把实现接过来即可，调用点无需改动。
// 为保证控制流可见，这里给出保守实现：落位一律失败、扫描一律失败，
// 于是 SpawnBaseUnit 会走到"删除单位"那一支 —— 与原版"没有可放置位置
// 就撤掉单位"的行为一致，且不会把单位留在半初始化状态。
// ----------------------------------------------------------------------------
bool MPGameModeClass::Place_Base_Unit_At_Lepton(UnitClass& unit,
                                               const CoordStruct& coords)
{
    (void)unit;
    (void)coords;
    return false;
}

bool MPGameModeClass::Scan_Place_Object(UnitClass& unit,
                                        const CellStruct& where,
                                        int32 mode)
{
    (void)unit;
    (void)where;
    (void)mode;
    return false;
}

// ----------------------------------------------------------------------------
// asm: MPGameMode_ShouldTeam   (asm:986541)
//
//   ; char __thiscall MPGameMode_ShouldTeam(MPGameModeClass *this, int team)
//   arg_0 = 队伍号（不是玩家席号）。
//
//     mov al,[ecx+3Fh]        ; MustAlly
//     eax = team
//     if (MustAlly && team == -2) return 0
//     if (team < 0 || team > 3)   return 0
//     return 1
//
//   -2 是"未编队"哨兵值：强制结盟模式下不允许游离队伍；其余模式只放行
//   0..3 这四支队伍。
// ----------------------------------------------------------------------------
bool MPGameModeClass::ShouldTeam(int32 slot) const
{
    if (MustAlly && slot == -2)
        return false;
    return slot >= 0 && slot <= 3;
}

void MPGameModeClass::AllyTeams(int32 teamA, int32 teamB)
{
    // 根据游戏行为，可知队伍间结盟把两队成员两两设为盟友。
    if (teamA == teamB) return;
    for (int32 i = 0; i < MAX_MP_PLAYERS; ++i) {
        if (GetTeam(i) == teamA) {
            for (int32 j = 0; j < MAX_MP_PLAYERS; ++j) {
                if (GetTeam(j) == teamB) {
                    SetAlliance(i, j, true);
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
// asm: MPGameMode_StartingPositionsToHouseBases (vftable +0x80, 0x5D6B90)
//
//   char __thiscall(MPGameModeClass *this, char unused)   retn 4
//   unused 只被读进 dl 后丢弃（死读）。
//
//   逐句（asm 行 988493-988565）：
//     VecWXY vec ; Get_Starting_Locations(&vec)      ; 候选出生格
//     for (i = 0 ; i < vec_Houses.length ; ++i):
//       h = vec_Houses[i]
//       if (h->[+0x34]->[+0x1A6]) continue           ; 该国家为 passive，跳过
//       sp = h->StartingPosition                     ; +0x1602C，-2 = 未分配
//       if (sp == 0FFFFFFFEh) continue
//       h->SetBaseSpawnCell(vec[sp])
//       Scenario->[0x1180 + sp*4] = i                ; HouseIndices[sp] = i
//     释放 vec ; return 1
//
//   字段对应：
//     h->[+0x34]        -> HouseClass::Type（本项目把 Country 并进了
//                          HouseTypeClass）
//     Type->[+0x1A6]    -> HouseTypeClass::MultiplayPassive（HouseTypeClass
//                          结构里 Suffix/Prefix/Multiplay/MultiplayPassive
//                          连续排列，+0x1A6 正是 MultiplayPassive）
//     h->StartingPosition(+0x1602C) -> HouseClass::StartingSlot。原版把
//                          "StartingSpotClass_GetStartSlot 的结果"存了两份：
//                          +0x1602C 供本函数与同步日志读取，+0x16058 供
//                          Assign_Houses 写入（见 asm 1330657
//                          `mov [ebp+16058h], ecx`）；本项目只建模了
//                          StartingSlot（赋值来源正是 Get_Start_Slot /
//                          AISlots.StartSlots），两者同值，故这里读
//                          StartingSlot。
//     Scenario->[0x1180] -> ScenarioClass::HouseIndices[16]
//
//   原版对 sp 不做越界检查（vec[sp] 直接取）；本项目候选表是
//   DynamicVectorClass，越界会断言，因此这里补 sp < Count 的防御。
// ----------------------------------------------------------------------------
void MPGameModeClass::StartingPositionsToHouseBases()
{
    ScenarioClass* pScen = ScenarioClass::Instance;
    if (pScen == nullptr) {
        return;
    }

    // asm: lea ecx,[esp+vec] / call Get_Starting_Locations
    DynamicVectorClass<CellStruct> cells;
    pScen->GetStartingLocations(cells);

    const int32 houseCount = HouseClass::ArrayCount;
    for (int32 i = 0; i < houseCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse == nullptr) {
            continue;
        }

        // asm: mov edx,[ecx+34h] / mov al,[edx+1A6h] / jnz next
        if (pHouse->Type != nullptr && pHouse->Type->MultiplayPassive) {
            continue;
        }

        // asm: mov esi,[ecx+StartingPosition] / cmp esi,0FFFFFFFEh / jz next
        const int32 sp = pHouse->StartingSlot;
        if (sp == -2) {
            continue;
        }
        if (sp < 0 || sp >= cells.Count || sp >= 16) {
            continue;                       // 防御：原版此处无边界检查
        }

        // asm: mov edx,[vec.data+esi*4] / push edx / call SetBaseSpawnCell
        pHouse->Set_Base_Spawn_Cell(cells[sp]);

        // asm: mov eax,Scenario / mov [eax+esi*4+1180h],edi
        pScen->HouseIndices[sp] = i;
    }
}

bool MPGameModeClass::MustAlly02() const
{
    // asm: MPGameMode_MustAlly02   (asm:986509)
    //   `mov al,[ecx+3Fh] / neg al / sbb eax,eax / and eax,2 / add eax,-2`
    //   ——原版返回 HRESULT 形态：MustAlly 真 → 0（成功），假 → -2（E_FAIL）。
    //   本槽在项目里声明为 bool，按"0 = 成功"投影成 MustAlly。
    return MustAlly;
}


// ============================================================================
// 配置应答槽位（对应 IDA 槽位名见各实现注记）
// ============================================================================

// asm: MPGameMode_AIAllowed   (asm:987214)
//   `mov al,[ecx+MPGameModeClass.AIAllowed]; retn`——直接回读模式对象的
//   字节标志（vtable 槽 AIPlayersAllowed 的默认实现）。
bool MPGameModeClass::AIAllowed()
{
    return AIAllowedFlag;
}

// asm: MPGameMode_AlliesAllowed   (asm:986525)
//   `mov al,[ecx+3Ch] / neg al / sbb eax,eax / and eax,5 / add eax,-2`
//   即标志非 0 → 5-2 = 3（允许档），为 0 → 0-2 = -2（拒绝码）。
int32 MPGameModeClass::AlliesAllowed()
{
    return AlliesAllowedFlag ? 3 : -2;
}

// ----------------------------------------------------------------------------
// asm: MPGameMode_AssignStartingPoints   (asm:988574)
//
//   ; char __thiscall MPGameMode_AssignStartingPoints(MPGameModeClass *this, char a2)
//   DATA XREF: .rdata:MultiplayerBattle::`vftable' —— 通用多人模式的出生点
//   分配槽；本项目的对应槽位就是 AssignStartingPoints。
//
//   与 StartingPositionsToHouseBases 的分工：那一个只认房主自带的
//   StartingSlot，本函数给"在 Scenario->HouseIndices 里还没有登记"的房主
//   现场挑一格。逐句（asm 行 988573-988745）：
//
//     VecWXY vec ; Get_Starting_Locations(&vec)
//
//     // ① 用 16 个槽的登记表拍一张"已占用"位图
//     for (i = 0 ; i < 16 ; ++i)
//       bytes[i] = (Scenario->HouseIndices[i] == -1) ? 0 : 1
//
//     // ② 逐个房主
//     for (i = 0 ; i < vec_Houses.length ; ++i):
//       h = vec_Houses[i]
//       if (h->[+34h]->[+1A6h]) continue          // MultiplayPassive，跳过
//       // 在 HouseIndices[0..0F] 里找值为 i 的槽（不 break，后命中覆盖）
//       for (j = 0 ; j < 16 ; ++j) if (HouseIndices[j] == i) { found = 1 ; idx = j }
//       if (!found):
//         WWDebugString("Assigning Starting Points for house %d (%s)", i, h->[+34h]+0x24)
//         Cell *c = this->[vt+0xC4](&a2, i, &vec, bytes)   // 现场挑一格
//         h->SetBaseSpawnCell(*c)
//       else:
//         h->SetBaseSpawnCell(vec[idx])
//     释放 vec ; return 1
//
//   字段对应：
//     h->[+0x34]             -> HouseClass::Type
//     Type->[+0x1A6]         -> HouseTypeClass::MultiplayPassive
//                               （原版 CountryClass 视图把同一字节又叫
//                                humanControlled；与 StartingPositionsToHouseBases
//                                读的是同一个字段）
//     Type+0x24              -> AbstractTypeClass 的 ID 串（get_ID()）
//     Scenario->[0x1180+j*4] -> ScenarioClass::HouseIndices[16]
//     vt+0xC4                -> MPGameModeClass::SmthStartingHouses2
//     vec                    -> DynamicVectorClass<CellStruct>
//
//   原版对 vec[idx] 不做越界检查；本项目候选表是动态数组，这里保留
//   idx < Count 的防御（与 StartingPositionsToHouseBases 同一处理）。
// ----------------------------------------------------------------------------
void MPGameModeClass::AssignStartingPoints()
{
    ScenarioClass* pScen = ScenarioClass::Instance;
    if (pScen == nullptr)
        return;

    // asm: lea ecx,[esp+2Ch+vec_Cells] / call Get_Starting_Locations
    DynamicVectorClass<CellStruct> cells;
    pScen->GetStartingLocations(cells);

    // asm: 16 次 cmp [ecx],0FFFFFFFFh / mov [esp+eax+bytes],...
    uint8 positionsTaken[0x10];
    for (int32 i = 0; i < 0x10; ++i)
        positionsTaken[i] = (pScen->HouseIndices[i] != -1) ? 1 : 0;

    const int32 houseCount = HouseClass::ArrayCount;
    for (int32 i = 0; i < houseCount; ++i)
    {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse == nullptr)
            continue;

        // asm: mov ebp,[edi+34h] / mov al,[ebp+1A6h] / test al,al / jnz next
        if (pHouse->Type != nullptr && pHouse->Type->MultiplayPassive)
            continue;

        // asm: cmp [ecx],esi / mov bl,1 / mov edx,eax —— 不 break，后命中覆盖
        int32 slot = -1;
        for (int32 j = 0; j < 0x10; ++j)
        {
            if (pScen->HouseIndices[j] == i)
                slot = j;
        }

        if (slot < 0)
        {
            // asm: push country+24h / push esi / push aAssigningStart
            //      call WWDebugString
            const char* pName = (pHouse->Type != nullptr) ? pHouse->Type->get_ID() : "";
            WWDebugString("Assigning Starting Points for house %d (%s)", i, pName);

            // asm: push &bytes / push &vec / push esi / push &a2
            //      call [edx+0C4h]  ->  返回 wXY*，取 [eax] 作为出生格
            CellStruct picked(0, 0);
            CellStruct* pCell = SmthStartingHouses2(&picked, i, &cells, positionsTaken);
            if (pCell != nullptr)
                pHouse->Set_Base_Spawn_Cell(*pCell);
        }
        else if (slot < cells.Count)
        {
            // asm: mov ecx,[vec.data] / mov edx,[ecx+edx*4] / push edx
            //      call HouseClass_SetBaseSpawnCell
            pHouse->Set_Base_Spawn_Cell(cells[slot]);
        }
    }
}

// asm: vt_MPGameModeClass.CreateMPTeams   (asm:?)
// 两两结盟；队伍号为 0 表示未编队，不参与结盟。
void MPGameModeClass::CreateMPTeams()
{
    if (TheSession == nullptr) {
        return;
    }
    const int32 count = TheSession->GetPlayerCount();
    for (int32 i = 0; i < count; ++i) {
        SessionPlayer* pA = TheSession->GetMutablePlayer(i);
        if (pA == nullptr || !pA->Connected || pA->Team <= 0) {
            continue;
        }
        for (int32 j = i + 1; j < count; ++j) {
            SessionPlayer* pB = TheSession->GetMutablePlayer(j);
            if (pB == nullptr || !pB->Connected) {
                continue;
            }
            if (pB->Team == pA->Team) {
                SetAlliance(i, j, true);
            }
        }
    }
}

// 根据游戏行为，可知初始单位生成复用选人界面的出生单位入口，为每个
// 已连接且非观战的玩家槽位在其出生点落位初始载具（通常为基地车）。
void MPGameModeClass::CreateStartingUnits()
{
    if (TheSession == nullptr) {
        return;
    }
    const int32 count = TheSession->GetPlayerCount();
    for (int32 i = 0; i < count; ++i) {
        SessionPlayer* pPlayer = TheSession->GetMutablePlayer(i);
        if (pPlayer == nullptr || !pPlayer->Connected || pPlayer->IsObserver) {
            continue;
        }
        SpawnBaseUnit(i);
    }
}

// asm: vt_MPGameModeClass.DrawTeamSelector   (asm:?)
// 逐槽位刷新，保证下拉框与当前队伍号一致。
void MPGameModeClass::DrawTeamSelector()
{
    FillTeamSelector();
    if (TheSession == nullptr) {
        return;
    }
    const int32 count = TheSession->GetPlayerCount();
    for (int32 slot = 0; slot < count; ++slot) {
        FillTeamSelectorForSlot(slot);
    }
}

// ---------------------------------------------------------------------------
// asm: MPGameMode_SmthStartingHouses2 (0x5D6820, vftable +0xC4)
//
//   __thiscall + `retn 10h` → 4 个参数（旧实现按 3 个写，签名就是错的）：
//     [ebp+8]   outCell        结果写回的 CellStruct*
//     [ebp+0Ch] idxHouse       要分配席位的房主索引
//     [ebp+10h] cells          候选格表（+04 = 数组，+10h = 个数）
//     [ebp+14h] positionsTaken 每格是否已占用的字节表
//
//   逻辑逐段对应：
//     ① loc_5D68B4：扫 Scenario->HouseIndices[0..n) 找第一个 == -1 的槽。
//     ② loc_5D692E（全占）：`Random2Class::operator()(0, n-1)` 随机挑一格；
//        asm 之后还会查 vec_Houses[idxHouse] 是否人类、其 CSFName 能否在
//        NameNodeTypes 里命中、命中节点 [+6Bh] 是否为 -1，把 byte_ABFDD4
//        置 1（观战者标记）。
//     ③ loc_5D69F2（有空槽）：
//        - ABFDD4 已置 且 ABFDD5 未置 → 直接占这个空槽、置 ABFDD5；
//        - 人类玩家命中同名节点 → 走过 loc_5D6B86，两个标记都置；
//        - 否则 loc_5D6A85：在未占格中选"到所有已占格的距离和最大"的一格
//          （逐对 `sqrt(dx*dx+dy*dy)` 取整后累加，取最大者）。
//     ④ loc_5D6B3C：positionsTaken[slot]=1、HouseIndices[slot]=idxHouse、
//        outCell = cells[slot]，返回 outCell。
//
//   ⚠ NameNodeTypes 的元素结构与本项目 MPTeams.h 的 `HouseClass**` 声明
//   不一致（asm 里对元素用 `+4Bh/+4Fh/+6Bh` 取字段，又把它当 wchar_t* 传
//   给 _wcscmp，说明元素是"内联了名字缓冲的会话节点"）。该表在单机流程里
//   长度为 0，观战者分支不可达；这里保留结构、按长度判空，等会话节点结构
//   建模后再按 +0x6B 取值。
// ---------------------------------------------------------------------------
namespace {

// asm: byte_ABFDD4 / byte_ABFDD5 —— 观战者席位的一次性标记（A = 观战格已定，
// B = 第二个观战格已用掉）。
uint8 s_ObserverSlotFlagA = 0;
uint8 s_ObserverSlotFlagB = 0;

// asm: Scenario->HouseIndices 的"未登记"哨兵（0FFFFFFFFh）。
constexpr int32 kHouseSlotUnset = -1;

// 返回 true 表示 idxHouse 对应的人类玩家能在会话节点表里命中，且该节点的
// [+0x6B] == -1（观战者 / 随机国家标记）。
bool Is_Observer_House(int32 idxHouse)
{
    const int32 nodeCount = NameNodeTypes_Length();
    if (nodeCount <= 0)
        return false;

    HouseClass* pHouse = HouseClass::GetHouseByIndex(idxHouse);
    if (pHouse == nullptr || !pHouse->Is_Human())
        return false;

    HouseClass** nodes = NameNodeTypes_Data();
    if (nodes == nullptr)
        return false;

    for (int32 i = 0; i < nodeCount; ++i)
    {
        const uint8* pNode = reinterpret_cast<const uint8*>(nodes[i]);
        if (pNode == nullptr)
            continue;

        // asm: add eax,1602Ah / push eax —— 节点名与房子 CSFName 比较。
        // 节点前 0x1602A 字节不全是名字，这里按"元素首字段即名字缓冲"读，
        // 与 asm 把元素指针直接当 wchar_t* 的做法一致。
        if (wcscmp(pHouse->CSFName, reinterpret_cast<const wchar_t*>(pNode)) != 0)
            continue;

        // asm: cmp dword ptr [edx+6Bh], 0FFFFFFFFh
        int32 marker = 0;
        std::memcpy(&marker, pNode + 0x6B, sizeof(marker));
        return marker == -1;
    }
    return false;
}

}   // namespace

CellStruct* MPGameModeClass::SmthStartingHouses2(CellStruct* pOutCell, int32 idxHouse,
                                                DynamicVectorClass<CellStruct>* pCells,
                                                uint8* positionsTaken)
{
    if (pOutCell == nullptr || pCells == nullptr || positionsTaken == nullptr)
        return pOutCell;

    if (pCells->Items == nullptr || pCells->Count <= 0)
        return pOutCell;

    ScenarioClass* pScenario = ScenarioClass::Instance;
    if (pScenario == nullptr || pScenario->HouseIndices == nullptr)
        return pOutCell;

    const int32 count = pCells->Count;

    // ① 第一个未登记的槽
    int32 freeSlot = 0;
    while (freeSlot < count && pScenario->HouseIndices[freeSlot] != kHouseSlotUnset)
        ++freeSlot;

    int32 slot = -1;

    if (freeSlot == count)
    {
        // ② 全占：随机挑一格。asm 先把两个标记清零再判观战者。
        s_ObserverSlotFlagA = 0;
        s_ObserverSlotFlagB = 0;

        slot = pScenario->Random.Next(0, count - 1);   // asm: Random2Class(0, n-1)

        if (Is_Observer_House(idxHouse))
            s_ObserverSlotFlagA = 1;                    // asm: byte_ABFDD4 = 1
    }
    else if (s_ObserverSlotFlagA != 0 && s_ObserverSlotFlagB == 0)
    {
        // ③-1 观战者的第二个席位
        slot = freeSlot;
        s_ObserverSlotFlagB = 1;                        // asm: byte_ABFDD5 = 1
    }
    else if (Is_Observer_House(idxHouse))
    {
        // ③-2 loc_5D6B86：观战格，用当前空槽，两个标记都置
        slot = freeSlot;
        s_ObserverSlotFlagA = 1;
        s_ObserverSlotFlagB = 1;
    }
    else
    {
        // ③-3 loc_5D6A85：选"到所有已占格距离和最大"的未占格。
        //      asm 用 fild/fmul/faddp + float_sqrt + Float_To_Int_Floor，
        //      即每对算 floor(sqrt(dx^2+dy^2)) 后累加。
        int32 bestSum    = -1;
        int32 bestSlot   = -1;

        for (int32 b = 0; b < count; ++b)
        {
            if (positionsTaken[b] != 0)
                continue;

            int32 sum = 0;
            for (int32 s = 0; s < count; ++s)
            {
                if (positionsTaken[s] == 0)
                    continue;

                const int32 dx = static_cast<int32>(pCells->Items[b].X) - pCells->Items[s].X;
                const int32 dy = static_cast<int32>(pCells->Items[b].Y) - pCells->Items[s].Y;
                sum += static_cast<int32>(std::sqrt(static_cast<double>(dx * dx + dy * dy)));
            }

            if (sum > bestSum)
            {
                bestSum  = sum;
                bestSlot = b;
            }
        }

        slot = bestSlot;
    }

    if (slot < 0 || slot >= count)
        return pOutCell;

    // ④ 收尾
    positionsTaken[slot]              = 1;
    pScenario->HouseIndices[slot]     = idxHouse;
    *pOutCell                         = pCells->Items[slot];
    return pOutCell;
}

// 对应原版槽位 MPGameModeClass::func10：根据游戏行为，可知开局前会遍历
// 玩家名节点表，把选择“随机国家”(取值 -3)的节点国家槽清为 -1，其余
// 节点记录其在表中的序号，处理完返回成功。
bool MPGameModeClass::func10()
{
    if (TheSession == nullptr) {
        return true;
    }
    const int32 count = TheSession->GetPlayerCount();
    for (int32 i = 0; i < count; ++i) {
        SessionPlayer* pPlayer = TheSession->GetMutablePlayer(i);
        if (pPlayer == nullptr) {
            continue;
        }
        if (pPlayer->Side == -3) {
            pPlayer->Side = -1;
        } else {
            pPlayer->Side = i;
        }
    }
    return true;
}

// 根据游戏行为，可知该槽位固定应答“否”，用于关闭对应的功能开关。
bool MPGameModeClass::func40()
{
    return false;
}

// 对应原版槽位 MPGameModeClass::func54：根据游戏行为，可知这是网络消息
// 缓冲的释放回调——缓冲指针有效且第 5 个参数的高位字节(标志)非零时
// 释放缓冲，随后一律返回 0 表示处理完成。
int32 MPGameModeClass::func54(void* memory, int32 a2, int32 a3, int32 a4, int32 a5, int32 a6, int32 a7)
{
    (void)a2; (void)a3; (void)a4; (void)a6; (void)a7;
    if (memory != nullptr && ((static_cast<uint32>(a5) >> 8) & 0xFF) != 0) {
        delete[] static_cast<uint8*>(memory);
    }
    return 0;
}

// 对应原版槽位 MPGameModeClass::func58：与 func54 同族的消息缓冲释放
// 回调，仅调用约定携带的参数量不同（0x24 字节），判定与返回一致。
int32 MPGameModeClass::func58(void* memory, int32 a2, int32 a3, int32 a4, int32 a5, int32 a6, int32 a7, int32 a8, int32 a9)
{
    (void)a2; (void)a3; (void)a4; (void)a6; (void)a7; (void)a8; (void)a9;
    if (memory != nullptr && ((static_cast<uint32>(a5) >> 8) & 0xFF) != 0) {
        delete[] static_cast<uint8*>(memory);
    }
    return 0;
}

// 对应原版槽位 MPGameModeClass::func6C：根据游戏行为，可知该槽位以全局
// 会话对象为接收者，把会话设置转发给场景应用流程。
void MPGameModeClass::func6C()
{
    if (TheSession != nullptr) {
        TheSession->ApplySettingsToScenario();
    }
}

// 对应原版槽位 MPGameModeClass::func70：根据游戏行为，可知它只是把调用
// 转发回本对象的会话应用槽位（vtable +0x6C 处的转发桩），自身不产生
// 额外结果。
int32 MPGameModeClass::func70(int32 a1, int32 a2, int32 a3)
{
    (void)a1; (void)a2; (void)a3;
    func6C();
    return 0;
}

// 对应原版槽位 MPGameModeClass::func7C：根据游戏行为，可知进入战斗模式
// 前会清掉场景标志字中的 0x400 位（抹去上局遗留的特别标志），然后
// 报告成功。
bool MPGameModeClass::func7C()
{
    if (TheScenario != nullptr) {
        TheScenario->SpecialFlags.Raw &= ~0x400u;
    }
    return true;
}

// asm: vt_MPGameModeClass.ret_0__2   (asm:?)
// 各自返回固定常量，供选项判定使用。
int32 MPGameModeClass::ret0_2() { return 0; }
int32 MPGameModeClass::ret1_0() { return 1; }
int32 MPGameModeClass::ret1_2() { return 1; }
int32 MPGameModeClass::ret1_4() { return 1; }
int32 MPGameModeClass::ret1_5() { return 1; }

bool MPGameModeClass::funcB4(void* memory, int32 a2, int32 a3, int32 a4, int32 a5, int32 a6)
{
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    // 根据游戏行为，可知该槽位是战后清理入口：登记内存与附带标记
    // 都置位时释放内存，随后固定返回未处理。
    if (memory && (a6 & 0xFF00) != 0)
        delete[] static_cast<char*>(memory);
    return false;
}

// ============================================================================
// asm: MPGameModeClass_CreateList (0x5D7CC0)
//
//   MPGameModeClass_CreateList proc near     ; 无 ecx / 无栈参数 -> void()
//   调用者：InitGame（asm +1096）
//
//   loc_5D7CEC:                                ; once-guard byte_ABFD70 bit 0
//     mov al,byte_ABFD70 / test al,1 / jnz loc_5D7D1F
//     mov cl,al / or cl,1 / mov byte_ABFD70,cl
//     mov ecx,offset vec_MPGameModes / push 0 / push 0 / call sub_5D86D0
//     push offset sub_5D5B20 / call _atexit
//   loc_5D7D1F:
//     cmp esi,vec_MPGameModes.length / jge loc_5D7D3C
//     call vec_MPGameMode_CTOR / mov eax,[eax+4]      ; 取模式数组
//     mov ecx,[eax+esi*4] / test ecx,ecx / jz inc
//     mov edx,[ecx] / push 1 / call dword ptr [edx]   ; virtual[vt+0](mode,1)
//   loc_5D7D3C:                                ; 六次固定注册
//     "Battle" / "ManBattle" / "Siege" / "Unholy" / "FreeForAll" / "Cooperative"
//     title = alloc_str(NAME) ; init = Initializer<T>::vftable
//     MPGameMode_InitList(&title, &init) ; strdelete(&title)
//
// 铁律①（宁可留白，不写错）：vec_MPGameMode_CTOR / Initializer<T>::vftable
//   / MPGameMode_InitList / sub_5D5B20 四个依赖尚未落地，此处只固化已确证
//   的 once-guard 语义、六次注册的**顺序**与**模式名**（INI/名字必须一致），
//   不臆造引擎侧实现。
// ============================================================================
void MPGameModeClass::CreateList()
{
    // asm: loc_5D7CEC -- once-guard byte_ABFD70 bit 0
    static uint8 s_CreateListGuard = 0;                // asm: byte_ABFD70

    if ((s_CreateListGuard & 1) == 0) {
        // asm: mov cl,al / or cl,bl / mov byte_ABFD70,cl
        s_CreateListGuard |= 1;

        // asm: mov ecx,offset vec_MPGameModes / call sub_5D86D0 (0, 0)
        if (Array != nullptr)
            Array->Clear();                            // asm: vec ctor (0,0)

        // asm: push offset sub_5D5B20 / call _atexit
        //      依赖 sub_5D5B20（列表清理函数），尚未落地 -> 留白。
    }

    // asm: loc_5D7D1F -- 遍历已注册模式并刷新
    //   call vec_MPGameMode_CTOR / mov eax,[eax+4] ...
    //   依赖 vec_MPGameMode_CTOR，尚未落地 -> 留白。

    // asm: loc_5D7D3C -- 六次固定注册（名字与 INI 必须一致，铁律①）
    static const char* const kModeNames[6] = {
        "Battle",                                    // asm: aBattle
        "ManBattle",                                 // asm: aManbattle
        "Siege",                                     // asm: aSiege
        "Unholy",                                    // asm: aUnholy
        "FreeForAll",                                // asm: aFreeforall
        "Cooperative",                               // asm: aCooperative
    };

    for (int32 i = 0; i < 6; ++i) {
        // asm: mov [esp+..],offset Initializer<T>::vftable
        //      push offset aXxx / lea ecx,[title] / call alloc_str
        //      lea edx,[init] / lea ecx,[title] / call MPGameMode_InitList
        //      lea ecx,[title] / call strdelete
        // 依赖 Initializer<T>::vftable 与 MPGameMode_InitList，
        // 尚未落地 -> 留白。此处仅保留名字表与注册顺序。
        (void)kModeNames[i];
    }
}
