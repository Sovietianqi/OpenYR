#pragma once

#include <Abstract/TechnoClass.h>
#include <Core/Macros.h>
#include <Containers/DynamicVectorClass.h>

// Forward declarations for fields/types used by BuildingClass.
class FactoryClass;
class InfantryClass;
class LightSourceClass;
class TargetClass;
class CellClass;
class AnimClass;
class BuildingLightClass;
class WarheadTypeClass;
class INIClass;
class AirstrikeClass;

// ============================================================================
// BStateType - the high-level state a building's art/logic is in. Mirrors the
// original binary's BStateType enumeration.
// ============================================================================
enum class BStateType : int32 {
    Construction = 0,
    Idle         = 1,
    Active       = 2,
    Full         = 3,
    Aux1         = 4,
    Aux2         = 5,
    None         = -1
};

// ============================================================================
// BuildingAnimSlot - index into the per-building animation array. The
// original binary uses a fixed array of 0x15 (21) animation slots.
// ============================================================================
enum class BuildingAnimSlot : int32 {
    None          = -1,
    Default       = 0,
    Active,
    Special1,
    Special2,
    Special3,
    Idle1,
    Idle2,
    Idle3,
    Aux1,
    Aux2,
    Aux3,
    Turret,
    Garrison,
    Garrisoned,
    Damaged,
    DamagedActive,
    DamagedIdle,
    Construct,
    Sell,
    PoweredOff,
    Reserve
};

// Number of animation slots carried by every BuildingClass instance.
static constexpr int32 BUILDING_ANIM_SLOT_COUNT = 21;
// Number of damage-fire animation slots.
static constexpr int32 BUILDING_DAMAGE_FIRE_ANIM_COUNT = 8;
// Number of upgrade type slots a building may carry.
static constexpr int32 BUILDING_UPGRADE_COUNT = 3;

// ============================================================================
// BuildingAnimationClass - manages building animation frames
// ============================================================================
class BuildingAnimationClass {
public:
    BuildingAnimationClass() : AnimationValue(0) {}

    void Update();
    void SetAnimation(int32 state, int32 frame);
    int32 GetCurrentFrame() const;

    int32 AnimationValue;
    int32 CurrentFrame;
    bool IsAnimating;
    bool IsDamaged;
};

// ============================================================================
// BuildingClass - base for all buildings (inherits TechnoClass directly, NOT FootClass)
// ============================================================================
class NOVTABLE BuildingClass : public TechnoClass {
public:
    static const AbstractType AbsID = AbstractType::Building;
    static DynamicVectorClass<BuildingClass*>* Array;

    // ========================================================================
    // IPersistStream
    // ========================================================================
    virtual HRESULT __stdcall Load(IStream* pStm) override;
    virtual HRESULT __stdcall Save(IStream* pStm, BOOL fClearDirty) override;

    // ========================================================================
    // Destructor
    // ========================================================================
    virtual ~BuildingClass();

    // ========================================================================
    // TechnoClass overrides
    // ========================================================================
    virtual bool IsPowerOnline() const override;
    virtual bool IsUnitFactory() const override;
    virtual bool IsArmed() const override;
    virtual bool CanOccupyFire() const override;
    virtual double GetStoragePercentage() const override;
    virtual int32 GetRefund() const override;
    virtual BulletClass* Fire(AbstractClass* pTarget, int32 nWeaponIndex) override;
    virtual void Uncloak(bool bPlaySound) override;
    virtual void Cloak(bool bPlaySound) override;
    virtual bool IsClearlyVisibleTo(HouseClass* House) const override;
    virtual bool CanScatter() const override;

