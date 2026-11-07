#include <Rules/RulesClass.h>
#include <IO/MovieClass.h>
#include <Audio/VocClass.h>
#include <Combat/BulletTypeClass.h>
#include <Rendering/ConvertClass.h>
#include <Core/Definitions.h>
#include <INI/INIClass.h>
#include <IO/MixFileClass.h>
#include <IO/FileSystem.h>
#include <FileFormats/SHP.h>

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
#include <AI/TeamTypeClass.h>
#include <Game/Game.h>


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

    // ------------------------------------------------------------------------
    // ReadSoundName - "声音名 -> VOC 索引" 读取（asm: RulesClass_Addition_AudioVisual）
    //
    //   原版对 [AudioVisual] 里的每一个声音键都内联展开同一段控制流：
    //
    //     fallback = *field;                       ; ebx = 字段旧值
    //     if (GetString(section,key,"",Str,0x80)) {
    //         i = VocClass_FindIndexOfName(Str);   ; asm: VocClass_FindIndexOfName
    //         if (i != -1) fallback = i;           ; cmp eax,-1 / jnz
    //     }
    //     *field = fallback;
    //
    //   因此语义是：键缺失 -> 保留旧值；名字未知（-1）-> 同样保留旧值。
    //   只有"读到已知名字"才会改写字段。旧实现把这些键当整数直接 ReadInteger，
    //   与 rules(md).ini 里存的名字串不兼容，此处按 asm 修正。
    // ------------------------------------------------------------------------
    int32 ReadSoundName(CCINIClass* pINI, const char* pSection,
                        const char* pKey, int32 nFallback)
    {
        char buffer[0x80];
        if (pINI->ReadString(pSection, pKey, "", buffer, sizeof(buffer)) > 0)
        {
            const int32 idx = VocClass::FindIndexOfName(buffer);
            if (idx >= 0)
                return idx;
        }
        return nFallback;
    }

    // ------------------------------------------------------------------------
    // 为什么本文件里所有"小数"键都走 ReadFixed 而不是 ReadDouble
    // ------------------------------------------------------------------------
    //   逐键核对 asm 后可以确定：整个 gamemd.exe 里**不存在** GetDouble
    //   （`grep -c GetDouble` == 0），rules 的每一个浮点键都是
    //   `INIClass_GetFixed`（asm:649584）。两者的差别不只是名字：
    //
    //     INIClass_GetFixed:
    //         sscanf(value, "%f", &tmp);           // 先落到 float
    //         if (strchr(value, '%')) tmp *= 0.01; // 百分号当 1/100 处理
    //         return (double)tmp;
    //     ReadDouble:
    //         return strtod(value, nullptr);       // 完全不认 '%'
    //
    //   rules(md).ini 里 `RefundPercent=50%`、`AIUseTurbineUpgradeProbability=75%`
    //   这类带百分号的值很常见：用 ReadDouble 会拿到 50.0 而不是 0.5，整整差
    //   一百倍。因此本文件统一用 ReadFixed，且**不要**改回 ReadDouble。
    //   （ReadFixed 的实现在 INI/INIClass.cpp:951，逐句对齐上面那段 asm。）
    // ------------------------------------------------------------------------
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
    , ComputerBaseDefenseResponse(0), MaximumBaseDefenseValue(0),
      BaseDefenseSuspendSeconds(0.0)
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
    , AdvancedCommandBarButtonCount(0), AdvancedCommandBarIsMP(false)
{
    // 命令栏按钮名表先清空（容量固定 0x19）。
    for (int32 i = 0; i < ADVANCED_COMMAND_BAR_MAX; ++i)
        AdvancedCommandBarButtons[i][0] = '\0';

    // Initialize the [LandCharacteristics] table.  Every movement multiplier
    // defaults to 1.0 (fully passable) and every terrain is buildable until a
    // rules file says otherwise - that mirrors the binary, where a missing
    // section leaves the previous value untouched.
    for (int32 i = 0; i < LAND_TYPE_COUNT; ++i) {
        LandCharacteristics[i].Foot       = 1.0f;
        LandCharacteristics[i].Track      = 1.0f;
        LandCharacteristics[i].Wheel      = 1.0f;
        LandCharacteristics[i].Hover      = 1.0f;
        LandCharacteristics[i].Winged     = 1.0f;
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

// asm: RulesClass_Init (0x6686A0, retn 4) —— 整份规则表的"从头来一遍"入口。
//   原版控制流：
//     1. 把全局配色表 ColorScheme 逐个 Remove + delete，末尾把 length 归零；
//        紧跟两次调色板缓存重置（sub_627F00 / sub_6269A0）。
//     2. 把二十来个类型表（TeamTypes / TaskForces / ScriptTypes / OverlayType …
//        ）里的对象逐个走 vtbl+0x20(arg=1)，相当于清空注册表。
//     3. Find_Section("Maximums") 后 GetInteger("Players") —— 单独读一次人数上限。
//     4. call RulesClass_Addition(pINI) —— 主规则表整段读取。
//     5. 打开 LANGRULE.INI：Is_Available 为真时 CCINIClass::Load 进来，
//        **再跑一遍 RulesClass_Addition**（本地化表是主表的增量补丁）。
//     6. 收尾（MPGameMode 构造等，见 Init_Rules）。
//   重建侧的差异：类型注册表由各 XXXTypeClass::Array 自持，析构时统一释放，
//   故第 2 步不在此重复销毁（否则会与持有者形成双重释放）；配色表是
//   ColorScheme 自己的静态数组，按 asm 清理是安全的。
void RulesClass::Init(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return;

    // 1) 清空全局配色表。
    if (ColorScheme::Array != nullptr)
    {
        for (int32 i = 0; i < ColorScheme::Array->Count; ++i)
            delete (*ColorScheme::Array)[i];
        ColorScheme::Array->Clear();
    }

    // 3) [Maximums] Players —— Addition 之前单独读。
    Read_Maximums(pINI);

    // 4) 主规则表。
    Addition(pINI);

    // 5) LANGRULE.INI 增量补丁表。
    CCFileClass langRuleFile("LANGRULE.INI");
    if (langRuleFile.IsAvailable())
    {
        CCINIClass langRuleINI;
        if (langRuleINI.ReadCCFile(&langRuleFile, false, false))
        {
            Addition(&langRuleINI);
        }
        langRuleFile.Close();
    }

    // 6) 不属于 RulesClass_Addition 的三段（原版由 Init_Rules / InitGame 另调）。
    Read_TileTypes(pINI);
    Read_MissionControl(pINI);
    Read_Movies(pINI);
}

void RulesClass::Read_File(CCINIClass* pINI)
{
    if (!pINI) return;

    // 与 RulesClass::Addition 保持同一顺序，对齐 asm RulesClass_Addition。
    Read_Colors(pINI);
    Read_ColorAdd(pINI);
    Read_Countries(pINI);
    Read_Sides(pINI);
    Read_OverlayTypes(pINI);
    Read_SuperWeaponTypes(pINI);
    Read_Warheads(pINI);
    Read_Projectiles(pINI);
    Read_WeaponTypes(pINI);
    Read_SmudgeTypes(pINI);
    Read_TerrainTypes(pINI);
    Read_BuildingTypes(pINI);
    Read_VehicleTypes(pINI);
    Read_AircraftTypes(pINI);
    Read_InfantryTypes(pINI);
    Read_Animations(pINI);
    Read_VoxelAnims(pINI);
    Read_Particles(pINI);
    Read_ParticleSystems(pINI);
    Read_JumpjetControls(pINI);
    Read_MultiplayerDialogSettings(pINI);
    Read_AI(pINI);
    Read_Powerups(pINI);
    Read_LandCharacteristics(pINI);
    Read_IQ(pINI);
    Read_General(pINI);
    Read_Difficulties(pINI);
    Read_CrateRules(pINI);
    Read_CombatDamage(pINI);
    Read_Radiation(pINI);
    Read_ElevationModel(pINI);
    Read_WallModel(pINI);
    Read_AudioVisual(pINI);
    Read_SpecialWeapons(pINI);
    Read_Tiberiums(pINI);
    Read_AdvancedCommandBar(pINI);
    Read_Maximums(pINI);
    Read_TileTypes(pINI);
    Read_Movies(pINI);
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
 // Read_SpecialWeapons - RulesClass_Addition_SpecialWeapons
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

    // asm: RulesClass_Addition_AudioVisual 的第一条指令就是 Find_Section("...")：
    // 段不存在时整段跳过。缺这条门会让每个键都走"缺省=字段原值"的路径——
    // 看着等价，但向量键会先 Clear() 再填，段缺失就会把已有列表清空。
    if (!pINI->SectionExists(section))
        return;

    // Sound mappings —— 全部走"声音名 -> VOC 索引"（asm: GetString + VocClass_FindIndexOfName）。
    // 旧实现误用 ReadInteger，此处逐键按 asm 修正；SpyPlaneCameraFrames 仍是 GetInteger。
    DigSound                    = ReadSoundName(pINI, section, "DigSound", DigSound);
    CreateUnitSound             = ReadSoundName(pINI, section, "CreateUnitSound", CreateUnitSound);
    CreateInfantrySound         = ReadSoundName(pINI, section, "CreateInfantrySound", CreateInfantrySound);
    CreateAircraftSound         = ReadSoundName(pINI, section, "CreateAircraftSound", CreateAircraftSound);
    BaseUnderAttackSound        = ReadSoundName(pINI, section, "BaseUnderAttackSound", BaseUnderAttackSound);
    GUIMainButtonSound          = ReadSoundName(pINI, section, "GUIMainButtonSound", GUIMainButtonSound);
    GUIBuildSound               = ReadSoundName(pINI, section, "GUIBuildSound", GUIBuildSound);
    GUITabSound                 = ReadSoundName(pINI, section, "GUITabSound", GUITabSound);
    GUIOpenSound                = ReadSoundName(pINI, section, "GUIOpenSound", GUIOpenSound);
    GUICloseSound               = ReadSoundName(pINI, section, "GUICloseSound", GUICloseSound);
    GUIMoveOutSound             = ReadSoundName(pINI, section, "GUIMoveOutSound", GUIMoveOutSound);
    GUIMoveInSound              = ReadSoundName(pINI, section, "GUIMoveInSound", GUIMoveInSound);
    GUIComboOpenSound           = ReadSoundName(pINI, section, "GUIComboOpenSound", GUIComboOpenSound);
    GUIComboCloseSound          = ReadSoundName(pINI, section, "GUIComboCloseSound", GUIComboCloseSound);
    GUICheckboxSound            = ReadSoundName(pINI, section, "GUICheckboxSound", GUICheckboxSound);
    ScoreAnimSound              = ReadSoundName(pINI, section, "ScoreAnimSound", ScoreAnimSound);
    IFVTransformSound           = ReadSoundName(pINI, section, "IFVTransformSound", IFVTransformSound);
    PsychicSensorDetectSound    = ReadSoundName(pINI, section, "PsychicSensorDetectSound", PsychicSensorDetectSound);
    BuildingGarrisonedSound     = ReadSoundName(pINI, section, "BuildingGarrisonedSound", BuildingGarrisonedSound);
    BuildingAbandonedSound      = ReadSoundName(pINI, section, "BuildingAbandonedSound", BuildingAbandonedSound);
    BuildingRepairedSound       = ReadSoundName(pINI, section, "BuildingRepairedSound", BuildingRepairedSound);
    CheerSound                  = ReadSoundName(pINI, section, "CheerSound", CheerSound);
    PlaceBeaconSound            = ReadSoundName(pINI, section, "PlaceBeaconSound", PlaceBeaconSound);
    DefaultChronoSound          = ReadSoundName(pINI, section, "DefaultChronoSound", DefaultChronoSound);
    StartPlanningModeSound      = ReadSoundName(pINI, section, "StartPlanningModeSound", StartPlanningModeSound);
    AddPlanningModeCommandSound = ReadSoundName(pINI, section, "AddPlanningModeCommandSound", AddPlanningModeCommandSound);
    ExecutePlanSound            = ReadSoundName(pINI, section, "ExecutePlanSound", ExecutePlanSound);
    EndPlanningModeSound        = ReadSoundName(pINI, section, "EndPlanningModeSound", EndPlanningModeSound);
    CrateMoneySound             = ReadSoundName(pINI, section, "CrateMoneySound", CrateMoneySound);
    CrateRevealSound            = ReadSoundName(pINI, section, "CrateRevealSound", CrateRevealSound);
    CrateFireSound              = ReadSoundName(pINI, section, "CrateFireSound", CrateFireSound);
    CrateArmourSound            = ReadSoundName(pINI, section, "CrateArmourSound", CrateArmourSound);
    CrateSpeedSound             = ReadSoundName(pINI, section, "CrateSpeedSound", CrateSpeedSound);
    CrateUnitSound              = ReadSoundName(pINI, section, "CrateUnitSound", CrateUnitSound);
    CratePromoteSound           = ReadSoundName(pINI, section, "CratePromoteSound", CratePromoteSound);
    ImpactWaterSound            = ReadSoundName(pINI, section, "ImpactWaterSound", ImpactWaterSound);
    ImpactLandSound             = ReadSoundName(pINI, section, "ImpactLandSound", ImpactLandSound);
    SinkingSound                = ReadSoundName(pINI, section, "SinkingSound", SinkingSound);
    BombTickingSound            = ReadSoundName(pINI, section, "BombTickingSound", BombTickingSound);
    BombAttachSound             = ReadSoundName(pINI, section, "BombAttachSound", BombAttachSound);
    YuriMindControlSound        = ReadSoundName(pINI, section, "YuriMindControlSound", YuriMindControlSound);
    ChronoInSound               = ReadSoundName(pINI, section, "ChronoInSound", ChronoInSound);
    ChronoOutSound              = ReadSoundName(pINI, section, "ChronoOutSound", ChronoOutSound);
    SpySatActivationSound       = ReadSoundName(pINI, section, "SpySatActivationSound", SpySatActivationSound);
    SpySatDeactivationSound     = ReadSoundName(pINI, section, "SpySatDeactivationSound", SpySatDeactivationSound);
    UpgradeVeteranSound         = ReadSoundName(pINI, section, "UpgradeVeteranSound", UpgradeVeteranSound);
    UpgradeEliteSound           = ReadSoundName(pINI, section, "UpgradeEliteSound", UpgradeEliteSound);
    VoiceIFVRepair              = ReadSoundName(pINI, section, "VoiceIFVRepair", VoiceIFVRepair);
    SlavesFreeSound             = ReadSoundName(pINI, section, "SlavesFreeSound", SlavesFreeSound);
    SlaveMinerDeploySound       = ReadSoundName(pINI, section, "SlaveMinerDeploySound", SlaveMinerDeploySound);
    SlaveMinerUndeploySound     = ReadSoundName(pINI, section, "SlaveMinerUndeploySound", SlaveMinerUndeploySound);
    BunkerWallsUpSound          = ReadSoundName(pINI, section, "BunkerWallsUpSound", BunkerWallsUpSound);
    BunkerWallsDownSound        = ReadSoundName(pINI, section, "BunkerWallsDownSound", BunkerWallsDownSound);
    RepairBridgeSound           = ReadSoundName(pINI, section, "RepairBridgeSound", RepairBridgeSound);
    PsychicDominatorActivateSound = ReadSoundName(pINI, section, "PsychicDominatorActivateSound", PsychicDominatorActivateSound);
    GeneticMutatorActivateSound = ReadSoundName(pINI, section, "GeneticMutatorActivateSound", GeneticMutatorActivateSound);
    PsychicRevealActivateSound  = ReadSoundName(pINI, section, "PsychicRevealActivateSound", PsychicRevealActivateSound);
    MasterMindOverloadDeathSound = ReadSoundName(pINI, section, "MasterMindOverloadDeathSound", MasterMindOverloadDeathSound);
    AirstrikeAbortSound         = ReadSoundName(pINI, section, "AirstrikeAbortSound", AirstrikeAbortSound);
    AirstrikeAttackVoice        = ReadSoundName(pINI, section, "AirstrikeAttackVoice", AirstrikeAttackVoice);
    MindClearedSound            = ReadSoundName(pINI, section, "MindClearedSound", MindClearedSound);
    EnterGrinderSound           = ReadSoundName(pINI, section, "EnterGrinderSound", EnterGrinderSound);
    LeaveGrinderSound           = ReadSoundName(pINI, section, "LeaveGrinderSound", LeaveGrinderSound);
    EnterBioReactorSound        = ReadSoundName(pINI, section, "EnterBioReactorSound", EnterBioReactorSound);
    LeaveBioReactorSound        = ReadSoundName(pINI, section, "LeaveBioReactorSound", LeaveBioReactorSound);
    ActivateSound               = ReadSoundName(pINI, section, "ActivateSound", ActivateSound);
    DeactivateSound             = ReadSoundName(pINI, section, "DeactivateSound", DeactivateSound);
    SpyPlaneCamera              = ReadSoundName(pINI, section, "SpyPlaneCamera", SpyPlaneCamera);
    LetsDoTheTimeWarpOutAgain   = ReadSoundName(pINI, section, "LetsDoTheTimeWarpOutAgain", LetsDoTheTimeWarpOutAgain);
    LetsDoTheTimeWarpInAgain    = ReadSoundName(pINI, section, "LetsDoTheTimeWarpInAgain", LetsDoTheTimeWarpInAgain);
    DiskLaserChargeUp           = ReadSoundName(pINI, section, "DiskLaserChargeUp", DiskLaserChargeUp);
    SpyPlaneCameraFrames        = pINI->ReadInteger(section, "SpyPlaneCameraFrames", SpyPlaneCameraFrames);

    // Visual properties
    // RadarEventSpeed / RadarEventRotationSpeed / RadarEventMinRadius /
    // RadarEventColorSpeed / FlashFrameTime / RadarCombatFlashTime /
    // MaxWaypointPathLength 属于 [General]（asm: RulesClass_Addition_General），
    // 已由 Read_General 读取，此处不再按 [AudioVisual] 段重复读。

    // Shell sounds —— 同为"声音名 -> VOC 索引"（asm: GetString + VocClass_FindIndexOfName）。
    BuildingDieSound            = ReadSoundName(pINI, section, "BuildingDieSound", BuildingDieSound);
    BuildingSlam                = ReadSoundName(pINI, section, "BuildingSlam", BuildingSlam);
    RadarOn                     = ReadSoundName(pINI, section, "RadarOn", RadarOn);
    RadarOff                    = ReadSoundName(pINI, section, "RadarOff", RadarOff);
    MovieOn                     = ReadSoundName(pINI, section, "MovieOn", MovieOn);
    MovieOff                    = ReadSoundName(pINI, section, "MovieOff", MovieOff);
    ScoldSound                  = ReadSoundName(pINI, section, "ScoldSound", ScoldSound);
    TeslaCharge                 = ReadSoundName(pINI, section, "TeslaCharge", TeslaCharge);
    TeslaZap                    = ReadSoundName(pINI, section, "TeslaZap", TeslaZap);
    GenericClick                = ReadSoundName(pINI, section, "GenericClick", GenericClick);
    GenericBeep                 = ReadSoundName(pINI, section, "GenericBeep", GenericBeep);
    BuildingDamageSound         = ReadSoundName(pINI, section, "BuildingDamageSound", BuildingDamageSound);
    HealCrateSound              = ReadSoundName(pINI, section, "HealCrateSound", HealCrateSound);
    ChuteSound                  = ReadSoundName(pINI, section, "ChuteSound", ChuteSound);
    StopSound                   = ReadSoundName(pINI, section, "StopSound", StopSound);
    GuardSound                  = ReadSoundName(pINI, section, "GuardSound", GuardSound);
    ScatterSound                 = ReadSoundName(pINI, section, "ScatterSound", ScatterSound);
    // 注: "DeploySound" 不是 [AudioVisual] 的键——asm 里它是 TechnoTypeClass
    // 的字段（TechnoTypeClass_LoadFromINI 从**单位自己的段**读，asm:1607977），
    // 早期重建误把它当成规则段键，按铁律③删除。RulesClass::DeploySound 字段
    // 在本节没有对应的 INI 键。
    StormSound                  = ReadSoundName(pINI, section, "StormSound", StormSound);
    ShellButtonSlideSound       = ReadSoundName(pINI, section, "ShellButtonSlideSound", ShellButtonSlideSound);
    CloakSound                  = ReadSoundName(pINI, section, "CloakSound", CloakSound);
    SellSound                   = ReadSoundName(pINI, section, "SellSound", SellSound);

    // Multiplayer sounds —— 同为"声音名 -> VOC 索引"。
    GameClosed                  = ReadSoundName(pINI, section, "GameClosed", GameClosed);
    IncomingMessage             = ReadSoundName(pINI, section, "IncomingMessage", IncomingMessage);
    SystemError                 = ReadSoundName(pINI, section, "SystemError", SystemError);
    OptionsChanged              = ReadSoundName(pINI, section, "OptionsChanged", OptionsChanged);
    GameForming                 = ReadSoundName(pINI, section, "GameForming", GameForming);
    PlayerLeft                  = ReadSoundName(pINI, section, "PlayerLeft", PlayerLeft);
    PlayerJoined                = ReadSoundName(pINI, section, "PlayerJoined", PlayerJoined);
    MessageCharTyped            = ReadSoundName(pINI, section, "MessageCharTyped", MessageCharTyped);
    Construction                = ReadSoundName(pINI, section, "Construction", Construction);

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
    ScrollMultiplier                 = pINI->ReadFixed(section, "ScrollMultiplier", ScrollMultiplier);
    ShakeScreen                      = pINI->ReadInteger(section, "ShakeScreen", ShakeScreen);
    { char _buf[0x40]; if (pINI->ReadString(section, "BuildingDrop", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) BuildingDrop = _i; } }
    { char _buf[0x400]; if (pINI->ReadString(section, "TreeFire", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); TreeFire.Clear(); for (int32 _t = 0; _t < _n; ++_t) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) TreeFire.Add(_p); } } }
    { char _buf[0x400]; if (pINI->ReadString(section, "OnFire", "", _buf, sizeof(_buf)) > 0) { char* _tok[64]; int32 _n = SplitCommaList(_buf, _tok, 64); OnFire.Clear(); for (int32 _t = 0; _t < _n; ++_t) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_tok[_t]); if (_p) OnFire.Add(_p); } } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Smoke", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) Smoke = _p; } }
    EliteFlashTimer                  = pINI->ReadInteger(section, "EliteFlashTimer", EliteFlashTimer);
    { char _buf[0x40]; if (pINI->ReadString(section, "SmallFire", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) SmallFire = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "LargeFire", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) LargeFire = _p; } }
    AllyReveal                       = pINI->ReadBool(section, "AllyReveal", AllyReveal);
    ConditionRed                     = pINI->ReadFixed(section, "ConditionRed", ConditionRed);
    ConditionYellow                  = pINI->ReadFixed(section, "ConditionYellow", ConditionYellow);
    EnemyHealth                      = pINI->ReadBool(section, "EnemyHealth", EnemyHealth);
    Gravity                          = pINI->ReadInteger(section, "Gravity", Gravity);
    IdleActionFrequency              = pINI->ReadFixed(section, "IdleActionFrequency", IdleActionFrequency);
    MessageDelay                     = pINI->ReadFixed(section, "MessageDelay", MessageDelay);
    MovieTime                        = pINI->ReadFixed(section, "MovieTime", MovieTime);
    NamedCivilians                   = pINI->ReadBool(section, "NamedCivilians", NamedCivilians);
    SavourDelay                      = pINI->ReadFixed(section, "SavourDelay", SavourDelay);
    ShroudRate                       = pINI->ReadFixed(section, "ShroudRate", ShroudRate);
    FogRate                          = pINI->ReadFixed(section, "FogRate", FogRate);
    VeinGrowthRate                   = pINI->ReadFixed(section, "VeinGrowthRate", VeinGrowthRate);
    IceGrowthRate                    = pINI->ReadFixed(section, "IceGrowthRate", IceGrowthRate);
    IceSolidifyFrameTime             = pINI->ReadInteger(section, "IceSolidifyFrameTime", IceSolidifyFrameTime);
    AmbientChangeRate                = pINI->ReadFixed(section, "AmbientChangeRate", AmbientChangeRate);
    AmbientChangeStep                = pINI->ReadFixed(section, "AmbientChangeStep", AmbientChangeStep);
    SpeakDelay                       = pINI->ReadFixed(section, "SpeakDelay", SpeakDelay);
    TimerWarning                     = pINI->ReadFixed(section, "TimerWarning", TimerWarning);
    ExtraUnitLight                   = static_cast<float>(pINI->ReadFixed(section, "ExtraUnitLight", ExtraUnitLight));
    ExtraInfantryLight               = static_cast<float>(pINI->ReadFixed(section, "ExtraInfantryLight", ExtraInfantryLight));
    ExtraAircraftLight               = static_cast<float>(pINI->ReadFixed(section, "ExtraAircraftLight", ExtraAircraftLight));
    pINI->Get3Bytes(section, "LocalRadarColor", reinterpret_cast<uint8*>(&LocalRadarColor));
    pINI->Get3Bytes(section, "LineTrailColorOverride", reinterpret_cast<uint8*>(&LineTrailColorOverride));
    pINI->Get3Bytes(section, "ChronoBeamColor", reinterpret_cast<uint8*>(&ChronoBeamColor));
    pINI->Get3Bytes(section, "MagnaBeamColor", reinterpret_cast<uint8*>(&MagnaBeamColor));
    OreTwinkleChance                 = pINI->ReadInteger(section, "OreTwinkleChance", OreTwinkleChance);
    LaserTargetColor                 = pINI->ReadInteger(section, "LaserTargetColor", LaserTargetColor);
    IronCurtainColor                 = pINI->ReadInteger(section, "IronCurtainColor", IronCurtainColor);
    BerserkColor                     = pINI->ReadInteger(section, "BerserkColor", BerserkColor);
    ForceShieldColor                 = pINI->ReadInteger(section, "ForceShieldColor", ForceShieldColor);
    DirectRockingCoefficient         = static_cast<float>(pINI->ReadFixed(section, "DirectRockingCoefficient", DirectRockingCoefficient));
    FallBackCoefficient              = static_cast<float>(pINI->ReadFixed(section, "FallBackCoefficient", FallBackCoefficient));

    // ---- asm: RulesClass_Addition_AudioVisual 尾部 ----
    // DropZoneRadius 用 CCINIClass::Get_Lepton 读整数（asm @ +0x14BC）。
    DropZoneRadius                   = pINI->ReadLepton(section, "DropZoneRadius", DropZoneRadius);
    // 三个 VOC 索引向量：asm 对这三个键（CreditTicks @1274272、
    // LightningSounds @1274806、IceCrackSounds @1275360）**都**调
    // INIClass_Get_Vector_Voc_525430 —— 值是逗号分隔的**声音名**，逐个查
    // VocClass 名字表换成索引；查不到的名字丢弃、不占位。旧代码用
    // GetVectorIntegers（atoi 名字→0），会把名字全判成 0 号音效，此处改回
    // 名字解析路径。
    pINI->Get_Vector_Voc(section, "CreditTicks",     CreditTicks);
    pINI->Get_Vector_Voc(section, "LightningSounds", LightningSounds);
    pINI->Get_Vector_Voc(section, "IceCrackSounds",  IceCrackSounds);
}

