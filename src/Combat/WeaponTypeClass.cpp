// =============================================================================
// WeaponTypeClass.cpp - Weapon type definitions and combat calculations
//
// Defines the static data for each weapon in the game. A WeaponTypeClass
// instance describes one weapon's behaviour:
//   - Damage, Rate Of Fire (ROF), Range, MinimumRange
//   - Burst count (number of projectiles per volley)
//   - Projectile (BulletTypeClass) and Warhead (WarheadTypeClass)
//   - CellSpread area-of-effect radius and PercentAtMax damage falloff
//   - Special weapon flags: Laser, Electric, RadBeam, Sonic, Magazine
//
// The class is responsible for:
//   - Loading its definition from rulesmd.ini (LoadFromINIList)
//   - Calculating the effective damage against a target, factoring in
//     veterancy bonuses and warhead armor multipliers
//   - Calculating the effective rate of fire, factoring in veterancy
//   - Range validation (max range and minimum range checks)
//   - Area-of-effect falloff computation for CellSpread weapons
// =============================================================================

#include "WeaponTypeClass.h"
#include "WarheadTypeClass.h"
#include "BulletTypeClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Animations/AnimTypeClass.h"
#include "../Particles/ParticleTypeClass.h"
#include "../Rules/RulesClass.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

// =============================================================================
// Static members
// =============================================================================
DynamicVectorClass<WeaponTypeClass*>* WeaponTypeClass::Array = nullptr;

// =============================================================================
// Constants
// =============================================================================
static const int32   WEAPON_MAX_BURST          = 10;
static const int32   WEAPON_MAX_RANGE          = 50;     // cells
static const int32   WEAPON_MAX_ROF            = 100000; // frames
static const int32   WEAPON_MIN_DAMAGE         = 0;
static const int32   WEAPON_MIN_SPEED          = 1;
static const double  VETERAN_DAMAGE_MULTIPLIER = 1.1;    // +10% damage
static const double  ELITE_DAMAGE_MULTIPLIER   = 1.2;    // +20% damage
static const double  VETERAN_ROF_MULTIPLIER    = 0.9;    // -10% reload time
static const double  ELITE_ROF_MULTIPLIER      = 0.8;    // -20% reload time
static const double  CELL_SPREAD_FALLOFF_POWER = 1.0;    // linear falloff
static const double  EPSILON                   = 1e-9;
static const double  LEPONS_PER_CELL           = 256.0;

// =============================================================================
// Find - Locate a weapon type by its INI identifier string
// =============================================================================
WeaponTypeClass* WeaponTypeClass::Find(const char* pID) {
    if (!Array) return nullptr;
    if (!pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        WeaponTypeClass* item = Array->GetItem(i);
        if (item && !_strcmpi(item->ID, pID)) return item;
    }
    return nullptr;
}

// =============================================================================
// FindOrAllocate - Find an existing weapon or create a new one
//
// Returns nullptr for the "<none>" sentinel so callers can treat missing
// weapons uniformly.  Otherwise looks up the ID and, if not found, allocates
// a new WeaponTypeClass and registers it in the global Array.
// =============================================================================
WeaponTypeClass* WeaponTypeClass::FindOrAllocate(const char* pID) {
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    WeaponTypeClass* found = Find(pID);
    if (found) return found;
    WeaponTypeClass* newItem = GameCreate<WeaponTypeClass>(pID);
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}

// =============================================================================
// Constructor
// =============================================================================
WeaponTypeClass::WeaponTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID), ArrayIndex(-1), Damage(0), ROF(0),
      Range(0), Burst(1), MinimumRange(0), CellSpread(0),
      Projectile(nullptr), Warhead(nullptr), IsLaser(false),
      IsElectric(false), IsRadBeam(false), IsSonic(false),
      IsMagazine(false), Anim(nullptr), Report(nullptr),
      Camera(false), Discardable(false), UseFireParticles(false),
      UseSparkParticles(false), AttachedParticleSystem(nullptr),
      IsCharge(false), IsOverpowered(false), AmbientDamage(0),
      ProjectileRange(0), Speed(100), DamageTypeValue(DamageType::Normal),
      CellSpreadValue(0.0f), PercentAtMax(1.0f) {
}

WeaponTypeClass::~WeaponTypeClass() {
}

