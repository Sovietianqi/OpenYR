#pragma once

#include "Core/Definitions.h"

class Game
{
public:
    // ── Window / Instance ──────────────────────────────────────────────
    static void*    hWnd;
    static void*    hInstance;
    static int      nCmdShow;

    // ── Frame Counters ──────────────────────────────────────────────────
    static int      CurrentFrame;
    static int      FrameCount;
    static int      FrameRate;

    // ── Video / Back Buffer ─────────────────────────────────────────────
    static bool     bVideoBackBuffer;
    static bool     bMoviePlaying;

    // ── Recording / Replay ──────────────────────────────────────────────
    static bool     RecordingFlag;
    static bool     PlaybackFlag;

    // ── Dialog / Modal State ────────────────────────────────────────────
    static int      SpecialDialog;
    static int      InGameUIState;

    // ── Random Seed ─────────────────────────────────────────────────────
    static unsigned int Seed;

    // ── Tech Level ──────────────────────────────────────────────────────
    static int      TechLevel;

    // ── Player / Observer ───────────────────────────────────────────────
    static int      PlayerCount;
    static int      ObserverMode;
    static int      ActivePlayerIndex;

    // ── Focus / Progress / Pause ────────────────────────────────────────
    static bool     GameInFocus;
    static bool     GameInProgress;
    static bool     GamePaused;
    static bool     ShutdownRequested;

    // ── Speed ───────────────────────────────────────────────────────────
    //
    //  GameSpeed values (original mapping):
    //    0 = Slowest   (15 FPS)
    //    1 = Slower
    //    2 = Slow
    //    3 = Normal
    //    4 = Fast
    //    5 = Faster
    //    6 = Fastest   (60 FPS)
    //
    static int      GameSpeed;

    // ── Game Modes ──────────────────────────────────────────────────────
    static bool     IsSinglePlayer;
    static bool     IsMultiplayer;
    static bool     IsCampaign;
    static bool     IsSkirmish;
    static bool     IsNetworkGame;
    static bool     IsMod;

    enum GameModeEnum
    {
        GAMEMODE_SKIRMISH   = 0,
        GAMEMODE_CAMPAIGN   = 1,
        GAMEMODE_NETWORK    = 2,
        GAMEMODE_MOD        = 3,
        GAMEMODE_COUNT      = 4
    };
    static int      GameMode;

    enum DifficultyEnum
    {
        DIFFICULTY_EASY     = 0,
        DIFFICULTY_NORMAL   = 1,
        DIFFICULTY_HARD     = 2,
        DIFFICULTY_COUNT    = 3
    };
    static int      Difficulty;

    // ── Game State Machine ──────────────────────────────────────────────
    enum GameStateEnum
    {
        GAMESTATE_TITLE     = 0,
        GAMESTATE_LOADING   = 1,
        GAMESTATE_PLAYING   = 2,
        GAMESTATE_PAUSED    = 3,
        GAMESTATE_MOVIE     = 4,
        GAMESTATE_SCORE     = 5,
        GAMESTATE_SHUTDOWN  = 6,
        GAMESTATE_COUNT     = 7
    };
    static int      GameState;
    static int      PrevGameState;
    static bool     bStateTransition;

    // ── Save/Load ───────────────────────────────────────────────────────
    static int      SaveGameVersion;
    static int      CurrentSaveNumber;
    static bool     bIsLoadingSave;
    static bool     bIsSaving;

    // ── Cheat System ────────────────────────────────────────────────────
    static bool     bCheatsEnabled;
    static char     CheatBuffer[32];
    static int      CheatBufferPos;

    // ── Current Difficulty (integer level, matches the original) ───────
    static int CurrentDifficulty;

    // ── Difficulty Multipliers ──────────────────────────────────────────
    enum DiffMultType
    {
        DIFFMULT_FIREPOWER  = 0,
        DIFFMULT_ARMOR      = 1,
        DIFFMULT_SPEED      = 2,
        DIFFMULT_ROF        = 3,
        DIFFMULT_COST       = 4,
        DIFFMULT_BUILDTIME  = 5,
        DIFFMULT_COUNT      = 6
    };
    static double   DifficultyFirepowerMult;
    static double   DifficultyArmorMult;
    static double   DifficultySpeedMult;
    static double   DifficultyROFMult;
    static double   DifficultyCostMult;
    static double   DifficultyBuildTimeMult;

    // ── Timing ──────────────────────────────────────────────────────────
    static unsigned int LastFrameTime;
    static unsigned int FrameDelay;
    static unsigned int TargetFrameTime;

    // ── Misc ────────────────────────────────────────────────────────────
    static bool     bShowDebugInfo;
    static bool     bAllowFrameStep;
    static bool     bFrameStep;
    static int      DebugFrameCount;

    // ── Methods ─────────────────────────────────────────────────────────

    static void Init();
    static void Shutdown();

    static void AdvanceFrame();
    static bool IsGameOver();

    static int  GetFrameRate();
    static void SetGameSpeed(int speed);
    static int  GetGameSpeed();
    static void Game_Sleep(unsigned int ms);
    static void Game_Update();

    // ── Helpers ─────────────────────────────────────────────────────────
    static unsigned int GetTickCount();
    static void ForceFullRedraw();

    static void SetGameMode(int mode);
    static void SetDifficulty(int diff);

