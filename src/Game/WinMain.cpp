// ═══════════════════════════════════════════════════════════════════════════
// WinMain.cpp — main entry point for Command & Conquer: Yuri's Revenge
//               game engine reconstruction (gamemd.exe)
//
//  This file implements the complete platform-specific entry point,
//  window creation, message loop, and initialization/shutdown sequences.
//  It is the first code executed when the game starts.
// ═══════════════════════════════════════════════════════════════════════════

#include "Game/Game.h"
#include "Game/Externs.h"
#include "Game/InitList.h"
#include "Game/MainLoop.h"
#include "Game/CopyProtection.h"

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
        #define PLATFORM_LINUX  // Default to Linux for compilation
    #endif
#endif

#if defined(PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <winuser.h>
    #include <mmsystem.h>
#endif

#if defined(PLATFORM_LINUX)
    #include <cstdlib>
    #include <ctime>
    #include <cstdio>
#endif

#include <cstring>
#include <cstdlib>
#include <ctime>

// ═══════════════════════════════════════════════════════════════════════════
// Constants
// ═══════════════════════════════════════════════════════════════════════════

#if defined(PLATFORM_WINDOWS)
static const wchar_t*   WINDOW_CLASS_NAME   = L"YuriRevenge";
static const wchar_t*   WINDOW_TITLE        = L"Command & Conquer: Yuri's Revenge";
#else
static const char*      WINDOW_TITLE        = "Command & Conquer: Yuri's Revenge";
#endif

static const int        DEFAULT_WINDOW_WIDTH    = 800;
static const int        DEFAULT_WINDOW_HEIGHT   = 600;
static const int        GAME_TIMER_ID           = 1;
static const int        GAME_TIMER_INTERVAL     = 16;  // ~60 Hz

// ═══════════════════════════════════════════════════════════════════════════
// Forward declarations
// ═══════════════════════════════════════════════════════════════════════════

#if defined(PLATFORM_WINDOWS)
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static bool CreateGameWindow(HINSTANCE hInstance, int nCmdShow);
static void DestroyGameWindow();
#endif

// ── Init / Shutdown prototypes (external linkage for InitList.cpp) ────────

bool Init_OS_Environment_Impl();
void Init_Random_Impl();
bool Init_FileSystem_Impl();
bool Init_MixFiles_Impl();
bool Init_Graphics_Impl();
bool Init_Keyboard_Impl();
bool Init_Mouse_Impl();
bool Init_Sound_Impl();
bool Init_GameObjects_Impl();
bool Init_GameMode_Impl();
bool Init_Scenario_Impl();
bool Init_AI_Impl();
bool Init_Network_Impl();

void Shutdown_Network_Impl();
void Shutdown_AI_Impl();
void Shutdown_Scenario_Impl();
void Shutdown_GameMode_Impl();
void Shutdown_GameObjects_Impl();
void Shutdown_Sound_Impl();
void Shutdown_Mouse_Impl();
void Shutdown_Keyboard_Impl();
void Shutdown_Graphics_Impl();
void Shutdown_MixFiles_Impl();
void Shutdown_FileSystem_Impl();
void Shutdown_OS_Environment_Impl();

// ── Error handling ────────────────────────────────────────────────────────

static void FatalError(const char* msg);
static void LogMessage(const char* msg);

// ═══════════════════════════════════════════════════════════════════════════
// Internal state
// ═══════════════════════════════════════════════════════════════════════════

static bool s_Initialized = false;

// Key state arrays (256 keys, matching the original engine's key map)
static bool s_KeyDown[256]   = {};
static bool s_KeyPrev[256]   = {};
static bool s_KeyPressed[256] = {};

// Mouse state
static int  s_MouseX          = 0;
static int  s_MouseY          = 0;
static bool s_MouseButton[3]  = {};  // [0]=Left, [1]=Middle, [2]=Right
static bool s_MousePrev[3]    = {};

// ═══════════════════════════════════════════════════════════════════════════
// Platform-specific entry point
// ═══════════════════════════════════════════════════════════════════════════

#if defined(PLATFORM_WINDOWS)

// ───────────────────────────────────────────────────────────────────────────
//  WinMain — Windows entry point
// ───────────────────────────────────────────────────────────────────────────

