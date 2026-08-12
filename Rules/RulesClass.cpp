#include <Rules/RulesClass.h>
#include <Core/Definitions.h>
#include <INI/INIClass.h>

#include <cstring>
#include <cstdlib>
#include <cmath>

#include <Abstract/InfantryTypeClass.h>
#include <Abstract/UnitTypeClass.h>
#include <Abstract/AircraftTypeClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Abstract/TerrainTypeClass.h>
#include <Abstract/SmudgeTypeClass.h>
#include <Abstract/OverlayTypeClass.h>
#include <Abstract/VoxelAnimTypeClass.h>
#include <Animations/AnimTypeClass.h>
#include <Houses/HouseTypeClass.h>
#include <Combat/WarheadTypeClass.h>
#include <SW/SuperWeaponTypeClass.h>
#include <Particles/ParticleTypeClass.h>
#include <Particles/ParticleSystemTypeClass.h>

// ============================================================================
// RulesClass.cpp - Rules class implementation
// ============================================================================
// Standalone engine reconstruction of the RulesClass.
// In the original game, these methods are at specific addresses:
//   Init:              0x6686C0
//   Read_File:         0x668BF0
//   Read_SpecialWeapons: 0x668FB0
//   etc.
// Here we implement the full INI parsing logic for each section.
// ============================================================================

// Static singleton
RulesClass* RulesClass::Instance = nullptr;

// ============================================================================
// Constructor / Destructor
// ============================================================================

RulesClass::RulesClass()
    : DetailMinFrameRateNormal(0)
    , DetailMinFrameRateMovie(0)
    , DetailBufferZoneWidth(0)
    , AmmoCrateDamage(0)
    , LargeVisceroid(nullptr)
    , SmallVisceroid(nullptr)
    , AttackingAircraftSightRange(0)
    , TunnelSpeed(0.0)
    , TiberiumHeal(0.0)
    , SelfHealInfantryFrames(0)
    , SelfHealInfantryAmount(0)
    , SelfHealUnitFrames(0)
    , SelfHealUnitAmount(0)
    , FreeMCV(false)
    , BerzerkAllowed(false)
    , PoseDir(0), DeployDir(0)
    , DropPodPuff(nullptr)
    , WaypointAnimationSpeed(0)
    , BarrelExplode(nullptr)
    , BarrelParticle(nullptr)
    , RadarEventColorSpeed(0.0f), RadarEventMinRadius(0)
    , RadarEventSpeed(0.0f), RadarEventRotationSpeed(0.0f)
    , FlashFrameTime(0), RadarCombatFlashTime(0), MaxWaypointPathLength(0)
    , Wake(nullptr), NukeTakeOff(nullptr)
    , InfantryExplode(nullptr), FlamingInfantry(nullptr)
    , InfantryHeadPop(nullptr), InfantryNuked(nullptr)
    , InfantryVirus(nullptr), InfantryBrute(nullptr)
    , InfantryMutate(nullptr), Behind(nullptr)
    , AITriggerSuccessWeightDelta(0.0), AITriggerFailureWeightDelta(0.0)
    , AITriggerTrackRecordCoefficient(0.0)
    , VeinholeMonsterStrength(0), MaxVeinholeGrowth(0)
    , VeinholeGrowthRate(0), VeinholeShrinkRate(0)
    , VeinAttack(nullptr), VeinDamage(0)
    , MaximumQueuedObjects(0), AircraftFogReveal(0)
    , WoodCrateImg(nullptr), CrateImg(nullptr), WaterCrateImg(nullptr)
    , DigSound(0), CreateUnitSound(0), CreateInfantrySound(0), CreateAircraftSound(0)
    , BaseUnderAttackSound(0), GUIMainButtonSound(0), GUIBuildSound(0)
    , GUITabSound(0), GUIOpenSound(0), GUICloseSound(0)
    , GUIMoveOutSound(0), GUIMoveInSound(0)
    , GUIComboOpenSound(0), GUIComboCloseSound(0), GUICheckboxSound(0)
    , ScoreAnimSound(0), IFVTransformSound(0), PsychicSensorDetectSound(0)
    , BuildingGarrisonedSound(0), BuildingAbandonedSound(0), BuildingRepairedSound(0)
    , CheerSound(0), PlaceBeaconSound(0), DefaultChronoSound(0)
    , StartPlanningModeSound(0), AddPlanningModeCommandSound(0)
    , ExecutePlanSound(0), EndPlanningModeSound(0)
    , CrateMoneySound(0), CrateRevealSound(0), CrateFireSound(0)
    , CrateArmourSound(0), CrateSpeedSound(0), CrateUnitSound(0), CratePromoteSound(0)
    , ImpactWaterSound(0), ImpactLandSound(0), SinkingSound(0)
    , BombTickingSound(0), BombAttachSound(0), YuriMindControlSound(0)
    , ChronoInSound(0), ChronoOutSound(0)
    , SpySatActivationSound(0), SpySatDeactivationSound(0)
    , UpgradeVeteranSound(0), UpgradeEliteSound(0)
    , VoiceIFVRepair(0), SlavesFreeSound(0)
    , SlaveMinerDeploySound(0), SlaveMinerUndeploySound(0)
    , BunkerWallsUpSound(0), BunkerWallsDownSound(0), RepairBridgeSound(0)
    , PsychicDominatorActivateSound(0), GeneticMutatorActivateSound(0)
    , PsychicRevealActivateSound(0), MasterMindOverloadDeathSound(0)
    , AirstrikeAbortSound(0), AirstrikeAttackVoice(0), MindClearedSound(0)
    , EnterGrinderSound(0), LeaveGrinderSound(0)
    , EnterBioReactorSound(0), LeaveBioReactorSound(0)
    , ActivateSound(0), DeactivateSound(0)
    , SpyPlaneCamera(0), LetsDoTheTimeWarpOutAgain(0), LetsDoTheTimeWarpInAgain(0)
    , DiskLaserChargeUp(0), SpyPlaneCameraFrames(0)
    , Dig(nullptr), IonBlast(nullptr), IonBeam(nullptr)
    , WeatherConBoltExplosion(nullptr)
    , DominatorWarhead(nullptr)
    , DominatorFirstAnim(nullptr), DominatorSecondAnim(nullptr)
    , DominatorFireAtPercentage(0), DominatorCaptureRange(0), DominatorDamage(0)
    , MindControlAttackLineFrames(0)
    , DrainMoneyFrameDelay(0), DrainMoneyAmount(0)
    , DrainAnimationType(nullptr), ControlledAnimationType(nullptr)
    , PermaControlledAnimationType(nullptr)
    , ChronoBlast(nullptr), ChronoBlastDest(nullptr), ChronoPlacement(nullptr)
    , ChronoBeam(nullptr), WarpIn(nullptr), WarpOut(nullptr), WarpAway(nullptr)
    , ChronoSparkle1(nullptr)
    , IronCurtainInvokeAnim(nullptr), ForceShieldInvokeAnim(nullptr)
    , WeaponNullifyAnim(nullptr), AtmosphereEntry(nullptr)
    , PrerequisiteProcAlternate(nullptr)
    , GateUp(0), GateDown(0), TurnRate(0), Speed(0)
    , Climb(0.0), CruiseHeight(0), Acceleration(0.0)
    , WobblesPerSecond(0.0), WobbleDeviation(0)
    , IonCannonDamage(0), RailgunDamageRadius(0)
    , PrismType(nullptr)
    , PrismSupportModifier(0), PrismSupportMax(0)
    , PrismSupportDelay(0), PrismSupportDuration(0), PrismSupportHeight(0)
    , ParadropRadius(0)
    , ZoomInFactor(0.0)
    , ConditionRedSparkingProbability(0.0), ConditionYellowSparkingProbability(0.0)
    , TiberiumExplosionDamage(0), TiberiumStrength(0)
    , MinLowPowerProductionSpeed(0.0f), MaxLowPowerProductionSpeed(0.0f)
    , LowPowerPenaltyModifier(0.0f), MultipleFactory(0.0f)
    , MaximumCheerRate(0), TreeFlammability(0.0)
    , MissileSpeedVar(0.0), MissileROTVar(0.0), MissileSafetyAltitude(0)
    , DropPodWeapon(nullptr), DropPodHeight(0), DropPodSpeed(0)
    , DropPodAngle(0.0), ScrollMultiplier(0.0), CrewEscape(0.0)
    , ShakeScreen(0), HoverHeight(0)
    , HoverBob(0.0), HoverBoost(0.0), HoverAcceleration(0.0)
    , HoverBrake(0.0), HoverDampen(0.0), PlacementDelay(0.0)
    , TireVoxelDebris(nullptr), ScrapVoxelDebris(nullptr), BridgeVoxelMax(0)
    , CloakingStages(0), RevealTriggerRadius(0)
    , ShipSinkingWeight(0.0), IceCrackingWeight(0.0), IceBreakingWeight(0.0)
    , CliffBackImpassability(0)
    , VeteranRatio(0.0), VeteranCombat(0.0), VeteranSpeed(0.0)
    , VeteranSight(0.0), VeteranArmor(0.0), VeteranROF(0.0), VeteranCap(0.0)
    , CloakSound(0), SellSound(0)
    , GameClosed(0), IncomingMessage(0), SystemError(0), OptionsChanged(0)
    , GameForming(0), PlayerLeft(0), PlayerJoined(0), MessageCharTyped(0)
    , Construction(0), BuildingDieSound(0), BuildingSlam(0)
    , RadarOn(0), RadarOff(0), MovieOn(0), MovieOff(0), ScoldSound(0)
    , TeslaCharge(0), TeslaZap(0), GenericClick(0), GenericBeep(0)
    , BuildingDamageSound(0), HealCrateSound(0), ChuteSound(0)
    , StopSound(0), GuardSound(0), ScatterSound(0), DeploySound(0), StormSound(0)
    , ShellButtonSlideSound(0)
    , WallBuildSpeedCoefficient(0.0), ChargeToDrainRatio(0.0)
    , TrackedUphill(0.0), TrackedDownhill(0.0)
    , WheeledUphill(0.0), WheeledDownhill(0.0)
    , SpotlightMovementRadius(0), SpotlightLocationRadius(0)
    , SpotlightSpeed(0.0), SpotlightAcceleration(0.0)
    , SpotlightAngle(0.0), SpotlightRadius(0)
    , WindDirection(0), CameraRange(0), FlightLevel(0)
    , ParachuteMaxFallRate(0), NoParachuteMaxFallRate(0), BuildingDrop(0)
    , GDIGateOne(nullptr), GDIGateTwo(nullptr)
    , NodGateOne(nullptr), NodGateTwo(nullptr), WallTower(nullptr)
    , GDIPowerPlant(nullptr), NodRegularPower(nullptr)
    , NodAdvancedPower(nullptr), ThirdPowerPlant(nullptr)
    , GDIWallDefense(0.0), GDIWallDefenseCoefficient(0.0)
    , NodBaseDefenseCoefficient(0.0), GDIBaseDefenseCoefficient(0.0)
    , ComputerBaseDefenseResponse(0), MaximumBaseDefenseValue(0)
    , Smoke(nullptr), Smoke_(nullptr), MoveFlash(nullptr)
    , BombParachute(nullptr), Parachute(nullptr)
    , SmallFire(nullptr), LargeFire(nullptr)
    , Paratrooper(nullptr), EliteFlashTimer(0)
    , ChronoDelay(0), ChronoReinfDelay(0), ChronoDistanceFactor(0)
    , ChronoTrigger(false), ChronoMinimumDelay(0), ChronoRangeMinimum(0)
    , SecretSum(0)
    , AlliedDisguise(nullptr), SovietDisguise(nullptr), ThirdDisguise(nullptr)
    , SpyPowerBlackout(0), SpyMoneyStealPercent(0.0f), AttackCursorOnDisguise(false)
    , AIMinorSuperReadyPercent(0.0f), AISafeDistance(0)
    , HarvesterTooFarDistance(0), ChronoHarvTooFarDistance(0)
    , AIRestrictReplaceTime(0), ThreatPerOccupant(0)
    , ApproachTargetResetMultiplier(0)
    , CampaignMoneyDeltaEasy(0), CampaignMoneyDeltaHard(0)
    , GuardAreaTargetingDelay(0), NormalTargetingDelay(0)
    , AINavalYardAdjacency(0), MaximumBuildingPlacementFailures(0)
    , AICaptureLowMoneyMark(0), AICaptureWoundedMark(0)
    , AISuperDefenseFrames(0), AISuperDefenseDistance(0.0f)
    , PurifierBonus(0.0f), OccupyDamageMultiplier(0.0f)
    , OccupyROFMultiplier(0.0f), OccupyWeaponRange(0)
    , BunkerDamageMultiplier(0), BunkerROFMultiplier(0.0f)
    , BunkerWeaponRangeBonus(0)
    , OpenToppedDamageMultiplier(0.0f), OpenToppedRangeBonus(0)
    , OpenToppedWarpDistance(0), FallingDamageMultiplier(0.0f)
    , CurrentStrengthDamage(false)
    , Technician(nullptr), Engineer(nullptr), Pilot(nullptr)
    , AlliedCrew(nullptr), SovietCrew(nullptr), ThirdCrew(nullptr)
    , FlameDamage(nullptr), FlameDamage2(nullptr), NukeWarhead(nullptr)
    , NukeProjectile(nullptr), NukeDown(nullptr)
    , MutateWarhead(nullptr), MutateExplosionWarhead(nullptr)
    , EMPulseWarhead(nullptr), EMPulseProjectile(nullptr)
    , C4Warhead(nullptr), CrushWarhead(nullptr)
    , V3Warhead(nullptr), DMislWarhead(nullptr)
    , V3EliteWarhead(nullptr), DMislEliteWarhead(nullptr)
    , CMislWarhead(nullptr), CMislEliteWarhead(nullptr), IvanWarhead(nullptr)
    , IvanDamage(0), IvanTimedDelay(0)
    , CanDetonateTimeBomb(false), CanDetonateDeathBomb(false)
    , IvanIconFlickerRate(0), DeathWeapon(nullptr)
    , BOMBCURS_SHP(nullptr), CHRONOSK_SHP(nullptr)
    , IronCurtainDuration(0), PsychicRevealRadius(0)
    , IonCannonWarhead(nullptr), VeinholeTypeClass(nullptr)
    , InfantryBlinkDisguiseTime(0)
    , DefaultLargeGreySmokeSystem(nullptr), DefaultSmallGreySmokeSystem(nullptr)
    , DefaultSparkSystem(nullptr), DefaultLargeRedSmokeSystem(nullptr)
    , DefaultSmallRedSmokeSystem(nullptr), DefaultDebrisSmokeSystem(nullptr)
    , DefaultFireStreamSystem(nullptr), DefaultTestParticleSystem(nullptr)
    , DefaultRepairParticleSystem(nullptr)
    , MyEffectivenessCoefficientDefault(0.0), TargetEffectivenessCoefficientDefault(0.0)
    , TargetSpecialThreatCoefficientDefault(0.0), TargetStrengthCoefficientDefault(0.0)
    , TargetDistanceCoefficientDefault(0.0)
    , DumbMyEffectivenessCoefficient(0.0), DumbTargetEffectivenessCoefficient(0.0)
    , DumbTargetSpecialThreatCoefficient(0.0), DumbTargetStrengthCoefficient(0.0)
    , DumbTargetDistanceCoefficient(0.0)
    , EnemyHouseThreatBonus(0.0), TurboBoost(0.0)
    , AttackInterval(0.0), AttackDelay(0.0), PowerEmergency(0.0)
    , AirstripRatio(0.0), AirstripLimit(0), HelipadRatio(0.0), HelipadLimit(0)
    , TeslaRatio(0.0), TeslaLimit(0), AARatio(0.0), AALimit(0)
    , DefenseRatio(0.0), DefenseLimit(0), WarRatio(0.0), WarLimit(0)
    , BarracksRatio(0.0), BarracksLimit(0), RefineryLimit(0), RefineryRatio(0.0)
    , BaseSizeAdd(0), PowerSurplus(0), InfantryReserve(0), InfantryBaseMult(0)
    , SoloCrateMoney(0), TreeStrength(0), UnitCrateType(nullptr)
    , PatrolScan(0.0), DissolveUnfilledTeamDelay(0)
    , AIAlternateProductionCreditCutoff(0)
    , AIUseTurbineUpgradeProbability(0.0)
    , CloakDelay(0.0), GameSpeedBias(0.0), BaseBias(0.0), ExpSpread(0.0)
    , FireSupress(0), MaxIQLevels(0), SuperWeapons(0), Production(0)
    , GuardArea(0), RepairSell(0), AutoCrush(0), Scatter(0)
    , ContentScan(0), Aircraft(0), Harvester(0), SellBack(0), AIBaseSpacing(0)
    , CrateMinimum(0), CrateMaximum(0), unknown_int_1478(0)
    , DropZoneAnim(nullptr)
    , MinMoney(0), Money(0), MaxMoney(0), MoneyIncrement(0)
    , MinUnitCount(0), UnitCount(0), MaxUnitCount(0)
    , TechLevel(0), GameSpeed(0), AIDifficultyStruct(0), AIPlayers(0)
    , BridgeDestruction(false), ShadowGrow(false), Shroud(false), Bases(false)
    , TiberiumGrows(false), Crates(false), CaptureTheFlag(false)
    , HarvesterTruce(false), MultiEngineer(false), AlliesAllowed(false)
    , ShortGame(false), FogOfWar(false), MCVRedeploys(false)
    , SuperWeaponsAllowed(false), BuildOffAlly(false), AllyChangeAllowed(false)
    , DropZoneRadius(0)
    , MessageDelay(0.0), SavourDelay(0.0), Players(0)
    , BaseDefenseDelay(0.0), SuspendPriority(0), SuspendDelay(0.0)
    , SurvivorRate(0.0)
    , AlliedSurvivorDivisor(0), SovietSurvivorDivisor(0), ThirdSurvivorDivisor(0)
    , ReloadRate(0.0), AutocreateTime(0.0), BuildupTime(0.0)
    , HarvesterLoadRate(0), HarvesterDumpRate(0.0), AtomDamage(0)
    , GrowthRate(0.0), ShroudRate(0.0), FogRate(0.0)
    , IceGrowthRate(0.0), VeinGrowthRate(0.0), IceSolidifyFrameTime(0)
    , AmbientChangeRate(0.0), AmbientChangeStep(0.0)
    , CrateRegen(0.0), TimerWarning(0.0), TiberiumTransmogrify(0)
    , unknown_double_1690(0.0), unknown_double_1698(0.0), unknown_double_16A0(0.0)
    , SpeakDelay(0.0), DamageDelay(0.0), Gravity(0)
    , LeptonsPerSightIncrease(0), Incoming(0)
    , MinDamage(0), MaxDamage(0), RepairStep(0), RepairPercent(0.0)
    , IRepairStep(0), RepairRate(0.0), URepairRate(0.0), IRepairRate(0.0)
    , unknown_double_16F8(0.0)
    , ConditionYellow(0.0), ConditionRed(0.0), IdleActionFrequency(0.0)
    , CloseEnough(0), Stray(0), RelaxedStray(0), GuardModeStray(0), Crush(0)
    , CrateRadius(0), HomingScatter(0), BallisticScatter(0)
    , RefundPercent(0.0), BridgeStrength(0), BuildSpeed(0.0)
    , C4Delay(0.0), CreditReserve(0), PathDelay(0.0), BlockagePathDelay(0)
    , MovieTime(0.0)
    , TiberiumShortScan(0), TiberiumLongScan(0)
    , SlaveMinerShortScan(0), SlaveMinerSlaveScan(0), SlaveMinerLongScan(0)
    , SlaveMinerScanCorrection(0), SlaveMinerKickFrameDelay(0)
    , LightningDeferment(0), LightningDamage(0), LightningStormDuration(0)
    , LightningHitDelay(0), LightningScatterDelay(0)
    , LightningCellSpread(0), LightningSeparation(0)
    , LightningPrintText(false), LightningWarhead(nullptr)
    , ForceShieldRadius(0), ForceShieldDuration(0)
    , ForceShieldBlackoutDuration(0), ForceShieldPlayFadeSoundTime(0)
    , MutateExplosion(false)
    , CollapseChance(0), WeedCapacity(0)
    , ExtraUnitLight(0.0f), ExtraInfantryLight(0.0f), ExtraAircraftLight(0.0f)
    , Paranoid(false), CurleyShuffle(false), BlendedFog(false)
    , CompEasyBonus(false), FineDiffControl(false), TiberiumExplosive(false)
    , EnemyHealth(false), AllyReveal(false), SeparateAircraft(false)
    , TreeTargeting(false), NamedCivilians(false)
    , PlayerAutoCrush(false), PlayerReturnFire(false), PlayerScatter(false)
    , RevealByHeight(false), AllowShroudedSubteranneanMoves(false)
    , ShroudGrow(false), NodAIBuildsWalls(false), AIBuildsWalls(false)
    , UseMinDefenseRule(false)
    , EMPulseSparkles(nullptr)
    , EngineerCaptureLevel(0.0f), EngineerCaptureLevel_(0.0f), TalkBubbleTime(0.0f)
    , RadDurationMultiple(0), RadApplicationDelay(0), RadLevelMax(0)
    , RadLevelDelay(0), RadLightDelay(0)
    , RadLevelFactor(0.0), RadLightFactor(0.0), RadTintFactor(0.0)
    , RadSiteWarhead(nullptr)
    , ElevationIncrement(0), ElevationIncrementBonus(0.0), ElevationBonusCap(0.0)
    , AlliedWallTransparency(false), WallPenetratorThreshold(0.0)
    , OreTwinkleChance(0), OreTwinkle(nullptr)
    , LaserTargetColor(0), IronCurtainColor(0), BerserkColor(0), ForceShieldColor(0)
    , DirectRockingCoefficient(0.0f), FallBackCoefficient(0.0f)
{
    // Initialize ColorAdd
    for (int32 i = 0; i < 0x10; ++i) {
        ColorAdd[i] = ColorStruct();
    }

    // Initialize SilverCrate
    SilverCrate.Type = Powerup::Money;
    SilverCrate.Amount = 2000;
    SilverCrate.Chance = 100;

    // Initialize WoodCrate
    WoodCrate.Type = Powerup::Money;
    WoodCrate.Amount = 2000;
    WoodCrate.Chance = 100;

    // Initialize WaterCrate
    WaterCrate.Type = Powerup::Money;
    WaterCrate.Amount = 2000;
    WaterCrate.Chance = 100;
}

