#include "SwizzleManagerClass.h"
#include "../Abstract/AbstractClass.h"

SwizzleManagerClass::SwizzleManagerClass() noexcept
    : RequestCount(0)
    , ResolvedCount(0)
{
}

void SwizzleManagerClass::Here_I_Am(int32 saveID, AbstractClass* pPointer)
{
    (void)saveID;
    (void)pPointer;
    ++ResolvedCount;
}

void SwizzleManagerClass::Swizzle(int32 saveID, void** ppPointer)
{
    (void)saveID;
    (void)ppPointer;
    ++RequestCount;
}

int32 SwizzleManagerClass::FetchSwizzleID(AbstractClass* pPointer) const
{
    (void)pPointer;
    return 0;
}

int32 SwizzleManagerClass::GetSaveSize() const
{
    return RequestCount * 8;
}
