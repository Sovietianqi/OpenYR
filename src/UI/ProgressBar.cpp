#include <UI/ProgressBar.h>

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <Rendering/Surface.h>

#include <cmath>
#include <cstdio>
#include <cwchar>

// ============================================================================
// ProgressBar.cpp - Progress bar implementation
//
//  Implements the ProgressBarClass member functions.  The progress bar
//  supports a solid fill, segmented display, smooth gradient and
//  indeterminate (marquee) mode.
//
//  Rendering features:
//    * Four visual styles: Solid, Segmented, Smooth (gradient), Marquee
//    * Progress-dependent color transitions (red -> yellow -> green)
//    * Animated display value with configurable interpolation speed
//    * Optional percentage text overlay centred on the bar
//    * 1-pixel border and bevelled edges for a 3D look
//    * Marquee block that bounces smoothly between edges
//
//  The actual pixel output goes through the Surface interface, which
//  abstracts 8/16/32-bit frame buffers.  Colour values are converted to
//  the surface's native format before being passed to FillRect / SetPixel.
// ============================================================================

// ----------------------------------------------------------------------------
// File-local helpers
// ----------------------------------------------------------------------------

namespace
{
    // ── Colour conversion ────────────────────────────────────────────────

    // Convert an 8-bit-per-channel ColorStruct into a 16-bit RGB565 DWORD,
    // which is the format used by DSurface (the primary rendering surface).
    DWORD ColorTo565(const ColorStruct& c) noexcept
    {
        uint16 r5 = static_cast<uint16>(c.R >> 3);
        uint16 g6 = static_cast<uint16>(c.G >> 2);
        uint16 b5 = static_cast<uint16>(c.B >> 3);
        return static_cast<DWORD>((r5 << 11) | (g6 << 5) | b5);
    }

    // Convert a ColorStruct into a 32-bit ARGB DWORD (for 32-bit surfaces).
    DWORD ColorToARGB(const ColorStruct& c) noexcept
    {
        return (static_cast<DWORD>(c.A) << 24) |
               (static_cast<DWORD>(c.R) << 16) |
               (static_cast<DWORD>(c.G) << 8)  |
               static_cast<DWORD>(c.B);
    }

    // Convert a ColorStruct to the surface's native DWORD format.
    DWORD ColorToNative(Surface* pSurface, const ColorStruct& c) noexcept
    {
        if (!pSurface) return 0;
        int32 bpp = pSurface->GetBytesPerPixel();
        if (bpp == 2) return ColorTo565(c);
        if (bpp == 4) return ColorToARGB(c);
        // 8-bit: use a simple luminance approximation for palette index.
        return static_cast<DWORD>((c.R + c.G + c.B) / 3);
    }

    // ── Colour interpolation ─────────────────────────────────────────────

    // Linear interpolation between two colours.  t is in [0, 1].
    ColorStruct LerpColor(const ColorStruct& a, const ColorStruct& b, float t) noexcept
    {
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        int32 r = static_cast<int32>(a.R + (static_cast<float>(b.R) - a.R) * t + 0.5f);
        int32 g = static_cast<int32>(a.G + (static_cast<float>(b.G) - a.G) * t + 0.5f);
        int32 bl = static_cast<int32>(a.B + (static_cast<float>(b.B) - a.B) * t + 0.5f);
        if (r < 0) r = 0;  if (r > 255) r = 255;
        if (g < 0) g = 0;  if (g > 255) g = 255;
        if (bl < 0) bl = 0;  if (bl > 255) bl = 255;
        return ColorStruct(static_cast<uint8>(r), static_cast<uint8>(g), static_cast<uint8>(bl));
    }

    // ── Progress-based colour mapping ────────────────────────────────────
    //
    //  Maps a progress value in [0, 1] to a colour on a red-yellow-green
    //  ramp.  This is used when the bar's FillColor has not been explicitly
    //  customised, giving the bar an intuitive "health bar" appearance.

    ColorStruct ProgressToColor(float progress) noexcept
    {
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        ColorStruct red  (200,  30,  30);
        ColorStruct yellow(220, 200,  30);
        ColorStruct green ( 30, 200,  30);

        if (progress < 0.5f)
        {
            // Red -> Yellow over the first half.
            return LerpColor(red, yellow, progress * 2.0f);
        }
        // Yellow -> Green over the second half.
        return LerpColor(yellow, green, (progress - 0.5f) * 2.0f);
    }