RulesClass::~RulesClass()
{
    // Cleanup is handled by destructors of member vectors
}

// ============================================================================
// Init - First-time initialization from INI
// ============================================================================

void RulesClass::Init(CCINIClass* pINI)
{
    if (!pINI) return;

    Read_File(pINI);
}

void RulesClass::Read_File(CCINIClass* pINI)
{
    if (!pINI) return;

    Read_SpecialWeapons(pINI);
    Read_AudioVisual(pINI);
    Read_CrateRules(pINI);
    Read_CombatDamage(pINI);
    Read_Radiation(pINI);
    Read_ElevationModel(pINI);
    Read_WallModel(pINI);
    Read_Difficulty(pINI);
    Read_Colors(pINI);
    Read_ColorAdd(pINI);
    Read_General(pINI);
    Read_MultiplayerDialogSettings(pINI);
    Read_Maximums(pINI);
    Read_InfantryTypes(pINI);
    Read_Countries(pINI);
    Read_VehicleTypes(pINI);
    Read_AircraftTypes(pINI);
    Read_Sides(pINI);
    Read_SuperWeaponTypes(pINI);
    Read_BuildingTypes(pINI);
    Read_TerrainTypes(pINI);
    Read_SmudgeTypes(pINI);
    Read_OverlayTypes(pINI);
    Read_Animations(pINI);
    Read_VoxelAnims(pINI);
    Read_Warheads(pINI);
    Read_Particles(pINI);
    Read_ParticleSystems(pINI);
    Read_AI(pINI);
    Read_Powerups(pINI);
    Read_LandCharacteristics(pINI);
    Read_IQ(pINI);
    Read_JumpjetControls(pINI);
    Read_Difficulties(pINI);
    Read_Movies(pINI);
    Read_AdvancedCommandBar(pINI);
    Read_HarvesterRules(pINI);
}

// ============================================================================
// Read_SpecialWeapons - [SpecialWeapons] section
// ============================================================================

void RulesClass::Read_SpecialWeapons(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "SpecialWeapons";

    // These are type references - they would be resolved by the type system
    // In the standalone engine, we store the INI keys for later resolution
}

// ============================================================================
// Read_AudioVisual - [AudioVisual] section
// ============================================================================

