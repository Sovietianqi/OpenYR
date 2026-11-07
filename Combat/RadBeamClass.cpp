// =============================================================================
// RadBeamClass.cpp - 雷达/辐射光束
//
// 权威来源：asm RadBeam_Create(0x659120) / RadBeamClass_CTOR(0x6590E0 区) /
//   RadBeamClass_Set* (0x6591..) / RadBeamClass_Draw(0x659690) /
//   RadBeam_DrawAll(0x659190) / sub_659AC0(0x659AC0) / sub_659CA0(0x659CA0)。
//
// 束体是**纯几何光栅**（不是 SHP）：Draw 逐段调用 sub_659AC0 求两个 3D 端点
//   （From + 段步进*k，Z 再叠加 sin(角)*Amplitude），投影到屏幕后交给
//   TempSurface 的 vt[34h]（= DrawLineColor_AZ）画一条 16bpp 线段。type-2 走
//   sub_659CA0 → sub_659FF0，几何与 type-1 同形，只是角度量程 180°、段循环
//   固定 field_8C 次。本文件按 asm 的循环边界/颜色公式实现等价光栅。
// =============================================================================

#include "RadBeamClass.h"
#include "../Houses/HouseClass.h"
#include "../Rendering/TacticalClass.h"
#include "../Rendering/Surface.h"
#include "../Math/Rectangle.h"
#include "../Core/Random2Class.h"
#include "../Helpers/AsmMath.h"
#include "../Helpers/AsmUIHelpers.h"

#include <cmath>
#include <cstring>

// ============================================================================
// 静态成员
// ============================================================================
DynamicVectorClass<RadBeamClass*>* RadBeamClass::Array = nullptr;   // vec_RadBeams

// ============================================================================
// 光栅常量（asm 逐条对照）
// ============================================================================
namespace {

// asm: dbl_839530 = 1.74532925199433e-2 —— 角度以“度”为单位，乘它转弧度。
constexpr double kBeamDegToRad = 1.74532925199433e-2;
// asm: dbl_7F04C0 = 360.0 —— type-1 正弦波长基准（Length 更小时取 360）。
constexpr double kBeamWaveLength = 360.0;
// asm: dbl_7E5F30 = 180.0 —— type-2 的角度量程。
constexpr double kBeamType2Span = 180.0;

// asm: col_R_SHR_3/SHL_11 + col_G_SHR_2/SHL_5 + col_B_SHR_3/SHL_0
inline int32 PackBeam565(uint8 r, uint8 g, uint8 b)
{
    return ((static_cast<int32>(r) >> 3) << 11) |
           ((static_cast<int32>(g) >> 2) << 5) |
            (static_cast<int32>(b) >> 3);
}

// asm: sub_7BC2B0 —— “线段是否与裁剪矩形相交”（Liang-Barsky 精确判定）。
bool BeamLineIntersectsRect(const Rectangle& rect, const Point2D& a, const Point2D& b)
{
    if (rect.Width <= 0 || rect.Height <= 0)
        return false;

    const double x0 = static_cast<double>(a.X);
    const double y0 = static_cast<double>(a.Y);
    const double dx = static_cast<double>(b.X) - x0;
    const double dy = static_cast<double>(b.Y) - y0;

    double t0 = 0.0;
    double t1 = 1.0;

    const double p[4] = { -dx, dx, -dy, dy };
    const double q[4] = { x0 - static_cast<double>(rect.X),
                          static_cast<double>(rect.X + rect.Width) - x0,
                          y0 - static_cast<double>(rect.Y),
                          static_cast<double>(rect.Y + rect.Height) - y0 };

    for (int32 i = 0; i < 4; ++i)
    {
        if (p[i] == 0.0)
        {
            if (q[i] < 0.0)
                return false;              // 平行且在界外
            continue;
        }
        const double t = q[i] / p[i];
        if (p[i] < 0.0)
        {
            if (t > t1) return false;
            if (t > t0) t0 = t;
        }
        else
        {
            if (t < t0) return false;
            if (t < t1) t1 = t;
        }
    }
    return t0 <= t1;
}

// asm: sub_659AC0 / sub_659FF0 末尾的同一段颜色计算 ——
//   f = 0.5 + |floor(sin)| * 0.5 / Amplitude
//   C0..C2 = floor(Color.RGB * f)，写回 [+0B8h]/[+0C0h..+0C2h]
void ApplyBeamSegmentColor(RadBeamClass& beam, double sine)
{
    const double absS = std::fabs(std::floor(sine));
    const double f = (beam.Amplitude != 0.0)
                         ? (absS * 0.5 / beam.Amplitude + 0.5)
                         : 0.5;
    beam.field_B8 = f;

    beam.field_C0 = static_cast<uint8>(Float_To_Int_Floor(beam.Color.R * f));
    beam.field_C1 = static_cast<uint8>(Float_To_Int_Floor(beam.Color.G * f));
    beam.field_C2 = static_cast<uint8>(Float_To_Int_Floor(beam.Color.B * f));
}

} // namespace

