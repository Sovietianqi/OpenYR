#pragma once

#include "Core/Definitions.h"
#include <Containers/DynamicVectorClass.h>

// ═══════════════════════════════════════════════════════════════════════════
// Externs.h — global extern declarations for every major game system
//
//  This header mirrors the original game's pattern of declaring all
//  singleton pointers as extern so they are accessible from any
//  translation unit that includes this header.
//
//  The actual definitions live in Globals.cpp.
// ═══════════════════════════════════════════════════════════════════════════

// ── TypeClass headers (required for Complete types in extern declarations) ─

#include <Abstract/InfantryTypeClass.h>
#include <Abstract/UnitTypeClass.h>
#include <Abstract/AircraftTypeClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Abstract/VoxelAnimTypeClass.h>
#include <Abstract/TerrainTypeClass.h>
#include <Abstract/SmudgeTypeClass.h>
#include <Abstract/OverlayTypeClass.h>
#include <Animations/AnimTypeClass.h>
#include <Combat/BulletTypeClass.h>
#include <Combat/WarheadTypeClass.h>
#include <Combat/WeaponTypeClass.h>
#include <Particles/ParticleTypeClass.h>
#include <Particles/ParticleSystemTypeClass.h>
#include <SW/SuperWeaponTypeClass.h>
#include <Special/TiberiumClass.h>

// ── Instance class headers (required for Complete types in extern declarations) ─

#include <Abstract/InfantryClass.h>
#include <Abstract/UnitClass.h>
#include <Abstract/AircraftClass.h>
#include <Abstract/BuildingClass.h>
#include <Abstract/VoxelAnimClass.h>
#include <Abstract/TerrainClass.h>
#include <Abstract/SmudgeClass.h>
#include <Abstract/OverlayClass.h>
#include <Animations/AnimClass.h>
#include <Combat/BulletClass.h>
#include <Particles/ParticleClass.h>
#include <Particles/ParticleSystemClass.h>
#include <SW/SuperClass.h>
#include <AI/TagClass.h>
#include <AI/TriggerClass.h>
#include <AI/TeamClass.h>
#include <AI/ScriptClass.h>
#include <AI/TaskForceClass.h>
#include <AI/AITriggerClass.h>
#include <Special/WaveClass.h>
#include <Special/RadSiteClass.h>

// ── Forward declarations ──────────────────────────────────────────────────

class Game;
class MapClass;
class ScenarioClass;
class RulesClass;
class DisplayClass;
class TacticalClass;
class GScreenClass;
class SidebarClass;
class MouseClass;
class RadarClass;
class ConvertClass;
class NetworkingClass;
class SessionClass;
class VocClass;
class ThemeClass;
class InfantryTypeClass;
class UnitTypeClass;
class AircraftTypeClass;
class BuildingTypeClass;
class AnimTypeClass;
class BulletTypeClass;
class WarheadTypeClass;
class WeaponTypeClass;
class ParticleTypeClass;
class ParticleSystemTypeClass;
class VoxelAnimTypeClass;
class TerrainTypeClass;
class SmudgeTypeClass;
class OverlayTypeClass;
class SuperWeaponTypeClass;
class InfantryClass;
class UnitClass;
class AircraftClass;
class BuildingClass;
class AnimClass;
class BulletClass;
class ParticleClass;
class ParticleSystemClass;
class VoxelAnimClass;
class TerrainClass;
class SmudgeClass;
class OverlayClass;
class SuperClass;
class TagClass;
class TriggerClass;
class TeamClass;
class ScriptClass;
class TaskForceClass;
class AITriggerClass;
class WaveClass;
class RadSiteClass;
class TiberiumClass;
class HouseClass;
class HouseTypeClass;

// ── Core game systems ─────────────────────────────────────────────────────

extern Game*             TheGame;
extern MapClass*         TheMap;
extern ScenarioClass*    TheScenario;
extern RulesClass*       TheRules;

// ── Rendering systems ─────────────────────────────────────────────────────

extern DisplayClass*     TheDisplay;
extern TacticalClass*    TheTactical;
extern GScreenClass*     TheGScreen;
extern SidebarClass*     TheSidebar;
extern MouseClass*       TheMouse;
extern RadarClass*       TheRadar;
extern ConvertClass*     TheConvert;

