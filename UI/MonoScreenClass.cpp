// ============================================================================
// UI/MonoScreenClass.cpp
//
// asm 权威：Get_Mono_ScreenPtr_5BC450 / sub_5BCC90 / sub_5BCC40 /
//            MonoClass::Printf
//
// 文本缓冲按行推进：SetCursor 定位后写入，满屏后回到顶部（原版屏也是
// 定长缓冲，越界即卷回第一列）。vprintf 的结果先落到临时行缓冲，再按
// 游标拷进 Text，避免依赖平台 printf 的返回值语义。
// ============================================================================

#include "MonoScreenClass.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr int32 kMonoColumns = 80;
constexpr int32 kMonoRows    = 25;
constexpr int32 kLineBytes   = kMonoColumns + 2;   // 含 CRLF

// asm: dword_ABF168 —— 5 张单色屏的全局指针表。
MonoScreenClass* g_MonoScreens[MonoScreenClass::MaxScreens] = {
    nullptr, nullptr, nullptr, nullptr, nullptr
};

}  // namespace

MonoScreenClass* MonoScreenClass::Get(int32 index)
{
    if (index < 0 || index >= MaxScreens) {
        return nullptr;
    }
    if (g_MonoScreens[index] == nullptr) {
        g_MonoScreens[index] = new MonoScreenClass();
    }
    return g_MonoScreens[index];
}

void MonoScreenClass::DeleteAll()
{
    for (int32 i = 0; i < MaxScreens; ++i) {
        delete g_MonoScreens[i];
        g_MonoScreens[i] = nullptr;
    }
}

MonoScreenClass::MonoScreenClass() noexcept
    : Length(0)
    , CursorX(0)
    , CursorY(0)
{
    std::memset(Text, 0, sizeof(Text));
}

// asm: sub_5BCC90 —— 整屏清零，光标复位。
void MonoScreenClass::Clear()
{
    std::memset(Text, 0, sizeof(Text));
    Length = 0;
    CursorX = 0;
    CursorY = 0;
}

// asm: sub_5BCC40(this, x, y) —— 光标定位。
void MonoScreenClass::SetCursor(int32 x, int32 y)
{
    CursorX = (x < 0) ? 0 : (x >= kMonoColumns ? kMonoColumns - 1 : x);
    CursorY = (y < 0) ? 0 : (y >= kMonoRows ? kMonoRows - 1 : y);
}

void MonoScreenClass::Append(const char* pText, int32 count)
{
    for (int32 i = 0; i < count; ++i) {
        const char ch = pText[i];
        if (ch == '\n') {
            CursorX = 0;
            CursorY = (CursorY + 1) % kMonoRows;
            continue;
        }
        if (CursorX >= kMonoColumns) {
            CursorX = 0;
            CursorY = (CursorY + 1) % kMonoRows;
        }
        const int32 offset = CursorY * kLineBytes + CursorX;
        if (offset >= 0 && offset < TextCapacity) {
            Text[offset] = ch;
        }
        if (Length < offset + 1) {
            Length = offset + 1;
        }
        ++CursorX;
    }
}

// asm: MonoClass::Printf —— vprintf 到 mono 设备。
void MonoScreenClass::Printf(const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    PrintfV(pFormat, args);
    va_end(args);
}

void MonoScreenClass::PrintfV(const char* pFormat, va_list args)
{
    if (pFormat == nullptr) {
        return;
    }
    char line[0x200];
    const int32 count = std::vsnprintf(line, sizeof(line), pFormat, args);
    if (count <= 0) {
        return;
    }
    Append(line, (count < static_cast<int32>(sizeof(line))) ? count
                                                           : static_cast<int32>(sizeof(line)) - 1);
}
