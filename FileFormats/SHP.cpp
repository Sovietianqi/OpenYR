#include "FileFormats/SHP.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

// ============================================================================
// SHPStruct method implementations
// ============================================================================

// ----------------------------------------------------------------------------
// SHPStruct::Load - Load file data if this is a reference
// ----------------------------------------------------------------------------
void SHPStruct::Load()
{
    SHPReference* pRef = AsReference();
    if (!pRef)
        return;

    if (pRef->Loaded)
        return;

    if (pRef->Data)
    {
        // Copy metadata from the loaded data
        SHPFile* pFile = static_cast<SHPFile*>(pRef->Data);
        Width  = pFile->Width;
        Height = pFile->Height;
        Frames = pFile->Frames;
        pRef->Loaded = true;
    }
}

// ----------------------------------------------------------------------------
// SHPStruct::Unload - Unload data if this is a reference
// ----------------------------------------------------------------------------
void SHPStruct::Unload()
{
    SHPReference* pRef = AsReference();
    if (!pRef)
        return;

    if (!pRef->Loaded)
        return;

    Width  = 0;
    Height = 0;
    Frames = 0;
    pRef->Loaded = false;
}

// ----------------------------------------------------------------------------
// SHPStruct::GetData - Resolve to actual file data, loading if necessary
// ----------------------------------------------------------------------------
SHPFile* SHPStruct::GetData()
{
    if (IsReference())
    {
        Load();
        SHPReference* pRef = AsReference();
        if (pRef && pRef->Data)
            return static_cast<SHPFile*>(pRef->Data);
        return nullptr;
    }
    return AsFile();
}

// ----------------------------------------------------------------------------
// SHPStruct::GetFrameBounds(buffer, idxFrame) - Get bounding rect for a frame
// ----------------------------------------------------------------------------
Rectangle* SHPStruct::GetFrameBounds(Rectangle& buffer, int32 idxFrame) const
{
    if (idxFrame < 0 || idxFrame >= Frames)
    {
        buffer = Rectangle(0, 0, 0, 0);
        return &buffer;
    }

    const SHPFile* pFile = nullptr;
    if (IsReference())
    {
        const SHPReference* pRef = AsReference();
        if (pRef && pRef->Loaded && pRef->Data)
            pFile = static_cast<const SHPFile*>(pRef->Data);
    }
    else
    {
        pFile = AsFile();
    }

    if (!pFile)
    {
        buffer = Rectangle(0, 0, 0, 0);
        return &buffer;
    }

    const SHPFrame& frame = pFile->GetFrameHeader(idxFrame);
    buffer = Rectangle(frame.Left, frame.Top, frame.Width, frame.Height);
    return &buffer;
}

// ----------------------------------------------------------------------------
// SHPStruct::GetFrameBounds(idxFrame) - Convenience overload
// ----------------------------------------------------------------------------
Rectangle SHPStruct::GetFrameBounds(int32 idxFrame) const
{
    Rectangle buffer;
    GetFrameBounds(buffer, idxFrame);
    return buffer;
}

// ----------------------------------------------------------------------------
// SHPStruct::GetColor(buffer, idxFrame) - Get average color of a frame
// ----------------------------------------------------------------------------
ColorStruct* SHPStruct::GetColor(ColorStruct& buffer, int32 idxFrame) const
{
    if (idxFrame < 0 || idxFrame >= Frames)
    {
        buffer = ColorStruct(0, 0, 0);
        return &buffer;
    }

    const SHPFile* pFile = nullptr;
    if (IsReference())
    {
        const SHPReference* pRef = AsReference();
        if (pRef && pRef->Loaded && pRef->Data)
            pFile = static_cast<const SHPFile*>(pRef->Data);
    }
    else
    {
        pFile = AsFile();
    }

    if (!pFile)
    {
        buffer = ColorStruct(0, 0, 0);
        return &buffer;
    }

    const SHPFrame& frame = pFile->GetFrameHeader(idxFrame);
    buffer = frame.Color;
    return &buffer;
}

