// ============================================================================
// PaletteClass.cpp - 256-color palette management implementation
// ============================================================================
// Implements PAL file loading, per-entry colour access, brightness/contrast/
// tint adjustment, palette fading/interpolation, and nearest-colour lookup.
// ============================================================================

#include "PaletteClass.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// Construction / destruction
// ============================================================================

PaletteClass::PaletteClass() noexcept
{
    Clear();
}

PaletteClass::~PaletteClass()
{
    // No dynamically allocated resources.
}

// ============================================================================
// Helpers
// ============================================================================

uint8 PaletteClass::Clamp8(int32 value)
{
    if (value < 0)   return 0;
    if (value > 255) return 255;
    return static_cast<uint8>(value);
}

// Scale a 6-bit VGA value (0-63) to an 8-bit value (0-255).
static inline uint8 Scale6To8(uint8 val)
{
    // The original Westwood palette uses 6-bit per channel (0-63).
    // Scale to 8-bit by multiplying by 255/63 ≈ 4.0476.
    return static_cast<uint8>((static_cast<int32>(val) * 255 + 31) / 63);
}

// Scale an 8-bit value (0-255) down to 6-bit (0-63).
static inline uint8 Scale8To6(uint8 val)
{
    return static_cast<uint8>((static_cast<int32>(val) * 63 + 127) / 255);
}

// ============================================================================
// Palette loading
// ============================================================================

bool PaletteClass::Load_From_File(const char* pFilename)
{
    if (!pFilename || pFilename[0] == '\0') {
        return false;
    }

    FILE* pFile = std::fopen(pFilename, "rb");
    if (!pFile) {
        return false;
    }

    // Determine file size.
    std::fseek(pFile, 0, SEEK_END);
    long fileSize = std::ftell(pFile);
    std::fseek(pFile, 0, SEEK_SET);

    if (fileSize < 768) {
        // A PAL file must contain at least 256 * 3 = 768 bytes.
        std::fclose(pFile);
        return false;
    }

    // Read 768 bytes of RGB data.
    uint8 raw[768];
    size_t bytesRead = std::fread(raw, 1, 768, pFile);
    std::fclose(pFile);

    if (bytesRead < 768) {
        return false;
    }

    return Load_From_Data(raw, 768, true);
}

bool PaletteClass::Load_From_Data(const uint8* pData, int32 dataSize, bool bScale6Bit)
{
    if (!pData || dataSize < 768) {
        return false;
    }

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        uint8 r = pData[i * 3 + 0];
        uint8 g = pData[i * 3 + 1];
        uint8 b = pData[i * 3 + 2];
        if (bScale6Bit) {
            r = Scale6To8(r);
            g = Scale6To8(g);
            b = Scale6To8(b);
        }
        Colors[i].Red   = r;
        Colors[i].Green = g;
        Colors[i].Blue  = b;
    }

    return true;
}

bool PaletteClass::Save_To_File(const char* pFilename) const
{
    if (!pFilename || pFilename[0] == '\0') {
        return false;
    }

    FILE* pFile = std::fopen(pFilename, "wb");
    if (!pFile) {
        return false;
    }

    // Write 768 bytes of 6-bit-per-channel RGB data.
    uint8 raw[768];
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        raw[i * 3 + 0] = Scale8To6(Colors[i].Red);
        raw[i * 3 + 1] = Scale8To6(Colors[i].Green);
        raw[i * 3 + 2] = Scale8To6(Colors[i].Blue);
    }

    size_t bytesWritten = std::fwrite(raw, 1, 768, pFile);
    std::fclose(pFile);

    return bytesWritten == 768;
}

// ============================================================================
// Per-entry colour access
// ============================================================================

RGBClass& PaletteClass::Get_Color(int32 index)
{
    if (index < 0) index = 0;
    if (index >= PALETTE_SIZE) index = PALETTE_SIZE - 1;
    return Colors[index];
}

