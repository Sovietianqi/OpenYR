#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <chrono>

#include "Game/MainLoop.h"

// ═══════════════════════════════════════════════════════════════════════════
// FrameTimestep —— 定步长累加器
//
// 这一段是纯逻辑，不依赖任何引擎类型，因此 tools/test_timestep.cpp 可以
// 用 -DOPENYR_TIMESTEP_UNIT_TEST 直接编译本文件来单测它（同一份实现，
// 不是另抄一遍）。
// ═══════════════════════════════════════════════════════════════════════════

long long FrameTimestep::MonotonicMicros()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

FrameTimestep::IterationPlan
FrameTimestep::PlanIteration(long long* pAccumUs,
                             long long  elapsedUs,
                             unsigned   targetFrameMs,
                             int        fixedTicks,
                             int        maxTicks)
{
    IterationPlan plan{1, 0};

    // 定步长回归模式：每轮固定 N 帧，不看时钟、不睡。
    if (fixedTicks > 0)
    {
        plan.Ticks = fixedTicks;
        return plan;
    }

    const long long stepUs = static_cast<long long>(targetFrameMs) * 1000;

    long long accum = (pAccumUs != nullptr) ? *pAccumUs : 0;

    // 时钟回退：负 delta 不扣时间，余量也不允许为负。
    if (elapsedUs > 0)
        accum += elapsedUs;
    if (accum < 0)
        accum = 0;

    int ticks = 0;
    if (stepUs > 0)
    {
        const int cap = (maxTicks > 0) ? maxTicks : 1;
        while (accum >= stepUs && ticks < cap)
        {
            accum -= stepUs;
            ++ticks;
        }

        // 上限截断：卡顿期间积压的整步时间直接丢掉，
        // 与原版 sub_55E160 一致（落后就慢，不追帧）。
        if (accum >= stepUs)
            accum %= stepUs;
    }

    if (ticks == 0)
    {
        // 预算还没走满：睡到下一帧的起点（asm sub_55E160 的 Sleep(remaining)）。
        // 睡掉的时间正好把余量补到一整步，本轮那一帧就由它来付账 ——
        // 所以余量归零，调用方在睡完后重新取时钟（见 Main_Game 的 lastUs）。
        plan.Ticks   = 1;   // asm：一轮一帧，Frame++ 无条件执行
        plan.SleepUs = (stepUs > accum) ? (stepUs - accum) : 0;
        accum        = 0;
    }
    else
    {
        plan.Ticks = ticks;
    }

    if (pAccumUs != nullptr)
        *pAccumUs = accum;

    return plan;
}

#ifndef OPENYR_TIMESTEP_UNIT_TEST

#include "Game/Game.h"
#include "Game/GameInit.h"
#include "Game/Externs.h"
#include "Game/LogicClass.h"
#include "Game/ChainDebug.h"
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
#include "AI/AITeamClass.h"
#include "AI/AITriggerTypeClass.h"
#include "AI/TagClass.h"
#include "AI/TriggerClass.h"
#include "Particles/ParticleClass.h"
#include "Animations/AnimClass.h"
#include "Locomotion/LocomotionClass.h"
#include "SW/SuperClass.h"
#include "Special/TiberiumClass.h"
#include "Special/WaveClass.h"
#include "Special/RadSiteClass.h"

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

