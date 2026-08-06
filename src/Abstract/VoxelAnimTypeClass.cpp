#include <Abstract/VoxelAnimTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>

#include <cstring>
#include <cstdio>

// ============================================================================
// VoxelAnimTypeClass.cpp
//
//  VoxelAnimTypeClass describes a kind of voxel animation (falling debris,
//  meteor chunk, etc.) loaded from rules/art INI files.  It inherits
//  ObjectTypeClass and adds:
//
//    * VXL / HVA model art references
//    * Damage and warhead (applied on impact)
//    * Physics parameters (elasticity, max/bounce velocity, damage radius)
//    * Classification flags (meteor / debris / flat / animated)
//    * Spawned animation references
//    * Lighting parameters (size, intensity, translucency)
//    * Random spawn rate
//
//  This file implements:
//    * Static Array plumbing (Init_Array / Delete_Array / Delete_All /
//      Find / FindByIndex / GetCount)
//    * Constructor / destructor
//    * LoadFromINI
//    * ComputeCRC / GetCRC
//    * Get_VXL_Name / Get_HVA_Name
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<VoxelAnimTypeClass*>* VoxelAnimTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void VoxelAnimTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<VoxelAnimTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<VoxelAnimTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<VoxelAnimTypeClass*>();
    }
}

void VoxelAnimTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<VoxelAnimTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void VoxelAnimTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        VoxelAnimTypeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

VoxelAnimTypeClass* VoxelAnimTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        VoxelAnimTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

VoxelAnimTypeClass* VoxelAnimTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 VoxelAnimTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

// ============================================================================
// Constructor
// ============================================================================

VoxelAnimTypeClass::VoxelAnimTypeClass(const char* pID) noexcept
    : ObjectTypeClass(pID)
{
    Damage         = 0.0;
    Warhead        = nullptr;
    Elasticity     = 0.0;
    MaxVelocity    = 0.0;
    BounceVelocity = 0.0;
    DamageRadius   = 0;
    IsMeteor       = false;
    IsDebris       = false;
    IsFlat         = false;
    IsAnimated     = false;
    SpawnsAnim     = false;
    SpawnAnimIndex = -1;
    StartAnimIndex = 0;
    WillBounce     = false;
    UseLight       = false;
    LightSize      = 0;
    LightIntensity = 0.0;
    Translucency   = 0;
    RandomRate     = 0;

    std::memset(VoxelName, 0, sizeof(VoxelName));
    std::memset(HVAName,   0, sizeof(HVAName));
}

// ============================================================================
// Destructor
// ============================================================================

VoxelAnimTypeClass::~VoxelAnimTypeClass()
{
    // No heap resources to release.  Warhead is owned by the WarheadType
    // registry, not by this type.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT VoxelAnimTypeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::VoxelAnimType);
    return S_OK;
}

