#include <Abstract/UnitTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>

// ============================================================================
// UnitTypeClass.cpp
//
//  UnitTypeClass is the type descriptor for every vehicle unit - tanks,
//  harvesters, MCVs, transport trucks, artillery, etc.  It inherits the
//  techno-type fields (armor, weapons, sight) and adds:
//
//    * Harvester / weeder / resource-gatherer classification
//    * MCV / carryall / deployer configuration
//    * VXL/HVA voxel model references (units are voxel-rendered)
//    * Weapon-charge settings (for charged weapons like the Magnetron)
//    * Turret / crush / bomb interaction flags
//
//  This file implements:
//    * Static Array plumbing (Init_Array / Delete_Array / Find / FindByIndex
//      / GetCount / Delete_All)
//    * Constructor / destructor
//    * LoadFromINI / SaveToINI
//    * ComputeCRC / GetCRC
//    * Classification helpers (IsHarvester / Is_MCV / Is_Carryall / ...)
//    * Get_Max_Passengers / Get_Charge_Level
//    * Resolve_VXL_References - binds voxel model names
//    * Get_Image_Size / Get_Build_Queue_Type
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<UnitTypeClass*>* UnitTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void UnitTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<UnitTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<UnitTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<UnitTypeClass*>();
    }
}

void UnitTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<UnitTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

UnitTypeClass* UnitTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        UnitTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

UnitTypeClass* UnitTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 UnitTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

void UnitTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        UnitTypeClass* item = Array->Items[i];
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

UnitTypeClass::UnitTypeClass() noexcept
    : TechnoTypeClass(noinit)
{
    Harvester             = false;
    Weeder                = false;
    ResourceGatherer      = false;
    Undeployable          = false;
    Bombable              = false;
    AutoFire              = false;
    GuardRange            = false;
    Aggressive            = false;
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
    WeaponCount           = 0;
    EliteWeaponCount      = 0;
    HasTurret             = false;
    CanCloak              = false;
    IsVoxel               = true;   // Units are voxel by default
    HasDeployer           = false;
    HasUndeployer         = false;
    HasFirewall           = false;

    VoxelName[0]          = '\0';
    HVAName[0]            = '\0';

    std::memset(Weapons,        0, sizeof(Weapons));
    std::memset(EliteWeapons,   0, sizeof(EliteWeapons));
    std::memset(padding_UnitType, 0, sizeof(padding_UnitType));

    // Units are built by the war factory / weapons factory.
    Factory               = AbstractType::Unit;
    IsTrainable_          = true;
    IsBuildable_          = true;

    // Sync inherited TechnoTypeClass flags - the parent fields use the "Is"
    // prefix while the UnitTypeClass fields drop it.  Use TechnoTypeClass::
    // qualifier to disambiguate from the virtual accessor methods.
    TechnoTypeClass::IsHarvester        = Harvester;
    TechnoTypeClass::IsWeeder           = Weeder;
    TechnoTypeClass::IsResourceGatherer = ResourceGatherer;
    TechnoTypeClass::IsUndeployable     = Undeployable;
    TechnoTypeClass::IsBombable         = Bombable;
    TechnoTypeClass::IsAutoFire         = AutoFire;
    TechnoTypeClass::IsGuardRange       = GuardRange;
    TechnoTypeClass::IsAggressive       = Aggressive;
}

// ============================================================================
// Destructor
// ============================================================================

UnitTypeClass::~UnitTypeClass()
{
    // Voxel model references are owned by the art system.
}

// ============================================================================
// RTTI / size / ID
// ============================================================================

AbstractType UnitTypeClass::GetAbstractDerivationID() const
{
    return AbstractType::UnitType;
}

bool UnitTypeClass::HasThisID(const char* pID) const
{
    if (pID == nullptr)
        return false;
    return _strcmpi(this->ID, pID) == 0;
}

int32 UnitTypeClass::Size() const
{
    return sizeof(UnitTypeClass);
}

AbstractType UnitTypeClass::GetClassID() const
{
    return AbstractType::UnitType;
}

const char* UnitTypeClass::get_ID() const
{
    return this->ID;
}

const wchar_t* UnitTypeClass::GetUIName() const
{
    return this->UIName;
}

// ============================================================================
// Classification helpers
// ============================================================================

