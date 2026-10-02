#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class CCINIClass;

//============================================================================
// ThemeType / ThemeState
//
//   The theme "Side" selector stored on each theme record.  -1 (0xFFFFFFFF)
//   means "any side"; 0 = Allies, 1 = Soviet.
//============================================================================
enum class ThemeType : int32 {
    Peace = 0,
    Battle = 1,
    Both = 2
};

//============================================================================
// ThemeControl
//
//   One playlist entry.  The record is exactly 0x290 bytes:
//     +0x000  char       Sound[0x100]   file name (without extension)
//     +0x100  wchar_t    Name[0x80]     display name (0x40 wide chars)
//     +0x200  wchar_t    FullName[0x40] the string-table resolved name
//     +0x280  int32      Scenario       minimum scenario ordinal
//     +0x284  float      Length         track length in seconds
//     +0x288  bool       Normal         available in the normal rotation
//     +0x289  bool       Repeat         may repeat
//     +0x28A  bool       IsAvailable    set by the scanner once the file loads
//     +0x28C  int32      Side           side restriction (-1 = any)
//============================================================================
// The playlist vector grows in 0x0A steps; 128 is the practical upper bound the
// original's fixed free-store never exceeds.
static constexpr int32 MAX_THEMES = 128;

//============================================================================
// ThemeControl
//
//   One playlist entry.  The record is exactly 0x290 bytes:
//     +0x000  char       Sound[0x100]   file name (without extension)
//     +0x100  wchar_t    Name[0x80]     display name (0x40 wide chars)
//     +0x200  wchar_t    FullName[0x40] the string-table resolved name
//     +0x280  int32      Scenario       minimum scenario ordinal
//     +0x284  float      Length         track length in seconds
//     +0x288  bool       Normal         available in the normal rotation
//     +0x289  bool       Repeat         may repeat
//     +0x28A  bool       IsAvailable    set by the scanner once the file loads
//     +0x28C  int32      Side           side restriction (-1 = any)
//============================================================================
struct ThemeControl
{
    char    Sound[0x100];
    wchar_t Name[0x80];
    wchar_t FullName[0x40];
    int32   Scenario;
    float   Length;
    bool    Normal;
    bool    Repeat;
    bool    IsAvailable;
    int32   Side;
};

//============================================================================
// ThemeClass
//
//   The audio playlist manager.  Field offsets match the original layout:
//     +0x00  int32  Track        current song ordinal (-1 = none)
//     +0x04  int32  Side         requested side (-1 = any)
//     +0x08  int32  Scenario     requested scenario (-1 = none)
//     +0x0C  int32  Repeat       repeat counter (0xFF = infinite)
//     +0x10  bool   Unk10
//     +0x11  bool   Unk11        "stopped" latch
//     +0x12  bool   Unk12
//     +0x14  DynamicVectorClass<ThemeControl*> Themes   (vftable +0x14)
//     +0x24  int32  Count
//     +0x28  int32  Capacity (0x0A)
//     +0x2C  void*  SomeStream   currently playing stream handle
//============================================================================
class ThemeClass {
public:
    ThemeClass();
    ~ThemeClass();

    static ThemeClass* GetInstance();

    //========================================================================
    // Playlist access (the original's DynamicVectorClass<ThemeControl*>)
    //========================================================================

    ThemeControl* GetItem(int32 index) const
    {
        if (index < 0 || index >= Count || Items == nullptr)
            return nullptr;
        return Items[index];
    }

    int32 GetCount() const { return Count; }
    int32 GetPlaylistCount() const { return Count; }

    // Append an entry; returns false when the fixed capacity is exhausted.
    bool Add(ThemeControl* pTheme);

    void Clear();

    //========================================================================
    // Original accessors
    //========================================================================

    // ThemeClass::Base_Name (asm 0x72093C): the bare "Sound" file name of a
    // theme, or "No theme" when the ordinal is out of range.
    const char* Base_Name(int32 index) const;

    // ThemeClass::Full_Name (asm 0x7209AC): the string-table display name, or
    // null when the ordinal is out of range.
    const wchar_t* Full_Name(int32 index) const;

    // ThemeClass::Theme_File_Name (asm 0x720E2C): "<Sound>.WAV" built through
    // _makepath, or the empty string when the ordinal is out of range.
    const char* Theme_File_Name(int32 index);

    // ThemeClass::Track_Length (asm 0x720E9D): the floored track length in
    // seconds, or 0 when the ordinal is out of range.
    int32 Track_Length(int32 index) const;

    // ThemeClass::Is_Allowed (asm 0x7210B4): true when the theme may play for
    // the current side / scenario.
    bool Is_Allowed(int32 index) const;

    // ThemeClass::From_Name (asm 0x7212B6): the ordinal of a theme by its bare
    // name, or -1 when it is not in the playlist.
    int32 From_Name(const char* pName) const;

    // ThemeClass::FindIndex - the ordinal of a named theme, or -1.
    static int32 FindIndex(const char* pName);

    //========================================================================
    // INI ingestion
    //========================================================================

    // ThemeClass::Read_INI (asm 0x7204A2): reads one theme record from the
    // supplied section into pTheme.
    bool Read_INI(CCINIClass* pINI, const char* pSection, ThemeControl* pTheme);

    // ThemeClass::Process (asm 0x7205A3): walks the [Themes] list, reusing an
    // existing record when the name matches or allocating a new one otherwise,
    // then reading each record's section.
    void Process(CCINIClass* pINI);

    //========================================================================
    // Playback
    //========================================================================

    void Play();
    void PlayTrack(int32 index);
    void Stop();
    void Next();
    void Previous();

    int32 Next_Song(int32 side);
    void Queue_Song(int32 index);
    void Play_Song(int32 index);
    bool Still_Playing() const;

    //========================================================================
    // Layout (matches the original offsets)
    //========================================================================

    int32           Track;
    int32           Side;
    int32           Scenario;
    int32           Repeat;
    bool            Unk10;
    bool            Unk11;
    bool            Unk12;

    static constexpr int32 MAX_THEME_DATA = 128;

    ThemeControl**  Items;      // +0x18 (DynamicVectorClass storage)
    int32           Count;      // +0x24
    int32           Capacity;   // +0x28
    void*           SomeStream; // +0x2C
};