// ----------------------------------------------------------------------------
// SHPStruct::GetColor(idxFrame) - Convenience overload
// ----------------------------------------------------------------------------
ColorStruct SHPStruct::GetColor(int32 idxFrame) const
{
    ColorStruct buffer;
    GetColor(buffer, idxFrame);
    return buffer;
}

// ----------------------------------------------------------------------------
// SHPStruct::GetPixels - Get raw pixel data for a frame
// ----------------------------------------------------------------------------
BYTE* SHPStruct::GetPixels(int32 idxFrame)
{
    if (idxFrame < 0 || idxFrame >= Frames)
        return nullptr;

    SHPFile* pFile = GetData();
    if (!pFile)
        return nullptr;

    const SHPFrame& frame = pFile->GetFrameHeader(idxFrame);

    // Pixel data immediately follows the frame header array in the file
    // The frame header Offset points to the pixel data relative to file start
    BYTE* pBase = reinterpret_cast<BYTE*>(pFile);
    BYTE* pPixels = pBase + frame.Offset;

    return pPixels;
}

// ----------------------------------------------------------------------------
// SHPStruct::HasCompression - Check if a frame uses RLE compression
// Flags & 2 = LCW compressed
// ----------------------------------------------------------------------------
bool SHPStruct::HasCompression(int32 idxFrame) const
{
    if (idxFrame < 0 || idxFrame >= Frames)
        return false;

    const SHPFile* pFile = nullptr;
    if (IsReference())
    {
        const SHPReference* pRef = AsReference();
        if (pRef && pRef->Loaded && pRef->Data)
            pFile = static_cast<const SHPFile*>(pRef->Data);
    }
    else
    {
        pFile = AsFile();
    }

    if (!pFile)
        return false;

    const SHPFrame& frame = pFile->GetFrameHeader(idxFrame);
    return (frame.Flags & 0x0002) != 0;
}

// ============================================================================
// SHPReference constructor
// ============================================================================
SHPReference::SHPReference(const char* filename)
    : SHPStruct()
    , Filename(nullptr)
    , Data(nullptr)
    , Loaded(false)
    , Index(-1)
    , Next(nullptr)
    , Prev(nullptr)
    , unknown_20(0)
{
    // Set type to reference marker
    Type = 0xFFFF;

    // Copy the filename
    if (filename)
    {
        size_t len = std::strlen(filename);
        Filename = static_cast<char*>(std::malloc(len + 1));
        if (Filename)
        {
            std::memcpy(Filename, filename, len + 1);
        }
    }
}

// ============================================================================
// SHPClass implementation
// ============================================================================

// ----------------------------------------------------------------------------
// SHPClass::SHPClass - Default constructor
// ----------------------------------------------------------------------------
SHPClass::SHPClass()
    : m_pSHP(nullptr)
    , m_pRawData(nullptr)
    , m_RawDataSize(0)
    , m_pFrameHeaders(nullptr)
    , m_pFrameCache(nullptr)
    , m_bOwnsData(false)
{
}

// ----------------------------------------------------------------------------
// SHPClass::SHPClass(const char*) - Constructor with filename
// ----------------------------------------------------------------------------
SHPClass::SHPClass(const char* pFilename)
    : m_pSHP(nullptr)
    , m_pRawData(nullptr)
    , m_RawDataSize(0)
    , m_pFrameHeaders(nullptr)
    , m_pFrameCache(nullptr)
    , m_bOwnsData(false)
{
    if (pFilename)
        LoadFromFile(pFilename);
}

// ----------------------------------------------------------------------------
// SHPClass::~SHPClass - Destructor
// ----------------------------------------------------------------------------
SHPClass::~SHPClass()
{
    ReleaseCache();

    if (m_bOwnsData && m_pSHP)
    {
        std::free(m_pSHP);
        m_pSHP = nullptr;
    }

    if (m_bOwnsData && m_pRawData)
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
    }

    if (m_pFrameHeaders && m_bOwnsData)
    {
        std::free(m_pFrameHeaders);
        m_pFrameHeaders = nullptr;
    }

    m_bOwnsData = false;
}

