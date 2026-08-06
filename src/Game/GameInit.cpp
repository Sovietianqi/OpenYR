#include <Game/GameInit.h>
#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <Houses/HouseClass.h>
#include <Houses/HouseTypeClass.h>
#include <Map/MapClass.h>
#include <Scenario/ScenarioClass.h>
#include <Rules/RulesClass.h>
#include <Math/Timer.h>
#include <Rendering/GScreenClass.h>
#include <Rendering/Surface.h>
#include <Game/Externs.h>

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdio>

// ============================================================================
// GameInit.cpp - Game initialization and main loop implementation
// ============================================================================
// In the original game, these functions reside in the main executable and
// coordinate the entire game lifecycle: initialization, main loop, and shutdown.
//
// Key addresses in the original binary:
//   Init_Game:       0x47C6D0
//   Init_Random:     0x523A70
//   Init_FileSystem: 0x5B0E70
//   Init_MixFiles:   0x5B0E60
//   Main_Game:       0x47C5A0
//   Main_Loop:       0x47C5B0
//   Shutdown_Game:   0x47C6E0
// ============================================================================

// ============================================================================
// Global game state definitions
// ============================================================================
int32 GameInFocus = 0;
int32 GameInProgress = 0;
int32 GamePaused = 0;
int32 GameSpeed = 0;
int32 CurrentFrame = 0;
int32 FrameCounter = 0;
int32 ScreenWidth = 640;
int32 ScreenHeight = 480;
bool  GameActive = false;
bool  GameInitDone = false;
bool  GameMapLoaded = false;
bool  GameSoundOn = true;
bool  GameMusicOn = true;
bool  GameNetworkOn = false;
bool  GameIsMultiplayer = false;
bool  GameIsSkirmish = false;
bool  GameIsCampaign = false;
bool  GameIsLoading = false;
bool  GameIsSaving = false;
bool  GameIsQuitting = false;
bool  GameIsComplete = false;
bool  GameHasFocus = true;
int32 GameMixState = 0;
int32 GameState = 0;
int32 ScenarioInit = 0;
bool  SessionOver = false;
bool  SessionActive = false;
bool  MainLoopActive = true;
int32 WantedGameSpeed = 0;
bool  GameInEndGame = false;

// ============================================================================
// Internal state
// ============================================================================
namespace {
    bool g_GameObjectsInitialized = false;
    bool g_FileSystemInitialized = false;
    bool g_MixFilesLoaded = false;
    bool g_GraphicsInitialized = false;
    bool g_SoundInitialized = false;
    bool g_NetworkInitialized = false;
    bool g_AIInitialized = false;
    bool g_GameModeInitialized = false;
}

// ============================================================================
// Init_Game - Main initialization sequence
// ============================================================================

void Init_Game()
{
    if (GameInitDone) return;

    GameActive = true;
    GameInProgress = true;
    GameInFocus = 1;
    CurrentFrame = 0;
    FrameCounter = 0;
    GamePaused = 0;
    GameIsComplete = false;
    GameIsQuitting = false;
    GameInEndGame = false;
    GameMapLoaded = false;
    GameIsLoading = false;
    GameIsSaving = false;

    // Step 1: Seed random number generator
    Init_Random();

    // Step 2: Initialize file system
    Init_FileSystem();

    // Step 3: Load MIX archives
    Init_MixFiles();

    // Step 4: Initialize graphics subsystem
    Init_Graphics();

    // Step 5: Initialize input devices
    Init_Keyboard();
    Init_Mouse();

    // Step 6: Initialize audio
    Init_Sound();

    // Step 7: Initialize networking
    Init_Network();

    // Step 8: Initialize game mode
    Init_GameMode();

    // Step 9: Create game object singletons
    Init_Game_First();

    // Step 10: Initialize scenario
    Init_Scenario();

    // Step 11: Initialize AI
    Init_AI();

    // Step 12: Initialize COM interfaces
    Init_Interfaces();

    // Step 13: Initialize type data from rules
    Init_Animations();
    Init_Weapons();
    Init_Warheads();
    Init_BulletTypes();
    Init_Particles();
    Init_Terrain();
    Init_Overlay();
    Init_Smudge();
    Init_Infantry();
    Init_Vehicles();
    Init_Aircraft();
    Init_Buildings();
    Init_Houses();
    Init_SuperWeapons();
    Init_Sides();
    Init_Countries();
    Init_Campaign();
    Init_Triggers();
    Init_Teams();
    Init_Scripts();
    Init_TaskForces();
    Init_Tags();
    Init_Tiberiums();
    Init_RadSites();
    Init_Tubes();
    Init_Base();

    // Step 14: Initialize rendering systems
    Init_Player();
    Init_Palette();
    Init_Fonts();
    Init_Cursors();
    Init_UI();
    Init_IngameUI();
    Init_MessageList();
    Init_RadarEvents();
    Init_TacticalMap();
    Init_Spotlight();
    Init_Shroud();
    Init_Radar();
    Init_IonStorm();
    Init_LightSources();
    Init_FogOfWar();
    Init_EVAMessages();

    GameInitDone = true;
}

// ============================================================================
// Init_Game_First - Create singleton instances
// ============================================================================

void Init_Game_First()
{
    if (g_GameObjectsInitialized) return;

    // Create the map singleton
    if (!MapClass::Instance) {
        MapClass::Instance = new MapClass();
    }

    // Create the scenario singleton
    if (!ScenarioClass::Instance) {
        ScenarioClass::Instance = new ScenarioClass();
    }

    // Create the rules singleton
    if (!RulesClass::Instance) {
        RulesClass::Instance = new RulesClass();
    }

    // Create the house observer
    if (!HouseClass::Observer) {
        HouseClass::Observer = new HouseClass(nullptr);
        HouseClass::Observer->IsObserver = true;
    }

    g_GameObjectsInitialized = true;
}

