#pragma once

#include <Core/Definitions.h>

// ============================================================================
// ChainDebug — 逐帧调用链探针
//
//  用途：证明「对象每帧的更新链」在运行时确实是连通的，而不只是"有实现"。
//  原版链路（asm 证据）：
//
//    LogicClass_Update  vec_Logics 循环            L751847-751856
//      └─ call [obj+0x5C]  ──┬─ TechnoClass_Update  1561027
//                            │    ├─ ... 前半段
//                            │    ├─ MissionClass_Update      1561947
//                            │    │    ├─ ObjectClass_Update   911401
//                            │    │    └─ off_5B34E8 分派 → Mission_*   911430
//                            │    └─ ... 后半段
//                            └─ FootClass::AI             481281
//                                 └─ ILocomotion::Process  481617
//
//  这些计数器本身不改变任何游戏语义，只在每帧各链路口自增一次；
//  排查"单位不动 / 任务不派发"时用 ChainDebug::Dump() 一眼看出断在哪一环：
//
//    TechnoUpdateCalls   > 0  但 MissionUpdateCalls   == 0  → #3 断了
//    TechnoUpdateCalls   == 0                              → #2 断了（vec_Logics 没驱动到）
//    FootUpdateCalls     > 0  但 LocomotionProcessCalls== 0 → #7 断了（行走逻辑没推进）
//    MissionUpdateCalls  > 0  但 MissionDispatchCalls == 0  → 任务计时器/血量门没过
// ============================================================================
namespace ChainDebug
{
    // 各链路口被调用次数（每帧每对象各 +1）。
    extern int32 TechnoUpdateCalls;      // 进入 TechnoClass::Update
    extern int32 FootUpdateCalls;        // 进入 FootClass::Update
    extern int32 MissionUpdateCalls;     // 进入 MissionClass::Update
    extern int32 MissionDispatchCalls;   // 进入 MissionClass::AI_Mission（32 路分派）
    extern int32 LocomotionProcessCalls; // ILocomotion::Process 实际被调用

    // 全部清零（测试前调用）。
    void Reset();

    // 把当前计数打到日志，并按上面的判据给出断点结论。
    // pWhere 用于标注调用点（例如 "frame 120"）。
    void Dump(const char* pWhere);

    // 断言式检查：链路上每一环都必须 > 0，否则返回 false 并打印缺失项。
    bool CheckAllLinks(const char* pWhere);

    // 每帧调用（Main_Loop 帧尾）。仅在环境变量 OPENYR_CHAIN_DEBUG=1 时生效：
    // 每 300 帧打印一次各链路口计数并清零，用于在实际对局中确认
    // 「单位被 Unlimbo 后，每帧确实走到 MissionClass::Update → Mission_* 分派」。
    // 未设置该环境变量时是一次数组读 + 分支，开销可忽略。
    void Tick();
}
