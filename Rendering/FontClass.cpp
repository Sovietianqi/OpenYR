// ============================================================================
// FontClass.cpp - Bitmap font rendering system implementation
// ============================================================================
// Implements Westwood FNT-format font loading and glyph rasterisation onto
// 8-bit indexed Surfaces.  Glyphs are stored as 1-bit-per-pixel bitmaps,
// row-major, most-significant-bit-first.  This module also provides text
// layout helpers (width measurement, alignment, word-wrap, multi-line draw).
// ============================================================================

#include "FontClass.h"

#include <cstring>
#include <cstdio>

// ============================================================================
// FontClass - construction / destruction
// ============================================================================

FontClass::FontClass() noexcept
    : PixelData(nullptr)
    , PixelDataSize(0)
    , Height(0)
    , MaxWidth(0)
    , Baseline(0)
{
    for (int32 i = 0; i < MAX_FONT_CHARS; ++i) {
        CharWidths[i] = 0;
    }
}

FontClass::~FontClass()
{
    Unload();
}

// ============================================================================
// Font data loading
// ============================================================================

bool FontClass::Load_From_Data(const uint8* pData, int32 dataSize)
{
    if (!pData || dataSize < static_cast<int32>(sizeof(FontHeader))) {
        return false;
    }

    Unload();

    // Read and validate the FNT header.
    const FontHeader* pHdr = reinterpret_cast<const FontHeader*>(pData);

    // The signature may be 'FNT' or may be absent in some variants.  Accept
    // the data as long as the offsets are sane.
    int32 height    = static_cast<int32>(pHdr->Height);
    int32 maxWidth  = static_cast<int32>(pHdr->MaxWidth);
    int32 widthOff  = static_cast<int32>(pHdr->WidthTableOffset);
    int32 pixelOff  = static_cast<int32>(pHdr->PixelDataOffset);

    if (height <= 0 || maxWidth <= 0) {
        return false;
    }

    // The width table is expected at widthOff (or right after the header if
    // the offset is zero).  It is 256 bytes.
    if (widthOff == 0) {
        widthOff = static_cast<int32>(sizeof(FontHeader));
    }
    if (widthOff < 0 || widthOff + MAX_FONT_CHARS > dataSize) {
        return false;
    }

    // The pixel data starts at pixelOff (or immediately after the width table).
    if (pixelOff == 0) {
        pixelOff = widthOff + MAX_FONT_CHARS;
    }
    if (pixelOff < 0 || pixelOff >= dataSize) {
        return false;
    }

    int32 availPixelData = dataSize - pixelOff;
    int32 bytesPerRow    = (maxWidth + 7) / 8;
    int32 expectedSize   = bytesPerRow * height * MAX_FONT_CHARS;

    // Use the smaller of the available data and the expected size so that
    // fonts with a compact (non-256-character) data block still load.
    int32 actualSize = (availPixelData < expectedSize) ? availPixelData : expectedSize;
    if (actualSize <= 0) {
        return false;
    }

    // Copy the width table.
    for (int32 i = 0; i < MAX_FONT_CHARS; ++i) {
        CharWidths[i] = pData[widthOff + i];
    }

    // Copy the pixel data.
    PixelData = static_cast<uint8*>(YRMemory::Allocate(actualSize));
    if (!PixelData) {
        return false;
    }
    std::memcpy(PixelData, pData + pixelOff, actualSize);
    PixelDataSize = actualSize;

    Height   = height;
    MaxWidth = maxWidth;
    Baseline = height - 1; // default: baseline at the bottom row (no descenders)

    return true;
}

bool FontClass::Load_From_File(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0') {
        return false;
    }

    // Open the file in binary mode and read its entire contents.
    FILE* pFile = nullptr;
    pFile = std::fopen(pFilename, "rb");
    if (!pFile) {
        return false;
    }

    // Determine file size.
    std::fseek(pFile, 0, SEEK_END);
    long fileSize = std::ftell(pFile);
    std::fseek(pFile, 0, SEEK_SET);

    if (fileSize <= 0) {
        std::fclose(pFile);
        return false;
    }

    uint8* pBuf = static_cast<uint8*>(YRMemory::Allocate(static_cast<size_t>(fileSize)));
    if (!pBuf) {
        std::fclose(pFile);
        return false;
    }

    size_t bytesRead = std::fread(pBuf, 1, static_cast<size_t>(fileSize), pFile);
    std::fclose(pFile);

    if (static_cast<long>(bytesRead) != fileSize) {
        YRMemory::Deallocate(pBuf);
        return false;
    }

    bool result = Load_From_Data(pBuf, static_cast<int32>(bytesRead));
    YRMemory::Deallocate(pBuf);
    return result;
}

