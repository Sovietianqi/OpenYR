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

// ── 第 11 步（GameObjects_Impl）用到的类型 ────────────────────────────────
//   Externs.h 已包含绝大多数对象 / 类型头；这里补齐它没有覆盖的部分：
//   规则表本体、INI 与 MIX 读取、AI 类型表、以及渲染侧的配色表。
#include "INI/INIClass.h"
#include "IO/CCFileClass.h"
#include "IO/FileSystem.h"
#include "IO/MixFileClass.h"
#include "Rules/RulesClass.h"

#include "Houses/HouseClass.h"
#include "Houses/HouseTypeClass.h"
#include "Houses/SideClass.h"
#include "Houses/FactoryClass.h"
#include "Objects/IsometricTile.h"

#include "AI/TeamTypeClass.h"
#include "AI/TaskForceClass.h"
#include "AI/ScriptTypeClass.h"
#include "AI/TriggerTypeClass.h"
#include "AI/TagTypeClass.h"
#include "AI/TActionClass.h"
#include "AI/TEventClass.h"
#include "AI/AITeamClass.h"
#include "AI/AITeamTypeClass.h"
#include "AI/AITriggerTypeClass.h"

#include "Combat/BombClass.h"
#include "Combat/RadBeamClass.h"
#include "Combat/WeaponWaveClass.h"

#include "Particles/ParticleClass.h"
#include "Particles/ParticleSystemClass.h"
#include "Animations/AnimClass.h"
#include "Combat/BulletClass.h"
#include "Particles/ParticleTypeClass.h"
#include "Particles/ParticleSystemTypeClass.h"

#include "Special/SlaveManagerClass.h"
#include "Special/VeinholeMonsterClass.h"

#include "Scenario/CampaignClass.h"
#include "Scenario/CoopCampaignClass.h"
#include "IO/MovieClass.h"
#include "Network/MPGameModeClass.h"

#include "Rendering/ConvertClass.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/GScreenClass.h"
#include "Rendering/GameScreenClass.h"
#include "Rendering/Surface.h"
#include "Rendering/TacticalClass.h"
#include "Rendering/RadarClass.h"
#include "Rendering/RadarEventClass.h"
#include "UI/KeyboardClass.h"
#include "Audio/VoxClass.h"

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
#include <cctype>
#include <vector>

// ── asm: IsoMapPack5 的 LZO 解压入口 ─────────────────────────────────────
//   原版 DisplayClass::Read_INI 用 LZOStraw 解 [IsoMapPack5] 的 UU 块；
//   本树同族解码器实体在 COM/CStreamClass.cpp（asm 0x55C0C0 区）。地图来自
//   外部文件、属于不可信输入，故用带 ip/op 边界的 safe 版本，不另建封装层。
#include "COM/CStreamClass.h"

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

// ── 全局表铺底 ────────────────────────────────────────────────────────────

namespace {

// 表指针的"存在即跳过"铺底。
//
// 原版每一张 vec_XXX 都由该类自己的静态构造函数（XXX_SCTOR）在 .data 段里
// 铺底：`[data]=0 [count]=0 [IsInitialized]=1 [vtable]=… [incrementAmount]=0Ah`。
// 重建侧对应的形态就是这里的 Ensure_Array(XXX::Array) 调用。凡是已有
// `static void Init_Array()` 的类，一律走那个入口（语义相同，且内部已幂等）。
template <typename T>
inline void Ensure_Array(DynamicVectorClass<T*>*& pArray)
{
    if (pArray == nullptr)
        pArray = new DynamicVectorClass<T*>();
}

// 供日志统计用的计数（表还没建立时返回 -1，冒烟日志据此判断铺底是否漏了）。
template <typename T>
inline int32 Count_Of(const DynamicVectorClass<T*>* pArray)
{
    return (pArray != nullptr) ? pArray->Count : -1;
}

}   // namespace

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

    // asm: Create_Main_Window (0x777CC0) 尾部 —— 焦点、热键、光标
    //   SetFocus(hWnd) ; RegisterHotKey(hWnd, 1, 7, 'M') ;
    //   SetCursor(LoadCursorA(hInstance, 68h)) ;
    //   dword_B7355C = ImmGetContext(hWnd) ; ImmAssociateContext(hWnd, 0)
    // IME 上下文在本工程不使用（不引入 imm32 依赖），其余三步照搬。
    SetFocus(hWnd);
    RegisterHotKey(hWnd, 1, 7, 'M');
    SetCursor(wc.hCursor);

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
// 根据游戏行为，可知 OS_Environment_Impl 负责下面这段逻辑。
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
// 根据游戏行为，可知 Random_Impl 负责下面这段逻辑。
// ───────────────────────────────────────────────────────────────────────────

void Init_Random_Impl()
{
    // 固定种子：回归测试要能逐帧比对，随机种子必须可复现。
    //   OPENYR_SEED=<十进制> 时跳过时间种子，原版行为不变（不设就一直用时间）。
    if (const char* pSeed = std::getenv("OPENYR_SEED"))
    {
        Game::Seed = static_cast<unsigned int>(std::strtoul(pSeed, nullptr, 10));
        std::srand(Game::Seed);
        LogMessage("Random number generator seeded from OPENYR_SEED.");
        return;
    }

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
// 根据游戏行为，可知 FileSystem_Impl 负责下面这段逻辑。
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
// BUILTIN_RULES_INI —— 内置兜底（无原版数据文件时使用）
//
//   本机没有 rulesmd.ini（散装或 MIX 内都读不到）时，用这段最小规则集顶替，
//   目的只有一个：让 Init_GameObjects_Impl 能把六张关键类型表各填进至少一项，
//   使冒烟测试能造出实体。它**不**替代真实 rules 数据。
//
//   键名逐字取自 rulesmd.ini 的段布局：类型列表段（[VehicleTypes] 等）只登记
//   名字，名字对应的段才是该类型的字段。数值取常见 YR 值（MTNK/E1/GAPILE 的
//   Strength / Cost / Speed / Armor / Primary 等）。
// ───────────────────────────────────────────────────────────────────────────

static const char BUILTIN_RULES_INI[] =
    "; 内置兜底（无原版数据文件时使用）\n"
    "[General]\n"
    "BaseUnit=MTNK\n"
    "\n"
    "[MultiplayerDialogSettings]\n"
    "MinMoney=5000\n"
    "Money=10000\n"
    "MaxMoney=10000\n"
    "MoneyIncrement=100\n"
    "MinUnitCount=10\n"
    "UnitCount=10\n"
    "MaxUnitCount=10\n"
    "TechLevel=10\n"
    "GameSpeed=2\n"
    "AIDifficulty=0\n"
    "AIPlayers=0\n"
    "Bases=yes\n"
    "Shroud=yes\n"
    "BridgeDestruction=yes\n"
    "AllyChangeAllowed=yes\n"
    "\n"
    "[VehicleTypes]\n"
    "0=MTNK\n"
    "\n"
    "[InfantryTypes]\n"
    "0=E1\n"
    "\n"
    "[BuildingTypes]\n"
    "0=GAPILE\n"
    "\n"
    "[Warheads]\n"
    "0=AP\n"
    "\n"
    "[Projectiles]\n"
    "0=Cannonball\n"
    "1=InvisibleLow\n"
    "\n"
    "[WeaponTypes]\n"
    "0=105mm\n"
    "1=M1Carbine\n"
    "\n"
    "[MTNK]\n"
    "UIName=Name:MTNK\n"
    "Name=Grizzly Battle Tank\n"
    "Image=MTNK\n"
    "Primary=105mm\n"
    "Strength=300\n"
    "Armor=heavy\n"
    "TechLevel=2\n"
    "Sight=6\n"
    "Speed=6\n"
    "ROT=5\n"
    "Cost=700\n"
    "Soylent=700\n"
    "Points=25\n"
    "Owner=Americans,Russians\n"
    "AllowedToStartInMultiplayer=yes\n"
    "\n"
    "[E1]\n"
    "UIName=Name:E1\n"
    "Name=G.I.\n"
    "Image=E1\n"
    "Primary=M1Carbine\n"
    "Strength=125\n"
    "Armor=none\n"
    "TechLevel=1\n"
    "Sight=5\n"
    "Speed=4\n"
    "Cost=200\n"
    "Soylent=200\n"
    "Points=10\n"
    "Owner=Americans,Russians\n"
    "AllowedToStartInMultiplayer=yes\n"
    "\n"
    "[GAPILE]\n"
    "UIName=Name:GAPILE\n"
    "Name=G.I. Barracks\n"
    "Image=GAPILE\n"
    "Strength=500\n"
    "Armor=wood\n"
    "TechLevel=1\n"
    "Sight=4\n"
    "Cost=500\n"
    "Soylent=500\n"
    "Points=50\n"
    "Power=0\n"
    "Owner=Americans\n"
    "\n"
    "[105mm]\n"
    "Damage=50\n"
    "ROF=60\n"
    "Range=6\n"
    "Speed=40\n"
    "Projectile=Cannonball\n"
    "Warhead=AP\n"
    "\n"
    "[M1Carbine]\n"
    "Damage=15\n"
    "ROF=20\n"
    "Range=4\n"
    "Speed=100\n"
    "Projectile=InvisibleLow\n"
    "Warhead=AP\n"
    "\n"
    "[AP]\n"
    "Verses=100%,25%,25%,75%,100%,100%,100%,50%,25%,100%,100%\n"
    "\n"
    "[Cannonball]\n"
    "Image=120MM\n"
    "\n"
    "[InvisibleLow]\n"
    "Inviso=yes\n";

// ───────────────────────────────────────────────────────────────────────────
// INI 读取：与 CCFileClass 相同的"先 MIX 后散装"优先级
//
//   原版 CCFileClass 打开文件时先查 vec_MixFiles 注册表，未命中才落回散装
//   文件；重建版的 CCFileClass::Open 只实现了散装路径（见 IO/FileSystem.cpp），
//   所以这里把 MIX 那一层补上，让 rulesmd.ini / artmd.ini 无论躺在目录里
//   还是躺在 ra2md.mix 里都能读到。
// ───────────────────────────────────────────────────────────────────────────

static bool Load_INI_Source(const char* pFileName, CCINIClass& target)
{
    if (pFileName == nullptr || pFileName[0] == '\0')
        return false;

    if (FileSystem::FileExists(pFileName))
    {
        CCFileClass file(pFileName);
        return target.ReadCCFile(&file, false, false);
    }

    MixFileClass* pMix = MixFileClass::FindFileByName(pFileName);
    if (pMix == nullptr)
        return false;

    int32 size = 0;
    void* pData = pMix->GetFile(pFileName, &size);
    if (pData == nullptr || size <= 0)
        return false;

    const bool ok = target.LoadFromBuffer(static_cast<const char*>(pData),
                                          static_cast<size_t>(size), false);
    YRMemory::Deallocate(pData);
    return ok;
}

// ───────────────────────────────────────────────────────────────────────────
// 散装数据文件搜索
//
//   原版在启动目录下直接找 rulesmd.ini / *.map；YR 的常见布局还会把它们
//   放进 ./ra2md/ 子目录。这里按「当前目录 → ./ra2md/ → ./」的顺序逐个
//   候选名试探（"" 与 "./" 指向同一处，保留两项只为对齐排查习惯）。
// ───────────────────────────────────────────────────────────────────────────

static const char* const kDataSearchDirs[] = { "", "ra2md/", "./" };
static const int32       kDataSearchDirCount =
    static_cast<int32>(sizeof(kDataSearchDirs) / sizeof(kDataSearchDirs[0]));

// ───────────────────────────────────────────────────────────────────────────
// 规则表解析：真实文件优先，缺席才退内置兜底
//
//   幂等：Init_MixFiles_Impl（第 7 步）与 Init_GameObjects_Impl（第 11 步）
//   都会调用，第二次起直接返回上一次的结果，不会重复读文件/重复铺兜底。
//   每次成功/退回都落一条日志，明确写出「真实文件路径」或「内置兜底」。
// ───────────────────────────────────────────────────────────────────────────

static bool   s_RulesIniResolved     = false;
static bool   s_RulesIniFromRealFile = false;
static char   s_RulesIniSource[MAX_PATH_LEN] = {0};

// 单个候选路径的读取：不存在 → false（不吭声）；存在但读不进 → 告警后 false；
// 读成功 → 落一条 "loaded from real file" 日志并返回 true。
static bool Try_Load_Rules_File(const char* pPath)
{
    if (pPath == nullptr || pPath[0] == '\0')
        return false;

    if (!FileSystem::FileExists(pPath))
        return false;

    CCFileClass file(pPath);
    if (!file.IsAvailable() ||
        !CCINIClass::INI_Rules->ReadCCFile(&file, false, false))
    {
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "Warning: rules INI found but unreadable, ignored: %s",
                      pPath);
        LogMessage(msg);
        return false;
    }

    s_RulesIniFromRealFile = true;
    std::snprintf(s_RulesIniSource, sizeof(s_RulesIniSource), "%s", pPath);

    char msg[256];
    std::snprintf(msg, sizeof(msg),
                  "Rules INI loaded from real file: %s", pPath);
    LogMessage(msg);
    return true;
}

