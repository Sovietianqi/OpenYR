#include <UI/TextButton.h>

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <Rendering/Surface.h>
#include <Math/Rectangle.h>

#include <cstring>
#include <cwchar>

// ============================================================================
// TextButton.cpp - Text button implementation
//
//  Implements the TextButtonClass member functions.  The button handles
//  mouse press/release/click and keyboard activation (Space/Enter).
//
//  Rendering features:
//    * State-dependent foreground/background colors
//    * 3D bevel (raised when normal, sunken when pressed)
//    * Left/center/right text alignment
//    * Auto-repeat timer for held-button continuous fire
//    * Keyboard activation (Space/Enter)
// ============================================================================

// ----------------------------------------------------------------------------
// File-local helpers
// ----------------------------------------------------------------------------

namespace
{
    // ── Colour conversion ──────────────────────────────────────────────

    DWORD ColorTo565(const ColorStruct& c) noexcept
    {
        uint16 r5 = static_cast<uint16>(c.R >> 3);
        uint16 g6 = static_cast<uint16>(c.G >> 2);
        uint16 b5 = static_cast<uint16>(c.B >> 3);
        return static_cast<DWORD>((r5 << 11) | (g6 << 5) | b5);
    }

    DWORD ColorToARGB(const ColorStruct& c) noexcept
    {
        return (static_cast<DWORD>(c.A) << 24) |
               (static_cast<DWORD>(c.R) << 16) |
               (static_cast<DWORD>(c.G) << 8)  |
               static_cast<DWORD>(c.B);
    }

    DWORD ColorToNative(Surface* pSurface, const ColorStruct& c) noexcept
    {
        if (!pSurface) return 0;
        int32 bpp = pSurface->GetBytesPerPixel();
        if (bpp == 2) return ColorTo565(c);
        if (bpp == 4) return ColorToARGB(c);
        return static_cast<DWORD>((c.R + c.G + c.B) / 3);
    }

    // ── Drawing primitives ─────────────────────────────────────────────

    void FillRect(Surface* pSurface, int32 x, int32 y, int32 w, int32 h,
                  DWORD color) noexcept
    {
        if (!pSurface || w <= 0 || h <= 0) return;
        Rectangle rect(x, y, w, h);
        pSurface->FillRectEx(nullptr, &rect, color);
    }

    void DrawHLine(Surface* pSurface, int32 x1, int32 x2, int32 y,
                   DWORD color) noexcept
    {
        if (!pSurface || x2 < x1) return;
        Rectangle rect(x1, y, x2 - x1 + 1, 1);
        pSurface->FillRectEx(nullptr, &rect, color);
    }

    void DrawVLine(Surface* pSurface, int32 x, int32 y1, int32 y2,
                   DWORD color) noexcept
    {
        if (!pSurface || y2 < y1) return;
        Rectangle rect(x, y1, 1, y2 - y1 + 1);
        pSurface->FillRectEx(nullptr, &rect, color);
    }

    // Draw a 3D bevel around the button.  When raised, the top/left edges
    // are light and the bottom/right edges are dark.  When sunken, the
    // colors are swapped.
    void DrawBevel(Surface* pSurface, int32 x, int32 y, int32 w, int32 h,
                   bool raised,
                   const ColorStruct& light, const ColorStruct& dark) noexcept
    {
        if (!pSurface || w < 2 || h < 2) return;
        DWORD lightColor = ColorToNative(pSurface, light);
        DWORD darkColor = ColorToNative(pSurface, dark);

        if (raised)
        {
            DrawHLine(pSurface, x, x + w - 1, y, lightColor);
            DrawVLine(pSurface, x, y, y + h - 1, lightColor);
            DrawHLine(pSurface, x, x + w - 1, y + h - 1, darkColor);
            DrawVLine(pSurface, x + w - 1, y, y + h - 1, darkColor);
        }
        else
        {
            DrawHLine(pSurface, x, x + w - 1, y, darkColor);
            DrawVLine(pSurface, x, y, y + h - 1, darkColor);
            DrawHLine(pSurface, x, x + w - 1, y + h - 1, lightColor);
            DrawVLine(pSurface, x + w - 1, y, y + h - 1, lightColor);
        }
    }

