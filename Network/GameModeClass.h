#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../UI/StartSpotClass.h"

// ============================================================================
// GameModeClass
//
// 多人游戏模式的状态容器。asm 结构体 sizeof = 0x30D4，共 70 个字段。
// 本文件只收录**有明确 asm 依据**的部分；其余偏移暂以 `field_XX` 占位保留，
// 待后续批次逐段核实（铁律①：宁可留白，不写错）。
//
// 权威布局节选（+0x283C 起是起始点表与串口参数，IDA 存在联合体重叠命名）：
//   +0x283C  VecStartSpot  StartSpots     起始点表（vt/data/count 三连）
//   +0x2840       --        .data
//   +0x284C       --        .count
//   +0x84    int32         field_84       WorkWithColourIdx 的固定槽基址
//
// 注：IDA 对 +0x283C/+0x2840/+0x284C 同时给出了 `Port`/`DialMethod` 等
// 联合体别名（串口设置与起始点表共用同一段存储）。asm 的
// GameMode_WorkWithColourIdx 明确按 `VecStartSpot` 语义使用这三个偏移。
// ============================================================================

// asm: VecStartSpot (sizeof=0x18)
class VecStartSpot
{
public:
    void*  vt;                  // +0x00
    StartSpotClass** data;      // +0x04
    int32  count;               // +0x08
    uint8  IsInitialized;       // +0x0C
    uint8  IsAllocated;         // +0x0D
    uint8  field_E;             // +0x0E
    uint8  field_F;             // +0x0F
    int32  length;              // +0x10
    int32  incrementAmount;     // +0x14
};

class GameModeClass
{
public:
    // asm: idxGameMode
    int32 idxGameMode;          // +0x0000
    int32 ScenIndex;            // +0x0004
    int32 Players;              // +0x0008
    int32 Credits;              // +0x000C
    int32 UnitCount;            // +0x0010
    uint8 ShortGame;            // +0x0014
    uint8 SuperWeaponsAllowed;  // +0x0015
    uint8 BuildOffAlly;         // +0x0016
    uint8 MCVRepacks;           // +0x0017
    uint8 CratesAppear;         // +0x0018
    uint8 pad_19[0x84 - 0x19];  // +0x0019 暂未核实

    int32 field_84;             // +0x0084  WorkWithColourIdx 的 8 连固定槽基址
    int32 UniqueID;             // +0x0088
    int32 SideIdx;              // +0x008C
    int32 SideExIdx;            // +0x0090
    int32 Color;                // +0x0094
    int32 ColorEx;              // +0x0098
    int32 Side;                 // +0x009C
    int32 SideEx;               // +0x00A0
    uint8 pad_A4[0x283C - 0xA4];// +0x00A4 暂未核实

    VecStartSpot StartSpots;    // +0x283C 起始点表
    uint8 pad_2854[0x30D4 - 0x2854]; // +0x2854 暂未核实

    // ========================================================================
    // asm: GameMode_WorkWithColourIdx (0x69B5F0)
    //
    //   char __thiscall GameMode_WorkWithColourIdx(GameModeClass *this, int a2)
    //
    //   判断某个颜色下标是否已被占用。两轮：
    //     1) 遍历起始点表，取每个槽的颜色：
    //          sentinel[+0x57] == -2 且 colour[+0x53] == -1  -> 视为 -2
    //          否则取 colour[+0x53]
    //        命中 a2 就返回 true
    //     2) 再扫 8 个固定槽 [+0x84 + i*4]，命中返回 true
    //   默认 false。
    //
    //   注：asm 里 `test edi,edi / jle` 决定是否进入第一轮，
    //   所以 count <= 0 时直接落到第二轮。
    // ========================================================================
    bool Work_With_Colour_Idx(int32 idx);

    // ========================================================================
    // asm: GameModeClass_LoadCallStrings (0x6977B0)
    //
    //   void __fastcall GameModeClass_LoadCallStrings(
    //       DynamicVectorClass<wchar_t*> *pVec)
    //
    //   遍历 CSF 名称表（off_83ED04 .. MenuColorLinks），把
    //   `__wcsdup(Get_String(name))` 推进 pVec。Get_String 返回空指针时跳过
    //   该项（原版同样不插入）。
    //
    //   注：dump 里看不到指针表的完整边界，这里按原版引用的 lobby call-string
    //   名单复原（铁律①：只写能确证的部分）。解析失败返回 false 表示该批未推
    //   入任何条目。
    // ========================================================================
    static bool Load_Call_Strings(DynamicVectorClass<wchar_t*>* pVec);
};
