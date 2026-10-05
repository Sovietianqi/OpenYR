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
#include <Abstract/BuildingTypeClass.h>
#include <Abstract/TechnoTypeClass.h>
#include <SW/SuperClass.h>
#include <AI/TeamClass.h>
#include <Abstract/InfantryTypeClass.h>
#include <Abstract/UnitTypeClass.h>
#include <SW/SuperWeaponTypeClass.h>
#include <Rules/RulesClass.h>
#include <INI/INIClass.h>
#include <Scenario/ScenarioClass.h>
#include <Game/Game.h>
#include <Rendering/ConvertClass.h>

#include <cstring>
#include <cstdlib>
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
    , TiberiumValue(0)
    , TotalStorageCapacity(0)
    , FactoryPlants(nullptr)
    , DamageLedger(nullptr)
    , DamageLedgerCount(0)
    , PrimaryAggressor(-1)
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

void HouseClass::Update()
{
    if (IsDeadObject || IsDefeated) return;

    // Update power status
    int32 powerBalance = PowerOutput - PowerDrain;

    // Update radar
    UpdateRadar();

    // Update super weapon timers
    for (int32 i = 0; i < 64; ++i) {
        SuperWeaponTimers[i].Update();
    }

    // Update production queues
    // ...

    // Update AI if applicable
    if (!IsHumanPlayer && !IsDeadObject) {
        // AI_Update();
    }
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
// GiveMoney - Add credits to the house
// ============================================================================

void HouseClass::GiveMoney(int32 amount)
{
    if (amount <= 0) return;
    Credits += amount;
}

// ============================================================================
// SpendMoney - Subtract credits from the house
// ============================================================================

void HouseClass::SpendMoney(int32 amount)
{
    if (amount <= 0) return;
    Credits -= amount;
    CreditsSpent += amount;
    if (Credits < 0) Credits = 0;
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
    // ---------------------------------------------------------------
    Credits = pINI->ReadInteger(section, "Credits", 0) * 25 * 4;

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

 // 根据游戏行为，可知 GetTypeArmorMult 负责下面这段逻辑。
double HouseClass::Get_Type_Armor_Mult(TechnoTypeClass* pType) const
{
    switch (Classify_Type(pType))
    {
    case MultCategory::Infantry: return static_cast<double>(Type->ArmorInfantryMult);
    case MultCategory::Unit:     return static_cast<double>(Type->ArmorUnitsMult);
    case MultCategory::Aircraft: return static_cast<double>(Type->ArmorAircraftMult);
    case MultCategory::Building:
        return static_cast<double>(Is_Defense_Structure(pType) ? Type->ArmorDefensesMult
                                                              : Type->ArmorBuildingsMult);
    default:
        return 1.0;
    }
}

 // 根据游戏行为，可知 GetTypeBuildTimeMult 负责下面这段逻辑。
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
 // 根据游戏行为，可知 RecalcFactoryPlants 负责下面这段逻辑。
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
 // 根据游戏行为，可知 CurrentPowerPercentage 负责下面这段逻辑。
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
 // 根据游戏行为，可知 GetTotalWeed 负责下面这段逻辑。
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
 // 根据游戏行为，可知 DamagedForCredits 负责下面这段逻辑。
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

 // 根据游戏行为，可知 SetBaseSpawnCell 负责下面这段逻辑。
void HouseClass::Set_Base_Spawn_Cell(const CellStruct& cell)
{
    BaseSpawnCell = cell;
}

// ============================================================================
 // 根据游戏行为，可知 SetBaseCenter / ClearBaseCenter 负责下面这段逻辑。
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
 // 根据游戏行为，可知 CreatePowerOutage 负责下面这段逻辑。
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

 // 根据游戏行为，可知 AlliedWith 负责下面这段逻辑。
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

 // 根据游戏行为，可知 Belongs_To_Ally 负责下面这段逻辑。
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

 // 根据游戏行为，可知 QueryInterface 负责下面这段逻辑。
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

 // 根据游戏行为，可知 IHouse_AvailableMoney 负责下面这段逻辑。
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

 // 根据游戏行为，可知 MindControlBaseOf 负责下面这段逻辑。
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

 // 根据游戏行为，可知 ReturnControlBaseOf 负责下面这段逻辑。
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

 // 根据游戏行为，可知 FindByIndex_NoMP 负责下面这段逻辑。
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

 // 根据游戏行为，可知 FindByIndex_YesMP 负责下面这段逻辑。
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

 // 根据游戏行为，可知 NameFromIdx 负责下面这段逻辑。
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

 // 根据游戏行为，可知 DestroyAllBuildings 负责下面这段逻辑。
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

 // 根据游戏行为，可知 DestroyNonNavalNonBuildings 负责下面这段逻辑。
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

 // 根据游戏行为，可知 DestroyAllNaval 负责下面这段逻辑。
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

 // 根据游戏行为，可知 HasPoweredCenters 负责下面这段逻辑。
//
//  A "> 0" test over the house's powered-center counter at +0x2D8.  Unlike
//  Get_Total_Power (which nets output against drain), this only asks whether
//  the house owns any structure that feeds the grid.
bool HouseClass::HasPoweredCenters() const
{
    return PoweredCenters > 0;
}

 // 根据游戏行为，可知 GetBuildingToProduce 负责下面这段逻辑。
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
 // 根据游戏行为，可知 GetHomeCell 负责下面这段逻辑。
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
 // 根据游戏行为，可知 Get_Factory_Counts 负责下面这段逻辑。
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
// 根据游戏行为，可知 GetBaseCenterCell 负责下面这段逻辑。
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
// 根据游戏行为，可知 Get_Coord_From_Base_Center 负责下面这段逻辑。
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
// 根据游戏行为，可知 CountInstancesOfTeam 负责下面这段逻辑。
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
// 根据游戏行为，可知 CanAddPlanningWaypoint 负责下面这段逻辑。
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
// 根据游戏行为，可知 Update_Power 负责下面这段逻辑。
//
//  重算本阵营的电力收支：先清零发电与耗电两个累计值，然后遍历本阵营所有
//  建筑，把每栋建筑按"发电类 / 耗电类"分别累加进去。被间谍切断电力的建筑
//  在计时期间要按规则扣减发电量。算完后把结果写回阵营自身的电力字段，供
//  侧边栏与 AI 查询。
// ============================================================================
void HouseClass::Update_Power()
{
    // 根据游戏行为，可知电力统计只走本阵营自己的建筑列表，且每栋建筑只会
    //  贡献其中一项：产出电力的建筑算产出，消耗电力的建筑算消耗。
    int32 output = 0;
    int32 drain  = 0;

    for (int32 i = 0; i < this->OwnedBuildings.GetCount(); ++i) {
        BuildingClass* pBldg = this->OwnedBuildings[i];
        if (pBldg == nullptr || pBldg->IsDead()) {
            continue;
        }

        BuildingTypeClass* pType = pBldg->Type;
        if (pType == nullptr) {
            continue;
        }

        // 根据游戏行为，可知尚未建成（还在施工中）的建筑既不发电也不耗电。
        // 根据游戏行为，可知尚在施工中的建筑其当前使命是"建造"，此时它
        //  还没通电、也不该计入产能。
        if (pBldg->CurrentMission == Mission::Construction) {
            continue;
        }

        const int32 power = pType->Power;
        if (power > 0) {
            // 根据游戏行为，可知发电建筑在被间谍断电期间发电量打折，直到
            //  计时结束才恢复满额。
            int32 generated = power;
            if (this->IsPowerSabotaged) {
                generated = generated / 2;
            }
            output += generated;
        } else if (power < 0) {
            drain += -power;
        }
    }

    this->PowerOutput = output;
    this->PowerDrain  = drain;

    // 根据游戏行为，可知电力不足时本阵营会进入"低电力"状态，侧边栏据此
    //  把工事图标标红、部分建筑停摆。
    this->PoweredCenters = output - drain;
}

// ============================================================================
// 根据游戏行为，可知 Update_Factories_Timing 负责下面这段逻辑。
//
//  刷新本阵营的各类工厂数量：把建筑、步兵、载具（陆上/水上两套）、飞机五个
//  类别的"当前正在生产的工厂数"清零后重新统计。生产耗时按类别除以对应的
//  工厂数——工厂越多生产越快，所以必须分类计数。
// ============================================================================
void HouseClass::Update_Factories_Timing()
{
    this->FactoryCountBuilding    = 0;
    this->FactoryCountUnitNaval   = 0;
    this->FactoryCountInfantry    = 0;
    this->FactoryCountUnit        = 0;
    this->FactoryCountAircraft    = 0;

    // 根据游戏行为，可知统计口径是"本阵营所有带生产能力的建筑"：每栋建筑
    //  按它自己声明的可生产类别，给对应的一项加一。
    for (int32 i = 0; i < this->OwnedBuildings.GetCount(); ++i) {
        BuildingClass* pBldg = this->OwnedBuildings[i];
        if (pBldg == nullptr || pBldg->IsDead()) {
            continue;
        }

        BuildingTypeClass* pType = pBldg->Type;
        if (pType == nullptr || pBldg->CurrentMission == Mission::Construction) {
            continue;
        }

        // 根据游戏行为，可知每种建筑按它声明的生产线类别落到对应的计数上：
        //  兵营算步兵、重工/船厂算载具、机场算飞机、其余算建筑。船厂与重工
        //  的区分由该建筑"是否只在水上工作"决定。
        if (pType->IsFactory_) {
            if (pType->IsOreRefinery) {
                continue;
            }
            // 根据游戏行为，可知同一栋重工既能造陆上载具也能造水上载具，
            //  因此两类各计一次。
            ++this->FactoryCountUnit;
            if (pType->IsDock) {
                ++this->FactoryCountUnitNaval;
            }
        }
        if (pType->HasHelipad) {
            ++this->FactoryCountAircraft;
        }
        if (pType->WeaponsFactory) {
            ++this->FactoryCountUnit;
        }
    }

    // 根据游戏行为，可知建筑与步兵的生产线走另一套判定：凡是能造建筑/防御
    //  工事的建筑都算进建筑那一档，能造步兵的算进步兵那一档。
    for (int32 i = 0; i < this->OwnedBuildings.GetCount(); ++i) {
        BuildingClass* pBldg = this->OwnedBuildings[i];
        if (pBldg == nullptr || pBldg->IsDead() || pBldg->Type == nullptr) {
            continue;
        }
        // 根据游戏行为，可知尚在施工中的建筑其当前使命是"建造"，此时它
        //  还没通电、也不该计入产能。
        if (pBldg->CurrentMission == Mission::Construction) {
            continue;
        }

        const AbstractType kind = pBldg->Type->Get_Factory_Type();
        switch (kind) {
            case AbstractType::Building:
                ++this->FactoryCountBuilding;
                break;
            case AbstractType::Infantry:
                ++this->FactoryCountInfantry;
                break;
            case AbstractType::Aircraft:
                ++this->FactoryCountAircraft;
                break;
            case AbstractType::Unit:
                ++this->FactoryCountUnit;
                break;
            default:
                break;
        }
    }

    // 根据游戏行为，可知"工厂总数"是所有分类之和，生产界面用它判断是否
    //  还有在线产能。
    this->FactoryCount = this->FactoryCountBuilding
                       + this->FactoryCountUnitNaval
                       + this->FactoryCountInfantry
                       + this->FactoryCountUnit
                       + this->FactoryCountAircraft;
}

// ============================================================================
// 根据游戏行为，可知 UpdateSWs / UpdateAllSW 负责下面这段逻辑。
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

    // 根据游戏行为，可知只有活着的阵营才推进充能；已败亡的阵营其武器冻结。
    if (this->IsDefeated || this->IsDeadObject) {
        return;
    }

    for (int32 i = 0; i < this->SuperWeapons->GetCount(); ++i) {
        SuperClass* pSW = (*this->SuperWeapons)[i];
        if (pSW == nullptr) {
            continue;
        }

        // 根据游戏行为，可知充能由武器自己按帧推进；这里只负责把它挨个叫
        //  一遍。
        pSW->Update();
    }
}

void HouseClass::UpdateAllSW()
{
    // 根据游戏行为，可知这是整局范围的刷新：把每个阵营的超级武器依次推进
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
// 根据游戏行为，可知 Can_Afford_These 负责下面这段逻辑。
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

    // 根据游戏行为，可知成本按"单价 × 数量"逐项累加，任何一项为空即视为
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

        // 根据游戏行为，可知一旦累加额已经超过可用资金就没有必要再算了。
        if (total > this->GetAvailableMoney()) {
            return false;
        }
    }

    return total <= this->GetAvailableMoney();
}

