#include "CampaignClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"

#include <cstring>
#include <cctype>

// ============================================================================
// CampaignClass - implementation
//
//   Every function below is a 1:1 reproduction of one asm procedure; the
//   mapping is noted on each.  The class is an AbstractTypeClass derivative
//   (asm: CTOR calls AbstractTypeClass_CTOR, vdtor calls AbstractTypeClass_DTOR,
//   GetCRC calls AbstractTypeClass_GetCRC) and is registered in the global
//   `v_Campaign` vector, which is modelled here by CampaignClass::Array.
// ============================================================================

DynamicVectorClass<CampaignClass*>* CampaignClass::Array = nullptr;

// ============================================================================
// CampaignClass_CTOR  (retn 4 -> 1 arg: name)
//
//   AbstractTypeClass_CTOR(name) runs first, then CD and FinalMovie are set to
//   -1, the Scenario buffer is emptied, the Description is set to the empty
//   wide string (asm: wcscpy(this+0x2A0, wNull)) and the object is appended to
//   the campaign list.
// ============================================================================
CampaignClass::CampaignClass(const char* pID) noexcept
    : AbstractTypeClass(pID)
    , CD(-1)
    , FinalMovie(-1)
{
    Scenario[0] = '\0';
    Description[0] = L'\0';

    if (Array != nullptr)
        Array->Add(this);
}

CampaignClass::CampaignClass() noexcept
    : AbstractTypeClass("")
    , CD(-1)
    , FinalMovie(-1)
{
    Scenario[0] = '\0';
    Description[0] = L'\0';

    if (Array != nullptr)
        Array->Add(this);
}

// ============================================================================
// sub_46CC10 - virtual destructor (asm SDDTOR, retn 4 -> deleting form)
//
//   Re-stamps the vtables, calls Detach() on itself (the object is a
//   INoticeSink/Source and unregisters its notices), removes itself from the
//   campaign list (the found slot is closed by shifting the tail down one),
//   then runs AbstractTypeClass_DTOR.
// ============================================================================
CampaignClass::~CampaignClass()
{
    // asm: call Detach - the game-global "unregister this object from every
    // notice list" routine.  OpenYR has no such global visitor yet; the
    // INoticeSink / INoticeSource hooks this class inherits are empty stubs,
    // so the observable effect here is the array removal below.
    if (Array != nullptr)
    {
        const int32 index = FindByName(this->ID);
        if (index != -1 && index < Array->Count)
        {
            // Close the hole by shifting the remaining entries down one.
            const int32 count = Array->Count - 1;
            for (int32 i = index; i < count; ++i)
                (*Array)[i] = (*Array)[i + 1];
            Array->Count = count;
        }
    }
}

// ============================================================================
// sub_46CC90 - FindByName
//
//   Case-insensitive scan of the campaign list, comparing the passed name
//   against each entry's ID (the inherited nameINI slot).  Returns the array
//   index, or -1 when nothing matches.
// ============================================================================
int32 CampaignClass::FindByName(const char* pName)
{
    if (Array == nullptr || pName == nullptr)
        return -1;

    const int32 count = Array->Count;
    for (int32 i = 0; i < count; ++i)
    {
        CampaignClass* pItem = (*Array)[i];
        if (pItem == nullptr)
            continue;
        if (_strcmpi(pItem->ID, pName) == 0)
            return i;
    }
    return -1;
}

// ============================================================================
// CampaignClass_LoadFromINI  (retn 4 -> 1 arg: CCINIClass*)
//
//   AbstractTypeClass::LoadFromINI runs first and gates the rest: it returns
//   false when the section (the campaign's own ID) is absent, in which case
//   nothing else is touched and the whole load reports failure.
//
//     CD          int   "CD"          (default: current value)
//     FinalMovie  int   "FinalMovie"  resolved through the movie table
//     Scenario    str   "Scenario"    upper-cased in place
//     DebugOnly   bool  "DebugOnly"
//     Description when DebugOnly is clear it is the string-table entry,
//                 otherwise the raw "Description" text with the literal
//                 " - for debug/testing" appended.
// ============================================================================
bool CampaignClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // Gate on the inherited implementation, which also checks that the section
    // named after our ID exists.
    if (!AbstractTypeClass::LoadFromINI(pINI))
        return false;

    const char* section = this->ID;

    CD = pINI->ReadInteger(section, "CD", CD);

    FinalMovie = pINI->FindMovieIndex(section, "FinalMovie", FinalMovie);

    char scenario[0x200];
    scenario[0] = '\0';
    pINI->ReadString(section, "Scenario", "", scenario, sizeof(scenario));

    // The original upper-cases the stored name so lookups are case
    // insensitive.
    for (int32 i = 0; i < 0x200; ++i)
        Scenario[i] = scenario[i];
    for (int32 i = 0; Scenario[i] != '\0' && i < 0x200; ++i)
        Scenario[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(Scenario[i])));

    const bool debugOnly = pINI->ReadBool(section, "DebugOnly", false);

    if (!debugOnly)
    {
        char entry[0x100];
        entry[0] = '\0';
        pINI->ReadStringTableEntry(section, "Description", entry, sizeof(entry));

        for (int32 i = 0; i < 0x80; ++i)
            Description[i] = static_cast<wchar_t>(static_cast<unsigned char>(entry[i]));
        Description[0x7F] = L'\0';
    }
    else
    {
        // Raw "Description" text, then " - for debug/testing" appended by the
        // helper sub_735060 (a wide-string concatenation into +0x2A0).
        char raw[0x400];
        raw[0] = '\0';
        pINI->ReadString(section, "Description", "", raw, sizeof(raw));

        char combined[0x400];
        std::strncpy(combined, raw, sizeof(combined) - 1);
        combined[sizeof(combined) - 1] = '\0';
        std::strncat(combined, " - for debug/testing",
                     sizeof(combined) - std::strlen(combined) - 1);

        for (int32 i = 0; i < 0x80; ++i)
            Description[i] = static_cast<wchar_t>(static_cast<unsigned char>(combined[i]));
        Description[0x7F] = L'\0';
    }

    return true;
}

