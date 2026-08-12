#include <Abstract/AircraftTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>

// ============================================================================
// AircraftTypeClass.cpp
//
//  AircraftTypeClass is the type descriptor for every aircraft unit - fighters,
//  bombers, spy planes, paradrop transports, carryalls, etc.  It inherits the
//  techno-type fields and adds:
//
//    * Fighter / bomber / kamikaze / spyplane classification
//    * Paradrop / carryall transport configuration
//    * Flight level (cruise altitude in leptons)
//    * Docking configuration (helipad / airport dock slots)
//    * VXL/HVA voxel model references (aircraft are voxel-rendered)
//    * Landing spot type (helipad, airport, or none)
//
//  This file implements:
//    * Static Array plumbing
//    * Constructor / destructor
//    * LoadFromINI / SaveToINI
//    * ComputeCRC / GetCRC
//    * Classification helpers (IsFighter / Is_Bomber / Is_Carryall / ...)
//    * Get_Landing_Spot_Type - returns the dock type for this aircraft
//    * Resolve_VXL_References - binds voxel model names
//    * Get_Image_Size / Get_Build_Queue_Type
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<AircraftTypeClass*>* AircraftTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void AircraftTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<AircraftTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<AircraftTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<AircraftTypeClass*>();
    }
}

void AircraftTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<AircraftTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

AircraftTypeClass* AircraftTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        AircraftTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

AircraftTypeClass* AircraftTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 AircraftTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

void AircraftTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        AircraftTypeClass* item = Array->Items[i];
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

AircraftTypeClass::AircraftTypeClass() noexcept
    : TechnoTypeClass(noinit)
{
    Fighter       = false;
    Strafe        = false;
    Locked        = false;
    Loaded        = false;
    Kamikaze      = false;
    Spyplane      = false;
    Paradropping  = false;
    Carryall      = false;
    AntiAir       = false;

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
    IsTrain               = false;
    IsConsideredAircraft  = true;
    IsConsideredVehicle   = false;
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
    IsVoxel               = true;   // Aircraft are voxel by default
    HasDeployer           = false;
    HasUndeployer         = false;
    HasFirewall           = false;

    FlightLevel           = 0;
    DockOffset            = 0;
    NumberOfDocks         = 0;

    VoxelName[0]          = '\0';
    HVAName[0]            = '\0';

    std::memset(Weapons,        0, sizeof(Weapons));
    std::memset(EliteWeapons,   0, sizeof(EliteWeapons));
    std::memset(padding_AircraftType, 0, sizeof(padding_AircraftType));

    // Aircraft are built by the airport / helipad.
    Factory               = AbstractType::Aircraft;
    IsTrainable_          = true;
    IsBuildable_          = true;

    // Aircraft movement zone is Fly.
    MoveZone              = MovementZone::Fly;
    IsAir                 = true;
    IsLand                = false;

    // Sync parent flags
    TechnoTypeClass::IsCarryall        = Carryall;
    TechnoTypeClass::IsConsideredAircraft = IsConsideredAircraft;
    TechnoTypeClass::IsConsideredVehicle  = IsConsideredVehicle;
    TechnoTypeClass::IsVoxel_          = IsVoxel;
    TechnoTypeClass::Voxel             = IsVoxel;
    TechnoTypeClass::IsAir             = IsAir;
    TechnoTypeClass::IsLand            = IsLand;
}

// ============================================================================
// Destructor
// ============================================================================

AircraftTypeClass::~AircraftTypeClass()
{
    // Voxel model references are owned by the art system.
}

// ============================================================================
// RTTI / size / ID
// ============================================================================

AbstractType AircraftTypeClass::GetAbstractDerivationID() const
{
    return AbstractType::AircraftType;
}

bool AircraftTypeClass::HasThisID(const char* pID) const
{
    if (pID == nullptr)
        return false;
    return _strcmpi(this->ID, pID) == 0;
}

int32 AircraftTypeClass::Size() const
{
    return sizeof(AircraftTypeClass);
}

