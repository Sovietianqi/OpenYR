// ============================================================================
// CCFileClass.cpp
//
//  CCFileClass itself is implemented in IO/FileSystem.cpp - the high-level
//  open / read / write / seek entry points live there.  This file is kept
//  for build-system compatibility and houses the static factory methods,
//  file-search utilities, path-resolution helpers, and MIX-archive
//  integration hooks that the original binary attaches to the CCFileClass
//  translation unit.
// ============================================================================

#include "IO/CCFileClass.h"
#include "IO/FileSystem.h"
#include "IO/MixFileClass.h"
#include "Core/Definitions.h"
#include "Core/Macros.h"

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// ============================================================================
// CCFileClassFactory - static factory helpers
//
//  The original binary exposes a handful of free helpers next to the
//  CCFileClass implementation.  They construct a CCFileClass, open it in
//  the requested mode, and return the pointer.  Callers own the result.
// ============================================================================

// ----------------------------------------------------------------------------
// Open_For_Read - construct + open a CCFileClass for reading.
// ----------------------------------------------------------------------------
CCFileClass* CCFile_Open_For_Read(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0')
        return nullptr;

    CCFileClass* pFile = new CCFileClass(pFilename);
    if (!pFile)
        return nullptr;

    if (!pFile->Open(0)) // 0 = Read
    {
        delete pFile;
        return nullptr;
    }
    return pFile;
}

// ----------------------------------------------------------------------------
// Open_For_Write - construct + open a CCFileClass for writing.  The file is
// truncated if it already exists.
// ----------------------------------------------------------------------------
CCFileClass* CCFile_Open_For_Write(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0')
        return nullptr;

    CCFileClass* pFile = new CCFileClass(pFilename);
    if (!pFile)
        return nullptr;

    if (!pFile->Open(1)) // 1 = Write
    {
        delete pFile;
        return nullptr;
    }
    return pFile;
}

// ----------------------------------------------------------------------------
// Open_For_ReadWrite - construct + open a CCFileClass for read/write access.
// ----------------------------------------------------------------------------
CCFileClass* CCFile_Open_For_ReadWrite(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0')
        return nullptr;

    CCFileClass* pFile = new CCFileClass(pFilename);
    if (!pFile)
        return nullptr;

    if (!pFile->Open(2)) // 2 = ReadWrite
    {
        delete pFile;
        return nullptr;
    }
    return pFile;
}

// ============================================================================
// CCFile_Find - file search utilities
//
//  Wraps FileFindClass for the common "does this file exist anywhere on the
//  search path" query used by the asset loader.
// ============================================================================

// ----------------------------------------------------------------------------
// CCFile_Find_First - begin a directory enumeration.
// ----------------------------------------------------------------------------
bool CCFile_Find_First(const char* pSearchPath, FileFindClass& find)
{
    if (!pSearchPath || pSearchPath[0] == '\0')
        return false;
    return find.FindFirst(pSearchPath);
}

// ----------------------------------------------------------------------------
// CCFile_Find_Next - advance an existing enumeration.
// ----------------------------------------------------------------------------
bool CCFile_Find_Next(FileFindClass& find)
{
    return find.FindNext();
}

// ----------------------------------------------------------------------------
// CCFile_Find_Exists - convenience helper: returns true if the supplied path
// resolves to a regular file.  Mirrors the original binary's helper of the
// same name.
// ----------------------------------------------------------------------------
bool CCFile_Find_Exists(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0')
        return false;

    struct stat st;
    int result = stat(pFilename, &st);
    return (result == 0 && S_ISREG(st.st_mode));
}

// ============================================================================
// CCFile_Resolve_Path - path resolution helpers
//
//  The original game searches a handful of well-known directories (the
//  install dir, the CD-ROM root, the user profile) when an asset is
//  requested.  These helpers implement the resolution chain in a
//  platform-agnostic way.
// ============================================================================

