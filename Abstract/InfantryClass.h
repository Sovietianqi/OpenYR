#pragma once

#include <Abstract/FootClass.h>
#include <Math/Timer.h>

class INIClass;

// ============================================================================
// InfantryClass - base for all infantry units
// Inherits FootClass
// ============================================================================
class NOVTABLE InfantryClass : public FootClass {
public:
    static const AbstractType AbsID = AbstractType::Infantry;
    static DynamicVectorClass<InfantryClass*>* Array;

    // ========================================================================
    // IPersistStream
    // ========================================================================
    virtual HRESULT __stdcall Load(IStream* pStm) override;
    virtual HRESULT __stdcall Save(IStream* pStm, BOOL fClearDirty) override;

    // ========================================================================
    // Destructor
    // ========================================================================
    virtual ~InfantryClass();

    // ========================================================================
    // TechnoClass overrides
    // ========================================================================
    virtual bool IsVoxel() const override;
    virtual void Destroyed(ObjectClass* Killer) override;
    virtual bool CanScatter() const override;
    virtual int32 GetDefaultSpeed() const override;
    virtual bool IsEngineer() const override;
    virtual bool CanDeploySlashUnload() const override;
    virtual bool IsCloseEnough(AbstractClass* pTarget, int32 idxWeapon) const override;
    virtual bool IsCloseEnoughToAttack(AbstractClass* pTarget) const override;
    virtual void TakeDamage(int32 damage, ObjectClass* source, WarheadTypeClass* warhead);

    // ========================================================================
    // InfantryClass virtuals
    // ========================================================================
    virtual bool CanBeEngineer() const;
    virtual bool IsCrawling() const;
    virtual bool IsProne() const;
    virtual bool IsDeployed() const;
    virtual void StartCrawling();
    virtual void StopCrawling();
    virtual void GoProne();
    virtual void StandUp();
    virtual void Panic() override;
    virtual void Scatter(const CoordStruct& crd, bool ignoreMission, bool ignoreDestination) override;
    virtual void UpdateIdleAction() override;
    virtual void PerCellProcess();
    virtual void FireDeathWeapon(int32 additionalDamage);
    virtual void GetFiringCoords();
    virtual void GetFiringCoordsFromBomb();
    virtual int32 GetFiringSync();

    // ========================================================================
    // InfantryClass specific virtuals
    // ========================================================================
    virtual bool CanFireNow() const;
    virtual bool CanEnterCell(CellClass* pCell) const;
    virtual void EnteredCell();
    virtual void ExitCell();
    virtual void Prone();
    virtual void Unprone();
    virtual void Crawl();
    virtual void StopCrawl();
    virtual void Deploy();
    virtual void Undeploy();
    virtual bool CanDeploy() const;
    virtual void UnPanic();
    virtual bool IsPanicked() const;
    virtual void Berzerk();
    virtual void UnBerzerk();
    virtual bool IsBerzerk() const;
 // InfantryClass_GoBerzerk: sets the berzerk flag directly.
    void GoBerzerk();
 // InfantryClass_IsDeployer: the infantry type's Deployer
    // byte (InfantryTypeClass+0xEC8).
    bool IsDeployer() const;
    virtual void Stun();
    virtual void UnStun();
    virtual bool IsStunned() const;
    virtual void Sleep();
    virtual void Wake();
    virtual bool IsSleeping() const;
    virtual void SetTarget(AbstractClass* pTarget);
    virtual AbstractClass* GetTarget() const;
    virtual void ClearTarget();
    virtual bool HasTarget() const;
    // asm: InfantryClass_Update (gamemd:623746 .. 624348) —— vt+0x5C 覆写。
    //   前置段：隧道步进 / 传送中清目标 / 任务队列门；随后 call
    //   FootClass::AI（asm 624000）；最后跑步兵专属的
    //   Firing_AI / Doing_AI / Movement_AI（624322/624329/624336）。
    virtual void Update() override;
    virtual void SetMission(Mission mission);
    virtual Mission GetMission() const;
    virtual bool DoAction(Action action, AbstractClass* pTarget, CellStruct* pCell);
    virtual void QueueMission(Mission mission);
    virtual Mission GetQueuedMission() const;
    virtual void MissionAttack();
    virtual void MissionMove();
    virtual void MissionGuard();
    virtual void MissionSleep();
    virtual void MissionHunt();
    virtual void MissionReturn();
    virtual void MissionStop();
    virtual void MissionHarvest();
    virtual void MissionCapture();
    virtual void MissionEnter();
    virtual void MissionUnload();
    virtual void MissionPatrol();
    virtual void MissionAreaGuard();
    virtual void MissionParaDrop();

