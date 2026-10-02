#include <Abstract/BuildingTypeClass.h>
#include <Audio/VocClass.h>
#include <Abstract/UnitTypeClass.h>
#include <Abstract/OverlayTypeClass.h>
#include <Abstract/InfantryTypeClass.h>
#include <Objects/IsometricTile.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>
#include <Map/MapClass.h>
#include <Scenario/ScenarioClass.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>

// ============================================================================
// BuildingTypeClass.cpp
//
//  BuildingTypeClass is the type descriptor for every building - construction
//  yards, power plants, factories, base defenses, walls, etc.  It inherits
//  the techno-type fields (armor, weapons, sight) and adds:
//
//    * Foundation (width x height in cells)
//    * Power output / drain
//    * Bib / spotlight / helipad / dock art flags
//    * Factory / barracks / warfactory / airport / naval yard classification
//    * Wall / gate / ore-storage classification
//    * SuperWeapon slot indices
//    * Garrison / occupy weapon configuration
//    * Adjacent-build rules
//
//  This file implements:
//    * Static Array plumbing
//    * Constructor / destructor
//    * LoadFromINI / SaveToINI
//    * ComputeCRC / GetCRC
//    * Geometry helpers (Get_Width / Get_Height / Get_Occupy_Rect)
//    * Power helpers (Get_Power_Output / Get_Power_Drain)
//    * Storage helper (Get_Storage_Capacity)
//    * Factory classification (Get_Factory_Type, Is_Factory, etc.)
//    * Find_Exit_Cell - locates a clear cell adjacent to the foundation
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<BuildingTypeClass*>* BuildingTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================
void BuildingTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<BuildingTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<BuildingTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<BuildingTypeClass*>();
    }
}

void BuildingTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<BuildingTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

BuildingTypeClass* BuildingTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        BuildingTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

BuildingTypeClass* BuildingTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

// BuildingTypeClass_FindIndex: the ordinal of a building type inside the
// registry, or -1 when the name is unknown.  This is what the base-node
// reader stores when it resolves a "%03d" entry's type field.
int32 BuildingTypeClass::FindIndex(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return -1;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        BuildingTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return i;
    }
    return -1;
}

int32 BuildingTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

// BuildingTypeClass_ToTile (asm 0x465CC0): walks every registered building
// type and resolves its "ToTile" key inside RULES_INI against the tile-set
// registry.  A type that names no tile - or names one that is not registered
// - keeps whatever pointer it already held; the write only happens on a hit.
void BuildingTypeClass::ToTile()
{
    if (Array == nullptr)
        return;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        BuildingTypeClass* pType = Array->Items[i];
        if (pType == nullptr)
            continue;

        char buffer[0x3C];
        buffer[0] = '\0';

        CCINIClass* pRules = CCINIClass::INI_Rules;
        if (pRules == nullptr)
            return;

        pRules->ReadString(pType->ID, "ToTile", "", buffer, sizeof(buffer));

        const int32 idx = IsometricTileType::FindIndex(buffer);
        if (idx < 0 || idx >= IsometricTileType::GetTileSetCount())
            continue;

        pType->TileToUse = IsometricTileType::GetTileSet(idx);
    }
}

void BuildingTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        BuildingTypeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

// ============================================================================
// Constructor
// ============================================================================

BuildingTypeClass::BuildingTypeClass() noexcept
    : TechnoTypeClass(noinit)
{
    BuildingFoundation = 0x0101;  // 1x1 default foundation (packed B=1, W=1)
    Height             = 0;
    Power              = 0;
    PowerDrain         = 0;
    Bib                = 0;

    CanBeSold_         = true;
    IsUndeployable_    = false;
    IsSimpleDeployer_  = false;
    IsFirebase_        = false;
    IsFactory_         = false;
    HasSpotlight       = false;
    HasBib             = false;
    HasHelipad         = false;
    HasDock            = false;
    IsBase             = false;
    IsWall_            = false;
    IsGate             = false;
    IsOreRefinery      = false;
    IsOreStorage       = false;
    IsWeaponsFactory   = false;
    IsBarracks         = false;
    IsRadar            = false;
    IsTech             = false;
    IsSecretLab        = false;
    IsConstructionYard = false;
    IsAirport          = false;
    IsWarfactory       = false;
    IsNavalYard        = false;
    IsRepairPad        = false;
    IsMissileSilo      = false;
    IsPowered          = false;
    IsCanC4            = false;
    IsCanBeOccupied    = false;
    IsCanBeDriven      = false;
    IsCanBeCaptured    = true;
    IsCanBeRepaired_   = true;
    IsCanBeSold_       = true;
    IsCanBePowered     = false;
    IsCanBeDestroyed   = true;
    IsCanBeDamaged     = true;
    IsCanBeInfiltrated = false;
    IsCanBeSpied       = false;
    IsCanBeSabotaged   = false;
    IsCanBeStolen      = false;
    IsCanBeHijacked    = false;

    Storage            = 0;
    BuildingAnimCount  = 0;
    OccupyCount        = 0;
    NumberOfDocks      = 0;
    Adjacent           = 0;
    MaxWalls           = 0;
    SuperWeapon        = -1;
    SuperWeapon2       = -1;
    WeaponCount        = 0;
    OccupyWeaponCount  = 0;
    EliteOccupyWeaponCount = 0;

    std::memset(Weapons,             0, sizeof(Weapons));
    std::memset(OccupyWeapons,       0, sizeof(OccupyWeapons));
    std::memset(EliteOccupyWeapons,  0, sizeof(EliteOccupyWeapons));

    HasSuperWeapon     = false;
    HasSuperWeapon2    = false;
    IsPlug             = false;
    IsDrain            = false;
    IsDrainable        = false;
    IsRig              = false;
    IsRigOwner         = false;
    IsResource         = false;
    IsResourceGatherer = false;
    IsBombable         = false;
    IsAutoFire         = false;
    IsGuardRange       = false;
    IsAggressive       = false;
    IsUndeployableMember = false;
    IsSellable         = true;
    IsRepairable       = true;
    IsUnsellable       = false;
    IsUngarrisonable   = false;

    IsNaval            = false;
    IsLand             = true;
    IsAir              = false;
    IsOrganic          = false;
    IsNeutral          = false;
    IsInfiltratable    = false;
    IsStealthy         = false;
    IsHealable         = false;
    IsTilter           = false;
    IsToProtect        = false;
    IsNominal          = false;
    IsRadarInvisible   = false;
    IsDontScore        = false;
    IsNoThreat         = false;
    IsSensorsSight     = false;
    IsHunterSeeker     = false;
    IsIvan             = false;
    IsLeader           = false;
    IsCarryall         = false;
    IsTrain            = false;
    IsConsideredAircraft = false;
    IsConsideredVehicle  = true;

    IsPowered_         = false;
    IsScanner          = false;
    IsSensor           = false;
    IsDetector         = false;
    IsSensors          = false;
    IsPreventAttackMove = false;

    // Mirror flags - kept in sync with the primary set.
    IsNaval_           = IsNaval;
    IsLand_            = IsLand;
    IsAir_             = IsAir;
    IsOrganic_         = IsOrganic;
    IsNeutral_         = IsNeutral;
    IsInfiltratable_   = IsInfiltratable;
    IsStealthy_        = IsStealthy;
    IsHealable_        = IsHealable;
    IsTilter_          = IsTilter;
    IsToProtect_       = IsToProtect;
    IsNominal_         = IsNominal;
    IsRadarInvisible_  = IsRadarInvisible;
    IsDontScore_       = IsDontScore;
    IsNoThreat_        = IsNoThreat;
    IsSensorsSight_    = IsSensorsSight;
    IsHunterSeeker_    = IsHunterSeeker;
    IsIvan_            = IsIvan;
    IsLeader_          = IsLeader;

    IsDock             = HasDock;
    IsHelipad          = HasHelipad;
    IsHasBib           = HasBib;
    IsHasSpotlight     = HasSpotlight;
    IsBase_            = IsBase;
    IsGate_            = IsGate;
    IsOreRefinery_     = IsOreRefinery;
    IsOreStorage_      = IsOreStorage;
    IsWeaponsFactory_  = IsWeaponsFactory;
    IsBarracks_        = IsBarracks;
    IsRadar_           = IsRadar;
    IsTech_            = IsTech;
    IsSecretLab_       = IsSecretLab;
    IsConstructionYard_ = IsConstructionYard;
    IsAirport_         = IsAirport;
    IsWarfactory_      = IsWarfactory;
    IsNavalYard_       = IsNavalYard;
    IsRepairPad_       = IsRepairPad;
    IsMissileSilo_     = IsMissileSilo;
    IsPlug_            = IsPlug;
    IsDrain_           = IsDrain;
    IsDrainable_       = IsDrainable;
    IsRig_             = IsRig;
    IsRigOwner_        = IsRigOwner;
    IsResource_        = IsResource;
    IsResourceGatherer_ = IsResourceGatherer;
    IsBombable_        = IsBombable;
    IsAutoFire_        = IsAutoFire;
    IsGuardRange_      = IsGuardRange;
    IsAggressive_      = IsAggressive;
    IsUndeployable__   = IsUndeployable_;
    IsSellable_        = IsSellable;
    IsRepairable_      = IsRepairable;
    IsUnsellable_      = IsUnsellable;
    IsUngarrisonable_  = IsUngarrisonable;

    ThreatPosedValue_  = 0.0f;
    DeathWeaponIndex_  = -1;
    BuildingAnimCount_ = 0;
    OccupyCount_       = 0;
    NumberOfDocks_     = 0;
    Adjacent_          = 0;
    MaxWalls_          = 0;

    IsPowered2         = false;
    IsPlug2            = false;
    IsDrain2           = false;
    IsDrainable2       = false;
    IsRig2             = false;
    IsRigOwner2        = false;
    IsResource2        = false;
    IsResourceGatherer2 = false;

    std::memset(padding_BuildingType, 0, sizeof(padding_BuildingType));

    // Buildings are not human.
    IsNotHuman = true;
    IsTrainable_ = false;
    IsBuildable_ = true;
}

// ============================================================================
// Destructor
// ============================================================================

BuildingTypeClass::~BuildingTypeClass()
{
    // No heap resources to release at this level.
}

// ============================================================================
// RTTI / size / ID
// ============================================================================

AbstractType BuildingTypeClass::GetAbstractDerivationID() const
{
    return AbstractType::BuildingType;
}

bool BuildingTypeClass::HasThisID(const char* pID) const
{
    if (pID == nullptr)
        return false;
    return _strcmpi(this->ID, pID) == 0;
}

int32 BuildingTypeClass::Size() const
{
    return sizeof(BuildingTypeClass);
}

AbstractType BuildingTypeClass::GetClassID() const
{
    return AbstractType::BuildingType;
}

const char* BuildingTypeClass::get_ID() const
{
    return this->ID;
}

