#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Core/Memory.h"
#include "Math/Rectangle.h"

#include <cstring>
#include <cstdlib>

// Forward declarations
class ConvertClass;
struct SHPStruct;

// ColorStruct and Point2D are defined in Core/Definitions.h

// ============================================================================
// BytePalette - 256-entry palette (byte indices)
// ============================================================================
struct BytePalette
{
    BYTE Entries[256];

    BytePalette() { memset(Entries, 0, sizeof(Entries)); }
    BYTE& operator[](int32 index) { return Entries[index]; }
    const BYTE& operator[](int32 index) const { return Entries[index]; }
};

// ============================================================================
// RGBClass - 24-bit RGB color
// ============================================================================
struct RGBClass
{
    BYTE Red;
    BYTE Green;
    BYTE Blue;

    RGBClass() : Red(0), Green(0), Blue(0) {}
    RGBClass(int32 r, int32 g, int32 b)
        : Red(static_cast<BYTE>(r))
        , Green(static_cast<BYTE>(g))
        , Blue(static_cast<BYTE>(b))
    {}

    void Adjust(int32 ratio, const RGBClass& rgb)
    {
        ratio &= 0x00FF;
        int32 value = static_cast<int32>(rgb.Red) - static_cast<int32>(Red);
        Red = static_cast<BYTE>(static_cast<int32>(Red) + (value * ratio) / 256);
        value = static_cast<int32>(rgb.Green) - static_cast<int32>(Green);
        Green = static_cast<BYTE>(static_cast<int32>(Green) + (value * ratio) / 256);
        value = static_cast<int32>(rgb.Blue) - static_cast<int32>(Blue);
        Blue = static_cast<BYTE>(static_cast<int32>(Blue) + (value * ratio) / 256);
    }

    int32 Difference(const RGBClass& rgb) const
    {
        int32 r = static_cast<int32>(Red) - static_cast<int32>(rgb.Red);
        if (r < 0) r = -r;
        int32 g = static_cast<int32>(Green) - static_cast<int32>(rgb.Green);
        if (g < 0) g = -g;
        int32 b = static_cast<int32>(Blue) - static_cast<int32>(rgb.Blue);
        if (b < 0) b = -b;
        return r * r + g * g + b * b;
    }
};

// ============================================================================
// BlitterFlags - flags for blitter selection
// ============================================================================
enum class BlitterFlags : uint32
{
    None                 = 0x00000000,
    Centered             = 0x00000001,
    Translucent25        = 0x00000002,
    Translucent50        = 0x00000004,
    Translucent75        = 0x00000008,
    Translucent          = 0x0000000E,
    Transparent          = 0x00000010,
    MultiPass            = 0x00000020,
    Darken               = 0x00000040,
    Lighten              = 0x00000080,
    NoRemap              = 0x00000100,
    ZGradientNone        = 0x00000000,
    ZGradientGround      = 0x00000400,
    ZGradientDeg45       = 0x00000800,
    ZGradientDeg90       = 0x00000C00,
    ZGradientDeg135      = 0x00001000,
    ZGradient            = 0x00001C00,
    Flat                 = 0x00002000,
    Alpha                = 0x00004000,
    ZRead                = 0x00008000,
    ZWrite               = 0x00010000,
    ZReadWrite           = 0x00018000,
    Warp                 = 0x00020000,
    TintColor            = 0x00040000,
    Nonzero              = 0x00080000,
    ZRemap               = 0x00100000,
    Compressed           = 0x00200000,
    Plain                = 0x00400000,
    Remap                = 0x00800000,
    ZShape               = 0x01000000,
};

inline BlitterFlags operator|(BlitterFlags a, BlitterFlags b)
{
    return static_cast<BlitterFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
}
inline BlitterFlags operator&(BlitterFlags a, BlitterFlags b)
{
    return static_cast<BlitterFlags>(static_cast<uint32>(a) & static_cast<uint32>(b));
}

// ============================================================================
// TextPrintType - text rendering flags
// ============================================================================
enum class TextPrintType : uint32
{
    Left        = 0x0000,
    Center      = 0x0001,
    Right       = 0x0002,
    NoShadow    = 0x0004,
    Gradient    = 0x0008,
    FullShadow  = 0x0010,
    Point8      = 0x0020,
    Point6      = 0x0040,
    Point3      = 0x0080,
    DropShadow  = 0x0100,
    UseGradPal  = 0x0200,
    Metal12     = 0x0400,
};

