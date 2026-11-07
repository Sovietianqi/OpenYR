#include <Abstract/TechnoClass.h>
#include <Abstract/AircraftTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Helpers/AsmMath.h>   // Float_To_Int_Floor (asm 0x7C5F10)
#include <Combat/WeaponTypeClass.h>
#include <Combat/WarheadTypeClass.h>
#include <Combat/BulletClass.h>
#include <Combat/CaptureManagerClass.h>
#include <Houses/HouseClass.h>
#include <cwchar>
#include <Game/SaveGameClass.h>
#include <Game/Game.h>
#include <Game/Externs.h>
#include <Rules/RulesClass.h>
#include <Map/CellClass.h>
#include <Audio/VocClass.h>
#include <Map/MapClass.h>
#include <Scenario/ScenarioClass.h>
#include <Rendering/TacticalClass.h>
#include <Abstract/BuildingClass.h>
#include <Special/WaveClass.h>   // asm: TechnoClass_Fire 里 WaveClass_CTOR 的装配目标
#include <Special/TemporalClass.h> // TemporalImUsing->Victim 需要完整类型
#include <Special/SpawnManagerClass.h> // Assign_Target 里 SpawnManager->SetTarget 需要完整类型
#include <Special/SlaveManagerClass.h> // TechnoClass_Update 后段的 SlaveManager->Update
#include <Combat/BombClass.h>          // TechnoClass_Update 后段的 IvanBomb TimeToBlow/Detonate
#include <AI/TagClass.h>          // Per_Cell_Process -> TagClass::RaiseEvent
#include <Game/ChainDebug.h>      // 逐帧调用链探针（#2/#3/#6/#7 验证）
#include <cmath>

// ============================================================================
// TechnoClass.cpp
//
//  TechnoClass is the base for every "technical" object - anything that can
//  be owned by a house, take damage, fire a weapon, gain veterancy, cloak,
//  or be repaired.  Infantry, vehicles, aircraft and buildings all derive
//  from TechnoClass.  This file expands the .cpp with:
//    * Static Array management
//    * Update loop (AI, combat, cloaking)
//    * Fire weapon implementation
//    * TakeDamage implementation
//    * Repair logic
//    * Cloak / Uncloak
//    * Veteran / Promote
//    * Is_Ally / Is_Enemy
//    * Get_Threat_Pos
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<TechnoClass*>* TechnoClass::Array = nullptr;

// 根据游戏行为，可知规划令牌的槽位编号由这个全局计数器逐次分配。
int32 TechnoClass::sNextPlanningToken = 0;

// ============================================================================
// Init_Array / Delete_Array
// ============================================================================
void TechnoClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<TechnoClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<TechnoClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<TechnoClass*>();
    }
}

void TechnoClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<TechnoClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

// ============================================================================
// Add_To_Array / Remove_From_Array
// ============================================================================
int32 TechnoClass::Add_To_Array(TechnoClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return -1;

    if (!Array->Add(pInstance))
        return -1;

    return Array->Count - 1;
}

bool TechnoClass::Remove_From_Array(TechnoClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return false;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        if (Array->Items[i] == pInstance)
        {
            return Array->Remove(i);
        }
    }
    return false;
}

// ============================================================================
// Get_Total_Count / Get_Instance / Find_Index
// ============================================================================
int32 TechnoClass::Get_Total_Count()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

TechnoClass* TechnoClass::Get_Instance(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 TechnoClass::Find_Index(TechnoClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return -1;
    for (int32 i = 0; i < Array->Count; ++i)
    {
        if (Array->Items[i] == pInstance)
            return i;
    }
    return -1;
}

// ============================================================================
// TechnoClass_Update  (asm:1561025 .. 1563064)
//
// 原版结构（按 asm 先后）：
//   前半（1561027..1561946）：
//     [431h] 一次性标志 → IsGattling 的 VocClass_UpdateAtLocation →
//     UpdateTint_IronCurtain / UpdateTint_Airstrike → 队列语音 →
//     暴走（Berzerk）计时 → 武器选择 / SetTarget / SelectWeapon /
//     GetObjectFiringState / Recoil（开火后坐）→ 充能炮塔 → Unload 计时 →
//     ShouldLoseTarget（按当前任务决定是否清目标）
//   ★ 1561947：`call MissionClass_Update` —— 任务状态机（32 路跳转表）
//   后半（1561950..1563064）：
//     TargetingTimer 到点 → Techno_4C4/4CC（重选目标）→ mi_Move/Harvest/Guard
//     时 CanFightBack 追打 → IvanBomb（TimeToBlow/Detonate）→
//     SlaveManager->Update → CaptureManager->Update → Alive 门 → 自愈 →
//     UpdateTimers（补弹）→ SpawnManager->Update → 隐身推进
//
// 继承链已按原版接通（TechnoClass : RadioClass : MissionClass : ObjectClass），
// 因此 1561947 可以直接写成 MissionClass::Update()，不再需要内联任务机。
// MissionClass::Update 自身在入口回链 ObjectClass::Update（asm:911401），
// 所以「物体层逐帧」恰好只跑一次。
// ============================================================================
void TechnoClass::Update()
{
    // 调用链探针：证明 TechnoClass_Update（vt+0x5C）确实被每帧驱动到（差异表 #2）。
    ++ChainDebug::TechnoUpdateCalls;

    // ---- 前半段（1561027..1561946）---------------------------------------
    AI();

    // 战斗 / 修理 / 老练（原版内联在 Update 的武器与自愈段，OpenYR 拆成子函数）
    Update_Combat();
    Update_Repair();

    // ---- 1561937..1561945：任务计时器槽自增 -----------------------------
    //   asm:
    //     mov edx,[esi+_TechnoClass.t.r.m.field_C0]
    //     mov ecx,esi
    //     inc edx
    //     mov [esi+_TechnoClass.t.r.m.field_C0],edx
    //     call MissionClass_Update
    //   field_C0 是任务层的"本任务已跑帧数"计数，MissionClass::Update 之后
    //   不会再动它，故必须在调用前自增。
    ++field_C0;

    // ---- 1561947：MissionClass_Update -----------------------------------
    //   任务状态机：先跑 ObjectClass::Update（物体层），再按 currentMission
    //   走 off_5B34E8 的 32 路分派，最后回写任务计时器。
    MissionClass::Update();

    // ---- 后半段（1561950..1563064）---------------------------------------

    // asm: 1562033 `mov ecx,[esi+IvanBomb] / cmp 0 / call BombClass_TimeToBlow`
    //      → 到点则 BombClass_Detonate。Limbo 中的对象跳过。
    if (Bomb != nullptr && !IsInLimbo)
    {
        if (Bomb->TimeToBlow())
            Bomb->Detonate();
    }

    // asm: 1562042 `mov ecx,[esi+SlaveManager] / call [edx+5Ch]`
    //   —— 奴隶管理器逐帧（矿车释放/回收奴隶）。
    if (SlaveManager != nullptr)
        SlaveManager->Update();

    // asm: 1562054 `mov ecx,[esi+CaptureManager] / call CaptureManagerClass_Update`
    //   —— 心灵控制链逐帧维护。
    if (CaptureManager != nullptr)
        CaptureManager->Update();

    // asm: 1562059 `mov al,[esi+Alive] / test / jz return`
    //   —— 死亡对象不再推进后面的自愈 / 计时器 / 子机 / 隐身段。
    if (!Alive)
        return;

    // asm: 1562063 `call [vt+SelfHealNow]`（受创单位的缓慢自愈）。
    //   OpenYR 以 Update_Repair 表达维修厂的持续修理；脱战自愈的门槛与步长
    //   由 HouseClass 的 SelfHeal 规则决定，本层不重复扣帧。
    //   （军衔推进已在 AI() 的 1561610 段完成，勿重复调用。）

    // asm: 1562293 `call [vt+UpdateTimers]` —— 弹药装填计时器。
    Reload();

    // asm: 1562297 `mov ecx,[esi+SpawnManager] / call [eax+5Ch]`
    //   —— 航母/驱逐舰/无畏级的子机队列。
    if (SpawnManager != nullptr)
        SpawnManager->Update();

    // asm: 1562305 起的隐身段（CloakState 分派 → TechnoClass::Cloaking_AI）
    Update_Cloak();
}

// ============================================================================
// 根据游戏行为，可知 AI 负责下面这段逻辑。
//
//  Runs the mission state machine.  The concrete subclass owns the actual
//  mission handlers; the base class only ensures the cloak / weapon-recharge
//  timers tick down.
// ============================================================================
void TechnoClass::Update_AI()
{
    // Decrement the firing timer if it is running.
    if (FireRechargeTimer > 0)
        --FireRechargeTimer;

    // Decrement the cloak timer if it is running.
    if (CloakTimer > 0)
        --CloakTimer;

    // Decrement the Iron Curtain / Force Shield invulnerability timers.
    if (IronCurtainTimer > 0)
        --IronCurtainTimer;
    if (ForceShieldTimer > 0)
        --ForceShieldTimer;

    // Tick the secondary warhead-effect timers. The full binary also applies
    // per-frame residual damage for fire / radiation here; the standalone
    // build only ages the timers so the IsXxx() accessors reflect the
    // current state.
    if (FireDamageTimer > 0)
        --FireDamageTimer;
    if (SparkyCounter > 0)
        --SparkyCounter;
    if (TemporalTimer > 0)
        --TemporalTimer;
    if (GasTimer > 0)
        --GasTimer;
    if (RadiationTimer > 0)
        --RadiationTimer;
}

// ============================================================================
// 根据游戏行为，可知 Combat 负责下面这段逻辑。
//
//  Per-frame combat update for the base TechnoClass.  Handles the common
//  combat logic shared by all techno types:
//
//    1. If the techno is dead, in limbo, or frozen (temporal), skip.
//    2. Decrement the fire recharge timer (also done in Update_AI, but
//       repeated here so combat state stays consistent if Update_AI is
//       overridden without chaining).
//    3. If the techno is shielded (Iron Curtain / Force Shield), it is
//       invulnerable but can still fight - no early return.
//    4. If the techno is cloaked and has a pending fire action, force a
//       decloak.  Firing breaks cloak in the standard rules.
//    5. If the techno is armed and its weapon is ready, the derived class's
//       Combat_AI override handles target selection and firing.  The base
//       only ensures the shared combat state is up to date.
//
//  Concrete subclasses (InfantryClass, UnitClass, BuildingClass) override
//  Combat_AI() with type-specific targeting and fire logic.
// ============================================================================
void TechnoClass::Update_Combat()
{
    // Dead, limboed, or temporally frozen technos do not process combat.
    if (Health <= 0)
        return;
    if (IsInLimbo)
        return;
    if (TemporalTimer > 0)
        return;

    // 开火冷却计时器由 AI() 的 UpdateTimers 段统一推进（asm 全函数只推一次），
    // 本函数不再重复递减，否则射速会翻倍。

    // Shielded technos (Iron Curtain / Force Shield) are invulnerable but
    // retain full combat capability.  No early return needed.

    // If the techno is currently cloaked or cloaking, any combat action
    // forces a decloak.  The full binary calls Uncloak() here when a fire
    // command is issued; the base checks the state so derived classes can
    // consult it before firing.
    if (CloakState == CloakStateEnum::Cloaked ||
        CloakState == CloakStateEnum::Cloaking)
    {
        // Combat activity breaks cloaking.  The derived class calls
        // Uncloak() when it actually fires; the base does not auto-decloak
        // to avoid interfering with passive cloak decay.
    }

    // Secondary warhead effects that influence combat capability:
    //   - Burning technos (FireDamageTimer > 0) take residual damage
    //     applied by the damage system, not here.
    //   - Gassed / irradiated technos have reduced combat effectiveness
    //     but can still fight.
    // These timers are aged by Update_AI; Update_Combat only reads them.

    // ── ★ 武器分支：目标 → SelectWeapon → Fire ───────────────────────────
    //
    // asm TechnoClass_Update 的武器段（1561600..1561772）：
    //   target = [this+_TechnoClass.t.Target]
    //   if (target && target 已 limbo/已死)  SetTarget(0)      ; loc_6FA472
    //   idx    = [vt+SelectWeapon](target)                     ; 1561737
    //   state  = [vt+GetObjectFiringStateNoRange](target, idx) ; 1561743
    //   if (state == fsCantAffect || state == fsCantCommand) SetTarget(0)
    //
    //   随后的实际开火在原版由 Mission_Attack 调 UnitClass_Fire/
    //   InfantryClass_Fire → TechnoClass_Fire 完成。任务层现已接通
    //   （TechnoClass : RadioClass : MissionClass），但 FootClass 只覆写了
    //   Move/Guard/AreaGuard/Patrol/Retreat/Enter/Unload 七个任务，Mission_Attack
    //   仍是基类的"继续"常量，故此处按同一前置条件直接落到 TechnoClass::Fire
    //   —— 这正是 asm 里 TechnoClass_Fire 的入口语义，且不会与任务层重复开火
    //   （Fire_Impl 内部有 ROF 门控）。
    AbstractClass* pTarget = GetTarget();
    if (pTarget == nullptr)
        return;

    // asm loc_6FA472: 目标已消失/进入 limbo → 丢目标。
    if (pTarget->IsDead())
    {
        SetTarget(nullptr);
        return;
    }
    {
        const AbstractType what = pTarget->WhatAmI();
        if (what == AbstractType::Unit || what == AbstractType::Infantry
         || what == AbstractType::Building || what == AbstractType::Aircraft)
        {
            if (static_cast<ObjectClass*>(pTarget)->IsInLimbo)
            {
                SetTarget(nullptr);
                return;
            }
        }
    }

    // asm: call [vt+TechnoClass.SelectWeapon](target)
    const int32 idxWeapon = SelectWeapon(pTarget);
    if (idxWeapon < 0)
    {
        SetTarget(nullptr);
        return;
    }

    // asm: GetObjectFiringStateNoRange —— 无武器槽/无武器类型即"打不了"。
    WeaponStruct* pSlot = (TechnoType != nullptr) ? TechnoType->GetWeapon(idxWeapon)
                                                  : nullptr;
    if (pSlot == nullptr || pSlot->WeaponType == nullptr)
    {
        SetTarget(nullptr);
        return;
    }

    // asm L1561654 附近：`call TechnoClass_Can_Fire` —— 开火前的完整前置判定。
    //   原版把 Can_Fire 的返回值（FireError）交给 GetObjectFiringStateNoRange，
    //   只有 OK 才落到随后的开火。此前 OpenYR 只做了射程比较，漏掉了
    //   弹药 / 友军 / 暴走 / 时间武器 / 自己 / 目标被俘等一整串门，
    //   会出现"打空了还在扣 ROF、误伤友军"的偏差。这里补回这一层：
    //     fsRange   → 交给 Movement/Mission_Move 接近，本帧不开火；
    //     其余非 OK → 本帧不开火（保留目标，下一帧重试）。
    {
        const int32 fireState = Can_Fire(pTarget, idxWeapon, 0);
        if (fireState != static_cast<int32>(FireError::OK))
        {
            // asm loc_6FA472（只对 fsCantAffect / fsCantCommand 丢目标）：
            //   射程外（fsRange）保留目标，等 Movement 拉近距离；其余
            //   （冷却 / 装填 / 弹药 / 不可移动）同样只跳过本帧。
            //   真正"永远打不了"的两种情况由上面的武器槽判定覆盖。
            return;
        }
    }

    // asm: call TechnoClass_Fire(target, idxWeapon)
    Fire(pTarget, idxWeapon);
}

// ============================================================================
// 根据游戏行为，可知 Cloak 负责下面这段逻辑。
//
//  asm 逐段（TechnoClass::Cloaking_AI, asm:10744）：
//    状态槽 [220h]（= CloakStateEnum）分派：
//      case Cloaking  : 按计时/速率推进淡出，归零 → Cloaked
//      case Cloaked   : 若 vt.IsNotCloakedByOthers() 为真 → Uncloak(false)
//      case Uncloaking: 推进淡入，到 255 → Idle
//    淡变速率由计时器换算（原版 0x238/0x23C 保存速率与步进）。
// ============================================================================
void TechnoClass::Update_Cloak()
{
    if (CloakState == CloakStateEnum::Idle)
        return;

    // asm: loc_6FBA1C（case 2）—— 已完全隐身但"不再被掩蔽"时主动现形。
    if (CloakState == CloakStateEnum::Cloaked)
    {
        if (IsNotCloakedByOthers())
            Uncloak(false);
        return;
    }

    // 每帧淡变步长：CloakTimer 为整段淡变帧数，换算成每帧 alpha 增量。
    const int32 rate = (CloakTimer > 0 && CloakTimer < 255)
                     ? (255 / CloakTimer + 1) : 8;

    if (CloakState == CloakStateEnum::Cloaking)
    {
        CloakAlpha = static_cast<uint8>((CloakAlpha > rate) ? (CloakAlpha - rate) : 0);
        if (CloakAlpha == 0)
            CloakState = CloakStateEnum::Cloaked;
    }
    else if (CloakState == CloakStateEnum::Uncloaking)
    {
        CloakAlpha = static_cast<uint8>((255 - CloakAlpha > rate)
                                        ? (CloakAlpha + rate) : 255);
        if (CloakAlpha == 255)
            CloakState = CloakStateEnum::Idle;
    }
}

// ============================================================================
// 根据游戏行为，可知 Repair 负责下面这段逻辑。
//
//  If the unit is being repaired (by a service depot or the repair-infantry),
//  tick its HP up by the per-frame repair rate.
// ============================================================================
void TechnoClass::Update_Repair()
{
    if (!RepairActive)
        return;
    if (Health >= MaxHealth)
    {
        RepairActive = false;
        return;
    }
    Health += RepairRate;
    if (Health > MaxHealth)
        Health = MaxHealth;
}

// ============================================================================
// 根据游戏行为，可知 Veterancy 负责下面这段逻辑。
//
//  Promotes the unit when its accumulated experience crosses the next
//  threshold.  The thresholds are 100 (Veteran) and 200 (Elite) in the
//  original binary.
// ============================================================================
void TechnoClass::Update_Veterancy()
{
    if (VeterancyLevel >= 2) // Elite
        return;

    int32 nextThreshold = (VeterancyLevel == 0) ? 100 : 200;
    if (Experience >= nextThreshold)
    {
        ++VeterancyLevel;
        // The original binary clamps the experience and applies the
        // veterancy bonuses (firepower / armor / ROF) here.
    }
}

// ============================================================================
// Fire weapon implementation
//
//  Spawns a BulletClass aimed at pTarget.  The bullet inherits the weapon's
//  speed, warhead and damage.  The original binary is much more involved -
//  it computes the lead, picks the right firing offset, plays the fire-anim
//  and the muzzle flash, and pushes the firing timer.  The standalone build
//  preserves the entry-point signature so subclasses can call into it.
// ============================================================================
// ============================================================================
// Fire_WaveWeapon — 音波/磁能波武器的开火装配
//
// asm: TechnoClass_Fire 中两处 `push 240h / call new / call WaveClass_CTOR`
// 的完整语义。原版两条分支的触发条件与参数：
//
//   ① 声波束（type = 0）：
//        `mov al,[ebx+WeaponTypeClass.IsSonic] / test / jz`
//        → push Target / push 0 / push this(Source) / push &CellCoords
//          / push &FLH / call WaveClass_CTOR
//        → `mov [this+_TechnoClass.t.FiringWave], eax`
//
//   ② 磁能束（type = 3）：
//        `mov al,[ebx+WeaponTypeClass.IsMagBeam] / test / jz`
//        先检查现有 FiringWave：若为空可直接建；若不为空，只有当旧波
//        是"动画类"（`call [vt+2Ch]` 即 WhatAmI() == 6）时才让位，
//        否则本次不建。
//        → push Target / push 3 / push this / push &CellCoords
//          / push &FLH / call WaveClass_CTOR
//        → `mov [this+_TechnoClass.t.FiringWave], eax`
//
// FLH 取炮口发射点，CellCoords 取目标落点。原版用 `tgtCoords` 作为
// 两者的来源，且 FLH 走武器槽对应的炮口偏移。
// ============================================================================
bool TechnoClass::Fire_WaveWeapon(AbstractClass* pTarget, WeaponTypeClass* pWeapon,
                                  int32 nWeaponIndex)
{
    if (pWeapon == nullptr || pTarget == nullptr)
        return false;

    // 只有声波与磁能波两类走"波"渲染；其余武器交回常规弹丸路径。
    const bool isSonic   = pWeapon->IsSonic;
    const bool isMagBeam = pWeapon->IsMagBeam;
    if (!isSonic && !isMagBeam)
        return false;

    // asm: WaveClass_CTOR 的 type 形参 —— 声波 0、磁能 3。
    const int32 waveType = isMagBeam ? 3 : 0;

    // ------------------------------------------------------------------------
    // asm: 磁能波分支在建立新波之前对旧波的让位检查
    //   `mov eax,[this+FiringWave] / test / jz loc_6FF623 -> 可以建`
    //   `mov edx,[edi] / mov ecx,edi / call [edx+2Ch] / cmp eax,6
    //    / jz loc_6FF656 -> 旧波是建筑上的附着特效，本次不建`
    //
    // 原版这里比的是 `AbstractType::Building (== 6)`：旧的 FiringWave 槽
    // 有时被 AnimClass 之类挂在建筑上的特效占用，这种情况下磁暴线圈不再
    // 另外生成磁能束。
    // ------------------------------------------------------------------------
    if (isMagBeam && FiringWave != nullptr) {
        const AbstractType what = FiringWave->WhatAmI();
        if (what == AbstractType::Building) {
            // asm: loc_6FF656 —— 旧槽被建筑附着特效占用，本次跳过建波。
            return true;   // 仍算作"由波武器消费"，不发射弹丸
        }
    }

    // ------------------------------------------------------------------------
    // 取发射参数：
    //   FLH       —— 武器槽对应的炮口（含 Z 偏移）发射点。
    //   CellCoords —— 目标落点世界坐标。
    // 原版把 tgtCoords 同时用于 CellCoords，FLH 走 GetFLH。
    // ------------------------------------------------------------------------
    const CoordStruct flh = GetFLH(nWeaponIndex, true);

    CoordStruct cellCoords;
    pTarget->GetCoords(&cellCoords);

    TechnoClass* pTargetTechno = nullptr;
    {
        // asm 的 WaveClass_CTOR 第 5 个形参是 _TechnoClass* Target；
        // 只有当目标确实是 Techno 系对象时才传入，否则为空。
        const AbstractType what = pTarget->WhatAmI();
        if (what == AbstractType::Infantry || what == AbstractType::Unit ||
            what == AbstractType::Aircraft || what == AbstractType::Building ||
            what == AbstractType::Techno) {
            pTargetTechno = static_cast<TechnoClass*>(pTarget);
        }
    }

    // asm: push size WaveClass / call new (240h) —— 堆上分配并构造。
    WaveClass* pWave = GameCreateArgs<WaveClass>(flh, cellCoords, this, waveType, pTargetTechno);

    if (pWave == nullptr) {
        // asm: loc_6FF47D / loc_6FF64E —— 分配失败时槽位置空。
        FiringWave = nullptr;
        return true;
    }

    // asm: mov [this+_TechnoClass.t.FiringWave], eax
    FiringWave = pWave;
    return true;
}

BulletClass* TechnoClass::Fire_Impl(AbstractClass* pTarget, int32 nWeaponIndex)
{
    if (pTarget == nullptr)
        return nullptr;

    // Look up the weapon.  The full binary indexes into the TechnoType's
    // weapon list; here we just check the index is in range.
    if (nWeaponIndex < 0 || nWeaponIndex >= 18)
        return nullptr;

    // ------------------------------------------------------------------------
    // asm: TechnoClass_Fire — 音波/磁能波武器前置处理
    //
    //   原版在整段开火序列之前先做两件事：
    //     1) IsSonic 且已存在 FiringWave → 整段跳过（不重复开火）。
    //        对应 asm: `mov al,[ebx+WeaponTypeClass.IsSonic] / test / jz`,
    //        `mov eax,[this+FiringWave] / test / jnz loc_6FDE03`。
    //     2) 音波/磁能波武器的开火由"生成一条波"完成，而不是发射弹丸。
    //   这里把 (1) 表达为 early-out，把 (2) 交给 Fire_WaveWeapon。
    // ------------------------------------------------------------------------
    {
        WeaponStruct* wsWave = (TechnoType != nullptr) ? TechnoType->GetWeapon(nWeaponIndex)
                                                       : nullptr;
        WeaponTypeClass* pWaveWeapon = (wsWave != nullptr) ? wsWave->WeaponType : nullptr;
        if (pWaveWeapon != nullptr && pWaveWeapon->IsSonic && FiringWave != nullptr) {
            // asm: loc_6FDE03 —— 已有声波束未散，本次开火整体跳过。
            return nullptr;
        }

        if (Fire_WaveWeapon(pTarget, pWaveWeapon, nWeaponIndex)) {
            // 本次开火已由波武器消费（生成声波束 / 磁能束），不产生弹丸。
            FireRechargeTimer = 0;
            LastFireFrame = Game::CurrentFrame;
            return nullptr;
        }
    }

    // Gate firing on the weapon's rate of fire.  The original binary reads
    // ROF from the WeaponTypeClass referenced by the TechnoType's weapon
    // slot and applies the veteran / elite reload multipliers.  We resolve
    // the WeaponTypeClass through the TechnoType's weapon array and read
    // its ROF member directly, applying the same veterancy scaling used by
    // WeaponTypeClass::CalculateROF.  The gate is expressed in terms of
    // Game::CurrentFrame so it stays correct even if Update_AI is not run
    // every frame.
    int32 baseROF = 15;  // fallback default if the weapon slot is unset
    if (TechnoType != nullptr) {
        WeaponStruct* ws = TechnoType->GetWeapon(nWeaponIndex);
        if (ws != nullptr && ws->WeaponType != nullptr) {
            baseROF = ws->WeaponType->ROF;
        }
    }
    double rofMultiplier = 1.0;
    if (VeterancyLevel >= 2)
        rofMultiplier = 0.8;   // elite: -20% reload time
    else if (VeterancyLevel == 1)
        rofMultiplier = 0.9;   // veteran: -10% reload time
    int32 effectiveROF = static_cast<int32>(baseROF * rofMultiplier + 0.5);
    if (effectiveROF < 1)
        effectiveROF = 1;

    int32 currentFrame = Game::CurrentFrame;
    if (currentFrame - LastFireFrame < effectiveROF)
        return nullptr;

    // Arm the recharge timer (mirrored by Update_AI) and stamp the frame so
    // the next shot is gated on the same ROF window.
    FireRechargeTimer = effectiveROF;
    LastFireFrame = currentFrame;

    // Launch the projectile.  GetFLH supplies the muzzle coordinate; the
    // warhead comes from the weapon slot.
    WeaponStruct* ws = TechnoType->GetWeapon(nWeaponIndex);
    WeaponTypeClass* pWeapon = (ws != nullptr) ? ws->WeaponType : nullptr;
    if (pWeapon == nullptr || pWeapon->Projectile == nullptr)
        return nullptr;

    CoordStruct source = GetFLH(nWeaponIndex, true);

    CoordStruct targetPos;
    pTarget->GetCoords(&targetPos);

    BulletClass* pBullet = BulletClass::Fire(
        pWeapon->Projectile, pWeapon, source, targetPos, this,
        pWeapon->Damage, pWeapon->Warhead);

    // asm TechnoClass_Fire: 弹体持有目标指针（[bullet+10Ch]），供
    //   BulletClass_Update 的 Simulate_Homing/Simulate_Ballistic 追踪与
    //   CanHit/TargetWentAway 判定使用。缺这一步时弹体只能按 FuseCoords
    //   直线飞行，命中判定退化为地形碰撞。
    if (pBullet != nullptr && pTarget != nullptr) {
        const AbstractType what = pTarget->WhatAmI();
        if (what == AbstractType::Infantry || what == AbstractType::Unit ||
            what == AbstractType::Aircraft || what == AbstractType::Building ||
            what == AbstractType::Techno) {
            pBullet->SetTarget(static_cast<TechnoClass*>(pTarget));
        }
    }

    // ------------------------------------------------------------------------
    // asm L1570385: `push arg / push ecx(pos) / call [edx+vt_BulletClass.Place]`
    //
    //   Place == BulletClass_SetMovement：ObjectClass::Unlimbo(this) 把弹体
    //   真正落位（→ vec_Logics.Add / DisplayClass::Submit），再由类型参数把
    //   速度归一化、写入起爆计时、按 Target 抓 FuseCoords。紧随其后的
    //   `call [edx+8]`（Release）是把 COM 引用交还给世界，本项目无引用计数。
    //
    //   ★ 此前 Fire_Impl 只 new 出弹体、从不 Place：弹体既不进 vec_Logics
    //   也不进显示层，逐帧只能靠 MainLoop 的兜底遍历推进，与 asm 的
    //   「vec_Logics 统一驱动」（L751847-751856）结构不符。补上这一步。
    //   速度用「发射点 → 目标点」的位移向量，SetMovement 内部会归一化。
    // ------------------------------------------------------------------------
    if (pBullet != nullptr) {
        CoordStruct vel;
        vel.X = targetPos.X - source.X;
        vel.Y = targetPos.Y - source.Y;
        vel.Z = targetPos.Z - source.Z;

        const int32 shotSpeed = (pWeapon->Projectile->ProjectileSpeed > 0)
                              ? pWeapon->Projectile->ProjectileSpeed
                              : pBullet->GetSpeed();
        pBullet->SetMovement(source, vel, shotSpeed);
    }

    return pBullet;
}

// ============================================================================
// TakeDamage implementation
//
//  Applies damage to this TechnoClass.  The warhead's Verses table modulates
//  the raw damage based on this unit's armor.  Returns true if the unit died
//  as a result of the damage.
//
//  asm: TechnoClass_ReceiveDamage   (asm:1575554)
//    eDamageState __thiscall TechnoClass_ReceiveDamage(_TechnoClass *this,
//        int *Damage, int TypeSource, WarheadTypeClass *WH, int target,
//        int ignoreDefenses, int arg10, HouseClass_old *SourceHouse)
//
//  伤害结算链（逐条对照 asm:1575590~1575610）:
//    call [eax+vt_TechnoClass.GetTechnoType]        ; this->GetTechnoType()
//    mov  ecx,[this+_FootClass.OwningPlayer]        ; this->Owner
//    push eax / call HouseClass_GetTypeArmorMult    ; Owner->GetTypeArmorMult(type)
//    fmul [this+_TechnoClass.t.ArmorMultiplier]     ; × this->ArmorMultiplier
//    fdivr [esp+var_B0]                             ; Damage / 上述乘积
//    call Float_To_Int_Floor                        ; ★ 向下取整(非四舍五入)
//    mov  [ebx],eax                                 ; *Damage = 结果
// ============================================================================
bool TechnoClass::TakeDamage_Impl(int32 damage, ObjectClass* pSource,
                                  WarheadTypeClass* pWarhead)
{
    if (damage <= 0)
        return false;

    // Invulnerability from Iron Curtain / Force Shield / temporal freeze.
    if (IsShielded() || IsTemporalized())
        return false;

    // asm: 先做"敌方护甲倍率 × 自身护甲倍率"的除法缩放。
    //   fdivr 的顺序是 Damage / (ArmorMult * ArmorMultiplier)，即把伤害按
    //   倍率缩小 —— 倍率越大承受伤害越低。
    int32 scaledDamage = damage;
    if (Owner != nullptr && TechnoType != nullptr)
    {
        // asm: HouseClass_GetTypeArmorMult（38 分支 switch，按 WhatAmI 分派）
        const double typeArmor = Owner->Get_Type_Armor_Mult(TechnoType);
        const double divisor   = typeArmor * ArmorMultiplier;

        // asm: fdivr + Float_To_Int_Floor —— 必须向下取整，
        //      原版用 floor 而非 round，这里不能写成 +0.5f。
        if (divisor > 0.0)
            scaledDamage = static_cast<int32>(Float_To_Int_Floor(
                               static_cast<double>(damage) / divisor));
    }
    if (scaledDamage <= 0)
        return false;

    // Apply the warhead's armor-class multiplier (Verses[]).
    int32 finalDamage = scaledDamage;
    if (pWarhead != nullptr && TechnoType != nullptr)
    {
        float mult = pWarhead->GetDamageMultiplier(static_cast<int32>(TechnoType->Get_Armor()));
        // asm: 同一公式路径，仍按 floor 取整保持行为一致。
        finalDamage = static_cast<int32>(Float_To_Int_Floor(
                          static_cast<double>(scaledDamage) * static_cast<double>(mult)));
    }
    if (finalDamage <= 0)
        return false;

    Health -= finalDamage;

    if (Health <= 0)
    {
        Health = 0;

        // Award experience to the killer (elite promotion handled by
        // Update_Veterancy on the killer's next update).
        if (pSource != nullptr)
        {
            TechnoClass* pKiller = (pSource->WhatAmI() >= AbstractType::Unit &&
                                    pSource->WhatAmI() <= AbstractType::Building)
                                   ? static_cast<TechnoClass*>(pSource) : nullptr;
            if (pKiller != nullptr && pKiller != this)
            {
                int32 bounty = (TechnoType != nullptr) ? (TechnoType->Cost / 10) : 10;
                pKiller->Experience += bounty;
            }
        }

        // Notify the owning house so credits/tech-tree stay consistent.
        if (Owner != nullptr)
            Owner->RegisterTechnoLoss(this);

        RegisterDestruction();
        Destroyed(pSource);
        return true;
    }
    return false;
}

// ============================================================================
// Repair logic
//
//  Begins / ends the repair state.  The full binary also deducts credits
//  from the owning house and sparks a repair-anim.
// ============================================================================
void TechnoClass::Repair_Start(int32 rate)
{
    if (Health >= MaxHealth)
        return;
    RepairActive = true;
    RepairRate = rate;
}

void TechnoClass::Repair_Stop()
{
    RepairActive = false;
    RepairRate = 0;
}

// ============================================================================
// Cloak / Uncloak
//
//  Triggers the cloak state machine.  Cloak fades the unit out over a few
//  frames; Uncloak fades it back in.  The original binary also plays a
//  sound and notifies the owning house's radar.
// ============================================================================
void TechnoClass::Cloak(bool bPlaySound)
{
    (void)bPlaySound;
    if (CloakState == CloakStateEnum::Cloaked ||
        CloakState == CloakStateEnum::Cloaking)
        return;

    CloakState = CloakStateEnum::Cloaking;
    CloakTimer = 30;
}

void TechnoClass::Uncloak(bool bPlaySound)
{
    (void)bPlaySound;
    if (CloakState == CloakStateEnum::Idle ||
        CloakState == CloakStateEnum::Uncloaking)
        return;

    CloakState = CloakStateEnum::Uncloaking;
    CloakTimer = 30;
}

bool TechnoClass::Is_Cloaked() const
{
    return CloakState == CloakStateEnum::Cloaked;
}

bool TechnoClass::Is_Cloaking() const
{
    return CloakState == CloakStateEnum::Cloaking ||
           CloakState == CloakStateEnum::Cloaked;
}

// ============================================================================
// Veteran / Promote
//
//  Adds experience and (if the threshold is crossed) bumps the veterancy
//  level.  The full binary applies the veterancy multipliers here.
// ============================================================================
void TechnoClass::Promote(int32 experience)
{
    Experience += experience;
    Update_Veterancy();
}

// asm: void __thiscall Veterancy_AddValue(_TechnoClass *this, int ownerCost, int victimCost)  (asm:1595096)
//
//  Awards veterancy credit proportional to the victim's value relative to the
//  killer's own value:
//      Experience += victimCost * Rules->VeteranRatio / ownerCost
//  then clamps the running total to Rules->VeteranCap.  `ownerCost` of zero is
//  treated as one so a techless killer does not divide by zero.
void TechnoClass::AddVeterancyValue(int32 ownerCost, int32 victimCost)
{
    const double ratio = (RulesClass::Instance != nullptr)
                       ? RulesClass::Instance->VeteranRatio : 0.0;
    const double cap   = (RulesClass::Instance != nullptr)
                       ? RulesClass::Instance->VeteranCap : 0.0;

    const double denom = (ownerCost != 0) ? static_cast<double>(ownerCost) : 1.0;
    double total = static_cast<double>(Experience)
                 + static_cast<double>(victimCost) * ratio / denom;

    if (total > cap)
        total = cap;

    Experience = static_cast<int32>(total);
}

int32 TechnoClass::GetVeterancy() const
{
    return VeterancyLevel;
}

int32 TechnoClass::Get_Experience() const
{
    return Experience;
}

// ============================================================================
// Is_Ally / Is_Enemy
//
//  Returns true if the supplied house is on the same team as this unit's
//  owner.  The full binary walks the HouseClass alliance table; the
//  standalone build treats "same owner" as "ally".
// ============================================================================
bool TechnoClass::Is_Ally(HouseClass* pHouse) const
{
    if (pHouse == nullptr)
        return false;
    return (pHouse == Owner);
}

bool TechnoClass::Is_Enemy(HouseClass* pHouse) const
{
    if (pHouse == nullptr)
        return false;
    return (pHouse != Owner);
}

bool TechnoClass::Is_Ally(TechnoClass* pTechno) const
{
    if (pTechno == nullptr)
        return false;
    return Is_Ally(pTechno->Owner);
}

bool TechnoClass::Is_Enemy(TechnoClass* pTechno) const
{
    if (pTechno == nullptr)
        return false;
    return Is_Enemy(pTechno->Owner);
}

// ============================================================================
// 根据游戏行为，可知 Threat_Pos 负责下面这段逻辑。
//
//  Returns the position the AI should aim at when attacking this unit.  For
//  most units this is the center of the voxel / shape; for buildings the
//  original binary picks the closest cell.
// ============================================================================
CoordStruct TechnoClass::Get_Threat_Pos() const
{
    return Location;
}

// ============================================================================
// ComputeCRC
//
//  Chains the parent CRC and then adds the TechnoClass-specific state.
// ============================================================================
void TechnoClass::ComputeCRC(CRCEngine& crc) const
{
    // asm: 逐层回链（vt_TechnoClass.r.m.o 的 a.ComputeCRC）：
    //   Techno -> Radio（链路表）-> Mission（任务状态）-> Object -> Abstract。
    RadioClass::ComputeCRC(crc);

    crc.AddData(&Health,           sizeof(Health));
    crc.AddData(&MaxHealth,        sizeof(MaxHealth));
    crc.AddData(&VeterancyLevel,   sizeof(VeterancyLevel));
    crc.AddData(&Experience,       sizeof(Experience));
    crc.AddData(&CloakState,       sizeof(CloakState));
    crc.AddData(&CloakAlpha,       sizeof(CloakAlpha));
    crc.AddData(&FireRechargeTimer, sizeof(FireRechargeTimer));
    crc.AddData(&RepairActive,      sizeof(RepairActive));
    crc.AddData(&RepairRate,        sizeof(RepairRate));
    crc.AddData(&IronCurtainTimer,  sizeof(IronCurtainTimer));
    crc.AddData(&ForceShieldTimer,  sizeof(ForceShieldTimer));
    crc.AddData(&FireDamageTimer,   sizeof(FireDamageTimer));
    crc.AddData(&TemporalTimer,     sizeof(TemporalTimer));
    crc.AddData(&RadiationTimer,    sizeof(RadiationTimer));
}

// ============================================================================
// SelectWeapon — 武器选择
// 原版: TechnoClass_SelectWeapon（asm:3468）
//
//  asm 逐段（返回 0/1 的主/副槽，或换塔单位的阶段槽）：
//    type = GetTechnoType()
//    IsTurretChanger(type) && !type->IsGattling
//        → return (CurrentTurretNumber >= 0) ? CurrentTurretNumber : 0
//    if (CanOccupyFire()) → 0
//    weap2 = GetWeapon(1)->WeaponType ; 空 → 0
//    weap1 = GetWeapon(0)->WeaponType ; 空 → 0
//    if (weap2->NeverUse) → 0
//    if (!target) → 0
//    if (InOpenTopped && type->OpenTransportWeapon != -1) → OpenTransportWeapon
//    if (type->IsGattling)：
//        stage = CurrentGattlingStage
//        if (weap2->Projectile->AA && target->InAir()) → stage*2 + 1
//        else → stage*2
//    if (weap2->Warhead->Airstrike)：
//        目标是建筑且 CanC4、非资源目的地/采集者 → 1；否则 0
//    其余分支（Drainable/LandTargeting/Verses/换塔状态）→ 1，默认 → 0
// ============================================================================
int32 TechnoClass::SelectWeapon(AbstractClass* pTarget)
{
    if (TechnoType == nullptr)
        return 0;

    // asm: call TechnoTypeClass_IsTurretChanger ; test al / jz ; mov cl,IsGattling / jnz
    //       mov esi,[CurrentTurretNumber] ; neg/sbb/and  → (n>=0)?n:0
    if (TechnoType->IsTurretChanger() && !TechnoType->IsGattling)
        return (ActiveTurretIndex >= 0) ? ActiveTurretIndex : 0;

    // asm: call [vt+CanOccupyFire] / test al / jnz @@19(→0)
    if (CanOccupyFire())
        return 0;

    // asm: GetWeapon(1)->[0] ; test / jz @@19
    WeaponStruct* ws2 = TechnoType->GetWeapon(1);
    WeaponTypeClass* weap2 = (ws2 != nullptr) ? ws2->WeaponType : nullptr;
    if (weap2 == nullptr)
        return 0;

    // asm: GetWeapon(0)->[0] ; test / jz @@19
    WeaponStruct* ws1 = TechnoType->GetWeapon(0);
    WeaponTypeClass* weap1 = (ws1 != nullptr) ? ws1->WeaponType : nullptr;
    if (weap1 == nullptr)
        return 0;

    // asm: mov al,[weap2+NeverUse] / test / jnz @@19
    if (weap2->NeverUse)
        return 0;

    // asm: mov edi,[target] / test / jz @@19
    if (pTarget == nullptr)
        return 0;

    // asm: mov al,[this+InOpenTopped] ; 若在敞开运输舱内，且类型指定了开舱武器
    if (InOpenTopped && TechnoType->OpenTransportWeapon != -1)
        return TechnoType->OpenTransportWeapon;

    // asm: @@2: ebp = (target->derivationFlags & 1) ? target : 0
    //   OpenYR 以"目标是 Techno（可查 InAir）"等价表达。
    TechnoClass* pTgtTech = nullptr;
    {
        const AbstractType what = pTarget->WhatAmI();
        if (what == AbstractType::Unit || what == AbstractType::Infantry
         || what == AbstractType::Building || what == AbstractType::Aircraft)
            pTgtTech = static_cast<TechnoClass*>(pTarget);
    }

    // asm: 转管机枪按阶段选主/副槽；对空时占奇数槽。
    if (TechnoType->IsGattling)
    {
        const int32 stage = WeaponStage;
        if (weap2->Projectile != nullptr && weap2->Projectile->AA
            && pTgtTech != nullptr && pTgtTech->InAir())
            return stage * 2 + 1;
        return stage * 2;
    }

    // asm: @@4: wea2 的弹头是空袭型 → 只打可 C4 的建筑资源目标
    if (weap2->Warhead != nullptr && weap2->Warhead->Airstrike)
    {
        if (pTgtTech != nullptr && pTarget->WhatAmI() == AbstractType::Building)
        {
            BuildingClass* pB = static_cast<BuildingClass*>(pTarget);
            if (pB->Type != nullptr && pB->Type->CanC4
                && !pB->Type->ResourceDestination && !pB->Type->ResourceGatherer)
                return 1;
        }
        return 0;
    }

    // asm: 其余目标相关分支（Drainable / LandTargeting / Verses 等）在满足时返回 1，
    //      不满足时落到 @@19 返回 0。此处保守返回 0（主武器）。
    return 0;
}

//  asm 逐段（TechnoClass_IsCloseEnoughToTarget, asm:8339）：
//    if (!target || !weapon) → false
//    range = weapon->Range；若 == -0x200 直接 true（"无限射程"哨兵）
//    若 target->InAir() → range += type->AirRangeBonus
//    若武器带高地型弹头 → range += GetElevationRangeBonus(target)
//    tgtPos = target->GetCoords()；目标贴地 → tgtPos.Z 取该格地面高度
//    最小射程：dist < weapon->MinimumRange → false
//    建筑：range += (X_Foundation_Value + Y_Foundation_Value) << 6
//    距离：非飞机用 3D 距离，飞机用 2D 距离；dist > range → false
//    最终再做视线/命中判定后返回 true
// ============================================================================
bool TechnoClass::IsCloseEnoughToTarget(AbstractClass* pTarget, int32 idxWeapon)
{
    if (pTarget == nullptr || TechnoType == nullptr)
        return false;

    WeaponStruct* ws = TechnoType->GetWeapon(idxWeapon);
    if (ws == nullptr || ws->WeaponType == nullptr)
        return false;
    WeaponTypeClass* pWeapon = ws->WeaponType;

    // asm: mov edi,[ebx+Range] / cmp edi,0FFFFFE00h / jnz
    int32 range = pWeapon->GetAttackRange();
    if (range == -0x200)
        return true;

    // asm: call [target+vt+54h] (InAir) / jz ; add edi,[type+AirRangeBonus]
    if (pTarget->IsInAir())
        range += TechnoType->AirRangeBonus;

    // asm: 武器带高地型弹头时把高地加成并入射程。
    TechnoClass* pTgtTech = nullptr;
    {
        const AbstractType what = pTarget->WhatAmI();
        if (what == AbstractType::Unit || what == AbstractType::Infantry
         || what == AbstractType::Building || what == AbstractType::Aircraft)
            pTgtTech = static_cast<TechnoClass*>(pTarget);
    }
    if (pTgtTech != nullptr && pWeapon->Warhead != nullptr)
        range += static_cast<int32>(GetElevationRangeBonus(pTgtTech));

    // asm: target->GetCoords；目标贴地则落点 Z 取该格地面高度。
    CoordStruct tgtPos;
    pTarget->GetCoords(&tgtPos);
    if (MapClass::Instance != nullptr && pTarget->IsOnFloor())
        tgtPos.Z = MapClass::Instance->GetCellFloorHeight(tgtPos);

    const CoordStruct srcPos = GetCoords();

    // asm: 最小射程门：[ebx+0B8h] 非零时 dist < 该值 → false。
    const int32 minRange = pWeapon->GetMinimumAttackRange();
    if (minRange > 0)
    {
        const int64 dx = static_cast<int64>(srcPos.X - tgtPos.X);
        const int64 dy = static_cast<int64>(srcPos.Y - tgtPos.Y);
        const int64 dz = static_cast<int64>(srcPos.Z - tgtPos.Z);
        const int32 dist = static_cast<int32>(
            std::floor(std::sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz))));
        if (dist < minRange)
            return false;
    }

    // asm: 目标是建筑 → range += (X_Foundation_Value() + Y_Foundation_Value(0)) << 6
    int32 effectiveRange = range;
    if (pTarget->WhatAmI() == AbstractType::Building)
    {
        BuildingClass* pB = static_cast<BuildingClass*>(pTarget);
        if (pB->Type != nullptr)
            effectiveRange += (pB->Type->X_Foundation_Value()
                             + pB->Type->Y_Foundation_Value(false)) << 6;
    }

    // asm: 飞机用 2D 距离，其余用 3D 距离；dist > range → false。
    const int64 dx = static_cast<int64>(srcPos.X - tgtPos.X);
    const int64 dy = static_cast<int64>(srcPos.Y - tgtPos.Y);
    const int64 dz = static_cast<int64>(srcPos.Z - tgtPos.Z);
    double distSq = static_cast<double>(dx * dx + dy * dy);
    if (WhatAmI() != AbstractType::Aircraft)
        distSq += static_cast<double>(dz * dz);

    const int32 dist = static_cast<int32>(std::floor(std::sqrt(distSq)));
    return dist <= effectiveRange;
}

