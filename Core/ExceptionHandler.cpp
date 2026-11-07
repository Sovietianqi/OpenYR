// ============================================================================
// Core/ExceptionHandler.cpp
//
// asm 权威：Exception_WriteLog (0x4C85E0) 与 TopLevelExceptionFilter 的日志段
// (loc_4C8FE0 .. loc_4C9058)。逐条对照注释里的 asm 行号/标签。
//
// 原版通过 IMAGEHLP.DLL 的导出符号做符号化回溯：LoadLibraryA 之后用
// GetProcAddress 依次填满 .data 里的 Sym* 槽位（SymFuncNames 表），再调用。
// 本文件保持同一流程：Windows 构建下真的加载 IMAGEHLP.DLL；其余平台没有
// 这个 DLL，于是走 asm 里"Unable to load IMAGEHLP.DLL"的既存分支，日志仍
// 按原样写出（只是拿不到函数名）。
// ============================================================================

#include "Core/ExceptionHandler.h"
#include "Game/Shell.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
    #define OPENYR_EXC_WIN32 1
    #include <windows.h>
#else
    #include <csignal>
    #include <csetjmp>
    #include <ctime>
    #include <unistd.h>
    #if defined(__i386__) || defined(__x86_64__)
        #define OPENYR_EXC_X86 1
        #include <cpuid.h>
    #endif
#endif

// asm: str_except_txt —— 64 KB 累加器（0x10000）
char str_except_txt[0x10000];

