#include "IO/FileSystem.h"
#include "IO/MixFileClass.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <cerrno>
#include <ctime>
#include <cstdint>
#include <utime.h>

// ============================================================================
// RawFileClass implementation
// ============================================================================

// asm: RawFileClass_CTOR (0x65CA70) - primes [this+10h] = [this+14h] = -1 and
// zeroes the flag byte / name-ownership fields.
RawFileClass::RawFileClass()
    : SubFileStart(0)
    , SubFileLength(-1)
    , Handle(nullptr)
{
    FileName[0] = '\0';
}

RawFileClass::RawFileClass(const char* pFilename)
    : SubFileStart(0)
    , SubFileLength(-1)
    , Handle(nullptr)
{
    if (pFilename)
    {
        int32 i = 0;
        while (pFilename[i] && i < MAX_PATH_LEN - 1)
        {
            FileName[i] = pFilename[i];
            ++i;
        }
        FileName[i] = '\0';
    }
    else
    {
        FileName[0] = '\0';
    }
}

RawFileClass::~RawFileClass()
{
    Close();
}

bool RawFileClass::Open(FileAccessMode mode)
{
    if (Handle)
        Close();

    if (FileName[0] == '\0')
        return false;

    const char* pMode = "rb";
    switch (mode)
    {
    case FileAccessMode::Read:
        pMode = "rb";
        break;
    case FileAccessMode::Write:
        pMode = "wb";
        break;
    case FileAccessMode::ReadWrite:
        pMode = "r+b";
        break;
    }

    Handle = std::fopen(FileName, pMode);

    // asm: RawFileClass_OpenFile resets the sub-file window: [this+0Ch] = 0 and
    // [this+10h] = -1 (size not yet known) then, on success, re-seeks.
    SubFileStart  = 0;
    SubFileLength = -1;

    return (Handle != nullptr);
}

bool RawFileClass::Open(const char* pFilename, FileAccessMode mode)
{
    SetName(pFilename);
    return Open(mode);
}

void RawFileClass::Close()
{
    if (Handle)
    {
        std::fclose(static_cast<std::FILE*>(Handle));
        Handle = nullptr;
    }
}

int32 RawFileClass::Read(void* pBuffer, int32 nSize)
{
    if (!Handle || !pBuffer || nSize <= 0)
        return 0;

    size_t result = std::fread(pBuffer, 1, static_cast<size_t>(nSize),
        static_cast<std::FILE*>(Handle));
    return static_cast<int32>(result);
}

int32 RawFileClass::Write(const void* pBuffer, int32 nSize)
{
    if (!Handle || !pBuffer || nSize <= 0)
        return 0;

    size_t result = std::fwrite(pBuffer, 1, static_cast<size_t>(nSize),
        static_cast<std::FILE*>(Handle));
    return static_cast<int32>(result);
}

int32 RawFileClass::Seek(int32 nOffset, FileSeekMode mode)
{
    if (!Handle)
        return 0;

    int nOrigin = SEEK_SET;
    switch (mode)
    {
    case FileSeekMode::Set:
        nOrigin = SEEK_SET;
        break;
    case FileSeekMode::Current:
        nOrigin = SEEK_CUR;
        break;
    case FileSeekMode::End:
        nOrigin = SEEK_END;
        break;
    }

    std::fseek(static_cast<std::FILE*>(Handle), nOffset, nOrigin);
    return static_cast<int32>(std::ftell(static_cast<std::FILE*>(Handle)));
}

// asm: RawFileClass_FileSize (0x65D0C0).  Kept as a thin alias so the buffered
// and high-level layers share one sub-file-aware size computation.
int32 RawFileClass::GetSize()
{
    return FileSize();
}

int32 RawFileClass::GetPosition()
{
    if (!Handle)
        return 0;

    return static_cast<int32>(std::ftell(static_cast<std::FILE*>(Handle)));
}

bool RawFileClass::IsOpen() const
{
    return (Handle != nullptr);
}

void RawFileClass::SetName(const char* pName)
{
    if (pName)
    {
        int32 i = 0;
        while (pName[i] && i < MAX_PATH_LEN - 1)
        {
            FileName[i] = pName[i];
            ++i;
        }
        FileName[i] = '\0';
    }
    else
    {
        FileName[0] = '\0';
    }
}

const char* RawFileClass::GetName() const
{
    return FileName;
}

bool RawFileClass::Exists() const
{
    if (FileName[0] == '\0')
        return false;

    // Use stat() to check if the file exists
    struct stat st;
    int result = stat(FileName, &st);
    return (result == 0 && S_ISREG(st.st_mode));
}

bool RawFileClass::Delete()
{
    if (FileName[0] == '\0')
        return false;

    Close();

    int result = remove(FileName);
    return (result == 0);
}

bool RawFileClass::Create()
{
    if (FileName[0] == '\0')
        return false;

    // Create an empty file by opening in write mode and closing
    std::FILE* pFile = std::fopen(FileName, "wb");
    if (!pFile)
        return false;

    std::fclose(pFile);
    return true;
}

// ============================================================================
// BufferIOFileClass implementation
// ============================================================================

BufferIOFileClass::BufferIOFileClass()
    : m_BufferPos(0)
    , m_BufferFill(0)
    , m_bWriting(false)
{
    std::memset(m_Buffer, 0, BufferSize);
}

BufferIOFileClass::~BufferIOFileClass()
{
    Close();
}

bool BufferIOFileClass::Open(const char* pFilename, FileAccessMode mode)
{
    Close();

    bool bOpened = m_File.Open(pFilename, mode);
    if (!bOpened)
        return false;

    m_bWriting = (mode == FileAccessMode::Write || mode == FileAccessMode::ReadWrite);
    m_BufferPos = 0;
    m_BufferFill = 0;

    return true;
}

void BufferIOFileClass::Close()
{
    // Flush write buffer if writing
    if (m_bWriting && m_BufferPos > 0 && m_File.IsOpen())
    {
        m_File.Write(m_Buffer, m_BufferPos);
    }

    m_File.Close();
    m_BufferPos = 0;
    m_BufferFill = 0;
    m_bWriting = false;
}

