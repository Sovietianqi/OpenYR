#include "INI/INIClass.h"
#include "Core/GUID.h"

#include "Core/Memory.h"
#include "Helpers/StringHelpers.h"
#include "IO/FileSystem.h"
#include "IO/Straws.h"
#include "IO/Pipes.h"
#include "Houses/HouseTypeClass.h"
#include "Houses/HouseClass.h"
#include "Houses/SideClass.h"
#include "Combat/WarheadTypeClass.h"
#include "Audio/ThemeClass.h"
#include "Audio/VocClass.h"
#include "SW/SuperWeaponTypeClass.h"
#include "Rendering/ConvertClass.h"
#include "IO/MovieClass.h"
#include "Abstract/TechnoTypeClass.h"
#include "Abstract/AircraftTypeClass.h"
#include "Abstract/BuildingTypeClass.h"
#include "Abstract/InfantryTypeClass.h"
#include "Abstract/TerrainTypeClass.h"
#include "Abstract/UnitTypeClass.h"
#include "Rules/RulesClass.h"
#include "Map/MapClass.h"
#include "Map/CellClass.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <cwchar>

//========================================================================
// INIClass - Implementation
//========================================================================

// asm: trim(char* this) (0x727CF0)。去掉首尾所有值 <= 0x20 的字节
// （空格、\t、\r、\n 及控制符），原地左移。GetString 对读到的值与
// 缺省值都会走一遍，故所有字符串读取结果两端都被裁剪。
static void INI_Trim(char* pStr)
{
    if (!pStr || *pStr == '\0')
        return;

    // 跳过行首 <= 0x20 的字节
    char* p = pStr;
    while (*p != '\0' && static_cast<unsigned char>(*p) <= 0x20)
        ++p;

    if (p != pStr)
    {
        char* d = pStr;
        while (*p != '\0')
            *d++ = *p++;
        *d = '\0';
    }

    // 去行尾
    size_t len = std::strlen(pStr);
    while (len > 0 && static_cast<unsigned char>(pStr[len - 1]) <= 0x20)
        pStr[--len] = '\0';
}

// ===========================================================================
// SHA-1
//
//   asm: CCINIClass::Load 在 bDigest 为真时用 SHAPipe（asm: sub_526470，初值
//   67452301 / EFCDAB89 / 98BADCFE / 10325476 / C3D2E1F0）对**整个文件字节**
//   求 20 字节摘要，再与 INI 里 "Digest" 键的 UUBlock（base64）逐 DWORD 比对
//   （asm: repe cmpsd, 5 次）。这里给出同一算法的紧凑实现：
//     H0..H4 初值同上，64 字节分组，大端写入长度。
//   仅在被要求校验时才计算，规则表加载的热路径上没有额外开销。
// ===========================================================================
namespace
{
    struct SHA1Context
    {
        uint32 H[5];
        uint64 Total;
        uint8  Block[64];
        size_t Fill;
    };

    inline uint32 SHA1_Rotl(uint32 value, int32 bits)
    {
        return (value << bits) | (value >> (32 - bits));
    }

    void SHA1_Init(SHA1Context& ctx)
    {
        ctx.H[0] = 0x67452301u;
        ctx.H[1] = 0xEFCDAB89u;
        ctx.H[2] = 0x98BADCFEu;
        ctx.H[3] = 0x10325476u;
        ctx.H[4] = 0xC3D2E1F0u;
        ctx.Total = 0;
        ctx.Fill  = 0;
    }

    void SHA1_Transform(SHA1Context& ctx, const uint8* pBlock)
    {
        uint32 w[80];
        for (int32 i = 0; i < 16; ++i)
        {
            w[i] = (static_cast<uint32>(pBlock[i * 4 + 0]) << 24)
                 | (static_cast<uint32>(pBlock[i * 4 + 1]) << 16)
                 | (static_cast<uint32>(pBlock[i * 4 + 2]) << 8)
                 |  static_cast<uint32>(pBlock[i * 4 + 3]);
        }
        for (int32 i = 16; i < 80; ++i)
            w[i] = SHA1_Rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32 a = ctx.H[0], b = ctx.H[1], c = ctx.H[2], d = ctx.H[3], e = ctx.H[4];

        for (int32 i = 0; i < 80; ++i)
        {
            uint32 f, k;
            if (i < 20)      { f = (b & c) | ((~b) & d);          k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;                      k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d);    k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;                      k = 0xCA62C1D6u; }

            const uint32 temp = SHA1_Rotl(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = SHA1_Rotl(b, 30);
            b = a;
            a = temp;
        }

        ctx.H[0] += a; ctx.H[1] += b; ctx.H[2] += c; ctx.H[3] += d; ctx.H[4] += e;
    }

    void SHA1_Update(SHA1Context& ctx, const void* pData, size_t nSize)
    {
        const uint8* p = static_cast<const uint8*>(pData);
        ctx.Total += nSize;

        while (nSize > 0)
        {
            const size_t chunk = (nSize < (64 - ctx.Fill)) ? nSize : (64 - ctx.Fill);
            std::memcpy(ctx.Block + ctx.Fill, p, chunk);
            ctx.Fill += chunk;
            p        += chunk;
            nSize    -= chunk;

            if (ctx.Fill == 64)
            {
                SHA1_Transform(ctx, ctx.Block);
                ctx.Fill = 0;
            }
        }
    }

    void SHA1_Final(SHA1Context& ctx, uint8 pOut[20])
    {
        const uint64 bitLength = ctx.Total * 8;

        uint8 pad = 0x80;
        SHA1_Update(ctx, &pad, 1);
        pad = 0x00;
        while (ctx.Fill != 56)
            SHA1_Update(ctx, &pad, 1);

        uint8 length[8];
        for (int32 i = 0; i < 8; ++i)
            length[i] = static_cast<uint8>((bitLength >> (56 - i * 8)) & 0xFF);
        SHA1_Update(ctx, length, 8);

        for (int32 i = 0; i < 5; ++i)
        {
            pOut[i * 4 + 0] = static_cast<uint8>((ctx.H[i] >> 24) & 0xFF);
            pOut[i * 4 + 1] = static_cast<uint8>((ctx.H[i] >> 16) & 0xFF);
            pOut[i * 4 + 2] = static_cast<uint8>((ctx.H[i] >>  8) & 0xFF);
            pOut[i * 4 + 3] = static_cast<uint8>( ctx.H[i]        & 0xFF);
        }
    }

    void SHA1_Compute(const void* pData, size_t nSize, uint8 pOut[20])
    {
        SHA1Context ctx;
        SHA1_Init(ctx);
        SHA1_Update(ctx, pData, nSize);
        SHA1_Final(ctx, pOut);
    }
}

// asm 数据表：.rdata pips[11] / PipScales[5]（cStringEnum {char* string; int idx}）
// pips[0..10]   名字 → 编号：green1 yellow2 white3 red4 blue5
//                          persongreen7 personyellow8 personwhite9
//                          personred10 personblue11 personpurple12
// PipScales[0..4] 名字 → 编号：Ammo1 Tiberium2 Passengers3 Power4 MindControl5
struct INI_StringEnum { const char* Name; int32 Index; };

static const INI_StringEnum g_Pips[] =
{
    { "green",          1 }, { "yellow",        2 }, { "white",        3 },
    { "red",            4 }, { "blue",          5 }, { "persongreen",  7 },
    { "personyellow",   8 }, { "personwhite",   9 }, { "personred",   10 },
    { "personblue",    11 }, { "personpurple", 12 },
};

static const INI_StringEnum g_PipScales[] =
{
    { "Ammo", 1 }, { "Tiberium", 2 }, { "Passengers", 3 },
    { "Power", 4 }, { "MindControl", 5 },
};

INIClass::INIClass()
    : CurrentSectionName(nullptr)
    , CurrentSection(nullptr)
    , LineComments(nullptr)
{
    m_LineBuffer[0] = '\0';
}

INIClass::~INIClass() noexcept
{
    Reset();
}

void INIClass::Reset()
{
    // asm: INIClass_Reset —— ★ 只把"当前解析游标"归位，**不清空已加载的段**。
    //
    //   原实现把 Sections 整条链删光，后果是：每个 *TypeClass::LoadFromINI
    //   开头都会调它（ScriptTypeClass / AnimTypeClass / GameTypePrefsClass），
    //   于是"加载完第一个类型，后面所有类型的 [] 段就全没了"——
    //   [ScriptTypes] 读完 1 条后，[TaskForces]/[TeamTypes]/[AITriggerTypes]
    //   整张表读到的是空 INI（实测 tfKeys 从 2 变 0）。
    //   原版这里是"重置下一个待读游标"，语义上是幂等的读准备动作。
    CurrentSectionName = nullptr;
    CurrentSection = nullptr;
}

void INIClass::Clear(const char* pSection, const char* pKey)
{
    if (!pSection) return;

    INISection* section = GetSection(pSection);
    if (!section) return;

    if (pKey)
    {
        // Clear specific key
        INIEntry* entry = FindEntry(pSection, pKey);
        if (entry)
        {
            entry->Unlink();
            delete entry;
        }
    }
    else
    {
        // Clear entire section
        section->Unlink();
        delete section;
    }
}

// asm: CCINIClass::Load (0x474200) —— 从 CCFileClass 读整份文件，再交给
//   INIFile 管道逐行解析。原版用 FileStraw 流式读，重建侧一次性读进内存：
//   文件规格最多几 MB，代价可以忽略，且能顺带算一次内容摘要（见 ReadCCFile）。
bool INIClass::LoadFile(CCFileClass* pFile)
{
    if (pFile == nullptr)
        return false;

    if (!pFile->Open(static_cast<int32>(FileAccessMode::Read)))
        return false;

    const int32 fileSize = pFile->GetSize();
    if (fileSize <= 0)
    {
        pFile->Close();
        return false;
    }

    char* buffer = static_cast<char*>(YRMemory::Allocate(static_cast<size_t>(fileSize) + 1));
    if (buffer == nullptr)
    {
        pFile->Close();
        return false;
    }

    const int32 bytesRead = pFile->Read(buffer, fileSize);
    pFile->Close();

    if (bytesRead <= 0)
    {
        YRMemory::Deallocate(buffer);
        return false;
    }
    buffer[bytesRead] = '\0';

    const bool parsed = LoadFromBuffer(buffer, static_cast<size_t>(bytesRead), false);
    YRMemory::Deallocate(buffer);
    return parsed;
}

// ===========================================================================
// LoadFromBuffer - INI 文本的逐行解析
//
//   原版把这一步放在 INIFile/FileStraw 管道里（asm: sub_525A60 → INIFile::Load），
//   行为特征（逐条对齐）：
//     * 行分隔符同时接受 CRLF 与 LF；行首尾的 <= 0x20 字节一律裁掉（INI_Trim）；
//     * ';' 开头是注释行，整行忽略（原版在 bLoadComments 时才挂进注释链）；
//     * '[' 开头到第一个 ']' 之间是节名，节名两端空白裁掉；
//     * 其余含 '=' 的行是键值：键取第一个 '=' 左边（右裁空白），值取右边（左裁空白，
//       值里的 '=' 原样保留 —— 原版只用 strchr 找第一个 '='）；
//     * 不属于任何节的行（第一个节头之前）直接丢弃；
//     * 同名节重复出现时并入同一节，同名键后者覆盖前者。
//   与旧实现相比修掉了两个坑：空行时 `line + strlen(line) - 1` 的下标下溢，
//   以及最后一行没有换行符时被跳过的问题。
// ===========================================================================
bool INIClass::LoadFromBuffer(const char* pData, size_t nSize, bool bLoadComments)
{
    if (pData == nullptr || nSize == 0)
        return false;

    Reset();

    char* pText = static_cast<char*>(YRMemory::Allocate(nSize + 1));
    if (pText == nullptr)
        return false;
    std::memcpy(pText, pData, nSize);
    pText[nSize] = '\0';

    INISection* pSection = nullptr;

    char* pCursor = pText;
    char* pLimit = pText + nSize;

    while (pCursor < pLimit && *pCursor != '\0')
    {
        char* pLine = pCursor;

        // 切行：LF 或 CRLF 都算一行结束。
        char* pEol = std::strchr(pCursor, '\n');
        if (pEol != nullptr)
        {
            *pEol = '\0';
            pCursor = pEol + 1;
        }
        else
        {
            pCursor = pLimit;
        }

        // 去掉行尾的 \r 与空白。
        size_t len = std::strlen(pLine);
        while (len > 0)
        {
            const unsigned char ch = static_cast<unsigned char>(pLine[len - 1]);
            if (ch > 0x20)
                break;
            pLine[--len] = '\0';
        }

        // 去掉行首空白。
        while (*pLine != '\0' && static_cast<unsigned char>(*pLine) <= 0x20)
            ++pLine;

        if (*pLine == '\0')
            continue;                      // 空行

        if (*pLine == ';')
        {
            // 注释行：bLoadComments 时挂进当前节的注释链，否则整行丢弃。
            if (bLoadComments && pSection != nullptr)
            {
                INIComment* pComment = new INIComment();
                if (pComment != nullptr)
                {
                    const size_t commentLen = std::strlen(pLine + 1);
                    pComment->Value = static_cast<char*>(YRMemory::Allocate(commentLen + 1));
                    if (pComment->Value != nullptr)
                    {
                        std::memcpy(pComment->Value, pLine + 1, commentLen);
                        pComment->Value[commentLen] = '\0';
                        INI_Trim(pComment->Value);
                    }
                    if (pSection->Comments == nullptr)
                        pSection->Comments = pComment;
                    else
                    {
                        INIComment* pTail = pSection->Comments;
                        while (pTail->Next != nullptr)
                            pTail = pTail->Next;
                        pTail->Next = pComment;
                    }
                }
            }
            continue;
        }

        if (*pLine == '[')
        {
            char* pClosing = std::strchr(pLine, ']');
            if (pClosing == nullptr)
                continue;                  // 缺 ']'：原版整行丢弃

            *pClosing = '\0';
            char* pName = pLine + 1;
            INI_Trim(pName);
            if (*pName == '\0')
                continue;

            pSection = GetOrCreateSection(pName);
            continue;
        }

        if (pSection == nullptr)
            continue;                      // 第一个节头之前的行丢弃

        char* pEquals = std::strchr(pLine, '=');
        if (pEquals == nullptr)
            continue;                      // 段内无 '=' 的行丢弃

        *pEquals = '\0';
        char* pKey = pLine;
        char* pValue = pEquals + 1;

        INI_Trim(pKey);
        if (*pKey == '\0')
            continue;
        while (*pValue != '\0' && static_cast<unsigned char>(*pValue) <= 0x20)
            ++pValue;

        INIEntry* pEntry = GetOrCreateEntry(pSection->Name, pKey);
        if (pEntry != nullptr)
            SetEntryValue(pEntry, pValue);
    }

    YRMemory::Deallocate(pText);
    return true;
}

bool INIClass::SaveFile(CCFileClass* pFile) const
{
    if (!pFile) return false;

    if (!pFile->Open(static_cast<int32>(FileAccessMode::Write)))
        return false;

    // Write sections
    INISection* section = Sections.First();
    while (section)
    {
        // Write section header
        char lineBuffer[512];
        snprintf(lineBuffer, sizeof(lineBuffer), "[%s]\n", section->Name);
        pFile->Write(lineBuffer, static_cast<int32>(strlen(lineBuffer)));

        // Write entries
        INIEntry* entry = section->Entries.First();
        while (entry)
        {
            if (entry->Key && entry->Value)
            {
                snprintf(lineBuffer, sizeof(lineBuffer), "%s=%s\n",
                         entry->Key, entry->Value);
                pFile->Write(lineBuffer, static_cast<int32>(strlen(lineBuffer)));
            }
            entry = static_cast<INIEntry*>(entry->Next);
        }

        // Write a blank line between sections
        pFile->Write(const_cast<char*>("\n"), 1);

        section = static_cast<INISection*>(section->Next);
    }

    pFile->Close();
    return true;
}

//========================================================================
// Section Operations
//========================================================================

INISection* INIClass::GetSection(const char* pSection)
{
    if (!pSection) return nullptr;

    INISection* section = Sections.First();
    while (section)
    {
        if (section->Name && strcasecmp(section->Name, pSection) == 0)
            return section;
        section = static_cast<INISection*>(section->Next);
    }

    return nullptr;
}

int32 INIClass::GetKeyCount(const char* pSection)
{
    INISection* section = GetSection(pSection);
    if (!section) return 0;

    int32 count = 0;
    INIEntry* entry = section->Entries.First();
    while (entry)
    {
        ++count;
        entry = static_cast<INIEntry*>(entry->Next);
    }

    return count;
}

const char* INIClass::GetKeyName(const char* pSection, int32 nKeyIndex)
{
    INISection* section = GetSection(pSection);
    if (!section) return nullptr;

    int32 index = 0;
    INIEntry* entry = section->Entries.First();
    while (entry)
    {
        if (index == nKeyIndex)
            return entry->Key;
        ++index;
        entry = static_cast<INIEntry*>(entry->Next);
    }

    return nullptr;
}

bool INIClass::SectionExists(const char* pSection)
{
    return GetSection(pSection) != nullptr;
}

bool INIClass::KeyExists(const char* pSection, const char* pKey)
{
    return FindEntry(pSection, pKey) != nullptr;
}

//========================================================================
// String Reading/Writing
//========================================================================

// asm: INIClass_GetString (0x528A40, retn 14h = 5 参数)
//   int __thiscall GetString(this, section, key, fallback, dest, maxlen)
// 语义：
//   - dest==0 或 maxlen<2 或 section==0 或 key==0 → 返回 0，且**不写 dest**。
//   - 命中键：strncpy(dest, value, maxlen); dest[maxlen-1]=0; trim(dest);
//     返回 trim 后长度。
//   - 未命中：fallback==0 → dest[0]=0 返回 0；否则
//     strncpy(dest, fallback, maxlen); dest[maxlen-1]=0; trim(dest);
//     返回 trim 后长度。
int32 INIClass::ReadString(const char* pSection, const char* pKey,
                           const char* pDefault, char* pBuffer, size_t bufferSize)
{
    if (!pBuffer || bufferSize < 2) return 0;
    if (!pSection || !pKey) return 0;

    INIEntry* entry = FindEntry(pSection, pKey);

    const char* pSrc = nullptr;
    if (entry && entry->Value)
    {
        pSrc = entry->Value;
    }
    else
    {
        if (!pDefault)
        {
            pBuffer[0] = '\0';
            return 0;
        }
        pSrc = pDefault;
    }

    std::strncpy(pBuffer, pSrc, bufferSize);
    pBuffer[bufferSize - 1] = '\0';
    INI_Trim(pBuffer);
    return static_cast<int32>(std::strlen(pBuffer));
}