// ============================================================================
// Init_Game_Load - Load game data after singletons are created
// ============================================================================

void Init_Game_Load()
{
    if (!g_GameObjectsInitialized) {
        Init_Game_First();
    }

    // Load rules from INI files
    // In the original game, CCINIClass loads the INI file from disk
    // and is then passed to RulesClass::Read_File for parsing.
    if (RulesClass::Instance) {
        // Create an INI file reader for each rules file
        // CCINIClass rulesINI("rulesmd.ini");
        // RulesClass::Instance->Read_File(&rulesINI);
        // Similarly for artmd.ini, soundmd.ini, aimd.ini
    }
}

// ============================================================================
// Init_Random - Seed the random number generator
// ============================================================================

void Init_Random()
{
    // Use the current time as the seed for the standard C random generator
    uint32 seed = static_cast<uint32>(time(nullptr));

    // XOR with process-specific data for better entropy
    seed ^= static_cast<uint32>(reinterpret_cast<uintptr_t>(&seed));

    srand(seed);

    // Also seed the ScenarioClass randomizer if it exists
    if (ScenarioClass::Instance) {
        ScenarioClass::Instance->Random.Seed = static_cast<int32>(seed);
    }
}

// ============================================================================
// Init_FileSystem - Initialize the virtual file system
// ============================================================================

void Init_FileSystem()
{
    if (g_FileSystemInitialized) return;

    // In the original game, this initializes the CCFileClass system,
    // sets up the current directory, and initializes the MIX file manager.
    // The game uses a virtual file system where files can come from:
    // 1. Physical files on disk
    // 2. MIX archive files
    // 3. Memory-mapped resources

    GameMixState = 0;
    g_FileSystemInitialized = true;
}

// ============================================================================
// Init_MixFiles - Load game MIX archives
// ============================================================================

void Init_MixFiles()
{
    if (g_MixFilesLoaded) return;

    // In the original game, MIX files are loaded in a specific order
    // to ensure proper asset priority. Files loaded later override earlier ones.
    //
    // Load order for Yuri's Revenge:
    //   1. expandmd01.mix - Core game data
    //   2. expandmd02.mix - Additional game data
    //   3. expandmd03.mix - More game data
    //   4. language.mix - Language-specific resources
    //   5. multiplayer.mix - Multiplayer-specific resources
    //   6. cache.mix - Cached resources
    //   7. local.mix - Local modifications
    //
    // Each MIX file is a container that stores multiple files
    // with optional encryption (RA2 uses a Blowfish-based scheme).

    GameMixState = 1;
    g_MixFilesLoaded = true;
}

// ============================================================================
// Init_Graphics - Initialize the graphics subsystem
// ============================================================================

void Init_Graphics()
{
    if (g_GraphicsInitialized) return;

    // In the original game, this initializes:
    // - DirectDraw or Direct3D surface (via GScreenClass)
    // - Display mode (640x480 default)
    // - Color palette
    // - Back buffer and off-screen surfaces
    // - Hardware cursor
    // - VSync settings

    ScreenWidth = 640;
    ScreenHeight = 480;

    g_GraphicsInitialized = true;
}

// ============================================================================
// Init_Keyboard - Initialize keyboard input
// ============================================================================

void Init_Keyboard()
{
    // In the original game, this initializes:
    // - DirectInput keyboard device
    // - Key state buffer (256 keys)
    // - Key repeat settings
    // - Keyboard message queue
    // - Hotkey system
    // - Chat input buffer
}

// ============================================================================
// Init_Mouse - Initialize mouse input
// ============================================================================

void Init_Mouse()
{
    // In the original game, this initializes:
    // - DirectInput mouse device
    // - Mouse cursor rendering
    // - Mouse capture area
    // - Scroll speed settings
    // - Mouse button state tracking
    // - In-game cursor shapes (select, move, attack, etc.)
}

// ============================================================================
// Init_Sound - Initialize the audio subsystem
// ============================================================================

void Init_Sound()
{
    if (g_SoundInitialized) return;

    // In the original game, this initializes:
    // - Miles Sound System (MSS) or DirectSound
    // - Audio channels (voc, theme, ambient)
    // - Sound effect cache
    // - Music playback system
    // - Volume levels
    // - EVA voice system

    GameSoundOn = true;
    GameMusicOn = true;
    g_SoundInitialized = true;
}

// ============================================================================
// Init_Network - Initialize the networking subsystem
// ============================================================================

void Init_Network()
{
    if (g_NetworkInitialized) return;

    // In the original game, this initializes:
    // - IPX/SPX protocol stack (legacy)
    // - UDP/IP protocol stack (modern)
    // - Westwood Online (WOL) interface
    // - Network event queue
    // - Session management
    // - Latency compensation

    GameNetworkOn = false;
    GameIsMultiplayer = false;
    g_NetworkInitialized = true;
}

// ============================================================================
// Init_GameMode - Determine and initialize the game mode
// ============================================================================

void Init_GameMode()
{
    if (g_GameModeInitialized) return;

    // Determine the game mode based on startup parameters
    if (GameIsCampaign) {
        Init_GameMode_Campaign();
    } else if (GameIsSkirmish) {
        Init_GameMode_Skirmish();
    } else if (GameIsMultiplayer) {
        if (GameNetworkOn) {
            Init_GameMode_Network();
        } else {
            Init_GameMode_Multiplayer();
        }
    } else {
        Init_GameMode_SinglePlayer();
    }

    g_GameModeInitialized = true;
}

// ============================================================================
// Init_Scenario - Initialize the scenario system
// ============================================================================

void Init_Scenario()
{
    if (ScenarioClass::Instance) {
        ScenarioClass::Instance->Init();
    }
    ScenarioInit = 1;
}

