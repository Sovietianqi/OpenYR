#include "Rendering/ConvertClass.h"
#include "Rendering/Surface.h"
#include "Rendering/Blitter.h"
#include "Core/Memory.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <algorithm>

// ============================================================================
// Static member initialization
// ============================================================================
DynamicVectorClass<ConvertClass*>* ConvertClass::Array = nullptr;
DynamicVectorClass<LightConvertClass*>* LightConvertClass::Array = nullptr;
DynamicVectorClass<ColorScheme*>* ColorScheme::Array = nullptr;

// ============================================================================
// ConvertClass implementation
// ============================================================================

ConvertClass::ConvertClass(
    const BytePalette& palette,
    const BytePalette& eightBitPalette,
    DSurface* pSurface,
    size_t shadeCount,
    bool skipBlitters)
    : BytesPerPixel(2)
    , ShadeCount(static_cast<int32>(shadeCount))
    , FullColorData(nullptr)
    , PaletteData(nullptr)
    , ByteColorData(nullptr)
    , CurrentZRemap(0)
    , HalfTranslucencyMask(0)
    , QuatTranslucencyMask(0)
{
    memset(Blitters, 0, sizeof(Blitters));
    memset(RLEBlitters, 0, sizeof(RLEBlitters));

    BuildTranslucencyMasks();

    // Allocate full color data: ShadeCount slots of 8 bytes per pixel
    // Each slot contains: 2 bytes for 16-bit color, plus padding
    size_t fullSize = shadeCount * 8 * static_cast<size_t>(BytesPerPixel);
    FullColorData = YRMemory::Allocate(fullSize);
    if (FullColorData)
    {
        memset(FullColorData, 0, fullSize);

        // PaletteData is the main lookup table at offset 0
        PaletteData = FullColorData;

        // Build the palette lookup table from the source palette
        uint16* pal = reinterpret_cast<uint16*>(PaletteData);
        for (int32 i = 0; i < 256; ++i)
        {
            BYTE palIndex = palette[i];
            // Convert palette entry to 16-bit RGB 565
            if (palette.Entries[i] != 0)
            {
                // Use the palette index as a lookup into the full color space
                // In production, this would use the actual palette RGB values
                int32 r = (palIndex & 0xE0) >> 2;       // 5 bits of red
                int32 g = (palIndex & 0x1C) << 1;       // 6 bits of green
                int32 b = (palIndex & 0x03) << 3;       // 5 bits of blue
                pal[i] = static_cast<uint16>((r << 11) | (g << 5) | b);
            }
            else
            {
                pal[i] = 0;
            }
        }
    }

    // Allocate byte color data for 8-bit surfaces
    if (BytesPerPixel == 1)
    {
        ByteColorData = YRMemory::Allocate(256);
        if (ByteColorData)
            memset(ByteColorData, 0, 256);
    }

    if (!skipBlitters)
        BuildBlitters(pSurface, skipBlitters);
}

ConvertClass::~ConvertClass()
{
    // Clean up blitters
    for (int32 i = 0; i < 50; ++i)
    {
        if (Blitters[i])
        {
            delete Blitters[i];
            Blitters[i] = nullptr;
        }
    }
    for (int32 i = 0; i < 39; ++i)
    {
        if (RLEBlitters[i])
        {
            delete RLEBlitters[i];
            RLEBlitters[i] = nullptr;
        }
    }

    if (FullColorData)
        YRMemory::Deallocate(FullColorData);
    if (ByteColorData)
        YRMemory::Deallocate(ByteColorData);

    FullColorData = nullptr;
    PaletteData = nullptr;
    ByteColorData = nullptr;
}

