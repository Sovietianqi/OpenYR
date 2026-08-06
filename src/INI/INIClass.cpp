#include "INI/INIClass.h"

#include "Core/Memory.h"
#include "IO/FileSystem.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>

//========================================================================
// INIClass - Implementation
//========================================================================

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
    // Delete all sections
    INISection* section = Sections.First();
    while (section)
    {
        INISection* next = static_cast<INISection*>(section->Next);
        delete section;
        section = next;
    }

    delete LineComments;
    LineComments = nullptr;

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

bool INIClass::LoadFile(CCFileClass* pFile)
{
    if (!pFile) return false;

    Reset();

    if (!pFile->Open(static_cast<int32>(FileAccessMode::Read)))
        return false;

    int32 fileSize = pFile->GetSize();
    if (fileSize <= 0)
    {
        pFile->Close();
        return false;
    }

    char* buffer = static_cast<char*>(YRMemory::Allocate(static_cast<size_t>(fileSize) + 1));
    if (!buffer)
    {
        pFile->Close();
        return false;
    }

    int32 bytesRead = pFile->Read(buffer, fileSize);
    pFile->Close();

    if (bytesRead <= 0)
    {
        YRMemory::Deallocate(buffer);
        return false;
    }

    buffer[bytesRead] = '\0';

    // Parse the INI content
    char* lineStart = buffer;
    char* lineEnd = buffer;
    INISection* currentSection = nullptr;
    INIEntry* currentEntry = nullptr;

    while (lineStart && *lineStart)
    {
        // Find end of line
        lineEnd = strchr(lineStart, '\n');
        if (lineEnd)
        {
            *lineEnd = '\0';
            ++lineEnd; // Move past the newline
        }
        else
        {
            // Last line
            lineEnd = lineStart + strlen(lineStart);
        }

        // Trim trailing whitespace and carriage return
        char* end = lineStart + strlen(lineStart) - 1;
        while (end >= lineStart && (*end == '\r' || *end == ' ' || *end == '\t'))
        {
            *end = '\0';
            --end;
        }

        // Skip leading whitespace
        while (*lineStart == ' ' || *lineStart == '\t')
            ++lineStart;

        if (*lineStart == ';' || *lineStart == '#')
        {
            // Comment line
            // In a full implementation, we'd store comments
        }
        else if (*lineStart == '[')
        {
            // Section header
            char* closing = strchr(lineStart, ']');
            if (closing)
            {
                *closing = '\0';
                const char* sectionName = lineStart + 1;

                // Trim whitespace
                while (*sectionName == ' ' || *sectionName == '\t')
                    ++sectionName;

                char* se = closing - 1;
                while (se > sectionName && (*se == ' ' || *se == '\t'))
                {
                    *se = '\0';
                    --se;
                }

                currentSection = GetOrCreateSection(sectionName);
                currentEntry = nullptr;
            }
        }
        else if (*lineStart != '\0' && currentSection)
        {
            // Key=Value line
            char* equals = strchr(lineStart, '=');
            if (equals)
            {
                *equals = '\0';
                const char* key = lineStart;
                const char* value = equals + 1;

                // Trim key whitespace
                char* keyEnd = equals - 1;
                while (keyEnd >= key && (*keyEnd == ' ' || *keyEnd == '\t'))
                {
                    *keyEnd = '\0';
                    --keyEnd;
                }
                while (*key == ' ' || *key == '\t')
                    ++key;

                // Trim value whitespace
                while (*value == ' ' || *value == '\t')
                    ++value;

                // Create or update the entry
                currentEntry = GetOrCreateEntry(currentSection->Name, key);
                if (currentEntry)
                {
                    SetEntryValue(currentEntry, value);
                }
            }
        }

        lineStart = lineEnd;
        if (lineStart >= buffer + bytesRead)
            break;
    }

    YRMemory::Deallocate(buffer);
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

int32 INIClass::ReadString(const char* pSection, const char* pKey,
                           const char* pDefault, char* pBuffer, size_t bufferSize)
{
    if (!pBuffer || bufferSize == 0) return 0;

    INIEntry* entry = FindEntry(pSection, pKey);
    if (entry && entry->Value)
    {
        size_t len = strlen(entry->Value);
        if (len >= bufferSize)
            len = bufferSize - 1;
        memcpy(pBuffer, entry->Value, len);
        pBuffer[len] = '\0';
        return static_cast<int32>(len);
    }

    // Use default value
    if (pDefault)
    {
        size_t len = strlen(pDefault);
        if (len >= bufferSize)
            len = bufferSize - 1;
        memcpy(pBuffer, pDefault, len);
        pBuffer[len] = '\0';
        return static_cast<int32>(len);
    }

    pBuffer[0] = '\0';
    return 0;
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

int32 INIClass::ReadInteger(const char* pSection, const char* pKey, int32 nDefault)
{
    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) return nDefault;

    // Check for hex format
    if (entry->Value[0] == '0' && (entry->Value[1] == 'x' || entry->Value[1] == 'X'))
    {
        return static_cast<int32>(strtol(entry->Value + 2, nullptr, 16));
    }

    return static_cast<int32>(strtol(entry->Value, nullptr, 10));
}

bool INIClass::WriteInteger(const char* pSection, const char* pKey, int32 nValue, bool bHex)
{
    if (!pSection || !pKey) return false;

    char buffer[64];
    if (bHex)
        snprintf(buffer, sizeof(buffer), "0x%X", nValue);
    else
        snprintf(buffer, sizeof(buffer), "%d", nValue);

    return WriteString(pSection, pKey, buffer);
}

//========================================================================
// Boolean Reading/Writing
//========================================================================

bool INIClass::ReadBool(const char* pSection, const char* pKey, bool bDefault)
{
    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) return bDefault;

    // Check for common boolean representations
    if (strcasecmp(entry->Value, "true") == 0 ||
        strcasecmp(entry->Value, "yes") == 0 ||
        strcasecmp(entry->Value, "1") == 0)
        return true;

    if (strcasecmp(entry->Value, "false") == 0 ||
        strcasecmp(entry->Value, "no") == 0 ||
        strcasecmp(entry->Value, "0") == 0)
        return false;

    return bDefault;
}

