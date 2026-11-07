#include "FileFormats/VXL.h"
#include "IO/MixFileClass.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// VXLClass —— 严格按 gamemd.exe 的 VoxLib_CTOR（0x755DB0）解析
//
// 载入顺序（asm 原样）：
//   1. ReadFile(32)                     -> 文件头
//   2. new(SectionCount * 12)           -> section 表（只保留 first/last/flags）
//   3. ReadFile(SectionCount * 28)      -> section 头，逐条抽取三个字段
//   4. ReadFile(BlobSize)               -> 体素载荷
//   5. ReadFile(RecordCount * 92)       -> record 表
//
// 本实现不再拷贝/改写 record，而是在访问时把文件里的 uint32 偏移加上
// blob 基址，等价于 asm 里「载入时把 offset 改写成指针」的结果，但不需要
// 164 字节的运行时展开缓冲。
// ============================================================================

namespace
{
    constexpr uint32 kVoxelEmpty = 0xFFFFFFFFu;

    inline uint32 ReadU32(const BYTE* p)
    {
        uint32 v;
        std::memcpy(&v, p, sizeof(v));
        return v;
    }

    inline float ReadF32(const BYTE* p)
    {
        float v;
        std::memcpy(&v, p, sizeof(v));
        return v;
    }
}

// ----------------------------------------------------------------------------
// 构造 / 析构
// ----------------------------------------------------------------------------
VXLClass::VXLClass()
    : m_pRawData(nullptr)
    , m_RawDataSize(0)
    , m_bOwnsData(false)
    , m_pHeader(nullptr)
    , m_pSections(nullptr)
    , m_pRecords(nullptr)
    , m_pBlob(nullptr)
    , m_SectionCount(0)
    , m_RecordCount(0)
    , m_BlobSize(0)
    , m_LastError("not loaded")
{
}

VXLClass::VXLClass(const char* pFilename)
    : VXLClass()
{
    if (pFilename != nullptr)
        LoadFromFile(pFilename);
}

VXLClass::~VXLClass()
{
    Reset();
}

void VXLClass::Reset()
{
    if (m_bOwnsData && m_pRawData != nullptr)
        std::free(m_pRawData);

    m_pRawData = nullptr;
    m_RawDataSize = 0;
    m_bOwnsData = false;
    m_pHeader = nullptr;
    m_pSections = nullptr;
    m_pRecords = nullptr;
    m_pBlob = nullptr;
    m_SectionCount = 0;
    m_RecordCount = 0;
    m_BlobSize = 0;
}

// ----------------------------------------------------------------------------
// LoadFromFile
// ----------------------------------------------------------------------------
bool VXLClass::LoadFromFile(const char* pFilename)
{
    Reset();
    m_LastError = "not loaded";

    if (pFilename == nullptr || pFilename[0] == '\0')
    {
        m_LastError = "empty filename";
        return false;
    }

    // ------------------------------------------------------------------
    // 先查 MIX：零售版的 .vxl 全在 ra2md.mix / expandmd*.mix 里，
    // 直接用 fopen 永远找不到模型。
    // ------------------------------------------------------------------
    {
        int32 mixSize = 0;
        void* pMixData = MixFileClass::RetrieveSized(pFilename, &mixSize);
        if (pMixData != nullptr && mixSize > static_cast<int32>(sizeof(VXLFileHeader)))
        {
            m_pRawData = static_cast<BYTE*>(std::malloc(static_cast<size_t>(mixSize)));
            if (m_pRawData != nullptr)
            {
                std::memcpy(m_pRawData, pMixData, static_cast<size_t>(mixSize));
                m_RawDataSize = mixSize;
                m_bOwnsData = true;
            }
            YRMemory::Deallocate(pMixData);
        }
    }

    // ------------------------------------------------------------------
    // 散装文件兜底
    // ------------------------------------------------------------------
    if (m_pRawData == nullptr)
    {
        std::FILE* pFile = std::fopen(pFilename, "rb");
        if (pFile == nullptr)
        {
            m_LastError = "file not found";
            return false;
        }

        std::fseek(pFile, 0, SEEK_END);
        const long fileSize = std::ftell(pFile);
        std::fseek(pFile, 0, SEEK_SET);

        if (fileSize < static_cast<long>(sizeof(VXLFileHeader)))
        {
            std::fclose(pFile);
            m_LastError = "file too small";
            return false;
        }

        m_RawDataSize = static_cast<int32>(fileSize);
        m_pRawData = static_cast<BYTE*>(std::malloc(static_cast<size_t>(m_RawDataSize)));
        if (m_pRawData == nullptr)
        {
            std::fclose(pFile);
            m_LastError = "out of memory";
            return false;
        }

        const size_t read = std::fread(m_pRawData, 1, static_cast<size_t>(m_RawDataSize), pFile);
        std::fclose(pFile);

        if (read != static_cast<size_t>(m_RawDataSize))
        {
            std::free(m_pRawData);
            m_pRawData = nullptr;
            m_RawDataSize = 0;
            m_LastError = "short read";
            return false;
        }

        m_bOwnsData = true;
    }

    return Parse();
}

