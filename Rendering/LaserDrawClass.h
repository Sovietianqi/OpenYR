#pragma once

#include "Core/Definitions.h"

// ============================================================================
// LaserDrawClass - 激光束的绘制描述块
//
//   asm: LaserDrawClass struc ; (sizeof=0x5C, mappedto_4652)
//
// 原版把一个"待绘制的激光"全部状态压在这个 0x5C 字节的结构里：两个端点
// （世界坐标 + Z）、所属方颜色、锯齿段数与闪烁相位。绘制入口
// LaserDrawClass_Draw_InHouseColor (0x5508C0, thiscall, 793 条指令) 把它
// 按颜色画成一条带锯齿的光束。
//
// 字段偏移逐一取自 asm 的 struc 定义；头文件末尾有 static_assert 守住
// 0x5C 的总长，任何字段增删都会在编译期被拦下。
// ============================================================================

// asm: rgbbx ; (sizeof=3, mappedto_5135)  RG dw / B db
struct LaserDrawRGB
{
    uint8 R;
    uint8 G;
    uint8 B;
};

// asm: TimerStruct（创建帧时间戳，0x0C 字节：CreationFrame 占 0x08..0x13）
struct LaserDrawTimer
{
    int32 Start;
    int32 Duration;
    int32 TimeLeft;
};

class LaserDrawClass
{
public:
    // asm: LaserDrawClass_Draw_InHouseColor (0x5508C0, thiscall)
    //   在已存的两对坐标之间按 InnerColor 画锯齿激光；首调用惰性建锯齿
    //   方向表（见 .cpp 的 JagTable()）。
    void Draw_InHouseColor();

    int32 TimeElapsed;              // 0x00 已存活帧数
    uint8 field_4;                  // 0x04
    uint8 field_5;                  // 0x05
    uint8 field_6;                  // 0x06
    uint8 field_7;                  // 0x07
    LaserDrawTimer CreationFrame;   // 0x08 (0x0C 字节)
    int32 field_14;                 // 0x14
    int32 TimeElapsedPerFrame;      // 0x18
    int32 field_1C;                 // 0x1C 锯齿段数
    uint8 IsHouseColor;             // 0x20 用所属方颜色
    uint8 field_21;                 // 0x21 双色束（InnerColor 满色起画）
    uint8 field_22;                 // 0x22
    uint8 field_23;                 // 0x23
    int32 X1;                       // 0x24 端点一 X（lepton）
    int32 Y1;                       // 0x28 端点一 Y
    int32 Z1;                       // 0x2C 端点一 Z
    int32 X2;                       // 0x30 端点二 X
    int32 Y2;                       // 0x34 端点二 Y
    int32 Z2;                       // 0x38 端点二 Z
    int32 field_3C;                 // 0x3C 端点一的绘制基准 Z
    uint8 field_40;                 // 0x40
    LaserDrawRGB InnerColor;        // 0x41 内层颜色（R,G,B）
    LaserDrawRGB OuterColor;        // 0x44
    LaserDrawRGB OuterSpread;       // 0x47
    uint8 field_4A;                 // 0x4A
    uint8 field_4B;                 // 0x4B
    int32 Duration;                 // 0x4C 总持续帧数（相位分母）
    uint8 Blinks;                   // 0x50
    uint8 BlinkState;               // 0x51
    uint8 field_52;                 // 0x52 非零：按 Duration 插值相位
    uint8 field_53;                 // 0x53
    float floatVal;                 // 0x54 相位起点（0..1 乘以 255）
    int32 field_58;                 // 0x58 相位终点
};

static_assert(sizeof(LaserDrawClass) == 0x5C,
              "LaserDrawClass 必须与 asm 的 0x5C 字节布局一致");
