#include "WOnlineClass.h"
#include "../INI/INIClass.h"

// ============================================================================
// WOnlineClass - [WOnline] section of RA2MD.INI (asm sub_77DED0 / sub_77DFD0)
// ============================================================================

static WOnlineClass* g_WOnlineInstance = nullptr;

WOnlineClass::WOnlineClass()
    : AllowPage(1)
    , AllowFind(1)
    , LangFilter(1)
    , LobMusic(1)
    , ShowAll(1)
    , DisplayAsian(1)
    , DisplayLatin(1)
    , DisplayBuddies(true)
    , DisplayClan(true)
{
}

WOnlineClass::~WOnlineClass()
{
}

WOnlineClass* WOnlineClass::GetInstance()
{
    if (g_WOnlineInstance == nullptr) {
        g_WOnlineInstance = new WOnlineClass();
    }
    return g_WOnlineInstance;
}

void WOnlineClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }

    static const char* const SECTION = "WOnline";

    AllowPage      = pINI->ReadInteger(SECTION, "AllowPage",      AllowPage);
    AllowFind      = pINI->ReadInteger(SECTION, "AllowFind",      AllowFind);
    LangFilter     = pINI->ReadInteger(SECTION, "LangFilter",     LangFilter);
    LobMusic       = pINI->ReadInteger(SECTION, "LobMusic",       LobMusic);
    ShowAll        = pINI->ReadInteger(SECTION, "ShowAll",        ShowAll);
    DisplayAsian   = pINI->ReadInteger(SECTION, "DisplayAsian",   DisplayAsian);
    DisplayLatin   = pINI->ReadInteger(SECTION, "DisplayLatin",   DisplayLatin);
    DisplayBuddies = pINI->ReadBool   (SECTION, "DisplayBuddies", DisplayBuddies);
    DisplayClan    = pINI->ReadBool   (SECTION, "DisplayClan",    DisplayClan);
}

void WOnlineClass::SaveToINI(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }

    static const char* const SECTION = "WOnline";

    pINI->WriteInteger(SECTION, "AllowPage",      AllowPage);
    pINI->WriteInteger(SECTION, "AllowFind",      AllowFind);
    pINI->WriteInteger(SECTION, "LangFilter",     LangFilter);
    pINI->WriteInteger(SECTION, "LobMusic",       LobMusic);
    pINI->WriteInteger(SECTION, "ShowAll",        ShowAll);
    pINI->WriteInteger(SECTION, "DisplayAsian",   DisplayAsian);
    pINI->WriteInteger(SECTION, "DisplayLatin",   DisplayLatin);
    pINI->WriteBool   (SECTION, "DisplayBuddies", DisplayBuddies);
    pINI->WriteBool   (SECTION, "DisplayClan",    DisplayClan);
}
