#pragma once

#include <Abstract/TechnoTypeClass.h>

// ============================================================================
// UnitTypeClass - type definition for vehicle units
// ============================================================================
class NOVTABLE UnitTypeClass : public TechnoTypeClass {
public:
    static const AbstractType AbsID = AbstractType::UnitType;
    static DynamicVectorClass<UnitTypeClass*>* Array;

    // Static lookup / array management
    static UnitTypeClass* Find(const char* pID);
    static UnitTypeClass* FindByIndex(int32 index);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    virtual ~UnitTypeClass();
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
    int32       WeaponCount;
    WeaponStruct Weapons[2];
    int32       EliteWeaponCount;
    WeaponStruct EliteWeapons[2];
    bool        HasTurret;
    bool        CanCloak;
    bool        IsVoxel;
    bool        HasDeployer;
    bool        HasUndeployer;
    bool        HasFirewall;
    char        VoxelName[0x20];
    char        HVAName[0x20];
    BYTE        padding_UnitType[4];
};