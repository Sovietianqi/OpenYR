#include "GameTypePrefsClass.h"
#include "MPGameModeClass.h"
#include "../INI/INIClass.h"
#include "../Rules/RulesClass.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// GameTypePrefsClass
// ============================================================================

GameTypePrefsClass::GameTypePrefsClass()
    : GameMode(0)
    , ScenIndex(0)
    , GameSpeed(0)
    , Credits(0)
    , UnitCount(0)
    , ShortGame(false)
    , SuperWeaponsAllowed(true)
    , BuildOffAlly(false)
    , MCVRepacks(false)
    , CratesAppear(false)
    , idxGameMode(0)
{
    for (int32 i = 0; i < 8; ++i) {
        Slot[i].First  = 0;
        Slot[i].Second = 0;
        Slot[i].Third  = 0;
    }
}

void GameTypePrefsClass::Reset()
{
    GameMode            = 0;
    ScenIndex           = 0;
    GameSpeed           = 0;
    Credits             = 0;
    UnitCount           = 0;
    ShortGame           = false;
    SuperWeaponsAllowed = true;
    BuildOffAlly        = false;
    MCVRepacks          = false;
    CratesAppear        = false;
    idxGameMode         = 0;

    for (int32 i = 0; i < 8; ++i) {
        Slot[i].First  = 0;
        Slot[i].Second = 0;
        Slot[i].Third  = 0;
    }
}

// ============================================================================
// Game_GetGameTypePrefs - asm 0x69831C
//
//   INIClass_Reset(INI)
//   MPGameMode_ResetList()
//   GameMode            = GetInteger(section, "GameMode",            mode[+28h])
//   ScenIndex           = GetInteger(section, "ScenIndex",           0)
//   GameSpeed           = GetInteger(section, "GameSpeed",           Rules+14A0h)
//   Credits             = GetInteger(section, "Credits",             Rules+1484h)
//   UnitCount           = GetInteger(section, "UnitCount",           Rules+1494h)
//   ShortGame           = GetBool   (section, "ShortGame",           Rules+14B6h)
//   SuperWeaponsAllowed = GetBool   (section, "SuperWeaponsAllowed", Rules+14B9h)
//   BuildOffAlly        = GetBool   (section, "BuildOffAlly",        Rules+14BAh)
//   MCVRepacks          = GetBool   (section, "MCVRepacks",          Rules+14B8h)
//   CratesAppear        = GetBool   (section, "CratesAppear",        Rules+14B1h)
//   for (i = 1; i < 8; ++i) {
//       sprintf(key, "Slot%02d", i);
//       a4 = (i == 1) ? arg_C : arg_8;
//       sub_477440(INI, section, key, &first, &second, &third);
//       this->Slot[i]          = { a4, first, second };
//       this->SlotHash[i + 9]  = third;
//   }
//
//   The two caller supplied defaults are the only values that differ between
//   the Skirmish, LAN and WonlinePref blocks: Skirmish is invoked with
//   (a4 = 1, a5 = 6) while LAN and WonlinePref both pass (2, 2).
// ============================================================================
void GameTypePrefsClass::ReadFromINI(CCINIClass* pINI, const char* pSection,
                                     int32 nSlotDefault, int32 nHashDefault)
{
    if (pINI == nullptr || pSection == nullptr)
        return;

    pINI->Reset();
    MPGameModeClass::ResetList();

    RulesClass* pRules = RulesClass::Instance;

    const int32 modeDefault =
        pRules != nullptr ? static_cast<int32>(pRules->GameSpeed) : 0;

    GameMode = pINI->ReadInteger(pSection, "GameMode", modeDefault);

    MPGameModeClass* pMode = MPGameModeClass::Find(GameMode);
    idxGameMode = (pMode != nullptr) ? pMode->Field_28 : 0;

    ScenIndex = pINI->ReadInteger(pSection, "ScenIndex", 0);

    GameSpeed = pINI->ReadInteger(pSection, "GameSpeed",
                                  pRules != nullptr ? pRules->GameSpeed : 0);
    Credits   = pINI->ReadInteger(pSection, "Credits",
                                  pRules != nullptr ? pRules->Money : 0);
    UnitCount = pINI->ReadInteger(pSection, "UnitCount",
                                  pRules != nullptr ? pRules->UnitCount : 0);

    ShortGame           = pINI->ReadBool(pSection, "ShortGame",
                                         pRules != nullptr ? pRules->ShortGame : false);
    SuperWeaponsAllowed = pINI->ReadBool(pSection, "SuperWeaponsAllowed",
                                         pRules != nullptr ? pRules->SuperWeaponsAllowed : true);
    BuildOffAlly        = pINI->ReadBool(pSection, "BuildOffAlly",
                                         pRules != nullptr ? pRules->BuildOffAlly : false);
    MCVRepacks          = pINI->ReadBool(pSection, "MCVRepacks",
                                         pRules != nullptr ? pRules->MCVRedeploys : false);
    CratesAppear        = pINI->ReadBool(pSection, "CratesAppear",
                                         pRules != nullptr ? pRules->Crates : false);

    for (int32 i = 1; i < 8; ++i)
    {
        char key[0x10];
        std::sprintf(key, "Slot%02d", i);

        const int32 slotDefault = (i == 1) ? nHashDefault : nSlotDefault;

        int32 first  = slotDefault;
        int32 second = -2;
        int32 third  = -2;

        pINI->ReadSlotTriple(pSection, key, &first, &second, &third);

        Slot[i].First  = slotDefault;
        Slot[i].Second = first;
        Slot[i].Third  = second;
        idxGameMode    = third;
    }
}
