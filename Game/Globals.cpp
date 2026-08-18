#include "Game/Externs.h"

#include "Core/Definitions.h"

// ═══════════════════════════════════════════════════════════════════════════
// Globals.cpp — global variable definitions for all game systems
//
//  Every variable declared as extern in Externs.h is defined (and
//  initialized) here.  This is the single point of truth for global
//  state initialization.
//
//  The file is organised into the following sections:
//
//  1.  Core game system singletons (Game, Map, Scenario, Rules)
//  2.  Rendering system singletons (Display, Tactical, GScreen, ...)
//  3.  Network system singletons
//  4.  House / player globals
//  5.  Misc runtime globals (debug, screen, audio)
//  6.  File path globals
//  7.  Audio system globals
//  8.  Object type-class static Array pointers
//  9.  Object instance-class static Array pointers
//  10. House arrays
//  11. Global counters
//  12. Global timers
//  13. Global flags
//  14. Map / cell globals
//  15. Scenario-specific globals
//  16. Score / stats globals
//  17. Money globals
//  18. Combat globals
//  19. SuperWeapon global arrays
//  20. Waypoint globals
//  21. Minimap / radar globals
//  22. Input globals
//  23. UI globals
//  24. INI file path globals
//  25. Version globals
//  26. Language / locale globals
//  27. Game profile / options globals
//  28. Network sync globals
//  29. Network player globals
//  30. Render globals
//  31. Misc runtime globals
//  32. Initialization helper functions
//
//  The initialization helpers (InitGlobals, ResetGameCounters,
//  ResetScoreStats, ClearWaypoints, ShutdownGlobals) provide a
//  controlled lifecycle for the global state, ensuring that the
//  game starts from a known baseline and that scenario transitions
//  properly reset per-game state without leaking data from a
//  previous match.
// ═══════════════════════════════════════════════════════════════════════════

// ── Core game systems ─────────────────────────────────────────────────────

Game*             TheGame             = nullptr;
MapClass*         TheMap              = nullptr;
ScenarioClass*    TheScenario         = nullptr;
RulesClass*       TheRules            = nullptr;

// ── Rendering systems ─────────────────────────────────────────────────────

DisplayClass*     TheDisplay          = nullptr;
TacticalClass*    TheTactical         = nullptr;
GScreenClass*     TheGScreen          = nullptr;
SidebarClass*     TheSidebar          = nullptr;
MouseClass*       TheMouse            = nullptr;
RadarClass*       TheRadar            = nullptr;
ConvertClass*     TheConvert          = nullptr;

// ── Network systems ───────────────────────────────────────────────────────

NetworkingClass*  TheNetworking       = nullptr;
SessionClass*     TheSession          = nullptr;

// ── House / Player globals ────────────────────────────────────────────────

int               ThePlayerIndex      = 0;
int               TheHouseCount       = 0;

// ── Misc globals ──────────────────────────────────────────────────────────

bool              g_bDebugMode        = false;
bool              g_bNoMusic          = false;
bool              g_bNoSound          = false;
bool              g_bFullScreen       = true;
int               g_ScreenWidth       = 800;
int               g_ScreenHeight      = 600;
int               g_ColorDepth        = 16;
bool              g_bVSync            = true;
bool              g_bAllowHiResModes  = false;

// ── File paths ────────────────────────────────────────────────────────────

const char*       g_GameDirectory     = "./";
const char*       g_ConfigDirectory   = "./";
const char*       g_SaveDirectory     = "./Save/";

// ── Audio system globals ──────────────────────────────────────────────────

VocClass*         TheVoc              = nullptr;
ThemeClass*       TheTheme            = nullptr;
bool              g_bMusicEnabled     = true;
bool              g_bSoundEnabled     = true;
int               g_SoundVolume       = 100;
int               g_MusicVolume       = 100;
int               g_VoiceVolume       = 100;

// ── Object type arrays ────────────────────────────────────────────────────

DynamicVectorClass<ParticleTypeClass*>*    ParticleTypeClass::Array    = nullptr;
DynamicVectorClass<TiberiumClass*>*        TiberiumClass::Array        = nullptr;

// ── Object instance arrays ────────────────────────────────────────────────

