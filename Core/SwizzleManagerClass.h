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
    int32 FetchSwizzleID(AbstractClass* pPointer) const;
    int32 GetSaveSize() const;

    int32 RequestCount;
    int32 ResolvedCount;
};
