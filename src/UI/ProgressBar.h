#pragma once

// ============================================================================
// ProgressBar.h - Progress bar widget
//
//  ProgressBarClass displays a horizontal bar that fills from left to right
//  to indicate the progress of a long-running operation (loading, saving,
//  downloading, etc.).  Features:
//    * Configurable min/max range
//    * Optional percentage text overlay
//    * Customizable fill and background colors
//    * Smooth or stepped animation
// ============================================================================

#include <UI/Gadget.h>
#include <Core/Definitions.h>
#include <Core/Macros.h>

#include <cstdint>

// ============================================================================
// Forward declarations
// ============================================================================

class Surface;

// ============================================================================
// ProgressBarStyle - visual style of the progress bar
// ============================================================================

enum class ProgressBarStyle : int32
{
    Solid       = 0,   // Single solid fill color
    Segmented   = 1,   // Discrete segments (classic C&C style)
    Smooth      = 2,   // Smooth gradient fill
    Marquee     = 3,   // Indeterminate (bouncing block)
};

// ============================================================================
// ProgressBarClass - horizontal progress indicator
// ============================================================================

class ProgressBarClass : public GadgetClass
{
public:
    // ── Construction / Destruction ──────────────────────────────────────

    ProgressBarClass() noexcept;
    ProgressBarClass(int32 x, int32 y, int32 w, int32 h,
                     int32 minVal = 0, int32 maxVal = 100,
                     int32 id = 0) noexcept;
    virtual ~ProgressBarClass();

    // ── GadgetClass overrides ───────────────────────────────────────────

    virtual void Draw(Surface* pSurface) override;
    virtual bool Update() override;

    // ── Range ───────────────────────────────────────────────────────────

    void SetRange(int32 minVal, int32 maxVal) noexcept;
    int32 GetMin() const noexcept { return MinValue; }
    int32 GetMax() const noexcept { return MaxValue; }

    // ── Value ───────────────────────────────────────────────────────────

    void SetValue(int32 value) noexcept;
    int32 GetValue() const noexcept { return CurrentValue; }

    // Returns the progress as a float in [0.0, 1.0].
    float GetProgress() const noexcept;

    // Returns the percentage (0..100).
    int32 GetPercentage() const noexcept;

    // ── Animated value ──────────────────────────────────────────────────

    // Animate towards the target value.  When animated, the displayed
    // value smoothly approaches the actual value over several frames.
    void AnimateTo(int32 targetValue) noexcept;
    void SetAnimationSpeed(float speed) noexcept { AnimSpeed = speed; }
    bool IsAnimating() const noexcept;

    // ── Display options ─────────────────────────────────────────────────

    void SetShowPercentage(bool show) noexcept { ShowText = show; SetNeedsRedraw(true); }
    bool GetShowPercentage() const noexcept { return ShowText; }

    void SetStyle(ProgressBarStyle style) noexcept { BarStyle = style; SetNeedsRedraw(true); }
    ProgressBarStyle GetStyle() const noexcept { return BarStyle; }

    // ── Colors ──────────────────────────────────────────────────────────

    void SetFillColor(const ColorStruct& color) noexcept { FillColor = color; SetNeedsRedraw(true); }
    void SetBackgroundColor(const ColorStruct& color) noexcept { BgColor = color; SetNeedsRedraw(true); }
    void SetBorderColor(const ColorStruct& color) noexcept { BorderColor = color; SetNeedsRedraw(true); }

    const ColorStruct& GetFillColor() const noexcept { return FillColor; }
    const ColorStruct& GetBackgroundColor() const noexcept { return BgColor; }
    const ColorStruct& GetBorderColor() const noexcept { return BorderColor; }

    // ── Marquee (indeterminate) mode ────────────────────────────────────

    void SetMarquee(bool enable) noexcept;
    bool IsMarquee() const noexcept { return BarStyle == ProgressBarStyle::Marquee; }

private:
    int32             MinValue;
    int32             MaxValue;
    int32             CurrentValue;     // The actual value
    float             DisplayValue;     // The animated display value
    float             AnimSpeed;        // Animation speed (0=instant, 1=slow)
    bool              ShowText;         // Show percentage text
    ProgressBarStyle  BarStyle;

    ColorStruct       FillColor;
    ColorStruct       BgColor;
    ColorStruct       BorderColor;

    // Marquee state.
    float             MarqueePos;       // Position of the bouncing block [0..1]
    float             MarqueeSpeed;     // Speed of the marquee block
};
