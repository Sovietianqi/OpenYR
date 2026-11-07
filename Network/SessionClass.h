#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"
#include "../Math/Timer.h"
#include "GameTypePrefsClass.h"

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
    int32 Port;
    int32 PortLastProbeTime;
    bool  IsLeaving;
    bool  IsPortReachable;
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

    // ------------------------------------------------------------------------
    // asm: SessionClass_Progess_5EF930 (0x5EF930, thiscall, retn 4)
    //   多人载入进度泵。随机地图先把目标位置折半，推进进度条，然后按会话
    //   类型分派：类型 3/4 走 IPX 广播"保证进度"包，类型 1/2 走连接对话框；
    //   每次循环睡 20ms，直到 0xF0 的预算耗尽或服务计数降到 5 以下。
    //   进来的闸门：Debug_Map 置位直接返回；[this+284Ch]==1 也直接返回。
    // ------------------------------------------------------------------------
    void Progress(int32 pos);

    // ── Preferences (RA2MD.INI) ─────────────────────────────────────────
    //
    //  SessionClass::Read_MultiPlayer_Settings (asm 0x698074) restores the
    //  persistent multiplayer configuration from RA2MD.INI.  The three
    //  per-mode preference blocks (Skirmish / LAN / WonlinePref) are read
    //  through Game_GetGameTypePrefs.
    void ReadMultiPlayerSettings();
    void ReadSerialDefaults();
    void ReadSyncBugSettings();

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
    // asm +0x0000 —— 会话类型（1..4）：Progress 的 4 分支 switch 就开着它，
    //   (类型-1) 为 2/3 走 IPX 广播，0/1 走连接对话框。
    int32 SessionType;
    // asm +0x284C —— 等于 1 时 Progress 直接返回（会话正忙，不推进进度）
    int32 Field_284C;
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
    // asm: gBases —— "允许建造基地"复选框（ModemHost_ReadDialog 0x66Ah 写入）
    bool Bases = false;
    // asm: gHarvesterTruce —— "采矿车停战"复选框（0x596h 写入）
    bool HarvesterTruce = false;
    // asm: gMultiEngineer —— "多个工程师"复选框（0x551h 写入）
    bool MultiEngineer = false;
    int32 CountdownTimer;
    bool CountdownStarted;
    int32 LobbyReadyCount;
    bool AutoStartEnabled;
    int32 AutoStartTimer;
    bool ReconnectEnabled;
    int32 ReconnectTimeout;
    int32 GameDuration;
    int32 AIPlayers;
    // asm: gAIDifficulty —— AI 难度（ModemHost_ReadDialog 0x553h 写入）
    int32 AIDifficulty = 0;
    int32 ScenarioIndex;
    bool TournamentMode;
    bool BattleMode;
    bool CampaignMode;
    int32 StartingCredits;
    bool ShroudRegrows;
    bool CoopGame;
    bool PendingSave;
    int32 HostID;

    // ── Persistent multiplayer preferences (RA2MD.INI) ──────────────────
    //
    //  Layout mirrors GameModeClass at +0x28A8..+0x30CC.
    int32               PortBase;
    int32               ForcePortBase;
    int32               PortPool;
    bool                SendDelay;
    int32               PortNumberOverride;
    int32               PhoneIndex;
    int32               CheckHeap;
    bool                WOLLimitResolution;
    int32               LastNickSlot;
    wchar_t             Handle[0x14];
    int32               Color;
    int32               ColorEx;
    int32               Side;
    int32               SideEx;
    int32               SideIdx;
    int32               SideExIdx;
    int32               ScenIndex;
    bool                WOLTaunts;
    bool                LANTaunts;
    bool                WOLScrollText;
    bool                LANScrollText;
    char                ModemName[0x40];
    int32               Port;
    int32               IRQ;
    int32               Baud;
    int32               Compression;
    int32               ErrorCorrection;
    int32               DialMethod;
    int32               InitStringIndex;
    int32               CallWaitStringIndex;
    char                CallWaitString[0x10];

    // ── SyncBug diagnostics (RA2MD.INI [SyncBug]) ───────────────────────
    int32               SyncBugFrame;
    int32               SyncBugType;
    int32               SyncBugTarget;
    char                SyncBugCoord[0x50];
    char                SyncBugCell[0x50];
    int32               SyncBugPrintCRC;

    GameTypePrefsClass  SkirmishPrefs;
    GameTypePrefsClass  LANPrefs;
    GameTypePrefsClass  WOLPrefs;

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