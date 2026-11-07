#pragma once

#include <Abstract/FootClass.h>

class Surface;
class TargetClass;
class CCINIClass;
class INIClass;
class InfantryTypeClass;
struct CoordStruct;

// ============================================================================
// UnitClass - base for all vehicle units (tanks, harvesters, etc.)
// Inherits FootClass
// ============================================================================
class NOVTABLE UnitClass : public FootClass {
public:
    static const AbstractType AbsID = AbstractType::Unit;
    static DynamicVectorClass<UnitClass*>* Array;

    // ========================================================================
    // IPersistStream
    // ========================================================================
    virtual HRESULT __stdcall Load(IStream* pStm) override;
    virtual HRESULT __stdcall Save(IStream* pStm, BOOL fClearDirty) override;

    // ========================================================================
    // Destructor
    // ========================================================================
    virtual ~UnitClass();

    // ========================================================================
    // TechnoClass overrides
    // ========================================================================
    virtual bool IsVoxel() const override;
    virtual void Destroyed(ObjectClass* Killer) override;
    virtual bool CanScatter() const override;
    virtual int32 GetDefaultSpeed() const override;
    virtual bool HasTurret() const override;
    virtual bool CanDeploySlashUnload() const override;
    virtual bool IsUnitFactory() const override;

    // ========================================================================
    // UnitClass virtuals
    // ========================================================================
    virtual bool CanHarvest() const;
    virtual bool IsHarvesting() const;
    virtual int32 GetTiberiumLoad() const;
    virtual float GetTiberiumValue() const;
    virtual double Get_Tiberium() const override;
    virtual bool IsHarvestingTooMuch() const;

 // 根据游戏行为，可知 GetTiberiumPipFullness 负责下面这段逻辑。
    //
    //  Fraction of the harvester's ore bay that is full, used to size the
    //  collection pip.  Non-harvesters (neither Harvester nor Weeder on the
    //  type) report 0.0; harvesters divide their accumulated ore by the
    //  type's Storage capacity.
    double GetTiberiumPipFullness() const;

 // 根据游戏行为，可知 CanCrush 负责下面这段逻辑。
    //
    //  True when this unit may roll over `pTarget`.  The unit must be able to
    //  crush (the type's Crusher flag, or the CRUSHER veteran/elite ability),
    //  the target must exist and be crushable, and the two must be within
    //  crushing distance.
    bool CanCrush(FootClass* pTarget) const;
    virtual void StartHarvesting();
    virtual void StopHarvesting();
    virtual void HarvestTiberium();
    virtual void EnterTiberiumField();
    virtual void DumpTiberium();

    // ========================================================================
    // UnitClass specific virtuals - driving, harvesting, turret
    // ========================================================================
    virtual void DrawAsVXL(Point2D Coords, RectangleStruct BoundingRect, int32 Brightness, int32 Tint);
    virtual void DrawAsSHP(Point2D Coords, RectangleStruct BoundingRect, int32 Brightness, int32 Tint);
    virtual void DrawObject(Surface* pSurface, Point2D Coords, RectangleStruct CacheRect, int32 Brightness, int32 Tint);
    virtual bool IsDeactivated() const;
    virtual void UpdateTube();
    virtual void UpdateRotation();
    virtual void UpdateEdgeOfWorld();
    virtual void UpdateFiring();
    virtual void UpdateVisceroid();

    // ========================================================================
    // 根据游戏行为，可知 ExitObject 让本载具的第一名乘客下车并回到地图上，
    //  没有乘客时返回空。
    // ========================================================================
    FootClass* ExitObject();

    // ========================================================================
    // 根据游戏行为，可知 Die 是载具死亡的统一入口：先赶光乘客，再走通用
    //  死亡结算。
    // ========================================================================
    virtual void Die(TechnoClass* pKiller) override;

    // ========================================================================
    // 根据游戏行为，可知 AutoCrushSomething 在载具脚下找可碾压的步兵并顺手
    //  压死，返回真表示这一帧碾到了东西。
    // ========================================================================
    bool AutoCrushSomething();

    // ========================================================================
    // 根据游戏行为，可知 VisceroidWander 是病毒体的游荡步进：在矿脉附近随机
    //  换方向挪一格。
    // ========================================================================
    void VisceroidWander();