static void Resolve_Rules_INI()
{
    if (s_RulesIniResolved)
        return;
    s_RulesIniResolved = true;

    if (CCINIClass::INI_Rules == nullptr)
        CCINIClass::INI_Rules = new CCINIClass();

    // 原版文件名的优先序：rulesmd.ini（YR）→ rules.ini（RA2）。
    static const char* const kRulesNames[] = { "rulesmd.ini", "rules.ini" };

    // 搜索序（与需求一致）：./ → ./ra2md/ → OPENYR_RULES 指定路径。
    //   "" 与 "ra2md/" 两项即可覆盖当前目录与 YR 常见的 ra2md/ 子目录；
    //   同一目录下的两个文件名按 YR 优先。
    static const char* const kRulesDirs[] = { "", "ra2md/" };

    for (int32 d = 0; d < 2; ++d)
    {
        for (int32 n = 0; n < 2; ++n)
        {
            char candidate[MAX_PATH_LEN];
            std::snprintf(candidate, sizeof(candidate), "%s%s",
                          kRulesDirs[d], kRulesNames[n]);
            if (Try_Load_Rules_File(candidate))
                return;
        }
    }

    // 最后一条显式路径：OPENYR_RULES（可指向任意目录 / 任意文件名）。
    const char* pEnvRules = std::getenv("OPENYR_RULES");
    if (pEnvRules != nullptr && pEnvRules[0] != '\0')
    {
        if (Try_Load_Rules_File(pEnvRules))
            return;

        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "Warning: OPENYR_RULES specified but not readable: %s",
                      pEnvRules);
        LogMessage(msg);
    }

    // 散装文件没有，再查已挂载的 MIX（ra2md.mix 内含 rulesmd.ini 的部署形态）。
    if (Load_INI_Source("rulesmd.ini", *CCINIClass::INI_Rules))
    {
        s_RulesIniFromRealFile = true;
        std::snprintf(s_RulesIniSource, sizeof(s_RulesIniSource), "%s",
                      "rulesmd.ini (from MIX)");
        LogMessage("Rules INI loaded from real file: rulesmd.ini (from MIX)");
        return;
    }

    // 都没有：退内置兜底集（不是真实数据，日志里明确标注 FALLBACK）。
    CCINIClass::INI_Rules->LoadFromBuffer(BUILTIN_RULES_INI,
                                          std::strlen(BUILTIN_RULES_INI), false);
    s_RulesIniFromRealFile = false;
    std::snprintf(s_RulesIniSource, sizeof(s_RulesIniSource), "%s",
                  "built-in fallback set");
    LogMessage("Rules INI FALLBACK: no rulesmd.ini / rules.ini found "
               "(cwd, ./ra2md/, OPENYR_RULES, MIX); using built-in minimal set.");
}

// ───────────────────────────────────────────────────────────────────────────
// 场景地图搜索：优先 OPENYR_MAP 显式指定，否则扫 *.map / *.mpr
//
//   同一目录内取文件名字典序最小的一个（readdir 顺序不稳定，排序后多次
//   运行结果一致）。扫不到就返回 false，由调用方退回内置 64×64。
// ───────────────────────────────────────────────────────────────────────────

static bool Find_Scenario_Map(char* pOut, size_t nOutSize)
{
    if (pOut == nullptr || nOutSize == 0)
        return false;

    const char* pEnv = std::getenv("OPENYR_MAP");
    if (pEnv != nullptr && pEnv[0] != '\0' && FileSystem::FileExists(pEnv))
    {
        std::snprintf(pOut, nOutSize, "%s", pEnv);
        return true;
    }

    static const char* const kMapPatterns[] = { "*.map", "*.mpr", "*.MAP", "*.MPR" };
    static const int32 kMapPatternCount =
        static_cast<int32>(sizeof(kMapPatterns) / sizeof(kMapPatterns[0]));

    for (int32 d = 0; d < kDataSearchDirCount; ++d)
    {
        for (int32 p = 0; p < kMapPatternCount; ++p)
        {
            char search[MAX_PATH_LEN];
            std::snprintf(search, sizeof(search), "%s%s",
                          kDataSearchDirs[d], kMapPatterns[p]);

            char best[MAX_PATH_LEN] = {0};
            FileFindClass finder;
            if (finder.FindFirst(search))
            {
                do
                {
                    if (finder.IsDirectory() || finder.IsHidden())
                        continue;

                    const char* pName = finder.GetFileName();
                    if (pName == nullptr || pName[0] == '\0')
                        continue;

                    if (best[0] == '\0' || std::strcmp(pName, best) < 0)
                        std::snprintf(best, sizeof(best), "%s", pName);
                }
                while (finder.FindNext());
            }

            if (best[0] != '\0')
            {
                std::snprintf(pOut, nOutSize, "%s%s", kDataSearchDirs[d], best);
                return true;
            }
        }
    }

    return false;
}

