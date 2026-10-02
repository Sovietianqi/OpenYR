#include "ThemeClass.h"
#include "../INI/INIClass.h"
#include "../Houses/HouseClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Houses/SideClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Game/Externs.h"
#include "../Game/GameInit.h"
#include "../IO/CCFileClass.h"
#include "../IO/FileSystem.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <cmath>

// ============================================================
// ThemeClass
//
//   Faithful reimplementation of the original audio playlist manager.
//   Field offsets, the "No theme" / ".WAV" / "Theme::QueueSong(%d)\n"
//   strings and the "-1"/sentinel handling all mirror the assembly.
// ============================================================

// The global instance the game hands out through ThemeClass::GetInstance.
static ThemeClass* g_ThemeInstance = nullptr;

// The 0x290/"No theme" constants the original embeds.
static constexpr const char* THEME_NO_THEME   = "No theme";
static constexpr const char* THEME_WAV_SUFFIX = ".WAV";
static constexpr const char* THEME_QUEUE_FMT  = "Theme::QueueSong(%d)\n";

// The engine's debug channel; the original routes "Theme::QueueSong(%d)\n"
// through WWDebugString (asm 0x7C5B10).  Matches the local helper used by the
// other subsystems in this tree.
static void WWDebugString(const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    std::vfprintf(stderr, pFormat, args);
    va_end(args);
}

// ============================================================
// Construction / destruction
//
//   Mirrors ThemeClass::ThemeClass (asm 0x7208F8):
//     [+00] = -1, [+04] = -1, [+08] = -1, [+0C] = 0xFF,
//     [+10] = [+11] = [+12] = 0,
//     [+14] = DynamicVectorClass vftable, [+18] = 0, [+1C] = 0,
//     [+20] = 1, [+21] = 0, [+24] = 0, [+28] = 0x0A, [+2C] = 0.
// ============================================================
ThemeClass::ThemeClass()
    : Track(-1), Side(-1), Scenario(-1), Repeat(0xFF)
    , Unk10(false), Unk11(false), Unk12(false)
    , Items(nullptr), Count(0), Capacity(0x0A), SomeStream(nullptr)
{
}

ThemeClass::~ThemeClass()
{
    Clear();
}

ThemeClass* ThemeClass::GetInstance()
{
    if (g_ThemeInstance == nullptr) {
        g_ThemeInstance = new ThemeClass();
    }
    return g_ThemeInstance;
}

// ============================================================
// Playlist storage
// ============================================================

bool ThemeClass::Add(ThemeControl* pTheme)
{
    if (pTheme == nullptr) {
        return false;
    }

    // Grow the backing array in the original's 0x0A step when the vector is
    // full; the vector starts with zero storage.
    if (Count >= Capacity) {
        const int32 newCapacity = Capacity + 0x0A;
        ThemeControl** pNew = static_cast<ThemeControl**>(
            std::realloc(Items, sizeof(ThemeControl*) * static_cast<size_t>(newCapacity)));
        if (pNew == nullptr) {
            return false;
        }
        Items = pNew;
        Capacity = newCapacity;
    }

    Items[Count] = pTheme;
    ++Count;
    return true;
}

void ThemeClass::Clear()
{
    for (int32 i = 0; i < Count; ++i) {
        delete Items[i];
    }
    std::free(Items);
    Items = nullptr;
    Count = 0;
    Capacity = 0x0A;
}

// ============================================================
// ThemeClass::Base_Name - asm 0x72093C
//
//   Returns the bare "Sound" name, or "No theme" when the ordinal is out of
//   range.  The original compares against [+24] (the count) with jnb.
// ============================================================
const char* ThemeClass::Base_Name(int32 index) const
{
    if (static_cast<uint32>(index) >= static_cast<uint32>(Count)) {
        return THEME_NO_THEME;
    }
    return Items[index]->Sound;
}

// ============================================================
// ThemeClass::Full_Name - asm 0x7209AC
//
//   Returns the string-table resolved display name (+0x200), or null.
// ============================================================
const wchar_t* ThemeClass::Full_Name(int32 index) const
{
    if (static_cast<uint32>(index) >= static_cast<uint32>(Count)) {
        return nullptr;
    }
    return Items[index]->FullName;
}

