// =============================================================================
// RadarEventClass.cpp - 雷达事件（minimap 脉冲标记）
//
// 权威来源：asm RadarEventClass_Add (0x65FA80) / RadarEventClass_CTOR (0x65FAF0)
//   / sub_65FDD0 (UpdateAll) / sub_65FE00 (逐事件 Update) /
//   vec_RadarEvents_Clear (0x65FD50) / RadarEventClass_ClearVector /
//   sub_660000 (DrawAll) / sub_660050 / sub_660540 / sub_660730。
//
// 脉冲是**纯几何光栅**（不是 SHP）：sub_660730 把 (Radius,0,0) 绕 Z 轴旋转
//   Angle 后取 floor，得到菱形的四个角 (x,y)、(-y,x)、(-x,-y)、(y,-x)，
//   再平移到事件位置；sub_660540 逐帧把四边像素交给雷达脏点登记，
//   sub_660050 按事件类型取双色把四边画成渐变线。本文件按 asm 的颜色表、
//   循环边界与角度/半径公式实现等价光栅。
//
//   sub_660050 的关键数值（逐句核对）：
//     * var_84 = max(|p0.x-p1.x|, |p0.y-p1.y|)            ; 第一条边长
//     * var_78 = (Radius*2 * 0.70710678) / var_84 * ColorSpeed
//                —— dbl_7F0AF0 = sqrt(2)/2，(2*Radius*cos45)/sideLen 是
//                   “与角度无关的归一化系数”，正常姿态下恒约为 1，
//                   于是 var_78 ≈ ColorSpeed（每像素色相步长）。
//     * var_74 = [this+18h]（field_18，Update 里 0..1 三角波）为起点。
//     四条边依次以 (var_74 → var_74 + var_78) 在 c1/c2 之间插值，
//     所以整枚菱形随 field_18 在亮色与半亮色之间脉动。
//   sub_660540 则只做脏区登记（回调 loc_65FB60 → sub_6562D0 置 [Map+14D9h]），
//     不写颜色。
// =============================================================================

#include "Rendering/RadarEventClass.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/RadarClass.h"
#include "Rendering/Surface.h"
#include "Map/MapClass.h"
#include "Map/CellClass.h"
#include "Rules/RulesClass.h"
#include "Math/Timer.h"
#include "Helpers/AsmMath.h"

#include <cmath>
#include <cstdlib>

// asm: PI / 4 （dbl_7E3D88 = 0.7853981633974483）
static constexpr double kPiOverFour = 0.7853981633974483;
// asm: PI / 2 （double_Pi2 = 1.570796326794897，sub_65FE00 用于四分圈量化）
static constexpr double kPiOverTwo = 1.5707963267948966;
// asm: 2 * PI （dbl_7E3CC0 = 6.283185307179586）
static constexpr double kTwoPi = 6.283185307179586;

// asm: vec_RadarEvents
DynamicVectorClass<RadarEventClass*>* RadarEventClass::Array = nullptr;

