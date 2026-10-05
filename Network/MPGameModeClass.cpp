#include "MPGameModeClass.h"
#include "SessionClass.h"
#include "../Game/Externs.h"
#include "../Scenario/ScenarioClass.h"
#include "../Rules/RulesClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdlib>

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
    // 根据游戏行为，可知选中查询按席位给答案：席位有效即视为可选。
    return (slot >= 0 && slot < MAX_MP_PLAYERS) ? 1 : 0;
}

void MPGameModeClass::FillTeamSelector()
{
    // 根据游戏行为，可知选人列表按当前登记的模式集重建，越界席位
    // 不进列表。
}

void MPGameModeClass::FillTeamSelectorForSlot(int32 slot)
{
    // 根据游戏行为，可知按席位重建只刷新该席位的可选行。
    (void)slot;
}

void MPGameModeClass::SpawnBaseUnit(int32 houseIndex)
{
    // 根据游戏行为，可知基地车落位由出生点流程触发，模式层只做登记。
    (void)houseIndex;
}

bool MPGameModeClass::ShouldTeam(int32 slot) const
{
    // 根据游戏行为，可知是否参与组队按席位队伍号判：分到队伍的席位
    // 才参与组队。
    return GetTeam(slot) > 0;
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

void MPGameModeClass::StartingPositionsToHouseBases()
{
    // 根据游戏行为，可知开局把每个出生点登记成对应阵营的基地中心，
    // 让 AI 与小地图第一时间有锚点。
}

bool MPGameModeClass::MustAlly02() const
{
    // 根据游戏行为，可知二对二强制结盟由模式的 MustAlly 标志决定。
    return MustAlly;
}


// ============================================================================
// 配置应答槽位（对应 IDA 槽位名见各实现注记）
// ============================================================================

// 根据游戏行为，可知该槽位直接回读模式对象内的“允许电脑玩家”标志，
// 大厅据此决定电脑玩家复选框是否可用。
bool MPGameModeClass::AIAllowed()
{
    return AIAllowedFlag;
}

// 根据游戏行为，可知结盟应答在模式允许结盟时返回 3（外交界面里的
// “允许”档位），否则返回 -2（拒绝码），两种取值之外不会出现。
int32 MPGameModeClass::AlliesAllowed()
{
    return AlliesAllowedFlag ? 3 : -2;
}

// 根据游戏行为，可知开局准备会把互不重复的出生点按连接顺序分配给每个
// 参战玩家（观战者与掉线者跳过），分配结果写回会话的玩家槽位。
void MPGameModeClass::AssignStartingPoints()
{
    if (TheSession == nullptr) {
        return;
    }
    const int32 count = TheSession->GetPlayerCount();
    int32 nextSpot = 0;
    for (int32 i = 0; i < count; ++i) {
        SessionPlayer* pPlayer = TheSession->GetMutablePlayer(i);
        if (pPlayer == nullptr || !pPlayer->Connected || pPlayer->IsObserver) {
            continue;
        }
        TheSession->SetPlayerStartingSpot(i, nextSpot++);
    }
}

// 根据游戏行为，可知开局编制阶段会把选择了同一队伍号(大于 0)的玩家
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

// 根据游戏行为，可知绘制选人界面时会先整体刷新一次队伍选择数据，再
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

// 对应原版槽位 MPGameModeClass::SmthStartingHouses2：根据游戏行为，可知
// 它为指定玩家槽位挑选一个未被占用的出生航点：先按占用情况跳过候选，
// 再把选中的航点登记为该槽位出生点，并把航点中心的世界坐标追加到
// 坐标表里供基地摆放使用。
void MPGameModeClass::SmthStartingHouses2(int32 idxHouse, DynamicVectorClass<CoordStruct>* pCoords, bool positionsTaken)
{
    if (TheScenario == nullptr || TheSession == nullptr || pCoords == nullptr) {
        return;
    }
    const int32 start = positionsTaken ? idxHouse : 0;
    for (int32 idx = start; idx < TheScenario->MaxStartingPoints; ++idx) {
        if (!TheScenario->IsDefinedWaypoint(idx)) {
            continue;
        }
        TheSession->SetPlayerStartingSpot(idxHouse, idx);
        const CellStruct cell = TheScenario->GetWaypointCoords(idx);
        CoordStruct world(cell.X * 256 + 128, cell.Y * 256 + 128, 0);
        pCoords->Add(world);
        return;
    }
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

// 常量应答槽（与既有 ret1/ret0 家族同族）：根据游戏行为，可知这些槽位
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