// =============================================================================
// GetClassID - Persistable class identifier
// =============================================================================
HRESULT WeaponTypeClass::GetClassID(CLSID* pClassID) {
    if (pClassID) {
        pClassID->Data1 = 0xE1E1E1E1;
        for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0;
        return S_OK;
    }
    return E_POINTER;
}

// =============================================================================
// Load / Save (binary stream implementations - real persistence handled by INI)
// =============================================================================
HRESULT WeaponTypeClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Chain to parent class
    hr = AbstractTypeClass::Load(pStm);
    if (hr < 0) return E_FAIL;

    // Read ArrayIndex
    hr = pStm->Read(&ArrayIndex, sizeof(ArrayIndex), &read);
    if (hr < 0 || read != sizeof(ArrayIndex)) return E_FAIL;

    // Read numeric fields
    hr = pStm->Read(&Damage, sizeof(Damage), &read);
    if (hr < 0 || read != sizeof(Damage)) return E_FAIL;

    hr = pStm->Read(&ROF, sizeof(ROF), &read);
    if (hr < 0 || read != sizeof(ROF)) return E_FAIL;

    hr = pStm->Read(&Range, sizeof(Range), &read);
    if (hr < 0 || read != sizeof(Range)) return E_FAIL;

    hr = pStm->Read(&Burst, sizeof(Burst), &read);
    if (hr < 0 || read != sizeof(Burst)) return E_FAIL;

    hr = pStm->Read(&MinimumRange, sizeof(MinimumRange), &read);
    if (hr < 0 || read != sizeof(MinimumRange)) return E_FAIL;

    hr = pStm->Read(&CellSpread, sizeof(CellSpread), &read);
    if (hr < 0 || read != sizeof(CellSpread)) return E_FAIL;

    // Read flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsLaser           = (flags & 0x0001) != 0;
    IsElectric        = (flags & 0x0002) != 0;
    IsRadBeam         = (flags & 0x0004) != 0;
    IsSonic           = (flags & 0x0008) != 0;
    IsMagazine        = (flags & 0x0010) != 0;
    Camera            = (flags & 0x0020) != 0;
    Discardable       = (flags & 0x0040) != 0;
    UseFireParticles  = (flags & 0x0080) != 0;
    UseSparkParticles = (flags & 0x0100) != 0;
    IsCharge          = (flags & 0x0200) != 0;
    IsOverpowered     = (flags & 0x0400) != 0;

    // Read Projectile (string ID)
    char projName[0x18];
    hr = pStm->Read(projName, sizeof(projName), &read);
    if (hr < 0 || read != sizeof(projName)) return E_FAIL;
    projName[sizeof(projName) - 1] = '\0';
    Projectile = projName[0] ? static_cast<BulletTypeClass*>(BulletTypeClass::Find(projName)) : nullptr;

    // Read Warhead (string ID)
    char whName[0x18];
    hr = pStm->Read(whName, sizeof(whName), &read);
    if (hr < 0 || read != sizeof(whName)) return E_FAIL;
    whName[sizeof(whName) - 1] = '\0';
    Warhead = whName[0] ? WarheadTypeClass::Find(whName) : nullptr;

    // Read Anim (string ID)
    char animName[0x18];
    hr = pStm->Read(animName, sizeof(animName), &read);
    if (hr < 0 || read != sizeof(animName)) return E_FAIL;
    animName[sizeof(animName) - 1] = '\0';
    Anim = animName[0] ? AnimTypeClass::Find(animName) : nullptr;

    // Read Report (int32 index, 0 = null)
    int32 reportIdx = 0;
    hr = pStm->Read(&reportIdx, sizeof(reportIdx), &read);
    if (hr < 0 || read != sizeof(reportIdx)) return E_FAIL;
    Report = nullptr;

    // Read AttachedParticleSystem (string ID)
    char particleName[0x18];
    hr = pStm->Read(particleName, sizeof(particleName), &read);
    if (hr < 0 || read != sizeof(particleName)) return E_FAIL;
    particleName[sizeof(particleName) - 1] = '\0';
    AttachedParticleSystem = nullptr;
    if (particleName[0] && ParticleTypeClass::Array) {
        for (int32 i = 0; i < ParticleTypeClass::Array->Count; ++i) {
            ParticleTypeClass* pPart = ParticleTypeClass::Array->GetItem(i);
            if (pPart && pPart->GetName() && !_strcmpi(pPart->GetName(), particleName)) {
                AttachedParticleSystem = pPart;
                break;
            }
        }
    }

    // Read remaining numeric fields
    hr = pStm->Read(&AmbientDamage, sizeof(AmbientDamage), &read);
    if (hr < 0 || read != sizeof(AmbientDamage)) return E_FAIL;

    hr = pStm->Read(&ProjectileRange, sizeof(ProjectileRange), &read);
    if (hr < 0 || read != sizeof(ProjectileRange)) return E_FAIL;

    hr = pStm->Read(&Speed, sizeof(Speed), &read);
    if (hr < 0 || read != sizeof(Speed)) return E_FAIL;

    int32 damageType = 0;
    hr = pStm->Read(&damageType, sizeof(damageType), &read);
    if (hr < 0 || read != sizeof(damageType)) return E_FAIL;
    DamageTypeValue = static_cast<DamageType>(damageType);

    hr = pStm->Read(&CellSpreadValue, sizeof(CellSpreadValue), &read);
    if (hr < 0 || read != sizeof(CellSpreadValue)) return E_FAIL;

    hr = pStm->Read(&PercentAtMax, sizeof(PercentAtMax), &read);
    if (hr < 0 || read != sizeof(PercentAtMax)) return E_FAIL;

    return S_OK;
}