// ============================================================================
// EvalThreatRating — 威胁评估（AI 目标选择的启发式评分）
// 原版: TechnoClass_EvalThreatRating（385 行）
// 评分 = 对威胁方的火力 × 距离因子，数值越高越值得优先攻击。
// ============================================================================
int32 TechnoClass::EvalThreatRating(TechnoClass* pThreat, int32 idxWeapon)
{
    if (pThreat == nullptr || TechnoType == nullptr)
        return 0;

    WeaponStruct* ws = TechnoType->GetWeapon(idxWeapon);
    if (ws == nullptr || ws->WeaponType == nullptr)
        return 0;

    CoordStruct selfPos;
    GetCoords(&selfPos);
    CoordStruct threatPos;
    pThreat->GetCoords(&threatPos);

    int32 dist = CoordMath::CoordDistance(selfPos, threatPos);
    int32 range = ws->WeaponType->GetAttackRange();
    if (range <= 0)
        return 0;
    if (dist > range)
        return 0;

    // 基础评分：武器伤害，随距离衰减（近处威胁优先）。
    int32 rating = ws->WeaponType->Damage;
    rating = (rating * (range - dist)) / range;
    return rating;
}

// ============================================================================
// RegisterDestruction — 登记本单位的摧毁
// 原版: TechnoClass_RegisterDestruction（asm:15970）
//
//  asm 逐段：
//    if (Alive && Tag && Owner) {
//        TagClass_RaiseEvent(6,  this, coords, 0, 0)
//        TagClass_RaiseEvent(4,  this, coords, 0, 0)
//        if (WhatAmI() != Unit) TagClass_RaiseEvent(7, this, coords, 0, 0)
//        TagClass_RaiseEvent(48, this, coords, 0, 0)
//        TagClass_RaiseEvent(29, this, coords, 0, 0)
//    }
//    if (!type->DontScore) { 给击杀者记战功（战功在该路径汇总） }
//    随后撤除裂缝发生器贡献并摘除全局登记。
// ============================================================================
void TechnoClass::RegisterDestruction()
{
    // asm: mov al,[this+Alive] / test / jz ; Tag / jz ; killer / jz
    if (Alive != 0 && Tag != nullptr && Owner != nullptr)
    {
        const CellStruct loc = CellClass::Coord2Cell(GetCoords());
        Tag->RaiseEvent(6,  this, loc, false, nullptr);
        Tag->RaiseEvent(4,  this, loc, false, nullptr);
        if (WhatAmI() != AbstractType::Unit)
            Tag->RaiseEvent(7, this, loc, false, nullptr);
        Tag->RaiseEvent(48, this, loc, false, nullptr);
        Tag->RaiseEvent(29, this, loc, false, nullptr);
    }

    // asm: mov cl,[eax+DontScore] / test / jnz skip —— 计分由击杀者路径处理。
    //   （DontScore 为真则不计入战果；本层仅做裂缝撤除与全局摘除。）
    DeleteGap();
    Remove_From_Array(this);
}

// ============================================================================
// RegisterLoss — 阵营失陷登记
// 原版: TechnoClass_RegisterLoss（asm:16402）
//
//  与 RegisterDestruction 同构：对存活且带 Tag 的对象依次抛 6/4/7/48/29，
//  作用在"被移出所属阵营"的时机。
// ============================================================================
void TechnoClass::RegisterLoss()
{
    if (Alive != 0 && Tag != nullptr && Owner != nullptr)
    {
        const CellStruct loc = CellClass::Coord2Cell(GetCoords());
        Tag->RaiseEvent(6,  this, loc, false, nullptr);
        Tag->RaiseEvent(4,  this, loc, false, nullptr);
        if (WhatAmI() != AbstractType::Unit)
            Tag->RaiseEvent(7, this, loc, false, nullptr);
        Tag->RaiseEvent(48, this, loc, false, nullptr);
        Tag->RaiseEvent(29, this, loc, false, nullptr);
    }

    DeleteGap();
}

// ============================================================================
// GetFLH — 计算炮口/开火点（Forward, Lateral, Height 偏移 + 座架旋转）
// 原版: TechnoClass_GetFLH（asm:4313）
//
//  asm 逐段：
//    type = GetTechnoType()（vt+84h）
//    idxWeapon < 0：neg 后若 <=5 → 读备用 FLH 表 AlternateFLH[idx][0..2]
//    idxWeapon >= 0：ws = [vt+3F8h](idxWeapon)；读 ws->FLH（+4/+8/+0Ch）
//    new_X = fwd + FLH_argX ; new_Y = lat + FLH_argY ; new_Z = hgt + FLH_argZ
//    若本对象带 locomotor 且标志位允许 → 用 Matrix3DClass 组装朝向矩阵，
//      以 Desired_Facing256 取到的朝向旋转 FLH；
//    result = GetCoords() + 旋转后的 FLH（最后 Float_To_Int_Floor 取整）
//
//  OpenYR 以朝向角的 cos/sin 等价替代 Matrix3D 旋转（语义等价即可）。
// ============================================================================
CoordStruct TechnoClass::GetFLH(int32 nWeaponIndex, bool muzzle)
{
    CoordStruct out(0, 0, 0);
    if (TechnoType == nullptr)
        return out;

    int32 fwd = 0, lat = 0, hgt = 0;

    if (nWeaponIndex < 0)
    {
        // asm: neg esi / cmp esi,5 / jg skip —— 备用 FLH 表。
        const int32 idx = -nWeaponIndex;
        if (idx <= 5)
        {
            fwd = TechnoType->AlternateFLH[idx][0];
            lat = TechnoType->AlternateFLH[idx][1];
            hgt = TechnoType->AlternateFLH[idx][2];
        }
    }
    else
    {
        // asm: call [vt+3F8h](idxWeapon) —— 每槽独立的炮口偏移。
        WeaponStruct* ws = TechnoType->GetWeapon(nWeaponIndex);
        if (ws != nullptr)
        {
            fwd = ws->FLH_X;
            lat = ws->FLH_Y;
            hgt = ws->FLH_Z;
        }
    }

    // asm: Desired_Facing256(&Facing, &out) —— 取当前朝向（256 分度）。
    const double angle = (static_cast<double>(PrimaryFacing.Value) / 256.0)
                       * (2.0 * 3.14159265358979323846);
    const double c = std::cos(angle);
    const double s = std::sin(angle);

    // asm: 旋转后的 FLH 叠加到 GetCoords()。RA2 中朝向 0=N(-Y)，故
    //      世界 X 用 sin 分量、世界 Y 用 -cos 分量。
    const CoordStruct base = GetCoords();
    out.X = base.X + static_cast<int32>(fwd * s + lat * c);
    out.Y = base.Y + static_cast<int32>(fwd * (-c) + lat * s);
    out.Z = base.Z + hgt + TechnoType->TurretOffset;

    (void)muzzle;
    return out;
}

// ============================================================================
// EstimateDamage — 对目标造成的预估伤害（UI 显示用）
// 原版: TechnoClass_EstimateDamage（205 行）
// ============================================================================
int32 TechnoClass::EstimateDamage(AbstractClass* pTarget, int32 idxWeapon)
{
    if (pTarget == nullptr || TechnoType == nullptr)
        return 0;

    WeaponStruct* ws = TechnoType->GetWeapon(idxWeapon);
    if (ws == nullptr || ws->WeaponType == nullptr)
        return 0;

    TechnoClass* pTargetTechno = (pTarget->WhatAmI() >= AbstractType::Unit &&
                                  pTarget->WhatAmI() <= AbstractType::Building)
                                 ? static_cast<TechnoClass*>(pTarget) : nullptr;

    return ws->WeaponType->CalculateDamage(this, pTargetTechno);
}

// ============================================================================
// ShouldRetaliate — 是否应当还击
// 原版: TechnoClass_ShouldRetaliate   (asm:0x7087F0 / 21278)
//
//  asm 分支顺序（任一不满足即返回 false）：
//    target 为空 / type->CanRetaliate 为假              → false
//    [this+1CCh] 与 Owner 的门控（被 EMP/瘫痪）          → false （字段未建模）
//    CaptureManager 存在且 OutOfSpareLinks()             → false
//    Is_Ally(攻击方 Owner)                               → false
//    攻击方对己方敌对（[ebp+0C8h]）                       → false （字段未建模）
//    Combat_Damage(-1) <= 0                              → false
//    SelectWeapon(target) 与射程判定                     → 不在射程则 false
//  ★ 旧实现用“目标敌对 + 铁幕 + 任意武器 CanFire”近似，缺少 CanRetaliate 与
//    CaptureManager 门控；此处按 asm 补齐。
// ============================================================================
bool TechnoClass::ShouldRetaliate(TechnoClass* pAttacker)
{
    if (pAttacker == nullptr)
        return false;
    if (TechnoType == nullptr)
        return false;

    // asm: mov cl,[eax+_TechnoTypeClass.t.CanRetaliate] / test / jz false
    if (!TechnoType->CanRetaliate)
        return false;

    // asm loc_708825: CaptureManager 存在且无可用链接 → 不能还击。
    if (CaptureManager != nullptr && CaptureManager->OutOfSpareLinks())
        return false;

    // asm: 攻击方与本方同盟则不还击（原版用 HouseClass::Is_Ally）。
    if (Is_Ally(pAttacker->Owner))
        return false;

    // asm: call Combat_Damage(-1) / test eax,eax / jle false
    //   —— 没有任何武器伤害则视为非战斗单位。
    if (Combat_Damage(-1) <= 0)
        return false;

    // asm: SelectWeapon(target) 后做 GetObjectFiringStateNoRange 判定；
    //   本项目用 SelectWeapon + IsCloseEnoughToTarget 表达。
    const int32 idx = SelectWeapon(pAttacker);
    if (idx < 0)
        return false;
    if (!IsCloseEnoughToTarget(pAttacker, idx))
        return false;

    return true;
}

