#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"

// ============================================================================
// StringTable —— CSF 字符串表（asm: CSF_* 全局族）
//
// 原版把表体放在一组裸全局里（CSF_Data1/2/3、CSF_array、CSF_arraylen、
// dword_B1CF88、bool_StringTable_Loaded），拆解入口 StringTable_UnloadAll
// 原先躺在 Helpers/AsmComplex6.cpp。这里把那些全局收进类内静态成员，
// 拆解逻辑成为类方法 —— 语义与偏移不变。
//
//   Data3      : 单链表，节点 +0x00 = next，节点本体是 wchar 缓冲
//   Data1/2    : 指针数组，长度取 ArrayLen，逐项释放后再释放数组本体
//   Array      : 单块缓冲
//   ListB1CF88 : 单链表，与 Data3 同样的走法
// ============================================================================
class StringTable
{
public:
    // asm: StringTable_UnloadAll (0x734D30)
    //   按 Data3 → Data1 → Data2 → Array → B1CF88 的顺序释放，最后清 Loaded。
    static void Unload_All();

    // asm: StringTable_ParseFile (0x734A00 区，ecx = 文件名，retn 4)
    //   CSF 加载器：先读 0x18 字节头，然后逐标签块（" LBL" 魔数）读入标签
    //   串（拷进 CSF_array 单块缓冲，并维护 MaxCSFLabelLen）与它的值：
    //   " STR"/" WRTS" 宽字符串先做反码解码，再过空白规范化（重复空格折叠、
    //   换行前的空格剔除、行尾空格剔除），然后按 wcslen 尺寸新分配到
    //   Data1；" WRTS" 额外的那段窄串落到 Data2。
    //   返回 true 表示解析完成（含"读不到下一个块 ID"的提前收尾）。
    static bool Parse_File(const char* pFname);

    // asm: StringTable_LoadFile（asm.txt:1674603，WinMain+1EB3 调用）
    //   原版启动时传的是 "ra2md.str"：函数把扩展名截掉再补 ".csf"，
    //   也就是真正打开 ra2md.csf。这里保留同样的语义（含 Loaded 幂等门），
    //   否则调用方得自己记住「传 str、开 csf」这个反直觉的约定。
    static bool Load_File(const char* pName);

    // asm: CSF_Data1 —— 字符串指针数组（长度 ArrayLen）
    static void** Data1;
    // asm: CSF_Data2 —— 第二组字符串指针数组（长度 ArrayLen）
    static void** Data2;
    // asm: CSF_Data3 —— 链表头（节点 +0x00 = next）
    static void*  Data3;
    // asm: CSF_array —— 单块缓冲
    static void*  Array;
    // asm: dword ptr CSF_arraylen+4 —— 数组条目数
    static int32  ArrayLen;
    // asm: dword_B1CF88 —— 另一条链表头
    static void*  ListB1CF88;
    // asm: bool_StringTable_Loaded
    static bool   Loaded;

    // asm: MainCSF —— 最近一次成功起始解析的 CSF 文件名（`const char*`）。
    //   StringTable_LoadFile 在空指针检查之后、真正解析之前写它
    //   （asm `mov MainCSF, ecx`）。Game_EnableJabber 靠它在 Jabber 开关
    //   变化后按**同一份** CSF 重建字符串表，所以这里必须是加载入口的记录，
    //   不能由调用方随手塞。
    static const char* MainCSF;

    // asm: MaxCSFLabelLen —— 已见标签串的最大字节长度（含结尾 0）
    static int32  MaxLabelLen;
    // asm: byte_B1BF38 —— 标签临时缓冲（db 1000h dup(?)）
    static char   LabelBuffer[0x1000];
    // asm: dword_B17F24 —— 宽字符值临时缓冲（dd 1001h dup(?) = 4004h 字节）
    static wchar_t ValueBuffer[0x2002];
};