// ============================================================================
// Init_AI - Initialize the AI subsystem
// ============================================================================

void Init_AI()
{
    if (g_AIInitialized) return;

    // In the original game, this initializes:
    // - AI trigger system
    // - AI team management
    // - AI script execution
    // - AI base building logic
    // - AI attack planning
    // - AI difficulty-based behavior modifiers

    // Initialize AI for all non-human houses
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse && !pHouse->IsHumanPlayer) {
            // AI houses get initialized with default IQ level
            if (pHouse->IQLevel == 0) {
                pHouse->IQLevel = 1; // Default AI IQ level
            }
        }
    }

    g_AIInitialized = true;
}

// ============================================================================
// Init_Interfaces - Initialize COM interfaces
// ============================================================================

void Init_Interfaces()
{
    // In the original game, this initializes:
    // - COM runtime
    // - IUnknown implementations
    // - Interface registration
    // - Connection point containers
}

// ============================================================================
// Init_Animations - Initialize animation type data
// ============================================================================

void Init_Animations()
{
    // In the original game, this loads:
    // - [Animations] section from rulesmd.ini
    // - Animation SHP frame data
    // - Animation rate and loop settings
    // - Damage-based animation mappings
}

// ============================================================================
// Init_Weapons - Initialize weapon type data
// ============================================================================

void Init_Weapons()
{
    // In the original game, this loads:
    // - [WeaponTypes] section from rulesmd.ini
    // - Weapon damage, ROF, range, projectile settings
    // - Warhead assignments
    // - Report (sound effect) assignments
    // - Laser/bullet/particle projectile types
}

// ============================================================================
// Init_Warheads - Initialize warhead type data
// ============================================================================

void Init_Warheads()
{
    // In the original game, this loads:
    // - [Warheads] section from rulesmd.ini
    // - Verses (damage percentages against armor types)
    // - Special warhead effects (temporal, radiation, etc.)
    // - AnimList and particle systems
    // - Shake and flash effects
}

// ============================================================================
// Init_BulletTypes - Initialize bullet type data
// ============================================================================

void Init_BulletTypes()
{
    // In the original game, this loads:
    // - [BulletTypes] section from rulesmd.ini
    // - Bullet speed, arcing, and image data
    // - Trail and particle effects
    // - Inviso and AA settings
}

// ============================================================================
// Init_Particles - Initialize particle system type data
// ============================================================================

void Init_Particles()
{
    // In the original game, this loads:
    // - [Particles] and [ParticleSystems] sections
    // - Particle behaviors (gravity, wind, etc.)
    // - Particle rendering parameters
    // - Particle system duration and spawn rate
}

// ============================================================================
// Init_Terrain - Initialize terrain type data
// ============================================================================

void Init_Terrain()
{
    // In the original game, this loads:
    // - [TerrainTypes] section from rulesmd.ini
    // - Terrain object SHP data
    // - Terrain size and placement restrictions
    // - Theater-specific terrain variations
}

// ============================================================================
// Init_Overlay - Initialize overlay type data
// ============================================================================

void Init_Overlay()
{
    // In the original game, this loads:
    // - [OverlayTypes] section from rulesmd.ini
    // - Overlay SHP data (walls, pavement, etc.)
    // - Overlay damage states
    // - Overlay theater-specific graphics
}

// ============================================================================
// Init_Smudge - Initialize smudge type data
// ============================================================================

void Init_Smudge()
{
    // In the original game, this loads:
    // - [SmudgeTypes] section from rulesmd.ini
    // - Smudge SHP data (craters, scorch marks)
    // - Smudge size and crater settings
}

// ============================================================================
// Init_Infantry - Initialize infantry type data
// ============================================================================

void Init_Infantry()
{
    // In the original game, this loads:
    // - [InfantryTypes] section from rulesmd.ini
    // - Infantry SHP sequences (walk, fire, die, etc.)
    // - Infantry voice and sound assignments
    // - Infantry movement zone and speed
    // - Infantry weapons and abilities
}

// ============================================================================
// Init_Vehicles - Initialize vehicle type data
// ============================================================================

void Init_Vehicles()
{
    // In the original game, this loads:
    // - [VehicleTypes] section from rulesmd.ini
    // - Vehicle VXL/HVA model data
    // - Vehicle movement zone, speed, and turn rate
    // - Vehicle weapons, turret settings, and abilities
    // - Vehicle size and passenger capacity
}

// ============================================================================
// Init_Aircraft - Initialize aircraft type data
// ============================================================================

void Init_Aircraft()
{
    // In the original game, this loads:
    // - [AircraftTypes] section from rulesmd.ini
    // - Aircraft VXL/HVA model data
    // - Aircraft flight characteristics
    // - Aircraft weapons and abilities
    // - Aircraft landing and docking behavior
}

// ============================================================================
// Init_Buildings - Initialize building type data
// ============================================================================

void Init_Buildings()
{
    // In the original game, this loads:
    // - [BuildingTypes] section from rulesmd.ini
    // - Building SHP art data
    // - Building size, foundation, and placement
    // - Building power, tech level, and prerequisites
    // - Building weapons, animations, and abilities
    // - Building exit coord and factory settings
}

// ============================================================================
// Init_Houses - Initialize house types and instances
// ============================================================================

void Init_Houses()
{
    // Clear all house arrays
    for (int32 i = 0; i < HouseClass::MaxHouses; ++i) {
        HouseClass::Array[i] = nullptr;
    }
    HouseClass::ArrayCount = 0;
    HouseClass::pCurrentPlayer = nullptr;
    HouseClass::Player = nullptr;

    // The observer is already created in Init_Game_First
    if (!HouseClass::Observer) {
        HouseClass::Observer = new HouseClass(nullptr);
        HouseClass::Observer->IsObserver = true;
    }
}

