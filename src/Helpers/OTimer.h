#pragma once

// ============================================================================
// OTimer.h - High-precision timer utility
//
//  Provides a thin wrapper around the platform's highest-resolution clock
//  for benchmarking, animation timing and debug profiling.  The original
//  Yuri's Revenge binary uses QueryPerformanceCounter on Windows; the
//  standalone build abstracts this behind a portable interface.
//
//  Key features:
//    * OTimerClass        - scoped stopwatch with start/stop/elapsed
//    * OTimerAutoScope    - RAII marker that records elapsed on destruction
//    * OProfileBlock      - named profile block with accumulated stats
//    * OProfileRegistry   - global registry of named profile blocks
// ============================================================================

#include <Core/Definitions.h>
#include <Core/Macros.h>

#include <cstdint>

// ============================================================================
// Platform abstraction - returns ticks from a monotonic high-resolution clock.
// Implementations live in the platform layer; the values are only meaningful
// when divided by OTimerClass::TicksPerSecond().
// ============================================================================

namespace OTimerDetail
{
    // Returns the current tick count from the high-resolution counter.
    uint64 QueryTicks();

    // Returns the tick frequency (ticks per second).
    uint64 QueryFrequency();
}

// ============================================================================
// OTimerClass - high-precision stopwatch
// ============================================================================

class OTimerClass
{
public:
    // ── Construction ────────────────────────────────────────────────────
    OTimerClass() noexcept
        : StartTick(0)
        , StopTick(0)
        , Accumulated(0)
        , Running(false)
    {
    }

    // Construct and immediately start timing.
    explicit OTimerClass(bool startNow) noexcept
        : StartTick(0)
        , StopTick(0)
        , Accumulated(0)
        , Running(false)
    {
        if (startNow)
            Start();
    }

    // ── Control ─────────────────────────────────────────────────────────

    // Begin (or resume) timing.  Accumulated time is preserved across
    // stop/start cycles so the timer can be paused and resumed.
    void Start() noexcept
    {
        if (Running)
            return;
        StartTick = OTimerDetail::QueryTicks();
        Running = true;
    }

    // Pause timing.  Elapsed time since the last Start() is folded into
    // Accumulated.
    void Stop() noexcept
    {
        if (!Running)
            return;
        StopTick = OTimerDetail::QueryTicks();
        Accumulated += (StopTick - StartTick);
        Running = false;
    }

    // Reset all counters and stop the timer.
    void Reset() noexcept
    {
        StartTick = 0;
        StopTick = 0;
        Accumulated = 0;
        Running = false;
    }

    // ── Queries ─────────────────────────────────────────────────────────

    // Returns the frequency used by the underlying counter (ticks / second).
    static uint64 TicksPerSecond() noexcept
    {
        return OTimerDetail::QueryFrequency();
    }

    // Total elapsed ticks, including the current running segment.
    uint64 GetElapsedTicks() const noexcept
    {
        if (Running)
            return Accumulated + (OTimerDetail::QueryTicks() - StartTick);
        return Accumulated;
    }

    // Elapsed time in seconds as a double-precision floating point value.
    double GetElapsedSeconds() const noexcept
    {
        const uint64 freq = TicksPerSecond();
        if (freq == 0)
            return 0.0;
        return static_cast<double>(GetElapsedTicks()) / static_cast<double>(freq);
    }

    // Elapsed time in milliseconds.
    double GetElapsedMilliseconds() const noexcept
    {
        return GetElapsedSeconds() * 1000.0;
    }

    // Elapsed time in microseconds.
    double GetElapsedMicroseconds() const noexcept
    {
        return GetElapsedSeconds() * 1000000.0;
    }

    // True if the timer is currently running.
    bool IsRunning() const noexcept
    {
        return Running;
    }

    // ── Convenience: measure a single call ──────────────────────────────

    // Measure the time taken by a callable and return the elapsed
    // microseconds.  The timer is reset before the call and stopped after.
    template <typename FnT>
    double MeasureMicroseconds(FnT&& fn) noexcept
    {
        Reset();
        Start();
        fn();
        Stop();
        return GetElapsedMicroseconds();
    }

