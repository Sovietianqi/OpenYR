#include "TiberiumClass.h"

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Abstract/OverlayTypeClass.h>
#include <Animations/AnimTypeClass.h>

#include <cstring>
#include <cstdlib>

// ============================================================================
// TiberiumClass.cpp
//
//  Implements the ore type of the [Tiberiums] list.  The constructor appends
//  the instance to the global registry and stamps its ordinal into TypeIndex;
//  the registry is what Is_Overlay_Idx_Tiberium walks when the map asks which
//  ore an overlay index belongs to.
//
//  LoadFromINI reads Spread / SpreadPercentage / Growth / GrowthPercentage /
//  Value / Power / Color / Debris / Image, with the last one driving a small
//  switch that selects the overlay image window the ore draws from.
// ============================================================================

// ============================================================================
// Static array management
//
// TiberiumClass::Array itself is defined alongside the other type-class arrays
// in Game/Globals.cpp, matching the project's convention.
// ============================================================================

void TiberiumClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<TiberiumClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<TiberiumClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<TiberiumClass*>();
    }
}

void TiberiumClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<TiberiumClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void TiberiumClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = 0; i < Array->Count; ++i)
        delete (*Array)[i];

    Array->Clear();
}

int32 TiberiumClass::GetCount()
{
    return Array ? Array->Count : 0;
}

TiberiumClass* TiberiumClass::FindByIndex(int32 index)
{
    if (Array == nullptr || index < 0 || index >= Array->Count)
        return nullptr;

    return (*Array)[index];
}

TiberiumClass* TiberiumClass::Find(const char* pID)
{
    if (pID == nullptr || Array == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i) {
        TiberiumClass* pType = (*Array)[i];
        if (pType != nullptr && _strcmpi(pType->ID, pID) == 0)
            return pType;
    }

    return nullptr;
}

TiberiumClass* TiberiumClass::FindOrAllocate(const char* pID)
{
    if (pID == nullptr)
        return nullptr;

    if (TiberiumClass* pFound = Find(pID))
        return pFound;

    return new TiberiumClass(pID);
}

// ============================================================================
// Constructor / destructor
// ============================================================================

TiberiumClass::TiberiumClass(const char* pID) noexcept
    : AbstractTypeClass(pID)
    , TypeIndex(-1)
    , Spread(0)
    , SpreadPercentage(0.1)     // the binary seeds 0x3FB99999 = 0.1
    , Growth(0)
    , GrowthPercentage(0.1)
    , Value(0)
    , Power(0)
    , Color(0)
    , Debris()
    , Image(nullptr)
    , ImageStart(0)
    , NumImages(0)
    , ImageCount(0)
    , field_F0(0)
    , field_F4(nullptr)
    , field_F8(nullptr)
    , field_FC(nullptr)
    , field_100(0)
    , field_104(0)
    , field_108(0)
    , field_10C(0)
    , field_110(nullptr)
    , field_114(nullptr)
    , field_118(nullptr)
    , field_11C(0)
    , field_120(0)
    , field_124(0)
{
    if (Array == nullptr)
        Init_Array();

    TypeIndex = Array->Count;
    Array->Add(this);
}

TiberiumClass::~TiberiumClass()
{
    // Drop ourselves from the global registry so stored ordinals never point at
    // freed memory, then release whatever the map-generation pass attached.
    if (Array != nullptr) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }

    if (field_F4  != nullptr) YRMemory::Deallocate(field_F4);
    if (field_F8  != nullptr) YRMemory::Deallocate(field_F8);
    if (field_FC  != nullptr) YRMemory::Deallocate(field_FC);
    if (field_110 != nullptr) YRMemory::Deallocate(field_110);
    if (field_114 != nullptr) YRMemory::Deallocate(field_114);
    if (field_118 != nullptr) YRMemory::Deallocate(field_118);
}

// ============================================================================
// COM plumbing
// ============================================================================

HRESULT TiberiumClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;

    // CLSID_TiberiumClass, copied straight out of .data by the binary.
    pClassID->Data1 = static_cast<uint32>(AbstractType::Tiberium);
    return S_OK;
}

HRESULT TiberiumClass::Load(IStream* pStm)
{
    (void)pStm;
    return S_OK;
}

HRESULT TiberiumClass::Save(IStream* pStm, BOOL fClearDirty)
{
    (void)pStm;
    (void)fClearDirty;
    return S_OK;
}

AbstractType TiberiumClass::WhatAmI() const
{
    return AbstractType::Tiberium;
}

int32 TiberiumClass::Size() const
{
    // mov eax, 128h
    return 0x128;
}

int32 TiberiumClass::GetArrayIndex() const
{
    return TypeIndex;
}

// ============================================================================
// ComputeCRC
//
// The binary folds the parent CRC first, then each scalar in order: Spread,
// Growth, Value, Power, Color, ImageStart, NumImages.
// ============================================================================

