#include "ParasiteClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/FootClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Animations/AnimClass.h"
#include "../Game/Game.h"
#include "../Houses/HouseClass.h"
#include "../AI/TeamClass.h"
#include "../Combat/WeaponTypeClass.h"

// ============================================================================
// ParasiteClass —— 寄生武器（恐怖机器人 / 巨型乌贼）
//
// asm 权威（/workspace/asm_export/ParasiteClass.asm.txt，sizeof=0x58）：
//   +0x24 Controller    发起寄生的单位（寄生体本体）
//   +0x28 Recipient     被寄生的宿主
//   +0x2C InfectTimer   感染计时（StartTime +0x2C / TimeLeft +0x34）
//   +0x38 DamageTimer   伤害计时（StartTime +0x38 / TimeLeft +0x40）
//   +0x44 SquidGrapple  吸附缠斗动画（乌贼）
//   +0x48 State         乌贼缠斗状态机 0..4
//   +0x4C StateCounter  状态机内层计数
//   +0x50 FrameCounter  动画节拍
//   +0x54 field_54      已登记 vec_Abstracts_4 标志
//
// ⚠ 架构说明：原版 ParasiteClass 派生自 AbstractClass（CTOR 首行即调
//   AbstractClass_CTOR 并铺 4 张虚表）。OpenYR 目前把该类建模为独立结构
//   （见 Combat/ParasiteClass.h），与 MissionClass/CellClass 属同一类
//   "继承链扁平化"缺口，待"重建真实调用链"阶段统一处理。
// ============================================================================

// ----------------------------------------------------------------------------
// asm: ParasiteClass_CTOR (asm:96)
//   AbstractClass_CTOR
//   [+0x2C] = Frame          ; InfectTimer.Start
//   [+0x34] = 0              ; InfectTimer.TimeLeft
//   [+0x38] = Frame          ; DamageTimer.Start
//   [+0x40] = 0              ; DamageTimer.TimeLeft
//   [+0x24] = Owner(param)   ; Controller
//   [+0x28] = 0              ; Recipient
//   [+0x3C] = 0
//   [+0x44] = [+0x48] = [+0x4C] = [+0x50] = 0
//   [+0x54] = 0
//   登记进 vec_Parasites
// ----------------------------------------------------------------------------
ParasiteClass::ParasiteClass() noexcept
    : Controller(nullptr)
    , Recipient(nullptr)
    , SquidGrapple(nullptr)
    , State(0)
    , StateCounter(0)
    , FrameCounter(0)
    , field_54(false)
{
    InfectTimer.Start(0);
    DamageTimer.Start(0);
}

ParasiteClass::~ParasiteClass()
{
    // asm: ParasiteClass_DCTOR —— 摘除缠斗动画后注销。
    if (SquidGrapple != nullptr) {
        SquidGrapple->UnInit();
        SquidGrapple = nullptr;
    }
}