    // ========================================================================
    // BuildingClass virtuals
    // ========================================================================
    virtual void Sell(DWORD dwUnk);
    virtual bool CanBeSold() const;
    virtual bool CanBeRepaired() const;
    virtual void RepairWithMoney(int32 money);
    virtual bool SWAvailable() const;
    virtual bool SW2Available() const;
    virtual bool IsControllable() const override;
    virtual bool IsSelectable() const override;
    virtual bool CanBeSelected() const override;
    virtual bool CanBeSelectedNow() const override;
    virtual bool IsManaDrainPossible() const;
    virtual AbstractClass* FindFactoryTarget(AbstractClass* pTarget) const;
    virtual bool HasNavigationDeal() const;
    virtual bool IsFactory() const;
    virtual bool IsFactoryExplicit() const;
    virtual bool IsToggledRallyPoint() const;

    // ========================================================================
    // BuildingClass specific virtuals - production, power, garrison
    // ========================================================================
    virtual bool CanFireNow() const;
    virtual bool CanEnterCell(CellClass* pCell) const;
    virtual void Place(bool captured);
    virtual void UpdateConstructionOptions();
    virtual void Draw(const Point2D& point, const RectangleStruct& rect);
    virtual DirStruct FireAngleTo(ObjectClass* pObject) const;
    virtual void Destroy(DWORD dwUnused, TechnoClass* pTechno, bool NoSurvivor, CellStruct& cell);
    virtual bool TogglePrimaryFactory();
    virtual void SensorArrayActivate(CellStruct cell);
    virtual void SensorArrayDeactivate(CellStruct cell);
    virtual void DisguiseDetectorActivate(CellStruct cell);
    virtual void DisguiseDetectorDeactivate(CellStruct cell);
    virtual int32 AlwaysZero();
 // BuildingClass_GetCurrentWeaponStage: the building's
    // current firing stage (+0x140), advanced for multi-stage weapons
    // (gattling / prism towers).
    int32 GetCurrentWeaponStage() const;
    virtual bool ForceCreate(CoordStruct& coord, DWORD dwUnk);
    virtual CellStruct FindExitCell(DWORD dwUnk, DWORD dwUnk2) const;
    virtual int32 DistanceToDockingCoord(ObjectClass* pObj) const;
    virtual void BeginMode(BStateType bType);
    virtual void GoOnline();
    virtual void GoOffline();
    virtual int32 GetPowerOutput() const;
    virtual int32 GetPowerDrain() const;
    virtual void EnableStuff();
    virtual void DisableStuff();
    virtual void EnableTemporal();
    virtual void DisableTemporal();
    virtual void UpdateAnimations();
    virtual int32 GetCurrentFrame() const;
    virtual bool IsAllFogged() const;
    virtual void SetRallypoint(CellStruct* pTarget, bool bPlayEVA);
    virtual int32 FirstActiveSWIdx() const;
    virtual int32 SecondActiveSWIdx() const;
    virtual int32 GetShapeNumber() const;
    virtual void FireLaser(CoordStruct Coords);
    virtual bool IsBeingDrained() const;
    virtual bool UpdateBunker();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 Absorber 负责判断"这座建筑是否会吞掉开进来的载具/
    //  步兵"：只要建筑类型带 UnitAbsorb 或 InfantryAbsorb（粉碎机一类）即为真。
    // ------------------------------------------------------------------------
    bool Absorber() const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 Update_Prism 负责推进棱镜塔的充能/开火状态机：
    //  按当前阶段递减倒计时，倒计时走完后依阶段执行——先"锁定目标"，
    //  再"向目标发射棱镜光束"，最后复位阶段。非充能阶段时直接结束。
    // ------------------------------------------------------------------------
    void Update_Prism();
    virtual void KillOccupants(TechnoClass* pAssaulter);
    virtual bool MakeTraversable();
    virtual bool CheckFog() const;
    virtual bool IsTraversable() const;
    virtual void UnloadBunker();
    virtual void ClearBunker();
    virtual void EmptyBunker();
    virtual void AfterDestruction();
    virtual void DestroyNthAnim(BuildingAnimSlot Slot);
    virtual void PlayAnim(const char* animName, BuildingAnimSlot Slot, bool Damaged, bool Garrisoned, int32 effectDelay);
    virtual void ToggleDamagedAnims(bool isDamaged);
    virtual void CreateEndPost(bool arg);
    virtual DWORD GetFWFlags() const;
    virtual int32 GetOccupantCount() const;

