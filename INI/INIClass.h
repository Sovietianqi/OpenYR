#pragma once

#include "Core/Definitions.h"
#include "../Containers/DynamicVectorClass.h"
#include "Core/Macros.h"
#include "Core/Memory.h"
#include "Containers/ListClass.h"
#include "IO/CCFileClass.h"

#include <cstring>
#include <cstdlib>
#include <cctype>

//========================================================================
// INIClass - INI file parser
//
// Parses and manages INI-format configuration files. The format is:
//
//   [SectionName]
//   Key=Value
//   ; Comment
//
// This is the primary configuration format used by C&C games.
// The parser supports:
// - Section and key lookup by name
// - Reading/writing strings, integers, booleans, floats, doubles
// - Reading/writing vectors (2D, 3D)
// - Reading/writing colors
// - Comment preservation
// - CRC-based section indexing for fast lookup
//========================================================================

//========================================================================
// Forward declarations
//========================================================================

class INIClass;

class AircraftTypeClass;
class BuildingTypeClass;
class InfantryTypeClass;
class TerrainTypeClass;
class UnitTypeClass;

enum class Powerup : uint32;

//========================================================================
// INIIntrusiveList - 侵入式双向链表（节点就是对象本身）
//
//   INIEntry / INISection 都以 Node<T> 为基类，对象自己带 Next / Prev，
//   原版 ListClass<T> 因此不需要额外的节点开销，遍历直接走 `p->Next`。
//
//   本工程通用的 List<T>（Containers/ListClass.h）是"自持节点"形态：AddTail
//   另 new 一个空节点、把元素丢掉，First() 又把那个空节点当元素返回。这套
//   语义与 INI 的侵入式用法不兼容（LoadFromBuffer 会在第一次建条目时崩），
//   所以 INI 层改用下面这个容器，只实现 INI 需要的三个动作。
//========================================================================

template <typename TNode>
class INIIntrusiveList
{
public:
    TNode* Head;
    TNode* Tail;
    int32  Count;

    // ------------------------------------------------------------------------
    // asm: list_INIClass_INISection_CTOR (0x55A8C0) —— List<INISection*> 的
    //   构造：本对象 +0x04 / +0x10 是两个内嵌哨兵 Node，先把两个哨兵的
    //   Next/Prev 清零、各自盖上 GenericNode vtable，再用 GenericList vtable
    //   盖头，然后交叉链接成环：
    //     tmp = First.Next ; Last.Next = First ; Last.Prev = tmp ;
    //     tmp ? tmp.Prev = Last ; First.Next = Last
    //   最后把 List<INISection*> vtable 盖到 [this] 上。
    //
    //   降级点：本类的链表用 Head/Tail 指针 + Count（不内嵌哨兵节点），空表
    //   的语义（First()/Size() 一致）相同；哨兵环的链接动作没有对应物，
    //   故只保留"空表"这一可观察状态。
    // ------------------------------------------------------------------------
    INIIntrusiveList() : Head(nullptr), Tail(nullptr), Count(0) {}

    // 尾部追加；pItem 必须非空，且不能已在别的链上。
    void AddTail(TNode* pItem)
    {
        if (pItem == nullptr) return;

        pItem->Next = nullptr;
        pItem->Prev = Tail;
        if (Tail != nullptr) Tail->Next = pItem;
        else                 Head = pItem;
        Tail = pItem;
        ++Count;
    }

    TNode* First() const { return Head; }
    int32  GetCount() const { return Count; }
    int32  Size() const { return Count; }

    // 摘链归零（不析构元素——元素的所有权在调用方，与原版 ListClass 一致）。
    void Reset()
    {
        Head  = nullptr;
        Tail  = nullptr;
        Count = 0;
    }
};

//========================================================================
// INIComment - Comment node in a linked list
//========================================================================

struct INIComment
{
    char* Value;
    INIComment* Next;

    INIComment() : Value(nullptr), Next(nullptr) {}
    ~INIComment()
    {
        if (Value)
            YRMemory::Deallocate(Value);
        delete Next;
    }
};

//========================================================================
// INIEntry - A single key=value entry in a section
//========================================================================

class INIEntry : public Node<INIEntry>
{
public:
    virtual ~INIEntry() noexcept override
    {
        if (Key)
            YRMemory::Deallocate(Key);
        if (Value)
            YRMemory::Deallocate(Value);
        if (CommentString)
            YRMemory::Deallocate(CommentString);
        delete Comments;
    }

    char* Key;
    char* Value;
    INIComment* Comments;
    char* CommentString;
    int32 PreIndentCursor;
    int32 PostIndentCursor;
    int32 CommentCursor;
};

//========================================================================
// INISection - A section in an INI file
//========================================================================

class INISection : public Node<INISection>
{
public:
    virtual ~INISection() noexcept override
    {
        if (Name)
            YRMemory::Deallocate(Name);
        delete Comments;

        // Delete all entries
        INIEntry* entry = Entries.First();
        while (entry)
        {
            INIEntry* next = static_cast<INIEntry*>(entry->Next);
            delete entry;
            entry = next;
        }
    }

    char* Name;
    INIIntrusiveList<INIEntry> Entries;
    INIComment* Comments;
};