// ============================================================================
// Init_SuperWeapons - Initialize super weapon type data
// ============================================================================

void Init_SuperWeapons()
{
    // In the original game, this loads:
    // - [SuperWeaponTypes] section from rulesmd.ini
    // - Super weapon type, recharge time, and requirements
    // - Super weapon UI (cameo, sidebar position)
    // - Super weapon effects (nuke, lightning, etc.)
}

// ============================================================================
// Init_Sides - Initialize side data
// ============================================================================

void Init_Sides()
{
    // In the original game, this loads:
    // - [Sides] section from rulesmd.ini
    // - Side names, logos, and loading screen data
    // - Side-specific UI colors
    // - Side-specific EVA voices
}

// ============================================================================
// Init_Countries - Initialize country data
// ============================================================================

void Init_Countries()
{
    // In the original game, this loads:
    // - [Countries] section from rulesmd.ini
    // - Country names, flags, and parent sides
    // - Country-specific unit bonuses
    // - Country-specific veteran units
    // - Multiplayer country availability
}

// ============================================================================
// Init_Campaign - Initialize campaign data
// ============================================================================

void Init_Campaign()
{
    // In the original game, this loads:
    // - Campaign progression data
    // - Mission list
    // - Campaign-specific briefings
    // - Campaign save/load state
}

// ============================================================================
// Init_Triggers - Initialize trigger system
// ============================================================================

void Init_Triggers()
{
    // In the original game, this loads:
    // - [Triggers] section from map INI
    // - Trigger events, actions, and conditions
    // - Trigger state (enabled, disabled, triggered)
    // - Global trigger variables
}

// ============================================================================
// Init_Teams - Initialize team type data
// ============================================================================

void Init_Teams()
{
    // In the original game, this loads:
    // - [TeamTypes] section from map INI
    // - Team composition (units, counts)
    // - Team script assignment
    // - Team house ownership
    // - Team auto-create settings
}

// ============================================================================
// Init_Scripts - Initialize script type data
// ============================================================================

void Init_Scripts()
{
    // In the original game, this loads:
    // - [ScriptTypes] section from map INI
    // - Script action sequences
    // - Script conditions and jumps
}

// ============================================================================
// Init_TaskForces - Initialize task force data
// ============================================================================

void Init_TaskForces()
{
    // In the original game, this loads:
    // - [TaskForces] section from map INI
    // - Task force unit composition
    // - Task force group assignments
}

// ============================================================================
// Init_Tags - Initialize tag data
// ============================================================================

void Init_Tags()
{
    // In the original game, this loads:
    // - [Tags] section from map INI
    // - Tag-trigger associations
    // - Tag repeat and delay settings
}

// ============================================================================
// Init_Tiberiums - Initialize tiberium type data
// ============================================================================

void Init_Tiberiums()
{
    // In the original game, this loads:
    // - [Tiberiums] section from rulesmd.ini
    // - Tiberium growth rate and spread settings
    // - Tiberium value per unit
    // - Tiberium damage and power settings
    // - Tiberium image and animation data
}

// ============================================================================
// Init_RadSites - Initialize radiation site data
// ============================================================================

void Init_RadSites()
{
    // In the original game, this initializes:
    // - Radiation site list
    // - Radiation damage settings
    // - Radiation spread behavior
    // - Radiation decay timer
}

// ============================================================================
// Init_Tubes - Initialize tube/teleport data
// ============================================================================

void Init_Tubes()
{
    // In the original game, this initializes:
    // - Tunnel network topology
    // - Tube entrance/exit pairs
    // - Tube traversal logic
}

// ============================================================================
// Init_Base - Initialize base class
// ============================================================================

void Init_Base()
{
    // In the original game, this initializes:
    // - Base node system
    // - Base building placement grid
    // - Base defense priority calculations
}

// ============================================================================
// Init_Player - Setup the player's house
// ============================================================================

void Init_Player()
{
    // Set the first non-observer, non-civilian house as the player
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse && !pHouse->IsObserver && !pHouse->IsCivilians) {
            if (pHouse->IsHumanPlayer || pHouse->PlayerControl) {
                HouseClass::Player = pHouse;
                HouseClass::pCurrentPlayer = pHouse;
                pHouse->CurrentPlayer = true;
                break;
            }
        }
    }

    // If no human player found, set the first available house
    if (!HouseClass::Player) {
        for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
            HouseClass* pHouse = HouseClass::Array[i];
            if (pHouse && !pHouse->IsObserver) {
                HouseClass::Player = pHouse;
                HouseClass::pCurrentPlayer = pHouse;
                pHouse->CurrentPlayer = true;
                break;
            }
        }
    }
}

// ============================================================================
// Init_Palette - Initialize color palette
// ============================================================================

void Init_Palette()
{
    // In the original game, this initializes:
    // - Game color palette (256 colors)
    // - Palette animation for special effects
    // - Palette remapping for unit colors
    // - Lighting and tint tables
}

// ============================================================================
// Init_Fonts - Initialize font rendering
// ============================================================================

void Init_Fonts()
{
    // In the original game, this initializes:
    // - Font SHP data loading
    // - Font size and spacing
    // - Font color mapping
    // - String table (CSF) loading
}

// ============================================================================
// Init_Cursors - Initialize mouse cursors
// ============================================================================

void Init_Cursors()
{
    // In the original game, this initializes:
    // - Cursor SHP loading
    // - Cursor shape mapping (normal, select, move, attack, etc.)
    // - Cursor hotspot positioning
    // - Animated cursor support
}

// ============================================================================
// Init_UI - Initialize user interface
// ============================================================================

void Init_UI()
{
    // In the original game, this initializes:
    // - Main menu UI
    // - Game options UI
    // - Loading screen UI
    // - Score screen UI
    // - Credits screen UI
}

