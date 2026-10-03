#include "Game/MainLoop.h"
#include "Game/Game.h"
#include "Game/GameInit.h"
#include "Game/Externs.h"
#include "Game/CopyProtection.h"
#include "Rendering/GScreenClass.h"
#include "Rendering/TacticalClass.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/RadarClass.h"
#include "Houses/FactoryClass.h"
#include "Abstract/UnitClass.h"
#include "Abstract/InfantryClass.h"
#include "Combat/BulletClass.h"
#include "Houses/HouseClass.h"
#include "AI/TeamTypeClass.h"
#include "AI/TeamClass.h"
#include "AI/AITriggerTypeClass.h"
#include "Particles/ParticleClass.h"
#include "Animations/AnimClass.h"
#include "Locomotion/LocomotionClass.h"
#include "SW/SuperClass.h"
#include "Special/TiberiumClass.h"

#include "Core/Definitions.h"
#include "Core/Macros.h"

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
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Internal state
// ═══════════════════════════════════════════════════════════════════════════

static unsigned int s_FrameStartTime    = 0;
static unsigned int s_FrameEndTime      = 0;
static unsigned int s_FrameElapsedTime  = 0;
static unsigned int s_LastRenderTime    = 0;
static int          s_FrameRateCounter  = 0;
static int          s_FrameRateDisplay  = 0;
static unsigned int s_FrameRateTimer    = 0;

static const int    COPYPROT_CHECK_INTERVAL = 300;  // check every 300 frames (~10 sec at 30 FPS)

// ═══════════════════════════════════════════════════════════════════════════
// Main_Game — the main game loop
// ═══════════════════════════════════════════════════════════════════════════