int32 BufferIOFileClass::ReadBytes(void* pBuffer, int32 nSize)
{
    if (!pBuffer || nSize <= 0 || !m_File.IsOpen())
        return 0;

    BYTE* pDest = static_cast<BYTE*>(pBuffer);
    int32 bytesRead = 0;

    while (bytesRead < nSize)
    {
        int32 remaining = nSize - bytesRead;

        // If we have buffered data, use it
        if (m_BufferPos < m_BufferFill)
        {
            int32 bufAvailable = m_BufferFill - m_BufferPos;
            int32 toCopy = (bufAvailable < remaining) ? bufAvailable : remaining;

            std::memcpy(pDest + bytesRead, m_Buffer + m_BufferPos,
                static_cast<size_t>(toCopy));
            m_BufferPos += toCopy;
            bytesRead += toCopy;
        }
        else
        {
            // Buffer is empty, refill it
            int32 toRead = (remaining > BufferSize) ? remaining : BufferSize;
            int32 actuallyRead = m_File.Read(m_Buffer, toRead);

            if (actuallyRead <= 0)
                break; // EOF or error

            m_BufferFill = actuallyRead;
            m_BufferPos = 0;
        }
    }

    return bytesRead;
}

int32 BufferIOFileClass::WriteBytes(const void* pBuffer, int32 nSize)
{
    if (!pBuffer || nSize <= 0 || !m_File.IsOpen())
        return 0;

    const BYTE* pSrc = static_cast<const BYTE*>(pBuffer);
    int32 bytesWritten = 0;

    while (bytesWritten < nSize)
    {
        int32 remaining = nSize - bytesWritten;
        int32 bufSpace = BufferSize - m_BufferPos;

        if (bufSpace > 0)
        {
            int32 toCopy = (bufSpace < remaining) ? bufSpace : remaining;

            std::memcpy(m_Buffer + m_BufferPos, pSrc + bytesWritten,
                static_cast<size_t>(toCopy));
            m_BufferPos += toCopy;
            bytesWritten += toCopy;
        }

        // Flush if buffer is full
        if (m_BufferPos >= BufferSize)
        {
            m_File.Write(m_Buffer, m_BufferPos);
            m_BufferPos = 0;
        }
    }

    return bytesWritten;
}

int32 BufferIOFileClass::Seek(int32 nOffset, FileSeekMode mode)
{
    // Flush buffer before seeking
    if (m_BufferPos > 0 && m_File.IsOpen())
    {
        if (m_bWriting)
        {
            m_File.Write(m_Buffer, m_BufferPos);
        }
        m_BufferPos = 0;
        m_BufferFill = 0;
    }

    return m_File.Seek(nOffset, mode);
}

int32 BufferIOFileClass::GetSize()
{
    return m_File.GetSize();
}

bool BufferIOFileClass::IsOpen() const
{
    return m_File.IsOpen();
}

// ============================================================================
// CCFileClass implementation
// ============================================================================

CCFileClass::CCFileClass()
    : IsFileOpen(false)
    , FileHandle(nullptr)
    , MixBuffer(nullptr)
    , MixBufferSize(0)
    , MixBufferPos(0)
    , AvailabilityCache(0)
{
    FileName[0] = '\0';
}

CCFileClass::CCFileClass(const char* pFilename)
    : IsFileOpen(false)
    , FileHandle(nullptr)
    , MixBuffer(nullptr)
    , MixBufferSize(0)
    , MixBufferPos(0)
    , AvailabilityCache(0)
{
    if (pFilename)
    {
        int32 i = 0;
        while (pFilename[i] && i < MAX_PATH_LEN - 1)
        {
            FileName[i] = pFilename[i];
            ++i;
        }
        FileName[i] = '\0';
    }
    else
    {
        FileName[0] = '\0';
    }
}

CCFileClass::~CCFileClass()
{
    Close();

    // asm: CCFileClass_SDDTOR -> Buffer::~Buffer on [esi+58h]; the MIX-backed
    // residency buffer is released here.
    if (MixBuffer)
    {
        YRMemory::Deallocate(MixBuffer);
        MixBuffer = nullptr;
    }
    MixBufferSize = 0;
    MixBufferPos = 0;
}

bool CCFileClass::Open(int32 mode)
{
    if (IsFileOpen)
        Close();

    if (FileName[0] == '\0')
        return false;

    FileAccessMode accessMode = FileAccessMode::Read;
    switch (mode)
    {
    case 0:
        accessMode = FileAccessMode::Read;
        break;
    case 1:
        accessMode = FileAccessMode::Write;
        break;
    case 2:
        accessMode = FileAccessMode::ReadWrite;
        break;
    default:
        accessMode = FileAccessMode::Read;
        break;
    }

    // Try to open from MIX archive first, then fall back to raw file.
    // The MIX archive lookup is handled by the CCFileClass layer above.
    bool bOpened = m_RawFile.Open(FileName, accessMode);
    if (bOpened)
    {
        IsFileOpen = true;
        FileHandle = m_RawFile.Handle;
    }
    return bOpened;
}

bool CCFileClass::Open(const char* pFilename, int32 mode)
{
    SetName(pFilename);
    return Open(mode);
}

void CCFileClass::Close()
{
    // asm: CCFileClass_Close (0x473CD0) - reset the MIX residency buffer
    // ([this+58h],[this+5Ch],[this+64h]) then close the underlying file.
    if (MixBuffer)
    {
        YRMemory::Deallocate(MixBuffer);
        MixBuffer = nullptr;
    }
    MixBufferSize = 0;
    MixBufferPos = 0;
    AvailabilityCache = 0;

    if (IsFileOpen)
    {
        m_RawFile.Close();
        IsFileOpen = false;
        FileHandle = nullptr;
    }
    else
    {
        m_RawFile.Close();
    }
}

int32 CCFileClass::Read(void* pBuffer, int32 nSize)
{
    if (!IsFileOpen || !pBuffer || nSize <= 0)
        return 0;

    return m_RawFile.Read(pBuffer, nSize);
}

int32 CCFileClass::Write(const void* pBuffer, int32 nSize)
{
    if (!IsFileOpen || !pBuffer || nSize <= 0)
        return 0;

    return m_RawFile.Write(pBuffer, nSize);
}

int32 CCFileClass::Seek(int32 nOffset, int32 nOrigin)
{
    if (!IsFileOpen)
        return 0;

    FileSeekMode seekMode = FileSeekMode::Set;
    switch (nOrigin)
    {
    case 0: // SEEK_SET
        seekMode = FileSeekMode::Set;
        break;
    case 1: // SEEK_CUR
        seekMode = FileSeekMode::Current;
        break;
    case 2: // SEEK_END
        seekMode = FileSeekMode::End;
        break;
    default:
        seekMode = FileSeekMode::Set;
        break;
    }

    return m_RawFile.Seek(nOffset, seekMode);
}

int32 CCFileClass::GetSize()
{
    if (!IsFileOpen)
        return 0;

    return m_RawFile.GetSize();
}

bool CCFileClass::IsOpen() const
{
    return IsFileOpen;
}

bool CCFileClass::IsAvailable() const
{
    // Check if the file exists (in MIX or on disk)
    if (IsFileOpen)
        return true;

    if (FileName[0] == '\0')
        return false;

    // Check if file exists on disk
    struct stat st;
    return (stat(FileName, &st) == 0 && S_ISREG(st.st_mode));
}

