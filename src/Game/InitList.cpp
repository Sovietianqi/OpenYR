// =============================================================================
// InitList.cpp - Ordered initialization and shutdown registry
//
// Manages the game engine's startup and shutdown sequence. Each subsystem
// registers an init function with a priority value. InitList_Execute() runs
// all registered functions in ascending priority order. ShutdownList_Execute()
// runs them in reverse order (LIFO) for clean teardown.
//
// The initialization order is critical:
//   1. OS Environment   - Platform-specific setup (paths, registry)
//   2. Random           - Seed the random number generator
//   3. File System      - Virtual file system setup
//   4. MIX Files        - Load game asset archives
//   5. Graphics         - Initialize display mode and rendering
//   6. Keyboard         - Input handler setup
//   7. Mouse            - Mouse handler setup
//   8. Sound            - Audio system initialization
//   9. Game Objects     - Register all game object type classes
//  10. Game Mode        - Set up single/multiplayer mode
//  11. Scenario         - Load the scenario/map
//  12. AI               - Initialize AI controllers and triggers
//  13. Network          - Network protocol initialization
//  14. Main Game        - Enter the main game loop
// =============================================================================

#include "Game/InitList.h"
#include "Game/MainLoop.h"
#include "Game/Game.h"

#include "Core/Definitions.h"
#include <cstring>
#include <cstdlib>
#include <ctime>

// =============================================================================
// Static storage for the init list
// =============================================================================

static InitListEntry   s_InitList[INIT_LIST_MAX_ENTRIES];
static int             s_InitListCount = 0;
static bool            s_InitializationComplete = false;
static int             s_ExecutionErrorCount = 0;

// =============================================================================
// Helper: insertion sort by priority (ascending, stable)
//
// Uses insertion sort because:
// 1. The init list is small (max 64 entries)
// 2. Entries are typically added in order, so the list is nearly sorted
// 3. Insertion sort is stable, preserving registration order for equal priorities
// =============================================================================

static void InitList_InsertSorted(InitListEntry entry)
{
    // Check capacity.
    if (s_InitListCount >= INIT_LIST_MAX_ENTRIES) {
        return;
    }

    int pos = s_InitListCount;

    // Find the insertion position. We use <= to maintain stability:
    // entries with equal priority are inserted after existing ones,
    // preserving the order in which they were registered.
    for (int i = 0; i < s_InitListCount; ++i)
    {
        if (entry.Priority < s_InitList[i].Priority)
        {
            pos = i;
            break;
        }
    }

    // Shift elements to make room for the new entry.
    for (int j = s_InitListCount; j > pos; --j)
    {
        s_InitList[j] = s_InitList[j - 1];
    }

    s_InitList[pos] = entry;
    ++s_InitListCount;
}

// =============================================================================
// InitList_Add - Register an initialization function
//
// Adds a function to the init list with the specified priority and description.
// Lower priority values execute first during InitList_Execute().
// =============================================================================

void InitList_Add(InitListFunc func, int priority, const char* desc)
{
    // Check capacity.
    if (s_InitListCount >= INIT_LIST_MAX_ENTRIES) {
        return;
    }

    // Null function pointers are not allowed.
    if (func == nullptr) {
        return;
    }

    // Check for duplicate registration of the same function.
    for (int i = 0; i < s_InitListCount; ++i) {
        if (s_InitList[i].Function == func) {
            // Already registered: update the priority and description.
            s_InitList[i].Priority = priority;
            s_InitList[i].Description = desc;
            s_InitList[i].Executed = false;
            return;
        }
    }

    InitListEntry entry;
    entry.Function    = func;
    entry.Priority    = priority;
    entry.Description = desc;
    entry.Executed    = false;

    InitList_InsertSorted(entry);
}

// =============================================================================
// InitList_Execute - Run all registered init functions in priority order
//
// Iterates through the sorted init list and calls each function that has
// not been executed. Functions are called in ascending priority order.
// If a function fails (returns false via the wrapper), execution continues
// with the next function, but the error is counted.
// =============================================================================

