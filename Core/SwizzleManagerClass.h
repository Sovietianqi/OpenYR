#pragma once

#include "Definitions.h"
#include "Macros.h"
#include "Memory.h"

class AbstractClass;

// ------------------------------------------------------------------------
// 根据游戏行为，可知指针重整面在存档还原时按"先登记、后回填"
// 两步把裸指针重新挂回活动对象。
// ------------------------------------------------------------------------
class SwizzleManagerClass {
public:
    SwizzleManagerClass() noexcept;

    void Here_I_Am(int32 saveID, AbstractClass* pPointer);
    void Swizzle(int32 saveID, void** ppPointer);

    // asm: SwizzleManagerClass_FetchSwizzleID (0x6CF490) —— __thiscall，
    //   retn 0Ch（this + 2 个 dword 形参）。名字虽叫 FetchSwizzleID，函数体
    //   实际是 IUnknown 风格的"把入口指针原样回填"：
    //       if (!pIn || !ppOut) return E_POINTER;   // 0x80004003
    //       *ppOut = pIn;       return S_OK;
    HRESULT FetchSwizzleID(void* pIn, void** ppOut);

    int32 GetSaveSize() const;

    int32 RequestCount;
    int32 ResolvedCount;
};