//========================================================================
// INIClass - INI file parser and manager
//========================================================================

class INIClass
{
public:
    //========================================================================
    // Construction / Destruction
    //========================================================================

    INIClass();
    virtual ~INIClass() noexcept;

    //========================================================================
    // File Operations
    //========================================================================

    // Reset the INI data (clear all sections and entries)
    void Reset();

    // Clear a specific section or key
    void Clear(const char* pSection, const char* pKey = nullptr);

    // Load an INI file from a CCFileClass
    bool LoadFile(CCFileClass* pFile);

    // asm: CCINIClass::Load 把真正的逐行解析交给 FileStraw/INIFile 管道
    //   （asm: sub_525A60），本函数就是那条管道的重建版：喂进一段文本与长度，
    //   原地切段/切键并填充 Sections 链表。空指针或长度为 0 返回 false。
    //   bLoadComments 为真时保留 ';' 注释（挂到节/条目的 Comments 链上）。
    bool LoadFromBuffer(const char* pData, size_t nSize, bool bLoadComments = false);

    // Save the INI data to a file
    bool SaveFile(CCFileClass* pFile) const;

    //========================================================================
    // Section Operations
    //========================================================================

    // Get a section by name
    INISection* GetSection(const char* pSection);

    // Get the number of keys in a section
    int32 GetKeyCount(const char* pSection);

    // Get the name of the N-th key in a section
    const char* GetKeyName(const char* pSection, int32 nKeyIndex);

    // Check if a section exists
    bool SectionExists(const char* pSection);

    // Check if a key exists in a section
    bool KeyExists(const char* pSection, const char* pKey);

    //========================================================================
    // String Reading/Writing
    //========================================================================

    // Read a string value
    int32 ReadString(const char* pSection, const char* pKey,
                     const char* pDefault, char* pBuffer, size_t bufferSize);

    int32 GetString(const char* pSection, const char* pKey,
                    char* pBuffer, size_t bufferSize);

    // asm: INIClass_GetString 的二元重载（retn 8）—— 上述五参数版的"只问值
    //  指针"形态：命中返回条目值的内部指针（**只读、不得改写、下次写入即失效**），
    //  未命中/无值返回 nullptr。原版里有大量调用点直接把它交给
    //  XXXTypeClass_FindOrAllocate，省掉本地缓冲。
    const char* GetString(const char* pSection, const char* pKey);

    // Write a string value
    bool WriteString(const char* pSection, const char* pKey, const char* pString);

    //========================================================================
    // Integer Reading/Writing
    //========================================================================

    // Read an integer value
    int32 ReadInteger(const char* pSection, const char* pKey, int32 nDefault);

    void GetInteger(const char* pSection, const char* pKey, int32& nValue);

    // Write an integer value
    bool WriteInteger(const char* pSection, const char* pKey, int32 nValue, bool bHex = false);

    // Read / write a bare-hex value (INIClass_GetHex / INIClass_WriteHex).
    // The writer emits "%X" with no "h" suffix; the reader parses base 16.
    int32 ReadHex(const char* pSection, const char* pKey, int32 nDefault = 0);
    bool  WriteHex(const char* pSection, const char* pKey, int32 nValue);

    //========================================================================
    // Boolean Reading/Writing
    //========================================================================

    // Read a boolean value
    bool ReadBool(const char* pSection, const char* pKey, bool bDefault);

    void GetBool(const char* pSection, const char* pKey, bool& bValue);

    // Write a boolean value
    bool WriteBool(const char* pSection, const char* pKey, bool bValue);

    //========================================================================
    // Float/Double Reading/Writing
    //========================================================================

    // Read a float value
    float ReadFloat(const char* pSection, const char* pKey, float fDefault);

    void GetFloat(const char* pSection, const char* pKey, float& fValue);

    // Write a float value
    bool WriteFloat(const char* pSection, const char* pKey, float fValue);

    // Read a double value
    double ReadDouble(const char* pSection, const char* pKey, double dDefault);

    void GetDouble(const char* pSection, const char* pKey, double& dValue);

    // Write a double value
    bool WriteDouble(const char* pSection, const char* pKey, double dValue);

    //========================================================================
    // Fixed / Percentage Reading
    //========================================================================

    double ReadFixed(const char* pSection, const char* pKey, double dDefault);

    void GetFixed(const char* pSection, const char* pKey, double& dValue);

    bool WriteFixed(const char* pSection, const char* pKey, double dValue);

    int32 ReadIntHundredth(const char* pSection, const char* pKey, int32 nDefault);

    void GetIntHundredth(const char* pSection, const char* pKey, int32& nValue);

    bool WriteIntHundredth(const char* pSection, const char* pKey, int32 nValue);

    //========================================================================
    // Lepton (fixed point) Reading/Writing
    //========================================================================

 // CCINIClass::Get_Lepton - the stored value is a floating
    // point fraction of a cell; a missing key yields nDefault.  A stored -1.0
    // is the "unset" sentinel and is returned as nDefault verbatim; every other
    // value is scaled by 256.0 and floored to an integer.
    int32 ReadLepton(const char* pSection, const char* pKey, int32 nDefault);

    void GetLepton(const char* pSection, const char* pKey, int32& nValue);