DynamicVectorClass<BulletClass*>*      BulletClass::Array      = nullptr;
DynamicVectorClass<ParticleSystemClass*>* ParticleSystemClass::Array = nullptr;
DynamicVectorClass<ScriptClass*>*      ScriptClass::Array      = nullptr;
DynamicVectorClass<WaveClass*>*        WaveClass::Array        = nullptr;
DynamicVectorClass<RadSiteClass*>*     RadSiteClass::Array     = nullptr;

// ── House arrays ──────────────────────────────────────────────────────────

HouseClass*       Houses[MAX_HOUSES]          = { nullptr };
HouseTypeClass*   HouseTypes[MAX_HOUSES]      = { nullptr };

// ── Global counters ───────────────────────────────────────────────────────

int               g_InfantryCount     = 0;
int               g_UnitCount         = 0;
int               g_AircraftCount     = 0;
int               g_BuildingCount     = 0;
int               g_AnimCount         = 0;
int               g_BulletCount       = 0;
int               g_ParticleCount     = 0;
int               g_TerrainCount      = 0;
int               g_SmudgeCount       = 0;
int               g_OverlayCount      = 0;

// ── Global timers ─────────────────────────────────────────────────────────

int               g_GameTimer          = 0;
int               g_MissionTimer       = 0;
int               g_ReinforcementTimer = 0;
int               g_AITriggerTimer     = 0;
int               g_SuperWeaponTimer   = 0;
int               g_BuildTimer         = 0;

// ── Global flags ──────────────────────────────────────────────────────────

bool              g_bGameStarted       = false;
bool              g_bGameEnded         = false;
bool              g_bInGameLoop        = false;
bool              g_bMapLoaded         = false;
bool              g_bRulesLoaded       = false;
bool              g_bScenarioLoaded    = false;
bool              g_bMultiplayerGame   = false;
bool              g_bCampaignGame      = false;
bool              g_bSkirmishGame      = false;
bool              g_bReplayGame        = false;
bool              g_bTutorialMode      = false;
bool              g_bMissionSuccess    = false;
bool              g_bMissionFailed     = false;
bool              g_bNoBuildings       = false;
bool              g_bBaseDestroyed     = false;
bool              g_bAllDestroyed      = false;
bool              g_bGamePaused        = false;
bool              g_bSkipMapTheaterCheck = false;
bool              g_bAITriggersEnabled = true;
bool              g_bTeamTriggersEnabled = true;
bool              g_bGameSpeedChanged  = false;

// ── Map / Cell globals ────────────────────────────────────────────────────

int               g_MapWidth           = 0;
int               g_MapHeight          = 0;
int               g_CellCount          = 0;
TheaterType       g_Theater            = TheaterType::Temperate;
bool              g_bTiberiumEnabled   = true;
bool              g_bVeinsEnabled      = false;
bool              g_bIceEnabled        = false;

// ── Scenario-specific globals ─────────────────────────────────────────────

char              g_ScenarioName[260]      = {0};
char              g_ScenarioDescription[512] = {0};
char              g_MapFileName[260]       = {0};
int               g_ScenarioPlayerCount    = 0;
int               g_ScenarioMaxPlayers     = 0;
bool              g_bScenarioIsCampaign    = false;
bool              g_bScenarioIsMultiplayer = false;

// ── Score / Stats globals ─────────────────────────────────────────────────

int               g_PlayerScore[MAX_PLAYERS]   = {0};
int               g_PlayerKills[MAX_PLAYERS]   = {0};
int               g_PlayerLosses[MAX_PLAYERS]  = {0};
int               g_PlayerBuildings[MAX_PLAYERS] = {0};
int               g_PlayerCreditsSpent[MAX_PLAYERS] = {0};
int               g_PlayerPower[MAX_PLAYERS]   = {0};
int               g_PlayerDrain[MAX_PLAYERS]   = {0};

// ── Money globals ─────────────────────────────────────────────────────────

int               g_StartingCredits    = 10000;
int               g_OreValue           = 25;
int               g_GemValue           = 50;

// ── Combat globals ────────────────────────────────────────────────────────