const char* CCFileClass::GetFileName() const
{
    return FileName;
}

void CCFileClass::SetName(const char* pName)
{
    if (pName)
    {
        int32 i = 0;
        while (pName[i] && i < MAX_PATH_LEN - 1)
        {
            FileName[i] = pName[i];
            ++i;
        }
        FileName[i] = '\0';
    }
    else
    {
        FileName[0] = '\0';
    }
}

bool CCFileClass::Exists() const
{
    if (FileName[0] == '\0')
        return false;

    struct stat st;
    int result = stat(FileName, &st);
    return (result == 0 && S_ISREG(st.st_mode));
}

int64 CCFileClass::Size() const
{
    if (FileName[0] == '\0')
        return 0;

    struct stat st;
    int result = stat(FileName, &st);
    if (result != 0 || !S_ISREG(st.st_mode))
        return 0;

    return static_cast<int64>(st.st_size);
}

// ============================================================================
// CDFileClass implementation
// ============================================================================

CDFileClass::CDFileClass()
    : CCFileClass()
{
}

CDFileClass::CDFileClass(const char* pFilename)
    : CCFileClass(pFilename)
{
}

CDFileClass::~CDFileClass()
{
}

// ============================================================================
// FileFindClass implementation
// ============================================================================

FileFindClass::FileFindClass()
    : IsFindValid(false)
    , m_pHandle(nullptr)
    , m_IsDirectory(false)
    , m_IsHidden(false)
{
    m_FindName[0] = '\0';
    m_SearchPath[0] = '\0';
}

FileFindClass::~FileFindClass()
{
    Close();
}

bool FileFindClass::FindFirst(const char* pSearchPath)
{
    Close();

    if (!pSearchPath || pSearchPath[0] == '\0')
        return false;

    // Store the search path
    int32 i = 0;
    while (pSearchPath[i] && i < MAX_PATH_LEN - 1)
    {
        m_SearchPath[i] = pSearchPath[i];
        ++i;
    }
    m_SearchPath[i] = '\0';

    // Extract directory and pattern from the search path
    char dirPath[MAX_PATH_LEN] = {0};
    char pattern[MAX_PATH_LEN] = {0};

    // Find the last '/' in the path
    const char* pLastSlash = nullptr;
    for (const char* p = pSearchPath; *p != '\0'; ++p)
    {
        if (*p == '/')
            pLastSlash = p;
    }

    if (pLastSlash)
    {
        // Split into directory and pattern
        size_t dirLen = static_cast<size_t>(pLastSlash - pSearchPath);
        std::memcpy(dirPath, pSearchPath, dirLen);
        dirPath[dirLen] = '\0';

        const char* pPattern = pLastSlash + 1;
        i = 0;
        while (pPattern[i] && i < MAX_PATH_LEN - 1)
        {
            pattern[i] = pPattern[i];
            ++i;
        }
        pattern[i] = '\0';
    }
    else
    {
        // No directory separator, use current directory
        dirPath[0] = '.';
        dirPath[1] = '\0';

        i = 0;
        while (pSearchPath[i] && i < MAX_PATH_LEN - 1)
        {
            pattern[i] = pSearchPath[i];
            ++i;
        }
        pattern[i] = '\0';
    }

    // Open the directory
    DIR* pDir = opendir(dirPath);
    if (!pDir)
        return false;

    m_pHandle = pDir;

    // Find the first matching file
    return FindNext();
}

bool FileFindClass::FindNext()
{
    if (!m_pHandle)
        return false;

    DIR* pDir = static_cast<DIR*>(m_pHandle);

    // Extract the pattern from the search path
    char pattern[MAX_PATH_LEN] = {0};

    // Find the last '/' in the search path
    const char* pLastSlash = nullptr;
    for (const char* p = m_SearchPath; *p != '\0'; ++p)
    {
        if (*p == '/')
            pLastSlash = p;
    }

    const char* pPattern = pLastSlash ? (pLastSlash + 1) : m_SearchPath;
    int32 i = 0;
    while (pPattern[i] && i < MAX_PATH_LEN - 1)
    {
        pattern[i] = pPattern[i];
        ++i;
    }
    pattern[i] = '\0';

    // Simple wildcard matching: if pattern is "*" or "*.*", match any file
    bool bMatchAll = (std::strcmp(pattern, "*") == 0 ||
                      std::strcmp(pattern, "*.*") == 0);

    struct dirent* pEntry = nullptr;
    while ((pEntry = readdir(pDir)) != nullptr)
    {
        // Skip "." and ".."
        if (std::strcmp(pEntry->d_name, ".") == 0 ||
            std::strcmp(pEntry->d_name, "..") == 0)
            continue;

        if (bMatchAll)
        {
            // Match any file/directory
            std::strncpy(m_FindName, pEntry->d_name, MAX_PATH_LEN - 1);
            m_FindName[MAX_PATH_LEN - 1] = '\0';
            m_IsDirectory = (pEntry->d_type == DT_DIR);
            m_IsHidden = (pEntry->d_name[0] == '.');
            IsFindValid = true;
            return true;
        }
        else
        {
            // Simple suffix matching: e.g., "*.ext" -> match files ending with ".ext"
            const char* pExt = std::strrchr(pattern, '.');
            if (pExt && pExt > pattern)
            {
                // Check if the pattern starts with "*"
                if (pattern[0] == '*')
                {
                    const char* pFileExt = std::strrchr(pEntry->d_name, '.');
                    if (pFileExt && std::strcmp(pFileExt, pExt) == 0)
                    {
                        std::strncpy(m_FindName, pEntry->d_name, MAX_PATH_LEN - 1);
                        m_FindName[MAX_PATH_LEN - 1] = '\0';
                        m_IsDirectory = (pEntry->d_type == DT_DIR);
                        m_IsHidden = (pEntry->d_name[0] == '.');
                        IsFindValid = true;
                        return true;
                    }
                }
            }
            else
            {
                // Exact match
                if (std::strcmp(pEntry->d_name, pattern) == 0)
                {
                    std::strncpy(m_FindName, pEntry->d_name, MAX_PATH_LEN - 1);
                    m_FindName[MAX_PATH_LEN - 1] = '\0';
                    m_IsDirectory = (pEntry->d_type == DT_DIR);
                    m_IsHidden = (pEntry->d_name[0] == '.');
                    IsFindValid = true;
                    return true;
                }
            }
        }
    }

    IsFindValid = false;
    return false;
}

void FileFindClass::Close()
{
    if (m_pHandle)
    {
        closedir(static_cast<DIR*>(m_pHandle));
        m_pHandle = nullptr;
    }
    IsFindValid = false;
    m_FindName[0] = '\0';
}

