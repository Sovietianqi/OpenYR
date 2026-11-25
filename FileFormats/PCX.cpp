#include "PCX.h"
#include "IO/FileSystem.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// PCXClass - ZSoft PCX image reader
//
//  The original PCX_LoadFile (0x6B6670 area) pipeline:
//    1. read the 128-byte PCXHeader
//    2. RLE-decode width*height bytes of plane-0 data
//    3. if buildPalette: seek back 769 bytes from EOF, expect 0x0C magic,
//       read the 768-byte VGA palette, then expand it into the 3-byte-per-
//       entry m_Palette table
//  PCX_LoadFile11 (0x6B68E0) drives this with both flag arguments = 1.
// ============================================================================

PCXClass::PCXClass()
    : m_pImageData(nullptr)
    , m_Width(0)
    , m_Height(0)
    , m_bLoaded(false)
    , m_bOwnsData(true)
{
    std::memset(m_Palette, 0, sizeof(m_Palette));
    std::memset(m_VgaPalette, 0, sizeof(m_VgaPalette));
    std::memset(&m_Header, 0, sizeof(m_Header));
}

PCXClass::PCXClass(const char* pFilename)
    : PCXClass()
{
    LoadFromFile(pFilename);
}

PCXClass::~PCXClass()
{
    // asm: DTOR releases the decoded image block
    if (m_pImageData != nullptr && m_bOwnsData) {
        delete[] m_pImageData;
        m_pImageData = nullptr;
    }
}

// ----------------------------------------------------------------------------
// asm: DecodeScanline -- RLE decoder: byte < 0xC0 is literal, otherwise the
// low 6 bits are a run count and the next byte is the fill value.
// ----------------------------------------------------------------------------
int32 PCXClass::DecodeScanline(BYTE* pDst, const BYTE* pSrc, int32 srcLen,
                               int32 width)
{
    int32 srcPos = 0;
    int32 dstPos = 0;

    while (dstPos < width && srcPos < srcLen) {
        const BYTE b = pSrc[srcPos++];
        if ((b & 0xC0) == 0xC0) {
            const int32 run = b & 0x3F;                 // run length (low 6 bits)
            if (srcPos >= srcLen)
                break;
            const BYTE fill = pSrc[srcPos++];
            const int32 n = (dstPos + run > width) ? (width - dstPos) : run;
            std::memset(pDst + dstPos, fill, static_cast<size_t>(n));
            dstPos += n;
        } else {
            pDst[dstPos++] = b;                          // literal byte
        }
    }
    return srcPos;
}

// ----------------------------------------------------------------------------
// asm: PCX_LoadFile core -- full load pipeline described in the header.
// ----------------------------------------------------------------------------
bool PCXClass::LoadFromFile(const char* pFilename)
{
    if (pFilename == nullptr)
        return false;

    // asm: fopen / fread header (128 bytes)
    std::FILE* fp = File_Open_Archive_Stream(pFilename);
    if (fp == nullptr)
        return false;

    if (std::fread(&m_Header, 1, sizeof(m_Header), fp) != sizeof(m_Header)) {
        std::fclose(fp);
        return false;
    }

    // asm: sanity -- magic 0x0A, 8bpp
    if (m_Header.Manufacturer != 0x0A || m_Header.BitsPerPixel != 8) {
        std::fclose(fp);
        return false;
    }

    m_Width  = static_cast<int32>(m_Header.XMax - m_Header.XMin) + 1;
    m_Height = static_cast<int32>(m_Header.YMax - m_Header.YMin) + 1;
    if (m_Width <= 0 || m_Height <= 0) {
        std::fclose(fp);
        return false;
    }

    // asm: RLE decode width*height bytes (bytes-per-line aligned to even)
    const int32 bytesPerLine = (m_Header.BytesPerLine > 0)
                             ? m_Header.BytesPerLine
                             : ((m_Width + 1) & ~1);
    const int32 totalRaw = bytesPerLine * m_Height;

    BYTE* pRaw = new BYTE[totalRaw];
    if (std::fread(pRaw, 1, static_cast<size_t>(totalRaw), fp) !=
        static_cast<size_t>(totalRaw)) {
        delete[] pRaw;
        std::fclose(fp);
        return false;
    }

    m_pImageData = new BYTE[m_Width * m_Height];
    for (int32 y = 0; y < m_Height; ++y) {
        DecodeScanline(m_pImageData + y * m_Width,
                       pRaw + y * bytesPerLine, bytesPerLine, m_Width);
    }
    delete[] pRaw;

    // asm: palette tail -- 769 bytes before EOF, 0x0C magic + 768 RGB triplets
    // (PCX_LoadFile is called with buildPalette = 1 from LoadFile11)
    const long curPos = std::ftell(fp);
    if (std::fseek(fp, -769L, SEEK_END) == 0) {
        BYTE magic = 0;
        if (std::fread(&magic, 1, 1, fp) == 1 && magic == 0x0C) {
            BYTE vga[768];
            if (std::fread(vga, 1, 768, fp) == 768) {
                std::memcpy(m_VgaPalette, vga, 768);
                // asm: VGA 6-bit components -> 8-bit (v << 2 | v >> 4 style
                // expansion used by the original's ConvertPal)
                for (int32 i = 0; i < 256; ++i) {
                    const BYTE r = vga[i * 3 + 0];
                    const BYTE g = vga[i * 3 + 1];
                    const BYTE b = vga[i * 3 + 2];
                    m_Palette[i * 3 + 0] = static_cast<BYTE>((r << 2) | (r >> 6));
                    m_Palette[i * 3 + 1] = static_cast<BYTE>((g << 2) | (g >> 6));
                    m_Palette[i * 3 + 2] = static_cast<BYTE>((b << 2) | (b >> 6));
                }
            }
        }
    }

    // restore stream position (the original keeps it at the image end)
    std::fseek(fp, curPos, SEEK_SET);

    std::fclose(fp);
    m_bLoaded = true;
    return true;
}
