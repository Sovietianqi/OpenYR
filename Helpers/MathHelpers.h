#pragma once

// ============================================================================
// MathHelpers.h - Math utility functions
//
//  Provides a collection of small, inlinable math helpers used across the
//  engine: clamping, interpolation (lerp, smoothstep), angle utilities,
//  power-of-two helpers and fixed-point conversions.
//
//  All functions are constexpr where possible so they can be used in
//  compile-time contexts.
// ============================================================================

#include <Core/Definitions.h>
#include <Core/Macros.h>

#include <cmath>
#include <cstdint>

// ============================================================================
// Clamping
// ============================================================================

namespace MathHelpers
{
    // Clamp an integral value to [lo, hi].
    template <typename T>
    constexpr T Clamp(T value, T lo, T hi) noexcept
    {
        if (value < lo) return lo;
        if (value > hi) return hi;
        return value;
    }

    // Clamp a floating-point value to [lo, hi].
    inline float ClampF(float value, float lo, float hi) noexcept
    {
        if (value < lo) return lo;
        if (value > hi) return hi;
        return value;
    }

    inline double ClampD(double value, double lo, double hi) noexcept
    {
        if (value < lo) return lo;
        if (value > hi) return hi;
        return value;
    }

    // Saturate a float to [0.0, 1.0].
    inline float Saturate(float value) noexcept
    {
        return ClampF(value, 0.0f, 1.0f);
    }

    inline double SaturateD(double value) noexcept
    {
        return ClampD(value, 0.0, 1.0);
    }

    // ============================================================================
    // Interpolation
    // ============================================================================

    // Linear interpolation: returns a + (b - a) * t.
    template <typename T, typename F>
    constexpr T Lerp(T a, T b, F t) noexcept
    {
        return static_cast<T>(a + (b - a) * t);
    }

    inline float LerpF(float a, float b, float t) noexcept
    {
        return a + (b - a) * t;
    }

    inline double LerpD(double a, double b, double t) noexcept
    {
        return a + (b - a) * t;
    }

    // Inverse lerp: returns the fraction t such that Lerp(a, b, t) == value.
    inline float InverseLerp(float a, float b, float value) noexcept
    {
        if (b == a) return 0.0f;
        return (value - a) / (b - a);
    }

    // Remap a value from [inA, inB] to [outA, outB].
    inline float Remap(float value, float inA, float inB,
                       float outA, float outB) noexcept
    {
        return LerpF(outA, outB, Saturate(InverseLerp(inA, inB, value)));
    }

    // Smoothstep: Hermite interpolation with zero derivative at the ends.
    // Returns 0 for t <= 0, 1 for t >= 1, and a smooth curve between.
    inline float Smoothstep(float t) noexcept
    {
        t = Saturate(t);
        return t * t * (3.0f - 2.0f * t);
    }

    // Smoothstep with explicit edges (edge0 -> edge1).
    inline float Smoothstep(float edge0, float edge1, float x) noexcept
    {
        return Smoothstep(InverseLerp(edge0, edge1, x));
    }

