#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class CCINIClass;

// ============================================================================
// GameOptionsClass - the persistent SUN.INI settings
//
//  Every entry is a user preference written by the options dialog and read
//  back at start-up.  Options_LoadFromINI merges the file over this object,
//  exactly as the original does, so a missing key leaves the current value
//  untouched.
// ============================================================================

class GameOptionsClass
{
public:
    GameOptionsClass();

    // Reads [Options], [Video], [Audio] and [Network] from the INI file.
    void LoadFromINI(CCINIClass* pINI);

    // ------------------------------------------------------------------
    // [Options]
    // ------------------------------------------------------------------
    int32  GameSpeed;
    int32  Difficulty;
    int32  CampDifficulty;
    int32  ScrollMethod;
    int32  ScrollRate;
    bool   AutoScroll;
    int32  DetailLevel;
    bool   SidebarCameoText;
    bool   UnitActionLines;
    bool   ShowHidden;
    bool   ToolTips;

    // ------------------------------------------------------------------
    // [Video]
    // ------------------------------------------------------------------
    int32  ScreenWidth;
    int32  ScreenHeight;
    bool   StretchMovies;
    bool   AllowHiResModes;
    bool   AllowModeToggle;
    bool   AllowVRAMSidebar;

    // ------------------------------------------------------------------
    // [Audio]
    // ------------------------------------------------------------------
    double SoundVolume;
    double VoiceVolume;
    double ScoreVolume;
    bool   IsScoreRepeat;
    bool   IsScoreShuffle;
    bool   InGameMusic;
    int32  SoundLatency;

    // ------------------------------------------------------------------
    // [Network]
    // ------------------------------------------------------------------
    int32  NetID;
    int32  Socket;
    int32  NetCard;
    int32  DestNet;
};