bool UnitTypeClass::IsHarvester() const        { return Harvester; }
bool UnitTypeClass::IsWeeder() const           { return Weeder; }
bool UnitTypeClass::IsResourceGatherer() const { return ResourceGatherer; }
bool UnitTypeClass::IsUndeployable() const     { return Undeployable; }
bool UnitTypeClass::IsBombable() const         { return Bombable; }
bool UnitTypeClass::IsAutoFire() const         { return AutoFire; }
bool UnitTypeClass::IsGuardRange() const       { return GuardRange; }
bool UnitTypeClass::IsAggressive() const       { return Aggressive; }

// ============================================================================
// Extended unit accessors
// ============================================================================

int32 UnitTypeClass::Get_Max_Passengers() const
{
    // The transport capacity is stored in the parent's Passengers field.
    // When HasPassengers is false the unit cannot carry passengers.
    if (!HasPassengers)
        return 0;
    return Passengers;
}

bool UnitTypeClass::Is_MCV() const
{
    // An MCV is a unit that deploys into a construction yard.  In the full
    // binary this is determined by checking whether the deployment target
    // type is a ConstructionYard building.  The standalone build uses the
    // Undeployable flag combined with the Factory field.
    return Undeployable && Factory == AbstractType::Building;
}

bool UnitTypeClass::Is_Carryall() const
{
    return IsCarryall;
}

int32 UnitTypeClass::Get_Charge_Level() const
{
    // Returns the weapon charge required before the primary weapon can fire.
    // Used by charged weapons like the Magnetron.  A value of 0 means the
    // weapon fires immediately.
    return WeaponCharge;
}

Point2D UnitTypeClass::Get_Image_Size() const
{
    // For voxel units the image size is derived from the voxel model bounds.
    // The full binary queries the VXL loader; the standalone build returns
    // the cached ImageSize (default 0,0 until Resolve_VXL_References runs).
    if (ImageSize.X > 0 || ImageSize.Y > 0)
        return ImageSize;

    // Fall back to a reasonable default for voxel units.
    if (IsVoxel)
        return Point2D(28, 28);

    return Point2D(0, 0);
}

AbstractType UnitTypeClass::Get_Build_Queue_Type() const
{
    // Units are produced by the war factory (Unit factory).
    return AbstractType::Unit;
}

// ============================================================================
// Resolve_VXL_References
//
//  Called after the art INI has been loaded.  Binds the voxel model name
//  (VXL) and the animation hierarchy (HVA) for this unit type.  The full
//  binary goes through the mix filesystem; the standalone build records
//  the names for later use by the renderer.
// ============================================================================

void UnitTypeClass::Resolve_VXL_References()
{
    // Chain the parent for SHP references (cameo, etc.)
    TechnoTypeClass::Resolve_SHP_References();

    // The voxel model name defaults to the ImageFile name if not explicitly
    // set.  Units rendered as voxels use the .vxl extension.
    if (VoxelName[0] == '\0' && ImageFile[0] != '\0')
    {
        int32 j = 0;
        while (ImageFile[j] != '\0' && j < static_cast<int32>(sizeof(VoxelName) - 1))
        {
            VoxelName[j] = ImageFile[j];
            ++j;
        }
        VoxelName[j] = '\0';
    }

    // The HVA name defaults to the voxel name with a .hva extension.  In
    // the mix filesystem the HVA file shares the base name of the VXL.
    if (HVAName[0] == '\0' && VoxelName[0] != '\0')
    {
        int32 j = 0;
        while (VoxelName[j] != '\0' && j < static_cast<int32>(sizeof(HVAName) - 1))
        {
            HVAName[j] = VoxelName[j];
            ++j;
        }
        HVAName[j] = '\0';
    }

    // Voxel model loading would happen here in the full binary:
    //   VoxelModel = MixFileSystem::LoadVXL(VoxelName);
    //   VoxelHVA   = MixFileSystem::LoadHVA(HVAName);
    // After loading, the image size is derived from the voxel bounds.
    if (IsVoxel && VoxelName[0] != '\0')
    {
        // Approximate the voxel image size based on the unit type.
        // The full binary reads the actual bounds from the VXL header.
        if (ImageSize.X == 0 && ImageSize.Y == 0)
        {
            ImageSize.X = 28;
            ImageSize.Y = 28;
        }
    }
}

