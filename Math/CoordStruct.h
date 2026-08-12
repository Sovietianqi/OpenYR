#pragma once

#include "../Core/Definitions.h"

namespace Math {
    inline CoordStruct CellToCoord(const CellStruct& cell) {
        return CoordStruct(
            static_cast<int32>(cell.X) * LeptonsPerCell,
            static_cast<int32>(cell.Y) * LeptonsPerCell,
            0
        );
    }

    inline CellStruct CoordToCell(const CoordStruct& coord) {
        return CellStruct(
            static_cast<int16>(coord.X / LeptonsPerCell),
            static_cast<int16>(coord.Y / LeptonsPerCell)
        );
    }

    inline int32 CellDistance(const CellStruct& a, const CellStruct& b) {
        int32 dx = static_cast<int32>(a.X) - static_cast<int32>(b.X);
        int32 dy = static_cast<int32>(a.Y) - static_cast<int32>(b.Y);
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        return (dx > dy) ? dx : dy;
    }

    inline int32 CoordDistance(const CoordStruct& a, const CoordStruct& b) {
        return a.DistanceFrom(b);
    }

    inline DirStruct DirectionTo(const CoordStruct& from, const CoordStruct& to) {
        int32 dx = to.X - from.X;
        int32 dy = to.Y - from.Y;
        double angle = std::atan2(static_cast<double>(dy), static_cast<double>(dx));
        if (angle < 0) angle += 2.0 * 3.141592653589793;
        uint8 dir = static_cast<uint8>((angle / (2.0 * 3.141592653589793)) * 256.0) % 256;
        return DirStruct(dir);
    }
}

namespace CoordMath = Math;