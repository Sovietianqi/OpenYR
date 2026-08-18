#pragma once

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <Math/CoordStruct.h>
#include <Math/Timer.h>
#include <COM/IUnknown.h>
#include <Houses/HouseTypeClass.h>
#include <Abstract/SuperWeaponTypeClass.h>
#include <SW/SuperWeaponTypeClass.h>
#include <Abstract/TechnoTypeClass.h>
#include <Audio/VocClass.h>

// Forward declarations
class CCINIClass;
class RulesClass;
class AbstractClass;
class BuildingClass;
class BuildingTypeClass;
class InfantryClass;
class InfantryTypeClass;
class UnitClass;
class UnitTypeClass;
class AircraftClass;
class AircraftTypeClass;
class TechnoClass;
class TechnoTypeClass;
class SuperWeaponTypeClass;
class BaseClass;
class BaseNodeClass;
class FactoryClass;
class CampaignClass;
class BeaconClass;
class TriggerClass;
class TeamClass;
class TagClass;
class AITriggerClass;
class AITriggerTypeClass;
class SideClass;
class ScriptClass;
class TaskForceClass;
class TeamTypeClass;
class WeaponTypeClass;
class WarheadTypeClass;

// COM interfaces
struct IHouse;
struct IPublicHouse;
struct IConnectionPointContainer;

