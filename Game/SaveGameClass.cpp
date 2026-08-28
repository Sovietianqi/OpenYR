#include "SaveGameClass.h"

#include <IO/FileSystem.h>
#include <cstring>

// ============================================================================
// SaveGameClass
// ============================================================================

SaveGameClass::SaveGameClass(CCFileClass* pFile)
    : File(pFile)
    , bOpen(false)
    , BytesWritten(0)
{
    bOpen = (pFile != nullptr);
}

SaveGameClass::~SaveGameClass()
{
    if (bOpen && File != nullptr)
        File->Close();
    bOpen = false;
}

void SaveGameClass::Write(int8 value)
{
    Write(&value, sizeof(value));
}

void SaveGameClass::Write(uint8 value)
{
    Write(&value, sizeof(value));
}

void SaveGameClass::Write(int16 value)
{
    Write(&value, sizeof(value));
}

void SaveGameClass::Write(uint16 value)
{
    Write(&value, sizeof(value));
}

void SaveGameClass::Write(int32 value)
{
    Write(&value, sizeof(value));
}

void SaveGameClass::Write(uint32 value)
{
    Write(&value, sizeof(value));
}

void SaveGameClass::Write(float value)
{
    Write(&value, sizeof(value));
}

void SaveGameClass::Write(double value)
{
    Write(&value, sizeof(value));
}

void SaveGameClass::Write(const char* pString)
{
    // Length-prefixed string: a 16-bit byte count followed by the raw
    // characters (no terminator is stored).
    int32 len = (pString != nullptr) ? static_cast<int32>(std::strlen(pString)) : 0;
    Write(static_cast<uint16>(len));
    if (len > 0)
        Write(pString, len);
}

void SaveGameClass::Write(const void* pData, int32 size)
{
    if (!bOpen || File == nullptr || pData == nullptr || size <= 0)
        return;

    int32 written = File->Write(pData, size);
    if (written > 0)
        BytesWritten += written;
}

void SaveGameClass::WriteHeader(int32 version, int32 frame, int32 randomSeed)
{
    // Magic + version + frame + random seed.
    Write(static_cast<uint32>(0x454D4147));   // "GAME"
    Write(version);
    Write(frame);
    Write(randomSeed);
}

// ============================================================================
// LoadGameClass
// ============================================================================

LoadGameClass::LoadGameClass(CCFileClass* pFile)
    : File(pFile)
    , bOpen(false)
    , BytesRead(0)
{
    bOpen = (pFile != nullptr);
}

LoadGameClass::~LoadGameClass()
{
    if (bOpen && File != nullptr)
        File->Close();
    bOpen = false;
}

bool LoadGameClass::Read(int8& value)
{
    return Read(&value, sizeof(value));
}

bool LoadGameClass::Read(uint8& value)
{
    return Read(&value, sizeof(value));
}

bool LoadGameClass::Read(int16& value)
{
    return Read(&value, sizeof(value));
}

bool LoadGameClass::Read(uint16& value)
{
    return Read(&value, sizeof(value));
}

bool LoadGameClass::Read(int32& value)
{
    return Read(&value, sizeof(value));
}

bool LoadGameClass::Read(uint32& value)
{
    return Read(&value, sizeof(value));
}

bool LoadGameClass::Read(float& value)
{
    return Read(&value, sizeof(value));
}

bool LoadGameClass::Read(double& value)
{
    return Read(&value, sizeof(value));
}

bool LoadGameClass::Read(char* pBuffer, int32 maxLen)
{
    uint16 len = 0;
    if (!Read(len))
        return false;
    if (static_cast<int32>(len) > maxLen - 1)
        return false;

    if (len > 0 && !Read(pBuffer, len))
        return false;
    pBuffer[len] = '\0';
    return true;
}

bool LoadGameClass::Read(void* pData, int32 size)
{
    if (!bOpen || File == nullptr || pData == nullptr || size <= 0)
        return false;

    int32 got = File->Read(pData, size);
    BytesRead += got;
    return got == size;
}

bool LoadGameClass::ReadHeader(int32& version, int32& frame, int32& randomSeed)
{
    uint32 magic = 0;
    if (!Read(magic))
        return false;
    if (magic != 0x454D4147)
        return false;

    if (!Read(version))
        return false;
    if (!Read(frame))
        return false;
    return Read(randomSeed);
}