bool              g_bCombatEnabled     = true;
bool              g_bFriendlyFire      = false;
bool              g_bCanDeployMCV      = true;
int               g_MaxBuildings       = 0;
int               g_MaxUnits           = 0;
int               g_MaxInfantry        = 0;
int               g_MaxAircraft        = 0;

// ── SuperWeapon global arrays ─────────────────────────────────────────────

SuperClass*       g_SuperWeapons[MAX_HOUSES][static_cast<int>(SuperWeaponType::Count)] = {
    { nullptr }
};
SuperClass*       g_ShowTimersList[MAX_HOUSES][static_cast<int>(SuperWeaponType::Count)] = {
    { nullptr }
};
int               g_SuperWeaponCount[MAX_HOUSES] = {0};

// ── Waypoint globals ──────────────────────────────────────────────────────

CellStruct        g_Waypoints[256]     = { CellStruct(0, 0) };
int               g_WaypointCount      = 0;
char              g_WaypointNames[256][32] = { {0} };

// ── Minimap / Radar globals ───────────────────────────────────────────────

bool              g_bRadarEnabled      = true;
bool              g_bRadarInitialized  = false;
int               g_RadarZoomLevel     = 0;

// ── Input globals ─────────────────────────────────────────────────────────

bool              g_bKeyboardEnabled   = true;
bool              g_bMouseEnabled      = true;
int               g_MouseX             = 0;
int               g_MouseY             = 0;
bool              g_bLeftButtonDown    = false;
bool              g_bRightButtonDown   = false;

// ── UI globals ────────────────────────────────────────────────────────────

bool              g_bSidebarVisible    = true;
bool              g_bMinimapVisible    = true;
bool              g_bTooltipsEnabled   = true;
bool              g_bScrollLocked      = false;
int               g_ScrollRate         = 4;
int               g_ScrollDelay        = 15;

// ── INI file path globals ─────────────────────────────────────────────────

const char*       g_RulesINIFile       = "rulesmd.ini";
const char*       g_ArtINIFile         = "artmd.ini";
const char*       g_AIINIFile          = "aimd.ini";
const char*       g_SoundINIFile       = "soundmd.ini";
const char*       g_ThemeINIFile       = "thememd.ini";
const char*       g_BattleINIFile      = "battlemd.ini";
const char*       g_MissionINIFile     = "missionmd.ini";
const char*       g_MapINIFile         = "map.ini";

// ── Version globals ───────────────────────────────────────────────────────

const int         g_GameVersionMajor   = 1;
const int         g_GameVersionMinor   = 1;
const int         g_GameVersionBuild   = 0;
const char*       g_GameVersionString  = "1.1.0";
const char*       g_GameName           = "Command & Conquer: Yuri's Revenge";

// ── Language / Locale ─────────────────────────────────────────────────────

int               g_LanguageID         = 0;  // 0 = English
const char*       g_StringTableFile    = "ra2md.csf";

// ── Game profile / Options ────────────────────────────────────────────────

int               g_ScrollMethod       = 0;  // 0 = edge, 1 = middle click
bool              g_bRightClickScroll  = false;
bool              g_bHardwareCursor    = true;
bool              g_bShowTeamColors    = true;
bool              g_bShowHealthBars    = true;
bool              g_bShowWaypointLines = false;
bool              g_bShowBuildingOutlines = true;
bool              g_bMoveToAttack      = true;
bool              g_bAutoDeploy        = true;
bool              g_bQueueMoves        = false;
bool              g_bBaseDefense       = false;
int               g_UnitResponse       = 0;  // 0 = confirm, 1 = no confirm
int               g_RepairSellMode     = 0;  // 0 = normal, 1 = hotkey

// ── Network sync globals ──────────────────────────────────────────────────

uint32            g_NetworkCRC         = 0;
bool              g_bNetworkSyncCheck  = true;
int               g_NetworkLatency     = 0;
int               g_NetworkMaxLatency  = 200;
bool              g_bNetworkGameStarted = false;

// ── Network player globals ────────────────────────────────────────────────

int               g_NetworkPlayerID    = 0;
bool              g_bNetworkHost       = false;
int               g_NetworkFrameRate   = 15;
int               g_NetworkMaxPlayers  = 8;
bool              g_bNetworkDedicated  = false;