    static bool IsActive();

    // ── Pause / Resume ──────────────────────────────────────────────────
    static void Pause();
    static void Resume();
    static void TogglePause();

    // ── Game State Machine ──────────────────────────────────────────────
    static void SetGameState(int newState);
    static int  GetGameState();
    static bool IsGameState(int state);
    static void OnStateEnter(int state);
    static void OnStateExit(int state);

    // ── Random Number Generator (LCG) ───────────────────────────────────
    static void         Random_Seed(unsigned int newSeed);
    static void         Random_Init();
    static unsigned int Random_Random();
    static int          Random_Int(int min, int max);
    static double       Random_Double();
    static double       Random_Double(double min, double max);

    // ── Cheat Code Detection ────────────────────────────────────────────
    static void Cheat_Input(char key);
    static bool Cheat_IsEnabled();
    static void Cheat_Disable();

    // ── Save/Load Version Management ────────────────────────────────────
    static void SetSaveGameVersion(int version);
    static int  GetSaveGameVersion();
    static bool IsSaveVersionCompatible(int version);
    static void SetCurrentSaveNumber(int number);
    static int  GetCurrentSaveNumber();
    static bool IsLoadingSave();
    static bool IsSaving();
    static void BeginSave();
    static void EndSave();
    static void BeginLoad();
    static void EndLoad();

    // ── Save/Load Entry Points ──────────────────────────────────────────
    static bool SaveGame(const char* pFilename);
    static bool LoadGame(const char* pFilename);

    // ── Debug / Dev ─────────────────────────────────────────────────────
    static void ToggleDebugInfo();
    static bool IsDebugInfoVisible();
    static void EnableFrameStep(bool enable);
    static void DoFrameStep();
    static int  GetDebugFrameCount();
    static void ResetDebugFrameCount();

    // ── Subsystem Initialization ────────────────────────────────────────
    static void InitSubsystems();

    // ── Movie playback ──────────────────────────────────────────────────
    static void PlayMovie(const char* movieName);
    static void StopMovie();
    static bool IsMoviePlaying();

    // ── Tech Level ──────────────────────────────────────────────────────
    static void SetTechLevel(int level);
    static int  GetTechLevel();
    static bool IsTechLevelAllowed(int requiredLevel);

    // ── Difficulty ──────────────────────────────────────────────────────
    static void   ApplyDifficultyMultipliers();
    static double GetDifficultyMultiplier(int type);

    // ── Main Loop ───────────────────────────────────────────────────────
    static void MainLoop();

    // ── Recording / Playback ────────────────────────────────────────────
    static void StartRecording();
    static void StopRecording();
    static bool IsRecording();
    static void StartPlayback();
    static void StopPlayback();
    static bool IsPlayback();

    // ── Observer ────────────────────────────────────────────────────────
    static void SetObserverMode(int mode);
    static int  GetObserverMode();
    static bool IsObserver();

    // ── Frame helpers ───────────────────────────────────────────────────
    static int  GetCurrentFrame();
    static int  GetFrameCount();
    static bool IsFrameMultipleOf(int divisor);
    static bool IsEvenFrame();

    // ── Scenario & multiplayer flow ────────────────────────────────────
    // 根据游戏行为，可知下面一组是网络对局与场景流程入口：网络包解析、
    // 席位管理、玩家进出、结盟处理、资源装载与卸载。
    static int32     ParsePKTs(void* pPktBuffer);
    static void      CreateDummySlots();
    static bool      IsRandomMap();
    static void      PlayerLeftMP(int32 houseIndex);
    static void      SaveTaunts();
    static void      ProcessRandomPlayers();
    static void      SetWantedSide(int32 sideIndex);
    static int32     GetWantedSide();
    static void      LoadPCXFiles();
    static void      EnableJabber(bool bEnable);
    static bool      IsJabberEnabled();
    static void      ReloadNeutralMIX();
    static bool      MakeScreenshot();
    static void      SaveNetworkConfig();
    static void      UnloadGUIPalettes();
    static void      UnloadStartingGenericSpots();
    static void      UnloadSideAndGUISHPs2();
    static void      ReleaseNeutralMIX();
    static void      MakeAlliances();
    static int32     GetGameTypePrefs();
    static void      SelectStartingPoint(int32 houseIndex, int32 spotIndex);
    static void      LoadUI_LoadFiles();
    static void      UnloadDiploOptPowerSHP();
    static void      UnloadDiploOptSHP();
    static void      UnloadGUISHPs();
    static void      PlayerLeft(int32 houseIndex);
    static bool      Load_ARTMD_INI();
    static void      LoadUI();
    static void      UnloadSideAndGUISHPs();
    static void      EmptyMaps_InitStrings_PhoneBook();
    static void      AllyHouses(int32 houseA, int32 houseB);
    static void      UnloadStartingSpotsAndWinsock();
    static void      ProcessCampaignOptions();

    // ── Speed helpers ───────────────────────────────────────────────────
    static unsigned int GetFrameDelay();
    static unsigned int GetTargetFrameTime();
    static double       GetGameSpeedFactor();
    static int          GetFPS();

private:
    // ── Internal ────────────────────────────────────────────────────────
    static void UpdateTiming();
    static void CalibrateFrameDelay();

    // Prohibit instantiation
    Game()  = delete;
    ~Game() = delete;
};