void RulesClass::Read_AudioVisual(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "AudioVisual";

    // Sound mappings
    DigSound                    = pINI->ReadInteger(section, "DigSound", DigSound);
    CreateUnitSound             = pINI->ReadInteger(section, "CreateUnitSound", CreateUnitSound);
    CreateInfantrySound         = pINI->ReadInteger(section, "CreateInfantrySound", CreateInfantrySound);
    CreateAircraftSound         = pINI->ReadInteger(section, "CreateAircraftSound", CreateAircraftSound);
    BaseUnderAttackSound        = pINI->ReadInteger(section, "BaseUnderAttackSound", BaseUnderAttackSound);
    GUIMainButtonSound          = pINI->ReadInteger(section, "GUIMainButtonSound", GUIMainButtonSound);
    GUIBuildSound               = pINI->ReadInteger(section, "GUIBuildSound", GUIBuildSound);
    GUITabSound                 = pINI->ReadInteger(section, "GUITabSound", GUITabSound);
    GUIOpenSound                = pINI->ReadInteger(section, "GUIOpenSound", GUIOpenSound);
    GUICloseSound               = pINI->ReadInteger(section, "GUICloseSound", GUICloseSound);
    GUIMoveOutSound             = pINI->ReadInteger(section, "GUIMoveOutSound", GUIMoveOutSound);
    GUIMoveInSound              = pINI->ReadInteger(section, "GUIMoveInSound", GUIMoveInSound);
    GUIComboOpenSound           = pINI->ReadInteger(section, "GUIComboOpenSound", GUIComboOpenSound);
    GUIComboCloseSound          = pINI->ReadInteger(section, "GUIComboCloseSound", GUIComboCloseSound);
    GUICheckboxSound            = pINI->ReadInteger(section, "GUICheckboxSound", GUICheckboxSound);
    ScoreAnimSound              = pINI->ReadInteger(section, "ScoreAnimSound", ScoreAnimSound);
    IFVTransformSound           = pINI->ReadInteger(section, "IFVTransformSound", IFVTransformSound);
    PsychicSensorDetectSound    = pINI->ReadInteger(section, "PsychicSensorDetectSound", PsychicSensorDetectSound);
    BuildingGarrisonedSound     = pINI->ReadInteger(section, "BuildingGarrisonedSound", BuildingGarrisonedSound);
    BuildingAbandonedSound      = pINI->ReadInteger(section, "BuildingAbandonedSound", BuildingAbandonedSound);
    BuildingRepairedSound       = pINI->ReadInteger(section, "BuildingRepairedSound", BuildingRepairedSound);
    CheerSound                  = pINI->ReadInteger(section, "CheerSound", CheerSound);
    PlaceBeaconSound            = pINI->ReadInteger(section, "PlaceBeaconSound", PlaceBeaconSound);
    DefaultChronoSound          = pINI->ReadInteger(section, "DefaultChronoSound", DefaultChronoSound);
    StartPlanningModeSound      = pINI->ReadInteger(section, "StartPlanningModeSound", StartPlanningModeSound);
    AddPlanningModeCommandSound = pINI->ReadInteger(section, "AddPlanningModeCommandSound", AddPlanningModeCommandSound);
    ExecutePlanSound            = pINI->ReadInteger(section, "ExecutePlanSound", ExecutePlanSound);
    EndPlanningModeSound        = pINI->ReadInteger(section, "EndPlanningModeSound", EndPlanningModeSound);
    CrateMoneySound             = pINI->ReadInteger(section, "CrateMoneySound", CrateMoneySound);
    CrateRevealSound            = pINI->ReadInteger(section, "CrateRevealSound", CrateRevealSound);
    CrateFireSound              = pINI->ReadInteger(section, "CrateFireSound", CrateFireSound);
    CrateArmourSound            = pINI->ReadInteger(section, "CrateArmourSound", CrateArmourSound);
    CrateSpeedSound             = pINI->ReadInteger(section, "CrateSpeedSound", CrateSpeedSound);
    CrateUnitSound              = pINI->ReadInteger(section, "CrateUnitSound", CrateUnitSound);
    CratePromoteSound           = pINI->ReadInteger(section, "CratePromoteSound", CratePromoteSound);
    ImpactWaterSound            = pINI->ReadInteger(section, "ImpactWaterSound", ImpactWaterSound);
    ImpactLandSound             = pINI->ReadInteger(section, "ImpactLandSound", ImpactLandSound);
    SinkingSound                = pINI->ReadInteger(section, "SinkingSound", SinkingSound);
    BombTickingSound            = pINI->ReadInteger(section, "BombTickingSound", BombTickingSound);
    BombAttachSound             = pINI->ReadInteger(section, "BombAttachSound", BombAttachSound);
    YuriMindControlSound        = pINI->ReadInteger(section, "YuriMindControlSound", YuriMindControlSound);
    ChronoInSound               = pINI->ReadInteger(section, "ChronoInSound", ChronoInSound);
    ChronoOutSound              = pINI->ReadInteger(section, "ChronoOutSound", ChronoOutSound);
    SpySatActivationSound       = pINI->ReadInteger(section, "SpySatActivationSound", SpySatActivationSound);
    SpySatDeactivationSound     = pINI->ReadInteger(section, "SpySatDeactivationSound", SpySatDeactivationSound);
    UpgradeVeteranSound         = pINI->ReadInteger(section, "UpgradeVeteranSound", UpgradeVeteranSound);
    UpgradeEliteSound           = pINI->ReadInteger(section, "UpgradeEliteSound", UpgradeEliteSound);
    VoiceIFVRepair              = pINI->ReadInteger(section, "VoiceIFVRepair", VoiceIFVRepair);
    SlavesFreeSound             = pINI->ReadInteger(section, "SlavesFreeSound", SlavesFreeSound);
    SlaveMinerDeploySound       = pINI->ReadInteger(section, "SlaveMinerDeploySound", SlaveMinerDeploySound);
    SlaveMinerUndeploySound     = pINI->ReadInteger(section, "SlaveMinerUndeploySound", SlaveMinerUndeploySound);
    BunkerWallsUpSound          = pINI->ReadInteger(section, "BunkerWallsUpSound", BunkerWallsUpSound);
    BunkerWallsDownSound        = pINI->ReadInteger(section, "BunkerWallsDownSound", BunkerWallsDownSound);
    RepairBridgeSound           = pINI->ReadInteger(section, "RepairBridgeSound", RepairBridgeSound);
    PsychicDominatorActivateSound = pINI->ReadInteger(section, "PsychicDominatorActivateSound", PsychicDominatorActivateSound);
    GeneticMutatorActivateSound = pINI->ReadInteger(section, "GeneticMutatorActivateSound", GeneticMutatorActivateSound);
    PsychicRevealActivateSound  = pINI->ReadInteger(section, "PsychicRevealActivateSound", PsychicRevealActivateSound);
    MasterMindOverloadDeathSound = pINI->ReadInteger(section, "MasterMindOverloadDeathSound", MasterMindOverloadDeathSound);
    AirstrikeAbortSound         = pINI->ReadInteger(section, "AirstrikeAbortSound", AirstrikeAbortSound);
    AirstrikeAttackVoice        = pINI->ReadInteger(section, "AirstrikeAttackVoice", AirstrikeAttackVoice);
    MindClearedSound            = pINI->ReadInteger(section, "MindClearedSound", MindClearedSound);
    EnterGrinderSound           = pINI->ReadInteger(section, "EnterGrinderSound", EnterGrinderSound);
    LeaveGrinderSound           = pINI->ReadInteger(section, "LeaveGrinderSound", LeaveGrinderSound);
    EnterBioReactorSound        = pINI->ReadInteger(section, "EnterBioReactorSound", EnterBioReactorSound);
    LeaveBioReactorSound        = pINI->ReadInteger(section, "LeaveBioReactorSound", LeaveBioReactorSound);
    ActivateSound               = pINI->ReadInteger(section, "ActivateSound", ActivateSound);
    DeactivateSound             = pINI->ReadInteger(section, "DeactivateSound", DeactivateSound);
    SpyPlaneCamera              = pINI->ReadInteger(section, "SpyPlaneCamera", SpyPlaneCamera);
    LetsDoTheTimeWarpOutAgain   = pINI->ReadInteger(section, "LetsDoTheTimeWarpOutAgain", LetsDoTheTimeWarpOutAgain);
    LetsDoTheTimeWarpInAgain    = pINI->ReadInteger(section, "LetsDoTheTimeWarpInAgain", LetsDoTheTimeWarpInAgain);
    DiskLaserChargeUp           = pINI->ReadInteger(section, "DiskLaserChargeUp", DiskLaserChargeUp);
    SpyPlaneCameraFrames        = pINI->ReadInteger(section, "SpyPlaneCameraFrames", SpyPlaneCameraFrames);

    // Visual properties
    RadarEventColorSpeed        = static_cast<float>(pINI->ReadDouble(section, "RadarEventColorSpeed", RadarEventColorSpeed));
    RadarEventMinRadius         = pINI->ReadInteger(section, "RadarEventMinRadius", RadarEventMinRadius);
    RadarEventSpeed             = static_cast<float>(pINI->ReadDouble(section, "RadarEventSpeed", RadarEventSpeed));
    RadarEventRotationSpeed     = static_cast<float>(pINI->ReadDouble(section, "RadarEventRotationSpeed", RadarEventRotationSpeed));
    FlashFrameTime              = pINI->ReadInteger(section, "FlashFrameTime", FlashFrameTime);
    RadarCombatFlashTime        = pINI->ReadInteger(section, "RadarCombatFlashTime", RadarCombatFlashTime);
    MaxWaypointPathLength       = pINI->ReadInteger(section, "MaxWaypointPathLength", MaxWaypointPathLength);

    // Shell sounds
    BuildingDieSound            = pINI->ReadInteger(section, "BuildingDieSound", BuildingDieSound);
    BuildingSlam                = pINI->ReadInteger(section, "BuildingSlam", BuildingSlam);
    RadarOn                     = pINI->ReadInteger(section, "RadarOn", RadarOn);
    RadarOff                    = pINI->ReadInteger(section, "RadarOff", RadarOff);
    MovieOn                     = pINI->ReadInteger(section, "MovieOn", MovieOn);
    MovieOff                    = pINI->ReadInteger(section, "MovieOff", MovieOff);
    ScoldSound                  = pINI->ReadInteger(section, "ScoldSound", ScoldSound);
    TeslaCharge                 = pINI->ReadInteger(section, "TeslaCharge", TeslaCharge);
    TeslaZap                    = pINI->ReadInteger(section, "TeslaZap", TeslaZap);
    GenericClick                = pINI->ReadInteger(section, "GenericClick", GenericClick);
    GenericBeep                 = pINI->ReadInteger(section, "GenericBeep", GenericBeep);
    BuildingDamageSound         = pINI->ReadInteger(section, "BuildingDamageSound", BuildingDamageSound);
    HealCrateSound              = pINI->ReadInteger(section, "HealCrateSound", HealCrateSound);
    ChuteSound                  = pINI->ReadInteger(section, "ChuteSound", ChuteSound);
    StopSound                   = pINI->ReadInteger(section, "StopSound", StopSound);
    GuardSound                  = pINI->ReadInteger(section, "GuardSound", GuardSound);
    ScatterSound                = pINI->ReadInteger(section, "ScatterSound", ScatterSound);
    DeploySound                 = pINI->ReadInteger(section, "DeploySound", DeploySound);
    StormSound                  = pINI->ReadInteger(section, "StormSound", StormSound);
    ShellButtonSlideSound       = pINI->ReadInteger(section, "ShellButtonSlideSound", ShellButtonSlideSound);
    CloakSound                  = pINI->ReadInteger(section, "CloakSound", CloakSound);
    SellSound                   = pINI->ReadInteger(section, "SellSound", SellSound);

    // Multiplayer sounds
    GameClosed                  = pINI->ReadInteger(section, "GameClosed", GameClosed);
    IncomingMessage             = pINI->ReadInteger(section, "IncomingMessage", IncomingMessage);
    SystemError                 = pINI->ReadInteger(section, "SystemError", SystemError);
    OptionsChanged              = pINI->ReadInteger(section, "OptionsChanged", OptionsChanged);
    GameForming                 = pINI->ReadInteger(section, "GameForming", GameForming);
    PlayerLeft                  = pINI->ReadInteger(section, "PlayerLeft", PlayerLeft);
    PlayerJoined                = pINI->ReadInteger(section, "PlayerJoined", PlayerJoined);
    MessageCharTyped            = pINI->ReadInteger(section, "MessageCharTyped", MessageCharTyped);
    Construction                = pINI->ReadInteger(section, "Construction", Construction);
}

// ============================================================================
// Read_CrateRules - [CrateRules] section
// ============================================================================

void RulesClass::Read_CrateRules(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "CrateRules";

    CrateMinimum    = pINI->ReadInteger(section, "CrateMinimum", CrateMinimum);
    CrateMaximum    = pINI->ReadInteger(section, "CrateMaximum", CrateMaximum);
    CrateRadius     = pINI->ReadInteger(section, "CrateRadius", CrateRadius);
    SoloCrateMoney  = pINI->ReadInteger(section, "SoloCrateMoney", SoloCrateMoney);
    AmmoCrateDamage = pINI->ReadInteger(section, "AmmoCrateDamage", AmmoCrateDamage);
    CrateRegen      = pINI->ReadDouble(section, "CrateRegen", CrateRegen);
}

// ============================================================================
// Read_CombatDamage - [CombatDamage] section
// ============================================================================

