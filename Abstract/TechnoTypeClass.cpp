#include <Abstract/TechnoTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>
#include <Houses/HouseClass.h>
#include <FileFormats/SHP.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>

// ============================================================================
// TechnoTypeClass.cpp
//
//  TechnoTypeClass is the shared base for every "techno" type - anything
//  that has weapons, armor, sight, and a producer (buildings, infantry,
//  vehicles, aircraft).  It inherits the object-type fields from
//  ObjectTypeClass and adds:
//
//    * Armor / Speed / ROT / TurretROT
//    * Weapon slots (primary, secondary, ... up to 18)
//    * Veteran / elite ability bitfields
//    * Cloak, deploy, firewall, turret, voxel flags
//    * Passenger / transport configuration
//    * Crew escape configuration
//    * Pip draw configuration
//    * Crash/death weapon
//
//  This file implements:
//    * Static Array plumbing
//    * Constructor / destructor
//    * LoadFromINI / SaveToINI for the techno-specific fields
//    * ComputeCRC / GetCRC
//    * The boolean accessors (HasTurret, CanCloak, IsVoxel, ...)
//    * GetWeapon / GetWeaponCount
//    * Get_Max_Speed / Get_Armor / Is_Two_Shooter / Get_Cameo_Index
//    * Get_Display_Coords / Is_Veteran / Is_Crewed
//    * Resolve_SHP_References / Get_Image_Size / Get_Build_Queue_Type
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<TechnoTypeClass*>* TechnoTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================
void TechnoTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<TechnoTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<TechnoTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<TechnoTypeClass*>();
    }
}

void TechnoTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<TechnoTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

TechnoTypeClass* TechnoTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        TechnoTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

TechnoTypeClass* TechnoTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 TechnoTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

void TechnoTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        TechnoTypeClass* item = Array->Items[i];
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

TechnoTypeClass::TechnoTypeClass() noexcept
    : ObjectTypeClass(noinit)
{
    Speed                 = 0;
    ArmorType             = Armor::None;
    SpeedTypeVal          = SpeedType::Slow;
    MoveZone              = MovementZone::Normal;
    Factory               = AbstractType::None;
    ROT                   = 0;
    WeaponCount           = 0;
    HasTurret_            = false;
    CanCloak_             = false;
    IsVoxel_              = false;
    HasDeployer_          = false;
    HasUndeployer_        = false;
    HasFirewall_          = false;
    IsBuildable_          = true;
    IsTrainable_          = false;
    HasPassengers         = false;
    Passengers            = 0;
    OpenTopped            = 0;
    SizeLimit             = 0;
    CloakSpeed            = 0.0;
    CloakRadius           = 0.0;
    CrewCount             = 0;
    Crew                  = nullptr;
    CrewType              = AbstractType::None;
    Ammo                  = -1;
    NoAmmo                = false;
    PipScale              = 0;
    OccupyWeaponCount     = 0;
    OccupyWeaponRangeBonus = 0;
    IsHarvester           = false;
    IsWeeder              = false;
    IsResourceGatherer    = false;
    IsUndeployable        = false;
    IsBombable            = false;
    IsAutoFire            = false;
    IsGuardRange          = false;
    IsAggressive          = false;
    IsSelectable_         = true;
    IsInsignificant_      = false;
    IsLegalTarget_        = true;
    IsImmune_             = false;
    IsLegalDamsel_        = false;
    IsUnsellable          = false;
    IsRepairable          = true;
    IsSellable            = true;
    IsPowered             = false;
    IsScanner             = false;
    IsSensor              = false;
    IsDetector            = false;
    IsSensors             = false;
    IsPreventAttackMove   = false;
    IsNaval               = false;
    IsLand                = true;
    IsAir                 = false;
    IsOrganic             = false;
    IsNeutral             = false;
    IsInfiltratable       = false;
    IsStealthy            = false;
    IsHealable            = false;
    IsSelectable_old      = true;
    IsTilter              = false;
    IsToProtect           = false;
    IsNominal             = false;
    IsRadarInvisible      = false;
    IsDontScore           = false;
    IsNoThreat            = false;
    IsSensorsSight        = false;
    IsHunterSeeker        = false;
    IsIvan                = false;
    IsLeader              = false;
    IsCarryall            = false;
    IsTrain               = false;
    IsConsideredAircraft  = false;
    IsConsideredVehicle   = true;
    IsSimpleDeployer      = false;
    IsFirebase            = false;
    IsSonic               = false;
    IsVan                 = false;
    IsBalloonHover        = false;
    IsCyborg              = false;
    IsNotHuman            = true;
    IsImmuneToPsionics    = false;
    IsImmuneToPoison      = false;
    IsImmuneToRadiation   = false;
    IsImmuneToBerserk     = false;
    IsImmuneToEMP         = false;
    IsCrushable           = false;
    IsCrushable2          = false;
    IsTeleporter          = false;
    IsChrono              = false;
    IsBomb                = false;
    IsCow                 = false;
    IsDog                 = false;
    IsBoris               = false;
    IsArmed               = false;
    IsMissileSpawn        = false;
    ThreatPosedValue      = 0.0f;
    DeathWeaponIndex      = -1;
    WeaponCharge          = 0;
    IsFake                = false;
    IsDisableable         = false;
    IsCanBeSuppressed     = false;
    IsCanBeOccupied       = false;
    IsCanBeDriven         = false;
    IsCanBeCaptured       = false;
    IsCanBeRepaired       = true;
    IsCanBeSold           = true;
    IsCanBePowered        = false;
    IsCanBeDestroyed      = true;
    IsCanBeDamaged        = true;
    IsCanBeInfiltrated    = false;
    IsCanBeSpied          = false;
    IsCanBeSabotaged      = false;
    IsCanBeStolen         = false;
    IsCanBeHijacked       = false;

    Deployer              = false;
    Undeployer            = false;
    Firewall              = false;
    Turret                = false;
    Cloak                 = false;
    Voxel                 = false;

    SightRange            = 0;
    GuardRange            = 0;
    Strength              = 0;
    BuildCost             = 0;
    BuildTime             = 0;
    RepairCost            = 0;
    RefundPercent         = 50;
    VeteranRatio          = 0;
    InitialVeterancy      = 0;
    TurretROT             = 0;
    IdleTimer             = 0;
    IsCrewed_             = false;

    Cameo[0]              = '\0';
    ImageFile[0]          = '\0';
    CameoShape            = nullptr;
    ImageShape            = nullptr;
    ImageSize             = Point2D(0, 0);

    for (int32 i = 0; i < 4; ++i)
    {
        VeteranAbilities[i] = 0;
        EliteAbilities[i]   = 0;
    }

    std::memset(Weapons, 0, sizeof(Weapons));
}