// ============================================================================
// LoadFromINI
// ============================================================================

bool UnitTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // Chain the parent first so techno fields are loaded.
    TechnoTypeClass::LoadFromINI(pINI);

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // ------------------------------------------------------------------
    // Unit-specific flags
    // ------------------------------------------------------------------
    Harvester        = pINI->ReadBool(section, "Harvester",        Harvester);
    Weeder           = pINI->ReadBool(section, "Weeder",           Weeder);
    ResourceGatherer = pINI->ReadBool(section, "ResourceGatherer", ResourceGatherer);
    Undeployable     = pINI->ReadBool(section, "Undeployable",     Undeployable);
    Bombable         = pINI->ReadBool(section, "Bombable",         Bombable);
    AutoFire         = pINI->ReadBool(section, "AutoFire",         AutoFire);
    GuardRange       = pINI->ReadBool(section, "GuardRange",       GuardRange);
    Aggressive       = pINI->ReadBool(section, "Aggressive",       Aggressive);

    // Sync with parent fields - use TechnoTypeClass:: qualifier to
    // disambiguate from the virtual accessor methods.
    TechnoTypeClass::IsHarvester        = Harvester;
    TechnoTypeClass::IsWeeder           = Weeder;
    TechnoTypeClass::IsResourceGatherer = ResourceGatherer;
    TechnoTypeClass::IsUndeployable     = Undeployable;
    TechnoTypeClass::IsBombable         = Bombable;
    TechnoTypeClass::IsAutoFire         = AutoFire;
    TechnoTypeClass::IsGuardRange       = GuardRange;
    TechnoTypeClass::IsAggressive       = Aggressive;

    // ------------------------------------------------------------------
    // Voxel / SHP classification
    // ------------------------------------------------------------------
    IsVoxel    = pINI->ReadBool(section, "Voxel", IsVoxel);
    Voxel      = IsVoxel;
    IsVoxel_   = IsVoxel;

    HasTurret     = pINI->ReadBool(section, "Turret",     HasTurret);
    CanCloak      = pINI->ReadBool(section, "Cloakable",  CanCloak);
    HasDeployer   = pINI->ReadBool(section, "Deployer",   HasDeployer);
    HasUndeployer = pINI->ReadBool(section, "Undeployer", HasUndeployer);
    HasFirewall   = pINI->ReadBool(section, "Firewall",   HasFirewall);

    Turret        = HasTurret;
    Cloak         = CanCloak;
    Deployer      = HasDeployer;
    Undeployer    = HasUndeployer;
    Firewall      = HasFirewall;

    // ------------------------------------------------------------------
    // Speed / ROT / TurretROT (unit-specific overrides)
    // ------------------------------------------------------------------
    Speed    = pINI->ReadInteger(section, "Speed",    Speed);
    ROT      = pINI->ReadInteger(section, "ROT",      ROT);
    TurretROT= pINI->ReadInteger(section, "TurretROT", TurretROT);

    // ------------------------------------------------------------------
    // Weapons
    // ------------------------------------------------------------------
    WeaponCount = pINI->ReadInteger(section, "WeaponCount", WeaponCount);
    if (WeaponCount < 0) WeaponCount = 0;
    if (WeaponCount > 2) WeaponCount = 2;
    for (int32 i = 0; i < WeaponCount; ++i)
    {
        std::memset(&Weapons[i], 0, sizeof(WeaponStruct));
    }

    EliteWeaponCount = pINI->ReadInteger(section, "EliteWeaponCount", EliteWeaponCount);
    if (EliteWeaponCount < 0) EliteWeaponCount = 0;
    if (EliteWeaponCount > 2) EliteWeaponCount = 2;
    for (int32 i = 0; i < EliteWeaponCount; ++i)
    {
        std::memset(&EliteWeapons[i], 0, sizeof(WeaponStruct));
    }

    DeathWeaponIndex = pINI->ReadInteger(section, "DeathWeapon", -1);
    WeaponCharge     = pINI->ReadInteger(section, "WeaponCharge", 0);

    // ------------------------------------------------------------------
    // Threat / score
    // ------------------------------------------------------------------
    ThreatPosedValue = pINI->ReadFloat(section, "ThreatPosed", ThreatPosedValue);
    Score            = pINI->ReadInteger(section, "Score", Score);

    // ------------------------------------------------------------------
    // VXL / HVA art references
    // ------------------------------------------------------------------
    char voxelBuf[64];
    pINI->ReadString(section, "Voxel", "", voxelBuf, sizeof(voxelBuf));
    if (voxelBuf[0] != '\0')
    {
        int32 j = 0;
        while (voxelBuf[j] != '\0' && j < static_cast<int32>(sizeof(VoxelName) - 1))
        {
            VoxelName[j] = voxelBuf[j];
            ++j;
        }
        VoxelName[j] = '\0';
    }

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

    // ------------------------------------------------------------------
    // Passengers / transport
    // ------------------------------------------------------------------
    HasPassengers = pINI->ReadBool(section, "Passengers", HasPassengers);
    Passengers    = pINI->ReadInteger(section, "Passengers", Passengers);
    OpenTopped    = pINI->ReadInteger(section, "OpenTopped", OpenTopped);
    SizeLimit     = pINI->ReadInteger(section, "SizeLimit", SizeLimit);

    // ------------------------------------------------------------------
    // Special unit classification flags
    // ------------------------------------------------------------------
    IsCarryall          = pINI->ReadBool(section, "Carryall",          IsCarryall);
    IsTrain             = pINI->ReadBool(section, "Train",             IsTrain);
    IsSimpleDeployer    = pINI->ReadBool(section, "SimpleDeployer",    IsSimpleDeployer);
    IsFirebase          = pINI->ReadBool(section, "Firebase",          IsFirebase);
    IsSonic             = pINI->ReadBool(section, "Sonic",             IsSonic);
    IsVan               = pINI->ReadBool(section, "Van",               IsVan);
    IsBalloonHover      = pINI->ReadBool(section, "BalloonHover",      IsBalloonHover);
    IsCyborg            = pINI->ReadBool(section, "Cyborg",            IsCyborg);
    IsConsideredAircraft= pINI->ReadBool(section, "ConsideredAircraft",IsConsideredAircraft);
    IsConsideredVehicle = pINI->ReadBool(section, "ConsideredVehicle", IsConsideredVehicle);

    // Sync parent mirror fields using TechnoTypeClass:: qualifier.
    TechnoTypeClass::IsCarryall           = IsCarryall;
    TechnoTypeClass::IsTrain              = IsTrain;
    TechnoTypeClass::IsSimpleDeployer     = IsSimpleDeployer;
    TechnoTypeClass::IsFirebase           = IsFirebase;
    TechnoTypeClass::IsSonic              = IsSonic;
    TechnoTypeClass::IsVan                = IsVan;
    TechnoTypeClass::IsBalloonHover       = IsBalloonHover;
    TechnoTypeClass::IsCyborg             = IsCyborg;
    TechnoTypeClass::IsConsideredAircraft = IsConsideredAircraft;
    TechnoTypeClass::IsConsideredVehicle  = IsConsideredVehicle;

    // ------------------------------------------------------------------
    // Combat / immunity flags
    // ------------------------------------------------------------------
    IsCrushable         = pINI->ReadBool(section, "Crushable",         IsCrushable);
    IsCrushable2        = pINI->ReadBool(section, "Crushable2",        IsCrushable2);
    IsTeleporter        = pINI->ReadBool(section, "Teleporter",        IsTeleporter);
    IsChrono            = pINI->ReadBool(section, "Chrono",            IsChrono);
    IsBomb              = pINI->ReadBool(section, "Bomb",              IsBomb);
    IsCow               = pINI->ReadBool(section, "Cow",               IsCow);
    IsDog               = pINI->ReadBool(section, "Dog",               IsDog);
    IsBoris             = pINI->ReadBool(section, "Boris",             IsBoris);
    IsArmed             = pINI->ReadBool(section, "Armed",             IsArmed);
    IsMissileSpawn      = pINI->ReadBool(section, "MissileSpawn",      IsMissileSpawn);
    IsFake              = pINI->ReadBool(section, "Fake",              IsFake);
    IsDisableable       = pINI->ReadBool(section, "Disableable",       IsDisableable);

    IsImmuneToPsionics  = pINI->ReadBool(section, "ImmuneToPsionics",  IsImmuneToPsionics);
    IsImmuneToPoison    = pINI->ReadBool(section, "ImmuneToPoison",    IsImmuneToPoison);
    IsImmuneToRadiation = pINI->ReadBool(section, "ImmuneToRadiation", IsImmuneToRadiation);
    IsImmuneToBerserk   = pINI->ReadBool(section, "ImmuneToBerserk",   IsImmuneToBerserk);
    IsImmuneToEMP       = pINI->ReadBool(section, "ImmuneToEMP",       IsImmuneToEMP);

    // ------------------------------------------------------------------
    // CanBeXxx interaction flags
    // ------------------------------------------------------------------
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

    // ------------------------------------------------------------------
    // Misc flags
    // ------------------------------------------------------------------
    IsTilter          = pINI->ReadBool(section, "Tilter",          IsTilter);
    IsToProtect       = pINI->ReadBool(section, "ToProtect",       IsToProtect);
    IsNominal         = pINI->ReadBool(section, "Nominal",         IsNominal);
    IsRadarInvisible  = pINI->ReadBool(section, "RadarInvisible",  IsRadarInvisible);
    IsDontScore       = pINI->ReadBool(section, "DontScore",       IsDontScore);
    IsNoThreat        = pINI->ReadBool(section, "NoThreat",        IsNoThreat);
    IsSensorsSight    = pINI->ReadBool(section, "SensorsSight",    IsSensorsSight);
    IsHunterSeeker    = pINI->ReadBool(section, "HunterSeeker",    IsHunterSeeker);
    IsIvan            = pINI->ReadBool(section, "Ivan",            IsIvan);
    IsLeader          = pINI->ReadBool(section, "Leader",          IsLeader);

    IsNaval            = pINI->ReadBool(section, "Naval",            IsNaval);
    IsLand             = pINI->ReadBool(section, "Land",             IsLand);
    IsAir              = pINI->ReadBool(section, "Air",              IsAir);
    IsOrganic          = pINI->ReadBool(section, "Organic",          IsOrganic);
    IsNeutral          = pINI->ReadBool(section, "Neutral",          IsNeutral);
    IsInfiltratable    = pINI->ReadBool(section, "Infiltratable",    IsInfiltratable);
    IsStealthy         = pINI->ReadBool(section, "Stealthy",         IsStealthy);
    IsHealable         = pINI->ReadBool(section, "Healable",         IsHealable);

    // ------------------------------------------------------------------
    // Factory type (what producer builds this unit)
    // ------------------------------------------------------------------
    char factoryBuf[32];
    pINI->ReadString(section, "Factory", "Unit", factoryBuf, sizeof(factoryBuf));
    if (!_strcmpi(factoryBuf, "Building"))   Factory = AbstractType::Building;
    else if (!_strcmpi(factoryBuf, "Infantry")) Factory = AbstractType::Infantry;
    else if (!_strcmpi(factoryBuf, "Unit"))  Factory = AbstractType::Unit;
    else if (!_strcmpi(factoryBuf, "Aircraft")) Factory = AbstractType::Aircraft;
    else                                     Factory = AbstractType::Unit;

    return true;
}