void RulesClass::Read_CombatDamage(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "CombatDamage";

    MinDamage            = pINI->ReadInteger(section, "MinDamage", MinDamage);
    MaxDamage            = pINI->ReadInteger(section, "MaxDamage", MaxDamage);
    RepairStep           = pINI->ReadInteger(section, "RepairStep", RepairStep);
    RepairPercent        = pINI->ReadDouble(section, "RepairPercent", RepairPercent);
    IRepairStep          = pINI->ReadInteger(section, "IRepairStep", IRepairStep);
    RepairRate           = pINI->ReadDouble(section, "RepairRate", RepairRate);
    URepairRate          = pINI->ReadDouble(section, "URepairRate", URepairRate);
    IRepairRate          = pINI->ReadDouble(section, "IRepairRate", IRepairRate);
    ConditionYellow      = pINI->ReadDouble(section, "ConditionYellow", ConditionYellow);
    ConditionRed         = pINI->ReadDouble(section, "ConditionRed", ConditionRed);
    ConditionRedSparkingProbability = pINI->ReadDouble(section, "ConditionRedSparkingProbability", ConditionRedSparkingProbability);
    ConditionYellowSparkingProbability = pINI->ReadDouble(section, "ConditionYellowSparkingProbability", ConditionYellowSparkingProbability);
    IdleActionFrequency  = pINI->ReadDouble(section, "IdleActionFrequency", IdleActionFrequency);
    CloseEnough          = pINI->ReadInteger(section, "CloseEnough", CloseEnough);
    Stray                = pINI->ReadInteger(section, "Stray", Stray);
    RelaxedStray         = pINI->ReadInteger(section, "RelaxedStray", RelaxedStray);
    GuardModeStray       = pINI->ReadInteger(section, "GuardModeStray", GuardModeStray);
    Crush                = pINI->ReadInteger(section, "Crush", Crush);
    FireSupress          = pINI->ReadInteger(section, "FireSupress", FireSupress);
    TiberiumExplosionDamage = pINI->ReadInteger(section, "TiberiumExplosionDamage", TiberiumExplosionDamage);
    TiberiumStrength     = pINI->ReadInteger(section, "TiberiumStrength", TiberiumStrength);
    AtomDamage           = pINI->ReadInteger(section, "AtomDamage", AtomDamage);
    HomingScatter        = pINI->ReadInteger(section, "HomingScatter", HomingScatter);
    BallisticScatter     = pINI->ReadInteger(section, "BallisticScatter", BallisticScatter);
    CollapseChance       = pINI->ReadInteger(section, "CollapseChance", CollapseChance);
    BridgeStrength       = pINI->ReadInteger(section, "BridgeStrength", BridgeStrength);
}

// ============================================================================
// Read_Radiation - [Radiation] section
// ============================================================================

void RulesClass::Read_Radiation(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "Radiation";

    RadDurationMultiple    = pINI->ReadInteger(section, "RadDurationMultiple", RadDurationMultiple);
    RadApplicationDelay    = pINI->ReadInteger(section, "RadApplicationDelay", RadApplicationDelay);
    RadLevelMax            = pINI->ReadInteger(section, "RadLevelMax", RadLevelMax);
    RadLevelDelay          = pINI->ReadInteger(section, "RadLevelDelay", RadLevelDelay);
    RadLightDelay          = pINI->ReadInteger(section, "RadLightDelay", RadLightDelay);
    RadLevelFactor         = pINI->ReadDouble(section, "RadLevelFactor", RadLevelFactor);
    RadLightFactor         = pINI->ReadDouble(section, "RadLightFactor", RadLightFactor);
    RadTintFactor          = pINI->ReadDouble(section, "RadTintFactor", RadTintFactor);
}

// ============================================================================
// Read_ElevationModel - [ElevationModel] section
// ============================================================================

void RulesClass::Read_ElevationModel(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "ElevationModel";

    ElevationIncrement       = pINI->ReadInteger(section, "ElevationIncrement", ElevationIncrement);
    ElevationIncrementBonus  = pINI->ReadDouble(section, "ElevationIncrementBonus", ElevationIncrementBonus);
    ElevationBonusCap        = pINI->ReadDouble(section, "ElevationBonusCap", ElevationBonusCap);
    AlliedWallTransparency   = pINI->ReadBool(section, "AlliedWallTransparency", AlliedWallTransparency);
    WallPenetratorThreshold  = pINI->ReadDouble(section, "WallPenetratorThreshold", WallPenetratorThreshold);
}

// ============================================================================
// Read_WallModel - [WallModel] section
// ============================================================================

void RulesClass::Read_WallModel(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "WallModel";

    WallBuildSpeedCoefficient = pINI->ReadDouble(section, "WallBuildSpeedCoefficient", WallBuildSpeedCoefficient);
}

// ============================================================================
// Read_Difficulty - [Difficulty] section
// ============================================================================

void RulesClass::Read_Difficulty(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "Difficulty";

    Easy.Firepower     = pINI->ReadDouble(section, "EasyFirepower", Easy.Firepower);
    Easy.GroundSpeed   = pINI->ReadDouble(section, "EasyGroundSpeed", Easy.GroundSpeed);
    Easy.AirSpeed      = pINI->ReadDouble(section, "EasyAirSpeed", Easy.AirSpeed);
    Easy.Armor         = pINI->ReadDouble(section, "EasyArmor", Easy.Armor);
    Easy.ROF           = pINI->ReadDouble(section, "EasyROF", Easy.ROF);
    Easy.Cost          = pINI->ReadDouble(section, "EasyCost", Easy.Cost);
    Easy.BuildTime     = pINI->ReadDouble(section, "EasyBuildTime", Easy.BuildTime);
    Easy.RepairDelay   = pINI->ReadDouble(section, "EasyRepairDelay", Easy.RepairDelay);
    Easy.BuildDelay    = pINI->ReadDouble(section, "EasyBuildDelay", Easy.BuildDelay);

    Normal.Firepower   = pINI->ReadDouble(section, "NormalFirepower", Normal.Firepower);
    Normal.GroundSpeed = pINI->ReadDouble(section, "NormalGroundSpeed", Normal.GroundSpeed);
    Normal.AirSpeed    = pINI->ReadDouble(section, "NormalAirSpeed", Normal.AirSpeed);
    Normal.Armor       = pINI->ReadDouble(section, "NormalArmor", Normal.Armor);
    Normal.ROF         = pINI->ReadDouble(section, "NormalROF", Normal.ROF);
    Normal.Cost        = pINI->ReadDouble(section, "NormalCost", Normal.Cost);
    Normal.BuildTime   = pINI->ReadDouble(section, "NormalBuildTime", Normal.BuildTime);
    Normal.RepairDelay = pINI->ReadDouble(section, "NormalRepairDelay", Normal.RepairDelay);
    Normal.BuildDelay  = pINI->ReadDouble(section, "NormalBuildDelay", Normal.BuildDelay);

    Difficult.Firepower   = pINI->ReadDouble(section, "DifficultFirepower", Difficult.Firepower);
    Difficult.GroundSpeed = pINI->ReadDouble(section, "DifficultGroundSpeed", Difficult.GroundSpeed);
    Difficult.AirSpeed    = pINI->ReadDouble(section, "DifficultAirSpeed", Difficult.AirSpeed);
    Difficult.Armor       = pINI->ReadDouble(section, "DifficultArmor", Difficult.Armor);
    Difficult.ROF         = pINI->ReadDouble(section, "DifficultROF", Difficult.ROF);
    Difficult.Cost        = pINI->ReadDouble(section, "DifficultCost", Difficult.Cost);
    Difficult.BuildTime   = pINI->ReadDouble(section, "DifficultBuildTime", Difficult.BuildTime);
    Difficult.RepairDelay = pINI->ReadDouble(section, "DifficultRepairDelay", Difficult.RepairDelay);
    Difficult.BuildDelay  = pINI->ReadDouble(section, "DifficultBuildDelay", Difficult.BuildDelay);
}

// ============================================================================
// Read_Colors - [Colors] section
// ============================================================================

void RulesClass::Read_Colors(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "Colors";

    // Read RGB color values
    uint8 rgb[3];
    pINI->Read3Bytes(rgb, section, "LocalRadarColor", nullptr);
    LocalRadarColor.R = rgb[0];
    LocalRadarColor.G = rgb[1];
    LocalRadarColor.B = rgb[2];

    pINI->Read3Bytes(rgb, section, "LineTrailColorOverride", nullptr);
    LineTrailColorOverride.R = rgb[0];
    LineTrailColorOverride.G = rgb[1];
    LineTrailColorOverride.B = rgb[2];

    pINI->Read3Bytes(rgb, section, "ChronoBeamColor", nullptr);
    ChronoBeamColor.R = rgb[0];
    ChronoBeamColor.G = rgb[1];
    ChronoBeamColor.B = rgb[2];

    pINI->Read3Bytes(rgb, section, "MagnaBeamColor", nullptr);
    MagnaBeamColor.R = rgb[0];
    MagnaBeamColor.G = rgb[1];
    MagnaBeamColor.B = rgb[2];

    OreTwinkleChance = pINI->ReadInteger(section, "OreTwinkleChance", OreTwinkleChance);
    LaserTargetColor = pINI->ReadInteger(section, "LaserTargetColor", LaserTargetColor);
    IronCurtainColor = pINI->ReadInteger(section, "IronCurtainColor", IronCurtainColor);
    BerserkColor     = pINI->ReadInteger(section, "BerserkColor", BerserkColor);
    ForceShieldColor = pINI->ReadInteger(section, "ForceShieldColor", ForceShieldColor);
}

// ============================================================================
// Read_ColorAdd - [ColorAdd] section
// ============================================================================

void RulesClass::Read_ColorAdd(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "ColorAdd";

    for (int32 i = 0; i < 0x10; ++i) {
        char key[32];
        int32 len = 0;
        const char* prefix = "Color";
        while (prefix[len]) { key[len] = prefix[len]; ++len; }
        if (i >= 10) {
            key[len++] = '0' + (i / 10);
        }
        key[len++] = '0' + (i % 10);
        key[len] = '\0';

        uint8 rgb[3];
        pINI->Read3Bytes(rgb, section, key, nullptr);
        ColorAdd[i].R = rgb[0];
        ColorAdd[i].G = rgb[1];
        ColorAdd[i].B = rgb[2];
    }
}

// ============================================================================
// Read_General - [General] section
// ============================================================================