    // ========================================================================
    // 根据游戏行为，可知 DrawVXL 负责"画车体"、DrawAt 负责"画在哪"：两层
    //  分开，特殊状态的绘制也能复用同一套画体逻辑。
    // ========================================================================
    virtual void DrawVXL(const Point2D& coords, const RectangleStruct& rect);
    virtual void DrawAt(const Point2D& coords, const RectangleStruct& rect);
    virtual void UpdateDisguise();
    virtual void Explode();
    virtual bool GotoClearSpot();
    virtual bool TryToDeploy();
    virtual void Deploy();
    virtual void Undeploy();
    virtual bool Harvesting();
    virtual bool FlagAttach(int32 nHouseIdx);
    virtual bool FlagRemove();
    virtual void APCCloseDoor();
    virtual void APCOpenDoor();
    virtual bool ShouldCrashIt(TechnoClass* pTarget);
    virtual AbstractClass* AssignDestination(AbstractClass* pTarget);
    virtual bool AStarAttempt(const CellStruct& cell1, const CellStruct& cell2);
    bool Find_Path(const CellStruct& start, const CellStruct& dest,
                   DynamicVectorClass<CellStruct>& outPath);
    virtual Action MouseOverCell(CellStruct const* pCell, bool checkFog, bool ignoreForce) const;
    virtual Action MouseOverObject(ObjectClass const* pObject, bool ignoreForce) const;
    virtual void MarkAllOccupationBits(const CoordStruct& coords);
    virtual void UnmarkAllOccupationBits(const CoordStruct& coords);
    virtual void SetTarget(AbstractClass* pTarget);
    virtual AbstractClass* GetTarget() const;
    virtual void ClearTarget();
    virtual bool HasTarget() const;
    // asm: UnitClass_Update (gamemd:1677595 .. 1678543) —— vt+0x5C 覆写。
    //   原版由 vec_Logics 循环 `call [obj+0x5C]` 驱动；内部先跑载具自身
    //   的前置段（隧道/传送/抽能/任务队列门），再 call FootClass::AI
    //   （asm 1678038），最后补载具表现段（伪装刷新等）。
    virtual void Update() override;
    virtual void SetMission(Mission mission);
    virtual Mission GetMission() const;
    virtual void QueueMission(Mission mission);
    virtual void MissionAttack();
    virtual void MissionMove();
    virtual void MissionGuard();
    virtual void MissionSleep();
    virtual void MissionHunt();
    virtual void MissionReturn();
    virtual void MissionStop();
    virtual void MissionHarvest();
    virtual void MissionUnload();
    virtual void MissionEnter();
    virtual void MissionPatrol();
    virtual void MissionAreaGuard();
    virtual void UpdateMission();
    virtual void AI_Update();
    virtual void Combat_AI();
    virtual void Movement_AI();
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
    virtual void Draw(Point2D& point, RectangleStruct& rect);
    virtual void DrawShadow(Point2D& point);
    virtual int32 GetZBias() const;
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
    virtual void Repair(int32 amount);
    virtual void Kill();
    virtual bool CanDeploy() const;
    virtual bool CanEnter() const;
    virtual bool CanBeEntered() const;
    virtual bool CanCrate() const;
    virtual void CreateCrate();
    virtual void PickUpCrate();
    virtual int32 GetValue() const;
    virtual int32 GetCost() const;
    virtual int32 GetBuildTime() const;
    virtual int32 GetSpeed() const;
    virtual int32 GetROF() const;
    virtual DirStruct GetDirection() const;
    virtual void SetDirection(DirStruct dir);
    virtual DirStruct GetTurretDir() const;
    virtual void SetTurretDir(DirStruct dir);
    virtual CoordStruct GetCoords() const;
    virtual void SetCoords(CoordStruct coords);
    virtual CoordStruct GetDestination() const;
    virtual void SetDestination(CoordStruct dest);
    virtual void Stop();
    virtual void Scatter();
    virtual void Hold();
    virtual bool IsMoving() const;
    virtual bool IsFiring() const;
    virtual bool IsIdle() const;
    virtual void SetIdle();
    virtual void Freeze();
    virtual void Unfreeze();
    virtual bool Limbo();
    virtual bool Unlimbo();
    // asm: UnitClass 的 Put 虚槽（vt_TechnoClass.r.m.o.Put 在单位层的覆写）：
    //  贴地后落位。Create_Units 用它放置基地车。
    virtual bool Put(const CoordStruct& coords, int32 a2) override;
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
    virtual bool IsUnit() const;
    virtual AbstractType WhatAmI() const;
    virtual int32 Size() const;