AbstractType AircraftTypeClass::GetClassID() const
{
    return AbstractType::AircraftType;
}

const char* AircraftTypeClass::get_ID() const
{
    return this->ID;
}

const wchar_t* AircraftTypeClass::GetUIName() const
{
    return this->UIName;
}

// ============================================================================
// Classification helpers
// ============================================================================

bool AircraftTypeClass::IsFighter() const     { return Fighter; }
bool AircraftTypeClass::IsStrafe() const      { return Strafe; }
bool AircraftTypeClass::IsLocked() const      { return Locked; }
bool AircraftTypeClass::IsLoaded() const      { return Loaded; }
bool AircraftTypeClass::IsKamikaze() const    { return Kamikaze; }
bool AircraftTypeClass::IsSpyplane() const    { return Spyplane; }
bool AircraftTypeClass::IsParadropping() const{ return Paradropping; }
bool AircraftTypeClass::IsCarryall() const    { return Carryall; }
bool AircraftTypeClass::IsAntiAir() const     { return AntiAir; }

// ============================================================================
// Extended aircraft accessors
// ============================================================================

bool AircraftTypeClass::Is_Fighter() const
{
    // A fighter is an aircraft that engages other aircraft and can land on
    // an airport.  The full binary checks the Landing flag combination; here
    // we use the Fighter field directly.
    return Fighter;
}

bool AircraftTypeClass::Is_Bomber() const
{
    // A bomber is an aircraft that performs ground attack runs.  In the
    // full binary, bombers are identified by the Strafe flag (they make
    // attack passes rather than hovering).  Kamikaze aircraft are also
    // considered bombers since they deliver a payload to a ground target.
    return Strafe || Kamikaze;
}

int32 AircraftTypeClass::Get_Landing_Spot_Type() const
{
    // Returns the type of landing spot required by this aircraft:
    //   0 = none (always airborne, e.g. spy planes)
    //   1 = helipad
    //   2 = airport runway
    //
    // The full binary checks the DockOffset and NumberOfDocks fields to
    // determine which building type the aircraft can dock at.  Helipad-
    // capable aircraft have DockOffset == 0; airport-capable aircraft
    // have DockOffset > 0.
    if (NumberOfDocks <= 0)
        return 0;  // No landing spot needed

    if (DockOffset == 0)
        return 1;  // Helipad

    return 2;  // Airport runway
}

Point2D AircraftTypeClass::Get_Image_Size() const
{
    if (ImageSize.X > 0 || ImageSize.Y > 0)
        return ImageSize;

    // Default voxel aircraft size.
    if (IsVoxel)
        return Point2D(40, 40);

    return Point2D(0, 0);
}

AbstractType AircraftTypeClass::Get_Build_Queue_Type() const
{
    // Aircraft are produced by the airport / helipad.
    return AbstractType::Aircraft;
}

// ============================================================================
// Resolve_VXL_References
//
//  Binds the voxel model name (VXL) and animation hierarchy (HVA) for this
//  aircraft type.  Aircraft are typically voxel-rendered.
// ============================================================================

void AircraftTypeClass::Resolve_VXL_References()
{
    // Chain the parent for SHP references (cameo, etc.)
    TechnoTypeClass::Resolve_SHP_References();

    // The voxel model name defaults to the ImageFile name if not set.
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

    // The HVA name defaults to the voxel name.
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
    if (IsVoxel && VoxelName[0] != '\0')
    {
        if (ImageSize.X == 0 && ImageSize.Y == 0)
        {
            ImageSize.X = 40;
            ImageSize.Y = 40;
        }
    }
}

// ============================================================================
// LoadFromINI
// ============================================================================