 // 根据游戏行为，可知 CanBeOccupied 负责下面这段逻辑。
    //
    //  Gate for garrisoning a civilian structure with `pInfantry`.  Grounds
    //  for refusal, in the binary's order: a null occupant; a type that is not
    //  occupiable; a building mid-construction or mid-sale; a building whose
    //  cell is not usable; a warping-out building; an Occupier infantry whose
    //  house neither matches the building's owner nor is a multiplayer-passive
    //  (neutral) house; a full occupant roster; a building on red health; and
    //  a mind-controlled infantry.
    bool CanBeOccupied(InfantryClass* pInfantry) const;
    virtual bool AddOccupant(InfantryClass* pInfantry);
    virtual bool RemoveOccupant(InfantryClass* pInfantry);
    virtual InfantryClass* GetOccupant(int32 index) const;
    virtual void FireFromOccupant(TechnoClass* pTarget);
    virtual int32 GetOccupantWeaponIndex() const;
    virtual bool HasSuperWeapon(int32 index) const;
    virtual TechnoTypeClass* GetSecretProduction() const;
    virtual void SetTarget(AbstractClass* pTarget);
    virtual AbstractClass* GetTarget() const;
    virtual void ClearTarget();
    virtual bool HasTarget() const;
    virtual void SetMission(Mission mission);
    virtual Mission GetMission() const;
    virtual void QueueMission(Mission mission);
    virtual void MissionAttack();
    virtual void MissionGuard();
    virtual void MissionSleep();
    virtual void MissionConstruction();
    virtual void MissionSelling();
    virtual void MissionRepair();
    virtual void MissionActive();
    virtual void MissionIdle();
    virtual void UpdateMission();
    virtual void AI_Update();
    virtual void Combat_AI();
    virtual void Production_AI();
    virtual void Power_AI();
    virtual void Fire_At(TargetClass* pTarget, int32 weaponIndex);
    virtual bool Can_Fire_At(TechnoClass* pTarget, int32 weaponIndex) const;
    virtual int32 GetWeaponRange(int32 weaponIndex) const;
    virtual int32 GetWeaponDamage(int32 weaponIndex) const;
    virtual void MuzzleFlash(int32 weaponIndex);
    virtual void OnFired(int32 weaponIndex);
    virtual int32 GetWeaponCount() const;
    virtual void TakeDamage(int32 damage, TechnoClass* pSource, WarheadTypeClass* pWarhead);
    virtual void OnDestroyed();
    virtual void OnCaptured(HouseClass* pNewOwner);
    virtual void OnVeterancyUp();
    virtual bool IsVisibleTo(HouseClass* pHouse) const;
    virtual void RevealTo(HouseClass* pHouse);
    virtual int32 GetSightRange() const;
    virtual int32 GetArmor() const;
    virtual int32 GetMaxHealth() const;
    virtual int32 GetHealth() const;
    virtual void SetHealth(int32 hp);
    virtual bool IsAlive() const;
    virtual bool IsDead() const;
    virtual bool IsDamaged() const;
    virtual bool IsGreenHP() const;
    virtual bool IsYellowHP() const;
    virtual bool IsRedHP() const;
    virtual float GetHealthRatio() const;

    // ========================================================================
    // Power accounting (asm BuildingClass_PowerProduced 0x44E7C0 /
    // BuildingClass_PowerAbsorbed 0x44E890)
    //
    //  PowerProduced sums the type's [General] power contribution, the
    //  upgrade bonus, the extra-power flags and the output of the three
    //  attached upgrade modules, then scales the whole figure by the
    //  structure's health when it is damaged (`conditionYellow`).
    //  PowerAbsorbed adds up the type's drain plus its upgrade drain.
    //  Both return 0 for an offline structure.
    // ========================================================================
    int32 PowerProduced() const;
    int32 PowerAbsorbed() const;

