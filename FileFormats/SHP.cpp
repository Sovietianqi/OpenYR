#include "FileFormats/SHP.h"
#include "IO/MixFileClass.h"
#include "IO/FileSystem.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <new>
#include <vector>
#include <unordered_map>
#include <string>

// ============================================================================
// Small helpers
// ============================================================================

// Unaligned little-endian reads; SHP/LCW streams are byte packed.
static inline uint16 ReadLE16(const uint8* p)
{
    return static_cast<uint16>(static_cast<uint16>(p[0]) |
                               (static_cast<uint16>(p[1]) << 8));
}

// LCW (Westwood Format80) 解压已抽到 FileFormats/LCW.cpp ——
// 它不依赖本文件的任何类型，独立出去才能单独做回归测试。

// ============================================================================
// SHP frame row decoding
//
// asm: BSurface_DrawSHPFrame (0x4379A0 / body around 0x437D51) walks the
//      compressed frame as rows:
//        437D59: mov cx,[edi] / add edi,ecx     ; skip a row
//        437D9D: push edi+2                     ; payload = row + 2
//        437E23: mov dx,[edi] / add edi,edx     ; edi += row length
//      => every row is  [uint16 n][n-2 payload bytes], i.e. the stored length
//      *includes* its own 2-byte prefix. This is per-row, never one stream for
//      the whole frame.
//
//      The payload is decoded by the RLE blitter RLEBlitTransXlat<uchar>::Blit
//      (sub_491820, selected for flag&2 frames by LightConvertClass_SelectBlitter1):
//        nonzero byte -> one palette index
//        zero byte    -> next byte = number of transparent pixels to skip
//      (that is zero-run RLE; it is *not* LCW. LCW is still accepted below as
//      a compatibility fallback for TS-dialect rows.)
// ============================================================================
static bool DecodeSHP_Row_RLE(const uint8* pSrc, int32 srcLen, uint8* pDst, int32 width)
{
    if (pSrc == nullptr || pDst == nullptr || srcLen <= 0 || width <= 0)
        return false;

    int32 s = 0;
    int32 d = 0;
    while (d < width)
    {
        if (s >= srcLen)
            return false;
        const uint8 token = pSrc[s++];

        if (token != 0)
        {
            pDst[d++] = token;
        }
        else
        {
            if (s >= srcLen)
                return false;
            const int32 skip = pSrc[s++];
            if (skip <= 0 || d + skip > width)
                return false;              // asm would spin forever on skip==0
            std::memset(pDst + d, 0, static_cast<size_t>(skip));
            d += skip;
        }
    }
    return true;
}


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
//
// asm: sub_69E7E0 (0x69E7E0) reads the four *signed 16-bit* fields at the
// start of the 24-byte frame record and stores them as rectangle
// (X, Y, Width, Height). It never falls back to the file-header size.
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
// SHPStruct::GetFrameSize - Per-frame pixel dimensions
//
// asm: sub_69E7E0, movsx esi, word ptr [eax+4] / movsx edi, word ptr [eax+6].
// These come from the frame record, so frames smaller than (or offset from)
// the file-header canvas keep their own size.
// ----------------------------------------------------------------------------
int32 SHPStruct::GetFrameSize(int32 idxFrame, int32& outW, int32& outH) const
{
    outW = 0;
    outH = 0;

    if (idxFrame < 0 || idxFrame >= Frames)
        return -1;

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
        return -1;

    const SHPFrame& frame = pFile->GetFrameHeader(idxFrame);
    outW = frame.Width;
    outH = frame.Height;
    return 0;
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

    // The frame record's Offset is rebased by Parse_Raw_Data() to point at the
    // pixel payload inside the contiguous SHPFile block (original files store
    // the frame-header offset there instead, which is why every reader has to
    // go through the rebased copy rather than the raw file image).
    BYTE* pBase = reinterpret_cast<BYTE*>(pFile);
    BYTE* pPixels = pBase + frame.Offset;

    return pPixels;
}

