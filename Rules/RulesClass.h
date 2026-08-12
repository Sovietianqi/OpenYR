#pragma once

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <Math/CoordStruct.h>

// Forward declaration
class CCINIClass;

// ============================================================================
// Forward declarations
// ============================================================================
class AircraftTypeClass;
class AnimTypeClass;
class BuildingTypeClass;
class BulletTypeClass;
class InfantryTypeClass;
class OverlayTypeClass;
class ParticleSystemTypeClass;
class SmudgeTypeClass;
class TerrainTypeClass;
class UnitTypeClass;
class VoxelAnimTypeClass;
class WarheadTypeClass;
class WeaponTypeClass;

struct SHPStruct;

// ============================================================================
// DifficultyStruct
// ============================================================================
struct DifficultyStruct {
    double  Firepower;
    double  GroundSpeed;
    double  AirSpeed;
    double  Armor;
    double  ROF;
    double  Cost;
    double  BuildTime;
    double  RepairDelay;
    double  BuildDelay;
    bool    BuildSlowdown;
    bool    DestroyWalls;
    bool    ContentScan;
    BYTE    unused_4B[5];

    DifficultyStruct()
        : Firepower(1.0), GroundSpeed(1.0), AirSpeed(1.0)
        , Armor(1.0), ROF(1.0), Cost(1.0), BuildTime(1.0)
        , RepairDelay(1.0), BuildDelay(1.0)
        , BuildSlowdown(false), DestroyWalls(false), ContentScan(false)
    {
        for (int32 i = 0; i < 5; ++i) unused_4B[i] = 0;
    }
};

// ============================================================================
// RocketStruct
// ============================================================================
struct RocketStruct {
    int32   PauseFrames;
    int32   TiltFrames;
    float   PitchInitial;
    float   PitchFinal;
    float   TurnRate;
    int32   RaiseRate;
    float   Acceleration;
    int32   Altitude;
    int32   Damage;
    int32   EliteDamage;
    int32   BodyLength;
    bool    LazyCurve;
    AircraftTypeClass* Type;

    RocketStruct()
        : PauseFrames(0), TiltFrames(0)
        , PitchInitial(0.0f), PitchFinal(0.0f)
        , TurnRate(0.0f), RaiseRate(0)
        , Acceleration(0.0f), Altitude(0)
        , Damage(0), EliteDamage(0), BodyLength(0)
        , LazyCurve(false), Type(nullptr) {}
};

// ============================================================================
// Powerup struct
// ============================================================================
enum class Powerup : uint32 {
    Money = 0, Unit = 1, HealBase = 2, Cloak = 3,
    Explosion = 4, Napalm = 5, Squad = 6, Darkness = 7,
    Reveal = 8, Armor = 9, Speed = 10, Firepower = 11,
    ICBM = 12, Invulnerability = 13, Veteran = 14,
    IonStorm = 15, Gas = 16, Tiberium = 17, Pod = 18
};

struct PowerupStruct {
    Powerup Type;
    int32   Amount;
    int32   Chance;
};

// ============================================================================
// RulesClass - Game rules database, singleton
// ============================================================================
class RulesClass {
public:
    // Static singleton
    static RulesClass* Instance;

    // ========================================================================
    // Constructor / Destructor
    // ========================================================================
    RulesClass();
    ~RulesClass();

    // ========================================================================
    // Initialization
    // ========================================================================
    void Init(CCINIClass* pINI);
    void Read_File(CCINIClass* pINI);

    // ========================================================================
    // Section readers
    // ========================================================================
    void Read_SpecialWeapons(CCINIClass* pINI);
    void Read_AudioVisual(CCINIClass* pINI);
    void Read_CrateRules(CCINIClass* pINI);
    void Read_CombatDamage(CCINIClass* pINI);
    void Read_Radiation(CCINIClass* pINI);
    void Read_ElevationModel(CCINIClass* pINI);
    void Read_WallModel(CCINIClass* pINI);
    void Read_Difficulty(CCINIClass* pINI);
    void Read_Colors(CCINIClass* pINI);
    void Read_ColorAdd(CCINIClass* pINI);
    void Read_General(CCINIClass* pINI);
    void Read_MultiplayerDialogSettings(CCINIClass* pINI);
    void Read_Maximums(CCINIClass* pINI);
    void Read_InfantryTypes(CCINIClass* pINI);
    void Read_Countries(CCINIClass* pINI);
    void Read_VehicleTypes(CCINIClass* pINI);
    void Read_AircraftTypes(CCINIClass* pINI);
    void Read_Sides(CCINIClass* pINI);
    void Read_SuperWeaponTypes(CCINIClass* pINI);
    void Read_BuildingTypes(CCINIClass* pINI);
    void Read_TerrainTypes(CCINIClass* pINI);
    void Read_SmudgeTypes(CCINIClass* pINI);
    void Read_OverlayTypes(CCINIClass* pINI);
    void Read_Animations(CCINIClass* pINI);
    void Read_VoxelAnims(CCINIClass* pINI);
    void Read_Warheads(CCINIClass* pINI);
    void Read_Particles(CCINIClass* pINI);
    void Read_ParticleSystems(CCINIClass* pINI);
    void Read_AI(CCINIClass* pINI);
    void Read_Powerups(CCINIClass* pINI);
    void Read_LandCharacteristics(CCINIClass* pINI);
    void Read_IQ(CCINIClass* pINI);
    void Read_JumpjetControls(CCINIClass* pINI);
    void Read_Difficulties(CCINIClass* pINI);
    void Read_Movies(CCINIClass* pINI);
    void Read_AdvancedCommandBar(CCINIClass* pINI);
    void Read_HarvesterRules(CCINIClass* pINI);