// ============================================================================
// Init_IngameUI - Initialize in-game UI elements
// ============================================================================

void Init_IngameUI()
{
    // In the original game, this initializes:
    // - Sidebar (building and unit production)
    // - Power bar indicator
    // - Credits display
    // - Radar minimap
    // - Tab bar (buildings, infantry, vehicles)
    // - Super weapon sidebar buttons
    // - Tooltip system
}

// ============================================================================
// Init_MessageList - Initialize message list
// ============================================================================

void Init_MessageList()
{
    // In the original game, this initializes:
    // - In-game message display
    // - EVA message queue
    // - Text message fade and scroll
    // - Event notification display
}

// ============================================================================
// Init_RadarEvents - Initialize radar event system
// ============================================================================

void Init_RadarEvents()
{
    // In the original game, this initializes:
    // - Radar event list
    // - Radar event colors and shapes
    // - Radar event duration and fade
    // - Radar event sound effects
}

// ============================================================================
// Init_TacticalMap - Initialize tactical map
// ============================================================================

void Init_TacticalMap()
{
    // In the original game, this initializes:
    // - Tactical map display
    // - Map viewport and scrolling
    // - Zoom level
    // - Cell rendering pipeline
}

// ============================================================================
// Init_Spotlight - Initialize spotlight
// ============================================================================

void Init_Spotlight()
{
    // In the original game, this initializes:
    // - Spotlight behavior (reveal area around cursor)
    // - Spotlight radius
    // - Spotlight fade behavior
}

// ============================================================================
// Init_Shroud - Initialize shroud system
// ============================================================================

void Init_Shroud()
{
    // In the original game, this initializes:
    // - Shroud renderer
    // - Shroud cell state tracking
    // - Shroud edge rendering
    // - Gap generator effects
    // - Spy plane reveal
}

// ============================================================================
// Init_Radar - Initialize radar minimap
// ============================================================================

void Init_Radar()
{
    // In the original game, this initializes:
    // - Radar minimap rendering
    // - Radar color mapping
    // - Radar blip tracking
    // - Radar event display
    // - Radar spy/satellite behavior
}

// ============================================================================
// Init_IonStorm - Initialize ion storm system
// ============================================================================

void Init_IonStorm()
{
    // In the original game, this initializes:
    // - Ion storm randomizer
    // - Ion storm duration and intensity
    // - Ion storm visual effects
    // - Ion storm radar jamming
    // - Ion storm power disruption
}

// ============================================================================
// Init_LightSources - Initialize light source system
// ============================================================================

void Init_LightSources()
{
    // In the original game, this initializes:
    // - Light source list
    // - Light source types (point, directional)
    // - Light source rendering
    // - Light source intensity and color
}

// ============================================================================
// Init_FogOfWar - Initialize fog of war
// ============================================================================

void Init_FogOfWar()
{
    // In the original game, this initializes:
    // - Fog of war renderer
    // - Fog cell state tracking
    // - Fog edge rendering
    // - Fog reveal radius per unit type
}

// ============================================================================
// Init_EVAMessages - Initialize EVA message system
// ============================================================================

void Init_EVAMessages()
{
    // In the original game, this initializes:
    // - EVA voice message queue
    // - EVA trigger conditions
    // - EVA message priority system
    // - EVA message cooldown timers
}

// ============================================================================
// Main_Game - Main game loop entry point
// ============================================================================

void Main_Game()
{
    // Ensure the game is initialized
    if (!GameInitDone) {
        Init_Game();
    }

    MainLoopActive = true;
    GameIsQuitting = false;

    while (MainLoopActive && !GameIsQuitting) {
        Main_Loop();

        if (GameIsQuitting) {
            Shutdown_Game();
            break;
        }
    }
}

// ============================================================================
// Main_Loop - Per-frame game loop iteration
// ============================================================================

void Main_Loop()
{
    // Process input events (keyboard, mouse, network)
    Main_Game_ProcessInput();

    // Skip game logic updates if the game is paused
    if (GamePaused > 0) {
        return;
    }

    // Update game logic
    Main_Game_Update();

    // Process network messages
    Main_Game_Network();

    // Update AI decision-making
    Main_Game_AI();

    // Render the frame
    Main_Game_Render();

    // Check for end-game conditions
    Main_Game_CheckEndGame();

    // Increment frame counters
    ++CurrentFrame;
    ++FrameCounter;
}

// ============================================================================
// Main_Game_Update - Update all game subsystems
// ============================================================================

void Main_Game_Update()
{
    Game_Update_Frame();
    Game_Update_Objects();
    Game_Update_Houses();
    Game_Update_Map();
    Game_Update_UI();
    Game_Update_Sound();
    Game_Update_Animation();
    Game_Update_Network();
    Game_Update_AI();
    Game_Update_Lighting();
    Game_Update_Crates();
    Game_Update_Radar();
    Game_Update_Shroud();
    Game_Update_Events();
    Game_Update_SuperWeapons();
    Game_Update_Tiberium();
    Game_Update_Veins();
    Game_Update_Ice();
    Game_Update_IonStorm();
    Game_Update_EVA();
}

// ============================================================================
// Main_Game_Render - Render the current frame
// ============================================================================

void Main_Game_Render()
{
    // In the original game, this is the main rendering pipeline:
    // 1. Clear the back buffer
    // 2. Render the tactical map (terrain, smudges, overlays)
    // 3. Render all objects (buildings, infantry, vehicles, aircraft)
    // 4. Render particles and effects
    // 5. Render the shroud
    // 6. Render the UI overlay (sidebar, radar, credits)
    // 7. Present the back buffer to the screen
}

// ============================================================================
// Main_Game_ProcessInput - Process player input
// ============================================================================

