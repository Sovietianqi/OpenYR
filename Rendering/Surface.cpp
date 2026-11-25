#include "Rendering/Surface.h"
#include "FileFormats/SHP.h"
#include "Rendering/ConvertClass.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <algorithm>

// asm: g_bDebugMode (Game/Globals.cpp, driven by OPENYR_DEBUG). Declared here
// instead of pulling in Game/Externs.h, which would drag every game class into
// this translation unit.
extern bool g_bDebugMode;

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
// asm: Plain_Text_Print (0x4A6710)
//
//   if (schemeIdx == -1): 直接用调用者给的 rawColor
//   else:                 取 ColorScheme.data[schemeIdx] 的 RGB，
//                         按 5-6-5 打包成 16 位色
//   然后调 DSurface_PrintText，把返回的两个 dword（绘制后的光标/包围盒）
//   拷进 *pOut，返回 pOut。
//
//   降级点见头文件（font / a14 不参与；色阶表用 BaseColor 代替）。
// ============================================================================
Point2D* Plain_Text_Print(Point2D* pOut, Surface* pSurface, int32 x, int32 y,
                          const char* pText, int32 rawColor, int32 font,
                          int32 a14, int32 a1C, int32 schemeIdx)
{
    (void)font;
    (void)a14;
    (void)a1C;

    if (pOut == nullptr)
        return nullptr;

    int32 color = rawColor;

    if (schemeIdx != -1) {
        // asm: mov ecx,ColorScheme.data / mov eax,[ecx+eax*4]
        ColorScheme* pScheme = nullptr;
        if (ColorScheme::Array != nullptr &&
            schemeIdx >= 0 && schemeIdx < ColorScheme::Array->Count) {
            pScheme = (*ColorScheme::Array)[schemeIdx];
        }
        if (pScheme != nullptr) {
            // asm: lea ecx,[eax+308h] / call sub_517440 -> RGB 三字节
            const uint8 r = pScheme->BaseColor.R;
            const uint8 g = pScheme->BaseColor.G;
            const uint8 b = pScheme->BaseColor.B;

            // asm: 各通道先右移（col_*_SHR）再左移（col_*_SHL）打包
            const int32 rr = (r >> 3) << 11;      // col_R_SHR_3 / col_R_SHL_11
            const int32 gg = (g >> 2) << 5;       // col_G_SHR_2 / col_G_SHL_5
            const int32 bb = (b >> 3) << 0;       // col_B_SHR_3 / col_B_SHL_0
            color = (rr | gg | bb) & 0xFFFF;
        }
    }

    if (pSurface == nullptr || pText == nullptr) {
        pOut->X = x;
        pOut->Y = y;
        return pOut;
    }

    // 窄字符 -> 宽字符（DSurface_PrintText 收的是 char*，本项目文本层收 wchar_t）
    wchar_t wide[512];
    int32 n = 0;
    for (; pText[n] != '\0' && n < 511; ++n)
        wide[n] = static_cast<wchar_t>(static_cast<uint8>(pText[n]));
    wide[n] = L'\0';

    Rectangle bounds(0, 0, pSurface->Width, pSurface->Height);
    Point2D location(x, y);
    Fancy_Text_Print_Wide(*pOut, wide, pSurface, bounds, location,
                          static_cast<DWORD>(color), 0, TextPrintType::Left);
    return pOut;
}

