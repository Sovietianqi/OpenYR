// ============================================================================
// StringTable.cpp —— CSF 字符串表拆解
//
// asm: StringTable_UnloadAll (0x734D30, retn 0)
//   eax = CSF_Data3
//   while (eax) { next = [eax] ; CSF_Data3 = next ; delete eax ; eax = CSF_Data3 }
//   CSF_Data3 = 0
//   if (CSF_Data1) {
//       for (i = 0 ; i < CSF_arraylen+4 ; ++i) if (CSF_Data1[i]) delete CSF_Data1[i]
//       delete CSF_Data1 ; CSF_Data1 = 0
//   }
//   if (CSF_Data2) { 同上 }
//   if (CSF_array) { delete CSF_array ; CSF_array = 0 }
//   eax = dword_B1CF88
//   while (eax) { esi = [eax] ; delete eax ; eax = esi }
//   dword_B1CF88 = 0
//   bool_StringTable_Loaded = 0
// ============================================================================

#include "FileFormats/StringTable.h"

#include "IO/CCFileClass.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

void** StringTable::Data1      = nullptr;
void** StringTable::Data2      = nullptr;
void*  StringTable::Data3      = nullptr;
void*  StringTable::Array      = nullptr;
int32  StringTable::ArrayLen   = 0;
void*  StringTable::ListB1CF88 = nullptr;
bool   StringTable::Loaded     = false;
const char* StringTable::MainCSF = nullptr;
int32  StringTable::MaxLabelLen = 0;
char   StringTable::LabelBuffer[0x1000] = {};
wchar_t StringTable::ValueBuffer[0x2002] = {};

namespace {

// 链表节点只在 +0x00 处有 next 指针，其余是字符串本体（asm: mov ecx,[eax]）。
void*& NodeNext(void* pNode)
{
    return *static_cast<void**>(pNode);
}

// 释放一组指针数组：逐项释放非空元素，再释放数组本体。
// asm: loc_734D72 / loc_734DB6 —— 两段完全同构。
void FreePointerArray(void**& arr, int32 count)
{
    if (arr == nullptr)
        return;

    for (int32 i = 0; i < count; ++i)
    {
        if (arr[i] != nullptr)
            std::free(arr[i]);
    }
    std::free(arr);
    arr = nullptr;
}

// 释放单链表：每步先把 next 取到局部，再释放当前节点。
// asm: loc_734D3D / loc_734E06
void FreeList(void*& head)
{
    void* pNode = head;
    while (pNode != nullptr)
    {
        void* const pNext = NodeNext(pNode);
        std::free(pNode);
        pNode = pNext;
    }
    head = nullptr;
}

} // namespace

void StringTable::Unload_All(){
    // asm: Data3 链表
    FreeList(Data3);

    // asm: Data1 / Data2 指针数组（长度 CSF_arraylen+4）
    FreePointerArray(Data1, ArrayLen);
    FreePointerArray(Data2, ArrayLen);

    // asm: loc_734DE5 —— 单块缓冲 CSF_array
    if (Array != nullptr)
    {
        std::free(Array);
        Array = nullptr;
    }

    // asm: loc_734E06 —— dword_B1CF88 链表
    FreeList(ListB1CF88);

    // asm: mov bool_StringTable_Loaded, edi(0)
    Loaded = false;
}

// ============================================================================
// asm: StringTable_ParseFile (0x734A00 区, ecx = 文件名, retn 4)
//
//   CCFileClass file(fname)
//   if (!file.Is_Available(0))                      return 0   ; 734CE9
//   if (!file.OpenFile(1))                          return 0
//   if (file.ReadNextBytes(&header, 0x18) != 0x18)  return 0
//   if (file.ReadNextBytes(&blockId, 4) != 4)       return 1   ; 734CAD
//   blockOff = 0 ; valueOff = 0
//   loop:                                                      ; 734A0A
//     if (blockId != ' LBL')                        return 0
//     read numValues(4) ; read labelLen(4)
//     if (labelLen) read LabelBuffer[labelLen]               ; 无返回检查
//     LabelBuffer[labelLen] = 0
//     strcpy(CSF_array + blockOff, LabelBuffer)
//     MaxCSFLabelLen = max(MaxCSFLabelLen, labelLen)
//     [CSF_array+blockOff+24h] = valueOff
//     [CSF_array+blockOff+20h] = numValues
//     if (numValues <= 0) goto next_block                      ; 734C83
//     valByteOff = valueOff * 4                                ; lea ebp,[ecx*4]
//     for (i = 0 ; i < numValues ; ++i) {                      ; 734ACA
//       read vId(4)
//       if (vId != ' STR' && vId != ' WRTS')         return 0
//       read vLen(4)
//       if (vLen) read ValueBuffer[vLen*2]                     ; 无返回检查
//       ValueBuffer[vLen] = 0
//       ones-complement decode + whitespace normalize           ; 734B35/734B4B
//       CSF_Data1[valByteOff] = new wchar[wcslen+1] ; wcscpy
//       if (vId == ' WRTS') {
//         read eLen(4) ; if (eLen) read LabelBuffer[eLen]
//         LabelBuffer[eLen] = 0
//         if (eLen) CSF_Data2[valByteOff] = new char[eLen+1] ; strcpy
//       }
//       ++i ; valByteOff += 4
//     }
//   next_block:
//     blockOff += 0x28 ; valueOff += numValues
//     if (file.ReadNextBytes(&blockId, 4) != 4)      return 1  ; 734CAD
//     goto loop
// ============================================================================

