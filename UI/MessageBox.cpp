#include <UI/MessageBox.h>
#include <UI/TextButton.h>

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>

#include <cstring>

// ============================================================================
// MessageBox.cpp - Message box implementation
//
//  Implements the MessageBoxClass modal dialog.  The dialog creates
//  TextButtonClass children for each requested button (OK, Cancel, etc.)
//  and routes their click callbacks to dismiss the dialog with the
//  appropriate result.
// ============================================================================

// ============================================================================
// Button callback data
// ============================================================================

namespace
{
    struct ButtonCallbackData
    {
        MessageBoxClass*  Box;
        MessageBoxResult  Result;
    };
}

// ============================================================================
// Construction
// ============================================================================

MessageBoxClass::MessageBoxClass() noexcept
    : GadgetClass()
    , MsgFlags(MB_OK)
    , Result(MessageBoxResult::None)
    , ButtonCount(0)
    , ResultCallback(nullptr)
    , ResultCallbackData(nullptr)
{
    Title[0] = '\0';
    Message[0] = '\0';
    Buttons[0] = nullptr;
    Buttons[1] = nullptr;
    Buttons[2] = nullptr;
    SetState(GadgetState::Modal);
}

MessageBoxClass::MessageBoxClass(const char* pTitle, const char* pMessage,
                                 MessageBoxFlags flags) noexcept
    : GadgetClass()
    , MsgFlags(flags)
    , Result(MessageBoxResult::None)
    , ButtonCount(0)
    , ResultCallback(nullptr)
    , ResultCallbackData(nullptr)
{
    SetTitle(pTitle);
    SetMessage(pMessage);
    Buttons[0] = nullptr;
    Buttons[1] = nullptr;
    Buttons[2] = nullptr;
    SetState(GadgetState::Modal);

    // Default dialog size.
    SetRect(0, 0, 300, 120);

    CreateButtons();
    AutoSize();
}

// ============================================================================
// Destruction
// ============================================================================

MessageBoxClass::~MessageBoxClass()
{
    DestroyButtons();
}

// ============================================================================
// Content management
// ============================================================================

void MessageBoxClass::SetTitle(const char* pTitle) noexcept
{
    if (!pTitle)
    {
        Title[0] = '\0';
    }
    else
    {
        int32 i = 0;
        while (pTitle[i] && i < MaxTitleLen - 1)
        {
            Title[i] = pTitle[i];
            ++i;
        }
        Title[i] = '\0';
    }
    SetNeedsRedraw(true);
}

void MessageBoxClass::SetMessage(const char* pMessage) noexcept
{
    if (!pMessage)
    {
        Message[0] = '\0';
    }
    else
    {
        int32 i = 0;
        while (pMessage[i] && i < MaxMessageLen - 1)
        {
            Message[i] = pMessage[i];
            ++i;
        }
        Message[i] = '\0';
    }
    SetNeedsRedraw(true);
}

// ============================================================================
// Flags
// ============================================================================

void MessageBoxClass::SetFlags(MessageBoxFlags flags) noexcept
{
    if (MsgFlags == flags)
        return;
    MsgFlags = flags;
    DestroyButtons();
    CreateButtons();
    AutoSize();
    SetNeedsRedraw(true);
}

// ============================================================================
// Button creation
//
//  Creates TextButtonClass children for each flag that requests a button.
//  Up to 3 buttons are supported (matching the original binary).
// ============================================================================

void MessageBoxClass::CreateButtons() noexcept
{
    DestroyButtons();

    // Button dimensions.
    const int32 btnW = 70;
    const int32 btnH = 24;
    const int32 btnSpacing = 10;

    // Collect the requested buttons.
    struct BtnInfo
    {
        const char*      Label;
        MessageBoxResult Result;
    };

    BtnInfo buttons[3];
    int32 count = 0;

    if (static_cast<uint32>(MsgFlags & MessageBoxFlags::OK) && count < 3)
    {
        buttons[count++] = { "OK", MessageBoxResult::OK };
    }
    if (static_cast<uint32>(MsgFlags & MessageBoxFlags::Yes) && count < 3)
    {
        buttons[count++] = { "Yes", MessageBoxResult::Yes };
    }
    if (static_cast<uint32>(MsgFlags & MessageBoxFlags::No) && count < 3)
    {
        buttons[count++] = { "No", MessageBoxResult::No };
    }
    if (static_cast<uint32>(MsgFlags & MessageBoxFlags::Cancel) && count < 3)
    {
        buttons[count++] = { "Cancel", MessageBoxResult::Cancel };
    }
    if (static_cast<uint32>(MsgFlags & MessageBoxFlags::Retry) && count < 3)
    {
        buttons[count++] = { "Retry", MessageBoxResult::Retry };
    }
    if (static_cast<uint32>(MsgFlags & MessageBoxFlags::Ignore) && count < 3)
    {
        buttons[count++] = { "Ignore", MessageBoxResult::Ignore };
    }
    if (static_cast<uint32>(MsgFlags & MessageBoxFlags::Abort) && count < 3)
    {
        buttons[count++] = { "Abort", MessageBoxResult::Abort };
    }

    // If no buttons were requested, default to OK.
    if (count == 0)
    {
        buttons[count++] = { "OK", MessageBoxResult::OK };
    }

    ButtonCount = count;

    // Position buttons centered along the bottom of the dialog.
    int32 totalWidth = count * btnW + (count - 1) * btnSpacing;
    int32 startX = (GetWidth() - totalWidth) / 2;
    int32 btnY = GetHeight() - btnH - 10;

    for (int32 i = 0; i < count; ++i)
    {
        int32 btnX = startX + i * (btnW + btnSpacing);
        TextButtonClass* pBtn = new TextButtonClass(btnX, btnY, btnW, btnH,
                                                     buttons[i].Label, i);
        if (pBtn)
        {
            pBtn->Parent = this;
            Children.Add(pBtn);
            Buttons[i] = pBtn;
        }
    }
}