// ============================================================================
// Read_CrateRules - [CrateRules] section
// ============================================================================

void RulesClass::Read_CrateRules(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "CrateRules";

    // asm: RulesClass_Addition_CrateRules 的第一条指令就是 Find_Section("...")：
    // 段不存在时整段跳过。缺这条门会让每个键都走"缺省=字段原值"的路径——
    // 看着等价，但向量键会先 Clear() 再填，段缺失就会把已有列表清空。
    if (!pINI->SectionExists(section))
        return;

    CrateMinimum    = pINI->ReadInteger(section, "CrateMinimum", CrateMinimum);
    CrateMaximum    = pINI->ReadInteger(section, "CrateMaximum", CrateMaximum);
    CrateRadius     = pINI->ReadLepton(section, "CrateRadius", CrateRadius);   // asm: Get_Lepton
    SoloCrateMoney  = pINI->ReadInteger(section, "SoloCrateMoney", SoloCrateMoney);
    // AmmoCrateDamage 在 asm 里属于 [CombatDamage]（Read_CombatDamage），
    // [CrateRules] 段没有这个键，此处移除误读。
    CrateRegen      = pINI->ReadFixed(section, "CrateRegen", CrateRegen);

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

    // asm: RulesClass_Addition_CombatDamage 的第一条指令就是 Find_Section("...")：
    // 段不存在时整段跳过。缺这条门会让每个键都走"缺省=字段原值"的路径——
    // 看着等价，但向量键会先 Clear() 再填，段缺失就会把已有列表清空。
    if (!pINI->SectionExists(section))
        return;

    MinDamage            = pINI->ReadInteger(section, "MinDamage", MinDamage);
    MaxDamage            = pINI->ReadInteger(section, "MaxDamage", MaxDamage);
    // RepairStep / RepairPercent / IRepairStep / RepairRate / URepairRate /
    // IRepairRate / ConditionRedSparkingProbability /
    // ConditionYellowSparkingProbability 属于 [General]；
    // ConditionYellow / ConditionRed / IdleActionFrequency 属于 [AudioVisual]。
    // 三者分别由 Read_General / Read_AudioVisual 读取，此处不再重复。
    // CloseEnough / Stray / RelaxedStray / GuardModeStray 在 asm 里由
    // RulesClass_Addition_General（str_General）读取，已移至 Read_General。
    // asm: RulesClass_Addition_CombatDamage 用 CCINIClass::Get_Lepton 读这些
    // lepton 量（整数，等价 ReadLepton）：Crush / FireSupress / HomingScatter /
    // BallisticScatter，均非 GetInteger。
    Crush                = pINI->ReadLepton(section, "Crush", Crush);
    FireSupress          = pINI->ReadLepton(section, "FireSupress", FireSupress);
    TiberiumExplosionDamage = pINI->ReadInteger(section, "TiberiumExplosionDamage", TiberiumExplosionDamage);
    TiberiumStrength     = pINI->ReadInteger(section, "TiberiumStrength", TiberiumStrength);
    AtomDamage           = pINI->ReadInteger(section, "AtomDamage", AtomDamage);
    HomingScatter        = pINI->ReadLepton(section, "HomingScatter", HomingScatter);
    BallisticScatter     = pINI->ReadLepton(section, "BallisticScatter", BallisticScatter);
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
    // asm: RulesClass_Addition_CombatDamage —— 紧接 IvanTimedDelay 之后用
    // MixFileClass<CCFileClass>::Finder(..., 0) 载入两张光标 SHP，分别存入
    // +0x0FE0 / +0x0FE4（BOMBCURS_SHP / CHRONOSK_SHP）。这里等价用 Retrieve。
    BOMBCURS_SHP                     = Load_SHP("BOMBCURS.SHP");
    CHRONOSK_SHP                     = Load_SHP("CHRONOSK.SHP");
    IvanIconFlickerRate              = pINI->ReadInteger(section, "IvanIconFlickerRate", IvanIconFlickerRate);
    IronCurtainDuration              = pINI->ReadInteger(section, "IronCurtainDuration", IronCurtainDuration);
    PsychicRevealRadius              = pINI->ReadInteger(section, "PsychicRevealRadius", PsychicRevealRadius);
    OccupyDamageMultiplier           = static_cast<float>(pINI->ReadFixed(section, "OccupyDamageMultiplier", OccupyDamageMultiplier));
    OccupyROFMultiplier              = static_cast<float>(pINI->ReadFixed(section, "OccupyROFMultiplier", OccupyROFMultiplier));
    OccupyWeaponRange                = pINI->ReadInteger(section, "OccupyWeaponRange", OccupyWeaponRange);
    BunkerROFMultiplier              = static_cast<float>(pINI->ReadFixed(section, "BunkerROFMultiplier", BunkerROFMultiplier));
    BunkerWeaponRangeBonus           = pINI->ReadInteger(section, "BunkerWeaponRangeBonus", BunkerWeaponRangeBonus);
    OpenToppedDamageMultiplier       = static_cast<float>(pINI->ReadFixed(section, "OpenToppedDamageMultiplier", OpenToppedDamageMultiplier));
    OpenToppedRangeBonus             = pINI->ReadInteger(section, "OpenToppedRangeBonus", OpenToppedRangeBonus);
    OpenToppedWarpDistance           = pINI->ReadInteger(section, "OpenToppedWarpDistance", OpenToppedWarpDistance);
    MindControlAttackLineFrames      = pINI->ReadInteger(section, "MindControlAttackLineFrames", MindControlAttackLineFrames);
    { char _buf[0x40]; if (pINI->ReadString(section, "DrainAnimationType", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) DrainAnimationType = _p; } }
    DrainMoneyFrameDelay             = pINI->ReadInteger(section, "DrainMoneyFrameDelay", DrainMoneyFrameDelay);
    DrainMoneyAmount                 = pINI->ReadInteger(section, "DrainMoneyAmount", DrainMoneyAmount);
    FallingDamageMultiplier          = static_cast<float>(pINI->ReadFixed(section, "FallingDamageMultiplier", FallingDamageMultiplier));
    // asm: RulesClass_Addition_CombatDamage @ addr +0x0F4C —— GetFixed 到 float 槽。
    BunkerDamageMultiplier           = static_cast<float>(pINI->ReadFixed(section, "BunkerDamageMultiplier", BunkerDamageMultiplier));
    // asm: RulesClass_Addition_CombatDamage —— INIClass_ReadIntHundredth 到 +0x1560。
    Incoming                         = pINI->ReadIntHundredth(section, "Incoming", Incoming);
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
    TurboBoost                       = pINI->ReadFixed(section, "TurboBoost", TurboBoost);
    C4Delay                          = pINI->ReadFixed(section, "C4Delay", C4Delay);
    ExpSpread                        = pINI->ReadFixed(section, "ExpSpread", ExpSpread);
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

    // asm: RulesClass_Addition_Radiation 的第一条指令就是 Find_Section("...")：
    // 段不存在时整段跳过。缺这条门会让每个键都走"缺省=字段原值"的路径——
    // 看着等价，但向量键会先 Clear() 再填，段缺失就会把已有列表清空。
    if (!pINI->SectionExists(section))
        return;

    RadDurationMultiple    = pINI->ReadInteger(section, "RadDurationMultiple", RadDurationMultiple);
    RadApplicationDelay    = pINI->ReadInteger(section, "RadApplicationDelay", RadApplicationDelay);
    RadLevelMax            = pINI->ReadInteger(section, "RadLevelMax", RadLevelMax);
    RadLevelDelay          = pINI->ReadInteger(section, "RadLevelDelay", RadLevelDelay);
    RadLightDelay          = pINI->ReadInteger(section, "RadLightDelay", RadLightDelay);
    RadLevelFactor         = pINI->ReadFixed(section, "RadLevelFactor", RadLevelFactor);
    RadLightFactor         = pINI->ReadFixed(section, "RadLightFactor", RadLightFactor);
    RadTintFactor          = pINI->ReadFixed(section, "RadTintFactor", RadTintFactor);

    // ---- rules keys ----
    pINI->Get3Bytes(section, "RadColor", reinterpret_cast<uint8*>(&RadColor));
    { char _buf[0x40]; if (pINI->ReadString(section, "RadSiteWarhead", "", _buf, sizeof(_buf)) > 0) { WarheadTypeClass* _p = WarheadTypeClass::FindOrAllocate(_buf); if (_p) RadSiteWarhead = _p; } }
}

