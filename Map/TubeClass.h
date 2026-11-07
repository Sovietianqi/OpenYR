#pragma once

#include "../Core/Definitions.h"
#include "../Core/Memory.h"
#include "../IO/CRC.h"

// ============================================================================
// Map/TubeClass.h
//
// asm 权威：vec_Tubes（隧道实体表）。
//
// 原版把地图上的每条隧道（含"穿过隧道"的传送语义）登记进 vec_Tubes，
// 同步日志的 43 个 CRC 段里有一段专列 Tube 的 CRC：对每个 Tube 调
// vt+34h 求值后按 "%05d      %08x" 输出。
//
// 本工程的隧道逻辑尚未接入，但实体表与 CRC 槽是同步日志的组成部分，
// 故按原版形态保留：登记表 + 出入口格 + 朝向，ComputeCRC 把这三项
// 折进累积器（等价原版 vt+34h 对 [Tube+0/+4/+8] 的取值）。
// ============================================================================
class TubeClass
{
public:
    // asm: vec_Tubes.data / vec_Tubes.length
    static DynamicVectorClass<TubeClass*>* Array;

    TubeClass() noexcept;

    // asm: TubeClass::ComputeCRC（vt+34h）—— 入口格 / 出口格 / 朝向 三项。
    void ComputeCRC(CRCEngine& crc) const;

    CellStruct  EnterCell;      // +0x00 入口格（X/Y 两个 int16）
    CellStruct  ExitCell;       // +0x04 出口格
    int32       Direction;      // +0x08 通行方向（0 = 双向，1 = 仅入口→出口）
};