// ───────────────────────────────────────────────────────────────────────────
// 根据游戏行为，可知 MixFiles_Impl 负责下面这段逻辑。
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
    int32 mounted  = 0;

    for (int i = 0; i < static_cast<int>(sizeof(mixFiles) / sizeof(mixFiles[0])); ++i)
    {
        // 原版逐个 MixFileClass_CTOR：构造函数自己 Open() 解析头+索引，末端
        // AddFile() 把 "this" 挂进 vec_MixFiles。缺席的档案不算失败——部分
        // MIX（mod 专用）本来就可以没有。
        if (!FileSystem::FileExists(mixFiles[i]))
            continue;

        MixFileClass* pMix = new MixFileClass(mixFiles[i]);   // CTOR 内已完成 Open/AddFile
        if (pMix->GetFileCount() > 0)
        {
            ++mounted;
        }
        else
        {
            // 头或索引解析失败：档案未进注册表，不能 delete（析构会清空整张
            // 注册表），只记一条告警。
            LogMessage("Warning: MIX file could not be mounted.");
            allLoaded = false;
        }
    }

    char info[128];
    snprintf(info, sizeof(info), "MIX files loaded. (%d archive(s) mounted)", (int)mounted);
    LogMessage(info);

    // ── 规则表：rulesmd.ini / artmd.ini ──────────────────────────────────
    // 原版把 rules(md).ini 读进全局 INI_Rules、art(md).ini 读进 ART_INI，
    // 第 11 步（Init_GameObjects_Impl）随后用它们铺满各 *TypeClass::Array。
    // Resolve_Rules_INI 先按「当前目录 → ./ra2md/ → ./ → MIX」找真实文件，
    // 全都没有才退内置兜底集，并在日志里注明走的是哪条路径。
    Resolve_Rules_INI();

    // artmd.ini 只提供美术字段，缺席时各 LoadFromINI 保留默认值。
    if (!Load_INI_Source("artmd.ini", CCINIClass::INI_Art))
        LogMessage("Art INI not available (artmd.ini).");

    return allLoaded;
}

// ───────────────────────────────────────────────────────────────────────────
// 剧场瓦片集（原版 Init_Graphics 第 8 步）
//
//   原版在这里读 <theater>.ini 的 [TileSet%04d] 段，逐段建
//   IsometricTileTypeClass 并累加 TileSet_TotalTileCount；每套的 FileName
//   之后按剧场扩展名（.tem/.sno/.urb/.des/.lun/.nub）去 theater MIX 取图像。
//
//   图像本身是惰性的（IsometricTileType::GetImageData），首帧绘制时才真正
//   读盘 —— 与 asm sub_547020 的懒惰装图一致。
// ───────────────────────────────────────────────────────────────────────────
// ───────────────────────────────────────────────────────────────────────────
// 全局调色板（StandardPalette）
//
//   原版是 units 调色板的 ConvertClass，随剧场取 unittem.pal / unitsno.pal。
//   一个 .pal 都读不到时仍要建一个转换器：CC_Draw_Shape 的 Convert 参数为
//   空会直接返回，整条绘制链会被空指针短路（不是"画成黑色"，是"什么都不画"）。
// ───────────────────────────────────────────────────────────────────────────
static void Init_Standard_Palette()
{
    if (StandardPalette != nullptr)
        return;

    static const char* const kPalNames[] = {
        "unittem.pal", "unitsno.pal", "units.pal", "temperat.pal"
    };

    for (const char* pName : kPalNames)
    {
        ConvertClass* pConv = ConvertClass::LoadFromPaletteFile(pName);
        if (pConv != nullptr)
        {
            StandardPalette = pConv;
            std::fprintf(stderr, "[PAL] StandardPalette = %s\n", pName);
            return;
        }
    }

    PaletteClass defaultPal = ConvertClass::MakeDefaultPalette();
    BytePalette identity;
    for (int32 i = 0; i < 256; ++i)
        identity.Entries[i] = static_cast<uint8>(i);

    StandardPalette = new ConvertClass(defaultPal, identity, nullptr, 1, false);
    std::fprintf(stderr, "[PAL] StandardPalette = built-in fallback\n");
}

static void Load_Theater_Tilesets()
{
    if (TileTypeClass_Array != nullptr && TileTypeClass_Array->Count > 0)
        return;

    // 顺序与 ObjectTypeClass::TheaterSpecificID 的扩展名表一致。
    static const char* const kTheaterIni[6] = {
        "temperat.ini", "snow.ini", "urbann.ini",
        "desert.ini", "lunar.ini", "urbann.ini"
    };

    int32 theater = static_cast<int32>(g_Theater);
    if (theater < 0 || theater > 5)
        theater = 0;

    static CCINIClass s_TheaterIni;
    bool loaded = Load_INI_Source(kTheaterIni[theater], s_TheaterIni);
    if (!loaded && theater != 0)
    {
        loaded = Load_INI_Source(kTheaterIni[0], s_TheaterIni);
        if (loaded)
            theater = 0;
    }

    if (!loaded)
    {
        std::fprintf(stderr, "[THEATER] no %s; terrain tiles will not be drawn\n",
                     kTheaterIni[theater]);
        return;
    }

    // bTheater = true 表示按 ToSnowTheater 过滤（雪地系），否则按 ToTemperate。
    const bool bSnow = (theater == 1);
    IsometricTileType::CreateFromINIList(&s_TheaterIni, bSnow);

    std::fprintf(stderr, "[THEATER] %s: %d set(s), %d tile(s)\n",
                 kTheaterIni[theater],
                 (TileTypeClass_Array != nullptr)
                     ? static_cast<int>(TileTypeClass_Array->Count) : 0,
                 static_cast<int>(TileSet_TotalTileCount));
}

// ───────────────────────────────────────────────────────────────────────────
// 根据游戏行为，可知 Graphics_Impl 负责下面这段逻辑。
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

    // 第 8 步：读 <theater>.ini 的 [TileSet%04d] 建瓦片集表。没有 theater INI
    // 时表为空，地形层就是不画（不拿程序化图形顶替素材）。
    Init_Standard_Palette();
    Load_Theater_Tilesets();

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
// 根据游戏行为，可知 Keyboard_Impl 负责下面这段逻辑。
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

    // asm: vec_Commands —— 热键命令表，键盘子系统起来时铺底（长度 0，
    // 具体命令由 KeyboardClass::Add_Command 在界面构造时逐条登记）。
    Ensure_Array(KeyboardClass::Commands);

    LogMessage("Keyboard handler initialized.");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
// 根据游戏行为，可知 Mouse_Impl 负责下面这段逻辑。
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
// 根据游戏行为，可知 Sound_Impl 负责下面这段逻辑。
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

    // asm: vec_Vox —— 语音音效表。音效关闭时也要铺底：表本身只是容器，
    // 空表比 nullptr 更接近原版（Get 会返回 -1 而不是崩在解引用上）。
    Ensure_Array(VoxClass::Array);

    if (g_bNoSound)
    {
        LogMessage("Sound disabled by user.");
        return true;
    }

    LogMessage("Sound system initialized.");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