    void PointerGotInvalid(AbstractClass* pInvalid, bool removed);

    // ========================================================================
    // Helper: get difficulty struct for current difficulty
    // ========================================================================
    const DifficultyStruct* GetDifficulty(int32 level) const;

    // ========================================================================
    // Properties - General
    // ========================================================================
    int32       DetailMinFrameRateNormal;
    int32       DetailMinFrameRateMovie;
    int32       DetailBufferZoneWidth;
    int32       AmmoCrateDamage;
    UnitTypeClass*          LargeVisceroid;
    UnitTypeClass*          SmallVisceroid;
    int32       AttackingAircraftSightRange;
    double      TunnelSpeed;
    double      TiberiumHeal;
    int32       SelfHealInfantryFrames;
    int32       SelfHealInfantryAmount;
    int32       SelfHealUnitFrames;
    int32       SelfHealUnitAmount;
    bool        FreeMCV;
    bool        BerzerkAllowed;
    int32       PoseDir;
    int32       DeployDir;
    AnimTypeClass*  DropPodPuff;
    int32       WaypointAnimationSpeed;
    AnimTypeClass*  BarrelExplode;
    DynamicVectorClass<VoxelAnimTypeClass*> BarrelDebris;
    ParticleSystemTypeClass* BarrelParticle;

    // ========================================================================
    // Properties - Radar
    // ========================================================================
    float       RadarEventColorSpeed;
    int32       RadarEventMinRadius;
    float       RadarEventSpeed;
    float       RadarEventRotationSpeed;
    int32       FlashFrameTime;
    int32       RadarCombatFlashTime;
    int32       MaxWaypointPathLength;

    // ========================================================================
    // Properties - Visuals
    // ========================================================================
    AnimTypeClass*  Wake;
    AnimTypeClass*  NukeTakeOff;
    AnimTypeClass*  InfantryExplode;
    AnimTypeClass*  FlamingInfantry;
    AnimTypeClass*  InfantryHeadPop;
    AnimTypeClass*  InfantryNuked;
    AnimTypeClass*  InfantryVirus;
    AnimTypeClass*  InfantryBrute;
    AnimTypeClass*  InfantryMutate;
    AnimTypeClass*  Behind;

    // ========================================================================
    // Properties - AI
    // ========================================================================
    double      AITriggerSuccessWeightDelta;
    double      AITriggerFailureWeightDelta;
    double      AITriggerTrackRecordCoefficient;
    int32       VeinholeMonsterStrength;
    int32       MaxVeinholeGrowth;
    int32       VeinholeGrowthRate;
    int32       VeinholeShrinkRate;
    AnimTypeClass*  VeinAttack;
    int32       VeinDamage;
    int32       MaximumQueuedObjects;
    int32       AircraftFogReveal;

    // ========================================================================
    // Properties - Crates
    // ========================================================================
    OverlayTypeClass* WoodCrateImg;
    OverlayTypeClass* CrateImg;
    OverlayTypeClass* WaterCrateImg;
    DynamicVectorClass<AnimTypeClass*> DropPod;
    DynamicVectorClass<AnimTypeClass*> DeadBodies;
    DynamicVectorClass<AnimTypeClass*> MetallicDebris;
    DynamicVectorClass<AnimTypeClass*> BridgeExplosions;

    // ========================================================================
    // Properties - Sounds
    // ========================================================================
    int32       DigSound;
    int32       CreateUnitSound;
    int32       CreateInfantrySound;
    int32       CreateAircraftSound;
    int32       BaseUnderAttackSound;
    int32       GUIMainButtonSound;
    int32       GUIBuildSound;
    int32       GUITabSound;
    int32       GUIOpenSound;
    int32       GUICloseSound;
    int32       GUIMoveOutSound;
    int32       GUIMoveInSound;
    int32       GUIComboOpenSound;
    int32       GUIComboCloseSound;
    int32       GUICheckboxSound;
    int32       ScoreAnimSound;
    int32       IFVTransformSound;
    int32       PsychicSensorDetectSound;
    int32       BuildingGarrisonedSound;
    int32       BuildingAbandonedSound;
    int32       BuildingRepairedSound;
    int32       CheerSound;
    int32       PlaceBeaconSound;
    int32       DefaultChronoSound;
    int32       StartPlanningModeSound;
    int32       AddPlanningModeCommandSound;
    int32       ExecutePlanSound;
    int32       EndPlanningModeSound;
    int32       CrateMoneySound;
    int32       CrateRevealSound;
    int32       CrateFireSound;
    int32       CrateArmourSound;
    int32       CrateSpeedSound;
    int32       CrateUnitSound;
    int32       CratePromoteSound;
    int32       ImpactWaterSound;
    int32       ImpactLandSound;
    int32       SinkingSound;
    int32       BombTickingSound;
    int32       BombAttachSound;
    int32       YuriMindControlSound;
    int32       ChronoInSound;
    int32       ChronoOutSound;
    int32       SpySatActivationSound;
    int32       SpySatDeactivationSound;
    int32       UpgradeVeteranSound;
    int32       UpgradeEliteSound;
    int32       VoiceIFVRepair;
    int32       SlavesFreeSound;
    int32       SlaveMinerDeploySound;
    int32       SlaveMinerUndeploySound;
    int32       BunkerWallsUpSound;
    int32       BunkerWallsDownSound;
    int32       RepairBridgeSound;
    int32       PsychicDominatorActivateSound;
    int32       GeneticMutatorActivateSound;
    int32       PsychicRevealActivateSound;
    int32       MasterMindOverloadDeathSound;
    int32       AirstrikeAbortSound;
    int32       AirstrikeAttackVoice;
    int32       MindClearedSound;
    int32       EnterGrinderSound;
    int32       LeaveGrinderSound;
    int32       EnterBioReactorSound;
    int32       LeaveBioReactorSound;
    int32       ActivateSound;
    int32       DeactivateSound;
    int32       SpyPlaneCamera;
    int32       LetsDoTheTimeWarpOutAgain;
    int32       LetsDoTheTimeWarpInAgain;
    int32       DiskLaserChargeUp;
    int32       SpyPlaneCameraFrames;