    // ========================================================================
    // 开火入口 / 任务处理器覆写（asm: InfantryClass_Fire 6196 / _Attack 7019 /
    //   _Guard 7266 / _Hunt 7165 / _Move 7330 / _Unload 7405）
    //
    //  步兵的任务跳表与载具完全不同：展开/收起姿态会改变任务的走向，
    //  工程师/C4 兵种会把 Attack 改写成占领/爆破。这些覆写把 asm 的分支
    //  按原样落到本项目的姿态字段上。
    // ========================================================================
    virtual BulletClass* Fire(AbstractClass* pTarget, int32 nWeaponIndex) override;
    virtual int32 Mission_Attack() override;
    virtual int32 Mission_Move() override;
    virtual int32 Mission_Guard() override;
    virtual int32 Mission_Hunt() override;
    virtual int32 Mission_Return() override;
    virtual int32 Mission_Enter() override;
    virtual int32 Mission_Unload() override;
    virtual int32 Mission_Capture() override;

    // ------------------------------------------------------------------------
    // asm: char __thiscall InfantryClass_GuardBuildingCheck
    //  警戒/区域警戒时检查脚下建筑：可驻守的民房/可穿越的闸门会让步兵
    //  贴上去（asm 624010 段的 Cell_Building + CanBeOccupied 分支）。
    // ------------------------------------------------------------------------
    bool TryEnterBuilding();

    // ------------------------------------------------------------------------
    // asm: InfantryClass_Update 的 624240..624322 段
    //  姿态收尾：清掉已播完的"忙"序列标记、推进趴下计时器、按类型决定
    //  是回到站姿还是保持伏地。InfantryClass::Update 在 FootClass::Update
    //  之后调用它。
    // ------------------------------------------------------------------------
    void Update_Posture();
    virtual void UpdateMission();
    virtual void AI_Update();
    virtual void Combat_AI();
    virtual void Movement_AI();
    virtual void Fire_At(AbstractClass* pTarget, int32 weaponIndex);
    virtual bool Can_Fire_At(TechnoClass* pTarget, int32 weaponIndex) const;
    virtual int32 GetWeaponRange(int32 weaponIndex) const;
    virtual int32 GetWeaponDamage(int32 weaponIndex) const;
    virtual CoordStruct GetFireCoords(int32 weaponIndex) const;
    virtual void MuzzleFlash(int32 weaponIndex);
    virtual void OnFired(int32 weaponIndex);
    virtual int32 GetWeaponCount() const;
    virtual void TakeDamage(int32 damage, TechnoClass* pSource, WarheadTypeClass* pWarhead);
    virtual void OnDestroyed();
    virtual void OnCaptured(HouseClass* pNewOwner);
    virtual void OnVeterancyUp();
    virtual void Draw(Point2D& point, RectangleStruct& rect);
    virtual void DrawSHP(Point2D& point, RectangleStruct& rect, int32 brightness, int32 tint);
    virtual void DrawShadow(Point2D& point);

