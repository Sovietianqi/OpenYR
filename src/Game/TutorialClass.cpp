// ============================================================================
// TutorialClass.cpp - In-game tutorial / help system implementation
// ============================================================================
// Implements tutorial message loading from INI, event-based triggering,
// on-screen banner rendering, voice playback, and progression tracking.
// ============================================================================

#include "TutorialClass.h"
#include "../INI/INIClass.h"
#include "../Audio/VocClass.h"
#include "../Game/Game.h"

#include <cstring>
#include <cstdio>

// ============================================================================
// Construction / destruction
// ============================================================================

TutorialClass::TutorialClass() noexcept
    : ActiveEntry(nullptr)
    , DisplayTimer(0)
    , ActiveVoice(nullptr)
    , VoiceEnabled(true)
    , BannerY(0)
{
}

TutorialClass::~TutorialClass()
{
    Clear();
}

// ============================================================================
// Initialization
// ============================================================================

bool TutorialClass::Load_From_INI(CCINIClass* pINI)
{
    if (!pINI) {
        return false;
    }

    // Clear any previously loaded entries.
    Clear();

    // The [Tutorial] section contains the text messages.
    // Keys are numeric IDs (1, 2, 3, ...), values are the display text.
    int32 textCount = pINI->GetKeyCount("Tutorial");
    if (textCount <= 0) {
        return false;
    }

    for (int32 i = 0; i < textCount; ++i) {
        const char* keyName = pINI->GetKeyName("Tutorial", i);
        if (!keyName) continue;

        int32 id = std::atoi(keyName);
        if (id <= 0) continue;

        // Allocate a new tutorial entry.
        TutorialEntry* pEntry = GameCreate<TutorialEntry>();
        if (!pEntry) continue;
        pEntry->ID = id;

        // Read the tutorial text.
        char textBuf[512];
        textBuf[0] = '\0';
        pINI->ReadString("Tutorial", keyName, "", textBuf, sizeof(textBuf));
        std::strncpy(pEntry->Text, textBuf, sizeof(pEntry->Text) - 1);
        pEntry->Text[sizeof(pEntry->Text) - 1] = '\0';

        // Read the event type from [TutorialEvents].
        int32 eventType = pINI->ReadInteger("TutorialEvents", keyName,
            static_cast<int32>(TutorialEventType::None));
        pEntry->EventType = static_cast<TutorialEventType>(eventType);

        // Read the voice file from [TutorialVoice].
        char voiceBuf[64];
        voiceBuf[0] = '\0';
        pINI->ReadString("TutorialVoice", keyName, "", voiceBuf, sizeof(voiceBuf));
        if (voiceBuf[0] != '\0') {
            std::strncpy(pEntry->VoiceFile, voiceBuf, sizeof(pEntry->VoiceFile) - 1);
            pEntry->VoiceFile[sizeof(pEntry->VoiceFile) - 1] = '\0';
            pEntry->PlayVoice = true;
        }

        // Read the display duration from [TutorialDuration].
        pEntry->DisplayFrames = pINI->ReadInteger("TutorialDuration", keyName, 180);

        // Read the "show once" flag from [TutorialShowOnce].
        pEntry->ShowOnce = pINI->ReadBool("TutorialShowOnce", keyName, true);

        Entries.Add(pEntry);
    }

    return Entries.Count > 0;
}

void TutorialClass::Clear()
{
    // Stop any active voice playback.
    Stop_Voice();

    // Delete all owned entries.
    for (int32 i = 0; i < Entries.Count; ++i) {
        TutorialEntry* pEntry = Entries.GetItem(i);
        GameDelete(pEntry);
    }
    Entries.Clear();

    ActiveEntry  = nullptr;
    DisplayTimer = 0;
}

void TutorialClass::Reset_Played()
{
    for (int32 i = 0; i < Entries.Count; ++i) {
        TutorialEntry* pEntry = Entries.GetItem(i);
        if (pEntry) {
            pEntry->HasPlayed = false;
        }
    }
}

// ============================================================================
// Internal helpers
// ============================================================================

TutorialEntry* TutorialClass::Find_Entry_By_Event(TutorialEventType eventType)
{
    for (int32 i = 0; i < Entries.Count; ++i) {
        TutorialEntry* pEntry = Entries.GetItem(i);
        if (pEntry && pEntry->EventType == eventType) {
            return pEntry;
        }
    }
    return nullptr;
}

void TutorialClass::Activate_Entry(TutorialEntry* pEntry)
{
    if (!pEntry) return;

    // If a tutorial is already active and the new one is the same, don't
    // re-activate it.
    if (ActiveEntry == pEntry) {
        return;
    }

    // Stop any currently playing voice.
    Stop_Voice();

    ActiveEntry  = pEntry;
    DisplayTimer = pEntry->DisplayFrames;

    // Mark as played.
    pEntry->HasPlayed = true;

    // Play the associated voice clip.
    if (pEntry->PlayVoice && VoiceEnabled) {
        Play_Voice(pEntry);
    }
}

