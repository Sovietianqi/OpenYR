#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Containers/DynamicVectorClass.h"

class TechnoClass;
class HouseClass;

// ============================================================================
// SpawnManagerClass - launcher spawn-point manager
//
//  根据游戏行为，可知发射载具（母舰/发射架）名下有一组出生点节点：
//  每个节点记录当前停靠的单位与就绪状态，管理器负责节点的生成、
//  回收与就绪统计。
// ============================================================================

class SpawnManagerClass {
public:
    // 一个出生点节点：停靠单位与占用标记。
    struct Node {
        TechnoClass* Unit;
        bool IsOccupied;
    };

    SpawnManagerClass() noexcept;
    virtual ~SpawnManagerClass();

    // 节点管理
    void CreateNode();
    void KillNodes();

    // 就绪统计
    int32 CountReadyNodes() const;
    int32 CountReadySpawns() const;
    int32 CountSpecialSpawns() const;

    // 载体与所属方
    TechnoClass* Owner;
    HouseClass* OwnerHouse;

    // 名下节点表
    DynamicVectorClass<Node> Nodes;
};