void ConvertClass::BuildTranslucencyMasks()
{
    // For 16-bit RGB 565:
    // Red:   5 bits (0xF800 / masks 0x7C00)
    // Green: 6 bits (0x07E0 / masks 0x03E0)
    // Blue:  5 bits (0x<｜image｜>    / masks 0x000F)
    //
    // The mask 0x7BEF removes the LSB of each color component,
    // preventing color drift during repeated blending operations.
    // 0x7BEF = 0111 1011 1110 1111
    HalfTranslucencyMask = 0x7BEF;
    QuatTranslucencyMask = 0x7BEF;
}

void ConvertClass::BuildBlitters(DSurface* pSurface, bool skipBlitters)
{
    if (!PaletteData || skipBlitters) return;

    uint16* pal = reinterpret_cast<uint16*>(PaletteData);

    // Build plain blitters array
    Blitters[0]  = new BlitTransXlat<uint16>(pal);
    Blitters[1]  = new BlitTransRemapXlat<uint16>(pal, nullptr);
    Blitters[2]  = new BlitTransZRemapXlat<uint16>(pal, nullptr);
    Blitters[3]  = new BlitTransLucent25<uint16>(pal, QuatTranslucencyMask);
    Blitters[4]  = new BlitTransLucent50<uint16>(pal, HalfTranslucencyMask);
    Blitters[5]  = new BlitTransLucent75<uint16>(pal, QuatTranslucencyMask);
    Blitters[6]  = new BlitTransDarken<uint16>(pal, 0xFFFF);
    Blitters[7]  = new BlitTransXlatAlpha<uint16>(pal, nullptr);
    Blitters[8]  = new BlitTransLucent25Alpha<uint16>(pal, QuatTranslucencyMask, nullptr);
    Blitters[9]  = new BlitTransLucent50Alpha<uint16>(pal, HalfTranslucencyMask, nullptr);
    Blitters[10] = new BlitTransLucent75Alpha<uint16>(pal, QuatTranslucencyMask, nullptr);
    Blitters[11] = new BlitTransXlatZRead<uint16>(pal);
    Blitters[12] = new BlitTransLucent25ZRead<uint16>(pal, QuatTranslucencyMask);
    Blitters[13] = new BlitTransLucent50ZRead<uint16>(pal, HalfTranslucencyMask);
    Blitters[14] = new BlitTransLucent75ZRead<uint16>(pal, QuatTranslucencyMask);
    Blitters[15] = new BlitTransXlatZReadWrite<uint16>(pal);
    Blitters[16] = new BlitPlainXlat<uint16>(pal);
    Blitters[17] = new BlitTransDarkenZRead<uint16>(pal, 0xFFFF);
    Blitters[18] = new BlitTransDarkenZReadWrite<uint16>(pal, 0xFFFF);
    Blitters[19] = new BlitPlainXlatAlpha<uint16>(pal, nullptr);
    Blitters[20] = new BlitPlainXlatZRead<uint16>(pal);
    Blitters[21] = new BlitPlainXlatZReadWrite<uint16>(pal);
    Blitters[22] = new BlitTransRemapDest<uint16>(pal, nullptr);

    // Build RLE blitters
    RLEBlitters[0]  = new RLEBlitTransLucent25<uint16>(pal, QuatTranslucencyMask);
    RLEBlitters[1]  = new RLEBlitTransLucent50<uint16>(pal, HalfTranslucencyMask);
    RLEBlitters[2]  = new RLEBlitTransLucent75<uint16>(pal, QuatTranslucencyMask);
    RLEBlitters[3]  = new RLEBlitTransDarken<uint16>(pal, 0xFFFF);
    RLEBlitters[4]  = new RLEBlitTransXlat<uint16>(pal);
    RLEBlitters[5]  = new RLEBlitTransXlatAlpha<uint16>(pal, nullptr);
    RLEBlitters[6]  = new RLEBlitTransXlatZRead<uint16>(pal);
    RLEBlitters[7]  = new RLEBlitTransXlatZReadWrite<uint16>(pal);
    RLEBlitters[8]  = new RLEBlitTransRemapXlat<uint16>(pal, nullptr);
    RLEBlitters[9]  = new RLEBlitTransRemapDest<uint16>(pal, nullptr);
    RLEBlitters[10] = new RLEBlitTransZRemapXlat<uint16>(pal, nullptr);
}

