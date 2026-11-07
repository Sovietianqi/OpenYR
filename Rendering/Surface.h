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
// ============================================================================
// BlitterFlags - 与 gamemd.exe 的 eBlitterFlags 位值逐位一致
//
//   asm: 枚举 eBlitterFlags（.data 的 mappedto_5425），原样：
//     bf_001 = 1, bf_Alpha25 = 2, bf_Alpha50 = 4, bf_Warp = 8,
//     bf_010 = 10h, bf_020 = 20h, bf_040 = 40h, bf_080 = 80h,
//     bf_MultiPass = 100h, bf_200 = 200h, bf_400 = 400h,
//     bf_Alpha = 800h, bf_1000 = 1000h, bf_2000 = 2000h,
//     bf_4000 = 4000h, bf_8000 = 8000h
//
//   名字对照（全部由 SelectBlitter0 的 0x490B9F..0x490E40 反推，
//   不是照 XCC 猜的）：
//     * CC_Draw_Shape 用 `test ah,2`（0x200）做居中 —— Centered
//     * 所有贴 SHP 的调用都带 0x400（0x600 / 0xE00 / 0x2601 / 0x4601）—— Transparent
//     * `and ecx,6; sub ecx,2` 三分支 —— 半透明档 2 / 4 / 6
//     * `test ah,8`（0x800）—— Alpha
//     * `test al,1`（0x1）→ 返回 BlitTransDarken 族（槽 2/9/19）—— **Darken**
//     * `test al,20h`（0x20）→ 返回 BlitPlainXlat 族（槽 0/24/36/45）—— **Plain**
//     * 两者都不设 → BlitTransXlat 族（槽 1/8/25/36/45）—— 色键透明
//     * `test ah,80h`（0x8000）/ `ah,1`（0x100）/ `al,40h`（0x40）——
//       槽 30/31/32 的写 alpha 族，含义未逐一确证，仅按位值命名
//
//   ⚠ 这一版是**改正**过的：此前把 0x1 当 Remap、0x20 当 Darken，正好反了。
//   证据：阴影绘制传 0x2601 / 0x4601（AnimClass_Draw、UnitClass、Aircraft），
//   若 0x1 不是 Darken，阴影会以全彩精灵贴出来而不是压暗的剪影。
// ============================================================================
enum class BlitterFlags : uint32
{
    None                 = 0x00000000,

    Darken               = 0x00000001,   // bf_001 → BlitTransDarken 族
    Translucent25        = 0x00000002,   // bf_Alpha25
    Translucent50        = 0x00000004,   // bf_Alpha50
    Translucent75        = 0x00000006,   // bf_Alpha25|bf_Alpha50
    Translucent          = 0x00000006,
    Warp                 = 0x00000008,   // bf_Warp

    ZRead                = 0x00000010,   // bf_010
    Plain                = 0x00000020,   // bf_020 → BlitPlainXlat 族（无透明色键）
    Lighten              = 0x00000040,   // bf_040 → 槽 32（写 alpha）
    TintColor            = 0x00000080,   // bf_080

    MultiPass            = 0x00000100,   // bf_MultiPass
    Centered             = 0x00000200,   // bf_200
    Transparent          = 0x00000400,   // bf_400
    Alpha                = 0x00000800,   // bf_Alpha

    ZGradientNone        = 0x00000000,
    ZGradientGround      = 0x00001000,   // bf_1000
    ZGradientDeg45       = 0x00002000,   // bf_2000
    ZGradient            = 0x00003000,

    ZWrite               = 0x00004000,   // bf_4000
    ZReadWrite           = 0x00004010,   // ZRead|ZWrite
    ZShape               = 0x00008000,   // bf_8000

    // ---- OpenYR 内部扩展（不占用原版 16 位 flags 位域）------------------
    //   注意：原版 asm 在 Lucent50 档里真的会 test 10000h / 20000h
    //   （见 SelectBlitter0 的 loc_490DB8 / loc_490DC9），所以这两个位
    //   在 asm 里是有意义的、只是语义没确认（对应槽 34 / 33）。
    //   这里保持位值一致，名字用中性写法，避免和 0x20 的 Plain 混淆。
    NoRemap              = 0x00010000,   // asm 槽 34
    Compressed           = 0x00020000,   // asm 槽 33
    ZRemap               = 0x00080000,
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