// 根据游戏行为，可知 GameObjects_Impl 负责下面这段逻辑。
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

    // ── 1. 全局对象池 ───────────────────────────────────────────────────
    // asm: L69914-69920 —— vec_Logics 与 vec_VisibleObjects 在同一块里建立。
    // 必须早于任何对象 Unlimbo（后者会把对象 Add 进 vec_Logics）。
    LogicClass_Init();

    // ── 2. 各层静态表 ───────────────────────────────────────────────────
    // 逐层从上往下铺：子类的 LoadFromINI/FindOrAllocate 会用到父类表。
    AbstractClass::Init_Array();
    AbstractTypeClass::Init_Array();

    ObjectClass::Init_Array();          // 同时铺底 vec_VisibleObjects
    ObjectTypeClass::Init_Array();

    TechnoClass::Init_Array();
    TechnoTypeClass::Init_Array();
    FootClass::Init_Array();

    // 类型表（rulesmd.ini 各 *Types 段的目标容器）
    UnitTypeClass::Init_Array();
    InfantryTypeClass::Init_Array();
    AircraftTypeClass::Init_Array();
    BuildingTypeClass::Init_Array();
    OverlayTypeClass::Init_Array();
    TerrainTypeClass::Init_Array();
    SmudgeTypeClass::Init_Array();
    VoxelAnimTypeClass::Init_Array();
    TiberiumClass::Init_Array();

    // 实例表
    SmudgeClass::Init_Array();
    TerrainClass::Init_Array();
    VoxelAnimClass::Init_Array();
    FactoryClass::Init_Array();

    // 其余类型表：原版同样是 SCTOR 铺底，重建侧统一在这里补上初始化点。
    Ensure_Array(AnimTypeClass::Array);
    Ensure_Array(BulletTypeClass::Array);
    Ensure_Array(WarheadTypeClass::Array);

    // 实例表：动画 / 抛射体 / 粒子系统。
    // 战术层 Draw_Transient_Objects 会遍历这三张表，表为空时整层直接跳过，
    // 表现为爆炸、炮口闪光、导弹、粒子一个都不出现。
    Ensure_Array(AnimClass::Array);
    Ensure_Array(BulletClass::Array);
    Ensure_Array(ParticleSystemClass::Array);
    Ensure_Array(WeaponTypeClass::Array);
    Ensure_Array(SuperWeaponTypeClass::Array);
    Ensure_Array(ParticleTypeClass::Array);
    Ensure_Array(ParticleSystemTypeClass::Array);

    // 其余实例表
    Ensure_Array(UnitClass::Array);
    Ensure_Array(InfantryClass::Array);
    Ensure_Array(AircraftClass::Array);
    Ensure_Array(BuildingClass::Array);
    Ensure_Array(OverlayClass::Array);
    Ensure_Array(AnimClass::Array);
    Ensure_Array(BulletClass::Array);
    Ensure_Array(ParticleClass::Array);
    Ensure_Array(ParticleSystemClass::Array);
    Ensure_Array(WaveClass::Array);
    Ensure_Array(RadSiteClass::Array);
    Ensure_Array(RadBeamClass::Array);
    Ensure_Array(WeaponWaveClass::Array);
    Ensure_Array(SuperClass::Array);

    // 炸弹 / 探雷器（asm: vec_BombList / vec_Detectors）
    Ensure_Array(BombClass::BombList);
    Ensure_Array(BombClass::Detectors);

    // AI 脚本层（vec_TeamTypes / vec_TaskForces / vec_ScriptTypes / vec_Triggers …）
    Ensure_Array(TeamTypeClass::Array);
    Ensure_Array(TeamClass::Array);
    Ensure_Array(TaskForceClass::Array);
    Ensure_Array(ScriptTypeClass::Array);
    Ensure_Array(ScriptClass::Array);
    Ensure_Array(TriggerTypeClass::Array);
    Ensure_Array(TriggerClass::Array);
    Ensure_Array(TagTypeClass::Array);
    Ensure_Array(TagClass::Array);
    Ensure_Array(TActionClass::Array);
    Ensure_Array(TEventClass::Array);
    Ensure_Array(AITriggerTypeClass::Array);
    Ensure_Array(AITriggerClass::Array);
    Ensure_Array(AITeamTypeClass::Array);
    Ensure_Array(AITeamClass::Array);

    // 阵营 / 战役 / 影片 / 特殊对象 / 渲染配色
    Ensure_Array(SideClass::Sides);
    Ensure_Array(CampaignClass::Array);
    Ensure_Array(CoopCampaignClass::Array);
    Ensure_Array(MovieClass::Array);
    Ensure_Array(MPGameModeClass::Array);
    Ensure_Array(SlaveManagerClass::Array);
    Ensure_Array(VeinholeMonsterClass::Array);
    Ensure_Array(RadarEventClass::Array);
    Ensure_Array(ConvertClass::Array);
    Ensure_Array(LightConvertClass::Array);
    Ensure_Array(ColorScheme::Array);

    // ── 3~12. 规则类型表 ────────────────────────────────────────────────
    // asm: RulesClass_Init —— 用全局 INI_Rules 跑一遍 RulesClass_Addition，
    // 逐个 *Types 段把名字交给 XXXTypeClass_FindOrAllocate 注册进该类的 Array，
    // 再用类型自己的 LoadFromINI 填字段。INI 由本函数第一件事准备：
    // 先找真实 rulesmd.ini / rules.ini（当前目录 → ./ra2md/ → ./ → MIX），
    // 找不到才退内置兜底集。Resolve_Rules_INI 幂等，第 7 步已调过则直接复用。
    Resolve_Rules_INI();

    if (RulesClass::Instance == nullptr)
        RulesClass::Instance = new RulesClass();

    // asm: the global `Rules` pointer (referenced everywhere as `TheRules`) is
    // the same object as `RulesClass::Instance`; the engine keeps both so that
    // hot paths can dereference a plain pointer.  Forgetting this assignment
    // left every TheRules-based predicate (CellClass::Is_Clear_To_Move,
    // Get_Movement_Cost, RadLevel caps ...) silently reading a nullptr and
    // reporting "impassable" for the whole map.
    TheRules = RulesClass::Instance;

    if (CCINIClass::INI_Rules == nullptr)
        CCINIClass::INI_Rules = new CCINIClass();

    {
        char msg[256];
        std::snprintf(msg, sizeof(msg), "Rules INI source: %s (%s)",
                      s_RulesIniSource,
                      s_RulesIniFromRealFile ? "real data file" : "FALLBACK");
        LogMessage(msg);
    }

    // asm: HouseTypeClass::One_Time runs before the rule set is parsed.
    //   TechnoTypeClass's `Owner=` key is turned into an owner bitmask through
    //   HouseTypeClass::FindIndex, so the country table must already exist;
    //   registering the houses afterwards leaves every type with Owner == 0
    //   and Create_Units then finds no buildable type at all.
    if (HouseTypeClass::ArrayCount == 0)
        HouseTypeClass::Init_Defaults();

    RulesClass::Instance->Init(CCINIClass::INI_Rules);
    g_bRulesLoaded = true;

    // ── 13. 对象工厂 ────────────────────────────────────────────────────
    // FactoryClass::Array 已在上面铺底；具体工厂实例在建筑物 Unlimbo 时建立
    // （FactoryClass_CTOR 自己 Add 进 vec_Factories）。

    // ── 冒烟诊断：各类型表落地情况（OPENYR_DEBUG=1 才输出）─────────────
    if (g_bDebugMode)
    {
        char counts[320];
        snprintf(counts, sizeof(counts),
                 "Rules types: units=%d infantry=%d buildings=%d aircraft=%d "
                 "weapons=%d warheads=%d projectiles=%d anims=%d superweapons=%d",
                 (int)Count_Of(UnitTypeClass::Array),
                 (int)Count_Of(InfantryTypeClass::Array),
                 (int)Count_Of(BuildingTypeClass::Array),
                 (int)Count_Of(AircraftTypeClass::Array),
                 (int)Count_Of(WeaponTypeClass::Array),
                 (int)Count_Of(WarheadTypeClass::Array),
                 (int)Count_Of(BulletTypeClass::Array),
                 (int)Count_Of(AnimTypeClass::Array),
                 (int)Count_Of(SuperWeaponTypeClass::Array));
        LogMessage(counts);

        snprintf(counts, sizeof(counts),
                 "Object tables: bullets=%d technos=%d logics=%d objects=%d "
                 "units=%d infantry=%d buildings=%d anims=%d",
                 (int)Count_Of(BulletClass::Array),
                 (int)Count_Of(TechnoClass::Array),
                 (vec_Logics != nullptr) ? (int)vec_Logics->Count : -1,
                 (int)Count_Of(ObjectClass::Array),
                 (int)Count_Of(UnitClass::Array),
                 (int)Count_Of(InfantryClass::Array),
                 (int)Count_Of(BuildingClass::Array),
                 (int)Count_Of(AnimClass::Array));
        LogMessage(counts);
    }

    LogMessage("Game objects initialized.");
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
// 根据游戏行为，可知 GameMode_Impl 负责下面这段逻辑。
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

// ── 第 13 步（Scenario_Impl）用到的类型 ──────────────────────────────────
#include "Map/MapClass.h"
#include "Map/CellClass.h"
#include "Scenario/ScenarioClass.h"
#include "Abstract/TechnoClass.h"
#include "Houses/HouseClass.h"
#include "Houses/HouseTypeClass.h"

// ═══════════════════════════════════════════════════════════════════════════
// Scenario_Impl —— 原版 Init_Scenario 链：
//
//   InitGame+0x2D : new ScenarioClass ; ScenarioClass_Init
//   Read_Scenario_INI → MapClass_Init（分配 CellClass 表；尺寸来自 .map）
//   HouseClass_LoadFromINIList → new HouseClass(HouseTypeClass*)
//                                （HouseClass_CTOR 尾部自入 vec_Houses）
//   ScenarioClass_CreateUnits → UnitClass / InfantryClass → Unlimbo
//                                （ObjectClass::Unlimbo 尾部 vec_Logics.Add）
//
// 本树不带 .map 文件，所以地图与参战方全部由代码生成（"内置最小场景"）。
// ═══════════════════════════════════════════════════════════════════════════

// 内置地图尺寸：原版尺寸取自 .map 的 [Map] Size=，无地图文件时用 64×64。
static const int32 kBuiltinMapWidth  = 64;
static const int32 kBuiltinMapHeight = 64;

// 内置兜底：类实例表。
//   原版 vec_UnitClass / vec_InfantryClass 由各自的 Init_Array 在启动期建立
//   （与 TechnoClass::Init_Array 相同的 alloc + placement-new 形态）。本树这
//   两张表尚无建立入口，这里在场景初始化时幂等补齐，保证随后创建的单位 /
//   步兵能被各自的构造函数登记进类表。
template <typename T>
static void Ensure_Class_Array(DynamicVectorClass<T>*& rArray)
{
    if (rArray != nullptr)
        return;

    rArray = static_cast<DynamicVectorClass<T>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<T>)));
    if (rArray != nullptr)
        new (rArray) DynamicVectorClass<T>();
}

// asm: HouseClass_CTOR 尾部 —— 新建的房子把自己追加进 vec_Houses，并把
//   ArrayIndex / AllHousesIndex 记为追加时的下标（与 LoadFromINIList 同形）。
static void Register_House(HouseClass* pHouse)
{
    if (pHouse == nullptr || HouseClass::ArrayCount >= HouseClass::MaxHouses)
        return;

    const int32 idx = HouseClass::ArrayCount;

    HouseClass::Array[idx]  = pHouse;
    pHouse->ArrayIndex      = idx;
    pHouse->AllHousesIndex  = idx;

    if (idx < MAX_HOUSES)
        Houses[idx] = pHouse;

    HouseClass::ArrayCount = idx + 1;
    TheHouseCount          = HouseClass::ArrayCount;
}