 // CCINIClass::Put_Lepton - writes value / 256.0 through the
    // "%f" float writer.
    bool WriteLepton(const char* pSection, const char* pKey, int32 nValue);

    // asm 名对齐：原版这几个入口叫 Put_Lepton，实现即 WriteLepton。
    bool Put_Lepton(const char* pSection, const char* pKey, int32 nValue)
    { return WriteLepton(pSection, pKey, nValue); }

    //========================================================================
    // UU Block Reading/Writing
    //========================================================================

 // INIClass::Put_UUBlock - serialise a binary blob into a
    // section as numbered "1", "2", ... keys, each holding up to 0x46 (70)
    // base64 characters.  Returns false when pSection / pValue is null or the
    // size is not positive.
    bool WriteUUBlock(const char* pSection, const void* pValue, size_t nSize);

 // INIClass::Get_UUBlock - the mirror: walk the section's
    // numbered keys, base64-decode each and pack the bytes back into pBuffer.
    // Returns the number of decoded bytes, capped at nSize.
    int32 ReadUUBlock(const char* pSection, void* pBuffer, size_t nSize);

    // asm 名对齐：原版这两个入口分别叫 Put_UUBlock / Get_UUBlock，实现即
    // WriteUUBlock / ReadUUBlock。INIClass::Get_PKey 依赖 Get_UUBlock 取
    // "PrivateKey" / "PublicKey" 段。
    bool Put_UUBlock(const char* pSection, const void* pValue, size_t nSize)
    { return WriteUUBlock(pSection, pValue, nSize); }
    int32 Get_UUBlock(const char* pSection, void* pBuffer, size_t nSize)
    { return ReadUUBlock(pSection, pBuffer, nSize); }

    //========================================================================
    // Multi-Value Reading
    //========================================================================

    // Read two integer values (e.g., "X,Y" or "X Y")
    int32* Read2Integers(int32* pBuffer, const char* pSection, const char* pKey,
                         const int32* pDefault);

    // Read three integer values (e.g., "X,Y,Z")
    int32* Read3Integers(int32* pBuffer, const char* pSection, const char* pKey,
                         const int32* pDefault);

    // Write two integer values
    bool Write2Integers(const char* pSection, const char* pKey, const int32* pValues);

    // Write three integer values
    bool Write3Integers(const char* pSection, const char* pKey, const int32* pValues);

    // Read two word values (e.g., "X,Y")
    uint16* Read2Words(uint16* pBuffer, const char* pSection, const char* pKey,
                       const uint16* pDefault);

    // Write two word values
    bool Write2Words(const char* pSection, const char* pKey, const uint16* pValues);

    // Write an integer scaled by a multiplier (value * multiplier / divisor)
    bool WriteIntMultiplied(const char* pSection, const char* pKey,
                            int32 nValue, int32 nMultiplier);

    // Write an integer scaled down by a divisor
    bool WriteIntDivided(const char* pSection, const char* pKey,
                         int32 nValue, int32 nDivisor);

    // Read a pip index (stored as a name, resolved to an index)
    int32 ReadPipIdx(const char* pSection, const char* pKey, int32 nDefault);

    void GetPipIdx(const char* pSection, const char* pKey, int32& nValue);

    // Write a pip index
    bool WritePipIdx(const char* pSection, const char* pKey, int32 nValue);

    // Read a pip scale index
    int32 ReadPipscaleIdx(const char* pSection, const char* pKey, int32 nDefault);

    void GetPipscaleIdx(const char* pSection, const char* pKey, int32& nValue);

    //========================================================================
    // Color Reading/Writing
    //========================================================================

    // Read three byte values (R,G,B)
    uint8* Read3Bytes(uint8* pBuffer, const char* pSection, const char* pKey,
                      const uint8* pDefault);

    // Write three byte values
    bool Write3Bytes(const char* pSection, const char* pKey, const uint8* pValues);

    //========================================================================
    // Section helpers
    //========================================================================

    // 根据游戏行为，可知这两者是把"按节名找第 N 个键 / 数键数"暴露成
    //  Section 前缀的便捷入口，找不到节时分别返回空与 0。
    const char* Section_GetKeyName(const char* pSection, int32 nKeyIndex);
    int32 Section_GetValueCount(const char* pSection);

    // asm: INIClass_Section_GetKeyName (0x5291F0) 的四/五参数形态
    //   char* __thiscall Section_GetKeyName(this, section, index, dest, size, def)
    // 语义：
    //   - 节不存在或下标越界 → 把 pDefault（可为 null，即空串）按 size 拷进
    //     pBuffer，返回 pBuffer（**不返回 null**）；
    //   - 命中 → strncpy(pBuffer, entry->Key, size) 并返回 pBuffer。
    //   原版用它把"第 N 个键的名字"直接写进 0x20 字节的栈缓冲（[Movies] /
    //   [InfantryTypes] 等"键只是序号"的段都走这条路）。
    const char* Section_GetKeyName(const char* pSection, int32 nKeyIndex,
                                   char* pBuffer, size_t bufferSize,
                                   const char* pDefault = nullptr);

    // asm: INIClass::Find_Section (asm:4867) —— 以节名（先 CRC32 哈希）在节表
    //  中查找；命中返回该节对象，未命中/空名返回 null。它是 GetString /
    //  GetInteger 等所有读取函数内部"节查找"那一步的独立入口。
    INISection* Find_Section(const char* pSection);