    // ========================================================================
    // Properties - Animations
    // ========================================================================
    AnimTypeClass*  Dig;
    AnimTypeClass*  IonBlast;
    AnimTypeClass*  IonBeam;
    DynamicVectorClass<AnimTypeClass*> DamageFireTypes;
    DynamicVectorClass<AnimTypeClass*> WeatherConClouds;
    DynamicVectorClass<AnimTypeClass*> WeatherConBolts;
    AnimTypeClass*  WeatherConBoltExplosion;

    // ========================================================================
    // Properties - Super Weapons
    // ========================================================================
    WarheadTypeClass* DominatorWarhead;
    AnimTypeClass*  DominatorFirstAnim;
    AnimTypeClass*  DominatorSecondAnim;
    int32       DominatorFireAtPercentage;
    int32       DominatorCaptureRange;
    int32       DominatorDamage;
    int32       MindControlAttackLineFrames;
    int32       DrainMoneyFrameDelay;
    int32       DrainMoneyAmount;
    AnimTypeClass*  DrainAnimationType;
    AnimTypeClass*  ControlledAnimationType;
    AnimTypeClass*  PermaControlledAnimationType;

    // ========================================================================
    // Properties - Chrono
    // ========================================================================
    AnimTypeClass*  ChronoBlast;
    AnimTypeClass*  ChronoBlastDest;
    AnimTypeClass*  ChronoPlacement;
    AnimTypeClass*  ChronoBeam;
    AnimTypeClass*  WarpIn;
    AnimTypeClass*  WarpOut;
    AnimTypeClass*  WarpAway;
    AnimTypeClass*  ChronoSparkle1;
    AnimTypeClass*  IronCurtainInvokeAnim;
    AnimTypeClass*  ForceShieldInvokeAnim;
    AnimTypeClass*  WeaponNullifyAnim;
    AnimTypeClass*  AtmosphereEntry;

    // ========================================================================
    // Properties - Prerequisites
    // ========================================================================
    DynamicVectorClass<int32> PrerequisitePower;
    DynamicVectorClass<int32> PrerequisiteFactory;
    DynamicVectorClass<int32> PrerequisiteBarracks;
    DynamicVectorClass<int32> PrerequisiteRadar;
    DynamicVectorClass<int32> PrerequisiteTech;
    DynamicVectorClass<int32> PrerequisiteProc;
    UnitTypeClass*  PrerequisiteProcAlternate;

    // ========================================================================
    // Properties - JumpJet
    // ========================================================================
    int32       GateUp;
    int32       GateDown;
    int32       TurnRate;
    int32       Speed;
    double      Climb;
    int32       CruiseHeight;
    double      Acceleration;
    double      WobblesPerSecond;
    int32       WobbleDeviation;

    // ========================================================================
    // Properties - Radar Events
    // ========================================================================
    DynamicVectorClass<int32> RadarEventSuppressionDistances;
    DynamicVectorClass<int32> RadarEventVisibilityDurations;
    DynamicVectorClass<int32> RadarEventDurations;

    // ========================================================================
    // Properties - Ion Cannon / Prism
    // ========================================================================
    int32       IonCannonDamage;
    int32       RailgunDamageRadius;
    BuildingTypeClass* PrismType;
    int32       PrismSupportModifier;
    int32       PrismSupportMax;
    int32       PrismSupportDelay;
    int32       PrismSupportDuration;
    int32       PrismSupportHeight;

    // ========================================================================
    // Properties - Rockets
    // ========================================================================
    RocketStruct V3Rocket;
    RocketStruct DMisl;
    RocketStruct CMisl;

    // ========================================================================
    // Properties - Paradrop
    // ========================================================================
    int32       ParadropRadius;

