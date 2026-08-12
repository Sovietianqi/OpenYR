#include <Abstract/ObjectTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>
#include <Map/MapClass.h>
#include <Houses/HouseClass.h>
#include <FileFormats/SHP.h>

#include <cstring>
#include <cstdlib>

// ============================================================================
// ObjectTypeClass.cpp
//
//  ObjectTypeClass is the shared base for every "placeable" type - anything
//  that can exist on the map (buildings, infantry, vehicles, aircraft,
//  overlays, smudges, terrain, voxel anims).  It sits between
//  AbstractTypeClass (which owns the ID / name / cost bookkeeping) and the
//  concrete TechnoTypeClass / OverlayTypeClass / SmudgeTypeClass / etc.
//
//  This file provides:
//    * Static Array plumbing (Init_Array / Delete_Array / Find / FindByIndex
//      / GetCount / Delete_All)
//    * Constructor / destructor
//    * IPersistStream implementations
//    * LoadFromINI / SaveToINI - parses the fields common to every object
//      type (Sight, Cost, TechLevel, Speed, BuildLimit, Score, ROT, ...)
//    * ComputeCRC / GetCRC
//    * Read_INI / Write_INI delegates
//    * Can_Place_On_Map / Get_Occupy_Bits - placement helpers
//    * Is_Temple_Of_NOD / Is_Flak / Is_Listed / Get_Max_Pips - type-flag
//      helpers used by the sidebar and AI
//    * Create_One_Of / Get_Cameo_Data - factory + sidebar art
//    * Read_TypeFlags - parses Yes/No flags into the bool fields
//    * Resolve_SHP_References - SHP art resolution hook
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<ObjectTypeClass*>* ObjectTypeClass::Array = nullptr;

// ============================================================================
// Init_Array / Delete_Array
//
//  Mirror the AbstractTypeClass helpers but operate on the typed
//  ObjectTypeClass array.  Both arrays coexist in the original binary -
//  AbstractTypeClass::Array is the polymorphic root and ObjectTypeClass::
//  Array narrows the iteration to only placeable types.
// ============================================================================
void ObjectTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<ObjectTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<ObjectTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<ObjectTypeClass*>();
    }
}

void ObjectTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<ObjectTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

// ============================================================================
// Find / FindByIndex / GetCount
// ============================================================================
ObjectTypeClass* ObjectTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        ObjectTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

ObjectTypeClass* ObjectTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 ObjectTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

// ============================================================================
// Delete_All
//
//  Destroys every registered ObjectTypeClass and clears the array.
// ============================================================================
void ObjectTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        ObjectTypeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

// ============================================================================
// Construction / destruction
// ============================================================================

ObjectTypeClass::ObjectTypeClass() noexcept
    : AbstractTypeClass(noinit)
{
    Sight          = 0;
    Speed          = 0;
    MaxStrength    = 0;
    BuildLimit     = -1;
    Score          = 0;
    ROT            = 0;
    Selectable     = true;
    LegalTarget    = true;
    Insignificant  = false;
    Immune         = false;
    OnFire         = false;
    Repairable     = true;
    Unsellable     = false;
    Cloakable      = false;
    TurretEquipped = false;
    IsStealthy     = false;
    IsTrainable    = false;
    IsNotHuman     = true;
    IsTheater      = false;
    IdleLayer      = Layer::Ground;
    Land           = LandType::Clear;
}

ObjectTypeClass::ObjectTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID)
{
    Sight          = 0;
    Speed          = 0;
    MaxStrength    = 0;
    BuildLimit     = -1;
    Score          = 0;
    ROT            = 0;
    Selectable     = true;
    LegalTarget    = true;
    Insignificant  = false;
    Immune         = false;
    OnFire         = false;
    Repairable     = true;
    Unsellable     = false;
    Cloakable      = false;
    TurretEquipped = false;
    IsStealthy     = false;
    IsTrainable    = false;
    IsNotHuman     = true;
    IsTheater      = false;
    IdleLayer      = Layer::Ground;
    Land           = LandType::Clear;
}

ObjectTypeClass::ObjectTypeClass(noinit_t) noexcept
    : AbstractTypeClass(noinit)
{
    // Subclasses own initialisation; leave fields untouched.
}

