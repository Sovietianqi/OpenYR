#pragma once

#include "../Abstract/AbstractTypeClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"
#include "../Math/CoordStruct.h"

class WeaponTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::WeaponType;

    static DynamicVectorClass<WeaponTypeClass*>* Array;

    static WeaponTypeClass* Find(const char* pID);
    static WeaponTypeClass* FindOrAllocate(const char* pID);

    virtual ~WeaponTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    bool LoadFromINIList(CCINIClass* pINI);
    bool SaveToINIList(CCINIClass* pINI);

    int32 CalculateDamage(TechnoClass* pSource, TechnoClass* pTarget) const;
    int32 CalculateROF(TechnoClass* pSource) const;
    bool IsInRange(CoordStruct source, CoordStruct target) const;
    bool IsAboveMinimumRange(CoordStruct source, CoordStruct target) const;
    bool CanFire(CoordStruct source, CoordStruct target) const;
    float GetCellSpread() const;
    float GetPercentAtMax() const;
    bool IsInstantHit() const;
    bool IsMagazineWeapon() const;
    bool IsChargedWeapon() const;
    bool IsRadiationWeapon() const;
    int32 GetBurstCount() const;
    int32 GetAttackRange() const;
    int32 GetMinimumAttackRange() const;
    int32 GetProjectileSpeed() const;
    DamageType GetDamageType() const;

    WeaponTypeClass(const char* pID) noexcept;

protected:
    explicit WeaponTypeClass(noinit_t) noexcept : AbstractTypeClass(noinit) {}

public:
    int32 ArrayIndex;
    int32 Damage;
    int32 ROF;
    int32 Range;
    int32 Burst;
    int32 MinimumRange;
    int32 CellSpread;
    BulletTypeClass* Projectile;
    WarheadTypeClass* Warhead;
    bool IsLaser;
    bool IsElectric;
    bool IsRadBeam;
    bool IsSonic;
    bool IsMagazine;
    AnimTypeClass* Anim;
    void* Report;
    bool Camera;
    bool Discardable;
    bool UseFireParticles;
    bool UseSparkParticles;
    ParticleTypeClass* AttachedParticleSystem;
    bool IsCharge;
    bool IsOverpowered;
    double AmbientDamage;
    int32 ProjectileRange;
    int32 Speed;
    DamageType DamageTypeValue;
    float CellSpreadValue;
    float PercentAtMax;
};