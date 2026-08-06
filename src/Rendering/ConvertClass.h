#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Rendering/Surface.h"
#include "Rendering/Blitter.h"
#include "FileFormats/SHP.h"

#include <cstring>

// Forward declarations
class Blitter;
class RLEBlitter;

// ============================================================================
// ConvertClass - Palette conversion and blitter management
//
// Manages color palette conversion tables and selects appropriate blitters
// based on rendering flags. Each ConvertClass instance holds a set of
// pre-configured blitters for different rendering modes.
// ============================================================================

class ConvertClass
{
public:
    // Global array of all ConvertClass instances
    static DynamicVectorClass<ConvertClass*>* Array;

    // Constructor
    ConvertClass(
        const BytePalette& palette,
        const BytePalette& eightBitPalette,
        DSurface* pSurface,
        size_t shadeCount,
        bool skipBlitters);

    virtual ~ConvertClass();

    // Select a plain (non-RLE) blitter based on flags
    Blitter* SelectPlainBlitter(BlitterFlags flags) const;

    // Select an RLE blitter based on flags
    RLEBlitter* SelectRLEBlitter(BlitterFlags flags) const;

    // Find or allocate a ConvertClass by filename
    static ConvertClass* FindOrAllocate(const char* pFilename);

    // Create a ConvertClass from a palette file
    static void CreateFromFile(
        const char* pFilename, BytePalette*& pPalette, ConvertClass*& pDestination);

protected:
    explicit ConvertClass(int) {} // noinit constructor

    // Build blitter tables
    void BuildBlitters(DSurface* pSurface, bool skipBlitters);

    // Build translucency masks from pixel format
    void BuildTranslucencyMasks();

public:
    int32 BytesPerPixel;
    Blitter* Blitters[50];
    RLEBlitter* RLEBlitters[39];
    int32 ShadeCount;
    void* FullColorData;
    void* PaletteData;
    void* ByteColorData;
    DWORD CurrentZRemap;
    uint16 HalfTranslucencyMask;
    uint16 QuatTranslucencyMask;
};

// ============================================================================
// LightConvertClass - Brightness/color conversion with lighting
//
// Manages brightness-adjusted color palettes for unit rendering.
// Each LightConvertClass represents a specific tint/brightness combination.
// ============================================================================

class LightConvertClass : public ConvertClass
{
public:
    // Global array of LightConvertClass instances
    static DynamicVectorClass<LightConvertClass*>* Array;

    // Constructor
    LightConvertClass(
        BytePalette* palette1,
        BytePalette* palette2,
        Surface* pSurface,
        int32 colorR,
        int32 colorG,
        int32 colorB,
        bool skipBlitters,
        BYTE* pBuffer,
        size_t shadeCount);

    virtual ~LightConvertClass();

    // Update the color values
    virtual void UpdateColors(int32 red, int32 green, int32 blue, bool tinted);

    // Initialize a LightConvertClass with given RGB values
    static LightConvertClass* InitLightConvert(int32 red, int32 green, int32 blue);

    // Draw the light convert effect to a surface
    void DrawIt(Surface* pSurface, Rectangle rect, int32 flags = 0);

    // Get the color at a given index
    DWORD GetColor(int32 index) const;

    // Apply the light conversion to a buffer
    void Apply(BYTE* buffer, int32 length) const;

protected:
    explicit LightConvertClass(int)
        : ConvertClass(0) {}

    void BuildLightTables(int32 colorR, int32 colorG, int32 colorB, bool tinted);

public:
    RGBClass* UsedPalette1;
    RGBClass* UsedPalette2;
    BYTE* IndexesToIgnore;
    int32 RefCount;
    TintStruct Color1;
    TintStruct Color2;
    bool Tinted;
    BYTE align_1B1[3];
};

// ============================================================================
// ColorScheme - Player color scheme management
// ============================================================================

class ColorScheme
{
public:
    enum {
        Yellow = 3,
        White = 5,
        Grey = 7,
        Red = 11,
        Orange = 13,
        Pink = 15,
        Purple = 17,
        Blue = 21,
        Green = 29,
    };

    // Global array
    static DynamicVectorClass<ColorScheme*>* Array;

    // Find a ColorScheme by ID
    static ColorScheme* Find(const char* pID, int32 ShadeCount = 1)
    {
        int32 index = FindIndex(pID, ShadeCount);
        return Array ? Array->operator[](index) : nullptr;
    }

    static int32 FindIndex(const char* pID, int32 ShadeCount = 1)
    {
        if (!Array) return -1;
        for (int32 i = 0; i < Array->Size(); ++i)
        {
            ColorScheme* pItem = (*Array)[i];
            if (pItem && strcmp(pItem->ID, pID) == 0)
            {
                if (pItem->ShadeCount == ShadeCount)
                    return i;
            }
        }
        return -1;
    }

    static ColorScheme* FindByName(
        const char* pID, const ColorStruct& BaseColor,
        const BytePalette& Pal1, const BytePalette& Pal2, int32 ShadeCount);

    static int32 GetNumberOfSchemes();

    static DynamicVectorClass<ColorScheme*>* GeneratePalette(char* name);

    // Constructor / Destructor
    ColorScheme(
        const char* pID, const ColorStruct& BaseColor,
        const BytePalette& Pal1, const BytePalette& Pal2,
        int32 ShadeCount, bool AddToArray);

    ~ColorScheme();

    // Properties
    int32 ArrayIndex;
    BytePalette Colors;
    char* ID;
    ColorStruct BaseColor;
    LightConvertClass* LightConvert;
    int32 ShadeCount;
    BYTE unknown_314[0x1C];
    int32 MainShadeIndex;
    BYTE unknown_334[0x8];
};