ObjectTypeClass::~ObjectTypeClass()
{
    // No heap resources to release at this level.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT ObjectTypeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Object);
    return S_OK;
}

HRESULT ObjectTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = AbstractTypeClass::Load(pStm);
    if (hr < 0) return hr;

    ULONG read = 0;

    hr = pStm->Read(&Sight, sizeof(Sight), &read);
    if (hr < 0 || read != sizeof(Sight)) return E_FAIL;

    hr = pStm->Read(&Speed, sizeof(Speed), &read);
    if (hr < 0 || read != sizeof(Speed)) return E_FAIL;

    hr = pStm->Read(&MaxStrength, sizeof(MaxStrength), &read);
    if (hr < 0 || read != sizeof(MaxStrength)) return E_FAIL;

    hr = pStm->Read(&BuildLimit, sizeof(BuildLimit), &read);
    if (hr < 0 || read != sizeof(BuildLimit)) return E_FAIL;

    hr = pStm->Read(&Score, sizeof(Score), &read);
    if (hr < 0 || read != sizeof(Score)) return E_FAIL;

    hr = pStm->Read(&ROT, sizeof(ROT), &read);
    if (hr < 0 || read != sizeof(ROT)) return E_FAIL;

    // Read bool flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    Selectable     = (flags & 0x0001) != 0;
    LegalTarget    = (flags & 0x0002) != 0;
    Insignificant  = (flags & 0x0004) != 0;
    Immune         = (flags & 0x0008) != 0;
    OnFire         = (flags & 0x0010) != 0;
    Repairable     = (flags & 0x0020) != 0;
    Unsellable     = (flags & 0x0040) != 0;
    Cloakable      = (flags & 0x0080) != 0;
    TurretEquipped = (flags & 0x0100) != 0;
    IsStealthy     = (flags & 0x0200) != 0;
    IsTrainable    = (flags & 0x0400) != 0;
    IsNotHuman     = (flags & 0x0800) != 0;
    IsTheater      = (flags & 0x1000) != 0;

    hr = pStm->Read(&IdleLayer, sizeof(IdleLayer), &read);
    if (hr < 0 || read != sizeof(IdleLayer)) return E_FAIL;

    hr = pStm->Read(&Land, sizeof(Land), &read);
    if (hr < 0 || read != sizeof(Land)) return E_FAIL;

    return S_OK;
}