Blitter* ConvertClass::SelectPlainBlitter(BlitterFlags flags) const
{
    uint32 f = static_cast<uint32>(flags);
    bool isTrans = (f & static_cast<uint32>(BlitterFlags::Transparent)) != 0;
    bool isZRead = (f & static_cast<uint32>(BlitterFlags::ZRead)) != 0;
    bool isZWrite = (f & static_cast<uint32>(BlitterFlags::ZWrite)) != 0;
    bool isAlpha = (f & static_cast<uint32>(BlitterFlags::Alpha)) != 0;
    bool isDarken = (f & static_cast<uint32>(BlitterFlags::Darken)) != 0;
    bool isRemap = (f & static_cast<uint32>(BlitterFlags::Remap)) != 0;
    bool isZRemap = (f & static_cast<uint32>(BlitterFlags::ZRemap)) != 0;
    bool isPlain = (f & static_cast<uint32>(BlitterFlags::Plain)) != 0;

    uint32 transMask = f & static_cast<uint32>(BlitterFlags::Translucent);
    int32 transIdx = (transMask == static_cast<uint32>(BlitterFlags::Translucent25)) ? 25 :
                     (transMask == static_cast<uint32>(BlitterFlags::Translucent50)) ? 50 :
                     (transMask == static_cast<uint32>(BlitterFlags::Translucent75)) ? 75 : 0;

    if (isPlain)
    {
        if (isZWrite) return Blitters[21];      // PlainXlatZReadWrite
        if (isZRead)  return Blitters[20];      // PlainXlatZRead
        if (isAlpha)  return Blitters[19];      // PlainXlatAlpha
        return Blitters[16];                     // PlainXlat
    }

    if (isDarken)
    {
        if (isZWrite) return Blitters[18];      // DarkenZReadWrite
        if (isZRead)  return Blitters[17];      // DarkenZRead
        return Blitters[6];                      // Darken
    }

    if (isZRemap)
        return Blitters[2];                      // ZRemapXlat

    if (isRemap)
        return Blitters[1];                      // RemapXlat

    if (isAlpha)
    {
        if (transIdx == 25) return Blitters[8];  // Lucent25Alpha
        if (transIdx == 50) return Blitters[9];  // Lucent50Alpha
        if (transIdx == 75) return Blitters[10]; // Lucent75Alpha
        return Blitters[7];                      // XlatAlpha
    }

    if (isZWrite)
    {
        if (transIdx == 25) return Blitters[12]; // Lucent25ZRead
        if (transIdx == 50) return Blitters[13]; // Lucent50ZRead
        if (transIdx == 75) return Blitters[14]; // Lucent75ZRead
        return Blitters[15];                     // XlatZReadWrite
    }

    if (isZRead)
    {
        if (transIdx == 25) return Blitters[12]; // Lucent25ZRead
        if (transIdx == 50) return Blitters[13]; // Lucent50ZRead
        if (transIdx == 75) return Blitters[14]; // Lucent75ZRead
        return Blitters[11];                     // XlatZRead
    }

    if (transIdx == 25) return Blitters[3];      // Lucent25
    if (transIdx == 50) return Blitters[4];      // Lucent50
    if (transIdx == 75) return Blitters[5];      // Lucent75

    return Blitters[0];                          // Xlat (default)
}