const wchar_t* BuildingTypeClass::GetUIName() const
{
    return this->UIName;
}

// ============================================================================
// Building classification helpers
// ============================================================================

bool BuildingTypeClass::IsUndeployable() const    { return IsUndeployable_; }
bool BuildingTypeClass::CanBeSold() const         { return CanBeSold_ && !IsUnsellable; }
bool BuildingTypeClass::CanBeRepaired() const     { return IsRepairable && IsCanBeRepaired_; }
bool BuildingTypeClass::IsSimpleDeployer() const  { return IsSimpleDeployer_; }
bool BuildingTypeClass::IsFirebase() const        { return IsFirebase_; }
bool BuildingTypeClass::IsFactory() const         { return IsFactory_; }
bool BuildingTypeClass::IsWall() const            { return IsWall_; }
bool BuildingTypeClass::IsTiberiumStorage() const { return IsOreStorage; }

bool BuildingTypeClass::IsPowerPlant() const
{
    // A power plant produces more power than it drains.
    return Power > 0 && PowerDrain == 0;
}

// ============================================================================
// Geometry helpers
// ============================================================================

int32 BuildingTypeClass::Get_Width() const
{
    // Foundation is packed as a single 32-bit value: high 16 bits = width,
    // low 16 bits = height.  The original binary stores it as two int16
    // values unioned into one int32.
    return (BuildingFoundation >> 16) & 0xFFFF;
}

int32 BuildingTypeClass::Get_Height() const
{
    return BuildingFoundation & 0xFFFF;
}

RectangleStruct BuildingTypeClass::Get_Occupy_Rect() const
{
    return RectangleStruct(0, 0, Get_Width(), Get_Height());
}

int32 BuildingTypeClass::Get_Power_Output() const
{
    // Net power = output - drain.  A negative result indicates the building
    // is a net consumer.
    return Power - PowerDrain;
}

int32 BuildingTypeClass::Get_Power_Drain() const
{
    return PowerDrain;
}

int32 BuildingTypeClass::Get_Storage_Capacity() const
{
    return Storage;
}

AbstractType BuildingTypeClass::Get_Factory_Type() const
{
    if (IsWeaponsFactory || IsWarfactory) return AbstractType::Unit;
    if (IsBarracks)                       return AbstractType::Infantry;
    if (IsAirport || HasHelipad)          return AbstractType::Aircraft;
    if (IsNavalYard)                      return AbstractType::Unit;
    if (IsFactory_)                       return AbstractType::Unit;
    return AbstractType::None;
}

// ============================================================================
// Find_Exit_Cell
//
//  Locates a clear cell adjacent to the building's foundation where a
//  produced unit can be unloaded.  The full implementation walks the
//  surrounding cells and picks the first unoccupied one; the standalone
//  build does the same via MapClass.
// ============================================================================

int32 BuildingTypeClass::Find_Exit_Cell(const CoordStruct& baseCoord) const
{
    if (MapClass::Instance == nullptr)
        return -1;

    int32 baseCell = MapClass::Instance->CoordToCell(baseCoord);
    if (!MapClass::Instance->IsValidCell(baseCell))
        return -1;

    int32 width  = Get_Width();
    int32 height = Get_Height();
    int32 baseX  = MapClass::Instance->GetCellX(baseCell);
    int32 baseY  = MapClass::Instance->GetCellY(baseCell);

    // Search the perimeter of the foundation for a clear cell.  Try the
    // four cardinal directions first, then the diagonals.
    static const int32 offsets[8][2] = {
        { 0, -1}, { 0,  1}, {-1,  0}, { 1,  0},
        {-1, -1}, {-1,  1}, { 1, -1}, { 1,  1}
    };

    for (int32 side = 0; side < 8; ++side)
    {
        int32 exitX = baseX + offsets[side][0] * width;
        int32 exitY = baseY + offsets[side][1] * height;
        if (!MapClass::Instance->IsValidCell(exitX, exitY))
            continue;

        CellStruct cell;
        cell.X = static_cast<int16>(exitX);
        cell.Y = static_cast<int16>(exitY);
        if (!MapClass::Instance->IsCellOccupied(cell))
        {
            return MapClass::Instance->XYToCell(exitX, exitY);
        }
    }

    return -1;
}

// ============================================================================
// Load2DArt
//
//   The second half of the art load: frames whose filename is resolved
//   against the active theatre.  Each key is read from the art INI section
//   named by ImageTag and, when it produced a non-empty value, the
//   theatre-qualified ".SHP" is stored in the corresponding slot.
// ============================================================================
void BuildingTypeClass::Load2DArt(int32 idxTheater)
{
    CCINIClass* pArt = &CCINIClass::INI_Art;
    if (pArt == nullptr)
        return;

    const char* section = this->Image;
    if (section[0] == '\0')
        section = this->ID;
    if (section[0] == '\0')
        return;

    static const char* const keys[] = {
        "DeployingAnim",
        "RoofDeployingAnim",
        "DoorAnim",
        "UnderDoorAnim",
        "UnderRoofDoorAnim",
        "Rubble",
        "SpecialZOverlay",
    };

    char* const slots[] = {
        DeployingAnim,
        RoofDeployingAnim,
        DoorAnim,
        UnderDoorAnim,
        UnderRoofDoorAnim,
        Rubble,
        SpecialZOverlay,
    };

    for (int32 i = 0; i < static_cast<int32>(sizeof(keys) / sizeof(keys[0])); ++i)
    {
        char name[0x40];
        name[0] = '\0';
        pArt->ReadString(section, keys[i], "", name, sizeof(name));
        if (name[0] == '\0')
            continue;

        std::strncpy(slots[i], name, 0x1F);
        slots[i][0x1F] = '\0';
    }
}

// ============================================================================
// LoadFromINI
// ============================================================================

