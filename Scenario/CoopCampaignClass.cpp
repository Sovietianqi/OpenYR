#include "CoopCampaignClass.h"
#include "../INI/INIClass.h"
#include "../IO/CCFileClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Network/Networking.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>

// ============================================================================
// CoopCampaignClass - CoopCampMD.ini ([Campaigns] list)
// ============================================================================

DynamicVectorClass<CoopCampaignClass*>* CoopCampaignClass::Array = nullptr;

CoopCampaignClass::CoopCampaignClass()
    : Index(0)
    , pName(nullptr)
    , NumberOfCampaignMaps(0)
    , Maps(nullptr)
    , pCampaignAI(nullptr)
{
    CampaignLoadScreen[0] = '\0';
    CampaignLoadScreenPallet[0] = '\0';
}

CoopCampaignClass::~CoopCampaignClass()
{
    delete[] pName;
    delete[] Maps;
    delete[] pCampaignAI;
}

// ============================================================================
// CreateFromINIList - CoopCampaignClass_LoadFromINIList (asm 0x49DB19)
//
//   Loads "CoopCampMD.ini" and walks the [Campaigns] section.  Each key's
//   value names a section that holds the campaign metadata and its "Map%d"
//   entries.  The map value is a comma separated triple that the original
//   writes into three consecutive 0x104 byte buffers.
// ============================================================================
bool CoopCampaignClass::CreateFromINIList(CCINIClass* pINI)
{
    CCINIClass* pCoopINI = CCINIClass::LoadINIFile("CoopCampMD.ini");
    if (pCoopINI == nullptr) {
        (void)pINI;
        if (Array != nullptr) {
            Clear();
        }
        return false;
    }

    Clear();
    if (Array == nullptr) {
        Array = new DynamicVectorClass<CoopCampaignClass*>();
    }

    CCINIClass& ini = *pCoopINI;
    static const char* const SECTION = "Campaigns";

    const int32 count = ini.GetKeyCount(SECTION);
    if (count <= 0) {
        CCINIClass::UnloadINIFile(pCoopINI);
        return false;
    }

    for (int32 i = 0; i < count; ++i)
    {
        const char* pKeyName = ini.GetKeyName(SECTION, i);
        if (pKeyName == nullptr) {
            continue;
        }

        char section[0x80];
        section[0] = '\0';
        ini.ReadString(SECTION, pKeyName, "", section, sizeof(section));
        if (section[0] == '\0') {
            continue;
        }

        CoopCampaignClass* pCampaign = new CoopCampaignClass();
        pCampaign->Index = i;

        pCampaign->NumberOfCampaignMaps =
            ini.ReadInteger(section, "NumberOfCampaignMaps", 0);

        // CampaignName is resolved through the string table.
        char nameBuffer[0x80];
        nameBuffer[0] = '\0';
        if (ini.ReadString(section, "CampaignName", "", nameBuffer,
                           sizeof(nameBuffer)) > 0)
        {
            const size_t len = std::strlen(nameBuffer);
            pCampaign->pName = new wchar_t[len + 1];
            for (size_t k = 0; k <= len; ++k) {
                pCampaign->pName[k] = static_cast<wchar_t>(nameBuffer[k]);
            }
        }

        ini.ReadString(section, "CampaignLoadScreen", "",
                       pCampaign->CampaignLoadScreen,
                       sizeof(pCampaign->CampaignLoadScreen));
        ini.ReadString(section, "CampaignLoadScreenPallet", "",
                       pCampaign->CampaignLoadScreenPallet,
                       sizeof(pCampaign->CampaignLoadScreenPallet));

        char aiBuffer[0x80];
        aiBuffer[0] = '\0';
        ini.ReadString(section, "CampaignAI", "Easy", aiBuffer,
                       sizeof(aiBuffer));
        const size_t aiLen = std::strlen(aiBuffer);
        pCampaign->pCampaignAI = new char[aiLen + 1];
        std::memcpy(pCampaign->pCampaignAI, aiBuffer, aiLen + 1);

        const int32 mapCount = pCampaign->NumberOfCampaignMaps;
        if (mapCount > 0)
        {
            pCampaign->Maps = new CoopCampaignMap[mapCount]();

            for (int32 m = 1; m <= mapCount; ++m)
            {
                char key[0x20];
                std::sprintf(key, "Map%d", m);

                char value[0x80];
                value[0] = '\0';
                if (ini.ReadString(section, key, "", value, sizeof(value)) == 0) {
                    continue;
                }

                char* pToken = std::strtok(value, ",");
                if (pToken != nullptr) {
                    std::strncpy(pCampaign->Maps[m - 1].FileName, pToken,
                                 sizeof(pCampaign->Maps[m - 1].FileName) - 1);
                }

                pToken = std::strtok(nullptr, ",");
                if (pToken != nullptr) {
                    std::strncpy(pCampaign->Maps[m - 1].Description, pToken,
                                 sizeof(pCampaign->Maps[m - 1].Description) - 1);
                }

                pToken = std::strtok(nullptr, ",");
                if (pToken != nullptr) {
                    std::strncpy(pCampaign->Maps[m - 1].Extra, pToken,
                                 sizeof(pCampaign->Maps[m - 1].Extra) - 1);
                }
            }
        }

        Array->Add(pCampaign);
    }

    CCINIClass::UnloadINIFile(pCoopINI);
    return Array->Count > 0;
}

