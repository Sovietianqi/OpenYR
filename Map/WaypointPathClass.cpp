#include "WaypointPathClass.h"

WaypointPathClass::WaypointPathClass() noexcept
    : Nodes(nullptr)
    , NodeCount(0)
    , NodeCapacity(0)
    , Cursor(0)
{
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知命中查询逐节点比对格位坐标，序列为空或越界
// 时立即报告未命中。
// ------------------------------------------------------------------------
bool WaypointPathClass::WaypointExistsHere(const CellStruct& cell) const
{
    for (int32 i = 0; i < NodeCount; ++i) {
        if (Nodes[i].X == cell.X && Nodes[i].Y == cell.Y) {
            return true;
        }
    }
    return false;
}

void WaypointPathClass::AddWaypoint(const CellStruct& cell)
{
    if (NodeCount >= NodeCapacity) {
        const int32 newCapacity = (NodeCapacity > 0) ? NodeCapacity * 2 : 16;
        CellStruct* pNew = new CellStruct[newCapacity];
        for (int32 i = 0; i < NodeCount; ++i) {
            pNew[i] = Nodes[i];
        }
        delete[] Nodes;
        Nodes = pNew;
        NodeCapacity = newCapacity;
    }
    Nodes[NodeCount++] = cell;
}

CellStruct WaypointPathClass::GetWaypointAfter(const CellStruct& cell) const
{
    for (int32 i = 0; i < NodeCount; ++i) {
        if (Nodes[i].X == cell.X && Nodes[i].Y == cell.Y) {
            if (i + 1 < NodeCount) {
                return Nodes[i + 1];
            }
            break;
        }
    }

    CellStruct sentinel;
    sentinel.X = -1;
    sentinel.Y = -1;
    return sentinel;
}
