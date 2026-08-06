#include "SessionClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Rules/RulesClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"
#include "../IO/CCFileClass.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>

// ============================================================
// SessionClass
// ============================================================

static SessionClass* g_SessionInstance = nullptr;

SessionClass::SessionClass()
    : SessionID(0), GameState(GameSessionState::Lobby)
    , PlayerCount(0), MaxPlayerCount(8), MapName{}
    , GameSpeed(2), Credits(10000), UnitCount(0)
    , TechLevel(10), SuperWeapons(true), ShortGame(false)
    , BuildOffAlly(false), MCVRepacks(true), Crates(true)
    , BridgesDestroyable(true), FogOfWar(true)
    , CountdownTimer(5), CountdownStarted(false)
    , LobbyReadyCount(0), AutoStartEnabled(false)
    , AutoStartTimer(0), ReconnectEnabled(true)
    , ReconnectTimeout(15000), GameDuration(0)
    , AIPlayers(0), ScenarioIndex(-1), TournamentMode(false)
    , BattleMode(false), CampaignMode(false)
    , StartingCredits(10000), ShroudRegrows(true)
{
    for (int32 i = 0; i < MAX_SESSION_PLAYERS; ++i) {
        Players[i].ID = i;
        Players[i].Name[0] = '\0';
        Players[i].Side = -1;
        Players[i].Color = 0;
        Players[i].Team = -1;
        Players[i].Ready = false;
        Players[i].Connected = false;
        Players[i].IsAI = false;
        Players[i].StartingSpot = i;
        Players[i].IsObserver = false;
        Players[i].Latency = 0;
        Players[i].DisconnectCount = 0;
        Players[i].LastActive = 0;
    }
}

SessionClass::~SessionClass() {
    DestroySession();
}

SessionClass* SessionClass::GetInstance() {
    if (!g_SessionInstance) {
        g_SessionInstance = new SessionClass();
    }
    return g_SessionInstance;
}

void SessionClass::CreateSession(int32 maxPlayers) {
    SessionID = static_cast<int32>(SystemTimer::GetTime()) % 65536;
    GameState = GameSessionState::Lobby;
    MaxPlayerCount = maxPlayers;
    if (MaxPlayerCount < 2) MaxPlayerCount = 2;
    if (MaxPlayerCount > 8) MaxPlayerCount = 8;
    PlayerCount = 0;
    LobbyReadyCount = 0;
    CountdownStarted = false;
    CountdownTimer = 5;
    GameDuration = 0;
}

void SessionClass::DestroySession() {
    for (int32 i = 0; i < MAX_SESSION_PLAYERS; ++i) {
        RemovePlayer(i);
    }
    PlayerCount = 0;
    LobbyReadyCount = 0;
    GameState = GameSessionState::Lobby;
    SessionID = 0;
}

int32 SessionClass::AddPlayer(const char* name, int32 side, int32 color, int32 team) {
    if (PlayerCount >= MaxPlayerCount) return -1;

    for (int32 i = 0; i < MAX_SESSION_PLAYERS; ++i) {
        if (!Players[i].Connected) {
            Players[i].ID = i;
            if (name) {
                int32 j = 0;
                while (name[j] && j < 31) {
                    Players[i].Name[j] = name[j];
                    ++j;
                }
                Players[i].Name[j] = '\0';
            }
            Players[i].Side = side;
            Players[i].Color = color;
            Players[i].Team = team;
            Players[i].Ready = false;
            Players[i].Connected = true;
            Players[i].IsAI = false;
            Players[i].StartingSpot = i;
            Players[i].IsObserver = false;
            Players[i].Latency = 0;
            Players[i].DisconnectCount = 0;
            Players[i].LastActive = static_cast<int32>(SystemTimer::GetTime());
            ++PlayerCount;
            return i;
        }
    }
    return -1;
}