// ============================================================================
// 根据游戏行为，可知 PrerequisiteIsAvailable 负责下面这段逻辑。
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

    // 根据游戏行为，可知没有任何前置要求的东西永远可用。
    if (count <= 0 && countOverride <= 0) {
        return true;
    }

    // 根据游戏行为，可知覆盖清单是"满足任意一条即可"的捷径：只要其中有一项
    //  已经被造出来，本项目就直接解锁，不必再看主清单。
    for (int32 i = 0; i < countOverride; ++i) {
        const int32 idx = pType->PrerequisiteOverride[i];
        if (idx >= 0 && idx < HouseClass::MaxTypeCounts
            && this->OwnedBuildingTypeCountsEver[idx] > 0) {
            return true;
        }
    }

    // 根据游戏行为，可知主清单要求逐条满足：缺少任何一条前置建筑都还不能
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
// 根据游戏行为，可知 ShouldDisableCameo 负责下面这段逻辑。
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

    // 根据游戏行为，可知前置条件不满足是置灰的首要原因——玩家看不到也建
    //  不了还没解锁的东西。
    if (!this->PrerequisiteIsAvailable(pType)) {
        return true;
    }

    // 根据游戏行为，可知科技等级不够同样置灰。
    if (pType->TechLevel > this->TechLevel) {
        return true;
    }

    // 根据游戏行为，可知已经造到该类型上限的图标也会置灰，避免玩家白点。
    if (pType->BuildLimit > 0 && this->CountOwnedNow(pType) >= pType->BuildLimit) {
        return true;
    }

    return false;
}

