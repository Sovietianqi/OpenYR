#include <Abstract/TerrainTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>

#include <cstring>
#include <cstdio>

// ============================================================================
// TerrainTypeClass.cpp
//
//  TerrainTypeClass describes a kind of terrain decoration (tree, rock,
//  marble block, etc.) loaded from rules/art INI files.  It inherits
//  ObjectTypeClass and adds:
//
//    * Classification flags (tree / rocks / marble / tiberium / vein / fog)
//    * Flammability and crushability
//    * Tiberium-spawning parameters (type, radius, chance)
//    * Damage and armor properties
//    * Frame count and fire animation reference
//    * Art name (SHP file reference)
//
//  This file implements:
//    * Static Array plumbing (Init_Array / Delete_Array / Delete_All /
//      Find / FindByIndex / GetCount)
//    * Constructor / destructor
//    * LoadFromINI
//    * ComputeCRC / GetCRC
//    * Is_Tree / Is_Rocks / Spawns_Tiberium
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<TerrainTypeClass*>* TerrainTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void TerrainTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<TerrainTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<TerrainTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<TerrainTypeClass*>();
    }
}

void TerrainTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<TerrainTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void TerrainTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        TerrainTypeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

TerrainTypeClass* TerrainTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        TerrainTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

TerrainTypeClass* TerrainTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 TerrainTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

// ============================================================================
// Constructor
// ============================================================================

TerrainTypeClass::TerrainTypeClass(const char* pID) noexcept
    : ObjectTypeClass(pID)
{
    IsMarble            = false;
    IsRocks             = false;
    IsTree              = false;
    IsTiberium          = false;
    SpawnsTiberium      = false;
    IsFlammable         = false;
    IsCrushable         = false;
    IsVein              = false;
    IsFog               = false;
    IsAnimated          = false;
    SpawnsTiberiumType  = 0;
    SpawnsTiberiumRadius = 0;
    SpawnsTiberiumChance = 0;
    Damage              = 0;
    ArmorIndex          = 0;
    FrameCount          = 0;
    FireAnim            = -1;

    std::memset(ArtName, 0, sizeof(ArtName));
}

// ============================================================================
// Destructor
// ============================================================================

TerrainTypeClass::~TerrainTypeClass()
{
    // No heap resources to release.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT TerrainTypeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::TerrainType);
    return S_OK;
}

HRESULT TerrainTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Load(pStm);
    if (hr < 0) return hr;

    ULONG read = 0;

    // Read bool flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsMarble       = (flags & 0x0001) != 0;
    IsRocks        = (flags & 0x0002) != 0;
    IsTree         = (flags & 0x0004) != 0;
    IsTiberium     = (flags & 0x0008) != 0;
    SpawnsTiberium = (flags & 0x0010) != 0;
    IsFlammable    = (flags & 0x0020) != 0;
    IsCrushable    = (flags & 0x0040) != 0;
    IsVein         = (flags & 0x0080) != 0;
    IsFog          = (flags & 0x0100) != 0;
    IsAnimated     = (flags & 0x0200) != 0;

    hr = pStm->Read(&SpawnsTiberiumType, sizeof(SpawnsTiberiumType), &read);
    if (hr < 0 || read != sizeof(SpawnsTiberiumType)) return E_FAIL;

    hr = pStm->Read(&SpawnsTiberiumRadius, sizeof(SpawnsTiberiumRadius), &read);
    if (hr < 0 || read != sizeof(SpawnsTiberiumRadius)) return E_FAIL;

    hr = pStm->Read(&SpawnsTiberiumChance, sizeof(SpawnsTiberiumChance), &read);
    if (hr < 0 || read != sizeof(SpawnsTiberiumChance)) return E_FAIL;

    hr = pStm->Read(&Damage, sizeof(Damage), &read);
    if (hr < 0 || read != sizeof(Damage)) return E_FAIL;

    hr = pStm->Read(&ArmorIndex, sizeof(ArmorIndex), &read);
    if (hr < 0 || read != sizeof(ArmorIndex)) return E_FAIL;

    hr = pStm->Read(&FrameCount, sizeof(FrameCount), &read);
    if (hr < 0 || read != sizeof(FrameCount)) return E_FAIL;

    hr = pStm->Read(&FireAnim, sizeof(FireAnim), &read);
    if (hr < 0 || read != sizeof(FireAnim)) return E_FAIL;

    hr = pStm->Read(ArtName, sizeof(ArtName), &read);
    if (hr < 0 || read != sizeof(ArtName)) return E_FAIL;
    ArtName[sizeof(ArtName) - 1] = '\0';

    return S_OK;
}