// ============================================================================
// CampaignClass_CreateFromINIList  (retn 0 -> no args beyond this; the INI is
//   in ecx because the original is __fastcall with ecx = CCINIClass*)
//
//   Walks [Battles].  Each key's value is the campaign ID; an existing
//   campaign with that ID (FindByName) is reused, otherwise a new one is
//   allocated.  Both branches then invoke the LoadFromINI virtual.  When the
//   section has no keys the whole walk is skipped.
// ============================================================================
bool CampaignClass::CreateFromINIList(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    if (Array == nullptr)
        Array = new DynamicVectorClass<CampaignClass*>();

    const int32 count = pINI->GetKeyCount("Battles");
    if (count <= 0)
        return false;

    for (int32 i = 0; i < count; ++i)
    {
        const char* pKeyName = pINI->GetKeyName("Battles", i);

        char name[0x20];
        name[0] = '\0';
        if (pKeyName != nullptr)
            pINI->GetString("Battles", pKeyName, name, sizeof(name));

        CampaignClass* pCampaign = nullptr;

        const int32 found = FindByName(name);
        if (found != -1)
        {
            pCampaign = (*Array)[found];
        }
        else
        {
            pCampaign = new CampaignClass(name);
            pCampaign->ArrayIndex = Array->Count - 1;
        }

        // asm: call dword ptr [vtable+0x64] -> the LoadFromINI virtual.
        if (pCampaign != nullptr)
            pCampaign->LoadFromINI(pINI);
    }

    return true;
}

// ============================================================================
// CampaignClass_GetClassID  (retn 8 -> 1 arg: CLSID*)
//
//   Null out-pointer -> 0x80004003 (E_POINTER); otherwise the CampaignClass
//   CLSID is stamped (AbstractType::Campaign) and 0 is returned.
// ============================================================================
HRESULT CampaignClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;

    pClassID->Data1 = static_cast<uint32>(AbstractType::Campaign);
    return S_OK;
}

// ============================================================================
// CampaignClass_GetCRC  (retn 4 -> 1 arg: CRCEngine*)
//
//   AbstractTypeClass_GetCRC first, then CD, the Scenario string and
//   FinalMovie are folded in, in that order.
// ============================================================================
void CampaignClass::ComputeCRC(CRCEngine& crc) const
{
    AbstractTypeClass::ComputeCRC(crc);

    crc.AddData(&CD, sizeof(CD));
    crc.AddData(Scenario, static_cast<int32>(sizeof(Scenario)));
    crc.AddData(&FinalMovie, sizeof(FinalMovie));
}

int32 CampaignClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// CampaignClass_Load  (retn 8 -> 2 args: IStream* pStm)
//
//   AbstractTypeClass::Load (the asm's AbstractClass_Load resolves to the
//   AbstractTypeClass override) runs first; a negative HRESULT is passed
//   straight back.  On success the vtables are re-stamped.
// ============================================================================
HRESULT CampaignClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    const HRESULT hr = AbstractTypeClass::Load(pStm);
    if (hr < 0)
        return hr;

    return S_OK;
}

// ============================================================================
// CampaignClass_Save  (retn 0Ch -> 3 args: IStream* pStm, BOOL fClearDirty)
//
//   AbstractTypeClass::Save, with the HRESULT propagated.
// ============================================================================
HRESULT CampaignClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    const HRESULT hr = AbstractTypeClass::Save(pStm, fClearDirty);
    if (hr < 0)
        return hr;

    return S_OK;
}

// ============================================================================
// CampaignClass_GetSize  (mov eax,3A0h ; retn)
//
//   The struct is 0x3A0 bytes.
// ============================================================================
int32 CampaignClass::Size() const
{
    return 0x3A0;
}

// ============================================================================
// CampaignClass_GetAbstractDerivationID  (mov eax,0Ah ; retn)
// ============================================================================
AbstractType CampaignClass::GetAbstractDerivationID() const
{
    return AbstractType::Campaign;
}

// ============================================================================
// Static array accessors (thin helpers over the v_Campaign vector).
// ============================================================================
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
