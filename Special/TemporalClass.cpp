#include "TemporalClass.h"

#include "../Abstract/TechnoClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Animations/AnimClass.h"
#include "../Animations/AnimTypeClass.h"
#include "../Audio/VoxClass.h"
#include "../Combat/WeaponTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/CellClass.h"
#include "../Map/MapClass.h"
#include "../Rules/RulesClass.h"
#include "../Game/Externs.h"
#include "../Game/GameInit.h"

#include <cmath>

// ============================================================================
// TemporalClass —— 超时空军团兵的"抹除"武器状态（详见头文件布局注释）
// ============================================================================

TemporalClass::TemporalClass() noexcept
    : AbstractClass()
    , Owner(nullptr)
    , Victim(nullptr)
    , CreationTimer(0)
    , field_38(0)
    , SourceSW(nullptr)
    , PrevTemporal(nullptr)
    , NextTemporal(nullptr)
    , RemainingWarp(0)
    , WeaponDamage(0)
{
}

// asm: TemporalClass_CTOR(TemporalClass *this, _TechnoClass *Owner)  retn 4
TemporalClass::TemporalClass(TechnoClass* pOwner) noexcept
    : AbstractClass()
    , Owner(pOwner)
    , Victim(nullptr)
    , CreationTimer(0)
    , field_38(0)
    , SourceSW(nullptr)
    , PrevTemporal(nullptr)
    , NextTemporal(nullptr)
    , RemainingWarp(0)
    , WeaponDamage(0)
{
}

// asm: TemporalClass_SDDTOR —— 只解绑，不触碰链上其它节点。
TemporalClass::~TemporalClass()
{
    Owner = nullptr;
    Victim = nullptr;
    NextTemporal = nullptr;
    PrevTemporal = nullptr;
    SourceSW = nullptr;
    field_38 = 0;
}

// ============================================================================
// asm: signed int __stdcall TemporalClass_GetClassID(int a1, CLSID *a2)  retn 8
// ============================================================================
HRESULT TemporalClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;

    pClassID->Data1 = static_cast<uint32>(AbstractType::Temporal);
    return S_OK;
}

// ============================================================================
// asm: void __stdcall TemporalClass_Load(TemporalClass *this, IStream *pStm)  retn 8
//   加载后重新贴回四段接口 vtable，再把六个对象指针过一遍 Swizzle。
//
//   注：asm 调用的 AbstractClass_Load 在本项目中是纯虚接口（实现落在
//   AbstractTypeClass），而 TemporalClass 并非类型类，故这里直接做它真正
//   要做的两件事：LoadTables 重贴接口虚表 + Swizzle 修正指针。
// ============================================================================
HRESULT TemporalClass::Load(IStream* pStm)
{
    LoadTables(pStm);

    // 原版对 Owner/Victim/PrevTemporal/NextTemporal/SourceSW/field_38 逐一
    // 调用 SwizzleManager::Swizzle 修正指针；单机构建无需地址重定位。
    return S_OK;
}

// ============================================================================
// asm: void __stdcall TemporalClass_Save(TemporalClass *this, IStream *pStm, int)  retn 0Ch
//   注：asm 调用 AbstractClass_Save（接口层纯虚），此处基类无状态可写，
//   字段由宿主 TechnoClass 的存档流程承载。
// ============================================================================
HRESULT TemporalClass::Save(IStream* /*pStm*/, BOOL /*fClearDirty*/)
{
    return S_OK;
}

// ============================================================================
// asm: void __thiscall TemporalClass_GetCRC(TemporalClass *this, byte *a2)  retn 4
// ============================================================================
void TemporalClass::ComputeCRC(CRCEngine& crc) const
{
    AbstractClass::ComputeCRC(crc);
}

// ============================================================================
// asm: void __thiscall TemporalClass_PointerGotInvalid(TemporalClass *this, void *ptr)  retn 4
//
//   由 TechnoClass::PointerGotInvalid 在 Owner/Victim 失效时直接调用（非虚）。
//     * ptr == Owner  ：若还挂着 Victim，先 LetGo()，再清 Victim 槽。
//     * ptr == Victim ：清 Victim 与整条链的外部引用，并请 Owner 回空闲。
// ============================================================================
void TemporalClass::PointerGotInvalid(AbstractClass* pInvalid, bool /*removed*/)
{
    if (pInvalid == Owner)
    {
        if (Victim != nullptr)
        {
            LetGo();
            Victim = nullptr;
        }
        return;
    }

    if (pInvalid == Victim)
    {
        Victim = nullptr;
        NextTemporal = nullptr;
        PrevTemporal = nullptr;
        SourceSW = nullptr;
        field_38 = 0;
        if (Owner != nullptr)
            Owner->Enter_Idle_Mode(0, 1);
    }
}