// ----------------------------------------------------------------------------
// SHPStruct::Get_Decoded_Pixels - Frame pixels ready for blitting
//
// Uncompressed frames (Flags & 2 == 0) are plain row-major indices and are
// returned as-is (asm 0x4AF1C9 path, no row table).
//
// Compressed frames use the row table described above at DecodeSHP_Row_RLE:
//   for each of the Height rows:  uint16 n ; n-2 payload bytes
// The decoded result is cached in FrameCache[idxFrame] and stays valid until
// the SHPStruct is destroyed.
// ----------------------------------------------------------------------------
uint8* SHPStruct::Get_Decoded_Pixels(int32 idxFrame)
{
    if (idxFrame < 0 || idxFrame >= Frames)
        return nullptr;

    SHPFile* pFile = GetData();
    if (!pFile)
        return nullptr;

    const SHPFrame& frame = pFile->GetFrameHeader(idxFrame);

    // asm 0x4AF11C: SHPStruct_ValidFrameIndex tests bit 1; clear => the frame
    // is stored row-major and can be handed to the blitter untouched.
    if ((frame.Flags & 0x0002) == 0)
        return GetPixels(idxFrame);

    const int32 w = frame.Width;
    const int32 h = frame.Height;
    if (w <= 0 || h <= 0)
        return nullptr;

    if (static_cast<int32>(FrameCache.size()) <= idxFrame)
        FrameCache.resize(static_cast<size_t>(idxFrame) + 1);

    std::vector<uint8>& cache = FrameCache[static_cast<size_t>(idxFrame)];
    const size_t need = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (cache.size() == need)
        return cache.data();               // already decoded (multi-frame reuse)

    cache.assign(need, 0);

    const uint8* pRow = GetPixels(idxFrame);
    if (pRow == nullptr)
        return nullptr;

    for (int32 y = 0; y < h; ++y)
    {
        const uint16 rowLen = ReadLE16(pRow);
        if (rowLen < 2)
            break;                         // corrupt row table: keep remainder clear

        const int32 payloadLen = static_cast<int32>(rowLen) - 2;
        uint8* pDstRow = cache.data() + static_cast<size_t>(y) * static_cast<size_t>(w);

        // RA2 的 SHP（flag & 2）是**逐行 LCW/Format80**：每行 uint16 长度 +
        //   压缩流。早期实现先试 DecodeSHP_Row_RLE，但那条状态机（非零字节
        //   = 一个像素、0 = 跳过）会把 LCW 的回溯/字面量指令误解成像素，
        //   常常“成功”返回一帧乱码，导致整张图错位。
        //   这里改成 LCW 优先，只有 LCW 明确失败（<0）才退回旧 RLE 方言。
        if (LCW_Uncompress(pRow + 2, payloadLen, pDstRow, w) < 0)
            DecodeSHP_Row_RLE(pRow + 2, payloadLen, pDstRow, w);

        pRow += rowLen;                    // length includes its own word
    }

    return cache.data();
}

// ----------------------------------------------------------------------------
// SHPStruct::HasCompression - Check if a frame is compressed
// Flags & 2 (asm SHPStruct_ValidFrameIndex 0x69E8F0: and eax,2 ; shr eax,1).
// gamemd then feeds such frames to the RLE blitter (0x491820) and to
// BSurface_DrawSHPFrame's per-row table.
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
        // m_pSHP is placement-new'd (SHPStruct now owns a std::vector cache)
        m_pSHP->~SHPStruct();
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
        m_pSHP->~SHPStruct();
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

    // ------------------------------------------------------------------
    // Asset resolution order: mounted MIX archives first, then a loose file.
    //
    // A retail install keeps every shape inside ra2md.mix / expandmd*.mix, so
    // a disk-only open() could never see them. The archive hands back the raw
    // file image; Parse_Raw_Data() turns it into an SHPFile.
    // ------------------------------------------------------------------
    {
        int32 mixSize = 0;
        void* pMixData = MixFileClass::RetrieveSized(pFilename, &mixSize);
        if (pMixData != nullptr && mixSize >= 2)
        {
            m_RawDataSize = mixSize;
            m_pRawData = static_cast<BYTE*>(std::malloc(static_cast<size_t>(mixSize)));
            if (m_pRawData != nullptr)
            {
                std::memcpy(m_pRawData, pMixData, static_cast<size_t>(mixSize));
                YRMemory::Deallocate(pMixData);
                m_bOwnsData = true;
                return Parse_Raw_Data();
            }
            YRMemory::Deallocate(pMixData);
        }
    }

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

    return Parse_Raw_Data();
}

