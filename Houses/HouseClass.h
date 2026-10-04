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

    // HouseClass_DefaultIonCannon_Coords (asm 0x8872E8).  The module sentinel
    // stamped into every "cleared" cell slot; a house cell holding this value
    // is unset.  The original stores it as a (0xFFFF, 0xFFFF) wXY.
    static const CellStruct DefaultIonCannon_Coords;

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

    // ── COM identity (asm HouseClass_QueryInterface 0x4F67F8 / the
    //    HouseClass_AddRef / _Release thunks) ──────────────────────────────
    // The object exposes five interfaces; every interface's own AddRef /
    // Release / QueryInterface is a thunk that subtracts the interface's byte
    // offset within the object and forwards here.  Therefore AddRef and
    // Release always succeed (the original returns 1 unconditionally), and
    // QueryInterface only needs to test the requested IID against the five
    // supported ones.
    HRESULT Query_Interface(const GUID& riid, void** ppvObject);
    uint32  Add_Ref();
    uint32  Release_Ref2();

    // IHouse_* accessors backed directly by house fields.
    int32   IHouse_Available_Money() const;
    int32   IHouse_Available_Storage() const;

    // Initialization
    void Init();
    void Update();

    // INI
    bool InitFromINI(class CCINIClass* pINI);
    static bool LoadFromINIList(class CCINIClass* pINI);
    static int32 FindIndexByName(const char* pName);

    // Alliance
    void MakeAlly(HouseClass* pHouse);
    void MakeEnemy(HouseClass* pHouse);
    bool IsAlliedWith(HouseClass* pHouse) const;
    bool IsHostileTo(HouseClass* pHouse) const;
    static bool IsAllied(int32 house1, int32 house2);

    // ── Diplomacy (asm HouseClass_AlliedWith / _Belongs_To_Ally /
    //    _MakeEnemyByIdx / _IsIdxMP) ─────────────────────────────────────────
    // AlliedWith compares the caller's act-like index against the ally bitfield
    // rather than dereferencing a pointer, so it is safe on stale indices.
    bool Allied_With(int32 actLikeIndex) const;
    bool Belongs_To_Ally(TechnoClass* pTechno) const;
    bool Make_Enemy_By_Idx(int32 idx, bool unk);
    // IsIdxMP reports whether a country index names one of the seven
    // hard-coded multiplayer "special" countries (0x117B..0x1182).
    static bool Is_Idx_MP(int32 countryIndex);

    // House lookup by country index (asm HouseClass_FindByIndex_NoMP /
    // _YesMP).  The NoMP variant matches against each house's country index;
    // the YesMP variant additionally maps the two single-player country slots
    // (0x4475, 0x4476) onto slot 0/1.
    static HouseClass* Find_By_Index_No_MP(int32 idxCountry);
    static HouseClass* Find_By_Index_Yes_MP(int32 idxCountry);

    // The display name for a country index (asm HouseClass_NameFromIdx).
    // Multiplayer slots yield "<Player @ A>".."<Player @ H>"; everything else
    // defers to the house type's name.
    static const char* Name_From_Idx(int32 idxCountry, int32 fallback);

    // Threat-node flag (asm HouseClass_AcquiredThreatNode 0x50EFFC).
    void Acquired_Threat_Node() { HasThreatNode = true; }

    // ── IHouse accessors (asm: plain field loads through the IHouse face) ────
    // HouseClass_IHouse_PowerOutput (asm 0x4F8xxx): the house's total power
    //   production, as exposed to the IHouse COM interface.  The interface
    //   pointer sits +0x24 into the object, so the original reads the field
    //   through the full object address.
    int32 GetPowerOutput() const { return PowerOutput; }
    // HouseClass_IHouse_PowerDrain (asm 0x4F8xxx): total power consumption.
    int32 GetPowerDrain() const { return PowerDrain; }
    // HouseClass_IHouse_IDNumber (asm 0x4F8xxx): the house's array index, which
    //   doubles as its network/ID number.
    int32 GetIDNumber() const { return ArrayIndex; }

    // ── Mass destruction helpers (asm HouseClass_DestroyAllBuildings
    //    0x4FC798, _DestroyNonNavalNonBuildings 0x4FC82C, _DestroyAllNaval
    //    0x4FC8DC) ───────────────────────────────────────────────────────────
    // Each walks the global techno list and applies fatal damage to every
    // object owned by this house that matches the filter.  A wall of damage
    // (RulesClass-><slot at +0xFA8>) is used rather than a special kill path so
    // the normal death handling, wreckage and score all run.
    void Destroy_All_Buildings();
    void Destroy_Non_Naval_Non_Buildings();
    void Destroy_All_Naval();

    // HouseClass_Blowup_All (asm 0x4FC8xx).  Detonates every object the house
    // owns - buildings first, then the remaining units.  This is the "destroy
    // all of house X" trigger action.
    void Blowup_All();

    // HouseClass_RespawnStartingTechnos (asm 0x50xxxx).  Re-creates the
    // starting units and structures recorded for this house when the scenario
    // was read, dropping each one back at its stored spawn point.
    void Respawn_Starting_Technos();

    // HouseClass_RespawnStartingBuildings (asm 0x50xxxx).  The building-only
    // half of the same replay.
    void Respawn_Starting_Buildings();

    // HouseClass_RadarBlackout (asm 0x50C8C6).  Kills the house's radar for
    // `duration` frames (used by the lightning storm superweapon).
    void Radar_Blackout(int32 duration);

    // HouseClass_RelocateAllAt (asm 0x50xxxx).  Teleports every object owned
    // by this house to the given cell - used by the 'Teleport all to
    // waypoint' trigger action.
    void RelocateAllAt(const CellStruct& cell);

    // Game state
    void Win();
    void Lose();
    void DestroyAll();
    bool Defeated() const;
    void ScatterAllUnits();
    void UpdateSightAroundUnit(class TechnoClass* pUnit);

    // HouseClass_SetThreat (asm 0x4FA2DE).  Adds (or subtracts, for a negative
    // amount) threat at a grid coordinate.  The value is spread over nine
    // levels - each successive level shifts the amount right by 1 or 2 bits
    // and lands at a progressively larger radial offset - so a single threat
    // point bleeds outward into a small pyramid of influence.  Every slot is
    // clamped at zero.
    void Set_Threat(int32 coordHash, int32 threat);

    // HouseClass_RecalcThreats (asm 0x5093A8).  Clears the grid and re-seeds
    // it from every live object's threat value.
    void Recalc_Threats();
    void CheerAllUnits();
    void SetPrimaryFactory(int32 factoryID);

    // HouseClass_GetHomeCell (asm 0x50DF00).
    //
    //  Returns the house's home (base) cell, falling back to the default
    //  ion-cannon aim point when no base cell has been recorded yet.  The
    //  binary writes a packed wXY, so the project returns it by reference.
    const CellStruct& GetHomeCell() const;
    void SellCell(const CellStruct& cell);

    // ── Base mind-control (asm HouseClass_MindControlBaseOf 0x50D28F /
    //    HouseClass_ReturnControlBaseOf 0x50D2C3) ───────────────────────────
    // MindControlBaseOf transfers every building in the victim house's
    // building list to `this` via the capture path (vtable slot +0x3D4),
    // recording the original owner for later restoration.  ReturnControlBaseOf
    // walks this house's own building list and hands back every structure whose
    // recorded original owner is the given house.
    void MindControl_Base_Of(HouseClass* pHouse);
    void Return_Control_Base_Of(HouseClass* pHouse);

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

    // ========================================================================
    // Type multiplier dispatch (asm 0x50BD46..0x50C134 / 0x50D9C8 / 0x50CEB8)
    // ========================================================================
    // Each of these calls the object's WhatAmI() (vtable slot +0x2C), reduces
    // the result to a category, and returns the matching HouseTypeClass
    // multiplier.  The building arms further split on BuildCat == Combat.
    double Get_Cost_Mult(TechnoTypeClass* pType) const;
    double Get_Type_Cost_Mult(TechnoTypeClass* pType) const;
    double Get_Type_Armor_Mult(TechnoTypeClass* pType) const;
    double Get_Type_Build_Time_Mult(TechnoTypeClass* pType) const;
    double Get_Type_Speed_Mult(TechnoTypeClass* pType) const;
    double Get_Income_Mult() const;

    // Factory-plant cost bonuses (asm HouseClass_RecalcFactoryPlants 0x50BF80)
    void Recalc_Factory_Plants();

    // Self-heal steps (asm 0x50DA98 / 0x50DAA6)
    int32 Get_Inf_Self_Heal_Step() const;
    int32 Get_Unit_Self_Heal_Step() const;

    // Power (asm HouseClass_CurrentPowerPercentage 0x4FCE50)
    double Current_Power_Percentage() const;

    // Sidebar counters (asm HouseClass_GetCounter / HouseClass_EnableCounter)
    bool Get_Counter(int32 typeIndex, bool isNaval, int32 buildCat) const;
    void Enable_Counter(int32 typeIndex, bool isNaval, int32 buildCat);

    // Map edges (asm HouseClass_GetEdge / GetEdge_ / GetEdgeInverse)
    int32 Get_Edge() const;
    int32 Get_Edge_Inverse() const;

    // Ore bookkeeping (asm HouseClass_GetTotalWeed / DamagedForCredits)
    double Get_Total_Weed(int32 count, int32 threshold) const;
    void Damaged_For_Credits(HouseClass* pDamaged, int32 amount);

    // ========================================================================
    // Trivial accessors and COM helpers
    // ========================================================================
    AbstractType Get_Abstract_Derivation_ID() const { return AbstractType::House; }
    uint32 Add_Ref() const { return 1; }
    uint32 Release_Ref() const { return 1; }

    // Self-heal gates (asm HouseClass_DoInfantrySelfHeal / _DoUnitsSelfHeal).
    // Both simply report whether the accumulated gain is positive.
    bool Do_Infantry_Self_Heal() const { return InfantrySelfHeal > 0; }
    bool Do_Units_Self_Heal() const { return UnitsSelfHeal > 0; }
    bool Has_Powered_Centers() const { return PowerOutput > 0; }

    // HouseClass_HasPoweredCenters (asm 0x4FD030): true when the powered-center
    // counter at +0x2D8 is positive.
    bool HasPoweredCenters() const;
    // HouseClass_GetBuildingToProduce (asm 0x4FD040): resolves the house's
    // current primary-factory building type index into the type object, or null
    // when nothing is selected.
    BuildingTypeClass* GetBuildingToProduce() const;

    // Shroud (asm HouseClass_ReshroudMap).  A house with an active SpySat has
    // full vision and is left alone; every other house gets the map re-shrouded.
    void Reshroud_Map();

    // Base / target cell bookkeeping (asm HouseClass_Set*Cell / Clear*Cell).
    // A "cleared" cell is stored as the module's sentinel coordinate.
    void Set_Target_Cell(const CellStruct& cell);
    void Clear_Target_Cell();
    void Set_Some_Target_Cell(const CellStruct& cell);
    void Set_Defensive_Cell(const CellStruct& cell);
    void Clear_Defensive_Cell();
    void Set_Base_Cell(const CellStruct& cell);
    void Clear_Base_Cell();
    void Set_Base_Spawn_Cell(const CellStruct& cell);

    int32 Get_Size_Of_Class() const;

    // Super Weapons
    void CheckSWs();
    void FireSW(int32 swIndex);
    int32 FindSuperWeapon(SuperWeaponType type) const;

    // ── Superweapon firing (asm HouseClass_SWFire 0x4FB440 /
    //    _GenericSWFire 0x509BEC / _Fire_Paradrop .. _Fire_PsyDom) ──────────
    // SWFire performs the common gating (the weapon must exist, be charged and
    // not be disabled) and then dispatches on the superweapon type to the
    // matching Fire_* handler.  GenericSWFire is the generic entry used by the
    // "fire any ready offensive superweapon" AI path: it picks a target cell
    // and forwards to SWFire.
    bool SW_Fire(int32 swIndex, const CellStruct& target);
    void Generic_SW_Fire(int32 swIndex);
    void Fire_Paradrop();
    void Fire_LightningStorm();
    void Fire_GeneticMutator();
    void Fire_PsychicDominator();

    // Pick_Offensive_SWTarget - HouseClass_PickOffensiveSWTarget (asm
    // 0x50B0A0): score every enemy object in the house's tracking list and
    // return the cell of the best offensive superweapon target.  The result is
    // the module sentinel when nothing qualifies.
    CellStruct Pick_Offensive_SWTarget();

    // Pick_Offensive_SWTarget_AtWaypoint - HouseClass_PickOffensiveSWTargetAtWaypoint
    // (asm 0x50B3C0): as above but restricted to the house's planning waypoint
    // bearing the given index.
    CellStruct Pick_Offensive_SWTarget_AtWaypoint(int32 waypointIndex);

    // SW_Defend_Against - HouseClass_SWDefendAgainst (asm 0x4FAF93): the AI's
    // reaction to an inbound superweapon.  Only plain AI houses with an
    // AIDefendAgainst weapon react; a defence is scheduled when the incoming
    // strike is within the weapon's response range.
    void SW_Defend_Against(SuperClass* pSW, const CellStruct& target);

    // Resolve_Target_Index - the helper behind the `vtable[4]` (slot +0x10)
    // call every Fire_* routine makes on the house's target-class instance.
    // Maps a target cell onto the superweapon's own target index.
    int32 Resolve_Target_Index(const CellStruct& target) const;

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
    char                InitialName[21];
    uint8               pad_InitialName[3];
    wchar_t             CSFName[21];
    uint8               pad_CSFName[6];
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
    // Production gate at +0x1EE: while set the house cannot start new
    // production.  TeamClass_EnableHouseProduction clears it.
    bool                ProductionSuspended;
    // Sell-property stance at +0x1B9-ish: when set, every structure the house
    // owns is queued for sale.  TeamClass_ForceSale raises it.
    bool                SellEverything;
    uint32              AllyBitfield;
    uint32              EnemyBitfield;
    uint32              ActiveSuperWeapons;
    uint32              AvailableSuperWeapons;
    uint32              UsedSuperWeapons;
    char                UIName[0x40];
    int32               RatioAITriggerTeam;
    int32               RatioTeamAircraft;
    int32               RatioTeamInfantry;
    int32               RatioTeamUnits;
    int32               TechLevel;
    int32               DifficultyLevel;
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

    // ── Base outline bookkeeping (asm HouseClass+0x1460+0x5728 block) ──────
    // The four values below track the bounding rectangle of everything the
    // player has marked as "base".  BuildingClass_MarkBaseSpace grows the
    // rectangle as structures are placed; UnmarkBaseSpace shrinks it as they
    // are removed.  `BaseOutlineInitialised` records whether the rectangle
    // has ever been seeded (the binary tests the first field for zero).
    int32               BaseOutlineLeft;
    int32               BaseOutlineTop;
    int32               BaseOutlineWidth;
    int32               BaseOutlineHeight;

    // ── Target / base cell bookkeeping ────────────────────────────────────
    // Each is a wXY (two int16s).  A "cleared" slot holds the module sentinel
    // (0xFFFF, 0xFFFF) which the original stamps from
    // HouseClass_DefaultIonCannon_Coords.
    CellStruct          BestTargetCell;     // +0x546C
    CellStruct          BaseCell;           // +0x5494
    CellStruct          BaseSpawnCell;      // +0x5470
    CellStruct          TargetCell;         // +0x54F0
    CellStruct          DefensiveCell;      // +0x54F4
    int32               DefensiveCellField; // +0x54FC

    // ── Preferred defensive cell ──────────────────────────────────────────
    // Chosen by SW_Defend_Against when the AI decides to intercept an inbound
    // superweapon strike.  PreferredDefensiveCellStartTime records the frame at
    // which the pick was made so the choice can expire.  (asm +0x54D0 / +0x54D4)
    CellStruct          PreferredDefensiveCell;          // +0x54D0
    int32               PreferredDefensiveCellStartTime; // +0x54D4

    // ── Sidebar category counters ─────────────────────────────────────────
    // HouseClass_GetCounter / HouseClass_EnableCounter address these as a flat
    // run of bytes at house + 0x53D0 .. + 0x53D8.  Each flag mirrors whether
    // the corresponding cameo group is currently "lit" in the sidebar.
    //
    // The slot index equals the low byte of the house offset, so the enum
    // values are simply offset - 0x53D0.
    enum class CounterField : uint8 {
        Building      = 0x53D0 - 0x53D0,   // +0x53D0  Structures
        Defense       = 0x53D1 - 0x53D0,   // +0x53D1  Defensive structures
        Infantry      = 0x53D2 - 0x53D0,   // +0x53D2  Infantry (land)
        InfantryNaval = 0x53D3 - 0x53D0,   // +0x53D3  Infantry (naval)
        Unit          = 0x53D4 - 0x53D0,   // +0x53D4  Vehicles
        Aircraft      = 0x53D8 - 0x53D0,   // +0x53D8  Aircraft
        Count         = 9
    };
    uint8               Counters[static_cast<int32>(CounterField::Count)];

    // Set once the house has captured a threat node; consulted by the AI when
    // deciding whether to expand.  (asm field +0x1CE)
    bool                HasThreatNode;

    // Radar blackout state (asm +0x2B0 timer, +0x5779 flag).  Armed by
    // Radar_Blackout; while it is running the radar display is suppressed.
    CDTimerClass        RadarBlackoutTimer;
    int32               RadarBlackoutFrame;

    // ── Self-heal accumulation ────────────────────────────────────────────
    // Running totals of the InfantryGainSelfHeal / UnitsGainSelfHeal values
    // contributed by the house's structures.  Get_*_Self_Heal_Step multiplies
    // these by the rules-side per-tick amounts.
    int32               InfantrySelfHeal;
    int32               UnitsSelfHeal;

    // ── Powered centers / production pick ─────────────────────────────────
    // PoweredCenters counts the house's active powered structures (asm +0x2D8);
    // HasPoweredCenters is a mere "> 0" test over it.  BuildingTypeToProduce is
    // the index (asm +0x2A8) of the building the house's primary factory is
    // currently set to build, or -1 for none.
    int32               PoweredCenters;
    int32               BuildingTypeToProduce;

    // ── Refinery storage / resource totals ────────────────────────────────
    // TiberiumValue is the raw value of every tiberium type the house tracks
    // (Tiberiums_GetValue); the IHouse accessors scale and clamp it against
    // the house's total storage capacity.
    int32               TiberiumValue;
    int32               TotalStorageCapacity;

    // ── Factory plant registry ────────────────────────────────────────────
    // Structures flagged as factory plants.  Recalc_Factory_Plants walks this
    // list to rebuild the five cached cost multipliers.
    DynamicVectorClass<BuildingClass*>* FactoryPlants;

    // ── Damage ledger ─────────────────────────────────────────────────────
    // A flat run of {house, accumulated credit value} pairs maintained for
    // the "who hurt us the most" bookkeeping performed by Damaged_For_Credits.
    struct DamageRecord {
        HouseClass* House;
        int32       Total;
    };
    DamageRecord*       DamageLedger;
    int32               DamageLedgerCount;

    // Index of the house currently judged to be our primary aggressor, or -1.
    int32               PrimaryAggressor;

    // Owned type counts
    int32               OwnedUnitTypeCounts[MaxTypeCounts];
    int32               OwnedInfantryTypeCounts[MaxTypeCounts];
    int32               OwnedAircraftTypeCounts[MaxTypeCounts];
    int32               OwnedBuildingTypeCounts[MaxTypeCounts];
    int32               OwnedUnitTypeCountsEver[MaxTypeCounts];
    int32               OwnedInfantryTypeCountsEver[MaxTypeCounts];
    int32               OwnedAircraftTypeCountsEver[MaxTypeCounts];
    int32               OwnedBuildingTypeCountsEver[MaxTypeCounts];

    // ── Threat grid ───────────────────────────────────────────────────────
    // A 130 x 130 grid of accumulated per-cell threat, held per house.  The
    // binary clears and addresses it as a flat run of 0x4204 dwords starting
    // at house + 0x57E4, indexed directly by MapClass::Cell_Region.
    enum { ThreatGridWidth = 130, ThreatGridHeight = 130 };
    static constexpr int32 ThreatGridCellCount = ThreatGridWidth * ThreatGridHeight;  // 0x4204
    int32               ThreatGrid[ThreatGridCellCount];

    // Tracking lists
    DynamicVectorClass<UnitClass*>       OwnedUnits;
    DynamicVectorClass<InfantryClass*>   OwnedInfantry;
    DynamicVectorClass<AircraftClass*>   OwnedAircraft;
    DynamicVectorClass<BuildingClass*>   OwnedBuildings;

    // vec_Conyards (asm +0x108): the subset of OwnedBuildings that are
    // construction yards.  Kept as its own list because the AI reads
    // element zero directly when picking a base anchor.
    DynamicVectorClass<BuildingClass*>   OwnedConyards;
    DynamicVectorClass<TechnoClass*>     AllOwnedObjects;
    DynamicVectorClass<TechnoClass*>     TrackingList;

    // Super weapon timers
    CDTimerClass        SuperWeaponTimers[MaxSuperWeapons];

    // Superweapon instances owned by this house.  Indexed in parallel with
    // SuperWeaponTimers / the ActiveSuperWeapons bitfield.
    DynamicVectorClass<SuperClass*>* SuperWeapons;

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