// ============================================================================
// MouseCursorType - mouse cursor types
// ============================================================================
enum class MouseCursorType : int32
{
    None = -1, Normal = 0, No = 1, Move = 2, Enter = 3,
    Deploy = 4, Attack = 5, Harvest = 6, Select = 7,
    ScrollN = 8, ScrollNE = 9, ScrollE = 10, ScrollSE = 11,
    ScrollS = 12, ScrollSW = 13, ScrollW = 14, ScrollNW = 15,
    NoMove = 16, NoEnter = 17, NoDeploy = 18, Sell = 19,
    Sellable = 20, Repair = 21, Repairable = 22, DisarmBomb = 23,
    ToggleSelect = 24, GuardArea = 25, Airstrike = 26,
    Chronosphere = 27, PlaceBeacon = 28, Heal = 29,
    SpyPlane = 30, PsychicReveal = 31, ChronoWarp = 32,
    IronCurtain = 33, PlaceWaypoint = 34, Demolish = 35,
    Count = 36
};

// ============================================================================
// MouseHotSpot enums
// ============================================================================
enum class MouseHotSpotX : int32 { Left = 0, Center = 1, Right = 2 };
enum class MouseHotSpotY : int32 { Top = 0, Middle = 1, Bottom = 2 };

// ============================================================================
// RGBMode enum
// ============================================================================
enum class RGBMode : int32 { _555 = 0, _565 = 1, _888 = 2 };

// ============================================================================
// Surface - 2D pixel buffer abstraction
//
// Base class for all rendering surfaces. Provides a virtual interface
// for pixel-level operations, line drawing, and blitting.
//============================================================================
class NOVTABLE Surface
{
public:
    Surface() = default;
    virtual ~Surface() = default;

    // Copy operations
    virtual bool CopyFromWhole(Surface* pSrc, bool bUnk1, bool bUnk2)
    {
        if (!pSrc || !pSrc->Buffer) return false;
        int32 copyW = (Width < pSrc->Width) ? Width : pSrc->Width;
        int32 copyH = (Height < pSrc->Height) ? Height : pSrc->Height;
        int32 bytesPerPix = GetBytesPerPixel();
        for (int32 y = 0; y < copyH; ++y)
        {
            memcpy(static_cast<BYTE*>(Buffer) + y * Pitch,
                   static_cast<BYTE*>(pSrc->Buffer) + y * pSrc->Pitch,
                   static_cast<size_t>(copyW) * bytesPerPix);
        }
        return true;
    }

    virtual bool CopyFromPart(
        Rectangle* pClipRect, Surface* pSrc, Rectangle* pSrcRect,
        bool bUnk1, bool bUnk2)
    {
        if (!pSrc || !pSrc->Buffer || !Buffer) return false;
        Rectangle srcRect = pSrcRect ? *pSrcRect : Rectangle(0, 0, pSrc->Width, pSrc->Height);
        Rectangle clipRect = pClipRect
            ? Rectangle(0, 0, Width, Height).Intersection(*pClipRect)
            : Rectangle(0, 0, Width, Height);
        int32 bytesPerPix = GetBytesPerPixel();
        int32 copyW = (srcRect.Width < clipRect.Width) ? srcRect.Width : clipRect.Width;
        int32 copyH = (srcRect.Height < clipRect.Height) ? srcRect.Height : clipRect.Height;
        for (int32 y = 0; y < copyH; ++y)
        {
            BYTE* dstLine = static_cast<BYTE*>(Buffer) + (clipRect.Y + y) * Pitch + clipRect.X * bytesPerPix;
            const BYTE* srcLine = static_cast<const BYTE*>(pSrc->Buffer) + (srcRect.Y + y) * pSrc->Pitch + srcRect.X * bytesPerPix;
            memcpy(dstLine, srcLine, static_cast<size_t>(copyW) * bytesPerPix);
        }
        return true;
    }

