#include <Abstract/OverlayTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>

// ============================================================================
// OverlayTypeClass.cpp
//
//  OverlayTypeClass describes a kind of overlay (wall, bridge section, tiberium
//  type, etc.) loaded from rules/art INI files.  It inherits ObjectTypeClass
//  and adds:
//
//    * Wall / tiberium / bridge / ramp classification
//    * Land type override (what land type the overlay creates)
//    * Radar color (minimap representation)
//    * Tiberium growth / spread parameters
//    * Damage and armor properties (for walls)
//    * Art name (SHP file reference)
//
//  This file implements:
//    * Static Array plumbing
//    * Constructor / destructor
//    * LoadFromINI / SaveToINI
//    * ComputeCRC / GetCRC
//    * Is_Wall / Is_Tiberium / Get_Land_Type / Get_Radar_Color
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<OverlayTypeClass*>* OverlayTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void OverlayTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<OverlayTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<OverlayTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<OverlayTypeClass*>();
    }
}

void OverlayTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<OverlayTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void OverlayTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        OverlayTypeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

OverlayTypeClass* OverlayTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        OverlayTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

OverlayTypeClass* OverlayTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 OverlayTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

// ============================================================================
// Constructor
// ============================================================================

OverlayTypeClass::OverlayTypeClass(const char* pID) noexcept
    : ObjectTypeClass(pID)
{
    Damage                 = 0;
    Tiberium               = 0;
    Wall                   = false;
    Climbs                 = false;
    IsTiberium             = false;
    IsVeins                = false;
    IsBridge               = false;
    IsRamp                 = false;
    IsWater                = false;
    IsVisible              = true;
    RadialInventory        = false;
    LandType_              = LandType::Clear;
    RadarColor             = ColorStruct(0, 0, 0);
    RadarBrightness        = 0;
    ArmorIndex             = 0;
    WallBonusDamage        = 0;
    TiberiumGrowthStage    = 0;
    TiberiumSpreadRadius   = 0;
    TiberiumSpreadProbability = 0;
    DeathAnim              = -1;
    ShapeCount             = 0;

    std::memset(ArtName, 0, sizeof(ArtName));
}

// ============================================================================
// Destructor
// ============================================================================

OverlayTypeClass::~OverlayTypeClass()
{
    // No heap resources to release.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT OverlayTypeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::OverlayType);
    return S_OK;
}

HRESULT OverlayTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Load(pStm);
    if (hr < 0) return hr;

    ULONG read = 0;

    hr = pStm->Read(&Damage, sizeof(Damage), &read);
    if (hr < 0 || read != sizeof(Damage)) return E_FAIL;

    hr = pStm->Read(&Tiberium, sizeof(Tiberium), &read);
    if (hr < 0 || read != sizeof(Tiberium)) return E_FAIL;

    // Read bool flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    Wall            = (flags & 0x0001) != 0;
    Climbs          = (flags & 0x0002) != 0;
    IsTiberium      = (flags & 0x0004) != 0;
    IsVeins         = (flags & 0x0008) != 0;
    IsBridge        = (flags & 0x0010) != 0;
    IsRamp          = (flags & 0x0020) != 0;
    IsWater         = (flags & 0x0040) != 0;
    IsVisible       = (flags & 0x0080) != 0;
    RadialInventory = (flags & 0x0100) != 0;

    hr = pStm->Read(&LandType_, sizeof(LandType_), &read);
    if (hr < 0 || read != sizeof(LandType_)) return E_FAIL;

    hr = pStm->Read(&RadarColor, sizeof(RadarColor), &read);
    if (hr < 0 || read != sizeof(RadarColor)) return E_FAIL;

    hr = pStm->Read(&RadarBrightness, sizeof(RadarBrightness), &read);
    if (hr < 0 || read != sizeof(RadarBrightness)) return E_FAIL;

    hr = pStm->Read(&ArmorIndex, sizeof(ArmorIndex), &read);
    if (hr < 0 || read != sizeof(ArmorIndex)) return E_FAIL;

    hr = pStm->Read(&WallBonusDamage, sizeof(WallBonusDamage), &read);
    if (hr < 0 || read != sizeof(WallBonusDamage)) return E_FAIL;

    hr = pStm->Read(&TiberiumGrowthStage, sizeof(TiberiumGrowthStage), &read);
    if (hr < 0 || read != sizeof(TiberiumGrowthStage)) return E_FAIL;

    hr = pStm->Read(&TiberiumSpreadRadius, sizeof(TiberiumSpreadRadius), &read);
    if (hr < 0 || read != sizeof(TiberiumSpreadRadius)) return E_FAIL;

    hr = pStm->Read(&TiberiumSpreadProbability, sizeof(TiberiumSpreadProbability), &read);
    if (hr < 0 || read != sizeof(TiberiumSpreadProbability)) return E_FAIL;

    hr = pStm->Read(&DeathAnim, sizeof(DeathAnim), &read);
    if (hr < 0 || read != sizeof(DeathAnim)) return E_FAIL;

    hr = pStm->Read(&ShapeCount, sizeof(ShapeCount), &read);
    if (hr < 0 || read != sizeof(ShapeCount)) return E_FAIL;

    hr = pStm->Read(ArtName, sizeof(ArtName), &read);
    if (hr < 0 || read != sizeof(ArtName)) return E_FAIL;
    ArtName[sizeof(ArtName) - 1] = '\0';

    return S_OK;
}