// ============================================================================
// CC_Draw_Shape  (asm 0x4AED80，retn 38h)
//
//   void __fastcall CC_Draw_Shape(
//       BSurface *Surface, ConvertClass *Convert, SHPStruct *SHP, int frameIdx,
//       dwXY *pos, dwLTRB *boundingRect, size_t flags,
//       _DWORD a8, _DWORD a9, _DWORD a10, _DWORD a11, int TintColor,
//       void *BUILDINGZ_SHA, _DWORD zFrame, int ZS_X, int ZS_Y)
//
//   逐段（asm 顺序）：
//     ① SHP 是复合体（word[0] == 0xFFFF）→ 先求值，再取 [SHP+0Ch] 的真实体；
//     ② 目标面 bounds ∩ boundingRect，空则退出；
//     ③ Convert->field_17C = arg_14（z 梯度索引）；
//     ④ 取帧头 (x,y,w,h) 与帧解码尺寸；
//     ⑤ 建临时面（尺寸 = SHP 画布的 Width/Height）——本实现直接解码到缓存，
//        语义相同；
//     ⑥ flags & 0x200（Centered）→ pos -= (画布宽/2, 画布高/2)；
//     ⑦ 有 ZShape 时再按 zShape 帧原点二次对齐；
//     ⑧ pos += 帧原点 (x,y)；
//     ⑨ 有 ZShape 时用 z 帧包围盒再做一次裁剪；
//     ⑩ ValidFrameIndex 决定 RLE / 普通 blitter；
//     ⑪ 逐行把 8bit 索引交给 blitter 合成到目标面。
//
//   与旧实现的差别：
//     * 旧版自己写逐像素循环、选了 blitter 却 `(void)blitter` 丢掉，
//       于是 Remap / 半透明 / Alpha / Darken / Z 全都不生效；
//     * 旧版把“居中”用的画布宽高与帧宽高混在一起，且 flags 位值是猜的
//       （Centered=1），实际会把每个精灵往左上挪半帧；
//     * 旧版忽略复合 SHP（建筑/动画常用），遇到 0xFFFF 头直接当普通 SHP。
// ============================================================================
void CC_Draw_Shape(
    Surface* pSurface, ConvertClass* Palette, SHPStruct* SHP, int32 FrameIndex,
    const Point2D* Position, const Rectangle* Bounds, BlitterFlags Flags,
    int32 Remap, int32 ZAdjust, int32 ZGradientDescIndex,
    int32 Brightness, int32 TintColor,
    SHPStruct* ZShape, int32 ZShapeFrame, int32 XOffset, int32 YOffset)
{
    (void)Remap;
    (void)Brightness;
    if (pSurface == nullptr || SHP == nullptr || Position == nullptr)
        return;
    if (pSurface->Buffer == nullptr)
        return;
    // ---- ① 复合 SHP：头 word 为 0xFFFF 时取引用体 -------------------------
    SHPStruct* shp = SHP;
    if (shp->Type == 0xFFFF)
    {
        SHPReference* pRef = shp->AsReference();
        if (pRef != nullptr)
        {
            if (!pRef->Loaded)
                pRef->Load();
            shp = pRef->Data;
        }
        if (shp == nullptr)
            return;
    }

    // ---- ② 目标面 ∩ 外部裁剪框 -------------------------------------------
    Rectangle surfRect(0, 0, pSurface->Width, pSurface->Height);
    Rectangle clipBounds = surfRect;
    if (Bounds != nullptr)
    {
        clipBounds = clipBounds.Intersection(*Bounds);
        if (clipBounds.IsEmpty())
            return;
    }

    // ---- ③ z 梯度索引写回转换器（原版 [Convert+17Ch]）---------------------
    if (Palette != nullptr)
        Palette->CurrentZRemap = static_cast<DWORD>(ZGradientDescIndex);

    // ---- ④ 帧头 -----------------------------------------------------------
    int32 frameW = 0;
    int32 frameH = 0;
    if (shp->GetFrameSize(FrameIndex, frameW, frameH) != 0)
        return;
    if (frameW <= 0 || frameH <= 0)
        return;

    const Rectangle frame = shp->GetFrameBounds(FrameIndex);

    // ---- ⑤⑥ 锚点：Centered 用**画布**宽高，不是帧宽高 --------------------
    int32 px = Position->X + XOffset;
    int32 py = Position->Y + YOffset;

    const int32 canvasW = (shp->Width  > 0) ? static_cast<int32>(shp->Width)  : frameW;
    const int32 canvasH = (shp->Height > 0) ? static_cast<int32>(shp->Height) : frameH;

    const uint32 f = static_cast<uint32>(Flags);
    const bool centered = (f & static_cast<uint32>(BlitterFlags::Centered)) != 0;

    if (centered)
    {
        px -= canvasW / 2;
        py -= canvasH / 2;
    }

    // ---- ⑦ ZShape 二次对齐（建筑 Z 形状）---------------------------------
    if (ZShape != nullptr)
    {
        const Rectangle zf = ZShape->GetFrameBounds(ZShapeFrame);
        px -= canvasW / 2 - zf.X;
        py -= canvasH / 2 - zf.Y;
    }

    // ---- ⑧ 加帧原点 -------------------------------------------------------
    px += frame.X;
    py += frame.Y;

    Rectangle dstRect(px, py, frameW, frameH);
    Rectangle drawRect = dstRect.Intersection(clipBounds);
    if (drawRect.IsEmpty())
        return;

    // ---- ⑨ 有 ZShape 时再按 z 帧包围盒裁一次 ------------------------------
    if (ZShape != nullptr)
    {
        Rectangle zRect(px, py, frameW, frameH);
        drawRect = drawRect.Intersection(zRect);
        if (drawRect.IsEmpty())
            return;
    }

    // ---- ⑩ blitter --------------------------------------------------------
    if (Palette == nullptr || Palette->PaletteData == nullptr)
        return;

    Blitter* pBlitter = Palette->SelectPlainBlitter(Flags);
    if (pBlitter == nullptr)
        return;

    // ---- ⑪ 逐行合成 -------------------------------------------------------
    uint8* pixels = shp->Get_Decoded_Pixels(FrameIndex);
    if (pixels == nullptr)
        return;

    const int32 srcOffX = drawRect.X - dstRect.X;
    const int32 srcOffY = drawRect.Y - dstRect.Y;
    const int32 copyW   = drawRect.Width;

    const int32 bytesPerPix = pSurface->GetBytesPerPixel();
    if (bytesPerPix != 2)
        return;                              // 本工程的战术面恒为 16bpp

    const int32 dstPitch = pSurface->GetPitch();
    BYTE* dstBase = static_cast<BYTE*>(pSurface->Buffer);

    // 染色：原版把 TintColor 交给 blitter 的 Blit_Copy_Tinted。
    //   Flags 没带 TintColor 时忽略（避免把 0x1000 这类标记当颜色用）。
    const bool useTint = (TintColor != 0) &&
                         ((f & static_cast<uint32>(BlitterFlags::TintColor)) != 0);

    // ---- ⑫ z 值 / 逐行 zbuf ---------------------------------------------
    //
    // asm: BSurface_DrawSHPFrame 在进入逐行 blit 之前先算好 z（loc_437C39），
    //   每行再把 zbuf 指针往后挪一行的宽度：
    //     mov  ax, [zobj+24h] / add ax,[zobj+4]     ; 8000h + 视口top
    //     sub  ax, rect.Y / sub ax,帧高 / sub ax,帧Y ; - 精灵底边屏幕 Y
    //     inc  eax / and eax,0FFFFh / add eax,[a8]   ; +1 + ZAdjust
    //   本树把这套换算收在 Surface::Compute_Z，这里只取"底边"和 ZAdjust。
    //   z 值按目标面的 z 视图（ZLeft/ZTop/ZWidth/ZHeight）寻址，越界的行
    //   传 nullptr —— 越界即"不做深度测试"，与 asm 里 zbuf 为空时的行为一致。
    uint16 zValue = 0;
    const bool hasZ = (pSurface->Get_Z_Buffer() != nullptr);
    if (hasZ)
        zValue = pSurface->Compute_Z(dstRect.Y + frameH, ZAdjust);

    for (int32 row = 0; row < drawRect.Height; ++row)
    {
        const int32 srcY = srcOffY + row;
        if (srcY < 0 || srcY >= frameH)
            continue;

        BYTE* srcLine = pixels + static_cast<size_t>(srcY) * static_cast<size_t>(frameW)
                        + srcOffX;
        BYTE* dstLine = dstBase + static_cast<size_t>(drawRect.Y + row) * static_cast<size_t>(dstPitch)
                        + static_cast<size_t>(drawRect.X) * 2u;

        uint16* zLine = hasZ ? pSurface->Get_Z_Line(drawRect.X, drawRect.Y + row) : nullptr;
        // 行首越界（视口右/下边切掉一半）时该行整行不测，避免越界写。
        if (zLine != nullptr && static_cast<int32>(drawRect.X - pSurface->ZLeft) + copyW > pSurface->Get_Z_Width())
            zLine = nullptr;

        if (useTint)
        {
            pBlitter->Blit_Copy_Tinted(dstLine, srcLine, copyW,
                                       static_cast<int32>(zValue), zLine, nullptr, 0, 0,
                                       static_cast<uint16>(TintColor));
        }
        else
        {
            pBlitter->Blit_Copy(dstLine, srcLine, copyW,
                                static_cast<int32>(zValue), zLine, nullptr, 0, 0);
        }
    }

    if (g_bDebugMode)
    {
        std::fprintf(stderr,
                     "[SHP] frame=%d canvas=%dx%d frame=(%d,%d %dx%d) dst=(%d,%d) "
                     "clip=(%d,%d %dx%d) flags=%08X\n",
                     FrameIndex, canvasW, canvasH,
                     frame.X, frame.Y, frameW, frameH,
                     dstRect.X, dstRect.Y,
                     drawRect.X, drawRect.Y, drawRect.Width, drawRect.Height,
                     static_cast<unsigned>(f));
    }
}

