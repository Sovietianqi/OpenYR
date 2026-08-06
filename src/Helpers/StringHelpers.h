#pragma once

// ============================================================================
// StringHelpers.h - String manipulation utilities
//
//  Provides safe, portable string helpers used across the engine for INI
//  parsing, path handling, tokenization and case-insensitive comparisons.
//  All functions are designed to work without exceptions and without the
//  C++ standard library's string class, mirroring the original binary's
//  C-style string handling.
// ============================================================================

#include <Core/Definitions.h>
#include <Core/Macros.h>

#include <cstring>
#include <cctype>
#include <cstdlib>

// ============================================================================
// Case-insensitive comparison
// ============================================================================

namespace StringHelpers
{
    // Case-insensitive string compare.  Returns 0 if equal, <0 if a < b,
    // >0 if a > b.
    inline int32 CompareNoCase(const char* a, const char* b) noexcept
    {
        if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
        while (*a && *b)
        {
            int32 ca = std::tolower(static_cast<uint8>(*a));
            int32 cb = std::tolower(static_cast<uint8>(*b));
            if (ca != cb) return ca - cb;
            ++a; ++b;
        }
        return static_cast<int32>(static_cast<uint8>(*a))
             - static_cast<int32>(static_cast<uint8>(*b));
    }

    // Case-insensitive string compare with length limit.
    inline int32 CompareNoCaseN(const char* a, const char* b, int32 n) noexcept
    {
        if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
        for (int32 i = 0; i < n; ++i)
        {
            if (a[i] == '\0' || b[i] == '\0')
                return static_cast<int32>(static_cast<uint8>(a[i]))
                     - static_cast<int32>(static_cast<uint8>(b[i]));
            int32 ca = std::tolower(static_cast<uint8>(a[i]));
            int32 cb = std::tolower(static_cast<uint8>(b[i]));
            if (ca != cb) return ca - cb;
        }
        return 0;
    }

    // True if two strings are equal (case-insensitive).
    inline bool EqualsNoCase(const char* a, const char* b) noexcept
    {
        return CompareNoCase(a, b) == 0;
    }

    // ============================================================================
    // Copy / length helpers
    // ============================================================================

    // Safe string copy with truncation.  Always null-terminates the
    // destination.  Returns the number of characters written (excluding the
    // terminator).
    inline int32 Copy(char* dst, int32 dstSize, const char* src) noexcept
    {
        if (!dst || dstSize <= 0) return 0;
        if (!src) { dst[0] = '\0'; return 0; }
        int32 i = 0;
        while (src[i] && i < dstSize - 1)
        {
            dst[i] = src[i];
            ++i;
        }
        dst[i] = '\0';
        return i;
    }

    // Safe string append.  Returns the new string length.
    inline int32 Append(char* dst, int32 dstSize, const char* src) noexcept
    {
        if (!dst || dstSize <= 0) return 0;
        int32 len = 0;
        while (dst[len] && len < dstSize) ++len;
        if (!src || len >= dstSize - 1) return len;
        int32 i = 0;
        while (src[i] && len + i < dstSize - 1)
        {
            dst[len + i] = src[i];
            ++i;
        }
        dst[len + i] = '\0';
        return len + i;
    }

    // ============================================================================
    // Whitespace helpers
    // ============================================================================