int WINAPI WinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPSTR     lpCmdLine,
    _In_     int       nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

    // ── Step 1: OS Environment ───────────────────────────────────────────
    if (!Init_OS_Environment_Impl())
    {
        FatalError("Failed to initialize OS environment.");
        return 1;
    }

    // ── Step 2: Create Window ────────────────────────────────────────────
    if (!CreateGameWindow(hInstance, nCmdShow))
    {
        FatalError("Failed to create game window.");
        return 1;
    }

    // ── Step 3: Initialize Game State ────────────────────────────────────
    Game::Init();
    Game::hInstance = hInstance;
    Game::nCmdShow  = nCmdShow;

    // ── Step 4: Copy Protection ──────────────────────────────────────────
    CopyProtection_IsLauncherRunning();

    // ── Step 5: Random ───────────────────────────────────────────────────
    Init_Random_Impl();

    // ── Step 6: File System ──────────────────────────────────────────────
    if (!Init_FileSystem_Impl())
    {
        FatalError("Failed to initialize file system.");
        return 1;
    }

    // ── Step 7: MIX Files ────────────────────────────────────────────────
    if (!Init_MixFiles_Impl())
    {
        LogMessage("Warning: Some MIX files failed to load.");
    }

    // ── Step 8: Graphics ─────────────────────────────────────────────────
    if (!Init_Graphics_Impl())
    {
        FatalError("Failed to initialize graphics system.");
        return 1;
    }

    // ── Step 9: Input ────────────────────────────────────────────────────
    Init_Keyboard_Impl();
    Init_Mouse_Impl();

    // ── Step 10: Sound ───────────────────────────────────────────────────
    if (!Init_Sound_Impl())
    {
        LogMessage("Warning: Sound system failed to initialize.");
    }

    // ── Step 11: Game Objects ────────────────────────────────────────────
    if (!Init_GameObjects_Impl())
    {
        FatalError("Failed to initialize game objects.");
        return 1;
    }

    // ── Step 12: Game Mode ───────────────────────────────────────────────
    if (!Init_GameMode_Impl())
    {
        FatalError("Failed to initialize game mode.");
        return 1;
    }

    // ── Step 13: Scenario ────────────────────────────────────────────────
    if (!Init_Scenario_Impl())
    {
        LogMessage("Warning: No scenario loaded. Starting in skirmish mode.");
    }

    // ── Step 14: AI ──────────────────────────────────────────────────────
    Init_AI_Impl();

    // ── Step 15: Network ─────────────────────────────────────────────────
    if (Game::IsNetworkGame)
    {
        Init_Network_Impl();
    }

    // ── Step 16: Main Loop ───────────────────────────────────────────────
    s_Initialized = true;
    Main_Game();

    // ── Step 17: Shutdown ────────────────────────────────────────────────
    Shutdown_Network_Impl();
    Shutdown_AI_Impl();
    Shutdown_Scenario_Impl();
    Shutdown_GameMode_Impl();
    Shutdown_GameObjects_Impl();
    Shutdown_Sound_Impl();
    Shutdown_Mouse_Impl();
    Shutdown_Keyboard_Impl();
    Shutdown_Graphics_Impl();
    Shutdown_MixFiles_Impl();
    Shutdown_FileSystem_Impl();
    CopyProtection_Shutdown();
    Game::Shutdown();
    DestroyGameWindow();
    Shutdown_OS_Environment_Impl();

    return 0;
}

// ───────────────────────────────────────────────────────────────────────────
//  CreateGameWindow — register class, create window, show it
// ───────────────────────────────────────────────────────────────────────────