// ============================================================================
// 全局离屏表面（asm .data: DSurface_Alternate / _Hidden / _Composite /
// _Tile / _Sidebar）—— 初始为空，由 DisplayClass::RecreateSurfaces 在
// SetVideoMode 尾部整批销毁重建。
//
// 原版在 .data 里只留五个 DSurfaceClass* 指针槽；Hidden 这张面在装载 /
// 进度 / 记分画面上承担"先画再翻"的中转角色（取面者只用到 Surface 层接口，
// 但槽本身的类型是 DSurface*，与建面时写入的一致）。
// ============================================================================
DSurface* DSurface_Alternate = nullptr;
DSurface* DSurface_Hidden    = nullptr;
DSurface* DSurface_Composite = nullptr;
DSurface* DSurface_Tile      = nullptr;
DSurface* DSurface_Sidebar   = nullptr;

// asm: ChronoScreenEffectSurf —— 超时空特效的离屏面，读档时被清空。
DSurface* ChronoScreenEffectSurf = nullptr;

// asm: dword_887644 —— 战术视口的深度缓冲。由
// TacticalClass::Set_View_Dimensions（对应原版 DisplayClass::Set_View_Dimensions）
// 销毁重建；空指针 = 本树当前没有 z 缓冲，提笔器按"不做深度测试"处理。
ZBufferClass* ZBuffer_Global = nullptr;