// ============================================================================
// asm: ParasiteClass_CanInfect(ParasiteClass *this, _TechnoClass *target) (asm:1517)
//
//   逐项否决（任一命中即返回 false）：
//     1) target == nullptr
//     2) byte[target+0x81] != 0        ; 已在 limbo / 不可交互
//     3) byte[target+0x90] == 0        ; 未激活
//     4) [target+0x6C] == 0            ; 缺关键关联对象（无主）
//     5) [target+0x694] != 0           ; 已被寄生（target->Parasite 非空）
//     6) type(target)->Parasiteable == 0
//     7) target->BunkerLinkedItem != 0 ; 窝在掩体里
//     8) 若 Controller 存在且类型为 Naval（乌贼）：
//          宿主所在格必须是水（CellClass_Tile_IsWater）
//   全部通过 → true。
// ============================================================================
bool ParasiteClass::CanInfect(TechnoClass* pTarget) const
{
    if (pTarget == nullptr)
        return false;

    // 2) 目标已进入 limbo / 尚不可交互。
    if (pTarget->IsInLimbo)
        return false;

    // 3) 目标必须处于激活状态。
    if (!pTarget->IsActive())
        return false;

    // 4) 目标必须有所属方（无主对象不在正常交互链上）。
    if (pTarget->Owner == nullptr)
        return false;

    // 5) 已被别的寄生体占据。
    if (pTarget->Parasite != nullptr)
        return false;

    // 6) 类型层面必须允许寄生。
    TechnoTypeClass* pTargetType = pTarget->TechnoType;
    if (pTargetType == nullptr || !pTargetType->Parasiteable)
        return false;

    // 7) 窝在掩体内不可寄生。
    if (pTarget->BunkerLinkedItem != nullptr)
        return false;

    // 8) 乌贼（海军寄生体）只能寄生于水面上的目标。
    if (Controller != nullptr && Controller->TechnoType != nullptr
        && Controller->TechnoType->Naval)
    {
        CellClass* pCell = pTarget->GetCell1();
        if (pCell == nullptr || !pCell->Tile_IsWater())
            return false;
    }

    return true;
}

// ============================================================================
// asm: ParasiteClass_Infect(ParasiteClass *this, _TechnoClass *target) (asm:1601)
//
//   1) 先清缠斗态：[+0x4C]=[+0x50]=[+0x48]=0；
//      若 SquidGrapple 非空 → 调其 vt+0xF8（结束动画）并置空。
//   2) 若 field_54 为真 → 从 vec_Abstracts_4 中摘除自己，field_54=0。
//   3) DamageTimer 重设为当前帧。
//   4) 校验 CanInfect：
//        - 不可寄生 → 令 Controller 脱离（ExitUnit 路径），返回；
//        - 可寄生   → Recipient = target（+0x28），宿主反向记录寄生体。
// ============================================================================
void ParasiteClass::Infect(TechnoClass* pTarget)
{
    // 1) 清缠斗状态。
    StateCounter = 0;
    FrameCounter = 0;
    State        = 0;

    if (SquidGrapple != nullptr) {
        SquidGrapple->UnInit();     // asm: vt+0xF8
        SquidGrapple = nullptr;
    }

    // 2) 注销 vec_Abstracts_4 登记。
    //   ⚠ OpenYR 未建模 vec_Abstracts_4（原版的"待处理抽象对象"表），
    //     此处保留标志位清扫，登记动作待该表建模后补。
    field_54 = false;

    // 3) 伤害计时重置到当前帧。
    DamageTimer.Start(0);

    // 4) 校验并挂钩。
    if (!CanInfect(pTarget)) {
        ExitUnit();
        return;
    }

    Recipient = pTarget;
    if (Controller != nullptr) {
        // 宿主反向记录寄生体（asm: target->Parasite = this）。
        pTarget->Parasite = this;
    }
}

