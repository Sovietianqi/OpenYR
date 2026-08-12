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
// TimerStruct - simple countdown timer
// ============================================================================

class TimerStruct {
public:
    int32 Value;

    TimerStruct();
    explicit TimerStruct(int32 v);

    void Start(int32 duration);
    void Stop();
    bool HasTimeLeft() const;
    bool Expired() const;
    void Update();
};