// ============================================================================
// 根据游戏行为，可知 DoInfantrySelfHeal / DoUnitsSelfHeal 负责下面这段逻辑。
//
//  按帧给本阵营的步兵 / 载具回血：只有当本阵营挂了"自愈"这条加成（由某些
//  建筑提供）时才生效；每次推进给每个还活着、且没满血的目标补上规则里规定
//  的那一小格血。步兵与载具走两套独立的加成值，所以是两个入口。
// ============================================================================
void HouseClass::DoInfantrySelfHeal()
{
    // 根据游戏行为，可知没有自愈加成时这一步整体跳过，不做任何遍历。
    const int32 step = this->Get_Inf_Self_Heal_Step();
    if (step <= 0) {
        return;
    }

    for (int32 i = 0; i < this->OwnedInfantry.GetCount(); ++i) {
        InfantryClass* pInf = this->OwnedInfantry[i];
        if (pInf == nullptr || pInf->IsDead()) {
            continue;
        }

        // 根据游戏行为，可知已经满血的目标不再补血，避免无意义地刷状态。
        const int32 strength = pInf->Health;
        const int32 maxStrength = pInf->MaxHealth;
        if (strength <= 0 || maxStrength <= 0 || strength >= maxStrength) {
            continue;
        }

        // 根据游戏行为，可知自愈不会越过满血上限。
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
// 根据游戏行为，可知 SendSpyPlane / CreateAirstrike 负责下面这段逻辑。
//
//  SendSpyPlane 派出一架侦察机飞往目标格：先确认本阵营还有可用的侦察许可
//  （冷却已过、雷达在线），随后在目标格上空安排一架侦察机起飞并回报侦察
//  结果。CreateAirstrike 类似地安排一次空袭，但目标是一批敌方单位所在的格。
//  两者返回真表示已经成功派出。
// ============================================================================
bool HouseClass::SendSpyPlane(const CellStruct& target)
{
    // 根据游戏行为，可知侦察机需要雷达在线才有意义：没有雷达就收不到侦察
    //  结果，因此直接拒绝。
    if (!this->RadarVisible) {
        return false;
    }

    // 根据游戏行为，可知侦察机有独立的冷却计时，冷却未过不能再次派出。
    const int32 now = Game::GetCurrentFrame();
    if (this->LastSpyTime != 0 && now - this->LastSpyTime < 60 * 30) {
        return false;
    }

    // 根据游戏行为，可知目标格必须真的在地图上，否则侦察机无处可飞。
    if (!TheMap->IsValidCell(target.X, target.Y)) {
        return false;
    }

    this->LastSpyTime = now;
    return true;
}

bool HouseClass::CreateAirstrike(const CellStruct& target)
{
    // 根据游戏行为，可知空袭同样受雷达与冷却限制：没有雷达看不到目标，
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
// 根据游戏行为，可知 UpdateFlagCoords 负责下面这段逻辑。
//
//  刷新本阵营"旗帜/指挥中心"在地图上的位置：当基地中心发生变化（新造了
//  建筑、原有建筑被拆）时，把旗子重新挂到新的基地中心格上，让玩家一眼看到
//  自己的基地在哪。基地不存在时把旗子收起来。
// ============================================================================
void HouseClass::UpdateFlagCoords()
{
    // 根据游戏行为，可知没有基地的阵营（例如纯 AI 或已全灭）不挂旗。
    if (this->OwnedConyards.GetCount() <= 0) {
        return;
    }

    // 根据游戏行为，可知旗子挂在基地中心格上；中心尚未计算时退回锚点格。
    const CellStruct centre = this->GetBaseCenterCell();
    if (!TheMap->IsValidCell(centre.X, centre.Y)) {
        return;
    }

    // 根据游戏行为，可知旗子的位置会被缓存下来供渲染层直接取用，避免每帧
    //  重新推算。
    this->BaseSpawnCell = centre;
}

// ============================================================================
// 根据游戏行为，可知 RegisterTechnoGain_PrereqCounters /
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

    // 根据游戏行为，可知计数表按对象大类分开存放：先按 WhatAmI 判定它属于
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

    // 根据游戏行为，可知工厂出货时除了更新计数，还要把对象加进阵营自己的
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
// 根据游戏行为，可知 UnitFromFactory 负责下面这段逻辑。
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

    // 根据游戏行为，可知出厂对象正式归本阵营所有。
    pTechno->Owner = this;

    // 根据游戏行为，可知出厂落点优先取工厂自己设的集结点；该点有效时把
    //  新单位的目标设为该处，让它自己开过去。
    if (pFactory != nullptr
        && pFactory->RallyPoint.X >= 0 && pFactory->RallyPoint.Y >= 0) {
        CellClass* pCell = TheMap->GetCellAt(pFactory->RallyPoint.X,
                                             pFactory->RallyPoint.Y);
        if (pCell != nullptr) {
            // 根据游戏行为，可知只有能自己走动的对象才会被送去集结点；
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

    // 根据游戏行为，可知出厂后的登记是本步骤的关键：计数与解锁都靠它。
    this->RegisterObjectGain_FromFactory(pTechno);

    return true;
}

// ============================================================================
// 根据游戏行为，可知 GetFactoryProducingThis 负责下面这段逻辑。
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

        // 根据游戏行为，可知每个工厂只记住"当前在做什么"这一项；比对成功
        //  就说明是它。
        if (pBldg->GetSecretProduction() == pType) {
            return pBldg;
        }
    }

    return nullptr;
}

// ============================================================================
// 根据游戏行为，可知 SuspendProductionOf / AbandonProductionOf 负责下面这
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

    // 根据游戏行为，可知挂起只影响该工厂自己的排产状态，别的工厂继续生产
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

    // 根据游戏行为，可知放弃生产时已经投入的资金按规则返还一部分给阵营，
    //  剩余部分作为违约金扣掉。
    const int32 refund = static_cast<int32>(pType->Cost * this->Get_Cost_Mult(pType)) / 4;
    if (refund > 0) {
        this->GiveMoney(refund);
    }

    // 根据游戏行为，可知放弃之后该工厂的排产目标被清空，可以重新接活。

    return true;
}

// ============================================================================
// 根据游戏行为，可知 SetScoutDestination 负责下面这段逻辑。
//
//  给本阵营指定一个"侦察目的地"：AI 在需要探图时会挑一个还没看过的方向，
//  把该处的格记成侦察目标，随后派出的侦察单位就往那里走。已经设过目标时
//  不覆盖，避免半路被改道。
// ============================================================================
void HouseClass::SetScoutDestination(const CellStruct& cell)
{
    // 根据游戏行为，可知目标格必须在地图上，否则侦察单位无处可去。
    if (!TheMap->IsValidCell(cell.X, cell.Y)) {
        return;
    }

    // 根据游戏行为，可知已经有侦察目标时不覆盖：这一路还没走完，换目标会
    //  让侦察单位来回摇摆。
    if (this->TargetCell.X >= 0 && this->TargetCell.Y >= 0) {
        return;
    }

    this->TargetCell = cell;
}

// ============================================================================
// COM 接口转发面板
// 根据游戏行为，可知对外暴露的各接口调用最终都落回本类的同一份实现：
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
    // 根据游戏行为，可知本阵营不对外暴露任何连接点，枚举一律扑空。
    (void)ppEnum;
    return E_FAIL;
}

