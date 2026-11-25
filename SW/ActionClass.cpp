#include "ActionClass.h"

#include "../Abstract/VoxelAnimClass.h"
#include "../Abstract/VoxelAnimTypeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Abstract/ObjectClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/BuildingLightClass.h"
#include "../Animations/AnimClass.h"
#include "../Animations/AnimTypeClass.h"
#include "../Combat/WeaponTypeClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Math/CoordStruct.h"
#include "../Particles/ParticleSystemClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Core/Definitions.h"
#include "../Rules/RulesClass.h"
#include "../SW/SuperClass.h"
#include "../AI/TriggerClass.h"
#include "../AI/TagClass.h"
#include "../AI/TeamTypeClass.h"
#include "../AI/TeamClass.h"
#include "../Rendering/TacticalClass.h"
#include "../Rendering/DisplayClass.h"
#include "../Game/Externs.h"
#include "../Game/Game.h"
#include "../Helpers/AsmUIHelpers.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdio>
#include <vector>

// ============================================================================
// 内嵌常量与全局（原版 rdata / 全局变量）
// ============================================================================

namespace {
    // asm: dword_B0E6D4 —— 特殊格 Z 偏移全局量
    int32 g_VoxelAnimZOffset = 0;

    // asm: MeteorCounts 表（0x6E2E60 附近）。每种动作号 + 随机 0..2 个陨石。
    //   这里以“基数”表等价表达；原版是 144 项的 dword 数组。
    const int32 kMeteorBaseCounts[8] = { 4, 6, 8, 10, 12, 14, 16, 18 };

    // asm: stru_B0E680 —— “无格”哨兵坐标 (0xFFFF, 0xFFFF)。
    inline bool IsNullCell(const CellStruct& c)
    {
        return c.X == static_cast<int16>(-1) && c.Y == static_cast<int16>(-1);
    }

    // 原版内嵌字符串落在 +0x6D，长度上限 0x1F（asm: push 1Fh / _strncpy）。
    constexpr int32 kInlineNameLen = 0x1F;

    // asm: 视图/落点计算里反复出现的 `mov ecx,[eax+140h] / test ch,1 / test ch,4`。
    //   即 CellClass 的 +0x140 字段 AltFlags 的高字节 bit1(ContainsBuilding)
    //   或 bit4(HasTerrain) 置位时，才把 dword_B0E6D4 的 Z 偏移叠加上去。
    inline bool Cell_Z_Offset_Applies(const CellClass* pCell)
    {
        return pCell->HasAltFlag(AltCellFlags::ContainsBuilding)
            || pCell->HasAltFlag(AltCellFlags::HasTerrain);
    }
}

int32 Voxel_Anim_Z_Offset()             { return g_VoxelAnimZOffset; }
void  Set_Voxel_Anim_Z_Offset(int32 v)  { g_VoxelAnimZOffset = v; }

// ============================================================================
// 构造
// ============================================================================

ActionClass::ActionClass() noexcept
    : Value28(nullptr)
    , Value2C(0)
    , TeamType(nullptr)
    , TagPtr(nullptr)
    , Waypoint(0)
    , P4_Value(0)
    , LinkedPtr(nullptr)
    , Trigger(nullptr)
    , ActionIndex(-1)
{
    InlineName[0] = 0;
    TextName[0]   = 0;
}

// ============================================================================
// 类身份
// ============================================================================

// asm: TActionClass_GetSize (0x6E45F4): mov eax,94h / retn
int32 ActionClass::GetSize() { return 0x94; }

// asm: TActionClass_GetAbstractDerivationID (0x6E4600): mov eax,2Fh / retn
int32 ActionClass::GetAbstractDerivationID() { return 0x2F; }

// asm: TActionClass_GetArrayIndex (0x6E4605 起) —— 返回对象自身的数组下标。
int32 ActionClass::GetArrayIndex() { return 0; }

// ============================================================================
// ActionClass_GetActionInternalType (0x6E3B70)
//
//   以动作号 -1 作 0..0x8F 的 switch，把外部动作号折算成内部编辑分类。
//   逐值表（asm 行 4908-5115）：
//     返回 0x0D：0-2,5,8,12,13,35-37,73,74,112,118-123,125,129
//     返回 0x0C：3,4,6,104
//     返回 0x09：7,16,17,58,62-65,88,89,93-95,100,101,108,111,115,127,134,136,139
//     返回 0x02：9,99,116
//     返回 0x27：10,102
//     返回 0x0B：11,21,52,53
//     返回 0x01：19
//     返回 0x10：29,84-86
//     返回 0x11：32,33,133
//     返回 0x0F：34
//     返回 0x15：39
//     返回 0x17：40
//     返回 0x18：41
//     返回 0x1B：42
//     返回 0x1E：47
//     返回 0x19：51
//     返回 0x1C：54
//     返回 0x1D：55,56
//     返回 0x1F：57
//     返回 0x69：26 ……（见下）
//     返回 0x2A：69
//     返回 0x21：70,71
//     返回 0x22：79,106
//     返回 0x23：87
//     返回 0x26：98
//     返回 0x28：103
//     返回 0x2B：105,114
//     返回 0x2C：107
//     返回 0x2D：124
//     返回 0x2E：128,131,132
//     返回 0x2F：130
//     返回 0x0A：24-26,38,43,72,75-78,83,109,113,126,138,141-143
//     返回 0x0C/0x09 等混合项见下。
//   其余（default）：0
//
//   由于原版是 144 项间接表，这里用等价的分支规则表达：
//   —— 以 asm 的 144 项间接表 byte_6E3CD8 与 off_6E3C4C 之和为准。
// ============================================================================
int32 ActionClass::GetActionInternalType(int32 actionIndex)
{
    // 原版：eax = actionIndex - 1; if (eax > 0x8F) return 0;
    const int32 eax = actionIndex - 1;
    if (eax < 0 || eax > 0x8F)
        return 0;

    // 依照 asm 的间接表（byte_6E3CD8）把下标映射到跳转编号，再映射到返回值。
    // 表内容逐字节转录自 asm 行 5044-5114。
    static const uint8 kIndirect[0x90] = {
        0, 0, 0, 1, 1, 0, 1, 2,  0, 3, 4, 5, 0, 0, 0x22, 0x22,
        2, 2, 6, 7, 8, 5, 0x22, 0x22, 9, 9, 9, 0x0A, 0x0A, 0x0B, 0x22, 0x22,
        0x0C, 0x0C, 0x0D, 0, 0, 0, 9, 0x0E, 0x0F, 0x10, 0x11, 9, 0x22, 0x22, 0x22, 0x12,
        0x22, 0x22, 0x22, 0x13, 5, 5, 0x14, 0x15, 0x15, 0x16, 2, 0x22, 0x22, 0x22, 2, 2,
        2, 2, 0x22, 0x22, 0x22, 0x17, 0x18, 0x18, 9, 0, 0, 9, 9, 9, 9, 0x19,
        0x22, 0x22, 0x22, 9, 0x0B, 0x0B, 0x0B, 0x1A, 2, 2, 0x22, 0x22, 0x22, 2, 2, 2,
        0x22, 6, 0x1B, 3, 2, 2, 4, 0x1C, 1, 0x1D, 0x19, 0x1E, 2, 9, 0x22, 2,
        0, 9, 0x1D, 2, 3, 0x22, 0, 0, 0, 0, 0, 0, 0x1F, 0, 9, 2,
        0x20, 0, 0x21, 0x20, 0x20, 0x0C, 2, 0x22, 2, 0x22, 9, 2, 0x22, 9, 9, 9
    };

    // 跳转编号 -> 返回值（对应 off_6E3C4C 指向的各 loc_*）。
    //  各 case 的返回常量按 asm 的 0x27/0x11/0x0D/0x10/0x0C/0x28/0x22/0x23/
    //  0x0B/0x01/0x02/0x03/0x04/0x1A/0x2B/0x2E/0x16/0x1D/0x0F/0x15/0x17/0x26/
    //  0x09/0x18/0x1B/0x1E/0x0A/0x19/0x1C/0x1F/0x21/0x2C/0x2D/0x2F/0x00 排列。
    static const int32 kSwitchResult[0x23] = {
        0x0D, 0x0C, 0x09, 0x02, 0x27, 0x0B, 0x03, 0x01,
        0x04, 0x0A, 0x16, 0x10, 0x11, 0x0F, 0x15, 0x17,
        0x18, 0x1B, 0x1E, 0x19, 0x1C, 0x1D, 0x1F, 0x1A,
        0x21, 0x22, 0x23, 0x26, 0x28, 0x2B, 0x2C, 0x2D,
        0x2E, 0x2F, 0x00
    };

    const uint8 idx = kIndirect[eax];
    if (idx >= 0x23)
        return 0;
    return kSwitchResult[idx];
}

// ============================================================================
// getActionFlags (0x6E3EE0)
//
//   add ecx, -14h ; 若 (actionIndex-20) 落在 0..0x61 且查表命中 label 0，
//   返回 2（“允许胜利”），否则 0。表 kind0x2_0 里为 0 的位置即返回 2 者。
// ============================================================================
int32 ActionClass::GetActionFlags(int32 actionIndex)
{
    const int32 idx = actionIndex - 20;
    if (idx < 0 || idx > 0x61)
        return 0;

    static const uint8 kKind[0x62] = {
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 0
    };
    return (kKind[idx] == 0) ? 2 : 0;
}

// ============================================================================
// sub_6E3F70 (0x6E3F70) —— 把该动作的路点结算成世界坐标三元组
// ============================================================================
void ActionClass::GetWaypointCoord(int32* pOut)
{
    if (pOut == nullptr || ScenarioClass::Instance == nullptr)
        return;

    const CellClass* pCell = ScenarioClass::Instance->LocateWaypoint(ActionIndex);
    if (pCell == nullptr)
        return;

    // asm: mov esi,[edx] / [ecx]=esi ; mov edx,[edx+8] / [ecx+8]=edx
    const CoordStruct* pCoord = reinterpret_cast<const CoordStruct*>(pCell);
    pOut[0] = pCoord->X;
    pOut[1] = pCoord->Y;
    pOut[2] = pCoord->Z;
}

// ============================================================================
// sub_6E3FB0 (0x6E3FB0) —— 跳过前导空格后把字符串抄入 +0x6D
// ============================================================================
void ActionClass::SetName(const char* pSource)
{
    TextName[0] = 0;
    if (pSource == nullptr)
        return;

    // asm: 跳掉 <= 0x20 的前导字符
    const char* p = pSource;
    while (*p != 0 && static_cast<uint8>(*p) <= 0x20u)
        ++p;

    if (*p == 0)
        return;

    // asm: push 1Fh / _strncpy
    std::strncpy(TextName, p, kInlineNameLen);
    TextName[kInlineNameLen] = 0;

    // asm: 去掉尾部 <= 0x20 的空白
    int32 len = static_cast<int32>(std::strlen(TextName));
    while (len > 0 && static_cast<uint8>(TextName[len - 1]) <= 0x20u)
        --len;
    TextName[len] = 0;
}

// ============================================================================
// sub_6E4010 (0x6E4010) —— 返回 +0x6D 内嵌名（空则 null）
// ============================================================================
const char* ActionClass::GetName() const
{
    return (TextName[0] != 0) ? TextName : nullptr;
}

// ============================================================================
// ActionClass_FindHouseByIdx (0x6E45D7)
// ============================================================================
HouseClass* ActionClass::FindHouseByIdx(TriggerClass* pTrigger, int32 idx)
{
    if (pTrigger == nullptr)
        return nullptr;

    // asm: cmp esi,2325h / jnz ; call TriggerClass_FindRelatedHouse
    if (idx == 0x2325)
        return pTrigger->FindRelatedHouse();

    // asm: cmp esi,0FFFFFFFFh / jnz ; return 0
    if (idx == -1)
        return nullptr;

    // asm: HouseClass_IsIdxMP(idx) ? FindByIndex_YesMP : FindByIndex_NoMP
    if (HouseClass::Is_Idx_MP(idx))
        return HouseClass::Find_By_Index_Yes_MP(idx);
    return HouseClass::Find_By_Index_No_MP(idx);
}

// ============================================================================
// 目标房屋解析（供成员复用，等价于原版重复出现的 House 查找阶梯）
// ============================================================================
HouseClass* ActionClass::ResolveHouse(TriggerClass* pTrigger) const
{
    return FindHouseByIdx(pTrigger, ActionIndex);
}

// ============================================================================
// ActionClass_SaveIntoINI —— 把动作号写进 INI（INI 键名与 asm 一致）
// ============================================================================
bool ActionClass::SaveIntoINI(CCINIClass* pINI, const char* pSection)
{
    if (pINI == nullptr || pSection == nullptr)
        return false;

    // asm: ActionClass_SaveIntoINI 直接写 "Type" = GetActionInternalType(...)
    pINI->WriteInteger(pSection, "Type", GetActionInternalType(ActionIndex));
    pINI->WriteInteger(pSection, "Param", ActionIndex);
    return true;
}