    // ========================================================================
    // Properties - General 2
    // ========================================================================
    double      ZoomInFactor;
    double      ConditionRedSparkingProbability;
    double      ConditionYellowSparkingProbability;
    int32       TiberiumExplosionDamage;
    int32       TiberiumStrength;
    float       MinLowPowerProductionSpeed;
    float       MaxLowPowerProductionSpeed;
    float       LowPowerPenaltyModifier;
    float       MultipleFactory;
    int32       MaximumCheerRate;
    double      TreeFlammability;
    double      MissileSpeedVar;
    double      MissileROTVar;
    int32       MissileSafetyAltitude;
    WeaponTypeClass* DropPodWeapon;
    int32       DropPodHeight;
    int32       DropPodSpeed;
    double      DropPodAngle;
    double      ScrollMultiplier;
    double      CrewEscape;
    int32       ShakeScreen;
    int32       HoverHeight;
    double      HoverBob;
    double      HoverBoost;
    double      HoverAcceleration;
    double      HoverBrake;
    double      HoverDampen;
    double      PlacementDelay;

    // ========================================================================
    // Properties - Voxel Debris
    // ========================================================================
    DynamicVectorClass<VoxelAnimTypeClass*> ExplosiveVoxelDebris;
    VoxelAnimTypeClass*  TireVoxelDebris;
    VoxelAnimTypeClass*  ScrapVoxelDebris;
    int32       BridgeVoxelMax;

    // ========================================================================
    // Properties - Cloaking
    // ========================================================================
    int32       CloakingStages;
    int32       RevealTriggerRadius;
    double      ShipSinkingWeight;
    double      IceCrackingWeight;
    double      IceBreakingWeight;
    DynamicVectorClass<int32> IceCrackSounds;
    uint8       CliffBackImpassability;
    double      VeteranRatio;
    double      VeteranCombat;
    double      VeteranSpeed;
    double      VeteranSight;
    double      VeteranArmor;
    double      VeteranROF;
    double      VeteranCap;
    int32       CloakSound;
    int32       SellSound;

    // ========================================================================
    // Properties - Multiplayer
    // ========================================================================
    int32       GameClosed;
    int32       IncomingMessage;
    int32       SystemError;
    int32       OptionsChanged;
    int32       GameForming;
    int32       PlayerLeft;
    int32       PlayerJoined;
    int32       MessageCharTyped;
    int32       Construction;
    DynamicVectorClass<int32> CreditTicks;
    int32       BuildingDieSound;
    int32       BuildingSlam;
    int32       RadarOn;
    int32       RadarOff;
    int32       MovieOn;
    int32       MovieOff;
    int32       ScoldSound;
    int32       TeslaCharge;
    int32       TeslaZap;
    int32       GenericClick;
    int32       GenericBeep;
    int32       BuildingDamageSound;
    int32       HealCrateSound;
    int32       ChuteSound;
    int32       StopSound;
    int32       GuardSound;
    int32       ScatterSound;
    int32       DeploySound;
    int32       StormSound;
    DynamicVectorClass<int32> LightningSounds;
    int32       ShellButtonSlideSound;

    // ========================================================================
    // Properties - Movement
    // ========================================================================
    double      WallBuildSpeedCoefficient;
    double      ChargeToDrainRatio;
    double      TrackedUphill;
    double      TrackedDownhill;
    double      WheeledUphill;
    double      WheeledDownhill;

    // ========================================================================
    // Properties - Spotlight
    // ========================================================================
    int32       SpotlightMovementRadius;
    int32       SpotlightLocationRadius;
    double      SpotlightSpeed;
    double      SpotlightAcceleration;
    double      SpotlightAngle;
    int32       SpotlightRadius;

    // ========================================================================
    // Properties - Misc
    // ========================================================================
    int32       WindDirection;
    int32       CameraRange;
    int32       FlightLevel;
    int32       ParachuteMaxFallRate;
    int32       NoParachuteMaxFallRate;
    int32       BuildingDrop;

    // ========================================================================
    // Properties - Scorches
    // ========================================================================
    DynamicVectorClass<SmudgeTypeClass*> Scorches;
    DynamicVectorClass<SmudgeTypeClass*> Scorches1;
    DynamicVectorClass<SmudgeTypeClass*> Scorches2;
    DynamicVectorClass<SmudgeTypeClass*> Scorches3;
    DynamicVectorClass<SmudgeTypeClass*> Scorches4;

    // ========================================================================
    // Properties - Buildings
    // ========================================================================
    DynamicVectorClass<BuildingTypeClass*> RepairBay;
    BuildingTypeClass* GDIGateOne;
    BuildingTypeClass* GDIGateTwo;
    BuildingTypeClass* NodGateOne;
    BuildingTypeClass* NodGateTwo;
    BuildingTypeClass* WallTower;
    DynamicVectorClass<BuildingTypeClass*> Shipyard;
    BuildingTypeClass* GDIPowerPlant;
    BuildingTypeClass* NodRegularPower;
    BuildingTypeClass* NodAdvancedPower;
    BuildingTypeClass* ThirdPowerPlant;
    DynamicVectorClass<BuildingTypeClass*> BuildConst;
    DynamicVectorClass<BuildingTypeClass*> BuildPower;
    DynamicVectorClass<BuildingTypeClass*> BuildRefinery;
    DynamicVectorClass<BuildingTypeClass*> BuildBarracks;
    DynamicVectorClass<BuildingTypeClass*> BuildTech;
    DynamicVectorClass<BuildingTypeClass*> BuildWeapons;
    DynamicVectorClass<BuildingTypeClass*> AlliedBaseDefenses;
    DynamicVectorClass<BuildingTypeClass*> SovietBaseDefenses;
    DynamicVectorClass<BuildingTypeClass*> ThirdBaseDefenses;
    DynamicVectorClass<int32> AIForcePredictionFudge;
    DynamicVectorClass<BuildingTypeClass*> BuildDefense;
    DynamicVectorClass<BuildingTypeClass*> BuildPDefense;
    DynamicVectorClass<BuildingTypeClass*> BuildAA;
    DynamicVectorClass<BuildingTypeClass*> BuildHelipad;
    DynamicVectorClass<BuildingTypeClass*> BuildRadar;
    DynamicVectorClass<BuildingTypeClass*> ConcreteWalls;
    DynamicVectorClass<BuildingTypeClass*> NSGates;
    DynamicVectorClass<BuildingTypeClass*> EWGates;
    DynamicVectorClass<BuildingTypeClass*> BuildNavalYard;
    DynamicVectorClass<BuildingTypeClass*> BuildDummy;
    DynamicVectorClass<BuildingTypeClass*> NeutralTechBuildings;