HRESULT OverlayTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;

    ULONG written = 0;

    hr = pStm->Write(&Damage, sizeof(Damage), &written);
    if (hr < 0 || written != sizeof(Damage)) return E_FAIL;

    hr = pStm->Write(&Tiberium, sizeof(Tiberium), &written);
    if (hr < 0 || written != sizeof(Tiberium)) return E_FAIL;

    // Write bool flags as a bitmask
    uint32 flags = 0;
    if (Wall)            flags |= 0x0001;
    if (Climbs)          flags |= 0x0002;
    if (IsTiberium)      flags |= 0x0004;
    if (IsVeins)         flags |= 0x0008;
    if (IsBridge)        flags |= 0x0010;
    if (IsRamp)          flags |= 0x0020;
    if (IsWater)         flags |= 0x0040;
    if (IsVisible)       flags |= 0x0080;
    if (RadialInventory) flags |= 0x0100;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    hr = pStm->Write(&LandType_, sizeof(LandType_), &written);
    if (hr < 0 || written != sizeof(LandType_)) return E_FAIL;

    hr = pStm->Write(&RadarColor, sizeof(RadarColor), &written);
    if (hr < 0 || written != sizeof(RadarColor)) return E_FAIL;

    hr = pStm->Write(&RadarBrightness, sizeof(RadarBrightness), &written);
    if (hr < 0 || written != sizeof(RadarBrightness)) return E_FAIL;

    hr = pStm->Write(&ArmorIndex, sizeof(ArmorIndex), &written);
    if (hr < 0 || written != sizeof(ArmorIndex)) return E_FAIL;

    hr = pStm->Write(&WallBonusDamage, sizeof(WallBonusDamage), &written);
    if (hr < 0 || written != sizeof(WallBonusDamage)) return E_FAIL;

    hr = pStm->Write(&TiberiumGrowthStage, sizeof(TiberiumGrowthStage), &written);
    if (hr < 0 || written != sizeof(TiberiumGrowthStage)) return E_FAIL;

    hr = pStm->Write(&TiberiumSpreadRadius, sizeof(TiberiumSpreadRadius), &written);
    if (hr < 0 || written != sizeof(TiberiumSpreadRadius)) return E_FAIL;

    hr = pStm->Write(&TiberiumSpreadProbability, sizeof(TiberiumSpreadProbability), &written);
    if (hr < 0 || written != sizeof(TiberiumSpreadProbability)) return E_FAIL;

    hr = pStm->Write(&DeathAnim, sizeof(DeathAnim), &written);
    if (hr < 0 || written != sizeof(DeathAnim)) return E_FAIL;

    hr = pStm->Write(&ShapeCount, sizeof(ShapeCount), &written);
    if (hr < 0 || written != sizeof(ShapeCount)) return E_FAIL;

    hr = pStm->Write(ArtName, sizeof(ArtName), &written);
    if (hr < 0 || written != sizeof(ArtName)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// RTTI / size / ID
// ============================================================================

AbstractType OverlayTypeClass::WhatAmI() const
{
    return AbstractType::OverlayType;
}

int32 OverlayTypeClass::Size() const
{
    return sizeof(OverlayTypeClass);
}

const char* OverlayTypeClass::get_ID() const
{
    return this->ID;
}

// ============================================================================
// Classification helpers
// ============================================================================

bool OverlayTypeClass::Is_Wall() const
{
    return Wall;
}

bool OverlayTypeClass::Is_Tiberium() const
{
    return IsTiberium;
}

LandType OverlayTypeClass::Get_Land_Type() const
{
    return LandType_;
}

ColorStruct OverlayTypeClass::Get_Radar_Color() const
{
    return RadarColor;
}

// ============================================================================
// LoadFromINI
// ============================================================================

bool OverlayTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // Chain the parent first.
    ObjectTypeClass::LoadFromINI(pINI);

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // ------------------------------------------------------------------
    // Classification flags
    // ------------------------------------------------------------------
    Wall            = pINI->ReadBool(section, "Wall",            Wall);
    Climbs          = pINI->ReadBool(section, "Climbs",          Climbs);
    IsTiberium      = pINI->ReadBool(section, "Tiberium",        IsTiberium);
    IsVeins         = pINI->ReadBool(section, "Veins",           IsVeins);
    IsBridge        = pINI->ReadBool(section, "Bridge",          IsBridge);
    IsRamp          = pINI->ReadBool(section, "Ramp",            IsRamp);
    IsWater         = pINI->ReadBool(section, "Water",           IsWater);
    IsVisible       = pINI->ReadBool(section, "Visible",         IsVisible);
    RadialInventory = pINI->ReadBool(section, "RadialInventory", RadialInventory);

    // ------------------------------------------------------------------
    // Combat properties
    // ------------------------------------------------------------------
    Damage           = pINI->ReadInteger(section, "Damage",          Damage);
    ArmorIndex       = pINI->ReadInteger(section, "Armor",           ArmorIndex);
    WallBonusDamage  = pINI->ReadInteger(section, "WallBonusDamage", WallBonusDamage);
    DeathAnim        = pINI->ReadInteger(section, "DeathAnim",       DeathAnim);

    // ------------------------------------------------------------------
    // Tiberium properties
    // ------------------------------------------------------------------
    Tiberium                 = pINI->ReadInteger(section, "Tiberium",               Tiberium);
    TiberiumGrowthStage      = pINI->ReadInteger(section, "TiberiumGrowthStage",    TiberiumGrowthStage);
    TiberiumSpreadRadius     = pINI->ReadInteger(section, "TiberiumSpreadRadius",   TiberiumSpreadRadius);
    TiberiumSpreadProbability= pINI->ReadInteger(section, "TiberiumSpreadProbability",
                                                  TiberiumSpreadProbability);

    // ------------------------------------------------------------------
    // Land type
    // ------------------------------------------------------------------
    char landBuf[32];
    pINI->ReadString(section, "Land", "Clear", landBuf, sizeof(landBuf));
    if (!_strcmpi(landBuf, "Clear"))       LandType_ = LandType::Clear;
    else if (!_strcmpi(landBuf, "Rough"))  LandType_ = LandType::Rough;
    else if (!_strcmpi(landBuf, "Road"))   LandType_ = LandType::Road;
    else if (!_strcmpi(landBuf, "Water"))  LandType_ = LandType::Water;
    else if (!_strcmpi(landBuf, "Rock"))   LandType_ = LandType::Rock;
    else if (!_strcmpi(landBuf, "Wall"))   LandType_ = LandType::Wall;
    else if (!_strcmpi(landBuf, "Tiberium"))LandType_= LandType::Tiberium;
    else if (!_strcmpi(landBuf, "Beach"))  LandType_ = LandType::Beach;
    else if (!_strcmpi(landBuf, "Tunnel")) LandType_ = LandType::Tunnel;
    else if (!_strcmpi(landBuf, "Railroad"))LandType_= LandType::Railroad;
    else if (!_strcmpi(landBuf, "Weeds"))  LandType_ = LandType::Weeds;
    else if (!_strcmpi(landBuf, "Ice"))    LandType_ = LandType::Ice;
    else                                   LandType_ = LandType::Clear;

    // ------------------------------------------------------------------
    // Radar color (parsed as R,G,B)
    // ------------------------------------------------------------------
    char radarBuf[64];
    pINI->ReadString(section, "RadarColor", "0,0,0", radarBuf, sizeof(radarBuf));
    int32 r = 0, g = 0, b = 0;
    int32 idx = 0;
    int32 val = 0;
    int32 comp = 0;
    while (radarBuf[idx] != '\0' && idx < 32)
    {
        char c = radarBuf[idx];
        if (c >= '0' && c <= '9')
        {
            val = val * 10 + (c - '0');
        }
        else if (c == ',')
        {
            if (comp == 0) r = val;
            else if (comp == 1) g = val;
            val = 0;
            ++comp;
        }
        else
        {
            break;
        }
        ++idx;
    }
    if (comp == 2) b = val;
    else if (comp == 1) { b = val; }
    else if (comp == 0) { r = val; }
    RadarColor = ColorStruct(static_cast<uint8>(r), static_cast<uint8>(g), static_cast<uint8>(b));

    RadarBrightness = pINI->ReadInteger(section, "RadarBrightness", RadarBrightness);

    // ------------------------------------------------------------------
    // Art reference
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

    ShapeCount = pINI->ReadInteger(section, "ShapeCount", ShapeCount);

    return true;
}

