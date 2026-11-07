#pragma once

#include "../Core/Definitions.h"

#include <cstdarg>

// ============================================================================
// UI/MonoScreenClass.h
//
// asm 权威：单色调试屏（mono screen）。
//
//   0x5BC450  Get_Mono_ScreenPtr_5BC450(ecx = 索引) -> dword_ABF168[ecx]
//   0x5BCC90  sub_5BCC90(this)             —— 清屏
//   0x5BCC40  sub_5BCC40(this, x, y)       —— 设置文本游标
//   0x5BCCxx  MonoClass::Printf(this, fmt, ...) —— 带游标推进的格式化输出
//
// 原版把 5 张单色屏挂在 dword_ABF168 的全局指针表里，同步日志用第 4 号屏
// 输出 Human 可读的实时状态。本工程原先只有一张恒为空指针的全局表，使
// 所有 mono 输出被静默丢弃；这里把那张表落成真实对象，清屏/游标/输出三个
// 入口也就是原版那三个函数，调用点不再经过任何函数指针。
// ============================================================================
class MonoScreenClass
{
public:
    static constexpr int32 MaxScreens   = 5;      // asm: dword_ABF168 dd 5 dup(?)
    static constexpr int32 TextCapacity = 0x1000; // 一屏文本缓冲

    // asm: Get_Mono_ScreenPtr_5BC450 —— 按索引取屏，越界返回空。
    static MonoScreenClass* Get(int32 index);

    // 释放全部屏（引擎关闭时调用）。
    static void DeleteAll();

    MonoScreenClass() noexcept;

    // asm: sub_5BCC90 —— 清屏并把游标归零。
    void Clear();

    // asm: sub_5BCC40 —— 把文本游标移到 (x, y)。
    void SetCursor(int32 x, int32 y);

    // asm: MonoClass::Printf —— 格式化输出到当前游标并推进。
    void Printf(const char* pFormat, ...);
    void PrintfV(const char* pFormat, va_list args);

    const char* GetText() const { return Text; }
    int32 GetLength() const { return Length; }

private:
    void Append(const char* pText, int32 count);

    char    Text[TextCapacity];
    int32   Length;
    int32   CursorX;
    int32   CursorY;
};