    // ========================================================================
    // Properties - Defense
    // ========================================================================
    double      GDIWallDefense;
    double      GDIWallDefenseCoefficient;
    double      NodBaseDefenseCoefficient;
    double      GDIBaseDefenseCoefficient;
    int32       ComputerBaseDefenseResponse;
    int32       MaximumBaseDefenseValue;

    // ========================================================================
    // Properties - Unit Lists
    // ========================================================================
    DynamicVectorClass<UnitTypeClass*> BaseUnit;
    DynamicVectorClass<UnitTypeClass*> HarvesterUnit;
    DynamicVectorClass<AircraftTypeClass*> PadAircraft;
    DynamicVectorClass<AnimTypeClass*> OnFire;
    DynamicVectorClass<AnimTypeClass*> TreeFire;
    AnimTypeClass*  Smoke;
    AnimTypeClass*  Smoke_;
    AnimTypeClass*  MoveFlash;
    AnimTypeClass*  BombParachute;
    AnimTypeClass*  Parachute;
    DynamicVectorClass<AnimTypeClass*> SplashList;
    AnimTypeClass*  SmallFire;
    AnimTypeClass*  LargeFire;

    // ========================================================================
    // Properties - Paratroopers
    // ========================================================================
    InfantryTypeClass* Paratrooper;
    int32       EliteFlashTimer;

    // ========================================================================
    // Properties - Chrono
    // ========================================================================
    int32       ChronoDelay;
    int32       ChronoReinfDelay;
    int32       ChronoDistanceFactor;
    bool        ChronoTrigger;
    int32       ChronoMinimumDelay;
    int32       ChronoRangeMinimum;
    int32       GetChronoVortexChance() const { return 0; }
    int32       GetChronoVortexDamage() const { return 0; }
    int32       GetChronoVortexRadius() const { return 0; }

    // ========================================================================
    // Properties - Paradrop Infantry
    // ========================================================================
    DynamicVectorClass<InfantryTypeClass*> AmerParaDropInf;
    DynamicVectorClass<int32> AmerParaDropNum;
    DynamicVectorClass<InfantryTypeClass*> AllyParaDropInf;
    DynamicVectorClass<int32> AllyParaDropNum;
    DynamicVectorClass<InfantryTypeClass*> SovParaDropInf;
    DynamicVectorClass<int32> SovParaDropNum;
    DynamicVectorClass<InfantryTypeClass*> YuriParaDropInf;
    DynamicVectorClass<int32> YuriParaDropNum;

    // ========================================================================
    // Properties - Secret Units
    // ========================================================================
    DynamicVectorClass<InfantryTypeClass*> AnimToInfantry;
    DynamicVectorClass<InfantryTypeClass*> SecretInfantry;
    DynamicVectorClass<UnitTypeClass*> SecretUnits;
    DynamicVectorClass<BuildingTypeClass*> SecretBuildings;
    int32       SecretSum;

    // ========================================================================
    // Properties - Spy
    // ========================================================================
    InfantryTypeClass* AlliedDisguise;
    InfantryTypeClass* SovietDisguise;
    InfantryTypeClass* ThirdDisguise;
    int32       SpyPowerBlackout;
    float       SpyMoneyStealPercent;
    bool        AttackCursorOnDisguise;

    // ========================================================================
    // Properties - AI (more)
    // ========================================================================
    float       AIMinorSuperReadyPercent;
    int32       AISafeDistance;
    int32       HarvesterTooFarDistance;
    int32       ChronoHarvTooFarDistance;
    DynamicVectorClass<int32> AlliedBaseDefenseCounts;
    DynamicVectorClass<int32> SovietBaseDefenseCounts;
    DynamicVectorClass<int32> ThirdBaseDefenseCounts;
    DynamicVectorClass<int32> AIPickWallDefensePercent;
    int32       AIRestrictReplaceTime;
    int32       ThreatPerOccupant;
    int32       ApproachTargetResetMultiplier;
    int32       CampaignMoneyDeltaEasy;
    int32       CampaignMoneyDeltaHard;
    int32       GuardAreaTargetingDelay;
    int32       NormalTargetingDelay;
    int32       AINavalYardAdjacency;
    DynamicVectorClass<int32> DisabledDisguiseDetectionPercent;
    DynamicVectorClass<int32> AIAutoDeployFrameDelay;
    int32       MaximumBuildingPlacementFailures;
    DynamicVectorClass<int32> AICaptureNormal;
    DynamicVectorClass<int32> AICaptureWounded;
    DynamicVectorClass<int32> AICaptureLowPower;
    DynamicVectorClass<int32> AICaptureLowMoney;
    int32       AICaptureLowMoneyMark;
    int32       AICaptureWoundedMark;
    DynamicVectorClass<int32> AISuperDefenseProbability;
    int32       AISuperDefenseFrames;
    float       AISuperDefenseDistance;
    DynamicVectorClass<int32> OverloadCount;
    DynamicVectorClass<int32> OverloadDamage;
    DynamicVectorClass<int32> OverloadFrames;
    float       PurifierBonus;
    float       OccupyDamageMultiplier;
    float       OccupyROFMultiplier;
    int32       OccupyWeaponRange;
    int32       BunkerDamageMultiplier;
    float       BunkerROFMultiplier;
    int32       BunkerWeaponRangeBonus;
    float       OpenToppedDamageMultiplier;
    int32       OpenToppedRangeBonus;
    int32       OpenToppedWarpDistance;
    float       FallingDamageMultiplier;
    bool        CurrentStrengthDamage;

