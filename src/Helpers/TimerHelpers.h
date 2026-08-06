#pragma once

// ============================================================================
// TimerHelpers.h - Timer helper functions
//
//  Provides utility functions that wrap the game's frame-based and
//  wall-clock timers.  These helpers are used throughout the engine for
//  cooldowns, rate-limiting, animation stepping and debug throttling.
//
//  Key helpers:
//    * Frame helpers  - work with the global frame counter
//    * Millisecond helpers - convert between frames, ms and seconds
//    * Cooldown helpers - simple frame-based cooldown state machines
//    * Throttle helpers - rate-limited execution of debug/log output
// ============================================================================

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Math/Timer.h>

#include <cstdint>

// ============================================================================
// Frame helpers - the global frame counter is the engine's primary clock.
// ============================================================================

namespace TimerHelpers
{
    // ── Frame queries ───────────────────────────────────────────────────

    // Returns the current global frame number.
    inline int32 GetFrame() noexcept
    {
        return FrameTimer::GetTime();
    }

    // Returns the number of frames elapsed since the given frame.
    inline int32 FramesSince(int32 frame) noexcept
    {
        return FrameTimer::GetTime() - frame;
    }

    // True if at least `frames` frames have elapsed since `frame`.
    inline bool FramesElapsed(int32 frame, int32 frames) noexcept
    {
        return FramesSince(frame) >= frames;
    }

    // Returns the frame that will occur after `frames` frames from now.
    inline int32 FrameAfter(int32 frames) noexcept
    {
        return FrameTimer::GetTime() + frames;
    }

    // ── Frame / time conversion ─────────────────────────────────────────
    //
    //  The original binary assumes 15 frames per second on the slowest
    //  speed and 60 FPS on the fastest.  The conversion helpers below use
    //  the nominal 15 FPS baseline so that one second equals 15 frames;
    //  callers that need the real-time conversion should use the
    //  millisecond variants.

    constexpr int32 FramesPerSecond = 15;

    // Convert milliseconds to the nearest frame count.
    inline int32 MillisecondsToFrames(int32 ms) noexcept
    {
        return (ms * FramesPerSecond + 500) / 1000;
    }

    // Convert frames to milliseconds.
    inline int32 FramesToMilliseconds(int32 frames) noexcept
    {
        return (frames * 1000) / FramesPerSecond;
    }

    // Convert seconds to frames.
    inline int32 SecondsToFrames(int32 seconds) noexcept
    {
        return seconds * FramesPerSecond;
    }

    // Convert frames to seconds (integer, truncated).
    inline int32 FramesToSeconds(int32 frames) noexcept
    {
        return frames / FramesPerSecond;
    }

    // ── Cooldown helpers ────────────────────────────────────────────────

    // A simple frame-based cooldown.  Call Start() to arm it with a
    // duration, then IsReady() to check if the cooldown has expired.
    struct FrameCooldown
    {
        int32 StartFrame;
        int32 Duration;

        FrameCooldown() noexcept : StartFrame(0), Duration(0) {}

        void Start(int32 durationFrames) noexcept
        {
            StartFrame = FrameTimer::GetTime();
            Duration = durationFrames;
        }

        void StartMs(int32 durationMs) noexcept
        {
            Start(MillisecondsToFrames(durationMs));
        }

        bool IsReady() const noexcept
        {
            if (Duration <= 0)
                return true;
            return FramesSince(StartFrame) >= Duration;
        }

        int32 FramesRemaining() const noexcept
        {
            int32 elapsed = FramesSince(StartFrame);
            if (elapsed >= Duration)
                return 0;
            return Duration - elapsed;
        }

        void Reset() noexcept
        {
            StartFrame = 0;
            Duration = 0;
        }
    };

    // ── Throttle helpers ────────────────────────────────────────────────
    //
    //  Throttle allows an action to run at most once every N frames.
    //  This is used by debug logging and infrequent AI evaluations.

    struct FrameThrottle
    {
        int32 LastFrame;
        int32 Interval;

        FrameThrottle() noexcept : LastFrame(0), Interval(0) {}

        explicit FrameThrottle(int32 intervalFrames) noexcept
            : LastFrame(0), Interval(intervalFrames) {}

        // Returns true if the throttle interval has elapsed since the last
        // successful call.  When true is returned the internal counter is
        // advanced so subsequent calls return false until the interval
        // elapses again.
        bool TryFire() noexcept
        {
            if (Interval <= 0)
                return true;
            int32 now = FrameTimer::GetTime();
            if (now - LastFrame >= Interval)
            {
                LastFrame = now;
                return true;
            }
            return false;
        }

        void SetInterval(int32 intervalFrames) noexcept
        {
            Interval = intervalFrames;
        }

        void Reset() noexcept
        {
            LastFrame = 0;
        }
    };

    // ── System-time helpers ─────────────────────────────────────────────

    // Returns the current system time in milliseconds.
    inline int32 GetSystemMs() noexcept
    {
        return SystemTimer::GetTime();
    }

    // Returns the current system time in microseconds.
    inline uint64 GetSystemUs() noexcept
    {
        return SystemTimer::GetTimeMicroseconds();
    }

    // ── Duration formatting ─────────────────────────────────────────────

    // Format a frame count as "Mm Ss" into the supplied buffer.
    // Returns the number of characters written (excluding the terminator).
    inline int32 FormatFramesAsTime(int32 frames, char* pBuffer, int32 bufferSize) noexcept
    {
        if (!pBuffer || bufferSize <= 0)
            return 0;
        int32 totalSeconds = FramesToSeconds(frames);
        int32 minutes = totalSeconds / 60;
        int32 seconds = totalSeconds % 60;
        // Simple formatting without sprintf to stay exception-free.
        int32 pos = 0;
        if (minutes >= 10)
        {
            pBuffer[pos++] = static_cast<char>('0' + (minutes / 10));
            pBuffer[pos++] = static_cast<char>('0' + (minutes % 10));
        }
        else if (minutes > 0)
        {
            pBuffer[pos++] = static_cast<char>('0' + minutes);
        }
        if (minutes > 0 && pos < bufferSize - 1)
            pBuffer[pos++] = 'm';
        if (seconds >= 10)
        {
            if (pos < bufferSize - 1) pBuffer[pos++] = static_cast<char>('0' + (seconds / 10));
            if (pos < bufferSize - 1) pBuffer[pos++] = static_cast<char>('0' + (seconds % 10));
        }
        else
        {
            if (pos < bufferSize - 1) pBuffer[pos++] = static_cast<char>('0' + seconds);
        }
        if (pos < bufferSize - 1)
            pBuffer[pos++] = 's';
        pBuffer[pos] = '\0';
        return pos;
    }
}