 // BuildingClass_UndamageAllAnims.  Switches every damage
    // animation on the structure between the "damaged" and "undamaged"
    // variants; `conditionYellow` selects the damaged set.
    void UndamageAllAnims(bool conditionYellow);

 // BuildingClass_PlaySomeAnim.  Replays the structure's
    // idle animation set - used after an upgrade installs new anims.
    void PlaySomeAnim(int32 a2);

 // BuildingClass_AddOverpowerer / _RemoveOverpowerer:
    // register / unregister a Tesla trooper boosting this structure.
    void AddOverpowerer(InfantryClass* pInfantry);
    void RemoveOverpowerer(InfantryClass* pInfantry);

    // ========================================================================
    // Upgrade slots.  `UpgradeLevel` (asm +0x702) counts the installed upgrade
    // modules held in `Upgrades[]`; the engine stacks at most
    // BUILDING_UPGRADE_COUNT of them.
    // ========================================================================

 // BuildingClass_CanReceiveUpgrade.  True when `pType` is a
    // legal upgrade module for this structure owned by `pHouse`: the module's
    // `PowersUpBuilding` must name our type, the house must match, and we must
    // still have a free slot (or the module carries no PowersUpToLevel gate).
    bool CanReceiveUpgrade(BuildingTypeClass* pType, HouseClass* pHouse) const;

 // BuildingClass_LoseUpgrade.  Removes the most recently
    // installed upgrade module, destroys its slot anim, and (when the module
    // had a super weapon) re-evaluates the owner's SWs.  Returns true when an
    // upgrade was actually removed.
    bool LoseUpgrade();

    // BuildingClass_InstallUpgrade - the mirror of LoseUpgrade.  Pushes a
    // module onto the upgrade stack, plays its install anim and bumps the
    // upgrade level.  Returns false when the stack is already full.
    bool InstallUpgrade(BuildingTypeClass* pType);

 // BuildingClass_GetRangeOfRadial.  The radius in cells of
    // the structure's radial indicator (psychic detection / gap generator /
    // cloak generator / weapon range).
    int32 GetRangeOfRadial() const;

 // BuildingClass_GetTintColor.  Applies the red/blue/green
    // modulation the structure owes to iron-curtain / airstrike states.
    int32 GetTintColor(int32 color);

 // BuildingClass_RGBModulate.  Iron-curtain blue tint.
    int32 RGBModulate(int32 color);

 // BuildingClass_RGBModulate2.  Airstrike red tint.
    int32 RGBModulate2(int32 color);

 // BuildingClass_IsAllShrouded.  True when every cell of the
    // structure's foundation is still under shroud.
    bool IsAllShrouded() const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 GetShrouded 负责统计建筑地基范围内"还盖着黑幕"
    //  的格子：遍历地基的每一格，凡是被黑幕遮住的就收集起来；同时把那些
    //  已经探明的格子顺手揭开。返回是否至少找到了一格被遮住的。
    //  传入的容器用于接收这些被遮住的格号。
    // ------------------------------------------------------------------------
    bool GetShrouded(DynamicVectorClass<CellStruct>* pFoggedCells,
                     DynamicVectorClass<CellStruct>* pVisibleCells,
                     int32 a3);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 Remove_Ocupents 负责把驻守本建筑的步兵全部请出去：
    //  先清掉"正在开火的那个驻守者"的编号，再逐个把驻守者从建筑里放出到
    //  附近空地，最后清空驻守名册。建筑被摧毁或易主时用它。
    // ------------------------------------------------------------------------
    void Remove_Ocupents(int32 a1, int32 a2);

 // BuildingClass_GetTiberiumFillPercentage.  Ore storage
    // occupancy in percent (0..100) for refineries and silos.
    int32 GetTiberiumFillPercentage() const;