// ============================================================================
// asm: void __fastcall TemporalClass_LetGo(TemporalClass *this)
//
//   完整松脱，按本对象在链中的位置分四种情形（Prev=+0x40, Next=+0x44）：
//     * Prev==0 且 Next!=0 （链首，有后继）：Victim.TemporalTargettingMe 改指
//       Next；Next.PrevTemporal 清零、Next.RemainingWarp 继承本对象进度。
//     * Prev==0 且 Next==0 （唯一节点）：清 Victim 的锁定位与 IsBeingWarpedOut；
//       若 Victim 是建筑则 Enable_Temporal 并请其 Owner 复核可建造项；刷新
//       Victim 的层次；再清自身。
//     * Prev!=0 且 Next!=0 （链中）：把前后驱对接。
//     * Prev!=0 且 Next==0 （链尾）：Prev.NextTemporal 清空。
//   四种情形最终都清空 Victim/Next/Prev/SourceSW/field_38。
// ============================================================================
void TemporalClass::LetGo()
{
    if (PrevTemporal == nullptr)
    {
        // 本对象是链首。
        if (NextTemporal != nullptr)
        {
            if (Victim != nullptr)
                Victim->TemporalTargettingMe = NextTemporal;

            NextTemporal->PrevTemporal = nullptr;
            NextTemporal->RemainingWarp = RemainingWarp;
        }
        else
        {
            // 唯一节点：真正解除对 Victim 的时间化。
            if (Victim != nullptr)
            {
                Victim->TemporalTargettingMe = nullptr;
                Victim->IsBeingWarpedOutFlag = false;

                if (Victim->WhatAmI() == AbstractType::Building)
                {
                    BuildingClass* pBld = static_cast<BuildingClass*>(Victim);
                    pBld->EnableTemporal();
                }

                Victim->Mark_Layer(2);
            }
        }
    }
    else
    {
        if (NextTemporal == nullptr)
        {
            // 链尾：前驱的 Next 清空。
            PrevTemporal->NextTemporal = nullptr;
        }
        else
        {
            // 链中：前后驱对接。
            NextTemporal->PrevTemporal = PrevTemporal;
            PrevTemporal->NextTemporal = NextTemporal;
        }
    }

    Victim = nullptr;
    NextTemporal = nullptr;
    PrevTemporal = nullptr;
    SourceSW = nullptr;
    field_38 = 0;
}

// ============================================================================
// asm: void __fastcall TemporalClass_JustLetGo(TemporalClass *a1)
//
//   即刻松脱：清 Victim 的 IsBeingWarpedOut 与锁定位；对链上前后驱，若其
//   反指本对象则先清空该端，再各自 Detach()；最后清自身链槽与来源。
// ============================================================================
void TemporalClass::JustLetGo()
{
    if (Victim != nullptr)
    {
        Victim->IsBeingWarpedOutFlag = false;
        Victim->TemporalTargettingMe = nullptr;
        Victim = nullptr;
    }

    if (NextTemporal != nullptr)
    {
        if (NextTemporal->PrevTemporal == this)
            NextTemporal->PrevTemporal = nullptr;
        NextTemporal->Detach();
        NextTemporal = nullptr;
    }

    if (PrevTemporal != nullptr)
    {
        if (PrevTemporal->NextTemporal == this)
            PrevTemporal->NextTemporal = nullptr;
        PrevTemporal->Detach();
        PrevTemporal = nullptr;
    }

    NextTemporal = nullptr;
    PrevTemporal = nullptr;
    SourceSW = nullptr;
    field_38 = 0;
}