bool INIClass::WriteString(const char* pSection, const char* pKey, const char* pString)
{
    if (!pSection || !pKey || !pString) return false;

    INIEntry* entry = GetOrCreateEntry(pSection, pKey);
    if (!entry) return false;

    SetEntryValue(entry, pString);
    return true;
}

//========================================================================
// Integer Reading/Writing
//========================================================================

// asm: INIClass_GetInteger (0x5276E0, retn 0Ch = 3 参数)
//   int __thiscall GetInteger(this, section, key, fallback)
// 语义（解析形态由 WriteInteger 的三态决定）：
//   - section==0 或 key==0 或键不存在/无值 → 返回 fallback。
//   - value[0]=='$'         → sscanf(value, "$%x", &out)，返回 out。
//   - 末字符 tolower=='h'   → sscanf(value, "%xh", &out)，返回 out。
//   - 其它                  → atoi(value)。
//   注意：asm **不**识别 "0x" 前缀，也不做十进制之外的兜底。
int32 INIClass::ReadInteger(const char* pSection, const char* pKey, int32 nDefault)
{
    if (!pSection || !pKey) return nDefault;

    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) return nDefault;

    const char* pValue = entry->Value;

    if (pValue[0] == '$')
    {
        int32 out = nDefault;
        std::sscanf(pValue, "$%x", &out);   // asm: asc_825BB8 == "$%x"
        return out;
    }

    size_t len = std::strlen(pValue);
    if (len > 0 && std::tolower(static_cast<unsigned char>(pValue[len - 1])) == 'h')
    {
        int32 out = nDefault;
        std::sscanf(pValue, "%xh", &out);   // asm: axh == "%xh"
        return out;
    }

    return std::atoi(pValue);
}

bool INIClass::WriteInteger(const char* pSection, const char* pKey, int32 nValue, bool bHex)
{
    if (!pSection || !pKey) return false;

 // INIClass_WriteInteger: the hexadecimal form is written
    // as "%Xh" and, when the value is negative, prefixed by a '$' through
    // "$%X" so the sign round-trips.  The decimal path is a plain "%d".
    char buffer[64];
    if (bHex)
    {
        if (nValue < 0)
            snprintf(buffer, sizeof(buffer), "$%X", -nValue);
        else
            snprintf(buffer, sizeof(buffer), "%Xh", nValue);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "%d", nValue);
    }

    return WriteString(pSection, pKey, buffer);
}

//----------------------------------------------------------------------
// INIClass::ReadHex / WriteHex
//
 //   INIClass_WriteHex is the bare-hex sibling of
//   WriteInteger: it holds the literal "%X" (asc_825BD0) and does nothing
//   more than
//       sprintf(buffer, "%X", value);
//       INIClass_WriteString(this, section, key, buffer);
//   so a value goes out as plain upper-case hexadecimal with no "h" suffix
//   and no sign decoration.  The matching reader parses the same bare form;
//   unlike ReadInteger it does not accept the "0x" prefix, because the
//   writer never emits one.  Both are used by the option and config files
//   whose fields are raw hex bitmasks rather than "valueh" scalars.
//----------------------------------------------------------------------

// asm: INIClass_GetHex (0x528230, retn 0Ch = 3 参数)
//   int __thiscall GetHex(this, section, key, fallback)
// 语义：section==0 或 key==0 或键缺失/无值 → 返回 fallback；否则
//   sscanf(value, "%x", &out) 返回 out（解析失败时 out 仍为 fallback）。
// 注意：asm **不**剥离 '$' 前缀；ReadHex 是它在本项目的同族入口，保持一致。
int32 INIClass::ReadHex(const char* pSection, const char* pKey, int32 nDefault)
{
    if (!pSection || !pKey) return nDefault;

    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) return nDefault;

    int32 out = nDefault;
    std::sscanf(entry->Value, "%x", &out);   // asm: asc_825BD4 == "%x"
    return out;
}

bool INIClass::WriteHex(const char* pSection, const char* pKey, int32 nValue)
{
    if (!pSection || !pKey) return false;

 // INIClass_WriteHex: sprintf(buffer, "%X", value) with no
    // sign handling - the value is taken as an unsigned bit pattern, which is
    // what the callers of this writer expect.
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%X",
             static_cast<unsigned int>(static_cast<uint32>(nValue)));

    return WriteString(pSection, pKey, buffer);
}

//----------------------------------------------------------------------
// INIClass::WriteUUBlock / ReadUUBlock
//
//   The "UU block" pair serialises an arbitrary binary blob into an INI
//   section as a sequence of numbered keys, each holding one line of base64:
//
 //     INIClass::Put_UUBlock
//       - guards: pSection != null, pValue != null, nSize >= 1;
//       - wraps the source in a Base64Straw over a memory Buffer;
//       - loops from key ordinal 1, reading at most 0x46 (70) characters
//         from the straw into a 0x46-byte staging line, NUL-terminating and
//         writing it with INIClass_WriteString(section, "%d", line);
//       - stops when the straw returns 0 bytes; always returns true on the
//         happy path, false from the guards.
//
 //     INIClass::Get_UUBlock
//       - guards: pSection != null (a null section returns 0);
//       - reads the section's key count, then for each ordinal (1-based)
//         fetches the key's string, trims surrounding whitespace, and feeds
//         it through a Base64Pipe into the caller's buffer;
//       - accumulates the decoded length, caps it at nSize, and returns it.
//
//   The 0x46 line length is the original's: 70 base64 characters decode to
//   exactly 52 bytes, so a block is a clean sequence of 52-byte lines.
//----------------------------------------------------------------------

bool INIClass::WriteUUBlock(const char* pSection, const void* pValue,
                            size_t nSize)
{
    if (!pSection || !pValue)
        return false;

    if (static_cast<int32>(nSize) < 1)
        return false;

    // The blob is encoded through a Base64Pipe; a sink collects the streamed
    // text and splits it into 0x46-character lines, writing "1", "2", ... as
    // it goes.  0x46 base64 characters are exactly 68 bytes of text, which is
    // the line width the original uses.
    static const size_t LINE_CHARS = 0x46;

    char line[LINE_CHARS + 1];
    size_t used = 0;
    int32 ordinal = 1;
    line[0] = '\0';

    // A sink that copies the encoder's text into the staging line, emitting a
    // key each time the line fills.
    struct LineSink : public Pipe
    {
        INIClass* Ini;
        const char* Section;
        char* Dest;
        size_t* Used;
        int32* Ordinal;
        size_t Cap;
        LineSink(INIClass* pIni, const char* pSection, char* pDest,
                 size_t* pUsed, int32* pOrdinal, size_t nCap)
            : Ini(pIni), Section(pSection), Dest(pDest), Used(pUsed)
            , Ordinal(pOrdinal), Cap(nCap) {}

        virtual int32 Put(const void* pSrc, int32 nLen) override
        {
            if (!pSrc || nLen <= 0) return 0;

            const char* p = static_cast<const char*>(pSrc);
            int32 consumed = 0;
            while (consumed < nLen)
            {
                size_t space = Cap - *Used;
                if (space == 0)
                {
                    Dest[*Used] = '\0';
                    char key[16];
                    std::sprintf(key, "%d", (*Ordinal)++);
                    Ini->WriteString(Section, key, Dest);
                    *Used = 0;
                    space = Cap;
                }

                size_t take = static_cast<size_t>(nLen - consumed);
                if (take > space) take = space;

                std::memcpy(Dest + *Used, p + consumed, take);
                *Used += take;
                consumed += static_cast<int32>(take);
            }

            Dest[*Used] = '\0';
            return nLen;
        }
    } sink(this, pSection, line, &used, &ordinal, LINE_CHARS);

    Base64Pipe encoder;
    encoder.Put_To(&sink);
    encoder.Put(pValue, static_cast<int32>(nSize));
    encoder.Flush();

    // Flush any residue that did not fill a complete line.
    if (used > 0)
    {
        line[used] = '\0';
        char key[16];
        std::sprintf(key, "%d", ordinal++);
        WriteString(pSection, key, line);
    }

    return true;
}

int32 INIClass::ReadUUBlock(const char* pSection, void* pBuffer, size_t nSize)
{
    if (!pSection)
        return 0;

    if (!pBuffer || nSize == 0)
        return 0;

    uint8* pDest = static_cast<uint8*>(pBuffer);
    int32 totalDecoded = 0;

    // Read ordinal keys 1..N until one is missing, decoding each line into the
    // remaining destination space.
    for (int32 ordinal = 1; ; ++ordinal)
    {
        char key[16];
        std::sprintf(key, "%d", ordinal);

        if (!KeyExists(pSection, key))
            break;

        char line[0x100];
        line[0] = '\0';
        if (ReadString(pSection, key, "", line, sizeof(line)) <= 0)
            break;

        // Trim the line the way the original does before decoding.
        char* pStart = line;
        while (*pStart == ' ' || *pStart == '\t')
            ++pStart;
        char* pEnd = pStart + std::strlen(pStart);
        while (pEnd > pStart && (pEnd[-1] == ' ' || pEnd[-1] == '\t' ||
                                 pEnd[-1] == '\r' || pEnd[-1] == '\n'))
            --pEnd;
        *pEnd = '\0';

        if (*pStart == '\0')
            continue;

        // Decode this line through a Base64Straw fed from a buffer.
        const size_t lineLen = std::strlen(pStart);

        BufferStraw feeder(pStart, lineLen);
        Base64Straw decoder;
        decoder.Get_From(&feeder);

        const int32 remaining = static_cast<int32>(nSize) - totalDecoded;
        if (remaining <= 0)
            break;

        const int32 got = decoder.Get(pDest + totalDecoded, remaining);
        totalDecoded += got;

        // Only stop early when the destination itself is full.
        if (totalDecoded >= static_cast<int32>(nSize))
            break;
    }

    return totalDecoded;
}

//========================================================================
// Boolean Reading/Writing
//========================================================================

// asm: INIClass_GetBool (0x5295A0, retn 0Ch = 3 参数)
//   bool __thiscall GetBool(this, section, key, fallback)
// 语义：只看值的**第一个字符**（toupper 后减 0x30 查 42 项跳表）：
//   '0' / 'F' / 'N'           → false
//   '1' / 'T' / 'Y'           → true
//   其余（含 section/key 为空、键缺失）→ fallback。
// 因此 "yes"/"yep"/"y" 皆为真，"no"/"none"/"n" 皆为假，"true"/"t" 真、
// "false"/"f" 假，"1x" 真、"0x" 假。
bool INIClass::ReadBool(const char* pSection, const char* pKey, bool bDefault)
{
    if (!pSection || !pKey) return bDefault;

    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) return bDefault;

    switch (std::toupper(static_cast<unsigned char>(entry->Value[0])))
    {
        case '0': case 'F': case 'N': return false;
        case '1': case 'T': case 'Y': return true;
        default:                      return bDefault;
    }
}

// asm: INIClass_WriteBool (0x529560, retn 0Ch)
//   非零写 "yes"，为零写 "no"（**不是** "true"/"false"）。
bool INIClass::WriteBool(const char* pSection, const char* pKey, bool bValue)
{
    return WriteString(pSection, pKey, bValue ? "yes" : "no");
}

//========================================================================
// Float/Double Reading/Writing
//========================================================================

float INIClass::ReadFloat(const char* pSection, const char* pKey, float fDefault)
{
    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) return fDefault;

    return static_cast<float>(strtod(entry->Value, nullptr));
}

bool INIClass::WriteFloat(const char* pSection, const char* pKey, float fValue)
{
 // INIClass_WriteFloat uses sprintf(buffer, "%f", value)
    // with a 0x200 byte destination before handing the text to WriteString.
    char buffer[0x200];
    snprintf(buffer, sizeof(buffer), "%f", static_cast<double>(fValue));
    return WriteString(pSection, pKey, buffer);
}

double INIClass::ReadDouble(const char* pSection, const char* pKey, double dDefault)
{
    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) return dDefault;

    return strtod(entry->Value, nullptr);
}

bool INIClass::WriteDouble(const char* pSection, const char* pKey, double dValue)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.15g", dValue);
    return WriteString(pSection, pKey, buffer);
}

//========================================================================
// Fixed / Percentage Reading/Writing
//========================================================================

double INIClass::ReadFixed(const char* pSection, const char* pKey, double dDefault)
{
    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) return dDefault;

    float fValue = 0.0f;
    if (sscanf(entry->Value, "%f", &fValue) != 1) return dDefault;

    double dResult = static_cast<double>(fValue);
    if (strchr(entry->Value, '%') != nullptr)
        dResult *= 0.01;

    return dResult;
}

bool INIClass::WriteFixed(const char* pSection, const char* pKey, double dValue)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%f", dValue);
    return WriteString(pSection, pKey, buffer);
}

//========================================================================
// Lepton (fixed point) Reading/Writing
//========================================================================

int32 INIClass::ReadLepton(const char* pSection, const char* pKey, int32 nDefault)
{
 // CCINIClass::Get_Lepton: the key is read through the signed
    // fixed point accessor seeded with (-1.875, 0.0); a result equal to -1.0 is
    // the "unset" sentinel and is replaced by nDefault, otherwise the value is
    // scaled by 256.0 and floored.
    double dValue = ReadFixed(pSection, pKey, -1.875);

    if (dValue == -1.0)
        return nDefault;

    return static_cast<int32>(std::floor(dValue * 256.0));
}

bool INIClass::WriteLepton(const char* pSection, const char* pKey, int32 nValue)
{
 // CCINIClass::Put_Lepton: value * 3.90625e-3 (1 / 256) is
    // written through the plain "%f" float writer.
    return WriteFloat(pSection, pKey, static_cast<float>(nValue) * 0.00390625f);
}

// asm: INIClass_ReadIntHundredth (0x474800, retn 0Ch = 3 参数)
//   int __thiscall ReadIntHundredth(this, section, key, fallback)
// 语义：把 0..100 的"百分数"换算成 0..255 的定标整数：
//   v = GetInteger(section, key, -1);
//   if (v == -1) return fallback;        // -1 视为未设置
//   v = clamp(v, 0, 100);
//   result = v * 256 / 100;              // asm: imul 0x51EB851F / sar 5
//   return min(result, 255);
int32 INIClass::ReadIntHundredth(const char* pSection, const char* pKey, int32 nDefault)
{
    int32 v = ReadInteger(pSection, pKey, -1);
    if (v == -1) return nDefault;

    if (v >= 100) v = 100;
    if (v <= 0)   v = 0;

    int32 result = v * 256 / 100;
    if (result > 255) result = 255;
    return result;
}

// asm: INIClass_WriteIntHundredth (0x474870, retn 0Ch)
//   写回 value * 100 / 256（向负无穷取整，等价 asm 的 sar 8）。
bool INIClass::WriteIntHundredth(const char* pSection, const char* pKey, int32 nValue)
{
    int32 scaled = nValue * 100;
    // asm: cdq; and edx,0FFh; add eax,edx; sar eax,8  —— 有符号向下取整除以 256
    int32 q = (scaled >= 0) ? (scaled / 256)
                            : -((static_cast<int32>(-static_cast<int64>(scaled)) + 255) / 256);
    return WriteInteger(pSection, pKey, q);
}

//========================================================================
// Word Pair / Scaled Integer Reading/Writing
//========================================================================

// asm: INIClass_Read2Words (0x4746C0, retn 10h = 4 参数)
//   uint16* __stdcall Read2Words(uint16* out, char* section, char* key,
//                                uint16* fallback)
// 语义（逐条对齐 asm）：
//   1. GetString(section, key, Null, dest, 0x40)。
//   2. 返回 0（键不存在）→ loc_4747A2：把缺省 word 对整 4 字节拷入 out。
//   3. 否则 sscanf(dest, "%d,%d", &w0, &w1)，再把两个 dword 的低 16 位
//      依次写入 out[0] / out[1]（asm 不检查 sscanf 命中数）。
uint16* INIClass::Read2Words(uint16* pBuffer, const char* pSection, const char* pKey,
                             const uint16* pDefault)
{
    if (!pBuffer) return nullptr;

    char buffer[0x40];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, nullptr, buffer, sizeof(buffer)) <= 0)
    {
        // asm: loc_4747A2 —— 键缺失时整体拷入缺省。
        pBuffer[0] = pDefault ? pDefault[0] : 0;
        pBuffer[1] = pDefault ? pDefault[1] : 0;
        return pBuffer;
    }

    // asm: sscanf(dest, "%d,%d", ...) 后按 word 写回。
    int32 nFirst  = pDefault ? static_cast<int32>(pDefault[0]) : 0;
    int32 nSecond = pDefault ? static_cast<int32>(pDefault[1]) : 0;
    std::sscanf(buffer, "%d,%d", &nFirst, &nSecond);

    pBuffer[0] = static_cast<uint16>(nFirst);
    pBuffer[1] = static_cast<uint16>(nSecond);
    return pBuffer;
}

bool INIClass::Write2Words(const char* pSection, const char* pKey, const uint16* pValues)
{
    if (!pValues) return false;

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%d,%d",
             static_cast<int32>(pValues[0]), static_cast<int32>(pValues[1]));

    return WriteString(pSection, pKey, buffer);
}

bool INIClass::WriteIntMultiplied(const char* pSection, const char* pKey,
                                  int32 nValue, int32 nMultiplier)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%d", nValue * nMultiplier);
    return WriteString(pSection, pKey, buffer);
}

bool INIClass::WriteIntDivided(const char* pSection, const char* pKey,
                               int32 nValue, int32 nDivisor)
{
    if (nDivisor == 0) return false;

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%d", nValue / nDivisor);
    return WriteString(pSection, pKey, buffer);
}

// asm: INIClass_GetPipIdx (0x474880, retn 0Ch = 3 参数)
//   int __thiscall GetPipIdx(this, section, key, startingIdx)
// 语义：以 pips.string[startingIdx] 为缺省值读字符串，再在 pips 表里
//   大小写不敏感地找名字；命中返回 pips.idx[i]，**未命中返回 1**（不是
//   startingIdx）。键缺失时缺省串会命中 startingIdx 那一项。
int32 INIClass::ReadPipIdx(const char* pSection, const char* pKey, int32 nDefault)
{
    const int32 pipCount = static_cast<int32>(sizeof(g_Pips) / sizeof(g_Pips[0]));

    const char* pDef = (nDefault >= 0 && nDefault < pipCount)
                       ? g_Pips[nDefault].Name
                       : g_Pips[0].Name;

    char buffer[0x20];
    buffer[0] = '\0';
    ReadString(pSection, pKey, pDef, buffer, sizeof(buffer));

    for (int32 i = 0; i < pipCount; ++i)
    {
        if (strcasecmp(buffer, g_Pips[i].Name) == 0)
            return g_Pips[i].Index;
    }
    return 1;
}