// ============================================================================
// Tutorial triggering
// ============================================================================

bool TutorialClass::Trigger(TutorialEventType eventType)
{
    if (eventType == TutorialEventType::None) {
        return false;
    }

    TutorialEntry* pEntry = Find_Entry_By_Event(eventType);
    if (!pEntry) {
        return false;
    }

    // If this tutorial has already been shown and should only show once,
    // do not re-trigger it.
    if (pEntry->HasPlayed && pEntry->ShowOnce) {
        return false;
    }

    Activate_Entry(pEntry);
    return true;
}

bool TutorialClass::Trigger_By_ID(int32 id)
{
    for (int32 i = 0; i < Entries.Count; ++i) {
        TutorialEntry* pEntry = Entries.GetItem(i);
        if (pEntry && pEntry->ID == id) {
            if (pEntry->HasPlayed && pEntry->ShowOnce) {
                return false;
            }
            Activate_Entry(pEntry);
            return true;
        }
    }
    return false;
}

// ============================================================================
// Update
// ============================================================================

void TutorialClass::Update()
{
    if (!ActiveEntry) {
        return;
    }

    // Decrement the display timer.
    if (DisplayTimer > 0) {
        --DisplayTimer;
    }

    // When the timer reaches zero, dismiss the tutorial.
    if (DisplayTimer <= 0) {
        Dismiss();
    }
}

// ============================================================================
// Display
// ============================================================================

void TutorialClass::Draw(Surface* pSurface) const
{
    if (!pSurface || !pSurface->Buffer || !ActiveEntry) {
        return;
    }

    // The tutorial banner is drawn as a semi-transparent rectangle at the
    // top-centre of the screen.
    int32 surfW = pSurface->Width;
    int32 surfH = pSurface->Height;

    // Compute the banner dimensions.
    // The banner is 80% of the screen width and 3 lines of text tall.
    int32 bannerW = (surfW * 80) / 100;
    int32 bannerH = 60;
    int32 bannerX = (surfW - bannerW) / 2;
    int32 bannerY = BannerY + 4;

    // Clamp to surface bounds.
    if (bannerX < 0) bannerX = 0;
    if (bannerX + bannerW > surfW) bannerW = surfW - bannerX;
    if (bannerY + bannerH > surfH) bannerH = surfH - bannerY;

    if (bannerW <= 0 || bannerH <= 0) return;

    // Draw the banner background (dark blue, index 0 in typical palettes is
    // black; we use a mid-range index for visibility).
    uint8 bgColor = 0;   // black
    uint8 borderColor = 12; // light red / orange in the default palette
    uint8 textColor = 14;   // yellow in the default palette

    // Fill the background.
    Rectangle bgRect(bannerX, bannerY, bannerW, bannerH);
    pSurface->FillRect(bgRect, bgColor);

    // Draw a border.
    Point2D tl(bannerX, bannerY);
    Point2D tr(bannerX + bannerW - 1, bannerY);
    Point2D bl(bannerX, bannerY + bannerH - 1);
    Point2D br(bannerX + bannerW - 1, bannerY + bannerH - 1);
    pSurface->DrawLine(&tl, &tr, borderColor);
    pSurface->DrawLine(&tr, &br, borderColor);
    pSurface->DrawLine(&br, &bl, borderColor);
    pSurface->DrawLine(&bl, &tl, borderColor);

    // Draw the tutorial text centred in the banner.
    // We draw the text character by character since we do not have a FontClass
    // instance here; in the real game, the global font manager is used.
    // For this implementation we write the text as raw pixels using a simple
    // 5x7 bitmap fallback if the text fits.
    const char* pText = ActiveEntry->Text;
    if (!pText || pText[0] == '\0') {
        return;
    }

    // Draw a simple text indicator: fill a small area with the text color
    // to indicate where text would be rendered.  In the full engine, the
    // FontClass::Draw_Text method is called here.
    // For now, we draw a thin coloured line under the text area to indicate
    // the tutorial is active.
    int32 textY = bannerY + bannerH / 2;
    Point2D lineStart(bannerX + 8, textY);
    Point2D lineEnd(bannerX + bannerW - 8, textY);
    pSurface->DrawLine(&lineStart, &lineEnd, textColor);

    // In the production engine, the actual text rendering would be:
    //   FontClass* pFont = FontControl->Get_Current_Font();
    //   if (pFont) {
    //       Rectangle textRect(bannerX + 8, bannerY + 4, bannerW - 16, bannerH - 8);
    //       pFont->Draw_Text_Multi(pText, pSurface, textRect, textColor,
    //                            TextPrintType::Center | TextPrintType::FullShadow);
    //   }
    // The character-by-character pixel writing below is a minimal fallback
    // that ensures the banner is visually non-empty even without a font.
    int32 textLen = static_cast<int32>(std::strlen(pText));
    int32 maxChars = (bannerW - 16) / 6;  // approx 6px per char
    if (textLen > maxChars) textLen = maxChars;

    int32 startX = bannerX + 8;
    for (int32 i = 0; i < textLen; ++i) {
        // Draw a small dot for each character position.
        if (pText[i] != ' ') {
            Point2D dot(startX + i * 6, textY - 2);
            pSurface->SetPixel(&dot, textColor);
        }
    }
}