HRESULT ObjectTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = AbstractTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;

    ULONG written = 0;

    hr = pStm->Write(&Sight, sizeof(Sight), &written);
    if (hr < 0 || written != sizeof(Sight)) return E_FAIL;

    hr = pStm->Write(&Speed, sizeof(Speed), &written);
    if (hr < 0 || written != sizeof(Speed)) return E_FAIL;

    hr = pStm->Write(&MaxStrength, sizeof(MaxStrength), &written);
    if (hr < 0 || written != sizeof(MaxStrength)) return E_FAIL;

    hr = pStm->Write(&BuildLimit, sizeof(BuildLimit), &written);
    if (hr < 0 || written != sizeof(BuildLimit)) return E_FAIL;

    hr = pStm->Write(&Score, sizeof(Score), &written);
    if (hr < 0 || written != sizeof(Score)) return E_FAIL;

    hr = pStm->Write(&ROT, sizeof(ROT), &written);
    if (hr < 0 || written != sizeof(ROT)) return E_FAIL;

    // Write bool flags as a bitmask
    uint32 flags = 0;
    if (Selectable)     flags |= 0x0001;
    if (LegalTarget)    flags |= 0x0002;
    if (Insignificant)  flags |= 0x0004;
    if (Immune)         flags |= 0x0008;
    if (OnFire)         flags |= 0x0010;
    if (Repairable)     flags |= 0x0020;
    if (Unsellable)     flags |= 0x0040;
    if (Cloakable)      flags |= 0x0080;
    if (TurretEquipped) flags |= 0x0100;
    if (IsStealthy)     flags |= 0x0200;
    if (IsTrainable)    flags |= 0x0400;
    if (IsNotHuman)     flags |= 0x0800;
    if (IsTheater)      flags |= 0x1000;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    hr = pStm->Write(&IdleLayer, sizeof(IdleLayer), &written);
    if (hr < 0 || written != sizeof(IdleLayer)) return E_FAIL;

    hr = pStm->Write(&Land, sizeof(Land), &written);
    if (hr < 0 || written != sizeof(Land)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType ObjectTypeClass::WhatAmI() const
{
    return AbstractType::Object;
}

int32 ObjectTypeClass::Size() const
{
    return sizeof(ObjectTypeClass);
}

// ============================================================================
// LoadFromINI
//
//  Parses the fields common to every object type from the rules-INI block
//  whose name matches the type's ID.  Subclasses chain this before reading
//  their own fields.
// ============================================================================
bool ObjectTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // ------------------------------------------------------------------
    // Economy / build metadata
    // ------------------------------------------------------------------
    Cost       = pINI->ReadInteger(section, "Cost",       Cost);
    TechLevel  = pINI->ReadInteger(section, "TechLevel",  TechLevel);
    BuildLimit = pINI->ReadInteger(section, "BuildLimit", BuildLimit);
    Score      = pINI->ReadInteger(section, "Score",      Score);

    // ------------------------------------------------------------------
    // Combat / movement
    // ------------------------------------------------------------------
    Sight       = pINI->ReadInteger(section, "Sight",      Sight);
    Speed       = pINI->ReadInteger(section, "Speed",      Speed);
    MaxStrength = pINI->ReadInteger(section, "Strength",   MaxStrength);
    ROT         = pINI->ReadInteger(section, "ROT",        ROT);

    // ------------------------------------------------------------------
    // UI name label - resolved against the string table at runtime
    // ------------------------------------------------------------------
    char labelBuf[64];
    pINI->ReadString(section, "UIName", "", labelBuf, sizeof(labelBuf));
    if (labelBuf[0] != '\0')
    {
        int32 j = 0;
        while (labelBuf[j] != '\0' && j < static_cast<int32>(sizeof(UINameLabel) - 1))
        {
            UINameLabel[j] = labelBuf[j];
            ++j;
        }
        UINameLabel[j] = '\0';
    }

    // ------------------------------------------------------------------
    // Display name (human-readable fallback when UIName is empty)
    // ------------------------------------------------------------------
    char nameBuf[64];
    pINI->ReadString(section, "Name", "", nameBuf, sizeof(nameBuf));
    if (nameBuf[0] != '\0')
    {
        int32 j = 0;
        while (nameBuf[j] != '\0' && j < static_cast<int32>(sizeof(Name) - 1))
        {
            Name[j] = nameBuf[j];
            ++j;
        }
        Name[j] = '\0';
    }

    // ------------------------------------------------------------------
    // Common boolean flags
    // ------------------------------------------------------------------
    Selectable     = pINI->ReadBool(section, "Selectable",     Selectable);
    LegalTarget    = pINI->ReadBool(section, "LegalTarget",    LegalTarget);
    Insignificant  = pINI->ReadBool(section, "Insignificant",  Insignificant);
    Immune         = pINI->ReadBool(section, "Immune",         Immune);
    OnFire         = pINI->ReadBool(section, "OnFire",         OnFire);
    Repairable     = pINI->ReadBool(section, "Repairable",     Repairable);
    Unsellable     = pINI->ReadBool(section, "Unsellable",     Unsellable);
    Cloakable      = pINI->ReadBool(section, "Cloakable",      Cloakable);
    TurretEquipped = pINI->ReadBool(section, "Turret",         TurretEquipped);
    IsStealthy     = pINI->ReadBool(section, "Stealthy",       IsStealthy);
    IsTrainable    = pINI->ReadBool(section, "Trainable",      IsTrainable);
    IsNotHuman     = pINI->ReadBool(section, "NotHuman",       IsNotHuman);
    IsTheater      = pINI->ReadBool(section, "Theater",        IsTheater);

    // ------------------------------------------------------------------
    // Render layer
    // ------------------------------------------------------------------
    char layerBuf[32];
    pINI->ReadString(section, "Layer", "Ground", layerBuf, sizeof(layerBuf));
    if (!_strcmpi(layerBuf, "Air"))          IdleLayer = Layer::Air;
    else if (!_strcmpi(layerBuf, "Top"))     IdleLayer = Layer::Top;
    else if (!_strcmpi(layerBuf, "Surface")) IdleLayer = Layer::Surface;
    else                                     IdleLayer = Layer::Ground;

    // ------------------------------------------------------------------
    // Land type the object occupies
    // ------------------------------------------------------------------
    char landBuf[32];
    pINI->ReadString(section, "Land", "Clear", landBuf, sizeof(landBuf));
    if (!_strcmpi(landBuf, "Rough"))      Land = LandType::Rough;
    else if (!_strcmpi(landBuf, "Road"))  Land = LandType::Road;
    else if (!_strcmpi(landBuf, "Water")) Land = LandType::Water;
    else if (!_strcmpi(landBuf, "Rock"))  Land = LandType::Rock;
    else if (!_strcmpi(landBuf, "Wall"))  Land = LandType::Wall;
    else if (!_strcmpi(landBuf, "Tiberium")) Land = LandType::Tiberium;
    else if (!_strcmpi(landBuf, "Beach")) Land = LandType::Beach;
    else                                  Land = LandType::Clear;

    // Delegate flag parsing to the shared helper.
    Read_TypeFlags(pINI, section);

    return true;
}