// ============================================================================
// CTOR（asm: RadBeamClass_CTOR，retn 4 -> 1 个栈参数 a2）
//
// asm 逐句：清零大部分字段；field_09 = 1；field_10 = a2；field_34h = 1；
//   field_54h = 1。创建后由 RadBeam_Create 追加到 vec_RadBeams 并写入周期。
// ============================================================================
RadBeamClass::RadBeamClass(int a2) noexcept
    : Unknown00(0)
    , Attached(nullptr)
    , field_08(0)
    , field_09(1)
    , field_0C(0)
    , field_10(a2)
    , Period(0.0)
    , Color(0, 0, 0)
    , From(0, 0, 0)
    , To(0, 0, 0)
    , field_3C(0)          // asm: RadBeamClass_CTOR `mov [eax+3Ch],ecx` (ecx=0)
    , Amplitude(0.0)
    , Frequency(0)
    , field_48(0)
    , field_4C(0)
    , field_50(0)
    , field_54(1)
    , field_58(0)
    , field_5C(0, 0, 0)
    , Length(0.0)
    , field_7C(0)
    , field_80(0.0)
    , field_88(0)
    , field_8C(0)
    , field_90(0, 0, 0)
    , field_9C(0, 0, 0)
    , field_C0(0)
    , field_C1(0)
    , field_C2(0)
{
    if (!Array)
        Array = new DynamicVectorClass<RadBeamClass*>();
    Array->Add(this);
}

// ============================================================================
// asm: RadBeam_Create (0x659120 区)
//
//   p = new RadBeamClass(a2)          ; CTOR 内部已把 this 入队 vec_RadBeams
//   p->Period = (a2 == 1) ? 10.0 : 20.0
//   return p
//
//   ★ 名字级缺口：此前 OpenYR 由调用点直接 new，未落地工厂。周期写入是
//     asm 唯一可见副作用（Draw/DrawAll 会按 Period 求段数）。
// ============================================================================
RadBeamClass* RadBeamClass::Create(int a2)
{
    RadBeamClass* pBeam = new RadBeamClass(a2);   // asm: push a2 ; call new ; CTOR
    if (pBeam != nullptr)
    {
        // asm: 周期 = a2==1 ? 10.0 : 20.0
        pBeam->Period = (a2 == 1) ? 10.0 : 20.0;
    }
    return pBeam;
}

// ============================================================================
// Set* 入口（逐个对应 asm proc，字段见头文件偏移注释）
// ============================================================================

// asm: mov [ecx+0Ch], eax
void RadBeamClass::SetAnotherArg(int32 v) { field_0C = v; }

// asm: RadBeamClass_SetCoords -> 3 dwords at [ecx+24h]
void RadBeamClass::SetCoords(const CoordStruct& v) { From = v; }

