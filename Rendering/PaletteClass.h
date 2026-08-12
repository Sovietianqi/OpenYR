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