void RulesClass::Read_General(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "General";

    DetailMinFrameRateNormal       = pINI->ReadInteger(section, "DetailMinFrameRateNormal", DetailMinFrameRateNormal);
    DetailMinFrameRateMovie        = pINI->ReadInteger(section, "DetailMinFrameRateMovie", DetailMinFrameRateMovie);
    DetailBufferZoneWidth          = pINI->ReadInteger(section, "DetailBufferZoneWidth", DetailBufferZoneWidth);
    AttackingAircraftSightRange    = pINI->ReadInteger(section, "AttackingAircraftSightRange", AttackingAircraftSightRange);
    TunnelSpeed                    = pINI->ReadDouble(section, "TunnelSpeed", TunnelSpeed);
    TiberiumHeal                   = pINI->ReadDouble(section, "TiberiumHeal", TiberiumHeal);
    SelfHealInfantryFrames         = pINI->ReadInteger(section, "SelfHealInfantryFrames", SelfHealInfantryFrames);
    SelfHealInfantryAmount         = pINI->ReadInteger(section, "SelfHealInfantryAmount", SelfHealInfantryAmount);
    SelfHealUnitFrames             = pINI->ReadInteger(section, "SelfHealUnitFrames", SelfHealUnitFrames);
    SelfHealUnitAmount             = pINI->ReadInteger(section, "SelfHealUnitAmount", SelfHealUnitAmount);
    FreeMCV                        = pINI->ReadBool(section, "FreeMCV", FreeMCV);
    BerzerkAllowed                 = pINI->ReadBool(section, "BerzerkAllowed", BerzerkAllowed);
    PoseDir                        = pINI->ReadInteger(section, "PoseDir", PoseDir);
    DeployDir                      = pINI->ReadInteger(section, "DeployDir", DeployDir);
    WaypointAnimationSpeed         = pINI->ReadInteger(section, "WaypointAnimationSpeed", WaypointAnimationSpeed);
    MaximumQueuedObjects           = pINI->ReadInteger(section, "MaximumQueuedObjects", MaximumQueuedObjects);
    AircraftFogReveal              = pINI->ReadInteger(section, "AircraftFogReveal", AircraftFogReveal);

    ZoomInFactor                   = pINI->ReadDouble(section, "ZoomInFactor", ZoomInFactor);
    MinLowPowerProductionSpeed     = static_cast<float>(pINI->ReadDouble(section, "MinLowPowerProductionSpeed", MinLowPowerProductionSpeed));
    MaxLowPowerProductionSpeed     = static_cast<float>(pINI->ReadDouble(section, "MaxLowPowerProductionSpeed", MaxLowPowerProductionSpeed));
    LowPowerPenaltyModifier        = static_cast<float>(pINI->ReadDouble(section, "LowPowerPenaltyModifier", LowPowerPenaltyModifier));
    MultipleFactory                = static_cast<float>(pINI->ReadDouble(section, "MultipleFactory", MultipleFactory));
    MaximumCheerRate               = pINI->ReadInteger(section, "MaximumCheerRate", MaximumCheerRate);
    TreeFlammability               = pINI->ReadDouble(section, "TreeFlammability", TreeFlammability);
    MissileSpeedVar                = pINI->ReadDouble(section, "MissileSpeedVar", MissileSpeedVar);
    MissileROTVar                  = pINI->ReadDouble(section, "MissileROTVar", MissileROTVar);
    MissileSafetyAltitude          = pINI->ReadInteger(section, "MissileSafetyAltitude", MissileSafetyAltitude);
    DropPodHeight                  = pINI->ReadInteger(section, "DropPodHeight", DropPodHeight);
    DropPodSpeed                   = pINI->ReadInteger(section, "DropPodSpeed", DropPodSpeed);
    DropPodAngle                   = pINI->ReadDouble(section, "DropPodAngle", DropPodAngle);
    ScrollMultiplier               = pINI->ReadDouble(section, "ScrollMultiplier", ScrollMultiplier);
    CrewEscape                     = pINI->ReadDouble(section, "CrewEscape", CrewEscape);
    ShakeScreen                    = pINI->ReadInteger(section, "ShakeScreen", ShakeScreen);
    HoverHeight                    = pINI->ReadInteger(section, "HoverHeight", HoverHeight);
    HoverBob                       = pINI->ReadDouble(section, "HoverBob", HoverBob);
    HoverBoost                     = pINI->ReadDouble(section, "HoverBoost", HoverBoost);
    HoverAcceleration              = pINI->ReadDouble(section, "HoverAcceleration", HoverAcceleration);
    HoverBrake                     = pINI->ReadDouble(section, "HoverBrake", HoverBrake);
    HoverDampen                    = pINI->ReadDouble(section, "HoverDampen", HoverDampen);
    PlacementDelay                 = pINI->ReadDouble(section, "PlacementDelay", PlacementDelay);
    BridgeVoxelMax                 = pINI->ReadInteger(section, "BridgeVoxelMax", BridgeVoxelMax);
    CloakingStages                 = pINI->ReadInteger(section, "CloakingStages", CloakingStages);
    RevealTriggerRadius            = pINI->ReadInteger(section, "RevealTriggerRadius", RevealTriggerRadius);
    ShipSinkingWeight              = pINI->ReadDouble(section, "ShipSinkingWeight", ShipSinkingWeight);
    IceCrackingWeight              = pINI->ReadDouble(section, "IceCrackingWeight", IceCrackingWeight);
    IceBreakingWeight              = pINI->ReadDouble(section, "IceBreakingWeight", IceBreakingWeight);
    CliffBackImpassability         = static_cast<uint8>(pINI->ReadInteger(section, "CliffBackImpassability", CliffBackImpassability));

    VeteranRatio                   = pINI->ReadDouble(section, "VeteranRatio", VeteranRatio);
    VeteranCombat                  = pINI->ReadDouble(section, "VeteranCombat", VeteranCombat);
    VeteranSpeed                   = pINI->ReadDouble(section, "VeteranSpeed", VeteranSpeed);
    VeteranSight                   = pINI->ReadDouble(section, "VeteranSight", VeteranSight);
    VeteranArmor                   = pINI->ReadDouble(section, "VeteranArmor", VeteranArmor);
    VeteranROF                     = pINI->ReadDouble(section, "VeteranROF", VeteranROF);
    VeteranCap                     = pINI->ReadDouble(section, "VeteranCap", VeteranCap);

    ChargeToDrainRatio             = pINI->ReadDouble(section, "ChargeToDrainRatio", ChargeToDrainRatio);
    TrackedUphill                  = pINI->ReadDouble(section, "TrackedUphill", TrackedUphill);
    TrackedDownhill                = pINI->ReadDouble(section, "TrackedDownhill", TrackedDownhill);
    WheeledUphill                  = pINI->ReadDouble(section, "WheeledUphill", WheeledUphill);
    WheeledDownhill                = pINI->ReadDouble(section, "WheeledDownhill", WheeledDownhill);

    SpotlightMovementRadius        = pINI->ReadInteger(section, "SpotlightMovementRadius", SpotlightMovementRadius);
    SpotlightLocationRadius        = pINI->ReadInteger(section, "SpotlightLocationRadius", SpotlightLocationRadius);
    SpotlightSpeed                 = pINI->ReadDouble(section, "SpotlightSpeed", SpotlightSpeed);
    SpotlightAcceleration          = pINI->ReadDouble(section, "SpotlightAcceleration", SpotlightAcceleration);
    SpotlightAngle                 = pINI->ReadDouble(section, "SpotlightAngle", SpotlightAngle);
    SpotlightRadius                = pINI->ReadInteger(section, "SpotlightRadius", SpotlightRadius);

    WindDirection                  = pINI->ReadInteger(section, "WindDirection", WindDirection);
    CameraRange                    = pINI->ReadInteger(section, "CameraRange", CameraRange);
    FlightLevel                    = pINI->ReadInteger(section, "FlightLevel", FlightLevel);
    ParachuteMaxFallRate           = pINI->ReadInteger(section, "ParachuteMaxFallRate", ParachuteMaxFallRate);
    NoParachuteMaxFallRate         = pINI->ReadInteger(section, "NoParachuteMaxFallRate", NoParachuteMaxFallRate);
    BuildingDrop                   = pINI->ReadInteger(section, "BuildingDrop", BuildingDrop);

    ChronoDelay                    = pINI->ReadInteger(section, "ChronoDelay", ChronoDelay);
    ChronoReinfDelay               = pINI->ReadInteger(section, "ChronoReinfDelay", ChronoReinfDelay);
    ChronoDistanceFactor           = pINI->ReadInteger(section, "ChronoDistanceFactor", ChronoDistanceFactor);
    ChronoTrigger                  = pINI->ReadBool(section, "ChronoTrigger", ChronoTrigger);
    ChronoMinimumDelay             = pINI->ReadInteger(section, "ChronoMinimumDelay", ChronoMinimumDelay);
    ChronoRangeMinimum             = pINI->ReadInteger(section, "ChronoRangeMinimum", ChronoRangeMinimum);

    SecretSum                      = pINI->ReadInteger(section, "SecretSum", SecretSum);
    EliteFlashTimer                = pINI->ReadInteger(section, "EliteFlashTimer", EliteFlashTimer);

    IronCurtainDuration            = pINI->ReadInteger(section, "IronCurtainDuration", IronCurtainDuration);
    PsychicRevealRadius            = pINI->ReadInteger(section, "PsychicRevealRadius", PsychicRevealRadius);
    InfantryBlinkDisguiseTime      = pINI->ReadInteger(section, "InfantryBlinkDisguiseTime", InfantryBlinkDisguiseTime);

    IvanDamage                     = pINI->ReadInteger(section, "IvanDamage", IvanDamage);
    IvanTimedDelay                 = pINI->ReadInteger(section, "IvanTimedDelay", IvanTimedDelay);
    CanDetonateTimeBomb            = pINI->ReadBool(section, "CanDetonateTimeBomb", CanDetonateTimeBomb);
    CanDetonateDeathBomb           = pINI->ReadBool(section, "CanDetonateDeathBomb", CanDetonateDeathBomb);
    IvanIconFlickerRate            = pINI->ReadInteger(section, "IvanIconFlickerRate", IvanIconFlickerRate);

    RefundPercent                  = pINI->ReadDouble(section, "RefundPercent", RefundPercent);
    BuildSpeed                     = pINI->ReadDouble(section, "BuildSpeed", BuildSpeed);
    C4Delay                        = pINI->ReadDouble(section, "C4Delay", C4Delay);
    CreditReserve                  = pINI->ReadInteger(section, "CreditReserve", CreditReserve);
    PathDelay                      = pINI->ReadDouble(section, "PathDelay", PathDelay);
    BlockagePathDelay              = pINI->ReadInteger(section, "BlockagePathDelay", BlockagePathDelay);
    MovieTime                      = pINI->ReadDouble(section, "MovieTime", MovieTime);

    CloakDelay                     = pINI->ReadDouble(section, "CloakDelay", CloakDelay);
    GameSpeedBias                  = pINI->ReadDouble(section, "GameSpeedBias", GameSpeedBias);
    BaseBias                       = pINI->ReadDouble(section, "BaseBias", BaseBias);
    ExpSpread                      = pINI->ReadDouble(section, "ExpSpread", ExpSpread);
    MaxIQLevels                    = pINI->ReadInteger(section, "MaxIQLevels", MaxIQLevels);
    SuperWeapons                   = pINI->ReadInteger(section, "SuperWeapons", SuperWeapons);
    Production                     = pINI->ReadInteger(section, "Production", Production);
    GuardArea                      = pINI->ReadInteger(section, "GuardArea", GuardArea);
    RepairSell                     = pINI->ReadInteger(section, "RepairSell", RepairSell);
    AutoCrush                      = pINI->ReadInteger(section, "AutoCrush", AutoCrush);
    Scatter                        = pINI->ReadInteger(section, "Scatter", Scatter);
    ContentScan                    = pINI->ReadInteger(section, "ContentScan", ContentScan);
    Aircraft                       = pINI->ReadInteger(section, "Aircraft", Aircraft);
    Harvester                      = pINI->ReadInteger(section, "Harvester", Harvester);
    SellBack                       = pINI->ReadInteger(section, "SellBack", SellBack);
    AIBaseSpacing                  = pINI->ReadInteger(section, "AIBaseSpacing", AIBaseSpacing);

    Paranoid                       = pINI->ReadBool(section, "Paranoid", Paranoid);
    CurleyShuffle                  = pINI->ReadBool(section, "CurleyShuffle", CurleyShuffle);
    BlendedFog                     = pINI->ReadBool(section, "BlendedFog", BlendedFog);
    CompEasyBonus                  = pINI->ReadBool(section, "CompEasyBonus", CompEasyBonus);
    FineDiffControl                = pINI->ReadBool(section, "FineDiffControl", FineDiffControl);
    TiberiumExplosive              = pINI->ReadBool(section, "TiberiumExplosive", TiberiumExplosive);
    EnemyHealth                    = pINI->ReadBool(section, "EnemyHealth", EnemyHealth);
    AllyReveal                     = pINI->ReadBool(section, "AllyReveal", AllyReveal);
    SeparateAircraft               = pINI->ReadBool(section, "SeparateAircraft", SeparateAircraft);
    TreeTargeting                  = pINI->ReadBool(section, "TreeTargeting", TreeTargeting);
    NamedCivilians                 = pINI->ReadBool(section, "NamedCivilians", NamedCivilians);
    PlayerAutoCrush                = pINI->ReadBool(section, "PlayerAutoCrush", PlayerAutoCrush);
    PlayerReturnFire               = pINI->ReadBool(section, "PlayerReturnFire", PlayerReturnFire);
    PlayerScatter                  = pINI->ReadBool(section, "PlayerScatter", PlayerScatter);
    RevealByHeight                 = pINI->ReadBool(section, "RevealByHeight", RevealByHeight);
    AllowShroudedSubteranneanMoves = pINI->ReadBool(section, "AllowShroudedSubteranneanMoves", AllowShroudedSubteranneanMoves);
    ShroudGrow                     = pINI->ReadBool(section, "ShroudGrow", ShroudGrow);
    NodAIBuildsWalls              = pINI->ReadBool(section, "NodAIBuildsWalls", NodAIBuildsWalls);
    AIBuildsWalls                  = pINI->ReadBool(section, "AIBuildsWalls", AIBuildsWalls);
    UseMinDefenseRule              = pINI->ReadBool(section, "UseMinDefenseRule", UseMinDefenseRule);

    EngineerCaptureLevel           = static_cast<float>(pINI->ReadDouble(section, "EngineerCaptureLevel", EngineerCaptureLevel));
    EngineerCaptureLevel_          = static_cast<float>(pINI->ReadDouble(section, "EngineerCaptureLevel.", EngineerCaptureLevel_));
    TalkBubbleTime                 = static_cast<float>(pINI->ReadDouble(section, "TalkBubbleTime", TalkBubbleTime));

    DirectRockingCoefficient       = static_cast<float>(pINI->ReadDouble(section, "DirectRockingCoefficient", DirectRockingCoefficient));
    FallBackCoefficient            = static_cast<float>(pINI->ReadDouble(section, "FallBackCoefficient", FallBackCoefficient));

    ExtraUnitLight                 = static_cast<float>(pINI->ReadDouble(section, "ExtraUnitLight", ExtraUnitLight));
    ExtraInfantryLight             = static_cast<float>(pINI->ReadDouble(section, "ExtraInfantryLight", ExtraInfantryLight));
    ExtraAircraftLight             = static_cast<float>(pINI->ReadDouble(section, "ExtraAircraftLight", ExtraAircraftLight));

    CurrentStrengthDamage          = pINI->ReadBool(section, "CurrentStrengthDamage", CurrentStrengthDamage);
    WeedCapacity                   = pINI->ReadInteger(section, "WeedCapacity", WeedCapacity);
    Gravity                        = pINI->ReadInteger(section, "Gravity", Gravity);
    LeptonsPerSightIncrease        = pINI->ReadInteger(section, "LeptonsPerSightIncrease", LeptonsPerSightIncrease);
    Incoming                       = pINI->ReadInteger(section, "Incoming", Incoming);
    DominatorFireAtPercentage      = pINI->ReadInteger(section, "DominatorFireAtPercentage", DominatorFireAtPercentage);
    DominatorCaptureRange          = pINI->ReadInteger(section, "DominatorCaptureRange", DominatorCaptureRange);
    DominatorDamage                = pINI->ReadInteger(section, "DominatorDamage", DominatorDamage);
    MindControlAttackLineFrames    = pINI->ReadInteger(section, "MindControlAttackLineFrames", MindControlAttackLineFrames);
    DrainMoneyFrameDelay           = pINI->ReadInteger(section, "DrainMoneyFrameDelay", DrainMoneyFrameDelay);
    DrainMoneyAmount               = pINI->ReadInteger(section, "DrainMoneyAmount", DrainMoneyAmount);
    IonCannonDamage                = pINI->ReadInteger(section, "IonCannonDamage", IonCannonDamage);
    RailgunDamageRadius            = pINI->ReadInteger(section, "RailgunDamageRadius", RailgunDamageRadius);
    PrismSupportModifier           = pINI->ReadInteger(section, "PrismSupportModifier", PrismSupportModifier);
    PrismSupportMax                = pINI->ReadInteger(section, "PrismSupportMax", PrismSupportMax);
    PrismSupportDelay              = pINI->ReadInteger(section, "PrismSupportDelay", PrismSupportDelay);
    PrismSupportDuration           = pINI->ReadInteger(section, "PrismSupportDuration", PrismSupportDuration);
    PrismSupportHeight             = pINI->ReadInteger(section, "PrismSupportHeight", PrismSupportHeight);
    ParadropRadius                 = pINI->ReadInteger(section, "ParadropRadius", ParadropRadius);
}

