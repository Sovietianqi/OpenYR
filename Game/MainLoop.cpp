#include "Game/MainLoop.h"
#include "Game/Game.h"
#include "Game/Externs.h"
#include "Game/CopyProtection.h"

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
    // In the original engine:
    //   1. Iterate over all AI-controlled houses
    //   2. Run base building logic
    //   3. Run unit production logic
    //   4. Run attack planning logic
    //   5. Run defense planning logic
    //   6. Run harvesting logic
    //   7. Process AI triggers
    //   8. Process AI team scripts
    //   9. Run skirmish AI behaviors
    //
    // The AI update is throttled — not every house is processed
    // every frame.  Houses are distributed across frames to avoid
    // performance spikes.
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Objects
// ═══════════════════════════════════════════════════════════════════════════

void Update_Objects()
{
    // In the original engine:
    //   1. Iterate over all active game objects
    //   2. Call ObjectClass::Update() for each object
    //   3. Process object state machines
    //   4. Handle object creation and destruction queues
    //   5. Process object cleanup for destroyed objects
    //   6. Update object visibility
    //
    // Object updates are distributed across frames to maintain
    // consistent performance.  The update order is:
    //   Buildings → Infantry → Vehicles → Aircraft → Projectiles
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Combat
// ═══════════════════════════════════════════════════════════════════════════

void Update_Combat()
{
    // In the original engine:
    //   1. Process all active bullets/projectiles
    //   2. Check for collision between bullets and targets
    //   3. Apply damage through warhead logic
    //   4. Process area-of-effect damage
    //   5. Handle weapon reload timers
    //   6. Process targeting logic (acquire new targets)
    //   7. Handle weapon firing sequences
    //   8. Process death animations and cleanup
    //   9. Process veterancy promotions
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Locomotion
// ═══════════════════════════════════════════════════════════════════════════

void Update_Locomotion()
{
    // In the original engine:
    //   1. Iterate over all mobile objects
    //   2. Call LocomotionClass::Update() for each
    //   3. Process pathfinding updates
    //   4. Handle movement along waypoints
    //   5. Process unit formation movement
    //   6. Handle collision avoidance
    //   7. Update facing direction
    //   8. Process terrain-based movement modifiers
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
    // In the original engine:
    //   1. Iterate over all houses with superweapons
    //   2. Update superweapon charge timers
    //   3. Process superweapon targeting
    //   4. Handle superweapon activation
    //   5. Process superweapon effects (nuke, lightning, etc.)
    //   6. Update superweapon UI indicators
    //   7. Handle superweapon cooldown
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_SpecialEffects
// ═══════════════════════════════════════════════════════════════════════════

void Update_SpecialEffects()
{
    // In the original engine:
    //   1. Process screen shake effects
    //   2. Process palette effects (lightning flashes, etc.)
    //   3. Process map reveal effects
    //   4. Process weather effects
    //   5. Process ion storm effects
    //   6. Process chrono effects
    //   7. Process iron curtain effects
    //   8. Process mind control effects
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Radar
// ═══════════════════════════════════════════════════════════════════════════

void Update_Radar()
{
    if (TheRadar == nullptr)
        return;

    // In the original engine:
    //   1. Update radar visibility
    //   2. Re-render radar surface if needed
    //   3. Process radar events (clicks)
    //   4. Handle radar jamming updates
    //   5. Update spy-plane radar sweep
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

    // In the original engine, the rendering pipeline is:
    //
    // 1. Clear back buffer
    // 2. Render terrain layer (isometric tiles)
    //    - Determine visible cells based on scroll position
    //    - Render terrain tiles (theater-specific)
    //    - Render cell overlays (ore, craters, etc.)
    // 3. Render object layer
    //    - Render buildings (sorted by position)
    //    - Render infantry (sorted by position)
    //    - Render vehicles (sorted by position)
    //    - Render aircraft (sorted by altitude)
    // 4. Render effect layer
    //    - Render animations
    //    - Render particles
    //    - Render superweapon effects
    // 5. Render UI layer
    //    - Render sidebar
    //    - Render radar/minimap
    //    - Render bottom bar
    //    - Render tooltips
    //    - Render cursor
    // 6. Render overlay layer
    //    - Render fog of war
    //    - Render shroud
    //    - Render selection boxes
    //    - Render health bars
    //    - Render waypoint lines
    // 7. Flip / present back buffer to screen
    //
    // The rendering is done in a specific order to ensure correct
    // z-ordering and blending.

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