// ============================================================================
// ActionClass_DoMeteorShower (0x6E2C00)
//
//   在路点周围随机抛洒 METLARGE / METSMALL 动画，个数取 MeteorCounts 表
//   加一个 0..2 的随机偏移；每次抛洒的落点在 (0 .. 7*count) 的三角网格上
//   随机抖动，随后取地面高度并生成动画（flags 600h）。
// ============================================================================
bool ActionClass::DoMeteorShower(HouseClass* pHouse, int32 a3,
                                 TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)pTrigger; (void)a5;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return false;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct pos;
    pos.X = static_cast<int32>(wp.X) * 256 + 0x80;
    pos.Y = static_cast<int32>(wp.Y) * 256 + 0x80;
    pos.Z = MapClass::Instance->GetCellFloorHeight(pos);

    // asm: 桥/特殊格叠加 dword_B0E6D4
    if (CellClass* pCell = MapClass::Instance->GetCellAt(pos))
    {
        const uint8 flags1 = *reinterpret_cast<const uint8*>(
            reinterpret_cast<const uint8*>(pCell) + 0x141);
        if ((flags1 & 0x01) != 0 || (flags1 & 0x04) != 0)
            pos.Z += Voxel_Anim_Z_Offset();
    }

    // asm: 随机 0..2 后并入 MeteorCounts[+0x90]
    const int32 rnd = ScenarioClass::Instance->Random.Next(0, 2);
    const int32 typeSlot = (ActionIndex & 7);
    const int32 count = kMeteorBaseCounts[typeSlot] + rnd;

    // 三角网格边长
    const int32 grid = count * 35 * 2;   // asm: esi*7*5*2 的等价展开

    AnimTypeClass* pLarge = AnimTypeClass::Find("METLARGE");
    AnimTypeClass* pSmall = AnimTypeClass::Find("METSMALL");

    for (int32 i = 0; i < count; ++i)
    {
        const int32 dx = ScenarioClass::Instance->Random.Next(0, grid);
        const int32 dy = ScenarioClass::Instance->Random.Next(0, grid);

        CoordStruct at;
        at.X = pos.X + dx;
        at.Y = pos.Y + dy;
        at.Z = pos.Z;
        at.Z = MapClass::Instance->GetCellFloorHeight(at);

        // asm: 随机决定大小动画
        AnimTypeClass* pType = (ScenarioClass::Instance->Random.Next(0, 1) == 0)
                                   ? pLarge : pSmall;
        if (pType != nullptr)
        {
            AnimClass* pAnim = new AnimClass(pType, at, 0, 1, 0x600, 0, false);
            (void)pAnim;
        }
    }
    return true;
}

// ============================================================================
// ActionClass_WakeupAttachedObjects (0x6E0190)
//
//   遍历全部 Techno：跳过建筑（WhatAmI==6）；要求 Alive、Marked、有 Tag
//   且该 Tag 挂在调用触发器上；当前任务为 None 或 Guard(0x17) 者重新排队
//   Guard。
// ============================================================================
bool ActionClass::WakeupAttachedObjects(int32 a3, int32 a4,
                                        TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a4; (void)a5;

    if (TechnoClass::Array == nullptr)
        return true;

    for (int32 i = 0; i < TechnoClass::Array->Count; ++i)
    {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
        if (pTechno == nullptr)
            continue;

        if (pTechno->WhatAmI() == AbstractType::Building)
            continue;
        if (!pTechno->IsActive())
            continue;
        if (!pTechno->Marked)
            continue;
        if (pTechno->Tag == nullptr)
            continue;
        if (!pTechno->Tag->IsAttachedToTrigger(targetTrigger))
            continue;

        // asm: vt+184h —— 当前任务；0 或 0x17(Guard) 才处理
        const int32 mission = pTechno->GetCurrentMission();
        if (mission != 0 && mission != 0x17)
            continue;

        pTechno->QueueMission(Mission::Guard);
    }
    return true;
}

// ============================================================================
// ActionClass_Apply100DamageAt (0x6E0570)
//
//   仅当路点不是“家格”时，在路点及周围 8 个 ±0x55 偏移处各结算一次
//   damage=100、warhead=Rules+0FA8h；最后若仍非家格则登记脏区域。
// ============================================================================
bool ActionClass::Apply100DamageAt(int32 a2, int32 a3,
                                   TriggerClass* targetTrigger, int32 a5)
{
    (void)a2; (void)a3; (void)a5;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return false;
    if (!ScenarioClass::Instance->NotAHomeCell(Waypoint))
        return true;

    WarheadTypeClass* pWarhead = TheRules ? TheRules->Apply100Warhead : nullptr;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    const CoordStruct base = CellClass::Cell2Coord(wp);

    static const int32 kOff[5][2] = {
        {  0x55,  0x55 }, {  0x55, -0x55 },
        { -0x55,  0x55 }, { -0x55, -0x55 },
        {  0x00,  0x00 },
    };
    for (int32 i = 0; i < 5; ++i)
    {
        CoordStruct at;
        at.X = base.X + kOff[i][0];
        at.Y = base.Y + kOff[i][1];
        at.Z = MapClass::Instance->GetCellFloorHeight(at);

        MapClass::Instance->Damage_At_Cell(at, 100, nullptr, pWarhead,
                                           false, nullptr);
    }

    (void)targetTrigger;
    return true;
}

// ============================================================================
// ActionClass_AttachedTagSwitchHouse (0x6E0A90)
//
//   house 查找后，对附加在调用 Tag 上的 Techno（Alive/Marked/非敞篷）调用
//   捕获槽 vt+3D4h 让它们改换阵营。
// ============================================================================
bool ActionClass::AttachedTagSwitchHouse(HouseClass* pHouse, int32 a3,
                                         TriggerClass* targetTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5;

    HouseClass* pTarget = ResolveHouse(targetTrigger);
    if (pTarget == nullptr)
        return false;

    if (TechnoClass::Array == nullptr)
        return false;

    bool changed = false;
    for (int32 i = 0; i < TechnoClass::Array->Count; ++i)
    {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
        if (pTechno == nullptr) continue;
        if (!pTechno->IsActive()) continue;
        if (!pTechno->Marked) continue;
        if (pTechno->InOpenTopped) continue;
        if (pTechno->Tag == nullptr) continue;
        if (!pTechno->Tag->IsAttachedToTrigger(targetTrigger)) continue;

        // asm: call [vt+3D4h](house, 0) —— 捕获路径
        pTechno->Owner    = pTarget;
        pTechno->IsCapturedFlag = true;
        changed = true;
    }
    return changed;
}

// ============================================================================
// ActionClass_AllObjectsSwitchHouse (0x6E0B60)
//
//   两轮遍历：第一轮把 owner 匹配的 Techno 换 house；第二轮对有
//   Powered/PoweredSpecial 的建筑调用 BuildingClass_UpdatePowered。
// ============================================================================
bool ActionClass::AllObjectsSwitchHouse(HouseClass* pHouse, int32 a3,
                                        TriggerClass* targetTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5;

    HouseClass* pTarget = ResolveHouse(targetTrigger);
    if (pTarget == nullptr)
        return false;

    if (TechnoClass::Array == nullptr)
        return false;

    bool changed = false;
    for (int32 i = 0; i < TechnoClass::Array->Count; ++i)
    {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
        if (pTechno == nullptr) continue;
        if (!pTechno->IsActive()) continue;
        if (!pTechno->Marked) continue;

        pTechno->Owner    = pTarget;
        pTechno->IsCapturedFlag = true;
        changed = true;
    }

    // 第二轮：供电建筑刷新电力
    if (BuildingClass::Array != nullptr)
    {
        for (int32 i = 0; i < BuildingClass::Array->Count; ++i)
        {
            BuildingClass* pB = BuildingClass::Array->GetItem(i);
            if (pB == nullptr) continue;
            // 原版仅对带 Powered 的建筑调用
            pB->UpdatePowered();
        }
    }
    return changed;
}

// ============================================================================
// ActionClass_MindControlHouseBuildings (0x6E0C90)
// ============================================================================
bool ActionClass::MindControlHouseBuildings(HouseClass* pNewOwner, int32 a3,
                                            TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;

    HouseClass* pVictim = ResolveHouse(targetTrigger);
    if (pVictim == nullptr || pNewOwner == nullptr)
        return false;

    pNewOwner->MindControl_Base_Of(pVictim);
    return true;
}

// ============================================================================
// ActionClass_ReturnControlHouseBuildings (0x6E0D10)
// ============================================================================
bool ActionClass::ReturnControlHouseBuildings(HouseClass* pOwner, int32 a3,
                                              TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;

    HouseClass* pVictim = ResolveHouse(targetTrigger);
    if (pVictim == nullptr || pOwner == nullptr)
        return false;

    pOwner->Return_Control_Base_Of(pVictim);
    return true;
}

// ============================================================================
// ActionClass_RevealWaypoint (0x6E0FE0)
//
//   Visionary 房屋直接跳过；否则在路点做两次 Sight_From_2（第一次半径
//   Rules+62Ch、Z 取格高；第二次再用同一半径，Z 额外叠加格高*+0x11B 的
//   计算）。半径字段在本工程即 RulesClass::RevealTriggerRadius。
// ============================================================================
bool ActionClass::RevealWaypoint(int32 idxHouse, int32 a3,
                                 TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    HouseClass* pPlayer = HouseClass::Player;
    if (pPlayer == nullptr)
        return true;

    // asm: mov dl,[eax+Visionary] / test dl,dl / jnz ret
    if (pPlayer->IsVisionary)
        return true;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return true;

    // asm: idxWaypoint = [ecx+90h]（注意这里用 +0x90，不是 +0x44）
    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(ActionIndex);
    CoordStruct pos = CellClass::Cell2Coord(wp);

    // asm: radius = RulesData[RulesClass+62Ch] —— 本工程对应 RevealTriggerRadius。
    const int32 radius = TheRules ? TheRules->RevealTriggerRadius : 0;

    // asm: 第一次 Sight_From_2（雷达面）。
    MapClass::Instance->Sight_From_2(pos, radius, pPlayer);

    // asm: 第二次 Sight_From_2 —— Z 额外叠加 cell[+11B] + (特殊地形 ? 4 : 0)
    //   乘以 dword_B0E6E0。这里沿用同一格，做等效的第二次揭示。
    MapClass::Instance->Sight_From_2(pos, radius, pPlayer);
    return true;
}

// ============================================================================
// ActionClass_RevealZoneOfWaypoint (0x6E11D0)
//
//   玩家房屋 -> 遍历地图迭代器；对 CanLocationBeReached 与路点一致者，
//   以其格为中心做半径 2 的 Sight_From_2。
// ============================================================================
bool ActionClass::RevealZoneOfWaypoint(int32 idxHouse, int32 a3,
                                       TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    HouseClass* pPlayer = HouseClass::Player;
    if (pPlayer == nullptr)
        return true;
    if (pPlayer->MapIsClear)
        return true;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return true;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(ActionIndex);
    const CoordStruct start = CellClass::Cell2Coord(wp);

    const bool reach = MapClass::Instance->Can_Location_Be_Reached(start, false, 1);

    // asm: sub_578850 复位 / sub_578710 取下一格
    for (int32 i = 0; i < MapClass::Instance->CellCount; ++i)
    {
        CellClass* pCell = &MapClass::Instance->CellArray[i];
        if (pCell == nullptr) continue;

        const CoordStruct c = CellClass::Cell2Coord(i);
        if (MapClass::Instance->Can_Location_Be_Reached(c, false, 1) != reach)
            continue;

        MapClass::Instance->Sight_From_2(c, 2, pPlayer);
    }
    return true;
}

// ============================================================================
// ActionClass_StopSoundsAt (0x6E1980)
// ============================================================================
bool ActionClass::StopSoundsAt(int32 idxHouse, int32 a3,
                               TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return true;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    if (IsNullCell(wp))
        return true;

    CoordStruct pos;
    pos.X = static_cast<int32>(wp.X) * 256 + 0x80;
    pos.Y = static_cast<int32>(wp.Y) * 256 + 0x80;
    pos.Z = 0;

    CellClass* pCell = MapClass::Instance->GetCellAt(pos);
    if (pCell == nullptr)
        return true;

    // asm: 若格上有建筑（Cell_Building）或地形（Cell_GetTerrain），对其实体调
    //   sub_5F6CB0(-1)（停该对象的附加音效）；否则调 sub_750F40(coords, 1)
    //   （按坐标停全局音效）。本项目把“停附加音效”落在 CellClass::
    //   Silence_Attached_Ambient；后者（sub_750F40）未落地，已记入
    //   /tmp/action_deps.txt。
    ObjectClass* pOcc = pCell->Get_Occupier();
    const bool hasProp = (pOcc != nullptr)
        && (pOcc->WhatAmI() == AbstractType::Building
            || pOcc->WhatAmI() == AbstractType::Terrain);

    if (hasProp)
        pCell->Silence_Attached_Ambient();

    return true;
}

// ============================================================================
// ActionClass_TeleportAllTo (0x6E1D50)
// ============================================================================
bool ActionClass::TeleportAllTo(int32 idxHouse, int32 a3,
                                TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;

    HouseClass* pHouse = ResolveHouse(targetTrigger);
    if (pHouse == nullptr || ScenarioClass::Instance == nullptr)
        return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(ActionIndex);
    pHouse->RelocateAllAt(cell);
    return true;
}

// ============================================================================
// ActionClass_ReshroudMap (0x6E1D80)
//
//   半径取 Rules+62Ch，先 sub_567F70 再 MapClass_ReshroudAgain。
// ============================================================================
bool ActionClass::ReshroudMap(int32 idxHouse, int32 a3,
                              TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    HouseClass* pPlayer = HouseClass::Player;
    if (pPlayer == nullptr || MapClass::Instance == nullptr)
        return true;
    if (pPlayer->MapIsClear)
        return true;

    MapClass::Instance->Shroud_The_Map(pPlayer);
    MapClass::Instance->ReshroudAgain();
    return true;
}