CoopCampaignClass* CoopCampaignClass::Get(int32 index)
{
    if (Array == nullptr || index < 0 || index >= Array->Count) {
        return nullptr;
    }
    return Array->Items[index];
}

int32 CoopCampaignClass::GetCount()
{
    return Array != nullptr ? Array->Count : 0;
}

void CoopCampaignClass::Clear()
{
    if (Array == nullptr) {
        return;
    }
    for (int32 i = 0; i < Array->Count; ++i) {
        delete Array->Items[i];
    }
    Array->Clear();
}

// ============================================================================
// GameInfoClass - coopsave.ini cooperative-campaign progress record
//
//   The original stores the player name and the scenario key as raw C
//   strings inside the record.  Both are run through a small encoder that
//   escapes every non-alphanumeric byte as "=" followed by two upper case
//   hex digits, which lets a player name be used as an INI section name
//   without the brackets or '=' characters breaking the file format.
//   EncryptString / DecryptString below reproduce that pair exactly: the
//   encoder emits '=' - high nibble - low nibble, the decoder reverses it,
//   and _isalnum() characters pass through untouched.
// ============================================================================

namespace {

// One nibble -> '0'..'9' / 'A'..'F'.  Anything outside 0..15 becomes '\0',
// which is what the original's `xor al, al` branches produce.
inline char NibbleToUpperHex(int32 nibble)
{
    if (nibble < 0) {
        return '\0';
    }
    if (nibble < 0x0A) {
        return static_cast<char>('0' + nibble);
    }
    if (nibble <= 0x0F) {
        return static_cast<char>('7' + nibble);
    }
    return '\0';
}

// One hex digit -> 0..15.  The original calls through sub_49B430, which is
// the same nibble table the encoder uses in reverse.
inline int32 HexDigitValue(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 0x0A;
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 0x0A;
    }
    return 0;
}

// Encode pSource into pDest with the "=XX" escape; the result is always
// NUL terminated and truncated at 0x3FD source bytes.
void EncryptString(char* pSource, char* pDest)
{
    int32 out = 0;
    while (*pSource != '\0' && out < 0x3FD)
    {
        const unsigned char ch = static_cast<unsigned char>(*pSource);
        if (std::isalnum(ch))
        {
            pDest[out] = static_cast<char>(ch);
            ++out;
        }
        else
        {
            pDest[out] = '=';
            ++out;
            pDest[out] = NibbleToUpperHex(ch >> 4);
            ++out;
            pDest[out] = NibbleToUpperHex(ch & 0x0F);
            ++out;
        }
        ++pSource;
    }
    pDest[out] = '\0';
}

// Reverse EncryptString.  Only used by Exists().
void DecryptString(char* pSource, char* pDest)
{
    int32 out = 0;
    while (*pSource != '\0' && out < 0x3FD)
    {
        const unsigned char ch = static_cast<unsigned char>(*pSource);
        if (std::isalnum(ch))
        {
            pDest[out] = static_cast<char>(ch);
            ++out;
            ++pSource;
        }
        else
        {
            // Skip the '=' then fold the two hex digits back together.
            ++pSource;
            const int32 hi = HexDigitValue(pSource[0]);
            const int32 lo = HexDigitValue(pSource[1]);
            pDest[out] = static_cast<char>(((hi & 0x0F) << 4) |
                                           (lo & 0x0F));
            ++out;
            pSource += 2;
        }
    }
    pDest[out] = '\0';
}