    // ------------------------------------------------------------------------
    // asm: int __thiscall sub_518D80(InfantryClass *this)   —— 步兵 SHP 帧号
    //
    //   帧号 = 序列.StartFrame
    //        + (AnimationFrame % max(序列.FrameCount, 1))
    //        + 方向序号 * 序列.FramesPerDirection      （仅当 FramesPerDirection > 0）
    //
    //   方向序号不是直接拿 8 方向枚举，而是把 16 位朝向切成 32 个扇区后查
    //   原版 .data:dword_7EAEFC（32 项 → 0..7）。该表把"顺时针增大的朝向"
    //   翻成"逆时针增大的 SHP 帧序"：0 = NW … 7 = N。
    // ------------------------------------------------------------------------
    int32 Get_Frame() const;
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
    virtual bool CanDeployNow() const;
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
    virtual void SetCloak(bool on);
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
    virtual bool CanChronoShift() const;
    virtual void TemporalWarp();
    virtual void UnTemporal();
    virtual bool IsTemporalWarped() const;
    virtual void MindControl(TechnoClass* pTarget);
    virtual void UnMindControl();
    virtual bool IsMindControlled() const;
    virtual void Parasite(TechnoClass* pHost);
    virtual void UnParasite();
    virtual bool IsParasited() const;
    virtual void Disguise();
    virtual void UnDisguise();
    virtual bool IsDisguised() const;
    virtual bool CanCaptureBuilding() const;
    virtual bool CanInfiltrate() const;
    virtual void Infiltrate(BuildingClass* pBuilding);
    virtual void CaptureBuilding(BuildingClass* pBuilding);
    virtual void Detach_Target();
    virtual void Attach_Target(AbstractClass* pTarget);
    virtual void PlayAnim(Sequence index, bool force, bool randomStartFrame);

    // ── Sequence classification (asm InfantryClass_Is* family) ────────────
    // The three predicates below classify the current sequence index.  They
    // are expressed numerically because the binary's ordinal layout is the
    // authoritative source and the project's Sequence enum orders the tail
    // entries differently.

 // InfantryClass_IsAutoUndeployer: true while a follow-up
    //  sequence is queued (index != -1).
    bool IsAutoUndeployer() const;

 // InfantryClass_IsDeployed_Anim (XX): the deployed set - indices
    //  0x1B..0x1E (DeployedFire, DeployedIdle, Undeploy, Paradrop).
    bool IsDeployed_Anim() const;

 // InfantryClass_IsSpecialSequence: sequences that suspend
    //  normal activity - the die/swim/flight and the harvest/count set:
    //  0x0B..0x0F, 0x14, 0x15 and 0x22..0x24.
    bool IsSpecialSequence() const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 UnsetSequenct 负责清掉"正在循环播放的行走/趴下类
    //  序列"：当当前序列正好是趴下射击、行走或行走(变体)这三种之一时，把
    //  记录序列的字段复位成 -1（表示无），其余序列不受影响。
    // ------------------------------------------------------------------------
    void UnsetSequenct();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 StopMovingAction 负责立刻中止步兵的移动动作：
    //  先确认运动控制器存在（不存在则报错），通知控制器停止，把当前序列
    //  切到"待机"、清掉"正在移动"标记与运动状态，最后退出当前脚点。
    // ------------------------------------------------------------------------
    void StopMovingAction();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 GarrisonBuilding 负责让步兵进入建筑驻守：先用自身
    //  的"可驻守"检查决定走哪条分支——可以驻守时把本步兵加入建筑的驻守名单、
    //  刷新该建筑所在格的威胁值，并在这是第一个驻守者且属于玩家时播放提示语
    //  与音效；属于"可被清除"分支时则反过来清除该建筑的驻守者并把本步兵清场。
    // ------------------------------------------------------------------------
    bool GarrisonBuilding(BuildingClass* pBuilding);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 SetDefaultDisguise 负责给间谍之类单位设置一个默认
    //  伪装：若本类型允许默认伪装，则置上伪装标记、按拥有的房屋派生出伪装
    //  归属，并根据房屋的类别(0/1/2)从规则里挑一款默认伪装外观；不允许时
    //  清掉伪装标记。
    // ------------------------------------------------------------------------
    void SetDefaultDisguise();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 DisguiseAs 负责把本步兵伪装成目标：目标必须是一个
    //  可以伪装成的对象（敌方步兵/载具之类），随后记下伪装身份并让外观
    //  显示成对方的模样。传入空指针表示解除伪装。
    // ------------------------------------------------------------------------
    virtual void DisguiseAs(AbstractClass* pTarget);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 SpawnParachuting 负责让本步兵以"空降"的方式落到
    //  指定位置：先做常规的投放处理，成功后按拥有者是否为玩家决定是否播放
    //  落地动作。
    // ------------------------------------------------------------------------
    bool SpawnParachuting(int32 a3);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 UpdateDeployment 负责推进"展开/坐下"这类变形动画：
    //  只在当前动作序列属于变形序列时才推进；序列播完就把状态定形。
    //  asm 返回 -1 表示"未在展开动画中"，其余返回动画剩余帧数，
    //  Guard/AreaGuard 据此决定是否跳过基类处理。
    // ------------------------------------------------------------------------
    int32 UpdateDeployment();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 UpdateInTunnel 负责推进单位在隧道中的移动：沿
    //  隧道走向按速度推进，到达出口就把单位放出到地面，途中与其它隧道内
    //  单位保持间距。
    // ------------------------------------------------------------------------
    void UpdateInTunnel(int32 a1);