    virtual bool CopyFrom(
        Rectangle* pClipRect, Rectangle* pClipRect2,
        Surface* pSrc, Rectangle* pDestRect, Rectangle* pSrcRect,
        bool bUnk1, bool bUnk2)
    {
        return CopyFromPart(pClipRect, pSrc, pSrcRect, bUnk1, bUnk2);
    }

    // Fill operations
    virtual bool FillRectEx(Rectangle* pClipRect, Rectangle* pFillRect, DWORD nColor)
    {
        if (!Buffer) return false;
        Rectangle r = pFillRect ? *pFillRect : Rectangle(0, 0, Width, Height);
        if (pClipRect) r = r.Intersection(*pClipRect);
        r = r.Intersection(Rectangle(0, 0, Width, Height));
        if (r.IsEmpty()) return true;
        int32 bytesPerPix = GetBytesPerPixel();
        for (int32 y = r.Y; y < r.Y + r.Height; ++y)
        {
            BYTE* line = static_cast<BYTE*>(Buffer) + y * Pitch + r.X * bytesPerPix;
            if (bytesPerPix == 1)
            {
                memset(line, static_cast<BYTE>(nColor), static_cast<size_t>(r.Width));
            }
            else if (bytesPerPix == 2)
            {
                uint16* scanline = reinterpret_cast<uint16*>(line);
                for (int32 x = 0; x < r.Width; ++x)
                    scanline[x] = static_cast<uint16>(nColor);
            }
            else if (bytesPerPix == 4)
            {
                uint32* scanline = reinterpret_cast<uint32*>(line);
                for (int32 x = 0; x < r.Width; ++x)
                    scanline[x] = nColor;
            }
        }
        return true;
    }

    virtual bool FillRect(Rectangle* pFillRect, DWORD nColor)
    {
        return FillRectEx(nullptr, pFillRect, nColor);
    }

    virtual bool Fill(DWORD nColor)
    {
        Rectangle r(0, 0, Width, Height);
        return FillRect(&r, nColor);
    }

    virtual bool FillRectTrans(Rectangle* pClipRect, ColorStruct* pColor, int32 nOpacity)
    {
        if (!Buffer || !pColor) return false;
        Rectangle r = pClipRect ? *pClipRect : Rectangle(0, 0, Width, Height);
        r = r.Intersection(Rectangle(0, 0, Width, Height));
        if (r.IsEmpty()) return true;
        int32 bytesPerPix = GetBytesPerPixel();
        if (bytesPerPix != 2) return false;
        uint16 baseColor = static_cast<uint16>((pColor->R >> 3) << 11 | (pColor->G >> 2) << 5 | (pColor->B >> 3));
        for (int32 y = r.Y; y < r.Y + r.Height; ++y)
        {
            uint16* line = reinterpret_cast<uint16*>(static_cast<BYTE*>(Buffer) + y * Pitch + r.X * 2);
            for (int32 x = 0; x < r.Width; ++x)
            {
                uint16 d = line[x];
                int32 srcR = nOpacity * (baseColor & 0xF800) >> 8;
                int32 srcG = nOpacity * (baseColor & 0x07E0) >> 8;
                int32 srcB = nOpacity * (baseColor & 0x001F) >> 8;
                int32 inv = 256 - nOpacity;
                int32 dstR = inv * (d & 0xF800) >> 8;
                int32 dstG = inv * (d & 0x07E0) >> 8;
                int32 dstB = inv * (d & 0x001F) >> 8;
                line[x] = static_cast<uint16>((srcR + dstR) & 0xF800 | (srcG + dstG) & 0x07E0 | (srcB + dstB) & 0x001F);
            }
        }
        return true;
    }

    // Draw operations
    virtual bool DrawEllipse(int32 XOff, int32 YOff, int32 CenterX, int32 CenterY, Rectangle Rect, DWORD nColor)
    {
        return false;
    }

    virtual bool SetPixel(Point2D* pPoint, DWORD nColor)
    {
        if (!pPoint || !Buffer) return false;
        if (pPoint->X < 0 || pPoint->X >= Width || pPoint->Y < 0 || pPoint->Y >= Height) return false;
        int32 bytesPerPix = GetBytesPerPixel();
        BYTE* dst = static_cast<BYTE*>(Buffer) + pPoint->Y * Pitch + pPoint->X * bytesPerPix;
        if (bytesPerPix == 1) *dst = static_cast<BYTE>(nColor);
        else if (bytesPerPix == 2) *reinterpret_cast<uint16*>(dst) = static_cast<uint16>(nColor);
        else if (bytesPerPix == 4) *reinterpret_cast<uint32*>(dst) = nColor;
        return true;
    }

