#pragma once

// ============================================================================
// FontClass - Bitmap font rendering system
//
// Loads and renders Westwood FNT-format bitmap fonts. Each font contains a
// fixed-height glyph strip with per-character widths, a baseline offset, and
// 1-bit-per-pixel glyph data. Text is blitted onto an 8-bit indexed Surface
// using a caller-supplied palette index (color).
//
// The class also provides high-level text layout helpers: width/height
// measurement, horizontal alignment, multi-line word-wrap, and clipping.
// ============================================================================

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/Rectangle.h"
#include "Surface.h"

#include <cstdint>

// ----------------------------------------------------------------------------
// Maximum number of characters a single font can address (full extended
// ASCII / Westwood code-page range).
// ----------------------------------------------------------------------------
constexpr int32 MAX_FONT_CHARS = 256;

// ----------------------------------------------------------------------------
// FNT file header (Westwood bitmap font format).
//
//   Offset  Size  Field
//   0x00    3     Signature: 'FNT'
//   0x03    1     Unknown/reserved
//   0x04    2     Height of each glyph row in pixels
//   0x06    2     Maximum glyph width in pixels
//   0x08    2     Unknown
//   0x0A    2     Offset to the width table
//   0x0C    4     Offset to the glyph pixel data
//   0x10    4     Unknown
//
// The width table is 256 bytes (one byte per character).
// The glyph data is a contiguous strip of height * max_width / 8 bytes per
// character, laid out row-by-row, most-significant-bit first within each byte.
// ----------------------------------------------------------------------------
struct FontHeader
{
    char     Signature[3];
    uint8    Reserved;
    uint16   Height;
    uint16   MaxWidth;
    uint16   Unknown08;
    uint16   WidthTableOffset;
    uint32   PixelDataOffset;
    uint32   Unknown10;
};

// ============================================================================
// FontClass
// ============================================================================
class FontClass
{
public:
    // ----------------------------------------------------------------------
    // Construction / destruction
    // ----------------------------------------------------------------------
    FontClass() noexcept;
    ~FontClass();

    // ----------------------------------------------------------------------
    // Font data loading
    // ----------------------------------------------------------------------

    // Load a font from an in-memory FNT file buffer.  The buffer is copied
    // internally so the caller may free it immediately after the call.
    bool Load_From_Data(const uint8* pData, int32 dataSize);

    // Load a font from a FNT file on disk.
    bool Load_From_File(const char* pFilename);

    // Load a font from raw embedded glyph data (used for hard-coded fonts).
    //   height        - pixel height of each glyph row
    //   maxWidth      - maximum glyph width (bytes per row = (maxWidth+7)/8)
    //   pWidths       - array of 256 per-character widths (in pixels)
    //   pPixelData    - glyph bitmap data, row-major, MSB-first
    //   pixelDataSize - size of pPixelData in bytes
    bool Load_From_Raw(int32 height, int32 maxWidth,
                       const uint8* pWidths, const uint8* pPixelData,
                       int32 pixelDataSize);

    // Release all allocated resources.
    void Unload();

    // ----------------------------------------------------------------------
    // Font metrics
    // ----------------------------------------------------------------------

    int32 Get_Height()        const { return Height; }
    int32 Get_Max_Width()     const { return MaxWidth; }
    int32 Get_Baseline()      const { return Baseline; }
    bool  Is_Loaded()         const { return PixelData != nullptr; }

    // Width of a single character in pixels (0 for characters outside range).
    int32 Get_Char_Width(char c) const;

    // Width of a single character by index (0-255).
    int32 Get_Char_Width_By_Index(int32 index) const;

    // Total pixel width of a string (sum of character widths, no kerning).
    int32 Get_Text_Width(const char* pText) const;

    // Total pixel width of a wide-character string.
    int32 Get_Text_Width_Wide(const wchar_t* pText) const;

    // Pixel height of a block of text with word-wrap at the given width.
    int32 Get_Text_Height(const char* pText, int32 wrapWidth) const;

    // Number of lines that result from wrapping pText at wrapWidth.
    int32 Get_Line_Count(const char* pText, int32 wrapWidth) const;

    // ----------------------------------------------------------------------
    // Character width table access
    // ----------------------------------------------------------------------

    const uint8* Get_Width_Table() const { return CharWidths; }

    // ----------------------------------------------------------------------
    // Single-line text drawing
    // ----------------------------------------------------------------------

    // Draw a string at (x, y) on the given surface with the specified palette
    // index.  If pClipRect is non-null, glyphs are clipped to that rectangle.
    void Draw_Text(const char* pText, Surface* pSurface,
                   int32 x, int32 y, uint8 color,
                   const Rectangle* pClipRect = nullptr) const;