const char* FileFindClass::GetFileName() const
{
    return m_FindName;
}

bool FileFindClass::IsDirectory() const
{
    return m_IsDirectory;
}

bool FileFindClass::IsHidden() const
{
    return m_IsHidden;
}

// ============================================================================
// FileSystem static methods implementation
// ============================================================================

bool FileSystem::FileExists(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0')
        return false;

    struct stat st;
    int result = stat(pFilename, &st);
    return (result == 0 && S_ISREG(st.st_mode));
}

bool FileSystem::CreateDirectory(const char* pPath)
{
    if (!pPath || pPath[0] == '\0')
        return false;

    // Try to create directory with 0755 permissions
    int result = mkdir(pPath, 0755);
    if (result == 0)
        return true;

    // If it already exists, that's also a success
    if (errno == EEXIST)
        return true;

    return false;
}

bool FileSystem::DeleteFile(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0')
        return false;

    int result = remove(pFilename);
    return (result == 0);
}

int32 FileSystem::GetFileSize(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0')
        return 0;

    struct stat st;
    int result = stat(pFilename, &st);
    if (result != 0)
        return 0;

    if (!S_ISREG(st.st_mode))
        return 0;

    return static_cast<int32>(st.st_size);
}
// ============================================================================
 // CD - the forced-drive selector
// ============================================================================

namespace {
// The current forced drive, mirroring the binary's ForcedCDNumber global.
// 0xFFFFFFFE ("current drive") is the initial value.
int32 g_ForcedCDNumber = static_cast<int32>(0xFFFFFFFE);
} // namespace

bool CD::CD_Files_Local = false;

void CD::Set_Volume(int32 nVolume)
{
 // CD::Set_Volume: when the "CD files are local" flag is
    // set, the selector is pinned to the current-drive sentinel and the
    // caller's value is ignored.  Otherwise a non-negative value wins and a
    // negative one leaves the selector untouched.
    if (CD::CD_Files_Local) {
        g_ForcedCDNumber = static_cast<int32>(0xFFFFFFFE);
        return;
    }

    if (nVolume >= 0) {
        g_ForcedCDNumber = nVolume;
    }
}

int32 CD::Get_Volume()
{
    return g_ForcedCDNumber;
}

// ----------------------------------------------------------------------------
// 「当前驱动盘哨兵」判定。
//
//   原版的 CD::Is_Available (0x4790E0) 是把整个判定交给强制选盘流程
//   （即 CD::ForceAvailable），该实现在 Game/CD.cpp。这里保留的是本项目
//   早期基于「强制盘号」的等价判据，供本文件的卷扫描逻辑使用，因此改成
//   内部名字以免与 CD::Is_Available 冲突。
// ----------------------------------------------------------------------------
static bool CD_Is_Available_ByForcedNumber(int32 nIndex)
{
 // CD::Is_Available: the current-drive sentinel is always
    // available; the numbered drives are available when they match the
    // forced selection or when nothing has been forced.
    if (nIndex == static_cast<int32>(0xFFFFFFFE)) {
        return true;
    }

    if (g_ForcedCDNumber == static_cast<int32>(0xFFFFFFFE)) {
        return true;
    }

    return nIndex == g_ForcedCDNumber;
}

bool CD_ForceAvailable(int32 nIndex)
{
    if (CD::CD_Files_Local) {
        return true;
    }

    if (g_ForcedCDNumber == static_cast<int32>(0xFFFFFFFE)) {
        return true;
    }

    return nIndex == g_ForcedCDNumber || nIndex == 0;
}

// ============================================================================
// CD volume scanning
// ============================================================================

// RequiredCDNumber / OldRequiredCDNumber - the resolved-drive globals.
int32 g_RequiredCDNumber    = static_cast<int32>(0xFFFFFFFE);
int32 g_OldRequiredCDNumber = static_cast<int32>(0xFFFFFFFE);

namespace {

// Platform_GetTickCount - the host's millisecond counter.  The original calls
// the Win32 GetTickCount; the standalone build reads the POSIX monotonic clock
// so the CD-scan timeout logic stays faithful.
uint32 Platform_GetTickCount()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint32>(ts.tv_sec) * 1000u
         + static_cast<uint32>(ts.tv_nsec / 1000000);
}

// TryGetVolumeLabel - the host's GetVolumeInformationA.
//
//  The original asks Win32 for the volume label of "<letter>:\\" and inspects
//  GetLastError on failure.  There is no portable equivalent, so the standalone
//  build reports "not ready" for every letter; that makes Get_CD_Index walk the
//  whole alphabet and answer -1, which is exactly what the game does on a
//  machine with no disc inserted.
bool TryGetVolumeLabel(char /*letter*/, char* pBuffer, size_t /*cbBuffer*/, int32& errOut)
{
    if (pBuffer != nullptr && pBuffer[0] != '\0')
        pBuffer[0] = '\0';

    errOut = 0x15; // ERROR_NOT_READY
    return false;
}

} // namespace

 // asm: RawFileClass_FileSize
//
//  Formats "<letter>:\" for each candidate drive and asks the OS for the
//  volume label, then compares it - case-insensitively - against the game's
//  disc label "YR1".  A drive that reports ERROR_NOT_READY (0x15) is retried
//  until `timeout` ticks have elapsed, so a spindle that has not spun up yet
//  is not mistaken for an empty drive.  The first match wins and its drive
//  index (letter - 'A') is returned; -1 means no disc was found.
//
//  The standalone build reaches the volume label through the platform layer
//  (see TryGetVolumeLabel); on a host without optical drives no letter ever
//  matches and the sentinel "current drive" answer stands.
int32 Get_CD_Index(char startLetter, int32 timeout)
{
    const uint32 begin = Platform_GetTickCount();

    for (char letter = startLetter; letter <= 'Z'; ++letter)
    {
        uint32 remaining = static_cast<uint32>(timeout);
        char volumeName[MAX_PATH_LEN] = {};
        bool ready = false;

        for (;;)
        {
            int32 err = 0;
            if (TryGetVolumeLabel(letter, volumeName, sizeof(volumeName), err))
            {
                ready = true;
                break;
            }

            // Only "device not ready" is worth waiting on; anything else means
            // there is nothing here.
            if (err != 0x15 /* ERROR_NOT_READY */)
                break;

            if (remaining == 0)
                break;

            const uint32 elapsed = Platform_GetTickCount() - begin;
            if (elapsed < remaining)
                continue;

            remaining = 0;
        }

        if (ready && _strcmpi(volumeName, "YR1") == 0)
            return letter - 'A';
    }

    return -1;
}

// ============================================================================
// asm: RawFileClass_FileSize
// ============================================================================

