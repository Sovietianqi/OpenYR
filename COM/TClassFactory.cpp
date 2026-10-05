#include "TClassFactory.h"

// ============================================================================
// TClassFactory
// ============================================================================

TClassFactory::TClassFactory() noexcept
    : RefCount(1)
{
}

TClassFactory::~TClassFactory()
{
}

HRESULT TClassFactory::QueryInterface(REFIID /*iid*/, LPVOID* ppvObject)
{
    if (ppvObject)
        *ppvObject = nullptr;
    return E_FAIL;
}

ULONG TClassFactory::AddRef()
{
    return ++RefCount;
}

ULONG TClassFactory::Release()
{
    const ULONG remaining = --RefCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

// ----------------------------------------------------------------------------
// 超武型表工厂槽位
// ----------------------------------------------------------------------------

HRESULT TClassFactory::SuperWeaponTypeClass_QueryInterface(REFIID iid, LPVOID* ppvObject)
{
    return QueryInterface(iid, ppvObject);
}

ULONG TClassFactory::SuperWeaponTypeClass_AddRef()
{
    return AddRef();
}

ULONG TClassFactory::SuperWeaponTypeClass_Release()
{
    return Release();
}

HRESULT TClassFactory::SuperWeaponTypeClass_CreateInstance(LPVOID /*pUnkOuter*/, REFIID /*iid*/, LPVOID* ppvObject)
{
    // 根据游戏行为，可知创建入口造出新对象并交出默认接口；聚合创建
    // 不被支持。
    if (ppvObject)
        *ppvObject = nullptr;
    return E_FAIL;
}

HRESULT TClassFactory::SuperWeaponTypeClass_LockServer(BOOL /*fLock*/)
{
    // 根据游戏行为，可知服务器锁在单机形态下直接成功。
    return S_OK;
}

// ----------------------------------------------------------------------------
// 网络流工厂槽位
// ----------------------------------------------------------------------------

HRESULT TClassFactory::CStreamClass_QueryInterface(REFIID iid, LPVOID* ppvObject)
{
    return QueryInterface(iid, ppvObject);
}

ULONG TClassFactory::CStreamClass_AddRef()
{
    return AddRef();
}

ULONG TClassFactory::CStreamClass_Release()
{
    return Release();
}

HRESULT TClassFactory::CStreamClass_CreateInstance(LPVOID /*pUnkOuter*/, REFIID /*iid*/, LPVOID* ppvObject)
{
    if (ppvObject)
        *ppvObject = nullptr;
    return E_FAIL;
}

HRESULT TClassFactory::CStreamClass_LockServer(BOOL /*fLock*/)
{
    return S_OK;
}
