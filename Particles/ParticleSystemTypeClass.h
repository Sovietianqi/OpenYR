#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Abstract/AbstractTypeClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class CCINIClass;
class ParticleTypeClass;

// ============================================================================
// ParticleSystemTypeClass - type definition for a particle system
// Inherits AbstractTypeClass. Describes a kind of particle system (smoke,
// fire, sparks, etc.) by referencing a ParticleTypeClass and the emission
// parameters used to spawn and simulate its particles. Parsed from the
// [ParticleSystems] INI block.
// ============================================================================
class NOVTABLE ParticleSystemTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::ParticleSystemType;

    static DynamicVectorClass<ParticleSystemTypeClass*>* Array;

    static ParticleSystemTypeClass* Find(const char* pID);
    static ParticleSystemTypeClass* FindByIndex(int32 index);
    static int32 GetCount();

    ParticleSystemTypeClass();
    explicit ParticleSystemTypeClass(noinit_t) noexcept : AbstractTypeClass(noinit) {}
    virtual ~ParticleSystemTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override { return E_FAIL; }
    virtual HRESULT Load(IStream* pStm) override { return S_OK; }
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override { return S_OK; }

    virtual AbstractType WhatAmI() const override { return AbstractType::ParticleSystemType; }
    virtual int32 Size() const override { return sizeof(ParticleSystemTypeClass); }

    virtual bool LoadFromINI(CCINIClass* pINI) override;

    void SetName(const char* name);

    // Particle type this system emits (index + resolved pointer)
    int32              ParticleTypeIndex;
    ParticleTypeClass* ParticleType;

    // Emission parameters
    int32 ParticleCount;
    int32 SpawnRate;
    int32 Behavior;
    int32 MaxLifetime;
    int32 SortingOrder;

    bool IsLooping;
    bool Enabled;
};