// ============================================================================
// SaveToINI
//
//  Emits the common fields back to the INI.  Used by the map editor when
//  saving custom rulesets.
// ============================================================================
bool ObjectTypeClass::SaveToINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    pINI->WriteInteger(section, "Cost",       Cost);
    pINI->WriteInteger(section, "TechLevel",  TechLevel);
    pINI->WriteInteger(section, "BuildLimit", BuildLimit);
    pINI->WriteInteger(section, "Score",      Score);
    pINI->WriteInteger(section, "Sight",      Sight);
    pINI->WriteInteger(section, "Speed",      Speed);
    pINI->WriteInteger(section, "Strength",   MaxStrength);
    pINI->WriteInteger(section, "ROT",        ROT);

    if (UINameLabel[0] != '\0')
        pINI->WriteString(section, "UIName", UINameLabel);
    if (Name[0] != '\0')
        pINI->WriteString(section, "Name", Name);

    pINI->WriteBool(section, "Selectable",     Selectable);
    pINI->WriteBool(section, "LegalTarget",    LegalTarget);
    pINI->WriteBool(section, "Insignificant",  Insignificant);
    pINI->WriteBool(section, "Immune",         Immune);
    pINI->WriteBool(section, "OnFire",         OnFire);
    pINI->WriteBool(section, "Repairable",     Repairable);
    pINI->WriteBool(section, "Unsellable",     Unsellable);
    pINI->WriteBool(section, "Cloakable",      Cloakable);
    pINI->WriteBool(section, "Turret",         TurretEquipped);
    pINI->WriteBool(section, "Stealthy",       IsStealthy);
    pINI->WriteBool(section, "Trainable",      IsTrainable);
    pINI->WriteBool(section, "NotHuman",       IsNotHuman);
    pINI->WriteBool(section, "Theater",        IsTheater);

    const char* layerName = "Ground";
    switch (IdleLayer)
    {
        case Layer::Air:     layerName = "Air";     break;
        case Layer::Top:     layerName = "Top";     break;
        case Layer::Surface: layerName = "Surface"; break;
        default:             layerName = "Ground";  break;
    }
    pINI->WriteString(section, "Layer", layerName);

    const char* landName = "Clear";
    switch (Land)
    {
        case LandType::Rough:    landName = "Rough";    break;
        case LandType::Road:     landName = "Road";     break;
        case LandType::Water:    landName = "Water";    break;
        case LandType::Rock:     landName = "Rock";     break;
        case LandType::Wall:     landName = "Wall";     break;
        case LandType::Tiberium: landName = "Tiberium"; break;
        case LandType::Beach:    landName = "Beach";    break;
        default:                 landName = "Clear";    break;
    }
    pINI->WriteString(section, "Land", landName);

    return true;
}

