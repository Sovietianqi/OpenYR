#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class CCINIClass;

// ============================================================================
// WOLProfileClass
//
//   One Westwood Online player profile stored in wolinfoMD.ini, keyed by the
//   player's login name.  WOLInfoMD_read (asm 0x7789DD) loads the section for
//   a given name.
// ============================================================================

class WOLProfileClass
{
public:
    WOLProfileClass();

    // WOLInfoMD_read: reads the profile for pName out of wolinfoMD.ini.
    bool LoadFromINI(const char* pName);
    void SaveToINI(CCINIClass* pINI, const char* pName);

    void Reset();

    char        Server[0x40];       // "Server"
    int32       Locale;             // "Locale"    (default 0)
    bool        LocaleVerified;     // "LocaleVerified"
    int32       Country;            // "Country"   (default -2)
    int32       Color;              // "Color"     (default -2)
    int32       Rank;               // "Rank"      (default -1)
    int32       Points;             // "Points"    (default -1)
    int32       Wins;               // "Wins"      (default -1)
    int32       Losses;             // "Losses"    (default -1)
    int32       Disconnects;        // "Disconnects" (default -1)
    bool        MatchByRes;         // "MatchByRes" (default true)
    char        UpdateTime[0x80];   // "UpdateTime"
    char        UpdateDate[0x80];   // "UpdateDate"

    // The original stores a derived 3 when the locale is not verified, else a
    // value of 2 subtracted from the boolean.
    int32       Status;
};

// ============================================================================
// WOL persona helpers - the [AutoLogin] DefaultPersona pair
//
//   sub_779B50 (asm 0x779B50) reads wolinfoMD.ini's [AutoLogin]
//   "DefaultPersona" value into a caller owned string.  When the file does
//   not exist the destination is reset to empty.
//
//   sub_779C90 (asm 0x779C90) is the matching writer: it creates the file if
//   needed and stores the persona back under the same section and key.
// ============================================================================

namespace WOLPersona {

// Reads [AutoLogin] DefaultPersona.  Returns true when the file existed.
bool LoadDefaultPersona(char* pDest, size_t destSize);

// Writes [AutoLogin] DefaultPersona, creating wolinfoMD.ini when absent.
bool SaveDefaultPersona(const char* pPersona);

extern const char* const FILE_NAME;      // "wolinfoMD.ini"
extern const char* const SECTION_AUTOLOGIN;   // "AutoLogin"
extern const char* const KEY_PERSONA;    // "DefaultPersona"

} // namespace WOLPersona
