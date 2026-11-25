#pragma once

#include "RadioClass.h"
#include "TechnoTypeClass.h"
#include <Math/Timer.h>

class BuildingClass;
class WaveClass;   // asm: _TechnoClass.t.FiringWave 持有的波束对象
class TemporalClass; // asm: _TechnoClass.t.TemporalImUsing / TemporalTargettingMe
class CaptureManagerClass; // asm: _TechnoClass.t.CaptureManager 心灵控制管理器
class SpawnManagerClass;   // asm: _TechnoClass.t.SpawnManager 子机管理器（航母/驱逐舰/无畏）
class SlaveManagerClass;   // asm: _TechnoClass.t.SlaveManager 奴隶矿工管理器
class ParasiteClass;       // asm: _TechnoClass.t.Parasite 寄生武器（恐怖机器人）
class BombClass;           // asm: _TechnoClass.t.Bomb 附着炸弹（伊文 C4）
class AnimClass;           // asm: _TechnoClass.t.BehindAnim / DeployingAnim

// ============================================================================
// CloakStateEnum - tracks the cloak fade animation
// ============================================================================
enum class CloakStateEnum : int32 {
    Idle        = 0,
    Cloaking    = 1,
    Cloaked     = 2,
    Uncloaking  = 3
};

// ============================================================================
// 原版真实继承链（vtable 符号 vt_TechnoClass.r.m.o.* 已证实）：
//     AbstractClass -> ObjectClass -> MissionClass -> RadioClass -> TechnoClass
// TechnoClass 从此不再直接派生自 ObjectClass，而是派生自 RadioClass（其上
// 依次是 MissionClass / ObjectClass）。这样 TechnoClass::Update 可以按
// asm L1561947 直接调用 MissionClass::Update()，且 FootClass 的 Mission_*
// 覆写自然落进任务层虚槽。
// ============================================================================
class TechnoClass : public RadioClass {
public:
    static const AbstractType AbsID = AbstractType::Techno;

    static DynamicVectorClass<TechnoClass*>* Array;

    TechnoClass() noexcept
        : RadioClass()
        , TechnoType(nullptr)
        , Health(0)
        , MaxHealth(0)
        , VeterancyLevel(0)
        , Experience(0)
        , ArmorMultiplier(1.0)      // asm: _TechnoClass.t.ArmorMultiplier 默认 1.0
        , CloakState(CloakStateEnum::Idle)
        , CloakAlpha(255)
        , FireRechargeTimer(0)
        , CloakTimer(0)
        , RepairActive(false)
        , RepairRate(0)
        , IronCurtainTimer(0)
        , ForceShieldTimer(0)
        , IronTintTimer(0), IronTintStage(0)
        , AirstrikeTintTimer(0), AirstrikeTintStage(0)
        , LastFireFrame(-0x40000000)   // "从未开火"哨兵：留足余量，任何 Frame-last 都不会溢出
        , FiringWave(nullptr)       // asm: _TechnoClass.t.FiringWave 初始为空
        , FireDamageTimer(0)
        , SparkyCounter(0)
        , IsParasited(false)
        , TemporalTimer(0)
        , GasTimer(0)
        , RadiationTimer(0)
        , Tunnel(false)
        , ActiveTurretIndex(0)
        , WeaponStageFrame(0)
        , RecoilAmount(0)
        , RecoilStartFrame(0)
        , IsDyingNow(false)
        , KilledBy(nullptr)
        , IsTalking(false)
        , TalkBubbleText(nullptr)
        , TalkBubbleEnd(0)
        , LastTalkFrame(0)
        , PlanningDestination()
        , SmokeSystemActive(false)
        , SmokeSystemStage(0)
        , CurrentWeaponNumber(-1)
        , OrigOwner(nullptr)
        , IsCapturedFlag(false)
        , IsDisguisedFlag(false)
        , WarpInTimer(0)
        , WarpOutTimer(0)
        , IsWarpingOutFlag(false)
        , DisguiseCreationFrame(-1)
        , DisguiseBlinkTimer()
        , DisguiseTypeId(-1), DisguiseHouse(nullptr)
        , BunkerLinkedItem(nullptr)
        , PassengerCount(0), PassengerHead(nullptr), PassengerCapacityCount(0)
        , PrimaryFacing()
        , GroundHeight(0)
        , DrainTimer(0)
        , PlanningToken(-1)
        , WeaponStage(0)
        , FocusOnUnit(nullptr)
        , TemporalImUsing(nullptr)
        , TemporalTargettingMe(nullptr)
        , CaptureManager(nullptr)
        , SpawnManager(nullptr)
        , SlaveManager(nullptr)
        , Parasite(nullptr)
        , Bomb(nullptr)
        , IsBeingWarpedOutFlag(false)
        , QueuedVoiceIndex(-1)
        , CurrentAmmo(0)
        , ReloadTimer()
        , AirstrikeTimeStart(0)
        , AirstrikeTimeLeft(0)
        , AirstrikeTimeGen(0)
        , LastTarget(nullptr)
        , TargetLockActive(false)
        , ThreatPosed(0)
        , RadioFlag418(false)
        , RadioFlag419(false)
        , RearmCount(0)
        , IsDizzy(false)
        , Deactivated(false)       // asm: _TechnoClass.t.Deactivated (+0x1C8)
        , EMPDurationRemaining(0)  // asm: _TechnoClass.t.EMPDurationRemaining
        , SlaveOwner(nullptr)      // asm: _TechnoClass.t.SlaveOwner (+0x2DC)
        , BehindAnim(nullptr)      // asm: _TechnoClass.t.BehindAnim (+0x128)
        , DeployingAnim(nullptr)   // asm: _TechnoClass.t.DeployingAnim (+0x12C)
    {
        // asm: TechnoClass_CTOR links the new instance into the global
        //   TechnoClass::Array; every engine sweep (AI, radar, save) walks
        //   that table, so an object that only lives in vec_Logics is
        //   invisible to them.
        Add_To_Array(this);
    }
    virtual ~TechnoClass()
    {
        Remove_From_Array(this);
    }

    virtual AbstractType WhatAmI() const override { return AbstractType::Techno; }

    // asm: vtable 槽 GetTechnoType（vt+0x84）—— 任何技术对象都应回自己的类型。
    //   基类 ObjectClass 的实现是 `return nullptr`，而全项目没有一处覆写，
    //   于是 GetTechnoType() 恒空：TaskForce 招募（按类型匹配坑位）、脚本/
    //   触发器的目标解析、AI 的"能否生产该类型"全部失效。
    virtual TechnoTypeClass* GetTechnoType() const override { return TechnoType; }
    virtual int32 Size() const override { return sizeof(TechnoClass); }

    virtual HRESULT GetClassID(CLSID* pClassID) override { return E_FAIL; }
    virtual HRESULT Load(IStream* pStm) override { return S_OK; }
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override { return S_OK; }

    // ------------------------------------------------------------------------
    // Mission/Radio 层的接入点（继承链 Techno : Radio : Mission : Object 已接通）
    //
    //  原版把三者都内联在 TechnoClass 的字段里，OpenYR 分层后由这里补齐：
    //    GetRadioHouse     asm: RadioClass_ReceivedRadioCommand 读 [this+0x21C]
    //                      （Owner 房，Hello 的结盟门）
    //    GetRadioSelf      asm: `push Is_Techno(this)` —— 让对端知道发送者是谁
    //    GetMissionHealth  asm: MissionClass_Update +56 读 _ObjectClass.o.Health
    // ------------------------------------------------------------------------
    HouseClass*  GetRadioHouse()     const override { return Owner; }
    TechnoClass* GetRadioSelf()      const override { return const_cast<TechnoClass*>(this); }
    int32        GetMissionHealth()  const override { return Health; }

    // 无线电报文槽（asm: vt_TechnoClass.r.m.o.Techno_ReceivedRadioCommand）。
    // 原版这一槽由 TechnoClass::Receive_Message 实现，再往下才落到 RadioClass
    // 的 Exit/Hello 状态机；参数顺序沿用本项目的 (cmd, 源对象, 载荷) 约定。
    virtual int32 ReceivedRadioCommand(int32 cmd, int32 arg0, int32 a4) override;

    HouseClass* GetOwningHouse() const { return Owner; }
    // asm: [this+21Ch] —— 建筑/单位身上的"归属房索引"。BaseClass::Placed 的
    //   第一分支就是拿它跟 Base.OwnerHouseIndex 比：相等才按"类型 + 格子"
    //   精确匹配节点。此前这里恒返回 0，等于把每座建筑都当成 0 号房的，
    //   AI 房的基地节点永远打不上标记 —— 清单不收敛，AI 会无限重复造同一座。
    //   定义放在 TechnoClass.cpp（此处 HouseClass 尚未完整定义）。
    int32 GetOwningHouseIndex() const;

 // TechnoClass_GetThreatPosed.  The threat this object
    // presents to the enemy, counted into the owning house's threat grid.
    //
    //   * Without a type the answer is zero.
    //   * A building carrying occupants (WhatAmI == 6) multiplies the occupant
    //     count by RulesClass::ThreatPerOccupant.
    //   * Otherwise the type's ThreatPosed value is returned.
    virtual int32 GetThreatValue() const;

    // The cell the object currently stands in.
    CellStruct Get_Cell_Ptr_Coord() const;