 // BuildingClass_SelectAutoTarget.  Folds the AG/AA flags of
    // weapon slots 0 and 1 into the incoming projectile mask and delegates the
    // actual acquisition to TechnoClass::Greatest_Threat.
    ObjectClass* SelectAutoTarget(int32 projFlags, int32 curThreat, int32 a4);

 // BuildingClass_SaveToMapINIList.  Walks the global building
    // array writing every placed, non-limbo structure into the map INI.
    static void SaveToMapINIList(INIClass* pINI);

 // BuildingClass_SaveToMapINI.  Serializes one structure's
    // placement record into the map INI under [Structures].
    void SaveToMapINI(INIClass* pINI) const;

 // BuildingClass_InitMore.  Post-construction fix-ups: light
    // source reset, cloak/gap registration, laser-fence post ping and the
    // powered-unit anim sweep.
    void InitMore();

 // BuildingClass_GetTurretChangingWeapon.  The weapon the
    // structure would fire right now, honouring its current turret stage.
    WeaponStruct* GetTurretChangingWeapon() const;

 // BuildingClass_PingLaserFencePost.  Registers this
    // structure as a laser-fence post in the wall network and (when `bAdd`)
    // also re-pings the neighbouring posts so the fence rebuilds its links.
    void PingLaserFencePost(bool bAdd);

    // ========================================================================
    // 根据游戏行为，可知激光围栏立柱需要登记/摘除、每帧核对供电、按方向
    //  接线：这四个入口共同维护"柱子-激光"网络的本地状态。
    // ========================================================================
    void EnableFencePost(bool bEnable);
    void ManageFencePost();
    bool SetupLaserFenceForDirection(int32 dir);
    void SetupLaserFences();

    // ========================================================================
    // 根据游戏行为，可知 Set_Rally_To_Point 把工厂集结点挪到指定格，只改
    //  数据、不播 EVA 语音。
    // ========================================================================
    void Set_Rally_To_Point(const CellStruct& cell);

    // ========================================================================
    // 根据游戏行为，可知受损建筑要按受损档点点起火情表现，CreateDamageFires
    //  负责登记火情槽位并返回当前火情数量。
    // ========================================================================
    int32 CreateDamageFires();

    // ========================================================================
    // 根据游戏行为，可知 ProcessAnims 是建筑动画的每帧裁决入口：推进各槽位、
    //  按血量切换受损动画、刷新火情。
    // ========================================================================
    void ProcessAnims();

    // ========================================================================
    // 根据游戏行为，可知升级模块装/卸之后要一并刷新动画槽位与炮塔武器。
    // ========================================================================
    void UpdateAnimsAndTurretAfterUpgrade();

    // ========================================================================
    // 根据游戏行为，可知 Update_Factory 是工厂生产线的一帧推进入口。
    // ========================================================================
    void Update_Factory();

    // ========================================================================
    // 根据游戏行为，可知 KillOccupiers 把驻守步兵全部处死并清空名册（区别
    //  于只清空不放人的 KillOccupants）。
    // ========================================================================
    void KillOccupiers(TechnoClass* pAssaulter);

    // ========================================================================
    // 根据游戏行为，可知 GetObjectActivityState 报告建筑当前的活动状态
    //  （0=待命 1=建造中 2=出售中 3=修理中 4=受损未修）。
    // ========================================================================
    int32 GetObjectActivityState() const;

    // ========================================================================
    // 根据游戏行为，可知 GetAnimLengths 把建筑各动画槽位的帧数抄给调用方，
    //  没有对应动画的槽位填零。
    // ========================================================================
    void GetAnimLengths(int32* pLengths, int32 count) const;

    // ========================================================================
    // 根据游戏行为，可知 Mi_Repair / Mi_Missile 是建筑侧的使命处理步骤：
    //  前者每步修一格血、修满自动结束；后者在普通建筑上留空、直接完成。
    // ========================================================================
    bool Mi_Repair();
    bool Mi_Missile();