// ============================================================================
// Destructor
// ============================================================================

TechnoTypeClass::~TechnoTypeClass()
{
    // SHP references are owned by the art system, not by the type class.
}

// ============================================================================
// RTTI / size / ID
// ============================================================================

AbstractType TechnoTypeClass::GetAbstractDerivationID() const
{
    return AbstractType::TechnoType;
}

bool TechnoTypeClass::HasThisID(const char* pID) const
{
    if (pID == nullptr)
        return false;
    return _strcmpi(this->ID, pID) == 0;
}

int32 TechnoTypeClass::Size() const
{
    return sizeof(TechnoTypeClass);
}

AbstractType TechnoTypeClass::GetClassID() const
{
    return AbstractType::TechnoType;
}

const char* TechnoTypeClass::get_ID() const
{
    return this->ID;
}

const wchar_t* TechnoTypeClass::GetUIName() const
{
    return this->UIName;
}

// ============================================================================
// Boolean accessors
// ============================================================================

bool TechnoTypeClass::IsBuildable() const      { return IsBuildable_; }
bool TechnoTypeClass::IsTrainable() const      { return IsTrainable_; }
bool TechnoTypeClass::IsSelectable() const     { return IsSelectable_; }
bool TechnoTypeClass::IsLegalTarget() const    { return IsLegalTarget_; }
bool TechnoTypeClass::IsImmune() const         { return IsImmune_; }
bool TechnoTypeClass::IsLegalDamsel() const    { return IsLegalDamsel_; }
bool TechnoTypeClass::IsInsignificant() const  { return IsInsignificant_; }

bool TechnoTypeClass::HasTurret() const        { return HasTurret_ || Turret; }
bool TechnoTypeClass::CanCloak() const         { return CanCloak_ || Cloak; }
bool TechnoTypeClass::IsVoxel() const          { return IsVoxel_ || Voxel; }
bool TechnoTypeClass::HasDeployer() const      { return HasDeployer_ || Deployer; }
bool TechnoTypeClass::HasUndeployer() const    { return HasUndeployer_ || Undeployer; }
bool TechnoTypeClass::HasFirewall() const      { return HasFirewall_ || Firewall; }

int32 TechnoTypeClass::GetWeaponCount() const
{
    return WeaponCount;
}

WeaponStruct* TechnoTypeClass::GetWeapon(int32 index) const
{
    if (index < 0 || index >= WeaponCount || index >= 18)
        return nullptr;
    return const_cast<WeaponStruct*>(&Weapons[index]);
}

// ============================================================================
// Extended accessors
// ============================================================================

int32 TechnoTypeClass::Get_Max_Speed() const
{
    // The full binary consults the SpeedType table in RulesClass to map the
    // SpeedType enum to a leptons-per-frame value.  We approximate by
    // scaling the raw Speed field by 16 (the original uses 256/16 cell
    // conversion).
    if (Speed > 0)
        return Speed;

    switch (SpeedTypeVal)
    {
        case SpeedType::VeryFast: return 16;
        case SpeedType::Fast:     return 12;
        case SpeedType::Medium:   return 8;
        case SpeedType::Slow:     return 4;
        default:                  return 0;
    }
}

Armor TechnoTypeClass::Get_Armor() const
{
    return ArmorType;
}

bool TechnoTypeClass::Is_Two_Shooter() const
{
    // A "two shooter" fires both primary and secondary weapons in a single
    // attack cycle.  The full binary consults the FireOnce flag on the
    // secondary weapon; here we approximate by checking whether a secondary
    // weapon slot is populated.
    return WeaponCount >= 2;
}

int32 TechnoTypeClass::Get_Cameo_Index() const
{
    // The full binary resolves the cameo through the art INI and returns a
    // shape-table index.  Here we return -1 to indicate "not resolved" and
    // let the sidebar fall back to Get_Cameo_Data.
    return -1;
}

CoordStruct TechnoTypeClass::Get_Display_Coords() const
{
    // Type classes don't have a map position; return the origin.  The
    // sidebar uses this for the build-preview overlay.
    return CoordStruct(0, 0, 0);
}

