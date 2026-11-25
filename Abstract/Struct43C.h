#pragma once

#include "../Core/Definitions.h"
#include "../Math/Timer.h"

// ============================================================================
// Struct43C —— 通用的“生产 / 门 / 循环”计时状态机（asm 0x4A50E0..0x4A5388）
//
//  原版布局（sizeof 0x1C，asm 8937）：
//      +0x00  double       RateDouble      启动速率（秒换算成 900 倍）
//      +0x08  TimerStruct  CycleTimer      { TimeStart, LastCheck, TimeLeft }
//      +0x14  int32        RateInt2        本轮总量（= floor(RateDouble)）
//      +0x18  uint8        field_18        状态高位：1 = 本轮已启动
//      +0x19  uint8        field_19        状态低位
//      +0x1A  uint8        field_1A        未使用
//      +0x1B  uint8        field_1B        未使用
//
//  两个标志组成四态：
//      (18,19) = (0,0) 00 —— 空闲
//                (0,1) 01 —— 已装载但未开始（Set01 进入）
//                (1,1) 11 —— 计时中（StartTimer11 进入）
//                (1,0) 10 —— 已完成（Update 在 TimeLeft 归零后翻转出来）
//
//  典型用法：StartTimer11(sec) 起跑，Update() 每帧推进，IsTimerFinished()
//  判完成，SetValuesDone() 把 11 → 01、10 → 00 收尾。
// ============================================================================
#pragma pack(push, 4)
struct Struct43C {
    Struct43C() noexcept;

    // 状态切换（asm: Struct43C_Set00 / Struct43C_Set01）
    void Set00();
    void Set01();

    // 状态查询（asm: Struct43C_IsSet01 / IsSet10 / IsSet11 / 0x4A51D0）
    bool IsSet01() const;
    bool IsSet10() const;
    bool IsSet11() const;
    bool IsSet00() const;

    // asm: Struct43C_SetValuesDone（0x4A5363）—— 11 → 01，10 → 00，其余不动。
    void SetValuesDone();

    // asm: Struct43C_StartTimer11（0x4A51F0）—— 已处于 01 时是空操作。
    //        RateDouble = seconds * 900
    //        CycleTimer = { Frame, <ignored>, floor(seconds*900) }
    //        RateInt2   = floor(seconds*900)
    //        (18,19)    = (1,1)
    void StartTimer11(double seconds);

    // asm: DoorClass::Close_Door（0x4A523C，同一结构上的另一路起跑）——
    //   与 StartTimer11 的唯一区别是把状态低位清 0，得到 (1,0)=10 态；
    //   进入条件是 (18,19) 属于 {11, 01}（与 StartTimer11 恰好互补）。
    void StartTimer10(double seconds);

    // asm: Struct43C_Update（0x4A52CC）—— 按已过帧数扣减 TimeLeft，
    //   随后翻转状态低位。只在 field_18 != 0 时动作。
    void Update();

    // asm: Struct43C_IsTimerFinished（0x4A5146）—— 空闲 / 总量为 0 /
    //   已花比例达到 1.0 时判完成。
    bool IsTimerFinished() const;

    // asm: Struct43C_RatioSpent（0x4A531C）—— 已花比例；空闲或总量为 0
    //   时返回 1.0。
    double RatioSpent() const;

    double      RateDouble;     // +0x00
    TimerStruct CycleTimer;     // +0x08
    int32       RateInt2;       // +0x14
    uint8       field_18;       // +0x18
    uint8       field_19;       // +0x19
    uint8       field_1A;       // +0x1A
    uint8       field_1B;       // +0x1B
};
#pragma pack(pop)

static_assert(sizeof(Struct43C) == 0x1C, "Struct43C 必须与 asm 的 sizeof 0x1C 一致");

// 全局帧钟（asm 的 Frame 符号），同时供本结构与调用方取用。
int32 Struct43C_CurrentFrame();