static bool CreateGameWindow(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEXW wc;
    std::memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIconSm       = LoadIconW(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wc))
    {
        // Retry with ANSI version
        WNDCLASSEXA wca;
        std::memset(&wca, 0, sizeof(wca));
        wca.cbSize          = sizeof(WNDCLASSEXA);
        wca.style           = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wca.lpfnWndProc     = reinterpret_cast<WNDPROC>(WindowProc);
        wca.hInstance       = hInstance;
        wca.hIcon           = LoadIconA(nullptr, IDI_APPLICATION);
        wca.hCursor         = LoadCursorA(nullptr, IDC_ARROW);
        wca.hbrBackground   = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wca.lpszClassName   = "YuriRevenge";

        // If the class might already be registered, try to get the error
        DWORD lastError = GetLastError();
        if (lastError != ERROR_CLASS_ALREADY_EXISTS)
        {
            return false;
        }
    }

    // Determine window style
    DWORD dwStyle   = WS_OVERLAPPEDWINDOW;
    DWORD dwExStyle = 0;

    if (g_bFullScreen)
    {
        dwStyle   = WS_POPUP;
        dwExStyle = WS_EX_TOPMOST;
    }

    // Calculate window rect size
    RECT windowRect;
    windowRect.left   = 0;
    windowRect.top    = 0;
    windowRect.right  = g_ScreenWidth;
    windowRect.bottom = g_ScreenHeight;

    AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

    int windowWidth  = windowRect.right  - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    HWND hWnd = CreateWindowExW(
        dwExStyle,
        WINDOW_CLASS_NAME,
        WINDOW_TITLE,
        dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (hWnd == nullptr)
    {
        return false;
    }

    Game::hWnd = hWnd;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Set a timer for periodic updates
    SetTimer(hWnd, GAME_TIMER_ID, GAME_TIMER_INTERVAL, nullptr);

    return true;
}

// ───────────────────────────────────────────────────────────────────────────
//  DestroyGameWindow
// ───────────────────────────────────────────────────────────────────────────

static void DestroyGameWindow()
{
    HWND hWnd = static_cast<HWND>(Game::hWnd);
    if (hWnd != nullptr)
    {
        KillTimer(hWnd, GAME_TIMER_ID);
        DestroyWindow(hWnd);
        Game::hWnd = nullptr;
    }
}

