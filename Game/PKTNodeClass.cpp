#include "PKTNodeClass.h"
#include "../INI/INIClass.h"
#include "../IO/FileSystem.h"
#include "../IO/MixFileClass.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================================
 // PKTNodeClass - PKTNode_CTOR
// ============================================================================

PKTNodeClass::PKTNodeClass()
    : DigestValid(true)
    , HasDescription(false)
    , MinPlayers(2)
    , MaxPlayers(4)
{
    Description[0] = L'\0';
    FileName[0]    = '\0';
    std::strcpy(Digest, "No Digest");
}

PKTNodeClass::~PKTNodeClass()
{
    for (int32 i = 0; i < GameModes.Count; ++i) {
        delete[] GameModes.Items[i];
    }
    GameModes.Clear();
}

void PKTNodeClass::Reset()
{
    Description[0] = L'\0';
    FileName[0]    = '\0';
    std::strcpy(Digest, "No Digest");
    DigestValid    = true;
    HasDescription = false;
    MinPlayers     = 2;
    MaxPlayers     = 4;
    CDs.Clear();

    for (int32 i = 0; i < GameModes.Count; ++i) {
        delete[] GameModes.Items[i];
    }
    GameModes.Clear();
}

// ============================================================================
// Construct - PKTNode_CTOR
//
//   The original appends ".MAP" to the section name, resolves the
//   description either from the "DescriptionText" key or the string table
//   entry named by "Description", reads the player counts and the CD list,
//   then loads the scenario file and pulls its embedded [Digest] section.
// ============================================================================
bool PKTNodeClass::Construct(CCINIClass* pINI, const char* pSection)
{
    if (pINI == nullptr || pSection == nullptr || pSection[0] == '\0') {
        return false;
    }

    // File name is the section plus the ".MAP" suffix.
    std::snprintf(FileName, sizeof(FileName), "%s.MAP", pSection);

    // Description: "DescriptionText" wins when present, otherwise the
    // string table entry addressed by "Description".
    char localDesc[0x2C];
    localDesc[0] = '\0';
    const bool hasTextKey =
        pINI->KeyExists(pSection, "DescriptionText") ||
        pINI->SectionExists(pSection);

    if (hasTextKey) {
        if (pINI->ReadString(pSection, "DescriptionText", "", localDesc,
                             sizeof(localDesc)) > 0) {
            const size_t len = std::strlen(localDesc);
            const size_t n = (len < 0x7F) ? len : 0x7F;
            for (size_t k = 0; k < n; ++k) {
                Description[k] = static_cast<wchar_t>(localDesc[k]);
            }
            Description[n] = L'\0';
            HasDescription = true;
        }
    }

    if (!HasDescription) {
        char entry[0x100];
        entry[0] = '\0';
        pINI->ReadStringTableEntry(pSection, "Description", entry,
                                   sizeof(entry));
        const size_t len = std::strlen(entry);
        const size_t n = (len < 0x7F) ? len : 0x7F;
        for (size_t k = 0; k < n; ++k) {
            Description[k] = static_cast<wchar_t>(entry[k]);
        }
        Description[n] = L'\0';
    }

    MinPlayers = pINI->ReadInteger(pSection, "MinPlayers", MinPlayers);
    MaxPlayers = pINI->ReadInteger(pSection, "MaxPlayers", MaxPlayers);

    // ── "CD" comma separated list ───────────────────────────────────────
    char cdBuffer[0x80];
    cdBuffer[0] = '\0';
    if (pINI->ReadString(pSection, "CD", "", cdBuffer, sizeof(cdBuffer)) > 0) {
        char* pToken = std::strtok(cdBuffer, ",");
        while (pToken != nullptr && pToken[0] != '\0') {
            CDs.Add(std::atoi(pToken));
            pToken = std::strtok(nullptr, ",");
        }
    }

    // ── "GameMode" comma separated list ─────────────────────────────────
    char modeBuffer[0x80];
    modeBuffer[0] = '\0';
    if (pINI->ReadString(pSection, "GameMode", "", modeBuffer,
                         sizeof(modeBuffer)) > 0)
    {
        char* pToken = std::strtok(modeBuffer, ",");
        while (pToken != nullptr) {
            const size_t len = std::strlen(pToken);
            char* pEntry = new char[len + 1];
            std::memcpy(pEntry, pToken, len + 1);
            GameModes.Add(pEntry);
            pToken = std::strtok(nullptr, ",");
        }
    }

    // ── [Digest] block embedded in the scenario file ────────────────────
    CCINIClass* pScenario = CCINIClass::LoadINIFile(FileName);
    if (pScenario != nullptr)
    {
        char digest[0x80];
        digest[0] = '\0';
        if (pScenario->ReadString("Digest", "1", "", digest, sizeof(digest)) > 0) {
            std::strncpy(Digest, digest, sizeof(Digest) - 1);
            Digest[sizeof(Digest) - 1] = '\0';
            DigestValid = true;
        } else {
            std::strcpy(Digest, "No Digest");
            DigestValid = false;
        }
        CCINIClass::UnloadINIFile(pScenario);
    }
    else
    {
        std::strcpy(Digest, "No Digest");
        DigestValid = false;
    }

    return true;
}

