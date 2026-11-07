#pragma once

#include <Abstract/TechnoTypeClass.h>

class IsometricTileType;

// ============================================================================
// FoundationData - one entry of a building's footprint descriptor.
//
//   In the original binary this is the wXY pair at FoundationData: a list of
//   cell offsets (relative to the building's origin cell) that the structure
//   covers, terminated by the sentinel entry { 0x7FFF, 0x7FFF }.  Callers
//   (BuildingClass_GetShrouded, placement code) walk it until they hit the
//   sentinel.  The 7FFF/7FFF "invalid" descriptor is shared by every type
//   whose FoundationData was never assigned.
// ============================================================================
struct FoundationData {
    int16 X;
    int16 Y;

    static const int16 Invalid = 0x7FFF;

    bool IsValid() const {
        return X != Invalid || Y != Invalid;
    }
};

// ============================================================================
// BuildingTypeClass - type definition for buildings
// ============================================================================
class NOVTABLE BuildingTypeClass : public TechnoTypeClass {
public:
    static const AbstractType AbsID = AbstractType::BuildingType;
    static DynamicVectorClass<BuildingTypeClass*>* Array;

    // Static lookup
    static BuildingTypeClass* Find(const char* pID);
    static BuildingTypeClass* FindOrAllocate(const char* pID);
    static BuildingTypeClass* FindByIndex(int32 index);
    static int32 FindIndex(const char* pID);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    virtual ~BuildingTypeClass();
    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual bool SaveToINI(CCINIClass* pINI) const override;

    // BuildingTypeClass_ToTile - the post-load pass over every registered
    // building type.  Each type's own section is queried for "ToTile" and the
    // name is resolved through IsometricTileTypeClass_FindIndex; a miss
    // leaves the stored pointer untouched.  Runs at the tail of
    // 根据游戏行为，可知 ReadScenario 负责下面这段逻辑。
    static void ToTile();

    // Second stage of the art load: the frames that need a theatre-specific
    // lookup (DeployingAnim, RoofDeployingAnim, DoorAnim, UnderDoorAnim,
    // UnderRoofDoorAnim, Rubble, SpecialZOverlay).
    void Load2DArt(int32 idxTheater);
    virtual AbstractType GetAbstractDerivationID() const override;
    virtual bool HasThisID(const char* pID) const override;
    virtual int32 GetCRC() const override;
    virtual int32 Size() const override;
    virtual AbstractType GetClassID() const override;
    virtual const char* get_ID() const override;
    virtual const wchar_t* GetUIName() const override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // Building-specific helpers
    virtual bool IsUndeployable() const;
    virtual bool CanBeSold() const;
    virtual bool CanBeRepaired() const;
    virtual bool IsSimpleDeployer() const;
    virtual bool IsFirebase() const;
    virtual bool IsFactory() const;
    virtual bool IsPowerPlant() const;
    virtual bool IsWall() const;
    virtual bool IsTiberiumStorage() const;

    // ------------------------------------------------------------------------
    // asm vtable probes / overrides
    // ------------------------------------------------------------------------

    // asm: BuildingTypeClass_GetFacing (0x45F3C0).  Buildings have no free
    // heading; the type-level facing is the fixed deploy direction parsed from
    // the "DeployFacing" rules key.
    virtual int32 GetFacing() const;

    // asm: BuildingTypeClass_CanAttackOnTheMove (0x465960).  Structures never
    // fire while relocating, so the type answers false unconditionally.
    virtual bool CanAttackOnTheMove() const override;

    // asm: BuildingTypeClass_GetMaxPips (0x45ECC0).  Caps the number of pips
    // drawn over the structure: a scaled cell-count for the storage/weeder
    // cases, otherwise the PipScale-driven TechnoTypeClass figure.
    virtual int32 GetMaxPips() const;

    // asm: BuildingTypeClass_GetCost (0x45ED70).  The build cost, adjusted by
    // the paired power-plant variant and reduced by any free unit granted with
    // the structure.  Never negative.
    virtual int32 GetCost() const;

