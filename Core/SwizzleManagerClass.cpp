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

// asm: SwizzleManagerClass_FetchSwizzleID (0x6CF490)
//
//   mov ecx, [esp+arg_4]      ; pIn
//   test ecx, ecx / jz  fail
//   mov eax, [esp+arg_8]      ; ppOut
//   test eax, eax / jz  fail
//   mov [eax], ecx            ; *ppOut = pIn
//   xor eax, eax / retn 0Ch   ; S_OK
// fail:
//   mov eax, 80004003h / retn 0Ch
//
// 函数体不读 this，行为就是把传入的接口指针原样交还。
HRESULT SwizzleManagerClass::FetchSwizzleID(void* pIn, void** ppOut)
{
    if (pIn == nullptr || ppOut == nullptr)
    {
        return static_cast<HRESULT>(0x80004003);   // E_POINTER
    }

    *ppOut = pIn;
    return 0;
}

int32 SwizzleManagerClass::GetSaveSize() const
{
    return RequestCount * 8;
}