    // ── Drawing primitives ───────────────────────────────────────────────

    // Fill a horizontal span of pixels on the surface with a solid colour.
    // Used for gradient rendering where each column may have a different
    // colour.
    void FillHorizontalSpan(Surface* pSurface, int32 x, int32 y,
                            int32 width, int32 height, DWORD nativeColor) noexcept
    {
        if (!pSurface || width <= 0 || height <= 0) return;
        Rectangle rect(x, y, width, height);
        pSurface->FillRectEx(nullptr, &rect, nativeColor);
    }

    // Draw a single-pixel-wide horizontal line.
    void DrawHLine(Surface* pSurface, int32 x1, int32 x2, int32 y,
                   DWORD nativeColor) noexcept
    {
        if (!pSurface || x2 < x1) return;
        Rectangle rect(x1, y, x2 - x1 + 1, 1);
        pSurface->FillRectEx(nullptr, &rect, nativeColor);
    }

    // Draw a single-pixel-wide vertical line.
    void DrawVLine(Surface* pSurface, int32 x, int32 y1, int32 y2,
                   DWORD nativeColor) noexcept
    {
        if (!pSurface || y2 < y1) return;
        Rectangle rect(x, y1, 1, y2 - y1 + 1);
        pSurface->FillRectEx(nullptr, &rect, nativeColor);
    }

    // Draw a 1-pixel border around the given rectangle.
    void DrawBorder(Surface* pSurface, const Rectangle& rect,
                    DWORD nativeColor) noexcept
    {
        if (!pSurface || rect.IsEmpty()) return;
        DrawHLine(pSurface, rect.X, rect.X + rect.Width - 1, rect.Y, nativeColor);
        DrawHLine(pSurface, rect.X, rect.X + rect.Width - 1, rect.Y + rect.Height - 1, nativeColor);
        DrawVLine(pSurface, rect.X, rect.Y, rect.Y + rect.Height - 1, nativeColor);
        DrawVLine(pSurface, rect.X + rect.Width - 1, rect.Y, rect.Y + rect.Height - 1, nativeColor);
    }

    // Draw a 3D bevel (raised or sunken) around the bar.
    void DrawBevel(Surface* pSurface, const Rectangle& rect, bool raised,
                   const ColorStruct& light, const ColorStruct& dark) noexcept
    {
        if (!pSurface || rect.Width < 2 || rect.Height < 2) return;
        DWORD lightColor = ColorToNative(pSurface, light);
        DWORD darkColor  = ColorToNative(pSurface, dark);

        if (raised)
        {
            // Top and left edges are light; bottom and right are dark.
            DrawHLine(pSurface, rect.X, rect.X + rect.Width - 1, rect.Y, lightColor);
            DrawVLine(pSurface, rect.X, rect.Y, rect.Y + rect.Height - 1, lightColor);
            DrawHLine(pSurface, rect.X, rect.X + rect.Width - 1, rect.Y + rect.Height - 1, darkColor);
            DrawVLine(pSurface, rect.X + rect.Width - 1, rect.Y, rect.Y + rect.Height - 1, darkColor);
        }
        else
        {
            // Sunken: top/left dark, bottom/right light.
            DrawHLine(pSurface, rect.X, rect.X + rect.Width - 1, rect.Y, darkColor);
            DrawVLine(pSurface, rect.X, rect.Y, rect.Y + rect.Height - 1, darkColor);
            DrawHLine(pSurface, rect.X, rect.X + rect.Width - 1, rect.Y + rect.Height - 1, lightColor);
            DrawVLine(pSurface, rect.X + rect.Width - 1, rect.Y, rect.Y + rect.Height - 1, lightColor);
        }
    }

    // ── Percentage text formatting ───────────────────────────────────────

    // Format the percentage as a wide-character string: "NNN%".
    void FormatPercentage(wchar_t* buf, int32 bufSize, int32 percentage) noexcept
    {
        if (percentage < 0) percentage = 0;
        if (percentage > 999) percentage = 999;
        int32 len = swprintf(buf, static_cast<size_t>(bufSize), L"%d%%", percentage);
        if (len < 0 && bufSize > 0) buf[0] = L'\0';
    }

    // ── Style-specific renderers ─────────────────────────────────────────

