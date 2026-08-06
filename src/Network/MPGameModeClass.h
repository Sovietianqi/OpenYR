#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

static constexpr int32 MAX_MP_PLAYERS = 8;
static constexpr int32 MAX_MP_TEAMS = 4;

enum class MultiplayerGameMode : int32 {
    FreeForAll = 0,
    Cooperative = 1,
    TeamGame = 2,
    Battle = 3,
    Tournament = 4,
    Megawealth = 5,
    Duel = 6,
    MeatGrind = 7,
    NavalWar = 8,
    AirWar = 9,
    FFA = 10,
    Coop = 11,
    Count = 12
};

enum class VictoryType : int32 {
    DestroyAll = 0,
    Score = 1,
    Time = 2,
    Campaign = 3,
    Custom = 4
};

class MPGameModeClass {
public:
    MPGameModeClass();
    ~MPGameModeClass();

    static MPGameModeClass* GetInstance();

    void SetGameMode(MultiplayerGameMode mode);
    void ApplyModeDefaults();
    MultiplayerGameMode GetGameMode() const;

    void SetAlliance(int32 player1, int32 player2, bool allied);
    bool IsAllied(int32 player1, int32 player2) const;
    void LockAlliances();
    void UnlockAlliances();
    void SetAllianceLockTime(int32 minutes);
    void UpdateAlliances();

    void SetTeam(int32 playerID, int32 team);
    int32 GetTeam(int32 playerID) const;

    void UpdateScores();
    int32 GetPlayerScore(int32 playerID) const;
    int32 GetTeamScore(int32 team) const;
    void AddPlayerKill(int32 playerID);
    void AddPlayerLoss(int32 playerID);
    void AddPlayerEconomy(int32 playerID, int32 amount);
    void UpdatePlayerUnits(int32 playerID, int32 count);
    void UpdatePlayerBuildings(int32 playerID, int32 count);
    void SetPlayerAlive(int32 playerID, bool alive);
    bool IsPlayerAlive(int32 playerID) const;
    bool IsPlayerEliminated(int32 playerID) const;

    int32 CheckWinCondition();
    int32 CheckDestroyAllWin();
    int32 CheckScoreWin();
    int32 CheckTimeWin();
    int32 CheckCampaignWin();
    int32 CheckCustomWin();

    void SetScoreLimit(int32 limit);
    void SetTimeLimit(int32 minutes);
    void SetStartingCredits(int32 credits);
    int32 GetStartingCredits() const;
    void SetStartingUnits(int32 count);
    int32 GetStartingUnits() const;
    void SetMapRevealed(bool revealed);
    void SetAlliesRevealed(bool revealed);
    void SetNoSuperWeapons(bool disabled);
    void SetNoMCV(bool disabled);
    void SetNoInfantry(bool disabled);
    void SetNoVehicles(bool disabled);
    void SetNoNavy(bool disabled);
    void SetNoAircraft(bool disabled);
    void SetNoBuildings(bool disabled);
    void SetNoDefenses(bool disabled);
    void SetPreBuiltBase(bool enabled);
    void SetBaseTemplateIndex(int32 index);
    void SetRandomStartingPositions(bool random);
    void SetAllowObservers(bool allow);
    void SetRanked(bool ranked);
    void SetDedicatedServer(bool dedicated);
    void SetBattleLAN(bool lan);
    void SetSharedTech(bool shared);
    void SetRevealRadius(int32 radius);
    void SetUseMapReveal(bool use);

    void UpdateGameTime();
    int32 GetGameTime() const;
    int32 GetGameTimeMinutes() const;
    int32 GetGameTimeSeconds() const;

    bool IsMapRevealed() const;
    bool IsAlliesRevealed() const;
    bool IsAllianceLocked() const;
    bool IsTeamVictory() const;
    bool IsNoSuperWeapons() const;
    bool IsNoMCV() const;
    bool IsNoInfantry() const;
    bool IsNoVehicles() const;
    bool IsNoNavy() const;
    bool IsNoAircraft() const;
    bool IsNoBuildings() const;
    bool IsNoDefenses() const;
    bool IsPreBuiltBase() const;
    int32 GetBaseTemplateIndex() const;
    bool IsRandomStartingPositions() const;
    bool IsAllowObservers() const;
    bool IsRanked() const;
    bool IsDedicatedServer() const;
    bool IsBattleLAN() const;
    bool IsSharedTech() const;
    int32 GetRevealRadius() const;
    bool IsUseMapReveal() const;

    void ResetGame();
    int32 GetMaxPlayers() const;
    int32 GetMinPlayers() const;
    void SetMaxPlayers(int32 count);
    void SetMinPlayers(int32 count);

    int32 GetPlayerKills(int32 playerID) const;
    int32 GetPlayerLosses(int32 playerID) const;
    int32 GetPlayerEconomy(int32 playerID) const;
    int32 GetPlayerUnitCount(int32 playerID) const;
    int32 GetPlayerBuildingCount(int32 playerID) const;
    int32 GetTeamKills(int32 team) const;
    int32 GetTeamLosses(int32 team) const;
    int32 GetTeamUnitCount(int32 team) const;
    int32 GetTeamBuildingCount(int32 team) const;
    int32 GetScoreLimit() const;
    int32 GetTimeLimit() const;
    VictoryType GetVictoryCondition() const;
    void SetVictoryCondition(VictoryType type);
    void SetTeamVictory(bool team);
    int32 GetObserverCount() const;
    void AddObserver();
    void RemoveObserver();

    MultiplayerGameMode GameMode;
    int32 MaxPlayers;
    int32 MinPlayers;
    int32 StartingCredits;
    int32 StartingUnits;
    bool MapRevealed;
    bool AlliesRevealed;
    int32 ScoreLimit;
    int32 TimeLimit;
    int32 GameTime;
    bool AllianceLocked;
    int32 AllianceLockedAfter;
    bool RandomStartingPositions;
    bool AllowObservers;
    int32 ObserverCount;
    bool PreBuiltBase;
    int32 BaseTemplateIndex;
    bool OneVsOne;
    bool Ranked;
    bool DedicatedServer;
    bool BattleLAN;
    VictoryType VictoryCondition;
    bool TeamVictory;
    bool SharedTech;
    bool UseMapReveal;
    int32 RevealRadius;
    bool NoSuperWeapons;
    bool NoMCV;
    bool NoInfantry;
    bool NoVehicles;
    bool NoNavy;
    bool NoAircraft;
    bool NoBuildings;
    bool NoDefenses;

    int32 PlayerScores[MAX_MP_PLAYERS];
    int32 PlayerKills[MAX_MP_PLAYERS];
    int32 PlayerLosses[MAX_MP_PLAYERS];
    int32 PlayerEconomy[MAX_MP_PLAYERS];
    int32 PlayerUnits[MAX_MP_PLAYERS];
    int32 PlayerBuildings[MAX_MP_PLAYERS];
    bool PlayerAlive[MAX_MP_PLAYERS];
    int32 PlayerTeam[MAX_MP_PLAYERS];
    bool PlayerEliminated[MAX_MP_PLAYERS];
    bool Alliances[MAX_MP_PLAYERS][MAX_MP_PLAYERS];

    int32 TeamScores[MAX_MP_TEAMS];
    int32 TeamUnits[MAX_MP_TEAMS];
    int32 TeamBuildings[MAX_MP_TEAMS];
    int32 TeamKills[MAX_MP_TEAMS];
    int32 TeamLosses[MAX_MP_TEAMS];
    bool TeamAlive[MAX_MP_TEAMS];
};