// ----------------------------------------------------------------------------
// SHPClass::LoadFromFile - Parse a SHP file
//
// SHP format:
//   uint16 FrameCount
//   uint16 FrameOffsets[FrameCount]  (offsets from start of file)
//   For each frame:
//     SHPFrame header (16 bytes):
//       int16 Left, Top, Width, Height
//       DWORD Flags (bit 1 = LCW compressed)
//       ColorStruct Color (3 bytes)
//       DWORD unknown_10
//       int32 Offset (absolute offset to pixel data)
//     Pixel data (Width * Height bytes, possibly LCW compressed)
// ----------------------------------------------------------------------------
bool SHPClass::LoadFromFile(const char* pFilename)
{
    ReleaseCache();

    if (m_bOwnsData && m_pSHP)
    {
        std::free(m_pSHP);
        m_pSHP = nullptr;
    }
    if (m_bOwnsData && m_pRawData)
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
    }
    if (m_pFrameHeaders && m_bOwnsData)
    {
        std::free(m_pFrameHeaders);
        m_pFrameHeaders = nullptr;
    }
    m_bOwnsData = false;

    // Open the file
    std::FILE* pFile = std::fopen(pFilename, "rb");
    if (!pFile)
        return false;

    // Get file size
    std::fseek(pFile, 0, SEEK_END);
    long fileSize = std::ftell(pFile);
    std::fseek(pFile, 0, SEEK_SET);

    if (fileSize < 2)
    {
        std::fclose(pFile);
        return false;
    }

    // Read the entire file into memory
    m_RawDataSize = static_cast<int32>(fileSize);
    m_pRawData = static_cast<BYTE*>(std::malloc(m_RawDataSize));
    if (!m_pRawData)
    {
        std::fclose(pFile);
        return false;
    }

    size_t bytesRead = std::fread(m_pRawData, 1, static_cast<size_t>(m_RawDataSize), pFile);
    std::fclose(pFile);

    if (bytesRead != static_cast<size_t>(m_RawDataSize))
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
        m_RawDataSize = 0;
        return false;
    }

    m_bOwnsData = true;

    // Parse the header
    // First 2 bytes: frame count
    int16 frameCount = *reinterpret_cast<int16*>(m_pRawData);
    if (frameCount <= 0 || frameCount > 32767)
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
        m_RawDataSize = 0;
        m_bOwnsData = false;
        return false;
    }

    // Calculate space needed for SHPFile + frame headers + pixel data
    // SHPFile struct has one SHPFrame embedded (FirstFrame), then the rest follow
    // We need: sizeof(SHPFile) + (frameCount - 1) * sizeof(SHPFrame) + pixel data area
    size_t headerSize = sizeof(SHPFile) + (static_cast<size_t>(frameCount) - 1) * sizeof(SHPFrame);
    m_pFrameHeaders = static_cast<SHPFrame*>(std::malloc(
        static_cast<size_t>(frameCount) * sizeof(SHPFrame)));
    if (!m_pFrameHeaders)
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
        m_RawDataSize = 0;
        m_bOwnsData = false;
        return false;
    }

    // Allocate SHPFile struct
    m_pSHP = static_cast<SHPStruct*>(std::malloc(headerSize));
    if (!m_pSHP)
    {
        std::free(m_pRawData);
        std::free(m_pFrameHeaders);
        m_pRawData = nullptr;
        m_pFrameHeaders = nullptr;
        m_RawDataSize = 0;
        m_bOwnsData = false;
        return false;
    }

    // Initialize the SHPFile
    SHPFile* pSHPFile = static_cast<SHPFile*>(m_pSHP);
    pSHPFile->Type = 0;
    pSHPFile->Frames = frameCount;

    // Read frame offsets from the offset table
    // Offset table starts at byte 2 in the file
    uint32* pOffsets = reinterpret_cast<uint32*>(m_pRawData + 2);

    // Parse each frame header
    int16 maxWidth = 0;
    int16 maxHeight = 0;

    for (int16 i = 0; i < frameCount; ++i)
    {
        uint32 frameOffset = pOffsets[i];
        if (frameOffset >= static_cast<uint32>(m_RawDataSize))
        {
            // Invalid offset
            std::free(m_pRawData);
            std::free(m_pFrameHeaders);
            std::free(m_pSHP);
            m_pRawData = nullptr;
            m_pFrameHeaders = nullptr;
            m_pSHP = nullptr;
            m_RawDataSize = 0;
            m_bOwnsData = false;
            return false;
        }

        // Read frame header from the file data
        BYTE* pFrameData = m_pRawData + frameOffset;
        SHPFrame& frame = m_pFrameHeaders[i];

        // Parse in the same layout as the original game
        // The SHPFrame is 16 bytes at the beginning of the frame data
        std::memcpy(&frame, pFrameData, sizeof(SHPFrame));

        // Track maximum dimensions
        if (frame.Left + frame.Width > maxWidth)
            maxWidth = frame.Left + frame.Width;
        if (frame.Top + frame.Height > maxHeight)
            maxHeight = frame.Top + frame.Height;
    }

    pSHPFile->Width = maxWidth;
    pSHPFile->Height = maxHeight;

    // Copy frame headers into the SHPFile struct
    SHPFrame* pFirstFrame = &pSHPFile->FirstFrame;
    std::memcpy(pFirstFrame, m_pFrameHeaders, static_cast<size_t>(frameCount) * sizeof(SHPFrame));

    // Allocate frame cache
    m_pFrameCache = static_cast<BYTE**>(std::malloc(
        static_cast<size_t>(frameCount) * sizeof(BYTE*)));
    if (m_pFrameCache)
    {
        std::memset(m_pFrameCache, 0, static_cast<size_t>(frameCount) * sizeof(BYTE*));
    }

    return true;
}