// ============================================================================
// SaveToINI
// ============================================================================

bool UnitTypeClass::SaveToINI(CCINIClass* pINI) const
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // Chain the parent.
    const_cast<UnitTypeClass*>(this)->TechnoTypeClass::SaveToINI(pINI);

    pINI->WriteInteger(section, "WeaponCount",     WeaponCount);
    pINI->WriteInteger(section, "EliteWeaponCount",EliteWeaponCount);
    pINI->WriteInteger(section, "DeathWeapon",     DeathWeaponIndex);
    pINI->WriteInteger(section, "WeaponCharge",    WeaponCharge);
    pINI->WriteFloat(section,  "ThreatPosed",     ThreatPosedValue);
    pINI->WriteInteger(section, "Passengers",      Passengers);
    pINI->WriteInteger(section, "OpenTopped",      OpenTopped);
    pINI->WriteInteger(section, "SizeLimit",       SizeLimit);

    pINI->WriteBool(section, "Harvester",        Harvester);
    pINI->WriteBool(section, "Weeder",           Weeder);
    pINI->WriteBool(section, "ResourceGatherer", ResourceGatherer);
    pINI->WriteBool(section, "Undeployable",     Undeployable);
    pINI->WriteBool(section, "Bombable",         Bombable);
    pINI->WriteBool(section, "AutoFire",         AutoFire);
    pINI->WriteBool(section, "GuardRange",       GuardRange);
    pINI->WriteBool(section, "Aggressive",       Aggressive);

    pINI->WriteBool(section, "Voxel",     IsVoxel);
    pINI->WriteBool(section, "Turret",    HasTurret);
    pINI->WriteBool(section, "Cloakable", CanCloak);
    pINI->WriteBool(section, "Deployer",  HasDeployer);
    pINI->WriteBool(section, "Undeployer",HasUndeployer);
    pINI->WriteBool(section, "Firewall",  HasFirewall);

    pINI->WriteBool(section, "Carryall",          IsCarryall);
    pINI->WriteBool(section, "Train",             IsTrain);
    pINI->WriteBool(section, "SimpleDeployer",    IsSimpleDeployer);
    pINI->WriteBool(section, "Firebase",          IsFirebase);
    pINI->WriteBool(section, "Sonic",             IsSonic);
    pINI->WriteBool(section, "Van",               IsVan);
    pINI->WriteBool(section, "BalloonHover",      IsBalloonHover);
    pINI->WriteBool(section, "Cyborg",            IsCyborg);
    pINI->WriteBool(section, "ConsideredAircraft",IsConsideredAircraft);
    pINI->WriteBool(section, "ConsideredVehicle", IsConsideredVehicle);

    pINI->WriteBool(section, "Crushable",         IsCrushable);
    pINI->WriteBool(section, "Crushable2",        IsCrushable2);
    pINI->WriteBool(section, "Teleporter",        IsTeleporter);
    pINI->WriteBool(section, "Chrono",            IsChrono);
    pINI->WriteBool(section, "Bomb",              IsBomb);
    pINI->WriteBool(section, "Cow",               IsCow);
    pINI->WriteBool(section, "Dog",               IsDog);
    pINI->WriteBool(section, "Boris",             IsBoris);
    pINI->WriteBool(section, "Armed",             IsArmed);
    pINI->WriteBool(section, "MissileSpawn",      IsMissileSpawn);
    pINI->WriteBool(section, "Fake",              IsFake);
    pINI->WriteBool(section, "Disableable",       IsDisableable);

    pINI->WriteBool(section, "ImmuneToPsionics",  IsImmuneToPsionics);
    pINI->WriteBool(section, "ImmuneToPoison",    IsImmuneToPoison);
    pINI->WriteBool(section, "ImmuneToRadiation", IsImmuneToRadiation);
    pINI->WriteBool(section, "ImmuneToBerserk",   IsImmuneToBerserk);
    pINI->WriteBool(section, "ImmuneToEMP",       IsImmuneToEMP);

    pINI->WriteBool(section, "CanSuppressed",     IsCanBeSuppressed);
    pINI->WriteBool(section, "CanBeOccupied",     IsCanBeOccupied);
    pINI->WriteBool(section, "CanBeDriven",       IsCanBeDriven);
    pINI->WriteBool(section, "CanBeCaptured",     IsCanBeCaptured);
    pINI->WriteBool(section, "CanBeRepaired",     IsCanBeRepaired);
    pINI->WriteBool(section, "CanBeSold",         IsCanBeSold);
    pINI->WriteBool(section, "CanBePowered",      IsCanBePowered);
    pINI->WriteBool(section, "CanBeDestroyed",    IsCanBeDestroyed);
    pINI->WriteBool(section, "CanBeDamaged",      IsCanBeDamaged);
    pINI->WriteBool(section, "CanBeInfiltrated",  IsCanBeInfiltrated);
    pINI->WriteBool(section, "CanBeSpied",        IsCanBeSpied);
    pINI->WriteBool(section, "CanBeSabotaged",    IsCanBeSabotaged);
    pINI->WriteBool(section, "CanBeStolen",       IsCanBeStolen);
    pINI->WriteBool(section, "CanBeHijacked",     IsCanBeHijacked);

    pINI->WriteBool(section, "Tilter",          IsTilter);
    pINI->WriteBool(section, "ToProtect",       IsToProtect);
    pINI->WriteBool(section, "Nominal",         IsNominal);
    pINI->WriteBool(section, "RadarInvisible",  IsRadarInvisible);
    pINI->WriteBool(section, "DontScore",       IsDontScore);
    pINI->WriteBool(section, "NoThreat",        IsNoThreat);
    pINI->WriteBool(section, "SensorsSight",    IsSensorsSight);
    pINI->WriteBool(section, "HunterSeeker",    IsHunterSeeker);
    pINI->WriteBool(section, "Ivan",            IsIvan);
    pINI->WriteBool(section, "Leader",          IsLeader);

    pINI->WriteBool(section, "Naval",            IsNaval);
    pINI->WriteBool(section, "Land",             IsLand);
    pINI->WriteBool(section, "Air",              IsAir);
    pINI->WriteBool(section, "Organic",          IsOrganic);
    pINI->WriteBool(section, "Neutral",          IsNeutral);
    pINI->WriteBool(section, "Infiltratable",    IsInfiltratable);
    pINI->WriteBool(section, "Stealthy",         IsStealthy);
    pINI->WriteBool(section, "Healable",         IsHealable);

    if (VoxelName[0] != '\0')
        pINI->WriteString(section, "Voxel", VoxelName);
    if (HVAName[0] != '\0')
        pINI->WriteString(section, "HVA", HVAName);

    return true;
}