void TutorialClass::Dismiss()
{
    Stop_Voice();
    ActiveEntry  = nullptr;
    DisplayTimer = 0;
}

const char* TutorialClass::Get_Active_Text() const
{
    if (!ActiveEntry) {
        return nullptr;
    }
    return ActiveEntry->Text;
}

// ============================================================================
// Tutorial progression tracking
// ============================================================================

bool TutorialClass::Has_Played(TutorialEventType eventType) const
{
    for (int32 i = 0; i < Entries.Count; ++i) {
        const TutorialEntry* pEntry = Entries.GetItem(i);
        if (pEntry && pEntry->EventType == eventType) {
            return pEntry->HasPlayed;
        }
    }
    return false;
}

void TutorialClass::Mark_Played(TutorialEventType eventType)
{
    TutorialEntry* pEntry = Find_Entry_By_Event(eventType);
    if (pEntry) {
        pEntry->HasPlayed = true;
    }
}

const TutorialEntry* TutorialClass::Get_Entry(int32 index) const
{
    if (index < 0 || index >= Entries.Count) {
        return nullptr;
    }
    return Entries.GetItem(index);
}

const TutorialEntry* TutorialClass::Find_By_Event(TutorialEventType eventType) const
{
    for (int32 i = 0; i < Entries.Count; ++i) {
        const TutorialEntry* pEntry = Entries.GetItem(i);
        if (pEntry && pEntry->EventType == eventType) {
            return pEntry;
        }
    }
    return nullptr;
}

// ============================================================================
// Voice playback
// ============================================================================

void TutorialClass::Play_Voice(const TutorialEntry* pEntry)
{
    if (!pEntry || !pEntry->PlayVoice || pEntry->VoiceFile[0] == '\0') {
        return;
    }
    if (!VoiceEnabled) {
        return;
    }

    // Stop any currently playing voice.
    Stop_Voice();

    // Create a new VocClass and load the voice file.
    ActiveVoice = GameCreate<VocClass>();
    if (!ActiveVoice) {
        return;
    }

    if (!ActiveVoice->Load(pEntry->VoiceFile)) {
        // Failed to load; clean up.
        GameDelete(ActiveVoice);
        ActiveVoice = nullptr;
        return;
    }

    // Set speech volume and play.
    ActiveVoice->SetVolume(255);
    ActiveVoice->SetCategory(2); // speech category
    ActiveVoice->Play();
}

void TutorialClass::Stop_Voice()
{
    if (ActiveVoice) {
        ActiveVoice->Stop();
        GameDelete(ActiveVoice);
        ActiveVoice = nullptr;
    }
}

// ============================================================================
// Save / Load state
// ============================================================================

bool TutorialClass::Save_State(CCINIClass* pINI) const
{
    if (!pINI) return false;

    // Write the "played" state of each tutorial entry to the [TutorialState]
    // section.  The key is the entry ID, the value is 1 (played) or 0 (not).
    for (int32 i = 0; i < Entries.Count; ++i) {
        const TutorialEntry* pEntry = Entries.GetItem(i);
        if (!pEntry) continue;

        char keyBuf[32];
        std::snprintf(keyBuf, sizeof(keyBuf), "%d", pEntry->ID);
        pINI->WriteInteger("TutorialState", keyBuf,
                           pEntry->HasPlayed ? 1 : 0);
    }

    return true;
}

bool TutorialClass::Load_State(CCINIClass* pINI)
{
    if (!pINI) return false;

    for (int32 i = 0; i < Entries.Count; ++i) {
        TutorialEntry* pEntry = Entries.GetItem(i);
        if (!pEntry) continue;

        char keyBuf[32];
        std::snprintf(keyBuf, sizeof(keyBuf), "%d", pEntry->ID);
        int32 played = pINI->ReadInteger("TutorialState", keyBuf, 0);
        pEntry->HasPlayed = (played != 0);
    }

    return true;
}
