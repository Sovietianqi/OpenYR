#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"
#include "../Math/Timer.h"

static constexpr int32 MAX_SESSION_PLAYERS = 8;

enum class GameSessionState : int32 {
    Lobby = 0,
    Countdown = 1,
    Playing = 2,
    Paused = 3,
    Ended = 4
};

struct SessionPlayer {
    int32 ID;
    char Name[32];
    int32 Side;
    int32 Color;
    int32 Team;
    bool Ready;
    bool Connected;
    bool IsAI;
    int32 StartingSpot;
    bool IsObserver;
    int32 Latency;
    int32 DisconnectCount;
    int32 LastActive;
    // Extended fields for the original 46-event sync protocol
    int32 OptionsFlags;
    int32 LastFrame;
    int32 ProcessTime;
    int32 LatencyFudge;
    int32 Address;
    bool  IsLeaving;
};

class SessionClass {
public:
    SessionClass();
    ~SessionClass();

    static SessionClass* GetInstance();

    void CreateSession(int32 maxPlayers);
    void DestroySession();

    int32 AddPlayer(const char* name, int32 side, int32 color, int32 team);
    int32 AddAIPlayer(int32 side, int32 difficulty);
    bool RemovePlayer(int32 playerID);
    void SetPlayerReady(int32 playerID, bool ready);
    void SetPlayerSide(int32 playerID, int32 side);
    void SetPlayerColor(int32 playerID, int32 color);
    void SetPlayerTeam(int32 playerID, int32 team);
    void SetPlayerStartingSpot(int32 playerID, int32 spot);

    void CountReadyPlayers();
    bool AllPlayersReady() const;
    void CheckAutoStart();

    void SetMap(const char* mapName);
    void SetGameSpeed(int32 speed);
    void SetCredits(int32 credits);
    void SetUnitCount(int32 count);
    void SetTechLevel(int32 level);
    void SetSuperWeaponsAllowed(bool allowed);
    void SetShortGame(bool shortGame);
    void SetBuildOffAlly(bool allow);
    void SetMCVRepacks(bool allow);
    void SetCrates(bool enable);
    void SetBridgesDestroyable(bool destroyable);
    void SetFogOfWar(bool fog);
    void SetShroudRegrows(bool regrows);
    void SetTournamentMode(bool enable);
    void SetBattleMode(bool enable);
    void SetCampaignMode(bool enable);
    void SetScenarioIndex(int32 index);
    void SetAutoStart(bool enable);

    void StartCountdown();
    void UpdateCountdown();
    void CancelCountdown();
    void LaunchGame();
    void EndGame();
    void UpdateGame();

    void HandleDisconnect(int32 playerID);
    bool HandleReconnection(int32 playerID);
    void TogglePlayerReady(int32 playerID);
    void KickPlayer(int32 playerID);
    void BanPlayer(int32 playerID);
    void SetReconnectTimeout(int32 timeoutMs);

    const char* GetMapName() const;
    GameSessionState GetGameState() const;
    int32 GetPlayerCount() const;
    int32 GetMaxPlayers() const;
    int32 GetReadyCount() const;
    const SessionPlayer* GetPlayer(int32 playerID) const;
    SessionPlayer* GetMutablePlayer(int32 playerID);
    bool IsCoopGame() const;
    void SetPendingSave(bool pending);
    int32 MaxLatencyFudge;
    int32 GetGameSpeed() const;
    int32 GetCredits() const;
    int32 GetUnitCount() const;
    int32 GetTechLevel() const;
    bool IsSuperWeaponsAllowed() const;
    bool IsShortGame() const;
    bool IsBuildOffAlly() const;
    bool IsMCVRepacks() const;
    bool IsCratesEnabled() const;
    bool IsBridgesDestroyable() const;
    bool IsFogOfWar() const;
    bool IsShroudRegrows() const;
    bool IsTournamentMode() const;
    bool IsBattleMode() const;
    bool IsCampaignMode() const;
    int32 GetScenarioIndex() const;

    bool ApplySettingsToScenario();
    void ResetSession();
    void SetupFromLobby();

    void SetStartingCredits(int32 credits);
    int32 GetStartingCredits() const;
    int32 GetAIPlayerCount() const;
    int32 GetHumanPlayerCount() const;
    int32 GetGameDuration() const;
    int32 GetCountdownTimer() const;
    bool IsCountdownStarted() const;
    bool IsReconnectEnabled() const;
    int32 GetReconnectTimeout() const;
    void SetReconnectEnabled(bool enabled);

    void UpdatePlayerActivity(int32 playerID);
    bool IsPlayerConnected(int32 playerID) const;
    bool IsPlayerReady(int32 playerID) const;
    bool IsPlayerAI(int32 playerID) const;
    int32 GetPlayerSide(int32 playerID) const;
    int32 GetPlayerColor(int32 playerID) const;
    int32 GetPlayerTeam(int32 playerID) const;
    int32 GetPlayerStartingSpot(int32 playerID) const;
    const char* GetPlayerName(int32 playerID) const;
    int32 GetPlayerLatency(int32 playerID) const;
    void SetPlayerLatency(int32 playerID, int32 latency);

    int32 SessionID;
    GameSessionState GameState;
    int32 PlayerCount;
    int32 MaxPlayerCount;
    SessionPlayer Players[MAX_SESSION_PLAYERS];
    char MapName[64];
    int32 GameSpeed;
    int32 Credits;
    int32 UnitCount;
    int32 TechLevel;
    bool SuperWeapons;
    bool ShortGame;
    bool BuildOffAlly;
    bool MCVRepacks;
    bool Crates;
    bool BridgesDestroyable;
    bool FogOfWar;
    int32 CountdownTimer;
    bool CountdownStarted;
    int32 LobbyReadyCount;
    bool AutoStartEnabled;
    int32 AutoStartTimer;
    bool ReconnectEnabled;
    int32 ReconnectTimeout;
    int32 GameDuration;
    int32 AIPlayers;
    int32 ScenarioIndex;
    bool TournamentMode;
    bool BattleMode;
    bool CampaignMode;
    int32 StartingCredits;
    bool ShroudRegrows;
    bool CoopGame;
    bool PendingSave;
    int32 HostID;

public:
    int32 GetHostID() const { return HostID; }
    void SetHostPlayer(int32 hostID) { HostID = hostID; }
};

class LobbyGameClass {
public:
    LobbyGameClass();

    void SetGameInfo(const char* name, const char* map, const char* host,
        int32 players, int32 maxPlayers, int32 speed, bool passworded, bool started);
    void SetPingTime(int32 ping);
    int32 GetPingTime() const;

    const char* GetGameName() const;
    const char* GetMapName() const;
    const char* GetHostName() const;
    int32 GetPlayerCount() const;
    int32 GetMaxPlayers() const;
    bool IsPasswordProtected() const;
    bool HasStarted() const;
    int32 GetGameSpeed() const;

    char GameName[64];
    char MapName[64];
    char HostName[64];
    int32 PlayerCount;
    int32 MaxPlayers;
    int32 GameSpeed;
    bool IsPassworded;
    bool IsStarted;
    int32 PingTime;
};