struct IHouse : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Get_CurrentPlayer(bool* pVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Set_CurrentPlayer(bool Val) = 0;
    virtual HRESULT STDMETHODCALLTYPE Get_PlayerColor(COLORREF* pVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Set_PlayerColor(COLORREF Val) = 0;
    virtual HRESULT STDMETHODCALLTYPE Get_LoadPlayer(bool* pVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Set_LoadPlayer(bool Val) = 0;
    virtual HRESULT STDMETHODCALLTYPE Get_PlayerName(wchar_t** pVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Set_PlayerName(wchar_t* Val) = 0;
    virtual HRESULT STDMETHODCALLTYPE Get_ActLike(int32* pVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Set_ActLike(int32 Val) = 0;
    virtual HRESULT STDMETHODCALLTYPE Is_Ally(int32 DwHouseIndex, bool* pVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Is_Player(bool* pVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Get_IsObserver(bool* pVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Set_IsObserver(bool Val) = 0;
    virtual HRESULT STDMETHODCALLTYPE Get_IsMultiplayPassive(bool* pVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Set_IsMultiplayPassive(bool Val) = 0;
    virtual HRESULT STDMETHODCALLTYPE Make_Ally(int32 DwHouseIndex) = 0;
    virtual HRESULT STDMETHODCALLTYPE Make_Enemy(int32 DwHouseIndex) = 0;
};

struct IPublicHouse : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Get_PlayerColor(COLORREF* pVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Set_PlayerColor(COLORREF Val) = 0;
    virtual HRESULT STDMETHODCALLTYPE Get_PlayerName(wchar_t** pVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Set_PlayerName(wchar_t* Val) = 0;
    virtual HRESULT STDMETHODCALLTYPE Get_ActLike(int32* pVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Set_ActLike(int32 Val) = 0;
    virtual HRESULT STDMETHODCALLTYPE Is_Ally(int32 DwHouseIndex, bool* pVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE Is_Player(bool* pVal) = 0;
};

struct IConnectionPointContainer : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE EnumConnectionPoints(void** ppEnum) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindConnectionPoint(REFIID riid, void** ppCP) = 0;
};

// ============================================================================
// HouseClass - The main house/player class
// ============================================================================
class HouseClass : public AbstractClass, public IHouse
{
public:
    static constexpr int32 MaxHouses = 32;
    static constexpr int32 MaxSuperWeapons = 64;
    static constexpr int32 MaxTypeCounts = 512;

    // Static members
    static HouseClass* Array[MaxHouses];
    static int32 ArrayCount;
    static HouseClass* GetHouseByIndex(int32 index) { return index >= 0 && index < ArrayCount ? Array[index] : nullptr; }
    DynamicVectorClass<TechnoClass*>* GetTechnos() { return nullptr; }
    static HouseClass* pCurrentPlayer;
    static HouseClass* Player;
    static HouseClass* Observer;

    // Constructor / Destructor
    HouseClass(HouseTypeClass* pType);
    virtual ~HouseClass() noexcept override;

    // AbstractClass overrides
    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual int32 GetArrayIndex() const override;
    virtual bool IsDead() const override;
    virtual HRESULT GetClassID(CLSID* pClassID) override;

    // IHouse interface implementation
    virtual HRESULT STDMETHODCALLTYPE Get_CurrentPlayer(bool* pVal) override;
    virtual HRESULT STDMETHODCALLTYPE Set_CurrentPlayer(bool Val) override;
    virtual HRESULT STDMETHODCALLTYPE Get_PlayerColor(COLORREF* pVal) override;
    virtual HRESULT STDMETHODCALLTYPE Set_PlayerColor(COLORREF Val) override;
    virtual HRESULT STDMETHODCALLTYPE Get_LoadPlayer(bool* pVal) override;
    virtual HRESULT STDMETHODCALLTYPE Set_LoadPlayer(bool Val) override;
    virtual HRESULT STDMETHODCALLTYPE Get_PlayerName(wchar_t** pVal) override;
    virtual HRESULT STDMETHODCALLTYPE Set_PlayerName(wchar_t* Val) override;
    virtual HRESULT STDMETHODCALLTYPE Get_ActLike(int32* pVal) override;
    virtual HRESULT STDMETHODCALLTYPE Set_ActLike(int32 Val) override;
    virtual HRESULT STDMETHODCALLTYPE Is_Ally(int32 DwHouseIndex, bool* pVal) override;
    virtual HRESULT STDMETHODCALLTYPE Is_Player(bool* pVal) override;
    virtual HRESULT STDMETHODCALLTYPE Get_IsObserver(bool* pVal) override;
    virtual HRESULT STDMETHODCALLTYPE Set_IsObserver(bool Val) override;
    virtual HRESULT STDMETHODCALLTYPE Get_IsMultiplayPassive(bool* pVal) override;
    virtual HRESULT STDMETHODCALLTYPE Set_IsMultiplayPassive(bool Val) override;
    virtual HRESULT STDMETHODCALLTYPE Make_Ally(int32 DwHouseIndex) override;
    virtual HRESULT STDMETHODCALLTYPE Make_Enemy(int32 DwHouseIndex) override;

    // Serialization
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL bSave) override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // Initialization
    void Init();
    void Update();

    // Alliance
    void MakeAlly(HouseClass* pHouse);
    void MakeEnemy(HouseClass* pHouse);
    bool IsAlliedWith(HouseClass* pHouse) const;
    bool IsHostileTo(HouseClass* pHouse) const;
    static bool IsAllied(int32 house1, int32 house2);

    // Game state
    void Win();
    void Lose();
    void DestroyAll();
    bool Defeated() const;
    void ScatterAllUnits();
    void UpdateSightAroundUnit(class TechnoClass* pUnit);
    void CheerAllUnits();
    void SetPrimaryFactory(int32 factoryID);
    void SellCell(const CellStruct& cell);

    // Building
    bool CanBuild(TechnoTypeClass* pType) const;
    bool CanBuildNow(TechnoTypeClass* pType) const;
    bool CanExpectToBuild(TechnoTypeClass* pType) const;
    int32 CountOwnedNow(TechnoTypeClass* pType) const;
    int32 CountOwnedEver(TechnoTypeClass* pType) const;

    // Economy
    int32 GetAvailableMoney() const;
    void GiveMoney(int32 amount);
    void SpendMoney(int32 amount);

    // Super Weapons
    void CheckSWs();
    void FireSW(int32 swIndex);
    int32 FindSuperWeapon(SuperWeaponType type) const;

    // Radar
    void UpdateRadar();

    // Tracking
    void Tracking_Add(TechnoClass* pTechno);
    void Tracking_Remove(TechnoClass* pTechno);

    // Production / loss bookkeeping (mirror HouseClass_* in the original)
    bool BeginProductionOf(TechnoTypeClass* pType, int32 quantity = 1);
    void RegisterTechnoLoss(TechnoClass* pTechno);
    void AITakeover(TechnoClass* pTechno);
    bool Can_Afford(int32 cost) const;
    void GenerateAIBuildList();
    int32 Get_Total_Value() const;

    // Registration
    void RegisterJustBuilt(TechnoTypeClass* pType);
    void RegisterLoss(TechnoTypeClass* pType);

    // Voice
    void QueueVoice(VocType voice);
    void Speak(VocType voice);

    // Pointer invalidation
    void PointerGotInvalid(AbstractClass* pInvalid, bool removed);

    // ========================================================================
    // Members
    // ========================================================================
    HouseTypeClass*     Type;
    int32               TimesDefeated;
    int32               TimesWon;
    int32               Credits;
    int32               CreditsSpent;
    bool                MapIsClear;
    int32               AirUnits;
    int32               InfantryUnits;
    int32               Buildings;
    int32               Ships;
    int32               Vehicles;
    int32               AllTechnos;
    int32               PowerOutput;
    int32               PowerDrain;
    bool                CurrentPlayer;
    bool                PlayerControl;
    bool                IsDeadObject;
    bool                IsDefeated;
    bool                IsWinner;
    bool                IsObserver;
    bool                IsDiscovered;
    bool                IsControlStatus;
    bool                IsHumanPlayer;
    bool                IsBaseZone;
    bool                IsRebuilding;
    bool                IsCivilians;
    bool                IsVisionary;
    bool                IsMultiplayerPassive;
    bool                IsMPGameOver;
    bool                IsGPSActive;
    bool                IsGPSActiveVisible;
    bool                IsGPSActiveInRadar;
    bool                IsSpySatActive;
    bool                IsSpySatActiveVisible;
    bool                IsSpySatActiveInRadar;
    AbstractClass*      SpiedBy;
    AbstractClass*      SpiedBy_SpySat;
    uint32              AllyBitfield;
    uint32              EnemyBitfield;
    uint32              ActiveSuperWeapons;
    uint32              AvailableSuperWeapons;
    uint32              UsedSuperWeapons;
    int32               TechLevel;
    int32               IQLevel;
    int32               IQLevel2;
    int32               Edge;
    int32               ColorSchemeIndex;
    int32               UnitCount;
    int32               InfantryCount;
    int32               AircraftCount;
    int32               BuildingCount;
    int32               OwnedUnitCount;
    int32               OwnedInfantryCount;
    int32               OwnedAircraftCount;
    int32               OwnedBuildingCount;
    int32               DestroyedUnitCount;
    int32               DestroyedInfantryCount;
    int32               DestroyedAircraftCount;
    int32               DestroyedBuildingCount;
    int32               TotalUnitCount;
    int32               TotalInfantryCount;
    int32               TotalAircraftCount;
    int32               TotalBuildingCount;
    int32               DestroyedUnitValue;
    int32               DestroyedInfantryValue;
    int32               DestroyedAircraftValue;
    int32               DestroyedBuildingValue;
    int32               TotalUnitValue;
    int32               TotalInfantryValue;
    int32               TotalAircraftValue;
    int32               TotalBuildingValue;
    int32               AllHousesIndex;
    int32               ArrayIndex;
    int32               ActLikeIndex;
    wchar_t             PlayerName[32];
    int32               FactoryCount;
    int32               AlliesCounter;
    int32               EnemiesCounter;
    bool                RadarVisible;
    bool                RadarVisibleToPlayer;
    bool                RadarDisabled;
    bool                RadarJammed;
    AbstractClass*      RadarJammedBy;
    bool                RadarSpied;
    AbstractClass*      RadarSpiedBy;
    bool                RevealedByHeight;
    CellStruct          BaseCenter;
    int32               BaseNodesCount;

    // Owned type counts
    int32               OwnedUnitTypeCounts[MaxTypeCounts];
    int32               OwnedInfantryTypeCounts[MaxTypeCounts];
    int32               OwnedAircraftTypeCounts[MaxTypeCounts];
    int32               OwnedBuildingTypeCounts[MaxTypeCounts];
    int32               OwnedUnitTypeCountsEver[MaxTypeCounts];
    int32               OwnedInfantryTypeCountsEver[MaxTypeCounts];
    int32               OwnedAircraftTypeCountsEver[MaxTypeCounts];
    int32               OwnedBuildingTypeCountsEver[MaxTypeCounts];

    // Tracking lists
    DynamicVectorClass<UnitClass*>       OwnedUnits;
    DynamicVectorClass<InfantryClass*>   OwnedInfantry;
    DynamicVectorClass<AircraftClass*>   OwnedAircraft;
    DynamicVectorClass<BuildingClass*>   OwnedBuildings;
    DynamicVectorClass<TechnoClass*>     AllOwnedObjects;
    DynamicVectorClass<TechnoClass*>     TrackingList;

    // Super weapon timers
    CDTimerClass        SuperWeaponTimers[MaxSuperWeapons];

    // Timestamps
    int32 LastBuildTime, LastProductionTime, LastAttackTime, LastEnemySightingTime;
    int32 LastTeamCreationTime, LastBaseScanTime, LastCombatTime, LastNavalCombatTime;
    int32 LastAirCombatTime, LastSpySatTime, LastIronCurtainTime, LastForceShieldTime;
    int32 LastPsychicRevealTime, LastSonarTime, LastRadarTime, LastBuildingTime;
    int32 LastInfantryTime, LastVehicleTime, LastAircraftTime, LastSuperWeaponTime;
    int32 LastAirstrikeTime, LastParadropTime, LastSpyTime, LastEngineerTime;
    int32 LastChronoTime, LastChronoWarpTime, LastSabotageTime, LastDisguiseTime;
    int32 LastFlashTime, LastMoneyDrainTime, LastBackgroundMusicTime, LastSpeechTime;
    int32 LastEVAEventTime, LastTargetTime, LastBaseDefenseTime, LastRepairTime;
    int32 LastSellTime, LastPowerTime, LastUpgradeTime, LastConstructionTime;
    int32 LastTiberiumCollectionTime, LastHarvesterDumpTime, LastSlaveMinerTime;
    int32 LastResourceScanTime, LastAutoSaveTime, LastAutoSaveGameTime, LastCursorTime;
    int32 LastMessageTime, LastTriggerTime, LastTeamTime, LastScriptTime;
    int32 LastGlobalTime, LastLocalTime, LastEVAEventTime2, LastVoiceTime;
    int32 LastSoundTime, LastCheerTime, LastClockTime, LastMapTime;
    int32 LastRadarFlashTime, LastRadarEventTime, LastBeaconTime, LastBuildTime2;
    int32 LastAnimTime, LastMusicTime, LastMovieTime, LastBriefingTime;
    int32 LastScoreTime, LastOverlayTime, LastTiberiumTime, LastVeinTime;
    int32 LastIceTime, LastExplosionTime, LastFireTime, LastSparkTime;
    int32 LastSmokeTime, LastDustTime, LastDebrisTime, LastParticleTime;
    int32 LastWeatherTime, LastIonStormTime, LastLightningTime, LastMeteoriteTime;
    int32 LastEarthquakeTime, LastVolcanoTime, LastTornadoTime, LastFloodTime;
    int32 LastDroughtTime, LastFamineTime, LastPestilenceTime, LastWarTime;
    int32 LastPeaceTime, LastAllianceTime, LastWarDeclarationTime, LastDiplomacyTime;
    int32 LastTradeTime, LastGiftTime, LastTributeTime, LastBribeTime;
    int32 LastBlackmailTime, LastEspionageTime, LastCounterintelligenceTime, LastPropagandaTime;
    int32 LastInsurgencyTime, LastRevolutionTime, LastCoupTime, LastAssassinationTime;
    int32 LastSabotageTime2, LastTerrorismTime, LastGuerrillaTime, LastResistanceTime;
    int32 LastLiberationTime, LastOccupationTime, LastAnnexationTime, LastColonizationTime;
    int32 LastDecolonizationTime, LastIndependenceTime, LastSuccessionTime, LastSecessionTime;
    int32 LastUnificationTime, LastDivisionTime, LastPartitionTime, LastFederationTime;
    int32 LastConfederationTime, LastIntegrationTime, LastDisintegrationTime, LastReformationTime;
    int32 LastTransformationTime, LastRestorationTime, LastRenovationTime, LastReconstructionTime;
    int32 LastRehabilitationTime, LastRegenerationTime, LastResurrectionTime, LastRevivalTime;
    int32 LastRenaissanceTime, LastEnlightenmentTime, LastAwakeningTime, LastRebirthTime;
    int32 LastGenesisTime, LastApocalypseTime, LastArmageddonTime, LastCataclysmTime;
    int32 LastCatastropheTime, LastCalamityTime, LastDisasterTime, LastCataclysmTime2;
    int32 LastDoomsdayTime, LastJudgmentTime, LastOmegaTime, LastAlphaTime;
    int32 LastBetaTime, LastGammaTime, LastDeltaTime, LastEpsilonTime;
    int32 LastZetaTime, LastEtaTime, LastThetaTime, LastIotaTime;
    int32 LastKappaTime, LastLambdaTime, LastMuTime, LastNuTime;
    int32 LastXiTime, LastOmicronTime, LastPiTime, LastRhoTime;
    int32 LastSigmaTime, LastTauTime, LastUpsilonTime, LastPhiTime;
    int32 LastChiTime, LastPsiTime, LastOmegaTime2;
};