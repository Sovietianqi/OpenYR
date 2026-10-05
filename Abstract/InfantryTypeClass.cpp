#include <Abstract/InfantryTypeClass.h>
#include <Combat/WeaponTypeClass.h>
#include <Audio/VocClass.h>
#include <Animations/AnimTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>
#include <FileFormats/SHP.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>

// ============================================================================
// InfantryTypeClass.cpp
//
//  InfantryTypeClass is the type descriptor for every infantry unit - soldiers,
//  engineers, spies, dogs, civilians, cyborgs, etc.  It inherits the techno-
//  type fields and adds:
//
//    * Engineer / thief / spy / dog / Boris special-unit flags
//    * Civilian classification
//    * SHP animation sequence configuration (crawling, prone, deploy fire)
//    * Crush vulnerability flags
//    * Weapon slots (primary / secondary + elite variants)
//
//  This file implements:
//    * Static Array plumbing
//    * Constructor / destructor
//    * LoadFromINI / SaveToINI
//    * ComputeCRC / GetCRC
//    * Classification helpers (IsEngineer / Is_Civilian / Is_Spy / ...)
//    * Get_Sequence_Count - returns the number of SHP animation sequences
//    * Resolve_SHP_References - binds infantry SHP art
//    * Get_Cameo_Data / Get_Image_Size / Get_Build_Queue_Type
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<InfantryTypeClass*>* InfantryTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void InfantryTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<InfantryTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<InfantryTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<InfantryTypeClass*>();
    }
}

void InfantryTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<InfantryTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

InfantryTypeClass* InfantryTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        InfantryTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

InfantryTypeClass* InfantryTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 InfantryTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

void InfantryTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        InfantryTypeClass* item = Array->Items[i];
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

InfantryTypeClass::InfantryTypeClass() noexcept
    : TechnoTypeClass(noinit)
{
    Engineer          = false;
    Thief             = false;
    Cow               = false;
    Dog               = false;
    Boris             = false;
    IsArmed_          = false;
    IsMissileSpawn_   = false;
    IsFake_           = false;
    IsDisableable_    = false;
    IsCanBeSuppressed_= false;
    IsCanBeOccupied_  = false;
    IsCanBeDriven_    = false;
    IsCanBeCaptured_  = false;
    IsCanBeRepaired_  = true;
    IsCanBeSold_      = true;
    IsCanBePowered_   = false;
    IsCanBeDestroyed_ = true;
    IsCanBeDamaged_   = true;
    IsCanBeInfiltrated_= false;
    IsCanBeSpied_     = false;
    IsCanBeSabotaged_ = false;
    IsCanBeStolen_    = false;
    IsCanBeHijacked_  = false;

    IsCrushable       = false;
    IsCrushable2      = false;
    IsTeleporter      = false;
    IsChrono          = false;
    IsBomb            = false;
    IsCow_            = false;
    IsDog_            = false;
    IsBoris_          = false;
    IsTilter          = false;
    IsToProtect       = false;
    IsNominal         = false;
    IsRadarInvisible  = false;
    IsDontScore       = false;
    IsNoThreat        = false;
    IsSensorsSight    = false;
    IsHunterSeeker    = false;
    IsIvan            = false;
    IsLeader          = false;
    IsCarryall        = false;
    IsTrain           = false;
    IsConsideredAircraft = false;
    IsConsideredVehicle  = false;
    IsSimpleDeployer  = false;
    IsFirebase        = false;
    IsSonic           = false;
    IsVan             = false;
    IsBalloonHover    = false;
    IsCyborg          = false;
    IsNotHuman        = false;  // Infantry ARE human by default
    IsImmuneToPsionics= false;
    IsImmuneToPoison  = false;
    IsImmuneToRadiation = false;
    IsImmuneToBerserk = false;
    IsImmuneToEMP     = false;

    ThreatPosedValue  = 0.0f;
    DeathWeaponIndex  = -1;
    WeaponCharge      = 0;
    WeaponCount       = 0;
    EliteWeaponCount  = 0;
    HasTurret         = false;
    CanCloak          = false;
    IsVoxel           = false;  // Infantry are SHP, not voxel
    HasDeployer       = false;
    HasUndeployer     = false;
    HasFirewall       = false;

    DeployFireSequence = Sequence::Deploy;

    std::memset(Weapons,        0, sizeof(Weapons));
    std::memset(EliteWeapons,   0, sizeof(EliteWeapons));
    std::memset(padding_InfantryType, 0, sizeof(padding_InfantryType));

    // Infantry are built by the barracks.
    Factory           = AbstractType::Infantry;
    IsTrainable_      = true;
    IsBuildable_      = true;
    IsNotHuman        = false;
    IsOrganic         = true;

    // Sync parent flags
    TechnoTypeClass::IsArmed         = IsArmed_;
    TechnoTypeClass::IsMissileSpawn  = IsMissileSpawn_;
    TechnoTypeClass::IsFake          = IsFake_;
    TechnoTypeClass::IsDisableable   = IsDisableable_;
    TechnoTypeClass::IsCrushable     = IsCrushable;
    TechnoTypeClass::IsBomb          = IsBomb;
    TechnoTypeClass::IsCow           = Cow;
    TechnoTypeClass::IsDog           = Dog;
    TechnoTypeClass::IsBoris         = Boris;
    TechnoTypeClass::IsVoxel_        = IsVoxel;
    TechnoTypeClass::Voxel           = IsVoxel;
}