namespace {

// ----------------------------------------------------------------------------
// 常量（全部取自 asm 的直接寻址/字面量）
// ----------------------------------------------------------------------------
const char k_ImageHlpName[] = "IMAGEHLP.DLL";           // asm: LibFileName
const int32 k_ExcRecOff      = 0x00;                    // EXCEPTION_POINTERS
const int32 k_CtxPtrOff      = 0x04;
const int32 k_RecCodeOff     = 0x00;                    // EXCEPTION_RECORD
const int32 k_RecInfo0Off    = 0x14;                    // ExceptionInformation[0]
const int32 k_RecInfo1Off    = 0x18;                    // ExceptionInformation[1]
const uint32 k_AccessViolation = 0xC0000005u;           // EXCEPTION_ACCESS_VIOLATION

// CONTEXT 偏移（Win32 x86 CONTEXT 布局）
const int32 k_CtxEdi      = 0x9C;
const int32 k_CtxEsi      = 0xA0;
const int32 k_CtxEbx      = 0xA4;
const int32 k_CtxEdx      = 0xA8;
const int32 k_CtxEcx      = 0xAC;
const int32 k_CtxEax      = 0xB0;
const int32 k_CtxEbp      = 0xB4;
const int32 k_CtxEip      = 0xB8;
const int32 k_CtxSegCs    = 0xBC;
const int32 k_CtxEFlags   = 0xC0;
const int32 k_CtxEsp      = 0xC4;
const int32 k_CtxSegSs    = 0xC8;
const int32 k_CtxSegEs    = 0x94;
const int32 k_CtxSegDs    = 0x98;
const int32 k_CtxSegFs    = 0x90;
const int32 k_CtxSegGs    = 0x8C;

// IMAGEHLP_SYMBOL 布局
const int32 k_SymSizeOfStruct = 0x00;
const int32 k_SymAddress      = 0x04;
const int32 k_SymSize         = 0x08;
const int32 k_SymMaxNameLen   = 0x10;
const int32 k_SymName         = 0x14;

const int32 k_EipDumpLen   = 0x20;      // asm: var_4C4 = 20h
const int32 k_StackDumpLen = 0x400;     // asm: var_4C4 = 400h
const int32 k_MaxAppend    = 0xFFFF;    // asm: cmp ..., 0FFFFh

const int32 k_ErrorStringSize = 0x4A4;  // asm: error_string
const int32 k_ImageNameSize   = 0x104;  // asm: var_204
const int32 k_SymbolBufSize   = 0x100;  // asm: var_384 起的 IMAGEHLP_SYMBOL

// ----------------------------------------------------------------------------
// lpString[21] —— 异常码描述（asm .data，逐字节照抄）
// ----------------------------------------------------------------------------
const char* const k_ErrorStrings[21] = {
    /* 0  EXCEPTION_ACCESS_VIOLATION */
    "Error code: EXCEPTION_ACCESS_VIOLATION\r\r\n"
    "Description: The thread tried to read from or write to a virtual "
    "address for which it does not have the appropriate access.",
    /* 1  EXCEPTION_ARRAY_BOUNDS_EXCEEDED */
    "Error code: EXCEPTION_ARRAY_BOUNDS_EXCEEDED\r\r\n"
    "Description: The thread tried to access an array element that is out "
    "of bounds and the underlying hardware supports bounds checking.",
    /* 2  EXCEPTION_BREAKPOINT */
    "Error code: EXCEPTION_BREAKPOINT\r\r\n"
    "Description: A breakpoint was encountered.",
    /* 3  EXCEPTION_DATATYPE_MISALIGNMENT */
    "Error code: EXCEPTION_DATATYPE_MISALIGNMENT\r\r\n"
    "Description: The thread tried to read or write data that is misaligned "
    "on hardware that does not provide alignment. For example, 16-bit values "
    "must be aligned on 2-byte boundaries; 32-bit values on 4-byte "
    "boundaries, and so on.",
    /* 4  EXCEPTION_FLT_DENORMAL_OPERAND */
    "Error code: EXCEPTION_FLT_DENORMAL_OPERAND\r\r\n"
    "Description: One of the operands in a floating-point operation is "
    "denormal. A denormal value is one that is too small to represent as a "
    "standard floating-point value.",
    /* 5  EXCEPTION_FLT_DIVIDE_BY_ZERO */
    "Error code: EXCEPTION_FLT_DIVIDE_BY_ZERO\r\r\n"
    "Description: The thread tried to divide a floating-point value by a "
    "floating-point divisor of zero.",
    /* 6  EXCEPTION_FLT_INEXACT_RESULT */
    "Error code: EXCEPTION_FLT_INEXACT_RESULT\r\r\n"
    "Description: The result of a floating-point operation cannot be "
    "represented exactly as a decimal fraction.",
    /* 7  EXCEPTION_FLT_INVALID_OPERATION */
    "Error code: EXCEPTION_FLT_INVALID_OPERATION\r\r\n"
    "Description: Some strange unknown floating point operation was "
    "attempted.",
    /* 8  EXCEPTION_FLT_OVERFLOW */
    "Error code: EXCEPTION_FLT_OVERFLOW\r\r\n"
    "Description: The exponent of a floating-point operation is greater than "
    "the magnitude allowed by the corresponding type.",
    /* 9  EXCEPTION_FLT_STACK_CHECK */
    "Error code: EXCEPTION_FLT_STACK_CHECK\r\r\n"
    "Description: The stack overflowed or underflowed as the result of a "
    "floating-point operation.",
    /* 10 EXCEPTION_FLT_UNDERFLOW */
    "Error code: EXCEPTION_FLT_UNDERFLOW\r\r\n"
    "Description:\tThe exponent of a floating-point operation is less than "
    "the magnitude allowed by the corresponding type.",
    /* 11 EXCEPTION_ILLEGAL_INSTRUCTION */
    "Error code: EXCEPTION_ILLEGAL_INSTRUCTION\r\r\n"
    "Description:\tThe thread tried to execute an invalid instruction.",
    /* 12 EXCEPTION_IN_PAGE_ERROR */
    "Error code: EXCEPTION_IN_PAGE_ERROR\r\r\n"
    "Description:\tThe thread tried to access a page that was not present, "
    "and the system was unable to load the page. For example, this exception "
    "might occur if a network connection is lost while running a program "
    "over the network.",
    /* 13 EXCEPTION_INT_DIVIDE_BY_ZERO */
    "Error code: EXCEPTION_INT_DIVIDE_BY_ZERO\r\r\n"
    "Description: The thread tried to divide an integer value by an integer "
    "divisor of zero.",
    /* 14 EXCEPTION_INT_OVERFLOW */
    "Error code: EXCEPTION_INT_OVERFLOW\r\r\n"
    "Description: The result of an integer operation caused a carry out of "
    "the most significant bit of the result.",
    /* 15 EXCEPTION_INVALID_DISPOSITION */
    "Error code: EXCEPTION_INVALID_DISPOSITION\r\r\n"
    "Description: An exception handler returned an invalid disposition to "
    "the exception dispatcher. Programmers using a high-level language such "
    "as C should never encounter this exception.",
    /* 16 EXCEPTION_NONCONTINUABLE_EXCEPTION */
    "Error code: EXCEPTION_NONCONTINUABLE_EXCEPTION\r\r\n"
    "Description: The thread tried to continue execution after a "
    "noncontinuable exception occurred.",
    /* 17 EXCEPTION_PRIV_INSTRUCTION */
    "Error code: EXCEPTION_PRIV_INSTRUCTION\r\r\n"
    "Description: The thread tried to execute an instruction whose operation "
    "is not allowed in the current machine mode.",
    /* 18 EXCEPTION_SINGLE_STEP */
    "Error code: EXCEPTION_SINGLE_STEP\r\r\n"
    "Description: A trace trap or other single-instruction mechanism "
    "signaled that one instruction has been executed.",
    /* 19 EXCEPTION_STACK_OVERFLOW */
    "Error code: EXCEPTION_STACK_OVERFLOW\r\r\n"
    "Description: The thread used up its stack.",
    /* 20 未识别（ERRID 表的 -1 哨兵落点） */
    "Error code: ?????\r\r\n"
    "Description: Unknown exception."
};

// asm: ERRID_AccessViolation —— 异常码表，-1 结束
const uint32 k_ErrIdTable[] = {
    0xC0000005u, 0xC000008Cu, 0x80000003u, 0x80000002u,
    0xC000008Du, 0xC000008Eu, 0xC000008Fu, 0xC0000090u,
    0xC0000091u, 0xC0000092u, 0xC0000093u, 0xC000001Du,
    0xC0000006u, 0xC0000094u, 0xC0000095u, 0xC0000026u,
    0xC0000025u, 0xC0000096u, 0x80000004u, 0xC00000FDu,
    0xFFFFFFFFu
};

// asm: SymFuncNames —— 依次填进 .data 的 Sym* 槽位
const char* const k_SymFuncNames[] = {
    "SymCleanup",
    "SymGetSymFromAddr",
    "SymInitialize",
    "SymLoadModule",
    "SymSetOptions",
    "SymUnloadModule",
    "StackWalk",
    "SymFunctionTableAccess",
    "SymGetModuleBaseType",
    nullptr
};

// ----------------------------------------------------------------------------
// dbghelp 入口（asm 里就是可写全局槽位；这里按同一顺序整块存放）
// ----------------------------------------------------------------------------
#if defined(OPENYR_EXC_WIN32)
    #define OPENYR_EXC_CALL __stdcall
#else
    #define OPENYR_EXC_CALL
#endif

using SymSetOptions_t       = int32 (OPENYR_EXC_CALL*)(int32);
using SymInitialize_t       = int32 (OPENYR_EXC_CALL*)(void*, const char*, int32);
using SymLoadModule_t       = int32 (OPENYR_EXC_CALL*)(void*, void*, const char*,
                                                       const char*, int32, int32);
using SymGetSymFromAddr_t   = int32 (OPENYR_EXC_CALL*)(void*, int32, uint32*, void*);
using SymCleanup_t          = int32 (OPENYR_EXC_CALL*)(void*);
using SymUnloadModule_t     = int32 (OPENYR_EXC_CALL*)(void*, int32);

// asm: .data 里从 SymCleanup 起的 9 个连续槽位（SymFuncNames 一一对应）
struct SymSlots {
    void* Cleanup;              // asm: SymCleanup
    void* GetSymFromAddr;       // asm: SymGetSymFromAddr
    void* Initialize;           // asm: SymInitialize
    void* LoadModule;           // asm: SymLoadModule
    void* SetOptions;           // asm: SymSetOptions
    void* UnloadModule;         // asm: SymUnloadModule
    void* StackWalk;            // asm: StackWalk
    void* FunctionTableAccess;  // asm: SymFunctionTableAccess
    void* GetModuleBaseType;    // asm: SymGetModuleBaseType
};

SymSlots g_Sym = {nullptr, nullptr, nullptr, nullptr, nullptr,
                  nullptr, nullptr, nullptr, nullptr};

// ----------------------------------------------------------------------------
// 平台薄层：asm 直调 kernel32/IMAGEHLP 的位置
// ----------------------------------------------------------------------------
#if defined(OPENYR_EXC_WIN32)

using IsBadReadPtr_t = int32 (OPENYR_EXC_CALL*)(const void*, uintptr_t);
using IsBadCodePtr_t = int32 (OPENYR_EXC_CALL*)(const void*);

struct Win32Api {
    void* (OPENYR_EXC_CALL* LoadLibraryA)(const char*);
    void* (OPENYR_EXC_CALL* GetProcAddress)(void*, const char*);
    int32 (OPENYR_EXC_CALL* FreeLibrary)(void*);
    uint32 (OPENYR_EXC_CALL* GetModuleFileNameA)(void*, char*, uint32);
    void* (OPENYR_EXC_CALL* GetCurrentProcess)();
    uint32 (OPENYR_EXC_CALL* GetLastError)();
    IsBadReadPtr_t IsBadReadPtr;
    IsBadCodePtr_t IsBadCodePtr;
};

Win32Api g_Win32 = {
    &::LoadLibraryA, &::GetProcAddress, &::FreeLibrary, &::GetModuleFileNameA,
    &::GetCurrentProcess, &::GetLastError, &::IsBadReadPtr, &::IsBadCodePtr
};

inline void* Api_LoadLibrary(const char* pName) { return g_Win32.LoadLibraryA(pName); }
inline void* Api_GetProcAddress(void* hLib, const char* pName)
{
    return g_Win32.GetProcAddress(hLib, pName);
}
inline int32 Api_FreeLibrary(void* hLib) { return g_Win32.FreeLibrary(hLib); }
inline void* Api_GetCurrentProcess() { return g_Win32.GetCurrentProcess(); }
inline int32 Api_GetLastError()
{
    return static_cast<int32>(g_Win32.GetLastError());
}
inline int32 Api_IsBadReadPtr(const void* p, int32 n)
{
    return g_Win32.IsBadReadPtr(p, static_cast<uintptr_t>(n));
}
inline int32 Api_IsBadCodePtr(const void* p) { return g_Win32.IsBadCodePtr(p); }

inline int32 Api_GetModuleFileName(char* pDst, int32 size)
{
    return static_cast<int32>(g_Win32.GetModuleFileNameA(nullptr, pDst,
                                                         static_cast<uint32>(size)));
}

#else   // !OPENYR_EXC_WIN32

// ---------------------------------------------------------------------------
// 非 Windows 构建：没有 IMAGEHLP / kernel32。
//
//   * LoadLibrary 恒失败 —— asm 里"Unable to load IMAGEHLP.DLL\n"的既存分支；
//   * 可读性探测用 SIGSEGV 保护的真读，语义等同 IsBadReadPtr/IsBadCodePtr；
//   * 模块名取 /proc/self/exe；
//   * GetLastError 退化为 errno。
// ---------------------------------------------------------------------------
sigjmp_buf        g_ProbeJmp;
volatile sig_atomic_t g_ProbeActive = 0;
bool              g_ProbeInstalled  = false;

void Probe_SigHandler(int)
{
    if (g_ProbeActive != 0)
        siglongjmp(g_ProbeJmp, 1);
}

bool Probe_Readable(const void* p, int32 n)
{
    if (p == nullptr || n <= 0)
        return false;

    if (!g_ProbeInstalled) {
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sa_handler = &Probe_SigHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_NODEFER;
        sigaction(SIGSEGV, &sa, nullptr);
        sigaction(SIGBUS, &sa, nullptr);
        g_ProbeInstalled = true;
    }

    if (sigsetjmp(g_ProbeJmp, 1) != 0) {
        g_ProbeActive = 0;
        return false;
    }

    g_ProbeActive = 1;
    volatile const uint8* pBytes = static_cast<volatile const uint8*>(p);
    volatile uint8 sink = 0;
    for (int32 i = 0; i < n; ++i)
        sink = static_cast<uint8>(sink ^ pBytes[i]);
    g_ProbeActive = 0;
    (void)sink;
    return true;
}

inline void* Api_LoadLibrary(const char*) { return nullptr; }
inline void* Api_GetProcAddress(void*, const char*) { return nullptr; }
inline int32 Api_FreeLibrary(void*) { return 0; }
inline void* Api_GetCurrentProcess() { return reinterpret_cast<void*>(static_cast<intptr_t>(1)); }
inline int32 Api_GetLastError() { return 0; }
inline int32 Api_IsBadReadPtr(const void* p, int32 n)
{
    return Probe_Readable(p, n) ? 0 : 1;
}
inline int32 Api_IsBadCodePtr(const void* p)
{
    return Probe_Readable(p, 1) ? 0 : 1;
}

inline int32 Api_GetModuleFileName(char* pDst, int32 size)
{
    if (pDst == nullptr || size <= 0)
        return 0;
    const ssize_t n = ::readlink("/proc/self/exe", pDst,
                                 static_cast<size_t>(size - 1));
    if (n <= 0) {
        pDst[0] = '\0';
        return 0;
    }
    pDst[n] = '\0';
    return static_cast<int32>(n);
}

#endif  // OPENYR_EXC_WIN32

// ----------------------------------------------------------------------------
// 版本 / CPU 信息（asm: GetModuleVersion / GetModuleInternalVersion /
// GetModuleBuildTimestamp / GetModuleBuildAuthor / GetModuleBuildVersion /
// GetCPUMMXSupport / GetCPUType）
// ----------------------------------------------------------------------------
const char* Get_Module_Version()
{
    // asm: 读自身 PE 的 VS_VERSIONINFO，任何一步失败都返回 "zzz"。
#if defined(OPENYR_EXC_WIN32)
    static char s_ver[0x40];
    static bool s_done = false;
    if (s_done)
        return s_ver;
    s_done = true;
    s_ver[0] = '\0';                       // asm: byte_846358 一次性标志

    char image[k_ImageNameSize];
    if (Api_GetModuleFileName(image, k_ImageNameSize) <= 0)
        return "zzz";

    DWORD handle = 0;
    const DWORD size = ::GetFileVersionInfoSizeA(image, &handle);
    if (size == 0)
        return "zzz";

    void* pBlock = ::HeapAlloc(::GetProcessHeap(), 0, size);
    if (pBlock == nullptr)
        return "zzz";

    if (::GetFileVersionInfoA(image, handle, size, pBlock) == 0) {
        ::HeapFree(::GetProcessHeap(), 0, pBlock);
        return "zzz";
    }

    // asm: VerQueryValueA("\\") 取 VS_FIXEDFILEINFO，再按
    //      "\\StringFileInfo\\%04d%04d\\ProductVersion" 取字符串。
    VS_FIXEDFILEINFO* pFixed = nullptr;
    UINT fixedLen = 0;
    if (::VerQueryValueA(pBlock, "\\", reinterpret_cast<void**>(&pFixed),
                         &fixedLen) == 0 ||
        pFixed == nullptr) {
        ::HeapFree(::GetProcessHeap(), 0, pBlock);
        return "zzz";
    }

    const WORD lang = static_cast<WORD>(pFixed->dwFileVersionMS >> 16);
    const WORD cp   = static_cast<WORD>(pFixed->dwFileVersionMS & 0xFFFF);

    char subBlock[0x80];
    std::snprintf(subBlock, sizeof(subBlock),
                  "\\StringFileInfo\\%04x%04x\\ProductVersion", lang, cp);

    char* pText = nullptr;
    UINT textLen = 0;
    if (::VerQueryValueA(pBlock, subBlock, reinterpret_cast<void**>(&pText),
                         &textLen) != 0 &&
        pText != nullptr && textLen != 0) {
        std::snprintf(s_ver, sizeof(s_ver), "%s", pText);
        ::HeapFree(::GetProcessHeap(), 0, pBlock);
        return s_ver;
    }

    ::HeapFree(::GetProcessHeap(), 0, pBlock);
    return "zzz";
#else
    return "zzz";                          // asm: loc_74FEE3 / loc_74FECC
#endif
}

const char* Get_Module_Internal_Version()
{
    // asm: GetModuleInternalVersion —— 读 VERSION.TXT 前 16 字节进
    //      Version.VersionString（+0x2A），剥掉尾部 CR/LF。
    static char s_version[0x40];
    static bool s_done = false;
    if (s_done)
        return s_version;

    s_done = true;
    std::memset(s_version, 0, sizeof(s_version));

    std::FILE* pFile = std::fopen("VERSION.TXT", "rb");
    if (pFile == nullptr)
        return s_version;

    char raw[0x10];
    const size_t n = std::fread(raw, 1, sizeof(raw), pFile);
    std::fclose(pFile);

    size_t len = 0;
    while (len < n && raw[len] != '\0')
        ++len;

    // asm: 循环把末尾的 0Dh / 0Ah 逐个改成 0
    while (len > 0 && (raw[len - 1] == '\r' || raw[len - 1] == '\n'))
        --len;

    const size_t copy = (len < sizeof(s_version) - 1) ? len : sizeof(s_version) - 1;
    std::memcpy(s_version, raw, copy);
    s_version[copy] = '\0';
    return s_version;
}

void Get_Module_Build_Timestamp(char* pDst)
{
    // asm: FileTimeToSystemTime(编译期 FILETIME) 后
    //      sprintf("%02d/%02d/%04d - %02d:%02d:%02d",
    //              month, day, year, hour, minute, second)
    // 本工程的可执行体在编译时生成，故用 __DATE__/__TIME__ 承载同一份
    // 构建时刻；格式串与 asm 逐字节一致。
    static const char kMonths[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char* const pDate = __DATE__;          // "Mmm dd yyyy"
    const char* const pTime = __TIME__;          // "hh:mm:ss"

    int32 month = 0;
    for (int32 i = 0; i < 12; ++i) {
        if (std::strncmp(pDate, kMonths + i * 3, 3) == 0) {
            month = i + 1;
            break;
        }
    }
    const int32 day   = std::atoi(pDate + 4);
    const int32 year  = std::atoi(pDate + 7);
    const int32 hour  = std::atoi(pTime);
    const int32 minute = std::atoi(pTime + 3);
    const int32 second = std::atoi(pTime + 6);

    char buffer[0x80];
    std::snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d - %02d:%02d:%02d",
                  month, day, year, hour, minute, second);
    std::snprintf(pDst, 0x80, "%s", buffer);
}

const char* Get_Module_Build_Author(char* pDst)
{
    // asm: strncpy(dst, "gsmallwoo", 20h) —— 作者串原样照抄
    std::strncpy(pDst, "gsmallwoo", 0x20);
    pDst[0x1F] = '\0';
    return pDst;
}

const char* Get_Module_Build_Version(char* pDst)
{
    // asm: sprintf(dst, "%d", BuildVersion)
    std::snprintf(pDst, 0x80, "%d", 1);
    return pDst;
}

void Get_CPU_MMX_Support(int32* pType, uint8* pMmx, char* pDest, int32 length)
{
    // asm: GetCPUMMXSupport —— cpuid 取 MMX 位与家族号，再按需把
    //      "Not available" strncpy 进 dest。
    *pMmx = 0;
    *pType = -1;

#if defined(OPENYR_EXC_X86)
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) != 0) {
        *pMmx  = ((edx >> 23) & 1) ? 1 : 0;               // MMX = EDX bit23
        *pType = static_cast<int32>((eax >> 8) & 0x0F);   // 家族号
    }
