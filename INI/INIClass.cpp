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

int32 INIClass::ReadHex(const char* pSection, const char* pKey, int32 nDefault)
{
    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) return nDefault;

    // INIClass_WriteHex writes a plain "%X"; the reader folds the same form
    // back through a base-16 conversion.  A leading '$' is tolerated so a
    // value that was written by WriteInteger's negative path still reads.
    const char* pValue = entry->Value;
    if (pValue[0] == '$') {
        ++pValue;
    }

    return static_cast<int32>(strtoul(pValue, nullptr, 16));
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

int32 INIClass::ReadIntHundredth(const char* pSection, const char* pKey, int32 nDefault)
{
    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) return nDefault;

    if (strchr(entry->Value, '.') == nullptr)
        return static_cast<int32>(strtol(entry->Value, nullptr, 10));

    float fValue = 0.0f;
    if (sscanf(entry->Value, "%f", &fValue) != 1) return nDefault;

    return static_cast<int32>(fValue * 100.0f);
}

bool INIClass::WriteIntHundredth(const char* pSection, const char* pKey, int32 nValue)
{
    char buffer[64];

    if (nValue < 0)
        snprintf(buffer, sizeof(buffer), "-%d.%02d", (-nValue) / 100, (-nValue) % 100);
    else
        snprintf(buffer, sizeof(buffer), "%d.%02d", nValue / 100, nValue % 100);

    return WriteString(pSection, pKey, buffer);
}

//========================================================================
// Word Pair / Scaled Integer Reading/Writing
//========================================================================

uint16* INIClass::Read2Words(uint16* pBuffer, const char* pSection, const char* pKey,
                             const uint16* pDefault)
{
    if (!pBuffer) return nullptr;

    pBuffer[0] = pDefault ? pDefault[0] : 0;
    pBuffer[1] = pDefault ? pDefault[1] : 0;

    char buffer[256];
    buffer[0] = '\0';
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));

    int32 nFirst = 0;
    int32 nSecond = 0;
    if (sscanf(buffer, "%d,%d", &nFirst, &nSecond) == 2)
    {
        pBuffer[0] = static_cast<uint16>(nFirst);
        pBuffer[1] = static_cast<uint16>(nSecond);
    }

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

int32 INIClass::ReadPipIdx(const char* pSection, const char* pKey, int32 nDefault)
{
    return ReadInteger(pSection, pKey, nDefault);
}

bool INIClass::WritePipIdx(const char* pSection, const char* pKey, int32 nValue)
{
    return WriteInteger(pSection, pKey, nValue);
}