    // ========================================================================
    // 类型 / 反射信息（asm: InfantryClass_GetType / _GetImage / _GetUIName /
    //  _GetAbstractDerivationID / _GetSize / _GetCRC）
    //
    //  GetType 直接返回步兵类型；GetImage 是 FootClass::GetImage 的转发；
    //  GetUIName 在"技工"标记置位时返回 TXT_TECHNICIAN，否则按敌我关系返回
    //  伪装名或自身类型名；GetAbstractDerivationID 返回 AbstractType::Infantry；
    //  Size 返回原版 InfantryClass 结构体字节数 0x6F0；ComputeCRC 把步兵特有的
    //  序列与状态字节并入校验。
    // ========================================================================
    virtual TechnoTypeClass* GetType() const override;
    virtual SHPStruct* GetImage() const override;
    virtual const wchar_t* GetUIName() const override;
    virtual AbstractType GetAbstractDerivationID() const;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // ========================================================================
    // 速度 / 活动状态（asm: InfantryClass_GetCurrentSpeed /
    //  _GetObjectActivityState）
    //
    //  GetCurrentSpeed 在基类速度之上按"爬行/趴下"状态打折；
    //  GetObjectActivityState 对处于特殊序列（死亡/游泳/飞翔）的步兵统一
    //  报告活动状态 6，其余情况汇报"能否开火/能否接近"。
    // ========================================================================
    virtual int32 GetCurrentSpeed() const;
    int32 GetObjectActivityState(AbstractClass* pTarget, int32 idxWeapon,
                                 int32 ignoreCloseEnough);
    // ========================================================================
    // 开火位置（asm: InfantryClass_GetFLH）
    //
    //  步兵搭乘载具且属于该载具乘客时，改由载具的 FLH 提供炮口位置；
    //  其余情况沿用基类的炮口位置计算。
    // ========================================================================
    CoordStruct GetFLH(int32 idxWeapon, bool muzzle);

    // ========================================================================
    // 伪装查询（asm: InfantryClass_GetDisguise / _GetDisguiseHouse /
    //  _IsDisguisedAs）
    //
    //  GetDisguise / GetDisguiseHouse 在目标方与己方同盟且查询方要求"看真身"
    //  时返回己方信息，否则返回对方的伪装身份；IsDisguisedAs 判断目标对象是否
    //  与本步兵的伪装身份一致。
    // ========================================================================
    virtual int32 GetDisguise(int32 a2) const;
    virtual HouseClass* GetDisguiseHouse(int32 a2) const;
    virtual bool IsDisguisedAs(int32 a2) const override;

