#pragma once

// ============================================================================
// Hashing.h - Hash utility functions
//
//  Provides compile-time and runtime hash functions used by the engine for
//  string interning, INI section lookup, CRC checksums and the multiplayer
//  sync checks.
//
//  Key functions:
//    * FNV-1a 32/64-bit string hashes (compile-time and runtime)
//    * CRC32 computation
//    * Simple string-to-id hashing for INI keys
//    * Hash combining
// ============================================================================

#include <Core/Definitions.h>
#include <Core/Macros.h>

#include <cstdint>
#include <cstring>

// ============================================================================
// FNV-1a hash constants
// ============================================================================

namespace Hashing
{
    // ── FNV-1a 32-bit ───────────────────────────────────────────────────

    constexpr uint32 FNV32_OffsetBasis = 2166136261u;
    constexpr uint32 FNV32_Prime       = 16777619u;

    // Compile-time FNV-1a 32-bit hash of a null-terminated string.
    constexpr uint32 FNV1a32(const char* s, uint32 hash = FNV32_OffsetBasis) noexcept
    {
        return (s[0] == '\0')
            ? hash
            : FNV1a32(s + 1, (hash ^ static_cast<uint32>(static_cast<uint8>(s[0]))) * FNV32_Prime);
    }

    // Runtime FNV-1a 32-bit hash with explicit length.
    inline uint32 FNV1a32(const void* data, int32 length, uint32 hash = FNV32_OffsetBasis) noexcept
    {
        if (!data || length <= 0) return hash;
        const uint8* p = static_cast<const uint8*>(data);
        for (int32 i = 0; i < length; ++i)
        {
            hash ^= static_cast<uint32>(p[i]);
            hash *= FNV32_Prime;
        }
        return hash;
    }

    // Runtime FNV-1a 32-bit hash of a null-terminated string.
    inline uint32 FNV1a32String(const char* s) noexcept
    {
        if (!s) return FNV32_OffsetBasis;
        uint32 hash = FNV32_OffsetBasis;
        while (*s)
        {
            hash ^= static_cast<uint32>(static_cast<uint8>(*s));
            hash *= FNV32_Prime;
            ++s;
        }
        return hash;
    }

    // ── FNV-1a 64-bit ───────────────────────────────────────────────────

    constexpr uint64 FNV64_OffsetBasis = 14695981039346656037ull;
    constexpr uint64 FNV64_Prime       = 1099511628211ull;

    // Compile-time FNV-1a 64-bit hash of a null-terminated string.
    constexpr uint64 FNV1a64(const char* s, uint64 hash = FNV64_OffsetBasis) noexcept
    {
        return (s[0] == '\0')
            ? hash
            : FNV1a64(s + 1, (hash ^ static_cast<uint64>(static_cast<uint8>(s[0]))) * FNV64_Prime);
    }

    // Runtime FNV-1a 64-bit hash with explicit length.
    inline uint64 FNV1a64(const void* data, int32 length, uint64 hash = FNV64_OffsetBasis) noexcept
    {
        if (!data || length <= 0) return hash;
        const uint8* p = static_cast<const uint8*>(data);
        for (int32 i = 0; i < length; ++i)
        {
            hash ^= static_cast<uint64>(p[i]);
            hash *= FNV64_Prime;
        }
        return hash;
    }

    // ── CRC32 ───────────────────────────────────────────────────────────
    //
    //  Standard CRC-32 (polynomial 0xEDB88320) as used by ZIP / the game's
    //  save-file and multiplayer checksums.

    // Precomputed CRC32 lookup table (generated lazily on first use).
    inline uint32* CRC32Table() noexcept
    {
        static uint32 table[256];
        static bool initialized = false;
        if (!initialized)
        {
            for (uint32 i = 0; i < 256; ++i)
            {
                uint32 c = i;
                for (int32 k = 0; k < 8; ++k)
                    c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                table[i] = c;
            }
            initialized = true;
        }
        return table;
    }

    // Compute CRC32 of a data block.
    inline uint32 CRC32(const void* data, int32 length, uint32 crc = 0) noexcept
    {
        if (!data || length <= 0) return crc;
        crc = ~crc;
        const uint8* p = static_cast<const uint8*>(data);
        uint32* table = CRC32Table();
        for (int32 i = 0; i < length; ++i)
            crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
        return ~crc;
    }

    // Compute CRC32 of a null-terminated string.
    inline uint32 CRC32String(const char* s, uint32 crc = 0) noexcept
    {
        if (!s) return crc;
        return CRC32(s, static_cast<int32>(std::strlen(s)), crc);
    }

    // ── String-to-ID hashing ────────────────────────────────────────────
    //
    //  The engine maps INI section/key names to integer IDs for fast
    //  lookup.  This is a simple case-insensitive hash that fits in an
    //  int32 and is stable across runs.

    inline int32 StringToID(const char* s) noexcept
    {
        if (!s) return 0;
        uint32 hash = FNV32_OffsetBasis;
        while (*s)
        {
            // Case-insensitive: convert to lowercase before hashing.
            uint32 c = static_cast<uint8>(*s);
            if (c >= 'A' && c <= 'Z') c += 32;
            hash ^= c;
            hash *= FNV32_Prime;
            ++s;
        }
        return static_cast<int32>(hash);
    }

    // ── Hash combining ──────────────────────────────────────────────────
    //
    //  Combine two hash values (e.g. for multi-field struct hashing).
    //  Uses the boost::hash_combine formula.

    inline uint32 Combine32(uint32 seed, uint32 value) noexcept
    {
        return seed ^ (value + 0x9E3779B9u + (seed << 6) + (seed >> 2));
    }

    inline uint64 Combine64(uint64 seed, uint64 value) noexcept
    {
        return seed ^ (value + 0x9E3779B97F4A7C15ull + (seed << 6) + (seed >> 2));
    }

    // ── Compile-time string literal operator ────────────────────────────
    //
    //  Allows "string"_fnv1a to produce a compile-time uint32 hash.

    constexpr uint32 operator""_fnv1a(const char* s, std::size_t) noexcept
    {
        return FNV1a32(s);
    }
}