    // asm: BuildingTypeClass_GetFoundationData (0x45EC30).  Returns the
    // footprint descriptor: the type's own FoundationData entry, or the shared
    // 7FFF/7FFF "invalid" descriptor when none was assigned.
    FoundationData* GetFoundationData();

    // asm: BuildingTypeClass_GetArrayIndex (0x465DA0).  Index of this type
    // within the owning base-node table (-1 when the pointer is null).
    virtual int32 GetArrayIndex() const;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    int32 Get_Width() const;
    int32 Get_Height() const;

 // Foundation span in cells along each axis.
    // X_Foundation_Value indexes Foundation_CellsX with the Foundation enum;
    // Y_Foundation_Value optionally adds the extra bib row when the building
    // type carries a bib and the caller asks for it.
    int32 X_Foundation_Value() const;
    int32 Y_Foundation_Value(bool bib) const;

    RectangleStruct Get_Occupy_Rect() const;
    int32 Get_Power_Output() const;
    int32 Get_Power_Drain() const;
    int32 Get_Storage_Capacity() const;
    AbstractType Get_Factory_Type() const;
    int32 Find_Exit_Cell(const CoordStruct& baseCoord) const;

    BuildingTypeClass() noexcept;
    explicit BuildingTypeClass(noinit_t) noexcept : TechnoTypeClass(noinit) {}

public:
    int32       BuildingFoundation; // Foundation width/height
    int32       Height;
    int32       Power;
    int32       PowerDrain;
    int32       Bib;
    bool        CanBeSold_;
    bool        IsUndeployable_;
    bool        IsSimpleDeployer_;
    bool        IsFirebase_;
    bool        IsFactory_;
    bool        HasSpotlight;
    bool        HasBib;
    bool        HasHelipad;
    bool        HasDock;
    bool        IsBase;
    bool        IsGate;
    bool        IsOreRefinery;
    bool        IsOreStorage;
    bool        IsWeaponsFactory;
    bool        IsBarracks;
    bool        IsRadar;
    bool        IsTech;
    bool        IsSecretLab;
    bool        IsConstructionYard;
    bool        IsAirport;
    bool        IsWarfactory;
    bool        IsNavalYard;
    bool        IsRepairPad;
    bool        IsMissileSilo;
    bool        IsPowered;
    bool        IsCanC4;
    bool        IsCanBeOccupied;
    bool        IsCanBeDriven;
    bool        IsCanBeCaptured;
    bool        IsCanBeRepaired_;
    bool        IsCanBeSold_;
    bool        IsCanBePowered;
    bool        IsCanBeDestroyed;
    bool        IsCanBeDamaged;
    bool        IsCanBeInfiltrated;
    bool        IsCanBeSpied;
    bool        IsCanBeSabotaged;
    bool        IsCanBeStolen;
    bool        IsCanBeHijacked;
    int32       Storage;
    int32       BuildingAnimCount;
    int32       OccupyCount;
    int32       NumberOfDocks;
    int32       Adjacent;
    int32       MaxWalls;
    int32       SuperWeapon;
    int32       SuperWeapon2;
    int32       WeaponCount;
    WeaponStruct Weapons[2];
    int32       OccupyWeaponCount;
    WeaponStruct OccupyWeapons[2];
    int32       EliteOccupyWeaponCount;
    WeaponStruct EliteOccupyWeapons[2];
    bool        HasSuperWeapon;
    bool        HasSuperWeapon2;
    bool        IsPlug;
    bool        IsDrain;
    bool        IsDrainable;
    bool        IsRig;
    bool        IsRigOwner;
    bool        IsResource;
    bool        IsResourceGatherer;
    bool        IsBombable;
    bool        IsAutoFire;
    bool        IsGuardRange;
    bool        IsAggressive;
    bool        IsUndeployableMember;
    bool        IsSellable;
    bool        IsRepairable;
    bool        IsUnsellable;
    bool        IsUngarrisonable;
    bool        IsNaval;
    bool        IsLand;
    bool        IsAir;
    bool        IsOrganic;
    bool        IsNeutral;
    bool        IsInfiltratable;
    bool        IsStealthy;
    bool        IsHealable;
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
    bool        IsPowered_;
    bool        IsScanner;
    bool        IsSensor;
    bool        IsDetector;
    bool        IsSensors;
    bool        IsPreventAttackMove;
    bool        IsNaval_;
    bool        IsLand_;
    bool        IsAir_;
    bool        IsOrganic_;
    bool        IsNeutral_;
    bool        IsInfiltratable_;
    bool        IsStealthy_;
    bool        IsHealable_;
    bool        IsTilter_;
    bool        IsToProtect_;
    bool        IsNominal_;
    bool        IsRadarInvisible_;
    bool        IsDontScore_;
    bool        IsNoThreat_;
    bool        IsSensorsSight_;
    bool        IsHunterSeeker_;
    bool        IsIvan_;
    bool        IsLeader_;
    bool        IsDock;
    bool        IsHelipad;
    bool        IsHasBib;
    bool        IsHasSpotlight;
    bool        IsBase_;
    bool        IsWall_;
    bool        IsGate_;
    bool        IsOreRefinery_;
    bool        IsOreStorage_;
    bool        IsWeaponsFactory_;
    bool        IsBarracks_;
    bool        IsRadar_;
    bool        IsTech_;
    bool        IsSecretLab_;
    bool        IsConstructionYard_;
    bool        IsAirport_;
    bool        IsWarfactory_;
    bool        IsNavalYard_;
    bool        IsRepairPad_;
    bool        IsMissileSilo_;
    bool        IsPlug_;
    bool        IsDrain_;
    bool        IsDrainable_;
    bool        IsRig_;
    bool        IsRigOwner_;
    bool        IsResource_;
    bool        IsResourceGatherer_;
    bool        IsBombable_;
    bool        IsAutoFire_;
    bool        IsGuardRange_;
    bool        IsAggressive_;
    bool        IsUndeployable__;
    bool        IsSellable_;
    bool        IsRepairable_;
    bool        IsUnsellable_;
    bool        IsUngarrisonable_;
    float       ThreatPosedValue_;
    int32       DeathWeaponIndex_;
    int32       BuildingAnimCount_;
    int32       OccupyCount_;
    int32       NumberOfDocks_;
    int32       Adjacent_;
    int32       MaxWalls_;
    bool        IsPowered2;
    bool        IsPlug2;
    bool        IsDrain2;
    bool        IsDrainable2;
    bool        IsRig2;
    bool        IsRigOwner2;
    bool        IsResource2;
    bool        IsResourceGatherer2;
    BYTE        padding_BuildingType[4];
    // ------------------------------------------------------------------
    // Rules / Art INI fields
    // ------------------------------------------------------------------
    BuildCat     BuildCatValue;
    int32        AntiInfantryValue;
    int32        AntiArmorValue;
    int32        AntiAirValue;
    int32        HalfDamageSmokeLocation1[3];
    int32        HalfDamageSmokeLocation2[3];
    bool         Radar;
    bool         SpySat;
    bool         WaterBound;
    bool         Capturable;
    bool         Powered;
    bool         PoweredSpecial;
    bool         Overpowerable;
    bool         Spyable;
    bool         CanC4;
    bool         WantsExtraSpace;
    bool         Unsellable;
    bool         ClickRepairable;
    bool         CanBeOccupied;
    bool         CanOccupyFire;
    bool         ShowOccupantPips;
    int32        MaxNumberOccupants;
    int32        NumberImpassableRows;
    int32        ProduceCashStartup;
    int32        ProduceCashAmount;
    int32        ProduceCashDelay;
    int32        InfantryGainSelfHeal;
    int32        UnitsGainSelfHeal;
    int32        RefinerySmokeFrames;
    bool         BaseNormal;
    bool         EligibileForAllyBuilding;
    bool         EligibleForDelayKill;
    bool         NeedsEngineer;
    bool         ProtectWithWall;
    bool         Wall;
    bool         Weeder;
    bool         Helipad;
    bool         OrePurifier;
    bool         FactoryPlant;
    UnitTypeClass* FreeUnit;
    bool         HoverPad;
    bool         IsTemple;
    InfantryTypeClass* SecretInfantry;
    UnitTypeClass* SecretUnit;
    double       InfantryCostBonus;
    double       UnitsCostBonus;
    double       AircraftCostBonus;
    double       BuildingsCostBonus;
    double       DefensesCostBonus;
    bool         TogglePower;
    int32 BuildupSound;
    int32 PackupSound;
    int32 CreateUnitSound;
    int32 UnitExitSound;
    int32 UnitEnterSound;
    int32 WorkingSound;
    int32 NotWorkingSound;
    bool         UnitRepair;
    bool         UnitReload;
    bool         Bunker;
    bool         Cloning;
    bool         Grinding;
    bool         UnitAbsorb;
    bool         InfantryAbsorb;
    bool         SecretLab;
    bool         DockUnload;
    bool         Gate;
    bool         SAM;
    bool         ConstructionYard;
    bool         NukeSilo;
    bool         Refinery;
    bool         WeaponsFactory;
    bool         LaserFencePost;
    bool         LaserFence;
    bool         FirestormWall;
    bool         Hospital;
    bool         Armory;
    bool         GDIBarracks;
    bool         NODBarracks;
    bool         YuriBarracks;
    bool         EMPulseCannon;
    bool         TickTank;
    double       ChargedAnimTime;
    bool         CloakGenerator;
    bool         SensorArray;
    int32        CloakRadiusInCells;
    int32        PsychicDetectionRadius;
    int32        BarrelStartPitch;
    int32        DeployFacing;
    int32        LightVisibility;
    double       LightIntensity;
    double       LightRedTint;
    double       LightGreenTint;
    double       LightBlueTint;
    double       GateCloseDelay;
    bool         InvisibleInGame;
    char        PowersUpBuilding[0x20];
    int32        PowersUpToLevel;
    // BuildingTypeClass_PoweredUnit (asm +0xF8C, three 0x44-byte slots).  A
    // structure may power up to three external units; each entry names the
    // unit id that receives a continuous power feed.
    bool         PoweredUnit[BUILDING_POWERED_UNIT_COUNT];
    // BuildingTypeClass_DontSaveToMap (asm +0x16BF).  Set on decoration types
    // (walls, fence posts, bridge repair huts) that must not be written into
    // the map INI's [Structures] list.
    bool         DontSaveToMap;
    // BuildingTypeClass_GapGenerator-driven "super charged" variant source:
    // whether the structure is currently feeding a super-charged gap.
    bool         GapSuperCharged;
    // BuildingTypeClass reference at +0x16C7: cloak generator enabled.
    bool         IsCloakGenerator;
    // BuildingTypeClass reference at +0x16C8: private/sensor cloak; the
    // structure reveals its own radius without a public gap.
    bool         PrivateCloak;
    // BuildingTypeClass +0x16B7: the type can be driven over when its door is
    // open (gates, walls-with-door).
    bool         IsTraversableType;
    // BuildingTypeClass +0x1707: the radius shown for sensor/detector types.
    int32        SensorRadiusInCells;
    bool         BridgeRepairHut;
    bool         HasStupidGuardMode;
    bool         CrateBeneath;
    bool         CrateBeneathIsMoney;
    bool         LeaveRubble;
    bool         PlaceAnywhere;
    bool         ICBMLauncher;
    bool         Artillary;
    int32        TargetCoordOffset[3];
    int32        ExitCoord[3];
    bool         AIBuildThis;
    bool         IsBaseDefense;
    bool         IsThreatRatingNode;
    bool         ConcentricRadialIndicator;
    int32        ExtraPower;
    int32        OccupyHeight;
    bool         Recoilless;
    bool         DoubleThick;
    bool         Flat;
    bool         SiloDamage;
    bool         ChargeAnim;
    bool         IsAnimDelayedFire;
    int32        DelayedFireDelay;
    OverlayTypeClass* ToOverlay;
    Foundation   FoundationValue;
    int32        MidPoint;
    int32        DoorStages;
    bool         DamagedDoor;
    bool         TerrainPalette;
    int32        GateStages;
    int32        PrimaryFirePixelOffset[2];
    int32        SecondaryFirePixelOffset[2];
    bool         PrimaryFireDualOffset;
    bool         ExtraDamageStage;
    int32        SpecialZOverlayZAdjust;
    int32        NormalZAdjust;
    int32        ZShapePointMove[2];
    int32        ExtraLight;
    bool         CanHideThings;
    int32        QueueingCell[2];
    char        BuildupName[0x20];
    char        DeployingAnim[0x20];
    char        RoofDeployingAnim[0x20];
    char        DoorAnim[0x20];
    char        UnderDoorAnim[0x20];
    char        UnderRoofDoorAnim[0x20];
    char        Rubble[0x20];
    char        SpecialZOverlay[0x20];
    char        AnimIdle[0x20];
    char        AnimActive[0x20];
    char        AnimAux1[0x20];
    char        AnimAux2[0x20];
    char        ActiveAnim[0x20];
    char        ActiveAnimDamaged[0x20];
    char        ActiveAnimGarrisoned[0x20];
    int32        ActiveAnimX;
    int32        ActiveAnimY;
    int32        ActiveAnimZAdjust;
    int32        ActiveAnimYSort;
    bool         ActiveAnimPowered;
    bool         ActiveAnimPoweredLight;
    bool         ActiveAnimPoweredEffect;
    bool         ActiveAnimPoweredSpecial;
    char        ActiveAnimTwo[0x20];
    char        ActiveAnimTwoDamaged[0x20];
    char        ActiveAnimTwoGarrisoned[0x20];
    int32        ActiveAnimTwoX;
    int32        ActiveAnimTwoY;
    int32        ActiveAnimTwoZAdjust;
    int32        ActiveAnimTwoYSort;
    bool         ActiveAnimTwoPowered;
    bool         ActiveAnimTwoPoweredLight;
    bool         ActiveAnimTwoPoweredEffect;
    bool         ActiveAnimTwoPoweredSpecial;
    char        ActiveAnimThree[0x20];
    char        ActiveAnimThreeDamaged[0x20];
    char        ActiveAnimThreeGarrisoned[0x20];
    int32        ActiveAnimThreeX;
    int32        ActiveAnimThreeY;
    int32        ActiveAnimThreeZAdjust;
    int32        ActiveAnimThreeYSort;
    bool         ActiveAnimThreePowered;
    bool         ActiveAnimThreePoweredLight;
    bool         ActiveAnimThreePoweredEffect;
    bool         ActiveAnimThreePoweredSpecial;
    char        ActiveAnimFour[0x20];
    char        ActiveAnimFourDamaged[0x20];
    char        ActiveAnimFourGarrisoned[0x20];
    int32        ActiveAnimFourX;
    int32        ActiveAnimFourY;
    int32        ActiveAnimFourZAdjust;
    int32        ActiveAnimFourYSort;
    bool         ActiveAnimFourPowered;
    bool         ActiveAnimFourPoweredLight;
    bool         ActiveAnimFourPoweredEffect;
    bool         ActiveAnimFourPoweredSpecial;
    char        SuperAnim[0x20];
    char        SuperAnimDamaged[0x20];
    char        SuperAnimGarrisoned[0x20];
    int32        SuperAnimX;
    int32        SuperAnimY;
    int32        SuperAnimZAdjust;
    int32        SuperAnimYSort;
    bool         SuperAnimPowered;
    bool         SuperAnimPoweredLight;
    bool         SuperAnimPoweredEffect;
    bool         SuperAnimPoweredSpecial;
    char        SuperAnimTwo[0x20];
    char        SuperAnimTwoDamaged[0x20];
    char        SuperAnimTwoGarrisoned[0x20];
    int32        SuperAnimTwoX;
    int32        SuperAnimTwoY;
    int32        SuperAnimTwoZAdjust;
    int32        SuperAnimTwoYSort;
    bool         SuperAnimTwoPowered;
    bool         SuperAnimTwoPoweredLight;
    bool         SuperAnimTwoPoweredEffect;
    bool         SuperAnimTwoPoweredSpecial;
    char        SuperAnimThree[0x20];
    char        SuperAnimThreeDamaged[0x20];
    char        SuperAnimThreeGarrisoned[0x20];
    int32        SuperAnimThreeX;
    int32        SuperAnimThreeY;
    int32        SuperAnimThreeZAdjust;
    int32        SuperAnimThreeYSort;
    bool         SuperAnimThreePowered;
    bool         SuperAnimThreePoweredLight;
    bool         SuperAnimThreePoweredEffect;
    bool         SuperAnimThreePoweredSpecial;
    char        SuperAnimFour[0x20];
    char        SuperAnimFourDamaged[0x20];
    char        SuperAnimFourGarrisoned[0x20];
    int32        SuperAnimFourX;
    int32        SuperAnimFourY;
    int32        SuperAnimFourZAdjust;
    int32        SuperAnimFourYSort;
    bool         SuperAnimFourPowered;
    bool         SuperAnimFourPoweredLight;
    bool         SuperAnimFourPoweredEffect;
    bool         SuperAnimFourPoweredSpecial;
    char        SpecialAnim[0x20];
    char        SpecialAnimDamaged[0x20];
    char        SpecialAnimGarrisoned[0x20];
    int32        SpecialAnimX;
    int32        SpecialAnimY;
    int32        SpecialAnimZAdjust;
    int32        SpecialAnimYSort;
    bool         SpecialAnimPowered;
    bool         SpecialAnimPoweredLight;
    bool         SpecialAnimPoweredEffect;
    bool         SpecialAnimPoweredSpecial;
    char        SpecialAnimTwo[0x20];
    char        SpecialAnimTwoDamaged[0x20];
    char        SpecialAnimTwoGarrisoned[0x20];
    int32        SpecialAnimTwoX;
    int32        SpecialAnimTwoY;
    int32        SpecialAnimTwoZAdjust;
    int32        SpecialAnimTwoYSort;
    bool         SpecialAnimTwoPowered;
    bool         SpecialAnimTwoPoweredLight;
    bool         SpecialAnimTwoPoweredEffect;
    bool         SpecialAnimTwoPoweredSpecial;
    char        SpecialAnimThree[0x20];
    char        SpecialAnimThreeDamaged[0x20];
    char        SpecialAnimThreeGarrisoned[0x20];
    int32        SpecialAnimThreeX;
    int32        SpecialAnimThreeY;
    int32        SpecialAnimThreeZAdjust;
    int32        SpecialAnimThreeYSort;
    bool         SpecialAnimThreePowered;
    bool         SpecialAnimThreePoweredLight;
    bool         SpecialAnimThreePoweredEffect;
    bool         SpecialAnimThreePoweredSpecial;
    char        SpecialAnimFour[0x20];
    char        SpecialAnimFourDamaged[0x20];
    char        SpecialAnimFourGarrisoned[0x20];
    int32        SpecialAnimFourX;
    int32        SpecialAnimFourY;
    int32        SpecialAnimFourZAdjust;
    int32        SpecialAnimFourYSort;
    bool         SpecialAnimFourPowered;
    bool         SpecialAnimFourPoweredLight;
    bool         SpecialAnimFourPoweredEffect;
    bool         SpecialAnimFourPoweredSpecial;
    char        LowPower[0x20];
    char        LowPowerDamaged[0x20];
    char        LowPowerGarrisoned[0x20];
    int32        LowPowerX;
    int32        LowPowerY;
    int32        LowPowerZAdjust;
    int32        LowPowerYSort;
    bool         LowPowerPowered;
    bool         LowPowerPoweredLight;
    bool         LowPowerPoweredEffect;
    bool         LowPowerPoweredSpecial;
    char        SuperLowPower[0x20];
    char        SuperLowPowerDamaged[0x20];
    char        SuperLowPowerGarrisoned[0x20];
    int32        SuperLowPowerX;
    int32        SuperLowPowerY;
    int32        SuperLowPowerZAdjust;
    int32        SuperLowPowerYSort;
    bool         SuperLowPowerPowered;
    bool         SuperLowPowerPoweredLight;
    bool         SuperLowPowerPoweredEffect;
    bool         SuperLowPowerPoweredSpecial;
    char        ProductionAnim[0x20];
    char        ProductionAnimDamaged[0x20];
    char        ProductionAnimGarrisoned[0x20];
    int32        ProductionAnimX;
    int32        ProductionAnimY;
    int32        ProductionAnimZAdjust;
    int32        ProductionAnimYSort;
    char        IdleAnim[0x20];
    char        IdleAnimDamaged[0x20];
    char        IdleAnimGarrisoned[0x20];
    int32        IdleAnimX;
    int32        IdleAnimY;
    int32        IdleAnimZAdjust;
    int32        IdleAnimYSort;
    bool         IdleAnimPowered;
    bool         IdleAnimPoweredLight;
    bool         IdleAnimPoweredEffect;
    bool         IdleAnimPoweredSpecial;
    char        PreProductionAnim[0x20];
    char        PreProductionAnimDamaged[0x20];
    char        PreProductionAnimGarrisoned[0x20];
    int32        PreProductionAnimX;
    int32        PreProductionAnimY;
    int32        PreProductionAnimZAdjust;
    int32        PreProductionAnimYSort;
    char        TurretAnim[0x20];
    char        TurretAnimDamaged[0x20];
    char        TurretAnimGarrisoned[0x20];
    int32        TurretAnimX;
    int32        TurretAnimY;
    int32        TurretAnimZAdjust;
    int32        TurretAnimYSort;
    bool         TurretAnimIsVoxel;
    bool         BarrelAnimIsVoxel;
    char        VoxelBarrelFile[0x20];
    int32        VoxelBarrelOffsetToPitchPivotPoint[3];
    int32        VoxelBarrelOffsetToRotatePivotPoint[3];
    int32        VoxelBarrelOffsetToBuildingPivotPoint[3];
    int32        VoxelBarrelOffsetToBarrelEnd[3];
    int32        Upgrades;
    int32        AddOccupy[8][2];
    int32        RemoveOccupy[8][2];
    char        PowerUpAnim[4][0x10];
    char        PowerUpDamagedAnim[4][0x10];
    int32       PowerUpLocXX[4];
    int32       PowerUpLocYY[4];
    int32       PowerUpLocZZ[4];
    int32       PowerUpYSort[4];
    int32        DockingOffset[4][3];
    int32        MuzzleFlash;
    int32        DamageFireOffset[2];
    int32        CaptureEvaEvent;
    BuildingTypeClass* SecretBuilding;