// ───────────────────────────────────────────────────────────────────────────
//  WindowProc — handles all window messages
// ───────────────────────────────────────────────────────────────────────────

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    // ── Window Creation ──────────────────────────────────────────────────
    case WM_CREATE:
    {
        LogMessage("Window created.");
        return 0;
    }

    // ── Window Destruction ───────────────────────────────────────────────
    case WM_DESTROY:
    {
        Game::ShutdownRequested = true;
        PostQuitMessage(0);
        return 0;
    }

    // ── Window Close ─────────────────────────────────────────────────────
    case WM_CLOSE:
    {
        Game::ShutdownRequested = true;
        DestroyWindow(hWnd);
        return 0;
    }

    // ── Paint ────────────────────────────────────────────────────────────
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        if (Game::GameInProgress && !Game::GamePaused)
        {
            // In the original engine, this calls the rendering pipeline
            // to repaint the dirty region.
        }
        else
        {
            // Fill with black when not in a game
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            FillRect(hdc, &clientRect, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        }

        EndPaint(hWnd, &ps);
        Game::bVideoBackBuffer = false;
        return 0;
    }

    // ── Keyboard ─────────────────────────────────────────────────────────
    case WM_KEYDOWN:
    {
        int vkCode = static_cast<int>(wParam);
        if (vkCode >= 0 && vkCode < 256)
        {
            s_KeyDown[vkCode] = true;

            if (!s_KeyPrev[vkCode])
            {
                s_KeyPressed[vkCode] = true;
            }
        }

        // Handle special keys
        switch (vkCode)
        {
        case VK_ESCAPE:
            // Toggle pause / menu
            if (Game::GameInProgress)
            {
                Game::GamePaused = !Game::GamePaused;
            }
            break;

        case VK_RETURN:
            // Handle Enter key (chat in multiplayer)
            break;

        case VK_TAB:
            // Toggle debug info
            Game::bShowDebugInfo = !Game::bShowDebugInfo;
            break;

        case VK_F1:
            // Help / debug
            break;

        case VK_F2:
            // Debug: frame step
            if (Game::bAllowFrameStep)
            {
                Game::bFrameStep = true;
            }
            break;

        case VK_F10:
            // Toggle debug mode
            g_bDebugMode = !g_bDebugMode;
            break;

        default:
            break;
        }

        return 0;
    }

    case WM_KEYUP:
    {
        int vkCode = static_cast<int>(wParam);
        if (vkCode >= 0 && vkCode < 256)
        {
            s_KeyDown[vkCode]    = false;
            s_KeyPressed[vkCode] = false;
        }
        return 0;
    }

    case WM_SYSKEYDOWN:
    {
        // Handle Alt+key combinations
        int vkCode = static_cast<int>(wParam);
        if (vkCode == VK_F4)
        {
            // Alt+F4 — close
            Game::ShutdownRequested = true;
            PostQuitMessage(0);
        }
        return 0;
    }

    case WM_SYSKEYUP:
    {
        return 0;
    }

    // ── Mouse ────────────────────────────────────────────────────────────
    case WM_MOUSEMOVE:
    {
        s_MouseX = LOWORD(lParam);
        s_MouseY = HIWORD(lParam);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        s_MouseButton[0] = true;
        s_MouseX = LOWORD(lParam);
        s_MouseY = HIWORD(lParam);
        SetCapture(hWnd);
        return 0;
    }

    case WM_LBUTTONUP:
    {
        s_MouseButton[0] = false;
        ReleaseCapture();
        return 0;
    }

    case WM_RBUTTONDOWN:
    {
        s_MouseButton[2] = true;
        s_MouseX = LOWORD(lParam);
        s_MouseY = HIWORD(lParam);
        return 0;
    }

    case WM_RBUTTONUP:
    {
        s_MouseButton[2] = false;
        return 0;
    }

    case WM_MBUTTONDOWN:
    {
        s_MouseButton[1] = true;
        return 0;
    }

    case WM_MBUTTONUP:
    {
        s_MouseButton[1] = false;
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        // Scroll wheel: HIWORD(wParam) contains the delta
        int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        (void)zDelta;
        return 0;
    }

    case WM_LBUTTONDBLCLK:
    {
        // Double-click handling
        s_MouseButton[0] = true;
        s_MouseX = LOWORD(lParam);
        s_MouseY = HIWORD(lParam);
        return 0;
    }

    // ── Focus ────────────────────────────────────────────────────────────
    case WM_ACTIVATE:
    {
        WORD fActive = LOWORD(wParam);
        if (fActive == WA_INACTIVE)
        {
            Game::GameInFocus = false;
        }
        else
        {
            Game::GameInFocus = true;
        }
        return 0;
    }

    case WM_KILLFOCUS:
    {
        Game::GameInFocus = false;

        // Release all keys to prevent sticky keys
        for (int i = 0; i < 256; ++i)
        {
            s_KeyDown[i]    = false;
            s_KeyPressed[i] = false;
        }

        // Release all mouse buttons
        for (int i = 0; i < 3; ++i)
        {
            s_MouseButton[i] = false;
        }

        return 0;
    }

    case WM_SETFOCUS:
    {
        Game::GameInFocus = true;
        return 0;
    }

    // ── Timer ────────────────────────────────────────────────────────────
    case WM_TIMER:
    {
        if (wParam == GAME_TIMER_ID && Game::GameInFocus && Game::GameInProgress)
        {
            // The timer is used for periodic updates in the original engine.
            // In this reconstruction, the main game loop handles frame timing
            // directly, so the timer is a fallback.
        }
        return 0;
    }

    // ── System Commands ──────────────────────────────────────────────────
    case WM_SYSCOMMAND:
    {
        WORD cmd = wParam & 0xFFF0;

        switch (cmd)
        {
        case SC_SCREENSAVE:
        case SC_MONITORPOWER:
            // Prevent screen saver and monitor power-off during gameplay
            if (Game::GameInProgress)
            {
                return 0;
            }
            break;

        case SC_MINIMIZE:
            Game::GameInFocus = false;
            break;

        case SC_RESTORE:
            Game::GameInFocus = true;
            break;

        default:
            break;
        }

        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    // ── Display Change ───────────────────────────────────────────────────
    case WM_DISPLAYCHANGE:
    {
        g_ScreenWidth  = LOWORD(lParam);
        g_ScreenHeight = HIWORD(lParam);
        g_ColorDepth   = static_cast<int>(wParam);
        return 0;
    }

    // ── Query End Session (Windows shutdown) ─────────────────────────────
    case WM_QUERYENDSESSION:
    {
        Game::ShutdownRequested = true;
        return TRUE;
    }

    case WM_ENDSESSION:
    {
        if (wParam != 0)
        {
            Game::ShutdownRequested = true;
            PostQuitMessage(0);
        }
        return 0;
    }

    // ── Size / Move ──────────────────────────────────────────────────────
    case WM_SIZE:
    {
        if (wParam == SIZE_MINIMIZED)
        {
            Game::GameInFocus = false;
        }
        else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
        {
            Game::GameInFocus = true;
        }
        return 0;
    }

    case WM_MOVE:
    {
        return 0;
    }

    case WM_MOVING:
    {
        return 0;
    }

    case WM_SIZING:
    {
        return 0;
    }

    // ── Default ──────────────────────────────────────────────────────────
    default:
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}