bool AircraftTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // Chain the parent first.
    TechnoTypeClass::LoadFromINI(pINI);

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // ------------------------------------------------------------------
    // Aircraft classification flags
    // ------------------------------------------------------------------
    Fighter      = pINI->ReadBool(section, "Fighter",      Fighter);
    Strafe       = pINI->ReadBool(section, "Strafe",       Strafe);
    Locked       = pINI->ReadBool(section, "Locked",       Locked);
    Loaded       = pINI->ReadBool(section, "Loaded",       Loaded);
    Kamikaze     = pINI->ReadBool(section, "Kamikaze",     Kamikaze);
    Spyplane     = pINI->ReadBool(section, "Spyplane",     Spyplane);
    Paradropping = pINI->ReadBool(section, "Paradrop",     Paradropping);
    Carryall     = pINI->ReadBool(section, "Carryall",     Carryall);
    AntiAir      = pINI->ReadBool(section, "AntiAir",      AntiAir);

    // Sync parent
    TechnoTypeClass::IsCarryall = Carryall;

    // ------------------------------------------------------------------
    // Flight / landing configuration
    // ------------------------------------------------------------------
    FlightLevel  = pINI->ReadInteger(section, "FlightLevel",  FlightLevel);
    DockOffset   = pINI->ReadInteger(section, "DockOffset",   DockOffset);
    NumberOfDocks= pINI->ReadInteger(section, "NumberOfDocks",NumberOfDocks);

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

    Turret     = HasTurret;
    Cloak      = CanCloak;
    Deployer   = HasDeployer;
    Undeployer = HasUndeployer;
    Firewall   = HasFirewall;

    // ------------------------------------------------------------------
    // Speed / ROT
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
    // Movement zone (aircraft always fly)
    // ------------------------------------------------------------------
    MoveZone   = MovementZone::Fly;
    IsAir      = true;
    IsLand     = false;

    TechnoTypeClass::IsAir  = IsAir;
    TechnoTypeClass::IsLand = IsLand;
    TechnoTypeClass::MoveZone = MoveZone;

    // ------------------------------------------------------------------
    // Special classification flags
    // ------------------------------------------------------------------
    IsTrain             = pINI->ReadBool(section, "Train",             IsTrain);
    IsSimpleDeployer    = pINI->ReadBool(section, "SimpleDeployer",    IsSimpleDeployer);
    IsFirebase          = pINI->ReadBool(section, "Firebase",          IsFirebase);
    IsSonic             = pINI->ReadBool(section, "Sonic",             IsSonic);
    IsVan               = pINI->ReadBool(section, "Van",               IsVan);
    IsBalloonHover      = pINI->ReadBool(section, "BalloonHover",      IsBalloonHover);
    IsCyborg            = pINI->ReadBool(section, "Cyborg",            IsCyborg);
    IsConsideredAircraft= pINI->ReadBool(section, "ConsideredAircraft",IsConsideredAircraft);
    IsConsideredVehicle = pINI->ReadBool(section, "ConsideredVehicle", IsConsideredVehicle);

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

    // ------------------------------------------------------------------
    // Factory type
    // ------------------------------------------------------------------
    char factoryBuf[32];
    pINI->ReadString(section, "Factory", "Aircraft", factoryBuf, sizeof(factoryBuf));
    if (!_strcmpi(factoryBuf, "Building"))   Factory = AbstractType::Building;
    else if (!_strcmpi(factoryBuf, "Infantry")) Factory = AbstractType::Infantry;
    else if (!_strcmpi(factoryBuf, "Unit"))  Factory = AbstractType::Unit;
    else if (!_strcmpi(factoryBuf, "Aircraft")) Factory = AbstractType::Aircraft;
    else                                     Factory = AbstractType::Aircraft;

    return true;
}

// ============================================================================
// SaveToINI
// ============================================================================