bool FontClass::Load_From_Raw(int32 height, int32 maxWidth,
                              const uint8* pWidths, const uint8* pPixelData,
                              int32 pixelDataSize)
{
    if (height <= 0 || maxWidth <= 0 || !pWidths || !pPixelData || pixelDataSize <= 0) {
        return false;
    }

    Unload();

    // Copy the width table.
    for (int32 i = 0; i < MAX_FONT_CHARS; ++i) {
        CharWidths[i] = pWidths[i];
    }

    // Copy the pixel data.
    PixelData = static_cast<uint8*>(YRMemory::Allocate(pixelDataSize));
    if (!PixelData) {
        return false;
    }
    std::memcpy(PixelData, pPixelData, pixelDataSize);
    PixelDataSize = pixelDataSize;

    Height   = height;
    MaxWidth = maxWidth;
    Baseline = height - 1;

    return true;
}

void FontClass::Unload()
{
    if (PixelData) {
        YRMemory::Deallocate(PixelData);
        PixelData = nullptr;
    }
    PixelDataSize = 0;
    Height   = 0;
    MaxWidth = 0;
    Baseline = 0;
    for (int32 i = 0; i < MAX_FONT_CHARS; ++i) {
        CharWidths[i] = 0;
    }
}

// ============================================================================
// Font metrics
// ============================================================================

int32 FontClass::Get_Char_Width(char c) const
{
    // Treat the character as an unsigned index (0-255).
    uint8 idx = static_cast<uint8>(c);
    return CharWidths[idx];
}

int32 FontClass::Get_Char_Width_By_Index(int32 index) const
{
    if (index < 0 || index >= MAX_FONT_CHARS) {
        return 0;
    }
    return CharWidths[index];
}

int32 FontClass::Get_Text_Width(const char* pText) const
{
    if (!pText) {
        return 0;
    }
    int32 width = 0;
    for (const char* p = pText; *p != '\0'; ++p) {
        width += Get_Char_Width(*p);
    }
    return width;
}

int32 FontClass::Get_Text_Width_Wide(const wchar_t* pText) const
{
    if (!pText) {
        return 0;
    }
    int32 width = 0;
    for (const wchar_t* p = pText; *p != L'\0'; ++p) {
        int32 ch = static_cast<int32>(*p);
        if (ch >= 0 && ch < MAX_FONT_CHARS) {
            width += CharWidths[ch];
        }
    }
    return width;
}

int32 FontClass::Get_Line_Count(const char* pText, int32 wrapWidth) const
{
    if (!pText || pText[0] == '\0' || wrapWidth <= 0) {
        if (!pText) return 0;
        // If wrapWidth is invalid but text exists, count explicit newlines + 1.
        int32 count = 1;
        for (const char* p = pText; *p != '\0'; ++p) {
            if (*p == '\n') ++count;
        }
        return count;
    }

    int32 lineCount = 0;
    const char* pCursor = pText;
    char lineBuf[4096];

    while (pCursor != nullptr && *pCursor != '\0') {
        pCursor = Wrap_Line(pCursor, wrapWidth, lineBuf, sizeof(lineBuf));
        ++lineCount;
    }

    return lineCount;
}

int32 FontClass::Get_Text_Height(const char* pText, int32 wrapWidth) const
{
    int32 lines = Get_Line_Count(pText, wrapWidth);
    if (lines <= 0) {
        return 0;
    }
    return lines * Height;
}

// ============================================================================
// Internal glyph rendering
// ============================================================================

const uint8* FontClass::Get_Glyph_Data(int32 charIndex) const
{
    if (charIndex < 0 || charIndex >= MAX_FONT_CHARS) {
        return nullptr;
    }
    if (!PixelData) {
        return nullptr;
    }

    int32 glyphSize = Glyph_Data_Size();
    if (glyphSize <= 0) {
        return nullptr;
    }

    int32 offset = charIndex * glyphSize;
    if (offset + glyphSize > PixelDataSize) {
        // If the data is truncated, return what we can (the last valid glyph
        // or nullptr if even the first glyph is unavailable).
        if (offset < PixelDataSize) {
            return PixelData + offset;
        }
        return nullptr;
    }
    return PixelData + offset;
}