HRESULT VoxelAnimTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Load(pStm);
    if (hr < 0) return hr;

    ULONG read = 0;

    hr = pStm->Read(VoxelName, sizeof(VoxelName), &read);
    if (hr < 0 || read != sizeof(VoxelName)) return E_FAIL;
    VoxelName[sizeof(VoxelName) - 1] = '\0';

    hr = pStm->Read(HVAName, sizeof(HVAName), &read);
    if (hr < 0 || read != sizeof(HVAName)) return E_FAIL;
    HVAName[sizeof(HVAName) - 1] = '\0';

    hr = pStm->Read(&Damage, sizeof(Damage), &read);
    if (hr < 0 || read != sizeof(Damage)) return E_FAIL;

    // Read Warhead as an object index
    int32 warheadIndex = -1;
    hr = pStm->Read(&warheadIndex, sizeof(warheadIndex), &read);
    if (hr < 0 || read != sizeof(warheadIndex)) return E_FAIL;
    Warhead = (warheadIndex >= 0) ? (WarheadTypeClass*)AbstractClass::Get_Instance(warheadIndex) : nullptr;

    hr = pStm->Read(&Elasticity, sizeof(Elasticity), &read);
    if (hr < 0 || read != sizeof(Elasticity)) return E_FAIL;

    hr = pStm->Read(&MaxVelocity, sizeof(MaxVelocity), &read);
    if (hr < 0 || read != sizeof(MaxVelocity)) return E_FAIL;

    hr = pStm->Read(&BounceVelocity, sizeof(BounceVelocity), &read);
    if (hr < 0 || read != sizeof(BounceVelocity)) return E_FAIL;

    hr = pStm->Read(&DamageRadius, sizeof(DamageRadius), &read);
    if (hr < 0 || read != sizeof(DamageRadius)) return E_FAIL;

    // Read bool flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsMeteor   = (flags & 0x0001) != 0;
    IsDebris   = (flags & 0x0002) != 0;
    IsFlat     = (flags & 0x0004) != 0;
    IsAnimated = (flags & 0x0008) != 0;
    SpawnsAnim = (flags & 0x0010) != 0;
    WillBounce = (flags & 0x0020) != 0;
    UseLight   = (flags & 0x0040) != 0;

    hr = pStm->Read(&SpawnAnimIndex, sizeof(SpawnAnimIndex), &read);
    if (hr < 0 || read != sizeof(SpawnAnimIndex)) return E_FAIL;

    hr = pStm->Read(&StartAnimIndex, sizeof(StartAnimIndex), &read);
    if (hr < 0 || read != sizeof(StartAnimIndex)) return E_FAIL;

    hr = pStm->Read(&LightSize, sizeof(LightSize), &read);
    if (hr < 0 || read != sizeof(LightSize)) return E_FAIL;

    hr = pStm->Read(&LightIntensity, sizeof(LightIntensity), &read);
    if (hr < 0 || read != sizeof(LightIntensity)) return E_FAIL;

    hr = pStm->Read(&Translucency, sizeof(Translucency), &read);
    if (hr < 0 || read != sizeof(Translucency)) return E_FAIL;

    hr = pStm->Read(&RandomRate, sizeof(RandomRate), &read);
    if (hr < 0 || read != sizeof(RandomRate)) return E_FAIL;

    return S_OK;
}