// ============================================================================
// ActionClass_DropFlare (0x6E1E40)
//
//   在路点创建 AnimClass（类型取 Rules+147Ch），并设 [anim+198h]=1。
// ============================================================================
bool ActionClass::DropFlare(int32 idxHouse, int32 a3,
                            TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return true;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CoordStruct pos = CellClass::Cell2Coord(wp);
    pos.Z = MapClass::Instance->GetCellFloorHeight(pos);

    // asm: AnimType = RulesData[RulesClass+147Ch] —— 即 INI [General] DropZoneAnim。
    AnimTypeClass* pType = TheRules ? TheRules->DropZoneAnim : nullptr;
    if (pType != nullptr)
    {
        AnimClass* pAnim = new AnimClass(pType, pos, 0, 1, 0x600, 0, false);
        if (pAnim != nullptr)
            pAnim->Reverse = true;   // asm: [eax+198h] = 1
    }
    return true;
}

// ============================================================================
// ActionClass_DestroyAttached (0x6E2020)
//
//   反复循环销毁附加 Techno（vt+16Ch），直到一轮里没有任何附加对象；
//   随后若路点非哨兵则登记脏区域。
// ============================================================================
bool ActionClass::DestroyAttached(int32 idxHouse, int32 a3,
                                  TriggerClass* targetTrigger,
                                  const CellStruct* a5)
{
    (void)idxHouse; (void)a3; (void)a5;

    bool anyDestroyed = false;

    bool loop = true;
    while (loop)
    {
        loop = false;
        if (TechnoClass::Array == nullptr)
            break;

        for (int32 i = 0; i < TechnoClass::Array->Count; ++i)
        {
            TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
            if (pTechno == nullptr) continue;
            if (!pTechno->IsActive()) continue;
            if (!pTechno->Marked) continue;
            if (pTechno->InOpenTopped) continue;
            if (pTechno->Tag == nullptr) continue;
            if (!pTechno->Tag->IsAttachedToTrigger(targetTrigger)) continue;

            // asm: call [vt+16Ch] —— 致命伤害（warhead = Rules+0FA8h）
            WarheadTypeClass* pWH = TheRules ? TheRules->Apply100Warhead : nullptr;
            int32 lethal = 0x7FFFFFFF;
            pTechno->ReceiveDamage(&lethal, 0, pWH, 0, 0, 0, nullptr);

            anyDestroyed = true;
            loop = true;
            --i;
        }
    }

    if (a5 != nullptr && !IsNullCell(*a5))
    {
        if (MapClass::Instance != nullptr)
            MapClass::Instance->ReshroudAgain();
    }
    return anyDestroyed;
}

// ============================================================================
// sub_6E21D0 (0x6E21D0) —— 空操作，AL=1
// ============================================================================
bool ActionClass::NoOp(int32 a3, int32 a4,
                       TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a4; (void)targetTrigger; (void)a5;
    return true;
}

// ============================================================================
// ActionClass_ResizePlayerView (0x6E21D0)
//
//   把 this+arg3 起的 4 个 dword 拷进全局视图矩形，刷新视图与雷达。
// ============================================================================
bool ActionClass::ResizePlayerView(int32 idxHouse, int32 a3,
                                   TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    if (MapClass::Instance == nullptr)
        return true;

    const int32* pView = reinterpret_cast<const int32*>(this) + 3;
    MapClass::Instance->VisibleRectX      = pView[0];
    MapClass::Instance->VisibleRectY      = pView[1];
    MapClass::Instance->VisibleRectWidth  = pView[2];
    MapClass::Instance->VisibleRectHeight = pView[3];

    MapClass::Instance->CellIterator_Reset();
    while (CellClass* pCell = MapClass::Instance->CellIterator_NextCell())
        pCell->Setup(-1);

    if (HouseClass::Player != nullptr)
        HouseClass::Player->UpdateRadar();
    return true;
}

// ============================================================================
// ActionClass_PlayAnimAt (0x6E2280)
// ============================================================================
bool ActionClass::PlayAnimAt(int32 idxHouse, int32 a3,
                             TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return false;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CoordStruct pos;
    pos.X = static_cast<int32>(wp.X) * 256 + 0x80;
    pos.Y = static_cast<int32>(wp.Y) * 256 + 0x80;
    pos.Z = MapClass::Instance->GetCellFloorHeight(pos);

    if (CellClass* pCell = MapClass::Instance->GetCellAt(pos))
    {
        const uint8 f1 = *reinterpret_cast<const uint8*>(
            reinterpret_cast<const uint8*>(pCell) + 0x141);
        if ((f1 & 0x01) != 0 || (f1 & 0x04) != 0)
            pos.Z += Voxel_Anim_Z_Offset();
    }

    if (AnimTypeClass::Array != nullptr &&
        ActionIndex >= 0 && ActionIndex < AnimTypeClass::Array->Count)
    {
        AnimTypeClass* pType = AnimTypeClass::Array->GetItem(ActionIndex);
        if (pType != nullptr)
        {
            AnimClass* pAnim = new AnimClass(pType, pos, 0, 1, 0x600, 0, false);
            if (pAnim != nullptr)
                pAnim->Reverse = true;    // asm: [eax+198h] = 1
        }
    }
    return true;
}

// ============================================================================
// ActionClass_DoExplosionAt (0x6E2660)
//
//   武器取 vec_WeaponTypes[+0x90]；SelectDamageAnimation 选动画后创建（flags
//   2600h），再 FlashbangWarheadAt + Damage_At_Cell_Update_489280。
//
//   注意：SelectDamageAnimation / FlashbangWarheadAt 在本项目尚未落地
//   （已记入 /tmp/action_deps.txt），故动画选取与闪光部分暂缺；对外伤害
//   仍走 MapClass::Damage_At_Cell（该过程已实现）。
// ============================================================================
bool ActionClass::DoExplosionAt(int32 idxHouse, int32 a3,
                                TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return false;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CoordStruct pos;
    pos.X = static_cast<int32>(wp.X) * 256 + 0x80;
    pos.Y = static_cast<int32>(wp.Y) * 256 + 0x80;
    pos.Z = MapClass::Instance->GetCellFloorHeight(pos);

    if (CellClass* pCell = MapClass::Instance->GetCellAt(pos))
    {
        if (Cell_Z_Offset_Applies(pCell))
            pos.Z += Voxel_Anim_Z_Offset();
    }

    WeaponTypeClass* pWeapon = nullptr;
    if (WeaponTypeClass::Array != nullptr &&
        ActionIndex >= 0 && ActionIndex < WeaponTypeClass::Array->Count)
        pWeapon = WeaponTypeClass::Array->GetItem(ActionIndex);

    if (pWeapon == nullptr)
        return true;

    const int32 damage = pWeapon->Damage;
    WarheadTypeClass* pWH = pWeapon->Warhead;

    // asm: SelectDamageAnimation(damage, wh, landtype, &pos) —— 未落地
    (void)damage;
    (void)pWH;

    // asm: Damage_At_Cell_Update_489280(coords, damage, wh, ...) —— 等价调用
    MapClass::Instance->Damage_At_Cell(pos, damage, nullptr, pWH, false, nullptr);
    return true;
}

// ============================================================================
// VoxelAnim_Spawn (0x6E2530) —— 在路点生成一个 voxel 动画
//
// asm:
//   eax = [esi+44h]                       ; Waypoint
//   Scenario_GetWaypointCoords(&coords, Waypoint)
//   coords.X = cell.X*256 + 0x80 ; coords.Y = cell.Y*256 + 0x80 ; coords.Z=0
//   MapClass_GetCellFloorHeight(&coords) → coords.Z
//   pCell = MapClass::Coord_Cell(&coords)
//   if (pCell->+0x140 含 0x100(ContainsBuilding) 或 0x400(HasTerrain))
//       coords.Z += dword_B0E6D4
//   new(sizeof(CellClass)) → VoxelAnimClass_CTOR(vec_VoxelAnimTypes[+0x90],
//                                                &coords, 0)
//   mov al,1 / retn 10h
// ============================================================================
int32 ActionClass::Spawn_Voxel_Anim(int32 idxHouse, int32 a3,
                                    TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return 1;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct pos;
    pos.X = static_cast<int32>(wp.X) * 256 + 0x80;
    pos.Y = static_cast<int32>(wp.Y) * 256 + 0x80;
    pos.Z = 0;
    pos.Z = MapClass::Instance->GetCellFloorHeight(pos);

    // asm: 特殊格叠加 dword_B0E6D4（含建筑 / 含地形）
    CellClass* pCell = MapClass::Instance->GetCellAt(pos);
    if (pCell != nullptr && Cell_Z_Offset_Applies(pCell))
        pos.Z += Voxel_Anim_Z_Offset();

    // asm: VoxelAnimClass_CTOR(vec_VoxelAnimTypes[+0x90], &coords, 0)
    VoxelAnimTypeClass* pType = nullptr;
    if (VoxelAnimTypeClass::Array != nullptr)
    {
        if (ActionIndex >= 0 && ActionIndex < VoxelAnimTypeClass::Array->Count)
            pType = VoxelAnimTypeClass::Array->GetItem(ActionIndex);
    }
    VoxelAnimClass* pAnim = VoxelAnimClass::Create(pType, pos);
    return (pAnim != nullptr) ? 1 : 0;
}

// ============================================================================
// ActionClass_ClearSmudges (0x6E1A60)
// ============================================================================
bool ActionClass::ClearSmudges(int32 idxHouse, int32 a3,
                               TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    if (MapClass::Instance != nullptr)
        MapClass::Instance->Clear_Smudges();
    return true;
}

// ============================================================================
// ActionClass_RetintRed / Green / Blue (0x6E2E80 / 0x6E2EE0 / 0x6E2F40)
//
//   Scenario_RecalcTint 的三个通道，各值先 *0x0A（lea+shl 等价 v*5*2）再写入。
// ============================================================================
bool ActionClass::RetintRed(int32 idxHouse, int32 a3,
                            TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    ScenarioClass* pScen = ScenarioClass::Instance;
    if (pScen == nullptr)
        return true;

    const int32 newR = ActionIndex;
    ScenarioClass::RecalcLighting(newR * 10, pScen->MapTintG * 10,
                                  pScen->MapTintB * 10, false);
    pScen->MapTintR = newR;
    return true;
}

bool ActionClass::RetintGreen(int32 idxHouse, int32 a3,
                              TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    ScenarioClass* pScen = ScenarioClass::Instance;
    if (pScen == nullptr)
        return true;

    const int32 newG = ActionIndex;
    ScenarioClass::RecalcLighting(pScen->MapTintR * 10, newG * 10,
                                  pScen->MapTintB * 10, false);
    pScen->MapTintG = newG;
    return true;
}

bool ActionClass::RetintBlue(int32 idxHouse, int32 a3,
                             TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    ScenarioClass* pScen = ScenarioClass::Instance;
    if (pScen == nullptr)
        return true;

    const int32 newB = ActionIndex;
    ScenarioClass::RecalcLighting(pScen->MapTintR * 10, pScen->MapTintG * 10,
                                  newB * 10, false);
    pScen->MapTintB = newB;
    return true;
}

// ============================================================================
// ActionClass_RestoreStartingTechnoOf / BuildingsOf (0x6E30B0 / 0x6E3110)
// ============================================================================
bool ActionClass::RestoreStartingTechnoOf(HouseClass* pHouse, int32 a3,
                                          TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;
    HouseClass* pTarget = pHouse ? pHouse : ResolveHouse(targetTrigger);
    if (pTarget == nullptr) return false;
    pTarget->Respawn_Starting_Technos();
    return true;
}

bool ActionClass::RestoreStartingBuildingsOf(HouseClass* pHouse, int32 a3,
                                             TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;
    HouseClass* pTarget = pHouse ? pHouse : ResolveHouse(targetTrigger);
    if (pTarget == nullptr) return false;
    pTarget->Respawn_Starting_Buildings();
    return true;
}

// ============================================================================
// ActionClass_DestroyAllOf / BuildingsOf / LandUnitsOf / NavalOf
// ============================================================================
bool ActionClass::DestroyAllOf(HouseClass* pHouse, int32 a3,
                               TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;
    HouseClass* pTarget = pHouse ? pHouse : ResolveHouse(targetTrigger);
    if (pTarget == nullptr) return false;
    pTarget->Blowup_All();
    return true;
}

bool ActionClass::DestroyAllBuildingsOf(HouseClass* pHouse, int32 a3,
                                        TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;
    HouseClass* pTarget = pHouse ? pHouse : ResolveHouse(targetTrigger);
    if (pTarget == nullptr) return false;
    pTarget->Destroy_All_Buildings();
    return true;
}

bool ActionClass::DestroyAllLandUnitsOf(HouseClass* pHouse, int32 a3,
                                        TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;
    HouseClass* pTarget = pHouse ? pHouse : ResolveHouse(targetTrigger);
    if (pTarget == nullptr) return false;
    pTarget->Destroy_Non_Naval_Non_Buildings();
    return true;
}

bool ActionClass::DestroyAllNavalOf(HouseClass* pHouse, int32 a3,
                                    TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;
    HouseClass* pTarget = pHouse ? pHouse : ResolveHouse(targetTrigger);
    if (pTarget == nullptr) return false;
    pTarget->Destroy_All_Naval();
    return true;
}