// asm: StringTable_LoadFile（asm.txt:1674603）
bool StringTable::Load_File(const char* pName)
{
    // asm: bool_StringTable_Loaded 为真时直接返回 1（重复调用是空操作）。
    if (Loaded)
        return true;

    if (pName == nullptr || pName[0] == '\0')
        return false;

    // asm: 拷进 Main_CSF_Filename -> strchr('.') -> 截断 -> 追加 ".csf"。
    static char s_MainCSF[MAX_PATH_LEN];
    std::snprintf(s_MainCSF, sizeof(s_MainCSF), "%s", pName);

    char* pDot = std::strrchr(s_MainCSF, '.');
    if (pDot != nullptr)
        *pDot = '\0';

    const size_t used = std::strlen(s_MainCSF);
    if (used + 4 < sizeof(s_MainCSF))
        std::strncat(s_MainCSF, ".csf", sizeof(s_MainCSF) - used - 1);

    MainCSF = s_MainCSF;
    return Parse_File(s_MainCSF);
}

bool StringTable::Parse_File(const char* pFname)
{
    // asm: StringTable_LoadFile 在空指针检查之后、真正解析之前就
    //   `mov MainCSF, ecx` —— 记下"最近一次要加载的文件名"。Game_EnableJabber
    //   在 Jabber 开关变化后靠它按同一份 CSF 重建字符串表。
    if (pFname != nullptr && pFname[0] != '\0')
        MainCSF = pFname;

    CCFileClass file(pFname);                          // asm: lea ecx,[esp+2Ch] ; CCFileClass_CTOR

    if (!file.Is_Available(0))                         // asm: CCFileClass::Is_Available(0)
        return false;                                  // asm: loc_734CE9
    if (!file.OpenFile(1))                             // asm: CCFileClass_OpenFile(1)
        return false;

    uint8 header[0x18];                                // asm: Buffer (CSFHeader)
    if (file.ReadNextBytes(header, 0x18) != 0x18)
        return false;

    int32 blockId = 0;                                 // asm: var_98
    if (file.ReadNextBytes(&blockId, 4) != 4)
        goto success;                                  // asm: loc_734CAD

    {
        int32 blockOff = 0;                            // asm: ebp（0x28 步长）
        int32 valueOff = 0;                            // asm: var_94
        for (;;) {                                     // asm: loc_734A0A
            if (blockId != 0x4C424C20)                 // ' LBL'
                return false;                          // asm: loc_734CE9

            int32 numValues = 0;                       // asm: var_90
            file.ReadNextBytes(&numValues, 4);
            int32 labelLen = 0;                        // asm: nNumberOfBytesToRead
            file.ReadNextBytes(&labelLen, 4);
            if (labelLen != 0)
                file.ReadNextBytes(LabelBuffer, labelLen);
            LabelBuffer[labelLen] = 0;                 // asm: byte_B1BF38[eax], bl

            char* const slab = static_cast<char*>(Array);
            {
                int32 len = 0;                         // asm: repne scasb / rep movsd
                while (LabelBuffer[len]) ++len;
                std::memcpy(slab + blockOff, LabelBuffer,
                            static_cast<size_t>(len) + 1);
            }
            if (labelLen > MaxLabelLen)                // asm: cmp eax,ecx / jle
                MaxLabelLen = labelLen;

            *reinterpret_cast<int32*>(slab + blockOff + 0x24) = valueOff;
            *reinterpret_cast<int32*>(slab + blockOff + 0x20) = numValues;

            if (numValues > 0) {
                int32 valueByte = valueOff * 4;         // asm: lea ebp, ds:0[ecx*4]
                for (int32 v = 0; v < numValues; ++v) { // asm: loc_734ACA
                    int32 vId = 0;
                    file.ReadNextBytes(&vId, 4);
                    if (vId != 0x53545220 && vId != 0x53545257)   // ' STR' / ' WRTS'
                        return false;

                    int32 vLen = 0;
                    file.ReadNextBytes(&vLen, 4);
                    if (vLen != 0)
                        file.ReadNextBytes(ValueBuffer, vLen * 2);
                    ValueBuffer[vLen] = 0;

                    // asm: loc_734B35 —— 逐 wchar 反码，直到遇到 0
                    if (ValueBuffer[0] != 0) {
                        int32 k = 0;
                        for (;;) {
                            ValueBuffer[k] = static_cast<wchar_t>(
                                static_cast<uint16>(~ValueBuffer[k]));
                            ++k;
                            if (ValueBuffer[k] == 0)
                                break;
                        }
                    }
                    // asm: loc_734B4B —— 空白规范化（edx=prev 初值 0，esi=blank 初值 1，
                    //   读游标从 buf+2 起，写游标从 buf 起）
                    if (ValueBuffer[0] != 0) {
                        int32 rd = 1;
                        int32 wr = 0;
                        uint16 prev = 0;
                        int32 blank = 1;
                        for (;;) {
                            const uint16 c = static_cast<uint16>(ValueBuffer[rd]);
                            ++rd;
                            if (c == 0) {
                                if (prev == 0x20)      // asm: cmp dx,20h / sub ecx,2
                                    --wr;
                                break;
                            }
                            if (c == 0x20) {           // asm: loc_734B61 空格分支
                                if (prev == c)         // asm: cmp dx,ax / jz
                                    continue;
                                if (blank != 0)        // asm: cmp esi,ebx / jz 734B7E
                                    continue;
                                ValueBuffer[wr] = static_cast<wchar_t>(c);
                                ++wr;
                                prev = c;
                                blank = 0;
                                continue;
                            }
                            if (c == 0x0A || c == 0x09) {   // asm: loc_734B8A
                                if (prev == 0x20)
                                    --wr;
                                ValueBuffer[wr] = static_cast<wchar_t>(c);
                                blank = 1;
                                prev = c;
                                ++wr;
                                continue;
                            }
                            ValueBuffer[wr] = static_cast<wchar_t>(c);   // asm: loc_734B7E
                            ++wr;
                            prev = c;
                            blank = 0;
                        }
                        ValueBuffer[wr] = 0;           // asm: mov [ecx],bx
                    }

                    const int32 wl = static_cast<int32>(std::wcslen(ValueBuffer));
                    void* str = std::malloc(static_cast<size_t>(wl * 2 + 2));
                    Data1[valueByte / 4] = str;
                    std::wcscpy(reinterpret_cast<wchar_t*>(Data1[valueByte / 4]),
                                ValueBuffer);      // asm: _wcscpy

                    if (vId == 0x53545257) {       // ' WRTS' 附加窄串
                        int32 eLen = 0;
                        file.ReadNextBytes(&eLen, 4);
                        if (eLen != 0)
                            file.ReadNextBytes(LabelBuffer, eLen);
                        LabelBuffer[eLen] = 0;
                        if (eLen != 0) {
                            void* ex = std::malloc(static_cast<size_t>(eLen) + 1);
                            Data2[valueByte / 4] = ex;
                            std::memcpy(ex, LabelBuffer,
                                        static_cast<size_t>(eLen) + 1);
                        }
                    }
                    valueByte += 4;
                }
            }
            blockOff += 0x28;
            valueOff += numValues;
            if (file.ReadNextBytes(&blockId, 4) != 4)
                goto success;                          // asm: loc_734CAD
        }
    }

success:                                               // asm: loc_734CAD
    file.Close();                                      // asm: Buffer / CDFileClass 析构链
    // asm: bool_StringTable_Loaded 在 StringTable_LoadFile 入口就置 1；
    // 这里在真正解析成功后落位，保证 Load_File 的幂等门与「表已可用」一致。
    Loaded = true;
    return true;
}