    // Smootherstep: 5th-degree polynomial with zero 1st and 2nd derivatives
    // at the endpoints.  Slightly more expensive than Smoothstep.
    inline float Smootherstep(float t) noexcept
    {
        t = Saturate(t);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    // ============================================================================
    // Angle / direction helpers
    //
    //  The engine uses 256-direction facings (a full circle = 256 units).
    //  These helpers convert between degrees, radians and 256-direction
    //  values.
    // ============================================================================

    constexpr float PI = 3.14159265358979323846f;
    constexpr double PI_D = 3.14159265358979323846;

    constexpr float TwoPI = 2.0f * PI;
    constexpr float HalfPI = 0.5f * PI;

    // Degrees to radians.
    inline float DegToRad(float deg) noexcept { return deg * (PI / 180.0f); }
    inline double DegToRadD(double deg) noexcept { return deg * (PI_D / 180.0); }

    // Radians to degrees.
    inline float RadToDeg(float rad) noexcept { return rad * (180.0f / PI); }
    inline double RadToDegD(double rad) noexcept { return rad * (180.0 / PI_D); }

    // 256-direction to radians (0 = east, 64 = south, 128 = west, 192 = north).
    inline float Dir256ToRad(uint8 dir) noexcept
    {
        return static_cast<float>(dir) * (TwoPI / 256.0f);
    }

    // Radians to 256-direction.
    inline uint8 RadToDir256(float rad) noexcept
    {
        float normalized = std::fmod(rad, TwoPI);
        if (normalized < 0.0f) normalized += TwoPI;
        return static_cast<uint8>(normalized * (256.0f / TwoPI));
    }

    // Smallest angular difference between two 256-direction values,
    // returned in [-128, 127].
    inline int32 Dir256Delta(uint8 a, uint8 b) noexcept
    {
        int32 delta = static_cast<int32>(b) - static_cast<int32>(a);
        if (delta > 128) delta -= 256;
        if (delta < -128) delta += 256;
        return delta;
    }

    // ============================================================================
    // Power-of-two helpers
    // ============================================================================

    // True if n is a power of two (and non-zero).
    inline bool IsPowerOfTwo(uint32 n) noexcept
    {
        return n != 0 && (n & (n - 1)) == 0;
    }

    // Next power of two >= n.  Returns 1 for n == 0.
    inline uint32 NextPowerOfTwo(uint32 n) noexcept
    {
        if (n == 0) return 1;
        --n;
        n |= n >> 1;  n |= n >> 2;
        n |= n >> 4;  n |= n >> 8;
        n |= n >> 16;
        return n + 1;
    }

    // Previous power of two <= n.  Returns 0 for n == 0.
    inline uint32 PrevPowerOfTwo(uint32 n) noexcept
    {
        if (n == 0) return 0;
        uint32 next = NextPowerOfTwo(n);
        return (next == n) ? n : (next >> 1);
    }

    // Base-2 log, rounded down.  Returns 0 for n == 0.
    inline int32 Log2(uint32 n) noexcept
    {
        if (n == 0) return 0;
        int32 r = 0;
        while (n > 1) { n >>= 1; ++r; }
        return r;
    }

    // ============================================================================
    // Fixed-point helpers
    //
    //  The engine uses 8.8 fixed-point for some speed/facing calculations.
    //  These helpers convert between float and fixed-point.
    // ============================================================================

    using Fixed16 = uint16;  // 8.8 fixed-point

    inline Fixed16 FloatToFixed8_8(float value) noexcept
    {
        return static_cast<Fixed16>(Saturate(value) * 255.0f);
    }

    inline float Fixed8_8ToFloat(Fixed16 value) noexcept
    {
        return static_cast<float>(value) / 255.0f;
    }

    // ============================================================================
    // Distance helpers
    // ============================================================================

    // 2D distance (integer, truncated).
    inline int32 Distance2D(int32 dx, int32 dy) noexcept
    {
        return static_cast<int32>(std::sqrt(static_cast<double>(dx * dx + dy * dy)));
    }

    // 2D distance squared (avoids sqrt).
    inline int64 Distance2DSquared(int32 dx, int32 dy) noexcept
    {
        return static_cast<int64>(dx) * dx + static_cast<int64>(dy) * dy;
    }

    // 3D distance.
    inline int32 Distance3D(int32 dx, int32 dy, int32 dz) noexcept
    {
        return static_cast<int32>(std::sqrt(static_cast<double>(
            dx * dx + dy * dy + dz * dz)));
    }

    // 3D distance squared.
    inline int64 Distance3DSquared(int32 dx, int32 dy, int32 dz) noexcept
    {
        return static_cast<int64>(dx) * dx
             + static_cast<int64>(dy) * dy
             + static_cast<int64>(dz) * dz;
    }

    // Approximate 2D distance using the "alpha max + beta min" method.
    // Faster than a sqrt and accurate to within ~4%.
    inline int32 FastDistance2D(int32 dx, int32 dy) noexcept
    {
        int32 a = dx < 0 ? -dx : dx;
        int32 b = dy < 0 ? -dy : dy;
        if (a < b) { int32 t = a; a = b; b = t; }
        // 0.96 * max + 0.40 * min  (approximately 15/16 and 6/16)
        return a + (b >> 1) - (b >> 4);
    }

    // ============================================================================
    // Sign helpers
    // ============================================================================

    template <typename T>
    constexpr int32 Sign(T value) noexcept
    {
        return (value > T(0)) ? 1 : (value < T(0) ? -1 : 0);
    }

    // ============================================================================
    // Wrap / modulo helpers
    // ============================================================================

    // Wrap value to [0, range).  Works correctly for negative inputs.
    inline int32 Wrap(int32 value, int32 range) noexcept
    {
        if (range <= 0) return 0;
        int32 r = value % range;
        if (r < 0) r += range;
        return r;
    }

    // Wrap a float to [0, range).
    inline float WrapF(float value, float range) noexcept
    {
        if (range <= 0.0f) return 0.0f;
        float r = std::fmod(value, range);
        if (r < 0.0f) r += range;
        return r;
    }
}
