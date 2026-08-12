#include "IO/FileSystem.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <cerrno>

// ============================================================================
// RawFileClass implementation
// ============================================================================

RawFileClass::RawFileClass()
    : Handle(nullptr)
{
    FileName[0] = '\0';
}

RawFileClass::RawFileClass(const char* pFilename)
    : Handle(nullptr)
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

int32 RawFileClass::GetSize()
{
    if (!Handle)
        return 0;

    std::FILE* pFile = static_cast<std::FILE*>(Handle);
    long curPos = std::ftell(pFile);
    std::fseek(pFile, 0, SEEK_END);
    long size = std::ftell(pFile);
    std::fseek(pFile, curPos, SEEK_SET);
    return static_cast<int32>(size);
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
{
    FileName[0] = '\0';
}

CCFileClass::CCFileClass(const char* pFilename)
    : IsFileOpen(false)
    , FileHandle(nullptr)
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
    if (IsFileOpen)
    {
        m_RawFile.Close();
        IsFileOpen = false;
        FileHandle = nullptr;
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