#pragma once

#include <Abstract/TechnoTypeClass.h>

class HouseClass;

// ============================================================================
// UnitTypeClass - type definition for vehicle units
// ============================================================================
class NOVTABLE UnitTypeClass : public TechnoTypeClass {
public:
    static const AbstractType AbsID = AbstractType::UnitType;
    static DynamicVectorClass<UnitTypeClass*>* Array;

    // Static lookup / array management
    static UnitTypeClass* Find(const char* pID);
    static UnitTypeClass* FindOrAllocate(const char* pID);
    static UnitTypeClass* FindByIndex(int32 index);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    virtual ~UnitTypeClass();
    virtual AbstractType WhatAmI() const override { return AbstractType::UnitType; }
    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual bool SaveToINI(CCINIClass* pINI) const override;
    virtual AbstractType GetAbstractDerivationID() const override;
    virtual bool HasThisID(const char* pID) const override;
    virtual int32 GetCRC() const override;
    virtual int32 Size() const override;
    virtual AbstractType GetClassID() const override;
    virtual const char* get_ID() const override;
    virtual const wchar_t* GetUIName() const override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    virtual bool IsHarvester() const;
    virtual bool IsWeeder() const;
    virtual bool IsResourceGatherer() const;
    virtual bool IsUndeployable() const;
    virtual bool IsBombable() const;
    virtual bool IsAutoFire() const;
    virtual bool IsGuardRange() const;
    virtual bool IsAggressive() const;

    // Extended unit accessors
    int32  Get_Max_Passengers() const;
    bool   Is_MCV() const;
    bool   Is_Carryall() const;
    int32  Get_Charge_Level() const;
    virtual void Resolve_VXL_References();

    // Factory: construct a UnitClass of this type owned by pOwner and wire up
    // its Type back-pointer.  Mirrors UnitTypeClass_CreateUnit (returns nullptr
    // if the allocation failed).
    UnitClass* CreateUnit(HouseClass* pOwner) const;

    // Stores the weapon index used by turret slot idxTur.  The original writes
    // dword [this + idxTur*4 + 0x814], the base of the turret-weapon table
    // that begins at NormalTurretWeapon.  Mirrors UnitTypeClass_SetTurretWeapon.
    void SetTurretWeapon(int32 idxTur, int32 idxWeapon);

    // Reads the weapon index for the given IFV mode from the turret-weapon
    // table at +0x814.  Mirrors UnitTypeClass_GetTurretWeaponIndex.
    int32 GetTurretWeaponIndex(int32 IFVMode) const;
    virtual Point2D Get_Image_Size() const override;
    virtual AbstractType Get_Build_Queue_Type() const override;