    virtual DWORD GetPixel(Point2D* pPoint)
    {
        if (!pPoint || !Buffer) return 0;
        if (pPoint->X < 0 || pPoint->X >= Width || pPoint->Y < 0 || pPoint->Y >= Height) return 0;
        int32 bytesPerPix = GetBytesPerPixel();
        BYTE* src = static_cast<BYTE*>(Buffer) + pPoint->Y * Pitch + pPoint->X * bytesPerPix;
        if (bytesPerPix == 1) return *src;
        if (bytesPerPix == 2) return *reinterpret_cast<uint16*>(src);
        return *reinterpret_cast<uint32*>(src);
    }

    // Line drawing
    virtual bool DrawLineEx(Rectangle* pClipRect, Point2D* pStart, Point2D* pEnd, DWORD nColor)
    {
        if (!pStart || !pEnd || !Buffer) return false;
        int32 x0 = pStart->X, y0 = pStart->Y;
        int32 x1 = pEnd->X, y1 = pEnd->Y;
        int32 dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
        int32 dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
        int32 sx = (x0 < x1) ? 1 : -1;
        int32 sy = (y0 < y1) ? 1 : -1;
        int32 err = dx - dy;
        int32 bytesPerPix = GetBytesPerPixel();
        Rectangle clip = pClipRect ? *pClipRect : Rectangle(0, 0, Width, Height);
        while (true)
        {
            Point2D pt(x0, y0);
            if (clip.ContainsPoint(x0, y0))
                SetPixel(&pt, nColor);
            if (x0 == x1 && y0 == y1) break;
            int32 e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx) { err += dx; y0 += sy; }
        }
        return true;
    }

    virtual bool DrawLine(Point2D* pStart, Point2D* pEnd, DWORD nColor)
    {
        return DrawLineEx(nullptr, pStart, pEnd, nColor);
    }

    virtual bool DrawLineColor_AZ(
        Rectangle* pRect, Point2D* pStart, Point2D* pEnd, DWORD nColor,
        DWORD dwUnk1, DWORD dwUnk2, bool bUnk)
    {
        return DrawLineEx(pRect, pStart, pEnd, nColor);
    }

    virtual bool DrawMultiplyingLine_AZ(
        Rectangle* pRect, Point2D* pStart, Point2D* pEnd, DWORD dwMultiplier,
        DWORD dwUnk1, DWORD dwUnk2, bool bUnk)
    {
        return DrawLineEx(pRect, pStart, pEnd, dwMultiplier);
    }

    virtual bool DrawSubtractiveLine_AZ(
        Rectangle* pRect, Point2D* pStart, Point2D* pEnd, ColorStruct* pColor,
        DWORD dwUnk1, DWORD dwUnk2, bool bUnk1, bool bUnk2,
        bool bUnk3, bool bUnk4, float fUnk)
    {
        return DrawLineEx(pRect, pStart, pEnd, 0);
    }

    virtual bool DrawRGBMultiplyingLine_AZ(
        Rectangle* pRect, Point2D* pStart, Point2D* pEnd, ColorStruct* pColor,
        float Intensity, DWORD dwUnk1, DWORD dwUnk2)
    {
        return DrawLineEx(pRect, pStart, pEnd, 0);
    }

    virtual bool PlotLine(
        Rectangle* pRect, Point2D* pStart, Point2D* pEnd,
        bool(__fastcall* fpDrawCallback)(int32*))
    {
        return false;
    }

    virtual bool DrawDashedLine(
        Point2D* pStart, Point2D* pEnd, int32 nColor, bool* Pattern, int32 nOffset)
    {
        return DrawLine(pStart, pEnd, static_cast<DWORD>(nColor));
    }

    virtual bool DrawDashedLine_(
        Point2D* pStart, Point2D* pEnd, int32 nColor, bool* Pattern, int32 nOffset, bool bUnk)
    {
        return DrawDashedLine(pStart, pEnd, nColor, Pattern, nOffset);
    }

    virtual bool DrawLine_(Point2D* pStart, Point2D* pEnd, int32 nColor, bool bUnk)
    {
        return DrawLine(pStart, pEnd, static_cast<DWORD>(nColor));
    }

    // Rectangle drawing
    virtual bool DrawRectEx(Rectangle* pClipRect, Rectangle* pDrawRect, int32 nColor)
    {
        if (!pDrawRect || !Buffer) return false;
        Point2D tl(pDrawRect->X, pDrawRect->Y);
        Point2D tr(pDrawRect->X + pDrawRect->Width - 1, pDrawRect->Y);
        Point2D bl(pDrawRect->X, pDrawRect->Y + pDrawRect->Height - 1);
        Point2D br(pDrawRect->X + pDrawRect->Width - 1, pDrawRect->Y + pDrawRect->Height - 1);
        DWORD color = static_cast<DWORD>(nColor);
        DrawLineEx(pClipRect, &tl, &tr, color);
        DrawLineEx(pClipRect, &tr, &br, color);
        DrawLineEx(pClipRect, &br, &bl, color);
        DrawLineEx(pClipRect, &bl, &tl, color);
        return true;
    }

    virtual bool DrawRect(Rectangle* pDrawRect, DWORD dwColor)
    {
        return DrawRectEx(nullptr, pDrawRect, static_cast<int32>(dwColor));
    }

    // Locking / surface access
    virtual void* Lock(int32 X, int32 Y)
    {
        if (!Buffer) return nullptr;
        int32 bytesPerPix = GetBytesPerPixel();
        return static_cast<BYTE*>(Buffer) + Y * Pitch + X * bytesPerPix;
    }

    virtual bool Unlock() { return true; }
    virtual bool CanLock(DWORD dwUnk1 = 0, DWORD dwUnk2 = 0) { return Buffer != nullptr; }
    virtual bool vt_entry_68(DWORD dwUnk1, DWORD dwUnk2) { return false; }
    virtual bool IsLocked() { return false; }

    // Surface properties
    virtual int32 GetBytesPerPixel() { return 1; }
    virtual int32 GetPitch() { return Pitch; }
    virtual Rectangle* GetRect(Rectangle* pRect)
    {
        if (pRect) *pRect = Rectangle(0, 0, Width, Height);
        return pRect;
    }
    virtual int32 GetWidth() { return Width; }
    virtual int32 GetHeight() { return Height; }
    virtual bool IsDSurface() { return false; }

    // Helper
    Rectangle GetRect() { Rectangle ret; GetRect(&ret); return ret; }

    // =========================================================================
    // Non-virtual utility methods
    // =========================================================================
    void Clear(BYTE color = 0)
    {
        if (!Buffer) return;
        memset(Buffer, color, static_cast<size_t>(Width) * Height * GetBytesPerPixel());
    }

    void Blit(Surface* src, int32 x, int32 y)
    {
        if (!src || !Buffer) return;
        int32 bytesPerPix = GetBytesPerPixel();
        int32 copyW = src->Width;
        int32 copyH = src->Height;
        if (x + copyW > Width) copyW = Width - x;
        if (y + copyH > Height) copyH = Height - y;
        if (copyW <= 0 || copyH <= 0) return;
        int32 srcBytes = src->GetBytesPerPixel();
        for (int32 row = 0; row < copyH; ++row)
        {
            BYTE* dstLine = static_cast<BYTE*>(Buffer) + (y + row) * Pitch + x * bytesPerPix;
            const BYTE* srcLine = static_cast<const BYTE*>(src->Buffer) + row * src->Pitch;
            if (bytesPerPix == srcBytes)
                memcpy(dstLine, srcLine, static_cast<size_t>(copyW) * bytesPerPix);
            else
            {
                for (int32 col = 0; col < copyW; ++col)
                {
                    if (bytesPerPix == 2 && srcBytes == 1)
                    {
                        // 8-bit to 16-bit conversion (palette mapping)
                        reinterpret_cast<uint16*>(dstLine)[col] = srcLine[col];
                    }
                }
            }
        }
    }

    void BlitWithColorKey(Surface* src, int32 x, int32 y, BYTE colorKey)
    {
        if (!src || !Buffer) return;
        int32 bytesPerPix = GetBytesPerPixel();
        int32 copyW = src->Width;
        int32 copyH = src->Height;
        if (x + copyW > Width) copyW = Width - x;
        if (y + copyH > Height) copyH = Height - y;
        if (copyW <= 0 || copyH <= 0) return;
        int32 srcBytes = src->GetBytesPerPixel();
        if (bytesPerPix != srcBytes || bytesPerPix != 1) return;
        for (int32 row = 0; row < copyH; ++row)
        {
            BYTE* dstLine = static_cast<BYTE*>(Buffer) + (y + row) * Pitch + x;
            const BYTE* srcLine = static_cast<const BYTE*>(src->Buffer) + row * src->Pitch;
            for (int32 col = 0; col < copyW; ++col)
            {
                if (srcLine[col] != colorKey)
                    dstLine[col] = srcLine[col];
            }
        }
    }

    void BlitPart(Surface* src, Rectangle srcRect, int32 dstX, int32 dstY)
    {
        if (!src || !Buffer) return;
        int32 bytesPerPix = GetBytesPerPixel();
        int32 copyW = srcRect.Width;
        int32 copyH = srcRect.Height;
        if (dstX + copyW > Width) copyW = Width - dstX;
        if (dstY + copyH > Height) copyH = Height - dstY;
        if (copyW <= 0 || copyH <= 0) return;
        int32 srcBytes = src->GetBytesPerPixel();
        for (int32 row = 0; row < copyH; ++row)
        {
            BYTE* dstLine = static_cast<BYTE*>(Buffer) + (dstY + row) * Pitch + dstX * bytesPerPix;
            const BYTE* srcLine = static_cast<const BYTE*>(src->Buffer) + (srcRect.Y + row) * src->Pitch + srcRect.X * srcBytes;
            if (bytesPerPix == srcBytes)
                memcpy(dstLine, srcLine, static_cast<size_t>(copyW) * bytesPerPix);
        }
    }

    void SetPixel(int32 x, int32 y, BYTE color)
    {
        if (!Buffer || x < 0 || x >= Width || y < 0 || y >= Height) return;
        static_cast<BYTE*>(Buffer)[y * Pitch + x] = color;
    }

    BYTE GetPixel(int32 x, int32 y) const
    {
        if (!Buffer || x < 0 || x >= Width || y < 0 || y >= Height) return 0;
        return static_cast<const BYTE*>(Buffer)[y * Pitch + x];
    }

    void FillRect(const Rectangle& rect, BYTE color)
    {
        if (!Buffer) return;
        Rectangle r = rect.Intersection(Rectangle(0, 0, Width, Height));
        if (r.IsEmpty()) return;
        for (int32 row = r.Y; row < r.Y + r.Height; ++row)
        {
            memset(static_cast<BYTE*>(Buffer) + row * Pitch + r.X, color, static_cast<size_t>(r.Width));
        }
    }

    // Properties
    int32 Width;
    int32 Height;
    int32 Pitch;
    void* Buffer;
    bool IsAllocated;
    int32 PixelFormat;
};