// ============================================================================
// asm: sub_62AB40 —— 宿主所在格能不能让寄生体落地（0x62AB40，无栈参）
//
//   if (!Recipient)                                return false;   // 62AB4B
//   if (Recipient->InAir())                        return false;   // vt+54h
//   if (Controller->GetType()->Naval)              goto 最终判据;   // 62ABFB
//   if (Controller && Controller->WhatAmI() == 1) {               // Unit
//       if (Recipient->GetCell(0)->GetTerrain())   return false;   // 63AB.. 
//   }
//   cell = Recipient->GetCell(0);
//   if (cell->Land == ltWater || ltBeach || ltRock) {              // 62ABA5
//       bl = 1;
//       if (cell->Flags & 0x0100)                  bl = 0;         // test bl,ch
//       if (0x4A <= cell->Overlay <= 0x63)         bl = 0;
//       if (!(0xCD <= cell->Overlay <= 0xE6) && bl) return false;
//   }
//   return cell->Cell_Building() == nullptr;                        // 62AC0F
// ============================================================================
bool ParasiteClass::Can_Exit_Host() const
{
    if (Recipient == nullptr)
        return false;

    // asm 62AB50: Recipient->vt+54h —— 腾空中的宿主不能落地（原版 InAir 槽）。
    if (Recipient->InAir())
        return false;

    TechnoTypeClass* pControllerType =
        (Controller != nullptr) ? Controller->GetType() : nullptr;
    const bool naval = (pControllerType != nullptr) && pControllerType->Naval;

    CellClass* pCell = Recipient->GetCell1();
    if (pCell == nullptr)
        return false;

    if (!naval) {
        // asm 62AB7E-62AB95: 陆行寄生体（恐怖机器人）脱离前要求宿主格上没有
        // 地形对象；控制器必须是单位（WhatAmI == 1）才走这一步。
        if (Controller != nullptr
            && Controller->WhatAmI() == AbstractType::Unit
            && pCell->Terrain != nullptr)
            return false;

        // asm 62ABA5-62AC06: 水域 / 岸 / 岩格上，叠加物落在特定区间时不许脱离。
        const LandType land = static_cast<LandType>(pCell->Land);
        if (land == LandType::Water || land == LandType::Beach
            || land == LandType::Rock) {
            bool okToExit = true;
            if ((pCell->Field_140 & 0x100u) != 0)
                okToExit = false;                    // asm: test bl,ch
            if (pCell->Overlay >= 0x4A && pCell->Overlay <= 0x63)
                okToExit = false;
            if (!(pCell->Overlay >= 0xCD && pCell->Overlay <= 0xE6) && okToExit)
                return false;
        }
    }

    // asm loc_62AC0F: 格上有建筑就不许落地。
    return pCell->Cell_Building() == nullptr;
}