// ----------------------------------------------------------------------------
// SHPClass::LoadFromMemory - Adopt a resident SHP image
//
// The buffer is copied: callers hand us archive memory they are about to
// release, and every later frame access reads from m_pRawData.
// ----------------------------------------------------------------------------
bool SHPClass::LoadFromMemory(const void* pData, int32 size)
{
    ReleaseCache();

    if (m_bOwnsData && m_pSHP)
    {
        m_pSHP->~SHPStruct();
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

    if (pData == nullptr || size < 2)
        return false;

    m_RawDataSize = size;
    m_pRawData = static_cast<BYTE*>(std::malloc(static_cast<size_t>(size)));
    if (m_pRawData == nullptr)
    {
        m_RawDataSize = 0;
        return false;
    }

    std::memcpy(m_pRawData, pData, static_cast<size_t>(size));
    m_bOwnsData = true;

    return Parse_Raw_Data();
}

// ----------------------------------------------------------------------------
// SHPClass::Parse_Raw_Data - Build SHPFile + frame table from m_pRawData
// ----------------------------------------------------------------------------
bool SHPClass::Parse_Raw_Data()
{
    // ------------------------------------------------------------------
    // 文件头（8 字节）—— 布局以 asm sub_69E580 为准：
    //     cmp word ptr [edi], 0FFFFh    ; +0 恒为 0xFFFF（"这是 SHP"）
    //     movsx edx, word ptr [eax+6]   ; +6 = 帧数
    //   +0 uint16 0xFFFF
    //   +2 uint16 画布宽 (Width)
    //   +4 uint16 画布高 (Height)
    //   +6 uint16 帧数
    //   +8 uint32[帧数] 帧偏移表，每项指向该帧的 24 字节帧头
    //
    // 早期实现把 +0 当帧数、偏移表放在 +2 —— 对零售 SHP 而言 +0 是 0xFFFF
    // （= -1），于是每一张图都在第一行判定里被拒，所有建筑/单位/界面素材
    // 静默加载失败。这里按上面的布局改正。
    // ------------------------------------------------------------------
    if (m_RawDataSize < 8)
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
        m_RawDataSize = 0;
        m_bOwnsData = false;
        return false;
    }

    uint16 fileWidth = 0, fileHeight = 0, frameCountRaw = 0;
    std::memcpy(&fileWidth,   m_pRawData + 2, 2);
    std::memcpy(&fileHeight,  m_pRawData + 4, 2);
    std::memcpy(&frameCountRaw, m_pRawData + 6, 2);

    const int32 frameCount = static_cast<int32>(frameCountRaw);
    if (frameCount <= 0 || frameCount > 32767)
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
        m_RawDataSize = 0;
        m_bOwnsData = false;
        return false;
    }

    if (m_RawDataSize < 8 + frameCount * 4)
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
        m_RawDataSize = 0;
        m_bOwnsData = false;
        return false;
    }

    // Calculate space needed for SHPFile + frame headers
    // SHPFile struct has one SHPFrame embedded (FirstFrame), then the rest follow
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

    // 偏移表从 +8 开始，每项 uint32；用 memcpy 逐项读，避免未对齐访问。
    std::vector<uint32> offsets(static_cast<size_t>(frameCount), 0);
    for (int32 i = 0; i < frameCount; ++i)
    {
        std::memcpy(&offsets[static_cast<size_t>(i)],
                    m_pRawData + 8 + static_cast<size_t>(i) * 4, 4);
    }

    // ------------------------------------------------------------------
    // Pass 1: copy every frame record out of the file image.
    //
    // The records carry a *file* offset (SHPFrame::Offset). It cannot be used
    // as-is later: GetPixels() adds it to the SHPFile base pointer, and the
    // engine keeps that structure in its own allocation, not on top of the raw
    // file. Retail gamemd builds one contiguous block (header + records +
    // pixels) and rebases every Offset into it; we do the same below.
    // ------------------------------------------------------------------
    int16 maxWidth = 0;
    int16 maxHeight = 0;
    uint32 pixelsStart = 0;          // 取 max(帧头末地址)，从 0 起累加

    for (int32 i = 0; i < frameCount; ++i)
    {
        const uint32 frameOffset = offsets[static_cast<size_t>(i)];
        // 判界要分两步：先确认 offset 落在文件内，再确认剩余空间够 24 字节。
        // 直接写 `size - offset < 24` 在 offset > size 时会下溢，反过来又
        // 写 `offset + 24 > size` 则会在 offset 接近 UINT32_MAX 时上溢。
        if (frameOffset > static_cast<uint32>(m_RawDataSize) ||
            static_cast<uint32>(m_RawDataSize) - frameOffset < sizeof(SHPFrame))
        {
            // Invalid offset
            std::free(m_pRawData);
            std::free(m_pFrameHeaders);
            m_pRawData = nullptr;
            m_pFrameHeaders = nullptr;
            m_RawDataSize = 0;
            m_bOwnsData = false;
            return false;
        }

        // Read frame header from the file data
        BYTE* pFrameData = m_pRawData + frameOffset;
        SHPFrame& frame = m_pFrameHeaders[i];

        // Parse in the same layout as the original game
        // The SHPFrame is 24 bytes at the beginning of the frame data
        std::memcpy(&frame, pFrameData, sizeof(SHPFrame));

        // Track maximum dimensions
        if (frame.Left + frame.Width > maxWidth)
            maxWidth = frame.Left + frame.Width;
        if (frame.Top + frame.Height > maxHeight)
            maxHeight = frame.Top + frame.Height;

        // 像素区在所有 24 字节帧头**之后**。偏移表指向的是各帧的帧头，
        // 所以取 max(offset + 24) —— 取 min 会落到第二帧的帧头上，
        // 让第 0 帧的像素指针直接指进帧表。
        const uint32 afterHeader = frameOffset + static_cast<uint32>(sizeof(SHPFrame));
        if (afterHeader > pixelsStart)
            pixelsStart = afterHeader;
    }

    if (pixelsStart >= static_cast<uint32>(m_RawDataSize))
        pixelsStart = static_cast<uint32>(m_RawDataSize);

    // ------------------------------------------------------------------
    // 逐帧算出像素在**文件**里的绝对偏移。
    //
    // 文件里帧的像素首尾相接、顺序与帧表一致，但没有显式长度字段：
    //   未压缩帧（Flags & 2 == 0）长度 = w * h
    //   压缩帧（Flags & 2 != 0）  长度 = 逐行累加的 uint16 行长度
    // 所以必须顺序推进，不能各帧独立定位。
    // ------------------------------------------------------------------
    std::vector<uint32> pixelOffsets(static_cast<size_t>(frameCount), 0);
    {
        uint32 cursor = pixelsStart;
        const uint32 fileEnd = static_cast<uint32>(m_RawDataSize);
        for (int32 i = 0; i < frameCount; ++i)
        {
            pixelOffsets[static_cast<size_t>(i)] = cursor;

            const SHPFrame& f = m_pFrameHeaders[i];
            const int32 w = f.Width;
            const int32 h = f.Height;
            uint32 bytes = 0;

            if (w > 0 && h > 0)
            {
                if ((f.Flags & 0x0002u) == 0)
                {
                    bytes = static_cast<uint32>(w) * static_cast<uint32>(h);
                }
                else
                {
                    for (int32 row = 0; row < h; ++row)
                    {
                        if (cursor + bytes + 2 > fileEnd)
                            break;
                        uint16 rowLen = 0;
                        std::memcpy(&rowLen, m_pRawData + cursor + bytes, 2);
                        if (rowLen < 2)
                            break;
                        bytes += rowLen;
                    }
                }
            }

            if (cursor + bytes > fileEnd)
                bytes = fileEnd - cursor;
            cursor += bytes;
        }
    }

    // ------------------------------------------------------------------
    // Pass 2: one allocation holding header + records + pixel payloads.
    //
    // The pixel region is copied verbatim (compressed row tables included);
    // only the record offsets are rebased, so both the plain and the LCW /
    // row-RLE paths keep reading exactly the bytes the file shipped.
    // ------------------------------------------------------------------
    const size_t pixelBytes = static_cast<size_t>(m_RawDataSize) - pixelsStart;
    const size_t blockSize  = headerSize + pixelBytes;

    void* pBlock = std::malloc(blockSize);
    m_pSHP = (pBlock != nullptr) ? new (pBlock) SHPFile() : nullptr;
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

    SHPFile* pSHPFile = static_cast<SHPFile*>(m_pSHP);
    pSHPFile->Type   = 0;
    pSHPFile->Frames = frameCount;
    // 画布尺寸取文件头的 +2/+4（原版 SHPFile 就是这两个字段）；
    // 文件头缺失（0）时才退回按各帧包围盒推出来的值。
    pSHPFile->Width  = (fileWidth  > 0) ? fileWidth  : maxWidth;
    pSHPFile->Height = (fileHeight > 0) ? fileHeight : maxHeight;

    BYTE* pBlockBytes = reinterpret_cast<BYTE*>(pBlock);
    std::memcpy(pBlockBytes + headerSize, m_pRawData + pixelsStart, pixelBytes);

    SHPFrame* pFirstFrame = &pSHPFile->FirstFrame;
    for (int32 i = 0; i < frameCount; ++i)
    {
        SHPFrame rec = m_pFrameHeaders[i];
        // 用 Pass 1 顺序算出的绝对像素偏移重定位到新块内。
        const uint32 srcOff = pixelOffsets[static_cast<size_t>(i)];
        rec.Offset = (srcOff >= pixelsStart)
                   ? static_cast<int32>(headerSize) + static_cast<int32>(srcOff - pixelsStart)
                   : 0;
        pFirstFrame[i] = rec;
    }

    // The payload now lives inside the SHPFile block; release the file image
    // so one shape costs one allocation instead of two.
    std::free(m_pRawData);
    m_pRawData = nullptr;
    m_RawDataSize = 0;

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

    // Get pixel data from the parsed SHPStruct.
    //
    // This used to read `m_pRawData + frame.Offset + sizeof(SHPFrame)`, which
    // is wrong twice over: Parse_Raw_Data() frees the raw file image once the
    // contiguous SHPFile block is built, and the frame record's Offset is
    // rebased onto that block (it points at the pixels, not at the header).
    // The result was that every frame came back as all-zero pixels.
    //
    // Get_Decoded_Pixels() already implements both cases correctly: plain
    // frames return a pointer into the SHPFile block, compressed frames are
    // decoded once into the struct's own cache.
    const BYTE* pSrcData = m_pSHP->Get_Decoded_Pixels(frameIndex);

    if (pSrcData == nullptr)
    {
        std::memset(pCache, 0, static_cast<size_t>(pixelCount));
        m_pFrameCache[frameIndex] = pCache;
        return;
    }

    std::memcpy(pCache, pSrcData, static_cast<size_t>(pixelCount));

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

