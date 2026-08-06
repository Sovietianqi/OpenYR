#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Core/Memory.h"
#include "Containers/ListClass.h"
#include "Containers/DynamicVectorClass.h"
#include "IO/CCFileClass.h"

#include <cstring>

//========================================================================
// MixFileClass - MIX Archive Container
//
// MIX files are Westwood's proprietary archive format used to bundle
// game assets. The format consists of:
//
// [Header]
//   uint16 Flags       - bit 0 = has checksum, bit 1 = encrypted
//   uint16 FileCount   - number of files in the archive
//   uint32 BodySize    - total size of the file data (excluding header)
//
// [Index Table] (FileCount entries)
//   uint32 FileID      - CRC32 hash of the file name (case-insensitive)
//   uint32 Offset      - offset from start of body to this file's data
//   uint32 Size        - size of the file data
//
// [File Data]
//   Raw file data, possibly compressed with PKWare or LZO
//
// The MIX format supports two encryption/compression flavors:
// 1. RA2 format: Blowfish encryption + PKWare compression
// 2. TS format: PKWare compression only
//
// MIX files are chained in a linked list. The game searches for
// files by iterating through all loaded MIX files.
//========================================================================

//========================================================================
// MixHeader - MIX file header structure
//========================================================================

#pragma pack(push, 1)
struct MixHeader
{
    uint16 Flags;       // 0 = checksum, 1 = encrypted
    uint16 FileCount;   // number of files
    uint32 BodySize;    // total size of body data (excluding header)

    static constexpr uint16 FLAG_CHECKSUM  = 0x0001;
    static constexpr uint16 FLAG_ENCRYPTED = 0x0002;

    bool HasChecksum() const { return (Flags & FLAG_CHECKSUM) != 0; }
    bool IsEncrypted() const { return (Flags & FLAG_ENCRYPTED) != 0; }
};

//========================================================================
// MixIndexEntry - MIX file index entry
//========================================================================

struct MixIndexEntry
{
    uint32 FileID;      // CRC32 hash of the file name
    uint32 Offset;      // offset from start of body
    uint32 Size;        // size of the file data

    bool operator==(const MixIndexEntry& other) const
    {
        return FileID == other.FileID && Offset == other.Offset && Size == other.Size;
    }
};

//========================================================================
// Sentinal markers for special MIX files
//========================================================================

struct MixSentinel
{
    static constexpr uint32 CACHE_ENTRY = 0xFFFFFFFF;
    static constexpr uint32 END_OF_INDEX = 0x00000000;
    static constexpr uint32 EMPTY_ENTRY = 0x00000000;
};

#pragma pack(pop)

//========================================================================
// MixFileClass - MIX archive class
//========================================================================

class MixFileClass : public Node<MixFileClass>
{
public:
    //========================================================================
    // Static members - global MIX file registry
    //========================================================================

    // All loaded MIX files (linked list)
    static List<MixFileClass*>& GetMixList();

    // All loaded MIX files (array for fast iteration)
    static DynamicVectorClass<MixFileClass*>& GetMixArray();

    // Map-specific MIX files
    static DynamicVectorClass<MixFileClass*>& GetMaps();

    // Movie-specific MIX files
    static DynamicVectorClass<MixFileClass*>& GetMovies();

    //========================================================================
    // Construction / Destruction
    //========================================================================

    explicit MixFileClass(const char* pFileName);

    virtual ~MixFileClass() noexcept override;

    //========================================================================
    // MIX file operations
    //========================================================================

    // Open the MIX file and parse its header and index
    bool Open();

    // Close the MIX file and free all resources
    void Close();

    // Cache the MIX file contents for faster access
    bool Cache();

    // Check if a file exists in this MIX archive
    bool FileExists(const char* pFileName) const;

    // Check if a file exists by its CRC32 hash
    bool FileExists(uint32 crc) const;

    // Get a file's data from this MIX archive
    // Returns a pointer to the file data (caller must free with YRMemory::Deallocate)
    void* GetFile(const char* pFileName, int32* pOutSize = nullptr) const;

    // Get a file's data by CRC32 hash
    void* GetFile(uint32 crc, int32* pOutSize = nullptr) const;

    // Get the index entry for a file (returns nullptr if not found)
    const MixIndexEntry* GetIndexEntry(const char* pFileName) const;

    // Get the index entry by CRC32 hash
    const MixIndexEntry* GetIndexEntry(uint32 crc) const;

    //========================================================================
    // Static utility methods
    //========================================================================

    // Find a file in all loaded MIX archives
    static void* Retrieve(const char* pFileName, bool forceShapeCache = false);

    // Find a file's offset within a MIX archive
    static bool Offset(const char* pFileName, void*& pData,
                       MixFileClass*& pMixFile, int32& offset, int32& length);

    // Destroy all cached MIX data
    static void DestroyCache();

    // Initialize the MIX file system
    static void Bootstrap();

    //========================================================================
    // CRC32 utility
    //========================================================================

    // Compute CRC32 for a file name (case-insensitive, as used by MIX)
    static uint32 ComputeCRC(const char* pFileName);

    //========================================================================
    // Accessors
    //========================================================================

    const char* GetFileName() const { return FileName; }
    bool IsEncrypted() const { return Encryption; }
    bool IsBlowfish() const { return Blowfish; }
    int32 GetFileCount() const { return CountFiles; }
    int32 GetTotalSize() const { return FileSize; }

    //========================================================================
    // Blowfish key for RA2/YR MIX files
    //========================================================================

    static constexpr uint8 BlowfishKey[16] = {
        0x3A, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

protected:
    // Internal: read the MIX header
    bool ReadHeader();

    // Internal: read the index table
    bool ReadIndex();

    // Internal: decrypt the MIX header (Blowfish)
    bool DecryptHeader();

    // Internal: calculate CRC32 table
    static void InitCRCTable();

public:
    const char* FileName;
    bool Blowfish;
    bool Encryption;
    int32 CountFiles;
    int32 FileSize;
    int32 BodySize;
    int32 FileStartOffset;
    MixIndexEntry* Headers;
    int32 field_24;

    // Internal file handle
    CCFileClass* FileHandle;

    // Cached data buffer
    uint8* CachedData;
    int32 CachedSize;

    // CRC table (lazy-initialized)
    static uint32 CRCTable[256];
    static bool CRCTableInitialized;

private:
    DISABLE_COPY_AND_MOVE(MixFileClass)
};