    // ========================================================================
    // Type-flag probes
    // ========================================================================
 // UnitClass_IsARealVehicle: true when the unit type is not
    // flagged IsConsideredVehicle (UnitTypeClass+0xE1B == 0).
    bool IsARealVehicle() const;
 // UnitClass_IsDeployer: the unit type's IsSimpleDeployer
    // byte (UnitTypeClass+0x6AC).
    bool IsDeployable() const;
 // UnitClass_GetCrewCount: thunk to TechnoClass::Crew_Type -
    // the infantry type this unit leaves behind when destroyed.
    InfantryTypeClass* GetCrewCount() const;
 // UnitClass_CalcPipPercentage: the percentage of the
    // unit's pip bar that is filled, used for the passenger/ammo pip display.
    int32 CalcPipPercentage() const;
 // UnitClass_ClearSomeVec: clears the object's attached
    // abstract vector (FootClass vec_Abs).
    void ClearSomeVec();

    // ========================================================================
    // 身份 / 运行时类型 / CRC
    // ========================================================================

    // ------------------------------------------------------------------------
    // asm: int __fastcall UnitClass_GetClassID(UnitClass *this, CLSID *pClassID)
    //  返回本类的 CLSID：指针为空时报参数错误，否则把 UnitClass 的类 GUID
    //  抄进调用方缓冲。
    // ------------------------------------------------------------------------
    HRESULT GetClassID(CLSID* pClassID) override;

    // ------------------------------------------------------------------------
    // asm: int __fastcall UnitClass_GetAbstractDerivationID(UnitClass *this)
    //  抽象派生链上的编号：载具恒为 1。
    // ------------------------------------------------------------------------
    virtual int32 GetAbstractDerivationID() const;

    // ------------------------------------------------------------------------
    // asm: void __thiscall UnitClass_GetCRC(UnitClass *this, CRCEngine *crc)
    //  把本载具参与同步校验的字段逐个喂给 CRC 引擎：类型、炮塔状态、拖挂从车、
    //  载旗、若干布尔标志。
    // ------------------------------------------------------------------------
    void GetCRC(CRCEngine& crc) const;

    // ------------------------------------------------------------------------
    // asm: UnitClass_GetSize
    //  返回本单位实例序列化所需的字节数。
    // ------------------------------------------------------------------------
    int32 GetSize() const;

    // ------------------------------------------------------------------------
    // asm: char __thiscall UnitClass_GetType(UnitClass *this)
    //  返回本对象的运行时类型定义。
    // ------------------------------------------------------------------------
    UnitTypeClass* GetUnitType() const;

    // ------------------------------------------------------------------------
    // asm: wchar_t *__thiscall UnitClass_GetUIName(UnitClass *this)
    //  显示名：伪装状态下优先用伪装身份的名字，多炮塔载具按当前炮塔换成
    //  "Tip:Rocket"/"Tip:MachineGun"/"Tip:Repair" 之类的提示名，其余用类型
    //  自带的 UI 名。
    // ------------------------------------------------------------------------
    virtual const wchar_t* GetUIName() const;

    // ========================================================================
    // 伪装
    // ========================================================================

    // ------------------------------------------------------------------------
    // asm: void __thiscall UnitClass_DisguiseAs(_TechnoClass *this, _TechnoClass *tgt)
    //  把本单位伪装成目标对象：按目标类别记下伪装用的类型与所属，并让外观
    //  刷新；空目标表示解除伪装。
    // ------------------------------------------------------------------------
    virtual void DisguiseAs(AbstractClass* pTarget);

