#pragma once

// ============================================================================
// Gadget.h - Base UI gadget class
//
//  GadgetClass is the root of the UI widget hierarchy in Yuri's Revenge.
//  Every on-screen interactive element (buttons, sliders, progress bars,
//  list boxes, message boxes) derives from Gadget.
//
//  Key features:
//    * Position and size (RectangleStruct)
//    * Visible / enabled / focused state
//    * Parent-child hierarchy for nested layouts
//    * Virtual drawing and input-handling interface
//    * ID-based callback routing for click/keypress events
//    * Z-order management for overlapping gadgets
//
//  The original binary uses a vtable-based dispatch; this reconstruction
//  uses C++ virtual functions for the same purpose.
// ============================================================================

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <Containers/DynamicVectorClass.h>

#include <cstdint>

// ============================================================================
// Forward declarations
// ============================================================================

class GadgetClass;
class Surface;
class MouseClass;

// ============================================================================
// GadgetState - bitfield of per-gadget state flags
// ============================================================================

enum class GadgetState : uint32
{
    None        = 0x00000000,
    Visible     = 0x00000001,   // Gadget is drawn on screen
    Enabled     = 0x00000002,   // Gadget accepts input
    Focused     = 0x00000004,   // Gadget has keyboard focus
    Pressed     = 0x00000008,   // Gadget is being pressed (mouse down)
    Hovered     = 0x00000010,   // Mouse is over the gadget
    Checked     = 0x00000020,   // Gadget is in a checked/toggled state
    Sticky      = 0x00000040,   // Gadget stays pressed until toggled off
    Disabled    = 0x00000080,   // Gadget is explicitly disabled
    NeedsRedraw = 0x00000100,   // Gadget needs to be redrawn
    Modal       = 0x00000200,   // Gadget blocks input to others (modal dialog)
    Hidden      = 0x00000400,   // Gadget is hidden (not drawn, not interactive)
    Transparent = 0x00000800,   // Gadget background is transparent
    NoFocus     = 0x00001000,   // Gadget cannot receive focus
    TabStop     = 0x00002000,   // Gadget is a tab-navigation stop
    Default     = 0x00004000,   // Gadget is the default action (Enter key)
    Cancel      = 0x00008000,   // Gadget is the cancel action (Esc key)
};

inline GadgetState operator|(GadgetState a, GadgetState b) noexcept
{
    return static_cast<GadgetState>(static_cast<uint32>(a) | static_cast<uint32>(b));
}
inline GadgetState operator&(GadgetState a, GadgetState b) noexcept
{
    return static_cast<GadgetState>(static_cast<uint32>(a) & static_cast<uint32>(b));
}
inline GadgetState operator~(GadgetState a) noexcept
{
    return static_cast<GadgetState>(~static_cast<uint32>(a));
}

// ============================================================================
// GadgetEvent - input event passed to gadget handlers
// ============================================================================

enum class GadgetEventType : int32
{
    None        = 0,
    MouseMove   = 1,
    MouseDown   = 2,
    MouseUp     = 3,
    MouseClick  = 4,
    MouseDblClick = 5,
    KeyDown     = 6,
    KeyUp       = 7,
    KeyChar     = 8,
    Focus       = 9,
    Blur        = 10,
    MouseWheel  = 11,
    MouseEnter  = 12,
    MouseLeave  = 13,
};

struct GadgetEvent
{
    GadgetEventType  Type;
    Point2D          MousePos;      // Mouse position relative to gadget
    int32            Key;           // Virtual key code (for key events)
    int32            WheelDelta;    // Scroll amount (for wheel events)
    uint32           Modifiers;     // Shift/Ctrl/Alt modifier flags
};

// ============================================================================
// GadgetCallback - function pointer for event callbacks
// ============================================================================

using GadgetCallback = void (*)(GadgetClass* pGadget, int32 actionID, void* pUserData);

// ============================================================================
// GadgetClass - base UI widget
// ============================================================================

class NOVTABLE GadgetClass
{
public:
    // ── Construction / Destruction ──────────────────────────────────────

    GadgetClass() noexcept;
    GadgetClass(int32 x, int32 y, int32 w, int32 h, int32 id = 0) noexcept;
    virtual ~GadgetClass();

    // ── Virtual interface ───────────────────────────────────────────────

    // Draw the gadget onto the supplied surface.  The surface clip rect
    // is set to the gadget's bounds before this is called.
    virtual void Draw(Surface* pSurface) {}

    // Called every frame to update the gadget's state.  Returns true if
    // the gadget needs to be redrawn.
    virtual bool Update() { return false; }

    // Handle an input event.  Returns true if the event was consumed.
    virtual bool HandleEvent(const GadgetEvent& event) { (void)event; return false; }

    // Called when the gadget receives focus.
    virtual void OnFocus() {}

    // Called when the gadget loses focus.
    virtual void OnBlur() {}

    // Called when the gadget is pressed (mouse down inside bounds).
    virtual void OnPress() {}

    // Called when the gadget is released (mouse up inside bounds).
    virtual void OnRelease() {}