// ============================================================================
// Destructor
// ============================================================================

InfantryTypeClass::~InfantryTypeClass()
{
    // SHP references are owned by the art system.
}

// ============================================================================
// RTTI / size / ID
// ============================================================================

AbstractType InfantryTypeClass::GetAbstractDerivationID() const
{
    return AbstractType::InfantryType;
}

bool InfantryTypeClass::HasThisID(const char* pID) const
{
    if (pID == nullptr)
        return false;
    return _strcmpi(this->ID, pID) == 0;
}

int32 InfantryTypeClass::Size() const
{
    return sizeof(InfantryTypeClass);
}

AbstractType InfantryTypeClass::GetClassID() const
{
    return AbstractType::InfantryType;
}

const char* InfantryTypeClass::get_ID() const
{
    return this->ID;
}

const wchar_t* InfantryTypeClass::GetUIName() const
{
    return this->UIName;
}

// ============================================================================
// Classification helpers
// ============================================================================

bool InfantryTypeClass::IsEngineer() const         { return Engineer; }
bool InfantryTypeClass::IsThief() const            { return Thief; }
bool InfantryTypeClass::IsCow() const              { return Cow; }
bool InfantryTypeClass::IsDog() const              { return Dog; }
bool InfantryTypeClass::IsBoris() const            { return Boris; }
bool InfantryTypeClass::IsArmed() const            { return IsArmed_; }
bool InfantryTypeClass::IsMissileSpawn() const     { return IsMissileSpawn_; }
bool InfantryTypeClass::IsFake() const             { return IsFake_; }
bool InfantryTypeClass::IsDisableable() const      { return IsDisableable_; }
bool InfantryTypeClass::IsCanBeSuppressed() const  { return IsCanBeSuppressed_; }
bool InfantryTypeClass::IsCanBeOccupied() const    { return IsCanBeOccupied_; }
bool InfantryTypeClass::IsCanBeDriven() const      { return IsCanBeDriven_; }
bool InfantryTypeClass::IsCanBeCaptured() const    { return IsCanBeCaptured_; }
bool InfantryTypeClass::IsCanBeRepaired() const    { return IsCanBeRepaired_; }
bool InfantryTypeClass::IsCanBeSold() const        { return IsCanBeSold_; }
bool InfantryTypeClass::IsCanBePowered() const     { return IsCanBePowered_; }
bool InfantryTypeClass::IsCanBeDestroyed() const   { return IsCanBeDestroyed_; }
bool InfantryTypeClass::IsCanBeDamaged() const     { return IsCanBeDamaged_; }
bool InfantryTypeClass::IsCanBeInfiltrated() const { return IsCanBeInfiltrated_; }
bool InfantryTypeClass::IsCanBeSpied() const       { return IsCanBeSpied_; }
bool InfantryTypeClass::IsCanBeSabotaged() const   { return IsCanBeSabotaged_; }
bool InfantryTypeClass::IsCanBeStolen() const      { return IsCanBeStolen_; }
bool InfantryTypeClass::IsCanBeHijacked() const    { return IsCanBeHijacked_; }

// ============================================================================
// Extended infantry accessors
// ============================================================================

bool InfantryTypeClass::Is_Civilian() const
{
    // A civilian infantry unit is one that belongs to the civilian house and
    // is not a combatant.  In the full binary this is determined by the
    // owner house type; the standalone build uses the Insignificant flag
    // combined with the absence of weapons.
    return IsInsignificant_ && WeaponCount == 0 && !Engineer && !Thief;
}

bool InfantryTypeClass::Is_Spy() const
{
    // In Yuri's Revenge, the spy is a special infantry type identified by
    // the Thief flag (the spy inherits from the thief class in the binary).
    // The standalone build checks both Thief and a specific "spy" name match.
    return Thief;
}

int32 InfantryTypeClass::Get_Sequence_Count() const
{
    // Returns the number of SHP animation sequences defined for this
    // infantry type.  The Sequence enum defines 35 possible sequences
    // (Ready, Guard, Prone, Walk, FireUp, Down, etc.).  Not all sequences
    // are used by every infantry type; the full binary checks the SHP
    // frame count for each sequence.  Here we return the full count.
    return static_cast<int32>(Sequence::Count);
}

Point2D InfantryTypeClass::Get_Image_Size() const
{
    // Infantry are SHP-rendered.  The image size is derived from the SHP
    // bounds.  If Resolve_SHP_References hasn't run yet, return a default.
    if (ImageSize.X > 0 || ImageSize.Y > 0)
        return ImageSize;

    // Default infantry SHP size.
    return Point2D(28, 28);
}