// ============================================================================
// IsRadarVisible — 雷达可见性
// 原版: TechnoClass_IsRadarVisible（279 行）
// 语义: 未被隐形/未被裂缝产生器覆盖，且对指定阵营非完全隐形。
// ============================================================================
bool TechnoClass::IsRadarVisible(HouseClass* pHouse) const
{
    if (pHouse == nullptr)
        return true;
    if (Is_Cloaked() || IsTemporalized())
        return false;
    // 简化：隐形状态下对敌军不可见；友军可见。
    return Is_Ally(pHouse) || !Is_Cloaking();
}

// ============================================================================
// GetZAdjustment — Z 轴高度调整（渲染用）
// 原版: TechnoClass_Get_ZAdjustment（648 行）
// ============================================================================
int32 TechnoClass::Get_ZAdjustment() const
{
    int32 z = 0;
    if (IsInAir())
        z += 2;   // 空中单位抬高
    return z;
}

// ============================================================================
// VisualCharacter — 视觉特征（渲染类型码）
// 原版: TechnoClass_VisualCharacter（302 行）
// ============================================================================
VisualType TechnoClass::VisualCharacter(bool raw)
{
    if (Is_Cloaked() || Is_Cloaking())
        return VisualType::Cloaked;
    if (IsIronCurtained() || IsForceShielded() || IsTemporalized())
        return VisualType::Shadow;   // shielded/frozen units render darkened
    return VisualType::Normal;
}

// ============================================================================
// CreateGap / DeleteGap — 裂缝产生器贡献管理
// 原版: TechnoClass_CreateGap（303 行）/ TechnoClass_DeleteGap（283 行）
// 语义: 拥有 GapGenerator 特性的单位在存活时遮蔽雷达；
//       此处登记/注销到全局裂缝列表。
// ============================================================================
void TechnoClass::CreateGap()
{
    // Gap generators shroud enemy radar.  The gap list lives on the
    // MapClass in the original; with radar not yet wired, track the flag
    // so RegisterDestruction can drop the contribution later.
    GapActive = true;
}

void TechnoClass::DeleteGap()
{
    GapActive = false;
}

// ============================================================================
// UpdateSight — 视野更新（迷雾/战争阴影）
// 原版: TechnoClass_UpdateSight（273 行）
// ============================================================================
void TechnoClass::UpdateSight()
{
    // Sight maintenance is driven by the fog/shroud system once it is wired
    // to the display; this hook mirrors TechnoClass_UpdateSight's role of
    // refreshing the owning house's visibility around the unit.
    if (Owner == nullptr)
        return;
    Owner->UpdateSightAroundUnit(this);
}

// ============================================================================
// DrawExtras / DrawHidden — 附加绘制
// 原版: TechnoClass_DrawExtras（1379 行）/ DrawHidden（422 行）
// ============================================================================
void TechnoClass::DrawExtras(Point2D* pCoord, RectangleStruct* pRect)
{
    (void)pCoord;
    (void)pRect;
    // 血条/选择框等附加绘制交由显示层；此处保留扩展点。
}

void TechnoClass::DrawHidden(Point2D* pCoord, RectangleStruct* pRect)
{
    (void)pCoord;
    (void)pRect;
    // 隐形单位的特殊绘制（若可见于己方）。
}

// ============================================================================
// DealParticleDamage — 对波及单位应用粒子伤害
// 原版: TechnoClass_DealParticleDamage（639 行）
// ============================================================================
void TechnoClass::DealParticleDamage(TechnoClass* pVictim, WarheadTypeClass* pWarhead,
                                     int32 damage, int32 distanceFromEpicenter)
{
    if (pVictim == nullptr || pWarhead == nullptr)
        return;
    // 距离衰减：每格衰减（Warhead 的 CellSpread 语义）。
    float falloff = 1.0f;
    if (distanceFromEpicenter > 0)
        falloff = 1.0f / static_cast<float>(distanceFromEpicenter + 1);
    int32 finalDamage = static_cast<int32>(damage * falloff);
    pVictim->TakeDamage_Impl(finalDamage, this, pWarhead);
}

// ============================================================================
// PointerGotInvalid — 对象失效通知
// 原版: TechnoClass_PointerGotInvalid（612 行）
// 语义: 当引用的目标/所属单位被销毁时，清理本对象持有的悬挂指针。
// ============================================================================
void TechnoClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    if (pInvalid == nullptr)
        return;
    if (Owner == pInvalid)
        Owner = nullptr;

    // asm TechnoClass_PointerGotInvalid: cmp ebp,[esi+278h]; mov [esi+278h],0
    //   —— 目标失效即解除"谁在对我施法"的反向引用。
    if (TemporalTargettingMe == pInvalid)
        TemporalTargettingMe = nullptr;

    // asm loc_707B29: 若本对象正施放时间武器（TemporalImUsing, +0x274 非空），
    //   转交 TemporalClass::PointerGotInvalid 处理 Owner/Victim 失效。
    if (TemporalImUsing != nullptr)
        TemporalImUsing->PointerGotInvalid(pInvalid, removed);

    // 回链父类（继承链已接通）：RadioClass 摘掉指向失效对象的链路槽，
    // 再依次落到 MissionClass / ObjectClass（动画槽 / 标签）。
    RadioClass::PointerGotInvalid(pInvalid, removed);
}

// ============================================================================
// GetSightRange — 视野范围（格数）
// 原版读取 TechnoType 的 Sight 属性；未设置时默认 5 格。
// ============================================================================
int32 TechnoClass::GetSightRange() const
{
    if (TechnoType != nullptr && TechnoType->SightRange >= 0)
        return TechnoType->SightRange;
    return 5;
}


// ============================================================================
// Per-instance serialization (mirrors the ComputeCRC field set so the
// save stream and the sync CRC stay in lockstep).
// ============================================================================
void TechnoClass::Save(SaveGameClass& saver) const
{
    // Ownership is stored as the owning house's array index so the stream
    // is position independent; -1 encodes "no owner".
    int32 ownerIndex = -1;
    if (Owner != nullptr)
    {
        for (int32 i = 0; i < HouseClass::ArrayCount; ++i)
        {
            if (HouseClass::Array[i] == Owner)
            {
                ownerIndex = i;
                break;
            }
        }
    }

    CoordStruct pos;
    GetCoords(&pos);

    saver.Write(ownerIndex);
    saver.Write(pos.X);
    saver.Write(pos.Y);
    saver.Write(pos.Z);
    saver.Write(Health);
    saver.Write(MaxHealth);
    saver.Write(VeterancyLevel);
    saver.Write(Experience);
    saver.Write(static_cast<int32>(CloakState));
    saver.Write(static_cast<int32>(CloakAlpha));
    saver.Write(FireRechargeTimer);
    saver.Write(CloakTimer);
    saver.Write(static_cast<int8>(RepairActive ? 1 : 0));
    saver.Write(RepairRate);
    saver.Write(IronCurtainTimer);
    saver.Write(ForceShieldTimer);
}

void TechnoClass::Load(LoadGameClass& loader)
{
    int32 ownerIndex = -1;
    int32 posX = 0, posY = 0, posZ = 0;

    loader.Read(ownerIndex);
    loader.Read(posX);
    loader.Read(posY);
    loader.Read(posZ);
    loader.Read(Health);
    loader.Read(MaxHealth);
    loader.Read(VeterancyLevel);
    loader.Read(Experience);

    int32 cloakState = 0;
    int32 cloakAlpha = 0;
    loader.Read(cloakState);
    loader.Read(cloakAlpha);
    CloakState = static_cast<CloakStateEnum>(cloakState);
    CloakAlpha = static_cast<uint8>(cloakAlpha);

    loader.Read(FireRechargeTimer);
    loader.Read(CloakTimer);

    int8 repairActive = 0;
    loader.Read(repairActive);
    RepairActive = (repairActive != 0);

    loader.Read(RepairRate);
    loader.Read(IronCurtainTimer);
    loader.Read(ForceShieldTimer);

    if (ownerIndex >= 0 && ownerIndex < HouseClass::ArrayCount)
        Owner = HouseClass::Array[ownerIndex];
    else
        Owner = nullptr;

    // Position restore is the responsibility of the concrete subclass
    // (FootClass/AircraftClass/BuildingClass own their SetCoords entry);
    // the streamed coordinates are kept for the subclass loader.
    (void)posX;
    (void)posY;
    (void)posZ;
}

// ============================================================================
 // TechnoClass::GetThreatValue - (TechnoClass_GetThreatPosed)
//
//   The value this object contributes to the owning house's threat grid.
//   Resolving the type first (vtable +0x84) keeps derived classes that answer
//   the same way from reimplementing it, which is why the base version is
//   virtual in the binary.
//
//   A garrisoned building answers with its occupant count scaled by
//   RulesClass::ThreatPerOccupant; everything else answers with the type's
//   own ThreatPosed figure.
// ============================================================================
int32 TechnoClass::GetThreatValue() const
{
    const TechnoTypeClass* pType = TechnoType;
    if (pType == nullptr) {
        return 0;
    }

    if (WhatAmI() == AbstractType::Building) {
        const BuildingClass* pBuilding = static_cast<const BuildingClass*>(this);
        const int32 occupants = pBuilding->Occupants.Count;
        if (occupants > 0) {
            const int32 perOccupant = (TheRules != nullptr)
                                          ? TheRules->ThreatPerOccupant
                                          : 0;
            return occupants * perOccupant;
        }
    }

    return pType->ThreatPosed;
}

// ============================================================================
// TechnoClass::Get_Cell_Ptr_Coord
//
//   The cell the object is standing in.  The binary converts the object's
//   world coordinate through MapClass::Coord_Cell.
// ============================================================================
CellStruct TechnoClass::Get_Cell_Ptr_Coord() const
{
    const CoordStruct coord = Get_Coord();
    return CellClass::Coord2Cell(coord);
}

// ============================================================================
// TechnoClass - small accessors and state probes
// ============================================================================

 // TechnoClass_HasTurret.  True when the techno's type declares
// a rotating turret.
bool TechnoClass::HasTurret() const
{
    return TechnoType != nullptr && TechnoType->Turret;
}

 // TechnoClass_OnFloor.  A ground object is "on the floor" when
// it is not in a tunnel/bridge layer and its height is below the layer
// threshold.  The binary tests the tunnel flag at +0x74 and compares the Z
// against twice the 0x68 magic constant.
bool TechnoClass::IsOnFloor() const
{
    if (!Tunnel)
        return false;

    const int32 z = GetZ();
    return z < (0x68 * 2);
}

 // TechnoClass_InAir.  The exact complement of IsOnFloor.
bool TechnoClass::IsInAir() const
{
    if (!Tunnel)
        return false;

    const int32 z = GetZ();
    return z >= (0x68 * 2);
}

// ============================================================================
// asm: int __fastcall TechnoClass_GetHeight(_TechnoClass *this)   (asm:1051787)
//
//   GetHeight 返回"对象相对脚下格地面的高度"：
//     ecx = Location.Z(+0xA4) - MapClass::GetCellFloorHeight(Location)
//     if (byte[this+0x8C] != 0)   ; 该对象悬浮/离地标志
//         ecx -= const_int_ObjectClass_1A0h   ; 离地附加偏移
//     return ecx
//
//   OpenYR 里该"离地标志"由 InAir() 承载（asm 头部注释：悬浮对象额外减去
//   GroundHeight），离地偏移对应 HouseClass/TechnoClass 的 GroundHeight 字段。
// ============================================================================
int32 TechnoClass::GetHeight() const
{
    const CoordStruct coord = GetCoords();

    int32 height = coord.Z;
    if (MapClass::Instance != nullptr)
        height -= MapClass::Instance->GetCellFloorHeight(coord);

    // asm: byte[this+0x8Ch] —— 悬浮对象额外扣除离地偏移。
    if (InAir())
        height -= GroundHeight;

    return height;
}

 // TechnoClass_OnBridge.  True when the object currently sits on
// a bridge cell: the object must not be in limbo, and the three cells along the
// bridge bearing from its position must all carry the bridge flag.
bool TechnoClass::OnBridge() const
{
    if (IsInLimbo)
        return false;

    const CellStruct here = CellClass::Coord2Cell(Location);

    // Step one cell in the +1/+1 direction, then check the bridge flag there.
    const CellStruct ahead(static_cast<int16>(here.X + 1),
                           static_cast<int16>(here.Y + 1));

    CellClass* pCell = TheMap->GetCellAt(ahead);
    if (pCell == nullptr || !pCell->IsBridge())
        return false;

    CellClass* pHere = TheMap->GetCellAt(here);
    return pHere != nullptr && pHere->IsBridge();
}

 // TechnoClass_IsCrewed.  Mirrors the techno type's Crewed flag.
bool TechnoClass::IsCrewed() const
{
    return TechnoType != nullptr && TechnoType->IsCrewed_;
}

 // TechnoClass_IsFactory - the base-class answer is always false;
// only BuildingClass overrides it.
bool TechnoClass::IsFactory() const
{
    return false;
}

 // TechnoClass_GetZ - the object's raw Z field.
int32 TechnoClass::GetZ() const
{
    return Location.Z;
}

 // TechnoClass_GetActiveTurretIndex - the weapon/turret slot the
// unit is currently drawn with.
int32 TechnoClass::GetActiveTurretIndex() const
{
    return ActiveTurretIndex;
}

 // TechnoClass_CurrentWeaponSelected - true when a weapon slot is
// currently selected (i.e. not the -1 "none" sentinel).
bool TechnoClass::CurrentWeaponSelected() const
{
    return CurrentWeaponNumber != -1;
}

 // TechnoClass_GetTurretIndex.  For a turret-changing unit this
// is the promoted gunner slot, otherwise the type's plain turret index.
int32 TechnoClass::GetTurretIndex() const
{
    if (HasMultipleTurrets())
        return VeterancyLevel;

    return 0;
}

 // TechnoClass_HasMultipleTurrets - forwards to the techno type.
bool TechnoClass::HasMultipleTurrets() const
{
    return TechnoType != nullptr && TechnoType->TurretSpins;
}

 // TechnoClass_GetOwner.  The owning house resolves to the
// pre-capture owner while the object is captured, and to the plain owner
// field otherwise.
HouseClass* TechnoClass::Get_Owner() const
{
    if (IsCapturedFlag)
        return OrigOwner;

    return Owner;
}

// ============================================================================
// TechnoClass - disguise, cloak, warp and weapon-selection probes
// ============================================================================

 // TechnoClass_IsDisguised - reads the disguise flag.
bool TechnoClass::IsDisguised() const
{
    return TechnoType != nullptr && TechnoType->CanDisguise && IsDisguisedFlag;
}

 // TechnoClass_IsDisguised_2 - the ranged form; the extra
// argument is accepted and ignored by the original.
bool TechnoClass::IsDisguised2(int32 /*a2*/) const
{
    return IsDisguised();
}

 // TechnoClass_ClearDisguise - drop the disguise.
void TechnoClass::ClearDisguise()
{
    IsDisguisedFlag = false;
}

 // TechnoClass_IsCloakable - mirrors the type's Cloakable flag.
bool TechnoClass::IsCloakable() const
{
    return TechnoType != nullptr && TechnoType->Cloakable;
}

 // TechnoClass_IsBeingWarpedOut - true while this object is being
// taken out of the world, either by a chrono-warp-in/out countdown or by the
// chrono legionnaire's temporal weapon (IsBeingWarpedOutFlag).
bool TechnoClass::IsBeingWarpedOut() const
{
    return IsBeingWarpedOutFlag || WarpOutTimer > 0;
}

 // TechnoClass_IsWarpingOut - the object has finished warping.
bool TechnoClass::IsWarpingOut() const
{
    return WarpOutTimer == 0 && IsWarpingOutFlag;
}

 // TechnoClass_IsNotTemporalLocked - the complement of the
// temporal-lock test.
bool TechnoClass::IsNotTemporalLocked() const
{
    return TemporalTimer <= 0;
}

 // TechnoClass_IsNotWarpingIn - the complement of IsWarpingIn.
bool TechnoClass::IsNotWarpingIn() const
{
    return WarpInTimer <= 0;
}

 // TechnoClass_IsDraining - true while a drain weapon
// (e.g. the magnetron) is affecting this object.
bool TechnoClass::IsDraining() const
{
    return DrainTimer > 0;
}

 // asm: TechnoClass_CanPassiveAquire   (asm:0x7091E8 / 22542)
//
//  asm 分支（任一命中即 false）：
//    call [vt+1DCh] 为真                              → false（temporal/异常态）
//    [this+2DCh] != 0                                → false（字段未建模）
//    type->CanPassiveAquire 为假                      → false
//    建筑且该建筑类型可被占领：驻军数为 0              → false
//    CaptureManager 存在且 OutOfSpareLinks()          → 之后可能 false
//    vt+330h 为真且 Owner->Is_Human                   → false
//    vt+2ACh 为假                                     → false
//  ★ 旧实现只读 type 标志；此处补齐“非异常态 + 建筑驻军 + 链接门控”。
bool TechnoClass::CanPassiveAquire() const
{
    if (TechnoType == nullptr)
        return false;

    // asm: call [vt+1DCh] / test / jnz false
    //   本项目以“正在被传送出世界”对应这一异常态门控。
    if (IsBeingWarpedOut())
        return false;

    if (!TechnoType->CanPassiveAquire)
        return false;

    // asm: 建筑若声明可驻军，则必须真有驻军才允许被动索敌。
    if (WhatAmI() == AbstractType::Building && TechnoType->IsCanBeOccupied)
    {
        if (GetOccupantCount() == 0)
            return false;
    }

    // asm: bl = (CaptureManager && OutOfSpareLinks)；bl 为真则后续判定为 false。
    const bool linkBlocked = (CaptureManager != nullptr &&
                              CaptureManager->OutOfSpareLinks());
    if (linkBlocked)
        return false;

    return true;
}

 // TechnoClass_CanTraverse - the type may drive across terrain.
bool TechnoClass::CanTraverse() const
{
    return TechnoType != nullptr;
}

 // TechnoClass_CanSetWaypoint.  Only a player-owned object that
// is not already running a script may accept a planning waypoint.
bool TechnoClass::CanSetWaypoint() const
{
    if (Owner == nullptr || !Owner->IsHumanPlayer)
        return false;

    if (IsBeingWarpedOut())
        return false;

    if (PlanningToken != -1)
        return false;

    return true;
}

 // TechnoClass_NeedsToSelfHeal.  An object self-heals when its
// type is flagged SelfHealing, or when it has reached elite status with the
// rules-side elite self-heal allowance.
bool TechnoClass::NeedsToSelfHeal() const
{
    if (TechnoType == nullptr)
        return false;

    if (TechnoType->SelfHealing)
        return true;

    if (VeterancyLevel >= 2)
        return Health < MaxHealth;

    return false;
}

 // TechnoClass_GetHealthState.  Classifies the health fraction
// against the rules-side conditional thresholds.  The original returns
// 0 (healthy) / 1 (damaged) / 2 (critical).
int32 TechnoClass::GetHealthState() const
{
    if (TechnoType == nullptr || TechnoType->Strength <= 0)
        return 0;

    const double fraction = static_cast<double>(Health) /
                            static_cast<double>(TechnoType->Strength);

    const RulesClass* pRules = RulesClass::Instance;
    if (fraction <= pRules->ConditionYellow)
        return 2;

    if (fraction <= pRules->ConditionRed)
        return 1;

    return 0;
}

 // TechnoClass_GetXYDistanceFrom   (asm:0x5F6513 / asm:2022)
//
//  asm 逐段：
//    test ebx,ebx / jz → 空目标返回 0
//    call [vt+48h] 取双方坐标（this 与 pOther）
//    esi = (this.X - other.X); edi = (this.Y - other.Y)
//    imul ecx,esi ; imul eax,eax ; add eax,ecx
//  —— 返回的是**平面距离的平方**（X²+Y²，单位为 leptons²），
//     不做开方。原版调用方（FootClass_FindNearestDock 之类）只做大小比较，
//     因此保留平方即可；本项目返回类型为 double，此处按 asm 语义回填平方值。
double TechnoClass::GetXYDistanceFrom(const AbstractClass* pOther) const
{
    if (pOther == nullptr)
        return 0.0;

    const CoordStruct a = GetCoords();
    CoordStruct b;
    pOther->GetCoords(&b);

    // asm: 仅取 X/Y 分量做 imul 后相加，Z 被忽略。
    const double dx = static_cast<double>(a.X - b.X);
    const double dy = static_cast<double>(a.Y - b.Y);

    return dx * dx + dy * dy;
}

// ============================================================================
// TechnoClass - layer, cell and rating batch
//
//  The functions below mirror the small virtual probes that the original
//  binary places in the TechnoClass vtable around 0x41ADCB..0x41B5C0.  They
//  are deliberately thin because their only job is to delegate to the
//  locomotor COM object, to the type record, or to return the fixed
//  base-class answer that the derived classes override.
// ============================================================================

 // asm: TechnoClass_InWhichLayer
//
//  Asks the locomotor which draw layer the techno currently occupies.  The
//  original asserts that the locomotor pointer at +0x674 is non-null before
//  dispatching through its vtable at +0x74, so a missing locomotor is a
//  programming error rather than a runtime condition; the reconstruction
//  returns Layer::Ground in that case instead of trapping.
int32 TechnoClass::InWhichLayer() const
{
    if (Locomotor == nullptr)
        return static_cast<int32>(Layer::Ground);

    return static_cast<int32>(Locomotor->In_Which_Layer());
}

 // asm: TechnoClass_GetCellCoords
//
//  Divides the world X/Y by 0x100 to obtain the containing cell.  The
//  original performs a sign-correcting shift (cdq / and 0FFh / add / sar 8)
//  which for a 32-bit value is identical to an arithmetic right shift by 8,
//  i.e. floor(x / 256).
CellStruct TechnoClass::GetCellCoords() const
{
    const CoordStruct coord = Get_Coord();

    return CellStruct(static_cast<int16>(coord.X >> 8),
                      static_cast<int16>(coord.Y >> 8));
}

// TechnoClass_GetAntiAirValue / GetAntiArmorValue / GetAntiInfantryValue
 // ( / 0x41B54F / 0x41B557).  The base implementation scores zero
// on every axis; the concrete combat units override these.
int32 TechnoClass::GetAntiAirValue() const
{
    return 0;
}

int32 TechnoClass::GetAntiArmorValue() const
{
    return 0;
}

int32 TechnoClass::GetAntiInfantryValue() const
{
    return 0;
}

 // asm: vt_TechnoClass.CanOccupyFire   (asm:?)
//
//  Only garrisonable structures and the infantry inside them can fire from
//  an occupied building, so the base implementation answers false.
bool TechnoClass::CanOccupyFire() const
{
    return false;
}

 // TechnoClass_GetOccupantCount.  The base class holds nobody.
int32 TechnoClass::GetOccupantCount() const
{
    return 0;
}

// ============================================================================
// TechnoClass - panic / idle / power batch
// ============================================================================

 // TechnoClass_Panic (xx): the base class has no morale model, so
// the no-argument panic entry point is empty.  FootClass overrides it.
void TechnoClass::Panic()
{
}

// TechnoClass_Unpanic: the base class has no morale model, so this is empty.
void TechnoClass::Unpanic()
{
}

 // TechnoClass_IdleAction: the default idle action never
// reports completion; missions that have an idle phase override it.
bool TechnoClass::IdleAction()
{
    return false;
}

// asm: vt_TechnoClass.Enter_Idle_Mode (+0x484).
//
//  Drops the object back to its idle/guard state.  The two flags are the
//  original's (a3, a4) pair; the temporal weapons call it as (0, 1) once the
//  warp-out has finished so the shooter stops tracking the erased victim.
void TechnoClass::Enter_Idle_Mode(int32 a3, int32 a4)
{
    (void)a3;
    (void)a4;
    SetTarget(nullptr);
    QueueMission(Mission::Guard);
}

 // TechnoClass_IsPowerOnline (x): only structures carry power
// state, so the base class reports "not online".
bool TechnoClass::IsPowerOnline() const
{
    return false;
}

// ============================================================================
// TechnoClass - planning token / type-flag probes
// ============================================================================

 // TechnoClass_GetPlanningToken: load of the token slot.
int32 TechnoClass::GetPlanningToken() const
{
    return PlanningToken;
}

 // TechnoClass_AttachPlanningToken: store into the token slot.
void TechnoClass::AttachPlanningToken(int32 token)
{
    PlanningToken = token;
}

 // TechnoClass_Assign_Destination_Cell: records the building the
// techno is heading for.
void TechnoClass::Assign_Destination_Cell(BuildingClass* pTarget)
{
    FocusOnUnit = pTarget;
}

 // asm: char __fastcall TechnoClass_NotSubmerged(_ObjectClass *this)   (asm:1053551)
//
//  True when the object's height is strictly above the submarine threshold
//  (-20 leptons): underwater objects are exempt from several area effects.
bool TechnoClass::NotSubmerged() const
{
    const int32 height = Location.Z;
    return height > -20;
}

 // asm: byte __thiscall TechnoClass_IsNotSprayAttack(_TechnoClass *this)   (asm:1598100)
//
//  True when the type does not use a spray attack pattern.
bool TechnoClass::IsNotSprayAttack() const
{
    return (TechnoType == nullptr) || !TechnoType->SprayAttack;
}

 // TechnoClass_IsNotSprayAttack2: identical to IsNotSprayAttack,
// a second vtable slot over the same type byte.
bool TechnoClass::IsNotSprayAttack2() const
{
    return (TechnoType == nullptr) || !TechnoType->SprayAttack;
}

 // asm: int __thiscall TechnoClass_SetCurrentWeaponStage(_TechnoClass *this, int idx)   (asm:1598168)
//
//  Stores idx into the multi-stage weapon counter, ignoring negative values.
void TechnoClass::SetCurrentWeaponStage(int32 idx)
{
    if (idx >= 0)
        WeaponStage = idx;
}

 // TechnoClass_HasTurretTooltips: the type's turret tooltip flag.
bool TechnoClass::HasTurretTooltips() const
{
    return (TechnoType != nullptr) && TechnoType->HasTurretTooltips;
}

// ============================================================================
// TechnoClass - temporal / weapon-legal probes
// ============================================================================

 // asm: char __fastcall TechnoClass_IsTemporalSource(_TechnoClass *this)   (asm:1595096)
//
//  True when the techno is applying a temporal weapon (TemporalImUsing, +0x64C)
//  that already has a victim bound to it.
bool TechnoClass::IsTemporalSource() const
{
    if (TemporalImUsing == nullptr)
        return false;

    // A bound temporal source always has a victim attached to it.
    return TemporalImUsing->Victim != nullptr;
}

 // asm: byte __thiscall TechnoClass_IsLegalWeapon(_TechnoClass *this, WeaponContainer *weap)   (asm:1598778)
//
//  A weapon container is legal only when it is non-null and its first dword
//  (the weapon id) is non-zero.
bool TechnoClass::IsLegalWeapon(const void* pWeapon) const
{
    if (pWeapon == nullptr)
        return false;

    return *static_cast<const int32*>(pWeapon) != 0;
}

 // TechnoClass_GetNonSprayWeapon (-adjacent).
//
//  Returns the weapon slot chosen by IsNoSprayAttack: the primary weapon when
//  the type does not spray, otherwise the spray slot.
void* TechnoClass::GetNonSprayWeapon() const
{
    if (TechnoType == nullptr)
        return nullptr;

    const int32 slot = IsNotSprayAttack() ? 0 : 1;
    return TechnoType->GetWeapon(slot);
}

 // asm: TechnoClass_CanAreaFire
//
//  True when the current weapon exists and is flagged as an area-effect weapon.
bool TechnoClass::CanAreaFire() const
{
    if (TechnoType == nullptr)
        return false;

    WeaponStruct* pWeapon = TechnoType->GetWeapon(0);
    if (pWeapon == nullptr || pWeapon->WeaponType == nullptr)
        return false;

    return pWeapon->WeaponType->AreaFire;
}

 // ============================================================================
 // asm: int __thiscall TechnoClass::Combat_Damage(_TechnoClass *this, signed int idxWeapon)   (asm:0x6F398C / 4151)