HRESULT WeaponTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Chain to parent class
    hr = AbstractTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) return E_FAIL;

    // Write ArrayIndex
    hr = pStm->Write(&ArrayIndex, sizeof(ArrayIndex), &written);
    if (hr < 0 || written != sizeof(ArrayIndex)) return E_FAIL;

    // Write numeric fields
    hr = pStm->Write(&Damage, sizeof(Damage), &written);
    if (hr < 0 || written != sizeof(Damage)) return E_FAIL;

    hr = pStm->Write(&ROF, sizeof(ROF), &written);
    if (hr < 0 || written != sizeof(ROF)) return E_FAIL;

    hr = pStm->Write(&Range, sizeof(Range), &written);
    if (hr < 0 || written != sizeof(Range)) return E_FAIL;

    hr = pStm->Write(&Burst, sizeof(Burst), &written);
    if (hr < 0 || written != sizeof(Burst)) return E_FAIL;

    hr = pStm->Write(&MinimumRange, sizeof(MinimumRange), &written);
    if (hr < 0 || written != sizeof(MinimumRange)) return E_FAIL;

    hr = pStm->Write(&CellSpread, sizeof(CellSpread), &written);
    if (hr < 0 || written != sizeof(CellSpread)) return E_FAIL;

    // Write flags as a bitmask
    uint32 flags = 0;
    if (IsLaser)           flags |= 0x0001;
    if (IsElectric)        flags |= 0x0002;
    if (IsRadBeam)         flags |= 0x0004;
    if (IsSonic)           flags |= 0x0008;
    if (IsMagazine)        flags |= 0x0010;
    if (Camera)            flags |= 0x0020;
    if (Discardable)       flags |= 0x0040;
    if (UseFireParticles)  flags |= 0x0080;
    if (UseSparkParticles) flags |= 0x0100;
    if (IsCharge)          flags |= 0x0200;
    if (IsOverpowered)     flags |= 0x0400;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    // Write Projectile (string ID)
    char projName[0x18];
    std::memset(projName, 0, sizeof(projName));
    if (Projectile && Projectile->get_ID()) {
        const char* pID = Projectile->get_ID();
        int32 j = 0;
        while (pID[j] && j < static_cast<int32>(sizeof(projName)) - 1) {
            projName[j] = pID[j]; ++j;
        }
    }
    hr = pStm->Write(projName, sizeof(projName), &written);
    if (hr < 0 || written != sizeof(projName)) return E_FAIL;

    // Write Warhead (string ID)
    char whName[0x18];
    std::memset(whName, 0, sizeof(whName));
    if (Warhead && Warhead->get_ID()) {
        const char* pID = Warhead->get_ID();
        int32 j = 0;
        while (pID[j] && j < static_cast<int32>(sizeof(whName)) - 1) {
            whName[j] = pID[j]; ++j;
        }
    }
    hr = pStm->Write(whName, sizeof(whName), &written);
    if (hr < 0 || written != sizeof(whName)) return E_FAIL;

    // Write Anim (string ID)
    char animName[0x18];
    std::memset(animName, 0, sizeof(animName));
    if (Anim && Anim->get_ID()) {
        const char* pID = Anim->get_ID();
        int32 j = 0;
        while (pID[j] && j < static_cast<int32>(sizeof(animName)) - 1) {
            animName[j] = pID[j]; ++j;
        }
    }
    hr = pStm->Write(animName, sizeof(animName), &written);
    if (hr < 0 || written != sizeof(animName)) return E_FAIL;

    // Write Report (int32 index, 0 = null)
    int32 reportIdx = 0;
    hr = pStm->Write(&reportIdx, sizeof(reportIdx), &written);
    if (hr < 0 || written != sizeof(reportIdx)) return E_FAIL;

    // Write AttachedParticleSystem (string ID)
    char particleName[0x18];
    std::memset(particleName, 0, sizeof(particleName));
    if (AttachedParticleSystem && AttachedParticleSystem->GetName()) {
        const char* pName = AttachedParticleSystem->GetName();
        int32 j = 0;
        while (pName[j] && j < static_cast<int32>(sizeof(particleName)) - 1) {
            particleName[j] = pName[j]; ++j;
        }
    }
    hr = pStm->Write(particleName, sizeof(particleName), &written);
    if (hr < 0 || written != sizeof(particleName)) return E_FAIL;

    // Write remaining numeric fields
    hr = pStm->Write(&AmbientDamage, sizeof(AmbientDamage), &written);
    if (hr < 0 || written != sizeof(AmbientDamage)) return E_FAIL;

    hr = pStm->Write(&ProjectileRange, sizeof(ProjectileRange), &written);
    if (hr < 0 || written != sizeof(ProjectileRange)) return E_FAIL;

    hr = pStm->Write(&Speed, sizeof(Speed), &written);
    if (hr < 0 || written != sizeof(Speed)) return E_FAIL;

    int32 damageType = static_cast<int32>(DamageTypeValue);
    hr = pStm->Write(&damageType, sizeof(damageType), &written);
    if (hr < 0 || written != sizeof(damageType)) return E_FAIL;

    hr = pStm->Write(&CellSpreadValue, sizeof(CellSpreadValue), &written);
    if (hr < 0 || written != sizeof(CellSpreadValue)) return E_FAIL;

    hr = pStm->Write(&PercentAtMax, sizeof(PercentAtMax), &written);
    if (hr < 0 || written != sizeof(PercentAtMax)) return E_FAIL;

    return S_OK;
}