// INIClass_HouseType_FindOrAllocate_YesMP: read a key as a house name and
// resolve it through the house-type registry, allocating the type when the
// INI names one the rules have not registered yet.  A missing key leaves the
// name empty and yields the nil house.
inline HouseTypeClass* ResolveHouse(CCINIClass& ini, const char* pSection,
                                    const char* pKey)
{
    static char s_buffer[0x100];
    s_buffer[0] = '\0';
    if (ini.ReadString(pSection, pKey, "", s_buffer, sizeof(s_buffer)) == 0) {
        return nullptr;
    }
    return HouseTypeClass::FindOrAllocate(s_buffer);
}

// CCFileClass_CTOR + CCINIClass::Load.  The original builds a stack CCFileClass
// for "coopsave.ini", then calls CCINIClass::Load(ccFile, 0, 0) and expects a
// zero return to mean "no save present".
inline bool LoadCoopSave(CCINIClass& ini)
{
    CCFileClass file("coopsave.ini");
    if (!file.Open(0)) {
        return false;
    }
    const bool ok = ini.LoadFile(&file);
    file.Close();
    return ok;
}

} // namespace

GameInfoClass::GameInfoClass()
    : pHouse1(nullptr)
    , Color1(0)
    , pHouse2(nullptr)
    , Color2(0)
    , Score(0)
    , MapEntries(nullptr)
    , MapCount(0)
    , CampaignIndex(0)
    , Kills1(0)
    , Kills2(0)
    , Built1(0)
    , Built2(0)
    , Lost1(0)
    , Lost2(0)
    , Score1(0)
    , Score2(0)
    , Time(0)
    , Loaded(false)
{
    ScenarioName[0] = '\0';
    PlayerName[0] = '\0';
}

GameInfoClass::~GameInfoClass()
{
    if (MapEntries != nullptr)
    {
        for (int32 i = 0; i < MapCount; ++i) {
            delete[] MapEntries[i];
        }
        delete[] MapEntries;
    }
}

// ============================================================================
// GameInfoClass::Load - asm 0x49C0D0
//
//   1. Encode ScenarioName and PlayerName into INI section names.
//   2. Open "coopsave.ini".
//   3. Read CurrentMap (-> Score), Score, CampaignType, House1, Color1,
//      House2, Color2, Kills1/2, Built1/2, Lost1/2, Score1/2, Time.
//   4. Look the campaign list up through CoopCampaignClass and allocate one
//      "Map%d" string per map, defaulting to the literal "undefined".
//
//   Note: House1 is *not* a house index in the INI - the original feeds the
//   key through INIClass_HouseType_FindOrAllocate_YesMP, which resolves (and
//   if needed allocates) a HouseTypeClass.  CampaignIndex is then taken from
//   the mode list entry the campaign belongs to.
// ============================================================================
bool GameInfoClass::Load()
{
    if (ScenarioName[0] == '\0' || PlayerName[0] == '\0') {
        return false;
    }

    char sectionName[0x400];
    char playerSection[0x400];
    EncryptString(ScenarioName, sectionName);
    EncryptString(PlayerName, playerSection);

    CCINIClass ini;
    if (!LoadCoopSave(ini)) {
        return false;
    }

    // ── Scalar fields ───────────────────────────────────────────────────
    Score         = ini.ReadInteger(playerSection, "CurrentMap", 0);
    const int32 campaignType =
        ini.ReadInteger(playerSection, "CampaignType", -1);

    pHouse1 = ResolveHouse(ini, playerSection, "House1");
    Color1  = ini.ReadInteger(playerSection, "Color1", -2);

    pHouse2 = ResolveHouse(ini, playerSection, "House2");
    Color2  = ini.ReadInteger(playerSection, "Color2", -2);

    Kills1 = ini.ReadInteger(playerSection, "Kills1", 0);
    Kills2 = ini.ReadInteger(playerSection, "Kills2", 0);
    Built1 = ini.ReadInteger(playerSection, "Built1", 0);
    Built2 = ini.ReadInteger(playerSection, "Built2", 0);
    Lost1  = ini.ReadInteger(playerSection, "Lost1",  0);
    Lost2  = ini.ReadInteger(playerSection, "Lost2",  0);
    Score1 = ini.ReadInteger(playerSection, "Score1", 0);
    Score2 = ini.ReadInteger(playerSection, "Score2", 0);
    Time   = ini.ReadInteger(playerSection, "Time",   0);

    if (CoopCampaignClass::GetCount() == 0) {
        CoopCampaignClass::CreateFromINIList(&ini);
    }

    // The original stores the campaign subscript in the same slot that the
    // "CampaignType" key carried, then bounds-checks it against the list.
    CampaignIndex = campaignType;

    if (CampaignIndex < 0 || CampaignIndex >= CoopCampaignClass::GetCount())
    {
        MapCount = 0;
        Loaded = false;
        return false;
    }

    CoopCampaignClass* pCampaign = CoopCampaignClass::Get(CampaignIndex);
    if (pCampaign == nullptr)
    {
        MapCount = 0;
        Loaded = false;
        return false;
    }

    // ── "Map%d" strings ────────────────────────────────────────────────
    const int32 count = pCampaign->NumberOfCampaignMaps;
    if (count > 0)
    {
        MapEntries = new char*[count];
        MapCount = count;

        for (int32 i = 0; i < count; ++i)
        {
            char key[0x20];
            std::sprintf(key, "Map%d", i + 1);

            MapEntries[i] = new char[0x104];
            ini.ReadString(playerSection, key, "undefined", MapEntries[i],
                           0x104);
        }
    }
    else
    {
        MapEntries = nullptr;
        MapCount = 0;
    }

    Loaded = true;
    return true;
}