// ============================================================================
// RawFileClass - the raw Westwood file slots
//
//   The retail class operates on a Win32 HANDLE at [this+14h] together with a
//   sub-file window ([this+0Ch] = base, [this+10h] = length, -1 = unknown).
//   Every size/seek call is expressed relative to that window, which is what
//   lets a MIX member be served through an already-open archive handle.
// ============================================================================

// asm: RawFileClass_FileSize (0x65D0C0)
//
//   If the window length is already resolved ([this+10h] != -1) it is returned
//   verbatim.  Otherwise the physical extent is queried (handle first, disk
//   name second), biased by the window base and cached into [this+10h].
int32 RawFileClass::FileSize()
{
    if (SubFileLength != -1) {
        return SubFileLength;
    }

    int32 physicalSize = 0;

    if (Handle)
    {
        std::FILE* pFile = static_cast<std::FILE*>(Handle);
        const long curPos = std::ftell(pFile);
        if (curPos >= 0)
        {
            std::fseek(pFile, 0, SEEK_END);
            physicalSize = static_cast<int32>(std::ftell(pFile));
            std::fseek(pFile, curPos, SEEK_SET);
        }
    }
    else if (FileName[0] != '\0')
    {
        struct stat st;
        if (::stat(FileName, &st) == 0) {
            physicalSize = static_cast<int32>(st.st_size);
        }
    }

    // asm: eax = physicalSize - [this+0Ch]; store into [this+10h].
    SubFileLength = physicalSize - SubFileStart;
    if (SubFileLength < 0) {
        SubFileLength = 0;
    }
    return SubFileLength;
}

// asm: RawFileClass_GetFileTime (0x65D270)
//
//   GetFileInformationByHandle -> FileTimeToDosDateTime, then the packed FAT
//   stamp is returned as (wFatDate << 16) | wFatTime.  On this platform the
//   same value is synthesised from the file's mtime.
int32 RawFileClass::GetFileTime()
{
    if (FileName[0] == '\0') {
        return 0;
    }

    struct stat st;
    if (::stat(FileName, &st) != 0) {
        return 0;
    }

    struct tm tmv;
    if (localtime_r(&st.st_mtime, &tmv) == nullptr) {
        return 0;
    }

    // FAT date: yyyyyyymmmmddddd (year biased by 1980); FAT time: hhhhhmmmmmmsssss
    // with seconds stored in 2-second units - the same layout FileTimeToDosDateTime
    // produces in the original.
    const int32 fatDate = (((tmv.tm_year + 1900) - 1980) << 9)
                        | ((tmv.tm_mon + 1) << 5)
                        | tmv.tm_mday;
    const int32 fatTime = (tmv.tm_hour << 11)
                        | (tmv.tm_min << 5)
                        | (tmv.tm_sec >> 1);

    return (static_cast<int32>(static_cast<uint16>(fatDate)) << 16)
         | static_cast<int32>(static_cast<uint16>(fatTime));
}

// asm: int __stdcall RawFileClass_SetFileTime(WORD wFatTime)   (0x65D3A0)
//
//   Splits the packed stamp back into FAT date/time, converts to a FILETIME and
//   stamps the handle.  Here the DOS value is decoded and written through utime.
bool RawFileClass::SetFileTime(int32 nTime)
{
    if (nTime == 0 || FileName[0] == '\0') {
        return false;
    }

    const uint16 fatTime = static_cast<uint16>(nTime & 0xFFFF);
    const uint16 fatDate = static_cast<uint16>((nTime >> 16) & 0xFFFF);

    struct tm tmv;
    std::memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = ((fatDate >> 9) & 0x7F) + 1980 - 1900;
    tmv.tm_mon  = ((fatDate >> 5) & 0x0F) - 1;
    tmv.tm_mday = (fatDate & 0x1F);
    tmv.tm_hour = ((fatTime >> 11) & 0x1F);
    tmv.tm_min  = ((fatTime >> 5) & 0x3F);
    tmv.tm_sec  = (fatTime & 0x1F) * 2;

    const time_t when = mktime(&tmv);
    if (when == static_cast<time_t>(-1)) {
        return false;
    }

    struct utimbuf stamp;
    stamp.actime  = when;
    stamp.modtime = when;
    return ::utime(FileName, &stamp) == 0;
}

// asm: RawFileClass_HasHandle (0x65D420)
bool RawFileClass::HasHandle() const
{
    return (Handle != nullptr) && (Handle != reinterpret_cast<void*>(-1));
}

// asm: RawFileClass_MoveFilePointer (0x65CEA0)
//
//   Windowed seek.  When the window length is unknown (-1) the raw handle is
//   seeked and the OS position returned.  Otherwise the logical target is
//   computed per method, clamped to [0, length] and the returned value is the
//   logical position inside the window.
int32 RawFileClass::MoveFilePointer(int32 nOffset, FileSeekMode mode)
{
    if (!Handle) {
        return 0;
    }

    std::FILE* pFile = static_cast<std::FILE*>(Handle);
    const int32 len = SubFileLength;

    if (len == -1)
    {
        // asm: loc_65D057 - no window, plain SetFilePointer.
        int origin = SEEK_SET;
        if (mode == FileSeekMode::Current) origin = SEEK_CUR;
        else if (mode == FileSeekMode::End) origin = SEEK_END;
        std::fseek(pFile, nOffset, origin);
        const long pos = std::ftell(pFile);
        return (pos < 0) ? 0 : static_cast<int32>(pos);
    }

    // asm: compute the raw target position.
    int32 logical;
    switch (mode)
    {
    case FileSeekMode::Set:
        logical = nOffset;
        break;
    case FileSeekMode::End:
        logical = len + nOffset;
        break;
    case FileSeekMode::Current:
    default:
    {
        const long raw = std::ftell(pFile);
        logical = static_cast<int32>(raw - SubFileStart) + nOffset;
        break;
    }
    }

    // asm: clamp into [0, len]; an under-run resets to the window base and an
    // over-run pins to the window end.
    if (logical < 0) {
        logical = 0;
    }
    if (logical > len) {
        logical = len;
    }

    std::fseek(pFile, SubFileStart + logical, SEEK_SET);
    return logical;
}

// asm: RawFileClass_SetFilePointer (0x65D460)   (distanceToMove, moveMode)
//
//   Re-binds the sub-file window.  A zero distance resets it to the whole file
//   ([this+0Ch] = 0, [this+10h] = -1).  A non-zero distance shifts the base and,
//   when moveMode is not -1, caps the window length at moveMode.
int32 RawFileClass::SetFilePointer(int32 nDistanceToMove)
{
    if (nDistanceToMove == 0)
    {
        SubFileStart  = 0;
        SubFileLength = -1;
        return 0;
    }

    const int32 size = FileSize();
    SubFileLength = size;
    SubFileStart += nDistanceToMove;
    if (SubFileStart < 0) {
        SubFileStart = 0;
    }
    return SubFileStart;
}

