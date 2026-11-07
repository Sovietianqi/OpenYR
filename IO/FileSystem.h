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

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知原版文件层以原始 WW 命名暴露同一组操作；这里
    // 与工程内语义化接口一一对应。
    // ------------------------------------------------------------------------
    virtual int32 FileSize();
    virtual int32 GetFileTime();
    virtual bool  SetFileTime(int32 nTime);
    virtual bool  HasHandle() const;
    virtual int32 MoveFilePointer(int32 nOffset, FileSeekMode mode);
    virtual int32 SetFilePointer(int32 nOffset);
    virtual bool  OpenFile(FileAccessMode mode);
    virtual int32 ReadNextBytes(void* pBuffer, int32 nSize);
    virtual bool  SetFileName(const char* pName);

    // asm: RawFileClass_FileExists (0x65CBF0) - probe the file.  A null name
    // fails; otherwise the file is opened (mode 1) if not already open, probed,
    // closed again and the verdict cached.  nArg selects the "open if needed"
    // path used by BufferIOFileClass.
    bool FileExists(int32 nArg = 1);

    // asm: [this+0Ch] / [this+10h] - the sub-file window.
    //
    //   A RawFileClass can be bound to a byte range inside an already-open file
    //   (how MIX members are served without a second open).  FileSize() and
    //   MoveFilePointer() report positions relative to SubFileStart and clamp to
    //   SubFileStart + SubFileLength; RawFileClass_SetFilePointer re-binds it.
    int32 SubFileStart;    // [this+0Ch]
    int32 SubFileLength;   // [this+10h]; -1 while the true size is unknown

    char FileName[MAX_PATH_LEN];
    void* Handle;
};

// ============================================================================
// RAMFileClass - a file whose backing store is a plain memory buffer
//
//   The retail engine uses this to feed a byte blob (e.g. an INI embedded in a
//   string) into the same file interface INIClass consumes.  The constructor
//   wraps an existing buffer, or allocates one when handed only a size.
// ============================================================================
class RAMFileClass {
public:
    RAMFileClass();
    // asm: sub_65C250 - wrap <pData> (borrowed) as a Size-byte file.  When
    // pData is null and Size > 0, an owned buffer of Size bytes is allocated.
    RAMFileClass(void* pData, int32 nSize);
    explicit RAMFileClass(int32 nSize);
    ~RAMFileClass();

    // asm: RAMFileClass_OpenFile (0x65C38F) - (re)arm the read/write cursor.
    // Mode 2 re-zeroes the logical size; the return is HasHandle().
    bool OpenFile(int32 nMode);
    // asm: RAMFileClass_ReadNextBytes (0x65C3A0)
    int32 ReadNextBytes(void* pBuffer, int32 nSize);
    // asm: RAMFileClass_MoveFilePointer (0x65C420)
    int32 MoveFilePointer(int32 nDistance, int32 nMethod);
    // asm: RAMFileClass_FileSize (0x65C4D0)
    int32 FileSize();
    // asm: RAMFileClass_HasHandle (0x65C2F0) / _FileExists (0x65C2E8)
    bool HasHandle() const;
    bool FileExists(int32 nArg = 0) const;
    // asm: RAMFileClass_GetFileName returns the literal "UNKNOWN"
    const char* GetFileName() const;
    bool SetFileName(const char* pName);
    // asm: RAMFileClass_GetFileTime (0) / _SetFileTime (true)
    int32 GetFileTime();
    bool  SetFileTime(int32 nTime);

    void Close();
    // asm: sub_65C300 - clear the logical size (refused while open).
    bool Reset();

    // Accessors for the backing store.
    uint8* GetBuffer() const { return Buffer; }
    int32  GetCapacity() const { return Capacity; }
    int32  GetPosition() const { return Position; }
    int32  GetMode() const { return OpenMode; }
    bool   IsOpen() const { return Open; }
    bool   SetBuffer(void* pData, int32 nSize);

private:
    uint8* Buffer;     // [this+8]
    int32  Capacity;   // [this+0Ch]
    int32  Size;       // [this+10h]
    int32  Position;   // [this+14h]
    int32  OpenMode;   // [this+18h]
    bool   OwnsBuffer; // [this+1Dh] - free Buffer on close/destroy
    bool   Open;       // [this+1Ch]
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