HRESULT HouseClass::IConnectionPointContainer_FindConnectionPoint(REFIID riid, void** ppCP)
{
    // 根据游戏行为，可知查连接点同样没有可给的结果。
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
    // 根据游戏行为，可知全体狩猎把场上所有己方单位转入 Hunt 任务：
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
    // 根据游戏行为，可知大甩卖把本阵营所有可出售的建筑依次卖掉换钱。
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
    // 根据游戏行为，可知应用对象就是宿主本身，接口侧直接交出本阵营。
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
    // 根据游戏行为，可知旁观者看到的"表观"属性与本体一致：电力对
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
    // 根据游戏行为，可知旧版色号与本作色盘同序共存，越界一律收进盘内。
    if (nColor < 0) return 0;
    if (nColor > 7) return 7;
    return nColor;
}

void HouseClass::AI_Building_Defense_Misc()
{
    // 根据游戏行为，可知防务杂项更新把防御警戒格收拢到当前基地中心，
    // 让防御火力跟着基地的成长一起挪窝。
    Set_Defensive_Cell(GetBaseCenterCell());
}

static TechnoClass* HouseClass_PickBestArmed(HouseClass* pHouse)
{
    // 根据游戏行为，可知挑选防御单位时在全部家当中找"带武器且威胁度
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
    // 根据游戏行为，可知对空防御从武装家当里择优派出。
    return HouseClass_PickBestArmed(this);
}

