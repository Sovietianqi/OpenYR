#include "FileFormats/TMP.h"

#include "IO/CCFileClass.h"

#include <cstring>
#include <new>
#include <vector>

namespace
{
// 每格最多 3 段 raster，帧号 = cellIndex * 3 + rasterIndex。
constexpr int32 RastersPerCell = 3;

inline int32 ReadS32(const uint8* p)
{
    int32 v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

struct CellBuild
{
    int32  CellIndex;
    int32  CanvasTop;      // frame.Top（已做画布向上溢出补偿）
    int32  CanvasLeft;
    int32  RasterW[RastersPerCell];
    int32  RasterH[RastersPerCell];
    const uint8* pRaster[RastersPerCell];
};
}  // namespace

// ----------------------------------------------------------------------------
// TMPFile::Load
//
// asm sub_547020 的逐句对应：
//   FileSize -> new(bufSize) -> ReadNextBytes
//   cl = [buf+0]; [this+2E4h] = cl          // 列
//   cl = [buf+4]; [this+2E8h] = cl          // 行
//   ecx = buf + 0x10; loop (列*行) 次 { if (0 < *ecx < size) *ecx += buf; ecx += 4 }
// ----------------------------------------------------------------------------
SHPStruct* TMPFile::Load(const uint8* pData, int32 dataSize, int32* pOutCellCount)
{
    if (pOutCellCount != nullptr)
        *pOutCellCount = 0;
    if (pData == nullptr || dataSize < (int32)sizeof(TMPFileHeader) + 4)
        return nullptr;

    TMPFileHeader header;
    std::memcpy(&header, pData, sizeof(header));

    const int32 cellsX = header.Width;
    const int32 cellsY = header.Height;
    if (cellsX <= 0 || cellsY <= 0 || cellsX > 64 || cellsY > 64)
        return nullptr;

    const int32 cellCount = cellsX * cellsY;
    const int32 tableOffset = (int32)sizeof(TMPFileHeader);
    if (dataSize < tableOffset + cellCount * 4)
        return nullptr;

    const int32 canvasW = (header.CellWidth  > 0) ? header.CellWidth  : 60;
    const int32 canvasH = (header.CellHeight > 0) ? header.CellHeight : 30;

    // ---- 第 1 遍：定位每个 cell 的 raster 指针，求出画布向上溢出的量 --------
    std::vector<CellBuild> builds(static_cast<size_t>(cellCount));
    int32 minTop = 0;
    int32 maxBottom = canvasH;
    int32 maxRight = canvasW;
    int32 framedCells = 0;

    for (int32 i = 0; i < cellCount; ++i)
    {
        CellBuild& b = builds[static_cast<size_t>(i)];
        b.CellIndex = i;
        b.CanvasTop = 0;
        b.CanvasLeft = 0;
        for (int32 r = 0; r < RastersPerCell; ++r)
        {
            b.RasterW[r] = 0;
            b.RasterH[r] = 0;
            b.pRaster[r] = nullptr;
        }

        const int32 rawOffset = ReadS32(pData + tableOffset + i * 4);
        // 注意用减法而不是 rawOffset + sizeof(...)：后者在 rawOffset 接近
        // INT32_MAX 时会溢出成负数，把越界值放进来（会直接读崩）。
        if (rawOffset <= 0 || rawOffset > dataSize - (int32)sizeof(TMPCellHeader))
            continue;

        TMPCellHeader cell;
        std::memcpy(&cell, pData + rawOffset, sizeof(cell));

        // cell 数据紧跟 52 字节头：可选的菱形高度图，然后 3 段 raster。
        int32 cursor = rawOffset + (int32)sizeof(TMPCellHeader);
        if (cell.ExtraWidth != 0 || cell.ExtraHeight != 0)
        {
            // ExtraWidth/ExtraHeight 是文件里的 dword，可能是任意值；
            // 先钳到剩余空间内再相乘，避免 mapW*mapH 溢出。
            const int32 remaining = dataSize - cursor;
            if (cell.ExtraWidth < 0 || cell.ExtraHeight < 0 || remaining <= 0)
                continue;
            const int32 mapW = cell.ExtraWidth + 1;
            const int32 mapH = cell.ExtraHeight + 1;
            if (mapW <= 0 || mapH <= 0)
                continue;
            // 高度图的字节数必须落在剩余空间内：既防越界，也防 mapW*mapH 溢出。
            if (mapW > remaining || mapH > remaining / mapW)
                continue;
            cursor += mapW * mapH;
        }

        bool any = false;
        for (int32 r = 0; r < RastersPerCell; ++r)
        {
            const int32 w = cell.Raster[r].Width;
            const int32 h = cell.Raster[r].Height;
            const int32 bytes = w * h;
            if (w <= 0 || h <= 0 || bytes <= 0
                || cursor < 0 || cursor > dataSize - bytes)
            {
                cursor += (bytes > 0) ? bytes : 0;
                continue;
            }
            b.RasterW[r] = w;
            b.RasterH[r] = h;
            b.pRaster[r] = pData + cursor;
            cursor += bytes;
            any = true;
        }

        if (!any)
            continue;

        ++framedCells;
        b.CanvasLeft = cell.X;
        b.CanvasTop  = cell.Y;
        if (cell.Y < minTop)
            minTop = cell.Y;
        const int32 bottom = cell.Y + b.RasterH[0];
        if (bottom > maxBottom)
            maxBottom = bottom;
        const int32 right = cell.X + b.RasterW[0];
        if (right > maxRight)
            maxRight = right;
    }

    if (framedCells == 0)
        return nullptr;

    // 画布：原版居中锚点用的是「cell 画布」而不是单帧尺寸。有向上溢出
    // （Y 为负，坡/悬崖）时把画布整体下移，帧的 Top 也同步加回这个量。
    const int32 topPad = (minTop < 0) ? -minTop : 0;
    const int32 finalW  = (maxRight > canvasW) ? maxRight : canvasW;
    const int32 finalH  = maxBottom + topPad;
    if (finalW > 0x7FFF || finalH > 0x7FFF)
        return nullptr;

    // ---- 第 2 遍：算像素总量，一次分配 block ------------------------------
    int32 pixelBytes = 0;
    for (int32 i = 0; i < cellCount; ++i)
    {
        const CellBuild& b = builds[static_cast<size_t>(i)];
        for (int32 r = 0; r < RastersPerCell; ++r)
        {
            if (b.pRaster[r] != nullptr)
                pixelBytes += b.RasterW[r] * b.RasterH[r];
        }
    }

    const int32 frameCount = cellCount * RastersPerCell;
    const size_t framesBytes = sizeof(SHPFile)
                             + sizeof(SHPFrame) * static_cast<size_t>(frameCount - 1);
    const size_t total = framesBytes + static_cast<size_t>(pixelBytes);

    uint8* pBlock = static_cast<uint8*>(::operator new(total));
    if (pBlock == nullptr)
        return nullptr;
    std::memset(pBlock, 0, total);

    SHPFile* pFile = new (pBlock) SHPFile();
    pFile->Type   = 0;
    pFile->Width  = static_cast<int16>(finalW);
    pFile->Height = static_cast<int16>(finalH);
    pFile->Frames = static_cast<int16>(frameCount);

    // ---- 第 3 遍：填帧头 + 拷像素 ----------------------------------------
    uint8* pDst = pBlock + framesBytes;
    for (int32 i = 0; i < cellCount; ++i)
    {
        const CellBuild& b = builds[static_cast<size_t>(i)];
        for (int32 r = 0; r < RastersPerCell; ++r)
        {
            SHPFrame& frame = const_cast<SHPFrame&>(
                pFile->GetFrameHeader(i * RastersPerCell + r));

            if (b.pRaster[r] == nullptr)
            {
                frame.Left = frame.Top = frame.Width = frame.Height = 0;
                frame.Flags = 0;
                frame.Offset = 0;
                continue;
            }

            const int32 bytes = b.RasterW[r] * b.RasterH[r];
            frame.Left   = static_cast<int16>(b.CanvasLeft);
            frame.Top    = static_cast<int16>(b.CanvasTop + topPad);
            frame.Width  = static_cast<int16>(b.RasterW[r]);
            frame.Height = static_cast<int16>(b.RasterH[r]);
            frame.Flags  = 0;   // 未压缩：CC_Draw_Shape 走 plain blitter
            frame.Offset = static_cast<int32>(pDst - pBlock);
            std::memcpy(pDst, b.pRaster[r], static_cast<size_t>(bytes));
            pDst += bytes;
        }
    }

    if (pOutCellCount != nullptr)
        *pOutCellCount = cellCount;
    return pFile;
}

// ----------------------------------------------------------------------------
// TMPFile::LoadFromFile
// ----------------------------------------------------------------------------
SHPStruct* TMPFile::LoadFromFile(const char* pFileName, int32* pOutCellCount)
{
    if (pOutCellCount != nullptr)
        *pOutCellCount = 0;
    if (pFileName == nullptr || pFileName[0] == '\0')
        return nullptr;

    CCFileClass file;
    if (!file.Open(pFileName, static_cast<int32>(FileAccessMode::Read)))
        return nullptr;

    const int32 size = file.FileSize();
    if (size <= 0)
        return nullptr;

    uint8* pRaw = new uint8[static_cast<size_t>(size)];
    const int32 got = file.ReadNextBytes(pRaw, size);
    file.Close();

    if (got <= 0)
    {
        delete[] pRaw;
        return nullptr;
    }

    SHPStruct* pShape = TMPFile::Load(pRaw, got, pOutCellCount);
    delete[] pRaw;
    return pShape;
}

// ----------------------------------------------------------------------------
int32 TMPFile::GetCanvasHeight(const SHPStruct* pShape)
{
    return (pShape != nullptr) ? pShape->Height : 0;
}