    // ========================================================================
    // Static Array management
    // ========================================================================
    static void Init_Array();
    static void Delete_Array();
    static int32 Add_To_Array(TechnoClass* pInstance);
    static bool Remove_From_Array(TechnoClass* pInstance);
    static int32 Get_Total_Count();
    static TechnoClass* Get_Instance(int32 index);
    static int32 Find_Index(TechnoClass* pInstance);

    // ========================================================================
    // Update loop (AI, combat, cloaking)
    // ========================================================================
    virtual void Update() override;
    void Update_AI();
    virtual void Update_Combat();
    void Update_Cloak();
    void Update_Repair();
    void Update_Veterancy();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 UpdateTint_IronCurtain 负责推进"铁幕"染色动画：
    //  单位不在铁幕之下时什么都不做；在铁幕之下则按阶段计时，从一个阶段
    //  逐步走到下一个阶段，阶段切换的同时重设本阶段的持续时间，从而形成
    //  金属光泽循环闪烁的观感。
    // ------------------------------------------------------------------------
    void UpdateTint_IronCurtain();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 UpdateTint_Airstrike 负责推进"空袭标记"染色动画：
    //  只有正被空袭击中的目标才需要闪烁；按阶段计时逐个推进，闪烁一轮后
    //  若空袭已经结束就把阶段复位，否则重新开始下一轮。
    // ------------------------------------------------------------------------
    void UpdateTint_Airstrike();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ClearPlanningNodes 负责清掉本单位所属的行军规划
    //  令牌：找到本阵营当前使用的规划令牌后，把令牌上记录的节点清空，使
    //  单位不再沿既有路线行动。单位被俘获或易主时用它。
    // ------------------------------------------------------------------------
    void ClearPlanningNodes();

    // ========================================================================
    // 根据游戏行为，可知 DrawVeterancy 在单位头顶按军衔画出升阶标记（老兵
    //  一枚、精锐两枚），新兵不画。
    // ========================================================================
    void DrawVeterancy(Point2D* pCoord, RectangleStruct* pRect);

    // ========================================================================
    // asm: TechnoClass_DisplayTalkBubble (asm:1600862)
    //      void __fastcall(TechnoClass* this, int type)
    //
    //  让**游戏全局**只有一个人说话：写入 TalkBubbleType / TalkBubbleOwner /
    //  TalkBubbleTimer（时长取 Rules->TalkBubbleTime，默认 300ms），再让该
    //  单位所在格可见。type 是 TALKBUBL.SHP 的 1-based 帧号；
    //  type == 0 表示"收掉当前话泡"（Owner 与 Type 一并清零）。
    //
    //  绘制侧在 TechnoClass::DrawExtras 里：只有 TalkBubbleOwner == this 且
    //  计时未到时才画 TALKBUBL.SHP 的第 type-1 帧。
    // ========================================================================
    void DisplayTalkBubble(int32 type);

    // 原版的函数体允许 this == nullptr（= "没人说话"，用于动作清场）。
    // C++ 成员函数做不到空 this，因此把真正的实现放在这个静态入口上，
    // 成员版本只是转发。Owner 为 nullptr 时等价于清空全局话泡。
    static void DisplayTalkBubble_Global(TechnoClass* pOwner, int32 type);

    // ========================================================================
    // 根据游戏行为，可知 CreatePlanningToken 为一个将要执行的行军规划点分配
    //  一个规划令牌槽位；已有未消费令牌时复用。
    // ========================================================================
    int32 CreatePlanningToken(const CoordStruct& dest);

    // ========================================================================
    // 根据游戏行为，可知 GetNextMission 在没有外部指令时为单位挑一件"下一该
    //  做的事"（脚本/队列优先，其余进入警戒）。
    // ========================================================================
    Mission GetNextMission() const;

    // ========================================================================
    // 根据游戏行为，可知 PrintSelectedTip 把单位的名称与状态填进选中提示，
    //  供界面的信息栏显示。
    // ========================================================================
    void PrintSelectedTip(wchar_t* pBuffer, int32 bufferLength) const;

    // ========================================================================
    // 根据游戏行为，可知 ShouldSuppress 判断本次开火是否该被压住（目标已死、
    //  已不在有效打击范围）。
    // ========================================================================
    bool ShouldSuppress(AbstractClass* pTarget) const;

    // ========================================================================
    // asm: TechnoClass_9C   (asm:1574440, retn)
    //
    //   char __thiscall CanBeCommanded(_TechnoClass *this)
    //   {
    //       if (this->Deactivated /* +0x1C8 */) return false;
    //       if (!this->Owner /* +0x21C */->Is_Player()) return false;
    //       ws = this->GetWeapon_TurretChanger();      // vt+0x3F4
    //       if (!ws) return false;
    //       if (*ws == 0) return false;                // 武器槽里没有 WeaponType
    //       if (this->IsUnderEMP()) return false;      // vt+0x37C
    //       return true;
    //   }
    //
    //  这是「玩家发出的指令能不能落到这个单位身上」的总闸门；Stop / Deploy
    //  两条命令的循环都以它（或 0x0A0 CanReceiveWaypoint）为先决条件。
    //  0x3F4 → GetTurretChangingWeapon()，0x37C → IsUnderEMP()。
    // ========================================================================
    bool Can_Be_Commanded() const;

    // ========================================================================
    // Init_Locomotion —— 按 TechnoType->Locomotor 装配行走逻辑（asm +0x674）
    //
    //   原版在 TechnoClass_Init 里用 CoCreateInstance(type->Locomotor) 造出
    //   ILocomotion 子对象再 Link_To_Object(this)。本树此前从未装配，
    //   于是 FootClass::Update_Movement 的 `Locomotor == nullptr` 门恒真、
    //   所有单位永远不动。这里补上同一件事：
    //     ① 解析 GUID 串（或裸名字）→ CLSIDs 索引；
    //     ② CreateInstance → Link_To_Object(this)；
    //     ③ 类型没写 Locomotor 时按 RTTI 给默认值
    //        （Unit→Drive、Infantry→Walk、Aircraft→Fly）。
    //   已有实例或类型缺失时安全返回；返回是否最终持有行走逻辑。
    // ========================================================================
    bool Init_Locomotion();

    // ========================================================================
    // Start_Default_Mission —— 落位后的任务启动（asm TechnoClass::Unlimbo 尾）
    //
    //   `call Enter_Idle_Mode(1,1)` 只把 Guard 塞进待提升槽（field_B4），
    //   随后 asm 还有 `if (vt+200h()) call [vt+1ECh]` 才真正把它提升为
    //   当前任务。本树此前只做了前半段，于是所有新对象的 CurrentMission
    //   恒为 None(-1)，32 路分派落进 default（QMove/Sleep），任务系统空转。
    //   这里补齐后半段；已有任务的对象不受影响。
    // ========================================================================
    void Start_Default_Mission();

    // ------------------------------------------------------------------------
    // asm: TechnoClass_374   (asm:1572021)   虚表槽 0x374
    //
    //   原版把 `mission` 包成 NetworkEvent 塞进 0x80 槽环形包队列，由各端
    //   按序消费。本工程没有该包格式，功能等价地直接落到 MissionClass 的
    //   任务队列：受理成功返回 true（命令随之播确认语音），被拒返回 false。
    // ------------------------------------------------------------------------
    bool DispatchMission(Mission mission);

    // ========================================================================
    // asm: TechnoClass_37C   (asm:1600595)   —— 虚表槽 0x37C
    //
    //     mov edx,[ecx+EMPDurationRemaining] / xor eax,eax / test / setnle al
    //     return EMPDurationRemaining > 0;
    //
    //  EMP 脉冲（EMPulseClass_Initialize 写 [+30h] 到这里）打上的瘫痪计时；
    //  TechnoClass_Update 每帧递减，归零那一帧才恢复动力/雷达。
    // ========================================================================
    virtual bool IsUnderEMP() const;

    // ========================================================================
    // 虚表槽 0x380 —— asm 里 Techno 对象指向 FootClass_IsParalysed。
    //  TechnoClass 本身没有瘫痪计时器，基类实现恒为 false；FootClass 覆写
    //  后读取 ParalysisTimer。CanSetWaypoint（以及 Can_Be_Commanded）都走
    //  虚调用，所以建筑/地形自然落到这里的 false。
    // ========================================================================
    virtual bool IsParalysed() const;

    // ========================================================================
    // 根据游戏行为，可知 PredictTargetCoords 按目标当前位置与速度推算炮弹
    //  抵达时刻目标会在哪，用于带提前量的武器。
    // ========================================================================
    CoordStruct PredictTargetCoords(AbstractClass* pTarget, int32 flightTime) const;

    // ========================================================================
    // 根据游戏行为，可知 FireEBolt 朝目标打出一道电弧并在命中时结算伤害。
    // ========================================================================
    bool FireEBolt(AbstractClass* pTarget, int32 damage);

    // ========================================================================
    // 根据游戏行为，可知 DistributeFire 把一轮火力按"离中心越远衰减越多"的
    //  方式分摊到目标附近区域内的敌人身上。
    // ========================================================================
    int32 DistributeFire(AbstractClass* pTarget, int32 damage, int32 radius);

    // ========================================================================
    // 根据游戏行为，可知 UpdateGattling_ 推进转管机枪类武器的射速档位：
    //  连射升档、停火降档。
    // ========================================================================
    void UpdateGattling_(int32 stages, int32 rate);

    // ========================================================================
    // 根据游戏行为，可知 Drain 处理吸取类效果：把本单位当前的能量/生命按
    //  额度转走一部分给发起者。
    // ========================================================================
    int32 Drain(int32 amount, TechnoClass* pSource);