// ----------------------------------------------------------------------------
// SHPClass::GetFrame - Get cached frame data, decompressing if needed
// ----------------------------------------------------------------------------
BYTE* SHPClass::GetFrame(int32 frameIndex)
{
    if (!m_pSHP || !m_pFrameCache)
        return nullptr;

    if (frameIndex < 0 || frameIndex >= m_pSHP->Frames)
        return nullptr;

    // If already cached, return cached data
    if (m_pFrameCache[frameIndex])
        return m_pFrameCache[frameIndex];

    // Cache the frame
    CacheFrame(frameIndex);
    return m_pFrameCache[frameIndex];
}

// ----------------------------------------------------------------------------
// SHPClass::GetFrameCount - Return number of frames
// ----------------------------------------------------------------------------
int32 SHPClass::GetFrameCount() const
{
    if (!m_pSHP)
        return 0;
    return m_pSHP->Frames;
}

// ----------------------------------------------------------------------------
// SHPClass::GetFrameRect - Get the bounding rectangle for a frame
// ----------------------------------------------------------------------------
Rectangle SHPClass::GetFrameRect(int32 frameIndex) const
{
    Rectangle rect(0, 0, 0, 0);

    if (!m_pSHP || !m_pFrameHeaders)
        return rect;

    if (frameIndex < 0 || frameIndex >= m_pSHP->Frames)
        return rect;

    const SHPFrame& frame = m_pFrameHeaders[frameIndex];
    rect.X = frame.Left;
    rect.Y = frame.Top;
    rect.Width = frame.Width;
    rect.Height = frame.Height;

    return rect;
}

// ----------------------------------------------------------------------------
// SHPClass::HasCompression - Check if a frame is LCW compressed
// ----------------------------------------------------------------------------
bool SHPClass::HasCompression(int32 frameIndex) const
{
    if (!m_pSHP || !m_pFrameHeaders)
        return false;

    if (frameIndex < 0 || frameIndex >= m_pSHP->Frames)
        return false;

    const SHPFrame& frame = m_pFrameHeaders[frameIndex];
    return (frame.Flags & 0x0002) != 0;
}

