#include "Game/Game.h"

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Core/Memory.h"

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
// Game_Sleep
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
// Game_Update
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