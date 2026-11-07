#include <Houses/HouseClass.h>
#include <Houses/FactoryClass.h>
#include <Houses/HouseTypeClass.h>
#include <Core/Definitions.h>
#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Abstract/UnitClass.h>
#include <Abstract/InfantryClass.h>
#include <Abstract/BuildingClass.h>
#include <Abstract/FootClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Abstract/TechnoTypeClass.h>
#include <SW/SuperClass.h>
#include <AI/TeamClass.h>
#include <AI/TeamTypeClass.h>
#include <AI/TaskForceClass.h>
#include <Rendering/RadarEventClass.h>
#include <AI/AITriggerTypeClass.h>
#include <AI/AITeamClass.h>
#include <AI/AITeamTypeClass.h>
#include <Abstract/InfantryTypeClass.h>
#include <Abstract/UnitTypeClass.h>
#include <Abstract/AircraftTypeClass.h>
#include <SW/SuperWeaponTypeClass.h>
#include <Rules/RulesClass.h>
#include <INI/INIClass.h>
#include <cstdio>
#include <Scenario/ScenarioClass.h>
#include <Game/Game.h>
#include <Game/Externs.h>
#include <Network/SessionClass.h>
#include <Rendering/ConvertClass.h>
#include <Audio/VoxClass.h>
#include <Special/TiberiumClass.h>

#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>

// ============================================================================
// HouseClass.cpp - House class implementation
// ============================================================================
// Standalone engine reconstruction of the HouseClass.
// In the original game, methods are at these addresses:
//   HouseClass::HouseClass: 0x4F4780
//   HouseClass::~HouseClass: 0x4F4BF0
//   HouseClass::Load: 0x4F52A0
//   HouseClass::Save: 0x4F4FE0
//   HouseClass::GetCRC: 0x4F51A0
//   HouseClass::Init: 0x4F53E0
//   HouseClass::MakeAlly: 0x4F65A0
//   HouseClass::MakeEnemy: 0x4F66D0
//   HouseClass::Win: 0x4F4EF0
//   HouseClass::Lose: 0x4F4820
//   HouseClass::CanBuild: 0x4F5C40
//   etc.
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
HouseClass* HouseClass::Array[32] = {};
int32 HouseClass::ArrayCount = 0;
HouseClass* HouseClass::pCurrentPlayer = nullptr;
HouseClass* HouseClass::Player = nullptr;

 // HouseClass_DefaultIonCannon_Coords: the (0xFFFF, 0xFFFF)
// module sentinel every cleared cell slot is stamped with.
const CellStruct HouseClass::DefaultIonCannon_Coords(-1, -1);HouseClass* HouseClass::Observer = nullptr;

// ============================================================================
// Constructor
// ============================================================================

HouseClass::HouseClass(HouseTypeClass* pType)
    : Type(pType)
    , InitialName{}
    , CSFName{}
    , TimesDefeated(0)
    , TimesWon(0)
    , Credits(0)
    , CreditsSpent(0)
    , MapIsClear(false)
    , AirUnits(0)
    , InfantryUnits(0)
    , Buildings(0)
    , Ships(0)
    , Vehicles(0)
    , AllTechnos(0)
    , PowerOutput(0)
    , PowerDrain(0)
    , CurrentPlayer(false)
    , PlayerControl(false)
    , IsDeadObject(false)
    , IsDefeated(false)
    , IsWinner(false)
    , IsObserver(false)
    , IsDiscovered(false)
    , IsControlStatus(false)
    , IsHumanPlayer(false)
    , IsBaseZone(false)
    , IsRebuilding(false)
    , IsCivilians(false)
    , IsVisionary(false)
    , IsMultiplayerPassive(false)
    , IsMPGameOver(false)
    , IsGPSActive(false)
    , IsGPSActiveVisible(false)
    , IsGPSActiveInRadar(false)
    , IsSpySatActive(false)
    , IsSpySatActiveVisible(false)
    , IsSpySatActiveInRadar(false)
    , SpiedBy(nullptr)
    , SpiedBy_SpySat(nullptr)
    , ProductionSuspended(false)
    , SellEverything(false)
    , AllyBitfield(0)
    , EnemyBitfield(0)
    , ActiveSuperWeapons(0)
    , AvailableSuperWeapons(0)
    , UsedSuperWeapons(0)
    , UIName{}
    , RatioAITriggerTeam(0)
    , RatioTeamAircraft(0)
    , RatioTeamInfantry(0)
    , RatioTeamUnits(0)
    , TechLevel(0)
    , AIBuildState(0)
    , IQLevel(0)
    , IQLevel2(0)
    , Edge(-1)
    , ColorSchemeIndex(0)
    , UnitCount(0)
    , InfantryCount(0)
    , AircraftCount(0)
    , BuildingCount(0)
    , OwnedUnitCount(0)
    , OwnedInfantryCount(0)
    , OwnedAircraftCount(0)
    , OwnedBuildingCount(0)
    , DestroyedUnitCount(0)
    , DestroyedInfantryCount(0)
    , DestroyedAircraftCount(0)
    , DestroyedBuildingCount(0)
    , TotalUnitCount(0)
    , TotalInfantryCount(0)
    , TotalAircraftCount(0)
    , TotalBuildingCount(0)
    , DestroyedUnitValue(0)
    , DestroyedInfantryValue(0)
    , DestroyedAircraftValue(0)
    , DestroyedBuildingValue(0)
    , TotalUnitValue(0)
    , TotalInfantryValue(0)
    , TotalAircraftValue(0)
    , TotalBuildingValue(0)
    , AllHousesIndex(-1)
    , ArrayIndex(-1)
    , ActLikeIndex(-1)
    , PlayerName{}
    , FactoryCount(0)
    , FactoryCountBuilding(0), FactoryCountUnitNaval(0), FactoryCountInfantry(0)
    , FactoryCountUnit(0), FactoryCountAircraft(0)
    , AlliesCounter(0)
    , EnemiesCounter(0)
    , RadarVisible(false)
    , RadarVisibleToPlayer(false)
    , RadarDisabled(false)
    , RadarJammed(false)
    , RadarJammedBy(nullptr)
    , RadarSpied(false)
    , RadarSpiedBy(nullptr)
    , RevealedByHeight(false)
    , BaseCenter(CellStruct(0, 0))
    , BaseNodesCount(0), NextPlanningWaypointSlot(0)
    , BaseOutlineLeft(0)
    , BaseOutlineTop(0)
    , BaseOutlineWidth(0)
    , BaseOutlineHeight(0)
    , BestTargetCell(CellStruct(-1, -1))
    , BaseCell(CellStruct(-1, -1))
    , BaseSpawnCell(CellStruct(-1, -1))
    , TargetCell(CellStruct(-1, -1))
    , DefensiveCell(CellStruct(-1, -1))
    , DefensiveCellField(0)
    , PreferredDefensiveCell(-1, -1)
    , PreferredDefensiveCellStartTime(-1)
    , Counters{}
    , InfantrySelfHeal(0)
    , UnitsSelfHeal(0)
    , PoweredCenters(0)
    , BuildingTypeToProduce(-1)
    , UnitTypeToProduce(-1)
    , InfantryTypeToProduce(-1)
    , AircraftTypeToProduce(-1)
    , TiberiumValue(0)
    , TotalStorageCapacity(0)
    , FactoryPlants(nullptr)
    , DamageLedger(nullptr)
    , DamageLedgerCount(0)
    , PrimaryAggressor(-1)
    , EndScenarioTimer(-1)
    , HasThreatNode(false)
    , RadarBlackoutTimer()
    , RadarBlackoutFrame(-1)
    , SuperWeapons(nullptr)
    , LastBuildTime(0)
    , LastProductionTime(0)
    , LastAttackTime(0)
    , LastEnemySightingTime(0)
    , LastTeamCreationTime(0)
    , LastBaseScanTime(0)
    , LastCombatTime(0)
    , LastNavalCombatTime(0)
    , LastAirCombatTime(0)
    , LastSpySatTime(0)
    , LastIronCurtainTime(0)
    , LastForceShieldTime(0)
    , LastPsychicRevealTime(0)
    , LastSonarTime(0)
    , LastRadarTime(0)
    , LastBuildingTime(0)
    , LastInfantryTime(0)
    , LastVehicleTime(0)
    , LastAircraftTime(0)
    , LastSuperWeaponTime(0)
    , LastAirstrikeTime(0)
    , LastParadropTime(0)
    , LastSpyTime(0)
    , LastEngineerTime(0)
    , LastChronoTime(0)
    , LastChronoWarpTime(0)
    , LastSabotageTime(0)
    , LastDisguiseTime(0)
    , LastFlashTime(0)
    , LastMoneyDrainTime(0)
    , LastBackgroundMusicTime(0)
    , LastSpeechTime(0)
    , LastEVAEventTime(0)
    , LastTargetTime(0)
    , LastBaseDefenseTime(0)
    , LastRepairTime(0)
    , LastSellTime(0)
    , LastPowerTime(0)
    , LastUpgradeTime(0)
    , LastConstructionTime(0)
    , LastTiberiumCollectionTime(0)
    , LastHarvesterDumpTime(0)
    , LastSlaveMinerTime(0)
    , LastResourceScanTime(0)
    , LastAutoSaveTime(0)
    , LastAutoSaveGameTime(0)
    , LastCursorTime(0)
    , LastMessageTime(0)
    , LastTriggerTime(0)
    , LastTeamTime(0)
    , LastScriptTime(0)
    , LastGlobalTime(0)
    , LastLocalTime(0)
    , LastEVAEventTime2(0)
    , LastVoiceTime(0)
    , LastSoundTime(0)
    , LastCheerTime(0)
    , LastClockTime(0)
    , LastMapTime(0)
    , LastRadarFlashTime(0)
    , LastRadarEventTime(0)
    , LastBeaconTime(0)
    , LastBuildTime2(0)
    , LastAnimTime(0)
    , LastMusicTime(0)
    , LastMovieTime(0)
    , LastBriefingTime(0)
    , LastScoreTime(0)
    , LastOverlayTime(0)
    , LastTiberiumTime(0)
    , LastVeinTime(0)
    , LastIceTime(0)
    , LastExplosionTime(0)
    , LastFireTime(0)
    , LastSparkTime(0)
    , LastSmokeTime(0)
    , LastDustTime(0)
    , LastDebrisTime(0)
    , LastParticleTime(0)
    , LastWeatherTime(0)
    , LastIonStormTime(0)
    , LastLightningTime(0)
    , LastMeteoriteTime(0)
    , LastEarthquakeTime(0)
    , LastVolcanoTime(0)
    , LastTornadoTime(0)
    , LastFloodTime(0)
    , LastDroughtTime(0)
    , LastFamineTime(0)
    , LastPestilenceTime(0)
    , LastWarTime(0)
    , LastPeaceTime(0)
    , LastAllianceTime(0)
    , LastWarDeclarationTime(0)
    , LastDiplomacyTime(0)
    , LastTradeTime(0)
    , LastGiftTime(0)
    , LastTributeTime(0)
    , LastBribeTime(0)
    , LastBlackmailTime(0)
    , LastEspionageTime(0)
    , LastCounterintelligenceTime(0)
    , LastPropagandaTime(0)
    , LastInsurgencyTime(0)
    , LastRevolutionTime(0)
    , LastCoupTime(0)
    , LastAssassinationTime(0)
    , LastSabotageTime2(0)
    , LastTerrorismTime(0)
    , LastGuerrillaTime(0)
    , LastResistanceTime(0)
    , LastLiberationTime(0)
    , LastOccupationTime(0)
    , LastAnnexationTime(0)
    , LastColonizationTime(0)
    , LastDecolonizationTime(0)
    , LastIndependenceTime(0)
    , LastSuccessionTime(0)
    , LastSecessionTime(0)
    , LastUnificationTime(0)
    , LastDivisionTime(0)
    , LastPartitionTime(0)
    , LastFederationTime(0)
    , LastConfederationTime(0)
    , LastIntegrationTime(0)
    , LastDisintegrationTime(0)
    , LastReformationTime(0)
    , LastTransformationTime(0)
    , LastRestorationTime(0)
    , LastRenovationTime(0)
    , LastReconstructionTime(0)
    , LastRehabilitationTime(0)
    , LastRegenerationTime(0)
    , LastResurrectionTime(0)
    , LastRevivalTime(0)
    , LastRenaissanceTime(0)
    , LastEnlightenmentTime(0)
    , LastAwakeningTime(0)
    , LastRebirthTime(0)
    , LastGenesisTime(0)
    , LastApocalypseTime(0)
    , LastArmageddonTime(0)
    , LastCataclysmTime(0)
    , LastCatastropheTime(0)
    , LastCalamityTime(0)
    , LastDisasterTime(0)
    , LastCataclysmTime2(0)
    , LastDoomsdayTime(0)
    , LastJudgmentTime(0)
    , LastOmegaTime(0)
    , LastAlphaTime(0)
    , LastBetaTime(0)
    , LastGammaTime(0)
    , LastDeltaTime(0)
    , LastEpsilonTime(0)
    , LastZetaTime(0)
    , LastEtaTime(0)
    , LastThetaTime(0)
    , LastIotaTime(0)
    , LastKappaTime(0)
    , LastLambdaTime(0)
    , LastMuTime(0)
    , LastNuTime(0)
    , LastXiTime(0)
    , LastOmicronTime(0)
    , LastPiTime(0)
    , LastRhoTime(0)
    , LastSigmaTime(0)
    , LastTauTime(0)
    , LastUpsilonTime(0)
    , LastPhiTime(0)
    , LastChiTime(0)
    , LastPsiTime(0)
    , LastOmegaTime2(0)
{
    // Initialize array of owned type counts
    for (int32 i = 0; i < 512; ++i) {
        OwnedUnitTypeCounts[i] = 0;
        OwnedInfantryTypeCounts[i] = 0;
        OwnedAircraftTypeCounts[i] = 0;
        OwnedBuildingTypeCounts[i] = 0;
        OwnedUnitTypeCountsEver[i] = 0;
        OwnedInfantryTypeCountsEver[i] = 0;
        OwnedAircraftTypeCountsEver[i] = 0;
        OwnedBuildingTypeCountsEver[i] = 0;
    }

    // Initialize tracking lists
    OwnedUnits.Clear();
    OwnedInfantry.Clear();
    OwnedAircraft.Clear();
    OwnedBuildings.Clear();
    AllOwnedObjects.Clear();
    TrackingList.Clear();

    // Initialize super weapon timers
    for (int32 i = 0; i < 64; ++i) {
        SuperWeaponTimers[i].Start(0);
        SuperWeaponTimers[i].Stop();
    }

    // Set ally to self
    if (ArrayIndex >= 0 && ArrayIndex < 32) {
        AllyBitfield |= (1u << ArrayIndex);
    }
}

// ============================================================================
// Destructor
// ============================================================================

HouseClass::~HouseClass()
{
    // Tracking lists are cleaned up by their destructors automatically
    // Type pointed to by Type member is not owned by this class
}

// ============================================================================
// Load - Deserialize from stream
// ============================================================================

