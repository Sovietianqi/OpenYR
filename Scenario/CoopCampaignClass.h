#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class CCINIClass;
class HouseTypeClass;

// ============================================================================
// CoopCampaignMap
//
//   One mission inside a cooperative campaign.  The original lays out three
//   fixed-size 0x104 byte name buffers per map entry, addressed through
//   offsets -0x30C, -0x208 and -0x104 from the map array cursor.
// ============================================================================

struct CoopCampaignMap
{
    char FileName[0x104];
    char Description[0x104];
    char Extra[0x104];
};

// ============================================================================
// CoopCampaignClass
//
//   One entry of the [Campaigns] list in CoopCampMD.ini.  The list is walked
//   by CoopCampaignClass::CreateFromINIList (asm 0x49DB19).
//
//   Layout of one entry (stride 0x4C):
//     +00  Index
//     +04  wchar_t* pName          (CampaignName, resolved via string table)
//     +08  NumberOfCampaignMaps
//     +0C  CoopCampaignMap* Maps
//     +10  DynamicVectorClass<char*>
//     +40  char* pCampaignAI
//     +44  CampaignLoadScreen
//     +48  CampaignLoadScreenPallet
// ============================================================================

class CoopCampaignClass
{
public:
    CoopCampaignClass();
    ~CoopCampaignClass();

    // CoopCampaignClass_LoadFromINIList: reads CoopCampMD.ini.
    static bool CreateFromINIList(CCINIClass* pINI);
    static CoopCampaignClass* Get(int32 index);
    static int32 GetCount();
    static void Clear();

    int32               Index;
    wchar_t*            pName;              // "CampaignName"
    int32               NumberOfCampaignMaps; // "NumberOfCampaignMaps"
    CoopCampaignMap*    Maps;               // "Map%d" pre-comma split
    char*               pCampaignAI;        // "CampaignAI" (default "Easy")
    char                CampaignLoadScreen[0x80];
    char                CampaignLoadScreenPallet[0x80];

    static DynamicVectorClass<CoopCampaignClass*>* Array;
};

// ============================================================================
// CoopCampaignDialog
//
//   The cooperative-game setup dialog helper sub_5C23B0 (asm 0x5C23B0).
//   It primes a combo box from the map entries of one campaign, then walks
//   the house list selected by the caller and resolves each entry back to a
//   house ordinal through INIClass_FindHouseIndex, writing the house's
//   display name (+0x60) into the list.
// ============================================================================

namespace CoopCampaignDialog
{
    // The two string-table formats the original builds for a map entry:
    // "NAME:COOPDESC%d%d" for the caption and the plain-name variant.
    extern const char* const DESC_FORMAT;      // "NAME:COOPDESC%d%d"
    extern const char* const RANDOM_ENTRY;     // "GUI:Random"
    extern const char* const SOURCE_FILE;      // "D:\\ra2mdpost\\MPCoop.cpp"

    // Builds the "NAME:COOPDESC%d%d" string-table key of one campaign map.
    void BuildMapKey(CoopCampaignClass* pCampaign, int32 nMapOrdinal,
                     char* pBuffer, size_t nSize);

    // sub_5C23B0's ordinal resolution: the house ordinal behind a name, or
    // -1 when the name is not a registered house.
    int32 ResolveHouseOrdinal(const char* pName);
}

// ============================================================================
// GameInfoClass
//
//   The cooperative-campaign progress record.  The original keeps one of
//   these inside the multiplayer session, addressed through these offsets:
//
//     +00  ScenarioName[0xA8]   scenario / campaign key
//     +A8  PlayerName[0x20]     the local player's name
//     +1C  (see below)          the "second" scenario key used by Save()
//     +14  House1               HouseTypeClass* (Country #1)
//     +18  Color1
//     +30  House2               HouseTypeClass* (Country #2)
//     +34  Color2
//     +38  Score                CurrentMap
//     +3C  PPFile strings       "Map%d" values, one char* per campaign map
//     +40  MapCount
//     +44  CampaignIndex        index into CoopCampaignClass::Array
//     +48  Kills1
//     +4C  Kills2
//     +50  Built1
//     +54  Built2
//     +58  Lost1
//     +5C  Lost2
//     +60  Score1
//     +64  Score2
//     +68  Time
//     +6C  Loaded               non-zero once the record is readable/writable
//
//   Three functions operate on the record, all reproduced here:
//     GameInfoClass::Load   (asm 0x49C0D0)  coopsave.ini -> record
//     GameInfoClass::Exists (asm 0x49D390)  probes [section]CurrentMap
//     GameInfoClass::Save   (asm 0x49D680)  record -> packet string
// ============================================================================

class GameInfoClass
{
public:
    GameInfoClass();
    ~GameInfoClass();

    // GameInfoClass::Load (asm 0x49C0D0)
    bool Load();

    // GameInfoClass::Exists (asm 0x49D390) - returns true when the named
    // section holds a "Map%d" entry that is not the literal "undefined".
    bool Exists(char* pDest);

    // GameInfoClass::Save (asm 0x49D680) - builds the "C0,..." option string
    // and pushes it through the session bridge.
    bool Save();

    // GameInfoClass::SaveAISettings (asm 0x5C25E0) - builds the "C1,..." AI
    // settings string: the campaign's map count, then one ",<map>,<ai>"
    // segment per map with a duplicate-free AI ordinal drawn through the
    // caller's RNG (the original uses Scenario->RandomSlot(0, 7)).
    bool SaveAISettings(CoopCampaignClass* pCampaign,
                        const int32* pMapOrdinals,
                        int32 (*pDrawAI)(int32 nMin, int32 nMax));

    // ── +00 ──────────────────────────────────────────────────────────────
    char                ScenarioName[0xA8];

    // ── +14 ──────────────────────────────────────────────────────────────
    HouseTypeClass*     pHouse1;
    int32               Color1;

    // ── +1C ──────────────────────────────────────────────────────────────
    char                PlayerName[0x20];

    // ── +30 ──────────────────────────────────────────────────────────────
    HouseTypeClass*     pHouse2;
    int32               Color2;

    // ── +38 ──────────────────────────────────────────────────────────────
    int32               Score;              // "CurrentMap"
    char**              MapEntries;         // "Map%d" strings
    int32               MapCount;
    int32               CampaignIndex;      // "House1" lookup result

    // ── +48 ──────────────────────────────────────────────────────────────
    int32               Kills1;
    int32               Kills2;
    int32               Built1;
    int32               Built2;
    int32               Lost1;
    int32               Lost2;
    int32               Score1;
    int32               Score2;
    int32               Time;
    bool                Loaded;             // +6C
};
