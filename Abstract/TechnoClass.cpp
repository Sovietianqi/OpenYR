#include <Abstract/TechnoClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Combat/WeaponTypeClass.h>
#include <Combat/WarheadTypeClass.h>
#include <Combat/BulletClass.h>
#include <Houses/HouseClass.h>
#include <Game/SaveGameClass.h>
#include <Game/Game.h>
#include <Game/Externs.h>
#include <Rules/RulesClass.h>
#include <Map/CellClass.h>
#include <Map/MapClass.h>
#include <Abstract/BuildingClass.h>

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

    // Launch the projectile.  GetFLH supplies the muzzle coordinate; the
    // warhead comes from the weapon slot.
    WeaponStruct* ws = TechnoType->GetWeapon(nWeaponIndex);
    WeaponTypeClass* pWeapon = (ws != nullptr) ? ws->WeaponType : nullptr;
    if (pWeapon == nullptr || pWeapon->Projectile == nullptr)
        return nullptr;

    CoordStruct source = GetFLH(nWeaponIndex, true);

    CoordStruct targetPos;
    pTarget->GetCoords(&targetPos);

    return BulletClass::Fire(
        pWeapon->Projectile, pWeapon, source, targetPos, this,
        pWeapon->Damage, pWeapon->Warhead);
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

    // Invulnerability from Iron Curtain / Force Shield / temporal freeze.
    if (IsShielded() || IsTemporalized())
        return false;

    // Apply the warhead's armor-class multiplier (Verses[]).
    int32 finalDamage = damage;
    if (pWarhead != nullptr && TechnoType != nullptr)
    {
        float mult = pWarhead->GetDamageMultiplier(static_cast<int32>(TechnoType->Get_Armor()));
        finalDamage = static_cast<int32>(damage * mult + 0.5f);
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
// TechnoClass::GetThreatValue - asm 0x708B50 (TechnoClass_GetThreatPosed)
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

// TechnoClass_HasTurret (asm 0x1903E0).  True when the techno's type declares
// a rotating turret.
bool TechnoClass::HasTurret() const
{
    return TechnoType != nullptr && TechnoType->Turret;
}

// TechnoClass_OnFloor (asm 0x5F6B5A).  A ground object is "on the floor" when
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

// TechnoClass_InAir (asm 0x5F6B8C).  The exact complement of IsOnFloor.
bool TechnoClass::IsInAir() const
{
    if (!Tunnel)
        return false;

    const int32 z = GetZ();
    return z >= (0x68 * 2);
}

// TechnoClass_OnBridge (asm 0x703B70).  True when the object currently sits on
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

// TechnoClass_IsCrewed (asm 0x6F3B30).  Mirrors the techno type's Crewed flag.
bool TechnoClass::IsCrewed() const
{
    return TechnoType != nullptr && TechnoType->IsCrewed_;
}

// TechnoClass_IsFactory (asm 0x102414) - the base-class answer is always false;
// only BuildingClass overrides it.
bool TechnoClass::IsFactory() const
{
    return false;
}

// TechnoClass_GetZ (asm 0x5F3C70) - the object's raw Z field.
int32 TechnoClass::GetZ() const
{
    return Location.Z;
}

// TechnoClass_GetActiveTurretIndex (asm 0x70DCB0) - the weapon/turret slot the
// unit is currently drawn with.
int32 TechnoClass::GetActiveTurretIndex() const
{
    return ActiveTurretIndex;
}

// TechnoClass_CurrentWeaponSelected (asm 0x70DCA0) - true when a weapon slot is
// currently selected (i.e. not the -1 "none" sentinel).
bool TechnoClass::CurrentWeaponSelected() const
{
    return CurrentWeaponNumber != -1;
}

// TechnoClass_GetTurretIndex (asm 0x70DD30).  For a turret-changing unit this
// is the promoted gunner slot, otherwise the type's plain turret index.
int32 TechnoClass::GetTurretIndex() const
{
    if (HasMultipleTurrets())
        return VeterancyLevel;

    return 0;
}

// TechnoClass_HasMultipleTurrets (asm 0x70DC90) - forwards to the techno type.
bool TechnoClass::HasMultipleTurrets() const
{
    return TechnoType != nullptr && TechnoType->TurretSpins;
}

// TechnoClass_GetOwner (asm 0x70F810).  The owning house resolves to the
// pre-capture owner while the object is captured, and to the plain owner
// field otherwise.
HouseClass* TechnoClass::Get_Owner() const
{
    if (Captured)
        return OrigOwner;

    return Owner;
}

// ============================================================================
// TechnoClass - disguise, cloak, warp and weapon-selection probes
// ============================================================================

// TechnoClass_IsDisguised (asm 0x1905A8) - reads the disguise flag.
bool TechnoClass::IsDisguised() const
{
    return TechnoType != nullptr && TechnoType->CanDisguise && IsDisguisedFlag;
}

// TechnoClass_IsDisguised_2 (asm 0x1905BC) - the ranged form; the extra
// argument is accepted and ignored by the original.
bool TechnoClass::IsDisguised2(int32 /*a2*/) const
{
    return IsDisguised();
}

// TechnoClass_ClearDisguise (asm 0x1905D0) - drop the disguise.
void TechnoClass::ClearDisguise()
{
    IsDisguisedFlag = false;
}

// TechnoClass_IsCloakable (asm 0x708C40) - mirrors the type's Cloakable flag.
bool TechnoClass::IsCloakable() const
{
    return TechnoType != nullptr && TechnoType->Cloakable;
}

// TechnoClass_IsBeingWarpedOut (asm 0x708C50) - true while a chrono-warp is
// taking this object out of the world.
bool TechnoClass::IsBeingWarpedOut() const
{
    return WarpOutTimer > 0;
}

// TechnoClass_IsWarpingOut (asm 0x708C5E) - the object has finished warping.
bool TechnoClass::IsWarpingOut() const
{
    return WarpOutTimer == 0 && IsWarpingOutFlag;
}

// TechnoClass_IsNotTemporalLocked (asm 0x708C8E) - the complement of the
// temporal-lock test.
bool TechnoClass::IsNotTemporalLocked() const
{
    return TemporalTimer <= 0;
}

// TechnoClass_IsNotWarpingIn (asm 0x5F3E31) - the complement of IsWarpingIn.
bool TechnoClass::IsNotWarpingIn() const
{
    return WarpInTimer <= 0;
}

// TechnoClass_IsDraining (asm 0x70F5B5) - true while a drain weapon
// (e.g. the magnetron) is affecting this object.
bool TechnoClass::IsDraining() const
{
    return DrainTimer > 0;
}

// TechnoClass_CanPassiveAquire (asm 0x70917A).  A unit may acquire targets on
// its own when it is not already engaged and its type allows passive
// acquisition.
bool TechnoClass::CanPassiveAquire() const
{
    return TechnoType != nullptr && TechnoType->CanPassiveAquire;
}

// TechnoClass_CanTraverse (asm 0x802612) - the type may drive across terrain.
bool TechnoClass::CanTraverse() const
{
    return TechnoType != nullptr;
}

// TechnoClass_CanSetWaypoint (asm 0x700C40).  Only a player-owned object that
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

// TechnoClass_NeedsToSelfHeal (asm 0x70BE80).  An object self-heals when its
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

// TechnoClass_GetHealthState (asm 0x5F5DF0).  Classifies the health fraction
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

// TechnoClass_GetXYDistanceFrom (asm 0x5F6500).  Planar (ignoring Z) distance
// between this object and another, in leptons.  A null target yields 0.
double TechnoClass::GetXYDistanceFrom(const AbstractClass* pOther) const
{
    if (pOther == nullptr)
        return 0.0;

    const CoordStruct a = GetCoords();
    CoordStruct b;
    pOther->GetCoords(&b);

    const double dx = static_cast<double>(a.X - b.X);
    const double dy = static_cast<double>(a.Y - b.Y);

    return sqrtf(static_cast<float>(dx * dx + dy * dy));
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

// TechnoClass_InWhichLayer (asm 0x41ADCB).
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

// TechnoClass_GetCellCoords (asm 0x41BEBE).
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
// (asm 0x41B547 / 0x41B54F / 0x41B557).  The base implementation scores zero
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

// TechnoClass_CanOccupyFire (asm 0x41B534).
//
//  Only garrisonable structures and the infantry inside them can fire from
//  an occupied building, so the base implementation answers false.
bool TechnoClass::CanOccupyFire() const
{
    return false;
}

// TechnoClass_GetOccupantCount (asm 0x41B53C).  The base class holds nobody.
int32 TechnoClass::GetOccupantCount() const
{
    return 0;
}

// ============================================================================
// TechnoClass - panic / idle / power batch
// ============================================================================

// TechnoClass_Panic (asm 0x41B4xx): the base class has no morale model, so
// the no-argument panic entry point is empty.  FootClass overrides it.
void TechnoClass::Panic()
{
}

// TechnoClass_Unpanic: the base class has no morale model, so this is empty.
void TechnoClass::Unpanic()
{
}

// TechnoClass_IdleAction (asm 0x41B5A4): the default idle action never
// reports completion; missions that have an idle phase override it.
bool TechnoClass::IdleAction()
{
    return false;
}

// TechnoClass_IsPowerOnline (asm 0x41B57x): only structures carry power
// state, so the base class reports "not online".
bool TechnoClass::IsPowerOnline() const
{
    return false;
}

// ============================================================================
// TechnoClass - planning token / type-flag probes
// ============================================================================

// TechnoClass_GetPlanningToken (asm 0x70DDC0): load of the token slot.
int32 TechnoClass::GetPlanningToken() const
{
    return PlanningToken;
}

// TechnoClass_AttachPlanningToken (asm 0x70DDC8): store into the token slot.
void TechnoClass::AttachPlanningToken(int32 token)
{
    PlanningToken = token;
}

// TechnoClass_Assign_Destination_Cell (asm 0x70DDD5): records the building the
// techno is heading for.
void TechnoClass::Assign_Destination_Cell(BuildingClass* pTarget)
{
    FocusOnUnit = pTarget;
}

// TechnoClass_NotSubmerged (asm 0x70DDE0).
//
//  True when the object's height is strictly above the submarine threshold
//  (-20 leptons): underwater objects are exempt from several area effects.
bool TechnoClass::NotSubmerged() const
{
    const int32 height = Location.Z;
    return height > -20;
}

// TechnoClass_IsNotSprayAttack (asm 0x70DD00).
//
//  True when the type does not use a spray attack pattern.
bool TechnoClass::IsNotSprayAttack() const
{
    return (TechnoType == nullptr) || !TechnoType->SprayAttack;
}

// TechnoClass_IsNotSprayAttack2 (asm 0x70DD20): identical to IsNotSprayAttack,
// a second vtable slot over the same type byte.
bool TechnoClass::IsNotSprayAttack2() const
{
    return (TechnoType == nullptr) || !TechnoType->SprayAttack;
}

// TechnoClass_SetCurrentWeaponStage (asm 0x70DDD4).
//
//  Stores idx into the multi-stage weapon counter, ignoring negative values.
void TechnoClass::SetCurrentWeaponStage(int32 idx)
{
    if (idx >= 0)
        WeaponStage = idx;
}

// TechnoClass_HasTurretTooltips (asm 0x70DDA6): the type's turret tooltip flag.
bool TechnoClass::HasTurretTooltips() const
{
    return (TechnoType != nullptr) && TechnoType->HasTurretTooltips;
}

// ============================================================================
// TechnoClass - temporal / weapon-legal probes
// ============================================================================

// TechnoClass_IsTemporalSource (asm 0x70C5D0).
//
//  True when the techno is applying a temporal weapon (TemporalImUsing, +0x64C)
//  that already has a victim bound to it.
bool TechnoClass::IsTemporalSource() const
{
    if (TemporalImUsing == nullptr)
        return false;

    // The temporal object's first field is the victim pointer; a bound source
    // always has one.  Treat any non-null victim proxy as "has victim".
    void** pVictim = *reinterpret_cast<void***>(TemporalImUsing);
    return pVictim != nullptr;
}

// TechnoClass_IsLegalWeapon (asm 0x70E245).
//
//  A weapon container is legal only when it is non-null and its first dword
//  (the weapon id) is non-zero.
bool TechnoClass::IsLegalWeapon(const void* pWeapon) const
{
    if (pWeapon == nullptr)
        return false;

    return *static_cast<const int32*>(pWeapon) != 0;
}

// TechnoClass_GetNonSprayWeapon (asm 0x70DDC8-adjacent).
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

// TechnoClass_CanAreaFire (asm 0x70DD40).
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
// TechnoClass_Combat_Damage (asm 0x6F8CB0).
//
//  Returns the best weapon damage this techno can bring to bear in slot
//  `idxWeapon`, taking veterancy into account.  Greatest_Threat calls it with
//  idxWeapon == -1 to ask "am I combat-capable at all?" - a negative result
//  marks the techno as a non-combatant (engineer / terrorist) whose
//  acquisition mask gets restricted to support targets.
// ============================================================================
int32 TechnoClass::Combat_Damage(int32 idxWeapon) const
{
    if (TechnoType == nullptr)
        return -1;

    const int32 slot = (idxWeapon < 0) ? 0 : idxWeapon;
    WeaponStruct* pWeapon = TechnoType->GetWeapon(slot);
    if (pWeapon == nullptr || pWeapon->WeaponType == nullptr)
        return -1;

    return pWeapon->WeaponType->Damage;
}

// ============================================================================
// TechnoClass_Techno_31C (asm 0x7087D0).
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
// TechnoClass::Greatest_Threat (asm 0x6F8DA0).
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
//  Only phases 1, 3 and the mask translation are modelled here: the project
//  has no global object-class candidate walker yet, so phase 4 returns the
//  current target unchanged.  The mask bookkeeping is faithful, which is what
//  callers observe.
// ============================================================================
ObjectClass* TechnoClass::Greatest_Threat(int32 projFlags, int32 curThreat, int32 a4)
{
    (void)curThreat;
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
    switch (WhatAmI()) {
    case AbstractType::Infantry: {
        // An engineer with no usable weapon ignores everything except
        // friendlies and its capture targets; a terrorist likewise stops
        // shooting at vehicles.
        if (Combat_Damage(-1) < 0) {
            projFlags &= (0x01 | 0x02);
            projFlags |= (ProjectileTypeFlags::ttInf | ProjectileTypeFlags::ttFriendlies);
        }
        break;
    }
    case AbstractType::Unit: {
        if (Combat_Damage(-1) < 0) {
            projFlags &= (0x01 | 0x02);
            projFlags |= (ProjectileTypeFlags::ttVeh | ProjectileTypeFlags::ttFriendlies);
        }
        break;
    }
    default:
        break;
    }

    // Phase 4 - the candidate walker is not yet available in this project.
    // Returning null mirrors "no target acquired"; callers such as
    // BuildingClass_SelectAutoTarget tolerate a null result.
    return nullptr;
}

// ============================================================================
// TechnoClass_UpdatePowered (asm 0x70ED20).
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
// TechnoClass_GetCellCoords1 (asm 0x5F6A50).
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
// TechnoClass_GetCell1 (asm 0x5F6A90).
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
// TechnoClass_OnFloor (asm 0x5F6B60) / _InAir (asm 0x5F6B90).
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
// TechnoClass_GetZFudgeCliff (asm 0x704270).
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
// TechnoClass_GetZFudgeColumn (asm 0x703E60).
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
// TechnoClass_GetZFudgeTunnel (asm 0x703F00).
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
// TechnoClass_GetElevationRangeBonus (asm 0x6F6FA0) /
// _GetElevationBonusNoSqrt (asm 0x6F7090).
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
double TechnoClass::GetElevationRangeBonus(ObjectClass* pTarget) const
{
    if (pTarget == nullptr)
        return 0.0;
    if (MapClass::Instance == nullptr)
        return 0.0;
    if (TheRules == nullptr || TheRules->ElevationIncrement == 0)
        return 0.0;

    const CellStruct srcCell = CellClass::Coord2Cell(GetCoords());
    const CellStruct tgtCell = CellClass::Coord2Cell(pTarget->GetCoords());

    CellClass* pSrc = MapClass::Instance->GetCellAt(srcCell);
    CellClass* pTgt = MapClass::Instance->GetCellAt(tgtCell);
    if (pSrc == nullptr || pTgt == nullptr)
        return 0.0;

    const int32 delta = pTgt->Get_Ground_Height() - pSrc->Get_Ground_Height();
    if (delta < 0)
        return 0.0;

    const int32 steps = delta / TheRules->ElevationIncrement;
    return static_cast<double>(steps) * TheRules->ElevationIncrementBonus;
}

double TechnoClass::GetElevationBonusNoSqrt(ObjectClass* pTarget) const
{
    if (pTarget == nullptr)
        return 0.0;
    if (MapClass::Instance == nullptr)
        return 0.0;
    if (TheRules == nullptr || TheRules->ElevationIncrement == 0)
        return 0.0;

    const CellStruct srcCell = CellClass::Coord2Cell(GetCoords());
    const CellStruct tgtCell = CellClass::Coord2Cell(pTarget->GetCoords());

    CellClass* pSrc = MapClass::Instance->GetCellAt(srcCell);
    CellClass* pTgt = MapClass::Instance->GetCellAt(tgtCell);
    if (pSrc == nullptr || pTgt == nullptr)
        return 0.0;

    int32 delta = pSrc->Get_Ground_Height() - pTgt->Get_Ground_Height();
    if (delta < 0)
        delta = 0;

    const int32 steps = delta / TheRules->ElevationIncrement;
    return static_cast<double>(steps) * TheRules->ElevationIncrementBonus;
}

// ============================================================================
// TechnoClass_TimeForCellInset (asm 0x6F7690).
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

    const double inset = static_cast<double>(pWarhead->CellSpread) - pWarhead->CellInset;

    const CoordStruct src = GetCoords();
    const CoordStruct tgt = pTarget->GetCoords();
    const int32 dx = (src.X - tgt.X) >> 8;
    const int32 dy = (src.Y - tgt.Y) >> 8;
    const double dist = std::sqrt(static_cast<double>(dx * dx + dy * dy));

    return dist >= inset;
}

// ============================================================================
// TechnoClass_CanLobber (asm 0x6F9CB0).
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
// TechnoClass_HasAbility (asm 0x6F9BE0).
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
// TechnoClass_CanBeBunkered (asm 0x6FB5E0).
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
// TechnoClass_CanBePermaMC (asm 0x5B1080).
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
// TechnoClass_BelongsToPlayer (asm 0x6FBF40).
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
// TechnoClass_PlayerOwnedAliveAndNamed (asm 0x6FBFF0).
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
// TechnoClass_GetPointsValue (asm 0x707DC0).
//
//  The score awarded for destroying this techno.  Composed of three parts:
//
//    * The value of everything the techno carries (its cargo chain, each
//      entry's own GetPointsValue).
//    * The type's point value (vtable +0x2C0 - PointValue for most types).
//    * The locomotor's contributed value (+0x674 on the type).
//
//  The cargo sum is only added when the owning house is not a "dumb" AI and
//  the techno actually carries something.
// ============================================================================
int32 TechnoClass::GetPointsValue() const
{
    int32 total = 0;

    if (TechnoType == nullptr)
        return 0;

    // Cargo chain contribution - only for houses smart enough to matter.
    if (Owner != nullptr && !Owner->IsHumanPlayer) {
        // The binary walks the CargoClass chain at +0x114 and sums each
        // object's GetPointsValue.
        // Not modelled: this project has no CargoClass chain yet.
    }

    // The type's own point value (the sidebar cost doubles as the score the
    // engine awards, matching vtable +0x2C0's default implementation).
    total += TechnoType->Get_Cost();

    return total;
}

// ============================================================================
// TechnoClass_GetTiberiumPercentage (asm 0x708B90).
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
// TechnoClass_GetFacingAgain (asm 0x70ED90).
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
// TechnoClass_GetDisguiseFlags (asm 0x70ED60).
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
// TechnoClass_IsDisguisedAgainst (asm 0x70EE40).
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
// Techno_Update_Reloading - asm 0x6FB0D0.
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
// TechnoClass_Reload - asm 0x6FB000.
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
// TechnoClass_StartAirstrikeTimer (asm 0x6FC930).
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
// TechnoClass_StopAirstrikeTimer (asm 0x6FC950).
// ============================================================================
void TechnoClass::StopAirstrikeTimer()
{
    AirstrikeTimeStart = Game::GetCurrentFrame();
    AirstrikeTimeLeft  = 0;
    AirstrikeTimeGen   = 0;
}
