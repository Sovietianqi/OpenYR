#include "WDTCampaignStats.h"
#include "../INI/INIClass.h"

#include <cstdio>

// ============================================================================
// WDTCampaignStats - sub_764B90 (WDTHist.INI)
// ============================================================================

WDTCampaignStats::WDTCampaignStats()
{
    Reset();
}

WDTCampaignStats::~WDTCampaignStats()
{
}

void WDTCampaignStats::Reset()
{
    CampaignID    = 0;
    LastDay[0]    = 0;
    LastDay[1]    = 0;
    LastDay[2]    = 0;
    LastDay[3]    = 0;
    PlayerFaction = 0;
}

// ============================================================================
// LoadFromINI
//
//   The original only consults WDTHist.INI when the file exists; a missing
//   file leaves the four day counters at zero.  CampaignID is clamped so a
//   negative value becomes zero, and the day counters only load when the
//   stored campaign id still matches the campaign being played.
// ============================================================================
bool WDTCampaignStats::LoadFromINI(const char* pPlayerName)
{
    if (pPlayerName == nullptr || pPlayerName[0] == '\0') {
        Reset();
        return false;
    }

    CCINIClass* pINI = CCINIClass::LoadINIFile("WDTHist.INI");
    if (pINI == nullptr) {
        Reset();
        return false;
    }

    CCINIClass& ini = *pINI;
    const char* section = pPlayerName;

    const int32 storedCampaignID =
        ini.ReadInteger(section, "CampaignID", -1);
    CampaignID = (storedCampaignID < 0) ? 0 : storedCampaignID;

    PlayerFaction = ini.ReadInteger(section, "PlayerFaction", PlayerFaction);

    for (int32 i = 0; i < 4; ++i)
    {
        char key[0x20];
        std::sprintf(key, "LastDay%d", i);

        const int32 value = ini.ReadInteger(section, key, 0);
        LastDay[i] = (value < 0) ? 0 : value;
    }

    CCINIClass::UnloadINIFile(pINI);
    return true;
}

void WDTCampaignStats::SaveToINI(CCINIClass* pINI, const char* pPlayerName) const
{
    if (pINI == nullptr || pPlayerName == nullptr || pPlayerName[0] == '\0') {
        return;
    }

    const char* section = pPlayerName;

    pINI->WriteInteger(section, "CampaignID", CampaignID);

    for (int32 i = 0; i < 4; ++i) {
        char key[0x20];
        std::sprintf(key, "LastDay%d", i);
        pINI->WriteInteger(section, key, LastDay[i]);
    }

    pINI->WriteInteger(section, "PlayerFaction", PlayerFaction);
}