RLEBlitter* ConvertClass::SelectRLEBlitter(BlitterFlags flags) const
{
    uint32 f = static_cast<uint32>(flags);
    bool isZRead = (f & static_cast<uint32>(BlitterFlags::ZRead)) != 0;
    bool isZWrite = (f & static_cast<uint32>(BlitterFlags::ZWrite)) != 0;
    bool isAlpha = (f & static_cast<uint32>(BlitterFlags::Alpha)) != 0;
    bool isDarken = (f & static_cast<uint32>(BlitterFlags::Darken)) != 0;
    bool isRemap = (f & static_cast<uint32>(BlitterFlags::Remap)) != 0;
    bool isZRemap = (f & static_cast<uint32>(BlitterFlags::ZRemap)) != 0;

    uint32 transMask = f & static_cast<uint32>(BlitterFlags::Translucent);
    int32 transIdx = (transMask == static_cast<uint32>(BlitterFlags::Translucent25)) ? 25 :
                     (transMask == static_cast<uint32>(BlitterFlags::Translucent50)) ? 50 :
                     (transMask == static_cast<uint32>(BlitterFlags::Translucent75)) ? 75 : 0;

    if (isDarken)
        return RLEBlitters[3];                   // RLEDarken

    if (isZRemap)
        return RLEBlitters[10];                  // RLEZRemapXlat

    if (isRemap)
        return RLEBlitters[8];                   // RLERemapXlat

    if (isAlpha)
        return RLEBlitters[5];                   // RLEXlatAlpha

    if (isZWrite)
        return RLEBlitters[7];                   // RLEXlatZReadWrite

    if (isZRead)
        return RLEBlitters[6];                   // RLEXlatZRead

    if (transIdx == 25) return RLEBlitters[0];   // RLELucent25
    if (transIdx == 50) return RLEBlitters[1];   // RLELucent50
    if (transIdx == 75) return RLEBlitters[2];   // RLELucent75

    return RLEBlitters[4];                       // RLEXlat (default)
}

ConvertClass* ConvertClass::FindOrAllocate(const char* pFilename)
{
    if (!pFilename || !Array) return nullptr;

    // Search for existing ConvertClass with matching filename
    for (int32 i = 0; i < Array->Size(); ++i)
    {
        ConvertClass* item = (*Array)[i];
        if (item)
        {
            // In production, this would compare against stored filenames
            // For now, we search by index
            char idxName[32];
            int32 written = snprintf(idxName, sizeof(idxName), "palette_%d", i);
            if (written > 0 && strncmp(pFilename, idxName, static_cast<size_t>(written)) == 0)
                return item;
        }
    }

    // Create from file
    BytePalette* pal = nullptr;
    ConvertClass* result = nullptr;
    CreateFromFile(pFilename, pal, result);
    if (result && Array)
        Array->Add(result);

    // Clean up intermediate palette
    if (pal)
    {
        YRMemory::Deallocate(pal);
        pal = nullptr;
    }

    return result;
}