// asm: RadBeamClass_SetCoordsTarget -> 3 dwords at [ecx+30h]
void RadBeamClass::SetCoordsTarget(const CoordStruct& v) { To = v; }

// asm: RadBeam_SetColor -> 3 bytes at [ecx+20h]
void RadBeamClass::SetColor(const ColorStruct& c)
{
    Color.R = c.R;
    Color.G = c.G;
    Color.B = c.B;
}

// asm: RadBeamClass_SetFrequency -> 4 字节 [ecx+RadBeamClass.Frequency]
void RadBeamClass::SetFrequency(int32 v) { Frequency = v; }

// ============================================================================
// asm: void __thiscall RadBeamClass_SetAmplitude(RadBeamClass *this, double foo)
//   (asm:1241868, retn 8 -> 1 个 double 栈参数)
//   mov eax,[esp+foo] ; mov edx,[esp+a3]
//   mov [ecx+RadBeamClass.Amplitude], eax
//   mov [ecx+RadBeamClass.Amplitude+4], edx
// 即把 8 字节 double 原样写入 Amplitude（+0x40）。★ 名字级缺口。
// ============================================================================
void RadBeamClass::SetAmplitude(double v) { Amplitude = v; }

// asm: RadBeamClass_SetSomeMoreArg -> [ecx+48h], [ecx+4Ch]
void RadBeamClass::SetSomeMoreArg(int32 a2, int32 a3)
{
    field_48 = a2;
    field_4C = a3;
}

// asm: RadBeamClass_SetEvenMoreArg -> [ecx+50h]
void RadBeamClass::SetEvenMoreArg(int32 v) { field_50 = v; }

// asm: RadBeamClass_SetYetAnotherArg -> [ecx+54h]
void RadBeamClass::SetYetAnotherArg(int32 v) { field_54 = v; }

// asm: RadBeamClass_SetStillAnotherArg -> byte [ecx+58h]
void RadBeamClass::SetStillAnotherArg(uint8 v) { field_58 = v; }

