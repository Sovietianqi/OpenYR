#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class CCINIClass;
class BuildingTypeClass;
class CRCEngine;

// ============================================================================
// BaseNodeClass - one pre-placed structure of a player's starting base.
//
//   Sixteen bytes per node, exactly as the original strides them:
//
//     +00  BuildingTypeClass* pType    resolved "Type" (or -1 for a raw index)
//     +04  Point Cell                  the cell the node sits on
//     +08  int32 field_08              fourth comma field
//     +0C  int32 field_0C              fifth comma field
//
//   The "<Type>,<X>,<Y>,<...>,<...>" string is split with strtok; a leading
//   '-' means the first field is a numeric building index rather than a name.
// ============================================================================

struct BaseNodeClass
{
    BuildingTypeClass*  pType;
    CellStruct          Cell;
    int32               field_08;
    int32               field_0C;
};

// ============================================================================
// BaseClass - the starting base of one house.
//
 //   BaseClass_LoadFromINI reads:
//     "PercentBuilt"  -> +1C  (default: previous value)
//     "NodeCount"     -> number of "%03d" entries to walk
//   Each "%03d" entry names a node, and the nodes are appended to the array
//   at +04 as long as the capacity check at +10/+14 allows.
// ============================================================================

class BaseClass
{
public:
    BaseClass();
    virtual ~BaseClass();

 // 根据游戏行为，可知 LoadFromINI 负责下面这段逻辑。
    void LoadFromINI(CCINIClass* pINI, const char* pSection);

    // ------------------------------------------------------------------------
    // asm: BaseClass_SaveToINI (retn 8) —— LoadFromINI 的逆操作。
    //
    //   ecx = this, 两个栈参数 (INI, section)。逐字段写回：
    //     PercentBuilt  (= [+1C])
    //     NodeCount     (= [+14])
    //     每个节点的 "%03d" 键 = "<类型名>,<X>,<Y>"（索引 < 0 时写成
    //     "<裸索引>,<X>,<Y>"，与 LoadFromINI 的 '-' 前缀约定互为逆）。
    //   节点类型索引取自 node.pType 的数组下标（+24h 的 Name 由
    //   BuildingTypeClass::Vector[idx] 提供）。
    // ------------------------------------------------------------------------
    bool SaveToINI(CCINIClass* pINI, const char* pSection) const;

    // ------------------------------------------------------------------------
    // asm: BaseClass_GetCRC (retn 4) —— 喂给 CRC：先 NodeCount，再逐节点
    //   { pType(4B), Cell.X(2B), Cell.Y(2B) }。field_08/0C 不参与。
    // ------------------------------------------------------------------------
    void ComputeCRC(CRCEngine& crc) const;

    // ------------------------------------------------------------------------
    // 节点管理。原版把节点数组放在基类里的 DynamicVectorClass<BaseNodeClass>
    //   (+04 data / +14 length)，这里用等价的裸数组 + 计数表达；下面三个
    //   helper 就是该向量的 Add / Find / Remove 语义，供 LoadFromINI、
    //   Placed、以及基地重建流程复用。
    // ------------------------------------------------------------------------
    // 返回首个 {pType, cell} 完全匹配的节点下标，找不到返回 -1。
    int32 Find_Node(BuildingTypeClass* pType, const CellStruct& cell) const;
    // 追加一个节点；必要时按 GrowthStep 扩容。成功返回新节点下标，失败 -1。
    int32 Add_Node(BuildingTypeClass* pType, const CellStruct& cell);
    // 删除指定下标的节点并左移后继。越界返回 false。
    bool Remove_Node(int32 index);

    void Clear();

