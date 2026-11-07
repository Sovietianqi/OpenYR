// =============================================================================
// VoxelRaster.cpp - VXL 体素光栅器（画家算法 + 逐面明暗）
//
// 参考 gamemd.exe：
//   0x73B4A0 UnitClass_DrawVXL          帧号：WalkingFrame % BodyHVA帧数；
//                                       bodyFrame == 0 时炮塔帧 =
//                                       TurretWalkFrame % TurretHVA帧数
//   0x706ED0 TechnoClass_706ED0         逐 section 取 VoxLib 记录
//   0x753F90                            记录 = 变换后中心 + 4 角点 + 包围盒
//   0x754510 / sub_7542F0               按记录 +0x24（变换后 z）排序 -> 画家算法
//   0x756590                            4 角点正交投影 floor((v+128-pos)*256)
//   0x758670 Voxel_ProcessNormalsTables 明暗 = clamp(n·L,0,1)*16（16 级）
//   0x754C00                            L = (0,-0.7071,0) 绕竖直轴旋转，z 恒 0
//
// 体素记录布局（VoxLib_Process 返回的 164 字节，sizeof(VoxLib)=0x1C）：
//   +0x00 3x4 矩阵（HVA 帧变换）
//   +0x40 8 个角点（8 x 3 float，载入时 Buffer_Clear(rec+0x40, 8, 12) 证实）
//   +0xA0 R, +0xA1 G, +0xA2 B（section 颜色）
//   +0xA3 normalMode（0 = 需要法线明暗）
// 本实现直接从 VXL 的 span 求每个非空体素，等价于上述记录展开，但保留
// 逐面剔除，避免绘制模型内部的面。
// =============================================================================

#include "Rendering/VoxelRaster.h"
#include "Rendering/Surface.h"
#include "Rendering/ConvertClass.h"
#include "Rendering/PaletteClass.h"
#include "FileFormats/VXL.h"
#include "FileFormats/HVA.h"

