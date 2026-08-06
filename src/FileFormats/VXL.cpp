#include "FileFormats/VXL.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// VXLClass implementation
// ============================================================================

// ----------------------------------------------------------------------------
// VXLClass::VXLClass - Default constructor
// ----------------------------------------------------------------------------
VXLClass::VXLClass()
    : m_pRawData(nullptr)
    , m_RawDataSize(0)
    , m_bOwnsData(false)
    , m_pHeader(nullptr)
    , m_pSections(nullptr)
    , m_SectionCount(0)
    , m_pHVAData(nullptr)
    , m_HVASize(0)
{
}

// ----------------------------------------------------------------------------
// VXLClass::VXLClass(const char*) - Constructor with filename
// ----------------------------------------------------------------------------
VXLClass::VXLClass(const char* pFilename)
    : m_pRawData(nullptr)
    , m_RawDataSize(0)
    , m_bOwnsData(false)
    , m_pHeader(nullptr)
    , m_pSections(nullptr)
    , m_SectionCount(0)
    , m_pHVAData(nullptr)
    , m_HVASize(0)
{
    if (pFilename)
        LoadFromFile(pFilename);
}

// ----------------------------------------------------------------------------
// VXLClass::~VXLClass - Destructor
// ----------------------------------------------------------------------------
VXLClass::~VXLClass()
{
    if (m_bOwnsData)
    {
        if (m_pSections)
        {
            std::free(m_pSections);
            m_pSections = nullptr;
        }
        if (m_pRawData)
        {
            std::free(m_pRawData);
            m_pRawData = nullptr;
        }
    }
    m_pHeader = nullptr;
    m_pHVAData = nullptr;
    m_SectionCount = 0;
    m_HVASize = 0;
    m_bOwnsData = false;
}

