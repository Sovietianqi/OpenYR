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
#include <Houses/BaseClass.h>

// Forward declarations
class TagClass;
class CCINIClass;
class INIClass;      // asm: 基类 —— CCINIClass : public INIClass
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

 // HouseClass_DefaultIonCannon_Coords.  The module sentinel
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

    // ------------------------------------------------------------------------
    // HouseClass::Is_Human (0x50B740)
    //
    //   asm:
    //     mov eax,Session / test eax,eax / mov al,[ecx+HouseClass.IsHuman]
    //     jnz ret                        ; in a session -> answer is just IsHuman
    //     test al,al / jnz true
    //     mov al,[ecx+HouseClass.IsPlayer]
    //     test al,al / jnz true
    //     xor al,al / ret                ; neither -> false
    //
    //   项目字段对应：IsHuman -> IsHumanPlayer(+0x1EC)，IsPlayer -> PlayerControl。
    //   注意 Session 非空时直接返回 IsHuman，不会再看 PlayerControl —— 多人
    //   对局里"玩家所属"是按 IsHuman 标记而不是控制权来判定的。
    // ------------------------------------------------------------------------
    bool Is_Human() const;
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

    // HouseClass_GetCRC (asm 0x50BAE0): vtable CRC entry.  Hashes the base
    // AbstractClass state plus the house-specific scalars (ActLike index,
    // the type's country index, the colour triple and the four credit
    // accumulators) so two peers in a session can detect divergence.
    void Get_CRC(CRCEngine& crc) const;

    // HouseClass_SaveToMapINIList (asm 0x501160).  Writes every house's
    // index into the map's [Houses] registry, then delegates each house's own
    // fields to SaveToMapINI.
    static void Save_To_Map_INI_List(INIClass* pINI);
    // HouseClass_SaveToMapINI (asm 0x5012A0).  Persists a single house's
    // map-scope fields (country, tech level, credits, IQ, edge, control
    // flags, ...) under its own section of the map INI.
    void Save_To_Map_INI(INIClass* pINI);

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

    // ------------------------------------------------------------------------
    // asm: HouseClass::Init_Data (0x501xxx, retn 0Ch)
    //
    //   Colour = 颜色槽；Side = 国家/阵营下标；Money = 初始资金。
    //     [this+1DCh] = Money                      ; 资金
    //     [this+30Ch] = Money                      ; 资金镜像
    //     [Type(0x34h)+CountryClass.Color] = Colour
    //     [this+16054h] = Colour                   ; ColorSchemeIndex
    //
    //   Assign_Houses 每新建一个房子就调一次（剧本房 + AI 房各一次）。
    // ------------------------------------------------------------------------
    void Init_Data(int32 colour, int32 side, int32 money);

    // ------------------------------------------------------------------------
    // asm: HouseClass::Assign_Handicap (0x4F6F50, retn 4)
    //
    //   把难度写进 AIDifficulty，再按"是否有 Session"分两条路算乘子：
    //     有 Session：Difficulties[diff].X * Type->XMult
    //     无 Session：直接取 Difficulties[diff].X
    //   然后按 TeamDelays[diff][PlayerNumber] 重排 TeamDelayTimer。
    // ------------------------------------------------------------------------
    void Assign_Handicap(int32 newDifficulty);

    // ------------------------------------------------------------------------
    // asm: HouseClass::Harvested (0x554176, retn 8)
    //
    //   SiloMoney += floor(percentage * 1.0)
    //   Credits   += floor(Tiberium.Value * Type->IncomeMult * percentage)
    //   返回 percentage（原版 fstp 前把入参留在 st0）。
    // ------------------------------------------------------------------------
    float Harvested(float percentage, int32 idxTiberium);

    // ------------------------------------------------------------------------
    // asm: HouseClass_50BA00 (0x50BA00) —— 由本房的颜色三分量算归一化色向：
    //   len = sqrt(r^2+g^2+b^2)；len == 0 时三分量都取 255，
    //   否则各分量除以 len 后存回向量槽。Assign_Houses 每建房一次就调一次。
    // ------------------------------------------------------------------------
    void Recalc_Color_Vector();
    // ========================================================================
    // asm: HouseClass_Update (0x4F83E8) —— 阵营逐帧更新主循环。按 asm 顺序：
    //   黑屏计时到点复位 -> 雷达黑屏计时到点复位 -> RecheckItems 触发
    //   Update_Power -> RadarOutage 时 Radar_Activate + SpySat_Update ->
    //   每 100 帧衰减 AngerNodes -> 自动建造开关 -> 胜/败计时 -> ...
    //   末尾在非人类阵营时走 AI()。逐段见 .cpp。
    // ========================================================================
    void Update();
    // ========================================================================
    // asm: HouseClass_Update_AI (0x4FD4D0) —— 阵营 AI 主循环（逐帧）。逐段见 .cpp。
    //   非 MP 会话/难度判定后：Update_AI_TryFireSW -> 按 Mode 决定是否跑
    //   建造型 AI（AI_Build_Income / Expert_AI_Building / AI_Building ...）。
    //   返回值 = Random2(1,7)+0x69，由 HouseClass_Update 写入 AITimer.TimeLeft
    //   作为下一次调度的间隔（asm retn 前的 `add eax,69h`）。
    // ========================================================================
    int32 AI();

    // ========================================================================
    // asm: HouseClass::AI_Building (0x4FE3B0) —— 从基地节点表里挑下一个
    //   可造建筑，写回 BuildingTypeToProduce。逐段见 .cpp。
    // ========================================================================
    void AI_Building();
    // ========================================================================
    // asm: HouseClass::AI_Unit (0x4FEA40) —— 选一个可造的载具（含采矿车/
    //   精炼厂）写回 UnitTypeToProduce。逐段见 .cpp。
    // ========================================================================
    void AI_Unit();
    // ========================================================================
    // asm: HouseClass::AI_Infantry (0x4FEEB0) —— 选一个可造的步兵，写回
    //   InfantryTypeToProduce。逐段见 .cpp。
    // ========================================================================
    void AI_Infantry();
    // ========================================================================
    // asm: HouseClass::AI_Aircraft (0x4FF1E0) —— 选一个可造的飞机，写回
    //   AircraftTypeToProduce。逐段见 .cpp。
    // ========================================================================
    void AI_Aircraft();
    // ========================================================================
    // asm: HouseClass::Expert_AI_Building (0x4FDD00) —— AI 的"专家建造"：
    //   按优先级依次探 防御/发电/科技/兵营/重工/精炼厂 清单，挑出第一个
    //   可造项并写回 BuildingTypeToProduce；无可造项时清空该索引。
    // ========================================================================
    void Expert_AI_Building();

    // ------------------------------------------------------------------------
    // asm: 建造类"能不能造"判定（AI_Building/AI_Unit 里反复出现的
    //   `call [type+0x94h](type, 1, 1, 0, this)`）——本房能否生产该类型。
    //   原版该虚函数综合 Owners/前置/上限；这里落到 Can_Build 等价判定。
    // ------------------------------------------------------------------------
    bool Can_Produce_Type(TechnoTypeClass* pType) const;

    // ========================================================================
    // asm: HouseClass_Update 里的 AI 建造状态机字段（+0x1C0）。
    //   0/1/2 三种状态按资金与建筑构成轮转，决定本帧跑哪几路 AI_*。
    // ========================================================================
    int32               AIBuildState;

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
 // HouseClass_IHouse_PowerOutput (xxx): the house's total power
    //   production, as exposed to the IHouse COM interface.  The interface
    //   pointer sits +0x24 into the object, so the original reads the field
    //   through the full object address.
    int32 GetPowerOutput() const { return PowerOutput; }
 // HouseClass_IHouse_PowerDrain (xxx): total power consumption.
    int32 GetPowerDrain() const { return PowerDrain; }
 // HouseClass_IHouse_IDNumber (xxx): the house's array index, which
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

 // HouseClass_Blowup_All (xx).  Detonates every object the house
    // owns - buildings first, then the remaining units.  This is the "destroy
    // all of house X" trigger action.
    void Blowup_All();

 // HouseClass_RespawnStartingTechnos (xxxx).  Re-creates the
    // starting units and structures recorded for this house when the scenario
    // was read, dropping each one back at its stored spawn point.
    void Respawn_Starting_Technos();

 // HouseClass_RespawnStartingBuildings (xxxx).  The building-only
    // half of the same replay.
    void Respawn_Starting_Buildings();

 // HouseClass_RadarBlackout.  Kills the house's radar for
    // `duration` frames (used by the lightning storm superweapon).
    void Radar_Blackout(int32 duration);

 // HouseClass_RelocateAllAt (xxxx).  Teleports every object owned
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

 // HouseClass_SetThreat.  Adds (or subtracts, for a negative
    // amount) threat at a grid coordinate.  The value is spread over nine
    // levels - each successive level shifts the amount right by 1 or 2 bits
    // and lands at a progressively larger radial offset - so a single threat
    // point bleeds outward into a small pyramid of influence.  Every slot is
    // clamped at zero.
    void Set_Threat(int32 coordHash, int32 threat);

 // HouseClass_RecalcThreats.  Clears the grid and re-seeds
    // it from every live object's threat value.
    void Recalc_Threats();
    void CheerAllUnits();
    void SetPrimaryFactory(int32 factoryID);

 // 根据游戏行为，可知 GetHomeCell 负责下面这段逻辑。
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
    // asm: 阵营经济三入口（+0x30C Credits / +0x2DC CreditsSpent）：
    //   GiveMoney    (0x4F9960, retn 4)  —— Credits += amount，无其它副作用。
    //   SpendMoney   (0x4F9740, retn 4)  —— 先扣 Credits；不足时按矿仓储量
    //                 折算补足，最后把本次实付累加到 CreditsSpent 并联动
    //                 建筑侧的资金事件（sub_4F9970）。
    //   AvailableMoney —— 仅返回 Credits（+0x30C）；TakeMoney 是 SpendMoney
    //                 的布尔化封装（够则扣并返回 true）。
    // ========================================================================
    int32 AvailableMoney() const { return Credits; }
    bool  TakeMoney(int32 amount);
    // ========================================================================
    // asm: HouseClass_Update_Unit_Counts —— 重算 +Unit/Infantry/Aircraft/
    //   Building 五档的"现存/累计"计数（供侧边栏与 AI 查询）。返回技术单位
    //   总数。逐段见 .cpp。
    // ========================================================================
    int32 Update_Unit_Counts();
    // ========================================================================
    // asm: HouseClass 里 +0x2DCh（DamageLedger 累计伤害值）的增减更新。
    //   正数表示本房对外造成的伤害额，负数表示吸收的伤害额；供
    //   Damaged_For_Credits / 计分簿使用。
    // ========================================================================
    void Update_Damage_Charge(int32 amount) { DamageCharge += amount; }

    // ========================================================================
 // Type multiplier dispatch (..0x50C134 / 0x50D9C8 / 0x50CEB8)
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

 // Self-heal steps
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

    // ========================================================================
    // 根据游戏行为，可知 Update_Power 负责按当前建筑构成重算本阵营的电力
    //  收支（发电与耗电两个累计值），并把结果写回阵营自身的电力字段，供
    //  侧边栏与 AI 查询。
    // ========================================================================
    void Update_Power();

    // ========================================================================
    // 根据游戏行为，可知 Update_Factories_Timing 负责刷新本阵营各类工厂的
    //  数量（建筑 / 步兵 / 陆上载具 / 水上载具 / 飞机五档），生产耗时按类别
    //  分摊到对应的工厂数上。
    // ========================================================================
    void Update_Factories_Timing();

    // ========================================================================
    // 根据游戏行为，可知 UpdateSWs 按帧推进本阵营所有超级武器的充能；
    //  UpdateAllSW 是"全部阵营一起推进"的外层入口。
    // ========================================================================
    void UpdateSWs();
    static void UpdateAllSW();

    // ========================================================================
    // 根据游戏行为，可知 Can_Afford_These 判断本阵营能否同时负担起一批
    //  建造项（逐项累加单价乘数量后与可用资金比较）。
    // ========================================================================
    bool Can_Afford_These(TechnoTypeClass** ppTypes, int32* pCounts, int32 count) const;

    // ========================================================================
    // 根据游戏行为，可知 ShouldDisableCameo 判断侧边栏上某个建造图标是否
    //  该被置灰（前置条件不足 / 科技等级不够 / 已达数量上限）。
    // ========================================================================
    bool ShouldDisableCameo(TechnoTypeClass* pType) const;

    // ========================================================================
    // 根据游戏行为，可知 PrerequisiteIsAvailable 判断本阵营是否已具备建造
    //  某个项目所需的前置条件（逐条核对前置建筑清单的拥有情况）。
    // ========================================================================
    bool PrerequisiteIsAvailable(TechnoTypeClass* pType) const;

    // ========================================================================
    // 根据游戏行为，可知 DoInfantrySelfHeal 与 DoUnitsSelfHeal 按帧给本阵营
    //  的步兵 / 载具补血，前提是阵营已挂上对应的自愈加成。
    // ========================================================================
    void DoInfantrySelfHeal();
    void DoUnitsSelfHeal();

    // ========================================================================
    // 根据游戏行为，可知 SendSpyPlane 派出一架侦察机前往目标格（需雷达在线
    //  且冷却已过）；CreateAirstrike 类似地安排一次空袭。
    // ========================================================================
    bool SendSpyPlane(const CellStruct& target);
    bool CreateAirstrike(const CellStruct& target);

    // ========================================================================
    // 根据游戏行为，可知 UpdateFlagCoords 负责把本阵营的旗帜重新挂到最新的
    //  基地中心格上，让玩家一眼看到自己的基地在哪。
    // ========================================================================
    void UpdateFlagCoords();

    // ========================================================================
    // 根据游戏行为，可知 RegisterTechnoGain_PrereqCounters 在某个单位/建筑
    //  落成时把"已获得"事实写进对应的计数表，从而解锁依赖它的建造项；
    //  RegisterObjectGain_FromFactory 是工厂出货路径上的入口。
    // ========================================================================
    void RegisterTechnoGain_PrereqCounters(TechnoClass* pTechno);
    void RegisterObjectGain_FromFactory(TechnoClass* pTechno);

    // ========================================================================
    // 根据游戏行为，可知 UnitFromFactory 是工厂完成产品时的统一出口：指定
    //  归属、送到集结点、登记计数。
    // ========================================================================
    bool UnitFromFactory(TechnoClass* pTechno, BuildingClass* pFactory, bool a3);

    // ========================================================================
    // 根据游戏行为，可知 GetFactoryProducingThis 找出本阵营当前正在生产指定
    //  项目的那座工厂。
    // ========================================================================
    BuildingClass* GetFactoryProducingThis(TechnoTypeClass* pType) const;

    // ========================================================================
    // asm: HouseClass::Fetch_Factory (0x5004C0)
    //
    //   FactoryClass *__thiscall Fetch_Factory(HouseClass *this,
    //                                          eAbstractIDs id, char naval,
    //                                          int buildcat)
    //
    // 按建造分类取本阵营对应工厂：原版用 house+0x53AC..0x53CC 六张缓存槽，
    // 这里改为按工厂建筑的 Type->Factory 归类实时查找（OpenYR 没有那六张槽）。
    // ========================================================================
    FactoryClass* Fetch_Factory(int32 abstractId, bool isNaval,
                                int32 buildCat) const;

    // ========================================================================
    // 根据游戏行为，可知 SuspendProductionOf 把正在生产的工厂挂起（进度保留），
    //  AbandonProductionOf 则彻底放弃并退还部分资金。
    // ========================================================================
    bool SuspendProductionOf(TechnoTypeClass* pType);
    bool AbandonProductionOf(TechnoTypeClass* pType);

    // ========================================================================
    // 根据游戏行为，可知 SetScoutDestination 给本阵营记下一个侦察目的地，
    //  已有的目标不会被覆盖。
    // ========================================================================
    void SetScoutDestination(const CellStruct& cell);

    //========================================================================
    // 根据游戏行为，可知下面一层是对外 COM 接口族的转发面板：调用者以
    // 接口名访问，这里转回本类的对应实现，接口行为随实现走。
    //========================================================================
    HRESULT IRTTITypeInfo_QueryInterface(REFIID iid, LPVOID* ppv);
    ULONG   IRTTITypeInfo_AddRef();
    ULONG   IRTTITypeInfo_Release();
    HRESULT IConnectionPointContainer_QueryInterface(REFIID iid, LPVOID* ppv);
    ULONG   IConnectionPointContainer_AddRef();
    ULONG   IConnectionPointContainer_Release();
    HRESULT IConnectionPointContainer_EnumConnectionPoints(void** ppEnum);
    HRESULT IConnectionPointContainer_FindConnectionPoint(REFIID riid, void** ppCP);
    HRESULT IHouse_Name(wchar_t** pVal);
    HRESULT IHouse_PowerOutput(int32* pVal);
    HRESULT IHouse_CategoryPower(int32* pVal);
    HRESULT IHouse_CategoryQuantity(int32* pVal);
    HRESULT IHouse_AllToHunt();
    HRESULT IHouse_BaseCenter(CellStruct* pVal);
    HRESULT IHouse_FireSale();
    HRESULT IHouse_PowerDrain(int32* pVal);
    HRESULT IHouse_IDNumber(int32* pVal);
    HRESULT IHouse_GetApplication(void** ppApp);
    HRESULT IHouse_QueryInterface(REFIID iid, LPVOID* ppv);
    ULONG   IHouse_AddRef();
    ULONG   IHouse_Release();
    HRESULT IPublicHouse_Name(wchar_t** pVal);
    HRESULT IPublicHouse_ApparentCategoryPower(int32* pVal);
    HRESULT IPublicHouse_ApparentCategoryQuantity(int32* pVal);
    HRESULT IPublicHouse_ApparentBaseCenter(CellStruct* pVal);
    HRESULT IPublicHouse_IsPowered(bool* pVal);
    HRESULT IPublicHouse_IDNumber(int32* pVal);
    HRESULT IPublicHouse_QueryInterface(REFIID iid, LPVOID* ppv);
    ULONG   IPublicHouse_AddRef();
    ULONG   IPublicHouse_Release();

    //========================================================================
    // 根据游戏行为，可知下面是阵营层的战斗与 AI 行为入口：防御单位挑选、
    // 基地墙网维护、电力中心得失、超级武器开火、生产推进与初始部队生成。
    //========================================================================
    static int32 PlayerColorType_Convert_(int32 nColor);
    // ========================================================================
    // asm: HouseClass_AI_Building_Defense_Misc (0x508430, __thiscall) —— AI 的
    //   "沿基地边界砌墙"。不是造某一座建筑，而是把基地矩形四周扫一遍，把
    //   "墙段 + 段中点一座门"的候选节点成批写进 Base 的节点表（+5704h），
    //   随后由 AI_Building 按顺序把它们变成真正的生产序列。
    //
    //   逐段：
    //     * 基地区域矩形 [+5754h]={left,top,width,height} 外扩一格
    //       （left/top 各 -1、width/height 各 +2），结束时整体写回 [+5754h]。
    //     * Rules 三条列表各取一个"本房可造"的建筑原型：
    //       +0A50h=ConcreteWalls 作墙，+0A88h=EWGates、+0A6Ch=NSGates 作门。
    //     * 以基地中心格 [+5750h] 的高度层号为基准，沿警戒圈的"南/北外沿"
    //       与"东/西内列"四条线逐格扫描。
    //     * 判据（格本身 + 朝外一格同时满足）：地面不是 水/岩石/冰、
    //       高度层号与中心相差 <= 2、本格无叠加物、两格都无建筑与地形物件、
    //       本格坡度为 0、本格在雷达范围内。连续满足的长度 >= 5 才算一段。
    //     * 段内在 len/2-1 处放门（南/北边用 EW 门，东/西列用 NS 门），
    //       其余格子放墙；先落墙段、再落门。
    //   逐段对照见 .cpp。
    // ========================================================================
    void AI_Building_Defense_Misc();
    // ========================================================================
    // asm: HouseClass::Attacked (0x4F93E0, retn 4) —— 本房（或其盟军）的单位
    //   遭到攻击时由 BuildingClass::Take_Damage 调用。按受害对象类别给出
    //   对应的雷达事件（矿车/基地/盟军）与 EVA 播报，并向本房全部标签
    //   RaiseEvent(6)。逐段见 .cpp。
    // ========================================================================
    void Attacked(TechnoClass* pSource);
    // ========================================================================
    // asm: HouseClass_Update 里读 +1ECh=RecheckItems / +1EDh=recheckTechLevels
    //   两个"脏标记"：置位后下一次 Update 重算电力与科技可用项，然后清零。
    //   这里给出显式的重算入口，语义与 asm 的两处清零一致。
    // ========================================================================
    bool Recheck_Items();
    bool Recheck_Tech_Levels();
    TechnoClass* PickAntiAirDefense();
    TechnoClass* PickAntiArmorDefense();
    TechnoClass* PickAntiInfantryDefense();
    static HouseClass* FindPlayerAtX(int32 index);
    void EndScenario();
    // asm HouseClass_FirstBuildableTechno_FromVector (0x5051xx, retn 4):
    //   list 为 TechnoTypeClass* 向量。
    TechnoTypeClass* FirstBuildableTechno_FromVector(DynamicVectorClass<TechnoTypeClass*>* pList);
    // asm HouseClass_FindFirstAccessibleTechnoTypeFromArray (0x505320, retn 4).
    TechnoTypeClass* FindFirstAccessibleTechnoTypeFromArray(DynamicVectorClass<TechnoTypeClass*>* pList);
    bool DoWeNeedWalls();
    int32 Base_WhatNeedsWalls();
    bool Base_ShouldRebuildNow();
    // asm HouseClass_LostPoweredCenter / _GotPoweredCenter (retn 4): 电力中心
    //  计数增减，并让机械单位上线/下线。参数 type 为受影响单位的类型索引。
    void LostPoweredCenter(int32 type);
    void Fire_GenMutator(int32 a3);
    bool ClickWaypoint_unused(const CoordStruct* pPos);
    void GotPoweredCenter_unused(int32 type);
    void LostPoweredCenter_unused(int32 type);
    void GotPoweredCenter(int32 type);
    bool CanSeePsiWarning();
    void ActivateBaseBuilding(BuildingClass* pBuilding);
    void AcquiredConyard(BuildingClass* pConyard);
    void IonCannonFire(const CellStruct& target);
    void SpySat_Update();
    void SellOverlay(const CellStruct& cell);
    void CreateParadrop(const CellStruct& target);
    CellStruct GetPlanningWaypointAt(int32 index);
    int32 GetPlanningWaypointProperties(int32 index);
    void AcquiredThreatNode(const CellStruct& cell);
    void Fire_PsyDom(const CellStruct& target);
    // asm: HouseClass_TryFindEligibleAITriggers (0x6F0AF2, retn 4)
    //   int __fastcall(VectorPtrAITeamTypeClass *resultVector, HouseClass *Owner, int arg)
    //   pResult 为 null 时只返回"是否命中"；非空时把选中触发器的
    //   Team1 / Team2 依次追加进去（原版 resultVector 的语义）。
    bool TryFindEligibleAITriggers(DynamicVectorClass<TeamTypeClass*>* pResult = nullptr);
    bool CanInstantiateTeam(TeamTypeClass* pTeamType);
    // ========================================================================
    // asm: HouseClass_Update 的建队段 (asm:552780..552900, loc_4F8A00..loc_4F8B08)
    //
    //   HouseClass_Update 在跑 AI() 之前先过 TeamDelayTimer 时间窗；到点后
    //   HouseClass_TryFindEligibleAITriggers 选出的 TeamType 逐个交给
    //   TeamTypeClass_CreateTeam 成军，再把下一次窗口写回 TeamDelayTimer
    //   （时长 = Rules->TeamDelays[AIDifficulty]）。逐段见 .cpp。
    // ========================================================================
    void Update_Team_Production();
    // asm HouseClass_FindRepairBay (0x455xxx，retn，__fastcall this/unit):
    //  在己方舰船/维修坞中挑选离 pUnit 最近、且在修理半径内的那座。
    BuildingClass* FindRepairBay(TechnoClass* pUnit);
    void Update_AI_TryFireSW();
    void Radar_Activate();
    void GenerateStartingUnits();
    // ========================================================================
    // asm: HouseClass_GetTechnoToProduce (0x4FBD80, retn 8)
    //   TechnoTypeClass *__thiscall(HouseClass *this, int TechnoKind, int a3)
    //   按 TechnoKind（AbstractType 值）把本房对应的 "+56xx ToProduce" 索引
    //   解析回类型对象；未设置（-1）或未知类别返回 null。
    // ========================================================================
    TechnoTypeClass* GetTechnoToProduce(AbstractType technoKind);
    // ========================================================================
    // asm: HouseClass::Find_Building (0x4FD060, retn 8)
    //   BuildingClass *__thiscall(HouseClass *this, int idx, int distance)
    //   在本房已拥有该类建筑（按类型数组下标 idx 过滤）中挑一座未处于 limbo
    //   的；distance != -1 时还要求它到本房目标格的路径距离相等。找不到返回
    //   null。
    // ========================================================================
    BuildingClass* Find_Building(int32 idx, int32 distance);
    // ========================================================================
    // asm: HouseClass::Find_Unit / Find_Infantry / Find_Aircraft
    //   （0x4FDxxx，均为 retn 8 形态的 "Find_<Kind>(idx, distance)" 家族）
    //   与 Find_Building 同构：在对应类别的己方对象列表里挑出类型数组下标
    //   等于 idx、未处于 limbo，且（distance==-1 或路径距离相等）的第一个。
    //   三者共用下面的模板 helper。
    // ========================================================================
    TechnoClass* Find_Unit(int32 idx, int32 distance);
    TechnoClass* Find_Infantry(int32 idx, int32 distance);
    TechnoClass* Find_Aircraft(int32 idx, int32 distance);
    void DroppedFlag();
    void CreateWaypointPath(const CellStruct& from, const CellStruct& to);
    void CreateWaypointPath2(const CellStruct& from, const CellStruct& to, int32 hop);
    void Update_AI_Production(int32 arg0 = -1);
    // ========================================================================
    // asm: HouseClass::AI_Build_Income (0x4FD980, __thiscall this) ——
    //   AI 的"补经济"评估：不可负担时检查是否有采矿单位/精炼厂在线，必要时
    //   从 Rules 的 _HarvesterUnit 列表挑一个可建造的采矿车加入生产。返回
    //   1 表示"需要继续建"，0 表示无需动作。逐段见 .cpp。
    // ========================================================================
    int32 AI_Build_Income();
    void CreateBulletWarningAnim(const CellStruct& target);
    void LoadVectors();

 // HouseClass_HasPoweredCenters: true when the powered-center
    // counter at +0x2D8 is positive.
    bool HasPoweredCenters() const;
 // HouseClass_GetBuildingToProduce: resolves the house's
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
    // 根据游戏行为，可知基地中心是基地集合的几何中心，AI 用它来挑选防御与
    // 建造的参考点；清空时写入哨兵坐标表示"尚未计算"。
    void Set_Base_Center(const CellStruct& cell);
    void Clear_Base_Center();
    void Set_Base_Spawn_Cell(const CellStruct& cell);

    // ------------------------------------------------------------------------
    // asm: HouseClass::Flag_Attach(HouseClass_old* this, _ObjectClass* a2, int what)
    //   把本阵营的旗帜挂到 a2 上：先让旧旗主收旗（UpdateFlagCoords），
    //   再让 a2 举起本阵营的旗，最后记下新的旗主。a2 为空或仍在 limbo
    //   时不做事并返回假。
    // ------------------------------------------------------------------------
    bool Flag_Attach(TechnoClass* pObject, int32 what);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 GetBaseCenterCell 负责给出本阵营的基地中心格：
    //  若中心尚未计算过（仍是哨兵值），就退回基地的锚点格；否则用真正的
    //  中心。AI 挑防御位置与放建筑时都拿它当参考点。
    // ------------------------------------------------------------------------
    CellStruct GetBaseCenterCell() const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 Get_Coord_From_Base_Center 负责在基地中心附近挑
    //  一个可用的空地作为出兵/放兵的落点：从中心格开始按给定方向与步长
    //  向外扩散试探，返回第一个可站立的位置；附近全被占满时就退到中心格。
    // ------------------------------------------------------------------------
    CellStruct Get_Coord_From_Base_Center(int32 a1, int32 a2) const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 CountInstancesOfTeam 负责数出属于本阵营、且编成
    //  指定的这种队伍类型的队伍数量，AI 用它判断某种编队是否已经够多。
    // ------------------------------------------------------------------------
    int32 CountInstancesOfTeam(TeamTypeClass* pTeamType) const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 CanAddPlanningWaypoint 负责判断本阵营能否再添加
    //  一条规划路线：路线上限为 12 条，且候选槽位必须为空、已经没有任何
    //  路径占用它。
    // ------------------------------------------------------------------------
    bool CanAddPlanningWaypoint() const;

    // 根据游戏行为，可知间谍切断电力后调用它：把本阵营标记为"电力受损"，
    // 并按给定的持续帧数启动断电计时。
    void Create_Power_Outage(int32 duration);

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
 //: as above but restricted to the house's planning waypoint
    // bearing the given index.
    CellStruct Pick_Offensive_SWTarget_AtWaypoint(int32 waypointIndex);

 // SW_Defend_Against - HouseClass_SWDefendAgainst: the AI's
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
    // asm HouseClass+0x30 —— 玩家编号。外交屏用它与行索引比较：相等即"本机
    //   玩家"，行索引折回 0（即 dword_83923C/_83925C 的 -1 槽）。
    int32               PlayerNumber = 0;
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
    // asm +0x1F5 —— 外交屏用的"灰显"标记（为真时名字静态文本用 "Grey" 配色）
    bool                IsGreyedOut = false;
    // asm +0x53E4..+0x5433 / +0x5438.. —— 20 类击杀统计（外交屏的击杀列 =
    //   两张表逐项求和；asm RadarClass_Diplomacy 的 sub_658330 里
    //   `add ebx,53E4h` 之后 20 次 `[ebx] + [ebx+54h]` 累加即此）。
    int32               KilledUnits[20] = {};
    int32               TotalKilledUnits = 0;
    int32               KilledBuildings[20] = {};
    int32               TotalKilledBuildings = 0;
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

    // 根据游戏行为，可知 Get_Factory_Counts 返回本阵营"当前正在生产的工厂
    // 数量"，并且按要生产的东西分门别类： Gebäude(建筑)、步兵、载具(陆上/水上
    // 两种)、飞机各有一份独立计数。生产耗时的计算要用到对应类别的那一份，
    // 工厂越多生产越快，所以这里必须按类别分别存放，而不是一个总数。
    int32               FactoryCountBuilding;    // +0x5378
    int32               FactoryCountUnitNaval;   // +0x537C
    int32               FactoryCountInfantry;    // +0x5380
    int32               FactoryCountUnit;        // +0x5384
    int32               FactoryCountAircraft;    // +0x5388

    // 根据游戏行为，可知它按被生产对象的类别挑选对应的工厂计数；类别与计数
    // 的归属关系由规则侧的映射表决定，取不到时返回零。
    int32 Get_Factory_Counts(int32 objType, bool naval) const;
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

    // ------------------------------------------------------------------------
    // 原版把整个 BaseClass 内嵌在 HouseClass 里（asm 里表现为
    //   `add ecx, HouseClass.Base` 之后调用 BaseClass_* 系列）。
    // 这里同样内嵌一份，BaseClass_Placed / WhatNeedsWalls / ShouldRebuildNow
    // 都通过它访问起始基地的节点表。
    //
    // 注意它与上面的 BaseCenter / BaseNodesCount 不是同一份数据：那两个是
    // 项目早期的扁平占位字段（基地中心格、节点计数），而 Base 才是原版真正
    // 存放节点数组（Nodes/NodeCount/PercentBuilt/OwnerHouseIndex）的对象。
    // ------------------------------------------------------------------------
    BaseClass           Base;

    // 根据游戏行为，可知 PlanningWaypoints 是玩家在地图上画出的若干条
    //  行军路线的槽位，共 12 条：每条路线可以是一条折线。AI 与玩家都用它
    //  预先规划进攻或撤退的走法，路线为空时该槽位可以再被占用。
    static constexpr int32 MaxPlanningPaths = 12;
    void*               PlanningWaypoints[MaxPlanningPaths];
    int32               NextPlanningWaypointSlot;

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
    // 根据游戏行为，可知 DefaultIonCannon_Coords 负责下面这段逻辑。
    CellStruct          BestTargetCell;     // +0x546C
    CellStruct          BaseCell;           // +0x5494
    CellStruct          BaseSpawnCell;      // +0x5470
    CellStruct          TargetCell;         // +0x54F0
    CellStruct          DefensiveCell;      // +0x54F4
    int32               DefensiveCellField; // +0x54FC

    // ── CTF flag bookkeeping ──────────────────────────────────────────────
    // asm: HouseClass_old.IHouse._.ObjectWithOurFlag / .FlagCoords —— 本阵营
    //   "旗帜"当前挂在哪台单位上，以及旗帜所在格。Create_Units 落好基地车后
    //   把两者清零，CTF 模式下再经 Flag_Attach 把旗挂到基地车上。
    TechnoClass*        ObjectWithOurFlag = nullptr;
    CellStruct          FlagCoords;         // +0x54F8 起的两字 wXY

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

    // 根据游戏行为，可知 Create_Power_Outage 记录"本阵营被间谍切断电力"
    // 这一状态：间谍潜入电厂成功后，给本阵营装上一段计时，并立刻把"电力是否
    // 被破坏"的标记置位；计时期间本阵营的电力供应按规则惩罚，倒计时结束后由
    // 更新流程把标记复位。
    bool                IsPowerSabotaged;
    CDTimerClass        PowerOutageTimer;
    int32               PowerOutageFrame;

    // ── Self-heal accumulation ────────────────────────────────────────────
    // Running totals of the InfantryGainSelfHeal / UnitsGainSelfHeal values
    // contributed by the house's structures.  Get_*_Self_Heal_Step multiplies
    // these by the rules-side per-tick amounts.
    int32               InfantrySelfHeal;
    int32               UnitsSelfHeal;

    // ── Powered centers / production pick ─────────────────────────────────
    // PoweredCenters counts the house's active powered structures (asm +0x2D8);
    // HasPoweredCenters is a mere "> 0" test over it.
    //
    // The four "*ToProduce" indices cache which type each category's primary
    // factory is currently building (asm +0x564C/+0x5650/+0x5654/+0x5658);
    // GetTechnoToProduce resolves them back into type objects.  -1 = none.
    // asm HouseClass_GetTechnoToProduce reads exactly these four slots:
    //   +0x564C  BuildingTypeToProduce
    //   +0x5650  UnitTypeToProduce
    //   +0x5654  InfantryTypeToProduce
    //   +0x5658  AircraftTypeToProduce
    int32               PoweredCenters;
    // 根据游戏行为，可知 RecheckItems/recheckTechLevels 是阵营层的两个"脏
    //  标记"：某处改变了建筑构成（如电力中心得失、建筑被摧毁）时置位，下一次
    //  Update 重算电力与科技可用项后清零。原版偏移见上（+1EC/+1ED）。
    bool                RecheckItems;
    bool                recheckTechLevels;
    // asm: +0x2DCh —— 本房"对外造成的伤害总值"累计；Attack/受击记账使用。
    int32               DamageCharge;
    int32               BuildingTypeToProduce;
    int32               UnitTypeToProduce;
    int32               InfantryTypeToProduce;
    int32               AircraftTypeToProduce;

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

    // asm HouseClass_EndScenario (0x4FCDB6) 里的 +0x298 时间戳：当本阵营
    //  已经胜/败时记录“场景结束”的帧号，-1 表示尚未记录。
    int32               EndScenarioTimer;

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

    // asm: HouseClass +0x5610 —— TimerStruct { TimeStart, TimeLeft }。
    //   HouseClass_Update 用它节流 HouseClass_Update_AI：AI() 返回
    //   Random2(1,7)+0x69（106..112 帧），写入 TimeLeft 作为下一次的间隔。
    CDTimerClass AITimer;

    // asm: HouseClass +0x5624 —— TeamDelayTimer。
    //   与 AITimer 不同：它节流的是「AI 触发器 → 组建新 Team」那一段
    //   （asm:552777 loc_4F8A00 的 TimeStart/TimeLeft 比较），而不是 AI()
    //   本身。到点时取 Rules->TeamDelays[DifficultyLevel] 作下一次窗口。
    CDTimerClass TeamDelayTimer;
    int32 LastAirCombatTime, LastSpySatTime, LastIronCurtainTime, LastForceShieldTime;

    // 根据游戏行为，可知规划路径点除对象表外还有一份坐标侧记录，供
    // 查询与画线直接取格。
    CellStruct PlanningWaypointCoords[8];
    int32 PlanningWaypointCoordCount;
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

    // ========================================================================
    // Assign_Houses 批新增的真实成员（asm 偏移见注释）
    // ========================================================================
    // asm +0x16058 —— 起始槽：StartingSpotClass_GetStartSlot 的结果，
    //   AI 房则取 AISlots.StartSlots[i]。
    int32 StartingSlot;
    // asm +0x1605C —— 队伍号：剧本房的 node+63h（idxTeam），
    //   AI 房取 AISlots[+0x60]（-1 表示无队伍，同时把 Scenario+11E0h 置位）。
    int32 TeamIndex;
    // asm AllowWinBlocks —— 本房已挂载的"允许胜利"标签实例计数，
    //   ScenarioClass::Fill_In_Data 的 AllowWin 扫描逐个加一。
    int32 AllowWinBlocks;
    // asm SiloMoney —— 筒仓内矿石折算的金额（Harvested 累加）。
    int32 SiloMoney;
    // asm +0x30Ch —— 初始资金镜像：Init_Data 与 Credits 写同一个值，
    //   记分/结算面板读这一份。
    int32 InitialCredits;
    // asm +0x38 —— 本房挂载的 TagClass 实例表（DynamicVectorClass）。
    DynamicVectorClass<TagClass*> Tags;

    // asm FirepowerMultiplier / GroundspeedMultiplier / AirspeedMultiplier /
    //     ArmorMultiplier / ROFMultiplier / CostMultiplier /
    //     BuildTimeMultiplier / RepairDelay / BuildDelay
    //   Assign_Handicap 按难度表计算出来的本房乘子。
    double FirepowerMultiplier;
    double GroundspeedMultiplier;
    double AirspeedMultiplier;
    double ArmorMultiplier;
    double ROFMultiplier;
    double CostMultiplier;
    double BuildTimeMultiplier;
    double RepairDelay;
    double BuildDelay;

    // asm +0x56F9 / +0x56FA / +0x56FB —— 本房颜色三分量（字节），
    //   HouseClass_50BA00 读取它们算归一化色向。
    uint8 HouseColorR, HouseColorG, HouseColorB;
    // 归一化后的色向（HouseClass_50BA00 的结果）。
    double ColorVecR, ColorVecG, ColorVecB;
};