    // Called when the gadget is clicked (press + release inside bounds).
    virtual void OnClick() {}

    // ── Position / size ─────────────────────────────────────────────────

    const RectangleStruct& GetRect() const noexcept { return Rect; }

    int32 GetX() const noexcept { return Rect.X; }
    int32 GetY() const noexcept { return Rect.Y; }
    int32 GetWidth() const noexcept { return Rect.Width; }
    int32 GetHeight() const noexcept { return Rect.Height; }

    void SetPosition(int32 x, int32 y) noexcept;
    void SetSize(int32 w, int32 h) noexcept;
    void SetRect(int32 x, int32 y, int32 w, int32 h) noexcept;

    // True if the point is inside the gadget's bounds.
    bool Contains(int32 x, int32 y) const noexcept;
    bool Contains(const Point2D& pt) const noexcept { return Contains(pt.X, pt.Y); }

    // ── State ───────────────────────────────────────────────────────────

    bool HasState(GadgetState flag) const noexcept
    {
        return (static_cast<uint32>(State) & static_cast<uint32>(flag)) != 0;
    }

    void SetState(GadgetState flag) noexcept
    {
        State = State | flag;
    }

    void ClearState(GadgetState flag) noexcept
    {
        State = State & ~flag;
    }

    void ToggleState(GadgetState flag) noexcept
    {
        State = static_cast<GadgetState>(
            static_cast<uint32>(State) ^ static_cast<uint32>(flag));
    }

    // Convenience accessors
    bool IsVisible() const noexcept  { return HasState(GadgetState::Visible) && !HasState(GadgetState::Hidden); }
    bool IsEnabled() const noexcept  { return HasState(GadgetState::Enabled) && !HasState(GadgetState::Disabled); }
    bool IsFocused() const noexcept  { return HasState(GadgetState::Focused); }
    bool IsPressed() const noexcept  { return HasState(GadgetState::Pressed); }
    bool IsHovered() const noexcept  { return HasState(GadgetState::Hovered); }
    bool NeedsRedraw() const noexcept { return HasState(GadgetState::NeedsRedraw); }

    void SetVisible(bool v)   { v ? SetState(GadgetState::Visible) : ClearState(GadgetState::Visible); }
    void SetEnabled(bool v)   { v ? SetState(GadgetState::Enabled) : ClearState(GadgetState::Enabled); }
    void SetDisabled(bool v)  { v ? SetState(GadgetState::Disabled) : ClearState(GadgetState::Disabled); }
    void SetHidden(bool v)    { v ? SetState(GadgetState::Hidden) : ClearState(GadgetState::Hidden); }
    void SetNeedsRedraw(bool v) { v ? SetState(GadgetState::NeedsRedraw) : ClearState(GadgetState::NeedsRedraw); }

    void Show()  { SetVisible(true); SetNeedsRedraw(true); }
    void Hide()  { SetVisible(false); }
    void Enable()  { SetEnabled(true); SetNeedsRedraw(true); }
    void Disable() { SetEnabled(false); SetNeedsRedraw(true); }

    // ── Focus management ────────────────────────────────────────────────

    void Focus()    { if (!HasState(GadgetState::NoFocus)) SetState(GadgetState::Focused); }
    void Unfocus()  { ClearState(GadgetState::Focused); }

    // ── ID ──────────────────────────────────────────────────────────────

    int32  GetID() const noexcept { return ID; }
    void   SetID(int32 id) noexcept { ID = id; }

    // ── Callback ────────────────────────────────────────────────────────

    void SetCallback(GadgetCallback callback, void* pUserData = nullptr) noexcept
    {
        Callback = callback;
        CallbackData = pUserData;
    }

    void InvokeCallback(int32 actionID) noexcept
    {
        if (Callback)
            Callback(this, actionID, CallbackData);
    }

    // ── Z-order ─────────────────────────────────────────────────────────

    int32 GetZOrder() const noexcept { return ZOrder; }
    void  SetZOrder(int32 z) noexcept { ZOrder = z; }

    // ── Color ───────────────────────────────────────────────────────────

    const ColorStruct& GetColor() const noexcept { return Color; }
    void SetColor(const ColorStruct& color) noexcept { Color = color; SetNeedsRedraw(true); }

    const ColorStruct& GetBackColor() const noexcept { return BackColor; }
    void SetBackColor(const ColorStruct& color) noexcept { BackColor = color; SetNeedsRedraw(true); }

    // ── Flag for dirty/redraw ───────────────────────────────────────────

    void MarkDirty() noexcept { SetNeedsRedraw(true); }

protected:
    RectangleStruct    Rect;            // Position and size
    GadgetState        State;           // Current state flags
    int32              ID;              // User-assigned identifier
    int32              ZOrder;          // Drawing order (higher = on top)
    ColorStruct        Color;           // Foreground color
    ColorStruct        BackColor;       // Background color
    GadgetCallback     Callback;        // Event callback
    void*              CallbackData;    // User data for callback

public:
    // Parent / children (public for simplicity; managed by container gadgets)
    GadgetClass*                        Parent;
    DynamicVectorClass<GadgetClass*>    Children;
};
