// ============================================================================
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
    ParticleCount     = pINI->ReadInteger(ID, "ParticleCount", 0);
    SpawnRate         = pINI->ReadInteger(ID, "SpawnRate", 0);
    Behavior          = pINI->ReadInteger(ID, "Behavior", 0);
    MaxLifetime       = pINI->ReadInteger(ID, "MaxLifetime", 0);
    SortingOrder      = pINI->ReadInteger(ID, "SortingOrder", 0);
    IsLooping         = pINI->ReadBool(ID, "Looping", false);
    Enabled           = pINI->ReadBool(ID, "Enabled", true);

    return true;
}
