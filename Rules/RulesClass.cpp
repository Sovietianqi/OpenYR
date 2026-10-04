#include <Rules/RulesClass.h>
#include <IO/MovieClass.h>
#include <Audio/VocClass.h>
#include <Combat/BulletTypeClass.h>
#include <Core/Definitions.h>
#include <INI/INIClass.h>

#include <cstring>
#include <cstdlib>
#include <cmath>

#include <Abstract/InfantryTypeClass.h>
#include <Abstract/UnitTypeClass.h>
#include <Abstract/AircraftTypeClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Abstract/MissionClass.h>
#include <Special/TiberiumClass.h>
#include <Objects/IsometricTile.h>
#include <Abstract/TerrainTypeClass.h>
#include <Abstract/SmudgeTypeClass.h>
#include <Abstract/OverlayTypeClass.h>
#include <Abstract/VoxelAnimTypeClass.h>
#include <Animations/AnimTypeClass.h>
#include <Houses/HouseTypeClass.h>
#include <Houses/SideClass.h>
#include <Combat/WarheadTypeClass.h>
#include <Combat/WeaponTypeClass.h>
#include <SW/SuperWeaponTypeClass.h>
#include <Particles/ParticleTypeClass.h>
#include <Particles/ParticleSystemTypeClass.h>
#include <Helpers/StringHelpers.h>


// ============================================================================
// SplitCommaList - 原地切分逗号分隔的名字列表（去首尾空白）
//
// 原版 rules 里大量键（DamageFireTypes、BaseUnit、SplashList ...）是逗号分隔的
// 类型名列表，读入后逐个 FindOrAllocate 填入 DynamicVectorClass。
// ============================================================================
namespace
{
    int32 SplitCommaList(char* pBuffer, char* pTokens[], int32 maxTokens)
    {
        int32 count = 0;
        char* p = pBuffer;

        while (*p)
        {
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
            if (!*p) break;

            char* start = p;
            while (*p && *p != ',') ++p;

            char* end = p;
            while (end > start &&
                   (*(end - 1) == ' ' || *(end - 1) == '\t' ||
                    *(end - 1) == '\r' || *(end - 1) == '\n')) --end;
            *end = '\0';

            if (count < maxTokens)
                pTokens[count] = start;
            ++count;

            if (*p == ',') ++p;
        }

        return count < maxTokens ? count : maxTokens;
    }
}

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
    , BuildBaseSpacer(2)
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
    , Apply100Warhead(nullptr)
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
    // Initialize the [LandCharacteristics] table.  Every movement multiplier
    // defaults to 1.0 (fully passable) and every terrain is buildable until a
    // rules file says otherwise - that mirrors the binary, where a missing
    // section leaves the previous value untouched.
    for (int32 i = 0; i < LAND_TYPE_COUNT; ++i) {
        LandCharacteristics[i].Hover      = 1.0f;
        LandCharacteristics[i].Foot       = 1.0f;
        LandCharacteristics[i].Track      = 1.0f;
        LandCharacteristics[i].Wheel      = 1.0f;
        LandCharacteristics[i].Float      = 1.0f;
        LandCharacteristics[i].Amphibious = 1.0f;
        LandCharacteristics[i].FloatBeach = 1.0f;
        LandCharacteristics[i].Buildable  = true;
    }

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
    Read_Colors(pINI);
    Read_ColorAdd(pINI);
    Read_Tiberiums(pINI);
    Read_TileTypes(pINI);
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
    Read_Movies(pINI);
    Read_AdvancedCommandBar(pINI);
    Read_HarvesterRules(pINI);
    Read_MissionControl(pINI);
}

// ============================================================================
// Read_MissionControl - per-mission control sections ( [Sleep], [Guard], ... )
// ============================================================================

void RulesClass::Read_MissionControl(CCINIClass* pINI)
{
    if (!pINI) return;

    MissionControlClass::LoadAllFromINI(pINI);
}

// ============================================================================
// ============================================================================
// Read_SpecialWeapons - RulesClass_Addition_SpecialWeapons (asm 0x668FB0)
//
//   Gated on the [SpecialWeapons] section actually existing: when it is
//   absent the call answers false and every pointer keeps its value.  Each
//   key resolves through the type registry with FindOrAllocate - a key that
//   is missing or empty leaves the previous pointer alone rather than
//   clearing it.  The tail walk notifies every registered warhead type once
//   the block has been read.
// ============================================================================