// 在玩家基地区域放一个起始步兵/载具：Type / TechnoType 双向回填后再 Unlimbo。
//   原版 ObjectClass_CTOR 把新对象以 limbo 状态交出（IsInLimbo=1），Unlimbo
//   才允许落位；本树 ObjectClass 默认 IsInLimbo=0，故落位前置一。
template <typename TTechno, typename TType>
static TTechno* Place_Starting_Techno(HouseClass* pOwner, TType* pType,
                                      const CellStruct& cell)
{
    if (pType == nullptr)
        return nullptr;

    // 内置兜底：规则未加载时类型的生命值是 0，给一个合法下限，避免
    // Unlimbo 后立刻被判为死亡对象。
    if (pType->Strength <= 0)
        pType->Strength = 200;

    TTechno* pTechno = GameCreateArgs<TTechno>(pOwner);
    if (pTechno == nullptr)
        return nullptr;

    pTechno->Type       = pType;
    pTechno->TechnoType = pType;
    pTechno->Health     = pType->Strength;
    pTechno->MaxHealth  = pType->Strength;
    pTechno->Location   = CellClass::Cell2Coord(cell);
    pTechno->IsInLimbo  = true;

    if (!pTechno->Unlimbo())
    {
        GameDelete(pTechno);
        return nullptr;
    }

    // asm: TechnoClass_CTOR 尾部把 this 追加进 vec_Techno；本树 CTOR 未接
    //   这一步，故在落位成功后显式补登记。
    TechnoClass::Add_To_Array(pTechno);

    // asm: ObjectClass_Update → DisplayClass::Submit(this)（0x4A96E0 同族）：
    //   把对象挂进 vec_ObjectsInLayers 并同时登记进 vec_VisibleObjects。
    //   绘制期 TacticalClass::GetVisibleObjects 就从这张表取对象；不 Submit
    //   的对象在原版同样不会被画出来。
    if (TheDisplay != nullptr)
        TheDisplay->Submit(pTechno);

    return pTechno;
}

// ───────────────────────────────────────────────────────────────────────────
// .map / .mpr 的真实解析
//
//   asm: DisplayClass::Read_INI —— 场景 INI 里与本树已建模部分相关的键：
//     [Map] Size=      起始 X,Y + 宽,高（决定 CellClass 表尺寸）
//     [Map] LocalSize= 可玩区域（原版存入 Scenario，不影响分配）
//     [Map] Theater=   剧场（原版 INIClass_ReadTheater）
//     [Basic] Name=    场景显示名
//     [IsoMapPack5]    UU 块：LZO1X 压缩的逐格记录，是地图"真实地形"本体
//
//   格记录布局（asm: sub_56BAC0 / IsoMapPack5 解码循环）：
//     每格 11 字节 = X(int16) + Y(int16) + TileIndex(int32)
//                    + Passability(1, 写 CellClass+0x11A)
//                    + LandByte(1,  写 CellClass+0x11B)
//                    + 未用(1,      写 CellClass+0x119)
//     终止记录 = 坐标 (-1,-1)（asm: MapClass_Default_CellCoords 哨兵）。
//   注意：IsoMapPack 1~4 的格记录是 10 字节（无末字节），由
//   sub_56B8A0 / sub_56B9A0 读取；本树只解 YR 地图实际使用的 Pack5。
// ───────────────────────────────────────────────────────────────────────────

struct MapTileRecord
{
    int16 X;
    int16 Y;
    int32 Tile;
    uint8 Passability;   // CellClass +0x11A
    uint8 LandByte;      // CellClass +0x11B
};

struct ScenarioMapFile
{
    bool  SizeOk        = false;
    bool  TilesDecoded  = false;
    bool  TileBlockPresent = false;
    int32 StartX        = 0;
    int32 StartY        = 0;
    int32 Width         = 0;
    int32 Height        = 0;
    bool  HasLocalSize  = false;
    int32 LocalX        = 0;
    int32 LocalY        = 0;
    int32 LocalW        = 0;
    int32 LocalH        = 0;
    char  Theater[32]   = {0};
    char  Name[64]      = {0};
    std::vector<MapTileRecord> Tiles;
};

// 读一个 "a,b,c,d" 四元组；成功返回 true。Map 的 Size / LocalSize 同格式。
static bool Parse_Rect4(const char* pText, int32& a, int32& b, int32& c, int32& d)
{
    if (pText == nullptr)
        return false;

    int v0 = 0, v1 = 0, v2 = 0, v3 = 0;
    if (std::sscanf(pText, "%d,%d,%d,%d", &v0, &v1, &v2, &v3) != 4)
        return false;

    a = v0; b = v1; c = v2; d = v3;
    return true;
}

// 解析场景文件。返回 false 表示"文件找到了但用不了"（日志已写明原因）；
// true 表示 Size 合法，TilesDecoded 说明格数据是否解出。
static bool Parse_Scenario_Map_File(const char* pPath, ScenarioMapFile& out)
{
    CCFileClass file(pPath);
    CCINIClass  ini;

    if (!file.IsAvailable() || !ini.LoadFile(&file))
    {
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "Warning: scenario map found but unreadable: %s", pPath);
        LogMessage(msg);
        return false;
    }

    char sizeBuf[128] = {0};
    if (ini.ReadString("Map", "Size", "", sizeBuf, sizeof(sizeBuf)) <= 0)
    {
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "Warning: scenario map has no [Map] Size= key: %s", pPath);
        LogMessage(msg);
        return false;
    }

    int32 sx = 0, sy = 0, sw = 0, sh = 0;
    if (!Parse_Rect4(sizeBuf, sx, sy, sw, sh) || sw <= 0 || sh <= 0)
    {
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "Warning: scenario map [Map] Size= malformed (%s): %s",
                      sizeBuf, pPath);
        LogMessage(msg);
        return false;
    }

    // asm: sub_56B8A0 的坐标上限 —— 格索引必须落在 0x40000 (256K) 以内。
    if (static_cast<int64>(sw) * static_cast<int64>(sh) > 0x40000)
    {
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "Warning: scenario map [Map] Size= out of range "
                      "(%d,%d): %s", (int)sw, (int)sh, pPath);
        LogMessage(msg);
        return false;
    }

    out.SizeOk = true;
    out.StartX = sx;
    out.StartY = sy;
    out.Width  = sw;
    out.Height = sh;

    ini.ReadString("Map", "Theater", "", out.Theater, sizeof(out.Theater));
    ini.ReadString("Basic", "Name", "", out.Name, sizeof(out.Name));

    char localBuf[128] = {0};
    if (ini.ReadString("Map", "LocalSize", "", localBuf, sizeof(localBuf)) > 0)
    {
        int32 lx = 0, ly = 0, lw = 0, lh = 0;
        if (Parse_Rect4(localBuf, lx, ly, lw, lh) && lw > 0 && lh > 0)
        {
            out.HasLocalSize = true;
            out.LocalX = lx; out.LocalY = ly;
            out.LocalW = lw; out.LocalH = lh;
        }
    }

    // ── 格数据（[IsoMapPack5]）─────────────────────────────────────────
    const size_t maxBytes = (static_cast<size_t>(sw) * static_cast<size_t>(sh)
                             + 1) * 11;
    std::vector<uint8> packed(maxBytes, 0);

    const int32 packedLen = ini.Get_UUBlock("IsoMapPack5", packed.data(),
                                            packed.size());
    if (packedLen <= 0)
    {
        // 合法的纯剧本 / 空地图可能没有格数据：不算失败，但如实记录。
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "Scenario map has no [IsoMapPack5] tile block: %s "
                      "(map size kept, tile data absent)", pPath);
        LogMessage(msg);
        return true;
    }

    out.TileBlockPresent = true;

    std::vector<uint8> plain(maxBytes + 16, 0);
    int32 plainLen = 0;
    const int32 rc = lzo1x_decompress_safe(packed.data(), packedLen,
                                           plain.data(),
                                           static_cast<int32>(plain.size()),
                                           &plainLen);
    if (rc != 0 || plainLen < 11)
    {
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "Warning: [IsoMapPack5] LZO decode failed "
                      "(rc=%d, packed=%d, plain=%d): %s "
                      "(map size kept, tile data ignored)",
                      (int)rc, (int)packedLen, (int)plainLen, pPath);
        LogMessage(msg);
        return true;
    }

    // asm: sub_56BAC0 记录循环。每格 11 字节；坐标 (-1,-1) 即终止哨兵
    //   （哨兵可以只带 4 字节坐标，后面的字节不再读取）。
    const uint8* p     = plain.data();
    const uint8* pEnd  = plain.data() + plainLen;
    out.Tiles.reserve(static_cast<size_t>(plainLen / 11));

    while (static_cast<int32>(pEnd - p) >= 4)
    {
        int16 x = 0, y = 0;
        std::memcpy(&x, p, sizeof(int16));
        std::memcpy(&y, p + 2, sizeof(int16));
        p += 4;

        if (x == static_cast<int16>(-1) && y == static_cast<int16>(-1))
            break;

        if (static_cast<int32>(pEnd - p) < 7)
        {
            LogMessage("Warning: [IsoMapPack5] truncated tile record; "
                       "decoding stopped.");
            break;
        }

        int32 tile = 0;
        std::memcpy(&tile, p, sizeof(int32));
        p += 4;

        MapTileRecord rec;
        rec.X           = x;
        rec.Y           = y;
        rec.Tile        = tile;
        rec.Passability = *p++;
        rec.LandByte    = *p++;
        ++p;                         // +0x119 的未用字节（本树无消费方）

        out.Tiles.push_back(rec);
    }

    out.TilesDecoded = !out.Tiles.empty();

    char msg[256];
    std::snprintf(msg, sizeof(msg),
                  "Scenario map tile data decoded: %d records "
                  "(%d bytes plain, from %d bytes packed): %s",
                  (int)out.Tiles.size(), (int)plainLen, (int)packedLen, pPath);
    LogMessage(msg);
    return true;
}