// asm: INIClass_WritePipIdx (0x474920, retn 0Ch)
//   idx==0 → 返回 false；否则把 pips.string[idx] 写入（asm 以 arg 直接
//   索引名字表，故这里也按位置索引并做越界保护）。
bool INIClass::WritePipIdx(const char* pSection, const char* pKey, int32 nValue)
{
    const int32 pipCount = static_cast<int32>(sizeof(g_Pips) / sizeof(g_Pips[0]));
    if (nValue <= 0 || nValue >= pipCount) return false;
    return WriteString(pSection, pKey, g_Pips[nValue].Name);
}

// asm: INIClass_GetPipscaleIdx (0x474930, retn 0Ch = 3 参数)
// 语义：读字符串（缺省空串）；读到长度 <=0 → 返回 fallback；否则在
//   PipScales 表里找，命中返回 PipScales.idx[i]，未命中返回 0。
int32 INIClass::ReadPipscaleIdx(const char* pSection, const char* pKey, int32 nDefault)
{
    char buffer[0x20];
    buffer[0] = '\0';
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return nDefault;

    const int32 scaleCount = static_cast<int32>(sizeof(g_PipScales) / sizeof(g_PipScales[0]));
    for (int32 i = 0; i < scaleCount; ++i)
    {
        if (strcasecmp(buffer, g_PipScales[i].Name) == 0)
            return g_PipScales[i].Index;
    }
    return 0;
}

//========================================================================
// Multi-Value Reading
//========================================================================

// asm: INIClass_Get2Integers (0x475BE0, retn 10h = 4 参数)
//   int* __stdcall Get2Integers(int* out, char* section, char* key, int* fallback)
// 语义（逐条对齐 asm）：
//   1. 先把 fallback[0..1] 拷进 out。
//   2. GetString(section, key, Null, Str, 0x200)；返回 0（键不存在）→ 返回 out。
//   3. 否则以 "," 拆两段 _strtok，逐段 _atoi 写回 out[0]/out[1]。
int32* INIClass::Read2Integers(int32* pBuffer, const char* pSection,
                               const char* pKey, const int32* pDefault)
{
    if (!pBuffer) return nullptr;

    if (pDefault)
    {
        pBuffer[0] = pDefault[0];
        pBuffer[1] = pDefault[1];
    }

    char buffer[0x200];
    buffer[0] = '\0';
    if (ReadString(pSection, pKey, nullptr, buffer, sizeof(buffer)) <= 0)
        return pBuffer;

    char* pToken = std::strtok(buffer, ",");
    if (pToken) pBuffer[0] = std::atoi(pToken);

    pToken = std::strtok(nullptr, ",");
    if (pToken) pBuffer[1] = std::atoi(pToken);

    return pBuffer;
}

// asm: INIClass_Read3Integers (0x476430, retn 10h = 4 参数)
//   int* __stdcall Read3Integers(int* out, const char* section, const char* key,
//                                const int* fallback)
// 语义（逐条对齐 asm）：
//   1. 先把 fallback[0..2] 拷进 out —— 键缺失/空值时的最终结果。
//   2. GetString(section, key, Null, Str, 0x200)。
//   3. 返回 0（键不存在）→ 直接返回 out（不改）。
//   4. 否则以 "," 拆三段 _strtok，逐段 _atoi 写入 out[0..2]（缺段保留原值）。
int32* INIClass::Read3Integers(int32* pBuffer, const char* pSection,
                               const char* pKey, const int32* pDefault)
{
    if (!pBuffer) return nullptr;

    if (pDefault)
    {
        pBuffer[0] = pDefault[0];
        pBuffer[1] = pDefault[1];
        pBuffer[2] = pDefault[2];
    }

    char buffer[0x200];
    buffer[0] = '\0';
    if (ReadString(pSection, pKey, nullptr, buffer, sizeof(buffer)) <= 0)
        return pBuffer;

    // asm: strtok(Str, ",") → atoi ; strtok(0,",") → atoi ; strtok(0,",") → atoi
    char* pToken = std::strtok(buffer, ",");
    if (pToken) pBuffer[0] = std::atoi(pToken);

    pToken = std::strtok(nullptr, ",");
    if (pToken) pBuffer[1] = std::atoi(pToken);

    pToken = std::strtok(nullptr, ",");
    if (pToken) pBuffer[2] = std::atoi(pToken);

    return pBuffer;
}

bool INIClass::Write2Integers(const char* pSection, const char* pKey,
                              const int32* pValues)
{
    if (!pValues) return false;
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%d,%d", pValues[0], pValues[1]);
    return WriteString(pSection, pKey, buffer);
}

bool INIClass::Write3Integers(const char* pSection, const char* pKey,
                              const int32* pValues)
{
    if (!pValues) return false;
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%d,%d,%d", pValues[0], pValues[1], pValues[2]);
    return WriteString(pSection, pKey, buffer);
}

//========================================================================
// Color/Byte Reading
//========================================================================

// asm: INIClass_Get3Bytes (0x474B30, retn 10h = 4 参数)
//   uint8* __thiscall Get3Bytes(this, uint8* out, const char* section,
//                               char* key, uint8* fallback)
// 语义（逐条对齐 asm）：
//   1. 把缺省颜色 a5[0..2] 按 "%d,%d,%d" 串成 fallback 串（asm: sprintf）。
//   2. GetString(section, key, fallback, dest, 0x40)。
//   3. 返回 0（键不存在且 fallback 为空串）→ 直接把 a5 三字节拷入 out。
//   4. 否则 sscanf(dest, "%d,%d,%d", &v0,&v1,&v2)，把 v0/v1/v2 低字节
//      依次写入 out[0..2]（asm 用 word+byte 组装）。
uint8* INIClass::Read3Bytes(uint8* pBuffer, const char* pSection,
                            const char* pKey, const uint8* pDefault)
{
    if (!pBuffer) return nullptr;

    uint8 r0 = pDefault ? pDefault[0] : 0;
    uint8 r1 = pDefault ? pDefault[1] : 0;
    uint8 r2 = pDefault ? pDefault[2] : 0;

    // asm: sprintf(fallback, "%d,%d,%d", a5[0], a5[1], a5[2])
    char szFallback[64];
    std::snprintf(szFallback, sizeof(szFallback), "%d,%d,%d", r0, r1, r2);

    char buffer[0x40];
    // asm: GetString(section, key, fallback, dest, 0x40)
    if (ReadString(pSection, pKey, szFallback, buffer, sizeof(buffer)) <= 0)
    {
        // asm: loc_474BFF —— 直接拷入缺省三字节。
        pBuffer[0] = r0;
        pBuffer[1] = r1;
        pBuffer[2] = r2;
        return pBuffer;
    }

    // asm: sscanf(dest, "%d,%d,%d", &v0, &v1, &v2)
    int32 v0 = r0, v1 = r1, v2 = r2;
    std::sscanf(buffer, "%d,%d,%d", &v0, &v1, &v2);

    pBuffer[0] = static_cast<uint8>(v0 & 0xFF);
    pBuffer[1] = static_cast<uint8>(v1 & 0xFF);
    pBuffer[2] = static_cast<uint8>(v2 & 0xFF);
    return pBuffer;
}

bool INIClass::Write3Bytes(const char* pSection, const char* pKey,
                           const uint8* pValues)
{
    if (!pValues) return false;
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%d,%d,%d",
             static_cast<int32>(pValues[0]),
             static_cast<int32>(pValues[1]),
             static_cast<int32>(pValues[2]));
    return WriteString(pSection, pKey, buffer);
}

//========================================================================
// Utility
//========================================================================

bool INIClass::Exists(const char* pSection, const char* pKey)
{
    if (!pSection) return false;

    if (pKey)
        return KeyExists(pSection, pKey);

    return SectionExists(pSection);
}

bool INIClass::IsBlank(const char* pValue)
{
    if (!pValue) return true;
    return (strcasecmp(pValue, "<none>") == 0 ||
            strcasecmp(pValue, "none") == 0);
}

int32 INIClass::GetEntryCount(const char* pSection)
{
    return GetKeyCount(pSection);
}

int32 INIClass::GetLineCount() const
{
    int32 count = 0;
    INIComment* comment = LineComments;
    while (comment)
    {
        ++count;
        comment = comment->Next;
    }
    return count;
}

//========================================================================
// Internal Helpers
//========================================================================

INISection* INIClass::GetOrCreateSection(const char* pSection)
{
    if (!pSection) return nullptr;

    INISection* section = GetSection(pSection);
    if (section) return section;

    // Create new section
    section = new INISection();
    if (!section) return nullptr;

    size_t nameLen = strlen(pSection) + 1;
    section->Name = static_cast<char*>(YRMemory::Allocate(nameLen));
    if (section->Name)
    {
        memcpy(section->Name, pSection, nameLen);
    }
    section->Comments = nullptr;

    Sections.AddTail(section);
    return section;
}

INIEntry* INIClass::GetOrCreateEntry(const char* pSection, const char* pKey)
{
    if (!pSection || !pKey) return nullptr;

    INISection* section = GetOrCreateSection(pSection);
    if (!section) return nullptr;

    INIEntry* entry = FindEntry(pSection, pKey);
    if (entry) return entry;

    // Create new entry
    entry = new INIEntry();
    if (!entry) return nullptr;

    size_t keyLen = strlen(pKey) + 1;
    entry->Key = static_cast<char*>(YRMemory::Allocate(keyLen));
    if (entry->Key)
    {
        memcpy(entry->Key, pKey, keyLen);
    }
    entry->Value = nullptr;
    entry->Comments = nullptr;
    entry->CommentString = nullptr;
    entry->PreIndentCursor = 0;
    entry->PostIndentCursor = 0;
    entry->CommentCursor = 0;

    section->Entries.AddTail(entry);
    return entry;
}

INIEntry* INIClass::FindEntry(const char* pSection, const char* pKey) const
{
    if (!pSection || !pKey) return nullptr;

    INISection* section = const_cast<INIClass*>(this)->GetSection(pSection);
    if (!section) return nullptr;

    INIEntry* entry = section->Entries.First();
    while (entry)
    {
        if (entry->Key && strcasecmp(entry->Key, pKey) == 0)
            return entry;
        entry = static_cast<INIEntry*>(entry->Next);
    }

    return nullptr;
}

void INIClass::SetEntryValue(INIEntry* pEntry, const char* pValue)
{
    if (!pEntry) return;

    if (pEntry->Value)
    {
        YRMemory::Deallocate(pEntry->Value);
        pEntry->Value = nullptr;
    }

    if (pValue)
    {
        size_t len = strlen(pValue) + 1;
        pEntry->Value = static_cast<char*>(YRMemory::Allocate(len));
        if (pEntry->Value)
        {
            memcpy(pEntry->Value, pValue, len);
        }
    }
}

//========================================================================
// CCINIClass - Implementation
//========================================================================

// Static members
CCINIClass* CCINIClass::INI_Rules = nullptr;
CCINIClass CCINIClass::INI_Art;

// The original exposes artmd.ini through the ART_INI global that the art
// readers address directly.  The rebuild keeps the same single instance and
// hands it out through this accessor.
CCINIClass* CCINIClass::GetArtINI()
{
    return &INI_Art;
}
CCINIClass CCINIClass::INI_AI;
CCINIClass CCINIClass::INI_UIMD;
CCINIClass CCINIClass::INI_RA2MD;

uint32 CCINIClass::RulesHash = 0;
uint32 CCINIClass::ArtHash = 0;
uint32 CCINIClass::AIHash = 0;

CCINIClass::CCINIClass()
    : INIClass()
    , Digested(false)
    , CRCValue(0)
{
    memset(Digest, 0, sizeof(Digest));
}

CCINIClass::~CCINIClass() noexcept
{
}

CCINIClass* CCINIClass::LoadINIFile(const char* pFileName)
{
    CCINIClass* pINI = new CCINIClass();
    if (!pINI) return nullptr;

    // 不再用 FileSystem::FileExists 预判：那是纯磁盘检查，而 rulesmd.ini /
    // artmd.ini 在零售版里就躺在 ra2md.mix 内部。ReadCCFile 走的是
    // CCFileClass::Open（先查 MIX 注册表、再落磁盘），失败会返回 false。
    CCFileClass* pFile = new CCFileClass(pFileName);
    pINI->ReadCCFile(pFile, false, false);
    delete pFile;

    return pINI;
}

void CCINIClass::UnloadINIFile(CCINIClass*& pINI)
{
    if (pINI)
    {
        delete pINI;
        pINI = nullptr;
    }
}

// asm: CCINIClass::Load (0x474200) 的完整形态
//   int __thiscall Load(this, CCFileClass* ccFile, char bDigest, int)
//   1. 用 FileStraw 把整份文件读进来并解析（解析器见 LoadFromBuffer）；
//   2. 解析失败（返回 0）→ 直接返回；
//   3. bDigest 为真时：Digested = 0，`Get_UUBlock("Digest", buf, 0x14)` 取 INI
//      内嵌的 20 字节摘要；长度不为 20 → 返回 2（失败）；
//   4. 对整份文件字节跑 SHAPipe（SHA-1），结果写进 this+0x41（Digest[20]），
//      并把 this+0x40（Digested）置 1；
//   5. `repe cmpsd` 比 5 个 DWORD：不一致 → 返回 2；一致 → 返回解析结果。
//   返回 2 与 false 在调用方等价（都当失败处理），故这里用 false。
bool CCINIClass::ReadCCFile(CCFileClass* pFile, bool bDigest, bool bLoadComments)
{
    if (pFile == nullptr)
        return false;

    if (!pFile->Open(static_cast<int32>(FileAccessMode::Read)))
        return false;

    const int32 fileSize = pFile->GetSize();
    if (fileSize <= 0)
    {
        pFile->Close();
        return false;
    }

    char* pBuffer = static_cast<char*>(YRMemory::Allocate(static_cast<size_t>(fileSize) + 1));
    if (pBuffer == nullptr)
    {
        pFile->Close();
        return false;
    }

    const int32 bytesRead = pFile->Read(pBuffer, fileSize);
    pFile->Close();

    if (bytesRead <= 0)
    {
        YRMemory::Deallocate(pBuffer);
        return false;
    }
    pBuffer[bytesRead] = '\0';

    Digested = false;

    if (!LoadFromBuffer(pBuffer, static_cast<size_t>(bytesRead), bLoadComments))
    {
        YRMemory::Deallocate(pBuffer);
        return false;
    }

    if (bDigest)
    {
        // 原版先算摘要（无论是否内嵌 Digest 都算），再与内嵌值比对。
        SHA1_Compute(pBuffer, static_cast<size_t>(bytesRead), Digest);

        uint8 stored[20];
        for (int32 i = 0; i < 20; ++i)
            stored[i] = 0;

        const int32 digestSize = ReadUUBlock("Digest", stored, sizeof(stored));

        if (digestSize != static_cast<int32>(sizeof(stored)))
        {
            YRMemory::Deallocate(pBuffer);
            return false;                  // asm: 长度不符 -> 返回 2
        }

        if (std::memcmp(stored, Digest, sizeof(stored)) != 0)
        {
            YRMemory::Deallocate(pBuffer);
            return false;                  // asm: 5 个 DWORD 不等 -> 返回 2
        }

        Digested = true;
    }

    YRMemory::Deallocate(pBuffer);
    return true;
}

bool CCINIClass::WriteCCFile(CCFileClass* pFile, bool /*bDigest*/)
{
    if (!pFile) return false;

    return SaveFile(pFile);
}

int32 CCINIClass::ReadStringTableEntry(const char* pSection, const char* pKey,
                                       char* pBuffer, size_t bufferSize)
{
    // String table entries are formatted as "Name:Description"
    // This reads the value and looks it up in the string table
    return ReadString(pSection, pKey, nullptr, pBuffer, bufferSize);
}

uint32 CCINIClass::GetCRC() const
{
    // Compute a simple CRC32 of all section/key/value pairs
    uint32 crc = 0xFFFFFFFFu;

    INISection* section = Sections.First();
    while (section)
    {
        if (section->Name)
        {
            const char* name = section->Name;
            while (*name)
            {
                crc ^= static_cast<uint32>(static_cast<uint8>(*name));
                for (int32 i = 0; i < 8; ++i)
                {
                    if (crc & 1)
                        crc = (crc >> 1) ^ 0xEDB88320u;
                    else
                        crc >>= 1;
                }
                ++name;
            }
        }

        INIEntry* entry = section->Entries.First();
        while (entry)
        {
            if (entry->Key)
            {
                const char* key = entry->Key;
                while (*key)
                {
                    crc ^= static_cast<uint32>(static_cast<uint8>(*key));
                    for (int32 i = 0; i < 8; ++i)
                    {
                        if (crc & 1)
                            crc = (crc >> 1) ^ 0xEDB88320u;
                        else
                            crc >>= 1;
                    }
                    ++key;
                }
            }
            if (entry->Value)
            {
                const char* value = entry->Value;
                while (*value)
                {
                    crc ^= static_cast<uint32>(static_cast<uint8>(*value));
                    for (int32 i = 0; i < 8; ++i)
                    {
                        if (crc & 1)
                            crc = (crc >> 1) ^ 0xEDB88320u;
                        else
                            crc >>= 1;
                    }
                    ++value;
                }
            }
            entry = static_cast<INIEntry*>(entry->Next);
        }

        section = static_cast<INISection*>(section->Next);
    }

    return ~crc;
}
//========================================================================
// Audio Enumeration Parsing
//========================================================================

int32 CCINIClass::ParseSoundPriority(const char* pValue)
{
    static const struct
    {
        const char* Name;
        int32 Index;
    } kPriorities[] =
    {
        { "LOWEST",   0 },
        { "LOW",      1 },
        { "NORMAL",   2 },
        { "HIGH",     3 },
        { "CRITICAL", 4 },
    };

    if (!pValue) return -1;

    const int32 nCount = static_cast<int32>(sizeof(kPriorities) / sizeof(kPriorities[0]));
    for (int32 i = 0; i < nCount; ++i)
    {
        if (strcasecmp(pValue, kPriorities[i].Name) == 0)
            return kPriorities[i].Index;
    }

    return -1;
}