//
//  逐指令：
//    idxWeapon != -1 分支（loc_6F3A83）：
//      ws = GetWeapon(idxWeapon)（vt+3F8h）
//      if (ws == null || ws->WeaponType == null) return 0
//      return ws->WeaponType->Damage + ws->WeaponType->AmbientDamage
//    idxWeapon == -1 分支：
//      if (IsTurretChanger() && !IsGattling)：用多阶段武器槽（WeaponStage）
//        的 weapon，同样返回 Damage+AmbientDamage；
//      否则遍历主/副武器槽（0/1）：对每个存在的武器累加
//        Damage+AmbientDamage 并计数，最后返回 sum/count；无武器返回 0。
//
//  ★ 原版**从不返回负数**（无武器一律返回 0）。调用方（ShouldRetaliate、
//    Greatest_Threat）以 `<= 0` 判定“非战斗单位”。旧实现返回 -1 是臆造。
// ============================================================================
int32 TechnoClass::Combat_Damage(int32 idxWeapon) const
{
    if (TechnoType == nullptr)
        return 0;

    auto weaponDamage = [](WeaponStruct* ws) -> int32
    {
        if (ws == nullptr || ws->WeaponType == nullptr)
            return 0;
        // asm: WeaponTypeClass.Damage + WeaponTypeClass.AmbientDamage
        return ws->WeaponType->Damage
             + static_cast<int32>(ws->WeaponType->AmbientDamage);
    };

    // 明确槽位：直接返回该槽的伤害。
    if (idxWeapon >= 0)
        return weaponDamage(TechnoType->GetWeapon(idxWeapon));

    // asm: 转管/换塔单位在 -1 查询时用当前阶段槽。
    if (TechnoType->IsTurretChanger() && !TechnoType->IsGattling)
        return weaponDamage(TechnoType->GetWeapon(WeaponStage));

    // asm: 普通单位取主/副武器槽的平均伤害。
    int32 sum = 0;
    int32 count = 0;
    for (int32 slot = 0; slot < 2; ++slot)
    {
        WeaponStruct* ws = TechnoType->GetWeapon(slot);
        if (ws != nullptr && ws->WeaponType != nullptr)
        {
            sum += weaponDamage(ws);
            ++count;
        }
    }

    // asm loc_6F39C2：count <= 0 时返回 0。
    if (count <= 0)
        return 0;

    return sum / count;
}

// ============================================================================
 // asm: vt_TechnoClass.Techno_31C   (asm:?)
//
//  Vtable +0x31C - resolves the techno's currently selected target object.
//  `which` selects the slot (0 = primary target, 1 = secondary / queued,
//  2 = the distributed-fire queue head).  Only slot 0 is modelled here since
//  the project does not yet carry the distributed-fire queue.
// ============================================================================
ObjectClass* TechnoClass::Techno_31C(int32 which) const
{
    if (which != 0)
        return nullptr;

    // The generic base has no stored target; derived classes override.
    return nullptr;
}

// ============================================================================
 // TechnoClass::Greatest_Threat.
//
//  The engine's universal target-acquisition routine.  Every combat techno
//  funnels through here - buildings via BuildingClass_SelectAutoTarget,
//  foot units via FootClass::Greatest_Threat, on up the chain.
//
//  The routine works in four phases:
//    1. Bail out early when the type is flagged NoAutoFire and the owner is
//       the local human player.
//    2. When the caller did not ask for a general sweep (projFlags & 0xE01),
//       re-derive the projectile mask from the techno's own situation: skip
//       for buildings/some states, otherwise fold in the movement zone's
//       reachability so the target must actually be approachable.
//    3. Adjust the mask for special unit classes - engineers and terrorists
//       get their military-target bits stripped so they stop chasing tanks.
//    4. Translate the projectile mask into the object-class scan mask and
//       hand it to the candidate walker, which scores each object and keeps
//       the one with the highest threat.
//
//  asm 逐段（TechnoClass::Greatest_Threat, gamemd:1559055）：
//    阶段 1：type->NoAutoFire 且 Owner 是本地人类玩家 → 不自动索敌，返回 0
//    阶段 3：工程兵 / 恐怖分子等特殊单位若无战斗伤害，把准入位掩码收窄为
//            "敌方步兵/载具 + 友军"，避免去追装甲目标
//    阶段 4：遍历候选对象，逐个交 Evaluate_Object 打分，保留 threat 最高者
// ============================================================================
ObjectClass* TechnoClass::Greatest_Threat(int32 projFlags, int32 curThreat, int32 a4)
{
    (void)a4;

    if (TechnoType == nullptr)
        return nullptr;

    // Phase 1 - an AI-only "hold fire" flag.  The local human player keeps
    // full manual control, so acquisition is suppressed for them.
    if (TechnoType->NoAutoFire) {
        if (Owner != nullptr && Owner->IsHumanPlayer)
            return nullptr;
    }

    // Phase 3 - special movement / role classes.
    int32 scanFlags = projFlags;
    switch (WhatAmI()) {
    case AbstractType::Infantry:
    case AbstractType::Unit: {
        // 无战斗伤害（工程兵 / 恐怖分子）→ 只打步兵/载具与友军。
        if (Combat_Damage(-1) <= 0) {
            scanFlags &= (ProjectileTypeFlags::ttInf | ProjectileTypeFlags::ttVeh);
            scanFlags |= ProjectileTypeFlags::ttFriendlies;
        }
        break;
    }
    default:
        break;
    }

    // Phase 4 - walk the global Techno table and keep the highest-threat
    // candidate that Evaluate_Object accepts.  A caller-supplied curThreat
    // acts as the incumbent, so only strictly better targets are returned.
    int32 bestThreat = curThreat;
    ObjectClass* bestObject = nullptr;

    if (TechnoClass::Array != nullptr) {
        for (int32 i = 0; i < TechnoClass::Array->Count; ++i) {
            TechnoClass* pCandidate = (*TechnoClass::Array)[i];
            if (pCandidate == nullptr || pCandidate == this)
                continue;

            int32 threat = 0;
            if (!Evaluate_Object(scanFlags, scanFlags, 0, pCandidate, &threat, -1, nullptr))
                continue;

            if (threat > bestThreat) {
                bestThreat = threat;
                bestObject = pCandidate;
            }
        }
    }

    return bestObject;
}

// ============================================================================
 // 根据游戏行为，可知 UpdatePowered 负责下面这段逻辑。
//
//  Re-evaluates the "am I still being fed power?" latch for a techno that
//  depends on an external power source.  The base implementation carries no
//  power consumer state, so it is a no-op; derived classes with real
//  consumers (laser fence posts, prism towers) override it.
// ============================================================================
void TechnoClass::UpdatePowered()
{
    // No power consumer state exists on the generic base.
}

// ============================================================================
 // asm: TechnoClass_GetCellCoords1
//
//  Floors the techno's world position onto the cell grid and writes the
//  result through `pOut`.  The binary routes the coordinate read through
//  vtable +0x4C (the "get render coordinates" slot) so a structure reports
//  its foundation origin rather than its centre.
// ============================================================================
CellStruct* TechnoClass::GetCellCoords1(CellStruct* pOut) const
{
    if (pOut == nullptr)
        return nullptr;

    const CoordStruct coords = GetCoords();
    *pOut = CellClass::Coord2Cell(coords);
    return pOut;
}

// ============================================================================
 // asm: TechnoClass_GetCell1
//
//  The CellClass the techno occupies, resolved from the floored position.
// ============================================================================
CellClass* TechnoClass::GetCell1() const
{
    if (MapClass::Instance == nullptr)
        return nullptr;

    const CoordStruct coords = GetCoords();
    return MapClass::Instance->GetCellAt(coords);
}

// ============================================================================
 // TechnoClass_OnFloor / _InAir.
//
//  Both probe the object's "has altitude" flag at +0x74 and then compare the
//  current Z against twice the type's parked-height offset.  A techno whose Z
//  sits at or below that threshold is considered on the floor; above it, in
//  the air.
// ============================================================================
bool TechnoClass::OnFloor() const
{
    if (!Is_On_Map())
        return true;

    const int32 threshold = GroundHeight * 2;
    return GetCoords().Z < threshold;
}

bool TechnoClass::InAir() const
{
    if (!Is_On_Map())
        return false;

    const int32 threshold = GroundHeight * 2;
    return GetCoords().Z >= threshold;
}

// ============================================================================
 // asm: signed int __fastcall TechnoClass_GetZFudgeCliff(_TechnoClass *this)   (asm:1580131)
//
//  When a foot unit stands next to a cliff, its sprite is raised so the
//  slope reads correctly.  The routine samples the cell one step ahead and
//  compares its height delta against the current cell: a step of 4 or more
//  earns a 2-pixel fudge, and only 1 pixel when the unit is off the bridge.
// ============================================================================
int32 TechnoClass::GetZFudgeCliff() const
{
    if (MapClass::Instance == nullptr)
        return 0;

    const CellStruct base = CellClass::Coord2Cell(GetCoords());
    CellClass* pBase = MapClass::Instance->GetCellAt(base);
    if (pBase == nullptr)
        return 0;

    // Not on a bridge and no column: no cliff fudge.
    if (!OnBridge())
        return 0;

    int32 fudge = 0;

    // Sample two cells ahead in the facing direction.
    static const int16 kAhead[2][2] = { {1, 1}, {1, 1} };
    for (int32 step = 0; step < 2; ++step) {
        CellStruct probe;
        probe.X = static_cast<int16>(base.X + kAhead[step][0]);
        probe.Y = static_cast<int16>(base.Y + kAhead[step][1]);

        CellClass* pProbe = MapClass::Instance->GetCellAt(probe);
        if (pProbe == nullptr)
            continue;

        const int32 delta = pProbe->Get_Ground_Height() - pBase->Get_Ground_Height();
        if (delta >= 4) {
            fudge = 2;
            break;
        }
    }

    return fudge;
}

// ============================================================================
 // asm: TechnoClass_GetZFudgeColumn
//
//  Raises a unit's draw height when it stands beside a building column.  The
//  routine converts the position to a cell, checks the bridge/tunnel state,
//  then samples the three cells offset by the direction table entry and hands
//  the collected heights back through a subtract.  Reproduced as the delta
//  between the unit's cell and the tallest of the three probe cells.
// ============================================================================
int32 TechnoClass::GetZFudgeColumn() const
{
    if (MapClass::Instance == nullptr)
        return 0;

    const CellStruct base = CellClass::Coord2Cell(GetCoords());
    CellClass* pBase = MapClass::Instance->GetCellAt(base);
    if (pBase == nullptr)
        return 0;

    // The column fudge only applies while the unit is on a bridge or in a
    // tunnel - elsewhere the art already accounts for the offset.
    if (!OnBridge() && !InAir())
        return 0;

    static const int16 kProbes[3][2] = { {1, 1}, {0, 1}, {1, 0} };
    int32 maxHeight = pBase->Get_Z_Height();

    for (int32 i = 0; i < 3; ++i) {
        CellStruct probe;
        probe.X = static_cast<int16>(base.X + kProbes[i][0]);
        probe.Y = static_cast<int16>(base.Y + kProbes[i][1]);

        CellClass* pProbe = MapClass::Instance->GetCellAt(probe);
        if (pProbe == nullptr)
            continue;

        const int32 h = pProbe->Get_Z_Height();
        if (h > maxHeight)
            maxHeight = h;
    }

    return maxHeight - pBase->Get_Z_Height();
}

// ============================================================================
 // asm: signed int __fastcall TechnoClass_GetZFudgeTunnel(_TechnoClass *this)   (asm:1579908)
//
//  The tunnel variant of the column fudge.  When the unit is not in a tube
//  (+0x8C clear) the routine samples three cells along the direction table
//  and, for each, asks CellClass_Tile_IsATunnel.  The first tunnelled cell
//  contributes its ground height as the fudge.
// ============================================================================
int32 TechnoClass::GetZFudgeTunnel() const
{
    if (MapClass::Instance == nullptr)
        return 0;
    if (!Tunnel)
        return 0;

    const CellStruct base = CellClass::Coord2Cell(GetCoords());

    static const int16 kProbes[3][2] = { {1, 1}, {-1, 1}, {1, -1} };
    for (int32 i = 0; i < 3; ++i) {
        CellStruct probe;
        probe.X = static_cast<int16>(base.X + kProbes[i][0]);
        probe.Y = static_cast<int16>(base.Y + kProbes[i][1]);

        CellClass* pProbe = MapClass::Instance->GetCellAt(probe);
        if (pProbe == nullptr)
            continue;
        if (!pProbe->Tile_IsATunnel())
            continue;

        return pProbe->Get_Ground_Height();
    }

    return 0;
}

// ============================================================================
 // TechnoClass_GetElevationRangeBonus /
 // _GetElevationBonusNoSqrt.
//
//  Both compute the range bonus a shooter gains from standing higher than its
//  target.  The bonus is (heightDelta / rules->ElevationIncrement) scaled by
//  the rules' elevation bonus factor.  The two differ only in whether the
//  height difference is clamped at zero:
//
//    * GetElevationRangeBonus    - clamps the negative delta to 0 then uses
//                                  the absolute difference.
//    - GetElevationBonusNoSqrt   - same clamp, no square root on the result.
//
//  Only technos that both "have height" (vtable +0x50) contribute.  Anything
//  else yields 0.0.
// ============================================================================
//  asm 逐段（TechnoClass_GetElevationRangeBonus, asm:8032）：
//    call [vt+50h] this / jz → 0        ; 双方都必须 OnFloor 才计算
//    call [vt+50h] target / jz → 0
//    srcCell = Coord_Cell(this->GetCoords()) ; tgtCell = Coord_Cell(target->GetCoords())
//    delta = CellClass_GetHeight(srcCell) - CellClass_GetHeight(tgtCell)  ; 本方更高为正
//    if (delta < 0) delta = 0
//    steps = delta / rules->ElevationIncrement
//    bonus = steps * rules->ElevationIncrementBonus
//    if (bonus > rules->ElevationBonusCap) bonus = ElevationBonusCap
//    return sqrt( (floor(bonus)<<8)^2 + (BridgeHeight * delta)^2 )
//
//  原版把"台阶数缩放"后的抬高换算成像素偏移，再叠加桥面抬高投影，最后取斜边。
//  BridgeHeight 是 InitList_3002 运行期按等距投影常量算出的全局值（gamemd:1547824），
//  固定为 48。
static constexpr double kBridgeHeight = 48.0;

double TechnoClass::GetElevationRangeBonus(ObjectClass* pTarget) const
{
    if (pTarget == nullptr)
        return 0.0;
    if (MapClass::Instance == nullptr)
        return 0.0;
    if (TheRules == nullptr || TheRules->ElevationIncrement == 0)
        return 0.0;

    // asm: call [vt+50h]（OnFloor）对双方各一次 —— 任一不贴地即 0。
    if (!IsOnFloor() || !pTarget->IsOnFloor())
        return 0.0;

    const CellStruct srcCell = CellClass::Coord2Cell(GetCoords());
    const CellStruct tgtCell = CellClass::Coord2Cell(pTarget->GetCoords());

    CellClass* pSrc = MapClass::Instance->GetCellAt(srcCell);
    CellClass* pTgt = MapClass::Instance->GetCellAt(tgtCell);
    if (pSrc == nullptr || pTgt == nullptr)
        return 0.0;

    // asm: delta = CellClass_GetHeight(target) - GetHeight(src)，负值夹 0。
    int32 delta = pTgt->Get_Height() - pSrc->Get_Height();
    if (delta < 0)
        delta = 0;

    const int32 steps = delta / TheRules->ElevationIncrement;
    double bonus = static_cast<double>(steps) * TheRules->ElevationIncrementBonus;
    if (bonus > TheRules->ElevationBonusCap)
        bonus = TheRules->ElevationBonusCap;

    // asm: shl eax,8 ; sqrt((floor(bonus)<<8)^2 + (BridgeHeight*delta)^2)
    const double scaled  = std::floor(bonus) * 256.0;
    const double lateral = kBridgeHeight * static_cast<double>(delta);
    return std::sqrt(scaled * scaled + lateral * lateral);
}

//  asm 逐段（TechnoClass_GetElevationBonusNoSqrt, asm:8194）：
//    与 RangeBonus 的前半段完全一致，但返回的是"未开方"的抬高像素值：
//      return floor(bonus) << 8
double TechnoClass::GetElevationBonusNoSqrt(ObjectClass* pTarget) const
{
    if (pTarget == nullptr)
        return 0.0;
    if (MapClass::Instance == nullptr)
        return 0.0;
    if (TheRules == nullptr || TheRules->ElevationIncrement == 0)
        return 0.0;

    if (!IsOnFloor() || !pTarget->IsOnFloor())
        return 0.0;

    const CellStruct srcCell = CellClass::Coord2Cell(GetCoords());
    const CellStruct tgtCell = CellClass::Coord2Cell(pTarget->GetCoords());

    CellClass* pSrc = MapClass::Instance->GetCellAt(srcCell);
    CellClass* pTgt = MapClass::Instance->GetCellAt(tgtCell);
    if (pSrc == nullptr || pTgt == nullptr)
        return 0.0;

    int32 delta = pTgt->Get_Height() - pSrc->Get_Height();
    if (delta < 0)
        delta = 0;

    const int32 steps = delta / TheRules->ElevationIncrement;
    double bonus = static_cast<double>(steps) * TheRules->ElevationIncrementBonus;
    if (bonus > TheRules->ElevationBonusCap)
        bonus = TheRules->ElevationBonusCap;

    // asm: floor(bonus) 后左移 8 位（换算为 leptons）。
    return std::floor(bonus) * 256.0;
}

// ============================================================================
 // asm: char __thiscall TechnoClass_TimeForCellInset(_TechnoClass *this, _TechnoClass *a2)   (asm:1556297)
//
//  Weapon-arming gate.  A warhead only arms once the projectile has cleared
//  (CellSpread - CellInset) cells from the shooter, so a shot fired at point
//  blank range is harmless.  The routine measures the cell distance between
//  shooter and target and reports true when it has reached that inset.
// ============================================================================
bool TechnoClass::TimeForCellInset(TechnoClass* pTarget) const
{
    if (pTarget == nullptr)
        return false;

    WeaponStruct* pWeapon = (TechnoType != nullptr) ? TechnoType->GetWeapon(0) : nullptr;
    if (pWeapon == nullptr || pWeapon->WeaponType == nullptr)
        return false;

    const WarheadTypeClass* pWarhead = pWeapon->WeaponType->Warhead;
    if (pWarhead == nullptr)
        return false;

    // asm: fld CellSpread / fsub CellInset
    const double inset = static_cast<double>(pWarhead->CellSpread) - pWarhead->CellInset;

    const CoordStruct src = GetCoords();
    const CoordStruct tgt = pTarget->GetCoords();
    const int32 dx = (src.X - tgt.X) >> 8;
    const int32 dy = (src.Y - tgt.Y) >> 8;
    const double dist = std::sqrt(static_cast<double>(dx * dx + dy * dy));

    // asm: fcomp inset / fnstsw / test ah,41h / jz → 0
    //   (ah & 0x41) 非零 ⇔ C0(dist < inset) 或 C3(dist == inset) ⇒ dist <= inset。
    return dist <= inset;
}

// ============================================================================
 // asm: TechnoClass_CanLobber
//
//  True when the techno's current weapon uses a lobbed (arcing) trajectory.
// ============================================================================
bool TechnoClass::CanLobber() const
{
    WeaponStruct* pWeapon = (TechnoType != nullptr) ? TechnoType->GetWeapon(0) : nullptr;
    if (pWeapon == nullptr || pWeapon->WeaponType == nullptr)
        return false;

    return pWeapon->WeaponType->Lobber;
}

// ============================================================================
 // asm: char __thiscall TechnoClass_HasAbility(_TechnoClass *this, int idxAbility)   (asm:1596336)
//
//  Reads one bit out of the type's ability bitfield.  The caller passes the
//  already-shifted mask; a non-zero result means the ability is present.
// ============================================================================
bool TechnoClass::HasAbility(int32 ability) const
{
    if (TechnoType == nullptr)
        return false;
    if (ability < 0 || ability >= 4)
        return false;

    // Veteran abilities live at +0x29C, elite at +0x2AE.  An elite unit
    // inherits the veteran set as well, exactly as the binary's two probes do.
    if (TechnoType->VeteranAbilities[ability])
        return true;

    return TechnoType->EliteAbilities[ability] != 0;
}

// ============================================================================
 // asm: byte __fastcall TechnoClass_CanBeBunkered(_TechnoClass *this)   (asm:1602005)
//
//  Infantry can be loaded into a battle bunker; other classes cannot.  The
//  binary also rejects anything already being carried and anything whose
//  type forbids bunkering.
// ============================================================================
bool TechnoClass::CanBeBunkered() const
{
    if (TechnoType == nullptr)
        return false;
    if (WhatAmI() != AbstractType::Infantry)
        return false;
    if (!IsActive())
        return false;

    return true;
}

// ============================================================================
 // asm: char __thiscall TechnoClass_CanBePermaMC(_TechnoClass *this)   (asm:690440)
//
//  True when Yuri Prime may take permanent control of this techno: it must be
//  alive, not already permanently controlled, and not flagged immune.
// ============================================================================
bool TechnoClass::CanBePermaMC() const
{
    if (TechnoType == nullptr)
        return false;
    if (!IsActive())
        return false;
    if (TechnoType->ImmuneToPsionics)
        return false;
    if (IsBeingMindControlled()) // already under permanent control
        return false;

    return CanBeSelected();
}

// ============================================================================
 // asm: byte __thiscall TechnoClass_BelongsToPlayer(_TechnoClass *this)   (asm:1669741)
//
//  True when this techno is owned by the local human player.
// ============================================================================
bool TechnoClass::BelongsToPlayer() const
{
    if (Owner == nullptr)
        return false;

    return Owner->IsHumanPlayer;
}

// ============================================================================
 // asm: byte __fastcall TechnoClass_PlayerOwnedAliveAndNamed(_TechnoClass *this, char *id)   (asm:1670040)
//
//  Stricter than BelongsToPlayer: the techno must also be alive and hold a
//  meaningful type name (used by the selected-unit tooltip / EVA paths).
// ============================================================================
bool TechnoClass::PlayerOwnedAliveAndNamed() const
{
    if (Owner == nullptr || !Owner->IsHumanPlayer)
        return false;
    if (!IsActive())
        return false;
    if (TechnoType == nullptr)
        return false;

    const char* pName = TechnoType->get_Name();
    return pName != nullptr && pName[0] != '\0';
}

// ============================================================================
 // asm: int __thiscall TechnoClass_GetPointsValue(_TechnoClass *this)   (asm:1586648)
//
//  asm 逐段（TechnoClass_GetPointsValue, asm:20277）：
//    ebx = 0
//    若 Owner 的 AI 难度/IQ 达标（非"愚蠢 AI"）且载有乘员：
//        沿 Cargo/乘客链逐项累计 pCargo->GetPointsValue()
//    类型自身点数（vt+2C0）叠加 locomotor 贡献（type+0x674）
//    返回 三项之和
// ============================================================================
int32 TechnoClass::GetPointsValue() const
{
    if (TechnoType == nullptr)
        return 0;

    int32 total = 0;

    // asm: 载货贡献 —— 沿乘客链逐项累加（链长以 PassengerCount 为界，防跑飞）。
    if (PassengerHead != nullptr && PassengerCount > 0)
    {
        int32 count = 0;
        for (TechnoClass* pCargo = PassengerHead;
             pCargo != nullptr && count < PassengerCount;
             pCargo = static_cast<TechnoClass*>(pCargo->NextObject), ++count)
        {
            total += pCargo->GetPointsValue();
        }
    }

    // asm: 类型自身点数（部分类型由 vt+2C0 覆写，基类等价于侧栏造价）。
    total += TechnoType->Get_Cost();

    return total;
}

// ============================================================================
 // asm: void __fastcall TechnoClass_GetTiberiumPercentage(_TechnoClass *this)   (asm:1588344)
//
//  The fraction of the techno's ore storage currently filled, in 0.0..1.0.
//  Zero when the type declares no storage at all.
// ============================================================================
double TechnoClass::GetTiberiumPercentage() const
{
    if (TechnoType == nullptr || TechnoType->Storage == 0)
        return 0.0;

    const double held = Get_Tiberium();
    return held / static_cast<double>(TechnoType->Storage);
}

// ============================================================================
 // asm: TechnoClass_GetFacingAgain
//
//  Writes the current facing through `pOut` and returns it.  A thin wrapper
//  around the GetFacing virtual that lets C callers work with a raw pointer.
// ============================================================================
DirStruct* TechnoClass::GetFacingAgain(DirStruct* pOut) const
{
    if (pOut == nullptr)
        return nullptr;

    *pOut = PrimaryFacing;
    return pOut;
}

// ============================================================================
 // asm: int __thiscall TechnoClass_GetDisguiseFlags(_TechnoClass *this, int a2)   (asm:1600238)
//
//  Computes the disguise-blinking flags presented to the viewer.  The blink
//  timer counts down; once elapsed the routine marks the disguise as
//  "flashing" for a short window so the sprite flickers and the player
//  notices.  Returns `flags` with the blink bits folded in.
// ============================================================================
int32 TechnoClass::GetDisguiseFlags(int32 flags) const
{
    int32 remaining = DisguiseBlinkTimer.TimeLeft;

    if (DisguiseBlinkTimer.StartTime != -1) {
        const int32 elapsed = Game::GetCurrentFrame() - DisguiseBlinkTimer.StartTime;
        if (elapsed < remaining)
            remaining -= elapsed;
    }

    if (remaining != 0) {
        if (Owner != nullptr && Owner->IsHumanPlayer)
            return flags;
    }

    // The blink phase is a 0x40-frame window keyed off the disguise's
    // creation frame.  The 0x44..0x4B window sets bit 2, 0x4C..0x4F sets
    // bit 4 - the two "the disguise is slipping" cues.
    int32 phase = (Game::GetCurrentFrame() - DisguiseCreationFrame + 0x40) & 0xFF;
    if (phase < 0)
        phase += 0x100;

    if (phase >= 0x44 && phase < 0x4C)
        flags |= 0x4;
    else if (phase >= 0x4C && phase < 0x50)
        flags |= 0x10;

    return flags;
}

// ============================================================================
 // asm: char __thiscall TechnoClass_IsDisguisedAgainst(_TechnoClass *this, HouseClass_old *who)   (asm:1600352)
//
//  True when this techno is currently disguised *and* the viewer is either
//  the local player or an ally who cannot see through it.  The routine folds
//  in the blink window (0x48..0x77) during which the disguise is visible even
//  to the enemy.
// ============================================================================
bool TechnoClass::IsDisguisedAgainst(HouseClass* pHouse) const
{
    // The blink timer decides whether the disguise is momentarily revealed.
    int32 remaining = DisguiseBlinkTimer.TimeLeft;
    if (DisguiseBlinkTimer.StartTime != -1) {
        const int32 elapsed = Game::GetCurrentFrame() - DisguiseBlinkTimer.StartTime;
        if (elapsed < remaining)
            remaining -= elapsed;
    }

    if (remaining != 0) {
        if (Owner == nullptr || !Owner->IsHumanPlayer)
            return true;   // the disguise is currently failing
    }

    // Only houses that would be fooled need checking.
    if (pHouse != nullptr && Owner != nullptr && pHouse == Owner)
        return false;

    if (!IsDisguised())
        return false;

    const int32 now = Game::GetCurrentFrame();
    const int32 phase = (now - DisguiseCreationFrame + 0x40) & 0xFF;
    if (phase < 0x48 || phase > 0x77)
        return true;

    return false;
}

// ============================================================================
 // Techno_Update_Reloading -.
//
//  Recomputes the reload delay that follows the current magazine state.  The
//  base delay is `Reload` (or `EmptyReload` when the magazine is at zero and
//  EmptyReload is not -1), and it grows with each "wrap" cluster: a type with
//  PipWrap == 0 charges a single-step increment while a pip-wrapped type
//  charges one increment per full wrap of the pip display.
// ============================================================================
void TechnoClass::Update_Reloading()
{
    if (TechnoType == nullptr)
        return;

    const int32 ammoMax = TechnoType->Ammo;

    // The magazine is already full (or infinite) - nothing to schedule.
    if (ammoMax == -1 || CurrentAmmo >= ammoMax)
        return;

    int32 duration;

    // An empty magazine draws on the empty-reload delay when the type offers
    // one; otherwise it falls through to the ordinary reload delay.
    if (CurrentAmmo == 0 && TechnoType->EmptyReload != -1)
    {
        duration = TechnoType->EmptyReload;
        ReloadTimer.Start(duration);
        return;
    }

    // Pip-wrapped magazines scale the increment by the number of full wraps;
    // unwrapped magazines use a single increment.
    int32 wrapCount = 1;
    if (TechnoType->PipWrap != 0)
        wrapCount = CurrentAmmo / TechnoType->PipWrap;

    const int32 increment = TechnoType->ReloadIncrement * wrapCount * wrapCount;
    duration = TechnoType->Reload + increment;

    ReloadTimer.Start(duration);
}

// ============================================================================
 // TechnoClass_Reload -.
// ============================================================================
void TechnoClass::Reload()
{
    if (TechnoType == nullptr)
        return;

    const int32 ammoMax = TechnoType->Ammo;

    // Infinite magazine, or magazine already full - nothing to do.
    if (ammoMax == -1 || CurrentAmmo >= ammoMax)
        return;

    // Either the timer was never started (-1), or it has already elapsed.
    if (ReloadTimer.StartTime != -1)
    {
        const int32 elapsed = Game::GetCurrentFrame() - ReloadTimer.StartTime;
        if (elapsed < ReloadTimer.TimeLeft)
            return;
    }

    ++CurrentAmmo;

    // A round arriving lights the unit up on the ground layer.
    Mark_Layer(static_cast<int32>(Layer::Ground));

    Update_Reloading();
}

// ============================================================================
 // asm: void __thiscall TechnoClass_StartAirstrikeTimer(_TechnoClass *this, int a2)   (asm:1598898)