// ============================================================================
// ZBufferClass —— 见 Surface.h 顶部的反汇编对照
//
//   asm 里的构造（sub_7BC970）等效于：
//     new BSurface(宽, 高, 2bpp)  -> 缓冲；
//     末尾清零到 0xFFFF（push 0FFFFh / call [vt+14h] = FillRect）；
//     +1Ch = 基址 + 宽*高*2；  +20h = 宽*高*2；  +24h = 0x8000。
//   布局上 +28h/+2Ch 存的是宽/高（同一份数据在 +08h/+0Ch 也存了一份），
//   这里用 Width/Height 一个字段表达 —— 语义不变，省掉二份冗余。
// ============================================================================
ZBufferClass::ZBufferClass(const Rectangle& view)
    : Left(view.X)
    , Top(view.Y)
    , Width(view.Width)
    , Height(view.Height)
    , Offset(0)
    , Buffer(nullptr)
    , Base(nullptr)
    , End(nullptr)
    , Wrap(0)
    , ZBase(0x8000)
    , Target(nullptr)
{
    if (Width <= 0 || Height <= 0)
        return;

    // asm: push 20h / call new —— 一个 0x20 字节的 BSurface
    Buffer = new BSurface();
    if (Buffer == nullptr)
        return;

    // asm: AllocateMemory(0, 宽*高*2)，面宽 = 视口宽，2bpp => 行距 = 宽*2。
    //   BSurface::Allocate 自己会先 memset 0，紧接着的 Clear() 再全部改成
    //   0xFFFF，两步都与 asm 的 new + FillRect(0xFFFF) 一致。
    Buffer->Allocate(Width, Height);
    Base = reinterpret_cast<uint16*>(Buffer->Buffer);
    if (Base == nullptr)
        return;

    Wrap = static_cast<uint32>(Width) * static_cast<uint32>(Height) * sizeof(uint16);
    End  = reinterpret_cast<uint16*>(reinterpret_cast<BYTE*>(Base) + Wrap);

    Clear();
}