// ── Network systems ───────────────────────────────────────────────────────

extern NetworkingClass*  TheNetworking;
extern SessionClass*     TheSession;

// ── Audio systems ─────────────────────────────────────────────────────────

extern VocClass*         TheVoc;
extern ThemeClass*       TheTheme;
extern bool              g_bMusicEnabled;
extern bool              g_bSoundEnabled;
extern int               g_SoundVolume;
extern int               g_MusicVolume;
extern int               g_VoiceVolume;

// ── House / Player globals ────────────────────────────────────────────────

extern int               ThePlayerIndex;
extern int               TheHouseCount;

// ── Object type arrays ────────────────────────────────────────────────────
// (static members declared in respective class headers; defined in Globals.cpp)

// ── Object instance arrays ────────────────────────────────────────────────
// (static members declared in respective class headers; defined in Globals.cpp)

// ── House arrays ──────────────────────────────────────────────────────────

extern HouseClass*       Houses[MAX_HOUSES];
extern HouseTypeClass*   HouseTypes[MAX_HOUSES];

// ── Global counters ───────────────────────────────────────────────────────

extern int               g_InfantryCount;
extern int               g_UnitCount;
extern int               g_AircraftCount;
extern int               g_BuildingCount;
extern int               g_AnimCount;
extern int               g_BulletCount;
extern int               g_ParticleCount;
extern int               g_TerrainCount;
extern int               g_SmudgeCount;
extern int               g_OverlayCount;

// ── Global timers ─────────────────────────────────────────────────────────

extern int               g_GameTimer;
extern int               g_MissionTimer;
extern int               g_ReinforcementTimer;
extern int               g_AITriggerTimer;
extern int               g_SuperWeaponTimer;
extern int               g_BuildTimer;

// ── Global flags ──────────────────────────────────────────────────────────

extern bool              g_bGameStarted;
extern bool              g_bGameEnded;
extern bool              g_bInGameLoop;
extern bool              g_bMapLoaded;
extern bool              g_bRulesLoaded;
extern bool              g_bScenarioLoaded;
extern bool              g_bMultiplayerGame;
extern bool              g_bCampaignGame;
extern bool              g_bSkirmishGame;
extern bool              g_bReplayGame;
extern bool              g_bTutorialMode;
extern bool              g_bMissionSuccess;
extern bool              g_bMissionFailed;
extern bool              g_bNoBuildings;
extern bool              g_bBaseDestroyed;
extern bool              g_bAllDestroyed;
extern bool              g_bGamePaused;
extern bool              g_bSkipMapTheaterCheck;
extern bool              g_bAITriggersEnabled;
extern bool              g_bTeamTriggersEnabled;
extern bool              g_bGameSpeedChanged;

// ── Map / Cell globals ────────────────────────────────────────────────────

extern int               g_MapWidth;
extern int               g_MapHeight;
extern int               g_CellCount;
extern TheaterType       g_Theater;
extern bool              g_bTiberiumEnabled;
extern bool              g_bVeinsEnabled;
extern bool              g_bIceEnabled;

// ── Misc globals ──────────────────────────────────────────────────────────

extern bool              g_bDebugMode;
extern bool              g_bNoMusic;
extern bool              g_bNoSound;
extern bool              g_bFullScreen;
extern int               g_ScreenWidth;
extern int               g_ScreenHeight;
extern int               g_ColorDepth;
extern bool              g_bVSync;
extern bool              g_bAllowHiResModes;

// ── File paths ────────────────────────────────────────────────────────────

extern const char*       g_GameDirectory;
extern const char*       g_ConfigDirectory;
extern const char*       g_SaveDirectory;

// ── Scenario-specific globals ─────────────────────────────────────────────

extern char              g_ScenarioName[260];
extern char              g_ScenarioDescription[512];
extern char              g_MapFileName[260];
extern int               g_ScenarioPlayerCount;
extern int               g_ScenarioMaxPlayers;
extern bool              g_bScenarioIsCampaign;
extern bool              g_bScenarioIsMultiplayer;

