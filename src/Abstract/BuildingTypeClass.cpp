#include <Abstract/BuildingTypeClass.h>

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

int32 BuildingTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
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
    PowerDrain = pINI->ReadInteger(section, "PowerDrain", PowerDrain);
    IsPowered  = pINI->ReadBool(section,   "Powered",  IsPowered);
    IsPowered_ = IsPowered;

    // ------------------------------------------------------------------
    // Storage / economy
    // ------------------------------------------------------------------
    Storage   = pINI->ReadInteger(section, "Storage",  Storage);
    Adjacent  = pINI->ReadInteger(section, "Adjacent", Adjacent);
    MaxWalls  = pINI->ReadInteger(section, "MaxWalls", MaxWalls);

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
    HasSpotlight = pINI->ReadBool(section, "Spotlight", HasSpotlight);
    Bib          = pINI->ReadInteger(section, "BibShape", Bib);
    IsHasBib       = HasBib;
    IsHasSpotlight = HasSpotlight;

    // ------------------------------------------------------------------
    // Factory / building-type classification
    // ------------------------------------------------------------------
    IsFactory_         = pINI->ReadBool(section, "Factory",          IsFactory_);
    IsBarracks         = pINI->ReadBool(section, "Barracks",         IsBarracks);
    IsWeaponsFactory   = pINI->ReadBool(section, "WeaponsFactory",   IsWeaponsFactory);
    IsWarfactory       = pINI->ReadBool(section, "WarFactory",       IsWarfactory);
    IsAirport          = pINI->ReadBool(section, "Airport",          IsAirport);
    IsNavalYard        = pINI->ReadBool(section, "NavalYard",        IsNavalYard);
    IsRepairPad        = pINI->ReadBool(section, "RepairPad",        IsRepairPad);
    IsMissileSilo      = pINI->ReadBool(section, "MissileSilo",      IsMissileSilo);
    IsConstructionYard = pINI->ReadBool(section, "ConstructionYard", IsConstructionYard);
    IsOreRefinery      = pINI->ReadBool(section, "OreRefinery",      IsOreRefinery);
    IsOreStorage       = pINI->ReadBool(section, "OreStorage",       IsOreStorage);
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
    OccupyCount            = pINI->ReadInteger(section, "OccupyCount", OccupyCount);
    OccupyWeaponCount      = pINI->ReadInteger(section, "OccupyWeaponCount",
                                                OccupyWeaponCount);
    EliteOccupyWeaponCount = pINI->ReadInteger(section, "EliteOccupyWeaponCount",
                                                EliteOccupyWeaponCount);
    if (OccupyWeaponCount < 0)      OccupyWeaponCount = 0;
    if (OccupyWeaponCount > 2)      OccupyWeaponCount = 2;
    if (EliteOccupyWeaponCount < 0) EliteOccupyWeaponCount = 0;
    if (EliteOccupyWeaponCount > 2) EliteOccupyWeaponCount = 2;
    OccupyCount_ = OccupyCount;

    // ------------------------------------------------------------------
    // Build / sell flags
    // ------------------------------------------------------------------
    CanBeSold_       = pINI->ReadBool(section, "Sellable",       CanBeSold_);
    IsCanBeSold_     = CanBeSold_;
    IsSellable       = CanBeSold_;
    IsSellable_      = CanBeSold_;
    IsUnsellable     = pINI->ReadBool(section, "Unsellable",     IsUnsellable);
    IsUnsellable_    = IsUnsellable;
    IsRepairable     = pINI->ReadBool(section, "Repairable",     IsRepairable);
    IsRepairable_    = IsRepairable;
    IsCanBeRepaired_ = IsRepairable;
    IsUngarrisonable = pINI->ReadBool(section, "Ungarrisonable", IsUngarrisonable);
    IsUngarrisonable_ = IsUngarrisonable;

    // ------------------------------------------------------------------
    // CanBeXxx interaction flags
    // ------------------------------------------------------------------
    IsCanC4            = pINI->ReadBool(section, "CanC4",          IsCanC4);
    IsCanBeOccupied    = pINI->ReadBool(section, "CanBeOccupied",  IsCanBeOccupied);
    IsCanBeDriven      = pINI->ReadBool(section, "CanBeDriven",    IsCanBeDriven);
    IsCanBeCaptured    = pINI->ReadBool(section, "CanBeCaptured",  IsCanBeCaptured);
    IsCanBePowered     = pINI->ReadBool(section, "CanBePowered",   IsCanBePowered);
    IsCanBeDestroyed   = pINI->ReadBool(section, "CanBeDestroyed", IsCanBeDestroyed);
    IsCanBeDamaged     = pINI->ReadBool(section, "CanBeDamaged",   IsCanBeDamaged);
    IsCanBeInfiltrated = pINI->ReadBool(section, "CanBeInfiltrated", IsCanBeInfiltrated);
    IsCanBeSpied       = pINI->ReadBool(section, "CanBeSpied",     IsCanBeSpied);
    IsCanBeSabotaged   = pINI->ReadBool(section, "CanBeSabotaged", IsCanBeSabotaged);
    IsCanBeStolen      = pINI->ReadBool(section, "CanBeStolen",    IsCanBeStolen);
    IsCanBeHijacked    = pINI->ReadBool(section, "CanBeHijacked",  IsCanBeHijacked);

    // ------------------------------------------------------------------
    // Plug / drain / rig
    // ------------------------------------------------------------------
    IsPlug             = pINI->ReadBool(section, "Plug",             IsPlug);
    IsDrain            = pINI->ReadBool(section, "Drain",            IsDrain);
    IsDrainable        = pINI->ReadBool(section, "Drainable",        IsDrainable);
    IsRig              = pINI->ReadBool(section, "Rig",              IsRig);
    IsRigOwner         = pINI->ReadBool(section, "RigOwner",         IsRigOwner);
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
    IsAutoFire    = pINI->ReadBool(section, "AutoFire",    IsAutoFire);
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
    IsInfiltratable    = pINI->ReadBool(section, "Infiltratable",    IsInfiltratable);
    IsStealthy         = pINI->ReadBool(section, "Stealthy",         IsStealthy);
    IsHealable         = pINI->ReadBool(section, "Healable",         IsHealable);
    IsTilter           = pINI->ReadBool(section, "Tilter",           IsTilter);
    IsToProtect        = pINI->ReadBool(section, "ToProtect",        IsToProtect);
    IsNominal          = pINI->ReadBool(section, "Nominal",          IsNominal);
    IsRadarInvisible   = pINI->ReadBool(section, "RadarInvisible",   IsRadarInvisible);
    IsDontScore        = pINI->ReadBool(section, "DontScore",        IsDontScore);
    IsNoThreat         = pINI->ReadBool(section, "NoThreat",         IsNoThreat);
    IsSensorsSight     = pINI->ReadBool(section, "SensorsSight",     IsSensorsSight);
    IsHunterSeeker     = pINI->ReadBool(section, "HunterSeeker",     IsHunterSeeker);
    IsIvan             = pINI->ReadBool(section, "Ivan",             IsIvan);
    IsLeader           = pINI->ReadBool(section, "Leader",           IsLeader);
    IsCarryall         = pINI->ReadBool(section, "Carryall",         IsCarryall);
    IsTrain            = pINI->ReadBool(section, "Train",            IsTrain);
    IsConsideredAircraft = pINI->ReadBool(section, "ConsideredAircraft", IsConsideredAircraft);
    IsConsideredVehicle  = pINI->ReadBool(section, "ConsideredVehicle",  IsConsideredVehicle);

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

    IsScanner            = pINI->ReadBool(section, "Scanner",  IsScanner);
    IsSensor             = pINI->ReadBool(section, "Sensor",   IsSensor);
    IsDetector           = pINI->ReadBool(section, "Detector", IsDetector);
    IsSensors            = pINI->ReadBool(section, "Sensors",  IsSensors);
    IsPreventAttackMove  = pINI->ReadBool(section, "PreventAttackMove", IsPreventAttackMove);

    // ------------------------------------------------------------------
    // Undeployable / simple-deployer / firebase
    // ------------------------------------------------------------------
    IsUndeployable_     = pINI->ReadBool(section, "Undeployable",   IsUndeployable_);
    IsSimpleDeployer_   = pINI->ReadBool(section, "SimpleDeployer", IsSimpleDeployer_);
    IsFirebase_         = pINI->ReadBool(section, "Firebase",       IsFirebase_);
    IsUndeployable__    = IsUndeployable_;
    IsUndeployableMember= IsUndeployable_;

    // Building animation count - read from art INI normally; allow override.
    BuildingAnimCount = pINI->ReadInteger(section, "BuildingAnimCount",
                                          BuildingAnimCount);
    BuildingAnimCount_ = BuildingAnimCount;

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
    pINI->WriteBool(section, "SimpleDeployer",   IsSimpleDeployer_);
    pINI->WriteBool(section, "Firebase",         IsFirebase_);

    pINI->WriteBool(section, "CanC4",            IsCanC4);
    pINI->WriteBool(section, "CanBeOccupied",    IsCanBeOccupied);
    pINI->WriteBool(section, "CanBeDriven",      IsCanBeDriven);
    pINI->WriteBool(section, "CanBeCaptured",    IsCanBeCaptured);
    pINI->WriteBool(section, "CanBePowered",     IsCanBePowered);
    pINI->WriteBool(section, "CanBeDestroyed",   IsCanBeDestroyed);
    pINI->WriteBool(section, "CanBeDamaged",     IsCanBeDamaged);
    pINI->WriteBool(section, "CanBeInfiltrated", IsCanBeInfiltrated);
    pINI->WriteBool(section, "CanBeSpied",       IsCanBeSpied);
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