// ============================================================================
// CRC
// ============================================================================

void UnitTypeClass::ComputeCRC(CRCEngine& crc) const
{
    TechnoTypeClass::ComputeCRC(crc);

    crc.AddData(&Harvester,        sizeof(Harvester));
    crc.AddData(&Weeder,           sizeof(Weeder));
    crc.AddData(&ResourceGatherer, sizeof(ResourceGatherer));
    crc.AddData(&Undeployable,     sizeof(Undeployable));
    crc.AddData(&Bombable,         sizeof(Bombable));
    crc.AddData(&AutoFire,         sizeof(AutoFire));
    crc.AddData(&GuardRange,       sizeof(GuardRange));
    crc.AddData(&Aggressive,       sizeof(Aggressive));
    crc.AddData(&IsCarryall,       sizeof(IsCarryall));
    crc.AddData(&IsTrain,          sizeof(IsTrain));
    crc.AddData(&IsSimpleDeployer, sizeof(IsSimpleDeployer));
    crc.AddData(&IsFirebase,       sizeof(IsFirebase));
    crc.AddData(&IsSonic,          sizeof(IsSonic));
    crc.AddData(&IsVan,            sizeof(IsVan));
    crc.AddData(&IsBalloonHover,   sizeof(IsBalloonHover));
    crc.AddData(&IsCyborg,         sizeof(IsCyborg));
    crc.AddData(&IsCrushable,      sizeof(IsCrushable));
    crc.AddData(&IsCrushable2,     sizeof(IsCrushable2));
    crc.AddData(&IsTeleporter,     sizeof(IsTeleporter));
    crc.AddData(&IsChrono,         sizeof(IsChrono));
    crc.AddData(&IsBomb,           sizeof(IsBomb));
    crc.AddData(&IsArmed,          sizeof(IsArmed));
    crc.AddData(&IsMissileSpawn,   sizeof(IsMissileSpawn));
    crc.AddData(&ThreatPosedValue, sizeof(ThreatPosedValue));
    crc.AddData(&DeathWeaponIndex, sizeof(DeathWeaponIndex));
    crc.AddData(&WeaponCharge,     sizeof(WeaponCharge));
    crc.AddData(&WeaponCount,      sizeof(WeaponCount));
    crc.AddData(Weapons,           static_cast<int32>(sizeof(Weapons)));
    crc.AddData(&EliteWeaponCount, sizeof(EliteWeaponCount));
    crc.AddData(EliteWeapons,      static_cast<int32>(sizeof(EliteWeapons)));
    crc.AddData(&HasTurret,        sizeof(HasTurret));
    crc.AddData(&CanCloak,         sizeof(CanCloak));
    crc.AddData(&IsVoxel,          sizeof(IsVoxel));
    crc.AddData(&HasDeployer,      sizeof(HasDeployer));
    crc.AddData(&HasUndeployer,    sizeof(HasUndeployer));
    crc.AddData(&HasFirewall,      sizeof(HasFirewall));
    crc.AddData(VoxelName,         static_cast<int32>(sizeof(VoxelName)));
    crc.AddData(HVAName,           static_cast<int32>(sizeof(HVAName)));
}

int32 UnitTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}
