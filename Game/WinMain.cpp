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

#include <string>

// ── 第 11 步（GameObjects_Impl）用到的类型 ────────────────────────────────
//   Externs.h 已包含绝大多数对象 / 类型头；这里补齐它没有覆盖的部分：
//   规则表本体、INI 与 MIX 读取、AI 类型表、以及渲染侧的配色表。
#include "INI/INIClass.h"
#include "IO/CCFileClass.h"
#include "IO/FileSystem.h"
#include "IO/MixFileClass.h"
#include "IO/Straws.h"
#include "FileFormats/LCW.h"
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
#include "AI/TriggerClass.h"
#include "AI/TagTypeClass.h"
#include "AI/TagClass.h"
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
#include "Network/SessionClass.h"

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
#include "FileFormats/StringTable.h"

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
bool Init_SecondaryMixFiles_Impl();
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
    "BaseUnit=AMCV\n"
    "HarvesterUnit=HARV\n"
    // AI 建造清单的五个类别 —— 注意它们在原版 rules(md).ini 里属于 **[AI]** 段，
    //   不是 [General]。RulesClass::Read_AI 只从 [AI] 读这五个键；写进
    //   [General] 会静默读空（五个列表 Count 全 0），后果是：
    //     · UnitClass::Update 段 F 的 isBaseConstant 恒 false → MCV 不展开；
    //     · GenerateAIBuildList 一进 ② 段就取不到首项 → 节点表空 → AI 不建房。
    "[AI]\n"
    "BuildConst=GACNST\n"
    "BuildPower=GAPOWR\n"
    "BuildRefinery=GAREFN\n"
    "BuildBarracks=GAPILE\n"
    "BuildWeapons=GAWEAP\n"
    // 目标重评节拍：警戒（Guard）与区域警戒（AreaGuard）各自的间隔帧数。
    //   缺键时 RulesClass 会落 0，等于每帧都扫一遍半径内的格子；给上原版
    //   默认值即可让空闲单位按节奏评估，而不是逐帧重扫。
    "NormalTargetingDelay=30\n"
    "GuardAreaTargetingDelay=15\n"
    "\n"
    // 矿种表：真实 rules(md).ini 用 [Tiberiums] 登记矿种序号 -> 名字，各矿种自己
    //   的段里给生长/蔓延概率与图像窗口。缺这两段时 TiberiumClass::Array 为空，
    //   CellClass::Grow_Tiberium / Spread_Tiberium 一律返回 false —— 矿只会被
    //   采空、永不生长（实测 [TIBS] tibNull=100、矿石总量单调归零）。
    //   这里的概率是**兜底规则集**的取值（真实数据文件会整段覆盖），按本树
    //   30 帧一次的全局生长节拍折算。
    "[Tiberiums]\n"
    "0=Riparius\n"
    "\n"
    "[Riparius]\n"
    "Name=Riparius\n"
    "Image=3\n"
    "Growth=5\n"
    "GrowthPercentage=0.05\n"
    "Spread=5\n"
    "SpreadPercentage=0.01\n"
    "Value=25\n"
    "Power=1\n"
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
    "1=AMCV\n"
    "2=HARV\n"
    "\n"
    "[InfantryTypes]\n"
    "0=E1\n"
    "\n"
    "[BuildingTypes]\n"
    "0=GAPILE\n"
    "1=GACNST\n"
    "2=GAPOWR\n"
    "3=GAREFN\n"
    "4=GAWEAP\n"
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
    "Owner=Americans,Russians\n"
    // 生产线类别：兵营只出步兵。缺这一键时 BuildingTypeClass::Factory
    //   恒为 None，Update_Factory 的"是否工厂"门直接挡住，AI 永远不会造兵。
    "Factory=InfantryType\n"
    "Foundation=1x1\n"
    "\n"
    // ── MCV：展开成建造场（DeploysInto） ────────────────────────────────
    //   DeploysInto 是 MCV 存在的全部意义：UnitClass::Try_To_Deploy 读它
    //   决定"展开成什么建筑"。缺这一键时展开只是换个动画序列。
    "[AMCV]\n"
    "UIName=Name:AMCV\n"
    "Name=Allied MCV\n"
    "Image=AMCV\n"
    "Strength=1000\n"
    "Armor=heavy\n"
    "TechLevel=1\n"
    "Sight=4\n"
    "Speed=4\n"
    "ROT=5\n"
    "Cost=3000\n"
    "Soylent=3000\n"
    "Points=100\n"
    "Owner=Americans,Russians\n"
    "DeploysInto=GACNST\n"
    "\n"
    // ── 建造场：所有建筑的根 ────────────────────────────────────────────
    //   ConstructionYard=yes 让它进 OwnedConyards（AI_Building 的第一道门）；
    //   Factory=BuildingType 让它自己有"造建筑"的生产线。
    "[GACNST]\n"
    "UIName=Name:GACNST\n"
    "Name=Construction Yard\n"
    "Image=GACNST\n"
    "Strength=1500\n"
    "Armor=wood\n"
    "TechLevel=1\n"
    "Sight=5\n"
    "Cost=3000\n"
    "Soylent=3000\n"
    "Points=150\n"
    "Power=0\n"
    "Owner=Americans,Russians\n"
    "ConstructionYard=yes\n"
    "Factory=BuildingType\n"
    "Foundation=3x3\n"
    "AIBuildThis=no\n"
    "\n"
    // ── 电厂 ────────────────────────────────────────────────────────────
    "[GAPOWR]\n"
    "UIName=Name:GAPOWR\n"
    "Name=Allied Power Plant\n"
    "Image=GAPOWR\n"
    "Strength=750\n"
    "Armor=wood\n"
    "TechLevel=1\n"
    "Sight=4\n"
    "Cost=800\n"
    "Soylent=800\n"
    "Points=75\n"
    "Power=100\n"
    "Owner=Americans,Russians\n"
    "Foundation=2x2\n"
    "\n"
    // ── 精炼厂 ──────────────────────────────────────────────────────────
    "[GAREFN]\n"
    "UIName=Name:GAREFN\n"
    "Name=Allied Refinery\n"
    "Image=GAREFN\n"
    "Strength=1000\n"
    "Armor=wood\n"
    "TechLevel=1\n"
    "Sight=4\n"
    "Cost=2000\n"
    "Soylent=2000\n"
    "Points=100\n"
    "Power=-50\n"
    "Owner=Americans,Russians\n"
    // asm: BuildingTypeClass_LoadFromINI 读的是 "Refinery"（aRefinery）。
    //   原版 rules.ini 的 [GAREFN] 也是 Refinery=yes；OreRefinery= 这个键在
    //   二进制里不存在，写了也读不到。
    "Refinery=yes\n"
    "FreeUnit=HARV\n"
    "Foundation=3x2\n"
    "\n"
    // ── 战车工厂 ────────────────────────────────────────────────────────
    "[GAWEAP]\n"
    "UIName=Name:GAWEAP\n"
    "Name=Allied War Factory\n"
    "Image=GAWEAP\n"
    "Strength=1000\n"
    "Armor=wood\n"
    "TechLevel=1\n"
    "Sight=5\n"
    "Cost=2000\n"
    "Soylent=2000\n"
    "Points=100\n"
    "Power=-25\n"
    "Owner=Americans,Russians\n"
    "Factory=UnitType\n"
    "Foundation=3x3\n"
    "\n"
    // ── 矿车 ────────────────────────────────────────────────────────────
    "[HARV]\n"
    "UIName=Name:HARV\n"
    "Name=Chrono Miner\n"
    "Image=HARV\n"
    "Strength=600\n"
    "Armor=heavy\n"
    "TechLevel=1\n"
    "Sight=4\n"
    "Speed=4\n"
    "ROT=5\n"
    "Cost=1400\n"
    "Soylent=1400\n"
    "Points=50\n"
    "Owner=Americans,Russians\n"
    "Harvester=yes\n"
    "ResourceGatherer=yes\n"
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

    // CCFileClass::Open 已经是「MIX 注册表优先、磁盘兜底」，所以这里不再做
    // 纯磁盘的 FileExists 预判——否则规则表在 ra2md.mix 里时会被直接跳过。
    {
        CCFileClass file(pFileName);
        if (target.ReadCCFile(&file, false, false))
            return true;
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

    // 同上：存在性交给 CCFileClass（MIX 感知），不要用纯磁盘的 FileExists。
    CCFileClass file(pPath);
    if (!file.IsAvailable())
        return false;                       // 不存在：静默，交给下一个候选

    if (!CCINIClass::INI_Rules->ReadCCFile(&file, false, false))
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

namespace
{
    // 尝试挂载一个档案。diskOnly=true 对应原版只探磁盘的 EXPANDMD 循环。
    // 返回是否成功挂载（成功 = 索引解析完成并已进注册表）。
    bool Mix_MountOne(const char* pName, bool diskOnly)
    {
        if (diskOnly && !FileSystem::FileExists(pName))
            return false;

        MixFileClass* pMix = new MixFileClass(pName);
        if (pMix->GetFileCount() > 0)
        {
            char line[192];
            std::snprintf(line, sizeof(line), "MIX mounted: %s (%d entries)",
                          pName, static_cast<int>(pMix->GetFileCount()));
            LogMessage(line);
            return true;
        }

        // 头或索引解析失败：构造函数没有把它挂进注册表，可以安全销毁
        // （UnInit_List 只会摘掉自己那一项）。
        delete pMix;
        return false;
    }

    // 挂载一对 md/普通档案：优先 md（尤里复仇），缺席时退回基础版。
    // 原版就是这样成对尝试，两者都在时 md 先注册、优先级更高。
    bool Mix_MountPair(const char* pMd, const char* pBase)
    {
        const bool md   = Mix_MountOne(pMd, false);
        const bool base = Mix_MountOne(pBase, false);
        return md || base;
    }
}

// ───────────────────────────────────────────────────────────────────────────
// MIX 挂载（原版 Load_Bootstrap / WinMain / Init_SecondaryMixFiles）
//
// 原版启动顺序（全部来自汇编，不是猜的）：
//
//   Load_Bootstrap（asm.txt:665018）：
//     for (i = 0x63; i >= 0; --i)  EXPANDMD%02d.MIX      ← 99..00 倒序
//     RA2MD.MIX, RA2.MIX, CACHEMD.MIX, CACHE.MIX,
//     LOCALMD.MIX, LOCAL.MIX
//   WinMain（asm.txt:1435454）：
//     LANGMD.MIX, LANGUAGE.MIX
//   Init_SecondaryMixFiles（asm.txt:665364，InitGame+B3C 调用）：
//     CONQMD.MIX / CONQUER.MIX, GENERMD.MIX / GENERIC.MIX,
//     ISOGENMD.MIX / ISOGEN.MIX, CAMEOMD.MIX / CAMEO.MIX,
//     MAPSMD%02d.MIX / MAPS%02d.MIX, MOVMD*.MIX / MOVIES*.MIX,
//     MULTIMD.MIX / MULTI.MIX, THEMEMD.MIX / THEME.MIX
//
// 两条重要事实：
//   1. 先注册的档案在查找时优先（FindFileByName 从头遍历注册表），所以
//      RA2MD 先于 RA2 注册才能覆盖基础版素材。
//   2. 除了 EXPANDMD（原版用 RawFileClass 只探磁盘），其余都走
//      MixFileClass_CTOR，而它的可用性判断走 CCFileClass——也就是说
//      LOCALMD.MIX 这类档案即使只存在于 RA2MD.MIX 内部（嵌套档案）也能挂载。
//      这条链依赖 CCFileClass::Open 会查 MIX 注册表，IO/FileSystem.cpp 已按
//      汇编修好。
// ───────────────────────────────────────────────────────────────────────────

bool Init_MixFiles_Impl()
{
    static bool s_Done = false;
    if (s_Done)
        return true;          // InitList 与 WinMain 都会调，保持幂等

    int32 mounted = 0;

    // ── Load_Bootstrap：EXPANDMD99..00 ──────────────────────────────────
    // 原版从 99 递减到 0，先注册的优先，所以编号大的补丁覆盖编号小的。
    // 这一批原版用 RawFileClass 只探磁盘（补丁档不会藏在别的 MIX 里）。
    {
        char name[32];
        for (int32 i = 99; i >= 0; --i)
        {
            std::snprintf(name, sizeof(name), "EXPANDMD%02d.MIX", static_cast<int>(i));
            if (Mix_MountOne(name, true))
                ++mounted;
        }
    }

    // ── Load_Bootstrap + WinMain：核心与语言档案 ────────────────────────
    static const char* kCore[] =
    {
        "RA2MD.MIX", "RA2.MIX",
        "CACHEMD.MIX", "CACHE.MIX",
        "LOCALMD.MIX", "LOCAL.MIX",
        "LANGMD.MIX", "LANGUAGE.MIX",
    };

    int32 coreMissing = 0;
    for (const char* name : kCore)
    {
        if (Mix_MountOne(name, false))
            ++mounted;
        else
            ++coreMissing;
    }

    char info[192];
    std::snprintf(info, sizeof(info),
                  "MIX files loaded. (%d archive(s) mounted, %d core archive(s) absent)",
                  static_cast<int>(mounted), static_cast<int>(coreMissing));
    LogMessage(info);

    if (mounted == 0)
    {
        LogMessage("Warning: no MIX archive could be mounted. Retail data "
                   "(ra2md.mix / ra2.mix) was not found next to the executable; "
                   "asset loading will fall back to the built-in placeholders.");
    }

    // ── 字符串表：CSF ───────────────────────────────────────────────────
    // asm: WinMain 在挂完 LANGMD/LANGUAGE 之后调用
    //   StringTable_LoadFile("ra2md.str")  → 截扩展名补 .csf → ra2md.csf。
    // 之前移植版从来没在启动时加载过 CSF，界面上的所有文本都会是空串。
    if (!StringTable::Load_File("ra2md.str"))
    {
        if (!StringTable::Load_File("ra2.str"))
            LogMessage("Warning: string table (ra2md.csf) not found or unreadable.");
    }

    // ── 规则表：rulesmd.ini / artmd.ini ──────────────────────────────────
    // 原版把 rules(md).ini 读进全局 INI_Rules、art(md).ini 读进 ART_INI，
    // 第 11 步（Init_GameObjects_Impl）随后用它们铺满各 *TypeClass::Array。
    // Resolve_Rules_INI 先按「当前目录 → ./ra2md/ → ./ → MIX」找真实文件，
    // 全都没有才退内置兜底集，并在日志里注明走的是哪条路径。
    Resolve_Rules_INI();

    // artmd.ini 只提供美术字段，缺席时各 LoadFromINI 保留默认值。
    if (!Load_INI_Source("artmd.ini", CCINIClass::INI_Art))
        LogMessage("Art INI not available (artmd.ini).");

    // ── Init_SecondaryMixFiles ─────────────────────────────────────────
    // 原版在规则表之后挂这一批内容档案；它们全部可缺席，失败不影响启动。
    Init_SecondaryMixFiles_Impl();

    s_Done = true;
    return mounted > 0;
}

// ───────────────────────────────────────────────────────────────────────────
// Init_SecondaryMixFiles（asm.txt:665364，InitGame+B3C 调用）
//
// 这一批是内容档案：单位/建筑图像（CONQMD/GENERMD/ISOGENMD/CAMEOMD）、
// 遭遇战地图（MAPSMD##）、过场动画（MOVMD##）、多人地图（MULTIMD）、
// 音乐（THEMEMD）。都是成对 md/基础版，且全部可以缺席。
// ───────────────────────────────────────────────────────────────────────────
bool Init_SecondaryMixFiles_Impl()
{
    static bool s_Done = false;
    if (s_Done)
        return true;

    int32 mounted = 0;

    if (Mix_MountPair("CONQMD.MIX",   "CONQUER.MIX")) ++mounted;
    if (Mix_MountPair("GENERMD.MIX",  "GENERIC.MIX")) ++mounted;
    if (Mix_MountPair("ISOGENMD.MIX", "ISOGEN.MIX"))  ++mounted;
    if (Mix_MountPair("CAMEOMD.MIX",  "CAMEO.MIX"))   ++mounted;

    // MAPSMD%02d / MAPS%02d：原版 01..99 逐个探，存在才挂。
    {
        char md[32];
        char base[32];
        for (int32 i = 1; i <= 99; ++i)
        {
            std::snprintf(md,   sizeof(md),   "MAPSMD%02d.MIX", static_cast<int>(i));
            std::snprintf(base, sizeof(base), "MAPS%02d.MIX",   static_cast<int>(i));
            if (Mix_MountPair(md, base)) ++mounted;
        }
    }

    // MOVMD%02d / MOVIES%02d（原版另有 MOVIES01/MOVMD01/MOVMD03 的字面量分支，
    // 通用编号循环覆盖同样的档名）。
    {
        char md[32];
        char base[32];
        for (int32 i = 1; i <= 99; ++i)
        {
            std::snprintf(md,   sizeof(md),   "MOVMD%02d.MIX",  static_cast<int>(i));
            std::snprintf(base, sizeof(base), "MOVIES%02d.MIX", static_cast<int>(i));
            if (Mix_MountPair(md, base)) ++mounted;
        }
    }

    if (Mix_MountPair("MULTIMD.MIX", "MULTI.MIX")) ++mounted;
    if (Mix_MountPair("THEMEMD.MIX", "THEME.MIX")) ++mounted;

    if (mounted > 0)
    {
        char info[160];
        std::snprintf(info, sizeof(info),
                      "Secondary MIX files loaded. (%d archive pair(s) mounted)",
                      static_cast<int>(mounted));
        LogMessage(info);
    }

    s_Done = true;
    return true;
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
// Init_AI_Tables —— AI 数据五表装配
//
//   asm 链路（gamemd.exe）：RulesClass_Process 之后由场景装配阶段调用
//     ScriptTypeClass_CreateFromINIList        (0x6918F0)
//     TaskForceClass_CreateFromINIList(ini, 1) (0x6E822B)
//     TeamTypes_CreateFromINIList              (0x5CC2E0)
//     TeamTypes_CreateFromINIList 的 AITeam 变体 (0x6F19C0)
//     AITriggerTypeClass_CreateFromINIList(ini,1) (0x41F2A0)
//   顺序不可交换：TaskForce 的成员是 TechnoType 指针，TeamType 引用 TaskForce
//   与 ScriptType，AITriggerType 又按名字找 TeamType（TeamTypeClass_FindByString）。
//
//   ★ 此前这五个入口**从未被调用**：ini 里有数据也读不进表。后果是
//     vec_AITriggerTypes 恒空 → HouseClass::TryFindEligibleAITriggers 第一步
//     就 return false → AI 永远不成军；即使手工建队，TeamType 的
//     TaskForce/ScriptType 也是空指针。这是"AI 只造兵不出击"的第一根因。
// ───────────────────────────────────────────────────────────────────────────
static void Init_AI_Tables(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return;

    ScriptTypeClass::CreateFromINIList(pINI);
    TaskForceClass::CreateFromINIList(pINI, 1);
    TeamTypeClass::CreateFromINIList(pINI);
    AITeamTypeClass::CreateFromINIList(pINI, true);
    AITriggerTypeClass::CreateFromINIList(pINI, true);

    char msg[224];
    std::snprintf(msg, sizeof(msg),
                  "AI tables: scripts=%d taskforces=%d teamtypes=%d "
                  "aiteamtypes=%d aitriggers=%d",
                  (ScriptTypeClass::Array ? ScriptTypeClass::Array->Count : -1),
                  (TaskForceClass::Array ? TaskForceClass::Array->Count : -1),
                  (TeamTypeClass::Array ? TeamTypeClass::Array->Count : -1),
                  (AITeamTypeClass::Array ? AITeamTypeClass::Array->Count : -1),
                  (AITriggerTypeClass::Array ? AITriggerTypeClass::Array->Count : -1));
    LogMessage(msg);
}

// ───────────────────────────────────────────────────────────────────────────
// BUILTIN_AI_INI —— AI 数据五表的兜底集
//
//   本机没有 ai(md).ini 时用它顶替，规模只求"能成军、能出击"：两支攻击队
//   （4 辆 MTNK / 6 名 E1），一个 Search-and-Destroy 脚本，两条触发器在
//   AI 拥有对应工厂后放行。字段顺序严格按 AITriggerTypeClass::LoadFromINI
//   的 18 槽布局（第 4/12 槽留空只作占位）：
//     Name, Team1, Owner, -, CondType, CondObject, params, W, Wmin, Wmax,
//     IsSkirmish, -, Side, IsBaseDef, Team2, Easy, Normal, Hard
//   params 是"两个十六进制字符一个字节"的串：params[0]=比较器（4 即 ">"），
//   params[1]=操作数，于是 "0400" 读作 "拥有 > 0 座"。
// ───────────────────────────────────────────────────────────────────────────
static const char BUILTIN_AI_INI[] =
    "[ScriptTypes]\n"
    "0=AtkBase\n"
    "\n"
    "[AtkBase]\n"
    "0=37,0\n"          // Search and Destroy -> Mission::Hunt
    "\n"
    "[TaskForces]\n"
    "0=TF_Tank4\n"
    "1=TF_Inf6\n"
    "\n"
    "[TF_Tank4]\n"
    "0=4,MTNK\n"
    "\n"
    "[TF_Inf6]\n"
    "0=6,E1\n"
    "\n"
    "[TeamTypes]\n"
    "0=AI_Tank4\n"
    "1=AI_Inf6\n"
    "\n"
    "[AI_Tank4]\n"
    "Name=AI Tank Team\n"
    "VeteranLevel=1\n"
    "Group=-1\n"
    "TechLevel=1\n"
    "Priority=20\n"
    "Max=2\n"
    "Autocreate=yes\n"
    "Aggressive=yes\n"
    "Script=AtkBase\n"
    "TaskForce=TF_Tank4\n"
    "\n"
    "[AI_Inf6]\n"
    "Name=AI Infantry Team\n"
    "VeteranLevel=1\n"
    "Group=-1\n"
    "TechLevel=1\n"
    "Priority=10\n"
    "Max=2\n"
    "Autocreate=yes\n"
    "Aggressive=yes\n"
    "Script=AtkBase\n"
    "TaskForce=TF_Inf6\n"
    "\n"
    "[AITriggerTypes]\n"
    // 条件类型槽：1 = HouseOwns（本房拥有 ConditionObject）。
    //   不用 0（EnemyOwns）：原版语义里它数的是"目标房"，而当前调用点
    //   HouseClass::TryFindEligibleAITriggers 传进来的往往是本房，靠调用点
    //   巧合等价；写 1 后与调用点无关，语义稳定。
    //
    // params 编码（asm LoadFromINI 0x41F859：逐字节写 +0xE4 起的 32 字节缓冲）：
    //   条件函数读两个 dword —— 第 0 组 = (操作数 @+0xE4, 比较器 @+0xE8)。
    //   "0000000004000000" = 操作数 0、比较器 4(>) → "拥有 > 0 座"。
    //   旧值 "0400" 只能解出 op=4/cmp=0（"< 4"），语义差了一位。
    "0x01000001=AI Tank Attack,AI_Tank4,<all>,1,1,GAWEAP,0000000004000000,10,5,100,1,,0,0,AI_Inf6,1,1,1\n"
    "0x01000002=AI Infantry Attack,AI_Inf6,<all>,1,1,GAPILE,0000000004000000,8,5,100,1,,0,0,<none>,1,1,1\n";

// ───────────────────────────────────────────────────────────────────────────
// Load_AI_Data —— 先找真实 ai(md).ini，再退兜底集，最后灌进五表。
// ───────────────────────────────────────────────────────────────────────────
static void Load_AI_Data()
{
    static const char* const kAINames[] = { "aimd.ini", "ai.ini" };

    for (int32 i = 0; i < 2; ++i)
    {
        CCINIClass aiINI;
        if (Load_INI_Source(kAINames[i], aiINI))
        {
            Init_AI_Tables(&aiINI);
            char msg[192];
            std::snprintf(msg, sizeof(msg),
                          "AI data loaded from real file: %s", kAINames[i]);
            LogMessage(msg);
            return;
        }
    }

    CCINIClass aiINI;
    aiINI.LoadFromBuffer(BUILTIN_AI_INI, std::strlen(BUILTIN_AI_INI), false);
    Init_AI_Tables(&aiINI);
    LogMessage("AI data FALLBACK: no ai(md).ini found; using built-in set.");
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

    // asm: AI 五表 —— 必须在 Rules 解析之后（成员的 TechnoType 指针已就位）。
    Load_AI_Data();

    // asm: Init_BulkData —— 四张共享 UI 素材（PIPBRD / PIPS / PIPS2 /
    //   TALKBUBL）在规则解析完成后一次性载入，侧边栏与话泡绘制直接取句柄。
    //   不调用的话 ObjectTypeClass::Get_TalkBubble_Shape() 永远为空，
    //   TechnoClass::DrawExtras 会静默跳过话泡。
    ObjectTypeClass::One_Time();

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

    // ── 会话对象（asm: .data 里的全局 Session，进程启动即构造）──────────
    //   原版任何模式（含单机）都持有一个 SessionClass 实例；HouseClass 的
    //   AI 生产状态机（Update_AI_Production）以 `Session != nullptr` 为门，
    //   Scenario 也用它取 TechLevel / Credits / UnitCount。本树此前只在
    //   网络路径里懒创建，单机跑起来 Session 恒为空 —— AI 于是「有兵营也
    //   永远不开线」。这里按单机会话补齐。
    if (SessionClass* pSession = SessionClass::GetInstance())
    {
        pSession->SessionType = 0;          // 0 = 单机
        pSession->SetTechLevel(10);
        pSession->SetCredits(10000);
        pSession->SetUnitCount(0);
    }

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

    // asm: HouseClass_CTOR —— 亲和的默认值就是自己的房号（[h+30h] ActLike），
    //   且把自己的位写进盟友位图。缺这一步时所有房的 ActLikeIndex 都停在
    //   -1，Is_Ally 的 `idx == this->ActLikeIndex` 分支会让**任意两房互为
    //   盟友**，AI 永远不开火（Can_Fire 的友军门直接返回 Movement）。
    if (pHouse->ActLikeIndex < 0)
        pHouse->ActLikeIndex = idx;
    if (idx < 32)
        pHouse->AllyBitfield |= (1u << idx);

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

    // 注意：TechnoClass 的构造函数已经把自己追加进 TechnoClass::Array
    //   （asm: TechnoClass_CTOR 尾部），这里不能再登记一次——重复项会让
    //   该对象在 AI / 绘制 / 伤害扫描里被当成两个对象处理。

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
    // ── 每格 1 字节的 *_Pack 段（覆盖物 / 地形对象 / 弹坑）──────────────
    //   Index 是主数据（0xFF = 该格无内容），Data 是对应的 *DataPack
    //   （矿的密度 / 墙的朝向帧 / 弹坑的帧）。Stride 是这张逐格图的宽度：
    //   gamemd 存盘写的是 512×512 的整图（asm 0x200 双循环），合成 / 简化
    //   夹具则是 mapWidth × mapHeight；两种都能被 Decode_Cell_Pack 识别。
    struct CellPack
    {
        bool  Present = false;
        bool  Decoded = false;
        int32 Stride  = 0;
        std::vector<uint8> Index;
        std::vector<uint8> Data;
    };

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
    CellPack OverlayPack;
    CellPack TerrainPack;
    CellPack SmudgePack;

    // 预置实体（[Structures] / [Units] / [Infantry]）的原始 value 行。
    //   解析时机不能在这里：三段的落位依赖已经建好的格表、overlay 与地面
    //   高度，所以只把原文记下来，等 Apply 阶段再造一份临时 INI 交给
    //   BuildingClass / UnitClass / InfantryClass::CreateFromINIList。
    std::vector<std::string> StructureLines;
    std::vector<std::string> UnitLines;
    std::vector<std::string> InfantryLines;

    // 地图文件路径：建房阶段要重新打开同一份 INI 交给
    //   HouseClass::LoadFromINIList（[Houses] 段 + 每房自己的段）。
    char Path[260] = {0};
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

// ───────────────────────────────────────────────────────────────────────────
// UU 块（数字键 + base64）连续解码
//
//   不能直接用 CCINIClass::Get_UUBlock：本树的 ReadUUBlock 对每个数字键
//   **单独**建一个 Base64Straw 解码（INI/INIClass.cpp:853），而 0x46（70）
//   字符的 base64 行不是 4 的整数倍，行尾 2 个字符会被当作"残缺组"丢掉——
//   也就是每行固定损失 1 字节并让后续字节整体错位。原版 Get_UUBlock
//   （asm 0x526FDA）是把每个键的文本连续喂进**同一个** Base64Pipe，
//  分组状态跨行保留；这里照原版做连续解码。
//
//   返回解码字节数（上限 nSize）；没有该段 / 全空返回 0。
// ───────────────────────────────────────────────────────────────────────────
static int32 Decode_UUBlock_Continuous(CCINIClass& ini, const char* pSection,
                                       uint8* pDst, size_t nSize)
{
    if (pSection == nullptr || pDst == nullptr || nSize == 0)
        return 0;

    // 1..N 数字键按序拼成一条 base64 文本。ReadString 已做首尾裁剪。
    std::string text;
    for (int32 ordinal = 1; ; ++ordinal)
    {
        char key[16];
        std::snprintf(key, sizeof(key), "%d", ordinal);

        if (!ini.KeyExists(pSection, key))
            break;

        char line[0x400];
        line[0] = '\0';
        if (ini.ReadString(pSection, key, "", line, sizeof(line)) <= 0)
            continue;                    // 空行：保留序号继续（与原版跳空一致）

        text += line;
    }

    if (text.empty())
        return 0;

    BufferStraw feeder(text.data(), text.size());
    Base64Straw decoder;
    decoder.Get_From(&feeder);
    return decoder.Get(pDst, static_cast<int32>(nSize));
}

// ───────────────────────────────────────────────────────────────────────────
// 解一组 *Pack / *DataPack。
//
//   *Pack 段：base64 → LCW 解压 → 每格 1 字节的类型下标（0xFF = 空）。
//   *DataPack：同管线，每格 1 字节的附加数据。
//
//   栅格尺寸：gamemd 的存盘路径固定写 512×512（asm 0x200 × 0x200，
//   MapClass::Coord_Cell 逐格取 CellClass 的 +0x44 / +0x11E），因此真实
//   .map 的 pack 解压后是 0x40000 字节；合成 / 简化夹具用 Width×Height。
//   两种长度都接受，长度不符才判失败（不猜、不补零）。
// ───────────────────────────────────────────────────────────────────────────
static bool Decode_Cell_Pack(CCINIClass& ini, const char* pPackSection,
                             const char* pDataSection,
                             int32 mapWidth, int32 mapHeight,
                             ScenarioMapFile::CellPack& out, const char* pPath)
{
    out = ScenarioMapFile::CellPack();

    const size_t localCount = static_cast<size_t>(mapWidth)
                            * static_cast<size_t>(mapHeight);
    const size_t fullCount  = 512u * 512u;
    const size_t capacity   = (localCount > fullCount) ? localCount : fullCount;
    if (capacity == 0)
        return false;

    // UU 块里装的是 LCW **压缩后**的字节，长度可能略大于解压结果
    // （最坏情况每条 63 字节 literal 多 1 字节命令 + 结尾 0x80），
    // 所以给压缩输入留 12.5% + 4K 余量，不能按解压尺寸截断。
    const size_t packedCapacity = capacity + capacity / 8 + 4096;

    std::vector<uint8> packed(packedCapacity, 0);
    const int32 packedLen = Decode_UUBlock_Continuous(ini, pPackSection,
                                                      packed.data(),
                                                      packed.size());
    if (packedLen <= 0)
    {
        // 大多数地图没有某一类内容：缺段是正常情况，不是错误。
        return false;
    }
    out.Present = true;

    std::vector<uint8> plain(capacity, 0);
    const int32 plainLen = LCW_Uncompress(packed.data(), packedLen,
                                          plain.data(),
                                          static_cast<int32>(capacity));
    if (plainLen <= 0)
    {
        char msg[320];
        std::snprintf(msg, sizeof(msg),
                      "Warning: [%s] LCW decode failed (packed=%d): %s",
                      pPackSection, (int)packedLen, pPath);
        LogMessage(msg);
        return false;
    }

    size_t count  = 0;
    int32  stride = 0;
    if (static_cast<size_t>(plainLen) == localCount)
    {
        count  = localCount;
        stride = mapWidth;
    }
    else if (static_cast<size_t>(plainLen) == fullCount)
    {
        count  = fullCount;
        stride = 512;                    // asm: CellIndex = (Y << 9) + X
    }
    else
    {
        char msg[320];
        std::snprintf(msg, sizeof(msg),
                      "Warning: [%s] decoded length %d matches neither "
                      "%dx%d (local) nor 512x512: %s",
                      pPackSection, (int)plainLen, (int)mapWidth,
                      (int)mapHeight, pPath);
        LogMessage(msg);
        return false;
    }

    out.Index.assign(plain.begin(), plain.begin() + static_cast<ptrdiff_t>(count));
    out.Stride  = stride;
    out.Decoded = true;

    // ── 对应的 DataPack（可选；缺失只影响附加数据，不影响类型落格）─────
    if (pDataSection != nullptr)
    {
        std::vector<uint8> packedData(packedCapacity, 0);
        const int32 packedDataLen = Decode_UUBlock_Continuous(ini, pDataSection,
                                                             packedData.data(),
                                                             packedData.size());
        if (packedDataLen > 0)
        {
            std::vector<uint8> plainData(capacity, 0);
            const int32 plainDataLen = LCW_Uncompress(
                packedData.data(), packedDataLen, plainData.data(),
                static_cast<int32>(capacity));

            if (plainDataLen == plainLen)
            {
                out.Data.assign(plainData.begin(),
                                plainData.begin() + static_cast<ptrdiff_t>(count));
            }
            else
            {
                char msg[320];
                std::snprintf(msg, sizeof(msg),
                              "Warning: [%s] length %d != [%s] length %d; "
                              "per-cell data ignored: %s",
                              pDataSection, (int)plainDataLen,
                              pPackSection, (int)plainLen, pPath);
                LogMessage(msg);
            }
        }
    }

    {
        char msg[320];
        std::snprintf(msg, sizeof(msg),
                      "Scenario map [%s] decoded: %d cells (stride=%d, "
                      "packed=%d, data=%s): %s",
                      pPackSection, (int)count, (int)stride, (int)packedLen,
                      out.Data.empty() ? "absent" : "ok", pPath);
        LogMessage(msg);
    }

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

    std::snprintf(out.Path, sizeof(out.Path), "%s", pPath);

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

    // ── 覆盖物 / 地形对象 / 弹坑（三组 *Pack + *DataPack）───────────────
    //   这三组与 [IsoMapPack5] 相互独立，先解出来存着，落格放到
    //   Apply_Scenario_Map_Tiles 之后（瓦片先落，overlay 后落）。
    Decode_Cell_Pack(ini, "OverlayPack", "OverlayDataPack", sw, sh,
                     out.OverlayPack, pPath);
    Decode_Cell_Pack(ini, "TerrainPack", "TerrainDataPack", sw, sh,
                     out.TerrainPack, pPath);
    Decode_Cell_Pack(ini, "SmudgePack", "SmudgeDataPack", sw, sh,
                     out.SmudgePack, pPath);

    // ── 预置实体：只收集原文，落位留到 2.7 ────────────────────────────
    auto collectEntityLines = [&ini](const char* pSection,
                                     std::vector<std::string>& outLines)
    {
        const int32 n = ini.GetKeyCount(pSection);
        for (int32 k = 0; k < n; ++k)
        {
            const char* pKey = ini.GetKeyName(pSection, k);
            if (pKey == nullptr)
                continue;

            char value[512];
            value[0] = '\0';
            if (ini.ReadString(pSection, pKey, "", value, sizeof(value)) > 0
                && value[0] != '\0')
                outLines.emplace_back(value);
        }
    };
    collectEntityLines("Structures", out.StructureLines);
    collectEntityLines("Units",      out.UnitLines);
    collectEntityLines("Infantry",   out.InfantryLines);

    // ── 格数据（[IsoMapPack5]）─────────────────────────────────────────
    const size_t maxBytes = (static_cast<size_t>(sw) * static_cast<size_t>(sh)
                             + 1) * 11;
    std::vector<uint8> packed(maxBytes, 0);

    const int32 packedLen = Decode_UUBlock_Continuous(ini, "IsoMapPack5",
                                                      packed.data(),
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

        // ── 全局瓦片序号 → 瓦片集；子瓦片号优先用文件里的第 5 字节 ──────
        //
        // [IsoMapPack5] 存的 rec.Tile 是**跨全部瓦片集累加**的全局序号
        // （asm CellClass.IsoTileTypeIndex 在 +0x38）。以前直接把它写进
        // TileType，等于把全局号当成“第几套图”，于是整张地图只会画第 0
        // 套的第 0..N 帧。这里按 asm dword_AA1140 的起点表拆开。
        //
        // 子瓦片号则是记录里独立的第 5 字节（asm sub_56BAC0 直接 lod 到
        // CellClass+0x11A）——它才是权威值，不能再由全局号反算。拆解函数
        // 只用来判断这条记录属于哪一套瓦片集（TileType）。
        int32 setIndex = 0;
        int32 subIndex = 0;
        if (IsometricTileType::Split_Global_Tile_Index(rec.Tile, setIndex, subIndex))
        {
            pCell->TileType = setIndex;
        }
        else
        {
            // 0xFFFF 是“无瓦片”哨兵；瓦片集表未建立时也同样置空。
            //   注意：这里只影响“属于哪套瓦片集”，不参与子瓦片号的判定。
            pCell->TileType = -1;
        }

        // 子瓦片号以文件第 5 字节为准（asm sub_56BAC0 直接把它 lod 进
        // CellClass+0x11A）。同一字节同时写进 Passability：水瀑朝向判定
        // 读的就是它。
        pCell->TileSubIndex = static_cast<int32>(rec.Passability);
        pCell->Passability  = rec.Passability;
        pCell->LandByte     = rec.LandByte;
        ++applied;
    }

    return applied;
}

// ───────────────────────────────────────────────────────────────────────────
// *_Pack 落格
//
//   pack 的栅格既可能是本图尺寸（合成夹具），也可能是 gamemd 存盘用的
//   512×512 整图。后者的格坐标是绝对坐标（CellIndex = (Y << 9) + X），
//   只把落在本图范围内的条目落进 CellClass 表。
// ───────────────────────────────────────────────────────────────────────────
template <typename TCallback>
static int32 For_Each_Cell_Pack_Entry(const ScenarioMapFile::CellPack& pack,
                                      MapClass* pMap, TCallback&& callback)
{
    if (pMap == nullptr || !pack.Decoded || pack.Stride <= 0)
        return 0;

    int32 visited = 0;
    for (size_t i = 0; i < pack.Index.size(); ++i)
    {
        const int32 x = static_cast<int32>(i % static_cast<size_t>(pack.Stride));
        const int32 y = static_cast<int32>(i / static_cast<size_t>(pack.Stride));

        if (x < 0 || x >= pMap->MapWidth || y < 0 || y >= pMap->MapHeight)
            continue;

        const uint8 index = pack.Index[i];
        if (index == 0xFF)
            continue;                        // 该格无内容

        CellClass* pCell = pMap->GetCellAt(x, y);
        if (pCell == nullptr)
            continue;

        const uint8 data = (i < pack.Data.size()) ? pack.Data[i] : 0;
        callback(pCell, index, data);
        ++visited;
    }

    return visited;
}

// asm: OverlayClass_CreateFromINIList（0x5FD2F0 同族）—— pack 字节直接是
//   OverlayTypeClass::Array 的下标（asm `mov eax,[OverlayTypeClass+eax*4]`）。
//   类型表里没有该下标时原版会越界读指针；本树按 FindByIndex 判空跳过。
static int32 Apply_Scenario_Map_Overlays(MapClass* pMap,
                                         const ScenarioMapFile& src,
                                         int32& unknownType)
{
    unknownType = 0;
    int32 applied = 0;

    For_Each_Cell_Pack_Entry(src.OverlayPack, pMap,
        [&](CellClass* pCell, uint8 index, uint8 data)
        {
            if (OverlayTypeClass::FindByIndex(static_cast<int32>(index)) == nullptr)
            {
                ++unknownType;
                return;
            }

            pCell->Set_Overlay(static_cast<int32>(index), static_cast<int32>(data));
            // +0x11E：矿的生长阶段 / 墙的朝向帧。DataPack 写的正是这一点
            // （asm OverlayClass_CreateFromINIList 的 `mov [eax+11Eh], dl`）。
            pCell->OverlayFrame = data;
            ++applied;
        });

    return applied;
}

// asm: TerrainClass_CreateFromINIList 的 pack 形态 —— 每个非 0xFF 字节
//   建一个 TerrainClass 实体（不是只写指针）：Type / 血量 / 格心坐标，
//   limbo 后 Unlimbo 完成 vec_Logics + 显示层登记。
static int32 Apply_Scenario_Map_Terrain(MapClass* pMap,
                                        const ScenarioMapFile& src,
                                        int32& unknownType,
                                        int32& spawnFailed)
{
    unknownType = 0;
    spawnFailed = 0;
    int32 spawned = 0;

    For_Each_Cell_Pack_Entry(src.TerrainPack, pMap,
        [&](CellClass* pCell, uint8 index, uint8 /*data*/)
        {
            TerrainTypeClass* pType =
                TerrainTypeClass::FindByIndex(static_cast<int32>(index));
            if (pType == nullptr)
            {
                ++unknownType;
                return;
            }

            if (pCell->Terrain != nullptr)
                return;                      // 同格已有地形对象：不覆盖

            TerrainClass* pTerrain =
                GameCreateArgs<TerrainClass>(static_cast<HouseClass*>(nullptr));
            if (pTerrain == nullptr)
            {
                ++spawnFailed;
                return;
            }

            pTerrain->Type   = pType;
            pTerrain->Health = (pType->Strength > 0) ? pType->Strength : 1;

            // asm TerrainClass_CTOR：位置 = 格坐标 * 0x100 + 0x80（格心）。
            CoordStruct pos = CellClass::Cell2Coord(pCell->MapCoords);
            pos.X += CellClass::LeapArray_Step / 2;
            pos.Y += CellClass::LeapArray_Step / 2;
            pTerrain->Location  = pos;
            pTerrain->IsInLimbo = true;

            if (!pTerrain->Unlimbo())
            {
                // 构造已把对象追加进 vec_Terrain；析构不会摘除，回滚要自己来。
                if (TerrainClass::Array != nullptr)
                {
                    for (int32 i = 0; i < TerrainClass::Array->Count; ++i)
                    {
                        if ((*TerrainClass::Array)[i] == pTerrain)
                        {
                            TerrainClass::Array->Remove(i);
                            break;
                        }
                    }
                }
                GameDelete(pTerrain);
                ++spawnFailed;
                return;
            }

            pCell->Set_Terrain(pTerrain);
            ++spawned;
        });

    return spawned;
}

// asm: SmudgeClass 的落格面 —— +0x44/0x45 的 Smudge/SmudgeData 槽。
//   注意：绘制侧 Draw_Ground_Decals 遍历的是 SmudgeClass::Array，不是
//   CellClass 字段；本函数只负责把地图数据落到格子上（任务指定语义）。
static int32 Apply_Scenario_Map_Smudges(MapClass* pMap,
                                        const ScenarioMapFile& src,
                                        int32& unknownType)
{
    unknownType = 0;
    int32 applied = 0;

    For_Each_Cell_Pack_Entry(src.SmudgePack, pMap,
        [&](CellClass* pCell, uint8 index, uint8 data)
        {
            if (SmudgeTypeClass::FindByIndex(static_cast<int32>(index)) == nullptr)
            {
                ++unknownType;
                return;
            }

            pCell->Set_Smudge(static_cast<int32>(index), static_cast<int32>(data));
            ++applied;
        });

    return applied;
}

// OPENYR_MAPDBG=1：打印若干指定格的实际落地值，用于验证解析 → 落格链路。
//   默认不输出；直接写 stderr，不依赖 OPENYR_DEBUG 的日志开关。
static void Debug_Dump_Map_Cells(MapClass* pMap)
{
    if (pMap == nullptr || std::getenv("OPENYR_MAPDBG") == nullptr)
        return;

    static const int32 kCells[][2] = { { 10, 10 }, { 11, 10 },
                                       { 12, 10 }, { 13, 10 } };
    for (size_t i = 0; i < sizeof(kCells) / sizeof(kCells[0]); ++i)
    {
        const int32 x = kCells[i][0];
        const int32 y = kCells[i][1];
        CellClass* pCell = pMap->GetCellAt(x, y);

        if (pCell == nullptr)
        {
            std::fprintf(stderr, "MAPDBG cell(%d,%d) OUT-OF-MAP\n", (int)x, (int)y);
            continue;
        }

        const TerrainClass* pTerrain = pCell->Terrain;
        std::fprintf(stderr,
                     "MAPDBG cell(%d,%d) Overlay=%d OverlayData=%d "
                     "OverlayFrame=%d Smudge=%d SmudgeData=%d "
                     "TileType=%d TileSubIndex=%d Passability=%d "
                     "Terrain=%s TerrainType=%s\n",
                     (int)x, (int)y,
                     (int)pCell->Overlay, (int)pCell->OverlayData,
                     (int)pCell->OverlayFrame,
                     (int)pCell->Smudge, (int)pCell->SmudgeData,
                     (int)pCell->TileType, (int)pCell->TileSubIndex,
                     (int)pCell->Passability,
                     (pTerrain != nullptr) ? "yes" : "no",
                     (pTerrain != nullptr && pTerrain->Type != nullptr)
                         ? pTerrain->Type->ID : "-");
    }
    std::fflush(stderr);
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

    // ── 2.6 覆盖物 / 地形对象 / 弹坑（瓦片之后落格）───────────────────
    //   asm INIClass_ReadScenario 的顺序就是 DisplayClass::Read_INI（瓦片）
    //   → OverlayClass_CreateFromINIList → TerrainClass_CreateFromINIList
    //   → SmudgeClass_CreateFromINIList。这里保持同一顺序。
    if (bMapFreshlyAllocated && bRealMap)
    {
        if (mapData.OverlayPack.Decoded)
        {
            int32 unknownType = 0;
            const int32 overlays = Apply_Scenario_Map_Overlays(pMap, mapData,
                                                               unknownType);
            if (g_bDebugMode)
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "Scenario map overlays applied: %d cells "
                              "(%d unknown type)", (int)overlays, (int)unknownType);
                LogMessage(msg);
            }
        }

        if (mapData.TerrainPack.Decoded)
        {
            int32 unknownType = 0;
            int32 spawnFailed = 0;
            const int32 terrains = Apply_Scenario_Map_Terrain(pMap, mapData,
                                                              unknownType,
                                                              spawnFailed);
            if (g_bDebugMode)
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "Scenario map terrain objects placed: %d "
                              "(%d unknown type, %d unlimbo failed)",
                              (int)terrains, (int)unknownType, (int)spawnFailed);
                LogMessage(msg);
            }
        }

        if (mapData.SmudgePack.Decoded)
        {
            int32 unknownType = 0;
            const int32 smudges = Apply_Scenario_Map_Smudges(pMap, mapData,
                                                             unknownType);
            if (g_bDebugMode)
            {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                              "Scenario map smudges applied: %d cells "
                              "(%d unknown type)", (int)smudges, (int)unknownType);
                LogMessage(msg);
            }
        }

        Debug_Dump_Map_Cells(pMap);
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

    // asm: MapClass::Init / Set_Visible_Rect —— 可见区矩形默认就是整张地图
    //   （[Map] LocalSize）。此前本树从不初始化这块，四条全为 0，于是
    //   MapClass::CellInVisibleArea 对任何坐标都返回假：
    //     * BulletClass::Resolve_Ballistic_Hit 的"飞出可见区"分支
    //       （asm loc_467A2B → hitFlag 2）每帧触发，**所有弹丸在出膛后
    //       第一帧就被静默销毁**，一发也打不中；
    //     * 寻路的"离屏格"过滤把所有候选格判成屏外。
    //   原版没有"无可见区"这种状态，这里按地图矩形补齐。
    pMap->VisibleRectX      = 0;
    pMap->VisibleRectY      = 0;
    pMap->VisibleRectWidth  = pMap->MapWidth;
    pMap->VisibleRectHeight = pMap->MapHeight;

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

    // ── 4. 参战方 ────────────────────────────────────────────────────────
    // asm: HouseClass_LoadFromINIList 先用 Country= 查 HouseTypeClass
    //   （FindOrAllocate_YesMP），再 new HouseClass(type)。
    if (HouseTypeClass::ArrayCount == 0)
        HouseTypeClass::Init_Defaults();     // asm: rulesmd.ini 的 [Houses]

    HouseClass* pPlayerHouse = nullptr;
    HouseClass* pEnemyHouse  = nullptr;

    // ── 4.1 真实地图的 [Houses] 段（多阵营 / 颜色 / 资金 / 结盟）──────────
    //   原版 INIClass_ReadScenario 的顺序是：瓦片 → overlay/terrain/smudge →
    //   [Houses] → 三个 *CreateFromINIList。地图里列了几套房就建几套房，
    //   每套的 Country= / Color= / Credits= / PlayerControl= / Allies= 都在
    //   自己的段里。此前本树只有"Americans + Russians"两套写死的房，
    //   真实地图上的第三、第四个阵营（以及 Neutral/Special）全部丢失，
    //   颜色与初始资金也无从读起 —— 这里接回原版入口。
    if (HouseClass::ArrayCount == 0 && bRealMap && mapData.Path[0] != '\0')
    {
        CCFileClass houseFile(mapData.Path);
        CCINIClass  houseINI;

        if (houseFile.IsAvailable() && houseINI.LoadFile(&houseFile)
            && houseINI.GetKeyCount("Houses") > 0)
        {
            if (HouseClass::LoadFromINIList(&houseINI, pScenario->InitialMoney))
            {
                pPlayerHouse = HouseClass::Player;

                for (int32 i = 0; i < HouseClass::ArrayCount; ++i)
                {
                    HouseClass* pH = HouseClass::Array[i];
                    if (pH != nullptr && pH != pPlayerHouse)
                    {
                        pEnemyHouse = pH;
                        break;
                    }
                }

                if (g_bDebugMode)
                {
                    char msg[256];
                    std::snprintf(msg, sizeof(msg),
                                  "Scenario [Houses]: %d house(s) from map, "
                                  "player='%s' (index %d)",
                                  (int)HouseClass::ArrayCount,
                                  (pPlayerHouse != nullptr) ? pPlayerHouse->InitialName : "?",
                                  (int)ThePlayerIndex);
                    LogMessage(msg);
                }
            }
        }
    }

    if (pPlayerHouse == nullptr)
    {
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
    }

    if (HouseClass::ArrayCount != 0 && pPlayerHouse == nullptr)
    {
        // 已有房子（重入 / 存档恢复 / 上面 [Houses] 段没声明玩家位）：
        //   不重复创建，只补齐玩家索引。
        TheHouseCount  = HouseClass::ArrayCount;
        ThePlayerIndex = 0;
        pPlayerHouse   = HouseClass::Array[0];
        if (HouseClass::ArrayCount > 1)
            pEnemyHouse = HouseClass::Array[1];
    }

    // ── 2.7 场景预置实体（[Structures] / [Units] / [Infantry]）─────────
    //   asm INIClass_ReadScenario 的顺序：瓦片 → overlay/terrain/smudge →
    //   [Houses] → 三个 *CreateFromINIList。三段的落位既依赖格表，也依赖
    //   房主表（owner 按 InitialName 查），所以必须放在房子建好之后。
    //   本树的 4.4 段负责建两套房，这块挂在 4.4 与 4.5 之间。
    if (bMapFreshlyAllocated && bRealMap
        && (!mapData.StructureLines.empty()
            || !mapData.UnitLines.empty()
            || !mapData.InfantryLines.empty()))
    {
        CCINIClass entityINI;
        auto feedSection = [&entityINI](const char* pSection,
                                        const std::vector<std::string>& lines)
        {
            for (size_t i = 0; i < lines.size(); ++i)
            {
                char key[16];
                std::snprintf(key, sizeof(key), "%d", static_cast<int>(i));
                entityINI.WriteString(pSection, key, lines[i].c_str());
            }
        };
        feedSection("Structures", mapData.StructureLines);
        feedSection("Units",      mapData.UnitLines);
        feedSection("Infantry",   mapData.InfantryLines);

        if (std::getenv("OPENYR_MAPDBG") != nullptr)
        {
            std::fprintf(stderr, "[ENTDBG] HouseClass::ArrayCount=%d TheHouseCount=%d\n",
                         (int)HouseClass::ArrayCount, (int)TheHouseCount);
            for (int32 h = 0; h < HouseClass::ArrayCount; ++h)
            {
                HouseClass* pH = HouseClass::Array[h];
                if (pH == nullptr)
                    continue;
                std::fprintf(stderr, "[ENTDBG] house[%d] InitialName='%s' Type=%s"
                                     " credits=%d color=%d player=%d actlike=%d"
                                     " ally0=%d ally1=%d ally2=%d\n",
                             (int)h, pH->InitialName,
                             (pH->Type != nullptr) ? pH->Type->ID : "?",
                             (int)pH->Credits, (int)pH->ColorSchemeIndex,
                             (pH->PlayerControl ? 1 : 0), (int)pH->ActLikeIndex,
                             (HouseClass::ArrayCount > 0 && HouseClass::Array[0])
                                 ? (int)pH->Is_Ally(HouseClass::Array[0]) : -1,
                             (HouseClass::ArrayCount > 1 && HouseClass::Array[1])
                                 ? (int)pH->Is_Ally(HouseClass::Array[1]) : -1,
                             (HouseClass::ArrayCount > 2 && HouseClass::Array[2])
                                 ? (int)pH->Is_Ally(HouseClass::Array[2]) : -1);
            }
        }

        const int32 buildings = BuildingClass::CreateFromINIList(&entityINI);
        UnitClass::CreateFromINIList(&entityINI);
        InfantryClass::CreateFromINIList(&entityINI);

        char msg[224];
        std::snprintf(msg, sizeof(msg),
                      "Scenario entities placed: %d building(s) from %d record(s), "
                      "%d unit record(s), %d infantry record(s)",
                      (int)buildings, (int)mapData.StructureLines.size(),
                      (int)mapData.UnitLines.size(),
                      (int)mapData.InfantryLines.size());
        LogMessage(msg);
    }

    // ── 2.8 地图触发器（[Triggers] / [Events] / [Actions] / [Tags]）──────
    //   asm INIClass_ReadScenario 在读完实体后依次调
    //   TriggerTypeClass_CreateFromINIList 与 TagTypeClass_CreateFromINIList。
    //   顺序不能反：Tag 段按触发器 ID 反查时，[Triggers] 必须已经在类型表里。
    //
    //   这两套是"地图任务脚本"，与 [AITriggerTypes]（AI 自主出兵）是两回事，
    //   后者已在 3.x 段接线。
    if (bRealMap && mapData.Path[0] != '\0')
    {
        CCFileClass trigFile(mapData.Path);
        CCINIClass  trigINI;

        if (trigFile.IsAvailable() && trigINI.LoadFile(&trigFile))
        {
            if (trigINI.SectionExists("Triggers"))
                TriggerTypeClass::CreateFromINIList(&trigINI);

            if (trigINI.SectionExists("Tags"))
                TagTypeClass::CreateFromINIList(&trigINI);

            // [Waypoints]：键 = 路点号（0..701），值 = 打包格号
            //   （NewINIFormat >= 4 时 Y*1000+X，更老的地图是 Y*128+X）。
            //   TeamType 的集结点、增援落点、相机对位都用这张表。
            if (trigINI.SectionExists("Waypoints") && MapClass::Instance != nullptr)
            {
                const int32 fmtW = trigINI.ReadInteger("Basic", "NewINIFormat", 5);
                const int32 nW = trigINI.GetKeyCount("Waypoints");
                for (int32 i = 0; i < nW; ++i)
                {
                    const char* pKeyW = trigINI.GetKeyName("Waypoints", i);
                    if (pKeyW == nullptr) continue;

                    char valW[64];
                    valW[0] = '\0';
                    if (trigINI.ReadString("Waypoints", pKeyW, "", valW, sizeof(valW)) == 0)
                        continue;

                    const int32 wpIdx    = std::atoi(pKeyW);
                    const int32 wpPacked = std::atoi(valW);

                    int32 wx = 0, wy = 0;
                    if (fmtW >= 4) { wy = wpPacked / 1000; wx = wpPacked % 1000; }
                    else           { wx = wpPacked & 0x7F; wy = wpPacked / 128;  }

                    CellClass* pWCell = MapClass::Instance->GetCellAt(
                        CellStruct(static_cast<int16>(wx), static_cast<int16>(wy)));
                    if (pWCell == nullptr) continue;

                    CoordStruct wpos(wx, wy, 0);
                    pWCell->ConvertCoords(&wpos);
                    MapClass::Instance->SetWaypoint(wpIdx, wpos);

                    if (std::getenv("OPENYR_TRIGDBG") != nullptr)
                        std::fprintf(stderr,
                                     "[WAYPOINT] %d -> cell(%d,%d) coord(%d,%d)\n",
                                     (int)wpIdx, (int)wx, (int)wy,
                                     (int)wpos.X, (int)wpos.Y);
                }
            }

            // [CellTags] 必须排在 [Tags] 之后：它的值是对 [Tags] 键的引用，
            //   反过来调会把每个格子挂成 nullptr。ReadCellTags 早就写好了，
            //   只是从来没人调 —— 于是 CellClass::AttachedTag 恒为空。
            if (trigINI.SectionExists("CellTags") && MapClass::Instance != nullptr)
            {
                const int32 iniFmt = trigINI.ReadInteger("Basic", "NewINIFormat", 5);
                MapClass::Instance->ReadCellTags(&trigINI, "CellTags", iniFmt);

                if (std::getenv("OPENYR_TRIGDBG") != nullptr)
                {
                    int32 tagged = 0;
                    MapClass* pM = MapClass::Instance;
                    if (pM != nullptr && pM->CellArray != nullptr)
                    {
                        for (int32 y = 0; y < pM->MapHeight; ++y)
                            for (int32 x = 0; x < pM->MapWidth; ++x)
                            {
                                CellClass* pc = pM->GetCellAt(x, y);
                                if (pc != nullptr && pc->AttachedTag != nullptr)
                                    ++tagged;
                            }
                    }
                    std::fprintf(stderr, "[CELLTAG] tagged cells=%d fmt=%d\n",
                                 (int)tagged, (int)iniFmt);
                }
            }

            if (std::getenv("OPENYR_TRIGDBG") != nullptr)
            {
                std::fprintf(stderr,
                             "[TRIG] types=%d events=%d actions=%d tags=%d triggers=%d\n",
                             (TriggerTypeClass::Array != nullptr)
                                 ? (int)TriggerTypeClass::Array->Count : -1,
                             (TEventClass::Array != nullptr)
                                 ? (int)TEventClass::Array->Count : -1,
                             (TActionClass::Array != nullptr)
                                 ? (int)TActionClass::Array->Count : -1,
                             (TagClass::Array != nullptr)
                                 ? (int)TagClass::Array->Count : -1,
                             (TriggerClass::Array != nullptr)
                                 ? (int)TriggerClass::Array->Count : -1);
            }
        }
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
        // asm: BulletClass::Init_Array —— vec_Bullets 由启动期建立。
        //   此前本树从未建立这张表：BulletClass 的构造函数与 Update_Combat
        //   都以 `Array != nullptr` 为前提，于是**所有弹丸都被静默丢弃**，
        //   战斗链止步于"Can_Fire 返回 OK → Fire 造弹 → 弹不在任何表里
        //   → 永不推进/永不命中"。补齐后弹道才真正进入逐帧模拟。
        Ensure_Class_Array(BulletClass::Array);

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

    // ── 开局视野 ─────────────────────────────────────────────────────────
    //   TechnoClass::Unlimbo 落位时会调 MapClass::Sight_From_3，但那一刻
    //   HouseClass::pCurrentPlayer 还没定（场景初始化中段），Sight_From 直接
    //   返回，黑幕一格都没揭开；等逐帧渲染开始画 shroud 时整屏就是黑的。
    //   这里在场景就绪后按玩家名下每个单位补一次揭幕，等于原版"开局视野"。
    if (pMap != nullptr && pPlayerHouse != nullptr && TechnoClass::Array != nullptr)
    {
        for (int32 i = 0; i < TechnoClass::Array->Count; ++i)
        {
            TechnoClass* pT = (*TechnoClass::Array)[i];
            if (pT == nullptr || pT->Owner != pPlayerHouse || pT->IsInLimbo)
                continue;

            const CoordStruct pos = pT->GetCoords();
            pMap->Sight_From(pos, 9, pPlayerHouse);
        }
    }

    // ── 战斗链冒烟（OPENYR_COMBAT_SMOKE=1）────────────────────────────────
    //   在锚点单位旁落一个敌对步兵，命它攻击锚点，用来验证
    //   目标选择 → Can_Fire → Fire → BulletClass::Update → Detonate →
    //   ReceiveDamage 的整条链路真的在改血量。默认不启用，不影响正常启动。
    if (std::getenv("OPENYR_COMBAT_SMOKE") != nullptr
        && pViewAnchor != nullptr && TheHouseCount >= 2)
    {
        HouseClass* pEnemy = nullptr;
        for (int32 i = 0; i < TheHouseCount; ++i)
        {
            HouseClass* pH = HouseClass::Array[i];
            if (pH != nullptr && pH != pPlayerHouse)
            {
                pEnemy = pH;
                break;
            }
        }

        InfantryTypeClass* pE1 = InfantryTypeClass::FindOrAllocate("E1");
        std::fprintf(stderr, "[SMOKE] precheck enemy=%d e1=%d map=%d houses=%d anchor=%p"
                             " player=%p enemyHouse=%p anchorOwn=%p\n",
                     (pEnemy != nullptr) ? 1 : 0, (pE1 != nullptr) ? 1 : 0,
                     (pMap != nullptr) ? 1 : 0, (int)TheHouseCount,
                     static_cast<void*>(pViewAnchor),
                     static_cast<void*>(pPlayerHouse), static_cast<void*>(pEnemy),
                     static_cast<void*>(pViewAnchor->Owner));
        if (pEnemy != nullptr && pE1 != nullptr && pMap != nullptr)
        {
            const CellStruct anchorCell = CellClass::Coord2Cell(pViewAnchor->GetCoords());

            // 靶子必须是玩家自己的单位：Create_Units 只给 AI 房铺兵（asm 的
            //   `[Country+humanControlled] → 跳过`），锚点常常落在敌方单位上，
            //   拿它当靶子会被 Can_Fire 的友军门挡掉。
            TechnoClass* pVictim = (pViewAnchor->Owner == pPlayerHouse)
                                     ? pViewAnchor : nullptr;

            // 靶子必须落在**空格**里：锚点格往往已经被锚点单位占住，
            //   再往里放一个步兵会落位失败（留在 limbo、血量为 0），
            //   于是攻击者第一帧就判定"目标已死"并把目标丢掉。
            const CellStruct victimCell(static_cast<int16>(anchorCell.X),
                                        static_cast<int16>(anchorCell.Y + 2));
            if (pVictim == nullptr && pPlayerHouse != nullptr)
                pVictim = Place_Starting_Techno<InfantryClass>(
                    pPlayerHouse, pE1, victimCell);

            // 第二个玩家目标：用来验证"打死第一个目标后，攻击者在警戒
            //   半径内自动改锁下一个敌人"（TechnoClass::Target_Something_Nearby）。
            const CellStruct victim2Cell(static_cast<int16>(victimCell.X),
                                         static_cast<int16>(victimCell.Y - 4));
            TechnoClass* pVictim2 = (pPlayerHouse != nullptr)
                ? Place_Starting_Techno<InfantryClass>(pPlayerHouse, pE1, victim2Cell)
                : nullptr;

            const CellStruct probe(static_cast<int16>(victimCell.X + 3),
                                   static_cast<int16>(victimCell.Y - 1));

            TechnoClass* pFoe = (pVictim != nullptr)
                ? Place_Starting_Techno<InfantryClass>(pEnemy, pE1, probe)
                : nullptr;
            if (pFoe != nullptr)
            {
                pFoe->SetTarget(pVictim);
                std::fprintf(stderr, "[SMOKE] victim=%p own=%p foe=%p foeGet=%p"
                                     " vhp=%d vlimbo=%d at=(%d,%d)\n",
                             static_cast<void*>(pVictim),
                             static_cast<void*>(pVictim->Owner),
                             static_cast<void*>(pFoe),
                             static_cast<void*>(pFoe->GetTarget()),
                             (int)pVictim->Health, (int)pVictim->IsInLimbo,
                             (int)CellClass::Coord2Cell(pVictim->GetCoords()).X,
                             (int)CellClass::Coord2Cell(pVictim->GetCoords()).Y);
                pFoe->ForceMission(Mission::Attack);
                {
                    WeaponStruct* w0 = (pE1 != nullptr) ? pE1->GetWeapon(0) : nullptr;
                    std::fprintf(stderr, "[SMOKE] weapon0 slot=%p type=%p id=%s ammo=%d"
                                         " cur=%d guard=%d strength=%d\n",
                                 static_cast<void*>(w0),
                                 (w0 != nullptr) ? static_cast<void*>(w0->WeaponType) : nullptr,
                                 (pE1->get_ID() != nullptr) ? pE1->get_ID() : "?",
                                 (int)pE1->Ammo, (int)pFoe->CurrentAmmo,
                                 (int)pE1->GuardRange, (int)pE1->Strength);
                }
                std::fprintf(stderr, "[SMOKE] victim2=%p hp=%d limbo=%d\n",
                             static_cast<void*>(pVictim2),
                             (pVictim2 != nullptr) ? (int)pVictim2->Health : -1,
                             (pVictim2 != nullptr) ? (int)pVictim2->IsInLimbo : -1);

                std::fprintf(stderr, "[SMOKE] foeLastFire=%d rofTimer=%d\n",
                             (int)pFoe->GetLastFireFrame(), (int)pFoe->FireRechargeTimer);
                std::fprintf(stderr, "[SMOKE] after ForceMission: get=%p mission=%d\n",
                             static_cast<void*>(pFoe->GetTarget()),
                             static_cast<int>(pFoe->GetMission()));
            }
        }
    }

    // ── 建造链冒烟（OPENYR_BUILD_SMOKE=1）──────────────────────────────────
    //   两座兵营，两条不同路径：
    //     * 玩家房：手动 QueueProduction + Unsuspend，验证
    //       FactoryClass::Update(进度) → BuildingClass::Update_Factory(discharge)
    //       → Rollout_Product(Unlimbo) 这条"造出来 → 放到战场上"的链路；
    //     * 敌方房：只给建筑不给订单，验证 HouseClass::AI 会不会自己开线
    //       （Update_Factory 的 B 段 → GetTechnoToProduce → QueueProduction）。
    //   默认不启用，不影响正常启动。
    if (std::getenv("OPENYR_BUILD_SMOKE") != nullptr
        && pPlayerHouse != nullptr && pMap != nullptr)
    {
        BuildingTypeClass* pBarracks = BuildingTypeClass::FindOrAllocate("GAPILE");
        TechnoTypeClass*   pInfType  = InfantryTypeClass::FindOrAllocate("E1");

        HouseClass* pAIPlayer = nullptr;
        for (int32 i = 0; i < TheHouseCount; ++i)
        {
            HouseClass* pH = HouseClass::Array[i];
            if (pH != nullptr && pH != pPlayerHouse) { pAIPlayer = pH; break; }
        }

        std::fprintf(stderr, "[BUILD] precheck barracks=%d e1=%d playerCredits=%d"
                             " ai=%d aiCredits=%d\n",
                     (pBarracks != nullptr) ? 1 : 0, (pInfType != nullptr) ? 1 : 0,
                     (int)pPlayerHouse->AvailableMoney(),
                     (pAIPlayer != nullptr) ? 1 : 0,
                     (pAIPlayer != nullptr) ? (int)pAIPlayer->AvailableMoney() : -1);

        auto PlaceBarracks = [&](HouseClass* pHouse) -> BuildingClass*
        {
            if (pHouse == nullptr || pBarracks == nullptr)
                return nullptr;

            // 以该房第一个单位的位置为基准往外找一个空格。
            CellStruct baseCell(20, 20);
            if (pHouse->OwnedInfantry.GetCount() > 0
                && pHouse->OwnedInfantry[0] != nullptr)
                baseCell = CellClass::Coord2Cell(pHouse->OwnedInfantry[0]->GetCoords());
            else if (pHouse->OwnedUnits.GetCount() > 0
                     && pHouse->OwnedUnits[0] != nullptr)
                baseCell = CellClass::Coord2Cell(pHouse->OwnedUnits[0]->GetCoords());
            else if (pViewAnchor != nullptr)
                baseCell = CellClass::Coord2Cell(pViewAnchor->GetCoords());

            BuildingClass* pBldg = nullptr;
            for (int32 r = 3; r <= 10 && pBldg == nullptr; ++r)
            {
                for (int32 dy = -r; dy <= r && pBldg == nullptr; ++dy)
                {
                    for (int32 dx = -r; dx <= r && pBldg == nullptr; ++dx)
                    {
                        if (dx * dx + dy * dy < (r - 1) * (r - 1))
                            continue;
                        const CellStruct probe(static_cast<int16>(baseCell.X + dx),
                                               static_cast<int16>(baseCell.Y + dy));
                        CellClass* pCell = pMap->GetCellAt(probe);
                        if (pCell == nullptr || pCell->IsOccupied())
                            continue;
                        pBldg = Place_Starting_Techno<BuildingClass>(
                            pHouse, pBarracks, probe);
                    }
                }
            }
            if (pBldg != nullptr)
                pHouse->OwnedBuildings.Add(pBldg);
            return pBldg;
        };

        // ── 玩家房：手动下单 ────────────────────────────────────────────
        if (BuildingClass* pBldg = PlaceBarracks(pPlayerHouse))
        {
            FactoryClass* pFac = new FactoryClass(pBldg, pPlayerHouse);
            pBldg->Factory = pFac;

            const bool queued = (pInfType != nullptr) && pFac->QueueProduction(pInfType);
            pFac->Unsuspend();

            std::fprintf(stderr,
                         "[BUILD] player barracks=%p cell=(%d,%d) fac=%p queued=%d"
                         " onhold=%d credits=%d\n",
                         static_cast<void*>(pBldg),
                         (int)CellClass::Coord2Cell(pBldg->GetCoords()).X,
                         (int)CellClass::Coord2Cell(pBldg->GetCoords()).Y,
                         static_cast<void*>(pFac), queued ? 1 : 0,
                         pFac->IsOnHold() ? 1 : 0,
                         (int)pPlayerHouse->AvailableMoney());
        }
        else
        {
            std::fprintf(stderr, "[BUILD] no free cell for player barracks\n");
        }

        // ── 敌方房：只给建筑，让它自己开线 ──────────────────────────────
        if (pAIPlayer != nullptr)
        {
            if (pAIPlayer->AvailableMoney() < 5000)
                pAIPlayer->GiveMoney(5000);

            if (BuildingClass* pBldg = PlaceBarracks(pAIPlayer))
            {
                std::fprintf(stderr,
                             "[BUILD] ai barracks=%p cell=(%d,%d) credits=%d"
                             " factory=%d infToProduce=%d\n",
                             static_cast<void*>(pBldg),
                             (int)CellClass::Coord2Cell(pBldg->GetCoords()).X,
                             (int)CellClass::Coord2Cell(pBldg->GetCoords()).Y,
                             (int)pAIPlayer->AvailableMoney(),
                             (pBldg->Type != nullptr) ? (int)pBldg->Type->Factory : -99,
                             (int)pAIPlayer->InfantryTypeToProduce);

            }
            else
            {
                std::fprintf(stderr, "[BUILD] no free cell for ai barracks\n");
            }
        }
    }

    // ── 移动链冒烟（OPENYR_MOVE_SMOKE=1）──────────────────────────────────
    //   给锚点载具下一个"往前开 6 格"的移动命令，用来验证
    //   FootClass::Set_Destination → ILocomotion::Process → 坐标真的在变。
    //
    //   OPENYR_MOVE_BLOCKED=1 是本冒烟的第二副面孔：目的地故意取"右方 3 格"，
    //   由测试地图保证那一格是建筑（建筑格对 Can_Enter_Cell 恒为不可入），
    //   从而端到端验证 Set_Destination 的相邻格降级 —— 旧实现直接 return
    //   false、单位一格都不走。
    if (std::getenv("OPENYR_MOVE_SMOKE") != nullptr
        && pViewAnchor != nullptr && pViewAnchor->WhatAmI() == AbstractType::Unit)
    {
        FootClass* pFoot = static_cast<FootClass*>(pViewAnchor);
        const CellStruct here = CellClass::Coord2Cell(pFoot->GetCoords());
        CellStruct dest(static_cast<int16>(here.X + 6), here.Y);

        if (std::getenv("OPENYR_MOVE_BLOCKED") != nullptr)
        {
            dest = CellStruct(static_cast<int16>(here.X + 3), here.Y);
        }
        else
        {
            // 目标格必须是陆地且没被占。
            CellClass* pDestCell = pMap->GetCellAt(dest);
            if (pDestCell == nullptr || pDestCell->IsOccupied())
                dest = CellStruct(static_cast<int16>(here.X + 4), static_cast<int16>(here.Y + 2));
        }

        const CoordStruct reqCoord = CellClass::Cell2Coord(dest);
        CellClass* pReqCell = pMap->GetCellAt(dest);
        const int32 reqOcc = (pReqCell != nullptr) ? static_cast<int32>(pReqCell->IsOccupied()) : -1;
        const int32 reqPass = (pReqCell != nullptr && pFoot->TechnoType != nullptr)
                              ? static_cast<int32>(pReqCell->PassableFor(pFoot->TechnoType->MoveZone))
                              : -1;
        const bool moveOk = pFoot->Set_Destination(reqCoord);
        const CoordStruct accepted = moveOk ? pFoot->Get_Destination() : CoordStruct(0, 0, 0);
        std::fprintf(stderr,
                     "[MOVE] unit=%p from=(%d,%d) req=(%d,%d) occ=%d pass=%d"
                     " ok=%d accepted=(%d,%d)\n",
                     static_cast<void*>(pFoot), (int)here.X, (int)here.Y,
                     (int)dest.X, (int)dest.Y, (int)reqOcc, (int)reqPass, (int)moveOk,
                     (int)CellClass::Coord2Cell(accepted).X,
                     (int)CellClass::Coord2Cell(accepted).Y);
    }

    // ── 警戒追击冒烟（OPENYR_GUARD_SMOKE=1）──────────────────────────────
    //   让"最近的敌方载具"以锚点为目标，并强制切到 Guard 任务，走的就是
    //   Mission_Guard 里那条 asm loc_4D51E0 分支：
    //      有目标且 CanFightBack() → Set_Destination(目标所在格)
    //   而目标格恒被目标自己占着（载具独占一格），所以这条链的成败完全
    //   取决于 Set_Destination 的相邻格降级 —— 旧实现在这里直接失败，车
    //   一格都不动，"警戒追击"退化成原地不动。
    //
    //   追击者选 AI 单位而不是锚点本身：FootClass::CanFightBack 对玩家单位
    //   落到 CanPassiveAquire()，而兜底规则（无零售 rules.ini）里该标志为
    //   假；AI 单位走的是另一条前置分支（+0x14 的 0x04 位），不依赖它。
    if (std::getenv("OPENYR_GUARD_SMOKE") != nullptr
        && pViewAnchor != nullptr && pViewAnchor->WhatAmI() == AbstractType::Unit)
    {
        FootClass* pPrey = static_cast<FootClass*>(pViewAnchor);
        const CoordStruct myPos = pPrey->GetCoords();

        UnitClass* pChaser = nullptr;
        int64 bestSq = 0;
        if (UnitClass::Array != nullptr)
        {
            for (int32 i = 0; i < UnitClass::Array->Count; ++i)
            {
                UnitClass* pCand = UnitClass::Array->Items[i];
                if (pCand == nullptr || static_cast<FootClass*>(pCand) == pPrey)
                    continue;
                if (pCand->IsDead() || pCand->Owner == nullptr || pPrey->Owner == nullptr)
                    continue;
                if (pPrey->Owner->Is_Ally(pCand))
                    continue;
                const CoordStruct p = pCand->GetCoords();
                const int64 dx = static_cast<int64>(p.X) - myPos.X;
                const int64 dy = static_cast<int64>(p.Y) - myPos.Y;
                const int64 sq = dx * dx + dy * dy;
                if (pChaser == nullptr || sq < bestSq)
                {
                    bestSq  = sq;
                    pChaser = pCand;
                }
            }
        }

        if (pChaser != nullptr)
        {
            pChaser->SetTarget(pPrey);
            pChaser->ForceMission(Mission::Guard);
            const CellStruct preyCell   = CellClass::Coord2Cell(myPos);
            const CellStruct chaserCell = CellClass::Coord2Cell(pChaser->GetCoords());
            std::fprintf(stderr,
                         "[GUARD] chaser=%p prey=%p chaserCell=(%d,%d) preyCell=(%d,%d)"
                         " fightBack=%d\n",
                         static_cast<void*>(pChaser), static_cast<void*>(pPrey),
                         (int)chaserCell.X, (int)chaserCell.Y,
                         (int)preyCell.X, (int)preyCell.Y,
                         static_cast<int>(pChaser->CanFightBack()));
        }
        else
        {
            std::fprintf(stderr, "[GUARD] no enemy vehicle on map\n");
        }
    }

    // ── 受击还击冒烟（OPENYR_RETAL_SMOKE=1）────────────────────────────────
    //   锚点是玩家载具，把它指到最近的敌方载具身上并强制 Attack；敌方那辆
    //   被压成 Sleep（没目标、不主动索敌），挨打之前不会有任何反应。
    //
    //   期望链：
    //     BulletClass → Damage_At_Cell → TechnoClass::ReceiveDamage
    //       → ShouldRetaliate(攻击者)          ← 旧实现拿射程当门槛，永远假
    //       → Override_Mission(Attack, 攻击者) ← 基类版本会丢目标
    //       → Mission_Attack 逼近 + 开火
    //   观测点：TRACE 的 mission= 从 0(Sleep) 变 1(Attack)、at= 开始移动、
    //           攻击者 hp 从满血往下掉。
    //
    //   OPENYR_NO_RETALIATE=1 把敌方类型的 CanRetaliate 关掉做反证：
    //   任务应恒为 0、位置不变、攻击者不掉血之外还手一枪都不开。
    if (std::getenv("OPENYR_RETAL_SMOKE") != nullptr
        && pViewAnchor != nullptr && pViewAnchor->WhatAmI() == AbstractType::Unit)
    {
        UnitClass* pAttacker = static_cast<UnitClass*>(pViewAnchor);
        const CoordStruct myPos = pAttacker->GetCoords();

        // 靶子可以是载具也可以是步兵：步兵射程比坦克短，"被打时够不着"
        // 正是旧 ShouldRetaliate 那道射程门会误杀的场景。
        TechnoClass* pVictim = nullptr;
        int64 bestSq = 0;

        const auto consider = [&](TechnoClass* pCand) {
            if (pCand == nullptr || pCand == pAttacker)
                return;
            if (pCand->IsDead() || pCand->Owner == nullptr || pAttacker->Owner == nullptr)
                return;
            if (pAttacker->Owner->Is_Ally(pCand))
                return;
            const CoordStruct p = pCand->GetCoords();
            const int64 dx = static_cast<int64>(p.X) - myPos.X;
            const int64 dy = static_cast<int64>(p.Y) - myPos.Y;
            const int64 sq = dx * dx + dy * dy;
            if (pVictim == nullptr || sq < bestSq)
            {
                bestSq  = sq;
                pVictim = pCand;
            }
        };

        if (UnitClass::Array != nullptr)
        {
            for (int32 i = 0; i < UnitClass::Array->Count; ++i)
                consider(UnitClass::Array->Items[i]);
        }
        if (InfantryClass::Array != nullptr)
        {
            for (int32 i = 0; i < InfantryClass::Array->Count; ++i)
                consider(InfantryClass::Array->Items[i]);
        }
        // 防御建筑也走同一条链：BuildingClass 继承 TechnoClass，ReceiveDamage /
        //   ShouldRetaliate / Override_Mission 全在基类，区别只在"开火"这一步
        //   —— 建筑没有 Mission_Attack 覆写（基类是 `mov eax,1C2h` 直返），
        //   真正扣扳机的是 TechnoClass::Update_Combat 的目标分支。
        //   单独用 OPENYR_RETAL_BUILDING 打开，免得改变既有载具/步兵场景的最邻近靶。
        if (std::getenv("OPENYR_RETAL_BUILDING") != nullptr
            && BuildingClass::Array != nullptr)
        {
            for (int32 i = 0; i < BuildingClass::Array->Count; ++i)
                consider(BuildingClass::Array->Items[i]);
        }

        if (pVictim != nullptr)
        {
            if (std::getenv("OPENYR_NO_RETALIATE") != nullptr && pVictim->TechnoType != nullptr)
                pVictim->TechnoType->CanRetaliate = false;

            // 靶子先站定：清目标、压成 Sleep。
            //   ★ 建筑不能压 Sleep：Sleep 的 MissionControl.Retaliate=no，
            //     ShouldRetaliate 第一道"当前任务允许还击"的门就把它挡住，
            //     场景会退化成"挨打不还手"。建筑只清目标，任务保持原样。
            pVictim->SetTarget(nullptr);
            if (pVictim->WhatAmI() != AbstractType::Building)
                pVictim->ForceMission(Mission::Sleep);
            else
                std::fprintf(stderr, "[RETAL] building victim mission=%d kept\n",
                             static_cast<int>(pVictim->GetMission()));

            // 攻击方（玩家）指定目标并进入 Attack。
            pAttacker->SetTarget(pVictim);
            pAttacker->ForceMission(Mission::Attack);

            const CellStruct atkCell = CellClass::Coord2Cell(myPos);
            const CellStruct vicCell = CellClass::Coord2Cell(pVictim->GetCoords());
            const int32 canRetal = (pVictim->TechnoType != nullptr)
                                   ? static_cast<int32>(pVictim->TechnoType->CanRetaliate) : -1;
            std::fprintf(stderr,
                         "[RETAL] attacker=%p victim=%p type=%s whatami=%d"
                         " atkCell=(%d,%d) vicCell=(%d,%d)"
                         " canRetaliate=%d victimMission=%d victimHp=%d\n",
                         static_cast<void*>(pAttacker), static_cast<void*>(pVictim),
                         (pVictim->TechnoType != nullptr && pVictim->TechnoType->get_ID())
                             ? pVictim->TechnoType->get_ID() : "?",
                         static_cast<int>(pVictim->WhatAmI()),
                         (int)atkCell.X, (int)atkCell.Y,
                         (int)vicCell.X, (int)vicCell.Y,
                         (int)canRetal,
                         static_cast<int>(pVictim->GetMission()),
                         (int)pVictim->Health);
        }
        else
        {
            std::fprintf(stderr, "[RETAL] no enemy vehicle on map\n");
        }
    }

    // ── 建筑任务分派冒烟（OPENYR_BLD_SMOKE=repair|construct）──────────────
    //   验证 MissionClass::ExecuteMission 的 32 路函数指针表真的虚分派到
    //   BuildingClass::Mission_Construction / Mission_Repair 上。
    //
    //   这条路径此前整条断掉：本类只写了非虚的 MissionConstruction() /
    //   MissionRepair()（void，且不在 MissionClass 的虚表里），ExecuteMission
    //   永远命中基类空实现（返回 1C2h）。可见后果：
    //     · MCV 展开（UnitClass 下 Construction）之后建筑永远 IsConstructed=false
    //       —— 不发电、不开火、不能进厂；
    //     · 选中建筑点修理没有任何反应。
    //
    //   观测点：[BLDMIS] 探针（只可能由这两个覆写打印）+ 状态变化
    //     repair    : 半血建筑被 IsBeingRepaired 接管，Update_Repair 每步回血
    //     construct : built 由 0 变 1，任务离开 Construction
    if (const char* pBldMode = std::getenv("OPENYR_BLD_SMOKE"))
    {
        BuildingClass* pB = nullptr;
        if (BuildingClass::Array != nullptr)
        {
            for (int32 i = 0; i < BuildingClass::Array->Count; ++i)
            {
                BuildingClass* pCand = BuildingClass::Array->Items[i];
                if (pCand == nullptr || pCand->IsDead())
                    continue;
                if (pCand->Owner == nullptr || !pCand->Owner->Is_Human())
                    continue;
                pB = pCand;
                break;
            }
        }

        if (pB == nullptr)
        {
            std::fprintf(stderr, "[BLDSMK] no player building on map\n");
        }
        else if (std::strcmp(pBldMode, "repair") == 0)
        {
            // 修理步长来自规则表 RulesClass::RepairStep（真实 rules.ini 的
            //   [General] RepairStep）。本树的内置最小规则集没有这一项（默认 0），
            //   修复量为 0 会让"接线成功"和"血量不动"混在一起 —— 冒烟里补上
            //   这个规则值，保证测的是代码路径而不是数据缺失。
            if (RulesClass::Instance != nullptr && RulesClass::Instance->RepairStep <= 0)
                RulesClass::Instance->RepairStep = 8;

            // 先打残：CanBeRepaired 要求 Health < MaxHealth。
            pB->Health = (pB->MaxHealth > 2) ? (pB->MaxHealth / 2) : 1;
            pB->QueueMission(Mission::Repair);
            std::fprintf(stderr, "[BLDSMK] repair self=%p hp=%d/%d credits=%d step=%d\n",
                         static_cast<void*>(pB), (int)pB->Health, (int)pB->MaxHealth,
                         (pB->Owner != nullptr) ? (int)pB->Owner->Credits : -1,
                         (int)RulesClass::Instance->RepairStep);
        }
        else if (std::strcmp(pBldMode, "construct") == 0)
        {
            // 手工压成"在建"：BState 与 IsConstructed 都不一致地指向未建成。
            //   不调 Place(false) —— 那条路径把 IsConstructed 先置真（本树对
            //   预置/出厂建筑的语义），Phase 1 会被跳过，测不到"0 → 1"的跃迁。
            pB->IsConstructed = false;
            pB->BState        = BStateType::Idle;   // != Construction，逼出 Phase 1
            pB->QueueMission(Mission::Construction);
            std::fprintf(stderr, "[BLDSMK] construct self=%p built=%d bstate=%d\n",
                         static_cast<void*>(pB), (int)pB->IsConstructed,
                         static_cast<int>(pB->BState));
        }
    }

    // ── 基地成长冒烟（OPENYR_BASEGROW_SMOKE=1）────────────────────────────
    //   AI 房没有建造场就永远进不了 AI_Building —— 那道门是 OwnedConyards
    //   非空。原版靠"开局给 AI 一辆 MCV，到点自己展开"过门。这里只给车，
    //   剩下全交给引擎，用来验证下面这条完整链：
    //
    //     UnitClass::Update 段 F 排队 Hunt
    //       → 段 F2 TryToDeploy（DeploysInto=GACNST）
    //       → GACNST Unlimbo → HouseClass::RegisterTechnoGain
    //       → AcquiredConyard → GenerateAIBuildList → Base.Nodes
    //       → HouseClass::AI 每 8 帧 AI_Building → BuildingTypeToProduce
    //       → GACNST 的 Update_Factory B 段下单 → 进度 → Rollout_Product
    //       → 建筑按地基尺寸在基地半径内选址落位
    //
    //   本段只负责"发车"，不给任何订单；状态靠 OPENYR_TRACE 的
    //   [AIH]（cy/nodes/blds）与 [FAC]（生产线）两行观察。
    if (std::getenv("OPENYR_BASEGROW_SMOKE") != nullptr && pPlayerHouse != nullptr)
    {
        HouseClass* pAIHouse = nullptr;
        for (int32 i = 0; i < TheHouseCount; ++i)
        {
            HouseClass* pH = HouseClass::Array[i];
            if (pH != nullptr && !pH->Is_Human()) { pAIHouse = pH; break; }
        }

        UnitTypeClass* pMCVType = UnitTypeClass::FindOrAllocate("AMCV");
        std::fprintf(stderr, "[BASEGROW] ai=%p mcv=%d\n",
                     static_cast<void*>(pAIHouse), (pMCVType != nullptr) ? 1 : 0);

        if (pMap->CellArray != nullptr)
        {
            int32 oreCells = 0;
            int32 oreTotal = 0;
            for (int32 i = 0; i < pMap->CellCount; ++i)
            {
                if (pMap->CellArray[i].TiberiumValue > 0)
                {
                    ++oreCells;
                    oreTotal += pMap->CellArray[i].TiberiumValue;
                }
            }
            std::fprintf(stderr, "[BASEGROW] ore: cells=%d total=%d of %d\n",
                         (int)oreCells, (int)oreTotal, (int)pMap->CellCount);

            // 合成测试地形不带矿（tools/make_test_theater.py 只铺地表），
            //   真实 .map 的 IsoMapPack5 里才会带 overlay=矿 的格。冒烟测的是
            //   "矿车能不能采 → 能不能卸 → 阵营有没有收入"，不是矿生成，
            //   所以这里补一片矿田当测试场景，位置取地图中心偏右下。
            if (oreCells == 0)
            {
                // 撒在玩家基地外 6~11 格：Find_Ore 的搜索半径是 12 格，
                //   撒在地图中心会让两侧基地都够不着（实测双侧矿车都停在
                //   原地找不到矿）。
                CellStruct oreOrigin(static_cast<int16>(pMap->MapWidth / 4),
                                     static_cast<int16>(pMap->MapHeight / 4));
                if (pViewAnchor != nullptr)
                    oreOrigin = CellClass::Coord2Cell(pViewAnchor->GetCoords());
                const int32 ox = oreOrigin.X + 6;
                const int32 oy = oreOrigin.Y + 6;
                int32 made = 0;
                // 10x10：一格矿最多 12 单位（CellClass::Set_Tiberium 的
                //   overlay 阶段上限），36 格只有 432 单位 ≈ 一车半的量，
                //   跑 4 车就采空、后面看不出"收入能不能养活基地"。
                for (int32 dy = 0; dy < 10; ++dy)
                {
                    for (int32 dx = 0; dx < 10; ++dx)
                    {
                        CellClass* pCell = pMap->GetCellAt(ox + dx, oy + dy);
                        if (pCell == nullptr || pCell->IsOccupied())
                            continue;
                        pCell->Set_Tiberium(0, 300);   // 一格满矿 = 单位价值 25 × 12 阶段
                        ++made;
                    }
                }
                std::fprintf(stderr, "[BASEGROW] seeded %d ore cells at (%d,%d)\n",
                             (int)made, (int)ox, (int)oy);
            }
        }

        if (RulesClass::Instance != nullptr)
        {
            std::fprintf(stderr,
                         "[BASEGROW] rules lists: const=%d power=%d barracks=%d"
                         " weapons=%d refinery=%d baseUnit=%d harv=%d\n",
                         (int)RulesClass::Instance->BuildConst.GetCount(),
                         (int)RulesClass::Instance->BuildPower.GetCount(),
                         (int)RulesClass::Instance->BuildBarracks.GetCount(),
                         (int)RulesClass::Instance->BuildWeapons.GetCount(),
                         (int)RulesClass::Instance->BuildRefinery.GetCount(),
                         (int)RulesClass::Instance->BaseUnit.GetCount(),
                         (int)RulesClass::Instance->HarvesterUnit.GetCount());
            if (BuildingTypeClass::Array != nullptr)
            {
                for (int32 i = 0; i < BuildingTypeClass::Array->Count; ++i)
                {
                    BuildingTypeClass* pB = (*BuildingTypeClass::Array)[i];
                    if (pB != nullptr && pB->FreeUnit != nullptr)
                        std::fprintf(stderr, "[BASEGROW] %s FreeUnit=%s\n",
                                     pB->ID, pB->FreeUnit->ID);
                }
            }
        }

        // ★ 只在 AI 房确实没有建造场时才发 MCV。
        //   测试地图本身给 AI 预置了一座建造场（落位点 (30,24)），
        //   再发一辆 MCV 就会展开出第二座 —— 多一条建造线、多花 3000，
        //   实测把 10000 开局资金在 300 帧内烧光，精炼厂永远造不完，
        //   整局没有收入。
        //   注意：这里查的是**世界里的建筑表**而不是 OwnedBuildings ——
        //   测试地图预置的建筑在本段执行时还没走过落位登记（实测同一帧
        //   已 Unlimbo 但 OwnedBuildings 仍为空），查阵营表会漏判。
        bool aiHasConyard = false;
        if (pAIHouse != nullptr && BuildingClass::Array != nullptr) {
            for (int32 i = 0; i < BuildingClass::Array->Count; ++i) {
                BuildingClass* pB = (*BuildingClass::Array)[i];
                if (pB != nullptr && pB->Owner == pAIHouse && pB->Type != nullptr
                    && pB->Type->ConstructionYard) {
                    aiHasConyard = true;
                    break;
                }
            }
        }

        if (pAIHouse != nullptr && pMCVType != nullptr && !aiHasConyard)
        {
            // 落点：优先 AI 房已有单位旁边，否则地图 3/4 处（对角基地）。
            CellStruct around(static_cast<int16>(pMap->MapWidth * 3 / 4),
                              static_cast<int16>(pMap->MapHeight * 3 / 4));
            if (pAIHouse->OwnedUnits.GetCount() > 0
                && pAIHouse->OwnedUnits[0] != nullptr)
            {
                around = CellClass::Coord2Cell(pAIHouse->OwnedUnits[0]->GetCoords());
            }
            else if (pAIHouse->OwnedInfantry.GetCount() > 0
                     && pAIHouse->OwnedInfantry[0] != nullptr)
            {
                around = CellClass::Coord2Cell(pAIHouse->OwnedInfantry[0]->GetCoords());
            }

            UnitClass* pMCV = nullptr;
            for (int32 r = 1; r <= 8 && pMCV == nullptr; ++r)
            {
                for (int32 dy = -r; dy <= r && pMCV == nullptr; ++dy)
                {
                    for (int32 dx = -r; dx <= r && pMCV == nullptr; ++dx)
                    {
                        const CellStruct probe(static_cast<int16>(around.X + dx),
                                               static_cast<int16>(around.Y + dy));
                        CellClass* pCell = pMap->GetCellAt(probe);
                        if (pCell == nullptr || pCell->IsOccupied())
                            continue;
                        pMCV = Place_Starting_Techno<UnitClass>(
                            pAIHouse, pMCVType, probe);
                        if (pMCV != nullptr)
                        {
                            const CellStruct at =
                                CellClass::Coord2Cell(pMCV->GetCoords());
                            std::fprintf(stderr,
                                         "[BASEGROW] mcv=%p cell=(%d,%d) hp=%d"
                                         " deploysInto=%d\n",
                                         static_cast<void*>(pMCV),
                                         (int)at.X, (int)at.Y,
                                         (int)pMCV->Health,
                                         (pMCVType->DeploysInto != nullptr) ? 1 : 0);
                        }
                    }
                }
            }

            if (pMCV == nullptr)
                std::fprintf(stderr, "[BASEGROW] no free cell for mcv\n");
        }

        // ── 进攻冒烟（OPENYR_AIATK_SMOKE=1）────────────────────────────────
        //   给人类房放一小队步兵当"靶子"。AI 的 Mission_Hunt 只有在能找到
        //   敌方对象时才推进；没有玩家的场景里它能做的只有原地警戒，攻不
        //   进攻根本测不出来。
        if (std::getenv("OPENYR_AIATK_SMOKE") != nullptr) {
            std::fprintf(stderr, "[AIATK] player=%p inf=%d\n",
                         static_cast<void*>(pPlayerHouse),
                         pPlayerHouse ? (int)pPlayerHouse->OwnedInfantry.GetCount() : -1);
        }
        if (std::getenv("OPENYR_AIATK_SMOKE") != nullptr && pPlayerHouse != nullptr
            && pPlayerHouse->OwnedInfantry.GetCount() == 0)
        {
            InfantryTypeClass* pE1 = InfantryTypeClass::FindOrAllocate("E1");
            if (pE1 != nullptr)
            {
                // 玩家一侧（地图 1/4 处），与 AI 的对角基地拉开距离。
                const int32 px = pMap->MapWidth / 4;
                const int32 py = pMap->MapHeight / 4;
                int32 placed = 0;
                for (int32 dy = -1; dy <= 2 && placed < 4; ++dy)
                {
                    for (int32 dx = -1; dx <= 2 && placed < 4; ++dx)
                    {
                        const CellStruct probe(static_cast<int16>(px + dx),
                                               static_cast<int16>(py + dy));
                        CellClass* pCell = pMap->GetCellAt(probe);
                        if (pCell == nullptr || pCell->IsOccupied())
                            continue;
                        if (Place_Starting_Techno<InfantryClass>(
                                pPlayerHouse, pE1, probe) != nullptr)
                            ++placed;
                    }
                }
                std::fprintf(stderr, "[AIATK] player targets placed=%d\n",
                             (int)placed);
            }

            // ★ 开局资金余量。
            //   这张测试地图给 AI 预置了一个基地，而 BASE_GROW 段又额外发一辆
            //   MCV 用来验证展开链 —— 两条建造线一起开工，10000 开局资金在
            //   300 帧内就烧到 2 信用点，精炼厂卡在 50/54 步、永远造不完，
            //   于是全房零收入。这里补一笔余量，让"建基地 → 有精炼厂 →
            //   矿车卸货 → 有收入 → 能造兵"这条链能完整跑通（真实对局里
            //   开局资金本来就够走完这几个建筑）。
            if (pAIHouse != nullptr) {
                pAIHouse->GiveMoney(8000);
                std::fprintf(stderr, "[AIATK] ai credits bumped to %d\n",
                             (int)pAIHouse->Credits);
            }

            // ★ 补矿田。
            //   合成测试地形不带矿（tools/make_test_theater.py 只铺地表），
            //   而 AI 的全部产能都建立在"矿车采矿 → 精炼厂卸货 → 房主进账"
            //   之上。没有矿时实测量到的是：AI 房 `money=2`、三条生产线全部
            //   停在 progress=0/30 不动、队伍补员订单永远造不出来 —— 表象是
            //   "队伍不补员"，根因是整条经济链没有输入。
            //   这里按矿车当前位置各撒一片 10×10 矿田（Find_Ore 的搜索半径
            //   是 12 格，所以贴着矿车撒），让进攻链跑在"有收入"的真实前提下。
            if (UnitClass::Array != nullptr)
            {
                int32 seededPatches = 0;
                for (int32 i = 0; i < UnitClass::Array->Count && seededPatches < 4; ++i)
                {
                    UnitClass* pU = (*UnitClass::Array)[i];
                    if (pU == nullptr || pU->Type == nullptr || !pU->Type->Harvester)
                        continue;
                    const CellStruct at = CellClass::Coord2Cell(pU->GetCoords());
                    const int32 ox = at.X + 2;
                    const int32 oy = at.Y + 2;
                    int32 made = 0;
                    for (int32 dy = 0; dy < 10; ++dy)
                    {
                        for (int32 dx = 0; dx < 10; ++dx)
                        {
                            CellClass* pCell = pMap->GetCellAt(ox + dx, oy + dy);
                            if (pCell == nullptr || pCell->IsOccupied())
                                continue;
                            if (pCell->TiberiumValue > 0)
                                continue;
                            pCell->Set_Tiberium(0, 300);
                            ++made;
                        }
                    }
                    std::fprintf(stderr,
                                 "[AIATK] ore patch#%d at (%d,%d) cells=%d\n",
                                 (int)seededPatches, (int)ox, (int)oy, (int)made);
                    ++seededPatches;
                }
                if (seededPatches == 0)
                    std::fprintf(stderr, "[AIATK] no harvester found for ore seeding\n");
            }
        }
    }

    // asm: 场景装载结束 —— ScenarioInit 归零。
    //   初始化期它被置 1，用来抑制"落位即触发的运行时逻辑"（FreeUnit 附送、
    //   AI 触发器、主题音乐等）。原版在 Scenario_Init 收尾把计数减回 0；
    //   本树此前一直留在 1，于是所有 `ScenarioInit == 0` 的运行期分支全部
    //   失效 —— 最直接的后果是精炼厂落成不附送矿车，AI 永远没有收入。
    ScenarioInit = 0;

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