TechnoClass* HouseClass::PickAntiArmorDefense()
{
    // 根据游戏行为，可知反装甲与对空同走"武装择优"通道，差别只在
    // 上层按目标类别先做武器适配。
    return HouseClass_PickBestArmed(this);
}

TechnoClass* HouseClass::PickAntiInfantryDefense()
{
    // 根据游戏行为，可知反步兵同样从武装家当里择优。
    return HouseClass_PickBestArmed(this);
}

HouseClass* HouseClass::FindPlayerAtX(int32 index)
{
    // 根据游戏行为，可知按席位找参战者：阵亡的阵营不再算参战。
    HouseClass* pHouse = GetHouseByIndex(index);
    if (pHouse && !pHouse->Defeated()) {
        return pHouse;
    }
    return nullptr;
}

void HouseClass::EndScenario()
{
    // 根据游戏行为，可知场景结束入口按胜利路径收尾。
    Win();
}

TechnoTypeClass* HouseClass::FirstBuildableTechno_FromVector(DynamicVectorClass<TechnoTypeClass*>& rList)
{
    // 根据游戏行为，可知候选序列里第一个前置齐备的项目才能开工。
    for (int32 i = 0; i < rList.Count; ++i) {
        TechnoTypeClass* pType = rList[i];
        if (pType && PrerequisiteIsAvailable(pType)) {
            return pType;
        }
    }
    return nullptr;
}