// asm: byte __thiscall RawFileClass_OpenFile(RawFileClass *this, int a2)
//
//   Clears the window, maps the mode to the OS open flags and opens.  A null
//   name routes through the error reporter in the original; the port simply
//   fails.  The result reports whether a valid handle was obtained.
bool RawFileClass::OpenFile(FileAccessMode mode)
{
    return Open(mode);
}

// asm: int __stdcall RawFileClass_ReadNextBytes(LPVOID lpBuffer, int)  (0x65CD10)
//
//   Reads from the current position, clamped to the window end when the window
//   length is known.  The original loops on ReadFile to fill short reads; the
//   port loops on fread for the same effect and returns the running total.
int32 RawFileClass::ReadNextBytes(void* pBuffer, int32 nSize)
{
    if (!Handle || pBuffer == nullptr || nSize <= 0) {
        return 0;
    }

    int32 toRead = nSize;

    // asm: when the window length is known, never read past base + length.
    if (SubFileLength != -1)
    {
        std::FILE* pFile = static_cast<std::FILE*>(Handle);
        const long pos = std::ftell(pFile);
        const int32 remaining = (SubFileStart + SubFileLength) - static_cast<int32>(pos);
        if (remaining <= 0) {
            return 0;
        }
        if (toRead > remaining) {
            toRead = remaining;
        }
    }

    std::FILE* pFile = static_cast<std::FILE*>(Handle);
    uint8* pDest = static_cast<uint8*>(pBuffer);
    int32 total = 0;
    while (total < toRead)
    {
        const size_t got = std::fread(pDest + total, 1,
                                      static_cast<size_t>(toRead - total), pFile);
        if (got == 0) {
            break;
        }
        total += static_cast<int32>(got);
    }
    return total;
}

// asm: RawFileClass_SetFileName (0x65CAB0)
//
//   Releases the old name, resets the window and the sub-file base, then stores
//   a fresh copy.  A null name clears the name and reports failure; a fresh
//   object that is already open keeps its handle.
bool RawFileClass::SetFileName(const char* pName)
{
    if (pName == nullptr) {
        FileName[0] = '\0';
        return false;
    }

    // asm: [this+0Ch] = 0, [this+10h] = -1 before the new name is stored.
    SubFileStart  = 0;
    SubFileLength = -1;

    SetName(pName);
    return FileName[0] != '\0';
}

// asm: byte __thiscall RawFileClass_FileExists(RawFileClass *this, int a2)  (0x65CBF0)
//
//   A null name fails.  When already usable the answer is true immediately;
//   otherwise (and when a2 requests it) the file is opened, probed and closed
//   again, leaving the handle parked at -1.
bool RawFileClass::FileExists(int32 nArg)
{
    if (FileName[0] == '\0') {
        return false;
    }

    if (HasHandle()) {
        return true;
    }

    struct stat st;
    const bool onDisk = (::stat(FileName, &st) == 0);

    if (nArg != 0)
    {
        // asm: open, re-check, then close so no handle is retained.
        if (Open(FileAccessMode::Read)) {
            const bool ok = HasHandle();
            Close();
            return ok;
        }
        return false;
    }

    return onDisk;
}

// ============================================================================
// RAMFileClass - memory-backed file
// ============================================================================

// asm: sub_65C250 (0x65C250) - default form.  The retail CTOR always takes the
// (data, size) pair; the port offers a no-argument form for containers.
RAMFileClass::RAMFileClass()
    : Buffer(nullptr)
    , Capacity(0)
    , Size(0)
    , Position(0)
    , OpenMode(1)
    , OwnsBuffer(false)
    , Open(false)
{
}

// asm: sub_65C250 (0x65C250) - wrap <pData> (borrowed) as a Size-byte file.
// When pData is null and Size > 0 an owned buffer is allocated and flagged at
// [this+1Dh] so the destructor frees it.
RAMFileClass::RAMFileClass(void* pData, int32 nSize)
    : Buffer(static_cast<uint8*>(pData))
    , Capacity(nSize)
    , Size(nSize)
    , Position(0)
    , OpenMode(1)
    , OwnsBuffer(false)
    , Open(false)
{
    if (pData == nullptr && nSize > 0)
    {
        Buffer = static_cast<uint8*>(YRMemory::Allocate(static_cast<size_t>(nSize)));
        Capacity = nSize;
        Size = nSize;
        OwnsBuffer = true;
    }
}

RAMFileClass::RAMFileClass(int32 nSize)
    : RAMFileClass(nullptr, nSize)
{
}

// asm: RAMFileClass_DTOR / _SDDTOR (0x65C2B0 / 0x65C5A0)
RAMFileClass::~RAMFileClass()
{
    Close();
}

// asm: sub_65C540 (0x65C540) - clears the open flag; the destructor path also
// frees an owned buffer and nulls [this+8].
void RAMFileClass::Close()
{
    if (OwnsBuffer && Buffer != nullptr)
    {
        YRMemory::Deallocate(Buffer);
    }
    Buffer = nullptr;
    OwnsBuffer = false;
    Open = false;
}

// asm: RAMFileClass_FileExists (0x65C2E8) - a RAM file always "exists".
bool RAMFileClass::FileExists(int32 /*nArg*/) const
{
    return true;
}

// asm: RAMFileClass_HasHandle (0x65C2F8)
bool RAMFileClass::HasHandle() const
{
    return Open;
}

// asm: RAMFileClass_OpenFile (0x65C38F)
//
//   Fails when there is no backing buffer or the file is already open.
//   Otherwise the cursor resets to 0, the mode is stored and, for mode 2, the
//   logical size is zeroed.  The result is HasHandle().
bool RAMFileClass::OpenFile(int32 nMode)
{
    if (Buffer == nullptr) {
        return false;
    }
    if (HasHandle()) {
        return false;
    }

    Position = 0;
    OpenMode = nMode;
    Open = true;

    if (nMode == 2) {
        Size = 0;
    }

    return HasHandle();
}