// ============================================================================
 // PKTPool - Game_ParsePKTs
// ============================================================================

namespace PKTPool
{
    const wchar_t* const RANGE_FORMAT_SINGLE = L"CD";
    const wchar_t* const RANGE_FORMAT_RANGE  = L"CDDC";
    const wchar_t* const FILL_CHAR           = L" ";
    const char*    const SCENARIO_SOURCE     = "D:\\ra2mdpost\\Session.CPP";
    const char*    const NO_DESCRIPTION      = "MSG:NoDescription";

    // The three file patterns the original walks, in order.  The first is
    // mounted by name, the other two are directory scans.
    static const char* const MISSIONS_PKT = "MISSIONSMD.PKT";
    static const char* const PATTERN_PKT  = "*.PKT";
    static const char* const PATTERN_YRO  = "*.YRO";
    static const char* const PATTERN_YRM  = "*.YRM";
    static const char* const MISSIONS_YRO = "MISSIONS.YRO";
    static const char* const SECTION      = "MultiMaps";
    static const char* const MISSING_PKT  = "Can't see .pkt inside .yro!\n";

    // WWDebugString - the original routes its diagnostics through the
    // debugger console; on this platform the equivalent is stderr, which is
    // where the message text is reproduced verbatim from the binary.
    static void WWDebugString(const char* pMessage)
    {
        if (pMessage == nullptr)
            return;
        std::fputs(pMessage, stderr);
    }

    // One [MultiMaps] section -> one node per key.  Shared by all three
    // sources; the caller only has to have the section loaded in pINI.
    static void HarvestSection(CCINIClass* pINI,
                               DynamicVectorClass<PKTNodeClass*>* pPool)
    {
        if (pINI == nullptr || pPool == nullptr)
            return;

        const int32 count = pINI->GetKeyCount(SECTION);
        if (count <= 0)
            return;

        for (int32 idx = 0; idx < count; ++idx) {
            char sectionName[0x40];
            sectionName[0] = '\0';

            const char* pKey = pINI->GetKeyName(SECTION, idx);
            if (pKey == nullptr)
                continue;

            std::strncpy(sectionName, pKey, sizeof(sectionName) - 1);
            sectionName[sizeof(sectionName) - 1] = '\0';

            char value[0x40];
            value[0] = '\0';
            if (pINI->ReadString(SECTION, sectionName, "", value,
                                 sizeof(value)) <= 0) {
                continue;
            }

            // The node only makes it into the pool when the scenario file
            // behind the section name actually exists.
            PKTNodeClass* pNode = new PKTNodeClass();
            if (pNode == nullptr)
                continue;

            if (!pNode->Construct(pINI, sectionName)) {
                delete pNode;
                continue;
            }

            pPool->Add(pNode);
        }
    }

    // Mounts a directory scan and harvests every matching file.  pSkip names
    // a single file to ignore case-insensitively (the original compares
    // against the alternate name first and falls back to the long name).
    static void HarvestDirectory(const char* pPattern, const char* pSkip,
                                 DynamicVectorClass<PKTNodeClass*>* pPool)
    {
        FileFindClass finder;
        if (!finder.FindFirst(pPattern))
            return;

        do {
            const char* pName = finder.GetFileName();
            if (pName == nullptr || pName[0] == '\0')
                continue;

            if (pSkip != nullptr && _strcmpi(pName, pSkip) == 0)
                continue;

            CCINIClass* pINI = CCINIClass::LoadINIFile(pName);
            if (pINI != nullptr) {
                HarvestSection(pINI, pPool);
                CCINIClass::UnloadINIFile(pINI);
            }
        } while (finder.FindNext());

        finder.Close();
    }