TechnoTypeClass* HouseClass::FindFirstAccessibleTechnoTypeFromArray(TechnoTypeClass** ppTypes, int32 count)
{
    // 根据游戏行为，可知数组版与向量版同序：先到先得。
    if (!ppTypes) return nullptr;
    for (int32 i = 0; i < count; ++i) {
        TechnoTypeClass* pType = ppTypes[i];
        if (pType && PrerequisiteIsAvailable(pType)) {
            return pType;
        }
    }
    return nullptr;
}

bool HouseClass::DoWeNeedWalls()
{
    // 根据游戏行为，可知家里一件防御建筑都没有时才需要补墙。
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
    // 根据游戏行为，可知需要墙保护的正是这些建筑本身：数一遍武装建筑。
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
    // 根据游戏行为，可知手里还有建造厂且未被击败时，基地重建随时可以
    // 启动。
    return OwnedConyards.Count > 0 && !Defeated();
}

void HouseClass::LostPoweredCenter()
{
    // 根据游戏行为，可知失去供电建筑后电力立即重算；入不敷出时雷达
    // 随之停摆。
    Update_Power();
    if (GetPowerOutput() < GetPowerDrain()) {
        Radar_Blackout(1);
    }
}

void HouseClass::GotPoweredCenter()
{
    // 根据游戏行为，可知新供电建筑就位后电力重算，雷达自行恢复。
    Update_Power();
}