    // Draw a wide-character string.
    void Draw_Text_Wide(const wchar_t* pText, Surface* pSurface,
                        int32 x, int32 y, uint8 color,
                        const Rectangle* pClipRect = nullptr) const;

    // Draw text with alignment and shadow support.
    //   pBounds   - bounding rectangle for alignment / clipping
    //   color     - foreground palette index
    //   backColor - shadow palette index (ignored if no shadow flag)
    //   flags     - TextPrintType alignment and shadow flags
    void Draw_Text(const char* pText, Surface* pSurface,
                   const Rectangle& bounds, uint8 color, uint8 backColor,
                   TextPrintType flags) const;

    // ----------------------------------------------------------------------
    // Multi-line text drawing with word-wrap
    // ----------------------------------------------------------------------

    // Draw multi-line text within a bounding rectangle.  Text is word-wrapped
    // at the right edge of the bounds.  Vertical alignment is top-justified.
    void Draw_Text_Multi(const char* pText, Surface* pSurface,
                         const Rectangle& bounds, uint8 color,
                         TextPrintType flags = TextPrintType::Left) const;

    // Draw multi-line text with line spacing (in pixels) between lines.
    void Draw_Text_Multi(const char* pText, Surface* pSurface,
                         const Rectangle& bounds, uint8 color,
                         uint8 backColor, int32 lineSpacing,
                         TextPrintType flags) const;

    // ----------------------------------------------------------------------
    // Word-wrap helper: fills the provided line buffer with one line of text
    // that fits within maxPixelWidth.  Returns a pointer to the start of the
    // next line, or nullptr when the entire string has been consumed.
    // The output buffer must be at least as large as the input text.
    // ----------------------------------------------------------------------
    const char* Wrap_Line(const char* pText, int32 maxPixelWidth,
                          char* pOutBuffer, int32 bufferSize) const;

private:
    // ----------------------------------------------------------------------
    // Internal: draw a single glyph at (x, y) on the surface.
    // ----------------------------------------------------------------------
    void Draw_Glyph(int32 charIndex, Surface* pSurface,
                    int32 x, int32 y, uint8 color,
                    const Rectangle* pClipRect) const;

    // Bytes per glyph row (derived from MaxWidth).
    int32 Bytes_Per_Row() const { return (MaxWidth + 7) / 8; }

    // Size in bytes of a single character's bitmap data.
    int32 Glyph_Data_Size() const { return Bytes_Per_Row() * Height; }

    // Pointer to the start of a specific character's bitmap data.
    const uint8* Get_Glyph_Data(int32 charIndex) const;

    // ----------------------------------------------------------------------
    // Font data
    // ----------------------------------------------------------------------
    uint8  CharWidths[MAX_FONT_CHARS];  // per-character width in pixels
    uint8* PixelData;                   // glyph bitmap data (owned)
    int32  PixelDataSize;               // size of PixelData in bytes
    int32  Height;                      // glyph row height in pixels
    int32  MaxWidth;                    // maximum glyph width in pixels
    int32  Baseline;                    // baseline offset from top (for descenders)

    // Disable copy
    FontClass(const FontClass&) = delete;
    FontClass& operator=(const FontClass&) = delete;
};

// ============================================================================
// FontControlClass - manages a set of named fonts (singleton-like registry)
//
// The original game keeps a small set of pre-loaded fonts identified by an
// enum index. This helper provides the same look-up semantics.
// ============================================================================
class FontControlClass
{
public:
    enum FontID : int32
    {
        FONT_NONE      = -1,
        FONT_TINY      = 0,
        FONT_SMALL     = 1,
        FONT_NORMAL    = 2,
        FONT_LARGE     = 3,
        FONT_VCR       = 4,
        FONT_6POINT    = 5,
        FONT_8POINT    = 6,
        FONT_12POINT   = 7,
        FONT_14POINT   = 8,
        FONT_18POINT   = 9,
        FONT_TITLE     = 10,
        FONT_COUNT     = 11
    };

    FontControlClass();
    ~FontControlClass();

    // Register a loaded font under the given ID.
    void Set_Font(FontID id, FontClass* pFont);

    // Retrieve a font by ID (returns nullptr if not registered).
    FontClass* Get_Font(FontID id) const;

    // Get the currently active font.
    FontClass* Get_Current_Font() const { return CurrentFont; }

    // Set the currently active font.
    void Set_Current_Font(FontID id);

    // Draw text using the current font (convenience wrapper).
    void Draw(const char* pText, Surface* pSurface,
              int32 x, int32 y, uint8 color) const;

private:
    FontClass*   Fonts[FONT_COUNT];
    FontClass*   CurrentFont;
    FontID       CurrentID;
};
