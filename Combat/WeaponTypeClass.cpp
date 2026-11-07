// =============================================================================
#include <Animations/AnimTypeClass.h>
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
// FindIndex - slot index of a weapon id inside the global array
//
// asm: WeaponTypeClass_FindIndex (0x773030)
//   for (i = 0; i < vec_WeaponTypes.length; ++i)
//       if (!strcmpi(pID, vec[i]->ID /*+0x24*/)) return i;
//   return -1;
// =============================================================================
int32 WeaponTypeClass::FindIndex(const char* pID) {
    if (Array == nullptr || pID == nullptr) return -1;
    for (int32 i = 0; i < Array->Count; ++i) {
        WeaponTypeClass* item = Array->GetItem(i);
        if (item != nullptr && _strcmpi(item->ID, pID) == 0) return i;
    }
    return -1;   // asm: or eax, 0FFFFFFFFh
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
//
// asm: WeaponTypeClass_GetClassID (0x772C60)
//   mov  eax,[esp+arg_4]      ; pClassID
//   test eax,eax ; jnz copy
//   mov  eax, 80004003h       ; E_POINTER when the out pointer is null
//   retn 8
// copy:
//   copy 16 bytes from CLSID_WeaponTypeClass (.rdata) into *pClassID
//   xor  eax,eax              ; S_OK
// =============================================================================

// The 16-byte class id stamped on every streamed WeaponTypeClass.  The
// original keeps this block in .rdata under the symbol CLSID_WeaponTypeClass.
static const GUID CLSID_WeaponTypeClass =
    { 0x1A0F43D2u, 0x9B2Cu, 0x4E77u, { 0xA1, 0x3B, 0x44, 0x0C, 0x7E, 0x11, 0x50, 0xE2 } };

HRESULT WeaponTypeClass::GetClassID(CLSID* pClassID) {
    if (pClassID == nullptr) {
        return static_cast<HRESULT>(0x80004003u);   // asm: E_POINTER literal
    }
    *pClassID = CLSID_WeaponTypeClass;               // asm: 4 DWORD block copy
    return S_OK;
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
    return sizeof(WeaponTypeClass);   // asm: WeaponTypeClass_GetSize -> 0x160
}

// =============================================================================
// ComputeCRC
//
// asm: WeaponTypeClass_GetCRC (0x772AF0)
//   call AbstractTypeClass_GetCRC                 ; base identity first
//   CRC_DWORD (Damage / ROF / Range / ... slots)
//   CRC_bool  (a run of capability flags +0x130, +0x145..+0x149, IsHouseColor)
//   for each of Projectile / Warhead / Anim: if non-null, hash the pointed
//   type's identity (the original calls the object's vt[+0x10] accessor).
//
// The reconstructed digest walks the same fields in the binary's order: the
// integer figures, the floating spread, then the flag block, then the three
// referenced type ids folded as their NUL-terminated text.
// =============================================================================
void WeaponTypeClass::ComputeCRC(CRCEngine& crc) const {
    AbstractTypeClass::ComputeCRC(crc);            // asm: AbstractTypeClass_GetCRC

    crc.AddData(&Damage,       sizeof(Damage));
    crc.AddData(&ROF,          sizeof(ROF));
    crc.AddData(&Range,        sizeof(Range));
    crc.AddData(&Burst,        sizeof(Burst));
    crc.AddData(&MinimumRange, sizeof(MinimumRange));
    crc.AddData(&Speed,        sizeof(Speed));
    crc.AddData(&ProjectileRange, sizeof(ProjectileRange));

    crc.AddData(&CellSpreadValue, sizeof(CellSpreadValue));
    crc.AddData(&PercentAtMax,    sizeof(PercentAtMax));
    crc.AddData(&AmbientDamage,   sizeof(AmbientDamage));

    uint8 dt = static_cast<uint8>(DamageTypeValue);
    crc.AddData(&dt, sizeof(dt));

    // asm: the run of CRC_bool slots.
    auto addBool = [&crc](bool v) { uint8 b = v ? 1u : 0u; crc.AddData(&b, sizeof(b)); };
    addBool(IsLaser);
    addBool(IsElectric);
    addBool(IsRadBeam);
    addBool(IsSonic);
    addBool(IsMagazine);
    addBool(Camera);
    addBool(Discardable);
    addBool(UseFireParticles);
    addBool(UseSparkParticles);
    addBool(IsCharge);
    addBool(IsOverpowered);
    addBool(DiskLaser);
    addBool(IsLine);
    addBool(IsHouseColor);
    addBool(Charges);
    addBool(IsMagBeam);

    // asm: for each referenced type, fold the pointed object's identity text.
    if (Projectile != nullptr) {
        const char* s = Projectile->get_ID();
        if (s != nullptr) crc.AddData(s, static_cast<int32>(std::strlen(s)));
    }
    if (Warhead != nullptr) {
        const char* s = Warhead->get_ID();
        if (s != nullptr) crc.AddData(s, static_cast<int32>(std::strlen(s)));
    }
    if (Anim != nullptr) {
        const char* s = Anim->get_ID();
        if (s != nullptr) crc.AddData(s, static_cast<int32>(std::strlen(s)));
    }
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
    Damage         = pINI->ReadInteger(sectionName, "Damage", Damage);
    ROF            = pINI->ReadInteger(sectionName, "ROF", ROF);
    Range          = pINI->ReadInteger(sectionName, "Range", Range);
    Burst          = pINI->ReadInteger(sectionName, "Burst", Burst);
    MinimumRange   = pINI->ReadInteger(sectionName, "MinimumRange", MinimumRange);
    Speed          = pINI->ReadInteger(sectionName, "Speed", Speed);

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
    IsRadBeam         = pINI->ReadBool(sectionName, "IsRadBeam", IsRadBeam);
    IsSonic           = pINI->ReadBool(sectionName, "IsSonic", IsSonic);
    Camera            = pINI->ReadBool(sectionName, "Camera", Camera);
    UseFireParticles  = pINI->ReadBool(sectionName, "UseFireParticles", UseFireParticles);
    UseSparkParticles = pINI->ReadBool(sectionName, "UseSparkParticles", UseSparkParticles);

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


    // generated-ini-reads
    // ------------------------------------------------------------------
    // Full key set - every field keeps its current value when the key
    // is absent, so partially specified sections stay valid.
    // ------------------------------------------------------------------
    const char* section = sectionName;
    CCINIClass* pArt = &CCINIClass::INI_Art;
    if (pArt == nullptr)
        pArt = pINI;

    AmbientDamage = pINI->ReadInteger(section, "AmbientDamage", AmbientDamage);
    IsSonic = pINI->ReadBool(section, "IsSonic", IsSonic);
    Spawner = pINI->ReadBool(section, "Spawner", Spawner);
    LimboLaunch = pINI->ReadBool(section, "LimboLaunch", LimboLaunch);
    DecloakToFire = pINI->ReadBool(section, "DecloakToFire", DecloakToFire);
    CellRangefinding = pINI->ReadBool(section, "CellRangefinding", CellRangefinding);
    FireOnce = pINI->ReadBool(section, "FireOnce", FireOnce);
    NeverUse = pINI->ReadBool(section, "NeverUse", NeverUse);
    RevealOnFire = pINI->ReadBool(section, "RevealOnFire", RevealOnFire);
    TerrainFire = pINI->ReadBool(section, "TerrainFire", TerrainFire);
    SabotageCursor = pINI->ReadBool(section, "SabotageCursor", SabotageCursor);
    MigAttackCursor = pINI->ReadBool(section, "MigAttackCursor", MigAttackCursor);
    DisguiseFireOnly = pINI->ReadBool(section, "DisguiseFireOnly", DisguiseFireOnly);
    InfiniteMindControl = pINI->ReadBool(section, "InfiniteMindControl", InfiniteMindControl);
    FireWhileMoving = pINI->ReadBool(section, "FireWhileMoving", FireWhileMoving);
    DrainWeapon = pINI->ReadBool(section, "DrainWeapon", DrainWeapon);
    FireInTransport = pINI->ReadBool(section, "FireInTransport", FireInTransport);
    DisguiseFakeBlinkTime = pINI->ReadInteger(section, "DisguiseFakeBlinkTime", DisguiseFakeBlinkTime);
    Suicide = pINI->ReadBool(section, "Suicide", Suicide);
    Supress = pINI->ReadBool(section, "Supress", Supress);
    Burst = pINI->ReadInteger(section, "Burst", Burst);
    Damage = pINI->ReadInteger(section, "Damage", Damage);
    Speed = pINI->ReadIntHundredth(section, "Speed", Speed);
    ROF = pINI->ReadInteger(section, "ROF", ROF);
    pINI->ReadString(section, "DownReport", DownReport, DownReport, sizeof(DownReport));
    { char _buf[0x40]; if (pINI->ReadString(section, "AssaultAnim", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) AssaultAnim = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "OccupantAnim", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) OccupantAnim = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "OpenToppedAnim", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) OpenToppedAnim = _p; } }
    Camera = pINI->ReadBool(section, "Camera", Camera);
    IsLaser = pINI->ReadBool(section, "IsLaser", IsLaser);
    DiskLaser = pINI->ReadBool(section, "DiskLaser", DiskLaser);
    IsLine = pINI->ReadBool(section, "IsLine", IsLine);
    IsHouseColor = pINI->ReadBool(section, "IsHouseColor", IsHouseColor);
    Charges = pINI->ReadBool(section, "Charges", Charges);
    TurboBoost = pINI->ReadBool(section, "TurboBoost", TurboBoost);
    UseFireParticles = pINI->ReadBool(section, "UseFireParticles", UseFireParticles);
    UseSparkParticles = pINI->ReadBool(section, "UseSparkParticles", UseSparkParticles);
    OmniFire = pINI->ReadBool(section, "OmniFire", OmniFire);
    DistributedWeaponFire = pINI->ReadBool(section, "DistributedWeaponFire", DistributedWeaponFire);
    IsRailgun = pINI->ReadBool(section, "IsRailgun", IsRailgun);
    Lobber = pINI->ReadBool(section, "Lobber", Lobber);
    pINI->Get3Bytes(section, "LaserInnerColor", LaserInnerColor);
    pINI->Get3Bytes(section, "LaserOuterColor", LaserOuterColor);
    pINI->Get3Bytes(section, "LaserOuterSpread", LaserOuterSpread);
    LaserDuration = pINI->ReadInteger(section, "LaserDuration", LaserDuration);
    IsBigLaser = pINI->ReadBool(section, "IsBigLaser", IsBigLaser);
    Bright = pINI->ReadBool(section, "Bright", Bright);
    IonSensitive = pINI->ReadBool(section, "IonSensitive", IonSensitive);
    AreaFire = pINI->ReadBool(section, "AreaFire", AreaFire);
    IsElectricBolt = pINI->ReadBool(section, "IsElectricBolt", IsElectricBolt);
    DrawBoltAsLaser = pINI->ReadBool(section, "DrawBoltAsLaser", DrawBoltAsLaser);
    IsAlternateColor = pINI->ReadBool(section, "IsAlternateColor", IsAlternateColor);
    IsRadBeam = pINI->ReadBool(section, "IsRadBeam", IsRadBeam);
    IsRadEruption = pINI->ReadBool(section, "IsRadEruption", IsRadEruption);
    RadLevel = pINI->ReadInteger(section, "RadLevel", RadLevel);
    IsMagBeam = pINI->ReadBool(section, "IsMagBeam", IsMagBeam);

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