// ============================================================================
// Read_TypeFlags
//
//  Parses Yes/No-style INI flags into the bool fields.  Centralised so
//  subclasses don't have to repeat the boilerplate.
// ============================================================================
void ObjectTypeClass::Read_TypeFlags(CCINIClass* pINI, const char* pSection)
{
    if (pINI == nullptr || pSection == nullptr)
        return;

    Selectable     = pINI->ReadBool(pSection, "Selectable",     Selectable);
    LegalTarget    = pINI->ReadBool(pSection, "LegalTarget",    LegalTarget);
    Insignificant  = pINI->ReadBool(pSection, "Insignificant",  Insignificant);
    Immune         = pINI->ReadBool(pSection, "Immune",         Immune);
    Repairable     = pINI->ReadBool(pSection, "Repairable",     Repairable);
    Unsellable     = pINI->ReadBool(pSection, "Unsellable",     Unsellable);
    Cloakable      = pINI->ReadBool(pSection, "Cloakable",      Cloakable);
    IsStealthy     = pINI->ReadBool(pSection, "Stealthy",       IsStealthy);
    IsTrainable    = pINI->ReadBool(pSection, "Trainable",      IsTrainable);
    IsNotHuman     = pINI->ReadBool(pSection, "NotHuman",       IsNotHuman);
    IsTheater      = pINI->ReadBool(pSection, "Theater",        IsTheater);
}

// ============================================================================
// ComputeCRC
//
//  Hashes the ObjectTypeClass state into the supplied CRC engine.
// ============================================================================
void ObjectTypeClass::ComputeCRC(CRCEngine& crc) const
{
    AbstractTypeClass::ComputeCRC(crc);

    crc.AddData(&Sight,          sizeof(Sight));
    crc.AddData(&Speed,          sizeof(Speed));
    crc.AddData(&MaxStrength,    sizeof(MaxStrength));
    crc.AddData(&BuildLimit,     sizeof(BuildLimit));
    crc.AddData(&Score,          sizeof(Score));
    crc.AddData(&ROT,            sizeof(ROT));
    crc.AddData(&Selectable,     sizeof(Selectable));
    crc.AddData(&LegalTarget,    sizeof(LegalTarget));
    crc.AddData(&Insignificant,  sizeof(Insignificant));
    crc.AddData(&Immune,         sizeof(Immune));
    crc.AddData(&OnFire,         sizeof(OnFire));
    crc.AddData(&Repairable,     sizeof(Repairable));
    crc.AddData(&Unsellable,     sizeof(Unsellable));
    crc.AddData(&Cloakable,      sizeof(Cloakable));
    crc.AddData(&TurretEquipped, sizeof(TurretEquipped));
    crc.AddData(&IsStealthy,     sizeof(IsStealthy));
    crc.AddData(&IsTrainable,    sizeof(IsTrainable));
    crc.AddData(&IsNotHuman,     sizeof(IsNotHuman));
    crc.AddData(&IsTheater,      sizeof(IsTheater));
    crc.AddData(&IdleLayer,      sizeof(IdleLayer));
    crc.AddData(&Land,           sizeof(Land));
}

int32 ObjectTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// Read_INI / Write_INI
// ============================================================================
bool ObjectTypeClass::Read_INI(CCINIClass* pINI)
{
    return LoadFromINI(pINI);
}

bool ObjectTypeClass::Write_INI(CCINIClass* pINI) const
{
    if (pINI == nullptr)
        return false;
    return const_cast<ObjectTypeClass*>(this)->SaveToINI(pINI);
}