bool INIClass::WriteBool(const char* pSection, const char* pKey, bool bValue)
{
    return WriteString(pSection, pKey, bValue ? "true" : "false");
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
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%g", fValue);
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
// Multi-Value Reading
//========================================================================

int32* INIClass::Read2Integers(int32* pBuffer, const char* pSection,
                               const char* pKey, const int32* pDefault)
{
    if (!pBuffer) return nullptr;

    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value)
    {
        if (pDefault)
        {
            pBuffer[0] = pDefault[0];
            pBuffer[1] = pDefault[1];
        }
        return pBuffer;
    }

    // Parse "X,Y" or "X Y" format
    char temp[256];
    strncpy(temp, entry->Value, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char* token = strtok(temp, ", \t");
    if (token)
        pBuffer[0] = static_cast<int32>(strtol(token, nullptr, 10));
    else if (pDefault)
        pBuffer[0] = pDefault[0];

    token = strtok(nullptr, ", \t");
    if (token)
        pBuffer[1] = static_cast<int32>(strtol(token, nullptr, 10));
    else if (pDefault)
        pBuffer[1] = pDefault[1];

    return pBuffer;
}

int32* INIClass::Read3Integers(int32* pBuffer, const char* pSection,
                               const char* pKey, const int32* pDefault)
{
    if (!pBuffer) return nullptr;

    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value)
    {
        if (pDefault)
        {
            pBuffer[0] = pDefault[0];
            pBuffer[1] = pDefault[1];
            pBuffer[2] = pDefault[2];
        }
        return pBuffer;
    }

    char temp[256];
    strncpy(temp, entry->Value, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char* token = strtok(temp, ", \t");
    if (token)
        pBuffer[0] = static_cast<int32>(strtol(token, nullptr, 10));
    else if (pDefault)
        pBuffer[0] = pDefault[0];

    token = strtok(nullptr, ", \t");
    if (token)
        pBuffer[1] = static_cast<int32>(strtol(token, nullptr, 10));
    else if (pDefault)
        pBuffer[1] = pDefault[1];

    token = strtok(nullptr, ", \t");
    if (token)
        pBuffer[2] = static_cast<int32>(strtol(token, nullptr, 10));
    else if (pDefault)
        pBuffer[2] = pDefault[2];

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

uint8* INIClass::Read3Bytes(uint8* pBuffer, const char* pSection,
                            const char* pKey, const uint8* pDefault)
{
    if (!pBuffer) return nullptr;

    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value)
    {
        if (pDefault)
        {
            pBuffer[0] = pDefault[0];
            pBuffer[1] = pDefault[1];
            pBuffer[2] = pDefault[2];
        }
        return pBuffer;
    }

    char temp[256];
    strncpy(temp, entry->Value, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char* token = strtok(temp, ", \t");
    if (token)
        pBuffer[0] = static_cast<uint8>(strtol(token, nullptr, 10));
    else if (pDefault)
        pBuffer[0] = pDefault[0];

    token = strtok(nullptr, ", \t");
    if (token)
        pBuffer[1] = static_cast<uint8>(strtol(token, nullptr, 10));
    else if (pDefault)
        pBuffer[1] = pDefault[1];

    token = strtok(nullptr, ", \t");
    if (token)
        pBuffer[2] = static_cast<uint8>(strtol(token, nullptr, 10));
    else if (pDefault)
        pBuffer[2] = pDefault[2];

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

    CCFileClass* pFile = new CCFileClass(pFileName);
    if (FileSystem::FileExists(pFileName))
    {
        pINI->ReadCCFile(pFile, false, false);
    }
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

bool CCINIClass::ReadCCFile(CCFileClass* pFile, bool bDigest, bool bLoadComments)
{
    if (!pFile) return false;

    bool result = LoadFile(pFile);

    if (result && bDigest)
    {
        Digested = true;
        // Compute CRC digest
        CRCValue = GetCRC();
        memcpy(Digest, &CRCValue, sizeof(CRCValue));
    }

    return result;
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