int32 SessionClass::AddAIPlayer(int32 side, int32 difficulty) {
    if (PlayerCount >= MaxPlayerCount) return -1;

    for (int32 i = 0; i < MAX_SESSION_PLAYERS; ++i) {
        if (!Players[i].Connected) {
            Players[i].ID = i;
            snprintf(Players[i].Name, sizeof(Players[i].Name), "AI-%d", i);
            Players[i].Side = side;
            Players[i].Color = i % 8;
            Players[i].Team = -1;
            Players[i].Ready = true;
            Players[i].Connected = true;
            Players[i].IsAI = true;
            Players[i].StartingSpot = i;
            Players[i].IsObserver = false;
            Players[i].Latency = 0;
            Players[i].DisconnectCount = 0;
            Players[i].LastActive = static_cast<int32>(SystemTimer::GetTime());
            ++PlayerCount;
            ++AIPlayers;
            return i;
        }
    }
    return -1;
}

bool SessionClass::RemovePlayer(int32 playerID) {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return false;
    if (!Players[playerID].Connected) return false;

    Players[playerID].Connected = false;
    Players[playerID].Ready = false;
    Players[playerID].Name[0] = '\0';
    Players[playerID].Side = -1;
    Players[playerID].Team = -1;
    if (Players[playerID].IsAI) {
        --AIPlayers;
    }
    Players[playerID].IsAI = false;
    Players[playerID].IsObserver = false;
    Players[playerID].DisconnectCount = 0;
    --PlayerCount;

    // Recalculate ready count
    CountReadyPlayers();
    return true;
}

void SessionClass::SetPlayerReady(int32 playerID, bool ready) {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return;
    if (!Players[playerID].Connected) return;

    Players[playerID].Ready = ready;
    CountReadyPlayers();

    if (ready && AutoStartEnabled) {
        CheckAutoStart();
    }
}

void SessionClass::SetPlayerSide(int32 playerID, int32 side) {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return;
    Players[playerID].Side = side;
}

void SessionClass::SetPlayerColor(int32 playerID, int32 color) {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return;
    Players[playerID].Color = color;
}

void SessionClass::SetPlayerTeam(int32 playerID, int32 team) {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return;
    Players[playerID].Team = team;
}

void SessionClass::SetPlayerStartingSpot(int32 playerID, int32 spot) {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return;
    Players[playerID].StartingSpot = spot;
}

void SessionClass::CountReadyPlayers() {
    LobbyReadyCount = 0;
    for (int32 i = 0; i < MAX_SESSION_PLAYERS; ++i) {
        if (Players[i].Connected && Players[i].Ready) {
            ++LobbyReadyCount;
        }
    }
}

bool SessionClass::AllPlayersReady() const {
    int32 humanCount = 0;
    for (int32 i = 0; i < MAX_SESSION_PLAYERS; ++i) {
        if (Players[i].Connected && !Players[i].IsAI) {
            ++humanCount;
        }
    }
    return LobbyReadyCount >= humanCount;
}

void SessionClass::CheckAutoStart() {
    if (AllPlayersReady() && PlayerCount >= 2) {
        if (AutoStartTimer <= 0) {
            AutoStartTimer = 10;
        }
    }
}

void SessionClass::SetMap(const char* mapName) {
    if (mapName) {
        int32 i = 0;
        while (mapName[i] && i < 63) {
            MapName[i] = mapName[i];
            ++i;
        }
        MapName[i] = '\0';
    }
}

void SessionClass::SetGameSpeed(int32 speed) {
    GameSpeed = speed;
    if (GameSpeed < 0) GameSpeed = 0;
    if (GameSpeed > 6) GameSpeed = 6;
}

void SessionClass::SetCredits(int32 credits) {
    Credits = credits;
    StartingCredits = credits;
    if (Credits < 0) Credits = 0;
    if (Credits > 100000) Credits = 100000;
}

void SessionClass::SetUnitCount(int32 count) {
    UnitCount = count;
    if (UnitCount < 0) UnitCount = 0;
    if (UnitCount > 500) UnitCount = 500;
}

void SessionClass::SetTechLevel(int32 level) {
    TechLevel = level;
    if (TechLevel < 1) TechLevel = 1;
    if (TechLevel > 10) TechLevel = 10;
}

void SessionClass::SetSuperWeaponsAllowed(bool allowed) {
    SuperWeapons = allowed;
}

void SessionClass::SetShortGame(bool shortGame) {
    ShortGame = shortGame;
}

void SessionClass::SetBuildOffAlly(bool allow) {
    BuildOffAlly = allow;
}