namespace {

// asm: 一轮一帧。Main_Loop 的帧尾 `inc edx / mov Frame,edx`（0x756888）
// 执行一次，循环体内只有一处 `call LogicClass_Update`（0x756687），
// 也没有任何补帧 / 跳帧分支 —— 所以单轮上限就是 1。
const int kMaxTicksPerIteration = 1;

// OPENYR_FIXED_TIMESTEP=N（N 为正整数）→ 每轮强制推进 N 个逻辑帧，
// 不读时钟、不 Sleep。未设置 / 非法值 → 0 = 实时模式（生产默认，行为不变）。
int ReadFixedTimestepEnv()
{
    const char* raw = std::getenv("OPENYR_FIXED_TIMESTEP");
    if (raw == nullptr || *raw == '\0')
        return 0;

    char*      end   = nullptr;
    const long value = std::strtol(raw, &end, 10);

    // 非法输入不静默改行为：按未设置处理（实时模式）。
    if (end == raw || value <= 0 || value > 100000)
        return 0;

    return static_cast<int>(value);
}

// 推进 count 个逻辑帧。帧后的拷贝保护检查与 FPS 统计按帧计，
// 定步长模式下它们照常执行，只是不参与"该跑几帧"的判断。
void RunFrameSteps(int count)
{
    for (int i = 0; i < count && !Game::ShutdownRequested; ++i)
    {
        // 帧号递增在 Main_Loop 的**帧尾**（asm L756886-756890），不在帧首：
        // 本帧所有 Frame 读取看到的仍是上一帧编号，任务计时器
        // `Frame - TimeStart >= TimeLeft` 的比较依赖这一点。
        Main_Loop();

        if ((Game::CurrentFrame % COPYPROT_CHECK_INTERVAL) == 0)
        {
            CopyProtection_Check();
        }

        Record_FrameStats();
    }
}

}   // namespace

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

    const int fixedTicks = ReadFixedTimestepEnv();

    // 步长累加器的余量（微秒）与上一轮的时间戳。只在实时模式下使用；
    // 定步长模式连时钟都不读。
    long long accumUs = 0;
    long long lastUs  = (fixedTicks == 0) ? FrameTimestep::MonotonicMicros() : 0;

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

        // 5. Frame pacing
        long long deltaUs = 0;
        if (fixedTicks == 0)
        {
            const long long nowUs = FrameTimestep::MonotonicMicros();
            deltaUs = nowUs - lastUs;
            lastUs  = nowUs;
        }

        const FrameTimestep::IterationPlan plan =
            FrameTimestep::PlanIteration(&accumUs, deltaUs,
                                         Game::TargetFrameTime,
                                         fixedTicks,
                                         kMaxTicksPerIteration);

        // 定步长模式永不睡；实时模式的睡眠由累加器算出的剩余预算决定
        // （asm sub_55E160：`Sleep(remaining)`）。
        if (plan.SleepUs > 0)
        {
            Game::Game_Sleep(static_cast<unsigned int>((plan.SleepUs + 999) / 1000));

            // 睡掉的这段时间已经被累加器记成"本帧的预算"，重新取基准，
            // 否则下一轮的 delta 会把它再算一次（等于每帧多算一次睡眠）。
            lastUs = FrameTimestep::MonotonicMicros();
        }

        // 6. Execute this round's logic frames
        RunFrameSteps(plan.Ticks);
    }

    Game::GameInProgress = false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Main_Loop — per-frame update
// ═══════════════════════════════════════════════════════════════════════════

