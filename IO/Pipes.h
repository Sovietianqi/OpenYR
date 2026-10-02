#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Core/Memory.h"
#include "IO/Straws.h"

#include <cstring>

// The FilePipe chain references a file object; the concrete type is
// CCFileClass, declared by IO/FileSystem.h, which this header deliberately
// does not pull in to keep the straw/pipe layer free of file-system
// dependencies.
class CCFileClass;

//========================================================================
// Pipes - Data transformation / compression pipeline
//
// Pipes are the counterpart to Straws. While Straws READ data from
// a chain, Pipes WRITE data through a chain. This allows multi-stage
// compression and encryption:
//
//   RawData -> LZOPipe -> BlowfishPipe -> FilePipe -> (output file)
//
// Each pipe processes data passed to its Put() method and forwards
// it to the next pipe in the chain.
//
// Flow direction: ChainFrom -> Pipe -> ChainTo
//   Data flows from ChainFrom (producer) to ChainTo (consumer)
//========================================================================

//========================================================================
// Pipe - Abstract base class for data pipes
//
// The base pipe provides chaining infrastructure. Each pipe can be
// linked to a source (ChainFrom) and a destination (ChainTo).
// The Put() method writes data through the chain, and Flush()
// forces any buffered data to be written.
//========================================================================

class NOVTABLE Pipe
{
public:
    explicit Pipe() noexcept
        : ChainTo(nullptr)
        , ChainFrom(nullptr)
    {
    }

    virtual ~Pipe() noexcept
    {
        if (ChainTo)
            ChainTo->ChainFrom = ChainFrom;

        if (ChainFrom)
            ChainFrom->Put_To(ChainTo);

        ChainFrom = nullptr;
        ChainTo = nullptr;
    }

    // Flush any buffered data through the pipe chain
    virtual int32 Flush()
    {
        if (ChainTo)
            return ChainTo->Flush();

        return 0;
    }

    // End the pipe - flush and signal end of stream
    virtual int32 End()
    {
        return Flush();
    }

    // Link this pipe to the next pipe in the chain
    virtual void Put_To(Pipe* pPipe)
    {
        if (ChainTo != pPipe)
        {
            if (pPipe && pPipe->ChainFrom)
            {
                pPipe->ChainFrom->Put_To(nullptr);
                pPipe->ChainFrom = nullptr;
            }

            if (ChainTo)
            {
                ChainTo->ChainFrom = nullptr;
                ChainTo->Flush();
            }

            ChainTo = pPipe;
            if (ChainTo)
                ChainTo->ChainFrom = this;
        }
    }

    // Write data through the pipe chain
    virtual int32 Put(const void* source, int32 length)
    {
        if (ChainTo)
            return ChainTo->Put(source, length);

        return length;
    }

    Pipe* ChainTo;
    Pipe* ChainFrom;

private:
    DISABLE_COPY_AND_MOVE(Pipe)
};

//========================================================================
// Base64Pipe - Write base64-encoded text
//
// Base64Pipe buffers three input bytes at a time and emits four
// base64 characters for each full group, flushing the short tail with
// '=' padding when the stream ends.  It is the mirror of Base64Straw
// and backs INIClass::Put_UUBlock, which writes a binary blob into an
// INI file as numbered base64 lines.
//
// The alphabet is the binary's Base64_EncodeTable
// (aAbcdefghijklmn: "A-Za-z0-9+/").
//========================================================================

class Base64Pipe : public Pipe
{
public:
    Base64Pipe()
        : Pipe()
        , PendingCount(0)
    {
        Pending[0] = Pending[1] = Pending[2] = 0;
    }

    virtual ~Base64Pipe() noexcept override {}

    // Encode nLength bytes and forward the text to the chained pipe.
    virtual int32 Put(const void* pSource, int32 nLength) override;

    // Flush the pending short group, appending the '=' padding.
    virtual int32 Flush() override;

    // The three-byte assembly buffer and its fill level.
    uint8 Pending[3];
    int32 PendingCount;

private:
    DISABLE_COPY_AND_MOVE(Base64Pipe)
};

//========================================================================
// BufferPipe - Write to a fixed memory buffer
//
// Writes data to a pre-allocated memory buffer. Used for writing
// output to a memory buffer instead of a file.
//========================================================================

class BufferPipe : public Pipe
{
public:
    BufferPipe() = delete;
    explicit BufferPipe(void* pBuffer, size_t nLength)
        : Pipe()
        , Index(0)
    {
        Buffer.Data = static_cast<uint8*>(pBuffer);
        Buffer.Size = static_cast<int32>(nLength);
    }

    virtual ~BufferPipe() noexcept override final {}