    // ========================================================================
    // 根据游戏行为，可知 Die 是所有战斗单位死亡的统一入口：残骸、经验、
    //  统计都在这里结算，子类按各自的表现扩展它。
    // ========================================================================
    virtual void Die(TechnoClass* pKiller);

    // ========================================================================
    // 根据游戏行为，可知 Recoil 推进炮管后座的表现状态：开火瞬间后座拉满，
    //  随后按帧复位；复位期间渲染层把炮管画在偏后的位置。
    // ========================================================================
    void Recoil(int32 amount);

    // ========================================================================
    // 根据游戏行为，可知 GetThreatPosed 给出"本单位对某个阵营构成的威胁
    //  值"：AI 挑目标时用它排序，威胁越大的单位越优先被打。
    // ========================================================================
    int32 GetThreatPosed(HouseClass* pToHouse) const;

    // ========================================================================
    // Fire weapon implementation
    // ========================================================================
    BulletClass* Fire_Impl(AbstractClass* pTarget, int32 nWeaponIndex);

    // ------------------------------------------------------------------------
    // Fire_WaveWeapon —— 音波/磁能波类武器的开火入口
    //
    // asm: TechnoClass_Fire 中两处 `new(0x240)` + `WaveClass_CTOR` 的装配点。
    //   1) `WeaponTypeClass::IsSonic`  为真 → WaveClass(type=0, 声波束)
    //   2) `WeaponTypeClass::IsMagBeam` 为真 → WaveClass(type=3, 磁能束)
    // 生成的波存入 this->FiringWave（同一时刻仅一条）。
    // 返回 true 表示本次开火由"波"武器消费掉（不发射实体弹丸）。
    // ------------------------------------------------------------------------
    bool Fire_WaveWeapon(AbstractClass* pTarget, WeaponTypeClass* pWeapon, int32 nWeaponIndex);

    // ========================================================================
    // TakeDamage implementation
    // ========================================================================
    //  ★ 这里是**唯一**做装甲倍率与 Verses 结算的地方（对应 asm 1575590 一带的
    //    GetTypeArmorMult / ArmorMultiplier / fdivr / GetTotalDamage）。
    //    TechnoClass::ReceiveDamage 只负责"入口语义"（铁幕、忽略防御、写回
    //    伤害值、死亡状态、受击还击），不再自己算一遍 —— 早先两处都算，
    //    同一个伤害被缩放两次（50% Verses 实际只吃 25%）。
    //
    //    pFinalDamage 非空时回报实际扣掉的血量（缩放 + Verses 之后）。
    bool TakeDamage_Impl(int32 damage, ObjectClass* pSource,
                         WarheadTypeClass* pWarhead,
                         int32* pFinalDamage = nullptr,
                         bool ignoreDefenses = false);

    // ------------------------------------------------------------------------
    // asm: eDamageState __thiscall TechnoClass_ReceiveDamage(
    //          _TechnoClass *this, int *Damage, int TypeSource,
    //          WarheadTypeClass *WH, int target, int ignoreDefenses,
    //          int arg10, HouseClass_old *SourceHouse)          (asm:1575554)
    //
    //  单位承受伤害的统一入口，参数与汇编逐一对应：
    //    pDamage        —— 伤害量的指针（原地改写，返还最终值）
    //    typeSource     —— 伤害来源类型（0 = 普通武器）
    //    pWarhead       —— 弹头（Verses 倍率来源）
    //    pAttacker      —— 攻击者对象（原版形参写作 `int target`，实际是按指针用：
    //                      受击还击要拿它当 ShouldRetaliate 的实参，也会对它调
    //                      GetCoords()/GetTechnoType()。这里按真实语义写成指针）
    //    ignoreDefenses —— 非零时跳过装甲与铁幕减免
    //    arg10          —— 附加标志位
    //    pSourceHouse   —— 伤害来源所属方（用于仇恨与结算）
    //  返回 eDamageState。
    // ------------------------------------------------------------------------
    int32 ReceiveDamage(int32* pDamage, int32 typeSource,
                        WarheadTypeClass* pWarhead, TechnoClass* pAttacker,
                        int32 ignoreDefenses, int32 arg10,
                        HouseClass* pSourceHouse);

    // ========================================================================
    // Repair logic
    // ========================================================================
    void Repair_Start(int32 rate);
    void Repair_Stop();

    // ========================================================================
    // Cloak / Uncloak
    // ========================================================================
    virtual void Cloak(bool bPlaySound);
    virtual void Uncloak(bool bPlaySound);
    bool Is_Cloaked() const;
    bool Is_Cloaking() const;

    // ========================================================================
    // Veteran / Promote
    // ========================================================================
    void Promote(int32 experience);
    virtual int32 GetVeterancy() const;
    int32 Get_Experience() const;

    // asm: char __thiscall Veterancy_AddValue(float *this, int ownerCost, int victimCost)
    //      (asm:1725745)
    //   Accumulates veterancy credit for destroying a victim of the given
    //   cost:  Experience += victimCost / (ownerCost * Rules->VeteranRatio),
    //   clamped to Rules->VeteranCap.  The original stores the running total
    //   in a float; OpenYR folds it into the integer Experience pool.
    void AddVeterancyValue(int32 ownerCost, int32 victimCost);

    // asm: vt_TechnoClass.Enter_Idle_Mode (+0x484), called as (0, 1) when a
    //   temporal weapon finishes and its owner must drop back to idle.
    virtual void Enter_Idle_Mode(int32 a3, int32 a4);

    // ========================================================================
    // Is_Ally / Is_Enemy
    // ========================================================================
    bool Is_Ally(HouseClass* pHouse) const;
    bool Is_Enemy(HouseClass* pHouse) const;
    bool Is_Ally(TechnoClass* pTechno) const;
    bool Is_Enemy(TechnoClass* pTechno) const;

    // ========================================================================
    // Threat position
    // ========================================================================
    CoordStruct Get_Threat_Pos() const;

    // ========================================================================
    // CRC
    // ========================================================================
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // Per-instance serialization for the save-game stream.
    void Save(class SaveGameClass& saver) const;
    void Load(class LoadGameClass& loader);