    // Draw a 2-pixel-wide bevel for a more pronounced 3D effect.
    void DrawDoubleBevel(Surface* pSurface, int32 x, int32 y, int32 w, int32 h,
                         bool raised,
                         const ColorStruct& light, const ColorStruct& dark) noexcept
    {
        if (!pSurface || w < 4 || h < 4) return;
        // Outer bevel.
        DrawBevel(pSurface, x, y, w, h, raised, light, dark);
        // Inner bevel (slightly darker/lighter).
        ColorStruct innerLight = light;
        ColorStruct innerDark = dark;
        // Dim the inner bevel colors by 50%.
        innerLight.R = static_cast<uint8>(innerLight.R / 2);
        innerLight.G = static_cast<uint8>(innerLight.G / 2);
        innerLight.B = static_cast<uint8>(innerLight.B / 2);
        innerDark.R = static_cast<uint8>(innerDark.R / 2 + 32);
        innerDark.G = static_cast<uint8>(innerDark.G / 2 + 32);
        innerDark.B = static_cast<uint8>(innerDark.B / 2 + 32);
        DrawBevel(pSurface, x + 1, y + 1, w - 2, h - 2, raised,
                  innerLight, innerDark);
    }

    // ── Text alignment calculation ─────────────────────────────────────
    //
    //  Computes the pixel position for the text based on the alignment
    //  setting and the estimated text width.

    Point2D ComputeTextPosition(int32 buttonX, int32 buttonY,
                                int32 buttonW, int32 buttonH,
                                int32 textWidth, int32 textHeight,
                                TextButtonClass::TextAlign align) noexcept
    {
        Point2D pos;
        pos.Y = buttonY + (buttonH - textHeight) / 2;

        switch (align)
        {
            case TextButtonClass::TextAlign::Left:
                pos.X = buttonX + 4; // 4px left padding
                break;
            case TextButtonClass::TextAlign::Right:
                pos.X = buttonX + buttonW - textWidth - 4; // 4px right padding
                break;
            case TextButtonClass::TextAlign::Center:
            default:
                pos.X = buttonX + (buttonW - textWidth) / 2;
                break;
        }
        return pos;
    }

    // Estimate the rendered width of a text string based on font size.
    // The original game uses a proportional font; this approximation
    // assumes an average character width of 60% of the font size.
    int32 EstimateTextWidth(const char* pText, int32 fontSize) noexcept
    {
        if (!pText || !pText[0])
            return 0;
        int32 len = static_cast<int32>(strlen(pText));
        return static_cast<int32>(len * fontSize * 0.6f);
    }

    // ── Auto-repeat processing ─────────────────────────────────────────
    //
    //  Called from Draw() each frame while the button is pressed.
    //  Decrements the auto-repeat counter and fires the callback when
    //  the counter reaches zero.  Returns true if a repeat event was fired.

    bool ProcessAutoRepeat(TextButtonClass* pButton) noexcept
    {
        if (!pButton || !pButton->IsAutoRepeat() || !pButton->IsPressed())
            return false;

        // Access the private auto-repeat fields through the public interface.
        // Since we can't access private members directly, we use the fact
        // that the Draw method is a member function and has access.
        // This function is called from within TextButtonClass::Draw, so
        // it operates on the button's state indirectly.
        //
        // The auto-repeat logic is implemented directly in Draw() below
        // because it needs access to private members.
        return false;
    }

    // ── Focus indicator drawing ────────────────────────────────────────
    //
    //  Draws a dotted rectangle inside the button bounds to indicate
    //  keyboard focus.  Uses alternating pixels.

