#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

namespace VectorMath {
    inline int32 DotProduct(const CoordStruct& a, const CoordStruct& b) {
        return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
    }

    inline CoordStruct CrossProduct(const CoordStruct& a, const CoordStruct& b) {
        return CoordStruct(
            a.Y * b.Z - a.Z * b.Y,
            a.Z * b.X - a.X * b.Z,
            a.X * b.Y - a.Y * b.X
        );
    }

    inline int32 Magnitude(const CoordStruct& v) {
        return static_cast<int32>(std::sqrt(static_cast<double>(
            v.X * v.X + v.Y * v.Y + v.Z * v.Z)));
    }

    inline CoordStruct Normalize(const CoordStruct& v) {
        int32 mag = Magnitude(v);
        if (mag == 0) return CoordStruct(0, 0, 0);
        return CoordStruct(v.X / mag, v.Y / mag, v.Z / mag);
    }

    inline CoordStruct Lerp(const CoordStruct& a, const CoordStruct& b, float t) {
        return CoordStruct(
            static_cast<int32>(a.X + (b.X - a.X) * t),
            static_cast<int32>(a.Y + (b.Y - a.Y) * t),
            static_cast<int32>(a.Z + (b.Z - a.Z) * t)
        );
    }

    inline CoordStruct MoveTowards(const CoordStruct& current, const CoordStruct& target, int32 speed) {
        int32 dist = current.DistanceFrom(target);
        if (dist <= speed) return target;
        float ratio = static_cast<float>(speed) / static_cast<float>(dist);
        return Lerp(current, target, ratio);
    }
}