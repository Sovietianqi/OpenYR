// ============================================================================
#include <Particles/ParticleTypeClass.h>
#include <Particles/ParticleSystemTypeClass.h>
// ParticleSystemTypeClass.cpp
//
// Type definition for a particle system ([ParticleSystems] INI block).
// Mirrors the original binary: each system type is registered in the global
// Array and resolved by ID when the rules INI is loaded.
//
// NOTE: the constructor, SetName() and the Array definition live in
// ParticleSystemClass.cpp; this file supplies the remaining interface
// methods that were declared but missing from the original project.
// ============================================================================

#include "ParticleSystemTypeClass.h"
#include "../Core/Memory.h"
#include "../IO/CCFileClass.h"
#include "../INI/INIClass.h"

#include <cstring>

// ============================================================================
// Global registration array (single definition)
// ============================================================================

DynamicVectorClass<ParticleSystemTypeClass*>* ParticleSystemTypeClass::Array = nullptr;

// ============================================================================
// Destructor
// ============================================================================

ParticleSystemTypeClass::~ParticleSystemTypeClass()
{
}

// ============================================================================
// Find - linear, case-insensitive ID lookup
// ============================================================================

ParticleSystemTypeClass* ParticleSystemTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        ParticleSystemTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

// ============================================================================
// FindByIndex - index-based lookup
// ============================================================================

ParticleSystemTypeClass* ParticleSystemTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

// ============================================================================
// GetCount
// ============================================================================

int32 ParticleSystemTypeClass::GetCount()
{
    return (Array != nullptr) ? Array->Count : 0;
}

// SetName is defined in ParticleSystemClass.cpp (single definition).

// ============================================================================
// LoadFromINI - [ParticleSystems] entry parser
// ============================================================================

bool ParticleSystemTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    ParticleTypeIndex = pINI->ReadInteger(ID, "ParticleType", -1);


    // generated-ini-reads
    // ------------------------------------------------------------------
    // Full key set - every field keeps its current value when the key
    // is absent, so partially specified sections stay valid.
    // ------------------------------------------------------------------
    const char* section = this->ID;
    CCINIClass* pArt = &CCINIClass::INI_Art;
    if (pArt == nullptr)
        pArt = pINI;

    { char _buf[0x40]; if (pINI->ReadString(section, "HoldsWhat", "", _buf, sizeof(_buf)) > 0) { int32 _i = ParticleTypeClass::FindIndexOrAllocate(_buf); if (_i >= 0) HoldsWhat = _i; } }
    Spawns = pINI->ReadBool(section, "Spawns", Spawns);
    SpawnFrames = pINI->ReadInteger(section, "SpawnFrames", SpawnFrames);
    ParticleCap = pINI->ReadInteger(section, "ParticleCap", ParticleCap);
    SpawnRadius = pINI->ReadInteger(section, "SpawnRadius", SpawnRadius);
    Slowdown = pINI->ReadFixed(section, "Slowdown", Slowdown);
    SpawnCutoff = pINI->ReadFixed(section, "SpawnCutoff", SpawnCutoff);
    SpawnTranslucencyCutoff = pINI->ReadFixed(section, "SpawnTranslucencyCutoff", SpawnTranslucencyCutoff);
    Lifetime = pINI->ReadInteger(section, "Lifetime", Lifetime);
    { char _buf[0x40]; if (pINI->ReadString(section, "BehavesLike", "", _buf, sizeof(_buf)) > 0) BehavesLike = ParticleSystemTypeClass::BehavesLikeFromName(_buf); }
    { int32 _t[3]; if (pINI->Get3Integers(section, "SpawnDirection", _t)) { for (int32 _i = 0; _i < 3; ++_i) SpawnDirection[_i] = static_cast<double>(_t[_i]); } }
    ParticlesPerCoord = pINI->ReadFixed(section, "ParticlesPerCoord", ParticlesPerCoord);
    SpiralDeltaPerCoord = pINI->ReadFixed(section, "SpiralDeltaPerCoord", SpiralDeltaPerCoord);
    SpiralRadius = pINI->ReadFixed(section, "SpiralRadius", SpiralRadius);
    PositionPerturbationCoefficient = pINI->ReadFixed(section, "PositionPerturbationCoefficient", PositionPerturbationCoefficient);
    MovementPerturbationCoefficient = pINI->ReadFixed(section, "MovementPerturbationCoefficient", MovementPerturbationCoefficient);
    VelocityPerturbationCoefficient = pINI->ReadFixed(section, "VelocityPerturbationCoefficient", VelocityPerturbationCoefficient);
    Laser = pINI->ReadBool(section, "Laser", Laser);
    pINI->Get3Bytes(section, "LaserColor", LaserColor);
    SparkSpawnFrames = pINI->ReadInteger(section, "SparkSpawnFrames", SparkSpawnFrames);
    LightSize = pINI->ReadInteger(section, "LightSize", LightSize);
    OneFrameLight = pINI->ReadBool(section, "OneFrameLight", OneFrameLight);
    SpawnSparkPercentage = pINI->ReadFixed(section, "SpawnSparkPercentage", SpawnSparkPercentage);

        return true;
}

// ============================================================================
// ParticleSystemTypeClass - static lookup helpers
// ============================================================================

ParticleSystemTypeClass* ParticleSystemTypeClass::FindOrAllocate(const char* pID)
{
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    ParticleSystemTypeClass* found = Find(pID);
    if (found) return found;
    ParticleSystemTypeClass* newItem = GameCreate<ParticleSystemTypeClass>();
    if (newItem)
    {
        strncpy(newItem->ID, pID, sizeof(newItem->ID) - 1);
        newItem->ID[sizeof(newItem->ID) - 1] = '\0';
    }
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}


// ============================================================================
// ParticleSystemTypeClass - index based lookup (FindIndexOrAllocate)
// ============================================================================
int32 ParticleSystemTypeClass::FindIndexOrAllocate(const char* pID)
{
    if (!pID || !*pID) return -1;

    if (Array)
    {
        for (int32 i = 0; i < Array->Count; ++i)
        {
            ParticleSystemTypeClass* item = Array->GetItem(i);
            if (item && !_strcmpi(item->ID, pID)) return i;
        }
    }

    ParticleSystemTypeClass* pNew = FindOrAllocate(pID);
    if (!pNew || !Array) return -1;

    return Array->Count - 1;
}

int32 ParticleSystemTypeClass::BehavesLikeFromName(const char* pName)
{
    static const char* const names[] = {
        "Smoke", "Gas", "Fire", "Spark", "Railgun"
    };

    if (!pName || !*pName) return -1;

    for (int32 i = 0; i < 5; ++i)
    {
        if (!_strcmpi(pName, names[i])) return i;
    }

    return -1;
}

const char* ParticleSystemTypeClass::BehavesLikeToName(int32 nIndex)
{
    static const char* const names[] = {
        "Smoke", "Gas", "Fire", "Spark", "Railgun"
    };

    if (nIndex < 0 || nIndex >= 5) return nullptr;

    return names[nIndex];
}