HRESULT TerrainTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;

    ULONG written = 0;

    // Write bool flags as a bitmask
    uint32 flags = 0;
    if (IsMarble)       flags |= 0x0001;
    if (IsRocks)        flags |= 0x0002;
    if (IsTree)         flags |= 0x0004;
    if (IsTiberium)     flags |= 0x0008;
    if (SpawnsTiberium) flags |= 0x0010;
    if (IsFlammable)    flags |= 0x0020;
    if (IsCrushable)    flags |= 0x0040;
    if (IsVein)         flags |= 0x0080;
    if (IsFog)          flags |= 0x0100;
    if (IsAnimated)     flags |= 0x0200;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    hr = pStm->Write(&SpawnsTiberiumType, sizeof(SpawnsTiberiumType), &written);
    if (hr < 0 || written != sizeof(SpawnsTiberiumType)) return E_FAIL;

    hr = pStm->Write(&SpawnsTiberiumRadius, sizeof(SpawnsTiberiumRadius), &written);
    if (hr < 0 || written != sizeof(SpawnsTiberiumRadius)) return E_FAIL;

    hr = pStm->Write(&SpawnsTiberiumChance, sizeof(SpawnsTiberiumChance), &written);
    if (hr < 0 || written != sizeof(SpawnsTiberiumChance)) return E_FAIL;

    hr = pStm->Write(&Damage, sizeof(Damage), &written);
    if (hr < 0 || written != sizeof(Damage)) return E_FAIL;

    hr = pStm->Write(&ArmorIndex, sizeof(ArmorIndex), &written);
    if (hr < 0 || written != sizeof(ArmorIndex)) return E_FAIL;

    hr = pStm->Write(&FrameCount, sizeof(FrameCount), &written);
    if (hr < 0 || written != sizeof(FrameCount)) return E_FAIL;

    hr = pStm->Write(&FireAnim, sizeof(FireAnim), &written);
    if (hr < 0 || written != sizeof(FireAnim)) return E_FAIL;

    hr = pStm->Write(ArtName, sizeof(ArtName), &written);
    if (hr < 0 || written != sizeof(ArtName)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType TerrainTypeClass::WhatAmI() const
{
    return AbstractType::TerrainType;
}

int32 TerrainTypeClass::Size() const
{
    return sizeof(TerrainTypeClass);
}

// ============================================================================
// Classification helpers
// ============================================================================

bool TerrainTypeClass::Is_Tree() const
{
    return IsTree;
}

bool TerrainTypeClass::Is_Rocks() const
{
    return IsRocks;
}

bool TerrainTypeClass::Spawns_Tiberium() const
{
    return SpawnsTiberium;
}

// ============================================================================
// LoadFromINI
// ============================================================================

bool TerrainTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // Chain the parent first so common fields are read.
    ObjectTypeClass::LoadFromINI(pINI);

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // ------------------------------------------------------------------
    // Classification flags
    // ------------------------------------------------------------------
    IsTree         = pINI->ReadBool(section, "Tree",        IsTree);
    IsRocks        = pINI->ReadBool(section, "Rocks",       IsRocks);
    IsMarble       = pINI->ReadBool(section, "Marble",      IsMarble);
    IsTiberium     = pINI->ReadBool(section, "Tiberium",    IsTiberium);
    IsVein         = pINI->ReadBool(section, "Veins",       IsVein);
    IsFog          = pINI->ReadBool(section, "Fog",         IsFog);
    IsAnimated     = pINI->ReadBool(section, "Animated",    IsAnimated);
    IsFlammable    = pINI->ReadBool(section, "Flammable",   IsFlammable);
    IsCrushable    = pINI->ReadBool(section, "Crushable",   IsCrushable);
    SpawnsTiberium = pINI->ReadBool(section, "SpawnsTiberium", SpawnsTiberium);

    // ------------------------------------------------------------------
    // Combat properties
    // ------------------------------------------------------------------
    Damage     = pINI->ReadInteger(section, "Damage", Damage);
    ArmorIndex = pINI->ReadInteger(section, "Armor",  ArmorIndex);

    // ------------------------------------------------------------------
    // Tiberium spawning parameters
    // ------------------------------------------------------------------
    SpawnsTiberiumType   = pINI->ReadInteger(section, "SpawnsTiberiumType",   SpawnsTiberiumType);
    SpawnsTiberiumRadius = pINI->ReadInteger(section, "SpawnsTiberiumRadius", SpawnsTiberiumRadius);
    SpawnsTiberiumChance = pINI->ReadInteger(section, "SpawnsTiberiumChance", SpawnsTiberiumChance);

    // ------------------------------------------------------------------
    // Animation properties
    // ------------------------------------------------------------------
    FrameCount = pINI->ReadInteger(section, "Frames",   FrameCount);
    FireAnim   = pINI->ReadInteger(section, "FireAnim", FireAnim);

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
// CRC
// ============================================================================

void TerrainTypeClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectTypeClass::ComputeCRC(crc);

    crc.AddData(&IsMarble,             sizeof(IsMarble));
    crc.AddData(&IsRocks,              sizeof(IsRocks));
    crc.AddData(&IsTree,               sizeof(IsTree));
    crc.AddData(&IsTiberium,           sizeof(IsTiberium));
    crc.AddData(&SpawnsTiberium,       sizeof(SpawnsTiberium));
    crc.AddData(&IsFlammable,          sizeof(IsFlammable));
    crc.AddData(&IsCrushable,          sizeof(IsCrushable));
    crc.AddData(&IsVein,               sizeof(IsVein));
    crc.AddData(&IsFog,                sizeof(IsFog));
    crc.AddData(&IsAnimated,           sizeof(IsAnimated));
    crc.AddData(&SpawnsTiberiumType,   sizeof(SpawnsTiberiumType));
    crc.AddData(&SpawnsTiberiumRadius, sizeof(SpawnsTiberiumRadius));
    crc.AddData(&SpawnsTiberiumChance, sizeof(SpawnsTiberiumChance));
    crc.AddData(&Damage,               sizeof(Damage));
    crc.AddData(&ArmorIndex,           sizeof(ArmorIndex));
    crc.AddData(&FrameCount,           sizeof(FrameCount));
    crc.AddData(&FireAnim,             sizeof(FireAnim));
    crc.AddData(ArtName,               static_cast<int32>(sizeof(ArtName)));
}

int32 TerrainTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}
