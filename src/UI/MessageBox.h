#pragma once

// ============================================================================
// MessageBox.h - Message box dialog
//
//  MessageBoxClass is a modal dialog that displays a message with one or
//  more buttons (OK, Cancel, Yes, No, etc.).  It blocks input to the
//  game until the user dismisses it by clicking a button or pressing
//  the corresponding key.
//
//  The original binary uses a fixed-size dialog with a text area and up
//  to 3 buttons.  This reconstruction follows the same layout.
// ============================================================================

#include <UI/Gadget.h>
#include <Core/Definitions.h>
#include <Core/Macros.h>

#include <cstdint>

// ============================================================================
// Forward declarations
// ============================================================================

class TextButtonClass;
class Surface;

// ============================================================================
// MessageBox flags
// ============================================================================

enum class MessageBoxFlags : uint32
{
    None        = 0x00000000,
    OK          = 0x00000001,   // Show an OK button
    Cancel      = 0x00000002,   // Show a Cancel button
    Yes         = 0x00000004,   // Show a Yes button
    No          = 0x00000008,   // Show a No button
    Retry       = 0x00000010,   // Show a Retry button
    Ignore      = 0x00000020,   // Show an Ignore button
    Abort       = 0x00000040,   // Show an Abort button
    CenterText  = 0x00000100,   // Center the message text
    WrapText    = 0x00000200,   // Word-wrap the message text
    NoFrame     = 0x00000400,   // Don't draw a border frame
    Modal       = 0x00000800,   // Block input to other gadgets
};

inline constexpr MessageBoxFlags operator|(MessageBoxFlags a, MessageBoxFlags b) noexcept
{
    return static_cast<MessageBoxFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
}
inline constexpr MessageBoxFlags operator&(MessageBoxFlags a, MessageBoxFlags b) noexcept
{
    return static_cast<MessageBoxFlags>(static_cast<uint32>(a) & static_cast<uint32>(b));
}

// ============================================================================
// MessageBoxResult - which button was clicked
// ============================================================================

enum class MessageBoxResult : int32
{
    None    = 0,
    OK      = 1,
    Cancel  = 2,
    Yes     = 3,
    No      = 4,
    Retry   = 5,
    Ignore  = 6,
    Abort   = 7,
    Timeout = 8,
};

// ============================================================================
// Convenience flag combinations
// ============================================================================

constexpr MessageBoxFlags MB_OK     = MessageBoxFlags::OK | MessageBoxFlags::Modal;
constexpr MessageBoxFlags MB_OKCANCEL = MessageBoxFlags::OK | MessageBoxFlags::Cancel | MessageBoxFlags::Modal;
constexpr MessageBoxFlags MB_YESNO  = MessageBoxFlags::Yes | MessageBoxFlags::No | MessageBoxFlags::Modal;
constexpr MessageBoxFlags MB_YESNOCANCEL = MessageBoxFlags::Yes | MessageBoxFlags::No | MessageBoxFlags::Cancel | MessageBoxFlags::Modal;
constexpr MessageBoxFlags MB_RETRYCANCEL = MessageBoxFlags::Retry | MessageBoxFlags::Cancel | MessageBoxFlags::Modal;

// ============================================================================
// MessageBoxClass - modal message dialog
// ============================================================================

class MessageBoxClass : public GadgetClass
{
public:
    // ── Construction / Destruction ──────────────────────────────────────

    MessageBoxClass() noexcept;
    MessageBoxClass(const char* pTitle, const char* pMessage,
                    MessageBoxFlags flags = MB_OK) noexcept;
    virtual ~MessageBoxClass();

    // ── GadgetClass overrides ───────────────────────────────────────────

    virtual void Draw(Surface* pSurface) override;
    virtual bool HandleEvent(const GadgetEvent& event) override;

    // ── Content ─────────────────────────────────────────────────────────

    void SetTitle(const char* pTitle) noexcept;
    void SetMessage(const char* pMessage) noexcept;

    const char* GetTitle() const noexcept { return Title; }
    const char* GetMessage() const noexcept { return Message; }

    // ── Flags ───────────────────────────────────────────────────────────

    MessageBoxFlags GetFlags() const noexcept { return MsgFlags; }
    void SetFlags(MessageBoxFlags flags) noexcept;

    // ── Result ──────────────────────────────────────────────────────────

    MessageBoxResult GetResult() const noexcept { return Result; }
    bool IsDismissed() const noexcept { return Result != MessageBoxResult::None; }

    // ── Callback ────────────────────────────────────────────────────────

    // Called when a button is clicked.  The result is available via
    // GetResult().
    using MessageBoxCallback = void (*)(MessageBoxClass* pBox, MessageBoxResult result, void* pUserData);

    void SetResultCallback(MessageBoxCallback callback, void* pUserData = nullptr) noexcept
    {
        ResultCallback = callback;
        ResultCallbackData = pUserData;
    }

    // ── Layout ──────────────────────────────────────────────────────────

    // Auto-size the dialog to fit the message and buttons.  Called
    // automatically when the title or message changes.
    void AutoSize() noexcept;

    // Show the dialog centered on the given screen dimensions.
    void ShowCentered(int32 screenW, int32 screenH) noexcept;

private:
    // ── Internal helpers ────────────────────────────────────────────────

    void CreateButtons() noexcept;
    void DestroyButtons() noexcept;
    void Dismiss(MessageBoxResult result) noexcept;

    // ── Data ────────────────────────────────────────────────────────────

    static constexpr int32 MaxTitleLen   = 64;
    static constexpr int32 MaxMessageLen = 512;

    char              Title[MaxTitleLen];
    char              Message[MaxMessageLen];
    MessageBoxFlags   MsgFlags;
    MessageBoxResult  Result;

    // Up to 3 buttons.
    TextButtonClass*  Buttons[3];
    int32             ButtonCount;

    MessageBoxCallback ResultCallback;
    void*              ResultCallbackData;
};
