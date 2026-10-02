#include "ScoreScreenClass.h"
#include "../INI/INIClass.h"

// ============================================================================
// ScoreScreenClass - implementation
// ============================================================================

ScoreScreenClass::ScoreScreenClass()
    : WinningThreshold(0)
    , MegaWinningThreshold(0)
    , LosingThreshold(0)
    , MegaLosingThreshold(0)
{
}

// ============================================================================
// LoadFromINI
//
//   Order matches the original: winning, mega winning, losing, mega losing.
// ============================================================================
void ScoreScreenClass::LoadFromINI(CCINIClass* pINI, const char* pSection)
{
    if (pINI == nullptr || pSection == nullptr)
        return;

    WinningThreshold     = pINI->ReadInteger(pSection, "WinningThreshold",     WinningThreshold);
    MegaWinningThreshold = pINI->ReadInteger(pSection, "MegaWinningThreshold", MegaWinningThreshold);
    LosingThreshold      = pINI->ReadInteger(pSection, "LosingThreshold",      LosingThreshold);
    MegaLosingThreshold  = pINI->ReadInteger(pSection, "MegaLosingThreshold",  MegaLosingThreshold);
}