    // ------------------------------------------------------------------------
    // asm: void __fastcall UnitClass_ClearDisguise(UnitClass *this)
    //  解除伪装：清掉伪装标志、伪装类型与伪装所属，并通知外观刷新。
    // ------------------------------------------------------------------------
    virtual void ClearDisguise() override;

    // ------------------------------------------------------------------------
    // asm: char __thiscall UnitClass_IsDisguisedAs(UnitClass *this, HouseClass *h)
    //  当本单位处于伪装状态、且没有被指定阵营的探测手段看穿时，报告"对
    //  h 而言显示为伪装身份"。
    // ------------------------------------------------------------------------
    virtual bool IsDisguisedAs(int32 a2) const override;

    // ------------------------------------------------------------------------
    // asm: _TechnoTypeClass *__thiscall UnitClass_GetDisguise(UnitClass *this, char a2)
    //  返回伪装用的类型：自己是玩家盟友且 a2 非零时直接返回真实类型，否则
    //  返回伪装类型。
    // ------------------------------------------------------------------------
    virtual TechnoTypeClass* GetDisguise(int32 a2) const;

    // ------------------------------------------------------------------------
    // asm: HouseClass *__thiscall UnitClass_GetDisguiseHouse(UnitClass *this, char a2)
    //  返回伪装身份的所属阵营：自己是玩家盟友且 a2 非零时返回真实阵营，否则
    //  返回伪装阵营。
    // ------------------------------------------------------------------------
    virtual HouseClass* GetDisguiseHouse(int32 a2) const;

    // ========================================================================
    // 部署 / 面向
    // ========================================================================

    // ------------------------------------------------------------------------
    // asm: char __thiscall UnitClass_IsDeployed(UnitClass *this)
    //  本单位是否处于展开状态。
    // ------------------------------------------------------------------------
    bool IsDeployed() const;

    // ------------------------------------------------------------------------
    // asm: int __thiscall UnitClass_GetFacing(UnitClass *this, int *a2)
    //  计算朝向目标点 a2 的 256 分度朝向写回 a2：多炮塔载具用炮塔朝向起算，
    //  其余用车身朝向。
    // ------------------------------------------------------------------------
    void GetFacing(CoordStruct* pCoord);

    // ========================================================================
    // 点击 / 指令 / 自动索敌
    // ========================================================================

    // ------------------------------------------------------------------------
    // asm: char __thiscall UnitClass_ClickedMission(UnitClass *this, eActions act, _ObjectClass *target, int a4)
    //  玩家点击本单位时把光标动作归一到任务动作，再交给基类处理；已经展开
    //  或正在忙的单位不接受新指令。
    // ------------------------------------------------------------------------
    bool ClickedMission(int32 act, ObjectClass* pTarget, int32 a4) const;

    // ------------------------------------------------------------------------
    // asm: int __thiscall UnitClass_ReceivedRadioCommand(UnitClass *this, int val, uint cmd, int arg)
    //  处理无线电报文：少数与载具状态相关的报文就地处理，其余转交基类。
    // ------------------------------------------------------------------------
    int32 ReceivedRadioCommand(int32 val, int32 cmd, int32 arg);

    // ------------------------------------------------------------------------
    // asm: _ObjectClass *__thiscall UnitClass_SelectAutoTarget(UnitClass *this, int TargetFlags, int a5, int a6)
    //  自动索敌：把本单位武器的反地/反空标志折进 TargetFlags 后交给通用索敌。
    // ------------------------------------------------------------------------
    virtual ObjectClass* SelectAutoTarget(int32 projFlags, int32 curThreat, int32 a4);

    // ------------------------------------------------------------------------
    // asm: signed int __thiscall UnitClass_GetObjectActivityState(UnitClass *this, _TechnoClass *target, int idx, int what)
    //  报告本单位"当前正在干什么"，供选中信息与 AI 判断：正在装填/开火/移动
    //  等各返回对应状态码。
    // ------------------------------------------------------------------------
    int32 GetObjectActivityState() const;

    // ========================================================================
    // 销毁登记 / 指针失效
    // ========================================================================

    // ------------------------------------------------------------------------
    // asm: void __thiscall UnitClass_RegisterDestruction(UnitClass *this, _TechnoClass *killer)
    //  登记本载具被摧毁：先按触发器规则抛出若干事件，再走通用的摧毁登记。
    // ------------------------------------------------------------------------
    void RegisterDestruction(TechnoClass* pKiller);
    using TechnoClass::RegisterDestruction;

