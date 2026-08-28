#pragma once

#include <Core/Definitions.h>

// ============================================================================
// SaveGameClass / LoadGameClass
//
// Serialization core for the save-game format.  Every value is written with
// explicit little-endian primitives so the format is byte-stable across
// runs; the whole payload is framed with a magic number, format version,
// frame counter and the mission random seed.
// ============================================================================

class SaveGameClass
{
public:
    explicit SaveGameClass(class CCFileClass* pFile);
    ~SaveGameClass();

    SaveGameClass() = delete;
    SaveGameClass(const SaveGameClass&) = delete;
    SaveGameClass& operator=(const SaveGameClass&) = delete;

    // --- format primitives ---
    void Write(int8 value);
    void Write(uint8 value);
    void Write(int16 value);
    void Write(uint16 value);
    void Write(int32 value);
    void Write(uint32 value);
    void Write(float value);
    void Write(double value);
    void Write(const char* pString);
    void Write(const void* pData, int32 size);

    // --- file header ---
    void WriteHeader(int32 version, int32 frame, int32 randomSeed);

    bool IsOpen() const { return bOpen; }
    int32 GetBytesWritten() const { return BytesWritten; }

private:
    class CCFileClass* File;
    bool          bOpen;
    int32         BytesWritten;
};

class LoadGameClass
{
public:
    explicit LoadGameClass(class CCFileClass* pFile);
    ~LoadGameClass();

    LoadGameClass() = delete;
    LoadGameClass(const LoadGameClass&) = delete;
    LoadGameClass& operator=(const LoadGameClass&) = delete;

    // --- format primitives ---
    bool Read(int8& value);
    bool Read(uint8& value);
    bool Read(int16& value);
    bool Read(uint16& value);
    bool Read(int32& value);
    bool Read(uint32& value);
    bool Read(float& value);
    bool Read(double& value);
    bool Read(char* pBuffer, int32 maxLen);
    bool Read(void* pData, int32 size);

    // --- file header ---
    bool ReadHeader(int32& version, int32& frame, int32& randomSeed);

    bool IsOpen() const { return bOpen; }
    int32 GetBytesRead() const { return BytesRead; }

private:
    class CCFileClass* File;
    bool          bOpen;
    int32         BytesRead;
};
