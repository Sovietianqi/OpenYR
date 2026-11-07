#include "Rendering/LaserDrawClass.h"

#include "Rendering/Surface.h"
#include "Rendering/GScreenClass.h"
#include "Rendering/TacticalClass.h"
#include "Helpers/AsmMath.h"
#include "Helpers/AsmUIHelpers.h"

#include <cmath>

// ============================================================================
// LaserDrawClass::Draw_InHouseColor
//
//   asm: LaserDrawClass_Draw_InHouseColor (0x5508C0, thiscall, 793 条指令)
//
// 流程（asm 行号见各处注释）：
//   1) 首次调用惰性建 8x4 dword 的锯齿方向表（-1/0/+1 交替）；
//   2) 由两端点的世界坐标求束向角，折算成 0..7 的方向表行号；
//   3) 两个端点各自投影到屏幕像素，并按建筑 Z 求两条 Z 基准；
//   4) 细节闸门（DetailFrameRate >= GetMinFrameRate）与相位插值；
//   5) 取 InnerColor（或翻倍饱和后）作为束色，主循环按段推进四个端点，
//      每段画两笔；非双色束每段后把颜色减半，太暗就提前收束；
//   6) 循环后补一笔整束（细节层走 RGB 乘法线，否则先 Adjust 再画 565 线）。
// ============================================================================

namespace {

// ----------------------------------------------------------------------------
// asm: byte_ABC7C8（初始化守卫字节）+ dword_ABC7F8..dword_ABC874（8x4 表）
//
// 原版在首次进入时用一串 mov 把 32 个 dword 写进 .bss，把守卫字节的 bit0
// 置位，并向 atexit 注册 nullsub_21（MSVC6 静态初始化守卫的空操作）。
// C++ 的函数内 static 局部初始化本身就是线程安全的惰性初始化，语义等价，
// 故直接用常量表，守卫字节与 atexit 都不再需要。
//
// 表值逐条对照 asm 729080-729132 的赋值序列（每行 4 个 dword，前 4 行
// 与后 4 行相同）：
//   dword_ABC7F8 {-1,-1, 1, 1}   dword_ABC808 { 0,-1, 0, 1}
//   dword_ABC818 {-1, 1, 1,-1}   dword_ABC828 {-1, 0, 1, 0}
//   dword_ABC838 {-1,-1, 1, 1}   dword_ABC848 { 0,-1, 0, 1}
//   dword_ABC858 {-1, 1, 1,-1}   dword_ABC868 {-1, 0, 1, 0}
// ----------------------------------------------------------------------------
struct LaserJagTable
{
    int32 D[8][4];
};

const LaserJagTable& JagTable()
{
    static const LaserJagTable kTable = { {
        { -1, -1,  1,  1 },     // dword_ABC7F8
        {  0, -1,  0,  1 },     // dword_ABC808
        { -1,  1,  1, -1 },     // dword_ABC818
        { -1,  0,  1,  0 },     // dword_ABC828
        { -1, -1,  1,  1 },     // dword_ABC838
        {  0, -1,  0,  1 },     // dword_ABC848
        { -1,  1,  1, -1 },     // dword_ABC858
        { -1,  0,  1,  0 },     // dword_ABC868
    } };
    return kTable;
}

// asm: byte_8A0DF0 —— D3D 顶点/质量管线开关。本构建里该字节只被写过 0
// （asm 1435990 / 1436031），顶点路径恒不启用；本工程的渲染后端也只有软件
// Surface，故按原版实测值取 0。分支保留，语义与 asm 一致。
constexpr uint8 kQualityMode = 0;

// asm: flt_7E2220 = 255.0 / flt_7E3560 = 0.000015259022（1/65535）
constexpr float kPhaseScale = 255.0f;

// asm: col_B_SHR_3 / col_B_SHL_0 / col_G_SHR_2 / col_G_SHL_5 /
//      col_R_SHR_3 / col_R_SHL_11 —— 16bpp 5-6-5 打包用的移位常量。
constexpr int32 kBlueShift = 3,  kBlueLeft = 0;
constexpr int32 kGreenShift = 2, kGreenLeft = 5;
constexpr int32 kRedShift = 3,   kRedLeft = 11;

// asm 16210-16232 / 16526-16551：把 RGB 三元组按上面六个移位压成表面色。
inline int32 Pack565(uint8 r, uint8 g, uint8 b)
{
    int32 v = (static_cast<int32>(b) >> kBlueShift) << kBlueLeft;
    v |= (static_cast<int32>(g) >> kGreenShift) << kGreenLeft;
    v |= (static_cast<int32>(r) >> kRedShift) << kRedLeft;
    return v;
}

// 一像素一个交叉积符号判定的三角形填充（软件后端的顶点路径消费端）。
void FillTriangle(Surface* pSurface, const Rectangle& clip,
                  const Point2D& p0, const Point2D& p1, const Point2D& p2,
                  DWORD color)
{
    Rectangle bbox(p0.X < p1.X ? (p0.X < p2.X ? p0.X : p2.X)
                               : (p1.X < p2.X ? p1.X : p2.X),
                   p0.Y < p1.Y ? (p0.Y < p2.Y ? p0.Y : p2.Y)
                               : (p1.Y < p2.Y ? p1.Y : p2.Y),
                   0, 0);
    const int32 maxX = p0.X > p1.X ? (p0.X > p2.X ? p0.X : p2.X)
                                   : (p1.X > p2.X ? p1.X : p2.X);
    const int32 maxY = p0.Y > p1.Y ? (p0.Y > p2.Y ? p0.Y : p2.Y)
                                   : (p1.Y > p2.Y ? p1.Y : p2.Y);
    bbox.Width = maxX - bbox.X + 1;
    bbox.Height = maxY - bbox.Y + 1;

    bbox = bbox.Intersection(clip);
    if (bbox.IsEmpty())
        return;

    const int64 e0 = static_cast<int64>(p1.X - p0.X) * (p2.Y - p0.Y) -
                     static_cast<int64>(p1.Y - p0.Y) * (p2.X - p0.X);
    if (e0 == 0)
        return;                                        // 退化三角形

    for (int32 y = bbox.Y; y < bbox.Y + bbox.Height; ++y)
    {
        for (int32 x = bbox.X; x < bbox.X + bbox.Width; ++x)
        {
            const int64 w0 = static_cast<int64>(p1.X - p0.X) * (y - p0.Y) -
                             static_cast<int64>(p1.Y - p0.Y) * (x - p0.X);
            const int64 w1 = static_cast<int64>(p2.X - p1.X) * (y - p1.Y) -
                             static_cast<int64>(p2.Y - p1.Y) * (x - p1.X);
            const int64 w2 = static_cast<int64>(p0.X - p2.X) * (y - p2.Y) -
                             static_cast<int64>(p0.Y - p2.Y) * (x - p2.X);
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                (w0 <= 0 && w1 <= 0 && w2 <= 0))
            {
                Point2D pt(x, y);
                pSurface->SetPixel(&pt, color);
            }
        }
    }
}

} // namespace