    void Parse(DynamicVectorClass<PKTNodeClass*>* pPool)
    {
        if (pPool == nullptr)
            return;

        // ── 1. MISSIONSMD.PKT - always first, by name ───────────────────
        {
            CCINIClass* pINI = CCINIClass::LoadINIFile(MISSIONS_PKT);
            if (pINI != nullptr) {
                HarvestSection(pINI, pPool);
                CCINIClass::UnloadINIFile(pINI);
            }
        }

        // ── 2. every other *.PKT in the working directory ───────────────
        HarvestDirectory(PATTERN_PKT, MISSIONS_PKT, pPool);

        // ── 3. every *.YRO archive ──────────────────────────────────────
        //
        //   The original forces the current drive for the whole scan
        //   (CD::Set_Volume(0xFFFFFFFE), saving the prior value), walks the
        //   directory, mounts each archive through the two-argument
        //   MixFileClass constructor with the empty encryption key, appends it
        //   to vec_MixFiles, then probes the sibling ".PKT" (archive name
        //   minus its 3-character extension plus "PKT") and restores the
        //   drive selector once the scan is done.
        {
            const int32 previousVolume = CD::Get_Volume();
            CD::Set_Volume(static_cast<int32>(0xFFFFFFFE));

            FileFindClass finder;
            const bool found = finder.FindFirst(PATTERN_YRO);

            if (found) {
                do {
                    const char* pArchive = finder.GetFileName();
                    if (pArchive == nullptr || pArchive[0] == '\0')
                        continue;

                    // The attribute filter the original applies before the
                    // name test (dwFileAttributes & 0x116).
                    if (finder.IsDirectory() || finder.IsHidden()) {
                        continue;
                    }

                    if (_strcmpi(pArchive, MISSIONS_YRO) == 0)
                        continue;
                    if (_strcmpi(pArchive, MISSIONS_PKT) == 0)
                        continue;

                    // Mount the archive and register it in the global list.
                    MixFileClass* pMix = new MixFileClass(pArchive, "");
                    if (pMix != nullptr) {
                        MixFileClass::GetMixArray().Add(pMix);
                    }

                    // The archive name minus its 3-character extension, with
                    // "PKT" appended, is the container the original looks
                    // inside for the [MultiMaps] index.
                    const size_t len = std::strlen(pArchive);
                    if (len <= 3)
                        continue;

                    static char pktName[0x104];
                    std::strncpy(pktName, pArchive, len - 3);
                    pktName[len - 3] = '\0';
                    std::strcat(pktName, "PKT");

                    CCINIClass* pPKT = CCINIClass::LoadINIFile(pktName);
                    if (pPKT != nullptr) {
                        HarvestSection(pPKT, pPool);
                        CCINIClass::UnloadINIFile(pPKT);
                    } else {
                        WWDebugString(MISSING_PKT);
                    }
                } while (finder.FindNext());

                finder.Close();
            }

            // Restore the drive selector the original saved in var_570.
            CD::Set_Volume(previousVolume);
        }

        // ── 4. every *.YRM archive ──────────────────────────────────────
        //
        //   The tail of Game_ParsePKTs repeats the same directory scan for
        //   the ".YRM" extension, but instead of reading a sibling ".PKT" it
        //   reads the mission metadata straight out of the archive index:
        //   [Basic] Name (default "No Name"), [Digest] 1 (default "No
        //   Digest") and [Basic] Official (default false), then hands the
        //   tuple to sub_6994F0 (sub_6994F0 fills the description / player
        //   range) and builds the node through sub_69A980.
        {
            FileFindClass finder;
            if (finder.FindFirst(PATTERN_YRM)) {
                do {
                    const char* pArchive = finder.GetFileName();
                    if (pArchive == nullptr || pArchive[0] == '\0')
                        continue;

                    if (finder.IsDirectory() || finder.IsHidden())
                        continue;

                    CCINIClass* pINI = CCINIClass::LoadINIFile(pArchive);
                    if (pINI == nullptr)
                        continue;

                    char name[0x40];
                    name[0] = '\0';
                    pINI->ReadString("Basic", "Name", "No Name", name,
                                     sizeof(name));

                    char digest[0x20];
                    digest[0] = '\0';
                    pINI->ReadString("Digest", "1", "No Digest", digest,
                                     sizeof(digest));

                    const bool official =
                        pINI->ReadBool("Basic", "Official", false);

                    PKTNodeClass* pNode = new PKTNodeClass();
                    if (pNode != nullptr) {
                        std::strncpy(pNode->Digest, digest,
                                     sizeof(pNode->Digest) - 1);
                        pNode->Digest[sizeof(pNode->Digest) - 1] = '\0';
                        pNode->DigestValid = true;
                        pNode->HasDescription = false;

                        // The archive's own name is the scenario key; the
                        // "[Basic] Name" value is the display caption.
                        std::strncpy(pNode->FileName, pArchive,
                                     sizeof(pNode->FileName) - 1);
                        pNode->FileName[sizeof(pNode->FileName) - 1] = '\0';

                        const size_t nameLen = std::strlen(name);
                        const size_t copyLen =
                            (nameLen < (sizeof(pNode->Description) /
                                        sizeof(pNode->Description[0])) - 1)
                                ? nameLen
                                : (sizeof(pNode->Description) /
                                   sizeof(pNode->Description[0])) - 1;
                        for (size_t k = 0; k < copyLen; ++k) {
                            pNode->Description[k] =
                                static_cast<wchar_t>(name[k]);
                        }
                        pNode->Description[copyLen] = L'\0';

                        // The original seeds the player range as 2..4 for the
                        // ".YRM" scan (ebx was set to 4 above the loop).
                        pNode->MinPlayers = 2;
                        pNode->MaxPlayers = 4;
                        (void)official;

                        pPool->Add(pNode);
                    }

                    CCINIClass::UnloadINIFile(pINI);
                } while (finder.FindNext());

                finder.Close();
            }
        }
    }
}