    // ========================================================================
    // 空闲动作（asm: InfantryClass_IdleAction / _PlayIdleAnim /
    //  _Is_Ready_To_Random_Animate）
    //
    //  IdleAction 在空闲计时到达后按规则频率重置计时器，并处理 Fraidycat
    //  的自动散开与"牛"类步兵的特殊空闲；PlayIdleAnim 轮流播放两套空闲
    //  动画；Is_Ready_To_Random_Animate 汇总各"正在忙"标记决定能否随机动画。
    // ========================================================================
    virtual bool IdleAction() override;
    virtual void PlayIdleAnim(int32 a2) override;
    virtual bool Is_Ready_To_Random_Animate() const;

    // ========================================================================
    // 光标 / 点击（asm: InfantryClass_ClickedMission /
    //  _GetCursorOverCell / _PointerGotInvalid）
    //
    //  ClickedMission 把被点击对象映射为步兵可执行的任务码；
    //  GetCursorOverCell 只对己方玩家给出单元格光标；
    //  PointerGotInvalid 在类型指针失效时把自身类型清空。
    // ========================================================================
    virtual bool ClickedMission(int32 act, ObjectClass* pTarget, int32 a4) const;
    virtual int32 GetCursorOverCell(const CellStruct& cell, bool a3, bool a4) const;
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;

    // ========================================================================
    // 放置 / 移动（asm: InfantryClass_Put / _MoveTo）
    //
    //  Put 把步兵放到地图上的指定坐标（贴合地面高度并登记所在格）；
    //  MoveTo 朝目标坐标规划一条步兵可走的路径。
    // ========================================================================
    virtual bool Put(const CoordStruct& coords, int32 a2) override;
    bool MoveTo(const CoordStruct& where);

    // ========================================================================
    // 卸载 / 区域防守（asm: InfantryClass_Unload / _AreaGuard）
    //
    //  Unload 推进步兵的"收起"-"展开"状态机：简单部署单位按当前序列在
    //  展开/收起之间切换；AreaGuard 先推进部署动画，未定形时继续，否则退回
    //  基类的区域防守。
    // ========================================================================
    void Unload();
    void AreaGuard();

    // ========================================================================
    // 场景 INI 读写（asm: InfantryClass_CreateFromINIList /
    //  _SaveToMapINIList）
    // ========================================================================
    static void CreateFromINIList(INIClass* pINI);
    static void SaveToMapINIList(INIClass* pINI);

    // ========================================================================
    // 初始化（asm: InfantryClass_Init）
    //
    //  在构造末尾调用：把本单位登记进所属房屋的追踪表；按房屋的"老兵/精英"
    //  规则以及类型自身的老兵标记决定是否直接以老兵身份出场；把类型的初始
    //  朝向同步到自身的两套朝向字段。
    // ========================================================================
    void Init();

    virtual bool IsTechno() const;
    virtual bool IsInfantry() const;
    virtual bool IsUnit() const;
    virtual bool IsAircraft() const;
    virtual bool IsBuilding() const;
    virtual AbstractType WhatAmI() const;
    virtual int32 Size() const;
    virtual HRESULT GetClassID(CLSID* pClassID);

    // ========================================================================
    // Constructor
    // ========================================================================
    InfantryClass(HouseClass* pOwner) noexcept;

protected:
    explicit __forceinline InfantryClass(noinit_t) noexcept : FootClass(noinit) {}

