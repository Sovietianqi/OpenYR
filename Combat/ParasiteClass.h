#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/Timer.h"

class TechnoClass;
class HouseClass;
class AnimClass;
class AbstractClass;

// ============================================================================
// ParasiteClass - 寄生武器（恐怖机器人 / 乌贼）
//
// 权威来源：/workspace/asm_export/ParasiteClass.asm.txt
//
// 原版 sizeof=0x58，AbstractClass 之后：
//   +0x24 Controller     发起寄生的单位（恐怖机器人本体）
//   +0x28 Recipient      被寄生的宿主
//   +0x2C TimerA         感染计时（StartTime/…/TimeLeft）
//   +0x38 TimerB         伤害计时（StartTime/…/TimeLeft）
//   +0x44 SquidGrapple   吸附特效动画（乌贼缠斗）
//   +0x48 State          乌贼缠斗状态机 0..4
//   +0x4C StateCounter   状态机内层计数
//   +0x50 FrameCounter   动画节拍
//   +0x54 field_54       已登记 vec_Abstracts_4 标志
//
// 注意：只有「海军且有机体」的宿主（即乌贼对舰船）才走 UpdateSquiddy 缠斗
// 分支；其余宿主走 Update 的「钻入后按武器节奏造成伤害」分支。
// ============================================================================
class ParasiteClass {
public:
    ParasiteClass() noexcept;
    ~ParasiteClass();

    // asm: ParasiteClass_Update (asm:1027)
    //   宿主为空直接返回；宿主 Naval&&Organic 走缠斗；否则按武器节奏对宿主
    //   造成伤害，直到宿主死亡跳出。
    void Update();

    // asm: ParasiteClass_UpdateSquiddy (asm:449)
    //   吸附缠斗状态机：0 起手 → 1/2/3 推进 → 4 攻击。
    void UpdateSquiddy();

    // asm: ParasiteClass_CanInfect (asm:1517)
    //   逐项校验目标是否可被寄生：非建筑、可被寄生、未在城堡、无挂载、
    //   类型 Parasiteable、非 BunkerLinked；乌贼（海军有机物）还要求目标在水上。
    bool CanInfect(TechnoClass* pTarget) const;

    // asm: ParasiteClass_Infect (asm:1601)
    //   清理旧状态 → 若不可寄生则让控制器脱离 → 否则把控制器挂到宿主上。
    void Infect(TechnoClass* pTarget);

    // asm: Parasite_ExitUnit (0x62A4E0, 464 行)
    //   控制器脱离宿主：算出退出朝向与落格、校验可达与可通行、把控制器
    //   重新放到地图上，然后复位缠斗态与宿主的寄生簿记。
    void ExitUnit();

    // asm: sub_62AB40 —— 宿主所在格能不能让寄生体落地（0x62AB40，无栈参）。
    //   宿主为空 / 宿主在空中 / 宿主格上有地形对象 / 水域·岸·岩上叠加物落在
    //   特定区间 / 格上有建筑 —— 任一命中即 false。
    bool Can_Exit_Host() const;

    // asm: ParasiteClass_PointerGotInvalid (asm:1278)
    //   三个可能失效的指针：Controller(+0x24) / Recipient(+0x28) / SquidGrapple(+0x44)。
    void PointerGotInvalid(AbstractClass* pInvalid, bool removed);

    // ---- 字段 ----
    TechnoClass* Controller;   // +0x24
    TechnoClass* Recipient;    // +0x28
    CDTimerClass InfectTimer;  // +0x2C
    CDTimerClass DamageTimer;  // +0x38
    AnimClass*   SquidGrapple; // +0x44
    int32        State;        // +0x48
    int32        StateCounter; // +0x4C
    int32        FrameCounter; // +0x50
    bool         field_54;     // +0x54

    // asm: sub_629720 —— 推进缠斗动画帧，StateCounter 到 10 时返回 true。
    bool AdvanceGrapple();
};