int32 CCINIClass::ParseSoundControl(const char* pValue)
{
    if (!pValue) return -1;

    char* pEnd = nullptr;
    int32 nValue = static_cast<int32>(strtol(pValue, &pEnd, 10));
    if (pEnd == pValue) return -1;

    return nValue;
}

int32 CCINIClass::ParseSoundType(const char* pValue)
{
    if (!pValue) return -1;

    char* pEnd = nullptr;
    int32 nValue = static_cast<int32>(strtol(pValue, &pEnd, 10));
    if (pEnd == pValue) return -1;

    return nValue;
}

//========================================================================
// Category Index Mapping
//========================================================================

int32 CCINIClass::BuildCatIdxToNameIdx(const char* const* pNames, int32 nCount,
                                       const char* pValue, int32 nDefault)
{
    if (!pNames || !pValue || nCount <= 0) return nDefault;

    for (int32 i = 0; i < nCount; ++i)
    {
        if (pNames[i] && strcasecmp(pValue, pNames[i]) == 0)
            return i;
    }

    return nDefault;
}

const char* CCINIClass::BuildCatNameToIdx(const char* const* pNames, int32 nCount,
                                          int32 nIndex)
{
    if (!pNames || nIndex < 0 || nIndex >= nCount) return nullptr;

    return pNames[nIndex];
}

bool CCINIClass::GetDigest(uint8* pDigest, size_t digestSize) const
{
    if (!pDigest || digestSize < sizeof(Digest)) return false;

    memcpy(pDigest, Digest, sizeof(Digest));
    return true;
}

//========================================================================
// Game Enumeration Parsing
//
// Each parser mirrors the string table the rules parser uses.  A token that
// is not part of the table falls back to the caller supplied default so a
// typo in an INI can never push an out-of-range value into a type class.
//========================================================================

namespace
{
    struct StringEnum
    {
        const char* Name;
        const char* Abbrev;
    };

    struct StringValue
    {
        const char* Name;
        int32 Value;
    };

    int32 MatchStringValue(const StringValue* pTable, int32 nCount,
                           const char* pValue, int32 nDefault)
    {
        if (!pTable || !pValue || nCount <= 0) return nDefault;

        for (int32 i = 0; i < nCount; ++i)
        {
            if (pTable[i].Name && strcasecmp(pValue, pTable[i].Name) == 0)
                return pTable[i].Value;
        }

        return nDefault;
    }

    const char* StringValueToName(const StringValue* pTable, int32 nCount,
                                  int32 nValue)
    {
        if (!pTable || nCount <= 0) return nullptr;

        for (int32 i = 0; i < nCount; ++i)
        {
            if (pTable[i].Value == nValue) return pTable[i].Name;
        }

        return pTable[0].Name;
    }
}

static const StringEnum Categories[] = {
    { "Soldier",                   "Soldier"   },
    { "Civilian",                  "Civilian"  },
    { "VIP/Agent",                 "VIP"       },
    { "Recon Vehicle",             "Recon"     },
    { "Armored Fighting Vehicle",  "AFV"       },
    { "Infantry Fighting Vehicle", "IFV"       },
    { "Indirect Fire Support",     "LRFS"      },
    { "Misc. Support Vehicle",     "Support"   },
    { "Transport Vehicle",         "Transport" },
    { "Air Combat Support",        "AirPower"  },
    { "Air Transport",             "AirLift"   },
};

static const char* const VHPScans[] = { "None", "Normal", "Strong" };

static const char* const Foundations[] = {
    "1x1", "2x1", "1x2", "2x2", "2x3", "3x2", "3x3", "3x5",
    "4x2", "3x3Refinery", "1x3", "3x1", "4x3", "1x4", "1x5",
    "2x6", "2x5", "5x3", "4x4", "3x4", "6x4", "0x0",
};

static const StringValue PipScales[] = {
    { "Ammo", 1 }, { "Tiberium", 2 }, { "Passengers", 3 },
    { "Power", 4 }, { "MindControl", 5 },
};

static const char* const LandTypes[] = {
    "Clear", "Road", "Water", "Rock", "Wall", "Tiberium",
    "Beach", "Rough", "Ice", "Railroad", "Tunnel", "Weeds",
};

static const char* const MovementZones[] = {
    "Normal", "Crusher", "Destroyer", "AmphibiousDestroyer",
    "AmphibiousCrusher", "Amphibious", "Subterannean", "Infantry",
    "InfantryDestroyer", "Fly", "Water", "WaterBeach", "CrusherAll",
};

static const char* const SpeedTypes[] = {
    "Foot", "Track", "Wheel", "Hover", "Winged", "Float",
    "Amphibious", "FloatBeach",
};

static const char* const Layers[] = {
    "Underground", "Surface", "Ground", "Air", "Top",
};

static const char* const ArmorTypes[] = {
    "none", "flak", "plate", "light", "medium", "heavy",
    "wood", "steel", "concrete", "special_1", "special_2",
};

Armor CCINIClass::ParseArmorType(const char* pValue)
{
    return static_cast<Armor>(BuildCatIdxToNameIdx(
        ArmorTypes, static_cast<int32>(Armor::Count), pValue,
        static_cast<int32>(Armor::None)));
}

Category CCINIClass::ParseCategory(const char* pValue)
{
    if (!pValue || !*pValue) return Category::Soldier;

    for (int32 i = 0; i < static_cast<int32>(Category::Count); ++i)
    {
        if (!_strcmpi(pValue, Categories[i].Name) ||
            !_strcmpi(pValue, Categories[i].Abbrev))
        {
            return static_cast<Category>(i);
        }
    }

    return Category::Soldier;
}

VHPScan CCINIClass::ParseVHPScan(const char* pValue)
{
    return static_cast<VHPScan>(BuildCatIdxToNameIdx(
        VHPScans, static_cast<int32>(VHPScan::Count), pValue,
        static_cast<int32>(VHPScan::None)));
}

Foundation CCINIClass::ParseFoundation(const char* pValue)
{
    return static_cast<Foundation>(BuildCatIdxToNameIdx(
        Foundations, static_cast<int32>(Foundation::Count), pValue,
        static_cast<int32>(Foundation::_1x1)));
}

PipScale CCINIClass::ParsePipScale(const char* pValue)
{
    return static_cast<PipScale>(MatchStringValue(
        PipScales, 5, pValue, static_cast<int32>(PipScale::None)));
}

LandType CCINIClass::ParseLandType(const char* pValue)
{
    return static_cast<LandType>(BuildCatIdxToNameIdx(
        LandTypes, static_cast<int32>(LandType::Count), pValue,
        static_cast<int32>(LandType::Clear)));
}

MovementZone CCINIClass::ParseMovementZone(const char* pValue)
{
    return static_cast<MovementZone>(BuildCatIdxToNameIdx(
        MovementZones, static_cast<int32>(MovementZone::Count), pValue,
        static_cast<int32>(MovementZone::Normal)));
}

SpeedType CCINIClass::ParseSpeedType(const char* pValue)
{
    return static_cast<SpeedType>(BuildCatIdxToNameIdx(
        SpeedTypes, static_cast<int32>(SpeedType::Count), pValue,
        static_cast<int32>(SpeedType::Foot)));
}

Layer CCINIClass::ParseLayer(const char* pValue)
{
    return static_cast<Layer>(BuildCatIdxToNameIdx(
        Layers, static_cast<int32>(Layer::Count), pValue,
        static_cast<int32>(Layer::Ground)));
}

const char* CCINIClass::CategoryIdxToName(int32 nIndex)
{
    if (nIndex < 0 || nIndex >= static_cast<int32>(Category::Count))
        return nullptr;

    return Categories[nIndex].Name;
}

const char* CCINIClass::VHPScanIdxToName(int32 nIndex)
{
    return BuildCatNameToIdx(VHPScans, static_cast<int32>(VHPScan::Count), nIndex);
}

const char* CCINIClass::FoundationIdxToName(int32 nIndex)
{
    return BuildCatNameToIdx(Foundations, static_cast<int32>(Foundation::Count), nIndex);
}

const char* CCINIClass::PipScaleIdxToName(int32 nIndex)
{
    return StringValueToName(PipScales, 5, nIndex);
}

const char* CCINIClass::LandTypeIdxToName(int32 nIndex)
{
    return BuildCatNameToIdx(LandTypes, static_cast<int32>(LandType::Count), nIndex);
}

const char* CCINIClass::MovementZoneIdxToName(int32 nIndex)
{
    return BuildCatNameToIdx(MovementZones, static_cast<int32>(MovementZone::Count), nIndex);
}

const char* CCINIClass::SpeedTypeIdxToName(int32 nIndex)
{
    return BuildCatNameToIdx(SpeedTypes, static_cast<int32>(SpeedType::Count), nIndex);
}

const char* CCINIClass::LayerIdxToName(int32 nIndex)
{
    return BuildCatNameToIdx(Layers, static_cast<int32>(Layer::Count), nIndex);
}

void CCINIClass::ParseAbilities(const char* pValue, int32* pOut, int32 nCount)
{
    if (!pOut || nCount <= 0) return;

    for (int32 i = 0; i < nCount; ++i)
        pOut[i] = 0;

    if (!pValue) return;

    char buffer[256];
    strncpy(buffer, pValue, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* context = nullptr;
    char* token = strtok(buffer, ",");
    int32 slot = 0;

    while (token != nullptr && slot < nCount)
    {
        while (*token == ' ' || *token == '\t') ++token;

        int32 bit = atoi(token);
        if (bit >= 0 && bit < 32)
            pOut[slot] |= (1 << bit);

        token = strtok(nullptr, ",");
        if (token == nullptr)
            break;

        char* next = strtok(token, " ");
        if (next != nullptr)
        {
            pOut[slot] = 0;
            bit = atoi(next);
            if (bit >= 0 && bit < 32)
                pOut[slot] |= (1 << bit);
        }
        ++slot;
    }
}

//========================================================================
// Typed Getters
//========================================================================

static const StringValue BuildCats[] = {
    { "DontCare", 0 }, { "Tech", 1 }, { "Power", 3 },
    { "Resource", 2 }, { "Infrastructure", 4 }, { "Combat", 5 },
};

BuildCat CCINIClass::ParseBuildCat(const char* pValue)
{
    return static_cast<BuildCat>(MatchStringValue(
        BuildCats, 6, pValue, static_cast<int32>(BuildCat::DontCare)));
}

const char* CCINIClass::BuildCatIdxToName(int32 nIndex)
{
    return StringValueToName(BuildCats, 6, nIndex);
}

// asm: INIClass_GetBuildCat (0x475040, retn 0Ch = 3 参数)
//   int __thiscall GetBuildCat(this, char* section, char* key, int idx)
// 语义（逐条对齐 asm）：
//   1. 缺省名取 BuildCatIdxToNameIdx(idx)（arr_BuildCat 中 idx 对应的名字）。
//   2. GetString(section, key, 缺省名, name, 0x20)。
//   3. return BuildCatNameToIdx(name)（未命中 → 0 = DontCare）。
BuildCat CCINIClass::GetBuildCat(const char* pSection, const char* pKey, BuildCat nDefault)
{
    const char* pFallback = BuildCatIdxToName(static_cast<int32>(nDefault));
    if (pFallback == nullptr)
        pFallback = "DontCare";

    char buffer[0x20];
    buffer[0] = '\0';
    ReadString(pSection, pKey, pFallback, buffer, sizeof(buffer));

    return ParseBuildCat(buffer);
}

// asm: INIClass_GetArmorType (0x475460, retn 0Ch = 3 参数)
//   int __thiscall GetArmorType(this, char* string, char* key, int a4)
// 语义（逐条对齐 asm）：
//   1. 缺省名取 strlist_ArmorTypes[a4]（本项目 ArmorTypes[a4]，序与 Armor 枚举一致）。
//   2. GetString(section, key, 缺省名, name, 0x80)。
//   3. return ArmorClass_FindIndexOfString(name)（本项目等价：ParseArmorType）。
Armor CCINIClass::GetArmorType(const char* pSection, const char* pKey, Armor nDefault)
{
    int32 idx = static_cast<int32>(nDefault);
    if (idx < 0 || idx >= static_cast<int32>(Armor::Count))
        idx = 0;

    char buffer[0x80];
    buffer[0] = '\0';
    ReadString(pSection, pKey, ArmorTypes[idx], buffer, sizeof(buffer));

    return ParseArmorType(buffer);
}

Category CCINIClass::GetCategory(const char* pSection, const char* pKey, Category nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return nDefault;

    return ParseCategory(buffer);
}

VHPScan CCINIClass::GetVHPScan(const char* pSection, const char* pKey, VHPScan nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return nDefault;

    return ParseVHPScan(buffer);
}

Foundation CCINIClass::GetFoundation(const char* pSection, const char* pKey, Foundation nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return nDefault;

    return ParseFoundation(buffer);
}

// asm: INIClass_GetAction (0x474ED0)
// ; int __thiscall INIClass_GetAction(CCINIClass *this, char *string,
//                                     char *key, int idx)
//
// 语义对照 asm：
//   1. 读取 section/key 的字符串（32 字节缓冲），缺省值取字符串表第 idx 项
//      （asm: mov eax, ds:strlist_Actions[edx*4]）。
//   2. 以 strlist_Actions 起点遍历到 off_7E4D74（数组末尾），逐项
//      以 __strcmpi（大小写不敏感）比较；命中返回循环下标 edi。
//   3. 未命中返回 0（asm: xor eax,eax）。
//
// 字符串表来源：主 asm .rdata:strlist_Actions（0x7E4B7C 起 73 项指针，
// 终点 off_7E4D74 恰好距起点 73*4 字节）。各指针指向的字面值逐一取自
// asm 的 aXxx db '...',0 定义，按数组声明顺序内联如下，保证下标一致。
int32 CCINIClass::GetAction(const char* pSection, const char* pKey, int32 nDefault)
{
    // asm: strlist_Actions[] / .rdata:off_7E4D74 (数组终点)
    static const char* const kActions[] = {
        "None",             // 0
        "Move",             // 1
        "NoMove",           // 2
        "Enter",            // 3
        "Self",             // 4
        "Attack",           // 5
        "Harvest",          // 6
        "Select",           // 7
        "ToggleSelect",     // 8
        "Capture",          // 9
        "Eaten",            // 10
        "Repair",           // 11
        "Sell",             // 12
        "SellUnit",         // 13
        "NoSell",           // 14
        "NoRepair",         // 15
        "Sabotage",         // 16
        "Tote",             // 17
        "DontUse2",         // 18
        "DontUse3",         // 19
        "Nuke",             // 20
        "DontUse4",         // 21
        "DontUse5",         // 22
        "DontUse6",         // 23
        "DontUse7",         // 24
        "DontUse8",         // 25
        "GuardArea",        // 26
        "Heal",             // 27
        "Damage",           // 28
        "GRepair",          // 29
        "NoDeploy",         // 30
        "NoEnter",          // 31
        "NoGRepair",        // 32
        "TogglePower",      // 33
        "NoTogglePower",    // 34
        "EnterTunnel",      // 35
        "NoEnterTunnel",    // 36
        "IronCurtain",      // 37
        "LightningStorm",   // 38
        "ChronoSphere",     // 39
        "ChronoWarp",       // 40
        "ParaDrop",         // 41
        "PlaceWaypoint",    // 42
        "TibSunBug",        // 43
        "EnterWaypointMode",// 44
        "FollowWaypoint",   // 45
        "SelectWaypoint",   // 46
        "LoopWaypointPath", // 47
        "DragWaypoint",     // 48
        "AttackWaypoint",   // 49
        "EnterWaypoint",    // 50
        "PatrolWaypoint",   // 51
        "AreaAttack",       // 52
        "IvanBomb",         // 53
        "NoIvanBomb",       // 54
        "Detonate",         // 55
        "DetonateAll",      // 56
        "DisarmBomb",       // 57
        "SelectNode",       // 58
        "AttackSupport",    // 59
        "PlaceBeacon",      // 60
        "SelectBeacon",     // 61
        "AttackMoveNav",    // 62
        "AttackMoveTar",    // 63
        "Demolish",         // 64
        "AmerParaDrop",     // 65
        "PsychicDominator", // 66
        "SpyPlane",         // 67
        "GeneticConverter", // 68
        "ForceShield",      // 69
        "NoForceShield",    // 70
        "Airstrike",        // 71
        "PsychicReveal",    // 72
    };
    const int32 kActionCount = static_cast<int32>(sizeof(kActions) / sizeof(kActions[0]));

    const char* pDefault = (nDefault >= 0 && nDefault < kActionCount)
                               ? kActions[nDefault]
                               : "None";

    char buffer[32];
    // asm: push 20h / GetString(section, key, strlist_Actions[idx], Str1, 32)
    // 缺省值即表中第 idx 项，键不存在时用该串参与比较（即命中下标 idx）。
    if (ReadString(pSection, pKey, pDefault, buffer, sizeof(buffer)) <= 0)
        return 0;

    // asm: for (esi = strlist_Actions; esi < off_7E4D74; esi += 4, ++edi)
    for (int32 i = 0; i < kActionCount; ++i)
    {
        if (strcasecmp(kActions[i], buffer) == 0)
            return i;
    }

    return 0;
}

PipScale CCINIClass::GetPipScale(const char* pSection, const char* pKey, PipScale nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return nDefault;

    return ParsePipScale(buffer);
}

// asm: INIClass_GetLand (0x4754E0, retn 0Ch = 3 参数)
//   int __thiscall GetLand(this, char* section, char* key, int idx)
// 语义（逐条对齐 asm）：
//   1. 缺省名取 LandTypeFromIdx(idx)（即 LandTypes[idx] 的名字）。
//   2. GetString(section, key, 缺省名, name, 0x80)。
//   3. 返回 0（键不存在）→ loc_475502：直接返回 idx。
//   4. 否则 LandTypeToIdx(name)（本项目为 ParseLandType）。
LandType CCINIClass::GetLandType(const char* pSection, const char* pKey, LandType nDefault)
{
    int32 idx = static_cast<int32>(nDefault);
    if (idx < 0 || idx >= static_cast<int32>(LandType::Count))
        idx = 0;

    char buffer[0x80];
    buffer[0] = '\0';

    // asm: LandTypeFromIdx(idx) 作为缺省名。
    if (ReadString(pSection, pKey, LandTypes[idx], buffer, sizeof(buffer)) <= 0)
        return static_cast<LandType>(idx);

    return ParseLandType(buffer);
}

MovementZone CCINIClass::GetMovementZone(const char* pSection, const char* pKey,
                                         MovementZone nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return nDefault;

    return ParseMovementZone(buffer);
}

SpeedType CCINIClass::GetSpeedType(const char* pSection, const char* pKey, SpeedType nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return nDefault;

    return ParseSpeedType(buffer);
}

Layer CCINIClass::GetLayer(const char* pSection, const char* pKey, Layer nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return nDefault;

    return ParseLayer(buffer);
}

bool CCINIClass::GetAbilities(const char* pSection, const char* pKey,
                              int32* pOut, int32 nCount)
{
    if (!pOut || nCount <= 0) return false;

    char buffer[256];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return false;

    if (buffer[0] == '\0') return false;

    ParseAbilities(buffer, pOut, nCount);
    return true;
}

bool CCINIClass::Get3Integers(const char* pSection, const char* pKey, int32* pValues)
{
    if (!pValues) return false;

    char buffer[128];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return false;

    return sscanf(buffer, "%d,%d,%d", &pValues[0], &pValues[1], &pValues[2]) == 3;
}

bool CCINIClass::Get3Bytes(const char* pSection, const char* pKey, uint8* pValues)
{
    if (!pValues) return false;

    int32 temp[3] = { 0, 0, 0 };
    if (!Get3Integers(pSection, pKey, temp))
        return false;

    for (int32 i = 0; i < 3; ++i)
        pValues[i] = static_cast<uint8>(temp[i] & 0xFF);

    return true;
}

// asm: INIClass_GetColor (0x474ED0)
// ; byte *__thiscall INIClass_GetColor(CCINIClass *this, byte *a2,
//                                      const char *section, char *key, byte *a5)
//
// 语义对照 asm：
//   1. 把缺省颜色 a5 的三个字节按 "%d,%d,%d" 格式化成 fallback 串
//      （asm: sprintf("%d,%d,%d", a5[0], a5[1], a5[2])）。
//   2. 以 fallback 为缺省值读 section/key 到 40h 字节缓冲 dest
//      （asm: GetString(section, key, fallback, dest, 0x40)）。
//   3. 若读不到（返回 0），直接把 a5 的三个字节拷入 a2 并返回 a2。
//   4. 否则 sscanf(dest, "%d,%d,%d", &v0, &v1, &v2)，再把 v0/v1/v2
//      的低字节依次写入 a2[0..2] 并返回 a2。
//
// 注：原版返回 a2 且调用者不检查空指针，故此处同样直接返回 pOut。
ColorStruct* CCINIClass::GetColor(ColorStruct* pOut, const char* pSection,
                                  const char* pKey, const ColorStruct* pDefault)
{
    if (!pOut) return pOut;

    ColorStruct fallback = pDefault ? *pDefault : ColorStruct(0, 0, 0);

    // asm: sprintf(fallback, "%d,%d,%d", a5[0], a5[1], a5[2])
    char szFallback[64];
    std::snprintf(szFallback, sizeof(szFallback), "%d,%d,%d",
                  fallback.R, fallback.G, fallback.B);

    char buffer[0x40];
    // asm: GetString(section, key, fallback, dest, 0x40)
    if (ReadString(pSection, pKey, szFallback, buffer, sizeof(buffer)) <= 0)
    {
        // asm: loc_474D2D — 键不存在，直接返回缺省颜色。
        *pOut = fallback;
        return pOut;
    }

    // asm: sscanf(dest, "%d,%d,%d", &v0, &v1, &v2)
    int32 v0 = 0, v1 = 0, v2 = 0;
    std::sscanf(buffer, "%d,%d,%d", &v0, &v1, &v2);

    // asm: mov [edx], si ; mov [edx+2], cl — 依次写入三个低字节。
    pOut->R = static_cast<uint8>(v0 & 0xFF);
    pOut->G = static_cast<uint8>(v1 & 0xFF);
    pOut->B = static_cast<uint8>(v2 & 0xFF);
    return pOut;
}

bool CCINIClass::Get2Integers(const char* pSection, const char* pKey, int32* pValues)
{
    if (!pValues) return false;

    char buffer[128];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return false;

    return sscanf(buffer, "%d,%d", &pValues[0], &pValues[1]) == 2;
}

bool CCINIClass::GetVectorIntegers(const char* pSection, const char* pKey,
                                   int32* pValues, int32 nMax)
{
    if (!pValues || nMax <= 0) return false;

    char buffer[512];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return false;

    if (buffer[0] == '\0') return false;

    int32 count = 0;
    char* cursor = buffer;

    while (*cursor != '\0' && count < nMax)
    {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',')
            ++cursor;

        if (*cursor == '\0') break;

        char* start = cursor;
        while (*cursor != '\0' && *cursor != ',')
            ++cursor;

        if (*cursor == ',')
            *cursor++ = '\0';

        pValues[count++] = atoi(start);
    }

    return count > 0;
}