void FontClass::Draw_Glyph(int32 charIndex, Surface* pSurface,
                           int32 x, int32 y, uint8 color,
                           const Rectangle* pClipRect) const
{
    if (!pSurface || !pSurface->Buffer || charIndex < 0 || charIndex >= MAX_FONT_CHARS) {
        return;
    }

    int32 glyphW = CharWidths[charIndex];
    if (glyphW <= 0) {
        return;
    }

    const uint8* pGlyph = Get_Glyph_Data(charIndex);
    if (!pGlyph) {
        return;
    }

    int32 bytesPerRow = Bytes_Per_Row();
    int32 surfWidth   = pSurface->Width;
    int32 surfHeight  = pSurface->Height;
    int32 surfPitch   = pSurface->Pitch;

    // Determine clipping bounds.
    int32 clipX = 0, clipY = 0, clipW = surfWidth, clipH = surfHeight;
    if (pClipRect) {
        clipX = pClipRect->X;
        clipY = pClipRect->Y;
        clipW = pClipRect->Width;
        clipH = pClipRect->Height;
    }

    // Iterate over each row of the glyph.
    for (int32 row = 0; row < Height; ++row) {
        int32 dstY = y + row;
        if (dstY < clipY || dstY >= clipY + clipH || dstY < 0 || dstY >= surfHeight) {
            continue;
        }

        const uint8* pRow = pGlyph + row * bytesPerRow;
        BYTE* pDstLine = static_cast<BYTE*>(pSurface->Buffer) + dstY * surfPitch;

        // Iterate over each pixel column of the glyph.
        for (int32 col = 0; col < glyphW; ++col) {
            int32 dstX = x + col;
            if (dstX < clipX || dstX >= clipX + clipW || dstX < 0 || dstX >= surfWidth) {
                continue;
            }

            // Extract the bit for this column (MSB-first within each byte).
            int32 byteIndex = col / 8;
            int32 bitIndex  = 7 - (col % 8);
            if (byteIndex < bytesPerRow) {
                if (pRow[byteIndex] & (1 << bitIndex)) {
                    pDstLine[dstX] = color;
                }
            }
        }
    }
}

// ============================================================================
// Single-line text drawing
// ============================================================================

void FontClass::Draw_Text(const char* pText, Surface* pSurface,
                          int32 x, int32 y, uint8 color,
                          const Rectangle* pClipRect) const
{
    if (!pText || !pSurface) {
        return;
    }

    int32 curX = x;
    for (const char* p = pText; *p != '\0'; ++p) {
        if (*p == '\n') {
            // Newline: move to the next line.
            curX = x;
            y += Height;
            continue;
        }
        int32 idx = static_cast<uint8>(*p);
        Draw_Glyph(idx, pSurface, curX, y, color, pClipRect);
        curX += CharWidths[idx];
    }
}

void FontClass::Draw_Text_Wide(const wchar_t* pText, Surface* pSurface,
                               int32 x, int32 y, uint8 color,
                               const Rectangle* pClipRect) const
{
    if (!pText || !pSurface) {
        return;
    }

    int32 curX = x;
    for (const wchar_t* p = pText; *p != L'\0'; ++p) {
        if (*p == L'\n') {
            curX = x;
            y += Height;
            continue;
        }
        int32 idx = static_cast<int32>(*p);
        if (idx >= 0 && idx < MAX_FONT_CHARS) {
            Draw_Glyph(idx, pSurface, curX, y, color, pClipRect);
            curX += CharWidths[idx];
        }
    }
}