void SessionClass::SetMCVRepacks(bool allow) {
    MCVRepacks = allow;
}

void SessionClass::SetCrates(bool enable) {
    Crates = enable;
}

void SessionClass::SetBridgesDestroyable(bool destroyable) {
    BridgesDestroyable = destroyable;
}

void SessionClass::SetFogOfWar(bool fog) {
    FogOfWar = fog;
}

void SessionClass::SetShroudRegrows(bool regrows) {
    ShroudRegrows = regrows;
}

void SessionClass::SetTournamentMode(bool enable) {
    TournamentMode = enable;
}

void SessionClass::SetBattleMode(bool enable) {
    BattleMode = enable;
}

void SessionClass::SetCampaignMode(bool enable) {
    CampaignMode = enable;
}

void SessionClass::SetScenarioIndex(int32 index) {
    ScenarioIndex = index;
}

void SessionClass::SetAutoStart(bool enable) {
    AutoStartEnabled = enable;
    AutoStartTimer = 0;
}

void SessionClass::StartCountdown() {
    if (GameState != GameSessionState::Lobby) return;
    if (PlayerCount < 2) return;

    CountdownStarted = true;
    CountdownTimer = 5;
    GameState = GameSessionState::Countdown;
}

void SessionClass::UpdateCountdown() {
    if (!CountdownStarted) return;
    if (GameState != GameSessionState::Countdown) return;

    --CountdownTimer;
    if (CountdownTimer <= 0) {
        LaunchGame();
    }
}

void SessionClass::CancelCountdown() {
    CountdownStarted = false;
    CountdownTimer = 5;
    if (GameState == GameSessionState::Countdown) {
        GameState = GameSessionState::Lobby;
    }
}

void SessionClass::LaunchGame() {
    if (GameState != GameSessionState::Countdown) return;
    GameState = GameSessionState::Playing;
    GameDuration = 0;
    CountdownStarted = false;
}

void SessionClass::EndGame() {
    GameState = GameSessionState::Lobby;
    CountdownStarted = false;
    CountdownTimer = 5;
    GameDuration = 0;
    LobbyReadyCount = 0;
    for (int32 i = 0; i < MAX_SESSION_PLAYERS; ++i) {
        Players[i].Ready = false;
    }
}

void SessionClass::UpdateGame() {
    if (GameState == GameSessionState::Playing) {
        ++GameDuration;
    }

    if (CountdownStarted && GameState == GameSessionState::Countdown) {
        UpdateCountdown();
    }

    if (AutoStartEnabled && GameState == GameSessionState::Lobby) {
        if (AutoStartTimer > 0) {
            --AutoStartTimer;
            if (AutoStartTimer <= 0 && AllPlayersReady() && PlayerCount >= 2) {
                StartCountdown();
            }
        }
    }

    // Handle disconnections
    for (int32 i = 0; i < MAX_SESSION_PLAYERS; ++i) {
        if (Players[i].Connected && !Players[i].IsAI) {
            int32 currentTime = static_cast<int32>(SystemTimer::GetTime());
            int32 elapsed = currentTime - Players[i].LastActive;
            if (elapsed > ReconnectTimeout && Players[i].DisconnectCount > 0) {
                if (ReconnectEnabled) {
                    // Try to re-establish connection
                    Players[i].DisconnectCount++;
                } else {
                    RemovePlayer(i);
                }
            }
        }
    }
}

void SessionClass::HandleDisconnect(int32 playerID) {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return;
    if (!Players[playerID].Connected) return;

    Players[playerID].DisconnectCount++;
    Players[playerID].LastActive = static_cast<int32>(SystemTimer::GetTime());

    if (ReconnectEnabled) {
        // Mark for reconnection
        Players[playerID].Ready = false;
        CountReadyPlayers();
    }

    if (Players[playerID].DisconnectCount >= 3) {
        RemovePlayer(playerID);
    }
}

bool SessionClass::HandleReconnection(int32 playerID) {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return false;
    if (Players[playerID].Connected) return true;

    Players[playerID].Connected = true;
    Players[playerID].DisconnectCount = 0;
    Players[playerID].LastActive = static_cast<int32>(SystemTimer::GetTime());
    return true;
}