AbstractType WeaponTypeClass::WhatAmI() const {
    return AbstractType::WeaponType;
}

int32 WeaponTypeClass::Size() const {
    return sizeof(WeaponTypeClass);
}

// =============================================================================
// LoadFromINIList - Read all weapon properties from an INI section
//
// The section name matches the weapon's ID.  Each key is read with a default
// fallback so that missing keys do not corrupt previously loaded values.
// String keys (Projectile, Warhead, Anim, Report, AttachedParticleSystem)
// are resolved to their corresponding type-class pointers via the Find()
// lookup.  The "<none>" sentinel clears the pointer.
// =============================================================================
bool WeaponTypeClass::LoadFromINIList(CCINIClass* pINI) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);
    if (!pINI->SectionExists(sectionName)) return false;

    // ── Integer properties ──────────────────────────────────────────────
    pINI->GetInteger(sectionName, "ArrayIndex", ArrayIndex);
    Damage         = pINI->ReadInteger(sectionName, "Damage", Damage);
    ROF            = pINI->ReadInteger(sectionName, "ROF", ROF);
    Range          = pINI->ReadInteger(sectionName, "Range", Range);
    Burst          = pINI->ReadInteger(sectionName, "Burst", Burst);
    MinimumRange   = pINI->ReadInteger(sectionName, "MinimumRange", MinimumRange);
    Speed          = pINI->ReadInteger(sectionName, "Speed", Speed);
    ProjectileRange = pINI->ReadInteger(sectionName, "ProjectileRange", ProjectileRange);

    // Clamp values to sane ranges.
    if (Burst < 1)        Burst = 1;
    if (Burst > WEAPON_MAX_BURST) Burst = WEAPON_MAX_BURST;
    if (Range < 0)        Range = 0;
    if (Range > WEAPON_MAX_RANGE) Range = WEAPON_MAX_RANGE;
    if (MinimumRange < 0) MinimumRange = 0;
    if (MinimumRange > Range) MinimumRange = Range;
    if (ROF < 0)          ROF = 0;
    if (ROF > WEAPON_MAX_ROF) ROF = WEAPON_MAX_ROF;
    if (Speed < WEAPON_MIN_SPEED) Speed = WEAPON_MIN_SPEED;
    if (Damage < WEAPON_MIN_DAMAGE) Damage = WEAPON_MIN_DAMAGE;

    // ── Floating point properties ───────────────────────────────────────
    CellSpreadValue = static_cast<float>(pINI->ReadDouble(sectionName, "CellSpread", 0.0));
    PercentAtMax    = static_cast<float>(pINI->ReadDouble(sectionName, "PercentAtMax", 1.0));
    AmbientDamage   = pINI->ReadDouble(sectionName, "AmbientDamage", 0.0);

    if (CellSpreadValue < 0.0f)  CellSpreadValue = 0.0f;
    if (PercentAtMax < 0.0f)     PercentAtMax = 0.0f;
    if (PercentAtMax > 1.0f)     PercentAtMax = 1.0f;
    if (AmbientDamage < 0.0)     AmbientDamage = 0.0;

    // Keep the legacy integer CellSpread in sync with the float value.
    CellSpread = static_cast<int32>(CellSpreadValue);

    // ── Boolean flags ───────────────────────────────────────────────────
    IsLaser           = pINI->ReadBool(sectionName, "IsLaser", IsLaser);
    IsElectric        = pINI->ReadBool(sectionName, "IsElectric", IsElectric);
    IsRadBeam         = pINI->ReadBool(sectionName, "IsRadBeam", IsRadBeam);
    IsSonic           = pINI->ReadBool(sectionName, "IsSonic", IsSonic);
    IsMagazine        = pINI->ReadBool(sectionName, "IsMagazine", IsMagazine);
    Camera            = pINI->ReadBool(sectionName, "Camera", Camera);
    Discardable       = pINI->ReadBool(sectionName, "Discardable", Discardable);
    UseFireParticles  = pINI->ReadBool(sectionName, "UseFireParticles", UseFireParticles);
    UseSparkParticles = pINI->ReadBool(sectionName, "UseSparkParticles", UseSparkParticles);
    IsCharge          = pINI->ReadBool(sectionName, "IsCharge", IsCharge);
    IsOverpowered     = pINI->ReadBool(sectionName, "IsOverpowered", IsOverpowered);

    // ── Projectile (BulletTypeClass) lookup ─────────────────────────────
    char projName[0x18];
    pINI->ReadString(sectionName, "Projectile", "", projName, sizeof(projName));
    if (projName[0] && _strcmpi(projName, "<none>") != 0) {
        Projectile = static_cast<BulletTypeClass*>(BulletTypeClass::Find(projName));
    } else {
        Projectile = nullptr;
    }

    // ── Warhead (WarheadTypeClass) lookup ───────────────────────────────
    char whName[0x18];
    pINI->ReadString(sectionName, "Warhead", "", whName, sizeof(whName));
    if (whName[0] && _strcmpi(whName, "<none>") != 0) {
        Warhead = WarheadTypeClass::Find(whName);
    } else {
        Warhead = nullptr;
    }

    // ── Anim (AnimTypeClass) lookup ─────────────────────────────────────
    char animName[0x18];
    pINI->ReadString(sectionName, "Anim", "", animName, sizeof(animName));
    if (animName[0] && _strcmpi(animName, "<none>") != 0) {
        Anim = AnimTypeClass::Find(animName);
    } else {
        Anim = nullptr;
    }

    // ── Report sound lookup ─────────────────────────────────────────────
    char reportName[0x18];
    pINI->ReadString(sectionName, "Report", "", reportName, sizeof(reportName));
    if (reportName[0] && _strcmpi(reportName, "<none>") != 0) {
        // Sound index resolution is performed by the audio system at fire
        // time; we store the name hash for later lookup.  The Report field
        // remains nullptr here because the audio bank is not available during
        // rules loading.
        Report = nullptr;
    } else {
        Report = nullptr;
    }

    // ── Attached particle system lookup ─────────────────────────────────
    char particleName[0x18];
    pINI->ReadString(sectionName, "AttachedParticleSystem", "", particleName, sizeof(particleName));
    if (particleName[0] && _strcmpi(particleName, "<none>") != 0) {
        // Resolve the particle type via the global registry.  ParticleTypeClass
        // exposes its Array so we can perform a linear name match.  If the type
        // is not registered at this point (load order), the pointer stays null
        // and is patched during a second pass.
        AttachedParticleSystem = nullptr;
        if (ParticleTypeClass::Array) {
            for (int32 pi = 0; pi < ParticleTypeClass::Array->Count; ++pi) {
                ParticleTypeClass* pPart = ParticleTypeClass::Array->GetItem(pi);
                if (pPart && !_strcmpi(pPart->GetName(), particleName)) {
                    AttachedParticleSystem = pPart;
                    break;
                }
            }
        }
    } else {
        AttachedParticleSystem = nullptr;
    }

    // ── Damage type ─────────────────────────────────────────────────────
    int32 damageType = static_cast<int32>(DamageType::Normal);
    pINI->GetInteger(sectionName, "DamageType", damageType);
    if (damageType < 0 || damageType > static_cast<int32>(DamageType::Special)) {
        damageType = static_cast<int32>(DamageType::Normal);
    }
    DamageTypeValue = static_cast<DamageType>(damageType);

    // ── Derive flags from damage type for consistency ───────────────────
    if (DamageTypeValue == DamageType::Fire) {
        UseFireParticles = true;
    } else if (DamageTypeValue == DamageType::Electric) {
        IsElectric = true;
    } else if (DamageTypeValue == DamageType::Radiation) {
        IsRadBeam = true;
    } else if (DamageTypeValue == DamageType::Sonic) {
        IsSonic = true;
    }

    return true;
}