bool TechnoTypeClass::Is_Veteran() const
{
    return InitialVeterancy > 0;
}

bool TechnoTypeClass::Is_Crewed() const
{
    return IsCrewed_ || CrewCount > 0;
}

Point2D TechnoTypeClass::Get_Image_Size() const
{
    return ImageSize;
}

AbstractType TechnoTypeClass::Get_Build_Queue_Type() const
{
    // Returns the AbstractType of the producer that builds this techno.
    // Defaults to None - subclasses override.
    return Factory;
}

// ============================================================================
// LoadFromINI - parses the techno-specific fields
// ============================================================================

bool TechnoTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // Chain the parent first so Cost / TechLevel / Sight / etc. are loaded.
    ObjectTypeClass::LoadFromINI(pINI);

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // ------------------------------------------------------------------
    // Combat attributes
    // ------------------------------------------------------------------
    Strength      = pINI->ReadInteger(section, "Strength",   Strength);
    SightRange    = pINI->ReadInteger(section, "Sight",      SightRange);
    GuardRange    = pINI->ReadInteger(section, "GuardRange", GuardRange);
    BuildCost     = pINI->ReadInteger(section, "Cost",       BuildCost);
    BuildTime     = pINI->ReadInteger(section, "BuildTime",  BuildTime);
    RepairCost    = pINI->ReadInteger(section, "RepairCost", RepairCost);
    RefundPercent = pINI->ReadInteger(section, "RefundPercent", RefundPercent);

    // Sync the inherited Cost / Sight fields so callers using the parent
    // accessors see the same value.
    Cost  = BuildCost;
    Sight = SightRange;
    MaxStrength = Strength;

    // ------------------------------------------------------------------
    // Armor
    // ------------------------------------------------------------------
    char armorBuf[32];
    pINI->ReadString(section, "Armor", "None", armorBuf, sizeof(armorBuf));
    if (!_strcmpi(armorBuf, "None"))         ArmorType = Armor::None;
    else if (!_strcmpi(armorBuf, "Flak"))    ArmorType = Armor::Flak;
    else if (!_strcmpi(armorBuf, "Plate"))   ArmorType = Armor::Plate;
    else if (!_strcmpi(armorBuf, "Light"))   ArmorType = Armor::Light;
    else if (!_strcmpi(armorBuf, "Medium"))  ArmorType = Armor::Medium;
    else if (!_strcmpi(armorBuf, "Heavy"))   ArmorType = Armor::Heavy;
    else if (!_strcmpi(armorBuf, "Wood"))    ArmorType = Armor::Wood;
    else if (!_strcmpi(armorBuf, "Steel"))   ArmorType = Armor::Steel;
    else if (!_strcmpi(armorBuf, "Concrete"))ArmorType = Armor::Concrete;
    else if (!_strcmpi(armorBuf, "Drone"))   ArmorType = Armor::Drone;
    else if (!_strcmpi(armorBuf, "Special_1")) ArmorType = Armor::Special_1;
    else                                     ArmorType = Armor::None;

    // ------------------------------------------------------------------
    // Speed / ROT
    // ------------------------------------------------------------------
    Speed    = pINI->ReadInteger(section, "Speed", Speed);
    ROT      = pINI->ReadInteger(section, "ROT",   ROT);
    TurretROT= pINI->ReadInteger(section, "TurretROT", TurretROT);
    IdleTimer= pINI->ReadInteger(section, "IdleTimer", IdleTimer);

    char speedBuf[32];
    pINI->ReadString(section, "SpeedType", "Slow", speedBuf, sizeof(speedBuf));
    if (!_strcmpi(speedBuf, "Slow"))         SpeedTypeVal = SpeedType::Slow;
    else if (!_strcmpi(speedBuf, "Medium"))  SpeedTypeVal = SpeedType::Medium;
    else if (!_strcmpi(speedBuf, "Fast"))    SpeedTypeVal = SpeedType::Fast;
    else if (!_strcmpi(speedBuf, "VeryFast"))SpeedTypeVal = SpeedType::VeryFast;
    else                                     SpeedTypeVal = SpeedType::Slow;

    char zoneBuf[32];
    pINI->ReadString(section, "MovementZone", "Normal",
                     zoneBuf, sizeof(zoneBuf));
    if (!_strcmpi(zoneBuf, "Normal"))                MoveZone = MovementZone::Normal;
    else if (!_strcmpi(zoneBuf, "Crusher"))          MoveZone = MovementZone::Crusher;
    else if (!_strcmpi(zoneBuf, "Destroyer"))        MoveZone = MovementZone::Destroyer;
    else if (!_strcmpi(zoneBuf, "Water"))            MoveZone = MovementZone::Water;
    else if (!_strcmpi(zoneBuf, "WaterBeach"))       MoveZone = MovementZone::WaterBeach;
    else if (!_strcmpi(zoneBuf, "Amphibious"))       MoveZone = MovementZone::Amphibious;
    else if (!_strcmpi(zoneBuf, "AmphibiousCrusher"))MoveZone = MovementZone::AmphibiousCrusher;
    else if (!_strcmpi(zoneBuf, "AmphibiousDestroyer"))MoveZone = MovementZone::AmphibiousDestroyer;
    else if (!_strcmpi(zoneBuf, "Fly"))              MoveZone = MovementZone::Fly;
    else                                             MoveZone = MovementZone::Normal;

    // ------------------------------------------------------------------
    // Weapons
    // ------------------------------------------------------------------
    WeaponCount = pINI->ReadInteger(section, "WeaponCount", 0);
    if (WeaponCount < 0) WeaponCount = 0;
    if (WeaponCount > 18) WeaponCount = 18;

    // Primary / secondary weapon slots are referenced by name and resolved
    // against WeaponTypeClass::Array.  The full binary stores a WeaponStruct
    // per slot; here we zero-init them so the resolver can fill them later.
    for (int32 i = 0; i < WeaponCount; ++i)
    {
        std::memset(&Weapons[i], 0, sizeof(WeaponStruct));
    }

    DeathWeaponIndex = pINI->ReadInteger(section, "DeathWeapon", -1);
    WeaponCharge     = pINI->ReadInteger(section, "WeaponCharge", 0);

    // ------------------------------------------------------------------
    // Veteran / elite abilities
    // ------------------------------------------------------------------
    VeteranRatio     = pINI->ReadInteger(section, "VeteranRatio", 0);
    InitialVeterancy = pINI->ReadInteger(section, "VeteranAbilities", InitialVeterancy);

    // VeteranAbilities / EliteAbilities are bitfields; the INI uses comma-
    // separated flag names.  Read each slot individually.
    for (int32 i = 0; i < 4; ++i)
    {
        char key[24];
        sprintf_s(key, sizeof(key), "VeteranAbilities%d", i);
        VeteranAbilities[i] = pINI->ReadInteger(section, key, 0);

        sprintf_s(key, sizeof(key), "EliteAbilities%d", i);
        EliteAbilities[i] = pINI->ReadInteger(section, key, 0);
    }

    // ------------------------------------------------------------------
    // Cloak / deploy / firewall / turret / voxel flags
    // ------------------------------------------------------------------
    HasTurret_     = pINI->ReadBool(section, "Turret",     HasTurret_);
    CanCloak_      = pINI->ReadBool(section, "Cloakable",  CanCloak_);
    IsVoxel_       = pINI->ReadBool(section, "Voxel",      IsVoxel_);
    HasDeployer_   = pINI->ReadBool(section, "Deployer",   HasDeployer_);
    HasUndeployer_ = pINI->ReadBool(section, "Undeployer", HasUndeployer_);
    HasFirewall_   = pINI->ReadBool(section, "Firewall",   HasFirewall_);

    Turret    = HasTurret_;
    Cloak     = CanCloak_;
    Voxel     = IsVoxel_;
    Deployer  = HasDeployer_;
    Undeployer= HasUndeployer_;
    Firewall  = HasFirewall_;

    CloakSpeed  = pINI->ReadFloat(section, "CloakSpeed",  0.0);
    CloakRadius = pINI->ReadFloat(section, "CloakRadius", 0.0);

    // ------------------------------------------------------------------
    // Passengers / transport
    // ------------------------------------------------------------------
    HasPassengers = pINI->ReadBool(section, "Passengers", HasPassengers);
    Passengers    = pINI->ReadInteger(section, "Passengers", Passengers);
    OpenTopped    = pINI->ReadInteger(section, "OpenTopped", OpenTopped);
    SizeLimit     = pINI->ReadInteger(section, "SizeLimit",  SizeLimit);

    // ------------------------------------------------------------------
    // Crew escape
    // ------------------------------------------------------------------
    CrewCount = pINI->ReadInteger(section, "Crewed", 0);
    IsCrewed_ = (CrewCount > 0);

    // ------------------------------------------------------------------
    // Ammo
    // ------------------------------------------------------------------
    Ammo   = pINI->ReadInteger(section, "Ammo",   Ammo);
    NoAmmo = pINI->ReadBool(section, "NoAmmo", NoAmmo);
    PipScale = pINI->ReadInteger(section, "PipScale", PipScale);

    // ------------------------------------------------------------------
    // Occupy weapons (garrison)
    // ------------------------------------------------------------------
    OccupyWeaponCount      = pINI->ReadInteger(section, "OccupyWeaponCount",
                                                OccupyWeaponCount);
    OccupyWeaponRangeBonus = pINI->ReadInteger(section, "OccupyWeaponRangeBonus",
                                                OccupyWeaponRangeBonus);

    // ------------------------------------------------------------------
    // Economy / build flags
    // ------------------------------------------------------------------
    IsBuildable_     = pINI->ReadBool(section, "Buildable",     IsBuildable_);
    IsTrainable_     = pINI->ReadBool(section, "Trainable",     IsTrainable_);
    IsSelectable_    = pINI->ReadBool(section, "Selectable",    IsSelectable_);
    IsInsignificant_ = pINI->ReadBool(section, "Insignificant", IsInsignificant_);
    IsLegalTarget_   = pINI->ReadBool(section, "LegalTarget",   IsLegalTarget_);
    IsImmune_        = pINI->ReadBool(section, "Immune",        IsImmune_);
    IsLegalDamsel_   = pINI->ReadBool(section, "LegalDamsel",   IsLegalDamsel_);
    IsUnsellable     = pINI->ReadBool(section, "Unsellable",    IsUnsellable);
    IsRepairable     = pINI->ReadBool(section, "Repairable",    IsRepairable);
    IsSellable       = pINI->ReadBool(section, "Sellable",      IsSellable);
    IsPowered        = pINI->ReadBool(section, "Powered",       IsPowered);

    // ------------------------------------------------------------------
    // Sensor / detection flags
    // ------------------------------------------------------------------
    IsScanner  = pINI->ReadBool(section, "Scanner",  IsScanner);
    IsSensor   = pINI->ReadBool(section, "Sensor",   IsSensor);
    IsDetector = pINI->ReadBool(section, "Detector", IsDetector);
    IsSensors  = pINI->ReadBool(section, "Sensors",  IsSensors);
    IsSensorsSight = pINI->ReadBool(section, "SensorsSight", IsSensorsSight);

    // ------------------------------------------------------------------
    // Movement flags
    // ------------------------------------------------------------------
    IsNaval              = pINI->ReadBool(section, "Naval",              IsNaval);
    IsLand               = pINI->ReadBool(section, "Land",               IsLand);
    IsAir                = pINI->ReadBool(section, "Air",                IsAir);
    IsOrganic            = pINI->ReadBool(section, "Organic",            IsOrganic);
    IsPreventAttackMove  = pINI->ReadBool(section, "PreventAttackMove",  IsPreventAttackMove);
    IsBalloonHover       = pINI->ReadBool(section, "BalloonHover",       IsBalloonHover);
    IsConsideredAircraft = pINI->ReadBool(section, "ConsideredAircraft", IsConsideredAircraft);
    IsConsideredVehicle  = pINI->ReadBool(section, "ConsideredVehicle",  IsConsideredVehicle);

    // ------------------------------------------------------------------
    // Combat-immunity flags
    // ------------------------------------------------------------------
    IsImmuneToPsionics  = pINI->ReadBool(section, "ImmuneToPsionics",  IsImmuneToPsionics);
    IsImmuneToPoison    = pINI->ReadBool(section, "ImmuneToPoison",    IsImmuneToPoison);
    IsImmuneToRadiation = pINI->ReadBool(section, "ImmuneToRadiation", IsImmuneToRadiation);
    IsImmuneToBerserk   = pINI->ReadBool(section, "ImmuneToBerserk",   IsImmuneToBerserk);
    IsImmuneToEMP       = pINI->ReadBool(section, "ImmuneToEMP",       IsImmuneToEMP);

    // ------------------------------------------------------------------
    // Crush / teleport / chrono / bomb
    // ------------------------------------------------------------------
    IsCrushable   = pINI->ReadBool(section, "Crushable",   IsCrushable);
    IsCrushable2  = pINI->ReadBool(section, "Crushable2",  IsCrushable2);
    IsTeleporter  = pINI->ReadBool(section, "Teleporter",  IsTeleporter);
    IsChrono      = pINI->ReadBool(section, "Chrono",      IsChrono);
    IsBomb        = pINI->ReadBool(section, "Bomb",        IsBomb);

    // ------------------------------------------------------------------
    // Special-unit flags
    // ------------------------------------------------------------------
    IsCow            = pINI->ReadBool(section, "Cow",            IsCow);
    IsDog            = pINI->ReadBool(section, "Dog",            IsDog);
    IsBoris          = pINI->ReadBool(section, "Boris",          IsBoris);
    IsArmed          = pINI->ReadBool(section, "Armed",          IsArmed);
    IsMissileSpawn   = pINI->ReadBool(section, "MissileSpawn",   IsMissileSpawn);
    IsIvan           = pINI->ReadBool(section, "Ivan",           IsIvan);
    IsLeader         = pINI->ReadBool(section, "Leader",         IsLeader);
    IsCarryall       = pINI->ReadBool(section, "Carryall",       IsCarryall);
    IsTrain          = pINI->ReadBool(section, "Train",          IsTrain);
    IsSimpleDeployer = pINI->ReadBool(section, "SimpleDeployer", IsSimpleDeployer);
    IsFirebase       = pINI->ReadBool(section, "Firebase",       IsFirebase);
    IsSonic          = pINI->ReadBool(section, "Sonic",          IsSonic);
    IsVan            = pINI->ReadBool(section, "Van",            IsVan);
    IsCyborg         = pINI->ReadBool(section, "Cyborg",         IsCyborg);
    IsNotHuman       = pINI->ReadBool(section, "NotHuman",       IsNotHuman);
    IsFake           = pINI->ReadBool(section, "Fake",           IsFake);
    IsDisableable    = pINI->ReadBool(section, "Disableable",    IsDisableable);

    // ------------------------------------------------------------------
    // Threat / score
    // ------------------------------------------------------------------
    ThreatPosedValue = pINI->ReadFloat(section, "ThreatPosed", ThreatPosedValue);
    Score            = pINI->ReadInteger(section, "Score", Score);

    // ------------------------------------------------------------------
    // Various interaction flags
    // ------------------------------------------------------------------
    IsNeutral         = pINI->ReadBool(section, "Neutral",         IsNeutral);
    IsInfiltratable   = pINI->ReadBool(section, "Infiltratable",   IsInfiltratable);
    IsStealthy        = pINI->ReadBool(section, "Stealthy",        IsStealthy);
    IsHealable        = pINI->ReadBool(section, "Healable",        IsHealable);
    IsTilter          = pINI->ReadBool(section, "Tilter",          IsTilter);
    IsToProtect       = pINI->ReadBool(section, "ToProtect",       IsToProtect);
    IsNominal         = pINI->ReadBool(section, "Nominal",         IsNominal);
    IsRadarInvisible  = pINI->ReadBool(section, "RadarInvisible",  IsRadarInvisible);
    IsDontScore       = pINI->ReadBool(section, "DontScore",       IsDontScore);
    IsNoThreat        = pINI->ReadBool(section, "NoThreat",        IsNoThreat);
    IsHunterSeeker    = pINI->ReadBool(section, "HunterSeeker",    IsHunterSeeker);

    IsCanBeSuppressed  = pINI->ReadBool(section, "CanSuppressed",  IsCanBeSuppressed);
    IsCanBeOccupied    = pINI->ReadBool(section, "CanBeOccupied",  IsCanBeOccupied);
    IsCanBeDriven      = pINI->ReadBool(section, "CanBeDriven",    IsCanBeDriven);
    IsCanBeCaptured    = pINI->ReadBool(section, "CanBeCaptured",  IsCanBeCaptured);
    IsCanBeRepaired    = pINI->ReadBool(section, "CanBeRepaired",  IsCanBeRepaired);
    IsCanBeSold        = pINI->ReadBool(section, "CanBeSold",      IsCanBeSold);
    IsCanBePowered     = pINI->ReadBool(section, "CanBePowered",   IsCanBePowered);
    IsCanBeDestroyed   = pINI->ReadBool(section, "CanBeDestroyed", IsCanBeDestroyed);
    IsCanBeDamaged     = pINI->ReadBool(section, "CanBeDamaged",   IsCanBeDamaged);
    IsCanBeInfiltrated = pINI->ReadBool(section, "CanBeInfiltrated", IsCanBeInfiltrated);
    IsCanBeSpied       = pINI->ReadBool(section, "CanBeSpied",     IsCanBeSpied);
    IsCanBeSabotaged   = pINI->ReadBool(section, "CanBeSabotaged", IsCanBeSabotaged);
    IsCanBeStolen      = pINI->ReadBool(section, "CanBeStolen",    IsCanBeStolen);
    IsCanBeHijacked    = pINI->ReadBool(section, "CanBeHijacked",  IsCanBeHijacked);

    IsHarvester        = pINI->ReadBool(section, "Harvester",        IsHarvester);
    IsWeeder           = pINI->ReadBool(section, "Weeder",           IsWeeder);
    IsResourceGatherer = pINI->ReadBool(section, "ResourceGatherer", IsResourceGatherer);
    IsUndeployable     = pINI->ReadBool(section, "Undeployable",     IsUndeployable);
    IsBombable         = pINI->ReadBool(section, "Bombable",         IsBombable);
    IsAutoFire         = pINI->ReadBool(section, "AutoFire",         IsAutoFire);
    IsGuardRange       = pINI->ReadBool(section, "GuardRange",       IsGuardRange);
    IsAggressive       = pINI->ReadBool(section, "Aggressive",       IsAggressive);

    // ------------------------------------------------------------------
    // Art references
    // ------------------------------------------------------------------
    char imageBuf[64];
    pINI->ReadString(section, "Image", "", imageBuf, sizeof(imageBuf));
    if (imageBuf[0] != '\0')
    {
        int32 j = 0;
        while (imageBuf[j] != '\0' && j < static_cast<int32>(sizeof(ImageFile) - 1))
        {
            ImageFile[j] = imageBuf[j];
            ++j;
        }
        ImageFile[j] = '\0';
    }
    else if (ID[0] != '\0')
    {
        // Default the image name to the type ID.
        int32 j = 0;
        while (ID[j] != '\0' && j < static_cast<int32>(sizeof(ImageFile) - 1))
        {
            ImageFile[j] = ID[j];
            ++j;
        }
        ImageFile[j] = '\0';
    }

    char cameoBuf[64];
    pINI->ReadString(section, "Cameo", "", cameoBuf, sizeof(cameoBuf));
    if (cameoBuf[0] != '\0')
    {
        int32 j = 0;
        while (cameoBuf[j] != '\0' && j < static_cast<int32>(sizeof(Cameo) - 1))
        {
            Cameo[j] = cameoBuf[j];
            ++j;
        }
        Cameo[j] = '\0';
    }

    // ------------------------------------------------------------------
    // Factory type (what producer builds this techno)
    // ------------------------------------------------------------------
    char factoryBuf[32];
    pINI->ReadString(section, "Factory", "None", factoryBuf, sizeof(factoryBuf));
    if (!_strcmpi(factoryBuf, "Building"))   Factory = AbstractType::Building;
    else if (!_strcmpi(factoryBuf, "Infantry")) Factory = AbstractType::Infantry;
    else if (!_strcmpi(factoryBuf, "Unit"))  Factory = AbstractType::Unit;
    else if (!_strcmpi(factoryBuf, "Aircraft")) Factory = AbstractType::Aircraft;
    else                                     Factory = AbstractType::None;

    return true;
}

