#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../IO/CRC.h"
#include "../Math/CoordStruct.h"

// ============================================================================
// WaypointPathClass —— 一条"行军规划路线"（由若干节点顺序串成）。
//
// asm 依据（WaypointPathClass_CTOR / _DCTOR / ~WaypointPathClass，0x7637xx）：
//   +00..+0F  四个 vftable（WaypointPathClass / IRTTITypeInfo / INoticeSink /
//             INoticeSource）。本工程不模拟多继承 vtable，故略。
//   +24h      int，构造时写 -1。HouseClass_CanAddPlanningWaypoint 要求它仍是
//             -1，才允许继续往这条路线追加节点。
//   +28h      DynamicVectorClass<WaypointClass> 的 vftable。
//   +2Ch      节点表数据指针。
//   +30h      int，构造时清零（游标/扫描位置）。
//   +34h      byte，构造时置 1。
//   +35h      byte，构造时清零。
//   +38h      节点数（length）。
//   +3Ch      节点表容量，构造时写 0Ah —— 即默认容量为 10。
//   析构时按 +35h 的"自有"标记释放节点表。
//
// 本工程用 CellStruct 存放节点。asm 的 WaypointClass 是 {int X; int Y}，单位是
// 像素（lepton）；各处取格时统一做 (v + (v>>31 & 0xFF)) >> 8，即算术右移 8 位
// —— 与 CellClass::Coord2Cell 完全等价。故这里直接存格坐标，比较语义一致。
// ============================================================================
class WaypointPathClass {
public:
    // asm: vec_WaypointPaths —— 全部航线链的登记表（同步日志按其转储 CRC）。
    static DynamicVectorClass<WaypointPathClass*>* Array;

    // asm: WaypointPathClass_CTOR (0x7637F5) —— 参数是槽位号：仅当
    //   0 <= index < 0Ch 时才把自己登记进 vec_WaypointPaths（越界则不登记，
    //   但仍然正常构造）。
    explicit WaypointPathClass(int32 index) noexcept;

    // asm: ~WaypointPathClass (0x7638xx) —— 从 vec_WaypointPaths 里摘除自己并
    //   释放节点表。
    ~WaypointPathClass();

    // asm: WaypointPathClass::ComputeCRC（vt+34h）—— 节点数 + 逐个节点格。
    void ComputeCRC(CRCEngine& crc) const;

    // asm: +38h —— 节点数。
    int32 GetWaypointCount() const { return NodeCount; }

    // asm: WaypointPathClass_GetWaypoint —— 取第 idx 个节点；越界返回空。
    const CellStruct* GetWaypoint(int32 idx) const;

    // 根据游戏行为，可知航线链是一条顺序节点序列：查询命中、
    // 追加节点与取下一跳都沿序列推进。
    bool WaypointExistsHere(const CellStruct& cell) const;
    void AddWaypoint(const CellStruct& cell);
    CellStruct GetWaypointAfter(const CellStruct& cell) const;

    CellStruct* Nodes;
    int32 NodeCount;
    int32 NodeCapacity;
    int32 Cursor;

    // asm: +24h —— 构造置 -1；被规划逻辑占用后改写。
    int32 RefId;

    // asm: +3Ch —— 节点表初始容量。
    static constexpr int32 DefaultCapacity = 10;

private:
    // 保证 vec_WaypointPaths 已建立（asm 里是 .data 段的静态对象）。
    static DynamicVectorClass<WaypointPathClass*>* EnsureRegistry();
    // 从登记表里摘除自己。
    void Unregister();

    WaypointPathClass(const WaypointPathClass&) = delete;
    WaypointPathClass& operator=(const WaypointPathClass&) = delete;
};
