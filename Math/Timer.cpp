// =============================================================================
// Timer.cpp - Timer implementations
//
// Standalone engine reconstruction of the game timer system.
// - FrameTimer: global frame clock source, advances once per game tick.
// - SystemTimer: platform-specific high-resolution wall-clock timer.
// - CDTimerClass: frame-based countdown timer used for mission control,
//   weapon recharge, cloak duration, and other gameplay timers.
// - RateTimer: accumulator-based timer for rate-limited operations.
// - TimerStruct: simple countdown timer for short-duration events.
// =============================================================================

#include "Math/Timer.h"

#include <sys/time.h>
#include <ctime>
#include <cstdint>

// =============================================================================
// FrameTimer static member definitions
// =============================================================================
int32 FrameTimer::CurrentFrame = 0;

// =============================================================================
// SystemTimer - Platform-specific high-resolution timer
//
// On Linux, SystemTimer uses gettimeofday() and clock_gettime() for
// high-resolution timing. On Windows, it uses GetTickCount() and the
// QueryPerformanceCounter API.
// =============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>

    // Windows millisecond timer using GetTickCount.
    int32 SystemTimer::GetTime()
    {
        return static_cast<int32>(GetTickCount());
    }

    // Windows microsecond timer using QueryPerformanceCounter.
    uint64 SystemTimer::GetTimeMicroseconds()
    {
        LARGE_INTEGER freq;
        LARGE_INTEGER counter;
        if (QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&counter)) {
            return static_cast<uint64>(counter.QuadPart * 1000000ULL / freq.QuadPart);
        }
        // Fallback to millisecond precision.
        return static_cast<uint64>(GetTickCount()) * 1000ULL;
    }

    // Windows nanosecond timer using QueryPerformanceCounter.
    uint64 SystemTimer::GetTimeNanoseconds()
    {
        LARGE_INTEGER freq;
        LARGE_INTEGER counter;
        if (QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&counter)) {
            return static_cast<uint64>(counter.QuadPart * 1000000000ULL / freq.QuadPart);
        }
        // Fallback to millisecond precision.
        return static_cast<uint64>(GetTickCount()) * 1000000ULL;
    }

#else
    // Linux / POSIX implementation using gettimeofday and clock_gettime.

    #include <unistd.h>

    // Millisecond resolution using gettimeofday.
    int32 SystemTimer::GetTime()
    {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        // Return milliseconds since epoch (truncated to int32).
        return static_cast<int32>(
            static_cast<uint64>(tv.tv_sec) * 1000ULL +
            static_cast<uint64>(tv.tv_usec) / 1000ULL
        );
    }

    // Microsecond resolution using gettimeofday.
    uint64 SystemTimer::GetTimeMicroseconds()
    {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        return static_cast<uint64>(tv.tv_sec) * 1000000ULL +
               static_cast<uint64>(tv.tv_usec);
    }

    // Nanosecond resolution using clock_gettime with monotonic clock.
    // The monotonic clock is not affected by system time changes,
    // making it ideal for measuring elapsed time.
    uint64 SystemTimer::GetTimeNanoseconds()
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64>(ts.tv_sec) * 1000000000ULL +
               static_cast<uint64>(ts.tv_nsec);
    }

#endif

// =============================================================================
// CDTimerClass - Frame-based countdown timer
//
// This timer counts down in game frames. It is the most commonly used timer
// in the game engine, powering weapon recharge delays, cloak fade durations,
// building production timers, and super weapon recharge cycles.
//
// Usage pattern:
//   CDTimerClass timer;
//   timer.Start(150);      // 150 frames (5 seconds at 30 FPS)
//   while (timer.HasTimeLeft()) {
//       timer.Update();     // Called once per frame
//   }
// =============================================================================

CDTimerClass::CDTimerClass()
    : StartTime(0)
    , TimeLeft(0)
{
}

CDTimerClass::CDTimerClass(int32 duration)
    : StartTime(0)
    , TimeLeft(duration)
{
}

// Start the timer with the specified duration in frames.
void CDTimerClass::Start(int32 duration)
{
    if (duration < 0) duration = 0;
    StartTime = FrameTimer::GetTime();
    TimeLeft = duration;
}