AbstractType InfantryTypeClass::Get_Build_Queue_Type() const
{
    // Infantry are produced by the barracks.
    return AbstractType::Infantry;
}

// ============================================================================
// 根据游戏行为，可知 SHP_References 负责下面这段逻辑。
//
//  Binds the infantry SHP image.  Infantry use SHP files (not voxels) for
//  their rendering, with multiple sequences for walking, firing, prone, etc.
// ============================================================================

void InfantryTypeClass::Resolve_SHP_References()
{
    // Chain the parent for cameo and base SHP references.
    TechnoTypeClass::Resolve_SHP_References();

    // Infantry SHP image defaults to the ImageFile name.  The parent's
    // Resolve_SHP_References already attempts to load ImageShape from
    // ImageFile; here we just set the image size if the shape is loaded.
    if (ImageShape != nullptr)
    {
        if (ImageSize.X == 0 && ImageSize.Y == 0)
        {
            // The full binary reads the SHP header for width/height.
            // Default to standard infantry size.
            ImageSize.X = 28;
            ImageSize.Y = 28;
        }
    }
}

// ============================================================================
// 根据游戏行为，可知 Cameo_Data 负责下面这段逻辑。
//
//  Returns the cameo SHP for the sidebar build button.  Falls back to the
//  parent implementation if no infantry-specific cameo is set.
// ============================================================================

SHPStruct* InfantryTypeClass::Get_Cameo_Data() const
{
    // Use the cameo shape resolved by Resolve_SHP_References.
    if (CameoShape != nullptr)
        return CameoShape;

    // Fall back to the parent implementation.
    return ObjectTypeClass::Get_Cameo_Data();
}

// ============================================================================
// LoadFromINI
// ============================================================================