// ============================================================================
// XSurface - Extended surface with additional properties
// ============================================================================
class NOVTABLE XSurface : public Surface
{
public:
    XSurface(int32 nWidth = 640, int32 nHeight = 400)
    {
        Width = nWidth;
        Height = nHeight;
        Pitch = nWidth;
        BytesPerPixel = 1;
        LockLevel = 0;
        Buffer = nullptr;
        IsAllocated = false;
    }

    virtual bool PutPixelClip(Point2D* pPoint, int16 nUnk, Rectangle* pRect)
    {
        if (!pPoint || !Buffer || !pRect) return false;
        if (pRect->ContainsPoint(pPoint->X, pPoint->Y))
        {
            Point2D pt(pPoint->X, pPoint->Y);
            return SetPixel(&pt, static_cast<DWORD>(nUnk));
        }
        return false;
    }

    virtual int16 GetPixelClip(Point2D* pPoint, Rectangle* pRect)
    {
        if (!pPoint || !Buffer || !pRect) return 0;
        if (pRect->ContainsPoint(pPoint->X, pPoint->Y))
        {
            return static_cast<int16>(GetPixel(pPoint));
        }
        return 0;
    }

    int32 LockLevel;
    int32 BytesPerPixel;
};

// ============================================================================
// BSurface - Buffered surface with double-buffering
// ============================================================================
class NOVTABLE BSurface : public XSurface
{
public:
    BSurface()
        : XSurface()
        , MemoryBuffer()
    {
        BytesPerPixel = 2;
    }