void ConvertClass::CreateFromFile(
    const char* pFilename, BytePalette*& pPalette, ConvertClass*& pDestination)
{
    pDestination = nullptr;
    pPalette = nullptr;

    if (!pFilename) return;

    // In production, this would load a .PAL file from disk
    // The PAL format is 768 bytes of RGB triples
    BytePalette* pal = static_cast<BytePalette*>(YRMemory::Allocate(sizeof(BytePalette)));
    if (!pal) return;

    // Build a default palette with a gradient
    for (int32 i = 0; i < 256; ++i)
    {
        if (i < 16)
        {
            // Standard VGA color palette entries
            static constexpr BYTE vgaColors[16][3] = {
                {0x00, 0x00, 0x00}, {0x00, 0x00, 0xA8}, {0x00, 0xA8, 0x00}, {0x00, 0xA8, 0xA8},
                {0xA8, 0x00, 0x00}, {0xA8, 0x00, 0xA8}, {0xA8, 0x54, 0x00}, {0xA8, 0xA8, 0xA8},
                {0x54, 0x54, 0x54}, {0x54, 0x54, 0xFC}, {0x54, 0xFC, 0x54}, {0x54, 0xFC, 0xFC},
                {0xFC, 0x54, 0x54}, {0xFC, 0x54, 0xFC}, {0xFC, 0xFC, 0x54}, {0xFC, 0xFC, 0xFC},
            };
            int32 idx = i % 16;
            pal->Entries[i] = static_cast<BYTE>(
                (((vgaColors[idx][0] >> 3) << 11) | ((vgaColors[idx][1] >> 2) << 5) | (vgaColors[idx][2] >> 3)) & 0xFF);
        }
        else
        {
            // Color gradient
            int32 band = (i - 16) / 16;
            int32 pos = (i - 16) % 16;
            int32 intensity = pos * 16 + 8;
            switch (band % 15)
            {
                case 0:  pal->Entries[i] = static_cast<BYTE>(((intensity >> 3) << 11) | ((0 >> 2) << 5) | (0 >> 3)); break;
                case 1:  pal->Entries[i] = static_cast<BYTE>(((0 >> 3) << 11) | ((intensity >> 2) << 5) | (0 >> 3)); break;
                case 2:  pal->Entries[i] = static_cast<BYTE>(((0 >> 3) << 11) | ((0 >> 2) << 5) | (intensity >> 3)); break;
                case 3:  pal->Entries[i] = static_cast<BYTE>(((intensity >> 3) << 11) | ((intensity >> 2) << 5) | (0 >> 3)); break;
                case 4:  pal->Entries[i] = static_cast<BYTE>(((0 >> 3) << 11) | ((intensity >> 2) << 5) | (intensity >> 3)); break;
                case 5:  pal->Entries[i] = static_cast<BYTE>(((intensity >> 3) << 11) | ((0 >> 2) << 5) | (intensity >> 3)); break;
                case 6:  pal->Entries[i] = static_cast<BYTE>(((intensity / 2 >> 3) << 11) | ((intensity >> 2) << 5) | (intensity / 2 >> 3)); break;
                case 7:  pal->Entries[i] = static_cast<BYTE>(((intensity >> 3) << 11) | ((intensity / 2 >> 2) << 5) | (intensity / 2 >> 3)); break;
                case 8:  pal->Entries[i] = static_cast<BYTE>(((intensity / 2 >> 3) << 11) | ((intensity / 2 >> 2) << 5) | (intensity >> 3)); break;
                case 9:  pal->Entries[i] = static_cast<BYTE>(((intensity >> 3) << 11) | ((intensity >> 2) << 5) | (0 >> 3)); break;
                case 10: pal->Entries[i] = static_cast<BYTE>(((intensity >> 3) << 11) | ((intensity / 2 >> 2) << 5) | (0 >> 3)); break;
                case 11: pal->Entries[i] = static_cast<BYTE>(((intensity >> 3) << 11) | ((0 >> 2) << 5) | (intensity >> 3)); break;
                case 12: pal->Entries[i] = static_cast<BYTE>(((intensity / 2 >> 3) << 11) | ((intensity >> 2) << 5) | (0 >> 3)); break;
                case 13: pal->Entries[i] = static_cast<BYTE>(((intensity >> 3) << 11) | ((intensity >> 2) << 5) | (intensity >> 3)); break;
                case 14: pal->Entries[i] = static_cast<BYTE>(((intensity >> 3) << 11) | ((intensity / 2 >> 2) << 5) | (intensity / 2 >> 3)); break;
                default: pal->Entries[i] = static_cast<BYTE>(intensity); break;
            }
        }
    }

    BytePalette eightBitPal;
    memset(eightBitPal.Entries, 0, sizeof(eightBitPal.Entries));

    pDestination = new ConvertClass(*pal, eightBitPal, nullptr, 1, false);
    pPalette = pal;
}

// ============================================================================
// LightConvertClass implementation
// ============================================================================