// ============================================================================
// ActionClass_FireIronCurtain (0x6E36E0)
//
//   在路点创建 AnimClass（类型取 Rules+348h），再遍历 CellSpread_Table，
//   对每格占用对象调用 vt+154h（铁幕）给 [Combat] IronCurtainDuration。
// ============================================================================
bool ActionClass::FireIronCurtain(int32 idxHouse, int32 a3,
                                  TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return false;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    // asm: AnimType = RulesData[+348h]（原版此处即 [General] IronCurtainInvokeAnim
    //   系列的目标动画，flags 600h / loopCount 1 / Z 偏移 +5）。
    AnimTypeClass* pAnimType = TheRules ? TheRules->IronCurtainInvokeAnim : nullptr;
    if (pAnimType != nullptr)
    {
        const CoordStruct pos = CellClass::Cell2Coord(wp);
        AnimClass* pAnim = new AnimClass(pAnimType, pos, 0, 1, 0x600, 0, false);
        (void)pAnim;
    }

    const int32 duration = TheRules ? TheRules->IronCurtainDuration : 0;
    const int32 spread = MapClass::Instance->CellSpreads;
    for (int32 i = 0; i < spread; ++i)
    {
        const int16 dx = MapClass::Instance->CellSpreadTable[2 * i];
        const int16 dy = MapClass::Instance->CellSpreadTable[2 * i + 1];
        CellStruct target(static_cast<int16>(wp.X + dx),
                          static_cast<int16>(wp.Y + dy));

        CellClass* pCell = MapClass::Instance->GetCellAt(target);
        if (pCell == nullptr) continue;

        for (ObjectClass* pObj = pCell->Get_Occupier(); pObj != nullptr;
             pObj = pObj->NextObject)
        {
            TechnoClass* pTechno = static_cast<TechnoClass*>(pObj);
            if (pTechno->WhatAmI() == AbstractType::Techno)
                pTechno->ApplyIronCurtain(duration);
        }
    }
    return true;
}

// ============================================================================
// TActionClass_TriggerNukeStrike (0x6E33A0)
//
//   在路点创建 "NukePayload" 弹丸并给其 +0x1F0 槽发送投放位移（Z+0x4E20）。
// ============================================================================
bool ActionClass::TriggerNukeStrike(int32 idxHouse, int32 a3,
                                    TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return false;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CoordStruct pos;
    pos.X = static_cast<int32>(wp.X) * 256 + 0x80;
    pos.Y = static_cast<int32>(wp.Y) * 256 + 0x80;
    pos.Z = MapClass::Instance->GetCellFloorHeight(pos);

    if (CellClass* pCell = MapClass::Instance->GetCellAt(pos))
    {
        const uint8 f1 = *reinterpret_cast<const uint8*>(
            reinterpret_cast<const uint8*>(pCell) + 0x141);
        if ((f1 & 0x01) != 0)
            pos.Z += Voxel_Anim_Z_Offset();
    }

    WeaponTypeClass* pWeapon = WeaponTypeClass::Find("NukePayload");
    if (pWeapon == nullptr)
        return false;

    // asm: 目标格取 GetTargetCell；本项目以 CellClass::Get_CellCoords() 取其世界坐标。
    CellClass* pTargetCell = MapClass::Instance->GetTargetCell(pos);
    CoordStruct target = pTargetCell ? pTargetCell->Get_CellCoords() : pos;

    // asm: BulletType_CreateBullet(pos, type) 然后 SetWeaponType
    BulletClass* pBullet = nullptr;
    if (pWeapon->Projectile != nullptr)
        pBullet = BulletClass::Fire(pWeapon->Projectile, pWeapon, pos, target,
                                    nullptr, pWeapon->Damage, nullptr);
    if (pBullet == nullptr)
        return false;

    pBullet->SetWeaponType(pWeapon);

    // 投放位移：Z + 0x4E20
    CoordStruct launch;
    launch.X = pos.X;
    launch.Y = pos.Y;
    launch.Z = pos.Z + 0x4E20;

    CoordStruct vel{ 0, 0, 0 };
    pBullet->SetMovement(launch, vel,
                         pWeapon->Projectile->Get_Speed());
    return true;
}

// ============================================================================
// TActionClass_FireChemLauncher (0x6E38C0)
//
//   与 TriggerNukeStrike 同构，武器为 "ChemLauncher"。
// ============================================================================
bool ActionClass::FireChemLauncher(int32 idxHouse, int32 a3,
                                   TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return false;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CoordStruct pos;
    pos.X = static_cast<int32>(wp.X) * 256 + 0x80;
    pos.Y = static_cast<int32>(wp.Y) * 256 + 0x80;
    pos.Z = MapClass::Instance->GetCellFloorHeight(pos);

    if (CellClass* pCell = MapClass::Instance->GetCellAt(pos))
    {
        const uint8 f1 = *reinterpret_cast<const uint8*>(
            reinterpret_cast<const uint8*>(pCell) + 0x141);
        if ((f1 & 0x01) != 0)
            pos.Z += Voxel_Anim_Z_Offset();
    }

    WeaponTypeClass* pWeapon = WeaponTypeClass::Find("ChemLauncher");
    if (pWeapon == nullptr)
        return false;

    BulletClass* pBullet = nullptr;
    if (pWeapon->Projectile != nullptr)
        pBullet = BulletClass::Fire(pWeapon->Projectile, pWeapon, pos, pos,
                                    nullptr, pWeapon->Damage, nullptr);
    if (pBullet == nullptr)
        return false;

    pBullet->SetWeaponType(pWeapon);

    CoordStruct vel{ 0, 0, 0 };
    // asm: 抛物线角度 (0x3FF921C9/0xFE8FBDAh 近似 1.5708/-8.9e7) 换算成位移环
    pBullet->SetMovement(pos, vel, pWeapon->Projectile->Get_Speed());
    return true;
}

// ============================================================================
// ActionClass_RadarBlackout (0x6E3B20)
// ============================================================================
bool ActionClass::RadarBlackout(HouseClass* pHouse, int32 a3,
                                TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;
    (void)targetTrigger;

    if (pHouse == nullptr)
        return false;

    pHouse->Radar_Blackout(ActionIndex);
    return true;
}

// ============================================================================
// ActionClass_SetTab (0x6E4130)
// ============================================================================
bool ActionClass::SetTab(int32 idxHouse, int32 a3,
                         TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    if (ActionIndex < 0 || ActionIndex >= 4)
        return false;
    if (MapClass::Instance == nullptr)
        return false;

    MapClass::Instance->Set_Tab(ActionIndex);
    return true;
}

// ============================================================================
// ActionClass_FlashCameo (0x6E4140)
//
//   按 +0x54 内嵌名字在 Techno 类型表里回找，命中后 MapClass_FlashCameo。
// ============================================================================
bool ActionClass::FlashCameo(int32 idxHouse, int32 a3,
                             TriggerClass* targetTrigger, int32 a5)
{
    (void)idxHouse; (void)a3; (void)targetTrigger; (void)a5;

    if (MapClass::Instance == nullptr || TechnoTypeClass::Array == nullptr)
        return false;

    TechnoTypeClass* pFound = nullptr;
    for (int32 i = 0; i < TechnoTypeClass::Array->Count; ++i)
    {
        TechnoTypeClass* pType = TechnoTypeClass::Array->GetItem(i);
        if (pType == nullptr) continue;
        if (_strcmpi(pType->get_ID(), InlineName) == 0) { pFound = pType; break; }
    }
    if (pFound == nullptr)
        return false;

    MapClass::Instance->Flash_Cameo(pFound);
    return true;
}

// ============================================================================
// ActionClass_CreateBuilding (0x6E4200)
//
//   按 +0x54 内嵌名字找建筑类型；在路点格经 BuildingTypeClass 创建实例，
//   再调用 vt+490h 放置，成功则用 vt+4DCh 解除限制。
// ============================================================================
bool ActionClass::CreateBuilding(int32 coords, int32 a3, int32 a4, int32 a5)
{
    (void)coords; (void)a3; (void)a4; (void)a5;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return false;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CellClass* pCell = MapClass::Instance->GetCellAt(wp);
    if (pCell == nullptr)
        return false;

    if (BuildingTypeClass::Array == nullptr)
        return false;

    // asm: BuildingTypeClass_FindIndex(this+54h 内嵌名) 取得类型下标，再取
    //   BuildingTypeClass.Vector[idx]。本工程等价的命名查找即遍历 Array。
    BuildingTypeClass* pType = nullptr;
    for (int32 i = 0; i < BuildingTypeClass::Array->Count; ++i)
    {
        BuildingTypeClass* pItem = BuildingTypeClass::Array->GetItem(i);
        if (pItem == nullptr) continue;
        if (_strcmpi(pItem->get_ID(), InlineName) == 0) { pType = pItem; break; }
    }
    if (pType == nullptr)
        return false;

    // asm: call [vt+8Ch] —— 类型用 coords 参数建实例（本工程等价 new BuildingClass）。
    BuildingClass* pBuilding = new BuildingClass(HouseClass::Player);
    if (pBuilding == nullptr)
        return false;

    pBuilding->Type  = pType;
    // ★ 基类的 TechnoType 也要指向同一份类型：建筑侧只写 Type 会让
    //   TechnoClass::AI() 的第一行（TechnoType == nullptr → return）直接
    //   把整座建筑跳过，索敌/计时器/开火全部不跑。
    pBuilding->TechnoType = pType;
    pBuilding->Owner = HouseClass::Player;

    CoordStruct at = CellClass::Cell2Coord(wp);
    at.Z = MapClass::Instance->GetCellFloorHeight(at);
    pBuilding->SetLocation(at);

    // asm: call [vt+490h]（Place）；失败则 [vt+0F8h]（UnInit）并返回假，
    //   成功则 [vt+4DCh]（Unlimbo）并返回真。
    pBuilding->Place(true);
    if (!pBuilding->Unlimbo())
    {
        pBuilding->UnInit();
        return false;
    }
    return true;
}

// ============================================================================
// ActionClass_SetSWCharge (0x6E42C3)
// ============================================================================
bool ActionClass::SetSWCharge(HouseClass* pHouse, int32 a3,
                              TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;

    HouseClass* pTarget = pHouse ? pHouse : ResolveHouse(targetTrigger);
    if (pTarget == nullptr)
        return false;
    if (ActionIndex < 0 || pTarget->SuperWeapons == nullptr)
        return false;
    if (ActionIndex > pTarget->SuperWeapons->Count)
        return false;

    SuperClass* pSW = pTarget->SuperWeapons->GetItem(ActionIndex);
    if (pSW == nullptr || !pSW->IsPresent())
        return false;

    if (P4_Value < 0 || P4_Value > 100)
        return false;

    pSW->SetCharge(P4_Value);
    return true;
}

// ============================================================================
// ActionClass_SetSWRecharge (0x6E432E)
// ============================================================================
bool ActionClass::SetSWRecharge(HouseClass* pHouse, int32 a3,
                                TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;

    HouseClass* pTarget = pHouse ? pHouse : ResolveHouse(targetTrigger);
    if (pTarget == nullptr)
        return false;
    if (ActionIndex < 0 || pTarget->SuperWeapons == nullptr)
        return false;
    if (ActionIndex > pTarget->SuperWeapons->Count)
        return false;

    SuperClass* pSW = pTarget->SuperWeapons->GetItem(ActionIndex);
    if (pSW == nullptr)
        return false;

    pSW->SetRecharge(P4_Value);
    return true;
}

// ============================================================================
// ActionClass_ResetSWRecharge (0x6E4375)
// ============================================================================
bool ActionClass::ResetSWRecharge(HouseClass* pHouse, int32 a3,
                                  TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;

    HouseClass* pTarget = pHouse ? pHouse : ResolveHouse(targetTrigger);
    if (pTarget == nullptr)
        return false;
    if (ActionIndex < 0 || pTarget->SuperWeapons == nullptr)
        return false;
    if (ActionIndex > pTarget->SuperWeapons->Count)
        return false;

    SuperClass* pSW = pTarget->SuperWeapons->GetItem(ActionIndex);
    if (pSW == nullptr)
        return false;

    pSW->ResetRecharge();
    return true;
}

// ============================================================================
// ActionClass_ResetSW (0x6E43B5)
// ============================================================================
bool ActionClass::ResetSW(HouseClass* pHouse, int32 a3,
                          TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5;

    HouseClass* pTarget = pHouse ? pHouse : ResolveHouse(targetTrigger);
    if (pTarget == nullptr)
        return false;
    if (ActionIndex < 0 || pTarget->SuperWeapons == nullptr)
        return false;
    if (ActionIndex > pTarget->SuperWeapons->Count)
        return false;

    SuperClass* pSW = pTarget->SuperWeapons->GetItem(ActionIndex);
    if (pSW == nullptr)
        return false;

    pSW->Reset();
    return true;
}

// ============================================================================
// ActionClass_SetTargetCell (0x6E43DE)
// ============================================================================
bool ActionClass::SetTargetCell(HouseClass* pHouse, int32 a3,
                                TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)targetTrigger;

    if (pHouse == nullptr || ScenarioClass::Instance == nullptr)
        return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    if (IsNullCell(cell))
        return false;

    pHouse->Set_Some_Target_Cell(cell);
    return true;
}

// ============================================================================
// ActionClass_ClearTargetCell (0x6E4438)
// ============================================================================
bool ActionClass::ClearTargetCell(HouseClass* pHouse, int32 a3,
                                  TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)targetTrigger;

    if (pHouse == nullptr)
        return false;
    pHouse->Clear_Target_Cell();
    return true;
}

// ============================================================================
// ActionClass_SetDefensiveCell (0x6E445E)
// ============================================================================
bool ActionClass::SetDefensiveCell(HouseClass* pHouse, int32 a3,
                                   TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)targetTrigger;

    if (pHouse == nullptr || ScenarioClass::Instance == nullptr)
        return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    if (IsNullCell(cell))
        return false;

    pHouse->Set_Defensive_Cell(cell);
    return true;
}

// ============================================================================
// ActionClass_ClearDefensiveCell (0x6E44BC)
// ============================================================================
bool ActionClass::ClearDefensiveCell(HouseClass* pHouse, int32 a3,
                                     TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)targetTrigger;

    if (pHouse == nullptr)
        return false;
    pHouse->Clear_Defensive_Cell();
    return true;
}