    // ========================================================================
    // 根据游戏行为，可知 RGBModulate1 按建筑状态（铁幕/断电）返回颜色调制
    //  强度，渲染层据此调整色板。
    // ========================================================================
    int32 RGBModulate1() const;

    // ========================================================================
    // 根据游戏行为，可知 AllocateDockedVector 为"已停靠单位"分配名册，重复
    //  调用直接复用已有的。
    // ========================================================================
    bool AllocateDockedVector();

    // ========================================================================
    // 根据游戏行为，可知光标悬停的形状判定有两个入口：悬在建筑本体与悬在
    //  地基格，最终都折算成光标编号。
    // ========================================================================
    int32 GetCursorOverObject(int32 currentCursor, bool a2) const;
    int32 GetCursorOverCell(const CellStruct& cell, int32 currentCursor) const;

    // ========================================================================
    // 根据游戏行为，可知 Captured 是建筑被占领的统一入口：换主人、清驻守者、
    //  记"被占领过"。
    // ========================================================================
    void Captured(HouseClass* pNewOwner);

    // ========================================================================
    // Base-space marking.  When a structure is placed, the engine stamps its
    // owner's bit into CellClass::BaseSpacerOfHouses over a rectangle grown
    // by two rings (the buildable "spacer" area around the foundation).  The
    // owner's base bounding rectangle is grown to cover the same region.
    // ========================================================================

 // 根据游戏行为，可知 MarkBaseSpace 负责下面这段逻辑。
    void MarkBaseSpace(bool mark);

 // 根据游戏行为，可知 UnmarkBaseSpace 负责下面这段逻辑。
    void UnmarkBaseSpace();

 // BuildingClass_IsBibOccupied.  When the type declares a
    // bib (the concrete apron in front of a factory), any unit sitting on the
    // bib is shooed away.  Returns true when the structure has a bib.
    bool IsBibOccupied();

 // BuildingClass_SetAnimTranslucency.  Applies a
    // translucency level to all of the structure's animation slots.  Slot 15
    // (the upgrade anim) is remapped to 16 so it does not share the building
    // body's fade.
    void SetAnimTranslucency(int32 slot);

 // BuildingClass_DestroyAllAnims.  Tears down every active
    // animation slot, optionally matching a specific anim pointer.
    void DestroyAllAnims(AnimClass* pAnim);

 // BuildingClass_PingMore.  The powered-on counterpart of
    // InitMore: starts the light source, re-pings the laser fence post and
    // notifies every attached powered unit.
    void PingMore();
    virtual void Repair(int32 amount);
    virtual void Kill();
    virtual int32 GetValue() const;
    virtual int32 GetCost() const;
    virtual int32 GetBuildTime() const;
    virtual DirStruct GetDirection() const;
    virtual void SetDirection(DirStruct dir);
    virtual CoordStruct GetCoords() const;
    virtual void SetCoords(CoordStruct coords);
    virtual void Stop();
    virtual void Hold();
    virtual bool IsIdle() const;
    virtual void SetIdle();
    virtual void Freeze();
    virtual void Unfreeze();
    virtual bool Limbo();
    virtual bool Unlimbo();
    virtual bool InLimbo() const;
    virtual void Mark(MarkType mark);
    virtual void Unmark();
    virtual void Sync();
    virtual void Unsync();
    virtual void Lock();
    virtual void Unlock();
    virtual bool IsLocked() const;
    virtual void Disable();
    virtual void Enable();
    virtual bool IsDisabled() const;
    virtual void Activate();
    virtual void Deactivate();
    virtual bool IsActive() const;
    virtual void Cloak();
    virtual void Decloak();
    virtual bool IsCloaked() const;
    virtual void EMPulse();
    virtual void UnEMP();
    virtual bool IsEMPed() const;
    virtual void IronCurtain();
    virtual void UnIronCurtain();
    virtual bool IsIronCurtained() const;
    virtual void ForceShield();
    virtual void UnForceShield();
    virtual bool IsForceShielded() const;
    virtual void ChronoShift();
    virtual void TemporalWarp();
    virtual void UnTemporal();
    virtual bool IsTemporalWarped() const;
    virtual void MindControl(TechnoClass* pTarget);
    virtual void UnMindControl();
    virtual bool IsMindControlled() const;
    virtual void Disguise();
    virtual void UnDisguise();
    virtual bool IsDisguised() const;
    virtual bool IsBuilding() const;
    virtual AbstractType WhatAmI() const;
    virtual int32 Size() const;
    virtual void Destroyed(ObjectClass* Killer);