    // asm 里没有独立的 Find_Key：键查找内联在 GetString/GetInteger 的
    //  CRC 比对流程中（Find_Section 找节后，再在节内按键 CRC 二分）。这里把
    //  同一语义单独暴露成 Find_Key，命中返回条目指针，否则 null，与 FindEntry 等价。
    INIEntry* Find_Key(const char* pSection, const char* pKey) const;

    // asm: 读取族统一先 Find_Section / 键存在性判定的两个布尔的命名入口。
    bool Section_Exists(const char* pSection) { return SectionExists(pSection); }
    bool Key_Exists(const char* pSection, const char* pKey) { return KeyExists(pSection, pKey); }

    // 根据游戏行为，可知 Get_Hex 走无符号裸十六进制读取，等价于 ReadHex。
    int32 GetHex(const char* pSection, const char* pKey, int32 nDefault);

    //========================================================================
    // Pointer-form accessors
    //========================================================================

    // 根据游戏行为，可知 char** 形式的取值函数把条目值指针直接交给调用者：
    // 键存在时写入指针并返回真，键不存在时不改写输出并返回假。
    bool GetInteger_charPP(const char* pSection, const char* pKey, char** pOut);
    bool GetString_charPP(const char* pSection, const char* pKey, char** pOut);

    // 根据游戏行为，可知宽字符串读取按 UTF-16 解码、按字符数截断，
    // 键缺失时拷入缺省串。
    int32 GetUnicodeString(const char* pSection, const char* pKey,
                           const wchar_t* pDefault, wchar_t* pBuffer, size_t nChars);

    // 根据游戏行为，可知三浮点读取按 "x,y,z" 逗号拆分，缺项保留原值。
    float* Read3Floats(float* pBuffer, const char* pSection, const char* pKey,
                       const float* pDefault);

    // 根据游戏行为，可知场景读取把整个场景文件内容并入当前 INI 对象。
    bool ReadScenario(const char* pFileName);

    // 根据游戏行为，可知地图预览包按 Base64 编码、切成固定宽度的多行写入。
    bool SaveMapPreview(const char* pSection, const void* pData, int32 nSize);

    // asm: INIClass_ParseSideHouses (asm:275442, retn 28h)
    //
    //   [Sides] 的值是"逗号分隔的**房屋类型名**"，这个函数把它翻成房屋序号
    //   向量（不是把字符串回写！旧实现把它当成"去空白后回写规范化串"，与
    //   asm 完全不符，此处按 asm 重写）：
    //
    //     if (!GetString(section, key, "", Str, 0x80)) -> 结果 = 空向量
    //     for (tok = strtok(Str, ","); tok && tok[0]; tok = strtok(NULL, ",")) {
    //         i = HouseTypeClass::From_Name(tok);   // 名字 -> 房屋表序号
    //         if (i != -1) push(i);                 // 查不到丢弃，不占位
    //     }
    //     return vector;
    //
    //   RulesClass_Addition_Sides 把返回值直接拷进 SideClass::Houses
    //   （asm:4246 `VectorClass_Copy(side+0x98, vec)`），随后 SideClass 遍历
    //   这些序号给每个 HouseTypeClass 打上 Side 归属。注意**没有**"某个 token
    //   是另一个阵营名就把它整串展开"的分支——asm 只有 HouseTypeClass::From_Name
    //   一次查找。
    void ParseSideHouses(const char* pSection, const char* pKey,
                         DynamicVectorClass<int32>& rList);

    // 根据游戏行为，可知宽字符写出口中非 ASCII 字符以 \xXXXX 转义。
    bool WriteUnicodeEscaped(const char* pSection, const char* pKey, const wchar_t* pValue);

    // 根据游戏行为，可知指针形式的写入函数先解引用再走普通写入，
    // 空指针视为无值可写、返回假。
    bool WriteBool_charPP(const char* pSection, const char* pKey, const bool* pValue);
    bool WriteString_charPP(const char* pSection, const char* pKey, char* const* pValue);
    bool WriteInteger_charPP(const char* pSection, const char* pKey, const int32* pValue);
    bool Write2Integers_charPP(const char* pSection, const char* pKey, const int32* pValues);
    bool Write3Floats_charPP(const char* pSection, const char* pKey, const float* pValues);

    //========================================================================
    // Utility
    //========================================================================

    // Check if a section/key exists
    bool Exists(const char* pSection, const char* pKey = nullptr);

    // Check if a value is blank (none or <none>)
    static bool IsBlank(const char* pValue);

    // Get the number of sections
    int32 GetSectionCount() const { return Sections.GetCount(); }

    // Get the number of entries in a section
    int32 GetEntryCount(const char* pSection);

    // Get the number of line comments
    int32 GetLineCount() const;

    //========================================================================
    // Internal: get or create a section
    //========================================================================

    INISection* GetOrCreateSection(const char* pSection);

    //========================================================================
    // Internal: get or create an entry
    //========================================================================

    INIEntry* GetOrCreateEntry(const char* pSection, const char* pKey);

    //========================================================================
    // Internal: find an entry
    //========================================================================