// ============================================================================
// asm: void __fastcall TemporalClass_Detach(TemporalClass *this)
//
//   从 Victim 与既有前后链上摘除自己（前后驱各自 Detach），再请 Owner 回空闲。
// ============================================================================
void TemporalClass::Detach()
{
    if (Victim != nullptr)
    {
        Victim->IsBeingWarpedOutFlag = false;
        Victim->TemporalTargettingMe = nullptr;
    }
    Victim = nullptr;

    if (NextTemporal != nullptr)
    {
        if (NextTemporal->PrevTemporal == this)
            NextTemporal->PrevTemporal = nullptr;
        NextTemporal->Detach();
    }

    if (PrevTemporal != nullptr)
    {
        if (PrevTemporal->NextTemporal == this)
            PrevTemporal->NextTemporal = nullptr;
        PrevTemporal->Detach();
    }

    NextTemporal = nullptr;
    PrevTemporal = nullptr;
    SourceSW = nullptr;
    field_38 = 0;

    if (Owner != nullptr)
        Owner->Enter_Idle_Mode(0, 1);
}

// ============================================================================
// asm: byte __thiscall TemporalClass_CanWarpTarget(TemporalClass *this, _TechnoClass *target)  retn 4
//
//   逐条否决：目标为空、类型不可传送、目标正在传送出场中、或目标是正停在
//   兵工厂格内的步兵，均拒绝。
// ============================================================================
bool TemporalClass::CanWarpTarget(TechnoClass* pTarget)
{
    if (pTarget == nullptr)
        return false;

    // 类型必须允许时间传送（Warpable）。
    TechnoTypeClass* pType = pTarget->GetTechnoType();
    if (pType == nullptr || !pType->Warpable)
        return false;

    // 目标正在传送出场中（asm [vt+0x160] = IsWarpingOut）：拒绝。
    if (pTarget->IsWarpingOut())
        return false;

    // 目标是步兵：若其所停靠的宿主建筑是兵工厂且同格，拒绝（避免抹掉
    // 正待出厂的载具）。asm 走 RadioClass_GetNthEntry 取宿主；OpenYR 的
    // TechnoClass 未继承 RadioClass，这里以 FocusOnUnit 作为宿主代理。
    if (pTarget->WhatAmI() == AbstractType::Infantry)
    {
        BuildingClass* pBld = pTarget->FocusOnUnit;
        if (pBld != nullptr && pBld->Type != nullptr &&
            pBld->Type->WeaponsFactory && TheMap != nullptr)
        {
            CellClass* pCell = TheMap->GetTargetCell(pTarget->GetCoords());
            if (pCell != nullptr)
            {
                for (ObjectClass* pOcc = pCell->First_Object();
                     pOcc != nullptr; pOcc = pOcc->NextObject)
                {
                    if (pOcc == pBld)
                        return false;
                }
            }
        }
    }

    return true;
}

// ============================================================================
// asm: int __thiscall TemporalClass_GetHelperDamagePerStep(TemporalClass *this, int a2)  retn 4
//
//   沿 NextTemporal 链递归累加每一步的武器伤害；a2 为层数（上限 50）。
//   本层把自身武器 Damage 写进 WeaponDamage(+0x4C)，返回本层伤害 + 后继链
//   的累加值。
// ============================================================================
int32 TemporalClass::GetHelperDamagePerStep(int32 a2)
{
    int32 helper = 0;

    if (NextTemporal != nullptr && a2 <= 50)
        helper = NextTemporal->GetHelperDamagePerStep(a2 + 1);

    int32 ownDamage = 0;
    if (Owner != nullptr && Owner->GetTechnoType() != nullptr)
    {
        const int32 slot = Owner->SelectWeapon(nullptr);
        WeaponStruct* pWeapon = Owner->GetTechnoType()->GetWeapon(slot);
        if (pWeapon != nullptr && pWeapon->WeaponType != nullptr)
            ownDamage = pWeapon->WeaponType->Damage;
    }

    WeaponDamage = ownDamage;
    return ownDamage + helper;
}