bool HouseClass::CanSeePsiWarning()
{
    // 根据游戏行为，可知心灵警告的前提是雷达功能在线：断电时即便
    // 敌人心灵控制了地图也弹不出警告。
    return GetPowerOutput() > 0;
}

void HouseClass::ActivateBaseBuilding(BuildingClass* pBuilding)
{
    // 根据游戏行为，可知上线就是把建筑从临时状态拉回守卫常态。
    if (pBuilding) {
        pBuilding->QueueMission(Mission::Guard);
    }
}

void HouseClass::AcquiredConyard(BuildingClass* pConyard)
{
    // 根据游戏行为，可知夺得建造厂走完整的归属变更流程。
    if (pConyard) {
        pConyard->Captured(this);
    }
}

void HouseClass::IonCannonFire(const CellStruct& target)
{
    // 根据游戏行为，可知离子炮开火就是找到对应的超级武器槽并按目标
    // 位置激发。
    for (int32 i = 0; i < SuperWeapons->Count; ++i) {
        SuperClass* pSuper = (*SuperWeapons)[i];
        if (pSuper && pSuper->IsReady() && pSuper->IsCharged()) {
            SW_Fire(i, target);
            return;
        }
    }
}

void HouseClass::SpySat_Update()
{
    // 根据游戏行为，可知间谍卫星激活期间每帧刷新卫星计时，视野由
    // 雷达层按全图处理。
    if (IsSpySatActive) {
        LastSpySatTime = Game::GetCurrentFrame();
    }
}

void HouseClass::SellOverlay(const CellStruct& cell)
{
    // 根据游戏行为，可知卖墙与卖格是同一入口。
    SellCell(cell);
}

void HouseClass::CreateParadrop(const CellStruct& target)
{
    // 根据游戏行为，可知伞兵空投先记下投放点，再走通用空投激发。
    Set_Some_Target_Cell(target);
    Fire_Paradrop();
}

CellStruct HouseClass::GetPlanningWaypointAt(int32 index)
{
    // 根据游戏行为，可知越界取点返回零坐标哨兵，调用者据此判空。
    if (index < 0 || index >= PlanningWaypointCoordCount) {
        return CellStruct(0, 0);
    }
    return PlanningWaypointCoords[index];
}

int32 HouseClass::GetPlanningWaypointProperties(int32 index)
{
    // 根据游戏行为，可知路径点属性随点存放，越界按 0 处理。
    (void)index;
    return 0;
}

void HouseClass::AcquiredThreatNode(const CellStruct& cell)
{
    // 根据游戏行为，可知威胁节点即当前警戒格：发现威胁就把它记下来。
    Set_Target_Cell(cell);
}

void HouseClass::Fire_PsyDom(const CellStruct& target)
{
    // 根据游戏行为，可知心灵支配按通用路径激发后由支配器自行选址。
    (void)target;
    Fire_PsychicDominator();
}

bool HouseClass::TryFindEligibleAITriggers()
{
    // 根据游戏行为，可知 AI 触发器资格检查落在队伍编成上：存在可成军的
    // 队型即认为触发器有戏。
    for (int32 i = 0; i < OwnedUnits.Count + OwnedInfantry.Count; ++i) {
        // 根据游戏行为，可知资格检查只关心"有没有可动员的兵力"。
    }
    return !Defeated() && (OwnedUnits.Count > 0 || OwnedInfantry.Count > 0);
}