bool BuildingTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // Chain the parent first so common techno fields are loaded.
    TechnoTypeClass::LoadFromINI(pINI);

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // ------------------------------------------------------------------
    // Foundation - parsed as "WxH" string (e.g. "2x3")
    // ------------------------------------------------------------------
    char foundBuf[32];
    pINI->ReadString(section, "Foundation", "1x1", foundBuf, sizeof(foundBuf));
    int32 fw = 1, fh = 1;
    // Parse "WxH" - simple parser since the format is fixed.
    int32 idx = 0;
    int32 val = 0;
    bool parsedW = false;
    while (foundBuf[idx] != '\0' && idx < 16)
    {
        char c = foundBuf[idx];
        if (c >= '0' && c <= '9')
        {
            val = val * 10 + (c - '0');
        }
        else if (c == 'x' || c == 'X')
        {
            if (!parsedW) { fw = val; val = 0; parsedW = true; }
        }
        else
        {
            break;
        }
        ++idx;
    }
    if (parsedW) fh = val;
    BuildingFoundation = (fw << 16) | (fh & 0xFFFF);

    Height = pINI->ReadInteger(section, "Height", Height);

    // ------------------------------------------------------------------
    // Power
    // ------------------------------------------------------------------
    Power      = pINI->ReadInteger(section, "Power",    Power);
    IsPowered  = pINI->ReadBool(section,   "Powered",  IsPowered);
    IsPowered_ = IsPowered;

    // ------------------------------------------------------------------
    // Storage / economy
    // ------------------------------------------------------------------
    Storage   = pINI->ReadInteger(section, "Storage",  Storage);
    Adjacent  = pINI->ReadInteger(section, "Adjacent", Adjacent);

    // ------------------------------------------------------------------
    // Docks / helipad
    // ------------------------------------------------------------------
    NumberOfDocks = pINI->ReadInteger(section, "NumberOfDocks", NumberOfDocks);
    HasDock       = pINI->ReadBool(section,    "Dock",          HasDock);
    HasHelipad    = pINI->ReadBool(section,    "Helipad",       HasHelipad);
    IsDock        = HasDock;
    IsHelipad     = HasHelipad;

    // ------------------------------------------------------------------
    // Bib / spotlight
    // ------------------------------------------------------------------
    HasBib       = pINI->ReadBool(section, "Bib",       HasBib);
    Bib          = pINI->ReadInteger(section, "BibShape", Bib);
    IsHasBib       = HasBib;
    IsHasSpotlight = HasSpotlight;

    // ------------------------------------------------------------------
    // Factory / building-type classification
    // ------------------------------------------------------------------
    IsFactory_         = pINI->ReadBool(section, "Factory",          IsFactory_);
    IsWeaponsFactory   = pINI->ReadBool(section, "WeaponsFactory",   IsWeaponsFactory);
    IsConstructionYard = pINI->ReadBool(section, "ConstructionYard", IsConstructionYard);
    IsRadar            = pINI->ReadBool(section, "Radar",            IsRadar);
    IsTech             = pINI->ReadBool(section, "Tech",             IsTech);
    IsSecretLab        = pINI->ReadBool(section, "SecretLab",        IsSecretLab);
    IsBase             = pINI->ReadBool(section, "Base",             IsBase);
    IsWall_            = pINI->ReadBool(section, "Wall",             IsWall_);
    IsGate             = pINI->ReadBool(section, "Gate",             IsGate);

    // Mirror flags - kept in sync with the primary set.
    IsBase_             = IsBase;
    IsGate_             = IsGate;
    IsOreRefinery_      = IsOreRefinery;
    IsOreStorage_       = IsOreStorage;
    IsWeaponsFactory_   = IsWeaponsFactory;
    IsBarracks_         = IsBarracks;
    IsRadar_            = IsRadar;
    IsTech_             = IsTech;
    IsSecretLab_        = IsSecretLab;
    IsConstructionYard_ = IsConstructionYard;
    IsAirport_          = IsAirport;
    IsWarfactory_       = IsWarfactory;
    IsNavalYard_        = IsNavalYard;
    IsRepairPad_        = IsRepairPad;
    IsMissileSilo_      = IsMissileSilo;

    // ------------------------------------------------------------------
    // Super weapons
    // ------------------------------------------------------------------
    SuperWeapon     = pINI->ReadInteger(section, "SuperWeapon",  SuperWeapon);
    SuperWeapon2    = pINI->ReadInteger(section, "SuperWeapon2", SuperWeapon2);
    HasSuperWeapon  = (SuperWeapon  >= 0);
    HasSuperWeapon2 = (SuperWeapon2 >= 0);

    // ------------------------------------------------------------------
    // Weapons (buildings have up to two weapon slots)
    // ------------------------------------------------------------------
    WeaponCount = pINI->ReadInteger(section, "WeaponCount", WeaponCount);
    if (WeaponCount < 0) WeaponCount = 0;
    if (WeaponCount > 2) WeaponCount = 2;
    for (int32 i = 0; i < WeaponCount; ++i)
    {
        std::memset(&Weapons[i], 0, sizeof(WeaponStruct));
    }
    DeathWeaponIndex_ = pINI->ReadInteger(section, "DeathWeapon", -1);

    // ------------------------------------------------------------------
    // Garrison / occupy weapons
    // ------------------------------------------------------------------
    if (OccupyWeaponCount < 0)      OccupyWeaponCount = 0;
    if (OccupyWeaponCount > 2)      OccupyWeaponCount = 2;
    if (EliteOccupyWeaponCount < 0) EliteOccupyWeaponCount = 0;
    if (EliteOccupyWeaponCount > 2) EliteOccupyWeaponCount = 2;
    OccupyCount_ = OccupyCount;

    // ------------------------------------------------------------------
    // Build / sell flags
    // ------------------------------------------------------------------
    IsCanBeSold_     = CanBeSold_;
    IsSellable       = CanBeSold_;
    IsSellable_      = CanBeSold_;
    IsUnsellable     = pINI->ReadBool(section, "Unsellable",     IsUnsellable);
    IsUnsellable_    = IsUnsellable;
    IsRepairable     = pINI->ReadBool(section, "Repairable",     IsRepairable);
    IsRepairable_    = IsRepairable;
    IsCanBeRepaired_ = IsRepairable;
    IsUngarrisonable_ = IsUngarrisonable;

    // ------------------------------------------------------------------
    // CanBeXxx interaction flags
    // ------------------------------------------------------------------
    IsCanC4            = pINI->ReadBool(section, "CanC4",          IsCanC4);
    IsCanBeOccupied    = pINI->ReadBool(section, "CanBeOccupied",  IsCanBeOccupied);
    IsCanBeCaptured    = pINI->ReadBool(section, "Capturable",  IsCanBeCaptured);
    IsCanBePowered     = pINI->ReadBool(section, "Powered",   IsCanBePowered);
    IsCanBeSpied       = pINI->ReadBool(section, "Spyable",     IsCanBeSpied);

    // ------------------------------------------------------------------
    // Plug / drain / rig
    // ------------------------------------------------------------------
    IsDrainable        = pINI->ReadBool(section, "Drainable",        IsDrainable);
    IsResource         = pINI->ReadBool(section, "Resource",         IsResource);
    IsResourceGatherer = pINI->ReadBool(section, "ResourceGatherer", IsResourceGatherer);

    IsPlug_             = IsPlug;
    IsDrain_            = IsDrain;
    IsDrainable_        = IsDrainable;
    IsRig_              = IsRig;
    IsRigOwner_         = IsRigOwner;
    IsResource_         = IsResource;
    IsResourceGatherer_ = IsResourceGatherer;

    // ------------------------------------------------------------------
    // Combat flags
    // ------------------------------------------------------------------
    IsBombable    = pINI->ReadBool(section, "Bombable",    IsBombable);
    IsGuardRange  = pINI->ReadBool(section, "GuardRange",  IsGuardRange);
    IsAggressive  = pINI->ReadBool(section, "Aggressive",  IsAggressive);

    IsBombable_    = IsBombable;
    IsAutoFire_    = IsAutoFire;
    IsGuardRange_  = IsGuardRange;
    IsAggressive_  = IsAggressive;

    ThreatPosedValue_ = pINI->ReadFloat(section, "ThreatPosed", ThreatPosedValue_);

    // ------------------------------------------------------------------
    // Misc classification flags
    // ------------------------------------------------------------------
    IsNaval            = pINI->ReadBool(section, "Naval",            IsNaval);
    IsLand             = pINI->ReadBool(section, "Land",             IsLand);
    IsNeutral          = pINI->ReadBool(section, "Neutral",          IsNeutral);
    IsToProtect        = pINI->ReadBool(section, "ToProtect",        IsToProtect);
    IsNominal          = pINI->ReadBool(section, "Nominal",          IsNominal);
    IsRadarInvisible   = pINI->ReadBool(section, "RadarInvisible",   IsRadarInvisible);
    IsDontScore        = pINI->ReadBool(section, "DontScore",        IsDontScore);
    IsNoThreat         = pINI->ReadBool(section, "NoThreat",         IsNoThreat);
    IsSensorsSight     = pINI->ReadBool(section, "SensorsSight",     IsSensorsSight);
    IsHunterSeeker     = pINI->ReadBool(section, "HunterSeeker",     IsHunterSeeker);
    IsIvan             = pINI->ReadBool(section, "Ivan",             IsIvan);
    IsCarryall         = pINI->ReadBool(section, "Carryall",         IsCarryall);
    IsConsideredAircraft = pINI->ReadBool(section, "ConsideredAircraft", IsConsideredAircraft);

    // Mirror flags.
    IsNaval_           = IsNaval;
    IsLand_            = IsLand;
    IsNeutral_         = IsNeutral;
    IsInfiltratable_   = IsInfiltratable;
    IsStealthy_        = IsStealthy;
    IsHealable_        = IsHealable;
    IsTilter_          = IsTilter;
    IsToProtect_       = IsToProtect;
    IsNominal_         = IsNominal;
    IsRadarInvisible_  = IsRadarInvisible;
    IsDontScore_       = IsDontScore;
    IsNoThreat_        = IsNoThreat;
    IsSensorsSight_    = IsSensorsSight;
    IsHunterSeeker_    = IsHunterSeeker;
    IsIvan_            = IsIvan;
    IsLeader_          = IsLeader;

    IsSensors            = pINI->ReadBool(section, "Sensors",  IsSensors);
    IsPreventAttackMove  = pINI->ReadBool(section, "PreventAttackMove", IsPreventAttackMove);

    // ------------------------------------------------------------------
    // Undeployable / simple-deployer / firebase
    // ------------------------------------------------------------------
    IsSimpleDeployer_   = pINI->ReadBool(section, "Deployer", IsSimpleDeployer_);
    IsUndeployable__    = IsUndeployable_;
    IsUndeployableMember= IsUndeployable_;

    // Building animation count - read from art INI normally; allow override.
    BuildingAnimCount_ = BuildingAnimCount;


    // generated-ini-reads
    // ------------------------------------------------------------------
    // Full key set - every field keeps its current value when the key
    // is absent, so partially specified sections stay valid.
    // ------------------------------------------------------------------
    CCINIClass* pArt = &CCINIClass::INI_Art;
    if (pArt == nullptr)
        pArt = pINI;

    BuildCatValue = pINI->GetBuildCat(section, "BuildCat", BuildCatValue);
    AntiInfantryValue = pINI->ReadInteger(section, "AntiInfantryValue", AntiInfantryValue);
    AntiArmorValue = pINI->ReadInteger(section, "AntiArmorValue", AntiArmorValue);
    AntiAirValue = pINI->ReadInteger(section, "AntiAirValue", AntiAirValue);
    HasSpotlight = pINI->ReadBool(section, "HasSpotlight", HasSpotlight);
    pINI->Get3Integers(section, "HalfDamageSmokeLocation1", HalfDamageSmokeLocation1);
    pINI->Get3Integers(section, "HalfDamageSmokeLocation2", HalfDamageSmokeLocation2);
    Radar = pINI->ReadBool(section, "Radar", Radar);
    SpySat = pINI->ReadBool(section, "SpySat", SpySat);
    WaterBound = pINI->ReadBool(section, "WaterBound", WaterBound);
    Adjacent = pINI->ReadInteger(section, "Adjacent", Adjacent);
    Capturable = pINI->ReadBool(section, "Capturable", Capturable);
    Powered = pINI->ReadBool(section, "Powered", Powered);
    PoweredSpecial = pINI->ReadBool(section, "PoweredSpecial", PoweredSpecial);
    Overpowerable = pINI->ReadBool(section, "Overpowerable", Overpowerable);
    Spyable = pINI->ReadBool(section, "Spyable", Spyable);
    CanC4 = pINI->ReadBool(section, "CanC4", CanC4);
    WantsExtraSpace = pINI->ReadBool(section, "WantsExtraSpace", WantsExtraSpace);
    Bib = pINI->ReadBool(section, "Bib", Bib);
    Unsellable = pINI->ReadBool(section, "Unsellable", Unsellable);
    ClickRepairable = pINI->ReadBool(section, "ClickRepairable", ClickRepairable);
    CanBeOccupied = pINI->ReadBool(section, "CanBeOccupied", CanBeOccupied);
    CanOccupyFire = pINI->ReadBool(section, "CanOccupyFire", CanOccupyFire);
    ShowOccupantPips = pINI->ReadBool(section, "ShowOccupantPips", ShowOccupantPips);
    MaxNumberOccupants = pINI->ReadInteger(section, "MaxNumberOccupants", MaxNumberOccupants);
    NumberImpassableRows = pINI->ReadInteger(section, "NumberImpassableRows", NumberImpassableRows);
    ProduceCashStartup = pINI->ReadInteger(section, "ProduceCashStartup", ProduceCashStartup);
    ProduceCashAmount = pINI->ReadInteger(section, "ProduceCashAmount", ProduceCashAmount);
    ProduceCashDelay = pINI->ReadInteger(section, "ProduceCashDelay", ProduceCashDelay);
    InfantryGainSelfHeal = pINI->ReadInteger(section, "InfantryGainSelfHeal", InfantryGainSelfHeal);
    UnitsGainSelfHeal = pINI->ReadInteger(section, "UnitsGainSelfHeal", UnitsGainSelfHeal);
    RefinerySmokeFrames = pINI->ReadInteger(section, "RefinerySmokeFrames", RefinerySmokeFrames);
    BaseNormal = pINI->ReadBool(section, "BaseNormal", BaseNormal);
    EligibileForAllyBuilding = pINI->ReadBool(section, "EligibileForAllyBuilding", EligibileForAllyBuilding);
    EligibleForDelayKill = pINI->ReadBool(section, "EligibleForDelayKill", EligibleForDelayKill);
    NeedsEngineer = pINI->ReadBool(section, "NeedsEngineer", NeedsEngineer);
    ProtectWithWall = pINI->ReadBool(section, "ProtectWithWall", ProtectWithWall);
    Wall = pINI->ReadBool(section, "Wall", Wall);
    Weeder = pINI->ReadBool(section, "Weeder", Weeder);
    Helipad = pINI->ReadBool(section, "Helipad", Helipad);
    OrePurifier = pINI->ReadBool(section, "OrePurifier", OrePurifier);
    FactoryPlant = pINI->ReadBool(section, "FactoryPlant", FactoryPlant);
    { char _buf[0x40]; if (pINI->ReadString(section, "FreeUnit", "", _buf, sizeof(_buf)) > 0) { UnitTypeClass* _p = UnitTypeClass::FindOrAllocate(_buf); if (_p) FreeUnit = _p; } }
    HoverPad = pINI->ReadBool(section, "HoverPad", HoverPad);
    IsTemple = pINI->ReadBool(section, "IsTemple", IsTemple);
    IsPlug = pINI->ReadBool(section, "IsPlug", IsPlug);
    { char _buf[0x40]; if (pINI->ReadString(section, "SecretInfantry", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) SecretInfantry = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "SecretUnit", "", _buf, sizeof(_buf)) > 0) { UnitTypeClass* _p = UnitTypeClass::FindOrAllocate(_buf); if (_p) SecretUnit = _p; } }
    InfantryCostBonus = pINI->ReadFixed(section, "InfantryCostBonus", InfantryCostBonus);
    UnitsCostBonus = pINI->ReadFixed(section, "UnitsCostBonus", UnitsCostBonus);
    AircraftCostBonus = pINI->ReadFixed(section, "AircraftCostBonus", AircraftCostBonus);
    BuildingsCostBonus = pINI->ReadFixed(section, "BuildingsCostBonus", BuildingsCostBonus);
    DefensesCostBonus = pINI->ReadFixed(section, "DefensesCostBonus", DefensesCostBonus);
    TogglePower = pINI->ReadBool(section, "TogglePower", TogglePower);
    { char _buf[0x40]; if (pINI->ReadString(section, "BuildupSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) BuildupSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "PackupSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) PackupSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "CreateUnitSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) CreateUnitSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "UnitExitSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) UnitExitSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "UnitEnterSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) UnitEnterSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "WorkingSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) WorkingSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "NotWorkingSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) NotWorkingSound = _i; } }
    UnitRepair = pINI->ReadBool(section, "UnitRepair", UnitRepair);
    UnitReload = pINI->ReadBool(section, "UnitReload", UnitReload);
    Bunker = pINI->ReadBool(section, "Bunker", Bunker);
    Cloning = pINI->ReadBool(section, "Cloning", Cloning);
    Grinding = pINI->ReadBool(section, "Grinding", Grinding);
    UnitAbsorb = pINI->ReadBool(section, "UnitAbsorb", UnitAbsorb);
    InfantryAbsorb = pINI->ReadBool(section, "InfantryAbsorb", InfantryAbsorb);
    SecretLab = pINI->ReadBool(section, "SecretLab", SecretLab);
    DockUnload = pINI->ReadBool(section, "DockUnload", DockUnload);
    Gate = pINI->ReadBool(section, "Gate", Gate);
    SAM = pINI->ReadBool(section, "SAM", SAM);
    ConstructionYard = pINI->ReadBool(section, "ConstructionYard", ConstructionYard);
    NukeSilo = pINI->ReadBool(section, "NukeSilo", NukeSilo);
    Refinery = pINI->ReadBool(section, "Refinery", Refinery);
    WeaponsFactory = pINI->ReadBool(section, "WeaponsFactory", WeaponsFactory);
    LaserFencePost = pINI->ReadBool(section, "LaserFencePost", LaserFencePost);
    LaserFence = pINI->ReadBool(section, "LaserFence", LaserFence);
    FirestormWall = pINI->ReadBool(section, "FirestormWall", FirestormWall);
    Hospital = pINI->ReadBool(section, "Hospital", Hospital);
    Armory = pINI->ReadBool(section, "Armory", Armory);
    GDIBarracks = pINI->ReadBool(section, "GDIBarracks", GDIBarracks);
    NODBarracks = pINI->ReadBool(section, "NODBarracks", NODBarracks);
    YuriBarracks = pINI->ReadBool(section, "YuriBarracks", YuriBarracks);
    EMPulseCannon = pINI->ReadBool(section, "EMPulseCannon", EMPulseCannon);
    TickTank = pINI->ReadBool(section, "TickTank", TickTank);
    ChargedAnimTime = pINI->ReadFixed(section, "ChargedAnimTime", ChargedAnimTime);
    CloakGenerator = pINI->ReadBool(section, "CloakGenerator", CloakGenerator);
    SensorArray = pINI->ReadBool(section, "SensorArray", SensorArray);
    CloakRadiusInCells = pINI->ReadInteger(section, "CloakRadiusInCells", CloakRadiusInCells);
    PsychicDetectionRadius = pINI->ReadInteger(section, "PsychicDetectionRadius", PsychicDetectionRadius);
    BarrelStartPitch = pINI->ReadInteger(section, "BarrelStartPitch", BarrelStartPitch);
    DeployFacing = pINI->ReadInteger(section, "DeployFacing", DeployFacing);
    LightVisibility = pINI->ReadInteger(section, "LightVisibility", LightVisibility);
    LightIntensity = pINI->ReadFixed(section, "LightIntensity", LightIntensity);
    LightRedTint = pINI->ReadFixed(section, "LightRedTint", LightRedTint);
    LightGreenTint = pINI->ReadFixed(section, "LightGreenTint", LightGreenTint);
    LightBlueTint = pINI->ReadFixed(section, "LightBlueTint", LightBlueTint);
    GateCloseDelay = pINI->ReadFixed(section, "GateCloseDelay", GateCloseDelay);
    InvisibleInGame = pINI->ReadBool(section, "InvisibleInGame", InvisibleInGame);
    pINI->ReadString(section, "PowersUpBuilding", PowersUpBuilding, PowersUpBuilding, sizeof(PowersUpBuilding));
    PowersUpToLevel = pINI->ReadInteger(section, "PowersUpToLevel", PowersUpToLevel);
    BridgeRepairHut = pINI->ReadBool(section, "BridgeRepairHut", BridgeRepairHut);
    HasStupidGuardMode = pINI->ReadBool(section, "HasStupidGuardMode", HasStupidGuardMode);
    CrateBeneath = pINI->ReadBool(section, "CrateBeneath", CrateBeneath);
    CrateBeneathIsMoney = pINI->ReadBool(section, "CrateBeneathIsMoney", CrateBeneathIsMoney);
    LeaveRubble = pINI->ReadBool(section, "LeaveRubble", LeaveRubble);
    PlaceAnywhere = pINI->ReadBool(section, "PlaceAnywhere", PlaceAnywhere);
    ICBMLauncher = pINI->ReadBool(section, "ICBMLauncher", ICBMLauncher);
    Artillary = pINI->ReadBool(section, "Artillary", Artillary);
    pINI->Get3Integers(section, "TargetCoordOffset", TargetCoordOffset);
    pINI->Get3Integers(section, "ExitCoord", ExitCoord);
    AIBuildThis = pINI->ReadBool(section, "AIBuildThis", AIBuildThis);
    IsBaseDefense = pINI->ReadBool(section, "IsBaseDefense", IsBaseDefense);
    IsThreatRatingNode = pINI->ReadBool(section, "IsThreatRatingNode", IsThreatRatingNode);
    ConcentricRadialIndicator = pINI->ReadBool(section, "ConcentricRadialIndicator", ConcentricRadialIndicator);
    Power = pINI->ReadInteger(section, "Power", Power);
    ExtraPower = pINI->ReadInteger(section, "ExtraPower", ExtraPower);
    FoundationValue = pINI->GetFoundation(section, "Foundation", FoundationValue);
    pINI->ReadString(section, "TurretAnim", TurretAnim, TurretAnim, sizeof(TurretAnim));
    pINI->ReadString(section, "TurretAnimDamaged", TurretAnimDamaged, TurretAnimDamaged, sizeof(TurretAnimDamaged));
    pINI->ReadString(section, "TurretAnimGarrisoned", TurretAnimGarrisoned, TurretAnimGarrisoned, sizeof(TurretAnimGarrisoned));
    TurretAnimX = pINI->ReadInteger(section, "TurretAnimX", TurretAnimX);
    TurretAnimY = pINI->ReadInteger(section, "TurretAnimY", TurretAnimY);
    TurretAnimZAdjust = pINI->ReadInteger(section, "TurretAnimZAdjust", TurretAnimZAdjust);
    TurretAnimYSort = pINI->ReadInteger(section, "TurretAnimYSort", TurretAnimYSort);
    TurretAnimIsVoxel = pINI->ReadBool(section, "TurretAnimIsVoxel", TurretAnimIsVoxel);
    BarrelAnimIsVoxel = pINI->ReadBool(section, "BarrelAnimIsVoxel", BarrelAnimIsVoxel);
    pINI->ReadString(section, "VoxelBarrelFile", VoxelBarrelFile, VoxelBarrelFile, sizeof(VoxelBarrelFile));
    pINI->Get3Integers(section, "VoxelBarrelOffsetToPitchPivotPoint", VoxelBarrelOffsetToPitchPivotPoint);
    pINI->Get3Integers(section, "VoxelBarrelOffsetToRotatePivotPoint", VoxelBarrelOffsetToRotatePivotPoint);
    pINI->Get3Integers(section, "VoxelBarrelOffsetToBuildingPivotPoint", VoxelBarrelOffsetToBuildingPivotPoint);
    pINI->Get3Integers(section, "VoxelBarrelOffsetToBarrelEnd", VoxelBarrelOffsetToBarrelEnd);
    Upgrades = pINI->ReadInteger(section, "Upgrades", Upgrades);
    NumberOfDocks = pINI->ReadInteger(section, "NumberOfDocks", NumberOfDocks);

    // ------------------------------------------------------------------
    // artmd.ini fields
    // ------------------------------------------------------------------
    Height = pArt->ReadInteger(section, "Height", Height);
    OccupyHeight = pArt->ReadInteger(section, "OccupyHeight", OccupyHeight);
    Recoilless = pArt->ReadBool(section, "Recoilless", Recoilless);
    DoubleThick = pArt->ReadBool(section, "DoubleThick", DoubleThick);
    Flat = pArt->ReadBool(section, "Flat", Flat);
    SiloDamage = pArt->ReadBool(section, "SiloDamage", SiloDamage);
    ChargeAnim = pArt->ReadBool(section, "ChargeAnim", ChargeAnim);
    IsAnimDelayedFire = pArt->ReadBool(section, "IsAnimDelayedFire", IsAnimDelayedFire);
    DelayedFireDelay = pArt->ReadInteger(section, "DelayedFireDelay", DelayedFireDelay);
    { char _buf[0x40]; if (pArt->ReadString(section, "ToOverlay", "", _buf, sizeof(_buf)) > 0) { OverlayTypeClass* _p = OverlayTypeClass::FindOrAllocate(_buf); if (_p) ToOverlay = _p; } }
    MidPoint = pArt->ReadInteger(section, "MidPoint", MidPoint);
    DoorStages = pArt->ReadInteger(section, "DoorStages", DoorStages);
    DamagedDoor = pArt->ReadBool(section, "DamagedDoor", DamagedDoor);
    TerrainPalette = pArt->ReadBool(section, "TerrainPalette", TerrainPalette);
    GateStages = pArt->ReadInteger(section, "GateStages", GateStages);
    pArt->Get2Integers(section, "PrimaryFirePixelOffset", PrimaryFirePixelOffset);
    pArt->Get2Integers(section, "SecondaryFirePixelOffset", SecondaryFirePixelOffset);
    PrimaryFireDualOffset = pArt->ReadBool(section, "PrimaryFireDualOffset", PrimaryFireDualOffset);
    ExtraDamageStage = pArt->ReadBool(section, "ExtraDamageStage", ExtraDamageStage);
    SpecialZOverlayZAdjust = pArt->ReadInteger(section, "SpecialZOverlayZAdjust", SpecialZOverlayZAdjust);
    NormalZAdjust = pArt->ReadInteger(section, "NormalZAdjust", NormalZAdjust);
    pArt->Get2Integers(section, "ZShapePointMove", ZShapePointMove);
    ExtraLight = pArt->ReadInteger(section, "ExtraLight", ExtraLight);
    CanHideThings = pArt->ReadBool(section, "CanHideThings", CanHideThings);
    pArt->Get2Integers(section, "QueueingCell", QueueingCell);
    pArt->ReadString(section, "Buildup", Buildup, Buildup, sizeof(Buildup));
    pArt->ReadString(section, "AnimIdle", AnimIdle, AnimIdle, sizeof(AnimIdle));
    pArt->ReadString(section, "DeployingAnim", DeployingAnim, DeployingAnim, sizeof(DeployingAnim));
    pArt->ReadString(section, "RoofDeployingAnim", RoofDeployingAnim, RoofDeployingAnim, sizeof(RoofDeployingAnim));
    pArt->ReadString(section, "DoorAnim", DoorAnim, DoorAnim, sizeof(DoorAnim));
    pArt->ReadString(section, "UnderDoorAnim", UnderDoorAnim, UnderDoorAnim, sizeof(UnderDoorAnim));
    pArt->ReadString(section, "UnderRoofDoorAnim", UnderRoofDoorAnim, UnderRoofDoorAnim, sizeof(UnderRoofDoorAnim));
    pArt->ReadString(section, "Rubble", Rubble, Rubble, sizeof(Rubble));
    pArt->ReadString(section, "SpecialZOverlay", SpecialZOverlay, SpecialZOverlay, sizeof(SpecialZOverlay));
    pArt->ReadString(section, "AnimActive", AnimActive, AnimActive, sizeof(AnimActive));
    pArt->ReadString(section, "AnimAux1", AnimAux1, AnimAux1, sizeof(AnimAux1));
    pArt->ReadString(section, "AnimAux2", AnimAux2, AnimAux2, sizeof(AnimAux2));
    pArt->ReadString(section, "ActiveAnim", ActiveAnim, ActiveAnim, sizeof(ActiveAnim));
    pArt->ReadString(section, "ActiveAnimDamaged", ActiveAnimDamaged, ActiveAnimDamaged, sizeof(ActiveAnimDamaged));
    pArt->ReadString(section, "ActiveAnimGarrisoned", ActiveAnimGarrisoned, ActiveAnimGarrisoned, sizeof(ActiveAnimGarrisoned));
    ActiveAnimX = pArt->ReadInteger(section, "ActiveAnimX", ActiveAnimX);
    ActiveAnimY = pArt->ReadInteger(section, "ActiveAnimY", ActiveAnimY);
    ActiveAnimZAdjust = pArt->ReadInteger(section, "ActiveAnimZAdjust", ActiveAnimZAdjust);
    ActiveAnimYSort = pArt->ReadInteger(section, "ActiveAnimYSort", ActiveAnimYSort);
    ActiveAnimPowered = pArt->ReadBool(section, "ActiveAnimPowered", ActiveAnimPowered);
    ActiveAnimPoweredLight = pArt->ReadBool(section, "ActiveAnimPoweredLight", ActiveAnimPoweredLight);
    ActiveAnimPoweredEffect = pArt->ReadBool(section, "ActiveAnimPoweredEffect", ActiveAnimPoweredEffect);
    ActiveAnimPoweredSpecial = pArt->ReadBool(section, "ActiveAnimPoweredSpecial", ActiveAnimPoweredSpecial);
    pArt->ReadString(section, "ActiveAnimTwo", ActiveAnimTwo, ActiveAnimTwo, sizeof(ActiveAnimTwo));
    pArt->ReadString(section, "ActiveAnimTwoDamaged", ActiveAnimTwoDamaged, ActiveAnimTwoDamaged, sizeof(ActiveAnimTwoDamaged));
    pArt->ReadString(section, "ActiveAnimTwoGarrisoned", ActiveAnimTwoGarrisoned, ActiveAnimTwoGarrisoned, sizeof(ActiveAnimTwoGarrisoned));
    ActiveAnimTwoX = pArt->ReadInteger(section, "ActiveAnimTwoX", ActiveAnimTwoX);
    ActiveAnimTwoY = pArt->ReadInteger(section, "ActiveAnimTwoY", ActiveAnimTwoY);
    ActiveAnimTwoZAdjust = pArt->ReadInteger(section, "ActiveAnimTwoZAdjust", ActiveAnimTwoZAdjust);
    ActiveAnimTwoYSort = pArt->ReadInteger(section, "ActiveAnimTwoYSort", ActiveAnimTwoYSort);
    ActiveAnimTwoPowered = pArt->ReadBool(section, "ActiveAnimTwoPowered", ActiveAnimTwoPowered);
    ActiveAnimTwoPoweredLight = pArt->ReadBool(section, "ActiveAnimTwoPoweredLight", ActiveAnimTwoPoweredLight);
    ActiveAnimTwoPoweredEffect = pArt->ReadBool(section, "ActiveAnimTwoPoweredEffect", ActiveAnimTwoPoweredEffect);
    ActiveAnimTwoPoweredSpecial = pArt->ReadBool(section, "ActiveAnimTwoPoweredSpecial", ActiveAnimTwoPoweredSpecial);
    pArt->ReadString(section, "ActiveAnimThree", ActiveAnimThree, ActiveAnimThree, sizeof(ActiveAnimThree));
    pArt->ReadString(section, "ActiveAnimThreeDamaged", ActiveAnimThreeDamaged, ActiveAnimThreeDamaged, sizeof(ActiveAnimThreeDamaged));
    pArt->ReadString(section, "ActiveAnimThreeGarrisoned", ActiveAnimThreeGarrisoned, ActiveAnimThreeGarrisoned, sizeof(ActiveAnimThreeGarrisoned));
    ActiveAnimThreeX = pArt->ReadInteger(section, "ActiveAnimThreeX", ActiveAnimThreeX);
    ActiveAnimThreeY = pArt->ReadInteger(section, "ActiveAnimThreeY", ActiveAnimThreeY);
    ActiveAnimThreeZAdjust = pArt->ReadInteger(section, "ActiveAnimThreeZAdjust", ActiveAnimThreeZAdjust);
    ActiveAnimThreeYSort = pArt->ReadInteger(section, "ActiveAnimThreeYSort", ActiveAnimThreeYSort);
    ActiveAnimThreePowered = pArt->ReadBool(section, "ActiveAnimThreePowered", ActiveAnimThreePowered);
    ActiveAnimThreePoweredLight = pArt->ReadBool(section, "ActiveAnimThreePoweredLight", ActiveAnimThreePoweredLight);
    ActiveAnimThreePoweredEffect = pArt->ReadBool(section, "ActiveAnimThreePoweredEffect", ActiveAnimThreePoweredEffect);
    ActiveAnimThreePoweredSpecial = pArt->ReadBool(section, "ActiveAnimThreePoweredSpecial", ActiveAnimThreePoweredSpecial);
    pArt->ReadString(section, "ActiveAnimFour", ActiveAnimFour, ActiveAnimFour, sizeof(ActiveAnimFour));
    pArt->ReadString(section, "ActiveAnimFourDamaged", ActiveAnimFourDamaged, ActiveAnimFourDamaged, sizeof(ActiveAnimFourDamaged));
    pArt->ReadString(section, "ActiveAnimFourGarrisoned", ActiveAnimFourGarrisoned, ActiveAnimFourGarrisoned, sizeof(ActiveAnimFourGarrisoned));
    ActiveAnimFourX = pArt->ReadInteger(section, "ActiveAnimFourX", ActiveAnimFourX);
    ActiveAnimFourY = pArt->ReadInteger(section, "ActiveAnimFourY", ActiveAnimFourY);
    ActiveAnimFourZAdjust = pArt->ReadInteger(section, "ActiveAnimFourZAdjust", ActiveAnimFourZAdjust);
    ActiveAnimFourYSort = pArt->ReadInteger(section, "ActiveAnimFourYSort", ActiveAnimFourYSort);
    ActiveAnimFourPowered = pArt->ReadBool(section, "ActiveAnimFourPowered", ActiveAnimFourPowered);
    ActiveAnimFourPoweredLight = pArt->ReadBool(section, "ActiveAnimFourPoweredLight", ActiveAnimFourPoweredLight);
    ActiveAnimFourPoweredEffect = pArt->ReadBool(section, "ActiveAnimFourPoweredEffect", ActiveAnimFourPoweredEffect);
    ActiveAnimFourPoweredSpecial = pArt->ReadBool(section, "ActiveAnimFourPoweredSpecial", ActiveAnimFourPoweredSpecial);
    pArt->ReadString(section, "SuperAnim", SuperAnim, SuperAnim, sizeof(SuperAnim));
    pArt->ReadString(section, "SuperAnimDamaged", SuperAnimDamaged, SuperAnimDamaged, sizeof(SuperAnimDamaged));
    pArt->ReadString(section, "SuperAnimGarrisoned", SuperAnimGarrisoned, SuperAnimGarrisoned, sizeof(SuperAnimGarrisoned));
    SuperAnimX = pArt->ReadInteger(section, "SuperAnimX", SuperAnimX);
    SuperAnimY = pArt->ReadInteger(section, "SuperAnimY", SuperAnimY);
    SuperAnimZAdjust = pArt->ReadInteger(section, "SuperAnimZAdjust", SuperAnimZAdjust);
    SuperAnimYSort = pArt->ReadInteger(section, "SuperAnimYSort", SuperAnimYSort);
    SuperAnimPowered = pArt->ReadBool(section, "SuperAnimPowered", SuperAnimPowered);
    SuperAnimPoweredLight = pArt->ReadBool(section, "SuperAnimPoweredLight", SuperAnimPoweredLight);
    SuperAnimPoweredEffect = pArt->ReadBool(section, "SuperAnimPoweredEffect", SuperAnimPoweredEffect);
    SuperAnimPoweredSpecial = pArt->ReadBool(section, "SuperAnimPoweredSpecial", SuperAnimPoweredSpecial);
    pArt->ReadString(section, "SuperAnimTwo", SuperAnimTwo, SuperAnimTwo, sizeof(SuperAnimTwo));
    pArt->ReadString(section, "SuperAnimTwoDamaged", SuperAnimTwoDamaged, SuperAnimTwoDamaged, sizeof(SuperAnimTwoDamaged));
    pArt->ReadString(section, "SuperAnimTwoGarrisoned", SuperAnimTwoGarrisoned, SuperAnimTwoGarrisoned, sizeof(SuperAnimTwoGarrisoned));
    SuperAnimTwoX = pArt->ReadInteger(section, "SuperAnimTwoX", SuperAnimTwoX);
    SuperAnimTwoY = pArt->ReadInteger(section, "SuperAnimTwoY", SuperAnimTwoY);
    SuperAnimTwoZAdjust = pArt->ReadInteger(section, "SuperAnimTwoZAdjust", SuperAnimTwoZAdjust);
    SuperAnimTwoYSort = pArt->ReadInteger(section, "SuperAnimTwoYSort", SuperAnimTwoYSort);
    SuperAnimTwoPowered = pArt->ReadBool(section, "SuperAnimTwoPowered", SuperAnimTwoPowered);
    SuperAnimTwoPoweredLight = pArt->ReadBool(section, "SuperAnimTwoPoweredLight", SuperAnimTwoPoweredLight);
    SuperAnimTwoPoweredEffect = pArt->ReadBool(section, "SuperAnimTwoPoweredEffect", SuperAnimTwoPoweredEffect);
    SuperAnimTwoPoweredSpecial = pArt->ReadBool(section, "SuperAnimTwoPoweredSpecial", SuperAnimTwoPoweredSpecial);
    pArt->ReadString(section, "SuperAnimThree", SuperAnimThree, SuperAnimThree, sizeof(SuperAnimThree));
    pArt->ReadString(section, "SuperAnimThreeDamaged", SuperAnimThreeDamaged, SuperAnimThreeDamaged, sizeof(SuperAnimThreeDamaged));
    pArt->ReadString(section, "SuperAnimThreeGarrisoned", SuperAnimThreeGarrisoned, SuperAnimThreeGarrisoned, sizeof(SuperAnimThreeGarrisoned));
    SuperAnimThreeX = pArt->ReadInteger(section, "SuperAnimThreeX", SuperAnimThreeX);
    SuperAnimThreeY = pArt->ReadInteger(section, "SuperAnimThreeY", SuperAnimThreeY);
    SuperAnimThreeZAdjust = pArt->ReadInteger(section, "SuperAnimThreeZAdjust", SuperAnimThreeZAdjust);
    SuperAnimThreeYSort = pArt->ReadInteger(section, "SuperAnimThreeYSort", SuperAnimThreeYSort);
    SuperAnimThreePowered = pArt->ReadBool(section, "SuperAnimThreePowered", SuperAnimThreePowered);
    SuperAnimThreePoweredLight = pArt->ReadBool(section, "SuperAnimThreePoweredLight", SuperAnimThreePoweredLight);
    SuperAnimThreePoweredEffect = pArt->ReadBool(section, "SuperAnimThreePoweredEffect", SuperAnimThreePoweredEffect);
    SuperAnimThreePoweredSpecial = pArt->ReadBool(section, "SuperAnimThreePoweredSpecial", SuperAnimThreePoweredSpecial);
    pArt->ReadString(section, "SuperAnimFour", SuperAnimFour, SuperAnimFour, sizeof(SuperAnimFour));
    pArt->ReadString(section, "SuperAnimFourDamaged", SuperAnimFourDamaged, SuperAnimFourDamaged, sizeof(SuperAnimFourDamaged));
    pArt->ReadString(section, "SuperAnimFourGarrisoned", SuperAnimFourGarrisoned, SuperAnimFourGarrisoned, sizeof(SuperAnimFourGarrisoned));
    SuperAnimFourX = pArt->ReadInteger(section, "SuperAnimFourX", SuperAnimFourX);
    SuperAnimFourY = pArt->ReadInteger(section, "SuperAnimFourY", SuperAnimFourY);
    SuperAnimFourZAdjust = pArt->ReadInteger(section, "SuperAnimFourZAdjust", SuperAnimFourZAdjust);
    SuperAnimFourYSort = pArt->ReadInteger(section, "SuperAnimFourYSort", SuperAnimFourYSort);
    SuperAnimFourPowered = pArt->ReadBool(section, "SuperAnimFourPowered", SuperAnimFourPowered);
    SuperAnimFourPoweredLight = pArt->ReadBool(section, "SuperAnimFourPoweredLight", SuperAnimFourPoweredLight);
    SuperAnimFourPoweredEffect = pArt->ReadBool(section, "SuperAnimFourPoweredEffect", SuperAnimFourPoweredEffect);
    SuperAnimFourPoweredSpecial = pArt->ReadBool(section, "SuperAnimFourPoweredSpecial", SuperAnimFourPoweredSpecial);
    pArt->ReadString(section, "SpecialAnim", SpecialAnim, SpecialAnim, sizeof(SpecialAnim));
    pArt->ReadString(section, "SpecialAnimDamaged", SpecialAnimDamaged, SpecialAnimDamaged, sizeof(SpecialAnimDamaged));
    pArt->ReadString(section, "SpecialAnimGarrisoned", SpecialAnimGarrisoned, SpecialAnimGarrisoned, sizeof(SpecialAnimGarrisoned));
    SpecialAnimX = pArt->ReadInteger(section, "SpecialAnimX", SpecialAnimX);
    SpecialAnimY = pArt->ReadInteger(section, "SpecialAnimY", SpecialAnimY);
    SpecialAnimZAdjust = pArt->ReadInteger(section, "SpecialAnimZAdjust", SpecialAnimZAdjust);
    SpecialAnimYSort = pArt->ReadInteger(section, "SpecialAnimYSort", SpecialAnimYSort);
    SpecialAnimPowered = pArt->ReadBool(section, "SpecialAnimPowered", SpecialAnimPowered);
    SpecialAnimPoweredLight = pArt->ReadBool(section, "SpecialAnimPoweredLight", SpecialAnimPoweredLight);
    SpecialAnimPoweredEffect = pArt->ReadBool(section, "SpecialAnimPoweredEffect", SpecialAnimPoweredEffect);
    SpecialAnimPoweredSpecial = pArt->ReadBool(section, "SpecialAnimPoweredSpecial", SpecialAnimPoweredSpecial);
    pArt->ReadString(section, "SpecialAnimTwo", SpecialAnimTwo, SpecialAnimTwo, sizeof(SpecialAnimTwo));
    pArt->ReadString(section, "SpecialAnimTwoDamaged", SpecialAnimTwoDamaged, SpecialAnimTwoDamaged, sizeof(SpecialAnimTwoDamaged));
    pArt->ReadString(section, "SpecialAnimTwoGarrisoned", SpecialAnimTwoGarrisoned, SpecialAnimTwoGarrisoned, sizeof(SpecialAnimTwoGarrisoned));
    SpecialAnimTwoX = pArt->ReadInteger(section, "SpecialAnimTwoX", SpecialAnimTwoX);
    SpecialAnimTwoY = pArt->ReadInteger(section, "SpecialAnimTwoY", SpecialAnimTwoY);
    SpecialAnimTwoZAdjust = pArt->ReadInteger(section, "SpecialAnimTwoZAdjust", SpecialAnimTwoZAdjust);
    SpecialAnimTwoYSort = pArt->ReadInteger(section, "SpecialAnimTwoYSort", SpecialAnimTwoYSort);
    SpecialAnimTwoPowered = pArt->ReadBool(section, "SpecialAnimTwoPowered", SpecialAnimTwoPowered);
    SpecialAnimTwoPoweredLight = pArt->ReadBool(section, "SpecialAnimTwoPoweredLight", SpecialAnimTwoPoweredLight);
    SpecialAnimTwoPoweredEffect = pArt->ReadBool(section, "SpecialAnimTwoPoweredEffect", SpecialAnimTwoPoweredEffect);
    SpecialAnimTwoPoweredSpecial = pArt->ReadBool(section, "SpecialAnimTwoPoweredSpecial", SpecialAnimTwoPoweredSpecial);
    pArt->ReadString(section, "SpecialAnimThree", SpecialAnimThree, SpecialAnimThree, sizeof(SpecialAnimThree));
    pArt->ReadString(section, "SpecialAnimThreeDamaged", SpecialAnimThreeDamaged, SpecialAnimThreeDamaged, sizeof(SpecialAnimThreeDamaged));
    pArt->ReadString(section, "SpecialAnimThreeGarrisoned", SpecialAnimThreeGarrisoned, SpecialAnimThreeGarrisoned, sizeof(SpecialAnimThreeGarrisoned));
    SpecialAnimThreeX = pArt->ReadInteger(section, "SpecialAnimThreeX", SpecialAnimThreeX);
    SpecialAnimThreeY = pArt->ReadInteger(section, "SpecialAnimThreeY", SpecialAnimThreeY);
    SpecialAnimThreeZAdjust = pArt->ReadInteger(section, "SpecialAnimThreeZAdjust", SpecialAnimThreeZAdjust);
    SpecialAnimThreeYSort = pArt->ReadInteger(section, "SpecialAnimThreeYSort", SpecialAnimThreeYSort);
    SpecialAnimThreePowered = pArt->ReadBool(section, "SpecialAnimThreePowered", SpecialAnimThreePowered);
    SpecialAnimThreePoweredLight = pArt->ReadBool(section, "SpecialAnimThreePoweredLight", SpecialAnimThreePoweredLight);
    SpecialAnimThreePoweredEffect = pArt->ReadBool(section, "SpecialAnimThreePoweredEffect", SpecialAnimThreePoweredEffect);
    SpecialAnimThreePoweredSpecial = pArt->ReadBool(section, "SpecialAnimThreePoweredSpecial", SpecialAnimThreePoweredSpecial);
    pArt->ReadString(section, "SpecialAnimFour", SpecialAnimFour, SpecialAnimFour, sizeof(SpecialAnimFour));
    pArt->ReadString(section, "SpecialAnimFourDamaged", SpecialAnimFourDamaged, SpecialAnimFourDamaged, sizeof(SpecialAnimFourDamaged));
    pArt->ReadString(section, "SpecialAnimFourGarrisoned", SpecialAnimFourGarrisoned, SpecialAnimFourGarrisoned, sizeof(SpecialAnimFourGarrisoned));
    SpecialAnimFourX = pArt->ReadInteger(section, "SpecialAnimFourX", SpecialAnimFourX);
    SpecialAnimFourY = pArt->ReadInteger(section, "SpecialAnimFourY", SpecialAnimFourY);
    SpecialAnimFourZAdjust = pArt->ReadInteger(section, "SpecialAnimFourZAdjust", SpecialAnimFourZAdjust);
    SpecialAnimFourYSort = pArt->ReadInteger(section, "SpecialAnimFourYSort", SpecialAnimFourYSort);
    SpecialAnimFourPowered = pArt->ReadBool(section, "SpecialAnimFourPowered", SpecialAnimFourPowered);
    SpecialAnimFourPoweredLight = pArt->ReadBool(section, "SpecialAnimFourPoweredLight", SpecialAnimFourPoweredLight);
    SpecialAnimFourPoweredEffect = pArt->ReadBool(section, "SpecialAnimFourPoweredEffect", SpecialAnimFourPoweredEffect);
    SpecialAnimFourPoweredSpecial = pArt->ReadBool(section, "SpecialAnimFourPoweredSpecial", SpecialAnimFourPoweredSpecial);
    pArt->ReadString(section, "LowPower", LowPower, LowPower, sizeof(LowPower));
    pArt->ReadString(section, "LowPowerDamaged", LowPowerDamaged, LowPowerDamaged, sizeof(LowPowerDamaged));
    pArt->ReadString(section, "LowPowerGarrisoned", LowPowerGarrisoned, LowPowerGarrisoned, sizeof(LowPowerGarrisoned));
    LowPowerX = pArt->ReadInteger(section, "LowPowerX", LowPowerX);
    LowPowerY = pArt->ReadInteger(section, "LowPowerY", LowPowerY);
    LowPowerZAdjust = pArt->ReadInteger(section, "LowPowerZAdjust", LowPowerZAdjust);
    LowPowerYSort = pArt->ReadInteger(section, "LowPowerYSort", LowPowerYSort);
    LowPowerPowered = pArt->ReadBool(section, "LowPowerPowered", LowPowerPowered);
    LowPowerPoweredLight = pArt->ReadBool(section, "LowPowerPoweredLight", LowPowerPoweredLight);
    LowPowerPoweredEffect = pArt->ReadBool(section, "LowPowerPoweredEffect", LowPowerPoweredEffect);
    LowPowerPoweredSpecial = pArt->ReadBool(section, "LowPowerPoweredSpecial", LowPowerPoweredSpecial);
    pArt->ReadString(section, "SuperLowPower", SuperLowPower, SuperLowPower, sizeof(SuperLowPower));
    pArt->ReadString(section, "SuperLowPowerDamaged", SuperLowPowerDamaged, SuperLowPowerDamaged, sizeof(SuperLowPowerDamaged));
    pArt->ReadString(section, "SuperLowPowerGarrisoned", SuperLowPowerGarrisoned, SuperLowPowerGarrisoned, sizeof(SuperLowPowerGarrisoned));
    SuperLowPowerX = pArt->ReadInteger(section, "SuperLowPowerX", SuperLowPowerX);
    SuperLowPowerY = pArt->ReadInteger(section, "SuperLowPowerY", SuperLowPowerY);
    SuperLowPowerZAdjust = pArt->ReadInteger(section, "SuperLowPowerZAdjust", SuperLowPowerZAdjust);
    SuperLowPowerYSort = pArt->ReadInteger(section, "SuperLowPowerYSort", SuperLowPowerYSort);
    SuperLowPowerPowered = pArt->ReadBool(section, "SuperLowPowerPowered", SuperLowPowerPowered);
    SuperLowPowerPoweredLight = pArt->ReadBool(section, "SuperLowPowerPoweredLight", SuperLowPowerPoweredLight);
    SuperLowPowerPoweredEffect = pArt->ReadBool(section, "SuperLowPowerPoweredEffect", SuperLowPowerPoweredEffect);
    SuperLowPowerPoweredSpecial = pArt->ReadBool(section, "SuperLowPowerPoweredSpecial", SuperLowPowerPoweredSpecial);
    pArt->ReadString(section, "ProductionAnim", ProductionAnim, ProductionAnim, sizeof(ProductionAnim));
    pArt->ReadString(section, "ProductionAnimDamaged", ProductionAnimDamaged, ProductionAnimDamaged, sizeof(ProductionAnimDamaged));
    pArt->ReadString(section, "ProductionAnimGarrisoned", ProductionAnimGarrisoned, ProductionAnimGarrisoned, sizeof(ProductionAnimGarrisoned));
    ProductionAnimX = pArt->ReadInteger(section, "ProductionAnimX", ProductionAnimX);
    ProductionAnimY = pArt->ReadInteger(section, "ProductionAnimY", ProductionAnimY);
    ProductionAnimZAdjust = pArt->ReadInteger(section, "ProductionAnimZAdjust", ProductionAnimZAdjust);
    ProductionAnimYSort = pArt->ReadInteger(section, "ProductionAnimYSort", ProductionAnimYSort);
    pArt->ReadString(section, "IdleAnim", IdleAnim, IdleAnim, sizeof(IdleAnim));
    pArt->ReadString(section, "IdleAnimDamaged", IdleAnimDamaged, IdleAnimDamaged, sizeof(IdleAnimDamaged));
    pArt->ReadString(section, "IdleAnimGarrisoned", IdleAnimGarrisoned, IdleAnimGarrisoned, sizeof(IdleAnimGarrisoned));
    IdleAnimX = pArt->ReadInteger(section, "IdleAnimX", IdleAnimX);
    IdleAnimY = pArt->ReadInteger(section, "IdleAnimY", IdleAnimY);
    IdleAnimZAdjust = pArt->ReadInteger(section, "IdleAnimZAdjust", IdleAnimZAdjust);
    IdleAnimYSort = pArt->ReadInteger(section, "IdleAnimYSort", IdleAnimYSort);
    IdleAnimPowered = pArt->ReadBool(section, "IdleAnimPowered", IdleAnimPowered);
    IdleAnimPoweredLight = pArt->ReadBool(section, "IdleAnimPoweredLight", IdleAnimPoweredLight);
    IdleAnimPoweredEffect = pArt->ReadBool(section, "IdleAnimPoweredEffect", IdleAnimPoweredEffect);
    IdleAnimPoweredSpecial = pArt->ReadBool(section, "IdleAnimPoweredSpecial", IdleAnimPoweredSpecial);
    pArt->ReadString(section, "PreProductionAnim", PreProductionAnim, PreProductionAnim, sizeof(PreProductionAnim));
    pArt->ReadString(section, "PreProductionAnimDamaged", PreProductionAnimDamaged, PreProductionAnimDamaged, sizeof(PreProductionAnimDamaged));
    pArt->ReadString(section, "PreProductionAnimGarrisoned", PreProductionAnimGarrisoned, PreProductionAnimGarrisoned, sizeof(PreProductionAnimGarrisoned));
    PreProductionAnimX = pArt->ReadInteger(section, "PreProductionAnimX", PreProductionAnimX);
    PreProductionAnimY = pArt->ReadInteger(section, "PreProductionAnimY", PreProductionAnimY);
    PreProductionAnimZAdjust = pArt->ReadInteger(section, "PreProductionAnimZAdjust", PreProductionAnimZAdjust);
    PreProductionAnimYSort = pArt->ReadInteger(section, "PreProductionAnimYSort", PreProductionAnimYSort);

    // ------------------------------------------------------------------
    // Indexed key families
    // ------------------------------------------------------------------
    for (int32 i = 0; i < 8; ++i)
    {
        char key[40];
        sprintf_s(key, sizeof(key), "AddOccupy%d", i);
        pArt->Get2Integers(section, key, AddOccupy[i]);
    }
    for (int32 i = 0; i < 8; ++i)
    {
        char key[40];
        sprintf_s(key, sizeof(key), "RemoveOccupy%d", i);
        pArt->Get2Integers(section, key, RemoveOccupy[i]);
    }
    // Upgrade spot art - numbered from one up to the declared Upgrades
    // count; each slot carries an anim pair plus its pixel offset.
    if (Upgrades > 0)
    {
        for (int32 i = 1; i <= Upgrades && i <= 4; ++i)
        {
            char key[40];
            sprintf_s(key, sizeof(key), "PowerUp%01dAnim", i);
            pArt->ReadString(section, key, PowerUpAnim[i - 1], PowerUpAnim[i - 1],
                             sizeof(PowerUpAnim[i - 1]));

            sprintf_s(key, sizeof(key), "PowerUp%01dDamagedAnim", i);
            pArt->ReadString(section, key, PowerUpDamagedAnim[i - 1],
                             PowerUpDamagedAnim[i - 1],
                             sizeof(PowerUpDamagedAnim[i - 1]));

            sprintf_s(key, sizeof(key), "PowerUp%01dLocXX", i);
            PowerUpLocXX[i - 1] = pArt->ReadInteger(section, key, PowerUpLocXX[i - 1]);

            sprintf_s(key, sizeof(key), "PowerUp%01dLocYY", i);
            PowerUpLocYY[i - 1] = pArt->ReadInteger(section, key, PowerUpLocYY[i - 1]);

            sprintf_s(key, sizeof(key), "PowerUp%01dLocZZ", i);
            PowerUpLocZZ[i - 1] = pArt->ReadInteger(section, key, PowerUpLocZZ[i - 1]);

            sprintf_s(key, sizeof(key), "PowerUp%01dYSort", i);
            PowerUpYSort[i - 1] = pArt->ReadInteger(section, key, PowerUpYSort[i - 1]);
        }
    }
    for (int32 i = 0; i < 4; ++i)
    {
        char key[40];
        sprintf_s(key, sizeof(key), "DockingOffset%d", i);
        pArt->Get3Integers(section, key, DockingOffset[i]);
    }

    MuzzleFlash = pArt->ReadInteger(section, "MuzzleFlash", MuzzleFlash);
    pArt->Get2Integers(section, "DamageFireOffset", DamageFireOffset);
    CaptureEvaEvent = pINI->ReadInteger(section, "CaptureEvaEvent", CaptureEvaEvent);
    { char _buf[0x40]; if (pINI->ReadString(section, "SecretBuilding", "", _buf, sizeof(_buf)) > 0) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_buf); if (_p) SecretBuilding = _p; } }


    // Foundation comes back as an enum; keep the packed legacy field in sync.
    {
        int32 fw = 1, fh = 1;
        switch (FoundationValue)
        {
        case Foundation::_2x1: case Foundation::_2x2: case Foundation::_2x3:
        case Foundation::_2x5: case Foundation::_2x6:
            fw = 2; break;
        case Foundation::_3x1: case Foundation::_3x2: case Foundation::_3x3:
        case Foundation::_3x4: case Foundation::_3x5:
            fw = 3; break;
        case Foundation::_4x2: case Foundation::_4x3: case Foundation::_4x4:
            fw = 4; break;
        case Foundation::_5x3: fw = 5; break;
        case Foundation::_6x4: fw = 6; break;
        default: fw = 1; break;
        }
        switch (FoundationValue)
        {
        case Foundation::_1x2: case Foundation::_2x2: case Foundation::_3x2:
        case Foundation::_4x2:
            fh = 2; break;
        case Foundation::_1x3: case Foundation::_2x3: case Foundation::_3x3:
        case Foundation::_4x3: case Foundation::_3x3Refinery: case Foundation::_5x3:
            fh = 3; break;
        case Foundation::_1x4: case Foundation::_3x4: case Foundation::_4x4:
        case Foundation::_6x4:
            fh = 4; break;
        case Foundation::_1x5: case Foundation::_2x5: case Foundation::_3x5:
            fh = 5; break;
        case Foundation::_2x6: fh = 6; break;
        default: fh = 1; break;
        }
        BuildingFoundation = (fw << 16) | (fh & 0xFFFF);
    }

        return true;
}

