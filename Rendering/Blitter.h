#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Rendering/Surface.h"

#include <algorithm>
#include <cstring>

// ============================================================================
// Blitter - Base class for all blitters
//
// Blitters are the core pixel-copying engines of the C&C rendering pipeline.
// They handle palette-based blitting with color remapping, translucency,
// alpha blending, z-buffer comparison, and RLE decompression.
//
// The vt_Blitter virtual table is constructed at runtime; each blitter is
// selected based on the combination of BlitterFlags.
// ============================================================================

// Forward declarations
class AlphaLightingRemapClass;

// ============================================================================
// AlphaLightingRemapClass - Pre-computed alpha lookup tables
// ============================================================================
class AlphaLightingRemapClass
{
public:
    static constexpr int32 MaxLevel = 255;

    AlphaLightingRemapClass()
    {
        memset(Table, 0, sizeof(Table));
    }

    void BuildTables()
    {
        for (int32 level = 0; level < MaxLevel; ++level)
        {
            for (int32 i = 0; i < 256; ++i)
            {
                int32 r = (i >> 5) & 0x07;
                int32 g = (i >> 2) & 0x07;
                int32 b = i & 0x03;
                int32 adj = (level * 256) / MaxLevel;
                r = (r * adj) / 256;
                g = (g * adj) / 256;
                b = (b * adj) / 256;
                Table[level][i] = static_cast<uint16>((r << 11) | (g << 5) | b);
            }
        }
    }

    uint16 Table[MaxLevel][256];
};

// ============================================================================
// Blitter - Plain (non-RLE) blitter base class
// ============================================================================
class Blitter
{
public:
    virtual ~Blitter() = default;
    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) = 0;
    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) = 0;
    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) = 0;
    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) = 0;

protected:
    static uint16* Lookup_Alpha_Remapper(int32 alvl, AlphaLightingRemapClass* remapper)
    {
        if (!remapper) return nullptr;
        int32 level = std::min(254, (261 * std::max(0, alvl)) >> 11);
        return remapper->Table[level];
    }
};

// ============================================================================
// RLEBlitter - RLE-compressed blitter base class
// ============================================================================
class RLEBlitter
{
public:
    virtual ~RLEBlitter() = default;
    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 line,
                           int32 zbase, uint16* zbuf, uint16* abuf,
                           int32 alvl, int32 warp, BYTE* zadjust) = 0;
    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 line,
                                  int32 zbase, uint16* zbuf, uint16* abuf,
                                  int32 alvl, int32 warp, BYTE* zadjust,
                                  uint16 tint) = 0;

protected:
    static uint16* Lookup_Alpha_Remapper(int32 alvl, AlphaLightingRemapClass* remapper)
    {
        if (!remapper) return nullptr;
        int32 level = std::min(254, (261 * std::max(0, alvl)) >> 11);
        return remapper->Table[level];
    }

    // RLE pre-line processing: skip lines before the current one
    template<bool UseZBuffer, bool UseABuffer, typename T>
    static void Process_Pre_Lines(T*& dest, BYTE*& src, int32& len,
                                  const int32& line, uint16*& zbuf, uint16*& abuf)
    {
        if (line > 0)
        {
            int32 off = -line;
            do
            {
                if (*src++)
                    ++off;
                else
                    off += *src++;
            }
            while (off < 0);

            dest += off;
            len -= off;

            if constexpr (UseZBuffer)
                zbuf += off;
            if constexpr (UseABuffer)
                abuf += off;
        }
    }

    // RLE pixel data processing with skip runs
    template<bool UseZBuffer, bool UseABuffer, typename T, typename Fn>
    static void Process_Pixel_Datas(T* dest, BYTE* src, int32 len,
                                    int32 zbase, uint16*& zbuf, uint16*& abuf,
                                    BYTE*& zadjust, Fn f)
    {
        if (len < 0) return;

        while (len > 0)
        {
            BYTE srcv = *src++;
            if (srcv)
            {
                if constexpr (UseZBuffer && UseABuffer)
                    f(*dest, srcv, zbase, *zbuf++, *zadjust++, *abuf++);
                else if constexpr (UseZBuffer && !UseABuffer)
                    f(*dest, srcv, zbase, *zbuf++, *zadjust++);
                else if constexpr (!UseZBuffer && UseABuffer)
                    f(*dest, srcv, *abuf++);
                else
                    f(*dest, srcv);

                ++dest;
                --len;
            }
            else
            {
                BYTE off = *src++;
                len -= off;
                dest += off;

                if constexpr (UseZBuffer)
                {
                    zbuf += off;
                    zadjust += off;
                }
                if constexpr (UseABuffer)
                    abuf += off;
            }
        }
    }
};