LightConvertClass::LightConvertClass(
    BytePalette* palette1,
    BytePalette* palette2,
    Surface* pSurface,
    int32 colorR,
    int32 colorG,
    int32 colorB,
    bool skipBlitters,
    BYTE* pBuffer,
    size_t shadeCount)
    : ConvertClass(0)
    , UsedPalette1(nullptr)
    , UsedPalette2(nullptr)
    , IndexesToIgnore(nullptr)
    , RefCount(1)
    , Color1(colorR, colorG, colorB)
    , Color2(0, 0, 0)
    , Tinted(false)
{
    BytesPerPixel = 2;
    ShadeCount = static_cast<int32>(shadeCount);

    memset(align_1B1, 0, sizeof(align_1B1));
    memset(Blitters, 0, sizeof(Blitters));
    memset(RLEBlitters, 0, sizeof(RLEBlitters));

    BuildTranslucencyMasks();

    size_t fullSize = shadeCount * 8 * static_cast<size_t>(BytesPerPixel);
    FullColorData = YRMemory::Allocate(fullSize);
    if (FullColorData)
    {
        memset(FullColorData, 0, fullSize);
        PaletteData = FullColorData;
    }

    BuildLightTables(colorR, colorG, colorB, false);

    if (!skipBlitters)
        BuildBlitters(nullptr, skipBlitters);
}

LightConvertClass::~LightConvertClass()
{
    // Clean up blitters inherited from ConvertClass
    for (int32 i = 0; i < 50; ++i)
    {
        if (Blitters[i])
        {
            delete Blitters[i];
            Blitters[i] = nullptr;
        }
    }
    for (int32 i = 0; i < 39; ++i)
    {
        if (RLEBlitters[i])
        {
            delete RLEBlitters[i];
            RLEBlitters[i] = nullptr;
        }
    }

    if (FullColorData)
        YRMemory::Deallocate(FullColorData);
    FullColorData = nullptr;
    PaletteData = nullptr;
}

void LightConvertClass::BuildLightTables(int32 colorR, int32 colorG, int32 colorB, bool tinted)
{
    if (!PaletteData) return;

    uint16* pal = reinterpret_cast<uint16*>(PaletteData);

    // Build a 256-entry lookup table that applies brightness/tint
    for (int32 i = 0; i < 256; ++i)
    {
        // For each palette index, compute the adjusted color
        // The original index is decomposed into RGB components
        int32 r = (i & 0xE0) >> 2;      // 0-31 range (5 bits)
        int32 g = (i & 0x1C) << 1;      // 0-63 range (6 bits)
        int32 b = (i & 0x03) << 3;      // 0-31 range (5 bits)

        if (tinted)
        {
            // Tint mode: interpolate between original and tint color
            int32 tintStrength = 500; // 50% blend
            r = (r * (1000 - tintStrength) + colorR * tintStrength / 10) / 1000;
            g = (g * (1000 - tintStrength) + colorG * tintStrength / 10) / 1000;
            b = (b * (1000 - tintStrength) + colorB * tintStrength / 10) / 1000;
        }
        else
        {
            // Brightness mode: scale RGB components
            r = (r * colorR) / 1000;
            g = (g * colorG) / 1000;
            b = (b * colorB) / 1000;
        }

        // Clamp to valid range
        if (r > 31) r = 31; if (r < 0) r = 0;
        if (g > 63) g = 63; if (g < 0) g = 0;
        if (b > 31) b = 31; if (b < 0) b = 0;

        // Pack into 16-bit RGB 565
        pal[i] = static_cast<uint16>((r << 11) | (g << 5) | b);
    }
}

void LightConvertClass::UpdateColors(int32 red, int32 green, int32 blue, bool tinted)
{
    Color1 = TintStruct(red, green, blue);
    Tinted = tinted;
    BuildLightTables(red, green, blue, tinted);
}

LightConvertClass* LightConvertClass::InitLightConvert(int32 red, int32 green, int32 blue)
{
    LightConvertClass* result = new LightConvertClass(
        nullptr, nullptr, nullptr, red, green, blue, true, nullptr, 1);

    if (Array)
        Array->Add(result);

    return result;
}