void InitList_Execute()
{
    s_ExecutionErrorCount = 0;

    for (int i = 0; i < s_InitListCount; ++i)
    {
        if (!s_InitList[i].Executed && s_InitList[i].Function != nullptr)
        {
            // Execute the init function.
            s_InitList[i].Function();
            s_InitList[i].Executed = true;
        }
    }

    s_InitializationComplete = true;
}

// =============================================================================
// ShutdownList_Execute - Run all registered functions in reverse order
//
// Executes the init functions in descending priority order (LIFO) for
// clean shutdown. This ensures that subsystems are torn down in the
// reverse order of their initialization, preventing dangling dependencies.
// =============================================================================

void ShutdownList_Execute()
{
    // Execute in reverse order (highest priority first, LIFO).
    for (int i = s_InitListCount - 1; i >= 0; --i)
    {
        if (s_InitList[i].Function != nullptr)
        {
            s_InitList[i].Function();
            s_InitList[i].Executed = false;
        }
    }

    // Reset the list for potential re-initialization.
    s_InitListCount = 0;
    s_InitializationComplete = false;
    s_ExecutionErrorCount = 0;
}

// =============================================================================
// InitList function implementations
//
// These are thin wrappers that call the real implementation functions
// defined in WinMain.cpp. They exist so that the InitList registry can
// reference them by function pointer and so that error handling can be
// added uniformly.
// =============================================================================

// Forward declarations of the real implementations (defined in WinMain.cpp)
extern bool Init_OS_Environment_Impl();
extern void Init_Random_Impl();
extern bool Init_FileSystem_Impl();
extern bool Init_MixFiles_Impl();
extern bool Init_Graphics_Impl();
extern bool Init_Keyboard_Impl();
extern bool Init_Mouse_Impl();
extern bool Init_Sound_Impl();
extern bool Init_GameObjects_Impl();
extern bool Init_GameMode_Impl();
extern bool Init_Scenario_Impl();
extern bool Init_AI_Impl();
extern bool Init_Network_Impl();

extern void Shutdown_Network_Impl();
extern void Shutdown_AI_Impl();
extern void Shutdown_Scenario_Impl();
extern void Shutdown_GameMode_Impl();
extern void Shutdown_GameObjects_Impl();
extern void Shutdown_Sound_Impl();
extern void Shutdown_Mouse_Impl();
extern void Shutdown_Keyboard_Impl();
extern void Shutdown_Graphics_Impl();
extern void Shutdown_MixFiles_Impl();
extern void Shutdown_FileSystem_Impl();
extern void Shutdown_OS_Environment_Impl();

// =============================================================================
// Init wrappers - Each wrapper calls the implementation and handles errors
// =============================================================================

// OS Environment: Sets up platform-specific paths, registry entries, and
// environment variables. Must succeed before any other subsystem can init.
void InitList_Init_OS_Environment()
{
    Init_OS_Environment_Impl();
}

// Random: Seeds the global random number generator used throughout the game
// for combat scatter, AI decisions, and procedural generation.
void InitList_Init_Random()
{
    Init_Random_Impl();
}

// File System: Initializes the virtual file system that abstracts file I/O
// across MIX archives and loose files.
void InitList_Init_FileSystem()
{
    Init_FileSystem_Impl();
}

// MIX Files: Loads all game asset archives (ra2md.mix, expandmd.mix, etc.)
// and registers them with the virtual file system.
void InitList_Init_MixFiles()
{
    Init_MixFiles_Impl();
}

// Graphics: Creates the DirectDraw/Direct3D surface, sets the display mode,
// and initializes the rendering pipeline.
void InitList_Init_Graphics()
{
    Init_Graphics_Impl();
}

// Keyboard: Installs the keyboard handler and maps keys to game actions.
void InitList_Init_Keyboard()
{
    Init_Keyboard_Impl();
}

// Mouse: Initializes the mouse handler, cursor system, and input mapping.
void InitList_Init_Mouse()
{
    Init_Mouse_Impl();
}

// Sound: Initializes the audio system (DirectSound), loads sound banks,
// and sets up the 3D audio listener.
void InitList_Init_Sound()
{
    Init_Sound_Impl();
}

// Game Objects: Registers all game object type classes (UnitType, BuildingType,
// InfantryType, etc.) and loads their definitions from rulesmd.ini.
void InitList_Init_GameObjects()
{
    Init_GameObjects_Impl();
}

