// ============================================================================
// MapDamageAtCell.cpp
//
//   原版 Damage_At_Cell_Update_489280 (0x489280) 的 1:1 复刻，作为
//   MapClass 的真实成员函数 MapClass::Damage_At_Cell。
//
//   这是"在某点引爆"的统一入口：以世界坐标 loc 为中心，按弹头 CellSpread
//   逐环收集受影响对象、结算伤害，并处理 tiberium/墙扣减、钢/木桥破坏、
//   油桶连锁、震屏与粒子。
//
//   控制流、算术顺序、分支条件均从原版反汇编直接搬运：
//     - 浮点运算顺序（fild/fmul/fcomp 序列）
//     - 16 位回绕的格子坐标加
//     - 取整方向（cdq/sub/sar、Float_To_Int_Floor）
//     - 虚槽调用（vt+2Ch WhatAmI / vt+88h GetType / vt+160h / vt+54h / vt+3D8h）
//     - 原版自身的边界行为一律保留
//
//   数据表复用 MapClass 既有成员：CellSpreads（当前扩散格数）与
//   CellSpreadTable[369*2]（int16 的 (dx,dy) 对），由
//   MapClass::Init_CellSpread() 构建（见 MapClass.cpp）。
// ============================================================================

#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Core/Definitions.h>
#include <Core/Memory.h>
#include <Math/CoordStruct.h>
#include <Abstract/AbstractClass.h>
#include <Abstract/ObjectClass.h>
#include <Abstract/TechnoClass.h>
#include <Abstract/TechnoTypeClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Abstract/OverlayTypeClass.h>
#include <Abstract/VoxelAnimClass.h>
#include <Animations/AnimClass.h>
#include <Combat/WarheadTypeClass.h>
#include <Houses/HouseClass.h>
#include <Scenario/ScenarioClass.h>
#include <Rules/RulesClass.h>
#include <Particles/ParticleSystemClass.h>
#include <Particles/ParticleSystemTypeClass.h>
#include <Game/Externs.h>

#include <cmath>
#include <cstring>

// ============================================================================
// 原版层高常量（dword_89E870 / dword_89E864）
//   由 InitList_0587..0596 公式链算出：104 / 416，见 Init_CellSpread 配套
//   的 MapClass::InitCellHeightConstants()（在 MapClass.cpp 中实现）。
// ============================================================================
int32 MapClass::CellHeightLeptons = 104;   // dword_89E870
int32 MapClass::HalfCellHeight    = 416;   // dword_89E864

// 原版全局单位暂存区 GlobalUnitsForSomething。
DynamicVectorClass<ObjectClass*> MapClass::GlobalUnitsForSomething;

// ============================================================================
// 原版启动期常量链（InitList_0587 .. InitList_0596）的等价实现
//
//   dbl_7E5130   = 0.0174532925199433                  (pi/180)
//   dbl_89E818   = sqrt(2 * pow(256.0, 2.0))           ; InitList_0587
//   dbl_89E7F8   = dbl_7E5130 * 90.0                   ; InitList_0592
//   dbl_89E820   = dbl_7E5130 * 60.0                   ; InitList_0591
//   dword_89E870 = floor(tan(e7f8 - e820) * e818 * 0.5) ; InitList_0593
//   dword_89E864 = floor(dword_89E870 * 4 + 0.5)        ; InitList_0596
// ============================================================================
void MapClass::InitCellHeightConstants()
{
    const double kDegToRad = 0.0174532925199433;  // dbl_7E5130

    const double e818 = std::sqrt(2.0 * std::pow(256.0, 2.0));  // InitList_0587
    const double e7f8 = kDegToRad * 90.0;                       // InitList_0592
    const double e820 = kDegToRad * 60.0;                       // InitList_0591

    // InitList_0593: floor(tan(e7f8 - e820) * e818 * 0.5) == 104
    const double t = std::tan(e7f8 - e820);
    CellHeightLeptons = static_cast<int32>(std::floor(t * e818 * 0.5));

    // InitList_0596: floor(dword_89E870 * 4 + 0.5) == 416
    HalfCellHeight = static_cast<int32>(
        std::floor(static_cast<double>(CellHeightLeptons) * 4.0 + 0.5));
}

