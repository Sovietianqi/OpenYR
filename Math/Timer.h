#pragma once

#include "../Core/Definitions.h"

#include <cstdint>

// ============================================================================
// FrameTimer - global frame clock source
// ============================================================================

struct FrameTimer {
    static int32 CurrentFrame;
    static int32 GetTime() { return CurrentFrame; }
};

// ============================================================================
// SystemTimer - Platform-specific high-resolution wall-clock timer
// ============================================================================

struct SystemTimer {
    // Returns current time in milliseconds
    static int32 GetTime();

    // Returns current time in microseconds
    static uint64 GetTimeMicroseconds();

    // Returns current time in nanoseconds
    static uint64 GetTimeNanoseconds();
};

// ============================================================================
// CDTimerClass - frame-based countdown timer
// ============================================================================

class CDTimerClass {
public:
    int32 StartTime;
    int32 TimeLeft;

    CDTimerClass();
    explicit CDTimerClass(int32 duration);

    void Start(int32 duration);
    void Stop();
    bool HasTimeLeft() const;
    bool Expired() const;
    int32 GetTimeLeft() const;
    void Update();
    bool Completed() const;
    bool InProgress() const;
    bool IsTicking() const;
};

// ============================================================================
// RateTimer - rate-based accumulator timer
// ============================================================================

class RateTimer {
public:
    int32 Accumulator;
    int32 Rate;

    RateTimer();
    void SetRate(int32 rate);
    bool Update(int32 delta);
    void Reset();
};

// ============================================================================
// TimerStruct - asm 的绝对时刻计时器（12 字节）
//
//   +0 TimeStart  —— 起跑时刻（毫秒，Get_System_Tick_Count）
//   +4 LastCheck  —— 最近一次查询时刻
//   +8 TimeLeft   —— 总时长（毫秒）
//
//   sub_4F1B20(TimerStruct*) 的算法：
//       if (TimeStart == -1)      return TimeLeft;   // 未起跑
//       elapsed = now - TimeStart;
//       if (elapsed >= TimeLeft)  return 0;
//       return TimeLeft - elapsed;
//
//   与 CDTimerClass 的区别：本结构比较**墙钟毫秒**，不依赖逻辑帧推进，
//   可以在任意渲染帧查询（话泡计时就是这种用法）。
// ============================================================================

struct TimerStruct {
    int32 TimeStart;
    int32 LastCheck;
    int32 TimeLeft;

    TimerStruct();
    explicit TimerStruct(int32 v);

    // 以当前墙钟时刻起跑 / 停止。
    void Start(int32 duration);
    void Start(int32 duration, int32 now);
    void Stop();
    void Stop(int32 now);

    // 剩余毫秒（<=0 表示已到期）。
    int32 GetTimeLeft() const;
    int32 GetTimeLeft(int32 now) const;

    bool HasTimeLeft() const;
    bool Expired() const;
};