    // ------------------------------------------------------------------------
    // asm: void __thiscall UnitClass_PointerGotInvalid(UnitClass *this, _AbstractClass *invalid, char a3)
    //  某个被引用的对象失效时清理本载具持有的悬挂指针（拖挂从车、伪装目标等）。
    // ------------------------------------------------------------------------
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;

    // ========================================================================
    // 地图 INI 存取（场景存档里的 [Units] 段）
    // ========================================================================

    // ------------------------------------------------------------------------
    // asm: void __thiscall UnitClass_CreateFromINIList(CCINIClass *pINI)
    //  读 [Units] 段逐行重建载具：每行给出拥有者、类型、坐标、朝向、生命值
    //  与任务，按类型与拥有者实例化后放到地图上。
    // ------------------------------------------------------------------------
    static void CreateFromINIList(CCINIClass* pINI);

    // ------------------------------------------------------------------------
    // asm: int __thiscall UnitClass_SaveToMapINIList(CCINIClass *INI)
    //  把全地图上所有已放置的载具回写进 [Units] 段，每台一行。
    // ------------------------------------------------------------------------
    static void SaveToMapINIList(INIClass* pINI);

    // ------------------------------------------------------------------------
    // asm: void __thiscall UnitClass_SaveToMapINI(UnitClass *this, INIClass *INI)
    //  回写单台载具的放置记录。
    // ------------------------------------------------------------------------
    void SaveToMapINI(INIClass* pINI) const;

    // ========================================================================
    // Constructor
    // ========================================================================
    UnitClass(HouseClass* pOwner) noexcept;

protected:
    explicit __forceinline UnitClass(noinit_t) noexcept : FootClass(noinit) {}

    // ========================================================================
    // Properties
    // ========================================================================
public:
    UnitTypeClass* Type;
    DirStruct TurretDir;
    DirStruct BarrelDir;
    int32 TurretRotation;
    int32 BarrelRotation;
    bool IsHarvestingTiberium;
    bool IsDumpingTiberium;
    bool IsShowingCrate;
    bool IsMCV;
    bool IsHarvester;
    bool IsAPC;
    bool IsDeployer;
    bool IsChrono;
    bool IsBombing;
    bool IsUnderground;
    bool IsSubterranean;
    bool IsButtMissile;
    bool IsSpace;
    bool IsTank;
    bool IsWalker;
    bool IsAntiAir;
    bool IsOpenTopped;
    bool IsCarryall;
    bool IsCarryAllFly;
    bool IsJumpJet;
    BYTE align_7D0[3];
    int32 HarvestAmount;
    int32 HarvestRate;
    int32 TotalTiberiumValue;
    int32 FlagHouseIndex;
    bool Deployed;
    bool Deploying;
    bool Undeploying;
    bool Unloading;
    int32 DeathFrameCounter;
    int32 NonPassengerCount;
    bool HasFollowerCar;

    // 根据游戏行为，可知 Gunner 是当前坐在本单位炮手位上的那名步兵；
    //  GunnerWeaponIndex 记录炮手兵种对应的武器档位（-1 表示尚未确定）。
    //  炮手下车时两者一并清空。
    InfantryClass* Gunner;
    int32          GunnerWeaponIndex;

    // ── Carried flag identity (+0x6CC) ──────────────────────────────────────
    // House index of the rally/capture flag this unit currently carries, or -1
    // when it carries none.  UnitClass_DropFlag/PickUpFlag own this slot, and
    // both re-broadcast ownership through the object's vtable slot (+0x124).
    int32 CarriesFlagOfHouse;

    // ── Disguise identity ───────────────────────────────────────────────────
    // 伪装身份：开启伪装后记下伪装成的类型与所属阵营，ClearDisguise 清空。
    //  DisguiseActive 与 IsDisguised() 的标志位保持同步。
    bool             DisguiseActive;
    TechnoTypeClass* DisguiseType;
    HouseClass*      DisguiseHouse;

