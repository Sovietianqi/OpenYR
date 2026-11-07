#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Containers/DynamicVectorClass.h"

class HouseClass;

// ============================================================================
// RadBeamClass - 雷达/辐射光束
//
// asm 权威布局（RadBeamClass_CTOR / RadBeam_Create / RadBeamClass_Draw）：
//   光束是一个“逐项填入的参数包”，由 TechnoClass::Radiation_Beam 通过
//   RadBeam_Create 创建后，依次调用各 Set* 入口写入起止点、颜色、频率与
//   振幅，最后由 RadBeam_DrawAll 每帧统一绘制并回收。
//
//   字段偏移（asm 逐条读出）：
//     +0x0C SetAnotherArg(int)
//     +0x10 CTOR 参数 a2
//     +0x18 double 周期（Create: a2==1 ? 10.0 : 20.0）
//     +0x20 RGB 颜色 (RadBeam_SetColor)
//     +0x24 CoordStruct From   (SetCoords)
//     +0x30 CoordStruct To     (SetCoordsTarget)
//     +0x3C int 最大相位
//     +0x40 double Amplitude   (SetAmplitude)  ★名字级缺口
//     +0x48/+0x4C SetSomeMoreArg(int,int)
//     +0x50 SetEvenMoreArg(int)
//     +0x54 SetYetAnotherArg(int)
//     +0x58 SetStillAnotherArg(byte)
//     +0x5C 由 Draw 计算的差值；+0x68 长度；+0x7C 相位步进
// ============================================================================
class RadBeamClass
{
public:
    static DynamicVectorClass<RadBeamClass*>* Array;   // asm: vec_RadBeams

    explicit RadBeamClass(int a2 = 0) noexcept;

    // ========================================================================
    // asm: RadBeam_Create (0x659120 区) —— 工厂：new RadBeamClass(a2)，
    //   再按 a2 写入周期：a2==1 ? 10.0 : 20.0，返回新对象（CTOR 已入队
    //   vec_RadBeams）。名字级缺口：OpenYR 里此前直接 new。
    // ========================================================================
    static RadBeamClass* Create(int a2);

    // --- Set* 参数入口（名字与 asm 一致）---
    void SetAnotherArg(int32 v);                 // asm: +0x0C
    void SetCoords(const CoordStruct& v);        // asm: RadBeamClass_SetCoords (+0x24)
    void SetCoordsTarget(const CoordStruct& v);  // asm: RadBeamClass_SetCoordsTarget (+0x30)
    void SetColor(const ColorStruct& c);         // asm: RadBeam_SetColor (+0x20)
    void SetFrequency(int32 v);                  // asm: RadBeamClass.Frequency
    void SetAmplitude(double v);                 // asm: RadBeamClass.Amplitude (+0x40)
    void SetSomeMoreArg(int32 a2, int32 a3);     // asm: +0x48/+0x4C
    void SetEvenMoreArg(int32 v);                // asm: +0x50
    void SetYetAnotherArg(int32 v);              // asm: +0x54
    void SetStillAnotherArg(uint8 v);            // asm: +0x58

    // 每帧绘制（asm: char RadBeamClass_Draw(RadBeamClass*)），返回是否存活。
    bool Draw();
    // type-2 波束的逐帧光栅（asm: char sub_659CA0(RadBeamClass*)），返回值同 Draw。
    bool Draw_Type2();
    // 把 vec_RadBeams 中所有波束提交绘制并回收已结束者（asm: RadBeam_DrawAll）。
    static void DrawAll();

    // ---- 字段（偏移见文件头注释）----
    int32       Unknown00;      // +0x00
    void*       Attached;       // +0x04  被依附对象（Draw 用其坐标做端点）
    uint8       field_08;       // +0x08
    uint8       field_09;       // +0x09  CTOR 置 1
    int32       field_0C;       // +0x0C  SetAnotherArg
    int32       field_10;       // +0x10  CTOR 参数 a2
    double      Period;         // +0x18  Create: (a2==1)?10.0:20.0
    ColorStruct Color;          // +0x20  RGB
    CoordStruct From;           // +0x24
    CoordStruct To;             // +0x30
    int32       field_3C;       // +0x3C  最大相位
    double      Amplitude;      // +0x40  ★ SetAmplitude
    int32       Frequency;      // asm 符号 RadBeamClass.Frequency
    int32       field_48;       // +0x48
    int32       field_4C;       // +0x4C
    int32       field_50;       // +0x50
    int32       field_54;       // +0x54  相位步进
    uint8       field_58;       // +0x58
    CoordStruct field_5C;       // +0x5C  终点-起点
    double      Length;         // +0x68  欧氏长度
    int32       field_7C;       // +0x7C  相位
    double      field_80;       // +0x80
    int32       field_88;       // +0x88  段索引
    int32       field_8C;       // +0x8C  段数
    CoordStruct field_90;       // +0x90  段步进
    CoordStruct field_9C;       // +0x9C  正弦位移后的第一个端点
    CoordStruct field_A8;       // +0xA8  正弦位移后的第二个端点
    double      field_B8;       // +0xB8  颜色增益 0.5 + |dz|*0.5/Amplitude
    uint8       field_C0;       // +0xC0  颜色副本 R
    uint8       field_C1;       // +0xC1
    uint8       field_C2;       // +0xC2

private:
    // asm: sub_659AC0 —— 由段索引求两个端点（含正弦 Z 位移）与颜色增益。
    void ComputeSegment(int32 index);
    // asm: sub_659FF0 —— type-2 的同形端点计算（角度按 180 度量程）。
    void ComputeType2Segment(int32 index);
    // asm: sub_659E30 —— type-2 首次初始化（差值/长度/段数/步进）。
    void InitType2();
    // asm: sub_659EE0 —— type-2 每帧重算增益并按相位推进目标偏移。
    void StepType2();
    // 端点投影 + 视口裁剪 + DrawLineColor_AZ 提交（asm: Coord_To_Pixel + vt[34h]）。
    void RasterSegment();
};