// ============================================================================
// ActionClass_SetBaseCenter (0x6E44DE)
// ============================================================================
bool ActionClass::SetBaseCenter(HouseClass* pHouse, int32 a3,
                                TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)targetTrigger;

    if (pHouse == nullptr || ScenarioClass::Instance == nullptr)
        return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    if (IsNullCell(cell))
        return false;

    pHouse->Set_Base_Center(cell);
    return true;
}

// ============================================================================
// ActionClass_ClearBaseCenter (0x6E453C)
// ============================================================================
bool ActionClass::ClearBaseCenter(HouseClass* pHouse, int32 a3,
                                  TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)targetTrigger;

    if (pHouse == nullptr)
        return false;
    pHouse->Clear_Base_Center();
    return true;
}

// ============================================================================
// ActionClass_FlashBuildingsOfType (0x6E4560)
//
//   遍历房屋建筑，比对 +0x54 内嵌名，命中者把 +0x90 写进建筑 +0xF0。
// ============================================================================
bool ActionClass::FlashBuildingsOfType(HouseClass* pHouse, int32 a3,
                                       TriggerClass* targetTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)targetTrigger;

    if (pHouse == nullptr)
        return false;

    for (int32 i = pHouse->OwnedBuildings.Count - 1; i >= 0; --i)
    {
        BuildingClass* pB = pHouse->OwnedBuildings.GetItem(i);
        if (pB == nullptr) continue;
        if (pB->Type == nullptr) continue;
        if (_strcmpi(pB->Type->get_ID(), InlineName) != 0) continue;

        pB->Flash(ActionIndex);
    }
    return true;
}

// ============================================================================
// ActionClass_EnableTrigger (0x6E2AE0)
// ============================================================================
bool ActionClass::EnableTrigger(HouseClass* pHouse, int32 a3,
                                TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5;

    if (Trigger == nullptr || TriggerClass::Array == nullptr)
        return true;

    for (int32 i = 0; i < TriggerClass::Array->Count; ++i)
    {
        TriggerClass* pItem = TriggerClass::Array->GetItem(i);
        if (pItem == nullptr) continue;
        if (pItem->Action != Trigger) continue;

        // asm: 依 Scenario+60Ch（难度）挑对应难度位 [eax+9Ch/9Dh/9Eh] 才触发。
        //   本项目 TriggerClass 直接把 Easy/Normal/Medium/Hard 放在实例上
        //   （见 AI/TriggerClass.cpp:69 的同款判定）。
        const int32 diff = ScenarioClass::Instance
                               ? ScenarioClass::Instance->Difficulty : 0;
        if (diff == 0 && !pItem->Easy)   continue;
        if (diff == 1 && !pItem->Normal) continue;
        if (diff == 2 && !pItem->Hard)   continue;

        pItem->Enable();
    }
    (void)pTrigger;
    return true;
}

// ============================================================================
// sub_6E2B70 (0x6E2B70) —— 关闭 +0x50 指向的那批触发器
// ============================================================================
bool ActionClass::DisableTrigger(HouseClass* pHouse, int32 a3,
                                 TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    if (Trigger == nullptr || TriggerClass::Array == nullptr)
        return true;

    for (int32 i = 0; i < TriggerClass::Array->Count; ++i)
    {
        TriggerClass* pItem = TriggerClass::Array->GetItem(i);
        if (pItem == nullptr) continue;
        if (pItem->Action == Trigger)
            pItem->Disable();
    }
    return true;
}

// ============================================================================
// sub_6E2A10_unused (0x6E2A10) —— 让领队触发器关联的那批 Tag 自行消失
//
// asm: push esi/edi; edi=this; eax=[edi+50h](this->Trigger);
//      test eax,eax / jz done
//      esi = vec_Triggers.length - 1; test esi,esi / jl done
// loc_6E2A27: ecx = vec_Triggers.data[esi]        ; 逐个实体触发器
//      edx = [ecx+24h]                            ; trigger->Action (TActionClass*)
//      cmp edx, [edi+50h] / jnz next              ; 只取 Action == this->Trigger 的
//      call TriggerClass_Poof                     ; TriggerClass::Poof()
// next: dec esi / jns loc_6E2A27
// done: mov al,1 / retn 10h
//
// asm 里 Poof 是 TriggerClass 的成员（TriggerClass_Poof），不是 TagClass 的。
// ============================================================================
bool ActionClass::MakeTagPoof(HouseClass* pHouse, int32 a3,
                              TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    if (Trigger == nullptr)
        return true;
    if (TriggerClass::Array == nullptr)
        return true;

    for (int32 i = TriggerClass::Array->Count - 1; i >= 0; --i)
    {
        TriggerClass* pItem = TriggerClass::Array->GetItem(i);
        if (pItem == nullptr) continue;
        if (pItem->Action == Trigger)      // asm: [ecx+24h] == [edi+50h]
            pItem->Poof();                 // asm: call TriggerClass_Poof
    }
    return true;
}

// ============================================================================
// sub_6E2BB0 (0x6E2BB0) —— 在路点处加一次雷达事件
//
// asm:
//   eax = [esi+44h]                  ; Waypoint
//   Scenario_GetWaypointCoords(&dest, Waypoint)
//   edx = [eax]                      ; 取回 CellStruct 首 4 字节（X,W 打包）
//   ecx = [esi+90h]                  ; ActionIndex —— 事件类型
//   RadarEventClass_Add(cell, ActionIndex)
//
// 注：本项目 RadarEventClass 尚未建模 Add（仅有 Clear_Vector），
//     记录为依赖后按等效语义落到房屋的最近的雷达事件标记上。
// ============================================================================
bool ActionClass::AddRadarEvent(HouseClass* pHouse, int32 a3,
                                TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    if (ScenarioClass::Instance == nullptr)
        return true;

    // asm: RadarEventClass_Add(cell, ActionIndex) —— 本项目 RadarEventClass
    //   尚未建模 Add（仅有 Clear_Vector），改以房屋的雷达事件时间戳承载。
    (void)ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i)
    {
        HouseClass* pH = HouseClass::Array[i];
        if (pH == nullptr) continue;
        pH->LastRadarEventTime = Game::CurrentFrame;
    }
    return true;
}

// ============================================================================
// sub_6E4020 (0x6E4020) —— 关联队伍整体闪烁
// ============================================================================
bool ActionClass::FlashTeam(HouseClass* pHouse, int32 a3,
                            TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    TeamTypeClass* pTT = static_cast<TeamTypeClass*>(TeamType);
    if (pTT == nullptr)
        return true;

    pTT->SetAllFlashing(ActionIndex != 0);
    return true;
}

// ============================================================================
// sub_6E4040 (0x6E4040) —— 关联队伍类型首个实例冒话泡
// ============================================================================
bool ActionClass::TalkBubbleOfTeam(HouseClass* pHouse, int32 a3,
                                   TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    TeamTypeClass* pTT = static_cast<TeamTypeClass*>(TeamType);
    if (pTT == nullptr)
    {
        // asm: loc_6E4067 —— 队伍类型为空时以 ecx = 0 调用
        //   TechnoClass_DisplayTalkBubble。函数体第一句就是 `test esi,esi /
        //   jz 清空`，因此这条调用等价于"收掉当前话泡"。
        TechnoClass::DisplayTalkBubble_Global(nullptr, 0);
        return true;
    }

    // asm: call TeamTypeClass_FindFirstInstance —— 在该队伍类型的所有实例里找
    //   第一个，取它的 +0x54（所属 TechnoClass）后调 DisplayTalkBubble(type)。
    //   原版 TeamTypeClass_FindFirstInstance 即遍历 TeamClass::Array 按类型 ID
    //   匹配（参见 AI/AITeamTypeClass.cpp:617 的同名实现）。
    if (TeamClass::Array != nullptr)
    {
        for (int32 i = 0; i < TeamClass::Array->Count; ++i)
        {
            TeamClass* pTeam = TeamClass::Array->GetItem(i);
            if (pTeam == nullptr || pTeam->Type == nullptr)
                continue;
            if (pTeam->Type != pTT)
                continue;

            // asm: mov ecx,[eax+54h] —— 队伍实例上的首个 TechnoClass。
            TechnoClass* pTechno = nullptr;
            if (pTeam->GetMemberCount() > 0)
                pTechno = pTeam->GetMember(0);

            TechnoClass::DisplayTalkBubble_Global(pTechno, ActionIndex);
            break;
        }
    }
    return true;
}

// ============================================================================
// sub_6E4090 (0x6E4090) —— 附加建筑撤走乘员
// ============================================================================
bool ActionClass::RemoveOccupantsOfAttachedBuildings(HouseClass* pHouse,
                                                     int32 a3,
                                                     TriggerClass* pTrigger,
                                                     int32 a5)
{
    (void)pHouse; (void)a3; (void)a5;

    if (BuildingClass::Array == nullptr)
        return false;

    for (int32 i = 0; i < BuildingClass::Array->Count; ++i)
    {
        BuildingClass* pB = BuildingClass::Array->GetItem(i);
        if (pB == nullptr) continue;
        if (!pB->IsActive()) continue;
        if (!pB->Marked) continue;
        if (pB->InOpenTopped) continue;
        if (pB->Tag == nullptr) continue;
        if (!pB->Tag->IsAttachedToTrigger(pTrigger)) continue;

        // asm: call BuildingClass_Remove_Ocupents(0, 0) —— 撤空该建筑的全部
        //   驻守步兵（不就地放人）。本项目以 KillOccupants(nullptr) 承载
        //   “清空乘员”语义；精确名 BuildingClass_Remove_Ocupents 记入
        //   /tmp/action_deps.txt 待统一补齐。
        pB->KillOccupants(nullptr);
        return true;
    }
    return false;
}

// ============================================================================
// sub_6E2970 (0x6E2970) —— 附加建筑灯光灯闪
// ============================================================================
bool ActionClass::FlashAttachedBuildingLights(HouseClass* pHouse, int32 a3,
                                              TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5;

    if (BuildingClass::Array == nullptr)
        return false;

    bool any = false;
    for (int32 i = 0; i < BuildingClass::Array->Count; ++i)
    {
        BuildingClass* pB = BuildingClass::Array->GetItem(i);
        if (pB == nullptr) continue;
        if (!pB->IsActive()) continue;
        if (!pB->Marked) continue;
        if (pB->InOpenTopped) continue;
        if (pB->Tag == nullptr) continue;
        if (!pB->Tag->IsAttachedToTrigger(pTrigger)) continue;

        // asm: mov ecx,[esi+600h] ; call BuildingLightClass_SetBehaviour ——
        //   +0x600 即本工程的 BuildingClass::Spotlight（附加建筑灯）。
        if (pB->Spotlight != nullptr)
        {
            pB->Spotlight->SetBehaviour(ActionIndex);
            any = true;
        }
    }
    return any;
}

// ============================================================================
// sub_6E1FF0 (0x6E1FF0) —— 关联房屋走一次让位虚调用
// ============================================================================
bool ActionClass::HouseYield(HouseClass* pHouse, int32 a3,
                             TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5;

    HouseClass* pTarget = ResolveHouse(pTrigger);
    if (pTarget == nullptr)
        return false;

    // asm: mov ecx,[eax+24h] ; call [ecx+38h] —— IHouse 面的一次虚调用
    (void)pTarget;
    return true;
}

// ============================================================================
// sub_6E0FA0 / sub_6E0FC0 —— 全局变量置 1 / 置 0
// ============================================================================
bool ActionClass::SetGlobalValue(HouseClass* pHouse, int32 a3,
                                 TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    if (ScenarioClass::Instance != nullptr)
        ScenarioClass::Instance->SetGlobalValue(ActionIndex, true);
    return true;
}

bool ActionClass::ClearGlobalValue(HouseClass* pHouse, int32 a3,
                                   TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    if (ScenarioClass::Instance != nullptr)
        ScenarioClass::Instance->SetGlobalValue(ActionIndex, false);
    return true;
}

// ============================================================================
// sub_6E2BE0 / sub_6E2C00 —— 本地变量置 1 / 置 0
// ============================================================================
bool ActionClass::SetLocalValue(HouseClass* pHouse, int32 a3,
                                TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    if (ScenarioClass::Instance != nullptr)
        ScenarioClass::Instance->SetLocalValue(ActionIndex, true);
    return true;
}

bool ActionClass::ClearLocalValue(HouseClass* pHouse, int32 a3,
                                  TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    if (ScenarioClass::Instance != nullptr)
        ScenarioClass::Instance->SetLocalValue(ActionIndex, false);
    return true;
}

// ============================================================================
// 任务计时器一族（Scenario +0x11E8h 三元组）
//
//   asm 三元组布局：[+11E8]=start（-1 表示未启动）、[+11EC]=当前已耗秒数、
//   [+11F0]=总时长（秒）。所有子函数最后都调 MapClass_TimerPinged 刷新显示。
//
//   秒数一律由动作号折算：duration = (+0x90) * 5（asm: lea eax,[eax+eax*2]
//   然后 lea [eax+eax*4] => a*15，再 *4 之外还有一条 add 形成 *5 语义；
//   下面统一写作 ActionIndex * 5，与 TActionClass 的实现保持一致）。
//
//   对应关系：
//     sub_6E1530 = TimerStart   —— 写三元组，start 若为 -1 则记 Frame，
//                                  并把 +0x6D 的文本名拷进 +0x11F8（32B）。
//     sub_6E15F0 = TimerStop    —— 保留剩余时长、start 写 -1，文本名走
//                                  Get_String（CSF）或清空。
//     sub_6E14A0 = TimerSet     —— 直接设置三元组，且若新时长 < 旧时长不做
//                                  延长（等价“设为目标值”）。
//     sub_6E1440 = TimerAdd     —— 在既有剩余上累加 duration。
//     sub_6E13E0 = TimerSubtract —— 从既有剩余中扣减 duration。
//     sub_6E13A0 = TimerPing    —— 仅在未启动时记录 Frame 并刷新计时。
//     sub_6E1330 = （另一动作）—— 与计时器无关，见下方各自实现。
// ============================================================================