#else
    (void)pType;
    (void)pMmx;
#endif

    if (pDest != nullptr && length > 0) {
        std::strncpy(pDest, "Not available", static_cast<size_t>(length));
        pDest[length - 1] = '\0';
    }
}

const char* Get_CPU_Type()
{
    // asm: GetCPUType —— strlist_CPUTypes[家族号]，越界返回 "Unknown"。
    static const char* const k_CpuTypes[] = {
        "386", "486", "Pentium", "Pentium Pro", "Pentium II",
        "Pentium III", "Pentium 4", "Unknown", "Unknown"
    };

    int32 type = -1;
#if defined(OPENYR_EXC_X86)
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) != 0)
        type = static_cast<int32>((eax >> 8) & 0x0F) - 3;   // 家族 3 -> 386
#endif

    if (type > -1 && type < 9)
        return k_CpuTypes[type];
    return "Unknown";
}

// ----------------------------------------------------------------------------
// 日志缓冲操作（asm: lstrlenA / lstrcatA / wsprintfA）
// ----------------------------------------------------------------------------
inline int32 Text_Len(const char* p)
{
    return static_cast<int32>(std::strlen(p));
}

void Text_Append(char* pDst, const char* pSrc)
{
    // asm: 每次追加前都做 lstrlen(dst)+lstrlen(src) < 0FFFFh 的容量检查
    if (Text_Len(pSrc) + Text_Len(pDst) >= k_MaxAppend)
        return;
    std::strcat(pDst, pSrc);
}