    void DrawFocusIndicator(Surface* pSurface, int32 x, int32 y,
                            int32 w, int32 h, DWORD color) noexcept
    {
        if (!pSurface || w < 6 || h < 6) return;
        // Top edge (dotted).
        for (int32 i = x + 2; i < x + w - 2; i += 2)
        {
            Rectangle px(i, y + 2, 1, 1);
            pSurface->FillRectEx(nullptr, &px, color);
        }
        // Bottom edge (dotted).
        for (int32 i = x + 2; i < x + w - 2; i += 2)
        {
            Rectangle px(i, y + h - 3, 1, 1);
            pSurface->FillRectEx(nullptr, &px, color);
        }
        // Left edge (dotted).
        for (int32 i = y + 2; i < y + h - 2; i += 2)
        {
            Rectangle px(x + 2, i, 1, 1);
            pSurface->FillRectEx(nullptr, &px, color);
        }
        // Right edge (dotted).
        for (int32 i = y + 2; i < y + h - 2; i += 2)
        {
            Rectangle px(x + w - 3, i, 1, 1);
            pSurface->FillRectEx(nullptr, &px, color);
        }
    }

    // ── Text rendering via DSurface ────────────────────────────────────

    void RenderText(Surface* pSurface, const char* pText,
                    int32 x, int32 y, DWORD color,
                    TextButtonClass::TextAlign align) noexcept
    {
        if (!pSurface || !pText || !pText[0])
            return;

        // Convert char* to wchar_t* for DSurface::DrawText.
        wchar_t wbuf[128];
        int32 i = 0;
        for (; pText[i] && i < 127; ++i)
            wbuf[i] = static_cast<wchar_t>(pText[i]);
        wbuf[i] = L'\0';

        DSurface* pDSurface = static_cast<DSurface*>(pSurface);
        Point2D location(x, y);

        TextPrintType flag = TextPrintType::NoShadow;
        switch (align)
        {
            case TextButtonClass::TextAlign::Left:   flag = TextPrintType::Left;   break;
            case TextButtonClass::TextAlign::Center: flag = TextPrintType::Center; break;
            case TextButtonClass::TextAlign::Right:  flag = TextPrintType::Right;  break;
        }

        pDSurface->DrawText(wbuf, &location, color);
        (void)flag;
    }

} // anonymous namespace

// ============================================================================
// Construction
// ============================================================================

TextButtonClass::TextButtonClass() noexcept
    : GadgetClass()
    , FontSize(12)
    , Alignment(TextAlign::Center)
    , NormalColor(255, 255, 255)
    , HoverColor(255, 255, 0)
    , PressedColor(255, 200, 0)
    , DisabledColor(128, 128, 128)
    , NormalBg(40, 40, 60)
    , HoverBg(60, 60, 80)
    , PressedBg(80, 80, 100)
    , DisabledBg(30, 30, 30)
    , AutoRepeatEnabled(false)
    , AutoRepeatInitialDelay(30)
    , AutoRepeatRepeatDelay(8)
    , AutoRepeatCounter(0)
    , AutoRepeatArmed(false)
{
    Text[0] = '\0';
}

TextButtonClass::TextButtonClass(int32 x, int32 y, int32 w, int32 h,
                                 const char* pText, int32 id) noexcept
    : GadgetClass(x, y, w, h, id)
    , FontSize(12)
    , Alignment(TextAlign::Center)
    , NormalColor(255, 255, 255)
    , HoverColor(255, 255, 0)
    , PressedColor(255, 200, 0)
    , DisabledColor(128, 128, 128)
    , NormalBg(40, 40, 60)
    , HoverBg(60, 60, 80)
    , PressedBg(80, 80, 100)
    , DisabledBg(30, 30, 30)
    , AutoRepeatEnabled(false)
    , AutoRepeatInitialDelay(30)
    , AutoRepeatRepeatDelay(8)
    , AutoRepeatCounter(0)
    , AutoRepeatArmed(false)
{
    SetText(pText);
}

// ============================================================================
// Destruction
// ============================================================================

TextButtonClass::~TextButtonClass()
{
}

// ============================================================================
// Text management
// ============================================================================

void TextButtonClass::SetText(const char* pText) noexcept
{
    if (!pText)
    {
        Text[0] = '\0';
    }
    else
    {
        int32 i = 0;
        while (pText[i] && i < MaxTextLen - 1)
        {
            Text[i] = pText[i];
            ++i;
        }
        Text[i] = '\0';
    }
    SetNeedsRedraw(true);
}