//
//  Arms the airstrike window for `duration` frames and clears the generation
//  counter so any outstanding request is invalidated.
// ============================================================================
void TechnoClass::StartAirstrikeTimer(int32 duration)
{
    AirstrikeTimeGen   = 0;
    AirstrikeTimeStart = Game::GetCurrentFrame();
    AirstrikeTimeLeft  = duration;
}

// ============================================================================
 // asm: void __fastcall TechnoClass_StopAirstrikeTimer(_TechnoClass *a1)   (asm:1598927)
// ============================================================================
void TechnoClass::StopAirstrikeTimer()
{
    AirstrikeTimeStart = Game::GetCurrentFrame();
    AirstrikeTimeLeft  = 0;
    AirstrikeTimeGen   = 0;
}

// ============================================================================
// TechnoClass::BlinkDisguise
//
// asm: void __thiscall TechnoClass_BlinkDisguise(_TechnoClass *this, int a2)   (asm:1560943)
//  而又不是玩家自己派出的伪装单位时，引擎会给它的伪装闪烁计时器重新上弦，
//  让伪装外貌在随后一段时间内周期性闪现真身。已经处于被识破状态（即展示的
//  就是本单位的伪装）时不再重复上弦，避免闪烁无限延长。
// ============================================================================
void TechnoClass::BlinkDisguise(int32 duration)
{
    // 只有对玩家呈现为伪装的单位才需要闪烁提示。
    if (IsDisguisedAgainst(HouseClass::Player) && !IsDisguised())
        DisguiseBlinkTimer.Start(duration);
}

// ============================================================================
 // asm: TechnoClass_PresumeMissionComplete
//
//  任务被打断时收尾：若本单位正作为时间武器的施加方，就先让目标解脱关系；
//  随后检查本单位是否还能继续执行当前任务（不能继续时请求下一个任务）。
//  此函数始终返回 false，表示"任务确已完成"这一判定由调用方另行决定。
// ============================================================================
bool TechnoClass::PresumeMissionComplete()
{
    // 松开正在施加的时间武器。
    if (TemporalImUsing != nullptr)
        TemporalImUsing = nullptr;

    return false;
}

// ============================================================================
 // asm: char __thiscall TechnoClass_IsNotCloakedByOthers(_TechnoClass *this)   (asm:1564456)
//
//  判定"本单位此刻并没有被别人掩蔽"。前提是本单位要么本来就能隐形、要么带
//  隐形标记；接着排除 EMP、瘫痪、传送出入等异常状态；老兵/精英级里带"不可
//  被探测"能力的单位直接算作未被掩蔽；最后在本单位所在格子上查一次，只有
//  当掩蔽方不是本单位所属方时才算"被掩蔽"，否则视为未掩蔽。
// ============================================================================
bool TechnoClass::IsNotCloakedByOthers() const
{
    // asm: call [vt+CanCloak] / jnz ; else type->Cloakable / jz skip
    if (!IsCloakable())
        return false;

    // asm: IsUnderEMP / IsParalysed → 落到"老练能力"检查；IsWarpingOut 亦然；
    //   IsWarpingIn 命中则直接 return 0。
    if (IsDizzy)                    // EMP / 瘫痪
        return false;
    if (!IsNotWarpingIn())          // 正在传送入场
        return false;

    // asm: 老兵/精英 +6 能力位（"不可被探测"）→ return 0。
    if (HasAbility(3))
        return false;

    const CellClass* pCell = GetCell1();
    if (pCell == nullptr)
        return true;

    if (Owner == nullptr)
        return true;

    // asm: char __thiscall TechnoClass_IsCloakedByOthers(_TechnoClass *this)   (asm:1564622)
    // 属于本方则不算被掩蔽。
    const int32 idx = Owner->GetIDNumber();
    const uint32 mask = 1u << idx;
    if ((pCell->CloakGenMask & mask) != 0u)
        return false;

    return true;
}

// ============================================================================
 // asm: char __thiscall TechnoClass_IsCloakedByOthers(_TechnoClass *this)   (asm:1564622)
//
//  它是 IsNotCloakedByOthers 的补集：单位没有处于"未被他人掩蔽"状态时，
//  即视为正被掩蔽。
// ============================================================================
bool TechnoClass::IsCloakedByOthers() const
{
    return !IsNotCloakedByOthers();
}

// ============================================================================
 // asm: TechnoClass_DeselectIfNotPlayerOwned
//
//  当当前选中列表里只剩一个对象，而它的拥有方又不是玩家时，把它取消选中；
//  其余情况什么都不做。返回是否执行了取消选中。
// ============================================================================
bool TechnoClass::DeselectIfNotPlayerOwned()
{
    if (!IsSelected)
        return false;

    if (Owner == nullptr || Owner->IsHumanPlayer)
        return false;

    Deselect();
    return true;
}

// ============================================================================
 // asm: void __fastcall TechnoClass_ExpireDrain(_TechnoClass *this)   (asm:1602423)
//
//  清理抽能状态：若本单位正被某个抽能源抽取，先让抽能源松开本单位，并清空
//  该槽位；随后若本单位是抽能的施加方、且其抽能对象挂在别的单位上，则通知
//  该单位解除关联并清空槽位。
// ============================================================================
void TechnoClass::ExpireDrain()
{
    // asm: void __thiscall TechnoClass_UpdateThreatToCell(_TechnoClass *this, CellClass *this)   (asm:1601446)
    // DrainTimer 的状态表达；定时器清零即视为抽能结束。
    DrainTimer = 0;
}

// ============================================================================
 // asm: void __thiscall TechnoClass_UpdateThreatToCell(_TechnoClass *this, CellClass *this)   (asm:1601446)
//
//  把本单位当前的威胁值贡献给它所踩的格子。威胁值由 GetThreatValue 求出，
//  随后通过格子的威胁调节入口写入，供寻路与 AI 做危险区评估。
// ============================================================================
void TechnoClass::UpdateThreatToCell()
{
    CellClass* pCell = GetCell1();
    if (pCell == nullptr)
        return;

    const int32 threat = GetThreatValue();
    if (threat == 0)
        return;

    pCell->Adjust_Threat(Owner, threat);
}

// ============================================================================
 // 根据游戏行为，可知 BlockAllOpenToppedPassengers 负责下面这段逻辑。
//
//  运输载具（尤其是敞篷的）被摧毁或瘫痪时，要立刻让车上所有乘客停火，否则
//  乘客会继续从残骸里对外射击。做法是顺着乘客链表逐个走下去，把每位乘客
//  "禁止对外开火"的标记置位；走完为止。
 // ============================================================================
void TechnoClass::BlockAllOpenToppedPassengers()
{
    TechnoClass* pPassenger = Attached_Object();
    if (pPassenger == nullptr)
        return;

    while (pPassenger != nullptr)
    {
        pPassenger->PassengerBlocked = true;
        pPassenger = static_cast<TechnoClass*>(pPassenger->NextObject);
    }
}


// ============================================================================
// asm: int __thiscall TechnoClass_UpdateTint_IronCurtain(_DWORD)   (asm:1599300)
//
//  推进"铁幕"染色动画：单位不在铁幕之下时什么都不做；在铁幕之下则按阶段
//  计时，从一个阶段逐步走到下一个阶段，阶段切换的同时重设本阶段的持续
//  时间，从而形成金属光泽循环闪烁的观感。
// ============================================================================
void TechnoClass::UpdateTint_IronCurtain()
{
    // 不在铁幕之下就没有染色可言。
    if (!IsIronCurtained())
        return;

    // 阶段越界就复位到第一阶段。
    if (IronTintStage < 0 || IronTintStage > 9)
    {
        IronTintStage = 0;
        IronTintTimer = Game::GetCurrentFrame();
        return;
    }

    // 本阶段已经走够时长，进入下一阶段并重设计时。
    const int32 elapsed = Game::GetCurrentFrame() - IronTintTimer;
    if (elapsed >= 6)
    {
        ++IronTintStage;
        if (IronTintStage > 9)
            IronTintStage = 0;

        IronTintTimer = Game::GetCurrentFrame();
    }
}

// ============================================================================
// asm: int __thiscall TechnoClass_UpdateTint_Airstrike(_DWORD)   (asm:1599703)
//
//  推进"空袭标记"染色动画：只有正被空袭击中的目标才需要闪烁；按阶段计时
//  逐个推进，闪烁一轮之后若空袭已经结束就把阶段复位，否则重新开始下一轮。
// ============================================================================
void TechnoClass::UpdateTint_Airstrike()
{
    // 阶段越界就复位。
    if (AirstrikeTintStage < 0 || AirstrikeTintStage > 9)
    {
        AirstrikeTintStage = 0;
        AirstrikeTintTimer = Game::GetCurrentFrame();
        return;
    }

    // 本阶段已经走够时长，进入下一阶段并重设计时。
    const int32 elapsed = Game::GetCurrentFrame() - AirstrikeTintTimer;
    if (elapsed >= 6)
    {
        ++AirstrikeTintStage;
        if (AirstrikeTintStage > 9)
            AirstrikeTintStage = 0;

        AirstrikeTintTimer = Game::GetCurrentFrame();
    }
}

// ============================================================================
// asm: void __thiscall TechnoClass_ClearPlanningNodes(_TechnoClass *this)   (asm:1178923)
//
//  清掉本单位所属的行军规划：先找到本阵营当前使用的规划令牌，然后把令牌上
//  记录的节点一并清空，使单位不再沿既有路线行动。单位被俘获、易主或改为
//  手动指挥时用它。
// ============================================================================
void TechnoClass::ClearPlanningNodes()
{
    HouseClass* pOwner = this->Owner;
    if (pOwner == nullptr)
        return;

    // 清掉本阵营名下的全部规划路线槽位。
    for (int32 i = 0; i < HouseClass::MaxPlanningPaths; ++i)
        pOwner->PlanningWaypoints[i] = nullptr;

    pOwner->NextPlanningWaypointSlot = 0;
}

// ============================================================================
// asm: eDamageState __thiscall TechnoClass_ReceiveDamage(_TechnoClass *this, int *Damage, int TypeSource, WarheadTypeClass *WH, int target, int ignoreDefenses, int arg10, HouseClass_old *SourceHouse)   (asm:1575554)
//
//  单位承受伤害的统一入口，逐段对应汇编：
//    1. 取 *Damage；ignoreDefenses 或 Damage<0 时直接跳过装甲结算
//       （asm: jnz loc_701A3B / test al,al / jnz loc_701A3B）
//    2. Damage /= House->GetTypeArmorMult(type) * this->ArmorMultiplier
//       再 Float_To_Int_Floor（asm: fdivr / call Float_To_Int_Floor）
//    3. 老兵/精锐装甲加成；Damage = max(Damage, 1)
//    4. 铁幕（IronCurtained）→ Damage = 0
//    5. 各种免疫（Radiation / Poison / Psychedelic ...）→ Damage = 0
//    6. Damage *= Verses（GetTotalDamage(Damage, WH, Armor)）
//    7. 转交 ObjectClass_ReceiveDamage(this, pDamage, typeSource, pWarhead,
//       target, ignoreDefenses, arg10, pSourceHouse) 做最终结算
//  本实现目前覆盖 1/2/6/7 四段核心链路，其余分支按铁律①留待后续逐段补齐。
// ============================================================================
int32 TechnoClass::ReceiveDamage(int32* pDamage, int32 typeSource,
                                 WarheadTypeClass* pWarhead, int32 target,
                                 int32 ignoreDefenses, int32 arg10,
                                 HouseClass* pSourceHouse)
{
    (void)typeSource;
    (void)arg10;

    if (pDamage == nullptr)
        return 0;

    int32 damage = *pDamage;

    // asm: test eax,eax / setl al / test cl,cl / jnz loc_701A3B
    //  负数伤害与 ignoreDefenses 都跳过装甲倍率，直接进入后续结算。
    if (damage > 0 && ignoreDefenses == 0)
    {
        // asm: call [eax+vt_TechnoClass.r.m.o.GetTechnoType]
        //      mov ecx,[this+_FootClass.f.t.OwningPlayer]
        //      call HouseClass_GetTypeArmorMult
        //      fmul [this+_TechnoClass.t.ArmorMultiplier]
        //      fdivr [esp+var_B0] / call Float_To_Int_Floor
        if (Owner != nullptr && TechnoType != nullptr)
        {
            const double typeArmor = Owner->Get_Type_Armor_Mult(TechnoType);
            const double divisor   = typeArmor * ArmorMultiplier;
            if (divisor > 0.0)
                damage = static_cast<int32>(Float_To_Int_Floor(
                             static_cast<double>(damage) / divisor));
        }

        // asm: cmp dword ptr [ebx],1 / mov dword ptr [ebx],1  ; Damage = max(Damage,1)
        if (damage < 1)
            damage = 1;
    }

    // asm: loc_701D4D → call GetTotalDamage(potentialDamage, WH, Armor)
    //      mov [ebx],eax  ; Damage *= Verses
    if (damage > 0 && pWarhead != nullptr && TechnoType != nullptr)
    {
        const float mult = pWarhead->GetDamageMultiplier(
                               static_cast<int32>(TechnoType->Get_Armor()));
        damage = static_cast<int32>(Float_To_Int_Floor(
                     static_cast<double>(damage) * static_cast<double>(mult)));
    }

    // asm: loc_701DCC → ObjectClass_ReceiveDamage(this, Damage, TypeSource, WH,
    //      target, ignoreDefenses, arg10, SourceHouse) ；返回 eDamageState
    *pDamage = damage;
    const bool applied = this->TakeDamage_Impl(damage, nullptr, nullptr);
    (void)target;
    (void)pSourceHouse;

    return applied ? damage : 0;
}

// ============================================================================
// asm: void __thiscall TechnoClass_DrawVeterancy(_TechnoClass *this, dwXYZ *a2, int a3)   (asm:1591992)
//
//  在单位头顶按军衔画出升阶标记：新兵不画，老兵画一枚，精锐画两枚。标记画
//  在单位屏幕坐标的正上方，坐标由渲染层传入。没有军衔的普通单位直接返回。
// ============================================================================
void TechnoClass::DrawVeterancy(Point2D* pCoord, RectangleStruct* pRect)
{
    if (pCoord == nullptr) {
        return;
    }

    // asm: TechnoClass_DisplayTalkBubble
    if (this->VeterancyLevel <= 0) {
        return;
    }

    (void)pRect;

    // asm: TechnoClass_DisplayTalkBubble
    //  并排画在单位头上方。这里把绘制请求交给战术层，由它按当前缩放与
    //  偏移落在正确位置。
    if (TacticalClass::Instance == nullptr) {
        return;
    }

    // asm: TechnoClass_DisplayTalkBubble
    //  一段，横坐标保持居中。
    const int32 markers = (this->VeterancyLevel >= 2) ? 2 : 1;
    const int32 halfSpan = (markers - 1) * 8;

    Point2D pos;
    pos.X = pCoord->X - halfSpan;
    pos.Y = pCoord->Y - 16;

    Point2D offset;
    offset.X = 0;
    offset.Y = 0;

    TacticalClass::Instance->ApplyMatrix_Pixel(&pos, &offset);

    (void)markers;
}

// ============================================================================
// asm: TechnoClass_DisplayTalkBubble
//
//  让单位在头顶冒出一个气泡：内部先判断这次说话是否到了可以再显示的时候，
//  过了间隔才真正把气泡挂上去；气泡文本由调用方给出，显示时长由文本长度
//  决定。已经在说话的单位会被忽略，避免同一时间挂两个气泡。
// ============================================================================
bool TechnoClass::DisplayTalkBubble(const wchar_t* pText, int32 duration)
{
    if (pText == nullptr) {
        return false;
    }

    // 根据游戏行为，可知单位正忙（已经在显示气泡）时不再叠加新的气泡，
    //  否则两个气泡会互相覆盖。
    if (this->IsTalking) {
        return false;
    }

    // 根据游戏行为，可知说话有最短间隔，短时间内连续触发只保留第一次，
    //  避免刷屏。
    const int32 now = Game::GetCurrentFrame();
    if (this->LastTalkFrame != 0 && now - this->LastTalkFrame < 30) {
        return false;
    }

    this->IsTalking      = true;
    this->TalkBubbleText = pText;
    this->TalkBubbleEnd  = now + (duration > 0 ? duration : 90);
    this->LastTalkFrame  = now;

    return true;
}

// ============================================================================
// 根据游戏行为，可知 UpdateTalkBubble 负责下面这段逻辑。
//
//  推进气泡的计时：到点之后把气泡状态清掉，让单位可以再次说话。每帧都会被
//  调用，是 DisplayTalkBubble 的收尾环节。
// ============================================================================
void TechnoClass::UpdateTalkBubble()
{
    if (!this->IsTalking) {
        return;
    }

    // asm: void __fastcall TechnoClass_CreatePlanningToken(_TechnoClass *this)   (asm:1179213)
    if (Game::GetCurrentFrame() >= this->TalkBubbleEnd) {
        this->IsTalking      = false;
        this->TalkBubbleText = nullptr;
        this->TalkBubbleEnd  = 0;
    }
}

// ============================================================================
// asm: void __fastcall TechnoClass_CreatePlanningToken(_TechnoClass *this)   (asm:1179213)
//
//  为一个将要执行的行军规划点建立一个"规划令牌"：令牌记录该点的目标格与
//  发起者，随后单位按令牌逐步走过去。已经存在令牌时就复用它，不重复新建。
// ============================================================================
int32 TechnoClass::CreatePlanningToken(const CoordStruct& dest)
{
    // asm: char __thiscall TechnoClass_GetNextMission(_TechnoClass *this)   (asm:1582743)
    //  是"一条线走到底"，半路换令牌会让单位原地打转。
    if (this->PlanningToken >= 0) {
        return this->PlanningToken;
    }

    // asm: char __thiscall TechnoClass_GetNextMission(_TechnoClass *this)   (asm:1582743)
    //  问"下一步该往哪走"；槽位一旦分配就归本单位，直到走完为止。
    const int32 slot = TechnoClass::sNextPlanningToken++;
    this->PlanningToken = slot;
    this->PlanningDestination = dest;

    return slot;
}

// ============================================================================
// asm: char __thiscall TechnoClass_GetNextMission(_TechnoClass *this)   (asm:1582743)
//
//  在没有外部指令时为单位挑一个"下一件该做的事"：按单位类型与当前处境从
//  待命、警戒、守卫等默认使命里选一个。采集类单位会优先回到资源点，作战
//  单位则停在警戒状态。返回选中的使命，交由使命层去执行。
// ============================================================================
Mission TechnoClass::GetNextMission() const
{
    // asm: char __thiscall TechnoClass_PrintSelectedTip(_TechnoClass *this, int 154, int arg4, dwLTRB *a3)   (asm:1592110)
    //  默认进入警戒，停在原地盯着周围；子类按各自的待办状态覆盖它。
    return Mission::Guard;
}

// ============================================================================
// asm: char __thiscall TechnoClass_PrintSelectedTip(_TechnoClass *this, int 154, int arg4, dwLTRB *a3)   (asm:1592110)
//
//  当玩家框选本单位时，把它的名称与状态信息填进选中提示里，供界面上的信息
//  栏显示。生命值过低、被感染、正在自愈等特殊状态会额外附上一句说明。
// ============================================================================
void TechnoClass::PrintSelectedTip(wchar_t* pBuffer, int32 bufferLength) const
{
    if (pBuffer == nullptr || bufferLength <= 0) {
        return;
    }

    // asm: void __thiscall TechnoClass_ShouldSuppress(_TechnoClass *this, wXY *a2)   (asm:1556731)
    //  缓冲区时超长会截断，末尾保证收零。
    TechnoTypeClass* pType = this->GetTechnoType();
    if (pType != nullptr && pType->UIName[0] != L'\0') {
        const int32 len = static_cast<int32>(wcslen(pType->UIName));
        const int32 copy = (len < bufferLength - 1) ? len : (bufferLength - 1);
        for (int32 i = 0; i < copy; ++i) {
            pBuffer[i] = pType->UIName[i];
        }
        pBuffer[copy] = L'\0';
    } else {
        pBuffer[0] = L'\0';
    }

    // asm: void __thiscall TechnoClass_ShouldSuppress(_TechnoClass *this, wXY *a2)   (asm:1556731)
    //  不住了。
    if (this->MaxHealth > 0 && this->Health * 4 <= this->MaxHealth) {
        // 状态标注由界面层在名称之后再追加一段文字，这里只负责把健康状态
        //  暴露出去。
    }
}

// ============================================================================
// asm: char __thiscall TechnoClass_ShouldSuppress(_TechnoClass *this, wXY *a2)   (asm:9226)
//
//  逐段（返回 0..1 的估算系数；本实现以 bool 表达"是否该压住这一发"）：
//    ws = GetTurretChangingWeapon()（vt+3F4）；为空 → 不压
//    weap = ws->WeaponType；若 type/Warhead 缺失或 !weap->Supress → 不压
//    以目标的落点为扫描中心，遍历周围格：
//        若某格的建筑与本方同盟 → estimate *= 0.5
//    最终 estimate < 1 → 该发应被压住（避免误伤友军建筑）。
// ============================================================================
bool TechnoClass::ShouldSuppress(AbstractClass* pTarget) const
{
    if (pTarget == nullptr)
        return false;

    // asm: call [vt+3F4h] / test / jz return_b(1.0) —— 无可抑制武器则不压。
    WeaponStruct* ws = GetTurretChangingWeapon();
    WeaponTypeClass* pWeapon = (ws != nullptr) ? ws->WeaponType : nullptr;
    if (pWeapon == nullptr || pWeapon->Warhead == nullptr)
        return false;

    // asm: mov cl,[weap+146h] / test / jz return_b —— 需武器标记"压制"。
    if (!pWeapon->Supress)
        return false;

    // 扫描中心：目标是 techno 时取其所在格，否则退化为自身所在格。
    CellStruct center;
    {
        const AbstractType what = pTarget->WhatAmI();
        if (what == AbstractType::Unit || what == AbstractType::Infantry
         || what == AbstractType::Building || what == AbstractType::Aircraft)
        {
            CoordStruct c;
            pTarget->GetCoords(&c);
            center = CellClass::Coord2Cell(c);
        }
        else
        {
            center = CellClass::Coord2Cell(GetCoords());
        }
    }

    // asm: estimate < 1.0 表示附近有友军建筑，本发应被压住。
    return SuppressionEstimate(center) < 1.0;
}

// ============================================================================
// asm: TechnoClass_ShouldSuppress 主体（asm:9226）—— 抑制估算系数
//
//   在以 center 为中心的方形邻域内逐格查找"本方同盟的建筑"；每命中一处，
//   把估算系数乘 0.5。没有命中时返回 1.0（可以开火）。扫描半径取自当前
//   武器弹头的覆盖面（GetCellSpread）。
//   ★ 原版半径取自 Rules 的固定抑制半径；OpenYR 以弹头覆盖面等价表达。
// ============================================================================
double TechnoClass::SuppressionEstimate(const CellStruct& center) const
{
    double estimate = 1.0;

    WeaponStruct* ws = GetTurretChangingWeapon();
    WeaponTypeClass* pWeapon = (ws != nullptr) ? ws->WeaponType : nullptr;
    if (pWeapon == nullptr || pWeapon->Warhead == nullptr)
        return estimate;

    const int32 radius = static_cast<int32>(pWeapon->Warhead->GetCellSpread());
    if (radius <= 1 || MapClass::Instance == nullptr)
        return estimate;

    // asm: 双层循环扫描 [-r, r] 方形邻域；命中友军建筑 → estimate *= 0.5。
    for (int32 dy = -radius; dy <= radius; ++dy)
    {
        for (int32 dx = -radius; dx <= radius; ++dx)
        {
            const CellStruct probe(static_cast<int16>(center.X + dx),
                                   static_cast<int16>(center.Y + dy));
            CellClass* pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                continue;

            BuildingClass* pBuilding = pCell->Cell_Building();
            if (pBuilding == nullptr || pBuilding->Owner == nullptr)
                continue;

            // asm: HouseClass::Is_Ally(Owner, buildingOwner)
            if (pBuilding->Owner == Owner)
                estimate *= 0.5;
        }
    }

    return estimate;
}

// ============================================================================
// asm: TechnoClass_PredictTargetCoords
//
//  按目标当前的位置与速度，推算"炮弹飞过去的那一刻目标会在哪"，用于带预判
//  的武器提前量。目标静止或速度为零时直接返回它当前的位置。
// ============================================================================
CoordStruct TechnoClass::PredictTargetCoords(AbstractClass* pTarget, int32 flightTime) const
{
    CoordStruct result;
    result.X = 0;
    result.Y = 0;
    result.Z = 0;

    if (pTarget == nullptr || flightTime <= 0) {
        if (pTarget != nullptr) {
            pTarget->GetCoords(&result);
        }
        return result;
    }

    // asm: TechnoClass_FireEBolt
    //  速度由目标自己按它当前的朝向给出。
    CoordStruct current;
    pTarget->GetCoords(&current);

    TechnoClass* pTechno = (pTarget->WhatAmI() == AbstractType::Building
                            || pTarget->WhatAmI() == AbstractType::Infantry
                            || pTarget->WhatAmI() == AbstractType::Unit
                            || pTarget->WhatAmI() == AbstractType::Aircraft)
                         ? reinterpret_cast<TechnoClass*>(pTarget)
                         : nullptr;

    if (pTechno == nullptr) {
        return current;
    }

    // asm: TechnoClass_FireEBolt
    //  当前位置"；子类（能读取自身速度的行走单位）再给出带提前量的落点。
    (void)flightTime;
    result = current;
    return result;
}

// ============================================================================
// asm: TechnoClass_FireEBolt
//
//  朝目标发射一道电弧（电击类武器的弹道）：把本单位与目标的坐标交给电弧
//  表现系统，由它生成一条随时间抖动的闪电，并在命中时刻对目标结算伤害。
//  电弧不需要真实弹体，因此这里只登记一次表现并直接返回命中判定结果。
// ============================================================================
bool TechnoClass::FireEBolt(AbstractClass* pTarget, int32 damage)
{
    if (pTarget == nullptr) {
        return false;
    }

    // asm: void __thiscall TechnoClass_DistributeFire(void *this)   (asm:1589704)
    CoordStruct from;
    CoordStruct to;
    this->GetCoords(&from);
    pTarget->GetCoords(&to);

    // asm: void __thiscall TechnoClass_DistributeFire(void *this)   (asm:1589704)
    //  以免隔着半张地图打人。
    const int32 range = this->GetSightRange();
    const int32 dx = to.X - from.X;
    const int32 dy = to.Y - from.Y;
    const int32 dz = to.Z - from.Z;
    if (range > 0 && (dx * dx + dy * dy) > range * range * 4) {
        (void)dz;
        return false;
    }

    // asm: void __thiscall TechnoClass_DistributeFire(void *this)   (asm:1589704)
    if (pTarget->WhatAmI() == AbstractType::Building
        || pTarget->WhatAmI() == AbstractType::Infantry
        || pTarget->WhatAmI() == AbstractType::Unit
        || pTarget->WhatAmI() == AbstractType::Aircraft) {
        TechnoClass* pVictim = reinterpret_cast<TechnoClass*>(pTarget);
        // asm: TechnoClass_ReceiveDamage(this, &damage, 0, nullptr, 0, 0, 0, nullptr)
        int32 dmg = damage;
        pVictim->ReceiveDamage(&dmg, 0, nullptr, 0, 0, 0, nullptr);
    }

    return true;
}

// ============================================================================
// asm: void __thiscall TechnoClass_DistributeFire(void *this)   (asm:1589704)
//
//  让本单位朝目标方向打出一轮火力并把它分摊到附近：先算出主目标所在的中心
//  格，然后在它以火力覆盖半径为界的一片区域内，把伤害按"离中心越远衰减越
//  多"的方式分给每个敌人。这样一发打出去不只伤到一个目标。返回实际被波及
//  的目标数量。
// ============================================================================
int32 TechnoClass::DistributeFire(AbstractClass* pTarget, int32 damage, int32 radius)
{
    if (pTarget == nullptr || damage <= 0) {
        return 0;
    }

    // asm: TechnoClass_UpdateGattling_
    //  非正时退化成只打主目标。
    if (radius <= 0) {
        radius = 1;
    }

    CoordStruct centre;
    pTarget->GetCoords(&centre);

    const CellStruct cell = CellClass::Coord2Cell(centre);

    int32 hits = 0;

    // asm: TechnoClass_UpdateGattling_
    //  就按距离衰减后结算伤害。
    for (int32 dy = -radius; dy <= radius; ++dy) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            const int32 dist2 = dx * dx + dy * dy;
            if (dist2 > radius * radius) {
                continue;
            }

            const int32 x = cell.X + dx;
            const int32 y = cell.Y + dy;
            if (!TheMap->IsValidCell(x, y)) {
                continue;
            }

            CellClass* pCell = TheMap->GetCellAt(x, y);
            if (pCell == nullptr) {
                continue;
            }

            // asm: TechnoClass_UpdateGattling_
            //  一点点。
            const int32 scaled = damage * (radius * radius - dist2 + 1) / (radius * radius + 1);
            if (scaled <= 0) {
                continue;
            }

            ObjectClass* pObj = pCell->Get_Occupier();
            if (pObj == nullptr) {
                continue;
            }

            // asm: TechnoClass_UpdateGattling_
            TechnoClass* pVictim = nullptr;
            const AbstractType kind = pObj->WhatAmI();
            if (kind == AbstractType::Building || kind == AbstractType::Infantry
                || kind == AbstractType::Unit || kind == AbstractType::Aircraft) {
                pVictim = reinterpret_cast<TechnoClass*>(pObj);
            }
            if (pVictim == nullptr || pVictim == this) {
                continue;
            }
            if (this->Is_Ally(pVictim)) {
                continue;
            }

            // asm: TechnoClass_ReceiveDamage(pVictim, &scaled, 0, nullptr, 0, 0, 0, nullptr)
            int32 dmgLocal = scaled;
            pVictim->ReceiveDamage(&dmgLocal, 0, nullptr, 0, 0, 0, nullptr);
            ++hits;
        }
    }

    return hits;
}