    // Render a solid-fill progress bar.
    void DrawSolid(Surface* pSurface, const Rectangle& fillRect,
                   float progress, DWORD fillColor) noexcept
    {
        if (!pSurface || fillRect.Width <= 0 || fillRect.Height <= 0) return;
        int32 fillWidth = static_cast<int32>(progress * static_cast<float>(fillRect.Width) + 0.5f);
        if (fillWidth <= 0) return;
        if (fillWidth > fillRect.Width) fillWidth = fillRect.Width;
        Rectangle filled(fillRect.X, fillRect.Y, fillWidth, fillRect.Height);
        pSurface->FillRectEx(nullptr, &filled, fillColor);
    }

    // Render a segmented progress bar (classic C&C style with discrete
    // blocks separated by 1-pixel gaps).
    void DrawSegmented(Surface* pSurface, const Rectangle& fillRect,
                       float progress, DWORD fillColor, DWORD bgColor,
                       int32 segmentWidth) noexcept
    {
        if (!pSurface || fillRect.Width <= 0 || fillRect.Height <= 0) return;
        if (segmentWidth < 2) segmentWidth = 6;

        int32 totalWidth = fillRect.Width;
        int32 segCount = totalWidth / segmentWidth;
        if (segCount <= 0) return;

        int32 litCount = static_cast<int32>(progress * static_cast<float>(segCount) + 0.5f);
        if (litCount > segCount) litCount = segCount;

        for (int32 i = 0; i < segCount; ++i)
        {
            int32 segX = fillRect.X + i * segmentWidth;
            int32 segW = segmentWidth - 1; // 1px gap
            if (segX + segW > fillRect.X + totalWidth)
                segW = fillRect.X + totalWidth - segX;
            if (segW <= 0) break;

            Rectangle segRect(segX, fillRect.Y, segW, fillRect.Height);
            DWORD color = (i < litCount) ? fillColor : bgColor;
            pSurface->FillRectEx(nullptr, &segRect, color);
        }
    }

    // Render a smooth gradient progress bar.  Each column is drawn with
    // an interpolated colour, producing a left-to-right gradient.
    void DrawSmooth(Surface* pSurface, const Rectangle& fillRect,
                    float progress, const ColorStruct& baseColor,
                    const ColorStruct& endColor) noexcept
    {
        if (!pSurface || fillRect.Width <= 0 || fillRect.Height <= 0) return;

        int32 fillWidth = static_cast<int32>(progress * static_cast<float>(fillRect.Width) + 0.5f);
        if (fillWidth <= 0) return;
        if (fillWidth > fillRect.Width) fillWidth = fillRect.Width;

        // Draw the gradient column by column.
        for (int32 x = 0; x < fillWidth; ++x)
        {
            float t = (fillWidth > 1)
                ? static_cast<float>(x) / static_cast<float>(fillWidth - 1)
                : 0.0f;
            ColorStruct col = LerpColor(baseColor, endColor, t);
            DWORD native = ColorToNative(pSurface, col);
            Rectangle colRect(fillRect.X + x, fillRect.Y, 1, fillRect.Height);
            pSurface->FillRectEx(nullptr, &colRect, native);
        }
    }

    // Render the marquee (indeterminate) bouncing block.
    void DrawMarquee(Surface* pSurface, const Rectangle& fillRect,
                     float marqueePos, DWORD fillColor) noexcept
    {
        if (!pSurface || fillRect.Width <= 0 || fillRect.Height <= 0) return;

        // The marquee block occupies 25% of the bar width.
        int32 blockWidth = fillRect.Width / 4;
        if (blockWidth < 4) blockWidth = 4;

        // marqueePos ranges from -1.0 to 1.0.  Map it to a pixel position.
        float normalized = marqueePos;
        if (normalized < -1.0f) normalized = -1.0f;
        if (normalized > 1.0f) normalized = 1.0f;

        // Centre the block around the position.
        int32 maxOffset = fillRect.Width - blockWidth;
        int32 blockX = fillRect.X + static_cast<int32>((normalized * 0.5f + 0.5f) * static_cast<float>(maxOffset) + 0.5f);
        if (blockX < fillRect.X) blockX = fillRect.X;
        if (blockX + blockWidth > fillRect.X + fillRect.Width)
            blockX = fillRect.X + fillRect.Width - blockWidth;

        Rectangle blockRect(blockX, fillRect.Y, blockWidth, fillRect.Height);
        pSurface->FillRectEx(nullptr, &blockRect, fillColor);
    }