void Text_Printf(char* pDst, int32 dstSize, const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    std::vsnprintf(pDst, static_cast<size_t>(dstSize), pFormat, args);
    va_end(args);
}

inline int32 Read_I32(const void* p, int32 off)
{
    int32 value = 0;
    std::memcpy(&value, static_cast<const uint8*>(p) + off, sizeof(value));
    return value;
}

inline void Write_I32(void* p, int32 off, int32 value)
{
    std::memcpy(static_cast<uint8*>(p) + off, &value, sizeof(value));
}

template <typename Fn>
inline Fn Sym_Fn(void* pSlot)
{
    return reinterpret_cast<Fn>(pSlot);
}

// asm: dword_820E5C —— 一次性递归保护，初值 -1
int32 g_RecursionGuard = -1;

}  // namespace

// ============================================================================
// asm: Exception_WriteLog (0x4C85E0)
// ============================================================================
void Exception_WriteLog(const void* pExcInfo)
{
    char* const outBuf = str_except_txt;            // asm: str_except_txt
    char error_string[k_ErrorStringSize];           // asm: error_string
    error_string[0] = '\0';

    std::printf("Dump exception info\n");           // asm: WWDebugString

    // asm: mov ecx,4000h / xor eax,eax / rep stosd —— 清空 64 KB
    std::memset(outBuf, 0, 0x4000 * 4);

    // ---- asm: LoadLibraryA("IMAGEHLP.DLL") + GetProcAddress 循环 ----------
    void* const hLib = Api_LoadLibrary(k_ImageHlpName);
    if (hLib != nullptr) {
        std::printf("Found IMAGEHLP.DLL - linking to required functions\n");

        // asm: loc_4C863B —— 按 SymFuncNames 顺序填槽，遇到 0 结束
        void** pSlot = &g_Sym.Cleanup;
        const char* const* pName = k_SymFuncNames;
        for (; *pName != nullptr; ++pSlot, ++pName)
            *pSlot = Api_GetProcAddress(hLib, *pName);
    } else {
        std::printf("Unable to load IMAGEHLP.DLL\n");
    }

    // ---- asm: loc_4C865C —— SymSetOptions(4) ------------------------------
    if (SymSetOptions_t f = Sym_Fn<SymSetOptions_t>(g_Sym.SetOptions))
        f(4);

    int32 loadModuleResult = 0;                    // asm: pwdDisplacement
    int32 symbolsAvailable = 0;                    // asm: symbols_available

    // ---- asm: loc_4C8669 —— SymInitialize(GetCurrentProcess(), 0, 0) ------
    if (SymInitialize_t fInit = Sym_Fn<SymInitialize_t>(g_Sym.Initialize)) {
        if (fInit(Api_GetCurrentProcess(), nullptr, 0) != 0) {
            std::printf("Symbols are available\r\n\n");
            symbolsAvailable = 1;

            if (SymSetOptions_t fOpt = Sym_Fn<SymSetOptions_t>(g_Sym.SetOptions))
                fOpt(6);

            char imageName[k_ImageNameSize];       // asm: var_204
            imageName[0] = '\0';
            Api_GetModuleFileName(imageName, k_ImageNameSize);

            if (SymLoadModule_t fLoad =
                    Sym_Fn<SymLoadModule_t>(g_Sym.LoadModule)) {
                loadModuleResult = fLoad(Api_GetCurrentProcess(), nullptr,
                                         imageName, nullptr, 0, 0);
            }
            if (loadModuleResult == 0) {
                // asm: 参数按栈序是 (GetLastError, imageName, 0)，即 %s 先吃
                // 到错误码；这里仍按格式串的语义喂值，避免把整数当指针解引用。
                const int32 err = Api_GetLastError();
                std::printf("SymLoad failed for module %s with code %d - %s\n",
                            imageName, err, "");
            }
        } else {
            const int32 err = Api_GetLastError();
            std::printf("SymInitialize failed with code %d - %s\n", err, "");
        }
    }

    // ---- asm: loc_4C871B —— 拆 EXCEPTION_POINTERS -------------------------
    const uint8* const excInfo = static_cast<const uint8*>(pExcInfo);
    const void* record = nullptr;
    const void* context = nullptr;
    if (excInfo != nullptr) {
        std::memcpy(&record, excInfo + k_ExcRecOff, sizeof(record));
        std::memcpy(&context, excInfo + k_CtxPtrOff, sizeof(context));
    }

    // 上下文缺失时用全零 CONTEXT 兜底，寄存器 dump 仍按同一套偏移读出
    uint8 emptyContext[0x2CC];
    if (context == nullptr) {
        std::memset(emptyContext, 0, sizeof(emptyContext));
        context = emptyContext;
    }

    int32 accessInfo = -1;                          // asm: var_4C4
    int32 dumpAddress = 0;                          // asm: DumpRemainingLength
    uint8 emptyRecord[0x20];
    if (record == nullptr) {
        std::memset(emptyRecord, 0, sizeof(emptyRecord));
        record = emptyRecord;
    }
    const int32 code = Read_I32(record, k_RecCodeOff);

    if (static_cast<uint32>(code) == k_AccessViolation) {
        std::printf("Exception is access violation\n");
        accessInfo   = Read_I32(record, k_RecInfo0Off);
        dumpAddress  = Read_I32(record, k_RecInfo1Off);
    } else {
        std::printf("Exception code is %d\n", code);
    }

    // ---- asm: loc_4C877E —— 在 ERRID 表里找描述下标 ------------------------
    int32 esi = 0;
    {
        uint32 cur = k_ErrIdTable[0];
        if (cur != 0xFFFFFFFFu) {
            const uint32* p = k_ErrIdTable;
            for (;;) {
                if (cur == static_cast<uint32>(code)) {
                    std::printf("Found exception description\n");
                    break;
                }
                ++p;
                ++esi;
                cur = *p;
                if (cur == 0xFFFFFFFFu)
                    break;
            }
        }
    }

    // ---- asm: loc_4C87B2 —— 追加描述 + 空行 -------------------------------
    Text_Append(outBuf, k_ErrorStrings[esi]);
    Text_Append(outBuf, "\r\n");

    // ---- asm: loc_4C8809 —— 访问违例的读写说明 ----------------------------
    if (static_cast<uint32>(code) == k_AccessViolation) {
        Text_Printf(error_string, k_ErrorStringSize, "Access address:%08X ",
                    dumpAddress);
        Text_Append(outBuf, error_string);
        Text_Append(outBuf, accessInfo != 0 ? "was written to.\r\n"
                                            : "was read from.\r\n");
    }

    // ---- asm: loc_4C88B2 —— 准备 IMAGEHLP_SYMBOL --------------------------
    uint8 symbol[k_SymbolBufSize];                  // asm: var_384
    std::memset(symbol, 0, 6 * 4);
    Write_I32(symbol, k_SymSizeOfStruct, 0x18);
    Write_I32(symbol, k_SymMaxNameLen, 0xE8);
    Write_I32(symbol, k_SymSize, 0);
    const int32 eip = Read_I32(context, k_CtxEip);
    Write_I32(symbol, k_SymAddress, eip);

    // ---- asm: loc_4C88xx —— EIP 符号化 ------------------------------------
    if (Api_IsBadCodePtr(reinterpret_cast<const void*>(
            static_cast<intptr_t>(eip))) != 0) {
        std::printf("context->Eip is bad code pointer\n");
    } else {
        bool resolved = false;
        if (SymGetSymFromAddr_t fSym =
                Sym_Fn<SymGetSymFromAddr_t>(g_Sym.GetSymFromAddr)) {
            int32 displacement = 0;                 // asm: var_4C0
            if (fSym(Api_GetCurrentProcess(), eip,
                     reinterpret_cast<uint32*>(&displacement), symbol) != 0) {
                Text_Printf(error_string, k_ErrorStringSize,
                            "Exception occurred at %08X - %s + %08X\r\n",
                            eip, symbol + k_SymName, displacement);
                resolved = true;
            }
        }
        if (!resolved) {
            std::printf("Failed to get symbol for EIP\r\n");
            if (g_Sym.GetSymFromAddr != nullptr) {
                const int32 err = Api_GetLastError();
                std::printf(
                    "SymGetSymFromAddr failed with code %d - %s\n", err, "");
            }
            Text_Printf(error_string, k_ErrorStringSize,
                        "Exception occurred at %08X\r\n", eip);
        }
    }
    Text_Append(outBuf, error_string);

    // ---- asm: loc_4C89CF —— 版本行 ----------------------------------------
    Text_Printf(error_string, k_ErrorStringSize, "\r\nVersion %s\r\n",
                Get_Module_Version());
    Text_Append(outBuf, error_string);

    // ---- asm: loc_4C8A0E —— 内部版本行 ------------------------------------
    Text_Printf(error_string, k_ErrorStringSize, "Internal Version %s\r\n",
                Get_Module_Internal_Version());
    Text_Append(outBuf, error_string);

    // ---- asm: loc_4C8A52 —— Release Build 行 ------------------------------
    {
        char timestamp[0x80];                       // asm: esp+254h
        char author[0x80];                          // asm: esp+3DCh
        char buildVer[0x80];                        // asm: esp+45Ch
        timestamp[0] = '\0';
        author[0] = '\0';
        buildVer[0] = '\0';

        Get_Module_Build_Timestamp(timestamp);
        const char* const pAuthor = Get_Module_Build_Author(author);
        const char* const pVersion = Get_Module_Build_Version(buildVer);

        Text_Printf(error_string, k_ErrorStringSize,
                    "Release Build: %s by %s - %s\r\n", pVersion, pAuthor,
                    timestamp);
    }
    Text_Append(outBuf, error_string);

    // ---- asm: loc_4C8AD0 —— CPU / MMX 行 ----------------------------------
    {
        int32 cpuType = 5;                          // asm: var_4AC
        uint8 mmxSupport[0x10];
        std::memset(mmxSupport, 0, sizeof(mmxSupport));
        Get_CPU_MMX_Support(&cpuType, mmxSupport, nullptr, 0);

        const char* const pMmx = (mmxSupport[0] != 0) ? "Yes" : "No";
        // asm: GetCPUType() 的两个栈参实际不被函数读取，返回值才是 CPU 名
        const char* const pCpu = Get_CPU_Type();

        Text_Printf(error_string, k_ErrorStringSize,
                    "CPU %s, MMX %s, Vendor: %s\r\n", pCpu, pMmx,
                    "Not available");
    }
    Text_Append(outBuf, error_string);

    // ---- asm: loc_4C8B45 —— "\r\nDetails:\r\n" ----------------------------
    Text_Append(outBuf, "\r\nDetails:\r\n");

    // ---- asm: loc_4C8B6F —— 寄存器 dump -----------------------------------
    std::printf("Register dump...\n");
    Text_Printf(error_string, k_ErrorStringSize,
                "Eip:%08X\tEsp:%08X\tEbp:%08X\r\n",
                Read_I32(context, k_CtxEip), Read_I32(context, k_CtxEsp),
                Read_I32(context, k_CtxEbp));
    Text_Append(outBuf, error_string);

    Text_Printf(error_string, k_ErrorStringSize,
                "Eax:%08X\tEbx:%08X\tEcx:%08X\r\n",
                Read_I32(context, k_CtxEax), Read_I32(context, k_CtxEbx),
                Read_I32(context, k_CtxEcx));
    Text_Append(outBuf, error_string);

    Text_Printf(error_string, k_ErrorStringSize,
                "Edx:%08X\tEsi:%08X\tEdi:%08X\r\n",
                Read_I32(context, k_CtxEdx), Read_I32(context, k_CtxEsi),
                Read_I32(context, k_CtxEdi));
    Text_Append(outBuf, error_string);

    Text_Printf(error_string, k_ErrorStringSize, "EFlags:%08X \r\n",
                Read_I32(context, k_CtxEFlags));
    Text_Append(outBuf, error_string);

    Text_Printf(error_string, k_ErrorStringSize,
                "CS:%04x  SS:%04x  DS:%04x  ES:%04x  FS:%04x  GS:%04x\r\n",
                Read_I32(context, k_CtxSegCs), Read_I32(context, k_CtxSegSs),
                Read_I32(context, k_CtxSegDs), Read_I32(context, k_CtxSegEs),
                Read_I32(context, k_CtxSegFs), Read_I32(context, k_CtxSegGs));
    Text_Append(outBuf, error_string);

    // ---- asm: loc_4C8D06 —— EIP 字节 dump ---------------------------------
    std::printf("EIP bytes dump...\n");
    Text_Printf(error_string, k_ErrorStringSize,
                "\r\nBytes at CS:EIP (%08X)  : ", eip);
    {
        const uint8* pByte = reinterpret_cast<const uint8*>(
            static_cast<intptr_t>(eip));
        for (int32 count = k_EipDumpLen; count != 0; --count, ++pByte) {
            if (Api_IsBadReadPtr(pByte, 1) != 0) {
                std::strcat(error_string, "?? ");
            } else {
                char chunk[16];                     // asm: Symbol.Flags 暂存
                Text_Printf(chunk, static_cast<int32>(sizeof(chunk)), "%02X ",
                            static_cast<int32>(*pByte));
                std::strcat(error_string, chunk);
            }
        }
        std::strcat(error_string, "\r\n\r\n");
    }
    Text_Append(outBuf, error_string);

    // ---- asm: loc_4C8DBD —— 栈回溯 ----------------------------------------
    std::printf("Stack dump...\n");
    Text_Append(outBuf,
                "Stack dump (* indicates possible code address) :\r\n");
    {
        const uint8* pStack = reinterpret_cast<const uint8*>(
            static_cast<intptr_t>(Read_I32(context, k_CtxEsp)));

        for (int32 count = k_StackDumpLen; count != 0; --count, pStack += 4) {
            if (Api_IsBadReadPtr(pStack, 4) != 0) {
                Text_Printf(error_string, k_ErrorStringSize, "%08X: ",
                            static_cast<int32>(
                                reinterpret_cast<intptr_t>(pStack)));
                std::strcat(error_string, "????????\r\n");
            } else {
                int32 value = 0;
                std::memcpy(&value, pStack, sizeof(value));

                if (Api_IsBadCodePtr(reinterpret_cast<const void*>(
                        static_cast<intptr_t>(value))) != 0) {
                    Text_Printf(error_string, k_ErrorStringSize,
                                "%08X: %08X ",
                                static_cast<int32>(
                                    reinterpret_cast<intptr_t>(pStack)),
                                value);
                    std::strcat(error_string, "DATA_PTR\r\n");
                } else {
                    Text_Printf(error_string, k_ErrorStringSize,
                                "%08X: %08X",
                                static_cast<int32>(
                                    reinterpret_cast<intptr_t>(pStack)),
                                value);

                    if (symbolsAvailable != 0) {
                        // asm: 栈帧用更大的 IMAGEHLP_SYMBOL（0x100/0x80）
                        std::memset(symbol, 0, k_SymName + 0x90);
                        Write_I32(symbol, k_SymSizeOfStruct, 0x100);
                        Write_I32(symbol, k_SymMaxNameLen, 0x80);
                        Write_I32(symbol, k_SymSize, 0);
                        Write_I32(symbol, k_SymAddress, value);

                        if (SymGetSymFromAddr_t fSym =
                                Sym_Fn<SymGetSymFromAddr_t>(
                                    g_Sym.GetSymFromAddr)) {
                            int32 displacement = 0;
                            if (fSym(Api_GetCurrentProcess(), value,
                                     reinterpret_cast<uint32*>(&displacement),
                                     symbol) != 0) {
                                char symLine[k_ImageNameSize];
                                Text_Printf(symLine,
                                            static_cast<int32>(sizeof(symLine)),
                                            " - %s + %08X",
                                            symbol + k_SymName, displacement);
                                std::strcat(error_string, symLine);
                            }
                        }
                    } else {
                        // asm: 仅在"没有符号表"时补 ' *' 标记
                        std::strcat(error_string, " *");
                    }
                    std::strcat(error_string, "\r\n");
                }
            }
            Text_Append(outBuf, error_string);
        }
    }

    // ---- asm: loc_4C8F6x —— 释放符号引擎 ----------------------------------
    if (symbolsAvailable != 0) {
        if (SymCleanup_t fCleanup = Sym_Fn<SymCleanup_t>(g_Sym.Cleanup))
            fCleanup(Api_GetCurrentProcess());

        if (loadModuleResult != 0) {
            if (SymUnloadModule_t fUnload =
                    Sym_Fn<SymUnloadModule_t>(g_Sym.UnloadModule))
                fUnload(Api_GetCurrentProcess(), 0);
        }
    }

    // ---- asm: loc_4C8F9C —— FreeLibrary -----------------------------------
    if (hLib != nullptr)
        Api_FreeLibrary(hLib);

    // ---- asm: loc_4C8FAB —— 结尾两个空行 ----------------------------------
    Text_Append(outBuf, "\r\n\r\n");

    // 崩溃对话框（Game/Shell.cpp 的 Exception_Dialog）读的是同一份正文
    Set_Except_Text(str_except_txt);
}

