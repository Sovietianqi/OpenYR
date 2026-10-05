#include "SpawnManagerClass.h"

// ============================================================================
// SpawnManagerClass
// ============================================================================

SpawnManagerClass::SpawnManagerClass() noexcept
    : Owner(nullptr), OwnerHouse(nullptr)
{
}

SpawnManagerClass::~SpawnManagerClass()
{
    KillNodes();
}

void SpawnManagerClass::CreateNode()
{
    // 根据游戏行为，可知节点生成追加一个空位：初始无停靠单位。
    Node node;
    node.Unit = nullptr;
    node.IsOccupied = false;
    Nodes.Add(node);
}

void SpawnManagerClass::KillNodes()
{
    // 根据游戏行为，可知节点回收把名下节点全部清空，停靠单位交还
    // 上层处置。
    Nodes.Clear();
}

int32 SpawnManagerClass::CountReadyNodes() const
{
    // 根据游戏行为，可知就绪节点是空位节点：无单位占用即就绪。
    int32 count = 0;
    for (int32 i = 0; i < Nodes.Count; ++i) {
        if (!Nodes.Items[i].IsOccupied)
            ++count;
    }
    return count;
}

int32 SpawnManagerClass::CountReadySpawns() const
{
    // 根据游戏行为，可知就绪发射位与就绪节点同口径统计。
    return CountReadyNodes();
}

int32 SpawnManagerClass::CountSpecialSpawns() const
{
    // 根据游戏行为，可知特殊发射位统计占用节点：占用即计一次。
    int32 count = 0;
    for (int32 i = 0; i < Nodes.Count; ++i) {
        if (Nodes.Items[i].IsOccupied)
            ++count;
    }
    return count;
}