    // Render the percentage text overlay centred in the bar.
    void DrawTextOverlay(Surface* pSurface, const Rectangle& barRect,
                         int32 percentage, DWORD textColor) noexcept
    {
        if (!pSurface) return;

        wchar_t textBuf[16];
        FormatPercentage(textBuf, 16, percentage);

        // Try to use DSurface::DrawText if the surface is a DSurface.
        // The primary rendering surface in the game is always a DSurface.
        DSurface* pDSurface = static_cast<DSurface*>(pSurface);
        Point2D location(barRect.CenterX(), barRect.CenterY());
        Rectangle bounds(barRect);
        pDSurface->DrawText(textBuf, &bounds, &location,
                           textColor, 0, TextPrintType::Center);
    }

} // anonymous namespace

// ============================================================================
// Construction
// ============================================================================

ProgressBarClass::ProgressBarClass() noexcept
    : GadgetClass()
    , MinValue(0)
    , MaxValue(100)
    , CurrentValue(0)
    , DisplayValue(0.0f)
    , AnimSpeed(0.0f)
    , ShowText(false)
    , BarStyle(ProgressBarStyle::Solid)
    , FillColor(0, 200, 0)
    , BgColor(40, 40, 40)
    , BorderColor(80, 80, 80)
    , MarqueePos(0.0f)
    , MarqueeSpeed(0.02f)
{
}

ProgressBarClass::ProgressBarClass(int32 x, int32 y, int32 w, int32 h,
                                   int32 minVal, int32 maxVal, int32 id) noexcept
    : GadgetClass(x, y, w, h, id)
    , MinValue(minVal)
    , MaxValue(maxVal)
    , CurrentValue(minVal)
    , DisplayValue(static_cast<float>(minVal))
    , AnimSpeed(0.0f)
    , ShowText(false)
    , BarStyle(ProgressBarStyle::Solid)
    , FillColor(0, 200, 0)
    , BgColor(40, 40, 40)
    , BorderColor(80, 80, 80)
    , MarqueePos(0.0f)
    , MarqueeSpeed(0.02f)
{
}

// ============================================================================
// Destruction
// ============================================================================

ProgressBarClass::~ProgressBarClass()
{
}

// ============================================================================
// Range management
// ============================================================================

void ProgressBarClass::SetRange(int32 minVal, int32 maxVal) noexcept
{
    if (minVal > maxVal)
    {
        int32 tmp = minVal;
        minVal = maxVal;
        maxVal = tmp;
    }
    MinValue = minVal;
    MaxValue = maxVal;

    // Clamp current value to the new range.
    if (CurrentValue < MinValue) CurrentValue = MinValue;
    if (CurrentValue > MaxValue) CurrentValue = MaxValue;

    // Clamp the display value as well.
    if (DisplayValue < static_cast<float>(MinValue))
        DisplayValue = static_cast<float>(MinValue);
    if (DisplayValue > static_cast<float>(MaxValue))
        DisplayValue = static_cast<float>(MaxValue);

    SetNeedsRedraw(true);
}

// ============================================================================
// Value management
// ============================================================================

void ProgressBarClass::SetValue(int32 value) noexcept
{
    if (value < MinValue) value = MinValue;
    if (value > MaxValue) value = MaxValue;

    if (CurrentValue == value)
        return;

    CurrentValue = value;

    // If animation is disabled, snap the display value.
    if (AnimSpeed <= 0.0f)
    {
        DisplayValue = static_cast<float>(CurrentValue);
    }

    SetNeedsRedraw(true);
}

float ProgressBarClass::GetProgress() const noexcept
{
    int32 range = MaxValue - MinValue;
    if (range <= 0)
        return 0.0f;
    return static_cast<float>(CurrentValue - MinValue) / static_cast<float>(range);
}

int32 ProgressBarClass::GetPercentage() const noexcept
{
    return static_cast<int32>(GetProgress() * 100.0f + 0.5f);
}

// ============================================================================
// Animation
// ============================================================================

void ProgressBarClass::AnimateTo(int32 targetValue) noexcept
{
    if (targetValue < MinValue) targetValue = MinValue;
    if (targetValue > MaxValue) targetValue = MaxValue;

    // If animation speed is zero, enable a default speed so AnimateTo
    // actually produces visible motion.
    if (AnimSpeed <= 0.0f)
        AnimSpeed = 0.15f;

    CurrentValue = targetValue;
    // DisplayValue will catch up in Update().
    SetNeedsRedraw(true);
}