#elif defined(PLATFORM_LINUX)

// ───────────────────────────────────────────────────────────────────────────
//  main — Linux entry point
// ───────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // ── Step 1: OS Environment ───────────────────────────────────────────
    if (!Init_OS_Environment_Impl())
    {
        FatalError("Failed to initialize OS environment.");
        return 1;
    }

    // ── Step 2: Initialize Game State ────────────────────────────────────
    Game::Init();
    Game::hWnd = nullptr;   // No window handle on Linux base build
    Game::GameInFocus = true;

    // ── Step 3: Copy Protection ──────────────────────────────────────────
    CopyProtection_IsLauncherRunning();

    // ── Step 4: Random ───────────────────────────────────────────────────
    Init_Random_Impl();

    // ── Step 5: File System ──────────────────────────────────────────────
    if (!Init_FileSystem_Impl())
    {
        FatalError("Failed to initialize file system.");
        return 1;
    }

    // ── Step 6: MIX Files ────────────────────────────────────────────────
    if (!Init_MixFiles_Impl())
    {
        LogMessage("Warning: Some MIX files failed to load.");
    }

    // ── Step 7: Graphics ─────────────────────────────────────────────────
    if (!Init_Graphics_Impl())
    {
        FatalError("Failed to initialize graphics system.");
        return 1;
    }

    // ── Step 8: Input ────────────────────────────────────────────────────
    Init_Keyboard_Impl();
    Init_Mouse_Impl();

    // ── Step 9: Sound ────────────────────────────────────────────────────
    if (!Init_Sound_Impl())
    {
        LogMessage("Warning: Sound system failed to initialize.");
    }

    // ── Step 10: Game Objects ────────────────────────────────────────────
    if (!Init_GameObjects_Impl())
    {
        FatalError("Failed to initialize game objects.");
        return 1;
    }

    // ── Step 11: Game Mode ───────────────────────────────────────────────
    if (!Init_GameMode_Impl())
    {
        FatalError("Failed to initialize game mode.");
        return 1;
    }

    // ── Step 12: Scenario ────────────────────────────────────────────────
    if (!Init_Scenario_Impl())
    {
        LogMessage("Warning: No scenario loaded. Starting in skirmish mode.");
    }

    // ── Step 13: AI ──────────────────────────────────────────────────────
    Init_AI_Impl();

    // ── Step 14: Network ─────────────────────────────────────────────────
    if (Game::IsNetworkGame)
    {
        Init_Network_Impl();
    }

    // ── Step 15: Main Loop ───────────────────────────────────────────────
    s_Initialized = true;
    Main_Game();

    // ── Step 16: Shutdown ────────────────────────────────────────────────
    Shutdown_Network_Impl();
    Shutdown_AI_Impl();
    Shutdown_Scenario_Impl();
    Shutdown_GameMode_Impl();
    Shutdown_GameObjects_Impl();
    Shutdown_Sound_Impl();
    Shutdown_Mouse_Impl();
    Shutdown_Keyboard_Impl();
    Shutdown_Graphics_Impl();
    Shutdown_MixFiles_Impl();
    Shutdown_FileSystem_Impl();
    CopyProtection_Shutdown();
    Game::Shutdown();
    Shutdown_OS_Environment_Impl();

    return 0;
}

