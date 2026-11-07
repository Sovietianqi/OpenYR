// =============================================================================
// BombClass.cpp - 定时炸弹 / 死亡炸弹
//
// 权威来源：/workspace/asm_export/BombClass.asm.txt
//   BombClass_SCTOR           (asm:21)   —— 初始化 vec_BombList 注册表
//   BombClass_Detonate        (asm:88)   —— 引爆：范围伤害 + C4 动画 + 桥面扫描
//   BombClass_Disarm          (asm:328)  —— 拆除：解挂载 + 清引用
//   BombClass_SagedMyself     (asm:378)  —— 返回 IsDeathBomb
//   BombClass_GetCurrentFrame (asm:405)  —— 图标闪烁帧 0..11
//   BombClass_TimeToBlow      (asm:477)  —— 是否到点
//   BombClass_GetCRC          (asm:516)
//   BombClass_GetClassID      (asm:590)
//   BombClass_Load            (asm:653)
//   BombClass_GetSize         (asm:679)  —— 0x5C
//   BombClass_SDDTOR          (asm:728)  —— 析构时从 vec_Bombs 摘除
// =============================================================================

#include "BombClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/ObjectClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Animations/AnimClass.h"
#include "../Combat/DamageArea.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Game/Game.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Rules/RulesClass.h"

#include <cstring>

// asm: vec_BombList.bombs —— 全局在册炸弹表
DynamicVectorClass<BombClass*>* BombClass::BombList = nullptr;
// asm: vec_BombList.detectors —— 观察者表
DynamicVectorClass<TechnoClass*>* BombClass::Detectors = nullptr;
// asm: vec_BombList.arg —— 写死的 0x64
int32 BombClass::BombListArg = 0;

// =============================================================================
// asm: BombClass_SCTOR (asm:21)
//   bombs.data=0 count=0 IsInitialized=1 IsAllocated=0 incrementAmount=0x0A
//   detectors 同形；arg=0x64；注册 atexit(BombListClass_Clear)。
// =============================================================================
void BombClass::sctor()
{
    if (BombList == nullptr) {
        BombList = new DynamicVectorClass<BombClass*>();
    }
    if (Detectors == nullptr) {
        Detectors = new DynamicVectorClass<TechnoClass*>();
    }
    BombListArg = 0x64;   // asm: mov vec_BombList.arg, 64h
}

BombClass::BombClass() noexcept
    : SourceUnit(nullptr)
    , OwnerHouse(nullptr)
    , TargetUnit(nullptr)
    , State(0)
    , PlantingFrame(0)
    , WhenToBlow(0)
    , field_54(false)
    , IsDeathBomb(false)
{
    AttachedName[0] = '\0';
    Add_To_BombList();
}

// asm: BombClass_SDDTOR (asm:728) —— 释放 strX 后从 vec_Bombs 摘除自身。
BombClass::~BombClass()
{
    Remove_From_BombList();
}

// asm: vec_BombList.bombs 的追加
void BombClass::Add_To_BombList()
{
    sctor();
    if (BombList != nullptr) {
        BombList->Add(this);
    }
}

// asm: vec_BombList.bombs 的摘除
void BombClass::Remove_From_BombList()
{
    if (BombList == nullptr) {
        return;
    }
    for (int32 i = BombList->Count - 1; i >= 0; --i) {
        if ((*BombList)[i] == this) {
            BombList->Remove(i);
            break;
        }
    }
}

// =============================================================================
// asm: char __fastcall BombClass_SagedMyself(BombClass *a1) (asm:378)
//   mov eax,[ecx+IsDeathBomb] / retn
// =============================================================================
bool BombClass::SagedMyself() const
{
    return IsDeathBomb;
}