// ============================================================================
// Read_ElevationModel - [ElevationModel] section
//
//   asm: RulesClass_Addition_ElevationModel (asm:3104)。段内只有 3 个键。
//   AlliedWallTransparency / WallPenetratorThreshold 属于 [WallModel]
//   （asm:3185），旧代码在这里重复读了一遍（缺键时是自赋值、无害，但不合
//   asm 的段划分），按 asm 删除。
// ============================================================================

void RulesClass::Read_ElevationModel(CCINIClass* pINI)
{
    if (!pINI) return;
    if (!pINI->SectionExists("ElevationModel")) return;

    const char* section = "ElevationModel";

    ElevationIncrement       = pINI->ReadInteger(section, "ElevationIncrement", ElevationIncrement);
    ElevationIncrementBonus  = pINI->ReadFixed(section, "ElevationIncrementBonus", ElevationIncrementBonus);
    ElevationBonusCap        = pINI->ReadFixed(section, "ElevationBonusCap", ElevationBonusCap);
}

// ============================================================================
// Read_WallModel - [WallModel] section
//
//   asm: RulesClass_Addition_WallModel (asm:3185)。段内只有两个键：
//   AlliedWallTransparency（GetBool，缺省 0）与 WallPenetratorThreshold
//   （GetFixed）。WallBuildSpeedCoefficient 在 asm 里属于 [General]
//   （asm:12836xx，GetFixed），已由 Read_General 读取，本节不再重复。
// ============================================================================

void RulesClass::Read_WallModel(CCINIClass* pINI)
{
    if (!pINI) return;
    if (!pINI->SectionExists("WallModel")) return;

    const char* section = "WallModel";

    AlliedWallTransparency           = pINI->ReadBool(section, "AlliedWallTransparency", AlliedWallTransparency);
    WallPenetratorThreshold          = pINI->ReadFixed(section, "WallPenetratorThreshold", WallPenetratorThreshold);
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

    // asm: RulesClass_Addition_Colors (0x66D3A0) —— 整个 [Colors] 段没有固定键名：
    //   1. Find_Section("Colors")，不存在直接返回 false（一个字段都不动）；
    //   2. count = Section_GetValueCount("Colors")；
    //   3. 逐个 key = Section_GetKeyName(i)：
    //        GetColor(&rgb, "Colors", key, {0,0,0})   // "R,G,B"
    //        ColorScheme_FindByName(key, rgb, CC_PAL, UNITxxx_PAL, 1)
    //        ColorScheme_FindByName(key, rgb, CC_PAL, UNITxxx_PAL, 0x35)
    //      —— 每个键既注册 1 阶也注册 0x35 阶（原版的两套阴影数）。
    //   [Colors] 里出现的键就是配色方案名（AlliedLoad / SovietLoad / ...），
    //   值是该方案的基础 RGB。
    if (!pINI->SectionExists(section))
        return;

    const int32 count = pINI->Section_GetValueCount(section);

    // 原版的 CC_PAL / UNITxxx_PAL 是两张固定的 256 色表；重建侧尚未加载调色板
    // 资源，用零填充的 BytePalette 保持签名一致（FindByName 只用于建立条目，
    // 真正的调色板在 GeneratePalette 阶段再填）。
    static const BytePalette EmptyPalette;

    for (int32 i = 0; i < count; ++i)
    {
        const char* pKey = pINI->Section_GetKeyName(section, i);
        if (pKey == nullptr || pKey[0] == '\0')
            continue;

        ColorStruct color(0, 0, 0);
        pINI->GetColor(&color, section, pKey, &color);

        ColorScheme::FindByName(pKey, color, EmptyPalette, EmptyPalette, 1);
        ColorScheme::FindByName(pKey, color, EmptyPalette, EmptyPalette, 0x35);
    }
}

