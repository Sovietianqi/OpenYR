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

    // 缺省值取自 asm RulesClass_Addition_Difficulty 的三个 GetFixed / GetBool
    // 立即数：战力/速度/装甲/射速/造价/建造时间为 1.0，RepairDelay 0.02，
    // BuildDelay 0.03，DestroyWalls 缺省为真。
    DifficultyStruct()
        : Firepower(1.0), GroundSpeed(1.0), AirSpeed(1.0)
        , Armor(1.0), ROF(1.0), Cost(1.0), BuildTime(1.0)
        , RepairDelay(0.02), BuildDelay(0.03)
        , BuildSlowdown(false), DestroyWalls(true), ContentScan(false)
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
    // LandTypeCharacteristics - one row of the [LandCharacteristics] table
    //
    //   The binary keeps twelve of these (one per LandType, stride 0x24) in a
    //   single contiguous block, reading each row from the identically named
    //   INI section.  The seven floats are movement multipliers and the flag
    //   says whether the terrain may carry a structure.
    // ========================================================================
    struct LandTypeCharacteristics {
        float Foot;         // +0x00
        float Track;        // +0x04
        float Wheel;        // +0x08
        float Hover;        // +0x0C
        float Winged;       // +0x10  hard-coded 1.0 (air ignores land type)
        float Float;        // +0x14
        float Amphibious;   // +0x18
        float FloatBeach;   // +0x1C
        bool  Buildable;    // +0x20
        uint8 _pad[3];
    };
    static_assert(sizeof(LandTypeCharacteristics) == 0x24,
                  "LandTypeCharacteristics must match the binary's 0x24 stride");

    // Number of rows, one per LandType.
    static constexpr int32 LAND_TYPE_COUNT = 12;

    // The whole [LandCharacteristics] table, indexed by LandType.
    LandTypeCharacteristics LandCharacteristics[LAND_TYPE_COUNT];

    // Movement multiplier for a cell, selected by land type and speed class.
    // Returns 0.0 when either index is out of range.
    double Get_Movement_Multiplier(int32 landType, int32 speedType) const;

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

    // asm: RulesData_LoadTypeData (0x679A00 区, __stdcall(INI), retn 4)
    //   对全部类型向量逐项调 vt+64h（LoadFromINI）——住户、超级武器、动画
    //   （改读 ART.INI）、建筑、飞机、载具、步兵、武器、弹丸、战斗部，随后
    //   逐武器 CalculateSpeed、逐建筑走 nullsub、地形/烟雾/覆盖物/等距瓦片/
    //   粒子、体素动画，最后按 0x20 步长扫 MissionControls 表。
    void LoadTypeData(CCINIClass* pINI);

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
    // [Projectiles] / [WeaponTypes]：弹丸表与武器表的名字登记段。两者与其它
    // 类型列表段同构（逐键取名字 -> FindOrAllocate -> LoadFromINI）。武器
    // LoadFromINI 用 Projectile/Warhead 反查，故弹丸与弹头必须先建表。
    void Read_Projectiles(CCINIClass* pINI);
    void Read_WeaponTypes(CCINIClass* pINI);
    void Read_Particles(CCINIClass* pINI);
    void Read_ParticleSystems(CCINIClass* pINI);
    void Read_AI(CCINIClass* pINI);
    void Read_Powerups(CCINIClass* pINI);
    void Read_LandCharacteristics(CCINIClass* pINI);
    void Read_IQ(CCINIClass* pINI);
    void Read_JumpjetControls(CCINIClass* pINI);
    void Read_Movies(CCINIClass* pINI);
    void Read_AdvancedCommandBar(CCINIClass* pINI, bool isMP = false);
    void Read_Tiberiums(CCINIClass* pINI);
    void Read_TileTypes(CCINIClass* pINI);
    void Read_MissionControl(CCINIClass* pINI);
    void Read_Difficulties(CCINIClass* pINI);

    void PointerGotInvalid(AbstractClass* pInvalid, bool removed);

    // ========================================================================
    // RulesClass_Addition_* - the original binary's rules-loading entry points
    //
    //  The engine assembles the whole RulesClass by invoking one Addition_*
    //  routine per INI section, in a fixed order, from RulesClass::Addition.
    //  The reconstruction's earlier Read_* helpers implement the same work;
    //  these members carry the original names so the dispatch table and the
    //  call order match the binary exactly, and Read_* forwards to them.
    // ========================================================================
    void Addition(CCINIClass* pINI);
    void CreateVectors();

    void Addition_SpecialWeapons(CCINIClass* pINI);
    void Addition_AudioVisual(CCINIClass* pINI);
    void Addition_CrateRules(CCINIClass* pINI);
    void Addition_CombatDamage(CCINIClass* pINI);
    void Addition_Radiation(CCINIClass* pINI);
    void Addition_ElevationModel(CCINIClass* pINI);
    void Addition_WallModel(CCINIClass* pINI);
    void Addition_Difficulty(CCINIClass* pINI);
    void Addition_Colors(CCINIClass* pINI);
    void Addition_ColorAdd(CCINIClass* pINI);
    void Addition_General(CCINIClass* pINI);
    void Addition_MultiplayerDialogSettings(CCINIClass* pINI);
    void Addition_Maximums(CCINIClass* pINI);
    void Addition_InfantryTypes(CCINIClass* pINI);
    void Addition_Countries(CCINIClass* pINI);
    void Addition_VehicleTypes(CCINIClass* pINI);
    void Addition_AircraftTypes(CCINIClass* pINI);
    void Addition_Sides(CCINIClass* pINI);
    void Addition_SuperWeaponTypes(CCINIClass* pINI);
    void Addition_BuildingTypes(CCINIClass* pINI);
    void Addition_TerrainTypes(CCINIClass* pINI);
    void Addition_Teams_obsolete(CCINIClass* pINI);
    void Addition_SmudgeTypes(CCINIClass* pINI);
    void Addition_OverlayTypes_obsolete(CCINIClass* pINI);
    void Addition_Animations(CCINIClass* pINI);
    void Addition_VoxelAnims(CCINIClass* pINI);
    void Addition_Warheads(CCINIClass* pINI);
    void Addition_Particles(CCINIClass* pINI);
    void Addition_ParticleSystems(CCINIClass* pINI);
    void Addition_AI(CCINIClass* pINI);
    void Addition_Powerups(CCINIClass* pINI);
    void Addition_LandCharacteristics(CCINIClass* pINI);
    void Addition_IQ(CCINIClass* pINI);
    void Addition_Movies(CCINIClass* pINI);
    void Addition_AdvancedCommandBar(CCINIClass* pINI, bool isMP = false);
    void LoadDifficulties_unused(CCINIClass* pINI);

    // ========================================================================
    // Helper: get difficulty struct for current difficulty
    // ========================================================================

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
    // Bridge / terrain tile indices
    //
    //   原版 RulesClass 在 +0x155C / +0x1740 处存放"压毁随机上限"（用于
    //   Damage_At_Cell 的钢桥/木桥破坏判定），并持有桥面与木桥的 tile
    //   起始索引，供 (tile - Set + 1) 落在 Middle 区间内时判定为桥体。
    //   这些值由 [General] 段的 BridgeSet / WoodBridgeSet 等键读入。
    // ========================================================================
    int32       field_155C;        // +0x155C  钢桥压毁随机上限（Random2 上限）
    int32       field_1740;        // +0x1740  木桥压毁随机上限
    int32       BridgeSet;         // 钢桥 tile 起始索引
    int32       WoodBridgeSet;     // 木桥 tile 起始索引
    int32       BridgeMiddle1;     // 钢桥中段 tile 起始
    int32       BridgeMiddle2;     // 钢桥中段 tile 起始（第二组）
    WarheadTypeClass* field_FA8;   // +0x0FA8  油桶连锁引爆使用的弹头
    int32       field_C;           // +0x000C  油桶连锁引爆使用的伤害值
    WarheadTypeClass* CrushWarhead2; // +0x0FF0  第二压毁弹头（isCrushWH2）

    // ------------------------------------------------------------------------
    // +0x0B40 / +0x0B4C —— 单位类型免疫列表
    //   Damage_At_Cell 在 what==1(Unit) 且 Scenario 标志 0x800 置位时，
    //   遍历该列表：命中则跳过该单位（原版 asm 比较的是 4 字节类型指针）。
    //   +0x0B40 为列表数据指针，+0x0B4C 为元素个数。
    // ------------------------------------------------------------------------
    TechnoTypeClass** ImmuneUnitTypes = nullptr;   // +0x0B40
    int32             ImmuneUnitTypeCount = 0;     // +0x0B4C

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
    // GateUp/GateDown 在 asm 里是**声音索引**，且来自 [AudioVisual]（
    // asm:1273880 / 1273907，GetString + VocClass_FindIndexOfName），不是
    // [JumpjetControls] 的整数键；下面的 TurnRate..WobbleDeviation 才是
    // RulesData_LoadJumpjetControls 读的 7 个键。
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
    // 注: [AudioVisual] 里**没有** DeploySound 键（asm 核实：该键只出现在
    //   TechnoTypeClass_LoadFromINI，读的是每个单位自己的段）。保留字段是为了
    //   维持 RulesClass 的字段布局与 asm 偏移一致，但规则段永远不会写它。
    int32       DeploySound;
    int32       StormSound;
    DynamicVectorClass<int32> LightningSounds;
    int32       ShellButtonSlideSound;

    // ========================================================================
    // Properties - Movement
    // ========================================================================
    double      WallBuildSpeedCoefficient;
    double      ChargeToDrainRatio;
    // RulesClass+0x1460: the width of the buildable "spacer" ring the engine
    // stamps around every placed structure.  BuildingClass_MarkBaseSpace
    // doubles it on each axis to obtain the total growth.
    int32       BuildBaseSpacer;
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

    // 根据游戏行为，可知 BaseDefenseSuspendSeconds 是"基地遇袭后次要 AI
    //  队伍被冻结的秒数"：某处基地被打时，指挥部会把优先度不足的队伍临时
    //  停摆这么久，好把资源集中到正面战场；换算成帧时每秒按 900 帧计。
    double      BaseDefenseSuspendSeconds;

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
    // asm: Addition_General 从 [General] 用 INIClass_GetFixed 读入并 fstp 到
    // 4 字节槽（RulesClass+0x0EC0），因此这里是 float 而非 int。
    float       AICaptureWoundedMark;
    DynamicVectorClass<int32> AISuperDefenseProbability;
    int32       AISuperDefenseFrames;
    float       AISuperDefenseDistance;

    // ── AI offensive-superweapon target weights ───────────────────────────
    // Per-difficulty priority tables consulted by
 // HouseClass_PickOffensiveSWTarget when deciding which
    // enemy object to aim an offensive superweapon at.  Each entry is indexed
    // by the attacker's AIDifficulty.  The offsets are the binary's own.
    DynamicVectorClass<int32> AITargetWeightConYard;        // +0x1198
    DynamicVectorClass<int32> AITargetWeightWarFactory;     // +0x11B4
    DynamicVectorClass<int32> AITargetWeightPower;          // +0x11D0
    DynamicVectorClass<int32> AITargetWeightBaseDefense;    // +0x1294
    DynamicVectorClass<int32> AITargetWeightPlug;           // +0x12B0
    DynamicVectorClass<int32> AITargetWeightTemple;         // +0x12E8
    DynamicVectorClass<int32> AITargetWeightHoverPad;       // +0x12CC
    DynamicVectorClass<int32> AITargetWeightBuildingOther;  // +0x11EC
    DynamicVectorClass<int32> AITargetWeightEngineer;       // +0x1208
    DynamicVectorClass<int32> AITargetWeightThief;          // +0x1224
    DynamicVectorClass<int32> AITargetWeightHarvester;      // +0x1240
    DynamicVectorClass<int32> AITargetWeightTransport;      // +0x125C
    DynamicVectorClass<int32> AITargetWeightUnitOther;      // +0x1278
    DynamicVectorClass<int32> OverloadCount;
    DynamicVectorClass<int32> OverloadDamage;
    DynamicVectorClass<int32> OverloadFrames;
    float       PurifierBonus;
    float       OccupyDamageMultiplier;
    float       OccupyROFMultiplier;
    int32       OccupyWeaponRange;
    // asm: Addition_CombatDamage 从 [CombatDamage] 用 INIClass_GetFixed 读入并
    // fstp 到 4 字节槽（RulesClass+0x0F4C），故为 float。
    float       BunkerDamageMultiplier;
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

    // [Combat] warhead at RulesClass+0xFA8, used by the "apply 100 damage at
    // waypoint" trigger action.
    WarheadTypeClass* Apply100Warhead;
    BulletTypeClass* NukeProjectile;
    BulletTypeClass* NukeDown;
    WarheadTypeClass* MutateWarhead;
    WarheadTypeClass* MutateExplosionWarhead;
    WarheadTypeClass* EMPulseWarhead;
    BulletTypeClass* EMPulseProjectile;
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

    // ========================================================================
    // Rules INI fields
    // ========================================================================

    // --- Read_General ---
    int32                    V3RocketPauseFrames;
    int32                    V3RocketTiltFrames;
    double                   V3RocketPitchInitial;
    double                   V3RocketPitchFinal;
    double                   V3RocketTurnRate;
    double                   V3RocketRaiseRate;
    double                   V3RocketAcceleration;
    int32                    V3RocketAltitude;
    int32                    V3RocketDamage;
    int32                    V3RocketEliteDamage;
    int32                    V3RocketBodyLength;
    bool                     V3RocketLazyCurve;
    AircraftTypeClass*       V3RocketType;
    int32                    DMislPauseFrames;
    int32                    DMislTiltFrames;
    double                   DMislPitchInitial;
    double                   DMislPitchFinal;
    double                   DMislTurnRate;
    double                   DMislRaiseRate;
    double                   DMislAcceleration;
    int32                    DMislAltitude;
    int32                    DMislDamage;
    int32                    DMislEliteDamage;
    int32                    DMislBodyLength;
    bool                     DMislLazyCurve;
    AircraftTypeClass*       DMislType;
    int32                    CMislPauseFrames;
    int32                    CMislTiltFrames;
    double                   CMislPitchInitial;
    double                   CMislPitchFinal;
    double                   CMislTurnRate;
    double                   CMislRaiseRate;
    double                   CMislAcceleration;
    int32                    CMislAltitude;
    int32                    CMislDamage;
    int32                    CMislEliteDamage;
    int32                    CMislBodyLength;
    bool                     CMislLazyCurve;
    AircraftTypeClass*       CMislType;
    double                   TargetEffectivenessCoefficientDefa;
    double                   TargetSpecialThreatCoefficientDefa;

    // --- Read_Colors ---
    ColorStruct              NoneValue;

    // --- Read_LandCharacteristics ---
    // (the table itself is declared at the top of the class)

    // --- Read_AdvancedCommandBar ---
    char                     NoneValue2[0x20];

    // asm: RulesClass_Addition_AdvancedCommandBar 把 [AdvancedCommandBar] /
    // [MultiplayerAdvancedCommandBar] 的 ButtonList（逗号分隔的按钮名）在命令栏
    // 按钮表里解析成槽位。原版写进 UI 全局命令栏对象；重建侧命令栏尚未落地，
    // 这里保留解析出的按钮名与此处的原始串，供后续消费。最多 0x19（25）个槽。
    static constexpr int32 ADVANCED_COMMAND_BAR_MAX = 0x19;
    char                     AdvancedCommandBarButtons[ADVANCED_COMMAND_BAR_MAX][0x20];
    int32                    AdvancedCommandBarButtonCount;
    bool                     AdvancedCommandBarIsMP;

    // --- Read_Difficulty ---
    double                   FirePower;
    double                   Groundspeed;
    double                   Airspeed;

    // --- Read_Difficulties ---
    DifficultyStruct Difficulties[3];
};

// Most implementations are in RulesClass.cpp