    // GetUIName 组装 "<载具名> <炮塔提示>" 时的暂存缓冲。
    mutable wchar_t  m_UINameCache[128];

    bool IsBusy() const;
    bool DropFlag();
    bool PickUpFlag(int32 houseIndex);

    // ========================================================================
    // 逐帧入口 / 开火 / 任务处理器 / 采矿扩展 / 载员 / 炮塔
    //
    //  下面这一组是 asm 里真正挂在 UnitClass 虚表上的槽位：
    //    Update        -> vt+0x5C  (asm:277 UnitClass_Update)
    //    Fire          -> vt+0x1E4 (asm:1697189 UnitClass_Fire)
    //    Mission_Move  -> vt+0x1E8 (asm:8261)
    //    Mission_Guard -> asm:8033
    //    Mission_Unload-> asm:1690071
    //  其余 Mission_* 是同一张任务跳表上的载具特化分支。采矿与载员部分
    //  是 UnitClass 独有的数据面（矿石舱、乘客链、炮塔）。语义对齐 asm，
    //  不要求逐字节一致。
    // ========================================================================

    // ------------------------------------------------------------------------
    // asm: void __thiscall UnitClass_Update(UnitClass *this)   (asm:277)
    //  逐帧入口：失能冒烟 → 抽能失效 → 时间武器 → 部署火花 → 死亡计数 →
    //  AI 自动展开 → FootClass::AI → 伪装刷新 → 开火帧 → 维修厂回厂。
    //  声明见本类前面的 vt+0x5C 覆写块（重复声明会编译失败）。
    // ------------------------------------------------------------------------

    // ------------------------------------------------------------------------
    // asm: BulletClass *__thiscall UnitClass_Fire(UnitClass *this,
    //          AbstractClass *target, int idxWeapon)   (asm:1697189, retn 8)
    //  开火入口：先取出武器槽，按连发序号决定是否沿用当前开火帧；判定
    //  "够得着"之后转交 TechnoClass::Fire，再处理装填、开火帧与伪装闪烁。
    // ------------------------------------------------------------------------
    virtual BulletClass* Fire(AbstractClass* pTarget, int32 nWeaponIndex) override;

    // ------------------------------------------------------------------------
    // asm: signed int __thiscall UnitClass::Mission_Move(UnitClass *this)   (asm:8261)
    //  载具移动：展开/收起中不接受移动，改为排队回警戒；否则关好舱门再走。
    // ------------------------------------------------------------------------
    virtual int32 Mission_Move() override;

    // ------------------------------------------------------------------------
    // asm: int __thiscall UnitClass::Mission_Guard(UnitClass *this)   (asm:8033)
    //  警戒：奴隶矿车唤醒、矿车自动回矿、传送单位回炼油厂、自动展开；
    //  都不成立时退回 FootClass::Mission_Guard。
    // ------------------------------------------------------------------------
    virtual int32 Mission_Guard() override;

    // ------------------------------------------------------------------------
    // asm: int __thiscall UnitClass_Hunt(UnitClass *this)   (asm: 见
    //      UnitClass::Goto_Clear_Spot+7B 的调用点)
    //  猎杀：没有目标时先占一个空位再交给基类猎杀。
    // ------------------------------------------------------------------------
    virtual int32 Mission_Hunt() override;

    // ------------------------------------------------------------------------
    // asm: int __thiscall UnitClass_Mission_Return(UnitClass *this)
    //  回厂：矿车带着矿石回炼油厂，空车回矿脉，其余退回基地。
    // ------------------------------------------------------------------------
    virtual int32 Mission_Return() override;

    // ------------------------------------------------------------------------
    // asm: int __thiscall UnitClass_Mission_Enter(UnitClass *this)
    //  进入：向建筑/运输载具申请停靠，靠近后交给基类。
    // ------------------------------------------------------------------------
    virtual int32 Mission_Enter() override;

    // ------------------------------------------------------------------------
    // asm: int __thiscall UnitClass_Unload(UnitClass *this)   (asm:1690071)
    //  卸载：矿车在炼油厂卸矿记钱；运兵车开门放人；简易展开单位收起。
    // ------------------------------------------------------------------------
    virtual int32 Mission_Unload() override;