// =============================================================================
// asm: char __thiscall BombClass_TimeToBlow(BombClass *this) (asm:477)
//
//   if (State != 0)               return false;   // 已处置
//   if (Frame <= WhenToBlow)      return false;   // 未到点
//   if (IsDeathBomb)              return false;   // 已解除
//   return true;
// =============================================================================
bool BombClass::TimeToBlow() const
{
    if (State != 0) {
        return false;
    }
    if (Game::CurrentFrame <= WhenToBlow) {
        return false;
    }
    if (IsDeathBomb) {
        return false;
    }
    return true;
}

// =============================================================================
// asm: TechnoClass_Update+8B6 的逐帧倒计时判定。
//   挂载炸弹每帧查一次 TimeToBlow，到点即引爆。
// =============================================================================
void BombClass::Update()
{
    if (State != 0 || IsDeathBomb) {
        return;
    }
    if (TimeToBlow()) {
        Detonate();
    }
}

// =============================================================================
// 全局炸弹扫描：等价原版每帧遍历 vec_BombList.bombs 逐个倒计时。
// =============================================================================
void BombClass::AI()
{
    if (BombList == nullptr) {
        return;
    }
    // 倒序以便 Detonate 期间可能发生的摘除不破坏索引。
    for (int32 i = BombList->Count - 1; i >= 0; --i) {
        BombClass* pBomb = (*BombList)[i];
        if (pBomb != nullptr) {
            pBomb->Update();
        }
    }
}

// =============================================================================
// asm: signed int __thiscall BombClass_GetCurrentFrame(BombClass *this) (asm:405)
//
//   State==1 -> 12（拆除动画帧）
//   否则按 IvanTimedDelay / 6 作为节拍，IvanIconFlickerRate 作为闪烁半周期，
//   算出拖尾循环帧 0..11。
// =============================================================================
int32 BombClass::GetCurrentFrame() const
{
    if (State == 1) {
        return 12;
    }

    RulesClass* pRules = RulesClass::Instance;
    int32 flicker = (pRules != nullptr) ? pRules->IvanIconFlickerRate : 0;
    int32 delay = (pRules != nullptr) ? pRules->IvanTimedDelay : 0;
    if (flicker <= 0) {
        flicker = 1;
    }
    int32 tick = delay / 6;
    if (tick <= 0) {
        tick = 1;
    }

    int32 frame = (Game::CurrentFrame - PlantingFrame) / tick;
    int32 doubled = frame * 2;
    int32 period = flicker * 2;
    if ((Game::CurrentFrame % period) >= flicker) {
        ++doubled;
    }
    if (doubled >= 12) {
        doubled = 11;
    }
    return doubled;
}

// =============================================================================
// asm: void __thiscall BombClass_Disarm(BombClass *this) (asm:328)
//
//   if (TargetUnit) { TargetUnit->+0x38 = 0; TargetUnit->+0x68 = 0; }
//   IsDeathBomb = 1; SourceUnit = TargetUnit = OwnerHouse = 0;
//   strX 析构；field_54 = 0。
// =============================================================================
void BombClass::Disarm()
{
    if (TargetUnit != nullptr) {
        TargetUnit->Bomb = nullptr;        // +0x38：摘掉挂载指针
        TargetUnit->IsParasited = false;   // +0x68：解除布雷标记
    }

    IsDeathBomb = true;
    SourceUnit = nullptr;
    TargetUnit = nullptr;
    OwnerHouse = nullptr;
    AttachedName[0] = '\0';
    field_54 = false;
}