    void Allocate(int32 w, int32 h)
    {
        Free();
        Width = w;
        Height = h;
        Pitch = w * BytesPerPixel;
        size_t size = static_cast<size_t>(w) * h * BytesPerPixel;
        MemoryBuffer.Buffer = YRMemory::Allocate(size);
        MemoryBuffer.Size = static_cast<uint32>(size);
        Buffer = MemoryBuffer.Buffer;
        IsAllocated = true;
        if (Buffer) memset(Buffer, 0, size);
    }

    void Free()
    {
        if (MemoryBuffer.Buffer && IsAllocated)
        {
            YRMemory::Deallocate(MemoryBuffer.Buffer);
            MemoryBuffer.Buffer = nullptr;
            MemoryBuffer.Size = 0;
        }
        Buffer = nullptr;
        IsAllocated = false;
        Width = 0;
        Height = 0;
        Pitch = 0;
    }

    // MemoryBuffer is a simple buffer struct
    struct MemoryBuffer
    {
        void* Buffer;
        uint32 Size;
        MemoryBuffer() : Buffer(nullptr), Size(0) {}
    } MemoryBuffer;
};

// ============================================================================
// ASurface - Alpha mask surface
// ============================================================================
class NOVTABLE ASurface : public Surface
{
public:
    ASurface()
    {
        Width = 0;
        Height = 0;
        Pitch = 0;
        Buffer = nullptr;
        IsAllocated = false;
        AlphaBuffer = nullptr;
    }

