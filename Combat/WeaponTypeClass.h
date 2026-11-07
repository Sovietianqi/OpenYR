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
    // asm: WeaponTypeClass_FindIndex (0x773030) --- linear scan of the type
    // array comparing each entry's ID (this+0x24) against pID, returning the
    // slot index or -1 when absent.
    static int32 FindIndex(const char* pID);

    virtual ~WeaponTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    // asm: WeaponTypeClass_GetCRC (0x772AF0) --- folds the base type digest
    // followed by the weapon's numeric fields and capability flags.
    virtual void ComputeCRC(CRCEngine& crc) const override;

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
    // asm: WeaponTypeClass_GetSpeed (0x773070) --- the projectile's terminal
    // speed at fire time.  A missing / non-ballistic projectile returns the
    // flat Speed field; a ballistic one re-integrates the trajectory with the
    // rules gravity (halved when the projectile requests half-gravity).
    int32 GetSpeed() const;
    DamageType GetDamageType() const;

 // 根据游戏行为，可知 _GetProjectileAGAA_Flags 负责下面这段逻辑。
    //
    //  Translates a weapon's projectile capability flags into the
    //  ProjectileTypeFlags bitmask consumed by TechnoClass::Greatest_Threat
    //  and every SelectAutoTarget override.  Returns 0 when the weapon has no
    //  projectile (instant-hit ray weapons are treated as "all targets").
    static int32 GetProjectileAGAA_Flags(const WeaponTypeClass* pWeapon);

    // True if this weapon's warhead is allowed to damage walls (used by
    // infantry/vehicle wall-clipping checks).  Mirrors WeaponTypeClass_CanHitWall.
    bool CanHitWall() const;

    // Recomputes the ballistic terminal speed into Speed (asm 0xA8) using the
    // RulesData gravity and the projectile's half-gravity flag.  Skipped when
    // the projectile is missing or non-ballistic.  Mirrors
    // WeaponTypeClass_CalculateSpeed (called from RulesData_LoadTypeData).
    void CalculateSpeed();

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
    // ------------------------------------------------------------------
    // Rules / Art INI fields
    // ------------------------------------------------------------------
    bool         Spawner;
    bool         LimboLaunch;
    bool         DecloakToFire;
    bool         CellRangefinding;
    bool         FireOnce;
    bool         NeverUse;
    bool         RevealOnFire;
    bool         TerrainFire;
    bool         SabotageCursor;
    bool         MigAttackCursor;
    bool         DisguiseFireOnly;
    bool         InfiniteMindControl;
    bool         FireWhileMoving;
    bool         DrainWeapon;
    bool         FireInTransport;
    int32        DisguiseFakeBlinkTime;
    bool         Suicide;
    bool         Supress;
    char        DownReport[0x100];
    AnimTypeClass* AssaultAnim;
    AnimTypeClass* OccupantAnim;
    AnimTypeClass* OpenToppedAnim;
    bool         DiskLaser;
    bool         IsLine;
    bool         IsHouseColor;
    bool         Charges;
    bool         TurboBoost;
    bool         OmniFire;
    bool         DistributedWeaponFire;
    bool         IsRailgun;
    bool         Lobber;
    uint8        LaserInnerColor[3];
    uint8        LaserOuterColor[3];
    uint8        LaserOuterSpread[3];
    int32        LaserDuration;
    bool         IsBigLaser;
    bool         Bright;
    bool         IonSensitive;
    bool         AreaFire;
    bool         IsElectricBolt;
    bool         DrawBoltAsLaser;
    bool         IsAlternateColor;
    bool         IsRadEruption;
    int32        RadLevel;
    bool         IsMagBeam;
};