void LightConvertClass::DrawIt(Surface* pSurface, Rectangle rect, int32 flags)
{
    if (!pSurface || !pSurface->Buffer || !PaletteData) return;

    Rectangle r = rect.Intersection(Rectangle(0, 0, pSurface->Width, pSurface->Height));
    if (r.IsEmpty()) return;

    uint16* pal = reinterpret_cast<uint16*>(PaletteData);
    int32 bytesPerPix = pSurface->GetBytesPerPixel();
    int32 pitch = pSurface->GetPitch();

    for (int32 y = r.Y; y < r.Y + r.Height; ++y)
    {
        BYTE* line = static_cast<BYTE*>(pSurface->Buffer) + y * pitch;
        for (int32 x = r.X; x < r.X + r.Width; ++x)
        {
            if (bytesPerPix == 1)
            {
                BYTE idx = line[x];
                // Find the palette entry that best matches the current light level
                uint16 target = pal[idx];
                int32 bestMatch = idx;
                int32 bestDist = 0x7FFFFFFF;
                for (int32 i = 0; i < 256; ++i)
                {
                    int32 dr = ((target >> 11) & 0x1F) - ((pal[i] >> 11) & 0x1F);
                    int32 dg = ((target >> 5) & 0x3F) - ((pal[i] >> 5) & 0x3F);
                    int32 db = (target & 0x1F) - (pal[i] & 0x1F);
                    int32 dist = dr * dr + dg * dg + db * db;
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestMatch = i;
                    }
                }
                line[x] = static_cast<BYTE>(bestMatch);
            }
            else if (bytesPerPix == 2)
            {
                uint16* scanline = reinterpret_cast<uint16*>(line);
                uint16 src = scanline[x];
                if (src == 0) continue;

                // Find best matching palette entry for the source color
                int32 bestIdx = 0;
                int32 bestDist = 0x7FFFFFFF;
                for (int32 i = 0; i < 256; ++i)
                {
                    int32 dr = ((src >> 11) & 0x1F) - ((pal[i] >> 11) & 0x1F);
                    int32 dg = ((src >> 5) & 0x3F) - ((pal[i] >> 5) & 0x3F);
                    int32 db = (src & 0x1F) - (pal[i] & 0x1F);
                    int32 dist = dr * dr * 2 + dg * dg * 4 + db * db * 2; // Perceptual weighting
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestIdx = i;
                    }
                }
                scanline[x] = pal[bestIdx];
            }
        }
    }
}

DWORD LightConvertClass::GetColor(int32 index) const
{
    if (!PaletteData || index < 0 || index >= 256) return 0;
    return reinterpret_cast<const uint16*>(PaletteData)[index];
}

void LightConvertClass::Apply(BYTE* buffer, int32 length) const
{
    if (!buffer || !PaletteData) return;
    uint16* pal = reinterpret_cast<uint16*>(PaletteData);
    for (int32 i = 0; i < length; ++i)
    {
        BYTE idx = buffer[i];
        // Apply the light table by finding the closest palette match
        uint16 target = pal[idx];
        int32 bestIdx = idx;
        int32 bestDist = 0x7FFFFFFF;
        for (int32 j = 0; j < 256; ++j)
        {
            int32 dr = ((target >> 11) & 0x1F) - ((pal[j] >> 11) & 0x1F);
            int32 dg = ((target >> 5) & 0x3F) - ((pal[j] >> 5) & 0x3F);
            int32 db = (target & 0x1F) - (pal[j] & 0x1F);
            int32 dist = dr * dr + dg * dg + db * db;
            if (dist < bestDist)
            {
                bestDist = dist;
                bestIdx = j;
            }
        }
        buffer[i] = static_cast<BYTE>(bestIdx);
    }
}

