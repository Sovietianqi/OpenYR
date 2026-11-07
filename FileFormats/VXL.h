#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Core/Memory.h"
#include "IO/CCFileClass.h"

#include <cstring>

// ============================================================================
// VXL —— Westwood 体素模型格式（gamemd.exe 实测布局）
//
// 全部字段来自对 VoxLib_CTOR（0x755DB0）逐条指令的核对，不是推测：
//
//   1) 文件头，固定 0x20 = 32 字节
//        +0x00  char   FileType[16]    "Voxel Section"
//        +0x10  uint32 Unknown
//        +0x14  uint32 SectionCount     -> 分配 SectionCount * 12  (lea eax,[eax+eax*2]; shl 2)
//        +0x18  uint32 RecordCount      -> 分配 RecordCount * 0xA4 (lea ecx,[ebp+ebp*4];
//                                                                  lea edx,[ebp+ecx*8]; shl 2)
//        +0x1C  uint32 BlobSize         -> 分配 BlobSize 字节
//
//   2) SectionCount 条 section 头，每条 0x1C = 28 字节，读进栈上 28 字节缓冲后
//      只保留三个字段（名字[16] 被丢弃）：
//        +0x00  char   Name[16]        （不保留）
//        +0x10  uint32 FirstRecord     -> 存进 sectionEntry+0
//        +0x14  uint32 LastRecord      -> 存进 sectionEntry+4
//        +0x18  uint32 Flags           -> 只取低字节，存进 sectionEntry+8
//
//   3) BlobSize 字节的体素载荷。每条 record 的 +0x00/+0x04/+0x08 是
//      三个「blob 内偏移」，载入时被就地改写成三个指针：
//        asm: mov eax,[esp+var_C8] ; add eax,[ebx+18h] ; mov [ecx+ebp-0A4h],eax
//      即 record[0] = blobBase + off0, record[1] = blobBase + off1,
//         record[2] = blobBase + off2。
//
//   4) RecordCount 条 record，每条 0x5C = 92 字节，运行时展开成 0xA4 = 164 字节：
//        文件 +0x00  uint32 Off0        运行时 +0x00  BYTE* = blob + Off0
//        文件 +0x04  uint32 Off1        运行时 +0x04  BYTE* = blob + Off1
//        文件 +0x08  uint32 Off2        运行时 +0x08  BYTE* = blob + Off2
//        文件 +0x0C  float  Unknown     运行时 +0x0C  float
//        文件 +0x10  48 字节变换矩阵    运行时 +0x10  48 字节（sub_5AE5E0 只是 memcpy 12 dword）
//                                     （4 行 x 3 列：M11..M14 / M21..M24 / M31..M34）
//        文件 +0x40  float  Corner0[3]  运行时 +0x40  AABB 的 8 个角点（8 x 12 字节，
//        文件 +0x4C  float  Corner1[3]              由 Corner0/Corner1 的 2^3 组合展开，
//                                                    载入时 Buffer_Clear(rec+0x40, 8, 12) 证实）
//        文件 +0x58  uint8  SizeX       运行时 +0xA0  SizeX
//        文件 +0x59  uint8  SizeY       运行时 +0xA1  SizeY
//        文件 +0x5A  uint8  SizeZ       运行时 +0xA2  SizeZ
//        文件 +0x5B  uint8  NormalMode  运行时 +0xA3  NormalMode
//
//   VoxLib_Process(section, frame)（0x755E.. 之后的独立函数）:
//        eax = SectionTable[section].FirstRecord + frame
//        return RecordTable + eax * 164
//     也就是说：**一个 section 的每一动画帧对应一条 record**，record 里有该帧的
//     包围盒与体素数据。旧实现把 record 当成「每个体素一条」，是错的。
//
//   5) 体素载荷编码：blob 里的数据按 uint32 数组存放，0xFFFFFFFF 表示空体素
//      （asm 0x7569F6: cmp dword ptr [edx+edi*4], 0FFFFFFFFh / jz skip）。
//      非空值的低字节是调色板索引，次低字节是法线编号（用于 16 级明暗）。
//      数组索引顺序为 x 变化最快：idx = x + y*SizeX + z*SizeX*SizeY。
//
// 坐标系：VXL 原生为 X 右、Y 前后、Z 上。本类对外暴露原生索引 (x,y,z)，
// 由渲染层负责映射到项目约定（项目 Y 为高度）。
// ============================================================================

// ============================================================================
// VXLFileHeader - 文件头，32 字节（字段名与 asm 语义一致）
// ============================================================================
#pragma pack(push, 1)
struct VXLFileHeader
{
    char   FileType[16];   // +0x00 "Voxel Section"
    uint32 Unknown;        // +0x10
    uint32 SectionCount;   // +0x14
    uint32 RecordCount;    // +0x18
    uint32 BlobSize;       // +0x1C
};

