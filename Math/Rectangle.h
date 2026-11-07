#pragma once

#include "Core/Definitions.h"

//========================================================================
// Rectangle
//
// A 2D integer rectangle structure used for UI layout, collision bounds,
// and screen-space regions. Matches the original game's RectangleStruct.
//
// The original game uses this as RectangleStruct in BasicStructures.h.
//========================================================================

struct Rectangle
{
    int32 X;
    int32 Y;
    int32 Width;
    int32 Height;

    //========================================================================
    // Construction
    //========================================================================

    constexpr Rectangle() noexcept
        : X(0), Y(0), Width(0), Height(0)
    {
    }

    constexpr Rectangle(int32 x, int32 y, int32 w, int32 h) noexcept
        : X(x), Y(y), Width(w), Height(h)
    {
    }

    //========================================================================
    // Comparison
    //========================================================================

    bool operator==(const Rectangle& rhs) const noexcept
    {
        return X == rhs.X && Y == rhs.Y && Width == rhs.Width && Height == rhs.Height;
    }

    bool operator!=(const Rectangle& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    //========================================================================
    // Point containment
    //========================================================================

    bool ContainsPoint(int32 px, int32 py) const noexcept
    {
        return px >= X && px < (X + Width) &&
               py >= Y && py < (Y + Height);
    }

    bool ContainsPoint(int32 px, int32 py, int32 margin) const noexcept
    {
        return px >= (X - margin) && px < (X + Width + margin) &&
               py >= (Y - margin) && py < (Y + Height + margin);
    }

    //========================================================================
    // Intersection
    //========================================================================

    bool Intersects(const Rectangle& other) const noexcept
    {
        return X < (other.X + other.Width) &&
               (X + Width) > other.X &&
               Y < (other.Y + other.Height) &&
               (Y + Height) > other.Y;
    }

    Rectangle Intersection(const Rectangle& other) const noexcept
    {
        int32 ix = (X > other.X) ? X : other.X;
        int32 iy = (Y > other.Y) ? Y : other.Y;
        int32 iw = ((X + Width) < (other.X + other.Width) ? (X + Width) : (other.X + other.Width)) - ix;
        int32 ih = ((Y + Height) < (other.Y + other.Height) ? (Y + Height) : (other.Y + other.Height)) - iy;

        if (iw < 0) iw = 0;
        if (ih < 0) ih = 0;

        return Rectangle(ix, iy, iw, ih);
    }

    //========================================================================
    // Inflate / Deflate
    //========================================================================

    Rectangle Inflate(int32 dx, int32 dy) const noexcept
    {
        return Rectangle(
            X - dx,
            Y - dy,
            Width + dx * 2,
            Height + dy * 2
        );
    }

    Rectangle Inflate(int32 amount) const noexcept
    {
        return Inflate(amount, amount);
    }

    Rectangle Deflate(int32 dx, int32 dy) const noexcept
    {
        return Inflate(-dx, -dy);
    }

    Rectangle Deflate(int32 amount) const noexcept
    {
        return Deflate(amount, amount);
    }

    //========================================================================
    // Utility
    //========================================================================

    int32 Right() const noexcept  { return X + Width; }
    int32 Bottom() const noexcept { return Y + Height; }
    int32 Left() const noexcept   { return X; }
    int32 Top() const noexcept    { return Y; }

    int32 CenterX() const noexcept { return X + Width / 2; }
    int32 CenterY() const noexcept { return Y + Height / 2; }

    bool IsEmpty() const noexcept
    {
        return Width <= 0 || Height <= 0;
    }

    bool IsValid() const noexcept
    {
        return Width > 0 && Height > 0;
    }

    int32 Area() const noexcept
    {
        return Width * Height;
    }

    //========================================================================
    // Position assignment
    //========================================================================

    // asm: Rect_SetPos (0x50E4A0 area) —— __thiscall，retn 0Ch
    //   dwLTRB *__thiscall Rect_SetPos(dwLTRB *this, dwXY *TopLeft, int R, int B)
    //   [this+0] = TopLeft->x ; [this+4] = TopLeft->y
    //   [this+8] = R          ; [this+0Ch] = B
    //
    // 按项目 Rectangle 的 X/Y/Width/Height 布局，即左上角取 TopLeft，
    // 右下边界（原版存 right/bottom）分别落到 Width/Height 槽位。
    void SetPos(const Point2D& topLeft, int32 right, int32 bottom) noexcept
    {
        X = topLeft.X;
        Y = topLeft.Y;
        Width = right;
        Height = bottom;
    }

    static const Rectangle Empty;
};

constexpr Rectangle Rectangle::Empty = Rectangle(0, 0, 0, 0);

//========================================================================
// Validate binary layout
//========================================================================

static_assert(sizeof(Rectangle) == sizeof(int32) * 4,
    "Rectangle must be exactly 4 int32 values");