    UnitTypeClass() noexcept;
    explicit UnitTypeClass(noinit_t) noexcept : TechnoTypeClass(noinit) {}

public:
    bool        Harvester;
    bool        Weeder;
    bool        ResourceGatherer;
    bool        Undeployable;
    bool        Bombable;
    bool        AutoFire;
    bool        GuardRange;
    bool        Aggressive;
    bool        IsSelectable_old;
    bool        IsTilter;
    bool        IsToProtect;
    bool        IsNominal;
    bool        IsRadarInvisible;
    bool        IsDontScore;
    bool        IsNoThreat;
    bool        IsSensorsSight;
    bool        IsHunterSeeker;
    bool        IsIvan;
    bool        IsLeader;
    bool        IsCarryall;
    bool        IsTrain;
    bool        IsConsideredAircraft;
    bool        IsConsideredVehicle;
    bool        IsSimpleDeployer;
    bool        IsFirebase;
    bool        IsSonic;
    bool        IsVan;
    bool        IsBalloonHover;
    bool        IsCyborg;
    bool        IsNotHuman;
    bool        IsImmuneToPsionics;
    bool        IsImmuneToPoison;
    bool        IsImmuneToRadiation;
    bool        IsImmuneToBerserk;
    bool        IsImmuneToEMP;
    bool        IsCrushable;
    bool        IsCrushable2;
    bool        IsTeleporter;
    bool        IsChrono;
    bool        IsBomb;
    bool        IsCow;
    bool        IsDog;
    bool        IsBoris;
    bool        IsArmed;
    bool        IsMissileSpawn;
    float       ThreatPosedValue;
    int32       DeathWeaponIndex;
    int32       WeaponCharge;
    bool        IsFake;
    bool        IsDisableable;
    bool        IsCanBeSuppressed;
    bool        IsCanBeOccupied;
    bool        IsCanBeDriven;
    bool        IsCanBeCaptured;
    bool        IsCanBeRepaired;
    bool        IsCanBeSold;
    bool        IsCanBePowered;
    bool        IsCanBeDestroyed;
    bool        IsCanBeDamaged;
    bool        IsCanBeInfiltrated;
    bool        IsCanBeSpied;
    bool        IsCanBeSabotaged;
    bool        IsCanBeStolen;
    bool        IsCanBeHijacked;
    // 注意：Weapons / EliteWeapons / WeaponCount 全部继承自 TechnoTypeClass
    //   （原版只有一份 18 槽武器表）。这里曾经重复声明过一份 2 槽的表，
    //   结果 Read_INI 写基类那份、战斗代码读子类这份，两边永远对不上：
    //   所有步兵/载具的 GetWeaponRange 返回 0、自动索敌半径为 1 格、
    //   有武器的单位也永远打不出子弹。
    bool        HasTurret;
    bool        CanCloak;
    bool        IsVoxel;
    bool        HasDeployer;
    bool        HasUndeployer;
    bool        HasFirewall;
    char        VoxelName[0x20];
    char        HVAName[0x20];
    BYTE        padding_UnitType[4];
    // ------------------------------------------------------------------
    // Rules / Art INI fields
    // ------------------------------------------------------------------
    bool         CrateGoodie;
    bool         DeployToFire;
    SpeedType    SpeedTypeValue;
    bool         CarriesCrate;
    bool         TooBigToFitUnderBridge;
    int32        HalfDamageSmokeLocation[3];
    bool         UseTurretShadow;
    int32        WalkFrames;
    int32        FiringFrames;
    bool         Passive;
    LandType     MovementRestrictedTo;
    bool         CanBeach;
    bool         SmallVisceroid;
    bool         LargeVisceroid;
    bool         NonVehicle;
    int32        StandingFrames;
    int32        DeathFrames;
    int32        DeathFrameRate;
    int32        Facings;
    int32        StartStandFrame;
    int32        StartWalkFrame;
    int32        StartFiringFrame;
    int32        StartDeathFrame;
    int32        MaxDeathCounter;
    UnitTypeClass* AltImage;
    int32        NormalTurretIndex;
    int32        NormalTurretWeapon;
    int32        RepairTurretIndex;
    int32        RepairTurretWeapon;
    int32        MachineGunTurretIndex;
    int32        MachineGunTurretWeapon;
    int32        FlakTurretIndex;
    int32        FlakTurretWeapon;
    int32        PistolTurretIndex;
    int32        PistolTurretWeapon;
    int32        SniperTurretIndex;
    int32        SniperTurretWeapon;
    int32        ShockTurretIndex;
    int32        ShockTurretWeapon;
    int32        ExplodeTurretIndex;
    int32        ExplodeTurretWeapon;
    int32        BrainBlastTurretIndex;
    int32        BrainBlastTurretWeapon;
    int32        RadCannonTurretIndex;
    int32        RadCannonTurretWeapon;
    int32        ChronoTurretIndex;
    int32        ChronoTurretWeapon;
    int32        TerroristExplodeTurretIndex;
    int32        TerroristExplodeTurretWeapon;
    int32        CowTurretIndex;
    int32        CowTurretWeapon;
    int32        InitiateTurretIndex;
    int32        InitiateTurretWeapon;
    int32        VirusTurretIndex;
    int32        VirusTurretWeapon;
    int32        YuriPrimeTurretIndex;
    int32        YuriPrimeTurretWeapon;
    int32        GuardianTurretIndex;
    int32        GuardianTurretWeapon;
    int32        FiringSyncFrame[10];
    int32        BurstDelay[10];
};