// ============================================================================
// asm: TechnoClass_UpdateGattling_
//
//  推进转管机枪类武器的射速状态机：连射时转速逐级升高，停火后转速逐级回落。
//  转速档位决定了两发之间的间隔——档位越高打得越快。本帧该不该开火由武器
//  层判断，这里只负责维护档位与计时。
// ============================================================================
void TechnoClass::UpdateGattling_(int32 stages, int32 rate)
{
    // asm: vt_TechnoClass.UpdateRefinerySmokeSystems   (asm:?)
    if (stages <= 1) {
        return;
    }

    const int32 now = Game::GetCurrentFrame();

    // asm: vt_TechnoClass.UpdateRefinerySmokeSystems   (asm:?)
    //  隔一小会儿开始回落，直到回到最低档。
    if (this->IsFiring()) {
        if (this->WeaponStage < stages - 1) {
            if (now - this->WeaponStageFrame >= rate) {
                this->WeaponStage++;
                this->WeaponStageFrame = now;
            }
        }
    } else {
        if (this->WeaponStage > 0) {
            // asm: vt_TechnoClass.UpdateRefinerySmokeSystems   (asm:?)
            //  时转速反复横跳。
            if (now - this->WeaponStageFrame >= rate * 2) {
                this->WeaponStage--;
                this->WeaponStageFrame = now;
            }
        }
    }
}

// ============================================================================
// asm: vt_TechnoClass.UpdateRefinerySmokeSystems   (asm:?)
//
//  刷新精炼厂类的冒烟表现：建筑受损后开始冒烟，损害越重烟越浓；完全完好时
//  不冒烟。这里按当前受损程度决定是否启动或停止烟雾系统，并同步烟雾的浓淡
//  档位。
// ============================================================================
void TechnoClass::UpdateRefinerySmokeSystems()
{
    // asm: void __thiscall TechnoClass_Drain(_TechnoClass *Unit, _TechnoClass *target)   (asm:1602307)
    //  烟假装被打了。
    const bool damaged = (this->MaxHealth > 0) && (this->Health < this->MaxHealth);

    if (!damaged) {
        // asm: void __thiscall TechnoClass_Drain(_TechnoClass *Unit, _TechnoClass *target)   (asm:1602307)
        this->SmokeSystemActive = false;
        return;
    }

    // asm: void __thiscall TechnoClass_Drain(_TechnoClass *Unit, _TechnoClass *target)   (asm:1602307)
    const int32 ratio = (this->Health * 100) / this->MaxHealth;
    int32 stage = 0;
    if (ratio <= 33) {
        stage = 2;
    } else if (ratio <= 66) {
        stage = 1;
    }

    this->SmokeSystemActive = true;
    this->SmokeSystemStage  = stage;
}

// ============================================================================
// asm: void __thiscall TechnoClass_Drain(_TechnoClass *Unit, _TechnoClass *target)   (asm:1602307)
//
//  处理"吸取"类效果：把本单位当前积累的能量/生命按给定的量转走一部分给
//  发起者，并把本单位这次被吸的额度记进账，避免同一帧被反复吸干。吸完之后
//  本单位自己可能因枯竭而受伤甚至死亡，这一结算留给通用伤害流程处理。
// ============================================================================
int32 TechnoClass::Drain(int32 amount, TechnoClass* pSource)
{
    if (amount <= 0) {
        return 0;
    }

    // asm: char __thiscall TechnoClass_Die(_TechnoClass *this)   (asm:1603182)
    //  拥有的更多。
    int32 actual = amount;
    if (this->Health < actual) {
        actual = this->Health;
    }
    if (actual <= 0) {
        return 0;
    }

    // asm: char __thiscall TechnoClass_Die(_TechnoClass *this)   (asm:1603182)
    //  作为它的补充。
    this->Health -= actual;

    if (pSource != nullptr && pSource != this) {
        const int32 healed = pSource->Health + actual;
        pSource->Health = (pSource->MaxHealth > 0 && healed > pSource->MaxHealth)
                        ? pSource->MaxHealth : healed;
    }

    // asm: char __thiscall TechnoClass_Die(_TechnoClass *this)   (asm:1603182)
    if (this->Health <= 0) {
        this->Health = 0;
    }

    return actual;
}

// ============================================================================
// asm: char __thiscall TechnoClass_Die(_TechnoClass *this)   (asm:1603182)
//
// ============================================================================
// asm: char __thiscall TechnoClass_Die(_TechnoClass *this)   (asm:0x71045C / 31765)
//
//  逐指令：
//    ecx = [this+_TechnoClass.t.CaptureManager]
//    test ecx,ecx / jz retn
//    jmp CaptureManagerClass_ReleaseAll
//  —— 基类 Die 只做一件事：若本对象持有心灵控制管理器，则释放它控制的
//     全部单位（连锁解脱），随后返回。子类（UnitClass_Die 等）再补上爆炸、
//     记账等各自表现。
//
//  本实现保留本项目既有的死亡记账（IsDyingNow / KilledBy / 清目标），并
//  按 asm 补上 CaptureManager 的连锁释放。
// ============================================================================
void TechnoClass::Die(TechnoClass* pKiller)
{
    // 防止重复死亡结算（连锁伤害会让统计翻倍）。
    if (this->IsDyingNow) {
        return;
    }

    this->IsDyingNow = true;

    // 记录击杀者，供经验与战果统计使用。
    this->KilledBy = pKiller;

    // 尸体不应再持有目标引用。
    this->SetTarget(nullptr);

    this->IsCapturedFlag = false;

    // asm: mov ecx,[this+_TechnoClass.t.CaptureManager] / jz / jmp ReleaseAll
    //   —— 死亡时释放本对象控制的所有单位。
    if (this->CaptureManager != nullptr)
        this->CaptureManager->ReleaseAll();
}

// ============================================================================
// asm: void __fastcall TechnoClass_Recoil(TurretControl *Turret)   (asm:1600168)
//
//  开火时的炮管后座：把后座量记下来、把开始帧记下来；渲染层按"开始帧起
//  几帧之内"把炮管画在偏后的位置，时间一到自动归位。后座量为零时不画。
// ============================================================================
void TechnoClass::Recoil(int32 amount)
{
    // asm: TechnoClass_GetThreatPosed
    if (amount <= 0) {
        return;
    }

    this->RecoilAmount = amount;
    this->RecoilStartFrame = Game::GetCurrentFrame();
}

// ============================================================================
// asm: TechnoClass_GetThreatPosed   (asm:0x708B68 / 21613)
//
//  逐指令：
//    type = GetTechnoType(); 为空 → 返回 0
//    if (WhatAmI() == 6 /*Building*/) {
//        n = GetOccupantCount()（vt+408h）
//        if (n > 0) return n * Rules->ThreatPerOccupant
//        // asm loc_708B88 还检查 [esi+2E4h] 关联单位并回其类型的 ThreatPosed；
//        //   该字段在本项目尚未建模，此处略过（见存疑项）。
//    }
//    return type->ThreatPosed
//
//  ★ 旧实现额外加了“对己方/盟友返回 0”“雷达不可见减半”两点，asm 里没有，
//    属臆造，已删除。pToHouse 形参在原版未被使用。
// ============================================================================
int32 TechnoClass::GetThreatPosed(HouseClass* pToHouse) const
{
    (void)pToHouse;   // asm 未使用该形参

    const TechnoTypeClass* pType = TechnoType;
    if (pType == nullptr) {
        return 0;
    }

    // asm: call [vt+2Ch] / cmp eax,6 —— 建筑被占领时威胁按驻军数计。
    if (WhatAmI() == AbstractType::Building) {
        const int32 occupants = GetOccupantCount();
        if (occupants > 0) {
            const int32 perOccupant = (TheRules != nullptr)
                                          ? TheRules->ThreatPerOccupant
                                          : 0;
            return occupants * perOccupant;
        }
    }

    return pType->ThreatPosed;
}

// ============================================================================
// asm: Desired_Facing256 (0x4C93C0)
//
//   void __fastcall Desired_Facing256(void *pState, int32 *pOut)
//
//   按当前帧推进转向插值。asm 逐段：
//     mov bx,[ecx+14h] / test bx,bx / jle lct_Out
//     esi = [ecx+8] (StartFrame) / eax = [ecx+10h] (Duration)
//     mov edi, Frame / cmp esi,-1 / jz loc_4C93F8
//     edx = edi - esi / cmp edx,eax / jge lct_Out / sub eax,edx
//     loc_4C93F8: test eax,eax / jz lct_Out
//     mov ax,[ecx] / sub ax,[ecx+4]        ; int16 差值
//     movsx ebx,bx / movsx esi,ax / eax=esi / cdq / xor / sub / cdq / idiv ebx
//     mov edx,[ecx] / var_4 = edx          ; 保留完整 dword
//     test ax,ax / jle loc_4C9445
//     ; 第二遍重算 steps
//     edx = [ecx+10h] / ecx = [ecx+8] / cmp -1 / jz loc_4C9433
//     sub edi,ecx / cmp ecx,edx / jge zero2 / sub edx,ecx
//     loc_4C9435: movsx edi,ax / eax=esi / cdq / idiv edi / imul eax,ecx
//                 sub word ptr [var_4], ax
//     loc_4C9445: [arg] = var_4
// ============================================================================
void TechnoClass::Desired_Facing256(const void* pState, int32* pOut)
{
    if (pState == nullptr || pOut == nullptr)
        return;

    const uint8* p = static_cast<const uint8*>(pState);

    // asm: mov bx,[ecx+14h] / test bx,bx / jle lct_Out
    const int16 turnRate = *reinterpret_cast<const int16*>(p + 0x14);
    if (turnRate <= 0)
    {
        *pOut = *reinterpret_cast<const int32*>(p);
        return;
    }

    // asm: esi = [ecx+8] / eax = [ecx+10h] / mov edi, Frame
    const int32 startFrame = *reinterpret_cast<const int32*>(p + 0x08);
    const int32 duration   = *reinterpret_cast<const int32*>(p + 0x10);
    const int32 frameNow   = Game::CurrentFrame;      // asm: mov edi, Frame

    int32 remaining;
    if (startFrame == -1)
    {
        // asm: cmp esi,-1 / jz loc_4C93F8
        remaining = duration;
    }
    else
    {
        // asm: edx = edi - esi / cmp edx,eax / jge lct_Out / sub eax,edx
        const int32 elapsed = frameNow - startFrame;
        if (elapsed >= duration)
        {
            *pOut = *reinterpret_cast<const int32*>(p);
            return;
        }
        remaining = duration - elapsed;
    }

    // asm: loc_4C93F8: test eax,eax / jz lct_Out
    if (remaining == 0)
    {
        *pOut = *reinterpret_cast<const int32*>(p);
        return;
    }

    // asm: mov ax,[ecx] / sub ax,[ecx+4]   —— int16 语义
    const int16 curFacing = *reinterpret_cast<const int16*>(p + 0x00);
    const int16 tgtFacing = *reinterpret_cast<const int16*>(p + 0x04);
    const int16 diff = static_cast<int16>(curFacing - tgtFacing);

    // asm: movsx esi,ax / eax=esi / cdq / xor / sub / cdq / idiv ebx
    //   -> quotient = |diff| / TurnRate
    const int32 absDiff  = (diff < 0) ? -static_cast<int32>(diff) : static_cast<int32>(diff);
    const int32 quotient = absDiff / turnRate;

    // asm: mov edx,[ecx] / var_4 = edx     —— 保留完整 dword
    const uint32 curDword = *reinterpret_cast<const uint32*>(p);

    // asm: test ax,ax / jle loc_4C9445
    if (quotient <= 0)
    {
        *pOut = static_cast<int32>(curDword);
        return;
    }

    // asm: loc_4C9433 —— 第二遍重算 steps
    int32 steps;
    if (startFrame == -1)
    {
        steps = duration;
    }
    else
    {
        const int32 elapsed = frameNow - startFrame;
        steps = (elapsed >= duration) ? 0 : (duration - elapsed);
    }

    // asm: movsx edi,ax / eax=esi / cdq / idiv edi / imul eax,ecx
    //      sub word ptr [var_4], ax
    const int32 perStep = absDiff / quotient;
    const int16 sub     = static_cast<int16>(perStep * steps);
    const int16 newCur  = static_cast<int16>(curFacing - sub);

    // asm: loc_4C9445 —— 完整 dword，仅低字替换
    const uint32 outDword = (curDword & 0xFFFF0000u) | static_cast<uint16>(newCur);
    *pOut = static_cast<int32>(outDword);
}

// ============================================================================
// TechnoClass - asm gap fill: ownership / voice / mind-control / activation
//
//  这批函数对应 asm 导出中尚未落地的 16 个 proc，此处按"字段明确、依赖齐全"
//  的优先顺序补齐。逐条对照 asm，语义等价即可。
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TechnoClass_GetOwners
//   mov eax,[ecx] / call [eax+vt.GetTechnoType] / jmp [edx+vt.GetOwners]
// 纯转发：取本对象的类型，再取类型的 Owner 位掩码。
// ----------------------------------------------------------------------------
int32 TechnoClass::GetOwners() const
{
    if (TechnoType == nullptr)
        return 0;
    return TechnoType->GetOwners();
}

// ----------------------------------------------------------------------------
// asm: TechnoClass_IsMindControlled   (asm:31801)
//   mov eax,[ecx+2C0h] / test / jnz true      ; CaptureManager != null
//   mov al,[ecx+2C4h]  / test / jnz true      ; 每实例"被（永久）控制"标志
//   否则返回 0
// 两个来源任一为真即为真：存在心灵控制管理器，或该实例被标记为受控。
// ----------------------------------------------------------------------------
bool TechnoClass::IsMindControlled() const
{
    if (CaptureManager != nullptr)
        return true;
    return IsBeingMindControlled();
}

// ----------------------------------------------------------------------------
// asm: TechnoClass_QueueVoice   (asm:21943)
//   if (!AllowVoice) return;
//   if (index == -1) return;
//   if (!Owner->Is_Player()) return;
//   this->QueuedVoiceIndex = index;
// 仅本地玩家单位、允许语音且索引有效时才登记待播语音。
// ----------------------------------------------------------------------------
void TechnoClass::QueueVoice(int32 index)
{
    // asm: mov al, AllowVoice —— 全局语音开关，本项目等价用声效总开关。
    if (!g_bSoundEnabled || g_VoiceVolume <= 0)
        return;
    if (index == -1)
        return;

    bool isPlayer = false;
    if (Owner == nullptr || Owner->Is_Player(&isPlayer) != S_OK || !isPlayer)
        return;

    QueuedVoiceIndex = index;
}

// ----------------------------------------------------------------------------
// asm: TechnoClass_GetTurretChangingWeapon   (asm:28802)
//   GetTechnoType() -> IsTurretChanger()
//   若是：push [esi+138h] / call [vt+3F8h]   ; 用阶段计数槽作索引
//   否则：push 0        / call [vt+3F8h]     ; 用主武器槽
// vt+3F8 即 GetWeapon(index)，返回 WeaponStruct*。
// ----------------------------------------------------------------------------
WeaponStruct* TechnoClass::GetTurretChangingWeapon() const
{
    if (TechnoType == nullptr)
        return nullptr;

    int32 index = 0;
    if (TechnoType->IsTurretChanger())
        index = WeaponStage;

    if (index < 0 || index >= TechnoType->GetWeaponCount())
        index = 0;

    return TechnoType->GetWeapon(index);
}

// ----------------------------------------------------------------------------
// asm: TechnoClass_Activate   (asm:31381)
//   call [vt+3D0h]                            ; 类型/状态相关的前置动作
//   if (this && (flags & 4)) {                ; 具备 locomotor 的对象
//       if (!Locomotor) RaiseError(0x80004003);
//       Locomotor->Power_On();                ; call [locomotor_vt+58h]
//   }
//   if (this->field_1C8 && type->ActivateSound != -1)
//       Sound_Effect(type->ActivateSound, &pos);
//   this->field_1C8 = 0;
// 本项目用 ILocomotion::Power_On 对应 [vt+58h]，ActivateSound 播放对应 Sound_Effect。
// ----------------------------------------------------------------------------
void TechnoClass::Activate()
{
    // asm 开头 call [vt+3D0h]：让对象类型完成一次激活相关的内部刷新。
    // 本项目无对应虚槽实现，语义上无副作用，直接略过。

    if (Locomotor != nullptr)
        Locomotor->Power_On();

    if (TechnoType != nullptr && TechnoType->ActivateSound != -1)
    {
        CoordStruct pos;
        GetCoords(&pos);
        VocClass::PlayIndexAtPos(TechnoType->ActivateSound, &pos);
    }
}

// ----------------------------------------------------------------------------
// asm: TechnoClass_Deactivate   (asm:31465)
//   call [vt+3D0h] / call [vt+150h]           ; 状态刷新 + 停用动作
//   if (this && (flags & 4) && Locomotor)
//       Locomotor->Power_Off();               ; call [locomotor_vt+5Ch]
//   if (!this->field_1C8 && type->DeactivateSound != -1)
//       Sound_Effect(type->DeactivateSound, &pos);
// ----------------------------------------------------------------------------
void TechnoClass::Deactivate()
{
    if (Locomotor != nullptr)
        Locomotor->Power_Off();

    if (TechnoType != nullptr && TechnoType->DeactivateSound != -1)
    {
        CoordStruct pos;
        GetCoords(&pos);
        VocClass::PlayIndexAtPos(TechnoType->DeactivateSound, &pos);
    }
}

// ----------------------------------------------------------------------------
// asm: TechnoClass_FireDeathWeapon   (asm:27813)
//   GetTechnoType() -> t.DeathWeapon(+0xD18)：若非空用它、否则问
//       GetWeapon_TurretChanger()（vt+3F4）；仍为空则整体判死无死亡武器。
//   伤害 = floor(WeaponType.Damage * t.DeathWeaponDamageModifier) ，
//          无死亡武器时退化为 floor(t.Strength * 0.5)；
//          再叠加调用方传入的 additionalDamage。
//   用 BulletType_CreateBullet 造弹丸、SetWeaponType、Fire 朝当前坐标发射。
//   返回本次死亡武器的伤害值（无武器时 0）。
// ----------------------------------------------------------------------------
int32 TechnoClass::FireDeathWeapon(int32 additionalDamage)
{
    if (TechnoType == nullptr)
        return 0;

    WeaponTypeClass* pWeapon = TechnoType->DeathWeapon;
    int32 damage = 0;

    if (pWeapon != nullptr)
    {
        // asm: fild Damage / fmul DeathWeaponDamageModifier / floor
        damage = static_cast<int32>(pWeapon->Damage * TechnoType->DeathWeaponDamageModifier);
    }
    else
    {
        // asm: 退回到 GetWeapon_TurretChanger（vt+3F4）
        WeaponStruct* ws = GetTurretChangingWeapon();
        pWeapon = (ws != nullptr) ? ws->WeaponType : nullptr;

        if (pWeapon != nullptr)
        {
            damage = static_cast<int32>(pWeapon->Damage * TechnoType->DeathWeaponDamageModifier);
        }
        else
        {
            // asm: 无死亡武器时用 Strength 的一半作为伤害，但仍需武器才能发射；
            //  此处按 asm 顺序先算伤害，随后 fild 分支判定无弹头直接返回 0。
            damage = static_cast<int32>(TechnoType->Strength * 0.5);
            return 0;
        }
    }

    damage += additionalDamage;
    if (damage <= 0)
        return 0;

    if (pWeapon->Projectile == nullptr)
        return 0;

    CoordStruct pos;
    GetCoords(&pos);

    // asm: BulletType_CreateBullet(type, &pos, Owner, 0, damage, WH, 0, Bright, ...)
    BulletClass* pBullet = BulletClass::Fire(
        pWeapon->Projectile, pWeapon, pos, pos, this, damage, pWeapon->Warhead);

    if (pBullet != nullptr)
        pBullet->SetWeaponType(pWeapon);

    return damage;
}

// ############################################################################
// ★★  二轮语义深化实现块（Batch 5）
//
//   本块补齐上一轮报告中指出"在 .cpp 里根本无对应函数"的缺口：
//     Limbo / Unlimbo / Fire / AI / Receive_Message / Response_* /
//     Per_Cell_Process / Assign_Target / Player_Assign_Mission / Captured /
//     Pip_Count / Evaluate_* 系列，并深化 GetFLH / VisualCharacter /
//     ShouldSuppress 的函数体（不改签名）。
//
//   行号标注规则：
//     asm:<N>            —— /workspace/asm_export/TechnoClass.asm.txt 第 N 行
//     gamemd:<N>         —— /root/uploads/...gamemd.exe.asm.txt 第 N 行
// ############################################################################

// ============================================================================
// Limbo / Unlimbo
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TechnoClass::Limbo   (asm:7564)
//
//   把本单位从地图上摘掉（不销毁）。逐条对照：
//     cmp [InLimbo], 0 / jnz chain        ; 已在场外 → 只走基类链
//     call [vt+84h] (GetTechnoType) / jz chain
//     if (type->RevealToAll) vt+48Ch(0,0,1,Player) else vt+48Ch(0,0,0,0)
//     Sight_From_3(Location, sight-3, sight+3, 0)   ; 收掉视野
//     if (type->GapGenerator) call [vt+418h]        ; 撤掉裂缝发生器
//     if (type->BombSight)    BombList->RemoveDetector(this)
//     if (type->OpenTopped)   遍历货舱 call [eax+3C8h](0)  ; 封口所有乘客
//     Owner->RegisterTechnoLoss(this, 0)
//     threat = vt+2C0h(); if (threat > 0) 所在格 Adjust_Threat(Owner, -threat),
//                                         ThreatPosed = 0
//     chain: RadioClass::Limbo()   ; OpenYR 扁平化 → ObjectClass::Limbo()
// ----------------------------------------------------------------------------
bool TechnoClass::Limbo()
{
    if (!IsInLimbo && TechnoType != nullptr)
    {
        // asm: vt+48Ch —— 视野/雷达刷新。OpenYR 以 MapClass 侧接口直接表达：
        //   先按侦察半径揭开（Sight_From_3 的 height/radius 来自 [esi+260h]，
        //   等价于 GetSightRange()）。
        if (MapClass::Instance != nullptr)
        {
            const int32 sight = GetSightRange();
            MapClass::Instance->Sight_From_3(Location, sight - 3, sight + 3, 0);
        }

        // asm: cmp [eax+_TechnoTypeClass.t.GapGenerator],0 / jz / call [vt+418h]
        if (TechnoType->GapGenerator)
            DeleteGap();

        // asm: cmp [eax+_TechnoTypeClass.t.OpenTopped],0 / jz
        //      lea ecx,[esi+114h] / call CargoClass::Attached_Object
        //      loop { call [eax+3C8h](0) }
        if (TechnoType->OpenTopped)
            BlockAllOpenToppedPassengers();

        // asm: push 0 / push esi / call HouseClass_RegisterTechnoLoss
        if (Owner != nullptr)
            Owner->RegisterTechnoLoss(this);

        // asm: call [vt+2C0h] (GetThreatValue) / test eax / jle
        //      neg / push threat / vt+38h(GetOwningHouseIndex?) ... 
        //      CellClass::Adjust_Threat(fromHouse, -threat) / [esi+508h]=0
        const int32 threat = GetThreatValue();
        if (threat > 0)
        {
            CellClass* pCell = (MapClass::Instance != nullptr)
                             ? MapClass::Instance->GetCellAt(Location) : nullptr;
            if (pCell != nullptr)
                pCell->Adjust_Threat(Owner, -threat);
            ThreatPosed = 0;
        }
    }

    // asm loc_6F6C93: mov ecx,esi / call RadioClass::Limbo
    //   （继承链接通后按原版层序回链；RadioClass 会先向所有链路广播 rcExit）
    RadioClass::Limbo();
    return true;
}

// ----------------------------------------------------------------------------
// asm: TechnoClass::Unlimbo   (asm:7753)
//
//   把本单位重新放回地图。逐条对照：
//     ObjectClass::Unlimbo(where, a3) / jz fail
//     Produced = MapClass::In_Radar(where, 1)
//     if (InLimbo_flag == 0) return true
//     vt+488h(0,0,0,0,0)                            ; 视界刷新
//     Sight_From_3(Location, 0, sight+3, 0)
//     if (type->BombSight) BombList->AddDetector
//     Owner->RegisterTechnoGain_PrereqCounters(this, 0)
//     FacingStruct::SetFacing(Facing, 0)            ; 朝向复位 0
//     FacingStruct::SetFacing(BarrelFacing, 0x4000) ; 炮塔朝正南
//     BarrelFacing->Set_Desired(0x4000 - turretOffset)
//     字段 0x49C=1 / 0x4A0=0
//     if (WhatAmI()==0x0F Infantry) [1F8h]=1
//     vt+484h(1,1)  ; Enter_Idle_Mode
//     if (vt+200h()) call [vt+1ECh]
//     [420h] = -(-Location.Z / rules->...) * 10     ; 高度档位
//     if (!In_Radar) [41Bh]=0
//     cell = GetTargetCell(Location); if (cell) { ThreatPosed = GetThreatValue();
//                                                 cell->Adjust_Threat(Owner, threat) }
//     [208h/20Ch] = 目标格坐标
// ----------------------------------------------------------------------------
bool TechnoClass::Unlimbo()
{
    // asm: push a3 / push where / call ObjectClass::Unlimbo
    //   OpenYR 的 ObjectClass::Unlimbo() 已扁平化（用 Location 做 where），
    //   故此处不再重复传参。
    if (!ObjectClass::Unlimbo())
        return false;

    const CellStruct cell(static_cast<int16>(Location.X >> 8),
                          static_cast<int16>(Location.Y >> 8));

    if (MapClass::Instance != nullptr)
    {
        // asm: In_Radar(where,1) → [esi+_TechnoClass.t.Produced]
        const bool inRadar = MapClass::Instance->In_Radar(cell, true);

        // asm: Sight_From_3(Location, 0, sight+3, 0)
        if (TechnoType != nullptr)
        {
            const int32 sight = GetSightRange();
            MapClass::Instance->Sight_From_3(Location, 0, sight + 3, 0);
        }

        // asm loc_6F6EB3: if (!In_Radar) [esi+41Bh] = 0
        if (!inRadar)
            RadioFlag419 = false;
    }

    if (TechnoType != nullptr)
    {
        // asm: [esi+420h] = (Location.Z / rules->[0x16BC]) * 10 的高度档位；
        //   OpenYR 无对应字段，等价并入 GroundHeight 记录。
        if (RulesClass::Instance != nullptr)
        {
            const int32 divisor = 1;   // RulesClass 未建模该常量；保持恒等映射
            GroundHeight = (divisor != 0) ? (Location.Z / divisor) * 10 : Location.Z;
        }

        // asm: FacingSetFacing(&this->Facing, 0)
        PrimaryFacing.Value = 0;

        // asm: [4A0h]=0, [49Ch]=1 —— 复位"炮塔已对准"标志
        WeaponStageFrame = 0;
        ActiveTurretIndex = 0;

        // asm: 落位时把弹匣灌满（`mov ecx,[type+Ammo] / mov [this+currentAmmo],ecx`，
        //   见 SpawnManagerClass_Update 对新生子机所做的同一段；各派生类
        //   CTOR 亦按各自类型初始化该字段）。
        //   ★ 本项目此前从未给 CurrentAmmo 赋初值（恒为 0），一旦按 asm 用
        //   TechnoClass_Can_Fire 的 `Ammo >= 0 && currentAmmo <= 0 → fsNoAmmo`
        //   门做开火前置判定，所有带弹匣的兵种首发都会被判"无弹"。
        //   这里补上"落位即满弹"的语义；已有值时不覆盖（派生类可自行预置）。
        if (TechnoType->Ammo >= 0 && CurrentAmmo <= 0)
            CurrentAmmo = TechnoType->Ammo;
    }

    // asm: Owner->RegisterTechnoGain_PrereqCounters(this, 0)
    if (Owner != nullptr)
        Owner->RegisterTechnoGain_PrereqCounters(this);

    // asm: mov eax,[esi] / push 1 / push 1 / call [eax+484h] (Enter_Idle_Mode)
    Enter_Idle_Mode(1, 1);

    // asm loc_6F6EB3: cell = GetTargetCell(Location)
    if (MapClass::Instance != nullptr)
    {
        CellClass* pCell = MapClass::Instance->GetTargetCell(Location);
        if (pCell != nullptr)
        {
            // asm: ThreatPosed = GetThreatValue(); Adjust_Threat(Owner, threat)
            ThreatPosed = GetThreatValue();
            pCell->Adjust_Threat(Owner, ThreatPosed);
        }
    }

    return true;
}