// ============================================================================
// Can_Place_On_Map
//
//  Returns true if an instance of this type can be placed at the supplied
//  coordinate.  The full implementation consults MapClass for cell
//  occupation, foundation, and terrain passability; the standalone build
//  performs a basic bounds + occupancy check.
// ============================================================================
bool ObjectTypeClass::Can_Place_On_Map(const CoordStruct& coord,
                                       HouseClass* /*pOwner*/) const
{
    if (MapClass::Instance == nullptr)
        return false;

    // Convert coord to a cell and check bounds + occupation.
    int32 cellIndex = MapClass::Instance->CoordToCell(coord);
    if (!MapClass::Instance->IsValidCell(cellIndex))
        return false;

    CellClass* cell = MapClass::Instance->GetCellAt(cellIndex);
    if (cell == nullptr)
        return false;

    // Objects cannot be placed on water unless their land type allows it.
    LandType cellLand = MapClass::Instance->GetLandType(
        MapClass::Instance->CellToCellStruct(cellIndex));
    if (cellLand == LandType::Water && Land != LandType::Water)
    {
        return false;
    }

    // Check occupation - the cell must not already be taken.
    if (MapClass::Instance->IsCellOccupied(
            MapClass::Instance->CellToCellStruct(cellIndex)))
    {
        return false;
    }

    return true;
}

// ============================================================================
// Get_Occupy_Bits
//
//  Returns the bitmask of cell-occupation flags this type sets when placed.
//  The full binary uses a 32-bit field; the base implementation returns
//  the "generic object" bit.
// ============================================================================
uint32 ObjectTypeClass::Get_Occupy_Bits() const
{
    return 0x1u;
}

// ============================================================================
// Is_Temple_Of_NOD / Is_Flak / Is_Listed
//
//  Type-flag helpers used by the sidebar and AI.  Defaults are conservative;
//  BuildingTypeClass / InfantryTypeClass override where appropriate.
// ============================================================================
bool ObjectTypeClass::Is_Temple_Of_NOD() const
{
    // The Temple of NOD is the superweapon building for the NOD faction.
    // It appears under two IDs depending on the side / theater:
    //   "NATEMP" - NOD Temple (Temple of NOD)
    //   "GATEMP" - GDI-adjacent / generic Temple variant
    // A case-insensitive comparison against the type's ID field (inherited
    // from AbstractTypeClass) identifies either variant.
    if (this->ID[0] == '\0')
        return false;

    if (!_strcmpi(this->ID, "NATEMP"))
        return true;
    if (!_strcmpi(this->ID, "GATEMP"))
        return true;

    return false;
}

bool ObjectTypeClass::Is_Flak() const
{
    // Flak types are infantry/vehicles with anti-air flak capability.
    // The definitive check uses the Armor field on TechnoTypeClass
    // (ArmorType == Armor::Flak), but ObjectTypeClass does not carry that
    // field.  As a fallback we identify known flak type IDs so the sidebar
    // and AI can classify them without a full TechnoTypeClass override.
    if (this->ID[0] == '\0')
        return false;

    // Known flak-type IDs in the standard rules:
    //   "FLAK"  - Flak Trooper (infantry, Soviet anti-air)
    //   "FLAKT" - Flak Track (vehicle, Soviet anti-air transport)
    if (!_strcmpi(this->ID, "FLAK"))
        return true;
    if (!_strcmpi(this->ID, "FLAKT"))
        return true;

    return false;
}

bool ObjectTypeClass::Is_Listed() const
{
    // A type is "listed" in the sidebar if it has a valid tech level and
    // is buildable.  TechLevel == -1 means "not buildable".
    return (TechLevel >= 0);
}

// ============================================================================
// Get_Max_Pips
//
//  Returns the number of pips drawn in the sidebar for this type.  Defaults
//  to 1 (single pip for build progress).  BuildingTypeClass overrides this
//  for factories that show production queues.
// ============================================================================
int32 ObjectTypeClass::Get_Max_Pips() const
{
    return 1;
}