    // ========================================================================
    // Constructor
    // ========================================================================
    BuildingClass(HouseClass* pOwner) noexcept;

protected:
    explicit __forceinline BuildingClass(noinit_t) noexcept : TechnoClass(noinit) {}

    // ========================================================================
    // Properties
    // ========================================================================
public:
    BuildingAnimationClass Animation;
    BuildingTypeClass* Type;
    bool IsConstructed;
    bool IsBeingDrained_;   // backing store for IsBeingDrained() (renamed to avoid name clash)
    bool IsOnline;
    bool IsPowerPlant;
    bool IsOverpowered;
    BYTE align_52D[3];
    int32 PowerOutput;
    int32 PowerDrain;

    // ========================================================================
    // BuildingClass specific state - production, power, garrison, anims.
    // These named members back the BuildingClass virtual methods. They mirror
    // the layout/semantics of the original binary's BuildingClass fields.
    // ========================================================================
    FactoryClass*       Factory;                       // production queue owner
    BStateType          BState;                        // current building art/logic state
    BStateType          QueueBState;                   // state queued for next transition
    InfantryClass*      C4AppliedBy;                   // infantry that planted C4 on us
    bool                C4Applied;                     // C4 has been planted
    AnimClass*          Anims[BUILDING_ANIM_SLOT_COUNT];     // per-slot active anims
    bool                AnimStates[BUILDING_ANIM_SLOT_COUNT];// whether each anim was enabled
    AnimClass*          DamageFireAnims[BUILDING_DAMAGE_FIRE_ANIM_COUNT];
    BuildingTypeClass*  Upgrades[BUILDING_UPGRADE_COUNT];    // installed upgrade types
    // UpgradeLevel (+0x702): number of live entries in Upgrades[].  Read as a
    // signed byte by the binary (BuildingClass_LoseUpgrade, GetWeapon, ...).
    int8                UpgradeLevel;
    // GapSuperCharged: this structure's gap generator is being overcharged by
    // a nearby Psychic Beacon, switching it to SuperGapRadiusInCells.
    // (asm BuildingClass offset used by GetRangeOfRadial.)
    bool                GapSuperCharged;
    // IsGeneratingGap (+[edi+6EB]): a gap field is currently being projected.
    bool                IsGeneratingGap;
    // CloakRadius (+CloakRadius, byte): the radius latched by InitMore for a
    // cloak generator / sensor structure.
    int8                CloakRadius;
    // AirstrikeImUsing (+AirstrikeImUsing): the airstrike super-weapon that
    // is currently inbound to this structure, or null.
    AirstrikeClass*     AirstrikeImUsing;
    // PoweredUnits (+0x55C, three slots): the external units this structure is
    // feeding power to, parallel to BuildingTypeClass::PoweredUnit[].
    TechnoClass*        PoweredUnits[BUILDING_POWERED_UNIT_COUNT];
    int32               FiringSWType;                  // super-weapon currently launching
    BuildingLightClass* Spotlight;                     // attached building light
    int32               GateTimer;                     // frames remaining for gate anim
    LightSourceClass*   LightSource;                   // tiled light source
    bool                HasPower;                      // power is currently available
    bool                RegisteredAsPoweredUnitSource;  // registered w/ powered-unit system
    DWORD               SupportingPrisms;              // prism chain contribution count
    // ConditionYellow (+0x6E6): latched "structure is showing its damaged
    // variant" state; BuildingClass_UndamageAllAnims owns it.
    bool                ConditionYellow;
    bool                HasExtraPowerBonus;
    bool                HasExtraPowerDrain;
    DynamicVectorClass<InfantryClass*> Overpowerers;   // tesla troopers boosting us
    DynamicVectorClass<InfantryClass*> Occupants;      // garrisoned infantry