// ============================================================================
// Read_MultiplayerDialogSettings - [MultiplayerDialogSettings] section
// ============================================================================

void RulesClass::Read_MultiplayerDialogSettings(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "MultiplayerDialogSettings";

    MinMoney         = pINI->ReadInteger(section, "MinMoney", MinMoney);
    Money            = pINI->ReadInteger(section, "Money", Money);
    MaxMoney         = pINI->ReadInteger(section, "MaxMoney", MaxMoney);
    MoneyIncrement   = pINI->ReadInteger(section, "MoneyIncrement", MoneyIncrement);
    MinUnitCount     = pINI->ReadInteger(section, "MinUnitCount", MinUnitCount);
    UnitCount        = pINI->ReadInteger(section, "UnitCount", UnitCount);
    MaxUnitCount     = pINI->ReadInteger(section, "MaxUnitCount", MaxUnitCount);
    TechLevel        = pINI->ReadInteger(section, "TechLevel", TechLevel);
    GameSpeed        = pINI->ReadInteger(section, "GameSpeed", GameSpeed);
    AIDifficultyStruct = pINI->ReadInteger(section, "AIDifficulty", AIDifficultyStruct);
    AIPlayers        = pINI->ReadInteger(section, "AIPlayers", AIPlayers);
    BridgeDestruction = pINI->ReadBool(section, "BridgeDestruction", BridgeDestruction);
    ShadowGrow       = pINI->ReadBool(section, "ShadowGrow", ShadowGrow);
    Shroud           = pINI->ReadBool(section, "Shroud", Shroud);
    Bases            = pINI->ReadBool(section, "Bases", Bases);
    TiberiumGrows    = pINI->ReadBool(section, "TiberiumGrows", TiberiumGrows);
    Crates           = pINI->ReadBool(section, "Crates", Crates);
    CaptureTheFlag   = pINI->ReadBool(section, "CaptureTheFlag", CaptureTheFlag);
    HarvesterTruce   = pINI->ReadBool(section, "HarvesterTruce", HarvesterTruce);
    MultiEngineer    = pINI->ReadBool(section, "MultiEngineer", MultiEngineer);
    AlliesAllowed    = pINI->ReadBool(section, "AlliesAllowed", AlliesAllowed);
    ShortGame        = pINI->ReadBool(section, "ShortGame", ShortGame);
    FogOfWar         = pINI->ReadBool(section, "FogOfWar", FogOfWar);
    MCVRedeploys     = pINI->ReadBool(section, "MCVRedeploys", MCVRedeploys);
    SuperWeaponsAllowed = pINI->ReadBool(section, "SuperWeaponsAllowed", SuperWeaponsAllowed);
    BuildOffAlly     = pINI->ReadBool(section, "BuildOffAlly", BuildOffAlly);
    AllyChangeAllowed = pINI->ReadBool(section, "AllyChangeAllowed", AllyChangeAllowed);
    DropZoneRadius   = pINI->ReadInteger(section, "DropZoneRadius", DropZoneRadius);
}

// ============================================================================
// Section readers - Type list readers and remaining section parsers
// ============================================================================

// ----------------------------------------------------------------------------
// Read_Maximums - [Maximums] section
// ----------------------------------------------------------------------------

void RulesClass::Read_Maximums(CCINIClass* pINI)
{
    if (!pINI) return;
    // [Maximums] section: per-type object count caps (Infantry, Units,
    // Building, Aircraft, Vessel, InfantryType, UnitType, BuildingType,
    // AircraftType, VesselType).  These limits are enforced by the
    // type-class Array containers and do not require RulesClass members.
}

// ----------------------------------------------------------------------------
// Read_InfantryTypes - [InfantryTypes] section
// ----------------------------------------------------------------------------