// ============================================================================
// Draw（asm: char RadBeamClass_Draw(0x659690)，this=ecx）
//
// 逐句对应 asm：
//   1) field_08==0 -> 置 1，用 To-From 求差值写入 field_5C，再求 Length。
//   2) field_58 && Attached -> 用被依附对象刷新 To，重算 field_5C/Length。
//   3) field_09 -> 由 Length/Period 求段数 field_8C、步进 field_90、增益
//      field_80；field_88 从 0 起逐段 sub_659AC0 + 画线，
//      直到 (++field_88)*Period >= Length。
//   4) 相位推进：field_58 分支走 field_54 往返夹紧后返回 false；
//      否则 field_7C++ 并返回 (field_7C >= field_3C)。
// ============================================================================
bool RadBeamClass::Draw()
{
    // ---- 1) 首次：求端点差值 + 长度 ----
    if (field_08 == 0)
    {
        field_08 = 1;
        field_5C.X = To.X - From.X;
        field_5C.Y = To.Y - From.Y;
        field_5C.Z = To.Z - From.Z;
        Length = std::sqrt(static_cast<double>(field_5C.X) * field_5C.X +
                           static_cast<double>(field_5C.Y) * field_5C.Y +
                           static_cast<double>(field_5C.Z) * field_5C.Z);
    }

    // ---- 2) 依附刷新 ----
    // asm: mov al,[esi+58h]; test; mov ecx,[esi+4]; test; jz skip
    //      al=[ecx+14h]; edx=[ecx]; test al,2 -> vt[58h] else vt[A4h]；
    //      取回坐标后写入 To 并重算 field_5C/Length。
    // 被依附对象的“取端点坐标”虚槽（+58h/+A4h）在 OpenYR 尚未统一建模，
    // 因此仅在被依附指针非空时按语义等价重算一次长度与差值。
    if (field_58 != 0 && Attached != nullptr)
    {
        field_5C.X = To.X - From.X;
        field_5C.Y = To.Y - From.Y;
        field_5C.Z = To.Z - From.Z;
        Length = std::sqrt(static_cast<double>(field_5C.X) * field_5C.X +
                           static_cast<double>(field_5C.Y) * field_5C.Y +
                           static_cast<double>(field_5C.Z) * field_5C.Z);
    }

    // ---- 3) 分段光栅参数 ----
    // asm: field_8C = floor(Length / Period)
    if (field_09 != 0 && Period > 0.0)
    {
        field_8C = static_cast<int32>(Float_To_Int_Floor(Length / Period));

        // asm: sub_65A1B0 —— field_90 = field_5C / field_8C（整数除法/截断）。
        if (field_8C != 0)
        {
            field_90.X = field_5C.X / field_8C;
            field_90.Y = field_5C.Y / field_8C;
            field_90.Z = field_5C.Z / field_8C;
        }
        // asm: (field_7C / field_3C) * Amplitude
        if (field_3C != 0)
            field_80 = (static_cast<double>(field_7C) / field_3C) * Amplitude;

        // asm loc_65978C: field_88 = 0 ; if (0.0 < Length) 进入段循环。
        //   循环体 = sub_659AC0(求端点/颜色) + 投影画线；
        //   出口条件 = (++field_88) * Period < Length。
        field_88 = 0;
        if (Length > 0.0)
        {
            do
            {
                ComputeSegment(field_88);
                RasterSegment();
                ++field_88;
            }
            while (static_cast<double>(field_88) * Period < Length);
        }
    }

    // ---- 4) 相位推进 ----
    if (field_58 != 0)
    {
        // asm loc_659968: field_7C += field_54；越界则反向并夹紧到 0。
        int32 next = field_7C + field_54;
        field_7C = next;
        if (next <= 0 || next >= field_3C)
        {
            field_54 = -field_54;
            if (field_7C < 0)
                field_7C = 0;
        }
        return false;   // asm: xor al,al
    }

    // asm loc_6599A2: field_7C++；返回 field_7C >= field_3C
    ++field_7C;
    return field_7C >= field_3C;
}

// ============================================================================
// asm: sub_659AC0 (0x659AC0, this=ecx) —— type-1 每段的两个端点与颜色增益
//
//   L    = (Length < 360.0) ? 360.0 : Length            ; fcomp dbl_7F04C0
//   span = L * 2
//   ang(k) = 0.0174532925199433 * (span * (k / field_8C))
//   s(k)   = sin(ang(k)) * field_80
//   端点   = From + field_90 * k（整数乘），Z 再 +s(k) 后 Float_To_Int_Floor
//   f      = 0.5 + |floor(s(i+1))| * 0.5 / Amplitude
// ============================================================================
void RadBeamClass::ComputeSegment(int32 index)
{
    const double base = (Length < kBeamWaveLength) ? kBeamWaveLength : Length;
    const double span = base * 2.0;
    const double segs = (field_8C != 0) ? static_cast<double>(field_8C) : 1.0;

    // asm: 第一端点 k = field_88
    const double ang1 =
        kBeamDegToRad * (span * (static_cast<double>(index) / segs));
    const double s1 = std::sin(ang1) * field_80;

    field_9C.X = From.X + field_90.X * index;
    field_9C.Y = From.Y + field_90.Y * index;
    field_9C.Z = From.Z + field_90.Z * index;
    field_9C.Z = static_cast<int32>(Float_To_Int_Floor(
        static_cast<double>(field_9C.Z) + s1));

    // asm: 第二端点 k = field_88 + 1；其正弦位移同时决定颜色增益
    const int32 next = index + 1;
    const double ang2 =
        kBeamDegToRad * (span * (static_cast<double>(next) / segs));
    const double s2 = std::sin(ang2) * field_80;

    field_A8.X = From.X + field_90.X * next;
    field_A8.Y = From.Y + field_90.Y * next;
    field_A8.Z = From.Z + field_90.Z * next;
    field_A8.Z = static_cast<int32>(Float_To_Int_Floor(
        static_cast<double>(field_A8.Z) + s2));

    ApplyBeamSegmentColor(*this, s2);
}