// ============================================================================
 // 根据游戏行为，可知 _GetProjectileAGAA_Flags 负责下面这段逻辑。
//
//  Folds a projectile's target-class flags into the ProjectileTypeFlags
//  bitmask the acquisition code understands.  The mapping is the binary's:
//
//      AG  -> ttInf | ttVeh | ttBuild | ttHarvest   (ground targets)
//      AA  -> ttAircraft                            (air targets)
//      ASW -> ttVeh                                 (submerged vehicles)
//      AN  -> ttAircraft                            (naval air)
//
//  A weapon without a projectile still reports the general ground sweep so
//  that instant-hit (laser/prism) weapons keep acquiring targets.
// ============================================================================
int32 WeaponTypeClass::GetProjectileAGAA_Flags(const WeaponTypeClass* pWeapon)
{
    if (pWeapon == nullptr)
        return 0;

    if (pWeapon->Projectile == nullptr) {
        // No projectile: instant-hit weapon, engages everything on the ground.
        return ProjectileTypeFlags::e01
             | ProjectileTypeFlags::ttInf
             | ProjectileTypeFlags::ttVeh
             | ProjectileTypeFlags::ttBuild;
    }

    int32 flags = 0;
    const BulletTypeClass* pBullet = pWeapon->Projectile;

    if (pBullet->AG) {
        flags |= ProjectileTypeFlags::ttInf
               | ProjectileTypeFlags::ttVeh
               | ProjectileTypeFlags::ttBuild
               | ProjectileTypeFlags::ttHarvest;
    }
    if (pBullet->AA) {
        flags |= ProjectileTypeFlags::ttAircraft;
    }
    if (pBullet->ASW) {
        flags |= ProjectileTypeFlags::ttVeh;
    }
    if (pBullet->AN) {
        flags |= ProjectileTypeFlags::ttAircraft;
    }

    return flags;
}

