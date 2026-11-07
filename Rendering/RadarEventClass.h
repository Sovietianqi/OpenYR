#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Containers/DynamicVectorClass.h"

class Surface;

// ============================================================================
// RadarEventClass - 雷达事件（minimap 上的脉冲标记）
//
// asm 权威（gamemd.exe，@0x65FA80 区）：
//   RadarEventClass struc ; (sizeof=0x3F)
//     +0x00 Type            dd  事件类型（1..16，与 RadarEventConfigs 索引）
//     +0x04 field_4         dd  相对雷达中心的像素偏移 X
//     +0x08 field_8         dd  相对雷达中心的像素偏移 Y
//     +0x0C Radius          dd  float 当前脉冲半径（初值 = 到雷达面四边的最大
//                                    距离，每帧按 RadarEventSpeed 收缩到
//                                    RadarEventMinRadius 为止）
//     +0x10 Angle           dd  float 当前角度（初值 Pi/4，随 RotationSpeed 旋转）
//     +0x14 RotationSpeed   dd  float RulesData.RadarEventRotationSpeed
//     +0x18 field_18        dd  float 角度累加器（用于颜色/闪烁脉冲）
//     +0x1C ColorSpeed      dd  float RulesData.RadarEventColorSpeed
//     +0x20 Position        wXY  事件所在格坐标（2 x int16）
//     +0x24 field_24        dd  帧戳
//     +0x28 field_28        dd
//     +0x2C field_2C        dd  RadarEventConfigs[Type].Duration
//     +0x30 field_30        dd  帧戳
//     +0x34 field_34        dd
//     +0x38 field_38        dd  RadarEventConfigs[Type].VisibilityDuration
//     +0x3C field_3C        db  活跃标记（脉冲结束置 0）
//     +0x3D field_3D        db  存活标记（到期清 0）
//     +0x3E field_3E        db
//   RadarEventClass ends
//
//   相关 proc：RadarEventClass_Add (0x65FA80) / RadarEventClass_CTOR (0x65FAF0) /
//              sub_65FDD0 (UpdateAll) / sub_65FE00 (逐事件 Update) /
//              vec_RadarEvents_Clear (0x65FD50) / RadarEventClass_ClearVector /
//              sub_660000 (DrawAll) / sub_660050 (画菱形) /
//              sub_660540 (脏点登记) / sub_660730 (旋转求四角)。
//
//   渲染是**纯几何光栅**（不是 SHP）：sub_660730 绕 Z 轴旋转取四角，
//   sub_660050 用 4 条带色相渐变的边画出旋转菱形，sub_660540 只做逐像素
//   脏点登记。颜色与半径全部按 asm 表/公式计算，本类已按此落地。
// ============================================================================
class RadarEventClass
{
public:
    // asm: vec_RadarEvents （DynamicVectorClass<RadarEventClass*>）
    static DynamicVectorClass<RadarEventClass*>* Array;

    // asm: RadarEventClass_CTOR (0x65FAF0, type / coords, retn 8)
    RadarEventClass(int32 type, int16 cellX, int16 cellY);

    // asm: RadarEventClass_Add (0x65FA80, this=type, coords, retn 4)
    //   已存在同类型且在抑制半径内的事件 -> 返回 false；否则新建并入队 -> true。
    static bool Add(int32 type, int16 cellX, int16 cellY);

    // asm: sub_65FE00 —— 单个事件的逐帧推进。
    //   返回 true 表示该事件仍在存活（field_3D 仍为 1）。
    bool Update();

    // asm: sub_65FDD0 —— 遍历 vec_RadarEvents，逐个 Update。
    static void UpdateAll();

    // asm: vec_RadarEvents_Clear (0x65FD50) —— 逆序删除所有事件。
    static void ClearVector();

    // asm: RadarEventClass_ClearVector (0x65FC20) —— 删除单个事件并移出向量。
    static void Remove(RadarEventClass* pEvent);

    // ========================================================================
    // asm: sub_660050 (0x660050, this=ecx) —— 画旋转菱形脉冲。
    //   * sub_660730 求四角：(x,y) (-y,x) (-x,-y) (y,-x)，x/y = floor(R*cos/sin)；
    //   * 颜色 c1/c2 由事件类型查 byte_660374/byte_660394 两张 13 项表：
    //       {0,3,4}    -> (FF,00,FF) / (80,00,80)   洋红
    //       {1,2,11,12}-> (FF,FF,00) / (80,80,00)   黄
    //       {5}        -> (00,FF,FF) / (00,80,80)   青
    //       其余(6..10)-> (00,00,00)，主色全 0 时 asm 直接返回不画；
    //   * 每条边按 t0 = field_18、dt = (2R*0.7071)/sideLen * ColorSpeed
    //     在 c1→c2 间做色相插值，于是整枚菱形随 field_18 在亮/半亮间脉动。
    //   center 为该事件格在雷达面上的绝对像素（asm 里等价于 field_4/field_8
    //   再叠加四角）。
    // ========================================================================
    void DrawPulse(Surface* pSrf, const Point2D& center) const;

    // ========================================================================
    // asm: sub_660540 (0x660540, this=ecx) —— 逐帧把菱形四边逐像素交给
    //   雷达脏点登记（回调 loc_65FB60 -> sub_6562D0）。本工程雷达面整面重绘，
    //   等价收敛为"任一边上有像素落在 128x128 雷达面内就置 [14D9h] 重绘闩锁"。
    // ========================================================================
    void RasterPulse();

    // ---- 字段（偏移见 struct 注释）----
    int32 Type;             // +0x00
    int32 field_4;          // +0x04
    int32 field_8;          // +0x08
    float Radius;           // +0x0C
    float Angle;            // +0x10
    float RotationSpeed;    // +0x14
    float field_18;         // +0x18
    float ColorSpeed;       // +0x1C
    int16 CellX;            // +0x20
    int16 CellY;            // +0x22
    int32 field_24;         // +0x24
    int32 field_28;         // +0x28
    int32 field_2C;         // +0x2C
    int32 field_30;         // +0x30
    int32 field_34;         // +0x34
    int32 field_38;         // +0x38
    uint8 field_3C;         // +0x3C
    uint8 field_3D;         // +0x3D
    uint8 field_3E;         // +0x3E
};