// ============================================================================
// asm: void __thiscall Parasite_ExitUnit(ParasiteClass *this)  (0x62A4E0)
//
//   控制器（+0x24）从宿主（+0x28）上脱离。逐段：
//     2181-2196  type = Controller->GetTechnoType(); bl = type->Naval;
//                Desired_Facing256(&host->Facing(+388h), &a2);
//                oct = ((a2>>12)+1)>>1 & 7
//     2199-2226  再取一次朝向，按 oct <= 2 取 +3FFFh 或 -3FFFh，只写回 a2 的低字
//     2216-2233  海军：exit 格 = host 格坐标 + CellCoordDirections[oct]
//     62A57A     非海军：InfectTimer 到期才继续，否则走 62A89B 的早退
//     62A5B7     交集：Map.Coord_Cell(exit) → 宿主格；MovementZone/速度取自类型；
//                Map.CanLocationBeReached + CellClass::Is_Clear_To_Move；
//                不可通行则改走 sub_703590（本工程 Nearby_Location）
//     62A63C     取落点的世界坐标；哨兵坐标 / 空位 → 62A851 失败路径
//     2412-2432  sub_62AB40 门槛 + Controller->Put(coord, facing)
//     2434-2460  432h 标记 + Is_Player → Select；Team->RecruitUnit
//     2463-2528  目的地/目标/任务复位、6A0h 时间戳、缠斗动画与状态机清空
//     loc_62A851 控制器 field_6C 清零 + UnInit
//     loc_62A862 宿主 328h/6A0h/694h 清空，Recipient = 0
// ============================================================================
void ParasiteClass::ExitUnit()
{
    TechnoClass* pController = Controller;   // +0x24
    TechnoClass* pHost       = Recipient;    // +0x28
    if (pController == nullptr || pHost == nullptr)
        return;                              // 原版不判空，由调用点保证

    // asm: CellCoordDirections（0x89F688，4 字节步长，每项 2 个 word）——
    //   索引 0..7 = N/NE/E/SE/S/SW/W/NW，+0 = dx（asm `add dx,[tbl+e*4]`），
    //   +2 = dy（asm `mov cx,[89F688h+e*4+2]`）。朝向 0 = 正北，故索引 0
    //   必须是 (0,-1)；取值与 HouseClass 里同一张表的 AI_Defense_CellDirs 一致。
    static const int8 kCellCoordDirections[8][2] = {
        {  0, -1 }, {  1, -1 }, {  1,  0 }, {  1,  1 },
        {  0,  1 }, { -1,  1 }, { -1,  0 }, { -1, -1 },
    };

    TechnoTypeClass* pControllerType = pController->GetType();
    const bool naval = (pControllerType != nullptr) && pControllerType->Naval;

    // asm 2181-2196: 第一个朝向只用来算八分向。
    int32 facing1 = 0;
    TechnoClass::Desired_Facing256(&pHost->PrimaryFacing, &facing1);
    int32 oct = ((facing1 >> 12) + 1) >> 1;
    oct &= 7;

    // asm 2199-2226: 第二次朝向按八分向偏 ±3FFFh，只回写低 16 位
    // （高 16 位保留第一次调用留下的内容 —— 原版就是这样读回的）。
    int32 facing2 = 0;
    TechnoClass::Desired_Facing256(&pHost->PrimaryFacing, &facing2);
    uint16 face = static_cast<uint16>(facing2 & 0xFFFF);
    if (oct <= 2)
        face = static_cast<uint16>(face + 0x3FFF);
    else
        face = static_cast<uint16>(face - 0x3FFF);

    const int32 exitFacing = static_cast<int32>(
        (static_cast<uint32>(facing1) & 0xFFFF0000u) | static_cast<uint32>(face));

    // asm 2420-2430: 落位调用的朝向实参 = ((var_28 >> 7) + 1) >> 1 & 0FFh
    // （256 分度，正北 0）。
    const uint8 exitFacingByte =
        static_cast<uint8>(((exitFacing >> 7) + 1) >> 1);

    CellStruct coords(0, 0);

    if (naval) {
        // asm 2216-2233: 海军按八分向在宿主格旁边找落点
        // （vt+1B8 = 宿主格坐标，返回 CellStruct*）。
        int32 oc = ((exitFacing >> 12) + 1) >> 1;
        oc &= 7;
        CellStruct here(0, 0);
        pHost->GetCellCoords1(&here);
        coords.X = static_cast<int16>(here.X + kCellCoordDirections[oc][0]);
        coords.Y = static_cast<int16>(here.Y + kCellCoordDirections[oc][1]);
    } else {
        // asm loc_62A57A: InfectTimer 到期判定。
        const int32 start = InfectTimer.StartTime;      // +0x2C
        int32 duration    = InfectTimer.TimeLeft;       // +0x34
        bool proceed;
        if (start != -1) {
            const int32 elapsed = Game::CurrentFrame - start;
            if (elapsed >= duration) {
                proceed = true;                         // asm: jge 62A59F
            } else {
                duration -= elapsed;                    // asm: sub eax,ecx
                proceed = (duration == 0);              // asm: cmp eax,ebx
            }
        } else {
            proceed = (duration == 0);                  // asm 62A593
        }

        if (!proceed) {
            // asm loc_62A89B: 计时未到 —— 只清控制器标记并解挂，宿主格原样保留。
            pController->field_6C = 0;                  // asm: [edx+6Ch],ebx
            pHost->ParasiteStampFrame    = Game::CurrentFrame;
            pHost->ParasiteStampSpare    = 0;           // 原版读未初始化栈槽 var_8
            pHost->ParasiteStampDuration = 0;
            pHost->Parasite          = nullptr;         // asm: [ecx+694h],ebx
            pController->UnInit();                      // asm: call [edx+0F8h]
            Recipient = nullptr;                        // asm: [esi+28h],ebx
            return;
        }

        // asm 62A5A1: 落点就是宿主所在格（vt+1B8 写出并返回格坐标）。
        CellStruct here(0, 0);
        pHost->GetCellCoords1(&here);
        coords = here;
    }

    // ── 共享段（loc_62A5B7）────────────────────────────────────────────────
    MapClass* pMap = MapClass::Instance;
    CellClass* pCell = (pMap != nullptr) ? pMap->GetCellAt(coords) : nullptr;

    // asm 2308-2322: MovementZone 取自类型（[type+5B4h]，sub_703590 里同样
    // 按 MovementZone 使用），速度类别取自类型（[type+67Ch]）。
    const MovementZone zone = pControllerType->MoveZone;

    // asm 2323-2327: Map.CanLocationBeReached(&coords, zone)。
    const int32 reach = (pMap != nullptr)
        ? static_cast<int32>(pMap->Can_Location_Be_Reached(
              CellClass::Cell2Coord(coords), false, static_cast<int32>(zone)))
        : 0;
    (void)reach;   // 原版把可达性作为 vt+84h 的实参再取一次类型，结果未被使用

    const int32 speedType = static_cast<int32>(pControllerType->SpeedTypeValue);

    // asm 2337-2342: 格不可通行则用 sub_703590 就近另找落点。
    bool clearToMove = false;
    if (pCell != nullptr) {
        clearToMove = pCell->Is_Clear_To_Move(speedType, false, false, -1,
                                              zone, -1, false);
    }
    if (!clearToMove) {
        if (pMap != nullptr) {
            coords = pMap->Nearby_Location(coords, speedType, zone);
            pCell  = pMap->GetCellAt(coords);
        }
    }

    // asm 62A63C-62A6E2: 落点先比 word_AC4928/2A 哨兵（相等直接失败，不再
    // 取世界坐标）；否则取格世界坐标（cell vt+48h），与 stru_AC4948 默认
    // 坐标全等同样失败。两道关卡都过才调 vt+62AB40 与 vt+0D8h 落位。
    bool placed = false;
    CoordStruct exitXYZ(0, 0, 0);
    const bool defaultCell =
        (coords.X == MapClass::DefaultCellCoords.X
         && coords.Y == MapClass::DefaultCellCoords.Y);
    if (!defaultCell && pCell != nullptr && pCell->Is_On_Map()) {
        exitXYZ = pCell->Get_CellCoords();           // asm: cell vt+48h
        const bool defaultXYZ =
            (exitXYZ.X == 0 && exitXYZ.Y == 0 && exitXYZ.Z == 0);
        if (!defaultXYZ && Can_Exit_Host()) {        // asm 2412: sub_62AB40(this)
            // asm 2420-2430: vt+0D8(Put)(&xyz, 朝向) —— 落位并写入朝向。
            pController->SetLocation(exitXYZ);
            placed = pController->Unlimbo();
            if (placed)
                pController->PrimaryFacing.Value = exitFacingByte;
        }
    }

    if (placed) {
        // asm 2434-2449: 宿主被玩家控制时，摘掉寄生体的 432h 标记并选中它。
        if (pController->field_432 != 0) {
            HouseClass* pOwner = pController->Owner;
            if (pOwner != nullptr && pOwner->PlayerControl) {   // HouseClass::Is_Player
                pController->field_432 = 0;
                pController->Select();                          // asm: call [edx+14Ch]
            }
        }

        // asm 2452-2460: 控制器有自己的队伍就重新入队。
        {
            const AbstractType what = pController->WhatAmI();
            if (what == AbstractType::Unit || what == AbstractType::Infantry) {
                FootClass* pFoot = static_cast<FootClass*>(pController);
                if (pFoot->Team != nullptr)
                    pFoot->Team->RecruitUnit(pFoot, false);     // asm: RecruitUnit(unit, 0)
            }
        }

        // asm 2463-2506: 拆挂后的运动状态复位。
        //   if (![vt+4ACh](this)) { Assign_Destination_Cell(0); [vt+3C8h](0);
        //                          [vt+480h](0,1); }
        //   [vt+484h](0,1);  [vt+488h](0,0,0,0,0);
        // vt+4ACh 在本工程是 FootClass::Has_Planning_Token（规划索引 != -1），
        // vt+3C8h 直调 TechnoClass::Assign_Target，vt+484h 直调
        // TechnoClass::Enter_Idle_Mode。vt+480h（把目标格设为目的地）与
        // vt+488h（五参任务复位）在项目里没有等价入口，不另找函数顶替。
        FootClass* pFoot = static_cast<FootClass*>(pController);
        if (!pFoot->Has_Planning_Token()) {              // asm: call [eax+4ACh]
            pController->Assign_Destination_Cell(nullptr);   // asm: Assign_Destination_Cell(0)
            pController->Assign_Target(nullptr);         // asm: call [edx+3C8h]
        }
        pController->Enter_Idle_Mode(0, 1);              // asm: call [edx+484h]

        // asm 2503-2517: 控制器 +6A0h = {Frame, 备用槽, 3 * 武器 ROF}
        int32 stampDuration = 0;
        {
            WeaponStruct* pWeapon =
                (pControllerType != nullptr) ? pControllerType->GetWeapon(0) : nullptr;
            if (pWeapon != nullptr && pWeapon->WeaponType != nullptr)
                stampDuration = pWeapon->WeaponType->ROF;    // asm: [[vt3F8]+0B0h]
        }
        pController->ParasiteStampFrame    = Game::CurrentFrame;
        pController->ParasiteStampSpare    = 0;      // 原版读未初始化栈槽 var_8
        pController->ParasiteStampDuration = stampDuration * 3;      // asm: lea eax,[eax+eax*2]

        // asm 2518-2528: 缠斗态三连清 + 吸附动画收尾。
        State        = 0;
        StateCounter = 0;
        FrameCounter = 0;
        if (SquidGrapple != nullptr) {
            SquidGrapple->UnInit();                      // asm: call [ecx+0F8h]
            SquidGrapple = nullptr;
        }

        // asm loc_62A7FB: 摘掉 vec_Abstracts_4 里的登记。本工程尚未建模该向量
        // （见 Infect 的同处说明），这里只复位登记标志。
        field_54 = false;
    } else {
        // asm loc_62A851: 失败路径 —— 只清控制器的挂接标记并收尾。
        pController->field_6C = 0;                       // asm: [eax+6Ch],ebx
        pController->UnInit();                           // asm: call [edx+0F8h]
    }

    // asm loc_62A862: 两条路径都走这里 —— 宿主侧的挂接簿记清空。
    pHost->field_328             = 0;                    // asm: [eax+328h],ebx
    pHost->ParasiteStampFrame    = Game::CurrentFrame;
    pHost->ParasiteStampSpare    = 0;                    // 原版读未初始化栈槽 var_8
    pHost->ParasiteStampDuration = 0;
    pHost->Parasite              = nullptr;              // asm: [ecx+694h],ebx
    Recipient                    = nullptr;              // asm: [esi+28h],ebx
}