bool CCINIClass::GetVectorColors(const char* pSection, const char* pKey,
                                 DynamicVectorClass<ColorStruct>& rList)
{
    char buffer[512];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return false;

    if (buffer[0] == '\0') return false;

    rList.Clear();

    char* pToken = std::strtok(buffer, ",");

    while (pToken != nullptr)
    {
        if (*pToken == '\0') break;

        int32 red   = std::atoi(pToken);
        int32 green = 0;
        int32 blue  = 0;
        bool  complete = true;

        pToken = std::strtok(nullptr, ",");
        if (pToken != nullptr && *pToken != '\0')
            green = std::atoi(pToken);
        else
            complete = false;

        pToken = std::strtok(nullptr, ",");
        if (pToken != nullptr && *pToken != '\0')
        {
            pToken[std::strlen(pToken) - 1] = '\0';
            blue = std::atoi(pToken);
        }
        else
        {
            complete = false;
        }

        if (complete)
            rList.Add(ColorStruct(static_cast<uint8>(red & 0xFF),
                                  static_cast<uint8>(green & 0xFF),
                                  static_cast<uint8>(blue & 0xFF)));

        pToken = std::strtok(nullptr, ",");
    }

    return rList.Count > 0;
}

namespace
{
    // Comma separated token walker shared by the vector readers.  Each token
    // is handed to the caller; whitespace around it is skipped so that
    // "1, 2, 3" and "1,2,3" parse identically.
    template <typename TFunc>
    void ForEachToken(char* pBuffer, TFunc func)
    {
        char* pToken = std::strtok(pBuffer, ",");

        while (pToken != nullptr)
        {
            while (*pToken == ' ' || *pToken == '\t')
                ++pToken;

            char* pEnd = pToken + std::strlen(pToken);
            while (pEnd > pToken && (pEnd[-1] == ' ' || pEnd[-1] == '\t'))
                --pEnd;
            *pEnd = '\0';

            if (*pToken != '\0')
                func(pToken);

            pToken = std::strtok(nullptr, ",");
        }
    }
}

bool CCINIClass::GetVectorIntegers(const char* pSection, const char* pKey,
                                   DynamicVectorClass<int32>& rList)
{
    char buffer[512];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return false;

    if (buffer[0] == '\0') return false;

    rList.Clear();
    ForEachToken(buffer, [&rList](const char* pToken)
    {
        rList.Add(std::atoi(pToken));
    });

    return rList.Count > 0;
}

bool CCINIClass::GetVectorAircraftType(const char* pSection, const char* pKey,
                                       DynamicVectorClass<AircraftTypeClass*>& rList)
{
    char buffer[512];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return false;

    if (buffer[0] == '\0') return false;

    rList.Clear();
    ForEachToken(buffer, [&rList](const char* pToken)
    {
        AircraftTypeClass* pType = AircraftTypeClass::FindOrAllocate(pToken);
        if (pType != nullptr)
            rList.Add(pType);
    });

    return rList.Count > 0;
}

bool CCINIClass::GetVectorUnitType(const char* pSection, const char* pKey,
                                   DynamicVectorClass<UnitTypeClass*>& rList)
{
    char buffer[512];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return false;

    if (buffer[0] == '\0') return false;

    rList.Clear();
    ForEachToken(buffer, [&rList](const char* pToken)
    {
        UnitTypeClass* pType = UnitTypeClass::FindOrAllocate(pToken);
        if (pType != nullptr)
            rList.Add(pType);
    });

    return rList.Count > 0;
}

bool CCINIClass::GetVectorBuildType(const char* pSection, const char* pKey,
                                    DynamicVectorClass<BuildingTypeClass*>& rList)
{
    char buffer[512];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return false;

    if (buffer[0] == '\0') return false;

    rList.Clear();
    ForEachToken(buffer, [&rList](const char* pToken)
    {
        BuildingTypeClass* pType = BuildingTypeClass::FindOrAllocate(pToken);
        if (pType != nullptr)
            rList.Add(pType);
    });

    return rList.Count > 0;
}

bool CCINIClass::GetVectorInfType(const char* pSection, const char* pKey,
                                  DynamicVectorClass<InfantryTypeClass*>& rList)
{
    char buffer[512];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return false;

    if (buffer[0] == '\0') return false;

    rList.Clear();
    ForEachToken(buffer, [&rList](const char* pToken)
    {
        InfantryTypeClass* pType = InfantryTypeClass::FindOrAllocate(pToken);
        if (pType != nullptr)
            rList.Add(pType);
    });

    return rList.Count > 0;
}

bool CCINIClass::GetVectorTerrainTypes(const char* pSection, const char* pKey,
                                       DynamicVectorClass<TerrainTypeClass*>& rList)
{
    char buffer[512];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return false;

    if (buffer[0] == '\0') return false;

    rList.Clear();
    ForEachToken(buffer, [&rList](const char* pToken)
    {
        TerrainTypeClass* pType = TerrainTypeClass::FindOrAllocate(pToken);
        if (pType != nullptr)
            rList.Add(pType);
    });

    return rList.Count > 0;
}

// asm: INIClass_GetPowerup (0x475AA0, retn 0Ch = 3 参数)
//   int __thiscall GetPowerup(this, char* section, char* key, int fallback)
// 语义（逐条对齐 asm）：
//   1. GetString(section, key, Null, dest, 0x80)。
//   2. 返回 0（键不存在）→ 返回 fallback。
//   3. 否则 PowerupType_FindIndexByName(dest)（本项目以 names[] 表序为索引）。
Powerup CCINIClass::GetPowerup(const char* pSection, const char* pKey, Powerup nDefault)
{
    static const char* const names[] = {
        "Money", "Unit", "HealBase", "Cloak", "Explosion", "Napalm", "Squad",
        "Darkness", "Reveal", "Armor", "Speed", "Firepower", "ICBM",
        "Invulnerability", "Veteran", "IonStorm", "Gas", "Tiberium", "Pod"
    };
    const int32 count = static_cast<int32>(sizeof(names) / sizeof(names[0]));

    char buffer[0x80];
    buffer[0] = '\0';
    if (ReadString(pSection, pKey, nullptr, buffer, sizeof(buffer)) <= 0)
        return nDefault;

    for (int32 i = 0; i < count; ++i)
    {
        if (_strcmpi(buffer, names[i]) == 0)
            return static_cast<Powerup>(i);
    }

    return nDefault;
}

// asm: INIClass_GetOwners (0x4750F0, retn 0Ch = 3 参数)
//   int __thiscall GetOwners(this, const char* section, char* key, int fallback)
// 语义（逐条对齐 asm）：
//   1. esi = fallback；GetString(section, key, Null, Str, 0x80)。
//   2. 返回 0（键不存在）→ 返回 fallback。
//   3. 随即 esi = 0，以 "," 拆名字；键存在但无 token → 返回 0。
//   4. 每个 token 走 Owner_From_Name（名字 → 拥有者位），按位或累加后返回。
int32 CCINIClass::GetOwners(const char* pSection, const char* pKey, int32 nDefault)
{
    char buffer[0x80];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, nullptr, buffer, sizeof(buffer)) <= 0)
        return nDefault;

    int32 mask = 0;

    char* pToken = std::strtok(buffer, ",");
    if (pToken == nullptr)
        return 0;   // asm: 键存在但无 token → 返回 0

    while (pToken != nullptr)
    {
        // asm: Owner_From_Name(token) —— 名字解析为该国家/房屋的拥有者位。
        const int32 index = HouseTypeClass::FindIndex(pToken);
        if (index >= 0 && index < 32)
            mask |= (1 << index);

        pToken = std::strtok(nullptr, ",");
    }

    return mask;
}

// ============================================================================
// CCINIClass::GetPrerequisiteList  -  asm: CCINIClass_GetPrerequisiteList
//                                     (asm:276562, retn 24h)
//
//   原版完整控制流（逐 token，六次 strcmpi + 一次建筑表查找）：
//
//     if (!GetString(section, key, "", Str, 0x80))
//         -> 结果 = 空向量（原版构造空 vector 再 operator= 覆盖调用者的向量）
//     for (tok = strtok(Str, ","); tok && tok[0]; tok = strtok(NULL, ",")) {
//         if (!_strcmpi(tok,"POWER"))    push(-1);
//         if (!_strcmpi(tok,"FACTORY"))  push(-2);
//         if (!_strcmpi(tok,"BARRACKS")) push(-3);
//         if (!_strcmpi(tok,"RADAR"))    push(-4);
//         if (!_strcmpi(tok,"TECH"))     push(-5);
//         if (!_strcmpi(tok,"PROC"))     push(-6);
//         i = BuildingTypeClass_FindIndex(tok);
//         if (i != -1) push(i);
//     }
//
//   六次关键字比较是**并列的 if**（不是 else-if 链），但因为每个 token 只可能
//   等于其中一个，效果等同于 else-if；关键字不参与建筑表查找（必然查不到）。
//   关键点：
//     * 解析用的是 **BuildingTypeClass** 表，不是 TechnoTypeClass 表——
//       Prerequisite= 里写的是建筑 ID（GAPOWR/NAPOWR/...）。
//     * 哨兵是负数，紧跟在它后面的才是普通建筑索引，二者靠符号区分。
//     * 每个关键字压入后都会把"命中过"标志置 1，但该标志最终不影响返回值。
// ============================================================================
namespace
{
    // asm: 六个关键字哨兵值（原版 push 立即数 -1..-6）。
    inline int32 PrereqKeywordIndex(const char* pToken)
    {
        static const struct { const char* pName; int32 nIndex; } keywords[] = {
            { "POWER",    -1 },
            { "FACTORY",  -2 },
            { "BARRACKS", -3 },
            { "RADAR",    -4 },
            { "TECH",     -5 },
            { "PROC",     -6 },
        };

        for (const auto& kw : keywords)
        {
            if (_strcmpi(pToken, kw.pName) == 0)
                return kw.nIndex;
        }

        return 0;   // 0 不是合法哨兵，调用者用"!=0"判断是否命中
    }
}

bool CCINIClass::GetPrerequisiteList(const char* pSection, const char* pKey,
                                     DynamicVectorClass<int32>& rList)
{
    char buffer[0x80];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
    {
        // asm: loc_477409 —— GetString 失败时把空向量拷给调用者。
        rList.Clear();
        return false;
    }

    DynamicVectorClass<int32> parsed;

    char* pToken = std::strtok(buffer, ",");
    while (pToken != nullptr && pToken[0] != '\0')
    {
        const int32 keyword = PrereqKeywordIndex(pToken);
        if (keyword != 0)
        {
            parsed.Add(keyword);
        }
        else
        {
            const int32 idx = BuildingTypeClass::FindIndex(pToken);
            if (idx != -1)
                parsed.Add(idx);
        }

        pToken = std::strtok(nullptr, ",");
    }

    rList = parsed;
    return true;
}

// ============================================================================
// CCINIClass::Get_Vector_Voc  -  asm: INIClass_Get_Vector_Voc_525430
//                                (asm:643239, retn 0Ch)
//
//   [AudioVisual] IceCrackSounds=... 走的就是这里：值是**声音名**列表，
//   原版逐个名字查 VOC 表拿索引，而不是把名字当数字 atoi。
//
//     if (!GetString(section, key, "", Str, 0x80)) -> 结果 = 空向量
//     for (tok = strtok(Str, ","); tok && tok[0]; tok = strtok(NULL, ",")) {
//         pVoc = VocClass_Find(tok);            // 名字表里查指针
//         if (!pVoc) continue;                  // 查不到:丢弃（不占位）
//         push(VocClass_FindIndex(pVoc));       // 指针 -> 索引
//     }
//
//   与 GetVectorIntegers 的差别:那边 atoi 失败也压 0，这边查不到就**不压**，
//   所以结果向量长度可能小于名字个数。
// ============================================================================
void CCINIClass::Get_Vector_Voc(const char* pSection, const char* pKey,
                                DynamicVectorClass<int32>& rList)
{
    char buffer[0x80];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
    {
        rList.Clear();
        return;
    }

    DynamicVectorClass<int32> parsed;

    char* pToken = std::strtok(buffer, ",");
    while (pToken != nullptr && pToken[0] != '\0')
    {
        // asm: VocClass_Find 返回名字条目指针；nullptr 表示未注册。
        const int32 idx = VocClass::FindIndexOfName(pToken);
        if (idx >= 0)
            parsed.Add(idx);

        pToken = std::strtok(nullptr, ",");
    }

    rList = parsed;
}

//========================================================================
// Color Scheme Lookup
//========================================================================

int32 CCINIClass::ReadColorSchemeIndex(const char* pSection, const char* pKey,
                                       int32 nDefault)
{
    if (!ColorScheme::Array || ColorScheme::Array->Count <= 0)
        return nDefault;

    if (nDefault < 0 || nDefault >= ColorScheme::Array->Count)
        nDefault = 0;

    ColorScheme* pDefaultScheme = (*ColorScheme::Array)[nDefault];
    if (pDefaultScheme == nullptr || pDefaultScheme->ID == nullptr)
        return nDefault;

    char buffer[0x20];
    buffer[0] = '\0';

    ReadString(pSection, pKey, pDefaultScheme->ID, buffer, sizeof(buffer));

    for (int32 i = 0; i < ColorScheme::Array->Count; ++i)
    {
        ColorScheme* pScheme = (*ColorScheme::Array)[i];
        if (pScheme == nullptr || pScheme->ID == nullptr)
            continue;

        if (_strcmpi(pScheme->ID, buffer) != 0)
            continue;

        if (pScheme->ShadeCount == 1)
            continue;

        return i;
    }

    return nDefault;
}