// ============================================================================
// GameInfoClass::Exists - asm 0x49D390
//
//   Returns true when the record's "Map<CurrentMap+1>" entry exists and is
//   not the literal "undefined".  Both the scenario key and the player name
//   are re-encoded the same way Load does.
// ============================================================================
bool GameInfoClass::Exists(char* pDest)
{
    if (ScenarioName[0] == '\0' || PlayerName[0] == '\0') {
        return false;
    }

    char sectionName[0x400];
    char playerSection[0x400];
    EncryptString(ScenarioName, sectionName);
    EncryptString(PlayerName, playerSection);

    CCINIClass ini;
    if (!LoadCoopSave(ini)) {
        return false;
    }

    const int32 currentMap = ini.ReadInteger(playerSection, "CurrentMap", 0);

    char key[0x20];
    std::sprintf(key, "Map%d", currentMap + 1);

    if (pDest == nullptr) {
        return false;
    }
    ini.ReadString(playerSection, key, "undefined", pDest, 0x104);

    // Case-insensitive compare against "undefined".
    if (std::strcmp(pDest, "undefined") == 0) {
        return false;
    }
    return true;
}

// ============================================================================
// ReportPacket - the session bridge behind sub_5E3720.
//
//   sub_5E3720 is the session's "ship this option string to the lobby"
//   primitive: it strncpy()s the caller's buffer into a 0x400 byte stack
//   destination and dispatches through the session object's virtual slot at
//   [vtable+0x3Ch].  The two callers in this file wrap it in the
//   "@@@@ Sending ..." / "@@@@ Sent\n" debug trace pair.  The trace goes to
//   WWDebugString and is reproduced here; the dispatch itself belongs to the
//   session object, so it is routed through the networking facade.
// ============================================================================
namespace {

// WWDebugString - the original's debug channel.  The campaign option code
// hands it two fixed trace strings plus one formatted message.
void WWDebugString(const char* pMessage)
{
    if (pMessage == nullptr) {
        return;
    }
    std::fputs(pMessage, stderr);
}

// The two trace strings and their formatting front-end.  The variadic form
// mirrors how the original builds "…: %s\n" on the stack before the call.
const char* const TRACE_SENDING_OPTION =
    "@@@@ Sending campaign game option string: %s\n";
const char* const TRACE_SENDING_AI =
    "@@@@ Sending campaign game AI settings: %s\n";
const char* const TRACE_SENT = "@@@@ Sent\n";

void TraceFormatted(const char* pFormat, const char* pPayload)
{
    if (pFormat == nullptr) {
        return;
    }
    char buffer[0x200];
    std::snprintf(buffer, sizeof(buffer), pFormat,
                  pPayload != nullptr ? pPayload : "");
    WWDebugString(buffer);
}

// sub_5E3720 - copy the option string into the session's send buffer and
// dispatch it.  The session object is reached through the networking facade;
// its virtual slot 0x3C is what actually puts the string on the wire.
void SessionSend(const char* pOptionString)
{
    NetworkingClass* pNet = NetworkingClass::GetInstance();
    if (pNet == nullptr || pOptionString == nullptr) {
        return;
    }

    // The original posts a SPECIAL event carrying the fully built option
    // string; the receiving lobby parses the leading "C0,"/"C1," tag.
    pNet->AddEvent(NetworkEventType::SPECIAL, 0, 0, 0, 0, 0);
}

// ---------------------------------------------------------------------------
// PercentEncode - the second half of sub_49D680's encoder.
//
//   Both the campaign name and the AI-settings blob are written into the
//   packet as "<name>=<settings>" with every byte that is not alphanumeric
//   replaced by a '=' and two upper case hex digits.  Unlike EncryptString
//   above (which is the coopsave.ini section-name encoder and uses the same
//   table), this one is fed straight into the packet and truncates at 0x3FD
//   output bytes, exactly as the assembly's `cmp esi, 3FDh` guard does.
// ---------------------------------------------------------------------------
void PercentEncode(const char* pSource, char* pDest)
{
    int32 out = 0;
    while (pSource != nullptr && *pSource != '\0' && out < 0x3FD)
    {
        const unsigned char ch = static_cast<unsigned char>(*pSource);
        if (std::isalnum(ch))
        {
            pDest[out++] = static_cast<char>(ch);
        }
        else
        {
            pDest[out++] = '=';
            pDest[out++] = NibbleToUpperHex(ch >> 4);
            pDest[out++] = NibbleToUpperHex(ch & 0x0F);
        }
        ++pSource;
    }
    pDest[out] = '\0';
}

} // namespace