// ============================================================================
// SaveToINI
// ============================================================================

bool TechnoTypeClass::SaveToINI(CCINIClass* pINI) const
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // Chain the parent.
    const_cast<TechnoTypeClass*>(this)->ObjectTypeClass::SaveToINI(pINI);

    pINI->WriteInteger(section, "Strength",     Strength);
    pINI->WriteInteger(section, "GuardRange",   GuardRange);
    pINI->WriteInteger(section, "BuildTime",    BuildTime);
    pINI->WriteInteger(section, "RepairCost",   RepairCost);
    pINI->WriteInteger(section, "RefundPercent",RefundPercent);
    pINI->WriteInteger(section, "ROT",          ROT);
    pINI->WriteInteger(section, "TurretROT",    TurretROT);
    pINI->WriteInteger(section, "IdleTimer",    IdleTimer);
    pINI->WriteInteger(section, "WeaponCount",  WeaponCount);
    pINI->WriteInteger(section, "DeathWeapon",  DeathWeaponIndex);
    pINI->WriteInteger(section, "WeaponCharge", WeaponCharge);
    pINI->WriteInteger(section, "VeteranRatio", VeteranRatio);
    pINI->WriteInteger(section, "Ammo",         Ammo);
    pINI->WriteInteger(section, "PipScale",     PipScale);
    pINI->WriteInteger(section, "OccupyWeaponCount",       OccupyWeaponCount);
    pINI->WriteInteger(section, "OccupyWeaponRangeBonus",  OccupyWeaponRangeBonus);
    pINI->WriteFloat(section,  "CloakSpeed",   CloakSpeed);
    pINI->WriteFloat(section,  "CloakRadius",  CloakRadius);
    pINI->WriteFloat(section,  "ThreatPosed",  ThreatPosedValue);

    // Armor
    const char* armorName = "None";
    switch (ArmorType)
    {
        case Armor::Flak:     armorName = "Flak";     break;
        case Armor::Plate:    armorName = "Plate";    break;
        case Armor::Light:    armorName = "Light";    break;
        case Armor::Medium:   armorName = "Medium";   break;
        case Armor::Heavy:    armorName = "Heavy";    break;
        case Armor::Wood:     armorName = "Wood";     break;
        case Armor::Steel:    armorName = "Steel";    break;
        case Armor::Concrete: armorName = "Concrete"; break;
        case Armor::Drone:    armorName = "Drone";    break;
        case Armor::Special_1:armorName = "Special_1";break;
        default:              armorName = "None";     break;
    }
    pINI->WriteString(section, "Armor", armorName);

    // SpeedType
    const char* speedName = "Slow";
    switch (SpeedTypeVal)
    {
        case SpeedType::Medium:   speedName = "Medium";   break;
        case SpeedType::Fast:     speedName = "Fast";     break;
        case SpeedType::VeryFast: speedName = "VeryFast"; break;
        default:                  speedName = "Slow";     break;
    }
    pINI->WriteString(section, "SpeedType", speedName);

    // MovementZone
    const char* zoneName = "Normal";
    switch (MoveZone)
    {
        case MovementZone::Crusher:           zoneName = "Crusher";           break;
        case MovementZone::Destroyer:         zoneName = "Destroyer";         break;
        case MovementZone::Water:             zoneName = "Water";             break;
        case MovementZone::WaterBeach:        zoneName = "WaterBeach";        break;
        case MovementZone::Amphibious:        zoneName = "Amphibious";        break;
        case MovementZone::AmphibiousCrusher: zoneName = "AmphibiousCrusher"; break;
        case MovementZone::AmphibiousDestroyer: zoneName = "AmphibiousDestroyer"; break;
        case MovementZone::Fly:               zoneName = "Fly";               break;
        default:                              zoneName = "Normal";            break;
    }
    pINI->WriteString(section, "MovementZone", zoneName);

    // Booleans
    pINI->WriteBool(section, "Turret",       HasTurret_);
    pINI->WriteBool(section, "Cloakable",    CanCloak_);
    pINI->WriteBool(section, "Voxel",        IsVoxel_);
    pINI->WriteBool(section, "Deployer",     HasDeployer_);
    pINI->WriteBool(section, "Undeployer",   HasUndeployer_);
    pINI->WriteBool(section, "Firewall",     HasFirewall_);
    pINI->WriteBool(section, "Buildable",    IsBuildable_);
    pINI->WriteBool(section, "Trainable",    IsTrainable_);
    pINI->WriteBool(section, "Selectable",   IsSelectable_);
    pINI->WriteBool(section, "Insignificant",IsInsignificant_);
    pINI->WriteBool(section, "LegalTarget",  IsLegalTarget_);
    pINI->WriteBool(section, "Immune",       IsImmune_);
    pINI->WriteBool(section, "Passengers",   HasPassengers);
    pINI->WriteInteger(section, "Passengers", Passengers);
    pINI->WriteInteger(section, "OpenTopped", OpenTopped);
    pINI->WriteInteger(section, "SizeLimit",  SizeLimit);
    pINI->WriteInteger(section, "Crewed",     CrewCount);

    pINI->WriteBool(section, "Harvester",        IsHarvester);
    pINI->WriteBool(section, "Weeder",           IsWeeder);
    pINI->WriteBool(section, "ResourceGatherer", IsResourceGatherer);
    pINI->WriteBool(section, "Undeployable",     IsUndeployable);
    pINI->WriteBool(section, "Bombable",         IsBombable);
    pINI->WriteBool(section, "AutoFire",         IsAutoFire);
    pINI->WriteBool(section, "GuardRange",       IsGuardRange);
    pINI->WriteBool(section, "Aggressive",       IsAggressive);

    if (ImageFile[0] != '\0')
        pINI->WriteString(section, "Image", ImageFile);
    if (Cameo[0] != '\0')
        pINI->WriteString(section, "Cameo", Cameo);

    return true;
}