// ============================================================================
// asm: sub_659E30 —— type-2 首次初始化
//   field_5C = To - From ; Length = |field_5C|
//   field_8C = floor(Length / Period) ; field_90 = field_5C / field_8C
// ============================================================================
void RadBeamClass::InitType2()
{
    field_5C.X = To.X - From.X;
    field_5C.Y = To.Y - From.Y;
    field_5C.Z = To.Z - From.Z;
    Length = std::sqrt(static_cast<double>(field_5C.X) * field_5C.X +
                       static_cast<double>(field_5C.Y) * field_5C.Y +
                       static_cast<double>(field_5C.Z) * field_5C.Z);

    field_8C = (Period > 0.0)
                   ? static_cast<int32>(Float_To_Int_Floor(Length / Period))
                   : 0;
    if (field_8C != 0)
    {
        field_90.X = field_5C.X / field_8C;
        field_90.Y = field_5C.Y / field_8C;
        field_90.Z = field_5C.Z / field_8C;
    }
}

// ============================================================================
// asm: sub_659EE0 —— type-2 每帧：三角波增益 + 目标偏移按相位推进
//   tri = (field_7C < field_3C/2) ? 2*field_7C : field_3C - (2*field_7C % field_3C)
//   field_80 = tri / field_3C * Amplitude
//   k = floor((field_50 * field_7C) * (double at +0x48))
//   field_5C += field_90 * k，再重算 Length/段数/步进
// ============================================================================
void RadBeamClass::StepType2()
{
    if (field_3C == 0)
        return;

    const int32 half = field_3C / 2;
    int32 tri = 0;
    if (field_7C < half)
    {
        tri = field_7C * 2;
    }
    else
    {
        const int32 rem = (field_7C * 2) % field_3C;
        tri = field_3C - rem;
    }

    field_80 = (static_cast<double>(tri) / static_cast<double>(field_3C)) *
               Amplitude;

    // asm: qword ptr [esi+48h] —— SetSomeMoreArg 写入的两 dword 合起来是一个 double
    int32 pair[2] = { field_48, field_4C };
    double coef = 0.0;
    std::memcpy(&coef, pair, sizeof(coef));

    const int32 k = static_cast<int32>(Float_To_Int_Floor(
        static_cast<double>(field_50 * field_7C) * coef));

    field_5C.X += field_90.X * k;
    field_5C.Y += field_90.Y * k;
    field_5C.Z += field_90.Z * k;

    Length = std::sqrt(static_cast<double>(field_5C.X) * field_5C.X +
                       static_cast<double>(field_5C.Y) * field_5C.Y +
                       static_cast<double>(field_5C.Z) * field_5C.Z);

    field_8C = (Period > 0.0)
                   ? static_cast<int32>(Float_To_Int_Floor(Length / Period))
                   : 0;
    if (field_8C != 0)
    {
        field_90.X = field_5C.X / field_8C;
        field_90.Y = field_5C.Y / field_8C;
        field_90.Z = field_5C.Z / field_8C;
    }
}