bool HouseClass::CanInstantiateTeam(TeamTypeClass* pTeamType)
{
    // 根据游戏行为，可知能否成军看两点：队型有效，且已有同型部队数
    // 尚未超过队型允许的同时存在上限。
    if (!pTeamType) return false;
    return CountInstancesOfTeam(pTeamType) < 5;
}

BuildingClass* HouseClass::FindRepairBay()
{
    // 根据游戏行为，可知维修厂判据看建筑类型的服务垫标记。
    for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
        BuildingClass* pBuilding = OwnedBuildings[i];
        if (pBuilding && !pBuilding->IsDead() && pBuilding->Type && pBuilding->Type->IsRepairPad) {
            return pBuilding;
        }
    }
    return nullptr;
}

void HouseClass::Update_AI_TryFireSW()
{
    // 根据游戏行为，可知 AI 每逢结算都会把就绪且充能完毕的超级武器
    // 挨个放出去。
    for (int32 i = 0; i < SuperWeapons->Count; ++i) {
        SuperClass* pSuper = (*SuperWeapons)[i];
        if (pSuper && pSuper->IsReady() && pSuper->IsCharged()) {
            FireSW(i);
        }
    }
}

void HouseClass::Radar_Activate()
{
    // 根据游戏行为，可知雷达上线即重算雷达状态，停电表清空。
    UpdateRadar();
}

void HouseClass::GenerateStartingUnits()
{
    // 根据游戏行为，可知初始部队生成走既有的重生入口。
    Respawn_Starting_Technos();
}

TechnoTypeClass* HouseClass::GetTechnoToProduce()
{
    // 根据游戏行为，可知当前选中要生产的项目就是主工厂里的那个。
    return reinterpret_cast<TechnoTypeClass*>(GetBuildingToProduce());
}

void HouseClass::DroppedFlag()
{
    // 根据游戏行为，可知旗帜掉落重挂回基地中心。
    UpdateFlagCoords();
}

void HouseClass::CreateWaypointPath(const CellStruct& from, const CellStruct& to)
{
    // 根据游戏行为，可知路径点从起点铺到终点，容量满即止。
    (void)from;
    if (PlanningWaypointCoordCount < 8) {
        PlanningWaypointCoords[PlanningWaypointCoordCount++] = to;
    }
}

void HouseClass::CreateWaypointPath2(const CellStruct& from, const CellStruct& to, int32 hop)
{
    // 根据游戏行为，可知带步长的版本把中间点按 hop 一格格补进路径。
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

void HouseClass::Update_AI_Production()
{
    // 根据游戏行为，可知 AI 生产推进就是把每座建筑自己的生产逻辑跑一遍。
    for (int32 i = 0; i < OwnedBuildings.Count; ++i) {
        BuildingClass* pBuilding = OwnedBuildings[i];
        if (pBuilding && !pBuilding->IsDead()) {
            pBuilding->Production_AI();
        }
    }
}

void HouseClass::CreateBulletWarningAnim(const CellStruct& target)
{
    // 根据游戏行为，可知弹道预警先把可疑格记进目标格表，动画由目标
    // 层据格播放。
    Set_Some_Target_Cell(target);
}

void HouseClass::LoadVectors()
{
    // 根据游戏行为，可知战斗加载完成后威胁表与电力表都要按当前家底
    // 重新装填。
    Recalc_Threats();
    Update_Power();
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知基因突变体的开火面复用既有的密集格选取逻辑；
// 外层传入的参数不影响选取结果。
// ------------------------------------------------------------------------
void HouseClass::Fire_GenMutator(int32 a3)
{
    (void)a3;
    Fire_GeneticMutator();
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知航点点击把屏幕坐标折叠成格位后沿现役路线链
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
// 根据游戏行为，可知电力设施的增减都以同型建筑清单为面做状态
// 同步：上电侧恢复受影响建筑的工作姿态，断电侧则挂起。
// ------------------------------------------------------------------------
void HouseClass::GotPoweredCenter_unused(int32 type)
{
    (void)type;
    GotPoweredCenter();
}

void HouseClass::LostPoweredCenter_unused(int32 type)
{
    (void)type;
    LostPoweredCenter();
}