    // ------------------------------------------------------------------------
    // asm: int __thiscall UnitClass_Mission_Capture(UnitClass *this)
    //  占领：载具本身不会占领，交给基类（用于工程师车一类特例）。
    // ------------------------------------------------------------------------
    virtual int32 Mission_Capture() override;

    // ------------------------------------------------------------------------
    // asm: char __thiscall UnitClass_Harvest(UnitClass *this)   (asm:6482)
    //  采矿一步：确认脚下有矿 → 扣一格矿量 → 累进矿石舱；满了改去卸矿。
    // ------------------------------------------------------------------------
    bool Harvest();

    // ------------------------------------------------------------------------
    // asm: char __thiscall UnitClass::Find_Ore(UnitClass *this, CellStruct *out)
    //  在当前位置周围螺旋搜索最近的含矿格，找到写回 out 并报告成功。
    // ------------------------------------------------------------------------
    bool Find_Ore(CellStruct& outCell);

    // ------------------------------------------------------------------------
    // asm: int __thiscall UnitClass_GetOreValue(UnitClass *this, CellStruct *cell)
    //  取一格矿石的单价（每单位矿石折算的信用点）。
    // ------------------------------------------------------------------------
    int32 Get_Ore_Value(const CellStruct& cell) const;

    // ------------------------------------------------------------------------
    // asm: char __thiscall UnitClass_Can_Enter(UnitClass *this, FootClass *p)
    //  运兵车能否再收一名乘客：必须开着门、未满员、且不是自己人重复上车。
    // ------------------------------------------------------------------------
    bool Can_Enter(FootClass* pPassenger) const;

    // ------------------------------------------------------------------------
    // asm: char __thiscall UnitClass_On_Enter(UnitClass *this, FootClass *p)
    //  乘客上车：挂进乘客链、同步 NonPassengerCount，并让乘客进入 limbo。
    // ------------------------------------------------------------------------
    bool On_Enter(FootClass* pPassenger);

    // ------------------------------------------------------------------------
    // asm: char __thiscall UnitClass_On_Exit(UnitClass *this, FootClass *p)
    //  乘客下车：从链上摘下、放到车旁空地并解除运载关系。
    // ------------------------------------------------------------------------
    bool On_Exit(FootClass* pPassenger);

    // ------------------------------------------------------------------------
    // asm: void __thiscall UnitClass_Kill_Passengers(UnitClass *this,
    //          TechnoClass *killer)
    //  车毁人亡：乘客全部按同一凶手结算死亡，而不是被放出来。
    // ------------------------------------------------------------------------
    void Kill_Passengers(TechnoClass* pKiller);

    // ------------------------------------------------------------------------
    // asm: void __thiscall UnitClass_UpdateTurret(UnitClass *this,
    //          AbstractClass *target)
    //  炮塔逐帧：按目标方向求期望朝向，再以 ROT 限速朝它转。
    // ------------------------------------------------------------------------
    void Update_Turret(AbstractClass* pTarget);

    // ------------------------------------------------------------------------
    // asm: void __thiscall UnitClass_Crash(UnitClass *this)
    //  坠毁：空中载具失去升力后逐帧下沉，落地即按凶手为空登记摧毁。
    // ------------------------------------------------------------------------
    void Crash();

    // ========================================================================
    // 炮手 / 载员管理
    //
    //  多功能车、战斗要塞一类载具可以现场接收一名步兵充当"炮手"，炮手的
    //  兵种决定载具当前使用哪一档武器。下面这组接口负责炮手的上下车与
    //  切换，以及把首位乘客卸下。
    // ========================================================================

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ReceiveGunner 负责接收一名步兵上车充当炮手：先
    //  检查该步兵确实能担任炮手，再把它挂到本单位的炮手位上，并按兵种切换
    //  当前武器档位。
    // ------------------------------------------------------------------------
    bool ReceiveGunner(InfantryClass* pGunner);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 RemoveGunner 负责让当前炮手下车：清掉炮手位，让
    //  步兵重新回到地图上，并把武器档位复位到默认。
    // ------------------------------------------------------------------------
    bool RemoveGunner(InfantryClass* pGunner);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 SwitchGunner 负责按给定档位切换炮手武器：档位在
    //  合法范围内就切过去，越界则忽略。
    // ------------------------------------------------------------------------
    bool SwitchGunner(int32 idx);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 RemoveFirstPassenger 负责把首位乘客卸下：从乘客
    //  链上摘下第一个，让它回到地图上；若本载具的武器要靠乘客提供、而卸完
    //  之后已经没人了，就把武器档位一并复位。
    // ------------------------------------------------------------------------
    void RemoveFirstPassenger();

