#pragma once

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Math/Timer.h>
#include <Scenario/ScenarioClass.h>
#include <Houses/HouseClass.h>
#include <Map/MapClass.h>
#include <ctime>

// Forward declarations
class MapClass;
class RulesClass;

// ============================================================================
// Global game state
// ============================================================================
extern int32 GameInFocus;
extern int32 GameInProgress;
extern int32 GamePaused;
extern int32 GameSpeed;
extern int32 CurrentFrame;
extern int32 FrameCounter;
extern int32 ScreenWidth;
extern int32 ScreenHeight;
extern bool  GameActive;
extern bool  GameInitDone;
extern bool  GameMapLoaded;
extern bool  GameSoundOn;
extern bool  GameMusicOn;
extern bool  GameNetworkOn;
extern bool  GameIsMultiplayer;
extern bool  GameIsSkirmish;
extern bool  GameIsCampaign;
extern bool  GameIsLoading;
extern bool  GameIsSaving;
extern bool  GameIsQuitting;
extern bool  GameIsComplete;
extern bool  GameHasFocus;
extern int32 GameMixState;
extern int32 GameState;
extern int32 ScenarioInit;
extern bool  SessionOver;
extern bool  SessionActive;
extern bool  MainLoopActive;
extern int32 WantedGameSpeed;
extern bool  GameInEndGame;

// ============================================================================
// Game initialization functions
// ============================================================================

// Main initialization sequence
void Init_Game();
void Init_Game_First();
void Init_Game_Load();

// Subsystem initialization
void Init_Random();
void Init_FileSystem();
void Init_MixFiles();
void Init_Graphics();
void Init_Keyboard();
void Init_Mouse();
void Init_Sound();
void Init_Network();
void Init_GameMode();
void Init_Scenario();
void Init_AI();
void Init_Interfaces();
void Init_Animations();
void Init_Weapons();
void Init_Warheads();
void Init_BulletTypes();
void Init_Particles();
void Init_Terrain();
void Init_Overlay();
void Init_Smudge();
void Init_Infantry();
void Init_Vehicles();
void Init_Aircraft();
void Init_Buildings();
void Init_Houses();
void Init_SuperWeapons();
void Init_Sides();
void Init_Countries();
void Init_Campaign();
void Init_Triggers();
void Init_Teams();
void Init_Scripts();
void Init_TaskForces();
void Init_Tags();
void Init_Tiberiums();
void Init_RadSites();
void Init_Tubes();
void Init_Base();
void Init_TacticalMap();
void Init_Spotlight();
void Init_Shroud();
void Init_Radar();
void Init_UI();
void Init_Palette();
void Init_Fonts();
void Init_Cursors();
void Init_IngameUI();
void Init_MessageList();
void Init_RadarEvents();
void Init_IonStorm();
void Init_LightSources();
void Init_FogOfWar();
void Init_EVAMessages();
void Init_Player();

// Main game loop
void Main_Game();
void Main_Loop();
void Main_Game_Update();
void Main_Game_Render();
void Main_Game_ProcessInput();
void Main_Game_Network();
void Main_Game_AI();
void Main_Game_CheckEndGame();

// Frame update
void Update_Game();
void Game_Update_Frame();
void Game_Update_Objects();
void Game_Update_Houses();
void Game_Update_Map();
void Game_Update_UI();
void Game_Update_Sound();
void Game_Update_Animation();
void Game_Update_Input();
void Game_Update_Network();
void Game_Update_AI();
void Game_Update_Lighting();
void Game_Update_Crates();
void Game_Update_Radar();
void Game_Update_Shroud();
void Game_Update_Events();
void Game_Update_SuperWeapons();
void Game_Update_Tiberium();
void Game_Update_Veins();
void Game_Update_Ice();
void Game_Update_IonStorm();
void Game_Update_EVA();

// Shutdown
void Shutdown_Game();
void Shutdown_Graphics();
void Shutdown_Sound();
void Shutdown_Network();
void Shutdown_FileSystem();
void Shutdown_AI();
void Shutdown_UI();
void Shutdown_MixFiles();

// Game mode
void Set_Game_Mode(int32 mode);
void Init_GameMode_None();
void Init_GameMode_SinglePlayer();
void Init_GameMode_Skirmish();
void Init_GameMode_Multiplayer();
void Init_GameMode_Campaign();
void Init_GameMode_Network();
void Init_GameMode_LAN();
void Init_GameMode_WOL();
void Init_GameMode_Modem();
void Init_GameMode_Serial();

// Game flow
void Game_Start();
void Game_End();
void Game_Pause();
void Game_Resume();
void Game_Loading();
void Game_Saving();
void Game_LoadingScreen();
void Game_SavingScreen();
void Game_Briefing();
void Game_ScoreScreen();
void Game_Credits();
void Game_MainMenu();
void Game_MapSelect();
void Game_GameOptions();
void Game_MultiplayerLobby();
void Game_MultiplayerSetup();
void Game_SkirmishSetup();
void Game_NetworkLobby();
void Game_NetworkSetup();
void Game_ModemSetup();
void Game_SerialSetup();
void Game_WOLSetup();
void Game_LoadGame();
void Game_SaveGame();
void Game_NewGame();
void Game_ContinueGame();
void Game_RestartGame();
void Game_QuitGame();
void Game_ExitToMenu();