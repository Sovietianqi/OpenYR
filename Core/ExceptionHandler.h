#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"

// ============================================================================
// Core/ExceptionHandler.h —— 崩溃日志（EXCEPT.TXT）生成器
//
// asm 权威：
//   Exception_WriteLog      (0x4C85E0, __usercall: ecx = EXCEPTION_POINTERS*)
//                           逐段把异常描述、模块版本、寄存器、EIP 字节、
//                           栈回溯写进 64 KB 的 str_except_txt 累加器。
//   TopLevelExceptionFilter (0x4C8FE0 段) 在第一次异常时调用上面的入口，
//                           再把 str_except_txt 原样落到 except.txt。
//
// 本模块只承载"文本生成 + 落盘"这两件事：
//   * str_except_txt 就是原版 .data 里的 64 KB 全局缓冲；
//   * Exception_WriteLog 逐字节对齐 asm 的追加顺序与格式串；
//   * Exception_Dump_To_File 对应过滤器里 CreateFileA("except.txt") /
//     WriteFile / CloseHandle 那一段（含一次性的递归保护）。
// 对话框与表面清理属于 TopLevelExceptionFilter 的其余部分，不在本模块。
// ============================================================================

// asm: str_except_txt —— 崩溃对话框正文 + except.txt 的 64 KB 累加器
extern char str_except_txt[0x10000];

// asm: Exception_WriteLog (0x4C85E0)
//   pExcInfo = EXCEPTION_POINTERS*（[+0] ExceptionRecord，[+4] ContextRecord）
void Exception_WriteLog(const void* pExcInfo);

// asm: TopLevelExceptionFilter 的日志段（loc_4C8FE0 .. loc_4C9058）
//   只有第一次进入时才生成日志并写盘（递归保护 dword_820E5C）。
void Exception_Dump_To_File(const void* pExcInfo);