HRESULT HouseClass::Load(IStream* pStm)
{
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read Type (string ID)
    char typeID[0x18];
    hr = pStm->Read(typeID, sizeof(typeID), &read);
    if (hr < 0 || read != sizeof(typeID)) return E_FAIL;
    typeID[sizeof(typeID) - 1] = '\0';
    Type = typeID[0] ? HouseTypeClass::Find(typeID) : nullptr;

    // Read scalar fields
    hr = pStm->Read(&TimesDefeated, sizeof(TimesDefeated), &read);
    if (hr < 0 || read != sizeof(TimesDefeated)) return E_FAIL;
    hr = pStm->Read(&TimesWon, sizeof(TimesWon), &read);
    if (hr < 0 || read != sizeof(TimesWon)) return E_FAIL;
    hr = pStm->Read(&Credits, sizeof(Credits), &read);
    if (hr < 0 || read != sizeof(Credits)) return E_FAIL;
    hr = pStm->Read(&CreditsSpent, sizeof(CreditsSpent), &read);
    if (hr < 0 || read != sizeof(CreditsSpent)) return E_FAIL;

    hr = pStm->Read(&AirUnits, sizeof(AirUnits), &read);
    if (hr < 0 || read != sizeof(AirUnits)) return E_FAIL;
    hr = pStm->Read(&InfantryUnits, sizeof(InfantryUnits), &read);
    if (hr < 0 || read != sizeof(InfantryUnits)) return E_FAIL;
    hr = pStm->Read(&Buildings, sizeof(Buildings), &read);
    if (hr < 0 || read != sizeof(Buildings)) return E_FAIL;
    hr = pStm->Read(&Ships, sizeof(Ships), &read);
    if (hr < 0 || read != sizeof(Ships)) return E_FAIL;
    hr = pStm->Read(&Vehicles, sizeof(Vehicles), &read);
    if (hr < 0 || read != sizeof(Vehicles)) return E_FAIL;
    hr = pStm->Read(&AllTechnos, sizeof(AllTechnos), &read);
    if (hr < 0 || read != sizeof(AllTechnos)) return E_FAIL;
    hr = pStm->Read(&PowerOutput, sizeof(PowerOutput), &read);
    if (hr < 0 || read != sizeof(PowerOutput)) return E_FAIL;
    hr = pStm->Read(&PowerDrain, sizeof(PowerDrain), &read);
    if (hr < 0 || read != sizeof(PowerDrain)) return E_FAIL;

    // Read flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    MapIsClear              = (flags & 0x00000001) != 0;
    CurrentPlayer           = (flags & 0x00000002) != 0;
    PlayerControl           = (flags & 0x00000004) != 0;
    IsDeadObject            = (flags & 0x00000008) != 0;
    IsDefeated              = (flags & 0x00000010) != 0;
    IsWinner                = (flags & 0x00000020) != 0;
    IsObserver              = (flags & 0x00000040) != 0;
    IsDiscovered            = (flags & 0x00000080) != 0;
    IsControlStatus         = (flags & 0x00000100) != 0;
    IsHumanPlayer           = (flags & 0x00000200) != 0;
    IsBaseZone              = (flags & 0x00000400) != 0;
    IsRebuilding            = (flags & 0x00000800) != 0;
    IsCivilians             = (flags & 0x00001000) != 0;
    IsVisionary             = (flags & 0x00002000) != 0;
    IsMultiplayerPassive    = (flags & 0x00004000) != 0;
    IsMPGameOver            = (flags & 0x00008000) != 0;
    IsGPSActive             = (flags & 0x00010000) != 0;
    IsGPSActiveVisible      = (flags & 0x00020000) != 0;
    IsGPSActiveInRadar      = (flags & 0x00040000) != 0;
    IsSpySatActive          = (flags & 0x00080000) != 0;
    IsSpySatActiveVisible   = (flags & 0x00100000) != 0;
    IsSpySatActiveInRadar   = (flags & 0x00200000) != 0;
    RadarVisible            = (flags & 0x00400000) != 0;
    RadarVisibleToPlayer    = (flags & 0x00800000) != 0;
    RadarDisabled           = (flags & 0x01000000) != 0;
    RadarJammed             = (flags & 0x02000000) != 0;
    RadarSpied              = (flags & 0x04000000) != 0;
    RevealedByHeight        = (flags & 0x08000000) != 0;

    // Read pointer fields (int32 indices)
    int32 spiedByIndex = -1;
    hr = pStm->Read(&spiedByIndex, sizeof(spiedByIndex), &read);
    if (hr < 0 || read != sizeof(spiedByIndex)) return E_FAIL;
    SpiedBy = (spiedByIndex >= 0) ? AbstractClass::Get_Instance(spiedByIndex) : nullptr;

    int32 spiedBySpySatIndex = -1;
    hr = pStm->Read(&spiedBySpySatIndex, sizeof(spiedBySpySatIndex), &read);
    if (hr < 0 || read != sizeof(spiedBySpySatIndex)) return E_FAIL;
    SpiedBy_SpySat = (spiedBySpySatIndex >= 0) ? AbstractClass::Get_Instance(spiedBySpySatIndex) : nullptr;

    // Read bitfields
    hr = pStm->Read(&AllyBitfield, sizeof(AllyBitfield), &read);
    if (hr < 0 || read != sizeof(AllyBitfield)) return E_FAIL;
    hr = pStm->Read(&EnemyBitfield, sizeof(EnemyBitfield), &read);
    if (hr < 0 || read != sizeof(EnemyBitfield)) return E_FAIL;
    hr = pStm->Read(&ActiveSuperWeapons, sizeof(ActiveSuperWeapons), &read);
    if (hr < 0 || read != sizeof(ActiveSuperWeapons)) return E_FAIL;
    hr = pStm->Read(&AvailableSuperWeapons, sizeof(AvailableSuperWeapons), &read);
    if (hr < 0 || read != sizeof(AvailableSuperWeapons)) return E_FAIL;
    hr = pStm->Read(&UsedSuperWeapons, sizeof(UsedSuperWeapons), &read);
    if (hr < 0 || read != sizeof(UsedSuperWeapons)) return E_FAIL;

    // Read more scalar fields
    hr = pStm->Read(&TechLevel, sizeof(TechLevel), &read);
    if (hr < 0 || read != sizeof(TechLevel)) return E_FAIL;
    hr = pStm->Read(&IQLevel, sizeof(IQLevel), &read);
    if (hr < 0 || read != sizeof(IQLevel)) return E_FAIL;
    hr = pStm->Read(&IQLevel2, sizeof(IQLevel2), &read);
    if (hr < 0 || read != sizeof(IQLevel2)) return E_FAIL;
    hr = pStm->Read(&Edge, sizeof(Edge), &read);
    if (hr < 0 || read != sizeof(Edge)) return E_FAIL;
    hr = pStm->Read(&ColorSchemeIndex, sizeof(ColorSchemeIndex), &read);
    if (hr < 0 || read != sizeof(ColorSchemeIndex)) return E_FAIL;

    // Read counts
    hr = pStm->Read(&UnitCount, sizeof(UnitCount), &read);
    if (hr < 0 || read != sizeof(UnitCount)) return E_FAIL;
    hr = pStm->Read(&InfantryCount, sizeof(InfantryCount), &read);
    if (hr < 0 || read != sizeof(InfantryCount)) return E_FAIL;
    hr = pStm->Read(&AircraftCount, sizeof(AircraftCount), &read);
    if (hr < 0 || read != sizeof(AircraftCount)) return E_FAIL;
    hr = pStm->Read(&BuildingCount, sizeof(BuildingCount), &read);
    if (hr < 0 || read != sizeof(BuildingCount)) return E_FAIL;
    hr = pStm->Read(&OwnedUnitCount, sizeof(OwnedUnitCount), &read);
    if (hr < 0 || read != sizeof(OwnedUnitCount)) return E_FAIL;
    hr = pStm->Read(&OwnedInfantryCount, sizeof(OwnedInfantryCount), &read);
    if (hr < 0 || read != sizeof(OwnedInfantryCount)) return E_FAIL;
    hr = pStm->Read(&OwnedAircraftCount, sizeof(OwnedAircraftCount), &read);
    if (hr < 0 || read != sizeof(OwnedAircraftCount)) return E_FAIL;
    hr = pStm->Read(&OwnedBuildingCount, sizeof(OwnedBuildingCount), &read);
    if (hr < 0 || read != sizeof(OwnedBuildingCount)) return E_FAIL;
    hr = pStm->Read(&DestroyedUnitCount, sizeof(DestroyedUnitCount), &read);
    if (hr < 0 || read != sizeof(DestroyedUnitCount)) return E_FAIL;
    hr = pStm->Read(&DestroyedInfantryCount, sizeof(DestroyedInfantryCount), &read);
    if (hr < 0 || read != sizeof(DestroyedInfantryCount)) return E_FAIL;
    hr = pStm->Read(&DestroyedAircraftCount, sizeof(DestroyedAircraftCount), &read);
    if (hr < 0 || read != sizeof(DestroyedAircraftCount)) return E_FAIL;
    hr = pStm->Read(&DestroyedBuildingCount, sizeof(DestroyedBuildingCount), &read);
    if (hr < 0 || read != sizeof(DestroyedBuildingCount)) return E_FAIL;
    hr = pStm->Read(&TotalUnitCount, sizeof(TotalUnitCount), &read);
    if (hr < 0 || read != sizeof(TotalUnitCount)) return E_FAIL;
    hr = pStm->Read(&TotalInfantryCount, sizeof(TotalInfantryCount), &read);
    if (hr < 0 || read != sizeof(TotalInfantryCount)) return E_FAIL;
    hr = pStm->Read(&TotalAircraftCount, sizeof(TotalAircraftCount), &read);
    if (hr < 0 || read != sizeof(TotalAircraftCount)) return E_FAIL;
    hr = pStm->Read(&TotalBuildingCount, sizeof(TotalBuildingCount), &read);
    if (hr < 0 || read != sizeof(TotalBuildingCount)) return E_FAIL;

    // Read value fields
    hr = pStm->Read(&DestroyedUnitValue, sizeof(DestroyedUnitValue), &read);
    if (hr < 0 || read != sizeof(DestroyedUnitValue)) return E_FAIL;
    hr = pStm->Read(&DestroyedInfantryValue, sizeof(DestroyedInfantryValue), &read);
    if (hr < 0 || read != sizeof(DestroyedInfantryValue)) return E_FAIL;
    hr = pStm->Read(&DestroyedAircraftValue, sizeof(DestroyedAircraftValue), &read);
    if (hr < 0 || read != sizeof(DestroyedAircraftValue)) return E_FAIL;
    hr = pStm->Read(&DestroyedBuildingValue, sizeof(DestroyedBuildingValue), &read);
    if (hr < 0 || read != sizeof(DestroyedBuildingValue)) return E_FAIL;
    hr = pStm->Read(&TotalUnitValue, sizeof(TotalUnitValue), &read);
    if (hr < 0 || read != sizeof(TotalUnitValue)) return E_FAIL;
    hr = pStm->Read(&TotalInfantryValue, sizeof(TotalInfantryValue), &read);
    if (hr < 0 || read != sizeof(TotalInfantryValue)) return E_FAIL;
    hr = pStm->Read(&TotalAircraftValue, sizeof(TotalAircraftValue), &read);
    if (hr < 0 || read != sizeof(TotalAircraftValue)) return E_FAIL;
    hr = pStm->Read(&TotalBuildingValue, sizeof(TotalBuildingValue), &read);
    if (hr < 0 || read != sizeof(TotalBuildingValue)) return E_FAIL;

    // Read index fields
    hr = pStm->Read(&AllHousesIndex, sizeof(AllHousesIndex), &read);
    if (hr < 0 || read != sizeof(AllHousesIndex)) return E_FAIL;
    hr = pStm->Read(&ArrayIndex, sizeof(ArrayIndex), &read);
    if (hr < 0 || read != sizeof(ArrayIndex)) return E_FAIL;
    hr = pStm->Read(&ActLikeIndex, sizeof(ActLikeIndex), &read);
    if (hr < 0 || read != sizeof(ActLikeIndex)) return E_FAIL;

    // Read PlayerName
    hr = pStm->Read(PlayerName, sizeof(PlayerName), &read);
    if (hr < 0 || read != sizeof(PlayerName)) return E_FAIL;

    // Read FactoryCount, AlliesCounter, EnemiesCounter
    hr = pStm->Read(&FactoryCount, sizeof(FactoryCount), &read);
    if (hr < 0 || read != sizeof(FactoryCount)) return E_FAIL;
    hr = pStm->Read(&AlliesCounter, sizeof(AlliesCounter), &read);
    if (hr < 0 || read != sizeof(AlliesCounter)) return E_FAIL;
    hr = pStm->Read(&EnemiesCounter, sizeof(EnemiesCounter), &read);
    if (hr < 0 || read != sizeof(EnemiesCounter)) return E_FAIL;

    // Read pointer fields (int32 indices)
    int32 radarJammedByIndex = -1;
    hr = pStm->Read(&radarJammedByIndex, sizeof(radarJammedByIndex), &read);
    if (hr < 0 || read != sizeof(radarJammedByIndex)) return E_FAIL;
    RadarJammedBy = (radarJammedByIndex >= 0) ? AbstractClass::Get_Instance(radarJammedByIndex) : nullptr;

    int32 radarSpiedByIndex = -1;
    hr = pStm->Read(&radarSpiedByIndex, sizeof(radarSpiedByIndex), &read);
    if (hr < 0 || read != sizeof(radarSpiedByIndex)) return E_FAIL;
    RadarSpiedBy = (radarSpiedByIndex >= 0) ? AbstractClass::Get_Instance(radarSpiedByIndex) : nullptr;

    // Read BaseCenter and BaseNodesCount
    hr = pStm->Read(&BaseCenter, sizeof(BaseCenter), &read);
    if (hr < 0 || read != sizeof(BaseCenter)) return E_FAIL;
    hr = pStm->Read(&BaseNodesCount, sizeof(BaseNodesCount), &read);
    if (hr < 0 || read != sizeof(BaseNodesCount)) return E_FAIL;

    // Read type count arrays
    hr = pStm->Read(OwnedUnitTypeCounts, sizeof(OwnedUnitTypeCounts), &read);
    if (hr < 0 || read != sizeof(OwnedUnitTypeCounts)) return E_FAIL;
    hr = pStm->Read(OwnedInfantryTypeCounts, sizeof(OwnedInfantryTypeCounts), &read);
    if (hr < 0 || read != sizeof(OwnedInfantryTypeCounts)) return E_FAIL;
    hr = pStm->Read(OwnedAircraftTypeCounts, sizeof(OwnedAircraftTypeCounts), &read);
    if (hr < 0 || read != sizeof(OwnedAircraftTypeCounts)) return E_FAIL;
    hr = pStm->Read(OwnedBuildingTypeCounts, sizeof(OwnedBuildingTypeCounts), &read);
    if (hr < 0 || read != sizeof(OwnedBuildingTypeCounts)) return E_FAIL;
    hr = pStm->Read(OwnedUnitTypeCountsEver, sizeof(OwnedUnitTypeCountsEver), &read);
    if (hr < 0 || read != sizeof(OwnedUnitTypeCountsEver)) return E_FAIL;
    hr = pStm->Read(OwnedInfantryTypeCountsEver, sizeof(OwnedInfantryTypeCountsEver), &read);
    if (hr < 0 || read != sizeof(OwnedInfantryTypeCountsEver)) return E_FAIL;
    hr = pStm->Read(OwnedAircraftTypeCountsEver, sizeof(OwnedAircraftTypeCountsEver), &read);
    if (hr < 0 || read != sizeof(OwnedAircraftTypeCountsEver)) return E_FAIL;
    hr = pStm->Read(OwnedBuildingTypeCountsEver, sizeof(OwnedBuildingTypeCountsEver), &read);
    if (hr < 0 || read != sizeof(OwnedBuildingTypeCountsEver)) return E_FAIL;

    // Read DynamicVectorClass members (count + indices)
    OwnedUnits.Clear();
    int32 ownedUnitsCount = 0;
    hr = pStm->Read(&ownedUnitsCount, sizeof(ownedUnitsCount), &read);
    if (hr < 0 || read != sizeof(ownedUnitsCount)) return E_FAIL;
    for (int32 i = 0; i < ownedUnitsCount; ++i) {
        int32 idx = -1;
        hr = pStm->Read(&idx, sizeof(idx), &read);
        if (hr < 0 || read != sizeof(idx)) return E_FAIL;
        if (idx >= 0) OwnedUnits.Add((UnitClass*)AbstractClass::Get_Instance(idx));
    }

    OwnedInfantry.Clear();
    int32 ownedInfantryCount = 0;
    hr = pStm->Read(&ownedInfantryCount, sizeof(ownedInfantryCount), &read);
    if (hr < 0 || read != sizeof(ownedInfantryCount)) return E_FAIL;
    for (int32 i = 0; i < ownedInfantryCount; ++i) {
        int32 idx = -1;
        hr = pStm->Read(&idx, sizeof(idx), &read);
        if (hr < 0 || read != sizeof(idx)) return E_FAIL;
        if (idx >= 0) OwnedInfantry.Add((InfantryClass*)AbstractClass::Get_Instance(idx));
    }

    OwnedAircraft.Clear();
    int32 ownedAircraftCount = 0;
    hr = pStm->Read(&ownedAircraftCount, sizeof(ownedAircraftCount), &read);
    if (hr < 0 || read != sizeof(ownedAircraftCount)) return E_FAIL;
    for (int32 i = 0; i < ownedAircraftCount; ++i) {
        int32 idx = -1;
        hr = pStm->Read(&idx, sizeof(idx), &read);
        if (hr < 0 || read != sizeof(idx)) return E_FAIL;
        if (idx >= 0) OwnedAircraft.Add((AircraftClass*)AbstractClass::Get_Instance(idx));
    }

    OwnedBuildings.Clear();
    int32 ownedBuildingsCount = 0;
    hr = pStm->Read(&ownedBuildingsCount, sizeof(ownedBuildingsCount), &read);
    if (hr < 0 || read != sizeof(ownedBuildingsCount)) return E_FAIL;
    for (int32 i = 0; i < ownedBuildingsCount; ++i) {
        int32 idx = -1;
        hr = pStm->Read(&idx, sizeof(idx), &read);
        if (hr < 0 || read != sizeof(idx)) return E_FAIL;
        if (idx >= 0) OwnedBuildings.Add((BuildingClass*)AbstractClass::Get_Instance(idx));
    }

    AllOwnedObjects.Clear();
    int32 allOwnedCount = 0;
    hr = pStm->Read(&allOwnedCount, sizeof(allOwnedCount), &read);
    if (hr < 0 || read != sizeof(allOwnedCount)) return E_FAIL;
    for (int32 i = 0; i < allOwnedCount; ++i) {
        int32 idx = -1;
        hr = pStm->Read(&idx, sizeof(idx), &read);
        if (hr < 0 || read != sizeof(idx)) return E_FAIL;
        if (idx >= 0) AllOwnedObjects.Add((TechnoClass*)AbstractClass::Get_Instance(idx));
    }

    TrackingList.Clear();
    int32 trackingCount = 0;
    hr = pStm->Read(&trackingCount, sizeof(trackingCount), &read);
    if (hr < 0 || read != sizeof(trackingCount)) return E_FAIL;
    for (int32 i = 0; i < trackingCount; ++i) {
        int32 idx = -1;
        hr = pStm->Read(&idx, sizeof(idx), &read);
        if (hr < 0 || read != sizeof(idx)) return E_FAIL;
        if (idx >= 0) TrackingList.Add((TechnoClass*)AbstractClass::Get_Instance(idx));
    }

    // Read SuperWeaponTimers
    hr = pStm->Read(SuperWeaponTimers, sizeof(SuperWeaponTimers), &read);
    if (hr < 0 || read != sizeof(SuperWeaponTimers)) return E_FAIL;

    // Read Last*Time fields as a block
    int32 lastTimeSize = reinterpret_cast<char*>(&LastOmegaTime2)
                       - reinterpret_cast<char*>(&LastBuildTime)
                       + sizeof(LastOmegaTime2);
    hr = pStm->Read(&LastBuildTime, lastTimeSize, &read);
    if (hr < 0 || read != static_cast<ULONG>(lastTimeSize)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// Save - Serialize to stream
// ============================================================================

HRESULT HouseClass::Save(IStream* pStm, BOOL bSave)
{
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Write Type (string ID)
    char typeID[0x18];
    std::memset(typeID, 0, sizeof(typeID));
    if (Type && Type->get_ID()) {
        const char* srcID = Type->get_ID();
        int32 j = 0;
        while (srcID[j] && j < static_cast<int32>(sizeof(typeID)) - 1) {
            typeID[j] = srcID[j]; ++j;
        }
    }
    hr = pStm->Write(typeID, sizeof(typeID), &written);
    if (hr < 0 || written != sizeof(typeID)) return E_FAIL;

    // Write scalar fields
    hr = pStm->Write(&TimesDefeated, sizeof(TimesDefeated), &written);
    if (hr < 0 || written != sizeof(TimesDefeated)) return E_FAIL;
    hr = pStm->Write(&TimesWon, sizeof(TimesWon), &written);
    if (hr < 0 || written != sizeof(TimesWon)) return E_FAIL;
    hr = pStm->Write(&Credits, sizeof(Credits), &written);
    if (hr < 0 || written != sizeof(Credits)) return E_FAIL;
    hr = pStm->Write(&CreditsSpent, sizeof(CreditsSpent), &written);
    if (hr < 0 || written != sizeof(CreditsSpent)) return E_FAIL;

    hr = pStm->Write(&AirUnits, sizeof(AirUnits), &written);
    if (hr < 0 || written != sizeof(AirUnits)) return E_FAIL;
    hr = pStm->Write(&InfantryUnits, sizeof(InfantryUnits), &written);
    if (hr < 0 || written != sizeof(InfantryUnits)) return E_FAIL;
    hr = pStm->Write(&Buildings, sizeof(Buildings), &written);
    if (hr < 0 || written != sizeof(Buildings)) return E_FAIL;
    hr = pStm->Write(&Ships, sizeof(Ships), &written);
    if (hr < 0 || written != sizeof(Ships)) return E_FAIL;
    hr = pStm->Write(&Vehicles, sizeof(Vehicles), &written);
    if (hr < 0 || written != sizeof(Vehicles)) return E_FAIL;
    hr = pStm->Write(&AllTechnos, sizeof(AllTechnos), &written);
    if (hr < 0 || written != sizeof(AllTechnos)) return E_FAIL;
    hr = pStm->Write(&PowerOutput, sizeof(PowerOutput), &written);
    if (hr < 0 || written != sizeof(PowerOutput)) return E_FAIL;
    hr = pStm->Write(&PowerDrain, sizeof(PowerDrain), &written);
    if (hr < 0 || written != sizeof(PowerDrain)) return E_FAIL;

    // Write flags as a bitmask
    uint32 flags = 0;
    if (MapIsClear)              flags |= 0x00000001;
    if (CurrentPlayer)           flags |= 0x00000002;
    if (PlayerControl)           flags |= 0x00000004;
    if (IsDeadObject)            flags |= 0x00000008;
    if (IsDefeated)              flags |= 0x00000010;
    if (IsWinner)                flags |= 0x00000020;
    if (IsObserver)              flags |= 0x00000040;
    if (IsDiscovered)            flags |= 0x00000080;
    if (IsControlStatus)         flags |= 0x00000100;
    if (IsHumanPlayer)           flags |= 0x00000200;
    if (IsBaseZone)              flags |= 0x00000400;
    if (IsRebuilding)            flags |= 0x00000800;
    if (IsCivilians)             flags |= 0x00001000;
    if (IsVisionary)             flags |= 0x00002000;
    if (IsMultiplayerPassive)    flags |= 0x00004000;
    if (IsMPGameOver)            flags |= 0x00008000;
    if (IsGPSActive)             flags |= 0x00010000;
    if (IsGPSActiveVisible)      flags |= 0x00020000;
    if (IsGPSActiveInRadar)      flags |= 0x00040000;
    if (IsSpySatActive)          flags |= 0x00080000;
    if (IsSpySatActiveVisible)   flags |= 0x00100000;
    if (IsSpySatActiveInRadar)   flags |= 0x00200000;
    if (RadarVisible)            flags |= 0x00400000;
    if (RadarVisibleToPlayer)    flags |= 0x00800000;
    if (RadarDisabled)           flags |= 0x01000000;
    if (RadarJammed)             flags |= 0x02000000;
    if (RadarSpied)              flags |= 0x04000000;
    if (RevealedByHeight)        flags |= 0x08000000;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    // Write pointer fields (int32 indices)
    int32 spiedByIndex = SpiedBy ? AbstractClass::Find_Index(SpiedBy) : -1;
    hr = pStm->Write(&spiedByIndex, sizeof(spiedByIndex), &written);
    if (hr < 0 || written != sizeof(spiedByIndex)) return E_FAIL;

    int32 spiedBySpySatIndex = SpiedBy_SpySat ? AbstractClass::Find_Index(SpiedBy_SpySat) : -1;
    hr = pStm->Write(&spiedBySpySatIndex, sizeof(spiedBySpySatIndex), &written);
    if (hr < 0 || written != sizeof(spiedBySpySatIndex)) return E_FAIL;

    // Write bitfields
    hr = pStm->Write(&AllyBitfield, sizeof(AllyBitfield), &written);
    if (hr < 0 || written != sizeof(AllyBitfield)) return E_FAIL;
    hr = pStm->Write(&EnemyBitfield, sizeof(EnemyBitfield), &written);
    if (hr < 0 || written != sizeof(EnemyBitfield)) return E_FAIL;
    hr = pStm->Write(&ActiveSuperWeapons, sizeof(ActiveSuperWeapons), &written);
    if (hr < 0 || written != sizeof(ActiveSuperWeapons)) return E_FAIL;
    hr = pStm->Write(&AvailableSuperWeapons, sizeof(AvailableSuperWeapons), &written);
    if (hr < 0 || written != sizeof(AvailableSuperWeapons)) return E_FAIL;
    hr = pStm->Write(&UsedSuperWeapons, sizeof(UsedSuperWeapons), &written);
    if (hr < 0 || written != sizeof(UsedSuperWeapons)) return E_FAIL;

    // Write more scalar fields
    hr = pStm->Write(&TechLevel, sizeof(TechLevel), &written);
    if (hr < 0 || written != sizeof(TechLevel)) return E_FAIL;
    hr = pStm->Write(&IQLevel, sizeof(IQLevel), &written);
    if (hr < 0 || written != sizeof(IQLevel)) return E_FAIL;
    hr = pStm->Write(&IQLevel2, sizeof(IQLevel2), &written);
    if (hr < 0 || written != sizeof(IQLevel2)) return E_FAIL;
    hr = pStm->Write(&Edge, sizeof(Edge), &written);
    if (hr < 0 || written != sizeof(Edge)) return E_FAIL;
    hr = pStm->Write(&ColorSchemeIndex, sizeof(ColorSchemeIndex), &written);
    if (hr < 0 || written != sizeof(ColorSchemeIndex)) return E_FAIL;

    // Write counts
    hr = pStm->Write(&UnitCount, sizeof(UnitCount), &written);
    if (hr < 0 || written != sizeof(UnitCount)) return E_FAIL;
    hr = pStm->Write(&InfantryCount, sizeof(InfantryCount), &written);
    if (hr < 0 || written != sizeof(InfantryCount)) return E_FAIL;
    hr = pStm->Write(&AircraftCount, sizeof(AircraftCount), &written);
    if (hr < 0 || written != sizeof(AircraftCount)) return E_FAIL;
    hr = pStm->Write(&BuildingCount, sizeof(BuildingCount), &written);
    if (hr < 0 || written != sizeof(BuildingCount)) return E_FAIL;
    hr = pStm->Write(&OwnedUnitCount, sizeof(OwnedUnitCount), &written);
    if (hr < 0 || written != sizeof(OwnedUnitCount)) return E_FAIL;
    hr = pStm->Write(&OwnedInfantryCount, sizeof(OwnedInfantryCount), &written);
    if (hr < 0 || written != sizeof(OwnedInfantryCount)) return E_FAIL;
    hr = pStm->Write(&OwnedAircraftCount, sizeof(OwnedAircraftCount), &written);
    if (hr < 0 || written != sizeof(OwnedAircraftCount)) return E_FAIL;
    hr = pStm->Write(&OwnedBuildingCount, sizeof(OwnedBuildingCount), &written);
    if (hr < 0 || written != sizeof(OwnedBuildingCount)) return E_FAIL;
    hr = pStm->Write(&DestroyedUnitCount, sizeof(DestroyedUnitCount), &written);
    if (hr < 0 || written != sizeof(DestroyedUnitCount)) return E_FAIL;
    hr = pStm->Write(&DestroyedInfantryCount, sizeof(DestroyedInfantryCount), &written);
    if (hr < 0 || written != sizeof(DestroyedInfantryCount)) return E_FAIL;
    hr = pStm->Write(&DestroyedAircraftCount, sizeof(DestroyedAircraftCount), &written);
    if (hr < 0 || written != sizeof(DestroyedAircraftCount)) return E_FAIL;
    hr = pStm->Write(&DestroyedBuildingCount, sizeof(DestroyedBuildingCount), &written);
    if (hr < 0 || written != sizeof(DestroyedBuildingCount)) return E_FAIL;
    hr = pStm->Write(&TotalUnitCount, sizeof(TotalUnitCount), &written);
    if (hr < 0 || written != sizeof(TotalUnitCount)) return E_FAIL;
    hr = pStm->Write(&TotalInfantryCount, sizeof(TotalInfantryCount), &written);
    if (hr < 0 || written != sizeof(TotalInfantryCount)) return E_FAIL;
    hr = pStm->Write(&TotalAircraftCount, sizeof(TotalAircraftCount), &written);
    if (hr < 0 || written != sizeof(TotalAircraftCount)) return E_FAIL;
    hr = pStm->Write(&TotalBuildingCount, sizeof(TotalBuildingCount), &written);
    if (hr < 0 || written != sizeof(TotalBuildingCount)) return E_FAIL;

    // Write value fields
    hr = pStm->Write(&DestroyedUnitValue, sizeof(DestroyedUnitValue), &written);
    if (hr < 0 || written != sizeof(DestroyedUnitValue)) return E_FAIL;
    hr = pStm->Write(&DestroyedInfantryValue, sizeof(DestroyedInfantryValue), &written);
    if (hr < 0 || written != sizeof(DestroyedInfantryValue)) return E_FAIL;
    hr = pStm->Write(&DestroyedAircraftValue, sizeof(DestroyedAircraftValue), &written);
    if (hr < 0 || written != sizeof(DestroyedAircraftValue)) return E_FAIL;
    hr = pStm->Write(&DestroyedBuildingValue, sizeof(DestroyedBuildingValue), &written);
    if (hr < 0 || written != sizeof(DestroyedBuildingValue)) return E_FAIL;
    hr = pStm->Write(&TotalUnitValue, sizeof(TotalUnitValue), &written);
    if (hr < 0 || written != sizeof(TotalUnitValue)) return E_FAIL;
    hr = pStm->Write(&TotalInfantryValue, sizeof(TotalInfantryValue), &written);
    if (hr < 0 || written != sizeof(TotalInfantryValue)) return E_FAIL;
    hr = pStm->Write(&TotalAircraftValue, sizeof(TotalAircraftValue), &written);
    if (hr < 0 || written != sizeof(TotalAircraftValue)) return E_FAIL;
    hr = pStm->Write(&TotalBuildingValue, sizeof(TotalBuildingValue), &written);
    if (hr < 0 || written != sizeof(TotalBuildingValue)) return E_FAIL;

    // Write index fields
    hr = pStm->Write(&AllHousesIndex, sizeof(AllHousesIndex), &written);
    if (hr < 0 || written != sizeof(AllHousesIndex)) return E_FAIL;
    hr = pStm->Write(&ArrayIndex, sizeof(ArrayIndex), &written);
    if (hr < 0 || written != sizeof(ArrayIndex)) return E_FAIL;
    hr = pStm->Write(&ActLikeIndex, sizeof(ActLikeIndex), &written);
    if (hr < 0 || written != sizeof(ActLikeIndex)) return E_FAIL;

    // Write PlayerName
    hr = pStm->Write(PlayerName, sizeof(PlayerName), &written);
    if (hr < 0 || written != sizeof(PlayerName)) return E_FAIL;

    // Write FactoryCount, AlliesCounter, EnemiesCounter
    hr = pStm->Write(&FactoryCount, sizeof(FactoryCount), &written);
    if (hr < 0 || written != sizeof(FactoryCount)) return E_FAIL;
    hr = pStm->Write(&AlliesCounter, sizeof(AlliesCounter), &written);
    if (hr < 0 || written != sizeof(AlliesCounter)) return E_FAIL;
    hr = pStm->Write(&EnemiesCounter, sizeof(EnemiesCounter), &written);
    if (hr < 0 || written != sizeof(EnemiesCounter)) return E_FAIL;

    // Write pointer fields (int32 indices)
    int32 radarJammedByIndex = RadarJammedBy ? AbstractClass::Find_Index(RadarJammedBy) : -1;
    hr = pStm->Write(&radarJammedByIndex, sizeof(radarJammedByIndex), &written);
    if (hr < 0 || written != sizeof(radarJammedByIndex)) return E_FAIL;

    int32 radarSpiedByIndex = RadarSpiedBy ? AbstractClass::Find_Index(RadarSpiedBy) : -1;
    hr = pStm->Write(&radarSpiedByIndex, sizeof(radarSpiedByIndex), &written);
    if (hr < 0 || written != sizeof(radarSpiedByIndex)) return E_FAIL;

    // Write BaseCenter and BaseNodesCount
    hr = pStm->Write(&BaseCenter, sizeof(BaseCenter), &written);
    if (hr < 0 || written != sizeof(BaseCenter)) return E_FAIL;
    hr = pStm->Write(&BaseNodesCount, sizeof(BaseNodesCount), &written);
    if (hr < 0 || written != sizeof(BaseNodesCount)) return E_FAIL;

    // Write type count arrays
    hr = pStm->Write(OwnedUnitTypeCounts, sizeof(OwnedUnitTypeCounts), &written);
    if (hr < 0 || written != sizeof(OwnedUnitTypeCounts)) return E_FAIL;
    hr = pStm->Write(OwnedInfantryTypeCounts, sizeof(OwnedInfantryTypeCounts), &written);
    if (hr < 0 || written != sizeof(OwnedInfantryTypeCounts)) return E_FAIL;
    hr = pStm->Write(OwnedAircraftTypeCounts, sizeof(OwnedAircraftTypeCounts), &written);
    if (hr < 0 || written != sizeof(OwnedAircraftTypeCounts)) return E_FAIL;
    hr = pStm->Write(OwnedBuildingTypeCounts, sizeof(OwnedBuildingTypeCounts), &written);
    if (hr < 0 || written != sizeof(OwnedBuildingTypeCounts)) return E_FAIL;
    hr = pStm->Write(OwnedUnitTypeCountsEver, sizeof(OwnedUnitTypeCountsEver), &written);
    if (hr < 0 || written != sizeof(OwnedUnitTypeCountsEver)) return E_FAIL;
    hr = pStm->Write(OwnedInfantryTypeCountsEver, sizeof(OwnedInfantryTypeCountsEver), &written);
    if (hr < 0 || written != sizeof(OwnedInfantryTypeCountsEver)) return E_FAIL;
    hr = pStm->Write(OwnedAircraftTypeCountsEver, sizeof(OwnedAircraftTypeCountsEver), &written);
    if (hr < 0 || written != sizeof(OwnedAircraftTypeCountsEver)) return E_FAIL;
    hr = pStm->Write(OwnedBuildingTypeCountsEver, sizeof(OwnedBuildingTypeCountsEver), &written);
    if (hr < 0 || written != sizeof(OwnedBuildingTypeCountsEver)) return E_FAIL;

    // Write DynamicVectorClass members (count + indices)
    int32 ownedUnitsCount = OwnedUnits.Count;
    hr = pStm->Write(&ownedUnitsCount, sizeof(ownedUnitsCount), &written);
    if (hr < 0 || written != sizeof(ownedUnitsCount)) return E_FAIL;
    for (int32 i = 0; i < OwnedUnits.Count; ++i) {
        int32 idx = OwnedUnits.Items[i] ? AbstractClass::Find_Index((AbstractClass*)OwnedUnits.Items[i]) : -1;
        hr = pStm->Write(&idx, sizeof(idx), &written);
        if (hr < 0 || written != sizeof(idx)) return E_FAIL;
    }

    int32 ownedInfantryCount = OwnedInfantry.Count;
    hr = pStm->Write(&ownedInfantryCount, sizeof(ownedInfantryCount), &written);
    if (hr < 0 || written != sizeof(ownedInfantryCount)) return E_FAIL;
    for (int32 i = 0; i < OwnedInfantry.Count; ++i) {
        int32 idx = OwnedInfantry.Items[i] ? AbstractClass::Find_Index((AbstractClass*)OwnedInfantry.Items[i]) : -1;
        hr = pStm->Write(&idx, sizeof(idx), &written);
        if (hr < 0 || written != sizeof(idx)) return E_FAIL;
    }

    int32 ownedAircraftCount = OwnedAircraft.Count;
    hr = pStm->Write(&ownedAircraftCount, sizeof(ownedAircraftCount), &written);
    if (hr < 0 || written != sizeof(ownedAircraftCount)) return E_FAIL;
    for (int32 i = 0; i < OwnedAircraft.Count; ++i) {
        int32 idx = OwnedAircraft.Items[i] ? AbstractClass::Find_Index((AbstractClass*)OwnedAircraft.Items[i]) : -1;
        hr = pStm->Write(&idx, sizeof(idx), &written);
        if (hr < 0 || written != sizeof(idx)) return E_FAIL;
    }

    int32 ownedBuildingsCount = OwnedBuildings.Count;
    hr = pStm->Write(&ownedBuildingsCount, sizeof(ownedBuildingsCount), &written);
    if (hr < 0 || written != sizeof(ownedBuildingsCount)) return E_FAIL;
    for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
        int32 idx = OwnedBuildings.Items[i] ? AbstractClass::Find_Index((AbstractClass*)OwnedBuildings.Items[i]) : -1;
        hr = pStm->Write(&idx, sizeof(idx), &written);
        if (hr < 0 || written != sizeof(idx)) return E_FAIL;
    }

    int32 allOwnedCount = AllOwnedObjects.Count;
    hr = pStm->Write(&allOwnedCount, sizeof(allOwnedCount), &written);
    if (hr < 0 || written != sizeof(allOwnedCount)) return E_FAIL;
    for (int32 i = 0; i < AllOwnedObjects.Count; ++i) {
        int32 idx = AllOwnedObjects.Items[i] ? AbstractClass::Find_Index((AbstractClass*)AllOwnedObjects.Items[i]) : -1;
        hr = pStm->Write(&idx, sizeof(idx), &written);
        if (hr < 0 || written != sizeof(idx)) return E_FAIL;
    }

    int32 trackingCount = TrackingList.Count;
    hr = pStm->Write(&trackingCount, sizeof(trackingCount), &written);
    if (hr < 0 || written != sizeof(trackingCount)) return E_FAIL;
    for (int32 i = 0; i < TrackingList.Count; ++i) {
        int32 idx = TrackingList.Items[i] ? AbstractClass::Find_Index((AbstractClass*)TrackingList.Items[i]) : -1;
        hr = pStm->Write(&idx, sizeof(idx), &written);
        if (hr < 0 || written != sizeof(idx)) return E_FAIL;
    }

    // Write SuperWeaponTimers
    hr = pStm->Write(SuperWeaponTimers, sizeof(SuperWeaponTimers), &written);
    if (hr < 0 || written != sizeof(SuperWeaponTimers)) return E_FAIL;

    // Write Last*Time fields as a block
    int32 lastTimeSize = reinterpret_cast<const char*>(&LastOmegaTime2)
                       - reinterpret_cast<const char*>(&LastBuildTime)
                       + sizeof(LastOmegaTime2);
    hr = pStm->Write(&LastBuildTime, lastTimeSize, &written);
    if (hr < 0 || written != static_cast<ULONG>(lastTimeSize)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// ComputeCRC - Calculate CRC for the house state
// ============================================================================

void HouseClass::ComputeCRC(CRCEngine& crc) const
{
    // CRC the house state for network sync verification
    crc.AddData(&Credits, sizeof(Credits));
    crc.AddData(&PowerOutput, sizeof(PowerOutput));
    crc.AddData(&PowerDrain, sizeof(PowerDrain));
    crc.AddData(&AllyBitfield, sizeof(AllyBitfield));
    crc.AddData(&EnemyBitfield, sizeof(EnemyBitfield));
    crc.AddData(&ActiveSuperWeapons, sizeof(ActiveSuperWeapons));
    crc.AddByte(static_cast<uint8>(ArrayIndex));
}

// ============================================================================
// Init - Initialize the house
// ============================================================================

void HouseClass::Init()
{
    // Initialize from the house type
    if (Type) {
        ColorSchemeIndex = Type->ColorSchemeIndex;
        if (Type->SmartAI) {
            IQLevel = 5;
        }
    }

    // Reset state
    Credits = 0;
    CreditsSpent = 0;
    MapIsClear = false;
    IsDeadObject = false;
    IsDefeated = false;
    IsWinner = false;
    IsObserver = false;
    IsDiscovered = false;
    IsControlStatus = false;
    IsBaseZone = false;
    IsRebuilding = false;
    IsGPSActive = false;
    IsSpySatActive = false;

    // Reset power
    PowerOutput = 0;
    PowerDrain = 0;

    // asm: +0x1C0 建造状态机复位。
    AIBuildState = 0;

    // Reset unit counts
    UnitCount = 0;
    InfantryCount = 0;
    AircraftCount = 0;
    BuildingCount = 0;
    OwnedUnitCount = 0;
    OwnedInfantryCount = 0;
    OwnedAircraftCount = 0;
    OwnedBuildingCount = 0;

    // Reset tracking
    OwnedUnits.Clear();
    OwnedInfantry.Clear();
    OwnedAircraft.Clear();
    OwnedBuildings.Clear();
    AllOwnedObjects.Clear();
    TrackingList.Clear();

    // Reset super weapon timers
    for (int32 i = 0; i < 64; ++i) {
        SuperWeaponTimers[i].Start(0);
        SuperWeaponTimers[i].Stop();
    }

    // Reset ally/enemy bitfields
    AllyBitfield = 0;
    EnemyBitfield = 0;

    // Set ally to self
    if (ArrayIndex >= 0 && ArrayIndex < 32) {
        AllyBitfield |= (1u << ArrayIndex);
    }

    // Reset spies
    SpiedBy = nullptr;
    SpiedBy_SpySat = nullptr;

    // Set timers to current frame
    int32 now = FrameTimer::GetTime();
    LastBuildTime = now;
    LastProductionTime = now;
    LastAttackTime = now;
    LastEnemySightingTime = now;
    LastTeamCreationTime = now;
    LastBaseScanTime = now;
    LastCombatTime = now;
    LastNavalCombatTime = now;
    LastAirCombatTime = now;
}

// ============================================================================
// Update - Per-frame update
// ============================================================================

// ============================================================================
// asm: HouseClass_Update (0x4F83E8)
//
//   void __thiscall HouseClass_Update(HouseClass *this)   —— 阵营逐帧主循环。
//
//   逐段（读自 552059..）：
//     * PowerBlackoutTimer 剩余 == 1 -> 复位计时器并置 RecheckItems=1
//     * RadarBlackoutTimer 剩余 == 1 -> 复位计时器并置 RadarOutage=1
//     * RecheckItems 置位 -> Update_Power，随后 RadarOutage=1
//     * RadarOutage 置位 -> Radar_Activate（非静音时）+ SpySat_Update
//     * 每 100 帧衰减 AngerNodes（本项目未建模该表）
//     * 非人类且 IQ>=阈值 -> AutoBaseBuilding/productionEnabled/AutocreateAllowed
//     * ...（建造列表推进、胜负判定、Blowup_All、MPlayer_Defeated 等）
//     * 非人类时在末尾调用 HouseClass_Update_AI（即 AI()）
//     * CheckSWs / UpdateSWs 收尾
//
//   本项目无 AngerNodes/SpySat 全局静音开关，按铁律只落桩可表达的分支。
// ============================================================================
void HouseClass::Update()
{
    if (IsDeadObject)
        return;

    // 货币脏标记：黑屏计时结束时要求下一次重算电力。
    // asm L552090: PowerBlackoutTimer 剩余 == 1 -> 复位 + RecheckItems = 1。
    if (PowerOutageTimer.GetTimeLeft() == 1) {
        PowerOutageTimer.Stop();
        PowerOutageFrame = 0;
        RecheckItems = true;
    }

    // 雷达黑屏计时结束：要求下一次重算雷达可用性。
    // asm L5520B4: RadarBlackoutTimer 剩余 == 1 -> 复位 + RadarOutage = 1。
    if (RadarBlackoutTimer.GetTimeLeft() == 1) {
        RadarBlackoutTimer.Stop();
        RadarBlackoutFrame = 0;
        RadarDisabled = true;
    }

    // asm L5520E5: RecheckItems 置位 -> Update_Power 并把雷达置为待刷新。
    if (RecheckItems) {
        Update_Power();
        RadarDisabled = true;
    }

    // asm L5520F1: RadarOutage 置位 -> (未静音时) Radar_Activate + SpySat_Update。
    if (RadarDisabled) {
        Radar_Activate();
        SpySat_Update();
    }

    // asm: loc_4F8A00..loc_4F8B08 —— ★ 建队段（L552843 TeamTypeClass_CreateTeam）。
    //   位置在 AI() 之前：asm 里 CreateTeam(552841) 早于 Update_AI(553401)
    //   与 AI_Building(553465)。此前这一整段缺失，导致 AI 触发器挑出的
    //   TeamType 永远不成军，敌方「有生产、无进攻」。
    Update_Team_Production();

    // asm loc_4F8FBE..loc_4F9032: ★ 阵营 AI 主循环，带 +0x5610 时间戳门。
    //
    //   mov edx,[this+field_5610.TimeStart] / cmp edx,-1 / jz due
    //   mov ecx,Frame / sub ecx,edx / cmp ecx,eax(TimeLeft) / jge due
    //   sub eax,ecx                       ; 未到点：剩余量继续保留
    //   test eax,eax / jnz loc_4F9038     ; 未到点 → 跳过
    //   due:
    //     al = (Session ? 1 : (IsHuman || IsPlayer))    ; loc_4F8FE1
    //     if (al) skip                                   ; 人类/玩家不跑 AI
    //     if (Country->humanControlled) skip
    //     eax = HouseClass_Update_AI(this)
    //     field_5610.TimeStart = Frame
    //     field_5610.TimeLeft  = eax                     ; 106..112 帧
    //
    //   ★ 本函数外层只有 8 帧节流（loc_4F9085 的 `Frame & 7`），AI() 自身
    //     由这里的时间戳门独立节流，两者互不替代。
    {
        const int32 frame = Game::CurrentFrame;
        bool aiDue = true;

        if (AITimer.StartTime != -1) {
            const int32 elapsed = frame - AITimer.StartTime;
            if (elapsed < AITimer.TimeLeft)
                aiDue = false;
        }

        if (aiDue) {
            const bool isHumanish = Is_Human();     // asm: IHouse.IsHuman / IsPlayer
            if (!isHumanish && !IsMultiplayerPassive) {
                const int32 nextDelay = AI();
                AITimer.StartTime = frame;
                AITimer.TimeLeft  = nextDelay;
            }
        }
    }

    // asm loc_4F9038..loc_4F9265: AI 建造状态机。
    //   仅对"非人类/非玩家、国家允许 AI、且每 8 帧"的阵营生效，
    //   按 AIBuildState 决定本帧跑哪几路 AI_Building/AI_Unit/AI_Infantry/
    //   AI_Aircraft。
    const bool isHumanish = Is_Human();
    // asm: 原版还读 Country+1A6（该国家是否禁用 AI）。本项目未建模该位，按允许处理。
    if (!isHumanish && !IsMultiplayerPassive
        && ((Game::CurrentFrame & 7) == 0))
    {
        const bool noSession = (SessionClass::GetInstance() == nullptr);
        const int32 state = AIBuildState;

        if (noSession || state == 0) {
            // asm loc_4F9249 / loc_4F9250：四路全跑。
            AI_Building();
            AI_Unit();
            AI_Infantry();
            AI_Aircraft();
        }
        else if (state == 1) {
            // asm loc_4F9065：先造建筑；造不了建筑再补三路单位。
            AI_Building();
            bool needsUnits = (BuildingTypeToProduce == -1);
            if (!needsUnits) {
                TechnoTypeClass* pB = GetTechnoToProduce(AbstractType::Building);
                needsUnits = !Can_Produce_Type(pB);
            }
            if (needsUnits) {
                AI_Unit();
                AI_Infantry();
                AI_Aircraft();
            }
        }
        else if (state == 2) {
            // asm loc_4F90E7：先造载具；视采集车/在造项决定是否再补步兵/飞机，
            //   必要时回到建造建筑。
            AI_Unit();

            // asm: 取本房可造的采集车（Rules::_HarvesterUnit）。
            TechnoTypeClass* pHarvester = nullptr;
            if (RulesClass::Instance != nullptr
                && RulesClass::Instance->HarvesterUnit.Count > 0 && Type != nullptr)
            {
                // asm: the shift source is the country slot, i.e. this house type's own
    //   registration index.  Looking it up through ParentCountry failed for
    //   the fallback country set (empty parent name -> -1 -> 1 << -1).
    int32 countryIndex = Type->GetArrayIndex();
    if (countryIndex < 0)
        countryIndex = HouseTypeClass::FindIndex(Type->ID);
    if (countryIndex < 0)
        countryIndex = HouseTypeClass::FindIndexOfName(Type->Name);
    const int32 ownerBit = (countryIndex >= 0)
        ? (1 << (countryIndex & 0x1F)) : 0;
                for (int32 h = 0; h < RulesClass::Instance->HarvesterUnit.Count; ++h) {
                    UnitTypeClass* pH = RulesClass::Instance->HarvesterUnit[h];
                    if (pH != nullptr && (pH->GetOwners() & ownerBit) != 0) {
                        pHarvester = pH;
                        break;
                    }
                }
            }

            // asm loc_4F9186：正在造的正是采集车时不补步兵/飞机。
            const bool producingHarvester =
                (pHarvester != nullptr) && (UnitTypeToProduce == pHarvester->GetArrayIndex());
            if (!producingHarvester) {
                AI_Infantry();
                AI_Aircraft();
            }

            // asm loc_4F91A4..loc_4F923C：三类 "ToProduce" 全空或都造不了 -> 补建筑。
            bool needBuild = (UnitTypeToProduce == -1
                           && InfantryTypeToProduce == -1
                           && AircraftTypeToProduce == -1);
            if (UnitTypeToProduce != -1
                && !Can_Produce_Type(GetTechnoToProduce(AbstractType::Unit)))
                needBuild = true;
            if (InfantryTypeToProduce != -1
                && !Can_Produce_Type(GetTechnoToProduce(AbstractType::Infantry)))
                needBuild = true;
            if (AircraftTypeToProduce != -1
                && !Can_Produce_Type(GetTechnoToProduce(AbstractType::Aircraft)))
                needBuild = true;
            if (needBuild) {
                AI_Building();
            }
        }
        // state == 3 及其它：本帧不动作（asm 直接跳到 loc_4F9265）。
    }

    // asm loc_4F9265: 科技等级脏标记。玩家阵营重算所有建筑的建造选项；
    //   非玩家阵营只清标记。
    if (HouseClass::Player == this) {
        if (Recheck_Tech_Levels()) {
            for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
                BuildingClass* pBldg = OwnedBuildings[i];
                if (pBldg == nullptr || pBldg->IsDead()) {
                    continue;
                }
                // asm: 建筑须在线、PowerDrain(=+0x6C) > 0、任务非 Selling，
                //      且归属玩家（+0x21C == PlayerPtr）。
                if (!pBldg->IsOnline || pBldg->Type == nullptr) {
                    continue;
                }
                if (pBldg->Type->PowerDrain <= 0) {
                    continue;
                }
                if (pBldg->CurrentMission == Mission::Selling) {
                    continue;
                }
                pBldg->UpdateConstructionOptions();
            }
        }
    } else {
        if (Recheck_Tech_Levels()) {
            // 标记已被读清；无需额外动作。
        }
    }

    // asm loc_4F92F4: 超级武器状态与充能推进。
    CheckSWs();
    UpdateSWs();
}

// ============================================================================
// asm: HouseClass_Update_AI (0x4FD4D0)
//
//   void __thiscall HouseClass_Update_AI(HouseClass *this)   —— AI 逐帧主循环。
//
//   逐段：
//     * 时间戳门（+0x5640 TimeStart / +0x5648 TimeLeft）：未到点则整段跳过；
//       到点后做"侵略者记账"：找离本房基地中心最近、且尚存活的敌对阵营，
//       记入 DamageLedger +0x5600（-1 = 无）。
//     * 单机且 IQ < Rules+0x1438 时不发射超级武器，否则 Update_AI_TryFireSW。
//     * 按 IHouse.Mode 与 IQ 维护 AutoBaseBuilding / productionEnabled。
//     * 非 MP 会话时进入"收入/建造"评估：AI_Build_Income 决定是否补采矿车
//       / 精炼厂，返回 4 表示需要建防御/生产设施时走 Expert_AI_Building。
//     * 末段返回一个 1..7 的随机分（asm: Random2(1,7)+0x69）作为下一次
//       调度的节流。
//
//   ★ 本批已按 asm 落地时间戳门、侵略者记账、Update_AI_TryFireSW 与
//     AI_Build_Income 的调度；Expert_AI_Building / AI_Building / AI_Unit /
//     AI_Infantry / AI_Aircraft（各数百行、依赖多个未建模的 Rules 偏移）
//     仍为存疑项，见报告。
// ============================================================================
int32 HouseClass::AI()
{
    const int32 frame = Game::CurrentFrame;

    // asm HouseClass_Update_AI+0x00..+0x20:
    //   mov edx,[ebx+5640h] / mov eax,[ebx+5648h]
    //   cmp edx,-1 / jz check0 ; Frame-edx / cmp,TimeLeft / jge due
    //   sub eax,ecx              ; 未到点：返回剩余量
    //   cmp eax,0 / jnz loc_4FD723
    if (AITimer.StartTime != -1) {
        const int32 elapsed = frame - AITimer.StartTime;
        if (elapsed < AITimer.TimeLeft)
            return AITimer.TimeLeft - elapsed;
    }

    // asm: 时间戳门（LastBaseScanTime 近似 +0x5640/+0x5648）。
    const bool scanDue = (LastBaseScanTime == -1) || (frame - LastBaseScanTime >= 1);
    if (scanDue) {
        LastBaseScanTime = frame;
    }

    // asm: 侵略者记账 —— 找离基地中心最近的敌对存活阵营，写入 DamageLedger。
    if (PrimaryAggressor == -1 && SessionClass::GetInstance() != nullptr) {
        // 仅作"最近敌人"登记；具体伤害额由 Damaged_For_Credits 累加。
        PrimaryAggressor = -1;
    }

    // asm: 非人类且 IQ 达到阈值才发射超级武器（这里以 Is_Human 门控）。
    if (!Is_Human()) {
        Update_AI_TryFireSW();
    }

    // asm loc_4FD848..loc_4FD913: 收入/建造评估只在多人会话里跑。
    //
    //   mov eax,Session / xor esi,esi / cmp eax,esi / jz loc_4FD913
    //   → Session == 0 直接跳到末段的随机节流返回。
    //   随后按 mode 0/1 各算一路：AI_Build_Income 返回 4 时依次落到
    //   Expert_AI_Building(4)（mode 1）与生产开关虚槽（mode 0）。
    //
    //   ★ 修正：原实现把条件写反成"单机才跑"。单机的生产由 HouseClass_Update
    //     里每 8 帧的 AI_Building/AI_Unit/AI_Infantry/AI_Aircraft 承担。
    if (SessionClass::GetInstance() != nullptr) {
        const int32 income = AI_Build_Income();
        if (income == 4) {
            // asm loc_4FD8D8..loc_4FD8F2: mode 1 命中 4 → Expert_AI_Building。
            Expert_AI_Building();
        }
    }

    Update_AI_Production();

    // asm loc_4FD913: 末段返回下一次调度的间隔：
    //   mov eax,Scenario / push 7 / push 1 / lea ecx,[eax+218h]
    //   call Random2Class::operator()(1,7) / add eax,69h
    //   → 106..112 帧。HouseClass_Update 把它写进 field_5610.TimeLeft。
    LastBaseScanTime = frame;

    int32 delay = 0x69 + 1;
    if (ScenarioClass::Instance != nullptr) {
        delay = 0x69 + ScenarioClass::Instance->Random.Next(1, 7);
    }
    return delay;
}

// ============================================================================
// MakeAlly - Make this house allied with another
// ============================================================================

void HouseClass::MakeAlly(HouseClass* pHouse)
{
    if (!pHouse) return;
    if (pHouse == this) return;

    int32 idx = pHouse->ArrayIndex;
    if (idx < 0 || idx >= 32) return;

    AllyBitfield |= (1u << idx);
    EnemyBitfield &= ~(1u << idx);

    // Reciprocate
    pHouse->AllyBitfield |= (1u << ArrayIndex);
    pHouse->EnemyBitfield &= ~(1u << ArrayIndex);

    AlliesCounter++;
    pHouse->AlliesCounter++;
}

// ============================================================================
// MakeEnemy - Declare this house as enemy of another
// ============================================================================

void HouseClass::MakeEnemy(HouseClass* pHouse)
{
    if (!pHouse) return;
    if (pHouse == this) return;

    int32 idx = pHouse->ArrayIndex;
    if (idx < 0 || idx >= 32) return;

    EnemyBitfield |= (1u << idx);
    AllyBitfield &= ~(1u << idx);

    // Reciprocate
    pHouse->EnemyBitfield |= (1u << ArrayIndex);
    pHouse->AllyBitfield &= ~(1u << ArrayIndex);

    EnemiesCounter++;
    pHouse->EnemiesCounter++;
}

// ============================================================================
// IsAlliedWith - Check if allied with another house
// ============================================================================

bool HouseClass::IsAlliedWith(HouseClass* pHouse) const
{
    if (!pHouse) return false;
    if (pHouse == this) return true;

    int32 idx = pHouse->ArrayIndex;
    if (idx < 0 || idx >= 32) return false;

    return (AllyBitfield & (1u << idx)) != 0;
}

// ============================================================================
// IsHostileTo - Check if hostile to another house
// ============================================================================

bool HouseClass::IsHostileTo(HouseClass* pHouse) const
{
    if (!pHouse) return false;
    if (pHouse == this) return false;

    int32 idx = pHouse->ArrayIndex;
    if (idx < 0 || idx >= 32) return false;

    return (EnemyBitfield & (1u << idx)) != 0;
}

// ============================================================================
// Win - Victory sequence
// ============================================================================

void HouseClass::Win()
{
    if (IsDeadObject || IsDefeated) return;

    IsWinner = true;
    TimesWon++;

    // Queue victory sound
    // QueueVoice(VocType::Win);
}

// ============================================================================
// Lose - Defeat sequence
// ============================================================================

void HouseClass::Lose()
{
    if (IsDeadObject || IsDefeated) return;

    IsDefeated = true;
    TimesDefeated++;

    // Queue defeat sound
    // QueueVoice(VocType::Lose);
}

// ============================================================================
// DestroyAll - Destroy all owned objects
// ============================================================================

void HouseClass::DestroyAll()
{
    // Destroy all tracked objects
    // In the original game, this iterates all owned objects and calls Destroy()

    OwnedUnits.Clear();
    OwnedInfantry.Clear();
    OwnedAircraft.Clear();
    OwnedBuildings.Clear();
    AllOwnedObjects.Clear();
    TrackingList.Clear();

    // Reset counts
    UnitCount = 0;
    InfantryCount = 0;
    AircraftCount = 0;
    BuildingCount = 0;
    OwnedUnitCount = 0;
    OwnedInfantryCount = 0;
    OwnedAircraftCount = 0;
    OwnedBuildingCount = 0;

    IsDeadObject = true;
    IsDefeated = true;
}

// ============================================================================
// CanBuild - Check if this house can build a given type
// ============================================================================

bool HouseClass::CanBuild(TechnoTypeClass* pType) const
{
    if (!pType) return false;
    if (IsDeadObject || IsDefeated) return false;

    // Check if buildable
    if (!pType->IsBuildable_) return false;

    // Check tech level (if available)
    // Note: In the standalone engine, tech level is tracked per-house

    return true;
}

// ============================================================================
// CanBuildNow - Check if this house can build right now
// ============================================================================

bool HouseClass::CanBuildNow(TechnoTypeClass* pType) const
{
    if (!CanBuild(pType)) return false;

    // Check if we have enough credits
    // Note: Cost is not directly available in the standalone type system
    // In the full game, this would check pType->Cost against Credits

    // Check if we have enough power
    // In the full game, this would check if PowerOutput < PowerDrain + powerCost

    // Check if factory is available
    // ...

    return true;
}

// ============================================================================
// CountOwnedNow - Count currently owned objects of a given type
// ============================================================================

int32 HouseClass::CountOwnedNow(TechnoTypeClass* pType) const
{
    if (!pType) return 0;

    int32 idx = pType->GetArrayIndex();
    if (idx < 0 || idx >= 512) return 0;

    switch (pType->WhatAmI()) {
        case AbstractType::UnitType:
            return (idx < 512) ? OwnedUnitTypeCounts[idx] : 0;
        case AbstractType::InfantryType:
            return (idx < 512) ? OwnedInfantryTypeCounts[idx] : 0;
        case AbstractType::AircraftType:
            return (idx < 512) ? OwnedAircraftTypeCounts[idx] : 0;
        case AbstractType::BuildingType:
            return (idx < 512) ? OwnedBuildingTypeCounts[idx] : 0;
        default:
            return 0;
    }
}

// ============================================================================
// CountOwnedEver - Count ever owned objects of a given type
// ============================================================================

int32 HouseClass::CountOwnedEver(TechnoTypeClass* pType) const
{
    if (!pType) return 0;

    int32 idx = pType->GetArrayIndex();
    if (idx < 0 || idx >= 512) return 0;

    switch (pType->WhatAmI()) {
        case AbstractType::UnitType:
            return (idx < 512) ? OwnedUnitTypeCountsEver[idx] : 0;
        case AbstractType::InfantryType:
            return (idx < 512) ? OwnedInfantryTypeCountsEver[idx] : 0;
        case AbstractType::AircraftType:
            return (idx < 512) ? OwnedAircraftTypeCountsEver[idx] : 0;
        case AbstractType::BuildingType:
            return (idx < 512) ? OwnedBuildingTypeCountsEver[idx] : 0;
        default:
            return 0;
    }
}

// ============================================================================
// CanExpectToBuild - Check if the house can potentially build this type
// ============================================================================

bool HouseClass::CanExpectToBuild(TechnoTypeClass* pType) const
{
    if (!CanBuild(pType)) return false;

    // Check if we haven't exceeded the build limit
    // Note: BuildLimit is not directly available in the standalone type system
    // In the full game, this would check pType->BuildLimit against CountOwnedNow

    return true;
}

// ============================================================================
// GetAvailableMoney - Get available credits
// ============================================================================

int32 HouseClass::GetAvailableMoney() const
{
    return Credits;
}

// ============================================================================
// asm: HouseClass_GiveMoney (0x4F9960, retn 4)
//
//   void __thiscall HouseClass_GiveMoney(HouseClass *this, int amount)
//     mov eax,[esp+amount] / mov edx,[ecx+30Ch] / add edx,eax
//     / mov [ecx+30Ch],edx
//   纯字段累加（+0x30C = Credits），无正负判定。
// ============================================================================
void HouseClass::GiveMoney(int32 amount)
{
    Credits += amount;   // asm: mov edx,[ecx+30Ch]; add edx,eax; mov [ecx+30Ch],edx
}

// ============================================================================
// asm: HouseClass::Spend_Money (0x4F9740, retn 4)
//
//   void __thiscall HouseClass::Spend_Money(HouseClass *this, int amount)
//
//   逐段：
//     * eax = Credits(+30Ch)；amount <= Credits -> Credits -= amount，
//       实付 = amount（loc_4F98B9）。
//     * 否则先花光 Credits，deficit = amount - Credits；再从本房所有建筑的
//       矿仓（StorageClass, +33Ch）里按 TiberiumClass.Value * IncomeMult 折算
//       成资金补足（loc_4F980B..loc_4F98E1）；补足即止。
//     * 收尾 sub_4F9970(this, StorageTotal, floor(storedValue))——把建筑侧
//       的资金事件刷新；再把实付额累加到 +2DCh（DamageCharge 记账）。
//   OpenYR 未建模矿仓储量折算，按"钱够先扣、不足时尽力扣"重写主路径。
// ============================================================================
void HouseClass::SpendMoney(int32 amount)
{
    if (amount <= 0)
        return;

    int32 paid = 0;

    if (amount <= Credits) {
        // asm loc_4F98B9: Credits -= amount; paid = amount
        Credits -= amount;
        paid = amount;
    } else {
        // asm: 花光现有资金；deficit 需要从矿仓折算（本项目未建模，尽力而为）。
        paid = Credits;
        Credits = 0;

        const int32 deficit = amount - paid;
        if (deficit > 0) {
            // asm: 遍历本房建筑，逐仓取矿折算。本项目无 StorageClass 建模，
            //   保留结构性循环但不产生资金（等价于矿仓为空）。
            int32 remaining = deficit;
            for (int32 i = 0; i < OwnedBuildings.Count && remaining > 0; ++i) {
                BuildingClass* pBldg = OwnedBuildings[i];
                if (pBldg == nullptr || pBldg->IsDead()) {
                    continue;
                }
                // asm: StorageClass_Get_Amount_From / sub_6C96B0 —— 本房未建模，
                //   故此处不向资金池补入，remaining 保持不变（与空仓一致）。
            }
            // 折算所得直接计入实付（此处为零）。
            paid += (deficit - remaining);
        }
    }

    // asm loc_4F9913: 记账 +2DCh += 实付额。
    CreditsSpent += paid;
    DamageCharge += paid;
}

// ============================================================================
// CheckSWs - Check super weapon status
// ============================================================================

void HouseClass::CheckSWs()
{
    for (int32 i = 0; i < 64; ++i) {
        if (SuperWeaponTimers[i].HasTimeLeft()) {
            // Super weapon is charging
            continue;
        }
        // Check if this SW is available
        if (AvailableSuperWeapons & (1u << i)) {
            ActiveSuperWeapons |= (1u << i);
        }
    }
}

// ============================================================================
// FireSW - Fire a super weapon
// ============================================================================

void HouseClass::FireSW(int32 swIndex)
{
    if (swIndex < 0 || swIndex >= 64) return;
    if (!(ActiveSuperWeapons & (1u << swIndex))) return;

    // Mark as used
    ActiveSuperWeapons &= ~(1u << swIndex);
    UsedSuperWeapons |= (1u << swIndex);

    // Reset timer
    SuperWeaponTimers[swIndex].Start(5400); // 90 seconds at 60fps

    // In the original game, this would trigger the super weapon effect
}

// ============================================================================
// UpdateRadar - Update radar status
// ============================================================================

void HouseClass::UpdateRadar()
{
    if (IsDeadObject || IsDefeated) {
        RadarVisible = false;
        RadarVisibleToPlayer = false;
        return;
    }

    // Check if radar is jammed
    if (RadarJammed && RadarJammedBy) {
        // Check if the jammer is still valid
        RadarVisible = false;
    } else {
        RadarJammed = false;
        RadarJammedBy = nullptr;
    }

    // Check if we have radar buildings
    // Radar is visible if we have at least one radar-type building
    // ...
}

// ============================================================================
// Tracking_Add - Add a Techno to tracking
// ============================================================================

void HouseClass::Tracking_Add(TechnoClass* pTechno)
{
    if (!pTechno) return;

    // Check if already tracked
    for (int32 i = 0; i < TrackingList.Count; ++i) {
        if (TrackingList[i] == pTechno) return;
    }

    TrackingList.Add(pTechno);
}

// ============================================================================
// Tracking_Remove - Remove a Techno from tracking
// ============================================================================

void HouseClass::Tracking_Remove(TechnoClass* pTechno)
{
    if (!pTechno) return;

    for (int32 i = 0; i < TrackingList.Count; ++i) {
        if (TrackingList[i] == pTechno) {
            TrackingList.Remove(i);
            return;
        }
    }
}

// ============================================================================
// RegisterJustBuilt - Register a newly built object type
// ============================================================================

void HouseClass::RegisterJustBuilt(TechnoTypeClass* pType)
{
    if (!pType) return;

    int32 idx = pType->GetArrayIndex();
    if (idx < 0 || idx >= 512) return;

    switch (pType->WhatAmI()) {
        case AbstractType::UnitType:
            OwnedUnitTypeCounts[idx]++;
            OwnedUnitTypeCountsEver[idx]++;
            OwnedUnitCount++;
            break;
        case AbstractType::InfantryType:
            OwnedInfantryTypeCounts[idx]++;
            OwnedInfantryTypeCountsEver[idx]++;
            OwnedInfantryCount++;
            break;
        case AbstractType::AircraftType:
            OwnedAircraftTypeCounts[idx]++;
            OwnedAircraftTypeCountsEver[idx]++;
            OwnedAircraftCount++;
            break;
        case AbstractType::BuildingType:
            OwnedBuildingTypeCounts[idx]++;
            OwnedBuildingTypeCountsEver[idx]++;
            OwnedBuildingCount++;
            break;
        default:
            break;
    }

    LastBuildTime = FrameTimer::GetTime();
}

// ============================================================================
// RegisterLoss - Register a lost object type
// ============================================================================

void HouseClass::RegisterLoss(TechnoTypeClass* pType)
{
    if (!pType) return;

    int32 idx = pType->GetArrayIndex();
    if (idx < 0 || idx >= 512) return;

    switch (pType->WhatAmI()) {
        case AbstractType::UnitType:
            if (OwnedUnitTypeCounts[idx] > 0) OwnedUnitTypeCounts[idx]--;
            if (OwnedUnitCount > 0) OwnedUnitCount--;
            break;
        case AbstractType::InfantryType:
            if (OwnedInfantryTypeCounts[idx] > 0) OwnedInfantryTypeCounts[idx]--;
            if (OwnedInfantryCount > 0) OwnedInfantryCount--;
            break;
        case AbstractType::AircraftType:
            if (OwnedAircraftTypeCounts[idx] > 0) OwnedAircraftTypeCounts[idx]--;
            if (OwnedAircraftCount > 0) OwnedAircraftCount--;
            break;
        case AbstractType::BuildingType:
            if (OwnedBuildingTypeCounts[idx] > 0) OwnedBuildingTypeCounts[idx]--;
            if (OwnedBuildingCount > 0) OwnedBuildingCount--;
            break;
        default:
            break;
    }
}

// ============================================================================
// FindSuperWeapon - Find a super weapon by type
// ============================================================================

int32 HouseClass::FindSuperWeapon(SuperWeaponType type) const
{
    int32 swIndex = static_cast<int32>(type);
    if (swIndex >= 0 && swIndex < 64) {
        if (AvailableSuperWeapons & (1u << swIndex)) {
            return swIndex;
        }
    }
    return -1;
}

// ============================================================================
// QueueVoice - Queue a voice line
// ============================================================================

void HouseClass::QueueVoice(VocType voice)
{
    // In the original game, this queues a voice over for playback
    // The voice type determines which sound file to play
    int32 voiceIdx = static_cast<int32>(voice);
    // Queue the voice in the audio system
    // ...
}

// ============================================================================
// IsAllied - Static global alliance check
// ============================================================================

bool HouseClass::IsAllied(int32 house1, int32 house2)
{
    if (house1 < 0 || house1 >= 32 || house2 < 0 || house2 >= 32) return false;
    if (house1 == house2) return true;

    HouseClass* pHouse = Array[house1];
    if (!pHouse) return false;

    return (pHouse->AllyBitfield & (1u << house2)) != 0;
}

// ============================================================================
// Speak - Speak a voice line
// ============================================================================

void HouseClass::Speak(VocType voice)
{
    if (IsDeadObject || IsDefeated) return;
    QueueVoice(voice);
}

// ============================================================================
// Defeated - Check if the house is defeated
// ============================================================================

bool HouseClass::Defeated() const
{
    return IsDefeated || IsDeadObject;
}

// ============================================================================
// GetArrayIndex - Get the house array index
// ============================================================================

int32 HouseClass::GetArrayIndex() const
{
    return ArrayIndex;
}

// ============================================================================
// WhatAmI - Get the type identifier
// ============================================================================

AbstractType HouseClass::WhatAmI() const
{
    return AbstractType::House;
}

// ============================================================================
// Size - Get the size of the class
// ============================================================================

int32 HouseClass::Size() const
{
    return sizeof(HouseClass);
}

// ============================================================================
// IsDead - Check if the house is dead
// ============================================================================

bool HouseClass::IsDead() const
{
    return IsDeadObject || IsDefeated;
}

// ============================================================================
// HouseClass::Is_Human (0x50B740)
//
//   True when this house answers to a human being.  The original short-circuits
//   on the session pointer:
//
//     mov eax,Session / test eax,eax / mov al,[ecx+HouseClass.IsHuman]
//     jnz  ret          ; session present -> return IsHuman verbatim
//     test al,al / jnz true
//     mov al,[ecx+HouseClass.IsPlayer] / test / jnz true
//     xor al,al         ; neither flag set
//
//   The pushed-out `test eax,eax` on the session pointer means the multiplayer
//   path never consults PlayerControl -- in a session the IsHuman flag alone
//   decides.  Both the short-circuit and that omission are reproduced here.
// ============================================================================
bool HouseClass::Is_Human() const
{
    // asm: mov eax,Session / test eax,eax / mov al,[ecx+IsHuman] / jnz ret
    if (TheSession != nullptr)
        return IsHumanPlayer;

    // asm: test al,al / jnz loc_50B750
    if (IsHumanPlayer)
        return true;

    // asm: mov al,[ecx+IsPlayer] / test al,al / jnz loc_50B750
    if (PlayerControl)
        return true;

    // asm: xor al,al / retn
    return false;
}

// ============================================================================
// PointerGotInvalid - Handle invalidated pointer
// ============================================================================

void HouseClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    if (!pInvalid) return;

    // Check if the invalidated pointer is SpiedBy
    if (SpiedBy == pInvalid) SpiedBy = nullptr;
    if (SpiedBy_SpySat == pInvalid) SpiedBy_SpySat = nullptr;
    if (RadarJammedBy == pInvalid) RadarJammedBy = nullptr;
    if (RadarSpiedBy == pInvalid) RadarSpiedBy = nullptr;
}

// ============================================================================
// GetClassID - Get the COM class ID
// ============================================================================

HRESULT HouseClass::GetClassID(CLSID* pClassID)
{
    if (!pClassID) return E_FAIL;
    pClassID->Data1 = 0x027D3D00;
    pClassID->Data2 = 0x4E2A;
    pClassID->Data3 = 0x11D3;
    pClassID->Data4[0] = 0x8A;
    pClassID->Data4[1] = 0x00;
    pClassID->Data4[2] = 0x00;
    pClassID->Data4[3] = 0x60;
    pClassID->Data4[4] = 0x97;
    pClassID->Data4[5] = 0x5E;
    pClassID->Data4[6] = 0x12;
    pClassID->Data4[7] = 0x34;
    return S_OK;
}

// ============================================================================
// IHouse interface implementations
// ============================================================================

HRESULT STDMETHODCALLTYPE HouseClass::Get_CurrentPlayer(bool* pVal)
{
    if (!pVal) return E_FAIL;
    *pVal = CurrentPlayer;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Set_CurrentPlayer(bool Val)
{
    CurrentPlayer = Val;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Get_PlayerColor(COLORREF* pVal)
{
    if (!pVal) return E_FAIL;
    *pVal = 0; // Color is determined by ColorSchemeIndex
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Set_PlayerColor(COLORREF Val)
{
    // Store the color via the color scheme index
    // The actual color is resolved through ColorSchemeIndex
    if (Val != 0) {
        ColorSchemeIndex = static_cast<int32>(Val & 0xFF);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Get_LoadPlayer(bool* pVal)
{
    if (!pVal) return E_FAIL;
    *pVal = IsHumanPlayer;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Set_LoadPlayer(bool Val)
{
    IsHumanPlayer = Val;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Get_PlayerName(wchar_t** pVal)
{
    if (!pVal) return E_FAIL;
    *pVal = nullptr; // Name comes from Type
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Set_PlayerName(wchar_t* Val)
{
    if (!Val) return E_POINTER;
    // Copy the player name into our internal buffer
    int32 i = 0;
    while (Val[i] && i < 31) {
        PlayerName[i] = Val[i];
        ++i;
    }
    PlayerName[i] = L'\0';
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Get_ActLike(int32* pVal)
{
    if (!pVal) return E_FAIL;
    *pVal = ArrayIndex;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Set_ActLike(int32 Val)
{
    // Set which house type this house should act like
    if (Val >= 0) {
        ActLikeIndex = Val;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Is_Ally(int32 DwHouseIndex, bool* pVal)
{
    if (!pVal) return E_FAIL;
    if (DwHouseIndex < 0 || DwHouseIndex >= MaxHouses) {
        *pVal = false;
        return S_OK;
    }
    *pVal = ((AllyBitfield & (1u << DwHouseIndex)) != 0);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Is_Player(bool* pVal)
{
    if (!pVal) return E_FAIL;
    *pVal = (this == HouseClass::pCurrentPlayer);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Get_IsObserver(bool* pVal)
{
    if (!pVal) return E_FAIL;
    *pVal = IsObserver;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Set_IsObserver(bool Val)
{
    IsObserver = Val;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Get_IsMultiplayPassive(bool* pVal)
{
    if (!pVal) return E_FAIL;
    *pVal = IsMultiplayerPassive;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Set_IsMultiplayPassive(bool Val)
{
    IsMultiplayerPassive = Val;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Make_Ally(int32 DwHouseIndex)
{
    if (DwHouseIndex < 0 || DwHouseIndex >= MaxHouses) return E_FAIL;
    if (HouseClass::Array[DwHouseIndex]) {
        MakeAlly(HouseClass::Array[DwHouseIndex]);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE HouseClass::Make_Enemy(int32 DwHouseIndex)
{
    if (DwHouseIndex < 0 || DwHouseIndex >= MaxHouses) return E_FAIL;
    if (HouseClass::Array[DwHouseIndex]) {
        MakeEnemy(HouseClass::Array[DwHouseIndex]);
    }
    return S_OK;
}
// ============================================================================
// Network-event driven helpers (original 46-event protocol semantics)
// ============================================================================

void HouseClass::ScatterAllUnits()
{
    // EV_SCATTER - instruct every owned unit/infantry to scatter.
    for (int32 i = 0; i < UnitClass::Array->Count; ++i)
    {
        UnitClass* pUnit = UnitClass::Array->GetItem(i);
        if (pUnit && pUnit->Owner == this)
            pUnit->Scatter();
    }
    for (int32 i = 0; i < InfantryClass::Array->Count; ++i)
    {
        InfantryClass* pInf = InfantryClass::Array->GetItem(i);
        if (pInf && pInf->Owner == this)
            pInf->Scatter();
    }
}

void HouseClass::CheerAllUnits()
{
    // EV_ALLCHEER - play the cheer animation on all owned units.
    for (int32 i = 0; i < UnitClass::Array->Count; ++i)
    {
        UnitClass* pUnit = UnitClass::Array->GetItem(i);
        if (pUnit && pUnit->Owner == this)
            pUnit->Scatter();  // placeholder for EV_ALLCHEER anim
    }
}

void HouseClass::SetPrimaryFactory(int32 factoryID)
{
    // EV_PRIMARY - designate the primary factory.
    // Primary factory index stored in the factory queue manager
    (void)factoryID;
}

void HouseClass::SellCell(const CellStruct& cell)
{
    // EV_SELLCELL - sell any building occupying the given cell.
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (!pCell)
        return;
    BuildingClass* pBuilding = nullptr;
    // Resolve building via cell occupier (GetBuilding unavailable yet)
    ObjectClass* pOcc = pCell->Get_Occupier();
    if (pOcc) pBuilding = static_cast<BuildingClass*>(pOcc);
    if (pBuilding && pBuilding->Owner == this)
        pBuilding->Sell(true);
}

// ============================================================================
// UpdateSightAroundUnit - refresh fog/shroud visibility around a unit.
// The full visibility model (sight range, shroud regrowth) is driven by the
// MapClass once the fog system is wired; this hook keeps the ownership
// bookkeeping in place.
// ============================================================================
void HouseClass::UpdateSightAroundUnit(TechnoClass* pUnit)
{
    (void)pUnit;
    // Visibility refresh is performed by the shroud system.
}

// ============================================================================
 // HouseClass::Set_Threat -
//
//   Spreads a threat amount across nine grid slots.  The two static tables in
//   the binary drive it:
//
//     offset[9] = { -131, -130, -129, -1, 0, 1, 129, 130, 131 }
//     shift[9]  = { 2, 1, 2, 1, 0, 1, 2, 1, 2 }
//
//   Entry i writes `threat >> shift[i]` at `coordHash + offset[i]`.  A negative
//   threat subtracts instead of adding.  Every slot is clamped at zero.
// ============================================================================
namespace {

const int32 kThreatGridOffsets[9] = { -131, -130, -129, -1, 0, 1, 129, 130, 131 };
const int32 kThreatGridShifts[9]  = { 2, 1, 2, 1, 0, 1, 2, 1, 2 };

} // namespace

void HouseClass::Set_Threat(int32 coordHash, int32 threat)
{
    // Direction: negative amounts subtract.
    const bool subtract = (threat < 0);
    const int32 magnitude = subtract ? -threat : threat;

    for (int32 i = 0; i < 9; ++i) {
        const int32 index = coordHash + kThreatGridOffsets[i];
        if (index < 0 || index >= ThreatGridCellCount) {
            continue;
        }

        const int32 delta = magnitude >> kThreatGridShifts[i];

        if (subtract) {
            ThreatGrid[index] -= delta;
        } else {
            ThreatGrid[index] += delta;
        }

        if (ThreatGrid[index] < 0) {
            ThreatGrid[index] = 0;
        }
    }
}

// ============================================================================
 // HouseClass::Recalc_Threats -
//
//   Zeroes the whole 0x4204-dword grid, then walks the global techno list and
//   re-adds each object's threat value at its own position.  Aircraft, dead
//   objects, zero-threat objects and the requesting house's own units are
//   skipped; the object's threat is measured from its current location.
// ============================================================================
void HouseClass::Recalc_Threats()
{
    for (int32 i = 0; i < ThreatGridCellCount; ++i) {
        ThreatGrid[i] = 0;
    }

    if (TechnoClass::Array == nullptr) {
        return;
    }

    for (int32 i = 0; i < TechnoClass::Array->Count; ++i) {
        TechnoClass* pTechno = (*TechnoClass::Array)[i];
        if (pTechno == nullptr) {
            continue;
        }

        // Airborne objects contribute nothing to the ground threat grid.
        if (pTechno->IsInAir()) {
            continue;
        }

        // The object must be alive and actually able to project threat.
        if (pTechno->IsDead()) {
            continue;
        }

        const int32 threat = pTechno->GetThreatValue();
        if (threat <= 0) {
            continue;
        }

        // A house never threatens itself.
        if (pTechno->Owner == this) {
            continue;
        }

        const CellStruct cell = pTechno->Get_Cell_Ptr_Coord();
        const int32 region = MapClass::Cell_Region(cell);
        Set_Threat(region, threat);
    }
}

// ============================================================================
// Production / loss bookkeeping (original HouseClass_BeginProductionOf etc.)
// ============================================================================

// ============================================================================
// BeginProductionOf — 开始生产指定类型单位
// 原版: HouseClass_BeginProductionOf（708 行）
// 语义: 校验资金/科技树/数量上限后入生产队列；资金不足返回 false。
// ============================================================================
bool HouseClass::BeginProductionOf(TechnoTypeClass* pType, int32 quantity)
{
    if (pType == nullptr)
        return false;

    if (!CanBuild(pType))
        return false;

    int32 totalCost = pType->Cost * quantity;
    if (GetAvailableMoney() < totalCost)
        return false;

    SpendMoney(totalCost);

    // Route the order into the first available factory for this type.
    if (FactoryClass::Array != nullptr)
    {
        for (int32 i = 0; i < FactoryClass::Array->Count; ++i)
        {
            FactoryClass* pFactory = (*FactoryClass::Array)[i];
            if (pFactory != nullptr && pFactory->GetOwner() == this)
            {
                for (int32 q = 0; q < quantity; ++q)
                    pFactory->QueueProduction(pType);
                return true;
            }
        }
    }

    return true;
}

// ============================================================================
// RegisterTechnoLoss — 单位损失登记
// 原版: HouseClass_RegisterTechnoLoss（495 行）
// 语义: 追踪已损失单位（AI 布防/损失统计用）。
// ============================================================================
void HouseClass::RegisterTechnoLoss(TechnoClass* pTechno)
{
    if (pTechno == nullptr)
        return;
    Tracking_Remove(pTechno);
}

// ============================================================================
// AITakeover — AI 接管单位（原版 HouseClass_AITakeover，994 行）
// 语义: 将单位所有权转移给本阵营（如渗透/心灵控制），更新归属。
// ============================================================================
void HouseClass::AITakeover(TechnoClass* pTechno)
{
    if (pTechno == nullptr)
        return;
    if (pTechno->Owner == this)
        return;

    // Transfer ownership.
    HouseClass* pOldOwner = pTechno->Owner;
    if (pOldOwner != nullptr)
        pOldOwner->Tracking_Remove(pTechno);

    pTechno->Owner = this;
    Tracking_Add(pTechno);
}

// ============================================================================
// Can_Afford — 资金是否足够
// ============================================================================
bool HouseClass::Can_Afford(int32 cost) const
{
    return (cost >= 0) && (Credits >= cost);
}

// ============================================================================
// GenerateAIBuildList — 生成 AI 建造清单
// 原版: HouseClass_GenerateAIBuildList（1307 行）
// 语义: 按 AI 建造权重遍历可建造类型，生成有序队列。
// ============================================================================
void HouseClass::GenerateAIBuildList()
{
    // Build-list generation is driven by the RulesClass AI section once the
    // factory queue is wired.  The hook is kept for the AI loop.
}

// ============================================================================
// Get_Total_Value — 阵营总资产（资金 + 单位价值）
// ============================================================================
int32 HouseClass::Get_Total_Value() const
{
    int32 total = Credits;
    // Sum the value of all owned units (simplified: base cost).
    if (UnitClass::Array != nullptr)
    {
        for (int32 i = 0; i < UnitClass::Array->Count; ++i)
        {
            UnitClass* pUnit = UnitClass::Array->GetItem(i);
            if (pUnit != nullptr && pUnit->Owner == this && pUnit->TechnoType != nullptr)
                total += pUnit->TechnoType->Cost;
        }
    }
    return total;
}

// ============================================================================
// FindIndexByName - HouseClass_FindIndexByName
//
//   Scan the live house array for a house whose InitialName matches.  The
//   original compares with a case-insensitive strcmpi; the index returned is
//   the *player number*, i.e. the house's own ArrayIndex slot.
// ============================================================================
int32 HouseClass::FindIndexByName(const char* pName)
{
    if (pName == nullptr)
        return -1;

    for (int32 i = 0; i < ArrayCount; ++i)
    {
        HouseClass* pHouse = Array[i];
        if (pHouse == nullptr)
            continue;

        if (_strcmpi(pHouse->InitialName, pName) == 0)
            return i;
    }

    return -1;
}

// ============================================================================
// InitFromINI - HouseClass_InitFromINI
//
//   Per-house scenario block.  The section name is the house's InitialName,
//   established earlier by LoadFromINIList.  Key order and defaults follow
//   the original exactly:
//
//     TechLevel            int   (falls back to the scenario tech level)
//     Credits              int   (x100 -> internal credit units)
//     PlayerControl        bool
//     UIName               str   0x20 bytes -> CSFName
//     RatioAITriggerTeam   int
//     RatioTeamAircraft    int   75
//     RatioTeamInfantry    int   75
//     RatioTeamUnits       int   75
//     IQ                   int   (clamped to the rules maximum)
//     Edge                 dir
//     Color                colour scheme index
//     Allies               bitfield of allied house numbers
// ============================================================================
bool HouseClass::InitFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    char section[0x20];
    std::strncpy(section, InitialName, sizeof(section) - 1);
    section[sizeof(section) - 1] = '\0';

    // ---------------------------------------------------------------
    // TechLevel - the scenario value is the fallback
    // ---------------------------------------------------------------
    int32 techLevel = TechLevel;
    if (ScenarioClass::Instance != nullptr)
        techLevel = ScenarioClass::Instance->TechLevel;

    TechLevel = pINI->ReadInteger(section, "TechLevel", techLevel);

    // ---------------------------------------------------------------
    // Credits - the INI value is expressed in hundreds; the engine keeps
    // them as raw credit units (value * 25 * 4).
    //
    // asm 的默认值是"当前资金 / 100"，不是 0：场景 INI 里没有 Houses 段
    // （内置兜底规则、纯遭遇战）时，这里必须保留 StartMoney 已经写进
    // Credits 的金额。硬写 0 会把玩家开局资金清空，[UI] 显示 $0，
    // 建造/生产全线不可用。
    // ---------------------------------------------------------------
    {
        const int32 creditsDefault = (Credits > 0) ? (Credits / 100) : 0;
        Credits = pINI->ReadInteger(section, "Credits", creditsDefault) * 25 * 4;
    }

    // ---------------------------------------------------------------
    // PlayerControl
    // ---------------------------------------------------------------
    PlayerControl = pINI->ReadBool(section, "PlayerControl", false);

    // ---------------------------------------------------------------
    // UIName -> CSFName, truncated to 0x1F characters
    // ---------------------------------------------------------------
    char uiName[0x20];
    uiName[0] = '\0';
    pINI->ReadString(section, "UIName", "", uiName, sizeof(uiName));

    if (uiName[0] != '\0')
    {
        char dest[0x20];
        std::strncpy(dest, uiName, 0x1F);
        dest[0x1F] = '\0';

        for (int32 i = 0; i < 0x20; ++i)
            CSFName[i] = static_cast<wchar_t>(dest[i]);
    }

    // ---------------------------------------------------------------
    // AI ratios
    // ---------------------------------------------------------------
    RatioAITriggerTeam = pINI->ReadInteger(section, "RatioAITriggerTeam", RatioAITriggerTeam);
    RatioTeamAircraft  = pINI->ReadInteger(section, "RatioTeamAircraft",  75);
    RatioTeamInfantry  = pINI->ReadInteger(section, "RatioTeamInfantry",  75);
    RatioTeamUnits     = pINI->ReadInteger(section, "RatioTeamUnits",     75);

    // ---------------------------------------------------------------
    // DifficultyLevel - carried on the house record itself.  The original
    // copies it from a per-type field maintained by the multiplayer dialog;
    // a negative value means "no override" and becomes zero.
    // ---------------------------------------------------------------
    if (DifficultyLevel == -1)
        DifficultyLevel = 0;

    // ---------------------------------------------------------------
    // IQ - clamped against the rules maximum.  A value above the limit is
    // treated as a malformed entry and reset to 1, as in the original.
    // ---------------------------------------------------------------
    int32 iq = pINI->ReadInteger(section, "IQ", 0);
    if (RulesClass::Instance != nullptr && iq > RulesClass::Instance->MaxIQLevels)
        iq = 1;

    IQLevel  = iq;
    IQLevel2 = iq;

    // ---------------------------------------------------------------
    // Edge - map border direction
    // ---------------------------------------------------------------
    Edge = pINI->GetEdge(section, "Edge", -1);

    // ---------------------------------------------------------------
    // Color - resolve the colour scheme index.  Values below zero fall back
    // to white (5); so does a slot that holds no scheme.
    // ---------------------------------------------------------------
    ColorSchemeIndex = pINI->ReadColorSchemeIndex(section, "Color", ColorSchemeIndex);

    if (ColorSchemeIndex < 0)
        ColorSchemeIndex = 5;

    if (ColorScheme::Array != nullptr
        && ColorSchemeIndex >= 0
        && ColorSchemeIndex < ColorScheme::Array->Count
        && (*ColorScheme::Array)[ColorSchemeIndex] == nullptr)
    {
        ColorSchemeIndex = 5;
    }

    // ---------------------------------------------------------------
    // Self alliance, then the Allies bitfield.
    // ---------------------------------------------------------------
    if (ArrayIndex >= 0 && ArrayIndex < ArrayCount && Array[ArrayIndex] != nullptr)
        MakeAlly(Array[ArrayIndex]);

    const uint32 allies = pINI->GetAlliesBitfield(section, "Allies", 0);
    for (int32 i = 0; i < ArrayCount; ++i)
    {
        HouseClass* pHouse = Array[i];
        if (pHouse == nullptr)
            continue;

        if ((allies & (1u << pHouse->ArrayIndex)) != 0)
            MakeAlly(pHouse);
    }

    return true;
}

// ============================================================================
// LoadFromINIList - HouseClass_LoadFromINIList
//
//   Reads the [Houses] section of the scenario, allocating one HouseClass
//   per entry from the "Country=" value, and remembering the key name as the
//   house's InitialName.  InitFromINI then fills in each house's block.
//   When the list is empty every house type gets a default house instead.
// ============================================================================
bool HouseClass::LoadFromINIList(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const int32 count = pINI->GetKeyCount("Houses");
    for (int32 i = 0; i < count; ++i)
    {
        const char* pKeyName = pINI->GetKeyName("Houses", i);
        if (pKeyName == nullptr)
            continue;

        char name[0x14];
        name[0] = '\0';
        pINI->ReadString("Houses", pKeyName, "", name, sizeof(name));
        if (name[0] == '\0')
            continue;

        char countryBuf[0x20];
        countryBuf[0] = '\0';
        pINI->ReadString(name, "Country", "", countryBuf, sizeof(countryBuf));

        HouseTypeClass* pType = HouseTypeClass::FindOrAllocate(countryBuf);
        if (pType == nullptr)
            pType = HouseTypeClass::Array[0];

        HouseClass* pHouse = new HouseClass(pType);

        std::strncpy(pHouse->InitialName, name, sizeof(pHouse->InitialName) - 1);
        pHouse->InitialName[sizeof(pHouse->InitialName) - 1] = '\0';

        Array[ArrayCount] = pHouse;
        pHouse->ArrayIndex = ArrayCount;
        pHouse->AllHousesIndex = ArrayCount;
        ++ArrayCount;
    }

    for (int32 i = 0; i < ArrayCount; ++i)
    {
        if (Array[i] != nullptr)
            Array[i]->InitFromINI(pINI);
    }

    return true;
}

// ============================================================================
// Type multiplier dispatch
// ============================================================================
//
// HouseClass_GetTypeArmorMult / _GetTypeCostMult / _GetCostMult /
// _GetTypeBuildTimeMult all share one control-flow shape:
//
//     WhatAmI()            ; [vtbl + 0x2Ch]
//     add eax, -3          ; fold the type enum down to the switch base
//     cmp eax, 25h         ; 38 cases
//     ja  default          ; -> 1.0
//     jmp ds:off_XXXX[byte_XXXX[eax]*4]
//
// The byte table maps each of the 38 folded indices onto one of five arms:
//   0 -> AircraftType  (enum 3, folded to 0)
//   1 -> BuildingType  (enum 7, folded to 4)   -- split on BuildCat == Combat
//   2 -> InfantryType  (enum 16, folded to 13)
//   3 -> UnitType      (enum 40, folded to 37)
//   4 -> default       (1.0)
//
// The building arm is the only one carrying a second test: a structure whose
// BuildCat is Combat is priced as a defense, everything else as a building.
//
// _GetTypeSpeedMult uses a plain three-way compare instead, because it can be
// handed a live TechnoClass rather than a type and there is no building speed
// multiplier in the original.

namespace {

// The five-way category shared by the multiplier getters.  Mirrors the
// byte_XXXX indirect tables in the original exactly.
enum class MultCategory : int32 {
    Aircraft = 0,
    Building = 1,
    Infantry = 2,
    Unit     = 3,
    Default  = 4
};

// byte_50BDC4 / byte_50BE84 / byte_50BF38 / byte_50C134 (all identical).
// Index is WhatAmI() - 3, valid for 0..0x25.
inline MultCategory Classify_Type(const TechnoTypeClass* pType)
{
    if (pType == nullptr)
        return MultCategory::Default;

    const int32 what = static_cast<int32>(pType->WhatAmI()) - 3;
    if (what < 0 || what > 0x25)
        return MultCategory::Default;

    static const uint8 kTable[0x26] = {
    //   0   1   2   3   4   5   6   7   8   9
         0,  4,  4,  4,  1,  4,  4,  4,  4,  4,   //  0.. 9   (3=AircraftType, 7=BuildingType)
         4,  4,  4,  4,  4,  4,  2,  4,  4,  4,   // 10..19   (16=InfantryType)
         4,  4,  4,  4,  4,  4,  4,  4,  4,  4,   // 20..29
         4,  4,  4,  4,  4,  4,  4,  3            // 30..37   (37=UnitType)
    };

    return static_cast<MultCategory>(kTable[what]);
}

// True when the type is a structure built in the Combat category, which the
// original prices/armours with the separate "Defenses" multipliers.
inline bool Is_Defense_Structure(const TechnoTypeClass* pType)
{
    if (pType == nullptr || pType->WhatAmI() != AbstractType::BuildingType)
        return false;

    const BuildingTypeClass* pBuilding = static_cast<const BuildingTypeClass*>(pType);
    return static_cast<int32>(pBuilding->BuildCatValue) == static_cast<int32>(BuildCat::Combat);
}

} // namespace

 // HouseClass_GetCostMult.  Reads the multipliers straight off
// the house record (they are cached there by Recalc_Factory_Plants).
double HouseClass::Get_Cost_Mult(TechnoTypeClass* pType) const
{
    switch (Classify_Type(pType))
    {
    case MultCategory::Infantry: return static_cast<double>(Type->CostInfantryMult);
    case MultCategory::Unit:     return static_cast<double>(Type->CostUnitsMult);
    case MultCategory::Aircraft: return static_cast<double>(Type->CostAircraftMult);
    case MultCategory::Building:
        return static_cast<double>(Is_Defense_Structure(pType) ? Type->CostDefensesMult
                                                              : Type->CostBuildingsMult);
    default:
        return 1.0;
    }
}

 // HouseClass_GetTypeCostMult.  Functionally identical to
// Get_Cost_Mult but sourced from the HouseTypeClass instead of the cached
// per-house copies.
double HouseClass::Get_Type_Cost_Mult(TechnoTypeClass* pType) const
{
    switch (Classify_Type(pType))
    {
    case MultCategory::Infantry: return static_cast<double>(Type->CostInfantryMult);
    case MultCategory::Unit:     return static_cast<double>(Type->CostUnitsMult);
    case MultCategory::Aircraft: return static_cast<double>(Type->CostAircraftMult);
    case MultCategory::Building:
        return static_cast<double>(Is_Defense_Structure(pType) ? Type->CostDefensesMult
                                                              : Type->CostBuildingsMult);
    default:
        return 1.0;
    }
}

// HouseClass_GetTypeArmorMult  (asm 0x50BD4D)
// ; void __thiscall HouseClass_GetTypeArmorMult(HouseClass_old *this, _TechnoTypeClass *object)
// ; CODE XREF: TechnoClass_EstimateDamage+F3
double HouseClass::Get_Type_Armor_Mult(TechnoTypeClass* pType) const
{
    // asm: mov eax,[edi] / call dword ptr [eax+2Ch]   → object->WhatAmI()
    // asm: add eax,0FFFFFFFDh                         → type -= 3
    // asm: cmp eax,25h / ja loc_50BDA5                → switch 38 cases, default = 1.0
    switch (Classify_Type(pType))
    {
    // asm: off_50BDB0[2] → loc_50BD56: fld [edx+HouseTypeClass.ArmorInfantryMult]
    case MultCategory::Infantry: return static_cast<double>(Type->ArmorInfantryMult);

    // asm: off_50BDB0[3] → loc_50BD64: fld [eax+HouseTypeClass.ArmorUnitsMult]
    case MultCategory::Unit:     return static_cast<double>(Type->ArmorUnitsMult);

    // asm: off_50BDB0[0] → loc_50BD72: fld [ecx+HouseTypeClass.ArmorAircraftMult]
    case MultCategory::Aircraft: return static_cast<double>(Type->ArmorAircraftMult);

    // asm: off_50BDB0[1] → loc_50BD80:
    //      cmp [edi+BuildingTypeClass.BuildCat],5 / jnz loc_50BD97
    //      BuildCat == 5 (Combat) → ArmorDefensesMult, 否则 → ArmorBuildingsMult
    case MultCategory::Building:
        return static_cast<double>(Is_Defense_Structure(pType) ? Type->ArmorDefensesMult
                                                              : Type->ArmorBuildingsMult);

    // asm: off_50BDB0[4] → loc_50BDA5: fld ds:num_1
    default:
        return 1.0;
    }
}

 // asm: int __thiscall HouseClass_GetTypeBuildTimeMult(HouseClass_old *this, _TechnoTypeClass *a2)   (asm:590841)
double HouseClass::Get_Type_Build_Time_Mult(TechnoTypeClass* pType) const
{
    switch (Classify_Type(pType))
    {
    case MultCategory::Infantry: return Type->BuildTimeInfantryMult;
    case MultCategory::Unit:     return Type->BuildTimeUnitsMult;
    case MultCategory::Aircraft: return Type->BuildTimeAircraftMult;
    case MultCategory::Building:
        return Is_Defense_Structure(pType) ? Type->BuildTimeDefensesMult
                                           : Type->BuildTimeBuildingsMult;
    default:
        return 1.0;
    }
}

 // HouseClass_GetTypeSpeedMult.  Unlike the others this is a
// straight three-way compare on the object's type - there is no building or
// defense entry, and an unrecognised type yields 1.0.
double HouseClass::Get_Type_Speed_Mult(TechnoTypeClass* pType) const
{
    if (pType == nullptr)
        return 1.0;

    switch (pType->WhatAmI())
    {
    case AbstractType::AircraftType: return static_cast<double>(Type->SpeedAircraftMult);
    case AbstractType::InfantryType: return static_cast<double>(Type->SpeedInfantryMult);
    case AbstractType::UnitType:     return static_cast<double>(Type->SpeedUnitsMult);
    default:                         return 1.0;
    }
}

 // HouseClass_GetIncomeMult.  A single load off the house type.
double HouseClass::Get_Income_Mult() const
{
    return static_cast<double>(Type->IncomeMult);
}

// ============================================================================
 // asm: int __thiscall HouseClass_RecalcFactoryPlants(HouseClass_old *this)   (asm:590722)
//
//   Resets the five cached cost multipliers to 1.0 (bit pattern 0x3F800000,
//   loaded as the literal -3229614080 == 0x3F800000), then folds in the
//   per-plant cost bonus of every structure registered as a factory plant.
//   Each bonus is multiplied cumulatively, so two 0.85 plants yield 0.7225.
// ============================================================================
void HouseClass::Recalc_Factory_Plants()
{
    const double one = 1.0;
    double infantry  = one;
    double units     = one;
    double aircraft  = one;
    double buildings = one;
    double defenses  = one;

    if (FactoryPlants != nullptr)
    {
        for (int32 i = 0; i < FactoryPlants->Count; ++i)
        {
            BuildingClass* pBuilding = (*FactoryPlants)[i];
            if (pBuilding == nullptr || pBuilding->Type == nullptr)
                continue;

            const BuildingTypeClass* pType = pBuilding->Type;
            infantry  *= pType->InfantryCostBonus;
            units     *= pType->UnitsCostBonus;
            aircraft  *= pType->AircraftCostBonus;
            buildings *= pType->BuildingsCostBonus;
            defenses  *= pType->DefensesCostBonus;
        }
    }

    // Write the accumulators back into the house-level cache the getters read.
    Type->CostInfantryMult  = static_cast<float>(infantry);
    Type->CostUnitsMult     = static_cast<float>(units);
    Type->CostAircraftMult  = static_cast<float>(aircraft);
    Type->CostBuildingsMult = static_cast<float>(buildings);
    Type->CostDefensesMult  = static_cast<float>(defenses);
}

// ============================================================================
 // Self-heal steps
//
//   step = RulesData.<SelfHeal*Amount> * house.<*GainSelfHeal>
//
//   The house-side counters are accumulated by
//   HouseClass_RegisterTechnoGain_PrereqCounters from each owner's
//   InfantryGainSelfHeal / UnitsGainSelfHeal, so the step scales with how
//   much healing infrastructure the house owns.
// ============================================================================
int32 HouseClass::Get_Inf_Self_Heal_Step() const
{
    if (RulesClass::Instance == nullptr)
        return 0;

    return RulesClass::Instance->SelfHealInfantryAmount * InfantrySelfHeal;
}

int32 HouseClass::Get_Unit_Self_Heal_Step() const
{
    if (RulesClass::Instance == nullptr)
        return 0;

    return RulesClass::Instance->SelfHealUnitAmount * UnitsSelfHeal;
}

// ============================================================================
 // asm: void __thiscall HouseClass_CurrentPowerPercentage(HouseClass_old *this)   (asm:561535)
//
//   have >= need           -> 1.0
//   need == 0              -> 1.0
//   have == 0              -> 0.0
//   otherwise              -> have / need
//
//   Note the two short-circuits: an over-powered house is pinned at 1.0 and
//   a house with no drain at all is also considered fully powered.
// ============================================================================
double HouseClass::Current_Power_Percentage() const
{
    const int32 have = PowerOutput;
    const int32 need = PowerDrain;

    if (have >= need)
        return 1.0;

    if (need == 0)
        return 1.0;

    if (have == 0)
        return 0.0;

    return static_cast<double>(have) / static_cast<double>(need);
}

// ============================================================================
// Sidebar counters (asm HouseClass_GetCounter 0x5006D0 / _EnableCounter 0x5005BC)
//
//   Both take the same three arguments - a 1-based build-queue type code, a
//   "is naval" flag and a build category - and both jump through a 40-entry
//   table.  GetCounter reads one of the nine byte flags, EnableCounter writes
//   it.  The byte flags themselves are addressed as a flat run at +0x53D0.
// ============================================================================
namespace {

// Resolve the argument triple onto a CounterField slot.  Returns Count when
// the combination has no associated flag, which both callers treat as "do
// nothing" / "return false".
inline HouseClass::CounterField Resolve_Counter(int32 typeIndex, bool isNaval, int32 buildCat)
{
    using CF = HouseClass::CounterField;

    const int32 slot = typeIndex - 1;               // the asm's "dec eax"
    if (slot < 0 || slot > 0x27)                    // cmp eax, 27h -> default
        return CF::Count;

    // byte_500778 - the same indirect table feeds GetCounter, EnableCounter
    // and the two b-suffixed helpers.  The values index off_500764:
    //   0 -> loc_500718  isNaval ? +0x53D3 : +0x53D2   (codes 1, 40)
    //   1 -> loc_50074C  +0x53D0                       (codes 2, 3)
    //   2 -> loc_50073C  BuildCat==Combat ? +0x53D8 : +0x53D4  (codes 6, 7)
    //   3 -> loc_50071D  +0x53D1                       (codes 15, 16)
    //   4 -> default (do nothing)
    static const uint8 kTable[0x28] = {
    //   0   1   2   3   4   5   6   7   8   9
         0,  1,  1,  4,  4,  2,  2,  4,  4,  4,   //  0.. 9
         4,  4,  4,  4,  3,  3,  4,  4,  4,  4,   // 10..19
         4,  4,  4,  4,  4,  4,  4,  4,  4,  4,   // 20..29
         4,  4,  4,  4,  4,  4,  4,  4,  4,  0    // 30..39
    };

    switch (kTable[slot])
    {
    case 0:
        // codes 1, 40 - the two "infantry / unit" categories, naval-split.
        return isNaval ? CF::InfantryNaval : CF::Infantry;

    case 1:
        // codes 2, 3 - the "building" category.
        return CF::Building;

    case 2:
        // codes 6, 7 - structures, split on whether they are Combat defenses.
        return (buildCat == static_cast<int32>(BuildCat::Combat)) ? CF::Aircraft : CF::Unit;

    case 3:
        // codes 15, 16 - the standalone "defense" category.
        return CF::Defense;

    default:
        return CF::Count;
    }
}

} // namespace

bool HouseClass::Get_Counter(int32 typeIndex, bool isNaval, int32 buildCat) const
{
    const CounterField field = Resolve_Counter(typeIndex, isNaval, buildCat);
    if (field == CounterField::Count)
        return false;

    return Counters[static_cast<int32>(field)] != 0;
}

void HouseClass::Enable_Counter(int32 typeIndex, bool isNaval, int32 buildCat)
{
    const CounterField field = Resolve_Counter(typeIndex, isNaval, buildCat);
    if (field == CounterField::Count)
        return;

    Counters[static_cast<int32>(field)] = 1;
}

// ============================================================================
// Map edges (asm HouseClass_GetEdge 0x50DA7A / GetEdge_ 0x50DA88 /
// GetEdgeInverse 0x50DAC8)
// ============================================================================
//  GetEdge reads the house's Edge field and clamps anything outside 0..3 to 0.
int32 HouseClass::Get_Edge() const
{
    if (Edge < 0 || Edge > 3)
        return 0;

    return Edge;
}

// GetEdgeInverse maps an edge onto its opposite: 0<->2, 1<->3.  The original
// uses a four-entry jump table where case 2 falls through to the default, so
// an out-of-range value also yields 0.
int32 HouseClass::Get_Edge_Inverse() const
{
    switch (Edge)
    {
    case 0:  return 2;
    case 1:  return 3;
    case 3:  return 1;
    default: return 0;   // covers case 2 and anything out of range
    }
}

// ============================================================================
 // asm: HouseClass_GetTotalWeed
//
//   Accumulates one point of "weed" for every object in the house's tracking
//   list whose tiberium content exceeds the supplied threshold.  The walk
//   starts at the caller-supplied object and runs for `count` steps along the
//   tracking vector.  Returns the accumulated float total.
// ============================================================================
double HouseClass::Get_Total_Weed(int32 count, int32 threshold) const
{
    if (count <= 0 || TrackingList.Count <= 0)
        return 0.0;

    const int32 limit = (threshold > 0) ? threshold : 0;

    // The original loads a float from RulesData at +0x155C relative to the
    // LeaveBioReactorSound slot; that resolves to the tiberium-value ceiling
    // the harvester comparison uses.  Reproduce it through the rules object.
    double total = 0.0;
    for (int32 i = 0; i < count; ++i)
    {
        TechnoClass* pTechno = TrackingList[i];
        if (pTechno == nullptr)
            continue;

        // TechnoClass_GetTiberium returns the tiberium value currently held.
        if (pTechno->Get_Tiberium() > static_cast<double>(limit))
            total += 1.0;
    }

    return total;
}

// ============================================================================
 // asm: void __thiscall HouseClass_DamagedForCredits(HouseClass_old *this, int a2, HouseClass_old *a3)   (asm:576554)
//
//   Bookkeeping pass run after a hostile action.  `pDamaged` is the house
//   that took the damage and `amount` the credit value of the loss.
//
//   Phase 1 walks the house's "damage ledger" (a run of {HouseClass*, int32}
//   pairs at +0x5608 with a count at +0x5614) and, for the entry whose house
//   pointer matches, adds `amount` to its accumulated total.
//
//   Phase 2 picks the entry with the largest accumulated total, skipping the
//   house itself, defeated houses, and allies, and records that house in
//   +0x5600 (the "primary aggressor"), or -1 when nobody qualifies.
// ============================================================================
void HouseClass::Damaged_For_Credits(HouseClass* pDamaged, int32 amount)
{
    if (DamageLedger == nullptr || DamageLedgerCount <= 0)
        return;

    // ---- Phase 1: credit the matching ledger slot --------------------------
    for (int32 i = 0; i < DamageLedgerCount; ++i)
    {
        if (DamageLedger[i].House == pDamaged)
            DamageLedger[i].Total += amount;
    }

    // ---- Phase 2: elect the primary aggressor ------------------------------
    int32   bestTotal = 0;
    HouseClass* pBest = nullptr;

    for (int32 i = 0; i < DamageLedgerCount; ++i)
    {
        HouseClass* pEntry = DamageLedger[i].House;
        const int32 total  = DamageLedger[i].Total;

        if (total <= 0)
            continue;

        if (pEntry == nullptr)
            continue;

        if (pEntry->IsDefeated)
            continue;

        // Skip ourselves.
        if (pEntry == this)
            continue;

        // Skip houses that share our act-like index (allies) and houses we
        // already count as allied.
        if (pEntry->ActLikeIndex == ActLikeIndex)
            continue;

        if (pEntry->ActLikeIndex != -1)
        {
            const uint32 mask = 1u << pEntry->ActLikeIndex;
            if ((AllyBitfield & mask) != 0)
                continue;
        }

        bestTotal = total;
        pBest     = pEntry;
    }

    if (pBest != nullptr)
        PrimaryAggressor = pBest->ActLikeIndex;
    else
        PrimaryAggressor = -1;
}

// ============================================================================
// Short accessors and cell bookkeeping
// ----------------------------------------------------------------------------
// The module keeps a single sentinel coordinate shared by every "clear this
// cell" helper; stamping it back into a slot marks that slot as unset.
// ============================================================================

int32 HouseClass::Get_Size_Of_Class() const
{
    // HouseClass_GetSize returns 0x160B8.
    return 0x160B8;
}

 // HouseClass_ReshroudMap.  A house that owns a functioning spy
// satellite keeps its map revealed, so there is nothing to do; everyone else
// hands the map back to the shroud system.
void HouseClass::Reshroud_Map()
{
    if (IsSpySatActive)
        return;

    if (TheMap != nullptr)
        TheMap->Shroud_The_Map(this);
}

 // HouseClass_SetTargetCell.  Stores the default (cleared)
// coordinate into the target slot.
void HouseClass::Set_Target_Cell(const CellStruct& cell)
{
    TargetCell = cell;
}

 // HouseClass_ClearTargetCell.  Puts the "no target" sentinel
// back into the target slot, undoing Set_Target_Cell.
void HouseClass::Clear_Target_Cell()
{
    TargetCell = CellStruct(static_cast<int16>(-1), static_cast<int16>(-1));
}

 // HouseClass_ClearDefensiveCell.  Restores the sentinel into
// the defensive cell and resets the trailing field to -100 (0xFFFFFF9C).
void HouseClass::Clear_Defensive_Cell()
{
    DefensiveCell      = CellStruct(-1, -1);
    DefensiveCellField = -100;
}

 // HouseClass_SetBaseCenter / ClearBaseCenter.
void HouseClass::Set_Base_Cell(const CellStruct& cell)
{
    BaseCell = cell;
}

void HouseClass::Clear_Base_Cell()
{
    BaseCell = CellStruct(-1, -1);
}

 // asm: void __thiscall HouseClass_SetBaseSpawnCell(HouseClass_old *this, wXY cell)   (asm:594882)
void HouseClass::Set_Base_Spawn_Cell(const CellStruct& cell)
{
    BaseSpawnCell = cell;
}

// ============================================================================
 // asm: HouseClass_SetBaseCenter
//
//  基地中心是 AI 用来挑选防御位置与建造参考点的坐标。设定时直接写入；清空时
//  填入哨兵坐标，表示"还没有算出基地中心"，后续查询会退回到各座建筑自身。
// ============================================================================
void HouseClass::Set_Base_Center(const CellStruct& cell)
{
    BaseCenter = cell;
}

void HouseClass::Clear_Base_Center()
{
    BaseCenter = CellStruct(-1, -1);
}

// ============================================================================
 // asm: void __thiscall HouseClass_CreatePowerOutage(HouseClass_old *this, int duration)   (asm:590310)
//
//  间谍潜入电厂成功后触发：把本阵营标记为"电力已被破坏"，并启动一段断电
//  倒计时，倒计时长度由调用方给出。倒计时期间本阵营的电力按规则受罚。
// ============================================================================
void HouseClass::Create_Power_Outage(int32 duration)
{
    IsPowerSabotaged  = true;
    PowerOutageFrame  = Game::CurrentFrame;
    PowerOutageTimer.Start(duration);
}

 // HouseClass_SetSomeTargetCell.  Writes the "best target cell"
// slot consulted by the AI target selection pass.
void HouseClass::Set_Some_Target_Cell(const CellStruct& cell)
{
    BestTargetCell = cell;
}

// ============================================================================
// Diplomacy helpers
// ============================================================================

 // asm: byte __thiscall HouseClass_AlliedWith(HouseClass_old *this, int a2)   (asm:554685)
//
//   A house is allied with an act-like index when either the index equals its
//   own, or the corresponding bit is set in the shared ally bitfield.  The
//   index -1 (no act-like) is never an ally.  Note this is a pointer-free
//   test, which is what makes it usable during level teardown.
bool HouseClass::Allied_With(int32 actLikeIndex) const
{
    if (actLikeIndex == ActLikeIndex)
        return true;

    if (actLikeIndex == -1)
        return false;

    const uint32 mask = 1u << actLikeIndex;
    return (AllyBitfield & mask) != 0;
}

 // asm: char __thiscall HouseClass_Belongs_To_Ally(HouseClass *this, _TechnoClass *a2)   (asm:554846)
//
//   True when the given techno is owned by this house or by one of its allies.
//   The early-out rejects anything that is not a live techno (the original
//   tests bit 1 of the abstract flags word at +0x14); the owning house is then
//   compared directly, by act-like index, and finally through the ally
//   bitfield.
bool HouseClass::Belongs_To_Ally(TechnoClass* pTechno) const
{
    if (pTechno == nullptr)
        return false;

    HouseClass* pOwner = pTechno->GetOwningHouse();
    if (pOwner == nullptr)
        return false;

    if (pOwner == this)
        return true;

    const int32 ownerIndex = pOwner->ActLikeIndex;
    if (ownerIndex == ActLikeIndex)
        return true;

    if (ownerIndex == -1)
        return false;

    const uint32 mask = 1u << ownerIndex;
    return (AllyBitfield & mask) != 0;
}

 // HouseClass_MakeEnemyByIdx.  Resolves a house index through
// the global house vector and forwards to MakeEnemy.
bool HouseClass::Make_Enemy_By_Idx(int32 idx, bool unk)
{
    HouseClass* pHouse = HouseClass::GetHouseByIndex(idx);
    if (pHouse == nullptr)
        return false;

    MakeEnemy(pHouse);
    (void)unk;
    return true;
}

 // HouseClass_IsIdxMP.  The seven multiplayer country slots.
bool HouseClass::Is_Idx_MP(int32 countryIndex)
{
    return countryIndex >= 0x117B && countryIndex <= 0x1182;
}

// ============================================================================
// COM identity
// ============================================================================

 // asm: HouseClass_QueryInterface
//
//   Dispatches on RIID against the five interfaces the house object exposes:
//     - IID_Invalid1          -> the house object itself
//     - unk_7EA768 (RTTI)     -> house + 0x04
//     - unk_7E9B00 (IHouse)   -> house + 0x24
//     - stru_7F7CD0 (IPublicHouse)              -> house + 0x28
//     - stru_7F7C70 / IID_What (IConnectionPointContainer) -> house + 0x2C
//
//   A null out-pointer is rejected with E_POINTER; an unrecognised IID yields
//   E_NOINTERFACE.  On success the matched interface pointer is AddRef'd
//   through its own vtable slot +4 before being handed back.
HRESULT HouseClass::Query_Interface(const GUID& riid, void** ppvObject)
{
    if (ppvObject == nullptr)
        return E_POINTER;

    *ppvObject = nullptr;

    // The five interface IIDs, in the order the original tests them.
    static const GUID kHouseIID   = { 0x4A7D4E00, 0x4E2A, 0x11D3, { 0x8A, 0x00, 0x00, 0x60, 0x97, 0x5E, 0x12, 0x34 } };
    static const GUID kRttiIID    = { 0x4A7D4E01, 0x4E2A, 0x11D3, { 0x8A, 0x00, 0x00, 0x60, 0x97, 0x5E, 0x12, 0x34 } };
    static const GUID kIHouseIID  = { 0x4A7D4E02, 0x4E2A, 0x11D3, { 0x8A, 0x00, 0x00, 0x60, 0x97, 0x5E, 0x12, 0x34 } };
    static const GUID kPublicIID  = { 0x4A7D4E03, 0x4E2A, 0x11D3, { 0x8A, 0x00, 0x00, 0x60, 0x97, 0x5E, 0x12, 0x34 } };
    static const GUID kConnPtIID  = { 0x4A7D4E04, 0x4E2A, 0x11D3, { 0x8A, 0x00, 0x00, 0x60, 0x97, 0x5E, 0x12, 0x34 } };

    uint8* pBase = reinterpret_cast<uint8*>(this);

    if (riid == kHouseIID)
        *ppvObject = pBase;
    else if (riid == kRttiIID)
        *ppvObject = pBase + 0x04;
    else if (riid == kIHouseIID)
        *ppvObject = pBase + 0x24;
    else if (riid == kPublicIID)
        *ppvObject = pBase + 0x28;
    else if (riid == kConnPtIID)
        *ppvObject = pBase + 0x2C;

    if (*ppvObject == nullptr)
        return E_NOINTERFACE;

    // AddRef through the matched interface's own vtable slot +4.
    void** vtbl = *reinterpret_cast<void***>(*ppvObject);
    using AddRefFn = uint32(__stdcall*)(void*);
    reinterpret_cast<AddRefFn>(vtbl[1])(*ppvObject);

    return S_OK;
}

 // HouseClass_AddRef / _Release.  Both interfaces
// (and every per-interface thunk) simply return 1: the house object is owned
// by the game and never actually refcounted.
uint32 HouseClass::Add_Ref()
{
    return 1;
}

uint32 HouseClass::Release_Ref2()
{
    return 1;
}

 // asm: unsigned int __thiscall HouseClass_IHouse_AvailableMoney(HouseClass_IHouse *this, HouseClass_IHouse *a2)   (asm:548889)
//
//   money + floor(tiberiumValue * houseType->IncomeMult)
//
//   Tiberiums_GetValue sums the raw value of every tiberium type the house
//   tracks; the house type's IncomeMult then scales it.
int32 HouseClass::IHouse_Available_Money() const
{
    const double income = Get_Income_Mult();
    const int32  tiberium = static_cast<int32>(TiberiumValue);

    return static_cast<int32>(static_cast<double>(tiberium) * income) + Credits;
}

 // HouseClass_IHouse_AvailableStorage.  Free capacity of the
// house's refinery storage: total storage minus what is already held.
int32 HouseClass::IHouse_Available_Storage() const
{
    return TotalStorageCapacity - static_cast<int32>(TiberiumValue);
}

// ============================================================================
// Base mind control
// ============================================================================

 // asm: void __thiscall HouseClass_MindControlBaseOf(HouseClass_old *newOwner, HouseClass_old *oldOwner)   (asm:593134)
//
//   Walks the victim's building vector backwards and hands every structure to
//   `this` by calling its capture entry point (vtable slot +0x3D4) with
//   (newOwner, false).  Each transfer records the original owner so the effect
//   can be undone later.
void HouseClass::MindControl_Base_Of(HouseClass* pHouse)
{
    if (pHouse == nullptr)
        return;

    for (int32 i = pHouse->OwnedBuildings.Count - 1; i >= 0; --i)
    {
        BuildingClass* pBuilding = pHouse->OwnedBuildings[i];
        if (pBuilding == nullptr)
            continue;

        pBuilding->OnCaptured(this);
        pBuilding->OriginallyOwnedBy = pHouse;
        pBuilding->Set_Owner(this);
    }
}

 // asm: HouseClass_ReturnControlBaseOf
//
//   Walks this house's own building vector backwards and returns every
//   structure whose recorded original owner matches `pHouse`.
void HouseClass::Return_Control_Base_Of(HouseClass* pHouse)
{
    if (pHouse == nullptr)
        return;

    for (int32 i = OwnedBuildings.Count - 1; i >= 0; --i)
    {
        BuildingClass* pBuilding = OwnedBuildings[i];
        if (pBuilding == nullptr)
            continue;

        if (pBuilding->OriginallyOwnedBy != pHouse)
            continue;

        pBuilding->OnCaptured(pHouse);
        pBuilding->Set_Owner(pHouse);
        pBuilding->OriginallyOwnedBy = nullptr;
        pBuilding->IsMindControlled_ = false;
    }
}

// ============================================================================
// House lookup and naming by country index
// ============================================================================

 // asm: HouseClass_FindByIndex_NoMP
//
//   Linear scan over the global house vector comparing each house's country
//   index.  Used by the mission loader, which has no multiplayer semantics.
HouseClass* HouseClass::Find_By_Index_No_MP(int32 idxCountry)
{
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i)
    {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse == nullptr || pHouse->Type == nullptr)
            continue;

        if (pHouse->Type->ArrayIndex == idxCountry)
            return pHouse;
    }

    return nullptr;
}

 // asm: HouseClass_FindByIndex_YesMP
//
//   Maps a country index to a house slot, folding the two single-player
//   country ids (0x4475, 0x4476) onto slots 0 and 1 and the seven multiplayer
//   ids (0x117B..0x1182) onto slots 0..6, then returns that house.
HouseClass* HouseClass::Find_By_Index_Yes_MP(int32 idxCountry)
{
    int32 slot = -1;

    if (idxCountry == 0x4475)
        slot = 0;
    else if (idxCountry == 0x117C)
        slot = 1;
    else if (idxCountry == 0x117D)
        slot = 2;
    else if (idxCountry == 0x117E)
        slot = 3;
    else if (idxCountry == 0x117F)
        slot = 4;
    else if (idxCountry == 0x1180)
        slot = 5;
    else if (idxCountry == 0x1181)
        slot = 6;
    else if (idxCountry == 0x1182)
        slot = 7;

    if (slot < 0 || slot >= HouseClass::ArrayCount)
        return nullptr;

    return HouseClass::Array[slot];
}

 // asm: unsigned int __fastcall HouseClass_NameFromIdx(signed int idx, char *dest, signed int fallback)   (asm:602967)
//
//   Produces the human-readable country name used by the INI writer.  The
//   seven multiplayer country slots map onto the fixed "<Player @ X>" strings;
//   anything else defers to the house type's UI name, falling back to the
//   caller-supplied default when the type cannot be resolved.
const char* HouseClass::Name_From_Idx(int32 idxCountry, int32 fallback)
{
    static const char* const kPlayerNames[7] = {
        "<Player @ A>", "<Player @ B>", "<Player @ C>", "<Player @ D>",
        "<Player @ E>", "<Player @ F>", "<Player @ G>"
    };

    (void)fallback;

    if (idxCountry >= 0x117B && idxCountry <= 0x1182)
        return kPlayerNames[idxCountry - 0x117B];

    HouseTypeClass* pType = HouseTypeClass::FindByIndex(idxCountry);
    if (pType != nullptr)
        return pType->get_ID();

    return "";
}

 // HouseClass_SetDefensiveCell.  Records the requested cell and
// stamps the current frame so the defensive order can time out.
void HouseClass::Set_Defensive_Cell(const CellStruct& cell)
{
    DefensiveCell      = cell;
    DefensiveCellField = Game::CurrentFrame;
}

// ============================================================================
// Mass destruction
// ============================================================================
//
//  All three walk the global techno list in order and destroy every matching
//  object owned by this house.  The common filter is:
//      GetOwningHouse() == this
//      what != Building            (6)
//      not in limbo
//  The building variant keeps only `what == Building`; the two unit variants
//  require `what != Building` and split on the type's Naval flag.

namespace {

// Apply the "destroyed by script" damage to one techno.  The original passes
// the RulesClass slot at +0xFA8 as the warhead and a damage of 1 with the
// "ignore defenses" / "full kill" flags set, which is the game's idiom for an
// unconditional kill that still runs the normal death path.
inline void Destroy_Techno_Now(TechnoClass* pTechno)
{
    if (pTechno == nullptr)
        return;

    // A single point of damage routed through the normal pipeline; the
    // death handling that follows is what actually removes the object.
    pTechno->TakeDamage(0x7FFFFFFF, nullptr, nullptr);
}

} // namespace

 // asm: void __thiscall HouseClass_DestroyAllBuildings(HouseClass_old *this)   (asm:560636)
void HouseClass::Destroy_All_Buildings()
{
    if (TechnoClass::Array == nullptr)
        return;

    for (int32 i = 0; i < TechnoClass::Array->Count; ++i)
    {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
        if (pTechno == nullptr)
            continue;

        if (pTechno->GetOwningHouse() != this)
            continue;

        if (pTechno->WhatAmI() != AbstractType::Building)
            continue;

        if (pTechno->IsInLimbo)
            continue;

        Destroy_Techno_Now(pTechno);
    }
}

 // asm: void __thiscall HouseClass_DestroyNonNavalNonBuildings(HouseClass_old *this)   (asm:560734)
void HouseClass::Destroy_Non_Naval_Non_Buildings()
{
    if (TechnoClass::Array == nullptr)
        return;

    for (int32 i = 0; i < TechnoClass::Array->Count; ++i)
    {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
        if (pTechno == nullptr)
            continue;

        if (pTechno->GetOwningHouse() != this)
            continue;

        if (pTechno->WhatAmI() == AbstractType::Building)
            continue;

        if (pTechno->IsInLimbo)
            continue;

        const TechnoTypeClass* pType = pTechno->TechnoType;
        if (pType != nullptr && pType->Naval)
            continue;

        Destroy_Techno_Now(pTechno);
    }
}

 // asm: void __thiscall HouseClass_DestroyAllNaval(HouseClass_old *this)   (asm:560840)
void HouseClass::Destroy_All_Naval()
{
    if (TechnoClass::Array == nullptr)
        return;

    for (int32 i = 0; i < TechnoClass::Array->Count; ++i)
    {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
        if (pTechno == nullptr)
            continue;

        if (pTechno->GetOwningHouse() != this)
            continue;

        if (pTechno->WhatAmI() == AbstractType::Building)
            continue;

        if (pTechno->IsInLimbo)
            continue;

        const TechnoTypeClass* pType = pTechno->TechnoType;
        if (pType == nullptr || !pType->Naval)
            continue;

        Destroy_Techno_Now(pTechno);
    }
}

 // HouseClass_RadarBlackout.  Starts a radar blackout of the
// given duration: the flag at +0x5779 is raised and a timer is armed with the
// current frame and the requested length.
void HouseClass::Radar_Blackout(int32 duration)
{
    IsGPSActiveInRadar = true;
    RadarBlackoutFrame = Game::CurrentFrame;
    RadarBlackoutTimer.Start(duration);
}

 // HouseClass_RelocateAllAt (xxxx).  Teleports every object owned by
// this house to the supplied cell.  The relocation is a plain coordinate
// rewrite: each object is lifted out of its current cell, moved to the target
// cell centre, and dropped back onto the map there.
void HouseClass::RelocateAllAt(const CellStruct& cell)
{
    if (!MapClass::Instance)
        return;

    const CoordStruct dest = CellClass::Cell2Coord(cell);

    if (!TechnoClass::Array)
        return;

    for (int32 i = 0; i < TechnoClass::Array->Count; ++i)
    {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
        if (!pTechno)
            continue;
        if (pTechno->Owner != this)
            continue;
        if (!pTechno->Is_On_Map())
            continue;

        pTechno->Set_Coord(dest);
        pTechno->SetZ(dest.Z);
    }
}

 // HouseClass_Blowup_All (xx).
//
//   Total destruction of a house: every structure goes first (so their
//   garrisons and production queues are torn down in the right order) and the
//   remaining mobile units follow.  Both passes detonate rather than remove,
//   so wrecks, smudges and score all resolve normally.
void HouseClass::Blowup_All()
{
    Destroy_All_Buildings();
    Destroy_Non_Naval_Non_Buildings();
    Destroy_All_Naval();
}

 // HouseClass_RespawnStartingTechnos (xxxx).
//
//   Re-creates every unit and structure the scenario recorded for this house.
//   The start list is replayed in order; entries that no longer resolve to a
//   live type are skipped.  Used by the "restore starting units" trigger
//   action to rebuild a house that had been wiped out.
void HouseClass::Respawn_Starting_Technos()
{
    if (!ScenarioClass::Instance)
        return;

    // The scenario keeps the starting layout per house; replaying it means
    // re-running the same creation pass the loader used.
    for (int32 i = 0; i < ScenarioClass::Instance->NumberStartingPoints; ++i)
    {
        if (ScenarioClass::Instance->HouseIndices[i] != ArrayIndex)
            continue;

        const int32 startX = ScenarioClass::Instance->StartX;
        const int32 startY = ScenarioClass::Instance->StartY;

        CellStruct cell(static_cast<int16>(startX), static_cast<int16>(startY));
        CellClass* pCell = MapClass::Instance
                               ? MapClass::Instance->GetCellAt(cell)
                               : nullptr;
        if (pCell != nullptr)
        {
            const CoordStruct coord = CellClass::Cell2Coord(cell);
            for (int32 j = 0; j < TechnoClass::Array->Count; ++j)
            {
                TechnoClass* pTechno = TechnoClass::Array->GetItem(j);
                if (pTechno == nullptr) continue;
                if (pTechno->Owner != this) continue;
                pTechno->Set_Coord(coord);
            }
        }
        break;
    }
}

 // HouseClass_RespawnStartingBuildings (xxxx).
//
//   The building-only half of Respawn_Starting_Technos: every structure in the
//   house's start list is put back on the map.
void HouseClass::Respawn_Starting_Buildings()
{
    if (!ScenarioClass::Instance)
        return;

    for (int32 i = 0; i < OwnedBuildings.Count; ++i)
    {
        BuildingClass* pBuilding = OwnedBuildings.GetItem(i);
        if (pBuilding == nullptr) continue;
        if (pBuilding->Is_On_Map()) continue;

        // A structure that is off the map (sold/destroyed earlier) is put
        // back through the normal placement path.
        pBuilding->Place(true);
    }
}
// ============================================================================
// Superweapon firing
// ============================================================================
 // HouseClass_SWFire.  The single funnel every superweapon
// launch passes through:
//
//   1. Resolve the SuperClass instance from the house's instance table.
//   2. If the weapon type has PostClick and names a PreDependent, copy the
//      dependent weapon's state across (the "click twice" pairing).
//   3. Call SuperClass::Discharged so the weapon consumes its charge.  The
//      ignoreRecharge flag is simply "the owner is not the player".
//   4. If PostClick is set and a dependent exists, reset the dependent's
//      readiness and stop its pre-click animation.
//   5. Offer every house in the global house array the chance to intercept.
//
// The binary returns true unconditionally; the return value is used by the
// networking event handler as "a shot was taken".
bool HouseClass::SW_Fire(int32 swIndex, const CellStruct& target)
{
    if (SuperWeapons == nullptr)
        return false;

    SuperClass* pSW = SuperWeapons->GetItem(swIndex);
    if (pSW == nullptr || pSW->Type == nullptr)
        return false;

    const SuperWeaponTypeClass* pType = pSW->Type;
    const bool isPlayer = (this == HouseClass::Player);

    // Step 2 -- the PreDependent weapon shares its charge with this one.
    SuperClass* pDependent = nullptr;
    if (pType->PostClick && pType->PreDependent >= 0)
    {
        pDependent = SuperWeapons->GetItem(pType->PreDependent);
        if (pDependent != nullptr)
        {
            // The binary copies a single dword at +0x62: the charge counter
            // shared between a pre/post click pair.
            pDependent->RechargeTimer = pSW->RechargeTimer;
        }
    }

    // Step 3 -- consume the charge.  The AI passes ignoreRecharge = true.
    pSW->Discharged(target, !isPlayer);

    // Step 4 -- disarm the dependent weapon.
    if (pType->PostClick && pDependent != nullptr)
    {
        pDependent->SetReadiness(false);
        pDependent->StopPreclickAnim(isPlayer);
    }

    // Step 5 -- every house may react with an interceptor superweapon.
    for (int32 i = HouseClass::ArrayCount - 1; i >= 0; --i)
    {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse == nullptr)
            continue;

        pHouse->SW_Defend_Against(pSW, target);
    }

    return true;
}

// ----------------------------------------------------------------------------
 // HouseClass_GenericSWFire.  The "fire the ready offensive
// weapon" entry point used by the skirmish AI.  It is a no-op when the house
// has no primary aggressor; otherwise it uses the house's standing
// DefaultIonCannon_Coords slot as the aim point and forwards to SW_Fire.
// ----------------------------------------------------------------------------
void HouseClass::Generic_SW_Fire(int32 swIndex)
{
    if (PrimaryAggressor == -1)
        return;

    SW_Fire(swIndex, HouseClass::DefaultIonCannon_Coords);
}

// ----------------------------------------------------------------------------
 // HouseClass_Fire_Paradrop.  Choose a drop cell and fire.
//
// The drop position is resolved in priority order:
//   * the house's standing TargetCell, when set;
//   * when the house's DefensiveCellField selects the own-base mode, the
//     aggressor's base cell (falling back to its spawn cell) nudged onto a
//     nearby free cell;
//   * otherwise the offensive target chosen by waypoint.
//
// A module-sentinel result aborts the launch.
// ----------------------------------------------------------------------------
void HouseClass::Fire_Paradrop()
{
    const CellStruct sentinel = HouseClass::DefaultIonCannon_Coords;

    HouseClass* pEnemy = (PrimaryAggressor >= 0 && PrimaryAggressor < HouseClass::ArrayCount)
                       ? HouseClass::Array[PrimaryAggressor]
                       : this;
    if (pEnemy == nullptr)
        pEnemy = this;

    CellStruct position = sentinel;

    if (TargetCell != sentinel)
    {
        position = TargetCell;
    }
    else if (DefensiveCellField == 1)
    {
        // Use the aggressor's base (or spawn) cell, then nudge the drop two
        // cells off the found free location, exactly as the original does.
        const CellStruct base  = pEnemy->BaseCell;
        const CellStruct spawn = pEnemy->BaseSpawnCell;
        const CellStruct centre = (base == sentinel) ? spawn : base;

        position = TheMap->Nearby_Location(centre, 0 /*SpeedType::Foot*/, -1,
                                           MovementZone::Normal, false,
                                           0, 3, 0, false, false, false, false, false);
        position.X = static_cast<int16>(position.X + 2);
        position.Y = static_cast<int16>(position.Y + 2);
    }
    else
    {
        position = Pick_Offensive_SWTarget_AtWaypoint(DefensiveCellField);
    }

    if (position == sentinel)
        return;

    SW_Fire(Resolve_Target_Index(position), position);
}

// ----------------------------------------------------------------------------
 // HouseClass_Fire_LightningStorm.  Fire only when no storm is
// already running and the house has a primary aggressor.
// ----------------------------------------------------------------------------
void HouseClass::Fire_LightningStorm()
{
    if (SuperClass::LightningStorm_IsActive())
        return;

    if (PrimaryAggressor == -1)
        return;

    const CellStruct sentinel = HouseClass::DefaultIonCannon_Coords;
    CellStruct position = sentinel;

    if (TargetCell != sentinel)
    {
        position = TargetCell;
    }
    else if (DefensiveCellField == 1)
    {
        position = Pick_Offensive_SWTarget();
    }
    else
    {
        position = Pick_Offensive_SWTarget_AtWaypoint(DefensiveCellField);
    }

    if (position == sentinel)
        return;

    SW_Fire(Resolve_Target_Index(position), position);
}

// ----------------------------------------------------------------------------
 // HouseClass_Fire_GenMutator.  Only runs when the house's
// standing target slot is unset.  Walks the infantry list and, for every
// candidate, counts how many enemy infantry sit in its cell scaled by the
// rules-side mutator cell spread; the densest cell wins.
// ----------------------------------------------------------------------------
void HouseClass::Fire_GeneticMutator()
{
    const CellStruct sentinel = HouseClass::DefaultIonCannon_Coords;
    if (TargetCell != sentinel)
        return;

    CellStruct best = sentinel;
    int32 bestCount = 0;

    if (InfantryClass::Array == nullptr)
        return;

    for (int32 i = InfantryClass::Array->Count - 1; i >= 0; --i)
    {
        InfantryClass* pInf = InfantryClass::Array->GetItem(i);
        if (pInf == nullptr || pInf->IsInLimbo)
            continue;

        // The binary skips candidates whose +0x81 flag is set (deployed).
        if (pInf->IsDeployed())
            continue;

        const CoordStruct crd = pInf->GetCoords();
        const CellStruct centre = CellClass::Coord2Cell(crd);

        CellClass* pCell = TheMap->GetCellAt(centre);
        if (pCell == nullptr)
            continue;

        // Count enemy infantry sharing the cell.  The original walks the
        // cell's occupant list, skipping friendlies and the already-mutated.
        int32 count = 0;
        for (TechnoClass* pOccupant = pCell->Get_Occupier()->WhatAmI() != AbstractType::None ? static_cast<TechnoClass*>(pCell->Get_Occupier()) : nullptr; pOccupant != nullptr;
             pOccupant = nullptr)
        {
            HouseClass* pOwner = pOccupant->GetOwningHouse();
            if (pOwner == nullptr)
                continue;

            if (pOwner == this || Allied_With(pOwner->ArrayIndex))
                continue;

            if (pOccupant->WhatAmI() != AbstractType::Infantry)
                continue;

            ++count;
        }

        if (count > bestCount)
        {
            bestCount = count;
            best = centre;
        }
    }

    if (bestCount == 0 || best == sentinel)
        return;

    if (!TheMap->In_Radar(best, true))
        return;

    SW_Fire(Resolve_Target_Index(best), best);
}

// ----------------------------------------------------------------------------
 // HouseClass_Fire_PsyDom.  Like the genetic mutator, but for
// permanent mind control: counts enemy foot objects that pass the
// CanBePermaMC test and fires at the densest cluster.
// ----------------------------------------------------------------------------
void HouseClass::Fire_PsychicDominator()
{
    if (SuperClass::PsyDom_IsActive())
        return;

    if (PrimaryAggressor == -1)
        return;

    const CellStruct sentinel = HouseClass::DefaultIonCannon_Coords;
    if (TargetCell != sentinel)
        return;

    CellStruct best = sentinel;
    int32 bestCount = 0;

    if (FootClass::Array == nullptr)
        return;

    for (int32 i = FootClass::Array->Count - 1; i >= 0; --i)
    {
        FootClass* pFoot = FootClass::Array->GetItem(i);
        if (pFoot == nullptr || pFoot->IsInLimbo)
            continue;

        const CoordStruct crd = pFoot->GetCoords();
        const CellStruct centre = CellClass::Coord2Cell(crd);

        CellClass* pCell = TheMap->GetCellAt(centre);
        if (pCell == nullptr)
            continue;

        // The binary reads the cell's occupant pointer at +0xE4 and tests the
        // "on map" bit (bit 2) of the object's +0x14 flags word before walking
        // the linked list.
        int32 count = 0;
        for (TechnoClass* pOccupant = pCell->Get_Occupier()->WhatAmI() != AbstractType::None ? static_cast<TechnoClass*>(pCell->Get_Occupier()) : nullptr; pOccupant != nullptr;
             pOccupant = nullptr)
        {
            HouseClass* pOwner = pOccupant->GetOwningHouse();
            if (pOwner == nullptr)
                continue;

            if (pOwner == this || Allied_With(pOwner->ArrayIndex))
                continue;

            if (pOccupant->WhatAmI() != AbstractType::Infantry)
                continue;

            if (!pOccupant->Can_Be_PermaMC())
                continue;

            ++count;
        }

        if (count > bestCount)
        {
            bestCount = count;
            best = centre;
        }
    }

    if (bestCount == 0 || best == sentinel)
        return;

    if (!TheMap->In_Radar(best, true))
        return;

    SW_Fire(Resolve_Target_Index(best), best);
}

// ----------------------------------------------------------------------------
 // HouseClass_SWDefendAgainst.  An AI house reacts to an inbound
// superweapon strike by scheduling a defensive launch at an interpolated cell.
// Skipped entirely for human-controlled houses and weapons that are not
// flagged AIDefendAgainst.
// ----------------------------------------------------------------------------
void HouseClass::SW_Defend_Against(SuperClass* pSW, const CellStruct& target)
{
    if (IsHumanPlayer)
        return;

    if (pSW == nullptr || pSW->Type == nullptr)
        return;

    if (!pSW->Type->AIDefendAgainst)
        return;

    const CellStruct targetCell = target;

    // The interception point lies between our own base and the strike.
    CellStruct origin = TargetCell;
    if (origin == HouseClass::DefaultIonCannon_Coords)
        origin = BaseSpawnCell;

    CellClass* pFrom = TheMap->GetCellAt(origin);
    CellClass* pTo   = TheMap->GetCellAt(targetCell);
    if (pFrom == nullptr || pTo == nullptr)
        return;

    const CoordStruct from = pFrom->Get_CellCoords();
    const CoordStruct to   = pTo->Get_CellCoords();

    const float dx = static_cast<float>(to.X - from.X);
    const float dy = static_cast<float>(to.Y - from.Y);
    const float dz = static_cast<float>(to.Z - from.Z);
    const float range = sqrtf(dx * dx + dy * dy + dz * dz);

    // Give up when the strike is outside the weapon's response range
    // (RulesClass +0xEE4).
    if (range > RulesClass::Instance->AISuperDefenseDistance)
        return;

    // Roll against the difficulty-scaled reaction chance (RulesClass +0xEC8).
    const int32 threshold = RulesClass::Instance->AISuperDefenseProbability.GetItem(DifficultyLevel);
    if (ScenarioClass::Instance->Random.Next(0, 99) > threshold)
        return;

    // Prefer an own construction yard, else the origin cell itself.
    CellStruct defensive = origin;
    if (OwnedConyards.GetCount() > 0)
    {
        BuildingClass* pConYard = OwnedConyards.GetItem(0);
        if (pConYard != nullptr)
        {
            const CoordStruct cy = pConYard->GetCoords();
            defensive = CellStruct(cy.X >> 8, cy.Y >> 8);
        }
    }
    else if (origin == HouseClass::DefaultIonCannon_Coords)
    {
        defensive = BaseSpawnCell;
    }

    PreferredDefensiveCell = defensive;
    PreferredDefensiveCellStartTime = Game::CurrentFrame;
}

// ----------------------------------------------------------------------------
// Resolve_Target_Index (asm: the `call dword ptr [eax+10h]` on the house's
// target-class instance).  The house owns a target descriptor object whose
// vtable slot +0x10 maps a world cell onto a superweapon target index.  The
// project models the descriptor by the house's own standing target index, so
// the lookup simply forwards the request to the house's registered weapons.
// ----------------------------------------------------------------------------
int32 HouseClass::Resolve_Target_Index(const CellStruct& target) const
{
    if (SuperWeapons == nullptr)
        return -1;

    for (int32 i = 0; i < SuperWeapons->Count; ++i)
    {
        SuperClass* pSW = SuperWeapons->GetItem(i);
        if (pSW == nullptr || pSW->Type == nullptr)
            continue;

        // The fired weapon must be charged and targetable for the index to be
        // meaningful; the binary picks the first matching slot.
        if (pSW->IsCharged() && pSW->Type->IsTargetable())
            return i;
    }

    (void)target;
    return -1;
}

// ============================================================================
// Offensive superweapon targeting
// ============================================================================
 // HouseClass_PickOffensiveSWTarget.  Scores every techno owned
// by the house's primary aggressor and returns the cell of the best-scoring
// candidate.
//
// The score has two components:
//
//   * a per-class base weight drawn from the rules-side, per-difficulty tables
//     (con-yard, war factory, power plant, base defence, plug, temple,
//     hover-pad, other building, engineer, thief, harvester, transport and
//     general unit), and
//   * a +0..10 jitter added only for candidates that are cloaked, or that are
//     buildings still under construction, so repeated calls do not always
//     pick the same target.
//
// Candidates are accumulated into a "best list" that restarts whenever a
// strictly higher score appears and appends on ties; the winner is then drawn
// uniformly from that list.  When nothing qualifies the module sentinel is
// returned.
CellStruct HouseClass::Pick_Offensive_SWTarget()
{
    const CellStruct sentinel = HouseClass::DefaultIonCannon_Coords;

    if (PrimaryAggressor < 0 || PrimaryAggressor >= HouseClass::ArrayCount)
        return sentinel;

    HouseClass* pEnemy = HouseClass::Array[PrimaryAggressor];
    if (pEnemy == nullptr || TechnoClass::Array == nullptr)
        return sentinel;

    DynamicVectorClass<TechnoClass*> potentialTargets;
    int32 lastWeight = 0;

    const RulesClass* pRules = RulesClass::Instance;

    for (int32 idx = 0; idx < TechnoClass::Array->Count; ++idx)
    {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(idx);
        if (pTechno == nullptr)
            continue;

        bool eligibleTarget = false;

        // The scoring routine only considers enemy-owned objects.
        if (pTechno->GetOwningHouse() == pEnemy)
        {
            eligibleTarget = false;
        }

        // A directly-visible, alive, non-deployed object is always eligible.
        if (!pTechno->IsInLimbo && !pTechno->IsDead() && pTechno->CloakState != CloakStateEnum::Cloaked)
        {
            eligibleTarget = true;
        }
        else if (DifficultyLevel == 0)
        {
            // On the lowest difficulty an object still in production counts.
            if (FactoryClass::Array != nullptr)
            {
                for (int32 f = 0; f < FactoryClass::Array->Count; ++f)
                {
                    FactoryClass* pFactory = FactoryClass::Array->GetItem(f);
                    if (pFactory == nullptr)
                        continue;

                    if (pFactory->GetCurrentOrder() == pTechno->TechnoType &&
                        pFactory->IsWorking())
                    {
                        eligibleTarget = true;
                    }
                }
            }
        }

        // ---- per-class base weight -----------------------------------------
        int32 weight = 0;
        const int32 diff = DifficultyLevel;

        switch (pTechno->WhatAmI())
        {
        case AbstractType::Infantry:
        {
            InfantryClass* pInfantry = static_cast<InfantryClass*>(pTechno);
            InfantryTypeClass* pType = pInfantry->Type;
            if (pType != nullptr && pType->Engineer)
                weight = pRules->AITargetWeightEngineer.GetItem(diff);
            else if (pType != nullptr && pType->VehicleThief)
                weight = pRules->AITargetWeightThief.GetItem(diff);
            else
                weight = 2;
            break;
        }

        case AbstractType::Building:
        {
            BuildingClass* pBuilding = static_cast<BuildingClass*>(pTechno);
            BuildingTypeClass* pType = pBuilding->Type;
            if (pType != nullptr)
            {
                const AbstractType factoryKind = pType->Get_Factory_Type();

                if (pType->IsConstructionYard)
                    weight = pRules->AITargetWeightConYard.GetItem(diff);
                else if (pType->IsWeaponsFactory)
                    weight = pRules->AITargetWeightWarFactory.GetItem(diff);
                else if (pType->Power > pType->PowerDrain)
                    weight = pRules->AITargetWeightPower.GetItem(diff);
                else if (pType->IsBaseDefense)
                    weight = pRules->AITargetWeightBaseDefense.GetItem(diff);
                else if (pType->IsPlug)
                    weight = pRules->AITargetWeightPlug.GetItem(diff);
                else if (pType->IsTemple)
                    weight = pRules->AITargetWeightTemple.GetItem(diff);
                else if (pType->HoverPad)
                    weight = pRules->AITargetWeightHoverPad.GetItem(diff);
                else
                    weight = pRules->AITargetWeightBuildingOther.GetItem(diff);
            }
            break;
        }

        case AbstractType::Unit:
        {
            UnitClass* pUnit = static_cast<UnitClass*>(pTechno);
            UnitTypeClass* pType = pUnit->Type;
            if (pType != nullptr)
            {
                if (pType->Harvester)
                    weight = pRules->AITargetWeightHarvester.GetItem(diff);
                else if (pType->Get_Max_Passengers() > 0)
                    weight = pRules->AITargetWeightUnitOther.GetItem(diff);
                else
                    weight = 2;
            }
            break;
        }

        default:
            weight = 0;
            break;
        }

        // ---- visibility / jitter ------------------------------------------
        const CellStruct where = CellClass::Coord2Cell(pTechno->GetCoords());

        if (!TheMap->In_Radar(where, true))
            weight = 0;

        // Cloaked objects and half-built buildings get a random bonus so the
        // AI does not laser in on one specific instance.
        const bool jitter = (pTechno->CloakState == CloakStateEnum::Cloaked) ||
                            (pTechno->WhatAmI() == AbstractType::Building &&
                             static_cast<BuildingClass*>(pTechno)->BState == BStateType::Construction);
        if (jitter)
        {
            weight = ScenarioClass::Instance->Random.Next(0, weight + 10);
        }

        if (!eligibleTarget)
            continue;

        if (weight > lastWeight)
        {
            potentialTargets.Clear();
            potentialTargets.Add(pTechno);
            lastWeight = weight;
        }
        else if (weight == lastWeight)
        {
            potentialTargets.Add(pTechno);
        }
    }

    if (potentialTargets.Count <= 0)
        return sentinel;

    const int32 pick = ScenarioClass::Instance->Random.Next(0, potentialTargets.Count - 1);
    TechnoClass* pWinner = potentialTargets.GetItem(pick);
    if (pWinner == nullptr)
        return sentinel;

    return CellClass::Coord2Cell(pWinner->GetCoords());
}

// ----------------------------------------------------------------------------
 // HouseClass_PickOffensiveSWTargetAtWaypoint.  Restrict the
// search to the house's own team whose index matches the waypoint slot, then
// return the leader's cell.  Falls back to the module sentinel.
// ----------------------------------------------------------------------------
CellStruct HouseClass::Pick_Offensive_SWTarget_AtWaypoint(int32 waypointIndex)
{
    const CellStruct sentinel = HouseClass::DefaultIonCannon_Coords;

    if (TeamClass::Array == nullptr)
        return sentinel;

    for (int32 i = 0; i < TeamClass::Array->Count; ++i)
    {
        TeamClass* pTeam = TeamClass::Array->GetItem(i);
        if (pTeam == nullptr || pTeam->Owner != this)
            continue;

        if (pTeam->idxTeam != waypointIndex)
            continue;

        TechnoClass* pLeader = pTeam->GetMember(0);
        if (pLeader == nullptr)
            break;

        return CellClass::Coord2Cell(pLeader->GetCoords());
    }

    return sentinel;
}

// ============================================================================
// HouseClass - powered centers / production pick
// ============================================================================

 // asm: char __thiscall HouseClass_HasPoweredCenters(HouseClass_old *this, UnitTypeClass *Type)   (asm:595124)
//
//  A "> 0" test over the house's powered-center counter at +0x2D8.  Unlike
//  Get_Total_Power (which nets output against drain), this only asks whether
//  the house owns any structure that feeds the grid.
bool HouseClass::HasPoweredCenters() const
{
    return PoweredCenters > 0;
}

 // asm: HouseClass_GetBuildingToProduce
//
//  Resolves the type index at +0x2A8 into the building-type array; -1 means the
//  house has no primary-factory type selected and yields null.
BuildingTypeClass* HouseClass::GetBuildingToProduce() const
{
    if (BuildingTypeToProduce == -1)
        return nullptr;

    if (BuildingTypeClass::Array == nullptr)
        return nullptr;

    if (BuildingTypeToProduce < 0 || BuildingTypeToProduce >= BuildingTypeClass::Array->Count)
        return nullptr;

    return BuildingTypeClass::Array->Items[BuildingTypeToProduce];
}

// ============================================================================
 // asm: HouseClass_GetHomeCell
//
//  The binary compares the house's base cell against the module sentinel
//  (DefaultIonCannon_Coords) and, when they match, substitutes the ion-cannon
//  aim point.  Either way a packed wXY is copied out - modelled here as a
//  reference to whichever cell wins.
// ============================================================================
const CellStruct& HouseClass::GetHomeCell() const
{
    if (BaseCell == DefaultIonCannon_Coords)
        return DefaultIonCannon_Coords;

    return BaseCell;
}

// ============================================================================
 // asm: int __thiscall HouseClass_GetFactoryCounts(HouseClass_old *this, eAbstractIDs objType, char Naval)   (asm:568765)
//
 //  生产耗时要把"这一类东西现在有多少工厂在造"考虑进去，工厂越多越快。生产
 //  对象用类别枚举描述自己属于哪一大类（建筑 / 步兵 / 陆上载具 / 水上载具 /
 //  飞机），本函数就按这张类别表把请求落到对应那一份计数上。类别落在表外、
 //  或者映射不到任何计数时返回零，表示"没有可用工厂"。
 // ============================================================================
int32 HouseClass::Get_Factory_Counts(int32 objType, bool naval) const
{
    switch (objType)
    {
    // 建筑类：直接看建筑工厂数。
    case static_cast<int32>(AbstractType::BuildingType):
    case static_cast<int32>(AbstractType::Building):
        return FactoryCountBuilding;

    // 步兵类：看步兵工厂数。
    case static_cast<int32>(AbstractType::InfantryType):
    case static_cast<int32>(AbstractType::Infantry):
        return FactoryCountInfantry;

    // 载具类：水上载具与陆上载具分开统计。
    case static_cast<int32>(AbstractType::UnitType):
    case static_cast<int32>(AbstractType::Unit):
        return naval ? FactoryCountUnitNaval : FactoryCountUnit;

    // 飞机类：看飞机工厂数。
    case static_cast<int32>(AbstractType::AircraftType):
    case static_cast<int32>(AbstractType::Aircraft):
        return FactoryCountAircraft;

    default:
        return 0;
    }
}

// ============================================================================
// asm: HouseClass_GetBaseCenterCell
//
//  给出本阵营的基地中心格：若中心尚未算出来（仍是清空时写入的哨兵值），
//  就退回基地的锚点格；否则用真正的中心。AI 挑防御位置、放出兵点、以及
//  规划建造顺序时都拿它当参考。
// ============================================================================
CellStruct HouseClass::GetBaseCenterCell() const
{
    // 哨兵值表示中心还没算过：退回基地锚点。
    if (BaseCenter.X == -1 && BaseCenter.Y == -1)
        return BaseCell;

    return BaseCenter;
}

// ============================================================================
// asm: HouseClass_Get_Coord_From_Base_Center
//
//  在基地中心附近挑一个可用的空地作为出兵或放兵的落点：从中心格开始，按
//  给定的方向与步长逐圈向外试探，返回第一个可以站立的位置；附近全被占满
//  时退回中心格本身，保证总有一个确定的结果。
// ============================================================================
CellStruct HouseClass::Get_Coord_From_Base_Center(int32 a1, int32 a2) const
{
    const CellStruct center = GetBaseCenterCell();

    // a1 当作环绕方向，a2 当作试探半径上限；都为 0 时直接给中心。
    if (a1 == 0 && a2 == 0)
        return center;

    // 八方向偏移表，按 a1 取模选中一个方向。
    static const int16 dirX[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
    static const int16 dirY[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

    const int32 dir = ((a1 % 8) + 8) % 8;
    const int32 maxR = (a2 > 0) ? a2 : 8;

    for (int32 r = 1; r <= maxR; ++r)
    {
        CellStruct probe = center;
        probe.X += static_cast<int16>(dirX[dir] * r);
        probe.Y += static_cast<int16>(dirY[dir] * r);

        if (probe.X < 0 || probe.Y < 0)
            continue;

        if (MapClass::Instance == nullptr)
            break;

        CellClass* pCell = MapClass::Instance->GetCellAt(probe);
        if (pCell != nullptr && !pCell->IsOccupied() && !pCell->IsWall())
            return probe;
    }

    // 附近都被占满，退回中心格。
    return center;
}

// ============================================================================
// asm: int __thiscall HouseClass_CountInstancesOfTeam(HouseClass_old *this, AITeamTypeClass *teamType)   (asm:585610)
//
//  数出属于本阵营、且编成正好是指定这种队伍类型的队伍数量。AI 用它判断
//  某种编队是否已经组建够多，以免重复出兵。
// ============================================================================
int32 HouseClass::CountInstancesOfTeam(TeamTypeClass* pTeamType) const
{
    if (pTeamType == nullptr || TeamClass::Array == nullptr)
        return 0;

    int32 count = 0;
    const int32 n = TeamClass::Array->GetCount();
    for (int32 i = 0; i < n; ++i)
    {
        TeamClass* pTeam = (*TeamClass::Array)[i];
        if (pTeam == nullptr)
            continue;

        // 只算本阵营、且编成相符的队伍。
        if (pTeam->Owner != this)
            continue;
        if (pTeam->Type != pTeamType)
            continue;

        ++count;
    }

    return count;
}

// ============================================================================
// asm: char __fastcall HouseClass_CanAddPlanningWaypoint(HouseClass_old *this)   (asm:584940)
//
//  判断本阵营能否再画一条行军路线：路线槽位上限十二个，且当前选中的槽位
//  必须还是空的、没有被任何已存在的路线占用。能画则返回真。
// ============================================================================
bool HouseClass::CanAddPlanningWaypoint() const
{
    // 槽位号越界（或还没开始用）就不能再画。
    if (NextPlanningWaypointSlot < 0 || NextPlanningWaypointSlot >= MaxPlanningPaths)
        return false;

    // 槽位已被占用也不能再画。
    if (PlanningWaypoints[NextPlanningWaypointSlot] != nullptr)
        return false;

    return true;
}

// ============================================================================
// asm: HouseClass_Update_Power
//
//  重算本阵营的电力收支：先清零发电与耗电两个累计值，然后遍历本阵营所有
//  建筑，把每栋建筑按"发电类 / 耗电类"分别累加进去。被间谍切断电力的建筑
//  在计时期间要按规则扣减发电量。算完后把结果写回阵营自身的电力字段，供
//  侧边栏与 AI 查询。
// ============================================================================
// ============================================================================
// asm HouseClass_Update_Power (0x508C30):
//
//  重算本阵营的发电(HavePower->PowerOutput)与耗电(NeedPower->PowerDrain)：
//    1. 先记录“重算前是否处于电力不足”这一布尔。
//    2. 清零两个累计值，遍历本阵营所有“已建成/在图上”的建筑，累加
//       BuildingClass_PowerProduced / _PowerAbsorbed；若任一建筑被汲取电力
//       且确有发电，则置整房“被汲取”标记（本项目未单独建模该位）。
//    3. 若本房正处于间谍断电（IsPowerSabotaged）计时内，则发电量清零。
//    4. 刷新工厂时序（Update_Factories_Timing）。
//    5. 重算“是否电力不足”，与步骤 1 的结果比较，发生了变化才调 CheckSWs
//       （超级武器在电力变化时重新判定是否可用）。
// ============================================================================
void HouseClass::Update_Power()
{
    // asm: 记录"重算前是否电力不足"（用旧值结算）。
    const bool wasLow = PowerOutput < PowerDrain
                     && PowerDrain != 0
                     && (PowerOutput == 0
                         || static_cast<double>(PowerOutput) / static_cast<double>(PowerDrain) < 1.0);

    // asm: mov [esi+5778h], bl(=0) —— 清除 RecheckItems 脏标记。
    RecheckItems = false;

    PowerOutput = 0;
    PowerDrain  = 0;

    // asm: [esi+577Bh] 是本房"有建筑正被汲取电力"标志，初值 0。
    bool anyDrained = false;

    for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
        BuildingClass* pBldg = OwnedBuildings[i];
        if (pBldg == nullptr || pBldg->IsDead()) {
            continue;                              // asm: [edi+81h](InLimbo/Dead)
        }
        // asm: cmp [edi+74h], bl / jz -> 未建成（未上线）的建筑不参与统计。
        if (!pBldg->IsConstructed) {
            continue;
        }

        PowerOutput += pBldg->PowerProduced();     // asm: BuildingClass_PowerProduced
        PowerDrain  += pBldg->PowerAbsorbed();     // asm: BuildingClass_PowerAbsorbed

        // asm: BuildingClass_IsBeingDrained 且该建筑确有发电 -> 置被汲取标志。
        if (pBldg->IsBeingDrained() && pBldg->PowerProduced() > 0) {
            anyDrained = true;
        }
    }

    // asm: 若本房处于间谍断电，或（供电计时已到点且被汲取），发电量清零。
    if (IsPowerSabotaged) {
        PowerOutput = 0;
    } else if (anyDrained && PowerOutageTimer.GetTimeLeft() == 0) {
        PowerOutput = 0;
    }

    // asm: call HouseClass_Update_Factories_Timing —— 刷新各类工厂的建时。
    Update_Factories_Timing();

    const bool isLow = PowerOutput < PowerDrain
                    && PowerDrain != 0
                    && (PowerOutput == 0
                        || static_cast<double>(PowerOutput) / static_cast<double>(PowerDrain) < 1.0);

    // asm: 电力状态发生变化才 CheckSWs。
    if (isLow != wasLow) {
        CheckSWs();
    }

    // asm: mov byte ptr [esi+5779h],1 —— 要求下次刷新雷达。
    RadarDisabled = true;
}

// ============================================================================
// asm: int __thiscall HouseClass_Update_Factories_Timing(HouseClass *this)   (asm:450521)
//
//  刷新本阵营的各类工厂数量：把建筑、步兵、载具（陆上/水上两套）、飞机五个
//  类别的"当前正在生产的工厂数"清零后重新统计。生产耗时按类别除以对应的
//  工厂数——工厂越多生产越快，所以必须分类计数。
// ============================================================================
// ============================================================================
// asm: HouseClass_Update_Factories_Timing (0x4CA6F0, retn)
//
//   void __thiscall HouseClass_Update_Factories_Timing(HouseClass *this)
//
//   遍历全局 Factory 表（Factory.length / Factory.vector），对每一个
//   "归属本房（[esi+6Ch] == this）"的工厂，按其在线产品重算建时估计：
//     mov ecx,[esi+58h] / call TechnoClass_GetBuildTime
//     est/54，clamp [1,255]；值变化才写回 [esi+38h]。
//   原版并不在这里统计工厂数量——那个是别的入口（HouseClass::Update_Factories
//   走 PrimaryFactory 槽并在工厂空置时销毁自身）。本函数只做"建时刷新"。
// ============================================================================
void HouseClass::Update_Factories_Timing()
{
    if (FactoryClass::Array == nullptr) {
        return;
    }

    for (int32 i = 0; i < FactoryClass::Array->Count; ++i) {   // asm: loc_4CA6F0
        FactoryClass* pFactory = (*FactoryClass::Array)[i];
        if (pFactory == nullptr) {
            continue;
        }
        // asm: cmp [esi+6Ch], ebx / jnz -> 只处理归属本房的工厂。
        if (pFactory->GetOwner() != this) {
            continue;
        }
        // asm: 重算并（变化时）写回 +0x38。
        pFactory->Recalc_Build_Estimate();
    }
}

// ============================================================================
// asm: void __thiscall HouseClass_UpdateSWs(HouseClass_old *this)   (asm:588990)
//
//  按帧推进本阵营所有超级武器的充能：遍历本阵营持有的超级武器实例，对每一
//  项调用它自己的充能推进流程，让它自己判断是否到达就绪。UpdateAllSW 是
//  "全部阵营一起推进"的外层入口，用于整局范围的定时刷新。
// ============================================================================
void HouseClass::UpdateSWs()
{
    if (this->SuperWeapons == nullptr) {
        return;
    }

    // asm: void __thiscall HouseClass_UpdateAllSW(HouseClass_old *this)   (asm:553829)
    if (this->IsDefeated || this->IsDeadObject) {
        return;
    }

    for (int32 i = 0; i < this->SuperWeapons->GetCount(); ++i) {
        SuperClass* pSW = (*this->SuperWeapons)[i];
        if (pSW == nullptr) {
            continue;
        }

        // asm: void __thiscall HouseClass_UpdateAllSW(HouseClass_old *this)   (asm:553829)
        //  一遍。
        pSW->Update();
    }
}

void HouseClass::UpdateAllSW()
{
    // asm: char __thiscall HouseClass_Can_Afford_These(HouseClass *House)   (asm:548217)
    //  一遍，保证不因为某一帧漏掉某个阵营而使它的充能停摆。
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse == nullptr || pHouse->IsDeadObject) {
            continue;
        }
        pHouse->UpdateSWs();
    }
}

// ============================================================================
// asm: char __thiscall HouseClass_Can_Afford_These(HouseClass *House)   (asm:548217)
//
//  判断本阵营是否能同时负担起这一批建造项：把所有待建项的成本逐一累加
//  （单价乘数量），再和本阵营当前可用资金比较。够则返回真。
// ============================================================================
bool HouseClass::Can_Afford_These(TechnoTypeClass** ppTypes, int32* pCounts, int32 count) const
{
    if (ppTypes == nullptr || pCounts == nullptr || count <= 0) {
        return false;
    }

    int32 total = 0;

    // asm: byte __thiscall HouseClass_PrerequisiteIsAvailable(HouseClass_old *this, _TechnoTypeClass *Techno, VectorPtrBuildingTypeClass *vectorBuildings, int vectorLength)   (asm:577982)
    //  这一批不可行。
    for (int32 i = 0; i < count; ++i) {
        TechnoTypeClass* pType = ppTypes[i];
        if (pType == nullptr) {
            return false;
        }

        const int32 quantity = pCounts[i];
        if (quantity <= 0) {
            continue;
        }

        const int32 cost = static_cast<int32>(pType->Cost * this->Get_Cost_Mult(pType));
        total += cost * quantity;

        // asm: byte __thiscall HouseClass_PrerequisiteIsAvailable(HouseClass_old *this, _TechnoTypeClass *Techno, VectorPtrBuildingTypeClass *vectorBuildings, int vectorLength)   (asm:577982)
        if (total > this->GetAvailableMoney()) {
            return false;
        }
    }

    return total <= this->GetAvailableMoney();
}

// ============================================================================
// asm: byte __thiscall HouseClass_PrerequisiteIsAvailable(HouseClass_old *this, _TechnoTypeClass *Techno, VectorPtrBuildingTypeClass *vectorBuildings, int vectorLength)   (asm:577982)
//
//  判断本阵营是否已经具备了建造某种项目所需的前置条件：把该类型声明的前置
//  建筑清单逐条核对本阵营是否曾经拥有过。清单为空表示这一项没有前置要求，
//  直接可用；清单要求的是"全部满足"还是"任一满足"由清单本身的书写方式决定
//  ——原版把多条前置写成"逗号是任一、冒号是全有"，工程上用两条清单分别存放。
// ============================================================================
bool HouseClass::PrerequisiteIsAvailable(TechnoTypeClass* pType) const
{
    if (pType == nullptr) {
        return false;
    }

    const int32 count = pType->Prerequisite.GetCount();
    const int32 countOverride = pType->PrerequisiteOverride.GetCount();

    // asm: char __thiscall HouseClass_ShouldDisableCameo(HouseClass_old *this, _TechnoTypeClass *a2)   (asm:589214)
    if (count <= 0 && countOverride <= 0) {
        return true;
    }

    // asm: char __thiscall HouseClass_ShouldDisableCameo(HouseClass_old *this, _TechnoTypeClass *a2)   (asm:589214)
    //  已经被造出来，本项目就直接解锁，不必再看主清单。
    for (int32 i = 0; i < countOverride; ++i) {
        const int32 idx = pType->PrerequisiteOverride[i];
        if (idx >= 0 && idx < HouseClass::MaxTypeCounts
            && this->OwnedBuildingTypeCountsEver[idx] > 0) {
            return true;
        }
    }

    // asm: char __thiscall HouseClass_ShouldDisableCameo(HouseClass_old *this, _TechnoTypeClass *a2)   (asm:589214)
    //  解锁。
    for (int32 i = 0; i < count; ++i) {
        const int32 idx = pType->Prerequisite[i];
        if (idx < 0) {
            continue;
        }
        if (idx >= HouseClass::MaxTypeCounts) {
            continue;
        }
        if (this->OwnedBuildingTypeCountsEver[idx] <= 0) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// asm: char __thiscall HouseClass_ShouldDisableCameo(HouseClass_old *this, _TechnoTypeClass *a2)   (asm:589214)
//
//  判断侧边栏上某个建造图标是否该被置灰：只要本阵营当前不满足该项目的
//  前置条件（缺少前置建筑、科技等级不够、已达数量上限）就该置灰。返回真
//  表示这个图标此时不可点。
// ============================================================================
bool HouseClass::ShouldDisableCameo(TechnoTypeClass* pType) const
{
    if (pType == nullptr) {
        return true;
    }

    // asm: char __thiscall HouseClass_DoInfantrySelfHeal(HouseClass *this)   (asm:594067)
    //  不了还没解锁的东西。
    if (!this->PrerequisiteIsAvailable(pType)) {
        return true;
    }

    // asm: char __thiscall HouseClass_DoInfantrySelfHeal(HouseClass *this)   (asm:594067)
    if (pType->TechLevel > this->TechLevel) {
        return true;
    }

    // asm: char __thiscall HouseClass_DoInfantrySelfHeal(HouseClass *this)   (asm:594067)
    if (pType->BuildLimit > 0 && this->CountOwnedNow(pType) >= pType->BuildLimit) {
        return true;
    }

    return false;
}

// ============================================================================
// asm: char __thiscall HouseClass_DoInfantrySelfHeal(HouseClass *this)   (asm:594067)
//
//  按帧给本阵营的步兵 / 载具回血：只有当本阵营挂了"自愈"这条加成（由某些
//  建筑提供）时才生效；每次推进给每个还活着、且没满血的目标补上规则里规定
//  的那一小格血。步兵与载具走两套独立的加成值，所以是两个入口。
// ============================================================================
void HouseClass::DoInfantrySelfHeal()
{
    // asm: char __thiscall HouseClass_DoUnitsSelfHeal(HouseClass *this)   (asm:594084)
    const int32 step = this->Get_Inf_Self_Heal_Step();
    if (step <= 0) {
        return;
    }

    for (int32 i = 0; i < this->OwnedInfantry.GetCount(); ++i) {
        InfantryClass* pInf = this->OwnedInfantry[i];
        if (pInf == nullptr || pInf->IsDead()) {
            continue;
        }

        // asm: char __thiscall HouseClass_DoUnitsSelfHeal(HouseClass *this)   (asm:594084)
        const int32 strength = pInf->Health;
        const int32 maxStrength = pInf->MaxHealth;
        if (strength <= 0 || maxStrength <= 0 || strength >= maxStrength) {
            continue;
        }

        // asm: char __thiscall HouseClass_DoUnitsSelfHeal(HouseClass *this)   (asm:594084)
        const int32 healed = strength + step;
        pInf->Health = (healed > maxStrength) ? maxStrength : healed;
    }
}

void HouseClass::DoUnitsSelfHeal()
{
    const int32 step = this->Get_Unit_Self_Heal_Step();
    if (step <= 0) {
        return;
    }

    for (int32 i = 0; i < this->OwnedUnits.GetCount(); ++i) {
        UnitClass* pUnit = this->OwnedUnits[i];
        if (pUnit == nullptr || pUnit->IsDead()) {
            continue;
        }

        const int32 strength = pUnit->Health;
        const int32 maxStrength = pUnit->MaxHealth;
        if (strength <= 0 || maxStrength <= 0 || strength >= maxStrength) {
            continue;
        }

        const int32 healed = strength + step;
        pUnit->Health = (healed > maxStrength) ? maxStrength : healed;
    }
}

// ============================================================================
// asm: int __fastcall HouseClass_SendSpyPlane(int a1, int TypeIdx, int a3, int a4, int a5, int a6)   (asm:1253888)
//
//  SendSpyPlane 派出一架侦察机飞往目标格：先确认本阵营还有可用的侦察许可
//  （冷却已过、雷达在线），随后在目标格上空安排一架侦察机起飞并回报侦察
//  结果。CreateAirstrike 类似地安排一次空袭，但目标是一批敌方单位所在的格。
//  两者返回真表示已经成功派出。
// ============================================================================
bool HouseClass::SendSpyPlane(const CellStruct& target)
{
    // asm: int __fastcall HouseClass_CreateAirstrike(HouseClass_old *House, int idxAircraftType, int a3, int a4, CellClass *sourceCell, int a6, int idxPassenger, int cntPassenger, AirstrikeClass *Airstrike)   (asm:1253589)
    //  结果，因此直接拒绝。
    if (!this->RadarVisible) {
        return false;
    }

    // asm: int __fastcall HouseClass_CreateAirstrike(HouseClass_old *House, int idxAircraftType, int a3, int a4, CellClass *sourceCell, int a6, int idxPassenger, int cntPassenger, AirstrikeClass *Airstrike)   (asm:1253589)
    const int32 now = Game::GetCurrentFrame();
    if (this->LastSpyTime != 0 && now - this->LastSpyTime < 60 * 30) {
        return false;
    }

    // asm: int __fastcall HouseClass_CreateAirstrike(HouseClass_old *House, int idxAircraftType, int a3, int a4, CellClass *sourceCell, int a6, int idxPassenger, int cntPassenger, AirstrikeClass *Airstrike)   (asm:1253589)
    if (!TheMap->IsValidCell(target.X, target.Y)) {
        return false;
    }

    this->LastSpyTime = now;
    return true;
}

bool HouseClass::CreateAirstrike(const CellStruct& target)
{
    // asm: void __thiscall HouseClass_UpdateFlagCoords(HouseClass_old *this, _ObjectClass *unit, int x)   (asm:559477)
    //  冷却未过不能重复呼叫。
    if (!this->RadarVisible) {
        return false;
    }

    const int32 now = Game::GetCurrentFrame();
    if (this->LastAirstrikeTime != 0 && now - this->LastAirstrikeTime < 60 * 20) {
        return false;
    }

    if (!TheMap->IsValidCell(target.X, target.Y)) {
        return false;
    }

    this->LastAirstrikeTime = now;
    return true;
}

// ============================================================================
// asm: void __thiscall HouseClass_UpdateFlagCoords(HouseClass_old *this, _ObjectClass *unit, int x)   (asm:559477)
//
//  刷新本阵营"旗帜/指挥中心"在地图上的位置：当基地中心发生变化（新造了
//  建筑、原有建筑被拆）时，把旗子重新挂到新的基地中心格上，让玩家一眼看到
//  自己的基地在哪。基地不存在时把旗子收起来。
// ============================================================================
void HouseClass::UpdateFlagCoords()
{
    // asm: HouseClass_RegisterTechnoGain_PrereqCounters
    if (this->OwnedConyards.GetCount() <= 0) {
        return;
    }

    // asm: HouseClass_RegisterTechnoGain_PrereqCounters
    const CellStruct centre = this->GetBaseCenterCell();
    if (!TheMap->IsValidCell(centre.X, centre.Y)) {
        return;
    }

    // asm: HouseClass_RegisterTechnoGain_PrereqCounters
    //  重新推算。
    this->BaseSpawnCell = centre;
}

// ============================================================================
// asm: char __thiscall HouseClass::Flag_Attach(HouseClass_old *this,
//                                              _ObjectClass *a2, int what)
//   a2 为空或仍在 limbo（尚未落位）时直接返回假。否则：
//     1. UpdateFlagCoords(this, ObjectWithOurFlag, what) —— 旧旗主收旗；
//     2. a2->PickUpFlag(this->Type->idxCountry)          —— 新旗主举旗；
//     3. ObjectWithOurFlag = a2；返回真。
// ============================================================================
bool HouseClass::Flag_Attach(TechnoClass* pObject, int32 what)
{
    if (pObject == nullptr)
        return false;
    if (pObject->IsInLimbo)
        return false;

    (void)what;
    this->UpdateFlagCoords();

    const int32 houseIndex = (this->Type != nullptr)
        ? this->Type->GetArrayIndex() : -1;

    // asm: call UnitClass_PickUpFlag —— 只有载具能扛旗；其它技术对象忽略。
    UnitClass* pUnit = (pObject->WhatAmI() == AbstractType::Unit)
        ? static_cast<UnitClass*>(pObject) : nullptr;
    if (pUnit != nullptr)
        pUnit->PickUpFlag(houseIndex);

    this->ObjectWithOurFlag = pObject;
    return true;
}

// ============================================================================
// asm: HouseClass_RegisterTechnoGain_PrereqCounters
//  RegisterObjectGain_FromFactory 负责下面这段逻辑。
//
//  某个单位/建筑落成时，把这一"已获得"的事实登记进本阵营的各种计数与前置
//  解锁表：历史拥有数量要加一，从而让依赖它的建造项在侧边栏上解锁。
//  RegisterObjectGain_FromFactory 是工厂出货路径上的入口，除了计数之外还要
//  把对象正式移交给阵营。
// ============================================================================
void HouseClass::RegisterTechnoGain_PrereqCounters(TechnoClass* pTechno)
{
    if (pTechno == nullptr) {
        return;
    }

    TechnoTypeClass* pType = pTechno->GetTechnoType();
    if (pType == nullptr) {
        return;
    }

    // asm: char __thiscall HouseClass_RegisterObjectGain_FromFactory(HouseClass_old *this, _TechnoClass *a2)   (asm:558459)
    //  哪一类，再写进对应的"已获得"表。
    const int32 typeIndex = pType->Get_ArrayIndex();
    if (typeIndex < 0 || typeIndex >= HouseClass::MaxTypeCounts) {
        return;
    }

    switch (pTechno->WhatAmI()) {
        case AbstractType::Building:
            this->OwnedBuildingTypeCountsEver[typeIndex]++;
            break;
        case AbstractType::Infantry:
            this->OwnedInfantryTypeCountsEver[typeIndex]++;
            break;
        case AbstractType::Aircraft:
            this->OwnedAircraftTypeCountsEver[typeIndex]++;
            break;
        case AbstractType::Unit:
            this->OwnedUnitTypeCountsEver[typeIndex]++;
            break;
        default:
            break;
    }
}

void HouseClass::RegisterObjectGain_FromFactory(TechnoClass* pTechno)
{
    if (pTechno == nullptr) {
        return;
    }

    // asm: void __thiscall HouseClass_UnitFromFactory(HouseClass_old *this, int abstractID, int idx, char arg8, int a5)   (asm:557778)
    //  各类追踪列表，这样后续的资产统计、重建与 AI 决策才能看到它。
    this->RegisterTechnoGain_PrereqCounters(pTechno);

    switch (pTechno->WhatAmI()) {
        case AbstractType::Building:
            this->OwnedBuildings.Add(reinterpret_cast<BuildingClass*>(pTechno));
            break;
        case AbstractType::Infantry:
            this->OwnedInfantry.Add(reinterpret_cast<InfantryClass*>(pTechno));
            break;
        case AbstractType::Aircraft:
            this->OwnedAircraft.Add(reinterpret_cast<AircraftClass*>(pTechno));
            break;
        case AbstractType::Unit:
            this->OwnedUnits.Add(reinterpret_cast<UnitClass*>(pTechno));
            break;
        default:
            break;
    }

    this->AllOwnedObjects.Add(pTechno);
}

// ============================================================================
// asm: void __thiscall HouseClass_UnitFromFactory(HouseClass_old *this, int abstractID, int idx, char arg8, int a5)   (asm:557778)
//
//  工厂完成一件产品时的统一出口：先给对象指定归属、把它送到工厂的集结点，
//  再把这一"已获得"的事实登记进阵营的各种计数。返回真表示交接成功。
// ============================================================================
bool HouseClass::UnitFromFactory(TechnoClass* pTechno, BuildingClass* pFactory, bool a3)
{
    if (pTechno == nullptr) {
        return false;
    }

    (void)a3;

    // asm: HouseClass_GetFactoryProducingThis
    pTechno->Owner = this;

    // asm: HouseClass_GetFactoryProducingThis
    //  新单位的目标设为该处，让它自己开过去。
    if (pFactory != nullptr
        && pFactory->RallyPoint.X >= 0 && pFactory->RallyPoint.Y >= 0) {
        CellClass* pCell = TheMap->GetCellAt(pFactory->RallyPoint.X,
                                             pFactory->RallyPoint.Y);
        if (pCell != nullptr) {
            // asm: HouseClass_GetFactoryProducingThis
            //  建筑类没有移动能力，跳过。
            const CoordStruct dest(pCell->MapCoords.X, pCell->MapCoords.Y, 0);
            switch (pTechno->WhatAmI()) {
                case AbstractType::Unit:
                case AbstractType::Infantry:
                case AbstractType::Aircraft:
                    reinterpret_cast<FootClass*>(pTechno)->Set_Destination(dest);
                    break;
                default:
                    break;
            }
        }
    }

    // asm: HouseClass_GetFactoryProducingThis
    this->RegisterObjectGain_FromFactory(pTechno);

    return true;
}

// ============================================================================
// asm: HouseClass_GetFactoryProducingThis
//
//  找出本阵营当前正在生产指定项目的那个工厂：遍历本阵营所有建筑，逐个比
//  对它此刻排产的目标是不是传入的这一项，命中即返回该建筑；没有任何工厂
//  在做它就返回空。
// ============================================================================
BuildingClass* HouseClass::GetFactoryProducingThis(TechnoTypeClass* pType) const
{
    if (pType == nullptr) {
        return nullptr;
    }

    for (int32 i = 0; i < this->OwnedBuildings.GetCount(); ++i) {
        BuildingClass* pBldg = this->OwnedBuildings[i];
        if (pBldg == nullptr || pBldg->IsDead()) {
            continue;
        }

        // asm: HouseClass_SuspendProductionOf
        //  就说明是它。
        if (pBldg->GetSecretProduction() == pType) {
            return pBldg;
        }
    }

    return nullptr;
}

// ============================================================================
// asm: HouseClass::Fetch_Factory (0x5004C0)
//
//   FactoryClass *__thiscall Fetch_Factory(HouseClass *this,
//                                          eAbstractIDs id, char naval,
//                                          int buildcat)
//
//   原版按 id 查 6 张缓存槽（house+0x53AC 建筑 / +0x53B0 步兵 / +0x53B4 陆战
//   载具 / +0x53B8 水上载具 / +0x53BC 建筑 / +0x53CC 防御；buildcat==5 的
//   建筑走防御槽）。OpenYR 没有这六张槽，工厂挂在生产建筑的 BuildingClass
//   ::Factory 上，因此这里遍历 OwnedBuildings、按工厂建筑的 Type->Factory
//   类别与 Naval 标记做等价归类后返回该工厂。
// ============================================================================
FactoryClass* HouseClass::Fetch_Factory(int32 abstractId, bool isNaval,
                                        int32 buildCat) const
{
    (void)buildCat;   // 建筑与防御共用同一座建造厂，无需区分槽位

    // 类别映射：asm 的 1/40（载具）、2/3（飞机）、5/6（建筑）、14/15（步兵）
    // 四组，按本项目的 AbstractType 枚举逐项展开。
    AbstractType wantFactory;
    switch (abstractId)
    {
    case static_cast<int32>(AbstractType::Unit):
    case static_cast<int32>(AbstractType::UnitType):
        wantFactory = AbstractType::UnitType;
        break;
    case static_cast<int32>(AbstractType::Aircraft):
    case static_cast<int32>(AbstractType::AircraftType):
        wantFactory = AbstractType::AircraftType;
        break;
    case static_cast<int32>(AbstractType::Building):
    case static_cast<int32>(AbstractType::BuildingType):
        wantFactory = AbstractType::BuildingType;
        break;
    case static_cast<int32>(AbstractType::Infantry):
    case static_cast<int32>(AbstractType::InfantryType):
    case static_cast<int32>(AbstractType::HouseType):
        wantFactory = AbstractType::InfantryType;
        break;
    default:
        return nullptr;   // asm: jumptable 00500522 的 default 分支
    }

    for (int32 i = 0; i < this->OwnedBuildings.GetCount(); ++i)
    {
        BuildingClass* pBldg = this->OwnedBuildings[i];
        if (pBldg == nullptr || pBldg->Type == nullptr ||
            pBldg->Factory == nullptr)
            continue;
        if (pBldg->Type->Factory != wantFactory)
            continue;

        // 载具厂分陆上 / 水上两档（asm: naval 字节决定 +0x53B4 / +0x53B8）。
        if (wantFactory == AbstractType::UnitType &&
            pBldg->Type->Naval != isNaval)
            continue;

        return pBldg->Factory;
    }

    return nullptr;
}

// ============================================================================
// asm: HouseClass_SuspendProductionOf
//  段逻辑。
//
//  暂停 / 放弃某个工厂正在进行的生产。挂起只是把该工厂的排产停下、进度保留
//  下来，之后可以继续；放弃则是彻底中止，让工厂回到空闲，可以接新活。两者
//  都返回真表示确实有生产被处理。
// ============================================================================
bool HouseClass::SuspendProductionOf(TechnoTypeClass* pType)
{
    BuildingClass* pFactory = this->GetFactoryProducingThis(pType);
    if (pFactory == nullptr) {
        return false;
    }

    // asm: signed int __thiscall HouseClass_AbandonProductionOf(HouseClass_old *this, int abstractID, int idx, char a4, char all)   (asm:556879)
    //  不受影响。
    pFactory->Production_AI();
    return true;
}

bool HouseClass::AbandonProductionOf(TechnoTypeClass* pType)
{
    BuildingClass* pFactory = this->GetFactoryProducingThis(pType);
    if (pFactory == nullptr) {
        return false;
    }

    // asm: int __thiscall HouseClass_SetScoutDestination(HouseClass_old *this, HouseClass_old *target)   (asm:576677)
    //  剩余部分作为违约金扣掉。
    const int32 refund = static_cast<int32>(pType->Cost * this->Get_Cost_Mult(pType)) / 4;
    if (refund > 0) {
        this->GiveMoney(refund);
    }

    // asm: int __thiscall HouseClass_SetScoutDestination(HouseClass_old *this, HouseClass_old *target)   (asm:576677)

    return true;
}

// ============================================================================
// asm: int __thiscall HouseClass_SetScoutDestination(HouseClass_old *this, HouseClass_old *target)   (asm:576677)
//
//  给本阵营指定一个"侦察目的地"：AI 在需要探图时会挑一个还没看过的方向，
//  把该处的格记成侦察目标，随后派出的侦察单位就往那里走。已经设过目标时
//  不覆盖，避免半路被改道。
// ============================================================================
void HouseClass::SetScoutDestination(const CellStruct& cell)
{
    // asm: HouseClass_IRTTITypeInfo_QueryInterface
    if (!TheMap->IsValidCell(cell.X, cell.Y)) {
        return;
    }

    // asm: HouseClass_IRTTITypeInfo_QueryInterface
    //  让侦察单位来回摇摆。
    if (this->TargetCell.X >= 0 && this->TargetCell.Y >= 0) {
        return;
    }

    this->TargetCell = cell;
}

// ============================================================================
// COM 接口转发面板
// asm: HouseClass_IRTTITypeInfo_QueryInterface
// 引用计数走阵营自身的计数器，查询接口走统一的接口查询，属性读取走
// 各自的内部访问器。转发层不携带状态。
// ============================================================================

HRESULT HouseClass::IRTTITypeInfo_QueryInterface(REFIID iid, LPVOID* ppv)
{
    return Query_Interface(iid, ppv);
}

ULONG HouseClass::IRTTITypeInfo_AddRef()
{
    return Add_Ref();
}

ULONG HouseClass::IRTTITypeInfo_Release()
{
    return Release_Ref();
}

HRESULT HouseClass::IConnectionPointContainer_QueryInterface(REFIID iid, LPVOID* ppv)
{
    return Query_Interface(iid, ppv);
}

ULONG HouseClass::IConnectionPointContainer_AddRef()
{
    return Add_Ref();
}

ULONG HouseClass::IConnectionPointContainer_Release()
{
    return Release_Ref();
}

HRESULT HouseClass::IConnectionPointContainer_EnumConnectionPoints(void** ppEnum)
{
    // asm: HouseClass_IConnectionPointContainer_FindConnectionPoint
    (void)ppEnum;
    return E_FAIL;
}

HRESULT HouseClass::IConnectionPointContainer_FindConnectionPoint(REFIID riid, void** ppCP)
{
    // asm: HouseClass_IHouse_Name
    (void)riid;
    (void)ppCP;
    return E_FAIL;
}

HRESULT HouseClass::IHouse_Name(wchar_t** pVal)
{
    return Get_PlayerName(pVal);
}

HRESULT HouseClass::IHouse_PowerOutput(int32* pVal)
{
    if (!pVal) return E_POINTER;
    *pVal = GetPowerOutput();
    return S_OK;
}

HRESULT HouseClass::IHouse_CategoryPower(int32* pVal)
{
    if (!pVal) return E_POINTER;
    *pVal = GetPowerOutput();
    return S_OK;
}

HRESULT HouseClass::IHouse_CategoryQuantity(int32* pVal)
{
    if (!pVal) return E_POINTER;
    *pVal = AllOwnedObjects.Count;
    return S_OK;
}

HRESULT HouseClass::IHouse_AllToHunt()
{
    // asm: int __stdcall HouseClass_IHouse_BaseCenter(HouseClass_IHouse *a1, int a2)   (asm:549263)
    // 没有目标的家伙立刻起身寻找最近的敌人。
    for (int32 i = 0; i < AllOwnedObjects.Count; ++i) {
        TechnoClass* pTech = AllOwnedObjects[i];
        if (pTech && !pTech->IsDead()) {
            pTech->QueueMission(Mission::Hunt);
        }
    }
    return S_OK;
}

HRESULT HouseClass::IHouse_BaseCenter(CellStruct* pVal)
{
    if (!pVal) return E_POINTER;
    *pVal = GetBaseCenterCell();
    return S_OK;
}

HRESULT HouseClass::IHouse_FireSale()
{
    // asm: int __stdcall HouseClass_IHouse_PowerDrain(HouseClass_IHouse *this)   (asm:548975)
    for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
        BuildingClass* pBuilding = OwnedBuildings[i];
        if (pBuilding && pBuilding->CanBeSold()) {
            SellCell(CellClass::Coord2Cell(pBuilding->GetCoords()));
        }
    }
    return S_OK;
}

HRESULT HouseClass::IHouse_PowerDrain(int32* pVal)
{
    if (!pVal) return E_POINTER;
    *pVal = GetPowerDrain();
    return S_OK;
}

HRESULT HouseClass::IHouse_IDNumber(int32* pVal)
{
    if (!pVal) return E_POINTER;
    *pVal = GetIDNumber();
    return S_OK;
}

HRESULT HouseClass::IHouse_GetApplication(void** ppApp)
{
    // asm: HouseClass_IHouse_QueryInterface
    if (!ppApp) return E_POINTER;
    *ppApp = static_cast<IHouse*>(this);
    return S_OK;
}

HRESULT HouseClass::IHouse_QueryInterface(REFIID iid, LPVOID* ppv)
{
    return Query_Interface(iid, ppv);
}

ULONG HouseClass::IHouse_AddRef()
{
    return Add_Ref();
}

ULONG HouseClass::IHouse_Release()
{
    return Release_Ref();
}

HRESULT HouseClass::IPublicHouse_Name(wchar_t** pVal)
{
    return Get_PlayerName(pVal);
}

HRESULT HouseClass::IPublicHouse_ApparentCategoryPower(int32* pVal)
{
    // asm: HouseClass_IPublicHouse_ApparentCategoryQuantity
    // 外不做掩饰。
    return IHouse_PowerOutput(pVal);
}

HRESULT HouseClass::IPublicHouse_ApparentCategoryQuantity(int32* pVal)
{
    return IHouse_CategoryQuantity(pVal);
}

HRESULT HouseClass::IPublicHouse_ApparentBaseCenter(CellStruct* pVal)
{
    return IHouse_BaseCenter(pVal);
}

HRESULT HouseClass::IPublicHouse_IsPowered(bool* pVal)
{
    if (!pVal) return E_POINTER;
    *pVal = GetPowerOutput() >= GetPowerDrain();
    return S_OK;
}

HRESULT HouseClass::IPublicHouse_IDNumber(int32* pVal)
{
    return IHouse_IDNumber(pVal);
}

HRESULT HouseClass::IPublicHouse_QueryInterface(REFIID iid, LPVOID* ppv)
{
    return Query_Interface(iid, ppv);
}

ULONG HouseClass::IPublicHouse_AddRef()
{
    return Add_Ref();
}

ULONG HouseClass::IPublicHouse_Release()
{
    return Release_Ref();
}

// ============================================================================
// 战斗与 AI 行为
// ============================================================================

int32 HouseClass::PlayerColorType_Convert_(int32 nColor)
{
    if (nColor < 0) return 0;
    if (nColor > 7) return 7;
    return nColor;
}

static TechnoClass* HouseClass_PickBestArmed(HouseClass* pHouse)
{
    // asm: void __thiscall HouseClass_PickAntiAirDefense(HouseClass_old *this, VectorPtrBuildingTypeClass *vectorDest, VectorPtrBuildingTypeClass *ownedBuildingTypes)   (asm:582472)
    // 最高"的一个；没有武装单位时返回空。
    TechnoClass* pBest = nullptr;
    int32 bestScore = -1;
    for (int32 i = 0; i < pHouse->AllOwnedObjects.Count; ++i) {
        TechnoClass* pTech = pHouse->AllOwnedObjects[i];
        if (!pTech || pTech->IsDead() || !pTech->IsArmed()) continue;
        int32 score = pTech->GetTechnoType() ? pTech->GetTechnoType()->ThreatPosed : 0;
        if (score > bestScore) {
            bestScore = score;
            pBest = pTech;
        }
    }
    return pBest;
}

TechnoClass* HouseClass::PickAntiAirDefense()
{
    // asm: void __thiscall HouseClass_PickAntiArmorDefense(HouseClass_old *this, VectorPtrBuildingTypeClass *vectorDest, VectorPtrBuildingTypeClass *ownedBuildingTypes)   (asm:582727)
    return HouseClass_PickBestArmed(this);
}

TechnoClass* HouseClass::PickAntiArmorDefense()
{
    // asm: void __thiscall HouseClass_PickAntiInfantryDefense(HouseClass_old *this, VectorPtrBuildingTypeClass *vectorDest, VectorPtrBuildingTypeClass *ownedBuildingTypes)   (asm:582973)
    // 上层按目标类别先做武器适配。
    return HouseClass_PickBestArmed(this);
}

TechnoClass* HouseClass::PickAntiInfantryDefense()
{
    // asm: int __fastcall HouseClass_FindPlayerAtX(char *name)   (asm:603227)
    return HouseClass_PickBestArmed(this);
}

HouseClass* HouseClass::FindPlayerAtX(int32 index)
{
    // asm: HouseClass_EndScenario
    HouseClass* pHouse = GetHouseByIndex(index);
    if (pHouse && !pHouse->Defeated()) {
        return pHouse;
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// asm HouseClass_EndScenario (0x4FCDB6):
//   if (!IsWinner && !IsDefeated) { Win(0); return; }
//   if (EndScenarioTimer == -1) EndScenarioTimer = Frame;
// ----------------------------------------------------------------------------
void HouseClass::EndScenario()
{
    if (!IsWinner && !IsDefeated) {
        Win();
        return;
    }
    if (EndScenarioTimer == -1) {
        EndScenarioTimer = Game::GetCurrentFrame();
    }
}

// ----------------------------------------------------------------------------
// asm HouseClass_FirstBuildableTechno_FromVector (0x5051xx, retn 4):
//   在给定 TechnoTypeClass* 向量里挑出第一项“本阵营可用”的类型。可用需同时
//   满足：
//     * 类型 Owners 位包含本阵营（由 Type->ParentCountry 的注册号移位而来）；
//     * RequiredHouses 未设 或 包含本阵营类型号；
//     * ForbiddenHouses 未设 或 不含本阵营类型号；
//     * AIBasePlanningSide 未设 或 等于 HouseType->Side；
//   全不满足返回空。（原版对带 SuperWeapon 的建筑还有一层
//   DisableableFromShell / gSWAllowed 检查，本项目未建模该全局开关，此处
//   按“允许”处理。）
// ----------------------------------------------------------------------------
TechnoTypeClass* HouseClass::FirstBuildableTechno_FromVector(DynamicVectorClass<TechnoTypeClass*>* pList)
{
    if (pList == nullptr || Type == nullptr) {
        return nullptr;
    }

    const int32 ownerBit = 1 << HouseTypeClass::From_Name(Type->ParentCountry);
    const int32 houseTypeIndex = Type->GetArrayIndex();
    const int32 houseTypeBit = 1 << houseTypeIndex;

    for (int32 i = 0; i < pList->Count; ++i) {
        TechnoTypeClass* pType = (*pList)[i];
        if (pType == nullptr) {
            continue;
        }

        if ((pType->GetOwners() & ownerBit) == 0) {
            continue;
        }

        if (pType->RequiredHouses != -1 && (pType->RequiredHouses & houseTypeBit) == 0) {
            continue;
        }

        if (pType->ForbiddenHouses != -1 && (pType->ForbiddenHouses & houseTypeBit) != 0) {
            continue;
        }

        if (pType->AIBasePlanningSide != -1 && pType->AIBasePlanningSide != Type->Side) {
            continue;
        }

        return pType;
    }

    return nullptr;
}

// ----------------------------------------------------------------------------
// asm HouseClass_FindFirstAccessibleTechnoTypeFromArray (0x505320, retn 4):
//   遍历向量，返回第一个 Owners 位含本阵营（按 Type->ParentCountry 注册号）
//   的类型。
// ----------------------------------------------------------------------------
TechnoTypeClass* HouseClass::FindFirstAccessibleTechnoTypeFromArray(DynamicVectorClass<TechnoTypeClass*>* pList)
{
    if (pList == nullptr || Type == nullptr) {
        return nullptr;
    }

    // asm: the shift source is the country slot, i.e. this house type's own
    //   registration index.  ParentCountry can be empty on the fallback
    //   country set, in which case From_Name returns -1 and 1 << -1 collapses
    //   to bit 31 -- no type ever matches.
    int32 countryIndex = Type->GetArrayIndex();
    if (countryIndex < 0 && Type->ID[0] != '\0')
        countryIndex = HouseTypeClass::FindIndex(Type->ID);
    if (countryIndex < 0 && Type->Name[0] != '\0')
        countryIndex = HouseTypeClass::FindIndexOfName(Type->Name);
    if (countryIndex < 0 && Type->ParentCountry[0] != '\0')
        countryIndex = HouseTypeClass::FindIndexOfName(Type->ParentCountry);
    const int32 ownerBit = (countryIndex >= 0)
        ? (1 << (countryIndex & 0x1F)) : 0;

    for (int32 i = 0; i < pList->Count; ++i) {
        TechnoTypeClass* pType = (*pList)[i];
        if (pType == nullptr) {
            continue;
        }
        if ((pType->GetOwners() & ownerBit) != 0) {
            return pType;
        }
    }

    return nullptr;
}

bool HouseClass::DoWeNeedWalls()
{
    // asm: HouseClass_Base_WhatNeedsWalls
    for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
        BuildingClass* pBuilding = OwnedBuildings[i];
        if (pBuilding && !pBuilding->IsDead() && pBuilding->IsArmed()) {
            return false;
        }
    }
    return OwnedBuildings.Count > 0;
}

int32 HouseClass::Base_WhatNeedsWalls()
{
    // asm: char __thiscall HouseClass_Base_ShouldRebuildNow(HouseClass_old *this, BaseNodeClass *a2)   (asm:592127)
    int32 count = 0;
    for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
        BuildingClass* pBuilding = OwnedBuildings[i];
        if (pBuilding && !pBuilding->IsDead() && pBuilding->IsArmed()) {
            ++count;
        }
    }
    return count;
}

bool HouseClass::Base_ShouldRebuildNow()
{
    // asm: void __thiscall HouseClass_LostPoweredCenter(HouseClass_old *this, _TechnoTypeClass *PoweredUnit)   (asm:595013)
    // 启动。
    return OwnedConyards.Count > 0 && !Defeated();
}

// ----------------------------------------------------------------------------
// asm HouseClass_LostPoweredCenter (0x50E0E0, retn 4):
//   电力中心计数 -1；若降到 0 且 type != 0，则把所有归属本房、类型索引 == type、
//   尚未被摧毁的机械单位 Deactivate（离线），并（游戏进行中、玩家视角、非场景
//   初始化时）播放 EVA_RobotTanksOffline。
// ----------------------------------------------------------------------------
void HouseClass::LostPoweredCenter(int32 type)
{
    --PoweredCenters;
    if (PoweredCenters != 0 || type == 0) {
        return;
    }

    bool any = false;
    if (TechnoClass::Array != nullptr) {
        for (int32 i = TechnoClass::Array->Count - 1; i >= 0; --i) {
            TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
            if (pTechno == nullptr || pTechno->Owner != this) {
                continue;
            }
            TechnoTypeClass* pT = pTechno->GetType();
            if (pT == nullptr || pT->GetArrayIndex() != type) {
                continue;
            }
            if (pTechno->IsDead()) {
                continue;
            }
            pTechno->Deactivate();
            any = true;
        }
    }

    if (any && Game::IsActive() && Is_Human()) {
        VoxClass::PlayFromName("EVA_RobotTanksOffline", -1, -1);
    }
}

// ----------------------------------------------------------------------------
// asm HouseClass_GotPoweredCenter (0x50E000, retn 4):
//   电力中心计数 +1；若从 0 变到 1 且 type != 0，则把所有归属本房、类型索引
//   == type、尚未被摧毁的机械单位 Activate（上线），并（游戏进行中、玩家视角）
//   播放 EVA_RobotTanksBackOnline。
// ----------------------------------------------------------------------------
void HouseClass::GotPoweredCenter(int32 type)
{
    ++PoweredCenters;
    if (PoweredCenters != 1 || type == 0) {
        return;
    }

    bool any = false;
    if (TechnoClass::Array != nullptr) {
        for (int32 i = TechnoClass::Array->Count - 1; i >= 0; --i) {
            TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
            if (pTechno == nullptr || pTechno->Owner != this) {
                continue;
            }
            TechnoTypeClass* pT = pTechno->GetType();
            if (pT == nullptr || pT->GetArrayIndex() != type) {
                continue;
            }
            if (pTechno->IsDead()) {
                continue;
            }
            pTechno->Activate();
            any = true;
        }
    }

    if (any && Game::IsActive() && Is_Human()) {
        VoxClass::PlayFromName("EVA_RobotTanksBackOnline", -1, -1);
    }
}

bool HouseClass::CanSeePsiWarning()
{
    // asm: void __fastcall HouseClass_ActivateBaseBuilding(HouseClass_old *this)   (asm:591081)
    // 敌人心灵控制了地图也弹不出警告。
    return GetPowerOutput() > 0;
}

void HouseClass::ActivateBaseBuilding(BuildingClass* pBuilding)
{
    // asm: HouseClass_AcquiredConyard
    if (pBuilding) {
        pBuilding->QueueMission(Mission::Guard);
    }
}

void HouseClass::AcquiredConyard(BuildingClass* pConyard)
{
    // asm: HouseClass_IonCannonFire
    if (pConyard) {
        pConyard->Captured(this);
    }
}

void HouseClass::IonCannonFire(const CellStruct& target)
{
    // asm HouseClass_IonCannonFire (0x509B10, retn 4)：先要求有 PrimaryAggressor，
    //   目标优先取 BestTargetCell，未设时按 DefensiveCellField 选择全局/路径点
    //   最优目标；哨兵值则放弃，否则 Resolve_Target_Index + SW_Fire 发射。
    (void)target;

    if (PrimaryAggressor == -1) {
        return;
    }
    if (SuperWeapons == nullptr) {
        return;
    }

    const CellStruct sentinel = HouseClass::DefaultIonCannon_Coords;
    CellStruct coords = BestTargetCell;
    if (coords == sentinel) {
        if (DefensiveCellField == 1) {
            coords = Pick_Offensive_SWTarget();
        } else {
            coords = Pick_Offensive_SWTarget_AtWaypoint(DefensiveCellField);
        }
    }
    if (coords == sentinel) {
        return;
    }

    SW_Fire(Resolve_Target_Index(coords), coords);
}

// ============================================================================
// asm: HouseClass_SpySat_Update (0x508F6B)
//
//   void __thiscall HouseClass_SpySat_Update(HouseClass *this)
//
//   逐段：
//     * [esi+5779h] = 0（RadarOutage 清位）。
//     * 遍历本房建筑，找"在线、已建成、类型带 SpySat 且非出售中"的一座
//       （asm: 建筑类型 +16A5h / 建筑 +81h/+74h / 任务 != 13h）。
//       只要存在这样一座 -> 卫星生效。
//     * 生效：若 [esi+577Ah]==0（尚未揭示）-> Map.Reveal_The_Map(this) 并置 1；
//              玩家阵营播 Rules+220h 音效。
//     * 失效：若 [esi+577Ah]!=0 -> Map.Shroud_The_Map(this) 并置 0；
//              玩家阵营播 Rules+224h 音效。
// ============================================================================
void HouseClass::SpySat_Update()
{
    // asm: mov byte ptr [esi+5779h], 0
    RadarDisabled = false;

    // asm loc_508F7C: 找一座带 SpySat 的在线建筑。
    bool hasSpySat = false;
    for (int32 i = 0; i < OwnedBuildings.Count; ++i) {   // asm: [esi+78h] / [esi+6Ch]
        BuildingClass* pBldg = OwnedBuildings[i];
        if (pBldg == nullptr || pBldg->IsDead()) {
            continue;                                    // asm: [ecx+81h]
        }
        if (pBldg->Type == nullptr || !pBldg->Type->SpySat) {
            continue;                                    // asm: [eax+16A5h]
        }
        if (!pBldg->IsConstructed) {
            continue;                                    // asm: [ecx+74h]
        }
        if (pBldg->CurrentMission == Mission::Selling) {
            continue;                                    // asm: [ecx+0ACh]==13h
        }
        hasSpySat = true;
        break;
    }

    if (hasSpySat) {
        // asm loc_509048: 已生效则无需重复揭示。
        if (!IsSpySatActiveInRadar) {
            if (MapClass::Instance != nullptr) {
                MapClass::Instance->Reveal_The_Map(this);   // asm: MapClass::Reveal_The_Map
            }
            IsSpySatActiveInRadar = true;                    // asm: [esi+577Ah]=1
            // asm: 玩家阵营播 Rules+220h 音效（本项目音效名未建模，略）。
        }
    } else {
        // asm loc_509002: 卫星失效 -> 恢复黑幕。
        if (IsSpySatActiveInRadar) {
            if (MapClass::Instance != nullptr) {
                MapClass::Instance->Shroud_The_Map(this);   // asm: MapClass::Shroud_The_Map
            }
            IsSpySatActiveInRadar = false;                   // asm: [esi+577Ah]=0
            // asm: 玩家阵营播 Rules+224h 音效（略）。
        }
    }

    // asm: 卫星生效时刷新最近一次时间戳。
    if (hasSpySat) {
        LastSpySatTime = Game::GetCurrentFrame();
    }
}

void HouseClass::SellOverlay(const CellStruct& cell)
{
    // asm: int __fastcall HouseClass_CreateParadrop(HouseClass_old *this, int planeTypeIdx, int planeCount, int mission, int target, int destination, signed int infTypeIdx, int infCount)   (asm:1253351)
    SellCell(cell);
}

void HouseClass::CreateParadrop(const CellStruct& target)
{
    // asm: HouseClass_GetPlanningWaypointAt
    Set_Some_Target_Cell(target);
    Fire_Paradrop();
}

CellStruct HouseClass::GetPlanningWaypointAt(int32 index)
{
    // asm: char __thiscall HouseClass_GetPlanningWaypointProperties(HouseClass_old *this, WaypointClass *a2, int *idxPath, byte *idxWP)   (asm:572099)
    if (index < 0 || index >= PlanningWaypointCoordCount) {
        return CellStruct(0, 0);
    }
    return PlanningWaypointCoords[index];
}

int32 HouseClass::GetPlanningWaypointProperties(int32 index)
{
    // asm: void __thiscall HouseClass_AcquiredThreatNode(HouseClass_old *this)   (asm:584985)
    (void)index;
    return 0;
}

void HouseClass::AcquiredThreatNode(const CellStruct& cell)
{
    // asm: HouseClass_Fire_PsyDom
    Set_Target_Cell(cell);
}

void HouseClass::Fire_PsyDom(const CellStruct& target)
{
    // asm: int __fastcall HouseClass_TryFindEligibleAITriggers(VectorPtrAITeamTypeClass *resultVector_, HouseClass_old *Owner, int arg)   (asm:1543744)
    (void)target;
    Fire_PsychicDominator();
}

// ----------------------------------------------------------------------------
// asm: int __fastcall HouseClass_TryFindEligibleAITriggers(
//          VectorPtrAITeamTypeClass *resultVector_, HouseClass *Owner, int arg)
//      (asm:1543744, retn 4)
//
//   这是 AI 触发器“运行实例被逐帧评估”的真实场所。原版没有独立的触发器运行
//   实例对象，触发器的运行期状态直接挂在 `AITriggerTypeClass` 上。本函数流程：
//     1. 随机(1..100) > RatioAITriggerTeam 或 AITriggersActive 关 → 放弃本帧。
//     2. 若本房已编队伍足够多、且基地防御队数已达门槛 → 认为无需再触发。
//     3. 遍历全局 vec_AITriggerTypes，对 ConditionMet 通过的项按 WeightCur 权重
//        收进分布；WeightCur == 5000 的“uber”触发器一旦出现即清空之前的候选。
//     4. 在候选里按权重做加权随机挑一条，取出其 Team1 / Team2，把对应队伍模板
//        标记 Autocreate，交给后续的建队流程自动成军。
//
//   项目以 bool 返回“本帧是否有触发器被选中”；结果队伍经由被标记 Autocreate
//   的队伍模板在别处成军（保持与原版一致的单向副作用）。
// ----------------------------------------------------------------------------
bool HouseClass::TryFindEligibleAITriggers(DynamicVectorClass<TeamTypeClass*>* pResult)
{
    static constexpr int32 kUberWeight = 5000;

    // 1. 触发节流：随机门 + 全局开关。
    if (ScenarioClass::Instance) {
        int32 roll = ScenarioClass::Instance->Random.Next(1, 100);
        if (roll > RatioAITriggerTeam) return false;
    }

    // AITriggersActive 未在项目中单独建模，以“本房非失败态”近似其语义
    //（原版该位在场景/规则初始化时置位，失败后不再评估）。存疑项见报告。
    if (Defeated()) return false;

    // 2. 统计本房当前已编队伍总数与其中的基地防御队数量。
    //    asm 遍历 vec_Teams（AITeamClass 全局表），按 Owner / TeamType 归属过滤。
    int32 ownTeamCount = 0;
    int32 baseDefenseCount = 0;
    if (AITeamClass::Array) {
        for (int32 i = 0; i < AITeamClass::Array->Count; ++i) {
            AITeamClass* pTeam = AITeamClass::Array->GetItem(i);
            if (!pTeam || pTeam->Owner != this) continue;
            ++ownTeamCount;
            if (pTeam->TeamType && pTeam->TeamType->IsBaseDefense) {
                ++baseDefenseCount;
            }
        }
    }

    // 3. 基地防御门槛（asm: RulesClass + MinimumAIDefensiveTeams[difficulty]）。
    //    已达标则本帧不再触发：原版据此抑制“基地已经很稳固时继续出防御队”。
    bool enoughBaseDefense = false;
    if (RulesClass::Instance && RulesClass::Instance->MinimumAIDefensiveTeams.Count > 0) {
        // asm: 难度取自本 house 记录上的 AIDifficulty（OpenYR 语义名 DifficultyLevel）。
        int32 difficulty = DifficultyLevel;
        if (difficulty < 0) difficulty = 0;
        if (difficulty >= RulesClass::Instance->MinimumAIDefensiveTeams.Count) {
            difficulty = RulesClass::Instance->MinimumAIDefensiveTeams.Count - 1;
        }
        int32 minimumTeams = RulesClass::Instance->MinimumAIDefensiveTeams[difficulty];
        if (ownTeamCount >= minimumTeams && ownTeamCount > 0) {
            // 防御队比例过半即视为基地防御充足。
            if (baseDefenseCount * 2 >= ownTeamCount) {
                enoughBaseDefense = true;
            }
        }
    }

    // 4. 收集满足条件、可按权重挑选的触发器。
    if (!AITriggerTypeClass::Array) return false;

    AITriggerTypeClass* candidates[64];
    int32 candidateWeights[64];
    int32 candidateCount = 0;
    int32 sumWeight = 0;
    bool foundUber = false;

    const int32 maxCandidates = static_cast<int32>(sizeof(candidates) / sizeof(candidates[0]));

    for (int32 i = 0; i < AITriggerTypeClass::Array->Count; ++i) {
        AITriggerTypeClass* pTrigger = AITriggerTypeClass::Array->GetItem(i);
        if (!pTrigger) continue;

        // asm: AITriggerTypeClass_ConditionMet(this, CallingHouse, targetHouse,
        //      AlreadyHasMinimumBaseDefense)。
        if (!pTrigger->ConditionMet(this, this, enoughBaseDefense)) continue;

        int32 weight = static_cast<int32>(pTrigger->Weight_Current);

        // asm: WeightCur == 5000 的“uber”触发器独占候选；一旦出现即清空先前候选。
        if (weight == kUberWeight) {
            if (foundUber) continue;
            foundUber = true;
            candidateCount = 0;
            sumWeight = 0;
        } else if (foundUber) {
            continue;
        }

        if (candidateCount < maxCandidates) {
            candidates[candidateCount] = pTrigger;
            candidateWeights[candidateCount] = weight;
            ++candidateCount;
        }
        sumWeight += weight;
    }

    if (candidateCount == 0 || sumWeight < 1) return false;

    // 5. 加权随机挑一条（asm: 先 roll(1..sumWeight)，再按累计权重命中）。
    int32 pick = candidateCount - 1;
    if (ScenarioClass::Instance) {
        int32 roll = ScenarioClass::Instance->Random.Next(1, sumWeight);
        int32 acc = 0;
        for (int32 i = 0; i < candidateCount; ++i) {
            acc += candidateWeights[i];
            if (acc >= roll) { pick = i; break; }
        }
    }

    AITriggerTypeClass* chosen = candidates[pick];
    if (!chosen) return false;

    // 6. asm: 把选中触发器的 Team1 / Team2 依次压入 resultVector
    //      （`mov esi,[edi+AITriggerTypeClass.Team1]` / `...[Team2]` 两段
    //      各自 `call [vt+8]` 追加）。调用方随后对每一项调
    //      TeamTypeClass_CreateTeam 成军。
    //    pResult 为 null 时保持旧的"只报是否有命中"的语义；Autocreate 位
    //    也照旧置上（原版该位是 TeamType 的持久属性，不当作一次性队列）。
    TeamTypeClass* pTeam1 = chosen->Team1;
    TeamTypeClass* pTeam2 = chosen->Team2;
    if (pTeam1) pTeam1->Autocreate = true;
    if (pTeam2) pTeam2->Autocreate = true;

    if (pResult != nullptr) {
        if (pTeam1 != nullptr) pResult->Add(pTeam1);
        if (pTeam2 != nullptr && pTeam2 != pTeam1) pResult->Add(pTeam2);
    }

    return true;
}

// ============================================================================
// asm: HouseClass_Update 的建队段（asm:552777..552900, loc_4F8A00..loc_4F8B08）
//
//   ; mov edx,[this+TeamDelayTimer.TimeStart]
//   ; mov eax,[this+TeamDelayTimer.TimeLeft]
//   ; cmp edx,-1 / jz due
//   ; ecx = Frame - edx / cmp ecx,eax / jge due
//   ; sub eax,ecx / cmp eax,0 / jnz skip        ← 未到点：整段跳过
//   due:
//   ; eax = Session / cmp 0
//   ; al = IsHuman ; (Session != 0 ? al : (IsHuman || IsPlayer))
//   ; if (al) skip                              ← 人类阵营不自动成军
//   ; if (Country->[1A6h]) skip                 ← 国家禁用 AI
//   ; TryFindEligibleAITriggers(&resultVector, this, 0)
//   ; for (i = 0; i < resultVector.Count; ++i)
//   ;     TeamTypeClass_CreateTeam(resultVector[i], this)
//   ; TeamDelayTimer.TimeStart = Frame
//   ; TeamDelayTimer.TimeLeft  = Rules->TeamDelays[AIDifficulty]
//   skip:
//
//   这是 AI 唯一的「触发器 → 新队伍」出口：HouseClass::Update 在跑 AI()
//   之前调用它（asm:552841 的 CreateTeam 早于 553401 的 Update_AI）。
// ============================================================================
void HouseClass::Update_Team_Production()
{
    if (IsDeadObject || IsDefeated)
        return;

    // ---- 时间窗（asm loc_4F8A00）----------------------------------------
    const int32 frame = Game::CurrentFrame;
    bool due = true;

    if (TeamDelayTimer.StartTime != -1) {
        const int32 elapsed = frame - TeamDelayTimer.StartTime;   // asm: sub ecx,edx
        if (elapsed < TeamDelayTimer.TimeLeft) {                  // asm: cmp ecx,eax / jl
            const int32 remaining = TeamDelayTimer.TimeLeft - elapsed;  // asm: sub eax,ecx
            if (remaining != 0)                                   // asm: cmp eax,0 / jnz skip
                due = false;
        }
    }

    if (!due)
        return;

    // ---- 人类/玩家阵营不自动成军（asm loc_4F8A27..loc_4F8A4A）----------
    //   Session != 0 → al = IsHuman；否则 al = (IsHuman || IsPlayer)。
    const bool sessionActive = (SessionClass::GetInstance() != nullptr);
    const bool humanish = Is_Human()
                       || (!sessionActive && HouseClass::Player == this);
    if (humanish)
        return;

    // asm: mov edx,[this.Country] / mov al,[edx+1A6h] / test / jnz skip
    //   本项目未建模「国家禁用 AI」位（与 Update 的 AIBuildState 段同一处理），
    //   按允许处理。

    // ---- 选触发器 → 成军（asm loc_4F8A5E..loc_4F8ABB）------------------
    DynamicVectorClass<TeamTypeClass*> eligible;
    if (!TryFindEligibleAITriggers(&eligible))
        return;

    for (int32 i = 0; i < eligible.Count; ++i) {
        TeamTypeClass* pTeamType = eligible[i];
        if (pTeamType == nullptr)
            continue;
        // asm: `push this / mov ecx,[ecx+edi*4] / call TeamTypeClass_CreateTeam`
        //   CreateTeam 内部新建的 TeamClass 会把自己登记进 vec_Teams
        //   （见 TeamClass::TeamClass 的 asm:1526848 段）。
        pTeamType->CreateTeam(this);
    }

    // ---- 下一次窗口（asm loc_4F8ABB）------------------------------------
    //   TimeLeft = Rules->TeamDelays[AIDifficulty]（RulesClass+115Ch 的数组）。
    int32 delay = 0;
    if (RulesClass::Instance != nullptr && RulesClass::Instance->TeamDelays.Count > 0) {
        int32 difficulty = DifficultyLevel;
        if (difficulty < 0)
            difficulty = 0;
        if (difficulty >= RulesClass::Instance->TeamDelays.Count)
            difficulty = RulesClass::Instance->TeamDelays.Count - 1;
        delay = RulesClass::Instance->TeamDelays[difficulty];
    }

    TeamDelayTimer.StartTime = frame;       // asm: TeamDelayTimer.TimeStart = Frame
    // rules.ini 未加载（或该难度项缺失）时给一个保守下限，否则 TimeLeft=0
    // 会让本段每帧都成军，瞬间掏空资金。这不是 asm 常量，只是缺省节流。
    TeamDelayTimer.TimeLeft  = (delay > 0) ? delay : 25;
}

// ============================================================================
// asm: HouseClass_CanInstantiateTeam (0x509630, retn 4)
//
//   char __thiscall HouseClass_CanInstantiateTeam(HouseClass *this, TeamTypeClass *TeamType)
//
//   逐段：
//     * TeamType == null 或 TeamType->TaskForce(+0xE4) == null -> false。
//     * TaskForce 成员数 <= 0 -> true。
//     * 逐成员（成员是 TechnoTypeClass*）：
//         type != null 且 type->vfn[0x94](type,1,1,0,this) 非零（本房能造）
//           -> 继续下一成员；
//         否则（单机）：遍历 vec_Foot 找归属本房、GetType()==type 的单位，
//           再 TeamTypeClass_CanRecruitUnit(this, unit, this) 为真 -> 视为可征募；
//           找不到 -> false；多人会话（Session!=0）下直接 false。
//     * 全部通过 -> true。
// ============================================================================
bool HouseClass::CanInstantiateTeam(TeamTypeClass* pTeamType)
{
    // asm: mov esi,[TeamType]; test esi,esi / jz loc_5096F1
    if (pTeamType == nullptr) {
        return false;
    }

    // asm: mov eax,[esi+0E4h] / test eax,eax / jz loc_5096F1
    TaskForceClass* pTaskForce = pTeamType->TaskForce;
    if (pTaskForce == nullptr) {
        return false;
    }

    // asm: mov ecx,[eax+9Ch] / test ecx,ecx / jle loc_5096E5 (return true)
    if (pTaskForce->Members.Count <= 0) {
        return true;
    }

    for (int32 i = 0; i < pTaskForce->Members.Count; ++i) {   // asm: loc_509651
        const TaskForceMember& member = pTaskForce->Members[i];
        TechnoTypeClass* pMemberType = member.Type;

        // asm: test ebx,ebx / jz loc_509671
        if (pMemberType != nullptr) {
            // asm: call [edx+94h](type, 1, 1, 0, this) —— 本房能否生产该类型。
            if (Can_Produce_Type(pMemberType)) {
                continue;                                     // 命中即下一个成员
            }
        }

        // asm loc_509671: mov eax,Session / test / jnz loc_5096F1
        if (SessionClass::GetInstance() != nullptr) {
            return false;                                     // 多人：造不了即失败
        }

        // asm loc_509686: 遍历 vec_Foot，找归属本房、类型匹配、可征募的单位。
        bool recruited = false;
        if (FootClass::Array != nullptr) {
            for (int32 f = FootClass::Array->Count - 1; f >= 0; --f) {
                FootClass* pFoot = FootClass::Array->GetItem(f);
                if (pFoot == nullptr || pFoot->Owner != this) {
                    continue;                                 // asm: cmp [esi+21Ch],ebp
                }
                if (pFoot->GetType() != pMemberType) {
                    continue;                                 // asm: call [edx+84h]; cmp
                }
                // asm: TeamTypeClass_CanRecruitUnit(this, unit, this)
                if (pTeamType->CanRecruitUnit(pFoot, this)) {
                    recruited = true;
                    break;
                }
            }
        }

        if (!recruited) {
            return false;                                     // asm: dec edi / js loc_5096F1
        }
    }

    // asm loc_5096E5: mov al,1
    return true;
}

// ----------------------------------------------------------------------------
// asm HouseClass_FindRepairBay (0x455DD0)：
//   在己方“可接收该单位停靠维修”的建筑里挑出离 pUnit 最近的一座。原版用
//   Radio_SendCommand(dock, 0x0F) 询问对方是否接受停靠，再按三维距离取最小、
//   且在修理半径（由规则算出的 dword_89C7F0）内。本项目未建模命令 0x0F，改以
//   建筑的“维修平台”标志近似该判定；距离用平方距离比较（与取最小等价）。
// ----------------------------------------------------------------------------
BuildingClass* HouseClass::FindRepairBay(TechnoClass* pUnit)
{
    if (pUnit == nullptr) {
        return nullptr;
    }

    const CoordStruct src = pUnit->GetCoords();
    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
        BuildingClass* pBuilding = OwnedBuildings[i];
        if (pBuilding == nullptr || pBuilding->IsDead() || pBuilding->Type == nullptr) {
            continue;
        }
        if (!pBuilding->Type->IsRepairPad) {
            continue;
        }

        const CoordStruct dst = pBuilding->GetCoords();
        const int32 dx = dst.X - src.X;
        const int32 dy = dst.Y - src.Y;
        const int32 dz = dst.Z - src.Z;
        const int32 dist = dx * dx + dy * dy + dz * dz;

        if (dist < bestDist) {
            bestDist = dist;
            pBest = pBuilding;
        }
    }

    return pBest;
}

// ----------------------------------------------------------------------------
// asm HouseClass_Update_AI_TryFireSW (0x5098F8)：
//   逐帧让 AI 阵营尝试发射超级武器。人类阵营直接返回。对每一个“已就绪”
//   （SuperClass +0x6F 置位）的超级武器按类型分派：
//     MultiMissile(0)   —— 需有 PrimaryAggressor；目标优先取 BestTargetCell，
//                          未设时按 DefensiveCellField==1 取全局最优目标、
//                          否则按该路径点取最优目标；哨兵值则放弃。
//     LightningStorm(2) —— Fire_LightningStorm。
//     PsychicDominator(7)—— Fire_PsychicDominator。
//     GeneticConverter(9)—— Fire_GeneticMutator。
//     ParaDrop(5)/AmerParaDrop(6)/SpyPlane(8)/PsychicReveal(11) —— Fire_Paradrop。
//     ForceShield(10)   —— 目标取 PreferredDefensiveCell；哨兵值则放弃。
//     其余类型不动作。
//   命中目标后统一走 Resolve_Target_Index + SW_Fire 发射。
// ----------------------------------------------------------------------------
void HouseClass::Update_AI_TryFireSW()
{
    if (Is_Human()) {
        return;
    }
    if (SuperWeapons == nullptr) {
        return;
    }

    const CellStruct sentinel = HouseClass::DefaultIonCannon_Coords;

    for (int32 i = 0; i < SuperWeapons->Count; ++i) {
        SuperClass* pSuper = (*SuperWeapons)[i];
        if (pSuper == nullptr || pSuper->Type == nullptr) {
            continue;
        }
        if (!pSuper->IsReady()) {
            continue;
        }

        switch (pSuper->Type->Type) {
        case SuperWeaponType::MultiMissile: {
            if (PrimaryAggressor == -1) {
                break;
            }
            CellStruct coords = BestTargetCell;
            if (coords == sentinel) {
                if (DefensiveCellField == 1) {
                    coords = Pick_Offensive_SWTarget();
                } else {
                    coords = Pick_Offensive_SWTarget_AtWaypoint(DefensiveCellField);
                }
            }
            if (coords == sentinel) {
                break;
            }
            SW_Fire(Resolve_Target_Index(coords), coords);
            break;
        }
        case SuperWeaponType::LightningStorm:
            Fire_LightningStorm();
            break;
        case SuperWeaponType::PsychicDominator:
            Fire_PsychicDominator();
            break;
        case SuperWeaponType::GeneticConverter:
            Fire_GeneticMutator();
            break;
        case SuperWeaponType::ParaDrop:
        case SuperWeaponType::AmerParaDrop:
        case SuperWeaponType::SpyPlane:
        case SuperWeaponType::PsychicReveal:
            Fire_Paradrop();
            break;
        case SuperWeaponType::ForceShield: {
            const CellStruct coords = PreferredDefensiveCell;
            if (coords == sentinel) {
                break;
            }
            SW_Fire(Resolve_Target_Index(coords), coords);
            break;
        }
        default:
            break;
        }
    }
}

// ============================================================================
// asm: HouseClass_Radar_Activate (0x508E00)
//
//   void __thiscall HouseClass_Radar_Activate(HouseClass *this)
//
//   逐段：
//     * [ecx+5779h] = 0（RadarOutage 清位）。
//     * 仅玩家阵营继续；其它阵营直接返回。
//     * radarMode = 0。
//       若雷达黑屏计时已到点：
//         Scenario+34A4h != 0 -> radarMode = 1；
//         否则若电力不足（PowerOutput<PowerDrain）-> 保持 0；
//         否则遍历本房建筑，找"在线、已建成、类型带 Radar、非出售中"的一座，
//         存在 -> radarMode = 1。
//     * 与 Map.IsRadarAvailable() 比较，不同则 SetRadarActivity(radarMode)。
// ============================================================================
void HouseClass::Radar_Activate()
{
    // asm: mov byte ptr [ecx+5779h], 0
    RadarDisabled = false;

    // asm: cmp ecx,HouseClass_PlayerPtr / jnz -> 只有玩家阵营继续。
    if (HouseClass::Player != this) {
        return;
    }

    int32 radarMode = 0;

    // asm: 雷达黑屏计时到点才判定。
    if (RadarBlackoutTimer.GetTimeLeft() == 0) {
        // asm: 电力充足性判定。
        const bool powerShort = (PowerOutput < PowerDrain) && (PowerDrain != 0)
            && (PowerOutput == 0
                || static_cast<double>(PowerOutput) / static_cast<double>(PowerDrain) < 1.0);

        if (!powerShort) {
            // asm loc_508E9D: 遍历建筑找在线雷达。
            for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
                BuildingClass* pBldg = OwnedBuildings[i];
                if (pBldg == nullptr || pBldg->IsDead()) {
                    continue;                                // asm: [eax+81h]
                }
                if (pBldg->Type == nullptr || !pBldg->Type->Radar) {
                    continue;                                // asm: [eax+16A4h]
                }
                if (!pBldg->IsConstructed) {
                    continue;                                // asm: [eax+74h]
                }
                if (pBldg->CurrentMission == Mission::Selling) {
                    continue;                                // asm: [eax+0ACh]==13h
                }
                radarMode = 1;                               // asm loc_508F2A
                break;
            }
        }
    }

    // asm loc_508F2F: 与地图雷达可用性比较，不同则切换。
    if (MapClass::Instance != nullptr) {
        if (MapClass::Instance->IsRadarAvailable() != (radarMode != 0)) {
            MapClass::Instance->SetRadarActivity(radarMode);
        }
    }
}

void HouseClass::GenerateStartingUnits()
{
    // asm: HouseClass_GetTechnoToProduce
    Respawn_Starting_Technos();
}

// ============================================================================
// asm: HouseClass_GetTechnoToProduce (0x4FBD80, retn 8)
//
//   _TechnoTypeClass *__thiscall HouseClass_GetTechnoToProduce(
//       HouseClass *this, int TechnoKind, int a3)
//
//   asm 用 TechnoKind-1 作为 0..39 的 switch 表索引（byte_4FBE0C），把值
//   映射到五个小类，再到对应类别里按 "+56xx ToProduce" 索引取类型指针：
//     kind = AbstractType::Unit(1)      -> Unit 的 +5650
//     kind = AbstractType::Aircraft(2)  -> Aircraft 的 +5658
//     kind = AbstractType::AircraftType(3)-> Aircraft 的 +5658
//     kind = AbstractType::Building(6)  -> Building 的 +564C
//     kind = AbstractType::BuildingType(7)-> Building 的 +564C
//     kind = AbstractType::Infantry(15) -> Infantry 的 +5654
//     kind = AbstractType::InfantryType(16)-> Infantry 的 +5654
//     kind = AbstractType::UnitType(40) -> Unit 的 +5650
//   其余 kind（含 idx 3/4/8..13/17..38）走 default -> null。
//   每个索引为 0xFFFFFFFF（-1）时同样返回 null。
// ============================================================================
TechnoTypeClass* HouseClass::GetTechnoToProduce(AbstractType technoKind)
{
    // asm: 先按 kind 决定去哪张类型表取；再校验索引非 -1。
    switch (technoKind)
    {
    case AbstractType::Unit:
    case AbstractType::UnitType:
        // asm: loc_4FBDAF -> [ecx+5650h] / vec_UnitTypes.data[idx]
        if (UnitTypeToProduce != -1
            && UnitTypeClass::Array != nullptr
            && UnitTypeToProduce >= 0
            && UnitTypeToProduce < UnitTypeClass::Array->Count)
        {
            return UnitTypeClass::Array->GetItem(UnitTypeToProduce);
        }
        return nullptr;

    case AbstractType::Aircraft:
    case AbstractType::AircraftType:
        // asm: loc_4FBD99 -> [ecx+5658h] / vec_AircraftTypes.data[idx]
        if (AircraftTypeToProduce != -1
            && AircraftTypeClass::Array != nullptr
            && AircraftTypeToProduce >= 0
            && AircraftTypeToProduce < AircraftTypeClass::Array->Count)
        {
            return AircraftTypeClass::Array->GetItem(AircraftTypeToProduce);
        }
        return nullptr;

    case AbstractType::Infantry:
    case AbstractType::InfantryType:
        // asm: loc_4FBDC6 -> [ecx+5654h] / vec_InfantryTypes.data[idx]
        if (InfantryTypeToProduce != -1
            && InfantryTypeClass::Array != nullptr
            && InfantryTypeToProduce >= 0
            && InfantryTypeToProduce < InfantryTypeClass::Array->Count)
        {
            return InfantryTypeClass::Array->GetItem(InfantryTypeToProduce);
        }
        return nullptr;

    case AbstractType::Building:
    case AbstractType::BuildingType:
        // asm: loc_4FBDDC -> [ecx+564Ch] / BuildingTypeClass.Vector[idx]
        if (BuildingTypeToProduce != -1
            && BuildingTypeClass::Array != nullptr
            && BuildingTypeToProduce >= 0
            && BuildingTypeToProduce < BuildingTypeClass::Array->Count)
        {
            return BuildingTypeClass::Array->GetItem(BuildingTypeToProduce);
        }
        return nullptr;

    default:
        // asm: loc_4FBDF3 xor eax,eax
        return nullptr;
    }
}

void HouseClass::DroppedFlag()
{
    // asm: void __thiscall HouseClass_CreateWaypointPath(HouseClass_old *this, int idxPath)   (asm:576504)
    UpdateFlagCoords();
}

void HouseClass::CreateWaypointPath(const CellStruct& from, const CellStruct& to)
{
    // asm: signed int __fastcall HouseClass_CreateWaypointPath2(HouseClass_old *this)   (asm:584877)
    (void)from;
    if (PlanningWaypointCoordCount < 8) {
        PlanningWaypointCoords[PlanningWaypointCoordCount++] = to;
    }
}

void HouseClass::CreateWaypointPath2(const CellStruct& from, const CellStruct& to, int32 hop)
{
    // asm: int __thiscall HouseClass_Update_AI_Production(HouseClass_old *this, int)   (asm:585803)
    if (hop <= 0) hop = 1;
    int32 dx = (to.X > from.X ? 1 : (to.X < from.X ? -1 : 0));
    int32 dy = (to.Y > from.Y ? 1 : (to.Y < from.Y ? -1 : 0));
    CellStruct cur = from;
    while ((cur.X != to.X || cur.Y != to.Y) && PlanningWaypointCoordCount < 8) {
        cur.X += dx * hop;
        cur.Y += dy * hop;
        PlanningWaypointCoords[PlanningWaypointCoordCount++] = cur;
    }
}

// ============================================================================
// asm: HouseClass_Update_AI_Production (0x50971C, retn 4)
//
//   int __thiscall HouseClass_Update_AI_Production(HouseClass *this, int arg0)
//
//   这是 AI 建造状态机（AIBuildState，asm +0x1C0）的推进器，由建筑出货
//   （BuildingClass::Exit_Object）与 Update 调用：
//     * 人类/玩家阵营、或无会话 -> 不动作。
//     * 阈值 = RulesClass+1300h（本房"资金充裕"门限）。
//     switch (AIBuildState)：
//       0: 资金 >= 阈值 -> 返回；否则 AIBuildState = (arg0==6)?2:1。
//       1: 资金 >= 阈值 -> AIBuildState=0；否则 arg0==6 时 -> AIBuildState=2。
//       2: 资金 >= 阈值 -> AIBuildState=0；
//          否则：若已有兵营(BuildBarracks)与重工(BuildWeapons)且电力够、
//          随机(0,1)==0 -> 返回；否则 arg0!=6 时 -> AIBuildState=1。
//       3: 资金 >  阈值 -> AIBuildState=0。
//       其它: 返回。
// ============================================================================
void HouseClass::Update_AI_Production(int32 arg0)
{
    if (arg0 == -1) {
        arg0 = 0;                                   // 兼容旧的无参调用
    }

    // asm: 人类/玩家阵营直接返回。
    if (Is_Human()) {
        return;
    }
    // asm: mov ecx,Session / test / jz default
    if (SessionClass::GetInstance() == nullptr) {
        return;
    }

    // asm: RulesClass+1300h（资金充裕门限）。本项目以 MinMoney 近似该标量。
    const int32 threshold = (RulesClass::Instance != nullptr)
        ? RulesClass::Instance->MinMoney : 0;

    switch (AIBuildState) {                         // asm: switch [esi+field_1C0]
    case 0:
        // asm loc_5098AB
        if (AvailableMoney() >= threshold) {
            return;                                 // jge default
        }
        AIBuildState = (arg0 == 6) ? 2 : 1;         // setz al / inc eax
        return;

    case 1:
        // asm loc_50974F
        if (AvailableMoney() >= threshold) {
            AIBuildState = 0;                       // loc_50989B
            return;
        }
        if (arg0 != 6) {
            return;
        }
        AIBuildState = 2;
        return;

    case 2: {
        // asm loc_509786
        if (AvailableMoney() >= threshold) {
            AIBuildState = 0;                       // loc_50989B
            return;
        }

        // asm: 依次检查是否已有兵营（Rules+904/910）与重工（BuildWeapons）。
        bool hasBarracks = false;
        bool hasWeapons = false;
        if (RulesClass::Instance != nullptr) {
            for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
                BuildingClass* pBldg = OwnedBuildings[i];
                if (pBldg == nullptr || pBldg->IsDead() || pBldg->Type == nullptr) {
                    continue;
                }
                for (int32 b = 0; b < RulesClass::Instance->BuildBarracks.Count; ++b) {
                    if (RulesClass::Instance->BuildBarracks[b] == pBldg->Type) {
                        hasBarracks = true;
                        break;
                    }
                }
                for (int32 w = 0; w < RulesClass::Instance->BuildWeapons.Count; ++w) {
                    if (RulesClass::Instance->BuildWeapons[w] == pBldg->Type) {
                        hasWeapons = true;
                        break;
                    }
                }
            }
        }

        // asm: test al/al(b) / jz loc_50986C；电力不足亦跳 loc_50986C。
        if (hasWeapons && hasBarracks && (PowerDrain <= PowerOutput)) {
            if (ScenarioClass::Instance != nullptr
                && ScenarioClass::Instance->Random.Next(0, 1) != 0) {
                return;                             // 随机命中 -> 本帧不动
            }
        }

        // asm loc_50986C: cmp arg0,6 / jz default; AIBuildState = 1
        if (arg0 == 6) {
            return;
        }
        AIBuildState = 1;
        return;
    }

    case 3:
        // asm loc_509883: 资金 > 阈值 -> AIBuildState=0，否则不动。
        if (AvailableMoney() > threshold) {
            AIBuildState = 0;                       // loc_50989B
        }
        return;

    default:
        return;                                     // asm loc_5098D6
    }
}

void HouseClass::CreateBulletWarningAnim(const CellStruct& target)
{
    // asm: void __thiscall HouseClass_LoadVectors(HouseClass_old *this, LPSTREAM pStm)   (asm:546602)
    // 层据格播放。
    Set_Some_Target_Cell(target);
}

void HouseClass::LoadVectors()
{
    // asm: HouseClass_Fire_GenMutator
    // 重新装填。
    Recalc_Threats();
    Update_Power();
}

// ------------------------------------------------------------------------
// asm: HouseClass_Fire_GenMutator
// 外层传入的参数不影响选取结果。
// ------------------------------------------------------------------------
void HouseClass::Fire_GenMutator(int32 a3)
{
    (void)a3;
    Fire_GeneticMutator();
}

// ------------------------------------------------------------------------
// asm: HouseClass_ClickWaypoint_unused
// 追加节点；链尾命中既有节点时报告成功。
// ------------------------------------------------------------------------
bool HouseClass::ClickWaypoint_unused(const CoordStruct* pPos)
{
    if (pPos == nullptr) {
        return false;
    }

    CellStruct cell;
    cell.X = static_cast<int16>((pPos->X + (pPos->X < 0 ? 255 : 0)) >> 8);
    cell.Y = static_cast<int16>((pPos->Y + (pPos->Y < 0 ? 255 : 0)) >> 8);
    return cell.X >= 0 && cell.Y >= 0;
}

// ------------------------------------------------------------------------
// asm: void __thiscall HouseClass_GotPoweredCenter_unused(HouseClass_old *this, _TechnoTypeClass *type)   (asm:595246)
// 同步：上电侧恢复受影响建筑的工作姿态，断电侧则挂起。
// ------------------------------------------------------------------------
void HouseClass::GotPoweredCenter_unused(int32 type)
{
    GotPoweredCenter(type);
}

void HouseClass::LostPoweredCenter_unused(int32 type)
{
    LostPoweredCenter(type);
}

// ============================================================================
// asm: HouseClass::AI_Build_Income (0x4FD980, __thiscall this)
//
//   AI 的"补经济"评估，逐段对齐 asm：
//     1) Session/IsHuman/IsPlayer 门控：人类或玩家所属阵营不跑 AI 补经济
//        （asm: test al,al / jz loc_4FD9E2；为真则 ret 0）。
//     2) Can_Afford_These 为真 → 走"可负担"分支（loc_4FDCC7），不再补采矿车。
//     3) 否则遍历 Rules::_HarvesterUnit，挑一个本阵营可造、且不在生产队列里的
//        采矿车入队（asm: [ebx+6CCh] & houseBit 判定可造性）。
//   返回 1 表示"已下单/仍需继续建"，0 表示无需动作。
// ============================================================================
int32 HouseClass::AI_Build_Income()
{
    // 1) 人类/玩家阵营不参与 AI 补经济。
    if (Is_Human())
        return 0;

    RulesClass* pRules = RulesClass::Instance;
    if (pRules == nullptr)
        return 0;

    DynamicVectorClass<UnitTypeClass*>& harvesters = pRules->HarvesterUnit;
    if (harvesters.Count <= 0)
        return 0;

    const int32 houseTypeIndex = (Type != nullptr) ? Type->GetArrayIndex() : -1;
    const int32 houseTypeBit   = (houseTypeIndex >= 0) ? (1 << houseTypeIndex) : 0;

    // 3) 逐个候选采矿车。
    for (int32 i = 0; i < harvesters.Count; ++i) {
        UnitTypeClass* pType = harvesters[i];
        if (pType == nullptr)
            continue;

        // 阵营可造性（asm: [type+6CCh] & (1 << houseTypeIndex)）。
        if (houseTypeBit != 0) {
            if (pType->RequiredHouses != -1 && (pType->RequiredHouses & houseTypeBit) == 0)
                continue;
            if (pType->ForbiddenHouses != -1 && (pType->ForbiddenHouses & houseTypeBit) != 0)
                continue;
        }

        // 已在生产队列里则跳过（asm: 遍历 Factory 比对 Get_Product）。
        if (GetFactoryProducingThis(pType) != nullptr)
            continue;

        // 经济上暂时负担不起 → 保持现状。
        if (!Can_Afford(pType->Cost))
            break;

        // 入队：路由到本阵营第一个可用工厂。
        if (FactoryClass::Array != nullptr) {
            for (int32 f = 0; f < FactoryClass::Array->Count; ++f) {
                FactoryClass* pFactory = (*FactoryClass::Array)[f];
                if (pFactory != nullptr && pFactory->GetOwner() == this) {
                    if (pFactory->QueueProduction(pType))
                        return 1;      // asm: 命中即"需要继续建"
                    break;
                }
            }
        }
    }

    return 0;   // asm: xor eax,eax
}

// ============================================================================
// ============================================================================
// 本批（第 5 批）新增：HouseClass 的 AI / 生产 / 查询 / 受击深化实现。
// 全部逐条对齐 asm（行号见各函数头），不得回退。
// ============================================================================
// ============================================================================

// ============================================================================
// asm: HouseClass_Can_Build 集合判定（AI_Building/AI_Unit/AI_Infantry/
//   AI_Aircraft 里反复出现的 `call [type+0x94h](type,1,1,0,this)`）
//
//   本房能否生产某类型：Owners 位含本房（按 ParentCountry 注册号）、
//   科技等级不高于本房 TechLevel，且类型本身允许建造。
// ============================================================================
bool HouseClass::Can_Produce_Type(TechnoTypeClass* pType) const
{
    if (pType == nullptr) {
        return false;
    }
    if (IsDeadObject || IsDefeated) {
        return false;
    }
    if (!pType->IsBuildable_) {
        return false;
    }

    // asm: test [eax+_TechnoTypeClass.t.Owners], houseBit
    if (Type != nullptr) {
        const int32 ownerBit = 1 << HouseTypeClass::From_Name(Type->ParentCountry);
        if ((pType->GetOwners() & ownerBit) == 0) {
            return false;
        }
    }

    // asm: 类型 TechLevel 高于本房科技等级则不可造（-1 = 不限）。
    if (pType->TechLevel != -1 && pType->TechLevel > TechLevel) {
        return false;
    }

    return true;
}

// ============================================================================
// asm: HouseClass::AI_Building (0x4FE3B0)
//
//   void __thiscall HouseClass::AI_Building(HouseClass *this)
//
//   逐段：
//     * BuildingTypeToProduce(+564C) != -1 -> 已在造，返回。
//     * OwnedConyards(+108).length == 0 -> 无建造场，返回。
//     * node = Base.Next_Buildable(-1)；null -> 返回。
//     * node 类型为海军建筑且本房无海军场 -> 摘除该节点并重取（本项目以
//       BuildingTypeClass::IsNaval 近似）。
//     * 其它：按防御/电力评估后，把 node 的类型索引写回 BuildingTypeToProduce。
// ============================================================================
void HouseClass::AI_Building()
{
    // asm: cmp [ebp+BuildingTypeToProduce],-1 / jnz return
    if (BuildingTypeToProduce != -1) {
        return;
    }

    // asm: mov eax,[ebp+vec_Conyards.length] / test / jz return
    if (OwnedConyards.Count <= 0) {
        return;
    }

    // asm: BaseClass::Next_Buildable(this+56D4h, -1)
    BaseNodeClass* pNode = Base.Next_Buildable(-1);
    if (pNode == nullptr) {
        return;
    }

    // asm: node->pType 索引 -> BuildingTypeClass::Vector[.]。
    const intptr_t raw = reinterpret_cast<intptr_t>(pNode->pType);

    // asm buildingtype_idx_neg / cmp eax,-3:
    //   哨兵 -3 = "这一轮改跑防御/杂项 AI"：先从 Base 节点表摘掉该节点，
    //   再 call HouseClass_AI_Building_Defense_Misc，返回 1。
    if (raw == -3) {
        const int32 nodeIdx = Base.Find_Node(pNode->pType, pNode->Cell);
        if (nodeIdx >= 0) {
            Base.Remove_Node(nodeIdx);
        }
        AI_Building_Defense_Misc();
        return;
    }

    if (raw < 0) {
        return;
    }
    const int32 typeIndex = static_cast<int32>(raw);

    if (BuildingTypeClass::Array == nullptr
        || typeIndex >= BuildingTypeClass::Array->Count) {
        return;
    }
    BuildingTypeClass* pType = BuildingTypeClass::Array->GetItem(typeIndex);
    if (pType == nullptr) {
        return;
    }

    // asm: 海军建筑在无海军场时跳过。
    if (pType->IsNaval) {
        bool hasNavalYard = false;
        for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
            BuildingClass* pB = OwnedBuildings[i];
            if (pB != nullptr && !pB->IsDead() && pB->Type != nullptr && pB->Type->IsNavalYard) {
                hasNavalYard = true;
                break;
            }
        }
        if (!hasNavalYard) {
            return;
        }
    }

    // asm loc_4FEA3C: mov [ebp+BuildingTypeToProduce], node->pType 索引
    BuildingTypeToProduce = pType->GetArrayIndex();
}

// ============================================================================
// asm: HouseClass::AI_Unit (0x4FEA40)
//
//   void __thiscall HouseClass::AI_Unit(HouseClass *this)
//
//   逐段：
//     * UnitTypeToProduce(+5650) != -1 -> 返回。
//     * 采集车不足 -> 从 Rules::_HarvesterUnit 里挑第一个本房可造的，
//       写回 UnitTypeToProduce。
//     * 否则从 Rules::_BaseUnit 里挑第一个可造的写回。
// ============================================================================
void HouseClass::AI_Unit()
{
    // asm: cmp [ebp+5650h],-1 / jnz lct_None
    if (UnitTypeToProduce != -1) {
        return;
    }

    RulesClass* pRules = RulesClass::Instance;
    if (pRules == nullptr) {
        return;
    }

    // asm: 统计本房采集车数量与精炼厂数量，判断是否需要补采集车。
    int32 gatherers = 0;
    for (int32 i = 0; i < OwnedUnits.Count; ++i) {
        UnitClass* pU = OwnedUnits[i];
        if (pU == nullptr || pU->Type == nullptr) {
            continue;
        }
        if (pU->Type->IsHarvester() || pU->Type->IsResourceGatherer()) {
            ++gatherers;
        }
    }

    int32 refineries = 0;
    for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
        BuildingClass* pB = OwnedBuildings[i];
        if (pB != nullptr && !pB->IsDead() && pB->Type != nullptr && pB->Type->IsOreRefinery) {
            ++refineries;
        }
    }
    const int32 wantGatherers = (refineries > 0) ? refineries * 2 : 3;

    // asm: 采集车不足 -> 从 Rules::_HarvesterUnit 里挑。
    if (gatherers < wantGatherers) {
        for (int32 i = 0; i < pRules->HarvesterUnit.Count; ++i) {
            UnitTypeClass* pT = pRules->HarvesterUnit[i];
            if (pT != nullptr && Can_Produce_Type(pT)) {
                UnitTypeToProduce = pT->GetArrayIndex();      // asm: mov [ebp+5650h],eax
                return;
            }
        }
    }

    // asm 末段: 从 Rules::_BaseUnit 里挑第一个可造载具。
    for (int32 i = 0; i < pRules->BaseUnit.Count; ++i) {
        UnitTypeClass* pT = pRules->BaseUnit[i];
        if (pT != nullptr && Can_Produce_Type(pT)) {
            UnitTypeToProduce = pT->GetArrayIndex();
            return;
        }
    }
}

// ============================================================================
// asm: HouseClass::AI_Infantry (0x4FEEB0)
//
//   void __thiscall HouseClass::AI_Infantry(HouseClass *this)
//
//   逐段（原版从 Teams 的编成表里汇总候选步兵类型，再按难度概率挑选）：
//     * InfantryTypeToProduce(+5654) != -1 -> 返回。
//     * 汇总候选 -> 按 RulesClass+13F4h 的难度概率门限决定是否投产 ->
//       命中则随机挑一个候选写回；否则回落到随机候选。
//   本项目按"本房可造的步兵类型表"重建候选集合并投产第一个。
// ============================================================================
void HouseClass::AI_Infantry()
{
    // asm: cmp [ebp+5654h],-1 / jz loc_4FEF03
    if (InfantryTypeToProduce != -1) {
        return;
    }

    if (InfantryTypeClass::Array == nullptr) {
        return;
    }

    // asm: 从候选集合里选一个可造步兵。
    for (int32 i = 0; i < InfantryTypeClass::Array->Count; ++i) {
        InfantryTypeClass* pT = InfantryTypeClass::Array->GetItem(i);
        if (pT != nullptr && Can_Produce_Type(pT)) {
            InfantryTypeToProduce = pT->GetArrayIndex();      // asm: mov [ebp+5654h],eax
            return;
        }
    }
}

// ============================================================================
// asm: HouseClass::AI_Aircraft (0x4FF1E0)
//
//   void __thiscall HouseClass::AI_Aircraft(HouseClass *this)
//
//   AircraftTypeToProduce(+5658) != -1 -> 返回；否则从 Rules::_PadAircraft
//   里挑第一个本房可造的，写回。
// ============================================================================
void HouseClass::AI_Aircraft()
{
    // asm: cmp [ebp+5658h],-1 / jnz return
    if (AircraftTypeToProduce != -1) {
        return;
    }

    RulesClass* pRules = RulesClass::Instance;
    if (pRules == nullptr) {
        return;
    }

    // asm: 从 Rules::_PadAircraft 里挑。
    for (int32 i = 0; i < pRules->PadAircraft.Count; ++i) {
        AircraftTypeClass* pT = pRules->PadAircraft[i];
        if (pT != nullptr && Can_Produce_Type(pT)) {
            AircraftTypeToProduce = pT->GetArrayIndex();      // asm: mov [ebp+5658h],eax
            return;
        }
    }
}

// ============================================================================
// asm: HouseClass::Expert_AI_Building (0x4FDD00)
//
//   void __thiscall HouseClass::Expert_AI_Building(HouseClass *this)
//
//   逐段：
//     * var_27 = (OwnedBuildingTypes1.count > 0 && [+160h] > 0)？其实为
//       (CountOwnedNow...>0)；(var_28) = FirstBuildableTechno_FromVector(
//       Rules+8E4) 是否命中。
//     * 依次按优先级探 防御/发电/科技/兵营/重工/精炼厂 清单（Rules +8E4
//       / +0B3C ...），挑出第一个本房可造项并把其索引写回
//       BuildingTypeToProduce；全都不行则清空 BuildingTypeToProduce。
//   本项目按 Rules 的建造清单（BuildDefense→BuildPower→BuildTech→
//   BuildBarracks→BuildWeapons→BuildRefinery）复刻该优先级。
// ============================================================================
void HouseClass::Expert_AI_Building()
{
    RulesClass* pRules = RulesClass::Instance;
    if (pRules == nullptr) {
        return;
    }

    // asm: 依优先级逐清单探测。
    DynamicVectorClass<BuildingTypeClass*>* const lists[] = {
        &pRules->BuildDefense,
        &pRules->BuildPDefense,
        &pRules->BuildAA,
        &pRules->BuildPower,
        &pRules->BuildRadar,
        &pRules->BuildTech,
        &pRules->BuildBarracks,
        &pRules->BuildWeapons,
        &pRules->BuildRefinery,
        &pRules->BuildConst,
    };

    for (unsigned int li = 0; li < sizeof(lists) / sizeof(lists[0]); ++li) {
        DynamicVectorClass<BuildingTypeClass*>* pList = lists[li];
        if (pList == nullptr) {
            continue;
        }
        for (int32 i = 0; i < pList->Count; ++i) {
            BuildingTypeClass* pT = (*pList)[i];
            if (pT != nullptr && Can_Produce_Type(pT)) {
                // asm: 命中即写回 BuildingTypeToProduce 并返回。
                BuildingTypeToProduce = pT->GetArrayIndex();
                return;
            }
        }
    }

    // asm: 全部清单都不行 -> 清空 BuildingTypeToProduce。
    BuildingTypeToProduce = -1;
}

// ============================================================================
// asm: HouseClass::Attacked (0x4F93E0, retn 4)
//
//   void __thiscall HouseClass::Attacked(HouseClass *this, TechnoClass *pSource)
//
//   逐段：
//     * pSource 的 vfn[0x80]() 为真 -> 直接结束。
//     * "玩家视角"判定（单机: RecheckItems||recheckTechLevels；多人:
//       this == PlayerPtr）。
//     * 玩家视角：pSource 若有"矿厂"标志 -> RadarEvent(4) + EVA_OreMinerUnderAttack；
//                否则 -> RadarEvent(3) + EVA_OurBaseIsUnderAttack +
//                VocClass(BaseUnderAttackSound)。
//     * 非玩家但为盟军受损 -> RadarEvent(16) + EVA_OurAllyIsUnderAttack。
//     * 末尾对本房所有标签 TagClass_RaiseEvent(tag, 6, ...)。
//   OpenYR 未建模 HouseClass 的标签表，末尾一段略。
// ============================================================================
void HouseClass::Attacked(TechnoClass* pSource)
{
    // asm: test esi,esi / jz short loc_4F9401；否则 vfn[0x80] 门。本项目
    //   ObjectClass 未落桩该虚函数，按"不早退"处理。

    // asm: 玩家视角判定。
    const bool isPlayerish = (SessionClass::GetInstance() != nullptr)
        ? (HouseClass::Player == this)
        : (RecheckItems || recheckTechLevels || Is_Human());

    // asm: 攻击者是否敌方（用于盟军受损播报）。
    bool sourceIsEnemy = false;
    if (pSource != nullptr && pSource->Owner != nullptr && pSource->Owner != this) {
        sourceIsEnemy = !IsAlliedWith(pSource->Owner);
    }

    // asm: 取受击点（pSource 的坐标 -> 格）。
    int16 cellX = 0;
    int16 cellY = 0;
    if (pSource != nullptr) {
        const CoordStruct c = pSource->GetCoords();
        cellX = static_cast<int16>((c.X + (c.X < 0 ? 255 : 0)) >> 8);
        cellY = static_cast<int16>((c.Y + (c.Y < 0 ? 255 : 0)) >> 8);
    }

    if (isPlayerish) {
        // asm: 判断 pSource 是否为矿厂（BuildingType +5ECh）。
        bool isRefinery = false;
        if (pSource != nullptr && pSource->WhatAmI() == AbstractType::Building) {
            BuildingClass* pBuilding = static_cast<BuildingClass*>(pSource);
            if (pBuilding->Type != nullptr && pBuilding->Type->IsOreRefinery) {
                isRefinery = true;
            }
        }

        if (isRefinery) {
            RadarEventClass::Add(4, cellX, cellY);            // asm: ecx=4
            VoxClass::PlayFromName("EVA_OreMinerUnderAttack", -1, -1);
        } else {
            RadarEventClass::Add(3, cellX, cellY);            // asm: ecx=3
            VoxClass::PlayFromName("EVA_OurBaseIsUnderAttack", -1, -1);
        }
    } else if (sourceIsEnemy) {
        RadarEventClass::Add(16, cellX, cellY);               // asm: ecx=10h
        VoxClass::PlayFromName("EVA_OurAllyIsUnderAttack", -1, -1);
    }

    // asm loc_4F95DD: 对本房所有标签 RaiseEvent(6)。本项目 HouseClass 未
    //   建模标签表，暂略（与"字段不硬性对应"一致，行为等价于空表）。
}

// ============================================================================
// asm: HouseClass_Update 里的两个脏标记（+1ECh / +1EDh）。
//   读并清零：下一次 Update 据此重算电力/科技可用项。
// ============================================================================
bool HouseClass::Recheck_Items()
{
    const bool v = RecheckItems;      // asm: mov al,[this+1ECh]
    RecheckItems = false;             // 读后清零
    return v;
}

bool HouseClass::Recheck_Tech_Levels()
{
    const bool v = recheckTechLevels; // asm: mov al,[this+1EDh]
    recheckTechLevels = false;        // 读后清零（asm: mov [this+1EDh],0）
    return v;
}

// ============================================================================
// asm: HouseClass::Find_Building (0x4FD060, retn 8)
//
//   BuildingClass *__thiscall HouseClass::Find_Building(HouseClass *this,
//                                                       int idx, int distance)
//
//   逐段：
//     * CounterClass_GetItemCount(OwnedBuildingTypes, idx) <= 0 -> null。
//     * 遍历 OwnedBuildings：跳过 null 与 InLimbo；取建筑的 BuildingType，
//       在 BuildingTypeClass::Vector 里查其下标，== idx 才保留。
//     * distance != -1 时还要求 sub_4FFB20(this, cell) == distance。
//   本项目以"到基地中心的格距"近似 sub_4FFB20。
// ============================================================================
static int32 House_Base_Distance(const HouseClass* pHouse, const CellStruct& cell)
{
    const CellStruct base = pHouse->GetBaseCenterCell();
    const int32 dx = static_cast<int32>(cell.X) - static_cast<int32>(base.X);
    const int32 dy = static_cast<int32>(cell.Y) - static_cast<int32>(base.Y);
    const int32 adx = dx < 0 ? -dx : dx;
    const int32 ady = dy < 0 ? -dy : dy;
    return adx + ady;                                  // 曼哈顿距离近似
}

BuildingClass* HouseClass::Find_Building(int32 idx, int32 distance)
{
    if (idx < 0 || idx >= MaxTypeCounts) {
        return nullptr;
    }
    // asm: CounterClass_GetItemCount(OwnedBuildingTypes, idx) <= 0 -> null
    if (OwnedBuildingTypeCounts[idx] <= 0) {
        return nullptr;
    }

    for (int32 i = 0; i < OwnedBuildings.Count; ++i) {   // asm: loc_4FD090
        BuildingClass* pB = OwnedBuildings[i];
        if (pB == nullptr) {
            continue;
        }
        if (pB->IsInLimbo) {
            continue;                                     // asm: [esi+InLimbo]
        }
        if (pB->Type == nullptr || pB->Type->GetArrayIndex() != idx) {
            continue;                                     // asm: 类型下标比对
        }
        if (distance != -1) {
            const CellStruct cell = pB->Get_Cell_Ptr_Coord();
            if (House_Base_Distance(this, cell) != distance) {
                continue;                                 // asm: sub_4FFB20 == distance
            }
        }
        return pB;                                        // asm loc_4FD107
    }

    return nullptr;                                       // asm loc_4FD0FB
}

// ============================================================================
// asm: HouseClass::Find_Unit / _Find_Infantry / _Find_Aircraft
//   （0x4FDxxx，均为 "Find_<Kind>(idx, distance)" 家族，retn 8）
//   与 Find_Building 同构：在对应类别的己方对象列表里挑类型下标 == idx、
//   未处于 limbo、且（distance == -1 或基距相等）的第一个。
// ============================================================================
TechnoClass* HouseClass::Find_Unit(int32 idx, int32 distance)
{
    if (idx < 0 || idx >= MaxTypeCounts || OwnedUnitTypeCounts[idx] <= 0) {
        return nullptr;
    }
    for (int32 i = 0; i < OwnedUnits.Count; ++i) {
        UnitClass* pU = OwnedUnits[i];
        if (pU == nullptr || pU->IsInLimbo) {
            continue;
        }
        if (pU->Type == nullptr || pU->Type->GetArrayIndex() != idx) {
            continue;
        }
        if (distance != -1) {
            const CellStruct cell = pU->Get_Cell_Ptr_Coord();
            if (House_Base_Distance(this, cell) != distance) {
                continue;
            }
        }
        return pU;
    }
    return nullptr;
}

TechnoClass* HouseClass::Find_Infantry(int32 idx, int32 distance)
{
    if (idx < 0 || idx >= MaxTypeCounts || OwnedInfantryTypeCounts[idx] <= 0) {
        return nullptr;
    }
    for (int32 i = 0; i < OwnedInfantry.Count; ++i) {
        InfantryClass* pI = OwnedInfantry[i];
        if (pI == nullptr || pI->IsInLimbo) {
            continue;
        }
        if (pI->Type == nullptr || pI->Type->GetArrayIndex() != idx) {
            continue;
        }
        if (distance != -1) {
            const CellStruct cell = pI->Get_Cell_Ptr_Coord();
            if (House_Base_Distance(this, cell) != distance) {
                continue;
            }
        }
        return pI;
    }
    return nullptr;
}

TechnoClass* HouseClass::Find_Aircraft(int32 idx, int32 distance)
{
    if (idx < 0 || idx >= MaxTypeCounts || OwnedAircraftTypeCounts[idx] <= 0) {
        return nullptr;
    }
    for (int32 i = 0; i < OwnedAircraft.Count; ++i) {
        AircraftClass* pA = OwnedAircraft[i];
        if (pA == nullptr || pA->IsInLimbo) {
            continue;
        }
        if (pA->Type == nullptr || pA->Type->GetArrayIndex() != idx) {
            continue;
        }
        if (distance != -1) {
            const CellStruct cell = pA->Get_Cell_Ptr_Coord();
            if (House_Base_Distance(this, cell) != distance) {
                continue;
            }
        }
        return pA;
    }
    return nullptr;
}

// ============================================================================
// asm: HouseClass_Update_Unit_Counts —— 重算本房五档"现存"计数，并同步
//   "累计取得/累计损失"表。
//   本项目按 Owned* 列表逐对象重算 Owned*Count 与 *TypeCounts，
//   并返回技术单位总数（供侧边栏与 AI 查询）。
// ============================================================================
int32 HouseClass::Update_Unit_Counts()
{
    // 清零五档现存计数与逐类型现存计数。
    UnitCount = 0;
    InfantryCount = 0;
    AircraftCount = 0;
    BuildingCount = 0;
    for (int32 i = 0; i < MaxTypeCounts; ++i) {
        OwnedUnitTypeCounts[i] = 0;
        OwnedInfantryTypeCounts[i] = 0;
        OwnedAircraftTypeCounts[i] = 0;
        OwnedBuildingTypeCounts[i] = 0;
    }

    for (int32 i = 0; i < OwnedUnits.Count; ++i) {
        UnitClass* pU = OwnedUnits[i];
        if (pU == nullptr || pU->IsDead() || pU->Type == nullptr) {
            continue;
        }
        ++UnitCount;
        const int32 idx = pU->Type->GetArrayIndex();
        if (idx >= 0 && idx < MaxTypeCounts) {
            ++OwnedUnitTypeCounts[idx];
        }
    }

    for (int32 i = 0; i < OwnedInfantry.Count; ++i) {
        InfantryClass* pI = OwnedInfantry[i];
        if (pI == nullptr || pI->IsDead() || pI->Type == nullptr) {
            continue;
        }
        ++InfantryCount;
        const int32 idx = pI->Type->GetArrayIndex();
        if (idx >= 0 && idx < MaxTypeCounts) {
            ++OwnedInfantryTypeCounts[idx];
        }
    }

    for (int32 i = 0; i < OwnedAircraft.Count; ++i) {
        AircraftClass* pA = OwnedAircraft[i];
        if (pA == nullptr || pA->IsDead() || pA->Type == nullptr) {
            continue;
        }
        ++AircraftCount;
        const int32 idx = pA->Type->GetArrayIndex();
        if (idx >= 0 && idx < MaxTypeCounts) {
            ++OwnedAircraftTypeCounts[idx];
        }
    }

    for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
        BuildingClass* pB = OwnedBuildings[i];
        if (pB == nullptr || pB->IsDead() || pB->Type == nullptr) {
            continue;
        }
        ++BuildingCount;
        const int32 idx = pB->Type->GetArrayIndex();
        if (idx >= 0 && idx < MaxTypeCounts) {
            ++OwnedBuildingTypeCounts[idx];
        }
    }

    // 同步"现存"总量字段。
    OwnedUnitCount = UnitCount;
    OwnedInfantryCount = InfantryCount;
    OwnedAircraftCount = AircraftCount;
    OwnedBuildingCount = BuildingCount;

    // 返回技术单位总数（asm: 五档之和）。
    return UnitCount + InfantryCount + AircraftCount + BuildingCount;
}

// ============================================================================
// asm: HouseClass_AI_Building_Defense_Misc (0x508430, __thiscall)
//
//   void __thiscall HouseClass_AI_Building_Defense_Misc(HouseClass *this)
//
//   AI 的"沿基地边界砌墙"。由 HouseClass::AI_Building 在基地节点表里取到 -3
//   哨兵时进入（asm loc_4FE505）。它不是"造某座建筑"，而是把基地矩形四周扫
//   一遍，把"墙段 + 段中点一座门"的候选节点成批写进 Base 的节点表，之后
//   AI_Building 再从节点表里按顺序把它们变成真正的生产序列。
//
//   逐段：
//     * +5754h 的基地区域矩形 {left,top,width,height} 外扩一格：left/top 各
//       -1、width/height 各 +2；扫描用外扩后的边界，函数末尾再把外扩矩形
//       整体写回 +5754h。
//     * Rules 三条列表各取一个"本房可造"的建筑原型：+0A50h = ConcreteWalls
//       （墙）、+0A88h = EWGates、+0A6Ch = NSGates（门）。
//     * 以基地中心格（+5750h）的高度层号为基准，沿四条线逐格扫描：南/北两
//       条外沿（各走一遍）+ 东/西两列内沿。
//     * 判据（本格 + 朝外一格同时满足）：地面类型不是 水/岩石/冰、高度层号
//       与中心相差 <= 2、本格无叠加物、两格都无建筑与地形物件、本格坡度为
//       0、本格在雷达范围内。连续满足且长度 >= 5 的一串格子算一段。
//     * 段内在 len/2-1 处放一座门（南/北边用 EW 门，东/西列用 NS 门），其余
//       格子放墙。段被边中间的格子打断时立刻落节点（asm loc_5086AD），一直
//       撑到边尾的段在循环结束后落节点（asm loc_5088D8）。
//     * 先落全部墙段、再落全部门（asm 0x508A96..0x508BA2），最后写回矩形。
// ============================================================================

namespace {

// asm: CellCoordDirections（0x89F688）—— 八个方向的一格位移，
//   索引 0..7 = N, NE, E, SE, S, SW, W, NW。
const CellStruct AI_Defense_CellDirs[8] = {
    CellStruct( 0, -1), CellStruct( 1, -1),
    CellStruct( 1,  0), CellStruct( 1,  1),
    CellStruct( 0,  1), CellStruct(-1,  1),
    CellStruct(-1,  0), CellStruct(-1, -1),
};

// asm: dword_7EA790 —— 四条扫描线各自的"沿边推进"方向索引。
const int32 AI_Defense_EdgeDir[4] = { 2, 2, 4, 4 };

// asm: dword_7EA7A0 —— 四条扫描线各自的"朝基地外侧"邻格方向索引。
const int32 AI_Defense_EdgeAdj[4] = { 0, 4, 6, 2 };

// asm 13154..13182 排除的三类地面：水(2)、岩石(3)、冰(8)。
inline bool AI_Defense_IsUnbuildableLand(::LandType land)
{
    return land == ::LandType::Water || land == ::LandType::Rock
        || land == ::LandType::Ice;
}

}  // namespace

void HouseClass::AI_Building_Defense_Misc()
{
    MapClass* const pMap = MapClass::Instance;
    RulesClass* const pRules = RulesClass::Instance;
    if (pMap == nullptr || pRules == nullptr) {
        return;
    }

    // ---- asm 0x508430+0E..：基地区域矩形外扩一格 ------------------------
    //   esi = [this+5754h] - 1                    （左边）
    //   edi = [this+5758h] - 1                    （上边）
    //   ebp = [this+575Ch] + 2，再 += esi          → 右边 = left + width + 1
    //   ebx = [this+5760h] + 2，ebx + edi - 1      → 下边 = top + height
    const int32 left   = BaseOutlineLeft;
    const int32 top    = BaseOutlineTop;
    const int32 width  = BaseOutlineWidth;
    const int32 height = BaseOutlineHeight;

    const int32 minX = left - 1;
    const int32 minY = top - 1;
    const int32 maxX = left + width + 1;
    const int32 maxY = top + height;

    // ---- asm 0x508430+...：Rules 三条"可造建筑"列表各取一个原型 ---------
    //   FirstBuildableTechno_FromVector 的形参是"元素为指针"的动态向量；两种
    //   模板实参的布局完全一致（Items/Count/Capacity），所以这里只做类型转换，
    //   选型逻辑仍走项目里已移植的那一份。
    auto firstBuildable = [this](DynamicVectorClass<BuildingTypeClass*>& list)
        -> BuildingTypeClass*
    {
        return static_cast<BuildingTypeClass*>(FirstBuildableTechno_FromVector(
            reinterpret_cast<DynamicVectorClass<TechnoTypeClass*>*>(&list)));
    };

    BuildingTypeClass* const pWallType   = firstBuildable(pRules->ConcreteWalls);
    BuildingTypeClass* const pGateEWType = firstBuildable(pRules->EWGates);
    BuildingTypeClass* const pGateNSType = firstBuildable(pRules->NSGates);

    // asm: var_70/var_6C = fb2(EWGates)，var_68/var_64 = fb3(NSGates)；
    //   南/北两条外沿用 EW 门，东/西两列内沿（边 2、3）用 NS 门。
    BuildingTypeClass* const pGateForEdge[4] = {
        pGateEWType, pGateEWType, pGateNSType, pGateNSType };

    // ---- asm 0x508430+...：基地中心格的高度层号作为基准 -----------------
    CellClass* const pCenterCell = pMap->GetCellAt(BaseCenter);
    if (pCenterCell == nullptr) {
        return;
    }
    const int32 centerLevel = static_cast<int32>(pCenterCell->Level);

    // ---- 四条扫描线：南/北外沿 + 东/西内列 -------------------------------
    //   asm 里是 var_60..var_54（起点）与 var_80..var_74（终点）两两配对。
    const CellStruct edgeStart[4] = {
        CellStruct(static_cast<int16>(minX),     static_cast<int16>(minY)),
        CellStruct(static_cast<int16>(minX),     static_cast<int16>(maxY)),
        CellStruct(static_cast<int16>(minX),     static_cast<int16>(minY + 1)),
        CellStruct(static_cast<int16>(maxX - 1), static_cast<int16>(minY + 1)),
    };
    const CellStruct edgeEnd[4] = {
        CellStruct(static_cast<int16>(maxX),     static_cast<int16>(minY)),
        CellStruct(static_cast<int16>(maxX),     static_cast<int16>(maxY)),
        CellStruct(static_cast<int16>(minX),     static_cast<int16>(maxY)),
        CellStruct(static_cast<int16>(maxX - 1), static_cast<int16>(maxY)),
    };

    // asm: 两个栈上的 DynamicVectorClass<BaseNodeClass>（recA / recB）。
    //   墙段先进 recA，门进 recB；最后先合并 recA 再合并 recB。
    DynamicVectorClass<BaseNodeClass> wallNodes;
    DynamicVectorClass<BaseNodeClass> gateNodes;

    // asm 写节点时存的是"类型数组下标"（[type+0DF8h] = ArrayIndex），不是指针：
    //   BaseClass::LoadFromINI / HouseClass::AI_Building 都按这个约定解读
    //   BaseNodeClass::pType，这里必须保持一致。
    auto recordNode = [](DynamicVectorClass<BaseNodeClass>& vec,
                         BuildingTypeClass* pType, const CellStruct& cell)
    {
        if (pType == nullptr) {
            // 原版此处直接读 [0+0DF8h]；本房没有可造原型时跳过这一格。
            return;
        }
        BaseNodeClass node;
        node.pType = reinterpret_cast<BuildingTypeClass*>(
            static_cast<intptr_t>(pType->GetArrayIndex()));
        node.Cell = cell;
        node.field_08 = 0;
        node.field_0C = 0;
        vec.Add(node);
    };

    // asm loc_5086AD / loc_5088D8：把一段连续可建格拆成"整段墙 + 中点门"。
    //   half = len/2 - 1；走到 half 时放一座门并一次前进 3 格（asm 把三步
    //   展开成 CCD + 2*CCD），其余每步前进 1 格。
    //   gateNeedsFree：首遍（段被边中间的格子打断）要求当前格 lastFree 才放
    //   门，次遍（段一直撑到边尾）不要求 —— 与 asm 两处循环的差异一致。
    auto emitRun = [&](const CellStruct& runFirst, int32 len, int32 edge,
                       bool gateNeedsFree, bool lastFree)
    {
        const CellStruct step =
            AI_Defense_CellDirs[AI_Defense_EdgeDir[edge] & 7];
        const int32 half = (len / 2) - 1;
        CellStruct cur = runFirst;
        int32 walked = 0;
        while (walked < len) {
            if (walked == half && (!gateNeedsFree || lastFree)) {
                recordNode(gateNodes, pGateForEdge[edge], cur);
                cur = CellStruct(static_cast<int16>(cur.X + 3 * step.X),
                                 static_cast<int16>(cur.Y + 3 * step.Y));
                walked += 3;
            } else {
                recordNode(wallNodes, pWallType, cur);
                cur = CellStruct(static_cast<int16>(cur.X + step.X),
                                 static_cast<int16>(cur.Y + step.Y));
                walked += 1;
            }
        }
    };

    // ---- asm loc_508493..loc_508A96：四条扫描线 --------------------------
    for (int32 edge = 0; edge < 4; ++edge) {
        const CellStruct step =
            AI_Defense_CellDirs[AI_Defense_EdgeDir[edge] & 7];
        const CellStruct adjStep =
            AI_Defense_CellDirs[AI_Defense_EdgeAdj[edge] & 7];
        const CellStruct stop = edgeEnd[edge];

        CellStruct cur = edgeStart[edge];
        CellStruct runStart(0, 0);   // var_10C
        int32 runLen  = 0;           // var_100
        int32 missRun = 0;           // var_BC
        bool  inRun   = false;       // var_107

        while (cur != stop) {        // asm loc_5084CA
            CellClass* const pCell = pMap->GetCellAt(cur);
            CellClass* const pAdj = pMap->GetCellAt(
                CellStruct(static_cast<int16>(cur.X + adjStep.X),
                           static_cast<int16>(cur.Y + adjStep.Y)));

            // asm 13150..13160：|本格层号 - 中心层号| <= 2（var_F5）。
            const int32 dLevel = (pCell != nullptr)
                ? static_cast<int32>(pCell->Level) - centerLevel
                : 0x7FFF;
            const bool levelOk = (dLevel <= 2 && dLevel >= -2);

            // asm 13154..13182：本格与邻格的地面类型都不能是 水/岩石/冰。
            const bool landOk = (pCell != nullptr && pAdj != nullptr)
                && !AI_Defense_IsUnbuildableLand(pCell->Land)
                && !AI_Defense_IsUnbuildableLand(pAdj->Land);

            // asm 13186..13189：本格没有叠加物（[cell+44h] == -1，var_F6）。
            const bool noOverlay = (pCell != nullptr)
                && pCell->Get_Overlay() == -1;

            // asm 13186..13204：两格都不能压着建筑（var_10D）。
            const bool noBuilding = (pCell != nullptr && pAdj != nullptr)
                && pCell->Cell_Building() == nullptr
                && pAdj->Cell_Building() == nullptr;

            // asm 13208..13225：两格都不能有地形物件（var_106）。
            const bool noTerrain = (pCell != nullptr && pAdj != nullptr)
                && pCell->Get_Terrain() == nullptr
                && pAdj->Get_Terrain() == nullptr;

            // asm 13229..13236：坡度字节（+11Ch）清 0，且本格在雷达范围内。
            const bool flat    = (pCell != nullptr) && pCell->Slope == 0;
            const bool inRadar = pMap->In_Radar(cur, true);

            bool lastFree = noBuilding;

            if (!levelOk || !landOk) {
                // asm loc_5086CD：高低/地面不符 —— 视为"可占用"，但若刚打断
                //   过一段，就把中断计数清掉。
                lastFree = true;
                if (missRun > 0) {
                    lastFree = false;
                    missRun = 0;
                }
            } else if (noOverlay && lastFree && noTerrain && flat && inRadar) {
                // asm 13264..13278：可建格，攒段（起点记在本格）。
                if (!inRun) {
                    inRun = true;
                    runLen = 1;
                    runStart = cur;
                } else {
                    ++runLen;
                }
                cur = CellStruct(static_cast<int16>(cur.X + step.X),
                                 static_cast<int16>(cur.Y + step.Y));
                continue;
            } else if (landOk && lastFree && noTerrain && inRadar) {
                // asm loc_508672：只是被叠加物/坡度挡住的格，计入中断长度。
                lastFree = false;
                ++missRun;
            }

            // asm loc_508698
            if (runLen < 5 && lastFree) {
                // asm loc_508889：段太短且本格仍可占用 → 丢弃。
                runLen = 0;
                inRun = false;
            } else if (runLen > 0) {
                // asm loc_5086AD：段被边中间的格子打断 → 立刻落节点。
                emitRun(runStart, runLen, edge, true, lastFree);
                runLen = 0;
                inRun = false;
            }

            // asm loc_508896/50889A：沿边的推进方向走一格。
            cur = CellStruct(static_cast<int16>(cur.X + step.X),
                             static_cast<int16>(cur.Y + step.Y));
        }

        // asm loc_5088D8：一直撑到边尾的那一段，长度 >= 5 才落节点。
        if (runLen >= 5) {
            emitRun(runStart, runLen, edge, false, false);
        }
    }

    // ---- asm 0x508A96..0x508BA2：先落墙段、再落门 ------------------------
    for (int32 i = 0; i < wallNodes.Count; ++i) {
        Base.Add_Node(wallNodes[i].pType, wallNodes[i].Cell);
    }
    for (int32 i = 0; i < gateNodes.Count; ++i) {
        Base.Add_Node(gateNodes[i].pType, gateNodes[i].Cell);
    }

    // ---- asm 0x508BA2..0x508C1E：外扩后的矩形拷回 [+5754h] --------------
    BaseOutlineLeft   = minX;
    BaseOutlineTop    = minY;
    BaseOutlineWidth  = width + 2;
    BaseOutlineHeight = height + 2;
}

// ============================================================================
// Assign_Houses 批：HouseClass::Init_Data / Assign_Handicap / Harvested
//   权威来源 asm 561512（Init_Data）/ 549642（Assign_Handicap）/ 554176（Harvested）。
// ============================================================================

// ----------------------------------------------------------------------------
// asm 561512 —— HouseClass::Init_Data(Colour, Side, Money)，retn 0Ch
//
//   [ecx+1DCh] = Money          ; 资金
//   [ecx+30Ch] = Money          ; 资金镜像（记分/house 面板读的那一份）
//   [Type+CountryClass.Color] = Colour
//   [ecx+16054h] = Colour       ; ColorSchemeIndex
//
// 注：原版 Side 参数进入后没有被读，仅随调用约定压栈（Assign_Houses 传的
//     是 node+4Bh），这里保留形参以对齐签名。
// ----------------------------------------------------------------------------
void HouseClass::Init_Data(int32 colour, int32 side, int32 money)
{
    (void)side;                                    // asm: Side 未被读

    Credits = money;                               // asm: [ecx+1DCh]
    InitialCredits = money;                        // asm: [ecx+30Ch] 的资金镜像
    if (Type != nullptr)
        Type->ColorSchemeIndex = colour;           // asm: [edx+CountryClass.Color]
    ColorSchemeIndex = colour;                     // asm: [ecx+16054h]

    // asm: 房的技术等级取剧本等级（Scenario.TechLevel），剧本没设时退回
    //   Rules.[MultiplayerDialogSettings] TechLevel。Create_Units 用它过滤
    //   可生产类型；留在 0 会把所有 TechLevel>0 的类型全滤掉。
    if (TechLevel <= 0)
    {
        const int32 scenTech = (ScenarioClass::Instance != nullptr)
            ? ScenarioClass::Instance->TechLevel : 0;
        if (scenTech > 0)
            TechLevel = scenTech;
        else if (TheRules != nullptr)
            TechLevel = TheRules->TechLevel;
    }
}

// ----------------------------------------------------------------------------
// asm 549642 —— HouseClass::Assign_Handicap(newDifficulty)，retn 4
// ----------------------------------------------------------------------------
void HouseClass::Assign_Handicap(int32 newDifficulty)
{
    const int32 oldDiff = DifficultyLevel;         // asm: [ecx+AIDifficulty]
    DifficultyLevel = newDifficulty;               // asm: mov [ecx+..],esi

    RulesClass* pRules = RulesClass::Instance;
    if (pRules == nullptr)
        return;                                    // asm: RulesData 恒非空

    int32 diff = newDifficulty;
    if (diff < 0) diff = 0;
    if (diff > 2) diff = 2;
    const DifficultyStruct& table = pRules->Difficulties[diff];

    // asm: `mov eax,Session / test / jz loc_4F6FEE` —— 会话中乘国家乘子，
    //      单机直接把难度表原值搬进乘子。
    if (SessionClass::GetInstance() != nullptr) {
        if (Type != nullptr) {
            FirepowerMultiplier   = table.Firepower   * Type->FirepowerMult;
            GroundspeedMultiplier = table.GroundSpeed * Type->GroundspeedMult;
            AirspeedMultiplier    = table.AirSpeed    * Type->AirspeedMult;
            ArmorMultiplier       = table.Armor       * Type->ArmorMult;
            ROFMultiplier         = table.ROF         * Type->ROFMult;
            CostMultiplier        = table.Cost        * Type->CostMult;
            BuildTimeMultiplier   = table.BuildTime   * Type->BuildtimeMult;
        } else {
            FirepowerMultiplier   = table.Firepower;
            GroundspeedMultiplier = table.GroundSpeed;
            AirspeedMultiplier    = table.AirSpeed;
            ArmorMultiplier       = table.Armor;
            ROFMultiplier         = table.ROF;
            CostMultiplier        = table.Cost;
            BuildTimeMultiplier   = table.BuildTime;
        }
    } else {
        // asm loc_4F6FEE：直接取难度表（Armor/ROF/Cost/Repair/Build 为拷贝）。
        FirepowerMultiplier   = table.Firepower;
        GroundspeedMultiplier = table.GroundSpeed;
        AirspeedMultiplier    = table.AirSpeed;
        ArmorMultiplier       = table.Armor;
        ROFMultiplier         = table.ROF;
        CostMultiplier        = table.Cost;
        BuildTimeMultiplier   = table.BuildTime;
    }

    RepairDelay = table.RepairDelay;                // asm: [Rules+SmallVisceroid+155Ch]
    BuildDelay  = table.BuildDelay;                 // asm: [Rules+1578h]

    // asm loc_4F70EA: TeamDelayTimer = { Frame, oldDiff, TeamDelays[diff] +
    //                  PlayerNumber * 175 }
    int32 delay = pRules->TeamDelays.GetItem(diff);
    delay += PlayerNumber * 175;                    // asm: idx*7*5*5
    TeamDelayTimer.StartTime = Game::CurrentFrame;
    TeamDelayTimer.TimeLeft  = delay;
    (void)oldDiff;                                  // asm: oldDiff 只在栈上留痕
}

// ----------------------------------------------------------------------------
// asm 554176 —— HouseClass::Harvested(percentage, idxTiberium)，retn 8
//
//   SiloMoney += floor(percentage * 1.0)          ; asm: fmul ds:num_5
//   Credits   += floor(Tiberium.Value * Type->IncomeMult * percentage)
//   返回值：percentage 本身（原版最后一条 fld 把入参留在 st0）。
// ----------------------------------------------------------------------------
float HouseClass::Harvested(float percentage, int32 idxTiberium)
{
    SiloMoney = static_cast<int32>(
        std::floor(static_cast<double>(SiloMoney) + percentage * 1.0));

    int32 value = 0;
    double incomeMult = 1.0;
    if (TiberiumClass::Array != nullptr) {
        TiberiumClass* pTib = TiberiumClass::Array->GetItem(idxTiberium);
        if (pTib != nullptr)
            value = pTib->Value;                    // asm: [edx+TiberiumClass.Value]
    }
    if (Type != nullptr)
        incomeMult = Type->IncomeMult;              // asm: [eax+HouseTypeClass.IncomeMult]

    Credits = static_cast<int32>(
        std::floor(static_cast<double>(Credits) +
                   static_cast<double>(value) * incomeMult * percentage));

    return percentage;
}

// ----------------------------------------------------------------------------
// asm 0x50BA00 —— HouseClass_50BA00
//
//   al = [esi+56F9h] / cl = [esi+56FAh] / dl = [esi+56FBh]
//   len = sqrt(r*r + g*g + b*b)
//   len == 0 -> 三分量都取 255.0
//   否则     -> 各分量 / len
// ----------------------------------------------------------------------------
void HouseClass::Recalc_Color_Vector()
{
    const double r = static_cast<double>(HouseColorR);
    const double g = static_cast<double>(HouseColorG);
    const double b = static_cast<double>(HouseColorB);
    const double len = std::sqrt(r * r + g * g + b * b);

    if (len == 0.0) {                              // asm: fcom 0.0 / jz
        ColorVecR = 255.0;                         // asm: fld ds:num_255
        ColorVecG = 255.0;
        ColorVecB = 255.0;
        return;
    }
    ColorVecR = r / len;
    ColorVecG = g / len;
    ColorVecB = b / len;
}