    INIEntry* FindEntry(const char* pSection, const char* pKey) const;

    //========================================================================
    // Internal: set an entry value
    //========================================================================

    void SetEntryValue(INIEntry* pEntry, const char* pValue);

protected:
    // Buffer for reading lines
    char m_LineBuffer[4096];

public:
    char* CurrentSectionName;
    INISection* CurrentSection;
    INIIntrusiveList<INISection> Sections;
    INIComment* LineComments;

private:
    DISABLE_COPY_AND_MOVE(INIClass)
};

//========================================================================
// CCINIClass - Extended INI class for C&C use
//
// Extends INIClass with game-specific features:
// - CRC32 digest for file integrity checking
// - String table entry reading
// - Game-specific data type parsing
//========================================================================

class CCINIClass : public INIClass
{
public:
    //========================================================================
    // Construction / Destruction
    //========================================================================

    CCINIClass();
    virtual ~CCINIClass() noexcept override;

    //========================================================================
    // Static members
    //========================================================================

    static CCINIClass* INI_Rules;
    static CCINIClass INI_Art;
    static CCINIClass INI_AI;
    static CCINIClass INI_UIMD;
    static CCINIClass INI_RA2MD;

    static uint32 RulesHash;
    static uint32 ArtHash;
    static uint32 AIHash;

    //========================================================================
    // File Operations
    //========================================================================

    // The shared artmd.ini instance.  Returns null until the rules loader
    // has opened it, mirroring the original's ART_INI global.
    static CCINIClass* GetArtINI();

    // Load an INI file
    static CCINIClass* LoadINIFile(const char* pFileName);

    // Unload an INI file
    static void UnloadINIFile(CCINIClass*& pINI);

    // Parse an INI file from a CCFileClass
    bool ReadCCFile(CCFileClass* pFile, bool bDigest = false, bool bLoadComments = false);

    // Write to a CCFileClass
    bool WriteCCFile(CCFileClass* pFile, bool bDigest = false);

    //========================================================================
    // String Table
    //========================================================================

    // Read a string table entry
    int32 ReadStringTableEntry(const char* pSection, const char* pKey,
                               char* pBuffer, size_t bufferSize);

    //========================================================================
    // Movie lookup
    //========================================================================

    // Resolves a movie name stored under pKey to its index in the global
    // movie table.  "<none>" and unknown names yield nDefault.
    int32 FindMovieIndex(const char* pSection, const char* pKey, int32 nDefault);

    //========================================================================
    // Multi-field slot parsing
    //========================================================================

    // Reads pKey as a "a,b,c" triple of comma separated integers and stores
    // each component through the out pointers.  Components that are absent
    // leave the corresponding output untouched, matching the original's
    // sequential _strtok loop (asm sub_477440).
    void ReadSlotTriple(const char* pSection, const char* pKey,
                        int32* pFirst, int32* pSecond, int32* pThird);

    //========================================================================
    // Geometry reading
    //========================================================================

    // INIClass::Get_Point: reads "x,y" into a two-integer point.  Absent
    // components leave the corresponding output untouched.
    void ReadPoint(const char* pSection, const char* pKey,
                   int32* pX, int32* pY);

    // INIClass::Get_Rect: reads "x,y,w,h" into a RectangleStruct.  Absent
    // components leave the corresponding output untouched.
    void ReadRect(const char* pSection, const char* pKey,
                  RectangleStruct* pRect);

 // INIClass::Put_Rect: writes the rectangle as the
    // "%d,%d,%d,%d" string via INIClass_WriteString.
    bool WriteRect(const char* pSection, const char* pKey,
                   const RectangleStruct* pRect);

    // asm 名对齐（asm: INIClass::Get_Point / INIClass::Get_Rect /
    // INIClass::Put_Rect）：分别等价 ReadPoint / ReadRect / WriteRect。
    void Get_Point(const char* pSection, const char* pKey,
                   int32* pX, int32* pY)
    { ReadPoint(pSection, pKey, pX, pY); }
    void Get_Rect(const char* pSection, const char* pKey,
                  RectangleStruct* pRect)
    { ReadRect(pSection, pKey, pRect); }
    bool Put_Rect(const char* pSection, const char* pKey,
                  const RectangleStruct* pRect)
    { return WriteRect(pSection, pKey, pRect); }


    //========================================================================
    // Audio Enumeration Parsing
    //========================================================================

    static int32 ParseSoundPriority(const char* pValue);
    static int32 ParseSoundControl(const char* pValue);
    static int32 ParseSoundType(const char* pValue);

    //========================================================================
    // Category Index Mapping
    //========================================================================

    static int32 BuildCatIdxToNameIdx(const char* const* pNames, int32 nCount,
                                      const char* pValue, int32 nDefault);
    static const char* BuildCatNameToIdx(const char* const* pNames, int32 nCount,
                                         int32 nIndex);

    //========================================================================
    // Game Enumeration Parsing
    //========================================================================

