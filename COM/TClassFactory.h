#pragma once

#include "../COM/IUnknown.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// TClassFactory - COM class factory
//
//  根据游戏行为，可知原版用一套模板化类工厂承载各可实例化类型的
//  COM 面板：CreateInstance 造出对象并转发接口查询，LockServer 把
//  工厂锁在内存里，引用计数三槽转发到工厂自身。重构按两类可实例化
//  类型（超武型表、网络流）给出两组槽位。
// ============================================================================

class TClassFactory {
public:
    TClassFactory() noexcept;
    virtual ~TClassFactory();

    // ------------------------------------------------------------------------
    // asm: TClassFactory_SuperWeaponTypeClass_CTOR (0x5C0B40)
    //   mov dword ptr [eax+4], 0     ; +4 槽清零
    //   mov dword ptr [eax], vt      ; TClassFactory<SuperWeaponTypeClass>::vftable
    //   mov eax, esi / retn
    //
    // 降级点：模板特化的 vtable 在 C++ 里由编译器给出，无法手写；本类唯一的
    // +4 槽是 RefCount，故只保留"清零"这一步。
    // ------------------------------------------------------------------------
    void Construct_SuperWeaponTypeFactory() noexcept { RefCount = 0; }

    // IUnknown（工厂自身）
    HRESULT QueryInterface(REFIID iid, LPVOID* ppvObject);
    ULONG AddRef();
    ULONG Release();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知工厂的实例面：创建即构造新对象并交出请求的
    // 接口；服务器锁在单机形态下直接成功。
    // ------------------------------------------------------------------------

    // 超武型表工厂槽位
    HRESULT SuperWeaponTypeClass_QueryInterface(REFIID iid, LPVOID* ppvObject);
    ULONG SuperWeaponTypeClass_AddRef();
    ULONG SuperWeaponTypeClass_Release();
    HRESULT SuperWeaponTypeClass_CreateInstance(LPVOID pUnkOuter, REFIID iid, LPVOID* ppvObject);
    HRESULT SuperWeaponTypeClass_LockServer(BOOL fLock);

    // 网络流工厂槽位
    HRESULT CStreamClass_QueryInterface(REFIID iid, LPVOID* ppvObject);
    ULONG CStreamClass_AddRef();
    ULONG CStreamClass_Release();
    HRESULT CStreamClass_CreateInstance(LPVOID pUnkOuter, REFIID iid, LPVOID* ppvObject);
    HRESULT CStreamClass_LockServer(BOOL fLock);

    ULONG RefCount;
};