// ============================================================================
// Visual state
// ============================================================================

ButtonVisualState TextButtonClass::GetVisualState() const noexcept
{
    if (!IsEnabled())
        return ButtonVisualState::Disabled;
    if (IsPressed())
        return ButtonVisualState::Pressed;
    if (IsHovered())
        return ButtonVisualState::Hovered;
    if (IsFocused())
        return ButtonVisualState::Focused;
    return ButtonVisualState::Normal;
}

// ============================================================================
// Auto-repeat
// ============================================================================

void TextButtonClass::SetAutoRepeat(bool enable, int32 initialDelay,
                                    int32 repeatDelay) noexcept
{
    AutoRepeatEnabled = enable;
    AutoRepeatInitialDelay = initialDelay;
    AutoRepeatRepeatDelay = repeatDelay;
    AutoRepeatCounter = 0;
    AutoRepeatArmed = false;
}

// ============================================================================
// Drawing
//
//  The Draw method selects colors based on the current visual state and
//  fills the button background, draws a border and renders the text label.
//  The actual rendering is done through the Surface interface; this
//  implementation selects the colors and computes the layout.
// ============================================================================

void TextButtonClass::Draw(Surface* pSurface)
{
    if (!pSurface)
        return;

    if (!IsVisible())
        return;

    // ── Auto-repeat processing ─────────────────────────────────────────
    //  Since we cannot override Update() (it is not declared in the
    //  header), we process auto-repeat here in Draw(), which is called
    //  every frame while the button is visible.

    if (AutoRepeatEnabled && IsPressed())
    {
        if (AutoRepeatCounter > 0)
        {
            --AutoRepeatCounter;
            if (AutoRepeatCounter == 0)
            {
                // Fire a repeat click.
                InvokeCallback(GetID());
                // Set up the next repeat interval.
                AutoRepeatCounter = AutoRepeatRepeatDelay;
                AutoRepeatArmed = true;
            }
        }
        else if (AutoRepeatArmed)
        {
            // Continuous repeat.
            --AutoRepeatCounter;
            if (AutoRepeatCounter <= 0)
            {
                InvokeCallback(GetID());
                AutoRepeatCounter = AutoRepeatRepeatDelay;
            }
        }
    }

    // ── Select colors based on visual state ────────────────────────────
    ButtonVisualState state = GetVisualState();
    const ColorStruct* pFg = &NormalColor;
    const ColorStruct* pBg = &NormalBg;

    switch (state)
    {
    case ButtonVisualState::Hovered:
        pFg = &HoverColor;    pBg = &HoverBg;    break;
    case ButtonVisualState::Pressed:
        pFg = &PressedColor;  pBg = &PressedBg;  break;
    case ButtonVisualState::Disabled:
        pFg = &DisabledColor; pBg = &DisabledBg; break;
    case ButtonVisualState::Focused:
        pFg = &HoverColor;    pBg = &NormalBg;   break;
    default:
        break;
    }

    // Store the selected colors for the base class.
    Color = *pFg;
    BackColor = *pBg;

    // ── Compute button geometry ────────────────────────────────────────
    int32 bx = GetX();
    int32 by = GetY();
    int32 bw = GetWidth();
    int32 bh = GetHeight();

    if (bw < 4 || bh < 4)
        return;

    // ── Fill the background ────────────────────────────────────────────
    DWORD bgNative = ColorToNative(pSurface, *pBg);
    FillRect(pSurface, bx, by, bw, bh, bgNative);

    // ── Draw the 3D bevel ──────────────────────────────────────────────
    ColorStruct bevelLight(255, 255, 255);
    ColorStruct bevelDark  (0,   0,   0);

    if (!IsEnabled())
    {
        bevelLight = ColorStruct(100, 100, 100);
        bevelDark  = ColorStruct( 60,  60,  60);
    }

    bool raised = !IsPressed();
    if (bw >= 8 && bh >= 8)
    {
        DrawDoubleBevel(pSurface, bx, by, bw, bh, raised,
                        bevelLight, bevelDark);
    }
    else
    {
        DrawBevel(pSurface, bx, by, bw, bh, raised,
                  bevelLight, bevelDark);
    }

    // ── Draw the text label ────────────────────────────────────────────
    if (Text[0] != '\0')
    {
        int32 textW = EstimateTextWidth(Text, FontSize);
        int32 textH = FontSize;
        Point2D textPos = ComputeTextPosition(bx, by, bw, bh,
                                              textW, textH, Alignment);

        DWORD fgNative = ColorToNative(pSurface, *pFg);
        RenderText(pSurface, Text, textPos.X, textPos.Y, fgNative, Alignment);
    }

    // ── Draw focus indicator ───────────────────────────────────────────
    if (IsFocused() && IsEnabled())
    {
        DWORD focusColor = ColorToNative(pSurface, ColorStruct(255, 255, 0));
        DrawFocusIndicator(pSurface, bx, by, bw, bh, focusColor);
    }

    SetNeedsRedraw(false);
}