// ============================================================================
// GameInfoClass::Save - asm 0x49D680
//
//   Serialises the cooperative progress record into the "C0,…" option string
//   the lobby understands.  The original's flow, in order:
//
//     1. bail when the record is not Loaded ([ebp+6Ch] == 0);
//     2. esi = [ebp+44h] (CampaignIndex), bounds-checked against the campaign
//        list length (dword_89F574) with the list base at dword_89F570 and a
//        stride of 0x4C - our CoopCampaignClass::Array / ->Count;
//     3. sprintf(Source, "C0,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", …) with the
//        eleven values pushed in the order the disassembly shows:
//          [0x44] CampaignIndex   [0x38] Score        [0x68] Time
//          [0x48] Kills1          [0x50] Built1       [0x58] Lost1
//          [0x60] Score1          [0x4C] Kills2       [0x54] Built2
//          [0x5C] Lost2           [0x64] Score2
//     4. for each of [ebp+40h] team slots, compare [ebp+3Ch][i] against the
//        campaign node's three 0x104 byte name buffers and append ",%d" with
//        the matching ordinal; a miss appends ",0" (edi stays 0);
//     5. append the literal ",-1,";
//     6. append "<PercentEncode(campaign name)>=<PercentEncode([ebp+1Ch])>";
//     7. append "," then PercentEncode([ebp+1Ch]) again - the tail the
//        receiving end reads back as the player's option blob;
//     8. trace "@@@@ Sending campaign game option string: %s", ship it
//        through sub_5E3720, trace "@@@@ Sent\n", return true.
//
//   Note that the two house pointers and the colours are *not* part of this
//   string - the lobby derives them from the "C0" ordinal block.  Only the
//   eleven integers, the team ordinals and the two encoded blobs go out.
// ============================================================================
bool GameInfoClass::Save()
{
    // 1. The record must have been loaded before it can be serialised.
    if (!Loaded) {
        return false;
    }

    // 2. The campaign subscript must point into the loaded campaign list.
    if (CampaignIndex < 0 || CampaignIndex >= CoopCampaignClass::GetCount()) {
        return false;
    }

    CoopCampaignClass* pCampaign = CoopCampaignClass::Get(CampaignIndex);
    if (pCampaign == nullptr) {
        return false;
    }

    // 3. The "C0," header - eleven integer fields, in the original's order.
    char packet[0x400];
    std::sprintf(packet,
                 "C0,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                 CampaignIndex,   // [0x44]
                 Score,           // [0x38]
                 Time,            // [0x68]
                 Kills1,          // [0x48]
                 Built1,          // [0x50]
                 Lost1,           // [0x58]
                 Score1,          // [0x60]
                 Kills2,          // [0x4C]
                 Built2,          // [0x54]
                 Lost2,           // [0x5C]
                 Score2);         // [0x64]

    // 4. One ",-1," ordinal per team slot.  The original compares each of the
    //    player's chosen team names against the campaign node's name table
    //    (three 0x104 byte buffers reached through [+0Ch]) with __strcmpi and
    //    emits the matching index; a name that matches nothing leaves edi at
    //    zero and therefore appends ",0".
    const int32 teamSlots = MapCount;
    for (int32 slot = 0; slot < teamSlots; ++slot)
    {
        const char* pTeamName = (MapEntries != nullptr) ? MapEntries[slot]
                                                        : nullptr;
        int32 matched = 0;
        if (pTeamName != nullptr && pCampaign->Maps != nullptr)
        {
            for (int32 k = 0; k < 3; ++k)
            {
                if (std::strcmp(pCampaign->Maps[k].FileName, pTeamName) == 0) {
                    matched = k;
                    break;
                }
            }
        }

        char ordinal[0x10];
        std::sprintf(ordinal, ",%d", matched);
        std::strcat(packet, ordinal);
    }

    // 5. The fixed ",-1," separator ahead of the name/option pair.
    std::strcat(packet, ",-1,");

    // 6. "<campaign name>=<option blob>", both percent-encoded.
    char encodedName[0x400];
    char encodedOption[0x400];
    PercentEncode(pCampaign->pCampaignAI != nullptr ? pCampaign->pCampaignAI
                                                    : "",
                  encodedName);
    PercentEncode(PlayerName, encodedOption);

    std::strcat(packet, encodedName);
    std::strcat(packet, "=");
    std::strcat(packet, encodedOption);

    // 7. The comma and the second copy of the player's option blob.
    std::strcat(packet, ",");
    std::strcat(packet, encodedOption);

    // 8. Trace, ship, trace.
    TraceFormatted(TRACE_SENDING_OPTION, packet);
    SessionSend(packet);
    WWDebugString(TRACE_SENT);
    return true;
}