void RulesClass::Read_SpecialWeapons(CCINIClass* pINI)
{
    if (!pINI) return;
    if (!pINI->SectionExists("SpecialWeapons")) return;

    const char* section = "SpecialWeapons";

    // These are type references resolved through the type registries; an
    // empty or absent key leaves the pointer the rules already carried.

    { char _buf[0x80]; if (pINI->ReadString(section, "NukeWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) NukeWarhead = _p; } }
    { char _buf[0x80]; if (pINI->ReadString(section, "NukeProjectile", "", _buf, sizeof(_buf)) > 0) { BulletTypeClass* _p = BulletTypeClass::FindOrAllocate(_buf); if (_p) NukeProjectile = _p; } }
    { char _buf[0x80]; if (pINI->ReadString(section, "NukeDown", "", _buf, sizeof(_buf)) > 0) { BulletTypeClass* _p = BulletTypeClass::FindOrAllocate(_buf); if (_p) NukeDown = _p; } }
    { char _buf[0x80]; if (pINI->ReadString(section, "MutateWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) MutateWarhead = _p; } }
    { char _buf[0x80]; if (pINI->ReadString(section, "MutateExplosionWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) MutateExplosionWarhead = _p; } }
    { char _buf[0x80]; if (pINI->ReadString(section, "EMPulseWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) EMPulseWarhead = _p; } }
    { char _buf[0x80]; if (pINI->ReadString(section, "EMPulseProjectile", "", _buf, sizeof(_buf)) > 0) { BulletTypeClass* _p = BulletTypeClass::FindOrAllocate(_buf); if (_p) EMPulseProjectile = _p; } }

    // The block closes by notifying every warhead type (vtable +0x64) - the
    // original's broadcast hook after the special weapons have been wired up.
    WarheadTypeClass::NotifyAll();
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

    // ---- rules keys ----
    DetailMinFrameRateNormal         = pINI->ReadInteger(section, "DetailMinFrameRateNormal", DetailMinFrameRateNormal);
    DetailMinFrameRateMovie          = pINI->ReadInteger(section, "DetailMinFrameRateMovie", DetailMinFrameRateMovie);
    DetailBufferZoneWidth            = pINI->ReadInteger(section, "DetailBufferZoneWidth", DetailBufferZoneWidth);
    PoseDir                          = pINI->ReadInteger(section, "PoseDir", PoseDir);
    DeployDir                        = pINI->ReadInteger(section, "DeployDir", DeployDir);
    { char _buf[0x40]; if (pINI->ReadString(section, "DropPodPuff", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) DropPodPuff = _p; } }
    WaypointAnimationSpeed           = pINI->ReadInteger(section, "WaypointAnimationSpeed", WaypointAnimationSpeed);
    { char _buf[0x40]; if (pINI->ReadString(section, "VeinAttack", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) VeinAttack = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Dig", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) Dig = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "AtmosphereEntry", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) AtmosphereEntry = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "GateUp", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) GateUp = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "GateDown", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) GateDown = _i; } }
    ShroudGrow                       = pINI->ReadBool(section, "ShroudGrow", ShroudGrow);
    ScrollMultiplier                 = pINI->ReadDouble(section, "ScrollMultiplier", ScrollMultiplier);
    ShakeScreen                      = pINI->ReadInteger(section, "ShakeScreen", ShakeScreen);
    { char _buf[0x40]; if (pINI->ReadString(section, "BuildingDrop", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) BuildingDrop = _i; } }
    { char _buf[0x400]; if (pINI->ReadString(section, "TreeFire", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); TreeFire.Clear(); for (int32 _t = 0; _t < _n; ++_t) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) TreeFire.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "OnFire", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); OnFire.Clear(); for (int32 _t = 0; _t < _n; ++_t) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) OnFire.Add(_p); } } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Smoke", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) Smoke = _p; } }
    EliteFlashTimer                  = pINI->ReadInteger(section, "EliteFlashTimer", EliteFlashTimer);
    { char _buf[0x40]; if (pINI->ReadString(section, "SmallFire", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) SmallFire = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "LargeFire", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) LargeFire = _p; } }
    AllyReveal                       = pINI->ReadBool(section, "AllyReveal", AllyReveal);
    ConditionRed                     = pINI->ReadDouble(section, "ConditionRed", ConditionRed);
    ConditionYellow                  = pINI->ReadDouble(section, "ConditionYellow", ConditionYellow);
    EnemyHealth                      = pINI->ReadBool(section, "EnemyHealth", EnemyHealth);
    Gravity                          = pINI->ReadInteger(section, "Gravity", Gravity);
    IdleActionFrequency              = pINI->ReadDouble(section, "IdleActionFrequency", IdleActionFrequency);
    MessageDelay                     = pINI->ReadDouble(section, "MessageDelay", MessageDelay);
    MovieTime                        = pINI->ReadDouble(section, "MovieTime", MovieTime);
    NamedCivilians                   = pINI->ReadBool(section, "NamedCivilians", NamedCivilians);
    SavourDelay                      = pINI->ReadDouble(section, "SavourDelay", SavourDelay);
    ShroudRate                       = pINI->ReadDouble(section, "ShroudRate", ShroudRate);
    FogRate                          = pINI->ReadDouble(section, "FogRate", FogRate);
    VeinGrowthRate                   = pINI->ReadDouble(section, "VeinGrowthRate", VeinGrowthRate);
    IceGrowthRate                    = pINI->ReadDouble(section, "IceGrowthRate", IceGrowthRate);
    IceSolidifyFrameTime             = pINI->ReadInteger(section, "IceSolidifyFrameTime", IceSolidifyFrameTime);
    AmbientChangeRate                = pINI->ReadDouble(section, "AmbientChangeRate", AmbientChangeRate);
    AmbientChangeStep                = pINI->ReadDouble(section, "AmbientChangeStep", AmbientChangeStep);
    SpeakDelay                       = pINI->ReadDouble(section, "SpeakDelay", SpeakDelay);
    TimerWarning                     = pINI->ReadDouble(section, "TimerWarning", TimerWarning);
    ExtraUnitLight                   = static_cast<float>(pINI->ReadDouble(section, "ExtraUnitLight", ExtraUnitLight));
    ExtraInfantryLight               = static_cast<float>(pINI->ReadDouble(section, "ExtraInfantryLight", ExtraInfantryLight));
    ExtraAircraftLight               = static_cast<float>(pINI->ReadDouble(section, "ExtraAircraftLight", ExtraAircraftLight));
    pINI->Get3Bytes(section, "LocalRadarColor", reinterpret_cast<uint8*>(&LocalRadarColor));
    pINI->Get3Bytes(section, "LineTrailColorOverride", reinterpret_cast<uint8*>(&LineTrailColorOverride));
    pINI->Get3Bytes(section, "ChronoBeamColor", reinterpret_cast<uint8*>(&ChronoBeamColor));
    pINI->Get3Bytes(section, "MagnaBeamColor", reinterpret_cast<uint8*>(&MagnaBeamColor));
    OreTwinkleChance                 = pINI->ReadInteger(section, "OreTwinkleChance", OreTwinkleChance);
    LaserTargetColor                 = pINI->ReadInteger(section, "LaserTargetColor", LaserTargetColor);
    IronCurtainColor                 = pINI->ReadInteger(section, "IronCurtainColor", IronCurtainColor);
    BerserkColor                     = pINI->ReadInteger(section, "BerserkColor", BerserkColor);
    ForceShieldColor                 = pINI->ReadInteger(section, "ForceShieldColor", ForceShieldColor);
    DirectRockingCoefficient         = static_cast<float>(pINI->ReadDouble(section, "DirectRockingCoefficient", DirectRockingCoefficient));
    FallBackCoefficient              = static_cast<float>(pINI->ReadDouble(section, "FallBackCoefficient", FallBackCoefficient));
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

    // ---- rules keys ----
    FreeMCV                          = pINI->ReadBool(section, "FreeMCV", FreeMCV);
    { char _buf[0x40]; if (pINI->ReadString(section, "WoodCrateImg", "", _buf, sizeof(_buf)) > 0) { OverlayTypeClass* _p = OverlayTypeClass::FindOrAllocate(_buf); if (_p) WoodCrateImg = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "CrateImg", "", _buf, sizeof(_buf)) > 0) { OverlayTypeClass* _p = OverlayTypeClass::FindOrAllocate(_buf); if (_p) CrateImg = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "WaterCrateImg", "", _buf, sizeof(_buf)) > 0) { OverlayTypeClass* _p = OverlayTypeClass::FindOrAllocate(_buf); if (_p) WaterCrateImg = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "HealCrateSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) HealCrateSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "UnitCrateType", "", _buf, sizeof(_buf)) > 0) { UnitTypeClass* _p = UnitTypeClass::FindOrAllocate(_buf); if (_p) UnitCrateType = _p; } }
    SilverCrate.Type = pINI->GetPowerup(section, "SilverCrate", SilverCrate.Type);
    WoodCrate.Type   = pINI->GetPowerup(section, "WoodCrate",   WoodCrate.Type);
    WaterCrate.Type  = pINI->GetPowerup(section, "WaterCrate",  WaterCrate.Type);
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

    // ---- rules keys ----
    AmmoCrateDamage                  = pINI->ReadInteger(section, "AmmoCrateDamage", AmmoCrateDamage);
    IonCannonDamage                  = pINI->ReadInteger(section, "IonCannonDamage", IonCannonDamage);
    RailgunDamageRadius              = pINI->ReadInteger(section, "RailgunDamageRadius", RailgunDamageRadius);
    { char _buf[0x400]; if (pINI->ReadString(section, "Scorches", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); Scorches.Clear(); for (int32 _t = 0; _t < _n; ++_t) { SmudgeTypeClass* _p = SmudgeTypeClass::FindOrAllocate(_tok[_t]); if (_p) Scorches.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "Scorches1", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); Scorches1.Clear(); for (int32 _t = 0; _t < _n; ++_t) { SmudgeTypeClass* _p = SmudgeTypeClass::FindOrAllocate(_tok[_t]); if (_p) Scorches1.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "Scorches2", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); Scorches2.Clear(); for (int32 _t = 0; _t < _n; ++_t) { SmudgeTypeClass* _p = SmudgeTypeClass::FindOrAllocate(_tok[_t]); if (_p) Scorches2.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "Scorches3", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); Scorches3.Clear(); for (int32 _t = 0; _t < _n; ++_t) { SmudgeTypeClass* _p = SmudgeTypeClass::FindOrAllocate(_tok[_t]); if (_p) Scorches3.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "Scorches4", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); Scorches4.Clear(); for (int32 _t = 0; _t < _n; ++_t) { SmudgeTypeClass* _p = SmudgeTypeClass::FindOrAllocate(_tok[_t]); if (_p) Scorches4.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "SplashList", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); SplashList.Clear(); for (int32 _t = 0; _t < _n; ++_t) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) SplashList.Add(_p); } } }
    { char _buf[0x40]; if (pINI->ReadString(section, "FlameDamage", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) FlameDamage = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "FlameDamage2", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) FlameDamage2 = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "C4Warhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) C4Warhead = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "CrushWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) CrushWarhead = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "V3Warhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) V3Warhead = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DMislWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) DMislWarhead = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "V3EliteWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) V3EliteWarhead = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DMislEliteWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) DMislEliteWarhead = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "CMislWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) CMislWarhead = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "CMislEliteWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) CMislEliteWarhead = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "IvanWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) IvanWarhead = _p; } }
    CanDetonateTimeBomb              = pINI->ReadBool(section, "CanDetonateTimeBomb", CanDetonateTimeBomb);
    CanDetonateDeathBomb             = pINI->ReadBool(section, "CanDetonateDeathBomb", CanDetonateDeathBomb);
    { char _buf[0x40]; if (pINI->ReadString(section, "DeathWeapon", "", _buf, sizeof(_buf)) > 0) { WeaponTypeClass* _p = WeaponTypeClass::FindOrAllocate(_buf); if (_p) DeathWeapon = _p; } }
    IvanDamage                       = pINI->ReadInteger(section, "IvanDamage", IvanDamage);
    IvanTimedDelay                   = pINI->ReadInteger(section, "IvanTimedDelay", IvanTimedDelay);
    IvanIconFlickerRate              = pINI->ReadInteger(section, "IvanIconFlickerRate", IvanIconFlickerRate);
    IronCurtainDuration              = pINI->ReadInteger(section, "IronCurtainDuration", IronCurtainDuration);
    PsychicRevealRadius              = pINI->ReadInteger(section, "PsychicRevealRadius", PsychicRevealRadius);
    OccupyDamageMultiplier           = static_cast<float>(pINI->ReadDouble(section, "OccupyDamageMultiplier", OccupyDamageMultiplier));
    OccupyROFMultiplier              = static_cast<float>(pINI->ReadDouble(section, "OccupyROFMultiplier", OccupyROFMultiplier));
    OccupyWeaponRange                = pINI->ReadInteger(section, "OccupyWeaponRange", OccupyWeaponRange);
    BunkerROFMultiplier              = static_cast<float>(pINI->ReadDouble(section, "BunkerROFMultiplier", BunkerROFMultiplier));
    BunkerWeaponRangeBonus           = pINI->ReadInteger(section, "BunkerWeaponRangeBonus", BunkerWeaponRangeBonus);
    OpenToppedDamageMultiplier       = static_cast<float>(pINI->ReadDouble(section, "OpenToppedDamageMultiplier", OpenToppedDamageMultiplier));
    OpenToppedRangeBonus             = pINI->ReadInteger(section, "OpenToppedRangeBonus", OpenToppedRangeBonus);
    OpenToppedWarpDistance           = pINI->ReadInteger(section, "OpenToppedWarpDistance", OpenToppedWarpDistance);
    MindControlAttackLineFrames      = pINI->ReadInteger(section, "MindControlAttackLineFrames", MindControlAttackLineFrames);
    { char _buf[0x40]; if (pINI->ReadString(section, "DrainAnimationType", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) DrainAnimationType = _p; } }
    DrainMoneyFrameDelay             = pINI->ReadInteger(section, "DrainMoneyFrameDelay", DrainMoneyFrameDelay);
    DrainMoneyAmount                 = pINI->ReadInteger(section, "DrainMoneyAmount", DrainMoneyAmount);
    FallingDamageMultiplier          = static_cast<float>(pINI->ReadDouble(section, "FallingDamageMultiplier", FallingDamageMultiplier));
    CurrentStrengthDamage            = pINI->ReadBool(section, "CurrentStrengthDamage", CurrentStrengthDamage);
    { char _buf[0x40]; if (pINI->ReadString(section, "ControlledAnimationType", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) ControlledAnimationType = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "PermaControlledAnimationType", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) PermaControlledAnimationType = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "IonCannonWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) IonCannonWarhead = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DefaultLargeGreySmokeSystem", "", _buf, sizeof(_buf)) > 0) { ParticleSystemTypeClass* _p = ParticleSystemTypeClass::FindOrAllocate(_buf); if (_p) DefaultLargeGreySmokeSystem = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DefaultSmallGreySmokeSystem", "", _buf, sizeof(_buf)) > 0) { ParticleSystemTypeClass* _p = ParticleSystemTypeClass::FindOrAllocate(_buf); if (_p) DefaultSmallGreySmokeSystem = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DefaultSparkSystem", "", _buf, sizeof(_buf)) > 0) { ParticleSystemTypeClass* _p = ParticleSystemTypeClass::FindOrAllocate(_buf); if (_p) DefaultSparkSystem = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DefaultLargeRedSmokeSystem", "", _buf, sizeof(_buf)) > 0) { ParticleSystemTypeClass* _p = ParticleSystemTypeClass::FindOrAllocate(_buf); if (_p) DefaultLargeRedSmokeSystem = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DefaultSmallRedSmokeSystem", "", _buf, sizeof(_buf)) > 0) { ParticleSystemTypeClass* _p = ParticleSystemTypeClass::FindOrAllocate(_buf); if (_p) DefaultSmallRedSmokeSystem = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DefaultDebrisSmokeSystem", "", _buf, sizeof(_buf)) > 0) { ParticleSystemTypeClass* _p = ParticleSystemTypeClass::FindOrAllocate(_buf); if (_p) DefaultDebrisSmokeSystem = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DefaultFireStreamSystem", "", _buf, sizeof(_buf)) > 0) { ParticleSystemTypeClass* _p = ParticleSystemTypeClass::FindOrAllocate(_buf); if (_p) DefaultFireStreamSystem = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DefaultTestParticleSystem", "", _buf, sizeof(_buf)) > 0) { ParticleSystemTypeClass* _p = ParticleSystemTypeClass::FindOrAllocate(_buf); if (_p) DefaultTestParticleSystem = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DefaultRepairParticleSystem", "", _buf, sizeof(_buf)) > 0) { ParticleSystemTypeClass* _p = ParticleSystemTypeClass::FindOrAllocate(_buf); if (_p) DefaultRepairParticleSystem = _p; } }
    BerzerkAllowed                   = pINI->ReadBool(section, "BerzerkAllowed", BerzerkAllowed);
    TurboBoost                       = pINI->ReadDouble(section, "TurboBoost", TurboBoost);
    C4Delay                          = pINI->ReadDouble(section, "C4Delay", C4Delay);
    ExpSpread                        = pINI->ReadDouble(section, "ExpSpread", ExpSpread);
    TiberiumExplosive                = pINI->ReadBool(section, "TiberiumExplosive", TiberiumExplosive);
    PlayerAutoCrush                  = pINI->ReadBool(section, "PlayerAutoCrush", PlayerAutoCrush);
    PlayerReturnFire                 = pINI->ReadBool(section, "PlayerReturnFire", PlayerReturnFire);
    PlayerScatter                    = pINI->ReadBool(section, "PlayerScatter", PlayerScatter);
    TreeTargeting                    = pINI->ReadBool(section, "TreeTargeting", TreeTargeting);
    pINI->GetVectorIntegers(section, "OverloadCount",  OverloadCount);
    pINI->GetVectorIntegers(section, "OverloadDamage", OverloadDamage);
    pINI->GetVectorIntegers(section, "OverloadFrames", OverloadFrames);
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

    // ---- rules keys ----
    pINI->Get3Bytes(section, "RadColor", reinterpret_cast<uint8*>(&RadColor));
    { char _buf[0x40]; if (pINI->ReadString(section, "RadSiteWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) RadSiteWarhead = _p; } }
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

    // ---- rules keys ----
    AlliedWallTransparency           = pINI->ReadBool(section, "AlliedWallTransparency", AlliedWallTransparency);
    WallPenetratorThreshold          = pINI->ReadDouble(section, "WallPenetratorThreshold", WallPenetratorThreshold);
}


// ============================================================================
// Read_TileTypes - the [TileSet####] blocks of isometr(md).ini
// ============================================================================

void RulesClass::Read_TileTypes(CCINIClass* pINI)
{
    if (!pINI) return;

    IsometricTileType::CreateFromINIList(pINI, false);
}

// ============================================================================
// Read_Tiberiums - [Tiberiums] section
// ============================================================================

void RulesClass::Read_Tiberiums(CCINIClass* pINI)
{
    if (!pINI) return;

    TiberiumClass::CreateFromINIList(pINI);
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

    // ---- rules keys ----
    pINI->Get3Bytes(section, "None", reinterpret_cast<uint8*>(&NoneValue));
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

    // ---- rules keys ----
    pINI->Get3Bytes(section, "None", reinterpret_cast<uint8*>(&NoneValue));
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

    // ---- rules keys ----
    { char _buf[0x400]; if (pINI->ReadString(section, "DamageFireTypes", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); DamageFireTypes.Clear(); for (int32 _t = 0; _t < _n; ++_t) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) DamageFireTypes.Add(_p); } } }
    { char _buf[0x40]; if (pINI->ReadString(section, "OreTwinkle", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) OreTwinkle = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "BarrelExplode", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) BarrelExplode = _p; } }
    { char _buf[0x400]; if (pINI->ReadString(section, "BarrelDebris", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BarrelDebris.Clear(); for (int32 _t = 0; _t < _n; ++_t) { VoxelAnimTypeClass* _p = VoxelAnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) BarrelDebris.Add(_p); } } }
    { char _buf[0x40]; if (pINI->ReadString(section, "BarrelParticle", "", _buf, sizeof(_buf)) > 0) { ParticleSystemTypeClass* _p = ParticleSystemTypeClass::FindOrAllocate(_buf); if (_p) BarrelParticle = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "NukeTakeOff", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) NukeTakeOff = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Wake", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) Wake = _p; } }
    { char _buf[0x400]; if (pINI->ReadString(section, "DropPod", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); DropPod.Clear(); for (int32 _t = 0; _t < _n; ++_t) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) DropPod.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "DeadBodies", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); DeadBodies.Clear(); for (int32 _t = 0; _t < _n; ++_t) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) DeadBodies.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "MetallicDebris", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); MetallicDebris.Clear(); for (int32 _t = 0; _t < _n; ++_t) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) MetallicDebris.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "BridgeExplosions", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BridgeExplosions.Clear(); for (int32 _t = 0; _t < _n; ++_t) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) BridgeExplosions.Add(_p); } } }
    { char _buf[0x40]; if (pINI->ReadString(section, "IonBlast", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) IonBlast = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "IonBeam", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) IonBeam = _p; } }
    { char _buf[0x400]; if (pINI->ReadString(section, "WeatherConClouds", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); WeatherConClouds.Clear(); for (int32 _t = 0; _t < _n; ++_t) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) WeatherConClouds.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "WeatherConBolts", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); WeatherConBolts.Clear(); for (int32 _t = 0; _t < _n; ++_t) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) WeatherConBolts.Add(_p); } } }
    { char _buf[0x40]; if (pINI->ReadString(section, "WeatherConBoltExplosion", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) WeatherConBoltExplosion = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DominatorWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) DominatorWarhead = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DominatorFirstAnim", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) DominatorFirstAnim = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DominatorSecondAnim", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) DominatorSecondAnim = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ChronoPlacement", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) ChronoPlacement = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ChronoBeam", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) ChronoBeam = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ChronoBlast", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) ChronoBlast = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ChronoBlastDest", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) ChronoBlastDest = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "WarpIn", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) WarpIn = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "WarpOut", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) WarpOut = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "WarpAway", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) WarpAway = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "IronCurtainInvokeAnim", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) IronCurtainInvokeAnim = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ForceShieldInvokeAnim", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) ForceShieldInvokeAnim = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "WeaponNullifyAnim", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) WeaponNullifyAnim = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ChronoSparkle1", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) ChronoSparkle1 = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "InfantryExplode", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) InfantryExplode = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "FlamingInfantry", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) FlamingInfantry = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "InfantryHeadPop", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) InfantryHeadPop = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "InfantryNuked", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) InfantryNuked = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "InfantryVirus", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) InfantryVirus = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "InfantryBrute", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) InfantryBrute = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "InfantryMutate", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) InfantryMutate = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Behind", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) Behind = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "MoveFlash", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) MoveFlash = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Parachute", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) Parachute = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "BombParachute", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) BombParachute = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DropZoneAnim", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) DropZoneAnim = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "EMPulseSparkles", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) EMPulseSparkles = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "LargeVisceroid", "", _buf, sizeof(_buf)) > 0) { UnitTypeClass* _p = UnitTypeClass::FindOrAllocate(_buf); if (_p) LargeVisceroid = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "SmallVisceroid", "", _buf, sizeof(_buf)) > 0) { UnitTypeClass* _p = UnitTypeClass::FindOrAllocate(_buf); if (_p) SmallVisceroid = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DropPodWeapon", "", _buf, sizeof(_buf)) > 0) { WeaponTypeClass* _p = WeaponTypeClass::FindOrAllocate(_buf); if (_p) DropPodWeapon = _p; } }
    { char _buf[0x400]; if (pINI->ReadString(section, "ExplosiveVoxelDebris", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); ExplosiveVoxelDebris.Clear(); for (int32 _t = 0; _t < _n; ++_t) { VoxelAnimTypeClass* _p = VoxelAnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) ExplosiveVoxelDebris.Add(_p); } } }
    { char _buf[0x40]; if (pINI->ReadString(section, "TireVoxelDebris", "", _buf, sizeof(_buf)) > 0) { VoxelAnimTypeClass* _p = VoxelAnimTypeClass::FindOrAllocate(_buf); if (_p) TireVoxelDebris = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ScrapVoxelDebris", "", _buf, sizeof(_buf)) > 0) { VoxelAnimTypeClass* _p = VoxelAnimTypeClass::FindOrAllocate(_buf); if (_p) ScrapVoxelDebris = _p; } }
    { char _buf[0x400]; if (pINI->ReadString(section, "RepairBay", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); RepairBay.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) RepairBay.Add(_p); } } }
    { char _buf[0x40]; if (pINI->ReadString(section, "GDIGateOne", "", _buf, sizeof(_buf)) > 0) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_buf); if (_p) GDIGateOne = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "GDIGateTwo", "", _buf, sizeof(_buf)) > 0) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_buf); if (_p) GDIGateTwo = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "NodGateOne", "", _buf, sizeof(_buf)) > 0) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_buf); if (_p) NodGateOne = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "NodGateTwo", "", _buf, sizeof(_buf)) > 0) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_buf); if (_p) NodGateTwo = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "WallTower", "", _buf, sizeof(_buf)) > 0) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_buf); if (_p) WallTower = _p; } }
    { char _buf[0x400]; if (pINI->ReadString(section, "Shipyard", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); Shipyard.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) Shipyard.Add(_p); } } }
    { char _buf[0x40]; if (pINI->ReadString(section, "GDIPowerPlant", "", _buf, sizeof(_buf)) > 0) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_buf); if (_p) GDIPowerPlant = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "NodRegularPower", "", _buf, sizeof(_buf)) > 0) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_buf); if (_p) NodRegularPower = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "NodAdvancedPower", "", _buf, sizeof(_buf)) > 0) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_buf); if (_p) NodAdvancedPower = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ThirdPowerPlant", "", _buf, sizeof(_buf)) > 0) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_buf); if (_p) ThirdPowerPlant = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "PrerequisiteProcAlternate", "", _buf, sizeof(_buf)) > 0) { UnitTypeClass* _p = UnitTypeClass::FindOrAllocate(_buf); if (_p) PrerequisiteProcAlternate = _p; } }
    { char _buf[0x400]; if (pINI->ReadString(section, "BaseUnit", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BaseUnit.Clear(); for (int32 _t = 0; _t < _n; ++_t) { UnitTypeClass* _p = UnitTypeClass::FindOrAllocate(_tok[_t]); if (_p) BaseUnit.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "HarvesterUnit", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); HarvesterUnit.Clear(); for (int32 _t = 0; _t < _n; ++_t) { UnitTypeClass* _p = UnitTypeClass::FindOrAllocate(_tok[_t]); if (_p) HarvesterUnit.Add(_p); } } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Paratrooper", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) Paratrooper = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "AlliedDisguise", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) AlliedDisguise = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "SovietDisguise", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) SovietDisguise = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ThirdDisguise", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) ThirdDisguise = _p; } }
    SpyPowerBlackout                 = pINI->ReadInteger(section, "SpyPowerBlackout", SpyPowerBlackout);
    SpyMoneyStealPercent             = static_cast<float>(pINI->ReadDouble(section, "SpyMoneyStealPercent", SpyMoneyStealPercent));
    AttackCursorOnDisguise           = pINI->ReadBool(section, "AttackCursorOnDisguise", AttackCursorOnDisguise);
    PurifierBonus                    = static_cast<float>(pINI->ReadDouble(section, "PurifierBonus", PurifierBonus));
    { char _buf[0x40]; if (pINI->ReadString(section, "Engineer", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) Engineer = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Technician", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) Technician = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Pilot", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) Pilot = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "AlliedCrew", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) AlliedCrew = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "SovietCrew", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) SovietCrew = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ThirdCrew", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) ThirdCrew = _p; } }
    AIAlternateProductionCreditCutoff = pINI->ReadInteger(section, "AIAlternateProductionCreditCutoff", AIAlternateProductionCreditCutoff);
    AIUseTurbineUpgradeProbability   = pINI->ReadDouble(section, "AIUseTurbineUpgradeProbability", AIUseTurbineUpgradeProbability);
    DissolveUnfilledTeamDelay        = pINI->ReadInteger(section, "DissolveUnfilledTeamDelay", DissolveUnfilledTeamDelay);
    AISafeDistance                   = pINI->ReadInteger(section, "AISafeDistance", AISafeDistance);
    AIMinorSuperReadyPercent         = static_cast<float>(pINI->ReadDouble(section, "AIMinorSuperReadyPercent", AIMinorSuperReadyPercent));
    HarvesterTooFarDistance          = pINI->ReadInteger(section, "HarvesterTooFarDistance", HarvesterTooFarDistance);
    ChronoHarvTooFarDistance         = pINI->ReadInteger(section, "ChronoHarvTooFarDistance", ChronoHarvTooFarDistance);
    AIRestrictReplaceTime            = pINI->ReadInteger(section, "AIRestrictReplaceTime", AIRestrictReplaceTime);
    ThreatPerOccupant                = pINI->ReadInteger(section, "ThreatPerOccupant", ThreatPerOccupant);
    ApproachTargetResetMultiplier    = pINI->ReadInteger(section, "ApproachTargetResetMultiplier", ApproachTargetResetMultiplier);
    CampaignMoneyDeltaEasy           = pINI->ReadInteger(section, "CampaignMoneyDeltaEasy", CampaignMoneyDeltaEasy);
    CampaignMoneyDeltaHard           = pINI->ReadInteger(section, "CampaignMoneyDeltaHard", CampaignMoneyDeltaHard);
    GuardAreaTargetingDelay          = pINI->ReadInteger(section, "GuardAreaTargetingDelay", GuardAreaTargetingDelay);
    NormalTargetingDelay             = pINI->ReadInteger(section, "NormalTargetingDelay", NormalTargetingDelay);
    AINavalYardAdjacency             = pINI->ReadInteger(section, "AINavalYardAdjacency", AINavalYardAdjacency);
    MaximumBuildingPlacementFailures = pINI->ReadInteger(section, "MaximumBuildingPlacementFailures", MaximumBuildingPlacementFailures);
    SlaveMinerKickFrameDelay         = pINI->ReadInteger(section, "SlaveMinerKickFrameDelay", SlaveMinerKickFrameDelay);
    AISuperDefenseFrames             = pINI->ReadInteger(section, "AISuperDefenseFrames", AISuperDefenseFrames);
    AICaptureLowMoneyMark            = pINI->ReadInteger(section, "AICaptureLowMoneyMark", AICaptureLowMoneyMark);
    BaseDefenseDelay                 = pINI->ReadDouble(section, "BaseDefenseDelay", BaseDefenseDelay);
    SuspendPriority                  = pINI->ReadInteger(section, "SuspendPriority", SuspendPriority);
    SuspendDelay                     = pINI->ReadDouble(section, "SuspendDelay", SuspendDelay);
    SurvivorRate                     = pINI->ReadDouble(section, "SurvivorRate", SurvivorRate);
    AlliedSurvivorDivisor            = pINI->ReadInteger(section, "AlliedSurvivorDivisor", AlliedSurvivorDivisor);
    SovietSurvivorDivisor            = pINI->ReadInteger(section, "SovietSurvivorDivisor", SovietSurvivorDivisor);
    ThirdSurvivorDivisor             = pINI->ReadInteger(section, "ThirdSurvivorDivisor", ThirdSurvivorDivisor);
    ReloadRate                       = pINI->ReadDouble(section, "ReloadRate", ReloadRate);
    BuildupTime                      = pINI->ReadDouble(section, "BuildupTime", BuildupTime);
    HarvesterDumpRate                = pINI->ReadDouble(section, "HarvesterDumpRate", HarvesterDumpRate);
    HarvesterLoadRate                = pINI->ReadInteger(section, "HarvesterLoadRate", HarvesterLoadRate);
    DamageDelay                      = pINI->ReadDouble(section, "DamageDelay", DamageDelay);
    GrowthRate                       = pINI->ReadDouble(section, "GrowthRate", GrowthRate);
    RepairPercent                    = pINI->ReadDouble(section, "RepairPercent", RepairPercent);
    RepairStep                       = pINI->ReadInteger(section, "RepairStep", RepairStep);
    IRepairStep                      = pINI->ReadInteger(section, "IRepairStep", IRepairStep);
    RepairRate                       = pINI->ReadDouble(section, "RepairRate", RepairRate);
    URepairRate                      = pINI->ReadDouble(section, "URepairRate", URepairRate);
    IRepairRate                      = pINI->ReadDouble(section, "IRepairRate", IRepairRate);
    TiberiumTransmogrify             = pINI->ReadInteger(section, "TiberiumTransmogrify", TiberiumTransmogrify);
    LightningDeferment               = pINI->ReadInteger(section, "LightningDeferment", LightningDeferment);
    LightningDamage                  = pINI->ReadInteger(section, "LightningDamage", LightningDamage);
    LightningStormDuration           = pINI->ReadInteger(section, "LightningStormDuration", LightningStormDuration);
    LightningHitDelay                = pINI->ReadInteger(section, "LightningHitDelay", LightningHitDelay);
    LightningScatterDelay            = pINI->ReadInteger(section, "LightningScatterDelay", LightningScatterDelay);
    LightningCellSpread              = pINI->ReadInteger(section, "LightningCellSpread", LightningCellSpread);
    LightningSeparation              = pINI->ReadInteger(section, "LightningSeparation", LightningSeparation);
    { char _buf[0x40]; if (pINI->ReadString(section, "LightningWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) LightningWarhead = _p; } }
    LightningPrintText               = pINI->ReadBool(section, "LightningPrintText", LightningPrintText);
    ForceShieldRadius                = pINI->ReadInteger(section, "ForceShieldRadius", ForceShieldRadius);
    ForceShieldDuration              = pINI->ReadInteger(section, "ForceShieldDuration", ForceShieldDuration);
    ForceShieldBlackoutDuration      = pINI->ReadInteger(section, "ForceShieldBlackoutDuration", ForceShieldBlackoutDuration);
    ForceShieldPlayFadeSoundTime     = pINI->ReadInteger(section, "ForceShieldPlayFadeSoundTime", ForceShieldPlayFadeSoundTime);
    MutateExplosion                  = pINI->ReadBool(section, "MutateExplosion", MutateExplosion);
    { char _buf[0x40]; if (pINI->ReadString(section, "PrismType", "", _buf, sizeof(_buf)) > 0) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_buf); if (_p) PrismType = _p; } }
    V3RocketPauseFrames              = pINI->ReadInteger(section, "V3RocketPauseFrames", V3RocketPauseFrames);
    V3RocketTiltFrames               = pINI->ReadInteger(section, "V3RocketTiltFrames", V3RocketTiltFrames);
    V3RocketPitchInitial             = pINI->ReadDouble(section, "V3RocketPitchInitial", V3RocketPitchInitial);
    V3RocketPitchFinal               = pINI->ReadDouble(section, "V3RocketPitchFinal", V3RocketPitchFinal);
    V3RocketTurnRate                 = pINI->ReadDouble(section, "V3RocketTurnRate", V3RocketTurnRate);
    V3RocketRaiseRate                = pINI->ReadDouble(section, "V3RocketRaiseRate", V3RocketRaiseRate);
    V3RocketAcceleration             = pINI->ReadDouble(section, "V3RocketAcceleration", V3RocketAcceleration);
    V3RocketAltitude                 = pINI->ReadInteger(section, "V3RocketAltitude", V3RocketAltitude);
    V3RocketDamage                   = pINI->ReadInteger(section, "V3RocketDamage", V3RocketDamage);
    V3RocketEliteDamage              = pINI->ReadInteger(section, "V3RocketEliteDamage", V3RocketEliteDamage);
    V3RocketBodyLength               = pINI->ReadInteger(section, "V3RocketBodyLength", V3RocketBodyLength);
    V3RocketLazyCurve                = pINI->ReadBool(section, "V3RocketLazyCurve", V3RocketLazyCurve);
    { char _buf[0x40]; if (pINI->ReadString(section, "V3RocketType", "", _buf, sizeof(_buf)) > 0) { AircraftTypeClass* _p = AircraftTypeClass::FindOrAllocate(_buf); if (_p) V3RocketType = _p; } }
    DMislPauseFrames                 = pINI->ReadInteger(section, "DMislPauseFrames", DMislPauseFrames);
    DMislTiltFrames                  = pINI->ReadInteger(section, "DMislTiltFrames", DMislTiltFrames);
    DMislPitchInitial                = pINI->ReadDouble(section, "DMislPitchInitial", DMislPitchInitial);
    DMislPitchFinal                  = pINI->ReadDouble(section, "DMislPitchFinal", DMislPitchFinal);
    DMislTurnRate                    = pINI->ReadDouble(section, "DMislTurnRate", DMislTurnRate);
    DMislRaiseRate                   = pINI->ReadDouble(section, "DMislRaiseRate", DMislRaiseRate);
    DMislAcceleration                = pINI->ReadDouble(section, "DMislAcceleration", DMislAcceleration);
    DMislAltitude                    = pINI->ReadInteger(section, "DMislAltitude", DMislAltitude);
    DMislDamage                      = pINI->ReadInteger(section, "DMislDamage", DMislDamage);
    DMislEliteDamage                 = pINI->ReadInteger(section, "DMislEliteDamage", DMislEliteDamage);
    DMislBodyLength                  = pINI->ReadInteger(section, "DMislBodyLength", DMislBodyLength);
    DMislLazyCurve                   = pINI->ReadBool(section, "DMislLazyCurve", DMislLazyCurve);
    { char _buf[0x40]; if (pINI->ReadString(section, "DMislType", "", _buf, sizeof(_buf)) > 0) { AircraftTypeClass* _p = AircraftTypeClass::FindOrAllocate(_buf); if (_p) DMislType = _p; } }
    CMislPauseFrames                 = pINI->ReadInteger(section, "CMislPauseFrames", CMislPauseFrames);
    CMislTiltFrames                  = pINI->ReadInteger(section, "CMislTiltFrames", CMislTiltFrames);
    CMislPitchInitial                = pINI->ReadDouble(section, "CMislPitchInitial", CMislPitchInitial);
    CMislPitchFinal                  = pINI->ReadDouble(section, "CMislPitchFinal", CMislPitchFinal);
    CMislTurnRate                    = pINI->ReadDouble(section, "CMislTurnRate", CMislTurnRate);
    CMislRaiseRate                   = pINI->ReadDouble(section, "CMislRaiseRate", CMislRaiseRate);
    CMislAcceleration                = pINI->ReadDouble(section, "CMislAcceleration", CMislAcceleration);
    CMislAltitude                    = pINI->ReadInteger(section, "CMislAltitude", CMislAltitude);
    CMislDamage                      = pINI->ReadInteger(section, "CMislDamage", CMislDamage);
    CMislEliteDamage                 = pINI->ReadInteger(section, "CMislEliteDamage", CMislEliteDamage);
    CMislBodyLength                  = pINI->ReadInteger(section, "CMislBodyLength", CMislBodyLength);
    CMislLazyCurve                   = pINI->ReadBool(section, "CMislLazyCurve", CMislLazyCurve);
    { char _buf[0x40]; if (pINI->ReadString(section, "CMislType", "", _buf, sizeof(_buf)) > 0) { AircraftTypeClass* _p = AircraftTypeClass::FindOrAllocate(_buf); if (_p) CMislType = _p; } }
    WallBuildSpeedCoefficient        = pINI->ReadDouble(section, "WallBuildSpeedCoefficient", WallBuildSpeedCoefficient);
    ConditionYellowSparkingProbability = pINI->ReadDouble(section, "ConditionYellowSparkingProbability", ConditionYellowSparkingProbability);
    ConditionRedSparkingProbability  = pINI->ReadDouble(section, "ConditionRedSparkingProbability", ConditionRedSparkingProbability);
    AITriggerSuccessWeightDelta      = pINI->ReadDouble(section, "AITriggerSuccessWeightDelta", AITriggerSuccessWeightDelta);
    AITriggerFailureWeightDelta      = pINI->ReadDouble(section, "AITriggerFailureWeightDelta", AITriggerFailureWeightDelta);
    AITriggerTrackRecordCoefficient  = pINI->ReadDouble(section, "AITriggerTrackRecordCoefficient", AITriggerTrackRecordCoefficient);
    FlashFrameTime                   = pINI->ReadInteger(section, "FlashFrameTime", FlashFrameTime);
    RadarCombatFlashTime             = pINI->ReadInteger(section, "RadarCombatFlashTime", RadarCombatFlashTime);
    RadarEventSpeed                  = static_cast<float>(pINI->ReadDouble(section, "RadarEventSpeed", RadarEventSpeed));
    RadarEventRotationSpeed          = static_cast<float>(pINI->ReadDouble(section, "RadarEventRotationSpeed", RadarEventRotationSpeed));
    RadarEventMinRadius              = pINI->ReadInteger(section, "RadarEventMinRadius", RadarEventMinRadius);
    RadarEventColorSpeed             = static_cast<float>(pINI->ReadDouble(section, "RadarEventColorSpeed", RadarEventColorSpeed));
    MyEffectivenessCoefficientDefault = pINI->ReadDouble(section, "MyEffectivenessCoefficientDefault", MyEffectivenessCoefficientDefault);
    TargetEffectivenessCoefficientDefa = pINI->ReadDouble(section, "TargetEffectivenessCoefficientDefa", TargetEffectivenessCoefficientDefa);
    TargetSpecialThreatCoefficientDefa = pINI->ReadDouble(section, "TargetSpecialThreatCoefficientDefa", TargetSpecialThreatCoefficientDefa);
    TargetStrengthCoefficientDefault = pINI->ReadDouble(section, "TargetStrengthCoefficientDefault", TargetStrengthCoefficientDefault);
    TargetDistanceCoefficientDefault = pINI->ReadDouble(section, "TargetDistanceCoefficientDefault", TargetDistanceCoefficientDefault);
    DumbMyEffectivenessCoefficient   = pINI->ReadDouble(section, "DumbMyEffectivenessCoefficient", DumbMyEffectivenessCoefficient);
    DumbTargetEffectivenessCoefficient = pINI->ReadDouble(section, "DumbTargetEffectivenessCoefficient", DumbTargetEffectivenessCoefficient);
    DumbTargetSpecialThreatCoefficient = pINI->ReadDouble(section, "DumbTargetSpecialThreatCoefficient", DumbTargetSpecialThreatCoefficient);
    DumbTargetStrengthCoefficient    = pINI->ReadDouble(section, "DumbTargetStrengthCoefficient", DumbTargetStrengthCoefficient);
    DumbTargetDistanceCoefficient    = pINI->ReadDouble(section, "DumbTargetDistanceCoefficient", DumbTargetDistanceCoefficient);
    EnemyHouseThreatBonus            = pINI->ReadDouble(section, "EnemyHouseThreatBonus", EnemyHouseThreatBonus);
    VeinholeMonsterStrength          = pINI->ReadInteger(section, "VeinholeMonsterStrength", VeinholeMonsterStrength);
    MaxVeinholeGrowth                = pINI->ReadInteger(section, "MaxVeinholeGrowth", MaxVeinholeGrowth);
    VeinholeGrowthRate               = pINI->ReadInteger(section, "VeinholeGrowthRate", VeinholeGrowthRate);
    VeinholeShrinkRate               = pINI->ReadInteger(section, "VeinholeShrinkRate", VeinholeShrinkRate);
    VeinDamage                       = pINI->ReadInteger(section, "VeinDamage", VeinDamage);
    { char _buf[0x40]; if (pINI->ReadString(section, "VeinholeTypeClass", "", _buf, sizeof(_buf)) > 0) { TerrainTypeClass* _p = TerrainTypeClass::FindOrAllocate(_buf); if (_p) VeinholeTypeClass = _p; } }
    MaxWaypointPathLength            = pINI->ReadInteger(section, "MaxWaypointPathLength", MaxWaypointPathLength);
    TreeStrength                     = pINI->ReadInteger(section, "TreeStrength", TreeStrength);

    pINI->GetPrerequisiteList(section, "PrerequisitePower",    PrerequisitePower);
    pINI->GetPrerequisiteList(section, "PrerequisiteFactory",  PrerequisiteFactory);
    pINI->GetPrerequisiteList(section, "PrerequisiteBarracks", PrerequisiteBarracks);
    pINI->GetPrerequisiteList(section, "PrerequisiteRadar",    PrerequisiteRadar);
    pINI->GetPrerequisiteList(section, "PrerequisiteTech",     PrerequisiteTech);
    pINI->GetPrerequisiteList(section, "PrerequisiteProc",     PrerequisiteProc);

    pINI->GetVectorAircraftType(section, "PadAircraft", PadAircraft);
    pINI->GetVectorUnitType(section, "SecretUnits", SecretUnits);
    pINI->GetVectorBuildType(section, "SecretBuildings", SecretBuildings);

    pINI->GetVectorIntegers(section, "TeamDelays",                      TeamDelays);
    pINI->GetVectorIntegers(section, "AIHateDelays",                    AIHateDelays);
    pINI->GetVectorIntegers(section, "FillEarliestTeamProbability",     FillEarliestTeamProbability);
    pINI->GetVectorIntegers(section, "MinimumAIDefensiveTeams",         MinimumAIDefensiveTeams);
    pINI->GetVectorIntegers(section, "MaximumAIDefensiveTeams",         MaximumAIDefensiveTeams);
    pINI->GetVectorIntegers(section, "TotalAITeamCap",                  TotalAITeamCap);
    pINI->GetVectorIntegers(section, "AlliedBaseDefenseCounts",         AlliedBaseDefenseCounts);
    pINI->GetVectorIntegers(section, "SovietBaseDefenseCounts",         SovietBaseDefenseCounts);
    pINI->GetVectorIntegers(section, "ThirdBaseDefenseCounts",          ThirdBaseDefenseCounts);
    pINI->GetVectorIntegers(section, "AIPickWallDefensePercent",        AIPickWallDefensePercent);
    pINI->GetVectorIntegers(section, "DisabledDisguiseDetectionPercent", DisabledDisguiseDetectionPercent);
    pINI->GetVectorIntegers(section, "AIAutoDeployFrameDelay",          AIAutoDeployFrameDelay);
    pINI->GetVectorIntegers(section, "AISuperDefenseProbability",       AISuperDefenseProbability);
    pINI->GetVectorIntegers(section, "AICaptureNormal",                 AICaptureNormal);
    pINI->GetVectorIntegers(section, "AICaptureWounded",                AICaptureWounded);
    pINI->GetVectorIntegers(section, "AICaptureLowPower",               AICaptureLowPower);
    pINI->GetVectorIntegers(section, "AICaptureLowMoney",               AICaptureLowMoney);
    pINI->GetVectorIntegers(section, "MultiplayerAICM",                 MultiplayerAICM);
    pINI->GetVectorIntegers(section, "AIVirtualPurifiers",              AIVirtualPurifiers);
    pINI->GetVectorIntegers(section, "AISlaveMinerNumber",              AISlaveMinerNumber);
    pINI->GetVectorIntegers(section, "HarvestersPerRefinery",           HarvestersPerRefinery);
    pINI->GetVectorIntegers(section, "AIExtraRefineries",               AIExtraRefineries);

    pINI->GetVectorInfType(section, "AmerParaDropInf",  AmerParaDropInf);
    pINI->GetVectorIntegers(section, "AmerParaDropNum", AmerParaDropNum);
    pINI->GetVectorInfType(section, "AllyParaDropInf",  AllyParaDropInf);
    pINI->GetVectorIntegers(section, "AllyParaDropNum", AllyParaDropNum);
    pINI->GetVectorInfType(section, "SovParaDropInf",   SovParaDropInf);
    pINI->GetVectorIntegers(section, "SovParaDropNum",  SovParaDropNum);
    pINI->GetVectorInfType(section, "YuriParaDropInf",  YuriParaDropInf);
    pINI->GetVectorIntegers(section, "YuriParaDropNum", YuriParaDropNum);
    pINI->GetVectorInfType(section, "AnimToInfantry",   AnimToInfantry);

    pINI->GetVectorIntegers(section, "AIIonCannonConYardValue",     AIIonCannonConYardValue);
    pINI->GetVectorIntegers(section, "AIIonCannonWarFactoryValue",  AIIonCannonWarFactoryValue);
    pINI->GetVectorIntegers(section, "AIIonCannonPowerValue",       AIIonCannonPowerValue);
    pINI->GetVectorIntegers(section, "AIIonCannonTechCenterValue",  AIIonCannonTechCenterValue);
    pINI->GetVectorIntegers(section, "AIIonCannonEngineerValue",    AIIonCannonEngineerValue);
    pINI->GetVectorIntegers(section, "AIIonCannonThiefValue",       AIIonCannonThiefValue);
    pINI->GetVectorIntegers(section, "AIIonCannonHarvesterValue",   AIIonCannonHarvesterValue);
    pINI->GetVectorIntegers(section, "AIIonCannonMCVValue",         AIIonCannonMCVValue);
    pINI->GetVectorIntegers(section, "AIIonCannonAPCValue",         AIIonCannonAPCValue);
    pINI->GetVectorIntegers(section, "AIIonCannonBaseDefenseValue", AIIonCannonBaseDefenseValue);
    pINI->GetVectorIntegers(section, "AIIonCannonPlugValue",        AIIonCannonPlugValue);
    pINI->GetVectorIntegers(section, "AIIonCannonHelipadValue",     AIIonCannonHelipadValue);
    pINI->GetVectorIntegers(section, "AIIonCannonTempleValue",      AIIonCannonTempleValue);

    pINI->GetVectorIntegers(section, "RadarEventSuppressionDistances",  RadarEventSuppressionDistances);
    pINI->GetVectorIntegers(section, "RadarEventVisibilityDurations",   RadarEventVisibilityDurations);
    pINI->GetVectorIntegers(section, "RadarEventDurations",             RadarEventDurations);

    pINI->GetVectorTerrainTypes(section, "DefaultMirageDisguises", DefaultMirageDisguises);
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
    const char* section = "Maximums";
    if (!pINI) return;
    // [Maximums] section: per-type object count caps (Infantry, Units,
    // Building, Aircraft, Vessel, InfantryType, UnitType, BuildingType,
    // AircraftType, VesselType).  These limits are enforced by the
    // type-class Array containers and do not require RulesClass members.

    // ---- rules keys ----
    Players                          = pINI->ReadInteger(section, "Players", Players);
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
// ----------------------------------------------------------------------------
// Read_Sides - RulesClass_Addition_Sides (asm 0x6723BE)
//
//   Every [Sides] key names a side.  One that vec_Sides already carries is
//   reused, otherwise a new SideClass is registered.  The key's value is then
//   re-parsed as the side's comma separated house list through
//   INIClass_ParseSideHouses, and every house that came back stores the
//   side's ordinal in its own Side field.
// ----------------------------------------------------------------------------

void RulesClass::Read_Sides(CCINIClass* pINI)
{
    if (!pINI) return;

    SideClass::ReadSides(pINI);
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

    // ---- rules keys ----
    { char _buf[0x400]; if (pINI->ReadString(section, "BuildConst", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BuildConst.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) BuildConst.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "BuildPower", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BuildPower.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) BuildPower.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "BuildRefinery", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BuildRefinery.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) BuildRefinery.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "BuildBarracks", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BuildBarracks.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) BuildBarracks.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "BuildTech", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BuildTech.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) BuildTech.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "BuildWeapons", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BuildWeapons.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) BuildWeapons.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "AlliedBaseDefenses", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); AlliedBaseDefenses.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) AlliedBaseDefenses.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "SovietBaseDefenses", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); SovietBaseDefenses.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) SovietBaseDefenses.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "ThirdBaseDefenses", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); ThirdBaseDefenses.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) ThirdBaseDefenses.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "BuildDefense", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BuildDefense.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) BuildDefense.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "BuildPDefense", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BuildPDefense.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) BuildPDefense.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "BuildAA", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BuildAA.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) BuildAA.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "BuildHelipad", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BuildHelipad.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) BuildHelipad.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "BuildRadar", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); BuildRadar.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) BuildRadar.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "ConcreteWalls", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); ConcreteWalls.Clear(); for (int32 _t = 0; _t < _n; ++_t) { BuildingTypeClass* _p = BuildingTypeClass::FindOrAllocate(_tok[_t]); if (_p) ConcreteWalls.Add(_p); } } }
    CreditReserve                    = pINI->ReadInteger(section, "CreditReserve", CreditReserve);
    PathDelay                        = pINI->ReadDouble(section, "PathDelay", PathDelay);
    BlockagePathDelay                = pINI->ReadInteger(section, "BlockagePathDelay", BlockagePathDelay);
    AutocreateTime                   = pINI->ReadDouble(section, "AutocreateTime", AutocreateTime);
    CompEasyBonus                    = pINI->ReadBool(section, "CompEasyBonus", CompEasyBonus);
    Paranoid                         = pINI->ReadBool(section, "Paranoid", Paranoid);
    AIBaseSpacing                    = pINI->ReadInteger(section, "AIBaseSpacing", AIBaseSpacing);

    pINI->GetVectorIntegers(section, "AIForcePredictionFudge", AIForcePredictionFudge);
    pINI->GetVectorBuildType(section, "NSGates",              NSGates);
    pINI->GetVectorBuildType(section, "EWGates",              EWGates);
    pINI->GetVectorBuildType(section, "BuildNavalYard",       BuildNavalYard);
    pINI->GetVectorBuildType(section, "BuildDummy",           BuildDummy);
    pINI->GetVectorBuildType(section, "NeutralTechBuildings", NeutralTechBuildings);
}

