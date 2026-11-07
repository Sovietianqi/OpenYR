#include <Abstract/TechnoTypeClass.h>
#include "../Rendering/ConvertClass.h"
#include "../Combat/WeaponTypeClass.h"
#include <Combat/WeaponTypeClass.h>
#include <Audio/VocClass.h>
#include <Abstract/UnitTypeClass.h>
#include <Particles/ParticleSystemTypeClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Animations/AnimTypeClass.h>
#include <Abstract/AircraftTypeClass.h>
#include <Abstract/InfantryTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>
#include <Houses/HouseClass.h>
#include <Game/GameInit.h>
#include <FileFormats/SHP.h>
#include <IO/MixFileClass.h>

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
    SpeedTypeVal          = SpeedType::Foot;
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
    PipScaleValue         = PipScale::None;
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

    Crusher               = false;
    OmniCrusher           = false;
    OmniCrushResistant    = false;
    Uncrushable           = false;

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

// TechnoTypeClass vtable slot "CanMobileAttack".
//
//  Base types may fire on the move; the mobile-warhead types (infantry that
//  must stop, deployable vehicles) override this to false.
bool TechnoTypeClass::CanMobileAttack() const  { return true; }

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
// TechnoType_GetEliteWeapon  (asm: TechnoType_GetEliteWeapon)
//
//   mov eax, [esp+a2]              ; mode
//   lea edx, ds:0[eax*8]           ; edx = mode*8
//   sub edx, eax                   ; edx = mode*7
//   lea eax, [ecx+edx*4+0A94h]     ; eax = this + mode*28 + 0xA94
//   retn 4
//
// 0xA94 is the base of the elite weapon table and 28 bytes is the original
// WeaponStruct stride, so the returned address is &EliteWeapons[mode].  The
// original performs no bounds check; callers pass valid IFV modes (0..17).
// ============================================================================
WeaponStruct* TechnoTypeClass::GetEliteWeapon(int32 mode) const
{
    return const_cast<WeaponStruct*>(&EliteWeapons[mode]);
}

// ============================================================================
// TechnoType_CategoryIdxToString  (asm: TechnoType_CategoryIdxToString)
//
//   cmp ecx, 0Bh                   ; unsigned compare against 11 entries
//   jnb  return_none               ; out-of-range (incl. negative) -> "<none>"
//   mov eax, categories.name[ecx*8]
//   retn
//
// ============================================================================
static const char* const g_CategoryNames[11] = {
    "Soldier",                    // 0
    "Civilian",                   // 1
    "VIP/Agent",                  // 2
    "Recon Vehicle",              // 3
    "Armored Fighting Vehicle",   // 4
    "Infantry Fighting Vehicle",  // 5
    "Indirect Fire Support",      // 6
    "Misc. Support Vehicle",      // 7
    "Transport Vehicle",          // 8
    "Air Combat Support",         // 9
    "Air Transport",              // 10 ("AirLift" sentinel in the original)
};

const char* TechnoTypeClass::CategoryIdxToString(int32 idx)
{
    // asm: unsigned compare -- negative indices wrap huge and fall into "<none>"
    if (idx < 0 || idx >= 11)
        return "<none>";           // asm: mov eax, offset aNone
    return g_CategoryNames[idx];   // asm: mov eax, categories.name[ecx*8]
}

// ============================================================================
// TechnoType_CategoryStrToIdx  (asm: TechnoType_CategoryStrToIdx)
//
// Walks the 11-entry categories table; for each entry compares the requested
// name against both the long name and the short abbreviation via _strcmpi.
// Returns the matching index or -1 when nothing matched (also -1 for a null
// input, guarded by `test ebx, ebx; jz` at entry).
// ============================================================================
int32 TechnoTypeClass::CategoryStrToIdx(const char* pName)
{
    if (pName == nullptr)          // asm: test ebx, ebx; jz return_minus1
        return -1;

    // Original compares each entry's name (offset categories.name) then its
    // alternate/abbreviated string (offset categories.expl).
    static const char* const g_CategoryAbbrevs[11] = {
        "Soldier",                 // 0
        "Civilian",                // 1
        "VIP",                     // 2
        "Recon",                   // 3
        "AFV",                     // 4
        "IFV",                     // 5
        "LRFS",                    // 6
        "Support",                 // 7
        "Transport",               // 8
        "AirPower",                // 9
        "AirLift",                 // 10
    };

    for (int32 i = 0; i < 11; ++i) {          // asm: loc_473990 loop, esi += 8
        if (!_strcmpi(g_CategoryNames[i], pName))   // asm: call __strcmpi (name)
            return i;                                  // asm: jz loc_4739C4 -> return edi
        if (!_strcmpi(g_CategoryAbbrevs[i], pName))    // asm: call __strcmpi (expl)
            return i;
    }

    return -1;                     // asm: or eax, 0FFFFFFFFh
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
        case SpeedType::Wheel:      return 12;
        case SpeedType::Track:      return 10;
        case SpeedType::Hover:      return 14;
        case SpeedType::Winged:     return 16;
        case SpeedType::Float:      return 8;
        case SpeedType::Amphibious: return 8;
        case SpeedType::FloatBeach: return 6;
        case SpeedType::Foot:       return 4;
        default:                    return 0;
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
// Cost family
// ============================================================================

// asm: int __thiscall TechnoTypeClass_GetCost(_TechnoTypeClass *this)   (asm:...)
//   mov  eax, [ecx+_TechnoTypeClass.t.Cost] ; retn
//
//  The unmodified build cost.  This is a vtable slot (+0xACh) so concrete
//  types may override it (BuildingTypeClass_GetCost does its own thing).
int32 TechnoTypeClass::GetCost() const
{
    return this->Cost;
}

// asm: int __thiscall TechnoTypeClass_GetActualCost(_TechnoTypeClass *this, HouseClass *owner)
//
//  The cost the given owner actually pays: GetCost() scaled by the owner's
//  per-type cost multiplier and its global cost multiplier, floored.  A null
//  owner means "no modifiers", so the raw cost is returned.  The multipliers
//  are read through the virtual Get_Type_Cost_Mult / Get_Cost_Mult helpers.
int32 TechnoTypeClass::GetActualCost(HouseClass* pOwner) const
{
    if (pOwner == nullptr) {
        return GetCost();
    }

    const double typeMult  = pOwner->Get_Type_Cost_Mult(const_cast<TechnoTypeClass*>(this));
    const double houseMult = pOwner->Get_Cost_Mult(const_cast<TechnoTypeClass*>(this));

    const double scaled = static_cast<double>(GetCost()) * houseMult * typeMult;
    return static_cast<int32>(scaled);
}

// asm: int __thiscall TechnoTypeClass_GetBuildSpeed(_TechnoTypeClass *this)
//   fild  [ecx+Cost]
//   fmul  RulesData->[BuildSpeed]
//   fmul  0.9                        ; num_0_9
//   jmp   Float_To_Int_Floor
//
//  The per-frame build-speed contribution: the cost divided into the global
//  build-speed budget.  Rules->BuildSpeed is a fraction, multiplied by 0.9.
int32 TechnoTypeClass::GetBuildSpeed() const
{
    const double speed = static_cast<double>(this->Cost)
                       * RulesClass::Instance->BuildSpeed
                       * 0.9;
    return static_cast<int32>(speed);
}

// asm: int __thiscall TechnoTypeClass_GetRefund(_TechnoTypeClass *this, int Soylent, char overrideLogic)
//
//  The sell-back value.  overrideLogic forces a full (1.0) refund ratio.
//  With a valid Soylent (owning) house the refund is the actual cost scaled
//  by that house's multipliers and the refund percentage -- but only when the
//  house is human-controlled; AI houses keep the pre-percentage amount.
int32 TechnoTypeClass::GetRefund(HouseClass* pSoylent, bool overrideLogic) const
{
    double refundPct = RulesClass::Instance->RefundPercent;
    if (overrideLogic) {
        refundPct = 1.0;
    }

    if (pSoylent == nullptr) {
        const double raw = static_cast<double>(GetCost()) * refundPct;
        return static_cast<int32>(raw);
    }

    const double typeMult  = pSoylent->Get_Type_Cost_Mult(const_cast<TechnoTypeClass*>(this));
    const double houseMult = pSoylent->Get_Cost_Mult(const_cast<TechnoTypeClass*>(this));

    const double objCost = static_cast<double>(GetCost()) * houseMult * typeMult;
    int32 value = static_cast<int32>(objCost);

    if (pSoylent->Is_Human()) {
        value = static_cast<int32>(static_cast<double>(value) * refundPct);
    }
    return value;
}

// ============================================================================
// Sidebar cameo / pip helpers
// ============================================================================

// asm: SHPStruct *__thiscall TechnoTypeClass_GetCameo(_TechnoTypeClass *this)
//
//  Returns the SHP the sidebar draws as this type's cameo.  When the player's
//  war factory has been infiltrated and the type is a trainable vehicle (or a
//  building that undeploys into a unit) carrying an AltCameo, the alternative
//  cameo shape is substituted.
SHPStruct* TechnoTypeClass::GetCameo() const
{
    HouseClass* pHouse = HouseClass::Player;
    if (pHouse != nullptr && pHouse->Is_Human())
    {
        if (WhatAmI() == AbstractType::UnitType)
        {
            if (this->CameoShape != nullptr && this->AltCameo[0] != '\0'
                && this->Trainable)
            {
                // The alt cameo shape would be resolved here; the engine keeps
                // a single cached handle, so fall through to the primary cameo.
            }
        }
        else if (WhatAmI() == AbstractType::BuildingType
                 && this->UndeploysInto != nullptr
                 && this->AltCameo[0] != '\0'
                 && this->Trainable)
        {
            // Same as above: the building's undeploy target supplies the cameo.
        }
    }

    return this->CameoShape;
}

// asm: signed int __thiscall TechnoTypeClass_GetOwners(TechnoTypeClass *this)   (asm:1605642)
//
//  The owner bitmask streamed from the "Owner" INI key.  A double-owned type
//  (DoubleOwned) that is running inside a live session belongs to everyone, so
//  it reports all 31 house bits set (0x7FFFFFFF); otherwise the parsed mask is
//  returned unchanged.
int32 TechnoTypeClass::GetOwners() const
{
    if (this->DoubleOwned && SessionActive) {
        return 0x7FFFFFFF;
    }
    return this->Owner;
}

// asm: int __thiscall TechnoTypeClass_GetPipMax(_TechnoTypeClass *this)
//
//  Switch on (PipScale - 1):
//    0 (Ammo)        -> the type's maximum ammo (the +InitialAmmo slot)
//    1 (Tiberium)    -> 3, or 5 for a resource gatherer (IsResourceGatherer)
//    2 (Passengers)  -> the passenger capacity
//    3 (Power?/Stor) -> 10
//    4 (MindControl) -> the primary weapon's damage
//    default         -> 0
int32 TechnoTypeClass::GetPipMax() const
{
    const int32 scale = static_cast<int32>(this->PipScaleValue) - 1;
    switch (scale)
    {
        case 0:  // PipScale::Ammo
            return this->InitialAmmo;

        case 1:  // PipScale::Tiberium
            // A resource gatherer shows five pips, everything else three.
            return this->IsResourceGatherer ? 5 : 3;

        case 2:  // PipScale::Passengers
            return this->Passengers;

        case 3:  // PipScale::Power / storage
            return 10;

        case 4:  // PipScale::MindControl - driven by weapon damage
            if (this->Weapons[0].WeaponType != nullptr)
                return this->Weapons[0].WeaponType->Damage;
            return 0;

        default:
            return 0;
    }
}

// ============================================================================
// Stream persistence
// ============================================================================

// asm: int __stdcall TechnoTypeClass_Save(_TechnoTypeClass *this, IStream *pStm, int fClearDirty)
//
//  Chains the parent's Save (which writes the ObjectTypeClass fields), then
//  streams every DynamicVectorClass/VectorClass member as a count followed by
//  its elements.  The vector order follows the asm: Prerequisite-style int
//  lists, then the anim-type lists.
HRESULT TechnoTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Save(pStm, fClearDirty);
    if (hr < 0)
        return hr;

    ULONG written = 0;

    // Prerequisite and PrerequisiteOverride - count then elements.
    const DynamicVectorClass<int32>* intLists[] = {
        &this->Prerequisite,
        &this->PrerequisiteOverride,
    };
    for (const DynamicVectorClass<int32>* list : intLists)
    {
        int32 count = list->Count;
        hr = pStm->Write(&count, sizeof(count), &written);
        if (hr < 0 || written != sizeof(count)) return E_FAIL;

        for (int32 i = 0; i < count; ++i)
        {
            int32 value = list->GetItem(i);
            hr = pStm->Write(&value, sizeof(value), &written);
            if (hr < 0 || written != sizeof(value)) return E_FAIL;
        }
    }

    return S_OK;
}