// ============================================================
// ThemeClass::Theme_File_Name - asm 0x720E2C
//
//   Builds "<Sound>.WAV" through _makepath into a static buffer and returns
//   it; on an out-of-range ordinal it returns the empty string.
// ============================================================
const char* ThemeClass::Theme_File_Name(int32 index)
{
    static char fileBuffer[0x100];

    if (static_cast<uint32>(index) >= static_cast<uint32>(Count)) {
        fileBuffer[0] = '\0';
        return fileBuffer;
    }

    // The original calls _makepath(buffer, null, null, Sound, ".WAV"), which
    // concatenates the file name and the extension.
    int32 n = 0;
    const char* pSound = Items[index]->Sound;
    while (pSound[n] != '\0' && n < static_cast<int32>(sizeof(fileBuffer)) - 5) {
        fileBuffer[n] = pSound[n];
        ++n;
    }
    for (int32 e = 0; THEME_WAV_SUFFIX[e] != '\0'; ++e) {
        fileBuffer[n++] = THEME_WAV_SUFFIX[e];
    }
    fileBuffer[n] = '\0';

    return fileBuffer;
}

// ============================================================
// ThemeClass::Track_Length - asm 0x720E9D
//
//   The floored length in seconds (+0x284), or 0 out of range.
// ============================================================
int32 ThemeClass::Track_Length(int32 index) const
{
    if (static_cast<uint32>(index) >= static_cast<uint32>(Count)) {
        return 0;
    }
    return static_cast<int32>(std::floor(Items[index]->Length));
}

// ============================================================
// ThemeClass::Is_Allowed - asm 0x7210B4
//
//   A theme is allowed when:
//     * the ordinal is one of the pending sentinels (-3 / -2), or
//     * the theme is marked available (+0x28A) and normal (+0x288), and
//     * the side restriction matches the local player's side (or is -1), and
//     * (in a campaign) the scenario ordinal is not past the theme's minimum.
// ============================================================
bool ThemeClass::Is_Allowed(int32 index) const
{
    // 0xFFFFFFFD and 0xFFFFFFFE are the "any" sentinels.
    if (index == -3 || index == -2) {
        return true;
    }

    if (static_cast<uint32>(index) >= static_cast<uint32>(Count)) {
        return false;
    }

    const ThemeControl* pTheme = Items[index];

    if (!pTheme->IsAvailable) {
        return false;
    }
    if (!pTheme->Normal) {
        return false;
    }

    // The side check consults the local player's side ordinal; when no player
    // exists or the theme is unrestricted the check passes.
    if (HouseClass::Player != nullptr && pTheme->Side != -1) {
        HouseTypeClass* pType = HouseClass::Player->Type;
        if (pType != nullptr && pType->SideIndex != pTheme->Side) {
            return false;
        }
    }

    // Outside a campaign scenario the theme's minimum scenario ordinal acts as
    // a lower bound.  The original compares Scenario->[+0x1254] against the
    // record's +0x280; the in-tree equivalent is ScenarioClass::CampaignIndex.
    if (ScenarioInit == 0 && TheScenario != nullptr &&
        TheScenario->CampaignIndex < pTheme->Scenario) {
        return false;
    }

    return true;
}

// ============================================================
// ThemeClass::From_Name - asm 0x7212B6
//
//   Case insensitive lookup of a theme by its bare name; -1 on miss.
// ============================================================
int32 ThemeClass::From_Name(const char* pName) const
{
    if (pName == nullptr || pName[0] == '\0') {
        return -1;
    }

    for (int32 i = 0; i < Count; ++i) {
        if (_strcmpi(Items[i]->Sound, pName) == 0) {
            return i;
        }
    }

    return -1;
}

// ============================================================
// ThemeClass::FindIndex - the ordinal of a named theme, or -1.
// ============================================================
int32 ThemeClass::FindIndex(const char* pName)
{
    ThemeClass* pInstance = GetInstance();
    if (pInstance == nullptr) {
        return -1;
    }
    return pInstance->From_Name(pName);
}