int32 INIClass::ReadPipscaleIdx(const char* pSection, const char* pKey, int32 nDefault)
{
    return ReadInteger(pSection, pKey, nDefault);
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

BuildCat CCINIClass::GetBuildCat(const char* pSection, const char* pKey, BuildCat nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return nDefault;

    return ParseBuildCat(buffer);
}

Armor CCINIClass::GetArmorType(const char* pSection, const char* pKey, Armor nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return nDefault;

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

PipScale CCINIClass::GetPipScale(const char* pSection, const char* pKey, PipScale nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return nDefault;

    return ParsePipScale(buffer);
}

LandType CCINIClass::GetLandType(const char* pSection, const char* pKey, LandType nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return nDefault;

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

Powerup CCINIClass::GetPowerup(const char* pSection, const char* pKey, Powerup nDefault)
{
    static const char* const names[] = {
        "Money", "Unit", "HealBase", "Cloak", "Explosion", "Napalm", "Squad",
        "Darkness", "Reveal", "Armor", "Speed", "Firepower", "ICBM",
        "Invulnerability", "Veteran", "IonStorm", "Gas", "Tiberium", "Pod"
    };

    char buffer[128];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return nDefault;

    if (buffer[0] == '\0') return nDefault;

    for (int32 i = 0; i < static_cast<int32>(sizeof(names) / sizeof(names[0])); ++i)
    {
        if (_strcmpi(buffer, names[i]) == 0)
            return static_cast<Powerup>(i);
    }

    return nDefault;
}

int32 CCINIClass::GetOwners(const char* pSection, const char* pKey, int32 nDefault)
{
    char buffer[512];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return nDefault;

    if (buffer[0] == '\0') return nDefault;

    int32 mask = 0;
    char* cursor = buffer;

    while (*cursor != '\0')
    {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',')
            ++cursor;

        if (*cursor == '\0') break;

        char* start = cursor;
        while (*cursor != '\0' && *cursor != ',')
            ++cursor;

        char terminator = *cursor;
        *cursor = '\0';

        const char* name = start;
        while (*name == ' ') ++name;

        int32 index = HouseTypeClass::FindIndex(name);
        if (index >= 0 && index < 32)
            mask |= (1 << index);

        *cursor = terminator;
        if (terminator == ',') ++cursor;
    }

    return mask;
}

// ============================================================================
// CCINIClass::GetPrerequisiteList
//
//   Reads a comma separated prerequisite list.  Each entry is either the
//   literal "POWER" (stored as -1) or the ID of a techno type, which is
//   stored as its index inside TechnoTypeClass::Array.
// ============================================================================
bool CCINIClass::GetPrerequisiteList(const char* pSection, const char* pKey,
                                     DynamicVectorClass<int32>& rList)
{
    char buffer[0x80];
    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return false;

    if (buffer[0] == '\0')
        return false;

    DynamicVectorClass<int32> parsed;

    char* pToken = std::strtok(buffer, ",");
    while (pToken != nullptr)
    {
        if (pToken[0] != '\0')
        {
            if (_strcmpi(pToken, "POWER") == 0)
            {
                parsed.Add(static_cast<int32>(-1));
            }
            else
            {
                TechnoTypeClass* pType = TechnoTypeClass::Find(pToken);
                if (pType != nullptr)
                    parsed.Add(pType->Get_ArrayIndex());
            }
        }
        pToken = std::strtok(nullptr, ",");
    }

    rList = parsed;
    return true;
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

int32 CCINIClass::GetEdge(const char* pSection, const char* pKey, int32 nDefault)
{
    static const char* const directions[] = {
        "North", "East", "South", "West", "Air"
    };

    char buffer[0x80];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) == 0)
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
uint32 CCINIClass::GetAlliesBitfield(const char* pSection, const char* pKey,
                                     uint32 nDefault)
{
    char buffer[0x80];
    buffer[0] = '\0';

    if (ReadString(pSection, pKey, "", buffer, sizeof(buffer)) == 0)
        return nDefault;

    uint32 mask = 0;
    bool   any  = false;

    char* pToken = std::strtok(buffer, ",");
    if (pToken == nullptr)
        return nDefault;

    while (pToken != nullptr)
    {
        const int32 index = HouseClass::FindIndexByName(pToken);
        if (index >= 0 && index < 32)
        {
            mask |= (1u << index);
            any   = true;
        }

        pToken = std::strtok(nullptr, ",");
    }

    return any ? mask : nDefault;
}

//========================================================================
// 根据游戏行为，可知下面一批 Get* 是把既有读取入口暴露成"引用出参"
// 形式的便捷层：键存在则覆写，不存在则保留调用者带来的现值。
//========================================================================

int32 INIClass::GetString(const char* pSection, const char* pKey,
                          char* pBuffer, size_t bufferSize)
{
    return ReadString(pSection, pKey, pBuffer, pBuffer, bufferSize);
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

int32 INIClass::Section_GetValueCount(const char* pSection)
{
    return GetKeyCount(pSection);
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

int32 INIClass::GetUnicodeString(const char* pSection, const char* pKey,
                                 const wchar_t* pDefault, wchar_t* pBuffer, size_t nChars)
{
    if (!pBuffer || nChars == 0) return 0;

    // 根据游戏行为，可知宽字符串读取时，文本里的 \xXXXX 转义被还原成
    // 对应码点，其余字符按 ANSI 逐字节展宽；超长按缓冲截断。
    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) {
        size_t n = 0;
        if (pDefault) {
            while (pDefault[n] && n < nChars - 1) { pBuffer[n] = pDefault[n]; ++n; }
        }
        pBuffer[n] = L'\0';
        return static_cast<int32>(n);
    }

    size_t out = 0;
    for (const char* p = entry->Value; *p && out < nChars - 1; ++p) {
        wchar_t wc;
        if (p[0] == '\\' && p[1] == 'x' &&
            isxdigit(static_cast<unsigned char>(p[2])) &&
            isxdigit(static_cast<unsigned char>(p[3])) &&
            isxdigit(static_cast<unsigned char>(p[4])) &&
            isxdigit(static_cast<unsigned char>(p[5])))
        {
            wc = static_cast<wchar_t>(strtol(p + 2, nullptr, 16));
            p += 5;
        } else {
            wc = static_cast<wchar_t>(static_cast<unsigned char>(*p));
        }
        pBuffer[out++] = wc;
    }
    pBuffer[out] = L'\0';
    return static_cast<int32>(out);
}

float* INIClass::Read3Floats(float* pBuffer, const char* pSection, const char* pKey,
                             const float* pDefault)
{
    if (!pBuffer) return nullptr;
    if (pDefault) pBuffer[0] = pDefault[0], pBuffer[1] = pDefault[1], pBuffer[2] = pDefault[2];

    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) return pBuffer;

    // 根据游戏行为，可知三浮点按 "x,y,z" 逗号拆分，缺席的分量保留原值。
    float v[3] = { pBuffer[0], pBuffer[1], pBuffer[2] };
    if (sscanf(entry->Value, "%f,%f,%f", &v[0], &v[1], &v[2]) >= 1) {
        pBuffer[0] = v[0];
        pBuffer[1] = v[1];
        pBuffer[2] = v[2];
    }
    return pBuffer;
}

bool INIClass::ReadScenario(const char* pFileName)
{
    if (!pFileName) return false;

    // 根据游戏行为，可知场景读取就是把整个场景文件并进当前 INI 对象，
    // 与既有节合并、重复键以后读入的为准。
    CCFileClass file(pFileName);
    if (!file.IsAvailable()) return false;
    return LoadFile(&file);
}

bool INIClass::SaveMapPreview(const char* pSection, const void* pData, int32 nSize)
{
    if (!pSection || !pData || nSize <= 0) return false;

    // 根据游戏行为，可知预览包按 Base64 编码、切成固定宽度行、以编号键
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

bool INIClass::ParseSideHouses(const char* pSection, const char* pKey)
{
    INIEntry* entry = FindEntry(pSection, pKey);
    if (!entry || !entry->Value) return false;

    // 根据游戏行为，可知回写时把逗号分隔表里的空白剥掉再拼接，
    // 使后续读取者拿到规范化的名字串。
    static char buffer[1024];
    buffer[0] = '\0';
    size_t len = 0;

    char* context = nullptr;
    char work[1024];
    strncpy(work, entry->Value, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    for (char* tok = strtok(work, ","); tok; tok = strtok(nullptr, ",")) {
        while (*tok == ' ' || *tok == '\t') ++tok;
        char* end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) --end;
        *end = '\0';
        if (*tok == '\0') continue;

        size_t n = strlen(tok);
        if (len + n + 2 >= sizeof(buffer)) break;
        if (len > 0) buffer[len++] = ',';
        memcpy(buffer + len, tok, n);
        len += n;
        buffer[len] = '\0';
    }

    return WriteString(pSection, pKey, buffer);
}

bool INIClass::WriteUnicodeEscaped(const char* pSection, const char* pKey, const wchar_t* pValue)
{
    if (!pValue) return false;

    // 根据游戏行为，可知宽字符写出口中，超出 ASCII 范围的字符以
    // \xXXXX 四位十六进制转义，其余原样通过。
    char buffer[2048];
    size_t len = 0;
    for (const wchar_t* p = pValue; *p; ++p) {
        if (*p < 0x80) {
            if (len + 1 >= sizeof(buffer)) break;
            buffer[len++] = static_cast<char>(*p);
        } else {
            if (len + 6 >= sizeof(buffer)) break;
            snprintf(buffer + len, 7, "\\x%04X", static_cast<uint32>(*p) & 0xFFFF);
            len += 6;
        }
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

TechnoTypeClass* CCINIClass::GetTechnoPrerequisite(const char* pSection, const char* pKey,
                                                   TechnoTypeClass* pDefault)
{
    // 根据游戏行为，可知前置需求读取与普通类型查找同路，只是缺省回落
    // 语义按调用者给的指针走。
    TechnoTypeClass* pType = FindTechnoTypeByName(pSection, pKey);
    return pType ? pType : pDefault;
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

int32 CCINIClass::GetSide(const char* pSection, const char* pKey, int32 nDefault)
{
    // 根据游戏行为，可知阵营按名字在阵营表里查序号，未知名回落缺省。
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0' || IsBlank(buffer)) return nDefault;

    int32 index = SideClass::From_Name(buffer);
    return (index >= 0) ? index : nDefault;
}

int32 CCINIClass::GetTheme(const char* pSection, const char* pKey, int32 nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0' || IsBlank(buffer)) return nDefault;

    int32 index = ThemeClass::FindIndex(buffer);
    return (index >= 0) ? index : nDefault;
}

int32 CCINIClass::GetVoxIndex(const char* pSection, const char* pKey, int32 nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0' || IsBlank(buffer)) return nDefault;

    int32 index = VocClass::FindIndexOfName(buffer);
    return (index >= 0) ? index : nDefault;
}

int32 CCINIClass::GetColorSchemeIdx(const char* pSection, const char* pKey, int32 nDefault)
{
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0' || IsBlank(buffer)) return nDefault;

    int32 index = ColorScheme::FindIndex(buffer);
    return (index >= 0) ? index : nDefault;
}

int32 CCINIClass::GetCategoryIdx(const char* pSection, const char* pKey, int32 nDefault)
{
    return static_cast<int32>(GetCategory(pSection, pKey, static_cast<Category>(nDefault)));
}

int32 CCINIClass::GetSWTypeIndex(const char* pSection, const char* pKey, int32 nDefault)
{
    char buffer[256];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0' || IsBlank(buffer)) return nDefault;

    SuperWeaponTypeClass* pType = SuperWeaponTypeClass::Find(buffer);
    if (pType == nullptr) return nDefault;

    // 根据游戏行为，可知超级武器按其在类型表中的注册序号对外暴露。
    for (int32 i = 0; i < SuperWeaponTypeClass::GetCount(); ++i) {
        if (SuperWeaponTypeClass::FindByIndex(i) == pType) return i;
    }
    return nDefault;
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

LandType CCINIClass::GetLand(const char* pSection, const char* pKey, LandType nDefault)
{
    return GetLandType(pSection, pKey, nDefault);
}

TheaterType CCINIClass::GetTheater(const char* pSection, const char* pKey, TheaterType nDefault)
{
    // 根据游戏行为，可知战场环境以既知名字串出现，顺序固定：
    // 温带、雪地、都市、沙漠、月球、新都市。
    static const char* const theaters[] = {
        "TEMPERATE", "SNOW", "URBAN", "DESERT", "LUNAR", "NEWURBAN"
    };

    char buffer[64];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0' || IsBlank(buffer)) return nDefault;

    for (int32 i = 0; i < 6; ++i) {
        if (strcasecmp(theaters[i], buffer) == 0) return static_cast<TheaterType>(i);
    }
    return nDefault;
}

AbstractType CCINIClass::GetFactory(const char* pSection, const char* pKey, AbstractType nDefault)
{
    // 根据游戏行为，可知生产类别以类型 ID 名串出现，未知名回落缺省。
    static const struct {
        const char*  Name;
        AbstractType Value;
    } factories[] = {
        { "BuildingType", AbstractType::BuildingType },
        { "InfantryType", AbstractType::InfantryType },
        { "UnitType",     AbstractType::UnitType },
        { "AircraftType", AbstractType::AircraftType },
    };

    char buffer[64];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0' || IsBlank(buffer)) return nDefault;

    for (const auto& f : factories) {
        if (strcasecmp(f.Name, buffer) == 0) return f.Value;
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

bool CCINIClass::WriteArmor(const char* pSection, const char* pKey, Armor nValue)
{
    // 根据游戏行为，可知装甲以既知名表反查成名字串写入。
    int32 idx = static_cast<int32>(nValue);
    if (idx < 0 || idx > 10) idx = 0;
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

bool CCINIClass::WriteTheater(const char* pSection, const char* pKey, TheaterType nValue)
{
    // 根据游戏行为，可知战场环境以名字串写回，序号越界视为温带。
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

bool CCINIClass::WriteColorScheme(const char* pSection, const char* pKey, int32 nIndex)
{
    return WriteInteger(pSection, pKey, nIndex);
}

bool CCINIClass::WriteHouseIndex(const char* pSection, const char* pKey, int32 nIndex)
{
    // 根据游戏行为，可知国家以其 ID 名写回，序号越界时清空键值。
    if (nIndex < 0 || nIndex >= HouseTypeClass::ArrayCount || !HouseTypeClass::Array[nIndex]) {
        return WriteString(pSection, pKey, "");
    }
    return WriteString(pSection, pKey, HouseTypeClass::Array[nIndex]->ID);
}

bool CCINIClass::WritePipscaleIdx(const char* pSection, const char* pKey, int32 nIndex)
{
    return WriteString(pSection, pKey, PipScaleIdxToName(nIndex));
}

//========================================================================
// CLSID / string-table / voice-list
//========================================================================

bool CCINIClass::ReadCLSID(const char* pSection, const char* pKey, GUID* pOut)
{
    if (!pOut) return false;

    // 根据游戏行为，可知 CLSID 串支持带花括号与不带两种形态：先剥掉
    // 花括号，再按标准八段位拆成二进制。
    char buffer[128];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0') return false;

    char* p = buffer;
    if (*p == '{') ++p;
    char* end = p + strlen(p);
    while (end > p && (end[-1] == '}' || end[-1] == ' ')) --end;
    *end = '\0';

    unsigned int d1 = 0;
    unsigned int d2 = 0, d3 = 0;
    unsigned int b[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    if (sscanf(p, "%8X-%4hX-%4hX-%2hhX%2hhX-%2hhX%2hhX%2hhX%2hhX%2hhX%2hhX",
               &d1, &d2, &d3, &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7]) != 11) {
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

    // 根据游戏行为，可知语音名字串按逗号拆分，逐个落索引；未知名不
    // 占位直接跳过。
    char buffer[512];
    ReadString(pSection, pKey, "", buffer, sizeof(buffer));
    if (buffer[0] == '\0' || IsBlank(buffer)) return false;

    char* context = nullptr;
    for (char* tok = strtok(buffer, ","); tok; tok = strtok(nullptr, ",")) {
        while (*tok == ' ' || *tok == '\t') ++tok;
        char* end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) --end;
        *end = '\0';
        if (*tok == '\0') continue;

        int32 index = VocClass::FindIndexOfName(tok);
        if (index >= 0) {
            rList.AddItem(index);
        }
    }
    return rList.Count > 0;
}