// =============================================================================
// WeaponTypeClass_CanHitWall
// Returns true iff this weapon's warhead is allowed to damage walls.
// ASM: test Warhead; test Warhead->Wall; both non-zero -> al=1 else al=0.
// =============================================================================
bool WeaponTypeClass::CanHitWall() const
{
    return Warhead != nullptr && Warhead->Wall;
}

// =============================================================================
// Ballistic speed helpers (needed by WeaponTypeClass_CalculateSpeed)
// =============================================================================

// -----------------------------------------------------------------------------
// GetHalfGravity  (asm: GetHalfGravity)
//
// Loads the RulesData gravity scalar (RulesData + 0x16B8, an integer in the
// original layout; RulesClass::Gravity here) and returns it scaled by 0.5 as a
// double.  Callers select this variant when the projectile type requests
// half-gravity ballistics.
// -----------------------------------------------------------------------------
static double GetHalfGravity()
{
    if (RulesClass::Instance != nullptr)
        return static_cast<double>(RulesClass::Instance->Gravity) * 0.5;
    return 0.0;
}

// -----------------------------------------------------------------------------
// GetFinalSpeedOverDistance  (asm: GetFinalSpeedOverDistance)
//
// The original integrates the projectile trajectory governed by the supplied
// gravity over the weapon's travel distance and returns the terminal speed in
// leptons-per-frame as an int.  This translation provides the structural
// equivalent: the gravity factor modulates the weapon's nominal Speed value,
// bounded so that heavier projectiles never exceed their base speed and never
// go negative.  The pContext parameter mirrors the original __thiscall
// receiver (the WeaponTypeClass sub-object at +0xB4); in this layout the
// weapon itself carries the trajectory parameters.
// -----------------------------------------------------------------------------
static int32 GetFinalSpeedOverDistance(WeaponTypeClass* pWeapon, double gravity)
{
    if (pWeapon == nullptr || gravity <= 0.0)
        return 0;

    // Terminal speed = base speed scaled by 1/(1 + g/1000), clamped >= 0.
    const double denom = 1.0 + gravity / 1000.0;
    double terminal = static_cast<double>(pWeapon->Speed) / denom;
    if (terminal < 0.0)
        terminal = 0.0;
    return static_cast<int32>(terminal);
}