void LaserDrawClass::Draw_InHouseColor()
{
    const LaserJagTable& jag = JagTable();             // asm 15804-15876

    // asm: mov ecx, TempSurface —— 原版画到全局隐面；本工程取屏幕后台面
    // （同一张合成面，见 GScreenClass::Get_Back_Buffer）。
    Surface* const pSurface = (GScreenClass::Instance != nullptr)
                                  ? GScreenClass::Instance->Get_Back_Buffer()
                                  : nullptr;
    TacticalClass* const pTactical = TacticalClass::Instance;
    if (pSurface == nullptr || pTactical == nullptr)
        return;                                        // 无面/无投影器可画

    // asm: rect_mouseclass_vp —— 原版用全局视口矩形做裁剪；本工程没有子
    // 视口，等价取目标面自身矩形。
    Rectangle vp = pSurface->GetRect();

    // ---- asm 15882-15913：束向角 -> 锯齿方向表行号 --------------------------
    //   arctanfoo(Y1 - Y2, X2 - X1) = atan2(dy, dx)
    const double dy = static_cast<double>(Y1) - static_cast<double>(Y2);
    const double dx = static_cast<double>(X2) - static_cast<double>(X1);
    double angle = std::atan2(dy, dx);
    angle -= 1.570796326794897;                        // fsub double_Pi2
    angle *= -1.043006004058427e4;                     // fmul num_m0_0001
    const int32 angleFix =
        static_cast<int32>(Float_To_Int_Floor(angle)) & 0xFFFF;   // word [var_11C]
    const int32 dirIndex = ((((angleFix >> 12) + 1) >> 1) & 7);

    // ---- asm 15905-15920：两个端点投影到像素 --------------------------------
    Point2D pixB = pTactical->To_Pixel(CoordStruct(X1, Y1, Z1));
    Point2D dst  = pTactical->To_Pixel(CoordStruct(X2, Y2, Z2));

    // ---- asm 15922-15933：建筑 Z 修正 --------------------------------------
    int32 ebpZ = field_3C - Draw_AdjustForBuildingZ(Z1) - 2;
    int32 esiZ = -2 - Draw_AdjustForBuildingZ(Z2);

    // ---- asm 15934-15946：细节闸门 ----------------------------------------
    //   原式为 DetailFrameRate >= GetMinFrameRate() && dword_A8EB78 != 0。
    //   dword_A8EB78 是选项对话框 0x52B 复选写出的 0/2 的半透明/细节开关；
    //   本工程选项表未建模该复选，而渲染后端始终走软件合成面，等价于开关
    //   打开（非零），故细节层只由帧率闸门决定。
    const bool detail = (TacticalClass::DetailFrameRate >=
                         TacticalClass::Get_Min_Frame_Rate());

    // ---- asm 15949-15973：相位插值 ----------------------------------------
    float phase = 1.0f;
    if (field_52 != 0)
    {
        const int32 den = Duration;
        phase = (static_cast<float>(floatVal) - static_cast<float>(field_58)) *
                    static_cast<float>(den - TimeElapsed) /
                    static_cast<float>(den) +
                static_cast<float>(field_58);
    }
    const int32 phaseI = static_cast<int32>(
        Float_To_Int_Floor(static_cast<double>(phase) *
                           static_cast<double>(kPhaseScale)));

    // ---- asm 15974-16039：取色 --------------------------------------------
    uint8 colMain[3];                                  // var_130
    uint8 colRaw[3];                                   // var_118
    if (field_21 == 0)
    {
        // loc_550CAA：raw = InnerColor，main = InnerColor / 2。
        colRaw[0] = InnerColor.R;
        colRaw[1] = InnerColor.G;
        colRaw[2] = InnerColor.B;
        colMain[0] = static_cast<uint8>(colRaw[0] >> 1);
        colMain[1] = static_cast<uint8>(colRaw[1] >> 1);
        colMain[2] = static_cast<uint8>(colRaw[2] >> 1);
    }
    else
    {
        // asm 15984-16019：三通道各翻倍，超过 0xFF 夹到 0xFF；raw = main。
        int32 b = static_cast<int32>(InnerColor.B) << 1;
        int32 r = static_cast<int32>(InnerColor.R) << 1;
        int32 g = static_cast<int32>(InnerColor.G) << 1;
        if (b > 0xFF) b = 0xFF;
        if (g > 0xFF) g = 0xFF;
        if (r > 0xFF) r = 0xFF;
        colRaw[0] = colMain[0] = static_cast<uint8>(r);
        colRaw[1] = colMain[1] = static_cast<uint8>(g);
        colRaw[2] = colMain[2] = static_cast<uint8>(b);
    }

    // ---- asm 16041-16059：工作端点 ----------------------------------------
    int32 b1x = pixB.X, b1y = pixB.Y;                  // var_F0 / var_EC
    int32 b2x = pixB.X, b2y = pixB.Y;                  // var_108 / var_104
    int32 a1x = dst.X,  a1y = dst.Y;                   // var_110 / var_10C
    int32 a2x = dst.X,  a2y = dst.Y;                   // var_F8 / var_F4

    const int32 segCount = field_1C;
    const int32 parity = dirIndex & 1;                 // var_D8

    for (int32 seg = 1; seg <= segCount; ++seg)        // var_E8
    {
        // ---- asm loc_550D31 / loc_550DAC / loc_550DEE：推进工作端点 --------
        const int32* const d = jag.D[dirIndex];
        if (parity != 0)
        {
            b1x += d[0]; b1y += d[1];
            b2x += d[2]; b2y += d[3];
            a1x += d[0]; a1y += d[1];
            a2x += d[2]; a2y += d[3];
        }
        else if ((seg & 1) != 0)                       // loc_550DAC
        {
            b1x += d[0]; b2y += d[3];
            a1x += d[0]; a2y += d[3];
        }
        else                                           // loc_550DEE
        {
            b1y += d[1]; b2x += d[2];
            a1y += d[1]; a2x += d[2];
        }

        if (kQualityMode == 0)                         // asm 16155-16158
        {
            Point2D pB1(b1x, b1y), pA1(a1x, a1y);
            Point2D pA2(a2x, a2y), pB2(b2x, b2y);

            if (detail)                                // asm 16160-16195
            {
                ColorStruct col(colMain[0], colMain[1], colMain[2]);
                pSurface->DrawRGBMultiplyingLine_AZ(&vp, &pB1, &pA1, &col,
                                                    phase, ebpZ, esiZ);
                pSurface->DrawRGBMultiplyingLine_AZ(&vp, &pB2, &pA2, &col,
                                                    phase, ebpZ, esiZ);
            }
            else                                       // asm 16199-16256
            {
                RGBClass adj(colMain[0], colMain[1], colMain[2]);
                adj.Adjust(phaseI, RGBClass(0xFF, 0xFF, 0xFF));   // var_60
                const int32 color16 = Pack565(adj.Red, adj.Green, adj.Blue);
                pSurface->DrawLineColor_AZ(&vp, &pB1, &pA1, color16,
                                           ebpZ, esiZ, false);
                pSurface->DrawLineColor_AZ(&vp, &pB2, &pA2, color16,
                                           ebpZ, esiZ, false);
            }

            // ---- asm 16259-16307：段后调色 --------------------------------
            if (field_21 != 0 && seg == 1)
            {
                // 双色束的第一段画完恢复成 InnerColor 满色。
                colMain[0] = InnerColor.R;
                colMain[1] = InnerColor.G;
                colMain[2] = InnerColor.B;
            }
            else
            {
                // 各通道减半；三通道都掉到阈值以下就收束（asm: jb loc_550FD1）。
                const int32 rHalf = colMain[0] >> 1;
                const int32 gHalf = colMain[1] >> 1;
                const int32 bHalf = colMain[2] >> 1;
                const int32 thresh = detail ? 8 : 0x40;
                if (rHalf < thresh && gHalf < thresh && bHalf < thresh)
                    break;
                colMain[0] = static_cast<uint8>(rHalf);
                colMain[1] = static_cast<uint8>(gHalf);
                colMain[2] = static_cast<uint8>(bHalf);
            }
        }
    }

    // ---- asm 16319-16481：循环后 -------------------------------------------
    if (kQualityMode != 0)
    {
        // 顶点路径：原版把两枚三角形（b1,b2,a1 / b2,a2,a1）按
        // 0xAARRGGBB = 0xFF, colRaw*phaseI>>8 提交给 D3D 顶点批次
        // （sub_4A35A0 写色 / sub_4A35F0 写顶点 / sub_4A3840 入批）。
        // 本工程没有 D3D 顶点批次，改为在目标面上按同一组顶点、同一颜色
        // 光栅化这两枚三角形（四边形 b1,b2,a2,a1）。
        const int32 r8 = (static_cast<int32>(colRaw[0]) * phaseI) >> 8;
        const int32 g8 = (static_cast<int32>(colRaw[1]) * phaseI) >> 8;
        const int32 b8 = (static_cast<int32>(colRaw[2]) * phaseI) >> 8;
        const DWORD argb = 0xFF000000u |
                           (static_cast<DWORD>(r8 & 0xFF) << 16) |
                           (static_cast<DWORD>(g8 & 0xFF) << 8) |
                           static_cast<DWORD>(b8 & 0xFF);
        const Point2D v0(b1x, b1y), v1(b2x, b2y);
        const Point2D v2(a2x, a2y), v3(a1x, a1y);
        FillTriangle(pSurface, vp, v0, v1, v3, argb);
        FillTriangle(pSurface, vp, v1, v2, v3, argb);
        return;
    }

    // ---- asm loc_5511CE / loc_55120D：整束补一笔 ---------------------------
    if (detail)
    {
        ColorStruct col(colRaw[0], colRaw[1], colRaw[2]);
        pSurface->DrawRGBMultiplyingLine_AZ(&vp, &pixB, &dst, &col,
                                            phase, ebpZ, esiZ);
        return;
    }

    RGBClass adj(colRaw[0], colRaw[1], colRaw[2]);
    adj.Adjust(phaseI, RGBClass(0xFF, 0xFF, 0xFF));
    const int32 color16 = Pack565(adj.Red, adj.Green, adj.Blue);
    pSurface->DrawLineColor_AZ(&vp, &pixB, &dst, color16, ebpZ, esiZ, false);
}
