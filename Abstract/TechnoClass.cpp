#include <Abstract/TechnoClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Combat/WeaponTypeClass.h>
#include <Combat/WarheadTypeClass.h>
#include <Combat/BulletClass.h>
#include <Houses/HouseClass.h>
#include <Game/Game.h>
#include <Game/Externs.h>

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
// Update loop (AI, combat, cloaking)
//
//  Drives the per-frame work for every TechnoClass instance.  The order
//  matters: cloaking must run before combat so a freshly-decloaked unit can
//  still fire this frame; repair / veterancy updates run last so they can
//  react to the combat results.
// ============================================================================
void TechnoClass::Update()
{
    // Chain parent (ObjectClass) - in the standalone build the parent has
    // no per-frame work, but the original binary uses this slot to update
    // the attachment list and the radar blip.

    Update_Cloak();
    Update_AI();
    Update_Combat();
    Update_Repair();
    Update_Veterancy();
}

// ============================================================================
// Update_AI
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
// Update_Combat
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

    // Tick the fire recharge timer.  Update_AI also does this, but
    // repeating it here keeps combat state consistent if a subclass
    // overrides Update_AI without chaining the base.
    if (FireRechargeTimer > 0)
        --FireRechargeTimer;

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
}

// ============================================================================
// Update_Cloak
//
//  Advances the cloak state machine.  When CloakState is "cloaking" the
//  alpha value fades toward zero; when "uncloaking" it fades toward 255.
// ============================================================================
void TechnoClass::Update_Cloak()
{
    if (CloakState == CloakStateEnum::Idle)
        return;

    if (CloakState == CloakStateEnum::Cloaking)
    {
        if (CloakAlpha > 0)
        {
            --CloakAlpha;
            if (CloakAlpha == 0)
                CloakState = CloakStateEnum::Cloaked;
        }
    }
    else if (CloakState == CloakStateEnum::Uncloaking)
    {
        if (CloakAlpha < 255)
        {
            ++CloakAlpha;
            if (CloakAlpha == 255)
                CloakState = CloakStateEnum::Idle;
        }
    }
}

// ============================================================================
// Update_Repair
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
// Update_Veterancy
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
BulletClass* TechnoClass::Fire_Impl(AbstractClass* pTarget, int32 nWeaponIndex)
{
    if (pTarget == nullptr)
        return nullptr;

    // Look up the weapon.  The full binary indexes into the TechnoType's
    // weapon list; here we just check the index is in range.
    if (nWeaponIndex < 0 || nWeaponIndex >= 18)
        return nullptr;

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

    // In the full binary this would allocate a BulletClass, set its
    // target / source / weapon pointers, and add it to the global bullet
    // array.  The standalone build has no bullet pool yet.
    return nullptr;
}