    // ========================================================================
    // Properties
    // ========================================================================
public:
    InfantryTypeClass* Type;
    Sequence CurrentSequence;
    Sequence UnkSequence;
    // asm +0F8h：当前序列内的动画帧号。InfantryClass_DoAction 在切序列时
    //   置为 [0, 序列.FrameCount) 的随机值（randomStartFrame）或 0。
    //   sub_518D80 用它配合 FrameCount 取模得到循环帧。
    int32 AnimationFrame;
    bool IsCrawlingNow;
    bool IsProneNow;
    bool IsDeployedNow;
    bool IsParadropping;
    bool IsBoarding;
    bool IsUnboarding;
    bool IsFiringNow;
    bool IsAiming;
    bool IsStunnedNow;
    BYTE align_7D9[3];
    int32 FearLevel;
    int32 PanicTimerVal;
    CDTimerClass PanicTimer;
    bool IsPanicking;
    bool IsABombNow;
    bool IsC4Now;
    bool IsTerrorDrone;
    // 技工标记（+0x6D9）：步兵被改造为技工（如从被拆解的建筑中出来）时置位，
    // GetUIName 据此返回 TXT_TECHNICIAN。
    bool IsTechnician;
    BYTE align_7F0[4];
    bool IsCivilian;
    bool IsBrute;
    bool IsOccupying;
    bool IsUsingDeployFireWeapon;
    bool IsUsingSecondaryWeapon;
    bool IsFiringWhileMoving;
    bool IsSwimming;
    bool IsDog;
    bool IsEngineerNow;
    bool IsThief;
    bool IsCow;
    bool IsChrono;
    bool IsTanya;
    bool IsBoris;
    bool IsSEAL;
    bool IsSpy;
    bool IsIvan;
    bool IsDesolator;
    bool IsCrazyIvan;
    bool IsCosmonaut;
    bool IsYuri;
    bool IsInitiate;
    bool IsVirus;
    bool IsSuperSoldier;
    bool IsMutant;
    bool IsJumpJet;
    bool IsDeployedFire;
    bool IsFiringFromVehicle;
    bool IsInWater;
    bool IsDemolition;
    bool IsInVehicle;
    bool IsInOpenTopped;
    BYTE align_818[4];
    // Reserved layout buffer for binary compatibility with original engine.
    // These bytes correspond to internal state fields in the original binary
    // (mission state, targeting, pathfinding, animation timers, etc.) that
    // are managed through the virtual method implementations above.
    BYTE ReservedLayout[0xA10 - 0x81C];

    // ========================================================================
    // Reconstruction working state
    //
    // Named fields that back the virtual-method implementations below.  These
    // complement the binary-compatible ReservedLayout buffer above and model
    // the high-level gameplay state (mission queue, targeting, destination
    // and the special-effect "Now" flags) that the original engine stored
    // inside the reserved region.  They are declared after the reserved
    // buffer so the binary-compatible footprint (offsets up to 0xA10) is
    // preserved; the cloak / iron-curtain / force-shield / temporal states
    // intentionally reuse the timers inherited from TechnoClass rather than
    // duplicating them here.
    // ========================================================================
    // ★ 任务状态不再在本类另存一份：CurrentMission / QueuedMission 已由
    //   MissionClass（继承链上）持有，本类只做副作用（序列/姿态/停火）。
    AbstractClass* TargetObj;          // current target (backed by Get/SetTarget)
    CoordStruct    DestinationCoord;   // current movement destination
    bool           IsSleepingNow;      // true while the Sleep mission is active
    bool           IsLockedNow;        // true while command input is locked out
    bool           IsFrozenNow;        // true while frozen ( Freeze / Unfreeze )
    bool           IsAliveNow;         // liveness flag (cleared by Kill/OnDestroyed)
    bool           IsMindControlledNow;// true while under external mind control
    bool           IsDisguisedNow;     // true while a spy is disguised

    // Per-instance serialization for the save-game stream (base + derived).
    void Save(class SaveGameClass& saver) const;
    void Load(class LoadGameClass& loader);
    TechnoClass*   MindControlVictim;  // techno this infantry is controlling

    // 最近一次本体绘制的屏幕包围盒（SHP 帧 dst），供血条/选中框定位。
    RectangleStruct LastRenderRect;
};