void Main_Game_ProcessInput()
{
    // In the original game, this processes:
    // 1. Keyboard state (movement, hotkeys, chat)
    // 2. Mouse state (selection, orders, scrolling)
    // 3. Game commands (pause, save, load, quit)
    Game_Update_Input();
}

// ============================================================================
// Main_Game_Network - Process network messages
// ============================================================================

void Main_Game_Network()
{
    // In the original game, this processes:
    // 1. Incoming network events from other players
    // 2. Outgoing network event queuing
    // 3. Network synchronization (CRC checks)
    // 4. Latency compensation
    // 5. Disconnection detection
}

// ============================================================================
// Main_Game_AI - Global AI update
// ============================================================================

void Main_Game_AI()
{
    // In the original game, this coordinates:
    // 1. AI house decision-making
    // 2. AI team script execution
    // 3. AI base building
    // 4. AI attack planning
    // 5. AI difficulty-based behavior
}

// ============================================================================
// Main_Game_CheckEndGame - Check for end-game conditions
// ============================================================================

void Main_Game_CheckEndGame()
{
    if (GameInEndGame) return;

    int32 alivePlayers = 0;
    int32 aliveAI = 0;
    HouseClass* lastAlive = nullptr;

    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (!pHouse) continue;
        if (pHouse->IsDefeated) continue;
        if (pHouse->IsDeadObject) continue;
        if (pHouse->IsObserver) continue;
        if (pHouse->IsMultiplayerPassive) continue;

        lastAlive = pHouse;
        if (pHouse->IsHumanPlayer) {
            ++alivePlayers;
        } else {
            ++aliveAI;
        }
    }

    // Check victory conditions:
    // - All human players eliminated -> AI wins
    // - Only one human player remains with no AI -> human wins
    // - All AI eliminated -> human wins
    int32 totalAlive = alivePlayers + aliveAI;

    if (totalAlive <= 1) {
        if (lastAlive) {
            if (lastAlive->IsHumanPlayer) {
                lastAlive->Win();
            } else {
                // AI won - find the last human player and make them lose
                for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
                    HouseClass* pHouse = HouseClass::Array[i];
                    if (pHouse && pHouse->IsHumanPlayer && !pHouse->IsDefeated) {
                        pHouse->Lose();
                    }
                }
            }
        }
        GameInEndGame = true;
    }
}

// ============================================================================
// Frame-level update functions
// ============================================================================

void Update_Game()
{
    Main_Game_Update();
}

void Game_Update_Frame()
{
    // Update frame-based systems
    if (ScenarioClass::Instance) {
        ScenarioClass::Instance->FrameCount = CurrentFrame;
    }

    // Update frame timer
    FrameTimer::CurrentFrame = CurrentFrame;
}

void Game_Update_Objects()
{
    // In the original game, this iterates all active game objects
    // and calls their per-frame update methods.
    // ObjectClass::UpdateAll() handles this in the original.
    //
    // Objects are updated in this order:
    // 1. Buildings
    // 2. Infantry
    // 3. Vehicles
    // 4. Aircraft
    // 5. Bullets and projectiles
    // 6. Animations
    // 7. Particles
    // 8. Voxel animations
    // 9. Terrain objects
    //
    // Each object's Update() method handles its own logic
    // (movement, targeting, firing, production, etc.)
}

void Game_Update_Houses()
{
    // Update all active houses
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        if (HouseClass::Array[i] && !HouseClass::Array[i]->IsDefeated) {
            HouseClass::Array[i]->Update();
        }
    }
}

void Game_Update_Map()
{
    // Update map-level systems
    if (MapClass::Instance) {
        // Update tiberium growth
        MapClass::Instance->Update_Tiberium_Spread();

        // Update crate respawn
        MapClass::Instance->Update_Crate_Respawn();
    }
}

void Game_Update_UI()
{
    // In the original game, this updates:
    // - Sidebar production progress
    // - Credits display
    // - Power bar
    // - Radar minimap blips
    // - Message list
    // - Tooltip display
    // - Cursor state
}

void Game_Update_Sound()
{
    // In the original game, this updates:
    // - Audio channel playback
    // - Sound effect queue
    // - Music playback state
    // - EVA message playback
    // - Ambient sound playback
}

void Game_Update_Animation()
{
    // In the original game, this updates:
    // - Animation frame progression
    // - Animation completion callbacks
    // - Animation cleanup
}

void Game_Update_Input()
{
    // In the original game, this processes:
    // - Keyboard state polling
    // - Mouse state polling
    // - Input event queuing
    // - Hotkey processing
    // - Chat input processing
}

void Game_Update_Network()
{
    // In the original game, this processes:
    // - Network event dequeuing
    // - Network event execution
    // - Network latency measurement
    // - Network state synchronization
}

void Game_Update_AI()
{
    // AI updates are handled per-house in Game_Update_Houses
    // This function handles global AI coordination:
    // - AI team coordination
    // - AI trigger evaluation
    // - AI difficulty adjustment
}

void Game_Update_Lighting()
{
    // Update lighting for all cells
    ScenarioClass::UpdateLighting();
}

void Game_Update_Crates()
{
    // Crate updates are handled in MapClass::Update_Crate_Respawn
    // This function handles:
    // - Crate pickup detection
    // - Crate effect application
    // - Crate animation
}

void Game_Update_Radar()
{
    // In the original game, this updates:
    // - Radar event processing
    // - Radar blip positions
    // - Radar spy/satellite state
    // - Radar jamming state
}

void Game_Update_Shroud()
{
    // In the original game, this updates:
    // - Shroud reveal per unit
    // - Gap generator effects
    // - Shroud edge recalculation
    // - Spy plane temporary reveal
}

void Game_Update_Events()
{
    // In the original game, this processes:
    // - Trigger condition evaluation
    // - Trigger action execution
    // - Global variable updates
    // - Local variable updates
}

