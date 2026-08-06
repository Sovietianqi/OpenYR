#pragma once

#include "Core/Definitions.h"

// ═══════════════════════════════════════════════════════════════════════════
// InitList — ordered initialization / shutdown registry
//
//  InitList_Add()      registers a function pointer with a priority and
//                      description string.  Lower priority values execute
//                      first during InitList_Execute().
//
//  InitList_Execute()  calls every registered init function in ascending
//                      priority order.
//
//  ShutdownList_Execute() calls every registered init function in
//                      descending priority order (reverse).
//
//  The maximum number of entries is defined by INIT_LIST_MAX_ENTRIES.
// ═══════════════════════════════════════════════════════════════════════════

#define INIT_LIST_MAX_ENTRIES      64

typedef void (*InitListFunc)(void);

struct InitListEntry
{
    InitListFunc    Function;
    int             Priority;
    const char*     Description;
    bool            Executed;
};

// ── API ───────────────────────────────────────────────────────────────────

void InitList_Add(InitListFunc func, int priority, const char* desc);
void InitList_Execute();
void ShutdownList_Execute();

// ── Pre-defined init priority constants ───────────────────────────────────

enum InitPriority
{
    INIT_PRIORITY_OS_ENVIRONMENT     = 0,
    INIT_PRIORITY_RANDOM             = 10,
    INIT_PRIORITY_FILESYSTEM         = 20,
    INIT_PRIORITY_MIX_FILES          = 30,
    INIT_PRIORITY_GRAPHICS           = 40,
    INIT_PRIORITY_KEYBOARD           = 50,
    INIT_PRIORITY_MOUSE              = 60,
    INIT_PRIORITY_SOUND              = 70,
    INIT_PRIORITY_GAME_OBJECTS       = 80,
    INIT_PRIORITY_GAME_MODE          = 90,
    INIT_PRIORITY_SCENARIO           = 100,
    INIT_PRIORITY_AI                 = 110,
    INIT_PRIORITY_NETWORK            = 120,
    INIT_PRIORITY_MAIN_GAME          = 130,
    INIT_PRIORITY_LAST               = 1000
};

// ── InitList-specific function declarations (avoid ODR conflict with GameInit.h) ──

void InitList_Init_OS_Environment();
void InitList_Init_Random();
void InitList_Init_FileSystem();
void InitList_Init_MixFiles();
void InitList_Init_Graphics();
void InitList_Init_Keyboard();
void InitList_Init_Mouse();
void InitList_Init_Sound();
void InitList_Init_GameObjects();
void InitList_Init_GameMode();
void InitList_Init_Scenario();
void InitList_Init_AI();
void InitList_Init_Network();
void InitList_Main_Game();

// ── Shutdown entries ──────────────────────────────────────────────────────

void InitList_Shutdown_Network();
void InitList_Shutdown_AI();
void InitList_Shutdown_Scenario();
void InitList_Shutdown_GameMode();
void InitList_Shutdown_GameObjects();
void InitList_Shutdown_Sound();
void InitList_Shutdown_Mouse();
void InitList_Shutdown_Keyboard();
void InitList_Shutdown_Graphics();
void InitList_Shutdown_MixFiles();
void InitList_Shutdown_FileSystem();
void InitList_Shutdown_OS_Environment();

// ── Register all init entries ─────────────────────────────────────────────

inline void InitList_RegisterAll()
{
    InitList_Add(InitList_Init_OS_Environment,  INIT_PRIORITY_OS_ENVIRONMENT, "OS Environment");
    InitList_Add(InitList_Init_Random,          INIT_PRIORITY_RANDOM,         "Random Number Generator");
    InitList_Add(InitList_Init_FileSystem,      INIT_PRIORITY_FILESYSTEM,     "File System");
    InitList_Add(InitList_Init_MixFiles,        INIT_PRIORITY_MIX_FILES,      "MIX Archive Files");
    InitList_Add(InitList_Init_Graphics,        INIT_PRIORITY_GRAPHICS,       "Graphics / Display");
    InitList_Add(InitList_Init_Keyboard,        INIT_PRIORITY_KEYBOARD,       "Keyboard Handler");
    InitList_Add(InitList_Init_Mouse,           INIT_PRIORITY_MOUSE,          "Mouse Handler");
    InitList_Add(InitList_Init_Sound,           INIT_PRIORITY_SOUND,          "Sound / Audio");
    InitList_Add(InitList_Init_GameObjects,     INIT_PRIORITY_GAME_OBJECTS,   "Game Objects");
    InitList_Add(InitList_Init_GameMode,        INIT_PRIORITY_GAME_MODE,      "Game Mode");
    InitList_Add(InitList_Init_Scenario,        INIT_PRIORITY_SCENARIO,       "Scenario");
    InitList_Add(InitList_Init_AI,              INIT_PRIORITY_AI,             "AI System");
    InitList_Add(InitList_Init_Network,         INIT_PRIORITY_NETWORK,        "Networking");
    InitList_Add(InitList_Main_Game,            INIT_PRIORITY_MAIN_GAME,      "Main Game Loop");
}