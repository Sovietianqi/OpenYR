#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class CCINIClass;

// ============================================================================
// ScoreScreenClass - end-of-game score screen thresholds
//
//  The score screen classifies each player into one of four bands.  The four
//  cut-offs come from the [ScoreScreen] section of the multiplayer INI and
//  are stored alongside the screen's own layout so a saved selection can be
//  restored verbatim.
// ============================================================================

class ScoreScreenClass
{
public:
    ScoreScreenClass();

    // Reads the four band thresholds.  Every key keeps the value it already
    // holds as its fallback, so a partially specified section is stable.
    void LoadFromINI(CCINIClass* pINI, const char* pSection);

    // ------------------------------------------------------------------
    // Band thresholds
    // ------------------------------------------------------------------
    int32 WinningThreshold;
    int32 MegaWinningThreshold;
    int32 LosingThreshold;
    int32 MegaLosingThreshold;
};