void FontClass::Draw_Text(const char* pText, Surface* pSurface,
                          const Rectangle& bounds, uint8 color,
                          uint8 backColor, TextPrintType flags) const
{
    if (!pText || !pSurface) {
        return;
    }

    // Measure the first line of text (up to the first newline or end).
    int32 firstLineLen = 0;
    while (pText[firstLineLen] != '\0' && pText[firstLineLen] != '\n') {
        ++firstLineLen;
    }
    int32 textWidth = 0;
    for (int32 i = 0; i < firstLineLen; ++i) {
        textWidth += Get_Char_Width(pText[i]);
    }

    // Compute the starting X position based on alignment.
    int32 startX = bounds.X;
    uint32 f = static_cast<uint32>(flags);
    uint32 alignFlags = f & 0x0003; // Left=0, Center=1, Right=2
    if (alignFlags == static_cast<uint32>(TextPrintType::Center)) {
        startX = bounds.X + (bounds.Width - textWidth) / 2;
        if (startX < bounds.X) startX = bounds.X;
    } else if (alignFlags == static_cast<uint32>(TextPrintType::Right)) {
        startX = bounds.X + bounds.Width - textWidth;
        if (startX < bounds.X) startX = bounds.X;
    }

    int32 startY = bounds.Y;

    // Draw shadow first if requested.
    bool noShadow = (f & static_cast<uint32>(TextPrintType::NoShadow)) != 0;
    bool fullShadow = (f & static_cast<uint32>(TextPrintType::FullShadow)) != 0;
    bool dropShadow = (f & static_cast<uint32>(TextPrintType::DropShadow)) != 0;

    if (!noShadow && (fullShadow || dropShadow)) {
        // Shadow is drawn offset by 1 pixel down-right.
        Rectangle shadowBounds = bounds;
        int32 shadowX = startX + 1;
        int32 shadowY = startY + 1;
        // Draw the text in shadow color.
        int32 curX = shadowX;
        for (int32 i = 0; i < firstLineLen; ++i) {
            int32 idx = static_cast<uint8>(pText[i]);
            Draw_Glyph(idx, pSurface, curX, shadowY, backColor, &shadowBounds);
            curX += CharWidths[idx];
        }
    }

    // Draw the main text.
    Rectangle clipRect = bounds;
    int32 curX = startX;
    for (int32 i = 0; i < firstLineLen; ++i) {
        int32 idx = static_cast<uint8>(pText[i]);
        Draw_Glyph(idx, pSurface, curX, startY, color, &clipRect);
        curX += CharWidths[idx];
    }
}

// ============================================================================
// Word-wrap
// ============================================================================

const char* FontClass::Wrap_Line(const char* pText, int32 maxPixelWidth,
                                 char* pOutBuffer, int32 bufferSize) const
{
    if (!pText || !pOutBuffer || bufferSize <= 0) {
        return nullptr;
    }
    pOutBuffer[0] = '\0';

    // Skip leading spaces.
    while (*pText == ' ' || *pText == '\t') {
        ++pText;
    }
    if (*pText == '\0') {
        return nullptr;
    }

    // Handle explicit newline: output an empty line and advance past it.
    if (*pText == '\n') {
        pOutBuffer[0] = '\0';
        return pText + 1;
    }

    const char* lineStart   = pText;
    const char* lastBreak   = nullptr; // position after the last acceptable word boundary
    const char* cur         = pText;
    int32 curWidth          = 0;
    int32 breakWidth        = 0;

    while (*cur != '\0' && *cur != '\n') {
        int32 charW = Get_Char_Width(*cur);
        if (curWidth + charW > maxPixelWidth) {
            // The current character would overflow the line.
            if (lastBreak != nullptr) {
                // Break at the last word boundary.
                int32 copyLen = static_cast<int32>(lastBreak - lineStart);
                if (copyLen >= bufferSize) copyLen = bufferSize - 1;
                std::memcpy(pOutBuffer, lineStart, copyLen);
                pOutBuffer[copyLen] = '\0';

                // Skip spaces after the break point.
                const char* next = lastBreak;
                while (*next == ' ' || *next == '\t') ++next;
                return next;
            } else {
                // No word boundary yet; break at the current position (single
                // long word that exceeds the width).
                int32 copyLen = static_cast<int32>(cur - lineStart);
                if (copyLen >= bufferSize) copyLen = bufferSize - 1;
                std::memcpy(pOutBuffer, lineStart, copyLen);
                pOutBuffer[copyLen] = '\0';
                return cur;
            }
        }

        curWidth += charW;

        // Track word boundaries (spaces).
        if (*cur == ' ' || *cur == '\t') {
            lastBreak   = cur + 1; // break after this space
            breakWidth  = curWidth;
        }

        ++cur;
    }

    // End of string or newline reached; output everything up to here.
    int32 copyLen = static_cast<int32>(cur - lineStart);
    if (copyLen >= bufferSize) copyLen = bufferSize - 1;
    std::memcpy(pOutBuffer, lineStart, copyLen);
    pOutBuffer[copyLen] = '\0';

    if (*cur == '\n') {
        return cur + 1;
    }
    return nullptr; // end of string
}

// ============================================================================
// Multi-line text drawing
// ============================================================================

void FontClass::Draw_Text_Multi(const char* pText, Surface* pSurface,
                                const Rectangle& bounds, uint8 color,
                                TextPrintType flags) const
{
    Draw_Text_Multi(pText, pSurface, bounds, color, 0, 0, flags);
}