    static Armor        ParseArmorType(const char* pValue);
    static Category     ParseCategory(const char* pValue);
    static VHPScan      ParseVHPScan(const char* pValue);
    static Foundation   ParseFoundation(const char* pValue);
    static PipScale     ParsePipScale(const char* pValue);
    static LandType     ParseLandType(const char* pValue);
    static MovementZone ParseMovementZone(const char* pValue);
    static SpeedType    ParseSpeedType(const char* pValue);
    static Layer        ParseLayer(const char* pValue);
    static BuildCat     ParseBuildCat(const char* pValue);

    static const char*  CategoryIdxToName(int32 nIndex);
    static const char*  VHPScanIdxToName(int32 nIndex);
    static const char*  FoundationIdxToName(int32 nIndex);
    static const char*  PipScaleIdxToName(int32 nIndex);
    static const char*  LandTypeIdxToName(int32 nIndex);
    static const char*  MovementZoneIdxToName(int32 nIndex);
    static const char*  SpeedTypeIdxToName(int32 nIndex);
    static const char*  LayerIdxToName(int32 nIndex);
    static const char*  BuildCatIdxToName(int32 nIndex);

    static void         ParseAbilities(const char* pValue, int32* pOut, int32 nCount);

    //========================================================================
    // Map save helpers
    //========================================================================

    // ------------------------------------------------------------------------
    // asm: Overlay_Pack_Type_SaveToINI (0x5FD680, __thiscall, retn 0)
    //   this = CCINIClass（调用点 ScenarioClass_SaveMap 传的是 ccINI）
    //
    //   mangleStuff_("OVERLAY", 0) ; mangleStuff_("OverlayPack", 0)
    //   for (y = 0; y < 200h; ++y) for (x = 0; x < 200h; ++x):
    //       cell = MapClass::Coord_Cell(Map, {x,y})
    //       total += sub_5520A0(&bufpipe, cell + 44h)      ; 1 字节/格
    //   if (total): Put_UUBlock(this, "OverlayPack", <base64 文本>)
    //   ... 对 OverlayDataPack 重复同样管线，取 cell + 11Eh（OverlayFrame）
    //
    //   本项目等价：把 512x512 的逐格字节图直接交给 Put_UUBlock ——
    //   WriteUUBlock 内部就是 Base64Pipe + 0x46 字符分行，和原版
    //   BufferPipe/Base64Pipe 管线的产出**同一份文本**，所以不再手工串
    //   Pipe（避免二次编码）。
    //   降级点：mangleStuff_（节名混淆）在本项目的 INI 层没有对应物，
    //   节名按明文写入。
    // ------------------------------------------------------------------------
    bool Put_Overlay_Pack();

    //========================================================================
    // Typed getters by name lookup
    //========================================================================

    // 根据游戏行为，可知类型读取一族都走"读名字 → 在对应类型表里查 →
    // 校验类别"三步：名字为空或类别不符时一律回落到调用者给的缺省值。
    TechnoTypeClass*   FindTechnoTypeByName(const char* pSection, const char* pKey);

    // asm: INIClass_GetTechnoPrerequisite (asm:273851, retn 0Ch = 3 参数)
    //   uint32 __thiscall GetTechnoPrerequisite(this, section, key, fallback)
    //
    //   **不是**"按名字查一个 TechnoType"，而是一个**位图累加器**——这一点与
    //   GetPrerequisiteList 返回"索引向量"完全不同，两者不可混用：
    //
    //     if (!GetString(section, key, "", Str, 0x80)) return fallback;
    //     mask = 0;
    //     for (tok = strtok(Str, ","); tok; tok = strtok(NULL, ",")) {
    //         if (tok[0] == 0) break;
    //         if (!strcmpi(tok, "POWER"))    mask |= 0x80000000;  // bit31
    //         if (!strcmpi(tok, "FACTORY"))  mask |= 0x40000000;  // bit30
    //         if (!strcmpi(tok, "BARRACKS")) mask |= 0x20000000;  // bit29
    //         if (!strcmpi(tok, "RADAR"))    mask |= 0x10000000;  // bit28
    //         i = BuildingTypeClass_FindIndex(tok);
    //         if (i != -1) mask |= (1u << i);                     // 低 28 位
    //     }
    //     return mask;
    //
    //   四个关键字位落在高 4 位、建筑索引位落在低 28 位，所以"建筑索引 > 27"
    //   会与关键字位重叠——原版就是这样（YR 建筑表远小于 28，不会真的撞上）。
    //   注意:缺键返回 fallback，而"键存在但为空串"返回 0（GetString 成功、
    //   strtok 立即为空）。这与 GetPrerequisiteList 缺键返回空向量一致。
    uint32             GetTechnoPrerequisite(const char* pSection, const char* pKey,
                                             uint32 nDefault);
    AircraftTypeClass* GetAircraftType(const char* pSection, const char* pKey,
                                       AircraftTypeClass* pDefault);
    BuildingTypeClass* GetBuildingType(const char* pSection, const char* pKey,
                                       BuildingTypeClass* pDefault);
    InfantryTypeClass* GetInfantryType(const char* pSection, const char* pKey,
                                       InfantryTypeClass* pDefault);
    UnitTypeClass*     GetUnitType(const char* pSection, const char* pKey,
                                   UnitTypeClass* pDefault);
    TerrainTypeClass*  GetTerrainType(const char* pSection, const char* pKey,
                                      TerrainTypeClass* pDefault);
    WarheadTypeClass*  GetWarheadType(const char* pSection, const char* pKey,
                                      WarheadTypeClass* pDefault);