// ============================================================================
// GetEdge - INIClass_GetEdge
//
//   The key holds a map-border direction name.  The original walks the
//   six entry strlist_Directions table ("North", "East", "South", "West",
//   "Air") comparing case-insensitively; an unknown or absent value yields
//   the supplied fallback verbatim.
// ============================================================================
// ============================================================================
// FindMovieIndex - INIClass_FindMovieIndex
//
//   Reads a movie name and resolves it through the global movie table.
//   "<none>", an empty value and any unknown name all yield the fallback.
// ============================================================================
int32 CCINIClass::FindMovieIndex(const char* pSection, const char* pKey,
                                 int32 nDefault)
{
    char buffer[0x80];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) == 0)
        return nDefault;

    const int32 index = MovieClass::FindIndexByName(buffer);
    return index < 0 ? nDefault : index;
}

// ============================================================================
// ReadSlotTriple - sub_477440
//
//   Reads a "a,b,c" triple into three integers.  The original tokenises the
//   value with _strtok(", ") and only writes an output when the matching
//   token exists, so missing trailing components keep their previous value.
// ============================================================================
void CCINIClass::ReadSlotTriple(const char* pSection, const char* pKey,
                                int32* pFirst, int32* pSecond, int32* pThird)
{
    char buffer[0x200];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) == 0)
        return;

    char* pToken = std::strtok(buffer, ",");
    if (pToken != nullptr && pFirst != nullptr)
        *pFirst = std::atoi(pToken);

    pToken = std::strtok(nullptr, ",");
    if (pToken != nullptr && pSecond != nullptr)
        *pSecond = std::atoi(pToken);

    pToken = std::strtok(nullptr, ",");
    if (pToken != nullptr && pThird != nullptr)
        *pThird = std::atoi(pToken);
}

// ============================================================================
// ReadPoint - INIClass::Get_Point
//
//   Reads "x,y" into a two-integer point.  The assembly first fetches the
//   section/key text through the CRC-keyed accessor, copies at most 0x40 bytes
//   into a scratch buffer, trims it and finally sscanf's the "%d,%d" pair.  A
//   missing key leaves both outputs untouched.
// ============================================================================
void CCINIClass::ReadPoint(const char* pSection, const char* pKey,
                           int32* pX, int32* pY)
{
    if (pX == nullptr || pY == nullptr)
        return;

    INIEntry* entry = FindEntry(pSection, pKey);
    const char* pValue = (entry != nullptr) ? entry->Value : nullptr;
    if (pValue == nullptr)
        return;

    char buffer[0x40];
    std::strncpy(buffer, pValue, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    StringHelpers::Trim(buffer);
    if (buffer[0] == '\0')
        return;

    int32 nFirst = 0;
    int32 nSecond = 0;
    std::sscanf(buffer, "%d,%d", &nFirst, &nSecond);

    *pX = nFirst;
    *pY = nSecond;
}

// ============================================================================
// ReadRect - INIClass::Get_Rect
//
//   Reads "x,y,w,h" into a RectangleStruct.  Like Get_Point the text is copied
//   into a 0x40 byte scratch buffer, trimmed and parsed with a single
//   sscanf("%d,%d,%d,%d").  When the key is absent every field keeps the
//   caller's prior value.
// ============================================================================
void CCINIClass::ReadRect(const char* pSection, const char* pKey,
                          RectangleStruct* pRect)
{
    if (pRect == nullptr)
        return;

    INIEntry* entry = FindEntry(pSection, pKey);
    const char* pValue = (entry != nullptr) ? entry->Value : nullptr;
    if (pValue == nullptr)
        return;

    char buffer[0x40];
    std::strncpy(buffer, pValue, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    StringHelpers::Trim(buffer);
    if (buffer[0] == '\0')
        return;

    RectangleStruct parsed = *pRect;
    std::sscanf(buffer, "%d,%d,%d,%d",
                &parsed.X, &parsed.Y, &parsed.Width, &parsed.Height);

    *pRect = parsed;
}

// ============================================================================
// WriteRect - INIClass::Put_Rect
//
//   The mirror of Get_Rect: the four fields are rendered with
//   sprintf("%d,%d,%d,%d") and stored through INIClass_WriteString.
// ============================================================================
bool CCINIClass::WriteRect(const char* pSection, const char* pKey,
                           const RectangleStruct* pRect)
{
    if (pSection == nullptr || pKey == nullptr || pRect == nullptr)
        return false;

    char buffer[0x40];
    std::sprintf(buffer, "%d,%d,%d,%d",
                 pRect->X, pRect->Y, pRect->Width, pRect->Height);

    return WriteString(pSection, pKey, buffer);
}

// asm: INIClass_GetEdge (0x475960, retn 0Ch = 3 参数)
//   int __thiscall GetEdge(this, char* string, char* key, int fallback)
// 语义（逐条对齐 asm）：
//   1. GetString(section, key, Null, Dest, 0x80)。
//   2. 返回 0（键不存在）→ 返回 fallback。
//   3. 否则 Direction_Find(Dest)（本项目以 directions[] 表序为索引）。
int32 CCINIClass::GetEdge(const char* pSection, const char* pKey, int32 nDefault)
{
    static const char* const directions[] = {
        "North", "East", "South", "West", "Air"
    };

    char buffer[0x80];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, nullptr, buffer, sizeof(buffer)) <= 0)
        return nDefault;

    for (int32 i = 0; i < static_cast<int32>(sizeof(directions) / sizeof(directions[0])); ++i)
    {
        if (_strcmpi(directions[i], buffer) == 0)
            return i;
    }

    return nDefault;
}

// ============================================================================
// GetAlliesBitfield - INIClass_GetAlliesBitfield
//
//   The value is a comma separated run of house names.  Each name is
//   resolved through the live house array and the corresponding bit (indexed
//   by that house's player number) is accumulated into the result.  If the
//   key produces no text the fallback is returned untouched.
// ============================================================================
// asm: INIClass_GetAlliesBitfield (0x475270, retn 0Ch = 3 参数)
//   uint32 __thiscall GetAlliesBitfield(this, char* section, char* key, int fallback)
// 语义（逐条对齐 asm）：
//   1. esi = fallback；GetString(section, key, Null, Str, 0x80)。
//   2. 返回 0（键不存在）→ 返回 fallback。
//   3. 随即 esi = 0，以 "," 拆名字；键存在但一个 token 都没有 → 返回 0。
//   4. 每个 token 取 HouseClass_FindIndexByName，按位或 `1 << idx`；
//      循环结束后**无条件**返回累加值（即使为 0，不回退 fallback）。
uint32 CCINIClass::GetAlliesBitfield(const char* pSection, const char* pKey,
                                     uint32 nDefault)
{
    char buffer[0x80];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, nullptr, buffer, sizeof(buffer)) == 0)
        return nDefault;

    uint32 mask = 0;

    char* pToken = std::strtok(buffer, ",");
    if (pToken == nullptr)
        return 0;   // asm: 键存在但无 token → 返回 0

    while (pToken != nullptr)
    {
        // asm: HouseClass_FindIndexByName(name) ; shl 1, cl ; or esi
        const int32 index = HouseClass::FindIndexByName(pToken);
        if (index >= 0 && index < 32)
            mask |= (1u << index);

        pToken = std::strtok(nullptr, ",");
    }

    return mask;
}

//========================================================================
// asm: signed int __thiscall INIClass_GetString(CCINIClass *this, const char *section, char *key, char *fallback, char *dest, size_t maxlen)   (asm:650415)
// 形式的便捷层：键存在则覆写，不存在则保留调用者带来的现值。
//========================================================================

// asm 中无 4 参数版本：调用者一律显式传 fallback（多数传空串 Null）。
// 这里以 nullptr 为缺省，等价于原版传空串：未命中时把缓冲清空并返回 0，
// 使 `GetString(...) > 0` 成为可靠的"键存在"判据（旧实现误用 pBuffer
// 自身作缺省值，读未初始化内容，已修正）。
int32 INIClass::GetString(const char* pSection, const char* pKey,
                          char* pBuffer, size_t bufferSize)
{
    return ReadString(pSection, pKey, nullptr, pBuffer, bufferSize);
}

// asm: INIClass_GetString 的二元形态（2 个栈参数）。
//   原版不少地方不落缓冲，而是直接取条目值指针：
//     "if (GetString(section, key)) pType = XXXType_FindOrAllocate(GetString(...))"
//   这里返回内部指针，命中条件与 ReadString 一致：
//   - section / key 为空、节或键不存在、条目无 Value → nullptr；
//   - 否则返回 Value（**指向 INI 内部存储，只读**）。
const char* INIClass::GetString(const char* pSection, const char* pKey)
{
    if (!pSection || !pKey) return nullptr;

    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) return nullptr;

    return entry->Value;
}

void INIClass::GetInteger(const char* pSection, const char* pKey, int32& nValue)
{
    nValue = ReadInteger(pSection, pKey, nValue);
}

void INIClass::GetBool(const char* pSection, const char* pKey, bool& bValue)
{
    bValue = ReadBool(pSection, pKey, bValue);
}

void INIClass::GetFloat(const char* pSection, const char* pKey, float& fValue)
{
    fValue = ReadFloat(pSection, pKey, fValue);
}

void INIClass::GetDouble(const char* pSection, const char* pKey, double& dValue)
{
    dValue = ReadDouble(pSection, pKey, dValue);
}

void INIClass::GetFixed(const char* pSection, const char* pKey, double& dValue)
{
    dValue = ReadFixed(pSection, pKey, dValue);
}

void INIClass::GetIntHundredth(const char* pSection, const char* pKey, int32& nValue)
{
    nValue = ReadIntHundredth(pSection, pKey, nValue);
}

void INIClass::GetLepton(const char* pSection, const char* pKey, int32& nValue)
{
    nValue = ReadLepton(pSection, pKey, nValue);
}

void INIClass::GetPipIdx(const char* pSection, const char* pKey, int32& nValue)
{
    nValue = ReadPipIdx(pSection, pKey, nValue);
}

void INIClass::GetPipscaleIdx(const char* pSection, const char* pKey, int32& nValue)
{
    nValue = ReadPipscaleIdx(pSection, pKey, nValue);
}

//========================================================================
// Section helpers
//========================================================================

const char* INIClass::Section_GetKeyName(const char* pSection, int32 nKeyIndex)
{
    return GetKeyName(pSection, nKeyIndex);
}

// asm: INIClass_Section_GetKeyName (0x5291F0, retn 14h = 5 参数)
//   char* __thiscall Section_GetKeyName(this, section, idx, dest, size, default)
//   逐条对齐：
//     - dest == 0 → 直接返回 null（原版没有判空，但重建侧不写空指针）；
//     - 节缺失 / idx 越界 / 条目无 Key → 走"缺省"分支：
//           strncpy(dest, default ? default : "", size); dest[size-1] = 0;
//           return dest;
//     - 命中 → 同样拷进 dest 后返回 dest。
//   注意与二元版的关键差别：**命中与否都返回 dest**，缺省串也由它落进缓冲，
//   调用方（[Movies]、[InfantryTypes] 等）随后直接拿缓冲当键名用。
const char* INIClass::Section_GetKeyName(const char* pSection, int32 nKeyIndex,
                                         char* pBuffer, size_t bufferSize,
                                         const char* pDefault)
{
    if (!pBuffer || bufferSize == 0) return nullptr;

    const char* pSource = GetKeyName(pSection, nKeyIndex);
    if (pSource == nullptr)
        pSource = pDefault ? pDefault : "";

    std::strncpy(pBuffer, pSource, bufferSize);
    pBuffer[bufferSize - 1] = '\0';
    return pBuffer;
}

int32 INIClass::Section_GetValueCount(const char* pSection)
{
    return GetKeyCount(pSection);
}

//========================================================================
// asm: INIClass::Find_Section (asm:4867, retn 4 = 1 参数)
//
//   原版把节名过一次 CRC32，再在节向量的（按 CRC 排序的）索引里二分：命中就
//   返回该节节点，未命中或名称为空返回 null。重建侧节表是链表 + 名字比较，
//   语义等价：按名（大小写不敏感）返回节对象或 null。所有读取函数内部的
//   "节查找"那一步都等价于调用它。
//========================================================================
INISection* INIClass::Find_Section(const char* pSection)
{
    if (pSection == nullptr || *pSection == '\0')
        return nullptr;

    return GetSection(pSection);
}

//========================================================================
// asm 无独立 Find_Key：键查找内联在 GetString/GetInteger 的 CRC 比对流程
//   里（先 Find_Section 定节，再在节内按键 CRC 命中条目）。这里把同一语义
//   单独暴露出来，等价于 FindEntry。
//========================================================================
INIEntry* INIClass::Find_Key(const char* pSection, const char* pKey) const
{
    return FindEntry(pSection, pKey);
}

int32 INIClass::GetHex(const char* pSection, const char* pKey, int32 nDefault)
{
    return ReadHex(pSection, pKey, nDefault);
}

//========================================================================
// Pointer-form accessors
//========================================================================

bool INIClass::GetInteger_charPP(const char* pSection, const char* pKey, char** pOut)
{
    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value || !pOut) return false;
    *pOut = entry->Value;
    return true;
}

bool INIClass::GetString_charPP(const char* pSection, const char* pKey, char** pOut)
{
    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value || !pOut) return false;
    *pOut = entry->Value;
    return true;
}

// asm: INIClass_GetUnicodeString (0x528520, retn 14h = 5 参数)
//   int __stdcall GetUnicodeString(char* section, char* key, wchar_t* fallback,
//                                  wchar_t* dest, size_t count)
// 语义（逐条对齐 asm）：
//   1. GetString(section, key, Null, Dest, 0x5000) 取 ASCII 串。
//   2. 缺失/空串 → wcsncpy(dest, fallback, count)；dest[count-1]=0；返回 wcslen。
//   3. 否则以 "," 拆 token，逐段 sscanf("%x") 解析成 16 位码点写入 dest，
//      最多 count 段；循环后写终止符并返回 wcslen(dest)。
//   ★ 注意：原版存的是**逗号分隔的十六进制码点**，不是原始 ANSI/转义串。
int32 INIClass::GetUnicodeString(const char* pSection, const char* pKey,
                                 const wchar_t* pDefault, wchar_t* pBuffer, size_t nChars)
{
    if (!pBuffer || nChars == 0) return 0;

    char buffer[0x5000];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, nullptr, buffer, sizeof(buffer)) <= 0)
    {
        // asm: loc_5290B6 —— wcsncpy(dest, fallback, count) 后 wcslen。
        if (pDefault != nullptr)
        {
            std::wcsncpy(pBuffer, pDefault, nChars);
            pBuffer[nChars - 1] = L'\0';
        }
        else
        {
            pBuffer[0] = L'\0';
        }
        return static_cast<int32>(std::wcslen(pBuffer));
    }

    // asm: strtok(Dest, ",") ; sscanf(tok, "%x", &v) ; dest[i] = (wchar_t)v
    size_t count = 0;
    for (char* pToken = std::strtok(buffer, ",");
         pToken != nullptr && count + 1 < nChars;
         pToken = std::strtok(nullptr, ","))
    {
        unsigned int v = 0;
        std::sscanf(pToken, "%x", &v);
        pBuffer[count++] = static_cast<wchar_t>(v & 0xFFFF);
    }

    pBuffer[count] = L'\0';
    return static_cast<int32>(std::wcslen(pBuffer));
}

// asm: INIClass_Read3Floats (0x4762F0, retn 10h = 4 参数)
//   float* __stdcall Read3Floats(float* out, const char* section, const char* key,
//                                const float* fallback)
// 语义（逐条对齐 asm）：
//   1. 先把 fallback 三项原样拷进 out —— 这是键缺失/空值时的最终结果。
//      （asm: mov edx,[arg_C]; mov [out],edx ... 三项 movsd 拷贝）
//   2. GetString(section, key, Null, Str, 0x200)。
//   3. 返回 0（键不存在）→ 直接返回 out（不改）。
//   4. 否则对 Str 以 "," 做 _strtok 拆三段，每段 _atof 后写回 out[0..2]。
//      asm 对每段无条件 atof，故"1"、"1,2"、"1,2,3" 都能解析；缺段这里保留
//      fallback 原值（等价于 asm 的 as-if 行为，且避免对 NULL 调 atof）。
float* INIClass::Read3Floats(float* pBuffer, const char* pSection, const char* pKey,
                             const float* pDefault)
{
    if (!pBuffer) return nullptr;

    if (pDefault)
    {
        pBuffer[0] = pDefault[0];
        pBuffer[1] = pDefault[1];
        pBuffer[2] = pDefault[2];
    }

    char buffer[0x200];
    buffer[0] = '\0';
    if (ReadString(pSection, pKey, nullptr, buffer, sizeof(buffer)) <= 0)
        return pBuffer;

    // asm: strtok(Str, ",") → atof ; strtok(0,",") → atof ; strtok(0,",") → atof
    char* pToken = std::strtok(buffer, ",");
    if (pToken) pBuffer[0] = static_cast<float>(std::atof(pToken));

    pToken = std::strtok(nullptr, ",");
    if (pToken) pBuffer[1] = static_cast<float>(std::atof(pToken));

    pToken = std::strtok(nullptr, ",");
    if (pToken) pBuffer[2] = static_cast<float>(std::atof(pToken));

    return pBuffer;
}

bool INIClass::ReadScenario(const char* pFileName)
{
    if (!pFileName) return false;

    // asm: INIClass_SaveMapPreview
    // 与既有节合并、重复键以后读入的为准。
    CCFileClass file(pFileName);
    if (!file.IsAvailable()) return false;
    return LoadFile(&file);
}

bool INIClass::SaveMapPreview(const char* pSection, const void* pData, int32 nSize)
{
    if (!pSection || !pData || nSize <= 0) return false;

    // asm: INIClass_SaveMapPreview (asm:1197879)
    // "1"、"2"... 逐行写入节内。
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    const uint8* pBytes = static_cast<const uint8*>(pData);
    int32 line = 1;
    char row[80];
    int32 col = 0;

    for (int32 i = 0; i < nSize; i += 3) {
        uint32 trip = static_cast<uint32>(pBytes[i]) << 16;
        if (i + 1 < nSize) trip |= static_cast<uint32>(pBytes[i + 1]) << 8;
        if (i + 2 < nSize) trip |= static_cast<uint32>(pBytes[i + 2]);

        row[col++] = b64[(trip >> 18) & 0x3F];
        row[col++] = b64[(trip >> 12) & 0x3F];
        row[col++] = (i + 1 < nSize) ? b64[(trip >> 6) & 0x3F] : '=';
        row[col++] = (i + 2 < nSize) ? b64[trip & 0x3F] : '=';

        if (col >= 72) {
            row[col] = '\0';
            char key[16];
            snprintf(key, sizeof(key), "%d", line++);
            WriteString(pSection, key, row);
            col = 0;
        }
    }
    if (col > 0) {
        row[col] = '\0';
        char key[16];
        snprintf(key, sizeof(key), "%d", line);
        WriteString(pSection, key, row);
    }
    return true;
}