// ----------------------------------------------------------------------------
// Read_ColorAdd - [ColorAdd] section
//
//   asm: RulesClass_Addition_ColorAdd (asm:1278381)
//     1. 先 Find_Section("ColorAdd")，不存在则返回 false（不改任何字段）；
//     2. count = Section_GetValueCount("ColorAdd")；
//     3. for i in [0,count): key = Section_GetKeyName(i)；
//        Get3Bytes(&rgb, "ColorAdd", key, {0,0,0}) —— 缺省三字节清零；
//        把 rgb 顺序写入 ColorAdd[i]（基址 +0x1874，步长 3）。
//   键名不固定，按段内出现顺序落格；本实现按 ColorAdd[0x10] 容量截断。
// ----------------------------------------------------------------------------
void RulesClass::Read_ColorAdd(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "ColorAdd";

    if (!pINI->SectionExists(section))
        return;

    const int32 count = pINI->Section_GetValueCount(section);
    const int32 limit = (count < 0x10) ? count : 0x10;

    for (int32 i = 0; i < limit; ++i)
    {
        const char* key = pINI->Section_GetKeyName(section, i);
        if (key == nullptr)
            continue;

        uint8 rgb[3] = { 0, 0, 0 };
        pINI->Read3Bytes(rgb, section, key, rgb);
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

    // asm: RulesClass_Addition_General 的第一条指令就是 Find_Section("...")：
    // 段不存在时整段跳过。缺这条门会让每个键都走"缺省=字段原值"的路径——
    // 看着等价，但向量键会先 Clear() 再填，段缺失就会把已有列表清空。
    if (!pINI->SectionExists(section))
        return;

    // DetailMinFrameRateNormal / DetailMinFrameRateMovie / DetailBufferZoneWidth /
    // PoseDir / DeployDir / WaypointAnimationSpeed 属于 [AudioVisual]（asm:
    // RulesClass_Addition_AudioVisual，str_AudioVisual），已由 Read_AudioVisual 读取。
    // FreeMCV 属于 [CrateRules]、BerzerkAllowed 属于 [CombatDamage]，同理。
    AttackingAircraftSightRange    = pINI->ReadInteger(section, "AttackingAircraftSightRange", AttackingAircraftSightRange);
    TunnelSpeed                    = pINI->ReadFixed(section, "TunnelSpeed", TunnelSpeed);
    TiberiumHeal                   = pINI->ReadFixed(section, "TiberiumHeal", TiberiumHeal);
    SelfHealInfantryFrames         = pINI->ReadInteger(section, "SelfHealInfantryFrames", SelfHealInfantryFrames);
    SelfHealInfantryAmount         = pINI->ReadInteger(section, "SelfHealInfantryAmount", SelfHealInfantryAmount);
    SelfHealUnitFrames             = pINI->ReadInteger(section, "SelfHealUnitFrames", SelfHealUnitFrames);
    SelfHealUnitAmount             = pINI->ReadInteger(section, "SelfHealUnitAmount", SelfHealUnitAmount);
    MaximumQueuedObjects           = pINI->ReadInteger(section, "MaximumQueuedObjects", MaximumQueuedObjects);
    AircraftFogReveal              = pINI->ReadInteger(section, "AircraftFogReveal", AircraftFogReveal);

    ZoomInFactor                   = pINI->ReadFixed(section, "ZoomInFactor", ZoomInFactor);
    MinLowPowerProductionSpeed     = static_cast<float>(pINI->ReadFixed(section, "MinLowPowerProductionSpeed", MinLowPowerProductionSpeed));
    MaxLowPowerProductionSpeed     = static_cast<float>(pINI->ReadFixed(section, "MaxLowPowerProductionSpeed", MaxLowPowerProductionSpeed));
    LowPowerPenaltyModifier        = static_cast<float>(pINI->ReadFixed(section, "LowPowerPenaltyModifier", LowPowerPenaltyModifier));
    MultipleFactory                = static_cast<float>(pINI->ReadFixed(section, "MultipleFactory", MultipleFactory));
    MaximumCheerRate               = pINI->ReadInteger(section, "MaximumCheerRate", MaximumCheerRate);
    TreeFlammability               = pINI->ReadFixed(section, "TreeFlammability", TreeFlammability);
    MissileSpeedVar                = pINI->ReadFixed(section, "MissileSpeedVar", MissileSpeedVar);
    MissileROTVar                  = pINI->ReadFixed(section, "MissileROTVar", MissileROTVar);
    MissileSafetyAltitude          = pINI->ReadInteger(section, "MissileSafetyAltitude", MissileSafetyAltitude);
    DropPodHeight                  = pINI->ReadInteger(section, "DropPodHeight", DropPodHeight);
    DropPodSpeed                   = pINI->ReadInteger(section, "DropPodSpeed", DropPodSpeed);
    DropPodAngle                   = pINI->ReadFixed(section, "DropPodAngle", DropPodAngle);
    CrewEscape                     = pINI->ReadFixed(section, "CrewEscape", CrewEscape);
    HoverHeight                    = pINI->ReadInteger(section, "HoverHeight", HoverHeight);
    HoverBob                       = pINI->ReadFixed(section, "HoverBob", HoverBob);
    HoverBoost                     = pINI->ReadFixed(section, "HoverBoost", HoverBoost);
    HoverAcceleration              = pINI->ReadFixed(section, "HoverAcceleration", HoverAcceleration);
    HoverBrake                     = pINI->ReadFixed(section, "HoverBrake", HoverBrake);
    HoverDampen                    = pINI->ReadFixed(section, "HoverDampen", HoverDampen);
    PlacementDelay                 = pINI->ReadFixed(section, "PlacementDelay", PlacementDelay);
    BridgeVoxelMax                 = pINI->ReadInteger(section, "BridgeVoxelMax", BridgeVoxelMax);
    CloakingStages                 = pINI->ReadInteger(section, "CloakingStages", CloakingStages);
    RevealTriggerRadius            = pINI->ReadInteger(section, "RevealTriggerRadius", RevealTriggerRadius);
    ShipSinkingWeight              = pINI->ReadFixed(section, "ShipSinkingWeight", ShipSinkingWeight);
    IceCrackingWeight              = pINI->ReadFixed(section, "IceCrackingWeight", IceCrackingWeight);
    IceBreakingWeight              = pINI->ReadFixed(section, "IceBreakingWeight", IceBreakingWeight);
    CliffBackImpassability         = static_cast<uint8>(pINI->ReadInteger(section, "CliffBackImpassability", CliffBackImpassability));

    VeteranRatio                   = pINI->ReadFixed(section, "VeteranRatio", VeteranRatio);
    VeteranCombat                  = pINI->ReadFixed(section, "VeteranCombat", VeteranCombat);
    VeteranSpeed                   = pINI->ReadFixed(section, "VeteranSpeed", VeteranSpeed);
    VeteranSight                   = pINI->ReadFixed(section, "VeteranSight", VeteranSight);
    VeteranArmor                   = pINI->ReadFixed(section, "VeteranArmor", VeteranArmor);
    VeteranROF                     = pINI->ReadFixed(section, "VeteranROF", VeteranROF);
    VeteranCap                     = pINI->ReadFixed(section, "VeteranCap", VeteranCap);

    ChargeToDrainRatio             = pINI->ReadFixed(section, "ChargeToDrainRatio", ChargeToDrainRatio);
    TrackedUphill                  = pINI->ReadFixed(section, "TrackedUphill", TrackedUphill);
    TrackedDownhill                = pINI->ReadFixed(section, "TrackedDownhill", TrackedDownhill);
    WheeledUphill                  = pINI->ReadFixed(section, "WheeledUphill", WheeledUphill);
    WheeledDownhill                = pINI->ReadFixed(section, "WheeledDownhill", WheeledDownhill);

    SpotlightMovementRadius        = pINI->ReadInteger(section, "SpotlightMovementRadius", SpotlightMovementRadius);
    SpotlightLocationRadius        = pINI->ReadInteger(section, "SpotlightLocationRadius", SpotlightLocationRadius);
    SpotlightSpeed                 = pINI->ReadFixed(section, "SpotlightSpeed", SpotlightSpeed);
    SpotlightAcceleration          = pINI->ReadFixed(section, "SpotlightAcceleration", SpotlightAcceleration);
    SpotlightAngle                 = pINI->ReadFixed(section, "SpotlightAngle", SpotlightAngle);
    SpotlightRadius                = pINI->ReadInteger(section, "SpotlightRadius", SpotlightRadius);

    WindDirection                  = pINI->ReadInteger(section, "WindDirection", WindDirection);
    // asm: RulesClass_Addition_General 用 CCINIClass::Get_Lepton 读 CameraRange。
    CameraRange                    = pINI->ReadLepton(section, "CameraRange", CameraRange);
    FlightLevel                    = pINI->ReadInteger(section, "FlightLevel", FlightLevel);
    ParachuteMaxFallRate           = pINI->ReadInteger(section, "ParachuteMaxFallRate", ParachuteMaxFallRate);
    NoParachuteMaxFallRate         = pINI->ReadInteger(section, "NoParachuteMaxFallRate", NoParachuteMaxFallRate);
    // BuildingDrop 属于 [AudioVisual]（asm: Addition_AudioVisual）。

    ChronoDelay                    = pINI->ReadInteger(section, "ChronoDelay", ChronoDelay);
    ChronoReinfDelay               = pINI->ReadInteger(section, "ChronoReinfDelay", ChronoReinfDelay);
    ChronoDistanceFactor           = pINI->ReadInteger(section, "ChronoDistanceFactor", ChronoDistanceFactor);
    ChronoTrigger                  = pINI->ReadBool(section, "ChronoTrigger", ChronoTrigger);
    ChronoMinimumDelay             = pINI->ReadInteger(section, "ChronoMinimumDelay", ChronoMinimumDelay);
    ChronoRangeMinimum             = pINI->ReadInteger(section, "ChronoRangeMinimum", ChronoRangeMinimum);

    // EliteFlashTimer 属于 [AudioVisual]；IronCurtainDuration / PsychicRevealRadius /
    // IvanDamage / IvanTimedDelay / CanDetonate*Bomb / IvanIconFlickerRate / C4Delay
    // 属于 [CombatDamage]（asm: Addition_CombatDamage）。
    InfantryBlinkDisguiseTime      = pINI->ReadInteger(section, "InfantryBlinkDisguiseTime", InfantryBlinkDisguiseTime);

    RefundPercent                  = pINI->ReadFixed(section, "RefundPercent", RefundPercent);
    BuildSpeed                     = pINI->ReadFixed(section, "BuildSpeed", BuildSpeed);
    // C4Delay 属于 [CombatDamage]；CreditReserve / PathDelay / BlockagePathDelay
    // 属于 [AI]（asm: Addition_AI）；MovieTime 属于 [AudioVisual]。

    CloakDelay                     = pINI->ReadFixed(section, "CloakDelay", CloakDelay);
    GameSpeedBias                  = pINI->ReadFixed(section, "GameSpeedBias", GameSpeedBias);
    BaseBias                       = pINI->ReadFixed(section, "BaseBias", BaseBias);
    // ExpSpread 属于 [CombatDamage]；MaxIQLevels..SellBack 属于 [IQ]。
    // SellBack 属于 [IQ]；AIBaseSpacing / Paranoid / CompEasyBonus 属于 [AI]；
    // TiberiumExplosive / TreeTargeting / Player* 属于 [CombatDamage]；
    // EnemyHealth / AllyReveal / NamedCivilians / ShroudGrow 属于 [AudioVisual]。
    CurleyShuffle                  = pINI->ReadBool(section, "CurleyShuffle", CurleyShuffle);
    BlendedFog                     = pINI->ReadBool(section, "BlendedFog", BlendedFog);
    FineDiffControl                = pINI->ReadBool(section, "FineDiffControl", FineDiffControl);
    SeparateAircraft               = pINI->ReadBool(section, "SeparateAircraft", SeparateAircraft);
    RevealByHeight                 = pINI->ReadBool(section, "RevealByHeight", RevealByHeight);
    AllowShroudedSubteranneanMoves = pINI->ReadBool(section, "AllowShroudedSubteranneanMoves", AllowShroudedSubteranneanMoves);
    NodAIBuildsWalls              = pINI->ReadBool(section, "NodAIBuildsWalls", NodAIBuildsWalls);
    AIBuildsWalls                  = pINI->ReadBool(section, "AIBuildsWalls", AIBuildsWalls);
    UseMinDefenseRule              = pINI->ReadBool(section, "UseMinDefenseRule", UseMinDefenseRule);

    // asm: RulesClass_Addition_General 把同一个 "EngineerCaptureLevel" 读两次
    // （+0x155C 与 +0x1548 两个 float 槽），并不存在带句点的键名。
    EngineerCaptureLevel           = static_cast<float>(pINI->ReadFixed(section, "EngineerCaptureLevel", EngineerCaptureLevel));
    EngineerCaptureLevel_          = static_cast<float>(pINI->ReadFixed(section, "EngineerCaptureLevel", EngineerCaptureLevel_));
    // asm: TalkBubbleTime = floor(GetFixed("TalkBubbleTime", old/60.0) * 60.0)
    // —— INI 里是秒，字段里存的是 60fps 的帧数。
    TalkBubbleTime                 = static_cast<float>(std::floor(
                                         pINI->ReadFixed(section, "TalkBubbleTime",
                                                         static_cast<double>(TalkBubbleTime) / 60.0) * 60.0));

    // DirectRockingCoefficient / FallBackCoefficient / Extra*Light 属于 [AudioVisual]。
    WeedCapacity                   = pINI->ReadInteger(section, "WeedCapacity", WeedCapacity);
    LeptonsPerSightIncrease        = pINI->ReadInteger(section, "LeptonsPerSightIncrease", LeptonsPerSightIncrease);
    // Incoming 在 asm 里属于 [CombatDamage]（ReadIntHundredth），已移至 Read_CombatDamage。
    DominatorFireAtPercentage      = pINI->ReadInteger(section, "DominatorFireAtPercentage", DominatorFireAtPercentage);
    DominatorCaptureRange          = pINI->ReadInteger(section, "DominatorCaptureRange", DominatorCaptureRange);
    DominatorDamage                = pINI->ReadInteger(section, "DominatorDamage", DominatorDamage);
    // MindControlAttackLineFrames / DrainMoneyFrameDelay / DrainMoneyAmount 同样
    // 属于 [CombatDamage]。
    // IonCannonDamage / RailgunDamageRadius 在 asm 里属于 [CombatDamage]
    // （RulesClass_Addition_CombatDamage），已由 Read_CombatDamage 读取，此处不再重复。
    // asm: RulesClass_Addition_General 用 INIClass_GetFixed 读 PrismSupportModifier，
    // 随后 *100 再 Float_To_Int_Floor 存入 int32 槽；故按百分比解析再放大。
    PrismSupportModifier           = static_cast<int32>(std::floor(
                                         pINI->ReadFixed(section, "PrismSupportModifier",
                                                         static_cast<double>(PrismSupportModifier)) * 100.0));
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
    SpyMoneyStealPercent             = static_cast<float>(pINI->ReadFixed(section, "SpyMoneyStealPercent", SpyMoneyStealPercent));
    AttackCursorOnDisguise           = pINI->ReadBool(section, "AttackCursorOnDisguise", AttackCursorOnDisguise);
    PurifierBonus                    = static_cast<float>(pINI->ReadFixed(section, "PurifierBonus", PurifierBonus));
    { char _buf[0x40]; if (pINI->ReadString(section, "Engineer", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) Engineer = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Technician", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) Technician = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "Pilot", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) Pilot = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "AlliedCrew", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) AlliedCrew = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "SovietCrew", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) SovietCrew = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "ThirdCrew", "", _buf, sizeof(_buf)) > 0) { InfantryTypeClass* _p = InfantryTypeClass::FindOrAllocate(_buf); if (_p) ThirdCrew = _p; } }
    AIAlternateProductionCreditCutoff = pINI->ReadInteger(section, "AIAlternateProductionCreditCutoff", AIAlternateProductionCreditCutoff);
    AIUseTurbineUpgradeProbability   = pINI->ReadFixed(section, "AIUseTurbineUpgradeProbability", AIUseTurbineUpgradeProbability);
    DissolveUnfilledTeamDelay        = pINI->ReadInteger(section, "DissolveUnfilledTeamDelay", DissolveUnfilledTeamDelay);
    AISafeDistance                   = pINI->ReadInteger(section, "AISafeDistance", AISafeDistance);
    AIMinorSuperReadyPercent         = static_cast<float>(pINI->ReadFixed(section, "AIMinorSuperReadyPercent", AIMinorSuperReadyPercent));
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
    // asm: RulesClass_Addition_General 紧接其后是六个 Get_Lepton 的扫描半径
    // （CCINIClass::Get_Lepton → 本项目的 ReadLepton），再接 SlaveMinerKickFrameDelay。
    TiberiumShortScan                = pINI->ReadLepton(section, "TiberiumShortScan", TiberiumShortScan);
    TiberiumLongScan                 = pINI->ReadLepton(section, "TiberiumLongScan", TiberiumLongScan);
    SlaveMinerShortScan              = pINI->ReadLepton(section, "SlaveMinerShortScan", SlaveMinerShortScan);
    SlaveMinerSlaveScan              = pINI->ReadLepton(section, "SlaveMinerSlaveScan", SlaveMinerSlaveScan);
    SlaveMinerLongScan               = pINI->ReadLepton(section, "SlaveMinerLongScan", SlaveMinerLongScan);
    SlaveMinerScanCorrection         = pINI->ReadLepton(section, "SlaveMinerScanCorrection", SlaveMinerScanCorrection);
    SlaveMinerKickFrameDelay         = pINI->ReadInteger(section, "SlaveMinerKickFrameDelay", SlaveMinerKickFrameDelay);
    AISuperDefenseFrames             = pINI->ReadInteger(section, "AISuperDefenseFrames", AISuperDefenseFrames);
    AICaptureLowMoneyMark            = pINI->ReadInteger(section, "AICaptureLowMoneyMark", AICaptureLowMoneyMark);
    // asm: RulesClass_Addition_General @ +0x0EC0 —— GetFixed 到 float 槽。
    AICaptureWoundedMark             = static_cast<float>(pINI->ReadFixed(section, "AICaptureWoundedMark", AICaptureWoundedMark));
    // asm: RulesClass_Addition_General @ +0x0EE4 —— Get_Lepton（整数，1 lepton/格）。
    AISuperDefenseDistance           = static_cast<float>(pINI->ReadLepton(section, "AISuperDefenseDistance", static_cast<int32>(AISuperDefenseDistance)));
    // 射击散布（asm: Addition_General，全部用 CCINIClass::Get_Lepton）。
    // asm: RulesClass_Addition_General 对 CloseEnough / Stray / RelaxedStray /
    // GuardModeStray 一律用 CCINIClass::Get_Lepton（lepton 距离，等价 ReadLepton）。
    CloseEnough                      = pINI->ReadLepton(section, "CloseEnough", CloseEnough);
    Stray                            = pINI->ReadLepton(section, "Stray", Stray);
    RelaxedStray                     = pINI->ReadLepton(section, "RelaxedStray", RelaxedStray);
    GuardModeStray                   = pINI->ReadLepton(section, "GuardModeStray", GuardModeStray);
    // [General] 的步兵"秘密单位/动画转步兵"列表（asm: GetVectorInfType）。
    pINI->GetVectorInfType(section, "SecretInfantry", SecretInfantry);
    BaseDefenseDelay                 = pINI->ReadFixed(section, "BaseDefenseDelay", BaseDefenseDelay);
    SuspendPriority                  = pINI->ReadInteger(section, "SuspendPriority", SuspendPriority);
    SuspendDelay                     = pINI->ReadFixed(section, "SuspendDelay", SuspendDelay);
    SurvivorRate                     = pINI->ReadFixed(section, "SurvivorRate", SurvivorRate);
    AlliedSurvivorDivisor            = pINI->ReadInteger(section, "AlliedSurvivorDivisor", AlliedSurvivorDivisor);
    SovietSurvivorDivisor            = pINI->ReadInteger(section, "SovietSurvivorDivisor", SovietSurvivorDivisor);
    ThirdSurvivorDivisor             = pINI->ReadInteger(section, "ThirdSurvivorDivisor", ThirdSurvivorDivisor);
    ReloadRate                       = pINI->ReadFixed(section, "ReloadRate", ReloadRate);
    BuildupTime                      = pINI->ReadFixed(section, "BuildupTime", BuildupTime);
    HarvesterDumpRate                = pINI->ReadFixed(section, "HarvesterDumpRate", HarvesterDumpRate);
    HarvesterLoadRate                = pINI->ReadInteger(section, "HarvesterLoadRate", HarvesterLoadRate);
    DamageDelay                      = pINI->ReadFixed(section, "DamageDelay", DamageDelay);
    GrowthRate                       = pINI->ReadFixed(section, "GrowthRate", GrowthRate);
    RepairPercent                    = pINI->ReadFixed(section, "RepairPercent", RepairPercent);
    RepairStep                       = pINI->ReadInteger(section, "RepairStep", RepairStep);
    IRepairStep                      = pINI->ReadInteger(section, "IRepairStep", IRepairStep);
    RepairRate                       = pINI->ReadFixed(section, "RepairRate", RepairRate);
    URepairRate                      = pINI->ReadFixed(section, "URepairRate", URepairRate);
    IRepairRate                      = pINI->ReadFixed(section, "IRepairRate", IRepairRate);
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
    V3RocketPitchInitial             = pINI->ReadFixed(section, "V3RocketPitchInitial", V3RocketPitchInitial);
    V3RocketPitchFinal               = pINI->ReadFixed(section, "V3RocketPitchFinal", V3RocketPitchFinal);
    V3RocketTurnRate                 = pINI->ReadFixed(section, "V3RocketTurnRate", V3RocketTurnRate);
    // asm: GetFixed -> Float_To_Int_Floor，字段是整数（asm: fild dword 作缺省）。
    V3RocketRaiseRate = std::floor(pINI->ReadFixed(section, "V3RocketRaiseRate", V3RocketRaiseRate));
    V3RocketAcceleration             = pINI->ReadFixed(section, "V3RocketAcceleration", V3RocketAcceleration);
    V3RocketAltitude                 = pINI->ReadInteger(section, "V3RocketAltitude", V3RocketAltitude);
    V3RocketDamage                   = pINI->ReadInteger(section, "V3RocketDamage", V3RocketDamage);
    V3RocketEliteDamage              = pINI->ReadInteger(section, "V3RocketEliteDamage", V3RocketEliteDamage);
    V3RocketBodyLength               = pINI->ReadInteger(section, "V3RocketBodyLength", V3RocketBodyLength);
    V3RocketLazyCurve                = pINI->ReadBool(section, "V3RocketLazyCurve", V3RocketLazyCurve);
    { char _buf[0x40]; if (pINI->ReadString(section, "V3RocketType", "", _buf, sizeof(_buf)) > 0) { AircraftTypeClass* _p = AircraftTypeClass::FindOrAllocate(_buf); if (_p) V3RocketType = _p; } }
    DMislPauseFrames                 = pINI->ReadInteger(section, "DMislPauseFrames", DMislPauseFrames);
    DMislTiltFrames                  = pINI->ReadInteger(section, "DMislTiltFrames", DMislTiltFrames);
    DMislPitchInitial                = pINI->ReadFixed(section, "DMislPitchInitial", DMislPitchInitial);
    DMislPitchFinal                  = pINI->ReadFixed(section, "DMislPitchFinal", DMislPitchFinal);
    DMislTurnRate                    = pINI->ReadFixed(section, "DMislTurnRate", DMislTurnRate);
    // asm: GetFixed -> Float_To_Int_Floor，字段是整数（asm: fild dword 作缺省）。
    DMislRaiseRate = std::floor(pINI->ReadFixed(section, "DMislRaiseRate", DMislRaiseRate));
    DMislAcceleration                = pINI->ReadFixed(section, "DMislAcceleration", DMislAcceleration);
    DMislAltitude                    = pINI->ReadInteger(section, "DMislAltitude", DMislAltitude);
    DMislDamage                      = pINI->ReadInteger(section, "DMislDamage", DMislDamage);
    DMislEliteDamage                 = pINI->ReadInteger(section, "DMislEliteDamage", DMislEliteDamage);
    DMislBodyLength                  = pINI->ReadInteger(section, "DMislBodyLength", DMislBodyLength);
    DMislLazyCurve                   = pINI->ReadBool(section, "DMislLazyCurve", DMislLazyCurve);
    { char _buf[0x40]; if (pINI->ReadString(section, "DMislType", "", _buf, sizeof(_buf)) > 0) { AircraftTypeClass* _p = AircraftTypeClass::FindOrAllocate(_buf); if (_p) DMislType = _p; } }
    CMislPauseFrames                 = pINI->ReadInteger(section, "CMislPauseFrames", CMislPauseFrames);
    CMislTiltFrames                  = pINI->ReadInteger(section, "CMislTiltFrames", CMislTiltFrames);
    CMislPitchInitial                = pINI->ReadFixed(section, "CMislPitchInitial", CMislPitchInitial);
    CMislPitchFinal                  = pINI->ReadFixed(section, "CMislPitchFinal", CMislPitchFinal);
    CMislTurnRate                    = pINI->ReadFixed(section, "CMislTurnRate", CMislTurnRate);
    // asm: GetFixed -> Float_To_Int_Floor，字段是整数（asm: fild dword 作缺省）。
    CMislRaiseRate = std::floor(pINI->ReadFixed(section, "CMislRaiseRate", CMislRaiseRate));
    CMislAcceleration                = pINI->ReadFixed(section, "CMislAcceleration", CMislAcceleration);
    CMislAltitude                    = pINI->ReadInteger(section, "CMislAltitude", CMislAltitude);
    CMislDamage                      = pINI->ReadInteger(section, "CMislDamage", CMislDamage);
    CMislEliteDamage                 = pINI->ReadInteger(section, "CMislEliteDamage", CMislEliteDamage);
    CMislBodyLength                  = pINI->ReadInteger(section, "CMislBodyLength", CMislBodyLength);
    CMislLazyCurve                   = pINI->ReadBool(section, "CMislLazyCurve", CMislLazyCurve);
    { char _buf[0x40]; if (pINI->ReadString(section, "CMislType", "", _buf, sizeof(_buf)) > 0) { AircraftTypeClass* _p = AircraftTypeClass::FindOrAllocate(_buf); if (_p) CMislType = _p; } }
    WallBuildSpeedCoefficient        = pINI->ReadFixed(section, "WallBuildSpeedCoefficient", WallBuildSpeedCoefficient);
    ConditionYellowSparkingProbability = pINI->ReadFixed(section, "ConditionYellowSparkingProbability", ConditionYellowSparkingProbability);
    ConditionRedSparkingProbability  = pINI->ReadFixed(section, "ConditionRedSparkingProbability", ConditionRedSparkingProbability);
    AITriggerSuccessWeightDelta      = pINI->ReadFixed(section, "AITriggerSuccessWeightDelta", AITriggerSuccessWeightDelta);
    AITriggerFailureWeightDelta      = pINI->ReadFixed(section, "AITriggerFailureWeightDelta", AITriggerFailureWeightDelta);
    AITriggerTrackRecordCoefficient  = pINI->ReadFixed(section, "AITriggerTrackRecordCoefficient", AITriggerTrackRecordCoefficient);
    FlashFrameTime                   = pINI->ReadInteger(section, "FlashFrameTime", FlashFrameTime);
    RadarCombatFlashTime             = pINI->ReadInteger(section, "RadarCombatFlashTime", RadarCombatFlashTime);
    RadarEventSpeed                  = static_cast<float>(pINI->ReadFixed(section, "RadarEventSpeed", RadarEventSpeed));
    RadarEventRotationSpeed          = static_cast<float>(pINI->ReadFixed(section, "RadarEventRotationSpeed", RadarEventRotationSpeed));
    RadarEventMinRadius              = pINI->ReadInteger(section, "RadarEventMinRadius", RadarEventMinRadius);
    RadarEventColorSpeed             = static_cast<float>(pINI->ReadFixed(section, "RadarEventColorSpeed", RadarEventColorSpeed));
    MyEffectivenessCoefficientDefault = pINI->ReadFixed(section, "MyEffectivenessCoefficientDefault", MyEffectivenessCoefficientDefault);
    // 原版键名以 Default 结尾（asm: Addition_General str_General）；旧代码误用了
    // 被截断的 "...Defa"，导致真实键永远读不到，这里按 asm 逐字对齐。
    TargetEffectivenessCoefficientDefault = pINI->ReadFixed(section, "TargetEffectivenessCoefficientDefault", TargetEffectivenessCoefficientDefault);
    TargetSpecialThreatCoefficientDefault = pINI->ReadFixed(section, "TargetSpecialThreatCoefficientDefault", TargetSpecialThreatCoefficientDefault);
    TargetStrengthCoefficientDefault = pINI->ReadFixed(section, "TargetStrengthCoefficientDefault", TargetStrengthCoefficientDefault);
    TargetDistanceCoefficientDefault = pINI->ReadFixed(section, "TargetDistanceCoefficientDefault", TargetDistanceCoefficientDefault);
    DumbMyEffectivenessCoefficient   = pINI->ReadFixed(section, "DumbMyEffectivenessCoefficient", DumbMyEffectivenessCoefficient);
    DumbTargetEffectivenessCoefficient = pINI->ReadFixed(section, "DumbTargetEffectivenessCoefficient", DumbTargetEffectivenessCoefficient);
    DumbTargetSpecialThreatCoefficient = pINI->ReadFixed(section, "DumbTargetSpecialThreatCoefficient", DumbTargetSpecialThreatCoefficient);
    DumbTargetStrengthCoefficient    = pINI->ReadFixed(section, "DumbTargetStrengthCoefficient", DumbTargetStrengthCoefficient);
    DumbTargetDistanceCoefficient    = pINI->ReadFixed(section, "DumbTargetDistanceCoefficient", DumbTargetDistanceCoefficient);
    EnemyHouseThreatBonus            = pINI->ReadFixed(section, "EnemyHouseThreatBonus", EnemyHouseThreatBonus);
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

    // asm: RulesClass_Addition_MultiplayerDialogSettings 的第一条指令就是 Find_Section("...")：
    // 段不存在时整段跳过。缺这条门会让每个键都走"缺省=字段原值"的路径——
    // 看着等价，但向量键会先 Clear() 再填，段缺失就会把已有列表清空。
    if (!pINI->SectionExists(section))
        return;

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
    // DropZoneRadius 在 asm 里属于 [AudioVisual]（Read_AudioVisual），此处移除误读。
}

// ============================================================================
// Section readers - Type list readers and remaining section parsers
// ============================================================================

// ----------------------------------------------------------------------------
// Read_Maximums - asm: RulesClass_Addition_Maximums (asm:1285319)
//
//   先 Find_Section("Maximums")：段不存在则不动 Players（asm 用 jz 跳过整段），
//   存在才 GetInteger("Players", Players)。其余 per-type 计数上限由各类型数组
//   容器自行约束，不落在 RulesClass 字段上。
// ----------------------------------------------------------------------------

void RulesClass::Read_Maximums(CCINIClass* pINI)
{
    const char* section = "Maximums";
    if (!pINI) return;

    if (!pINI->SectionExists(section))
        return;

    // ---- rules keys ----
    Players                          = pINI->ReadInteger(section, "Players", Players);
}

// ----------------------------------------------------------------------------
// Read_InfantryTypes - [InfantryTypes] section
// ----------------------------------------------------------------------------

// asm: RulesClass_Addition_InfantryTypes (0x672270)
//   count = Section_GetValueCount("InfantryTypes")
//   for i in [0, count):
//       name = Section_GetKeyName("InfantryTypes", i)   // 键只是序号
//       v    = GetString("InfantryTypes", name)         // 值才是类型名
//       if (v) InfantryTypeClass_FindOrAllocate(v)
//   return count > 0
// 原版把逐类型的 LoadFromINI 留给其后的 RulesData_LoadTypeData；重建侧没有那
// 一趟，故在登记后立即补一次，语义等价。
void RulesClass::Read_InfantryTypes(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "InfantryTypes";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        InfantryTypeClass* pType = InfantryTypeClass::FindOrAllocate(pID);
        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }
}

// ----------------------------------------------------------------------------
// Read_Countries - [Countries] section
// ----------------------------------------------------------------------------

// asm: [Countries] 的遍历在 RulesClass_Addition 里内联（asm: RulesClass_Addition+B8）：
//   count = Section_GetValueCount("Countries")
//   for i in [0, count):
//       name = Section_GetKeyName("Countries", i)
//       v    = GetString("Countries", name)
//       if (v) HouseTypeClass_FindOrAllocate(v)
// 逐国的属性（Color、Side、IQ …）由 HouseTypeClass::LoadFromINI 从以国名命名的
// 段里读，所以登记之后要补一次。
void RulesClass::Read_Countries(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "Countries";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        HouseTypeClass* pType = HouseTypeClass::FindOrAllocate(pID);
        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }
}

// ----------------------------------------------------------------------------
// Read_VehicleTypes - [VehicleTypes] section
// ----------------------------------------------------------------------------

// asm: RulesClass_Addition_VehicleTypes (0x6722E0) —— 与 InfantryTypes 同构，
// 只是把名字喂给 UnitTypeClass_FindOrAllocate。
void RulesClass::Read_VehicleTypes(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "VehicleTypes";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        UnitTypeClass* pType = UnitTypeClass::FindOrAllocate(pID);
        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }
}