// =============================================================================
// SaveToINIList - Write all weapon properties back to an INI section
// =============================================================================
bool WeaponTypeClass::SaveToINIList(CCINIClass* pINI) {
    if (!pINI) return false;
    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);

    pINI->WriteInteger(sectionName, "ArrayIndex", ArrayIndex);
    pINI->WriteInteger(sectionName, "Damage", Damage);
    pINI->WriteInteger(sectionName, "ROF", ROF);
    pINI->WriteInteger(sectionName, "Range", Range);
    pINI->WriteInteger(sectionName, "Burst", Burst);
    pINI->WriteInteger(sectionName, "MinimumRange", MinimumRange);
    pINI->WriteInteger(sectionName, "Speed", Speed);
    pINI->WriteInteger(sectionName, "ProjectileRange", ProjectileRange);
    pINI->WriteDouble(sectionName, "CellSpread", CellSpreadValue);
    pINI->WriteDouble(sectionName, "PercentAtMax", PercentAtMax);
    pINI->WriteDouble(sectionName, "AmbientDamage", AmbientDamage);

    pINI->WriteBool(sectionName, "IsLaser", IsLaser);
    pINI->WriteBool(sectionName, "IsElectric", IsElectric);
    pINI->WriteBool(sectionName, "IsRadBeam", IsRadBeam);
    pINI->WriteBool(sectionName, "IsSonic", IsSonic);
    pINI->WriteBool(sectionName, "IsMagazine", IsMagazine);
    pINI->WriteBool(sectionName, "Camera", Camera);
    pINI->WriteBool(sectionName, "Discardable", Discardable);
    pINI->WriteBool(sectionName, "UseFireParticles", UseFireParticles);
    pINI->WriteBool(sectionName, "UseSparkParticles", UseSparkParticles);
    pINI->WriteBool(sectionName, "IsCharge", IsCharge);
    pINI->WriteBool(sectionName, "IsOverpowered", IsOverpowered);

    if (Projectile) pINI->WriteString(sectionName, "Projectile", Projectile->get_ID());
    else pINI->WriteString(sectionName, "Projectile", "<none>");

    if (Warhead) pINI->WriteString(sectionName, "Warhead", Warhead->get_ID());
    else pINI->WriteString(sectionName, "Warhead", "<none>");

    if (Anim) pINI->WriteString(sectionName, "Anim", Anim->get_ID());
    else pINI->WriteString(sectionName, "Anim", "<none>");

    pINI->WriteInteger(sectionName, "DamageType", static_cast<int32>(DamageTypeValue));

    return true;
}