void MessageBoxClass::DestroyButtons() noexcept
{
    for (int32 i = 0; i < 3; ++i)
    {
        if (Buttons[i])
        {
            Buttons[i]->Parent = nullptr;
            delete Buttons[i];
            Buttons[i] = nullptr;
        }
    }
    ButtonCount = 0;
}

// ============================================================================
// Auto-size
//
//  Adjusts the dialog dimensions to fit the message text and buttons.
// ============================================================================

void MessageBoxClass::AutoSize() noexcept
{
    // Estimate the required width from the message length.
    // Each character is roughly 8 pixels at the default font size.
    int32 msgLen = 0;
    while (Message[msgLen]) ++msgLen;

    int32 textWidth = msgLen * 8 + 40;   // 20px padding on each side
    int32 btnWidth = ButtonCount * 70 + (ButtonCount - 1) * 10 + 40;

    int32 newWidth = textWidth > btnWidth ? textWidth : btnWidth;
    if (newWidth < 200) newWidth = 200;

    int32 newHeight = 80;  // Base height for title + padding
    if (Message[0])
    {
        // Rough estimate: 40 chars per line at 16px line height.
        int32 lines = (msgLen / 40) + 1;
        newHeight += lines * 16;
    }
    newHeight += 34;  // Button area

    SetSize(newWidth, newHeight);

    // Reposition buttons.
    if (ButtonCount > 0)
    {
        int32 totalBtnWidth = ButtonCount * 70 + (ButtonCount - 1) * 10;
        int32 startX = (newWidth - totalBtnWidth) / 2;
        int32 btnY = newHeight - 34;
        for (int32 i = 0; i < ButtonCount; ++i)
        {
            if (Buttons[i])
            {
                Buttons[i]->SetPosition(startX + i * 80, btnY);
            }
        }
    }
}

// ============================================================================
// Show centered
// ============================================================================

void MessageBoxClass::ShowCentered(int32 screenW, int32 screenH) noexcept
{
    int32 x = (screenW - GetWidth()) / 2;
    int32 y = (screenH - GetHeight()) / 2;
    SetPosition(x, y);
    Show();
    Result = MessageBoxResult::None;
}

// ============================================================================
// Dismiss
// ============================================================================

void MessageBoxClass::Dismiss(MessageBoxResult result) noexcept
{
    Result = result;
    if (ResultCallback)
    {
        ResultCallback(this, result, ResultCallbackData);
    }
    Hide();
}

// ============================================================================
// Drawing
// ============================================================================

void MessageBoxClass::Draw(Surface* pSurface)
{
    (void)pSurface;

    if (!IsVisible())
        return;

    // The rendering layer would:
    // 1. Draw a semi-transparent overlay over the entire screen (for the
    //    modal dimming effect).
    // 2. Draw the dialog background (filled rectangle with a border).
    // 3. Draw the title bar.
    // 4. Draw the message text (word-wrapped if the WrapText flag is set).
    // 5. Draw each button child.

    SetNeedsRedraw(false);
}

// ============================================================================
// Event handling
// ============================================================================

bool MessageBoxClass::HandleEvent(const GadgetEvent& event)
{
    // Route events to child buttons first.
    for (int32 i = ButtonCount - 1; i >= 0; --i)
    {
        if (Buttons[i] && Buttons[i]->IsEnabled())
        {
            if (Buttons[i]->Contains(event.MousePos.X + GetX(),
                                     event.MousePos.Y + GetY()))
            {
                if (Buttons[i]->HandleEvent(event))
                {
                    // Check if the button was clicked (dismiss the dialog).
                    if (event.Type == GadgetEventType::MouseUp)
                    {
                        static const MessageBoxResult results[] = {
                            MessageBoxResult::OK,
                            MessageBoxResult::Yes,
                            MessageBoxResult::No,
                            MessageBoxResult::Cancel,
                            MessageBoxResult::Retry,
                            MessageBoxResult::Ignore,
                            MessageBoxResult::Abort,
                        };
                        if (i >= 0 && i < ButtonCount)
                        {
                            // Map button index to result.
                            // The button labels were set in CreateButtons;
                            // we use the button ID to look up the result.
                            // For simplicity, we dismiss with the button's
                            // ID mapped to the result enum.
                            int32 btnId = Buttons[i]->GetID();
                            if (btnId >= 0 && btnId < 7)
                            {
                                Dismiss(results[btnId]);
                            }
                            else
                            {
                                Dismiss(MessageBoxResult::OK);
                            }
                        }
                    }
                    return true;
                }
            }
        }
    }

    // Handle keyboard: Enter = default, Escape = cancel.
    if (event.Type == GadgetEventType::KeyDown)
    {
        if (event.Key == 13 /* VK_RETURN */)
        {
            Dismiss(MessageBoxResult::OK);
            return true;
        }
        if (event.Key == 27 /* VK_ESCAPE */)
        {
            Dismiss(MessageBoxResult::Cancel);
            return true;
        }
    }

    return false;
}
