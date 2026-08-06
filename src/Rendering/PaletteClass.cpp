// ============================================================================
// PaletteClass.cpp - 256-color palette management implementation
// ============================================================================
// Implements PAL file loading, per-entry colour access, brightness/contrast/
// tint adjustment, palette fading/interpolation, and nearest-colour lookup.
// ============================================================================

#include "PaletteClass.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// Construction / destruction
// ============================================================================

PaletteClass::PaletteClass() noexcept
{
    Clear();
}

PaletteClass::~PaletteClass()
{
    // No dynamically allocated resources.
}

// ============================================================================
// Helpers
// ============================================================================

uint8 PaletteClass::Clamp8(int32 value)
{
    if (value < 0)   return 0;
    if (value > 255) return 255;
    return static_cast<uint8>(value);
}

// Scale a 6-bit VGA value (0-63) to an 8-bit value (0-255).
static inline uint8 Scale6To8(uint8 val)
{
    // The original Westwood palette uses 6-bit per channel (0-63).
    // Scale to 8-bit by multiplying by 255/63 ≈ 4.0476.
    return static_cast<uint8>((static_cast<int32>(val) * 255 + 31) / 63);
}

// Scale an 8-bit value (0-255) down to 6-bit (0-63).
static inline uint8 Scale8To6(uint8 val)
{
    return static_cast<uint8>((static_cast<int32>(val) * 63 + 127) / 255);
}

// ============================================================================
// Palette loading
// ============================================================================

bool PaletteClass::Load_From_File(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0') {
        return false;
    }

    FILE* pFile = std::fopen(pFilename, "rb");
    if (!pFile) {
        return false;
    }

    // Determine file size.
    std::fseek(pFile, 0, SEEK_END);
    long fileSize = std::ftell(pFile);
    std::fseek(pFile, 0, SEEK_SET);

    if (fileSize < 768) {
        // A PAL file must contain at least 256 * 3 = 768 bytes.
        std::fclose(pFile);
        return false;
    }

    // Read 768 bytes of RGB data.
    uint8 raw[768];
    size_t bytesRead = std::fread(raw, 1, 768, pFile);
    std::fclose(pFile);

    if (bytesRead < 768) {
        return false;
    }

    return Load_From_Data(raw, 768, true);
}

bool PaletteClass::Load_From_Data(const uint8* pData, int32 dataSize, bool bScale6Bit)
{
    if (!pData || dataSize < 768) {
        return false;
    }

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        uint8 r = pData[i * 3 + 0];
        uint8 g = pData[i * 3 + 1];
        uint8 b = pData[i * 3 + 2];
        if (bScale6Bit) {
            r = Scale6To8(r);
            g = Scale6To8(g);
            b = Scale6To8(b);
        }
        Colors[i].Red   = r;
        Colors[i].Green = g;
        Colors[i].Blue  = b;
    }

    return true;
}

bool PaletteClass::Save_To_File(const char* pFilename) const
{
    if (!pFilename || pFilename[0] == '\0') {
        return false;
    }

    FILE* pFile = std::fopen(pFilename, "wb");
    if (!pFile) {
        return false;
    }

    // Write 768 bytes of 6-bit-per-channel RGB data.
    uint8 raw[768];
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        raw[i * 3 + 0] = Scale8To6(Colors[i].Red);
        raw[i * 3 + 1] = Scale8To6(Colors[i].Green);
        raw[i * 3 + 2] = Scale8To6(Colors[i].Blue);
    }

    size_t bytesWritten = std::fwrite(raw, 1, 768, pFile);
    std::fclose(pFile);

    return bytesWritten == 768;
}

// ============================================================================
// Per-entry colour access
// ============================================================================

RGBClass& PaletteClass::Get_Color(int32 index)
{
    if (index < 0) index = 0;
    if (index >= PALETTE_SIZE) index = PALETTE_SIZE - 1;
    return Colors[index];
}

const RGBClass& PaletteClass::Get_Color(int32 index) const
{
    if (index < 0) index = 0;
    if (index >= PALETTE_SIZE) index = PALETTE_SIZE - 1;
    return Colors[index];
}

void PaletteClass::Set_Color(int32 index, const RGBClass& color)
{
    if (index < 0 || index >= PALETTE_SIZE) return;
    Colors[index] = color;
}

void PaletteClass::Set_Color(int32 index, uint8 r, uint8 g, uint8 b)
{
    if (index < 0 || index >= PALETTE_SIZE) return;
    Colors[index].Red   = r;
    Colors[index].Green = g;
    Colors[index].Blue  = b;
}

// ============================================================================
// Bulk palette data access
// ============================================================================

void PaletteClass::Get_Palette_Data(RGBClass* pOutColors) const
{
    if (!pOutColors) return;
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        pOutColors[i] = Colors[i];
    }
}

