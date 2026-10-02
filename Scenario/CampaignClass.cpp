#include "CampaignClass.h"
#include "../INI/INIClass.h"
#include "../IO/FileSystem.h"

#include <cstring>
#include <cctype>

// ============================================================================
// CampaignClass - implementation
// ============================================================================

DynamicVectorClass<CampaignClass*>* CampaignClass::Array = nullptr;

CampaignClass::CampaignClass()
    : CD(0)
    , FinalMovie(-1)
    , DebugOnly(false)
    , ArrayIndex(-1)
{
    ID[0] = '\0';
    Scenario[0] = '\0';
    Description[0] = L'\0';
}

CampaignClass::~CampaignClass()
{
}

// ============================================================================
// LoadFromINI - CampaignClass_LoadFromINI
//
//   The section name is the campaign's own ID (set up by CreateFromINIList).
//   AbstractTypeClass::LoadFromINI runs first and gates the rest: when the
//   section is absent the load reports failure and nothing is touched.
//
//     CD          int    the required CD number
//     FinalMovie  movie index resolved through the movie table
//     Scenario    scenario file name, upper-cased
//     DebugOnly   bool   when set, Description is taken verbatim
//     Description either the string-table entry or the raw text plus
//                 " - for debug/testing"
// ============================================================================
bool CampaignClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const char* section = ID;
    if (section[0] == '\0')
        return false;

    if (!pINI->SectionExists(section))
        return false;

    CD = pINI->ReadInteger(section, "CD", CD);

    FinalMovie = pINI->FindMovieIndex(section, "FinalMovie", FinalMovie);

    char scenario[0x200];
    scenario[0] = '\0';
    pINI->ReadString(section, "Scenario", "", scenario, sizeof(scenario));

    // The original upper-cases the stored name so lookups are case
    // insensitive.
    for (int32 i = 0; scenario[i] != '\0' && i < 0x1FF; ++i)
        Scenario[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(scenario[i])));
    Scenario[0x1FF] = '\0';

    DebugOnly = pINI->ReadBool(section, "DebugOnly", false);

    if (!DebugOnly)
    {
        char entry[0x100];
        entry[0] = '\0';
        pINI->ReadStringTableEntry(section, "Description", entry, sizeof(entry));

        for (int32 i = 0; i < 0x80; ++i)
            Description[i] = static_cast<wchar_t>(entry[i]);
        Description[0x7F] = L'\0';
    }
    else
    {
        char raw[0x400];
        raw[0] = '\0';
        pINI->ReadString(section, "Description", "", raw, sizeof(raw));

        char combined[0x400];
        std::strncpy(combined, raw, sizeof(combined) - 1);
        combined[sizeof(combined) - 1] = '\0';
        std::strncat(combined, " - for debug/testing",
                     sizeof(combined) - std::strlen(combined) - 1);

        for (int32 i = 0; i < 0x80; ++i)
            Description[i] = static_cast<wchar_t>(combined[i]);
        Description[0x7F] = L'\0';
    }

    return true;
}

// ============================================================================
// CreateFromINIList - CampaignClass_CreateFromINIList
//
//   Walks [Battles] of the campaign INI.  Each key names a campaign; a
//   campaign with that ID already in the array is re-used, otherwise a new
//   one is allocated and its ID set from the key name before LoadFromINI
//   fills in the rest.
// ============================================================================
bool CampaignClass::CreateFromINIList(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    if (Array == nullptr)
        Array = new DynamicVectorClass<CampaignClass*>();

    const int32 count = pINI->GetKeyCount("Battles");
    for (int32 i = 0; i < count; ++i)
    {
        const char* pKeyName = pINI->GetKeyName("Battles", i);
        if (pKeyName == nullptr)
            continue;

        char name[0x20];
        name[0] = '\0';
        pINI->GetString("Battles", pKeyName, name, sizeof(name));
        if (name[0] == '\0')
            continue;

        CampaignClass* pCampaign = FindByName(name);
        if (pCampaign == nullptr)
        {
            pCampaign = new CampaignClass();
            std::strncpy(pCampaign->ID, name, sizeof(pCampaign->ID) - 1);
            pCampaign->ID[sizeof(pCampaign->ID) - 1] = '\0';
            pCampaign->ArrayIndex = Array->Count;
            Array->Add(pCampaign);
        }

        pCampaign->LoadFromINI(pINI);
    }

    return true;
}

CampaignClass* CampaignClass::FindByName(const char* pName)
{
    if (Array == nullptr || pName == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        CampaignClass* pItem = (*Array)[i];
        if (pItem == nullptr)
            continue;
        if (_strcmpi(pItem->ID, pName) == 0)
            return pItem;
    }
    return nullptr;
}

CampaignClass* CampaignClass::Get(int32 index)
{
    if (Array == nullptr || index < 0 || index >= Array->Count)
        return nullptr;
    return (*Array)[index];
}

int32 CampaignClass::GetCount()
{
    return Array != nullptr ? Array->Count : 0;
}

void CampaignClass::Clear()
{
    if (Array == nullptr)
        return;

    for (int32 i = 0; i < Array->Count; ++i)
        delete (*Array)[i];

    Array->Clear();
}