// ============================================================================
// INIClass::ParseSideHouses  -  asm: INIClass_ParseSideHouses (asm:275442)
//
//   [Sides] 的键是阵营名、值是"逗号分隔的**房屋类型名**"（如
//   `Allied=Americans,British,French,...`）。这个函数只做一件事：
//   把那一串名字翻译成 vec_HouseTypes 里的序号。
//
//     if (!GetString(section, key, "", Str, 0x80))
//         -> 构造空向量并拷给调用者（原版 loc_4769BA 的
//            VectorClass_Append(空) + 回填 length/increment）
//     for (tok = strtok(Str, ","); tok && tok[0]; tok = strtok(NULL, ",")) {
//         i = HouseTypeClass::From_Name(tok);   // 名字 -> 房屋表序号
//         if (i == -1) continue;                // 查不到:丢弃
//         push(i);
//     }
//
//   ★ 与旧实现的三点区别（旧实现是"去空白再回写字符串"，属臆造）：
//     1. 原版**不写回** INI（回写是另一个 proc，在 Get_PKey 附近的
//        INIClass_WriteString 调用链里）；
//     2. 原版**不做** trim —— strtok 按 ',' 切分，名字两边的空格不会被去掉
//        （rules 里的 [Sides] 本来就没有空格）；
//     3. 关键产物是**序号向量**，写回字符串对调用者毫无意义。
//   SideClass::ReadSides 需要的就是这个向量（asm:4246 `VectorClass_Copy`
//   到 side+0x98）。
// ============================================================================
void INIClass::ParseSideHouses(const char* pSection, const char* pKey,
                               DynamicVectorClass<int32>& rList)
{
    if (!pSection || !pKey)
    {
        rList.Clear();
        return;
    }

    char buffer[0x80];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
    {
        rList.Clear();
        return;
    }

    DynamicVectorClass<int32> parsed;

    char* pToken = std::strtok(buffer, ",");
    while (pToken != nullptr && pToken[0] != '\0')
    {
        const int32 idx = HouseTypeClass::FindIndexOfName(pToken);   // asm: HouseTypeClass::From_Name
        if (idx != -1)
            parsed.Add(idx);

        pToken = std::strtok(nullptr, ",");
    }

    rList = parsed;
}

// asm: INIClass_WriteUnicodeEscaped (0x528DC0, retn 0Ch = 3 参数)
//   bool __thiscall WriteUnicodeEscaped(this, char* section, char* key,
//                                       const wchar_t* str)
// 语义（逐条对齐 asm）：
//   1. str == null → 返回 false（asm: bl 恒 0）。
//   2. 分配 wcslen*8+1 缓冲；对每个 wchar 做 _itoa(v & 0xFFFF, buf, 16)，
//      再把 "," 追加到结果尾（strcat）——输出形如 "41,42,"，**带尾逗号**。
//   3. 用 INIClass_WriteString 写回，返回其结果。
//   ★ 与 GetUnicodeString 严格互逆（逗号分隔的十六进制码点串）。
bool INIClass::WriteUnicodeEscaped(const char* pSection, const char* pKey, const wchar_t* pValue)
{
    if (pValue == nullptr)
        return false;

    char buffer[8192];
    size_t len = 0;
    buffer[0] = '\0';

    for (const wchar_t* p = pValue; *p != L'\0'; ++p)
    {
        // asm: _itoa(v & 0FFFFh, buf, 16)
        char tmp[16];
        std::snprintf(tmp, sizeof(tmp), "%x",
                      static_cast<unsigned int>(
                          static_cast<unsigned short>(*p) & 0xFFFF));
        const size_t n = std::strlen(tmp);

        if (len + n + 1 >= sizeof(buffer))
            break;

        std::memcpy(buffer + len, tmp, n);
        len += n;
        buffer[len++] = ',';   // asm: strcat(buf, ",")
    }

    buffer[len] = '\0';
    return WriteString(pSection, pKey, buffer);
}

bool INIClass::WriteBool_charPP(const char* pSection, const char* pKey, const bool* pValue)
{
    if (!pValue) return false;
    return WriteBool(pSection, pKey, *pValue);
}

bool INIClass::WriteString_charPP(const char* pSection, const char* pKey, char* const* pValue)
{
    if (!pValue || !*pValue) return false;
    return WriteString(pSection, pKey, *pValue);
}

bool INIClass::WriteInteger_charPP(const char* pSection, const char* pKey, const int32* pValue)
{
    if (!pValue) return false;
    return WriteInteger(pSection, pKey, *pValue);
}

bool INIClass::Write2Integers_charPP(const char* pSection, const char* pKey, const int32* pValues)
{
    if (!pValues) return false;
    return Write2Integers(pSection, pKey, pValues);
}

bool INIClass::Write3Floats_charPP(const char* pSection, const char* pKey, const float* pValues)
{
    if (!pValues) return false;

    // 根据游戏行为，可知三浮点写出口与读入口对称："x,y,z" 逗号串。
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%f,%f,%f",
             static_cast<double>(pValues[0]),
             static_cast<double>(pValues[1]),
             static_cast<double>(pValues[2]));
    return WriteString(pSection, pKey, buffer);
}

//========================================================================
// CCINIClass - typed getters by name lookup
//========================================================================

TechnoTypeClass* CCINIClass::FindTechnoTypeByName(const char* pSection, const char* pKey)
{
    char buffer[256];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0' || IsBlank(buffer)) return nullptr;
    return TechnoTypeClass::Find(buffer);
}

// ============================================================================
// CCINIClass::GetTechnoPrerequisite  -  asm: INIClass_GetTechnoPrerequisite
//                                       (asm:273851, retn 0Ch)
//
//   与 GetPrerequisiteList 不同：这里产出的是**32 位掩码**，不是索引向量。
//   原版控制流：
//
//     if (!GetString(section, key, "", Str, 0x80)) return fallback;
//     mask = 0;
//     for (tok = strtok(Str, ","); tok && tok[0]; tok = strtok(NULL, ",")) {
//         if (!_strcmpi(tok, "POWER"))    mask |= 0x80000000;
//         if (!_strcmpi(tok, "FACTORY"))  mask |= 0x40000000;
//         if (!_strcmpi(tok, "BARRACKS")) mask |= 0x20000000;
//         if (!_strcmpi(tok, "RADAR"))    mask |= 0x10000000;
//         i = BuildingTypeClass_FindIndex(tok);
//         if (i != -1) mask |= (1u << i);
//     }
//     return mask;
//
//   注意四个关键字检查是并列 if：一个 token 至多命中一个，效果等价 else-if。
//   高 4 位留给关键字、低 28 位留给建筑索引位（原版就是直接 shl，越界建筑索引
//   会与关键字位重叠，这里照搬不额外防护）。
// ============================================================================
uint32 CCINIClass::GetTechnoPrerequisite(const char* pSection, const char* pKey,
                                         uint32 nDefault)
{
    char buffer[0x80];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return nDefault;            // asm: loc_475B55 -> 返回 arg_8

    uint32 mask = 0;

    char* pToken = std::strtok(buffer, ",");
    while (pToken != nullptr && pToken[0] != '\0')
    {
        if (_strcmpi(pToken, "POWER") == 0)    mask |= 0x80000000u;
        if (_strcmpi(pToken, "FACTORY") == 0)  mask |= 0x40000000u;
        if (_strcmpi(pToken, "BARRACKS") == 0) mask |= 0x20000000u;
        if (_strcmpi(pToken, "RADAR") == 0)    mask |= 0x10000000u;

        const int32 idx = BuildingTypeClass::FindIndex(pToken);
        if (idx != -1)
            mask |= (1u << idx);

        pToken = std::strtok(nullptr, ",");
    }

    return mask;
}

AircraftTypeClass* CCINIClass::GetAircraftType(const char* pSection, const char* pKey,
                                               AircraftTypeClass* pDefault)
{
    // 根据游戏行为，可知按名字找到科技类型后还要校验它确实属于该类别，
    // 类别不符视为没找到。
    TechnoTypeClass* pType = FindTechnoTypeByName(pSection, pKey);
    if (pType == nullptr || pType->GetClassID() != AbstractType::AircraftType) {
        return pDefault;
    }
    return reinterpret_cast<AircraftTypeClass*>(pType);
}

BuildingTypeClass* CCINIClass::GetBuildingType(const char* pSection, const char* pKey,
                                               BuildingTypeClass* pDefault)
{
    TechnoTypeClass* pType = FindTechnoTypeByName(pSection, pKey);
    if (pType == nullptr || pType->GetClassID() != AbstractType::BuildingType) {
        return pDefault;
    }
    return reinterpret_cast<BuildingTypeClass*>(pType);
}

InfantryTypeClass* CCINIClass::GetInfantryType(const char* pSection, const char* pKey,
                                               InfantryTypeClass* pDefault)
{
    TechnoTypeClass* pType = FindTechnoTypeByName(pSection, pKey);
    if (pType == nullptr || pType->GetClassID() != AbstractType::InfantryType) {
        return pDefault;
    }
    return reinterpret_cast<InfantryTypeClass*>(pType);
}

UnitTypeClass* CCINIClass::GetUnitType(const char* pSection, const char* pKey,
                                       UnitTypeClass* pDefault)
{
    TechnoTypeClass* pType = FindTechnoTypeByName(pSection, pKey);
    if (pType == nullptr || pType->GetClassID() != AbstractType::UnitType) {
        return pDefault;
    }
    return reinterpret_cast<UnitTypeClass*>(pType);
}

TerrainTypeClass* CCINIClass::GetTerrainType(const char* pSection, const char* pKey,
                                             TerrainTypeClass* pDefault)
{
    char buffer[256];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0' || IsBlank(buffer)) return pDefault;

    TerrainTypeClass* pType = TerrainTypeClass::Find(buffer);
    return pType ? pType : pDefault;
}

WarheadTypeClass* CCINIClass::GetWarheadType(const char* pSection, const char* pKey,
                                            WarheadTypeClass* pDefault)
{
    char buffer[256];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0' || IsBlank(buffer)) return pDefault;

    WarheadTypeClass* pType = WarheadTypeClass::Find(buffer);
    return pType ? pType : pDefault;
}

// asm: INIClass_GetSide (0x475720, retn 0Ch = 3 参数)
//   int __thiscall GetSide(this, char* section, char* key, int fallback)
// 语义（逐条对齐 asm）：
//   1. GetString(section, key, Null, name, 0x80)；返回 0（键不存在）→ fallback。
//   2. Side_From_Name(name)；命中（!= -1）直接返回该序号。
//   3. 未命中：new SideClass(name) 并登记，再在 vec_Sides 里按指针线性查找，
//      命中返回下标，找不到返回 -1。
int32 CCINIClass::GetSide(const char* pSection, const char* pKey, int32 nDefault)
{
    char buffer[0x80];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, nullptr, buffer, sizeof(buffer)) <= 0)
        return nDefault;

    int32 index = SideClass::From_Name(buffer);
    if (index != -1)
        return index;

    // asm: 名字未知 → 新建 SideClass 并登记，随后在 Sides 表中定位序号。
    SideClass* pNew = SideClass::FindOrAllocate(buffer);
    if (pNew != nullptr && SideClass::Sides != nullptr)
    {
        for (int32 i = 0; i < SideClass::Sides->Count; ++i)
        {
            if ((*SideClass::Sides)[i] == pNew)
                return i;
        }
    }

    return -1;
}

// asm: INIClass_GetTheme (0x475900, retn 0Ch = 3 参数)
//   int __thiscall GetTheme(this, char* string, char* key, int fallback)
// 语义（逐条对齐 asm）：
//   1. GetString(section, key, Null, Dest, 0x80)。
//   2. 返回 0（键不存在）→ 返回 fallback。
//   3. 否则 ThemeClass::From_Name(全局 Theme, Dest)（本项目等价入口
//      ThemeClass::FindIndex），未命中回落 fallback。
int32 CCINIClass::GetTheme(const char* pSection, const char* pKey, int32 nDefault)
{
    char buffer[0x80];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, nullptr, buffer, sizeof(buffer)) <= 0)
        return nDefault;

    int32 index = ThemeClass::FindIndex(buffer);
    return (index >= 0) ? index : nDefault;
}

// asm: INIClass_GetVoxIndex (0x474F90, retn 0Ch = 3 参数)
//   int __thiscall GetVoxIndex(this, char* section, char* key, int fallback)
// 语义：GetString(section, key, Null, dest, 0x64)；读到长度为 0（键不存在）→
//   返回 fallback；否则 VoxClass_FindIndex(dest)（本项目等价入口为
//   VocClass::FindIndex），未命中回落 fallback。
int32 CCINIClass::GetVoxIndex(const char* pSection, const char* pKey, int32 nDefault)
{
    char buffer[0x64];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, nullptr, buffer, sizeof(buffer)) <= 0)
        return nDefault;

    int32 index = VocClass::FindIndex(buffer);
    return (index >= 0) ? index : nDefault;
}

// asm: INIClass_GetColorSchemeIdx (0x474A90, retn 0Ch = 3 参数)
//   int __thiscall GetColorSchemeIdx(this, char* section, char* key, int fallback)
// 语义（逐条对齐 asm）：
//   1. 缺省名取 ColorScheme.Array[fallback]->ID（asm: mov eax,[edx+ebx*4];
//      mov edx,[eax+304h]，0x304 即 ColorScheme::ID）。
//   2. GetString(section, key, 缺省名, Str2, 0x20)。
//   3. 以 ColorScheme.length 为界遍历，__strcmpi(Array[i]->ID, Str2)==0
//      且 Array[i]->ShadeCount != 1 时返回 i（asm: cmp [ecx+310h],1）。
//   4. 未命中返回 fallback 原值（asm: mov eax, ebx）。
int32 CCINIClass::GetColorSchemeIdx(const char* pSection, const char* pKey, int32 nDefault)
{
    if (ColorScheme::Array == nullptr || ColorScheme::Array->Count <= 0)
        return nDefault;

    int32 idx = nDefault;
    if (idx < 0 || idx >= ColorScheme::Array->Count)
        idx = 0;

    ColorScheme* pDef = (*ColorScheme::Array)[idx];
    const char* pFallback = (pDef && pDef->ID) ? pDef->ID : "";

    char buffer[0x20];
    buffer[0] = '\0';
    ReadString(pSection, pKey, pFallback, buffer, sizeof(buffer));

    for (int32 i = 0; i < ColorScheme::Array->Count; ++i)
    {
        ColorScheme* pScheme = (*ColorScheme::Array)[i];
        if (pScheme == nullptr || pScheme->ID == nullptr)
            continue;

        if (_strcmpi(pScheme->ID, buffer) != 0)
            continue;

        // asm: ShadeCount==1 的条目视作无效，跳过。
        if (pScheme->ShadeCount == 1)
            continue;

        return i;
    }

    return nDefault;
}

int32 CCINIClass::GetCategoryIdx(const char* pSection, const char* pKey, int32 nDefault)
{
    return static_cast<int32>(GetCategory(pSection, pKey, static_cast<Category>(nDefault)));
}

// asm: INIClass_GetSWTypeIndex (0x474F40, retn 0Ch = 3 参数)
//   int __thiscall GetSWTypeIndex(this, char* section, char* key, int idx)
// 语义（逐条对齐 asm）：
//   1. idx != -1 时缺省名取 vec_SuperWeaponTypes[idx]->ID（asm: [eax+24h]）；
//      idx == -1 时缺省为空串（asm: 局部 0 字节）。
//   2. GetString(section, key, 缺省名, dest, 0x20)。
//   3. return SuperWeaponTypeClass::FindIndex(dest)（按 ID 在表中查序号）。
int32 CCINIClass::GetSWTypeIndex(const char* pSection, const char* pKey, int32 nDefault)
{
    const char* pFallback = "";

    if (nDefault != -1 &&
        nDefault >= 0 && nDefault < SuperWeaponTypeClass::GetCount())
    {
        SuperWeaponTypeClass* pType = SuperWeaponTypeClass::FindByIndex(nDefault);
        if (pType != nullptr && pType->ID[0] != '\0')
            pFallback = pType->ID;
    }

    char buffer[0x20];
    buffer[0] = '\0';
    ReadString(pSection, pKey, pFallback, buffer, sizeof(buffer));

    int32 index = SuperWeaponTypeClass::FindIndex(buffer);
    return (index >= 0) ? index : nDefault;
}

int32 CCINIClass::FindHouseIndex(const char* pSection, const char* pKey)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return -1;

    // 根据游戏行为，可知国家按 ID 名在注册表里查序号，未知名得 -1。
    for (int32 i = 0; i < HouseTypeClass::ArrayCount; ++i) {
        HouseTypeClass* pType = HouseTypeClass::Array[i];
        if (pType && pType->ID[0] && strcasecmp(pType->ID, buffer) == 0) return i;
    }
    return -1;
}

// ============================================================================
// asm INIClass_HouseType_FindOrAllocate_YesMP  (真实 0x475540, retn 0Ch)
//
//   ; int __thiscall INIClass_HouseType_FindOrAllocate_YesMP(
//         CCINIClass *this, char *section, char *key, int fallback)
//
// 反汇编流程：
//   1. 取 0x80 字节名字缓冲，GetString(section, key, nullptr, name, 0x80)；
//      返回值（读到长度）为 0 时直接返回 fallback。
//   2. 按名字找玩家项 HouseClass_FindPlayerAtX(name) → esi；
//      若 HouseClass_IsIdxMP(esi) 为真，直接返回 esi（该序号本身就是
//      七个多人专用国之一）。
//   3. 否则 HouseTypeClass::From_Name(name)；不等于 -1 时直接返回该序号。
//   4. 等于 -1 时 new HouseTypeClass + CTOR(name)，然后在 vec_HouseTypes
//      里按指针线性查找，命中返回下标，未命中返回 -1。
//
// ⚠ 依赖留白说明：
//   项目 `Houses/HouseClass.cpp:5199 HouseClass::FindPlayerAtX(int32 index)`
//   是**错误移植**（源码自身注释即写着 `__fastcall HouseClass_FindPlayerAtX
//   (char *name)`，但函数体却在做 GetHouseByIndex + !Defeated）。asm 真身
//   (0x510FA0) 做的是「字面名 → 国号」映射：逐字比较 "<Player @ A>" ..
//   "<Player @ H>"，命中返回 0x117B..0x1182，全部未命中返回
//   (0xFFFFFFFF & 0xFFFFEE7D) + 0x1182 == 0x1100。按铁律①（宁可留白，
//   不写错），此处不调用那个错版，改为在本地按 asm 语义内联这段映射。
//   待 HouseClass::FindPlayerAtX 被修正为正确的自由函数语义后，可回填替换。
static int32 INIClass_PlayerNameToIndex(const char* pName)
{
    static const char* const kNames[] = {
        "<Player @ A>", "<Player @ B>", "<Player @ C>", "<Player @ D>",
        "<Player @ E>", "<Player @ F>", "<Player @ G>", "<Player @ H>",
    };
    if (pName) {
        for (int32 i = 0; i < 8; ++i) {
            if (strcasecmp(pName, kNames[i]) == 0) return 0x117B + i;
        }
    }
    return 0x1100;  // asm: 未命中 → (0xFFFFFFFF & 0xFFFFEE7D) + 0x1182
}