    // "ToTile" - the tile set this building is drawn from, resolved by
    // BuildingTypeClass_ToTile after the rules are loaded.
    IsometricTileType* TileToUse;
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知建筑类型层补全占地折算、标价、放置检查、地基
    // 初始化、主图/建造图装载与按坐标生成实例等入口。
    // ------------------------------------------------------------------------
    virtual void   GetAreaInLeptons(CoordStruct& out) const;
    virtual int32  GetPrice(HouseClass* pOwner) const;
    virtual bool   CanPlaceHere(int32 x, int32 y) const;
    virtual void   Buildup();
    virtual void   ClearBuildup();
    virtual void   InitFoundations();
    virtual void   InitFoundationOutlines();
    virtual void   LoadImageSHP(int32 idxTheater);
    // Queries the loaded building image dimensions into pOut[0..2]:
    // original writes the SHP frame width into both pOut[0] and pOut[1] and
    // the frame height into pOut[2]; zeros when no image is loaded.
    // Mirrors BuildingTypeClass_GetImageDimensions.
    virtual void   GetImageDimensions(int32* pOut);
    virtual void   LoadArt();
    virtual BuildingClass* SpawnAtCoords(const CoordStruct& coord, HouseClass* pOwner);
    // asm: BuildingTypeClass_CreateBuilding (0x45E860).  Allocates a fresh
    // BuildingClass bound to this type and the given owner; returns null when
    // the allocation fails.  Placement is left to the caller.
    virtual BuildingClass* CreateBuilding(HouseClass* pOwner);
    static  BuildingTypeClass* FindOrAllocateFromList(DynamicVectorClass<BuildingTypeClass*>& list, const char* pID);

