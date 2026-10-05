#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"

class WaypointPathClass {
public:
    WaypointPathClass() noexcept;

    // 根据游戏行为，可知航线链是一条顺序节点序列：查询命中、
    // 追加节点与取下一跳都沿序列推进。
    bool WaypointExistsHere(const CellStruct& cell) const;
    void AddWaypoint(const CellStruct& cell);
    CellStruct GetWaypointAfter(const CellStruct& cell) const;

    CellStruct* Nodes;
    int32 NodeCount;
    int32 NodeCapacity;
    int32 Cursor;
};
