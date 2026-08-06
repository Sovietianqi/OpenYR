#pragma once

#include "Core/Definitions.h"
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
                    char* pBuffer, size_t bufferSize)
    {
        return ReadString(pSection, pKey, pBuffer, pBuffer, bufferSize);
    }

    // Write a string value
    bool WriteString(const char* pSection, const char* pKey, const char* pString);

    //========================================================================
    // Integer Reading/Writing
    //========================================================================

    // Read an integer value
    int32 ReadInteger(const char* pSection, const char* pKey, int32 nDefault);

    void GetInteger(const char* pSection, const char* pKey, int32& nValue)
    {
        nValue = ReadInteger(pSection, pKey, nValue);
    }

    // Write an integer value
    bool WriteInteger(const char* pSection, const char* pKey, int32 nValue, bool bHex = false);

    //========================================================================
    // Boolean Reading/Writing
    //========================================================================

    // Read a boolean value
    bool ReadBool(const char* pSection, const char* pKey, bool bDefault);

    void GetBool(const char* pSection, const char* pKey, bool& bValue)
    {
        bValue = ReadBool(pSection, pKey, bValue);
    }

    // Write a boolean value
    bool WriteBool(const char* pSection, const char* pKey, bool bValue);

    //========================================================================
    // Float/Double Reading/Writing
    //========================================================================

    // Read a float value
    float ReadFloat(const char* pSection, const char* pKey, float fDefault);

    void GetFloat(const char* pSection, const char* pKey, float& fValue)
    {
        fValue = ReadFloat(pSection, pKey, fValue);
    }

    // Write a float value
    bool WriteFloat(const char* pSection, const char* pKey, float fValue);

    // Read a double value
    double ReadDouble(const char* pSection, const char* pKey, double dDefault);

    void GetDouble(const char* pSection, const char* pKey, double& dValue)
    {
        dValue = ReadDouble(pSection, pKey, dValue);
    }

    // Write a double value
    bool WriteDouble(const char* pSection, const char* pKey, double dValue);

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

    //========================================================================
    // Color Reading/Writing
    //========================================================================

    // Read three byte values (R,G,B)
    uint8* Read3Bytes(uint8* pBuffer, const char* pSection, const char* pKey,
                      const uint8* pDefault);

    // Write three byte values
    bool Write3Bytes(const char* pSection, const char* pKey, const uint8* pValues);

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