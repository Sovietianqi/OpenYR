#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class CCINIClass;

// ============================================================================
// WDTCampaignStats
//
//   Persistent per-player progress for one World Domination Tour campaign,
//   stored in WDTHist.INI under the player's name.  Read by sub_764B90
//   (asm 0x764B90) when a WDT campaign object is constructed.
//
//   The section is named after the player; inside it:
//     "CampaignID"     - the campaign index (clamped: negative -> 0)
//     "LastDay"        - four consecutive day counters (indices 0..3)
//     "PlayerFaction"  - the chosen faction index
// ============================================================================

class WDTCampaignStats
{
public:
    WDTCampaignStats();
    ~WDTCampaignStats();

    // sub_764B90: loads the stats for pPlayerName out of WDTHist.INI.
    bool LoadFromINI(const char* pPlayerName);

    // Persists the current stats back to WDTHist.INI.
    void SaveToINI(CCINIClass* pINI, const char* pPlayerName) const;

    void Reset();

    int32   CampaignID;
    int32   LastDay[4];
    int32   PlayerFaction;
};
