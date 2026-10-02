#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class CCINIClass;

// ============================================================================
// CampaignClass - one entry of the [Battles] list in a campaign INI
//
//  Each campaign names a scenario file, an optional final movie and a
//  localised description.  CampaignClass::CreateFromINIList walks
//  [Battles] and creates one instance per key; the description is resolved
//  through the string table unless DebugOnly is set, in which case the raw
//  text is used with a " - for debug/testing" suffix.
// ============================================================================

class CampaignClass
{
public:
    CampaignClass();
    virtual ~CampaignClass();

    bool LoadFromINI(CCINIClass* pINI);

    // Walks [Battles] and instantiates one campaign per key.
    static bool CreateFromINIList(CCINIClass* pINI);
    static CampaignClass* FindByName(const char* pName);
    static CampaignClass* Get(int32 index);
    static int32 GetCount();
    static void Clear();

    // ------------------------------------------------------------------
    // Members
    // ------------------------------------------------------------------
    char        ID[0x24];
    int32       CD;
    char        Scenario[0x200];
    int32       FinalMovie;
    bool        DebugOnly;
    wchar_t     Description[0x80];
    int32       ArrayIndex;

    static DynamicVectorClass<CampaignClass*>* Array;
};
