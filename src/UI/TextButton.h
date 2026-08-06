#pragma once

// ============================================================================
// TextButton.h - Text button widget
//
//  TextButtonClass is a simple push-button that displays a text label.
//  It fires a callback when clicked.  The button supports:
//    * Normal, pressed, hovered and disabled visual states
//    * Custom foreground/background colors per state
//    * Keyboard activation (Space/Enter when focused)
//    * Auto-repeat when held (optional)
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
// ButtonState - visual state of the button
// ============================================================================

enum class ButtonVisualState : int32
{
    Normal   = 0,
    Hovered  = 1,
    Pressed  = 2,
    Disabled = 3,
    Focused  = 4,
};

// ============================================================================
// TextButtonClass - a push button with a text label
// ============================================================================

class TextButtonClass : public GadgetClass
{
public:
    // ── Construction / Destruction ──────────────────────────────────────

    TextButtonClass() noexcept;
    TextButtonClass(int32 x, int32 y, int32 w, int32 h,
                    const char* pText = "", int32 id = 0) noexcept;
    virtual ~TextButtonClass();

    // ── GadgetClass overrides ───────────────────────────────────────────

    virtual void Draw(Surface* pSurface) override;
    virtual bool HandleEvent(const GadgetEvent& event) override;
    virtual void OnPress() override;
    virtual void OnRelease() override;
    virtual void OnClick() override;

    // ── Text ────────────────────────────────────────────────────────────

    void SetText(const char* pText) noexcept;
    const char* GetText() const noexcept { return Text; }

    // ── Visual state ────────────────────────────────────────────────────

    ButtonVisualState GetVisualState() const noexcept;

    // ── Colors per state ────────────────────────────────────────────────

    void SetNormalColor(const ColorStruct& color) noexcept   { NormalColor = color; SetNeedsRedraw(true); }
    void SetHoverColor(const ColorStruct& color) noexcept    { HoverColor = color; SetNeedsRedraw(true); }
    void SetPressedColor(const ColorStruct& color) noexcept  { PressedColor = color; SetNeedsRedraw(true); }
    void SetDisabledColor(const ColorStruct& color) noexcept { DisabledColor = color; SetNeedsRedraw(true); }

    void SetNormalBackColor(const ColorStruct& color) noexcept   { NormalBg = color; SetNeedsRedraw(true); }
    void SetHoverBackColor(const ColorStruct& color) noexcept    { HoverBg = color; SetNeedsRedraw(true); }
    void SetPressedBackColor(const ColorStruct& color) noexcept  { PressedBg = color; SetNeedsRedraw(true); }
    void SetDisabledBackColor(const ColorStruct& color) noexcept { DisabledBg = color; SetNeedsRedraw(true); }

    // ── Auto-repeat ────────────────────────────────────────────────────

    void SetAutoRepeat(bool enable, int32 initialDelay = 30,
                       int32 repeatDelay = 8) noexcept;

    bool IsAutoRepeat() const noexcept { return AutoRepeatEnabled; }

    // ── Font ────────────────────────────────────────────────────────────

    void SetFontSize(int32 size) noexcept { FontSize = size; SetNeedsRedraw(true); }
    int32 GetFontSize() const noexcept { return FontSize; }

    // ── Text alignment ──────────────────────────────────────────────────

    enum class TextAlign : int32
    {
        Left   = 0,
        Center = 1,
        Right  = 2,
    };

    void SetTextAlign(TextAlign align) noexcept { Alignment = align; SetNeedsRedraw(true); }
    TextAlign GetTextAlign() const noexcept { return Alignment; }

private:
    // ── Data ────────────────────────────────────────────────────────────

    static constexpr int32 MaxTextLen = 64;

    char              Text[MaxTextLen];
    int32             FontSize;
    TextAlign         Alignment;

    // Per-state colors.
    ColorStruct       NormalColor;
    ColorStruct       HoverColor;
    ColorStruct       PressedColor;
    ColorStruct       DisabledColor;
    ColorStruct       NormalBg;
    ColorStruct       HoverBg;
    ColorStruct       PressedBg;
    ColorStruct       DisabledBg;

    // Auto-repeat.
    bool              AutoRepeatEnabled;
    int32             AutoRepeatInitialDelay;  // Frames before first repeat
    int32             AutoRepeatRepeatDelay;   // Frames between repeats
    int32             AutoRepeatCounter;       // Current repeat counter
    bool              AutoRepeatArmed;         // True after initial delay expires
};