// ----------------------------------------------------------------------------
// Parse - 按 asm 顺序切分四段
// ----------------------------------------------------------------------------
bool VXLClass::Parse()
{
    if (m_RawDataSize < static_cast<int32>(sizeof(VXLFileHeader)))
    {
        m_LastError = "file too small";
        Reset();
        return false;
    }

    const BYTE* pBase = m_pRawData;
    const BYTE* pEnd  = pBase + m_RawDataSize;

    // ---- 1) 文件头 ----
    m_pHeader = reinterpret_cast<const VXLFileHeader*>(pBase);

    // asm 的校验是 ReadFile 返回值 == 32；这里额外核对签名的前 4 字节，
    // 与原版 ASCIIZ 字符串 "Voxel Section" 一致。
    if (std::memcmp(m_pHeader->FileType, "Voxel Section", 4) != 0)
    {
        m_LastError = "bad signature";
        Reset();
        return false;
    }

    const uint32 sectionCount = m_pHeader->SectionCount;
    const uint32 recordCount  = m_pHeader->RecordCount;
    const uint32 blobSize     = m_pHeader->BlobSize;

    if (sectionCount == 0 || sectionCount > static_cast<uint32>(MaxSections))
    {
        m_LastError = "bad section count";
        Reset();
        return false;
    }
    if (recordCount == 0 || recordCount > static_cast<uint32>(MaxRecords))
    {
        m_LastError = "bad record count";
        Reset();
        return false;
    }

    // ---- 2) section 头 ----
    const size_t sectionsOffset = sizeof(VXLFileHeader);
    const size_t sectionsBytes  = static_cast<size_t>(sectionCount) * sizeof(VXLSectionEntry);
    if (sectionsOffset + sectionsBytes > static_cast<size_t>(m_RawDataSize))
    {
        m_LastError = "section table out of range";
        Reset();
        return false;
    }

    m_pSections = reinterpret_cast<const VXLSectionEntry*>(pBase + sectionsOffset);

    // ---- 3) blob ----
    const size_t blobOffset = sectionsOffset + sectionsBytes;
    if (blobOffset + static_cast<size_t>(blobSize) > static_cast<size_t>(m_RawDataSize))
    {
        m_LastError = "blob out of range";
        Reset();
        return false;
    }
    m_pBlob = pBase + blobOffset;

    // ---- 4) record 表 ----
    const size_t recordsOffset = blobOffset + static_cast<size_t>(blobSize);
    const size_t recordsBytes  = static_cast<size_t>(recordCount) * sizeof(VXLRecordRaw);
    if (recordsOffset + recordsBytes > static_cast<size_t>(m_RawDataSize))
    {
        m_LastError = "record table out of range";
        Reset();
        return false;
    }

    m_pRecords = reinterpret_cast<const VXLRecordRaw*>(pBase + recordsOffset);

    // 每条 section 的 FirstRecord/LastRecord 必须落在 record 表内；
    // 越界的 section 在 GetRecord 里会被拒绝，但这里先做一次整体体检。
    for (uint32 i = 0; i < sectionCount; ++i)
    {
        const VXLSectionEntry& s = m_pSections[i];
        if (s.FirstRecord > s.LastRecord || s.LastRecord >= recordCount)
        {
            m_LastError = "bad section record range";
            Reset();
            return false;
        }
    }

    m_SectionCount = static_cast<int32>(sectionCount);
    m_RecordCount  = static_cast<int32>(recordCount);
    m_BlobSize     = static_cast<int32>(blobSize);
    m_LastError    = "";

    (void)pEnd;
    return true;
}

// ----------------------------------------------------------------------------
// section 查询
// ----------------------------------------------------------------------------
const char* VXLClass::GetSectionName(int32 sectionIndex) const
{
    if (!HasSection(sectionIndex))
        return nullptr;
    return m_pSections[sectionIndex].Name;
}

bool VXLClass::HasSection(int32 sectionIndex) const
{
    return m_pSections != nullptr && sectionIndex >= 0 && sectionIndex < m_SectionCount;
}

int32 VXLClass::GetFrameCount(int32 sectionIndex) const
{
    if (!HasSection(sectionIndex))
        return 0;

    // asm: VoxLib_Process 直接用 FirstRecord + frame 索引 record，
    // frame 的合法范围就是 [0, LastRecord - FirstRecord]。
    const VXLSectionEntry& s = m_pSections[sectionIndex];
    return static_cast<int32>(s.LastRecord - s.FirstRecord) + 1;
}

