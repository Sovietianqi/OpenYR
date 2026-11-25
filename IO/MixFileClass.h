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
// Two header flavours exist; which one is in use is decided by the first
// 16-bit word (asm MixFileClass_CTOR, asm.txt:913026):
//
//   word[0] != 0  ->  plain (TS style)
//       uint16 FileCount
//       uint32 BodySize
//
//   word[0] == 0  ->  RA2 / YR style
//       uint16 ZeroSentinel        (always 0)
//       uint16 Flags               bit0 = has checksum, bit1 = index encrypted
//       [if bit1] 80-byte RSA block (2 x 40 bytes, see below)
//       uint16 FileCount           } plain, or the first 6 bytes of the
//       uint32 BodySize            } Blowfish-ECB stream
//
// [Index Table] (FileCount entries, 12 bytes each, CRC-ascending)
//   uint32 FileID      - CRC32 hash of the file name (case-insensitive)
//   uint32 Offset      - offset from the END OF THE INDEX to the file data
//   uint32 Size        - size of the file data
//
// When Flags bit1 is set, the {FileCount, BodySize} pair and the index table
// form ONE continuous Blowfish-ECB stream that starts right after the 80-byte
// RSA block; the tail of the last 8-byte block is zero padding.  The 56-byte
// Blowfish key is not stored anywhere - it is recovered from the RSA block
// with Key_0's public exponent (see "Blowfish key for RA2/YR MIX files").
//
// [File Data]
//   Raw file data (never encrypted), possibly compressed with PKWare or LZO.
//
// IMPORTANT: BodySize is not consistent across archives (some include the
// index, some do not), so this implementation measures the physical stream
// length at open time and derives the data window from that.
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
    //
    // This used to be an unresolved gap; it is now fully derived from the
    // binary.  The chain is:
    //
    //  1. WinMain's Init_Keys (asm.txt:1435396) feeds
    //       "[PublicKey]\n1=AihRvNoIbTn85FZRYNZRcT+i6KpU+maCsEqr3Q5q+LDB5tH7Tz2qQ38V"
    //     to an INIClass and calls INIClass::Get_PKey(out, 1).
    //  2. Get_PKey (asm.txt:654046) with arg_4 == 1 does NOT read a private
    //     key: it DER-encodes the constant 0x10001 into the exponent slot and
    //     reads the "PublicKey" UUBlock for the modulus.
    //  3. The UUBlock is base64: 56 chars -> 42 bytes = DER INTEGER
    //       tag 0x02, length 0x28, then 40 bytes of modulus
    //       51BCDA086D39FCE4...3DAA437F15  (319 bits).
    //     PKey::Decode_Modulus (asm.txt:1165729) stores that as a little-endian
    //     bignum and writes Key_Voodoo3()-1 (318) to Key_0+0x200.
    //  4. MixFileClass_CTOR (asm.txt:913144) hands &Key_0 to PKStraw_SetKey and
    //     creates PKStraw with mode 1, so PKStraw_ReadNextBytes takes the
    //     "raw key" branch: it reads (55/39 + 1) * 40 = 80 bytes from the
    //     stream and RSA-recovers them through sub_632870 -> sub_5C8500.
    //  5. Each 40-byte chunk c is transformed to c^e mod n (e = 65537), which
    //     is *public-key signature recovery* - no private exponent needed.
    //     The 80 recovered bytes are handed to BlowStraw in two 40-byte
    //     pieces, and the first 56 of them are the Blowfish key.
    //
    // Every step above is a direct reading of the assembly; nothing is
    // guessed.  The RSA parameters below are the decoded Key_0 fields.
    //========================================================================
    static constexpr int32 BlowfishKeySize = 56;
    // Bytes consumed from the archive before the Blowfish stream starts:
    // 4-byte leading header + 80-byte RSA key block.
    static constexpr int32 BlowfishKeyBlockSize = 80;

protected:
    // Internal: read the MIX header
    bool ReadHeader();

    // Internal: read the index table
    bool ReadIndex();

    // Internal: calculate CRC32 table
    static void InitCRCTable();

public:
    const char* FileName;
    bool Blowfish;
    bool Encryption;
    int32 CountFiles;
    int32 FileSize;
    int32 BodySize;
    // True when the index table is sorted by FileID ascending, which is what
    // the retail binary search assumes.  Archives that are not sorted fall back
    // to a linear scan instead of returning wrong results.
    bool IndexSorted;
    // Bytes of file data that actually follow the index (FileSize -
    // FileStartOffset).  The header's BodySize field is documented as "body
    // size" but retail archives disagree on whether the index is included, so
    // every bounds check uses this measured value instead of trusting it.
    int32 DataSize;
    int32 FileStartOffset;
    MixIndexEntry* Headers;
    int32 field_24;

    // asm: the physical size of the leading header block (10 bytes for the
    // RA2 flag-word flavour, 6 for the plain flavour).  Kept so the body start
    // offset and the cached-index pointer can be recomputed without re-parsing.
    //
    // For a Blowfish archive the header is *not* contiguous: 4 plain bytes,
    // then the 80-byte RSA key block, then the encrypted {count, bodySize}
    // pair.  HeaderSize therefore counts the whole prefix (4 + 80 + 6 = 90)
    // while BlowfishStreamOffset records where the ciphertext actually starts
    // (4 + 80 = 84), which is what the cached copy has to decrypt.
    int32 HeaderSize;
    int32 BlowfishStreamOffset;

    // Blowfish key recovered from the archive's RSA block (valid only while
    // BlowfishStreamOffset != 0).
    uint8 BlowfishKey[BlowfishKeySize];

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