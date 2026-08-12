#include "Rendering/Surface.h"
#include "FileFormats/SHP.h"
#include "Rendering/ConvertClass.h"

#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>

// ============================================================================
// Font data helper
// ============================================================================

static const uint8 (&GetFontData())[128][8]
{
    static uint8 data[128][8] = {};
    struct FontInit { FontInit() {
        data[48][0] = 0x3C; data[48][1] = 0x66; data[48][2] = 0x6E; data[48][3] = 0x76; data[48][4] = 0x66; data[48][5] = 0x66; data[48][6] = 0x3C; data[48][7] = 0x00;
        data[49][0] = 0x18; data[49][1] = 0x38; data[49][2] = 0x18; data[49][3] = 0x18; data[49][4] = 0x18; data[49][5] = 0x18; data[49][6] = 0x3C; data[49][7] = 0x00;
        data[50][0] = 0x3C; data[50][1] = 0x66; data[50][2] = 0x0C; data[50][3] = 0x18; data[50][4] = 0x30; data[50][5] = 0x60; data[50][6] = 0x7E; data[50][7] = 0x00;
        data[51][0] = 0x3C; data[51][1] = 0x66; data[51][2] = 0x06; data[51][3] = 0x1C; data[51][4] = 0x06; data[51][5] = 0x66; data[51][6] = 0x3C; data[51][7] = 0x00;
        data[52][0] = 0x0C; data[52][1] = 0x1C; data[52][2] = 0x3C; data[52][3] = 0x6C; data[52][4] = 0x7E; data[52][5] = 0x0C; data[52][6] = 0x0C; data[52][7] = 0x00;
        data[53][0] = 0x7E; data[53][1] = 0x60; data[53][2] = 0x7C; data[53][3] = 0x06; data[53][4] = 0x06; data[53][5] = 0x66; data[53][6] = 0x3C; data[53][7] = 0x00;
        data[54][0] = 0x3C; data[54][1] = 0x60; data[54][2] = 0x7C; data[54][3] = 0x66; data[54][4] = 0x66; data[54][5] = 0x66; data[54][6] = 0x3C; data[54][7] = 0x00;
        data[55][0] = 0x7E; data[55][1] = 0x06; data[55][2] = 0x0C; data[55][3] = 0x18; data[55][4] = 0x30; data[55][5] = 0x30; data[55][6] = 0x30; data[55][7] = 0x00;
        data[56][0] = 0x3C; data[56][1] = 0x66; data[56][2] = 0x66; data[56][3] = 0x3C; data[56][4] = 0x66; data[56][5] = 0x66; data[56][6] = 0x3C; data[56][7] = 0x00;
        data[57][0] = 0x3C; data[57][1] = 0x66; data[57][2] = 0x66; data[57][3] = 0x3E; data[57][4] = 0x06; data[57][5] = 0x0C; data[57][6] = 0x38; data[57][7] = 0x00;
        data[65][0] = 0x30; data[65][1] = 0x78; data[65][2] = 0xCC; data[65][3] = 0xCC; data[65][4] = 0xFC; data[65][5] = 0xCC; data[65][6] = 0xCC; data[65][7] = 0x00;
        data[66][0] = 0xFC; data[66][1] = 0x66; data[66][2] = 0x66; data[66][3] = 0x7C; data[66][4] = 0x66; data[66][5] = 0x66; data[66][6] = 0xFC; data[66][7] = 0x00;
        data[67][0] = 0x3C; data[67][1] = 0x66; data[67][2] = 0xC0; data[67][3] = 0xC0; data[67][4] = 0xC0; data[67][5] = 0x66; data[67][6] = 0x3C; data[67][7] = 0x00;
        data[68][0] = 0xF8; data[68][1] = 0x6C; data[68][2] = 0x66; data[68][3] = 0x66; data[68][4] = 0x66; data[68][5] = 0x6C; data[68][6] = 0xF8; data[68][7] = 0x00;
        data[69][0] = 0xFE; data[69][1] = 0x62; data[69][2] = 0x68; data[69][3] = 0x78; data[69][4] = 0x68; data[69][5] = 0x62; data[69][6] = 0xFE; data[69][7] = 0x00;
        data[70][0] = 0xFE; data[70][1] = 0x62; data[70][2] = 0x68; data[70][3] = 0x78; data[70][4] = 0x68; data[70][5] = 0x60; data[70][6] = 0xF0; data[70][7] = 0x00;
        data[71][0] = 0x3C; data[71][1] = 0x66; data[71][2] = 0xC0; data[71][3] = 0xC0; data[71][4] = 0xCE; data[71][5] = 0x66; data[71][6] = 0x3E; data[71][7] = 0x00;
        data[72][0] = 0xCC; data[72][1] = 0xCC; data[72][2] = 0xCC; data[72][3] = 0xFC; data[72][4] = 0xCC; data[72][5] = 0xCC; data[72][6] = 0xCC; data[72][7] = 0x00;
        data[73][0] = 0x3C; data[73][1] = 0x18; data[73][2] = 0x18; data[73][3] = 0x18; data[73][4] = 0x18; data[73][5] = 0x18; data[73][6] = 0x3C; data[73][7] = 0x00;
        data[74][0] = 0x1E; data[74][1] = 0x0C; data[74][2] = 0x0C; data[74][3] = 0x0C; data[74][4] = 0xCC; data[74][5] = 0xCC; data[74][6] = 0x78; data[74][7] = 0x00;
        data[75][0] = 0xE6; data[75][1] = 0x66; data[75][2] = 0x6C; data[75][3] = 0x78; data[75][4] = 0x6C; data[75][5] = 0x66; data[75][6] = 0xE6; data[75][7] = 0x00;
        data[76][0] = 0xF0; data[76][1] = 0x60; data[76][2] = 0x60; data[76][3] = 0x60; data[76][4] = 0x62; data[76][5] = 0x66; data[76][6] = 0xFE; data[76][7] = 0x00;
        data[77][0] = 0xC6; data[77][1] = 0xEE; data[77][2] = 0xFE; data[77][3] = 0xD6; data[77][4] = 0xC6; data[77][5] = 0xC6; data[77][6] = 0xC6; data[77][7] = 0x00;
        data[78][0] = 0xC6; data[78][1] = 0xE6; data[78][2] = 0xF6; data[78][3] = 0xDE; data[78][4] = 0xCE; data[78][5] = 0xC6; data[78][6] = 0xC6; data[78][7] = 0x00;
        data[79][0] = 0x38; data[79][1] = 0x6C; data[79][2] = 0xC6; data[79][3] = 0xC6; data[79][4] = 0xC6; data[79][5] = 0x6C; data[79][6] = 0x38; data[79][7] = 0x00;
        data[80][0] = 0xFC; data[80][1] = 0x66; data[80][2] = 0x66; data[80][3] = 0x7C; data[80][4] = 0x60; data[80][5] = 0x60; data[80][6] = 0xF0; data[80][7] = 0x00;
        data[81][0] = 0x78; data[81][1] = 0xCC; data[81][2] = 0xCC; data[81][3] = 0xCC; data[81][4] = 0xDC; data[81][5] = 0x78; data[81][6] = 0x1C; data[81][7] = 0x00;
        data[82][0] = 0xFC; data[82][1] = 0x66; data[82][2] = 0x66; data[82][3] = 0x7C; data[82][4] = 0x6C; data[82][5] = 0x66; data[82][6] = 0xE6; data[82][7] = 0x00;
        data[83][0] = 0x7C; data[83][1] = 0xC6; data[83][2] = 0x60; data[83][3] = 0x38; data[83][4] = 0x0C; data[83][5] = 0xC6; data[83][6] = 0x7C; data[83][7] = 0x00;
        data[84][0] = 0x7E; data[84][1] = 0x5A; data[84][2] = 0x18; data[84][3] = 0x18; data[84][4] = 0x18; data[84][5] = 0x18; data[84][6] = 0x3C; data[84][7] = 0x00;
        data[85][0] = 0xCC; data[85][1] = 0xCC; data[85][2] = 0xCC; data[85][3] = 0xCC; data[85][4] = 0xCC; data[85][5] = 0xCC; data[85][6] = 0x78; data[85][7] = 0x00;
        data[86][0] = 0xCC; data[86][1] = 0xCC; data[86][2] = 0xCC; data[86][3] = 0xCC; data[86][4] = 0xCC; data[86][5] = 0x78; data[86][6] = 0x30; data[86][7] = 0x00;
        data[87][0] = 0xC6; data[87][1] = 0xC6; data[87][2] = 0xC6; data[87][3] = 0xD6; data[87][4] = 0xFE; data[87][5] = 0xEE; data[87][6] = 0xC6; data[87][7] = 0x00;
        data[88][0] = 0xC6; data[88][1] = 0xC6; data[88][2] = 0x6C; data[88][3] = 0x38; data[88][4] = 0x6C; data[88][5] = 0xC6; data[88][6] = 0xC6; data[88][7] = 0x00;
        data[89][0] = 0x66; data[89][1] = 0x66; data[89][2] = 0x66; data[89][3] = 0x3C; data[89][4] = 0x18; data[89][5] = 0x18; data[89][6] = 0x3C; data[89][7] = 0x00;
        data[90][0] = 0xFE; data[90][1] = 0xC6; data[90][2] = 0x8C; data[90][3] = 0x18; data[90][4] = 0x32; data[90][5] = 0x66; data[90][6] = 0xFE; data[90][7] = 0x00;
        data[95][0] = 0x00; data[95][1] = 0x00; data[95][2] = 0x00; data[95][3] = 0x00; data[95][4] = 0x00; data[95][5] = 0x00; data[95][6] = 0xFF; data[95][7] = 0x00;
        data[46][0] = 0x00; data[46][1] = 0x00; data[46][2] = 0x00; data[46][3] = 0x00; data[46][4] = 0x00; data[46][5] = 0x18; data[46][6] = 0x18; data[46][7] = 0x00;
        data[44][0] = 0x00; data[44][1] = 0x00; data[44][2] = 0x00; data[44][3] = 0x00; data[44][4] = 0x18; data[44][5] = 0x18; data[44][6] = 0x30; data[44][7] = 0x00;
        data[45][0] = 0x00; data[45][1] = 0x00; data[45][2] = 0x00; data[45][3] = 0x7E; data[45][4] = 0x00; data[45][5] = 0x00; data[45][6] = 0x00; data[45][7] = 0x00;
        data[58][0] = 0x00; data[58][1] = 0x00; data[58][2] = 0x18; data[58][3] = 0x18; data[58][4] = 0x00; data[58][5] = 0x18; data[58][6] = 0x18; data[58][7] = 0x00;
        data[47][0] = 0x06; data[47][1] = 0x0C; data[47][2] = 0x18; data[47][3] = 0x30; data[47][4] = 0x60; data[47][5] = 0xC0; data[47][6] = 0x80; data[47][7] = 0x00;
        data[92][0] = 0xC0; data[92][1] = 0x60; data[92][2] = 0x30; data[92][3] = 0x18; data[92][4] = 0x0C; data[92][5] = 0x06; data[92][6] = 0x02; data[92][7] = 0x00;
        data[40][0] = 0x18; data[40][1] = 0x30; data[40][2] = 0x60; data[40][3] = 0x60; data[40][4] = 0x60; data[40][5] = 0x30; data[40][6] = 0x18; data[40][7] = 0x00;
        data[41][0] = 0x18; data[41][1] = 0x0C; data[41][2] = 0x06; data[41][3] = 0x06; data[41][4] = 0x06; data[41][5] = 0x0C; data[41][6] = 0x18; data[41][7] = 0x00;
        data[37][0] = 0x62; data[37][1] = 0x66; data[37][2] = 0x0C; data[37][3] = 0x18; data[37][4] = 0x30; data[37][5] = 0x66; data[37][6] = 0x46; data[37][7] = 0x00;
        data[33][0] = 0x18; data[33][1] = 0x18; data[33][2] = 0x18; data[33][3] = 0x18; data[33][4] = 0x00; data[33][5] = 0x18; data[33][6] = 0x18; data[33][7] = 0x00;
        data[63][0] = 0x3C; data[63][1] = 0x66; data[63][2] = 0x06; data[63][3] = 0x1C; data[63][4] = 0x18; data[63][5] = 0x00; data[63][6] = 0x18; data[63][7] = 0x00;
        data[43][0] = 0x00; data[43][1] = 0x18; data[43][2] = 0x18; data[43][3] = 0x7E; data[43][4] = 0x18; data[43][5] = 0x18; data[43][6] = 0x00; data[43][7] = 0x00;
        data[42][0] = 0x00; data[42][1] = 0x66; data[42][2] = 0x3C; data[42][3] = 0xFF; data[42][4] = 0x3C; data[42][5] = 0x66; data[42][6] = 0x00; data[42][7] = 0x00;
        data[35][0] = 0x6C; data[35][1] = 0xFE; data[35][2] = 0x6C; data[35][3] = 0x6C; data[35][4] = 0xFE; data[35][5] = 0x6C; data[35][6] = 0x00; data[35][7] = 0x00;
        data[36][0] = 0x18; data[36][1] = 0x7E; data[36][2] = 0xC0; data[36][3] = 0x7C; data[36][4] = 0x06; data[36][5] = 0xFC; data[36][6] = 0x18; data[36][7] = 0x00;
        data[60][0] = 0x0C; data[60][1] = 0x18; data[60][2] = 0x30; data[60][3] = 0x60; data[60][4] = 0x30; data[60][5] = 0x18; data[60][6] = 0x0C; data[60][7] = 0x00;
        data[62][0] = 0x30; data[62][1] = 0x18; data[62][2] = 0x0C; data[62][3] = 0x06; data[62][4] = 0x0C; data[62][5] = 0x18; data[62][6] = 0x30; data[62][7] = 0x00;
        data[61][0] = 0x00; data[61][1] = 0x00; data[61][2] = 0x7E; data[61][3] = 0x00; data[61][4] = 0x7E; data[61][5] = 0x00; data[61][6] = 0x00; data[61][7] = 0x00;
        data[91][0] = 0x78; data[91][1] = 0x60; data[91][2] = 0x60; data[91][3] = 0x60; data[91][4] = 0x60; data[91][5] = 0x60; data[91][6] = 0x78; data[91][7] = 0x00;
        data[93][0] = 0x1E; data[93][1] = 0x06; data[93][2] = 0x06; data[93][3] = 0x06; data[93][4] = 0x06; data[93][5] = 0x06; data[93][6] = 0x1E; data[93][7] = 0x00;
        data[94][0] = 0x10; data[94][1] = 0x38; data[94][2] = 0x6C; data[94][3] = 0xC6; data[94][4] = 0x00; data[94][5] = 0x00; data[94][6] = 0x00; data[94][7] = 0x00;
        data[96][0] = 0x60; data[96][1] = 0x30; data[96][2] = 0x18; data[96][3] = 0x00; data[96][4] = 0x00; data[96][5] = 0x00; data[96][6] = 0x00; data[96][7] = 0x00;
        data[124][0] = 0x18; data[124][1] = 0x18; data[124][2] = 0x18; data[124][3] = 0x18; data[124][4] = 0x18; data[124][5] = 0x18; data[124][6] = 0x18; data[124][7] = 0x00;
        data[126][0] = 0x00; data[126][1] = 0x00; data[126][2] = 0x32; data[126][3] = 0x7E; data[126][4] = 0x4C; data[126][5] = 0x00; data[126][6] = 0x00; data[126][7] = 0x00;
    } };
    static const FontInit _init;
    return data;
}