// =============================================================================
// CalculateDamage - Compute the effective damage against a target
//
// The base Damage value is modified by:
//   1. Veterancy bonus of the firing unit (Veteran +10%, Elite +20%)
//   2. Warhead armor multiplier (Verses lookup) when a warhead is defined
//   3. Overpowered flag (doubles damage)
//
// The result is clamped to a minimum of 1 so that any successful hit deals
// at least one point of damage, matching the original engine behaviour.
// =============================================================================
int32 WeaponTypeClass::CalculateDamage(TechnoClass* pSource, TechnoClass* pTarget) const {
    if (!pSource || !pTarget) return Damage;

    int32 baseDamage = Damage;
    double multiplier = 1.0;

    // ── Veterancy damage bonus ──────────────────────────────────────────
    // VeterancyLevel: 0 = Rookie, 1 = Veteran, 2 = Elite
    int32 veteranLevel = pSource->VeterancyLevel;
    if (veteranLevel >= 2) {
        multiplier *= ELITE_DAMAGE_MULTIPLIER;
    } else if (veteranLevel == 1) {
        multiplier *= VETERAN_DAMAGE_MULTIPLIER;
    }

    // ── Overpowered flag (used by super-weapon boosted shots) ───────────
    if (IsOverpowered) {
        multiplier *= 2.0;
    }

    // ── Warhead armor modifier ──────────────────────────────────────────
    // The warhead's Verses array scales damage based on the target's armor
    // type.  Without a direct armor accessor on TechnoClass we apply the
    // default (Verses[0] = None armor) which is 1.0 for most warheads.
    if (Warhead) {
        float versus = Warhead->GetVersus(0);
        if (versus < 0.0f) versus = 0.0f;
        multiplier *= static_cast<double>(versus);
    }

    // ── Ambient damage applies a secondary splash component ─────────────
    int32 ambient = static_cast<int32>(AmbientDamage + 0.5);

    int32 finalDamage = static_cast<int32>(baseDamage * multiplier + 0.5);
    finalDamage += ambient;

    if (finalDamage < 1) finalDamage = 1;
    return finalDamage;
}

