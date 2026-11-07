#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Core/Memory.h"
#include "Rendering/Surface.h"
#include "IO/CCFileClass.h"
#include "FileFormats/LCW.h"

#include <cstring>
#include <vector>

// ============================================================================
// SHP file format structures (Westwood sprite format)
//
// SHP files contain sprite frame data used for animations, cursors,
// building art, and unit graphics. The format supports both uncompressed
// and RLE (LCW) compressed frames.
// ============================================================================

// Forward declarations
struct SHPFile;
struct SHPReference;

// Westwood Format80 (LCW) 解压实现在 FileFormats/LCW.cpp，
// 声明与 asm 对照表见 FileFormats/LCW.h。

// ============================================================================
// SHPStruct - Base SHP structure (can be a file or reference)
// ============================================================================
struct SHPStruct
{
    SHPStruct()
        : Type(0), Width(0), Height(0), Frames(0)
    {}

    virtual ~SHPStruct() {}

    // Load the file data if this is a reference
    void Load();

    // Unload the data if this is a reference
    void Unload();

    // Resolve to actual file data, loading if necessary
    SHPFile* GetData();

    // asm: SHPStruct_ValidFrameIndex (0x69E8F0)
    // Resolves the frame table, checks idxFrame against the header frame
    // count, then reads the frame flag byte (+8 within the 24-byte frame
    // record) and returns bit 1 -- the "valid/has-compression" flag.
    bool ValidFrameIndex(int32 idxFrame) const;

    // Get the bounding rectangle for a frame
    // asm: sub_69E7E0 (0x69E7E0) -- reads four *signed words* from the frame
    // record (+0..+6) and stores them as (X, Y, Width, Height). These are the
    // per-frame header values, never the file-header dimensions.
    Rectangle* GetFrameBounds(Rectangle& buffer, int32 idxFrame) const;
    Rectangle GetFrameBounds(int32 idxFrame) const;

    // Get the pixel dimensions of a frame from its own header.
    // asm: sub_69E7E0 reads word [frame+4] (Width) and word [frame+6] (Height).
    // Returns 0 on success, -1 for an invalid frame index.
    int32 GetFrameSize(int32 idxFrame, int32& outW, int32& outH) const;

    // Get the average color of a frame (for color key optimization)
    ColorStruct* GetColor(ColorStruct& buffer, int32 idxFrame) const;
    ColorStruct GetColor(int32 idxFrame) const;

    // Get raw pixel data for a frame (unchanged, original semantics: pointer
    // into the file image, still compressed when Flags bit 1 is set).
    BYTE* GetPixels(int32 idxFrame);

    // Get decoded 8-bit indexed pixels for a frame (Width x Height, row major,
    // stride == frame width). Uncompressed frames are returned as a direct
    // pointer into the file (no copy); compressed frames are decoded once and
    // cached per SHPStruct in FrameCache[idxFrame].
    //
    // asm: SHPStruct_GetPixels at 0x69E740 only returns fileBase+[frame+0x14];
    //      the actual per-row decoding lives in the draw path (see comments in
    //      SHP.cpp). The cache is a plain mutable member: the engine is
    //      single-threaded, so no lock is required, `mutable` is only there so
    //      this can be called through a const-ish SHPStruct pointer.
    uint8* Get_Decoded_Pixels(int32 idxFrame);

    // Check if a frame uses RLE compression
    bool HasCompression(int32 idxFrame) const;

    // Check if this is a reference or file struct
    bool IsReference() const { return Type == 0xFFFF; }
    SHPReference* AsReference();
    const SHPReference* AsReference() const;
    SHPFile* AsFile();
    const SHPFile* AsFile() const;

    uint16 Type;
    int16 Width;
    int16 Height;
    int16 Frames;

    // Decoded-frame cache, lazily filled by Get_Decoded_Pixels().
    // One std::vector<uint8> per frame (Width*Height bytes, row major).
    // `mutable` so the accessor can be used on const pointers as well; the
    // engine never draws the same SHPStruct from two threads, so no lock.
    mutable std::vector<std::vector<uint8>> FrameCache;
};

// ============================================================================
// SHPReference - Linked list reference to a SHP file
// ============================================================================
struct SHPReference : public SHPStruct
{
    SHPReference(const char* filename);

    char* Filename;
    SHPStruct* Data;
    bool Loaded;
    int32 Index;
    SHPReference* Next;
    SHPReference* Prev;
    DWORD unknown_20;
};

// ============================================================================
// SHPFrame - Individual frame header within a SHP file
// ============================================================================
struct SHPFrame
{
    int16 Left;
    int16 Top;
    int16 Width;
    int16 Height;
    DWORD Flags;
    ColorStruct Color;
    DWORD unknown_10;
    int32 Offset;
};

// ============================================================================
// SHPFile - Main SHP file data structure
// ============================================================================
struct SHPFile : public SHPStruct
{
    const SHPFrame& GetFrameHeader(int32 idxFrame) const
    {
        return (&FirstFrame)[idxFrame];
    }

    SHPFrame FirstFrame;
};

// ============================================================================
// SHPClass - High-level SHP file reader
// ============================================================================
class SHPClass
{
public:
    SHPClass();
    SHPClass(const char* pFilename);
    ~SHPClass();

    // Load from file (MIX archives are searched before the loose file).
    bool LoadFromFile(const char* pFilename);

    // Parse an already-resident SHP image. This is the path used when the
    // bytes come out of a MIX archive: the archive hands back the raw file
    // image, which still has to be turned into an SHPFile structure before
    // anything can draw it.
    bool LoadFromMemory(const void* pData, int32 size);

    // Get frame data
    BYTE* GetFrame(int32 frameIndex);
    int32 GetFrameCount() const;
    Rectangle GetFrameRect(int32 frameIndex) const;
    bool HasCompression(int32 frameIndex) const;

    // Get the underlying SHPStruct
    SHPStruct* GetSHPStruct() const { return m_pSHP; }

    // RLE (LCW) decompression
    static bool DecompressLCW(
        const BYTE* pSrc, int32 srcLen, BYTE* pDst, int32 dstLen);

    // Frame caching
    void CacheFrame(int32 frameIndex);
    void ReleaseCache();

private:
    // Parse m_pRawData (complete SHP file image) into m_pSHP / m_pFrameHeaders.
    bool Parse_Raw_Data();

    SHPStruct* m_pSHP;
    BYTE* m_pRawData;
    int32 m_RawDataSize;
    SHPFrame* m_pFrameHeaders;
    BYTE** m_pFrameCache;
    bool m_bOwnsData;
};

// ============================================================================
// Load_SHP - Resolve and parse a SHP asset by name.
//
// Search order: mounted MIX archives, then the loose file. Results are cached
// per name, so repeated lookups (every draw call resolves its shape once)
// return the same SHPStruct instead of re-parsing or leaking.
//
// Returns nullptr when the asset is absent. Callers must NOT free the result.
// ============================================================================
SHPStruct* Load_SHP(const char* pName);