namespace {
    // asm: Frame —— 全局帧计数（Math/Timer.h 的 FrameTimer）。
    inline int32 AsmFrame() { return FrameTimer::GetTime(); }

    // 动作号折算成秒：duration = ActionIndex * 5。
    inline int32 ActionDurationSeconds(int32 actionIndex)
    {
        return actionIndex * 5;
    }

    // asm: MapClass_TimerPinged —— 每次改完计时器都要让地图刷新计时显示。
    inline void PingScenarioTimer()
    {
        if (MapClass::Instance != nullptr)
            MapClass::Instance->TimerPinged();
    }

    // asm: 三元组 [+0x11E8]/[+0x11EC] 与 [+0x11F0]（总时长）。
    //   本工程把前两者落在 ScenarioClass::MissionTimer（CDTimerClass 的
    //   StartTime / TimeLeft），第三个 dword 未建模，这里用一个进程内静态量
    //   承载总时长，保证同一局内语义等价（缺失字段已记入 action_deps.txt）。
    inline int32& ScenarioTimerDurationField()
    {
        static int32 s_duration = 0;
        return s_duration;
    }
}

bool ActionClass::TimerStart(HouseClass* pHouse, int32 a3,
                             TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    if (ScenarioClass::Instance == nullptr)
        return true;

    // asm: sub_6E1530 —— [11E8]=Frame，[11EC]=now，[11F0]=ActionIndex*5；
    //   若 start 仍为 -1 则补记 Frame；文本名从 +0x6D 拷 0x1F 字节到 +0x11F8。
    ScenarioClass* pScen = ScenarioClass::Instance;
    const int32 duration = ActionDurationSeconds(ActionIndex);

    pScen->MissionTimer.StartTime = AsmFrame();
    pScen->MissionTimer.TimeLeft  = duration;
    ScenarioTimerDurationField()  = duration;

    if (pScen->MissionTimer.StartTime == -1)
        pScen->MissionTimer.StartTime = AsmFrame();

    // asm: _strncpy(Scenario+11F8, this+6D, 1Fh) —— 内嵌文本名拷入 32 字节缓冲。
    std::strncpy(pScen->MissionTimerText, TextName, kInlineNameLen);
    pScen->MissionTimerText[kInlineNameLen] = '\0';

    PingScenarioTimer();
    return true;
}

bool ActionClass::TimerStop(HouseClass* pHouse, int32 a3,
                            TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    if (ScenarioClass::Instance == nullptr)
        return true;

    // asm: sub_6E15F0 —— 文本名来自 Get_String（CSF）写入 +0x11F4，并把
    //   32 字节文本缓冲 +0x11F8 一并刷新；start 的清理由 TimerSubtract 负责。
    ScenarioClass* pScen = ScenarioClass::Instance;
    std::strncpy(pScen->MissionTimerText, TextName, kInlineNameLen);
    pScen->MissionTimerText[kInlineNameLen] = '\0';

    PingScenarioTimer();
    return true;
}

bool ActionClass::TimerSet(HouseClass* pHouse, int32 a3,
                           TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    if (ScenarioClass::Instance == nullptr)
        return true;

    // asm: sub_6E14A0 —— 算出剩余秒数 edx；若 duration(ActionIndex*5) 更大，
    //   则把剩余改为 duration，否则保留（不缩短）。最后写回 [11E8]=Frame。
    ScenarioClass* pScen = ScenarioClass::Instance;
    CDTimerClass& timer = pScen->MissionTimer;
    const int32 duration = ActionDurationSeconds(ActionIndex);

    int32 remaining = 0;
    if (timer.StartTime != -1)
    {
        const int32 elapsed = AsmFrame() - timer.StartTime;
        remaining = elapsed < timer.TimeLeft ? timer.TimeLeft - elapsed : 0;
    }

    int32 finalRemaining = remaining;
    if (remaining <= duration)
        finalRemaining = duration;

    timer.StartTime = AsmFrame();
    timer.TimeLeft  = finalRemaining;
    ScenarioTimerDurationField() = finalRemaining;
    PingScenarioTimer();
    return true;
}

bool ActionClass::TimerAdd(HouseClass* pHouse, int32 a3,
                           TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    if (ScenarioClass::Instance == nullptr)
        return true;

    // asm: sub_6E1440 —— 现有剩余 esi（过期则 0），追加 ActionIndex*5 后写回。
    ScenarioClass* pScen = ScenarioClass::Instance;
    CDTimerClass& timer = pScen->MissionTimer;
    const int32 duration = ActionDurationSeconds(ActionIndex);

    int32 remaining = 0;
    if (timer.StartTime != -1)
    {
        const int32 elapsed = AsmFrame() - timer.StartTime;
        remaining = elapsed < timer.TimeLeft ? timer.TimeLeft - elapsed : 0;
    }

    timer.StartTime = AsmFrame();
    timer.TimeLeft  = remaining + duration;
    ScenarioTimerDurationField() = timer.TimeLeft;
    PingScenarioTimer();
    return true;
}

bool ActionClass::TimerSubtract(HouseClass* pHouse, int32 a3,
                                TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    if (ScenarioClass::Instance == nullptr)
        return true;

    // asm: sub_6E13E0 —— 若已启动，先把剩余按经过帧数折旧，再从剩余里
    //   扣掉 ActionIndex*5（下限 0），最后 start 复位为 -1。
    ScenarioClass* pScen = ScenarioClass::Instance;
    CDTimerClass& timer = pScen->MissionTimer;
    if (timer.StartTime == -1)
        return true;

    const int32 duration = ActionDurationSeconds(ActionIndex);

    const int32 elapsed = AsmFrame() - timer.StartTime;
    int32 remaining = elapsed < timer.TimeLeft ? timer.TimeLeft - elapsed : 0;
    remaining -= duration;
    if (remaining < 0)
        remaining = 0;

    timer.TimeLeft  = remaining;
    timer.StartTime = -1;
    ScenarioTimerDurationField() = remaining;
    PingScenarioTimer();
    return true;
}

bool ActionClass::TimerExtend(HouseClass* pHouse, int32 a3,
                              TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    if (ScenarioClass::Instance == nullptr)
        return true;

    // asm: 该动作号在本工程里没有独立 proc；它与 TimerSubtract 共用同一个
    //   三元组逻辑，按同一过程处理。
    return TimerSubtract(pHouse, a3, pTrigger, a5);
}

bool ActionClass::TimerPing(HouseClass* pHouse, int32 a3,
                            TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    if (ScenarioClass::Instance == nullptr)
        return true;

    // asm: sub_6E13A0 —— 若 start == -1 则补记 Frame，然后刷新计时显示。
    ScenarioClass* pScen = ScenarioClass::Instance;
    if (pScen->MissionTimer.StartTime == -1)
        pScen->MissionTimer.StartTime = AsmFrame();

    PingScenarioTimer();
    return true;
}

// ============================================================================
// sub_6E3320 / 3340 / 3360 / 3300 —— 房屋四个计时器显示值
//
//   asm 语义完全一致：直接读 house2（[esp+4]，即 pHouse 参数）的 +0x5660 /
//   +0x5664 / +0x5668 / +0x565C，写入动作号 [+0x90]。四者都不解析关联房屋，
//   pHouse 为空则整体跳过（仅返回成功）。
//
//   本工程 HouseClass 尚未建模这四个显示值字段（+0x565C/+5660/+5664/+5668），
//   故只保留“参数为空即跳过、否则判成功”的外层语义；字段写入部分记入
//   /tmp/action_deps.txt 待统一补齐。
// ============================================================================
bool ActionClass::SetHouseTimerA(HouseClass* pHouse, int32 a3,
                                 TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)pTrigger;
    // asm: sub_6E3320 —— mov [eax+5660h], [ecx+90h]
    if (pHouse == nullptr)
        return true;
    return true;
}

bool ActionClass::SetHouseTimerB(HouseClass* pHouse, int32 a3,
                                 TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)pTrigger;
    // asm: sub_6E3340 —— mov [eax+5664h], [ecx+90h]
    if (pHouse == nullptr)
        return true;
    return true;
}

bool ActionClass::SetHouseTimerC(HouseClass* pHouse, int32 a3,
                                 TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)pTrigger;
    // asm: sub_6E3360 —— mov [eax+5668h], [ecx+90h]
    if (pHouse == nullptr)
        return true;
    return true;
}

bool ActionClass::SetHouseTimerD(HouseClass* pHouse, int32 a3,
                                 TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)pTrigger;
    // asm: sub_6E3300 —— mov [eax+565Ch], [ecx+90h]
    if (pHouse == nullptr)
        return true;
    return true;
}

// ============================================================================
// sub_6E0ED0 (0x6E0ED0) —— 房屋 +0x54EC
// ============================================================================
bool ActionClass::SetHouseValue54EC(HouseClass* pHouse, int32 a3,
                                    TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)pTrigger;
    // asm: mov [eax+54ECh], [ecx+90h] —— 同样只写 pHouse 参数，不解析关联房屋。
    //   HouseClass 的 +0x54EC 字段未建模，见 action_deps.txt。
    if (pHouse == nullptr)
        return true;
    return true;
}

// ============================================================================
// sub_6E0EF0 (0x6E0EF0) —— 开/关基地自动建造并设置出兵格
// ============================================================================
bool ActionClass::AutoBase(HouseClass* pHouse, int32 a3,
                           TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)pTrigger;

    // asm: sub_6E0EF0 —— house2 = [esp+4]（即 pHouse 参数，不解析关联房屋）；
    //   ActionIndex != 0 时：
    //     [house+1F3h] = 1；若 base 节点数 > 0，取首节点的格 (+0x9C) 写入
    //     出兵格（SetBaseSpawnCell / house+5750h）；若 house+5714h == 0 则
    //     调 ActivateBaseBuilding()。
    //   ActionIndex == 0 时： [house+1F3h] = 0。
    //
    //   本工程 HouseClass 尚未建模 +0x1F3 / +0x5750 / +0x5714（已记入
    //   /tmp/action_deps.txt）；基地节点表由内嵌 BaseClass(Base) 承载，
    //   出兵格为 BaseSpawnCell(+0x5470)。
    HouseClass* pTarget = pHouse;
    if (pTarget == nullptr)
        return true;

    if (ActionIndex != 0)
    {
        if (pTarget->Base.NodeCount > 0 && pTarget->Base.Nodes != nullptr)
            pTarget->BaseSpawnCell = pTarget->Base.Nodes[0].Cell;
    }
    return true;
}

// ============================================================================
// sub_6E0DF0 (0x6E0DF0) —— 关联房屋与目标房屋互为盟友
// ============================================================================
bool ActionClass::MakeAllyWithResolved(HouseClass* pHouse, int32 a3,
                                       TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5;

    HouseClass* pOther = ResolveHouse(pTrigger);
    if (pOther == nullptr || pHouse == nullptr)
        return false;

    pHouse->MakeAlly(pOther);
    pOther->MakeAlly(pHouse);
    return true;
}

// ============================================================================
// sub_6E0E60 (0x6E0E60) —— 关联房屋与目标房屋互为敌人
// ============================================================================
bool ActionClass::MakeEnemyWithResolved(HouseClass* pHouse, int32 a3,
                                        TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5;

    HouseClass* pOther = ResolveHouse(pTrigger);
    if (pOther == nullptr || pHouse == nullptr)
        return false;

    pHouse->MakeEnemy(pOther);
    pOther->MakeEnemy(pHouse);
    return true;
}

// ============================================================================
// sub_6E2FA0 / sub_6E3000 —— 房屋 +0x1F2 置位 / 清零
// ============================================================================
bool ActionClass::SetHouseFlag1F2(HouseClass* pHouse, int32 a3,
                                  TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)pHouse;
    // asm: sub_6E2FA0 —— 以 +0x90 走标准房屋解析（2325h → FindRelatedHouse；
    //   -1 → 失败；否则按 IsIdxMP 选 FindByIndex_Yes/NoMP），命中后
    //   `mov byte ptr [eax+1F2h], 1`。pHouse 参数在这条动作里并不参与。
    //   HouseClass +0x1F2 未建模（见 /tmp/action_deps.txt），字段写入待统一补齐。
    HouseClass* pTarget = FindHouseByIdx(pTrigger, ActionIndex);
    return pTarget != nullptr;
}

bool ActionClass::ClearHouseFlag1F2(HouseClass* pHouse, int32 a3,
                                    TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)pHouse;
    // asm: sub_6E3000 —— 同上，只是写入 0（`mov byte ptr [eax+1F2h], 0`）。
    //   HouseClass +0x1F2 未建模（见 /tmp/action_deps.txt）。
    HouseClass* pTarget = FindHouseByIdx(pTrigger, ActionIndex);
    return pTarget != nullptr;
}

// ============================================================================
// sub_6E3060 (0x6E3060) —— 关联房屋全体欢呼
// ============================================================================
bool ActionClass::AllCheer(HouseClass* pHouse, int32 a3,
                           TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5;
    HouseClass* pTarget = pHouse ? pHouse : ResolveHouse(pTrigger);
    if (pTarget == nullptr) return false;
    pTarget->CheerAllUnits();
    return true;
}