// 把解出的格记录写回 MapClass。越界记录丢弃并计数。
static int32 Apply_Scenario_Map_Tiles(MapClass* pMap, const ScenarioMapFile& src,
                                      int32& skipped)
{
    skipped = 0;
    if (pMap == nullptr || pMap->CellArray == nullptr)
        return 0;

    int32 applied = 0;
    for (size_t i = 0; i < src.Tiles.size(); ++i)
    {
        const MapTileRecord& rec = src.Tiles[i];
        CellClass* pCell = pMap->GetCellAt(static_cast<int32>(rec.X),
                                           static_cast<int32>(rec.Y));
        if (pCell == nullptr)
        {
            ++skipped;
            continue;
        }

        // ── 全局瓦片序号 → (瓦片集, 集内子序号) ──────────────────────────
        //
        // [IsoMapPack5] 存的 rec.Tile 是**跨全部瓦片集累加**的全局序号
        // （asm CellClass.IsoTileTypeIndex 在 +0x38）。以前直接把它写进
        // TileType，等于把全局号当成“第几套图”，于是整张地图只会画第 0
        // 套的第 0..N 帧。这里按 asm dword_AA1140 的起点表拆开。
        int32 setIndex = 0;
        int32 subIndex = 0;
        if (IsometricTileType::Split_Global_Tile_Index(rec.Tile, setIndex, subIndex))
        {
            pCell->TileType     = setIndex;
            pCell->TileSubIndex = subIndex;
        }
        else
        {
            // 0xFFFF 是“无瓦片”哨兵；瓦片集表未建立时也同样置空。
            pCell->TileType     = -1;
            pCell->TileSubIndex = 0;
        }

        pCell->Passability = rec.Passability;
        pCell->LandByte    = rec.LandByte;
        ++applied;
    }

    return applied;
}

// asm: INIClass_ReadTheater —— [Map] Theater= 的字符串按剧场名解析成枚举。
//   未知 / 缺失时原版落回 Temperate。
static TheaterType Theater_From_Name(const char* pName)
{
    if (pName == nullptr || pName[0] == '\0')
        return TheaterType::Temperate;

    char lower[16] = {0};
    for (int32 i = 0; i < 15 && pName[i] != '\0'; ++i)
        lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(pName[i])));

    if (std::strcmp(lower, "snow") == 0)     return TheaterType::Snow;
    if (std::strcmp(lower, "urban") == 0)    return TheaterType::Urban;
    if (std::strcmp(lower, "desert") == 0)   return TheaterType::Desert;
    if (std::strcmp(lower, "lunar") == 0)    return TheaterType::Lunar;
    if (std::strcmp(lower, "newurban") == 0) return TheaterType::NewUrban;
    return TheaterType::Temperate;
}

// ───────────────────────────────────────────────────────────────────────────
// 根据游戏行为，可知 Scenario_Impl 负责下面这段逻辑。
// ───────────────────────────────────────────────────────────────────────────