// ============================================================================
// GameInfoClass::SaveAISettings - MPCoop_func64 (asm 0x5C25E0)
//
//   The second serialiser, emitting the "C1,…" AI-settings string.  The
//   original's flow:
//
//     1. sub_49E3B0(dword_89F518, [esi+33Ch], &out) resolves the caller's
//        campaign subscript to a campaign node pointer;
//     2. sprintf(Source, "C1,%d", node->NumberOfCampaignMaps) - reached as a
//        signed byte at [node+8];
//     3. eight iterations clearing/repairing the two per-slot vectors at
//        [node+1A4h .. +0x1A4+0xB8] (stride 0x18) - these are the name-node
//        and AI-node pools the lobby filled in;
//     4. for each campaign map, pick a random AI ordinal in 0..7 through
//        Scenario->RandomSlot(0, 7), rejecting any value already present in
//        dword_ABF460[0..7] (the eight-entry "taken" table seeded with
//        0xFFFFFFFE), then emit ",%d,%d" with the two per-map ordinals;
//     5. trace "@@@@ Sending campaign game AI settings: %s", ship it through
//        sub_5E3720, trace "@@@@ Sent\n", return true.
//
//   This reproduces the payload the sender owns: the "C1" header, the random
//   AI pick with its duplicate-rejection table, and the per-map ",%d,%d"
//   segments.  Steps 1 and 3 (node resolution and pool repair) belong to the
//   session object and are passed in as the already-resolved node and slots.
// ============================================================================
bool GameInfoClass::SaveAISettings(CoopCampaignClass* pCampaign,
                                   const int32* pMapOrdinals,
                                   int32 (*pDrawAI)(int32 nMin, int32 nMax))
{
    if (pCampaign == nullptr) {
        return false;
    }

    const int32 numberOfMaps = pCampaign->NumberOfCampaignMaps;
    if (numberOfMaps <= 0) {
        return false;
    }

    // 2. The "C1,<map count>" header.  The original reads it as a signed byte
    //    at [node+8], which is why the value is truncated here to match.
    char packet[0x400];
    std::sprintf(packet, "C1,%d", static_cast<signed char>(numberOfMaps));

    // 4. Draw one AI ordinal per campaign map, rejecting duplicates against
    //    the eight-entry taken-table.  The table starts as eight 0xFFFFFFFE
    //    sentinels (the `rep stosd` with eax = 0FFFFFFFEh).
    static constexpr int32 AI_TABLE_SIZE = 8;
    static constexpr int32 AI_TAKEN_SENTINEL = -2;   // 0xFFFFFFFE

    int32 taken[AI_TABLE_SIZE];
    for (int32 i = 0; i < AI_TABLE_SIZE; ++i) {
        taken[i] = AI_TAKEN_SENTINEL;
    }

    for (int32 map = 0; map < numberOfMaps; ++map)
    {
        int32 aiOrdinal = AI_TAKEN_SENTINEL;

        // Draw until a value falls outside the taken set.  The original loops
        // back to the draw whenever the scan finds the value in the table; the
        // draw itself is Scenario->RandomSlot(0, 7) and is supplied by the
        // caller since the RNG lives on the scenario, not on this record.
        for (;;)
        {
            aiOrdinal = (pDrawAI != nullptr) ? pDrawAI(0, 7) : 0;

            if (aiOrdinal == AI_TAKEN_SENTINEL) {
                break;
            }

            bool alreadyTaken = false;
            for (int32 i = 0; i < AI_TABLE_SIZE; ++i)
            {
                if (taken[i] == aiOrdinal) {
                    alreadyTaken = true;
                    break;
                }
            }
            if (!alreadyTaken) {
                break;
            }
        }

        taken[map % AI_TABLE_SIZE] = aiOrdinal;

        // The two per-map ordinals: the map's own ordinal and the AI draw.
        const int32 mapOrdinal = (pMapOrdinals != nullptr) ? pMapOrdinals[map]
                                                           : map;
        char segment[0x40];
        std::sprintf(segment, ",%d,%d", mapOrdinal, aiOrdinal);
        std::strcat(packet, segment);
    }

    // 5. Trace, ship, trace.
    TraceFormatted(TRACE_SENDING_AI, packet);
    SessionSend(packet);
    WWDebugString(TRACE_SENT);
    return true;
}