    void Allocate(int32 w, int32 h)
    {
        Free();
        Width = w;
        Height = h;
        Pitch = w * 2;
        size_t size = static_cast<size_t>(w) * h * 2;
        Buffer = YRMemory::Allocate(size);
        if (Buffer) memset(Buffer, 0, size);
        AlphaBuffer = reinterpret_cast<uint16*>(Buffer);
        IsAllocated = true;
    }

    void Free()
    {
        if (Buffer && IsAllocated)
        {
            YRMemory::Deallocate(Buffer);
            Buffer = nullptr;
        }
        AlphaBuffer = nullptr;
        IsAllocated = false;
        Width = 0;
        Height = 0;
        Pitch = 0;
    }

    uint16* AlphaBuffer;
};

// ============================================================================
// DSurface - DirectDraw surface (primary rendering surface)
// ============================================================================
class NOVTABLE DSurface : public XSurface
{
public:
    DSurface(int32 nWidth = 640, int32 nHeight = 400)
        : XSurface(nWidth, nHeight)
    {
        IsAllocated = false;
        IsInVideoRam = false;
        VideoSurfacePtr = nullptr;
        VideoSurfaceDescription = nullptr;
    }

    virtual bool DrawGradientLine(
        Rectangle* pRect, Point2D* pStart, Point2D* pEnd,
        ColorStruct* pStartColor, ColorStruct* pEndColor, float fStep, int32 nColor)
    {
        return DrawLineEx(pRect, pStart, pEnd, static_cast<DWORD>(nColor));
    }

    virtual bool CanBlit() { return Buffer != nullptr; }

    void Allocate(int32 w, int32 h)
    {
        Free();
        Width = w;
        Height = h;
        BytesPerPixel = 2;
        Pitch = w * BytesPerPixel;
        size_t size = static_cast<size_t>(w) * h * BytesPerPixel;
        Buffer = YRMemory::Allocate(size);
        if (Buffer) memset(Buffer, 0, size);
        IsAllocated = true;
        IsInVideoRam = false;
    }

    void Free()
    {
        if (Buffer && IsAllocated && !IsInVideoRam)
        {
            YRMemory::Deallocate(Buffer);
            Buffer = nullptr;
        }
        IsAllocated = false;
        IsInVideoRam = false;
        Width = 0;
        Height = 0;
        Pitch = 0;
    }