// ============================================================================
// Lookup table helpers for translucency blending
// ============================================================================
namespace BlitterTables
{
    // Pre-computed 25% translucent blending table
    // Result = 0.25 * src + 0.75 * dst
    inline void InitTranslucent25Table(uint16* table, uint16 mask)
    {
        for (int32 i = 0; i < 256; ++i)
        {
            table[i] = static_cast<uint16>((mask & static_cast<uint16>(i * 0x0101) >> 2))
                     + 3 * static_cast<uint16>(mask & static_cast<uint16>(i * 0x0101) >> 2);
        }
    }

    // 25% blend: (mask & (dst >> 2)) + 3 * (mask & (src >> 2))
    inline uint16 Blend25(uint16 dst, uint16 src, uint16 mask)
    {
        return static_cast<uint16>((mask & (dst >> 2)) + 3 * (mask & (src >> 2)));
    }

    // 50% blend: (mask & (dst >> 1)) + (mask & (src >> 1))
    inline uint16 Blend50(uint16 dst, uint16 src, uint16 mask)
    {
        return static_cast<uint16>((mask & (dst >> 1)) + (mask & (src >> 1)));
    }

    // 75% blend: 3 * (mask & (dst >> 2)) + (mask & (src >> 2))
    inline uint16 Blend75(uint16 dst, uint16 src, uint16 mask)
    {
        return static_cast<uint16>(3 * (mask & (dst >> 2)) + (mask & (src >> 2)));
    }

    // Darken: multiply dst by src factor
    inline uint16 Darken(uint16 dst, uint16 src, uint16 mask)
    {
        uint16 d = dst & mask;
        uint16 s = src & mask;
        uint16 r = static_cast<uint16>((d * s) >> 8);
        return r & mask;
    }
}

// ============================================================================
// Tint structure
// ============================================================================
struct TintStruct
{
    int32 Red;
    int32 Green;
    int32 Blue;

    TintStruct() : Red(0), Green(0), Blue(0) {}
    TintStruct(int32 r, int32 g, int32 b) : Red(r), Green(g), Blue(b) {}
};

// ============================================================================
// Blitter Virtual Table Structure
// ============================================================================
struct vt_Blitter
{
    void* dtor;
    void* Blit_Copy;
    void* Blit_Copy_Tinted;
    void* Blit_Move;
    void* Blit_Move_Tinted;
};

struct vt_RLEBlitter
{
    void* dtor;
    void* Blit_Copy;
    void* Blit_Copy_Tinted;
};

// ============================================================================
// Macro to define blitter classes
// ============================================================================
#define DEFINE_BLITTER(x) \
template<typename T> \
class x final : public Blitter

#define DEFINE_RLE_BLITTER(x) \
template<typename T> \
class x final : public RLEBlitter

// ============================================================================
// PLAIN BLITTERS (no translucency, no z-buffer)
// ============================================================================