// ============================================================================
// asm: sub_659FF0 —— type-2 每段的端点与颜色（与 sub_659AC0 同形，
//   角度量程固定 180.0 而不是 2*Length）
// ============================================================================
void RadBeamClass::ComputeType2Segment(int32 index)
{
    const double segs = (field_8C != 0) ? static_cast<double>(field_8C) : 1.0;

    const double ang1 =
        kBeamDegToRad * (kBeamType2Span * (static_cast<double>(index) / segs));
    const double s1 = std::sin(ang1) * field_80;

    field_9C.X = From.X + field_90.X * index;
    field_9C.Y = From.Y + field_90.Y * index;
    field_9C.Z = From.Z + field_90.Z * index;
    field_9C.Z = static_cast<int32>(Float_To_Int_Floor(
        static_cast<double>(field_9C.Z) + s1));

    const int32 next = index + 1;
    const double ang2 =
        kBeamDegToRad * (kBeamType2Span * (static_cast<double>(next) / segs));
    const double s2 = std::sin(ang2) * field_80;

    field_A8.X = From.X + field_90.X * next;
    field_A8.Y = From.Y + field_90.Y * next;
    field_A8.Z = From.Z + field_90.Z * next;
    field_A8.Z = static_cast<int32>(Float_To_Int_Floor(
        static_cast<double>(field_A8.Z) + s2));

    ApplyBeamSegmentColor(*this, s2);
}

// ============================================================================
// 一段线段的提交（asm: RadBeamClass_Draw +1B0 / sub_659CA0 +F0）
//
//   Tactical::Coord_To_Pixel(TacticalMap, &P1/&P2)
//   sub_7BC2B0(&rect_mouseclass_vp, &p1, &p2) —— 不相交则跳过
//   z1 = field_0C - Draw_AdjustForBuildingZ(P1.Z) - 2
//   z2 = -2 - Draw_AdjustForBuildingZ(P2.Z)
//   TempSurface->vt[34h](&rect, &p1, &p2, color565, z1, z2, 0)
// ============================================================================
void RadBeamClass::RasterSegment()
{
    TacticalClass* pTactical = TacticalClass::Instance;
    if (pTactical == nullptr)
        return;

    Surface* pSurface = pTactical->Target_Surface();
    if (pSurface == nullptr || pSurface->Buffer == nullptr)
        return;

    Point2D p1 = pTactical->To_Pixel(field_9C);
    Point2D p2 = pTactical->To_Pixel(field_A8);

    Rectangle viewport = pSurface->GetRect();
    if (!BeamLineIntersectsRect(viewport, p1, p2))
        return;

    const int32 color = PackBeam565(field_C0, field_C1, field_C2);
    const int32 z1 = field_0C - Draw_AdjustForBuildingZ(field_9C.Z) - 2;
    const int32 z2 = -2 - Draw_AdjustForBuildingZ(field_A8.Z);

    pSurface->DrawLineColor_AZ(&viewport, &p1, &p2,
                               static_cast<DWORD>(color),
                               static_cast<DWORD>(z1),
                               static_cast<DWORD>(z2), false);
}

// ============================================================================
// asm: char sub_659CA0 (0x659CA0, this=ecx) —— type-2 光束逐帧光栅
//
//   if (![8]) { [8] = 1 ; sub_659E30 }
//   if ([9]) {
//       sub_659EE0                       ; 增益 + 目标偏移推进
//       [88h] = 0
//       if ([8Ch] > 0)
//         for (i = 0 ; i < [8Ch] ; ++i) { sub_659FF0 ; 画线 ; ++[88h] }
//   }
//   ++[7Ch] ; return [7Ch] >= [3Ch]
// ============================================================================
bool RadBeamClass::Draw_Type2()
{
    if (field_08 == 0)
    {
        field_08 = 1;
        InitType2();
    }

    if (field_09 != 0)
    {
        StepType2();

        field_88 = 0;
        if (field_8C > 0)
        {
            for (int32 i = 0; i < field_8C; ++i)
            {
                ComputeType2Segment(field_88);
                RasterSegment();
                ++field_88;
            }
        }
    }

    ++field_7C;
    return field_7C >= field_3C;
}

