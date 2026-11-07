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
    // asm: Blit_Copy_Tinted 的通道混合 —— 结果 = (src + tint) / 2（逐通道）。
    //   原版 blitter 的 tinted 版本就是这条；旧实现直接转调 Blit_Copy，
    //   把 tint 参数整个丢掉，于是铁幕/超时空/力场染色全都不显示。
    static uint16 Tint565(uint16 src, uint16 tint) noexcept
    {
        const uint32 r = (((src >> 11) & 0x1Fu) + ((tint >> 11) & 0x1Fu)) >> 1;
        const uint32 g = (((src >>  5) & 0x3Fu) + ((tint >>  5) & 0x3Fu)) >> 1;
        const uint32 b = (( src        & 0x1Fu) + ( tint        & 0x1Fu)) >> 1;
        return static_cast<uint16>((r << 11) | (g << 5) | b);
    }

    static uint16* Lookup_Alpha_Remapper(int32 alvl, AlphaLightingRemapClass* remapper)
    {
        if (!remapper) return nullptr;
        int32 level = std::min(254, (261 * std::max(0, alvl)) >> 11);
        return remapper->Table[level];
    }

    // 深度测试。
    //   asm 里每张 DSurface 都带 z 缓冲，CC_Draw_Shape 一定会传非空指针；
    //   本工程的 Surface 还没有 z 缓冲，因此传 nullptr。历史实现用
    //   `if (idx && zbuf && ...)` 直接跳过，导致所有 ZRead 类提笔器
    //   一个像素都不落（0x2600 / 0x2601 / 0x4E00 这些真实 flags 全中招）。
    //   这里把"没有 z 缓冲"定义为"不做深度测试"，与 asm 在有缓冲时的
    //   语义一致，且不改变选槽结果 —— 等 Surface 补上 z 缓冲后自动生效。
    static bool ZPass(uint16 zVal, const uint16* zbuf) noexcept
    {
        return zbuf == nullptr || zVal >= *zbuf;
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

    // 与 Blitter::ZPass 同义（RLE 侧独立基类，不能直接继承）。
    static bool ZPass(uint16 zVal, const uint16* zbuf) noexcept
    {
        return zbuf == nullptr || zVal >= *zbuf;
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
            // Plain 不带色键：索引 0 也照写（原版 BlitPlainXlat）。
            *dest = PaletteData[*src++];
            ++dest;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        while (len--)
        {
            *dest = Tint565(PaletteData[*src++], tint);
            ++dest;
        }
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
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        while (len--)
        {
            BYTE idx = *src++;
            if (idx) *dest = Tint565(PaletteData[idx], tint);
            ++dest;
        }
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
            if (idx)
            {
                if (ZPass(zVal, zbuf))
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
            if (idx && ZPass(zVal, zbuf))
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
            if (idx && ZPass(zVal, zbuf))
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
            if (idx && ZPass(zVal, zbuf))
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
            if (idx)
            {
                if (ZPass(zVal, zbuf))
                {
                    *dest = PaletteData[idx];
                    if (zbuf) *zbuf = zVal;
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
            if (ZPass(zVal, zbuf))
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
            if (ZPass(zVal, zbuf))
            {
                *dest = PaletteData[idx];
                if (zbuf) *zbuf = zVal;
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
            if (idx && ZPass(zVal, zbuf))
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
            if (idx && ZPass(zVal, zbuf))
            {
                *dest = static_cast<T>(BlitterTables::Darken(*dest, PaletteData[idx], Mask));
                if (zbuf) *zbuf = zVal;
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
// ============================================================================
// 组合型提笔器（asm SelectBlitter0 的 36/45/10/20/26/37/46 槽）
//
// 这几个槽在零售版里真实可达（例如 0x2E00 / 0x4E00 的建筑/动画阴影路径），
// 语义就是已有几种效果的并集：透明色键 + 可选 z 重映射 + 可选 alpha 缓冲 +
// 可选 z 读写。下面按 asm 的分支结构逐个组合，不新增任何"大概像"的行为。
// ============================================================================

// --- BlitTransXlatAlphaZRead ---（asm 槽 36）
// 透明色键 + alpha 缓冲混合 + z 只读：被更近的像素挡住时不落笔。
DEFINE_BLITTER(BlitTransXlatAlphaZRead)
{
public:
    explicit BlitTransXlatAlphaZRead(T* data, AlphaLightingRemapClass* alphaRemap) noexcept
        : PaletteData(data), AlphaRemap(alphaRemap) {}
    virtual ~BlitTransXlatAlphaZRead() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16* alphaTable = Lookup_Alpha_Remapper(alvl, AlphaRemap);
        const uint16 zVal = static_cast<uint16>(zval);

        while (len--)
        {
            const BYTE idx = *src++;
            if (idx && ZPass(zVal, zbuf))
            {
                const uint16 srcColor = PaletteData[idx];
                if (alphaTable && abuf)
                {
                    const uint16 alpha = *abuf++;
                    const uint16 blended = alphaTable[alpha & 0xFF];
                    *dest = static_cast<T>(BlitterTables::Blend50(*dest, srcColor, blended));
                }
                else
                {
                    *dest = srcColor;
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
    AlphaLightingRemapClass* AlphaRemap;
};

// --- BlitTransXlatAlphaZReadWrite ---（asm 槽 45）
// 同上，但通过 z 测试时同时把 zval 写回 z 缓冲。
DEFINE_BLITTER(BlitTransXlatAlphaZReadWrite)
{
public:
    explicit BlitTransXlatAlphaZReadWrite(T* data, AlphaLightingRemapClass* alphaRemap) noexcept
        : PaletteData(data), AlphaRemap(alphaRemap) {}
    virtual ~BlitTransXlatAlphaZReadWrite() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        uint16* alphaTable = Lookup_Alpha_Remapper(alvl, AlphaRemap);
        const uint16 zVal = static_cast<uint16>(zval);

        while (len--)
        {
            const BYTE idx = *src++;
            if (idx && ZPass(zVal, zbuf))
            {
                const uint16 srcColor = PaletteData[idx];
                if (alphaTable && abuf)
                {
                    const uint16 alpha = *abuf++;
                    const uint16 blended = alphaTable[alpha & 0xFF];
                    *dest = static_cast<T>(BlitterTables::Blend50(*dest, srcColor, blended));
                }
                else
                {
                    *dest = srcColor;
                }
                if (zbuf) *zbuf = zVal;
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
    AlphaLightingRemapClass* AlphaRemap;
};

// --- BlitTransZRemapXlatZRead ---（asm 槽 10）
DEFINE_BLITTER(BlitTransZRemapXlatZRead)
{
public:
    explicit BlitTransZRemapXlatZRead(T* data, BYTE** zRemap) noexcept
        : PaletteData(data), ZRemapTables(zRemap) {}
    virtual ~BlitTransZRemapXlatZRead() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        BYTE* zrmp = (ZRemapTables && zval >= 0) ? ZRemapTables[zval] : nullptr;
        const uint16 zVal = static_cast<uint16>(zval);

        while (len--)
        {
            const BYTE idx = *src++;
            if (idx && ZPass(zVal, zbuf))
            {
                const BYTE mapped = zrmp ? zrmp[idx] : idx;
                *dest = PaletteData[mapped];
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
    BYTE** ZRemapTables;
};

// --- BlitTransZRemapXlatZReadWrite ---（asm 槽 20）
DEFINE_BLITTER(BlitTransZRemapXlatZReadWrite)
{
public:
    explicit BlitTransZRemapXlatZReadWrite(T* data, BYTE** zRemap) noexcept
        : PaletteData(data), ZRemapTables(zRemap) {}
    virtual ~BlitTransZRemapXlatZReadWrite() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        BYTE* zrmp = (ZRemapTables && zval >= 0) ? ZRemapTables[zval] : nullptr;
        const uint16 zVal = static_cast<uint16>(zval);

        while (len--)
        {
            const BYTE idx = *src++;
            if (idx && ZPass(zVal, zbuf))
            {
                const BYTE mapped = zrmp ? zrmp[idx] : idx;
                *dest = PaletteData[mapped];
                if (zbuf) *zbuf = zVal;
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
    BYTE** ZRemapTables;
};

// --- BlitTransZRemapXlatAlpha ---（asm 槽 26）
DEFINE_BLITTER(BlitTransZRemapXlatAlpha)
{
public:
    explicit BlitTransZRemapXlatAlpha(T* data, BYTE** zRemap,
                                      AlphaLightingRemapClass* alphaRemap) noexcept
        : PaletteData(data), ZRemapTables(zRemap), AlphaRemap(alphaRemap) {}
    virtual ~BlitTransZRemapXlatAlpha() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        BYTE* zrmp = (ZRemapTables && zval >= 0) ? ZRemapTables[zval] : nullptr;
        uint16* alphaTable = Lookup_Alpha_Remapper(alvl, AlphaRemap);

        while (len--)
        {
            const BYTE idx = *src++;
            if (idx)
            {
                const BYTE mapped = zrmp ? zrmp[idx] : idx;
                const uint16 srcColor = PaletteData[mapped];
                if (alphaTable && abuf)
                {
                    const uint16 alpha = *abuf++;
                    const uint16 blended = alphaTable[alpha & 0xFF];
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
    BYTE** ZRemapTables;
    AlphaLightingRemapClass* AlphaRemap;
};

// --- BlitTransZRemapXlatAlphaZRead ---（asm 槽 37）
DEFINE_BLITTER(BlitTransZRemapXlatAlphaZRead)
{
public:
    explicit BlitTransZRemapXlatAlphaZRead(T* data, BYTE** zRemap,
                                           AlphaLightingRemapClass* alphaRemap) noexcept
        : PaletteData(data), ZRemapTables(zRemap), AlphaRemap(alphaRemap) {}
    virtual ~BlitTransZRemapXlatAlphaZRead() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        BYTE* zrmp = (ZRemapTables && zval >= 0) ? ZRemapTables[zval] : nullptr;
        uint16* alphaTable = Lookup_Alpha_Remapper(alvl, AlphaRemap);
        const uint16 zVal = static_cast<uint16>(zval);

        while (len--)
        {
            const BYTE idx = *src++;
            if (idx && ZPass(zVal, zbuf))
            {
                const BYTE mapped = zrmp ? zrmp[idx] : idx;
                const uint16 srcColor = PaletteData[mapped];
                if (alphaTable && abuf)
                {
                    const uint16 alpha = *abuf++;
                    const uint16 blended = alphaTable[alpha & 0xFF];
                    *dest = static_cast<T>(BlitterTables::Blend50(*dest, srcColor, blended));
                }
                else
                {
                    *dest = srcColor;
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
    BYTE** ZRemapTables;
    AlphaLightingRemapClass* AlphaRemap;
};

// --- BlitTransZRemapXlatAlphaZReadWrite ---（asm 槽 46）
DEFINE_BLITTER(BlitTransZRemapXlatAlphaZReadWrite)
{
public:
    explicit BlitTransZRemapXlatAlphaZReadWrite(T* data, BYTE** zRemap,
                                                AlphaLightingRemapClass* alphaRemap) noexcept
        : PaletteData(data), ZRemapTables(zRemap), AlphaRemap(alphaRemap) {}
    virtual ~BlitTransZRemapXlatAlphaZReadWrite() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;
        auto dest = reinterpret_cast<T*>(dst);
        BYTE* zrmp = (ZRemapTables && zval >= 0) ? ZRemapTables[zval] : nullptr;
        uint16* alphaTable = Lookup_Alpha_Remapper(alvl, AlphaRemap);
        const uint16 zVal = static_cast<uint16>(zval);

        while (len--)
        {
            const BYTE idx = *src++;
            if (idx && ZPass(zVal, zbuf))
            {
                const BYTE mapped = zrmp ? zrmp[idx] : idx;
                const uint16 srcColor = PaletteData[mapped];
                if (alphaTable && abuf)
                {
                    const uint16 alpha = *abuf++;
                    const uint16 blended = alphaTable[alpha & 0xFF];
                    *dest = static_cast<T>(BlitterTables::Blend50(*dest, srcColor, blended));
                }
                else
                {
                    *dest = srcColor;
                }
                if (zbuf) *zbuf = zVal;
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
    BYTE** ZRemapTables;
    AlphaLightingRemapClass* AlphaRemap;
};

// ============================================================================
// 组合提笔器：Lucent × {ZRead / ZReadWrite / Warp / Alpha}
//
// 这 15 个 asm 槽（14/15/16、21/22/23、38/39/40、41/42/43、47/48/49）此前
// 在 SlotToBlitter[] 里是 -1，选到就退回 BlitTransXlat。它们不是"另一种
// 混合公式"，而是三条已验证语义的正交组合：
//
//   ① 混合系数 —— 与同档 Lucent 完全一致，取自 asm 的移位/乘加：
//        25%: shr ax,2 / lea eax,[eax+eax*2]  → 3/4 src + 1/4 dst
//        50%: shr ax,1 / add                  → 1/2 src + 1/2 dst
//        75%: shr ax,2 ×3 + ...               → 1/4 src + 3/4 dst
//      对应项目里已有的 Blend25 / Blend50 / Blend75。
//   ② z 深度 —— 读：`cmp zval, *zbuf / jge 跳过`（等价 ZPass）；
//                写：落笔后 `mov [zbuf], zval`。
//   ③ Warp —— 背景采样取 `dst[warp]` 而不是 `dst[0]`。
//      证据：sub_4933D0（BlitTransLucent25ZReadWarp::Blit_Copy）
//            mov ebp,[esp+arg_1C] / mov bp,[edi+ebp*2]   ; 背景 = dst[warp]
//      注意写回仍在 dst[0]，warp 只影响"读哪一颗背景像素"。
//   ④ Alpha —— 读 `*abuf` 当 alpha，经 Lookup_Alpha_Remapper(alvl) 取表，
//      再按本工程既有的 alpha 语义混合（与 BlitTransXlatAlpha 同一套，
//      asm sub_495250 的 (261*alvl)>>11 与之逐位一致）。
//
// 之所以用模板而不是抄 15 份：15 份手抄必然漂移，而它们的差异只有这 4 个
// 布尔量和 1 个档位。命名仍按 asm 的类名做别名，调用方看不出区别。
// ============================================================================
template<typename T, int Level, bool UseZRead, bool UseZWrite, bool UseWarp, bool UseAlpha>
class BlitTransLucentVariant final : public Blitter
{
public:
    BlitTransLucentVariant(T* data, uint16 mask, AlphaLightingRemapClass* alphaRemap = nullptr) noexcept
        : PaletteData(data), Mask(mask), AlphaRemap(alphaRemap) {}

    virtual ~BlitTransLucentVariant() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;

        auto dest = reinterpret_cast<T*>(dst);
        const uint16 zVal = static_cast<uint16>(zval);
        uint16* const alphaTable = UseAlpha ? Lookup_Alpha_Remapper(alvl, AlphaRemap) : nullptr;

        while (len--)
        {
            const BYTE idx = *src++;
            if (idx)
            {
                if (!UseZRead || ZPass(zVal, zbuf))
                {
                    // ③ Warp：背景取 dst[warp]；非 warp 变体取 dst[0]。
                    const T back = UseWarp ? dest[warp] : *dest;
                    const T fore = PaletteData[idx];

                    T out;
                    if constexpr (Level == 25)
                        out = static_cast<T>(BlitterTables::Blend25(back, fore, Mask));
                    else if constexpr (Level == 50)
                        out = static_cast<T>(BlitterTables::Blend50(back, fore, Mask));
                    else
                        out = static_cast<T>(BlitterTables::Blend75(back, fore, Mask));

                    // ④ Alpha：与 BlitTransXlatAlpha 同一套（表项当掩码用）。
                    if constexpr (UseAlpha)
                    {
                        if (alphaTable != nullptr && abuf != nullptr)
                        {
                            const uint16 a = alphaTable[static_cast<uint16>(*abuf) & 0xFF];
                            out = static_cast<T>(BlitterTables::Blend50(out, fore, a));
                        }
                    }

                    *dest = out;

                    // ② 写深度：asm 只在真正落笔的分支里写 zbuf。
                    if constexpr (UseZWrite)
                    {
                        if (zbuf != nullptr) *zbuf = zVal;
                    }
                }
            }

            ++dest;
            if (zbuf != nullptr) ++zbuf;
            if (abuf != nullptr) ++abuf;
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

// asm 槽 14/15/16 —— Lucent75/50/25 + ZRead + Warp
template<typename T> using BlitTransLucent75ZReadWarp = BlitTransLucentVariant<T, 75, true, false, true, false>;
template<typename T> using BlitTransLucent50ZReadWarp = BlitTransLucentVariant<T, 50, true, false, true, false>;
template<typename T> using BlitTransLucent25ZReadWarp = BlitTransLucentVariant<T, 25, true, false, true, false>;

// asm 槽 21/22/23 —— Lucent75/50/25 + ZRead + ZWrite
template<typename T> using BlitTransLucent75ZReadWrite = BlitTransLucentVariant<T, 75, true, true, false, false>;
template<typename T> using BlitTransLucent50ZReadWrite = BlitTransLucentVariant<T, 50, true, true, false, false>;
template<typename T> using BlitTransLucent25ZReadWrite = BlitTransLucentVariant<T, 25, true, true, false, false>;

// asm 槽 38/39/40 —— Lucent75/50/25 + Alpha + ZRead
template<typename T> using BlitTransLucent75AlphaZRead = BlitTransLucentVariant<T, 75, true, false, false, true>;
template<typename T> using BlitTransLucent50AlphaZRead = BlitTransLucentVariant<T, 50, true, false, false, true>;
template<typename T> using BlitTransLucent25AlphaZRead = BlitTransLucentVariant<T, 25, true, false, false, true>;

// asm 槽 41/42/43 —— Lucent75/50/25 + Alpha + ZRead + Warp
template<typename T> using BlitTransLucent75AlphaZReadWarp = BlitTransLucentVariant<T, 75, true, false, true, true>;
template<typename T> using BlitTransLucent50AlphaZReadWarp = BlitTransLucentVariant<T, 50, true, false, true, true>;
template<typename T> using BlitTransLucent25AlphaZReadWarp = BlitTransLucentVariant<T, 25, true, false, true, true>;

// asm 槽 47/48/49 —— Lucent75/50/25 + Alpha + ZRead + ZWrite
template<typename T> using BlitTransLucent75AlphaZReadWrite = BlitTransLucentVariant<T, 75, true, true, false, true>;
template<typename T> using BlitTransLucent50AlphaZReadWrite = BlitTransLucentVariant<T, 50, true, true, false, true>;
template<typename T> using BlitTransLucent25AlphaZReadWrite = BlitTransLucentVariant<T, 25, true, true, false, true>;

// ============================================================================
// 写 alpha 缓冲的两个提笔器（asm 槽 30 / 31）
//
// 反汇编依据：
//   sub_494490  BlitTransXlatWriteAlpha::Blit_Copy
//     mov  cl,[esi] / test cl,cl / jz skip        ; 色键 0：连 alpha 都不写
//     and  ecx,0FFh / add ecx,ebx                 ; ebx = 第 4 参 zval
//     cmp  ecx,0FFh / jl ok / mov ecx,0FFh        ; 钳到 255
//     mov  [eax],cx                               ; eax = 第 6 参 abuf
//   → **只写 alpha，一个颜色字节都不写**。
//
//   sub_494590  BlitTransXlatMultWriteAlpha::Blit_Copy
//     and  ecx,0FFh / imul ecx,edi / add ecx,ebx  ; edi = 第 7 参 alvl，ebx = zval
//     cmp  ecx,0FFh / jl ok / mov ecx,0FFh
//     mov  [eax],cx
//   → alpha = clamp(srcIdx * alvl + zval)。
//
// 两者都不接受 palette —— asm 里这两个函数连 pal 都不读，颜色通道完全不动。
// ============================================================================
template<typename T>
class BlitTransXlatWriteAlpha final : public Blitter
{
public:
    explicit BlitTransXlatWriteAlpha(T* /*data*/) noexcept {}
    virtual ~BlitTransXlatWriteAlpha() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;

        auto dest = reinterpret_cast<T*>(dst);
        while (len--)
        {
            const BYTE idx = *src++;
            if (idx && abuf != nullptr)
            {
                int32 a = static_cast<int32>(idx) + zval;   // asm: add ecx,ebx
                if (a > 255) a = 255;                       // asm: cmp 0FFh / jl / mov 0FFh
                *abuf = static_cast<uint16>(a);
            }
            ++dest;
            if (abuf != nullptr) ++abuf;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    { Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp); }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    { Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp); }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    { Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp); }
};

template<typename T>
class BlitTransXlatMultWriteAlpha final : public Blitter
{
public:
    explicit BlitTransXlatMultWriteAlpha(T* /*data*/) noexcept {}
    virtual ~BlitTransXlatMultWriteAlpha() override final = default;

    virtual void Blit_Copy(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    {
        if (len < 0) return;

        auto dest = reinterpret_cast<T*>(dst);
        while (len--)
        {
            const BYTE idx = *src++;
            if (idx && abuf != nullptr)
            {
                int32 a = static_cast<int32>(idx) * alvl + zval;   // asm: imul ecx,edi / add ecx,ebx
                if (a > 255) a = 255;
                if (a < 0)   a = 0;
                *abuf = static_cast<uint16>(a);
            }
            ++dest;
            if (abuf != nullptr) ++abuf;
        }
    }

    virtual void Blit_Copy_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    { Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp); }

    virtual void Blit_Move(void* dst, BYTE* src, int32 len, int32 zval,
                           uint16* zbuf, uint16* abuf, int32 alvl, int32 warp) override final
    { Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp); }

    virtual void Blit_Move_Tinted(void* dst, BYTE* src, int32 len, int32 zval,
                                  uint16* zbuf, uint16* abuf, int32 alvl, int32 warp,
                                  uint16 tint) override final
    { Blit_Copy(dst, src, len, zval, zbuf, abuf, alvl, warp); }
};