// ----------------------------------------------------------------------------
// VXLClass::LoadFromFile - Parse a VXL voxel model file
//
// VXL format:
//   VXLHeader (20 bytes):
//     char Signature[4]   - "Voxel Section"
//     uint32 SectionCount - number of sections
//     uint32 BodySize     - size of body section data
//     uint32 HVAStart     - offset to embedded HVA (0 if none)
//     uint32 HVASize      - size of embedded HVA data
//
//   For each section:
//     VXLSectionHeader (28 bytes):
//       char Name[16]         - section name
//       uint32 SpanStart      - start offset in span array
//       uint32 SpanEnd        - end offset in span array
//       uint32 SpanDataOffset - offset to span data in file
//       uint32 SectionSize    - total size of section data
//
//   For each section, span data consists of:
//     For each z-layer (0 to dimZ-1):
//       For each y-row (0 to dimY-1):
//         Span header: uint32 byteCount, uint32 spanCount
//         For each span: uint32 start, uint32 end, uint8 color
// ----------------------------------------------------------------------------
bool VXLClass::LoadFromFile(const char* pFilename)
{
    // Clean up existing data
    if (m_bOwnsData)
    {
        if (m_pSections)
        {
            std::free(m_pSections);
            m_pSections = nullptr;
        }
        if (m_pRawData)
        {
            std::free(m_pRawData);
            m_pRawData = nullptr;
        }
    }
    m_pHeader = nullptr;
    m_pHVAData = nullptr;
    m_SectionCount = 0;
    m_HVASize = 0;
    m_bOwnsData = false;

    // Open the file
    std::FILE* pFile = std::fopen(pFilename, "rb");
    if (!pFile)
        return false;

    // Get file size
    std::fseek(pFile, 0, SEEK_END);
    long fileSize = std::ftell(pFile);
    std::fseek(pFile, 0, SEEK_SET);

    if (fileSize < static_cast<long>(sizeof(VXLHeader)))
    {
        std::fclose(pFile);
        return false;
    }

    // Read the entire file
    m_RawDataSize = static_cast<int32>(fileSize);
    m_pRawData = static_cast<BYTE*>(std::malloc(static_cast<size_t>(m_RawDataSize)));
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

    // Parse the VXL header
    m_pHeader = reinterpret_cast<VXLHeader*>(m_pRawData);

    // Validate signature
    if (std::memcmp(m_pHeader->Signature, "Voxel Section", 4) != 0)
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
        m_RawDataSize = 0;
        m_bOwnsData = false;
        m_pHeader = nullptr;
        return false;
    }

    m_SectionCount = static_cast<int32>(m_pHeader->SectionCount);

    if (m_SectionCount <= 0 || m_SectionCount > MaxSections)
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
        m_RawDataSize = 0;
        m_bOwnsData = false;
        m_pHeader = nullptr;
        m_SectionCount = 0;
        return false;
    }

    // Parse section headers (they follow the VXLHeader)
    m_pSections = reinterpret_cast<VXLSectionHeader*>(
        m_pRawData + sizeof(VXLHeader));

    // Validate that section headers are within the file
    size_t sectionsEnd = sizeof(VXLHeader) +
        static_cast<size_t>(m_SectionCount) * sizeof(VXLSectionHeader);
    if (sectionsEnd > static_cast<size_t>(m_RawDataSize))
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
        m_RawDataSize = 0;
        m_bOwnsData = false;
        m_pHeader = nullptr;
        m_pSections = nullptr;
        m_SectionCount = 0;
        return false;
    }

    // Extract embedded HVA data if present
    if (m_pHeader->HVAStart != 0 && m_pHeader->HVASize > 0)
    {
        uint32 hvaStart = m_pHeader->HVAStart;
        uint32 hvaSize = m_pHeader->HVASize;

        if (hvaStart + hvaSize <= static_cast<uint32>(m_RawDataSize))
        {
            m_HVASize = static_cast<int32>(hvaSize);
            m_pHVAData = m_pRawData + hvaStart;
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// VXLClass::GetSectionName - Get the name of a section
// ----------------------------------------------------------------------------
const char* VXLClass::GetSectionName(int32 sectionIndex) const
{
    if (!m_pSections || sectionIndex < 0 || sectionIndex >= m_SectionCount)
        return nullptr;

    return m_pSections[sectionIndex].Name;
}

// ----------------------------------------------------------------------------
// VXLClass::GetSpanCount - Get the number of spans for a section
// ----------------------------------------------------------------------------
int32 VXLClass::GetSpanCount(int32 sectionIndex) const
{
    if (!m_pSections || sectionIndex < 0 || sectionIndex >= m_SectionCount)
        return 0;

    const VXLSectionHeader& section = m_pSections[sectionIndex];
    return static_cast<int32>(section.SpanEnd - section.SpanStart);
}

// ----------------------------------------------------------------------------
// VXLClass::GetSpanData - Get raw span data pointer for a section
// ----------------------------------------------------------------------------
const BYTE* VXLClass::GetSpanData(int32 sectionIndex) const
{
    if (!m_pRawData || !m_pSections || sectionIndex < 0 || sectionIndex >= m_SectionCount)
        return nullptr;

    const VXLSectionHeader& section = m_pSections[sectionIndex];
    uint32 offset = section.SpanDataOffset;

    if (offset >= static_cast<uint32>(m_RawDataSize))
        return nullptr;

    return m_pRawData + offset;
}

// ----------------------------------------------------------------------------
// VXLClass::GetSpanStartOffset - Get the start index in the span array
// ----------------------------------------------------------------------------
int32 VXLClass::GetSpanStartOffset(int32 sectionIndex) const
{
    if (!m_pSections || sectionIndex < 0 || sectionIndex >= m_SectionCount)
        return 0;

    return static_cast<int32>(m_pSections[sectionIndex].SpanStart);
}

// ----------------------------------------------------------------------------
// VXLClass::GetSpanEndOffset - Get the end index in the span array
// ----------------------------------------------------------------------------
int32 VXLClass::GetSpanEndOffset(int32 sectionIndex) const
{
    if (!m_pSections || sectionIndex < 0 || sectionIndex >= m_SectionCount)
        return 0;

    return static_cast<int32>(m_pSections[sectionIndex].SpanEnd);
}

// ----------------------------------------------------------------------------
// VXLClass::GetVoxel - Get voxel color at a specific 3D position
//
// VXL uses span-based run-length encoding:
// For each (z, y) pair, there is a list of spans.
// Each span defines a contiguous range of x positions with a single color.
// To find the voxel at (x, y, z):
//   1. Navigate to the span data for layer z, row y
//   2. Search through spans for one that contains x
//   3. Return the span's color, or 0 if not found
// ----------------------------------------------------------------------------
BYTE VXLClass::GetVoxel(int32 sectionIndex, int32 x, int32 y, int32 z) const
{
    if (!m_pRawData || !m_pSections || sectionIndex < 0 || sectionIndex >= m_SectionCount)
        return 0;

    if (x < 0 || x >= VoxelSize || y < 0 || y >= VoxelSize || z < 0 || z >= VoxelSizeZ)
        return 0;

    const VXLSectionHeader& section = m_pSections[sectionIndex];
    const BYTE* pSpanData = m_pRawData + section.SpanDataOffset;

    if (section.SpanDataOffset >= static_cast<uint32>(m_RawDataSize))
        return 0;

    // The span data is organized as:
    // For each z layer:
    //   For each y row:
    //     uint32 byteCount (total bytes in this row including header)
    //     uint32 spanCount (number of spans in this row)
    //     (spanCount) spans, each: uint32 start, uint32 end, uint8 color
    //     Padding to align

    const BYTE* pData = pSpanData;
    const BYTE* pDataEnd = m_pRawData + m_RawDataSize;

    // Navigate to the correct z-layer and y-row
    int32 currentZ = 0;
    int32 currentY = 0;

    while (pData < pDataEnd && currentZ <= z)
    {
        // Read the row header
        if (pData + 8 > pDataEnd)
            return 0;

        uint32 byteCount = *reinterpret_cast<const uint32*>(pData);
        pData += 4;
        uint32 spanCount = *reinterpret_cast<const uint32*>(pData);
        pData += 4;

        if (byteCount == 0 || spanCount == 0)
        {
            // Empty row, move to next
            currentY++;
            if (currentY >= VoxelSize)
            {
                currentY = 0;
                currentZ++;
            }
            continue;
        }

        if (byteCount < 8)
            return 0;

        if (pData + byteCount - 8 > pDataEnd)
            return 0;

        if (currentZ == z && currentY == y)
        {
            // Found the correct row, search spans
            const BYTE* pSpanStart = pData;
            const BYTE* pSpanEnd = pData + byteCount - 8;

            for (uint32 s = 0; s < spanCount && pSpanStart + 8 <= pSpanEnd; ++s)
            {
                uint32 spanStart = *reinterpret_cast<const uint32*>(pSpanStart);
                pSpanStart += 4;
                uint32 spanEnd = *reinterpret_cast<const uint32*>(pSpanStart);
                pSpanStart += 4;
                uint8 color = *pSpanStart;
                pSpanStart += 1;

                if (static_cast<int32>(spanStart) <= x && static_cast<int32>(spanEnd) >= x)
                {
                    return color;
                }
            }
            return 0;
        }

        // Move to next row
        pData += byteCount - 8;
        currentY++;
        if (currentY >= VoxelSize)
        {
            currentY = 0;
            currentZ++;
        }
    }

    return 0;
}

// ----------------------------------------------------------------------------
// VXLClass::HasSection - Check if a section exists
// ----------------------------------------------------------------------------
bool VXLClass::HasSection(int32 sectionIndex) const
{
    return (sectionIndex >= 0 && sectionIndex < m_SectionCount);
}

// ----------------------------------------------------------------------------
// VXLClass::GetBounds - Compute the bounding box of a section
//
// Iterates through all spans to find min/max x, y, z coordinates.
// A span defines a run of filled voxels in x, at a given (y, z) position.
// ----------------------------------------------------------------------------
void VXLClass::GetBounds(int32 sectionIndex, int32& minX, int32& maxX,
                         int32& minY, int32& maxY, int32& minZ, int32& maxZ) const
{
    minX = VoxelSize;
    maxX = 0;
    minY = VoxelSize;
    maxY = 0;
    minZ = VoxelSizeZ;
    maxZ = 0;

    if (!m_pRawData || !m_pSections || sectionIndex < 0 || sectionIndex >= m_SectionCount)
        return;

    const VXLSectionHeader& section = m_pSections[sectionIndex];
    const BYTE* pSpanData = m_pRawData + section.SpanDataOffset;

    if (section.SpanDataOffset >= static_cast<uint32>(m_RawDataSize))
        return;

    const BYTE* pData = pSpanData;
    const BYTE* pDataEnd = m_pRawData + m_RawDataSize;

    int32 currentZ = 0;
    int32 currentY = 0;

    while (pData < pDataEnd)
    {
        if (pData + 8 > pDataEnd)
            break;

        uint32 byteCount = *reinterpret_cast<const uint32*>(pData);
        pData += 4;
        uint32 spanCount = *reinterpret_cast<const uint32*>(pData);
        pData += 4;

        if (byteCount == 0 || spanCount == 0)
        {
            currentY++;
            if (currentY >= VoxelSize)
            {
                currentY = 0;
                currentZ++;
            }
            if (currentZ >= VoxelSizeZ)
                break;
            continue;
        }

        if (byteCount < 8 || pData + byteCount - 8 > pDataEnd)
            break;

        const BYTE* pSpanStart = pData;
        const BYTE* pRowEnd = pData + byteCount - 8;

        for (uint32 s = 0; s < spanCount && pSpanStart + 8 <= pRowEnd; ++s)
        {
            uint32 spanStart = *reinterpret_cast<const uint32*>(pSpanStart);
            pSpanStart += 4;
            uint32 spanEnd = *reinterpret_cast<const uint32*>(pSpanStart);
            pSpanStart += 4;
            pSpanStart += 1; // Skip color byte

            int32 sx = static_cast<int32>(spanStart);
            int32 ex = static_cast<int32>(spanEnd);

            if (sx < minX) minX = sx;
            if (ex > maxX) maxX = ex;
            if (currentY < minY) minY = currentY;
            if (currentY > maxY) maxY = currentY;
            if (currentZ < minZ) minZ = currentZ;
            if (currentZ > maxZ) maxZ = currentZ;
        }

        pData += byteCount - 8;
        currentY++;
        if (currentY >= VoxelSize)
        {
            currentY = 0;
            currentZ++;
        }
        if (currentZ >= VoxelSizeZ)
            break;
    }

    // If no spans were found, reset bounds
    if (minX > maxX)
    {
        minX = 0; maxX = 0;
        minY = 0; maxY = 0;
        minZ = 0; maxZ = 0;
    }
}