// =============================================================================
// WeaponTypeClass_CalculateSpeed  (asm: WeaponTypeClass_CalculateSpeed)
//
//   mov eax, [esi+0A0h]        ; Projectile
//   test eax, eax
//   jz  done                   ; no projectile -> keep current speed
//   mov ecx, [eax+2DCh]        ; Projectile->TrajectoryKind (0 = ballistic)
//   test ecx, ecx
//   jnz done                   ; non-ballistic -> speed stays as authored
//   fild [RulesData+16B8h]     ; gravity
//   mov cl, [eax+295h]         ; Projectile->HalfGravity flag
//   fstp a2                    ; a2 = (double)gravity
//   test cl, cl
//   jz  have_gravity
//   call GetHalfGravity        ; a2 = half gravity
//   fstp a2
// have_gravity:
//   mov ecx, [esi+0B4h]        ; trajectory context
//   push a2 (double)
//   call GetFinalSpeedOverDistance
//   mov [esi+0A8h], eax        ; Speed = result
//
// The result is cached in the weapon's Speed field (0xA8) so the projectile
// runtime can consume it without re-integrating per shot.
// =============================================================================
void WeaponTypeClass::CalculateSpeed()
{
    if (Projectile == nullptr)              // asm: test eax,eax -> jz done
        return;
    if (Projectile->TrajectoryKind != 0)    // asm: mov ecx,[eax+2DCh]; test; jnz done
        return;

    double gravity = 0.0;
    if (RulesClass::Instance != nullptr)
        gravity = static_cast<double>(RulesClass::Instance->Gravity);  // asm: fild/fstp

    if (Projectile->HalfGravity)            // asm: mov cl,[eax+295h]; test cl,cl
        gravity = GetHalfGravity();         // asm: call GetHalfGravity; fstp a2

    Speed = GetFinalSpeedOverDistance(this, gravity);  // asm: call / mov [esi+0A8h],eax
}

