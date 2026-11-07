#pragma once

// ============================================================================
// PaletteClass - 256-color palette management
//
// Manages a 256-entry RGB palette used by the 8-bit indexed rendering
// pipeline.  Each entry is an RGBClass (red, green, blue) with 8-bit
// components.  The class supports loading from Westwood PAL files,
// per-entry colour access, brightness/contrast/tint adjustment, palette
// fading, and nearest-colour lookup (RGB to palette index).
// ============================================================================

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "Surface.h"  // for RGBClass, BytePalette

// ============================================================================
// Constants
// ============================================================================
constexpr int32 PALETTE_SIZE = 256;

// ============================================================================
// FadeDirection - direction of a palette fade operation
// ============================================================================
enum class FadeDirection : int32
{
    In   = 0,   // Fade from black (or another palette) toward the target
    Out  = 1    // Fade from the target toward black
};

// ============================================================================
// PaletteClass
// ============================================================================
class PaletteClass
{
public:
    // ----------------------------------------------------------------------
    // Construction / destruction
    // ----------------------------------------------------------------------
    PaletteClass() noexcept;
    ~PaletteClass();

    // ----------------------------------------------------------------------
    // Palette loading
    // ----------------------------------------------------------------------

    // Load a palette from a Westwood PAL file (768 bytes of RGB triplets,
    // 6-bit-per-channel VGA values scaled to 8-bit internally).
    bool Load_From_File(const char* pFilename);

    // Load from an in-memory PAL buffer (768 bytes of R,G,B triplets).
    // If bScale6Bit is true, each 6-bit value (0-63) is scaled to 8-bit (0-255).
    bool Load_From_Data(const uint8* pData, int32 dataSize, bool bScale6Bit = true);

    // Save the palette to a PAL file (768 bytes, 6-bit-per-channel).
    bool Save_To_File(const char* pFilename) const;

    // ----------------------------------------------------------------------
    // Per-entry colour access
    // ----------------------------------------------------------------------

    RGBClass& Get_Color(int32 index);
    const RGBClass& Get_Color(int32 index) const;

    void Set_Color(int32 index, const RGBClass& color);
    void Set_Color(int32 index, uint8 r, uint8 g, uint8 b);

    // asm: Palette_FillWith (0x626000)
    // Zero-fills all 256 RGB triplets and then copies the single `fill`
    // triplet across every entry -- a uniform-color palette sweep.
    void FillWith(const RGBClass& fill);

    // ----------------------------------------------------------------------
    // Bulk palette data access
    // ----------------------------------------------------------------------

    // Copy the entire palette into the provided 256-entry RGBClass array.
    void Get_Palette_Data(RGBClass* pOutColors) const;

    // Copy the entire palette into a BytePalette (byte indices are unused;
    // the raw RGB values are stored in the Entries array reinterpret-cast).
    void Get_Palette_Data(BytePalette* pOutPalette) const;

    // Set the entire palette from a 256-entry RGBClass array.
    void Set_Palette_Data(const RGBClass* pColors);

    // Set the entire palette from raw byte triplets (768 bytes, R,G,B,...).
    void Set_Palette_Data(const uint8* pRawData, bool bScale6Bit = false);

    // ----------------------------------------------------------------------
    // Palette adjustment
    // ----------------------------------------------------------------------

    // Create an adjusted copy of this palette.
    //   brightness  - -255 (full black) to +255 (full white), 0 = no change
    //   contrast    - -100 to +100, 0 = no change (applied as a multiplier
    //                 centred at 128)
    //   tintR/G/B   - -255 to +255 colour-channel tint, 0 = no change
    // The result is written to pOut (which may be the same as 'this').
    void Create_Adjust_Palette(PaletteClass* pOut,
                               int32 brightness,
                               int32 contrast,
                               int32 tintR, int32 tintG, int32 tintB) const;

    // Convenience: adjust this palette in-place.
    void Adjust(int32 brightness, int32 contrast,
                int32 tintR, int32 tintG, int32 tintB);

    // ----------------------------------------------------------------------
    // Palette fading
    // ----------------------------------------------------------------------

