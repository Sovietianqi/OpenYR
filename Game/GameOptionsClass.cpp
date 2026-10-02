#include "GameOptionsClass.h"
#include "../INI/INIClass.h"

// ============================================================================
// GameOptionsClass - implementation
// ============================================================================

GameOptionsClass::GameOptionsClass()
    : GameSpeed(1)
    , Difficulty(0)
    , CampDifficulty(0)
    , ScrollMethod(0)
    , ScrollRate(0)
    , AutoScroll(true)
    , DetailLevel(2)
    , SidebarCameoText(true)
    , UnitActionLines(true)
    , ShowHidden(false)
    , ToolTips(true)
    , ScreenWidth(640)
    , ScreenHeight(480)
    , StretchMovies(false)
    , AllowHiResModes(false)
    , AllowModeToggle(false)
    , AllowVRAMSidebar(false)
    , SoundVolume(1.0)
    , VoiceVolume(1.0)
    , ScoreVolume(1.0)
    , IsScoreRepeat(false)
    , IsScoreShuffle(false)
    , InGameMusic(true)
    , SoundLatency(0)
    , NetID(0)
    , Socket(0)
    , NetCard(0)
    , DestNet(0)
{
}

// ============================================================================
// LoadFromINI - Options_LoadFromINI
//
//   The original only clamps Difficulty right after reading it: anything
//   of 4 or more is pulled back to 4, and a non-positive value becomes 0.
//   Every other key is stored as-is.
// ============================================================================
void GameOptionsClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return;

    // ---------------------------------------------------------------
    // [Options]
    // ---------------------------------------------------------------
    GameSpeed = pINI->ReadInteger("Options", "GameSpeed", GameSpeed);

    {
        int32 value = pINI->ReadInteger("Options", "Difficulty", Difficulty);
        if (value >= 4)
            value = 4;
        if (value <= 0)
            value = 0;
        Difficulty = value;
    }

    CampDifficulty = pINI->ReadInteger("Options", "CampDifficulty", CampDifficulty);
    ScrollMethod   = pINI->ReadInteger("Options", "ScrollMethod",   ScrollMethod);
    ScrollRate     = pINI->ReadInteger("Options", "ScrollRate",     ScrollRate);
    AutoScroll     = pINI->ReadBool("Options", "AutoScroll",        AutoScroll);
    DetailLevel    = pINI->ReadInteger("Options", "DetailLevel",    DetailLevel);
    SidebarCameoText = pINI->ReadBool("Options", "SidebarCameoText", SidebarCameoText);
    UnitActionLines  = pINI->ReadBool("Options", "UnitActionLines",  UnitActionLines);
    ShowHidden       = pINI->ReadBool("Options", "ShowHidden",       ShowHidden);
    ToolTips         = pINI->ReadBool("Options", "ToolTips",         ToolTips);

    // ---------------------------------------------------------------
    // [Video]
    // ---------------------------------------------------------------
    ScreenWidth      = pINI->ReadInteger("Video", "ScreenWidth",      ScreenWidth);
    ScreenHeight     = pINI->ReadInteger("Video", "ScreenHeight",     ScreenHeight);
    StretchMovies    = pINI->ReadBool("Video", "StretchMovies",       StretchMovies);
    AllowHiResModes  = pINI->ReadBool("Video", "AllowHiResModes",     AllowHiResModes);
    AllowModeToggle  = pINI->ReadBool("Video", "AllowModeToggle",     AllowModeToggle);
    AllowVRAMSidebar = pINI->ReadBool("Video", "AllowVRAMSidebar",    AllowVRAMSidebar);

    // ---------------------------------------------------------------
    // [Audio]
    // ---------------------------------------------------------------
    SoundVolume  = pINI->ReadFixed("Audio", "SoundVolume",  SoundVolume);
    VoiceVolume  = pINI->ReadFixed("Audio", "VoiceVolume",  VoiceVolume);
    ScoreVolume  = pINI->ReadFixed("Audio", "ScoreVolume",  ScoreVolume);
    IsScoreRepeat  = pINI->ReadBool("Audio", "IsScoreRepeat",  IsScoreRepeat);
    IsScoreShuffle = pINI->ReadBool("Audio", "IsScoreShuffle", IsScoreShuffle);
    InGameMusic    = pINI->ReadBool("Audio", "InGameMusic",    InGameMusic);
    SoundLatency   = pINI->ReadInteger("Audio", "SoundLatency", SoundLatency);

    // ---------------------------------------------------------------
    // [Network]
    // ---------------------------------------------------------------
    NetID   = pINI->ReadInteger("Network", "NetID",   NetID);
    Socket  = pINI->ReadInteger("Network", "Socket",  Socket);
    NetCard = pINI->ReadInteger("Network", "NetCard", NetCard);
    DestNet = pINI->ReadInteger("Network", "DestNet", DestNet);
}