void SessionClass::TogglePlayerReady(int32 playerID) {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return;
    SetPlayerReady(playerID, !Players[playerID].Ready);
}

void SessionClass::KickPlayer(int32 playerID) {
    RemovePlayer(playerID);
}

void SessionClass::BanPlayer(int32 playerID) {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return;
    Players[playerID].Connected = false;
    Players[playerID].Ready = false;
    Players[playerID].Name[0] = '\0';
    Players[playerID].DisconnectCount = -1;
    if (Players[playerID].IsAI) --AIPlayers;
    Players[playerID].IsAI = false;
    --PlayerCount;
    CountReadyPlayers();
}

void SessionClass::SetReconnectTimeout(int32 timeoutMs) {
    ReconnectTimeout = timeoutMs;
    if (ReconnectTimeout < 1000) ReconnectTimeout = 1000;
    if (ReconnectTimeout > 60000) ReconnectTimeout = 60000;
}

const char* SessionClass::GetMapName() const {
    return MapName;
}

GameSessionState SessionClass::GetGameState() const {
    return GameState;
}

int32 SessionClass::GetPlayerCount() const {
    return PlayerCount;
}

int32 SessionClass::GetMaxPlayers() const {
    return MaxPlayerCount;
}

int32 SessionClass::GetReadyCount() const {
    return LobbyReadyCount;
}

const SessionPlayer* SessionClass::GetPlayer(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return nullptr;
    if (!Players[playerID].Connected) return nullptr;
    return &Players[playerID];
}

int32 SessionClass::GetGameSpeed() const {
    return GameSpeed;
}

int32 SessionClass::GetCredits() const {
    return Credits;
}

int32 SessionClass::GetUnitCount() const {
    return UnitCount;
}

int32 SessionClass::GetTechLevel() const {
    return TechLevel;
}

bool SessionClass::IsSuperWeaponsAllowed() const {
    return SuperWeapons;
}

bool SessionClass::IsShortGame() const {
    return ShortGame;
}

bool SessionClass::IsBuildOffAlly() const {
    return BuildOffAlly;
}

bool SessionClass::IsMCVRepacks() const {
    return MCVRepacks;
}

bool SessionClass::IsCratesEnabled() const {
    return Crates;
}

bool SessionClass::IsBridgesDestroyable() const {
    return BridgesDestroyable;
}

bool SessionClass::IsFogOfWar() const {
    return FogOfWar;
}

bool SessionClass::IsShroudRegrows() const {
    return ShroudRegrows;
}

bool SessionClass::IsTournamentMode() const {
    return TournamentMode;
}

bool SessionClass::IsBattleMode() const {
    return BattleMode;
}

bool SessionClass::IsCampaignMode() const {
    return CampaignMode;
}

int32 SessionClass::GetScenarioIndex() const {
    return ScenarioIndex;
}

bool SessionClass::ApplySettingsToScenario() {
    if (!ScenarioClass::Instance) return false;

    // Apply all multiplayer session settings to the scenario object.
    ScenarioClass::Instance->Difficulty = GameSpeed;
    ScenarioClass::Instance->TechLevel = TechLevel;
    ScenarioClass::Instance->IsShortGame = ShortGame;
    ScenarioClass::Instance->IsMCVRepack = MCVRepacks;
    ScenarioClass::Instance->IsCrates = Crates;
    ScenarioClass::Instance->IsBridgeDestructionEnabled = BridgesDestroyable;
    ScenarioClass::Instance->IsFogOfWar = FogOfWar;
    ScenarioClass::Instance->IsSuperWeapons = SuperWeapons;

    return true;
}

void SessionClass::ResetSession() {
    DestroySession();
    CreateSession(MaxPlayerCount);
}

void SessionClass::SetupFromLobby() {
    if (GameState != GameSessionState::Lobby) return;
    CountReadyPlayers();
    if (AllPlayersReady() && PlayerCount >= 2) {
        StartCountdown();
    }
}

void SessionClass::SetStartingCredits(int32 credits) {
    StartingCredits = credits;
    if (StartingCredits < 0) StartingCredits = 0;
    if (StartingCredits > 100000) StartingCredits = 100000;
}