    // Fade this palette toward a target palette by a given ratio.
    //   ratio = 0   -> result equals *this
    //   ratio = 255 -> result equals target
    void Fade_Palette(const PaletteClass& target, int32 ratio);

    // Fade toward solid black (all channels 0).
    //   ratio = 0   -> no change
    //   ratio = 255 -> fully black
    void Fade_To_Black(int32 ratio);

    // Fade toward solid white (all channels 255).
    void Fade_To_White(int32 ratio);

    // Interpolate between two palettes and store the result in this palette.
    //   ratio = 0   -> result equals source1
    //   ratio = 255 -> result equals source2
    void Interpolate(const PaletteClass& source1,
                     const PaletteClass& source2, int32 ratio);

    // ----------------------------------------------------------------------
    // Colour matching
    // ----------------------------------------------------------------------

    // Find the palette index whose colour is closest to the given RGB value.
    // Uses a simple squared-distance metric in RGB space.
    int32 Get_Closest_Color(uint8 r, uint8 g, uint8 b) const;
    int32 Get_Closest_Color(const RGBClass& color) const;

    // ----------------------------------------------------------------------
    // Utility
    // ----------------------------------------------------------------------

    // asm: Palette_FillBuffer (0x4BBAC0)
    //
    //   ; void __fastcall Palette_FillBuffer(WORD *a1, int len, Palette *source)
    //   ecx = a1 (目标 16bpp 缓冲) ; edx = len (要填的调色板组数)
    //   arg_0 = source (const uint8*，8bit RGB 三元组流)，retn 4
    //
    //   调用者：ConvertClass::ConvertClass+411 —— 构造时把 N 份 16bpp
    //   调色板一次铺出来，第 g 份整体按 16.16 定点因子
    //   g*2/(len-1) 缩放（len <= 1 时因子取 0x10000，即不缩放）：
    //     for (g = 0 ; g < len ; ++g):
    //       f = (len > 1) ? g*0x20000/(len-1) : 0x10000
    //       for (i = 0 ; i < 0x100 ; ++i):
    //         p = source + (g*0x100 + i)*3
    //         R = min((p[0]*f) >> 16, 0xFF)
    //         G = min((p[1]*f) >> 16, 0xFF)
    //         B = min((p[2]*f) >> 16, 0xFF)
    //         out[i] = ((R >> col_R_SHR_3) << col_R_SHL_11)
    //                | ((G >> col_G_SHR_2) << col_G_SHL_5)
    //                | ((B >> col_B_SHR_3) << col_B_SHL_0)
    //   源缓冲按 0x100*3 字节步进，定点因子按 0x20000 递增。
    //
    //   注 1：asm 里 source 是 Palette*（0x300 字节），但逐字节取的是
    //         RGB 三元组，与本题的 const uint8* 视角等价。
    //   注 2：col_*_SHR_/SHL_ 是运行期全局量（SetVideoMode 按色深设置，
    //         见 sub_4BBC30/sub_4BBC40 一对 getter）。本项目统一按
    //         RGB565 处理（红 >>3<<11、绿 >>2<<5、蓝 >>3<<0），
    //         与 Surface.h 已有的打包约定一致。
    //   注 3：原版用 sar（算术右移）取整除；三个分量都非负，<<16 后的
    //         中间量也非负，故 /65536 与之等价。
    // ----------------------------------------------------------------------
    static void FillBuffer(uint16* pDest, int32 count, const uint8* pSource);

    // Reset all entries to black.
    void Clear();

    // Reset to the default Westwood palette (a neutral grey ramp).
    void Set_Default();

    // Check whether the palette is all-black.
    bool Is_Black() const;

    // Number of palette entries (always 256).
    int32 Get_Size() const { return PALETTE_SIZE; }

    // Array-style access.
    RGBClass& operator[](int32 index) { return Colors[index]; }
    const RGBClass& operator[](int32 index) const { return Colors[index]; }

private:
    // Clamp a value to the 0-255 range.
    static uint8 Clamp8(int32 value);

    RGBClass Colors[PALETTE_SIZE];
};