bool InfantryTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // Chain the parent first.
    TechnoTypeClass::LoadFromINI(pINI);

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // ------------------------------------------------------------------
    // Special-unit classification
    // ------------------------------------------------------------------
    Engineer = pINI->ReadBool(section, "Engineer", Engineer);
    Thief    = pINI->ReadBool(section, "Thief",    Thief);

    IsCow_   = Cow;
    IsDog_   = Dog;
    IsBoris_ = Boris;

    // Sync parent
    TechnoTypeClass::IsCow  = Cow;
    TechnoTypeClass::IsDog  = Dog;
    TechnoTypeClass::IsBoris = Boris;

    // ------------------------------------------------------------------
    // Combat flags (with underscore-suffixed own fields)
    // ------------------------------------------------------------------
    IsMissileSpawn_   = pINI->ReadBool(section, "MissileSpawn",   IsMissileSpawn_);
    IsDisableable_    = pINI->ReadBool(section, "Disableable",    IsDisableable_);

    TechnoTypeClass::IsArmed        = IsArmed_;
    TechnoTypeClass::IsMissileSpawn = IsMissileSpawn_;
    TechnoTypeClass::IsFake         = IsFake_;
    TechnoTypeClass::IsDisableable  = IsDisableable_;

    // ------------------------------------------------------------------
    // CanBeXxx interaction flags
    // ------------------------------------------------------------------
    IsCanBeOccupied_    = pINI->ReadBool(section, "CanBeOccupied",   IsCanBeOccupied_);
    IsCanBeCaptured_    = pINI->ReadBool(section, "Capturable",   IsCanBeCaptured_);
    IsCanBeRepaired_    = pINI->ReadBool(section, "Repairable",   IsCanBeRepaired_);
    IsCanBePowered_     = pINI->ReadBool(section, "Powered",    IsCanBePowered_);
    IsCanBeSpied_       = pINI->ReadBool(section, "Spyable",      IsCanBeSpied_);

    // ------------------------------------------------------------------
    // Crush / movement flags
    // ------------------------------------------------------------------
    IsCrushable   = pINI->ReadBool(section, "Crushable",   IsCrushable);
    IsCrushable2  = pINI->ReadBool(section, "Crushable",  IsCrushable2);
    IsTeleporter  = pINI->ReadBool(section, "Teleporter",  IsTeleporter);
    IsBomb        = pINI->ReadBool(section, "Bomb",        IsBomb);

    TechnoTypeClass::IsCrushable  = IsCrushable;
    TechnoTypeClass::IsCrushable2 = IsCrushable2;
    TechnoTypeClass::IsTeleporter = IsTeleporter;
    TechnoTypeClass::IsChrono     = IsChrono;
    TechnoTypeClass::IsBomb       = IsBomb;

    // ------------------------------------------------------------------
    // Pips
    // ------------------------------------------------------------------
    Pip       = pINI->ReadPipIdx(section, "Pip",       Pip);
    OccupyPip = pINI->ReadPipIdx(section, "OccupyPip", OccupyPip);

    // ------------------------------------------------------------------
    // Voxel / SHP classification
    // ------------------------------------------------------------------
    IsVoxel    = pINI->ReadBool(section, "Voxel", IsVoxel);
    Voxel      = IsVoxel;
    IsVoxel_   = IsVoxel;

    HasTurret     = pINI->ReadBool(section, "Turret",     HasTurret);
    CanCloak      = pINI->ReadBool(section, "Cloakable",  CanCloak);
    HasDeployer   = pINI->ReadBool(section, "Deployer",   HasDeployer);

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
    TurretROT= pINI->ReadInteger(section, "ROT", TurretROT);

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

    if (EliteWeaponCount < 0) EliteWeaponCount = 0;
    if (EliteWeaponCount > 2) EliteWeaponCount = 2;
    for (int32 i = 0; i < EliteWeaponCount; ++i)
    {
        std::memset(&EliteWeapons[i], 0, sizeof(WeaponStruct));
    }

    DeathWeaponIndex = pINI->ReadInteger(section, "DeathWeapon", -1);

    // ------------------------------------------------------------------
    // Deploy fire sequence
    // ------------------------------------------------------------------
    int32 deploySeq = pINI->ReadInteger(section, "DeployFire",
                                        static_cast<int32>(DeployFireSequence));
    DeployFireSequence = static_cast<Sequence>(deploySeq);

    // ------------------------------------------------------------------
    // Threat / score
    // ------------------------------------------------------------------
    ThreatPosedValue = pINI->ReadFloat(section, "ThreatPosed", ThreatPosedValue);
    Score            = pINI->ReadInteger(section, "Score", Score);

    // ------------------------------------------------------------------
    // Special classification flags
    // ------------------------------------------------------------------
    IsIvan            = pINI->ReadBool(section, "Ivan",            IsIvan);
    IsCarryall        = pINI->ReadBool(section, "Carryall",        IsCarryall);
    IsSimpleDeployer  = pINI->ReadBool(section, "Deployer",  IsSimpleDeployer);
    IsSonic           = pINI->ReadBool(section, "Sonic",           IsSonic);
    IsBalloonHover    = pINI->ReadBool(section, "BalloonHover",    IsBalloonHover);
    IsCyborg          = pINI->ReadBool(section, "Cyborg",          IsCyborg);
    IsConsideredAircraft = pINI->ReadBool(section, "ConsideredAircraft", IsConsideredAircraft);

    TechnoTypeClass::IsIvan            = IsIvan;
    TechnoTypeClass::IsLeader          = IsLeader;
    TechnoTypeClass::IsCarryall        = IsCarryall;
    TechnoTypeClass::IsTrain           = IsTrain;
    TechnoTypeClass::IsSimpleDeployer  = IsSimpleDeployer;
    TechnoTypeClass::IsFirebase        = IsFirebase;
    TechnoTypeClass::IsSonic           = IsSonic;
    TechnoTypeClass::IsVan             = IsVan;
    TechnoTypeClass::IsBalloonHover    = IsBalloonHover;
    TechnoTypeClass::IsCyborg          = IsCyborg;
    TechnoTypeClass::IsConsideredAircraft = IsConsideredAircraft;
    TechnoTypeClass::IsConsideredVehicle  = IsConsideredVehicle;

    // ------------------------------------------------------------------
    // Immunity flags
    // ------------------------------------------------------------------
    IsImmuneToPsionics  = pINI->ReadBool(section, "ImmuneToPsionics",  IsImmuneToPsionics);
    IsImmuneToPoison    = pINI->ReadBool(section, "ImmuneToPoison",    IsImmuneToPoison);
    IsImmuneToRadiation = pINI->ReadBool(section, "ImmuneToRadiation", IsImmuneToRadiation);

    // ------------------------------------------------------------------
    // Misc flags
    // ------------------------------------------------------------------
    IsToProtect       = pINI->ReadBool(section, "ToProtect",       IsToProtect);
    IsNominal         = pINI->ReadBool(section, "Nominal",         IsNominal);
    IsRadarInvisible  = pINI->ReadBool(section, "RadarInvisible",  IsRadarInvisible);
    IsDontScore       = pINI->ReadBool(section, "DontScore",       IsDontScore);
    IsNoThreat        = pINI->ReadBool(section, "NoThreat",        IsNoThreat);
    IsSensorsSight    = pINI->ReadBool(section, "SensorsSight",    IsSensorsSight);
    IsHunterSeeker    = pINI->ReadBool(section, "HunterSeeker",    IsHunterSeeker);

    IsNaval            = pINI->ReadBool(section, "Naval",            IsNaval);
    IsLand             = pINI->ReadBool(section, "Land",             IsLand);
    IsAir              = pINI->ReadBool(section, "Air",              IsAir);
    IsNotHuman         = pINI->ReadBool(section, "NotHuman",         IsNotHuman);
    IsOrganic          = pINI->ReadBool(section, "Organic",          IsOrganic);
    IsNeutral          = pINI->ReadBool(section, "Neutral",          IsNeutral);

    // ------------------------------------------------------------------
    // Factory type
    // ------------------------------------------------------------------
    char factoryBuf[32];
    pINI->ReadString(section, "Factory", "Infantry", factoryBuf, sizeof(factoryBuf));
    if (!_strcmpi(factoryBuf, "Building"))   Factory = AbstractType::Building;
    else if (!_strcmpi(factoryBuf, "Infantry")) Factory = AbstractType::Infantry;
    else if (!_strcmpi(factoryBuf, "Unit"))  Factory = AbstractType::Unit;
    else if (!_strcmpi(factoryBuf, "Aircraft")) Factory = AbstractType::Aircraft;
    else                                     Factory = AbstractType::Infantry;


    // generated-ini-reads
    // ------------------------------------------------------------------
    // Full key set - every field keeps its current value when the key
    // is absent, so partially specified sections stay valid.
    // ------------------------------------------------------------------
    CCINIClass* pArt = &CCINIClass::INI_Art;
    if (pArt == nullptr)
        pArt = pINI;

    Pip = pINI->ReadPipIdx(section, "Pip", Pip);
    OccupyPip = pINI->ReadPipIdx(section, "OccupyPip", OccupyPip);
    { char _buf[0x40]; if (pINI->ReadString(section, "OccupyWeapon", "", _buf, sizeof(_buf)) > 0) { WeaponTypeClass* _p = WeaponTypeClass::FindOrAllocate(_buf); if (_p) OccupyWeapon = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "EliteOccupyWeapon", "", _buf, sizeof(_buf)) > 0) { WeaponTypeClass* _p = WeaponTypeClass::FindOrAllocate(_buf); if (_p) EliteOccupyWeapon = _p; } }
    pINI->ReadString(section, "VoiceComment", VoiceComment, VoiceComment, sizeof(VoiceComment));
    { char _buf[0x40]; if (pINI->ReadString(section, "DeadBodies", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) DeadBodies = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DeathAnims", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) DeathAnims = _p; } }
    Cyborg = pINI->ReadBool(section, "Cyborg", Cyborg);
    NotHuman = pINI->ReadBool(section, "NotHuman", NotHuman);
    { char _buf[0x40]; if (pINI->ReadString(section, "EnterWaterSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) EnterWaterSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "LeaveWaterSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) LeaveWaterSound = _i; } }
    Fearless = pINI->ReadBool(section, "Fearless", Fearless);
    Fraidycat = pINI->ReadBool(section, "Fraidycat", Fraidycat);
    Infiltrate = pINI->ReadBool(section, "Infiltrate", Infiltrate);
    Ivan = pINI->ReadBool(section, "Ivan", Ivan);
    Occupier = pINI->ReadBool(section, "Occupier", Occupier);
    Assaulter = pINI->ReadBool(section, "Assaulter", Assaulter);
    DetectionDistance = pINI->ReadInteger(section, "DetectionDistance", DetectionDistance);
    HarvestRate = pINI->ReadInteger(section, "HarvestRate", HarvestRate);
    C4 = pINI->ReadBool(section, "C4", C4);
    Civilian = pINI->ReadBool(section, "Civilian", Civilian);
    Engineer = pINI->ReadBool(section, "Engineer", Engineer);
    TiberiumProof = pINI->ReadBool(section, "TiberiumProof", TiberiumProof);
    Agent = pINI->ReadBool(section, "Agent", Agent);
    Thief = pINI->ReadBool(section, "Thief", Thief);
    VehicleThief = pINI->ReadBool(section, "VehicleThief", VehicleThief);
    Doggie = pINI->ReadBool(section, "Doggie", Doggie);
    Deployer = pINI->ReadBool(section, "Deployer", Deployer);
    DeployedCrushable = pINI->ReadBool(section, "DeployedCrushable", DeployedCrushable);
    UseOwnName = pINI->ReadBool(section, "UseOwnName", UseOwnName);
    JumpJetTurn = pINI->ReadBool(section, "JumpJetTurn", JumpJetTurn);

    // ------------------------------------------------------------------
    // artmd.ini fields
    // ------------------------------------------------------------------
    Crawls = pArt->ReadBool(section, "Crawls", Crawls);
    FireUp = pArt->ReadInteger(section, "FireUp", FireUp);
    FireProne = pArt->ReadInteger(section, "FireProne", FireProne);
    SecondaryFire = pArt->ReadInteger(section, "SecondaryFire", SecondaryFire);
    SecondaryProne = pArt->ReadInteger(section, "SecondaryProne", SecondaryProne);

        return true;
}

// ============================================================================
// SaveToINI
// ============================================================================

bool InfantryTypeClass::SaveToINI(CCINIClass* pINI) const
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // Chain the parent.
    const_cast<InfantryTypeClass*>(this)->TechnoTypeClass::SaveToINI(pINI);

    pINI->WriteInteger(section, "WeaponCount",      WeaponCount);
    pINI->WriteInteger(section, "EliteWeaponCount", EliteWeaponCount);
    pINI->WriteInteger(section, "DeathWeapon",      DeathWeaponIndex);
    pINI->WriteInteger(section, "WeaponCharge",     WeaponCharge);
    pINI->WriteInteger(section, "DeployFire", static_cast<int32>(DeployFireSequence));
    pINI->WriteFloat(section,  "ThreatPosed",      ThreatPosedValue);

    pINI->WriteBool(section, "Engineer", Engineer);
    pINI->WriteBool(section, "Thief",    Thief);
    pINI->WriteBool(section, "Cow",      Cow);
    pINI->WriteBool(section, "Dog",      Dog);
    pINI->WriteBool(section, "Boris",    Boris);

    pINI->WriteBool(section, "Armed",         IsArmed_);
    pINI->WriteBool(section, "MissileSpawn",  IsMissileSpawn_);
    pINI->WriteBool(section, "Fake",          IsFake_);
    pINI->WriteBool(section, "Disableable",   IsDisableable_);

    pINI->WriteBool(section, "CanSuppressed",    IsCanBeSuppressed_);
    pINI->WriteBool(section, "CanBeOccupied",    IsCanBeOccupied_);
    pINI->WriteBool(section, "CanBeDriven",      IsCanBeDriven_);
    pINI->WriteBool(section, "Capturable",    IsCanBeCaptured_);
    pINI->WriteBool(section, "Repairable",    IsCanBeRepaired_);
    pINI->WriteBool(section, "CanBeSold",        IsCanBeSold_);
    pINI->WriteBool(section, "Powered",     IsCanBePowered_);
    pINI->WriteBool(section, "CanBeDestroyed",   IsCanBeDestroyed_);
    pINI->WriteBool(section, "CanBeDamaged",     IsCanBeDamaged_);
    pINI->WriteBool(section, "CanBeInfiltrated", IsCanBeInfiltrated_);
    pINI->WriteBool(section, "Spyable",       IsCanBeSpied_);
    pINI->WriteBool(section, "CanBeSabotaged",   IsCanBeSabotaged_);
    pINI->WriteBool(section, "CanBeStolen",      IsCanBeStolen_);
    pINI->WriteBool(section, "CanBeHijacked",    IsCanBeHijacked_);

    pINI->WriteBool(section, "Crushable",   IsCrushable);
    pINI->WriteBool(section, "Crushable",  IsCrushable2);
    pINI->WriteBool(section, "Teleporter",  IsTeleporter);
    pINI->WriteBool(section, "Chrono",      IsChrono);
    pINI->WriteBool(section, "Bomb",        IsBomb);

    pINI->WriteBool(section, "Voxel",     IsVoxel);
    pINI->WriteBool(section, "Turret",    HasTurret);
    pINI->WriteBool(section, "Cloakable", CanCloak);
    pINI->WriteBool(section, "Deployer",  HasDeployer);
    pINI->WriteBool(section, "Undeployer",HasUndeployer);
    pINI->WriteBool(section, "Firewall",  HasFirewall);

    pINI->WriteBool(section, "Ivan",            IsIvan);
    pINI->WriteBool(section, "Leader",          IsLeader);
    pINI->WriteBool(section, "Carryall",        IsCarryall);
    pINI->WriteBool(section, "Train",           IsTrain);
    pINI->WriteBool(section, "Deployer",  IsSimpleDeployer);
    pINI->WriteBool(section, "Firebase",        IsFirebase);
    pINI->WriteBool(section, "Sonic",           IsSonic);
    pINI->WriteBool(section, "Van",             IsVan);
    pINI->WriteBool(section, "BalloonHover",    IsBalloonHover);
    pINI->WriteBool(section, "Cyborg",          IsCyborg);
    pINI->WriteBool(section, "ConsideredAircraft", IsConsideredAircraft);
    pINI->WriteBool(section, "ConsideredVehicle",  IsConsideredVehicle);

    pINI->WriteBool(section, "ImmuneToPsionics",  IsImmuneToPsionics);
    pINI->WriteBool(section, "ImmuneToPoison",    IsImmuneToPoison);
    pINI->WriteBool(section, "ImmuneToRadiation", IsImmuneToRadiation);
    pINI->WriteBool(section, "ImmuneToBerserk",   IsImmuneToBerserk);
    pINI->WriteBool(section, "ImmuneToEMP",       IsImmuneToEMP);

    pINI->WriteBool(section, "Tilter",          IsTilter);
    pINI->WriteBool(section, "ToProtect",       IsToProtect);
    pINI->WriteBool(section, "Nominal",         IsNominal);
    pINI->WriteBool(section, "RadarInvisible",  IsRadarInvisible);
    pINI->WriteBool(section, "DontScore",       IsDontScore);
    pINI->WriteBool(section, "NoThreat",        IsNoThreat);
    pINI->WriteBool(section, "SensorsSight",    IsSensorsSight);
    pINI->WriteBool(section, "HunterSeeker",    IsHunterSeeker);

    pINI->WriteBool(section, "Naval",            IsNaval);
    pINI->WriteBool(section, "Land",             IsLand);
    pINI->WriteBool(section, "Air",              IsAir);
    pINI->WriteBool(section, "NotHuman",         IsNotHuman);
    pINI->WriteBool(section, "Organic",          IsOrganic);
    pINI->WriteBool(section, "Neutral",          IsNeutral);
    pINI->WriteBool(section, "Infiltratable",    IsInfiltratable);
    pINI->WriteBool(section, "Stealthy",         IsStealthy);
    pINI->WriteBool(section, "Healable",         IsHealable);

    return true;
}