void PaletteClass::Get_Palette_Data(BytePalette* pOutPalette) const
{
    if (!pOutPalette) return;
    // The BytePalette is 256 bytes; we copy the RGB data into a raw buffer
    // and reinterpret.  In practice the caller uses this for compatibility
    // with ConvertClass which expects raw palette bytes.
    uint8* pRaw = reinterpret_cast<uint8*>(pOutPalette->Entries);
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        // Store the palette index itself (identity mapping) since BytePalette
        // holds byte indices, not RGB values.  This is the convention used by
        // the ConvertClass system for identity palettes.
        pRaw[i] = static_cast<uint8>(i);
    }
}

void PaletteClass::Set_Palette_Data(const RGBClass* pColors)
{
    if (!pColors) return;
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        Colors[i] = pColors[i];
    }
}

void PaletteClass::Set_Palette_Data(const uint8* pRawData, bool bScale6Bit)
{
    if (!pRawData) return;
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        uint8 r = pRawData[i * 3 + 0];
        uint8 g = pRawData[i * 3 + 1];
        uint8 b = pRawData[i * 3 + 2];
        if (bScale6Bit) {
            r = Scale6To8(r);
            g = Scale6To8(g);
            b = Scale6To8(b);
        }
        Colors[i].Red   = r;
        Colors[i].Green = g;
        Colors[i].Blue  = b;
    }
}

// ============================================================================
// Palette adjustment
// ============================================================================

void PaletteClass::Create_Adjust_Palette(PaletteClass* pOut,
                                         int32 brightness,
                                         int32 contrast,
                                         int32 tintR, int32 tintG, int32 tintB) const
{
    if (!pOut) return;

    // Compute the contrast factor centred at 128.
    // contrast ranges from -100 to +100; factor = 1.0 + contrast/100.0
    double contrastFactor = 1.0 + static_cast<double>(contrast) / 100.0;
    if (contrastFactor < 0.0) contrastFactor = 0.0;

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        int32 r = static_cast<int32>(Colors[i].Red);
        int32 g = static_cast<int32>(Colors[i].Green);
        int32 b = static_cast<int32>(Colors[i].Blue);

        // Apply contrast: stretch values away from / toward 128.
        r = static_cast<int32>((r - 128) * contrastFactor + 128);
        g = static_cast<int32>((g - 128) * contrastFactor + 128);
        b = static_cast<int32>((b - 128) * contrastFactor + 128);

        // Apply brightness (additive).
        r += brightness;
        g += brightness;
        b += brightness;

        // Apply tint (per-channel additive).
        r += tintR;
        g += tintG;
        b += tintB;

        pOut->Colors[i].Red   = Clamp8(r);
        pOut->Colors[i].Green = Clamp8(g);
        pOut->Colors[i].Blue  = Clamp8(b);
    }
}

void PaletteClass::Adjust(int32 brightness, int32 contrast,
                          int32 tintR, int32 tintG, int32 tintB)
{
    Create_Adjust_Palette(this, brightness, contrast, tintR, tintG, tintB);
}

// ============================================================================
// Palette fading
// ============================================================================

void PaletteClass::Fade_Palette(const PaletteClass& target, int32 ratio)
{
    // Clamp the ratio to [0, 255].
    if (ratio < 0)   ratio = 0;
    if (ratio > 255) ratio = 255;

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        int32 r1 = static_cast<int32>(Colors[i].Red);
        int32 g1 = static_cast<int32>(Colors[i].Green);
        int32 b1 = static_cast<int32>(Colors[i].Blue);

        int32 r2 = static_cast<int32>(target.Colors[i].Red);
        int32 g2 = static_cast<int32>(target.Colors[i].Green);
        int32 b2 = static_cast<int32>(target.Colors[i].Blue);

        // Linear interpolation: result = src + (dst - src) * ratio / 255
        Colors[i].Red   = static_cast<uint8>(r1 + (r2 - r1) * ratio / 255);
        Colors[i].Green = static_cast<uint8>(g1 + (g2 - g1) * ratio / 255);
        Colors[i].Blue  = static_cast<uint8>(b1 + (b2 - b1) * ratio / 255);
    }
}

void PaletteClass::Fade_To_Black(int32 ratio)
{
    if (ratio < 0)   ratio = 0;
    if (ratio > 255) ratio = 255;

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        int32 r = static_cast<int32>(Colors[i].Red);
        int32 g = static_cast<int32>(Colors[i].Green);
        int32 b = static_cast<int32>(Colors[i].Blue);

        // Fade toward (0,0,0): result = value * (255 - ratio) / 255
        Colors[i].Red   = static_cast<uint8>(r * (255 - ratio) / 255);
        Colors[i].Green = static_cast<uint8>(g * (255 - ratio) / 255);
        Colors[i].Blue  = static_cast<uint8>(b * (255 - ratio) / 255);
    }
}