// ============================================================================
// asm: void __thiscall TemporalClass_Fire(TemporalClass *this, _TechnoClass *target)  retn 4
//
//   锁定目标并结算首帧副效果。
//
//   注：asm 开头会对目标调用 SpawnManagerClass_KillNodes 与
//   CaptureManagerClass_ReleaseAll（目标 +0x2D0 / +0x2BC）；OpenYR 的
//   TechnoClass 暂无这两个管理器字段，故此处省略（存疑项）。
// ============================================================================
void TemporalClass::Fire(TechnoClass* pTarget)
{
    // Owner 现有时间武器若已锁定目标，先松脱旧链。
    if (Owner != nullptr && Owner->TemporalImUsing != nullptr)
    {
        TemporalClass* pOld = Owner->TemporalImUsing;
        if (pOld->Victim != nullptr)
            pOld->LetGo();
    }

    if (!CanWarpTarget(pTarget))
        return;

    // Owner 正被别人时间化时，本次不锁定。
    if (Owner != nullptr && Owner->TemporalTargettingMe != nullptr)
        return;

    Victim = pTarget;

    if (pTarget->TemporalTargettingMe == nullptr)
    {
        // 全新绑定：以目标类型成本 * 10 作为抹除总量。
        pTarget->TemporalTargettingMe = this;

        TechnoTypeClass* pType = pTarget->GetTechnoType();
        if (pType != nullptr)
            RemainingWarp = pType->GetCost() * 10;

        // 收矿车且归属玩家：拉响 EVA 告警（原版另投递雷达事件）。
        if (pType != nullptr && pType->IsHarvester &&
            Owner != nullptr && Owner->Owner != nullptr &&
            Owner->Owner->IsHumanPlayer)
        {
            VoxClass::PlayFromName("EVA_OreMinerUnderAttack", -1, -1);
        }
    }
    else
    {
        // 目标已被别的时间武器锁定：把本对象插到那条链的前驱位置。
        TemporalClass* pExisting = pTarget->TemporalTargettingMe;
        PrevTemporal = pExisting;
        NextTemporal = pExisting->NextTemporal;
        pExisting->NextTemporal = this;
        if (NextTemporal != nullptr)
            NextTemporal->PrevTemporal = this;
    }

    // 标记 Victim 正被抹除。
    pTarget->IsBeingWarpedOutFlag = true;

    // 目标是加特林：按 asm sub_70E000(victim, 1) 降一档射速。
    //   原函数：WeaponStage -= RateDown * 1（<0 钳到 0），并清空若干武器
    //   缓存串/动画槽（+0x4A4/+0x4B8/+0x4C0/+0x4D4）。OpenYR 的 TechnoClass
    //   无这些串/动画槽字段，这里只复现射速档位这一实质行为（存疑项）。
    TechnoTypeClass* pVictimType = pTarget->GetTechnoType();
    if (pVictimType != nullptr && pVictimType->IsGattling)
    {
        pTarget->WeaponStage -= pVictimType->RateDown;
        if (pTarget->WeaponStage < 0)
            pTarget->WeaponStage = 0;
    }

    // 目标是建筑：时间化建筑令其停工。
    if (pTarget->WhatAmI() == AbstractType::Building)
    {
        BuildingClass* pBld = static_cast<BuildingClass*>(pTarget);
        pBld->DisableTemporal();
    }

    // 刷新目标层次。
    pTarget->Mark_Layer(2);

    // 目标自身的旧时间武器让位。
    if (pTarget->TemporalImUsing != nullptr &&
        pTarget->TemporalImUsing->Victim != nullptr)
    {
        pTarget->TemporalImUsing->LetGo();
    }
}