// ============================================================================
// SHPStruct_ValidFrameIndex  (asm: SHPStruct_ValidFrameIndex, 0x69E8F0)
//
//   call sub_69E580          ; resolve SHPFile data pointer (load if needed)
//   test eax, eax ; jz fail
//   movsx edx, word [eax+6]  ; Frames from the file header
//   cmp ecx, edx ; jnb fail  ; idxFrame >= Frames -> false
//   ecx = idxFrame*3*8 + data + 8    ; frame record base (24-byte stride)
//   al = [ecx+8]             ; frame Flags byte
//   and eax, 2 ; shr eax, 1  ; return bit 1
// ============================================================================
bool SHPStruct::ValidFrameIndex(int32 idxFrame) const
{
    if (idxFrame < 0)
        return false;                      // asm: unsigned compare jnb

    SHPStruct* pSelf = const_cast<SHPStruct*>(this);
    SHPFile* pData = pSelf->GetData();     // asm: call sub_69E580
    if (pData == nullptr)
        return false;                      // asm: test eax,eax ; jz fail

    if (idxFrame >= static_cast<int32>(pData->Frames))
        return false;                      // asm: cmp ecx,edx ; jnb fail

    // Frame records sit right behind the header with a 24-byte stride and the
    // flag byte is at +8 of each record. Read it through the structure: the
    // asm does raw `[record + 8]` arithmetic because the original SHPStruct is
    // packed at the start of the file image, which is not true on a 64-bit
    // build (vtable pointer + std::vector in SHPStruct).
    return (pData->GetFrameHeader(idxFrame).Flags & 0x0002) != 0;   // asm: and eax,2
}

