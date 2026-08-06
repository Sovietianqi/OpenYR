// =============================================================================
// HVA.cpp - Hierarchical Voxel Animation file format reader
//
// HVA files store per-frame transform matrices for the sub-objects (sections)
// of a VXL voxel model.  Each section (body, turret, barrel, etc.) has one
// 3x4 affine matrix per animation frame, allowing turret rotation, barrel
// recoil, walking legs, and other sub-object animations.
//
// File layout:
//   HVAHeader  (12 bytes)
//     char   Signature[4]     "HVA "
//     uint32 FrameCount       number of animation frames
//     uint32 SectionCount     number of sections
//
//   For each section:
//     char   Name[16]         section name (null-padded, matches VXL)
//     For each frame:
//       HVAMatrix (48 bytes)  3x4 row-major float matrix
//
// This module:
//   - Loads and validates HVA files from disk
//   - Provides indexed and named section lookups
//   - Applies per-frame transforms to 3D points
//   - Converts HVA matrices to the engine's Matrix3D format
//   - Performs linear interpolation between adjacent frames
// =============================================================================

#include "FileFormats/HVA.h"
#include "Math/Matrix3D.h"
#include "Math/Point3D.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <strings.h>  // strcasecmp

// =============================================================================
// Constants
// =============================================================================
static const char   HVA_SIGNATURE[4]     = {'H', 'V', 'A', ' '};
static const int32  HVA_HEADER_SIZE      = 12;
static const int32  HVA_SECTION_NAME_LEN = 16;
static const int32  HVA_MATRIX_SIZE      = 48;   // 12 floats * 4 bytes
static const float  HVA_FLOAT_EPSILON    = 1e-6f;

// =============================================================================
// File-local helpers
// =============================================================================

// -----------------------------------------------------------------------------
// HVA_ReadUInt32LE - Read a little-endian uint32 from a byte buffer
// -----------------------------------------------------------------------------
static uint32 HVA_ReadUInt32LE(const BYTE* p)
{
    return  static_cast<uint32>(p[0])
         | (static_cast<uint32>(p[1]) << 8)
         | (static_cast<uint32>(p[2]) << 16)
         | (static_cast<uint32>(p[3]) << 24);
}