namespace {

// ----------------------------------------------------------------------------
// RadarEventConfigs —— asm 里由 Rules 的三个按类型向量拼出的配置表：
//   field_0  使能字节         (非零表示该类型启用)
//   field_4  SuppressionDistance（去重半径）
//   field_8  VisibilityDuration （扩张结束后的存活时长）
//   field_C  Duration           （寿命）
// OpenYR 直接读取 RulesClass 的三个向量，等价于原版的 RadarEventConfigs。
// ----------------------------------------------------------------------------
struct RadarEventConfig
{
    bool  Enabled;
    int32 SuppressionDistance;
    int32 VisibilityDuration;
    int32 Duration;
};

int32 VectorAt(const DynamicVectorClass<int32>& vec, int32 index)
{
    if (index < 0 || index >= vec.Count)
        return 0;
    return vec.Items[index];
}

RadarEventConfig GetConfig(int32 type)
{
    RadarEventConfig cfg = { false, 0, 0, 0 };
    RulesClass* rules = RulesClass::Instance;
    if (rules == nullptr)
        return cfg;

    // asm: RadarEventConfigs.field_0[type*16] —— 使能由类型是否落在配置表内决定。
    cfg.SuppressionDistance = VectorAt(rules->RadarEventSuppressionDistances, type);
    cfg.VisibilityDuration  = VectorAt(rules->RadarEventVisibilityDurations, type);
    cfg.Duration            = VectorAt(rules->RadarEventDurations, type);
    cfg.Enabled = (type >= 0 &&
                   type < rules->RadarEventSuppressionDistances.Count);
    return cfg;
}

// ----------------------------------------------------------------------------
// CellScreenOffset —— asm: sub_6550C0 (0x6550C0)
//
//   逐句：
//     x = floor(((cellX - cellY) + [Map+1490h]) * [Map+1488h] + [Map+149Ch])
//     y = floor(((cellX + cellY) - [Map+1498h]) * [Map+1488h] + [Map+14A0h])
//   这是一次**雷达等距投影**（[1488h] = RadarSizeFactor，[149Ch]/[14A0h] 是
//   雷达地形面原点）。随后 CTOR 再减去雷达面原点得到 field_4 / field_8。
//   本工程用 RadarClass::WorldToRadar 做同一投影（同一公式、同一字段）。
// ----------------------------------------------------------------------------
void CellScreenOffset(int16 cellX, int16 cellY, int32& outDx, int32& outDy)
{
    outDx = 0;
    outDy = 0;

    RadarClass* pRadar = RadarClass::Instance;
    if (pRadar == nullptr)
        return;

    const CoordStruct world = CellClass::Cell2Coord(CellStruct(cellX, cellY));
    const Point2D pixel = pRadar->WorldToRadar(world);
    // asm: sub ecx, unk_880C84 / sub eax, unk_880C88 —— 减去雷达地形面原点
    outDx = pixel.X - pRadar->unknown_rect_149C.X;   // asm: [esi+4]
    outDy = pixel.Y - pRadar->unknown_rect_149C.Y;   // asm: [esi+8]
}

// ----------------------------------------------------------------------------
// 脉冲菱形的颜色（asm: sub_660050 的两个 switch，类型经 byte_660374 折叠）
//
//   type 0/3/4     -> 第一色 (FF,00,FF)，第二色 (80,00,80)
//   type 1/2/11/12 -> 第一色 (FF,FF,00)，第二色 (80,80,00)
//   type 5         -> 第一色 (00,FF,FF)，第二色 (00,80,80)
//   其余（6..10）  -> (00,00,00)：第一色全 0 时 asm 直接返回，不画。
// ----------------------------------------------------------------------------
struct PulseColor
{
    uint8 R;
    uint8 G;
    uint8 B;
};

void PulseColorsFor(int32 type, PulseColor& c1, PulseColor& c2)
{
    switch (type)
    {
    case 0: case 3: case 4:
        c1.R = 0xFF; c1.G = 0x00; c1.B = 0xFF;
        c2.R = 0x80; c2.G = 0x00; c2.B = 0x80;
        break;
    case 1: case 2: case 11: case 12:
        c1.R = 0xFF; c1.G = 0xFF; c1.B = 0x00;
        c2.R = 0x80; c2.G = 0x80; c2.B = 0x00;
        break;
    case 5:
        c1.R = 0x00; c1.G = 0xFF; c1.B = 0xFF;
        c2.R = 0x00; c2.G = 0x80; c2.B = 0x80;
        break;
    default:
        c1.R = 0x00; c1.G = 0x00; c1.B = 0x00;
        c2.R = 0x00; c2.G = 0x00; c2.B = 0x00;
        break;
    }
}

inline DWORD Pulse565(const PulseColor& c)
{
    return (static_cast<DWORD>(c.R >> 3) << 11) |
           (static_cast<DWORD>(c.G >> 2) << 5) |
            static_cast<DWORD>(c.B >> 3);
}

// 单点颜色：t 为 asm 的归一化色相（0 = c1 主色，1 = c2 半亮色）。
//   asm 的渐变线 blitter 在 RGBb 上插值，最后统一转 16bpp（RGB565）。
DWORD PulseColorAt(const PulseColor& c1, const PulseColor& c2, float t)
{
    if (t <= 0.0f)
        return Pulse565(c1);
    if (t >= 1.0f)
        return Pulse565(c2);

    const int32 r = static_cast<int32>(static_cast<float>(c1.R) +
                                       (static_cast<float>(c2.R) - c1.R) * t);
    const int32 g = static_cast<int32>(static_cast<float>(c1.G) +
                                       (static_cast<float>(c2.G) - c1.G) * t);
    const int32 b = static_cast<int32>(static_cast<float>(c1.B) +
                                       (static_cast<float>(c2.B) - c1.B) * t);
    return (static_cast<DWORD>((r >> 3) & 0x1F) << 11) |
           (static_cast<DWORD>((g >> 2) & 0x3F) << 5) |
            static_cast<DWORD>((b >> 3) & 0x1F);
}

// ----------------------------------------------------------------------------
// asm: sub_660730 —— 旋转方形的四个角（Matrix3D 绕 Z 轴旋转 Angle）
//   x = floor(Radius*cos(Angle)) ; y = floor(Radius*sin(Angle))
//   角点（asm 写入顺序）：(x,y) (-y,x) (-x,-y) (y,-x)
//   asm 之后再把四角统一加上 (field_4, field_8)，此处等价地按 center 平移。
void BuildPulseDiamond(const RadarEventClass& e, const Point2D& center, Point2D out[4])
{
    const double fx = std::cos(static_cast<double>(e.Angle)) *
                      static_cast<double>(e.Radius);
    const double fy = std::sin(static_cast<double>(e.Angle)) *
                      static_cast<double>(e.Radius);
    const int32 x = static_cast<int32>(Float_To_Int_Floor(fx));
    const int32 y = static_cast<int32>(Float_To_Int_Floor(fy));

    out[0] = Point2D(center.X + x,  center.Y + y);
    out[1] = Point2D(center.X - y,  center.Y + x);
    out[2] = Point2D(center.X - x,  center.Y - y);
    out[3] = Point2D(center.X + y,  center.Y - x);
}

// 一条边：Bresenham 逐像素，颜色按 asm 的 var_74/var_78 在 c1/c2 间插值。
//   t 从 t0 线性走到 t0 + dt（asm 把这两个 float 的地址交给渐变线 blitter）。
void PulseEdge(Surface* pSrf, const Point2D& a, const Point2D& b,
               const PulseColor& c1, const PulseColor& c2, float t0, float dt)
{
    int32 x0 = a.X;
    int32 y0 = a.Y;
    const int32 x1 = b.X;
    const int32 y1 = b.Y;

    const int32 dx = std::abs(x1 - x0);
    const int32 dy = -std::abs(y1 - y0);
    const int32 sx = (x0 < x1) ? 1 : -1;
    const int32 sy = (y0 < y1) ? 1 : -1;
    const int32 steps = (dx > -dy) ? dx : -dy;

    int32 err = dx + dy;
    int32 step = 0;

    for (;;)
    {
        const float t = t0 +
            ((steps > 0) ? dt * (static_cast<float>(step) /
                                 static_cast<float>(steps)) : 0.0f);
        Point2D pt(x0, y0);
        pSrf->SetPixel(&pt, PulseColorAt(c1, c2, t));

        if (x0 == x1 && y0 == y1)
            break;

        const int32 e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
        ++step;
    }
}

} // namespace