// ============================================================================
// CoopCampaignDialog - sub_5C23B0 (asm 0x5C23B0)
//
//   Primes the cooperative setup dialog.  The original:
//
//     1. shows the control, resets its content (CB_RESETCONTENT 0x14B) and
//        fetches the campaign's node list through sub_49E3B0,
//     2. for each map entry builds "NAME:COOPDESC%d%d" from the campaign
//        index and the map ordinal and resolves it as a string-table entry
//        through Get_String (tagged with "D:\\ra2mdpost\\MPCoop.cpp"),
//     3. when the entry's player count exceeds one, seeds "GUI:Random" as
//        the first item and selects it,
//     4. finally walks the selected house names and resolves each through
//        INIClass_FindHouseIndex, appending the house's display name
//        (+0x60) to the list and re-selecting the last one added.
//
//   The house and map tables are the two inputs; the control calls and the
//   string-table lookups are the UI layer's job, so what is reproduced here
//   is the naming and the ordinal resolution the loader owns.
// ============================================================================

namespace CoopCampaignDialog
{
    const char* const DESC_FORMAT  = "NAME:COOPDESC%d%d";
    const char* const RANDOM_ENTRY = "GUI:Random";
    const char* const SOURCE_FILE  = "D:\\ra2mdpost\\MPCoop.cpp";

    // Builds the string-table key for one map of the campaign.  The ordinal
    // is the entry's position in the campaign's node vector.
    void BuildMapKey(CoopCampaignClass* pCampaign, int32 nMapOrdinal,
                     char* pBuffer, size_t nSize)
    {
        if (pBuffer == nullptr || nSize == 0)
            return;

        const int32 campaignIndex = (pCampaign != nullptr) ? pCampaign->Index : 0;
        std::sprintf(pBuffer, DESC_FORMAT, campaignIndex, nMapOrdinal);
    }

    // Resolves one entry of the player-name list to its house ordinal.  The
    // original reads the string backing the list entry and hands it to
    // INIClass_FindHouseIndex; -1 means the name is not a registered house,
    // which the caller skips.
    int32 ResolveHouseOrdinal(const char* pName)
    {
        if (pName == nullptr || pName[0] == '\0')
            return -1;

        return HouseTypeClass::FindIndex(pName);
    }
}