// ----------------------------------------------------------------------------
// Read_AircraftTypes - [AircraftTypes] section
// ----------------------------------------------------------------------------

// asm: RulesClass_Addition_AircraftTypes (0x672350) —— 同构，
// AircraftTypeClass_FindOrAllocate。
void RulesClass::Read_AircraftTypes(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "AircraftTypes";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        AircraftTypeClass* pType = AircraftTypeClass::FindOrAllocate(pID);
        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
 // Read_Sides - RulesClass_Addition_Sides
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

// asm: RulesClass_Addition_BuildingTypes (0x6723C0) —— 同构，
// BuildingTypeClass_FindOrAllocate。
void RulesClass::Read_BuildingTypes(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "BuildingTypes";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        BuildingTypeClass* pType = BuildingTypeClass::FindOrAllocate(pID);
        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }
}

// ----------------------------------------------------------------------------
// Read_TerrainTypes - [TerrainTypes] section
// ----------------------------------------------------------------------------

// asm: RulesClass_Addition_TerrainTypes (0x672A80) —— 同构，
// TerrainTypeClass_FindOrAllocate。
void RulesClass::Read_TerrainTypes(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "TerrainTypes";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        TerrainTypeClass* pType = TerrainTypeClass::FindOrAllocate(pID);
        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }
}

// ----------------------------------------------------------------------------
// Read_SmudgeTypes - [SmudgeTypes] section
// ----------------------------------------------------------------------------

// asm: RulesClass_Addition_SmudgeTypes (0x672B40) —— 同构，
// SmudgeTypeClass_FindOrAllocate。
void RulesClass::Read_SmudgeTypes(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "SmudgeTypes";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        SmudgeTypeClass* pType = SmudgeTypeClass::FindOrAllocate(pID);
        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }
}

// ----------------------------------------------------------------------------
// Read_OverlayTypes - [OverlayTypes] section
// ----------------------------------------------------------------------------

// asm: 该段在 RulesClass_Addition 里是内联循环（asm: RulesClass_Addition+142）：
// Section_GetKeyName("OverlayTypes", i) → GetString → OverlayTypeClass_FindOrAllocate。
void RulesClass::Read_OverlayTypes(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "OverlayTypes";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        OverlayTypeClass* pType = OverlayTypeClass::FindOrAllocate(pID);
        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }
}

// ----------------------------------------------------------------------------
// Read_Animations - [Animations] section
// ----------------------------------------------------------------------------

// asm: RulesClass_Addition_Animations (0x672B00) —— 同构，AnimTypeClass_FindOrAllocate。
void RulesClass::Read_Animations(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "Animations";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        AnimTypeClass* pType = AnimTypeClass::FindOrAllocate(pID);
        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }
}

// ----------------------------------------------------------------------------
// Read_VoxelAnims - [VoxelAnims] section
// ----------------------------------------------------------------------------

// asm: RulesClass_Addition_VoxelAnims (0x672C60) —— 同构，
// VoxelAnimTypeClass_FindOrAllocate。
void RulesClass::Read_VoxelAnims(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "VoxelAnims";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        VoxelAnimTypeClass* pType = VoxelAnimTypeClass::FindOrAllocate(pID);
        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }
}

// ----------------------------------------------------------------------------
// Read_Warheads - [Warheads] section
// ----------------------------------------------------------------------------

// asm: RulesClass_Addition_Warheads (0x672CC0) —— 同构，
// WarheadTypeClass_FindOrAllocate，登记后立即 LoadFromINI。
void RulesClass::Read_Warheads(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "Warheads";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        WarheadTypeClass* pType = WarheadTypeClass::FindOrAllocate(pID);
        if (pType != nullptr)
            pType->LoadFromINIList(pINI);
    }
}

// ----------------------------------------------------------------------------
// Read_Projectiles - [Projectiles] section
// ----------------------------------------------------------------------------

// 与 RulesClass_Addition_Warheads 同构：逐键取名字 -> BulletTypeClass 的
// FindOrAllocate -> 立即用同名段 LoadFromINI。弹丸是武器 Projectile= 的
// 反查目标，所以必须排在武器之前。
void RulesClass::Read_Projectiles(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "Projectiles";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        BulletTypeClass* pType = BulletTypeClass::FindOrAllocate(pID);
        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }
}

// ----------------------------------------------------------------------------
// Read_WeaponTypes - [WeaponTypes] section
// ----------------------------------------------------------------------------

// 同上，目标是 WeaponTypeClass::Array（asm: vec_WeaponTypes）。
void RulesClass::Read_WeaponTypes(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "WeaponTypes";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        WeaponTypeClass* pType = WeaponTypeClass::FindOrAllocate(pID);
        if (pType != nullptr)
            pType->LoadFromINIList(pINI);
    }
}

// ----------------------------------------------------------------------------
// Read_Particles - [Particles] section
// ----------------------------------------------------------------------------

// asm: RulesClass_Addition_Particles (0x672D90) —— 同构，但粒子类型没有
// FindOrAllocate：直接 new + SetName，再用同一个名字调 ReadFromINI。
void RulesClass::Read_Particles(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "Particles";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        ParticleTypeClass* pType = new ParticleTypeClass();
        if (pType != nullptr)
        {
            pType->SetName(pID);
            pType->ReadFromINI(pINI, pID);
        }
    }
}

// ----------------------------------------------------------------------------
// Read_ParticleSystems - [ParticleSystems] section
// ----------------------------------------------------------------------------

// asm: RulesClass_Addition_ParticleSystems (0x672E00) —— 同构：先按名字查，
// 未命中再 new + SetName，最后 LoadFromINI。
void RulesClass::Read_ParticleSystems(CCINIClass* pINI)
{
    if (pINI == nullptr) return;
    const char* section = "ParticleSystems";

    const int32 count = pINI->Section_GetValueCount(section);
    for (int32 i = 0; i < count; ++i)
    {
        char name[0x40];
        pINI->Section_GetKeyName(section, i, name, sizeof(name));

        const char* pID = pINI->GetString(section, name);
        if (pID == nullptr) continue;

        ParticleSystemTypeClass* pType = ParticleSystemTypeClass::Find(pID);
        if (pType == nullptr)
        {
            pType = new ParticleSystemTypeClass();
            if (pType != nullptr)
                pType->SetName(pID);
        }
        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }
}

// ----------------------------------------------------------------------------
// Read_AI - [AI] section
// ----------------------------------------------------------------------------