    // ========================================================================
    // 变形 / 展开
    // ========================================================================

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ToggleDeployState 负责切换本单位的"变形"状态：
    //  只有类型上允许变形、且当前确实停稳时才会进行；展开中的再点一次就
    //  收回、收起中的再点一次就展开。
    // ------------------------------------------------------------------------
    void ToggleDeployState();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ToggleSimpleDeploy 负责切换"简易展开"（例如采矿车
    //  就地卸矿这类不改变外形的展开）：按当前是展开还是收起状态，走对应的
    //  展开/收起流程。
    // ------------------------------------------------------------------------
    void ToggleSimpleDeploy();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ChangeOwnership 负责把本单位转交给新的拥有者：
    //  随车拖挂的从车一并转手；若本单位载着乘客，乘客也随之易主；最后更新
    //  自己的归属并让界面重绘。
    // ------------------------------------------------------------------------
    void ChangeOwnership(HouseClass* pNewOwner, bool a3);

    // Per-instance serialization for the save-game stream (base + derived).
    void Save(class SaveGameClass& saver) const;
    void Load(class LoadGameClass& loader);
    UnitClass* FollowerCar;
    DWORD unknown_7E0;
    DWORD unknown_7E4;
    DWORD unknown_7E8;
    DWORD unknown_7EC;
    DWORD unknown_7F0;
    DWORD unknown_7F4;
    DWORD unknown_7F8;
    DWORD unknown_7FC;
    DWORD unknown_800;
    DWORD unknown_804;
    DWORD unknown_808;
    DWORD unknown_80C;
    DWORD unknown_810;
    DWORD unknown_814;
    DWORD unknown_818;
    DWORD unknown_81C;
    DWORD unknown_820;
    DWORD unknown_824;
    DWORD unknown_828;
    DWORD unknown_82C;
    DWORD unknown_830;
    DWORD unknown_834;
    DWORD unknown_838;
    DWORD unknown_83C;
    DWORD unknown_840;
    DWORD unknown_844;
    DWORD unknown_848;
    DWORD unknown_84C;
    DWORD unknown_850;
    DWORD unknown_854;
    DWORD unknown_858;
    DWORD unknown_85C;
    DWORD unknown_860;
    DWORD unknown_864;
    DWORD unknown_868;
    DWORD unknown_86C;
    DWORD unknown_870;
    DWORD unknown_874;
    DWORD unknown_878;
    DWORD unknown_87C;
    DWORD unknown_880;
    DWORD unknown_884;
    DWORD unknown_888;
    DWORD unknown_88C;
    DWORD unknown_890;
    DWORD unknown_894;
    DWORD unknown_898;
    DWORD unknown_89C;
    DWORD unknown_8A0;
    DWORD unknown_8A4;
    DWORD unknown_8A8;
    DWORD unknown_8AC;
    DWORD unknown_8B0;
    DWORD unknown_8B4;
    DWORD unknown_8B8;
    DWORD unknown_8BC;
    DWORD unknown_8C0;
    DWORD unknown_8C4;
    DWORD unknown_8C8;
    DWORD unknown_8CC;
    DWORD unknown_8D0;
    DWORD unknown_8D4;
    DWORD unknown_8D8;
    DWORD unknown_8DC;
    DWORD unknown_8E0;
    DWORD unknown_8E4;
    DWORD unknown_8E8;
    DWORD unknown_8EC;

    // 最近一次本体绘制的屏幕包围盒（SHP 帧 dst / 体素投影包围盒）。
    // 原版是 Draw_It 的局部量，这里提升为成员供 Draw() 里的
    // 选中框与血条定位（DrawAsSHP 与 CC_Draw_Shape 使用同一套锚点换算）。
    RectangleStruct LastRenderRect;
};