void Main_Game()
{
    // Original binary: Main_Game() calls InitGame() first, then enters
    // the frame loop.  Mirror that contract here.
    if (!GameInitDone)
    {
        Init_Game();
    }

    Game::GameInProgress = true;

    s_FrameStartTime    = Game::GetTickCount();
    s_FrameRateTimer    = Game::GetTickCount();
    s_FrameRateCounter  = 0;

    // ── Main Loop ────────────────────────────────────────────────────────
    while (!Game::ShutdownRequested)
    {
        // 1. Process pending Windows messages
        ProcessWindowsMessages();

        // 2. Skip if the window is not in focus
        if (!Game::GameInFocus)
        {
            Game::Game_Sleep(10);
            continue;
        }

        // 3. Handle pause
        if (Game::GamePaused)
        {
            Game::Game_Sleep(10);
            continue;
        }

        // 4. Frame step mode (debug)
        if (Game::bAllowFrameStep && !Game::bFrameStep)
        {
            Game::Game_Sleep(1);
            continue;
        }
        Game::bFrameStep = false;

        // 5. Frame timing
        Sync_FrameTime();

        // 6. Advance frame counter
        Game::AdvanceFrame();

        // 7. Execute per-frame logic
        Main_Loop();

        // 8. Periodic copy protection check
        if ((Game::CurrentFrame % COPYPROT_CHECK_INTERVAL) == 0)
        {
            CopyProtection_Check();
        }

        // 9. Frame rate statistics
        Record_FrameStats();
    }

    Game::GameInProgress = false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Main_Loop — per-frame update
// ═══════════════════════════════════════════════════════════════════════════

void Main_Loop()
{
    // ── Phase 1: Input ───────────────────────────────────────────────────
    Update_Input();

    // ── Phase 2: Network ─────────────────────────────────────────────────
    Update_Network();

    // ── Phase 3: AI ──────────────────────────────────────────────────────
    Update_AI();

    // ── Phase 4: Object Logic ────────────────────────────────────────────
    Update_Objects();

    // ── Phase 5: Combat ──────────────────────────────────────────────────
    Update_Combat();

    // ── Phase 6: Movement / Locomotion ───────────────────────────────────
    Update_Locomotion();

    // ── Phase 7: Visual Effects ──────────────────────────────────────────
    Update_Animations();
    Update_Particles();
    Update_SuperWeapons();
    Update_SpecialEffects();

    // ── Phase 8: UI ──────────────────────────────────────────────────────
    Update_Radar();
    Update_Sidebar();
    Update_Display();

    // ── Phase 9: Rendering ───────────────────────────────────────────────
    Render_Frame();

    // ── Phase 10: Audio ──────────────────────────────────────────────────
    Update_Audio();
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_GameSpeed — throttle the frame rate based on GameSpeed setting
// ═══════════════════════════════════════════════════════════════════════════

void Update_GameSpeed()
{
    static unsigned int s_LastSpeedUpdate = 0;

    unsigned int now = Game::GetTickCount();

    // Only re-check every 500 ms to avoid thrashing
    if ((now - s_LastSpeedUpdate) < 500)
        return;

    s_LastSpeedUpdate = now;

    // Frame delay is already calibrated by Game::SetGameSpeed()
    // No additional work needed here unless dynamic speed adjustment
    // is required (e.g., network game speed negotiation).
}

// ═══════════════════════════════════════════════════════════════════════════
// ProcessWindowsMessages
// ═══════════════════════════════════════════════════════════════════════════

void ProcessWindowsMessages()
{
#if defined(PLATFORM_WINDOWS)
    MSG msg;

    while (::PeekMessageA(&msg, static_cast<HWND>(Game::hWnd), 0, 0, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessageA(&msg);

        if (msg.message == WM_QUIT)
        {
            Game::ShutdownRequested = true;
            return;
        }
    }
#elif defined(PLATFORM_LINUX)
    // On Linux, message processing is handled by the windowing toolkit
    // (SDL, X11, etc.).  This is a no-op in the base implementation.
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Input
// ═══════════════════════════════════════════════════════════════════════════

void Update_Input()
{
    // Poll keyboard and mouse state
    // In the original engine, this reads DirectInput buffers and updates
    // the global key/mouse state arrays.
    //
    // The actual implementation routes through the Keyboard and Mouse
    // subsystem classes, which process buffered input events accumulated
    // during the window procedure.

    // Keyboard processing:
    //   - Read buffered key events
    //   - Update key-down and key-up states
    //   - Handle modifier keys (Shift, Ctrl, Alt)
    //   - Process hotkey bindings

    // Mouse processing:
    //   - Read buffered mouse events
    //   - Update cursor position
    //   - Update button states
    //   - Handle scroll wheel events
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Network
// ═══════════════════════════════════════════════════════════════════════════

void Update_Network()
{
    if (!Game::IsNetworkGame)
        return;

    if (TheNetworking == nullptr)
        return;

    // In the original engine:
    //   1. Process incoming network packets
    //   2. Handle command queue (received commands from other players)
    //   3. Send outgoing command frames
    //   4. Check for disconnections
    //   5. Handle latency compensation
    //   6. Process chat messages
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_AI
// ═══════════════════════════════════════════════════════════════════════════

void Update_AI()
{
    // 1. Advance every formed team (all houses, human and AI alike) so
    //    their scripts and movement keep executing.
    if (TeamClass::Array != nullptr)
    {
        for (int32 i = 0; i < TeamClass::Array->Count; ++i)
        {
            TeamClass* pTeam = (*TeamClass::Array)[i];
            if (pTeam != nullptr)
                pTeam->Update();
        }
    }

    // AI triggers are re-evaluated on a fixed cadence (15 frames) rather
    // than every frame, mirroring the original AI tick behaviour.
    static int32 s_AITick = 0;
    if ((++s_AITick % 15) != 0)
        return;

    if (AITriggerTypeClass::Array == nullptr)
        return;

    // 2. For every AI house, walk the trigger table and launch the attack
    //    wave bound to any trigger whose conditions are satisfied.
    for (int32 h = 0; h < HouseClass::ArrayCount; ++h)
    {
        HouseClass* pHouse = HouseClass::Array[h];
        if (pHouse == nullptr || pHouse->IsHumanPlayer)
            continue;

        // Resolve the primary enemy once per house.
        HouseClass* pTarget = nullptr;
        for (int32 th = 0; th < HouseClass::ArrayCount; ++th)
        {
            HouseClass* pOther = HouseClass::Array[th];
            if (pOther != nullptr && pOther != pHouse && !pHouse->IsAlliedWith(pOther))
            {
                pTarget = pOther;
                break;
            }
        }

        for (int32 t = 0; t < AITriggerTypeClass::Array->Count; ++t)
        {
            AITriggerTypeClass* pTrigger = (*AITriggerTypeClass::Array)[t];
            if (pTrigger == nullptr || !pTrigger->IsEnabled)
                continue;
            if (pHouse->TechLevel < pTrigger->TechLevel)
                continue;
            if (pTrigger->Weight_Current < pTrigger->Weight_Minimum)
                continue;

            if (pTrigger->ConditionMet(pHouse, pTarget, false))
            {
                TeamTypeClass* pWave = (pTrigger->Team1 != nullptr) ? pTrigger->Team1 : pTrigger->Team2;
                if (pWave != nullptr && TeamClass::Array != nullptr)
                {
                    TeamClass* pTeam = new TeamClass(pWave, pHouse, 0);
                    if (pTeam != nullptr)
                        TeamClass::Array->Add(pTeam);
                }
                pTrigger->RegisterSuccess();
            }
            else
            {
                pTrigger->RegisterFailure();
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Objects
// ═══════════════════════════════════════════════════════════════════════════

void Update_Objects()
{
    // Advance every production factory (build progress, completion,
    // queue draining).
    if (FactoryClass::Array != nullptr)
    {
        for (int32 i = 0; i < FactoryClass::Array->Count; ++i)
        {
            FactoryClass* pFactory = (*FactoryClass::Array)[i];
            if (pFactory != nullptr)
                pFactory->Update();
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Combat
// ═══════════════════════════════════════════════════════════════════════════

void Update_Combat()
{
    // Advance every in-flight projectile.  Each Update advances the
    // bullet along its trajectory, checks for collision with the
    // target or terrain, and detonates when it arrives.
    if (BulletClass::Array == nullptr)
        return;

    for (int32 i = BulletClass::Array->Count - 1; i >= 0; --i)
    {
        BulletClass* pBullet = (*BulletClass::Array)[i];
        if (pBullet == nullptr)
            continue;

        if (pBullet->IsBulletDetonated)
        {
            GameDelete(pBullet);
            continue;
        }

        if (pBullet->IsBulletActive)
        {
            pBullet->Update();
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Locomotion
// ═══════════════════════════════════════════════════════════════════════════

void Update_Locomotion()
{
    // Drive the per-frame movement update of every ground/water unit.
    // FootClass::Locomotion->Process() dispatches into the concrete
    // locomotion's main loop (DriveLocomotionClass::blah etc.).
    if (UnitClass::Array != nullptr)
    {
        for (int32 i = 0; i < UnitClass::Array->Count; ++i)
        {
            UnitClass* pUnit = (*UnitClass::Array)[i];
            if (pUnit == nullptr)
                continue;

            LocomotionClass* pLoco = pUnit->Get_Locomotion();
            if (pLoco != nullptr)
                pLoco->Process();
        }
    }

    // Infantry and other foot types share the same locomotion interface.
    if (InfantryClass::Array != nullptr)
    {
        for (int32 i = 0; i < InfantryClass::Array->Count; ++i)
        {
            InfantryClass* pInf = (*InfantryClass::Array)[i];
            if (pInf == nullptr)
                continue;

            LocomotionClass* pLoco = pInf->Get_Locomotion();
            if (pLoco != nullptr)
                pLoco->Process();
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Animations
// ═══════════════════════════════════════════════════════════════════════════

void Update_Animations()
{
    // In the original engine:
    //   1. Iterate over all active animations
    //   2. Advance animation frame counters
    //   3. Process animation state transitions
    //   4. Handle animation looping
    //   5. Remove completed animations
    //   6. Process animation-linked events (e.g., sound triggers)
    //   7. Handle building anims (construction, damaged, active)
    //   8. Handle unit idle animations
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Particles
// ═══════════════════════════════════════════════════════════════════════════

void Update_Particles()
{
    // In the original engine:
    //   1. Iterate over all active particle systems
    //   2. Update particle positions
    //   3. Update particle lifetimes
    //   4. Remove expired particles
    //   5. Handle particle spawning
    //   6. Process particle physics (gravity, wind, etc.)
    //   7. Handle smoke, fire, spark, debris particles
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_SuperWeapons
// ═══════════════════════════════════════════════════════════════════════════

void Update_SuperWeapons()
{
    // Drive the charge / ready / firing state machine of every super
    // weapon in the game.
    if (SuperClass::Array != nullptr)
    {
        for (int32 i = 0; i < SuperClass::Array->Count; ++i)
        {
            SuperClass* pSW = (*SuperClass::Array)[i];
            if (pSW != nullptr)
                pSW->Update();
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_SpecialEffects
// ═══════════════════════════════════════════════════════════════════════════

void Update_SpecialEffects()
{
    // Ore growth and spreading are driven per cell by the map's cell update
    // pass; there is no separate manager object in the original, so nothing
    // extra is done here.
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Radar
// ═══════════════════════════════════════════════════════════════════════════

void Update_Radar()
{
    if (TheRadar == nullptr)
        return;

    // 1. Update power/availability state (jamming, spy effects are
    //    folded into the availability check by the radar itself).
    TheRadar->Update();

    // 2. Re-render the radar surface when the map or object set changed;
    //    Draw() is cheap when nothing is dirty.
    TheRadar->Draw();
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Sidebar
// ═══════════════════════════════════════════════════════════════════════════

void Update_Sidebar()
{
    if (TheSidebar == nullptr)
        return;

    // In the original engine:
    //   1. Process sidebar button clicks
    //   2. Update production progress indicators
    //   3. Update build queue display
    //   4. Handle sidebar tab switching
    //   5. Update tooltip display
    //   6. Handle repair/sell cursor modes
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Display
// ═══════════════════════════════════════════════════════════════════════════

void Update_Display()
{
    // In the original engine:
    //   1. Update scroll position
    //   2. Process screen edge scrolling
    //   3. Handle minimap clicks
    //   4. Update fog of war
    //   5. Update shroud calculations
    //   6. Update cell visibility
    //   7. Process display mode changes
}

// ═══════════════════════════════════════════════════════════════════════════
// Render_Frame — full frame rendering pipeline
// ═══════════════════════════════════════════════════════════════════════════

void Render_Frame()
{
    if (TheDisplay == nullptr)
        return;

    // Back buffer for this frame (original: DSurface_CreatePrimary -> back
    // buffer, blitted to screen at the end of the frame).
    DSurface* pBack = (TheGScreen != nullptr) ? TheGScreen->Get_Back_Buffer() : nullptr;

    // ── Phase 1: Clear back buffer ──────────────────────────────────────
    if (pBack != nullptr)
    {
        pBack->Fill(0);   // black (original clears to the palette index 0)
    }

    // ── Phase 2: Terrain layer (isometric tiles) ────────────────────────
    if (TheTactical != nullptr)
    {
        TheTactical->Render(pBack, false, TacticalRenderMode::Terrain);
    }

    // ── Phase 3: Object layer (buildings/infantry/vehicles/aircraft) ────
    if (TheTactical != nullptr)
    {
        TheTactical->Render(pBack, false, TacticalRenderMode::MovingAnimating);
    }

    // ── Phase 4: Effect layer (animations / particles / superweapons) ───
    if (TheTactical != nullptr)
    {
        TheTactical->Render(pBack, false, TacticalRenderMode::AllAlt);
    }

    // ── Phase 5: Shroud / fog overlay ───────────────────────────────────
    if (TheTactical != nullptr)
    {
        TheTactical->Draw_Shroud();
        TheTactical->Draw_Fog();
    }

    // ── Phase 6: UI layer (radar / sidebar / cursor) ────────────────────
    if (TheDisplay != nullptr)
    {
        TheDisplay->Draw(true);
    }

    // ── Phase 7: Present back buffer to screen ──────────────────────────
    if (TheGScreen != nullptr)
    {
        TheGScreen->BlitToScreen();
    }

    s_LastRenderTime = Game::GetTickCount();
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Audio
// ═══════════════════════════════════════════════════════════════════════════

void Update_Audio()
{
    // In the original engine:
    //   1. Process audio event queue
    //   2. Start/stop sounds based on game events
    //   3. Update 3D positional audio
    //   4. Handle music track transitions
    //   5. Process volume changes
    //   6. Handle sound priority and culling
    //   7. Process audio streaming
}

// ═══════════════════════════════════════════════════════════════════════════
// Sync_FrameTime — ensure consistent frame timing
// ═══════════════════════════════════════════════════════════════════════════

void Sync_FrameTime()
{
    unsigned int currentTime = Game::GetTickCount();
    unsigned int elapsed     = currentTime - s_FrameStartTime;

    // If we finished the frame early, sleep to maintain the target frame rate
    if (elapsed < Game::FrameDelay)
    {
        unsigned int remaining = Game::FrameDelay - elapsed;
        Game::Game_Sleep(remaining);
    }

    // Record the actual frame start time
    s_FrameStartTime = Game::GetTickCount();
}

// ═══════════════════════════════════════════════════════════════════════════
// Record_FrameStats — update frame rate counters
// ═══════════════════════════════════════════════════════════════════════════

void Record_FrameStats()
{
    ++s_FrameRateCounter;

    unsigned int now = Game::GetTickCount();
    unsigned int diff = now - s_FrameRateTimer;

    if (diff >= 1000)
    {
        s_FrameRateDisplay = s_FrameRateCounter;
        Game::FrameRate    = s_FrameRateCounter;
        s_FrameRateCounter = 0;
        s_FrameRateTimer   = now;
    }

    s_FrameEndTime = now;
}