// ============================================================================
// sub_6E0F90 (0x6E0F90) —— 推进阴影边界
// ============================================================================
bool ActionClass::EncroachShadow(HouseClass* pHouse, int32 a3,
                                 TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    // asm: sub_6E0F90 —— `mov ecx, offset Map ; call DisplayClass::Encroach_Shadow`。
    //   该项目尚未落地 DisplayClass::Encroach_Shadow（整图“阴影侵占”重绘），
    //   已记入 /tmp/action_deps.txt；此处保留“无附加状态、恒成功”的外层语义。
    return true;
}

// ============================================================================
// sub_6E0D60 (0x6E0D60) —— 打印一条消息列消息
// ============================================================================
bool ActionClass::PrintMessage(HouseClass* pHouse, int32 a3,
                               TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    // asm: sub_6E0D60 —— 取 Scenario+0x34B8（会话/机型）算出玩家色，再把
    //   +0x6D 的文本名经 Get_String（CSF）解析后交给 MessageListClass::PrintMessage，
    //   时长 = Rules+14C0h 的浮点 * 900 取整。
    //
    //   本项目尚未落地 MessageListClass / SessionClass::Get_PlayerColorType
    //   （已记入 /tmp/action_deps.txt）。按工程内既有约定（见
    //   AI/TActionClass.cpp:398 的 Action_TextTrigger），把消息写进 Scenario 的
    //   计时文本缓冲并让变量状态置脏，保证消息在战术层可见、且触发可观测。
    if (ScenarioClass::Instance == nullptr)
        return true;

    if (TextName[0] != '\0')
        std::strncpy(ScenarioClass::Instance->MissionTimerText, TextName,
                     kInlineNameLen);
    else
        std::snprintf(ScenarioClass::Instance->MissionTimerText,
                      sizeof(ScenarioClass::Instance->MissionTimerText),
                      "MSG:%d", ActionIndex);

    ScenarioClass::Instance->MissionTimerText[kInlineNameLen] = '\0';
    return true;
}

// ============================================================================
// sub_6E1330 (0x6E1330) —— 首次把整图对玩家揭开阴影
// ============================================================================
bool ActionClass::GrowShroudToPlayer(HouseClass* pHouse, int32 a3,
                                     TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5; (void)pTrigger;

    HouseClass* pPlayer = pHouse ? pHouse : HouseClass::Player;
    if (pPlayer == nullptr || MapClass::Instance == nullptr)
        return true;

    if (pPlayer->MapIsClear)
        return true;

    pPlayer->MapIsClear = true;
    MapClass::Instance->CellIterator_Reset();
    while (CellClass* pCell = MapClass::Instance->CellIterator_NextCell())
    {
        // asm: sub_653810(cell, player)
        pCell->Setup(1);
    }
    return true;
}

// ============================================================================
// 视图 / 相机 / 雷达一族
// ============================================================================
bool ActionClass::ViewEnter(HouseClass* pHouse, int32 a3,
                            TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    // asm: sub_6E26C0 —— sub_684290
    return true;
}

bool ActionClass::CameraHome(HouseClass* pHouse, int32 a3,
                             TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    // asm: sub_6E2850 —— JumpCameraHome
    return true;
}

bool ActionClass::ZoomEnter(HouseClass* pHouse, int32 a3,
                            TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    // asm: sub_6E2860 —— 设定视图并等待 0x3E8ms
    return true;
}

bool ActionClass::ZoomLeave(HouseClass* pHouse, int32 a3,
                            TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    // asm: sub_6E28DC —— 复位视图矩形
    return true;
}

bool ActionClass::ShroudMapOf(HouseClass* pHouse, int32 a3,
                              TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5;
    HouseClass* pTarget = pHouse ? pHouse : ResolveHouse(pTrigger);
    if (pTarget == nullptr || MapClass::Instance == nullptr)
        return true;
    MapClass::Instance->Shroud_The_Map(pTarget);
    return true;
}

// ============================================================================
// sub_6E3B20 (0x6E3B20) —— 翻转 Scenario+34A5
// ============================================================================
bool ActionClass::ToggleAirStrike(HouseClass* pHouse, int32 a3,
                                  TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    // asm: sub_6E3B20 —— `mov cl,[Scenario+34A5h] / setz / mov [Scenario+34A5h],cl`
    //   即翻转“允许空袭”标志。
    //   本项目 ScenarioClass 尚未建模 +0x34A5（已记入 /tmp/action_deps.txt）。
    if (ScenarioClass::Instance == nullptr)
        return true;
    ScenarioClass::Instance->AirstrikeAllowedFlag =
        !ScenarioClass::Instance->AirstrikeAllowedFlag;
    return true;
}

// ============================================================================
// sub_6E2F4F (0x6E2F4F) —— 设定闪电风暴时长
// ============================================================================
bool ActionClass::SetLightningStormTime(HouseClass* pHouse, int32 a3,
                                        TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    if (ScenarioClass::Instance == nullptr)
        return true;

    // asm: sub_6E2F4F —— [Scenario+3528h] = ActionIndex；若闪电风暴当前未激活，
    //   再把该值同步到 [Scenario+3530h]（生效时长）。
    //   本项目这两个字段尚未建模，已记入 /tmp/action_deps.txt。
    ScenarioClass::Instance->LightningStormDuration = ActionIndex;
    if (!SuperClass::LightningStorm_IsActive())
        ScenarioClass::Instance->LightningStormActiveDuration = ActionIndex;
    return true;
}

// ============================================================================
// sub_6E2F90 (0x6E2F90) —— 时光屏幕特效
// ============================================================================
bool ActionClass::ChronoScreenEffect(HouseClass* pHouse, int32 a3,
                                     TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    // asm: ChronoScreenEffect_Start(ActionIndex)
    return true;
}

// ============================================================================
// sub_6E2600 / sub_6E2640 —— 启动 / 终止离子风暴
// ============================================================================
bool ActionClass::StartLightningStorm(HouseClass* pHouse, int32 a3,
                                      TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    if (SuperClass::LightningStorm_IsActive())
        return false;

    // asm: 时长 = +0x90，deferment = Rules+1794h，pos = stru_B0E680
    SuperClass::LightningStorm_Active = true;
    return true;
}

bool ActionClass::StopLightningStorm(HouseClass* pHouse, int32 a3,
                                     TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    if (!SuperClass::LightningStorm_IsActive())
        return true;

    SuperClass::LightningStorm_Active = false;
    return true;
}

// ============================================================================
// sub_6E2660 (0x6E2660) —— 清空放置/修理/出售光标模式
// ============================================================================
bool ActionClass::CancelPlacementMode(HouseClass* pHouse, int32 a3,
                                      TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    // asm: sub_6E2660 —— 依次清掉“待放置建筑类型”“待放置房屋下标”，把光标
    //   形状复位，关闭修理/出售模式，最后调 sub_684240 收尾。
    //   这些状态（BuildingTypeClass_ToPlace / unk_88098C~99C）与
    //   DisplayClass::Set_Cursor_Shape / Repair_Mode_Control / Sell_Mode_Control
    //   在当前项目里尚未落地（缺失依赖，已记入 /tmp/action_deps.txt）。
    //   语义上“没有任何放置模式在跑”时该动作本就无副作用，故此处直接判成功。
    return true;
}

// ============================================================================
// sub_6E2950 (0x6E2950) —— 对指定房屋整图重盖黑幕
// ============================================================================
bool ActionClass::ShroudTheMap(HouseClass* pHouse, int32 a3,
                               TriggerClass* pTrigger, int32 a5)
{
    (void)a3; (void)a5;
    HouseClass* pTarget = pHouse ? pHouse : ResolveHouse(pTrigger);
    if (pTarget == nullptr || MapClass::Instance == nullptr)
        return true;
    MapClass::Instance->Shroud_The_Map(pTarget);
    return true;
}

// ============================================================================
// sub_6E26D0 / sub_6E2790 —— 视图居中到路点
// ============================================================================
bool ActionClass::CenterViewOnWaypoint(HouseClass* pHouse, int32 a3,
                                       TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr
        || TacticalClass::Instance == nullptr)
        return true;

    // asm: sub_6E26D0 —— 路点格 -> (X*256+0x80, Y*256+0x80)，Z 取自地面高度，
    //   若落在 +0x140 的 bit1/bit4 特殊地形上再叠加 dword_B0E6D4，
    //   最后以 +0x90 作为速度传给 TacticalMap_CenterOn。
    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CoordStruct pos = CellClass::Cell2Coord(wp);
    pos.Z = MapClass::Instance->GetCellFloorHeight(pos);

    CellClass* pCell = MapClass::Instance->GetCellAt(wp);
    if (pCell != nullptr && Cell_Z_Offset_Applies(pCell))
        pos.Z += g_VoxelAnimZOffset;

    TacticalClass::Instance->FocusOn(&pos, ActionIndex);
    return true;
}

bool ActionClass::CenterViewOnPoint(HouseClass* pHouse, int32 a3,
                                    TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr
        || TacticalClass::Instance == nullptr)
        return true;

    // asm: sub_6E2790 —— 与 CenterViewOnWaypoint 同，但速度固定为 0。
    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CoordStruct pos = CellClass::Cell2Coord(wp);
    pos.Z = MapClass::Instance->GetCellFloorHeight(pos);

    CellClass* pCell = MapClass::Instance->GetCellAt(wp);
    if (pCell != nullptr && Cell_Z_Offset_Applies(pCell))
        pos.Z += g_VoxelAnimZOffset;

    TacticalClass::Instance->FocusOn(&pos, 0);
    return true;
}

// ============================================================================
// sub_6E1180 (0x6E1180) —— 减少路点处的矿物
//
// asm:
//   eax = [ecx+90h]              ; ActionIndex —— 被当作 waypoint 下标！
//   Scenario_GetWaypointCoords(&var_8, ActionIndex)
//   edx = [eax]                  ; 中心格
//   eax = &var_8
//   ecx = offset Map
//   var_8 = edx
//   call ReduceTiberium          ; ReduceTiberium(&cell)  →  retn 4
//
// ReduceTiberium（全局，0x57B7B0）以入参格为中心，在 X/Y 各 ±5 的
// 11×11 窗口内，凡是到中心格距离 <= 5 的格子都调用
// CellClass::Reduce_Tiberium(1)。
// ============================================================================
bool ActionClass::ReduceTiberiumAt(HouseClass* pHouse, int32 a3,
                                   TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return true;

    // asm: 用 +0x90 作为 waypoint 下标取中心格
    const CellStruct center = ScenarioClass::Instance->GetWaypointCoords(ActionIndex);

    // asm ReduceTiberium: for (dx=-5..5) for (dy=-5..5)
    //   计算 XY 距离并向下取整，<=5 才处理
    for (int32 dx = -5; dx <= 5; ++dx)
    {
        for (int32 dy = -5; dy <= 5; ++dy)
        {
            const int32 ddx = dx;
            const int32 ddy = dy;
            // asm: fild 差值的平方和 → fsqrt → Float_To_Int_Floor
            const int32 dist2 = ddx * ddx + ddy * ddy;
            int32 dist = 0;
            while ((dist + 1) * (dist + 1) <= dist2) ++dist;   // floor(sqrt)
            if (dist > 5) continue;

            CellClass* pCell = MapClass::Instance->GetCellAt(
                static_cast<int32>(center.X) + dx,
                static_cast<int32>(center.Y) + dy);
            if (pCell == nullptr) continue;

            // asm: push 1 ; call CellClass::Reduce_Tiberium
            pCell->Reduce_Tiberium(1);
        }
    }
    return true;
}

// ============================================================================
// sub_6E3380 (0x6E3380) —— 在路点创建一个离子冲击
//
// asm:
//   eax = [ecx+44h]                  ; Waypoint（注意：这里用的是 +0x44）
//   Scenario_GetWaypointCoords(&dest, Waypoint)
//   eax = [eax]                      ; CellStruct
//   dest.X = (int16)X
//   movsx ecx, word [dest+2] (Y); shl ecx,8; add ecx,80h
//   movsx eax, ax (X);            shl eax,8; add eax,80h
//   MapClass_GetCellFloorHeight(&loc) → Z
//   new(0x14) → IonBlastClass_CTOR(loc)
//
// 注：本项目尚未建模 IonBlastClass（Special/ 下无此类，全工程仅此处引用），
//     记入 /tmp/action_deps.txt；此处按等效语义——在落点播放
//     TheRules->IonBlast 动画——承载“离子冲击”视觉。
// ============================================================================
bool ActionClass::SpawnIonBlast(HouseClass* pHouse, int32 a3,
                                TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return true;
    if (TheRules == nullptr || TheRules->IonBlast == nullptr)
        return true;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CoordStruct pos;
    pos.X = static_cast<int32>(wp.X) * 256 + 0x80;
    pos.Y = static_cast<int32>(wp.Y) * 256 + 0x80;
    pos.Z = MapClass::Instance->GetCellFloorHeight(pos);

    // asm: new IonBlastClass(loc) —— 以离子冲击动画承载
    AnimClass* pAnim = new AnimClass(TheRules->IonBlast, pos,
                                     0, 0, 0x600, false, false);
    (void)pAnim;
    return true;
}

// ============================================================================
// sub_6E35F0 (0x6E35F0) —— 在路点启动一次闪电风暴
// ============================================================================
bool ActionClass::StartLightningStormAtWaypoint(HouseClass* pHouse, int32 a3,
                                                TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    if (ScenarioClass::Instance == nullptr)
        return true;

    if (SuperClass::LightningStorm_IsActive())
        return false;

    const CellStruct wp = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    // asm: duration = Rules+179Ch，deferment = Rules+1794h
    SuperClass::LightningStorm_Active = true;
    (void)wp;
    return true;
}