// asm: RAMFileClass_ReadNextBytes (0x65C3A0)
//
//   Reads from the buffer at the cursor, clamped to the logical size.  When the
//   file is not yet open it is opened for reading and closed again afterwards.
int32 RAMFileClass::ReadNextBytes(void* pBuffer, int32 nSize)
{
    if (Buffer == nullptr || pBuffer == nullptr || nSize <= 0) {
        return 0;
    }

    bool openedHere = false;
    if (!HasHandle())
    {
        if (!OpenFile(1)) {
            return 0;
        }
        openedHere = true;
    }
    else if ((OpenMode & 1) == 0)
    {
        // asm: not a read-mode handle -> refuse.
        return 0;
    }

    int32 available = Size - Position;
    int32 toCopy = (nSize < available) ? nSize : available;
    if (toCopy <= 0) {
        if (openedHere) {
            Close();
        }
        return 0;
    }

    std::memcpy(pBuffer, Buffer + Position, static_cast<size_t>(toCopy));
    Position += toCopy;

    if (openedHere) {
        // asm: sub_65C540 - the auto-open is unwound before returning.
        Close();
    }

    return toCopy;
}

// asm: RAMFileClass_MoveFilePointer (0x65C420)
//
//   Method 0 = Set, 1 = Current, 2 = End.  The clamp bound is the capacity for
//   write-mode handles and the logical size otherwise.  A position past the
//   logical size grows it.
int32 RAMFileClass::MoveFilePointer(int32 nDistance, int32 nMethod)
{
    if (Buffer == nullptr || !HasHandle()) {
        return Position;
    }

    const int32 bound = ((OpenMode & 2) != 0) ? Capacity : Size;

    switch (nMethod)
    {
    case 0:
        Position = nDistance;
        break;
    case 2:
        Position = bound + nDistance;
        break;
    case 1:
    default:
        Position += nDistance;
        break;
    }

    if (Position < 0) {
        Position = 0;
    }
    if (Position > bound) {
        Position = bound;
    }

    // asm: if the new position runs past the logical size, extend it.
    if (Position > Size) {
        Size = Position;
    }

    return Position;
}

// asm: RAMFileClass_FileSize (0x65C4D0)
int32 RAMFileClass::FileSize()
{
    return Size;
}

// asm: RAMFileClass_GetFileName (0x65C570) - always the literal "UNKNOWN"
const char* RAMFileClass::GetFileName() const
{
    return "UNKNOWN";
}

// asm: RAMFileClass_SetFileName (0x65C580) - a no-op that returns truthy.
bool RAMFileClass::SetFileName(const char* /*pName*/)
{
    return true;
}

// asm: RAMFileClass_GetFileTime (0x65C590)
int32 RAMFileClass::GetFileTime()
{
    return 0;
}

// asm: RAMFileClass_SetFileTime (0x65C598)
bool RAMFileClass::SetFileTime(int32 /*nTime*/)
{
    return true;
}

// Re-point the RAM file at a different buffer (utility; refuses while open).
bool RAMFileClass::SetBuffer(void* pData, int32 nSize)
{
    if (HasHandle()) {
        return false;
    }
    if (OwnsBuffer && Buffer != nullptr) {
        YRMemory::Deallocate(Buffer);
    }
    Buffer = static_cast<uint8*>(pData);
    Capacity = nSize;
    Size = nSize;
    Position = 0;
    OwnsBuffer = false;
    return true;
}

// asm: sub_65C300 (0x65C300) - clear the logical size.  Refuses (returns false)
// while the file is open; otherwise zeroes [this+10h] and reports success.
bool RAMFileClass::Reset()
{
    if (HasHandle()) {
        return false;
    }
    Size = 0;
    Position = 0;
    return true;
}

// asm: CCFileClass_AllocateMemory
void* CCFileClass::AllocateMemory(int32 nSize)
{
    if (nSize <= 0) {
        return nullptr;
    }
    return YRMemory::Allocate(static_cast<size_t>(nSize));
}

// asm: CCFileClass_CDCheck
// 登记了光盘源时按光盘可用性判定。
bool CCFileClass::CDCheck()
{
    return CD_Is_Available_ByForcedNumber(0);
}

// asm: CCFileClass_OpenEx
bool CCFileClass::OpenEx(const char* pFilename, int32 nMode)
{
    if (pFilename != nullptr && pFilename[0] != '\0') {
        SetName(pFilename);
    }
    return Open(nMode);
}

// ============================================================================
// CCFileClass - the raw Westwood interface (vt_CCFileClass slots)
//
//  The original keeps a single optional residency buffer at [this+58h]: when a
//  file is served from a MIX archive the whole entry is copied into it and all
//  read / seek / size traffic happens in memory; when it is a plain disk file
//  the buffer stays null and the calls fall through to the buffered file layer
//  (BufferIOFileClass / RawFileClass).  The glue below mirrors that split.
// ============================================================================

// asm: BufferIOFileClass_FileExists -> RawFileClass_FileExists / hit cache
static bool CCFile_OnDiskExists(const char* pName)
{
    if (pName == nullptr || pName[0] == '\0') {
        return false;
    }
    struct stat st;
    return ::stat(pName, &st) == 0 && S_ISREG(st.st_mode);
}

// asm: CCFileClass::Is_Available (0x473C40)  - vtable slot 5
//
//  [this+68h] caches the verdict: 1 == usable, 2 == not.  A file backed by the
//  residency buffer is usable immediately; otherwise the MIX registry is asked
//  first (MixFileClass_FindFileByName) and the disk second.
bool CCFileClass::Is_Available(int32 /*nMode*/)
{
    if (AvailabilityCache == 1) {
        return true;
    }

    if (HasHandle()) {
        AvailabilityCache = 1;
        return true;
    }

    if (MixFileClass::FindFileByName(FileName) != nullptr) {
        AvailabilityCache = 1;
        return true;
    }

    if (CCFile_OnDiskExists(FileName)) {
        AvailabilityCache = 1;
        return true;
    }

    AvailabilityCache = 2;
    return false;
}

// asm: CCFileClass_HasHandle (0x473CD0) - vtable slot 6
//
//  A residency buffer (MIX-backed) counts as a handle; otherwise defer to the
//  underlying raw file's handle.
bool CCFileClass::HasHandle()
{
    if (MixBuffer != nullptr) {
        return true;
    }
    return m_RawFile.HasHandle();
}

// asm: CCFileClass_OpenFile (0x473CF0) - vtable slot 7
//
//  The `mode & 2` test is the "write" request: writing never comes from a MIX,
//  so it goes straight to the CD/disk open path.  A read request first probes
//  the MIX registry; a hit copies the entry into the residency buffer and the
//  call succeeds without touching the disk.  A miss falls back to the disk.
bool CCFileClass::OpenFile(int32 nMode)
{
    Close();

    // asm: test [mode], 2 -> jnz lct_InGameDir (write path skips MIX lookup)
    if ((nMode & 2) != 0) {
        return Open(nMode);
    }

    if (!CCFile_OnDiskExists(FileName)) {
        // asm: MixFileClass_FindFileByName over the whole registry.
        MixFileClass* pMix = MixFileClass::FindFileByName(FileName);
        if (pMix != nullptr) {
            const MixIndexEntry* pEntry = pMix->GetIndexEntry(FileName);
            if (pEntry != nullptr && pEntry->Size > 0) {
                int32 nSize = static_cast<int32>(pEntry->Size);
                void* pData = pMix->GetFile(FileName, &nSize);
                if (pData != nullptr) {
                    MixBuffer = pData;
                    MixBufferSize = nSize;
                    MixBufferPos = 0;
                    AvailabilityCache = 1;
                    return true;
                }
            }
        }
        // asm: lct_NotFoundInMixes -> CDFileClass_OpenFile
    }

    return Open(nMode);
}