    // 根据游戏行为，可知激光围栏立柱需要记住"自己是否在线上"与"哪些方向
    //  接了激光"：IsFencePostActive 是在线标记，FenceLinkMask 的每一位对应
    //  一个方向的连线。
    bool                IsFencePostActive;             // laser-fence post online
    int32               FenceLinkMask;                 // per-direction laser links

    // 根据游戏行为，可知火情表现要记下每处火所在的槽位，修好或卖掉时一并
    //  熄掉；槽位数量有上限，坐标 -1 表示空槽。
    static constexpr int32 MaxDamageFireCells = 4;
    CellStruct          DamageFireCells[MaxDamageFireCells];
    int32               DamageFireCount;

    // 根据游戏行为，可知机场/船厂要记录哪些单位正停靠在这里，名册指针按需
    //  分配；DockedUnitsCapacity 记录预留容量。
    DynamicVectorClass<TechnoClass*>* DockedUnits;
    int32               DockedUnitsCapacity;
    int32               FiringOccupantIndex;           // occupant whose weapon fires next
    bool                WasOnline;                     // online state at last Update()
    bool                StuffEnabled;                  // set by EnableStuff/DisableStuff
    bool                BeingProduced;                 // AI_REBUILDABLE flag
    bool                ShouldRebuild;                 // AI_REPAIRABLE flag
    bool                HasBeenCaptured;               // ownership changed at least once
    bool                IsFogged;                      // currently hidden by fog
    bool                IsSensorActive;                // sensor array online
    bool                IsDetectorActive;              // disguise detector online
    bool                IsFrozen_;                     // frozen by temporal/chronoshift
    bool                IsLocked_;                     // update lock held
    bool                IsDisabled_;                   // explicitly disabled
    bool                IsDisguised_;                  // disguised (spy)
    bool                IsMindControlled_;             // under mind control

    // House that owned this structure before it was mind-controlled away.
    // Set by HouseClass::MindControl_Base_Of and cleared by
    // HouseClass::Return_Control_Base_Of.
    HouseClass*         OriginallyOwnedBy;             // +0x2E0
    bool                IsPrimaryFactory;              // primary factory for its type
    int32               BunkerState;                   // garrison state machine value
    int32               PrismStage;                    // prism charge state
    int32               WeaponStage;                   // +0x140 multi-stage firing counter
    CoordStruct         PrismTargetCoords;             // prism fire destination
    int32               DelayBeforeFiring;            // frames before next shot
    TechnoTypeClass*    SecretProduction;              // secret lab bonus type
    DWORD               StorageFilledSlots;            // silo occupancy
    CellStruct          RallyPoint;                    // factory rally point
    AbstractClass*      Target;                        // current target object
    Mission             CurrentMission;               // active mission
    Mission             QueuedMission;                 // mission to switch to

    // Reserved layout buffers for binary compatibility with original engine.
    // These bytes correspond to internal state fields in the original binary
    // (production state, animation timers, targeting, garrison state, etc.)
    // that are managed through the virtual method implementations above.
    BYTE ReservedLayout_538[0x544 - 0x538];
    bool IsTentativelyOccupied;
    bool IsCurrentlyOccupied;
    bool IsStateChanging;
    bool IsBeingSabotaged;

    // Per-instance serialization for the save-game stream (base + derived).
    void Save(class SaveGameClass& saver) const;
    void Load(class LoadGameClass& loader);
    BYTE ReservedLayout[0x70C - 0x548];
};