// ============================================================
// ThemeClass::Read_INI - asm 0x7204A2
//
//   Resolves the section, copies "Sound" into the record (stripping a leading
//   run of '$' / '#' marker bytes), reads "Scenario" / "Normal" / "Repeat" /
//   "Side" with the record's prior values as fallbacks, and only when Normal
//   is set resolves the string-table "Name".  Returns false when the section
//   is absent.
// ============================================================
bool ThemeClass::Read_INI(CCINIClass* pINI, const char* pSection, ThemeControl* pTheme)
{
    if (pINI == nullptr || pSection == nullptr || pTheme == nullptr) {
        return false;
    }

    if (pINI->GetSection(pSection) == nullptr) {
        return false;
    }

    // "Sound": read into a scratch buffer then copy after skipping the '$'/'#'
    // markers.  The original strips them in place with an inc/compare loop.
    char sound[0x100];
    sound[0] = '\0';
    pINI->ReadString(pSection, "Sound", "", sound, sizeof(sound));

    const char* pSound = sound;
    while (*pSound == '$' || *pSound == '#') {
        ++pSound;
    }
    std::strncpy(pTheme->Sound, pSound, sizeof(pTheme->Sound) - 1);
    pTheme->Sound[sizeof(pTheme->Sound) - 1] = '\0';

    pTheme->Scenario = pINI->ReadInteger(pSection, "Scenario", pTheme->Scenario);
    pTheme->Normal   = pINI->ReadBool(pSection, "Normal", pTheme->Normal);
    pTheme->Repeat   = pINI->ReadBool(pSection, "Repeat", pTheme->Repeat);

    // "Side" is stored as a side *name* which is resolved through the side
    // registry (INIClass_GetSide -> Side_From_Name).  An empty / unknown name
    // leaves the record's prior value in place.
    char side[0x80];
    side[0] = '\0';
    if (pINI->ReadString(pSection, "Side", "", side, sizeof(side)) != 0 &&
        side[0] != '\0') {
        const int32 resolved = SideClass::From_Name(side);
        pTheme->Side = (resolved != -1) ? resolved : pTheme->Side;
    }

    if (pTheme->Normal) {
        // The string-table entry is written directly into the record's wide
        // Name buffer (0x40 wide characters).
        char name[0x40];
        name[0] = '\0';
        pINI->ReadStringTableEntry(pSection, "Name", name, sizeof(name));

        int32 i = 0;
        while (name[i] != '\0' &&
               i < static_cast<int32>(sizeof(pTheme->FullName) /
                                      sizeof(pTheme->FullName[0])) - 1) {
            pTheme->FullName[i] = static_cast<wchar_t>(
                static_cast<unsigned char>(name[i]));
            ++i;
        }
        pTheme->FullName[i] = L'\0';
    }

    return true;
}

// ============================================================
// ThemeClass::Process - asm 0x7205A3
//
//   Walks the [Themes] section.  For each key the value names a theme record;
//   an existing record matching the name is reused, otherwise a fresh 0x290
//   byte record is allocated, its defaults seeded and the name copied in.
//   The record is then populated through Read_INI and appended to the vector.
// ============================================================
void ThemeClass::Process(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }

    const int32 count = pINI->GetKeyCount("Themes");
    if (count <= 0) {
        return;
    }

    for (int32 i = 0; i < count; ++i)
    {
        const char* pKeyName = pINI->GetKeyName("Themes", i);
        if (pKeyName == nullptr) {
            continue;
        }

        char value[0x20];
        value[0] = '\0';
        if (pINI->ReadString("Themes", pKeyName, "", value, sizeof(value)) == 0) {
            continue;
        }
        if (value[0] == '\0') {
            continue;
        }

        // Look for an existing record by name.
        int32 existing = -1;
        for (int32 k = 0; k < Count; ++k) {
            if (_strcmpi(Items[k]->Sound, value) == 0) {
                existing = k;
                break;
            }
        }

        ThemeControl* pTheme = nullptr;
        if (existing != -1) {
            pTheme = Items[existing];
        } else {
            pTheme = new ThemeControl();
            pTheme->Scenario   = 0;
            pTheme->Length     = 0.0f;
            pTheme->Name[0]    = L'\0';
            pTheme->Normal     = true;
            pTheme->Repeat     = false;
            pTheme->IsAvailable = false;
            pTheme->Side       = -1;
            pTheme->Sound[0]   = '\0';
            pTheme->FullName[0] = L'\0';

            std::strncpy(pTheme->Sound, value, sizeof(pTheme->Sound) - 1);
            pTheme->Sound[sizeof(pTheme->Sound) - 1] = '\0';
        }

        if (Read_INI(pINI, value, pTheme) && existing == -1) {
            Add(pTheme);
        } else if (existing == -1) {
            delete pTheme;
        }
    }
}