// asm: CCFileClass_ReadNextBytes (0x473CE0) - vtable slot 9
//
//  For a MIX-resident file the request is served out of the buffer, clamped to
//  what remains; otherwise it is forwarded to the buffered file layer.
int32 CCFileClass::ReadNextBytes(void* pBuffer, int32 nSize)
{
    if (MixBuffer != nullptr) {
        int32 nRemaining = MixBufferSize - MixBufferPos;
        int32 nToRead = (nSize < nRemaining) ? nSize : nRemaining;
        if (nToRead <= 0) {
            return 0;
        }
        if (pBuffer != nullptr) {
            std::memcpy(pBuffer,
                        static_cast<uint8*>(MixBuffer) + MixBufferPos,
                        static_cast<size_t>(nToRead));
        }
        MixBufferPos += nToRead;
        return nToRead;
    }

    if (!IsFileOpen) {
        return 0;
    }
    return m_RawFile.ReadNextBytes(pBuffer, nSize);
}

// asm: CCFileClass_MoveFilePointer (0x473FA0) - vtable slot 10
//
//  The buffer-backed path clamps the new position to [0, size]: a negative
//  result is pinned to 0 and an over-run is pinned to the end.  The disk path
//  defers to the raw file pointer.
int32 CCFileClass::MoveFilePointer(int32 nDistance, int32 nMethod)
{
    if (MixBuffer != nullptr) {
        // asm: method 0 == Set (pos = 0), 2 == End (pos = size), 1 == Current.
        switch (nMethod) {
        case 0:
            MixBufferPos = 0;
            break;
        case 2:
            MixBufferPos = MixBufferSize;
            break;
        default:
            break;
        }

        MixBufferPos += nDistance;
        if (MixBufferPos < 0) {
            MixBufferPos = 0;
        }
        if (MixBufferPos > MixBufferSize) {
            MixBufferPos = MixBufferSize;
        }
        return MixBufferPos;
    }

    if (!IsFileOpen) {
        return 0;
    }
    return m_RawFile.MoveFilePointer(nDistance, static_cast<FileSeekMode>(nMethod));
}

// asm: CCFileClass_FileSize (0x473C00) - vtable slot 11
//
//  A MIX-resident file reports the buffer length; otherwise the disk size, and
//  failing that the MIX index entry (a not-yet-opened archive member).
int32 CCFileClass::FileSize()
{
    if (MixBuffer != nullptr) {
        return MixBufferSize;
    }

    if (CCFile_OnDiskExists(FileName)) {
        return m_RawFile.FileSize();
    }

    MixFileClass* pMix = MixFileClass::FindFileByName(FileName);
    if (pMix != nullptr) {
        const MixIndexEntry* pEntry = pMix->GetIndexEntry(FileName);
        if (pEntry != nullptr) {
            return static_cast<int32>(pEntry->Size);
        }
    }
    return 0;
}

// asm: CCFileClass_WriteBytes (0x473D40) - vtable slot 12
//
//  Writing is always a disk operation: a MIX-resident buffer is dropped first,
//  then the bytes go to the underlying file.
int32 CCFileClass::WriteBytes(const void* pBuffer, int32 nSize)
{
    if (MixBuffer != nullptr) {
        YRMemory::Deallocate(MixBuffer);
        MixBuffer = nullptr;
        MixBufferSize = 0;
        MixBufferPos = 0;
    }

    if (!IsFileOpen) {
        return 0;
    }
    return m_RawFile.Write(pBuffer, nSize);
}

// asm: CCFileClass_GetFileTime (0x473E60) - vtable slot 14
//
//  The on-disk timestamp wins; a MIX-resident file falls back to the archive
//  member's own timestamp via a temporary probe, which on this platform is a
//  plain re-stat of the archive file.  With neither available the answer is 0.
int32 CCFileClass::GetFileTime()
{
    int32 nTime = m_RawFile.GetFileTime();
    if (nTime != 0) {
        return nTime;
    }

    if (MixFileClass::FindFileByName(FileName) != nullptr) {
        // The original builds a temporary CDFileClass over the MIX member and
        // asks it for the time; there is no per-member timestamp in the MIX
        // index, so the archive's own time is not attributable here.
        return 0;
    }
    return 0;
}

// asm: CCFileClass_SetFileTime (0x473F00) - vtable slot 15
bool CCFileClass::SetFileTime(int32 nTime)
{
    if (m_RawFile.SetFileTime(nTime)) {
        return true;
    }

    if (MixFileClass::FindFileByName(FileName) != nullptr) {
        // MIX members are read-only; there is nothing to stamp.
        return false;
    }
    return false;
}

// asm: CCFileClass_SetFileName (0x474010) - vtable slot 2
//
//  Setting the name invalidates the availability cache ([this+68h] = 0) before
//  delegating to the raw file layer.
bool CCFileClass::SetFileName(const char* pName)
{
    AvailabilityCache = 0;

    if (MixBuffer != nullptr) {
        YRMemory::Deallocate(MixBuffer);
        MixBuffer = nullptr;
        MixBufferSize = 0;
        MixBufferPos = 0;
    }

    SetName(pName);
    return m_RawFile.SetFileName(pName);
}

// asm: CCFileClass_Create (0x65D15C) - vtable slot 3
//
//  Close, open for write (mode 2), truncate to zero and reopen.  Returns true
//  when the file could be opened for writing.
bool CCFileClass::Create()
{
    Close();

    if (!Open(2)) {
        return false;
    }

    if (m_RawFile.FileSize() != -1) {
        MoveFilePointer(0, 0);
    }

    m_RawFile.MoveFilePointer(0, FileSeekMode::End);
    return true;
}

// asm: CCFileClass_Delete (0x65D1A0) - vtable slot 4
//
//  Close, make sure the handle exists, then remove the file from disk.  A
//  MIX-resident member cannot be deleted.
bool CCFileClass::Delete()
{
    Close();

    if (FileName[0] == '\0') {
        return false;
    }

    if (MixFileClass::FindFileByName(FileName) != nullptr) {
        return false;
    }

    if (!CCFile_OnDiskExists(FileName)) {
        return false;
    }

    return FileSystem::DeleteFile(FileName);
}