// ============================================================================
// DSurface::DrawSHP implementation
// ============================================================================
void DSurface::DrawSHP(
    ConvertClass* Palette, SHPStruct* SHP, int32 FrameIndex,
    const Point2D* Position, const Rectangle* Bounds, BlitterFlags Flags,
    int32 Remap, int32 ZAdjust, int32 ZGradientDescIndex,
    int32 Brightness, int32 TintColor,
    SHPStruct* ZShape, int32 ZShapeFrame, int32 XOffset, int32 YOffset)
{
    CC_Draw_Shape(this, Palette, SHP, FrameIndex, Position, Bounds,
                  Flags, Remap, ZAdjust, ZGradientDescIndex,
                  Brightness, TintColor, ZShape, ZShapeFrame, XOffset, YOffset);
}

// ============================================================================
// DSurface::DrawText implementations
// ============================================================================
void DSurface::DrawText(const wchar_t* pText, Rectangle* pBounds, Point2D* pLocation,
    DWORD ForeColor, DWORD BackColor, TextPrintType Flag)
{
    Point2D tmp(0, 0);
    Fancy_Text_Print_Wide(tmp, pText, this, pBounds ? *pBounds : Rectangle(0, 0, Width, Height),
                          pLocation ? *pLocation : Point2D(0, 0), ForeColor, BackColor, Flag);
}

