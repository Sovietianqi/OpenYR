#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class CCINIClass;

// ============================================================================
// WOnlineClass
//
//   Persistent Westwood Online interface preferences, read from the
//   [WOnline] section of RA2MD.INI by sub_77DED0 and written back by
//   sub_77DFD0.  Every flag defaults to 1.
// ============================================================================

class WOnlineClass
{
public:
    WOnlineClass();
    ~WOnlineClass();

    static WOnlineClass* GetInstance();

    void LoadFromINI(CCINIClass* pINI);
    void SaveToINI(CCINIClass* pINI);

    int32   AllowPage;
    int32   AllowFind;
    int32   LangFilter;
    int32   LobMusic;
    int32   ShowAll;
    int32   DisplayAsian;
    int32   DisplayLatin;
    bool    DisplayBuddies;
    bool    DisplayClan;
};