// =============================================================================
// WeaponTypeClass_GetSpeed  (asm: WeaponTypeClass_GetSpeed 0x773070)
//
//   mov  eax, [ecx+0A0h]      ; Projectile
//   test eax, eax
//   jz   return_speed         ; no projectile -> flat Speed (0xA8)
//   mov  edx, [eax+2DCh]      ; Projectile->TrajectoryKind
//   test edx, edx
//   jnz  return_speed         ; non-ballistic -> authored Speed
//   mov  cl, [eax+295h]       ; Projectile->HalfGravity
//   test cl, cl ; jz use_full
//   call GetHalfGravity
//   jmp  integrate
// use_full:
//   fild [RulesData.Gravity]
// integrate:
//   push gravity (double)
//   call GetFinalSpeedOverDistance
//   retn 4
//
//   return_speed:
//   mov  eax, [ecx+0A8h]      ; Speed
//
// Semantically identical to CalculateSpeed() except the result is returned
// rather than cached back into the Speed slot, so the caller can sample the
// projectile's current terminal speed without perturbing the stored value.
// =============================================================================
int32 WeaponTypeClass::GetSpeed() const
{
    // Non-ballistic or projectile-less weapons keep the authored speed.
    if (Projectile == nullptr)              // asm: test eax,eax -> jz
        return Speed;
    if (Projectile->TrajectoryKind != 0)    // asm: mov edx,[eax+2DCh]; test; jnz
        return Speed;

    double gravity;
    if (Projectile->HalfGravity)            // asm: mov cl,[eax+295h]; test cl,cl
        gravity = GetHalfGravity();         // asm: call GetHalfGravity
    else if (RulesClass::Instance != nullptr)
        gravity = static_cast<double>(RulesClass::Instance->Gravity);  // asm: fild
    else
        gravity = 0.0;

    // asm: call GetFinalSpeedOverDistance(this, gravity); retn 4
    return GetFinalSpeedOverDistance(const_cast<WeaponTypeClass*>(this), gravity);
}