void Game_Update_SuperWeapons()
{
    // Update super weapon timers for all houses
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        if (HouseClass::Array[i] && !HouseClass::Array[i]->IsDefeated) {
            HouseClass::Array[i]->CheckSWs();
        }
    }
}

void Game_Update_Tiberium()
{
    // Tiberium growth is handled in MapClass::Update_Tiberium_Spread
    // This function handles additional tiberium-related logic:
    // - Tiberium damage to units
    // - Tiberium collection by harvesters
    // - Tiberium value changes
}

void Game_Update_Veins()
{
    // In the original game, this handles:
    // - Vein growth and spread
    // - Vein monster behavior
    // - Vein damage to units
    // - Vein removal
}

void Game_Update_Ice()
{
    // In the original game, this handles:
    // - Ice growth and melting
    // - Ice cracking under weight
    // - Ice damage to units
}

void Game_Update_IonStorm()
{
    // In the original game, this handles:
    // - Ion storm movement
    // - Ion storm radar jamming
    // - Ion storm power disruption
    // - Ion storm damage to aircraft
}

void Game_Update_EVA()
{
    // In the original game, this handles:
    // - EVA message queue processing
    // - EVA message priority
    // - EVA message cooldown
    // - EVA voice playback
}

// ============================================================================
// Shutdown functions
// ============================================================================

void Shutdown_Game()
{
    GameActive = false;
    GameInProgress = false;
    GameInitDone = false;
    MainLoopActive = false;

    Shutdown_UI();
    Shutdown_AI();
    Shutdown_Sound();
    Shutdown_Network();
    Shutdown_Graphics();
    Shutdown_MixFiles();
    Shutdown_FileSystem();

    // Reset internal state
    g_GameObjectsInitialized = false;
    g_FileSystemInitialized = false;
    g_MixFilesLoaded = false;
    g_GraphicsInitialized = false;
    g_SoundInitialized = false;
    g_NetworkInitialized = false;
    g_AIInitialized = false;
    g_GameModeInitialized = false;
}

void Shutdown_Graphics()
{
    // In the original game, this:
    // - Releases DirectDraw/Direct3D surfaces
    // - Restores display mode
    // - Frees palette resources
    // - Destroys rendering contexts
    g_GraphicsInitialized = false;
}

void Shutdown_Sound()
{
    // In the original game, this:
    // - Stops all audio playback
    // - Releases audio channels
    // - Shuts down Miles Sound System or DirectSound
    // - Frees cached sound data
    GameSoundOn = false;
    GameMusicOn = false;
    g_SoundInitialized = false;
}

void Shutdown_Network()
{
    // In the original game, this:
    // - Disconnects from network sessions
    // - Releases socket handles
    // - Shuts down IPX/UDP interfaces
    // - Frees network buffers
    GameNetworkOn = false;
    g_NetworkInitialized = false;
}

void Shutdown_FileSystem()
{
    // In the original game, this:
    // - Closes all open file handles
    // - Flushes file caches
    // - Releases file system resources
    g_FileSystemInitialized = false;
}

void Shutdown_AI()
{
    // In the original game, this:
    // - Stops AI decision-making
    // - Clears AI team queues
    // - Frees AI trigger data
    g_AIInitialized = false;
}

void Shutdown_UI()
{
    // In the original game, this:
    // - Destroys UI elements
    // - Frees UI resources
    // - Clears message queues
}

void Shutdown_MixFiles()
{
    // In the original game, this:
    // - Unloads MIX archives
    // - Frees MIX file cache
    // - Closes MIX file handles
    g_MixFilesLoaded = false;
}

// ============================================================================
// Game mode functions
// ============================================================================

void Set_Game_Mode(int32 mode)
{
    GameState = mode;
}

void Init_GameMode_None()
{
    GameIsMultiplayer = false;
    GameIsSkirmish = false;
    GameIsCampaign = false;
    GameNetworkOn = false;
}

void Init_GameMode_SinglePlayer()
{
    GameIsMultiplayer = false;
    GameIsSkirmish = false;
    GameIsCampaign = false;
    GameNetworkOn = false;
}

void Init_GameMode_Skirmish()
{
    GameIsMultiplayer = false;
    GameIsSkirmish = true;
    GameIsCampaign = false;
    GameNetworkOn = false;
}

void Init_GameMode_Multiplayer()
{
    GameIsMultiplayer = true;
    GameIsSkirmish = false;
    GameIsCampaign = false;
}

void Init_GameMode_Campaign()
{
    GameIsMultiplayer = false;
    GameIsSkirmish = false;
    GameIsCampaign = true;
    GameNetworkOn = false;
}

void Init_GameMode_Network()
{
    GameNetworkOn = true;
    Init_GameMode_Multiplayer();
}

void Init_GameMode_LAN()
{
    Init_GameMode_Network();
}

void Init_GameMode_WOL()
{
    Init_GameMode_Network();
}

void Init_GameMode_Modem()
{
    Init_GameMode_Network();
}

void Init_GameMode_Serial()
{
    Init_GameMode_Network();
}

// ============================================================================
// Game flow functions
// ============================================================================

void Game_Start()
{
    GameActive = true;
    GameInProgress = true;
    GamePaused = 0;
    CurrentFrame = 0;
    GameInEndGame = false;
    GameIsComplete = false;
    Main_Game();
}

void Game_End()
{
    GameActive = false;
    GameInProgress = false;
    GameInEndGame = true;
}

void Game_Pause()
{
    ++GamePaused;
}

void Game_Resume()
{
    if (GamePaused > 0) {
        --GamePaused;
    }
}

void Game_Loading()
{
    GameIsLoading = true;
}

void Game_Saving()
{
    GameIsSaving = true;
}