// ── Score / Stats globals ─────────────────────────────────────────────────

extern int               g_PlayerScore[MAX_PLAYERS];
extern int               g_PlayerKills[MAX_PLAYERS];
extern int               g_PlayerLosses[MAX_PLAYERS];
extern int               g_PlayerBuildings[MAX_PLAYERS];
extern int               g_PlayerCreditsSpent[MAX_PLAYERS];
extern int               g_PlayerPower[MAX_PLAYERS];
extern int               g_PlayerDrain[MAX_PLAYERS];

// ── Money globals ─────────────────────────────────────────────────────────

extern int               g_StartingCredits;
extern int               g_OreValue;
extern int               g_GemValue;

// ── Combat globals ────────────────────────────────────────────────────────

extern bool              g_bCombatEnabled;
extern bool              g_bFriendlyFire;
extern bool              g_bCanDeployMCV;
extern int               g_MaxBuildings;
extern int               g_MaxUnits;
extern int               g_MaxInfantry;
extern int               g_MaxAircraft;

// ── SuperWeapon global arrays ─────────────────────────────────────────────

extern SuperClass*       g_SuperWeapons[MAX_HOUSES][static_cast<int>(SuperWeaponType::Count)];
extern SuperClass*       g_ShowTimersList[MAX_HOUSES][static_cast<int>(SuperWeaponType::Count)];
extern int               g_SuperWeaponCount[MAX_HOUSES];

// ── Waypoint globals ──────────────────────────────────────────────────────

extern CellStruct        g_Waypoints[256];
extern int               g_WaypointCount;
extern char              g_WaypointNames[256][32];

// ── Minimap / Radar globals ───────────────────────────────────────────────

extern bool              g_bRadarEnabled;
extern bool              g_bRadarInitialized;
extern int               g_RadarZoomLevel;

// ── Input globals ─────────────────────────────────────────────────────────

extern bool              g_bKeyboardEnabled;
extern bool              g_bMouseEnabled;
extern int               g_MouseX;
extern int               g_MouseY;
extern bool              g_bLeftButtonDown;
extern bool              g_bRightButtonDown;

// ── UI globals ────────────────────────────────────────────────────────────

extern bool              g_bSidebarVisible;
extern bool              g_bMinimapVisible;
extern bool              g_bTooltipsEnabled;
extern bool              g_bScrollLocked;
extern int               g_ScrollRate;
extern int               g_ScrollDelay;

// ── INI file path globals ─────────────────────────────────────────────────

extern const char*       g_RulesINIFile;
extern const char*       g_ArtINIFile;
extern const char*       g_AIINIFile;
extern const char*       g_SoundINIFile;
extern const char*       g_ThemeINIFile;
extern const char*       g_BattleINIFile;
extern const char*       g_MissionINIFile;
extern const char*       g_MapINIFile;

// ── Version globals ───────────────────────────────────────────────────────

extern const int         g_GameVersionMajor;
extern const int         g_GameVersionMinor;
extern const int         g_GameVersionBuild;
extern const char*       g_GameVersionString;
extern const char*       g_GameName;

// ── Language / Locale ─────────────────────────────────────────────────────

extern int               g_LanguageID;
extern const char*       g_StringTableFile;

// ── Game profile / Options ────────────────────────────────────────────────

extern int               g_ScrollMethod;
extern bool              g_bRightClickScroll;
extern bool              g_bHardwareCursor;
extern bool              g_bShowTeamColors;
extern bool              g_bShowHealthBars;
extern bool              g_bShowWaypointLines;
extern bool              g_bShowBuildingOutlines;
extern bool              g_bMoveToAttack;
extern bool              g_bAutoDeploy;
extern bool              g_bQueueMoves;
extern bool              g_bBaseDefense;
extern int               g_UnitResponse;
extern int               g_RepairSellMode;

// ── Network sync globals ──────────────────────────────────────────────────

extern uint32            g_NetworkCRC;
extern bool              g_bNetworkSyncCheck;
extern int               g_NetworkLatency;
extern int               g_NetworkMaxLatency;
extern bool              g_bNetworkGameStarted;