// Stop the timer and reset all state.
void CDTimerClass::Stop()
{
    TimeLeft = 0;
    StartTime = 0;
}

// Check if the timer still has time remaining.
bool CDTimerClass::HasTimeLeft() const
{
    return TimeLeft > 0;
}

// Check if the timer has expired (no time remaining).
bool CDTimerClass::Expired() const
{
    return TimeLeft <= 0;
}

// Return the number of frames remaining.
int32 CDTimerClass::GetTimeLeft() const
{
    return TimeLeft;
}

// Advance the timer by one frame. Called once per game tick.
void CDTimerClass::Update()
{
    if (TimeLeft > 0) {
        --TimeLeft;
        // When the timer reaches zero, it is considered completed.
        // The StartTime is preserved for elapsed-time calculations.
    }
}

// Check if the timer has completed its countdown.
bool CDTimerClass::Completed() const
{
    return TimeLeft <= 0;
}

// Check if the timer is currently counting down.
bool CDTimerClass::InProgress() const
{
    return TimeLeft > 0;
}

// Check if the timer is actively ticking (same as InProgress).
bool CDTimerClass::IsTicking() const
{
    return TimeLeft > 0;
}

// =============================================================================
// RateTimer - Rate-based accumulator timer
//
// This timer accumulates delta values until a threshold (Rate) is reached.
// It is used for operations that need to fire at a specific rate regardless
// of frame timing, such as:
// - Damage over time effects (e.g. radiation, fire)
// - Resource generation (e.g. power plant output)
// - AI decision-making frequency throttling
//
// Usage pattern:
//   RateTimer timer;
//   timer.SetRate(100);        // Fire every 100 accumulated units
//   if (timer.Update(delta)) {
//       // Perform the rate-limited action
//   }
// =============================================================================

RateTimer::RateTimer()
    : Accumulator(0)
    , Rate(0)
{
}

// Set the rate threshold. When Accumulator >= Rate, the timer fires.
void RateTimer::SetRate(int32 rate)
{
    if (rate < 0) rate = 0;
    Rate = rate;
}

// Accumulate delta and check if the rate threshold has been reached.
// Returns true if the threshold was reached, false otherwise.
// When the threshold is reached, the accumulator is reduced by Rate,
// allowing the timer to fire again after another full accumulation.
bool RateTimer::Update(int32 delta)
{
    if (Rate <= 0) return false;

    Accumulator += delta;
    if (Accumulator >= Rate) {
        Accumulator -= Rate;
        return true;
    }
    return false;
}

// Reset the accumulator to zero.
void RateTimer::Reset()
{
    Accumulator = 0;
}

// =============================================================================
// TimerStruct - Simple countdown timer
//
// A lightweight countdown timer that decrements once per frame.
// Used for short-duration events such as:
// - Animation frame timing
// - Sound effect duration tracking
// - Temporary state transitions
// =============================================================================

TimerStruct::TimerStruct()
    : Value(0)
{
}

TimerStruct::TimerStruct(int32 v)
    : Value(v)
{
}

// Start the timer with the specified duration.
void TimerStruct::Start(int32 duration)
{
    if (duration < 0) duration = 0;
    Value = duration;
}

// Stop the timer by resetting to zero.
void TimerStruct::Stop()
{
    Value = 0;
}

// Check if time remains on the timer.
bool TimerStruct::HasTimeLeft() const
{
    return Value > 0;
}

// Check if the timer has expired.
bool TimerStruct::Expired() const
{
    return Value <= 0;
}

// Advance the timer by one frame.
void TimerStruct::Update()
{
    if (Value > 0) {
        --Value;
    }
}