void Game_LoadingScreen()
{
    // Render the loading screen: a black background, a centered title,
    // and a progress bar that fills as assets stream in.  The original
    // game uses a dedicated PCX background; here we draw directly onto
    // the GScreen back buffer so the function is self-contained.
    DSurface* surf = nullptr;
    if (TheGScreen != nullptr)
        surf = TheGScreen->Get_Back_Buffer();

    if (surf == nullptr || surf->Buffer == nullptr)
        return;

    // Clear to black (16-bit 565: 0x0000).
    surf->Fill(0x0000);

    int32 w = surf->Width;
    int32 h = surf->Height;

    // Title text centered horizontally, in the upper third.
    if (w > 0 && h > 0)
    {
        const wchar_t* title = L"Loading...";
        int32 titleY = h / 3;
        surf->DrawText(title, w / 2 - 60, titleY, 0xFFFF);

        // Progress bar frame (white outline on dark fill).
        int32 barW = w * 2 / 3;
        int32 barH = 20;
        int32 barX = (w - barW) / 2;
        int32 barY = h / 2;

        Rectangle barBg(barX, barY, barW, barH);
        surf->FillRect(&barBg, 0x0000);

        Rectangle barFrame(barX, barY, barW, barH);
        surf->DrawRect(&barFrame, 0xFFFF);
    }
}

void Game_SavingScreen()
{
    // Render the saving screen: identical layout to the loading screen
    // but with "Saving..." as the title text.
    DSurface* surf = nullptr;
    if (TheGScreen != nullptr)
        surf = TheGScreen->Get_Back_Buffer();

    if (surf == nullptr || surf->Buffer == nullptr)
        return;

    // Clear to black.
    surf->Fill(0x0000);

    int32 w = surf->Width;
    int32 h = surf->Height;

    if (w > 0 && h > 0)
    {
        const wchar_t* title = L"Saving...";
        int32 titleY = h / 3;
        surf->DrawText(title, w / 2 - 60, titleY, 0xFFFF);

        // Progress bar frame.
        int32 barW = w * 2 / 3;
        int32 barH = 20;
        int32 barX = (w - barW) / 2;
        int32 barY = h / 2;

        Rectangle barBg(barX, barY, barW, barH);
        surf->FillRect(&barBg, 0x0000);

        Rectangle barFrame(barX, barY, barW, barH);
        surf->DrawRect(&barFrame, 0xFFFF);
    }
}

void Game_Briefing()
{
    // Display mission briefing screen
    // In the original game, this shows:
    // - Mission text
    // - Briefing audio
    // - Map preview
    // - Start button
}

void Game_ScoreScreen()
{
    // Display end-of-game score screen
    // In the original, this shows:
    // - Player statistics
    // - Unit production counts
    // - Destroyed unit counts
    // - Score calculation
    // - Continue/quit options
}

void Game_Credits()
{
    // Display game credits
}

void Game_MainMenu()
{
    // Display main menu
    // In the original game, this shows:
    // - New Game
    // - Load Game
    // - Multiplayer
    // - Options
    // - Credits
    // - Exit
}

void Game_MapSelect()
{
    // Display map selection screen
    // In the original game, this shows:
    // - Available maps
    // - Map preview
    // - Game options (starting credits, etc.)
}

void Game_GameOptions()
{
    // Display game options screen
    // In the original game, this shows:
    // - Graphics settings
    // - Sound settings
    // - Game speed
    // - Scroll rate
    // - Difficulty
}

void Game_MultiplayerLobby()
{
    // Display multiplayer lobby
    // In the original game, this shows:
    // - Player list
    // - Chat
    // - Game settings
    // - Ready status
}

void Game_MultiplayerSetup()
{
    // Display multiplayer setup screen
}

void Game_SkirmishSetup()
{
    // Display skirmish setup screen
    // In the original game, this shows:
    // - Map selection
    // - AI player count and difficulty
    // - Starting credits
    // - Game options
}

void Game_NetworkLobby()
{
    // Display network lobby
}

void Game_NetworkSetup()
{
    // Display network setup screen
    // In the original game, this shows:
    // - Connection type (LAN, WOL, modem, serial)
    // - IP address / hostname entry
    // - Game name
}

void Game_ModemSetup()
{
    // Display modem connection setup
}

void Game_SerialSetup()
{
    // Display serial connection setup
}

void Game_WOLSetup()
{
    // Display Westwood Online setup
    // In the original game, this shows:
    // - Login
    // - Lobby
    // - Game list
    // - Chat
}

void Game_LoadGame()
{
    GameIsLoading = true;

    // In the original game, this:
    // 1. Opens the save file
    // 2. Deserializes the game state
    // 3. Restores all objects
    // 4. Resumes the game loop

    if (ScenarioClass::Instance) {
        // Load the game state from the save file
        // ScenarioClass::LoadGame(saveFileName);
    }

    GameIsLoading = false;
}

void Game_SaveGame()
{
    GameIsSaving = true;

    // In the original game, this:
    // 1. Creates a save file
    // 2. Serializes the full game state
    // 3. Writes the save file header

    if (ScenarioClass::Instance) {
        // Save the game state
        // ScenarioClass::SaveGame(saveFileName);
    }

    GameIsSaving = false;
}

void Game_NewGame()
{
    Init_Game();
    Game_Start();
}

void Game_ContinueGame()
{
    Game_Resume();
}

void Game_RestartGame()
{
    Shutdown_Game();
    Init_Game();
    Game_Start();
}

void Game_QuitGame()
{
    GameIsQuitting = true;
    GameIsComplete = true;
    GameInEndGame = true;
    MainLoopActive = false;
}

void Game_ExitToMenu()
{
    GameIsQuitting = true;
    GameInEndGame = true;
    MainLoopActive = false;
    // In the original game, this returns to the main menu
    // instead of fully exiting the application
}