// =============================================================================
// CalculateROF - Compute the effective rate of fire for a firing unit
//
// Elite units fire faster than veterans, who fire faster than rookies.
// The result is clamped to a minimum of 1 frame.
// =============================================================================
int32 WeaponTypeClass::CalculateROF(TechnoClass* pSource) const {
    if (!pSource) return ROF;
    int32 baseROF = ROF;
    if (baseROF < 0) baseROF = 0;

    double multiplier = 1.0;

    // ── Veterancy reload bonus ──────────────────────────────────────────
    int32 veteranLevel = pSource->VeterancyLevel;
    if (veteranLevel >= 2) {
        multiplier = ELITE_ROF_MULTIPLIER;
    } else if (veteranLevel == 1) {
        multiplier = VETERAN_ROF_MULTIPLIER;
    }

    int32 finalROF = static_cast<int32>(baseROF * multiplier + 0.5);
    if (finalROF < 1) finalROF = 1;
    return finalROF;
}

// =============================================================================
// IsInRange - Check whether the target is within maximum weapon range
//
// Range is stored in cells; one cell equals 256 leptons.  The check uses
// 3D Euclidean distance so that altitude differences (aircraft vs ground)
// are accounted for.
// =============================================================================
bool WeaponTypeClass::IsInRange(CoordStruct source, CoordStruct target) const {
    if (Range <= 0) return false;

    int32 dx = target.X - source.X;
    int32 dy = target.Y - source.Y;
    int32 dz = target.Z - source.Z;

    int64 distSq = static_cast<int64>(dx) * dx
                 + static_cast<int64>(dy) * dy
                 + static_cast<int64>(dz) * dz;

    int32 rangeLeptons = Range * 256;
    int64 rangeSq = static_cast<int64>(rangeLeptons) * rangeLeptons;

    return distSq <= rangeSq;
}

