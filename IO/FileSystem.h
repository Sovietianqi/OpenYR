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

    // The attribute predicates the original tests through the
    // FILE_ATTRIBUTE bits (dwFileAttributes & 0x116): a directory, or a
    // hidden / system entry, is skipped before the name comparison.
    bool IsDirectory() const;
    bool IsHidden() const;

    bool IsFindValid;

private:
    void* m_pHandle;
    char m_FindName[MAX_PATH_LEN];
    char m_SearchPath[MAX_PATH_LEN];
    bool m_IsDirectory;
    bool m_IsHidden;
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

// ============================================================================
// CD - the forced-drive selector
//
//   CD::Set_Volume (asm 0x47909D) is a two-line setter:
//
//       if (CD::CD_Files_Local == 1) { ForcedCDNumber = 0xFFFFFFFE; return; }
//       if (volume >= 0)             ForcedCDNumber = volume;
//
//   The sentinel 0xFFFFFFFE means "the current drive", 0/1 name the two RA2
//   discs and 2 names Yuri's Revenge.  Game_ParsePKTs forces 0xFFFFFFFE for
//   the duration of its ".YRO" scan so the archives are read from the working
//   directory rather than a CD, then restores the previous value.  The
//   CD::Is_Available predicate (asm 0x4790EA) tests the same selector.
// ============================================================================
namespace CD {
    void Set_Volume(int32 nVolume);
    int32 Get_Volume();
    bool Is_Available(int32 nIndex);

    // The "CD files are local" flag the setter short-circuits on.
    extern bool CD_Files_Local;
}

// CD::ForceAvailable - the trampoline MixFileClass_CTOR reaches through the
// CD vtable at construction time.  Returns whether the requested drive is
// usable; on this platform the working directory always is.
bool CD_ForceAvailable(int32 nIndex);

// ============================================================================
// CD volume scanning (asm Get_CD_Index 0x4A80D0, CD_GetCDIndex 0x4A8xxx)
//
//  The retail binary locates the game discs by their volume label rather than
//  by drive letter, so a user whose CD-ROM landed on E: still works.  It walks
//  the drive letters and asks the OS for each volume's label, looking for the
//  string "YR1" (Yuri's Revenge disc 1).
//
//  Two globals drive the caching:
//      RequiredCDNumber    - the drive index the game last resolved (or
//                            0xFFFFFFFE for "current drive")
//      OldRequiredCDNumber - the previous value, restored by
//                            CD_SetRequiredCDIndex when a scan is reverted
// ============================================================================
// CD_GetCDIndex (asm): returns the cached RequiredCDNumber, scanning the
//   volume labels first when nothing has been resolved yet.
int32 CD_GetCDIndex();

// CD_SetRequiredCDIndex (asm): swaps in a new drive index, keeping the old one
//   in OldRequiredCDNumber so the previous selection can be restored.
void CD_SetRequiredCDIndex(int32 nVolume);

// Get_CD_Index (asm 0x4A80D0): probes drive letters for the "YR1" volume
//   label.  `startLetter` is the ASCII drive letter to begin at ('A'), and
//   `timeout` bounds how long the scan keeps retrying a busy drive (in
//   GetTickCount units).  Returns the zero-based drive index, or -1.
int32 Get_CD_Index(char startLetter, int32 timeout);