    // 根据游戏行为，可知下面一族读的是"名字或索引"，返回表内序号。
    int32      GetSide(const char* pSection, const char* pKey, int32 nDefault);
    int32      GetTheme(const char* pSection, const char* pKey, int32 nDefault);
    int32      GetVoxIndex(const char* pSection, const char* pKey, int32 nDefault);
    int32      GetColorSchemeIdx(const char* pSection, const char* pKey, int32 nDefault);
    int32      GetCategoryIdx(const char* pSection, const char* pKey, int32 nDefault);
    int32      GetSWTypeIndex(const char* pSection, const char* pKey, int32 nDefault);
    int32      FindHouseIndex(const char* pSection, const char* pKey);

    // 根据游戏行为，可知 Action 以既知名字表（strlist_Actions）存取：
    // 读字符串默认取表中第 nDefault 项，再在整表中大小写不敏感查找，
    // 命中返回表内下标，未命中返回 0（asm 如此，不返回 nDefault）。
    int32      GetAction(const char* pSection, const char* pKey, int32 nDefault);

    // 根据游戏行为，可知这是"从 INI 读国家名 → 在注册表里找到或分配一个
    // 多人可用国家 → 返回其序号"的一站式入口：先按名字取玩家项，命中且
    // 该序号属于七个多人专用国有槽位就直接返回；否则退回按类型名查找，
    // 查不到时若允许则新建类型并登记，最后返回该类型在表里的序号；
    // 读不到键或名字查不到类型时返回调用者给的缺省值。
    int32      HouseType_FindOrAllocate_YesMP(const char* pSection, const char* pKey,
                                              int32 nDefault);

    LandType   GetLand(const char* pSection, const char* pKey, LandType nDefault);
    TheaterType GetTheater(const char* pSection, const char* pKey, TheaterType nDefault);
    AbstractType GetFactory(const char* pSection, const char* pKey, AbstractType nDefault);

    // 根据游戏行为，可知能力名与速度类型名按既知表序映射为位索引，
    // 纯数字串直接按数取值。
    static int32 AbilityNameToIdx(const char* pValue);
    static int32 SpeedTypeNameToIdx(const char* pValue);

    //========================================================================
    // Typed writers
    //========================================================================

    // 根据游戏行为，可知下面一族把枚举值以既知名表反查成名字串写入，
    // 使 INI 文件保持与原版相同的可读形式。
    bool WriteSpeedType(const char* pSection, const char* pKey, SpeedType nValue);
    bool WriteMovementZone(const char* pSection, const char* pKey, MovementZone nValue);
    bool WriteArmor(const char* pSection, const char* pKey, Armor nValue);
    bool WriteLand(const char* pSection, const char* pKey, LandType nValue);
    bool WriteFoundation(const char* pSection, const char* pKey, Foundation nValue);
    bool WriteBuildCat(const char* pSection, const char* pKey, BuildCat nValue);
    bool WriteTheater(const char* pSection, const char* pKey, TheaterType nValue);
    bool WriteFactory(const char* pSection, const char* pKey, AbstractType nValue);
    bool WriteCategoryIdx(const char* pSection, const char* pKey, int32 nIndex);
    bool WriteColorScheme(const char* pSection, const char* pKey, int32 nIndex);
    bool WriteHouseIndex(const char* pSection, const char* pKey, int32 nIndex);
    bool WritePipscaleIdx(const char* pSection, const char* pKey, int32 nIndex);

    //========================================================================
    // CLSID & string-table & voice-list accessors
    //========================================================================

    // 根据游戏行为，可知 CLSID 以 "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}"
    // 完整大写形式存取，缺括号的裸段式同样能读入。
    bool ReadCLSID(const char* pSection, const char* pKey, GUID* pOut);
    bool WriteCLSID(const char* pSection, const char* pKey, const GUID* pValue);

    // 根据游戏行为，可知字符串表条目与普通串读取同路，只是按表内
    // 编号键走。
    int32 GetStringtableEntry(const char* pSection, const char* pKey,
                              char* pBuffer, size_t bufferSize);

    // 根据游戏行为，可知语音表按逗号名字串读取，逐个名字在语音表里
    // 落成索引，未知名跳过。
    bool Get_Vector_Voc_525430(const char* pSection, const char* pKey,
                               DynamicVectorClass<int32>& rList);

    //========================================================================
    // Typed Getters
    //========================================================================

    Armor        GetArmorType(const char* pSection, const char* pKey, Armor nDefault);
    Category     GetCategory(const char* pSection, const char* pKey, Category nDefault);
    VHPScan      GetVHPScan(const char* pSection, const char* pKey, VHPScan nDefault);
    Foundation   GetFoundation(const char* pSection, const char* pKey, Foundation nDefault);
    PipScale     GetPipScale(const char* pSection, const char* pKey, PipScale nDefault);
    LandType     GetLandType(const char* pSection, const char* pKey, LandType nDefault);
    MovementZone GetMovementZone(const char* pSection, const char* pKey, MovementZone nDefault);
    SpeedType    GetSpeedType(const char* pSection, const char* pKey, SpeedType nDefault);
    Layer        GetLayer(const char* pSection, const char* pKey, Layer nDefault);
    BuildCat     GetBuildCat(const char* pSection, const char* pKey, BuildCat nDefault);

