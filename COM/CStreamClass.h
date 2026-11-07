#pragma once

#include "../COM/IUnknown.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// CStreamClass - block-framed, LZO-compressed IStream
//
//   The retail object is a compressing pass-through over an inner IStream: the
//   read slots pull 8-byte block headers and LZO-compressed payloads out of the
//   inner stream and hand callers the decompressed bytes, and the write slots
//   accumulate 64 KB blocks, LZO-compress them and push [size][data] frames
//   down.  Three 64 KB scratch buffers back the codec.
//
//   Field map (asm, this-relative):
//     [this+8]  InnerStream          inner IStream
//     [this+0Ch] RefCount            (per-object; a global tally also exists)
//     [this+10h] Reading             read lock
//     [this+11h] Writing             write lock
//     [this+14h] BufferFill          decompressed bytes still pending in BufferA
//     [this+18h] BufferA             decompressed output / compress input
//     [this+1Ch] BufferB             compressed scratch
//     [this+20h] BufferC             compressor work / dictionary
//     [this+24h] BlockHeader[0]      compressed block length
//     [this+28h] BlockHeader[1]      uncompressed block length
// ============================================================================

class CStreamClass : public IStream {
public:
    CStreamClass() noexcept;
    explicit CStreamClass(IStream* pInner) noexcept;
    virtual ~CStreamClass() override;

    // IUnknown
    virtual HRESULT QueryInterface(REFIID iid, LPVOID* ppvObject) override;
    virtual ULONG AddRef() override;
    virtual ULONG Release() override;

    // IStream - the block codec (asm: CStreamClass_RemoteRead / _RemoteWrite)
    virtual HRESULT Read(void* pv, ULONG cb, ULONG* pcbRead) override;
    virtual HRESULT Write(const void* pv, ULONG cb, ULONG* pcbWritten) override;
    virtual HRESULT Seek(int64 dlibMove, DWORD dwOrigin, uint64* plibNewPosition) override;
    virtual HRESULT SetSize(uint64 libNewSize) override;
    virtual HRESULT Commit(DWORD grfCommitFlags) override;
    virtual HRESULT Revert() override;

    // ------------------------------------------------------------------------
    // Optional COM IStream slots.  The retail binary forwards each to the same
    // slot on the inner stream; the standalone IStream models only the core
    // read/write/seek surface, so the remaining slots keep the asm's guard
    // structure and fall back to a local equivalent.
    // ------------------------------------------------------------------------
    HRESULT LockRegion(uint64 libOffset, uint64 cb, DWORD dwLockType);
    HRESULT UnlockRegion(uint64 libOffset, uint64 cb, DWORD dwLockType);
    HRESULT Stat(uint64* pSize, uint64* pPosition);
    HRESULT Clone(CStreamClass** ppOut);

    // ------------------------------------------------------------------------
    // Remote-prefixed aliases: the retail vtable entries the IDA export names
    // Remote* are exactly the codec slots above.  Kept so callers that use the
    // original names still map onto the same code paths.
    // ------------------------------------------------------------------------
    HRESULT RemoteRead(void* pv, ULONG cb, ULONG* pcbRead);
    HRESULT RemoteWrite(const void* pv, ULONG cb, ULONG* pcbWritten);
    HRESULT RemoteSeek(int64 dlibMove, DWORD dwOrigin, uint64* plibNewPosition);
    HRESULT RemoteCopyTo(IStream* pDest, uint64 cb, uint64* pRead, uint64* pWritten);

    // Block codec primitives (asm: lzo1x_1_compress / lzo1x_decompress).
    static constexpr int32 BlockSize = 0x10000;

    // Compress one block; returns the compressed byte count (never <= 0).
    int32 CompressBlock(const uint8* pIn, int32 nLen, uint8* pOut);
    // Decompress one block; `pOutLen` in/out (capacity in, bytes out).
    int32 DecompressBlock(const uint8* pIn, int32 nLen, uint8* pOut, int32* pOutLen);

    // Emit one [8-byte header][compressed data] frame to the inner stream.
    HRESULT FlushBlock(const uint8* pData, int32 nLen);

    IStream* InnerStream;    // [this+8]
    uint8*   BufferA;        // [this+18h]
    uint8*   BufferB;        // [this+1Ch]
    uint8*   BufferC;        // [this+20h]
    int32    BufferFill;     // [this+14h]
    uint8    BlockHeader[8]; // [this+24h] / [this+28h]
    int32    BlockBytes;     // [this+28h]
    bool     Reading;        // [this+10h]
    bool     Writing;        // [this+11h]
};

// ============================================================================
// LZO1X block codec entry points (asm: lzo1x_1_compress / lzo1x_decompress)
//
//   lzo1x_decompress 是原版的裸指针端口（上层 Straw 负责截断，故本身无边界）。
//   lzo1x_decompress_safe 保持同一套 token 语义，额外检查 ip / op 余量，
//   用于读外部数据文件（如地图 [IsoMapPack5]）这种不可信输入；越界返回 -1。
// ============================================================================
int32 lzo1x_1_compress(const uint8* pIn, int32 inLen, uint8* pOut,
                       int32* pOutLen, void* pDict);
int32 lzo1x_decompress(const uint8* pIn, int32 inLen,
                       uint8* pDest, int32* pOutLen);
int32 lzo1x_decompress_safe(const uint8* pIn, int32 inLen,
                            uint8* pDest, int32 outCap, int32* pOutLen);
