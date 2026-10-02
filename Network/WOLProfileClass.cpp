#include "WOLProfileClass.h"
#include "../INI/INIClass.h"
#include "../IO/CCFileClass.h"

#include <cstring>

// ============================================================================
// WOLProfileClass - wolinfoMD.ini
// ============================================================================

WOLProfileClass::WOLProfileClass()
{
    Reset();
}

void WOLProfileClass::Reset()
{
    Server[0]       = '\0';
    Locale          = 0;
    LocaleVerified  = false;
    Country         = -2;
    Color           = -2;
    Rank            = -1;
    Points          = -1;
    Wins            = -1;
    Losses          = -1;
    Disconnects     = -1;
    MatchByRes      = true;
    UpdateTime[0]   = '\0';
    UpdateDate[0]   = '\0';
    Status          = 3;
}

// ============================================================================
// LoadFromINI - WOLInfoMD_read (asm 0x7789DD)
//
//   Opens "wolinfoMD.ini" and reads the section named after the player.  The
//   default values are exactly the ones in the original: Server "", Locale 0,
//   Country/Color -2, Rank/Points/Wins/Losses/Disconnects -1, MatchByRes 1.
// ============================================================================
bool WOLProfileClass::LoadFromINI(const char* pName)
{
    if (pName == nullptr || pName[0] == '\0') {
        Reset();
        return false;
    }

    CCINIClass* pINI = CCINIClass::LoadINIFile("wolinfoMD.ini");
    if (pINI == nullptr) {
        Reset();
        return false;
    }

    CCINIClass& ini = *pINI;
    const char* section = pName;

    if (!ini.SectionExists(section)) {
        CCINIClass::UnloadINIFile(pINI);
        Reset();
        return false;
    }

    Server[0] = '\0';
    ini.ReadString(section, "Server", "", Server, sizeof(Server));

    Locale         = ini.ReadInteger(section, "Locale", 0);
    LocaleVerified = ini.ReadBool   (section, "LocaleVerified", false);
    Country        = ini.ReadInteger(section, "Country", -2);
    Color          = ini.ReadInteger(section, "Color",   -2);
    Rank           = ini.ReadInteger(section, "Rank",    -1);
    Points         = ini.ReadInteger(section, "Points",  -1);
    Wins           = ini.ReadInteger(section, "Wins",    -1);
    Losses         = ini.ReadInteger(section, "Losses",  -1);
    Disconnects    = ini.ReadInteger(section, "Disconnects", -1);
    MatchByRes     = ini.ReadBool   (section, "MatchByRes", true);

    UpdateTime[0] = '\0';
    ini.ReadString(section, "UpdateTime", "", UpdateTime, sizeof(UpdateTime));

    UpdateDate[0] = '\0';
    ini.ReadString(section, "UpdateDate", "", UpdateDate, sizeof(UpdateDate));

    // The original packs the verification flag into a small status code:
    // unverified locales get 3, verified ones 2 - 1 = 1.
    Status = LocaleVerified ? 1 : 3;

    CCINIClass::UnloadINIFile(pINI);
    return true;
}

void WOLProfileClass::SaveToINI(CCINIClass* pINI, const char* pName)
{
    if (pINI == nullptr || pName == nullptr || pName[0] == '\0') {
        return;
    }

    const char* section = pName;

    pINI->WriteString (section, "Server", Server);
    pINI->WriteInteger(section, "Locale", Locale);
    pINI->WriteBool   (section, "LocaleVerified", LocaleVerified);
    pINI->WriteInteger(section, "Country", Country);
    pINI->WriteInteger(section, "Color", Color);
    pINI->WriteInteger(section, "Rank", Rank);
    pINI->WriteInteger(section, "Points", Points);
    pINI->WriteInteger(section, "Wins", Wins);
    pINI->WriteInteger(section, "Losses", Losses);
    pINI->WriteInteger(section, "Disconnects", Disconnects);
    pINI->WriteBool   (section, "MatchByRes", MatchByRes);
    pINI->WriteString (section, "UpdateTime", UpdateTime);
    pINI->WriteString (section, "UpdateDate", UpdateDate);
}

// ============================================================================
// WOLPersona - wolinfoMD.ini [AutoLogin]
//
//   The persona string remembers which WOL nickname the player last logged in
//   with.  The reader reports whether the file existed so the caller can tell
//   "no file" apart from "empty value".
// ============================================================================
namespace WOLPersona {

const char* const FILE_NAME        = "wolinfoMD.ini";
const char* const SECTION_AUTOLOGIN = "AutoLogin";
const char* const KEY_PERSONA      = "DefaultPersona";

bool LoadDefaultPersona(char* pDest, size_t destSize)
{
    if (pDest == nullptr || destSize == 0) {
        return false;
    }

    pDest[0] = '\0';

    CCFileClass file(FILE_NAME);
    if (!file.IsAvailable()) {
        return false;
    }

    CCINIClass ini;
    if (!ini.LoadFile(&file)) {
        return false;
    }

    ini.ReadString(SECTION_AUTOLOGIN, KEY_PERSONA, pDest, pDest, destSize);
    return true;
}

bool SaveDefaultPersona(const char* pPersona)
{
    CCFileClass file(FILE_NAME);
    if (!file.Exists()) {
        file.Open(1);
        file.Close();
    }

    CCINIClass ini;
    if (!ini.LoadFile(&file)) {
        return false;
    }

    ini.WriteString(SECTION_AUTOLOGIN, KEY_PERSONA,
                    pPersona != nullptr ? pPersona : "");
    return ini.SaveFile(&file);
}

} // namespace WOLPersona