// ============================================================================
// asm: ParasiteClass_PointerGotInvalid(this, AbstractClass* pInvalid, int a3) (asm:1278)
//   retn 8（双参虚，与全项目统一签名一致）
//
//   1) pInvalid == Controller(+0x24) → Controller=0，直接返回。
//   2) pInvalid == Recipient(+0x28)  →
//        若全局 byte_A8ED5C == 0 → Recipient=0，直接返回；
//        否则做计时补偿：把 InfectTimer 的剩余量按已流逝帧重算，
//        使宿主消失后寄生体仍有等量的"重新吸附"宽限期。
//   3) pInvalid == SquidGrapple(+0x44) → 置空。
// ============================================================================
void ParasiteClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    (void)removed;

    if (pInvalid == nullptr)
        return;

    // 1) 寄生体本体失效。
    if (static_cast<AbstractClass*>(Controller) == pInvalid) {
        Controller = nullptr;
        return;
    }

    // 2) 宿主失效。
    if (static_cast<AbstractClass*>(Recipient) == pInvalid) {
        // asm: 全局 byte_A8ED5C 为真时才做计时补偿（会话同步模式下生效）。
        const bool syncActive = false;   // OpenYR 尚未建模该全局开关

        if (!syncActive) {
            Recipient = nullptr;
            return;
        }

        // 计时补偿：remaining = max(0, TimeLeft - (Frame - StartTime))
        const int32 startTime = InfectTimer.StartTime;
        int32 remaining = InfectTimer.TimeLeft;
        if (startTime != -1) {
            const int32 elapsed = Game::CurrentFrame - startTime;
            remaining = (elapsed < remaining) ? (remaining - elapsed) : 0;
        }
        if (remaining > 0)
            InfectTimer.Start(remaining);   // 沿用同一宽限期

        Recipient = nullptr;
        return;
    }

    // 3) 缠斗动画失效。
    if (static_cast<AbstractClass*>(SquidGrapple) == pInvalid)
        SquidGrapple = nullptr;
}

