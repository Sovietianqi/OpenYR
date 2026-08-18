#include <Houses/HouseClass.h>
#include <Houses/HouseTypeClass.h>
#include <Core/Definitions.h>
#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Abstract/UnitClass.h>
#include <Abstract/InfantryClass.h>
#include <Abstract/BuildingClass.h>
#include <Rules/RulesClass.h>
#include <INI/INIClass.h>

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
HouseClass* HouseClass::Observer = nullptr;

// ============================================================================
// Constructor
// ============================================================================

HouseClass::HouseClass(HouseTypeClass* pType)
    : Type(pType)
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
    , AllyBitfield(0)
    , EnemyBitfield(0)
    , ActiveSuperWeapons(0)
    , AvailableSuperWeapons(0)
    , UsedSuperWeapons(0)
    , TechLevel(0)
    , IQLevel(0)
    , IQLevel2(0)
    , Edge(0)
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
    , BaseNodesCount(0)
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
