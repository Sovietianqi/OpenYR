#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Core/Memory.h"
#include "Math/Matrix3D.h"
#include "IO/CCFileClass.h"

#include <cstring>

// ============================================================================
// HVA file format (Hierarchical Voxel Animation)
//
// HVA files store animation data for VXL voxel models. Each section
// (body, turret, barrel) has a transform matrix per animation frame.
// This allows for turret rotation, barrel elevation, and other
// sub-object animations.
// ============================================================================

// ============================================================================
// HVAMatrix - Transform matrix for a section at one frame
// ============================================================================
#pragma pack(push, 1)
struct HVAMatrix
{
    float M11, M12, M13, M14;
    float M21, M22, M23, M24;
    float M31, M32, M33, M34;
};
#pragma pack(pop)

// ============================================================================
// HVASectionTransform - All frame transforms for one section
// ============================================================================
struct HVASectionTransform
{
    char Name[16];          // Section name (matches VXL section name)
    HVAMatrix* Matrices;    // Array of matrices, one per frame
};

// ============================================================================
// HVAHeader - HVA file header
// ============================================================================
#pragma pack(push, 1)
struct HVAHeader
{
    char Signature[4];      // "HVA!"
    uint32 FrameCount;      // Number of animation frames
    uint32 SectionCount;    // Number of sections
};
#pragma pack(pop)

// ============================================================================
// HVAClass - HVA animation reader
// ============================================================================
class HVAClass
{
public:
    static constexpr int32 MaxSections = 32;
    static constexpr int32 MaxFrames = 256;

    HVAClass();
    HVAClass(const char* pFilename);
    ~HVAClass();

    // Load from file
    bool LoadFromFile(const char* pFilename);

    // Get transform data
    const HVAMatrix* GetTransform(int32 sectionIndex, int32 frameIndex) const;
    const HVAMatrix* GetTransform(const char* sectionName, int32 frameIndex) const;

    // Get section information
    int32 GetFrameCount() const { return m_FrameCount; }
    int32 GetSectionCount() const { return m_SectionCount; }
    const char* GetSectionName(int32 sectionIndex) const;

    // Find section by name
    int32 FindSection(const char* sectionName) const;

    // Get all transforms for a section
    const HVAMatrix* GetSectionTransforms(int32 sectionIndex) const;

    // Apply transform to a coordinate
    void ApplyTransform(int32 sectionIndex, int32 frameIndex,
                        float& x, float& y, float& z) const;

private:
    BYTE* m_pRawData;
    int32 m_RawDataSize;
    bool m_bOwnsData;

    HVAHeader* m_pHeader;
    int32 m_FrameCount;
    int32 m_SectionCount;

    HVASectionTransform m_Sections[MaxSections];
    HVAMatrix* m_pMatrixData;
};