// ============================================================================
// asm: TopLevelExceptionFilter 的日志段（loc_4C8FE0 .. loc_4C9058）
// ============================================================================
void Exception_Dump_To_File(const void* pExcInfo)
{
    std::printf("Exception!\n");                 // asm: aException

    // asm: mov eax,dword_820E5C / inc / mov / jnz loc_4C9054
    const int32 depth = ++g_RecursionGuard;
    if (depth != 0)
        return;

    Exception_WriteLog(pExcInfo);

#if defined(OPENYR_EXC_WIN32)
    // asm: CreateFileA("except.txt", 40000000h, 0, 0, 2, 80h, 0)
    HANDLE const hFile = ::CreateFileA("except.txt", GENERIC_WRITE, 0, nullptr,
                                       CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return;

    const DWORD length = static_cast<DWORD>(Text_Len(str_except_txt));
    DWORD written = 0;
    ::WriteFile(hFile, str_except_txt, length, &written, nullptr);
    ::CloseHandle(hFile);
#else
    // 非 Windows：同一份内容落到工作目录的 except.txt
    std::FILE* pFile = std::fopen("except.txt", "wb");
    if (pFile == nullptr)
        return;
    std::fwrite(str_except_txt, 1,
                static_cast<size_t>(Text_Len(str_except_txt)), pFile);
    std::fclose(pFile);
#endif
}