bool Init_Scenario_Impl()
{
    // ── 1. ScenarioClass 单例 ────────────────────────────────────────────
    // asm: InitGame+0x2D —— new ScenarioClass，紧接着调 ScenarioClass_Init。
    if (ScenarioClass::Instance == nullptr)
        ScenarioClass::Instance = GameCreate<ScenarioClass>();

    if (ScenarioClass::Instance == nullptr)
    {
        LogMessage("Failed to instantiate Scenario!");
        return false;
    }

    ScenarioClass* pScenario = ScenarioClass::Instance;
    TheScenario = pScenario;            // asm: 全局 Scenario

    pScenario->Init();                  // asm: ScenarioClass_Init（冷启动复位）

    // ── 2. MapClass 单例 + 建图 ──────────────────────────────────────────
    if (MapClass::Instance == nullptr)
        MapClass::Instance = GameCreate<MapClass>();

    if (MapClass::Instance == nullptr)
    {
        LogMessage("Failed to instantiate Map!");
        return false;
    }

    MapClass* pMap = MapClass::Instance;
    TheMap = pMap;                      // asm: 全局 Map

    // asm: MapClass_Init —— 分配 CellClass 表，并铺好邻接格 / 航点 / 黑幕 /
    //   CellSpread 表 / 层高常量。
    //
    // 尺寸来源（与原版 .map 一致）：[Map] Size=<x>,<y>,<width>,<height>。
    //   先找真实地图（OPENYR_MAP 指定，或当前目录 → ./ra2md/ → ./ 下的
    //   *.map / *.mpr，字典序取第一个）；找不到 / 解析失败就退回内置 64×64，
    //   并落日志说明走的是哪条路径。无论哪条路径，随后都用同一套代码铺地。
    char  mapPath[MAX_PATH_LEN] = {0};
    bool  bRealMap              = false;
    int32 mapWidth              = kBuiltinMapWidth;
    int32 mapHeight             = kBuiltinMapHeight;
    int32 mapStartX             = 0;
    int32 mapStartY             = 0;
    ScenarioMapFile mapData;

    if (Find_Scenario_Map(mapPath, sizeof(mapPath)))
    {
        // asm: Read_Scenario_INI —— 场景 INI 的正式入口（CD 判定 + FileName
        //   回填 + INIClass::ReadScenario）。它把 INI 读进局部对象后即销毁，
        //   所以 [Map] Size / [IsoMapPack5] 需要自己再读一次；两次读的是同
        //   一个文件，只影响"谁持有解析结果"，不影响语义。
        if (!pScenario->Read_Scenario_INI(mapPath))
        {
            char msg[256];
            std::snprintf(msg, sizeof(msg),
                          "Warning: Read_Scenario_INI rejected map: %s "
                          "(size/tile parse attempted anyway)", mapPath);
            LogMessage(msg);
        }
        else
        {
            char msg[256];
            std::snprintf(msg, sizeof(msg),
                          "Read_Scenario_INI accepted map: %s", mapPath);
            LogMessage(msg);
        }

        if (Parse_Scenario_Map_File(mapPath, mapData))
        {
            mapStartX = mapData.StartX;
            mapStartY = mapData.StartY;
            mapWidth  = mapData.Width;
            mapHeight = mapData.Height;
            bRealMap  = true;
        }
        else
        {
            // 找到但用不了：解析函数已逐条告警，这里给最终去向。
            char msg[256];
            std::snprintf(msg, sizeof(msg),
                          "Scenario map FALLBACK: '%s' found but unusable "
                          "(reason logged above); using built-in %dx%d.",
                          mapPath, (int)kBuiltinMapWidth, (int)kBuiltinMapHeight);
            LogMessage(msg);
        }
    }

    bool bMapFreshlyAllocated = false;

    if (pMap->CellArray == nullptr)
    {
        pMap->Init(mapWidth, mapHeight);
        bMapFreshlyAllocated = true;

        // 真实地图尺寸可能超出本树能分配的范围：分配失败就退回内置尺寸，
        // 不把一张空图当成加载成功。
        if ((pMap->CellArray == nullptr || pMap->CellCount <= 0) && bRealMap)
        {
            LogMessage("Warning: real map cell allocation failed; "
                       "falling back to built-in 64x64 map.");
            bRealMap  = false;
            mapWidth  = kBuiltinMapWidth;
            mapHeight = kBuiltinMapHeight;
            mapStartX = 0;
            mapStartY = 0;
            pMap->Init(mapWidth, mapHeight);
        }
    }

    if (!bMapFreshlyAllocated)
    {
        // 重入 / 存档恢复：格子表已存在，本次没有读入任何地图文件。
        // 此时 bRealMap 不代表本次实际加载结果，日志要如实说明。
        bRealMap = false;
        LogMessage("Scenario map: cell array already allocated; "
                   "reusing the existing map (no map file read this pass).");
    }

    // ── 2.5 把解出的第格数据写回 CellClass ──────────────────────────────
    //   只在"本次新建的格子表 + 真实地图 + 格数据解码成功"时执行；
    //   否则 CellClass 保持构造默认值（TileType=0 / Passability=0）。
    int32 appliedTiles = 0;
    int32 skippedTiles = 0;
    bool  bTilesApplied = false;

    if (bMapFreshlyAllocated && bRealMap && mapData.TilesDecoded)
    {
        appliedTiles = Apply_Scenario_Map_Tiles(pMap, mapData, skippedTiles);
        bTilesApplied = (appliedTiles > 0);

        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "Scenario map tiles applied: %d cells (%d records "
                      "out of map bounds)", (int)appliedTiles, (int)skippedTiles);
        LogMessage(msg);

        if (!bTilesApplied)
        {
            LogMessage("Warning: decoded tile records matched no cell; "
                       "map stays at default flat terrain.");
        }
    }

    if (bRealMap)
    {
        char msg[320];
        std::snprintf(msg, sizeof(msg),
                      "Scenario map loaded from real file: %s "
                      "([Map] Size=%d,%d,%d,%d, mapcells=%d, theater=%s, "
                      "name=\"%s\", tile data=%s)",
                      mapPath, (int)mapStartX, (int)mapStartY,
                      (int)mapWidth, (int)mapHeight, (int)pMap->CellCount,
                      (mapData.Theater[0] != '\0') ? mapData.Theater : "-",
                      (mapData.Name[0] != '\0') ? mapData.Name : "-",
                      bTilesApplied ? "applied"
                                    : (mapData.TileBlockPresent
                                           ? "decode-failed" : "absent"));
        LogMessage(msg);

        if (mapData.HasLocalSize)
        {
            char localMsg[192];
            std::snprintf(localMsg, sizeof(localMsg),
                          "Scenario [Map] LocalSize=%d,%d,%d,%d",
                          (int)mapData.LocalX, (int)mapData.LocalY,
                          (int)mapData.LocalW, (int)mapData.LocalH);
            LogMessage(localMsg);
        }
    }
    else if (bMapFreshlyAllocated && mapPath[0] == '\0')
    {
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "Scenario map FALLBACK: no .map/.mpr found "
                      "(OPENYR_MAP / cwd / ./ra2md/); using built-in %dx%d.",
                      (int)kBuiltinMapWidth, (int)kBuiltinMapHeight);
        LogMessage(msg);
    }

    if (pMap->CellArray == nullptr || pMap->CellCount <= 0)
    {
        LogMessage("Failed to allocate the map cell array!");
        return false;
    }

    // 整图铺底：清掉覆盖物 / 污迹、全部已探明（原版 Unshroud 后的稳态）。
    //   Land 只在"没有真实格数据"时写死为 Clear —— 真实地图的土地类型
    //   由 [IsoMapPack5] 的 TileIndex 决定，不能被这层默认值覆盖。
    for (int32 i = 0; i < pMap->CellCount; ++i)
    {
        CellClass* pCell = &pMap->CellArray[i];

        if (!bTilesApplied)
            pCell->Land    = ::LandType::Clear;
        pCell->Overlay     = -1;
        pCell->OverlayData = 0;
        pCell->Smudge      = -1;
        pCell->SmudgeData  = 0;

        pCell->SetFlag(CellFlags::IsShrouded, false);
        pCell->SetFlag(CellFlags::Fogged, false);
        pCell->SetFlag(CellFlags::Explored, true);
        pCell->SetFlag(CellFlags::CenterRevealed, true);
        pCell->SetFlag(CellFlags::EdgeRevealed, true);
    }

    // 内置兜底才随机撒几块粗糙地形，避免整张地图是一块纯色白板；
    //   真实地图的地形来自文件，这里不得再"创作"。
    if (!bTilesApplied)
    {
        const int32 roughCellCount = (pMap->CellCount / 256) + 8;
        for (int32 i = 0; i < roughCellCount; ++i)
        {
            const int32 cellX = pScenario->Random.Next(1, pMap->MapWidth - 2);
            const int32 cellY = pScenario->Random.Next(1, pMap->MapHeight - 2);

            CellClass* pCell = pMap->GetCellAt(cellX, cellY);
            if (pCell != nullptr)
                pCell->Land = ::LandType::Rough;
        }
    }

    // 整图重算派生属性。原版是 MapClass::Recalc_Attributes 一把刷完；本树
    //   MapClass 未暴露该成员，逐格调 CellClass::Recalc_Attributes。
    for (int32 i = 0; i < pMap->CellCount; ++i)
        pMap->CellArray[i].Recalc_Attributes();

    // ── 3. Scenario 头字段 ───────────────────────────────────────────────
    // asm: [Map] Size= / LocalSize= 落在 StartX/StartY/Width/Height。
    //   真实地图的 StartX/StartY 取自 [Map] Size 的前两个分量；内置图固定 0。
    pScenario->StartX    = mapStartX;
    pScenario->StartY    = mapStartY;
    pScenario->Width     = pMap->MapWidth;
    pScenario->Height    = pMap->MapHeight;
    pScenario->MapWidth  = pMap->MapWidth;
    pScenario->MapHeight = pMap->MapHeight;
    pScenario->Theater   = (bRealMap) ? Theater_From_Name(mapData.Theater)
                                      : TheaterType::Temperate;
    pMap->CurrentTheater = pScenario->Theater;

    // 原版 skirmish 的缺省启动资金（[Basic] Money=）。
    pScenario->InitialMoney = 10000;

    // 光照：asm 里 [Lighting] Ambient/Red/Green/Blue 由 INIClass_GetFixed 读入
    //   后 fmul 100 存成定点（1.0 → 100），这里同尺度填 1.0 的白光。
    pScenario->AmbientOriginal     = 100;
    pScenario->AmbientCurrent      = 100;
    pScenario->AmbientTarget       = 100;
    pScenario->MapTintR            = 100;
    pScenario->MapTintG            = 100;
    pScenario->MapTintB            = 100;
    pScenario->NormalLighting.Tint = TintStruct(100, 100, 100);
    pScenario->NormalLighting.Ground = 0;
    pScenario->NormalLighting.Level  = 0;
    ScenarioClass::UpdateLighting();     // Current 朝 Target 收敛

    // ── 4. 参战方（玩家 + 敌方）──────────────────────────────────────────
    // asm: HouseClass_LoadFromINIList 先用 Country= 查 HouseTypeClass
    //   （FindOrAllocate_YesMP），再 new HouseClass(type)。
    if (HouseTypeClass::ArrayCount == 0)
        HouseTypeClass::Init_Defaults();     // asm: rulesmd.ini 的 [Houses]

    HouseTypeClass* pPlayerType = HouseTypeClass::Find("Americans");
    HouseTypeClass* pEnemyType  = HouseTypeClass::Find("Russians");

    if (pPlayerType == nullptr)
        pPlayerType = HouseTypeClass::FindOrAllocate("Americans");   // 内置兜底
    if (pEnemyType == nullptr)
        pEnemyType = HouseTypeClass::FindOrAllocate("Russians");     // 内置兜底

    if (pPlayerType == nullptr || pEnemyType == nullptr)
    {
        LogMessage("Failed to instantiate HouseType!");
        return false;
    }

    HouseClass* pPlayerHouse = nullptr;
    HouseClass* pEnemyHouse  = nullptr;

    if (HouseClass::ArrayCount == 0)
    {
        pPlayerHouse = GameCreateArgs<HouseClass>(pPlayerType);
        pEnemyHouse  = GameCreateArgs<HouseClass>(pEnemyType);

        if (pPlayerHouse == nullptr || pEnemyHouse == nullptr)
        {
            LogMessage("Failed to instantiate Houses!");
            return false;
        }

        std::strncpy(pPlayerHouse->InitialName, "Player",
                     sizeof(pPlayerHouse->InitialName) - 1);
        pPlayerHouse->InitialName[sizeof(pPlayerHouse->InitialName) - 1] = '\0';
        std::strncpy(pEnemyHouse->InitialName, "Enemy",
                     sizeof(pEnemyHouse->InitialName) - 1);
        pEnemyHouse->InitialName[sizeof(pEnemyHouse->InitialName) - 1] = '\0';

        // 玩家方：人类控制（asm HouseClass+0x1EC IsHuman / +0x1ED PlayerControl）
        pPlayerHouse->IsHumanPlayer = true;
        pPlayerHouse->PlayerControl = true;
        pPlayerHouse->CurrentPlayer = true;
        pPlayerHouse->Credits       = pScenario->InitialMoney;
        pEnemyHouse->Credits        = pScenario->InitialMoney;

        Register_House(pPlayerHouse);        // asm: HouseClass_CTOR 入 vec_Houses
        Register_House(pEnemyHouse);

        pPlayerHouse->MakeEnemy(pEnemyHouse);

        HouseClass::Player         = pPlayerHouse;
        HouseClass::pCurrentPlayer = pPlayerHouse;
        ThePlayerIndex             = pPlayerHouse->ArrayIndex;
    }
    else
    {
        // 已有房子（重入 / 存档恢复）：不重复创建，只补齐玩家索引。
        TheHouseCount = HouseClass::ArrayCount;
        ThePlayerIndex = 0;
        pPlayerHouse = HouseClass::Array[0];
        if (HouseClass::ArrayCount > 1)
            pEnemyHouse = HouseClass::Array[1];
    }

    // ── 4.5 表现层单例 ───────────────────────────────────────────────────
    // asm: Scenario_ClearClasses 先释放旧 TacticalMap，再 new TacticalClass；
    //   TacticalClass_CTOR 尾部把自身写进全局 TacticalMap。本树在场景初始化
    //   这里补上同一动作，否则 Render_Frame 全程拿到 nullptr、渲染链空转。
    //
    //   DisplayClass 在原版是 SidebarClass → PowerClass → RadarClass 构造链上
    //   的基类对象，全局 Map / Radar 就是同一个 RadarClass 实例。本树该 UI
    //   链尚未建模，DisplayClass 自身仍是抽象类（GetLastMouseCursor 为纯虚），
    //   故这里建一个 RadarClass 实例同时挂到 TheDisplay / TheRadar——这与
    //   原版"Radar 即 Map 即 Display"的对象身份一致。
    //   TacticalClass 的 CTOR 从 TheDisplay 取屏宽高，因此必须先建 Display。
    if (TheDisplay == nullptr)
    {
        RadarClass* pRadar = GameCreate<RadarClass>();
        TheRadar   = pRadar;
        TheDisplay = pRadar;             // RadarClass 公有继承 DisplayClass
    }

    if (TheTactical == nullptr)
        TheTactical = GameCreate<TacticalClass>();

    // ── 4.6 视频系统 + 五张离屏面 ─────────────────────────────────────────
    // asm: GScreenClass / DisplayClass 的视频模式建立顺序是
    //   SetVideoMode(w,h,bpp) -> (内部) RecreateSurfaces(vis, alt, tile, sidebar)
    //   五张面（Alternate / Hidden / Composite / Tile / Sidebar）全部由这一步
    //   建出。本树此前从没建过 TheGScreen，Render_Frame 拿 BackBuffer 恒为
    //   nullptr，整条渲染链空转 —— 这是"看得见东西"的最后一道闸。
    if (TheGScreen == nullptr)
    {
        const int32 screenW = 640;      // asm: 默认 640x400（WindowWidth）
        const int32 screenH = 400;

        TheGScreen = GameCreate<GameScreenClass>();
        if (TheGScreen != nullptr)
        {
            TheGScreen->SetVideoMode(screenW, screenH, 16);   // 16bpp = 5-6-5

            if (TheDisplay != nullptr)
            {
                // asm: SetVideoMode 的四个矩形参数
                //   vis     = 战术视口（屏幕宽减去侧边栏）
                //   alt     = 同视口
                //   tile    = 同视口
                //   sidebar = 侧边栏区（YR 固定 168 像素宽）
                //
                //   注意：RecreateSurfaces 里把 `.Right/.Bottom` **当宽高**用
                //   （asm 取 [rect+8]/[rect+0Ch] 直接喂给 MakeSurface），所以
                //   这里传的是 (l, t, w, h) 而不是右下角坐标。
                //
                //   以前 sidebarRect 硬编码 1x1，于是 DSurface_Sidebar 只有
                //   一个像素 —— 侧边栏的所有 SHP（SIDE1/2/3、cameo、按钮）
                //   全被压到那一个点上，等于没画。
                const int32 kSidebarWidth = 168;
                const int32 viewW = (screenW > kSidebarWidth)
                                  ? (screenW - kSidebarWidth) : screenW;

                const LTRBStruct visRect(0, 0, viewW, screenH);
                const LTRBStruct altRect(0, 0, viewW, screenH);
                const LTRBStruct tileRect(0, 0, viewW, screenH);
                const LTRBStruct sidebarRect(viewW, 0, kSidebarWidth, screenH);

                TheDisplay->RecreateSurfaces(visRect, altRect, tileRect,
                                             sidebarRect);
            }
        }
    }

    // ── 5. 起始对象 ──────────────────────────────────────────────────────
    // asm: ScenarioClass_CreateUnits (0x688690) —— 按剧本 / 大厅设置给每个
    //   AI 房铺开局部队，落位经 ObjectClass::Unlimbo 尾部进 vec_Logics，
    //   从此每帧被驱动。真实入口在 Scenario/ScenarioClass.cpp。
    TechnoClass* pViewAnchor = nullptr;

    if (pPlayerHouse != nullptr)
    {
        TechnoClass::Init_Array();           // asm: vec_Techno 的静态表
        Ensure_Class_Array(UnitClass::Array);       // 内置兜底，见函数注释
        Ensure_Class_Array(InfantryClass::Array);   // 内置兜底，见函数注释

        const int32 unitsBefore = (UnitClass::Array != nullptr)
                                      ? UnitClass::Array->Count : 0;
        const int32 infBefore   = (InfantryClass::Array != nullptr)
                                      ? InfantryClass::Array->Count : 0;

        // 真实路径：ScenarioClass::Create_Units。
        pScenario->CreateUnits();

        const int32 unitsAfter = (UnitClass::Array != nullptr)
                                     ? UnitClass::Array->Count : 0;
        const int32 infAfter   = (InfantryClass::Array != nullptr)
                                     ? InfantryClass::Array->Count : 0;

        // 真实路径没产出任何单位（规则未加载 / 大厅 UnitCount=0 / 只有真人房）
        // 时退回内置兜底：玩家基地落一辆灰熊 + 一个美国大兵。
        if (unitsAfter == unitsBefore && infAfter == infBefore)
        {
            const int32 baseX = pMap->MapWidth / 4;
            const int32 baseY = pMap->MapHeight / 4;

            // 灰熊坦克（rulesmd [MTNK]）
            UnitTypeClass* pUnitType = UnitTypeClass::FindOrAllocate("MTNK");
            if (pUnitType != nullptr)
            {
                pViewAnchor = Place_Starting_Techno<UnitClass>(
                    pPlayerHouse, pUnitType, CellStruct(baseX, baseY));
            }

            // 美国大兵（rulesmd [E1]）
            InfantryTypeClass* pInfType = InfantryTypeClass::FindOrAllocate("E1");
            if (pInfType != nullptr)
            {
                TechnoClass* pInfantry = Place_Starting_Techno<InfantryClass>(
                    pPlayerHouse, pInfType, CellStruct(baseX + 2, baseY + 1));
                if (pViewAnchor == nullptr)
                    pViewAnchor = pInfantry;
            }
        }
        else
        {
            // 真实路径产出了单位：相机要落在**玩家自己的基地**上。
            //
            // 原版开局后视口停在玩家的基地车（MCV）上。取"数组最后一个"
            // 会让相机落到最后一个被 Create_Units 随机摆下的兵身上 ——
            // 敌人的出生点、或地图角上的散兵，画面一开局就对着荒地，
            // 而且常常在地图边缘外半屏黑。这里按
            //   ① 玩家拥有的基地车（Rules.BaseUnit 列表里的类型）
            //   ② 玩家拥有的第一个载具
            //   ③ 玩家拥有的第一个步兵
            //   ④ 最后一个载具（敌人也算，兜底）
            // 的顺序挑锚点。
            if (pPlayerHouse != nullptr && UnitClass::Array != nullptr)
            {
                const DynamicVectorClass<UnitTypeClass*>* pBaseList =
                    (RulesClass::Instance != nullptr) ? &RulesClass::Instance->BaseUnit
                                                      : nullptr;

                for (int32 i = 0; i < UnitClass::Array->Count && pViewAnchor == nullptr; ++i)
                {
                    UnitClass* pUnit = UnitClass::Array->Items[i];
                    if (pUnit == nullptr || pUnit->Owner != pPlayerHouse)
                        continue;
                    if (pBaseList != nullptr && pUnit->Type != nullptr)
                    {
                        for (int32 b = 0; b < pBaseList->Count; ++b)
                        {
                            if ((*pBaseList)[b] == pUnit->Type)
                            {
                                pViewAnchor = pUnit;
                                break;
                            }
                        }
                    }
                }

                for (int32 i = 0; i < UnitClass::Array->Count && pViewAnchor == nullptr; ++i)
                {
                    UnitClass* pUnit = UnitClass::Array->Items[i];
                    if (pUnit != nullptr && pUnit->Owner == pPlayerHouse)
                        pViewAnchor = pUnit;
                }
            }

            if (pViewAnchor == nullptr && InfantryClass::Array != nullptr)
            {
                for (int32 i = 0; i < InfantryClass::Array->Count && pViewAnchor == nullptr; ++i)
                {
                    InfantryClass* pInf = InfantryClass::Array->Items[i];
                    if (pInf != nullptr && pInf->Owner == pPlayerHouse)
                        pViewAnchor = pInf;
                }
            }

            if (pViewAnchor == nullptr && UnitClass::Array != nullptr &&
                UnitClass::Array->Count > 0)
                pViewAnchor = UnitClass::Array->Items[UnitClass::Array->Count - 1];

            if (pViewAnchor == nullptr && InfantryClass::Array != nullptr &&
                InfantryClass::Array->Count > 0)
                pViewAnchor = InfantryClass::Array->Items[InfantryClass::Array->Count - 1];
        }
    }

    // 把战术视图对准第一个落位单位。TacticalPos 与 CoordsToScreen 同处
    //   "等距投影像素"空间（GetTacticalOrigin = TacticalPos - 半屏），所以
    //   直接取该单位的投影坐标即等于把它摆在屏幕中心。不这么做时默认
    //   TacticalPos=(0,0) 会把基地推到视口外，[RENDER] 只能看到 drawn=0。
    if (TheTactical != nullptr && pViewAnchor != nullptr)
    {
        CoordStruct anchorCoord;
        pViewAnchor->GetCoords(&anchorCoord);
        // 走 SetTacticalPosition 而不是直接写 TacticalPos：它内部会做
        //   Clamp_Tactical_Position，保证视口不会顶出地图边界（否则基地
        //   靠边时开局画面就有一大片黑）。
        TheTactical->SetTacticalPosition(&anchorCoord);
    }

    // DisplayClass is constructed together with the graphics system, which
    // can happen after the first objects are already on the map (Create_Units
    // runs during scenario init).  Objects placed before that point were never
    // submitted to the tactical layers, so re-register the whole world once the
    // display exists -- otherwise the first frame reports visible=0 for a
    // fully populated map.
    if (DisplayClass::Instance != nullptr && TechnoClass::Array != nullptr)
    {
        for (int32 i = 0; i < TechnoClass::Array->Count; ++i)
        {
            TechnoClass* pTechno = (*TechnoClass::Array)[i];
            if (pTechno != nullptr && !pTechno->IsInLimbo)
                DisplayClass::Instance->Submit(pTechno);
        }
    }

    g_bMapLoaded = true;

    {
        char buffer[224];
        std::snprintf(buffer, sizeof(buffer),
                      "Scenario initialized (map %dx%d, mapcells=%d, houses=%d,"
                      " units=%d, infantry=%d, logics=%d, anchor_hp=%d).",
                      pMap->MapWidth, pMap->MapHeight, pMap->CellCount,
                      TheHouseCount,
                      UnitClass::Array ? (int)UnitClass::Array->Count : -1,
                      InfantryClass::Array ? (int)InfantryClass::Array->Count : -1,
                      vec_Logics ? (int)vec_Logics->Count : -1,
                      (pViewAnchor != nullptr) ? (int)pViewAnchor->Health : -1);
        LogMessage(buffer);
    }
    return true;
}

// ───────────────────────────────────────────────────────────────────────────
// 根据游戏行为，可知 AI_Impl 负责下面这段逻辑。
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
// 根据游戏行为，可知 Network_Impl 负责下面这段逻辑。
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
        // 走 stderr 并立即 flush：stdout 重定向到文件时是全缓冲，
        // 被 timeout/SIGTERM 杀掉时日志会整块丢失，不利于冒烟定位。
        fprintf(stderr, "[GAME] %s\n", msg);
        fflush(stderr);
    }
#endif
}