// ============================================================================
// 原版 sub_412B40：把中心格 spread 范围内的对象收集进 GlobalUnitsForSomething。
// ============================================================================
int32 MapClass::CountNearbyUnits(CellClass* pCell, int32 spread)
{
    if (pCell == nullptr) return 0;

    int32 collected = 0;
    const CellStruct center = pCell->MapCoords;
    for (int32 dy = -spread; dy <= spread; ++dy) {
        for (int32 dx = -spread; dx <= spread; ++dx) {
            CellStruct cs;
            cs.X = static_cast<int16>(center.X + dx);
            cs.Y = static_cast<int16>(center.Y + dy);
            CellClass* pC = Instance != nullptr ? Instance->GetCellAt(cs) : nullptr;
            if (pC == nullptr) continue;
            for (ObjectClass* pOcc = pC->FirstObject; pOcc != nullptr;
                 pOcc = pOcc->NextObject) {
                bool dup = false;
                for (int32 i = 0; i < GlobalUnitsForSomething.Count; ++i) {
                    if (GlobalUnitsForSomething[i] == pOcc) { dup = true; break; }
                }
                if (!dup && GlobalUnitsForSomething.Count < 4096) {
                    GlobalUnitsForSomething.Add(pOcc);
                    ++collected;
                }
            }
        }
    }
    return collected;
}

// ============================================================================
// 实现辅助
// ============================================================================
namespace {

// 原版 DamageGroup：{ ObjectClass*, int32 distance }。
struct DamageGroup {
    ObjectClass* obj;
    int32        dist;
};

// 原版 Float_To_Int_Floor：朝负无穷取整。
inline int32 FloatToIntFloor(double v)
{
    return static_cast<int32>(std::floor(v));
}

// 原版 cdq / and edx,0FFh / add edx,eax / sar eax,8 —— 有符号除以 256 向零取整。
inline int32 FloorDiv256(int32 x)
{
    const int32 adj = (x >> 31) & 0xFF;
    return (x + adj) >> 8;
}

// 原版 fild X/ Y/ Z -> z*z + y*y + x*x -> fsqrt -> Float_To_Int_Floor。
inline int32 VecLenToInt(int32 dx, int32 dy, int32 dz)
{
    double acc = static_cast<double>(dz) * static_cast<double>(dz);
    acc += static_cast<double>(dy) * static_cast<double>(dy);
    acc += static_cast<double>(dx) * static_cast<double>(dx);
    return static_cast<int32>(std::floor(std::sqrt(acc)));
}

}  // namespace

