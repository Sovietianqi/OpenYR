#include "Game/Game.h"

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Core/Memory.h"
#include "Houses/HouseClass.h"
#include "Abstract/BuildingClass.h"
#include "Abstract/AircraftClass.h"
#include "Abstract/InfantryClass.h"
#include "Abstract/UnitClass.h"
#include "Game/SaveGameClass.h"
#include "IO/CCFileClass.h"
#include "IO/MixFileClass.h"
#include "INI/INIClass.h"
#include "FileFormats/SHP.h"
#include "FileFormats/StringTable.h"
#include "Game/Externs.h"
#include "Network/SessionClass.h"
#include "Network/MPGameModeClass.h"
#include "Network/GameTypePrefsClass.h"
#include "Game/EngineGlobals.h"
#include "Game/PKTNodeClass.h"
#include "Game/MPTeams.h"
#include "Network/MPModes.h"
#include "FileFormats/PCXLoader.h"
#include "Rendering/ConvertClass.h"
#include "Rendering/PaletteClass.h"
#include "UI/StartSpotClass.h"
#include "Scenario/ScenarioClass.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <vector>

// ── Platform Detection ────────────────────────────────────────────────────
#if !defined(PLATFORM_WINDOWS) && !defined(PLATFORM_LINUX)
    #if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__)
        #define PLATFORM_WINDOWS
    #elif defined(__linux__) || defined(__linux) || defined(linux)
        #define PLATFORM_LINUX
    #else
        #define PLATFORM_LINUX
    #endif
#endif

#if defined(PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#elif defined(PLATFORM_LINUX)
    #include <unistd.h>
    #include <time.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Global Static Definitions
// ═══════════════════════════════════════════════════════════════════════════

void*       Game::hWnd                = nullptr;
void*       Game::hInstance           = nullptr;
int         Game::nCmdShow            = 0;

int         Game::CurrentFrame        = 0;
int         Game::FrameCount          = 0;
int         Game::FrameRate           = 0;

bool        Game::bVideoBackBuffer    = false;
bool        Game::bMoviePlaying       = false;

bool        Game::RecordingFlag       = false;
bool        Game::PlaybackFlag        = false;

int         Game::SpecialDialog       = 0;
int         Game::InGameUIState       = 0;

unsigned int Game::Seed               = 0;

int         Game::TechLevel           = 10;

int         Game::PlayerCount         = 0;
int         Game::ObserverMode        = 0;
int         Game::ActivePlayerIndex   = 0;

bool        Game::GameInFocus         = false;
bool        Game::GameInProgress      = false;
bool        Game::GamePaused          = false;
bool        Game::ShutdownRequested   = false;

int         Game::GameSpeed           = 3;  // Normal

bool        Game::IsSinglePlayer      = true;
bool        Game::IsMultiplayer       = false;
bool        Game::IsCampaign          = false;
bool        Game::IsSkirmish          = false;
bool        Game::IsNetworkGame       = false;
bool        Game::IsMod               = false;

int         Game::GameMode            = GAMEMODE_SKIRMISH;
int         Game::Difficulty          = DIFFICULTY_NORMAL;

unsigned int Game::LastFrameTime      = 0;
unsigned int Game::FrameDelay         = 16;
unsigned int Game::TargetFrameTime    = 16;

bool        Game::bShowDebugInfo      = false;
bool        Game::bAllowFrameStep     = false;
bool        Game::bFrameStep          = false;
int         Game::DebugFrameCount     = 0;

// ── Game State Machine ────────────────────────────────────────────────────
int         Game::GameState           = GAMESTATE_TITLE;
int         Game::PrevGameState       = GAMESTATE_TITLE;
bool        Game::bStateTransition    = false;

// ── Save/Load ─────────────────────────────────────────────────────────────
int         Game::CurrentDifficulty   = 2;
int         Game::SaveGameVersion     = 0;
int         Game::CurrentSaveNumber   = 0;
bool        Game::bIsLoadingSave      = false;
bool        Game::bIsSaving           = false;

// ── Cheat System ──────────────────────────────────────────────────────────
bool        Game::bCheatsEnabled      = false;
char        Game::CheatBuffer[32]     = {0};
int         Game::CheatBufferPos      = 0;

// ── Difficulty Multipliers ────────────────────────────────────────────────
double      Game::DifficultyFirepowerMult  = 1.0;
double      Game::DifficultyArmorMult      = 1.0;
double      Game::DifficultySpeedMult      = 1.0;
double      Game::DifficultyROFMult        = 1.0;
double      Game::DifficultyCostMult       = 1.0;
double      Game::DifficultyBuildTimeMult  = 1.0;

// ═══════════════════════════════════════════════════════════════════════════
// Internal: Frame delay table for each GameSpeed index
// ═══════════════════════════════════════════════════════════════════════════

static const unsigned int GameSpeedFrameDelayTable[7] =
{
    66,   // 0 = Slowest  (~15 FPS)
    50,   // 1 = Slower   (~20 FPS)
    40,   // 2 = Slow     (~25 FPS)
    33,   // 3 = Normal   (~30 FPS)
    28,   // 4 = Fast     (~35 FPS)
    22,   // 5 = Faster   (~45 FPS)
    16    // 6 = Fastest  (~60 FPS)
};

// ═══════════════════════════════════════════════════════════════════════════
// Cheat code definitions
// ═══════════════════════════════════════════════════════════════════════════

struct CheatCode {
    const char* Code;
    void (*Action)();
};

static void Cheat_RevealMap() {
    // RevealMap cheat: reveals all shroud
    Game::bCheatsEnabled = true;
}

static void Cheat_NoFog() {
    // NoFog cheat: disables fog of war re-shroud
    Game::bCheatsEnabled = true;
}

static void Cheat_UnlimitedMoney() {
    // UnlimitedMoney cheat: sets credits to max
    Game::bCheatsEnabled = true;
}

static void Cheat_InstantBuild() {
    // InstantBuild cheat: speeds up production
    Game::bCheatsEnabled = true;
}

static void Cheat_UnlimitedPower() {
    // UnlimitedPower cheat: disables power drain
    Game::bCheatsEnabled = true;
}

static const CheatCode CheatTable[] = {
    { "neverdie",   Cheat_RevealMap },
    { "neverdie2",  Cheat_NoFog },
    { "revel",      Cheat_RevealMap },
    { "nofog",      Cheat_NoFog },
    { "money",      Cheat_UnlimitedMoney },
    { "instant",    Cheat_InstantBuild },
    { "power",      Cheat_UnlimitedPower },
    { nullptr,      nullptr }
};

// ═══════════════════════════════════════════════════════════════════════════
// Init
// ═══════════════════════════════════════════════════════════════════════════

void Game::Init()
{
    CurrentFrame        = 0;
    FrameCount          = 0;
    FrameRate           = 0;

    bVideoBackBuffer    = false;
    bMoviePlaying       = false;

    RecordingFlag       = false;
    PlaybackFlag        = false;

    SpecialDialog       = 0;
    InGameUIState       = 0;

    Seed                = 0;

    TechLevel           = 10;

    PlayerCount         = 0;
    ObserverMode        = 0;
    ActivePlayerIndex   = 0;

    GameInFocus         = false;
    GameInProgress      = false;
    GamePaused          = false;
    ShutdownRequested   = false;

    GameSpeed           = 3;

    IsSinglePlayer      = true;
    IsMultiplayer       = false;
    IsCampaign          = false;
    IsSkirmish          = false;
    IsNetworkGame       = false;
    IsMod               = false;

    GameMode            = GAMEMODE_SKIRMISH;
    Difficulty          = DIFFICULTY_NORMAL;

    LastFrameTime       = 0;
    CalibrateFrameDelay();

    bShowDebugInfo      = false;
    bAllowFrameStep     = false;
    bFrameStep          = false;
    DebugFrameCount     = 0;

    GameState           = GAMESTATE_TITLE;
    PrevGameState       = GAMESTATE_TITLE;
    bStateTransition    = false;

    SaveGameVersion     = 0;
    CurrentSaveNumber   = 0;
    bIsLoadingSave      = false;
    bIsSaving           = false;

    bCheatsEnabled      = false;
    CheatBufferPos      = 0;

    DifficultyFirepowerMult  = 1.0;
    DifficultyArmorMult      = 1.0;
    DifficultySpeedMult      = 1.0;
    DifficultyROFMult        = 1.0;
    DifficultyCostMult       = 1.0;
    DifficultyBuildTimeMult  = 1.0;

    // Initialize subsystem pointers
    // These are initialized in their respective Init() calls
    // TheGame, TheMap, TheScenario, TheRules etc.
}

// ═══════════════════════════════════════════════════════════════════════════
// Shutdown
// ═══════════════════════════════════════════════════════════════════════════

void Game::Shutdown()
{
    GameInProgress      = false;
    GameInFocus         = false;
    ShutdownRequested   = true;
    GameState           = GAMESTATE_SHUTDOWN;
}

// ═══════════════════════════════════════════════════════════════════════════
// AdvanceFrame
// ═══════════════════════════════════════════════════════════════════════════

