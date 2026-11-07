#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class CCINIClass;

// ============================================================================
// GameTypePrefsClass
//
//   Per-session-type preference block, filled by Game_GetGameTypePrefs
//   (asm 0x69831C).  The object layout and the eight per-player slot entries
//   mirror the original byte-for-byte:
//
//     +00  GameMode            int32
//     +04  ScenIndex           int32
//     +08  GameSpeed           int32
//     +0C  Credits             int32
//     +10  UnitCount           int32
//     +14  ShortGame           bool
//     +15  SuperWeaponsAllowed bool
//     +16  BuildOffAlly        bool
//     +17  MCVRepacks          bool
//     +18  CratesAppear        bool
//     +1C  Slot[i].First       int32   (i = 1..7, stride 0x0C)
//     +20  Slot[i].Second      int32
//     +24  Slot[i].Third       int32
//     +44  idxGameMode         int32
// ============================================================================

struct GameTypeSlot
{
    int32 First;
    int32 Second;
    int32 Third;
};

class GameTypePrefsClass
{
public:
    GameTypePrefsClass();

    // Game_GetGameTypePrefs: reads the preference block for one session type.
    //   nSlotDefault    -> fallback for slot component 1 (Skirmish=1, else 2)
    //   nHashDefault    -> fallback for the display name slot   (Skirmish=6)
    void ReadFromINI(CCINIClass* pINI, const char* pSection,
                     int32 nSlotDefault, int32 nHashDefault);

    void Reset();

    // ------------------------------------------------------------------------
    // asm: GameMode_SaveGameSettings (真实入口 ≈0x698F5E)
    //
    //   ; int __thiscall GameMode_SaveGameSettings(GameTypePrefsClass *this,
    //   ;                                          CCINIClass *INI,
    //   ;                                          char *section)
    //   ; CODE XREF: Game_SaveNetworkConfig+135
    //   尾部 retn 8 -> 3 个参数（this + INI + section）。
    //
    //   （AsmComplex3/4 原标注 0x698FB0 有误，该地址在 asm 中不存在；
    //     且批注把 this 写成了 const void* pSettings —— 类型全错。）
    //
    //   5 × INIClass::WriteInteger(section, key, value, 0)：
    //     "GameMode"(+00) "ScenIndex"(+04) "GameSpeed"(+08)
    //     "Credits"(+0C)  "UnitCount"(+10)
    //   5 × INIClass::WriteBool(section, key, byte)：
    //     "ShortGame"(+14) "SuperWeaponsAllowed"(+15) "BuildOffAlly"(+16)
    //     "MCVRepacks"(+17) "CratesAppear"(+18)
    //   for (n = 1 ; n < 8 ; ++n):
    //     sprintf(name, "Slot%02d", n) ; GameMode_SaveNthSlotIntoINI(
    //         section, name, Slot[n].First, Slot[n].Second, Slot[n].Third)
    // ------------------------------------------------------------------------
    void SaveGameSettings(CCINIClass* pINI, const char* pSection);

    // asm: GameMode_SaveNthSlotIntoINI (0x698F20 附近)
    //   ; int __stdcall GameMode_SaveNthSlotIntoINI(char *section, char *key,
    //   ;                                            int, int, int)
    //   内部走 INIClass::WriteString("%d,%d,%d") —— 注意不是 WriteInteger。
    static void SaveNthSlotIntoINI(CCINIClass* pINI, const char* pSection,
                                   const char* pKey,
                                   int32 a, int32 b, int32 c);

    int32           GameMode;
    int32           ScenIndex;
    int32           GameSpeed;
    int32           Credits;
    int32           UnitCount;
    bool            ShortGame;
    bool            SuperWeaponsAllowed;
    bool            BuildOffAlly;
    bool            MCVRepacks;
    bool            CratesAppear;

    // Eight slots; index 0 is unused because the original's loop runs 1..7.
    GameTypeSlot    Slot[8];

    // The resolved mode identifier (the low dword of the mode object's name
    // slot).  Stored at +0x44 in the original.
    int32           idxGameMode;
};