const RGBClass& PaletteClass::Get_Color(int32 index) const
{
    if (index < 0) index = 0;
    if (index >= PALETTE_SIZE) index = PALETTE_SIZE - 1;
    return Colors[index];
}

void PaletteClass::Set_Color(int32 index, const RGBClass& color)
{
    if (index < 0 || index >= PALETTE_SIZE) return;
    Colors[index] = color;
}

void PaletteClass::Set_Color(int32 index, uint8 r, uint8 g, uint8 b)
{
    if (index < 0 || index >= PALETTE_SIZE) return;
    Colors[index].Red   = r;
    Colors[index].Green = g;
    Colors[index].Blue  = b;
}

// ============================================================================
// Bulk palette data access
// ============================================================================

void PaletteClass::Get_Palette_Data(RGBClass* pOutColors) const
{
    if (!pOutColors) return;
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        pOutColors[i] = Colors[i];
    }
}

void PaletteClass::Get_Palette_Data(BytePalette* pOutPalette) const
{
    if (!pOutPalette) return;
    // The BytePalette is 256 bytes; we copy the RGB data into a raw buffer
    // and reinterpret.  In practice the caller uses this for compatibility
    // with ConvertClass which expects raw palette bytes.
    uint8* pRaw = reinterpret_cast<uint8*>(pOutPalette->Entries);
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        // Store the palette index itself (identity mapping) since BytePalette
        // holds byte indices, not RGB values.  This is the convention used by
        // the ConvertClass system for identity palettes.
        pRaw[i] = static_cast<uint8>(i);
    }
}

void PaletteClass::Set_Palette_Data(const RGBClass* pColors)
{
    if (!pColors) return;
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        Colors[i] = pColors[i];
    }
}

void PaletteClass::Set_Palette_Data(const uint8* pRawData, bool bScale6Bit)
{
    if (!pRawData) return;
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        uint8 r = pRawData[i * 3 + 0];
        uint8 g = pRawData[i * 3 + 1];
        uint8 b = pRawData[i * 3 + 2];
        if (bScale6Bit) {
            r = Scale6To8(r);
            g = Scale6To8(g);
            b = Scale6To8(b);
        }
        Colors[i].Red   = r;
        Colors[i].Green = g;
        Colors[i].Blue  = b;
    }
}

// ============================================================================
// Palette adjustment
// ============================================================================

void PaletteClass::Create_Adjust_Palette(PaletteClass* pOut,
                                         int32 brightness,
                                         int32 contrast,
                                         int32 tintR, int32 tintG, int32 tintB) const
{
    if (!pOut) return;

    // Compute the contrast factor centred at 128.
    // contrast ranges from -100 to +100; factor = 1.0 + contrast/100.0
    double contrastFactor = 1.0 + static_cast<double>(contrast) / 100.0;
    if (contrastFactor < 0.0) contrastFactor = 0.0;

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        int32 r = static_cast<int32>(Colors[i].Red);
        int32 g = static_cast<int32>(Colors[i].Green);
        int32 b = static_cast<int32>(Colors[i].Blue);

        // Apply contrast: stretch values away from / toward 128.
        r = static_cast<int32>((r - 128) * contrastFactor + 128);
        g = static_cast<int32>((g - 128) * contrastFactor + 128);
        b = static_cast<int32>((b - 128) * contrastFactor + 128);

        // Apply brightness (additive).
        r += brightness;
        g += brightness;
        b += brightness;

        // Apply tint (per-channel additive).
        r += tintR;
        g += tintG;
        b += tintB;

        pOut->Colors[i].Red   = Clamp8(r);
        pOut->Colors[i].Green = Clamp8(g);
        pOut->Colors[i].Blue  = Clamp8(b);
    }
}

void PaletteClass::Adjust(int32 brightness, int32 contrast,
                          int32 tintR, int32 tintG, int32 tintB)
{
    Create_Adjust_Palette(this, brightness, contrast, tintR, tintG, tintB);
}