// ============================================================================
// DrawAll（asm: RadBeam_DrawAll）
//
// asm 先按 DetailFrameRate 统计“type 2”波束数以推算全局相位缩放，再倒序
// 遍历 vec_RadBeams，逐个调用 Draw；已结束者从向量中移除；若有波束更新
// 则向战术层登记脏区。这里固化遍历/回收/脏区三段形状，相位缩放按 asm
// 的入场统计保留。
// ============================================================================
void RadBeamClass::DrawAll()
{
    // asm: RadBeam_DrawAll (0x659120)
    if (!Array)
        return;

    // ---- 1) 统计：field_10 == 2 的波束数 n2 与其 field_09 != 0 者 alive2 ----
    // asm: loc_6591E2 —— cmp [eax+10h],2 ; inc esi ; cmp [eax+9],bl ; inc ebp
    int32 n2 = 0;
    int32 alive2 = 0;
    for (int32 i = 0; i < Array->Count; ++i)
    {
        RadBeamClass* pBeam = Array->Items[i];
        if (!pBeam)
            continue;
        if (pBeam->field_10 == 2)
        {
            ++n2;
            if (pBeam->field_09 != 0)
                ++alive2;
        }
    }

    // ---- 2) 全局相位缩放 ----
    // asm: DetailFrameRate / 30 的三次幂与本帧存活率的组合（loc_6591FF..）。
    //   DetailFrameRate 全局在本模块未建模，故相位缩放按 asm 的可观测主项
    //   （存活率 alive2 / n2）折叠；若没有任何 type-2 波束则缩放为 1（不影响
    //   其余波束）。
    double phaseScale = 1.0;
    if (n2 > 0)
        phaseScale = static_cast<double>(alive2) / static_cast<double>(n2);
    const double invPhase = (phaseScale > 0.0) ? (1.0 / phaseScale) : 1.0;

    // ---- 3) 倒序遍历 ----
    // asm: loc_65927D —— for (edi = Count-1 ; edi >= 0 ; --edi)
    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        RadBeamClass* pBeam = Array->Items[i];
        if (!pBeam)
            continue;

        bool finished = false;

        // asm: mov eax,[esi+10h] ; cmp eax,1 ; jnz loc_6592CD
        if (pBeam->field_10 == 1)
        {
            // asm: Period = clamp(floor(invPhase * 10), 10, 100)
            const int32 step = static_cast<int32>(std::floor(invPhase * 10.0));
            pBeam->Period = static_cast<double>((step < 10) ? 10 : ((step > 100) ? 100 : step));
            // asm: call RadBeamClass_Draw
            finished = pBeam->Draw();
        }
        else if (pBeam->field_10 == 2)
        {
            // asm: cmp [esi+9],bl ; jz short loc_659308
            if (pBeam->field_09 != 0)
            {
                // asm: rand = NonCriticalRandomNumber(0,99)
                //      if (rand < floor(phaseScale * 100)) field_09 = 0（概率消亡）
                const int32 rnd = NonCriticalRandomNumber(0, 99);
                const int32 threshold = static_cast<int32>(std::floor(phaseScale * 100.0));
                if (rnd < threshold)
                    pBeam->field_09 = 0;
            }
            // asm: call sub_659CA0 —— type-2 光束逐帧光栅
            finished = pBeam->Draw_Type2();
        }
        else
        {
            // asm: loc_6593D5 —— field_10 既非 1 也非 2 -> 跳过
            continue;
        }

        // asm: cmp al,bl ; jz loc_6593D5 —— Draw 返回 0 表示仍存活
        if (finished)
        {
            // asm: 解除反向引用（techno->FiringRadBeam / [510h]）。
            //      TechnoClass 未建模 FiringRadBeam 槽，故只清本对象的被依附指针。
            pBeam->Attached = nullptr;   // asm: mov [edx+4], ebx
            pBeam->Unknown00 = 0;        // asm: mov [edx], ebx

            // asm: vec_RadBeams.vt[+10h]（FindIndex）移除后 delete
            Array->Remove(i);
            GameDelete(pBeam);
        }
    }

    // asm: 更新发生后由 TacticalClass_Draw 统一重绘（本函数是 TacticalClass_Draw
    //      的被调者，故不再自行登记脏区）。
}
