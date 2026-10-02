#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class CCINIClass;

static constexpr int32 PKT_MAX_CDS = 8;

// ============================================================================
// PKTNodeClass
//
//   One entry of the game-type list shipped inside a multiplayer scenario
//   (.PKT).  PKTNode_CTOR (asm 0x69A460) builds the node from a scenario
//   section of the game's own INI, then reads the [Digest] block embedded in
//   the scenario file itself.
//
//   Layout highlights from the original:
//     +000  wchar_t Description[0x80]        ("Description" / "DescriptionText")
//     +058  char    FileName[0x104]          (section name + ".MAP")
//     +15C  char    Digest[0x20]             ([Digest] 1, or "No Digest")
//     +17B  bool    DigestValid
//     +17C  bool    HasDescription
//     +180  int32   MinPlayers               ("MinPlayers")
//     +184  int32   MaxPlayers               ("MaxPlayers")
//     +188  DynamicVectorClass<int32> CDs    ("CD")
//     +1A4  DynamicVectorClass<char*> GameModes ("GameMode")
// ============================================================================

class PKTNodeClass
{
public:
    PKTNodeClass();
    ~PKTNodeClass();

    // PKTNode_CTOR: pSection names the scenario section to read.
    bool Construct(CCINIClass* pINI, const char* pSection);

    void Reset();

    wchar_t             Description[0x80];
    char                FileName[0x104];
    char                Digest[0x20];
    bool                DigestValid;
    bool                HasDescription;
    int32               MinPlayers;
    int32               MaxPlayers;

    DynamicVectorClass<int32>       CDs;        // "CD" comma list
    DynamicVectorClass<char*>       GameModes;  // "GameMode" comma list
};

// ============================================================================
// PKTNodePool
//
//   The container Game_ParsePKTs (asm 0x69991E) fills.  The original grabs
//   the pool off the session object at +0x690, resets the INIClass and walks
//   three sources in this order:
//
//     1. "MISSIONSMD.PKT" - always read first, its whole [MultiMaps] section
//        becomes nodes.
//     2. every other "*.PKT" in the working directory (case-insensitively
//        skipping MISSIONSMD.PKT itself).
//     3. every "*.YRO" in the working directory (skipping MISSIONS.YRO),
//        mounted as a MixFileClass archive; each archive contributes the
//        "*.PKT" it carries, and the ".PKT" tail of the archive name selects
//        which one gets the player-range string.
//
//   A node is only appended when the scenario file behind it exists, i.e.
//   Construct() finds a real ".MAP".  Nodes whose player range is a single
//   value get the "CD" wide format, the rest get the "CDDC" one.
// ============================================================================

namespace PKTPool
{
    // Game_ParsePKTs: fills pPool from the three sources above.
    void Parse(DynamicVectorClass<PKTNodeClass*>* pPool);

    // The two wide format strings the original builds for a node's player
    // range: "CD" when MinPlayers == MaxPlayers, "CDDC" otherwise.
    extern const wchar_t* const RANGE_FORMAT_SINGLE;   // L"CD"
    extern const wchar_t* const RANGE_FORMAT_RANGE;    // L"CDDC"
    extern const wchar_t* const FILL_CHAR;             // L" "
    extern const char*    const SCENARIO_SOURCE;       // "D:\\ra2mdpost\\Session.CPP"
    extern const char*    const NO_DESCRIPTION;        // "MSG:NoDescription"
}