// ============================================================================
// CRC
// ============================================================================

void TechnoTypeClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectTypeClass::ComputeCRC(crc);

    crc.AddData(&Speed,           sizeof(Speed));
    crc.AddData(&ArmorType,       sizeof(ArmorType));
    crc.AddData(&SpeedTypeVal,    sizeof(SpeedTypeVal));
    crc.AddData(&MoveZone,        sizeof(MoveZone));
    crc.AddData(&Factory,         sizeof(Factory));
    crc.AddData(&ROT,             sizeof(ROT));
    crc.AddData(&WeaponCount,     sizeof(WeaponCount));
    crc.AddData(Weapons,          static_cast<int32>(sizeof(Weapons)));

    crc.AddData(&HasTurret_,      sizeof(HasTurret_));
    crc.AddData(&CanCloak_,       sizeof(CanCloak_));
    crc.AddData(&IsVoxel_,        sizeof(IsVoxel_));
    crc.AddData(&HasDeployer_,    sizeof(HasDeployer_));
    crc.AddData(&HasUndeployer_,  sizeof(HasUndeployer_));
    crc.AddData(&HasFirewall_,    sizeof(HasFirewall_));
    crc.AddData(&IsBuildable_,    sizeof(IsBuildable_));
    crc.AddData(&IsTrainable_,    sizeof(IsTrainable_));
    crc.AddData(&HasPassengers,   sizeof(HasPassengers));
    crc.AddData(&Passengers,      sizeof(Passengers));
    crc.AddData(&OpenTopped,      sizeof(OpenTopped));
    crc.AddData(&SizeLimit,       sizeof(SizeLimit));
    crc.AddData(&CloakSpeed,      sizeof(CloakSpeed));
    crc.AddData(&CloakRadius,     sizeof(CloakRadius));
    crc.AddData(&CrewCount,       sizeof(CrewCount));
    crc.AddData(&Crew,            sizeof(Crew));
    crc.AddData(&CrewType,        sizeof(CrewType));
    crc.AddData(&Ammo,            sizeof(Ammo));
    crc.AddData(&NoAmmo,          sizeof(NoAmmo));
    crc.AddData(&PipScale,        sizeof(PipScale));
    crc.AddData(&OccupyWeaponCount,      sizeof(OccupyWeaponCount));
    crc.AddData(&OccupyWeaponRangeBonus, sizeof(OccupyWeaponRangeBonus));
    crc.AddData(&IsHarvester,     sizeof(IsHarvester));
    crc.AddData(&IsWeeder,        sizeof(IsWeeder));
    crc.AddData(&IsResourceGatherer, sizeof(IsResourceGatherer));
    crc.AddData(&IsUndeployable,  sizeof(IsUndeployable));
    crc.AddData(&IsBombable,      sizeof(IsBombable));
    crc.AddData(&IsAutoFire,      sizeof(IsAutoFire));
    crc.AddData(&IsGuardRange,    sizeof(IsGuardRange));
    crc.AddData(&IsAggressive,    sizeof(IsAggressive));

    crc.AddData(&ThreatPosedValue, sizeof(ThreatPosedValue));
    crc.AddData(&DeathWeaponIndex, sizeof(DeathWeaponIndex));
    crc.AddData(&WeaponCharge,     sizeof(WeaponCharge));

    crc.AddData(&SightRange,      sizeof(SightRange));
    crc.AddData(&GuardRange,      sizeof(GuardRange));
    crc.AddData(&Strength,        sizeof(Strength));
    crc.AddData(&BuildCost,       sizeof(BuildCost));
    crc.AddData(&BuildTime,       sizeof(BuildTime));
    crc.AddData(&TurretROT,       sizeof(TurretROT));
    crc.AddData(&IdleTimer,       sizeof(IdleTimer));
    crc.AddData(&IsCrewed_,       sizeof(IsCrewed_));
    crc.AddData(Cameo,            static_cast<int32>(sizeof(Cameo)));
    crc.AddData(ImageFile,        static_cast<int32>(sizeof(ImageFile)));
}