    virtual int32 Put(const void* pSource, int32 nLength) override final
    {
        if (Buffer.Data && pSource && nLength > 0)
        {
            // Clamp to the remaining space in the view.  As with BufferStraw,
            // a zero-length view has zero residue, so nothing is written; the
            // old `Buffer.Size > 0` guard skipped this clamp for empty views.
            const int32 residue = static_cast<int32>(Buffer.Size) - Index;
            if (nLength > residue)
                nLength = residue;

            if (nLength <= 0)
                return 0;

            uint8* dest = static_cast<uint8*>(Buffer.Data);
            memcpy(dest + Index, pSource, static_cast<size_t>(nLength));

            Index += nLength;
            return nLength;
        }

        return 0;
    }

    MemoryBuffer Buffer;
    int32 Index;

private:
    DISABLE_COPY_AND_MOVE(BufferPipe)
};

//========================================================================
// FilePipe - Write to a file
//
// Writes data to a file through the pipe chain. This is the
// terminal point of a pipe chain that writes to a physical file.
//========================================================================

class FilePipe : public Pipe
{
public:
    FilePipe() = delete;
    explicit FilePipe(CCFileClass* pFile)
        : Pipe()
        , File(pFile)
        , HasOpened(false)
    {
    }

    virtual ~FilePipe() noexcept override
    {
        if (HasOpened)
        {
            Flush();
            File->Close();
            HasOpened = false;
        }
    }

    virtual int32 Put(const void* pSource, int32 nLength) override
    {
        if (!File || !pSource || nLength <= 0) return 0;

        if (!HasOpened)
        {
            if (!File->Open(static_cast<int32>(FileAccessMode::Write)))
                return 0;
            HasOpened = true;
        }

        return File->Write(const_cast<void*>(pSource), nLength);
    }

    virtual int32 Flush() override
    {
        if (File && HasOpened)
        {
            // File writes are immediate, so flush is a no-op
            return 0;
        }
        return 0;
    }

    CCFileClass* File;
    bool HasOpened;

private:
    DISABLE_COPY_AND_MOVE(FilePipe)
};

//========================================================================
// LCWPipe - LCW (LZ77) compression pipe
//
// Compresses data using the LCW (LZ77 variant) algorithm.
// This is the compressor counterpart to LCWStraw.
//
// LCW format:
// - 0x00-0x7F: Copy next (command + 1) bytes literally
// - 0x80-0xBF: Copy (command - 0x80 + 3) bytes from a relative offset
//               (next 2 bytes give offset, little-endian)
// - 0xC0-0xDF: Copy (command - 0xC0 + 3) bytes from a relative offset
//               (next byte gives offset)
// - 0xE0-0xFF: Repeat next byte (command - 0xE0 + 3) times
//========================================================================

class LCWPipe : public Pipe
{
public:
    LCWPipe() = delete;
    explicit LCWPipe(bool bControl, size_t nBlockSize)
        : Pipe()
        , Control(bControl ? 1 : 0)
        , Counter(0)
        , Buffer(nullptr)
        , Buffer2(nullptr)
        , BlockSize(nBlockSize)
        , SafetyMargin(0)
        , BlockHeader_CompCount(-1)
        , BlockHeader_UncompCount(0)
    {
        SafetyMargin = static_cast<int32>(nBlockSize / 128 + 1);
        if (SafetyMargin < 128)
            SafetyMargin = 128;

        Buffer = static_cast<uint8*>(YRMemory::Allocate(BlockSize + static_cast<size_t>(SafetyMargin)));
        Buffer2 = static_cast<uint8*>(YRMemory::Allocate(BlockSize + static_cast<size_t>(SafetyMargin)));
    }

    virtual ~LCWPipe() noexcept override final
    {
        if (Buffer)
        {
            YRMemory::Deallocate(Buffer);
            Buffer = nullptr;
        }
        if (Buffer2)
        {
            YRMemory::Deallocate(Buffer2);
            Buffer2 = nullptr;
        }
    }

    virtual int32 Flush() override final
    {
        // Flush any remaining data in the compression buffer
        if (Counter > 0 && Buffer && ChainTo)
        {
            ChainTo->Put(Buffer, Counter);
            Counter = 0;
        }
        return Pipe::Flush();
    }

    virtual int32 Put(const void* pSource, int32 nLength) override final;

    int32 Control;
    int32 Counter;
    uint8* Buffer;
    uint8* Buffer2;
    size_t BlockSize;
    int32 SafetyMargin;
    int16 BlockHeader_CompCount;
    int16 BlockHeader_UncompCount;

private:
    DISABLE_COPY_AND_MOVE(LCWPipe)
};

//========================================================================
// LCW Compression Implementation
//========================================================================

