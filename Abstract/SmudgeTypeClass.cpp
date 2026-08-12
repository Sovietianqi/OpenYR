#include <Abstract/SmudgeTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>

#include <cstring>
#include <cstdio>

// ============================================================================
// SmudgeTypeClass.cpp
//
//  SmudgeTypeClass describes a kind of smudge (scorch mark, crater, bib, etc.)
//  loaded from rules/art INI files.  It inherits ObjectTypeClass and adds:
//
//    * Cell dimensions (width/height in cells)
//    * Frame count and animation flag
//    * Crater / scorch / bib classification
//    * Flat rendering flag (drawn beneath units)
//    * Crater chain parameters (ChainCount / ChainSteps)
//    * Art name (SHP file reference)
//
//  This file implements:
//    * Static Array plumbing (Init_Array / Delete_Array / Delete_All /
//      Find / FindByIndex / GetCount)
//    * Constructor / destructor
//    * LoadFromINI / SaveToINI
//    * ComputeCRC / GetCRC
//    * Get_Size / Is_Crater / Is_Scorch
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<SmudgeTypeClass*>* SmudgeTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void SmudgeTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<SmudgeTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<SmudgeTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<SmudgeTypeClass*>();
    }
}

void SmudgeTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<SmudgeTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void SmudgeTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        SmudgeTypeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

SmudgeTypeClass* SmudgeTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        SmudgeTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

SmudgeTypeClass* SmudgeTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 SmudgeTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

// ============================================================================
// Constructor
// ============================================================================

SmudgeTypeClass::SmudgeTypeClass(const char* pID) noexcept
    : ObjectTypeClass(pID)
{
    CellSize    = CellStruct(1, 1);
    Frames      = 0;
    IsCrater    = false;
    IsScorch    = false;
    IsBib       = false;
    IsAnimated  = false;
    IsFlat      = true;
    ChainCount  = 0;
    ChainSteps  = 0;

    std::memset(ArtName, 0, sizeof(ArtName));
}

// ============================================================================
// Destructor
// ============================================================================

SmudgeTypeClass::~SmudgeTypeClass()
{
    // No heap resources to release.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT SmudgeTypeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::SmudgeType);
    return S_OK;
}

HRESULT SmudgeTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Load(pStm);
    if (hr < 0) return hr;

    ULONG read = 0;

    hr = pStm->Read(&CellSize, sizeof(CellSize), &read);
    if (hr < 0 || read != sizeof(CellSize)) return E_FAIL;

    hr = pStm->Read(&Frames, sizeof(Frames), &read);
    if (hr < 0 || read != sizeof(Frames)) return E_FAIL;

    // Read bool flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsCrater   = (flags & 0x0001) != 0;
    IsScorch   = (flags & 0x0002) != 0;
    IsBib      = (flags & 0x0004) != 0;
    IsAnimated = (flags & 0x0008) != 0;
    IsFlat     = (flags & 0x0010) != 0;

    hr = pStm->Read(&ChainCount, sizeof(ChainCount), &read);
    if (hr < 0 || read != sizeof(ChainCount)) return E_FAIL;

    hr = pStm->Read(&ChainSteps, sizeof(ChainSteps), &read);
    if (hr < 0 || read != sizeof(ChainSteps)) return E_FAIL;

    hr = pStm->Read(ArtName, sizeof(ArtName), &read);
    if (hr < 0 || read != sizeof(ArtName)) return E_FAIL;
    ArtName[sizeof(ArtName) - 1] = '\0';

    return S_OK;
}

