#pragma once

#include "Core/Definitions.h"
#include "Math/Rectangle.h"

#include <cmath>

// ============================================================================
// VoxelRaster - VXL 体素光栅器
//
// 依据 gamemd.exe 的体素绘制链：
//
//   UnitClass_DrawVXL            0x73B4A0  取帧号、组装绘制列表
//   TechnoClass_706ED0           0x706ED0  逐 section：VoxLib_Process 取 164 字节
//                                          记录 -> 8 角点变换 -> 求包围盒
//   sub_7542F0 / sub_754510               按记录 +0x24（变换后 z）排序，
//                                          从后往前逐个体素 blit（画家算法）
//   sub_756590                   0x756590  四角点正交投影到屏幕
//   Voxel_ProcessNormalsTables   0x758670  法线点乘光照方向得到 0..16 级明暗
//
// 关键事实（asm 逐句核对，见 FileFormats/VXL.h）：
//   * VoxLib_Process(section, frame) = RecordTable + (FirstRecord + frame) * 164，
//     即 **一个 section 的每一动画帧对应一条 164 字节记录**。记录里有该帧的
//     4x3 变换、AABB（由两个角点展开成 8 个角点）和指向体素载荷的三个指针；
//   * 体素载荷是 blob 内的 uint32 数组，0xFFFFFFFF 表示空体素
//     （asm 0x7569F6: cmp dword ptr [edx+edi*4], 0FFFFFFFFh）；
//     低字节是调色板索引，次低字节是法线编号；
//   * 投影是正交投影，没有透视除法：asm 里
//       screen = floor((world + 128 - cellPos) * LeptonsPerCell)；
//   * 明暗 = clamp(dot(法线, 光照方向), 0, 1) * 16，共 16 级
//     （Voxel_NormalizedNodes[]）。
//
// 本实现把记录里的体素载荷展开成「每个可见面一个四边形」：遍历 section
// 包围盒，对每个非空体素检查 6 个邻居，只有朝向空气的面才生成四边形
// （面剔除），再按深度从远到近填充。这样既保留 asm 的画家算法与逐面明暗，
// 又不会把模型内部的面画出来。
//
// 本实现把“每体素一条记录”等价展开为“每个可见面一个四边形”：遍历
// section 包围盒，对每个非空体素检查 6 个邻居，只有朝向空气的面才生成
// 四边形（面剔除），再按深度从远到近填充。这样既保留 asm 的画家算法
// 与逐面明暗，又不会把模型内部的面画出来。
// ============================================================================

class Surface;
class ConvertClass;
class VXLClass;
class HVAClass;

// ============================================================================
// Matrix4 - 4x4 行主序矩阵
//
// 与 asm Matrix3D_5AFB80（0x5AFB80）同构：元素按 3 行 x 4 列存放，
// 第 4 列是平移量，点变换为
//     x' = M[0]*x + M[1]*y + M[2]*z  + M[3]
//     y' = M[4]*x + M[5]*y + M[6]*z  + M[7]
//     z' = M[8]*x + M[9]*y + M[10]*z + M[11]
// （asm: fld [edx+8]*[esi+8] + fld [edx+4]*[esi+4] + fld [edx]*[esi]
//        + fadd [edx+0Ch]）
// 第 4 行固定为 (0,0,0,1)。
// ============================================================================
struct Matrix4
{
    float Data[16];

    Matrix4()
    {
        for (int32 i = 0; i < 16; ++i) Data[i] = 0.0f;
        Data[0] = Data[5] = Data[10] = Data[15] = 1.0f;
    }

    static Matrix4 Identity()
    {
        return Matrix4();
    }

    // 绕模型竖直轴（Y）旋转：VXL 的 X 是左右、Y 是高度、Z 是前后。
    static Matrix4 RotationY(float radians)
    {
        Matrix4 m;
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        m.Data[0] = c;   m.Data[2] = -s;
        m.Data[8] = s;   m.Data[10] = c;
        return m;
    }

    static Matrix4 RotationZ(float radians)
    {
        Matrix4 m;
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        m.Data[0] = c;  m.Data[1] = -s;
        m.Data[4] = s;  m.Data[5] = c;
        return m;
    }

    // 绕 X 轴俯仰（飞机 / 斜坡上的载具）
    static Matrix4 RotationX(float radians)
    {
        Matrix4 m;
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        m.Data[5] = c;   m.Data[6] = -s;
        m.Data[9] = s;   m.Data[10] = c;
        return m;
    }

    static Matrix4 Translation(float x, float y, float z)
    {
        Matrix4 m;
        m.Data[3] = x;
        m.Data[7] = y;
        m.Data[11] = z;
        return m;
    }

    // this = this * rhs（先应用 rhs，再应用 this）
    void PostMultiply(const Matrix4& rhs)
    {
        float out[16];
        for (int32 r = 0; r < 4; ++r)
        {
            for (int32 c = 0; c < 4; ++c)
            {
                float sum = 0.0f;
                for (int32 k = 0; k < 4; ++k)
                    sum += Data[r * 4 + k] * rhs.Data[k * 4 + c];
                out[r * 4 + c] = sum;
            }
        }
        for (int32 i = 0; i < 16; ++i) Data[i] = out[i];
    }

    void TransformPoint(float x, float y, float z,
                        float& outX, float& outY, float& outZ) const
    {
        outX = Data[0] * x + Data[1] * y + Data[2]  * z + Data[3];
        outY = Data[4] * x + Data[5] * y + Data[6]  * z + Data[7];
        outZ = Data[8] * x + Data[9] * y + Data[10] * z + Data[11];
    }

    void TransformVector(float x, float y, float z,
                         float& outX, float& outY, float& outZ) const
    {
        outX = Data[0] * x + Data[1] * y + Data[2]  * z;
        outY = Data[4] * x + Data[5] * y + Data[6]  * z;
        outZ = Data[8] * x + Data[9] * y + Data[10] * z;
    }
};

// ============================================================================
// Voxel_Rasterize
//
// 把一个 VXL section 正交投影光栅化到 16bpp 目标面。
//
//   pDest       目标面（必须已分配 Buffer）
//   pConvert    调色板转换器；为 null 时不做调色板转换（颜色全黑，调用方不应传 null）
//   pVXL        VXL 模型
//   section     section 下标（0=本体，1=炮塔，2=炮管）
//   pHVA        HVA 动画，可为 null
//   hvaFrame    该 section 的 HVA 帧号（调用方按帧数取模，见 UnitClass_DrawVXL）
//   transform   模型 -> 世界的 4x4（本体朝向 / 炮塔朝向）
//   screenPos   模型原点在屏幕上的位置
//   brightness  0..1000，1000 = 原色
//   tint        色调索引（调色板下标），0 = 不变
//   pClip       裁剪矩形，null = 整个目标面
//
// 返回：至少写入一个像素返回 true；空 section / 无可绘面返回 false。
// ============================================================================
bool Voxel_Rasterize(Surface* pDest,
                     ConvertClass* pConvert,
                     VXLClass* pVXL,
                     int32 section,
                     HVAClass* pHVA,
                     int32 hvaFrame,
                     const Matrix4& transform,
                     Point2D screenPos,
                     int32 brightness,
                     int32 tint,
                     const Rectangle* pClip);

// ============================================================================
// Voxel_LightDirection - 全局光照方向
//
// asm sub_754C00（0x754C00，InitGame 调用）：把 (0, -0.70711, 0) 绕竖直轴
// 旋转地图光照角，z 分量恒为 0；即体素的明暗只由水平朝向决定。
// ============================================================================
void Voxel_SetLightAngle(float radians);
