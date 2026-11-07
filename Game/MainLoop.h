#pragma once

#include "Core/Definitions.h"

// ═══════════════════════════════════════════════════════════════════════════
// MainLoop.h — main game loop functions
// ═══════════════════════════════════════════════════════════════════════════

// ── Main entry into the game loop (called from WinMain after init) ────────

void Main_Game();

// ── Per-frame update (called once per iteration of the main loop) ─────────

void Main_Loop();

// ── Modal-wait heartbeat ──────────────────────────────────────────────────
//
// asm: Call_Back (0x48D092).  原版在"等影片/等会话/等玩家"这类模态等待里
// 反复调用它：跑一遍音频服务、轮询影片播放器、在多人会话里抽干网络事件
// 队列（把进度包喂给进度屏），Session==4 时按秒向对端发心跳。
void Call_Back();

// ── Sub-step functions ────────────────────────────────────────────────────

void Update_GameSpeed();
void ProcessWindowsMessages();
void Update_Input();
void Update_Network();
void Update_Triggers();
void Update_AI();
void Update_Combat();
void Update_Locomotion();
void Update_Animations();
void Update_Particles();
void Update_SuperWeapons();
void Update_SpecialEffects();
void Update_Radar();
void Update_Sidebar();
void Update_Display();
void Render_Frame();
void Update_Audio();

// ── Frame timing ──────────────────────────────────────────────────────────

void Record_FrameStats();

// ═══════════════════════════════════════════════════════════════════════════
// FrameTimestep —— 定步长驱动核心（实现在 MainLoop.cpp，可被
// tools/test_timestep.cpp 单独链接做单元测试，不复制第二份实现）
//
// 原版时序（asm Main_Loop @0x755698 + 帧尾等待 sub_55E160 @0x757324）：
//   * 每调用一次 Main_Loop 只跑**一个**逻辑帧，帧尾 `inc edx / mov Frame,edx`
//     （0x756888）无条件执行，循环体内 `call LogicClass_Update`（0x756687）
//     也只有一处 —— 一轮一帧；
//   * 全函数没有任何"按经过时间补多帧"的循环，也没有跳帧/丢帧分支；
//   * 时间只用于两件事：帧后自旋等预算（sub_55E160 里
//     `Get_System_Tick_Count - spSpeedTimer` 与 dword_887350 比，
//     不足就 `Sleep(remaining)`）和 FPS 统计（dword_A8B560/dword_A8B564）。
//
// 所以累加器在实时模式下只决定"这一轮是否还该等预算"，永远最多推进一帧
// （maxTicks = 1），积压的时间直接丢弃 —— 这正是原版"机器慢就慢，不追帧"。
// ═══════════════════════════════════════════════════════════════════════════
namespace FrameTimestep
{
    struct IterationPlan
    {
        int       Ticks;    // 本轮要推进的逻辑帧数（恒 ≥ 1）
        long long SleepUs;  // 推进前需要睡掉的微秒数（0 = 不睡）
    };

    // 单调时钟（微秒）。绝不用 wall clock：原版的 timeGetTime 是开机毫秒数。
    long long MonotonicMicros();

    // 步长累加器 + 本轮计划。
    //   pAccumUs      : 跨轮次的余量（微秒）。为 nullptr 时按 0 处理。
    //   elapsedUs     : 距上一轮的经过时间；<0（时钟回退）按 0 计，不产生负余量。
    //   targetFrameMs : 目标帧间隔（Game::TargetFrameTime），0 时退化为每轮一帧。
    //   fixedTicks    : >0 → 定步长模式，直接返回该帧数，**完全不看 elapsedUs**；
    //                   0  → 实时模式。
    //   maxTicks      : 单轮补帧上限；超出上限的整步时间被丢弃（防卡顿后雪崩）。
    IterationPlan PlanIteration(long long* pAccumUs,
                                long long  elapsedUs,
                                unsigned   targetFrameMs,
                                int        fixedTicks,
                                int        maxTicks);
}