// ============================================================================
// RadarEventClass::RadarEventClass  (asm: RadarEventClass_CTOR, retn 8)
//
//   this = ecx；栈参 type / coords(wXY)。逐句：
//     [this+0]    = type
//     [this+10h]  = PI/4                       ; 初始角度
//     [this+14h]  = RulesData.RadarEventRotationSpeed
//     [this+18h]  = 0                          ; 颜色脉冲相位
//     [this+1Ch]  = RulesData.RadarEventColorSpeed
//     [this+20h]  = coords.X                   ; Position
//     [this+24h]  = Frame
//     [this+2Ch]  = 0
//     [this+30h]  = Frame
//     [this+38h]  = 0
//     [this+3Ch]  = 1 ; [this+3Dh] = 1
//     field_4/field_8 = 相对屏幕中心偏移；Radius = max(...) 边缘距离
//   最后把 this 追加到 vec_RadarEvents。
// ============================================================================
RadarEventClass::RadarEventClass(int32 type, int16 cellX, int16 cellY)
    : Type(type)                 // asm: mov [esi], eax
    , field_4(0)
    , field_8(0)
    , Radius(0.0f)
    , Angle(static_cast<float>(kPiOverFour))   // asm: [esi+10h] = PI/4
    , RotationSpeed(0.0f)
    , field_18(0.0f)             // asm: [esi+18h] = 0
    , ColorSpeed(0.0f)
    , CellX(cellX)
    , CellY(cellY)               // asm: [esi+20h] = coords
    , field_24(0)
    , field_28(0)
    , field_2C(0)                // asm: [esi+2Ch] = 0
    , field_30(0)
    , field_34(0)
    , field_38(0)                // asm: [esi+38h] = 0
    , field_3C(1)                // asm: [esi+3Ch] = 1
    , field_3D(1)                // asm: [esi+3Dh] = 1
    , field_3E(0)
{
    // asm: mov ecx, RulesData ; mov edx,[ecx+84h] / mov ecx,[eax+78h]
    if (RulesClass::Instance != nullptr)
    {
        RotationSpeed = RulesClass::Instance->RadarEventRotationSpeed;
        ColorSpeed    = RulesClass::Instance->RadarEventColorSpeed;
    }

    // asm: sub_6550C0 换算屏幕坐标后减去屏幕中心
    CellScreenOffset(cellX, cellY, field_4, field_8);

    // asm: 取 field_4 / field_8 / 到右边界(unk_880C8C-field_4) / 到下边界
    //      (unk_880C90-field_8) 四者中的最大值作为初始半径（fild;fstp [esi+0Ch]）。
    //   unk_880C8C/90 即雷达地形面（asm 1220h）的宽/高，本工程取 149Ch 矩形的
    //   宽高（Render_Terrain 每帧按 asm 重算）；尚未建面时退回 128x128。
    RadarClass* pRadar = RadarClass::Instance;
    int32 radarW = RadarClass::RadarWidth;
    int32 radarH = RadarClass::RadarHeight;
    if (pRadar != nullptr && pRadar->unknown_rect_149C.Width > 0 &&
        pRadar->unknown_rect_149C.Height > 0)
    {
        radarW = pRadar->unknown_rect_149C.Width;
        radarH = pRadar->unknown_rect_149C.Height;
    }

    int32 maxAbs = field_4;
    if (field_8 > maxAbs) maxAbs = field_8;
    const int32 distRight  = radarW - field_4;   // asm: ecx = unk_880C8C - edi
    const int32 distBottom = radarH - field_8;   // asm: eax = unk_880C90 - edx
    if (distRight > maxAbs) maxAbs = distRight;
    if (distBottom > maxAbs) maxAbs = distBottom;
    if (maxAbs < 0) maxAbs = 0;
    Radius = static_cast<float>(maxAbs);   // asm: fstp dword ptr [esi+0Ch]

    // asm: mov eax, Frame / mov [esi+24h],eax ; mov ecx,Frame / mov [esi+30h],ecx
    const int32 frame = FrameTimer::GetTime();
    field_24 = frame;
    field_30 = frame;

    // asm: 追加到 vec_RadarEvents（capacity 不足则扩容）
    if (!Array)
        Array = new DynamicVectorClass<RadarEventClass*>();
    Array->Add(this);
}

