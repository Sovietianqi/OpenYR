#pragma once

#include "Core/Definitions.h"
#include "FileFormats/SHP.h"

// ============================================================================
// TMP - Westwood isometric tile format (.TEM / .SNO / .URB / .DES / .LUN)
//
// 布局全部取自 asm（gamemd.exe）：
//
//   sub_547020（IsometricTileTypeClass 的 lazy 装图入口）
//     - 整文件读进 [+0A4h]
//     - [buf+0]  -> [+2E4h]   cell 列数
//     - [buf+4]  -> [+2E8h]   cell 行数
//     - 循环起点 buf+0x10，步长 4，计数 (列*行)：
//         若 0 < dword < FileSize 则 dword += buf 基址
//       即 +0x10 是 **cell 偏移表**，装载时原地重定位成绝对指针。
//       偏移为 0 表示该格为空。
//
//   sub_549E90（预计算每格色表）
//     - 每个非空 cell：new 0x34 字节
//     - 从 [cell+2Bh..2Dh] 取第一组 RGB，[cell+2Eh..30h] 取第二组 RGB
//       -> 证实 cell 头里 +0x2B..+0x30 是两组雷达/平均色。
//
//   由此反推 cell 头 = 0x34 = 52 字节：
//     +00 int32 X, +04 Y, +08 ExtraX, +0C ExtraY,
//     +10 ExtraWidth, +14 ExtraHeight, +18 Flags,
//     +1C uint8 Height, +1D RampType, +1E TerrainType,
//     +1F Raster[0].X/Y/W/H, +23 Raster[1], +27 Raster[2]   (各 4 字节)
//     +2B Color1.R/G/B, +2E Color2.R/G/B                     (与 asm 吻合)
//     +31 RampHeight, +32 RadarLeft, +33 RadarRight
//
// cell 数据紧随 52 字节头之后：
//     [ExtraWidth>0 || ExtraHeight>0] ? (ExtraWidth+1)*(ExtraHeight+1) 字节高度图
//     然后 Raster[0..2] 依次各 W*H 字节（8 位调色板索引，0 = 透明）。
// ============================================================================

#pragma pack(push, 1)

struct TMPFileHeader
{
    int32 Width;        // +00  格数（列）
    int32 Height;       // +04  格数（行）
    int32 CellWidth;    // +08  单格像素宽（60）
    int32 CellHeight;   // +0C  单格像素高（30）
};

struct TMPCellRaster
{
    uint8 X;
    uint8 Y;
    uint8 Width;
    uint8 Height;
};

struct TMPCellHeader
{
    int32   X;              // +00  相对画布左上角的像素偏移（可为负）
    int32   Y;              // +04
    int32   ExtraX;         // +08
    int32   ExtraY;         // +0C
    int32   ExtraWidth;     // +10
    int32   ExtraHeight;    // +14
    int32   Flags;          // +18
    uint8   Height;         // +1C  菱形高度（0..15）
    uint8   RampType;       // +1D
    uint8   TerrainType;    // +1E
    TMPCellRaster Raster[3];// +1F  各 4 字节，共 12
    uint8   Color1R;        // +2B  asm [cell+2Bh]
    uint8   Color1G;        // +2C
    uint8   Color1B;        // +2D
    uint8   Color2R;        // +2E  asm [cell+2Eh]
    uint8   Color2G;        // +2F
    uint8   Color2B;        // +30
    uint8   RampHeight;     // +31
    uint8   RadarLeft;      // +32
    uint8   RadarRight;     // +33
};

#pragma pack(pop)

static_assert(sizeof(TMPFileHeader) == 16, "TMPFileHeader must be 16 bytes");
static_assert(sizeof(TMPCellHeader) == 0x34, "TMPCellHeader must be 0x34 bytes");

// ============================================================================
// TMPFile
//
// 把一个 TMP 文件解析成 **SHPStruct**，让 CC_Draw_Shape 能直接消费：
//   * 每个 cell 的每个 raster 对应一帧，帧号 = cellIndex * 3 + rasterIndex
//   * 帧的 Left/Top 取 cell.X / cell.Y（相对 cell 画布左上角）
//   * 帧的像素是未压缩的 8 位索引，Flags bit1 = 0，走 plain blitter
//   * SHPStruct::Width/Height 取 cell 画布（含向上溢出的部分）
// ============================================================================
class TMPFile
{
public:
    // 解析内存中的 TMP 数据。返回的指针由调用方 delete（虚析构已就绪）。
    // 失败返回 nullptr。pOutCellCount 可选，返回实际 cell 数。
    static SHPStruct* Load(const uint8* pData, int32 dataSize,
                           int32* pOutCellCount = nullptr);

    // 从磁盘 / MIX 虚文件系统读。
    static SHPStruct* LoadFromFile(const char* pFileName,
                                   int32* pOutCellCount = nullptr);

    // 该 TMP 每格的画布高度（CellHeight 加上向上溢出的部分）。
    static int32 GetCanvasHeight(const SHPStruct* pShape);
};
