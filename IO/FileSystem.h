#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

enum class FileAccessMode {
    Read = 0,
    Write = 1,
    ReadWrite = 2,
};

enum class FileSeekMode {
    Set = 0,
    Current = 1,
    End = 2,
};

// ============================================================================
// RawFileClass - Low-level file I/O wrapper around C stdio
// ============================================================================
class RawFileClass {
public:
    RawFileClass();
    explicit RawFileClass(const char* pFilename);
    ~RawFileClass();

    bool Open(FileAccessMode mode);
    bool Open(const char* pFilename, FileAccessMode mode);
    void Close();
    int32 Read(void* pBuffer, int32 nSize);
    int32 Write(const void* pBuffer, int32 nSize);
    int32 Seek(int32 nOffset, FileSeekMode mode);
    int32 GetSize();
    int32 GetPosition();
    bool IsOpen() const;
    void SetName(const char* pName);
    const char* GetName() const;
    bool Exists() const;
    bool Delete();
    bool Create();

    char FileName[MAX_PATH_LEN];
    void* Handle;
};

// ============================================================================
// BufferIOFileClass - Buffered file I/O on top of RawFileClass
// ============================================================================
class BufferIOFileClass {
public:
    BufferIOFileClass();
    ~BufferIOFileClass();

    bool Open(const char* pFilename, FileAccessMode mode);
    void Close();
    int32 ReadBytes(void* pBuffer, int32 nSize);
    int32 WriteBytes(const void* pBuffer, int32 nSize);
    int32 Seek(int32 nOffset, FileSeekMode mode);
    int32 GetSize();
    bool IsOpen() const;

private:
    static constexpr int32 BufferSize = 4096;

    RawFileClass m_File;
    BYTE m_Buffer[BufferSize];
    int32 m_BufferPos;
    int32 m_BufferFill;
    bool m_bWriting;
};

// ============================================================================
// CCFileClass - High-level file class with MIX archive support
// ============================================================================
class CCFileClass {
public:
    CCFileClass();
    explicit CCFileClass(const char* pFilename);
    ~CCFileClass();

    bool Open(int32 mode);
    bool Open(const char* pFilename, int32 mode);
    void Close();
    int32 Read(void* pBuffer, int32 nSize);
    int32 Write(const void* pBuffer, int32 nSize);
    int32 Seek(int32 nOffset, int32 nOrigin);
    int32 GetSize();
    bool IsOpen() const;
    bool IsAvailable() const;
    const char* GetFileName() const;
    void SetName(const char* pName);

    bool Exists() const;
    int64 Size() const;

    char FileName[MAX_PATH_LEN];
    bool IsFileOpen;
    void* FileHandle;
    RawFileClass m_RawFile;
};

// ============================================================================
// CDFileClass - CD-ROM specific file class (extends CCFileClass)
// ============================================================================
class CDFileClass : public CCFileClass {
public:
    CDFileClass();
    explicit CDFileClass(const char* pFilename);
    ~CDFileClass();
};

// ============================================================================
// FileFindClass - Directory enumeration
// ============================================================================
class FileFindClass {
public:
    FileFindClass();
    ~FileFindClass();

    bool FindFirst(const char* pSearchPath);
    bool FindNext();
    void Close();
    const char* GetFileName() const;

    bool IsFindValid;

private:
    void* m_pHandle;
    char m_FindName[MAX_PATH_LEN];
    char m_SearchPath[MAX_PATH_LEN];
};

// ============================================================================
// FileSystem - Static file utility methods
// ============================================================================
class FileSystem {
public:
    static bool FileExists(const char* pFilename);
    static bool CreateDirectory(const char* pPath);
    static bool DeleteFile(const char* pFilename);
    static int32 GetFileSize(const char* pFilename);
};