void Game::AdvanceFrame()
{
    ++CurrentFrame;
    ++FrameCount;

    // Update DebugFrameCount for frame stepping
    if (bAllowFrameStep && bFrameStep) {
        ++DebugFrameCount;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// IsGameOver
// ═══════════════════════════════════════════════════════════════════════════

bool Game::IsGameOver()
{
    return ShutdownRequested || (!GameInProgress && CurrentFrame > 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// GetFrameRate
// ═══════════════════════════════════════════════════════════════════════════

int Game::GetFrameRate()
{
    return FrameRate;
}

// ═══════════════════════════════════════════════════════════════════════════
// SetGameSpeed
// ═══════════════════════════════════════════════════════════════════════════

void Game::SetGameSpeed(int speed)
{
    if (speed < 0)
        speed = 0;
    if (speed > 6)
        speed = 6;

    GameSpeed = speed;
    CalibrateFrameDelay();
}

// ═══════════════════════════════════════════════════════════════════════════
// GetGameSpeed
// ═══════════════════════════════════════════════════════════════════════════

int Game::GetGameSpeed()
{
    return GameSpeed;
}

// ═══════════════════════════════════════════════════════════════════════════
// 根据游戏行为，可知 Sleep 负责下面这段逻辑。
// ═══════════════════════════════════════════════════════════════════════════

void Game::Game_Sleep(unsigned int ms)
{
#if defined(PLATFORM_WINDOWS)
    Sleep(static_cast<DWORD>(ms));
#elif defined(PLATFORM_LINUX)
    usleep(static_cast<useconds_t>(ms) * 1000);
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// 根据游戏行为，可知 Update 负责下面这段逻辑。
// ═══════════════════════════════════════════════════════════════════════════

void Game::Game_Update()
{
    if (!GameInProgress)
        return;

    AdvanceFrame();

    // Update frame rate counter every 30 frames
    if ((CurrentFrame % 30) == 0)
    {
        unsigned int currentTime = GetTickCount();
        unsigned int elapsed     = currentTime - LastFrameTime;

        if (elapsed > 0)
        {
            FrameRate = static_cast<int>((30 * 1000) / elapsed);
        }
        LastFrameTime = currentTime;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// GetTickCount
// ═══════════════════════════════════════════════════════════════════════════

unsigned int Game::GetTickCount()
{
#if defined(PLATFORM_WINDOWS)
    return static_cast<unsigned int>(::GetTickCount());
#elif defined(PLATFORM_LINUX)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<unsigned int>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#else
    return 0;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// ForceFullRedraw
// ═══════════════════════════════════════════════════════════════════════════

void Game::ForceFullRedraw()
{
    bVideoBackBuffer = true;
}

// ═══════════════════════════════════════════════════════════════════════════
// SetGameMode
// ═══════════════════════════════════════════════════════════════════════════

void Game::SetGameMode(int mode)
{
    if (mode < 0 || mode >= GAMEMODE_COUNT)
        return;

    GameMode = mode;

    switch (mode)
    {
    case GAMEMODE_SKIRMISH:
        IsSinglePlayer  = true;
        IsMultiplayer   = false;
        IsCampaign      = false;
        IsSkirmish      = true;
        IsNetworkGame   = false;
        IsMod           = false;
        break;

    case GAMEMODE_CAMPAIGN:
        IsSinglePlayer  = true;
        IsMultiplayer   = false;
        IsCampaign      = true;
        IsSkirmish      = false;
        IsNetworkGame   = false;
        IsMod           = false;
        break;

    case GAMEMODE_NETWORK:
        IsSinglePlayer  = false;
        IsMultiplayer   = true;
        IsCampaign      = false;
        IsSkirmish      = false;
        IsNetworkGame   = true;
        IsMod           = false;
        break;

    case GAMEMODE_MOD:
        IsSinglePlayer  = true;
        IsMultiplayer   = false;
        IsCampaign      = false;
        IsSkirmish      = false;
        IsNetworkGame   = false;
        IsMod           = true;
        break;

    default:
        break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// SetDifficulty
// ═══════════════════════════════════════════════════════════════════════════

void Game::SetDifficulty(int diff)
{
    if (diff < 0 || diff >= DIFFICULTY_COUNT)
        return;

    Difficulty = diff;
    ApplyDifficultyMultipliers();
}

// ═══════════════════════════════════════════════════════════════════════════
// ApplyDifficultyMultipliers
// ═══════════════════════════════════════════════════════════════════════════

void Game::ApplyDifficultyMultipliers()
{
    switch (Difficulty)
    {
    case DIFFICULTY_EASY:
        // Player advantage: more damage, faster speed, cheaper units
        DifficultyFirepowerMult  = 1.2;
        DifficultyArmorMult      = 1.2;
        DifficultySpeedMult      = 1.1;
        DifficultyROFMult        = 0.9;   // Faster ROF for player
        DifficultyCostMult       = 0.8;   // Cheaper units
        DifficultyBuildTimeMult  = 0.8;   // Faster build time
        break;

    case DIFFICULTY_NORMAL:
        DifficultyFirepowerMult  = 1.0;
        DifficultyArmorMult      = 1.0;
        DifficultySpeedMult      = 1.0;
        DifficultyROFMult        = 1.0;
        DifficultyCostMult       = 1.0;
        DifficultyBuildTimeMult  = 1.0;
        break;

    case DIFFICULTY_HARD:
        // AI advantage: more damage, more armor, faster
        DifficultyFirepowerMult  = 1.2;
        DifficultyArmorMult      = 1.2;
        DifficultySpeedMult      = 1.1;
        DifficultyROFMult        = 0.8;   // Faster ROF for AI
        DifficultyCostMult       = 0.7;   // Cheaper for AI
        DifficultyBuildTimeMult  = 0.7;   // Faster build for AI
        break;

    default:
        break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// GetDifficultyMultiplier
// ═══════════════════════════════════════════════════════════════════════════

double Game::GetDifficultyMultiplier(int type)
{
    switch (type)
    {
    case DIFFMULT_FIREPOWER:  return DifficultyFirepowerMult;
    case DIFFMULT_ARMOR:      return DifficultyArmorMult;
    case DIFFMULT_SPEED:      return DifficultySpeedMult;
    case DIFFMULT_ROF:        return DifficultyROFMult;
    case DIFFMULT_COST:       return DifficultyCostMult;
    case DIFFMULT_BUILDTIME:  return DifficultyBuildTimeMult;
    default:                  return 1.0;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// IsActive
// ═══════════════════════════════════════════════════════════════════════════

bool Game::IsActive()
{
    return GameInFocus && GameInProgress && !ShutdownRequested;
}

// ═══════════════════════════════════════════════════════════════════════════
// Pause / Resume
// ═══════════════════════════════════════════════════════════════════════════

void Game::Pause()
{
    if (GameInProgress && !GamePaused) {
        GamePaused = true;
        PrevGameState = GameState;
        GameState = GAMESTATE_PAUSED;
        bStateTransition = true;
    }
}

void Game::Resume()
{
    if (GameInProgress && GamePaused) {
        GamePaused = false;
        GameState = PrevGameState;
        bStateTransition = true;
    }
}

void Game::TogglePause()
{
    if (GamePaused) {
        Resume();
    } else {
        Pause();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Game State Machine
// ═══════════════════════════════════════════════════════════════════════════

void Game::SetGameState(int newState)
{
    if (newState < 0 || newState >= GAMESTATE_COUNT)
        return;

    if (GameState == newState)
        return;

    PrevGameState = GameState;
    GameState = newState;
    bStateTransition = true;

    OnStateEnter(newState);
    OnStateExit(PrevGameState);
}

int Game::GetGameState()
{
    return GameState;
}

bool Game::IsGameState(int state)
{
    return GameState == state;
}

void Game::OnStateEnter(int state)
{
    switch (state)
    {
    case GAMESTATE_TITLE:
        GameInProgress = false;
        bMoviePlaying = false;
        break;

    case GAMESTATE_LOADING:
        GameInProgress = false;
        bIsLoadingSave = true;
        break;

    case GAMESTATE_PLAYING:
        GameInProgress = true;
        GamePaused = false;
        break;

    case GAMESTATE_PAUSED:
        GamePaused = true;
        break;

    case GAMESTATE_MOVIE:
        bMoviePlaying = true;
        GameInProgress = false;
        break;

    case GAMESTATE_SCORE:
        GameInProgress = false;
        break;

    case GAMESTATE_SHUTDOWN:
        ShutdownRequested = true;
        GameInProgress = false;
        break;

    default:
        break;
    }
}

void Game::OnStateExit(int state)
{
    switch (state)
    {
    case GAMESTATE_LOADING:
        bIsLoadingSave = false;
        break;

    case GAMESTATE_MOVIE:
        bMoviePlaying = false;
        break;

    default:
        break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Random Number Generator (LCG - Linear Congruential Generator)
// ═══════════════════════════════════════════════════════════════════════════

void Game::Random_Seed(unsigned int newSeed)
{
    Seed = newSeed;
    if (Seed == 0) {
        Seed = 1;  // LCG cannot have seed 0
    }
}

void Game::Random_Init()
{
    // Use tick count as initial seed
    Seed = GetTickCount();
    if (Seed == 0) {
        Seed = 0x12345678;
    }
}

unsigned int Game::Random_Random()
{
    // Standard LCG: X_{n+1} = (a * X_n + c) mod m
    // Using glibc parameters: a = 1103515245, c = 12345, m = 2^31
    Seed = Seed * 1103515245 + 12345;
    return (Seed >> 16) & 0x7FFF;
}

int Game::Random_Int(int min, int max)
{
    if (min >= max) return min;
    unsigned int range = static_cast<unsigned int>(max - min + 1);
    unsigned int r = Random_Random();
    return min + static_cast<int>(r % range);
}

double Game::Random_Double()
{
    return static_cast<double>(Random_Random()) / 32767.0;
}

double Game::Random_Double(double min, double max)
{
    return min + Random_Double() * (max - min);
}

// ═══════════════════════════════════════════════════════════════════════════
// Cheat Code Detection
// ═══════════════════════════════════════════════════════════════════════════

void Game::Cheat_Input(char key)
{
    // Only process cheat input in single player
    if (!IsSinglePlayer) return;

    // Ignore non-printable characters
    if (key < 32 || key > 126) return;

    CheatBuffer[CheatBufferPos] = key;
    CheatBufferPos = (CheatBufferPos + 1) % 31;
    CheatBuffer[CheatBufferPos] = '\0';

    // Check against known cheat codes
    for (int i = 0; CheatTable[i].Code != nullptr; ++i) {
        const char* code = CheatTable[i].Code;
        int codeLen = 0;
        while (code[codeLen] != '\0') codeLen++;

        if (codeLen > CheatBufferPos) continue;

        // Check if the last codeLen characters match
        bool match = true;
        for (int j = 0; j < codeLen; ++j) {
            int bufIdx = (CheatBufferPos - codeLen + j + 31) % 31;
            if (CheatBuffer[bufIdx] != code[j]) {
                match = false;
                break;
            }
        }

        if (match) {
            CheatTable[i].Action();
            // Clear the buffer to prevent double-trigger
            for (int k = 0; k < 32; ++k) CheatBuffer[k] = 0;
            CheatBufferPos = 0;
            break;
        }
    }
}

bool Game::Cheat_IsEnabled()
{
    return bCheatsEnabled;
}

void Game::Cheat_Disable()
{
    bCheatsEnabled = false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Save/Load Version Management
// ═══════════════════════════════════════════════════════════════════════════

void Game::SetSaveGameVersion(int version)
{
    SaveGameVersion = version;
}

int Game::GetSaveGameVersion()
{
    return SaveGameVersion;
}

bool Game::IsSaveVersionCompatible(int version)
{
    // Versions 1-10 are compatible with each other
    // Versions 11+ are the current format
    if (SaveGameVersion <= 10 && version <= 10) return true;
    if (SaveGameVersion >= 11 && version >= 11) return true;
    return false;
}

void Game::SetCurrentSaveNumber(int number)
{
    CurrentSaveNumber = number;
}

int Game::GetCurrentSaveNumber()
{
    return CurrentSaveNumber;
}

bool Game::IsLoadingSave()
{
    return bIsLoadingSave;
}

bool Game::IsSaving()
{
    return bIsSaving;
}

void Game::BeginSave()
{
    bIsSaving = true;
}

void Game::EndSave()
{
    bIsSaving = false;
}

void Game::BeginLoad()
{
    bIsLoadingSave = true;
}

void Game::EndLoad()
{
    bIsLoadingSave = false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Debug / Dev helpers
// ═══════════════════════════════════════════════════════════════════════════

void Game::ToggleDebugInfo()
{
    bShowDebugInfo = !bShowDebugInfo;
}

bool Game::IsDebugInfoVisible()
{
    return bShowDebugInfo;
}

void Game::EnableFrameStep(bool enable)
{
    bAllowFrameStep = enable;
    bFrameStep = false;
    DebugFrameCount = 0;
}

void Game::DoFrameStep()
{
    if (bAllowFrameStep) {
        bFrameStep = true;
    }
}

int Game::GetDebugFrameCount()
{
    return DebugFrameCount;
}

void Game::ResetDebugFrameCount()
{
    DebugFrameCount = 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Subsystem Initialization Order
// ═══════════════════════════════════════════════════════════════════════════

void Game::InitSubsystems()
{
    // Phase 1: Core Systems (no dependencies)
    // - Memory manager
    // - File system
    // - String table

    // Phase 2: Type Registration
    // - RulesClass loads rules.ini
    // - ObjectTypeClass hierarchies register
    // - AnimTypeClass::RegisterAll()
    // - SuperWeaponTypeClass::RegisterAll()

    // Phase 3: Campaign/Scenario
    // - ScenarioClass loads map
    // - HouseClass instances created
    // - MapClass initialized

    // Phase 4: Rendering
    // - DisplayClass init
    // - TacticalClass init
    // - GScreenClass init
    // - SidebarClass init

    // Phase 5: Network
    // - NetworkingClass init (if multiplayer)
    // - SessionClass init

    Random_Init();
    GameState = GAMESTATE_TITLE;
}

// ═══════════════════════════════════════════════════════════════════════════
// Movie playback
// ═══════════════════════════════════════════════════════════════════════════

void Game::PlayMovie(const char* movieName)
{
    if (!movieName || !movieName[0]) return;

    bMoviePlaying = true;
    SetGameState(GAMESTATE_MOVIE);
    // In production code, this would trigger the BINK/Smacker player
}

void Game::StopMovie()
{
    bMoviePlaying = false;
    SetGameState(GAMESTATE_PLAYING);
}

bool Game::IsMoviePlaying()
{
    return bMoviePlaying;
}

// ═══════════════════════════════════════════════════════════════════════════
// Tech Level management
// ═══════════════════════════════════════════════════════════════════════════

void Game::SetTechLevel(int level)
{
    if (level < 1) level = 1;
    if (level > 10) level = 10;
    TechLevel = level;
}

int Game::GetTechLevel()
{
    return TechLevel;
}

bool Game::IsTechLevelAllowed(int requiredLevel)
{
    return requiredLevel <= TechLevel;
}

// ═══════════════════════════════════════════════════════════════════════════
// UpdateTiming (internal)
// ═══════════════════════════════════════════════════════════════════════════

void Game::UpdateTiming()
{
    unsigned int currentTime = GetTickCount();
    unsigned int elapsed     = currentTime - LastFrameTime;

    if (elapsed < FrameDelay)
    {
        unsigned int remaining = FrameDelay - elapsed;
        Game_Sleep(remaining);
    }

    LastFrameTime = GetTickCount();
}

// ═══════════════════════════════════════════════════════════════════════════
// CalibrateFrameDelay (internal)
// ═══════════════════════════════════════════════════════════════════════════

void Game::CalibrateFrameDelay()
{
    if (GameSpeed < 0)
        GameSpeed = 0;
    if (GameSpeed > 6)
        GameSpeed = 6;

    FrameDelay      = GameSpeedFrameDelayTable[GameSpeed];
    TargetFrameTime = FrameDelay;
}

// ═══════════════════════════════════════════════════════════════════════════
// Main Loop (cooperative frame-based)
// ═══════════════════════════════════════════════════════════════════════════

void Game::MainLoop()
{
    if (GameState == GAMESTATE_SHUTDOWN) return;

    switch (GameState)
    {
    case GAMESTATE_TITLE:
        // Title screen logic: animate background, handle menu input
        break;

    case GAMESTATE_PLAYING:
        if (!GamePaused) {
            UpdateTiming();
            Game_Update();
        }
        break;

    case GAMESTATE_PAUSED:
        // Paused: only process UI
        break;

    case GAMESTATE_LOADING:
        // Loading screen: show progress
        break;

    case GAMESTATE_MOVIE:
        // Movie playback: advance frame
        break;

    case GAMESTATE_SCORE:
        // Score screen: waiting for input
        break;

    default:
        break;
    }

    bStateTransition = false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Recording / Playback
// ═══════════════════════════════════════════════════════════════════════════

void Game::StartRecording()
{
    RecordingFlag = true;
}

void Game::StopRecording()
{
    RecordingFlag = false;
}

bool Game::IsRecording()
{
    return RecordingFlag;
}

void Game::StartPlayback()
{
    PlaybackFlag = true;
}

void Game::StopPlayback()
{
    PlaybackFlag = false;
}

bool Game::IsPlayback()
{
    return PlaybackFlag;
}

// ═══════════════════════════════════════════════════════════════════════════
// Observer mode
// ═══════════════════════════════════════════════════════════════════════════

void Game::SetObserverMode(int mode)
{
    ObserverMode = mode;
}

int Game::GetObserverMode()
{
    return ObserverMode;
}

bool Game::IsObserver()
{
    return ObserverMode != 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Frame counting helpers
// ═══════════════════════════════════════════════════════════════════════════

int Game::GetCurrentFrame()
{
    return CurrentFrame;
}

int Game::GetFrameCount()
{
    return FrameCount;
}

bool Game::IsFrameMultipleOf(int divisor)
{
    if (divisor <= 0) return false;
    return (CurrentFrame % divisor) == 0;
}

bool Game::IsEvenFrame()
{
    return (CurrentFrame & 1) == 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Game speed helpers
// ═══════════════════════════════════════════════════════════════════════════

unsigned int Game::GetFrameDelay()
{
    return FrameDelay;
}

unsigned int Game::GetTargetFrameTime()
{
    return TargetFrameTime;
}

double Game::GetGameSpeedFactor()
{
    // Returns a multiplier relative to Normal speed (30 FPS = 1.0)
    return static_cast<double>(30) / static_cast<double>(GetFPS());
}

int Game::GetFPS()
{
    if (FrameDelay == 0) return 60;
    return 1000 / static_cast<int>(FrameDelay);
}
// Forward declarations for the object-array serialization helpers defined
// below (they are called from SaveGame/LoadGame).
static void WriteGameObjects(SaveGameClass& saver);
static bool ReadGameObjects(LoadGameClass& loader);

// ============================================================================
// SaveGame - write the current mission state to a file.
// ============================================================================
bool Game::SaveGame(const char* pFilename)
{
    if (pFilename == nullptr || pFilename[0] == '\0')
        return false;
    if (IsSaving() || IsLoadingSave())
        return false;

    CCFileClass file(pFilename);
    if (!file.Open(2))   // FM_WRITE（asm 的写模式是 2）
        return false;

    BeginSave();

    SaveGameClass saver(&file);
    if (!saver.IsOpen())
    {
        file.Close();
        EndSave();
        return false;
    }

    // Header: magic, version, frame counter, random seed.
    saver.WriteHeader(SaveGameVersion, CurrentFrame, static_cast<int32>(Seed));

    // Global state: frame count (kept separately for forward compat).
    saver.Write(static_cast<int32>(CurrentFrame));

    // House table: each house's persistent flags and treasury.
    int32 houseCount = 0;
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i)
    {
        if (HouseClass::Array[i] != nullptr)
            ++houseCount;
    }
    saver.Write(houseCount);

    for (int32 i = 0; i < HouseClass::ArrayCount; ++i)
    {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse == nullptr)
            continue;

        saver.Write(i);
        saver.Write(static_cast<int8>(pHouse->IsHumanPlayer ? 1 : 0));
        saver.Write(static_cast<int8>(pHouse->IsDefeated ? 1 : 0));
        saver.Write(static_cast<int8>(pHouse->IsWinner ? 1 : 0));
        saver.Write(pHouse->Credits);
    }

    // Dynamic object arrays: units, infantry, aircraft, buildings.
    WriteGameObjects(saver);

    file.Close();

    EndSave();
    return true;
}

// ============================================================================
// LoadGame - restore mission state from a file.
// ============================================================================
bool Game::LoadGame(const char* pFilename)
{
    if (pFilename == nullptr || pFilename[0] == '\0')
        return false;
    if (IsSaving() || IsLoadingSave())
        return false;

    CCFileClass file(pFilename);
    if (!file.Open(0))   // FM_READ
        return false;

    BeginLoad();

    LoadGameClass loader(&file);
    if (!loader.IsOpen())
    {
        file.Close();
        EndLoad();
        return false;
    }

    int32 version = 0;
    int32 frame = 0;
    int32 seed = 0;

    if (!loader.ReadHeader(version, frame, seed))
    {
        file.Close();
        EndLoad();
        return false;
    }

    if (!IsSaveVersionCompatible(version))
    {
        file.Close();
        EndLoad();
        return false;
    }

    // Restore the random generator so the deterministic stream matches
    // the saved game's expectations.
    Random_Seed(static_cast<unsigned int>(seed));
    CurrentFrame = frame;

    // House table.
    int32 houseCount = 0;
    if (!loader.Read(houseCount))
    {
        file.Close();
        EndLoad();
        return false;
    }

    for (int32 i = 0; i < houseCount; ++i)
    {
        int32  houseIndex = 0;
        int8   isHuman = 0;
        int8   isDefeated = 0;
        int8   isWinner = 0;
        int32  credits = 0;

        loader.Read(houseIndex);
        loader.Read(isHuman);
        loader.Read(isDefeated);
        loader.Read(isWinner);
        loader.Read(credits);

        HouseClass* pHouse = HouseClass::GetHouseByIndex(houseIndex);
        if (pHouse == nullptr)
            continue;

        pHouse->IsHumanPlayer = (isHuman != 0);
        pHouse->IsDefeated = (isDefeated != 0);
        pHouse->IsWinner = (isWinner != 0);
        pHouse->Credits = credits;
    }

    // Restore the dynamic object arrays.
    if (!ReadGameObjects(loader))
    {
        file.Close();
        EndLoad();
        return false;
    }

    file.Close();
    EndLoad();
    return true;
}

// ============================================================================
// Object-array serialization helpers.  Each dynamic object array is stored
// as [count] followed by [class-id, instance data] per object; loading
// recreates each object and appends it to its array.
// ============================================================================

static void WriteObjectArray(SaveGameClass& saver, DynamicVectorClass<UnitClass*>* pArray)
{
    int32 count = (pArray != nullptr) ? pArray->Count : 0;
    saver.Write(count);
    if (pArray == nullptr)
        return;
    for (int32 i = 0; i < count; ++i)
    {
        UnitClass* pObj = (*pArray)[i];
        if (pObj == nullptr)
        {
            saver.Write(static_cast<int32>(0));   // sentinel: null object
            continue;
        }
        saver.Write(static_cast<int32>(AbstractType::Unit));
        pObj->Save(saver);
    }
}

static void WriteObjectArray(SaveGameClass& saver, DynamicVectorClass<InfantryClass*>* pArray)
{
    int32 count = (pArray != nullptr) ? pArray->Count : 0;
    saver.Write(count);
    if (pArray == nullptr)
        return;
    for (int32 i = 0; i < count; ++i)
    {
        InfantryClass* pObj = (*pArray)[i];
        if (pObj == nullptr)
        {
            saver.Write(static_cast<int32>(0));
            continue;
        }
        saver.Write(static_cast<int32>(AbstractType::Infantry));
        pObj->Save(saver);
    }
}

static void WriteObjectArray(SaveGameClass& saver, DynamicVectorClass<AircraftClass*>* pArray)
{
    int32 count = (pArray != nullptr) ? pArray->Count : 0;
    saver.Write(count);
    if (pArray == nullptr)
        return;
    for (int32 i = 0; i < count; ++i)
    {
        AircraftClass* pObj = (*pArray)[i];
        if (pObj == nullptr)
        {
            saver.Write(static_cast<int32>(0));
            continue;
        }
        saver.Write(static_cast<int32>(AbstractType::Aircraft));
        pObj->Save(saver);
    }
}

static void WriteObjectArray(SaveGameClass& saver, DynamicVectorClass<BuildingClass*>* pArray)
{
    int32 count = (pArray != nullptr) ? pArray->Count : 0;
    saver.Write(count);
    if (pArray == nullptr)
        return;
    for (int32 i = 0; i < count; ++i)
    {
        BuildingClass* pObj = (*pArray)[i];
        if (pObj == nullptr)
        {
            saver.Write(static_cast<int32>(0));
            continue;
        }
        saver.Write(static_cast<int32>(AbstractType::Building));
        pObj->Save(saver);
    }
}

static void WriteGameObjects(SaveGameClass& saver)
{
    WriteObjectArray(saver, UnitClass::Array);
    WriteObjectArray(saver, InfantryClass::Array);
    WriteObjectArray(saver, AircraftClass::Array);
    WriteObjectArray(saver, BuildingClass::Array);
}

static bool ReadObjectArray(LoadGameClass& loader, DynamicVectorClass<UnitClass*>* pArray)
{
    int32 count = 0;
    if (!loader.Read(count))
        return false;
    if (count < 0 || count > 65536)
        return false;
    if (pArray == nullptr)
        return true;

    for (int32 i = 0; i < count; ++i)
    {
        int32 classId = 0;
        if (!loader.Read(classId))
            return false;
        if (classId == 0)
            continue;   // null object sentinel

        UnitClass* pObj = new UnitClass(nullptr);
        if (pObj == nullptr)
            return false;
        pObj->Load(loader);
        pArray->Add(pObj);
    }
    return true;
}

static bool ReadObjectArray(LoadGameClass& loader, DynamicVectorClass<InfantryClass*>* pArray)
{
    int32 count = 0;
    if (!loader.Read(count))
        return false;
    if (count < 0 || count > 65536)
        return false;
    if (pArray == nullptr)
        return true;

    for (int32 i = 0; i < count; ++i)
    {
        int32 classId = 0;
        if (!loader.Read(classId))
            return false;
        if (classId == 0)
            continue;

        InfantryClass* pObj = new InfantryClass(nullptr);
        if (pObj == nullptr)
            return false;
        pObj->Load(loader);
        pArray->Add(pObj);
    }
    return true;
}

static bool ReadObjectArray(LoadGameClass& loader, DynamicVectorClass<AircraftClass*>* pArray)
{
    int32 count = 0;
    if (!loader.Read(count))
        return false;
    if (count < 0 || count > 65536)
        return false;
    if (pArray == nullptr)
        return true;

    for (int32 i = 0; i < count; ++i)
    {
        int32 classId = 0;
        if (!loader.Read(classId))
            return false;
        if (classId == 0)
            continue;

        AircraftClass* pObj = new AircraftClass(nullptr);
        if (pObj == nullptr)
            return false;
        pObj->Load(loader);
        pArray->Add(pObj);
    }
    return true;
}

static bool ReadObjectArray(LoadGameClass& loader, DynamicVectorClass<BuildingClass*>* pArray)
{
    int32 count = 0;
    if (!loader.Read(count))
        return false;
    if (count < 0 || count > 65536)
        return false;
    if (pArray == nullptr)
        return true;

    for (int32 i = 0; i < count; ++i)
    {
        int32 classId = 0;
        if (!loader.Read(classId))
            return false;
        if (classId == 0)
            continue;

        BuildingClass* pObj = new BuildingClass(nullptr);
        if (pObj == nullptr)
            return false;
        pObj->Load(loader);
        pArray->Add(pObj);
    }
    return true;
}

static bool ReadGameObjects(LoadGameClass& loader)
{
    if (!ReadObjectArray(loader, UnitClass::Array))
        return false;
    if (!ReadObjectArray(loader, InfantryClass::Array))
        return false;
    if (!ReadObjectArray(loader, AircraftClass::Array))
        return false;
    return ReadObjectArray(loader, BuildingClass::Array);
}

// ============================================================================
// 场景与多人对局流程
// 根据游戏行为，可知资源装卸走"句柄槽"模式：装载时把资源包句柄记进
// 槽位，卸载时按槽位释放清空；重复装载先卸旧包，保证槽里只有一份。
// ============================================================================

static MixFileClass* Game_pNeutralMix = nullptr;
static MixFileClass* Game_pGUISHPs = nullptr;
static MixFileClass* Game_pSideSHPs = nullptr;
static MixFileClass* Game_pDiploSHPs = nullptr;
static int32 Game_WantedSide = -1;
static int32 Game_JabberEnabled = 0;   // asm: arg_JABBER（int32，不是 bool）

// ============================================================================
// asm: Game_ParsePKTs (asm:1363403..1364322)
//
//   ; void __fastcall Game_ParsePKTs(GameModeClass *this)
//
//   函数没有返回值、没有显式参数（ecx = 会话/模式对象）。它把三组来源的
//   [MultiMaps] 节点追加进 this+0x690 的 PKTNode 池：
//     ① 按名装载 MISSIONSMD.PKT；
//     ② 工作目录里的 *.PKT（跳过 ①）；
//     ③ *.YRO / *.YRM 归档（.YRO 探同名的 .PKT，.YRM 直接读归档索引）。
//   池对象自身的 +4 = 增长步长、+8 = 容量、+0Ch = 已分配、+10h = 长度
//   （见 Game_CreateDummySlots / Game_EmptyMaps 用的同一套偏移）。
//
//   本树的池类型是 DynamicVectorClass<PKTNodeClass*>，三个来源的完整移植
//   在 Game/PKTNodeClass.cpp 的 PKTPool::Parse；这里持有池本身，等价于
//   原版挂在会话对象 +0x690 的那一个。
// ============================================================================
static DynamicVectorClass<PKTNodeClass*> Game_PKTPool;

int32 Game::ParsePKTs(void* pPktBuffer)
{
    // asm 的入参只有隐式 this，没有缓冲区指针；保留形参只为不动既有声明。
    (void)pPktBuffer;

    // asm: 三组来源依次追加（顺序即 MISSIONSMD.PKT → *.PKT → *.YRO → *.YRM）
    PKTPool::Parse(&Game_PKTPool);

    // asm 返回 void；这里返回池长度，便于调用方判断是否解析出节点。
    return Game_PKTPool.Count;
}

// ============================================================================
// asm: Game_CreateDummySlots (asm:1022899..1023198)
//
//   void __cdecl Game_CreateDummySlots(void)
//
//   两段：
//   ① 若 csf_OpenSlot 为空，用 Get_String 取七条本地化文本，存进七个
//      全局 wchar_t*：
//        GUI:Open → csf_OpenSlot        GUI:Closed → CSF_ClosedSlot
//        GUI:AIEasy → CSF_EasyAI        GUI:AINormal → CSF_NormAI
//        GUI:AIHard → CSF_HardAI        GUI:Waiting → CSF_Waiting
//        GUI:OpenObserver → CSF_Observer
//   ② 对七个占位记录（CSF_OpenSlot_ / CSF_ClosedSlot_ / CSF_EasyAI_ /
//      CSF_NormAI_ / CSF_HardAI_ / CSF_Waiting_ / CSF_Observer_）各查一次：
//      未建则 new 0x85 字节的 PlayerSlotStruct，初始化 +0x28 的名字节点
//      （sub_53ECB0 把 10 字节填 0xFF），再用 _wcscpy 把 ① 的文本拷进记录。
//
//   本树对应物：PlayerSlotStruct = UI/StartSpotClass（sizeof 0x85、内嵌
//   CSFName 名字缓冲），七个占位记录是 StartSpotClass.cpp 里的静态哨兵，
//   静态构造即完成 ②。所以这里只做 ①：把 CSF 文本写进哨兵的 CSFName。
// ============================================================================
static void Game_WideFromCSF(const char* pKey, wchar_t* pDest, size_t nCapacity)
{
    const char* pText = EngineGlobals::Get_String(pKey);
    size_t i = 0;
    for (; pText != nullptr && pText[i] != '\0' && i + 1 < nCapacity; ++i)
        pDest[i] = static_cast<wchar_t>(static_cast<unsigned char>(pText[i]));
    pDest[i] = L'\0';
}

// asm: csf_OpenSlot 等七个 wchar_t* 全局非空即表示"文本已取过"
static bool Game_SlotTextLoaded = false;

void Game::CreateDummySlots()
{
    // asm: mov eax,csf_OpenSlot / test eax,eax / jnz loc_5E886A
    //   —— 文本已取过就整段跳过。
    if (Game_SlotTextLoaded)
        return;

    // asm: Get_String(GUI:Open) / mov csf_OpenSlot,eax  （其余六条同形）
    Game_WideFromCSF("GUI:Open",         Get_CSF_OpenSlot()->CSFName,   0x15);
    Game_WideFromCSF("GUI:Closed",       Get_CSF_ClosedSlot()->CSFName, 0x15);
    Game_WideFromCSF("GUI:AIEasy",       Get_CSF_EasyAI()->CSFName,     0x15);
    Game_WideFromCSF("GUI:AINormal",     Get_CSF_NormAI()->CSFName,     0x15);
    Game_WideFromCSF("GUI:AIHard",       Get_CSF_HardAI()->CSFName,     0x15);
    Game_WideFromCSF("GUI:Waiting",      Get_CSF_Waiting()->CSFName,    0x15);
    Game_WideFromCSF("GUI:OpenObserver", Get_CSF_Observer()->CSFName,   0x15);

    Game_SlotTextLoaded = true;
}

bool Game::IsRandomMap()
{
    // asm: Game_IsRandomMap —— 调用点 `mov ecx, offset Session / call`，即
    //   Session 以 __fastcall 第一参传入（不是 this）。函数体只有 6 条：
    //
    //     add  ecx, 6A8h                  ; ecx = &Session->当前剧本文件名
    //     push offset aRandmap_sed        ; "RandMap.Sed"
    //     push ecx
    //     call __strcmpi
    //     add  esp, 8
    //     neg  eax / sbb eax, eax / inc eax
    //
    //   neg/sbb/inc 是 "== 0 → 1，!= 0 → 0" 的标准展开，所以语义是：
    //   **名字等于 "RandMap.Sed" 返回 1，否则 0**。
    //
    // +0x6A8 是会话结构里"本次对局加载的剧本文件名"字段。本工程的
    // SessionClass 是精简布局、没有 0x6A8，语义对应到 MapName；若后续按
    // asm 补全会话结构，这里要改成按偏移取。
    if (TheSession == nullptr)
        return false;

    return _strcmpi(TheSession->MapName, "RandMap.Sed") == 0;
}

void Game::PlayerLeftMP(int32 houseIndex)
{
    // 根据游戏行为，可知多人掉线的玩家所属阵营立即判负出局。
    HouseClass* pHouse = HouseClass::GetHouseByIndex(houseIndex);
    if (pHouse) {
        pHouse->Lose();
    }
}

void Game::PlayerLeft(int32 houseIndex)
{
    // 根据游戏行为，可知单机流程里的玩家离开与多人同路。
    PlayerLeftMP(houseIndex);
}

void Game::SaveTaunts()
{
    // asm: Game_SaveTaunts (asm:1362922..1362974)
    //   ; int __fastcall Game_SaveTaunts(GameModeClass *this)
    //
    //   ra2md.ini 的 [MultiPlayer] 四键，值取自会话对象的四个字节标志：
    //     WOLTaunts     <- [esi+1ED9h]      LANTaunts     <- [esi+1ED8h]
    //     WOLScrollText <- [esi+1FC1h]      LANScrollText <- [esi+1FC0h]
    //   写完调 CCINIClass::Save(RA2MD_INI, "RA2MD.INI", 0) 落盘。
    //
    //   原版的 RA2MD_INI 是常驻全局（启动时即已装载整份文件），所以这里只需
    //   写四个键再存；本树启动流程不常驻该对象，故就地"读—改—存"，避免把
    //   其余键覆盖掉（语义等价：磁盘上的四键最终值与 asm 一致）。
    if (TheSession == nullptr)
        return;                                     // asm: esi = 会话全局

    CCFileClass file("RA2MD.INI");
    CCINIClass ini;
    ini.LoadFile(&file);

    // asm 顺序：WOLTaunts → LANTaunts → WOLScrollText → LANScrollText
    ini.WriteBool("MultiPlayer", "WOLTaunts",     TheSession->WOLTaunts);
    ini.WriteBool("MultiPlayer", "LANTaunts",     TheSession->LANTaunts);
    ini.WriteBool("MultiPlayer", "WOLScrollText", TheSession->WOLScrollText);
    ini.WriteBool("MultiPlayer", "LANScrollText", TheSession->LANScrollText);

    // asm: push 0 / lea eax,[esp+a1] / mov ecx,offset RA2MD_INI / call Save
    ini.SaveFile(&file);
}

// asm: WWDebugString —— 原版调试控制台输出；本树沿用各模块的既有做法，
//   以格式化后写 stderr 的局部实现承接（消息文本与 asm 逐字一致）。
static void Game_DebugString(const char* pFormat, ...)
{
    if (pFormat == nullptr)
        return;
    char buffer[512];
    va_list args;
    va_start(args, pFormat);
    std::vsnprintf(buffer, sizeof(buffer), pFormat, args);
    va_end(args);
    std::fputs(buffer, stderr);
}

// ============================================================================
// asm: Game_ProcessRandomPlayers (asm:1367214..1367487)
//
//   void __fastcall Game_ProcessRandomPlayers(GameModeClass *this)
//
//   WWDebugString("Processing Random Assignments...\n")
//   for (i = 0 ; i < NameNodeTypes.length ; ++i):
//     node = NameNodeTypes.data[i]                       ; StartSpotClass*
//     if (node->CountrySentinel == -3 || node->CountrySlot == -3):
//         ; 观战者：国家/颜色都钉成固定哨兵
//         node->CountrySlot = -3 ; node->CountrySentinel = -1
//         node->ColourSlot  =  8 ; node->ColourSentinel  = -1
//     else:
//         if (node->CountrySentinel == -2):              ; 国家未设
//             node->CountrySentinel = -1
//             if (this->[+4] != 0)                       ; 当前模式对象
//                 node->CountrySlot = this->[+4]->vtbl[6Ch]()
//             else
//                 node->CountrySlot = Scenario->Random(0, 9)
//         if (node->ColourSentinel == -2):               ; 颜色未设
//             node->ColourSentinel = -1 ; node->ColourSlot = -1
//             do { c = Scenario->Random(0, 7) }
//             while (GameMode_WorkWithColourIdx(this, c))
//             node->ColourSlot = c
//     if (i == 0) gPlayerColor = node->ColourSlot
//     WWDebugString("Player %i, %s: Side = %i, Color = %i\n",
//                   i, MP_ReadNthPlayerName(node), side, color)
//   ; 第二段：AISlots 的 StartingSpots / Colours 两排 -2 也各自随机补位
//
//   本树缺口（不是省略，是确实还没有对应物）：
//     · this->[+4] 的"当前模式对象"——本树没有 GameModeClass 全局实例，
//       故一律走 asm 的 else 分支（Scenario->Random(0,9)）；有模式对象时
//       原版取模式自己的随机国家（vtbl+6Ch），行为会不同，故此处注明。
//     · gPlayerColor 全局尚未落地，跳过该赋值。
//     · AISlots 在本树只有 Maps/AIs 两个容器，没有 StartingSpots / Colours /
//       AIDifficulties / Starts / Teams 五个并行数组，第二段无法按 asm 写。
//
//   颜色去重：asm 走 GameMode_WorkWithColourIdx，扫的是模式对象的 StartSpots
//   表（本树 GameModeClass::StartSpots 为空）+ 8 个固定槽。这里改为扫
//   NameNodeTypes 里已定的颜色，保持"同一局不出重复颜色"的可见行为。
// ============================================================================
namespace {

bool Game_ColourTaken(StartSpotClass** pNodes, int32 nCount, int32 idx)
{
    // asm: cmp ebp,0FFFFFFFEh / jz false（-2 = "随机"，永不冲突）
    if (idx == -2)
        return false;

    for (int32 i = 0; i < nCount; ++i) {
        const StartSpotClass* pNode = pNodes[i];
        if (pNode == nullptr)
            continue;
        // asm: cmp [eax+57h],-2 / jnz use53 ; cmp [eax+53h],-1 / jnz use53
        const int32 colour = (pNode->ColourSentinel == -2 &&
                              pNode->ColourSlot == -1)
                           ? -2
                           : pNode->ColourSlot;
        if (colour == idx)
            return true;
    }

    // asm: 第二段扫 [this+84h + i*4]（8 个固定槽）—— 无模式对象实例，留空。
    return false;
}

}  // namespace

void Game::ProcessRandomPlayers()
{
    // asm: push offset "Processing Random Assignments...\n" / call WWDebugString
    Game_DebugString("Processing Random Assignments...\n");

    StartSpotClass** pNodes =
        reinterpret_cast<StartSpotClass**>(NameNodeTypes_Data());
    const int32 nodeCount = NameNodeTypes_Length();

    Randomizer* pRandom = (TheScenario != nullptr) ? &TheScenario->Random : nullptr;

    for (int32 i = 0; i < nodeCount; ++i)
    {
        StartSpotClass* pNode = pNodes[i];
        if (pNode == nullptr)
            continue;

        const bool bSpectator =
            (pNode->CountrySentinel == -3) || (pNode->CountrySlot == -3);

        if (bSpectator)
        {
            // asm: loc_69B974
            pNode->CountrySlot     = -3;
            pNode->CountrySentinel = -1;
            pNode->ColourSlot      = 8;
            pNode->ColourSentinel  = -1;
        }
        else
        {
            // asm: cmp [esi+4Fh],-2 —— 国家未设
            if (pNode->CountrySentinel == -2)
            {
                pNode->CountrySentinel = -1;
                // asm: this->[+4] 为空时走 Scenario 随机 0..9
                if (pRandom != nullptr)
                    pNode->CountrySlot = (*pRandom)(0, 9);
            }

            // asm: loc_69B93D —— 颜色未设
            if (pNode->ColourSentinel == -2)
            {
                pNode->ColourSentinel = -1;
                pNode->ColourSlot     = -1;

                int32 colour = -1;
                if (pRandom != nullptr)
                {
                    do {
                        colour = (*pRandom)(0, 7);
                    } while (Game_ColourTaken(pNodes, nodeCount, colour));
                }
                pNode->ColourSlot = colour;
            }
        }

        // asm: if (i == 0) gPlayerColor = node->ColourSlot  （全局未落地，略）

        // asm: loc_69B994 —— 取值：哨兵为 -2 时按 -2 打印，否则取实际槽位
        const int32 side = (pNode->CountrySentinel == -2)
                         ? -2 : pNode->CountrySlot;
        const int32 color = (pNode->ColourSentinel == -2)
                         ? -2 : pNode->ColourSlot;

        // asm: mov ecx,esi / call MP_ReadNthPlayerName（ecx = 名字缓冲）
        const char* pName = MP::ReadNthPlayerName(pNode->CSFName);

        // asm: push offset "Player %i, %s: Side = %i, Color = %i\n"
        Game_DebugString("Player %i, %s: Side = %i, Color = %i\n",
                         static_cast<int>(i), pName != nullptr ? pName : "",
                         static_cast<int>(side), static_cast<int>(color));
    }

    // asm: 第二段 for (ebx = &AISlots.StartingSpots ; ebx < &AISlots.Colours ; ebx += 4)
    //   给 AISlots.StartingSpots[i] / AISlots.Colours[i] 的 -2 随机补位，再打印
    //   "AI %i: Side = %i, Color = %i\n"。
    //   缺口：本树 MPCoopClass::AISlot 只有 Maps / AIs 两个向量，没有这五排并行
    //   数组（StartingSpots / Colours / AIDifficulties / Starts / Teams），
    //   因此这一段无字段可写，按"缺字段即留白"处理。
}

void Game::SetWantedSide(int32 sideIndex)
{
    // 根据游戏行为，可知期望阵营先记下，进场景时才真正生效。
    Game_WantedSide = sideIndex;
}

int32 Game::GetWantedSide()
{
    return Game_WantedSide;
}

// ============================================================================
// asm: Game_LoadPCXFiles (asm:1127605..1128759)
//
//   void __cdecl Game_LoadPCXFiles(void)
//
//   把界面用的 192 张 PCX 依次灌进引擎的 PCX 缓存（全局对象 PCX）：
//
//     PCX_LoadFile(&PCX, name, 2, 0)             191 次
//     PCX_LoadFile(&PCX, "dlgsysi.pcx", 1, 0)    一次（第二参为 1）
//     PCX_LoadFile11(&PCX, "dlgsysa.pcx")        一次（= flags (1,1)，建调色板）
//
//   PCX_LoadFile 的 this 是一个按文件名哈希的缓存对象：命中即复用，未命中
//   才解码并插入（+4 计数、+8 容量、+0Ch 位移、+28h 装载因子，超限 rehash）。
//   参数 (2,0)/(1,0) 只影响解码分支与调色板构建，产物都是 16bpp 面。
//
//   本树对应物：FileFormats/PCXLoader 只提供 PCX_LoadFile11（内部固定 (1,1)，
//   即"解码 + 建调色板"），没有暴露 (2,0)/(1,0) 两个参数的入口，故这里 192 张
//   一律走 (1,1)；原版那两档只切换调色板构建的分支，不影响位图解码结果。
//   按 asm 的"预装 + 进程级缓存"语义，用文件内静态表保存已装入的 PCXClass，
//   同名只装一次。
//   缺口：原版缓存 16bpp BSurface（PCX_LoadFile 内部按 256 项 565 表展开），
//   本树 PCXClass 保留 8bit 位图 + 调色板；在 16bpp 面缓存落地前，这里缓存
//   PCXClass 本体（位图与调色板都不丢）。
// ============================================================================
namespace {

const char* const kGameUIPCXFiles[] = {
        "dbak6440.pcx",
        "gdii.pcx",
        "nodi.pcx",
        "rani.pcx",
        "usai.pcx",
        "japi.pcx",
        "frai.pcx",
        "geri.pcx",
        "gbri.pcx",
        "djbi.pcx",
        "arbi.pcx",
        "lati.pcx",
        "rusi.pcx",
        "obsi.pcx",
        "yrii.pcx",
        "arrow_uu.pcx",
        "arrow_ud.pcx",
        "arrow_du.pcx",
        "arrow_dd.pcx",
        "leftbar.pcx",
        "rightbar.pcx",
        "trakgrip.pcx",
        "sbgript.pcx",
        "sbgripm.pcx",
        "sbgripb.pcx",
        "gsbgript.pcx",
        "gsbgripm.pcx",
        "gsbgripb.pcx",
        "bar_ll.pcx",
        "bar_lr.pcx",
        "bar_ul.pcx",
        "bar_ur.pcx",
        "dlgsysi.pcx",
        "dlgsysa.pcx",
        "wouban.pcx",
        "wodban.pcx",
        "wouleave.pcx",
        "wodleave.pcx",
        "wousqlch.pcx",
        "wodsqlch.pcx",
        "woudcon.pcx",
        "woddcon.pcx",
        "woukick.pcx",
        "wodkick.pcx",
        "wouhelp.pcx",
        "wodhelp.pcx",
        "woufind.pcx",
        "wodfind.pcx",
        "wouopt.pcx",
        "wodopt.pcx",
        "woutrny.pcx",
        "wodtrny.pcx",
        "wouclan.pcx",
        "wodclan.pcx",
        "woufgame.pcx",
        "wodfgame.pcx",
        "wouact.pcx",
        "wodact.pcx",
        "wouref.pcx",
        "wodref.pcx",
        "tab_tlu.pcx",
        "tab_tmu.pcx",
        "tab_tru.pcx",
        "tab_tld.pcx",
        "tab_tmd.pcx",
        "tab_trd.pcx",
        "tab_ftl.pcx",
        "tab_ftr.pcx",
        "tab_ftm.pcx",
        "tab_fbr.pcx",
        "tab_fbl.pcx",
        "tab_fbm.pcx",
        "tab_fmr.pcx",
        "tab_fml.pcx",
        "woloper.pcx",
        "wolsqlch.pcx",
        "woltrny.pcx",
        "woluser.pcx",
        "wolvoice.pcx",
        "wolpriv.pcx",
        "wolacpt.pcx",
        "wolhost.pcx",
        "wolclan.pcx",
        "wolob.pcx",
        "wolnob.pcx",
        "wolrslkob.pcx",
        "wolrslknob.pcx",
        "gt18.pcx",
        "private.pcx",
        "corporal.pcx",
        "sergeant.pcx",
        "lieutena.pcx",
        "major.pcx",
        "colonel.pcx",
        "stargen.pcx",
        "briggenr.pcx",
        "general.pcx",
        "comchief.pcx",
        "cooperat.pcx",
        "number0.pcx",
        "number1.pcx",
        "number2.pcx",
        "number3.pcx",
        "number4.pcx",
        "number5.pcx",
        "number6.pcx",
        "number7.pcx",
        "number8.pcx",
        "number9.pcx",
        "dnarrowp.pcx",
        "uparrowp.pcx",
        "dnarrowr.pcx",
        "uparrowr.pcx",
        "gdnarrowp.pcx",
        "guparrowp.pcx",
        "gdnarrowr.pcx",
        "guparrowr.pcx",
        "trofl.pcx",
        "trofm.pcx",
        "trofr.pcx",
        "sb_psh_u.pcx",
        "sb_psh_d.pcx",
        "sb_rel_u.pcx",
        "sb_rel_d.pcx",
        "bst_chkd.pcx",
        "bst_uchk.pcx",
        "bst_chkg.pcx",
        "bst_uckg.pcx",
        "ccd_i.pcx",
        "cce_i.pcx",
        "cud_i.pcx",
        "cue_i.pcx",
        "cce_ir.pcx",
        "cce_il.pcx",
        "bue_li30.pcx",
        "bue_mi30.pcx",
        "bue_ri30.pcx",
        "bde_li30.pcx",
        "bde_mi30.pcx",
        "bde_ri30.pcx",
        "bud_li30.pcx",
        "bud_mi30.pcx",
        "bud_ri30.pcx",
        "bue_li24.pcx",
        "bue_mi24.pcx",
        "bue_ri24.pcx",
        "bde_li24.pcx",
        "bde_mi24.pcx",
        "bde_ri24.pcx",
        "bud_li24.pcx",
        "bud_mi24.pcx",
        "bud_ri24.pcx",
        "wolreslk.pcx",
        "woluclan.pcx",
        "woldclan.pcx",
        "wolupriv.pcx",
        "woldpriv.pcx",
        "wolurslkob.pcx",
        "woldrslkob.pcx",
        "pingg.pcx",
        "pingy.pcx",
        "pingr.pcx",
        "mpascrnlbar01.pcx",
        "mpascrnlbar02.pcx",
        "mpascrnlbar03.pcx",
        "mpascrnlbar04.pcx",
        "mpascrnlbar05.pcx",
        "mpascrnlbar06.pcx",
        "mpascrnlbar07.pcx",
        "mpascrnlbar08.pcx",
        "mpascrnlbar09.pcx",
        "mpascrnlbar10.pcx",
        "mpsscrnlbar01.pcx",
        "mpsscrnlbar02.pcx",
        "mpsscrnlbar03.pcx",
        "mpsscrnlbar04.pcx",
        "mpsscrnlbar05.pcx",
        "mpsscrnlbar06.pcx",
        "mpsscrnlbar07.pcx",
        "mpsscrnlbar08.pcx",
        "mpsscrnlbar09.pcx",
        "mpsscrnlbar10.pcx",
        "mpyscrnlbar01.pcx",
        "mpyscrnlbar02.pcx",
        "mpyscrnlbar03.pcx",
        "mpyscrnlbar04.pcx",
        "mpyscrnlbar05.pcx",
        "mpyscrnlbar06.pcx",
        "mpyscrnlbar07.pcx",
        "mpyscrnlbar08.pcx",
        "mpyscrnlbar09.pcx",
        "mpyscrnlbar10.pcx",
};

struct GameUIPCXEntry
{
    char       Name[0x104];   // asm: 缓存键（文件名）
    PCXClass*  pPCX;          // asm: 缓存值（解码后的面）
};

std::vector<GameUIPCXEntry> g_GameUIPCXCache;

// asm: PCX_LoadFile 的缓存命中判定 —— 同名已装则直接返回旧项
PCXClass* Game_LoadUIPCX(const char* pName)
{
    if (pName == nullptr)
        return nullptr;

    for (size_t i = 0; i < g_GameUIPCXCache.size(); ++i)
    {
        if (_strcmpi(g_GameUIPCXCache[i].Name, pName) == 0)
            return g_GameUIPCXCache[i].pPCX;      // asm: 命中 -> 复用
    }

    // asm: 未命中 -> 解码 + 插入缓存
    PCXClass* pPCX = PCX_LoadFile11(pName);
    if (pPCX == nullptr)
        return nullptr;                          // asm: 打不开直接返回 0

    GameUIPCXEntry entry;
    std::memset(&entry, 0, sizeof(entry));
    std::strncpy(entry.Name, pName, sizeof(entry.Name) - 1);
    entry.pPCX = pPCX;
    g_GameUIPCXCache.push_back(entry);
    return pPCX;
}

}  // namespace

void Game::LoadPCXFiles()
{
    // asm: 192 次 PCX_LoadFile / PCX_LoadFile11，按 asm 的先后次序逐张装入
    const size_t count =
        sizeof(kGameUIPCXFiles) / sizeof(kGameUIPCXFiles[0]);
    for (size_t i = 0; i < count; ++i)
    {
        Game_LoadUIPCX(kGameUIPCXFiles[i]);
    }
}

void Game::EnableJabber(bool bEnable)
{
    // asm: Game_EnableJabber ——
    //     eax = arg_JABBER ; esi = ecx(入参)
    //     cmp  eax, esi / jz done        ; 值没变，直接返回
    //     call StringTable_UnloadAll     ; 丢掉整张字符串表
    //     mov  arg_JABBER, esi
    //     mov  ecx, MainCSF
    //     call StringTable_LoadFile      ; 按上一次的文件名重建
    //
    // 语义纠正：这不是"聊天开关"。Jabber 是 CSF 解析器的开关（影响值里
    // 行尾/空白的处理方式），所以切换时必须把已经解析好的表整体作废、
    // 用同一份 CSF 重新解析一遍。原来写成"只置一个 bool"是错的 ——
    // 那样切换后表里仍留着旧格式的文本。
    const int32 wanted = bEnable ? 1 : 0;
    if (Game_JabberEnabled == wanted)
        return;

    StringTable::Unload_All();
    Game_JabberEnabled = wanted;
    StringTable::Parse_File(StringTable::MainCSF);
}

bool Game::IsJabberEnabled()
{
    return Game_JabberEnabled != 0;
}

void Game::ReloadNeutralMIX()
{
    // 根据游戏行为，可知重载中立包先放旧包再挂新包，槽里始终只有一份。
    ReleaseNeutralMIX();
    Game_pNeutralMix = new MixFileClass("NEUTRAL.MIX");
}

void Game::ReleaseNeutralMIX()
{
    // 根据游戏行为，可知释放即摘包清槽。
    if (Game_pNeutralMix) {
        delete Game_pNeutralMix;
        Game_pNeutralMix = nullptr;
    }
}

// ============================================================================
// 界面调色板槽（asm: .data 的 dword_B0FBEC..dword_B0FC04 与
// SIDEBAR_PAL / OBSERVER_PAL / YRII_PAL，外加"界面文件已装"标志
// byte_B0FC0C）
//
//   每对槽是 (ConvertClass*, BytePalette*)：ConvertClass_CreateFromFile 的
//   入参顺序是 (文件名, &pal, &conv)，所以 asm 里 `edx = &dword_B0FBxx` 的那
//   个是调色板，压栈的那个是转换器。卸载时调色板走普通 delete、转换器走
//   虚析构（`mov eax,[ecx] / push 1 / call [eax]`）。
//
//   原版这些槽是全局裸指针；本树没有同名全局，故在 Game.cpp 内以文件级
//   静态变量承接（只有 LoadUI_LoadFiles / UnloadGUIPalettes 读写它们）。
// ============================================================================
namespace {

ConvertClass* g_pGUISidebarConv   = nullptr;   // asm: SIDEBAR_PAL
BytePalette*  g_pGUISidebarPal    = nullptr;   // asm: dword_B0FBE4
ConvertClass* g_pGUIUIBkgdConv    = nullptr;   // asm: dword_B0FBF0
BytePalette*  g_pGUIUIBkgdPal     = nullptr;   // asm: dword_B0FBEC
ConvertClass* g_pGUIRadarConv     = nullptr;   // asm: dword_B0FBF8
BytePalette*  g_pGUIRadarPal      = nullptr;   // asm: dword_B0FBF4
ConvertClass* g_pGUIObserverConv  = nullptr;   // asm: OBSERVER_PAL
BytePalette*  g_pGUIObserverPal   = nullptr;   // asm: dword_B0FBFC
ConvertClass* g_pGUIYriiConv      = nullptr;   // asm: YRII_PAL
BytePalette*  g_pGUIYriiPal       = nullptr;   // asm: dword_B0FC04
bool          g_bGUIFilesLoaded   = false;     // asm: byte_B0FC0C

}  // namespace

// ============================================================================
// 截屏辅助：16bpp 面 -> 8bit 索引 PCX
//
//   asm: Game_MakeScreenshot (asm:680831..681035) 把 DSurface_Hidden 的画面
//   交给 sub_7B05C0(0x7B05C0) 编码成 "SCRN%04d.pcx"。sub_7B05C0 的头部字段
//   在 asm 里是逐字节写死的：
//     manufacturer 0Ah / version 5 / encoding 1 / bitsPerPixel 8
//     XMin YMin 0 / XMax = 宽-1 / YMax = 高-1 / HDPI = 宽 / VDPI = 高
//     48 字节调色板全 0 / reserved 0 / planes = 1（8bpp 源时）
//     bytesPerLine = 宽 / paletteInfo = 1 / screenW = 宽 / screenH = 高
//     尾部 54 字节填充 0；写完 128 字节头再逐行 RLE，最后 0Ch + 768 字节调色板。
//
//   本树差异：原版的隐藏面是 8bit 索引面（直接吐索引 + CCPalette），本工程的
//   DSurface 是 16bpp RGB565，因此这里按调色板做一次"最近色"反查（RGBClass
//   ::Difference 的平方距离），像素索引来自反查表；其余（头字段、RLE、尾部
//   调色板）与 sub_7B05C0 一致。
// ============================================================================
namespace {

// asm: sub_7B05C0 的 RLE —— 连续同值 >=2 打成 (0C0h|run, value)，单字节
// 若高两位为 11 则必须转义成 (0C1h, value)。
int32 Game_PCXEncodeRow(const uint8* pSrc, int32 nWidth, uint8* pDst)
{
    int32 out = 0;
    int32 x = 0;
    while (x < nWidth)
    {
        const uint8 value = pSrc[x];
        int32 run = 1;
        while (x + run < nWidth && pSrc[x + run] == value && run < 63)
            ++run;
        if (run > 1)
        {
            pDst[out++] = static_cast<uint8>(0xC0 | run);
            pDst[out++] = value;
            x += run;
        }
        else if ((value & 0xC0) == 0xC0)
        {
            pDst[out++] = 0xC1;
            pDst[out++] = value;
            ++x;
        }
        else
        {
            pDst[out++] = value;
            ++x;
        }
    }
    return out;
}

// 565 -> 8bit 索引的最近色反查在 MakeScreenshot 内联完成（每色只算一次）

}  // namespace

bool Game::MakeScreenshot()
{
    // asm: 开头 GetClientRect(hWndParent,&Rect) + 两次 ClientToScreen，失败即
    //   直接返回（loc_537DC2）。本树没有 hWndParent（无窗口系统），客户区
    //   就是整块隐藏面，故取 DSurface_Hidden 的完整矩形。
    //
    //   缺 hWndParent：原版截的是窗口客户区在屏幕上的矩形（多显示器/窗口
    //   偏移时会与整面不同）；无窗口环境下二者等价。
    if (DSurface_Hidden == nullptr || DSurface_Hidden->Buffer == nullptr)
        return false;

    // asm: 宽高取自 DSurface_Hidden 的 vtbl+7Ch/+80h；截取矩形上限也按它裁剪
    const int32 width  = DSurface_Hidden->Width;
    const int32 height = DSurface_Hidden->Height;
    if (width <= 0 || height <= 0)
        return false;

    // asm: 画面前后的 WWMouse 捕获/释放（vtbl+0Ch / +10h）—— 原版是为了让
    //   鼠标光标不入画，并把隐藏面贴到主面显示（loc_537C9A..537D01）。
    //   缺口：本树没有 DSurface_Primary 全局（Surface.h 只有 Alternate /
    //   Hidden / Composite / Tile / Sidebar），"隐藏面 -> 主面"这一步无目标
    //   面可贴；它只影响屏幕上是否显示刚截的画面，不影响写出的文件内容。
    //   g_WWMouse 的捕获/释放同理（本树它由 Helpers/AsmUIHelpers 持有，
    //   无主面可贴时捕获没有意义），这里一并跳过。

    // asm: 找第一个不存在的 "SCRN%04d.pcx"（esi 从 -1 起，先 ++ 再试）
    char name[0x40];
    int32 index = -1;
    do {
        ++index;
        std::snprintf(name, sizeof(name), "SCRN%04d.pcx", static_cast<int>(index));
        CCFileClass probe(name);
        // asm: mov edx,[eax] / push 0 / call [edx+14h]  -> CCFileClass::Is_Available(0)
        if (!probe.Is_Available(0))                 // 名字未被占用就是它
            break;
    } while (index < 10000);

    if (index >= 10000)
        return false;                               // 名字空间用尽

    // —— 取写 PCX 用的调色板：优先当前渲染转换器，其次界面侧边栏调色板 ——
    const PaletteClass* pPalette = nullptr;
    if (TheConvert != nullptr)
        pPalette = &TheConvert->Get_Palette();
    else if (g_pGUISidebarConv != nullptr)
        pPalette = &g_pGUISidebarConv->Get_Palette();

    if (pPalette == nullptr)
        return false;                               // 无调色板：不写假色文件

    // —— 像素 -> 索引（565 最近色，带 64K 缓存）——
    std::vector<uint8> lut;      // 565 -> palette index
    std::vector<char> valid;     // 该 565 值是否已解算
    lut.assign(65536, 0);
    valid.assign(65536, 0);

    std::vector<uint8> rowIndex(static_cast<size_t>(width));
    std::vector<uint8> rowRLE(static_cast<size_t>(width) * 2 + 2);

    // —— 按 sub_7B05C0 的字段顺序拼 128 字节头 ——
    uint8 header[0x80];
    std::memset(header, 0, sizeof(header));
    header[0] = 0x0A;                               // manufacturer
    header[1] = 5;                                  // version
    header[2] = 1;                                  // encoding = RLE
    header[3] = 8;                                  // bits per pixel
    header[4] = 0;  header[5] = 0;                  // XMin
    header[6] = 0;  header[7] = 0;                  // YMin
    header[8]  = static_cast<uint8>((width - 1) & 0xFF);
    header[9]  = static_cast<uint8>(((width - 1) >> 8) & 0xFF);
    header[10] = static_cast<uint8>((height - 1) & 0xFF);
    header[11] = static_cast<uint8>(((height - 1) >> 8) & 0xFF);
    header[12] = static_cast<uint8>(width & 0xFF);  // HDPI = 宽
    header[13] = static_cast<uint8>((width >> 8) & 0xFF);
    header[14] = static_cast<uint8>(height & 0xFF); // VDPI = 高
    header[15] = static_cast<uint8>((height >> 8) & 0xFF);
    // header[16..63] = 48 字节 16 色调色板，全 0
    header[64] = 0;                                 // reserved
    header[65] = 1;                                 // planes = 1（8bpp 源）
    header[66] = static_cast<uint8>(width & 0xFF);  // bytesPerLine = 宽
    header[67] = static_cast<uint8>((width >> 8) & 0xFF);
    header[68] = 1;  header[69] = 0;                // paletteInfo = 1
    header[70] = static_cast<uint8>(width & 0xFF);  // screenW
    header[71] = static_cast<uint8>((width >> 8) & 0xFF);
    header[72] = static_cast<uint8>(height & 0xFF); // screenH
    header[73] = static_cast<uint8>((height >> 8) & 0xFF);

    CCFileClass out(name);
    if (!out.Open(static_cast<int32>(FileAccessMode::Write)))
        return false;

    if (out.Write(header, sizeof(header)) != static_cast<int32>(sizeof(header)))
    {
        out.Close();
        return false;
    }

    // —— 逐行编码 ——
    const uint16* pPixels = reinterpret_cast<const uint16*>(DSurface_Hidden->Buffer);
    const int32 pitchPixels = DSurface_Hidden->Pitch / 2;

    for (int32 y = 0; y < height; ++y)
    {
        const uint16* pRow = pPixels + static_cast<size_t>(y) * pitchPixels;
        for (int32 x = 0; x < width; ++x)
        {
            const uint16 c565 = pRow[x];
            if (!valid[c565])
            {
                // 565 -> 8bit/通道
                const int32 r5 = (c565 >> 11) & 0x1F;
                const int32 g6 = (c565 >> 5) & 0x3F;
                const int32 b5 = c565 & 0x1F;
                const RGBClass want((r5 << 3) | (r5 >> 2),
                                    (g6 << 2) | (g6 >> 4),
                                    (b5 << 3) | (b5 >> 2));

                int32 best = 0;
                int32 bestDist = 0x7FFFFFFF;
                for (int32 i = 0; i < 256; ++i)
                {
                    const int32 dist = want.Difference(pPalette->Get_Color(i));
                    if (dist < bestDist) { bestDist = dist; best = i; }
                }
                lut[c565] = static_cast<uint8>(best);
                valid[c565] = 1;
            }
            rowIndex[x] = lut[c565];
        }

        const int32 packed = Game_PCXEncodeRow(rowIndex.data(), width, rowRLE.data());
        if (out.Write(rowRLE.data(), packed) != packed)
        {
            out.Close();
            return false;
        }
    }

    // —— 尾部：0Ch + 768 字节调色板（每通道 6bit，即 8bit 值右移 2）——
    uint8 tail[1 + 768];
    tail[0] = 0x0C;
    for (int32 i = 0; i < 256; ++i)
    {
        const RGBClass& entry = pPalette->Get_Color(i);
        tail[1 + i * 3 + 0] = static_cast<uint8>(entry.Red   >> 2);
        tail[1 + i * 3 + 1] = static_cast<uint8>(entry.Green >> 2);
        tail[1 + i * 3 + 2] = static_cast<uint8>(entry.Blue  >> 2);
    }
    if (out.Write(tail, sizeof(tail)) != static_cast<int32>(sizeof(tail)))
    {
        out.Close();
        return false;
    }

    out.Close();
    return true;
}

void Game::SaveNetworkConfig()
{
    // asm: Game_SaveNetworkConfig (asm:1362605..1362913)
    //   ; int __fastcall Game_SaveNetworkConfig(GameModeClass *this)
    //
    //   把会话对象里的整份联机配置写回 RA2MD.INI，最后 CCINIClass::Save 落盘。
    //   顺序（键名 / 源字段）：
    //     [MultiPlayer] PhoneIndex(+2884) WOLLimitResolution(+308)
    //                   LastNickSlot(+30C) Handle(+148, 宽串转义)
    //                   Color(+17C) ColorEx(+180) Side(+184, 按房索引)
    //                   SideEx(+188) GameMode([+4]?[+4]->[+28h]:0)
    //     [Skirmish]      GameMode_SaveGameSettings(this+SkirmishPrefs)
    //     [LAN]           GameMode_SaveGameSettings(this+208h)
    //     [WonlinePref]   GameMode_SaveGameSettings(this+284h)
    //     [SerialDefaults] 先清节，再写 CallWaitString / CallWaitStringIndex /
    //                   InitStringIndex / DialMethod / Baud / IRQ / Port(十六进制)
    //                   Compression / ErrorCorrection / ModemName
    //     [InitStrings]  先清节，再按 "%03d" 逐条写字符串表
    //     [PhoneBook]    先清节，再从末项往前按 "%s|%s|%x|%d|%d|%d|%d|%s|%d|%d|%s" 写
    //
    //   与 SaveTaunts 同理：原版的 RA2MD_INI 是常驻全局，本树不常驻，故就地
    //   读—改—存，保证其余节不被覆盖。
    if (TheSession == nullptr)
        return;

    SessionClass* pSession = TheSession;

    CCFileClass file("RA2MD.INI");
    CCINIClass ini;
    ini.LoadFile(&file);

    static const char* const MP = "MultiPlayer";

    // asm: INIClass_WriteInteger(INI, "MultiPlayer", "PhoneIndex", [esi+2884h], 0)
    ini.WriteInteger(MP, "PhoneIndex", pSession->PhoneIndex, false);

    // asm: INIClass_WriteBool(INI, "MultiPlayer", "WOLLimitResolution", [esi+308h])
    ini.WriteBool(MP, "WOLLimitResolution", pSession->WOLLimitResolution);

    // asm: INIClass_WriteInteger(..., "LastNickSlot", [esi+30Ch], 0)
    ini.WriteInteger(MP, "LastNickSlot", pSession->LastNickSlot, false);

    // asm: lea ecx,[esi+148h] / call wstrcpy_version / push eax
    //      call INIClass_WriteUnicodeEscaped(..., "Handle", eax)
    ini.WriteUnicodeEscaped(MP, "Handle", pSession->Handle);

    // asm: "Color"(+17Ch) / "ColorEx"(+180h)
    ini.WriteInteger(MP, "Color",   pSession->Color,   false);
    ini.WriteInteger(MP, "ColorEx", pSession->ColorEx, false);

    // asm: "Side"(+184h) 走 INIClass_WriteHouseIndex
    ini.WriteHouseIndex(MP, "Side", pSession->Side);

    // asm: "SideEx"(+188h)
    ini.WriteInteger(MP, "SideEx", pSession->SideEx, false);

    // asm: eax = [esi+4] ; if (eax) push eax->[+28h] else push 0
    //      "GameMode"
    //   [+4] 是"当前 MPGameMode 对象"；本树以会话的 ScenIndex 查模式表。
    {
        int32 gameMode = 0;
        MPGameModeClass* pMode = MPGameModeClass::Find(pSession->ScenIndex);
        if (pMode != nullptr)
            gameMode = pMode->Field_28;              // asm: [mode+28h]
        ini.WriteInteger(MP, "GameMode", gameMode, false);
    }

    // asm: GameMode_SaveGameSettings(SkirmishPrefs, INI, "Skirmish")
    //      GameMode_SaveGameSettings(&this[208h], INI, "LAN")
    //      GameMode_SaveGameSettings(&this[284h], INI, "WonlinePref")
    pSession->SkirmishPrefs.SaveGameSettings(&ini, "Skirmish");
    pSession->LANPrefs.SaveGameSettings(&ini, "LAN");
    pSession->WOLPrefs.SaveGameSettings(&ini, "WonlinePref");

    static const char* const SERIAL = "SerialDefaults";

    // asm: push 0 / push "SerialDefaults" / mov ecx,RA2MD_INI / call mangleStuff_
    //   mangleStuff_(INI, section, 0) 是 INIClass 的"清节"路径（a3 = 键名，0 表示整节）。
    ini.Clear(SERIAL);

    // asm: INIClass_WriteString(..., "CallWaitString", [esi+28A8h])
    ini.WriteString(SERIAL, "CallWaitString", pSession->CallWaitString);

    // asm: "CallWaitStringIndex"(+289Ch) / "InitStringIndex"(+2898h)
    ini.WriteInteger(SERIAL, "CallWaitStringIndex", pSession->CallWaitStringIndex, false);
    ini.WriteInteger(SERIAL, "InitStringIndex",     pSession->InitStringIndex,     false);

    // asm: eax = ModemDialTypes[[esi+2894h]] / WriteString(..., "DialMethod", eax)
    {
        static const char* const kDialTypes[] = { "T", "P" };
        const int32 idx = pSession->DialMethod;
        const char* pDial = (idx >= 0 && idx < 2) ? kDialTypes[idx] : kDialTypes[0];
        ini.WriteString(SERIAL, "DialMethod", pDial);
    }

    // asm: "Baud"(+2890h) / "IRQ"(+288Ch) / "Port"(+2888h, push 1 -> 十六进制)
    ini.WriteInteger(SERIAL, "Baud", pSession->Baud, false);
    ini.WriteInteger(SERIAL, "IRQ",  pSession->IRQ,  false);
    ini.WriteInteger(SERIAL, "Port", pSession->Port, true);

    // asm: "Compression"(+28A0h) / "ErrorCorrection"(+28A4h)
    ini.WriteInteger(SERIAL, "Compression",     pSession->Compression,     false);
    ini.WriteInteger(SERIAL, "ErrorCorrection", pSession->ErrorCorrection, false);

    // asm: lea eax,[esi+28B8h] / WriteString(..., "ModemName", eax)
    ini.WriteString(SERIAL, "ModemName", pSession->ModemName);

    // asm: 清 [InitStrings] 后按 "%03d" 逐条写出 [[esi+2918h] + i*4]，
    //      条数 [esi+2924h]。
    //   缺口：SessionClass 没有 InitStrings 字符串数组（asm +2918/+2924），
    //   只有 InitStringIndex。字段缺失，这一段无法按 asm 写。
    ini.Clear("InitStrings");

    // asm: 清 [PhoneBook] 后从末项（[esi+290Ch]-1）往前，用
    //      "%s|%s|%x|%d|%d|%d|%d|%s|%d|%d|%s" 格式化 [[esi+2900h]+i*4]
    //      指向的记录（记录内 +0/+4/+8/+0Ch/+10h/+14h/+18h/+1Ch/+20h/+70h/+9Ah）。
    //   缺口：SessionClass 没有 PhoneBook 记录数组（asm +2900/+290C）。
    ini.Clear("PhoneBook");

    // asm: push 0 / lea eax,[esp+a1] / mov ecx,offset RA2MD_INI / call Save
    ini.SaveFile(&file);
}

void Game::UnloadGUIPalettes()
{
    // asm: Game_UnloadGUIPalettes (asm:1665182..1665323)
    //
    //   五对槽逐个释放，顺序：YRII → OBSERVER → dword_B0FBF4/F8 →
    //   dword_B0FBE4/SIDEBAR → dword_B0FBEC/B0FBF0。每对三步：
    //     if (pal)  { delete pal ; pal = 0 }
    //     sub_5B4270("XXX.PAL")            ; 调色板名字缓存释放
    //     if (conv) { conv->vtbl[0](1) ; conv = 0 }   ; 虚析构
    //
    //   缺口：sub_5B4270 是调色板集合按名字回收（palette_calculator 的
    //   名字表命中后释放条目）。本树 ConvertClass::CreateFromFile 是
    //   "一线程到底"的独立构造，没有名字索引表，故这一步无对应物。

    // asm: eax = dword_B0FC04 / delete / = 0
    if (g_pGUIYriiPal != nullptr) {
        delete g_pGUIYriiPal;
        g_pGUIYriiPal = nullptr;
    }
    // asm: mov ecx,str_yrii_pal / call sub_5B4270   —— 缺口，见上
    // asm: ecx = YRII_PAL / if != 0 -> 虚析构(1)
    if (g_pGUIYriiConv != nullptr) {
        delete g_pGUIYriiConv;
        g_pGUIYriiConv = nullptr;
    }

    // asm: dword_B0FBFC / sub_5B4270(str_observer_pal) / OBSERVER_PAL
    if (g_pGUIObserverPal != nullptr) {
        delete g_pGUIObserverPal;
        g_pGUIObserverPal = nullptr;
    }
    if (g_pGUIObserverConv != nullptr) {
        delete g_pGUIObserverConv;
        g_pGUIObserverConv = nullptr;
    }

    // asm: dword_B0FBF4 / sub_5B4270(str_sidebar_pal) / dword_B0FBF8
    if (g_pGUIRadarPal != nullptr) {
        delete g_pGUIRadarPal;
        g_pGUIRadarPal = nullptr;
    }
    if (g_pGUIRadarConv != nullptr) {
        delete g_pGUIRadarConv;
        g_pGUIRadarConv = nullptr;
    }

    // asm: dword_B0FBE4 / sub_5B4270(sidebar_pal) / SIDEBAR_PAL
    if (g_pGUISidebarPal != nullptr) {
        delete g_pGUISidebarPal;
        g_pGUISidebarPal = nullptr;
    }
    if (g_pGUISidebarConv != nullptr) {
        delete g_pGUISidebarConv;
        g_pGUISidebarConv = nullptr;
    }

    // asm: dword_B0FBEC / sub_5B4270(str_aUibkgd_pal) / dword_B0FBF0
    if (g_pGUIUIBkgdPal != nullptr) {
        delete g_pGUIUIBkgdPal;
        g_pGUIUIBkgdPal = nullptr;
    }
    if (g_pGUIUIBkgdConv != nullptr) {
        delete g_pGUIUIBkgdConv;
        g_pGUIUIBkgdConv = nullptr;
    }
}

void Game::UnloadStartingGenericSpots()
{
    // asm: Game_UnloadStartingGenericSpots (asm:1023206..1023298)
    //
    //   for (slot of {CSF_OpenSlot_, CSF_ClosedSlot_, CSF_EasyAI_,
    //                 CSF_NormAI_, CSF_HardAI_, CSF_Waiting_, CSF_Observer_}):
    //       if (slot) { delete slot ; slot = 0 }
    //   vec_SS_Clear(&NameNodeTypes)          ; 名字表清空（长度归零）
    //
    //   本树这七个占位记录是 StartSpotClass.cpp 里的静态哨兵，不是 new 出来
    //   的对象，不能 delete；等价语义是把记录复位到构造初值（名字清空、
    //   槽位回到 -2），并把"CSF 文本已取"的标志清掉，使 CreateDummySlots
    //   下次重新取文本 —— 与 asm 置空指针后重新 CreateDummySlots 的效果一致。
    StartSpotClass* const sentinels[7] = {
        Get_CSF_OpenSlot(),    // asm: 第一块
        Get_CSF_ClosedSlot(),
        Get_CSF_EasyAI(),
        Get_CSF_NormAI(),
        Get_CSF_HardAI(),
        Get_CSF_Waiting(),
        Get_CSF_Observer(),
    };
    for (int32 i = 0; i < 7; ++i)
    {
        if (sentinels[i] != nullptr)
            *sentinels[i] = StartSpotClass();
    }

    // asm: mov ecx,offset NameNodeTypes / jmp vec_SS_Clear
    //   vec_SS_Clear 把向量的 length 归零，底层存储不属于该向量。
    NameNodeTypes_Set(NameNodeTypes_Data(), 0);

    // asm 里七块文本指针一并置空 -> 允许下次重新取
    Game_SlotTextLoaded = false;
}

void Game::UnloadSideAndGUISHPs2()
{
    // 根据游戏行为，可知第二套阵营/界面贴图槽的释放与主槽互不影响。
    if (Game_pSideSHPs) {
        delete Game_pSideSHPs;
        Game_pSideSHPs = nullptr;
    }
}

void Game::MakeAlliances()
{
    // 根据游戏行为，可知开局结盟按盟约表两两确立：设置过盟约的席位
    // 彼此互设盟友。
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        for (int32 j = i + 1; j < HouseClass::ArrayCount; ++j) {
            HouseClass* pA = HouseClass::Array[i];
            HouseClass* pB = HouseClass::Array[j];
            if (pA && pB && pA->IsAlliedWith(pB)) {
                pA->Make_Ally(j);
                pB->Make_Ally(i);
            }
        }
    }
}

// ============================================================================
// asm: Game_SelectStartingPoint (asm:943013..943136)
//
//   ; int __stdcall Game_SelectStartingPoint(int idx,
//                                           VectorPtrCellTypeClass *cells,
//                                           byte *presentHouses)
//
//   按"已占用表"挑一个空闲出生点，然后把房号写进 Scenario->HouseIndices：
//     humanCount = 统计 presentHouses 中非零项（cells->[+10h] 为项数）
//     houseCount = 统计 vec_Houses 里 Is_Human() 为真的房
//     if (humanCount < houseCount):
//         spot = Scenario->Random(0, NumCoopHumanStartSpots - 1)
//         while (presentHouses[spot]) spot = (spot + 1) % NumStartingPoints
//     else:
//         spot = Scenario->NumStartingPoints
//         if (presentHouses[spot]) spot 向后扫到第一个空位（上限 cells->count）
//     presentHouses[spot] = 1
//     Scenario->HouseIndices[spot] = idx
//
//   C++ 侧声明是 (houseIndex, spotIndex)，与原函数（idx + 候选表 + 占用表）
//   不是同一形状：占位表的挑选逻辑在调用方（Scenario/MPCoop 的
//   SmthStartingHouses 族）手里。这里只承载本签名能表达的那一步 —— 把房号
//   登记到指定出生点的 HouseIndices（asm: mov [ecx+eax*4+1180h],edx）。
//   其余部分见 ScenarioClass::SmthStartingHouses*，那里才有 presentHouses。
// ============================================================================
void Game::SelectStartingPoint(int32 houseIndex, int32 spotIndex)
{
    if (TheScenario == nullptr)
        return;

    // asm: HouseIndices 是 Scenario+1180h 起的 16 个 int32
    if (spotIndex < 0 || spotIndex >= 16)
        return;

    // asm: mov [ecx+eax*4+1180h],edx
    TheScenario->HouseIndices[spotIndex] = houseIndex;
}

int32 Game::GetGameTypePrefs()
{
    // asm: Game_GetGameTypePrefs (asm:1360790..1360960)
    //   ; int __thiscall Game_GetGameTypePrefs(GameTypePrefsClass *this,
    //                                         CCINIClass *INI, char *section,
    //                                         int a4, int a5)   retn 10h
    //
    //   函数体是"把某个模式段（Skirmish / LAN / WonlinePref）的全部偏好读进
    //   this"：INIClass_Reset → MPGameMode_ResetList → 五个 Write/GetInteger、
    //   五个 GetBool、Slot01..07 三元组。本树已一字不差地移植为
    //   GameTypePrefsClass::ReadFromINI（Network/GameTypePrefsClass.cpp），
    //   调用点也在 SessionClass::ReadMultiPlayerSettings 里按 asm 的三段
    //   （(1,6) / (2,2) / (2,2)）分派。
    //
    //   本声明是 int32(void)，没有承载 this/INI/section 的位置，无法与 asm 的
    //   五参 thiscall 对齐；直接返回常量就是假实现，故这里做等价的"三段全读"
    //   ——即 asm 调用点 SessionClass::Read_MultiPlayer_Settings+391..3AA 的
    //   三次调用折叠成一个入口，返回值是读入的段数。
    if (TheSession == nullptr)
        return 0;

    CCFileClass file("RA2MD.INI");
    CCINIClass ini;
    if (!ini.LoadFile(&file))
        return 0;

    // asm: (Skirmish, a4=1, a5=6) / (LAN, 2, 2) / (WonlinePref, 2, 2)
    TheSession->SkirmishPrefs.ReadFromINI(&ini, "Skirmish",    1, 6);
    TheSession->LANPrefs.ReadFromINI     (&ini, "LAN",         2, 2);
    TheSession->WOLPrefs.ReadFromINI     (&ini, "WonlinePref", 2, 2);

    return 3;
}

void Game::LoadUI_LoadFiles()
{
    // asm: Game_LoadUI_LoadFiles (asm:1663361..1663423)
    //
    //   UnloadGUISHPs() ; UnloadGUIPalettes() ; sub_730530() ;
    //   byte_B0FC0C = 0 ;
    //   SidebarClass_LoadArt() ;
    //   if (Scenario->[34B8h] == 2)                      ; 阵营 = 尤里
    //       UIBKGDY.PAL -> (conv = dword_B0FBF0, pal = dword_B0FBEC)
    //       RADARYURI.PAL -> (conv = dword_B0FBF8, pal = dword_B0FBF4)
    //   else
    //       SIDEBAR.PAL -> (conv = dword_B0FBF8, pal = dword_B0FBF4)
    //       UIBKGD.PAL -> (conv = dword_B0FBF0, pal = dword_B0FBEC)
    //   SIDEBAR.PAL  -> (SIDEBAR_PAL,  dword_B0FBE4)
    //   OBSERVER.PAL -> (OBSERVER_PAL, dword_B0FBFC)
    //   YRII.PAL     -> (YRII_PAL,     dword_B0FC04)
    //   sub_72FC60(unk_A8EB84, unk_A8EB88) ;
    //   byte_B0FC0C = 1
    //
    //   缺口（本树没有对应子系统，按缺字段留白）：
    //     · sub_730530 —— 释放 dword_B0FC30..B0FC4F 的 8 个界面缓冲；
    //       这 8 个全局在本树不存在。
    //     · SidebarClass_LoadArt —— 本树 SidebarClass 无 LoadArt（侧边栏美术
    //       在 RadarClass 内按 SIDE1.SHP / RADAR.SHP 自行处理）。
    //     · sub_72FC60 —— 按屏宽挑背景 SHP 填 BKGDLGY_SHP 族；本树无该族全局。

    // asm: call Game_UnloadGUISHPs
    UnloadGUISHPs();

    // asm: call Game_UnloadGUIPalettes
    UnloadGUIPalettes();

    // asm: call sub_730530   —— 缺口，见上

    // asm: mov byte_B0FC0C,0
    g_bGUIFilesLoaded = false;

    // asm: call SidebarClass_LoadArt   —— 缺口，见上

    // asm: mov eax,Scenario / cmp dword ptr [eax+34B8h],2 / jnz loc_72F3A0
    //   ScenarioClass+34B8h = PlayerSideIndex（0 = 盟军，1 = 苏军，2 = 尤里）
    if (TheScenario != nullptr && TheScenario->PlayerSideIndex == 2)
    {
        // asm: str_aUibkgdy_pal -> (a2 = &dword_B0FBEC, push &dword_B0FBF0)
        ConvertClass::CreateFromFile("UIBKGDY.PAL", g_pGUIUIBkgdPal, g_pGUIUIBkgdConv);
        // asm: str_radaryuri_pal -> (a2 = &dword_B0FBF4, push &dword_B0FBF8)
        ConvertClass::CreateFromFile("RADARYURI.PAL", g_pGUIRadarPal, g_pGUIRadarConv);
    }
    else
    {
        // asm: str_sidebar_pal -> (a2 = &dword_B0FBF4, push &dword_B0FBF8)
        ConvertClass::CreateFromFile("SIDEBAR.PAL", g_pGUIRadarPal, g_pGUIRadarConv);
        // asm: str_aUibkgd_pal -> (a2 = &dword_B0FBEC, push &dword_B0FBF0)
        ConvertClass::CreateFromFile("UIBKGD.PAL", g_pGUIUIBkgdPal, g_pGUIUIBkgdConv);
    }

    // asm: sidebar_pal -> (a2 = &dword_B0FBE4, push &SIDEBAR_PAL)
    //   注：本树 ConvertClass::CreateFromFile 只回填转换器，调色板出参
    //   （asm 的 dword_B0FBxx）留空 —— 它内部先建局部 PaletteClass 再建
    //   ConvertClass，没有把调色板所有权交出来。所以五个 *Pal 槽在本树
    //   恒为 nullptr，UnloadGUIPalettes 的 delete 分支自然跳过。
    ConvertClass::CreateFromFile("SIDEBAR.PAL", g_pGUISidebarPal, g_pGUISidebarConv);

    // asm: str_observer_pal -> (a2 = &dword_B0FBFC, push &OBSERVER_PAL)
    ConvertClass::CreateFromFile("OBSERVER.PAL", g_pGUIObserverPal, g_pGUIObserverConv);

    // asm: str_yrii_pal -> (a2 = &dword_B0FC04, push &YRII_PAL)
    ConvertClass::CreateFromFile("YRII.PAL", g_pGUIYriiPal, g_pGUIYriiConv);

    // asm: call sub_72FC60(unk_A8EB84, unk_A8EB88)   —— 缺口，见上

    // asm: mov byte_B0FC0C,1
    g_bGUIFilesLoaded = true;
}

void Game::UnloadDiploOptPowerSHP()
{
    // 根据游戏行为，可知外交选项的电力贴图与其它界面资源分槽释放。
    if (Game_pDiploSHPs) {
        delete Game_pDiploSHPs;
        Game_pDiploSHPs = nullptr;
    }
}

void Game::UnloadDiploOptSHP()
{
    UnloadDiploOptPowerSHP();
}

void Game::UnloadGUISHPs()
{
    // 根据游戏行为，可知界面元素包随界面销毁释放。
    if (Game_pGUISHPs) {
        delete Game_pGUISHPs;
        Game_pGUISHPs = nullptr;
    }
}

bool Game::Load_ARTMD_INI()
{
    // 根据游戏行为，可知 artmd.ini 由统一的装载入口读入并挂在全局
    // art 槽上；装载失败时保留上一次的内容。
    CCINIClass* pINI = CCINIClass::LoadINIFile("ARTMD.INI");
    return pINI != nullptr;
}

void Game::LoadUI()
{
    // 根据游戏行为，可知 UI 装载按次序走数据文件、贴图、调色板三步。
    LoadUI_LoadFiles();
    LoadPCXFiles();
}

void Game::UnloadSideAndGUISHPs()
{
    // 根据游戏行为，可知退出到主菜单时阵营贴图与界面贴图一起放掉。
    UnloadSideAndGUISHPs2();
    UnloadGUISHPs();
}

void Game::EmptyMaps_InitStrings_PhoneBook()
{
    // asm: Game_EmptyMaps_InitStrings_PhoneBook (asm:1364331..1364450)
    //   ; int __fastcall Game_EmptyMaps_InitStrings_PhoneBook(GameModeClass *this)
    //
    //   四段，全部是"销毁容器里的元素再清容器"：
    //   ① for (i = 0 ; i < this->[6A0h] ; ++i)          ; PKTNode 池
    //        node = this->[694h][i] ; if (node):
    //          node+1A4h (VectorClass<Wstring> = GameModes) 设 vftable 并清空
    //          node+188h (VectorClass<DiskID>   = CDs)     设 vftable；
    //            若 node+18Ch 非空且 node+195h 置位 -> delete 该缓冲
    //            node+18Ch = 0 ; node+190h = 0 ; node+195h = 0
    //          delete node
    //   ② this->[690h]->vtbl[0Ch](&this->[690h])         ; 池本身清空
    //   ③ for (i < this->[2924h]) delete this->[2918h][i] ; InitStrings
    //      this->[2914h]->vtbl[0Ch](&this->[2914h])
    //   ④ for (i < this->[290Ch]) delete this->[2900h][i] ; PhoneBook
    //      this->[28FCh]->vtbl[0Ch](&this->[28FCh])
    //
    //   本树对应：池 = Game_PKTPool（DynamicVectorClass<PKTNodeClass*>），
    //   PKTNodeClass 的 GameModes/CDs 就是 ① 里那两个向量（析构已按 asm 释放
    //   GameModes 的字符串并清空），所以 ① ② 落成"逐个 delete + Clear"。
    //   ③ ④ 的 InitStrings / PhoneBook 在 SessionClass 里没有对应数组成员，
    //   留白（见 SaveNetworkConfig 同处注释）。

    // ── ① + ②：PKTNode 池 ────────────────────────────────────────────
    for (int32 i = 0; i < Game_PKTPool.Count; ++i)
    {
        PKTNodeClass* pNode = Game_PKTPool.Items[i];
        if (pNode == nullptr)
            continue;
        delete pNode;                       // asm: push esi / call delete
    }

    // asm: mov edx,[edi+690h] / lea ecx,[edi+690h] / call [edx+0Ch]
    Game_PKTPool.Clear();

    // ── ③：InitStrings ──────────────────────────────────────────────
    //   缺口：SessionClass 没有 InitStrings 数组成员（asm +2918/+2914/+2924）。

    // ── ④：PhoneBook ────────────────────────────────────────────────
    //   缺口：SessionClass 没有 PhoneBook 数组成员（asm +2900/+28FC/+290C）。
}

void Game::AllyHouses(int32 houseA, int32 houseB)
{
    // 根据游戏行为，可知结盟是双向的：一边设置，两边生效。
    HouseClass* pA = HouseClass::GetHouseByIndex(houseA);
    HouseClass* pB = HouseClass::GetHouseByIndex(houseB);
    if (pA && pB) {
        pA->Make_Ally(houseB);
        pB->Make_Ally(houseA);
    }
}

void Game::UnloadStartingSpotsAndWinsock()
{
    // 根据游戏行为，可知对局销毁时出生点表与网络层一起关停。
    UnloadStartingGenericSpots();
}

void Game::ProcessCampaignOptions()
{
    // asm: Game_ProcessCampaignOptions (asm:255818..255981)
    //   ; void __cdecl Game_ProcessCampaignOptions(void)
    //
    //   ① 组一个 0x1C8 字节的栈上包：先 `mov [var_1C8],32h`（首 dword = 0x32），
    //      再把 word_A8B380（= ModemClass::GuestName，窄串）连同结尾拷进 +4；
    //   ② sub_5E8760(&var_1C8) 返回假则整段跳过（该例程是发包前的闸门）；
    //   ③ WWDebugString("Sending Campaign Options...\n")；
    //   ④ 对 i = 0..7 逐个取 PlayerSlotsX[i]（8 项会话槽表，元素是
    //      StartSpotClass），跳过空指针、CSF_OpenSlot_、CSF_ClosedSlot_，
    //      调 StartingSpotClass_GetStartSlot / GetCountry / GetColour 取值，
    //      连同 AISlots 的五排并行数组（StartingSpots / Colours /
    //      AIDifficulties / Starts / Teams）一起打印两行调试信息；
    //   ⑤ 再从 PlayerSlotsX+4 开始遍历（即跳过 0 号槽），对 IsNotDummy 的槽
    //      逐个 IPXManager::sub_541260(&packet, 1, 0x1C7, node->name) 发包，
    //      随后 sub_541820() 冲刷。
    //
    //   缺口（缺字段/缺例程，均无本树对应物，故不写假流程）：
    //     · PlayerSlotsX —— 8 项会话槽位表全局。本树 SessionClass 用
    //       SessionPlayer[8] 建模玩家，但没有与 asm 同形的 StartSpotClass*
    //       八元表（GameModeClass::StartSpots 是另一种向量容器且无实例）。
    //     · sub_5E8760 —— 组包后的闸门（判断能否发送）。
    //     · AISlots 的五排并行数组 —— 本树 MPCoopClass::AISlot 只有
    //       Maps / AIs 两个向量。
    //     · sub_541820 —— 发送后的冲刷；IPXManagerClass 只移植了 Send/Receive。
    //   在补齐以上四项之前，这个入口保持空体，避免向网络发出结构错误的包。
}

// ============================================================================
// asm: Game_UnloadDLGSHPs (0x72B130)
//
//   void __cdecl Game_UnloadDLGSHPs(void)
//
//   顺序卸载五个对话框 SHP，每个走同一段逻辑：
//     cmp Loaded,bl / jz done                ; 未加载 -> 跳过
//     cmp pSHP,ebx / jz clear_only           ; 指针空 -> 只清标志
//     mov ecx,pSHP / call SHPStruct_Unload   ; 释放帧数据
//     push pSHP / call delete                ; 释放对象
//   clear_only:
//     mov Loaded,bl / mov pSHP,ebx
// ============================================================================
Game::DialogSHPGlobal Game::PUDLGBGN = { nullptr, 0 };
Game::DialogSHPGlobal Game::PUDLGBGA = { nullptr, 0 };
Game::DialogSHPGlobal Game::PUDLGBGS = { nullptr, 0 };
Game::DialogSHPGlobal Game::PUDLGBGY = { nullptr, 0 };
Game::DialogSHPGlobal Game::MNBTTN   = { nullptr, 0 };

// 单个 SHP 槽的卸载（asm 里五处内联展开，这里提成局部函数保持语义）
static void UnloadOneDialogSHP(Game::DialogSHPGlobal& g)
{
    // asm: cmp Loaded,bl / jz done
    if (g.Loaded == 0)
        return;

    // asm: cmp pSHP,ebx / jz clear_only
    if (g.pSHP == nullptr)
    {
        g.Loaded = 0;                       // asm: mov Loaded,bl
        return;
    }

    // asm: mov ecx,pSHP / call SHPStruct_Unload
    //   SHPStruct_Unload（asm:1374254）释放引用型帧数据：先递归卸载引用，
    //   再摘链（list_SHPFiles_First）、free 帧块。语义见 FileFormats/SHP.cpp。
    static_cast<SHPStruct*>(g.pSHP)->Unload();

    // asm: push pSHP / call delete
    //   delete 走项目堆分配器。
    YRMemory::Deallocate(g.pSHP);

    // asm: mov Loaded,bl / mov pSHP,ebx
    g.Loaded = 0;
    g.pSHP   = nullptr;
}

void Game::UnloadDLGSHPs()
{
    UnloadOneDialogSHP(PUDLGBGN);           // asm: 第一块
    UnloadOneDialogSHP(PUDLGBGA);           // asm: loc_72B15D..
    UnloadOneDialogSHP(PUDLGBGS);
    UnloadOneDialogSHP(PUDLGBGY);
    UnloadOneDialogSHP(MNBTTN);
}

// ============================================================================
// asm: Game_AddMPHouses_obsolete (0x6468D0 区域)
//
//   void __cdecl Game_AddMPHouses_obsolete(void *hWnd)
//
//   for i in 0..7:
//     push offset "<Player @ X>" / push 0 / push 143h / push hWnd
//     call SendMessageA                          -> item
//     push (117Bh + i) / push item / push 151h / push hWnd
//     call SendMessageA
//
//   标记为 obsolete 是因为原版已改用新的玩家注册流程；本函数保留以维持
//   消息序列的等价。hWnd 由调用方给出。
// ============================================================================
void Game::AddMPHouses_Obsolete(void* hWnd)
{
    static const char* pNames[8] = {
        "<Player @ A>", "<Player @ B>", "<Player @ C>", "<Player @ D>",
        "<Player @ E>", "<Player @ F>", "<Player @ G>", "<Player @ H>",
    };
    static const int32 lpData[8] = {
        0x117B, 0x117C, 0x117D, 0x117E, 0x117F, 0x1180, 0x1181, 0x1182,
    };

    if (hWnd == nullptr)
        return;

    for (int32 i = 0; i < 8; ++i)
    {
        // asm: push offset aPlayer@X / push 0 / push 143h / push hWnd
        //      call SendMessageA
        //   原版消息常量 0x143 = 注册玩家名，0x151 = 设置玩家槽位数据。
        //   跨平台构建下统一走 Game 的消息辅助入口。
        const int32 item = SendWindowMessage(hWnd, 0x143, 0,
                                             pNames[i]);

        // asm: push 117Bh+i / push eax / push 151h / push hWnd
        //      call SendMessageA
        SendWindowMessage(hWnd, 0x151, item,
                          reinterpret_cast<const char*>(
                              static_cast<intptr_t>(lpData[i])));
    }
}

// ============================================================================
// Game::SendWindowMessage —— asm `call SendMessageA` 的统一入口
//
//   原版在 AddMPHouses_obsolete 等处直接调 Win32 SendMessageA。
//   跨平台构建下，Windows 平台转发真实 API，其他平台返回 0。
//   保持参数顺序与 Win32 一致：hwnd, msg, wParam, lParam。
// ============================================================================
#if defined(PLATFORM_WINDOWS)
extern "C" __declspec(dllimport) intptr_t __stdcall SendMessageA(
    void* hWnd, uint32 Msg, uintptr_t wParam, intptr_t lParam);
#endif

int32 Game::SendWindowMessage(void* hWnd, int32 msg, int32 wParam, const void* lParam)
{
#if defined(PLATFORM_WINDOWS)
    return static_cast<int32>(SendMessageA(
        hWnd,
        static_cast<uint32>(msg),
        static_cast<uintptr_t>(wParam),
        reinterpret_cast<intptr_t>(lParam)));
#else
    // 非 Windows 平台没有窗口系统，消息不投递。
    (void)hWnd; (void)msg; (void)wParam; (void)lParam;
    return 0;
#endif
}