// ── Render globals ────────────────────────────────────────────────────────

int               g_RenderWidth        = 800;
int               g_RenderHeight       = 600;
bool              g_bWindowedMode      = false;
bool              g_bStretchMovies     = true;
int               g_DrawRate           = 1;

// ── Misc runtime globals ──────────────────────────────────────────────────

bool              g_bFirstUpdate       = true;
int               g_GameTickRate       = 15;
bool              g_bAllowScreenshots  = true;
bool              g_bAutoSaveEnabled   = true;
int               g_AutoSaveInterval   = 300;  // 5 minutes in seconds

// ═══════════════════════════════════════════════════════════════════════════
// Initialization helper functions
//
//  These functions provide a controlled lifecycle for the global state.
//  They are called by the InitList system during startup and by the
//  scenario loader when transitioning between maps.
// ═══════════════════════════════════════════════════════════════════════════

#include <cstring>

// Forward declarations of initialization helpers (defined below).
void ResetGameCounters();
void ResetScoreStats();
void ClearWaypoints();
void ClearSuperWeapons();

// -----------------------------------------------------------------------------
// InitGlobals - Set all globals to their default startup values
//
// Called once during game initialisation (before any subsystem is created).
// This function ensures that the global state is in a known baseline
// regardless of any previous initialisation order issues.
// -----------------------------------------------------------------------------
void InitGlobals()
{
    // Core singletons are created by their respective init functions.
    TheGame     = nullptr;
    TheMap      = nullptr;
    TheScenario = nullptr;
    TheRules    = nullptr;

    // Rendering singletons.
    TheDisplay  = nullptr;
    TheTactical = nullptr;
    TheGScreen  = nullptr;
    TheSidebar  = nullptr;
    TheMouse    = nullptr;
    TheRadar    = nullptr;
    TheConvert  = nullptr;

    // Network singletons.
    TheNetworking = nullptr;
    TheSession    = nullptr;

    // Audio singletons.
    TheVoc   = nullptr;
    TheTheme = nullptr;

    // Player globals.
    ThePlayerIndex = 0;
    TheHouseCount  = 0;

    // Debug / display flags.
    g_bDebugMode       = false;
    g_bNoMusic         = false;
    g_bNoSound         = false;
    g_bFullScreen      = true;
    g_ScreenWidth      = 800;
    g_ScreenHeight     = 600;
    g_ColorDepth       = 16;
    g_bVSync           = true;
    g_bAllowHiResModes = false;

    // Audio defaults.
    g_bMusicEnabled = true;
    g_bSoundEnabled = true;
    g_SoundVolume   = 100;
    g_MusicVolume   = 100;
    g_VoiceVolume   = 100;

    // Reset all per-game state.
    ResetGameCounters();
    ResetScoreStats();
    ClearWaypoints();

    // Clear scenario strings.
    std::memset(g_ScenarioName, 0, sizeof(g_ScenarioName));
    std::memset(g_ScenarioDescription, 0, sizeof(g_ScenarioDescription));
    std::memset(g_MapFileName, 0, sizeof(g_MapFileName));
    g_ScenarioPlayerCount    = 0;
    g_ScenarioMaxPlayers     = 0;
    g_bScenarioIsCampaign    = false;
    g_bScenarioIsMultiplayer = false;

    // Money defaults.
    g_StartingCredits = 10000;
    g_OreValue        = 25;
    g_GemValue        = 50;

    // Combat defaults.
    g_bCombatEnabled = true;
    g_bFriendlyFire  = false;
    g_bCanDeployMCV  = true;
    g_MaxBuildings   = 0;
    g_MaxUnits       = 0;
    g_MaxInfantry    = 0;
    g_MaxAircraft    = 0;

    // Map defaults.
    g_MapWidth         = 0;
    g_MapHeight        = 0;
    g_CellCount        = 0;
    g_Theater          = TheaterType::Temperate;
    g_bTiberiumEnabled = true;
    g_bVeinsEnabled    = false;
    g_bIceEnabled      = false;

    // Game flags.
    g_bGameStarted         = false;
    g_bGameEnded           = false;
    g_bInGameLoop          = false;
    g_bMapLoaded           = false;
    g_bRulesLoaded         = false;
    g_bScenarioLoaded      = false;
    g_bMultiplayerGame     = false;
    g_bCampaignGame        = false;
    g_bSkirmishGame        = false;
    g_bReplayGame          = false;
    g_bTutorialMode        = false;
    g_bMissionSuccess      = false;
    g_bMissionFailed       = false;
    g_bNoBuildings         = false;
    g_bBaseDestroyed       = false;
    g_bAllDestroyed        = false;
    g_bGamePaused          = false;
    g_bSkipMapTheaterCheck = false;
    g_bAITriggersEnabled   = true;
    g_bTeamTriggersEnabled = true;
    g_bGameSpeedChanged    = false;

    // Timers.
    g_GameTimer          = 0;
    g_MissionTimer       = 0;
    g_ReinforcementTimer = 0;
    g_AITriggerTimer     = 0;
    g_SuperWeaponTimer   = 0;
    g_BuildTimer         = 0;

    // Radar / minimap.
    g_bRadarEnabled     = true;
    g_bRadarInitialized = false;
    g_RadarZoomLevel    = 0;

    // Input.
    g_bKeyboardEnabled = true;
    g_bMouseEnabled    = true;
    g_MouseX           = 0;
    g_MouseY           = 0;
    g_bLeftButtonDown  = false;
    g_bRightButtonDown = false;

    // UI.
    g_bSidebarVisible       = true;
    g_bMinimapVisible       = true;
    g_bTooltipsEnabled      = true;
    g_bScrollLocked         = false;
    g_ScrollRate            = 4;
    g_ScrollDelay           = 15;

    // Render.
    g_RenderWidth     = 800;
    g_RenderHeight    = 600;
    g_bWindowedMode   = false;
    g_bStretchMovies  = true;
    g_DrawRate        = 1;

    // Network.
    g_NetworkCRC         = 0;
    g_bNetworkSyncCheck  = true;
    g_NetworkLatency     = 0;
    g_NetworkMaxLatency  = 200;
    g_bNetworkGameStarted = false;
    g_NetworkPlayerID    = 0;
    g_bNetworkHost       = false;
    g_NetworkFrameRate   = 15;
    g_NetworkMaxPlayers  = 8;
    g_bNetworkDedicated  = false;

    // Misc.
    g_bFirstUpdate      = true;
    g_GameTickRate      = 15;
    g_bAllowScreenshots = true;
    g_bAutoSaveEnabled  = true;
    g_AutoSaveInterval  = 300;
}