void RulesClass::Read_AI(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "AI";

    // asm: RulesClass_Addition_AI 的第一条指令就是 Find_Section("...")：
    // 段不存在时整段跳过。缺这条门会让每个键都走"缺省=字段原值"的路径——
    // 看着等价，但向量键会先 Clear() 再填，段缺失就会把已有列表清空。
    if (!pINI->SectionExists(section))
        return;

    // AITrigger* / AISafeDistance / HarvesterTooFarDistance / ChronoHarvTooFarDistance /
    // AIRestrictReplaceTime / ThreatPerOccupant / ApproachTargetResetMultiplier /
    // CampaignMoneyDelta* / GuardAreaTargetingDelay / NormalTargetingDelay /
    // AINavalYardAdjacency / MaximumBuildingPlacementFailures /
    // DissolveUnfilledTeamDelay / AIAlternateProductionCreditCutoff /
    // AIUseTurbineUpgradeProbability 全部属于 [General]（asm:
    // RulesClass_Addition_General），Read_AI 只读 asm 里 [AI] 段的 58 个键。
    // AICaptureLowMoneyMark / AICaptureWoundedMark / AISuperDefenseFrames /
    // AISuperDefenseDistance 同样在 Read_General 里读。

    // PurifierBonus 属于 [General]（Read_General）；Occupy* / Bunker* /
    // OpenTopped* / FallingDamageMultiplier 属于 [CombatDamage]（Read_CombatDamage）。
    // asm 里 [AI] 段（RulesClass_Addition_AI）并不含这些键，往这里误读会在
    // 同名键出现在 [AI] 时把 CombatDamage 的值覆盖掉，故整体移除。

    PatrolScan                        = pINI->ReadFixed(section, "PatrolScan", PatrolScan);
    // BaseDefenseSuspendSeconds 在 asm 的 [AI] 段里并不存在（原版用
    // SuspendPriority / SuspendDelay，两者都在 [General]），故不再读。

    GDIWallDefense                    = pINI->ReadFixed(section, "GDIWallDefense", GDIWallDefense);
    GDIWallDefenseCoefficient         = pINI->ReadFixed(section, "GDIWallDefenseCoefficient", GDIWallDefenseCoefficient);
    NodBaseDefenseCoefficient         = pINI->ReadFixed(section, "NodBaseDefenseCoefficient", NodBaseDefenseCoefficient);
    GDIBaseDefenseCoefficient         = pINI->ReadFixed(section, "GDIBaseDefenseCoefficient", GDIBaseDefenseCoefficient);
    ComputerBaseDefenseResponse       = pINI->ReadInteger(section, "ComputerBaseDefenseResponse", ComputerBaseDefenseResponse);
    MaximumBaseDefenseValue           = pINI->ReadInteger(section, "MaximumBaseDefenseValue", MaximumBaseDefenseValue);

    AttackInterval                    = pINI->ReadFixed(section, "AttackInterval", AttackInterval);
    AttackDelay                       = pINI->ReadFixed(section, "AttackDelay", AttackDelay);
    PowerEmergency                    = pINI->ReadFixed(section, "PowerEmergency", PowerEmergency);

    // 威胁系数族（MyEffectiveness / Target* / Dumb* / EnemyHouseThreatBonus）与
    // TurboBoost 在 asm 里分别属于 [General] 与 [CombatDamage]，已由 Read_General
    // 与 Read_CombatDamage 读取，这里不再按 [AI] 段重复读。

    AirstripRatio                     = pINI->ReadFixed(section, "AirstripRatio", AirstripRatio);
    AirstripLimit                     = pINI->ReadInteger(section, "AirstripLimit", AirstripLimit);
    HelipadRatio                      = pINI->ReadFixed(section, "HelipadRatio", HelipadRatio);
    HelipadLimit                      = pINI->ReadInteger(section, "HelipadLimit", HelipadLimit);
    TeslaRatio                        = pINI->ReadFixed(section, "TeslaRatio", TeslaRatio);
    TeslaLimit                        = pINI->ReadInteger(section, "TeslaLimit", TeslaLimit);
    AARatio                           = pINI->ReadFixed(section, "AARatio", AARatio);
    AALimit                           = pINI->ReadInteger(section, "AALimit", AALimit);
    DefenseRatio                      = pINI->ReadFixed(section, "DefenseRatio", DefenseRatio);
    DefenseLimit                      = pINI->ReadInteger(section, "DefenseLimit", DefenseLimit);
    WarRatio                          = pINI->ReadFixed(section, "WarRatio", WarRatio);
    WarLimit                          = pINI->ReadInteger(section, "WarLimit", WarLimit);
    BarracksRatio                     = pINI->ReadFixed(section, "BarracksRatio", BarracksRatio);
    BarracksLimit                     = pINI->ReadInteger(section, "BarracksLimit", BarracksLimit);
    RefineryLimit                     = pINI->ReadInteger(section, "RefineryLimit", RefineryLimit);
    RefineryRatio                     = pINI->ReadFixed(section, "RefineryRatio", RefineryRatio);
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
    PathDelay                        = pINI->ReadFixed(section, "PathDelay", PathDelay);
    BlockagePathDelay                = pINI->ReadInteger(section, "BlockagePathDelay", BlockagePathDelay);
    AutocreateTime                   = pINI->ReadFixed(section, "AutocreateTime", AutocreateTime);
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
 // Read_Powerups - RulesClass_Addition_Powerups
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

    // asm: RulesClass_Addition_Powerups 只枚举 [Powerups] 自己的 19 个箱子键
    // （strlist_CrateTypes，每个值形如 "weight,anim,naval,multiplier"）。
    // Crates 属于 [MultiplayerDialogSettings]、CrateMinimum / CrateMaximum /
    // UnitCrateType 属于 [CrateRules]、DropZoneAnim 属于 [General]，
    // 各自已在对应段读取，这里不再按 [Powerups] 段误读。

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

// asm RulesClass_Addition_LandCharacteristics: the value read from the INI is
// compared against 1.0; anything >= 1.0 collapses to 1.0, otherwise the value
// is kept.  Movement multipliers therefore never exceed 1.0.
inline float ClampLandMultiplier(double v)
{
    return (v < 1.0) ? static_cast<float>(v) : 1.0f;
}

} // namespace

void RulesClass::Read_LandCharacteristics(CCINIClass* pINI)
{
    if (!pINI) return;

    for (int32 i = 0; i < LAND_TYPE_COUNT; ++i) {
        const char* section = LandCharacteristicsSections[i];

        // asm RulesClass_Addition_LandCharacteristics 对本节先 Find_Section；
        // 段不存在则整行跳过、保留旧值（不清零、不复位）。
        if (!pINI->SectionExists(section))
            continue;

        LandTypeCharacteristics& row = LandCharacteristics[i];

        // 每个键（除 Winged）都以 1.0 为 GetFixed 缺省；读回后与 1.0 比较，
        // >=1.0 回落成 1.0（移动代价上限），<1.0 保留读到的值（asm: fcomp 1.0 /
        // test ah,1 / jz -> 用 1.0）。Winged 由 asm 直接硬写 1.0，不读 INI。
        row.Hover       = ClampLandMultiplier(pINI->ReadFixed(section, "Hover", 1.0));
        row.Foot        = ClampLandMultiplier(pINI->ReadFixed(section, "Foot", 1.0));
        row.Track       = ClampLandMultiplier(pINI->ReadFixed(section, "Track", 1.0));
        row.Wheel       = ClampLandMultiplier(pINI->ReadFixed(section, "Wheel", 1.0));
        row.Winged      = 1.0f;   // asm: mov dword ptr [ebx+0Ch],1.0
        row.Float       = ClampLandMultiplier(pINI->ReadFixed(section, "Float", 1.0));
        row.Amphibious  = ClampLandMultiplier(pINI->ReadFixed(section, "Amphibious", 1.0));
        row.FloatBeach  = ClampLandMultiplier(pINI->ReadFixed(section, "FloatBeach", 1.0));
        // asm: GetBool(section,"Buildable", 0) 后 mov [ebx+1Ch],al（缺省 0）。
        row.Buildable   = pINI->ReadBool(section, "Buildable", false);
    }
}

// ----------------------------------------------------------------------------
// Get_Movement_Multiplier - movement cost table lookup
//
//   Mirrors the binary's `LandCharacteristics.Foot[land*9 + speed]` indexing:
//   the row is nine dwords, speed classes laid out Foot, Track, Wheel, Hover,
//   Winged, Float, Amphibious, FloatBeach, Buildable.  SpeedType::Count (8) is
//   the Buildable byte, not a speed, so only Foot..FloatBeach (0..7) are valid.
// ----------------------------------------------------------------------------
double RulesClass::Get_Movement_Multiplier(int32 landType, int32 speedType) const
{
    if (landType < 0 || landType >= LAND_TYPE_COUNT) return 0.0;
    if (speedType < 0 || speedType >= static_cast<int32>(SpeedType::Count)) return 0.0;

    const LandTypeCharacteristics& row = LandCharacteristics[landType];
    const float* values = &row.Foot;    // Foot, Track, Wheel, Hover, Winged,
                                        // Float, Amphibious, FloatBeach
    return static_cast<double>(values[speedType]);
}

// ----------------------------------------------------------------------------
// Read_IQ - [IQ] section
// ----------------------------------------------------------------------------

