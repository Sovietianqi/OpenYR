#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../IO/CRC.h"
#include "../Math/CoordStruct.h"

class WaypointPathClass {
public:
    // asm: vec_WaypointPaths —— 全部航线链的登记表（同步日志按其转储 CRC）。
    static DynamicVectorClass<WaypointPathClass*>* Array;

    WaypointPathClass() noexcept;

    // asm: WaypointPathClass::ComputeCRC（vt+34h）—— 节点数 + 逐个节点格。
    void ComputeCRC(CRCEngine& crc) const;

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
