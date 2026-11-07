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
    // Static constructors of the three DynamicVectorClass registries
    //========================================================================
    //
    // asm: MixFileClass_SCTOR2 (asm:71204) / _SCTOR3 (asm:71256) /
    //      _SCTOR4 (asm:71308)
    //
    //   Original 局部（.data）各挂一个 DynamicVectorClass<MixFileClass*>
    //   全局：vec_MixFiles / vec_MAPS_MIX / vec_Movies_MIX。每个都由一段
    //   静态构造函数铺底，逐字段：
    //     [data]=0  [count]=0  [IsInitialized]=1  [IsAllocated]=0
    //     [vtable]=DynamicVectorClass<MixFileClass*>::`vftable'
    //     [incrementAmount]=0Ah  [length]=0
    //   随后 atexit(loc_40D400/…80/…00) 注册析构：析构把 vtable 回落成
    //   VectorClass<MixFileClass*>，若 IsAllocated 则 delete data，并把
    //   data / IsAllocated / count 清零。
    //
    //   本工程以函数内静态对象表达同一语义：首次调用构造（等价 SCTOR 铺底），
    //   进程退出按注册顺序析构（等价 atexit）。GetMixArray/GetMaps/GetMovies
    //   与这三个 sctor 同源，保证注册表只有一份。
    //========================================================================
    static DynamicVectorClass<MixFileClass*>& sctor2();   // asm: vec_MixFiles
    static DynamicVectorClass<MixFileClass*>& sctor3();   // asm: vec_MAPS_MIX
    static DynamicVectorClass<MixFileClass*>& sctor4();   // asm: vec_Movies_MIX

    //========================================================================
    // Construction / Destruction
    //========================================================================

    explicit MixFileClass(const char* pFileName);

 // MixFileClass_CTOR - the two-argument form.  pKey is the
    // archive's encryption key, handed to PKStraw_SetKey when the header's
    // flag bit marks the index as encrypted.  A missing or empty key leaves
    // the archive unmounted from the encrypted path, exactly as passing a
    // null key does in the original.
    MixFileClass(const char* pFileName, const char* pKey);

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

    // ------------------------------------------------------------------------
    // asm: MixFileClass CTOR's runtime dispatch over the whole registry.  The
    // lookup walks the loaded-archive list, CRC-hashes the requested name and
    // binary-searches each archive's (CRC-sorted) index table.  On a hit the
    // owning archive, the entry, the byte offset within the MIX body and the
    // entry length are handed back to the caller.
    //
    // The retail entry point takes the three out-parameters directly; the
    // convenience overload below keeps the historical return-the-archive form
    // used elsewhere in the tree while forwarding the same search.
    // ------------------------------------------------------------------------
    static MixFileClass* FindFileByName(const char* pName,
                                        int32* pOutLength = nullptr,
                                        int32* pOutOffset = nullptr);

    // Get the index entry for a file (returns nullptr if not found)
    const MixIndexEntry* GetIndexEntry(const char* pFileName) const;

    // Get the index entry by CRC32 hash
    const MixIndexEntry* GetIndexEntry(uint32 crc) const;

    //========================================================================
    // Static utility methods
    //========================================================================

    // Find a file in all loaded MIX archives
    static void* Retrieve(const char* pFileName, bool forceShapeCache = false);

    // Same search, but also reports the byte size. Asset loaders (SHP/VXL/HVA)
    // need the length to bounds-check the header before parsing. The returned
    // block is owned by the caller and must be released with YRMemory::Deallocate.
    static void* RetrieveSized(const char* pFileName, int32* pOutSize);

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

    // asm: the physical size of the leading header block (10 bytes for the
    // RA2 flag-word flavour, 6 for the plain flavour).  Kept so the body start
    // offset and the cached-index pointer can be recomputed without re-parsing.
    int32 HeaderSize;

    // The archive encryption key handed to the two-argument CTOR (borrowed,
    // not owned).  Only consulted while the encrypted path is active.
    const char* stashedKey;

    // Internal file handle
    CCFileClass* FileHandle;

    // Cached data buffer
    uint8* CachedData;
    int32 CachedSize;

    // CRC table (lazy-initialized)
    static uint32 CRCTable[256];
    static bool CRCTableInitialized;

public:
    // asm: MixFileClass CTOR tail - register this archive in every live
    // registry (list_MixFiles + vec_MixFiles + vec_MixFiles_0).  Every later
    // lookup walks these, so the archive must be inserted exactly once, after
    // the index has been parsed.
    void AddFile();

    // asm: MixFileClass CTOR lookup - fetch one member as a freshly allocated
    // block owned by the caller (mirrors the global Retrieve but scoped to
    // this archive).
    void* RetrieveFile(const char* pFileName, int32* pOutSize = nullptr);

private:
    DISABLE_COPY_AND_MOVE(MixFileClass)
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 MIX 注册表还提供链修正与列表装卸。
    // ------------------------------------------------------------------------
    void Init_List();
    void UnInit_List();
    bool CorrectChain();

};