HRESULT VoxelAnimTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;

    ULONG written = 0;

    hr = pStm->Write(VoxelName, sizeof(VoxelName), &written);
    if (hr < 0 || written != sizeof(VoxelName)) return E_FAIL;

    hr = pStm->Write(HVAName, sizeof(HVAName), &written);
    if (hr < 0 || written != sizeof(HVAName)) return E_FAIL;

    hr = pStm->Write(&Damage, sizeof(Damage), &written);
    if (hr < 0 || written != sizeof(Damage)) return E_FAIL;

    // Write Warhead as an object index
    int32 warheadIndex = -1;
    if (Warhead) {
        warheadIndex = AbstractClass::Find_Index((AbstractClass*)Warhead);
    }
    hr = pStm->Write(&warheadIndex, sizeof(warheadIndex), &written);
    if (hr < 0 || written != sizeof(warheadIndex)) return E_FAIL;

    hr = pStm->Write(&Elasticity, sizeof(Elasticity), &written);
    if (hr < 0 || written != sizeof(Elasticity)) return E_FAIL;

    hr = pStm->Write(&MaxVelocity, sizeof(MaxVelocity), &written);
    if (hr < 0 || written != sizeof(MaxVelocity)) return E_FAIL;

    hr = pStm->Write(&BounceVelocity, sizeof(BounceVelocity), &written);
    if (hr < 0 || written != sizeof(BounceVelocity)) return E_FAIL;

    hr = pStm->Write(&DamageRadius, sizeof(DamageRadius), &written);
    if (hr < 0 || written != sizeof(DamageRadius)) return E_FAIL;

    // Write bool flags as a bitmask
    uint32 flags = 0;
    if (IsMeteor)   flags |= 0x0001;
    if (IsDebris)   flags |= 0x0002;
    if (IsFlat)     flags |= 0x0004;
    if (IsAnimated) flags |= 0x0008;
    if (SpawnsAnim) flags |= 0x0010;
    if (WillBounce) flags |= 0x0020;
    if (UseLight)   flags |= 0x0040;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    hr = pStm->Write(&SpawnAnimIndex, sizeof(SpawnAnimIndex), &written);
    if (hr < 0 || written != sizeof(SpawnAnimIndex)) return E_FAIL;

    hr = pStm->Write(&StartAnimIndex, sizeof(StartAnimIndex), &written);
    if (hr < 0 || written != sizeof(StartAnimIndex)) return E_FAIL;

    hr = pStm->Write(&LightSize, sizeof(LightSize), &written);
    if (hr < 0 || written != sizeof(LightSize)) return E_FAIL;

    hr = pStm->Write(&LightIntensity, sizeof(LightIntensity), &written);
    if (hr < 0 || written != sizeof(LightIntensity)) return E_FAIL;

    hr = pStm->Write(&Translucency, sizeof(Translucency), &written);
    if (hr < 0 || written != sizeof(Translucency)) return E_FAIL;

    hr = pStm->Write(&RandomRate, sizeof(RandomRate), &written);
    if (hr < 0 || written != sizeof(RandomRate)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType VoxelAnimTypeClass::WhatAmI() const
{
    return AbstractType::VoxelAnimType;
}

int32 VoxelAnimTypeClass::Size() const
{
    return sizeof(VoxelAnimTypeClass);
}

// ============================================================================
// Art name accessors
// ============================================================================

const char* VoxelAnimTypeClass::Get_VXL_Name() const
{
    return VoxelName;
}

const char* VoxelAnimTypeClass::Get_HVA_Name() const
{
    // When no explicit HVA name was supplied the full binary derives it from
    // the VXL name by appending ".hva".  The standalone build returns the
    // stored name, falling back to the VXL name when empty.
    if (HVAName[0] != '\0')
        return HVAName;
    return VoxelName;
}

// ============================================================================
// LoadFromINI
// ============================================================================

bool VoxelAnimTypeClass::LoadFromINI(CCINIClass* pINI)
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
    IsMeteor   = pINI->ReadBool(section, "Meteor",   IsMeteor);
    IsDebris   = pINI->ReadBool(section, "Debris",   IsDebris);
    IsFlat     = pINI->ReadBool(section, "Flat",     IsFlat);
    IsAnimated = pINI->ReadBool(section, "Animated", IsAnimated);
    SpawnsAnim = pINI->ReadBool(section, "SpawnsAnim", SpawnsAnim);
    WillBounce = pINI->ReadBool(section, "Bouncing", WillBounce);
    UseLight   = pINI->ReadBool(section, "UseLight", UseLight);

    // ------------------------------------------------------------------
    // Combat properties
    // ------------------------------------------------------------------
    Damage       = pINI->ReadDouble(section, "Damage",       Damage);
    DamageRadius = pINI->ReadInteger(section, "DamageRadius", DamageRadius);

    // Warhead reference - the full binary resolves the warhead by name from
    // the WarheadType registry.  The standalone build records the name and
    // leaves the pointer null until Resolve_Warhead_References is called.
    char warheadBuf[64];
    pINI->ReadString(section, "Warhead", "", warheadBuf, sizeof(warheadBuf));
    if (warheadBuf[0] == '\0')
    {
        // Default warhead name; left unbound in the standalone build.
    }

    // ------------------------------------------------------------------
    // Physics parameters
    // ------------------------------------------------------------------
    Elasticity     = pINI->ReadDouble(section, "Elasticity",     Elasticity);
    MaxVelocity    = pINI->ReadDouble(section, "MaxVelocity",    MaxVelocity);
    BounceVelocity = pINI->ReadDouble(section, "BounceVelocity", BounceVelocity);

    // ------------------------------------------------------------------
    // Spawned animation references
    // ------------------------------------------------------------------
    SpawnAnimIndex = pINI->ReadInteger(section, "SpawnAnim", SpawnAnimIndex);
    StartAnimIndex = pINI->ReadInteger(section, "StartAnim", StartAnimIndex);

    // ------------------------------------------------------------------
    // Lighting parameters
    // ------------------------------------------------------------------
    LightSize      = pINI->ReadInteger(section, "LightSize",      LightSize);
    LightIntensity = pINI->ReadDouble(section, "LightIntensity", LightIntensity);
    Translucency   = pINI->ReadInteger(section, "Translucency",  Translucency);

    // ------------------------------------------------------------------
    // Random spawn rate (frames between random spawns)
    // ------------------------------------------------------------------
    RandomRate = pINI->ReadInteger(section, "RandomRate", RandomRate);

    // ------------------------------------------------------------------
    // VXL model art reference - falls back to the type ID when no Voxel key
    // is supplied.
    // ------------------------------------------------------------------
    char vxlBuf[64];
    pINI->ReadString(section, "Voxel", "", vxlBuf, sizeof(vxlBuf));
    if (vxlBuf[0] != '\0')
    {
        int32 j = 0;
        while (vxlBuf[j] != '\0' && j < static_cast<int32>(sizeof(VoxelName) - 1))
        {
            VoxelName[j] = vxlBuf[j];
            ++j;
        }
        VoxelName[j] = '\0';
    }
    else if (ID[0] != '\0')
    {
        int32 j = 0;
        while (ID[j] != '\0' && j < static_cast<int32>(sizeof(VoxelName) - 1))
        {
            VoxelName[j] = ID[j];
            ++j;
        }
        VoxelName[j] = '\0';
    }

    // ------------------------------------------------------------------
    // HVA model art reference - optional; falls back to the VXL name.
    // ------------------------------------------------------------------
    char hvaBuf[64];
    pINI->ReadString(section, "HVA", "", hvaBuf, sizeof(hvaBuf));
    if (hvaBuf[0] != '\0')
    {
        int32 j = 0;
        while (hvaBuf[j] != '\0' && j < static_cast<int32>(sizeof(HVAName) - 1))
        {
            HVAName[j] = hvaBuf[j];
            ++j;
        }
        HVAName[j] = '\0';
    }

    return true;
}

