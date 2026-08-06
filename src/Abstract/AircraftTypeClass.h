#pragma once

#include <Abstract/TechnoTypeClass.h>

// ============================================================================
// AircraftTypeClass - type definition for aircraft units
// ============================================================================
class NOVTABLE AircraftTypeClass : public TechnoTypeClass {
public:
    static const AbstractType AbsID = AbstractType::AircraftType;
    static DynamicVectorClass<AircraftTypeClass*>* Array;

    // Static lookup / array management
    static AircraftTypeClass* Find(const char* pID);
    static AircraftTypeClass* FindByIndex(int32 index);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    virtual ~AircraftTypeClass();
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

    virtual bool IsFighter() const;
    virtual bool IsStrafe() const;
    virtual bool IsLocked() const;
    virtual bool IsLoaded() const;
    virtual bool IsKamikaze() const;
    virtual bool IsSpyplane() const;
    virtual bool IsParadropping() const;
    virtual bool IsCarryall() const;
    virtual bool IsAntiAir() const;

    // Extended aircraft accessors
    bool   Is_Fighter() const;
    bool   Is_Bomber() const;
    int32  Get_Landing_Spot_Type() const;
    virtual void Resolve_VXL_References();
    virtual Point2D Get_Image_Size() const override;
    virtual AbstractType Get_Build_Queue_Type() const override;

    AircraftTypeClass() noexcept;
    explicit AircraftTypeClass(noinit_t) noexcept : TechnoTypeClass(noinit) {}

public:
    bool        Fighter;
    bool        Strafe;
    bool        Locked;
    bool        Loaded;
    bool        Kamikaze;
    bool        Spyplane;
    bool        Paradropping;
    bool        Carryall;
    bool        AntiAir;
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
    int32       FlightLevel;
    int32       DockOffset;
    int32       NumberOfDocks;
    char        VoxelName[0x20];
    char        HVAName[0x20];
    BYTE        padding_AircraftType[4];
};