    // Returns true if the character is whitespace.
    inline bool IsWhitespace(char c) noexcept
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
    }

    // Skip leading whitespace, returns a pointer to the first non-ws char.
    inline const char* SkipWhitespace(const char* s) noexcept
    {
        if (!s) return nullptr;
        while (*s && IsWhitespace(*s)) ++s;
        return s;
    }

    // Trim trailing whitespace in-place by writing a null terminator.
    inline void TrimRight(char* s) noexcept
    {
        if (!s) return;
        int32 len = 0;
        while (s[len]) ++len;
        while (len > 0 && IsWhitespace(s[len - 1]))
        {
            s[len - 1] = '\0';
            --len;
        }
    }

    // Trim both ends: returns a pointer to the first non-ws char and
    // null-terminates after the last non-ws char.
    inline const char* Trim(char* s) noexcept
    {
        if (!s) return nullptr;
        const char* start = SkipWhitespace(s);
        if (!*start) { s[0] = '\0'; return s; }
        // If start moved, shift the string back.
        if (start != s)
        {
            int32 i = 0;
            while (start[i]) { s[i] = start[i]; ++i; }
            s[i] = '\0';
        }
        TrimRight(s);
        return s;
    }

    // ============================================================================
    // Tokenization
    // ============================================================================

    // Extract the next token from a comma/space-separated list.
    // On input, *cursor points to the current position in the string.
    // On output, *cursor is advanced past the extracted token.
    // The token is written to `out` (up to outSize-1 chars + null).
    // Returns true if a token was extracted, false at end of string.
    inline bool NextToken(const char** cursor, char* out, int32 outSize,
                          char delimiter = ',') noexcept
    {
        if (!cursor || !*cursor || !out || outSize <= 0)
            return false;
        const char* s = *cursor;
        // Skip leading whitespace and delimiters.
        while (*s && (IsWhitespace(*s) || *s == delimiter)) ++s;
        if (!*s)
        {
            out[0] = '\0';
            *cursor = s;
            return false;
        }
        // Extract the token.
        int32 i = 0;
        while (*s && *s != delimiter && !IsWhitespace(*s) && i < outSize - 1)
        {
            out[i] = *s;
            ++i; ++s;
        }
        out[i] = '\0';
        // Skip trailing whitespace/delimiters so the next call starts clean.
        while (*s && (IsWhitespace(*s) || *s == delimiter)) ++s;
        *cursor = s;
        return true;
    }

    // Count the number of tokens in a delimited string.
    inline int32 CountTokens(const char* s, char delimiter = ',') noexcept
    {
        if (!s || !*s) return 0;
        int32 count = 0;
        const char* cursor = s;
        char buf[1];
        while (NextToken(&cursor, buf, 1, delimiter))
            ++count;
        return count;
    }

    // ============================================================================
    // Case conversion
    // ============================================================================

    // Convert a string to lowercase in-place.
    inline void ToLower(char* s) noexcept
    {
        if (!s) return;
        while (*s) { *s = static_cast<char>(std::tolower(static_cast<uint8>(*s))); ++s; }
    }

    // Convert a string to uppercase in-place.
    inline void ToUpper(char* s) noexcept
    {
        if (!s) return;
        while (*s) { *s = static_cast<char>(std::toupper(static_cast<uint8>(*s))); ++s; }
    }

    // ============================================================================
    // Integer / float parsing
    // ============================================================================

    // Parse an integer with a default fallback.
    inline int32 ToInt(const char* s, int32 defaultValue = 0) noexcept
    {
        if (!s || !*s) return defaultValue;
        return static_cast<int32>(std::strtol(s, nullptr, 10));
    }

    // Parse a float with a default fallback.
    inline float ToFloat(const char* s, float defaultValue = 0.0f) noexcept
    {
        if (!s || !*s) return defaultValue;
        return static_cast<float>(std::strtod(s, nullptr));
    }

    // Parse a double with a default fallback.
    inline double ToDouble(const char* s, double defaultValue = 0.0) noexcept
    {
        if (!s || !*s) return defaultValue;
        return std::strtod(s, nullptr);
    }

    // Parse a boolean from yes/no, true/false, 1/0.
    inline bool ToBool(const char* s, bool defaultValue = false) noexcept
    {
        if (!s || !*s) return defaultValue;
        if (EqualsNoCase(s, "yes") || EqualsNoCase(s, "true") || EqualsNoCase(s, "1"))
            return true;
        if (EqualsNoCase(s, "no") || EqualsNoCase(s, "false") || EqualsNoCase(s, "0"))
            return false;
        return defaultValue;
    }

    // ============================================================================
    // Path helpers
    // ============================================================================

    // Returns a pointer to the filename portion of a path (after the last
    // slash or backslash).
    inline const char* GetFileName(const char* path) noexcept
    {
        if (!path) return nullptr;
        const char* result = path;
        for (const char* p = path; *p; ++p)
        {
            if (*p == '/' || *p == '\\')
                result = p + 1;
        }
        return result;
    }

    // Returns a pointer to the file extension (including the dot), or
    // nullptr if there is no extension.
    inline const char* GetExtension(const char* path) noexcept
    {
        if (!path) return nullptr;
        const char* ext = nullptr;
        const char* fname = GetFileName(path);
        for (const char* p = fname; *p; ++p)
        {
            if (*p == '.')
                ext = p;
        }
        // If the dot is the first character of the filename, it's not an
        // extension (e.g. ".gitignore").
        if (ext == fname)
            return nullptr;
        return ext;
    }
}
