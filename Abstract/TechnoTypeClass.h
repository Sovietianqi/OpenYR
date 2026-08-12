#pragma once

#include <Abstract/AbstractTypeClass.h>
#include <Abstract/ObjectTypeClass.h>
#include <Containers/DynamicVectorClass.h>

class CCINIClass;
class CRCEngine;
class InfantryTypeClass;
class SHPStruct;

// ============================================================================
// TechnoTypeClass - base for all techno type definitions
// Inherits ObjectTypeClass
// ============================================================================
class NOVTABLE TechnoTypeClass : public ObjectTypeClass {
public:
    static const AbstractType AbsID = AbstractType::TechnoType;

    static DynamicVectorClass<TechnoTypeClass*>* Array;

    static TechnoTypeClass* Find(const char* pID);
    static TechnoTypeClass* FindByIndex(int32 index);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    virtual ~TechnoTypeClass();
    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual bool SaveToINI(CCINIClass* pINI) const;
    virtual AbstractType GetAbstractDerivationID() const;
    virtual bool HasThisID(const char* pID) const;
    virtual int32 GetCRC() const;
    virtual int32 Size() const;
    virtual AbstractType GetClassID() const;
    virtual const char* get_ID() const;
    virtual const wchar_t* GetUIName() const;
    virtual bool IsBuildable() const;
    virtual bool IsTrainable() const;
    virtual bool IsSelectable() const;
    virtual bool IsLegalTarget() const;
    virtual bool IsImmune() const;
    virtual bool IsLegalDamsel() const;
    virtual bool IsInsignificant() const;

    virtual bool HasTurret() const;
    virtual bool CanCloak() const;
    virtual bool IsVoxel() const;
    virtual bool HasDeployer() const;
    virtual bool HasUndeployer() const;
    virtual bool HasFirewall() const;
    virtual int32 GetWeaponCount() const;
    virtual WeaponStruct* GetWeapon(int32 index) const;

    // Extended accessors used by the sidebar / AI
    virtual int32  Get_Max_Speed() const;
    virtual Armor  Get_Armor() const;
    virtual bool   Is_Two_Shooter() const;
    virtual int32  Get_Cameo_Index() const;
    virtual CoordStruct Get_Display_Coords() const;
    virtual bool   Is_Veteran() const;
    virtual bool   Is_Crewed() const;
    virtual void   Resolve_SHP_References() override;
    virtual Point2D Get_Image_Size() const;
    virtual AbstractType Get_Build_Queue_Type() const;

    virtual void ComputeCRC(CRCEngine& crc) const override;

    TechnoTypeClass() noexcept;

protected:
    explicit __forceinline TechnoTypeClass(noinit_t) noexcept : ObjectTypeClass(noinit_t{}) {}

public:
    int32       Speed;
    Armor       ArmorType;
    SpeedType   SpeedTypeVal;
    MovementZone MoveZone;
    AbstractType Factory;
    int32       ROT;
    int32       WeaponCount;
    WeaponStruct Weapons[18];
    bool        HasTurret_;
    bool        CanCloak_;
    bool        IsVoxel_;
    bool        HasDeployer_;
    bool        HasUndeployer_;
    bool        HasFirewall_;
    bool        IsBuildable_;
    bool        IsTrainable_;
    bool        HasPassengers;
    int32       Passengers;
    int32       OpenTopped;
    int32       SizeLimit;
    double      CloakSpeed;
    double      CloakRadius;
    int32       CrewCount;
    InfantryTypeClass* Crew;
    AbstractType CrewType;
    int32       Ammo;
    bool        NoAmmo;
    int32       PipScale;
    int32       OccupyWeaponCount;
    int32       OccupyWeaponRangeBonus;
    bool        IsHarvester;
    bool        IsWeeder;
    bool        IsResourceGatherer;
    bool        IsUndeployable;
    bool        IsBombable;
    bool        IsAutoFire;
    bool        IsGuardRange;
    bool        IsAggressive;
    bool        IsSelectable_;
    bool        IsInsignificant_;
    bool        IsLegalTarget_;
    bool        IsImmune_;
    bool        IsLegalDamsel_;
    bool        IsUnsellable;
    bool        IsRepairable;
    bool        IsSellable;
    bool        IsPowered;
    bool        IsScanner;
    bool        IsSensor;
    bool        IsDetector;
    bool        IsSensors;
    bool        IsPreventAttackMove;
    bool        IsNaval;
    bool        IsLand;
    bool        IsAir;
    bool        IsOrganic;
    bool        IsNeutral;
    bool        IsInfiltratable;
    bool        IsStealthy;
    bool        IsHealable;
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

    // These are actually used by subclasses
    bool        Deployer;
    bool        Undeployer;
    bool        Firewall;
    bool        Turret;
    bool        Cloak;
    bool        Voxel;

    // ------------------------------------------------------------------
    // Additional fields populated by LoadFromINI
    // ------------------------------------------------------------------
    int32       SightRange;
    int32       GuardRange;
    int32       Strength;
    int32       BuildCost;
    int32       BuildTime;
    int32       RepairCost;
    int32       RefundPercent;
    int32       VeteranAbilities[4];
    int32       EliteAbilities[4];
    int32       VeteranRatio;
    int32       InitialVeterancy;
    int32       TurretROT;
    int32       IdleTimer;
    bool        IsCrewed_;
    char        Cameo[0x20];
    char        ImageFile[0x20];
    SHPStruct*  CameoShape;
    SHPStruct*  ImageShape;
    Point2D     ImageSize;
};
