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
    List<INIEntry*> Entries;
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

    // 根据游戏行为，可知阵营房屋表按逗号拆开、去空白后回写规范化串。
    bool ParseSideHouses(const char* pSection, const char* pKey);

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
    List<INISection*> Sections;
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
    // Typed getters by name lookup
    //========================================================================

    // 根据游戏行为，可知类型读取一族都走"读名字 → 在对应类型表里查 →
    // 校验类别"三步：名字为空或类别不符时一律回落到调用者给的缺省值。
    TechnoTypeClass*   FindTechnoTypeByName(const char* pSection, const char* pKey);
    TechnoTypeClass*   GetTechnoPrerequisite(const char* pSection, const char* pKey,
                                             TechnoTypeClass* pDefault);
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

    // Prerequisites are a comma separated run of techno IDs.  The literal
    // "POWER" is stored as -1, everything else is resolved through
    // TechnoTypeClass and stored as its array index.
    bool         GetPrerequisiteList(const char* pSection, const char* pKey,
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