void RulesClass::Read_IQ(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "IQ";

    // asm: RulesClass_Addition_IQ 的第一条指令就是 Find_Section("...")：
    // 段不存在时整段跳过。缺这条门会让每个键都走"缺省=字段原值"的路径——
    // 看着等价，但向量键会先 Clear() 再填，段缺失就会把已有列表清空。
    if (!pINI->SectionExists(section))
        return;

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
//
//   asm: RulesData_LoadJumpjetControls (asm:1289101)。段内只有 7 个键，且
//   GateUp / GateDown **不在这里**——它们是 [AudioVisual] 的"声音名→VOC 索引"
//   键（asm:1273880 / 1273907，GetString + VocClass_FindIndexOfName），由
//   Read_AudioVisual 负责。旧代码把这两个键当整数从本节读，既读错段又读错
//   类型，按 asm 删除。
// ----------------------------------------------------------------------------

void RulesClass::Read_JumpjetControls(CCINIClass* pINI)
{
    if (!pINI) return;
    if (!pINI->SectionExists("JumpjetControls")) return;

    const char* section = "JumpjetControls";

    // asm 顺序: TurnRate, Speed, Climb, CruiseHeight, Acceleration,
    //            WobblesPerSecond, WobbleDeviation。
    TurnRate          = pINI->ReadInteger(section, "TurnRate", TurnRate);
    Speed             = pINI->ReadInteger(section, "Speed", Speed);
    Climb             = pINI->ReadFixed(section, "Climb", Climb);
    CruiseHeight      = pINI->ReadInteger(section, "CruiseHeight", CruiseHeight);
    Acceleration      = pINI->ReadFixed(section, "Acceleration", Acceleration);
    WobblesPerSecond  = pINI->ReadFixed(section, "WobblesPerSecond", WobblesPerSecond);
    WobbleDeviation   = pINI->ReadInteger(section, "WobbleDeviation", WobbleDeviation);
}

// ----------------------------------------------------------------------------
void RulesClass::Read_Difficulties(CCINIClass* pINI)
{
    if (!pINI) return;

    // asm: RulesClass_Addition_Difficulty (0x66D280) —— 同一个 proc 被连着调用
    // 三次，段名分别是 "Easy" / "Normal" / "Difficult"（asm: RulesClass_Addition
    // 里三次 push 后紧跟三个 call）。每次：
    //   1. Find_Section(section)；不存在 -> 整段跳过，三个 scalar 都不动；
    //   2. 12 个键全用**硬编码常量**做缺省（不是字段现值）：
    //        FirePower/Groundspeed/Airspeed/Armor/ROF/Cost/BuildTime -> 1.0
    //        RepairDelay -> 0.02   BuildDelay -> 0.03
    //        BuildSlowdown -> false  DestroyWalls -> true  ContentScan -> false
    //   注：Difficulty 结构体在构造时是清零的，若沿用"字段现值作缺省"，段缺失时
    //   会留下 0 而不是原版的 1.0，故这里按 asm 常量走。
    static const char* sections[3] = { "Easy", "Normal", "Difficult" };

    for (int32 d = 0; d < 3; ++d)
    {
        const char* section = sections[d];

        if (!pINI->SectionExists(section))
            continue;

        DifficultyStruct& diff = Difficulties[d];

        diff.Firepower     = pINI->ReadFixed(section, "FirePower",    1.0);
        diff.GroundSpeed   = pINI->ReadFixed(section, "Groundspeed",  1.0);
        diff.AirSpeed      = pINI->ReadFixed(section, "Airspeed",     1.0);
        diff.Armor         = pINI->ReadFixed(section, "Armor",        1.0);
        diff.ROF           = pINI->ReadFixed(section, "ROF",          1.0);
        diff.Cost          = pINI->ReadFixed(section, "Cost",         1.0);
        diff.RepairDelay   = pINI->ReadFixed(section, "RepairDelay",  0.02);
        diff.BuildDelay    = pINI->ReadFixed(section, "BuildDelay",   0.03);
        diff.BuildSlowdown = pINI->ReadBool (section, "BuildSlowdown", false);
        diff.BuildTime     = pINI->ReadFixed(section, "BuildTime",    1.0);
        diff.DestroyWalls  = pINI->ReadBool (section, "DestroyWalls",  true);
        diff.ContentScan   = pINI->ReadBool (section, "ContentScan",   false);
    }
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// Read_Movies - [Movies] section
// ----------------------------------------------------------------------------

void RulesClass::Read_Movies(CCINIClass* pINI)
{
    if (!pINI) return;
    const char* section = "Movies";

    // asm: RulesClass_Addition_Movies (0x674560)
    //   count = Section_GetValueCount("Movies")；
    //   for i in [0,count):
    //       key = Section_GetKeyName("Movies", i)；      // 序号键 "1","2",...
    //       GetString("Movies", key, "<none>", buf, 0x20)  // 值是影片文件名
    //       if 读失败 -> 跳过
    //       if MovieClass_FindIndexByName(buf) != -1 -> 跳过（去重）
    //       else vec_Movies 追加 strdup(buf)
    //   即：键只是序号，值才是影片名；注册顺序构成影片表的索引。
    if (!pINI->SectionExists(section))
        return;

    const int32 count = pINI->Section_GetValueCount(section);

    for (int32 i = 0; i < count; ++i)
    {
        const char* pKeyName = pINI->Section_GetKeyName(section, i);
        if (pKeyName == nullptr)
            continue;

        char name[0x20];
        name[0] = '\0';
        if (pINI->ReadString(section, pKeyName, "<none>", name, sizeof(name)) <= 0)
            continue;

        // "<none>" 是原版的"无影片"占位，FindIndexByName 对它与空串一律返回 -1，
        // 但它显然不该进影片表，这里显式跳过（否则表里会多一个假影片）。
        if (name[0] == '\0' || INIClass::IsBlank(name))
            continue;

        if (MovieClass::FindIndexByName(name) != -1)
            continue;

        MovieClass::Register(name);
    }
}

// ----------------------------------------------------------------------------
// Read_AdvancedCommandBar - asm: RulesClass_Addition_AdvancedCommandBar (asm:5961 / 1289382)
//
//   段名二选一：isMP == false 用 [AdvancedCommandBar]，否则用
//   [MultiplayerAdvancedCommandBar]（asm: ds:str_AdvancedCommandBar /
//   ds:str_MultiplayerAdvancedCommandBar）。
//
//   流程：
//     1. Find_Section(section)；不存在 -> 返回 false，什么都不改；
//     2. 先把 0x19 个按钮槽全部清空（asm: sub_6CFD20(idx,null) 循环 0x19 次）；
//     3. GetString(section,"ButtonList","",Str,0x200)；
//        读到内容则按 ',' 逐个 strtok，每个名字经按钮表解析成索引后落到槽位
//        （asm: sub_6CFCC0 解析名字 / sub_6CFD20 写槽）；
//     4. 最后以实际按钮数收尾（asm: sub_6CFDB0(count)）。
//
//   重建侧命令栏按钮表尚未落地，此处按 asm 读全 ButtonList 键，并把解析出的
//   按钮名按槽顺序存入 AdvancedCommandBarButtons，供命令栏重建时消费。
// ----------------------------------------------------------------------------
void RulesClass::Read_AdvancedCommandBar(CCINIClass* pINI, bool isMP)
{
    if (pINI == nullptr)
        return;

    const char* section = isMP ? "MultiplayerAdvancedCommandBar"
                               : "AdvancedCommandBar";

    AdvancedCommandBarIsMP = isMP;

    // 1. 段不存在：直接返回，按钮表不动。
    if (!pINI->SectionExists(section))
        return;

    // 2. 清空 0x19 个槽。
    for (int32 i = 0; i < ADVANCED_COMMAND_BAR_MAX; ++i)
        AdvancedCommandBarButtons[i][0] = '\0';
    AdvancedCommandBarButtonCount = 0;
    NoneValue2[0] = '\0';

    // 3. 读 ButtonList 并逐项入库。
    char str[0x200];
    str[0] = '\0';
    if (pINI->ReadString(section, "ButtonList", "", str, sizeof(str)) > 0)
    {
        int32 slot = 0;
        char* token = std::strtok(str, ",");
        while (token != nullptr && slot < ADVANCED_COMMAND_BAR_MAX)
        {
            StringHelpers::Trim(token);
            if (*token != '\0')
            {
                std::strncpy(AdvancedCommandBarButtons[slot],
                             token, sizeof(AdvancedCommandBarButtons[slot]));
                AdvancedCommandBarButtons[slot]
                    [sizeof(AdvancedCommandBarButtons[slot]) - 1] = '\0';
                ++slot;
            }
            token = std::strtok(nullptr, ",");
        }
        AdvancedCommandBarButtonCount = slot;
    }
}

// ============================================================================
// PointerGotInvalid - Handle expired pointer references
// ============================================================================

// asm: RulesClass_PointerGotInvalid (0x67A100, retn 8)
//   原版是四百多行"逐槽 cmp / jz -> mov 0"的平铺比较：RulesClass 里凡是指向
//   AbstractClass 派生类型的槽位（动画模板、弹头、建筑/步兵/单位/飞机/覆盖物
//   类型、粒子系统、voxel 动画、子弹、武器、地形），都要在对象消亡时被清空，
//   否则整份规则会留下悬垂指针。
//   这里按"字段类型"分组把全部槽位铺开：语义与原版逐个 cmp 完全一致，只是
//   顺序按类型聚合而非按内存偏移。
//   注：BOMBCURS_SHP / CHRONOSK_SHP 是 SHP 资源指针（非 AbstractClass），
//   原版这段不处理它们。
void RulesClass::PointerGotInvalid(AbstractClass* pInvalid, bool /*removed*/)
{
    if (pInvalid == nullptr)
        return;

    // ---- AnimTypeClass : 44 个槽 ----
    if (DropPodPuff == reinterpret_cast<AnimTypeClass*>(pInvalid)) DropPodPuff = nullptr;
    if (BarrelExplode == reinterpret_cast<AnimTypeClass*>(pInvalid)) BarrelExplode = nullptr;
    if (Wake == reinterpret_cast<AnimTypeClass*>(pInvalid)) Wake = nullptr;
    if (NukeTakeOff == reinterpret_cast<AnimTypeClass*>(pInvalid)) NukeTakeOff = nullptr;
    if (InfantryExplode == reinterpret_cast<AnimTypeClass*>(pInvalid)) InfantryExplode = nullptr;
    if (FlamingInfantry == reinterpret_cast<AnimTypeClass*>(pInvalid)) FlamingInfantry = nullptr;
    if (InfantryHeadPop == reinterpret_cast<AnimTypeClass*>(pInvalid)) InfantryHeadPop = nullptr;
    if (InfantryNuked == reinterpret_cast<AnimTypeClass*>(pInvalid)) InfantryNuked = nullptr;
    if (InfantryVirus == reinterpret_cast<AnimTypeClass*>(pInvalid)) InfantryVirus = nullptr;
    if (InfantryBrute == reinterpret_cast<AnimTypeClass*>(pInvalid)) InfantryBrute = nullptr;
    if (InfantryMutate == reinterpret_cast<AnimTypeClass*>(pInvalid)) InfantryMutate = nullptr;
    if (Behind == reinterpret_cast<AnimTypeClass*>(pInvalid)) Behind = nullptr;
    if (VeinAttack == reinterpret_cast<AnimTypeClass*>(pInvalid)) VeinAttack = nullptr;
    if (Dig == reinterpret_cast<AnimTypeClass*>(pInvalid)) Dig = nullptr;
    if (IonBlast == reinterpret_cast<AnimTypeClass*>(pInvalid)) IonBlast = nullptr;
    if (IonBeam == reinterpret_cast<AnimTypeClass*>(pInvalid)) IonBeam = nullptr;
    if (WeatherConBoltExplosion == reinterpret_cast<AnimTypeClass*>(pInvalid)) WeatherConBoltExplosion = nullptr;
    if (DominatorFirstAnim == reinterpret_cast<AnimTypeClass*>(pInvalid)) DominatorFirstAnim = nullptr;
    if (DominatorSecondAnim == reinterpret_cast<AnimTypeClass*>(pInvalid)) DominatorSecondAnim = nullptr;
    if (DrainAnimationType == reinterpret_cast<AnimTypeClass*>(pInvalid)) DrainAnimationType = nullptr;
    if (ControlledAnimationType == reinterpret_cast<AnimTypeClass*>(pInvalid)) ControlledAnimationType = nullptr;
    if (PermaControlledAnimationType == reinterpret_cast<AnimTypeClass*>(pInvalid)) PermaControlledAnimationType = nullptr;
    if (ChronoBlast == reinterpret_cast<AnimTypeClass*>(pInvalid)) ChronoBlast = nullptr;
    if (ChronoBlastDest == reinterpret_cast<AnimTypeClass*>(pInvalid)) ChronoBlastDest = nullptr;
    if (ChronoPlacement == reinterpret_cast<AnimTypeClass*>(pInvalid)) ChronoPlacement = nullptr;
    if (ChronoBeam == reinterpret_cast<AnimTypeClass*>(pInvalid)) ChronoBeam = nullptr;
    if (WarpIn == reinterpret_cast<AnimTypeClass*>(pInvalid)) WarpIn = nullptr;
    if (WarpOut == reinterpret_cast<AnimTypeClass*>(pInvalid)) WarpOut = nullptr;
    if (WarpAway == reinterpret_cast<AnimTypeClass*>(pInvalid)) WarpAway = nullptr;
    if (ChronoSparkle1 == reinterpret_cast<AnimTypeClass*>(pInvalid)) ChronoSparkle1 = nullptr;
    if (IronCurtainInvokeAnim == reinterpret_cast<AnimTypeClass*>(pInvalid)) IronCurtainInvokeAnim = nullptr;
    if (ForceShieldInvokeAnim == reinterpret_cast<AnimTypeClass*>(pInvalid)) ForceShieldInvokeAnim = nullptr;
    if (WeaponNullifyAnim == reinterpret_cast<AnimTypeClass*>(pInvalid)) WeaponNullifyAnim = nullptr;
    if (AtmosphereEntry == reinterpret_cast<AnimTypeClass*>(pInvalid)) AtmosphereEntry = nullptr;
    if (Smoke == reinterpret_cast<AnimTypeClass*>(pInvalid)) Smoke = nullptr;
    if (Smoke_ == reinterpret_cast<AnimTypeClass*>(pInvalid)) Smoke_ = nullptr;
    if (MoveFlash == reinterpret_cast<AnimTypeClass*>(pInvalid)) MoveFlash = nullptr;
    if (BombParachute == reinterpret_cast<AnimTypeClass*>(pInvalid)) BombParachute = nullptr;
    if (Parachute == reinterpret_cast<AnimTypeClass*>(pInvalid)) Parachute = nullptr;
    if (SmallFire == reinterpret_cast<AnimTypeClass*>(pInvalid)) SmallFire = nullptr;
    if (LargeFire == reinterpret_cast<AnimTypeClass*>(pInvalid)) LargeFire = nullptr;
    if (DropZoneAnim == reinterpret_cast<AnimTypeClass*>(pInvalid)) DropZoneAnim = nullptr;
    if (EMPulseSparkles == reinterpret_cast<AnimTypeClass*>(pInvalid)) EMPulseSparkles = nullptr;
    if (OreTwinkle == reinterpret_cast<AnimTypeClass*>(pInvalid)) OreTwinkle = nullptr;

    // ---- WarheadTypeClass : 22 个槽 ----
    if (field_FA8 == reinterpret_cast<WarheadTypeClass*>(pInvalid)) field_FA8 = nullptr;
    if (CrushWarhead2 == reinterpret_cast<WarheadTypeClass*>(pInvalid)) CrushWarhead2 = nullptr;
    if (DominatorWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) DominatorWarhead = nullptr;
    if (FlameDamage == reinterpret_cast<WarheadTypeClass*>(pInvalid)) FlameDamage = nullptr;
    if (FlameDamage2 == reinterpret_cast<WarheadTypeClass*>(pInvalid)) FlameDamage2 = nullptr;
    if (NukeWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) NukeWarhead = nullptr;
    if (Apply100Warhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) Apply100Warhead = nullptr;
    if (MutateWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) MutateWarhead = nullptr;
    if (MutateExplosionWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) MutateExplosionWarhead = nullptr;
    if (EMPulseWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) EMPulseWarhead = nullptr;
    if (C4Warhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) C4Warhead = nullptr;
    if (CrushWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) CrushWarhead = nullptr;
    if (V3Warhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) V3Warhead = nullptr;
    if (DMislWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) DMislWarhead = nullptr;
    if (V3EliteWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) V3EliteWarhead = nullptr;
    if (DMislEliteWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) DMislEliteWarhead = nullptr;
    if (CMislWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) CMislWarhead = nullptr;
    if (CMislEliteWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) CMislEliteWarhead = nullptr;
    if (IvanWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) IvanWarhead = nullptr;
    if (IonCannonWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) IonCannonWarhead = nullptr;
    if (LightningWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) LightningWarhead = nullptr;
    if (RadSiteWarhead == reinterpret_cast<WarheadTypeClass*>(pInvalid)) RadSiteWarhead = nullptr;

    // ---- BuildingTypeClass : 10 个槽 ----
    if (PrismType == reinterpret_cast<BuildingTypeClass*>(pInvalid)) PrismType = nullptr;
    if (GDIGateOne == reinterpret_cast<BuildingTypeClass*>(pInvalid)) GDIGateOne = nullptr;
    if (GDIGateTwo == reinterpret_cast<BuildingTypeClass*>(pInvalid)) GDIGateTwo = nullptr;
    if (NodGateOne == reinterpret_cast<BuildingTypeClass*>(pInvalid)) NodGateOne = nullptr;
    if (NodGateTwo == reinterpret_cast<BuildingTypeClass*>(pInvalid)) NodGateTwo = nullptr;
    if (WallTower == reinterpret_cast<BuildingTypeClass*>(pInvalid)) WallTower = nullptr;
    if (GDIPowerPlant == reinterpret_cast<BuildingTypeClass*>(pInvalid)) GDIPowerPlant = nullptr;
    if (NodRegularPower == reinterpret_cast<BuildingTypeClass*>(pInvalid)) NodRegularPower = nullptr;
    if (NodAdvancedPower == reinterpret_cast<BuildingTypeClass*>(pInvalid)) NodAdvancedPower = nullptr;
    if (ThirdPowerPlant == reinterpret_cast<BuildingTypeClass*>(pInvalid)) ThirdPowerPlant = nullptr;

    // ---- InfantryTypeClass : 10 个槽 ----
    if (Paratrooper == reinterpret_cast<InfantryTypeClass*>(pInvalid)) Paratrooper = nullptr;
    if (AlliedDisguise == reinterpret_cast<InfantryTypeClass*>(pInvalid)) AlliedDisguise = nullptr;
    if (SovietDisguise == reinterpret_cast<InfantryTypeClass*>(pInvalid)) SovietDisguise = nullptr;
    if (ThirdDisguise == reinterpret_cast<InfantryTypeClass*>(pInvalid)) ThirdDisguise = nullptr;
    if (Technician == reinterpret_cast<InfantryTypeClass*>(pInvalid)) Technician = nullptr;
    if (Engineer == reinterpret_cast<InfantryTypeClass*>(pInvalid)) Engineer = nullptr;
    if (Pilot == reinterpret_cast<InfantryTypeClass*>(pInvalid)) Pilot = nullptr;
    if (AlliedCrew == reinterpret_cast<InfantryTypeClass*>(pInvalid)) AlliedCrew = nullptr;
    if (SovietCrew == reinterpret_cast<InfantryTypeClass*>(pInvalid)) SovietCrew = nullptr;
    if (ThirdCrew == reinterpret_cast<InfantryTypeClass*>(pInvalid)) ThirdCrew = nullptr;

    // ---- UnitTypeClass : 4 个槽 ----
    if (LargeVisceroid == reinterpret_cast<UnitTypeClass*>(pInvalid)) LargeVisceroid = nullptr;
    if (SmallVisceroid == reinterpret_cast<UnitTypeClass*>(pInvalid)) SmallVisceroid = nullptr;
    if (PrerequisiteProcAlternate == reinterpret_cast<UnitTypeClass*>(pInvalid)) PrerequisiteProcAlternate = nullptr;
    if (UnitCrateType == reinterpret_cast<UnitTypeClass*>(pInvalid)) UnitCrateType = nullptr;

    // ---- AircraftTypeClass : 4 个槽 ----
    if (V3RocketType == reinterpret_cast<AircraftTypeClass*>(pInvalid)) V3RocketType = nullptr;
    if (DMislType == reinterpret_cast<AircraftTypeClass*>(pInvalid)) DMislType = nullptr;
    if (CMislType == reinterpret_cast<AircraftTypeClass*>(pInvalid)) CMislType = nullptr;

    // ---- OverlayTypeClass : 3 个槽 ----
    if (WoodCrateImg == reinterpret_cast<OverlayTypeClass*>(pInvalid)) WoodCrateImg = nullptr;
    if (CrateImg == reinterpret_cast<OverlayTypeClass*>(pInvalid)) CrateImg = nullptr;
    if (WaterCrateImg == reinterpret_cast<OverlayTypeClass*>(pInvalid)) WaterCrateImg = nullptr;

    // ---- ParticleSystemTypeClass : 10 个槽 ----
    if (BarrelParticle == reinterpret_cast<ParticleSystemTypeClass*>(pInvalid)) BarrelParticle = nullptr;
    if (DefaultLargeGreySmokeSystem == reinterpret_cast<ParticleSystemTypeClass*>(pInvalid)) DefaultLargeGreySmokeSystem = nullptr;
    if (DefaultSmallGreySmokeSystem == reinterpret_cast<ParticleSystemTypeClass*>(pInvalid)) DefaultSmallGreySmokeSystem = nullptr;
    if (DefaultSparkSystem == reinterpret_cast<ParticleSystemTypeClass*>(pInvalid)) DefaultSparkSystem = nullptr;
    if (DefaultLargeRedSmokeSystem == reinterpret_cast<ParticleSystemTypeClass*>(pInvalid)) DefaultLargeRedSmokeSystem = nullptr;
    if (DefaultSmallRedSmokeSystem == reinterpret_cast<ParticleSystemTypeClass*>(pInvalid)) DefaultSmallRedSmokeSystem = nullptr;
    if (DefaultDebrisSmokeSystem == reinterpret_cast<ParticleSystemTypeClass*>(pInvalid)) DefaultDebrisSmokeSystem = nullptr;
    if (DefaultFireStreamSystem == reinterpret_cast<ParticleSystemTypeClass*>(pInvalid)) DefaultFireStreamSystem = nullptr;
    if (DefaultTestParticleSystem == reinterpret_cast<ParticleSystemTypeClass*>(pInvalid)) DefaultTestParticleSystem = nullptr;
    if (DefaultRepairParticleSystem == reinterpret_cast<ParticleSystemTypeClass*>(pInvalid)) DefaultRepairParticleSystem = nullptr;

    // ---- VoxelAnimTypeClass : 2 个槽 ----
    if (TireVoxelDebris == reinterpret_cast<VoxelAnimTypeClass*>(pInvalid)) TireVoxelDebris = nullptr;
    if (ScrapVoxelDebris == reinterpret_cast<VoxelAnimTypeClass*>(pInvalid)) ScrapVoxelDebris = nullptr;

    // ---- BulletTypeClass : 3 个槽 ----
    if (NukeProjectile == reinterpret_cast<BulletTypeClass*>(pInvalid)) NukeProjectile = nullptr;
    if (NukeDown == reinterpret_cast<BulletTypeClass*>(pInvalid)) NukeDown = nullptr;
    if (EMPulseProjectile == reinterpret_cast<BulletTypeClass*>(pInvalid)) EMPulseProjectile = nullptr;

    // ---- WeaponTypeClass : 2 个槽 ----
    if (DropPodWeapon == reinterpret_cast<WeaponTypeClass*>(pInvalid)) DropPodWeapon = nullptr;
    if (DeathWeapon == reinterpret_cast<WeaponTypeClass*>(pInvalid)) DeathWeapon = nullptr;

    // ---- TerrainTypeClass : 1 个槽 ----
    if (VeinholeTypeClass == reinterpret_cast<TerrainTypeClass*>(pInvalid)) VeinholeTypeClass = nullptr;
}


// ============================================================================
// RulesClass - Addition_* dispatch family
//
 //  RulesClass_Addition builds the entire rules set by invoking
//  one Addition_* routine per INI section in a fixed order.  The routines
//  themselves are the ones the reconstruction already provides under the
//  Read_* names; the members below carry the original binary's names and the
//  original invocation order so that the load sequence matches the assembly
//  exactly, while Read_* keeps working as the implementation body.
// ============================================================================

 // asm: int __thiscall RulesClass_Addition(RulesClass *this, CCINIClass *pINI)   (asm:1271004)
//
//  The order below mirrors the sequence of `call RulesClass_Addition_*` sites
//  in the original: the fundamental tables first, then the object type lists,
//  then the tuning sections and finally the command-bar/UI data.
void RulesClass::Addition(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return;

    CreateVectors();

    // 调用顺序严格对齐 asm RulesClass_Addition（asm:2053-2449）。后读的段会引用
    // 前段建立的类型数组与字段（例如 General 里用到的类型、AudioVisual 末尾的
    // 颜色覆盖），所以顺序不可随意调整。
    Addition_Colors(pINI);
    Addition_ColorAdd(pINI);
    Addition_Countries(pINI);
    Addition_Sides(pINI);
    Addition_OverlayTypes_obsolete(pINI);
    Addition_SuperWeaponTypes(pINI);
    Addition_Warheads(pINI);
    // 弹丸表与武器表：武器段里 Projectile= / Warhead= 要反查前两张表，
    // 所以顺序固定在 Warheads 之后、各对象段（会写 Primary=）之前。
    Read_Projectiles(pINI);
    Read_WeaponTypes(pINI);
    Addition_SmudgeTypes(pINI);
    Addition_TerrainTypes(pINI);
    Addition_BuildingTypes(pINI);
    Addition_VehicleTypes(pINI);
    Addition_AircraftTypes(pINI);
    Addition_InfantryTypes(pINI);
    Addition_Animations(pINI);
    Addition_VoxelAnims(pINI);
    Addition_Particles(pINI);
    Addition_ParticleSystems(pINI);
    Read_JumpjetControls(pINI);                 // asm: RulesData_LoadJumpjetControls
    Addition_MultiplayerDialogSettings(pINI);
    Addition_AI(pINI);
    Addition_Powerups(pINI);
    Addition_LandCharacteristics(pINI);
    Addition_IQ(pINI);
    Addition_General(pINI);
    LoadTypeData(pINI);                         // asm: RulesData_LoadTypeData（Addition_General 之后）
    Addition_Difficulty(pINI);                  // asm: Easy / Normal / Difficult
    Addition_CrateRules(pINI);
    Addition_CombatDamage(pINI);
    Addition_Radiation(pINI);
    Addition_ElevationModel(pINI);
    Addition_WallModel(pINI);
    Addition_AudioVisual(pINI);
    Addition_SpecialWeapons(pINI);
    Read_Tiberiums(pINI);                       // asm: TiberiumClass_CreateFromINIList

    // asm: isMP = (Session != 0 && Session != 5)
    //   `Session` 是 gamemd 的"模式"全局：0 = 主菜单（还没选模式），5 =
    //   过场/影片，其余（遭遇战/多人/战役）都用多人版命令栏。
    //   OpenYR 里没有这个整数全局，语义等价的现成标志是 Game::IsMultiplayer
    //   （它在主菜单阶段为 false，与 Session==0 对齐）；旧代码写死 false，
    //   等于把多人命令栏那段 INI 永远跳过——这里改为跟随实际模式。
    Addition_AdvancedCommandBar(pINI, Game::IsMultiplayer);
}

 // asm: RulesClass_CreateVectors (asm:21-652)
//
//  逐句核对结论：这 632 行**只做一件事**——把本类持有的每一个 DynamicVector
//  就地构造一遍。统计 asm 全文可见：
//      54 次 `<Type>_CTOR` + 41 次 `TypeList<...>::vftable` 覆盖（每种容器
//      一次），再加 2 次 `push offset ZeroOutColor`（向量元素析构回调）。
//      全文 **0 次** SetCapacity / SetIncrement（`call dword ptr [eax+8]`
//      与 `[edi+18h]` 写入的计数都是 0），也就是说原版**不预分配容量**，
//      容器的 incrementAmount 保持构造缺省（10）。
//
//  重建侧这些向量是成员对象，构造函数里已经全部建好（缺省 increment 同样是
//  10），因此这里不需要任何语句——不是留白，而是这一步在 C++ 里由语言保证。
//  保留函数体只为让 Read_File / Init 的调用序与原版逐句对应。
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
void RulesClass::Addition_AdvancedCommandBar(CCINIClass* pINI, bool isMP) { Read_AdvancedCommandBar(pINI, isMP); }
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
// asm: RulesClass_Addition_SpecialWeapons 解析 [SpecialWeapons]（核弹/突变/EMP
// 弹头与弹丸），与 [SuperWeaponTypes] 的类型注册是不同的 proc。
void RulesClass::Addition_SpecialWeapons(CCINIClass* pINI)   { Read_SpecialWeapons(pINI); }
void RulesClass::Addition_CrateRules(CCINIClass* pINI)      { Read_CrateRules(pINI); }
void RulesClass::Addition_AudioVisual(CCINIClass* pINI)     { Read_AudioVisual(pINI); }
// ----------------------------------------------------------------------------
// Addition_Teams_obsolete - asm: RulesClass_Addition_Teams_obsolete (asm:1286074)
//
//   遍历 [Teams] 段的每个键：取键值名，经 TeamTypeClass_FindOrAllocate 注册
//   一个队伍类型，再调用其 vtable+0x64（LoadFromINI）用同一个 INI 载入它。
//   返回 (键数 > 0)。名字为空（ReadString 返回 0）时 asm 会跳过该键、不建类型
//   （eax 置 0 后仍调用 [eax] 会在原版里崩溃，但实际 rules 不会有空项，这里按
//   "有名字才建"的安全等价实现）。
// ----------------------------------------------------------------------------
void RulesClass::Addition_Teams_obsolete(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return;

    const char* section = "Teams";
    const int32 count = pINI->Section_GetValueCount(section);
    if (count <= 0)
        return;

    for (int32 i = 0; i < count; ++i)
    {
        const char* pKey = pINI->Section_GetKeyName(section, i);
        if (pKey == nullptr)
            continue;

        char name[0x80];
        if (pINI->ReadString(section, pKey, "", name, sizeof(name)) <= 0)
            continue;

        TeamTypeClass* pType = TeamTypeClass::FindOrAllocate(name);
        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }
}
void RulesClass::Addition_OverlayTypes_obsolete(CCINIClass* pINI) { Read_OverlayTypes(pINI); }
void RulesClass::LoadDifficulties_unused(CCINIClass* pINI)  { Read_Difficulties(pINI); }

// ============================================================================
// asm: RulesData_LoadTypeData (0x679A00 区, __stdcall(INI), retn 4)
//
//   for (vec_HouseTypes)          [i]->vt+64h(INI)
//   for (vec_SuperWeaponTypes)    ...
//   for (vec_AnimTypes)           ...(ART_INI)          <- asm: push offset ART_INI
//   for (BuildingTypeClass.Vector)[i]->vt+64h(INI)
//   for (vec_AircraftTypes)       ...
//   for (vec_UnitTypes)           ...
//   for (vec_InfantryTypes)       ...
//   for (vec_WeaponTypes)         ...
//   for (vec_BulletTypes)         ...
//   for (vec_WarheadTypes)        ...
//   for (vec_WeaponTypes)         WeaponTypeClass_CalculateSpeed
//   for (BuildingTypeClass.Vector)nullsub_7            <- 原版就是空子程序
//   for (vec_TerrainTypes)        ...
//   for (vec_SmudgeTypes)         ...                  <- unk_A8EC1C/A8EC28
//   for (vec_OverlayTypes)        ...                  <- unk_A83D90
//   for (vec_IsometricTileTypes)  ...                  <- unk_A83D9C/A83DA8
//   for (vec_ParticleTypes)       ...                  <- unk_A83D6C/A83D78
//   for (vec_VoxelAnimTypes)      ...
//   edi = MissionControls ; do { [edi] = 0 ; LoadMissionControlFromINI(edi, INI) ;
//                                 edi += 20h } while (edi < dword_A8E7A8)
//   return 1
// ============================================================================
void RulesClass::LoadTypeData(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return;

    // asm: vec_HouseTypes
    for (int32 i = 0; i < HouseTypeClass::ArrayCount; ++i) {
        if (HouseTypeClass::Array[i] != nullptr)
            HouseTypeClass::Array[i]->LoadFromINI(pINI);
    }

    // asm: vec_SuperWeaponTypes
    if (SuperWeaponTypeClass::Array != nullptr)
        for (int32 i = 0; i < SuperWeaponTypeClass::Array->Count; ++i)
            if ((*SuperWeaponTypeClass::Array)[i] != nullptr)
                (*SuperWeaponTypeClass::Array)[i]->LoadFromINI(pINI);

    // asm: vec_AnimTypes -> ART_INI（原版直接 push offset ART_INI）
    if (AnimTypeClass::Array != nullptr) {
        CCINIClass* pArtINI = CCINIClass::GetArtINI();
        for (int32 i = 0; i < AnimTypeClass::Array->Count; ++i)
            if ((*AnimTypeClass::Array)[i] != nullptr)
                (*AnimTypeClass::Array)[i]->LoadFromINI(pArtINI);
    }

    // asm: BuildingTypeClass.Vector
    if (BuildingTypeClass::Array != nullptr)
        for (int32 i = 0; i < BuildingTypeClass::Array->Count; ++i)
            if ((*BuildingTypeClass::Array)[i] != nullptr)
                (*BuildingTypeClass::Array)[i]->LoadFromINI(pINI);

    // asm: vec_AircraftTypes
    if (AircraftTypeClass::Array != nullptr)
        for (int32 i = 0; i < AircraftTypeClass::Array->Count; ++i)
            if ((*AircraftTypeClass::Array)[i] != nullptr)
                (*AircraftTypeClass::Array)[i]->LoadFromINI(pINI);

    // asm: vec_UnitTypes
    if (UnitTypeClass::Array != nullptr)
        for (int32 i = 0; i < UnitTypeClass::Array->Count; ++i)
            if ((*UnitTypeClass::Array)[i] != nullptr)
                (*UnitTypeClass::Array)[i]->LoadFromINI(pINI);

    // asm: vec_InfantryTypes
    if (InfantryTypeClass::Array != nullptr)
        for (int32 i = 0; i < InfantryTypeClass::Array->Count; ++i)
            if ((*InfantryTypeClass::Array)[i] != nullptr)
                (*InfantryTypeClass::Array)[i]->LoadFromINI(pINI);

    // asm: vec_WeaponTypes
    if (WeaponTypeClass::Array != nullptr)
        for (int32 i = 0; i < WeaponTypeClass::Array->Count; ++i)
            if ((*WeaponTypeClass::Array)[i] != nullptr)
                (*WeaponTypeClass::Array)[i]->LoadFromINI(pINI);

    // asm: vec_BulletTypes
    if (BulletTypeClass::Array != nullptr)
        for (int32 i = 0; i < BulletTypeClass::Array->Count; ++i)
            if ((*BulletTypeClass::Array)[i] != nullptr)
                (*BulletTypeClass::Array)[i]->LoadFromINI(pINI);

    // asm: vec_WarheadTypes
    if (WarheadTypeClass::Array != nullptr)
        for (int32 i = 0; i < WarheadTypeClass::Array->Count; ++i)
            if ((*WarheadTypeClass::Array)[i] != nullptr)
                (*WarheadTypeClass::Array)[i]->LoadFromINI(pINI);

    // asm: vec_WeaponTypes -> WeaponTypeClass_CalculateSpeed
    if (WeaponTypeClass::Array != nullptr)
        for (int32 i = 0; i < WeaponTypeClass::Array->Count; ++i)
            if ((*WeaponTypeClass::Array)[i] != nullptr)
                (*WeaponTypeClass::Array)[i]->CalculateSpeed();

    // asm: BuildingTypeClass.Vector -> nullsub_7（原版该子程序为空，无动作）

    // asm: vec_TerrainTypes
    if (TerrainTypeClass::Array != nullptr)
        for (int32 i = 0; i < TerrainTypeClass::Array->Count; ++i)
            if ((*TerrainTypeClass::Array)[i] != nullptr)
                (*TerrainTypeClass::Array)[i]->LoadFromINI(pINI);

    // asm: vec_SmudgeTypes (unk_A8EC1C/A8EC28)
    if (SmudgeTypeClass::Array != nullptr)
        for (int32 i = 0; i < SmudgeTypeClass::Array->Count; ++i)
            if ((*SmudgeTypeClass::Array)[i] != nullptr)
                (*SmudgeTypeClass::Array)[i]->LoadFromINI(pINI);

    // asm: vec_OverlayTypes
    if (OverlayTypeClass::Array != nullptr)
        for (int32 i = 0; i < OverlayTypeClass::Array->Count; ++i)
            if ((*OverlayTypeClass::Array)[i] != nullptr)
                (*OverlayTypeClass::Array)[i]->LoadFromINI(pINI);

    // asm: vec_IsometricTileTypes (unk_A83D9C/A83DA8)
    if (TileTypeClass_Array != nullptr)
        for (int32 i = 0; i < TileTypeClass_Array->Count; ++i)
            if ((*TileTypeClass_Array)[i] != nullptr)
                (*TileTypeClass_Array)[i]->LoadFromINI(pINI);

    // asm: vec_ParticleTypes (unk_A83D6C/A83D78)
    if (ParticleTypeClass::Array != nullptr)
        for (int32 i = 0; i < ParticleTypeClass::Array->Count; ++i)
            if ((*ParticleTypeClass::Array)[i] != nullptr)
                (*ParticleTypeClass::Array)[i]->LoadFromINI(pINI);

    // asm: vec_VoxelAnimTypes
    if (VoxelAnimTypeClass::Array != nullptr)
        for (int32 i = 0; i < VoxelAnimTypeClass::Array->Count; ++i)
            if ((*VoxelAnimTypeClass::Array)[i] != nullptr)
                (*VoxelAnimTypeClass::Array)[i]->LoadFromINI(pINI);

    // asm: MissionControls（0x20 步长，直到 dword_A8E7A8）
    MissionClass::LoadMissionControlFromINI(pINI);
}