    bool         GetAbilities(const char* pSection, const char* pKey,
                              int32* pOut, int32 nCount);
    bool         Get3Integers(const char* pSection, const char* pKey,
                              int32* pValues);
    bool         Get3Bytes(const char* pSection, const char* pKey,
                           uint8* pValues);

    // asm: INIClass_GetColor (0x474ED0)。语义：把缺省颜色按 "%d,%d,%d"
    // 格式串成 "R,G,B" 作为读取缺省值，读回后再按 "%d,%d,%d" 解析成
    // 三个字节写入 pOut；键不存在时直接把缺省颜色拷入 pOut。始终返回
    // pOut（与原版一致，调用者不检查空指针）。
    ColorStruct* GetColor(ColorStruct* pOut, const char* pSection,
                          const char* pKey, const ColorStruct* pDefault);
    bool         Get2Integers(const char* pSection, const char* pKey,
                              int32* pValues);
    bool         GetVectorIntegers(const char* pSection, const char* pKey,
                                   int32* pValues, int32 nMax);
    bool         GetVectorIntegers(const char* pSection, const char* pKey,
                                   DynamicVectorClass<int32>& rList);
    bool         GetVectorAircraftType(const char* pSection, const char* pKey,
                                       DynamicVectorClass<AircraftTypeClass*>& rList);
    bool         GetVectorUnitType(const char* pSection, const char* pKey,
                                   DynamicVectorClass<UnitTypeClass*>& rList);
    bool         GetVectorBuildType(const char* pSection, const char* pKey,
                                    DynamicVectorClass<BuildingTypeClass*>& rList);
    bool         GetVectorInfType(const char* pSection, const char* pKey,
                                  DynamicVectorClass<InfantryTypeClass*>& rList);
    bool         GetVectorTerrainTypes(const char* pSection, const char* pKey,
                                       DynamicVectorClass<TerrainTypeClass*>& rList);
    Powerup      GetPowerup(const char* pSection, const char* pKey, Powerup nDefault);

    int32        ReadColorSchemeIndex(const char* pSection, const char* pKey,
                                      int32 nDefault);

    // Edge - a map border direction (North/East/South/West/Air).  When the
    // key is absent the fallback is returned unchanged, mirroring the
    // original INIClass_GetEdge.
    int32        GetEdge(const char* pSection, const char* pKey, int32 nDefault);

    // Allies - a comma separated list of house names decoded into a bitfield
    // indexed by each house's own player number.  Returns nDefault when the
    // key is missing, otherwise the accumulated mask.
    uint32       GetAlliesBitfield(const char* pSection, const char* pKey,
                                   uint32 nDefault);

    int32        GetOwners(const char* pSection, const char* pKey, int32 nDefault);
    bool         GetVectorColors(const char* pSection, const char* pKey,
                                 DynamicVectorClass<ColorStruct>& rList);

    // Prerequisites are a comma separated run of building IDs plus six literal
    // keywords.  asm: CCINIClass_GetPrerequisiteList (asm:276562) ——
    //
    //   每个 token 先比六个关键字，命中就往结果向量里压一个**负哨兵**，
    //   然后再无条件试一次 BuildingTypeClass_FindIndex（关键字必然查不到，
    //   互不干扰）：
    //       "POWER"    -> -1        "FACTORY"  -> -2
    //       "BARRACKS" -> -3        "RADAR"    -> -4
    //       "TECH"     -> -5        "PROC"     -> -6
    //   普通名字 -> BuildingTypeClass::FindIndex()，命中才压入（**排在最后**，
    //   即关键字哨兵永远在向量最前面，因为六次比较先做）。
    //   缺键/空值:结果向量被清空（原版构造一个空向量再 operator= 覆盖）。
    bool         GetPrerequisiteList(const char* pSection, const char* pKey,
                                     DynamicVectorClass<int32>& rList);

    // asm: INIClass_Get_Vector_Voc_525430 (asm:643239, retn 0Ch = 3 参数)
    //   void __thiscall Get_Vector_Voc(this, section, key, VectorPtrInteger out)
    //
    //   **声音名向量**：值是逗号分隔的 VOC 名字（不是编号），逐个走
    //   VocClass_Find(name) -> 非空再 VocClass_FindIndex(ptr) 取索引，压进
    //   结果向量；查不到的名字直接丢弃（不像 GetVectorIntegers 那样把无法
    //   解析的项当 0 存进去）。缺键:结果向量被清空。
    //   [AudioVisual] IceCrackSounds 就是这条路径（asm:1275355）。
    void         Get_Vector_Voc(const char* pSection, const char* pKey,
                                DynamicVectorClass<int32>& rList);

    // Retrieve the 20-byte digest computed when the file was read
    bool GetDigest(uint8* pDigest, size_t digestSize) const;

    //========================================================================
    // CRC Operations
    //========================================================================

    // Get CRC32 hash of the INI data
    uint32 GetCRC() const;

    //========================================================================
    // Properties
    //========================================================================

    bool Digested;
    uint8 Digest[20];
    uint32 CRCValue;
};