bool ProgressBarClass::IsAnimating() const noexcept
{
    if (AnimSpeed <= 0.0f)
        return false;
    return static_cast<int32>(DisplayValue) != CurrentValue;
}

// ============================================================================
// Marquee mode
// ============================================================================

void ProgressBarClass::SetMarquee(bool enable) noexcept
{
    if (enable)
    {
        BarStyle = ProgressBarStyle::Marquee;
        MarqueePos = 0.0f;
    }
    else
    {
        BarStyle = ProgressBarStyle::Solid;
    }
    SetNeedsRedraw(true);
}

// ============================================================================
// Update
//
//  Called every frame.  Handles animated value interpolation and marquee
//  movement.  Returns true if the gadget needs to be redrawn.
// ============================================================================

bool ProgressBarClass::Update()
{
    bool needsRedraw = false;

    // Animate the display value towards the current value using
    // exponential interpolation (frame-rate-independent ease).
    if (AnimSpeed > 0.0f && static_cast<int32>(DisplayValue) != CurrentValue)
    {
        float diff = static_cast<float>(CurrentValue) - DisplayValue;
        DisplayValue += diff * AnimSpeed;

        // Snap when close enough to avoid infinite micro-adjustments.
        float absDiff = diff < 0.0f ? -diff : diff;
        if (absDiff < 0.5f)
        {
            DisplayValue = static_cast<float>(CurrentValue);
        }
        needsRedraw = true;
    }

    // Marquee animation: bounce the block back and forth.
    if (BarStyle == ProgressBarStyle::Marquee)
    {
        MarqueePos += MarqueeSpeed;

        // Bounce at the edges.  MarqueePos goes from 0 to 1, then reverses
        // to -1, then back to 0, creating a smooth left-right sweep.
        if (MarqueePos > 1.0f)
        {
            MarqueePos = 1.0f;
            MarqueeSpeed = -MarqueeSpeed;
        }
        else if (MarqueePos < -1.0f)
        {
            MarqueePos = -1.0f;
            MarqueeSpeed = -MarqueeSpeed;
        }
        needsRedraw = true;
    }

    // If the gadget is hovered or pressed, request a redraw for visual
    // feedback (e.g. highlight pulse).
    if (IsHovered() || IsPressed())
    {
        needsRedraw = true;
    }

    return needsRedraw;
}

// ============================================================================
// Drawing
//
//  The Draw method fills the background, draws the fill bar (or marquee
//  block) and optionally renders the percentage text.  The actual pixel
//  rendering is done by the Surface interface; this implementation
//  computes the geometry and selects colours.
// ============================================================================