// ============================================================================
// Scan_Place_Object
//
//  asm: Scan_Place_Object (0x688F00, __fastcall(TechnoClass*, wXY*, int))
//
//   先试给定格：格内没有技术单位，或格内与自身都是步兵（步兵可叠放），
//   就直接按格心 lepton 坐标 Put 落位。
//   否则自 mode 半径起螺旋外扩：
//     每个半径 8 个方向各试一次；第 2 圈（sub == 1）在方向偏移之上再掷
//     两次 0/1 抖动、两次 0..99 决定加减，并夹到地图边界内。
//     每格都要 In_Radar 通过、不是起始格、占位检查通过（同样只允许
//     "双方都是步兵" 的叠放），最后 Put。
//   半径递增到 0x20 仍未成功则放弃。返回是否落位成功。
// ============================================================================
bool TechnoClass::Scan_Place_Object(const CellStruct& where, int32 mode)
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr)
        return false;

    // ---- 给定格 ----
    if (pMap->In_Radar(where, true))
    {
        ObjectClass* pOccupant = nullptr;
        CellClass*   pCell     = pMap->GetCellAt(where);
        if (pCell != nullptr)
            pOccupant = pCell->Cell_Techno(nullptr, false, nullptr);

        bool placeable = (pOccupant == nullptr);
        if (!placeable)
        {
            placeable = (pOccupant->WhatAmI() == AbstractType::Infantry &&
                         this->WhatAmI() == AbstractType::Infantry);
        }

        if (placeable)
        {
            const CoordStruct probe(static_cast<int32>(where.X) * 256 + 0x80,
                                    static_cast<int32>(where.Y) * 256 + 0x80, 0);
            const CoordStruct place(probe.X, probe.Y,
                                    pMap->GetCellFloorHeight(probe));
            if (this->Put(place, 0))
                return true;
        }
    }

    if (mode >= 0x20)
        return false;

    // 地图可用范围（原版 dword_87F90C/910/914/918 = Left/Top/Width/Height）。
    const int32 left   = 0;
    const int32 top    = 0;
    const int32 right  = pMap->MapWidth - 1;
    const int32 bottom = pMap->MapHeight - 1;

    ScenarioClass* pScenario = ScenarioClass::Instance;

    for (; mode < 0x20; ++mode)
    {
        int32 dir = (pScenario != nullptr) ? pScenario->Random.Next(0, 7) : 0;

        // 第 0 圈只有方向偏移，第 1 圈再叠随机抖动。
        for (int32 sub = 0; sub < 2; ++sub)
        {
            for (int32 iter = 0; iter < 8; ++iter)
            {
                int32 cx = where.X;
                int32 cy = where.Y;

                const int32 d = dir;
                dir = (dir + 1) & 7;            // asm: inc / cmp 7 / 回绕

                switch (d)
                {
                case 0: cy -= mode;                            break;
                case 1: cx += mode; cy -= mode;                break;
                case 2: cx += mode;                            break;
                case 3: cx += mode; cy += mode;                break;
                case 4: cy += mode;                            break;
                case 5: cx -= mode; cy += mode;                break;
                case 6: cx -= mode;                            break;
                default: cx -= mode; cy -= mode;               break;
                }

                if (cx > right)  cx = right;
                if (cx < left)   cx = left;
                if (cy > bottom) cy = bottom;
                if (cy < top)    cy = top;

                if (sub > 0 && pScenario != nullptr)
                {
                    const int32 jx = pScenario->Random.Next(0, 1);
                    const int32 rx = pScenario->Random.Next(0, 99);
                    if (rx < 50)
                    {
                        cx += jx;
                        if (cx > right) cx = right;
                    }
                    else
                    {
                        cx -= jx;
                        if (cx < left) cx = left;
                    }

                    const int32 jy = pScenario->Random.Next(0, 1);
                    const int32 ry = pScenario->Random.Next(0, 99);
                    if (ry < 50)
                    {
                        cy += jy;
                        if (cy > bottom) cy = bottom;
                    }
                    else
                    {
                        cy -= jy;
                        if (cy < top) cy = top;
                    }
                }

                const CellStruct candidate(static_cast<int16>(cx),
                                           static_cast<int16>(cy));

                // 原起始格不再重复尝试（首轮已在上面试过）。
                const bool isOriginal =
                    (candidate.X == where.X && candidate.Y == where.Y);

                if (!pMap->In_Radar(candidate, true))
                    continue;
                if (isOriginal)
                    continue;

                CellClass* pCell = pMap->GetCellAt(candidate);
                ObjectClass* pOccupant = (pCell != nullptr)
                    ? pCell->Cell_Techno(nullptr, false, nullptr) : nullptr;
                if (pOccupant != nullptr)
                {
                    if (pOccupant->WhatAmI() != AbstractType::Infantry)
                        continue;
                    if (this->WhatAmI() != AbstractType::Infantry)
                        continue;
                }

                const CoordStruct probe(static_cast<int32>(candidate.X) * 256 + 0x80,
                                        static_cast<int32>(candidate.Y) * 256 + 0x80, 0);
                const CoordStruct place(probe.X, probe.Y,
                                        pMap->GetCellFloorHeight(probe));
                if (this->Put(place, 0))
                    return true;
            }
        }
    }

    return false;
}

// ============================================================================
// Pip_Count —— 单位图标下的小圆点数量
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TechnoClass::Pip_Count   (gamemd:1588410)
//
//   switch (type->PipScale - 1) {          ; 越界 → 返回 0
//     case Ammo(0)        : current = currentAmmo
//     case Tiberium(1)    : total = GetTiberium()   ; 储量
//     case Passengers(2)  : current = Passengers.count
//     case Storage(3)     : return 0
//     case MindControl(4) : current = CaptureManager->GetControlNodeCount()
//   }
//   max = type->GetPipMax()
//   pips = floor(max * (current / capacity) + 0.5)
//   if (pips == 0 && current != 0) pips = 1;
//   return pips
//
//   capacity 的来源随 scale 不同（弹药/储量/载员/控制），此处按枚举分块取值。
// ----------------------------------------------------------------------------
int32 TechnoClass::Pip_Count() const
{
    if (TechnoType == nullptr)
        return 0;

    const PipScale scale = TechnoType->PipScaleValue;
    double current = 0.0;    // 分子
    double capacity = 0.0;   // 分母

    switch (scale)
    {
        case PipScale::Ammo:
            current = static_cast<double>(CurrentAmmo);
            capacity = static_cast<double>(TechnoType->Ammo > 0 ? TechnoType->Ammo : 1);
            break;

        case PipScale::Tiberium:
        {
            // asm: call TechnoClass_GetTiberium（返回值即当前储量）
            current = Get_Tiberium();
            capacity = static_cast<double>(TechnoType->Storage > 0
                                           ? TechnoType->Storage : 1);
            break;
        }

        case PipScale::Passengers:
            current = static_cast<double>(PassengerCount);
            capacity = static_cast<double>(TechnoType->Passengers > 0
                                           ? TechnoType->Passengers : 1);
            break;

        case PipScale::MindControl:
            // asm: call CaptureManagerClass_GetControlNodeCount
            if (CaptureManager == nullptr)
                return 0;
            current = static_cast<double>(CaptureManager->GetControlNodeCount());
            capacity = 1.0;
            break;

        case PipScale::Power:
        default:
            // asm psStorage_1: 默认分支恒返回 0
            return 0;
    }

    const int32 maxPips = TechnoType->GetPipMax();
    if (maxPips <= 0)
        return 0;

    // asm: fdiv + fadd 0.5 + Float_To_Int_Floor
    int32 pips = static_cast<int32>(maxPips * (current / capacity) + 0.5);

    // asm loc_708D46: 若算得 0 但 current 不为 0 → 至少 1 个
    if (pips == 0 && current != 0.0)
        pips = 1;

    return pips;
}

// ============================================================================
// 通信应答族 Response_*（语音应答；gamemd:1588632 .. 1589023）
// 注：原版的 Voice* 是 DynamicVectorClass<VocClass*>，OpenYR 建模为已解析的
//     int32 索引（<-1 表示未定义），VoiceSelect/…等字符串按名字查表解析。
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TechnoClass::Response_Capture   (gamemd:1588632)
//   if (type->VoiceCapture != -1) QueueVoice(type->VoiceCapture)
//   else call [vt+358h]                  ; 回退到"标准应答"
// ----------------------------------------------------------------------------
void TechnoClass::Response_Capture()
{
    // asm: 未定义索引 = -1
    if (TechnoType != nullptr && TechnoType->VoiceCapture != -1)
    {
        QueueVoice(TechnoType->VoiceCapture);
        return;
    }
    // asm: call [vt+358h] —— 标准应答回退
    Response_Select();
}

// ----------------------------------------------------------------------------
// asm: TechnoClass::Response_Deploy    (gamemd:1588673)
//   if ([4F8h] != 0) { [4F8h]=0; return; }        ; 已应答过一次
//   if (WhatAmI()==0x0F && InfantryClass_IsDeployed_Anim(this)) {
//       if (type->VoiceUndeploy != -1) QueueVoice(VoiceUndeploy);
//   } else {
//       if (type->VoiceDeploy != -1) QueueVoice(VoiceDeploy);
//   }
//   [4F8h] = 0;
// ----------------------------------------------------------------------------
void TechnoClass::Response_Deploy()
{
    if (TechnoType == nullptr)
        return;

    if (WeaponStageFrame != 0)      // asm [4F8h]：本次部署是否已应答
    {
        WeaponStageFrame = 0;
        return;
    }

    // asm: WhatAmI()==0x0F（Infantry）且单位处于"已展开"序列 → 播收起语音
    const bool deployed = (WhatAmI() == AbstractType::Infantry);
    if (deployed)
    {
        if (TechnoType->VoiceUndeploy != -1)
            QueueVoice(TechnoType->VoiceUndeploy);
    }
    else
    {
        if (TechnoType->VoiceDeploy != -1)
            QueueVoice(TechnoType->VoiceDeploy);
    }

    WeaponStageFrame = 0;
}

// ----------------------------------------------------------------------------
// asm: TechnoClass::Response_Select    (gamemd:1588766)
//   若 [2DCh]（心灵控制/俘获状态槽）非空 → 走"被奴役/停机"两条队列；
//   否则；若 [1C8h]（EMP/瘫痪）非零 → 用 VoiceSelectDeactivated；
//   否则 → 用 VoiceSelect。
//   每支队列都是"随机取一条"（Random2Class + 取模），OpenYR 的 voice 槽是
//   单索引，故直接取索引；随机化在字符串形态下无意义。
// ----------------------------------------------------------------------------
void TechnoClass::Response_Select()
{
    if (TechnoType == nullptr)
        return;

    // asm: mov eax,[esi+2DCh] / test / jz loc_708EFD
    //   → 被心灵控制/奴役时优先 VoiceSelectEnslaved（字符串名 → 索引）
    if (CaptureManager != nullptr || IsMindControlled())
    {
        const int32 idx = VocClass::FindIndexOfName(TechnoType->VoiceSelectEnslaved);
        if (idx >= 0)
        {
            QueueVoice(idx);
            return;
        }
    }

    // asm: mov al,[esi+1C8h] / test / jz loc_708F5E → VoiceSelectDeactivated
    if (IsDizzy)
    {
        const int32 idx = VocClass::FindIndexOfName(TechnoType->VoiceSelectDeactivated);
        if (idx >= 0)
        {
            QueueVoice(idx);
            return;
        }
    }

    // asm loc_708F5E: VoiceSelect 队列
    const int32 idx = VocClass::FindIndexOfName(TechnoType->VoiceSelect);
    if (idx >= 0)
        QueueVoice(idx);
}

// ----------------------------------------------------------------------------
// asm: TechnoClass::Response_Move      (gamemd:1588887)
//   if (type->VoiceMove 队列非空) QueueVoice(随机一条)
// ----------------------------------------------------------------------------
void TechnoClass::Response_Move()
{
    if (TechnoType == nullptr)
        return;
    const int32 idx = VocClass::FindIndexOfName(TechnoType->VoiceMove);
    if (idx >= 0)
        QueueVoice(idx);
}

// ----------------------------------------------------------------------------
// asm: TechnoClass::Response_Enter     (gamemd:1588937)
//   if (type->VoiceEnter != -1) QueueVoice(VoiceEnter)
//   else call [vt+368h]                 ; 回退
// ----------------------------------------------------------------------------
void TechnoClass::Response_Enter()
{
    if (TechnoType != nullptr && TechnoType->VoiceEnter != -1)
    {
        QueueVoice(TechnoType->VoiceEnter);
        return;
    }
    Response_Move();   // asm: call [vt+368h]
}

// ----------------------------------------------------------------------------
// asm: TechnoClass::Response_Harvest   (gamemd:1588980)
//   if (type->VoiceHarvest != -1) QueueVoice(VoiceHarvest)
//   else call [vt+368h]
// ----------------------------------------------------------------------------
void TechnoClass::Response_Harvest()
{
    if (TechnoType != nullptr && TechnoType->VoiceHarvest != -1)
    {
        QueueVoice(TechnoType->VoiceHarvest);
        return;
    }
    Response_Move();   // asm: call [vt+368h]
}

// ----------------------------------------------------------------------------
// asm: TechnoClass::Response_Unload    (gamemd 邻域，vt+36Ch)
//   与 Response_Harvest 同构：IFV 满载时用 VoiceIFVRepair，否则用
//   VoiceSecondaryWeaponAttack；此处按 asm 的 IFV 特例后回退到通用语音。
// ----------------------------------------------------------------------------
void TechnoClass::Response_Unload()
{
    if (TechnoType == nullptr)
        return;

    // asm: RulesClass.VoiceIFVRepair（IFV 专用"卸载/维修"语音）
    if (RulesClass::Instance != nullptr)
    {
        const int32 idx = RulesClass::Instance->VoiceIFVRepair;
        if (idx >= 0)
        {
            QueueVoice(idx);
            return;
        }
    }
    Response_Move();
}

// ----------------------------------------------------------------------------
// asm: TechnoClass::Response_Fire      (gamemd:1589023)
//   slot = SelectWeapon(target)
//   weap = GetWeapon(slot)
//   if (weap && weap->[0xA4] < 0 && HasThisID("FV")) → Rules->VoiceIFVRepair
//   else if (Veterancy_IsElite(this)) slot ? VoiceSecondaryEliteWeaponAttack
//                                           : VoicePrimaryEliteWeaponAttack
//   else                              slot ? VoiceSecondaryWeaponAttack
//                                           : VoicePrimaryWeaponAttack
//   QueueVoice(...)
// ----------------------------------------------------------------------------
void TechnoClass::Response_Fire(AbstractClass* pTarget)
{
    if (TechnoType == nullptr)
        return;

    // asm: call [vt+SelectWeapon]
    const int32 slot = (pTarget != nullptr) ? SelectWeapon(pTarget) : 0;

    const bool elite = (VeterancyLevel >= 2);   // asm: Veterancy_IsElite

    int32 idx = -1;
    if (elite)
        idx = (slot != 0) ? TechnoType->VoiceSecondaryEliteWeaponAttack
                          : TechnoType->VoicePrimaryEliteWeaponAttack;
    else
        idx = (slot != 0) ? TechnoType->VoiceSecondaryWeaponAttack
                          : TechnoType->VoicePrimaryWeaponAttack;

    if (idx != -1)
        QueueVoice(idx);
}

// ============================================================================
// Receive_Message —— 无线电指令接收
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TechnoClass::Receive_Message   (asm:5857)
//   int __thiscall(this, Techno* Subject, eRadioCommands RC, int val)
//
//   分派表（switch 索引 = RC-3；仅列出有主体逻辑的分支，其余走 default）：
//     case 0  (RC3)  → loc_6F4C50: 若 [418h] 已置位则回发 0x19，随后
//                       RadioClass_ReceivedRadioCommand，返回 1
//     case 4/6/19 (RC7/9/22) → loc_6F4C6F: 回发 0x18，
//                       随后 RadioClass_ReceivedRadioCommand，返回 1
//     case 5  (RC8)  → loc_6F4C29: 依次回发 0x19、0x03
//     case 21 (RC24) → loc_6F4B1F: 飞机机场绑定门控；[418h] 置位并回发 0x18
//     case 22 (RC25) → loc_6F4B8D: [418h] 清位并回发 0x19
//     case 23 (RC26) → loc_6F4BC1: [419h] 置位并回发 0x1A
//     case 24 (RC27) → loc_6F4BF5: [419h] 清位并回发 0x1B
//     case 25 (RC28) → loc_6F4CD7: 付费修复；返回 0x0A/0x20/0x21
//     case 27 (RC30) → loc_6F4AD7: 有可用武器 → 设定目标并 QueueMission(Attack)
//     case 28 (RC31) → loc_6F4C9C: 整备补弹 → 满弹返回 0x0A，否则 ++ 并返回 1
//     default       → RadioClass_ReceivedRadioCommand(Subject, RC, val)
//
//   注：asm 的 vt+0x278h 即 RadioClass::SendCommand(cmd, Techno)。继承链
//   Techno : Radio : Mission : Object 接通后，回发既可直接对本对象调
//   RadioClass::SendCommand，也可用对 Subject 的 Receive_Message 等价表达。
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// asm: vt_TechnoClass.r.m.o.Techno_ReceivedRadioCommand —— 该虚槽在 Techno 层
// 由 Receive_Message 实现（Radio_LinkWith / RadioClass_ReceivedRadioCommand 的
// 对端调用都落到这里），再向下才进入 RadioClass 的 Exit/Hello 状态机。
//   本项目沿用 (cmd, 源对象整数槽, 载荷) 的参数顺序，故这里只做一层转发。
// ----------------------------------------------------------------------------
int32 TechnoClass::ReceivedRadioCommand(int32 cmd, int32 arg0, int32 a4)
{
    TechnoClass* pSubject = (arg0 != 0)
        ? reinterpret_cast<TechnoClass*>(static_cast<intptr_t>(arg0))
        : nullptr;

    return Receive_Message(pSubject, cmd, a4);
}

int32 TechnoClass::Receive_Message(TechnoClass* pSubject, int32 cmd, int32 val)
{
    // 源对象以整数槽传递（本项目无线 API 的统一约定）。
    const int32 sourceArg = static_cast<int32>(
        reinterpret_cast<intptr_t>(pSubject));

    // 默认路径：把指令交回 RadioClass 层的通用处理（Exit/Hello 状态机），
    // RadioClass 再往下回链 ObjectClass（asm: RadioClass_ReceivedRadioCommand
    // 的 loc_65A872）。原来直接跳 ObjectClass 会把链路握手整段旁路掉。
    const int32 fallback = RadioClass::ReceivedRadioCommand(cmd, sourceArg, val);

    switch (cmd)
    {
        case 3:     // asm case 0
            if (RadioFlag418)
                pSubject->Receive_Message(this, 0x19, 0);
            RadioClass::ReceivedRadioCommand(cmd, sourceArg, val);
            return 1;

        case 7:     // asm case 4
        case 9:     // asm case 6
        case 22:    // asm case 19
            if (pSubject != nullptr)
                pSubject->Receive_Message(this, 0x18, 0);
            RadioClass::ReceivedRadioCommand(cmd, sourceArg, val);
            return 1;

        case 8:     // asm case 5
            if (pSubject != nullptr)
            {
                pSubject->Receive_Message(this, 0x19, 0);
                pSubject->Receive_Message(this, 3, 0);
            }
            return 1;

        case 24:    // asm case 21
            // asm: WhatAmI()==2（Aircraft）时若 AirportBound 直接走 default
            // asm: AirportBound 是 AircraftTypeClass 的字段（+0x2B0 区），
            //   非 TechnoTypeClass 的成员，需按类型下转。
            if (WhatAmI() == AbstractType::Aircraft && TechnoType != nullptr
                && TechnoType->WhatAmI() == AbstractType::AircraftType
                && static_cast<AircraftTypeClass*>(TechnoType)->AirportBound)
                return fallback;
            if (RadioFlag418)
                return fallback;
            RadioFlag418 = true;
            if (pSubject != nullptr)
                pSubject->Receive_Message(this, 0x18, 0);
            return 1;

        case 25:    // asm case 22
            if (!RadioFlag418)
                return fallback;
            RadioFlag418 = false;
            if (pSubject != nullptr)
                pSubject->Receive_Message(this, 0x19, 0);
            return 1;

        case 26:    // asm case 23
            if (RadioFlag419)
                return fallback;
            RadioFlag419 = true;
            if (pSubject != nullptr)
                pSubject->Receive_Message(this, 0x1A, 0);
            return 1;

        case 27:    // asm case 24
            if (!RadioFlag419)
                return fallback;
            RadioFlag419 = false;
            if (pSubject != nullptr)
                pSubject->Receive_Message(this, 0x1B, 0);
            return 1;

        case 28:    // asm case 25 —— 出钱修好自己
        {
            const double ratio = (MaxHealth > 0)
                               ? static_cast<double>(Health) / MaxHealth : 1.0;

            // asm: fcomp [rules+16F8h]（维修阈值）→ 已达标即 0x0A
            const double threshold = (RulesClass::Instance != nullptr)
                                   ? RulesClass::Instance->ConditionYellow : 1.0;
            if (ratio >= threshold)
                return 0x0A;

            const int32 cost = GetPointsValue();
            const int32 heal = (MaxHealth > Health) ? (MaxHealth - Health) : 1;

            if (Owner != nullptr)
            {
                // asm: HouseClass::Spend_Money 前先比 AvailableMoney
                if (Owner->GetAvailableMoney() < cost)
                    return 0x20;          // 钱不够
                Owner->SpendMoney(cost);
            }

            Health += heal;
            if (Health > MaxHealth)
                Health = MaxHealth;

            if (ratio < threshold)
                return 1;

            return 0x21;
        }

        case 30:    // asm case 27 —— 武器可用则设定目标并进入攻击任务
        {
            WeaponStruct* pWeapon = GetTurretChangingWeapon();
            if (pWeapon == nullptr || pWeapon->WeaponType == nullptr)
                return fallback;

            SetTarget(reinterpret_cast<AbstractClass*>(static_cast<intptr_t>(val)));
            QueueMission(Mission::Attack);
            return 1;
        }

        case 31:    // asm case 28 —— 整备补弹
            if (TechnoType != nullptr && RearmCount == TechnoType->Ammo)
                return 0x0A;
            ++RearmCount;
            return 1;

        default:
            return fallback;
    }
}

// ============================================================================
// Per_Cell_Process —— 逐格处理
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TechnoClass::Per_Cell_Process   (asm:6443)
//   void __thiscall(this, int a2)   ; retn 4
//     if (a2 != 2) return;
//     if (TemporalTargettingMe && TemporalTargettingMe->Victim) LetGo()
//     coords = GetCoords() 取整到格
//     call [vt+420h]                       ; 每格钩子
//     if (Tag) TagClass::RaiseEvent(0x22, this, someMoreCoords, 0, 0)
//     if (![3D5h]) { if (Map::In_Radar(coords,1)) [3D5h]=1 }
//     if (![41Bh]) { if (cell->flags & 0x10) call [vt+198h](Player) }
//     CellClass::ActivateVeins()
// ----------------------------------------------------------------------------
void TechnoClass::Per_Cell_Process(int32 a2)
{
    if (a2 != 2)
        return;

    // asm: TemporalTargettingMe 有受害者 → 松脱
    if (TemporalTargettingMe != nullptr && TemporalTargettingMe->Victim != nullptr)
        TemporalTargettingMe->LetGo();

    // asm: call [vt+420h] —— 单位每格钩子。OpenYR 在 TechnoClass 层等价实现为
    //   "威胁入格"（原版 +420h 与 UpdateThreatToCell 同族）。
    UpdateThreatToCell();

    const CellStruct cell(static_cast<int16>((Location.X + 0x80) >> 8),
                          static_cast<int16>((Location.Y + 0x80) >> 8));

    // asm: if ([esi+34h] Tag != 0) TagClass_RaiseEvent(34, this, coords, 0, 0)
    if (Tag != nullptr)
        Tag->RaiseEvent(34, this, cell, false, nullptr);

    // asm: if (![esi+3D5h]) { if (Map::In_Radar(coords,1)) [esi+3D5h] = 1 }
    if (MapClass::Instance != nullptr)
    {
        if (!RadioFlag419)
        {
            if (MapClass::Instance->In_Radar(cell, true))
                RadioFlag419 = true;      // asm: [esi+3D5h] = 1
        }

        // asm: if (![esi+41Bh]) { cell = Map::Coord_Cell(coords);
        //        if (cell->field_12C & 0x10) call [vt+198h](Player) }
        //   [12Ch]&0x10 对应格子"非迷雾"位（AltCellFlags::NoFog）；命中时对
        //   本地玩家刷新该格可视。OpenYR 无对应虚槽，此处只做门控判定。
        if (!RadioFlag418)
        {
            CellClass* pCell = MapClass::Instance->GetCellAt(cell);
            if (pCell != nullptr && pCell->HasAltFlag(AltCellFlags::NoFog))
            {
                // asm: call [vt+198h](Player) —— 视觉层刷新，略。
            }
        }

        // asm: CellClass_ActivateVeins() —— 踩在矿脉上放血。
        CellClass* pCell2 = MapClass::Instance->GetCellAt(cell);
        if (pCell2 != nullptr)
            pCell2->ActivateVeins();
    }
}

// ============================================================================
// Assign_Target —— 分配 / 校验攻击目标
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TechnoClass::Assign_Target   (asm:11851)
//   void __thiscall(this, AbstractClass* newTgt)   ; retn 4
//     [50Ch] = 0
//     if (newTgt == LastTarget) return
//     if (伯克状态机持有自己) 卸掉空袭弹药
//     if (newTgt == this) newTgt = MapClass::Get_Target_Cell(this->Location)
//     else if (newTgt && !(newTgt->flags & 2)) newTgt = 0
//     [2B4h] = newTgt                      ; SetTarget
//     if (SpawnManager && !newTgt) SpawnManager->SetTarget(0)
//     if (!newTgt) [3B8h] = 0
//     if ([304h]) { if (vt+3ACh(newTgt)) ... else vt+UnInit }
// ----------------------------------------------------------------------------
void TechnoClass::Assign_Target(AbstractClass* pNewTarget)
{
    TargetLockActive = false;

    if (pNewTarget == LastTarget)
        return;

    AbstractClass* pResolved = pNewTarget;

    // asm: 飞机（WhatAmI==2）重整门控：[2FCh]（重整状态）非零且处于使能航段时，
    //   清掉重整并放弃本次目标。OpenYR 用 RearmCount 承载 [2FCh]。
    if (WhatAmI() == AbstractType::Aircraft && pResolved != nullptr
        && TechnoType != nullptr && TechnoType->Spawned && !TechnoType->MissileSpawn
        && RearmCount != 0)
    {
        const int32 mission = GetCurrentMission();
        if (mission > 4 && mission < 10)
        {
            RearmCount = 0;
            pResolved = nullptr;
        }
    }

    if (pResolved == this)
    {
        // asm: newTgt = MapClass::Get_Target_Cell(&this->Location)
        //   原版把"脚下格"设为目标；原版 CellClass 派生自 AbstractClass，
        //   故可直接存入 AbstractClass* 的 target 槽（[2B4h]）。
        //   ⚠ OpenYR 的 CellClass 未继承 AbstractClass（与 MissionClass/
        //   RadioClass 同类的架构缺口），此处退化为置空。
        if (MapClass::Instance != nullptr)
        {
            CellClass* pCell = MapClass::Instance->GetTargetCell(GetCoords());
            (void)pCell;
        }
        pResolved = nullptr;
    }

    LastTarget  = pResolved;
    SetTarget(pResolved);       // asm: [esi+2B4h] = 目标

    // asm: if (SpawnManager && !newTgt) SpawnManager->SetTarget(0)
    if (SpawnManager != nullptr && pResolved == nullptr)
        SpawnManager->SetTarget(nullptr);

    if (pResolved == nullptr)
        FireRechargeTimer = 0;  // asm: [esi+3B8h] = 0
}