int32 SessionClass::GetStartingCredits() const {
    return StartingCredits;
}

int32 SessionClass::GetAIPlayerCount() const {
    return AIPlayers;
}

int32 SessionClass::GetHumanPlayerCount() const {
    return PlayerCount - AIPlayers;
}

int32 SessionClass::GetGameDuration() const {
    return GameDuration;
}

int32 SessionClass::GetCountdownTimer() const {
    return CountdownTimer;
}

bool SessionClass::IsCountdownStarted() const {
    return CountdownStarted;
}

bool SessionClass::IsReconnectEnabled() const {
    return ReconnectEnabled;
}

int32 SessionClass::GetReconnectTimeout() const {
    return ReconnectTimeout;
}

void SessionClass::SetReconnectEnabled(bool enabled) {
    ReconnectEnabled = enabled;
}

void SessionClass::UpdatePlayerActivity(int32 playerID) {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return;
    if (!Players[playerID].Connected) return;
    Players[playerID].LastActive = static_cast<int32>(SystemTimer::GetTime());
}

bool SessionClass::IsPlayerConnected(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return false;
    return Players[playerID].Connected;
}

bool SessionClass::IsPlayerReady(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return false;
    return Players[playerID].Ready;
}

bool SessionClass::IsPlayerAI(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return false;
    return Players[playerID].IsAI;
}

int32 SessionClass::GetPlayerSide(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return -1;
    return Players[playerID].Side;
}

int32 SessionClass::GetPlayerColor(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return -1;
    return Players[playerID].Color;
}

int32 SessionClass::GetPlayerTeam(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return -1;
    return Players[playerID].Team;
}

int32 SessionClass::GetPlayerStartingSpot(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return -1;
    return Players[playerID].StartingSpot;
}

const char* SessionClass::GetPlayerName(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return nullptr;
    if (!Players[playerID].Connected) return nullptr;
    return Players[playerID].Name;
}

int32 SessionClass::GetPlayerLatency(int32 playerID) const {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return -1;
    return Players[playerID].Latency;
}

void SessionClass::SetPlayerLatency(int32 playerID, int32 latency) {
    if (playerID < 0 || playerID >= MAX_SESSION_PLAYERS) return;
    Players[playerID].Latency = latency;
}

// ============================================================
// LobbyGameClass
// ============================================================

LobbyGameClass::LobbyGameClass()
    : GameName{}, MapName{}, HostName{}
    , PlayerCount(0), MaxPlayers(0), GameSpeed(0)
    , IsPassworded(false), IsStarted(false), PingTime(0) {
}

void LobbyGameClass::SetGameInfo(const char* name, const char* map, const char* host,
    int32 players, int32 maxPlayers, int32 speed, bool passworded, bool started) {
    if (name) {
        int32 i = 0;
        while (name[i] && i < 63) { GameName[i] = name[i]; ++i; }
        GameName[i] = '\0';
    }
    if (map) {
        int32 i = 0;
        while (map[i] && i < 63) { MapName[i] = map[i]; ++i; }
        MapName[i] = '\0';
    }
    if (host) {
        int32 i = 0;
        while (host[i] && i < 63) { HostName[i] = host[i]; ++i; }
        HostName[i] = '\0';
    }
    PlayerCount = players;
    MaxPlayers = maxPlayers;
    GameSpeed = speed;
    IsPassworded = passworded;
    IsStarted = started;
}

void LobbyGameClass::SetPingTime(int32 ping) {
    PingTime = ping;
}

int32 LobbyGameClass::GetPingTime() const {
    return PingTime;
}

const char* LobbyGameClass::GetGameName() const {
    return GameName;
}

const char* LobbyGameClass::GetMapName() const {
    return MapName;
}

const char* LobbyGameClass::GetHostName() const {
    return HostName;
}

int32 LobbyGameClass::GetPlayerCount() const {
    return PlayerCount;
}

int32 LobbyGameClass::GetMaxPlayers() const {
    return MaxPlayers;
}

bool LobbyGameClass::IsPasswordProtected() const {
    return IsPassworded;
}

bool LobbyGameClass::HasStarted() const {
    return IsStarted;
}

int32 LobbyGameClass::GetGameSpeed() const {
    return GameSpeed;
}