// ----------------------------------------------------------------------------
// SHPClass::DecompressLCW - Westwood LCW (Lempel-Castle-Welch) decompression
//
// LCW format:
// - Command bytes: 0x80 = write byte following, else = copy command
// - Copy command: bits 0-5 = count, bits 6-7 = type
//   Type 0 (00): Short copy from 16-bit offset in stream
//                Command byte: XXYY YYYY  (X=type bits, Y=short count)
//                Followed by 2 bytes: relative offset
//                Copy (count+3) bytes from (current_pos - offset)
//   Type 1 (01): Medium copy from 8-bit offset
//                Command byte: XXYY YYYY
//                Followed by 1 byte: relative offset
//                Copy (count+3) bytes from (current_pos - offset)
//   Type 2 (10): Long copy from 16-bit offset
//                Command byte: XXYY YYYY
//                Followed by 2 bytes: absolute offset
//                Followed by 2 bytes: copy count
//                Copy count bytes from absolute offset
//   Type 3 (11): RLE fill
//                Command byte: XXYY YYYY
//                Fill (count+3) bytes with next byte
// ----------------------------------------------------------------------------
bool SHPClass::DecompressLCW(
    const BYTE* pSrc, int32 srcLen, BYTE* pDst, int32 dstLen)
{
    if (!pSrc || !pDst || srcLen <= 0 || dstLen <= 0)
        return false;

    const BYTE* pSrcEnd = pSrc + srcLen;
    BYTE* pDstEnd = pDst + dstLen;
    const BYTE* pSrcPos = pSrc;
    BYTE* pDstPos = pDst;

    while (pSrcPos < pSrcEnd && pDstPos < pDstEnd)
    {
        uint8 command = *pSrcPos++;

        // Check if it's a direct write (0x80)
        if (command == 0x80)
        {
            if (pSrcPos >= pSrcEnd)
                return false;
            *pDstPos++ = *pSrcPos++;
            continue;
        }

        // Extract count and type
        uint8 count = (command & 0x3F);
        uint8 type  = (command >> 6) & 0x03;

        switch (type)
        {
        case 0: // Short copy from 16-bit relative offset
        {
            if (pSrcPos + 1 >= pSrcEnd)
                return false;

            uint16 offset = *reinterpret_cast<const uint16*>(pSrcPos);
            pSrcPos += 2;

            int32 copyCount = static_cast<int32>(count) + 3;
            const BYTE* pCopySrc = pDstPos - offset;

            // Validate source pointer
            if (pCopySrc < pDst || pCopySrc >= pDstPos)
                return false;

            for (int32 i = 0; i < copyCount && pDstPos < pDstEnd; ++i)
            {
                *pDstPos++ = *pCopySrc++;
            }
            break;
        }

        case 1: // Medium copy from 8-bit relative offset
        {
            if (pSrcPos >= pSrcEnd)
                return false;

            uint8 offset = *pSrcPos++;

            int32 copyCount = static_cast<int32>(count) + 3;
            const BYTE* pCopySrc = pDstPos - offset;

            if (pCopySrc < pDst || pCopySrc >= pDstPos)
                return false;

            for (int32 i = 0; i < copyCount && pDstPos < pDstEnd; ++i)
            {
                *pDstPos++ = *pCopySrc++;
            }
            break;
        }

        case 2: // Long copy from absolute offset
        {
            if (pSrcPos + 3 >= pSrcEnd)
                return false;

            uint16 absOffset = *reinterpret_cast<const uint16*>(pSrcPos);
            pSrcPos += 2;
            uint16 copyCount = *reinterpret_cast<const uint16*>(pSrcPos);
            pSrcPos += 2;

            if (copyCount == 0)
                copyCount = static_cast<uint16>(count) + 3;

            const BYTE* pCopySrc = pDst + absOffset;
            if (pCopySrc < pDst || pCopySrc >= pDstEnd)
                return false;

            for (uint16 i = 0; i < copyCount && pDstPos < pDstEnd; ++i)
            {
                *pDstPos++ = *pCopySrc++;
            }
            break;
        }

        case 3: // RLE fill
        {
            if (pSrcPos >= pSrcEnd)
                return false;

            uint8 fillByte = *pSrcPos++;
            int32 fillCount = static_cast<int32>(count) + 3;

            for (int32 i = 0; i < fillCount && pDstPos < pDstEnd; ++i)
            {
                *pDstPos++ = fillByte;
            }
            break;
        }
        }
    }

    return (pDstPos <= pDstEnd);
}