// ============================================================================
// SaveToINI
// ============================================================================

bool OverlayTypeClass::SaveToINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // Chain the parent.
    ObjectTypeClass::SaveToINI(pINI);

    pINI->WriteInteger(section, "Damage",          Damage);
    pINI->WriteInteger(section, "Tiberium",        Tiberium);
    pINI->WriteInteger(section, "Armor",           ArmorIndex);
    pINI->WriteInteger(section, "WallBonusDamage", WallBonusDamage);
    pINI->WriteInteger(section, "TiberiumGrowthStage",     TiberiumGrowthStage);
    pINI->WriteInteger(section, "TiberiumSpreadRadius",    TiberiumSpreadRadius);
    pINI->WriteInteger(section, "TiberiumSpreadProbability",TiberiumSpreadProbability);
    pINI->WriteInteger(section, "DeathAnim",       DeathAnim);
    pINI->WriteInteger(section, "ShapeCount",      ShapeCount);
    pINI->WriteInteger(section, "RadarBrightness", RadarBrightness);

    pINI->WriteBool(section, "Wall",            Wall);
    pINI->WriteBool(section, "Climbs",          Climbs);
    pINI->WriteBool(section, "Tiberium",        IsTiberium);
    pINI->WriteBool(section, "Veins",           IsVeins);
    pINI->WriteBool(section, "Bridge",          IsBridge);
    pINI->WriteBool(section, "Ramp",            IsRamp);
    pINI->WriteBool(section, "Water",           IsWater);
    pINI->WriteBool(section, "Visible",         IsVisible);
    pINI->WriteBool(section, "RadialInventory", RadialInventory);

    // Land type
    const char* landName = "Clear";
    switch (LandType_)
    {
        case LandType::Rough:    landName = "Rough";    break;
        case LandType::Road:     landName = "Road";     break;
        case LandType::Water:    landName = "Water";    break;
        case LandType::Rock:     landName = "Rock";     break;
        case LandType::Wall:     landName = "Wall";     break;
        case LandType::Tiberium: landName = "Tiberium"; break;
        case LandType::Beach:    landName = "Beach";    break;
        case LandType::Tunnel:   landName = "Tunnel";   break;
        case LandType::Railroad: landName = "Railroad"; break;
        case LandType::Weeds:    landName = "Weeds";    break;
        case LandType::Ice:      landName = "Ice";      break;
        default:                 landName = "Clear";    break;
    }
    pINI->WriteString(section, "Land", landName);

    // Radar color
    char radarBuf[32];
    sprintf_s(radarBuf, sizeof(radarBuf), "%d,%d,%d",
              RadarColor.R, RadarColor.G, RadarColor.B);
    pINI->WriteString(section, "RadarColor", radarBuf);

    if (ArtName[0] != '\0')
        pINI->WriteString(section, "Image", ArtName);

    return true;
}