// ----------------------------------------------------------------------------
// GetRecord - 等价于 VoxLib_Process(section, frame) 的展开
// ----------------------------------------------------------------------------
bool VXLClass::GetRecord(int32 sectionIndex, int32 frame, VXLRecordView& out) const
{
    if (!HasSection(sectionIndex))
        return false;

    const VXLSectionEntry& s = m_pSections[sectionIndex];
    const int32 frameCount = static_cast<int32>(s.LastRecord - s.FirstRecord) + 1;

    if (frameCount <= 0)
        return false;

    // 越界按帧数取模（HVA 帧与 VXL 帧数理论上一致，但缺失 HVA 时会传 0）
    if (frame < 0)
        frame = 0;
    if (frame >= frameCount)
        frame %= frameCount;

    const uint32 recordIndex = s.FirstRecord + static_cast<uint32>(frame);
    if (recordIndex >= static_cast<uint32>(m_RecordCount))
        return false;

    const VXLRecordRaw& r = m_pRecords[recordIndex];

    // ---- 体素数据 ----
    // off0 是数据起点；off1 若大于 off0 则视为终点，否则用 Size 推导长度。
    const uint32 off0 = r.Offset0;
    const uint32 off1 = r.Offset1;

    if (off0 >= static_cast<uint32>(m_BlobSize))
        return false;

    int32 sizeX = static_cast<int32>(r.SizeX);
    int32 sizeY = static_cast<int32>(r.SizeY);
    int32 sizeZ = static_cast<int32>(r.SizeZ);

    if (sizeX <= 0) sizeX = 1;
    if (sizeY <= 0) sizeY = 1;
    if (sizeZ <= 0) sizeZ = 1;

    const int64 bySize = static_cast<int64>(sizeX) * sizeY * sizeZ;
    int64 count = 0;

    if (off1 > off0)
    {
        const uint32 spanBytes = off1 - off0;
        count = static_cast<int64>(spanBytes / sizeof(uint32));
    }

    // off1 不可信或与 Size 不一致时，以 Size 为准（Size 是 record 自带的权威字段）
    if (count <= 0 || count > bySize)
        count = bySize;

    const int64 maxWords = static_cast<int64>((m_BlobSize - static_cast<int32>(off0)) / 4);
    if (count > maxWords)
        count = maxWords;

    if (count <= 0)
        return false;

    out.Voxels      = m_pBlob + off0;
    out.VoxelCount  = static_cast<int32>(count);
    out.SizeX       = sizeX;
    out.SizeY       = sizeY;
    out.SizeZ       = sizeZ;
    out.NormalMode  = static_cast<int32>(r.NormalMode);

    // ---- AABB：Corner0 / Corner1 逐分量取小/取大 ----
    for (int32 i = 0; i < 3; ++i)
    {
        const float a = r.Corner0[i];
        const float b = r.Corner1[i];
        out.Min[i] = (a < b) ? a : b;
        out.Max[i] = (a < b) ? b : a;
    }

    std::memcpy(out.Transform, r.Transform, sizeof(out.Transform));

    return true;
}

// ----------------------------------------------------------------------------
// GetVoxelRaw - 原生 (x,y,z) 索引，x 变化最快
// ----------------------------------------------------------------------------
uint32 VXLClass::GetVoxelRaw(int32 sectionIndex, int32 frame,
                             int32 x, int32 y, int32 z) const
{
    VXLRecordView rec;
    if (!GetRecord(sectionIndex, frame, rec))
        return kVoxelEmpty;

    if (x < 0 || y < 0 || z < 0 ||
        x >= rec.SizeX || y >= rec.SizeY || z >= rec.SizeZ)
        return kVoxelEmpty;

    const int64 index = static_cast<int64>(x)
                      + static_cast<int64>(y) * rec.SizeX
                      + static_cast<int64>(z) * rec.SizeX * rec.SizeY;

    if (index >= rec.VoxelCount)
        return kVoxelEmpty;

    return ReadU32(rec.Voxels + index * 4);
}

// ----------------------------------------------------------------------------
// GetVoxel - 兼容旧接口：调色板索引，0 = 空
// ----------------------------------------------------------------------------
BYTE VXLClass::GetVoxel(int32 sectionIndex, int32 x, int32 y, int32 z) const
{
    const uint32 v = GetVoxelRaw(sectionIndex, 0, x, y, z);
    if (v == kVoxelEmpty)
        return 0;
    return static_cast<BYTE>(v & 0xFF);
}

// ----------------------------------------------------------------------------
// GetBounds - 兼容旧接口：frame 0 的体素索引范围
// ----------------------------------------------------------------------------
void VXLClass::GetBounds(int32 sectionIndex, int32& minX, int32& maxX,
                         int32& minY, int32& maxY, int32& minZ, int32& maxZ) const
{
    minX = maxX = minY = maxY = minZ = maxZ = 0;

    VXLRecordView rec;
    if (!GetRecord(sectionIndex, 0, rec))
        return;

    minX = 0; maxX = rec.SizeX - 1;
    minY = 0; maxY = rec.SizeY - 1;
    minZ = 0; maxZ = rec.SizeZ - 1;
}

// ----------------------------------------------------------------------------
// GetWorldBounds - record 的模型空间 AABB
// ----------------------------------------------------------------------------
bool VXLClass::GetWorldBounds(int32 sectionIndex, int32 frame,
                              float outMin[3], float outMax[3]) const
{
    VXLRecordView rec;
    if (!GetRecord(sectionIndex, frame, rec))
        return false;

    for (int32 i = 0; i < 3; ++i)
    {
        outMin[i] = rec.Min[i];
        outMax[i] = rec.Max[i];
    }
    return true;
}
