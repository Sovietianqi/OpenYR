#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Core/Memory.h"
#include "Math/CoordStruct.h"
#include "IO/CCFileClass.h"

#include <cstring>

// ============================================================================
// VXL file format (Westwood Voxel Model Format)
//
// VXL files store 3D voxel models used for unit rendering in C&C.
// Each model consists of one or more VXL sections (body, turret, barrel),
// with span-based run-length encoding for efficient storage.
// ============================================================================

// ============================================================================
// VXLHeader - File-level header
// ============================================================================
#pragma pack(push, 1)
struct VXLHeader
{
    char Signature[4];      // "Voxel Section"
    uint32 SectionCount;    // Number of sections in the file
    uint32 BodySize;        // Size of the body section
    uint32 HVAStart;        // Offset to HVA section (0 if none)
    uint32 HVASize;         // Size of HVA section
};
#pragma pack(pop)

// ============================================================================
// VXLSectionHeader - Per-section header
// ============================================================================
#pragma pack(push, 1)
struct VXLSectionHeader
{
    char Name[16];          // Section name (e.g., "TURRET", "BARREL")
    uint32 SpanStart;       // Span start offset
    uint32 SpanEnd;         // Span end offset
    uint32 SpanDataOffset;  // Offset to span data
    uint32 SectionSize;     // Total section size
};
#pragma pack(pop)

// ============================================================================
// VXLSpan - Run-length encoded voxel span
// ============================================================================
#pragma pack(push, 1)
struct VXLSpan
{
    uint32 SpanCount;       // Number of spans in this data block
    uint32 SpanStartOffset; // Starting offset of spans
    uint32 SpanEndOffset;   // Ending offset of spans
};
#pragma pack(pop)

// ============================================================================
// VXLClass - Voxel model reader
// ============================================================================
class VXLClass
{
public:
    static constexpr int32 MaxSections = 32;
    static constexpr int32 VoxelSize = 256;     // 256x256 voxel grid
    static constexpr int32 VoxelSizeZ = 256;    // 256 layers deep

    VXLClass();
    VXLClass(const char* pFilename);
    ~VXLClass();

    // Load from file
    bool LoadFromFile(const char* pFilename);

    // Get section information
    int32 GetSectionCount() const { return m_SectionCount; }
    const char* GetSectionName(int32 sectionIndex) const;
    int32 GetSpanCount(int32 sectionIndex) const;

    // Get span data for rendering
    const BYTE* GetSpanData(int32 sectionIndex) const;
    int32 GetSpanStartOffset(int32 sectionIndex) const;
    int32 GetSpanEndOffset(int32 sectionIndex) const;

    // Get voxel at a specific position
    BYTE GetVoxel(int32 sectionIndex, int32 x, int32 y, int32 z) const;

    // Get the HVA data (if embedded)
    const BYTE* GetHVA() const { return m_pHVAData; }
    int32 GetHVASize() const { return m_HVASize; }

    // Check if a section exists
    bool HasSection(int32 sectionIndex) const;

    // Get bounding box
    void GetBounds(int32 sectionIndex, int32& minX, int32& maxX,
                   int32& minY, int32& maxY, int32& minZ, int32& maxZ) const;

private:
    BYTE* m_pRawData;
    int32 m_RawDataSize;
    bool m_bOwnsData;

    VXLHeader* m_pHeader;
    VXLSectionHeader* m_pSections;
    int32 m_SectionCount;

    BYTE* m_pHVAData;
    int32 m_HVASize;
};