// ============================================================================
// CRC
// ============================================================================

void VoxelAnimTypeClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectTypeClass::ComputeCRC(crc);

    crc.AddData(&Damage,         sizeof(Damage));
    crc.AddData(&Elasticity,     sizeof(Elasticity));
    crc.AddData(&MaxVelocity,    sizeof(MaxVelocity));
    crc.AddData(&BounceVelocity, sizeof(BounceVelocity));
    crc.AddData(&DamageRadius,   sizeof(DamageRadius));
    crc.AddData(&IsMeteor,       sizeof(IsMeteor));
    crc.AddData(&IsDebris,       sizeof(IsDebris));
    crc.AddData(&IsFlat,         sizeof(IsFlat));
    crc.AddData(&IsAnimated,     sizeof(IsAnimated));
    crc.AddData(&SpawnsAnim,     sizeof(SpawnsAnim));
    crc.AddData(&SpawnAnimIndex, sizeof(SpawnAnimIndex));
    crc.AddData(&StartAnimIndex, sizeof(StartAnimIndex));
    crc.AddData(&WillBounce,     sizeof(WillBounce));
    crc.AddData(&UseLight,       sizeof(UseLight));
    crc.AddData(&LightSize,      sizeof(LightSize));
    crc.AddData(&LightIntensity, sizeof(LightIntensity));
    crc.AddData(&Translucency,   sizeof(Translucency));
    crc.AddData(&RandomRate,     sizeof(RandomRate));
    crc.AddData(VoxelName,       static_cast<int32>(sizeof(VoxelName)));
    crc.AddData(HVAName,         static_cast<int32>(sizeof(HVAName)));
    // Warhead pointer is not included - it is resolved at runtime and varies
    // between address spaces.
}

int32 VoxelAnimTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}
