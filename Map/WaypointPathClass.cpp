#include "WaypointPathClass.h"

DynamicVectorClass<WaypointPathClass*>* WaypointPathClass::Array = nullptr;

// ------------------------------------------------------------------------
// asm: vec_WaypointPaths —— .data 段的静态 DynamicVectorClass，首次用到时才
//   真正建立（原版依赖静态构造顺序，本工程改为惰性建立）。
// ------------------------------------------------------------------------
DynamicVectorClass<WaypointPathClass*>* WaypointPathClass::EnsureRegistry()
{
    if (Array == nullptr) {
        Array = new DynamicVectorClass<WaypointPathClass*>();
    }
    return Array;
}

// ------------------------------------------------------------------------
// asm: WaypointPathClass_CTOR (0x7637F5)
//   __thiscall(this, int a2)
//     +24h = -1；+2Ch = 0；+30h = 0；+34h = 1；+35h = 0；+38h = 0；+3Ch = 0Ah
//     若 0 <= a2 < 0Ch，则把自己追加到 vec_WaypointPaths 末尾。
// ------------------------------------------------------------------------
WaypointPathClass::WaypointPathClass(int32 index) noexcept
    : Nodes(nullptr)
    , NodeCount(0)
    , NodeCapacity(DefaultCapacity)
    , Cursor(0)
    , RefId(-1)
{
    if (index >= 0 && index < 12) {
        DynamicVectorClass<WaypointPathClass*>* pReg = EnsureRegistry();
        if (pReg != nullptr) {
            pReg->Add(this);
        }
    }
}

// ------------------------------------------------------------------------
// asm: ~WaypointPathClass (0x7638xx)
//   先按登记表找出自己的下标（vt+10h = Find），命中就把其后的元素整体前移一位
//   并让 length 减一；随后按 +35h 的"自有"标记释放节点表。
//   节点表在本工程里总是由 AddWaypoint 用 new[] 分配，故一律 delete[]。
// ------------------------------------------------------------------------
WaypointPathClass::~WaypointPathClass()
{
    Unregister();

    if (Nodes != nullptr) {
        delete[] Nodes;
        Nodes = nullptr;
    }
    NodeCount = 0;
    NodeCapacity = 0;
}

void WaypointPathClass::Unregister()
{
    if (Array == nullptr || Array->Items == nullptr) {
        return;
    }

    for (int32 i = Array->Count - 1; i >= 0; --i) {
        if (Array->Items[i] == this) {
            Array->Remove(i);
            return;
        }
    }
}

const CellStruct* WaypointPathClass::GetWaypoint(int32 idx) const
{
    if (idx < 0 || idx >= NodeCount || Nodes == nullptr) {
        return nullptr;
    }
    return &Nodes[idx];
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
        const int32 newCapacity = (NodeCapacity > 0) ? NodeCapacity * 2 : DefaultCapacity;
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