int32 CCINIClass::HouseType_FindOrAllocate_YesMP(const char* pSection,
                                                 const char* pKey,
                                                 int32 nDefault)
{
    char buffer[0x80];
    buffer[0] = '\0';
    int32 len = GetString(pSection, pKey, buffer, sizeof(buffer));
    if (len == 0) return nDefault;

    // 先按名字取玩家项：命中且属于多人专用国槽位时直接返回序号。
    int32 idxHouse = INIClass_PlayerNameToIndex(buffer);
    if (HouseClass::Is_Idx_MP(idxHouse)) return idxHouse;

    // 退回按国家类型名查找：命中则原样返回序号（不置 Multiplay 标志）。
    int32 idxType = HouseTypeClass::FindIndexOfName(buffer);
    if (idxType != -1) return idxType;

    // 未命中：分配一个多人可用国家类型，返回其新序号；若分配失败按 -1 收尾。
    HouseTypeClass* pAllocated = HouseTypeClass::FindOrAllocate_YesMP(buffer);
    if (!pAllocated) return -1;
    return HouseTypeClass::FindIndexOfName(buffer);
}

LandType CCINIClass::GetLand(const char* pSection, const char* pKey, LandType nDefault)
{
    return GetLandType(pSection, pKey, nDefault);
}

// asm: INIClass_GetTheater (0x475880, retn 0Ch = 3 参数)
//   int __thiscall GetTheater(this, char* section, char* key, int fallback)
// 语义（逐条对齐 asm）：
//   1. GetString(section, key, Null, dest, 0x80)。
//   2. 返回 0（键不存在）→ 返回 fallback。
//   3. 否则 Theater_FindIndex(dest)（本项目以 theaters[] 表序为索引）。
TheaterType CCINIClass::GetTheater(const char* pSection, const char* pKey, TheaterType nDefault)
{
    static const char* const theaters[] = {
        "TEMPERATE", "SNOW", "URBAN", "DESERT", "LUNAR", "NEWURBAN"
    };

    char buffer[0x80];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, nullptr, buffer, sizeof(buffer)) <= 0)
        return nDefault;

    for (int32 i = 0; i < 6; ++i)
    {
        if (strcasecmp(theaters[i], buffer) == 0)
            return static_cast<TheaterType>(i);
    }
    return nDefault;
}

// asm: INIClass_GetFactory (0x474FD0, retn 0Ch = 3 参数)
//   int __thiscall GetFactory(this, char* string, char* key, int derivationID)
// 语义（逐条对齐 asm）：
//   1. 缺省名取 AbstractDerivationID_GetClassName(derivationID)（生产类别名，
//      即 "BuildingType" / "InfantryType" / "UnitType" / "AircraftType"）。
//   2. GetString(section, key, 缺省名, dest, 0x20)。
//   3. return AbstractDerivationID_GetID(dest)（名字 → AbstractType）。
AbstractType CCINIClass::GetFactory(const char* pSection, const char* pKey, AbstractType nDefault)
{
    static const struct {
        const char*  Name;
        AbstractType Value;
    } factories[] = {
        { "BuildingType", AbstractType::BuildingType },
        { "InfantryType", AbstractType::InfantryType },
        { "UnitType",     AbstractType::UnitType },
        { "AircraftType", AbstractType::AircraftType },
    };

    // asm: AbstractDerivationID_GetClassName(derivationID)
    const char* pFallback = factories[0].Name;
    for (const auto& f : factories)
    {
        if (f.Value == nDefault) { pFallback = f.Name; break; }
    }

    char buffer[0x20];
    buffer[0] = '\0';
    ReadString(pSection, pKey, pFallback, buffer, sizeof(buffer));

    // asm: AbstractDerivationID_GetID(dest)
    for (const auto& f : factories)
    {
        if (_strcmpi(f.Name, buffer) == 0)
            return f.Value;
    }
    return nDefault;
}

int32 CCINIClass::AbilityNameToIdx(const char* pValue)
{
    // 根据游戏行为，可知能力值以数字位串出现，位序即能力索引；
    // 无法解析的名字一律得 0（无能力）。
    if (!pValue || !*pValue) return 0;
    return atoi(pValue);
}

int32 CCINIClass::SpeedTypeNameToIdx(const char* pValue)
{
    return static_cast<int32>(ParseSpeedType(pValue));
}

//========================================================================
// CCINIClass - typed writers
//========================================================================

bool CCINIClass::WriteSpeedType(const char* pSection, const char* pKey, SpeedType nValue)
{
    return WriteString(pSection, pKey, SpeedTypeIdxToName(static_cast<int32>(nValue)));
}

bool CCINIClass::WriteMovementZone(const char* pSection, const char* pKey, MovementZone nValue)
{
    return WriteString(pSection, pKey, MovementZoneIdxToName(static_cast<int32>(nValue)));
}

// asm: INIClass_WriteArmor (0x475450, __stdcall, retn 0Ch = 3 参数)
//   bool __stdcall WriteArmor(char* section, char* key, int a4)
// 语义：直接写 strlist_ArmorTypes[a4]（本项目 ArmorTypes[]）；asm 不做边界检查，
//   这里补一个越界保护（越界回落到 none）。
bool CCINIClass::WriteArmor(const char* pSection, const char* pKey, Armor nValue)
{
    int32 idx = static_cast<int32>(nValue);
    if (idx < 0 || idx >= static_cast<int32>(Armor::Count))
        idx = 0;
    return WriteString(pSection, pKey, ArmorTypes[idx]);
}

bool CCINIClass::WriteLand(const char* pSection, const char* pKey, LandType nValue)
{
    return WriteString(pSection, pKey, LandTypeIdxToName(static_cast<int32>(nValue)));
}

bool CCINIClass::WriteFoundation(const char* pSection, const char* pKey, Foundation nValue)
{
    return WriteString(pSection, pKey, FoundationIdxToName(static_cast<int32>(nValue)));
}

bool CCINIClass::WriteBuildCat(const char* pSection, const char* pKey, BuildCat nValue)
{
    return WriteString(pSection, pKey, BuildCatIdxToName(static_cast<int32>(nValue)));
}

// asm: INIClass_WriteTheater (0x4758E0, __stdcall, retn 0Ch = 3 参数)
//   bool __stdcall WriteTheater(char* section, char* key, int idx)
// 语义：写 theatreData[idx * 0x70] 处的环境名（本项目以 theaters[] 表序取）。
bool CCINIClass::WriteTheater(const char* pSection, const char* pKey, TheaterType nValue)
{
    static const char* const theaters[] = {
        "TEMPERATE", "SNOW", "URBAN", "DESERT", "LUNAR", "NEWURBAN"
    };
    int32 idx = static_cast<int32>(nValue);
    if (idx < 0 || idx > 5) idx = 0;
    return WriteString(pSection, pKey, theaters[idx]);
}

bool CCINIClass::WriteFactory(const char* pSection, const char* pKey, AbstractType nValue)
{
    // 根据游戏行为，可知生产类别以类型 ID 名串写回。
    const char* pName = "BuildingType";
    switch (nValue) {
    case AbstractType::InfantryType: pName = "InfantryType"; break;
    case AbstractType::UnitType:     pName = "UnitType";     break;
    case AbstractType::AircraftType: pName = "AircraftType"; break;
    default: break;
    }
    return WriteString(pSection, pKey, pName);
}

bool CCINIClass::WriteCategoryIdx(const char* pSection, const char* pKey, int32 nIndex)
{
    return WriteString(pSection, pKey, CategoryIdxToName(nIndex));
}

// asm: INIClass_WriteColorScheme (0x474A20, __stdcall, retn 0Ch = 3 参数)
//   bool __stdcall WriteColorScheme(char* section, char* key, int idx)
// 语义：以 ColorScheme.Array[idx]->ID（偏移 0x304）写回，**不是**把整数
//   写出去。asm 直接索引不做边界检查，这里补一个越界保护后写空串。
bool CCINIClass::WriteColorScheme(const char* pSection, const char* pKey, int32 nIndex)
{
    if (ColorScheme::Array != nullptr &&
        nIndex >= 0 && nIndex < ColorScheme::Array->Count)
    {
        ColorScheme* pScheme = (*ColorScheme::Array)[nIndex];
        if (pScheme != nullptr && pScheme->ID != nullptr)
            return WriteString(pSection, pKey, pScheme->ID);
    }

    return WriteString(pSection, pKey, "");
}

// asm: INIClass_WriteHouseIndex (0x475620, retn 0Ch = 3 参数)
//   bool __thiscall WriteHouseIndex(this, char* section, char* key, int idxHouse)
// 语义（逐条对齐 asm）：
//   1. 先把 value 复制为 "<none>"（asm: rep movsd/movsb 复制字面串）。
//   2. HouseClass_IsIdxMP(idx) 为真 → HouseClass_NameFromIdx(idx) 填入 value。
//   3. 否则 idx==-2 或 idx==-3 → "<random>"。
//   4. 否则 idx==-1 → 保持 "<none>"。
//   5. 否则 0<=idx<vec_HouseTypes.length → 该房屋类型的 nameINI（ID）。
//   6. 其余越界情况保持 "<none>"；最后 INIClass_WriteString 写回。
bool CCINIClass::WriteHouseIndex(const char* pSection, const char* pKey, int32 nIndex)
{
    char value[0x80];
    std::strcpy(value, "<none>");

    if (HouseClass::Is_Idx_MP(nIndex))
    {
        // asm: HouseClass_NameFromIdx(idx, value, 0x80)
        const char* pName = HouseClass::Name_From_Idx(nIndex, 0);
        if (pName != nullptr)
        {
            std::strncpy(value, pName, sizeof(value) - 1);
            value[sizeof(value) - 1] = '\0';
        }
    }
    else if (nIndex == -2 || nIndex == -3)
    {
        // asm: cmp esi,0FFFFFFFEh / 0FFFFFFFDh → "<random>"
        std::strcpy(value, "<random>");
    }
    else if (nIndex == -1)
    {
        // asm: 保持 "<none>"
    }
    else if (nIndex >= 0 && nIndex < HouseTypeClass::ArrayCount &&
             HouseTypeClass::Array[nIndex] != nullptr)
    {
        // asm: [HouseTypeClass+nameINI] —— 即类型 ID。
        const char* pName = HouseTypeClass::Array[nIndex]->get_ID();
        if (pName != nullptr)
        {
            std::strncpy(value, pName, sizeof(value) - 1);
            value[sizeof(value) - 1] = '\0';
        }
    }

    return WriteString(pSection, pKey, value);
}

// asm: INIClass_WritePipscaleIdx (0x4749B0, retn 0Ch)
//   idx==0 → 返回 false；否则写 PipScales.string[idx]（asm 按位置索引表，
//   越界在此做保护）。
bool CCINIClass::WritePipscaleIdx(const char* pSection, const char* pKey, int32 nIndex)
{
    const int32 scaleCount = static_cast<int32>(sizeof(g_PipScales) / sizeof(g_PipScales[0]));
    if (nIndex <= 0 || nIndex >= scaleCount) return false;
    return WriteString(pSection, pKey, g_PipScales[nIndex].Name);
}

//========================================================================
// CLSID / string-table / voice-list
//========================================================================

bool CCINIClass::ReadCLSID(const char* pSection, const char* pKey, GUID* pOut)
{
    if (!pOut) return false;

    // CLSID 串两种形态都接受：带花括号 "{xxxxxxxx-xxxx-...}" 与不带。
    // 先剥花括号/尾空白，再按标准八段拆。
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return false;

    char* p = buffer;
    if (*p == '{') ++p;
    char* end = p + strlen(p);
    while (end > p && (end[-1] == '}' || end[-1] == ' ')) --end;
    *end = '\0';

    // ★ 类型必须与格式符严格配对：%4hX 要 unsigned short*、%2hhX 要
    //   unsigned char*。旧代码把它们直接指到 unsigned int* 上，是未定义行为
    //   （小端下"碰巧"只写低字节，换了编译器/优化级别就可能读到垃圾）。
    unsigned int  d1 = 0;
    unsigned short d2 = 0, d3 = 0;
    unsigned char  b[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    if (sscanf(p, "%8X-%4hX-%4hX-%2hhX%2hhX-%2hhX%2hhX%2hhX%2hhX%2hhX%2hhX",
               &d1, &d2, &d3,
               &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7]) != 11) {
        return false;
    }

    pOut->Data1 = d1;
    pOut->Data2 = static_cast<uint16>(d2);
    pOut->Data3 = static_cast<uint16>(d3);
    for (int i = 0; i < 8; ++i) {
        pOut->Data4[i] = static_cast<uint8>(b[i]);
    }
    return true;
}

bool CCINIClass::WriteCLSID(const char* pSection, const char* pKey, const GUID* pValue)
{
    if (!pValue) return false;

    // 根据游戏行为，可知写出口统一为带花括号的大写完整式。
    char buffer[64];
    snprintf(buffer, sizeof(buffer),
             "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
             pValue->Data1, pValue->Data2, pValue->Data3,
             pValue->Data4[0], pValue->Data4[1],
             pValue->Data4[2], pValue->Data4[3], pValue->Data4[4],
             pValue->Data4[5], pValue->Data4[6], pValue->Data4[7]);
    return WriteString(pSection, pKey, buffer);
}

int32 CCINIClass::GetStringtableEntry(const char* pSection, const char* pKey,
                                      char* pBuffer, size_t bufferSize)
{
    return ReadStringTableEntry(pSection, pKey, pBuffer, bufferSize);
}

bool CCINIClass::Get_Vector_Voc_525430(const char* pSection, const char* pKey,
                                       DynamicVectorClass<int32>& rList)
{
    rList.Clear();

    // asm INIClass_Get_Vector_Voc_525430 (0x525430):
    //   char Str[0x80]; if (!GetString(INI, section, key, 0, Str, 0x80))
    //                     { *this = empty; return this; }
    //   for (tok = strtok(Str, ",") ; tok ; tok = strtok(0, ",")):
    //     if (*tok):
    //       voc = VocClass_Find(tok)
    //       if (voc):
    //         idx = VocClass_FindIndex(voc)
    //         if (idx != 0):  local.append(idx)     <-- 0 视为无效项跳过
    //   VectorClass_Append(this, &local)
    //
    // 注意：原版 VocClass_FindIndex 的索引以 1 为基，0 是"无效/未注册"
    // 哨兵，故 idx==0 被丢弃。本项目 VocClass::FindIndexOfName 以 0 为基
    // （-1 才是未找到），所以等价判据是"名字查得到就追加"——即 idx >= 0。
    // 原版"跳过未知名"的意图在这里由 FindIndexOfName 返回 -1 直接表达，
    // 无需再额外排除 0。
    char buffer[512];
    buffer[0] = '\0';
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return false;

    char* context = nullptr;
    (void)context;
    for (char* tok = strtok(buffer, ","); tok; tok = strtok(nullptr, ",")) {
        while (*tok == ' ' || *tok == '\t') ++tok;
        char* end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) --end;
        *end = '\0';
        if (*tok == '\0') continue;

        // asm: VocClass_Find 命中后取索引；未知名直接跳过、不占位。
        int32 index = VocClass::FindIndexOfName(tok);
        if (index >= 0) {
            rList.AddItem(index);
        }
    }
    return true;
}

// ============================================================================
// asm: Overlay_Pack_Type_SaveToINI (0x5FD680)
//
//   调用点：ScenarioClass_SaveMap，紧跟在 BuildingClass_SaveToMapINIList 之后，
//   传的 ecx 是栈上的 CCINIClass（不是 ScenarioClass）。
//
//   两趟完全同构的管线，只是取字节的字段不同：
//     "OverlayPack"     -> cell + 0x44  (CellClass::Overlay)
//     "OverlayDataPack" -> cell + 0x11E (CellClass::OverlayFrame)
//   asm 里每格写 1 字节进 Base64Pipe，再整块 Put_UUBlock；本项目的
//   WriteUUBlock 内部就做 Base64 + 0x46 分行，故直接交原始字节。
// ============================================================================
namespace
{
    // 逐格收集一张 512x512 的字节图（asm: 固定的 200h x 200h 双循环）。
    // dataLayer == false 取 Overlay，true 取 OverlayFrame。
    int32 Collect_Cell_Map(bool dataLayer, uint8* pOut)
    {
        MapClass* pMap = MapClass::Instance;
        if (pMap == nullptr)
            return 0;

        int32 total = 0;
        for (int32 y = 0; y < 0x200; ++y) {
            for (int32 x = 0; x < 0x200; ++x) {
                // asm: MapClass::Coord_Cell(Map, &coords) 之后再 +0x44 / +0x11E
                CellClass* pCell = pMap->TryGetCellAt(x, y);
                uint8 value = 0;
                if (pCell != nullptr) {
                    value = dataLayer
                        ? pCell->OverlayFrame
                        : static_cast<uint8>(pCell->Overlay);
                }
                pOut[total++] = value;      // asm: sub_5520A0 返回 1（每格一字节）
            }
        }
        return total;
    }
}

bool CCINIClass::Put_Overlay_Pack()
{
    static uint8 buffer[0x200 * 0x200];

    // ---- OverlayPack -------------------------------------------------------
    // asm: mangleStuff_("OVERLAY",0) + mangleStuff_("OverlayPack",0)
    int32 total = Collect_Cell_Map(false, buffer);
    bool wrote = false;
    if (total != 0) {
        wrote = Put_UUBlock("OverlayPack", buffer, static_cast<size_t>(total));
    }

    // ---- OverlayDataPack ---------------------------------------------------
    total = Collect_Cell_Map(true, buffer);
    if (total != 0) {
        wrote = Put_UUBlock("OverlayDataPack", buffer, static_cast<size_t>(total)) || wrote;
    }

    return wrote;
}