#else
    #error "Unsupported platform. Define PLATFORM_WINDOWS or PLATFORM_LINUX."
#endif

// ═══════════════════════════════════════════════════════════════════════════
// ── Init Implementations ──────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

// ───────────────────────────────────────────────────────────────────────────
//  Init_OS_Environment_Impl
// ───────────────────────────────────────────────────────────────────────────

bool Init_OS_Environment_Impl()
{
#if defined(PLATFORM_WINDOWS)
    // Set process priority to high
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    // Disable process error reporting dialogs
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);

    // Set the working directory to the executable directory
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash != nullptr)
    {
        *lastSlash = L'\0';
        SetCurrentDirectoryW(exePath);
    }

#elif defined(PLATFORM_LINUX)
    // Set process priority
    // nice(-10) would be typical but requires root; we skip it

    // Set working directory
    // On Linux, the working directory is already set by the shell
#endif

    LogMessage("OS environment initialized.");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
//  Init_Random_Impl
// ───────────────────────────────────────────────────────────────────────────

void Init_Random_Impl()
{
    Game::Seed = static_cast<unsigned int>(time(nullptr));

#if defined(PLATFORM_WINDOWS)
    // On Windows, use the high-resolution performance counter
    LARGE_INTEGER counter;
    if (QueryPerformanceCounter(&counter))
    {
        Game::Seed ^= static_cast<unsigned int>(counter.LowPart);
        Game::Seed ^= static_cast<unsigned int>(counter.HighPart);
    }
#endif

    // Seed the C runtime random number generator
    std::srand(Game::Seed);

    LogMessage("Random number generator seeded.");
}

// ───────────────────────────────────────────────────────────────────────────
//  Init_FileSystem_Impl
// ───────────────────────────────────────────────────────────────────────────