// ============================================================================
// Palette fading
// ============================================================================

void PaletteClass::Fade_Palette(const PaletteClass& target, int32 ratio)
{
    // Clamp the ratio to [0, 255].
    if (ratio < 0)   ratio = 0;
    if (ratio > 255) ratio = 255;

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        int32 r1 = static_cast<int32>(Colors[i].Red);
        int32 g1 = static_cast<int32>(Colors[i].Green);
        int32 b1 = static_cast<int32>(Colors[i].Blue);

        int32 r2 = static_cast<int32>(target.Colors[i].Red);
        int32 g2 = static_cast<int32>(target.Colors[i].Green);
        int32 b2 = static_cast<int32>(target.Colors[i].Blue);

        // Linear interpolation: result = src + (dst - src) * ratio / 255
        Colors[i].Red   = static_cast<uint8>(r1 + (r2 - r1) * ratio / 255);
        Colors[i].Green = static_cast<uint8>(g1 + (g2 - g1) * ratio / 255);
        Colors[i].Blue  = static_cast<uint8>(b1 + (b2 - b1) * ratio / 255);
    }
}

void PaletteClass::Fade_To_Black(int32 ratio)
{
    if (ratio < 0)   ratio = 0;
    if (ratio > 255) ratio = 255;

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        int32 r = static_cast<int32>(Colors[i].Red);
        int32 g = static_cast<int32>(Colors[i].Green);
        int32 b = static_cast<int32>(Colors[i].Blue);

        // Fade toward (0,0,0): result = value * (255 - ratio) / 255
        Colors[i].Red   = static_cast<uint8>(r * (255 - ratio) / 255);
        Colors[i].Green = static_cast<uint8>(g * (255 - ratio) / 255);
        Colors[i].Blue  = static_cast<uint8>(b * (255 - ratio) / 255);
    }
}

void PaletteClass::Fade_To_White(int32 ratio)
{
    if (ratio < 0)   ratio = 0;
    if (ratio > 255) ratio = 255;

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        int32 r = static_cast<int32>(Colors[i].Red);
        int32 g = static_cast<int32>(Colors[i].Green);
        int32 b = static_cast<int32>(Colors[i].Blue);

        // Fade toward (255,255,255): result = value + (255 - value) * ratio / 255
        Colors[i].Red   = static_cast<uint8>(r + (255 - r) * ratio / 255);
        Colors[i].Green = static_cast<uint8>(g + (255 - g) * ratio / 255);
        Colors[i].Blue  = static_cast<uint8>(b + (255 - b) * ratio / 255);
    }
}

void PaletteClass::Interpolate(const PaletteClass& source1,
                               const PaletteClass& source2, int32 ratio)
{
    if (ratio < 0)   ratio = 0;
    if (ratio > 255) ratio = 255;

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        int32 r1 = static_cast<int32>(source1.Colors[i].Red);
        int32 g1 = static_cast<int32>(source1.Colors[i].Green);
        int32 b1 = static_cast<int32>(source1.Colors[i].Blue);

        int32 r2 = static_cast<int32>(source2.Colors[i].Red);
        int32 g2 = static_cast<int32>(source2.Colors[i].Green);
        int32 b2 = static_cast<int32>(source2.Colors[i].Blue);

        Colors[i].Red   = static_cast<uint8>(r1 + (r2 - r1) * ratio / 255);
        Colors[i].Green = static_cast<uint8>(g1 + (g2 - g1) * ratio / 255);
        Colors[i].Blue  = static_cast<uint8>(b1 + (b2 - b1) * ratio / 255);
    }
}

// ============================================================================
// Colour matching
// ============================================================================

int32 PaletteClass::Get_Closest_Color(uint8 r, uint8 g, uint8 b) const
{
    RGBClass target(r, g, b);
    return Get_Closest_Color(target);
}