int32 TechnoTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// Resolve_SHP_References
//
//  Called after the art INI has been loaded.  Binds the cameo and image SHP
//  pointers.  The full binary goes through the mix filesystem; the standalone
//  build leaves the pointers null so callers can detect missing art.
// ============================================================================

void TechnoTypeClass::Resolve_SHP_References()
{
    // Chain the parent for the base-class SHP references.
    ObjectTypeClass::Resolve_SHP_References();

    // Cameo lookup: prefer the explicit Cameo name, else fall back to the
    // image name with a "icon" suffix.
    if (CameoShape == nullptr && Cameo[0] != '\0')
    {
        // CameoShape = FileSystem::LoadSHP(Cameo);
    }
    else if (CameoShape == nullptr && ImageFile[0] != '\0')
    {
        // Try "<ImageFile>icon" as the cameo name.
        char iconBuf[0x24];
        int32 j = 0;
        while (ImageFile[j] != '\0' && j < 0x20)
        {
            iconBuf[j] = ImageFile[j];
            ++j;
        }
        iconBuf[j++] = 'i';
        iconBuf[j++] = 'c';
        iconBuf[j++] = 'o';
        iconBuf[j++] = 'n';
        iconBuf[j]   = '\0';

        // CameoShape = FileSystem::LoadSHP(iconBuf);
    }

    // Image lookup
    if (ImageShape == nullptr && ImageFile[0] != '\0')
    {
        // ImageShape = FileSystem::LoadSHP(ImageFile);
    }

    // Image size: derive from the loaded shape if present.
    if (ImageShape != nullptr)
    {
        // ImageSize.X = ImageShape->Width;
        // ImageSize.Y = ImageShape->Height;
    }
}