// ----------------------------------------------------------------------------
// CCFile_Resolve_Path - try each search root in order.  Returns true if the
// file was found; the resolved absolute path is written to pOutBuffer.
// ----------------------------------------------------------------------------
bool CCFile_Resolve_Path(const char* pRelativeName,
                         const char* const* pSearchRoots,
                         int32 nSearchRoots,
                         char* pOutBuffer,
                         int32 nBufferSize)
{
    if (!pRelativeName || !pOutBuffer || nBufferSize <= 0)
        return false;

    if (pSearchRoots == nullptr || nSearchRoots <= 0)
    {
        // No search roots - just check the relative path directly.
        if (CCFile_Find_Exists(pRelativeName))
        {
            int32 i = 0;
            while (pRelativeName[i] && i < nBufferSize - 1)
            {
                pOutBuffer[i] = pRelativeName[i];
                ++i;
            }
            pOutBuffer[i] = '\0';
            return true;
        }
        return false;
    }

    for (int32 root = 0; root < nSearchRoots; ++root)
    {
        const char* pRoot = pSearchRoots[root];
        if (!pRoot)
            continue;

        // Compose: root + "/" + relative
        int32 pos = 0;
        int32 i = 0;
        while (pRoot[i] && pos < nBufferSize - 1)
        {
            pOutBuffer[pos] = pRoot[i];
            ++pos; ++i;
        }
        if (pos > 0 && pOutBuffer[pos - 1] != '/' && pos < nBufferSize - 1)
        {
            pOutBuffer[pos] = '/';
            ++pos;
        }
        i = 0;
        while (pRelativeName[i] && pos < nBufferSize - 1)
        {
            pOutBuffer[pos] = pRelativeName[i];
            ++pos; ++i;
        }
        pOutBuffer[pos] = '\0';

        if (CCFile_Find_Exists(pOutBuffer))
            return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// CCFile_Normalize_Path - collapses "./" and "//" sequences.  The original
// binary uses this when ingesting user-supplied paths from INI files.
// ----------------------------------------------------------------------------
void CCFile_Normalize_Path(const char* pIn, char* pOut, int32 nBufferSize)
{
    if (!pIn || !pOut || nBufferSize <= 0)
        return;

    int32 outPos = 0;
    int32 i = 0;
    while (pIn[i] && outPos < nBufferSize - 1)
    {
        // Collapse consecutive slashes.
        if (pIn[i] == '/' && outPos > 0 && pOut[outPos - 1] == '/')
        {
            ++i;
            continue;
        }
        // Collapse "./" sequences.
        if (pIn[i] == '.' && pIn[i + 1] == '/')
        {
            i += 2;
            continue;
        }
        pOut[outPos] = pIn[i];
        ++outPos; ++i;
    }
    pOut[outPos] = '\0';
}

// ============================================================================
// MIX file integration
//
//  The full engine queries the loaded MIX archives for an asset before
//  falling back to the filesystem.  These helpers use the MixFileClass
//  registry to perform lookups, retrieve file data, and enumerate the
//  contents of every loaded archive.
// ============================================================================

// ----------------------------------------------------------------------------
// CCFile_Try_Open_From_MIX - look up the file in every loaded MIX archive.
// Returns true and fills pOutFile if found; false otherwise.
// ----------------------------------------------------------------------------
bool CCFile_Try_Open_From_MIX(const char* pFilename,
                              CCFileClass* /*pOutFile*/)
{
    if (!pFilename || pFilename[0] == '\0')
        return false;

    // Query the global MIX registry.  Retrieve returns a heap pointer to
    // the file data (or nullptr if not found); we only need the existence
    // check here, so the returned pointer is freed immediately.
    void* pData = MixFileClass::Retrieve(pFilename);
    if (!pData)
        return false;

    // The caller is responsible for copying the data into pOutFile; for the
    // existence check we simply free the buffer.
    YRMemory::Deallocate(pData);
    return true;
}

// ----------------------------------------------------------------------------
// CCFile_Is_In_MIX - check whether the file exists in any loaded MIX.
// ----------------------------------------------------------------------------
bool CCFile_Is_In_MIX(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0')
        return false;

    // Compute the CRC of the file name and search every loaded MIX.
    uint32 crc = MixFileClass::ComputeCRC(pFilename);

    DynamicVectorClass<MixFileClass*>& mixArray = MixFileClass::GetMixArray();
    for (int32 i = 0; i < mixArray.Count; ++i)
    {
        MixFileClass* pMix = mixArray.Items[i];
        if (!pMix)
            continue;
        if (pMix->FileExists(crc))
            return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// CCFile_Enumerate_MIX_Files - invoke the callback for every file inside
// every loaded MIX archive.  Used by the asset-index build step.
// ----------------------------------------------------------------------------
void CCFile_Enumerate_MIX_Files(void (*pCallback)(const char* pName,
                                                  int32 nSize,
                                                  void* pUser),
                                void* pUser)
{
    if (!pCallback)
        return;

    DynamicVectorClass<MixFileClass*>& mixArray = MixFileClass::GetMixArray();
    for (int32 i = 0; i < mixArray.Count; ++i)
    {
        MixFileClass* pMix = mixArray.Items[i];
        if (!pMix || !pMix->Headers)
            continue;

        // The MIX index stores CRC hashes, not file names.  We pass the
        // numeric hash formatted as a hex string so the callback receives
        // a stable, printable identifier for each entry.
        for (int32 j = 0; j < pMix->CountFiles; ++j)
        {
            const MixIndexEntry& entry = pMix->Headers[j];
            char hashBuf[16];
            std::snprintf(hashBuf, sizeof(hashBuf), "%08X", entry.FileID);
            pCallback(hashBuf, static_cast<int32>(entry.Size), pUser);
        }
    }
}

// ----------------------------------------------------------------------------
// CCFile_Retrieve_From_MIX - fetch a file's data from any loaded MIX.
// Returns a heap pointer (caller must free with YRMemory::Deallocate) and
// writes the byte count to *pOutSize.  Returns nullptr if not found.
// ----------------------------------------------------------------------------
void* CCFile_Retrieve_From_MIX(const char* pFilename, int32* pOutSize)
{
    if (!pFilename || pFilename[0] == '\0')
        return nullptr;

    void* pData = MixFileClass::Retrieve(pFilename);
    if (!pData)
        return nullptr;

    // If the caller wants the size, find the matching index entry.
    if (pOutSize)
    {
        *pOutSize = 0;
        uint32 crc = MixFileClass::ComputeCRC(pFilename);
        DynamicVectorClass<MixFileClass*>& mixArray = MixFileClass::GetMixArray();
        for (int32 i = 0; i < mixArray.Count; ++i)
        {
            MixFileClass* pMix = mixArray.Items[i];
            if (!pMix)
                continue;
            const MixIndexEntry* pEntry = pMix->GetIndexEntry(crc);
            if (pEntry)
            {
                *pOutSize = static_cast<int32>(pEntry->Size);
                break;
            }
        }
    }
    return pData;
}

// ----------------------------------------------------------------------------
// CCFile_Find_In_MIX - locate which MIX archive holds the file.  Returns
// the MixFileClass pointer (not owned by the caller) or nullptr.
// ----------------------------------------------------------------------------
MixFileClass* CCFile_Find_In_MIX(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0')
        return nullptr;

    uint32 crc = MixFileClass::ComputeCRC(pFilename);
    DynamicVectorClass<MixFileClass*>& mixArray = MixFileClass::GetMixArray();
    for (int32 i = 0; i < mixArray.Count; ++i)
    {
        MixFileClass* pMix = mixArray.Items[i];
        if (!pMix)
            continue;
        if (pMix->FileExists(crc))
            return pMix;
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// CCFile_Count_MIX_Files - returns the total number of files across all
// loaded MIX archives.
// ----------------------------------------------------------------------------
int32 CCFile_Count_MIX_Files()
{
    int32 total = 0;
    DynamicVectorClass<MixFileClass*>& mixArray = MixFileClass::GetMixArray();
    for (int32 i = 0; i < mixArray.Count; ++i)
    {
        MixFileClass* pMix = mixArray.Items[i];
        if (pMix)
            total += pMix->CountFiles;
    }
    return total;
}

// ----------------------------------------------------------------------------
// CCFile_Count_Loaded_MIXs - returns how many MIX archives are loaded.
// ----------------------------------------------------------------------------
int32 CCFile_Count_Loaded_MIXs()
{
    return MixFileClass::GetMixArray().Count;
}

// ============================================================================
// CCFile_Read_All - convenience helper that reads the entire file into a
// caller-supplied buffer.  Returns the number of bytes read, or -1 on
// error.  The buffer must be at least GetSize() bytes.
// ============================================================================
int32 CCFile_Read_All(CCFileClass* pFile, void* pBuffer, int32 nBufferSize)
{
    if (!pFile || !pBuffer || nBufferSize <= 0)
        return -1;

    int32 nSize = pFile->GetSize();
    if (nSize <= 0)
        return 0;
    if (nSize > nBufferSize)
        nSize = nBufferSize;

    return pFile->Read(pBuffer, nSize);
}

// ============================================================================
// CCFile_Copy - high-level helper that copies the contents of one CCFile
// into another.  Returns the number of bytes copied, or -1 on error.
// ============================================================================
int32 CCFile_Copy(CCFileClass* pSrc, CCFileClass* pDst, int32 nChunkSize)
{
    if (!pSrc || !pDst)
        return -1;
    if (nChunkSize <= 0)
        nChunkSize = 4096;

    // Stack-allocated transfer buffer; the original binary uses a
    // YRMemory pool for this.
    char stackBuffer[4096];
    char* pBuffer = stackBuffer;
    if (nChunkSize > 4096)
        nChunkSize = 4096;

    int32 totalCopied = 0;
    while (true)
    {
        int32 nRead = pSrc->Read(pBuffer, nChunkSize);
        if (nRead <= 0)
            break;
        int32 nWritten = pDst->Write(pBuffer, nRead);
        if (nWritten != nRead)
            return -1;
        totalCopied += nWritten;
        if (nRead < nChunkSize)
            break;
    }
    return totalCopied;
}

// ============================================================================
// CD-ROM support
//
//  The original game ships its assets on CD-ROM.  The engine maintains a
//  list of CD-ROM drive letters and searches them when an asset cannot be
//  found on the local hard drive.  These helpers implement the CD search
//  path management and file-lookup helpers.
// ============================================================================

// ----------------------------------------------------------------------------
// CD-ROM drive list - up to 8 drive letters are tracked.
// ----------------------------------------------------------------------------
namespace
{
    const int32 MAX_CD_DRIVES = 8;
    char g_CDDriveList[MAX_CD_DRIVES][4] = { {0}, {0}, {0}, {0},
                                             {0}, {0}, {0}, {0} };
    int32 g_CDDriveCount = 0;
}

// ----------------------------------------------------------------------------
// CCFile_Register_CD_Drive - add a CD-ROM drive letter to the search list.
// Returns true if the drive was added (or was already present).  The drive
// letter is stored as "X:" (2 chars + NUL).
// ----------------------------------------------------------------------------
bool CCFile_Register_CD_Drive(const char* pDriveLetter)
{
    if (!pDriveLetter || pDriveLetter[0] == '\0')
        return false;

    // Extract the drive letter (first non-space character + ':').
    char drive[4] = {0, ':', '\0', '\0'};
    drive[0] = pDriveLetter[0];

    // Check for duplicates.
    for (int32 i = 0; i < g_CDDriveCount; ++i)
    {
        if (g_CDDriveList[i][0] == drive[0])
            return true;
    }

    if (g_CDDriveCount >= MAX_CD_DRIVES)
        return false;

    g_CDDriveList[g_CDDriveCount][0] = drive[0];
    g_CDDriveList[g_CDDriveCount][1] = ':';
    g_CDDriveList[g_CDDriveCount][2] = '\0';
    ++g_CDDriveCount;
    return true;
}

// ----------------------------------------------------------------------------
// CCFile_Clear_CD_Drives - remove all registered CD-ROM drives.
// ----------------------------------------------------------------------------
void CCFile_Clear_CD_Drives()
{
    for (int32 i = 0; i < g_CDDriveCount; ++i)
    {
        g_CDDriveList[i][0] = '\0';
        g_CDDriveList[i][1] = '\0';
        g_CDDriveList[i][2] = '\0';
    }
    g_CDDriveCount = 0;
}

// ----------------------------------------------------------------------------
// CCFile_Get_CD_Drive_Count - returns the number of registered CD drives.
// ----------------------------------------------------------------------------
int32 CCFile_Get_CD_Drive_Count()
{
    return g_CDDriveCount;
}

// ----------------------------------------------------------------------------
// CCFile_Get_CD_Drive - returns the drive string ("X:") at the given index,
// or nullptr if the index is out of range.
// ----------------------------------------------------------------------------
const char* CCFile_Get_CD_Drive(int32 index)
{
    if (index < 0 || index >= g_CDDriveCount)
        return nullptr;
    return g_CDDriveList[index];
}

// ----------------------------------------------------------------------------
// CCFile_Find_On_CD - search every registered CD-ROM drive for the given
// relative file name.  Returns true and fills pOutPath with the fully
// resolved path ("X:/path/to/file") if found.
// ----------------------------------------------------------------------------
bool CCFile_Find_On_CD(const char* pRelativeName, char* pOutPath,
                       int32 nBufferSize)
{
    if (!pRelativeName || !pOutPath || nBufferSize <= 0)
        return false;

    for (int32 i = 0; i < g_CDDriveCount; ++i)
    {
        const char* pDrive = g_CDDriveList[i];
        if (!pDrive[0])
            continue;

        // Build "DRIVE:/relative/path".
        int32 pos = 0;
        int32 j = 0;
        while (pDrive[j] && pos < nBufferSize - 1)
        {
            pOutPath[pos] = pDrive[j];
            ++pos; ++j;
        }
        if (pos < nBufferSize - 1)
        {
            pOutPath[pos] = '/';
            ++pos;
        }
        j = 0;
        while (pRelativeName[j] && pos < nBufferSize - 1)
        {
            pOutPath[pos] = pRelativeName[j];
            ++pos; ++j;
        }
        pOutPath[pos] = '\0';

        if (CCFile_Find_Exists(pOutPath))
            return true;
    }
    return false;
}

// ============================================================================
// Path utility functions
//
//  These helpers extract and manipulate components of file paths.  They are
//  used by the asset loader to determine the directory, base name, and
//  extension of a requested file.
// ============================================================================

// ----------------------------------------------------------------------------
// CCFile_Get_Extension - returns a pointer to the extension (including the
// dot) of the supplied file name, or an empty string if there is none.
// ----------------------------------------------------------------------------
const char* CCFile_Get_Extension(const char* pFilename)
{
    if (!pFilename)
        return "";

    const char* pExt = "";
    int32 lastDot = -1;
    int32 lastSep = -1;
    int32 i = 0;
    while (pFilename[i])
    {
        if (pFilename[i] == '.')
            lastDot = i;
        if (pFilename[i] == '/' || pFilename[i] == '\\')
            lastSep = i;
        ++i;
    }

    // Only treat the dot as an extension if it comes after the last path
    // separator and is not the first character of the name.
    if (lastDot > lastSep && lastDot >= 0)
        pExt = pFilename + lastDot;
    return pExt;
}

// ----------------------------------------------------------------------------
// CCFile_Get_Directory - copies the directory portion of the path (everything
// up to and including the last separator) into pOutBuffer.  If the path has
// no separator, pOutBuffer is set to ".".
// ----------------------------------------------------------------------------
void CCFile_Get_Directory(const char* pFilename, char* pOutBuffer,
                          int32 nBufferSize)
{
    if (!pFilename || !pOutBuffer || nBufferSize <= 0)
        return;

    int32 lastSep = -1;
    int32 i = 0;
    while (pFilename[i])
    {
        if (pFilename[i] == '/' || pFilename[i] == '\\')
            lastSep = i;
        ++i;
    }

    if (lastSep < 0)
    {
        if (nBufferSize >= 2)
        {
            pOutBuffer[0] = '.';
            pOutBuffer[1] = '\0';
        }
        return;
    }

    int32 copyLen = lastSep + 1;
    if (copyLen >= nBufferSize)
        copyLen = nBufferSize - 1;
    for (i = 0; i < copyLen; ++i)
        pOutBuffer[i] = pFilename[i];
    pOutBuffer[copyLen] = '\0';
}

// ----------------------------------------------------------------------------
// CCFile_Get_BaseName - copies the file name without its directory or
// extension into pOutBuffer.
// ----------------------------------------------------------------------------
void CCFile_Get_BaseName(const char* pFilename, char* pOutBuffer,
                         int32 nBufferSize)
{
    if (!pFilename || !pOutBuffer || nBufferSize <= 0)
        return;

    int32 lastSep = -1;
    int32 lastDot = -1;
    int32 i = 0;
    while (pFilename[i])
    {
        if (pFilename[i] == '/' || pFilename[i] == '\\')
            lastSep = i;
        if (pFilename[i] == '.')
            lastDot = i;
        ++i;
    }

    int32 start = (lastSep >= 0) ? lastSep + 1 : 0;
    int32 end = (lastDot > lastSep && lastDot >= 0) ? lastDot : i;

    int32 copyLen = end - start;
    if (copyLen < 0) copyLen = 0;
    if (copyLen >= nBufferSize)
        copyLen = nBufferSize - 1;

    for (i = 0; i < copyLen; ++i)
        pOutBuffer[i] = pFilename[start + i];
    pOutBuffer[copyLen] = '\0';
}

// ----------------------------------------------------------------------------
// CCFile_Join_Path - concatenates a directory and a relative name with a
// path separator.  Handles the case where the directory already ends with a
// separator.
// ----------------------------------------------------------------------------
void CCFile_Join_Path(const char* pDir, const char* pName,
                      char* pOutBuffer, int32 nBufferSize)
{
    if (!pOutBuffer || nBufferSize <= 0)
        return;
    pOutBuffer[0] = '\0';
    if (!pDir || !pName)
        return;

    int32 pos = 0;
    int32 i = 0;
    while (pDir[i] && pos < nBufferSize - 1)
    {
        pOutBuffer[pos] = pDir[i];
        ++pos; ++i;
    }

    // Add a separator if the directory does not end with one.
    if (pos > 0 && pOutBuffer[pos - 1] != '/' && pOutBuffer[pos - 1] != '\\'
        && pos < nBufferSize - 1)
    {
        pOutBuffer[pos] = '/';
        ++pos;
    }

    i = 0;
    while (pName[i] && pos < nBufferSize - 1)
    {
        pOutBuffer[pos] = pName[i];
        ++pos; ++i;
    }
    pOutBuffer[pos] = '\0';
}

// ----------------------------------------------------------------------------
// CCFile_Has_Extension - returns true if the file name has the supplied
// extension (case-insensitive comparison).  The extension may or may not
// include the leading dot.
// ----------------------------------------------------------------------------
bool CCFile_Has_Extension(const char* pFilename, const char* pExtension)
{
    if (!pFilename || !pExtension)
        return false;

    const char* pActualExt = CCFile_Get_Extension(pFilename);
    if (!pActualExt[0] && !pExtension[0])
        return true;
    if (!pActualExt[0] || !pExtension[0])
        return false;

    // Skip the leading dot on the actual extension.
    const char* pA = pActualExt;
    if (pA[0] == '.')
        ++pA;
    const char* pB = pExtension;
    if (pB[0] == '.')
        ++pB;

    // Case-insensitive comparison.
    while (*pA && *pB)
    {
        char a = *pA;
        char b = *pB;
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b)
            return false;
        ++pA; ++pB;
    }
    return (*pA == '\0' && *pB == '\0');
}

// ============================================================================
// File metadata helpers
//
//  These query the filesystem for file size, modification time, and
//  read-only status.  They are used by the patcher and the save manager.
// ============================================================================

// ----------------------------------------------------------------------------
// CCFile_Get_File_Size - returns the file size in bytes, or -1 if the file
// does not exist or cannot be stat'd.
// ----------------------------------------------------------------------------
int32 CCFile_Get_File_Size(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0')
        return -1;
    return FileSystem::GetFileSize(pFilename);
}

// ----------------------------------------------------------------------------
// CCFile_Get_Modification_Time - returns the file's last-modification time
// as a Unix timestamp, or -1 on error.
// ----------------------------------------------------------------------------
int64 CCFile_Get_Modification_Time(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0')
        return -1;

    struct stat st;
    if (stat(pFilename, &st) != 0)
        return -1;
    return static_cast<int64>(st.st_mtime);
}

// ----------------------------------------------------------------------------
// CCFile_Is_ReadOnly - returns true if the file exists and is marked
// read-only.
// ----------------------------------------------------------------------------
bool CCFile_Is_ReadOnly(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0')
        return false;

    struct stat st;
    if (stat(pFilename, &st) != 0)
        return false;
    if (!S_ISREG(st.st_mode))
        return false;
    return (st.st_mode & S_IWUSR) == 0;
}

// ----------------------------------------------------------------------------
// CCFile_Set_ReadOnly - toggles the read-only flag on the file.  Returns
// true on success.
// ----------------------------------------------------------------------------
bool CCFile_Set_ReadOnly(const char* pFilename, bool bReadOnly)
{
    if (!pFilename || pFilename[0] == '\0')
        return false;

    struct stat st;
    if (stat(pFilename, &st) != 0)
        return false;

    mode_t newMode = st.st_mode;
    if (bReadOnly)
        newMode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
    else
        newMode |= S_IWUSR;

    return chmod(pFilename, newMode) == 0;
}

// ============================================================================
// Search-path management
//
//  The asset loader maintains an ordered list of search directories.  When
//  a file is requested, each directory is probed in order until the file is
//  found.  These helpers manage that list.
// ============================================================================

namespace
{
    const int32 MAX_SEARCH_PATHS = 16;
    char g_SearchPaths[MAX_SEARCH_PATHS][MAX_PATH_LEN];
    int32 g_SearchPathCount = 0;
}

// ----------------------------------------------------------------------------
// CCFile_Add_Search_Path - append a directory to the end of the search path
// list.  Returns true if the directory was added.
// ----------------------------------------------------------------------------
bool CCFile_Add_Search_Path(const char* pPath)
{
    if (!pPath || pPath[0] == '\0')
        return false;
    if (g_SearchPathCount >= MAX_SEARCH_PATHS)
        return false;

    int32 i = 0;
    while (pPath[i] && i < MAX_PATH_LEN - 1)
    {
        g_SearchPaths[g_SearchPathCount][i] = pPath[i];
        ++i;
    }
    g_SearchPaths[g_SearchPathCount][i] = '\0';
    ++g_SearchPathCount;
    return true;
}

// ----------------------------------------------------------------------------
// CCFile_Remove_Search_Path - remove a directory from the search path list.
// Returns true if the directory was found and removed.
// ----------------------------------------------------------------------------
bool CCFile_Remove_Search_Path(const char* pPath)
{
    if (!pPath)
        return false;

    for (int32 i = 0; i < g_SearchPathCount; ++i)
    {
        if (std::strcmp(g_SearchPaths[i], pPath) == 0)
        {
            // Shift remaining entries down.
            for (int32 j = i; j < g_SearchPathCount - 1; ++j)
            {
                std::strcpy(g_SearchPaths[j], g_SearchPaths[j + 1]);
            }
            g_SearchPaths[g_SearchPathCount - 1][0] = '\0';
            --g_SearchPathCount;
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// CCFile_Clear_Search_Paths - remove all search paths.
// ----------------------------------------------------------------------------
void CCFile_Clear_Search_Paths()
{
    for (int32 i = 0; i < g_SearchPathCount; ++i)
        g_SearchPaths[i][0] = '\0';
    g_SearchPathCount = 0;
}

// ----------------------------------------------------------------------------
// CCFile_Get_Search_Path_Count - returns the number of registered search
// directories.
// ----------------------------------------------------------------------------
int32 CCFile_Get_Search_Path_Count()
{
    return g_SearchPathCount;
}

// ----------------------------------------------------------------------------
// CCFile_Get_Search_Path - returns the search path at the given index, or
// nullptr if out of range.
// ----------------------------------------------------------------------------
const char* CCFile_Get_Search_Path(int32 index)
{
    if (index < 0 || index >= g_SearchPathCount)
        return nullptr;
    return g_SearchPaths[index];
}

// ----------------------------------------------------------------------------
// CCFile_Find_In_Search_Paths - probes every registered search directory
// for the given relative file name.  Returns true and fills pOutPath with
// the fully resolved path if found.
// ----------------------------------------------------------------------------
bool CCFile_Find_In_Search_Paths(const char* pRelativeName, char* pOutPath,
                                 int32 nBufferSize)
{
    if (!pRelativeName || !pOutPath || nBufferSize <= 0)
        return false;

    // Build an array of const char* pointing at the search paths so we can
    // reuse CCFile_Resolve_Path.
    const char* pathPtrs[MAX_SEARCH_PATHS];
    for (int32 i = 0; i < g_SearchPathCount; ++i)
        pathPtrs[i] = g_SearchPaths[i];

    return CCFile_Resolve_Path(pRelativeName, pathPtrs, g_SearchPathCount,
                               pOutPath, nBufferSize);
}

// ----------------------------------------------------------------------------
// CCFile_Find_Anywhere - the master lookup function.  It checks:
//   1. The MIX archives (if any are loaded)
//   2. The registered search paths
//   3. The CD-ROM drives
//   4. The literal relative path
// Returns true and fills pOutPath if found anywhere.  If the file is only
// inside a MIX archive, pOutPath is set to the original name (since MIX
// files do not have filesystem paths).
// ----------------------------------------------------------------------------
bool CCFile_Find_Anywhere(const char* pFilename, char* pOutPath,
                          int32 nBufferSize)
{
    if (!pFilename || !pOutPath || nBufferSize <= 0)
        return false;

    // 1. Check MIX archives first (they take priority in the original engine).
    if (CCFile_Is_In_MIX(pFilename))
    {
        int32 i = 0;
        while (pFilename[i] && i < nBufferSize - 1)
        {
            pOutPath[i] = pFilename[i];
            ++i;
        }
        pOutPath[i] = '\0';
        return true;
    }

    // 2. Check the registered search paths.
    if (CCFile_Find_In_Search_Paths(pFilename, pOutPath, nBufferSize))
        return true;

    // 3. Check the CD-ROM drives.
    if (CCFile_Find_On_CD(pFilename, pOutPath, nBufferSize))
        return true;

    // 4. Check the literal relative path.
    if (CCFile_Find_Exists(pFilename))
    {
        int32 i = 0;
        while (pFilename[i] && i < nBufferSize - 1)
        {
            pOutPath[i] = pFilename[i];
            ++i;
        }
        pOutPath[i] = '\0';
        return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// CCFile_Open_Anywhere - the master open function.  It locates the file
// using CCFile_Find_Anywhere and opens it for reading.  Returns a heap-
// allocated CCFileClass (caller owns it) or nullptr.
// ----------------------------------------------------------------------------
CCFileClass* CCFile_Open_Anywhere(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0')
        return nullptr;

    char resolvedPath[MAX_PATH_LEN];
    if (!CCFile_Find_Anywhere(pFilename, resolvedPath, MAX_PATH_LEN))
        return nullptr;

    return CCFile_Open_For_Read(resolvedPath);
}