// ============================================================================
// CRC
// ============================================================================

void InfantryTypeClass::ComputeCRC(CRCEngine& crc) const
{
    TechnoTypeClass::ComputeCRC(crc);

    crc.AddData(&Engineer,           sizeof(Engineer));
    crc.AddData(&Thief,              sizeof(Thief));
    crc.AddData(&Cow,                sizeof(Cow));
    crc.AddData(&Dog,                sizeof(Dog));
    crc.AddData(&Boris,              sizeof(Boris));
    crc.AddData(&IsArmed_,           sizeof(IsArmed_));
    crc.AddData(&IsMissileSpawn_,    sizeof(IsMissileSpawn_));
    crc.AddData(&IsFake_,            sizeof(IsFake_));
    crc.AddData(&IsDisableable_,     sizeof(IsDisableable_));
    crc.AddData(&IsCanBeSuppressed_, sizeof(IsCanBeSuppressed_));
    crc.AddData(&IsCanBeOccupied_,   sizeof(IsCanBeOccupied_));
    crc.AddData(&IsCanBeDriven_,     sizeof(IsCanBeDriven_));
    crc.AddData(&IsCanBeCaptured_,   sizeof(IsCanBeCaptured_));
    crc.AddData(&IsCanBeRepaired_,   sizeof(IsCanBeRepaired_));
    crc.AddData(&IsCanBeSold_,       sizeof(IsCanBeSold_));
    crc.AddData(&IsCanBePowered_,    sizeof(IsCanBePowered_));
    crc.AddData(&IsCanBeDestroyed_,  sizeof(IsCanBeDestroyed_));
    crc.AddData(&IsCanBeDamaged_,    sizeof(IsCanBeDamaged_));
    crc.AddData(&IsCanBeInfiltrated_,sizeof(IsCanBeInfiltrated_));
    crc.AddData(&IsCanBeSpied_,      sizeof(IsCanBeSpied_));
    crc.AddData(&IsCanBeSabotaged_,  sizeof(IsCanBeSabotaged_));
    crc.AddData(&IsCanBeStolen_,     sizeof(IsCanBeStolen_));
    crc.AddData(&IsCanBeHijacked_,   sizeof(IsCanBeHijacked_));
    crc.AddData(&IsCrushable,        sizeof(IsCrushable));
    crc.AddData(&IsCrushable2,       sizeof(IsCrushable2));
    crc.AddData(&IsTeleporter,       sizeof(IsTeleporter));
    crc.AddData(&IsChrono,           sizeof(IsChrono));
    crc.AddData(&IsBomb,             sizeof(IsBomb));
    crc.AddData(&IsCyborg,           sizeof(IsCyborg));
    crc.AddData(&IsBalloonHover,     sizeof(IsBalloonHover));
    crc.AddData(&ThreatPosedValue,   sizeof(ThreatPosedValue));
    crc.AddData(&DeathWeaponIndex,   sizeof(DeathWeaponIndex));
    crc.AddData(&WeaponCharge,       sizeof(WeaponCharge));
    crc.AddData(&WeaponCount,        sizeof(WeaponCount));
    crc.AddData(Weapons,             static_cast<int32>(sizeof(Weapons)));
    crc.AddData(&EliteWeaponCount,   sizeof(EliteWeaponCount));
    crc.AddData(EliteWeapons,        static_cast<int32>(sizeof(EliteWeapons)));
    crc.AddData(&HasTurret,          sizeof(HasTurret));
    crc.AddData(&CanCloak,           sizeof(CanCloak));
    crc.AddData(&IsVoxel,            sizeof(IsVoxel));
    crc.AddData(&HasDeployer,        sizeof(HasDeployer));
    crc.AddData(&HasUndeployer,      sizeof(HasUndeployer));
    crc.AddData(&HasFirewall,        sizeof(HasFirewall));
    crc.AddData(&DeployFireSequence, sizeof(DeployFireSequence));
}

