#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Core/Memory.h"
#include "Rendering/Surface.h"
#include "IO/CCFileClass.h"

#include <cstring>

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

    // Get the bounding rectangle for a frame
    Rectangle* GetFrameBounds(Rectangle& buffer, int32 idxFrame) const;
    Rectangle GetFrameBounds(int32 idxFrame) const;

    // Get the average color of a frame (for color key optimization)
    ColorStruct* GetColor(ColorStruct& buffer, int32 idxFrame) const;
    ColorStruct GetColor(int32 idxFrame) const;

    // Get raw pixel data for a frame
    BYTE* GetPixels(int32 idxFrame);

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

    // Load from file
    bool LoadFromFile(const char* pFilename);

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
    SHPStruct* m_pSHP;
    BYTE* m_pRawData;
    int32 m_RawDataSize;
    SHPFrame* m_pFrameHeaders;
    BYTE** m_pFrameCache;
    bool m_bOwnsData;
};