    // ========================================================================
    // Combat core (mirrors TechnoClass_* in the original binary)
    // ========================================================================
    int32  SelectWeapon(AbstractClass* pTarget);
    bool   IsCloseEnoughToTarget(AbstractClass* pTarget, int32 idxWeapon);
    int32  EvalThreatRating(TechnoClass* pThreat, int32 idxWeapon);
    int32  EstimateDamage(AbstractClass* pTarget, int32 idxWeapon);
    bool   ShouldRetaliate(TechnoClass* pAttacker);
    void   RegisterDestruction();
    void   RegisterLoss();
    CoordStruct GetFLH(int32 nWeaponIndex, bool muzzle);
    bool   IsRadarVisible(HouseClass* pHouse) const;
    int32  Get_ZAdjustment() const;
    VisualType VisualCharacter(bool raw);
    void   CreateGap();
    void   DeleteGap();
 // TechnoClass_UpdatePowered: re-evaluates whether this
    // techno still receives the power feed it depends on.  Called by an
    // owning structure's BuildingClass_InitMore for each powered unit slot.
    void   UpdatePowered();
    void   UpdateSight();
    void   DrawExtras(Point2D* pCoord, RectangleStruct* pRect);
    void   DrawHidden(Point2D* pCoord, RectangleStruct* pRect);
    void   DealParticleDamage(TechnoClass* pVictim, WarheadTypeClass* pWarhead,
                              int32 damage, int32 distanceFromEpicenter);
    // asm: TechnoClass_PointerGotInvalid proc near / a2=dword ptr 4 / a3=byte ptr 8 / retn 8
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed);
    int32  GetSightRange() const;
    bool   GapActive;

    // ========================================================================
    // TechnoClass virtuals (preserved from original header)
    // ========================================================================
    virtual bool IsVoxel() const { return false; }
    virtual void Destroyed(ObjectClass* Killer) {}
    virtual bool CanScatter() const { return false; }
    virtual int32 GetDefaultSpeed() const { return 0; }
    virtual bool HasTurret() const;
    virtual bool CanDeploySlashUnload() const { return false; }
    virtual bool IsUnitFactory() const { return false; }
    virtual void TakeDamage(int32 damage, ObjectClass* source, WarheadTypeClass* warhead) {}
    virtual bool IsEngineer() const { return false; }
    virtual bool IsCloseEnough(AbstractClass* pTarget, int32 idxWeapon) const { return false; }
    virtual bool IsCloseEnoughToAttack(AbstractClass* pTarget) const { return false; }
    virtual bool IsInAir() const;
    virtual bool IsOnFloor() const;
    virtual bool OnBridge() const;

    // ── TechnoClass state probes / accessors ──────────────────────────────
 // TechnoClass_IsCrewed: mirrors the type's Crewed flag.
    virtual bool IsCrewed() const;
 // TechnoClass_IsFactory: base-class answer is false.
    virtual bool IsFactory() const;
 // 根据游戏行为，可知 HasMultipleTurrets 负责下面这段逻辑。
    virtual bool HasMultipleTurrets() const;
 // 根据游戏行为，可知 GetZ 负责下面这段逻辑。
    virtual int32 GetZ() const;
 // 根据游戏行为，可知 GetActiveTurretIndex 负责下面这段逻辑。
    virtual int32 GetActiveTurretIndex() const;
 // 根据游戏行为，可知 GetTurretIndex 负责下面这段逻辑。
    virtual int32 GetTurretIndex() const;
 // 根据游戏行为，可知 CurrentWeaponSelected 负责下面这段逻辑。
    virtual bool CurrentWeaponSelected() const;
 // 根据游戏行为，可知 GetOwner 负责下面这段逻辑。
    HouseClass* Get_Owner() const;
    // ── Disguise ──────────────────────────────────────────────────────────
 // TechnoClass_IsDisguised: reads the type/instance disguise
    // flag.  The two-argument form adds a range parameter the original accepts
    // and ignores.
    virtual bool IsDisguised() const;
    virtual bool IsDisguised2(int32 a2) const;
 // TechnoClass_ClearDisguise: drops the disguise.
    virtual void ClearDisguise();

    // ── Cloak / warp / temporal state ─────────────────────────────────────
 // 根据游戏行为，可知 IsCloakable 负责下面这段逻辑。
    virtual bool IsCloakable() const;
 // 根据游戏行为，可知 IsBeingWarpedOut 负责下面这段逻辑。
    virtual bool IsBeingWarpedOut() const;
 // 根据游戏行为，可知 IsWarpingOut 负责下面这段逻辑。
    virtual bool IsWarpingOut() const;
 // 根据游戏行为，可知 IsNotTemporalLocked 负责下面这段逻辑。
    virtual bool IsNotTemporalLocked() const;
 // 根据游戏行为，可知 IsNotWarpingIn 负责下面这段逻辑。
    virtual bool IsNotWarpingIn() const;
 // 根据游戏行为，可知 IsDraining 负责下面这段逻辑。
    virtual bool IsDraining() const;

    // ── Temporal / weapon-legal probes ────────────────────────────────────
 // TechnoClass_IsTemporalSource: true when this techno is
    // currently the source of a temporal weapon that has a victim.
    bool IsTemporalSource() const;
 // TechnoClass_IsLegalWeapon: true when the weapon container
    // is non-null and holds a weapon id.
    bool IsLegalWeapon(const void* pWeapon) const;
 // TechnoClass_GetNonSprayWeapon (-adjacent): returns the weapon
    // in the slot that IsNoSprayAttack selects.
    void* GetNonSprayWeapon() const;
 // TechnoClass_CanAreaFire: true when the current weapon is
    // flagged as an area-effect weapon.
    bool CanAreaFire() const;

    // ── Weapon selection helpers ──────────────────────────────────────────
 // 根据游戏行为，可知 CanPassiveAquire 负责下面这段逻辑。
    virtual bool CanPassiveAquire() const;
 // 根据游戏行为，可知 CanTraverse 负责下面这段逻辑。
    virtual bool CanTraverse() const;
 // 根据游戏行为，可知 CanSetWaypoint 负责下面这段逻辑。
    virtual bool CanSetWaypoint() const;

    // ── Miscellaneous probes ──────────────────────────────────────────────
 // 根据游戏行为，可知 NeedsToSelfHeal 负责下面这段逻辑。
    virtual bool NeedsToSelfHeal() const;
 // TechnoClass_GetHealthState: 0 = healthy, 1 = damaged,
    // 2 = critical, matching the rules-side health thresholds.
    virtual int32 GetHealthState() const;
 // TechnoClass_GetXYDistanceFrom: planar distance to another
    // object, in leptons.
    virtual double GetXYDistanceFrom(const AbstractClass* pOther) const;
    // ── Panic / idle / power probes ───────────────────────────────────────
 // TechnoClass_Panic (xx): the zero-argument form does nothing at
    // this layer; the mission-controller entry point is FootClass::Panic.
    virtual void Panic();
    // TechnoClass_Unpanic: the counterpart that returns the object to normal
    // morale; the base class does nothing.
    virtual void Unpanic();
    virtual void Scatter(const CoordStruct& crd, bool ignoreMission, bool ignoreDestination) {}
 // TechnoClass_IdleAction: returns false - derived missions
    // override it to report that they have finished idling.
    virtual bool IdleAction();
    virtual void UpdateIdleAction() {}
 // TechnoClass_IsPowerOnline (x): true only for powered
    // structures; the base class answers false.
    virtual bool IsPowerOnline() const;
    virtual bool IsArmed() const { return false; }
    virtual bool IsBeingRepaired() const { return false; }
    virtual bool IsCurrentlyBeingSold() const { return false; }
    virtual bool IsPowered() const { return false; }
    virtual bool IsSelling() const { return false; }

    // ========================================================================
    // 任务调度入口（对应原版 vt 偏移 0x1E8 一带的 mission 层）
    //
    //  原版把"设定任务 / 排队任务 / 读取任务 / 设定目标 / 读取目标"这一组操作
    //  放在任务层，任何 Techno 都可以通过虚表统一下发。载具、步兵、建筑各自
    //  覆盖它们以实现自己的副作用（换序列、切换匍匐或待发状态等）。
    //
    //  ★ 继承链接通后（MissionClass 已上链），基类不再提供空实现：
    //    GetMission 直接读任务层的 currentMission（asm +0xA8），SetMission 走
    //    MissionClass::ForceMission，QueueMission 走 MissionClass 的入队路径。
    //    子类覆写只补充自己的表现副作用，不再另立第二份任务状态。
    // ========================================================================
    virtual void SetMission(Mission mission) { MissionClass::ForceMission(mission); }
    virtual Mission GetMission() const { return MissionClass::GetMission(); }
    virtual void QueueMission(Mission mission) { MissionClass::Queue_Mission(mission); }

    // ------------------------------------------------------------------------
    // asm: TechnoClass::Override_Mission  (asm:1574861, retn 0Ch)
    //
    //  Techno 层把"换任务"与"换目标"绑成一次操作：
    //      LastTarget = Target;                    // 记下被顶掉的旧目标
    //      MissionClass::Override_Mission(...);    // 任务槽照旧切换
    //      SetTarget(target);                      // ★ 虚分派写目标
    //
    //  ★ 基类 MissionClass::Override_Mission 收下 target 却**什么也不做**
    //    （汇编里它确实只碰 mission 槽）。真正的目标落地在这一层，所以任何
    //    "Override_Mission(Attack, 敌人, ...)" 的调用都必须落到这里，否则
    //    任务切过去了、目标还是空的，Mission_Attack 下一步就自己退回 Guard。
    // ------------------------------------------------------------------------
    virtual void Override_Mission(Mission mission, AbstractClass* target,
                                  AbstractClass* destination) override;

    virtual void SetTarget(AbstractClass* /*pTarget*/) {}
    virtual AbstractClass* GetTarget() const { return nullptr; }
    virtual bool IsFiring() const { return false; }
    virtual bool IsDeploying() const { return false; }
    virtual bool IsBeingDrained() const { return false; }
    virtual bool IsSensorsOnline() const { return false; }
    virtual bool IsPowerDrain() const { return false; }
    virtual bool IsCharged() const { return false; }
    virtual bool IsFactoryActive() const { return false; }
    virtual bool IsLaserFencePost() const { return false; }
    virtual bool IsCapturable() const { return false; }
    virtual bool IsOccupiable() const { return false; }
    virtual bool IsRubble() const { return false; }
    virtual bool IsBridge() const { return false; }
    virtual bool IsWall() const { return false; }
    virtual bool IsGate() const { return false; }
    virtual bool IsOverlay() const { return false; }
    virtual bool IsLight() const { return false; }
    virtual bool IsVehicle() const { return false; }
    virtual bool IsTiberium() const { return false; }
 // TechnoClass_GetTiberium.  Sums the four tiberium storage
    // floats carried by a harvesting techno and floors the running total.  The
    // base implementation carries no storage and therefore reports zero.
    virtual double Get_Tiberium() const { return 0.0; }

    // IsBeingMindControlled - the vtable slot the original queries at +0x160.
    // True while this object is already under an external mind controller.
    virtual bool IsBeingMindControlled() const { return false; }

 // TechnoClass_CanBePermaMC.  Whether this object may be
    // permanently mind controlled by a psychic dominator:
    //
    //   * buildings never qualify (WhatAmI() == Building);
    //   * the techno type must not be ImmuneToPsionics;
    //   * the object must not already be mind controlled;
    //   * the techno type must not be a balloon-hover type;
    //   * the object must still be alive.
    virtual bool Can_Be_PermaMC() const
    {
        if (WhatAmI() == AbstractType::Building)
            return false;

        const TechnoTypeClass* pType = TechnoType;
        if (pType != nullptr && pType->IsImmuneToPsionics)
            return false;

        if (IsBeingMindControlled())
            return false;

        if (pType != nullptr && pType->IsBalloonHover)
            return false;

        return !IsDead();
    }
 // TechnoClass_CanOccupyFire: only buildings and the infantry
    // that garrison them answer true; the base class returns false.
    virtual bool CanOccupyFire() const;
 // TechnoClass_GetOccupantCount: number of occupants garrisoned.
    virtual int32 GetOccupantCount() const;

    // ── Layer / cell helpers ──────────────────────────────────────────────
 // TechnoClass_InWhichLayer: asks the locomotor which draw
    // layer this techno currently occupies.
    virtual int32 InWhichLayer() const;
 // TechnoClass_GetCellCoords: converts the world coordinates
    // into the owning cell's X/Y, dividing by 0x100 (one leptons-per-cell unit).
    virtual CellStruct GetCellCoords() const;

 // ── Threat / value ratings ( / 0x41B54F / 0x41B557) ──────
    virtual int32 GetAntiAirValue() const;
    virtual int32 GetAntiArmorValue() const;
    virtual int32 GetAntiInfantryValue() const;

 // TechnoClass_UpdateRefinerySmokeSystems: 根据游戏行为，可知建筑受损后开始
    //  冒烟，受损越重烟越浓；完好时不冒烟。
    virtual void UpdateRefinerySmokeSystems();

    // ========================================================================
    // Planning-token and type-flag probes
    // ========================================================================
 // TechnoClass_GetPlanningToken: the waypoint token slot
    // (+0x514).
    int32 GetPlanningToken() const;
 // TechnoClass_AttachPlanningToken: stores token into the
    // waypoint token slot (+0x514).
    void AttachPlanningToken(int32 token);
 // TechnoClass_Assign_Destination_Cell: stores the target
    // building into the "focus on unit" slot.
    void Assign_Destination_Cell(BuildingClass* pTarget);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 PresumeMissionComplete 负责在任务被打断时收尾：
    //  先松开正在施加的时间武器（若有），再检查对象是否还能继续执行任务，
    //  能在必要时请求下一个任务。返回是否"任务确已完成"。
    // ------------------------------------------------------------------------
    bool PresumeMissionComplete();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 IsNotCloakedByOthers 负责判断"这个单位此刻并没有
    //  被别人掩蔽"：它自身要么本来就能隐形，要么带隐形标记；并且不受 EMP、
    //  瘫痪、正在传送出入等影响；老兵/精英级的"不可被探测"能力会直接否决；
    //  最后要求所在格子上没有掩蔽该单位的隐身发生器（非本方的那一种）。
    // ------------------------------------------------------------------------
    bool IsNotCloakedByOthers() const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 IsCloakedByOthers 负责判断"这个单位此刻正被别的
    //  东西掩蔽"，它是 IsNotCloakedByOthers 的补集。
    // ------------------------------------------------------------------------
    bool IsCloakedByOthers() const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 DeselectIfNotPlayerOwned 负责在只剩一个选中对象时，
    //  若它的拥有方不是玩家则把它取消选中。返回是否真的执行了取消选中。
    // ------------------------------------------------------------------------
    bool DeselectIfNotPlayerOwned();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 ExpireDrain 负责清理"抽能"状态：让正在被抽取的
    //  目标松开抽能源，并且通知施加方的抽能对象解除关联。
    // ------------------------------------------------------------------------
    void ExpireDrain();

    // ------------------------------------------------------------------------
    // asm: TechnoClass_UpdateThreatToCell (0x70F714, retn 4)
    //   把威胁贡献从旧格搬到新格：Adjust_Threat(owner, -ThreatPosed) → 清
    //   ThreatPosed → 重算 → 记下 → Adjust_Threat(owner, +threat)。
    //   无参重载用自身所踩的格子顶替 asm 的显式 cell 参数。
    // ------------------------------------------------------------------------
    void UpdateThreatToCell(CellClass* pCell);
    void UpdateThreatToCell();

 // TechnoClass_NotSubmerged: true when the object's height is
    // above the submarine threshold (-20).
    bool NotSubmerged() const;
 // 根据游戏行为，可知 IsNotSprayAttack 负责下面这段逻辑。
    bool IsNotSprayAttack() const;
 // 根据游戏行为，可知 IsNotSprayAttack2 负责下面这段逻辑。
    bool IsNotSprayAttack2() const;
 // TechnoClass_SetCurrentWeaponStage: stores idx into the
    // multi-stage weapon counter (+0x140) when it is non-negative.
    void SetCurrentWeaponStage(int32 idx);
 // TechnoClass_HasTurretTooltips: the type's turret-tooltip
    // flag.
    bool HasTurretTooltips() const;

 // TechnoClass::Greatest_Threat.  The engine's universal
    // target-acquisition entry point.  `projFlags` is the projectile-
    // capability bitmask (ProjectileTypeFlags); `curThreat` seeds the best
    // threat so a caller can require a strictly better candidate; `a4`
    // enumerates the caller class (0 = building, 1 = vehicle, 2 = infantry).
    // Returns the best target found, or null.
    virtual ObjectClass* Greatest_Threat(int32 projFlags, int32 curThreat, int32 a4);

 // TechnoClass_Combat_Damage.  Weapon-slot damage query used
    // by Greatest_Threat to decide whether a special movement class (engineer
    // / terrorist) should ignore military targets.
    int32 Combat_Damage(int32 idxWeapon) const;

 // TechnoClass_Techno_31C.  Vtable +0x31C - resolves the
    // techno's current target object honouring the requested slot.
    ObjectClass* Techno_31C(int32 which) const;

    // ========================================================================
    // Position / altitude probes
    // ========================================================================

 // TechnoClass_GetCellCoords1.  Writes the techno's owning
    // map cell (floored to the cell grid) into `pOut` and returns it.
    CellStruct* GetCellCoords1(CellStruct* pOut) const;

 // TechnoClass_GetCell1.  The CellClass the techno is
    // standing on, or null off-map.
    CellClass* GetCell1() const;

 // TechnoClass_OnFloor / _InAir.  True when
    // the techno's Z puts it on the ground / in the air.  Both test the
    // "has height" flag at +0x74 first, then compare the current Z against
    // twice the ObjectClass::HeightAtSpawn offset (the parked-on-ground
    // threshold).
    bool OnFloor() const;
    bool InAir() const;

 // TechnoClass_GetZFudgeCliff / _Column /
 // _Tunnel.  FootClass::Get_ZAdjustment consults these to
    // snap a unit's draw height to the terrain in front of it.  Each returns
    // a fudge value in pixels.
    int32 GetZFudgeCliff() const;
    int32 GetZFudgeColumn() const;
    int32 GetZFudgeTunnel() const;

 // TechnoClass_GetElevationRangeBonus /
 // _GetElevationBonusNoSqrt.  The extra weapon range a
    // height advantage confers.  The NoSqrt variant skips the square root and
    // is used by the cheaper proximity test.
    double GetElevationRangeBonus(ObjectClass* pTarget) const;
    double GetElevationBonusNoSqrt(ObjectClass* pTarget) const;

 // TechnoClass_TimeForCellInset.  True when the distance to
    // the target exceeds (warhead CellSpread - CellInset), i.e. the shot has
    // already cleared the minimum arming distance.
    bool TimeForCellInset(TechnoClass* pTarget) const;

    // ========================================================================
    // Combat / role classifiers
    // ========================================================================

 // TechnoClass_CanLobber.  True when the techno's current
    // weapon is flagged as a lobber (arcing artillery).
    bool CanLobber() const;

 // TechnoClass_HasAbility.  True when the type carries the
    // requested special ability flag.
    bool HasAbility(int32 ability) const;

 // TechnoClass_CanBeBunkered.  True when this techno may be
    // loaded into a battle bunker / tank bunker.
    bool CanBeBunkered() const;

    // 根据游戏行为，可知 BunkerLinkedItem 是"掩体"与"窝在掩体里的单位"之间的
    // 双向链接指针：掩体这一端存着里面的单位，单位这一端反过来存着它所在的掩体。
    // 建立链接时两侧同时写入，拆除时两侧同时清空；它既是占用标记，也是反查入口。
    TechnoClass* BunkerLinkedItem;

    // ========================================================================
    // 乘客链（对应原版的 cPassengerNode）：{ 数量, 首节点 }
    //
    //  运输载具用它挂载乘员：首节点是一个 TechnoClass 指针，乘员之间再用
    //  自身的 NextObject 串成单向链。数量字段只做快速判空与容量判断用。
    // ========================================================================
    int32         PassengerCount;
    TechnoClass*  PassengerHead;

    // 根据游戏行为，可知 PassengerCapacityCount 是本单位作为运输载具时
    // 能装下的乘客数；非运输单位恒为零，满员判断据此进行。
    int32         PassengerCapacityCount;

    // 返回链条上的第一个乘客（无乘客时为空）。
    TechnoClass* Attached_Object() const { return PassengerHead; }

    // 把整个乘客链全部"封口"：逐一点掉每位乘客的对外射击许可。
    void BlockAllOpenToppedPassengers();

 // TechnoClass_CanBePermaMC.  True when this techno may be
    // permanently mind-controlled (Yuri Prime's capture).
    bool CanBePermaMC() const;

 // TechnoClass_BelongsToPlayer /
 // _PlayerOwnedAliveAndNamed.  Ownership probes used by the
    // damage text / EVA paths.
    bool BelongsToPlayer() const;
    bool PlayerOwnedAliveAndNamed() const;

 // TechnoClass_GetPointsValue.  The score value this techno
    // contributes when destroyed: the type's PointValue, plus the value of
    // everything it carries, plus the locomotor's contributed value.
    int32 GetPointsValue() const;

 // TechnoClass_GetTiberiumPercentage.  Ore storage fill
    // fraction (0.0..1.0).  Zero when the type has no storage.
    double GetTiberiumPercentage() const;

 // TechnoClass_GetFacingAgain.  Writes the current facing
    // through the out-pointer and returns it.
    DirStruct* GetFacingAgain(DirStruct* pOut) const;

    // ========================================================================
    // asm: TechnoClass_GetOwners / _GetHeight / _SetHeight / _QueueVoice
    //      / _IsMindControlled / _GetTurretChangingWeapon / _Activate
    //      / _Deactivate / _FireDeathWeapon
    // ========================================================================

 // TechnoClass_GetOwners: forwards to the type's owner bitmask.
    int32 GetOwners() const;

 // TechnoClass_GetHeight: the object's height above the cell floor it stands
    //  on.  A hovering object additionally subtracts ObjectClass::HeightAtSpawn.
    int32 GetHeight() const;

 // TechnoClass_SetHeight: re-bases the object's Z to the given world height,
    //  re-adding the on-floor offset when the object hovers above the ground.
    void SetHeight(int32 height);

 // TechnoClass_QueueVoice: stores a voice line index to be spoken later, but
    //  only for the local player's units and only when voice is enabled.
    void QueueVoice(int32 index);

 // TechnoClass_IsMindControlled: true while this object is under an external
    //  mind controller (a live CaptureManager, or the per-instance flag).
    bool IsMindControlled() const;

 // TechnoClass_GetTurretChangingWeapon: the weapon slot selected by the
    //  turret-changer stage (gattling / prism style); slot 0 otherwise.
    WeaponStruct* GetTurretChangingWeapon() const;

 // TechnoClass_Activate: power the object's locomotor on and clear the
    //  pending-activate flag, firing its activation sound once.
    void Activate();

 // TechnoClass_Deactivate: power the object's locomotor off.
    void Deactivate();

 // asm: int __thiscall TechnoClass_FireDeathWeapon(_TechnoClass *this,
    //         int additionalDamage)
    //
    //  Fires a unit's death weapon on destruction: uses the type's DeathWeapon
    //  (or the current turret-changing weapon), scaled by the type's
    //  DeathWeaponDamageModifier plus the caller's additional damage.  Returns
    //  the damage the death shot carried, or zero when no death weapon exists.
    int32 FireDeathWeapon(int32 additionalDamage);

    // ------------------------------------------------------------------------
    // asm: Desired_Facing256 (0x4C93C0)
    //
    //   void __fastcall Desired_Facing256(void *pState, int32 *pOut)
    //
    //   按「当前帧」推进一个转向状态的插值朝向。状态对象布局：
    //     +0x00  int16  FacingCurrent   （读写的是低字）
    //     +0x02  int16  padding         （保留）
    //     +0x04  int16  FacingTarget
    //     +0x08  int32  StartFrame      （-1 表示尚未开始）
    //     +0x10  int32  Duration
    //     +0x14  int16  TurnRate
    //
    //   推进规则（逐指令）：
    //     TurnRate <= 0                -> 直接输出当前值
    //     StartFrame == -1             -> remaining = Duration
    //     elapsed = Frame - StartFrame
    //     elapsed >= Duration          -> 直接输出当前值
    //     remaining == 0               -> 直接输出当前值
    //     diff = int16(FacingCurrent - FacingTarget)
    //     quotient = |diff| / TurnRate      ; quotient <= 0 -> 输出当前值
    //     steps   = Duration - elapsed （elapsed >= Duration 时取 0）
    //     FacingCurrent -= int16((|diff| / quotient) * steps)
    //     输出：完整 dword，仅低 16 位被替换
    // ------------------------------------------------------------------------
    static void Desired_Facing256(const void* pState, int32* pOut);

 // TechnoClass_GetDisguiseFlags /
 // _IsDisguisedAgainst.  Disguise-blinking state helpers.
    int32 GetDisguiseFlags(int32 flags) const;
    bool IsDisguisedAgainst(HouseClass* pHouse) const;

    // 根据游戏行为，可知 BlinkDisguise 负责重设"伪装即将被识破"的闪烁计时器：
    //  当该单位对玩家呈现为伪装身份、并且此刻展示的不是玩家自己的伪装外貌
    //  时，把传入时长写入闪烁计时器，让伪装在随后若干帧内周期性地闪现真身；
    //  其余情况下忽略这次写入。闪烁计时器随后由 IsDisguisedAgainst 读取。
    void BlinkDisguise(int32 duration);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 +0x518/+0x51C 这一对保存"当前伪装"的两个分量：
    //  前者是伪装所冒充的外观编号，后者是伪装所归属的房屋指针。
    // ------------------------------------------------------------------------
    int32         DisguiseTypeId;    // +0x518 冒充的外观编号
    HouseClass*   DisguiseHouse;     // +0x51C 伪装归属的房屋


    // ========================================================================
    // ★ 二轮语义深化：原版存在、OpenYR 上一轮缺口的入口
    //   （asm 引用见各函数在 .cpp 中的注释；行号取自
    //    /workspace/asm_export/TechnoClass.asm.txt，括号内另注 gamemd 全表行号）
    // ========================================================================

    // asm: TechnoClass::Limbo            (asm:7564)  —— 把对象从地图摘下
    virtual bool Limbo() override;
    // asm: TechnoClass::Unlimbo          (asm:7753)  —— 重新放置到地图上
    virtual bool Unlimbo() override;

    // ------------------------------------------------------------------------
    // asm: Scan_Place_Object (0x688F00, __fastcall(TechnoClass*, wXY*, int))
    //
    //  先试给定格本身，再自 mode 半径起按 8 个方向螺旋外扩（第 2 圈附加
    //  随机抖动），每格先过 In_Radar / 占位检查，再调 Put 落位。
    //  返回是否成功落位。
    // ------------------------------------------------------------------------
    bool Scan_Place_Object(const CellStruct& where, int32 mode);

    // asm: TechnoClass::Pip_Count        (gamemd:1588410) —— 单位小图标数量
    int32 Pip_Count() const;

    // ── 通信应答族（Response_*，gamemd:1588632..1589023） ─────────────────
    void Response_Capture();
    void Response_Deploy();
    void Response_Select();
    void Response_Move();
    void Response_Enter();
    void Response_Harvest();
    void Response_Unload();
    void Response_Fire(AbstractClass* pTarget);

    // asm: TechnoClass::Receive_Message  (asm:5857) —— 无线电指令接收
    //   返回应答码（0 = 未处理，1 = 已处理，0x0A/0x20/0x21 = 特定拒绝）
    int32 Receive_Message(TechnoClass* pSubject, int32 cmd, int32 val);

    // asm: TechnoClass::Per_Cell_Process (asm:6443) —— 逐格处理（a2==2 时生效）
    void Per_Cell_Process(int32 a2);

    // asm: TechnoClass::Assign_Target    (asm:11851) —— 分配/校验攻击目标
    void Assign_Target(AbstractClass* pNewTarget);

    // asm: TechnoClass::Player_Assign_Mission (asm:13529)
    //   玩家下达任务：语音应答 + 组包（规划模式）/ 直接入队
    bool Player_Assign_Mission(int32 mission, AbstractClass* pTarget,
                               AbstractClass* pDest, AbstractClass* a2);

    // asm: TechnoClass::Captured         (asm:15489) —— 被占领 / 心灵控制转移
    bool Captured(HouseClass* pNewOwner);

    // asm: TechnoClass::Can_Fire         (asm:11226..11782) —— 开火前置判定
    //   返回 FireError；pTarget 为空表示只做自身检查
    int32 Can_Fire(AbstractClass* pTarget, int32 idxWeapon, int32 a3);

    // asm: TechnoClass_Update 的前半段（gamemd:1561027..1561946）
    //   —— 染色/语音/暴走/老练/抽能/残余效果计时器。
    //   ★ 非虚：asm 里它并不是独立虚槽，只是 Update 的一段。TechnoClass::Update
    //     按 asm 顺序先调它，再在 L1561947 调 MissionClass::Update()。
    //   ★ 不要与 FootClass::AI 混淆：后者是足类逐帧主体（虚、void），
    //     同名但语义不同，TechnoClass::Update 内以静态绑定调用本函数。
    void AI();

    // ── 目标评估族 ────────────────────────────────────────────────────────
    // asm: TechnoClass::Evaluate_Object  (gamemd:1557048)
    //   7 参 __thiscall（retn 1Ch）：flags1, flags2, wantedDistance, target,
    //   threat(out), distance, tgtCoords
    bool Evaluate_Object(int32 flags1, int32 flags2, int32 wantedDistance,
                         TechnoClass* pTarget, int32* pThreat, int32 distance,
                         const CoordStruct* pTgtCoords);
    // asm: TechnoClass::Evaluate_Cell    (asm:9522)
    //   7 参（retn 1Ch）：flags1, flags2, coords, wantedDistance, buf1(out),
    //   threat(out), a8
    bool Evaluate_Cell(int32 flags1, int32 flags2, const CellStruct& coords,
                       int32 wantedDistance, TechnoClass** pBuf, int32* pThreat,
                       int32 a8);
    // asm: TechnoClass::Evaluate_Just_Cell (asm:9802)
    int32 Evaluate_Just_Cell(const CellStruct& coords);

    // 供 ShouldSuppress 复用的"压制估算"（asm:9226 的主体，返回 0..1 的缩放系数）
    double SuppressionEstimate(const CellStruct& center) const;

    virtual double GetStoragePercentage() const { return 0.0; }
    virtual int32 GetRefund() const { return 0; }
    virtual BulletClass* Fire(AbstractClass* pTarget, int32 nWeaponIndex);
    virtual bool IsClearlyVisibleTo(HouseClass* House) const { return true; }
    virtual bool IsControllable() const { return false; }
    virtual bool IsActive() const { return true; }
    virtual bool IsSelectable() const { return true; }
    virtual bool CanBeSelected() const { return true; }
    virtual bool CanBeSelectedNow() const { return true; }

    // ========================================================================
    // 原版虚槽（Damage_At_Cell_Update_489280 通过 vtable 调用）
    //
    //   +0x160  原版 call [vt+160h]：返回 bool 的谓词，用于判定对象是否
    //           参与震屏/特效（原版在 noShake 链与伤害循环中调用）。
    //   +0x54   原版 call [vt+54h]：返回 bool 的谓词，用于飞机类的
    //           距离减半判定。
    //
    //   这两个槽在原版中属于 TechnoClass 子类族各自的 override；基类提供
    //   保守默认实现（false），与"未特化类型不参与"的原版语义一致。
    // ========================================================================
    virtual bool IsShakeParticipant() const { return false; }   // vt+0x160
    virtual bool HalvesDistanceWhenHit() const { return false; } // vt+0x54

    // 原版 vt+0x88：返回本对象的类型定义（TechnoTypeClass*）。
    // Damage_At_Cell 用它参与 Rules 免疫列表比对。
    TechnoTypeClass* GetType() const { return TechnoType; }

    // 原版 vt+0x3D8：让本对象执行一次震屏/受击特效，anchor 为特效锚点，
    // strength 为强度（原版传入 flash 值的单精度结果）。
    virtual void Shake(const CoordStruct& anchor, float strength) {
        (void)anchor; (void)strength;
    }

    // ========================================================================
    // Iron Curtain / Force Shield / damage-effect state
    //
    // The Iron Curtain and Force Shield super weapons grant absolute
    // invulnerability for a bounded number of frames. The damage-effect
    // timers track the secondary warhead effects (fire, sparks, parasite,
    // temporal freeze, gas, radiation) that ApplyToTechno attaches to a
    // target after the primary damage has been resolved.
    // ========================================================================
    bool IsIronCurtained() const { return IronCurtainTimer > 0; }
    bool IsForceShielded() const { return ForceShieldTimer > 0; }
    bool IsShielded() const { return IronCurtainTimer > 0 || ForceShieldTimer > 0; }

    void ApplyIronCurtain(int32 frames) { if (frames > IronCurtainTimer) IronCurtainTimer = frames; }
    void ApplyForceShield(int32 frames) { if (frames > ForceShieldTimer) ForceShieldTimer = frames; }

    bool IsOnFire() const { return FireDamageTimer > 0; }
    void SetOnFire(int32 frames) { if (frames > FireDamageTimer) FireDamageTimer = frames; }

    bool IsSparky() const { return SparkyCounter > 0; }
    void SetSparky(int32 count) { SparkyCounter += count; }

    bool IsParasiteAttached() const { return IsParasited; }
    void SetParasite() { IsParasited = true; }

    bool IsTemporalized() const { return TemporalTimer > 0; }
    void SetTemporal(int32 frames) { if (frames > TemporalTimer) TemporalTimer = frames; }

 // 根据游戏行为，可知 Reload 负责下面这段逻辑。
    //
    //  Ticks the ammo counter up by one once the reload timer has run out and
    //  the magazine is not yet full.  A type with Ammo == -1 never reloads
    //  (infinite magazine); a type with a finite magazine whose timer is
    //  still running is left alone.  On a successful reload the techno is
    //  Mark()ed (ground layer) and Techno_Update_Reloading restarts the timer
    //  for the next round.
    void Reload();

 // Techno_Update_Reloading: recomputes and restarts the
    //  reload delay after a round has been loaded.
    void Update_Reloading();

 // TechnoClass_StartAirstrikeTimer / _StopAirstrikeTimer
 //: arm / disarm the frame window during which a follow-up
    //  airstrike may be requested.  Start also zeroes the generation counter.
    void StartAirstrikeTimer(int32 duration);
    void StopAirstrikeTimer();

    bool IsGassed() const { return GasTimer > 0; }
    void SetGas(int32 frames) { if (frames > GasTimer) GasTimer = frames; }

    bool IsIrradiated() const { return RadiationTimer > 0; }
    void SetRadiation(int32 frames) { if (frames > RadiationTimer) RadiationTimer = frames; }

    int32 GetLastFireFrame() const { return LastFireFrame; }
    void SetLastFireFrame(int32 frame) { LastFireFrame = frame; }

    // ========================================================================
    // State (TechnoClass-specific)
    // ========================================================================
    TechnoTypeClass* TechnoType;        // back-pointer to this techno's type definition
    int32         Health;
    int32         MaxHealth;
    int32         VeterancyLevel;     // 0=Rookie, 1=Veteran, 2=Elite
    int32         Experience;

    // asm: _TechnoClass.t.ArmorMultiplier
    //   TechnoClass_ReceiveDamage 用它缩放进伤害：
    //   Damage /= House->GetTypeArmorMult(type) * this->ArmorMultiplier
    //   默认 1.0；MOD / 特殊状态可调整（如步兵掩体）。
    double        ArmorMultiplier;

    CloakStateEnum CloakState;
    uint8         CloakAlpha;         // 255 = fully visible, 0 = fully cloaked
    int32         FireRechargeTimer;  // frames remaining before next shot
    int32         CloakTimer;         // frames remaining in current cloak state
    bool          RepairActive;       // true while a service depot is healing us
    int32         RepairRate;         // HP per frame while being repaired

    // Iron Curtain / Force Shield invulnerability timers (frames remaining).
    int32         IronCurtainTimer;
    int32         ForceShieldTimer;

    // 根据游戏行为，可知钢铁化与空袭都会给单位叠上一层随时间变化的染色：
    // 计时器记录这一段染色的剩余帧数，阶段号记录当前处在染色曲线的哪一段
    // （上色、保持、褪色……）。绘制时按阶段与剩余帧数算出调制系数，再乘到
    // 本体颜色上，于是钢铁单位泛蓝、被空袭锁定的单位泛红。
    int32         IronTintTimer;
    int32         IronTintStage;
    int32         AirstrikeTintTimer;
    int32         AirstrikeTintStage;

    // Frame stamp of the last successful weapon discharge (Game::CurrentFrame).
    // Used by Fire_Impl to gate firing on the weapon's rate of fire.
    int32         LastFireFrame;

    // ── asm: _TechnoClass.t.FiringWave ────────────────────────────────────
    // 根据游戏行为，可知音波武器与磁能波武器开火时不发射实体弹丸，而是
    // 生成一条随帧向外推进的"波"（WaveClass）：音波炮生声波束、磁暴线圈
    // 与磁电坦克生磁能束。这一槽保存本对象当前持有的那条波，同一时刻只
    // 允许一条；旧波未散尽时再次开火会先把旧波顶掉。
    // asm: TechnoClass_Fire 里 `mov [this+_TechnoClass.t.FiringWave], eax`。
    WaveClass*    FiringWave;

    // Layer flag at +0x74: true while this object occupies a tunnel, bridge or
    // similar non-ground layer.  Consulted by IsOnFloor / IsInAir.
    bool          Tunnel;
    uint8         pad_Tunnel[3];

    // Turret-slot bookkeeping.  ActiveTurretIndex addresses +0x124 (the slot
    // the unit is currently drawn with) and CurrentWeaponNumber addresses
    // +0x134; -1 means "no weapon selected".
    int32         ActiveTurretIndex;
    int32         CurrentWeaponNumber;

    // House that owned this object before it was captured.  Only meaningful
    // while IsCapturedFlag is set.  (asm +0x2E0)
    HouseClass*   OrigOwner;
    bool          IsCapturedFlag;

    // ── Disguise / warp / drain state ─────────────────────────────────────
    bool          IsDisguisedFlag;    // +0x81 style disguise flag
    int32         WarpInTimer;        // remaining frames of a chrono-warp-in
    int32         WarpOutTimer;       // remaining frames of a chrono-warp-out
    bool          IsWarpingOutFlag;   // warp-out has completed
    int32         DrainTimer;         // remaining frames of a drain effect

    // Disguise blinking.  `DisguiseCreationFrame` (+0x1DC) remembers when the
    // disguise was taken; `DisguiseBlinkTimer` (+0x1EC / +0x1F4) drives the
    // periodic flicker that tips the player off.  GetDisguiseFlags and
    // IsDisguisedAgainst read both.
    int32         DisguiseCreationFrame;
    CDTimerClass  DisguiseBlinkTimer;

    // Planning-token slot: -1 when the object has no waypoint assigned.
    int32         PlanningToken;

    // Multi-stage weapon counter at +0x140 (gattling / prism style weapons).
    int32         WeaponStage;

    // 根据游戏行为，可知 WeaponStageFrame 记录射速档位最近一次变化的帧，
    //  用于判断升档/降档是否已经到点。
    int32         WeaponStageFrame;

    // ── 炮管后座状态 ──────────────────────────────────────────────────────
    // 根据游戏行为，可知开火瞬间后座拉满（RecoilAmount 为正），随后按帧
    //  复位；RecoilStartFrame 记录后座开始的那一帧。
    int32         RecoilAmount;
    int32         RecoilStartFrame;

    // ── 死亡记账 ──────────────────────────────────────────────────────────
    // 根据游戏行为，可知死亡判重靠 IsDyingNow：Die 進来先看它，置位后重复
    //  调用直接返回；KilledBy 记"谁杀的"，供经验与战果统计使用。
    bool          IsDyingNow;
    int32         field_1C4;           // +0x1C4  原版未命名字段（震屏 gate 检查）
    TechnoClass*  KilledBy;

    // ── 说话气泡状态（占位，不再承载逻辑） ────────────────────────────────
    //  原版的话泡状态是**全局**的（TalkBubbleType / Owner / Timer，见
    //  Game/Externs.h），不是每个 Techno 各存一份。这四个字段是早期实现留下
    //  的逐单位模型，现已无任何读写点；保留它们只是为了不改动 TechnoClass
    //  的对象布局（布局一变，存档偏移与既有 CRC 全部错位）。
    //  新代码一律走 TechnoClass::DisplayTalkBubble(int32)。
    bool          IsTalking;
    const wchar_t* TalkBubbleText;
    int32         TalkBubbleEnd;
    int32         LastTalkFrame;

    // 根据游戏行为，可知行军规划令牌除了槽位编号，还要记住该令牌指向的目的
    //  地，单位每帧据此决定下一步往哪走。
    CoordStruct   PlanningDestination;

    // ── 冒烟表现状态 ──────────────────────────────────────────────────────
    // 根据游戏行为，可知建筑受损后冒烟系统被拉起，SmokeSystemStage 表示烟的
    //  浓淡档位；完好时 SmokeSystemActive 复位。
    bool          SmokeSystemActive;
    int32         SmokeSystemStage;

    // 根据游戏行为，可知规划令牌的槽位编号由全局计数器统一分配，保证同一
    //  时刻不会有两条规划撞到同一个槽位。
    static int32  sNextPlanningToken;

    // The building this techno is currently focused on (asm FocusOnUnit).
    BuildingClass* FocusOnUnit;

    // Primary facing (+0x388).  Where the object points.  FootClass keeps its
    // own copy for the locomotor; this is the location the binary's
    // Desired_Facing256 reads for structures and turreted units.
    DirStruct     PrimaryFacing;

    // Height used by the on-floor / in-air tests (+0x68 in the original's
    // type block, mirrored here).  Zero for ground vehicles and structures.
    int32         GroundHeight;

    // Index of a voice line queued for playback, or -1.
    int32         QueuedVoiceIndex;

    // The locomotor COM object driving this techno's every-frame movement
    // (asm +0x674).  May be null for statics such as buildings.
    ILocomotion*  Locomotor;

    // Secondary warhead-effect state. These timers/counters are decremented
    // by Update_AI each frame and consulted by the damage / rendering code.
    int32         FireDamageTimer;    // frames remaining while burning
    int32         SparkyCounter;      // pending spark particle spawns
    bool          IsParasited;        // a parasite is attached to this techno
    int32         TemporalTimer;      // frames frozen by the chronosphere weapon

    // The temporal weapon object this techno is currently applying to a victim
    // (asm _TechnoClass.t.TemporalImUsing, +0x274, points at a TemporalClass),
    // or null.  TechnoClass_Fire/AI assign it; TemporalClass::Fire reads it
    // back when a second chrono weapon tries to lock onto the same target.
    TemporalClass* TemporalImUsing;

    // The TemporalClass that is currently warping *this* techno out of the
    // world (asm _TechnoClass.t.TemporalTargettingMe, +0x278).  Set by
    // TemporalClass::Fire and cleared by TemporalClass::Detach / Update.
    TemporalClass* TemporalTargettingMe;

    // ── asm: _TechnoClass.t.CaptureManager (+0x2BC) ───────────────────────
    // 根据游戏行为，可知具备心灵控制能力的单位（尤里、心灵控制塔等）在
    //  Init 时被装配一个 CaptureManagerClass，用来记录"我控制了谁"。TemporalClass
    //  的时间抹除与建筑摧毁时会通过它做连锁释放（KillNodes / ReleaseAll）。
    CaptureManagerClass* CaptureManager;

    // ── 附属实体族（SpawnManager/SlaveManager/Parasite/Bomb）─────────────
    // asm: _TechnoClass.t.SpawnManager / SlaveManager / Parasite / Bomb
    //   四个附属对象以成员指针挂在 TechnoClass 上，由 TechnoClass_Init 按
    //   类型装配、析构时释放；各自在 vec_SpawnManagers / vec_SlaveManagers /
    //   vec_Parasites / vec_BombList 里登记。
    SpawnManagerClass*   SpawnManager;
    SlaveManagerClass*   SlaveManager;

    // asm: _TechnoClass.t.SlaveOwner (+0x2DC) —— 反向链接：奴隶兵指向
    //   拥有它的那台奴隶矿车。SlaveManagerClass_CTOR 在创建奴隶时写
    //   `[slave+2DCh] = [manager+24h]`；LoseSlave / ReleaseSlaves 清零。
    //   非空表示"我是别人的奴隶"，不能接受玩家下达的目的地/航点。
    TechnoClass*         SlaveOwner;
    ParasiteClass*       Parasite;
    BombClass*           Bomb;

    // ── 寄生进出宿主的簿记（asm +0x328 / +0x432 / +0x6A0）────────────────
    // 原版 Parasite_ExitUnit 收尾写 [host+328h]=0，成功路径按
    // [controller+432h] 判定"是否要被玩家选中"，并把 {Frame, 备用槽, 时长}
    // 三元组写到 controller/host 的 +0x6A0。这三个槽 IDA 未命名，这里按
    // 真实成员落地，避免裸偏移访问。
    int32         field_328;             // +0x328 宿主侧的挂接标记
    int32         field_432;             // +0x432 寄生体侧"待选中"标记
    int32         ParasiteStampFrame;    // +0x6A0 挂接/脱离的帧号
    int32         ParasiteStampSpare;    // +0x6A4 备用槽（原版读未初始化栈槽）
    int32         ParasiteStampDuration; // +0x6A8 时长（成功路径 = 3 * 武器 ROF）

    // asm: _TechnoClass.t.IsBeingWarpedOut byte (+0x270).  Distinct from the
    // generic WarpOutTimer countdown: the chrono legionnaire's temporal weapon
    // sets this byte directly when it starts erasing a target.
    bool          IsBeingWarpedOutFlag;
    int32         GasTimer;           // frames affected by gas
    int32         RadiationTimer;     // frames irradiated by a rad warhead

    // ========================================================================
    // Ammunition & reloading (asm currentAmmo / ReloadTimer)
    // ========================================================================
    // CurrentAmmo (+0x[+], asm TechnoClass.currentAmmo): rounds remaining.
    //   Reload adds one round once ReloadTimer has elapsed, up to the type's
    //   Ammo ceiling (-1 = infinite, in which case Reload does nothing).
    int32         CurrentAmmo;
    CDTimerClass  ReloadTimer;

    // ========================================================================
    // Airstrike timer (asm TechnoClass.StartAirstrikeTimer / StopAirstrikeTimer)
    // ========================================================================
    // The airstrike window is a plain frame stamp plus a generation counter,
    // touched by the two helpers below rather than by a real countdown.
    int32         AirstrikeTimeStart;
    int32         AirstrikeTimeLeft;
    int32         AirstrikeTimeGen;

    // ========================================================================
    // ★ 二轮语义深化新增状态（仅补原版确有、此前未建模的槽位）
    // ========================================================================

    // asm +0x2B8 TechnoClass.LastTarget —— Assign_Target 以它做"目标是否变化"的
    //  比较基准（Target 在 +0x2B4，两槽相邻）。注意 +0x508 是 ThreatPosed，
    //  Limbo 在离开地图前对它做 Adjust_Threat(-ThreatPosed) 后清零。
    AbstractClass* LastTarget;
    // asm +0x50C —— Assign_Target 入口清零的锁定标志。
    bool           TargetLockActive;

    // asm _TechnoClass.t.ThreatPosed —— 本单位向所在格贡献的威胁值。
    int32          ThreatPosed;

    // asm +0x418 / +0x419 —— Receive_Message 维护的无线电信道握手标志。
    bool           RadioFlag418;
    bool           RadioFlag419;

    // asm +0x2FC —— 整备补弹计数（Receive_Message case 28 逐次自增）。
    int32          RearmCount;

    // asm +0x1C8  _TechnoClass.t.Deactivated —— Deactivate 置 1、Activate 清 0
    //   的"本帧不可指挥"字节。TechnoClass_9C / CanSetWaypoint / Response_Select
    //   都会查它。注意：Berzerk 是另一个字段，本树早期把两者混用，现已分开。
    bool           Deactivated;

    // asm _TechnoClass.t.EMPDurationRemaining —— EMP 脉冲打上的瘫痪帧数。
    //   EMPulseClass_Initialize 写入，TechnoClass_Update 每帧递减；>0 时
    //   vt+0x37C（IsUnderEMP）返回真。
    int32          EMPDurationRemaining;

    // asm +0x1C8 —— EMP / 狂化状态。本树用 IsDizzy 表示"狂化"（Berzerk）；
    //   真正的 Deactivate 字节已拆到上面的 Deactivated。
    bool           IsDizzy;

    // asm _TechnoClass.t.BehindAnim (+0x128) —— "身后层"动画，由
    //   TechnoClass::DrawHidden 在 Rules->Behind 存在时 new 出并 Attach_To(this)。
    // asm _TechnoClass.t.DeployingAnim (+0x12C) —— 展开/收起过程中的过渡动画；
    //   DrawHidden 入口以它非空为"别画隐身影"的门控。
    //   两个槽在二进制里紧邻 CurrentWeaponNumber 之后；本树把它们放在类尾，
    //   因为工程没有按字节偏移存取的代码（无 reinterpret_cast<TechnoClass*>(+n)）。
    AnimClass*      BehindAnim;
    AnimClass*      DeployingAnim;

protected:
    explicit TechnoClass(noinit_t) noexcept : RadioClass(noinit) {}
};