// ============================================================================
// Load_SHP - Resolve + parse a shape by name, cached per name.
//
// Before this existed, call sites did
//     static_cast<SHPStruct*>(MixFileClass::Retrieve("X.SHP"))
// which reinterpret-casts the *raw file image* as an SHPStruct. The first
// two bytes of an SHP file are the 0xFFFF signature (then canvas width,
// canvas height and the frame count), not Type/Width/Height/Frames, so every
// shape handed to CC_Draw_Shape carried a garbage header. That cast is the
// reason real assets could not draw.
//
// 走这里之前 SHPClass::Parse_Raw_Data 还必须能正确认头 —— 它原先也是按
// 「+0 = 帧数、+2 = 偏移表」解析的，对 0xFFFF 开头的零售文件直接判失败，
// 两个缺陷叠在一起，等于所有 SHP 素材都加载不到。两处已一并修正。
// ============================================================================
SHPStruct* Load_SHP(const char* pName)
{
    if (pName == nullptr || pName[0] == '\0')
        return nullptr;

    static std::unordered_map<std::string, SHPClass*> s_Cache;

    const std::string key(pName);

    auto it = s_Cache.find(key);
    if (it != s_Cache.end())
        return (it->second != nullptr) ? it->second->GetSHPStruct() : nullptr;

    SHPClass* pLoader = new SHPClass();
    if (!pLoader->LoadFromFile(pName))
    {
        delete pLoader;
        // Negative cache: a missing asset must not re-open the archive on
        // every single frame.
        s_Cache.emplace(key, nullptr);
        return nullptr;
    }

    SHPStruct* pShape = pLoader->GetSHPStruct();
    if (pShape == nullptr)
    {
        delete pLoader;
        s_Cache.emplace(key, nullptr);
        return nullptr;
    }

    s_Cache.emplace(key, pLoader);
    return pShape;
}