#include <cstring>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    // 全局光照方向。
    //
    // asm 0x754C00 把 (0,-0.7071,0) 绕竖直轴旋转，水平分量随地图光照角变化，
    // 垂直分量恒为 0。原版在**视图空间**做点乘，等轴视图的视线俯角把
    // 「屏幕向上」折算进了该向量；本渲染器在世界空间做点乘，因此保留
    // 水平旋转语义，同时给垂直分量一个固定仰角，保证顶面亮于侧面。
    float s_LightX = 0.0f;
    float s_LightY = 0.70710678f;
    float s_LightZ = -0.70710678f;

    // 读 blob 中的 uint32（blob 来自 malloc/memcpy，不保证 4 字节对齐）
    inline uint32 ReadU32At(const BYTE* p)
    {
        uint32 v;
        std::memcpy(&v, p, sizeof(v));
        return v;
    }


    // 一个待绘制的体素表面四边形。
    struct VoxelFace
    {
        float X[4];
        float Y[4];
        float Depth;
        float NX, NY, NZ;
        BYTE  Color;
    };

    inline int32 ClampInt(int32 v, int32 lo, int32 hi)
    {
        return (v < lo) ? lo : ((v > hi) ? hi : v);
    }

    // ------------------------------------------------------------------------
    // Voxel_PackColor - 调色板索引 -> 16bpp XRGB565
    //
    // asm 的最终合成走 LightConvertClass::SelectBlitter0 + CC_Draw_Shape，
    // 由 blitter 把 8bit 索引查 LightConvert 的 16bpp 表。这里直接按
    // ConvertClass 的源调色板取 RGB，再乘明暗级与亮度，等价落成 565。
    //   level:     0..16（Voxel_NormalizedNodes 的 16 级明暗）
    //   brightness: 0..1000
    //   tint:      调色板下标，非 0 时按 1/4 混入
    // ------------------------------------------------------------------------
    uint16 Voxel_PackColor(const ConvertClass* pConvert, BYTE index,
                           int32 level, int32 brightness, int32 tint)
    {
        int32 r = 0, g = 0, b = 0;

        if (pConvert != nullptr)
        {
            const RGBClass& c = pConvert->Get_Palette().Get_Color(index);
            r = c.Red;
            g = c.Green;
            b = c.Blue;
        }

        // 16 级斜坡：level 0 -> 0.5，level 16 -> 1.0。
        // （asm 用 VOXELS.VPL 的明暗表；本仓库没有该素材表，故用线性斜坡。）
        int32 ramp = (level + 16) * 16;      // 256..512，除以 512 得 0.5..1.0
        r = (r * ramp) >> 9;
        g = (g * ramp) >> 9;
        b = (b * ramp) >> 9;

        int32 bright = ClampInt(brightness, 0, 1000);
        r = (r * bright) / 1000;
        g = (g * bright) / 1000;
        b = (b * bright) / 1000;

        if (tint != 0 && pConvert != nullptr)
        {
            const RGBClass& t = pConvert->Get_Palette().Get_Color(tint & 0xFF);
            r += ((static_cast<int32>(t.Red)   - r) * 64) >> 8;
            g += ((static_cast<int32>(t.Green) - g) * 64) >> 8;
            b += ((static_cast<int32>(t.Blue)  - b) * 64) >> 8;
        }

        r = ClampInt(r, 0, 255);
        g = ClampInt(g, 0, 255);
        b = ClampInt(b, 0, 255);

        return static_cast<uint16>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }

    // ------------------------------------------------------------------------
    // Voxel_FillQuad - 扫描线填充凸四边形
    //
    // 顶点已经投影到屏幕空间；逐行求 4 条边与扫描线的交点，取最小/最大 x
    // 之间填色。裁剪矩形与目标面范围都在这里生效（asm 同样在 blitter 内
    // 用 row offset 表做逐行裁剪）。
    // ------------------------------------------------------------------------
    void Voxel_FillQuad(Surface* pDest, const VoxelFace& face, uint16 color,
                        const Rectangle& clip)
    {
        const int32 minYf = static_cast<int32>(std::floor(std::min(std::min(face.Y[0], face.Y[1]),
                                                                  std::min(face.Y[2], face.Y[3]))));
        const int32 maxYf = static_cast<int32>(std::ceil(std::max(std::max(face.Y[0], face.Y[1]),
                                                                 std::max(face.Y[2], face.Y[3]))));

        int32 y0 = ClampInt(minYf, clip.Y, clip.Y + clip.Height - 1);
        int32 y1 = ClampInt(maxYf, clip.Y, clip.Y + clip.Height - 1);
        if (y0 > y1)
            return;

        for (int32 y = y0; y <= y1; ++y)
        {
            const float sy = static_cast<float>(y) + 0.5f;
            float xMin = 1.0e30f;
            float xMax = -1.0e30f;
            bool  hit = false;

            for (int32 e = 0; e < 4; ++e)
            {
                const int32 e2 = (e + 1) & 3;
                const float ay = face.Y[e];
                const float by = face.Y[e2];
                if ((ay <= sy && by > sy) || (by <= sy && ay > sy))
                {
                    const float t = (sy - ay) / (by - ay);
                    const float x = face.X[e] + t * (face.X[e2] - face.X[e]);
                    if (x < xMin) xMin = x;
                    if (x > xMax) xMax = x;
                    hit = true;
                }
            }

            if (!hit)
                continue;

            int32 xStart = static_cast<int32>(std::floor(xMin));
            int32 xEnd   = static_cast<int32>(std::ceil(xMax)) - 1;
            if (xStart > xEnd)
                continue;

            xStart = ClampInt(xStart, clip.X, clip.X + clip.Width - 1);
            xEnd   = ClampInt(xEnd,   clip.X, clip.X + clip.Width - 1);
            if (xStart > xEnd)
                continue;

            for (int32 x = xStart; x <= xEnd; ++x)
            {
                Point2D pt(x, y);
                pDest->SetPixel(&pt, static_cast<DWORD>(color));
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Voxel_SetLightAngle - 旋转全局光照方向（绕竖直轴）
// ----------------------------------------------------------------------------
void Voxel_SetLightAngle(float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    // asm 0x754C00 只旋转水平分量：L = (0,-0.7071,0) 绕竖直轴转。
    // 这里保留水平旋转，并把 0.7071 的模长拆成「水平 + 固定仰角」，
    // 使顶面（+Y）与迎光侧面同时受光。
    const float k = 0.70710678f;
    s_LightX = k * c;
    s_LightY = k;
    s_LightZ = k * s;
}

// ----------------------------------------------------------------------------
// Voxel_Rasterize
// ----------------------------------------------------------------------------
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
                     const Rectangle* pClip)
{
    if (pDest == nullptr || pVXL == nullptr || pConvert == nullptr)
        return false;
    if (!pVXL->HasSection(section))
        return false;
    if (pDest->GetBytesPerPixel() != 2 && pDest->GetBytesPerPixel() != 4)
        return false;

    // ------------------------------------------------------------------
    // 取该 section 当前动画帧的 record。
    // asm: VoxLib_Process(section, frame) = RecordTable + (FirstRecord + frame) * 164。
    // VXL 帧数 = LastRecord - FirstRecord + 1；HVA 帧号超出时取模。
    // ------------------------------------------------------------------
    VXLRecordView rec;
    if (!pVXL->GetRecord(section, (hvaFrame >= 0) ? hvaFrame : 0, rec))
        return false;
    if (rec.Voxels == nullptr || rec.VoxelCount <= 0)
        return false;

    const int32 sizeX = rec.SizeX;
    const int32 sizeY = rec.SizeY;
    const int32 sizeZ = rec.SizeZ;
    if (sizeX <= 0 || sizeY <= 0 || sizeZ <= 0)
        return false;

    // 体素步长：record 的 AABB 覆盖 Size 个体素。AABB 退化时退回 1 单位/体素。
    float stepX = (rec.Max[0] - rec.Min[0]) / static_cast<float>(sizeX);
    float stepY = (rec.Max[1] - rec.Min[1]) / static_cast<float>(sizeY);
    float stepZ = (rec.Max[2] - rec.Min[2]) / static_cast<float>(sizeZ);
    if (!(stepX > 0.0f)) stepX = 1.0f;
    if (!(stepY > 0.0f)) stepY = 1.0f;
    if (!(stepZ > 0.0f)) stepZ = 1.0f;

    // 裁剪矩形：默认整个目标面。
    Rectangle clip(0, 0, pDest->GetWidth(), pDest->GetHeight());
    if (pClip != nullptr && pClip->Width > 0 && pClip->Height > 0)
        clip = clip.Intersection(*pClip);
    if (clip.IsEmpty())
        return false;

    // ------------------------------------------------------------------
    // 模型 -> 世界：先 HVA（section 自身动画），再挂到物体朝向。
    // asm 0x706FBD: Matrix3D_Multiply(objectMatrix, hvaMatrix)。
    // ------------------------------------------------------------------
    Matrix4 model = transform;
    if (pHVA != nullptr && hvaFrame >= 0)
    {
        const HVAMatrix* hva = pHVA->GetTransform(section, hvaFrame);
        if (hva != nullptr)
        {
            Matrix4 hm;
            hm.Data[0]  = hva->M11; hm.Data[1]  = hva->M12; hm.Data[2]  = hva->M13; hm.Data[3]  = hva->M14;
            hm.Data[4]  = hva->M21; hm.Data[5]  = hva->M22; hm.Data[6]  = hva->M23; hm.Data[7]  = hva->M24;
            hm.Data[8]  = hva->M31; hm.Data[9]  = hva->M32; hm.Data[10] = hva->M33; hm.Data[11] = hva->M34;
            hm.Data[15] = 1.0f;

            Matrix4 combined = transform;
            combined.PostMultiply(hm);
            model = combined;
        }
    }

    // ------------------------------------------------------------------
    // 体素实体查询（O(1)，直接索引 blob）
    //
    // blob 是 uint32 数组，0xFFFFFFFF = 空（asm 0x7569F6）。
    // 索引顺序 x 变化最快：idx = x + y*SizeX + z*SizeX*SizeY。
    // ------------------------------------------------------------------
    const BYTE* pVoxels = rec.Voxels;
    const int32 voxelCount = rec.VoxelCount;

    auto solidAt = [&](int32 x, int32 y, int32 z) -> bool {
        if (x < 0 || y < 0 || z < 0 || x >= sizeX || y >= sizeY || z >= sizeZ)
            return false;
        const int64 idx = static_cast<int64>(x)
                        + static_cast<int64>(y) * sizeX
                        + static_cast<int64>(z) * sizeX * sizeY;
        if (idx >= voxelCount)
            return false;
        return ReadU32At(pVoxels + idx * 4) != 0xFFFFFFFFu;
    };

    // 以 AABB 中心为模型原点（asm 记录里的「中心 + 角点」同义）。
    // 注意坐标系置换：VXL 原生 Z 向上，项目约定 Y 向上。
    //   项目 X = VXL X
    //   项目 Y = VXL Z
    //   项目 Z = VXL Y
    const float centerX = (rec.Min[0] + rec.Max[0]) * 0.5f;   // VXL X
    const float centerY = (rec.Min[1] + rec.Max[1]) * 0.5f;   // VXL Y
    const float centerZ = (rec.Min[2] + rec.Max[2]) * 0.5f;   // VXL Z

    // 6 个方向：法线 + 四边形 4 个角（VXL 原生局部坐标，单位 = 1 体素）。
    static const float kNormal[6][3] =
    {
        {  1.0f,  0.0f,  0.0f }, { -1.0f,  0.0f,  0.0f },
        {  0.0f,  1.0f,  0.0f }, {  0.0f, -1.0f,  0.0f },
        {  0.0f,  0.0f,  1.0f }, {  0.0f,  0.0f, -1.0f },
    };

    // 每个方向对应的 4 个角相对 (x,y,z) 的偏移。
    static const float kCorner[6][4][3] =
    {
        // +X
        { {1,0,0}, {1,1,0}, {1,1,1}, {1,0,1} },
        // -X
        { {0,0,1}, {0,1,1}, {0,1,0}, {0,0,0} },
        // +Y
        { {0,1,0}, {0,1,1}, {1,1,1}, {1,1,0} },
        // -Y
        { {0,0,1}, {0,0,0}, {1,0,0}, {1,0,1} },
        // +Z
        { {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1} },
        // -Z
        { {1,0,0}, {0,0,0}, {0,1,0}, {1,1,0} },
    };

    std::vector<VoxelFace> faces;
    faces.reserve(2048);

    for (int32 z = 0; z < sizeZ; ++z)         // VXL Z：向上
    {
        for (int32 y = 0; y < sizeY; ++y)     // VXL Y：前后
        {
            for (int32 x = 0; x < sizeX; ++x) // VXL X：左右
            {
                if (!solidAt(x, y, z))
                    continue;

                const int64 idx = static_cast<int64>(x)
                                + static_cast<int64>(y) * sizeX
                                + static_cast<int64>(z) * sizeX * sizeY;
                const uint32 raw = ReadU32At(pVoxels + idx * 4);
                const BYTE color = static_cast<BYTE>(raw & 0xFF);

                for (int32 f = 0; f < 6; ++f)
                {
                    if (solidAt(x + static_cast<int32>(kNormal[f][0]),
                                y + static_cast<int32>(kNormal[f][1]),
                                z + static_cast<int32>(kNormal[f][2])))
                        continue;   // 邻居实心：内部面，剔除

                    VoxelFace face;
                    face.Color = color;
                    face.Depth = 0.0f;

                    // 法线转到世界空间（只取旋转部分）。
                    // VXL (X,Y,Z) -> 项目 (X, Z, Y)。
                    model.TransformVector(kNormal[f][0], kNormal[f][2], kNormal[f][1],
                                          face.NX, face.NY, face.NZ);

                    float depthSum = 0.0f;
                    for (int32 v = 0; v < 4; ++v)
                    {
                        // 体素格角在 VXL 模型空间的位置
                        const float vx = rec.Min[0] + (static_cast<float>(x) + kCorner[f][v][0]) * stepX;
                        const float vy = rec.Min[1] + (static_cast<float>(y) + kCorner[f][v][1]) * stepY;
                        const float vz = rec.Min[2] + (static_cast<float>(z) + kCorner[f][v][2]) * stepZ;

                        // 转到项目空间并相对 AABB 中心：项目 X = VXL X,
                        // 项目 Y = VXL Z, 项目 Z = VXL Y。
                        const float lx = vx - centerX;
                        const float ly = vz - centerZ;
                        const float lz = vy - centerY;

                        float wx, wy, wz;
                        model.TransformPoint(lx, ly, lz, wx, wy, wz);

                        // 正交投影：asm sub_756590 直接
                        //   screen = floor((v + 128 - pos) * LeptonsPerCell)
                        // 没有透视除法（1 体素 = 固定像素数）。
                        face.X[v] = static_cast<float>(screenPos.X) + wx;
                        face.Y[v] = static_cast<float>(screenPos.Y) - wy;
                        depthSum += wz;
                    }
                    face.Depth = depthSum * 0.25f;
                    faces.push_back(face);
                }
            }
        }
    }

    if (faces.empty())
        return false;

    // ------------------------------------------------------------------
    // 画家算法：asm sub_754510/sub_7542F0 按记录 +0x24（变换后 z）排序后
    // 逐个绘制。这里同样按面平均深度从远到近排序。
    // ------------------------------------------------------------------
    std::sort(faces.begin(), faces.end(),
              [](const VoxelFace& a, const VoxelFace& b) { return a.Depth < b.Depth; });

    for (size_t i = 0; i < faces.size(); ++i)
    {
        const VoxelFace& face = faces[i];

        // 法线归一化后点乘光照方向 -> 0..16 级（Voxel_ProcessNormalsTables）
        const float len = std::sqrt(face.NX * face.NX + face.NY * face.NY + face.NZ * face.NZ);
        float dot = 0.0f;
        if (len > 1.0e-6f)
        {
            dot = (face.NX * s_LightX + face.NY * s_LightY + face.NZ * s_LightZ) / len;
        }
        if (dot < 0.0f) dot = 0.0f;
        if (dot > 1.0f) dot = 1.0f;

        const int32 level = static_cast<int32>(dot * 16.0f + 0.5f);
        const uint16 color = Voxel_PackColor(pConvert, face.Color, level, brightness, tint);

        Voxel_FillQuad(pDest, face, color, clip);
    }

    return true;
}