// ============================================================================
// asm: sub_660050 (0x660050, this=ecx) —— 按事件类型取双色，画脉冲菱形
//
//   颜色：两次 switch（byte_660374 折叠类型）→ 主色 + 半亮色；
//         主色 (0,0,0) 时 asm 在 loc_660172 前直接返回（不画）。
//   几何：sub_660730 的四角 + (field_4, field_8) 平移；
//   绘制：四条边各一次 vt[78h] 渐变线（两端色 + field_18 起、var_78 步长）。
// ============================================================================
void RadarEventClass::DrawPulse(Surface* pSrf, const Point2D& center) const
{
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return;

    PulseColor c1;
    PulseColor c2;
    PulseColorsFor(Type, c1, c2);

    // asm loc_66035C: 主色三通道全 0 -> 直接返回
    if (c1.R == 0 && c1.G == 0 && c1.B == 0)
        return;

    Point2D corner[4];
    BuildPulseDiamond(*this, center, corner);

    // asm: var_84 = max(|p0.x - p1.x|, |p0.y - p1.y|)
    int32 side = std::abs(corner[1].X - corner[0].X);
    const int32 sideY = std::abs(corner[1].Y - corner[0].Y);
    if (sideY > side)
        side = sideY;

    // asm: var_78 = (Radius*2 * 0.70710678) / var_84 * ColorSpeed
    constexpr double kCos45 = 0.7071067811865475;   // asm: dbl_7F0AF0
    float dt = 0.0f;
    if (side > 0)
    {
        dt = static_cast<float>((static_cast<double>(Radius) * 2.0 * kCos45) /
                                static_cast<double>(side)) * ColorSpeed;
    }

    // asm: var_74 = [this+18h]
    const float t0 = field_18;

    for (int32 i = 0; i < 4; ++i)
        PulseEdge(pSrf, corner[i], corner[(i + 1) & 3], c1, c2, t0, dt);
}