int32 PaletteClass::Get_Closest_Color(const RGBClass& color) const
{
    int32 bestIndex = 0;
    int32 bestDist  = INT_MAX;

    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        int32 dr = static_cast<int32>(Colors[i].Red)   - static_cast<int32>(color.Red);
        int32 dg = static_cast<int32>(Colors[i].Green) - static_cast<int32>(color.Green);
        int32 db = static_cast<int32>(Colors[i].Blue)  - static_cast<int32>(color.Blue);

        // Squared Euclidean distance in RGB space.
        int32 dist = dr * dr + dg * dg + db * db;

        if (dist < bestDist) {
            bestDist  = dist;
            bestIndex = i;
            // Early exit on exact match.
            if (dist == 0) break;
        }
    }

    return bestIndex;
}

// ============================================================================
// Utility
// ============================================================================

// ----------------------------------------------------------------------------
// asm: Palette_FillBuffer (0x4BBAC0)
//
//   ; void __fastcall Palette_FillBuffer(WORD *a1, int len, Palette *source)
//   ecx = a1（目标 16bpp 缓冲）; edx = len（调色板组数）;
//   stack arg = source（8bit RGB 三元组流），retn 4。
//
//   逐句对照（asm 行 421874-422006）：
//     cmp esi,1 / jl 结束              —— len < 1 直接返回
//     lea eax,[esi-1] / test / jl 结束 —— len == 1 也直接返回（见下注）
//     外层 g 由 var_C(=0) 每轮 += 0x20000、var_8(=len-1) 每轮 -- 计数
//     内层 i: 0..0x100，ebx（目标游标）每轮 +2
//       ecx = (i * 0x100 + g*?) 的字节偏移，取 3 个字节 R/G/B
//       R = [p] << 0 ；G = [p+1] ；B = [p+2]
//       各 imul f / sar 16 / 上限 0xFF
//       再按 col_* 移位打包进 WORD
//
//   注：asm 的进入判据 `cmp esi,1 / jl` 与 `lea eax,[esi-1] / test eax,eax
//       / jl` 两条合起来是 len <= 1 时直接返回 —— 也就是说 len == 1
//       时**不写任何像素**（asm 在那种情况下走 loc_4BBC1B 直接 retn），
//       而不是"用 0x10000 因子填一遍"。loc_4BBB47 里的
//       `mov eax, 10000h` 只在 len > 1 的循环体内、被外层计数逻辑
//       用到时才可达；这里照 asm 的门槛保留 len <= 1 直接返回。
// ----------------------------------------------------------------------------
void PaletteClass::FillBuffer(uint16* pDest, int32 count, const uint8* pSource)
{
    // asm: cmp esi,1 / jl loc_4BBC1B  —— 只挡 <= 0；
    //   len == 1 是“单组、不缩放”的合法情况（原版注释：因子取 0x10000）。
    //   旧实现把 len == 1 一并 return，等于把整张表留成黑色，
    //   于是所有 ConvertClass(…, 1, …) 建出来的调色板全黑。
    if (pDest == nullptr || pSource == nullptr || count <= 0) {
        return;
    }

    const int32 groups = (count > 1) ? count : 1;
    int32 factorAccum = 0;          // asm: var_C，16.16 域内的增量累加器

    for (int32 g = 0; g < groups; ++g) {
        // asm: cmp esi,1 / jle -> 0x10000 ；否则 eax=var_C / idiv (esi-1)
        const int32 factor = (count > 1)
                                 ? (factorAccum / (count - 1))
                                 : 0x10000;

        for (int32 i = 0; i < PALETTE_SIZE; ++i) {
            const uint8* p = pSource + (g * PALETTE_SIZE + i) * 3;

            // asm: mov dl,[ecx] / imul esi,eax / sar esi,10h（见下注2）
            //      cmp esi,0FFh / jl / mov esi,0FFh
            int32 r = (static_cast<int32>(p[0]) * factor) >> 16;
            int32 gg = (static_cast<int32>(p[1]) * factor) >> 16;
            int32 b = (static_cast<int32>(p[2]) * factor) >> 16;
            if (r > 0xFF) r = 0xFF;
            if (gg > 0xFF) gg = 0xFF;
            if (b > 0xFF) b = 0xFF;

            // asm: 按 col_R_SHR_3/SHL_11、col_G_SHR_2/SHL_5、
            //      col_B_SHR_3/SHL_0 移位后 or 起来
            pDest[g * PALETTE_SIZE + i] = static_cast<uint16>(
                ((r >> 3) << 11) | ((gg >> 2) << 5) | ((b >> 3) << 0));
        }

        // asm: mov ecx,var_C / add ecx,20000h ; mov eax,var_8 / dec eax
        factorAccum += 0x20000;
    }
}

