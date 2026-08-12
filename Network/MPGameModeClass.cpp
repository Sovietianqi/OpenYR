#include "MPGameModeClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Rules/RulesClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"

#include <cstring>
#include <cstdlib>

// ============================================================
// MPGameModeClass
// ============================================================

static MPGameModeClass* g_MPGameModeInstance = nullptr;

MPGameModeClass::MPGameModeClass()
    : GameMode(MultiplayerGameMode::FreeForAll)
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
{
    for (int32 i = 0; i < MAX_TEAMS; ++i) {
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
    if (team < 0 || team >= MAX_TEAMS) return;
    PlayerTeam[playerID] = team;
}

int32 MPGameModeClass::GetTeam(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_MP_PLAYERS) return -1;
    return PlayerTeam[playerID];
}

void MPGameModeClass::UpdateScores() {
    for (int32 i = 0; i < MAX_TEAMS; ++i) {
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
        if (team >= 0 && team < MAX_TEAMS) {
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
    if (team < 0 || team >= MAX_TEAMS) return 0;
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
    for (int32 i = 0; i < MAX_TEAMS; ++i) {
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
    if (team < 0 || team >= MAX_TEAMS) return 0;
    return TeamKills[team];
}

int32 MPGameModeClass::GetTeamLosses(int32 team) const {
    if (team < 0 || team >= MAX_TEAMS) return 0;
    return TeamLosses[team];
}

int32 MPGameModeClass::GetTeamUnitCount(int32 team) const {
    if (team < 0 || team >= MAX_TEAMS) return 0;
    return TeamUnits[team];
}

int32 MPGameModeClass::GetTeamBuildingCount(int32 team) const {
    if (team < 0 || team >= MAX_TEAMS) return 0;
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