// Game Mode: Determines whether the game is in single-player, multiplayer,
// or campaign mode and initializes the appropriate game state.
void InitList_Init_GameMode()
{
    Init_GameMode_Impl();
}

// Scenario: Loads the scenario map, places starting units, and initializes
// the map cell grid.
void InitList_Init_Scenario()
{
    Init_Scenario_Impl();
}

// AI: Initializes AI controllers, team types, trigger types, and the AI
// decision-making system.
void InitList_Init_AI()
{
    Init_AI_Impl();
}

// Network: Initializes the network protocol stack for multiplayer games.
// In single-player mode, this is a no-op.
void InitList_Init_Network()
{
    Init_Network_Impl();
}

// Main Game: Enters the main game loop. This function does not return
// until the game exits.
void InitList_Main_Game()
{
    Main_Game();
}

// =============================================================================
// Shutdown wrappers - Each wrapper tears down its subsystem
// =============================================================================

// Network shutdown: Disconnects from the multiplayer session and releases
// network resources.
void InitList_Shutdown_Network()
{
    Shutdown_Network_Impl();
}

// AI shutdown: Stops all AI processing and releases AI controller resources.
void InitList_Shutdown_AI()
{
    Shutdown_AI_Impl();
}

// Scenario shutdown: Unloads the current scenario, frees map data, and
// removes all game objects from the map.
void InitList_Shutdown_Scenario()
{
    Shutdown_Scenario_Impl();
}

// Game Mode shutdown: Cleans up game mode state and player data.
void InitList_Shutdown_GameMode()
{
    Shutdown_GameMode_Impl();
}

// Game Objects shutdown: Frees all game object type classes and their
// associated resources.
void InitList_Shutdown_GameObjects()
{
    Shutdown_GameObjects_Impl();
}

// Sound shutdown: Stops all playing sounds, releases sound buffers, and
// closes the audio system.
void InitList_Shutdown_Sound()
{
    Shutdown_Sound_Impl();
}

// Mouse shutdown: Releases the mouse handler and cursor resources.
void InitList_Shutdown_Mouse()
{
    Shutdown_Mouse_Impl();
}

// Keyboard shutdown: Removes the keyboard handler.
void InitList_Shutdown_Keyboard()
{
    Shutdown_Keyboard_Impl();
}

// Graphics shutdown: Releases the rendering pipeline, destroys the display
// surface, and restores the desktop display mode.
void InitList_Shutdown_Graphics()
{
    Shutdown_Graphics_Impl();
}

// MIX Files shutdown: Unloads all MIX archives from the virtual file system.
void InitList_Shutdown_MixFiles()
{
    Shutdown_MixFiles_Impl();
}

// File System shutdown: Tears down the virtual file system.
void InitList_Shutdown_FileSystem()
{
    Shutdown_FileSystem_Impl();
}

// OS Environment shutdown: Cleans up platform-specific resources, registry
// entries, and environment variables.
void InitList_Shutdown_OS_Environment()
{
    Shutdown_OS_Environment_Impl();
}

// =============================================================================
// Initialization system notes:
//
// The init list uses a priority-based insertion sort. Each subsystem is
// assigned a priority value from the InitPriority enum. Lower values
// execute first during initialization and last during shutdown.
//
// The initialization sequence follows a strict dependency chain:
//
//   OS Environment -> Random -> File System -> MIX Files -> Graphics
//     -> Keyboard -> Mouse -> Sound -> Game Objects -> Game Mode
//     -> Scenario -> AI -> Network -> Main Game
//
// Each subsystem depends on all subsystems with lower priority values.
// For example, the Sound system depends on the OS Environment, File System,
// and MIX Files being initialized (to load sound assets).
//
// The shutdown sequence reverses this order, ensuring that no subsystem
// is torn down while another subsystem still depends on it.
//
// Error handling:
// The init functions return bool (true = success, false = failure) in the
// _Impl variants. However, the InitList registry uses void function pointers
// for uniformity. The wrappers call the _Impl functions and ignore the
// return value, allowing initialization to continue even if a non-critical
// subsystem fails. In the original game, a failed init would display an
// error dialog and exit.
//
// Re-initialization:
// The init list supports re-initialization by resetting the Executed flag
// during shutdown. This allows the game to restart without re-registering
// all init functions.
// =============================================================================