    // DrawSHP: draw a shape to this surface
    void DrawSHP(
        ConvertClass* Palette, SHPStruct* SHP, int32 FrameIndex,
        const Point2D* Position, const Rectangle* Bounds, BlitterFlags Flags,
        int32 Remap, int32 ZAdjust, int32 ZGradientDescIndex,
        int32 Brightness, int32 TintColor,
        SHPStruct* ZShape, int32 ZShapeFrame, int32 XOffset, int32 YOffset);

    void DrawText(const wchar_t* pText, Rectangle* pBounds, Point2D* pLocation,
        DWORD ForeColor, DWORD BackColor, TextPrintType Flag);

    void DrawText(const wchar_t* pText, Point2D* pLocation, DWORD Color);
    void DrawText(const wchar_t* pText, int32 X, int32 Y, DWORD Color);

    void SetPixelAlpha(int32 x, int32 y, uint8 r, uint8 g, uint8 b, uint8 a)
    {
        if (!Buffer || x < 0 || x >= Width || y < 0 || y >= Height) return;
        // Convert RGBA to 16-bit color (565 format) with alpha
        uint16 baseColor = static_cast<uint16>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        int32 bytesPerPix = GetBytesPerPixel();
        if (bytesPerPix == 2)
        {
            uint16* dst = reinterpret_cast<uint16*>(static_cast<BYTE*>(Buffer) + y * Pitch + x * 2);
            uint16 d = *dst;
            if (a == 0)
            {
                *dst = baseColor;
            }
            else if (a < 255)
            {
                int32 srcR = a * (baseColor & 0xF800) >> 8;
                int32 srcG = a * (baseColor & 0x07E0) >> 8;
                int32 srcB = a * (baseColor & 0x001F) >> 8;
                int32 inv = 256 - a;
                int32 dstR = inv * (d & 0xF800) >> 8;
                int32 dstG = inv * (d & 0x07E0) >> 8;
                int32 dstB = inv * (d & 0x001F) >> 8;
                *dst = static_cast<uint16>((srcR + dstR) & 0xF800 | (srcG + dstG) & 0x07E0 | (srcB + dstB) & 0x001F);
            }
        }
        else if (bytesPerPix == 4)
        {
            uint32* dst = reinterpret_cast<uint32*>(static_cast<BYTE*>(Buffer) + y * Pitch + x * 4);
            *dst = (static_cast<uint32>(a) << 24) | (static_cast<uint32>(r) << 16) | (static_cast<uint32>(g) << 8) | b;
        }
    }

    bool IsInVideoRam;
    BYTE padding_1A[2];
    void* VideoSurfacePtr;
    void* VideoSurfaceDescription;
};

// ============================================================================
// LTRBStruct - left, top, right, bottom bounds
// ============================================================================
struct LTRBStruct
{
    int32 Left;
    int32 Top;
    int32 Right;
    int32 Bottom;

    LTRBStruct() : Left(0), Top(0), Right(0), Bottom(0) {}
    LTRBStruct(int32 l, int32 t, int32 r, int32 b)
        : Left(l), Top(t), Right(r), Bottom(b) {}
};

// CellStruct is defined in Core/Definitions.h

// ============================================================================
// Text printing functions
// ============================================================================
Point2D* Fancy_Text_Print_Wide(
    Point2D& retBuffer, const wchar_t* Text, Surface* pSurface,
    const Rectangle& Bounds, const Point2D& Location,
    DWORD ForeColor, DWORD BackColor, TextPrintType Flag);

Point2D* Simple_Text_Print_Wide(
    Point2D* RetVal, const wchar_t* Text, Surface* pSurface,
    Rectangle* Bounds, Point2D* Location,
    DWORD ForeColor, DWORD BackColor, TextPrintType Flag, bool bUnk);

void CC_Draw_Shape(
    Surface* pSurface, ConvertClass* Palette, SHPStruct* SHP, int32 FrameIndex,
    const Point2D* Position, const Rectangle* Bounds, BlitterFlags Flags,
    int32 Remap, int32 ZAdjust, int32 ZGradientDescIndex,
    int32 Brightness, int32 TintColor,
    SHPStruct* ZShape, int32 ZShapeFrame, int32 XOffset, int32 YOffset);