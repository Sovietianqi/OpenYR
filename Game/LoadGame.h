#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"

class LoadGameClass;

// ============================================================================
// Game/LoadGame.h - 存档读入（原版 LoadGame 主流程里的 slave 段）
//
//   asm: LoadGame_slave1 (0x67F9F0, thiscall, IStream* pStm) -> HRESULT
//
//   LoadGame 的顺序是：Scenario_ClearClasses -> LoadGame_slave2(Scenario)
//   -> Game_LoadUI -> 重建各张面 / Set_View_Dimensions / Theater_Init /
//   RulesClass_Load -> 若干 OleLoadFromStream -> LoadGame_slave3 ->
//   LoadGame_slave1。slave1 落在最后，负责"全局槽位 + 两张对象表 + 地形
//   特征表 + 特效全局 + 箱子表"这一段（见 ExtraData0_Save 的写入侧，
//   两边字段顺序逐字节对应）。
//
//   本工程没有 COM IStream，读流用 Game/SaveGameClass.h 里的 LoadGameClass；
//   语义一一对应：任一段读失败即返回负值，全部成功返回 0。
// ============================================================================

// ----------------------------------------------------------------------------
// asm: powerup_Weights / powerup_Anims / powerupData / powerupNaval
//   四张表一一对应 19 个箱子类型（Powerup 枚举 Money..Pod），落盘尺寸分别是
//   4Ch(19×4) / 4Ch(19×4) / 98h(19×8) / 13h(19×1)，读档时整块回填。
//   Goodie_Check 用 Weights 做加权抽取、用 Anims/Data/Naval 决定实际奖励。
// ----------------------------------------------------------------------------
static constexpr int32 POWERUP_TABLE_COUNT = 19;

extern int32  Powerup_Weights[POWERUP_TABLE_COUNT];  // asm: powerup_Weights
extern int32  Powerup_Anims[POWERUP_TABLE_COUNT];    // asm: powerup_Anims
extern double Powerup_Data[POWERUP_TABLE_COUNT];     // asm: powerupData（8 字节步长）
extern bool   Powerup_Naval[POWERUP_TABLE_COUNT];    // asm: powerupNaval

// asm: dword_B07784 —— 与 Frame 一起在 Main_Loop 里做"到点才跑"的节流比较
//   （Frame > 该值才调用 sub_684290 并把它回写成当前值）。原版没有名字。
extern int32 dword_B07784;

// ----------------------------------------------------------------------------
// asm: LoadGame_slave1 —— 返回 0，或读失败时 IStream::Read 原样带回的负值。
// ----------------------------------------------------------------------------
int32 LoadGame_Slave1(LoadGameClass& Loader);

// ----------------------------------------------------------------------------
// asm: int_CurrentTheatre —— 当前已装载的剧场号；-1 表示还没和任何剧场对上。
// ----------------------------------------------------------------------------
class MapClass;
class IStream;
extern int32 int_CurrentTheatre;

// ----------------------------------------------------------------------------
// asm: LoadGame_slave3 (0x5BE490) —— 地图 + 显示层这一段。
//
//   旧符号名是 LoadGame_slave3；这里带地址后缀是为了和
//   Helpers/AsmComplex14.cpp 里那份尚未删除的旧定义区分开（同
//   LoadGame_slave1_67F9F0 的命名惯例）。语义与 asm 逐句对应，见 .cpp。
// ----------------------------------------------------------------------------
HRESULT LoadGame_slave3_5BE490(MapClass* pMap, IStream* pStm);