// =============================================================================
// Timer system overview
//
// The timer hierarchy in the YR engine works as follows:
//
// 1. FrameTimer is the master clock. It increments CurrentFrame by 1 each
//    game tick. All frame-based timers reference this value.
//
// 2. SystemTimer provides wall-clock time for real-time operations like
//    network synchronization, replay recording, and UI animations that
//    must run smoothly regardless of game speed.
//
// 3. CDTimerClass is the workhorse gameplay timer. It counts down in
//    frames and is used for virtually all gameplay delays. The StartTime
//    field records when the timer was started, allowing calculation of
//    elapsed time even after the timer has expired.
//
// 4. RateTimer handles variable-rate events. Unlike CDTimerClass which
//    ticks at a fixed frame rate, RateTimer accumulates arbitrary delta
//    values, making it suitable for effects that scale with game speed
//    or damage that depends on the source's rate of fire.
//
// 5. TimerStruct is the simplest timer, used for cases where only a
//    countdown value is needed without start-time tracking.
//
// All timers are designed to be safe for use with -fno-rtti and
// -fno-exceptions. They do not allocate memory or use virtual functions.
// =============================================================================

// =============================================================================
// Timer utility constants
//
// These constants define the standard frame rates and time conversions used
// throughout the game engine. The game runs at 15 FPS internally, but the
// rendering loop may run faster. All gameplay timers operate on game frames,
// not real-time milliseconds.
// =============================================================================

namespace TimerConstants {
    // The standard game frame rate is 15 frames per second.
    static const int32 GAME_FPS = 15;

    // Standard frame duration in milliseconds.
    static const int32 FRAME_DURATION_MS = 1000 / GAME_FPS;

    // Convert seconds to game frames.
    static inline int32 SecondsToFrames(int32 seconds) {
        return seconds * GAME_FPS;
    }

    // Convert game frames to seconds (integer division).
    static inline int32 FramesToSeconds(int32 frames) {
        return frames / GAME_FPS;
    }

    // Convert minutes to game frames.
    static inline int32 MinutesToFrames(int32 minutes) {
        return minutes * 60 * GAME_FPS;
    }

    // Check if a frame count represents an even second boundary.
    static inline bool IsSecondBoundary(int32 frame) {
        return (frame % GAME_FPS) == 0;
    }
}

// =============================================================================
// Frame timing details
//
// The game loop runs at a fixed 15 FPS for gameplay logic. Each game frame:
//
// 1. FrameTimer::CurrentFrame is incremented by 1.
// 2. All active CDTimerClass instances call Update() to decrement their
//    TimeLeft counter.
// 3. TimerStruct instances are similarly updated.
// 4. RateTimer instances accumulate their delta values.
//
// The SystemTimer runs independently of the game frame rate. It provides
// real-time wall-clock measurements for:
// - Network packet timing and latency measurement
// - Replay recording timestamps
// - UI animation smoothing (cursor, menu transitions)
// - Performance profiling and frame time measurement
//
// The separation between frame-based and wall-clock timing ensures that
// gameplay remains deterministic across different hardware speeds, while
// UI and network operations remain responsive.
//
// Common timer durations in the game:
// - Weapon ROF:           3-60 frames (0.2s - 4s)
// - Building production:  300-2400 frames (20s - 160s)
// - Super weapon recharge: 13500 frames (15 minutes)
// - Cloak fade:           30-60 frames (2s - 4s)
// - Iron Curtain duration: 750 frames (50 seconds)
// - Force Shield duration: 500 frames (33 seconds)
// - Radiation decay:      600-900 frames (40-60 seconds)
// - Fire spread:          15-30 frames (1-2 seconds)
// =============================================================================

// =============================================================================
// Platform-specific notes
//
// Linux:
// - gettimeofday() provides microsecond resolution but is subject to NTP
//   adjustments. Used for GetTime() and GetTimeMicroseconds().
// - clock_gettime(CLOCK_MONOTONIC) provides nanosecond resolution and is
//   not affected by system time changes. Used for GetTimeNanoseconds().
//
// Windows:
// - GetTickCount() provides millisecond resolution with ~10-16ms accuracy.
//   Used for GetTime() and as a fallback.
// - QueryPerformanceCounter provides high-resolution timing (typically
//   sub-microsecond). Used for GetTimeMicroseconds() and
//   GetTimeNanoseconds().
//
// The timer system does not use exceptions or RTTI. All platform-specific
// code is isolated within #ifdef blocks to ensure portability.
// =============================================================================