    // Footprint descriptor: a {0x7FFF,0x7FFF}-terminated table of cell offsets
    // this structure covers.  Null until InitFoundations (or the INI loader)
    // assigns one; GetFoundationData substitutes the shared invalid entry.
    FoundationData* FoundationDataPtr = nullptr;

    DynamicVectorClass<int32> FoundationCells;     // 地基覆盖的格偏移序列
    DynamicVectorClass<int32> FoundationOutlines;  // 地基描边格偏移序列
    // asm: BuildingTypeClass +0x1784 -- a runtime VectorClass<TPoint3D<int>>
    //   (vftable +0x1784, data +0x1788, count +0x178C).  The 12-byte entries
    //   are the per-dock offsets consulted by BuildingClass_GetDockCoord; the
    //   table is streamed verbatim by BuildingTypeClass_Load/Save.  OpenYR
    //   models it as a vector of 3-int points (CoordStruct).
    DynamicVectorClass<CoordStruct> DockingOffsets; // +0x1784 入坞点偏移表
    bool  DemandLoadBuildup = false;      // 建造图是否按需加载
    void* BuildupData = nullptr;            // 已装载的建造图动画句柄
    bool  BuildupLoaded = false;          // 建造图装载标志

    // ------------------------------------------------------------------------
    // +0x1701 原版 BuildingTypeClass 的免疫/忽略标志字节。
    //   Damage_At_Cell 在结算建筑伤害前检查该位：非零表示该建筑类型
    //   不接受范围伤害（如特殊结构）。取自 IDA 结构偏移。
    // ------------------------------------------------------------------------
    uint8 field_1701 = 0;                 // +0x1701  范围伤害免疫标志

    // ------------------------------------------------------------------------
    // BaseClass_Placed (0x42F270) 依赖的两个字段。
    //
    // IDA 没有给这两个偏移命名，但两处用法都能确定语义：
    //
    //   +0x408  field_408 -- "这个建筑类型是否参与基地节点表"的开关。原版
    //     BaseClass_Placed 的第二分支先用它做前置门（test eax,eax / jz ret），
    //     为 0 时整段跳过、不写任何节点。
    //
    //   +0x0DF8 field_DF8 -- BuildingTypeClass 的专属区首字段（紧跟在 0xDF8
    //     字节的 TechnoTypeClass t 之后）。原版在该分支里用它和节点的
    //     BaseNodeClass::pType 做**指针**比较，所以它存的是另一个
    //     BuildingTypeClass*（即"此建筑对应哪个基地节点原型"）。
    //
    // 两个偏移在 1:1 复刻中是分开的、不可互换 —— 门用的是前者，比较用的是
    // 后者。
    // ------------------------------------------------------------------------
    int32 field_408 = 0;                     // +0x408  参与基地节点表的开关
    BuildingTypeClass* field_DF8 = nullptr;  // +0x0DF8 对应节点原型的类型指针

};