    // ------------------------------------------------------------------------
    // asm: sub_42E780(BaseClass *this, int idx) —— 判定某节点是否"已占用/
    //   不可再建"。依次：
    //     * HouseClass_Base_WhatNeedsWalls(house) != 0 -> 视为占用（true）；
    //     * 节点的 pType 索引 < 0 -> 未占用；
    //     * 该类型是墙(Wall) 且落点已被同种墙占 -> 占用；
    //     * 落点上已有建筑 -> 占用；
    //   否则未占用（false）。
    //   本项目以 Nodes[idx] 的类型与地图格上是否已有建筑近似。
    // ------------------------------------------------------------------------
    bool Is_Node_Occupied(int32 index) const;

    // ------------------------------------------------------------------------
    // asm: BaseClass::Next_Buildable (0x42EB3C, retn 4)
    //   BaseNodeClass *__thiscall BaseClass::Next_Buildable(BaseClass *this, int a2)
    //   a2 == -1 时找"任意类型"的下一个可建节点；否则找 pType 索引 == a2 的。
    //   返回命中节点的地址，找不到返回 nullptr。
    // ------------------------------------------------------------------------
    BaseNodeClass* Next_Buildable(int32 a2);

    // ------------------------------------------------------------------------
    // BaseClass_Placed (0x42F270)
    //
    //   Called from BuildingClass::Unlimbo (0x441597) as
    //
    //     ecx = &house->Base            (add ecx, HouseClass.Base)
    //     push building                 (arg_0)
    //     call BaseClass_Placed
    //
    //   i.e. `this` is a BaseClass and the sole argument is the BuildingClass
    //   that has just been placed.  The routine marks the matching start-node
    //   as "placed" so the AI stops rebuilding it.
    //
    //   Layout facts taken from the dump (VectorPtrBaseNodeClass is 0x18 bytes
    //   with data at +04 and length at +14, so inside BaseClass):
    //
    //     this+08  == vec_Nodes.data        (the node pointer array)
    //     this+14h == vec_Nodes.length      (node count)
    //     this+74h == the house index this base belongs to
    //     a2+21Ch  == BuildingClass's OwnerHouseIndex
    //     a2+520h  == BuildingClass's Type (BuildingTypeClass*)
    //
    //   The body has two arms.  arm 1 runs when the building's house index
    //   matches this+74h: it scans the nodes for one whose BuildingTypeClass
    //   ArrayIndex equals the placed building's type ArrayIndex AND whose cell
    //   equals the building's cell, and stamps {+08=1, +0C=0} on a hit -- but
    //   it only reports success, it does not write when the scan misses.
    //   arm 2 runs otherwise: when the placed type has a nonzero "is part of
    //   this base" flag (type+0DF8h) it scans instead for a node pointing at
    //   that exact type with a clear +08 byte, and stamps +08=1.
    //
    //   Landing this needs HouseClass to embed `BaseClass Base` (the original
    //   keeps it at the offset the call site adds) plus BuildingTypeClass's
    //   ArrayIndex accessor, so it belongs with the Techno/House batch.
    // ------------------------------------------------------------------------
    bool Placed(class BuildingClass* pBuilding);

    // ── Node storage ─────────────────────────────────────────────────────
    BaseNodeClass*  Nodes;          // +04
    int32           Capacity;       // +10
    bool            IsAllocated;    // +0D
    int32           GrowthStep;     // +14
    int32           PercentBuilt;   // +1C
    int32           NodeCount;      // current fill level

    // ------------------------------------------------------------------------
    // +0x74 -- 这个 Base 属于哪个 HouseClass（存的是 house 的数组下标，不是
    //   指针）。BaseClass_Placed 用它和建筑的 OwnerHouseIndex 比对，决定走
    //   第一条（精确匹配节点）还是第二条（按类型原型匹配）分支。
    //
    //   IDA 把该字段显示成 BaseClass 结构里的 `Owner`；这里沿用项目里
    //   "house index" 的命名习惯，避免与 ObjectClass::Owner（HouseClass*）
    //   混淆 —— 二者语义不同：这里是下标，那里是指针。
    // ------------------------------------------------------------------------
    int32           OwnerHouseIndex; // +74
};