int32 InfantryTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// InfantryTypeClass - static lookup helpers
// ============================================================================

InfantryTypeClass* InfantryTypeClass::FindOrAllocate(const char* pID)
{
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    InfantryTypeClass* found = Find(pID);
    if (found) return found;
    if (!Array) Init_Array();
    InfantryTypeClass* newItem = GameCreate<InfantryTypeClass>();
    if (newItem)
    {
        strncpy(newItem->ID, pID, sizeof(newItem->ID) - 1);
        newItem->ID[sizeof(newItem->ID) - 1] = '\0';
    }
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}

// ============================================================================
// InfantryTypeClass sequence data
//
//   Every entry of the Sequences table owns a 0x24 byte record.  The first
//   dword is the facing index (the "N"/"NE"/... token from the artmd.ini
//   value); the next two dwords are frame counters; the two VocClass slots
//   and their frame numbers fill the rest.  The original walks the records
//   with a 0x24 stride from SequenceData[0].
// ============================================================================

namespace {

// The 42 sequence names the original's "Sequences" table holds, in order.
const char* const g_SequenceNames[] = {
    "Ready", "Guard", "Prone", "Walk", "FireUp", "Down",
    "Crawl", "Up", "FireProne", "Idle1", "Idle2", "Die1",
    "Die2", "Die3", "Die4", "Die5", "Tread", "Swim",
    "WetIdle1", "WetIdle2", "WetDie1", "WetDie2", "WetAttack", "Hover",
    "Fly", "Tumble", "FireFly", "Deploy", "Deployed", "DeployedFire",
    "DeployedIdle", "Undeploy", "Cheer", "Paradrop", "AirDeathStart",
    "AirDeathFalling", "AirDeathFinish", "Panic", "Shovel", "Carry",
    "SecondaryFire", "SecondaryProne"
};

const int32 g_SequenceCount =
    sizeof(g_SequenceNames) / sizeof(g_SequenceNames[0]);

// Facing tokens in the order the original compares them.
const char* const g_FacingNames[] = {
    "N", "NE", "E", "SE", "S", "SW", "W", "NW"
};

int32 FacingFromName(const char* pName)
{
    for (int32 i = 0; i < 8; ++i)
    {
        if (std::strcmp(pName, g_FacingNames[i]) == 0) {
            return i;
        }
    }
    return 0;
}

} // namespace