// ============================================================================
// Event handling
//
//  The button responds to:
//    * MouseDown  -> set pressed state, arm auto-repeat
//    * MouseUp    -> if inside bounds, fire OnClick; release pressed state
//    * MouseMove  -> update hovered state
//    * KeyDown    -> Space/Enter triggers press; release fires click
// ============================================================================

bool TextButtonClass::HandleEvent(const GadgetEvent& event)
{
    if (!IsEnabled())
        return false;

    switch (event.Type)
    {
    case GadgetEventType::MouseDown:
        SetState(GadgetState::Pressed);
        OnPress();
        if (AutoRepeatEnabled)
        {
            AutoRepeatCounter = AutoRepeatInitialDelay;
            AutoRepeatArmed = false;
        }
        return true;

    case GadgetEventType::MouseUp:
        if (IsPressed())
        {
            ClearState(GadgetState::Pressed);
            OnRelease();
            // Fire click only if the mouse is still inside the button.
            if (Contains(event.MousePos.X + GetX(), event.MousePos.Y + GetY()))
            {
                OnClick();
                InvokeCallback(GetID());
            }
        }
        if (AutoRepeatEnabled)
        {
            AutoRepeatCounter = 0;
            AutoRepeatArmed = false;
        }
        return true;

    case GadgetEventType::MouseMove:
        {
            bool wasHovered = IsHovered();
            bool nowHovered = Contains(event.MousePos.X + GetX(),
                                       event.MousePos.Y + GetY());
            if (nowHovered != wasHovered)
            {
                if (nowHovered) SetState(GadgetState::Hovered);
                else ClearState(GadgetState::Hovered);
                SetNeedsRedraw(true);
            }
        }
        return true;

    case GadgetEventType::MouseEnter:
        SetState(GadgetState::Hovered);
        SetNeedsRedraw(true);
        return true;

    case GadgetEventType::MouseLeave:
        ClearState(GadgetState::Hovered);
        if (IsPressed())
        {
            ClearState(GadgetState::Pressed);
            if (AutoRepeatEnabled)
            {
                AutoRepeatCounter = 0;
                AutoRepeatArmed = false;
            }
        }
        SetNeedsRedraw(true);
        return true;

    case GadgetEventType::KeyDown:
        if (event.Key == 32 /* VK_SPACE */ || event.Key == 13 /* VK_RETURN */)
        {
            if (!IsPressed())
            {
                SetState(GadgetState::Pressed);
                OnPress();
            }
            return true;
        }
        break;

    case GadgetEventType::KeyUp:
        if (event.Key == 32 /* VK_SPACE */ || event.Key == 13 /* VK_RETURN */)
        {
            if (IsPressed())
            {
                ClearState(GadgetState::Pressed);
                OnRelease();
                OnClick();
                InvokeCallback(GetID());
            }
            return true;
        }
        break;

    default:
        break;
    }

    return false;
}

// ============================================================================
// State change callbacks
// ============================================================================

void TextButtonClass::OnPress()
{
    SetNeedsRedraw(true);
}

void TextButtonClass::OnRelease()
{
    SetNeedsRedraw(true);
}

void TextButtonClass::OnClick()
{
    SetNeedsRedraw(true);
}