void RulesClass::Read_InfantryTypes(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "InfantryTypes";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            InfantryTypeClass* pType = InfantryTypeClass::Find(buffer);
            if (!pType) {
                InfantryTypeClass::Init_Array();
                if (InfantryTypeClass::Array) {
                    pType = new InfantryTypeClass();
                    if (pType) {
                        for (int32 j = 0; j < (int32)(sizeof(pType->ID) - 1) && buffer[j] != '\0'; ++j)
                            pType->ID[j] = buffer[j];
                        pType->ID[sizeof(pType->ID) - 1] = '\0';
                        InfantryTypeClass::Array->Add(pType);
                    }
                }
            }
            if (pType) {
                pType->LoadFromINI(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_Countries - [Countries] section
// ----------------------------------------------------------------------------

void RulesClass::Read_Countries(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "Countries";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            HouseTypeClass* pType = HouseTypeClass::FindOrAllocate(buffer);
            if (pType) {
                pType->LoadFromINI(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_VehicleTypes - [VehicleTypes] section
// ----------------------------------------------------------------------------

void RulesClass::Read_VehicleTypes(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "VehicleTypes";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            UnitTypeClass* pType = UnitTypeClass::Find(buffer);
            if (!pType) {
                UnitTypeClass::Init_Array();
                if (UnitTypeClass::Array) {
                    pType = new UnitTypeClass();
                    if (pType) {
                        for (int32 j = 0; j < (int32)(sizeof(pType->ID) - 1) && buffer[j] != '\0'; ++j)
                            pType->ID[j] = buffer[j];
                        pType->ID[sizeof(pType->ID) - 1] = '\0';
                        UnitTypeClass::Array->Add(pType);
                    }
                }
            }
            if (pType) {
                pType->LoadFromINI(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_AircraftTypes - [AircraftTypes] section
// ----------------------------------------------------------------------------

void RulesClass::Read_AircraftTypes(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "AircraftTypes";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            AircraftTypeClass* pType = AircraftTypeClass::Find(buffer);
            if (!pType) {
                AircraftTypeClass::Init_Array();
                if (AircraftTypeClass::Array) {
                    pType = new AircraftTypeClass();
                    if (pType) {
                        for (int32 j = 0; j < (int32)(sizeof(pType->ID) - 1) && buffer[j] != '\0'; ++j)
                            pType->ID[j] = buffer[j];
                        pType->ID[sizeof(pType->ID) - 1] = '\0';
                        AircraftTypeClass::Array->Add(pType);
                    }
                }
            }
            if (pType) {
                pType->LoadFromINI(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_Sides - [Sides] section
// ----------------------------------------------------------------------------

void RulesClass::Read_Sides(CCINIClass* pINI)
{
    if (!pINI) return;
    // [Sides] section: side display names (GDI, Nod, ThirdSide, Civilian).
    // Side names are resolved at runtime from the HouseType registry and
    // do not require dedicated RulesClass members.
}

// ----------------------------------------------------------------------------
// Read_SuperWeaponTypes - [SuperWeaponTypes] section
// ----------------------------------------------------------------------------

void RulesClass::Read_SuperWeaponTypes(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "SuperWeaponTypes";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            SuperWeaponTypeClass* pType = SuperWeaponTypeClass::Find(buffer);
            if (!pType) {
                pType = new SuperWeaponTypeClass(buffer);
            }
            if (pType) {
                pType->LoadFromINI(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_BuildingTypes - [BuildingTypes] section
// ----------------------------------------------------------------------------

void RulesClass::Read_BuildingTypes(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "BuildingTypes";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            BuildingTypeClass* pType = BuildingTypeClass::Find(buffer);
            if (!pType) {
                BuildingTypeClass::Init_Array();
                if (BuildingTypeClass::Array) {
                    pType = new BuildingTypeClass();
                    if (pType) {
                        for (int32 j = 0; j < (int32)(sizeof(pType->ID) - 1) && buffer[j] != '\0'; ++j)
                            pType->ID[j] = buffer[j];
                        pType->ID[sizeof(pType->ID) - 1] = '\0';
                        BuildingTypeClass::Array->Add(pType);
                    }
                }
            }
            if (pType) {
                pType->LoadFromINI(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_TerrainTypes - [TerrainTypes] section
// ----------------------------------------------------------------------------

void RulesClass::Read_TerrainTypes(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "TerrainTypes";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            TerrainTypeClass* pType = TerrainTypeClass::Find(buffer);
            if (!pType) {
                pType = new TerrainTypeClass(buffer);
            }
            if (pType) {
                pType->LoadFromINI(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_SmudgeTypes - [SmudgeTypes] section
// ----------------------------------------------------------------------------

void RulesClass::Read_SmudgeTypes(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "SmudgeTypes";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            SmudgeTypeClass* pType = SmudgeTypeClass::Find(buffer);
            if (!pType) {
                pType = new SmudgeTypeClass(buffer);
            }
            if (pType) {
                pType->LoadFromINI(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_OverlayTypes - [OverlayTypes] section
// ----------------------------------------------------------------------------

void RulesClass::Read_OverlayTypes(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "OverlayTypes";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            OverlayTypeClass* pType = OverlayTypeClass::Find(buffer);
            if (!pType) {
                pType = new OverlayTypeClass(buffer);
            }
            if (pType) {
                pType->LoadFromINI(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_Animations - [Animations] section
// ----------------------------------------------------------------------------

void RulesClass::Read_Animations(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "Animations";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            AnimTypeClass* pType = AnimTypeClass::Find(buffer);
            if (!pType) {
                pType = new AnimTypeClass(buffer);
            }
            if (pType) {
                pType->LoadFromINI(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_VoxelAnims - [VoxelAnims] section
// ----------------------------------------------------------------------------

void RulesClass::Read_VoxelAnims(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "VoxelAnims";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            VoxelAnimTypeClass* pType = VoxelAnimTypeClass::Find(buffer);
            if (!pType) {
                pType = new VoxelAnimTypeClass(buffer);
            }
            if (pType) {
                pType->LoadFromINI(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_Warheads - [Warheads] section
// ----------------------------------------------------------------------------

void RulesClass::Read_Warheads(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "Warheads";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            WarheadTypeClass* pType = WarheadTypeClass::FindOrAllocate(buffer);
            if (pType) {
                pType->LoadFromINIList(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_Particles - [Particles] section
// ----------------------------------------------------------------------------

void RulesClass::Read_Particles(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "Particles";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            ParticleTypeClass* pType = new ParticleTypeClass();
            if (pType) {
                pType->SetName(buffer);
                pType->ReadFromINI(pINI, buffer);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_ParticleSystems - [ParticleSystems] section
// ----------------------------------------------------------------------------

void RulesClass::Read_ParticleSystems(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "ParticleSystems";

    int32 count = pINI->GetKeyCount(section);
    for (int32 i = 0; i < count; ++i) {
        const char* key = pINI->GetKeyName(section, i);
        if (!key) continue;
        char buffer[256];
        if (pINI->ReadString(section, key, "", buffer, sizeof(buffer)) > 0) {
            ParticleSystemTypeClass* pType = ParticleSystemTypeClass::Find(buffer);
            if (!pType) {
                pType = new ParticleSystemTypeClass();
                if (pType) {
                    pType->SetName(buffer);
                }
            }
            if (pType) {
                pType->LoadFromINI(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_AI - [AI] section
// ----------------------------------------------------------------------------

void RulesClass::Read_AI(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "AI";

    AITriggerSuccessWeightDelta       = pINI->ReadDouble(section, "AITriggerSuccessWeightDelta", AITriggerSuccessWeightDelta);
    AITriggerFailureWeightDelta       = pINI->ReadDouble(section, "AITriggerFailureWeightDelta", AITriggerFailureWeightDelta);
    AITriggerTrackRecordCoefficient   = pINI->ReadDouble(section, "AITriggerTrackRecordCoefficient", AITriggerTrackRecordCoefficient);

    AISafeDistance                    = pINI->ReadInteger(section, "AISafeDistance", AISafeDistance);
    HarvesterTooFarDistance           = pINI->ReadInteger(section, "HarvesterTooFarDistance", HarvesterTooFarDistance);
    ChronoHarvTooFarDistance          = pINI->ReadInteger(section, "ChronoHarvTooFarDistance", ChronoHarvTooFarDistance);
    AIRestrictReplaceTime             = pINI->ReadInteger(section, "AIRestrictReplaceTime", AIRestrictReplaceTime);
    ThreatPerOccupant                 = pINI->ReadInteger(section, "ThreatPerOccupant", ThreatPerOccupant);
    ApproachTargetResetMultiplier     = pINI->ReadInteger(section, "ApproachTargetResetMultiplier", ApproachTargetResetMultiplier);
    CampaignMoneyDeltaEasy            = pINI->ReadInteger(section, "CampaignMoneyDeltaEasy", CampaignMoneyDeltaEasy);
    CampaignMoneyDeltaHard            = pINI->ReadInteger(section, "CampaignMoneyDeltaHard", CampaignMoneyDeltaHard);
    GuardAreaTargetingDelay           = pINI->ReadInteger(section, "GuardAreaTargetingDelay", GuardAreaTargetingDelay);
    NormalTargetingDelay              = pINI->ReadInteger(section, "NormalTargetingDelay", NormalTargetingDelay);
    AINavalYardAdjacency              = pINI->ReadInteger(section, "AINavalYardAdjacency", AINavalYardAdjacency);
    MaximumBuildingPlacementFailures  = pINI->ReadInteger(section, "MaximumBuildingPlacementFailures", MaximumBuildingPlacementFailures);
    AICaptureLowMoneyMark             = pINI->ReadInteger(section, "AICaptureLowMoneyMark", AICaptureLowMoneyMark);
    AICaptureWoundedMark              = pINI->ReadInteger(section, "AICaptureWoundedMark", AICaptureWoundedMark);
    AISuperDefenseFrames              = pINI->ReadInteger(section, "AISuperDefenseFrames", AISuperDefenseFrames);

    AISuperDefenseDistance            = static_cast<float>(pINI->ReadDouble(section, "AISuperDefenseDistance", AISuperDefenseDistance));
    AIMinorSuperReadyPercent          = static_cast<float>(pINI->ReadDouble(section, "AIMinorSuperReadyPercent", AIMinorSuperReadyPercent));

    PurifierBonus                     = static_cast<float>(pINI->ReadDouble(section, "PurifierBonus", PurifierBonus));
    OccupyDamageMultiplier            = static_cast<float>(pINI->ReadDouble(section, "OccupyDamageMultiplier", OccupyDamageMultiplier));
    OccupyROFMultiplier               = static_cast<float>(pINI->ReadDouble(section, "OccupyROFMultiplier", OccupyROFMultiplier));
    OccupyWeaponRange                 = pINI->ReadInteger(section, "OccupyWeaponRange", OccupyWeaponRange);
    BunkerDamageMultiplier            = pINI->ReadInteger(section, "BunkerDamageMultiplier", BunkerDamageMultiplier);
    BunkerROFMultiplier               = static_cast<float>(pINI->ReadDouble(section, "BunkerROFMultiplier", BunkerROFMultiplier));
    BunkerWeaponRangeBonus            = pINI->ReadInteger(section, "BunkerWeaponRangeBonus", BunkerWeaponRangeBonus);
    OpenToppedDamageMultiplier        = static_cast<float>(pINI->ReadDouble(section, "OpenToppedDamageMultiplier", OpenToppedDamageMultiplier));
    OpenToppedRangeBonus              = pINI->ReadInteger(section, "OpenToppedRangeBonus", OpenToppedRangeBonus);
    OpenToppedWarpDistance            = pINI->ReadInteger(section, "OpenToppedWarpDistance", OpenToppedWarpDistance);
    FallingDamageMultiplier           = static_cast<float>(pINI->ReadDouble(section, "FallingDamageMultiplier", FallingDamageMultiplier));

    PatrolScan                        = pINI->ReadDouble(section, "PatrolScan", PatrolScan);
    DissolveUnfilledTeamDelay         = pINI->ReadInteger(section, "DissolveUnfilledTeamDelay", DissolveUnfilledTeamDelay);
    AIAlternateProductionCreditCutoff = pINI->ReadInteger(section, "AIAlternateProductionCreditCutoff", AIAlternateProductionCreditCutoff);
    AIUseTurbineUpgradeProbability   = pINI->ReadDouble(section, "AIUseTurbineUpgradeProbability", AIUseTurbineUpgradeProbability);

    GDIWallDefense                    = pINI->ReadDouble(section, "GDIWallDefense", GDIWallDefense);
    GDIWallDefenseCoefficient         = pINI->ReadDouble(section, "GDIWallDefenseCoefficient", GDIWallDefenseCoefficient);
    NodBaseDefenseCoefficient         = pINI->ReadDouble(section, "NodBaseDefenseCoefficient", NodBaseDefenseCoefficient);
    GDIBaseDefenseCoefficient         = pINI->ReadDouble(section, "GDIBaseDefenseCoefficient", GDIBaseDefenseCoefficient);
    ComputerBaseDefenseResponse       = pINI->ReadInteger(section, "ComputerBaseDefenseResponse", ComputerBaseDefenseResponse);
    MaximumBaseDefenseValue           = pINI->ReadInteger(section, "MaximumBaseDefenseValue", MaximumBaseDefenseValue);

    AttackInterval                    = pINI->ReadDouble(section, "AttackInterval", AttackInterval);
    AttackDelay                       = pINI->ReadDouble(section, "AttackDelay", AttackDelay);
    PowerEmergency                    = pINI->ReadDouble(section, "PowerEmergency", PowerEmergency);

    MyEffectivenessCoefficientDefault = pINI->ReadDouble(section, "MyEffectivenessCoefficientDefault", MyEffectivenessCoefficientDefault);
    TargetEffectivenessCoefficientDefault = pINI->ReadDouble(section, "TargetEffectivenessCoefficientDefault", TargetEffectivenessCoefficientDefault);
    TargetSpecialThreatCoefficientDefault = pINI->ReadDouble(section, "TargetSpecialThreatCoefficientDefault", TargetSpecialThreatCoefficientDefault);
    TargetStrengthCoefficientDefault  = pINI->ReadDouble(section, "TargetStrengthCoefficientDefault", TargetStrengthCoefficientDefault);
    TargetDistanceCoefficientDefault  = pINI->ReadDouble(section, "TargetDistanceCoefficientDefault", TargetDistanceCoefficientDefault);
    DumbMyEffectivenessCoefficient    = pINI->ReadDouble(section, "DumbMyEffectivenessCoefficient", DumbMyEffectivenessCoefficient);
    DumbTargetEffectivenessCoefficient = pINI->ReadDouble(section, "DumbTargetEffectivenessCoefficient", DumbTargetEffectivenessCoefficient);
    DumbTargetSpecialThreatCoefficient = pINI->ReadDouble(section, "DumbTargetSpecialThreatCoefficient", DumbTargetSpecialThreatCoefficient);
    DumbTargetStrengthCoefficient     = pINI->ReadDouble(section, "DumbTargetStrengthCoefficient", DumbTargetStrengthCoefficient);
    DumbTargetDistanceCoefficient     = pINI->ReadDouble(section, "DumbTargetDistanceCoefficient", DumbTargetDistanceCoefficient);
    EnemyHouseThreatBonus             = pINI->ReadDouble(section, "EnemyHouseThreatBonus", EnemyHouseThreatBonus);
    TurboBoost                        = pINI->ReadDouble(section, "TurboBoost", TurboBoost);

    AirstripRatio                     = pINI->ReadDouble(section, "AirstripRatio", AirstripRatio);
    AirstripLimit                     = pINI->ReadInteger(section, "AirstripLimit", AirstripLimit);
    HelipadRatio                      = pINI->ReadDouble(section, "HelipadRatio", HelipadRatio);
    HelipadLimit                      = pINI->ReadInteger(section, "HelipadLimit", HelipadLimit);
    TeslaRatio                        = pINI->ReadDouble(section, "TeslaRatio", TeslaRatio);
    TeslaLimit                        = pINI->ReadInteger(section, "TeslaLimit", TeslaLimit);
    AARatio                           = pINI->ReadDouble(section, "AARatio", AARatio);
    AALimit                           = pINI->ReadInteger(section, "AALimit", AALimit);
    DefenseRatio                      = pINI->ReadDouble(section, "DefenseRatio", DefenseRatio);
    DefenseLimit                      = pINI->ReadInteger(section, "DefenseLimit", DefenseLimit);
    WarRatio                          = pINI->ReadDouble(section, "WarRatio", WarRatio);
    WarLimit                          = pINI->ReadInteger(section, "WarLimit", WarLimit);
    BarracksRatio                     = pINI->ReadDouble(section, "BarracksRatio", BarracksRatio);
    BarracksLimit                     = pINI->ReadInteger(section, "BarracksLimit", BarracksLimit);
    RefineryLimit                     = pINI->ReadInteger(section, "RefineryLimit", RefineryLimit);
    RefineryRatio                     = pINI->ReadDouble(section, "RefineryRatio", RefineryRatio);
    BaseSizeAdd                       = pINI->ReadInteger(section, "BaseSizeAdd", BaseSizeAdd);
    PowerSurplus                      = pINI->ReadInteger(section, "PowerSurplus", PowerSurplus);
    InfantryReserve                   = pINI->ReadInteger(section, "InfantryReserve", InfantryReserve);
    InfantryBaseMult                  = pINI->ReadInteger(section, "InfantryBaseMult", InfantryBaseMult);
}

// ----------------------------------------------------------------------------
// Read_Powerups - [Powerups] section
// ----------------------------------------------------------------------------

void RulesClass::Read_Powerups(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "Powerups";

    Crates                            = pINI->ReadBool(section, "Crates", Crates);
    CrateMinimum                      = pINI->ReadInteger(section, "CrateMinimum", CrateMinimum);
    CrateMaximum                      = pINI->ReadInteger(section, "CrateMaximum", CrateMaximum);

    // UnitCrateType - resolve type name to UnitTypeClass pointer
    char unitCrateBuffer[256];
    if (pINI->ReadString(section, "UnitCrateType", "", unitCrateBuffer, sizeof(unitCrateBuffer)) > 0) {
        UnitTypeClass* pUnitType = UnitTypeClass::Find(unitCrateBuffer);
        if (pUnitType) {
            UnitCrateType = pUnitType;
        }
    }

    // DropZoneAnim - resolve type name to AnimTypeClass pointer
    char dropZoneBuffer[256];
    if (pINI->ReadString(section, "DropZoneAnim", "", dropZoneBuffer, sizeof(dropZoneBuffer)) > 0) {
        AnimTypeClass* pAnimType = AnimTypeClass::Find(dropZoneBuffer);
        if (pAnimType) {
            DropZoneAnim = pAnimType;
        }
    }
}

// ----------------------------------------------------------------------------
// Read_LandCharacteristics - [LandCharacteristics] section
// ----------------------------------------------------------------------------

void RulesClass::Read_LandCharacteristics(CCINIClass* pINI)
{
    if (!pINI) return;
    // [LandCharacteristics] section: per-LandType Speed, Buildable, Passable
    // values.  Land movement characteristics are handled by the SpeedType /
    // MovementZone systems in TechnoTypeClass and do not require RulesClass
    // members.
}

// ----------------------------------------------------------------------------
// Read_IQ - [IQ] section
// ----------------------------------------------------------------------------

void RulesClass::Read_IQ(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "IQ";

    MaxIQLevels   = pINI->ReadInteger(section, "MaxIQLevels", MaxIQLevels);
    SuperWeapons  = pINI->ReadInteger(section, "SuperWeapons", SuperWeapons);
    Production    = pINI->ReadInteger(section, "Production", Production);
    GuardArea     = pINI->ReadInteger(section, "GuardArea", GuardArea);
    RepairSell    = pINI->ReadInteger(section, "RepairSell", RepairSell);
    AutoCrush     = pINI->ReadInteger(section, "AutoCrush", AutoCrush);
    Scatter       = pINI->ReadInteger(section, "Scatter", Scatter);
    ContentScan   = pINI->ReadInteger(section, "ContentScan", ContentScan);
    Aircraft      = pINI->ReadInteger(section, "Aircraft", Aircraft);
    Harvester     = pINI->ReadInteger(section, "Harvester", Harvester);
    SellBack      = pINI->ReadInteger(section, "SellBack", SellBack);
}

// ----------------------------------------------------------------------------
// Read_JumpjetControls - [JumpjetControls] section
// ----------------------------------------------------------------------------

void RulesClass::Read_JumpjetControls(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "JumpjetControls";

    GateUp            = pINI->ReadInteger(section, "GateUp", GateUp);
    GateDown          = pINI->ReadInteger(section, "GateDown", GateDown);
    TurnRate          = pINI->ReadInteger(section, "TurnRate", TurnRate);
    Speed             = pINI->ReadInteger(section, "Speed", Speed);
    Climb             = pINI->ReadDouble(section, "Climb", Climb);
    CruiseHeight      = pINI->ReadInteger(section, "CruiseHeight", CruiseHeight);
    Acceleration      = pINI->ReadDouble(section, "Acceleration", Acceleration);
    WobblesPerSecond  = pINI->ReadDouble(section, "WobblesPerSecond", WobblesPerSecond);
    WobbleDeviation   = pINI->ReadInteger(section, "WobbleDeviation", WobbleDeviation);
}

// ----------------------------------------------------------------------------
// Read_Difficulties - [Difficulties] section
// ----------------------------------------------------------------------------

void RulesClass::Read_Difficulties(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "Difficulties";

    // Double multipliers (also read in Read_Difficulty from [Difficulty])
    Easy.Firepower      = pINI->ReadDouble(section, "EasyFirepower", Easy.Firepower);
    Easy.GroundSpeed    = pINI->ReadDouble(section, "EasyGroundSpeed", Easy.GroundSpeed);
    Easy.AirSpeed       = pINI->ReadDouble(section, "EasyAirSpeed", Easy.AirSpeed);
    Easy.Armor          = pINI->ReadDouble(section, "EasyArmor", Easy.Armor);
    Easy.ROF            = pINI->ReadDouble(section, "EasyROF", Easy.ROF);
    Easy.Cost           = pINI->ReadDouble(section, "EasyCost", Easy.Cost);
    Easy.BuildTime      = pINI->ReadDouble(section, "EasyBuildTime", Easy.BuildTime);
    Easy.RepairDelay    = pINI->ReadDouble(section, "EasyRepairDelay", Easy.RepairDelay);
    Easy.BuildDelay     = pINI->ReadDouble(section, "EasyBuildDelay", Easy.BuildDelay);

    Normal.Firepower    = pINI->ReadDouble(section, "NormalFirepower", Normal.Firepower);
    Normal.GroundSpeed  = pINI->ReadDouble(section, "NormalGroundSpeed", Normal.GroundSpeed);
    Normal.AirSpeed     = pINI->ReadDouble(section, "NormalAirSpeed", Normal.AirSpeed);
    Normal.Armor        = pINI->ReadDouble(section, "NormalArmor", Normal.Armor);
    Normal.ROF          = pINI->ReadDouble(section, "NormalROF", Normal.ROF);
    Normal.Cost         = pINI->ReadDouble(section, "NormalCost", Normal.Cost);
    Normal.BuildTime    = pINI->ReadDouble(section, "NormalBuildTime", Normal.BuildTime);
    Normal.RepairDelay  = pINI->ReadDouble(section, "NormalRepairDelay", Normal.RepairDelay);
    Normal.BuildDelay   = pINI->ReadDouble(section, "NormalBuildDelay", Normal.BuildDelay);

    Difficult.Firepower   = pINI->ReadDouble(section, "DifficultFirepower", Difficult.Firepower);
    Difficult.GroundSpeed = pINI->ReadDouble(section, "DifficultGroundSpeed", Difficult.GroundSpeed);
    Difficult.AirSpeed    = pINI->ReadDouble(section, "DifficultAirSpeed", Difficult.AirSpeed);
    Difficult.Armor       = pINI->ReadDouble(section, "DifficultArmor", Difficult.Armor);
    Difficult.ROF         = pINI->ReadDouble(section, "DifficultROF", Difficult.ROF);
    Difficult.Cost        = pINI->ReadDouble(section, "DifficultCost", Difficult.Cost);
    Difficult.BuildTime   = pINI->ReadDouble(section, "DifficultBuildTime", Difficult.BuildTime);
    Difficult.RepairDelay = pINI->ReadDouble(section, "DifficultRepairDelay", Difficult.RepairDelay);
    Difficult.BuildDelay  = pINI->ReadDouble(section, "DifficultBuildDelay", Difficult.BuildDelay);

    // Boolean fields (not read by Read_Difficulty)
    Easy.BuildSlowdown    = pINI->ReadBool(section, "EasyBuildSlowdown", Easy.BuildSlowdown);
    Easy.DestroyWalls     = pINI->ReadBool(section, "EasyDestroyWalls", Easy.DestroyWalls);
    Easy.ContentScan      = pINI->ReadBool(section, "EasyContentScan", Easy.ContentScan);

    Normal.BuildSlowdown  = pINI->ReadBool(section, "NormalBuildSlowdown", Normal.BuildSlowdown);
    Normal.DestroyWalls   = pINI->ReadBool(section, "NormalDestroyWalls", Normal.DestroyWalls);
    Normal.ContentScan    = pINI->ReadBool(section, "NormalContentScan", Normal.ContentScan);

    Difficult.BuildSlowdown  = pINI->ReadBool(section, "DifficultBuildSlowdown", Difficult.BuildSlowdown);
    Difficult.DestroyWalls   = pINI->ReadBool(section, "DifficultDestroyWalls", Difficult.DestroyWalls);
    Difficult.ContentScan    = pINI->ReadBool(section, "DifficultContentScan", Difficult.ContentScan);
}

// ----------------------------------------------------------------------------
// Read_Movies - [Movies] section
// ----------------------------------------------------------------------------

void RulesClass::Read_Movies(CCINIClass* pINI)
{
    if (!pINI) return;
    // [Movies] section: campaign movie filenames for victory/defeat events
    // (AlliedVictory, SovietVictory, IntroMovie, etc.).  Movie playback is
    // handled by the campaign/mission system and does not require RulesClass
    // members.
}

// ----------------------------------------------------------------------------
// Read_AdvancedCommandBar - [AdvancedCommandBar] section
// ----------------------------------------------------------------------------

void RulesClass::Read_AdvancedCommandBar(CCINIClass* pINI)
{
    if (!pINI) return;
    // [AdvancedCommandBar] section: command bar UI configuration.  These
    // settings are consumed directly by the UI system and do not require
    // RulesClass members.
}

// ----------------------------------------------------------------------------
// Read_HarvesterRules - [HarvesterRules] section
// ----------------------------------------------------------------------------

void RulesClass::Read_HarvesterRules(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "HarvesterRules";

    TiberiumShortScan           = pINI->ReadInteger(section, "TiberiumShortScan", TiberiumShortScan);
    TiberiumLongScan            = pINI->ReadInteger(section, "TiberiumLongScan", TiberiumLongScan);
    SlaveMinerShortScan         = pINI->ReadInteger(section, "SlaveMinerShortScan", SlaveMinerShortScan);
    SlaveMinerSlaveScan         = pINI->ReadInteger(section, "SlaveMinerSlaveScan", SlaveMinerSlaveScan);
    SlaveMinerLongScan          = pINI->ReadInteger(section, "SlaveMinerLongScan", SlaveMinerLongScan);
    SlaveMinerScanCorrection    = pINI->ReadInteger(section, "SlaveMinerScanCorrection", SlaveMinerScanCorrection);
    SlaveMinerKickFrameDelay    = pINI->ReadInteger(section, "SlaveMinerKickFrameDelay", SlaveMinerKickFrameDelay);
    HarvesterLoadRate           = pINI->ReadInteger(section, "HarvesterLoadRate", HarvesterLoadRate);
    HarvesterDumpRate           = pINI->ReadDouble(section, "HarvesterDumpRate", HarvesterDumpRate);
}

// ============================================================================
// PointerGotInvalid - Handle expired pointer references
// ============================================================================

void RulesClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    if (!pInvalid) return;

    // Check all pointer-type members for the expired pointer
    // This is called when an object is being destroyed

    // Type type pointers
    if (LargeVisceroid == reinterpret_cast<UnitTypeClass*>(pInvalid))
        LargeVisceroid = nullptr;
    if (SmallVisceroid == reinterpret_cast<UnitTypeClass*>(pInvalid))
        SmallVisceroid = nullptr;
    if (PrerequisiteProcAlternate == reinterpret_cast<UnitTypeClass*>(pInvalid))
        PrerequisiteProcAlternate = nullptr;
    if (PrismType == reinterpret_cast<BuildingTypeClass*>(pInvalid))
        PrismType = nullptr;
    if (GDIGateOne == reinterpret_cast<BuildingTypeClass*>(pInvalid))
        GDIGateOne = nullptr;
    if (GDIGateTwo == reinterpret_cast<BuildingTypeClass*>(pInvalid))
        GDIGateTwo = nullptr;
    if (NodGateOne == reinterpret_cast<BuildingTypeClass*>(pInvalid))
        NodGateOne = nullptr;
    if (NodGateTwo == reinterpret_cast<BuildingTypeClass*>(pInvalid))
        NodGateTwo = nullptr;
    if (WallTower == reinterpret_cast<BuildingTypeClass*>(pInvalid))
        WallTower = nullptr;
    if (GDIPowerPlant == reinterpret_cast<BuildingTypeClass*>(pInvalid))
        GDIPowerPlant = nullptr;
    if (NodRegularPower == reinterpret_cast<BuildingTypeClass*>(pInvalid))
        NodRegularPower = nullptr;
    if (NodAdvancedPower == reinterpret_cast<BuildingTypeClass*>(pInvalid))
        NodAdvancedPower = nullptr;
    if (ThirdPowerPlant == reinterpret_cast<BuildingTypeClass*>(pInvalid))
        ThirdPowerPlant = nullptr;
    if (UnitCrateType == reinterpret_cast<UnitTypeClass*>(pInvalid))
        UnitCrateType = nullptr;
    if (Paratrooper == reinterpret_cast<InfantryTypeClass*>(pInvalid))
        Paratrooper = nullptr;
    if (Technician == reinterpret_cast<InfantryTypeClass*>(pInvalid))
        Technician = nullptr;
    if (Engineer == reinterpret_cast<InfantryTypeClass*>(pInvalid))
        Engineer = nullptr;
    if (Pilot == reinterpret_cast<InfantryTypeClass*>(pInvalid))
        Pilot = nullptr;
    if (AlliedCrew == reinterpret_cast<InfantryTypeClass*>(pInvalid))
        AlliedCrew = nullptr;
    if (SovietCrew == reinterpret_cast<InfantryTypeClass*>(pInvalid))
        SovietCrew = nullptr;
    if (ThirdCrew == reinterpret_cast<InfantryTypeClass*>(pInvalid))
        ThirdCrew = nullptr;
    if (AlliedDisguise == reinterpret_cast<InfantryTypeClass*>(pInvalid))
        AlliedDisguise = nullptr;
    if (SovietDisguise == reinterpret_cast<InfantryTypeClass*>(pInvalid))
        SovietDisguise = nullptr;
    if (ThirdDisguise == reinterpret_cast<InfantryTypeClass*>(pInvalid))
        ThirdDisguise = nullptr;
    if (VeinholeTypeClass == reinterpret_cast<TerrainTypeClass*>(pInvalid))
        VeinholeTypeClass = nullptr;
}

// ============================================================================
// GetDifficulty - Get difficulty struct for given level
// ============================================================================

const DifficultyStruct* RulesClass::GetDifficulty(int32 level) const
{
    switch (level) {
        case 0: return &Easy;
        case 1: return &Normal;
        case 2: return &Difficult;
        default: return &Normal;
    }
}