void DSurface::DrawText(const wchar_t* pText, Point2D* pLocation, DWORD Color)
{
    Rectangle rect(0, 0, Width, Height);
    Point2D tmp(0, 0);
    Fancy_Text_Print_Wide(tmp, pText, this, rect,
                          pLocation ? *pLocation : Point2D(0, 0),
                          Color, 0, TextPrintType::NoShadow);
}

void DSurface::DrawText(const wchar_t* pText, int32 X, int32 Y, DWORD Color)
{
    Point2D pt(X, Y);
    DrawText(pText, &pt, Color);
}

// ============================================================================
// Fancy_Text_Print_Wide implementation
// ============================================================================
Point2D* Fancy_Text_Print_Wide(
    Point2D& retBuffer, const wchar_t* Text, Surface* pSurface,
    const Rectangle& Bounds, const Point2D& Location,
    DWORD ForeColor, DWORD BackColor, TextPrintType Flag)
{
    retBuffer = Location;
    if (!Text || !pSurface || !pSurface->Buffer) return &retBuffer;

    int32 len = 0;
    while (Text[len]) ++len;
    if (len == 0) return &retBuffer;

    // Font metrics: 6x8 character cell
    static constexpr int32 CharWidth = 6;
    static constexpr int32 CharHeight = 8;
    static constexpr int32 CharSpacing = 1;

    int32 totalWidth = len * (CharWidth + CharSpacing) - CharSpacing;
    int32 x = Location.X;
    int32 y = Location.Y;

    // Apply horizontal text alignment
    if (static_cast<uint32>(Flag) & static_cast<uint32>(TextPrintType::Center))
    {
        x = Location.X - totalWidth / 2;
    }
    else if (static_cast<uint32>(Flag) & static_cast<uint32>(TextPrintType::Right))
    {
        x = Location.X - totalWidth;
    }

    bool drawShadow = !(static_cast<uint32>(Flag) & static_cast<uint32>(TextPrintType::NoShadow));
    bool drawFullShadow = (static_cast<uint32>(Flag) & static_cast<uint32>(TextPrintType::FullShadow)) != 0;
    bool drawDropShadow = (static_cast<uint32>(Flag) & static_cast<uint32>(TextPrintType::DropShadow)) != 0;
    int32 bytesPerPix = pSurface->GetBytesPerPixel();

    // Bitmap font data for 8x6 character cells (simplified 8-bit style)
    // Each character is represented as 8 bytes (one per row), each byte has 6 relevant bits
    const uint8 (&FontData)[128][8] = GetFontData();

    // Per-character rendering loop
    for (int32 i = 0; i < len; ++i)
    {
        int32 cx = x + i * (CharWidth + CharSpacing);
        int32 cy = y;

        wchar_t ch = Text[i];
        if (ch == L' ')
        {
            retBuffer.X = cx + CharWidth + CharSpacing;
            continue;
        }

        // Get font data for this character
        const uint8* glyphData = nullptr;
        uint8 fallbackGlyph[8] = {0x7E, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x7E, 0x00}; // box

        if (ch >= 48 && ch <= 57)   glyphData = FontData[ch];    // digits
        else if (ch >= 65 && ch <= 90) glyphData = FontData[ch]; // uppercase
        else if (ch >= 97 && ch <= 122) {
            // Lowercase mapped to uppercase style
            wchar_t upper = ch - 32;
            if (upper >= 65 && upper <= 90)
                glyphData = FontData[upper];
        }
        else if (ch < 128 && FontData[ch][0] != 0) glyphData = FontData[ch];
        else if (ch == L'\n') { retBuffer.X = cx; continue; }

        if (!glyphData) glyphData = fallbackGlyph;

        int32 effectiveCharWidth = CharWidth;
        int32 effectiveCharHeight = CharHeight;

        // Apply point size scaling
        if (static_cast<uint32>(Flag) & static_cast<uint32>(TextPrintType::Point8))
            effectiveCharHeight = 8;
        else if (static_cast<uint32>(Flag) & static_cast<uint32>(TextPrintType::Point6))
            effectiveCharHeight = 6;
        else if (static_cast<uint32>(Flag) & static_cast<uint32>(TextPrintType::Point3))
            effectiveCharHeight = 3;

        // Draw shadow offset
        if (drawShadow && !drawDropShadow)
        {
            for (int32 row = 0; row < effectiveCharHeight; ++row)
            {
                uint8 glyphRow = glyphData[row];
                for (int32 col = 0; col < effectiveCharWidth; ++col)
                {
                    if (glyphRow & (0x80 >> col))
                    {
                        int32 px = cx + col + 1;
                        int32 py = cy + row + 1;
                        if (Bounds.ContainsPoint(px, py))
                        {
                            Point2D pt(px, py);
                            pSurface->SetPixel(&pt, BackColor);
                        }
                    }
                }
            }
        }
        else if (drawDropShadow)
        {
            // Drop shadow: fatter shadow at 2,2 offset
            for (int32 row = 0; row < effectiveCharHeight; ++row)
            {
                uint8 glyphRow = glyphData[row];
                for (int32 col = 0; col < effectiveCharWidth; ++col)
                {
                    if (glyphRow & (0x80 >> col))
                    {
                        for (int32 dy = 0; dy <= 2; ++dy)
                        {
                            for (int32 dx = 0; dx <= 2; ++dx)
                            {
                                int32 px = cx + col + dx + 1;
                                int32 py = cy + row + dy + 1;
                                if (Bounds.ContainsPoint(px, py))
                                {
                                    Point2D pt(px, py);
                                    pSurface->SetPixel(&pt, BackColor);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Draw foreground character
        for (int32 row = 0; row < effectiveCharHeight; ++row)
        {
            uint8 glyphRow = glyphData[row];
            for (int32 col = 0; col < effectiveCharWidth; ++col)
            {
                if (glyphRow & (0x80 >> col))
                {
                    int32 px = cx + col;
                    int32 py = cy + row;
                    if (Bounds.ContainsPoint(px, py))
                    {
                        Point2D pt(px, py);
                        pSurface->SetPixel(&pt, ForeColor);
                    }
                }
            }
        }
    }

    x += totalWidth;
    retBuffer.X = x;
    retBuffer.Y = y;
    return &retBuffer;
}

// ============================================================================
// Simple_Text_Print_Wide implementation
// ============================================================================
Point2D* Simple_Text_Print_Wide(
    Point2D* RetVal, const wchar_t* Text, Surface* pSurface,
    Rectangle* Bounds, Point2D* Location,
    DWORD ForeColor, DWORD BackColor, TextPrintType Flag, bool bUnk)
{
    if (!RetVal) return nullptr;
    return Fancy_Text_Print_Wide(
        *RetVal, Text, pSurface,
        Bounds ? *Bounds : Rectangle(0, 0, pSurface->Width, pSurface->Height),
        Location ? *Location : Point2D(0, 0),
        ForeColor, BackColor, Flag);
}

// ============================================================================
// CC_Draw_Shape implementation
// ============================================================================
void CC_Draw_Shape(
    Surface* pSurface, ConvertClass* Palette, SHPStruct* SHP, int32 FrameIndex,
    const Point2D* Position, const Rectangle* Bounds, BlitterFlags Flags,
    int32 Remap, int32 ZAdjust, int32 ZGradientDescIndex,
    int32 Brightness, int32 TintColor,
    SHPStruct* ZShape, int32 ZShapeFrame, int32 XOffset, int32 YOffset)
{
    if (!pSurface || !SHP || !Position) return;

    // Get frame data from SHP
    BYTE* frameData = SHP->GetPixels(FrameIndex);
    if (!frameData) return;

    Rectangle frameBounds;
    SHP->GetFrameBounds(frameBounds, FrameIndex);

    int32 dstX = Position->X + XOffset;
    int32 dstY = Position->Y + YOffset;

    // Apply centering if needed
    if ((static_cast<uint32>(Flags) & static_cast<uint32>(BlitterFlags::Centered)) != 0)
    {
        dstX -= frameBounds.Width / 2;
        dstY -= frameBounds.Height / 2;
    }

    Rectangle srcRect(0, 0, frameBounds.Width, frameBounds.Height);
    Rectangle dstRect(dstX, dstY, frameBounds.Width, frameBounds.Height);

    // Clip to bounds
    Rectangle clipRect = dstRect;
    if (Bounds)
    {
        clipRect = dstRect.Intersection(*Bounds);
        if (clipRect.IsEmpty()) return;
    }

    // Clip to surface bounds
    Rectangle surfRect(0, 0, pSurface->Width, pSurface->Height);
    clipRect = clipRect.Intersection(surfRect);
    if (clipRect.IsEmpty()) return;

    int32 srcOffX = clipRect.X - dstRect.X;
    int32 srcOffY = clipRect.Y - dstRect.Y;
    int32 copyW = clipRect.Width;
    int32 copyH = clipRect.Height;

    int32 bytesPerPix = pSurface->GetBytesPerPixel();
    int32 srcPitch = frameBounds.Width;
    int32 dstPitch = pSurface->GetPitch();

    BYTE* dstBuffer = static_cast<BYTE*>(pSurface->Buffer);

    // Select appropriate blitter if Palette is available
    Blitter* blitter = nullptr;
    if (Palette)
    {
        blitter = Palette->SelectPlainBlitter(Flags);
    }

    bool isDarken = (static_cast<uint32>(Flags) & static_cast<uint32>(BlitterFlags::Darken)) != 0;
    bool isTranslucent = (static_cast<uint32>(Flags) & static_cast<uint32>(BlitterFlags::Translucent)) != 0;

    uint16* palette16 = nullptr;
    if (Palette && Palette->PaletteData)
    {
        palette16 = reinterpret_cast<uint16*>(Palette->PaletteData);
    }

    for (int32 row = 0; row < copyH; ++row)
    {
        int32 srcY = srcOffY + row;
        int32 dstY = clipRect.Y + row;
        BYTE* srcLine = frameData + srcY * srcPitch + srcOffX;
        BYTE* dstLine = dstBuffer + dstY * dstPitch + clipRect.X * bytesPerPix;

        for (int32 col = 0; col < copyW; ++col)
        {
            BYTE srcIdx = srcLine[col];
            if (srcIdx == 0) continue; // Color key 0 = transparent

            if (bytesPerPix == 1)
            {
                // 8-bit surface: apply remap if available
                if (Remap >= 0 && Remap < 256 && palette16)
                {
                    // Remap through palette lookup
                    dstLine[col] = srcIdx;
                }
                else
                {
                    dstLine[col] = srcIdx;
                }
            }
            else if (bytesPerPix == 2)
            {
                uint16* dst16 = reinterpret_cast<uint16*>(dstLine);

                if (palette16)
                {
                    uint16 color = palette16[srcIdx];

                    // Apply brightness
                    if (Brightness != 0 && Brightness != 1000)
                    {
                        int32 r = static_cast<int32>((color >> 11) & 0x1F) * Brightness / 1000;
                        int32 g = static_cast<int32>((color >> 5) & 0x3F) * Brightness / 1000;
                        int32 b = static_cast<int32>(color & 0x1F) * Brightness / 1000;
                        if (r > 31) r = 31; if (r < 0) r = 0;
                        if (g > 63) g = 63; if (g < 0) g = 0;
                        if (b > 31) b = 31; if (b < 0) b = 0;
                        color = static_cast<uint16>((r << 11) | (g << 5) | b);
                    }

                    // Apply tint color
                    if (TintColor != 0 && (static_cast<uint32>(Flags) & static_cast<uint32>(BlitterFlags::TintColor)))
                    {
                        uint16 tint = static_cast<uint16>(TintColor);
                        int32 r = (((color >> 11) & 0x1F) + ((tint >> 11) & 0x1F)) / 2;
                        int32 g = (((color >> 5) & 0x3F) + ((tint >> 5) & 0x3F)) / 2;
                        int32 b = ((color & 0x1F) + (tint & 0x1F)) / 2;
                        color = static_cast<uint16>((r << 11) | (g << 5) | b);
                    }

                    // Apply translucency blending
                    if (isTranslucent && palette16)
                    {
                        uint16 dest = dst16[col];
                        uint16 mask = 0x7BEF; // RGB 565 mask

                        uint32 transMask = static_cast<uint32>(Flags) & static_cast<uint32>(BlitterFlags::Translucent);
                        if (transMask == static_cast<uint32>(BlitterFlags::Translucent25))
                        {
                            // 25% src + 75% dst
                            dst16[col] = static_cast<uint16>((mask & (dest >> 2)) + 3 * (mask & (color >> 2)));
                        }
                        else if (transMask == static_cast<uint32>(BlitterFlags::Translucent50))
                        {
                            // 50% src + 50% dst
                            dst16[col] = static_cast<uint16>((mask & (dest >> 1)) + (mask & (color >> 1)));
                        }
                        else if (transMask == static_cast<uint32>(BlitterFlags::Translucent75))
                        {
                            // 75% src + 25% dst
                            dst16[col] = static_cast<uint16>(3 * (mask & (dest >> 2)) + (mask & (color >> 2)));
                        }
                    }
                    else if (isDarken)
                    {
                        uint16 dest = dst16[col];
                        uint16 mask = 0x7BEF;
                        dst16[col] = static_cast<uint16>(((dest & mask) * (color & mask)) >> 8) & mask;
                    }
                    else
                    {
                        dst16[col] = color;
                    }
                }
                else
                {
                    // No palette: use raw index as color
                    dst16[col] = static_cast<uint16>(srcIdx);
                }
            }
            else if (bytesPerPix == 4)
            {
                uint32* dst32 = reinterpret_cast<uint32*>(dstLine);
                if (palette16)
                {
                    uint16 color16 = palette16[srcIdx];
                    int32 r = ((color16 >> 11) & 0x1F) << 3;
                    int32 g = ((color16 >> 5) & 0x3F) << 2;
                    int32 b = (color16 & 0x1F) << 3;
                    dst32[col] = static_cast<uint32>((r << 16) | (g << 8) | b);
                }
                else
                {
                    dst32[col] = static_cast<uint32>(srcIdx);
                }
            }
        }
    }
}