// -----------------------------------------------------------------------------
// ResetGameCounters - Zero all per-game object counters and timers
//
// Called when a new scenario starts to ensure that counters from a previous
// game do not bleed into the new one.
// -----------------------------------------------------------------------------
void ResetGameCounters()
{
    g_InfantryCount = 0;
    g_UnitCount     = 0;
    g_AircraftCount = 0;
    g_BuildingCount = 0;
    g_AnimCount     = 0;
    g_BulletCount   = 0;
    g_ParticleCount = 0;
    g_TerrainCount  = 0;
    g_SmudgeCount   = 0;
    g_OverlayCount  = 0;

    g_GameTimer          = 0;
    g_MissionTimer       = 0;
    g_ReinforcementTimer = 0;
    g_AITriggerTimer     = 0;
    g_SuperWeaponTimer   = 0;
    g_BuildTimer         = 0;
}

// -----------------------------------------------------------------------------
// ResetScoreStats - Zero all player score and statistics arrays
//
// Called at the start of each game to clear any leftover score data.
// -----------------------------------------------------------------------------
void ResetScoreStats()
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        g_PlayerScore[i]         = 0;
        g_PlayerKills[i]         = 0;
        g_PlayerLosses[i]        = 0;
        g_PlayerBuildings[i]     = 0;
        g_PlayerCreditsSpent[i]  = 0;
        g_PlayerPower[i]         = 0;
        g_PlayerDrain[i]         = 0;
    }
}

// -----------------------------------------------------------------------------
// ClearWaypoints - Clear all waypoint data
//
// Waypoints are loaded from the scenario file; this function resets the
// waypoint table before a new scenario is loaded.
// -----------------------------------------------------------------------------
void ClearWaypoints()
{
    for (int i = 0; i < 256; ++i) {
        g_Waypoints[i] = CellStruct(0, 0);
        std::memset(g_WaypointNames[i], 0, sizeof(g_WaypointNames[i]));
    }
    g_WaypointCount = 0;
}

