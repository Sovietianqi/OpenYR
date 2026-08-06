#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Core/Memory.h"
#include "Rendering/Surface.h"
#include "IO/CCFileClass.h"

#include <cstring>

// ============================================================================
// PCX file format reader (ZSoft PC Paintbrush format)
//
// PCX is the image format used for loading screens, sidebar backgrounds,
// and other static images in C&C. It supports palette-based 8-bit images
// with optional RLE compression.
// ============================================================================

// ============================================================================
// PCXHeader - 128-byte PCX file header
// ============================================================================
#pragma pack(push, 1)
struct PCXHeader
{
    BYTE Manufacturer;      // Always 0x0A (ZSoft)
    BYTE Version;           // 0 = v2.5, 2 = v2.8 with palette, 5 = v3.0
    BYTE Encoding;          // 1 = RLE compressed
    BYTE BitsPerPixel;      // 1, 2, 4, or 8 bits per pixel
    int16 XMin;             // Left edge of the image
    int16 YMin;             // Top edge of the image
    int16 XMax;             // Right edge of the image
    int16 YMax;             // Bottom edge of the image
    int16 DPI_Width;        // Horizontal DPI
    int16 DPI_Height;       // Vertical DPI
    BYTE Palette[48];       // 16-color palette (for 4-bit images)
    BYTE Reserved;          // Should be 0
    BYTE NumPlanes;         // Number of color planes
    int16 BytesPerLine;     // Number of bytes per scan line per plane
    int16 PaletteInfo;      // 1 = color, 2 = grayscale
    int16 ScreenWidth;      // Horizontal screen size
    int16 ScreenHeight;     // Vertical screen size
    BYTE Filler[54];        // Fill to 128 bytes
};
#pragma pack(pop)

static_assert(sizeof(PCXHeader) == 128, "PCXHeader must be 128 bytes");

// ============================================================================
// PCXClass - PCX image reader
// ============================================================================
class PCXClass
{
public:
    PCXClass();
    PCXClass(const char* pFilename);
    ~PCXClass();

    // Load from file
    bool LoadFromFile(const char* pFilename);

    // Get the image dimensions
    int32 GetWidth() const { return m_Width; }
    int32 GetHeight() const { return m_Height; }

    // Get the palette data (256-color palette, 768 bytes)
    const BYTE* GetPalette() const { return m_Palette; }

    // Get the raw image data
    const BYTE* GetImage() const { return m_pImageData; }

    // Get the VGA palette (used for in-game rendering)
    const BYTE* GetVgaPalette() const { return m_VgaPalette; }

    // Get the header
    const PCXHeader& GetHeader() const { return m_Header; }

    // Check if the file was loaded successfully
    bool IsLoaded() const { return m_bLoaded; }

    // Decode a single scanline of RLE-compressed data
    static int32 DecodeScanline(
        BYTE* pDst, const BYTE* pSrc, int32 srcLen, int32 width);

private:
    PCXHeader m_Header;
    BYTE m_Palette[768];        // 256-color palette
    BYTE m_VgaPalette[768];     // VGA format palette
    BYTE* m_pImageData;
    int32 m_Width;
    int32 m_Height;
    bool m_bLoaded;
    bool m_bOwnsData;
};