void PaletteClass::Clear()
{
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        Colors[i].Red   = 0;
        Colors[i].Green = 0;
        Colors[i].Blue  = 0;
    }
}

void PaletteClass::Set_Default()
{
    // Generate a default grey-ramp palette: the first 256 entries form a
    // smooth ramp from black to white, suitable as a fallback.
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        uint8 val = static_cast<uint8>(i);
        Colors[i].Red   = val;
        Colors[i].Green = val;
        Colors[i].Blue  = val;
    }

    // The first 16 entries get the standard EGA/VGA colour set so that
    // basic UI elements render correctly even without a game palette loaded.
    static const uint8 egaColors[16][3] = {
        { 0,   0,   0   },  // 0  black
        { 0,   0,   170 },  // 1  blue
        { 0,   170, 0   },  // 2  green
        { 0,   170, 170 },  // 3  cyan
        { 170, 0,   0   },  // 4  red
        { 170, 0,   170 },  // 5  magenta
        { 170, 85,  0   },  // 6  brown
        { 170, 170, 170 },  // 7  light grey
        { 85,  85,  85  },  // 8  dark grey
        { 85,  85,  255 },  // 9  light blue
        { 85,  255, 85  },  // 10 light green
        { 85,  255, 255 },  // 11 light cyan
        { 255, 85,  85  },  // 12 light red
        { 255, 85,  255 },  // 13 light magenta
        { 255, 255, 85  },  // 14 yellow
        { 255, 255, 255 }   // 15 white
    };

    for (int32 i = 0; i < 16; ++i) {
        Colors[i].Red   = egaColors[i][0];
        Colors[i].Green = egaColors[i][1];
        Colors[i].Blue  = egaColors[i][2];
    }
}

bool PaletteClass::Is_Black() const
{
    for (int32 i = 0; i < PALETTE_SIZE; ++i) {
        if (Colors[i].Red != 0 || Colors[i].Green != 0 || Colors[i].Blue != 0) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Palette_FillWith  (asm: Palette_FillWith, 0x626000)
//
//   edx = this+2 ; esi = 0x100 ; bl = 0
// clear_loop:                       ; zero 256 RGB triplets
//   [edx-2] = bl ; [edx-1] = bl ; [edx] = bl ; edx += 3
//   esi-- / jnz
//   edi = fill                       ; the source RGB triplet
// fill_loop:                        ; copy 256 triplets (2+1 byte moves)
//   [edx] = word [edi] ; [edx+2] = byte [edi+2]
// ============================================================================
void PaletteClass::FillWith(const RGBClass& fill)
{
    // asm clear loop: 256 entries zeroed (values overwritten below, kept for
    // exactness with the original's two-phase structure).
    for (int32 i = 0; i < 256; ++i) {
        RGBClass& entry = Get_Color(i);
        entry.Red   = 0;
        entry.Green = 0;
        entry.Blue  = 0;
    }

    // asm fill loop: one triplet replicated 256 times.
    for (int32 i = 0; i < 256; ++i) {
        RGBClass& entry = Get_Color(i);
        entry.Red   = fill.Red;
        entry.Green = fill.Green;
        entry.Blue  = fill.Blue;
    }
}