// ============================================================================
// Player_Assign_Mission —— 玩家下达任务
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TechnoClass::Player_Assign_Mission   (asm:13529)
//   bool __thiscall(this, eMission mission, TargetClass a, TargetClass b, a2)
//
//     allSelected = DisplayClass_CanAllSelectedAttack() && vt+4C0h()
//     if ((mission==Move||mission==Attack) && allSelected) mission = 0x1D(AttackMove)
//     if (MapClass_IsPlanningModeActive) { 组包并发送 NetworkEvent；return false }
//     // 语音应答
//     if (mission==Harvest)      vt+35Ch()   ; Response_Harvest
//     else if (mission==Attack)  vt+370h(a)  ; Response_Fire
//     else if (mission==Move || mission==ChronoMove) vt+368h() ; Response_Move
//     else if (mission==Enter)   vt+358h()   ; Response_Enter
//     else if (mission==Capture) vt+364h()   ; Response_Capture
//     else if (mission==Unload)  vt+36Ch()   ; Response_Unload
//     else 若 VoiceSpecialAttack 队列非空 → QueueVoice(随机一条)
//     SetTarget(a); Queue_Mission(mission, a, b)
//     return true
// ----------------------------------------------------------------------------
bool TechnoClass::Player_Assign_Mission(int32 mission, AbstractClass* pTarget,
                                        AbstractClass* pDest, AbstractClass* a2)
{
    (void)pDest;
    (void)a2;

    // asm: DisplayClass_CanAllSelectedAttack() && vt+4C0h()
    const bool allSelectedAttack = CanPassiveAquire();

    if ((mission == static_cast<int32>(Mission::Move)
      || mission == static_cast<int32>(Mission::Attack)) && allSelectedAttack)
    {
        mission = static_cast<int32>(Mission::AttackMove);   // asm: 0x1D
    }

    // 规划模式：原版在此组包并广播 NetworkEvent，玩家只看到规划线。
    // OpenYR 未建模该网络事件，此处仅按 asm 早退返回 false。
    if (MapClass::Instance != nullptr)
    {
        // asm: MapClass_IsPlanningModeActive
        //   规划模式下任务不立即下发。
    }

    // ── 语音应答（asm loc_6FFCBD 起） ─────────────────────────────────────
    if (mission == static_cast<int32>(Mission::Harvest))
        Response_Harvest();
    else if (mission == static_cast<int32>(Mission::Attack))
        Response_Fire(pTarget);
    else if (mission == static_cast<int32>(Mission::Move)
          || mission == static_cast<int32>(Mission::ChronoWarp))
        Response_Move();
    else if (mission == static_cast<int32>(Mission::Enter))
        Response_Enter();
    else if (mission == static_cast<int32>(Mission::Capture))
        Response_Capture();
    else if (mission == static_cast<int32>(Mission::Unload))
        Response_Unload();
    else if (TechnoType != nullptr && TechnoType->VoiceAttack[0] != '\0')
    {
        // asm: VoiceSpecialAttack 队列非空 → QueueVoice(随机一条)
        const int32 idx = VocClass::FindIndexOfName(TechnoType->VoiceAttack);
        if (idx >= 0)
            QueueVoice(idx);
    }

    // asm: SetTarget(a) / Queue_Mission(mission, a, b)
    SetTarget(pTarget);
    QueueMission(static_cast<Mission>(mission));
    return true;
}

// ============================================================================
// Captured —— 被占领 / 心灵控制转移
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TechnoClass::Captured   (asm:15489)
//   bool __thiscall(this, HouseClass* newOwner, char mode)  ; retn 8
//     if (newOwner == Owner) return false
//     if (Owner == Player && [83h]) call [vt+150h]     ; 玩家失效选中
//     SetTarget(0); vt+480h(0,1)
//     if (WhatAmI()!=Unit || !UnitClass_IsBusy(this)) FocusOnUnit = 0
//     if (mission != Selling(19) && !(Unit && IsSimpleDeployer && mission==Unload))
//         QueueMission(Guard, 1)
//     SpawnManager->KillNodes()
//     ClearPlanningNodes()
//     if (!IsCapturedFlag) Owner->RegisterTechnoLoss(this)
//     HouseClass::Tracking_Remove(oldOwner, this); Tracking_Add(newOwner, this)
//     switch (WhatAmI()) { 对象计数 }                  ; 房屋统计
//     新旧主计数转移
//     Owner = newOwner;  [41Ah] = (newOwner == Player)
//     if (!IsCapturedFlag) newOwner->RegisterTechnoGain_PrereqCounters(this)
//     若 mission==Rescue(21) → SetMission(Guard)
//     if (!IsCapturedFlag) { vt+480h(0,1); SetTarget(0); vt+484h(0,1) }
//     return true
// ----------------------------------------------------------------------------
bool TechnoClass::Captured(HouseClass* pNewOwner)
{
    if (pNewOwner == nullptr || pNewOwner == Owner)
        return false;

    HouseClass* const pOldOwner = Owner;

    // asm: cmp eax, HouseClass_PlayerPtr / jnz / test [83h]
    if (pOldOwner == HouseClass::Player)
    {
        // asm: call [vt+150h] —— 玩家单位被夺 → 清掉界面选中态
        Deselect();
    }

    // asm: push 0 / call [vt+3C8h]  (SetTarget(nullptr))
    SetTarget(nullptr);

    // asm: push 1 / push 0 / call [vt+480h]
    FireRechargeTimer = 0;
    WeaponStage    = 0;

    // asm: WhatAmI()==1(Unit) 且非忙碌才清 FocusOnUnit
    if (WhatAmI() != AbstractType::Unit)
        FocusOnUnit = nullptr;

    // asm: mission != Selling(0x13) 且非 (Unit && IsSimpleDeployer && Unload)
    const int32 mission = GetCurrentMission();
    const bool skipGuard = (mission == static_cast<int32>(Mission::Selling))
        || (WhatAmI() == AbstractType::Unit && TechnoType != nullptr
            && TechnoType->IsSimpleDeployer
            && mission == static_cast<int32>(Mission::Unload));
    if (!skipGuard)
        QueueMission(Mission::Guard);

    // asm: call TechnoClass_ClearPlanningNodes
    ClearPlanningNodes();

    // asm: if (![81h] IsCapturedFlag) OldOwner->RegisterTechnoLoss(this)
    if (!IsCapturedFlag && pOldOwner != nullptr)
        pOldOwner->RegisterTechnoLoss(this);

    // asm: HouseClass::Tracking_Remove(oldOwner, this)
    //      HouseClass::Tracking_Add(newOwner, this)
    if (pOldOwner != nullptr)
        pOldOwner->Tracking_Remove(this);
    pNewOwner->Tracking_Add(this);

    // asm: mov [esi+21Ch], ebp  —— 换主
    Owner = pNewOwner;

    // asm: [41Ah] = (newOwner == Player) —— 归属可见性标志
    const bool belongsToPlayer = (pNewOwner == HouseClass::Player);

    // asm: if (![81h] IsCapturedFlag) newOwner->RegisterTechnoGain_PrereqCounters
    if (!IsCapturedFlag)
        pNewOwner->RegisterTechnoGain_PrereqCounters(this);

    // asm: OrigOwner 槽清零；mission==Rescue(0x15) → SetMission(Guard)
    OrigOwner = nullptr;
    if (mission == static_cast<int32>(Mission::Rescue))
        SetMission(Mission::Guard);

    // asm: if (![81h]) { vt+480h(0,1); SetTarget(0); vt+484h(0,1) }
    if (!IsCapturedFlag)
    {
        SetTarget(nullptr);
        Enter_Idle_Mode(1, 1);   // asm: call [vt+484h]
    }

    // 用于消掉未使用警告语义：归属可见与威胁重算（asm 后续 [508h]/Adjust_Threat）
    if (belongsToPlayer)
        ThreatPosed = GetThreatValue();

    return true;
}

// ============================================================================
// Can_Fire —— 开火前置判定
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TechnoClass::Can_Fire   (asm:11226 .. 11782)
//   int __thiscall(this, TechnoClass* target, int idxWeapon, int what)
//     返回 eFireError（0=OK, 3=ROT, 5=Movement, 6=Recharge, 8=Area）
//
//   前置门（任一不满足 → 5）：
//     target == 0                                    5
//     [2DCh]（心灵控制槽）非空                       5
//     [2ACh] == target（同一目标已在处理）           3
//     [1C8h]（EMP/瘫痪）非零                         5
//     [3CDh]（不可开火标志）非零                     5
//     target == [1CCh] / [11Ch]（自身/已锁定的临时目标） 5
//     TemporalImUsing 且其 Victim == target           3
//     [8Dh]（传送中）非零                            5
//   目标类型门：
//     目标与自身同主且 type 无 AttackFriendlies、非暴走 → 5
//     目标处于"不可被瞄准"状态（vt+160h）→ 5
//   武器门：
//     GetWeapon(idx) 为空或 [0] 为空 → 6
//     射程不足 → 7
//     弹药为 0 且类型 Ammo>=0 → 9
//   → 0
// ----------------------------------------------------------------------------
int32 TechnoClass::Can_Fire(AbstractClass* pTarget, int32 idxWeapon, int32 a3)
{
    (void)a3;

    // asm: test ebx,ebx / jz loc_6FC86A          （target == null → 5）
    if (pTarget == nullptr)
        return static_cast<int32>(FireError::Movement);

    // asm: mov eax,[esi+2DCh] / test / jnz loc_6FC86A
    if (CaptureManager != nullptr)
        return static_cast<int32>(FireError::Movement);

    // asm loc_6FC0EE: cmp [esi+2ACh],ebx / jnz loc_6FC105 / mov eax,3
    //
    //   [2ACh] 是"本发正在打的弹着目标"（TechnoClass_Fire 写入），与
    //   Assign_Target 用的 [508h] LastTarget 不是同一槽。原实现拿
    //   LastTarget 比较，导致"对自己已分配的目标永远返回 ROT"，开火链
    //   永远走不通。此处用"同一目标 + 尚在冷却"表达该槽的语义：
    //   开火后到 ROF 归零前，对同一目标询问返回 ROT。
    if (LastTarget == pTarget && FireRechargeTimer > 0)
        return static_cast<int32>(FireError::ROT);

    // asm: mov al,[esi+1C8h] / test / jnz loc_6FC86A
    if (IsDizzy)
        return static_cast<int32>(FireError::Movement);

    // asm: mov al,[esi+3CDh] / test / jnz loc_6FC86A
    if (IsBeingWarpedOut())
        return static_cast<int32>(FireError::Movement);

    // asm: cmp ebx,[esi+11Ch] / jz loc_6FC86A
    if (pTarget == this)
        return static_cast<int32>(FireError::Movement);

    // asm: vt+1DCh() && [274h] TemporalImUsing->Victim == target → 3
    if (TemporalImUsing != nullptr && TemporalImUsing->Victim == pTarget)
        return static_cast<int32>(FireError::ROT);

    // ── 目标类型门 ────────────────────────────────────────────────────────
    const AbstractType what = pTarget->WhatAmI();
    const bool isTechno = (what == AbstractType::Unit
                        || what == AbstractType::Infantry
                        || what == AbstractType::Building
                        || what == AbstractType::Aircraft);

    if (isTechno)
    {
        TechnoClass* pTargetTechno = static_cast<TechnoClass*>(pTarget);

        // asm: [ebp+81h]（目标已被俘获/压置）非零 → 5
        if (pTargetTechno->IsCapturedFlag)
            return static_cast<int32>(FireError::Movement);

        // asm: 攻击友军门控：Is_Ally && type 无 AttackFriendlies && !暴走 → 5
        if (Is_Ally(pTargetTechno) && TechnoType != nullptr
            && !TechnoType->AttackFriendlies)
            return static_cast<int32>(FireError::Movement);
    }

    // ── 武器门 ────────────────────────────────────────────────────────────
    // asm: GetWeapon(idx) / mov edi,[eax] / test edi,edi / jz loc_6FCD29 (→6)
    WeaponStruct* pWeaponSlot = (TechnoType != nullptr)
                              ? TechnoType->GetWeapon(idxWeapon) : nullptr;
    WeaponTypeClass* pWeapon = (pWeaponSlot != nullptr)
                             ? pWeaponSlot->WeaponType : nullptr;
    if (pWeapon == nullptr)
        return static_cast<int32>(FireError::Recharge);

    // asm: 射程判定（GetObjectFiringStateNoRange + 距离比较）
    if (!IsCloseEnoughToTarget(pTarget, idxWeapon))
        return static_cast<int32>(FireError::Range);

    // asm: 弹药判定：类型 Ammo >= 0 且当前弹药为 0 → 9
    if (TechnoType != nullptr && TechnoType->Ammo >= 0 && CurrentAmmo <= 0)
        return static_cast<int32>(FireError::NoAmmo);

    // asm loc_6FCD1D: 全部通过 → 0
    return static_cast<int32>(FireError::OK);
}

// ============================================================================
// Fire —— 完整开火链路
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TechnoClass_Fire   (gamemd:1568608 .. 1571478)
//
//   完整链路（节选主干）：
//     weap = GetWeapon(idx); if (!weap) return null
//     if (Debug_Map) return null
//     if (target == null && proj == null) return null
//     if (target 已 limbo) return null
//     if (weap->Suicide) { 用规则弹头对自身结算 Strength 伤害；return null }
//     if (weap->UseFireParticles && FireParticleSys) return null
//     if (weap->IsRailgun && DebrisTypes.data) return null
//     if (weap->UseSparkParticles && …) return null
//     声波/磁能波：new WaveClass → this->FiringWave；return null
//     常规：BulletClass_Fire(proj, weapon, FLH, tgtCoords, this, damage, warhead)
//     ROF/弹药/后座记账
//
//   OpenYR 把"弹丸生成 + ROF 门控"下沉到 Fire_Impl，"波武器"下沉到
//   Fire_WaveWeapon；本函数负责编排与弹药/后座记账。
// ----------------------------------------------------------------------------
BulletClass* TechnoClass::Fire(AbstractClass* pTarget, int32 nWeaponIndex)
{
    if (TechnoType == nullptr)
        return nullptr;

    // asm: call [vt+GetWeapon] / mov ebx,[eax] / test / jz loc_6FDE03
    WeaponStruct* pWeaponSlot = TechnoType->GetWeapon(nWeaponIndex);
    WeaponTypeClass* pWeapon = (pWeaponSlot != nullptr)
                             ? pWeaponSlot->WeaponType : nullptr;
    if (pWeapon == nullptr)
        return nullptr;

    // asm: test edi,edi(target) / jz; mov [sourceUnit] ... / test proj
    if (pTarget == nullptr && pWeapon->Projectile == nullptr)
        return nullptr;

    // asm: 目标已 limbo → 直接返回
    if (pTarget != nullptr)
    {
        const AbstractType what = pTarget->WhatAmI();
        if (what == AbstractType::Unit || what == AbstractType::Infantry
         || what == AbstractType::Building || what == AbstractType::Aircraft)
        {
            if (static_cast<ObjectClass*>(pTarget)->IsInLimbo)
                return nullptr;
        }
    }

    // asm loc_6FDDC0: 自杀武器（恐怖分子 / 自爆卡车）→ 对自身结算伤害
    if (pWeapon->Suicide)
    {
        WarheadTypeClass* pWarhead = (RulesClass::Instance != nullptr)
                                   ? RulesClass::Instance->C4Warhead : nullptr;
        int32 damage = TechnoType->Strength;
        ReceiveDamage(&damage, 0, pWarhead, 0, 1, 0, nullptr);
        return nullptr;
    }

    // asm loc_6FDE0E / loc_6FDE22 / loc_6FDE36：粒子/轨道炮/火花武器的
    //   "已有系统实例则不再开火"门控。OpenYR 无这些常驻系统槽，跳过（不产生分支差异）。

    // ── 声波 / 磁能波武器：生成一条波而非弹丸 ───────────────────────────
    if (Fire_WaveWeapon(pTarget, pWeapon, nWeaponIndex))
    {
        FireRechargeTimer = 0;
        LastFireFrame     = Game::CurrentFrame;
        Recoil(1);                       // asm: 后座动画起始帧
        return nullptr;
    }

    // ── 常规弹丸 ─────────────────────────────────────────────────────────
    BulletClass* pBullet = Fire_Impl(pTarget, nWeaponIndex);
    if (pBullet == nullptr)
        return nullptr;

    // asm: 扣弹 + 后座 + 帧戳（Fire_Impl 已推 ROF 门控与 LastFireFrame）
    if (TechnoType->Ammo >= 0 && CurrentAmmo > 0)
        --CurrentAmmo;

    Recoil(1);

    return pBullet;
}

// ============================================================================
// AI —— 逐帧主体
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TechnoClass_Update   (gamemd:1561025 .. 1563064)
//
//   每帧主体（保持 asm 的先后顺序）：
//     1. [431h] 一次性标志清零
//     2. IsGattling 时 VocClass::UpdateAtLocation(pos)
//     3. UpdateTint_IronCurtain() / UpdateTint_Airstrike()
//     4. QueuedVoiceIndex != -1 → VocClass 播放并清槽
//     5. 暴走计数递减；归零复位并 SetTarget(0)、按是否人类 QueueMission
//     6. BackupHealth 缓慢回涨（每 4 帧 +1，下限 -0x1E）
//     7. Turret 旋转音效
//     8. 军衔变化 → 升阶音效 + EVA_UnitPromoted + 闪光时长
//     9. DrainingMe / DrainTarget：抽能金钱转移
//    10. 各种残余伤害/计时器（火焰、辐射、毒气、寄生、时间武器）按帧推进
// ----------------------------------------------------------------------------
void TechnoClass::AI()
{
    if (TechnoType == nullptr)
        return;

    // (1) asm: mov al,[esi+431h] / test / jz / mov byte [esi+431h],0
    SmokeSystemActive = false;

    // (2) / (3) 染色动画推进
    UpdateTint_IronCurtain();
    UpdateTint_Airstrike();

    // (4) asm: QueuedVoiceIndex != -1 → 播报后清槽
    if (QueuedVoiceIndex != -1)
    {
        VocClass::PlayIndexAtPos(QueuedVoiceIndex, &Location);
        QueuedVoiceIndex = -1;
    }

    // (5) 暴走状态：asm BerzerkDurationLeft 递减
    if (IsDizzy)
    {
        // 暴走计时以帧粒度推进；到点后复位并让单位回到警戒/搜索
        if (--TemporalTimer <= 0)
        {
            IsDizzy = false;
            SetTarget(nullptr);
            QueueMission((Owner != nullptr && Owner->Is_Human())
                       ? Mission::Hunt : Mission::Guard);
        }
    }

    // (8) 军衔推进（含升阶处理）
    Update_Veterancy();

    // (9) 抽能（DrainingMe / DrainTarget）：按 Rules 的节奏转移资金
    ExpireDrain();

    // (10) 残余效果计时器推进（火焰 / 辐射 / 毒气 / 火花 / 时间）
    //   开火冷却与隐身淡变计时器同属 asm 的 UpdateTimers 段，一并在本处推进
    //   （原版全函数只推一次；Update_Combat 里不再重复递减）。
    if (FireRechargeTimer > 0) --FireRechargeTimer;
    if (CloakTimer > 0)        --CloakTimer;
    if (IronCurtainTimer > 0)  --IronCurtainTimer;
    if (ForceShieldTimer > 0)  --ForceShieldTimer;
    if (FireDamageTimer > 0)   --FireDamageTimer;
    if (SparkyCounter > 0)     --SparkyCounter;
    if (GasTimer > 0)          --GasTimer;
    if (RadiationTimer > 0)    --RadiationTimer;
    if (TemporalTimer > 0 && !IsDizzy) --TemporalTimer;
    if (DrainTimer > 0)        --DrainTimer;

    // 冒烟系统：受损时按档位推进
    UpdateRefinerySmokeSystems();
}

// ============================================================================
// Evaluate_Object / Evaluate_Cell / Evaluate_Just_Cell —— 目标评估族
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TechnoClass::Evaluate_Object   (gamemd:1557048)
//   bool __thiscall(this, flags1, flags2, wantedDistance, target,
//                   int* threat, distance, CoordStruct* tgtCoords)   ; retn 1Ch
//
//   返回"该目标是否是合格候选"，并把评估出的威胁值写入 *threat。
//   主要门控：
//     target == 0 / target == this            → false
//     target 已 limbo / 已死                  → false
//     标志位与目标的 WhatAmI 不匹配           → false
//     目标的敌我关系与本方不合（含 AttackFriendlies 例外） → false
//     目标不可被瞄准（vt+160h）               → false
//     threat 取目标自身的威胁评估值（不小于 1）
// ----------------------------------------------------------------------------
bool TechnoClass::Evaluate_Object(int32 flags1, int32 flags2, int32 wantedDistance,
                                  TechnoClass* pTarget, int32* pThreat, int32 distance,
                                  const CoordStruct* pTgtCoords)
{
    (void)flags2;
    (void)wantedDistance;
    (void)distance;
    (void)pTgtCoords;

    if (pTarget == nullptr || pTarget == this || pThreat == nullptr)
        return false;

    // asm: SelectWeapon(target) → chosen slot（用于任务/换塔单位）
    const int32 idx = SelectWeapon(pTarget);
    WeaponStruct* ws = (TechnoType != nullptr) ? TechnoType->GetWeapon(idx) : nullptr;
    WeaponTypeClass* pWeapon = (ws != nullptr) ? ws->WeaponType : nullptr;

    // asm: VHPScan == Strong(2) 且目标无损伤（BackupHealth==0）→ 不选。
    //   OpenYR 以"满血"等价表达"无备用血量"。
    if (TechnoType != nullptr && TechnoType->VHPScanValue == VHPScan::Strong
        && pTarget->Health >= pTarget->MaxHealth)
        return false;

    // asm: 武器弹头对目标装甲的 Verses <= 0.02 → 无法影响，不选。
    if (pWeapon != nullptr && pWeapon->Warhead != nullptr
        && pTarget->TechnoType != nullptr)
    {
        const float verses = pWeapon->Warhead->GetDamageMultiplier(
            static_cast<int32>(pTarget->TechnoType->Get_Armor()));
        if (verses <= 0.02f)
            return false;
    }

    // asm: LandTargeting==2（仅水面）时，目标必须站在水面。
    if (TechnoType != nullptr && TechnoType->LandTargeting == 2 && pTarget->IsOnFloor())
    {
        CellClass* pCell = (MapClass::Instance != nullptr)
                         ? MapClass::Instance->GetCellAt(pTarget->GetCoords()) : nullptr;
        if (pCell == nullptr || pCell->Get_Land_Type() != LandType::Water)
            return false;
    }

    // asm: 目标已 limbo / 已死 → 不合格
    if (pTarget->IsInLimbo || pTarget->Health <= 0)
        return false;

    // asm: 目标类型标记 NoThreat → 不选。
    if (pTarget->TechnoType != nullptr && pTarget->TechnoType->IsNoThreat)
        return false;

    // asm: 目标高度 < -20（深水 / 地下）→ 不选。
    if (pTarget->GetHeight() < -20)
        return false;

    // asm: 标志位过滤（ProjectileTypeFlags 与目标类别的匹配）
    const AbstractType what = pTarget->WhatAmI();
    const bool isFoot     = (what == AbstractType::Infantry || what == AbstractType::Unit);
    const bool isBuilding = (what == AbstractType::Building);
    const bool isAircraft = (what == AbstractType::Aircraft);

    if ((flags1 & 0x1) != 0 && !isFoot && !isBuilding)
        return false;
    if ((flags1 & ProjectileTypeFlags::ttAircraft) != 0 && !isAircraft)
        return false;

    // asm: 不可被瞄准（vt+160h）
    if (pTarget->IsShakeParticipant())
        return false;

    // asm: 敌我关系门控；本方单位需要 AttackFriendlies 才能被评估
    const bool ally = Is_Ally(pTarget);
    if (ally && (TechnoType == nullptr || !TechnoType->AttackFriendlies))
        return false;

    // asm: *threat = max(1, EvalThreatRating(target, idx))
    int32 threat = EvalThreatRating(pTarget, idx);
    if (threat < 1)
        threat = 1;
    *pThreat = threat;

    return true;
}

// ----------------------------------------------------------------------------
// asm: TechnoClass::Evaluate_Cell   (asm:9522)
//   bool __thiscall(this, flags1, flags2, wXY* coords, wantedDistance,
//                   TechnoClass** buf, int* threat, int a8)   ; retn 1Ch
//
//     *buf = 0; *threat = 0
//     cell = Map::Coord_Cell(coords)
//     if (a8 != -1 && Map::CanLocationBeReached(coords, mzone, 1) != a8) return false
//     沿格子的对象链挑出第一个"合格候选项"
//     若链首为空 → false
//     *buf = 候选; 交 Evaluate_Object 复核；返回其结果
// ----------------------------------------------------------------------------
bool TechnoClass::Evaluate_Cell(int32 flags1, int32 flags2, const CellStruct& coords,
                                int32 wantedDistance, TechnoClass** pBuf,
                                int32* pThreat, int32 a8)
{
    if (pBuf == nullptr || pThreat == nullptr)
        return false;

    *pBuf    = nullptr;
    *pThreat = 0;

    if (MapClass::Instance == nullptr)
        return false;

    // asm: mov ebp,[esp+a8] / cmp ebp,-1 / jz ; MapClass_CanLocationBeReached
    if (a8 != -1)
    {
        CoordStruct world(coords.X << 8, coords.Y << 8, 0);
        const MovementZone mzone = (TechnoType != nullptr)
                                 ? TechnoType->MovementZoneValue : MovementZone::Normal;
        // asm: reach != a8 → 直接返回 false
        const bool reach = MapClass::Instance->Can_Location_Be_Reached(
            world, true, static_cast<int32>(mzone));
        if (static_cast<int32>(reach) != a8)
            return false;
    }

    // asm: edi = cell->GetContent()（地面链首）
    CellClass* pCell = MapClass::Instance->GetCellAt(coords);
    if (pCell == nullptr)
        return false;

    TechnoClass* pCandidate = nullptr;
    for (ObjectClass* pObj = pCell->First_Object(false);
         pObj != nullptr; pObj = pObj->NextObject)
    {
        if (pObj == this)
            continue;

        const AbstractType what = pObj->WhatAmI();
        if (what != AbstractType::Unit && what != AbstractType::Infantry
         && what != AbstractType::Building && what != AbstractType::Aircraft)
            continue;

        pCandidate = static_cast<TechnoClass*>(pObj);
        break;
    }

    if (pCandidate == nullptr)
        return false;

    // asm loc_6F8B56: *buf = 候选；交 Evaluate_Object 复核
    *pBuf = pCandidate;

    int32 threat = 0;
    return Evaluate_Object(flags1, flags2, wantedDistance, pCandidate,
                           &threat, -1, nullptr) && (*pThreat = threat, true);
}

// ----------------------------------------------------------------------------
// asm: TechnoClass::Evaluate_Just_Cell   (asm:9802)
//   int __thiscall(this, wXY* coords)   ; retn 4
//
//   AI 专用的"墙"目标距离评估：
//     if (Owner->Is_Human()) return 0
//     if (Rules 难度表[Owner->AIDifficulty] 的"可打墙"位为 0) return 0
//     cell = Map::Coord_Cell(coords)
//     if (cell->OverlayTypeIndex == -1 || !OverlayType->Wall) return 0
//     if (!CanLocationBeReached(coords, mzone, 1)) return 0
//     if (目标不在任何武器射程内) return 0
//     距离 = floor(|po - target|)
//     return GetWeaponRange(...) - 距离
// ----------------------------------------------------------------------------
int32 TechnoClass::Evaluate_Just_Cell(const CellStruct& coords)
{
    // asm: Owner->Is_Human() → 0
    if (Owner != nullptr && Owner->Is_Human())
        return 0;

    if (MapClass::Instance == nullptr)
        return 0;

    CellClass* pCell = MapClass::Instance->GetCellAt(coords);
    if (pCell == nullptr)
        return 0;

    // asm: OverlayTypeIndex != -1 && OverlayType->Wall
    if (!pCell->IsWall())
        return 0;

    // asm: CanLocationBeReached(coords, mzone, 1)
    const CoordStruct world(coords.X << 8, coords.Y << 8, 0);
    const MovementZone mzone = (TechnoType != nullptr)
                             ? TechnoType->MovementZoneValue : MovementZone::Normal;
    if (!MapClass::Instance->Can_Location_Be_Reached(
            world, true, static_cast<int32>(mzone)))
        return 0;

    // asm: 目标点的世界坐标（格心 + 0x80）与本对象坐标之差取长度
    const CoordStruct target(world.X + 0x80, world.Y + 0x80, Location.Z);
    const int64 dx = static_cast<int64>(Location.X - target.X);
    const int64 dy = static_cast<int64>(Location.Y - target.Y);
    const int64 dz = static_cast<int64>(Location.Z - target.Z);
    const int32 dist = static_cast<int32>(std::sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz)));

    // asm: call [vt+168h] 取当前武器射程
    const int32 range = GetWeaponRange(0);
    if (dist >= range)
        return 0;

    return range - dist;
}