    // ========================================================================
    // Properties - Infantry types
    // ========================================================================
    InfantryTypeClass* Technician;
    InfantryTypeClass* Engineer;
    InfantryTypeClass* Pilot;
    InfantryTypeClass* AlliedCrew;
    InfantryTypeClass* SovietCrew;
    InfantryTypeClass* ThirdCrew;

    // ========================================================================
    // Properties - Warheads
    // ========================================================================
    WarheadTypeClass* FlameDamage;
    WarheadTypeClass* FlameDamage2;
    WarheadTypeClass* NukeWarhead;
    BulletTypeClass* NukeProjectile;
    BulletTypeClass* NukeDown;
    WarheadTypeClass* MutateWarhead;
    WarheadTypeClass* MutateExplosionWarhead;
    WarheadTypeClass* EMPulseWarhead;
    WarheadTypeClass* EMPulseProjectile;
    WarheadTypeClass* C4Warhead;
    WarheadTypeClass* CrushWarhead;
    WarheadTypeClass* V3Warhead;
    WarheadTypeClass* DMislWarhead;
    WarheadTypeClass* V3EliteWarhead;
    WarheadTypeClass* DMislEliteWarhead;
    WarheadTypeClass* CMislWarhead;
    WarheadTypeClass* CMislEliteWarhead;
    WarheadTypeClass* IvanWarhead;

    // ========================================================================
    // Properties - Ivan
    // ========================================================================
    int32       IvanDamage;
    int32       IvanTimedDelay;
    bool        CanDetonateTimeBomb;
    bool        CanDetonateDeathBomb;
    int32       IvanIconFlickerRate;
    WeaponTypeClass* DeathWeapon;
    SHPStruct*  BOMBCURS_SHP;
    SHPStruct*  CHRONOSK_SHP;

    // ========================================================================
    // Properties - Super Weapons 2
    // ========================================================================
    int32       IronCurtainDuration;
    int32       PsychicRevealRadius;
    WarheadTypeClass* IonCannonWarhead;
    TerrainTypeClass* VeinholeTypeClass;
    DynamicVectorClass<TerrainTypeClass*> DefaultMirageDisguises;
    int32       InfantryBlinkDisguiseTime;

    // ========================================================================
    // Properties - Particle Systems
    // ========================================================================
    ParticleSystemTypeClass* DefaultLargeGreySmokeSystem;
    ParticleSystemTypeClass* DefaultSmallGreySmokeSystem;
    ParticleSystemTypeClass* DefaultSparkSystem;
    ParticleSystemTypeClass* DefaultLargeRedSmokeSystem;
    ParticleSystemTypeClass* DefaultSmallRedSmokeSystem;
    ParticleSystemTypeClass* DefaultDebrisSmokeSystem;
    ParticleSystemTypeClass* DefaultFireStreamSystem;
    ParticleSystemTypeClass* DefaultTestParticleSystem;
    ParticleSystemTypeClass* DefaultRepairParticleSystem;

    // ========================================================================
    // Properties - AI Threat
    // ========================================================================
    double      MyEffectivenessCoefficientDefault;
    double      TargetEffectivenessCoefficientDefault;
    double      TargetSpecialThreatCoefficientDefault;
    double      TargetStrengthCoefficientDefault;
    double      TargetDistanceCoefficientDefault;
    double      DumbMyEffectivenessCoefficient;
    double      DumbTargetEffectivenessCoefficient;
    double      DumbTargetSpecialThreatCoefficient;
    double      DumbTargetStrengthCoefficient;
    double      DumbTargetDistanceCoefficient;
    double      EnemyHouseThreatBonus;
    double      TurboBoost;
    double      AttackInterval;
    double      AttackDelay;
    double      PowerEmergency;

    // ========================================================================
    // Properties - AI Ratios
    // ========================================================================
    double      AirstripRatio;
    int32       AirstripLimit;
    double      HelipadRatio;
    int32       HelipadLimit;
    double      TeslaRatio;
    int32       TeslaLimit;
    double      AARatio;
    int32       AALimit;
    double      DefenseRatio;
    int32       DefenseLimit;
    double      WarRatio;
    int32       WarLimit;
    double      BarracksRatio;
    int32       BarracksLimit;
    int32       RefineryLimit;
    double      RefineryRatio;
    int32       BaseSizeAdd;
    int32       PowerSurplus;
    int32       InfantryReserve;
    int32       InfantryBaseMult;