void TiberiumClass::ComputeCRC(CRCEngine& crc) const
{
    AbstractTypeClass::ComputeCRC(crc);

    crc.AddData(&Spread, sizeof(Spread));
    crc.AddData(&Growth, sizeof(Growth));
    crc.AddData(&Value, sizeof(Value));
    crc.AddData(&Power, sizeof(Power));
    crc.AddData(&Color, sizeof(Color));
    crc.AddData(&ImageStart, sizeof(ImageStart));
    crc.AddData(&NumImages, sizeof(NumImages));
}

// ============================================================================
// PointerExpired
//
// The binary walks the debris vector backwards and removes every entry that
// matches the expiring pointer, shifting the tail down over the hole.
// ============================================================================

void TiberiumClass::PointerExpired(AbstractClass* pAbstract, bool removed)
{
    (void)removed;

    if (pAbstract == nullptr)
        return;

    for (int32 i = Debris.Count - 1; i >= 0; --i) {
        if (Debris[i] == pAbstract)
            Debris.Remove(i);
    }
}

// ============================================================================
// LoadFromINI
// ============================================================================

bool TiberiumClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const char* pSection = ID;

    Spread           = pINI->ReadInteger(pSection, "Spread", Spread);
    SpreadPercentage = pINI->ReadDouble(pSection, "SpreadPercentage", SpreadPercentage);
    Growth           = pINI->ReadInteger(pSection, "Growth", Growth);
    GrowthPercentage = pINI->ReadDouble(pSection, "GrowthPercentage", GrowthPercentage);
    Value            = pINI->ReadInteger(pSection, "Value", Value);
    Power            = pINI->ReadInteger(pSection, "Power", Power);
    Color            = pINI->ReadColorSchemeIndex(pSection, "Color", Color);

    char debrisText[0x80];
    debrisText[0] = '\0';
    if (pINI->ReadString(pSection, "Debris", "", debrisText, sizeof(debrisText)) > 0) {
        // A comma separated run of animation IDs; each is resolved against the
        // animation type table and appended in the order it appears.
        char* pToken = std::strtok(debrisText, ",");
        while (pToken != nullptr) {
            if (pToken[0] != '\0') {
                AnimTypeClass* pAnim = AnimTypeClass::FindOrAllocate(pToken);
                if (pAnim != nullptr)
                    Debris.Add(pAnim);
            }
            pToken = std::strtok(nullptr, ",");
        }
    }

    const int32 image = pINI->ReadInteger(pSection, "Image", -1);

    if (image != -1) {
        switch (image) {
            case 3:
                ImageStart = 0x1B;
                NumImages  = 12;
                ImageCount = 12;
                break;

            case 4:
                ImageStart = 0x7F;
                NumImages  = 12;
                ImageCount = 12;
                break;

            case 5:
                ImageStart = 0x93;
                NumImages  = 12;
                ImageCount = 12;
                break;

            default:
                ImageStart = 0x66;
                NumImages  = 12;
                ImageCount = 8;
                break;
        }
    }

    // Resolve the overlay art the ore renders as.
    Image = OverlayTypeClass::FindByIndex(ImageStart);

    return true;
}

// ============================================================================
// CreateFromINIList
// ============================================================================

bool TiberiumClass::CreateFromINIList(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return true;

    if (Array == nullptr)
        Init_Array();

    if (pINI->GetSection("Tiberiums") == nullptr)
        return true;

    const int32 count = pINI->GetKeyCount("Tiberiums");

    for (int32 i = 0; i < count; ++i) {
        const char* pKeyName = pINI->GetKeyName("Tiberiums", i);
        if (pKeyName == nullptr)
            continue;

        char name[0x18];
        name[0] = '\0';
        if (pINI->ReadString("Tiberiums", pKeyName, "", name, sizeof(name)) <= 0)
            continue;

        const int32 ordinal = std::atoi(pKeyName);

        TiberiumClass* pType = nullptr;
        if (ordinal < Array->Count)
            pType = (*Array)[ordinal];

        if (pType == nullptr)
            pType = new TiberiumClass(name);

        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }

    return true;
}

// ============================================================================
// Is_Overlay_Idx_Tiberium
// ============================================================================

int32 Is_Overlay_Idx_Tiberium(int32 overlayIndex)
{
    if (overlayIndex == -1)
        return -1;

    OverlayTypeClass* pOverlay = OverlayTypeClass::FindByIndex(overlayIndex);
    if (pOverlay == nullptr)
        return -1;

    if (!pOverlay->Is_Tiberium())
        return -1;

    if (TiberiumClass::Array == nullptr)
        return -1;

    const int32 total = TiberiumClass::Array->Count;

    for (int32 i = 0; i < total; ++i) {
        TiberiumClass* pType = (*TiberiumClass::Array)[i];
        if (pType == nullptr)
            continue;

        const int32 start = pType->ImageStart;

        if (overlayIndex >= start && overlayIndex < start + pType->NumImages)
            return pType->TypeIndex;

        if (overlayIndex >= start + pType->NumImages &&
            overlayIndex < start + pType->NumImages + pType->ImageCount)
            return pType->TypeIndex;
    }

    return -1;
}