void Main_Loop()
{
    // ═══════════════════════════════════════════════════════════════════════
    // 帧序严格按 asm Main_Loop (proc@755698-756938)：
    //
    //   输入 → LogicClass_Update → 表现/网络补充 → 渲染/音频
    //        → Frame++（帧尾）→ VisibleObjects_DeleteDead（帧尾）
    //
    // asm 里没有 “Update_AI / Update_Objects / Update_Combat / Update_Locomotion”
    // 这样的阶段划分，唯一的逻辑 tick 就是 LogicClass_Update（L756687），
    // 它内部再按固定顺序驱动 Tags/Teams/Objects(vec_Logics)/RadSites/
    // Factories/Tactical/Map/Houses。这里照此重构。
    // ═══════════════════════════════════════════════════════════════════════

    // ── 帧前：输入面（asm L756694-756796 DisplayClass::Scroll_Map 等）────
    Update_Input();

    // ── ★ 每帧逻辑总入口（asm L756686-756687：ecx = &vec_Logics）─────────
    LogicClass_Update();

    // ── 表现/网络补充 ─────────────────────────────────────────────────────
    // 这些是 LogicClass_Update 未覆盖的部分（asm 里散落在 Main_Loop 尾部
    // 的 Call_Back / sub_637550 / sub_647260 与各超武表现层）。
    Update_Network();

    // AI 触发（AITriggerType）评估：原版挂在 HouseClass_Update 下游，
    // OpenYR 的 HouseClass::Update 尚未接这一支，故作为补充阶段保留。
    Update_AI();

    // 过渡期补充：只驱动**未登记进 vec_Logics** 的弹体/波束/足类。
    // 已登记的对象由上面的 vec_Logics 循环驱动过，这里必须跳过，否则
    // 位移/开火/寿命会按帧翻倍，弹体还会被二次删除。
    Update_Combat();
    Update_Locomotion();

    Update_Animations();
    Update_Particles();
    Update_SuperWeapons();
    Update_SpecialEffects();

    // ── UI 面（雷达/侧栏/显示层）──────────────────────────────────────────
    Update_Radar();
    Update_Sidebar();
    Update_Display();

    // ── 表现面：整帧渲染 ──────────────────────────────────────────────────
    Render_Frame();

    // ── 音频面（asm sub_637550 / sub_647260）─────────────────────────────
    Update_Audio();

    // ═══════════════════════════════════════════════════════════════════════
    // 帧尾（asm L756886-756907）
    //
    //   mov edx, Frame / inc edx / mov Frame, edx      ← L756888  Frame++
    //   call sub_55E160                                ← L756903  帧尾整理
    //   call VisibleObjects_DeleteDead                 ← L756905  回收死对象
    //   call sub_637270                                ← L756907
    //
    // sub_55E160 / sub_637270 在 OpenYR 中无独立等价物（前者是帧计时与
    // 速度采样，已由 FrameTimestep 的定步长驱动承担；后者是音频队列刷新，
    // 由 Update_Audio 承担）。
    // ═══════════════════════════════════════════════════════════════════════

    // asm L756888：帧号在帧尾自增。
    Game::AdvanceFrame();

    // asm L756905：扫 vec_VisibleObjects，把 IsDead() 的对象摘表并清理显示层记录。
    ObjectClass::VisibleObjects_DeleteDead();

    // 调试探针（原版无此步）：OPENYR_CHAIN_DEBUG=1 时每 300 帧汇总一次
    // 「#2 TechnoClass::Update / #3 MissionClass::Update→Mission_* / #6
    //  FootClass::Update / #7 ILocomotion::Process」的调用次数，
    // 用于在实际对局里确认逐帧链路确实连通。
    ChainDebug::Tick();
}

// ═══════════════════════════════════════════════════════════════════════════
// Update_Triggers — 触发/事件族的每帧驱动
//
// asm: LogicClass_Update 的 Tag 段（L751177…L751336）—— 唯一每帧触发器入口。
// 遍历 vec_Tags_all，对每个标签上抛全局事件族（50/27/28/36/37/45/46/13/51/14），
// 命中的触发器经 HaveAllEventsOccured → FireActions → TActionClass::Execute 执行。
// 随后对所有触发器做一次服务遍（Waiting 计时器递减与重新武装）。
// ═══════════════════════════════════════════════════════════════════════════