    // Measure the time taken by a callable and return the elapsed
    // milliseconds.
    template <typename FnT>
    double MeasureMilliseconds(FnT&& fn) noexcept
    {
        Reset();
        Start();
        fn();
        Stop();
        return GetElapsedMilliseconds();
    }

private:
    uint64 StartTick;    // Tick captured by the most recent Start().
    uint64 StopTick;     // Tick captured by the most recent Stop().
    uint64 Accumulated;  // Total ticks from completed segments.
    bool   Running;      // True between Start() and Stop().
};

// ============================================================================
// OTimerAutoScope - RAII helper that starts a timer on construction and
// stops it on destruction.  Useful for scoped benchmarking.
// ============================================================================

class OTimerAutoScope
{
public:
    explicit OTimerAutoScope(OTimerClass& timer) noexcept
        : Timer(timer)
    {
        Timer.Start();
    }

    ~OTimerAutoScope() noexcept
    {
        Timer.Stop();
    }

    OTimerClass& GetTimer() noexcept { return Timer; }
    const OTimerClass& GetTimer() const noexcept { return Timer; }

private:
    OTimerClass& Timer;
};

// ============================================================================
// OProfileBlock - accumulates statistics across multiple timed segments.
// Each Start()/Stop() pair adds to the block's total and updates the
// min/max/average counters.
// ============================================================================

class OProfileBlock
{
public:
    explicit OProfileBlock(const char* pName) noexcept
        : Name(pName)
        , TotalTicks(0)
        , MinTicks(UINT64_MAX)
        , MaxTicks(0)
        , CallCount(0)
        , Running(false)
        , StartTick(0)
    {
    }

    void Start() noexcept
    {
        if (Running)
            return;
        StartTick = OTimerDetail::QueryTicks();
        Running = true;
    }

    void Stop() noexcept
    {
        if (!Running)
            return;
        const uint64 elapsed = OTimerDetail::QueryTicks() - StartTick;
        TotalTicks += elapsed;
        if (elapsed < MinTicks) MinTicks = elapsed;
        if (elapsed > MaxTicks) MaxTicks = elapsed;
        ++CallCount;
        Running = false;
    }

    void Reset() noexcept
    {
        TotalTicks = 0;
        MinTicks = UINT64_MAX;
        MaxTicks = 0;
        CallCount = 0;
        Running = false;
    }

    const char*  GetName() const noexcept { return Name; }
    uint64       GetTotalTicks() const noexcept { return TotalTicks; }
    uint64       GetMinTicks() const noexcept { return MinTicks; }
    uint64       GetMaxTicks() const noexcept { return MaxTicks; }
    uint32       GetCallCount() const noexcept { return CallCount; }

    double GetAverageMicroseconds() const noexcept
    {
        if (CallCount == 0) return 0.0;
        const uint64 freq = OTimerClass::TicksPerSecond();
        if (freq == 0) return 0.0;
        return (static_cast<double>(TotalTicks) / static_cast<double>(CallCount))
               / static_cast<double>(freq) * 1000000.0;
    }

    double GetTotalMilliseconds() const noexcept
    {
        const uint64 freq = OTimerClass::TicksPerSecond();
        if (freq == 0) return 0.0;
        return static_cast<double>(TotalTicks) / static_cast<double>(freq) * 1000.0;
    }

private:
    const char* Name;
    uint64      TotalTicks;
    uint64      MinTicks;
    uint64      MaxTicks;
    uint32      CallCount;
    bool        Running;
    uint64      StartTick;
};

// ============================================================================
// OProfileScope - RAII wrapper around an OProfileBlock.
// ============================================================================

class OProfileScope
{
public:
    explicit OProfileScope(OProfileBlock& block) noexcept
        : Block(block)
    {
        Block.Start();
    }

    ~OProfileScope() noexcept
    {
        Block.Stop();
    }

private:
    OProfileBlock& Block;
};