void ProgressBarClass::Draw(Surface* pSurface)
{
    if (!pSurface)
        return;

    if (!IsVisible())
        return;

    // ── Compute the bar geometry ──────────────────────────────────────
    //
    //  The gadget's Rect defines the outer bounds.  We inset by 1 pixel
    //  on each side for the border, giving us the fill area.

    Rectangle outerRect(GetX(), GetY(), GetWidth(), GetHeight());
    if (outerRect.Width < 4 || outerRect.Height < 4)
        return; // Too small to render meaningfully.

    Rectangle fillRect(outerRect.X + 1, outerRect.Y + 1,
                       outerRect.Width - 2, outerRect.Height - 2);

    // ── Compute progress based on the animated display value ─────────
    //
    //  When animation is enabled, the bar visually follows DisplayValue
    //  rather than CurrentValue, producing a smooth fill effect.

    float displayProgress = 0.0f;
    if (BarStyle != ProgressBarStyle::Marquee)
    {
        int32 range = MaxValue - MinValue;
        if (range > 0)
        {
            float dv = DisplayValue;
            if (dv < static_cast<float>(MinValue)) dv = static_cast<float>(MinValue);
            if (dv > static_cast<float>(MaxValue)) dv = static_cast<float>(MaxValue);
            displayProgress = (dv - static_cast<float>(MinValue)) / static_cast<float>(range);
        }
    }

    // ── Determine fill colour ─────────────────────────────────────────
    //
    //  Use the explicit FillColor if it differs from the default green.
    //  Otherwise, map progress to a red-yellow-green ramp for an
    //  intuitive "health bar" appearance.

    ColorStruct effectiveFill = FillColor;

    // Check if FillColor is still the constructor default (green).
    // If so, apply progress-based colour mapping.
    if (FillColor.R == 0 && FillColor.G == 200 && FillColor.B == 0)
    {
        effectiveFill = ProgressToColor(displayProgress);
    }

    // When the gadget is disabled, desaturate the colours.
    if (!IsEnabled())
    {
        int32 avg = (effectiveFill.R + effectiveFill.G + effectiveFill.B) / 3;
        effectiveFill = ColorStruct(static_cast<uint8>(avg),
                                    static_cast<uint8>(avg),
                                    static_cast<uint8>(avg));
    }

    // When hovered, brighten the fill slightly.
    if (IsHovered() && IsEnabled())
    {
        int32 r = effectiveFill.R + 30;  if (r > 255) r = 255;
        int32 g = effectiveFill.G + 30;  if (g > 255) g = 255;
        int32 b = effectiveFill.B + 30;  if (b > 255) b = 255;
        effectiveFill = ColorStruct(static_cast<uint8>(r),
                                    static_cast<uint8>(g),
                                    static_cast<uint8>(b));
    }

    // ── Draw the background ───────────────────────────────────────────
    DWORD bgColor = ColorToNative(pSurface, BgColor);
    pSurface->FillRectEx(nullptr, &fillRect, bgColor);

    // ── Draw the fill based on style ──────────────────────────────────
    DWORD fillColor = ColorToNative(pSurface, effectiveFill);

    switch (BarStyle)
    {
        case ProgressBarStyle::Solid:
        {
            DrawSolid(pSurface, fillRect, displayProgress, fillColor);
            break;
        }

        case ProgressBarStyle::Segmented:
        {
            // Segment width scales with bar width; minimum 4px, max 12px.
            int32 segW = fillRect.Width / 10;
            if (segW < 4) segW = 4;
            if (segW > 12) segW = 12;
            DrawSegmented(pSurface, fillRect, displayProgress,
                          fillColor, bgColor, segW);
            break;
        }

        case ProgressBarStyle::Smooth:
        {
            // Gradient from the fill colour to a lighter variant.
            ColorStruct lightFill = LerpColor(effectiveFill,
                                              ColorStruct(255, 255, 255), 0.4f);
            DrawSmooth(pSurface, fillRect, displayProgress,
                       effectiveFill, lightFill);
            break;
        }

        case ProgressBarStyle::Marquee:
        {
            DrawMarquee(pSurface, fillRect, MarqueePos, fillColor);
            break;
        }
    }

    // ── Draw the border ───────────────────────────────────────────────
    DWORD borderColor = ColorToNative(pSurface, BorderColor);
    DrawBorder(pSurface, outerRect, borderColor);

    // ── Draw a 3D bevel for a raised look ─────────────────────────────
    ColorStruct bevelLight(255, 255, 255);
    ColorStruct bevelDark (0,   0,   0);
    if (!IsEnabled())
    {
        bevelLight = ColorStruct(120, 120, 120);
        bevelDark  = ColorStruct(60,  60,  60);
    }
    DrawBevel(pSurface, outerRect, true, bevelLight, bevelDark);

    // ── Draw the percentage text overlay ──────────────────────────────
    if (ShowText && BarStyle != ProgressBarStyle::Marquee)
    {
        // Choose a text colour that contrasts with the fill.
        // If the bar is more than half full, use dark text on the fill;
        // otherwise use light text on the background.
        DWORD textColor;
        if (displayProgress > 0.5f)
        {
            textColor = ColorToNative(pSurface, ColorStruct(20, 20, 20));
        }
        else
        {
            textColor = ColorToNative(pSurface, ColorStruct(240, 240, 240));
        }

        int32 pct = static_cast<int32>(displayProgress * 100.0f + 0.5f);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        DrawTextOverlay(pSurface, fillRect, pct, textColor);
    }

    // ── Draw marquee text ("...") ─────────────────────────────────────
    if (ShowText && BarStyle == ProgressBarStyle::Marquee)
    {
        DWORD textColor = ColorToNative(pSurface, ColorStruct(240, 240, 240));
        DSurface* pDSurface = static_cast<DSurface*>(pSurface);
        Point2D location(fillRect.CenterX(), fillRect.CenterY());
        Rectangle bounds(fillRect);
        pDSurface->DrawText(L"...", &bounds, &location,
                           textColor, 0, TextPrintType::Center);
    }

    SetNeedsRedraw(false);
}