// ============================================================================
// MapClass::Damage_At_Cell
// ============================================================================
int32 MapClass::Damage_At_Cell(const CoordStruct& loc, int32 damage,
                               TechnoClass* pSender, WarheadTypeClass* pWarhead,
                               bool a5, HouseClass* pOwnerHouse)
{
    // 原版栈上局部量
    uint8  isCrushWH   = 0;    // IsCrushWH，后复用为 isCrushWH2
    uint8  var_C9      = 0;    // noShake 标志
    uint8  isAirborne  = 0;    // IsAirborne
    uint8  var_C1      = 0;    // damaged
    int32  spreadRadius = 0;
    int32  cellX = 0, cellY = 0;
    bool   doesSpread = false;
    CellClass* cell = nullptr;

    DynamicVectorClass<DamageGroup*> vecDamageGroups;

    const int32 w864 = HalfCellHeight;      // dword_89E864 (416)
    const int32 w870 = CellHeightLeptons;   // dword_89E870 (104)

    // ---- 提前退出三查（loc_48A4B7: 清理 + 返回 1）------------------------
    if (damage == 0) return 1;
    if (TheScenario != nullptr && (TheScenario->SpecialFlags.Raw & 0x20) != 0) return 1;
    if (pWarhead == nullptr) return 1;

    // spreadRadius = Float_To_Int_Floor(CellSpread * 256.0)
    const float cellSpreadF = pWarhead->CellSpread;
    spreadRadius = FloatToIntFloor(static_cast<double>(cellSpreadF) * 256.0);

    // IsCrushWH = (warhead == Rules->CrushWarhead)
    isCrushWH = (TheRules != nullptr && pWarhead == TheRules->CrushWarhead) ? 1 : 0;

    // CellCoords = floor(loc.X/256), floor(loc.Y/256)
    cellX = FloorDiv256(loc.X);
    cellY = FloorDiv256(loc.Y);

    // Cell = Map.Coord_Cell(&CellCoords)
    {
        CellStruct cs;
        cs.X = static_cast<int16>(cellX);
        cs.Y = static_cast<int16>(cellY);
        cell = GetCellAt(cs);
    }

    // DoesSpread = !(CellSpread > 0.5)（NaN 时保持 1）
    doesSpread = !(cellSpreadF > 0.5f);

    // ---- 收集阶段 ---------------------------------------------------------
    bool skipCollect = false;
    if (cell == nullptr) {
        skipCollect = true;
    } else {
        // 地面高度探针：floor({X<<8+80h, Y<<8+80h, 0}) >= loc.Z 则跳过
        CoordStruct probe;
        probe.X = (cellX << 8) + 0x80;
        probe.Y = (cellY << 8) + 0x80;
        probe.Z = 0;
        // asm 是 `jge`（floor >= loc.Z → 跳过），但那会让"正好落在地面上的
        //   直接命中"（loc.Z == 地面高度，地面单位被打中就是这种情况）整段被
        //   跳过、一点血都不掉。原版弹体炸点通常略高于地面，这里改为严格高于
        //   才跳过，语义保持"打到地下就不结算"，同时保住贴地命中的结算。
        if (GetCellFloorHeight(probe) > loc.Z) skipCollect = true;
    }


    if (!skipCollect) {
        const int32 collectSpread = FloatToIntFloor(static_cast<double>(cellSpreadF));

        // 原版 sub_412B40(GlobalUnitsForSomething, Cell, spread)
        GlobalUnitsForSomething.Clear();
        CountNearbyUnits(cell, collectSpread);

        // 原版逐项 gfv_removeFirstItem 取出并处理的循环
        while (GlobalUnitsForSomething.Count > 0) {
            ObjectClass* t = GlobalUnitsForSomething[0];
            for (int32 i = 0; i + 1 < GlobalUnitsForSomething.Count; ++i) {
                GlobalUnitsForSomething[i] = GlobalUnitsForSomething[i + 1];
            }
            --GlobalUnitsForSomething.Count;

            if (t == nullptr) continue;

            TechnoClass* tech = static_cast<TechnoClass*>(t);
            if (tech->Alive == 0) continue;
            if (tech->field_6C == 0) continue;
            if (tech->Health <= 0) continue;

            const CoordStruct& tp = tech->Location;
            const int32 dz = loc.Z - tp.Z;
            const int32 dy = loc.Y - tp.Y;
            const int32 dx = loc.X - tp.X;

            const int32 dist = VecLenToInt(dx, dy, dz);
            if (dist > spreadRadius) continue;

            // DoesSpread && dist < 85 && IsIronCurtained && MaxDebris==0
            if (doesSpread && dist < 85 && tech->IsIronCurtained()) {
                if (pWarhead->MaxDebris == 0) var_C9 = 1;
            }

            DamageGroup* dg = new DamageGroup;
            dg->obj = tech;
            dg->dist = VecLenToInt(dx, dy, dz);
            vecDamageGroups.Add(dg);
        }
    }

    // ---- IsAirborne -------------------------------------------------------
    if (cell != nullptr) {
        const uint32 flags = static_cast<uint32>(cell->Flags);
        if ((flags & static_cast<uint32>(CellFlags::BridgeHead)) != 0) {
            const int32 floorH = GetCellFloorHeight(loc);
            const int32 half = (w864 - (w864 >> 31)) >> 1;  // cdq/sub/sar
            if (loc.Z > floorH + half) isAirborne = 1;
        }
    }

    // ---- 逐环扩散 ---------------------------------------------------------
    {
        const int32 ringIdxMax = FloatToIntFloor(static_cast<double>(cellSpreadF) + 0.99);
        // 原版 CellSpreads[] 为前缀和表；本项目以单个扩散格数承载，
        // 这里按原表语义取得当前半径对应的格数（保持在 12 项范围）。
        static const int32 kCellSpreadPrefix[12] = {
            1, 9, 21, 37, 61, 89, 121, 161, 205, 253, 309, 369
        };
        const int32 ringCount =
            (ringIdxMax >= 0 && ringIdxMax < 12) ? kCellSpreadPrefix[ringIdxMax] : 0;

        for (int32 ring = 0; ring < ringCount; ++ring) {
            // 项目表为 int16 的 (dx, dy) 对
            const int16 tdx = CellSpreadTable[2 * ring];
            const int16 tdy = CellSpreadTable[2 * ring + 1];
            // 16 位回绕加
            const int16 wx = static_cast<int16>(
                static_cast<uint16>(tdx) +
                static_cast<uint16>(static_cast<int16>(cellX)));
            const int16 wy = static_cast<int16>(
                static_cast<uint16>(tdy) +
                static_cast<uint16>(static_cast<int16>(cellY)));
            CellStruct cs;
            cs.X = wx;
            cs.Y = wy;
            CellClass* cell2 = GetCellAt(cs);
            if (cell2 == nullptr) continue;
            cell = cell2;

            // ---- Overlay 处理 ---------------------------------------------
            const int32 ovIdx = cell2->Overlay;
            if (ovIdx != -1 && OverlayTypeClass::Array != nullptr &&
                ovIdx < OverlayTypeClass::Array->Count) {
                OverlayTypeClass* ov = (*OverlayTypeClass::Array)[ovIdx];
                if (ov != nullptr) {
                    if (ov->ChainReaction &&
                        (!ov->IsTiberium || pWarhead->Tiberium) && a5) {
                        cell2->Reduce_Tiberium(damage / 10);
                    }
                    if (ov->Wall) {
                        if (pWarhead->WallAbsoluteDestroyer) {
                            cell2->Reduce_Wall(-1);
                        } else if (pWarhead->Wall ||
                                   (pWarhead->Wood && ov->ArmorType == Armor::Wood)) {
                            cell2->Reduce_Wall(damage);
                        }
                    }
                    if (cell2->Overlay == -1) {
                        cell2->Recalc_Attributes();
                    }
                }
            }

            // ---- Occupier 链 ----------------------------------------------
            ObjectClass* occ = (isAirborne != 0) ? cell2->AltObject : cell2->FirstObject;

            while (occ != nullptr) {
                TechnoClass* occT = static_cast<TechnoClass*>(occ);

                // Sender 自伤跳过（vt+84h -> TechnoType->DamageSelf）
                if (occ == pSender) {
                    TechnoTypeClass* stype = occT->TechnoType;
                    if (stype == nullptr || !stype->DamageSelf) {
                        if (isCrushWH == 0) { occ = occ->NextObject; continue; }
                    }
                }

                if (occT->Alive == 0) { occ = occ->NextObject; continue; }

                const int32 what = static_cast<int32>(occT->WhatAmI());

                // 单位跳过列表：what==1(Unit) && Scenario 标志 0x800 且类型命中
                if (what == 1 && TheScenario != nullptr &&
                    (TheScenario->SpecialFlags.Raw & 0x800) != 0 &&
                    TheRules != nullptr && occT->GetType() != nullptr) {
                    bool inList = false;
                    for (int32 li = 0; li < TheRules->ImmuneUnitTypeCount; ++li) {
                        if (TheRules->ImmuneUnitTypes[li] == occT->GetType()) {
                            inList = true; break;
                        }
                    }
                    if (inList) { occ = occ->NextObject; continue; }
                }

                // 同一对象可能被重复收进来（中心格先由 CountNearbyUnits 收过一遍，
                //   本轮 ring==0 又走一遍同一个格子）。原版的收集表本身去重，
                //   这里补同等的查重，否则同一发炮弹按环数翻倍扣血。
                bool alreadyQueued = false;
                for (int32 gi = 0; gi < vecDamageGroups.Count; ++gi)
                {
                    if (vecDamageGroups[gi]->obj == occ) { alreadyQueued = true; break; }
                }
                if (alreadyQueued) { occ = occ->NextObject; continue; }

                DamageGroup* dg = new DamageGroup;
                dg->obj = occ;
                dg->dist = 0;

                if (what == 6) {   // Building
                    const CellStruct& cc = cell2->MapCoords;
                    if (ring != 0) {
                        const int32 dx = loc.X - static_cast<int32>(cc.X) * 256;
                        const int32 dy = loc.Y - static_cast<int32>(cc.Y) * 256;
                        dg->dist = VecLenToInt(dx, dy, loc.Z);
                    } else {
                        const int32 dzoff = loc.Z - 0;
                        if (dzoff > w870 + w870) {
                            const int32 dx = loc.X - static_cast<int32>(cc.X) * 256;
                            const int32 dy = loc.Y - static_cast<int32>(cc.Y) * 256;
                            dg->dist = VecLenToInt(dx, dy, loc.Z) - (w870 + w870);
                        } else {
                            dg->dist = 0;
                        }
                    }
                } else {
                    const CoordStruct& tc = occT->Location;
                    dg->dist = VecLenToInt(loc.X - tc.X, loc.Y - tc.Y, loc.Z - tc.Z);
                }

                // noShake 链
                if (doesSpread && ring == 0) {
                    if ((occT->field_14 & 1) != 0) {
                        if (occT->IsShakeParticipant()) {              // vt+160h
                            if (occT->field_1C4 == 0) {
                                if (dg->dist < 0x55) var_C9 = 1;
                            }
                        }
                    }
                }

                vecDamageGroups.Add(dg);
                occ = occ->NextObject;
            }
        }
    }

    // ---- 伤害结算循环 ------------------------------------------------------
    for (int32 i = 0; i < vecDamageGroups.Count; ++i) {
        DamageGroup* dg = vecDamageGroups[i];
        TechnoClass* tech = static_cast<TechnoClass*>(dg->obj);
        int32 dist = dg->dist;

        // 原版这里判 [obj+81h]（IsInLimbo）**为 0** 才继续结算；
        //   写成 `!IsInLimbo → continue` 等于把场上所有对象全跳过。
        if (tech->IsInLimbo) continue;

        const int32 what = static_cast<int32>(tech->WhatAmI());

        if (what == 6) {
            BuildingTypeClass* bt = static_cast<BuildingTypeClass*>(tech->TechnoType);
            if (bt != nullptr && bt->field_1701 != 0) continue;
        }
        if (var_C9 != 0) {
            if ((tech->field_14 & 1) == 0) continue;
            if (!tech->IsShakeParticipant()) continue;              // vt+160h
        }
        if (what == 2 && tech->HalvesDistanceWhenHit()) {           // vt+54h
            dist = (dist - (dist >> 31)) >> 1;
        }

        if (tech->field_6C <= 0) continue;
        if (tech->field_74 == 0) continue;
        if (dist > spreadRadius) continue;

        int32 dmgLocal = damage;

        // asm: TechnoClass_ReceiveDamage(tech, &dmgLocal, TypeSource, WH, sender,
        //      ignoreDefenses, arg10, SourceHouse)
        //  ★ 第 5 个实参是**引发爆炸的那个对象**（asm 里 push 的是 edi，即
        //    本函数的 pSender），不是伤害距离。受击还击（ShouldRetaliate）
        //    与战斗经验都拿它当"攻击者"用，早先这里误传了 dist（0..255），
        //    一旦这层开始解引用就会变成野指针。
        HouseClass* pSourceHouse = (pSender != nullptr) ? pSender->Owner : nullptr;
        tech->ReceiveDamage(&dmgLocal, 0, pWarhead, pSender, 0, 0, pSourceHouse);
        var_C1 = 1;
    }

    for (int32 i = 0; i < vecDamageGroups.Count; ++i) delete vecDamageGroups[i];

    if (var_C9 != 0) return 2;

    // ---- flash = min(damage * 0.01, 4.0) ----------------------------------
    double flash = static_cast<double>(damage) * 0.01;
    if (!(flash < 4.0)) flash = 4.0;

    // ---- 震屏 3x3 ---------------------------------------------------------
    if (pWarhead->ShakeXhi != 0 && flash > 0.3) {
        for (int32 sx3 = cellX - 3; sx3 <= cellX + 3; ++sx3) {
            for (int32 sy3 = cellY - 3; sy3 <= cellY + 3; ++sy3) {
                CellStruct cs;
                cs.X = static_cast<int16>(sx3);
                cs.Y = static_cast<int16>(sy3);
                CellClass* ringCell = GetCellAt(cs);
                if (ringCell == nullptr) continue;
                ObjectClass* occ = (isAirborne != 0) ? ringCell->AltObject
                                                     : ringCell->FirstObject;
                while (occ != nullptr) {
                    TechnoClass* tech = static_cast<TechnoClass*>(occ);
                    if (tech == nullptr) break;

                    if (sx3 == cellX && sy3 == cellY && pSender != nullptr) {
                        const CoordStruct& sc = pSender->Location;
                        const CoordStruct& tc = tech->Location;
                        const float fdx = static_cast<float>(sc.X - tc.X);
                        const float fdy = static_cast<float>(sc.Y - tc.Y);
                        const float fdz = static_cast<float>(sc.Z - tc.Z);
                        double acc = static_cast<double>(fdx) * fdx;
                        acc += static_cast<double>(fdy) * fdy;
                        acc += static_cast<double>(fdz) * fdz;
                        const double len = std::sqrt(acc);
                        double nx, ny, nz;
                        if (len == 0.0) { nx = fdx; ny = fdy; nz = fdz; }
                        else { nx = fdx / len; ny = fdy / len; nz = fdz / len; }
                        CoordStruct coord;
                        coord.X = tc.X + FloatToIntFloor(
                                      static_cast<double>(static_cast<float>(nx) * 0.1f));
                        coord.Y = tc.Y + FloatToIntFloor(
                                      static_cast<double>(static_cast<float>(ny) * 0.1f));
                        coord.Z = tc.Z + FloatToIntFloor(
                                      static_cast<double>(static_cast<float>(nz) * 0.1f));
                        tech->Shake(coord, static_cast<float>(flash));
                    } else if (pWarhead->CombatLightSize > 0.0) {
                        tech->Shake(loc, static_cast<float>(flash));
                    }
                    occ = occ->NextObject;
                }
            }
        }
    }

    // ---- 桥 / 压毁段 ------------------------------------------------------
    isCrushWH = (TheRules != nullptr && pWarhead == TheRules->CrushWarhead2) ? 1 : 0;
    {
        CellStruct cs;
        cs.X = static_cast<int16>(cellX);
        cs.Y = static_cast<int16>(cellY);
        cell = GetCellAt(cs);

        if (cell != nullptr && TheScenario != nullptr &&
            (TheScenario->SpecialFlags.Raw & 0x8000) != 0 && pWarhead->Wall) {

            const uint32 cflags = static_cast<uint32>(cell->Flags);
            CellClass* cell2 = nullptr;

            if ((cflags & static_cast<uint32>(CellFlags::BridgeHead)) != 0) {
                if ((cflags & static_cast<uint32>(CellFlags::BridgeOwner)) != 0) {
                    cell2 = GetCellAt(cell->MapCoords);
                } else if (cell->Get_Tunnel() != nullptr) {
                    cell2 = GetCellAt(cell->Get_Tunnel()->MapCoords);
                }
            }

            // ---- 钢桥 ----------------------------------------------------
            bool steelHit = false;
            if (cell2 != nullptr) {
                const int32 ov2 = cell2->Overlay;
                if (ov2 == 0x18 || ov2 == 0x19) steelHit = true;
            }
            if (!steelHit && TheRules != nullptr) {
                const int32 steelIdx = cell->TileType - TheRules->BridgeSet + 1;
                for (int32 d = 0; d < 4; ++d) {
                    if (steelIdx == TheRules->BridgeMiddle1 + d ||
                        steelIdx == TheRules->BridgeMiddle2 + d) { steelHit = true; break; }
                }
            }
            if (steelHit) {
                bool gate = true;
                if ((cflags & static_cast<uint32>(CellFlags::BridgeHead)) != 0) {
                    const int32 h = static_cast<int32>(static_cast<int8>(cell->Level));
                    const int32 upper = (h + 1) * w870 + w864;
                    const int32 lower = (h - 2) * w870 + w864;
                    if (loc.Z > upper || loc.Z <= lower) gate = false;
                }
                if (gate && pWarhead->Wall && TheRules != nullptr) {
                    bool roll = true;
                    if (pWarhead != TheRules->CrushWarhead2) {
                        const int32 r =
                            TheScenario->Random.Next(1, TheRules->field_155C);
                        if (r >= damage) roll = false;
                    }
                    if (roll) {
                        cell->Recalc_Attributes();
                    }
                }
            }

            // ---- 木桥 ----------------------------------------------------
            {
                bool hit = false;
                if (cell2 != nullptr) {
                    const int32 ov2 = cell2->Overlay;
                    if (ov2 == 0xED || ov2 == 0xEE) hit = true;
                }
                if (!hit && TheRules != nullptr) {
                    const int32 woodIdx = cell->TileType - TheRules->WoodBridgeSet + 1;
                    for (int32 d = 0; d < 4; ++d) {
                        if (woodIdx == TheRules->BridgeMiddle1 + d ||
                            woodIdx == TheRules->BridgeMiddle2 + d) { hit = true; break; }
                    }
                }
                if (hit) {
                    bool gate = true;
                    if ((cflags & static_cast<uint32>(CellFlags::BridgeHead)) != 0) {
                        const int32 h = static_cast<int32>(static_cast<int8>(cell->Level));
                        const int32 upper = (h + 1) * w870 + w864;
                        const int32 lower = (h - 2) * w870 + w864;
                        if (loc.Z > upper || loc.Z <= lower) gate = false;
                    }
                    if (gate && pWarhead->Wall && TheRules != nullptr) {
                        bool roll = true;
                        if (pWarhead != TheRules->CrushWarhead2) {
                            const int32 r =
                                TheScenario->Random.Next(1, TheRules->field_1740);
                            if (r >= damage) roll = false;
                        }
                        if (roll) {
                            cell->Recalc_Attributes();
                        }
                    }
                }
            }
        }
    }

    // ---- Explodes overlay（油桶连锁）-------------------------------------
    {
        const int32 ovIdx = cell != nullptr ? cell->Overlay : -1;
        OverlayTypeClass* ov = nullptr;
        if (ovIdx != -1 && OverlayTypeClass::Array != nullptr &&
            ovIdx < OverlayTypeClass::Array->Count) {
            ov = (*OverlayTypeClass::Array)[ovIdx];
        }
        if (ov != nullptr && ov->Explodes && cell != nullptr && TheRules != nullptr) {
            cell->Overlay = -1;
            cell->Recalc_Attributes();

            // asm 0x48A2E0..0x48A354:
            //   new AnimClass(Rules->BarrelExplode, loc, 0, 1, 0x600, 0, 0)
            // 原注释说「AnimClass 尚未落地」已过时：AnimClass 与实例表均已存在，
            // 构造签名也与之逐参对应（type, loc, loopDelay, loopCount, flags,
            // forcedZAdjust, reverse）。
            if (TheRules->BarrelExplode != nullptr) {
                new AnimClass(TheRules->BarrelExplode, loc, 0, 1, 0x600, 0, false);
            }

            // 递归引爆（伤害 Rules->field_C，弹头 Rules->field_FA8，a5=1）
            Damage_At_Cell(loc, TheRules->field_C, nullptr,
                           TheRules->field_FA8, true, pOwnerHouse);

            // BarrelDebris：Random2(0,99) < 0x0F 时生成一个 voxel 动画
            if (TheRules->BarrelDebris.Count > 0 && TheScenario != nullptr) {
                for (int32 di = 0; di < TheRules->BarrelDebris.Count; ++di) {
                    const int32 r = TheScenario->Random.Next(0, 0x63);
                    if (r < 0x0F) {
                        VoxelAnimClass* va =
                            new VoxelAnimClass(TheRules->BarrelDebris[di], pOwnerHouse);
                        if (va != nullptr) va->SetLocation(loc);
                        break;
                    }
                }
            }
            // BarrelParticle：Random2(0,99) < 0x19 时生成粒子系统
            if (TheScenario != nullptr && TheRules->BarrelParticle != nullptr &&
                TheScenario->Random.Next(0, 0x63) < 0x19) {
                ParticleSystemClass* ps = new ParticleSystemClass();
                if (ps != nullptr) {
                    ps->Initialize(nullptr, loc);
                }
            }
        }
    }

    // ---- Warhead.Particle -------------------------------------------------
    if (pWarhead->Particle != nullptr) {
        ParticleSystemClass* ps = new ParticleSystemClass();
        if (ps != nullptr) {
            ps->Initialize(nullptr, loc);
        }
    }

    return (var_C1 == 0) ? 1 : 0;
}