// ----------------------------------------------------------------------------
// SHPClass::CacheFrame - Decompress and cache a single frame
// ----------------------------------------------------------------------------
void SHPClass::CacheFrame(int32 frameIndex)
{
    if (!m_pSHP || !m_pFrameHeaders || !m_pFrameCache)
        return;

    if (frameIndex < 0 || frameIndex >= m_pSHP->Frames)
        return;

    // Already cached
    if (m_pFrameCache[frameIndex])
        return;

    const SHPFrame& frame = m_pFrameHeaders[frameIndex];
    int32 frameWidth = frame.Width;
    int32 frameHeight = frame.Height;

    if (frameWidth <= 0 || frameHeight <= 0)
        return;

    int32 pixelCount = frameWidth * frameHeight;
    BYTE* pCache = static_cast<BYTE*>(std::malloc(static_cast<size_t>(pixelCount)));
    if (!pCache)
        return;

    // Get pixel data from raw file data
    BYTE* pSrcData = nullptr;
    int32 srcDataSize = 0;

    if (m_pRawData && frame.Offset < static_cast<uint32>(m_RawDataSize))
    {
        // The pixel data starts after the 16-byte frame header
        pSrcData = m_pRawData + frame.Offset + sizeof(SHPFrame);
        srcDataSize = m_RawDataSize - frame.Offset - static_cast<int32>(sizeof(SHPFrame));
        if (srcDataSize < 0)
            srcDataSize = 0;
    }

    if (!pSrcData || srcDataSize <= 0)
    {
        std::memset(pCache, 0, static_cast<size_t>(pixelCount));
        m_pFrameCache[frameIndex] = pCache;
        return;
    }

    // Check if compressed
    if ((frame.Flags & 0x0002) != 0)
    {
        // LCW compressed - decompress
        bool success = DecompressLCW(pSrcData, srcDataSize, pCache, pixelCount);
        if (!success)
        {
            // Decompression failed - fill with zeros
            std::memset(pCache, 0, static_cast<size_t>(pixelCount));
        }
    }
    else
    {
        // Uncompressed - copy directly
        int32 copySize = (srcDataSize < pixelCount) ? srcDataSize : pixelCount;
        std::memcpy(pCache, pSrcData, static_cast<size_t>(copySize));
        if (copySize < pixelCount)
        {
            std::memset(pCache + copySize, 0, static_cast<size_t>(pixelCount - copySize));
        }
    }

    m_pFrameCache[frameIndex] = pCache;
}

// ----------------------------------------------------------------------------
// SHPClass::ReleaseCache - Free all cached frame data
// ----------------------------------------------------------------------------
void SHPClass::ReleaseCache()
{
    if (!m_pFrameCache)
        return;

    if (m_pSHP)
    {
        for (int32 i = 0; i < m_pSHP->Frames; ++i)
        {
            if (m_pFrameCache[i])
            {
                std::free(m_pFrameCache[i]);
                m_pFrameCache[i] = nullptr;
            }
        }
    }

    std::free(m_pFrameCache);
    m_pFrameCache = nullptr;
}
// ============================================================================
// Type-casting helpers: the original binary resolves SHP file/reference by
// casting the SHPStruct base pointer. SHPReference derives from SHPStruct and
// stores the resolved file data in Data; SHPFile is the concrete file layout.
// ============================================================================

SHPReference* SHPStruct::AsReference()
{
    return IsReference() ? reinterpret_cast<SHPReference*>(this) : nullptr;
}

const SHPReference* SHPStruct::AsReference() const
{
    return IsReference() ? reinterpret_cast<const SHPReference*>(this) : nullptr;
}

SHPFile* SHPStruct::AsFile()
{
    return IsReference() ? nullptr : reinterpret_cast<SHPFile*>(this);
}

const SHPFile* SHPStruct::AsFile() const
{
    return IsReference() ? nullptr : reinterpret_cast<const SHPFile*>(this);
}
