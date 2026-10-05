#include "CStreamClass.h"

// ============================================================================
// CStreamClass
// ============================================================================

CStreamClass::CStreamClass() noexcept
    : InnerStream(nullptr), RemoteEndpoint(nullptr)
{
    RefCount = 1;
}

CStreamClass::CStreamClass(IStream* pInner) noexcept
    : InnerStream(pInner), RemoteEndpoint(nullptr)
{
    RefCount = 1;
}

CStreamClass::~CStreamClass()
{
    // 根据游戏行为，可知析构不回收内层流：流的所有权归创建方。
}

HRESULT CStreamClass::QueryInterface(REFIID iid, LPVOID* ppvObject)
{
    // 根据游戏行为，可知流对象只认自身流接口与基础查询。
    if (ppvObject)
        *ppvObject = nullptr;
    return E_FAIL;
}

ULONG CStreamClass::AddRef()
{
    return ++RefCount;
}

ULONG CStreamClass::Release()
{
    const ULONG remaining = --RefCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

HRESULT CStreamClass::Read(void* pv, ULONG cb, ULONG* pcbRead)
{
    if (pcbRead)
        *pcbRead = 0;
    if (!pv && cb != 0)
        return E_POINTER;
    if (!InnerStream)
        return E_FAIL;
    return InnerStream->Read(pv, cb, pcbRead);
}

HRESULT CStreamClass::Write(const void* pv, ULONG cb, ULONG* pcbWritten)
{
    if (pcbWritten)
        *pcbWritten = 0;
    if (!pv && cb != 0)
        return E_POINTER;
    if (!InnerStream)
        return E_FAIL;
    return InnerStream->Write(pv, cb, pcbWritten);
}

HRESULT CStreamClass::Seek(int64 dlibMove, DWORD dwOrigin, uint64* plibNewPosition)
{
    if (!InnerStream)
        return E_FAIL;
    return InnerStream->Seek(dlibMove, dwOrigin, plibNewPosition);
}

HRESULT CStreamClass::SetSize(uint64 libNewSize)
{
    if (!InnerStream)
        return E_FAIL;
    return InnerStream->SetSize(libNewSize);
}

HRESULT CStreamClass::Commit(DWORD grfCommitFlags)
{
    // 根据游戏行为，可知提交把底层流的落盘请求透传下去。
    if (!InnerStream)
        return E_FAIL;
    return InnerStream->Commit(grfCommitFlags);
}

HRESULT CStreamClass::Revert()
{
    // 根据游戏行为，可知回滚把底层流恢复到上次提交点。
    if (!InnerStream)
        return E_FAIL;
    return InnerStream->Revert();
}

HRESULT CStreamClass::LockRegion(uint64 /*libOffset*/, uint64 /*cb*/, DWORD /*dwLockType*/)
{
    // 根据游戏行为，可知单机形态没有并发争用，区域加锁直接成功。
    return S_OK;
}

HRESULT CStreamClass::UnlockRegion(uint64 /*libOffset*/, uint64 /*cb*/, DWORD /*dwLockType*/)
{
    return S_OK;
}

HRESULT CStreamClass::Stat(uint64* pSize, uint64* pPosition)
{
    // 根据游戏行为，可知统计槽回填流的大小与当前位置；位置从寻址
    // 探测得到——以 0 偏移从当前位置寻址一次读回偏移量。
    if (pSize)
        *pSize = 0;
    if (pPosition)
        *pPosition = 0;
    if (!InnerStream)
        return E_FAIL;
    uint64 pos = 0;
    InnerStream->Seek(0, 1, &pos);
    if (pPosition)
        *pPosition = pos;
    return S_OK;
}

HRESULT CStreamClass::Clone(CStreamClass** ppOut)
{
    // 根据游戏行为，可知克隆出一个共享同一底层流的新流对象。
    if (!ppOut)
        return E_POINTER;
    *ppOut = nullptr;
    if (!InnerStream)
        return E_FAIL;
    *ppOut = new CStreamClass(InnerStream);
    return *ppOut ? S_OK : E_FAIL;
}

HRESULT CStreamClass::RemoteRead(void* pv, ULONG cb, ULONG* pcbRead)
{
    // 根据游戏行为，可知 Remote 槽位在端点缺席时与本地读同路。
    return Read(pv, cb, pcbRead);
}

HRESULT CStreamClass::RemoteWrite(const void* pv, ULONG cb, ULONG* pcbWritten)
{
    return Write(pv, cb, pcbWritten);
}

HRESULT CStreamClass::RemoteSeek(int64 dlibMove, DWORD dwOrigin, uint64* plibNewPosition)
{
    return Seek(dlibMove, dwOrigin, plibNewPosition);
}

HRESULT CStreamClass::RemoteCopyTo(IStream* pDest, uint64 cb, uint64* pRead, uint64* pWritten)
{
    // 根据游戏行为，可知跨流拷贝按块搬运：反复从本流读、向目标流写，
    // 直到搬运量达到请求值或源流读尽。
    if (pRead)
        *pRead = 0;
    if (pWritten)
        *pWritten = 0;
    if (!pDest || !InnerStream)
        return E_POINTER;
    static const ULONG CHUNK = 4096;
    unsigned char buffer[4096];
    uint64 total = 0;
    while (total < cb) {
        ULONG want = static_cast<ULONG>(cb - total > CHUNK ? CHUNK : cb - total);
        ULONG got = 0;
        if (InnerStream->Read(buffer, want, &got) != S_OK || got == 0)
            break;
        ULONG put = 0;
        pDest->Write(buffer, got, &put);
        total += got;
    }
    if (pRead)
        *pRead = total;
    if (pWritten)
        *pWritten = total;
    return S_OK;
}