// -----------------------------------------------------------------------------
// ClearSuperWeapons - Null out the SuperWeapon instance arrays
//
// Called before a new scenario populates the SuperWeapon tables.
// -----------------------------------------------------------------------------
void ClearSuperWeapons()
{
    int swCount = static_cast<int>(SuperWeaponType::Count);
    for (int h = 0; h < MAX_HOUSES; ++h) {
        for (int s = 0; s < swCount; ++s) {
            g_SuperWeapons[h][s]     = nullptr;
            g_ShowTimersList[h][s]   = nullptr;
        }
        g_SuperWeaponCount[h] = 0;
    }
}

// -----------------------------------------------------------------------------
// ShutdownGlobals - Release all global singleton pointers
//
// Called during game shutdown to null out all singleton pointers.  The
// actual object destruction is handled by each subsystem's shutdown
// function; this function simply clears the references to prevent
// dangling pointer access after teardown.
// -----------------------------------------------------------------------------
void ShutdownGlobals()
{
    TheGame     = nullptr;
    TheMap      = nullptr;
    TheScenario = nullptr;
    TheRules    = nullptr;

    TheDisplay  = nullptr;
    TheTactical = nullptr;
    TheGScreen  = nullptr;
    TheSidebar  = nullptr;
    TheMouse    = nullptr;
    TheRadar    = nullptr;
    TheConvert  = nullptr;

    TheNetworking = nullptr;
    TheSession    = nullptr;

    TheVoc   = nullptr;
    TheTheme = nullptr;

    // Clear house arrays.
    for (int i = 0; i < MAX_HOUSES; ++i) {
        Houses[i]     = nullptr;
        HouseTypes[i] = nullptr;
    }

    // Clear super weapons.
    ClearSuperWeapons();

    // Reset flags.
    g_bGameStarted      = false;
    g_bGameEnded        = false;
    g_bInGameLoop       = false;
    g_bMapLoaded        = false;
    g_bRulesLoaded      = false;
    g_bScenarioLoaded   = false;
    g_bNetworkGameStarted = false;

    // Clear waypoints.
    ClearWaypoints();

    // Reset counters.
    ResetGameCounters();
    ResetScoreStats();
}

// -----------------------------------------------------------------------------
// GetGameVersionString - Return a formatted version string
//
// Combines the major, minor, and build version numbers into a single
// human-readable string.  Used by the main menu and multiplayer lobby.
// -----------------------------------------------------------------------------
const char* GetGameVersionString()
{
    return g_GameVersionString;
}

// -----------------------------------------------------------------------------
// IsMultiplayerSession - Check whether the current game is a network session
//
// Returns true if the game is a LAN or Internet multiplayer game.  Skirmish
// games against AI opponents are not considered multiplayer for this check.
// -----------------------------------------------------------------------------
bool IsMultiplayerSession()
{
    return g_bMultiplayerGame && !g_bSkirmishGame;
}

// -----------------------------------------------------------------------------
// IsSinglePlayerSession - Check whether the current game is single-player
//
// Returns true for campaign and skirmish games (both are single-player in
// the sense that they do not involve network communication).
// -----------------------------------------------------------------------------
bool IsSinglePlayerSession()
{
    return g_bCampaignGame || g_bSkirmishGame;
}

// -----------------------------------------------------------------------------
// GetCurrentTheaterName - Return the string name of the current theater
//
// Maps the TheaterType enum to its string representation for file path
// construction and debug output.
// -----------------------------------------------------------------------------
const char* GetCurrentTheaterName()
{
    switch (g_Theater) {
        case TheaterType::Temperate: return "TEMPERATE";
        case TheaterType::Snow:      return "SNOW";
        case TheaterType::Urban:     return "URBAN";
        case TheaterType::Lunar:     return "LUNAR";
        case TheaterType::Desert:    return "DESERT";
        case TheaterType::NewUrban:  return "NEWURBAN";
        default:                     return "TEMPERATE";
    }
}