#pragma once

#include "../Abstract/AbstractTypeClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"

#define MAX_ANIM_LIST 16

class WarheadTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::WarheadType;

    static DynamicVectorClass<WarheadTypeClass*>* Array;

    static WarheadTypeClass* Find(const char* pID);
    static WarheadTypeClass* FindOrAllocate(const char* pID);
    static WarheadTypeClass* GetDefault() { return Array && Array->Count > 0 ? (*Array)[0] : nullptr; }

    virtual ~WarheadTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    bool LoadFromINIList(CCINIClass* pINI);
    bool SaveToINIList(CCINIClass* pINI);

    float GetVersus(int32 armorType) const;
    float GetDamageMultiplier(int32 armorType) const;
    float GetProneDamage() const;
    int32 GetInfDeath() const;
    bool IsWallDestroyerWeapon() const;
    bool IsWoodDestroyerWeapon() const;
    bool IsTiberiumDestroyerWeapon() const;
    bool IsOreDestroyerWeapon() const;
    bool IsSparkyWeapon() const;
    bool IsFireWeapon() const;
    bool IsSmokeWeapon() const;
    bool IsGasWeapon() const;
    bool IsRadiationWeapon() const;
    bool IsSonicWeapon() const;
    bool IsPsychicWeapon() const;
    bool IsMechanicalWeapon() const;
    bool HasBullets() const;
    bool IsTemporal() const;
    bool IsParasite() const;
    bool IsBright() const;
    bool PenetratesBunkerWeapon() const;
    float GetCellSpread() const;
    float GetPercentAtMax() const;
    ParticleTypeClass* GetParticle() const;
    AnimTypeClass* GetAnim() const;
    AnimTypeClass* GetRandomSplashAnim() const;
    bool IsAttachedParticleWeapon() const;
    int32 GetDebrisCount() const;
    void SetVersus(int32 armorType, float value);

    void ParseDebris(const char* debrisStr);
    void ParseSplashList(const char* splashStr);

    WarheadTypeClass(const char* pID) noexcept;

protected:
    explicit WarheadTypeClass(noinit_t) noexcept : AbstractTypeClass(noinit) {}

public:
    int32 ArrayIndex;
    bool IsWallDestroyer;
    bool IsWoodDestroyer;
    bool IsWallAbsoluteDestroyer;
    bool IsTiberiumDestroyer;
    bool IsOreDestroyer;
    float ProneDamage;
    int32 InfDeath;
    float CellSpread;
    float PercentAtMax;
    bool IsSparky;
    bool IsFire;
    bool IsSmoke;
    bool IsGas;
    bool IsLocomotor;
    bool IsSonic;
    bool IsRadiation;
    bool IsPsychic;
    bool IsMechanical;
    bool Bullets;
    ParticleTypeClass* Particle;
    AnimTypeClass* Anim;
    void* SplashList;
    bool IsAttachedParticle;
    bool Temporal;
    bool Parasite;
    bool Bright;
    bool PenetratesBunker;
    float Verses[11];
    AnimTypeClass* AnimList[MAX_ANIM_LIST];
    int32 AnimListCount;
    int32 DebrisCount;
    int32 DebrisMaximumsCount;
    TechnoTypeClass** DebrisTypes;
    int32* DebrisMaximums;
};