    // ========================================================================
    // Properties - Crate
    // ========================================================================
    int32       SoloCrateMoney;
    int32       TreeStrength;
    UnitTypeClass* UnitCrateType;

    // ========================================================================
    // Properties - AI Teams
    // ========================================================================
    double      PatrolScan;
    DynamicVectorClass<int32> TeamDelays;
    DynamicVectorClass<int32> AIHateDelays;
    int32       DissolveUnfilledTeamDelay;

    // ========================================================================
    // Properties - AI Ion Cannon
    // ========================================================================
    DynamicVectorClass<int32> AIIonCannonConYardValue;
    DynamicVectorClass<int32> AIIonCannonWarFactoryValue;
    DynamicVectorClass<int32> AIIonCannonPowerValue;
    DynamicVectorClass<int32> AIIonCannonTechCenterValue;
    DynamicVectorClass<int32> AIIonCannonEngineerValue;
    DynamicVectorClass<int32> AIIonCannonThiefValue;
    DynamicVectorClass<int32> AIIonCannonHarvesterValue;
    DynamicVectorClass<int32> AIIonCannonMCVValue;
    DynamicVectorClass<int32> AIIonCannonAPCValue;
    DynamicVectorClass<int32> AIIonCannonBaseDefenseValue;
    DynamicVectorClass<int32> AIIonCannonPlugValue;
    DynamicVectorClass<int32> AIIonCannonHelipadValue;
    DynamicVectorClass<int32> AIIonCannonTempleValue;

    // ========================================================================
    // Properties - AI Misc
    // ========================================================================
    int32       AIAlternateProductionCreditCutoff;
    DynamicVectorClass<int32> MultiplayerAICM;
    DynamicVectorClass<int32> AIVirtualPurifiers;
    DynamicVectorClass<int32> AISlaveMinerNumber;
    DynamicVectorClass<int32> HarvestersPerRefinery;
    DynamicVectorClass<int32> AIExtraRefineries;
    DynamicVectorClass<int32> MinimumAIDefensiveTeams;
    DynamicVectorClass<int32> MaximumAIDefensiveTeams;
    DynamicVectorClass<int32> TotalAITeamCap;
    double      AIUseTurbineUpgradeProbability;
    DynamicVectorClass<int32> FillEarliestTeamProbability;

    // ========================================================================
    // Properties - Misc 2
    // ========================================================================
    double      CloakDelay;
    double      GameSpeedBias;
    double      BaseBias;
    double      ExpSpread;
    int32       FireSupress;
    int32       MaxIQLevels;
    int32       SuperWeapons;
    int32       Production;
    int32       GuardArea;
    int32       RepairSell;
    int32       AutoCrush;
    int32       Scatter;
    int32       ContentScan;
    int32       Aircraft;
    int32       Harvester;
    int32       SellBack;
    int32       AIBaseSpacing;

    // ========================================================================
    // Properties - Crates (Powerups)
    // ========================================================================
    PowerupStruct SilverCrate;
    PowerupStruct WoodCrate;
    PowerupStruct WaterCrate;
    int32       CrateMinimum;
    int32       CrateMaximum;
    int32       unknown_int_1478;
    AnimTypeClass* DropZoneAnim;

    // ========================================================================
    // Properties - Money / Game Options
    // ========================================================================
    int32       MinMoney;
    int32       Money;
    int32       MaxMoney;
    int32       MoneyIncrement;
    int32       MinUnitCount;
    int32       UnitCount;
    int32       MaxUnitCount;
    int32       TechLevel;
    int32       GameSpeed;
    int32       AIDifficultyStruct;
    int32       AIPlayers;
    bool        BridgeDestruction;
    bool        ShadowGrow;
    bool        Shroud;
    bool        Bases;
    bool        TiberiumGrows;
    bool        Crates;
    bool        CaptureTheFlag;
    bool        HarvesterTruce;
    bool        MultiEngineer;
    bool        AlliesAllowed;
    bool        ShortGame;
    bool        FogOfWar;
    bool        MCVRedeploys;
    bool        SuperWeaponsAllowed;
    bool        BuildOffAlly;
    bool        AllyChangeAllowed;
    int32       DropZoneRadius;

    // ========================================================================
    // Properties - Timing
    // ========================================================================
    double      MessageDelay;
    double      SavourDelay;
    int32       Players;
    double      BaseDefenseDelay;
    int32       SuspendPriority;
    double      SuspendDelay;
    double      SurvivorRate;
    int32       AlliedSurvivorDivisor;
    int32       SovietSurvivorDivisor;
    int32       ThirdSurvivorDivisor;
    double      ReloadRate;
    double      AutocreateTime;
    double      BuildupTime;
    int32       HarvesterLoadRate;
    double      HarvesterDumpRate;
    int32       AtomDamage;

    // ========================================================================
    // Properties - Difficulty
    // ========================================================================
    DifficultyStruct Easy;
    DifficultyStruct Normal;
    DifficultyStruct Difficult;
    DWORD       align_1628[4];