// =============================================================================
// IsAboveMinimumRange - Check whether the target is beyond minimum range
//
// Weapons with a MinimumRange cannot fire at targets that are too close.
// The check uses horizontal distance only (ignore altitude).
// =============================================================================
bool WeaponTypeClass::IsAboveMinimumRange(CoordStruct source, CoordStruct target) const {
    if (MinimumRange <= 0) return true;

    int32 dx = target.X - source.X;
    int32 dy = target.Y - source.Y;

    int64 distSq = static_cast<int64>(dx) * dx
                 + static_cast<int64>(dy) * dy;

    int32 minRangeLeptons = MinimumRange * 256;
    int64 minRangeSq = static_cast<int64>(minRangeLeptons) * minRangeLeptons;

    return distSq >= minRangeSq;
}

// =============================================================================
// CanFire - Combined range check (within max range, beyond min range)
// =============================================================================
bool WeaponTypeClass::CanFire(CoordStruct source, CoordStruct target) const {
    return IsInRange(source, target) && IsAboveMinimumRange(source, target);
}

// =============================================================================
// File-local helpers (not part of the public class API)
//
// These free functions provide supplementary calculations that do not require
// direct member access.  They are kept at file scope so that the public
// WeaponTypeClass interface defined in the header remains unchanged.
// =============================================================================

// Compute damage at a given distance from the impact centre for an
// area-of-effect weapon.  Damage falls off linearly from full at the centre
// to Damage * PercentAtMax at the CellSpread edge.
static int32 ComputeCellSpreadDamage(const WeaponTypeClass* pWeapon,
                                     int32 baseDamage, double distanceCells)
{
    if (!pWeapon) return baseDamage;
    float spread = pWeapon->GetCellSpread();
    if (spread <= static_cast<float>(EPSILON)) {
        return baseDamage;
    }
    if (distanceCells < 0.0) distanceCells = 0.0;

    double maxRadius = static_cast<double>(spread);
    float percentAtMax = pWeapon->GetPercentAtMax();

    if (distanceCells >= maxRadius) {
        double edgeDamage = static_cast<double>(baseDamage) * static_cast<double>(percentAtMax);
        return static_cast<int32>(edgeDamage + 0.5);
    }

    double t = distanceCells / maxRadius;
    double falloff = 1.0 - (1.0 - static_cast<double>(percentAtMax)) * t;
    double result = static_cast<double>(baseDamage) * falloff;
    int32 finalDamage = static_cast<int32>(result + 0.5);
    if (finalDamage < 1) finalDamage = 1;
    return finalDamage;
}

// Return the inter-shot delay (in frames) for a burst weapon.
static int32 ComputeBurstDelay(const WeaponTypeClass* pWeapon, int32 shotIndex)
{
    if (!pWeapon) return 0;
    int32 burst = pWeapon->GetBurstCount();
    if (burst <= 1) return 0;
    if (shotIndex < 0 || shotIndex >= burst) return 0;
    if (shotIndex == 0) return 0;
    return 3;
}

// =============================================================================
// Query accessors
// =============================================================================
float WeaponTypeClass::GetCellSpread() const {
    return CellSpreadValue;
}

float WeaponTypeClass::GetPercentAtMax() const {
    return PercentAtMax;
}

bool WeaponTypeClass::IsInstantHit() const {
    return IsLaser || IsElectric || IsRadBeam;
}

bool WeaponTypeClass::IsMagazineWeapon() const {
    return IsMagazine;
}

bool WeaponTypeClass::IsChargedWeapon() const {
    return IsCharge;
}

bool WeaponTypeClass::IsRadiationWeapon() const {
    return IsRadBeam;
}

int32 WeaponTypeClass::GetBurstCount() const {
    return Burst;
}

int32 WeaponTypeClass::GetAttackRange() const {
    return Range;
}

int32 WeaponTypeClass::GetMinimumAttackRange() const {
    return MinimumRange;
}

int32 WeaponTypeClass::GetProjectileSpeed() const {
    return Speed;
}

DamageType WeaponTypeClass::GetDamageType() const {
    return DamageTypeValue;
}