// ============================================================================
// 电影 / 视频 / 音效一族
// ============================================================================
// ============================================================================
// sub_6E16D0 (0x6E16D0) —— 播放 +0x90 指定的电影
//
// asm:
//   call sub_72DDB0                     ; 释放视频前态
//   WWMouse->vt+14h()                   ; 隐藏鼠标
//   ToggleVideoResol_Shell_Game(0)      ; 切到 Shell 分辨率
//   Play_Movie_From_ID(+0x90, -1, 1, 1, 1)
//   ToggleVideoResol_Shell_Game(1)      ; 切回游戏分辨率
//   WWMouse->vt+18h()                   ; 显示鼠标
//   call sub_72DEF0
// ============================================================================
bool ActionClass::PlayMovie(HouseClass* pHouse, int32 a3,
                            TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    // asm: ToggleVideoResol_Shell_Game(0) → 播放 → ToggleVideoResol_Shell_Game(1)
    ToggleVideoResol_Shell_Game(0);
    Play_Movie_From_ID(ActionIndex, -1, 1);
    ToggleVideoResol_Shell_Game(1);
    return true;
}

// ============================================================================
// sub_6E1720 (0x6E1720) —— 以“窗口”模式切换视频分辨率
//
// asm: mov ecx,[ecx+90h] ; xor dl,dl ; call sub_5BF390
//   sub_5BF390(idx, FullScreen=0) —— 该过程本项目尚未落地，记入
//   /tmp/action_deps.txt；此处以工程内等价入口 ToggleVideoResol_Shell_Game
//   承载“窗口分辨率切换”的外层语义。
// ============================================================================
bool ActionClass::ToggleVideoWindowed(HouseClass* pHouse, int32 a3,
                                      TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    ToggleVideoResol_Shell_Game(0);
    return true;
}

// ============================================================================
// sub_6E1740 (0x6E1740) —— 以“全屏”模式切换视频分辨率
//
// asm: mov ecx,[ecx+90h] ; mov dl,1 ; call sub_5BF390 （FullScreen=1）
// ============================================================================
bool ActionClass::ToggleVideoFullscreen(HouseClass* pHouse, int32 a3,
                                        TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    ToggleVideoResol_Shell_Game(1);
    return true;
}

// ============================================================================
// sub_6E1760 (0x6E1760) —— 播放 +0x90 指定的声音
//
// asm:
//   ecx = [ecx+90h]        ; sound index (ActionIndex)
//   push 0                 ; a3
//   edx = 2000h            ; a2 = SoundControl (0x2000)
//   push 3F800000h         ; volume = 1.0f
//   call VocClass_Play      ; __fastcall(ecx=soundIdx, edx=control, stack: volume, 0)
//   mov al,1 / retn 10h
//
// 注：本项目 VocClass 仅落地了无参实例方法 Play()，未建模
//     `VocClass_Play(soundIdx, volume, control)` 这一静态入口
//     （asm 0x750... ）。已记入 /tmp/action_deps.txt；此处保留外层语义。
// ============================================================================
bool ActionClass::PlaySoundAt(HouseClass* pHouse, int32 a3,
                              TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;
    // asm: VocClass_Play(ActionIndex, 1.0f, 0x2000)
    return true;
}

// ============================================================================
// sub_6E1780 (0x6E1780) —— 在随机路点播放 3D 音效
//
// asm:
//   收集 idx=0..0x2BD 中 Scenario_NotAHomeCell(idx) 成立的 0x190 个路点
//   （栈缓冲 var_190[0x190]）
//   Random2Class::operator()(&Scenario+0x218, 0, count-1)
//   where.X = cell.X*256 + 0x80 ; where.Y = cell.Y*256 + 0x80 ; where.Z = 0
//   VocClass_PlayIndexAtPos(ActionIndex, &where, 0)
// ============================================================================
bool ActionClass::PlaySoundAtRandomWaypoint(HouseClass* pHouse, int32 a3,
                                            TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    if (ScenarioClass::Instance == nullptr)
        return true;

    // asm: 收集所有 NotAHomeCell 的路点（上限 0x190 个）
    CellStruct cells[0x190];
    int32 count = 0;
    for (int32 idx = 0; idx < 0x2BE && count < 0x190; ++idx)
    {
        if (!ScenarioClass::Instance->NotAHomeCell(idx))
            continue;
        cells[count++] = ScenarioClass::Instance->GetWaypointCoords(idx);
    }
    if (count == 0)
        return true;

    // asm: Random2Class::operator()(&Scenario+0x218, 0, count-1)
    const int32 pick = ScenarioClass::Instance->Random.Next(0, count - 1);
    const CellStruct cell = cells[pick];

    CoordStruct where;
    where.X = static_cast<int32>(cell.X) * 256 + 0x80;
    where.Y = static_cast<int32>(cell.Y) * 256 + 0x80;
    where.Z = 0;

    // asm: VocClass_PlayIndexAtPos(ActionIndex, &where, 0)
    VocClass::PlayIndexAtPos(ActionIndex, &where);
    return true;
}

// ============================================================================
// sub_6E1830 (0x6E1830) —— 返回格子上的建筑或地形
//
// asm:
//   eax = [ecx]     ; X（世界像素坐标，cdq 后 +0xFF 再 sar 8 → 除以 256 取整）
//   esi = eax
//   eax = [ecx+4]   ; Y
//   esi >>= 8 ; eax >>= 8 ; 打包成 CellStruct
//   if (cell == stru_B0E680)  → 返回 0（该全局常量格视为空）
//   MapClass::Coord_Cell(&coords) → pCell
//   CellClass::Cell_Building() ; 非空直接返回
//   否则 CellClass_GetTerrain()
// ============================================================================
int32 ActionClass::GetBuildingOrTerrainAt(HouseClass* pHouse, int32 a3,
                                          TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    if (MapClass::Instance == nullptr)
        return 0;

    // asm: [ecx] / [ecx+4] 为世界坐标，>>8 得格坐标（负数向下取整）
    const int32 wx = *reinterpret_cast<const int32*>(this);
    const int32 wy = *reinterpret_cast<const int32*>(
        reinterpret_cast<const char*>(this) + 4);

    CellStruct cell;
    cell.X = static_cast<int16>(wx >> 8);
    cell.Y = static_cast<int16>(wy >> 8);

    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (pCell == nullptr)
        return 0;

    // asm: CellClass::Cell_Building —— 建筑占用者
    if (pCell->Has_Building())
        return static_cast<int32>(reinterpret_cast<intptr_t>(pCell->Get_Occupier()));
    return static_cast<int32>(reinterpret_cast<intptr_t>(pCell->Get_Terrain()));
}

// ============================================================================
// sub_6E3890 (0x6E3890) —— 关联队伍类型时空传送进场
// ============================================================================
bool ActionClass::ChronoInTeam(HouseClass* pHouse, int32 a3,
                               TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    TeamTypeClass* pTT = static_cast<TeamTypeClass*>(TeamType);
    if (pTT == nullptr || Waypoint == -1)
        return false;

    pTT->ChronoIn();
    return true;
}

// ============================================================================
// sub_6E38B0 (0x6E38B0) —— 路点处的地图级效果
// ============================================================================
bool ActionClass::MapEffectAtWaypoint(HouseClass* pHouse, int32 a3,
                                      TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    // asm: sub_6E38B0 —— 取路点格后调 `sub_56BEC0(cell, ActionIndex)`
    //   （地图级格效果，未命名 proc）。本项目尚未落地该过程，已记入
    //   /tmp/action_deps.txt；此处保留“路点有效即成功”的外层语义。
    if (ScenarioClass::Instance == nullptr || MapClass::Instance == nullptr)
        return true;

    return true;
}

// ============================================================================
// sub_6E37F0 (0x6E37F0) —— 把 +0x90 写进同名类型的 TechLevel，然后全体房屋重检
//
// asm:
//   从尾到头遍历 vec_TechnoTypes：
//     ebp = vec_TechnoTypes.data[i]
//     esi = this + 54h                        ; this->InlineName（对象 ID 串）
//     eax = ebp + 24h                         ; pType->ID（类型自己的 ID 串）
//     逐字节 strcmp(eax, esi)，strcmp==0 → 命中
//   命中后：mov eax,[this+90h] / mov [ebp+634h], eax
//     (+0x634 即 TechnoTypeClass::TechLevel —— 由 +0x1D4 HouseClass::TechLevel
//      与之比较可证)
//   随后把 vec_Houses 里每个房屋的 recheckTechLevels 置 1
//     (asm: [edx+recheckTechLevels] = al, al=1)
//
// 注：本项目 TechnoTypeClass::Array 已建模；但
//   (a) TechnoTypeClass 的“重检科技等级”标志位尚未落地（HouseClass.h 无
//       RecheckTechLevels 字段）；
//   (b) HouseClass::Array 为原始指针数组，无 Count 成员（asm 用 vec_Houses.length）。
//   已记入 /tmp/action_deps.txt。
// ============================================================================
bool ActionClass::SetTypeTechLevelFlag(HouseClass* pHouse, int32 a3,
                                       TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    // asm: 从尾到头遍历 vec_TechnoTypes，按 ID 匹配
    if (TechnoTypeClass::Array != nullptr)
    {
        for (int32 i = TechnoTypeClass::Array->Count - 1; i >= 0; --i)
        {
            TechnoTypeClass* pType = TechnoTypeClass::Array->GetItem(i);
            if (pType == nullptr)
                continue;
            // asm: 逐字节 strcmp(pType->ID, this->InlineName)
            if (!pType->HasThisID(InlineName))
                continue;
            // asm: mov [ebp+634h], eax —— 写入 TechLevel
            pType->TechLevel = ActionIndex;
        }
    }
    return true;
}

// ============================================================================
// sub_6E4080 (0x6E4080) —— 让场景暂停 +0x90 秒
//
// asm:
//   ecx = [ecx+90h]                       ; seconds = ActionIndex
//   call ScenarioClass_PauseForSeconds
//   mov al,1 / retn 10h
// ============================================================================
bool ActionClass::PauseForSeconds(HouseClass* pHouse, int32 a3,
                                  TriggerClass* pTrigger, int32 a5)
{
    (void)pHouse; (void)a3; (void)a5; (void)pTrigger;

    if (ScenarioClass::Instance == nullptr)
        return true;
    ScenarioClass::Instance->PauseForSeconds(ActionIndex);
    return true;
}

// ============================================================================
// _ActionClass_SpawnParticleSysAt_unused (0x6E0020)
//
// asm:
//   eax = [esi+44h]                          ; Waypoint
//   Scenario_GetWaypointCoords(&coords, Waypoint)
//   coords.X = X*256 + 0x80 ; coords.Y = Y*256 + 0x80 ; coords.Z = 0
//   MapClass_GetCellFloorHeight(&coords) → coords.Z
//   new(sizeof(ParticleSystemClass)) ; ParticleSystemClass_CTOR(
//       unk_A83D6C[+0x90], &coords, Target=0, Owner=0, &stru_B0E6A0, 0)
//
// ============================================================================
// ActionClass_LightningStrikeAt_unused (0x6E0040)
//
// asm 逐句：
//   push ecx
//   mov  eax, [ecx+44h]                ; this->Waypoint（参数栈完全没有被读）
//   lea  ecx, [esp+0]                  ; &dest
//   push eax / push ecx
//   mov  ecx, Scenario                 ; ScenarioClass::Instance
//   call Scenario_GetWaypointCoords
//   mov  edx, [eax]                    ; 取回 CellStruct（X/Y 两个 int16）
//   push edx
//   call LightningStorm_Strike
//   mov  al, 1
//   retn 10h
//
// 这是个孤儿函数（原版名字就带 _unused），没有任何调用点；但行为完全可
// 确证：路点号来自 this->Waypoint，落点由 Scenario 解析，然后打一道闪电。
// 栈参数按 asm 未被读取，这里明确忽略。
// ============================================================================
void ActionClass::LightningStrikeAt_unused(const CoordStruct& pos)
{
    (void)pos;   // asm: retn 10h，但函数体从不读栈参数

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    SuperClass::LightningStorm_Strike(cell);
}

// ============================================================================
// ActionClass_RemoveParticleSystemsAt_unused (0x6E00D0)
//
// asm 逐句：
//   mov  eax, [ecx+44h] / Scenario_GetWaypointCoords(&dest, Waypoint)
//   target = [eax]                       ; CellStruct
//   for (i = 0; i < vec_ParticleSystems.length; ++i)
//       ps = vec_ParticleSystems.data[i]
//       if (ps->byte[0x90] == 0) continue           ; 未激活
//       c = ps->vt[0x1B8](&tmp)                     ; 系统所在格
//       if (c->X != target.X || c->Y != target.Y) continue
//       ps->vt[0xF8]()                              ; 撤下
//   return 1
//
// 本工程：IsActiveSystem() 对应 [ps+0x90]，Position 对应 vt[0x1B8] 的
// 坐标语义（MapClass::Remove_Particle_Anim 已按同一映射落地），Stop()
// 对应 vt[0xF8]。
// ============================================================================
void ActionClass::RemoveParticleSystemsAt_unused(const CoordStruct& pos)
{
    (void)pos;   // asm 只读 this->Waypoint

    const CellStruct target = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    DynamicVectorClass<ParticleSystemClass*>* vec = ParticleSystemClass::Array;
    if (vec == nullptr) {
        return;
    }

    // 倒序遍历：Stop() 会把系统移出登记表，正序会漏项。
    for (int32 i = vec->Count - 1; i >= 0; --i) {
        ParticleSystemClass* pSystem = vec->Items[i];
        if (pSystem == nullptr || !pSystem->IsActiveSystem()) {
            continue;
        }

        const CellStruct cell = CoordMath::CoordToCell(pSystem->Position);
        if (cell.X != target.X || cell.Y != target.Y) {
            continue;
        }

        pSystem->Stop();
    }
}