// --- BlitPlainXlat ---
// Simple palette translation: output = palette[src]
DEFINE_BLITTER(BlitPlainXlat)
{
public:
    explicit BlitPlainXlat(T* data) noexcept : PaletteData(data) {}
    virtual ~BlitPlainXlat() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        while (len--)
        {
            BYTE idx = *src++;
            if (idx) *dest = PaletteData[idx];
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
};

// --- BlitTransXlat ---
// Palette translation with color key transparency (index 0 = transparent)
DEFINE_BLITTER(BlitTransXlat)
{
public:
    explicit BlitTransXlat(T* data) noexcept : PaletteData(data) {}
    virtual ~BlitTransXlat() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        while (len--)
        {
            BYTE idx = *src++;
            if (idx) *dest = PaletteData[idx];
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
};

// --- BlitTransRemapXlat ---
// Palette translation with remap table and color key transparency
DEFINE_BLITTER(BlitTransRemapXlat)
{
public:
    explicit BlitTransRemapXlat(T* data, BYTE* remap) noexcept
        : PaletteData(data), RemapTable(remap) {}
    virtual ~BlitTransRemapXlat() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0 || !RemapTable) return;
        auto dest = reinterpret_cast<T*>(dst);
        while (len--)
        {
            BYTE idx = *src++;
            if (idx) *dest = PaletteData[RemapTable[idx]];
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    BYTE* RemapTable;
};

// --- BlitTransZRemapXlat ---
// Palette translation with Z gradient remap and color key transparency
DEFINE_BLITTER(BlitTransZRemapXlat)
{
public:
    explicit BlitTransZRemapXlat(T* data, BYTE** zRemap) noexcept
        : PaletteData(data), ZRemapTables(zRemap) {}
    virtual ~BlitTransZRemapXlat() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        BYTE* zrmp = (ZRemapTables && zval >= 0) ? ZRemapTables[zval] : nullptr;

        while (len--)
        {
            BYTE idx = *src++;
            if (idx)
            {
                if (zrmp) idx = zrmp[idx];
                *dest = PaletteData[idx];
            }
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    BYTE** ZRemapTables;
};

// ============================================================================
// TRANSLUCENT BLITTERS (25%, 50%, 75% opacity)
// ============================================================================

// --- BlitTransLucent25 ---
// 25% opaque: 3/4 dst + 1/4 src
DEFINE_BLITTER(BlitTransLucent25)
{
public:
    explicit BlitTransLucent25(T* data, uint16 mask) noexcept
        : PaletteData(data), Mask(mask) {}
    virtual ~BlitTransLucent25() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        while (len--)
        {
            BYTE idx = *src++;
            if (idx)
                *dest = static_cast<T>(BlitterTables::Blend25(*dest, PaletteData[idx], Mask));
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    uint16 Mask;
};

// --- BlitTransLucent50 ---
// 50% opaque: 1/2 dst + 1/2 src
DEFINE_BLITTER(BlitTransLucent50)
{
public:
    explicit BlitTransLucent50(T* data, uint16 mask) noexcept
        : PaletteData(data), Mask(mask) {}
    virtual ~BlitTransLucent50() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        while (len--)
        {
            BYTE idx = *src++;
            if (idx)
                *dest = static_cast<T>(BlitterTables::Blend50(*dest, PaletteData[idx], Mask));
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    uint16 Mask;
};

// --- BlitTransLucent75 ---
// 75% opaque: 1/4 dst + 3/4 src
DEFINE_BLITTER(BlitTransLucent75)
{
public:
    explicit BlitTransLucent75(T* data, uint16 mask) noexcept
        : PaletteData(data), Mask(mask) {}
    virtual ~BlitTransLucent75() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        while (len--)
        {
            BYTE idx = *src++;
            if (idx)
                *dest = static_cast<T>(BlitterTables::Blend75(*dest, PaletteData[idx], Mask));
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    uint16 Mask;
};

// --- BlitTransDarken ---
// Darken: multiply dst by src factor
DEFINE_BLITTER(BlitTransDarken)
{
public:
    explicit BlitTransDarken(T* data, uint16 mask) noexcept
        : PaletteData(data), Mask(mask) {}
    virtual ~BlitTransDarken() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        while (len--)
        {
            BYTE idx = *src++;
            if (idx)
                *dest = static_cast<T>(BlitterTables::Darken(*dest, PaletteData[idx], Mask));
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    uint16 Mask;
};

// ============================================================================
// ALPHA BLITTERS (with alpha blending)
// ============================================================================

// --- BlitTransXlatAlpha ---
// Palette translation with alpha blending
DEFINE_BLITTER(BlitTransXlatAlpha)
{
public:
    explicit BlitTransXlatAlpha(T* data, AlphaLightingRemapClass* alphaRemap) noexcept
        : PaletteData(data), AlphaRemap(alphaRemap) {}
    virtual ~BlitTransXlatAlpha() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16* alphaTable = Lookup_Alpha_Remapper(alvl, AlphaRemap);

        while (len--)
        {
            BYTE idx = *src++;
            if (idx)
            {
                uint16 srcColor = PaletteData[idx];
                if (alphaTable && abuf)
                {
                    uint16 alpha = *abuf++;
                    uint16 blended = alphaTable[alpha & 0xFF];
                    *dest = static_cast<T>(BlitterTables::Blend50(*dest, srcColor, blended));
                }
                else
                {
                    *dest = srcColor;
                }
            }
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    AlphaLightingRemapClass* AlphaRemap;
};

// --- BlitTransLucent25Alpha ---
// 25% translucent with alpha
DEFINE_BLITTER(BlitTransLucent25Alpha)
{
public:
    explicit BlitTransLucent25Alpha(T* data, uint16 mask, AlphaLightingRemapClass* alphaRemap) noexcept
        : PaletteData(data), Mask(mask), AlphaRemap(alphaRemap) {}
    virtual ~BlitTransLucent25Alpha() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16* alphaTable = Lookup_Alpha_Remapper(alvl, AlphaRemap);

        while (len--)
        {
            BYTE idx = *src++;
            if (idx)
            {
                uint16 srcColor = PaletteData[idx];
                uint16 dstColor = *dest;
                uint16 blended = dstColor;
                if (alphaTable && abuf)
                {
                    uint16 alpha = *abuf++;
                    uint16 alphaMask = alphaTable[alpha & 0xFF];
                    srcColor = BlitterTables::Blend50(srcColor, alphaMask, Mask);
                }
                *dest = static_cast<T>(BlitterTables::Blend25(dstColor, srcColor, Mask));
            }
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    uint16 Mask;
    AlphaLightingRemapClass* AlphaRemap;
};

// --- BlitTransLucent50Alpha ---
// 50% translucent with alpha
DEFINE_BLITTER(BlitTransLucent50Alpha)
{
public:
    explicit BlitTransLucent50Alpha(T* data, uint16 mask, AlphaLightingRemapClass* alphaRemap) noexcept
        : PaletteData(data), Mask(mask), AlphaRemap(alphaRemap) {}
    virtual ~BlitTransLucent50Alpha() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16* alphaTable = Lookup_Alpha_Remapper(alvl, AlphaRemap);

        while (len--)
        {
            BYTE idx = *src++;
            if (idx)
            {
                uint16 srcColor = PaletteData[idx];
                if (alphaTable && abuf)
                {
                    uint16 alpha = *abuf++;
                    uint16 alphaMask = alphaTable[alpha & 0xFF];
                    srcColor = BlitterTables::Blend50(srcColor, alphaMask, Mask);
                }
                *dest = static_cast<T>(BlitterTables::Blend50(*dest, srcColor, Mask));
            }
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    uint16 Mask;
    AlphaLightingRemapClass* AlphaRemap;
};

// --- BlitTransLucent75Alpha ---
// 75% translucent with alpha
DEFINE_BLITTER(BlitTransLucent75Alpha)
{
public:
    explicit BlitTransLucent75Alpha(T* data, uint16 mask, AlphaLightingRemapClass* alphaRemap) noexcept
        : PaletteData(data), Mask(mask), AlphaRemap(alphaRemap) {}
    virtual ~BlitTransLucent75Alpha() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16* alphaTable = Lookup_Alpha_Remapper(alvl, AlphaRemap);

        while (len--)
        {
            BYTE idx = *src++;
            if (idx)
            {
                uint16 srcColor = PaletteData[idx];
                if (alphaTable && abuf)
                {
                    uint16 alpha = *abuf++;
                    uint16 alphaMask = alphaTable[alpha & 0xFF];
                    srcColor = BlitterTables::Blend50(srcColor, alphaMask, Mask);
                }
                *dest = static_cast<T>(BlitterTables::Blend75(*dest, srcColor, Mask));
            }
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    uint16 Mask;
    AlphaLightingRemapClass* AlphaRemap;
};

// ============================================================================
// Z-READ BLITTERS (with z-buffer comparison)
// ============================================================================

// --- BlitTransXlatZRead ---
// Palette translation with z-buffer read comparison
DEFINE_BLITTER(BlitTransXlatZRead)
{
public:
    explicit BlitTransXlatZRead(T* data) noexcept : PaletteData(data) {}
    virtual ~BlitTransXlatZRead() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16 zVal = static_cast<uint16>(zval);

        while (len--)
        {
            BYTE idx = *src++;
            if (idx && zbuf)
            {
                if (zVal >= *zbuf)
                    *dest = PaletteData[idx];
            }
            ++dest;
            if (zbuf) ++zbuf;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
};

// --- BlitTransLucent25ZRead ---
// 25% translucent with z-buffer read
DEFINE_BLITTER(BlitTransLucent25ZRead)
{
public:
    explicit BlitTransLucent25ZRead(T* data, uint16 mask) noexcept
        : PaletteData(data), Mask(mask) {}
    virtual ~BlitTransLucent25ZRead() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16 zVal = static_cast<uint16>(zval);

        while (len--)
        {
            BYTE idx = *src++;
            if (idx && zbuf && zVal >= *zbuf)
                *dest = static_cast<T>(BlitterTables::Blend25(*dest, PaletteData[idx], Mask));
            ++dest;
            if (zbuf) ++zbuf;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    uint16 Mask;
};

// --- BlitTransLucent50ZRead ---
// 50% translucent with z-buffer read
DEFINE_BLITTER(BlitTransLucent50ZRead)
{
public:
    explicit BlitTransLucent50ZRead(T* data, uint16 mask) noexcept
        : PaletteData(data), Mask(mask) {}
    virtual ~BlitTransLucent50ZRead() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16 zVal = static_cast<uint16>(zval);

        while (len--)
        {
            BYTE idx = *src++;
            if (idx && zbuf && zVal >= *zbuf)
                *dest = static_cast<T>(BlitterTables::Blend50(*dest, PaletteData[idx], Mask));
            ++dest;
            if (zbuf) ++zbuf;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    uint16 Mask;
};

// --- BlitTransLucent75ZRead ---
// 75% translucent with z-buffer read
DEFINE_BLITTER(BlitTransLucent75ZRead)
{
public:
    explicit BlitTransLucent75ZRead(T* data, uint16 mask) noexcept
        : PaletteData(data), Mask(mask) {}
    virtual ~BlitTransLucent75ZRead() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16 zVal = static_cast<uint16>(zval);

        while (len--)
        {
            BYTE idx = *src++;
            if (idx && zbuf && zVal >= *zbuf)
                *dest = static_cast<T>(BlitterTables::Blend75(*dest, PaletteData[idx], Mask));
            ++dest;
            if (zbuf) ++zbuf;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    uint16 Mask;
};

// ============================================================================
// Z-READ-WRITE BLITTERS (z-buffer read and write)
// ============================================================================

// --- BlitTransXlatZReadWrite ---
// Palette translation with z-buffer read and write
DEFINE_BLITTER(BlitTransXlatZReadWrite)
{
public:
    explicit BlitTransXlatZReadWrite(T* data) noexcept : PaletteData(data) {}
    virtual ~BlitTransXlatZReadWrite() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16 zVal = static_cast<uint16>(zval);

        while (len--)
        {
            BYTE idx = *src++;
            if (idx && zbuf)
            {
                if (zVal >= *zbuf)
                {
                    *dest = PaletteData[idx];
                    *zbuf = zVal;
                }
            }
            ++dest;
            if (zbuf) ++zbuf;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
};

// ============================================================================
// RLE BLITTERS (Run-Length Encoded compressed blits)
// ============================================================================

// --- RLEBlitTransLucent25 ---
// RLE-compressed 25% translucent
DEFINE_RLE_BLITTER(RLEBlitTransLucent25)
{
public:
    explicit RLEBlitTransLucent25(T* data, uint16 mask) noexcept
        : PaletteData(data), Mask(mask) {}
    virtual ~RLEBlitTransLucent25() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 line,
                           int32 zbase, uint16* zbuf, uint16* abuf,
                           int32 alvl, int32 warp, BYTE* zadjust) override final
    {
        auto dest = reinterpret_cast<T*>(dst);
        Process_Pre_Lines<false, false>(dest, src, len, line, zbuf, abuf);

        auto handler = [this](T& d, BYTE s)
        {
            d = static_cast<T>(BlitterTables::Blend25(d, PaletteData[s], Mask));
        };

        Process_Pixel_Datas<false, false>(dest, src, len, zbase, zbuf, abuf, zadjust, handler);
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 line,
                                  int32 zbase, uint16* zbuf, uint16* abuf,
                                  int32 alvl, int32 warp, BYTE* zadjust,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, line, zbase, zbuf, abuf, alvl, warp, zadjust);
    }

private:
    T* PaletteData;
    uint16 Mask;
};

// --- RLEBlitTransLucent50 ---
// RLE-compressed 50% translucent
DEFINE_RLE_BLITTER(RLEBlitTransLucent50)
{
public:
    explicit RLEBlitTransLucent50(T* data, uint16 mask) noexcept
        : PaletteData(data), Mask(mask) {}
    virtual ~RLEBlitTransLucent50() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 line,
                           int32 zbase, uint16* zbuf, uint16* abuf,
                           int32 alvl, int32 warp, BYTE* zadjust) override final
    {
        auto dest = reinterpret_cast<T*>(dst);
        Process_Pre_Lines<false, false>(dest, src, len, line, zbuf, abuf);

        auto handler = [this](T& d, BYTE s)
        {
            d = static_cast<T>(BlitterTables::Blend50(d, PaletteData[s], Mask));
        };

        Process_Pixel_Datas<false, false>(dest, src, len, zbase, zbuf, abuf, zadjust, handler);
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 line,
                                  int32 zbase, uint16* zbuf, uint16* abuf,
                                  int32 alvl, int32 warp, BYTE* zadjust,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, line, zbase, zbuf, abuf, alvl, warp, zadjust);
    }

private:
    T* PaletteData;
    uint16 Mask;
};

// --- RLEBlitTransLucent75 ---
// RLE-compressed 75% translucent
DEFINE_RLE_BLITTER(RLEBlitTransLucent75)
{
public:
    explicit RLEBlitTransLucent75(T* data, uint16 mask) noexcept
        : PaletteData(data), Mask(mask) {}
    virtual ~RLEBlitTransLucent75() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 line,
                           int32 zbase, uint16* zbuf, uint16* abuf,
                           int32 alvl, int32 warp, BYTE* zadjust) override final
    {
        auto dest = reinterpret_cast<T*>(dst);
        Process_Pre_Lines<false, false>(dest, src, len, line, zbuf, abuf);

        auto handler = [this](T& d, BYTE s)
        {
            d = static_cast<T>(BlitterTables::Blend75(d, PaletteData[s], Mask));
        };

        Process_Pixel_Datas<false, false>(dest, src, len, zbase, zbuf, abuf, zadjust, handler);
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 line,
                                  int32 zbase, uint16* zbuf, uint16* abuf,
                                  int32 alvl, int32 warp, BYTE* zadjust,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, line, zbase, zbuf, abuf, alvl, warp, zadjust);
    }

private:
    T* PaletteData;
    uint16 Mask;
};

// --- RLEBlitTransDarken ---
// RLE-compressed darken
DEFINE_RLE_BLITTER(RLEBlitTransDarken)
{
public:
    explicit RLEBlitTransDarken(T* data, uint16 mask) noexcept
        : PaletteData(data), Mask(mask) {}
    virtual ~RLEBlitTransDarken() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 line,
                           int32 zbase, uint16* zbuf, uint16* abuf,
                           int32 alvl, int32 warp, BYTE* zadjust) override final
    {
        auto dest = reinterpret_cast<T*>(dst);
        Process_Pre_Lines<false, false>(dest, src, len, line, zbuf, abuf);

        auto handler = [this](T& d, BYTE s)
        {
            d = static_cast<T>(BlitterTables::Darken(d, PaletteData[s], Mask));
        };

        Process_Pixel_Datas<false, false>(dest, src, len, zbase, zbuf, abuf, zadjust, handler);
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 line,
                                  int32 zbase, uint16* zbuf, uint16* abuf,
                                  int32 alvl, int32 warp, BYTE* zadjust,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, line, zbase, zbuf, abuf, alvl, warp, zadjust);
    }

private:
    T* PaletteData;
    uint16 Mask;
};

// ============================================================================
// Additional Blitter variants (PlainXlatAlpha, ZRead variants, etc.)
// ============================================================================

// --- BlitPlainXlatAlpha ---
DEFINE_BLITTER(BlitPlainXlatAlpha)
{
public:
    explicit BlitPlainXlatAlpha(T* data, AlphaLightingRemapClass* alphaRemap) noexcept
        : PaletteData(data), AlphaRemap(alphaRemap) {}
    virtual ~BlitPlainXlatAlpha() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16* alphaTable = Lookup_Alpha_Remapper(alvl, AlphaRemap);

        while (len--)
        {
            BYTE idx = *src++;
            if (alphaTable && abuf)
            {
                uint16 alpha = *abuf++;
                *dest = alphaTable[(idx * 256 / 255) & 0xFF];
            }
            else
            {
                *dest = PaletteData[idx];
            }
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    AlphaLightingRemapClass* AlphaRemap;
};

// --- BlitPlainXlatZRead ---
DEFINE_BLITTER(BlitPlainXlatZRead)
{
public:
    explicit BlitPlainXlatZRead(T* data) noexcept : PaletteData(data) {}
    virtual ~BlitPlainXlatZRead() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16 zVal = static_cast<uint16>(zval);
        while (len--)
        {
            BYTE idx = *src++;
            if (zbuf && zVal >= *zbuf)
                *dest = PaletteData[idx];
            ++dest;
            if (zbuf) ++zbuf;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
};

// --- BlitPlainXlatZReadWrite ---
DEFINE_BLITTER(BlitPlainXlatZReadWrite)
{
public:
    explicit BlitPlainXlatZReadWrite(T* data) noexcept : PaletteData(data) {}
    virtual ~BlitPlainXlatZReadWrite() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16 zVal = static_cast<uint16>(zval);
        while (len--)
        {
            BYTE idx = *src++;
            if (zbuf && zVal >= *zbuf)
            {
                *dest = PaletteData[idx];
                *zbuf = zVal;
            }
            ++dest;
            if (zbuf) ++zbuf;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
};

// --- BlitTransDarkenZRead ---
DEFINE_BLITTER(BlitTransDarkenZRead)
{
public:
    explicit BlitTransDarkenZRead(T* data, uint16 mask) noexcept
        : PaletteData(data), Mask(mask) {}
    virtual ~BlitTransDarkenZRead() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16 zVal = static_cast<uint16>(zval);
        while (len--)
        {
            BYTE idx = *src++;
            if (idx && zbuf && zVal >= *zbuf)
                *dest = static_cast<T>(BlitterTables::Darken(*dest, PaletteData[idx], Mask));
            ++dest;
            if (zbuf) ++zbuf;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    uint16 Mask;
};

// --- BlitTransDarkenZReadWrite ---
DEFINE_BLITTER(BlitTransDarkenZReadWrite)
{
public:
    explicit BlitTransDarkenZReadWrite(T* data, uint16 mask) noexcept
        : PaletteData(data), Mask(mask) {}
    virtual ~BlitTransDarkenZReadWrite() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16 zVal = static_cast<uint16>(zval);
        while (len--)
        {
            BYTE idx = *src++;
            if (idx && zbuf && zVal >= *zbuf)
            {
                *dest = static_cast<T>(BlitterTables::Darken(*dest, PaletteData[idx], Mask));
                *zbuf = zVal;
            }
            ++dest;
            if (zbuf) ++zbuf;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    uint16 Mask;
};

// --- BlitTransRemapDest ---
// Remap destination blitter
DEFINE_BLITTER(BlitTransRemapDest)
{
public:
    explicit BlitTransRemapDest(T* data, BYTE* remap) noexcept
        : PaletteData(data), RemapTable(remap) {}
    virtual ~BlitTransRemapDest() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0 || !RemapTable) return;
        auto dest = reinterpret_cast<T*>(dst);
        while (len--)
        {
            BYTE idx = *src++;
            if (idx) *dest = PaletteData[RemapTable[idx]];
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp);
    }

private:
    T* PaletteData;
    BYTE* RemapTable;
};

// ============================================================================
// Additional RLE blitters
// ============================================================================

// --- RLEBlitTransXlat ---
DEFINE_RLE_BLITTER(RLEBlitTransXlat)
{
public:
    explicit RLEBlitTransXlat(T* data) noexcept : PaletteData(data) {}
    virtual ~RLEBlitTransXlat() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 line,
                           int32 zbase, uint16* zbuf, uint16* abuf,
                           int32 alvl, int32 warp, BYTE* zadjust) override final
    {
        auto dest = reinterpret_cast<T*>(dst);
        Process_Pre_Lines<false, false>(dest, src, len, line, zbuf, abuf);

        auto handler = [this](T& d, BYTE s) { d = PaletteData[s]; };
        Process_Pixel_Datas<false, false>(dest, src, len, zbase, zbuf, abuf, zadjust, handler);
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 line,
                                  int32 zbase, uint16* zbuf, uint16* abuf,
                                  int32 alvl, int32 warp, BYTE* zadjust,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, line, zbase, zbuf, abuf, alvl, warp, zadjust);
    }

private:
    T* PaletteData;
};

// --- RLEBlitTransXlatAlpha ---
DEFINE_RLE_BLITTER(RLEBlitTransXlatAlpha)
{
public:
    explicit RLEBlitTransXlatAlpha(T* data, AlphaLightingRemapClass* alphaRemap) noexcept
        : PaletteData(data), AlphaRemap(alphaRemap) {}
    virtual ~RLEBlitTransXlatAlpha() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 line,
                           int32 zbase, uint16* zbuf, uint16* abuf,
                           int32 alvl, int32 warp, BYTE* zadjust) override final
    {
        auto dest = reinterpret_cast<T*>(dst);
        uint16* alphaTable = Lookup_Alpha_Remapper(alvl, AlphaRemap);
        Process_Pre_Lines<false, false>(dest, src, len, line, zbuf, abuf);

        auto handler = [this, alphaTable](T& d, BYTE s)
        {
            uint16 srcColor = PaletteData[s];
            if (alphaTable)
                srcColor = BlitterTables::Blend50(srcColor, alphaTable[s], 0xFFFF);
            d = srcColor;
        };
        Process_Pixel_Datas<false, false>(dest, src, len, zbase, zbuf, abuf, zadjust, handler);
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 line,
                                  int32 zbase, uint16* zbuf, uint16* abuf,
                                  int32 alvl, int32 warp, BYTE* zadjust,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, line, zbase, zbuf, abuf, alvl, warp, zadjust);
    }

private:
    T* PaletteData;
    AlphaLightingRemapClass* AlphaRemap;
};

// --- RLEBlitTransXlatZRead ---
DEFINE_RLE_BLITTER(RLEBlitTransXlatZRead)
{
public:
    explicit RLEBlitTransXlatZRead(T* data) noexcept : PaletteData(data) {}
    virtual ~RLEBlitTransXlatZRead() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 line,
                           int32 zbase, uint16* zbuf, uint16* abuf,
                           int32 alvl, int32 warp, BYTE* zadjust) override final
    {
        auto dest = reinterpret_cast<T*>(dst);
        Process_Pre_Lines<true, false>(dest, src, len, line, zbuf, abuf);

        auto handler = [this, zbase](T& d, BYTE s, int32, uint16& z, BYTE& za)
        {
            if (static_cast<uint16>(zbase) >= z)
                d = PaletteData[s];
        };
        Process_Pixel_Datas<true, false>(dest, src, len, zbase, zbuf, abuf, zadjust, handler);
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 line,
                                  int32 zbase, uint16* zbuf, uint16* abuf,
                                  int32 alvl, int32 warp, BYTE* zadjust,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, line, zbase, zbuf, abuf, alvl, warp, zadjust);
    }

private:
    T* PaletteData;
};

// --- RLEBlitTransXlatZReadWrite ---
DEFINE_RLE_BLITTER(RLEBlitTransXlatZReadWrite)
{
public:
    explicit RLEBlitTransXlatZReadWrite(T* data) noexcept : PaletteData(data) {}
    virtual ~RLEBlitTransXlatZReadWrite() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 line,
                           int32 zbase, uint16* zbuf, uint16* abuf,
                           int32 alvl, int32 warp, BYTE* zadjust) override final
    {
        auto dest = reinterpret_cast<T*>(dst);
        Process_Pre_Lines<true, false>(dest, src, len, line, zbuf, abuf);

        auto handler = [this, zbase](T& d, BYTE s, int32, uint16& z, BYTE& za)
        {
            if (static_cast<uint16>(zbase) >= z)
            {
                d = PaletteData[s];
                z = static_cast<uint16>(zbase);
            }
        };
        Process_Pixel_Datas<true, false>(dest, src, len, zbase, zbuf, abuf, zadjust, handler);
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 line,
                                  int32 zbase, uint16* zbuf, uint16* abuf,
                                  int32 alvl, int32 warp, BYTE* zadjust,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, line, zbase, zbuf, abuf, alvl, warp, zadjust);
    }

private:
    T* PaletteData;
};

// --- RLEBlitTransRemapXlat ---
DEFINE_RLE_BLITTER(RLEBlitTransRemapXlat)
{
public:
    explicit RLEBlitTransRemapXlat(T* data, BYTE* remap) noexcept
        : PaletteData(data), RemapTable(remap) {}
    virtual ~RLEBlitTransRemapXlat() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 line,
                           int32 zbase, uint16* zbuf, uint16* abuf,
                           int32 alvl, int32 warp, BYTE* zadjust) override final
    {
        auto dest = reinterpret_cast<T*>(dst);
        Process_Pre_Lines<false, false>(dest, src, len, line, zbuf, abuf);

        auto handler = [this](T& d, BYTE s)
        {
            if (RemapTable) d = PaletteData[RemapTable[s]];
        };
        Process_Pixel_Datas<false, false>(dest, src, len, zbase, zbuf, abuf, zadjust, handler);
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 line,
                                  int32 zbase, uint16* zbuf, uint16* abuf,
                                  int32 alvl, int32 warp, BYTE* zadjust,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, line, zbase, zbuf, abuf, alvl, warp, zadjust);
    }

private:
    T* PaletteData;
    BYTE* RemapTable;
};

// --- RLEBlitTransRemapDest ---
DEFINE_RLE_BLITTER(RLEBlitTransRemapDest)
{
public:
    explicit RLEBlitTransRemapDest(T* data, BYTE* remap) noexcept
        : PaletteData(data), RemapTable(remap) {}
    virtual ~RLEBlitTransRemapDest() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 line,
                           int32 zbase, uint16* zbuf, uint16* abuf,
                           int32 alvl, int32 warp, BYTE* zadjust) override final
    {
        auto dest = reinterpret_cast<T*>(dst);
        Process_Pre_Lines<false, false>(dest, src, len, line, zbuf, abuf);

        auto handler = [this](T& d, BYTE s)
        {
            if (RemapTable) d = PaletteData[RemapTable[s]];
        };
        Process_Pixel_Datas<false, false>(dest, src, len, zbase, zbuf, abuf, zadjust, handler);
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 line,
                                  int32 zbase, uint16* zbuf, uint16* abuf,
                                  int32 alvl, int32 warp, BYTE* zadjust,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, line, zbase, zbuf, abuf, alvl, warp, zadjust);
    }