// 磁盘上的 section 头，28 字节
struct VXLSectionEntry
{
    char   Name[16];       // +0x00
    uint32 FirstRecord;    // +0x10
    uint32 LastRecord;     // +0x14
    uint32 Flags;          // +0x18（只取低字节）
};

// 磁盘上的 record，92 字节
struct VXLRecordRaw
{
    uint32 Offset0;        // +0x00 blob 内偏移
    uint32 Offset1;        // +0x04
    uint32 Offset2;        // +0x08
    float  Unknown;        // +0x0C
    float  Transform[12];  // +0x10 4x3
    float  Corner0[3];     // +0x40
    float  Corner1[3];     // +0x4C
    uint8  SizeX;          // +0x58
    uint8  SizeY;          // +0x59
    uint8  SizeZ;          // +0x5A
    uint8  NormalMode;     // +0x5B
};
#pragma pack(pop)

static_assert(sizeof(VXLFileHeader) == 0x20, "VXL file header must be 32 bytes");
static_assert(sizeof(VXLSectionEntry) == 0x1C, "VXL section entry must be 28 bytes");
static_assert(sizeof(VXLRecordRaw) == 0x5C, "VXL record must be 92 bytes");

// ============================================================================
// VXLRecordView - 解析后的一条 record（对应某 section 的某一帧）
// ============================================================================
struct VXLRecordView
{
    const uint8* Voxels;      // 指向 blob 内的 uint32 数组
    int32        VoxelCount;  // 数组长度（已按 BlobSize 与 Size 校验）
    int32        SizeX;       // 体素尺寸（1..255）
    int32        SizeY;
    int32        SizeZ;
    int32        NormalMode;  // +0xA3
    float        Min[3];      // Corner0 / Corner1 逐分量取小
    float        Max[3];      // 逐分量取大
    float        Transform[12]; // +0x10 原样

    VXLRecordView()
        : Voxels(nullptr), VoxelCount(0)
        , SizeX(0), SizeY(0), SizeZ(0), NormalMode(0)
    {
        for (int32 i = 0; i < 3; ++i) { Min[i] = 0.0f; Max[i] = 0.0f; }
        for (int32 i = 0; i < 12; ++i) Transform[i] = 0.0f;
    }
};

// ============================================================================
// VXLClass - 体素模型读取器
// ============================================================================
class VXLClass
{
public:
    static constexpr int32 MaxSections = 64;
    static constexpr int32 MaxRecords  = 65536;

    VXLClass();
    VXLClass(const char* pFilename);
    ~VXLClass();

    VXLClass(const VXLClass&) = delete;
    VXLClass& operator=(const VXLClass&) = delete;

    // 载入（先 MIX 后散装），失败时对象保持空状态
    bool LoadFromFile(const char* pFilename);
    bool IsLoaded() const { return m_pRawData != nullptr && m_pHeader != nullptr; }

    // ---- section ----
    int32       GetSectionCount() const { return m_SectionCount; }
    const char* GetSectionName(int32 sectionIndex) const;
    bool        HasSection(int32 sectionIndex) const;
    int32       GetFrameCount(int32 sectionIndex) const;

    // ---- record ----
    // frame 越界时按帧数取模；section 非法或文件损坏返回 false。
    bool GetRecord(int32 sectionIndex, int32 frame, VXLRecordView& out) const;

    // ---- 体素 ----
    // 读取 (x,y,z) 处的原始 uint32；空体素或越界返回 0xFFFFFFFF。
    uint32 GetVoxelRaw(int32 sectionIndex, int32 frame,
                       int32 x, int32 y, int32 z) const;

    // 兼容旧接口：返回调色板索引（0 = 空），frame 固定为 0
    BYTE GetVoxel(int32 sectionIndex, int32 x, int32 y, int32 z) const;

    // 兼容旧接口：返回 frame 0 的体素索引包围盒（闭区间）
    void GetBounds(int32 sectionIndex, int32& minX, int32& maxX,
                   int32& minY, int32& maxY, int32& minZ, int32& maxZ) const;

    // 模型空间 AABB（来自 record 的 Corner0/Corner1）
    bool GetWorldBounds(int32 sectionIndex, int32 frame,
                        float outMin[3], float outMax[3]) const;

    // 诊断：返回失败原因（载入失败时可用）
    const char* GetLastError() const { return m_LastError; }

private:
    void Reset();
    bool Parse();

    BYTE*  m_pRawData;
    int32  m_RawDataSize;
    bool   m_bOwnsData;

    const VXLFileHeader*   m_pHeader;
    const VXLSectionEntry* m_pSections;
    const VXLRecordRaw*    m_pRecords;
    const BYTE*            m_pBlob;

    int32  m_SectionCount;
    int32  m_RecordCount;
    int32  m_BlobSize;

    const char* m_LastError;
};