void FontClass::Draw_Text_Multi(const char* pText, Surface* pSurface,
                                const Rectangle& bounds, uint8 color,
                                uint8 backColor, int32 lineSpacing,
                                TextPrintType flags) const
{
    if (!pText || !pSurface) {
        return;
    }

    int32 wrapWidth = bounds.Width;
    if (wrapWidth <= 0) {
        wrapWidth = pSurface->Width - bounds.X;
    }
    if (wrapWidth <= 0) {
        return;
    }

    uint32 f = static_cast<uint32>(flags);
    uint32 alignFlags = f & 0x0003;

    int32 curY = bounds.Y;
    const char* pCursor = pText;
    char lineBuf[4096];

    while (pCursor != nullptr && *pCursor != '\0') {
        pCursor = Wrap_Line(pCursor, wrapWidth, lineBuf, sizeof(lineBuf));
        if (lineBuf[0] == '\0') {
            // Empty line (e.g. from a newline); still advance vertically.
            curY += Height + lineSpacing;
            continue;
        }

        // Measure this line for alignment.
        int32 lineWidth = Get_Text_Width(lineBuf);

        int32 startX = bounds.X;
        if (alignFlags == static_cast<uint32>(TextPrintType::Center)) {
            startX = bounds.X + (bounds.Width - lineWidth) / 2;
            if (startX < bounds.X) startX = bounds.X;
        } else if (alignFlags == static_cast<uint32>(TextPrintType::Right)) {
            startX = bounds.X + bounds.Width - lineWidth;
            if (startX < bounds.X) startX = bounds.X;
        }

        // Clip vertically: stop if we've gone past the bottom of the bounds.
        if (curY + Height > bounds.Y + bounds.Height && bounds.Height > 0) {
            break;
        }

        // Draw shadow if requested.
        bool noShadow = (f & static_cast<uint32>(TextPrintType::NoShadow)) != 0;
        bool fullShadow = (f & static_cast<uint32>(TextPrintType::FullShadow)) != 0;
        bool dropShadow = (f & static_cast<uint32>(TextPrintType::DropShadow)) != 0;

        Rectangle lineClip = bounds;

        if (!noShadow && (fullShadow || dropShadow)) {
            int32 shadowX = startX + 1;
            int32 shadowY = curY + 1;
            int32 cx = shadowX;
            for (int32 i = 0; lineBuf[i] != '\0'; ++i) {
                int32 idx = static_cast<uint8>(lineBuf[i]);
                Draw_Glyph(idx, pSurface, cx, shadowY, backColor, &lineClip);
                cx += CharWidths[idx];
            }
        }

        // Draw main text.
        int32 cx = startX;
        for (int32 i = 0; lineBuf[i] != '\0'; ++i) {
            int32 idx = static_cast<uint8>(lineBuf[i]);
            Draw_Glyph(idx, pSurface, cx, curY, color, &lineClip);
            cx += CharWidths[idx];
        }

        curY += Height + lineSpacing;
    }
}

// ============================================================================
// FontControlClass
// ============================================================================

FontControlClass::FontControlClass()
    : CurrentFont(nullptr)
    , CurrentID(FONT_NONE)
{
    for (int32 i = 0; i < FONT_COUNT; ++i) {
        Fonts[i] = nullptr;
    }
}

FontControlClass::~FontControlClass()
{
    // The FontControlClass does not own the FontClass pointers; the caller
    // is responsible for their lifetime.  We only clear our references.
    for (int32 i = 0; i < FONT_COUNT; ++i) {
        Fonts[i] = nullptr;
    }
    CurrentFont = nullptr;
    CurrentID = FONT_NONE;
}

void FontControlClass::Set_Font(FontID id, FontClass* pFont)
{
    if (id < 0 || id >= FONT_COUNT) {
        return;
    }
    Fonts[id] = pFont;
}

FontClass* FontControlClass::Get_Font(FontID id) const
{
    if (id < 0 || id >= FONT_COUNT) {
        return nullptr;
    }
    return Fonts[id];
}

void FontControlClass::Set_Current_Font(FontID id)
{
    if (id < 0 || id >= FONT_COUNT) {
        return;
    }
    CurrentID   = id;
    CurrentFont = Fonts[id];
}

void FontControlClass::Draw(const char* pText, Surface* pSurface,
                            int32 x, int32 y, uint8 color) const
{
    if (!CurrentFont) {
        return;
    }
    CurrentFont->Draw_Text(pText, pSurface, x, y, color, nullptr);
}
