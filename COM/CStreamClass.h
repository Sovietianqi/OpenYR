#pragma once

#include "../COM/IUnknown.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// CStreamClass - network-backed COM stream
//
//  根据游戏行为，可知原版在存档/读档走网络转发时把 IStream 包了一层：
//  本地操作直接落到内层流，带 Remote 前缀的槽位把读写请求转交给远端
//  端点后再回填结果。重构以内嵌 IStream 指针承载这层转发。
// ============================================================================

class CStreamClass : public IStream {
public:
    CStreamClass() noexcept;
    explicit CStreamClass(IStream* pInner) noexcept;
    virtual ~CStreamClass();

    // IUnknown
    virtual HRESULT QueryInterface(REFIID iid, LPVOID* ppvObject) override;
    virtual ULONG AddRef() override;
    virtual ULONG Release() override;

    // IStream
    virtual HRESULT Read(void* pv, ULONG cb, ULONG* pcbRead) override;
    virtual HRESULT Write(const void* pv, ULONG cb, ULONG* pcbWritten) override;
    virtual HRESULT Seek(int64 dlibMove, DWORD dwOrigin, uint64* plibNewPosition) override;
    virtual HRESULT SetSize(uint64 libNewSize) override;
    virtual HRESULT Commit(DWORD grfCommitFlags) override;
    virtual HRESULT Revert() override;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知流的可选槽位：区域加锁/解锁在单机形态下直接
    // 成功；统计槽回填流的大小与读写位置；克隆复制出一个共享同一底层
    // 流的新对象。
    // ------------------------------------------------------------------------
    HRESULT LockRegion(uint64 libOffset, uint64 cb, DWORD dwLockType);
    HRESULT UnlockRegion(uint64 libOffset, uint64 cb, DWORD dwLockType);
    HRESULT Stat(uint64* pSize, uint64* pPosition);
    HRESULT Clone(CStreamClass** ppOut);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 Remote 形态是网络转发槽位：请求先登记到转发
    // 端点，端点缺席时回落到本地流。
    // ------------------------------------------------------------------------
    HRESULT RemoteRead(void* pv, ULONG cb, ULONG* pcbRead);
    HRESULT RemoteWrite(const void* pv, ULONG cb, ULONG* pcbWritten);
    HRESULT RemoteSeek(int64 dlibMove, DWORD dwOrigin, uint64* plibNewPosition);
    HRESULT RemoteCopyTo(IStream* pDest, uint64 cb, uint64* pRead, uint64* pWritten);

    // 底层流：本地直读直写，Remote 槽位在无端点时回落到这里。
    IStream* InnerStream;
    // 转发端点：非空时 Remote 槽位走它。
    void* RemoteEndpoint;
};