private:
    T* PaletteData;
    BYTE* RemapTable;
};

// --- RLEBlitTransZRemapXlat ---
DEFINE_RLE_BLITTER(RLEBlitTransZRemapXlat)
{
public:
    explicit RLEBlitTransZRemapXlat(T* data, BYTE** zRemap) noexcept
        : PaletteData(data), ZRemapTables(zRemap) {}
    virtual ~RLEBlitTransZRemapXlat() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 line,
                           int32 zbase, uint16* zbuf, uint16* abuf,
                           int32 alvl, int32 warp, BYTE* zadjust) override final
    {
        auto dest = reinterpret_cast<T*>(dst);
        Process_Pre_Lines<false, false>(dest, src, len, line, zbuf, abuf);

        auto handler = [this](T& d, BYTE s)
        {
            d = PaletteData[s];
        };
        Process_Pixel_Datas<false, false>(dest, src, len, zbase, zbuf, abuf, zadjust, handler);
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 line,
                                  int32 zbase, uint16* zbuf, uint16* abuf,
                                  int32 alvl, int32 warp, BYTE* zadjust,
                                  uint16 tint) override final
    {
        Blit_Copy(dst, src, len, line, zbase, zbuf, abuf, alvl, warp, zadjust);
    }

private:
    T* PaletteData;
    BYTE** ZRemapTables;
};