    // ------------------------------------------------------------------------
    // 根据游戏行为，原版 vt_CCFileClass 的 16 个槽位依次为：析构、文件名、
    // 设置文件名、Create、Delete、Is_Available、HasHandle、OpenFile、
    // OpenEx、ReadNextBytes、MoveFilePointer、FileSize、WriteBytes、Close、
    // GetFileTime、SetFileTime、CDCheck。下面把这套原始接口补齐。
    // ------------------------------------------------------------------------
    virtual void* AllocateMemory(int32 nSize);
    virtual bool  CDCheck();
    virtual bool  OpenEx(const char* pFilename, int32 nMode);

    // asm: CCFileClass::Is_Available  (vtable slot 5)
    bool Is_Available(int32 nMode);
    // asm: CCFileClass_HasHandle      (vtable slot 6)
    bool HasHandle();
    // asm: CCFileClass_OpenFile       (vtable slot 7)
    bool OpenFile(int32 nMode);
    // asm: CCFileClass_ReadNextBytes  (vtable slot 9)
    int32 ReadNextBytes(void* pBuffer, int32 nSize);
    // asm: CCFileClass_MoveFilePointer(vtable slot 10)
    int32 MoveFilePointer(int32 nDistance, int32 nMethod);
    // asm: CCFileClass_FileSize       (vtable slot 11)
    int32 FileSize();
    // asm: CCFileClass_WriteBytes     (vtable slot 12)
    int32 WriteBytes(const void* pBuffer, int32 nSize);
    // asm: CCFileClass_GetFileTime    (vtable slot 14)
    int32 GetFileTime();
    // asm: CCFileClass_SetFileTime    (vtable slot 15)
    bool  SetFileTime(int32 nTime);
    // asm: CCFileClass_SetFileName    (vtable slot 2)
    bool  SetFileName(const char* pName);
    // asm: CCFileClass_Create         (vtable slot 3)
    bool  Create();
    // asm: CCFileClass_Delete         (vtable slot 4)
    bool  Delete();

    char FileName[MAX_PATH_LEN];
    bool IsFileOpen;
    void* FileHandle;
    RawFileClass m_RawFile;

    // asm: [this+58h] 指向 MIX 命中项的常驻缓冲；非空表示该文件由 MIX 提供，
    //       此时所有读写/定位都在内存里完成。
    //      [this+64h] 当前在内存缓冲中的读指针。
    //      [this+5Ch] 缓冲总字节数（即 MIX 条目长度）。
    //      [this+68h] Is_Available 的缓存结果：0=未判定，1=可用，2=不可用。
    void* MixBuffer;
    int32 MixBufferSize;
    int32 MixBufferPos;
    int32 AvailabilityCache;
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
 //   CD::Set_Volume is a two-line setter:
//
//       if (CD::CD_Files_Local == 1) { ForcedCDNumber = 0xFFFFFFFE; return; }
//       if (volume >= 0)             ForcedCDNumber = volume;
//
//   The sentinel 0xFFFFFFFE means "the current drive", 0/1 name the two RA2
//   discs and 2 names Yuri's Revenge.  Game_ParsePKTs forces 0xFFFFFFFE for
//   the duration of its ".YRO" scan so the archives are read from the working
//   directory rather than a CD, then restores the previous value.  The
 //   CD::Is_Available predicate tests the same selector.
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

 // Get_CD_Index: probes drive letters for the "YR1" volume
//   label.  `startLetter` is the ASCII drive letter to begin at ('A'), and
//   `timeout` bounds how long the scan keeps retrying a busy drive (in
//   GetTickCount units).  Returns the zero-based drive index, or -1.
int32 Get_CD_Index(char startLetter, int32 timeout);