// ----------------------------------------------------------------------------
// Read_Powerups - RulesClass_Addition_Powerups (asm 0x673E60)
//
//   Gated on the [Powerups] section existing.  For every one of the 19 crate
//   types in strlist_CrateTypes the value is read with the original's
//   "0,NONE" fallback and split on ',' into four fields:
//
//     <weight>,<anim>,<naval:yes|no>,<multiplier[%]>
//
//   Weight becomes an integer, the anim name resolves through
//   AnimClass_FindIndex, naval is a case-insensitive "yes"/"no" toggle and
//   the multiplier is parsed as a float with a trailing '%' dividing by 100.
//   A section that is absent leaves the whole table untouched.
// ----------------------------------------------------------------------------

// The 19 crate names, in the order strlist_CrateTypes stores them.
static const char* const CrateTypeNames[19] = {
    "Money", "Unit", "HealBase", "Cloak", "Explosion", "Napalm", "Squad",
    "Darkness", "Reveal", "Armor", "Speed", "Firepower", "ICBM",
    "Invulnerability", "Veteran", "IonStorm", "Gas", "Tiberium", "Pod"
};

// RulesClass::PowerupWeights / ::PowerupAnims / ::PowerupNaval /
// ::PowerupMultipliers - the four parallel tables the reader fills.
double  PowerupWeights[19];
int32   PowerupAnims[19];
bool    PowerupNaval[19];
double  PowerupMultipliers[19];

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

    // The per-crate table.  Gated on the section existing, exactly as the
    // original checks Find_Section before it walks strlist_CrateTypes.
    if (!pINI->SectionExists(section))
        return;

    for (int32 i = 0; i < 19; ++i) {
        char dest[0x80];
        dest[0] = '\0';

        if (pINI->ReadString(section, CrateTypeNames[i], "0,NONE", dest,
                             sizeof(dest)) <= 0) {
            continue;
        }

        // Field 1 - weight.
        char* pToken = std::strtok(dest, ",");
        if (pToken != nullptr) {
            StringHelpers::Trim(pToken);
            PowerupWeights[i] = std::atoi(pToken);
        }

        // Field 2 - the anim, resolved by name against the anim type list.
        pToken = std::strtok(nullptr, ",");
        if (pToken != nullptr) {
            StringHelpers::Trim(pToken);
            PowerupAnims[i] = AnimTypeClass::FindIndex(pToken);
        }

        // Field 3 - the naval flag, "yes" / "no".
        pToken = std::strtok(nullptr, ",");
        if (pToken != nullptr) {
            StringHelpers::Trim(pToken);
            if (_strcmpi(pToken, "yes") == 0)
                PowerupNaval[i] = 1;
            else if (_strcmpi(pToken, "no") == 0)
                PowerupNaval[i] = 0;
        }

        // Field 4 - the multiplier, optionally suffixed with '%'.
        pToken = std::strtok(nullptr, ",");
        if (pToken != nullptr) {
            if (std::strchr(pToken, '%') != nullptr) {
                PowerupMultipliers[i] = std::atof(pToken) * 1.0e-2;
            } else {
                StringHelpers::Trim(pToken);
                PowerupMultipliers[i] = std::atof(pToken);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Read_LandCharacteristics - [LandCharacteristics] section
//
//   Twelve sections - Clear, Road, Water, Rock, Wall, Tiberium, Beach, Rough,
//   Ice, Railroad, Tunnel, Weeds - are read in LandType order into a flat
//   table.  Each section supplies seven movement multipliers and one build
//   flag; every key falls back to its current value, so a partially specified
//   rules file keeps the previous (or default) settings.
// ----------------------------------------------------------------------------

namespace {

// The sections are read in this exact order; index N fills row N.
const char* const LandCharacteristicsSections[RulesClass::LAND_TYPE_COUNT] = {
    "Clear", "Road", "Water", "Rock", "Wall", "Tiberium",
    "Beach", "Rough", "Ice", "Railroad", "Tunnel", "Weeds"
};

} // namespace

void RulesClass::Read_LandCharacteristics(CCINIClass* pINI)
{
    if (!pINI) return;

    for (int32 i = 0; i < LAND_TYPE_COUNT; ++i) {
        const char* section = LandCharacteristicsSections[i];
        LandTypeCharacteristics& row = LandCharacteristics[i];

        row.Hover       = static_cast<float>(pINI->ReadDouble(section, "Hover", row.Hover));
        row.Foot        = static_cast<float>(pINI->ReadDouble(section, "Foot", row.Foot));
        row.Track       = static_cast<float>(pINI->ReadDouble(section, "Track", row.Track));
        row.Wheel       = static_cast<float>(pINI->ReadDouble(section, "Wheel", row.Wheel));
        row.Float       = static_cast<float>(pINI->ReadDouble(section, "Float", row.Float));
        row.Amphibious  = static_cast<float>(pINI->ReadDouble(section, "Amphibious", row.Amphibious));
        row.FloatBeach  = static_cast<float>(pINI->ReadDouble(section, "FloatBeach", row.FloatBeach));
        row.Buildable   = pINI->ReadBool(section, "Buildable", row.Buildable);
    }
}

// ----------------------------------------------------------------------------
// Get_Movement_Multiplier - movement cost table lookup
//
//   Mirrors the binary's `LandCharacteristics.Foot[land*9 + speed]` indexing:
//   the speed classes are laid out in SpeedType order immediately after the
//   seven named members, so the row is effectively a 9-entry array.
// ----------------------------------------------------------------------------
double RulesClass::Get_Movement_Multiplier(int32 landType, int32 speedType) const
{
    if (landType < 0 || landType >= LAND_TYPE_COUNT) return 0.0;
    if (speedType < 0 || speedType >= 9) return 0.0;

    const LandTypeCharacteristics& row = LandCharacteristics[landType];
    const float* values = &row.Hover;   // Hover, Foot, Track, Wheel, Float,
                                        // Amphibious, FloatBeach, then Buildable
    return static_cast<double>(values[speedType]);
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
void RulesClass::Read_Difficulties(CCINIClass* pINI)
{
    if (!pINI) return;

    static const char* sections[3] = { "Easy", "Normal", "Difficult" };

    for (int32 d = 0; d < 3; ++d)
    {
        const char* section = sections[d];
        DifficultyStruct& diff = Difficulties[d];

        diff.Firepower     = pINI->ReadDouble(section, "FirePower", diff.Firepower);
        diff.GroundSpeed   = pINI->ReadDouble(section, "Groundspeed", diff.GroundSpeed);
        diff.AirSpeed      = pINI->ReadDouble(section, "Airspeed", diff.AirSpeed);
        diff.Armor         = pINI->ReadDouble(section, "Armor", diff.Armor);
        diff.ROF           = pINI->ReadDouble(section, "ROF", diff.ROF);
        diff.Cost          = pINI->ReadDouble(section, "Cost", diff.Cost);
        diff.RepairDelay   = pINI->ReadDouble(section, "RepairDelay", diff.RepairDelay);
        diff.BuildDelay    = pINI->ReadDouble(section, "BuildDelay", diff.BuildDelay);
        diff.BuildSlowdown = pINI->ReadBool(section, "BuildSlowdown", diff.BuildSlowdown);
        diff.BuildTime     = pINI->ReadDouble(section, "BuildTime", diff.BuildTime);
        diff.DestroyWalls  = pINI->ReadBool(section, "DestroyWalls", diff.DestroyWalls);
        diff.ContentScan   = pINI->ReadBool(section, "ContentScan", diff.ContentScan);
    }
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// Read_Movies - [Movies] section
// ----------------------------------------------------------------------------

void RulesClass::Read_Movies(CCINIClass* pINI)
{
    if (!pINI) return;

    // [Movies] section: one key per campaign movie.  Registering the names
    // builds the movie table that the campaign and mission files index into
    // with their Intro / Brief / Win / Lose keys.
    const int32 count = pINI->GetKeyCount("Movies");
    for (int32 i = 0; i < count; ++i)
    {
        const char* pKeyName = pINI->GetKeyName("Movies", i);
        if (pKeyName == nullptr)
            continue;
        MovieClass::Register(pKeyName);
    }
}

// ----------------------------------------------------------------------------
// Read_AdvancedCommandBar - [AdvancedCommandBar] section
// ----------------------------------------------------------------------------

void RulesClass::Read_AdvancedCommandBar(CCINIClass* pINI)
{
    const char* section = "AdvancedCommandBar";
    if (!pINI) return;
    // [AdvancedCommandBar] section: command bar UI configuration.  These
    // settings are consumed directly by the UI system and do not require
    // RulesClass members.

    // ---- rules keys ----
    pINI->ReadString(section, "None", "", NoneValue2, sizeof(NoneValue2));
}

// ----------------------------------------------------------------------------
// Read_HarvesterRules - harvester tuning keys (reside in [General])
// ----------------------------------------------------------------------------

void RulesClass::Read_HarvesterRules(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "General";

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
// RulesClass - Addition_* dispatch family
//
//  RulesClass_Addition (asm 0x6AF6B0) builds the entire rules set by invoking
//  one Addition_* routine per INI section in a fixed order.  The routines
//  themselves are the ones the reconstruction already provides under the
//  Read_* names; the members below carry the original binary's names and the
//  original invocation order so that the load sequence matches the assembly
//  exactly, while Read_* keeps working as the implementation body.
// ============================================================================

// RulesClass_Addition (asm 0x6AF6B0).
//
//  The order below mirrors the sequence of `call RulesClass_Addition_*` sites
//  in the original: the fundamental tables first, then the object type lists,
//  then the tuning sections and finally the command-bar/UI data.
void RulesClass::Addition(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return;

    CreateVectors();

    Addition_SpecialWeapons(pINI);
    Addition_AudioVisual(pINI);
    Addition_CrateRules(pINI);
    Addition_CombatDamage(pINI);
    Addition_Radiation(pINI);
    Addition_ElevationModel(pINI);
    Addition_WallModel(pINI);
    Addition_Difficulty(pINI);
    Addition_Colors(pINI);
    Addition_ColorAdd(pINI);
    Addition_General(pINI);
    Addition_MultiplayerDialogSettings(pINI);
    Addition_Maximums(pINI);
    Addition_InfantryTypes(pINI);
    Addition_Countries(pINI);
    Addition_VehicleTypes(pINI);
    Addition_AircraftTypes(pINI);
    Addition_Sides(pINI);
    Addition_SuperWeaponTypes(pINI);
    Addition_BuildingTypes(pINI);
    Addition_TerrainTypes(pINI);
    Addition_Teams_obsolete(pINI);
    Addition_SmudgeTypes(pINI);
    Addition_OverlayTypes_obsolete(pINI);
    Addition_Animations(pINI);
    Addition_VoxelAnims(pINI);
    Addition_Warheads(pINI);
    Addition_Particles(pINI);
    Addition_ParticleSystems(pINI);
    Addition_AI(pINI);
    Addition_Powerups(pINI);
    Addition_LandCharacteristics(pINI);
    Addition_IQ(pINI);
    Addition_Movies(pINI);
    Addition_AdvancedCommandBar(pINI);
}

// RulesClass_CreateVectors (asm 0x6B1BC0).
//
//  Allocates every per-object vector the rules own.  The reconstruction keeps
//  its arrays as members that are constructed with the instance, so nothing
//  needs allocating here; the entry point exists so the load sequence is
//  identical.
void RulesClass::CreateVectors()
{
}

void RulesClass::Addition_InfantryTypes(CCINIClass* pINI)   { Read_InfantryTypes(pINI); }
void RulesClass::Addition_Countries(CCINIClass* pINI)       { Read_Countries(pINI); }
void RulesClass::Addition_VehicleTypes(CCINIClass* pINI)    { Read_VehicleTypes(pINI); }
void RulesClass::Addition_AircraftTypes(CCINIClass* pINI)   { Read_AircraftTypes(pINI); }
void RulesClass::Addition_Sides(CCINIClass* pINI)           { Read_Sides(pINI); }
void RulesClass::Addition_SuperWeaponTypes(CCINIClass* pINI){ Read_SuperWeaponTypes(pINI); }
void RulesClass::Addition_BuildingTypes(CCINIClass* pINI)   { Read_BuildingTypes(pINI); }
void RulesClass::Addition_TerrainTypes(CCINIClass* pINI)    { Read_TerrainTypes(pINI); }
void RulesClass::Addition_SmudgeTypes(CCINIClass* pINI)     { Read_SmudgeTypes(pINI); }
void RulesClass::Addition_Animations(CCINIClass* pINI)      { Read_Animations(pINI); }
void RulesClass::Addition_VoxelAnims(CCINIClass* pINI)      { Read_VoxelAnims(pINI); }
void RulesClass::Addition_Warheads(CCINIClass* pINI)        { Read_Warheads(pINI); }
void RulesClass::Addition_Particles(CCINIClass* pINI)       { Read_Particles(pINI); }
void RulesClass::Addition_ParticleSystems(CCINIClass* pINI) { Read_ParticleSystems(pINI); }
void RulesClass::Addition_AI(CCINIClass* pINI)              { Read_AI(pINI); }
void RulesClass::Addition_Powerups(CCINIClass* pINI)        { Read_Powerups(pINI); }
void RulesClass::Addition_LandCharacteristics(CCINIClass* pINI) { Read_LandCharacteristics(pINI); }
void RulesClass::Addition_IQ(CCINIClass* pINI)              { Read_IQ(pINI); }
void RulesClass::Addition_Movies(CCINIClass* pINI)          { Read_Movies(pINI); }
void RulesClass::Addition_AdvancedCommandBar(CCINIClass* pINI) { Read_AdvancedCommandBar(pINI); }
void RulesClass::Addition_General(CCINIClass* pINI)         { Read_General(pINI); }
void RulesClass::Addition_CombatDamage(CCINIClass* pINI)    { Read_CombatDamage(pINI); }
void RulesClass::Addition_Radiation(CCINIClass* pINI)       { Read_Radiation(pINI); }
void RulesClass::Addition_ElevationModel(CCINIClass* pINI)  { Read_ElevationModel(pINI); }
void RulesClass::Addition_WallModel(CCINIClass* pINI)       { Read_WallModel(pINI); }
void RulesClass::Addition_Colors(CCINIClass* pINI)          { Read_Colors(pINI); }
void RulesClass::Addition_ColorAdd(CCINIClass* pINI)        { Read_ColorAdd(pINI); }
void RulesClass::Addition_Difficulty(CCINIClass* pINI)      { Read_Difficulties(pINI); }
void RulesClass::Addition_MultiplayerDialogSettings(CCINIClass* pINI) { Read_MultiplayerDialogSettings(pINI); }
void RulesClass::Addition_Maximums(CCINIClass* pINI)        { Read_Maximums(pINI); }
void RulesClass::Addition_SpecialWeapons(CCINIClass* pINI)  { Read_SuperWeaponTypes(pINI); }
void RulesClass::Addition_CrateRules(CCINIClass* pINI)      { Read_CrateRules(pINI); }
void RulesClass::Addition_AudioVisual(CCINIClass* pINI)     { Read_AudioVisual(pINI); }
void RulesClass::Addition_Teams_obsolete(CCINIClass* pINI)  { (void)pINI; }
void RulesClass::Addition_OverlayTypes_obsolete(CCINIClass* pINI) { Read_OverlayTypes(pINI); }
void RulesClass::LoadDifficulties_unused(CCINIClass* pINI)  { Read_Difficulties(pINI); }
