#include "WaypointPathClass.h"

DynamicVectorClass<WaypointPathClass*>* WaypointPathClass::Array = nullptr;

WaypointPathClass::WaypointPathClass() noexcept
    : Nodes(nullptr)
    , NodeCount(0)
    , NodeCapacity(0)
    , Cursor(0)
{
}

// ------------------------------------------------------------------------
// asm: WaypointPathClass::ComputeCRC（vt+34h，同步日志的 Waypoint Path 段）
//   先喂节点数，再按顺序把每个节点格的两个 int16 喂进累积器。
// ------------------------------------------------------------------------
void WaypointPathClass::ComputeCRC(CRCEngine& crc) const
{
    crc.AddData(&NodeCount, sizeof(NodeCount));
    for (int32 i = 0; i < NodeCount; ++i) {
        crc.AddData(&Nodes[i].X, sizeof(Nodes[i].X));
        crc.AddData(&Nodes[i].Y, sizeof(Nodes[i].Y));
    }
}

// ------------------------------------------------------------------------
// asm: char __thiscall WaypointPathClass_WaypointExistsHere(WaypointPathClass *this, WaypointClass *wpt)   (asm:1762296)
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