// ============================================================================
// TakeDamage implementation
//
//  Applies damage to this TechnoClass.  The warhead's Verses table modulates
//  the raw damage based on this unit's armor.  Returns true if the unit died
//  as a result of the damage.
// ============================================================================
bool TechnoClass::TakeDamage_Impl(int32 damage, ObjectClass* pSource,
                                  WarheadTypeClass* pWarhead)
{
    if (damage <= 0)
        return false;

    // The full binary looks up the warhead's Verses[Armor] entry and
    // scales the damage.  The standalone build applies the raw damage.
    Health -= damage;

    // Award experience to the attacker if one was supplied.
    if (pSource != nullptr)
    {
        // The full binary dispatches through HouseClass::GainExperience.
    }

    if (Health <= 0)
    {
        Health = 0;
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
// Get_Threat_Pos
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
    Compute_CRC_Abstract(crc);

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
// 原版汇编: TechnoClass_SelectWeapon（1548588 行区段）
// 语义: 目标在射程内且武器可用时返回该武器索引；
//       若武器不可用（弹药耗尽/未装填）返回 -1 表示无武器可用。
// ============================================================================
int32 TechnoClass::SelectWeapon(AbstractClass* pTarget)
{
    if (pTarget == nullptr || TechnoType == nullptr)
        return -1;

    CoordStruct selfPos;
    GetCoords(&selfPos);

    CoordStruct tgtPos;
    pTarget->GetCoords(&tgtPos);

    for (int32 i = 0; i < TechnoType->WeaponCount; ++i)
    {
        WeaponStruct* ws = TechnoType->GetWeapon(i);
        if (ws == nullptr || ws->WeaponType == nullptr)
            continue;
        if (!ws->WeaponType->CanFire(selfPos, tgtPos))
            continue;
        return i;
    }
    return -1;
}

// ============================================================================
// IsCloseEnoughToTarget — 目标是否在指定武器射程内
// 原版: TechnoClass_IsCloseEnoughToTarget（含 sub_48ABC0 / sub_4CC310 射程判定）
// ============================================================================
bool TechnoClass::IsCloseEnoughToTarget(AbstractClass* pTarget, int32 idxWeapon)
{
    if (pTarget == nullptr || TechnoType == nullptr)
        return false;

    CoordStruct selfPos;
    GetCoords(&selfPos);
    CoordStruct tgtPos;
    pTarget->GetCoords(&tgtPos);

    WeaponStruct* ws = TechnoType->GetWeapon(idxWeapon);
    if (ws == nullptr || ws->WeaponType == nullptr)
        return false;

    return ws->WeaponType->IsInRange(selfPos, tgtPos);
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
// 原版: TechnoClass_RegisterDestruction（539 行）
// 语义: 从全局数组移除、通知所属阵营（经济返还/科技树状态）、
//       清除威胁/雷达贡献、释放占领者。
// ============================================================================
void TechnoClass::RegisterDestruction()
{
    // Drop any gap-generator contribution (original CreateGap/DeleteGap).
    DeleteGap();

    Remove_From_Array(this);
}

// ============================================================================
// RegisterLoss — 阵营失陷登记
// 原版: TechnoClass_RegisterLoss（383 行）
// ============================================================================
void TechnoClass::RegisterLoss()
{
    DeleteGap();
}

// ============================================================================
// GetFLH — 计算炮口/开火点（Forward, Lateral, Height 偏移 + 座架旋转）
// 原版: TechnoClass_GetFLH（270 行）
// ============================================================================
CoordStruct TechnoClass::GetFLH(int32 nWeaponIndex, bool muzzle)
{
    CoordStruct ret(0, 0, 0);
    if (TechnoType == nullptr)
        return ret;

    // 从武器槽位取 FLH 数据（WeaponStruct 后随 FLH 偏移）；
    // 简化路径：使用类型定义的默认开火高度。
    ret.Z = Get_ZAdjustment();
    return ret;
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
// 原版: TechnoClass_ShouldRetaliate（380 行）
// 语义: 拥有反击武器、目标敌对、射程内、且未被冻结（铁幕等）时还击。
// ============================================================================
bool TechnoClass::ShouldRetaliate(TechnoClass* pAttacker)
{
    if (pAttacker == nullptr)
        return false;
    if (!Is_Enemy(pAttacker))
        return false;
    if (IsShielded() || IsTemporalized())
        return false;

    // 检查是否有任意武器能打到攻击者。
    CoordStruct selfPos;
    GetCoords(&selfPos);
    CoordStruct atkPos;
    pAttacker->GetCoords(&atkPos);

    if (TechnoType == nullptr)
        return false;
    for (int32 i = 0; i < TechnoType->WeaponCount; ++i)
    {
        WeaponStruct* ws = TechnoType->GetWeapon(i);
        if (ws != nullptr && ws->WeaponType != nullptr &&
            ws->WeaponType->CanFire(selfPos, atkPos))
        {
            return true;
        }
    }
    return false;
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
void TechnoClass::PointerGotInvalid(AbstractClass* pInvalid)
{
    if (pInvalid == nullptr)
        return;
    if (Owner == pInvalid)
        Owner = nullptr;
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
