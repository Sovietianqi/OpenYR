#pragma once

#include "Definitions.h"

#define DECLARE_PROPERTY(type, name) type name

#define PROTECTED_PROPERTY(type, name) type name

#define R0 return 0
#define RX {}
#define RT(type) return static_cast<type>(0)
#define JMP_THIS(addr) {}
#define JMP_STD(addr) {}
#define THISCALL_EX(x, addr) {}

#define ABSTRACTTYPE_ARRAY(class_name, address) \
public: \
    static DynamicVectorClass<class_name*>* Array;

#define NOVTABLE

#define _strcmpi strcasecmp

#define sprintf_s snprintf

#define __stdcall
#define STDMETHODCALLTYPE __stdcall
#define STDMETHODIMP __stdcall
#define __declspec(x)
#define __fastcall
#define __forceinline inline
#define FORCEINLINE inline

#define HRESULT int32
#define S_OK 0
#define E_FAIL -1
#define E_POINTER -2
#define E_NOTIMPL -3
#define E_NOINTERFACE -4

#define VARIANT_BOOL int32
#define VARIANT_TRUE 1
#define VARIANT_FALSE 0

#define CONSTEXPR constexpr

// =============================================================================
// Disable copy/move
// =============================================================================

#define DISABLE_COPY(Class) \
    Class(const Class&) = delete; \
    Class& operator=(const Class&) = delete;

#define DISABLE_MOVE(Class) \
    Class(Class&&) = delete; \
    Class& operator=(Class&&) = delete;

#define DISABLE_COPY_AND_MOVE(Class) \
    DISABLE_COPY(Class); \
    DISABLE_MOVE(Class)

// =============================================================================
// Safe delete
// =============================================================================

#define SAFE_DELETE(p)  do { if (p) { delete (p); (p) = nullptr; } } while(0)
#define SAFE_DELETE_ARRAY(p) do { if (p) { delete[] (p); (p) = nullptr; } } while(0)

#define GameCreateArray(T, n) static_cast<T*>(YRMemory::Allocate(sizeof(T) * (n)))
#define GameDeleteArray(p) YRMemory::Deallocate(p)