HRESULT SmudgeTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;

    ULONG written = 0;

    hr = pStm->Write(&CellSize, sizeof(CellSize), &written);
    if (hr < 0 || written != sizeof(CellSize)) return E_FAIL;

    hr = pStm->Write(&Frames, sizeof(Frames), &written);
    if (hr < 0 || written != sizeof(Frames)) return E_FAIL;

    // Write bool flags as a bitmask
    uint32 flags = 0;
    if (IsCrater)   flags |= 0x0001;
    if (IsScorch)   flags |= 0x0002;
    if (IsBib)      flags |= 0x0004;
    if (IsAnimated) flags |= 0x0008;
    if (IsFlat)     flags |= 0x0010;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    hr = pStm->Write(&ChainCount, sizeof(ChainCount), &written);
    if (hr < 0 || written != sizeof(ChainCount)) return E_FAIL;

    hr = pStm->Write(&ChainSteps, sizeof(ChainSteps), &written);
    if (hr < 0 || written != sizeof(ChainSteps)) return E_FAIL;

    hr = pStm->Write(ArtName, sizeof(ArtName), &written);
    if (hr < 0 || written != sizeof(ArtName)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType SmudgeTypeClass::WhatAmI() const
{
    return AbstractType::SmudgeType;
}

int32 SmudgeTypeClass::Size() const
{
    return sizeof(SmudgeTypeClass);
}

// ============================================================================
// Classification helpers
// ============================================================================

CellStruct SmudgeTypeClass::Get_Size() const
{
    return CellSize;
}

bool SmudgeTypeClass::Is_Crater() const
{
    return IsCrater;
}

bool SmudgeTypeClass::Is_Scorch() const
{
    return IsScorch;
}

// ============================================================================
// LoadFromINI
// ============================================================================

bool SmudgeTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // Chain the parent first so common fields (Cost, Sight, etc.) are read.
    ObjectTypeClass::LoadFromINI(pINI);

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // ------------------------------------------------------------------
    // Classification flags
    // ------------------------------------------------------------------
    IsCrater   = pINI->ReadBool(section, "Crater",   IsCrater);
    IsScorch   = pINI->ReadBool(section, "Scorch",   IsScorch);
    IsBib      = pINI->ReadBool(section, "Bib",      IsBib);
    IsAnimated = pINI->ReadBool(section, "Animated", IsAnimated);
    IsFlat     = pINI->ReadBool(section, "Flat",     IsFlat);

    // ------------------------------------------------------------------
    // Dimensions
    // ------------------------------------------------------------------
    int32 defSize[2] = { CellSize.X, CellSize.Y };
    int32* pSize = pINI->Read2Integers(defSize, section, "Size", defSize);
    if (pSize != nullptr)
    {
        CellSize.X = static_cast<int16>(pSize[0]);
        CellSize.Y = static_cast<int16>(pSize[1]);
    }

    // ------------------------------------------------------------------
    // Animation / chain parameters
    // ------------------------------------------------------------------
    Frames     = pINI->ReadInteger(section, "Frames",     Frames);
    ChainCount = pINI->ReadInteger(section, "ChainCount", ChainCount);
    ChainSteps = pINI->ReadInteger(section, "ChainSteps", ChainSteps);

    // ------------------------------------------------------------------
    // Art reference - falls back to the type ID when no Image is given.
    // ------------------------------------------------------------------
    char artBuf[64];
    pINI->ReadString(section, "Image", "", artBuf, sizeof(artBuf));
    if (artBuf[0] != '\0')
    {
        int32 j = 0;
        while (artBuf[j] != '\0' && j < static_cast<int32>(sizeof(ArtName) - 1))
        {
            ArtName[j] = artBuf[j];
            ++j;
        }
        ArtName[j] = '\0';
    }
    else if (ID[0] != '\0')
    {
        int32 j = 0;
        while (ID[j] != '\0' && j < static_cast<int32>(sizeof(ArtName) - 1))
        {
            ArtName[j] = ID[j];
            ++j;
        }
        ArtName[j] = '\0';
    }

    return true;
}

// ============================================================================
// SaveToINI
// ============================================================================

bool SmudgeTypeClass::SaveToINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // Chain the parent.
    ObjectTypeClass::SaveToINI(pINI);

    pINI->WriteBool(section, "Crater",   IsCrater);
    pINI->WriteBool(section, "Scorch",   IsScorch);
    pINI->WriteBool(section, "Bib",      IsBib);
    pINI->WriteBool(section, "Animated", IsAnimated);
    pINI->WriteBool(section, "Flat",     IsFlat);

    int32 sizeVals[2] = { CellSize.X, CellSize.Y };
    pINI->Write2Integers(section, "Size", sizeVals);

    pINI->WriteInteger(section, "Frames",     Frames);
    pINI->WriteInteger(section, "ChainCount", ChainCount);
    pINI->WriteInteger(section, "ChainSteps", ChainSteps);

    if (ArtName[0] != '\0')
        pINI->WriteString(section, "Image", ArtName);

    return true;
}

// ============================================================================
// CRC
// ============================================================================

void SmudgeTypeClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectTypeClass::ComputeCRC(crc);

    crc.AddData(&CellSize,   sizeof(CellSize));
    crc.AddData(&Frames,     sizeof(Frames));
    crc.AddData(&IsCrater,   sizeof(IsCrater));
    crc.AddData(&IsScorch,   sizeof(IsScorch));
    crc.AddData(&IsBib,      sizeof(IsBib));
    crc.AddData(&IsAnimated, sizeof(IsAnimated));
    crc.AddData(&IsFlat,     sizeof(IsFlat));
    crc.AddData(&ChainCount, sizeof(ChainCount));
    crc.AddData(&ChainSteps, sizeof(ChainSteps));
    crc.AddData(ArtName,     static_cast<int32>(sizeof(ArtName)));
}

int32 SmudgeTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}