// ============================================================================
// asm: sub_660540 (0x660540, this=ecx) —— 逐帧脉冲的脏点登记
//
//   sub_660730 求四角 -> 加 [this+4]/[this+8] 偏移
//   for (i = 0 ; i < 4 ; ++i)
//       surface->vt[78h](&rect, &p[i], &p[i+1], loc_65FB60)   ; 逐像素
//       surface->vt[44h](surface, ret)
//   回调 loc_65FB60 即 sub_6562D0(Map, &pt)：点落在雷达面内则置位图并写
//   [Map+14D9h] = 1（雷达需要重绘）。
//   本工程雷达面每帧整面重绘，没有脏点消费者，故按 asm 的逐像素遍历等价
//   收敛为：任一边上有像素落在 128x128 雷达面内就置重绘闩锁。
// ============================================================================
void RadarEventClass::RasterPulse()
{
    RadarClass* pRadar = RadarClass::Instance;
    if (pRadar == nullptr)
        return;

    // asm: 四角直接就是 [this+4]/[this+8] + 旋转偏移，没有额外中心平移
    //   （field_4/field_8 在 CTOR 里已由 sub_6550C0 减去 [149Ch]/[14A0h]，
    //     本身就是雷达面局部坐标）。
    const Point2D center(field_4, field_8);

    Point2D corner[4];
    BuildPulseDiamond(*this, center, corner);

    // 雷达面尺寸（Render_Terrain 写回的 14A4h/14A8h）；未建面时退回 140x108。
    int32 boundW = static_cast<int32>(pRadar->unknown_rect_149C.Width);
    int32 boundH = static_cast<int32>(pRadar->unknown_rect_149C.Height);
    if (boundW <= 0) boundW = 0x8C;
    if (boundH <= 0) boundH = 0x6C;

    for (int32 i = 0; i < 4; ++i)
    {
        const Point2D& a = corner[i];
        const Point2D& b = corner[(i + 1) & 3];

        int32 x0 = a.X;
        int32 y0 = a.Y;
        const int32 x1 = b.X;
        const int32 y1 = b.Y;

        const int32 dx = std::abs(x1 - x0);
        const int32 dy = -std::abs(y1 - y0);
        const int32 sx = (x0 < x1) ? 1 : -1;
        const int32 sy = (y0 < y1) ? 1 : -1;
        int32 err = dx + dy;

        for (;;)
        {
            // asm: cmp [ebx],0 ; jl ; cmp [ebx], [121Ch]->GetWidth() ; jge
            //      行/列上界是雷达面（sub_6547C0 建的 BitW x BitH 面）尺寸。
            if (x0 >= 0 && x0 < boundW && y0 >= 0 && y0 < boundH)
            {
                pRadar->unknown_bool_14D9 = true;   // asm: mov byte [edi+14D9h],1
                return;
            }
            if (x0 == x1 && y0 == y1)
                break;

            const int32 e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

// ============================================================================
// RadarEventClass::Add  (asm: RadarEventClass_Add, this=type, coords, retn 4)
//
//   逐句：
//     eax = type << 4 ; cl = RadarEventConfigs[type].field_0
//     if (!cl) goto lct_New                  ; 该类型未启用 -> 直接新建
//     for (i in vec_RadarEvents):
//        if (event->Type != type) continue
//        dx = coords.X - event->CellX ; dy = coords.Y - event->CellY
//        dist = floor(sqrt(dx*dx + dy*dy))
//        if (dist < config.field_4) return false    ; 抑制半径内 -> 不新建
//     lct_New:
//        new RadarEventClass(type, coords) ; return true
// ============================================================================
bool RadarEventClass::Add(int32 type, int16 cellX, int16 cellY)
{
    // asm: shl eax,4 ; mov cl, RadarEventConfigs.field_0[eax] ; test cl,cl ; jz lct_New
    RadarEventConfig cfg = GetConfig(type);

    // asm: mov eax, vec_RadarEvents.Count ; xor esi,esi ; test eax,eax ; jle lct_New
    if (cfg.Enabled && Array != nullptr)
    {
        // asm: loc_65FAB1 遍历已有事件
        for (int32 i = 0; i < Array->Count; ++i)
        {
            RadarEventClass* pEvent = Array->Items[i];
            if (pEvent == nullptr)
                continue;
            // asm: mov eax,[esp+var_8] ; cmp [edx],eax ; jnz loc_65FB1A
            if (pEvent->Type != type)
                continue;

            // asm: cx = coords.Y - event->CellY ; ax = coords.X - event->CellX
            const int32 dx = static_cast<int32>(cellX) - pEvent->CellX;
            const int32 dy = static_cast<int32>(cellY) - pEvent->CellY;
            // asm: float_sqrt(dx*dx + dy*dy) ; Float_To_Int_Floor
            const double dist = std::sqrt(static_cast<double>(dx * dx + dy * dy));
            const int32 distI = static_cast<int32>(std::floor(dist));

            // asm: cmp eax, ds:dword_7F0998[edx] ; jl loc_65FB4A (return false)
            if (distI < cfg.SuppressionDistance)
                return false;   // 抑制半径内，不重复登记
        }
    }

    // asm: lct_New: push size / call new / RadarEventClass_CTOR(type,coords)
    //      mov al,1 / retn 4
    new RadarEventClass(type, cellX, cellY);
    return true;
}

// ============================================================================
// RadarEventClass::Update  (asm: sub_65FE00)
//
//   逐段（与 asm 一一对应）：
//     if (![this+3Dh]) return 0                      ; 已死不更新
//     if (![this+3Ch]) {                             ; 已停止扩张 -> 寿命检查
//        stamp = [this+30h] ; dur = [this+38h]
//        if (stamp != -1) { if (Frame-stamp >= dur) 3Dh = 0 }
//        else if (dur == 0) 3Dh = 0
//     }
//     sub_660540(this)                               ; 脏点登记
//     Radius = max(Radius - RulesData+80h, (float)RulesData+7Ch)
//              —— asm 的 fcom/test ah,41h 在 ST>mem 时保留 shrunk，
//                 否则换成 minR，等价于取大者（半径从边缘向内收缩）。
//     residual = (Angle + PI/4) - floor((Angle+PI/4)*2/PI) * (PI/2)
//              —— 即 (Angle+PI/4) 对 PI/2 取余，量化到"四分圈"内的相位。
//     if (!3Ch)                          -> 不推进角度（loc_65FF68）
//     else if (|MinRadius - residual| < 0.01) -> Angle += RotationSpeed
//     else if (residual >= RotationSpeed) {
//              Angle += RotationSpeed
//              RotationSpeed = max(base/3, RotationSpeed - base*0.02)  ; 减速
//          }
//     else { Angle += residual ; 3Ch = 0 ;
//            30h = Frame ; 38h = VisibilityDuration ;
//            24h = Frame ; 2Ch = Duration }        ; 扩张结束，进入可见期
//     if (Angle > 2*PI) Angle -= 2*PI
//     field_18 += ColorSpeed ; 0/1 越界反弹（fchs 反向 + 夹到端点）
//   返回 [this+3Dh] != 0（是否仍存活）。
// ============================================================================
bool RadarEventClass::Update()
{
    // asm: mov al,[esi+3Dh] ; test ; jz loc_65FFEE
    if (!field_3D)
        return false;

    // asm: mov al,[esi+3Ch] ; test ; jnz loc_65FE39  （仍扩张则跳过寿命检查）
    if (!field_3C)
    {
        // asm: mov edx,[esi+30h] ; mov eax,[esi+38h] ; cmp edx,-1
        const int32 stamp = field_30;
        const int32 dur   = field_38;
        if (stamp != -1)
        {
            // asm: mov ecx,Frame ; sub ecx,edx ; cmp ecx,eax ; jge loc_65FE35
            const int32 elapsed = FrameTimer::GetTime() - stamp;
            if (elapsed >= dur)
            {
                field_3D = 0;                 // asm: loc_65FE35 -> mov byte [esi+3Dh],0
                return false;
            }
        }
        else if (dur == 0)
        {
            // asm: loc_65FE31 test eax,eax ; jnz loc_65FE39 ; else 3Dh = 0
            field_3D = 0;
            return false;
        }
    }

    // asm: call sub_660540 —— 逐帧把脉冲菱形交给雷达脏点登记
    RasterPulse();
    RadarEventConfig cfg = GetConfig(Type);

    float minR = 0.0f;
    float rotationSpeed = RotationSpeed;

    // asm: fld [esi+0Ch] ; fsub RulesData+80h ; fild RulesData+7Ch
    //      ; fcom / ST>mem 保留 shrunk，否则换成 minR ; fst [esi+0Ch]
    if (RulesClass::Instance != nullptr)
    {
        const float speed = RulesClass::Instance->RadarEventSpeed;
        minR = static_cast<float>(RulesClass::Instance->RadarEventMinRadius);
        const float shrunk = Radius - speed;
        Radius = (shrunk > minR) ? shrunk : minR;   // asm: max(shrunk, minR)
    }

    // asm: fld [esi+10h] ; fadd PI/4 ; fld st ; fmul (2/PI) ; Float_To_Int_Floor
    //      ; fild n ; fmul PI/2 ; fsubr st, st(1)   -> residual
    const float a = Angle + static_cast<float>(kPiOverFour);
    const float n = static_cast<float>(static_cast<int32>(Float_To_Int_Floor(
        static_cast<double>(a) * (2.0 / 3.14159265358979323846))));
    const float residual = a - n * static_cast<float>(kPiOverTwo);

    if (!field_3C)
    {
        // asm: loc_65FF68 —— 扩张结束后不再推进角度
    }
    else if (std::fabs(minR - residual) < 0.01f)
    {
        // asm: loc_65FF5D —— 已贴近四分圈边界，仅旋转
        Angle += RotationSpeed;
    }
    else if (residual >= RotationSpeed)
    {
        // asm: loc_65FF1A —— EAX = Angle + RotationSpeed ; 随后减速
        Angle += RotationSpeed;
        rotationSpeed = RotationSpeed;

        if (RulesClass::Instance != nullptr)
        {
            const float base = RulesClass::Instance->RadarEventRotationSpeed;
            const float floorSpeed = base * 0.33333334f;      // asm: num_0_3_0
            const float decel      = base * 0.02f;            // asm: flt_7F0AE8
            const float slowed     = rotationSpeed - decel;
            // asm: fcom var_10 ; ST>mem 保留 base/3，否则取 slowed -> 取大者
            RotationSpeed = (floorSpeed > slowed) ? floorSpeed : slowed;
        }
    }
    else
    {
        // asm: fld residual ; fadd [esi+10h] ; fstp [esi+10h] —— 补齐到四分圈
        Angle += residual;
        field_3C = 0;                                  // asm: mov byte [esi+3Ch],0
        const int32 frame = FrameTimer::GetTime();
        field_30 = frame;                              // asm: mov [edi],ecx  (edi=esi+30h)
        field_38 = cfg.VisibilityDuration;             // asm: mov [edi+8],edx
        field_24 = frame;                              // asm: mov [edx],ecx  (edx=esi+24h)
        field_2C = cfg.Duration;                       // asm: mov [edx+8],eax
    }

    // asm: loc_65FF6A —— if (Angle > 2*PI) Angle -= 2*PI
    if (Angle > static_cast<float>(kTwoPi))
        Angle -= static_cast<float>(kTwoPi);

    // asm: fld [esi+1Ch] ; fadd [esi+18h] ; fst [esi+18h] ; 越界反弹
    field_18 += ColorSpeed;
    if (field_18 < 0.0f && ColorSpeed < 0.0f)
    {
        // asm: fchs ColorSpeed ; [esi+18h] = 0
        ColorSpeed = -ColorSpeed;
        field_18 = 0.0f;
    }
    else if (field_18 > 1.0f && ColorSpeed > 0.0f)
    {
        // asm: fchs ColorSpeed ; [esi+18h] = 1
        ColorSpeed = -ColorSpeed;
        field_18 = 1.0f;
    }

    return field_3D != 0;
}

// ============================================================================
// RadarEventClass::UpdateAll  (asm: sub_65FDD0)
//   遍历 vec_RadarEvents，逐个调用 Update（asm 里是 call sub_65FE00）。
// ============================================================================
void RadarEventClass::UpdateAll()
{
    if (!Array)
        return;
    for (int32 i = 0; i < Array->Count; ++i)
    {
        RadarEventClass* pEvent = Array->Items[i];
        if (pEvent != nullptr)
            pEvent->Update();
    }
}

// ============================================================================
// RadarEventClass::Remove  (asm: RadarEventClass_ClearVector)
//   从 vec_RadarEvents 里摘除并 delete 单个事件。
// ============================================================================
void RadarEventClass::Remove(RadarEventClass* pEvent)
{
    if (!Array || !pEvent)
        return;
    // asm: vec_RadarEvents.vt[+10h]（FindIndex）—— 线性查找等价实现
    int32 idx = -1;
    for (int32 i = 0; i < Array->Count; ++i)
    {
        if (Array->Items[i] == pEvent)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return;
    Array->Remove(idx);
    delete pEvent;
}

// ============================================================================
// RadarEventClass::ClearVector  (asm: vec_RadarEvents_Clear, 0x65FD50)
//   逆序遍历，逐个摘除并 delete；对应 asm 的
//     for (i = Count-1 ; i >= 0 ; --i) { Remove(vec[i]) ; delete vec[i]; }
// ============================================================================
void RadarEventClass::ClearVector()
{
    if (!Array)
        return;
    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        RadarEventClass* pEvent = Array->Items[i];
        if (!pEvent)
            continue;
        Array->Remove(i);
        delete pEvent;
    }
    Array->Clear();
}