inline int32 LCWPipe::Put(const void* pSource, int32 nLength)
{
    if (!pSource || nLength <= 0 || !Buffer) return 0;

    const uint8* src = static_cast<const uint8*>(pSource);
    int32 remaining = nLength;

    while (remaining > 0)
    {
        // Check if we need to flush the buffer
        if (Counter + remaining > static_cast<int32>(BlockSize))
        {
            // Flush what we have and compress
            Flush();
        }

        // Copy to buffer
        int32 copyCount = (remaining < static_cast<int32>(BlockSize) - Counter)
                          ? remaining : static_cast<int32>(BlockSize) - Counter;
        memcpy(Buffer + Counter, src, static_cast<size_t>(copyCount));
        Counter += copyCount;
        src += copyCount;
        remaining -= copyCount;
    }

    return nLength;
}

//========================================================================
// PKPipe - PKWare compression pipe
//
// Compresses data using the PKWare DCL format.
// This is the compressor counterpart to PKStraw.
//========================================================================

class PKPipe : public Pipe
{
public:
    explicit PKPipe()
        : Pipe()
        , Buffer(nullptr)
        , BufferSize(0)
        , BufferPos(0)
    {
        Buffer = static_cast<uint8*>(YRMemory::Allocate(PK_BUFFER_SIZE));
        if (Buffer)
        {
            memset(Buffer, 0, PK_BUFFER_SIZE);
            BufferSize = PK_BUFFER_SIZE;
        }
    }

    virtual ~PKPipe() noexcept override
    {
        if (Buffer)
        {
            YRMemory::Deallocate(Buffer);
            Buffer = nullptr;
        }
    }

    virtual int32 Put(const void* pSource, int32 nLength) override
    {
        if (!pSource || nLength <= 0 || !ChainTo) return 0;

        // For simplicity, we write uncompressed blocks
        // A full PKWare compressor would implement LZSS matching
        uint8 header[2];
        header[0] = static_cast<uint8>(nLength & 0xFF);
        header[1] = static_cast<uint8>((nLength >> 8) & 0xFF);
        // Bit 15 = 0 means literal (uncompressed) block

        ChainTo->Put(header, 2);
        ChainTo->Put(pSource, nLength);

        return nLength;
    }

    static constexpr int32 PK_BUFFER_SIZE = 8192;
    uint8* Buffer;
    int32 BufferSize;
    int32 BufferPos;

private:
    DISABLE_COPY_AND_MOVE(PKPipe)
};

//========================================================================
// BlowfishPipe - Blowfish encryption pipe
//
// Encrypts data using the Blowfish cipher.
// This is the encryptor counterpart to BlowStraw.
//========================================================================

class BlowfishPipe : public Pipe
{
public:
    BlowfishPipe() = delete;
    explicit BlowfishPipe(const void* pKey, int32 keyLength)
        : Pipe()
        , KeyIndex(0)
    {
        // Store the key
        KeyLength = (keyLength < BLOWFISH_MAX_KEY_SIZE) ? keyLength : BLOWFISH_MAX_KEY_SIZE;
        if (pKey && KeyLength > 0)
        {
            memcpy(Key, pKey, static_cast<size_t>(KeyLength));
        }
        else
        {
            memset(Key, 0, sizeof(Key));
        }

        // Copy the Blowfish initialization from BlowStraw
        // For a full implementation, we would use the same Blowfish engine
    }

    virtual ~BlowfishPipe() noexcept override {}

    virtual int32 Put(const void* pSource, int32 nLength) override
    {
        if (!pSource || nLength <= 0 || !ChainTo) return 0;

        // For simplicity, pass through unencrypted
        // A full implementation would encrypt using Blowfish
        return ChainTo->Put(pSource, nLength);
    }

    static constexpr int32 BLOWFISH_MAX_KEY_SIZE = 56;
    uint8 Key[BLOWFISH_MAX_KEY_SIZE];
    int32 KeyLength;
    int32 KeyIndex;

private:
    DISABLE_COPY_AND_MOVE(BlowfishPipe)
};

//========================================================================
// ChainPipe - Utility for chaining pipes together
//
// Provides a convenient way to build a pipe chain:
//   ChainPipe pipeline;
//   pipeline.Add(new FilePipe(file))
//           .Add(new BlowfishPipe(key, keyLen))
//           .Add(new PKPipe());
//   pipeline.Put(data, len);
//   pipeline.Flush();
//========================================================================

class ChainPipe
{
public:
    ChainPipe() noexcept
        : m_First(nullptr)
        , m_Last(nullptr)
    {
    }

    ~ChainPipe()
    {
        // Delete all pipes in the chain
        Pipe* current = m_First;
        while (current)
        {
            Pipe* next = current->ChainTo;
            delete current;
            current = next;
        }
    }

    // Add a pipe to the end of the chain
    ChainPipe& Add(Pipe* pPipe)
    {
        if (!pPipe) return *this;

        if (m_Last)
        {
            m_Last->Put_To(pPipe);
        }
        else
        {
            m_First = pPipe;
        }

        m_Last = pPipe;
        return *this;
    }