    // ========================================================================
    // Properties - Growth
    // ========================================================================
    double      GrowthRate;
    double      ShroudRate;
    double      FogRate;
    double      IceGrowthRate;
    double      VeinGrowthRate;
    int32       IceSolidifyFrameTime;
    double      AmbientChangeRate;
    double      AmbientChangeStep;
    double      CrateRegen;
    double      TimerWarning;
    int32       TiberiumTransmogrify;
    double      unknown_double_1690;
    double      unknown_double_1698;
    double      unknown_double_16A0;
    double      SpeakDelay;
    double      DamageDelay;
    int32       Gravity;
    int32       LeptonsPerSightIncrease;
    int32       Incoming;

    // ========================================================================
    // Properties - Damage
    // ========================================================================
    int32       MinDamage;
    int32       MaxDamage;
    int32       RepairStep;
    double      RepairPercent;
    int32       IRepairStep;
    double      RepairRate;
    double      URepairRate;
    double      IRepairRate;
    double      unknown_double_16F8;
    double      ConditionYellow;
    double      ConditionRed;
    double      IdleActionFrequency;
    int32       CloseEnough;
    int32       Stray;
    int32       RelaxedStray;
    int32       GuardModeStray;
    int32       Crush;

    // ========================================================================
    // Properties - Crate
    // ========================================================================
    int32       CrateRadius;
    int32       HomingScatter;
    int32       BallisticScatter;
    double      RefundPercent;
    int32       BridgeStrength;
    double      BuildSpeed;
    double      C4Delay;
    int32       CreditReserve;
    double      PathDelay;
    int32       BlockagePathDelay;
    double      MovieTime;

    // ========================================================================
    // Properties - Harvester
    // ========================================================================
    int32       TiberiumShortScan;
    int32       TiberiumLongScan;
    int32       SlaveMinerShortScan;
    int32       SlaveMinerSlaveScan;
    int32       SlaveMinerLongScan;
    int32       SlaveMinerScanCorrection;
    int32       SlaveMinerKickFrameDelay;

    // ========================================================================
    // Properties - Lightning
    // ========================================================================
    int32       LightningDeferment;
    int32       LightningDamage;
    int32       LightningStormDuration;
    int32       LightningHitDelay;
    int32       LightningScatterDelay;
    int32       LightningCellSpread;
    int32       LightningSeparation;
    bool        LightningPrintText;
    WarheadTypeClass* LightningWarhead;

    // ========================================================================
    // Properties - Force Shield
    // ========================================================================
    int32       ForceShieldRadius;
    int32       ForceShieldDuration;
    int32       ForceShieldBlackoutDuration;
    int32       ForceShieldPlayFadeSoundTime;
    bool        MutateExplosion;

    // ========================================================================
    // Properties - Misc
    // ========================================================================
    int32       CollapseChance;
    int32       WeedCapacity;
    float       ExtraUnitLight;
    float       ExtraInfantryLight;
    float       ExtraAircraftLight;
    bool        Paranoid;
    bool        CurleyShuffle;
    bool        BlendedFog;
    bool        CompEasyBonus;
    bool        FineDiffControl;
    bool        TiberiumExplosive;
    bool        EnemyHealth;
    bool        AllyReveal;
    bool        SeparateAircraft;
    bool        TreeTargeting;
    bool        NamedCivilians;
    bool        PlayerAutoCrush;
    bool        PlayerReturnFire;
    bool        PlayerScatter;
    bool        RevealByHeight;
    bool        AllowShroudedSubteranneanMoves;
    bool        ShroudGrow;
    bool        NodAIBuildsWalls;
    bool        AIBuildsWalls;
    bool        UseMinDefenseRule;

    // ========================================================================
    // Properties - EMP
    // ========================================================================
    AnimTypeClass* EMPulseSparkles;
    float       EngineerCaptureLevel;
    float       EngineerCaptureLevel_;
    float       TalkBubbleTime;

    // ========================================================================
    // Properties - Radiation
    // ========================================================================
    int32       RadDurationMultiple;
    int32       RadApplicationDelay;
    int32       RadLevelMax;
    int32       RadLevelDelay;
    int32       RadLightDelay;
    double      RadLevelFactor;
    double      RadLightFactor;
    double      RadTintFactor;
    ColorStruct RadColor;
    WarheadTypeClass* RadSiteWarhead;

    // ========================================================================
    // Properties - Elevation
    // ========================================================================
    int32       ElevationIncrement;
    double      ElevationIncrementBonus;
    double      ElevationBonusCap;
    bool        AlliedWallTransparency;
    double      WallPenetratorThreshold;

    // ========================================================================
    // Properties - Colors
    // ========================================================================
    ColorStruct LocalRadarColor;
    ColorStruct LineTrailColorOverride;
    ColorStruct ChronoBeamColor;
    ColorStruct MagnaBeamColor;
    int32       OreTwinkleChance;
    AnimTypeClass* OreTwinkle;

    ColorStruct ColorAdd[0x10];

    int32       LaserTargetColor;
    int32       IronCurtainColor;
    int32       BerserkColor;
    int32       ForceShieldColor;
    float       DirectRockingCoefficient;
    float       FallBackCoefficient;

    // ========================================================================
    // Properties - Locomotion helpers
    // ========================================================================
    int32 GetCloseEnoughSpeed() const { return CloseEnough; }
    int32 GetDropPodImpactDamage() const { return DropPodWeapon ? 100 : 0; }
    int32 GetDropPodImpactRadius() const { return 1; }
};

// Most implementations are in RulesClass.cpp