// ============================================================================
// ColorScheme implementation
// ============================================================================

ColorScheme* ColorScheme::FindByName(
    const char* pID, const ColorStruct& BaseColor,
    const BytePalette& Pal1, const BytePalette& Pal2, int32 ShadeCount)
{
    if (!pID || !Array) return nullptr;

    // Search for existing ColorScheme with matching ID
    for (int32 i = 0; i < Array->Size(); ++i)
    {
        ColorScheme* pItem = (*Array)[i];
        if (pItem && pItem->ID && strcmp(pItem->ID, pID) == 0 && pItem->ShadeCount == ShadeCount)
            return pItem;
    }

    // Not found, create a new one
    ColorScheme* pNew = new ColorScheme(pID, BaseColor, Pal1, Pal2, ShadeCount, true);
    if (Array)
        Array->Add(pNew);

    return pNew;
}

int32 ColorScheme::GetNumberOfSchemes()
{
    return Array ? Array->Size() : 0;
}

DynamicVectorClass<ColorScheme*>* ColorScheme::GeneratePalette(char* name)
{
    if (!Array)
        Array = new DynamicVectorClass<ColorScheme*>();

    if (!name) return Array;

    // Parse the palette file and create ColorScheme entries
    // In production, this would parse MIX/CSF data
    // For the engine reconstruction, we create default schemes

    // Default Allied blue scheme
    ColorStruct blue(0, 0, 255);
    BytePalette pal1, pal2;
    ColorScheme::FindByName("AlliedBlue", blue, pal1, pal2, 1);

    // Default Soviet red scheme
    ColorStruct red(255, 0, 0);
    ColorScheme::FindByName("SovietRed", red, pal1, pal2, 1);

    // Default Yuri purple scheme
    ColorStruct purple(128, 0, 128);
    ColorScheme::FindByName("YuriPurple", purple, pal1, pal2, 1);

    return Array;
}

ColorScheme::ColorScheme(
    const char* pID, const ColorStruct& BaseColor,
    const BytePalette& Pal1, const BytePalette& Pal2,
    int32 ShadeCount, bool AddToArray)
    : ArrayIndex(0)
    , ID(nullptr)
    , BaseColor(BaseColor)
    , LightConvert(nullptr)
    , ShadeCount(ShadeCount)
    , MainShadeIndex(0)
{
    memset(Colors.Entries, 0, sizeof(Colors.Entries));
    memset(unknown_314, 0, sizeof(unknown_314));
    memset(unknown_334, 0, sizeof(unknown_334));

    if (pID)
    {
        size_t len = strlen(pID) + 1;
        ID = static_cast<char*>(YRMemory::Allocate(len));
        if (ID) memcpy(ID, pID, len);
    }

    // Build the color palette entries
    // Each element in Colors.Entries maps a remap table index to the final color
    for (int32 i = 0; i < 256; ++i)
    {
        // Blend the base color with the palette entry
        int32 blend = (i < 16) ? 255 : 128;
        int32 r = (static_cast<int32>(BaseColor.R) * blend + static_cast<int32>(Pal1[i]) * (256 - blend)) / 256;
        int32 g = (static_cast<int32>(BaseColor.G) * blend + static_cast<int32>(Pal1[i]) * (256 - blend)) / 256;
        int32 b = (static_cast<int32>(BaseColor.B) * blend + static_cast<int32>(Pal1[i]) * (256 - blend)) / 256;

        if (r > 255) r = 255; if (r < 0) r = 0;
        if (g > 255) g = 255; if (g < 0) g = 0;
        if (b > 255) b = 255; if (b < 0) b = 0;

        Colors.Entries[i] = static_cast<BYTE>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }

    if (AddToArray && Array)
    {
        ArrayIndex = Array->Add(this);
    }
}

ColorScheme::~ColorScheme()
{
    if (ID)
        YRMemory::Deallocate(ID);
    ID = nullptr;
}