// ============================================================
// ThemeClass::Next_Song - asm 0x720989
//
//   Picks the next song ordinal for the requested side.  A non-negative
//   ordinal whose record is marked repeatable is kept; a negative ordinal (or
//   a non-repeatable current song) resumes sequentially from the current
//   ordinal, skipping disallowed themes, with a random fallback once the whole
//   playlist has been scanned.  Returns 0 when nothing is playable.
// ============================================================
int32 ThemeClass::Next_Song(int32 side)
{
    // If the current entry is repeatable and the requested side matches, keep
    // the current ordinal.
    if (side > -1) {
        if (!Items[side]->Repeat) {
            if (Unk10) {
                // fall through to the sequential scan
            } else {
                return side;
            }
        }
    }

    if (side >= 0 && Unk10) {
        return side;
    }

    // Random mode.
    if (Unk12) {
        for (int32 attempt = 0; attempt < 0x3E8; ++attempt) {
            int32 pick = 0;
            if (Count > 1) {
                pick = std::rand() % Count;
            }
            if (pick == side) {
                continue;
            }
            if (Is_Allowed(pick)) {
                return pick;
            }
        }
        return 0;
    }

    // Sequential scan wrapping around the playlist.
    int32 scanned = Count + 1;
    int32 ordinal = side;
    while (scanned-- != 0) {
        ++ordinal;
        if (ordinal >= Count) {
            ordinal = 0;
        }
        if (Is_Allowed(ordinal)) {
            return ordinal;
        }
    }

    return 0;
}

// ============================================================
// ThemeClass::Queue_Song - asm 0x720B24
//
//   Stores the requested song in [+08] and, when a stream is already running,
//   stops it so the new song can be queued.  Sentinels -1 / -2 are stored but
//   left for AI to resolve.
// ============================================================
void ThemeClass::Queue_Song(int32 index)
{
    Scenario = index;

    if (index == -1 || index == -2) {
        return;
    }

    // The original logs "Theme::QueueSong(%d)\n" before tearing down a running
    // stream.
    WWDebugString(THEME_QUEUE_FMT, index);

    if (SomeStream != nullptr) {
        Unk11 = true;
    }
}

// ============================================================
// ThemeClass::Play_Song - asm 0x720BBD
//
//   Begins playback of the requested ordinal.  The heavy lifting (opening the
//   .WAV through the audio backend) is handled by the stream helper the
//   original calls; this implementation records the selection and clears the
//   queue sentinel the AI uses.
// ============================================================
void ThemeClass::Play_Song(int32 index)
{
    if (index == -1 || index == -2) {
        return;
    }

    Track = index;
    Scenario = -2;
}

// ============================================================
// ThemeClass::Still_Playing - asm 0x720FB8
//
//   True while the current stream reports it is still running.
// ============================================================
bool ThemeClass::Still_Playing() const
{
    if (SomeStream == nullptr) {
        return false;
    }
    // Without the audio backend the stream is considered idle.
    return false;
}

// ============================================================
// High level playback helpers
// ============================================================

void ThemeClass::Play()
{
    if (Count <= 0) {
        return;
    }

    const int32 selected = Next_Song(Side);
    Play_Song(selected);
}

void ThemeClass::PlayTrack(int32 index)
{
    if (static_cast<uint32>(index) >= static_cast<uint32>(Count)) {
        return;
    }
    Play_Song(index);
}

void ThemeClass::Stop()
{
    Track = -1;
    Scenario = -1;
    Unk11 = true;
}

void ThemeClass::Next()
{
    if (Count <= 0) {
        return;
    }
    Play_Song(Next_Song(Track));
}

void ThemeClass::Previous()
{
    if (Count <= 0) {
        return;
    }

    int32 ordinal = Track - 1;
    if (ordinal < 0) {
        ordinal = Count - 1;
    }
    Play_Song(ordinal);
}
