#pragma once

#include "Core/Definitions.h"

#include <cmath>

//========================================================================
// Point3D
//
// A 3D floating-point vector structure used for 3D graphics calculations,
// model transformations, and rendering in the game engine.
//
// This is equivalent to Vector3D<float> in the original game.
//========================================================================

struct Point3D
{
    float X;
    float Y;
    float Z;

    //========================================================================
    // Construction
    //========================================================================

    constexpr Point3D() noexcept
        : X(0.0f), Y(0.0f), Z(0.0f)
    {
    }

    constexpr Point3D(float x, float y, float z) noexcept
        : X(x), Y(y), Z(z)
    {
    }

    //========================================================================
    // Comparison
    //========================================================================

    bool operator==(const Point3D& rhs) const noexcept
    {
        return X == rhs.X && Y == rhs.Y && Z == rhs.Z;
    }

    bool operator!=(const Point3D& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    //========================================================================
    // Arithmetic operators
    //========================================================================

    Point3D operator+(const Point3D& rhs) const noexcept
    {
        return Point3D(X + rhs.X, Y + rhs.Y, Z + rhs.Z);
    }

    Point3D operator-(const Point3D& rhs) const noexcept
    {
        return Point3D(X - rhs.X, Y - rhs.Y, Z - rhs.Z);
    }

    Point3D operator-() const noexcept
    {
        return Point3D(-X, -Y, -Z);
    }

    Point3D& operator+=(const Point3D& rhs) noexcept
    {
        X += rhs.X;
        Y += rhs.Y;
        Z += rhs.Z;
        return *this;
    }

    Point3D& operator-=(const Point3D& rhs) noexcept
    {
        X -= rhs.X;
        Y -= rhs.Y;
        Z -= rhs.Z;
        return *this;
    }

    // Scalar multiplication
    Point3D operator*(float scalar) const noexcept
    {
        return Point3D(X * scalar, Y * scalar, Z * scalar);
    }

    friend Point3D operator*(float scalar, const Point3D& v) noexcept
    {
        return Point3D(v.X * scalar, v.Y * scalar, v.Z * scalar);
    }

    Point3D& operator*=(float scalar) noexcept
    {
        X *= scalar;
        Y *= scalar;
        Z *= scalar;
        return *this;
    }

    // Scalar division
    Point3D operator/(float scalar) const noexcept
    {
        float inv = 1.0f / scalar;
        return Point3D(X * inv, Y * inv, Z * inv);
    }

    Point3D& operator/=(float scalar) noexcept
    {
        float inv = 1.0f / scalar;
        X *= inv;
        Y *= inv;
        Z *= inv;
        return *this;
    }

    //========================================================================
    // Dot product
    //========================================================================

    float Dot(const Point3D& other) const noexcept
    {
        return X * other.X + Y * other.Y + Z * other.Z;
    }

    //========================================================================
    // Cross product
    //========================================================================

    Point3D Cross(const Point3D& other) const noexcept
    {
        return Point3D(
            Y * other.Z - Z * other.Y,
            Z * other.X - X * other.Z,
            X * other.Y - Y * other.X
        );
    }

    //========================================================================
    // Length / Magnitude
    //========================================================================

    float Length() const noexcept
    {
        return std::sqrt(LengthSquared());
    }

    float LengthSquared() const noexcept
    {
        return X * X + Y * Y + Z * Z;
    }

    //========================================================================
    // Normalize
    //========================================================================

    Point3D Normalize() const noexcept
    {
        float len = Length();
        if (len > 0.0f)
        {
            float inv = 1.0f / len;
            return Point3D(X * inv, Y * inv, Z * inv);
        }
        return Point3D(0.0f, 0.0f, 0.0f);
    }

    Point3D& NormalizeInPlace() noexcept
    {
        float len = Length();
        if (len > 0.0f)
        {
            float inv = 1.0f / len;
            X *= inv;
            Y *= inv;
            Z *= inv;
        }
        return *this;
    }

    //========================================================================
    // Distance
    //========================================================================

    float DistanceFrom(const Point3D& other) const noexcept
    {
        return (*this - other).Length();
    }

    float DistanceFromSquared(const Point3D& other) const noexcept
    {
        return (*this - other).LengthSquared();
    }

    //========================================================================
    // Linear interpolation
    //========================================================================

    static Point3D Lerp(const Point3D& a, const Point3D& b, float t) noexcept
    {
        return Point3D(
            a.X + (b.X - a.X) * t,
            a.Y + (b.Y - a.Y) * t,
            a.Z + (b.Z - a.Z) * t
        );
    }

    //========================================================================
    // Utility
    //========================================================================

    bool IsZero() const noexcept
    {
        return X == 0.0f && Y == 0.0f && Z == 0.0f;
    }

    bool IsUnit() const noexcept
    {
        float lenSq = LengthSquared();
        return lenSq > 0.999f && lenSq < 1.001f;
    }

    static const Point3D Empty;
    static const Point3D UnitX;
    static const Point3D UnitY;
    static const Point3D UnitZ;
};

inline constexpr Point3D Point3D::Empty = Point3D(0.0f, 0.0f, 0.0f);
inline constexpr Point3D Point3D::UnitX = Point3D(1.0f, 0.0f, 0.0f);
inline constexpr Point3D Point3D::UnitY = Point3D(0.0f, 1.0f, 0.0f);
inline constexpr Point3D Point3D::UnitZ = Point3D(0.0f, 0.0f, 1.0f);

//========================================================================
// Validate binary layout
//========================================================================

static_assert(sizeof(Point3D) == sizeof(float) * 3,
    "Point3D must be exactly 3 float values");