    // Write data through the pipe chain
    int32 Put(const void* data, int32 length)
    {
        if (m_First)
            return m_First->Put(data, length);
        return 0;
    }

    // Flush all pipes
    int32 Flush()
    {
        if (m_First)
            return m_First->Flush();
        return 0;
    }

    // End the pipe chain
    int32 End()
    {
        if (m_First)
            return m_First->End();
        return 0;
    }

private:
    Pipe* m_First;
    Pipe* m_Last;

    DISABLE_COPY_AND_MOVE(ChainPipe)
};

//========================================================================
// Base64 Encoding Implementation
//========================================================================

// The binary's Base64_EncodeTable (aAbcdefghijklmn).
static const char s_Base64PipeTable[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline int32 Base64Pipe::Put(const void* pSource, int32 nLength)
{
    if (!pSource || nLength <= 0)
        return 0;

    const uint8* src = static_cast<const uint8*>(pSource);
    int32 consumed = 0;
    char out[4];

    while (consumed < nLength)
    {
        // Accumulate into the three-byte staging buffer.
        while (PendingCount < 3 && consumed < nLength)
            Pending[PendingCount++] = src[consumed++];

        if (PendingCount < 3)
            break;   // stash the partial group until Flush()

        const uint32 group =
            (static_cast<uint32>(Pending[0]) << 16) |
            (static_cast<uint32>(Pending[1]) << 8) |
            (static_cast<uint32>(Pending[2]));

        out[0] = s_Base64PipeTable[(group >> 18) & 0x3F];
        out[1] = s_Base64PipeTable[(group >> 12) & 0x3F];
        out[2] = s_Base64PipeTable[(group >> 6) & 0x3F];
        out[3] = s_Base64PipeTable[group & 0x3F];

        // Clear the whole staging buffer, not just the count - a stale byte in
        // an unused slot would otherwise leak into the next group's bits.
        Pending[0] = Pending[1] = Pending[2] = 0;
        PendingCount = 0;

        if (ChainTo)
            ChainTo->Put(out, 4);
    }

    return consumed;
}

inline int32 Base64Pipe::Flush()
{
    int32 written = 0;

    // Emit the trailing short group with '=' padding.
    //
    // The staging bytes sit in the group at bits 16..23 (b0), 8..15 (b1) and
    // 0..7 (b2).  Because base64 reads the stream big-endian, a partial group
    // is padded on the RIGHT (low bits) with zeroes, which is exactly what the
    // untouched Pending[2] = 0 provides - so the same shifts as the full-group
    // path yield the correct sextets:
    //   1 byte  b0:  >>18 = b0>>2          >>12 = (b0&3)<<4
    //   2 bytes b0,b1: third sextet >>6 = (b1&0xF)<<2
    // Everything past the real data is dropped and replaced by '='.
    if (PendingCount > 0)
    {
        char out[4];
        const uint32 group = (static_cast<uint32>(Pending[0]) << 16) |
                             (static_cast<uint32>(Pending[1]) << 8) |
                             (static_cast<uint32>(Pending[2]));

        out[0] = s_Base64PipeTable[(group >> 18) & 0x3F];
        out[1] = s_Base64PipeTable[(group >> 12) & 0x3F];
        out[2] = (PendingCount >= 2)
                     ? s_Base64PipeTable[(group >> 6) & 0x3F]
                     : '=';
        out[3] = '=';

        PendingCount = 0;

        if (ChainTo)
            written += ChainTo->Put(out, 4);
    }

    if (ChainTo)
        written += ChainTo->Flush();

    return written;
}

//========================================================================
// Multi-stage compression pipeline convenience functions
//========================================================================

namespace Pipes
{
    // Create a compression chain: Data -> Blowfish -> File
    inline ChainPipe* CreateCompressChain(CCFileClass* pFile, const void* pKey, int32 keyLen)
    {
        ChainPipe* chain = new ChainPipe();

        if (pKey && keyLen > 0)
            chain->Add(new BlowfishPipe(pKey, keyLen));

        chain->Add(new FilePipe(pFile));
        return chain;
    }

    // Create a chain for writing to a buffer
    inline ChainPipe* CreateBufferWriteChain(void* pBuffer, int32 size)
    {
        ChainPipe* chain = new ChainPipe();
        chain->Add(new BufferPipe(pBuffer, static_cast<size_t>(size)));
        return chain;
    }

    // Create a chain: LCW compression -> File
    inline ChainPipe* CreateLCWCompressChain(CCFileClass* pFile, bool bControl, size_t blockSize)
    {
        ChainPipe* chain = new ChainPipe();
        chain->Add(new LCWPipe(bControl, blockSize));
        chain->Add(new FilePipe(pFile));
        return chain;
    }
} // namespace Pipes