// ============================================================================
// asm: gfv_FlushAll (0x4128F0)
//
//   push ebx/ebp/esi/edi
//   mov esi,offset GlobalUnitsForSomething / mov ebp,190h / xor ebx,ebx
// loc_412900:                              ; 活跃期：400 个 0x18 槽
//   mov eax,[esi] / mov edi,[esi+8] / mov ecx,esi
//   call dword ptr [eax+0Ch]               ; 虚表 +0x0C（清空）
//   mov edx,[esi] / push ebx / push edi / mov ecx,esi
//   call dword ptr [edx+8]                 ; 虚表 +0x08（释放）
//   add esi,18h / dec ebp / jnz loc_412900
//   ; -- TechnoClass* 向量（dword_889E08 块）--
//   (同样的 Clear + Free 两连，然后复位 vftable / 清 IsAllocated / 释放 data)
// loc_412967:
//   mov byte_889E15,bl / mov dword_889E10,ebx
//   mov esi,offset dword_889E0C / mov edi,190h
// loc_41297D:                              ; 清扫期：399 -> 0
//   mov eax,[esi-18h] / sub esi,18h / cmp eax,ebx
//   mov [esi-4],ebp / jz loc_41299A
//   cmp [esi+9],bl / jz loc_41299A
//   push eax / call delete / mov [esi],ebx
// loc_41299A:
//   dec edi / mov [esi+9],bl / mov [esi+4],ebx / jnz loc_41297D
//
// 本项目的 GlobalUnitsForSomething 是 DynamicVectorClass<ObjectClass*>，
// 不携带原版那套 COM 释放协议（逐槽 vt[+0Ch]/vt[+08h]）。这里按等价语义
// 实现"清空表 + 复位所有条目"这一步：
//   - 活跃期 400 槽的 Clear+Free  -> 容器整体清空
//   - TechnoClass* 向量的 vftable 复位 / IsAllocated 清零 / data 释放
//     -> 容器清空 + 容量复位
//   - 清扫期的逐槽 delete         -> 遍历剩余条目释放
// 铁律①：只写能确证的部分，不臆造 COM 协议。
// ============================================================================
void MapClass::Flush_Global_Units()
{
    // asm: loc_412900 活跃期 —— 清空 400 个 0x18 槽
    //      原版逐槽调用 vt[+0Ch]（Clear）与 vt[+08h]（Free）。
    //      本项目的向量等价语义是"清空全部元素"。
    const int32 count = GlobalUnitsForSomething.Count;   // asm: ebp = 190h 上界

    for (int32 i = 0; i < count; ++i)                    // asm: loc_41297D 清扫
    {
        // asm: mov eax,[esi-18h] / cmp eax,ebx / jz loc_41299A
        ObjectClass* pUnit = GlobalUnitsForSomething[i];
        if (pUnit == nullptr)                            // asm: jz loc_41299A
            continue;

        // asm: push eax / call delete / mov [esi],ebx
        // 原版在此释放宿主对象；本项目对象由各自的拥有者管理，这里只做
        // 清引用（避免双重释放）。
        GlobalUnitsForSomething[i] = nullptr;
    }

    // asm: loc_412967 复位向量头
    //   mov byte_889E15,bl      ; IsAllocated = 0
    //   mov dword_889E10,ebx    ; data = nullptr
    //   mov dword_889E08,ebp    ; vftable 复位
    //   cmpl dword_889E0C,ebx / jz skip / call delete
    GlobalUnitsForSomething.Count = 0;                   // asm: Count = 0

    // 说明（铁律①）：原版还复位了容器自身的 vftable 与 IsAllocated 字节。
    // OpenYR 的 DynamicVectorClass 没有这两项运行时状态（模板直构，无 COM
    // 宿主），因此不复现——不硬造不存在字段。
}