ZBufferClass::~ZBufferClass()
{
    // 退挂：目标面还指着这块内存的话必须置空，否则下一帧会读到已释放的缓冲。
    if (Target != nullptr && Target->Get_Z_Buffer() == Base)
    {
        Target->Set_Z_Buffer(nullptr);
        Target->ZPitch  = 0;
        Target->ZWidth  = 0;
        Target->ZHeight = 0;
    }
    Target = nullptr;

    // asm: sub_7BCAE0 -> 缓冲面的析构（vt[0](1)）再 delete；这里 new/delete 配对。
    delete Buffer;
    Buffer = nullptr;
    Base   = nullptr;
    End    = nullptr;
    Wrap   = 0;

    if (ZBuffer_Global == this)
        ZBuffer_Global = nullptr;
}

void ZBufferClass::Clear()
{
    if (Base == nullptr || Width <= 0 || Height <= 0)
        return;

    // asm: sub_7BCFB0 用 `or eax, 0FFFFFFFFh / rep stosd` —— 一次写 4 字节，
    //   行尾奇数字再补一个 word 0FFFFh；等价于整片填 0xFFFF。
    memset(Base, 0xFF, static_cast<size_t>(Wrap));
}

void ZBufferClass::Clear_Rect(int32 x, int32 y, int32 w, int32 h)
{
    if (Base == nullptr || w <= 0 || h <= 0)
        return;

    int32 x0 = x;
    int32 y0 = y;
    int32 x1 = x + w;
    int32 y1 = y + h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > Width)  x1 = Width;
    if (y1 > Height) y1 = Height;
    if (x0 >= x1 || y0 >= y1)
        return;

    const size_t rowWords = static_cast<size_t>(Width);
    for (int32 row = y0; row < y1; ++row)
    {
        uint16* line = Base + static_cast<size_t>(row) * rowWords + x0;
        int32 count = x1 - x0;
        int32 i = 0;

        // asm: `test dl, 2 / jz` —— 目标地址只按 2 字节对齐时先补一个 word，
        //   后面才能安全地 rep stosd（一次 4 字节，两个 0FFFFh）。
        if ((reinterpret_cast<uintptr_t>(line) & 2u) != 0u && count > 0)
        {
            line[0] = 0xFFFF;
            i = 1;
        }

        // asm: `or eax, 0FFFFFFFFh / rep stosd`
        for (; i + 1 < count; i += 2)
            *reinterpret_cast<uint32*>(line + i) = 0xFFFFFFFFu;

        // asm: `test byte ptr [..], 1 / mov word ptr [edi], 0FFFFh` —— 奇尾
        if (i < count)
            line[i] = 0xFFFF;
    }
}

void ZBufferClass::Clear_Rect(const Rectangle& rect)
{
    Clear_Rect(rect.X, rect.Y, rect.Width, rect.Height);
}

uint16* ZBufferClass::Lock(int32 x, int32 y)
{
    // asm: sub_7BD130(this, x, y)：
    //   p = Buffer->Lock(x, y) ; Buffer->Unlock() ;
    //   eax = [this+10h] + p ; if (eax >= [this+1Ch]) eax -= [this+20h] ; return eax
    if (Buffer == nullptr || Base == nullptr)
        return nullptr;

    uint16* p = reinterpret_cast<uint16*>(Buffer->Lock(x, y));
    Buffer->Unlock();
    if (p == nullptr)
        return nullptr;

    if (Offset != 0)
        p = reinterpret_cast<uint16*>(reinterpret_cast<BYTE*>(p) + Offset);
    if (p >= End)
        p = reinterpret_cast<uint16*>(reinterpret_cast<BYTE*>(p) - Wrap);
    return p;
}

void ZBufferClass::Unlock()
{
    if (Buffer != nullptr)
        Buffer->Unlock();
}

void ZBufferClass::Bind(Surface* pTarget)
{
    if (pTarget == nullptr)
        return;

    // 没有缓冲时挂空 —— 提笔器按"不做深度测试"处理，与 asm 在
    //   `test ecx, ecx / jz` 分支里跳过后端的行为一致。
    pTarget->Set_Z_Buffer(Base);
    pTarget->ZPitch  = (Base != nullptr) ? Width  : 0;
    pTarget->ZWidth  = (Base != nullptr) ? Width  : 0;
    pTarget->ZHeight = (Base != nullptr) ? Height : 0;
    pTarget->ZLeft   = Left;
    pTarget->ZTop    = Top;
    pTarget->ZBase   = ZBase;

    Target = pTarget;
}