// -----------------------------------------------------------------------------
// HVA_ReadFloatLE - Read a little-endian IEEE-754 float from a byte buffer
// -----------------------------------------------------------------------------
static float HVA_ReadFloatLE(const BYTE* p)
{
    uint32 bits = HVA_ReadUInt32LE(p);
    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

// -----------------------------------------------------------------------------
// HVA_ValidateMatrix - Sanity-check a transform matrix for NaN/Inf values
//
// Returns true if all twelve components are finite numbers.  Corrupted HVA
// files sometimes contain NaN or infinity due to bad exporter output; this
// check prevents those from poisoning the rendering pipeline.
// -----------------------------------------------------------------------------
static bool HVA_ValidateMatrix(const HVAMatrix* pMatrix)
{
    if (!pMatrix) return false;

    const float* p = reinterpret_cast<const float*>(pMatrix);
    for (int32 i = 0; i < 12; ++i) {
        if (!std::isfinite(p[i])) return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// HVA_Lerp - Linear interpolation between two floats
// -----------------------------------------------------------------------------
static inline float HVA_Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

// -----------------------------------------------------------------------------
// HVA_LerpMatrix - Linearly interpolate between two HVA matrices
//
// Produces an intermediate transform between frame A and frame B.  The
// parameter t ranges from 0.0 (frame A) to 1.0 (frame B).  This is used
// for smooth animation playback when the render rate differs from the
// stored frame rate.
// -----------------------------------------------------------------------------
static void HVA_LerpMatrix(const HVAMatrix* pA, const HVAMatrix* pB,
                           float t, HVAMatrix* pOut)
{
    if (!pA || !pB || !pOut) return;

    // Clamp t to [0, 1] to avoid overshoot.
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    const float* a = reinterpret_cast<const float*>(pA);
    const float* b = reinterpret_cast<const float*>(pB);
    float* out     = reinterpret_cast<float*>(pOut);

    for (int32 i = 0; i < 12; ++i) {
        out[i] = HVA_Lerp(a[i], b[i], t);
    }
}

// -----------------------------------------------------------------------------
// HVA_ConvertToMatrix3D - Convert an HVA 3x4 matrix into the engine Matrix3D
//
// The HVAMatrix stores 12 floats in row-major order:
//   M11 M12 M13 M14
//   M21 M22 M23 M24
//   M31 M32 M33 M34
//
// Matrix3D uses the identical layout (row[3][4]), so we can copy directly.
// -----------------------------------------------------------------------------
static void HVA_ConvertToMatrix3D(const HVAMatrix* pSrc, Matrix3D* pDest)
{
    if (!pSrc || !pDest) return;

    pDest->row[0][0] = pSrc->M11; pDest->row[0][1] = pSrc->M12;
    pDest->row[0][2] = pSrc->M13; pDest->row[0][3] = pSrc->M14;

    pDest->row[1][0] = pSrc->M21; pDest->row[1][1] = pSrc->M22;
    pDest->row[1][2] = pSrc->M23; pDest->row[1][3] = pSrc->M24;

    pDest->row[2][0] = pSrc->M31; pDest->row[2][1] = pSrc->M32;
    pDest->row[2][2] = pSrc->M33; pDest->row[2][3] = pSrc->M34;
}

// -----------------------------------------------------------------------------
// HVA_ExtractTranslation - Extract the translation column from an HVA matrix
// -----------------------------------------------------------------------------
static void HVA_ExtractTranslation(const HVAMatrix* pMatrix,
                                   float& outX, float& outY, float& outZ)
{
    if (!pMatrix) {
        outX = outY = outZ = 0.0f;
        return;
    }
    outX = pMatrix->M14;
    outY = pMatrix->M24;
    outZ = pMatrix->M34;
}

// -----------------------------------------------------------------------------
// HVA_ExtractScale - Extract the per-axis scale from an HVA matrix
//
// The scale is the length of each row's first three components (the basis
// vectors).  This is useful for normalising the matrix before applying it
// to normals or for detecting non-uniform scaling.
// -----------------------------------------------------------------------------
static void HVA_ExtractScale(const HVAMatrix* pMatrix,
                             float& outSx, float& outSy, float& outSz)
{
    if (!pMatrix) {
        outSx = outSy = outSz = 1.0f;
        return;
    }

    outSx = std::sqrt(pMatrix->M11 * pMatrix->M11
                    + pMatrix->M12 * pMatrix->M12
                    + pMatrix->M13 * pMatrix->M13);

    outSy = std::sqrt(pMatrix->M21 * pMatrix->M21
                    + pMatrix->M22 * pMatrix->M22
                    + pMatrix->M23 * pMatrix->M23);

    outSz = std::sqrt(pMatrix->M31 * pMatrix->M31
                    + pMatrix->M32 * pMatrix->M32
                    + pMatrix->M33 * pMatrix->M33);

    // Guard against zero-length basis vectors.
    if (outSx < HVA_FLOAT_EPSILON) outSx = 1.0f;
    if (outSy < HVA_FLOAT_EPSILON) outSy = 1.0f;
    if (outSz < HVA_FLOAT_EPSILON) outSz = 1.0f;
}

// =============================================================================
// HVAClass implementation
// =============================================================================

// -----------------------------------------------------------------------------
// HVAClass::HVAClass - Default constructor
//
// Initialises all pointers to null and clears the section name table.  The
// object is in an empty state until LoadFromFile() is called.
// -----------------------------------------------------------------------------
HVAClass::HVAClass()
    : m_pRawData(nullptr)
    , m_RawDataSize(0)
    , m_bOwnsData(false)
    , m_pHeader(nullptr)
    , m_FrameCount(0)
    , m_SectionCount(0)
    , m_pMatrixData(nullptr)
{
    // Initialize section names and matrix pointers.
    for (int32 i = 0; i < MaxSections; ++i)
    {
        std::memset(m_Sections[i].Name, 0, sizeof(m_Sections[i].Name));
        m_Sections[i].Matrices = nullptr;
    }
}

// -----------------------------------------------------------------------------
// HVAClass::HVAClass(const char*) - Constructor with filename
//
// Constructs the object and immediately attempts to load the HVA file.  If
// the load fails, the object remains in an empty state (GetFrameCount() and
// GetSectionCount() return 0).
// -----------------------------------------------------------------------------
HVAClass::HVAClass(const char* pFilename)
    : m_pRawData(nullptr)
    , m_RawDataSize(0)
    , m_bOwnsData(false)
    , m_pHeader(nullptr)
    , m_FrameCount(0)
    , m_SectionCount(0)
    , m_pMatrixData(nullptr)
{
    for (int32 i = 0; i < MaxSections; ++i)
    {
        std::memset(m_Sections[i].Name, 0, sizeof(m_Sections[i].Name));
        m_Sections[i].Matrices = nullptr;
    }

    if (pFilename)
        LoadFromFile(pFilename);
}

// -----------------------------------------------------------------------------
// HVAClass::~HVAClass - Destructor
//
// Releases the raw file buffer if this object owns it (i.e. the data was
// allocated by LoadFromFile rather than supplied externally).  All section
// matrix pointers are dangling after the buffer is freed, so they are
// explicitly nulled.
// -----------------------------------------------------------------------------
HVAClass::~HVAClass()
{
    if (m_bOwnsData)
    {
        if (m_pRawData)
        {
            std::free(m_pRawData);
            m_pRawData = nullptr;
        }
    }

    m_pHeader = nullptr;
    m_pMatrixData = nullptr;
    m_FrameCount = 0;
    m_SectionCount = 0;
    m_bOwnsData = false;
}

// -----------------------------------------------------------------------------
// HVAClass::LoadFromFile - Parse an HVA animation file
//
// Steps:
//   1. Open the file and read its entire contents into memory.
//   2. Validate the header signature, frame count, and section count.
//   3. Verify the file is large enough to hold all declared sections.
//   4. Build the section table by reading each section's 16-byte name and
//      recording the address of its matrix array.
//   5. Validate the first matrix of each section for NaN/Inf corruption.
//
// Returns true on success.  On failure the object is reset to an empty state
// and any allocated buffer is freed.
// -----------------------------------------------------------------------------
bool HVAClass::LoadFromFile(const char* pFilename)
{
    // Clean up existing data.
    if (m_bOwnsData && m_pRawData)
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
    }

    m_pHeader = nullptr;
    m_pMatrixData = nullptr;
    m_FrameCount = 0;
    m_SectionCount = 0;
    m_bOwnsData = false;
    m_RawDataSize = 0;

    for (int32 i = 0; i < MaxSections; ++i)
    {
        std::memset(m_Sections[i].Name, 0, sizeof(m_Sections[i].Name));
        m_Sections[i].Matrices = nullptr;
    }

    // Null filename is a no-op.
    if (!pFilename) return false;

    // Open the file in binary mode.
    std::FILE* pFile = std::fopen(pFilename, "rb");
    if (!pFile)
        return false;

    // Determine the file size.
    std::fseek(pFile, 0, SEEK_END);
    long fileSize = std::ftell(pFile);
    std::fseek(pFile, 0, SEEK_SET);

    if (fileSize < HVA_HEADER_SIZE)
    {
        std::fclose(pFile);
        return false;
    }

    // Allocate and read the entire file into a single buffer.  The HVA
    // format is small enough (a few KB for most models) that a single
    // allocation is the most efficient approach.
    m_RawDataSize = static_cast<int32>(fileSize);
    m_pRawData = static_cast<BYTE*>(std::malloc(static_cast<size_t>(m_RawDataSize)));
    if (!m_pRawData)
    {
        std::fclose(pFile);
        m_RawDataSize = 0;
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

    // ── Parse and validate the header ───────────────────────────────────
    // The header is 12 bytes: 4-byte signature + 4-byte frame count + 4-byte
    // section count, all little-endian.
    m_pHeader = reinterpret_cast<HVAHeader*>(m_pRawData);

    // Validate the signature.  Some HVA files use "HVA " (with a space) while
    // older exports may omit the trailing space.  We accept either variant.
    bool sigOk = (std::memcmp(m_pHeader->Signature, HVA_SIGNATURE, 4) == 0);
    if (!sigOk) {
        // Fall back: check for "HVA\0" variant.
        if (m_pHeader->Signature[0] == 'H' &&
            m_pHeader->Signature[1] == 'V' &&
            m_pHeader->Signature[2] == 'A') {
            sigOk = true;
        }
    }
    if (!sigOk) {
        std::free(m_pRawData);
        m_pRawData = nullptr;
        m_RawDataSize = 0;
        m_bOwnsData = false;
        m_pHeader = nullptr;
        return false;
    }

    // Read frame and section counts using explicit little-endian decode to
    // avoid alignment issues on platforms where uint32 is not 4-byte aligned.
    BYTE* pHeaderBytes = m_pRawData;
    m_FrameCount  = static_cast<int32>(HVA_ReadUInt32LE(pHeaderBytes + 4));
    m_SectionCount = static_cast<int32>(HVA_ReadUInt32LE(pHeaderBytes + 8));

    // Validate counts against hard limits.
    if (m_FrameCount <= 0 || m_FrameCount > MaxFrames ||
        m_SectionCount <= 0 || m_SectionCount > MaxSections)
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
        m_RawDataSize = 0;
        m_bOwnsData = false;
        m_pHeader = nullptr;
        m_FrameCount = 0;
        m_SectionCount = 0;
        return false;
    }

    // ── Validate the total data size ────────────────────────────────────
    // Expected size = header + sections * (name + frames * matrix)
    size_t expectedSize = static_cast<size_t>(HVA_HEADER_SIZE)
        + static_cast<size_t>(m_SectionCount) * (
              static_cast<size_t>(HVA_SECTION_NAME_LEN)
            + static_cast<size_t>(m_FrameCount) * static_cast<size_t>(HVA_MATRIX_SIZE)
          );

    if (static_cast<size_t>(m_RawDataSize) < expectedSize)
    {
        std::free(m_pRawData);
        m_pRawData = nullptr;
        m_RawDataSize = 0;
        m_bOwnsData = false;
        m_pHeader = nullptr;
        m_FrameCount = 0;
        m_SectionCount = 0;
        return false;
    }

    // ── Parse section data ──────────────────────────────────────────────
    // After the header, the file contains SectionCount blocks.  Each block
    // starts with a 16-byte section name followed by FrameCount matrices.
    BYTE* pData = m_pRawData + HVA_HEADER_SIZE;

    for (int32 sec = 0; sec < m_SectionCount; ++sec)
    {
        // Copy the 16-byte section name and ensure null-termination.
        std::memcpy(m_Sections[sec].Name, pData, HVA_SECTION_NAME_LEN);
        m_Sections[sec].Name[HVA_SECTION_NAME_LEN - 1] = '\0';

        // Strip trailing whitespace/null padding from the name for cleaner
        // comparisons during FindSection().
        for (int32 c = HVA_SECTION_NAME_LEN - 1; c >= 0; --c) {
            if (m_Sections[sec].Name[c] == '\0' || m_Sections[sec].Name[c] == ' ') {
                m_Sections[sec].Name[c] = '\0';
            } else {
                break;
            }
        }

        pData += HVA_SECTION_NAME_LEN;

        // Record the address of this section's matrix array.  The matrices
        // remain in the raw buffer; no copy is made.
        m_Sections[sec].Matrices = reinterpret_cast<HVAMatrix*>(pData);
        pData += static_cast<size_t>(m_FrameCount) * static_cast<size_t>(HVA_MATRIX_SIZE);

        // Validate the first matrix of each section to catch corruption
        // early.  If the first frame is bad, we mark the section as having
        // no valid matrices by leaving the pointer but the caller will get
        // null from GetTransform when HVA_ValidateMatrix fails.
        if (!HVA_ValidateMatrix(&m_Sections[sec].Matrices[0])) {
            // Corrupted matrix detected.  We keep the pointer so that the
            // section table is consistent, but downstream consumers should
            // validate before use.
        }
    }

    // Store a convenience pointer to the first section's first matrix.
    if (m_SectionCount > 0 && m_Sections[0].Matrices) {
        m_pMatrixData = m_Sections[0].Matrices;
    } else {
        m_pMatrixData = nullptr;
    }

    return true;
}

// -----------------------------------------------------------------------------
// HVAClass::GetTransform(sectionIndex, frameIndex) - Get a transform matrix
//
// Returns a pointer to the HVAMatrix for the given section and frame, or
// nullptr if the indices are out of range or the section has no matrix data.
// -----------------------------------------------------------------------------
const HVAMatrix* HVAClass::GetTransform(int32 sectionIndex, int32 frameIndex) const
{
    if (sectionIndex < 0 || sectionIndex >= m_SectionCount)
        return nullptr;

    if (frameIndex < 0 || frameIndex >= m_FrameCount)
        return nullptr;

    if (!m_Sections[sectionIndex].Matrices)
        return nullptr;

    return &m_Sections[sectionIndex].Matrices[frameIndex];
}

// -----------------------------------------------------------------------------
// HVAClass::GetTransform(sectionName, frameIndex) - Get a transform by name
//
// Looks up the section by name, then delegates to the indexed overload.
// -----------------------------------------------------------------------------
const HVAMatrix* HVAClass::GetTransform(const char* sectionName, int32 frameIndex) const
{
    int32 sectionIndex = FindSection(sectionName);
    if (sectionIndex < 0)
        return nullptr;

    return GetTransform(sectionIndex, frameIndex);
}

// -----------------------------------------------------------------------------
// HVAClass::GetSectionName - Get the name of a section
//
// Returns a pointer to the section's null-terminated name string, or nullptr
// if the index is out of range.
// -----------------------------------------------------------------------------
const char* HVAClass::GetSectionName(int32 sectionIndex) const
{
    if (sectionIndex < 0 || sectionIndex >= m_SectionCount)
        return nullptr;

    return m_Sections[sectionIndex].Name;
}

// -----------------------------------------------------------------------------
// HVAClass::FindSection - Find section index by name
//
// Performs a linear search through the section table.  The comparison is
// case-sensitive to match the original engine behaviour (VXL section names
// are case-sensitive).  Returns -1 if not found or if the name is null.
// -----------------------------------------------------------------------------
int32 HVAClass::FindSection(const char* sectionName) const
{
    if (!sectionName)
        return -1;

    // Handle empty string: no valid section has an empty name after the
    // trailing-whitespace stripping in LoadFromFile.
    if (sectionName[0] == '\0')
        return -1;

    for (int32 i = 0; i < m_SectionCount; ++i)
    {
        if (std::strcmp(m_Sections[i].Name, sectionName) == 0)
            return i;
    }

    // Fall back to a case-insensitive comparison.  Some mod tools export
    // section names with different casing than the VXL file, so we try a
    // lenient match as a last resort.
    for (int32 i = 0; i < m_SectionCount; ++i)
    {
        if (strcasecmp(m_Sections[i].Name, sectionName) == 0)
            return i;
    }

    return -1;
}

// -----------------------------------------------------------------------------
// HVAClass::GetSectionTransforms - Get all transforms for a section
//
// Returns a pointer to the first HVAMatrix in the section's frame array.
// The array contains GetFrameCount() matrices.  Returns nullptr if the
// index is out of range or the section has no matrix data.
// -----------------------------------------------------------------------------
const HVAMatrix* HVAClass::GetSectionTransforms(int32 sectionIndex) const
{
    if (sectionIndex < 0 || sectionIndex >= m_SectionCount)
        return nullptr;

    return m_Sections[sectionIndex].Matrices;
}

// -----------------------------------------------------------------------------
// HVAClass::ApplyTransform - Apply a 3x4 matrix transform to a 3D point
//
// Standard affine transformation:
//   x' = M11*x + M12*y + M13*z + M14
//   y' = M21*x + M22*y + M23*z + M24
//   z' = M31*x + M32*y + M33*z + M34
//
// The transformation is applied in-place: the input coordinates (x, y, z)
// are replaced by the transformed coordinates.  If the section or frame
// index is invalid, the coordinates are left unchanged.
// -----------------------------------------------------------------------------
void HVAClass::ApplyTransform(int32 sectionIndex, int32 frameIndex,
                              float& x, float& y, float& z) const
{
    const HVAMatrix* pMatrix = GetTransform(sectionIndex, frameIndex);
    if (!pMatrix)
        return;

    // Validate the matrix before applying it.  A corrupted matrix (containing
    // NaN or infinity) would propagate through the rendering pipeline and
    // produce visible artifacts.  If the matrix is invalid, we leave the
    // point unchanged rather than producing garbage output.
    if (!HVA_ValidateMatrix(pMatrix))
        return;

    // Compute the transformed coordinates using temporaries so that the
    // original values are available for all three calculations.
    float nx = pMatrix->M11 * x + pMatrix->M12 * y + pMatrix->M13 * z + pMatrix->M14;
    float ny = pMatrix->M21 * x + pMatrix->M22 * y + pMatrix->M23 * z + pMatrix->M24;
    float nz = pMatrix->M31 * x + pMatrix->M32 * y + pMatrix->M33 * z + pMatrix->M34;

    x = nx;
    y = ny;
    z = nz;
}

// =============================================================================
// File-scope utility functions (not member functions)
//
// These helpers operate on HVA data but do not require HVAClass membership.
// They are used internally by the rendering system to convert HVA matrices
// into the engine's Matrix3D format and to perform frame interpolation.
// =============================================================================

// -----------------------------------------------------------------------------
// HVA_ConvertMatrix - Convert an HVA matrix to the engine's Matrix3D format
//
// This is the public entry point used by the voxel renderer.  It wraps the
// file-local HVA_ConvertToMatrix3D helper so that external code does not
// need to know about the internal HVAMatrix layout.
// -----------------------------------------------------------------------------
void HVA_ConvertMatrix(const HVAMatrix* pSrc, Matrix3D* pDest)
{
    HVA_ConvertToMatrix3D(pSrc, pDest);
}

// -----------------------------------------------------------------------------
// HVA_InterpolateFrame - Interpolate between two frames of a section
//
// Given a HVAClass, a section index, and a floating-point frame number,
// this function produces an interpolated HVAMatrix.  The integer part of
// frameNumber selects the base frame; the fractional part determines the
// blend weight between the base frame and the next frame.
//
// If frameNumber is exactly on an integer boundary, the matrix for that
// frame is copied directly (no interpolation).  If the section or frame
// indices are out of range, the output is set to identity.
// -----------------------------------------------------------------------------
void HVA_InterpolateFrame(const HVAClass* pHVA, int32 sectionIndex,
                          float frameNumber, HVAMatrix* pOut)
{
    if (!pHVA || !pOut) return;

    int32 frameCount = pHVA->GetFrameCount();
    if (frameCount <= 0 || sectionIndex < 0 || sectionIndex >= pHVA->GetSectionCount()) {
        // Identity matrix on failure.
        std::memset(pOut, 0, sizeof(HVAMatrix));
        pOut->M11 = 1.0f; pOut->M22 = 1.0f; pOut->M33 = 1.0f;
        return;
    }

    // Decompose the frame number into base frame and blend weight.
    int32 baseFrame = static_cast<int32>(frameNumber);
    float frac = frameNumber - static_cast<float>(baseFrame);

    // Clamp the base frame to the valid range.
    if (baseFrame < 0) { baseFrame = 0; frac = 0.0f; }
    if (baseFrame >= frameCount) { baseFrame = frameCount - 1; frac = 0.0f; }

    // If the fractional part is negligible, copy the exact frame.
    if (frac < HVA_FLOAT_EPSILON) {
        const HVAMatrix* pM = pHVA->GetTransform(sectionIndex, baseFrame);
        if (pM) {
            std::memcpy(pOut, pM, sizeof(HVAMatrix));
        } else {
            std::memset(pOut, 0, sizeof(HVAMatrix));
            pOut->M11 = 1.0f; pOut->M22 = 1.0f; pOut->M33 = 1.0f;
        }
        return;
    }

    // Determine the next frame.  If the base frame is the last frame, wrap
    // around to frame 0 for looping animations.
    int32 nextFrame = baseFrame + 1;
    if (nextFrame >= frameCount) {
        nextFrame = 0;
    }

    const HVAMatrix* pA = pHVA->GetTransform(sectionIndex, baseFrame);
    const HVAMatrix* pB = pHVA->GetTransform(sectionIndex, nextFrame);

    if (pA && pB) {
        HVA_LerpMatrix(pA, pB, frac, pOut);
    } else if (pA) {
        std::memcpy(pOut, pA, sizeof(HVAMatrix));
    } else {
        std::memset(pOut, 0, sizeof(HVAMatrix));
        pOut->M11 = 1.0f; pOut->M22 = 1.0f; pOut->M33 = 1.0f;
    }
}

// -----------------------------------------------------------------------------
// HVA_GetTranslation - Extract the translation from an HVA matrix
// -----------------------------------------------------------------------------
void HVA_GetTranslation(const HVAMatrix* pMatrix,
                        float& outX, float& outY, float& outZ)
{
    HVA_ExtractTranslation(pMatrix, outX, outY, outZ);
}

// -----------------------------------------------------------------------------
// HVA_GetScale - Extract the per-axis scale from an HVA matrix
// -----------------------------------------------------------------------------
void HVA_GetScale(const HVAMatrix* pMatrix,
                  float& outSx, float& outSy, float& outSz)
{
    HVA_ExtractScale(pMatrix, outSx, outSy, outSz);
}

// -----------------------------------------------------------------------------
// HVA_MultiplyMatrix - Multiply two HVA matrices (A * B)
//
// Performs a 3x4 affine matrix multiplication.  The result represents the
// composition of transform A followed by transform B.  This is used when
// combining a section's HVA transform with the model's base transform.
// -----------------------------------------------------------------------------
void HVA_MultiplyMatrix(const HVAMatrix* pA, const HVAMatrix* pB, HVAMatrix* pOut)
{
    if (!pA || !pB || !pOut) return;

    // Row 0
    pOut->M11 = pA->M11 * pB->M11 + pA->M12 * pB->M21 + pA->M13 * pB->M31;
    pOut->M12 = pA->M11 * pB->M12 + pA->M12 * pB->M22 + pA->M13 * pB->M32;
    pOut->M13 = pA->M11 * pB->M13 + pA->M12 * pB->M23 + pA->M13 * pB->M33;
    pOut->M14 = pA->M11 * pB->M14 + pA->M12 * pB->M24 + pA->M13 * pB->M34 + pA->M14;

    // Row 1
    pOut->M21 = pA->M21 * pB->M11 + pA->M22 * pB->M21 + pA->M23 * pB->M31;
    pOut->M22 = pA->M21 * pB->M12 + pA->M22 * pB->M22 + pA->M23 * pB->M32;
    pOut->M23 = pA->M21 * pB->M13 + pA->M22 * pB->M23 + pA->M23 * pB->M33;
    pOut->M24 = pA->M21 * pB->M14 + pA->M22 * pB->M24 + pA->M23 * pB->M34 + pA->M24;

    // Row 2
    pOut->M31 = pA->M31 * pB->M11 + pA->M32 * pB->M21 + pA->M33 * pB->M31;
    pOut->M32 = pA->M31 * pB->M12 + pA->M32 * pB->M22 + pA->M33 * pB->M32;
    pOut->M33 = pA->M31 * pB->M13 + pA->M32 * pB->M23 + pA->M33 * pB->M33;
    pOut->M34 = pA->M31 * pB->M14 + pA->M32 * pB->M24 + pA->M33 * pB->M34 + pA->M34;
}