// ============================================================================
// asm: sub_629720 —— 推进缠斗动画帧。
//   每帧把 FrameCounter 递增；StateCounter 到 10 时把 State 推进一步并
//   归零计数，返回"本步是否完成"。
// ============================================================================
bool ParasiteClass::AdvanceGrapple()
{
    ++FrameCounter;

    if (++StateCounter >= 10) {
        StateCounter = 0;
        ++State;
        return true;
    }
    return false;
}

// ============================================================================
// asm: ParasiteClass_UpdateSquiddy (asm:449)
//   乌贼吸附缠斗状态机（0 起手 → 1/2/3 推进 → 4 攻击）。
// ============================================================================
void ParasiteClass::UpdateSquiddy()
{
    if (Recipient == nullptr || Controller == nullptr) {
        ExitUnit();
        return;
    }

    // 起手：尚未生成缠斗动画时建立簿记。
    //   原版以宿主坐标为锚点创建 Rules 的 SquidGrappleAnim；OpenYR 的动画
    //   创建需要具体的 AnimTypeClass 实例，规则未加载时跳过创建，仅复位状态。
    if (SquidGrapple == nullptr) {
        State        = 0;
        StateCounter = 0;
        FrameCounter = 0;
    }

    AdvanceGrapple();

    switch (State) {
    case 0:
    case 1:
    case 2:
    case 3:
        // 前进段：仅推进动画节拍。
        break;

    case 4:
    default:
        // 攻击段：完成一次缠斗攻击后回到起点。
        State = 0;
        break;
    }
}

// ============================================================================
// asm: ParasiteClass_Update (asm:1027)
//   宿主为空直接返回；若宿主类型为 Naval && Organic → 走 UpdateSquiddy 缠斗；
//   否则走"钻入后按武器节奏造成伤害"分支，直到宿主死亡跳出。
// ============================================================================
void ParasiteClass::Update()
{
    if (Recipient == nullptr)
        return;

    TechnoTypeClass* pRecipientType = Recipient->TechnoType;
    const bool isNavalOrganic = (pRecipientType != nullptr)
                             && pRecipientType->Naval
                             && pRecipientType->Organic;

    if (isNavalOrganic) {
        UpdateSquiddy();
        return;
    }

    // 普通寄生（恐怖机器人钻入）：宿主死亡即跳出。
    if (!Recipient->IsActive()) {
        ExitUnit();
        return;
    }

    // 按伤害计时节奏对宿主施加寄生体武器伤害。
    if (DamageTimer.Expired()) {
        DamageTimer.Start(0);
        if (Controller != nullptr)
            Controller->Fire(Recipient, 0);      // asm: 以 0 号武器对宿主开火
    }
}