// =============================================================================
// asm: void __thiscall BombClass_Detonate(BombClass *this) (asm:88)
//
//   入口守卫：TargetUnit == null 或 IsDeathBomb -> 直接返回（loc_43899D）。
//   1. already = TargetUnit->+0x81（IsDyingNow）；清 TargetUnit->+0x38 挂载。
//   2. IsDeathBomb = 1（两条路径都置位，asm: mostlyHarmless1=1）。
//      · 若 already -> 只清目标 +0x68 并跳到收尾。
//   3. 否则：取 TargetUnit+0x9C 坐标；用 IvanWarhead / IvanDamage 施加范围伤害。
//   4. 生成爆心动画（SelectDamageAnimation 选类型）。
//   5. TargetUnit 为建筑且 type->BridgeRepairHut 时：扫描周边 -2..2 格，
//      命中木桥砖块/桥梁 overlay 则触发桥面损毁。
//   6. 收尾：清 SourceUnit / TargetUnit / OwnerHouse，复位 strX 与 field_54。
// =============================================================================
void BombClass::Detonate()
{
    if (TargetUnit == nullptr || IsDeathBomb) {
        // asm loc_43899D：入口守卫，直接返回。
        return;
    }

    // asm: cl = [TargetUnit+81h]（目标是否已进入移除流程）
    const bool alreadyRemoved = TargetUnit->IsDyingNow;
    TargetUnit->Bomb = nullptr;   // asm: [eax+38h]=0

    if (alreadyRemoved) {
        // asm loc_43875E 之前的分支：目标已爆，仅置位。
        TargetUnit->IsParasited = false;   // asm: [eax+68h]=0
        IsDeathBomb = true;                // asm: [esi+mostlyHarmless1]=1
    } else {
        // asm loc_43875E：目标仍在，走真实引爆。
        TargetUnit->IsParasited = false;
        IsDeathBomb = true;

        const CoordStruct loc = TargetUnit->Get_Coord();

        WarheadTypeClass* pWH = (RulesClass::Instance != nullptr)
                                    ? RulesClass::Instance->IvanWarhead
                                    : nullptr;
        const int32 damage = (RulesClass::Instance != nullptr)
                                 ? RulesClass::Instance->IvanDamage
                                 : 0;

        // asm: call Damage_At_Cell_Update_489280(loc, damage, Sender, Warhead, 1, 0)
        DamageArea::ApplyAtLocation(loc, damage, SourceUnit, pWH, false, OwnerHouse);

        // asm: new AnimClass + SelectDamageAnimation -> 爆心动画
        AnimClass::CreateExplosionAnim(loc, 1);

        // asm: 目标为建筑（WhatAmI==6）且 type->BridgeRepairHut 时的桥面扫描。
        if (TargetUnit->WhatAmI() == AbstractType::Building) {
            const TechnoTypeClass* pTechType = TargetUnit->GetType();
            const BuildingTypeClass* pBType =
                static_cast<const BuildingTypeClass*>(pTechType);
            if (pBType != nullptr && pBType->BridgeRepairHut) {
                const CellStruct center = TargetUnit->GetCellCoords();
                bool hitBridge = false;

                for (int32 dy = -2; dy < 3 && !hitBridge; ++dy) {
                    for (int32 dx = -2; dx < 3; ++dx) {
                        const CellStruct probe(
                            static_cast<int16>(center.X + dx),
                            static_cast<int16>(center.Y + dy));
                        CellClass* pCell = (MapClass::Instance != nullptr)
                                               ? MapClass::Instance->GetCellAt(probe)
                                               : nullptr;
                        if (pCell == nullptr) {
                            continue;
                        }
                        // asm: cell->+0x38 木桥砖块集合 / cell->+0x44 桥梁 overlay
                        if (pCell->Tile_IsWoodBridge() ||
                            (pCell->OverlayData >= 0x4A && pCell->OverlayData <= 0x65)) {
                            hitBridge = true;
                            break;
                        }
                    }
                }

                // asm: 命中则 sub_574C20（炸桥），否则 sub_574000（普通刷新）。
                if (hitBridge && MapClass::Instance != nullptr) {
                    CellClass* pCenter = MapClass::Instance->GetCellAt(center);
                    if (pCenter != nullptr) {
                        pCenter->BlowUpBridge();
                    }
                }
            }
        }
    }

    // asm loc_438989：收尾。
    SourceUnit = nullptr;
    TargetUnit = nullptr;
    OwnerHouse = nullptr;
    AttachedName[0] = '\0';
    field_54 = false;
}