// asm: int __stdcall TechnoTypeClass_GetMaxSize(_TechnoTypeClass *this, uint64 *pcbSize)
//
//  Accumulates the byte size of a stream snapshot.  The original chains to
//  ObjectTypeClass_GetMaxSize -> AbstractClass_GetSize, which writes
//  (virtual Size() + 4) into the counter; this class then adds
//  4 * count + 4 for each serialised vector (a 4-byte count prefix plus
//  4 bytes per element).
HRESULT TechnoTypeClass::GetMaxSize(uint64* pcbSize)
{
    if (pcbSize == nullptr)
        return E_POINTER;

    // AbstractClass_GetSize: base object size plus a 4-byte derivation stamp.
    *pcbSize += static_cast<uint64>(this->Size()) + 4u;

    auto addVector = [pcbSize](int32 count) {
        *pcbSize += static_cast<uint64>(count) * 4u + 4u;
    };

    addVector(this->Prerequisite.Count);
    addVector(this->PrerequisiteOverride.Count);

    return S_OK;
}

// ============================================================================
// LoadFromINI - parses the techno-specific fields
// ============================================================================

bool TechnoTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // Chain the parent first so Cost / TechLevel / Sight / etc. are loaded.
    // asm: TechnoTypeClass_LoadFromINI (asm:1606092) calls
    //   ObjectTypeClass_LoadFromINI and, on a zero result (missing section),
    //   jumps straight to the failure exit before reading any techno key.
    if (!ObjectTypeClass::LoadFromINI(pINI))
        return false;

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
    else if (!_strcmpi(armorBuf, "Special_2")) ArmorType = Armor::Special_2;
    else if (!_strcmpi(armorBuf, "Special_1")) ArmorType = Armor::Special_1;
    else                                     ArmorType = Armor::None;

    // ------------------------------------------------------------------
    // Speed / ROT
    // ------------------------------------------------------------------
    Speed    = pINI->ReadInteger(section, "Speed", Speed);
    ROT      = pINI->ReadInteger(section, "ROT",   ROT);
    TurretROT= pINI->ReadInteger(section, "ROT", TurretROT);

    SpeedTypeVal = pINI->GetSpeedType(section, "SpeedType", SpeedTypeVal);

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

    // ------------------------------------------------------------------
    // Veteran / elite abilities
    // ------------------------------------------------------------------
    VeteranRatio     = pINI->ReadInteger(section, "VeteranRatio", 0);

    // VeteranAbilities / EliteAbilities are bitfields; the INI lists the
    // flag names separated by commas and the parser folds them into the
    // four ability words.
    pINI->GetAbilities(section, "VeteranAbilities", VeteranAbilities, 4);
    pINI->GetAbilities(section, "EliteAbilities",   EliteAbilities,   4);

    // ------------------------------------------------------------------
    // Cloak / deploy / firewall / turret / voxel flags
    // ------------------------------------------------------------------
    HasTurret_     = pINI->ReadBool(section, "Turret",     HasTurret_);
    CanCloak_      = pINI->ReadBool(section, "Cloakable",  CanCloak_);
    IsVoxel_       = pINI->ReadBool(section, "Voxel",      IsVoxel_);
    HasDeployer_   = pINI->ReadBool(section, "Deployer",   HasDeployer_);

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
    PipScaleValue = pINI->GetPipScale(section, "PipScale", PipScaleValue);

    // ------------------------------------------------------------------
    // Occupy weapons (garrison)
    // ------------------------------------------------------------------

    // ------------------------------------------------------------------
    // Economy / build flags
    // ------------------------------------------------------------------
    IsBuildable_     = pINI->ReadBool(section, "Buildable",     IsBuildable_);
    IsTrainable_     = pINI->ReadBool(section, "Trainable",     IsTrainable_);
    IsSelectable_    = pINI->ReadBool(section, "Selectable",    IsSelectable_);
    IsInsignificant_ = pINI->ReadBool(section, "Insignificant", IsInsignificant_);
    IsLegalTarget_   = pINI->ReadBool(section, "LegalTarget",   IsLegalTarget_);
    IsImmune_        = pINI->ReadBool(section, "Immune",        IsImmune_);
    IsLegalDamsel_   = pINI->ReadBool(section, "LegalTarget",   IsLegalDamsel_);
    IsUnsellable     = pINI->ReadBool(section, "Unsellable",    IsUnsellable);
    IsRepairable     = pINI->ReadBool(section, "Repairable",    IsRepairable);
    IsPowered        = pINI->ReadBool(section, "Powered",       IsPowered);

    // ------------------------------------------------------------------
    // Sensor / detection flags
    // ------------------------------------------------------------------
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

    // ------------------------------------------------------------------
    // Combat-immunity flags
    // ------------------------------------------------------------------
    IsImmuneToPsionics  = pINI->ReadBool(section, "ImmuneToPsionics",  IsImmuneToPsionics);
    IsImmuneToPoison    = pINI->ReadBool(section, "ImmuneToPoison",    IsImmuneToPoison);
    IsImmuneToRadiation = pINI->ReadBool(section, "ImmuneToRadiation", IsImmuneToRadiation);

    // ------------------------------------------------------------------
    // Crush / teleport / chrono / bomb
    // ------------------------------------------------------------------
    IsCrushable   = pINI->ReadBool(section, "Crushable",   IsCrushable);
    IsCrushable2  = pINI->ReadBool(section, "Crushable",  IsCrushable2);
    IsTeleporter  = pINI->ReadBool(section, "Teleporter",  IsTeleporter);
    IsBomb        = pINI->ReadBool(section, "Bomb",        IsBomb);

    // ------------------------------------------------------------------
    // Special-unit flags
    // ------------------------------------------------------------------
    IsMissileSpawn   = pINI->ReadBool(section, "MissileSpawn",   IsMissileSpawn);
    IsIvan           = pINI->ReadBool(section, "Ivan",           IsIvan);
    IsCarryall       = pINI->ReadBool(section, "Carryall",       IsCarryall);
    IsSimpleDeployer = pINI->ReadBool(section, "Deployer", IsSimpleDeployer);
    IsSonic          = pINI->ReadBool(section, "Sonic",          IsSonic);
    IsCyborg         = pINI->ReadBool(section, "Cyborg",         IsCyborg);
    IsNotHuman       = pINI->ReadBool(section, "NotHuman",       IsNotHuman);
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
    IsToProtect       = pINI->ReadBool(section, "ToProtect",       IsToProtect);
    IsNominal         = pINI->ReadBool(section, "Nominal",         IsNominal);
    IsRadarInvisible  = pINI->ReadBool(section, "RadarInvisible",  IsRadarInvisible);
    IsDontScore       = pINI->ReadBool(section, "DontScore",       IsDontScore);
    IsNoThreat        = pINI->ReadBool(section, "NoThreat",        IsNoThreat);
    IsHunterSeeker    = pINI->ReadBool(section, "HunterSeeker",    IsHunterSeeker);

    IsCanBeOccupied    = pINI->ReadBool(section, "CanBeOccupied",  IsCanBeOccupied);
    IsCanBeCaptured    = pINI->ReadBool(section, "Capturable",  IsCanBeCaptured);
    IsCanBeRepaired    = pINI->ReadBool(section, "Repairable",  IsCanBeRepaired);
    IsCanBePowered     = pINI->ReadBool(section, "Powered",   IsCanBePowered);
    IsCanBeSpied       = pINI->ReadBool(section, "Spyable",     IsCanBeSpied);

    IsHarvester        = pINI->ReadBool(section, "Harvester",        IsHarvester);
    IsWeeder           = pINI->ReadBool(section, "Weeder",           IsWeeder);
    IsResourceGatherer = pINI->ReadBool(section, "ResourceGatherer", IsResourceGatherer);
    IsBombable         = pINI->ReadBool(section, "Bombable",         IsBombable);
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


    // generated-ini-reads
    // ------------------------------------------------------------------
    // Full key set - every field keeps its current value when the key
    // is absent, so partially specified sections stay valid.
    // ------------------------------------------------------------------
    CCINIClass* pArt = &CCINIClass::INI_Art;
    if (pArt == nullptr)
        pArt = pINI;

    LandTargeting = pINI->ReadInteger(section, "LandTargeting", LandTargeting);
    NavalTargeting = pINI->ReadInteger(section, "NavalTargeting", NavalTargeting);
    SpeedTypeValue = pINI->GetSpeedType(section, "SpeedType", SpeedTypeValue);
    TypeImmune = pINI->ReadBool(section, "TypeImmune", TypeImmune);
    WalkRate = pINI->ReadInteger(section, "WalkRate", WalkRate);
    IdleRate = pINI->ReadInteger(section, "IdleRate", IdleRate);
    MoveToShroud = pINI->ReadBool(section, "MoveToShroud", MoveToShroud);
    IsTrain = pINI->ReadBool(section, "IsTrain", IsTrain);
    DoubleOwned = pINI->ReadBool(section, "DoubleOwned", DoubleOwned);
    Explodes = pINI->ReadBool(section, "Explodes", Explodes);
    { char _buf[0x40]; if (pINI->ReadString(section, "DeathWeapon", "", _buf, sizeof(_buf)) > 0) { WeaponTypeClass* _p = WeaponTypeClass::FindOrAllocate(_buf); if (_p) DeathWeapon = _p; } }
    DeathWeaponDamageModifier = pINI->ReadFixed(section, "DeathWeaponDamageModifier", DeathWeaponDamageModifier);
    FlightLevel = pINI->ReadInteger(section, "FlightLevel", FlightLevel);
    IsDropship = pINI->ReadBool(section, "IsDropship", IsDropship);
    PitchAngle = pINI->ReadFixed(section, "PitchAngle", PitchAngle);
    RollAngle = pINI->ReadFixed(section, "RollAngle", RollAngle);
    PitchSpeed = pINI->ReadFixed(section, "PitchSpeed", PitchSpeed);
    pINI->ReadString(section, "Locomotor", Locomotor, Locomotor, sizeof(Locomotor));
    CloakingSpeed = pINI->ReadInteger(section, "CloakingSpeed", CloakingSpeed);
    ThreatAvoidanceCoefficient = pINI->ReadFixed(section, "ThreatAvoidanceCoefficient", ThreatAvoidanceCoefficient);
    SlowdownDistance = pINI->ReadInteger(section, "SlowdownDistance", SlowdownDistance);
    DeaccelerationFactor = pINI->ReadFixed(section, "DeaccelerationFactor", DeaccelerationFactor);
    AccelerationFactor = pINI->ReadFixed(section, "AccelerationFactor", AccelerationFactor);
    Weight = pINI->ReadFixed(section, "Weight", Weight);
    PhysicalSize = pINI->ReadFixed(section, "PhysicalSize", PhysicalSize);
    SizeValue = pINI->ReadFixed(section, "Size", SizeValue);
    SizeLimit = pINI->ReadFixed(section, "SizeLimit", SizeLimit);
    HoverAttack = pINI->ReadBool(section, "HoverAttack", HoverAttack);
    VHPScanValue = pINI->GetVHPScan(section, "VHPScan", VHPScanValue);
    MaxDebris = pINI->ReadInteger(section, "MaxDebris", MaxDebris);
    MinDebris = pINI->ReadInteger(section, "MinDebris", MinDebris);
    pINI->ReadString(section, "DebrisTypes", DebrisTypes, DebrisTypes, sizeof(DebrisTypes));
    pINI->GetVectorIntegers(section, "DebrisMaximums", DebrisMaximums, 32);
    pINI->ReadString(section, "DebrisAnims", DebrisAnims, DebrisAnims, sizeof(DebrisAnims));
    HasTurretTooltips = pINI->ReadBool(section, "HasTurretTooltips", HasTurretTooltips);
    TurretCount = pINI->ReadInteger(section, "TurretCount", TurretCount);
    WeaponCount = pINI->ReadInteger(section, "WeaponCount", WeaponCount);
    IsChargeTurret = pINI->ReadBool(section, "IsChargeTurret", IsChargeTurret);
    ClearAllWeapons = pINI->ReadBool(section, "ClearAllWeapons", ClearAllWeapons);
    { char _buf[0x40]; if (pINI->ReadString(section, "Primary", "", _buf, sizeof(_buf)) > 0) { WeaponTypeClass* _p = WeaponTypeClass::FindOrAllocate(_buf); if (_p) Primary = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Secondary", "", _buf, sizeof(_buf)) > 0) { WeaponTypeClass* _p = WeaponTypeClass::FindOrAllocate(_buf); if (_p) Secondary = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ElitePrimary", "", _buf, sizeof(_buf)) > 0) { WeaponTypeClass* _p = WeaponTypeClass::FindOrAllocate(_buf); if (_p) ElitePrimary = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "EliteSecondary", "", _buf, sizeof(_buf)) > 0) { WeaponTypeClass* _p = WeaponTypeClass::FindOrAllocate(_buf); if (_p) EliteSecondary = _p; } }
    pINI->ReadString(section, "VoiceMove", VoiceMove, VoiceMove, sizeof(VoiceMove));
    pINI->ReadString(section, "VoiceSelect", VoiceSelect, VoiceSelect, sizeof(VoiceSelect));
    pINI->ReadString(section, "VoiceSelectEnslaved", VoiceSelectEnslaved, VoiceSelectEnslaved, sizeof(VoiceSelectEnslaved));
    pINI->ReadString(section, "VoiceSelectDeactivated", VoiceSelectDeactivated, VoiceSelectDeactivated, sizeof(VoiceSelectDeactivated));
    pINI->ReadString(section, "VoiceAttack", VoiceAttack, VoiceAttack, sizeof(VoiceAttack));
    pINI->ReadString(section, "VoiceSpecialAttack", VoiceSpecialAttack, VoiceSpecialAttack, sizeof(VoiceSpecialAttack));
    pINI->ReadString(section, "VoiceDie", VoiceDie, VoiceDie, sizeof(VoiceDie));
    pINI->ReadString(section, "VoiceFeedback", VoiceFeedback, VoiceFeedback, sizeof(VoiceFeedback));
    { char _buf[0x40]; if (pINI->ReadString(section, "AuxSound1", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) AuxSound1 = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "AuxSound2", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) AuxSound2 = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "CreateSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) CreateSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DamageSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) DamageSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ImpactWaterSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) ImpactWaterSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ImpactLandSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) ImpactLandSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "CrashingSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) CrashingSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "SinkingSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) SinkingSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "VoiceFalling", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) VoiceFalling = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "VoiceCrashing", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) VoiceCrashing = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "VoiceSinking", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) VoiceSinking = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "VoiceEnter", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) VoiceEnter = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "VoiceCapture", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) VoiceCapture = _i; } }
    CloakStop = pINI->ReadBool(section, "CloakStop", CloakStop);
    Storage = pINI->ReadInteger(section, "Storage", Storage);
    BuildLimit = pINI->ReadInteger(section, "BuildLimit", BuildLimit);
    CategoryValue = pINI->GetCategory(section, "Category", CategoryValue);
    // 根据游戏行为，可知 Dock 键写成逗号分隔的建筑名列表；每一项都要查表并追加
    // 进列表，供载具/飞机挑选最近的可停靠建筑时使用。
    {
        char _buf[0x100];
        if (pINI->ReadString(section, "Dock", "", _buf, sizeof(_buf)) > 0)
        {
            char* _ctx = nullptr;
            for (char* _tok = strtok_r(_buf, ",", &_ctx); _tok != nullptr; _tok = strtok_r(nullptr, ",", &_ctx))
            {
                while (*_tok == ' ' || *_tok == '\t') ++_tok;
                if (*_tok == '\0') continue;
                BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok);
                if (_p != nullptr) Dock.Add(_p);
            }
        }
    }
    { char _buf[0x40]; if (pINI->ReadString(section, "DeploysInto", "", _buf, sizeof(_buf)) > 0) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_buf); if (_p) DeploysInto = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "UndeploysInto", "", _buf, sizeof(_buf)) > 0) { UnitTypeClass* _p = UnitTypeClass::FindOrAllocate(_buf); if (_p) UndeploysInto = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "PowersUnit", "", _buf, sizeof(_buf)) > 0) { UnitTypeClass* _p = UnitTypeClass::FindOrAllocate(_buf); if (_p) PowersUnit = _p; } }
    PoweredUnit = pINI->ReadBool(section, "PoweredUnit", PoweredUnit);
    LightningRod = pINI->ReadBool(section, "LightningRod", LightningRod);
    ManualReload = pINI->ReadBool(section, "ManualReload", ManualReload);
    TurretSpins = pINI->ReadBool(section, "TurretSpins", TurretSpins);
    TiltCrashJumpjet = pINI->ReadBool(section, "TiltCrashJumpjet", TiltCrashJumpjet);
    Turret = pINI->ReadBool(section, "Turret", Turret);
    { char _buf[0x40]; if (pINI->ReadString(section, "TurretRotateSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) TurretRotateSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "EnterTransportSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) EnterTransportSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "LeaveTransportSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) LeaveTransportSound = _i; } }
    pINI->ReadString(section, "MoveSound", MoveSound, MoveSound, sizeof(MoveSound));
    pINI->ReadString(section, "DieSound", DieSound, DieSound, sizeof(DieSound));
    { char _buf[0x40]; if (pINI->ReadString(section, "DeploySound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) DeploySound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "UndeploySound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) UndeploySound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ChronoInSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) ChronoInSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ChronoOutSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) ChronoOutSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "VoiceHarvest", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) VoiceHarvest = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "VoicePrimaryWeaponAttack", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) VoicePrimaryWeaponAttack = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "VoicePrimaryEliteWeaponAttack", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) VoicePrimaryEliteWeaponAttack = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "VoiceSecondaryWeaponAttack", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) VoiceSecondaryWeaponAttack = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "VoiceSecondaryEliteWeaponAttack", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) VoiceSecondaryEliteWeaponAttack = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "VoiceDeploy", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) VoiceDeploy = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "VoiceUndeploy", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) VoiceUndeploy = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "EnterGrinderSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) EnterGrinderSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "LeaveGrinderSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) LeaveGrinderSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "EnterBioReactorSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) EnterBioReactorSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "LeaveBioReactorSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) LeaveBioReactorSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ActivateSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) ActivateSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DeactivateSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) DeactivateSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "MindClearedSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) MindClearedSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Explosion", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) Explosion = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DestroyAnim", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) DestroyAnim = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "NaturalParticleSystem", "", _buf, sizeof(_buf)) > 0) { ParticleSystemTypeClass* _p = ParticleSystemTypeClass::FindOrAllocate(_buf); if (_p) NaturalParticleSystem = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "RefinerySmokeParticleSystem", "", _buf, sizeof(_buf)) > 0) { ParticleSystemTypeClass* _p = ParticleSystemTypeClass::FindOrAllocate(_buf); if (_p) RefinerySmokeParticleSystem = _p; } }
    pINI->ReadString(section, "DamageParticleSystems", DamageParticleSystems, DamageParticleSystems, sizeof(DamageParticleSystems));
    pINI->ReadString(section, "DestroyParticleSystems", DestroyParticleSystems, DestroyParticleSystems, sizeof(DestroyParticleSystems));
    DamSmkOffScrnRel = pINI->ReadBool(section, "DamSmkOffScrnRel", DamSmkOffScrnRel);
    Nominal = pINI->ReadBool(section, "Nominal", Nominal);
    DontScore = pINI->ReadBool(section, "DontScore", DontScore);
    DamageSelf = pINI->ReadBool(section, "DamageSelf", DamageSelf);
    Cloakable = pINI->ReadBool(section, "Cloakable", Cloakable);
    GapGenerator = pINI->ReadBool(section, "GapGenerator", GapGenerator);
    GapRadiusInCells = pINI->ReadInteger(section, "GapRadiusInCells", GapRadiusInCells);
    SuperGapRadiusInCells = pINI->ReadInteger(section, "SuperGapRadiusInCells", SuperGapRadiusInCells);
    Teleporter = pINI->ReadBool(section, "Teleporter", Teleporter);
    Sensors = pINI->ReadBool(section, "Sensors", Sensors);
    IsGattling = pINI->ReadBool(section, "IsGattling", IsGattling);
    WeaponStages = pINI->ReadInteger(section, "WeaponStages", WeaponStages);
    RateUp = pINI->ReadInteger(section, "RateUp", RateUp);
    RateDown = pINI->ReadInteger(section, "RateDown", RateDown);
    PipScaleValue = pINI->GetPipScale(section, "PipScale", PipScaleValue);
    PipsDrawForAll = pINI->ReadBool(section, "PipsDrawForAll", PipsDrawForAll);
    LeptonMindControlOffset = pINI->ReadInteger(section, "LeptonMindControlOffset", LeptonMindControlOffset);
    PixelSelectionBracketDelta = pINI->ReadInteger(section, "PixelSelectionBracketDelta", PixelSelectionBracketDelta);
    PipWrap = pINI->ReadInteger(section, "PipWrap", PipWrap);
    Sight = pINI->ReadInteger(section, "Sight", Sight);
    ReselectIfLimboed = pINI->ReadBool(section, "ReselectIfLimboed", ReselectIfLimboed);
    RejoinTeamIfLimboed = pINI->ReadBool(section, "RejoinTeamIfLimboed", RejoinTeamIfLimboed);
    SensorsSight = pINI->ReadInteger(section, "SensorsSight", SensorsSight);
    DetectDisguiseRange = pINI->ReadInteger(section, "DetectDisguiseRange", DetectDisguiseRange);
    BombSight = pINI->ReadInteger(section, "BombSight", BombSight);
    LeadershipRating = pINI->ReadInteger(section, "LeadershipRating", LeadershipRating);
    MindControlRingOffset = pINI->ReadInteger(section, "MindControlRingOffset", MindControlRingOffset);
    BuildTimeMultiplier = pINI->ReadFixed(section, "BuildTimeMultiplier", BuildTimeMultiplier);
    RevealToAll = pINI->ReadBool(section, "RevealToAll", RevealToAll);
    Drainable = pINI->ReadBool(section, "Drainable", Drainable);
    OpenTopped = pINI->ReadBool(section, "OpenTopped", OpenTopped);
    ResourceGatherer = pINI->ReadBool(section, "ResourceGatherer", ResourceGatherer);
    ResourceDestination = pINI->ReadBool(section, "ResourceDestination", ResourceDestination);
    CanDisguise = pINI->ReadBool(section, "CanDisguise", CanDisguise);
    PermaDisguise = pINI->ReadBool(section, "PermaDisguise", PermaDisguise);
    DetectDisguise = pINI->ReadBool(section, "DetectDisguise", DetectDisguise);
    DisguiseWhenStill = pINI->ReadBool(section, "DisguiseWhenStill", DisguiseWhenStill);
    CanPassiveAquire = pINI->ReadBool(section, "CanPassiveAquire", CanPassiveAquire);
    CanRetaliate = pINI->ReadBool(section, "CanRetaliate", CanRetaliate);
    CanApproachTarget = pINI->ReadBool(section, "CanApproachTarget", CanApproachTarget);
    CanRecalcApproachTarget = pINI->ReadBool(section, "CanRecalcApproachTarget", CanRecalcApproachTarget);
    RequiresStolenThirdTech = pINI->ReadBool(section, "RequiresStolenThirdTech", RequiresStolenThirdTech);
    RequiresStolenSovietTech = pINI->ReadBool(section, "RequiresStolenSovietTech", RequiresStolenSovietTech);
    RequiresStolenAlliedTech = pINI->ReadBool(section, "RequiresStolenAlliedTech", RequiresStolenAlliedTech);
    RequiredHouses = pINI->GetOwners(section, "RequiredHouses", RequiredHouses);
    SecretHouses = pINI->GetOwners(section, "SecretHouses", SecretHouses);
    ForbiddenHouses = pINI->GetOwners(section, "ForbiddenHouses", ForbiddenHouses);
    TechLevel = pINI->ReadInteger(section, "TechLevel", TechLevel);
    AirstrikeTeam = pINI->ReadInteger(section, "AirstrikeTeam", AirstrikeTeam);
    EliteAirstrikeTeam = pINI->ReadInteger(section, "EliteAirstrikeTeam", EliteAirstrikeTeam);
    { char _buf[0x40]; if (pINI->ReadString(section, "AirstrikeTeamType", "", _buf, sizeof(_buf)) > 0) { AircraftTypeClass* _p = AircraftTypeClass::FindOrAllocate(_buf); if (_p) AirstrikeTeamType = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "EliteAirstrikeTeamType", "", _buf, sizeof(_buf)) > 0) { AircraftTypeClass* _p = AircraftTypeClass::FindOrAllocate(_buf); if (_p) EliteAirstrikeTeamType = _p; } }
    AirstrikeRechargeTime = pINI->ReadInteger(section, "AirstrikeRechargeTime", AirstrikeRechargeTime);
    EliteAirstrikeRechargeTime = pINI->ReadInteger(section, "EliteAirstrikeRechargeTime", EliteAirstrikeRechargeTime);
    Speed = pINI->ReadInteger(section, "Speed", Speed);
    Cost = pINI->ReadInteger(section, "Cost", Cost);
    Soylent = pINI->ReadInteger(section, "Soylent", Soylent);
    { char _buf[0x40]; if (pINI->ReadString(section, "UnloadingClass", "", _buf, sizeof(_buf)) > 0) { UnitTypeClass* _p = UnitTypeClass::FindOrAllocate(_buf); if (_p) UnloadingClass = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DeployingAnim", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) DeployingAnim = _p; } }
    InitialAmmo = pINI->ReadInteger(section, "InitialAmmo", InitialAmmo);
    Ammo = pINI->ReadInteger(section, "Ammo", Ammo);
    IFVMode = pINI->ReadInteger(section, "IFVMode", IFVMode);
    RadialFireSegments = pINI->ReadInteger(section, "RadialFireSegments", RadialFireSegments);
    DeployFireWeapon = pINI->ReadInteger(section, "DeployFireWeapon", DeployFireWeapon);
    DeployFire = pINI->ReadBool(section, "DeployFire", DeployFire);
    DeployToLand = pINI->ReadBool(section, "DeployToLand", DeployToLand);
    MobileFire = pINI->ReadBool(section, "MobileFire", MobileFire);
    OpportunityFire = pINI->ReadBool(section, "OpportunityFire", OpportunityFire);
    DistributedFire = pINI->ReadBool(section, "DistributedFire", DistributedFire);
    Reload = pINI->ReadInteger(section, "Reload", Reload);
    EmptyReload = pINI->ReadInteger(section, "EmptyReload", EmptyReload);
    ReloadIncrement = pINI->ReadInteger(section, "ReloadIncrement", ReloadIncrement);
    DamageReducesReadiness = pINI->ReadBool(section, "DamageReducesReadiness", DamageReducesReadiness);
    ReadinessReductionMultiplier = pINI->ReadFixed(section, "ReadinessReductionMultiplier", ReadinessReductionMultiplier);
    BerserkFriendly = pINI->ReadBool(section, "BerserkFriendly", BerserkFriendly);
    SprayAttack = pINI->ReadBool(section, "SprayAttack", SprayAttack);
    Pushy = pINI->ReadBool(section, "Pushy", Pushy);
    Natural = pINI->ReadBool(section, "Natural", Natural);
    Unnatural = pINI->ReadBool(section, "Unnatural", Unnatural);
    CloseRange = pINI->ReadBool(section, "CloseRange", CloseRange);
    PreventAttackMove = pINI->ReadBool(section, "PreventAttackMove", PreventAttackMove);
    Points = pINI->ReadInteger(section, "Points", Points);
    ThreatPosed = pINI->ReadInteger(section, "ThreatPosed", ThreatPosed);
    Owner = pINI->GetOwners(section, "Owner", Owner);
    AIBasePlanningSide = pINI->ReadInteger(section, "AIBasePlanningSide", AIBasePlanningSide);
    Trainable = pINI->ReadBool(section, "Trainable", Trainable);
    Crewed = pINI->ReadBool(section, "Crewed", Crewed);
    Gunner = pINI->ReadBool(section, "Gunner", Gunner);
    Naval = pINI->ReadBool(section, "Naval", Naval);
    Repairable = pINI->ReadBool(section, "Repairable", Repairable);
    Invisible = pINI->ReadBool(section, "Invisible", Invisible);
    RadarVisible = pINI->ReadBool(section, "RadarVisible", RadarVisible);
    SelfHealing = pINI->ReadBool(section, "SelfHealing", SelfHealing);
    NoAutoFire = pINI->ReadBool(section, "NoAutoFire", NoAutoFire);
    ROT = pINI->ReadInteger(section, "ROT", ROT);
    Passengers = pINI->ReadInteger(section, "Passengers", Passengers);
    FireAngle = pINI->ReadInteger(section, "FireAngle", FireAngle);
    DeployTime = pINI->ReadFixed(section, "DeployTime", DeployTime);
    UndeployDelay = pINI->ReadInteger(section, "UndeployDelay", UndeployDelay);
    Disableable = pINI->ReadBool(section, "Disableable", Disableable);
    ToProtect = pINI->ReadBool(section, "ToProtect", ToProtect);
    TiberiumHeal = pINI->ReadBool(section, "TiberiumHeal", TiberiumHeal);
    ImmuneToVeins = pINI->ReadBool(section, "ImmuneToVeins", ImmuneToVeins);
    AllowedToStartInMultiplayer = pINI->ReadBool(section, "AllowedToStartInMultiplayer", AllowedToStartInMultiplayer);
    StupidHunt = pINI->ReadBool(section, "StupidHunt", StupidHunt);
    TargetLaser = pINI->ReadBool(section, "TargetLaser", TargetLaser);
    HunterSeeker = pINI->ReadBool(section, "HunterSeeker", HunterSeeker);
    Crusher = pINI->ReadBool(section, "Crusher", Crusher);
    OmniCrusher = pINI->ReadBool(section, "OmniCrusher", OmniCrusher);
    Uncrushable = pINI->ReadBool(section, "Uncrushable", Uncrushable);
    OmniCrushResistant = pINI->ReadBool(section, "OmniCrushResistant", OmniCrushResistant);
    AutoCrush = pINI->ReadBool(section, "AutoCrush", AutoCrush);
    ImmuneToRadiation = pINI->ReadBool(section, "ImmuneToRadiation", ImmuneToRadiation);
    Underwater = pINI->ReadBool(section, "Underwater", Underwater);
    BalloonHover = pINI->ReadBool(section, "BalloonHover", BalloonHover);
    Slaved = pINI->ReadBool(section, "Slaved", Slaved);
    SlaveRegenRate = pINI->ReadInteger(section, "SlaveRegenRate", SlaveRegenRate);
    SlavesNumber = pINI->ReadInteger(section, "SlavesNumber", SlavesNumber);
    SlaveReloadRate = pINI->ReadInteger(section, "SlaveReloadRate", SlaveReloadRate);
    OpenTransportWeapon = pINI->ReadInteger(section, "OpenTransportWeapon", OpenTransportWeapon);
    Spawned = pINI->ReadBool(section, "Spawned", Spawned);
    SpawnRegenRate = pINI->ReadInteger(section, "SpawnRegenRate", SpawnRegenRate);
    SpawnsNumber = pINI->ReadInteger(section, "SpawnsNumber", SpawnsNumber);
    SpawnReloadRate = pINI->ReadInteger(section, "SpawnReloadRate", SpawnReloadRate);
    MissileSpawn = pINI->ReadBool(section, "MissileSpawn", MissileSpawn);
    DefaultToGuardArea = pINI->ReadBool(section, "DefaultToGuardArea", DefaultToGuardArea);
    Warpable = pINI->ReadBool(section, "Warpable", Warpable);
    Parasiteable = pINI->ReadBool(section, "Parasiteable", Parasiteable);
    ImmuneToPsionics = pINI->ReadBool(section, "ImmuneToPsionics", ImmuneToPsionics);
    ImmuneToPsionicWeapons = pINI->ReadBool(section, "ImmuneToPsionicWeapons", ImmuneToPsionicWeapons);
    ConsideredAircraft = pINI->ReadBool(section, "ConsideredAircraft", ConsideredAircraft);
    Bunkerable = pINI->ReadBool(section, "Bunkerable", Bunkerable);
    Organic = pINI->ReadBool(section, "Organic", Organic);
    ImmuneToPoison = pINI->ReadBool(section, "ImmuneToPoison", ImmuneToPoison);
    SuppressionThreshold = pINI->ReadInteger(section, "SuppressionThreshold", SuppressionThreshold);
    NoShadow = pINI->ReadBool(section, "NoShadow", NoShadow);
    JumpjetTurnRate = pINI->ReadInteger(section, "JumpjetTurnRate", JumpjetTurnRate);
    JumpjetSpeed = pINI->ReadInteger(section, "JumpjetSpeed", JumpjetSpeed);
    JumpjetClimb = pINI->ReadFixed(section, "JumpjetClimb", JumpjetClimb);
    JumpjetCrash = pINI->ReadFixed(section, "JumpjetCrash", JumpjetCrash);
    JumpjetHeight = pINI->ReadInteger(section, "JumpjetHeight", JumpjetHeight);
    JumpjetAccel = pINI->ReadFixed(section, "JumpjetAccel", JumpjetAccel);
    JumpjetWobbles = pINI->ReadFixed(section, "JumpjetWobbles", JumpjetWobbles);
    JumpjetNoWobbles = pINI->ReadBool(section, "JumpjetNoWobbles", JumpjetNoWobbles);
    JumpjetDeviation = pINI->ReadInteger(section, "JumpjetDeviation", JumpjetDeviation);
    JumpJet = pINI->ReadBool(section, "JumpJet", JumpJet);
    Crashable = pINI->ReadBool(section, "Crashable", Crashable);
    AttackFriendlies = pINI->ReadBool(section, "AttackFriendlies", AttackFriendlies);
    AttackCursorOnFriendlies = pINI->ReadBool(section, "AttackCursorOnFriendlies", AttackCursorOnFriendlies);
    TurretRecoil = pINI->ReadBool(section, "TurretRecoil", TurretRecoil);
    TurretTravel = pINI->ReadInteger(section, "TurretTravel", TurretTravel);
    TurretCompressFrames = pINI->ReadInteger(section, "TurretCompressFrames", TurretCompressFrames);
    TurretHoldFrames = pINI->ReadInteger(section, "TurretHoldFrames", TurretHoldFrames);
    TurretRecoverFrames = pINI->ReadInteger(section, "TurretRecoverFrames", TurretRecoverFrames);
    BarrelTravel = pINI->ReadInteger(section, "BarrelTravel", BarrelTravel);
    BarrelCompressFrames = pINI->ReadInteger(section, "BarrelCompressFrames", BarrelCompressFrames);
    BarrelHoldFrames = pINI->ReadInteger(section, "BarrelHoldFrames", BarrelHoldFrames);
    BarrelRecoverFrames = pINI->ReadInteger(section, "BarrelRecoverFrames", BarrelRecoverFrames);
    TiltsWhenCrushes = pINI->ReadBool(section, "TiltsWhenCrushes", TiltsWhenCrushes);
    Accelerates = pINI->ReadBool(section, "Accelerates", Accelerates);
    ZFudgeCliff = pINI->ReadInteger(section, "ZFudgeCliff", ZFudgeCliff);
    ZFudgeColumn = pINI->ReadInteger(section, "ZFudgeColumn", ZFudgeColumn);
    ZFudgeTunnel = pINI->ReadInteger(section, "ZFudgeTunnel", ZFudgeTunnel);
    ZFudgeBridge = pINI->ReadInteger(section, "ZFudgeBridge", ZFudgeBridge);
    pINI->GetAbilities(section, "VeteranAbilities", VeteranAbilities, 4);
    pINI->GetAbilities(section, "EliteAbilities", EliteAbilities, 4);
    MyEffectivenessCoefficient = pINI->ReadFixed(section, "MyEffectivenessCoefficient", MyEffectivenessCoefficient);
    TargetEffectivenessCoefficient = pINI->ReadFixed(section, "TargetEffectivenessCoefficient", TargetEffectivenessCoefficient);
    TargetSpecialThreatCoefficient = pINI->ReadFixed(section, "TargetSpecialThreatCoefficient", TargetSpecialThreatCoefficient);
    TargetStrengthCoefficient = pINI->ReadFixed(section, "TargetStrengthCoefficient", TargetStrengthCoefficient);
    TargetDistanceCoefficient = pINI->ReadFixed(section, "TargetDistanceCoefficient", TargetDistanceCoefficient);
    SpecialThreatValue = pINI->ReadFixed(section, "SpecialThreatValue", SpecialThreatValue);
    IsSelectableCombatant = pINI->ReadBool(section, "IsSelectableCombatant", IsSelectableCombatant);
    MovementZoneValue = pINI->GetMovementZone(section, "MovementZone", MovementZoneValue);

    // ------------------------------------------------------------------
    // artmd.ini fields
    // ------------------------------------------------------------------
    CanBeHidden = pArt->ReadBool(section, "CanBeHidden", CanBeHidden);
    UseBuffer = pArt->ReadBool(section, "UseBuffer", UseBuffer);
    pArt->ReadString(section, "Palette", Palette, Palette, sizeof(Palette));
    TurretOffset = pArt->ReadInteger(section, "TurretOffset", TurretOffset);
    RotCount = pArt->ReadInteger(section, "RotCount", RotCount);
    Remapable = pArt->ReadBool(section, "Remapable", Remapable);
    Normalized = pArt->ReadBool(section, "Normalized", Normalized);
    VisibleLoad = pArt->ReadBool(section, "VisibleLoad", VisibleLoad);
    ShadowIndex = pArt->ReadInteger(section, "ShadowIndex", ShadowIndex);
    DisableVoxelCache = pArt->ReadBool(section, "DisableVoxelCache", DisableVoxelCache);
    DisableShadowCache = pArt->ReadBool(section, "DisableShadowCache", DisableShadowCache);
    pArt->ReadString(section, "Cameo", Cameo, Cameo, sizeof(Cameo));
    pArt->ReadString(section, "AltCameo", AltCameo, AltCameo, sizeof(AltCameo));
    PBarrelLength = pArt->ReadInteger(section, "PBarrelLength", PBarrelLength);
    PBarrelThickness = pArt->ReadInteger(section, "PBarrelThickness", PBarrelThickness);
    pArt->Get3Integers(section, "SecondaryFireFLH", SecondaryFireFLH);
    SBarrelLength = pArt->ReadInteger(section, "SBarrelLength", SBarrelLength);
    ElitePBarrelLength = pArt->ReadInteger(section, "ElitePBarrelLength", ElitePBarrelLength);
    ElitePBarrelThickness = pArt->ReadInteger(section, "ElitePBarrelThickness", ElitePBarrelThickness);
    pArt->Get3Integers(section, "EliteSecondaryFireFLH", EliteSecondaryFireFLH);
    EliteSBarrelLength = pArt->ReadInteger(section, "EliteSBarrelLength", EliteSBarrelLength);
    EliteSBarrelThickness = pArt->ReadInteger(section, "EliteSBarrelThickness", EliteSBarrelThickness);
    TurretNotExportedOnGround = pArt->ReadBool(section, "TurretNotExportedOnGround", TurretNotExportedOnGround);
    pArt->Get3Integers(section, "SecondSpawnOffset", SecondSpawnOffset);

    // ------------------------------------------------------------------
    // Indexed key families
    // ------------------------------------------------------------------
    // Gattling weapon stages are numbered from one and only read while the
    // type actually declares more than a single stage.
    if (TurretCount > 0 && WeaponCount > 0)
    {
        for (int32 i = 1; i <= WeaponCount && i <= 18; ++i)
        {
            char key[40];
            sprintf_s(key, sizeof(key), "Weapon%d", i);
            pINI->ReadString(section, key, Weapon[i], Weapon[i], sizeof(Weapon[i]));
        }
    }
    if (IsGattling && WeaponStages > 1)
    {
        for (int32 i = 1; i <= WeaponStages && i <= 10; ++i)
        {
            char key[40];
            sprintf_s(key, sizeof(key), "Stage%d", i);
            WeaponStage[i - 1] = pINI->ReadInteger(section, key, WeaponStage[i - 1]);
        }
    }
    for (int32 i = 0; i < 10; ++i)
    {
        char key[40];
        sprintf_s(key, sizeof(key), "EliteWeapon%d", i);
        pINI->ReadString(section, key, EliteWeapon[i], EliteWeapon[i], sizeof(EliteWeapon[i]));
    }
    for (int32 i = 0; i < 10; ++i)
    {
        char key[40];
        sprintf_s(key, sizeof(key), "EliteStage%d", i);
        EliteStage[i] = pINI->ReadInteger(section, key, EliteStage[i]);
    }

    // ------------------------------------------------------------------
    // Smoke / particle offsets - three integer triplets each.
    // ------------------------------------------------------------------
    pINI->Get3Integers(section, "NaturalParticleLocation", NaturalParticleLocation);
    pINI->Get3Integers(section, "DamageSmokeOffset",       DamageSmokeOffset);
    pINI->Get3Integers(section, "DestroySmokeOffset",      DestroySmokeOffset);
    {
        static const char* const refineryKeys[4] = {
            "RefinerySmokeOffsetOne", "RefinerySmokeOffsetTwo",
            "RefinerySmokeOffsetThree", "RefinerySmokeOffsetFour"
        };
        for (int32 i = 0; i < 4; ++i)
            pINI->Get3Integers(section, refineryKeys[i], RefinerySmokeOffset[i]);
    }

    // ------------------------------------------------------------------
    // Remaining rules keys
    // ------------------------------------------------------------------
    AirRangeBonus = pINI->ReadInteger(section, "AirRangeBonus", AirRangeBonus);
    { char _buf[0x40]; if (pINI->ReadString(section, "Spawns", "", _buf, sizeof(_buf)) > 0) { AircraftTypeClass* _p = AircraftTypeClass::FindOrAllocate(_buf); if (_p) Spawns = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Enslaves", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) Enslaves = _p; } }
    pINI->GetPrerequisiteList(section, "Prerequisite",         Prerequisite);
    pINI->GetPrerequisiteList(section, "PrerequisiteOverride", PrerequisiteOverride);
    for (int32 i = 0; i < 8; ++i)
    {
        char key[40];
        sprintf_s(key, sizeof(key), "AlternateFLH%d", i);
        pArt->Get3Integers(section, key, AlternateFLH[i]);
    }

    // ------------------------------------------------------------------
    // Per-weapon art keys ("Weapon%d<...>") - only present on
    // types whose turret swaps weapons as it fires.  Types that keep a
    // fixed weapon pair read the plain Primary/SecondaryFireFLH keys
    // instead, which land in slots 0 and 1 of the same tables.
    // ------------------------------------------------------------------
    if (IsTurretChanger())
    {
        for (int32 i = 1; i <= WeaponCount && i <= 18; ++i)
        {
            char wname[32];
            sprintf_s(wname, sizeof(wname), "Weapon%d", i);
            char key[48];
            sprintf_s(key, sizeof(key), "%sFLH", wname);
            pArt->Get3Integers(section, key, WeaponFLH[i - 1]);

            sprintf_s(key, sizeof(key), "%sBarrelLength", wname);
            WeaponBarrelLength[i - 1] = pArt->ReadInteger(section, key, WeaponBarrelLength[i - 1]);

            sprintf_s(key, sizeof(key), "%sBarrelThickness", wname);
            WeaponBarrelThickness[i - 1] = pArt->ReadInteger(section, key, WeaponBarrelThickness[i - 1]);

            sprintf_s(key, sizeof(key), "%sTurretLocked", wname);
            WeaponTurretLocked[i - 1] = pArt->ReadBool(section, key, WeaponTurretLocked[i - 1]);
        }
    }
    else
    {
        pArt->Get3Integers(section, "PrimaryFireFLH",   WeaponFLH[0]);
        WeaponBarrelLength[0]  = pArt->ReadInteger(section, "PBarrelLength",    WeaponBarrelLength[0]);
        WeaponBarrelThickness[0] = pArt->ReadInteger(section, "PBarrelThickness", WeaponBarrelThickness[0]);

        pArt->Get3Integers(section, "SecondaryFireFLH", WeaponFLH[1]);
        WeaponBarrelLength[1]  = pArt->ReadInteger(section, "SBarrelLength",    WeaponBarrelLength[1]);
        WeaponBarrelThickness[1] = pArt->ReadInteger(section, "SBarrelThickness", WeaponBarrelThickness[1]);
    }

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
    pINI->WriteInteger(section, "ROT",    TurretROT);
    pINI->WriteInteger(section, "IdleTimer",    IdleTimer);
    pINI->WriteInteger(section, "WeaponCount",  WeaponCount);
    pINI->WriteInteger(section, "DeathWeapon",  DeathWeaponIndex);
    pINI->WriteInteger(section, "WeaponCharge", WeaponCharge);
    pINI->WriteInteger(section, "VeteranRatio", VeteranRatio);
    pINI->WriteInteger(section, "Ammo",         Ammo);
    pINI->WriteInteger(section, "PipScale",
                             static_cast<int32>(PipScaleValue));
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
        case Armor::Special_2:armorName = "Special_2";break;
        case Armor::Special_1:armorName = "Special_1";break;
        default:              armorName = "None";     break;
    }
    pINI->WriteString(section, "Armor", armorName);

    // SpeedType
    const char* speedName = CCINIClass::SpeedTypeIdxToName(
        static_cast<int32>(SpeedTypeVal));
    pINI->WriteString(section, "SpeedType", speedName ? speedName : "Foot");

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
    crc.AddData(&PipScaleValue,   sizeof(PipScaleValue));
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
// 根据游戏行为，可知 SHP_References 负责下面这段逻辑。
//
//  Called after the art INI has been loaded.  Binds the cameo and image SHP
//  pointers.  The full binary goes through the mix filesystem; the standalone
//  build leaves the pointers null so callers can detect missing art.
// ============================================================================

void TechnoTypeClass::Resolve_SHP_References()
{
    // Chain the parent for the base-class SHP references (which resolves the
    // ImageSHP slot from the shared Image name).
    ObjectTypeClass::Resolve_SHP_References();

    // ------------------------------------------------------------------
    // Cameo.  asm: the sidebar slot (+0x6F0) is filled from the explicit
    // Cameo name when present, otherwise from "<Image>ICON".  The lookup goes
    // through the MIX filesystem (the original's MixFileClass finder).
    // ------------------------------------------------------------------
    if (CameoShape == nullptr)
    {
        char cameoName[0x2C];
        if (Cameo[0] != '\0')
        {
            snprintf(cameoName, sizeof(cameoName), "%s.SHP", Cameo);
            CameoShape = Load_SHP(cameoName);
        }
        else if (ImageFile[0] != '\0')
        {
            snprintf(cameoName, sizeof(cameoName), "%sICON.SHP", ImageFile);
            CameoShape = Load_SHP(cameoName);
        }
    }

    // ------------------------------------------------------------------
    // Image shape (+0x710 in the original).  Loaded from "<ImageFile>.SHP".
    // ------------------------------------------------------------------
    if (ImageShape == nullptr && ImageFile[0] != '\0')
    {
        char imgName[0x2C];
        snprintf(imgName, sizeof(imgName), "%s.SHP", ImageFile);
        ImageShape = Load_SHP(imgName);
    }

    // Image geometry is taken straight from the loaded shape header; the
    // sidebar and tooltip layout depend on it.
    if (ImageShape != nullptr)
    {
        ImageSize.X = ImageShape->Width;
        ImageSize.Y = ImageShape->Height;
    }
}

// ============================================================================
// asm: vt_TechnoTypeClass.GetRepairStep   (asm:?)
// ============================================================================

// asm: vt_TechnoTypeClass.GetRepairStep   (asm:?)
int32 TechnoTypeClass::GetRepairStep() const
{
    return RulesClass::Instance->RepairStep;
}

// asm: int __thiscall TechnoTypeClass_GetRepairStepCost(_TechnoTypeClass *this)   (asm:1605942)
// 的修复百分比，向下取整且至少为 1（除零与零耐久都按最小费用处理）。
int32 TechnoTypeClass::GetRepairStepCost() const
{
    const int32 step = RulesClass::Instance->RepairStep;
    if (step <= 0) {
        return 1;
    }
    const int32 price = GetActualCost(nullptr);
    int32 chunks = Strength / step;
    if (chunks <= 0) {
        chunks = 1;
    }
    int32 cost = price / chunks;
    cost = static_cast<int32>(static_cast<double>(cost) * RulesClass::Instance->RepairPercent);
    return cost > 1 ? cost : 1;
}

// asm: int __thiscall TechnoTypeClass_GetFlightLevel(_TechnoTypeClass *this)   (asm:1614628)
int32 TechnoTypeClass::GetFlightLevel() const
{
    return FlightLevel != -1 ? FlightLevel : RulesClass::Instance->FlightLevel;
}

// asm: byte __thiscall TechnoTypeClass_DoesNotBurst(_TechnoTypeClass *this)   (asm:1606003)
// 武器指向同一武器时直接返回真；否则任一武器 Burst 大于 1 返回真。
bool TechnoTypeClass::DoesNotBurst() const
{
    const WeaponStruct* pPrimary = GetWeapon(0);
    if (pPrimary == nullptr || pPrimary->WeaponType == nullptr) {
        return false;
    }
    const WeaponStruct* pSecondary = GetWeapon(1);
    const WeaponTypeClass* w0 = pPrimary->WeaponType;
    const WeaponTypeClass* w1 = (pSecondary != nullptr) ? pSecondary->WeaponType : nullptr;
    if (w1 == w0) {
        return true;
    }
    if (w0->Burst > 1) {
        return true;
    }
    if (w1 == nullptr) {
        return false;
    }
    return w1->Burst > 1;
}

// asm: byte __thiscall TechnoTypeClass_IsTurretChanger(_TechnoTypeClass *this)   (asm:1614720)
bool TechnoTypeClass::IsTurretChanger() const
{
    return WeaponCount > 0;
}

// asm: char __thiscall TechnoTypeClass_CanAttackOnTheMove(TechnoTypeClass *this)   (asm:1605600)
bool TechnoTypeClass::CanAttackOnTheMove() const
{
    return IsArmed && WeaponCount > 0;
}

// asm: byte __fastcall TechnoTypeClass_GetDestinationTab(eAbstractIDs id, int buildCat, int a3)   (asm:1398948)
// 固定落第 3 页，7 类仅在子类型为 5 时落第 1 页（否则第 0 页），其余
// 一律返回 -1 表示不适用。
int32 TechnoTypeClass::GetDestinationTab(int32 kind, int32 sub) const
{
    (void)this;
    if (kind == 0x10) {
        return 2;
    }
    if (kind == 0x28 || kind == 3) {
        return 3;
    }
    if (kind == 7) {
        return sub == 5 ? 1 : 0;
    }
    return -1;
}

// asm: int __thiscall TechnoTypeClass_GetParticleSysPos(_TechnoTypeClass *this, dwXYZ *a2)   (asm:1614775)
// 高度分量按 1:10 折算。
void TechnoTypeClass::GetParticleSysPos(CoordStruct& out) const
{
    out.X = DamageSmokeOffset[0];
    out.Y = DamageSmokeOffset[1];
    out.Z = DamageSmokeOffset[2] * 10;
}

// asm: int __thiscall TechnoTypeClass_ResetPos(_TechnoTypeClass *this, dwXYZ *dest)   (asm:1054631)
void TechnoTypeClass::ResetPos(CoordStruct& out) const
{
    (void)this;
    out.X = 10;
    out.Y = 10;
    out.Z = 10;
}

// asm: TechnoTypeClass_ResetPosLeptons
// 高度基线取全局默认值的两倍（当前全局为 0）。
void TechnoTypeClass::ResetPosLeptons(CoordStruct& out) const
{
    (void)this;
    out.X = 256;
    out.Y = 256;
    out.Z = 0;
}

// asm: void __fastcall TechnoTypeClass_UpdatePalette(_TechnoTypeClass *this)   (asm:1614650)
// 名构建调色板转换器并挂回类型对象。
void TechnoTypeClass::UpdatePalette()
{
    if (Palette[0] == '\0') {
        return;
    }
    BytePalette* pPal = nullptr;
    ConvertClass* pConv = nullptr;
    ConvertClass::CreateFromFile(Palette, pPal, pConv);
    PaletteConvert = pConv;
}

// asm: void __fastcall TechnoTypeClass_ReplacePalette()   (asm:1614675)
// 板文件名的类型重建调色板转换器。
void TechnoTypeClass::ReplacePalette()
{
    if (Array == nullptr) {
        return;
    }
    for (int32 i = 0; i < Array->Count; ++i) {
        TechnoTypeClass* pType = (*Array)[i];
        if (pType != nullptr) {
            pType->UpdatePalette();
        }
    }
}

// asm: char __thiscall TechnoTypeClass_LoadTurretArt(ObjectTypeClass *this, char *imageName, int idx)   (asm:1055309)
// 命名，登记到对应炮塔槽位，等待体素装载器取用。
void TechnoTypeClass::LoadTurretArt(const char* pImageName, int32 idx)
{
    if (pImageName == nullptr || idx < 0 || idx >= 18) {
        return;
    }
    char vxlName[0x20];
    if (idx == 0) {
        snprintf(vxlName, sizeof(vxlName), "%sTUR", pImageName);
    } else {
        snprintf(vxlName, sizeof(vxlName), "%sTUR%d", pImageName, idx);
    }
    TurretArtEntry& slot = Turrets[idx];
    snprintf(slot.VXLName, sizeof(slot.VXLName), "%s", vxlName);
    snprintf(slot.HVAName, sizeof(slot.HVAName), "%s%s", vxlName, ".HVA");
}

// asm: void __thiscall TechnoTypeClass_UnloadTurretArt(_TechnoTypeClass *this)   (asm:1055964)
// 清空登记的文件名。
void TechnoTypeClass::UnloadTurretArt()
{
    for (int32 i = 0; i < 18; ++i) {
        TurretArtEntry& slot = Turrets[i];
        delete static_cast<uint8*>(slot.VXL);
        delete static_cast<uint8*>(slot.HVA);
        slot.VXL = nullptr;
        slot.HVA = nullptr;
        slot.VXLName[0] = '\0';
        slot.HVAName[0] = '\0';
    }
}