// ============================================================================
// asm: void __thiscall TemporalClass_Update(TemporalClass *this)
//
//   逐帧推进。三条退出路径：
//     ① Victim 仍指向本对象且本对象有前驱 → 目标脱离锁定：清锁定位与
//        IsBeingWarpedOut，Detach 自身。
//     ② Owner 仍在场且目标超出射程阈值 → LetGo()。
//     ③ 否则扣减 RemainingWarp，归零时抹除目标。
// ============================================================================
void TemporalClass::Update()
{
    // ① 目标脱离锁定。
    if (Victim != nullptr && Victim->TemporalTargettingMe == this &&
        PrevTemporal != nullptr)
    {
        Victim->TemporalTargettingMe = nullptr;
        Victim->IsBeingWarpedOutFlag = false;
        Detach();
        return;
    }

    // ② 超距：仅当 Owner 正乘坐在敞篷运输载具内（asm [Owner+82h] 门控，
    //    与 SelectWeapon 处的 OpenTransportWeapon 判定同源）才做距离检查；
    //    3D 距离 > OpenToppedWarpDistance << 8 则松脱。
    //    注：ObjectClass 布局注释标 InOpenTopped 为 +0x81，asm 读 +0x82，
    //    存在一字节歧义；此处按语义取 InOpenTopped（存疑项）。
    if (Owner != nullptr && Owner->InOpenTopped && Victim != nullptr)
    {
        const CoordStruct ownerCrd  = Owner->GetCoords();
        const CoordStruct victimCrd = Victim->GetCoords();
        const double dx = static_cast<double>(victimCrd.X - ownerCrd.X);
        const double dy = static_cast<double>(victimCrd.Y - ownerCrd.Y);
        const double dz = static_cast<double>(victimCrd.Z - ownerCrd.Z);
        const int32 dist = static_cast<int32>(std::sqrt(dx * dx + dy * dy + dz * dz));
        const int32 maxDist = (RulesClass::Instance != nullptr)
                            ? (RulesClass::Instance->OpenToppedWarpDistance << 8)
                            : 0;
        if (dist > maxDist)
        {
            LetGo();
            return;
        }
    }

    // ③ 扣减抹除进度。
    int32 helper = 0;
    if (NextTemporal != nullptr)
        helper = GetHelperDamagePerStep(1);

    WeaponDamage = 0;
    if (Owner != nullptr && Owner->GetTechnoType() != nullptr)
    {
        const int32 slot = Owner->SelectWeapon(nullptr);
        WeaponStruct* pWeapon = Owner->GetTechnoType()->GetWeapon(slot);
        if (pWeapon != nullptr && pWeapon->WeaponType != nullptr)
            WeaponDamage = pWeapon->WeaponType->Damage;
    }

    RemainingWarp -= (WeaponDamage + helper);
    if (RemainingWarp > 0)
        return;

    // ── 抹除结算 ──────────────────────────────────────────────────────────
    if (Victim == nullptr)
    {
        NextTemporal = nullptr;
        PrevTemporal = nullptr;
        SourceSW = nullptr;
        field_38 = 0;
        if (Owner != nullptr)
            Owner->Enter_Idle_Mode(0, 1);
        return;
    }

    TechnoClass* pVictim = Victim;

    // 播放目标的 WarpAway 消散动画。
    if (RulesClass::Instance != nullptr &&
        RulesClass::Instance->WarpAway != nullptr)
    {
        const CoordStruct crd = pVictim->GetCoords();
        new AnimClass(RulesClass::Instance->WarpAway, crd, 0, 1, 0x600, 0, false);
    }

    // 老兵经验：按目标价值与自身价值比例记功。
    if (Owner != nullptr && Owner->GetTechnoType() != nullptr &&
        Owner->GetTechnoType()->IsTrainable())
    {
        TechnoTypeClass* pVictimType = pVictim->GetTechnoType();
        if (pVictimType != nullptr)
        {
            const int32 victimCost = pVictimType->GetActualCost(pVictim->Owner);
            const int32 ownerCost  = Owner->GetTechnoType()->GetActualCost(Owner->Owner);
            Owner->AddVeterancyValue(ownerCost, victimCost);
        }
    }

    if (pVictim->WhatAmI() == AbstractType::Building)
    {
        BuildingClass* pBld = static_cast<BuildingClass*>(pVictim);

        // 处死驻军；清空掩体（掩体内载员各自卸出）。
        if (pBld->GetOccupantCount() > 0)
            pBld->KillOccupiers(nullptr);
        if (!ScenarioInit)
            pBld->EmptyBunker();
        pBld->UnloadBunker();

        // 建筑：DestroyedBy / RegisterDestruction / UnInit。
        pBld->Destroyed(Owner);
        pBld->ObjectClass::RegisterDestruction(Owner);
        pBld->UnInit();
    }
    else
    {
        // 非建筑：若自身作为掩体登记着"窝在里面的单位"，先清掩体。
        //   asm: if ([Victim+0x2E4] = BunkerLinkedItem) BuildingClass_ClearBunker。
        if (pVictim->BunkerLinkedItem != nullptr)
        {
            BuildingClass* pBunker = static_cast<BuildingClass*>(pVictim->BunkerLinkedItem);
            pBunker->ClearBunker();
        }

        // DestroyedBy / RegisterDestruction / UnInit。
        pVictim->Destroyed(Owner);
        pVictim->ObjectClass::RegisterDestruction(Owner);
        pVictim->UnInit();
    }

    if (SourceSW != nullptr)
        SourceSW = nullptr;

    // 两处 Enter_Idle_Mode：分支尾部一次，清链后又一次。
    if (Owner != nullptr)
    {
        Owner->Enter_Idle_Mode(0, 1);

        Victim = nullptr;
        NextTemporal = nullptr;
        PrevTemporal = nullptr;
        SourceSW = nullptr;
        field_38 = 0;

        Owner->Enter_Idle_Mode(0, 1);
        return;
    }

    Victim = nullptr;
    NextTemporal = nullptr;
    PrevTemporal = nullptr;
    SourceSW = nullptr;
    field_38 = 0;
}