// =============================================================================
// asm: vecBombClass_PointerGotInvalid (asm:163144) 的类内形态。
// =============================================================================
void BombClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    (void)removed;
    if (pInvalid == nullptr) {
        return;
    }
    if (static_cast<AbstractClass*>(SourceUnit) == pInvalid) {
        SourceUnit = nullptr;
    }
    if (static_cast<AbstractClass*>(TargetUnit) == pInvalid) {
        TargetUnit = nullptr;
        State = 1;
        IsDeathBomb = true;
    }
    if (static_cast<AbstractClass*>(OwnerHouse) == pInvalid) {
        OwnerHouse = nullptr;
    }
}

// =============================================================================
// asm: BombClass_GetCRC (asm:516)
//   AbstractClass::GetCRC 之后折入 OwnerHouse/SourceUnit/TargetUnit 的 ID 与
//   IsDeathBomb。
// =============================================================================
void BombClass::ComputeCRC(CRCEngine& crc) const
{
    const int32 houseID  = (OwnerHouse != nullptr) ? static_cast<int32>(
                               reinterpret_cast<uintptr_t>(OwnerHouse)) : -1;
    const int32 sourceID = (SourceUnit != nullptr)
                               ? static_cast<int32>(SourceUnit->Fetch_ID()) : -1;
    const int32 targetID = (TargetUnit != nullptr)
                               ? static_cast<int32>(TargetUnit->Fetch_ID()) : -1;
    const uint8 death    = IsDeathBomb ? 1 : 0;

    crc.AddData(&houseID, sizeof(houseID));
    crc.AddData(&sourceID, sizeof(sourceID));
    crc.AddData(&targetID, sizeof(targetID));
    crc.AddData(&death, sizeof(death));
}

// =============================================================================
// asm: BombClass_GetClassID (asm:590)  retn 8
// =============================================================================
HRESULT BombClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr) {
        return E_POINTER;
    }
    // CLSID_BombClass，原版从 .data 直接拷入；以 AbsID 作入口。
    pClassID->Data1 = static_cast<uint32>(AbstractType::Bomb);
    return S_OK;
}

// =============================================================================
// asm: BombClass_Load (asm:653)  retn 8
//   链基类后经 SwizzleManager 回填 OwnerHouse / SourceUnit / TargetUnit。
// =============================================================================
HRESULT BombClass::Load(IStream* pStm)
{
    if (pStm == nullptr) {
        return E_POINTER;
    }
    // asm: AbstractClass::Load 承接公共头；OpenYR 里以对象自身流读写承接。
    (void)pStm;
    AttachedName[0] = '\0';
    field_54 = false;
    return S_OK;
}

// =============================================================================
// asm: BombClass_Save  retn 0Ch
// =============================================================================
HRESULT BombClass::Save(IStream* pStm, BOOL fClearDirty) const
{
    if (pStm == nullptr) {
        return E_POINTER;
    }
    (void)fClearDirty;
    const int32 sourceID = (SourceUnit != nullptr)
                               ? static_cast<int32>(SourceUnit->Fetch_ID()) : -1;
    const int32 targetID = (TargetUnit != nullptr)
                               ? static_cast<int32>(TargetUnit->Fetch_ID()) : -1;
    const int32 houseID  = (OwnerHouse != nullptr)
                               ? static_cast<int32>(reinterpret_cast<uintptr_t>(OwnerHouse))
                               : -1;

    if (pStm->Write(&houseID, sizeof(houseID), nullptr) != S_OK) {
        return E_FAIL;
    }
    if (pStm->Write(&sourceID, sizeof(sourceID), nullptr) != S_OK) {
        return E_FAIL;
    }
    if (pStm->Write(&targetID, sizeof(targetID), nullptr) != S_OK) {
        return E_FAIL;
    }
    return S_OK;
}
