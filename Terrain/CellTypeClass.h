#pragma once

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <Math/CoordStruct.h>

// ============================================================================
// Terrain/CellTypeClass.h
//
// 原版单元 CellTypeClass —— 也就是 .data 里的全局单例 `path_WTF`
// （`pathfinderWtf` 结构，sizeof = 0xBC）。它是"分层寻路"的引擎对象：
//
//   +0x28  dword  搜索代号（每轮 Reset 加一，用来给 per-cell 访问表打标记）
//   +0x38  byte   本轮分层寻路是否得到了可用的分层结果
//   +0x3C  dword  调用方透传的 a8（CellTypeClass_DoPathfinding 的最后一个参数）
//   +0x70  Vector 本轮"可达格"缓存（sub_42CCD0 填写）
//   +0x74  Vector x3 三种通行区（陆/水/空）的开放表，stride 0x18
//
// 结构体尾部（+0x74..+0xBB）在原版是三个 `VectorPtrIntLong`（带 vtable 的
// DynamicVectorClass），因此 pathfinder_CTOR 里对它们逐个走 `[vt+0Ch]`（清空）。
// 本工程用 DynamicVectorClass<uint32> 表达同样的三个槽位。
//
// 权威来源：asm 行 135445（CellTypeClass_DoPathfinding / 0x42C8D0）、
//           131174（sub_42A5B0 / Reset_Search）、135924（sub_42CCD0）、
//           129819（sub_429A90 / Find_Path）、131340（pathfinder_CTOR）。
// ============================================================================

class TechnoClass;

// ----------------------------------------------------------------------------
// PathClass —— 寻路结果。原版 PathClass 的"格数"在 +0x08（调用方
// `cmp dword ptr [eax+8], 19h` 判路径长度），这里用 Count 表达同一语义。
// ----------------------------------------------------------------------------
class PathClass {
public:
    PathClass() noexcept : Count(0) {}
    ~PathClass() noexcept {}

    void Clear() {
        Cells.Clear();
        Count = 0;
    }

    bool Add(const CellStruct& cell) {
        if (!Cells.Add(cell))
            return false;
        Count = Cells.Count;                       // asm: [path+8] 同步
        return true;
    }

    int32 Get_Count() const { return Count; }      // asm: [path+8]
    const CellStruct& operator[](int32 index) const { return Cells[index]; }

    DynamicVectorClass<CellStruct> Cells;
    int32                          Count;           // asm +0x08
};

// ----------------------------------------------------------------------------
// CellTypeClass —— 全局寻路单例（asm `path_WTF`）。
// ----------------------------------------------------------------------------
class CellTypeClass {
public:
    static CellTypeClass* Instance;                // asm: .data path_WTF

    CellTypeClass();
    ~CellTypeClass();

    // ------------------------------------------------------------------------
    // asm 0x42A5B0 —— 每轮搜索前的复位：
    //   [this+0Ch][0x180000] = 0 ; [this+10h][0x100000] = 0 ;
    //   清空 this+14h / this+68h 两张访问表；++[this+28h]；
    //   代号回绕时把 this+18h / this+1Ch 两张表整体清零；
    //   再按 this+4Ch/+50h/+54h 三个"区"把 this+40h/+44h/+48h 三张
    //   per-cell 表里本区条目清零。
    // ------------------------------------------------------------------------
    void Reset_Search();

    // ------------------------------------------------------------------------
    // asm 0x42CCD0 —— 围绕给定起点把"当前可直达的格"收进 this+70h。
    // 原版按 CellSpread 表逐环展开、用 sub_5840C0 判可达；命中即记格号。
    // ------------------------------------------------------------------------
    void Purge_Unreachable(const CellStruct& from, TechnoClass* pSubject);

    // ------------------------------------------------------------------------
    // asm 0x429A90 —— A* 主体。返回从 coords 到 position 的格链（PathClass），
    // 无路可走时返回 nullptr。MovementZone 决定可通行判定，
    // subject 的 WhatAmI() 决定是否按"飞行器"忽略桥面附加代价。
    // ------------------------------------------------------------------------
    PathClass* Find_Path(const CoordStruct& coords, const CoordStruct& position,
                         TechnoClass* pSubject, int32 a5, int32 a6,
                         MovementZone movZone);

    // ------------------------------------------------------------------------
    // asm 0x42C8D0 —— CellTypeClass_DoPathfinding：分层 A* 包装。
    //
    //   * this+38h = 1，Reset_Search()，三个区表各自清空；
    //   * this+3Ch = a8，起终点解析成格；
    //   * movZone == -1 时用 Subject->GetType()->MoveZone 兜底；
    //   * 两次 Can_Location_Be_Reached、两次 Resolve_Path_Cell 求 from/to；
    //   * Subject->WhatAmI() == Infantry 且其载具态标志置位时把
    //     MovZone 改成 Infantry(7) 并校验 locomotor 接口；
    //   * useHS 闸门（类型/Subject/雷达三重判定）；
    //   * 分层尝试失败时打印 WWDebug 串并把 useHS 清掉；
    //   * 最后跑 1 次或 5 次 Find_Path，必要时重开分层搜索。
    // ------------------------------------------------------------------------
    PathClass* Do_Pathfinding(const CoordStruct& coords,
                              const CoordStruct& position,
                              TechnoClass* pSubject, int32 a5, int32 a6,
                              int32 movZone, int32 a8);

    // ---- 原版布局对应的字段 ------------------------------------------------
    int32          SearchStamp;        // +0x28 搜索代号
    bool           HasPath;            // +0x38 本轮分层结果可用
    int32          SubjectIndex;       // +0x3C 调用方透传的 a8
    DynamicVectorClass<uint32> ReachableCells;      // +0x70 可达格缓存
    DynamicVectorClass<uint32> ZoneLists[3];        // +0x74 三个区表
};