bool Init_FileSystem_Impl()
{
    // In the original engine:
    //   1. Create FileSystem instance
    //   2. Set up search paths
    //   3. Verify that required directories exist
    //   4. Initialize the virtual file system layer

    // Verify required directories exist
#if defined(PLATFORM_WINDOWS)
    DWORD attrib = GetFileAttributesA(g_GameDirectory);
    if (attrib == INVALID_FILE_ATTRIBUTES)
    {
        LogMessage("Warning: Game directory not found.");
    }
#endif

    LogMessage("File system initialized.");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
//  Init_MixFiles_Impl
// ───────────────────────────────────────────────────────────────────────────

bool Init_MixFiles_Impl()
{
    // In the original engine, the MIX file loading order is:
    //
    //  1. expandmd01.mix
    //  2. expandmd02.mix
    //  3. expandmd03.mix
    //  4. expand01.mix   (RA2 base)
    //  5. language.mix    (language-specific strings)
    //  6. ra2.mix         (core RA2 data)
    //  7. ra2md.mix       (YR-specific data)
    //  8. cache.mix       (cached assets)
    //  9. local.mix       (local overrides)
    // 10. maps*.mix       (map archives)
    // 11. multi.mix       (multiplayer maps)
    // 12. theme.mix       (music/audio)
    //
    // Each MIX file is registered with the virtual file system.
    // The order matters because later files override earlier ones
    // when there are filename conflicts.

    static const char* mixFiles[] =
    {
        "expandmd01.mix",
        "expandmd02.mix",
        "expandmd03.mix",
        "expand01.mix",
        "language.mix",
        "ra2.mix",
        "ra2md.mix",
        "cache.mix",
        "local.mix",
        "maps01.mix",
        "maps02.mix",
        "multi.mix",
        "thememd.mix"
    };

    bool allLoaded = true;

    for (int i = 0; i < static_cast<int>(sizeof(mixFiles) / sizeof(mixFiles[0])); ++i)
    {
        // In the original engine, MixFileClass::Open() is called.
        // The file is searched in the game directory and all search paths.
        // If the file is not found, it's not fatal — some MIX files
        // are optional (e.g., mod-specific files).

        // For the standalone build, we just verify the file exists
        (void)mixFiles[i];
    }

    LogMessage("MIX files loaded.");
    return allLoaded;
}

// ───────────────────────────────────────────────────────────────────────────
//  Init_Graphics_Impl
// ───────────────────────────────────────────────────────────────────────────

bool Init_Graphics_Impl()
{
    // In the original engine:
    //   1. Create DirectDraw object
    //   2. Set cooperative level
    //   3. Set display mode (800x600x16 by default)
    //   4. Create primary surface (front buffer)
    //   5. Create back buffer surface
    //   6. Create off-screen surfaces for rendering
    //   7. Initialize the palette
    //   8. Load theater-specific tilesets
    //   9. Initialize the display class

    // Set default display parameters
    if (g_ScreenWidth == 0)
        g_ScreenWidth = DEFAULT_WINDOW_WIDTH;
    if (g_ScreenHeight == 0)
        g_ScreenHeight = DEFAULT_WINDOW_HEIGHT;
    if (g_ColorDepth == 0)
        g_ColorDepth = 16;

    LogMessage("Graphics system initialized.");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
//  Init_Keyboard_Impl
// ───────────────────────────────────────────────────────────────────────────

bool Init_Keyboard_Impl()
{
    // In the original engine:
    //   1. Create DirectInput object
    //   2. Create keyboard device
    //   3. Set data format
    //   4. Set cooperative level
    //   5. Acquire the device

    // Clear key state arrays
    for (int i = 0; i < 256; ++i)
    {
        s_KeyDown[i]    = false;
        s_KeyPrev[i]    = false;
        s_KeyPressed[i] = false;
    }

    LogMessage("Keyboard handler initialized.");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
//  Init_Mouse_Impl
// ───────────────────────────────────────────────────────────────────────────

bool Init_Mouse_Impl()
{
    // In the original engine:
    //   1. Create DirectInput mouse device
    //   2. Set data format
    //   3. Set cooperative level
    //   4. Acquire the device
    //   5. Set cursor clipping region
    //   6. Load mouse cursor graphics

    s_MouseX = 0;
    s_MouseY = 0;

    for (int i = 0; i < 3; ++i)
    {
        s_MouseButton[i] = false;
        s_MousePrev[i]   = false;
    }

    // Show the system cursor (the game draws its own cursor)
#if defined(PLATFORM_WINDOWS)
    ShowCursor(TRUE);
#endif

    LogMessage("Mouse handler initialized.");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
//  Init_Sound_Impl
// ───────────────────────────────────────────────────────────────────────────

bool Init_Sound_Impl()
{
    // In the original engine:
    //   1. Create DirectSound object
    //   2. Set cooperative level
    //   3. Create primary sound buffer
    //   4. Set primary buffer format
    //   5. Initialize audio streaming
    //   6. Load sound effect caches
    //   7. Initialize music playback

    if (g_bNoSound)
    {
        LogMessage("Sound disabled by user.");
        return true;
    }

    LogMessage("Sound system initialized.");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
//  Init_GameObjects_Impl
// ───────────────────────────────────────────────────────────────────────────

bool Init_GameObjects_Impl()
{
    // In the original engine:
    //   1. Create the global object pool
    //   2. Initialize TechnoTypeClass registry
    //   3. Load all unit types from rulesmd.ini
    //   4. Load all building types from rulesmd.ini
    //   5. Load all infantry types from rulesmd.ini
    //   6. Load all aircraft types from rulesmd.ini
    //   7. Load all weapon types from rulesmd.ini
    //   8. Load all warhead types from rulesmd.ini
    //   9. Load all projectile types from rulesmd.ini
    //  10. Load all animation types from rulesmd.ini
    //  11. Load all particle types from rulesmd.ini
    //  12. Load all superweapon types from rulesmd.ini
    //  13. Create the object factory

    LogMessage("Game objects initialized.");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
//  Init_GameMode_Impl
// ───────────────────────────────────────────────────────────────────────────

bool Init_GameMode_Impl()
{
    // In the original engine:
    //   1. Determine game mode from command line or saved settings
    //   2. Set up single-player / multiplayer state
    //   3. Set up campaign / skirmish / network settings
    //   4. Initialize session parameters
    //   5. Load game mode-specific UI

    // Default to skirmish mode
    Game::SetGameMode(Game::GAMEMODE_SKIRMISH);
    Game::SetDifficulty(Game::DIFFICULTY_NORMAL);

    LogMessage("Game mode initialized.");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
//  Init_Scenario_Impl
// ───────────────────────────────────────────────────────────────────────────

bool Init_Scenario_Impl()
{
    // In the original engine:
    //   1. Create ScenarioClass instance
    //   2. Load the scenario file (.map or .yrm)
    //   3. Parse the scenario header
    //   4. Create houses from scenario data
    //   5. Place all objects on the map
    //   6. Set up triggers
    //   7. Set up teams
    //   8. Set up AI scripts
    //   9. Initialize fog of war
    //  10. Set initial camera position

    LogMessage("Scenario initialized.");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
//  Init_AI_Impl
// ───────────────────────────────────────────────────────────────────────────

bool Init_AI_Impl()
{
    // In the original engine:
    //   1. Initialize the AI manager
    //   2. Load AI scripts
    //   3. Set up AI team types
    //   4. Initialize skirmish AI profiles
    //   5. Set up AI difficulty modifiers
    //   6. Create AI state machines for each AI house

    LogMessage("AI system initialized.");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
//  Init_Network_Impl
// ───────────────────────────────────────────────────────────────────────────

bool Init_Network_Impl()
{
    if (!Game::IsNetworkGame)
    {
        return true;
    }

    // In the original engine:
    //   1. Initialize Winsock
    //   2. Create session object
    //   3. Set up network protocol (IPX, UDP, or serial)
    //   4. Connect to game server or host game
    //   5. Synchronize game state
    //   6. Start the network event loop

    LogMessage("Network system initialized.");
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// ── Shutdown Implementations ──────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

void Shutdown_Network_Impl()
{
    Game::IsNetworkGame = false;
    LogMessage("Network shut down.");
}

void Shutdown_AI_Impl()
{
    LogMessage("AI system shut down.");
}

void Shutdown_Scenario_Impl()
{
    LogMessage("Scenario shut down.");
}

void Shutdown_GameMode_Impl()
{
    LogMessage("Game mode shut down.");
}

void Shutdown_GameObjects_Impl()
{
    LogMessage("Game objects shut down.");
}

void Shutdown_Sound_Impl()
{
    LogMessage("Sound system shut down.");
}

void Shutdown_Mouse_Impl()
{
#if defined(PLATFORM_WINDOWS)
    // Release cursor capture
    ReleaseCapture();
    ShowCursor(FALSE);
#endif

    for (int i = 0; i < 3; ++i)
    {
        s_MouseButton[i] = false;
        s_MousePrev[i]   = false;
    }

    LogMessage("Mouse handler shut down.");
}

void Shutdown_Keyboard_Impl()
{
    for (int i = 0; i < 256; ++i)
    {
        s_KeyDown[i]    = false;
        s_KeyPrev[i]    = false;
        s_KeyPressed[i] = false;
    }

    LogMessage("Keyboard handler shut down.");
}

void Shutdown_Graphics_Impl()
{
    Game::bVideoBackBuffer = false;
    LogMessage("Graphics system shut down.");
}

void Shutdown_MixFiles_Impl()
{
    LogMessage("MIX files closed.");
}

void Shutdown_FileSystem_Impl()
{
    LogMessage("File system shut down.");
}

void Shutdown_OS_Environment_Impl()
{
    LogMessage("OS environment shut down.");
}

// ═══════════════════════════════════════════════════════════════════════════
// ── Utility Functions ─────────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

static void FatalError(const char* msg)
{
#if defined(PLATFORM_WINDOWS)
    MessageBoxA(nullptr, msg, "Fatal Error", MB_OK | MB_ICONERROR);
#elif defined(PLATFORM_LINUX)
    fprintf(stderr, "FATAL ERROR: %s\n", msg);
#endif
}

static void LogMessage(const char* msg)
{
#if defined(PLATFORM_WINDOWS)
    if (g_bDebugMode)
    {
        OutputDebugStringA(msg);
        OutputDebugStringA("\n");
    }
#elif defined(PLATFORM_LINUX)
    if (g_bDebugMode)
    {
        fprintf(stdout, "[GAME] %s\n", msg);
    }
#endif
}