const char* InfantryTypeClass::GetSequenceName(int32 index)
{
    if (index < 0 || index >= g_SequenceCount) {
        return nullptr;
    }
    return g_SequenceNames[index];
}

int32 InfantryTypeClass::GetSequenceNameCount()
{
    return g_SequenceCount;
}

// ============================================================================
 // InfantryTypeClass_ParseSequence -
//
//   artmd.ini's section for this infantry is scanned twice per sequence.  The
//   "<Name>" key carries a "%d,%d,%d,%s" value: two frame numbers, a count,
//   and a facing token that selects the facing slot.  The "<Name>Sounds" key
//   then carries a whitespace separated run of "<frame>,<voc name>" pairs,
//   each of which is resolved through VocClass::FindIndexOfName and stored in
//   the sequence's two sound slots.
//
//   ParseSequence exits early when the section has no "Sequence" key at all.
// ============================================================================
void InfantryTypeClass::ParseSequence()
{
    // The section is the art image tag, normally this type's Image name.
    const char* pImage = Image;
    if (pImage == nullptr || pImage[0] == '\0') {
        pImage = ID;
    }

    char section[0x20];
    section[0] = '\0';

    CCINIClass* pArt = CCINIClass::GetArtINI();
    if (pArt == nullptr) {
        return;
    }

    if (pArt->ReadString(pImage, "Sequence", "", section,
                         sizeof(section)) <= 0) {
        return;
    }

    for (int32 seqIndex = 0; seqIndex < g_SequenceCount; ++seqIndex)
    {
        SequenceDataRec& data = SequenceDataAt(seqIndex);

        char value[0x20];
        value[0] = '\0';
        if (pArt->ReadString(section, g_SequenceNames[seqIndex], "", value,
                             sizeof(value)) <= 0) {
            continue;
        }

        // "%d,%d,%d,%s" -> StartFrame, FrameCount, unused, facing token.
        int32 first = 0;
        int32 second = 0;
        char facing[0x20];
        facing[0] = '\0';
        if (std::sscanf(value, "%d,%d,%d,%s", &first, &data.FrameCount,
                        &second, facing) < 4) {
            continue;
        }

        data.Facing = FacingFromName(facing);
        data.Frames[0].StartFrame = first;
        data.Frames[0].FrameCount = second;

        char key[0x40];
        std::sprintf(key, "%sSounds", g_SequenceNames[seqIndex]);

        char sounds[0x20];
        sounds[0] = '\0';
        if (pArt->ReadString(section, key, "", sounds, sizeof(sounds)) <= 0) {
            continue;
        }

        int32 slot = 0;
        char* pToken = std::strtok(sounds, " ,\t");
        while (pToken != nullptr)
        {
            const int32 frame = std::atoi(pToken);

            pToken = std::strtok(nullptr, " ,\t");
            if (pToken == nullptr) {
                break;
            }

            const int32 sound = VocClass::FindIndexOfName(pToken);
            if (sound != -1 && slot < 2)
            {
                data.Sounds[slot].Frame = frame;
                data.Sounds[slot].SoundIndex = sound;
                ++slot;
            }

            pToken = std::strtok(nullptr, " ,\t");
        }
    }
}