void Update_Triggers()
{
    TagClass::ProcessTagEvents();
    TriggerClass::ProcessTriggerEvents();
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
// 根据游戏行为，可知 Input 负责下面这段逻辑。
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
// 根据游戏行为，可知 Network 负责下面这段逻辑。
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
// 根据游戏行为，可知 AI 负责下面这段逻辑。
// ═══════════════════════════════════════════════════════════════════════════

void Update_AI()
{
    // ★ 本阶段已清空：AI 触发器的评估与「触发器 → 新队伍」的成军，原版都在
    //   HouseClass_Update 内部（asm:552777..552900 的 TeamDelayTimer 段 →
    //   HouseClass_TryFindEligibleAITriggers → TeamTypeClass_CreateTeam，
    //   调用点 L552843）。该段现已按 asm 落在
    //   HouseClass::Update_Team_Production()，由 LogicClass_Update 第 22 步
    //   的 House 循环逐帧驱动（HouseClass::Update 内部先于 AI() 调用）。
    //
    //   此前这里有一份 15 帧节流的替代循环：它绕过 TeamDelayTimer、绕过
    //   难度/国家门，而且每条触发器都被 RegisterSuccess/Failure 记一次账。
    //   保留它会与 asm 路径重复成军并污染触发器权重，故删除。
    //
    //   Team 的逐帧驱动（vec_Teams → [edx+5Ch]）在 LogicClass_Update 第 7 步。
}

// ═══════════════════════════════════════════════════════════════════════════
// Combat —— 过渡期补充驱动
//
// ★ Factory 循环已移入 LogicClass_Update 第 21 步（asm L751897-751912），
//   原版的生产推进发生在那一步，不在对象层。此处不再重复。
//
// 本函数只负责**未登记进 vec_Logics** 的弹体/波束：已登记的对象在上面
// vec_Logics 循环里被 `call [obj+0x5C]` 驱动过一次，这里必须跳过，否则
// 弹道会按帧翻倍、寿命减半，且已引爆的弹体会被二次析构。
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
            GameDelete(pBullet);          // 析构里会同时摘 BulletClass::Array 与 vec_Logics
            continue;
        }

        // 已登记进 vec_Logics 的弹体本帧已被 LogicClass_Update 驱动，
        // 此处只负责回收，不再重复 Update。
        if (pBullet->Is_Logic_Registered())
            continue;

        if (pBullet->IsBulletActive)
        {
            pBullet->Update();
        }
    }

    // ── 波束（asm: vec_Waves）─────────────────────────────────────────────
    // 根据游戏行为，可知音波束与磁能束与弹体同属"战斗"一轮：原版在同一
    //  次遍历里倒序扫描 vec_Waves，凡存活标志（obj+0x98，本项目映射到
    //  ObjectClass::Alive）非零的，就经 vtable +0x5C 调 Update —— 由
    //  WaveClass::Update 自己按 Type 推进扩散、结算声波伤害，并在寿命耗尽
    //  时自行 UnInit 摘除。这里复刻同一遍历。
    if (WaveClass::Array != nullptr)
    {
        for (int32 i = WaveClass::Array->Count - 1; i >= 0; --i)
        {
            WaveClass* pWave = (*WaveClass::Array)[i];
            if (pWave == nullptr)
                continue;

            // 寿命耗尽后 Update 会调 UnInit；此处把已死的波清出池子，
            // 对应 asm 的 VisibleObjects_DeleteDead 收尾。
            if (pWave->IsDead())
            {
                GameDelete(pWave);
                continue;
            }

            // 已登记进 vec_Logics 的波束已在对象循环里推进过，跳过以免翻倍。
            if (pWave->Is_Logic_Registered())
                continue;

            // asm: `mov al,[ecx+98h] / test / jz` —— 仅存活的波才推进。
            if (pWave->Alive != 0)
            {
                pWave->Update();
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 根据游戏行为，可知 Locomotion 负责下面这段逻辑。
// ═══════════════════════════════════════════════════════════════════════════

void Update_Locomotion()
{
    // ★ 过渡期补充：单位若已登记进 vec_Logics，本帧已由
    //   vec_Logics → UnitClass::Update → FootClass::Update → AI →
    //   Update_Movement → ILocomotion::Process 驱动过（asm L481617），
    //   这里必须跳过，否则位移按帧翻倍。
    //
    //   FootClass::Locomotion->Process() dispatches into the concrete
    //   locomotion's main loop (DriveLocomotionClass::blah etc.).
    if (UnitClass::Array != nullptr)
    {
        for (int32 i = 0; i < UnitClass::Array->Count; ++i)
        {
            UnitClass* pUnit = (*UnitClass::Array)[i];
            if (pUnit == nullptr || pUnit->Is_Logic_Registered())
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
            if (pInf == nullptr || pInf->Is_Logic_Registered())
                continue;

            LocomotionClass* pLoco = pInf->Get_Locomotion();
            if (pLoco != nullptr)
                pLoco->Process();
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 根据游戏行为，可知 Animations 负责下面这段逻辑。
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
// 根据游戏行为，可知 Particles 负责下面这段逻辑。
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
// 根据游戏行为，可知 SuperWeapons 负责下面这段逻辑。
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
// 根据游戏行为，可知 SpecialEffects 负责下面这段逻辑。
// ═══════════════════════════════════════════════════════════════════════════

void Update_SpecialEffects()
{
    // Ore growth and spreading are driven per cell by the map's cell update
    // pass; there is no separate manager object in the original, so nothing
    // extra is done here.
}

// ═══════════════════════════════════════════════════════════════════════════
// 根据游戏行为，可知 Radar 负责下面这段逻辑。
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
// 根据游戏行为，可知 Sidebar 负责下面这段逻辑。
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
// 根据游戏行为，可知 Display 负责下面这段逻辑。
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

    // ── Phase 0: 战术层 tick（asm L751895 → TacticalClass_Update）──────────
    //   asm 的调用点在 LogicClass_Update 第 20 步，已由那里的
    //   TheTactical->Update() 覆盖；这里再挂一次作为渲染阶段的兜底：
    //   Update() 内部有 `LastAIFrame == Frame` 门，同一帧不会重复执行。
    if (TheTactical != nullptr)
        TheTactical->Update();

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
// 根据游戏行为，可知 Audio 负责下面这段逻辑。
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
// asm: Call_Back (0x48D092) —— 模态等待期间的心跳回调
//
//   call sub_406F70                                    ; 音频/主题服务遍
//   if (vec_MovieHandles.length > 0 && data[0]) data[0]->[vt+8]()
//   if (Session == 3 || Session == 4) { sub_48D1E0() ; 重读 Session }
//   if (Session == 1) { if (!(dword_A8DAB8 & 1)) return ; }
//   else if (Session != 2) goto session4
//   sub_5F1C60(&dword_A8EA70)                          ; 接收队列复位
//   if (Scenario->[+3598h]) {
//       while (sub_5F1C30(&queue,&id,&rec) > 0) {
//           if (id == 0x6C && (rec[18h] & 0xFF) != dword_A8DB30)
//               sub_643C50(ProgressScreen, 1, (double)rec[1Dh], -1, -1) ;
//       }
//   }
// session4:
//   if (Session != 4) return ;
//   if (byte_B77DC2) return ;                          ; 已收尾则不心跳
//   if (time(0) == dword_89E9B0) return ;              ; 本秒已发过
//   if (ppv && !dword_B77DCC) ppv->[vt+0Ch]() ;
//   if (dword_AC1170) dword_AC1170->[vt+1Ch]() ;
//   dword_89E9B0 = time(0) ;
//
// 等价降级（原版设施在本工程尚未建模，均在此集中标注）：
//   * vec_MovieHandles 的播放器对象 —— MovieClass 只有片名表，没有 vt 持有者，
//     故不做影片轮询；
//   * sub_48D1E0 / sub_5F0040 这类会话服务遍 —— 无对应实现；
//   * dword_A8EA70 的收包队列（sub_5F1C30/5F1C60）—— ConnectionClass 还没有
//     该队列，队列段保留控制流但不再取包；
//   * ppv / dword_AC1170 两个 vt 回调 —— 对应的 Winsock / WOL 对象未建模。
// ═══════════════════════════════════════════════════════════════════════════

namespace {
// asm: dword_89E9B0 —— 上一次心跳的秒数（BSS 初值 0）
int32 s_CBLastPing = 0;
}

void Call_Back()
{
    // asm: call sub_406F70 —— 音频/主题服务遍（节流 34ms，内含 Theme AI + EVA）
    Update_Audio();

    // asm: mov eax,Session / cmp 3 / jz ; cmp 4 / jnz
    int32 session = g_Session;
    if (session == 3 || session == 4)
    {
        // asm: call sub_48D1E0 —— 会话服务遍（本工程未建模）
        session = g_Session;               // asm: mov eax, Session（重读）
    }

    // asm: cmp eax,1 / jz loc_48D0D3 ; cmp eax,2 / jnz loc_48D166
    bool runEvents = false;
    if (session == 1)
    {
        // asm: test byte ptr dword_A8DAB8,1 / jz exit
        if ((g_SessionReadyFlag & 1) != 0)
            runEvents = true;
    }
    else if (session == 2)
    {
        runEvents = true;
    }

    if (runEvents)
    {
        // asm: mov ecx,offset dword_A8EA70 / call sub_5F1C60（接收队列复位）
        //     队列对象未建模，见函数头"等价降级"。

        ScenarioClass* const pScenario = ScenarioClass::Instance;
        const bool bProgressPackets =
            (pScenario != nullptr) && (pScenario->unknown_3598 != 0);

        if (bProgressPackets)
        {
            // asm: 循环 sub_5F1C30(&queue,&id,&rec)，id==6Ch 且发送者不是自己时
            //      把 rec[+1Dh] 的 int64 交给进度屏（sub_643C50）。
            //      队列未建模 → 本循环没有元素可取。
        }

        session = g_Session;               // asm: loc_48D161 mov eax, Session
    }

    // asm: loc_48D166: cmp eax,4 / jnz exit
    if (session != 4)
        return;

    // asm: mov al,byte_B77DC2 / test / jnz exit
    //      （"心跳已停"标志；该全局属于未建模的 WOL 收尾流程）

    // asm: push 0 / call _time / cmp eax,dword_89E9B0 / jz exit
    const int32 now = static_cast<int32>(time(nullptr));
    if (now == s_CBLastPing)
        return;

    // asm: ppv 非空且未占用时 ppv->[vt+0Ch]()；dword_AC1170->[vt+1Ch]()
    //      两个对象未建模，见函数头"等价降级"。

    // asm: dword_89E9B0 = time(0)
    s_CBLastPing = static_cast<int32>(time(nullptr));
}

// ═══════════════════════════════════════════════════════════════════════════
// Record_FrameStats — update frame rate counters
//
// 只做统计：定步长模式下照常累计（此时 fps 是"每墙钟秒跑了多少逻辑帧"），
// 但绝不参与"该跑几帧"的判断 —— 那个判断只在 FrameTimestep::PlanIteration 里。
// ═══════════════════════════════════════════════════════════════════════════

void Record_FrameStats()
{
    ++s_FrameRateCounter;

    unsigned int now   = Game::GetTickCount();
    unsigned int diff  = now - s_FrameRateTimer;

    // asm: 帧尾 `sub eax, dword_A8B55C` —— 本帧经过的毫秒数（统计用）。
    s_FrameElapsedTime = now - s_FrameStartTime;
    s_FrameStartTime   = now;

    if (diff >= 1000)
    {
        s_FrameRateDisplay = s_FrameRateCounter;
        Game::FrameRate    = s_FrameRateCounter;
        s_FrameRateCounter = 0;
        s_FrameRateTimer   = now;

        // 冒烟诊断：每秒一次，报出逻辑帧号/帧率/各类对象存活数，
        // 用于确认主循环与 vec_* 驱动链真的在推进（OPENYR_DEBUG=1 才输出）。
        if (g_bDebugMode)
        {
            fprintf(stderr,
                    "[FRAME] frame=%u fps=%d bullets=%d technos=%d logics=%d\n",
                    (unsigned)Game::CurrentFrame, (int)s_FrameRateDisplay,
                    (BulletClass::Array   ? (int)BulletClass::Array->Count   : -1),
                    (TechnoClass::Array   ? (int)TechnoClass::Array->Count   : -1),
                    (vec_Logics ? (int)vec_Logics->Count : -1));

            // 冒烟诊断：渲染侧计数器（TacticalClass::Update / Draw_Objects /
            // Draw_Shroud 与 DisplayClass::Draw 维护）。
            //   visible    = 显示层里未 limbo 的对象数（登记）
            //   drawn      = 通过视口剔除后真正派发 Draw 的对象数
            //   drawcalls  = 真正进入具体 Draw() 并返回的次数（真画，不是登记）
            //   culled     = limbo / 视口外 / 未知类型被剔除的对象数
            //   visiblecells = 本帧登记进可见格表的格子数（视口剔除后的格数，
            //                  不是地图总格数，也不是 dirty 区域数）
            //   view       = 本帧视口矩形（ScreenX, ScreenY, Width, Height）
            fprintf(stderr,
                    "[RENDER] visible=%d drawn=%d drawcalls=%d culled=%d "
                    "view=(%d,%d,%d,%d) shrouded=%d visiblecells=%d "
                    "vobjs=%d rframes=%u ticks=%u\n",
                    (int)g_RenderDebugStats.VisibleObjects,
                    (int)g_RenderDebugStats.DrawnObjects,
                    (int)g_RenderDebugStats.DrawCalls,
                    (int)g_RenderDebugStats.CulledObjects,
                    (int)g_RenderDebugStats.ViewX, (int)g_RenderDebugStats.ViewY,
                    (int)g_RenderDebugStats.ViewW, (int)g_RenderDebugStats.ViewH,
                    (int)g_RenderDebugStats.ShroudedCells,
                    (TheTactical != nullptr) ? (int)TheTactical->VisibleCellCount : -1,
                    (ObjectClass::VisibleObjects != nullptr)
                        ? (int)ObjectClass::VisibleObjects->Count : -1,
                    (unsigned)g_RenderDebugStats.FrameCount,
                    (unsigned)g_RenderDebugStats.UpdateTicks);

            fflush(stderr);
        }
    }

    s_FrameEndTime = now;
}

#endif // !OPENYR_TIMESTEP_UNIT_TEST