// ============================================================================
// Create_One_Of
//
//  ABSTRACT factory hook.  Allocates a concrete instance of this type owned
//  by the supplied house and registers it with the map.
//
//  ObjectTypeClass itself is abstract - it describes the fields common to
//  every placeable type but does not know which concrete ObjectClass
//  subclass to instantiate.  The base therefore returns nullptr and each
//  derived type class overrides this to call its own construction path:
//
//    BuildingTypeClass::Create_One_Of  -> new BuildingClass(this, pOwner)
//    UnitTypeClass::Create_One_Of      -> new UnitClass(this, pOwner)
//    InfantryTypeClass::Create_One_Of  -> new InfantryClass(this, pOwner)
//    AircraftTypeClass::Create_One_Of  -> new AircraftClass(this, pOwner)
//
//  Callers (the sidebar / factory / AI build logic) always invoke this
//  polymorphically through an ObjectTypeClass pointer, so the correct
//  concrete instance is produced without RTTI checks.
// ============================================================================
ObjectClass* ObjectTypeClass::Create_One_Of(HouseClass* /*pOwner*/)
{
    // ObjectTypeClass is abstract - it describes the fields common to every
    // placeable type but does not know which concrete ObjectClass subclass
    // to instantiate.  Each derived type class overrides this:
    //
    //    BuildingTypeClass::Create_One_Of  -> new BuildingClass(this, pOwner)
    //    UnitTypeClass::Create_One_Of      -> new UnitClass(this, pOwner)
    //    InfantryTypeClass::Create_One_Of  -> new InfantryClass(this, pOwner)
    //    AircraftTypeClass::Create_One_Of  -> new AircraftClass(this, pOwner)
    //
    // The base validates the type state before returning nullptr so callers
    // can distinguish "abstract type" from "invalid type definition".
    if (this->ID[0] == '\0')
        return nullptr;

    // No concrete instance can be created from the abstract base.
    return nullptr;
}

// ============================================================================
// Get_Cameo_Data
//
//  Returns the SHP image used as the sidebar cameo (build-button icon) for
//  this type.
//
//  Header check: ObjectTypeClass does NOT declare a CameoData / CameoShape
//  or Image field.  Those art references (Cameo, CameoShape, ImageFile,
//  ImageShape) are introduced on the derived TechnoTypeClass, which also
//  overrides Resolve_SHP_References to bind them from the art INI / mix
//  filesystem.  Because the base has no art state to consult, it returns
//  nullptr.
//
//  The intended resolution order, performed by TechnoTypeClass and its
//  descendants (e.g. InfantryTypeClass::Get_Cameo_Data), is:
//    1. Return the cached CameoShape if already resolved.
//    2. Otherwise fall back to ObjectTypeClass::Get_Cameo_Data() (here),
//       which yields nullptr so the sidebar can draw a placeholder.
//
//  Subclasses that DO carry art state override this; the base is a correct
//  nullptr fallback.
// ============================================================================
SHPStruct* ObjectTypeClass::Get_Cameo_Data() const
{
    // Returns the SHP image used as the sidebar cameo (build-button icon).
    // The CameoShape pointer is introduced on TechnoTypeClass (along with
    // the Cameo / ImageFile name buffers); ObjectTypeClass has no art state
    // to consult.  The base validates the type ID and returns nullptr so
    // the sidebar draws a placeholder icon.
    //
    // Subclass resolution order (TechnoTypeClass::Get_Cameo_Data):
    //   1. Return the cached CameoShape if already resolved.
    //   2. Otherwise fall back to this base, which yields nullptr.
    if (this->ID[0] == '\0')
        return nullptr;

    // No cameo art at this level.
    return nullptr;
}

// ============================================================================
// Resolve_SHP_References
//
//  Called after the art INI has been loaded.  Binds SHP shape file
//  references to their loaded SHPStruct pointers.  The base ObjectTypeClass
//  has no art fields (CameoShape / ImageShape are introduced on
//  TechnoTypeClass), so this implementation validates the type state and
//  returns.  Subclasses override this to resolve their specific art:
//
//    TechnoTypeClass  - resolves CameoShape and ImageShape from the art INI
//    BuildingTypeClass- resolves building SHP art per theater
//    TerrainTypeClass - resolves terrain SHP art
//
//  The art INI section name is derived from the type's ID field.
// ============================================================================
void ObjectTypeClass::Resolve_SHP_References()
{
    // ObjectTypeClass carries no SHP pointers at this level.  Validate
    // that the type has a valid ID for subclasses that will chain this
    // call before performing their own resolution.
    if (this->ID[0] == '\0')
        return;

    // The IsTheater flag indicates whether the art varies by theater.
    // Theater-specific art resolution is performed by the subclass override
    // via LoadTheaterSpecificArt(); the base has nothing to resolve.
}
