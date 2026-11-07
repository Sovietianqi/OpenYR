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

// ============================================================================
// asm: GameMode_SaveNthSlotIntoINI
//
//   ; int __stdcall GameMode_SaveNthSlotIntoINI(char *section, char *key,
//   ;                                            int, int, int)
//   （asm 行 277123；调用者 GameMode_SaveGameSettings+...）
//   内部走 INIClass::WriteString 写 "%d,%d,%d"     —— 不是 WriteInteger。
// ============================================================================
void GameTypePrefsClass::SaveNthSlotIntoINI(CCINIClass* pINI,
                                            const char* pSection,
                                            const char* pKey,
                                            int32 a, int32 b, int32 c)
{
    if (pINI == nullptr)
        return;

    // asm: sprintf(buf, "%d,%d,%d", a, b, c) / call INIClass_WriteString
    char buf[0x40];
    std::sprintf(buf, "%d,%d,%d", static_cast<int>(a),
                 static_cast<int>(b), static_cast<int>(c));
    pINI->WriteString(pSection, pKey, buf);
}

// ============================================================================
// asm: GameMode_SaveGameSettings (真实入口 ≈0x698F5E)
//
//   ; int __thiscall GameMode_SaveGameSettings(GameTypePrefsClass *this,
//   ;                                          CCINIClass *INI, char *section)
//   ; CODE XREF: Game_SaveNetworkConfig+135
//   尾部 retn 8 -> 3 个参数。
//
// 修正 AsmComplex4 的两处错误：
//   1. 地址 0x698FB0 在 asm 中不存在（真实 ≈0x698F5E）。
//   2. 批注把 this 写成 const void* pSettings —— 真实类型是
//      GameTypePrefsClass*；INI 是 CCINIClass*；section 是 char*。
// ============================================================================
void GameTypePrefsClass::SaveGameSettings(CCINIClass* pINI,
                                          const char* pSection)
{
    if (pINI == nullptr)
        return;

    // asm: five INIClass::WriteInteger(section, key, value, 0) calls
    pINI->WriteInteger(pSection, "GameMode",  GameMode,  false);
    pINI->WriteInteger(pSection, "ScenIndex", ScenIndex, false);
    pINI->WriteInteger(pSection, "GameSpeed", GameSpeed, false);
    pINI->WriteInteger(pSection, "Credits",   Credits,   false);
    pINI->WriteInteger(pSection, "UnitCount", UnitCount, false);

    // asm: five INIClass::WriteBool(section, key, byte) calls
    pINI->WriteBool(pSection, "ShortGame",           ShortGame);
    pINI->WriteBool(pSection, "SuperWeaponsAllowed", SuperWeaponsAllowed);
    pINI->WriteBool(pSection, "BuildOffAlly",        BuildOffAlly);
    pINI->WriteBool(pSection, "MCVRepacks",          MCVRepacks);
    pINI->WriteBool(pSection, "CratesAppear",        CratesAppear);

    // asm: mov ebp,1 / add ebx,2Ch ; loop while (ebp < 8), ebx += 0Ch
    for (int32 n = 1; n < 8; ++n)
    {
        // asm: push ebp / push offset aSlot02d / lea ecx,[var_10] / call _sprintf
        char key[0x10];
        std::sprintf(key, "Slot%02d", static_cast<int>(n));

        // asm: call GameMode_SaveNthSlotIntoINI(section, name,
        //                                        Slot[n].First/Second/Third)
        SaveNthSlotIntoINI(pINI, pSection, key,
                           Slot[n].First, Slot[n].Second, Slot[n].Third);
    }
}