void PaletteClass::Fade_To_White(int32 ratio)
{
    if (ratio < 0)   ratio = 0;
    if (ratio > 255) ratio = 255;

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        int32 r = static_cast<int32>(Colors[i].Red);
        int32 g = static_cast<int32>(Colors[i].Green);
        int32 b = static_cast<int32>(Colors[i].Blue);

        // Fade toward (255,255,255): result = value + (255 - value) * ratio / 255
        Colors[i].Red   = static_cast<uint8>(r + (255 - r) * ratio / 255);
        Colors[i].Green = static_cast<uint8>(g + (255 - g) * ratio / 255);
        Colors[i].Blue  = static_cast<uint8>(b + (255 - b) * ratio / 255);
    }
}

void PaletteClass::Interpolate(const PaletteClass& source1,
                               const PaletteClass& source2, int32 ratio)
{
    if (ratio < 0)   ratio = 0;
    if (ratio > 255) ratio = 255;

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        int32 r1 = static_cast<int32>(source1.Colors[i].Red);
        int32 g1 = static_cast<int32>(source1.Colors[i].Green);
        int32 b1 = static_cast<int32>(source1.Colors[i].Blue);

        int32 r2 = static_cast<int32>(source2.Colors[i].Red);
        int32 g2 = static_cast<int32>(source2.Colors[i].Green);
        int32 b2 = static_cast<int32>(source2.Colors[i].Blue);

        Colors[i].Red   = static_cast<uint8>(r1 + (r2 - r1) * ratio / 255);
        Colors[i].Green = static_cast<uint8>(g1 + (g2 - g1) * ratio / 255);
        Colors[i].Blue  = static_cast<uint8>(b1 + (b2 - b1) * ratio / 255);
    }
}

// ============================================================================
// Colour matching
// ============================================================================

int32 PaletteClass::Get_Closest_Color(uint8 r, uint8 g, uint8 b) const
{
    RGBClass target(r, g, b);
    return Get_Closest_Color(target);
}

int32 PaletteClass::Get_Closest_Color(const RGBClass& color) const
{
    int32 bestIndex = 0;
    int32 bestDist  = INT_MAX;

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        int32 dr = static_cast<int32>(Colors[i].Red)   - static_cast<int32>(color.Red);
        int32 dg = static_cast<int32>(Colors[i].Green) - static_cast<int32>(color.Green);
        int32 db = static_cast<int32>(Colors[i].Blue)  - static_cast<int32>(color.Blue);

        // Squared Euclidean distance in RGB space.
        int32 dist = dr * dr + dg * dg + db * db;

        if (dist < bestDist) {
            bestDist  = dist;
            bestIndex = i;
            // Early exit on exact match.
            if (dist == 0) break;
        }
    }

    return bestIndex;
}

// ============================================================================
// Utility
// ============================================================================

void PaletteClass::Clear()
{
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        Colors[i].Red   = 0;
        Colors[i].Green = 0;
        Colors[i].Blue  = 0;
    }
}

void PaletteClass::Set_Default()
{
    // Generate a default grey-ramp palette: the first 256 entries form a
    // smooth ramp from black to white, suitable as a fallback.
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        uint8 val = static_cast<uint8>(i);
        Colors[i].Red   = val;
        Colors[i].Green = val;
        Colors[i].Blue  = val;
    }

    // The first 16 entries get the standard EGA/VGA colour set so that
    // basic UI elements render correctly even without a game palette loaded.
    static const uint8 egaColors[16][3] = {
        { 0,   0,   0   },  // 0  black
        { 0,   0,   170 },  // 1  blue
        { 0,   170, 0   },  // 2  green
        { 0,   170, 170 },  // 3  cyan
        { 170, 0,   0   },  // 4  red
        { 170, 0,   170 },  // 5  magenta
        { 170, 85,  0   },  // 6  brown
        { 170, 170, 170 },  // 7  light grey
        { 85,  85,  85  },  // 8  dark grey
        { 85,  85,  255 },  // 9  light blue
        { 85,  255, 85  },  // 10 light green
        { 85,  255, 255 },  // 11 light cyan
        { 255, 85,  85  },  // 12 light red
        { 255, 85,  255 },  // 13 light magenta
        { 255, 255, 85  },  // 14 yellow
        { 255, 255, 255 }   // 15 white
    };

    for (int32 i = 0; i < 16; ++i) {
        Colors[i].Red   = egaColors[i][0];
        Colors[i].Green = egaColors[i][1];
        Colors[i].Blue  = egaColors[i][2];
    }
}

bool PaletteClass::Is_Black() const
{
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        if (Colors[i].Red != 0 || Colors[i].Green != 0 || Colors[i].Blue != 0) {
            return false;
        }
    }
    return true;
}