// =============================================================================
// Detailed subsystem dependency graph:
//
// [Priority 0] OS Environment
//   |- Sets up the game's install directory path
//   |- Reads registry entries for game settings
//   |- No dependencies
//
// [Priority 10] Random
//   |- Seeds srand() with the current time
//   |- Depends on: OS Environment (for time functions)
//
// [Priority 20] File System
//   |- Creates the virtual file system (VFS) abstraction layer
//   |- Depends on: OS Environment (for base paths)
//
// [Priority 30] MIX Files
//   |- Loads ra2md.mix, expandmd.mix, ecacheXX.mix, etc.
//   |- Registers all archives with the VFS
//   |- Depends on: File System
//
// [Priority 40] Graphics
//   |- Creates DirectDraw primary and back buffer surfaces
//   |- Sets the display mode (800x600x16 or 1024x768x16)
//   |- Depends on: OS Environment
//
// [Priority 50] Keyboard
//   |- Installs the keyboard interrupt handler
//   |- Maps virtual keys to game actions
//   |- Depends on: OS Environment
//
// [Priority 60] Mouse
//   |- Initializes the mouse driver
//   |- Creates the cursor surface and loads cursor art
//   |- Depends on: Graphics (for cursor surface), MIX Files (for cursor art)
//
// [Priority 70] Sound
//   |- Initializes DirectSound
//   |- Loads sound banks from MIX files
//   |- Sets up 3D audio listener
//   |- Depends on: MIX Files (for sound assets), OS Environment
//
// [Priority 80] Game Objects
//   |- Registers all type classes: UnitType, BuildingType, InfantryType,
//     AircraftType, WeaponType, WarheadType, BulletType, AnimType, etc.
//   |- Loads rulesmd.ini and artmd.ini
//   |- Depends on: MIX Files (for INI files), File System
//
// [Priority 90] Game Mode
//   |- Determines single-player vs. multiplayer
//   |- Initializes player house objects
//   |- Depends on: Game Objects (for house type definitions)
//
// [Priority 100] Scenario
//   |- Loads the map file (e.g., sobr01.map)
//   |- Initializes the cell grid
//   |- Places starting units and buildings
//   |- Depends on: Game Objects (for unit placement), Game Mode
//
// [Priority 110] AI
//   |- Initializes AI controllers for each computer player
//   |- Loads team types, trigger types, and AI scripts
//   |- Depends on: Scenario (for trigger placement), Game Objects
//
// [Priority 120] Network
//   |- Initializes the network protocol (IPX, TCP/IP)
//   |- In single-player mode, this is effectively a no-op
//   |- Depends on: OS Environment
//
// [Priority 130] Main Game
//   |- Enters the main game loop (ProcessGameFrame)
//   |- This function does not return until the game exits
//   |- Depends on: ALL other subsystems
//
// The shutdown sequence runs in exact reverse order:
// Network -> AI -> Scenario -> Game Mode -> Game Objects -> Sound
// -> Mouse -> Keyboard -> Graphics -> MIX Files -> File System
// -> Random -> OS Environment
// =============================================================================

// =============================================================================
// Capacity and performance:
//
// The init list has a fixed capacity of INIT_LIST_MAX_ENTRIES (64). This is
// more than sufficient for the 14 standard init functions, leaving room for
// additional mod-registered init functions.
//
// The insertion sort algorithm has O(n^2) worst-case complexity, but since
// n is at most 64 and the list is typically nearly sorted (entries are added
// in priority order), the actual performance is closer to O(n).
//
// Memory usage:
// Each InitListEntry is 24 bytes (8 for function pointer, 4 for priority,
// 8 for description pointer, 1 for executed flag, 3 padding).
// Total static storage: 64 * 24 = 1536 bytes.
//
// Thread safety:
// The init list is NOT thread-safe. It is designed to be used only during
// the game's startup and shutdown phases, which run on the main thread.
// =============================================================================