// ============================================================================
// SaveToINI
// ============================================================================

bool BuildingTypeClass::SaveToINI(CCINIClass* pINI) const
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // Chain the parent.
    const_cast<BuildingTypeClass*>(this)->TechnoTypeClass::SaveToINI(pINI);

    char foundBuf[32];
    sprintf_s(foundBuf, sizeof(foundBuf), "%dx%d", Get_Width(), Get_Height());
    pINI->WriteString(section, "Foundation", foundBuf);

    pINI->WriteInteger(section, "Height",       Height);
    pINI->WriteInteger(section, "Power",        Power);
    pINI->WriteInteger(section, "PowerDrain",   PowerDrain);
    pINI->WriteInteger(section, "Storage",      Storage);
    pINI->WriteInteger(section, "Adjacent",     Adjacent);
    pINI->WriteInteger(section, "MaxWalls",     MaxWalls);
    pINI->WriteInteger(section, "NumberOfDocks",NumberOfDocks);
    pINI->WriteInteger(section, "OccupyCount",  OccupyCount);
    pINI->WriteInteger(section, "SuperWeapon",  SuperWeapon);
    pINI->WriteInteger(section, "SuperWeapon2", SuperWeapon2);
    pINI->WriteInteger(section, "WeaponCount",  WeaponCount);
    pINI->WriteInteger(section, "OccupyWeaponCount",      OccupyWeaponCount);
    pINI->WriteInteger(section, "EliteOccupyWeaponCount", EliteOccupyWeaponCount);
    pINI->WriteInteger(section, "DeathWeapon",  DeathWeaponIndex_);
    pINI->WriteFloat(section,  "ThreatPosed",  ThreatPosedValue_);

    pINI->WriteBool(section, "Factory",          IsFactory_);
    pINI->WriteBool(section, "Barracks",         IsBarracks);
    pINI->WriteBool(section, "WeaponsFactory",   IsWeaponsFactory);
    pINI->WriteBool(section, "WarFactory",       IsWarfactory);
    pINI->WriteBool(section, "Airport",          IsAirport);
    pINI->WriteBool(section, "NavalYard",        IsNavalYard);
    pINI->WriteBool(section, "RepairPad",        IsRepairPad);
    pINI->WriteBool(section, "MissileSilo",      IsMissileSilo);
    pINI->WriteBool(section, "ConstructionYard", IsConstructionYard);
    pINI->WriteBool(section, "OreRefinery",      IsOreRefinery);
    pINI->WriteBool(section, "OreStorage",       IsOreStorage);
    pINI->WriteBool(section, "Radar",            IsRadar);
    pINI->WriteBool(section, "Tech",             IsTech);
    pINI->WriteBool(section, "SecretLab",        IsSecretLab);
    pINI->WriteBool(section, "Base",             IsBase);
    pINI->WriteBool(section, "Wall",             IsWall_);
    pINI->WriteBool(section, "Gate",             IsGate);

    pINI->WriteBool(section, "Dock",             HasDock);
    pINI->WriteBool(section, "Helipad",          HasHelipad);
    pINI->WriteBool(section, "Bib",              HasBib);
    pINI->WriteBool(section, "Spotlight",        HasSpotlight);
    pINI->WriteBool(section, "Powered",          IsPowered);

    pINI->WriteBool(section, "Sellable",         CanBeSold_);
    pINI->WriteBool(section, "Unsellable",       IsUnsellable);
    pINI->WriteBool(section, "Repairable",       IsRepairable);
    pINI->WriteBool(section, "Ungarrisonable",   IsUngarrisonable);
    pINI->WriteBool(section, "Undeployable",     IsUndeployable_);
    pINI->WriteBool(section, "Deployer",   IsSimpleDeployer_);
    pINI->WriteBool(section, "Firebase",         IsFirebase_);

    pINI->WriteBool(section, "CanC4",            IsCanC4);
    pINI->WriteBool(section, "CanBeOccupied",    IsCanBeOccupied);
    pINI->WriteBool(section, "CanBeDriven",      IsCanBeDriven);
    pINI->WriteBool(section, "Capturable",    IsCanBeCaptured);
    pINI->WriteBool(section, "Powered",     IsCanBePowered);
    pINI->WriteBool(section, "CanBeDestroyed",   IsCanBeDestroyed);
    pINI->WriteBool(section, "CanBeDamaged",     IsCanBeDamaged);
    pINI->WriteBool(section, "CanBeInfiltrated", IsCanBeInfiltrated);
    pINI->WriteBool(section, "Spyable",       IsCanBeSpied);
    pINI->WriteBool(section, "CanBeSabotaged",   IsCanBeSabotaged);
    pINI->WriteBool(section, "CanBeStolen",      IsCanBeStolen);
    pINI->WriteBool(section, "CanBeHijacked",    IsCanBeHijacked);

    pINI->WriteBool(section, "Plug",             IsPlug);
    pINI->WriteBool(section, "Drain",            IsDrain);
    pINI->WriteBool(section, "Drainable",        IsDrainable);
    pINI->WriteBool(section, "Rig",              IsRig);
    pINI->WriteBool(section, "RigOwner",         IsRigOwner);
    pINI->WriteBool(section, "Resource",         IsResource);
    pINI->WriteBool(section, "ResourceGatherer", IsResourceGatherer);

    pINI->WriteBool(section, "Bombable",         IsBombable);
    pINI->WriteBool(section, "AutoFire",         IsAutoFire);
    pINI->WriteBool(section, "GuardRange",       IsGuardRange);
    pINI->WriteBool(section, "Aggressive",       IsAggressive);

    pINI->WriteBool(section, "Neutral",          IsNeutral);
    pINI->WriteBool(section, "Infiltratable",    IsInfiltratable);
    pINI->WriteBool(section, "Stealthy",         IsStealthy);
    pINI->WriteBool(section, "Healable",         IsHealable);
    pINI->WriteBool(section, "Tilter",           IsTilter);
    pINI->WriteBool(section, "ToProtect",        IsToProtect);
    pINI->WriteBool(section, "Nominal",          IsNominal);
    pINI->WriteBool(section, "RadarInvisible",   IsRadarInvisible);
    pINI->WriteBool(section, "DontScore",        IsDontScore);
    pINI->WriteBool(section, "NoThreat",         IsNoThreat);
    pINI->WriteBool(section, "SensorsSight",     IsSensorsSight);
    pINI->WriteBool(section, "HunterSeeker",     IsHunterSeeker);
    pINI->WriteBool(section, "Ivan",             IsIvan);
    pINI->WriteBool(section, "Leader",           IsLeader);
    pINI->WriteBool(section, "Carryall",         IsCarryall);
    pINI->WriteBool(section, "Train",            IsTrain);
    pINI->WriteBool(section, "ConsideredAircraft", IsConsideredAircraft);
    pINI->WriteBool(section, "ConsideredVehicle",  IsConsideredVehicle);

    return true;
}