// ============================================================================
// CRC
// ============================================================================

void OverlayTypeClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectTypeClass::ComputeCRC(crc);

    crc.AddData(&Damage,                   sizeof(Damage));
    crc.AddData(&Tiberium,                 sizeof(Tiberium));
    crc.AddData(&Wall,                     sizeof(Wall));
    crc.AddData(&Climbs,                   sizeof(Climbs));
    crc.AddData(&IsTiberium,               sizeof(IsTiberium));
    crc.AddData(&IsVeins,                  sizeof(IsVeins));
    crc.AddData(&IsBridge,                 sizeof(IsBridge));
    crc.AddData(&IsRamp,                   sizeof(IsRamp));
    crc.AddData(&IsWater,                  sizeof(IsWater));
    crc.AddData(&IsVisible,                sizeof(IsVisible));
    crc.AddData(&RadialInventory,          sizeof(RadialInventory));
    crc.AddData(&LandType_,                sizeof(LandType_));
    crc.AddData(&RadarColor,               sizeof(RadarColor));
    crc.AddData(&RadarBrightness,          sizeof(RadarBrightness));
    crc.AddData(&ArmorIndex,               sizeof(ArmorIndex));
    crc.AddData(&WallBonusDamage,          sizeof(WallBonusDamage));
    crc.AddData(&TiberiumGrowthStage,      sizeof(TiberiumGrowthStage));
    crc.AddData(&TiberiumSpreadRadius,     sizeof(TiberiumSpreadRadius));
    crc.AddData(&TiberiumSpreadProbability,sizeof(TiberiumSpreadProbability));
    crc.AddData(&DeathAnim,                sizeof(DeathAnim));
    crc.AddData(&ShapeCount,               sizeof(ShapeCount));
    crc.AddData(ArtName,                   static_cast<int32>(sizeof(ArtName)));
}

int32 OverlayTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}