    // ---------------------------------------------------------------------
    // asm: Draw_Confined_PCX (0x6BA590, retn 10h)
    //
    //   this   = DEST 表面（提供 stride）
    //   pOther = SOURCE PCX 表面（提供 W/H）
    //   pRect  = {x, y, w, h}，只用前两项（w/h 在 asm 里被 W2/H2 覆盖，是死值）
    //   colorKey = 16bpp 透明色
    //
    //   ptr1 = DEST->vt[5Ch](0,0) ; if (!ptr1) return false
    //   ptr2 = SRC ->vt[5Ch](0,0) ; if (!ptr2) { DEST->vt[60h]() ; return false }
    //   W2 = SRC->vt[7Ch]() ; H2 = SRC->vt[80h]()
    //   half = DEST->vt[74h]() / 2                (cdq / sub / sar 1)
    //   if (H2 > 0):
    //     dst = ptr1 + 2*(y*half + x)
    //     for (row < H2):
    //       for (i < W2): px = src[2i] ; if (px != colorKey) dst[2i] = px
    //       src += 2*W2 ; dst += 2*half
    //   DEST->vt[60h]() ; SRC->vt[60h]() ; return true
    //
    // 降级说明：原版 vt[5Ch]/[60h]/[74h]/[7Ch]/[80h] 是 DirectDraw 表面接口，
    // 本工程的 Surface 用同名虚函数 Lock/Unlock/GetPitch/GetWidth/GetHeight
    // 一一对应，语义不变。
    // ---------------------------------------------------------------------
    bool Draw_Confined_PCX(const Rectangle* pRect, Surface* pOther, int16 colorKey)
    {
        void* const pDest = Lock(0, 0);
        if (pDest == nullptr)
            return false;                              // asm: xor al,al / retn 10h

        void* const pSrc = (pOther != nullptr) ? pOther->Lock(0, 0) : nullptr;
        if (pSrc == nullptr)
        {
            Unlock();                                  // asm: call [eax+60h]
            return false;
        }

        const int32 w2   = pOther->GetWidth();
        const int32 h2   = pOther->GetHeight();
        const int32 half = GetPitch() / 2;             // asm: cdq / sub / sar 1
        const int32 rx   = (pRect != nullptr) ? pRect->X : 0;
        const int32 ry   = (pRect != nullptr) ? pRect->Y : 0;

        const uint16 key = static_cast<uint16>(colorKey);

        uint8*       dst = static_cast<uint8*>(pDest) + 2 * (ry * half + rx);
        const uint8* src = static_cast<const uint8*>(pSrc);

        for (int32 row = 0; row < h2; ++row)
        {
            const uint16* s = reinterpret_cast<const uint16*>(src);
            uint16*       d = reinterpret_cast<uint16*>(dst);
            for (int32 i = 0; i < w2; ++i)
            {
                const uint16 px = s[i];
                if (px != key)
                    d[i] = px;
            }
            src += 2 * static_cast<uint32>(w2);
            dst += 2 * half;
        }

        Unlock();                                      // asm: loc_6BA65C
        pOther->Unlock();
        return true;
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

    // ---------------------------------------------------------------------
    // 深度缓冲（z 缓冲）挂载点
    //
    // asm: 原版的 z 缓冲**不是** Surface 自己分配的，而是一个独立的全局
    //   对象（gamemd 的 dword_887644，见本文件末尾的 ZBufferClass），由
    //   DisplayClass::Set_View_Dimensions 在视口变更时整批销毁重建：
    //
    //     ; void __fastcall DisplayClass::Set_View_Dimensions(LTRB* a1)
    //     push  offset aDeletingZbuffe   ; "Deleting ZBuffer\n"
    //     ...
    //     push  30h ; new                ; 0x30 = ZBufferClass
    //     call  sub_7BC970               ; ZBufferClass::ZBufferClass(LTRB)
    //     mov   dword ptr [eax+24h], 8000h
    //     push  offset aAllocatingZbuf   ; "Allocating ZBuffer (%dx%d)\n"
    //
    //   尺寸取的是**视口**矩形（rect_mouseclass_vp）而不是整张面，所以
    //   z 的坐标空间是"视口坐标"，与屏幕坐标差一个 (Left, Top)。
    //
    //   提笔器（Blitter.h 的 ZRead 族）每行只拿到一个 "uint16* zbuf"，
    //   因此把基址/行距/视口原点摊在这几个字段上，由 ZBufferClass::Bind
    //   写入目标面。原版同一份数据存在 ZBufferClass 的 +18h/+28h/+4，
    //   这里只是把它们镜像到绘制面，方便 CC_Draw_Shape 按行取指针。
    // ---------------------------------------------------------------------
    uint16* Get_Z_Buffer() const { return ZBuffer; }
    void Set_Z_Buffer(uint16* pBuffer) { ZBuffer = pBuffer; }

    int32 Get_Z_Pitch() const { return ZPitch; }
    int32 Get_Z_Width() const { return ZWidth; }
    int32 Get_Z_Height() const { return ZHeight; }
    Point2D Get_Z_Origin() const { return Point2D(ZLeft, ZTop); }
    uint16 Get_Z_Base() const { return ZBase; }

    // 屏幕坐标 -> 该行 z 起始指针；不在视口内返回 nullptr（= 本行不测）
    uint16* Get_Z_Line(int32 x, int32 y) const
    {
        if (ZBuffer == nullptr) return nullptr;
        const int32 zx = x - ZLeft;
        const int32 zy = y - ZTop;
        if (zx < 0 || zy < 0 || zx >= ZWidth || zy >= ZHeight) return nullptr;
        return ZBuffer + static_cast<size_t>(zy) * static_cast<size_t>(ZPitch) + zx;
    }

    // ---------------------------------------------------------------------
    // asm: ZBufferClass 的 z 值公式（BSurface_DrawSHPFrame / sub_4373B0 同形）
    //
    //   loc_437C39:
    //     mov  si, [eax+24h]      ; si = 8000h            (z 基准)
    //     add  si, [eax+4]        ; si += ZBuffer->top
    //     sub  si, [edi+4]        ; si -= rect.Y
    //     sub  si, [edx+0Ch]      ; si -= 帧高
    //     sub  si, [edx+4]        ; si -= 帧 Y 偏移
    //     inc  esi                ; si += 1
    //     and  esi, 0FFFFh        ; 16 位回绕
    //     add  esi, [esp+a8]      ; si += ZAdjust
    //
    //   即 z = 8000h + 视口top - 精灵**底边**屏幕 Y + 1 + ZAdjust。
    //   屏幕 Y 越大（等距视图里越靠前）z 越小 —— 与提笔器的
    //   `cmp z, *zbuf / jge 跳过`（小 = 近）一致。
    // ---------------------------------------------------------------------
    uint16 Compute_Z(int32 bottomScreenY, int32 zAdjust = 0) const
    {
        const int32 z = static_cast<int32>(ZBase) + ZTop - bottomScreenY + 1 + zAdjust;
        return static_cast<uint16>(z & 0xFFFF);
    }

    // 整个 z 缓冲填成 value（asm: 新建时 FillRect(0xFFFF)，见 sub_7BC970）
    void Clear_Z(uint16 value = 0xFFFF)
    {
        if (ZBuffer == nullptr || ZPitch <= 0 || ZWidth <= 0 || ZHeight <= 0) return;
        if (value == 0xFFFF)
        {
            for (int32 y = 0; y < ZHeight; ++y)
                memset(ZBuffer + static_cast<size_t>(y) * static_cast<size_t>(ZPitch),
                       0xFF, static_cast<size_t>(ZWidth) * sizeof(uint16));
            return;
        }
        for (int32 y = 0; y < ZHeight; ++y)
        {
            uint16* line = ZBuffer + static_cast<size_t>(y) * static_cast<size_t>(ZPitch);
            for (int32 x = 0; x < ZWidth; ++x)
                line[x] = value;
        }
    }

    // 视口坐标下的矩形清除（asm: sub_7BCFB0 —— 逐行修 0FFFFh）
    void Clear_Z_Rect(const Rectangle& rect)
    {
        if (ZBuffer == nullptr) return;
        const Rectangle r = rect.Intersection(Rectangle(0, 0, ZWidth, ZHeight));
        if (r.IsEmpty()) return;
        for (int32 y = r.Y; y < r.Y + r.Height; ++y)
        {
            uint16* line = ZBuffer + static_cast<size_t>(y) * static_cast<size_t>(ZPitch) + r.X;
            for (int32 x = 0; x < r.Width; ++x)
                line[x] = 0xFFFF;
        }
    }

    // Properties
    int32 Width;
    int32 Height;
    int32 Pitch;
    void* Buffer;
    bool IsAllocated;
    int32 PixelFormat;

    // z 缓冲视图（0 = 没有 z 缓冲，提笔器按"不做深度测试"处理）
    uint16* ZBuffer = nullptr;   // 视口左上角对应的 z 像素
    int32 ZPitch = 0;            // 行距，单位 uint16
    int32 ZWidth = 0;
    int32 ZHeight = 0;
    int32 ZLeft = 0;             // 视口原点（屏幕坐标）
    int32 ZTop = 0;
    uint16 ZBase = 0x8000;
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

    // ------------------------------------------------------------------------
    // asm: SurfaceClass::GetBytesPerPixel —— 原版直接返回 [this+14h]。
    //
    //   本树基类 Surface::GetBytesPerPixel 硬编码 return 1，而 XSurface 明明
    //   持有 BytesPerPixel 字段（DSurface::Allocate 会把它设成 2）。结果是
    //   所有走虚函数取像素宽度的通用例程（FillRectEx、CopyFromPart、Lock、
    //   SetPixelAlpha）都把 16bpp 面当成 8bpp 处理：填充退化成单字节 memset、
    //   行偏移错一半，整片画面成碎石块。覆写成本字段就是修这个根因。
    //   Pitch/Width 的反推只作为字段尚未初始化时的兜底。
    // ------------------------------------------------------------------------
    virtual int32 GetBytesPerPixel() override
    {
        if (BytesPerPixel > 0)
            return BytesPerPixel;
        if (Width > 0 && Pitch > 0 && (Pitch % Width) == 0)
            return Pitch / Width;
        return 1;
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

    // ------------------------------------------------------------------------
    // asm: DSurfaceClass::DSurfaceClass (0x4BA5AC) —— 四参构造
    //
    //   a2 = 宽 -> [this+04h]      a3 = 高 -> [this+08h]
    //   a4 == 1  -> DDSURFACEDESC.dwFlags |= DDSCAPS_SYSTEMMEMORY
    //   a5 != 0 且 byte_8A0DEF != 0 -> dwFlags |= DDSCAPS_OFFSCREENPLAIN|3DDEVICE
    //
    //   构造末尾由 DDSCAPS_VIDEOMEMORY 位反推 [this+19h]（本类 IsInVideoRam），
    //   即 bEnable3D && !bSystemMemory 的两种组合正好对应 RecreateSurfaces
    //   里 "VRAM" / "SYSTEM MEMORY" 七处打印分布。
    // ------------------------------------------------------------------------
    DSurface(int32 nWidth, int32 nHeight, bool bSystemMemory, bool bEnable3D)
        : XSurface(nWidth, nHeight)
    {
        IsAllocated = false;
        IsInVideoRam = false;
        VideoSurfacePtr = nullptr;
        VideoSurfaceDescription = nullptr;

        Allocate(nWidth, nHeight);              // asm: lpDD->CreateSurface(...)
        IsInVideoRam = bEnable3D && !bSystemMemory;   // asm: mov [esi+19h],dl
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
// ZBufferClass - 战术视口的深度缓冲（asm: gamemd 的 dword_887644）
//
// 反汇编依据（逐字段对照 sub_7BC970，0x7BC970，__thiscall，retn 10h）：
//
//   ; SomeStruct30h *__thiscall sub_7BC970(this, Rectangle rect)
//     mov  edi, [esp+rect]      ; rect.left
//     mov  ecx, [esp+arg_8]     ; rect.top
//     mov  eax, [esp+arg_C]     ; rect.bottom(=高)
//     mov  [esi+2Ch], eax       ; +2Ch  -> 高
//     mov  [edx],   edi         ; +00h  -> Left
//     mov  [esi+28h], ecx       ; +28h  -> 宽
//     mov  [edx+4], edi         ; +04h  -> Top
//     mov  [edx+8], ecx         ; +08h  -> 宽
//     mov  [edx+0Ch], eax       ; +0Ch  -> 高
//     imul eax, ecx             ; 高*宽
//     shl  eax, 1               ; *2 字节
//     mov  [esi+20h], eax       ; +20h  -> 缓冲总字节（回绕量）
//     push 20h / call new       ; +14h  -> new 出来的 BSurface（0x20 字节）
//         mov [edi+4], 宽 ; mov [edi+8], 高 ; mov [edi+0Ch], 0
//         mov [edi+10h], 2      ; 每像素 2 字节
//         call CCFileClass_AllocateMemory(0, 宽*高*2)
//     push 0FFFFh ; push {0,0,宽,高} ; call [vt+14h]   ; FillRect(0xFFFF) 清整片
//     mov  ecx, [esi+2Ch] / imul ecx, [esi+28h]
//     mov  [esi+1Ch], eax+ecx*2 ; +1Ch -> 缓冲末尾
//     mov  dword ptr [esi+24h], 8000h ; +24h -> z 基准 0x8000
//
// 结论：
//   * z 是**独立的 16bpp 面**，不是像素面的一部分，也不跟在像素后面；
//   * 每像素 2 字节（uint16），行距 = 视口宽 * 2（无额外对齐）；
//   * 尺寸 = 视口宽 × 视口高（不是整张面）；
//   * 初值 / 清屏值 = 0xFFFF（"无限远"），+24h 的 0x8000 是 z 公式的基准；
//   * 生命周期跟随视口：DisplayClass::Set_View_Dimensions 里先删后建。
//
// 每帧的清零在 asm 里是按**脏区**做的（sub_6D2B60 里对 vec_DirtyAreas 的
// 每一项算它与 rect_mouseclass_vp 的交，再 sub_7BCFB0(zbuf, rect) 填
// 0FFFFh）。本树没有脏区向量，Draw() 本身是全量重绘，因此用整片清零等价
// 覆盖：只要本帧重画的区域与脏区一致，z 内容就一致。
// ============================================================================
class ZBufferClass
{
public:
    explicit ZBufferClass(const Rectangle& view);
    ~ZBufferClass();

    ZBufferClass(const ZBufferClass&) = delete;
    ZBufferClass& operator=(const ZBufferClass&) = delete;

    bool Is_Allocated() const { return Buffer != nullptr && Base != nullptr; }

    // asm: FillRect(0xFFFF) —— 整片清成"无限远"
    void Clear();

    // asm: sub_7BCFB0(zbuf, x, y, w, h) —— 视口坐标下的矩形清除
    void Clear_Rect(int32 x, int32 y, int32 w, int32 h);

    // asm: sub_7BD130 —— Lock(x, y - top) + [+10h]，再做一次回绕
    uint16* Lock(int32 x, int32 y);
    void Unlock();

    // 把本缓冲挂到绘制面上（供 CC_Draw_Shape 按行取 zbuf）
    void Bind(Surface* pTarget);

    // asm: sub_7BC970 的字段顺序（单位：屏幕坐标 / 像素）
    int32      Left;        // +00h
    int32      Top;         // +04h
    int32      Width;       // +08h
    int32      Height;      // +0Ch
    int32      Offset;      // +10h  锁定后额外加的字节偏移
    BSurface*  Buffer;      // +14h  真正的 16bpp 面
    uint16*    Base;        // +18h  锁定基址
    uint16*    End;         // +1Ch  基址 + 宽*高*2
    uint32     Wrap;        // +20h  宽*高*2（指针回绕量）
    uint16     ZBase;       // +24h  0x8000

    // 本树额外：当前挂着本缓冲的绘制面（析构时要退挂，避免悬空指针）
    Surface*   Target;
};

// asm: .data 的 dword_887644 —— 全局唯一的战术视口 z 缓冲。
extern ZBufferClass* ZBuffer_Global;

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

// ============================================================================
// asm: Plain_Text_Print (0x4A6710, ecx = out Point2D*, edx = Surface*, retn 20h)
//
//   调用点：TechnoClass::Draw_Pips 画 pips 数字。
//
//   if (schemeIdx == -1):
//       DSurface_PrintText(out, surf, x, y, text, rawColor, font, a14, a1C)
//   else:
//       entry = ColorScheme.data[schemeIdx] + 308h
//       rgb   = sub_517440(entry, &schemeIdx)          ; 取该色阶的 RGB
//       c16   = ((rgb.R >> col_R_SHR_3) << col_R_SHL_11)
//             | ((rgb.G >> col_G_SHR_2) << col_G_SHL_5)
//             | ((rgb.B >> col_B_SHR_3) << col_B_SHL_0)
//       DSurface_PrintText(..., c16, ...)
//   最后把 DSurface_PrintText 返回的两 dword 拷回 *out，返回 out。
//
//   降级点：
//     * font / a14 是原版字体的"字体句柄 + 旗标"槽，本项目文本走
//       Fancy_Text_Print_Wide 的 TextPrintType，两个参数不参与（保留形参）。
//     * ColorScheme 没有逐色阶 RGB 表（原版 +308h 那张），这里用
//       BaseColor 换算，色阶下标不参与。
// ============================================================================
Point2D* Plain_Text_Print(Point2D* pOut, Surface* pSurface, int32 x, int32 y,
                          const char* pText, int32 rawColor, int32 font,
                          int32 a14, int32 a1C, int32 schemeIdx);

void CC_Draw_Shape(
    Surface* pSurface, ConvertClass* Palette, SHPStruct* SHP, int32 FrameIndex,
    const Point2D* Position, const Rectangle* Bounds, BlitterFlags Flags,
    int32 Remap, int32 ZAdjust, int32 ZGradientDescIndex,
    int32 Brightness, int32 TintColor,
    SHPStruct* ZShape, int32 ZShapeFrame, int32 XOffset, int32 YOffset);

// ============================================================================
// 全局离屏表面（asm .data 的五个 DSurfaceClass* 槽）
//
//   五张面由 DisplayClass::RecreateSurfaces 在每次视频模式切换时整批销毁
//   重建：Alternate / Hidden 供战术层双缓冲，Composite 是战术层与侧边栏的
//   合成面，Tile 是地图层，Sidebar 是侧边栏层。Hidden 另在装载 / 进度画面上
//   承担"先画到离屏面再整体翻到主面"的中转角色。原版是
//   `mov ecx, DSurface_X` 直接取的引擎级全局，不走任何注入点；
//   定义集中在 Rendering/Surface.cpp。
// ============================================================================
extern DSurface* DSurface_Alternate;
extern DSurface* DSurface_Hidden;
extern DSurface* DSurface_Composite;
extern DSurface* DSurface_Tile;
extern DSurface* DSurface_Sidebar;

// asm: ChronoScreenEffectSurf —— 超时空传送全屏特效用的中转面。原版是 .data
//   里独立的 Surface* 槽，随特效建立 / 拆除（Chrono 特效段），并在读档的
//   sub_539AE0 末尾无条件清空。
extern DSurface* ChronoScreenEffectSurf;