bool AircraftTypeClass::SaveToINI(CCINIClass* pINI) const
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // Chain the parent.
    const_cast<AircraftTypeClass*>(this)->TechnoTypeClass::SaveToINI(pINI);

    pINI->WriteInteger(section, "WeaponCount",     WeaponCount);
    pINI->WriteInteger(section, "EliteWeaponCount",EliteWeaponCount);
    pINI->WriteInteger(section, "DeathWeapon",     DeathWeaponIndex);
    pINI->WriteInteger(section, "WeaponCharge",    WeaponCharge);
    pINI->WriteInteger(section, "FlightLevel",     FlightLevel);
    pINI->WriteInteger(section, "DockOffset",      DockOffset);
    pINI->WriteInteger(section, "NumberOfDocks",   NumberOfDocks);
    pINI->WriteFloat(section,  "ThreatPosed",     ThreatPosedValue);

    pINI->WriteBool(section, "Fighter",      Fighter);
    pINI->WriteBool(section, "Strafe",       Strafe);
    pINI->WriteBool(section, "Locked",       Locked);
    pINI->WriteBool(section, "Loaded",       Loaded);
    pINI->WriteBool(section, "Kamikaze",     Kamikaze);
    pINI->WriteBool(section, "Spyplane",     Spyplane);
    pINI->WriteBool(section, "Paradrop",     Paradropping);
    pINI->WriteBool(section, "Carryall",     Carryall);
    pINI->WriteBool(section, "AntiAir",      AntiAir);

    pINI->WriteBool(section, "Voxel",     IsVoxel);
    pINI->WriteBool(section, "Turret",    HasTurret);
    pINI->WriteBool(section, "Cloakable", CanCloak);
    pINI->WriteBool(section, "Deployer",  HasDeployer);
    pINI->WriteBool(section, "Undeployer",HasUndeployer);
    pINI->WriteBool(section, "Firewall",  HasFirewall);

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

    if (VoxelName[0] != '\0')
        pINI->WriteString(section, "Voxel", VoxelName);
    if (HVAName[0] != '\0')
        pINI->WriteString(section, "HVA", HVAName);

    return true;
}

// ============================================================================
// CRC
// ============================================================================

void AircraftTypeClass::ComputeCRC(CRCEngine& crc) const
{
    TechnoTypeClass::ComputeCRC(crc);

    crc.AddData(&Fighter,           sizeof(Fighter));
    crc.AddData(&Strafe,            sizeof(Strafe));
    crc.AddData(&Locked,            sizeof(Locked));
    crc.AddData(&Loaded,            sizeof(Loaded));
    crc.AddData(&Kamikaze,          sizeof(Kamikaze));
    crc.AddData(&Spyplane,          sizeof(Spyplane));
    crc.AddData(&Paradropping,      sizeof(Paradropping));
    crc.AddData(&Carryall,          sizeof(Carryall));
    crc.AddData(&AntiAir,           sizeof(AntiAir));
    crc.AddData(&FlightLevel,       sizeof(FlightLevel));
    crc.AddData(&DockOffset,        sizeof(DockOffset));
    crc.AddData(&NumberOfDocks,     sizeof(NumberOfDocks));
    crc.AddData(&ThreatPosedValue,  sizeof(ThreatPosedValue));
    crc.AddData(&DeathWeaponIndex,  sizeof(DeathWeaponIndex));
    crc.AddData(&WeaponCharge,      sizeof(WeaponCharge));
    crc.AddData(&WeaponCount,       sizeof(WeaponCount));
    crc.AddData(Weapons,            static_cast<int32>(sizeof(Weapons)));
    crc.AddData(&EliteWeaponCount,  sizeof(EliteWeaponCount));
    crc.AddData(EliteWeapons,       static_cast<int32>(sizeof(EliteWeapons)));
    crc.AddData(&HasTurret,         sizeof(HasTurret));
    crc.AddData(&CanCloak,          sizeof(CanCloak));
    crc.AddData(&IsVoxel,           sizeof(IsVoxel));
    crc.AddData(&HasDeployer,       sizeof(HasDeployer));
    crc.AddData(&HasUndeployer,     sizeof(HasUndeployer));
    crc.AddData(&HasFirewall,       sizeof(HasFirewall));
    crc.AddData(VoxelName,          static_cast<int32>(sizeof(VoxelName)));
    crc.AddData(HVAName,            static_cast<int32>(sizeof(HVAName)));
}

int32 AircraftTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}
