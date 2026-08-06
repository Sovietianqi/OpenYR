#pragma once

// ============================================================================
// Slider.h - Slider widget
//
//  SliderClass is a horizontal trackbar that lets the user select a value
//  from a range by dragging a thumb along a track.  Features:
//    * Configurable min/max range and step size
//    * Click-to-jump or drag-to-move thumb behavior
//    * Optional tick marks
//    * Keyboard support (Left/Right to step, Home/End for min/max)
//    * Callback notification when the value changes
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
// SliderStyle - visual style of the slider
// ============================================================================

enum class SliderStyle : int32
{
    Horizontal  = 0,   // Horizontal trackbar (left = min, right = max)
    Vertical    = 1,   // Vertical trackbar (bottom = min, top = max)
};

// ============================================================================
// SliderClass - draggable value selector
// ============================================================================

class SliderClass : public GadgetClass
{
public:
    // ── Construction / Destruction ──────────────────────────────────────

    SliderClass() noexcept;
    SliderClass(int32 x, int32 y, int32 w, int32 h,
                int32 minVal = 0, int32 maxVal = 100,
                int32 id = 0) noexcept;
    virtual ~SliderClass();

    // ── GadgetClass overrides ───────────────────────────────────────────

    virtual void Draw(Surface* pSurface) override;
    virtual bool HandleEvent(const GadgetEvent& event) override;
    virtual void OnPress() override;
    virtual void OnRelease() override;

    // ── Range ───────────────────────────────────────────────────────────

    void SetRange(int32 minVal, int32 maxVal) noexcept;
    int32 GetMin() const noexcept { return MinValue; }
    int32 GetMax() const noexcept { return MaxValue; }

    // ── Value ───────────────────────────────────────────────────────────

    void SetValue(int32 value) noexcept;
    int32 GetValue() const noexcept { return CurrentValue; }

    // Returns the value as a float in [0.0, 1.0].
    float GetNormalized() const noexcept;

    // ── Step ────────────────────────────────────────────────────────────

    void SetStep(int32 step) noexcept { StepSize = step; }
    int32 GetStep() const noexcept { return StepSize; }

    // Move the value by one step in the given direction.
    void StepUp() noexcept;
    void StepDown() noexcept;

    // ── Style ───────────────────────────────────────────────────────────

    void SetStyle(SliderStyle style) noexcept { SliderOrientation = style; SetNeedsRedraw(true); }
    SliderStyle GetStyle() const noexcept { return SliderOrientation; }

    void SetShowTicks(bool show) noexcept { ShowTicks = show; SetNeedsRedraw(true); }
    bool GetShowTicks() const noexcept { return ShowTicks; }

    void SetTickCount(int32 count) noexcept { TickCount = count; SetNeedsRedraw(true); }
    int32 GetTickCount() const noexcept { return TickCount; }

    // ── Colors ──────────────────────────────────────────────────────────

    void SetThumbColor(const ColorStruct& color) noexcept { ThumbColor = color; SetNeedsRedraw(true); }
    void SetTrackColor(const ColorStruct& color) noexcept { TrackColor = color; SetNeedsRedraw(true); }
    void SetFillColor(const ColorStruct& color) noexcept { FillColor = color; SetNeedsRedraw(true); }

    const ColorStruct& GetThumbColor() const noexcept { return ThumbColor; }
    const ColorStruct& GetTrackColor() const noexcept { return TrackColor; }
    const ColorStruct& GetFillColor() const noexcept { return FillColor; }

    // ── Thumb geometry ──────────────────────────────────────────────────

    // Returns the thumb's pixel position along the track.
    int32 GetThumbPosition() const noexcept;

    // Returns the thumb's pixel size.
    int32 GetThumbSize() const noexcept { return ThumbSize; }
    void  SetThumbSize(int32 size) noexcept { ThumbSize = size; SetNeedsRedraw(true); }

private:
    // ── Internal helpers ────────────────────────────────────────────────

    // Convert a pixel position to a value.
    int32 PixelToValue(int32 pixel) const noexcept;

    // Convert a value to a pixel position.
    int32 ValueToPixel(int32 value) const noexcept;

    // Update the value from a mouse position.
    void UpdateFromMouse(int32 mouseX, int32 mouseY) noexcept;

    // ── Data ────────────────────────────────────────────────────────────

    int32       MinValue;
    int32       MaxValue;
    int32       CurrentValue;
    int32       StepSize;
    SliderStyle SliderOrientation;
    bool        ShowTicks;
    int32       TickCount;
    int32       ThumbSize;

    // Dragging state.
    bool        IsDragging;
    int32       DragOffset;     // Offset from thumb center to mouse

    // Colors.
    ColorStruct ThumbColor;
    ColorStruct TrackColor;
    ColorStruct FillColor;
};