// ============================================================================
// CRC
// ============================================================================

void BuildingTypeClass::ComputeCRC(CRCEngine& crc) const
{
    TechnoTypeClass::ComputeCRC(crc);

    crc.AddData(&BuildingFoundation, sizeof(BuildingFoundation));
    crc.AddData(&Height,             sizeof(Height));
    crc.AddData(&Power,              sizeof(Power));
    crc.AddData(&PowerDrain,         sizeof(PowerDrain));
    crc.AddData(&Bib,                sizeof(Bib));
    crc.AddData(&CanBeSold_,         sizeof(CanBeSold_));
    crc.AddData(&IsUndeployable_,    sizeof(IsUndeployable_));
    crc.AddData(&IsSimpleDeployer_,  sizeof(IsSimpleDeployer_));
    crc.AddData(&IsFirebase_,        sizeof(IsFirebase_));
    crc.AddData(&IsFactory_,         sizeof(IsFactory_));
    crc.AddData(&HasSpotlight,       sizeof(HasSpotlight));
    crc.AddData(&HasBib,             sizeof(HasBib));
    crc.AddData(&HasHelipad,         sizeof(HasHelipad));
    crc.AddData(&HasDock,            sizeof(HasDock));
    crc.AddData(&IsBase,             sizeof(IsBase));
    crc.AddData(&IsWall_,            sizeof(IsWall_));
    crc.AddData(&IsGate,             sizeof(IsGate));
    crc.AddData(&IsOreRefinery,      sizeof(IsOreRefinery));
    crc.AddData(&IsOreStorage,       sizeof(IsOreStorage));
    crc.AddData(&IsWeaponsFactory,   sizeof(IsWeaponsFactory));
    crc.AddData(&IsBarracks,         sizeof(IsBarracks));
    crc.AddData(&IsRadar,            sizeof(IsRadar));
    crc.AddData(&IsTech,             sizeof(IsTech));
    crc.AddData(&IsSecretLab,        sizeof(IsSecretLab));
    crc.AddData(&IsConstructionYard, sizeof(IsConstructionYard));
    crc.AddData(&IsAirport,          sizeof(IsAirport));
    crc.AddData(&IsWarfactory,       sizeof(IsWarfactory));
    crc.AddData(&IsNavalYard,        sizeof(IsNavalYard));
    crc.AddData(&IsRepairPad,        sizeof(IsRepairPad));
    crc.AddData(&IsMissileSilo,      sizeof(IsMissileSilo));
    crc.AddData(&IsPowered,          sizeof(IsPowered));

    crc.AddData(&Storage,            sizeof(Storage));
    crc.AddData(&NumberOfDocks,      sizeof(NumberOfDocks));
    crc.AddData(&Adjacent,           sizeof(Adjacent));
    crc.AddData(&MaxWalls,           sizeof(MaxWalls));
    crc.AddData(&SuperWeapon,        sizeof(SuperWeapon));
    crc.AddData(&SuperWeapon2,       sizeof(SuperWeapon2));
    crc.AddData(&WeaponCount,        sizeof(WeaponCount));
    crc.AddData(Weapons,             static_cast<int32>(sizeof(Weapons)));
    crc.AddData(&OccupyWeaponCount,  sizeof(OccupyWeaponCount));
    crc.AddData(&HasSuperWeapon,     sizeof(HasSuperWeapon));
    crc.AddData(&HasSuperWeapon2,    sizeof(HasSuperWeapon2));
}

int32 BuildingTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// BuildingTypeClass - static lookup helpers
// ============================================================================

BuildingTypeClass* BuildingTypeClass::FindOrAllocate(const char* pID)
{
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    BuildingTypeClass* found = Find(pID);
    if (found) return found;
    if (!Array) Init_Array();
    BuildingTypeClass* newItem = GameCreate<BuildingTypeClass>();
    if (newItem)
    {
        strncpy(newItem->ID, pID, sizeof(newItem->ID) - 1);
        newItem->ID[sizeof(newItem->ID) - 1] = '\0';
    }
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}
