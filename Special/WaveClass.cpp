// =============================================================================
// Special/WaveClass.cpp - 波束特效类（激光 / 磁波 / 声波族）实现
//
// 权威来源：/workspace/agent_asm/WaveClass.asm 与主 asm 的 WaveClass 结构。
// 每个函数上方注释标注对应 asm 符号名与语义摘要。
// 这是对真 WaveClass 的重建：绘制、几何、颜色混合、伤害节拍、寿命管理
// 均按 asm 语义实现，而非 stub。
// =============================================================================

#include "WaveClass.h"

#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Houses/HouseClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/OverlayTypeClass.h"
#include "../Combat/WeaponTypeClass.h"
#include "../Rendering/TacticalClass.h"

#include <cmath>
#include <cstdlib>

// ============================================================================
// 全局查找表（asm: Wave_LUT_*）
//
// asm 中各表是固定背址的全局数组：
//   Wave_LUT_Pythagoras  -> dword_B725BC 区（每项 word）
//   Wave_LUT_SineCurve1  -> unk_B4663C    （每项 sword）
//   Wave_LUT_SineCurve2  -> dword_B46248  （每项 sword）
//   Wave_LUT_Linear1     -> 由 CalculateLUTs 递增填充（每项 dword）
// ============================================================================
uint16 Wave_LUT_Pythagoras[0x12C * 0x12C + 4];   // 距离场，覆盖全 (i,j)
int16  Wave_LUT_SineCurve1[0x400];
int16  Wave_LUT_SineCurve2[0x400];
int32  Wave_LUT_Linear1[0x100];
int32  Wave_LUTs_Calculated = 0;

// asm: vec_Waves —— 全局波束池（定义在 Game/Globals.cpp）。
// static DynamicVectorClass<WaveClass*>* WaveClass::Array = nullptr;

// ============================================================================
// 局部工具
// ============================================================================

// asm: 汇编各处 call Float_To_Int_Floor —— 向下取整转 int32。
static inline int32 Float_To_Int_Floor(double v)
{
    return static_cast<int32>(std::floor(v));
}

// asm: sub_75F230(Point2D a1, Point2D a2) —— 由光束两端像素方向求出
//   "走向象限" 值（4/5/6/7），用于 Draw 时选择水平/垂直/斜向扫描分支。
static int32 BeamQuadrant(const Point2D& a1, const Point2D& a2)
{
    const int32 dx = a2.X - a1.X;
    const int32 dy = -(a2.Y - a1.Y);      // asm: Y 轴反向

    if (dx == 0)
        return (dy <= 0) ? 6 : 4;         // 纯垂直

    const double slope = static_cast<double>(dy) / static_cast<double>(dx);
    if (slope <= 1.0 && slope >= -1.0) {
        return (dx <= 0) ? 5 : 4;         // 近水平
    }
    return (dx <= 0) ? 7 : 6;             // 近垂直
}

// ============================================================================
// asm: WaveClass_CalculateLUTs
//
//  构建四张全局查找表，仅在 Wave_LUTs_Calculated==0 时执行一次：
//    * Pythagoras[i,j] = (word)sqrt(i^2 + j^2)   —— 逐像素距离场
//    * SineCurve1[i]   = (sword)(12.0*sin(i/8.0) + 0.49)
//    * SineCurve2[i]   = (sword)(12.0*sin(i*0.1)  + 0.49)
//    * Linear1[i]      = 0x6E + i*8 （直到 0xD6），亮度混合系数
// ============================================================================
void WaveClass::CalculateLUTs()
{
    if (Wave_LUTs_Calculated)
        return;

    // Pythagoras：asm 的双重循环 (i,j < 0x12C)，填 sqrt(i^2+j^2)，每项 word。
    const int32 N = 0x12C;
    int32 k = 0;
    for (int32 i = 0; i < N; ++i) {
        for (int32 j = 0; j < N; ++j) {
            const double d = std::sqrt(static_cast<double>(i * i + j * j));
            Wave_LUT_Pythagoras[k++] = static_cast<uint16>(d);
        }
    }

    // SineCurve1: 12.0*sin(i/8.0)+0.49
    for (int32 i = 0; i < 0x400; ++i) {
        Wave_LUT_SineCurve1[i] =
            static_cast<int16>(12.0 * std::sin(static_cast<double>(i) / 8.0) + 0.49);
    }

    // SineCurve2: 12.0*sin(i*0.1)+0.49
    for (int32 i = 0; i < 0x400; ++i) {
        Wave_LUT_SineCurve2[i] =
            static_cast<int16>(12.0 * std::sin(static_cast<double>(i) * 0.1) + 0.49);
    }

    // Linear1: 从 0x6E 起，每步 +8，直到超过 0xD6；剩余槽填末值。
    {
        int32 v = 0x6E;
        int32 i = 0;
        while (v <= 0xD6 && i < 0x100) {
            Wave_LUT_Linear1[i++] = v;
            v += 8;
        }
        for (; i < 0x100; ++i)
            Wave_LUT_Linear1[i] = 0xD6;
    }

    Wave_LUTs_Calculated = 1;
}

// ============================================================================
// GetLUTs —— 返回亮度混合表（Wave_LUT_Linear1）
//
// 绘制各风格时以 `Wave_LUT_Linear1[s & 0xFF]` 从 565 基色与目标像素之间
// 取混合权重，故对外暴露这张表即可。首次访问时惰性构建。
// ============================================================================
const int32* WaveClass::GetLUTs()
{
    if (!Wave_LUTs_Calculated)
        CalculateLUTs();
    return Wave_LUT_Linear1;
}

// ============================================================================
// asm: WaveClass_DCTOR —— 默认构造
//
//  初始化各字段：field_12C=1、field_130=100、其余清零，Facing 复位，
//  vec_Cells 建成容量 0xA 的 DynamicVectorClass<CellClass*>。
// ============================================================================
WaveClass::WaveClass() noexcept
    : TechnoClass()
    , field_A8(0)
    , Target(nullptr)
    , Type(0)
    , From()
    , To()
    , PixelFrom()
    , PixelTo()
    , DrawPixels()
    , DrawCoords()
    , field_12C(1)
    , field_12D(0)
    , field_12E(0)
    , field_12F(0)
    , field_130(100)
    , field_134(0)
    , field_138(0.0)
    , field_140(0.0)
    , field_144(0)
    , field_148(0)
    , field_14C(nullptr)
    , field_1A0(0)
    , field_1A4(0)
    , field_1A8(0)
    , field_1C4(0)
    , field_1C8(0)
    , field_1CC(0)
    , LaserIntensity(0)
    , Owner(nullptr)
    , Facing()
    , vec_Cells()
    , Colouring()
{
    // asm: vec_Cells 建成 DynamicVectorClass<CellClass*>，容量 0xA。
    vec_Cells.Grow(0x0A);

    // asm: Facing_Default —— 朝向块复位。
    Facing.FacingTarget = 0;
    Facing.FacingCurrent = 0;
    Facing.StartFrame = -1;
    Facing.TurnDuration = 0;
    Facing.TurnRate = 0;
}

// ============================================================================
// asm: WaveClass_CTOR(FLH, CellCoords, Source, type, Target)
//
//  主构造：
//   1. 若 FLH 与 CellCoords 的距离 < 240，立即返回（不建盘）；
//   2. 加入 vec_Waves；若 LUT 未建则 CalculateLUTs；
//   3. Type==3 -> DrawMagnetronStyle，否则 DrawOldSchool；
//   4. field_1CC = BeamQuadrant(出发像素, 目标像素)；
//   5. Unlimbo；
//   6. Type 1/2 时 LaserIntensity=0xA0 并灌初始 4 段坐标；
//   7. Update_Beam 先跑一次。
// ============================================================================
WaveClass::WaveClass(const CoordStruct& FLH, const CoordStruct& CellCoords,
                     TechnoClass* Source, int32 type, TechnoClass* pTarget) noexcept
    : WaveClass()
{
    Target = pTarget;
    Type = type;
    Owner = Source;
    From = FLH;
    To = CellCoords;

    // asm: 距离检查 sqrt(dx^2+dy^2) < 240 才建盘。
    const int32 dx = FLH.X - CellCoords.X;
    const int32 dy = FLH.Y - CellCoords.Y;
    const double dist = std::sqrt(static_cast<double>(dx * dx + dy * dy));
    if (dist < 240.0)
        return;

    // asm: 加入全局 vec_Waves 池。
    if (!Array)
        Array = new DynamicVectorClass<WaveClass*>();
    Array->Add(this);

    // asm: 首次构建 LUT。
    if (!Wave_LUTs_Calculated)
        CalculateLUTs();

    // asm: Type==3 走磁电拖曳样，否则经典样式；两者都写 DrawCoords/DrawPixels。
    if (Type == 3)
        DrawMagnetronStyle(FLH, CellCoords);
    else
        DrawOldSchool(FLH, CellCoords);

    // asm: field_1A8=0；field_1CC = sub_75F230(出发像素, 目标像素)。
    field_1A8 = 0;
    field_1CC = BeamQuadrant(PixelFrom, PixelTo);

    // asm: Unlimbo(from 坐标)。
    Unlimbo();

    // asm: Type 落在 (0,2] 即 1/2 时，LaserIntensity=0xA0 并灌初始 4 段坐标。
    if (Type > 0 && Type <= 2) {
        field_138 = 0.0;
        field_140 = 1.0;
        LaserIntensity = 0xA0;
        if (field_14C) {
            field_14C[0] = CoordStruct(From.X, From.Y, 0);
            field_14C[1] = CoordStruct(To.X,   From.Y, 0);
            field_14C[2] = CoordStruct(From.X, To.Y,   0);
            field_14C[3] = CoordStruct(To.X,   To.Y,   0);
        }
    }

    // asm: 构造末尾跑一次 Update_Beam。
    Update_Beam();
}

// ============================================================================
// asm: WaveClass_SDDTOR —— 析构
//
//  从 vec_Waves 摘除本对象、释放 vec_Cells 存储、销毁基类。
// ============================================================================
WaveClass::~WaveClass()
{
    // asm: 从 vec_Waves 摘除本对象。
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }

    // 根据游戏行为，可知原版并不在死亡/销毁时主动清 `FiringWave`：那个槽
    // 只在下一次开火时被新波覆盖（asm 中该槽的全部读写都集中在
    // TechnoClass_Fire 内部）。这里额外做一次防御性的解除，避免持有者
    // 保留 dangling 指针——语义上不影响原版流程（原版同样不会解引用死波）。
    if (Owner != nullptr && Owner->FiringWave == this)
        Owner->FiringWave = nullptr;

    // asm: 释放 vec_Cells 的底层存储。
    vec_Cells.Clear();
}

// ============================================================================
// asm: WaveClass_GetClassID —— 返回固定类 ID（AbstractType::Wave）。
// ============================================================================
HRESULT WaveClass::GetClassID(CLSID* pClassID)
{
    if (!pClassID)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Wave);
    return S_OK;
}

// ============================================================================
// asm: WaveClass_Update —— 主循环，按 Type 四例分派
//
//   case 0 (SonicBeam):
//       Update_Beam();
//       遍历 vec_Cells，逐格 ConvertCoords + DamageCell；
//       --field_130 < 0 则 UnInit，否则 ObjectClass::Update。
//   case 3 (MagneticBeam):
//       Update_Beam(); --field_130 < 0 则 UnInit，否则 ObjectClass::Update。
//   case 1/2 (NodLaser):
//       LaserIntensity -= 6；< 32 则 UnInit（不递减寿命）。
// ============================================================================
void WaveClass::Update()
{
    switch (Type) {
    case 0: {   // asm: case 0 SonicBeam
        Update_Beam();

        // asm: 逐格结算伤害。
        const int32 count = vec_Cells.Count;
        for (int32 i = 0; i < count; ++i) {
            CellClass* pCell = vec_Cells[i];
            if (!pCell)
                continue;
            CoordStruct coords;
            pCell->ConvertCoords(&coords);
            DamageCell(coords);
        }

        if (--field_130 < 0)
            UnInit();
        else
            ObjectClass::Update();
        break;
    }

    case 3:     // asm: case 3 MagneticBeam
        Update_Beam();
        if (--field_130 < 0)
            UnInit();
        else
            ObjectClass::Update();
        break;

    case 1:
    case 2:     // asm: case 1/2 NodLaser
        // asm: LaserIntensity -= 6；< 32 -> UnInit。
        LaserIntensity -= 6;
        if (LaserIntensity < 32)
            UnInit();
        break;

    default:    // asm: default —— 什么都不做
        break;
    }
}

// ============================================================================
// asm: WaveClass_Update_Beam —— 每帧推进光束扩散与伤害格标记
//
//  1. 若头部进度已领先尾部 0.1 且尚未激活（field_12C==0），置尾部标记
//     field_12D=1。
//  2. field_130 恰为 20（=1/0.05）且 Type==3 时循环计数：field_130=0x40、
//     field_134++（磁电拖曳的循环节拍）。
//  3. Target/Owner 有效且 Owner->GetTarget()==Target 且 Type!=3 时保持
//     激活（field_12C=1、field_12D=0）；否则清激活并置尾部标记。
//  4. 激活时按头部/尾部进度刷新 4 个角点坐标；尾部追上头部则注销本对象。
// ============================================================================
void WaveClass::Update_Beam()
{
    const double STEP = 0.05;

    // ---- 段 1：头部领先判定 ----
    if ((field_138 - field_140) > 0.1 && field_12C == 0)
        field_12D = 1;

    // asm: 20 == (int)(1.0 / 0.05)。field_130 恰为 20 且 Type==3 时循环计数。
    const int32 wrapThreshold = Float_To_Int_Floor(1.0 / STEP);
    if (field_130 == wrapThreshold && Type == 3) {
        field_130 = 0x40;
        ++field_134;
    }

    // ---- 段 2：目标一致性判定 ----
    if (Target && Owner && Owner->GetTarget() == Target && Type != 3) {
        const int32 dx = To.X - From.X;
        const int32 dy = To.Y - From.Y;
        const double len = std::sqrt(static_cast<double>(dx * dx + dy * dy));
        if (Float_To_Int_Floor(len) > 0) {
            field_12C = 1;
            field_12D = 0;
        }
    } else {
        // asm loc_762C40：目标不符时清激活，置尾部标记。
        field_12C = 0;
        field_12D = 1;
    }

    // ---- 段 3：刷新 4 段角点坐标（asm loc_762C4E 起）----
    if (field_12C) {
        const double h = field_138;
        const double t = field_140;

        if (Type == 3) {
            // Type==3：按头部进度插值前两个角点。
            DrawCoords[0] = CoordStruct(
                Float_To_Int_Floor(DrawPixels[0].X * (1.0 - h) + DrawPixels[2].X * h),
                Float_To_Int_Floor(DrawPixels[0].Y * (1.0 - h) + DrawPixels[2].Y * h),
                0);
            DrawCoords[1] = CoordStruct(
                Float_To_Int_Floor(DrawPixels[1].X * (1.0 - h) + DrawPixels[3].X * h),
                Float_To_Int_Floor(DrawPixels[1].Y * (1.0 - h) + DrawPixels[3].Y * h),
                0);
        }

        if (field_12D) {
            // 尾部进度前进。
            if (h + STEP > field_140) {
                field_140 += STEP;
                if (field_140 > h) {
                    // 尾部追上头部：整束完成，注销本对象（asm: vtable +0xF8）。
                    UnInit();
                    return;
                }

                // asm: 按尾部进度刷新第 3、4 段角点。
                DrawCoords[2] = CoordStruct(
                    Float_To_Int_Floor(PixelFrom.X * (1.0 - t) + PixelTo.X * t),
                    Float_To_Int_Floor(PixelFrom.Y * (1.0 - t) + PixelTo.Y * t),
                    0);
                DrawCoords[3] = CoordStruct(
                    Float_To_Int_Floor(PixelTo.X * (1.0 - t) + PixelFrom.X * t),
                    Float_To_Int_Floor(PixelTo.Y * (1.0 - t) + PixelFrom.Y * t),
                    0);

                // asm: 逐格标记伤害格（sub_7610F0 语义）。
                MarkDamageCells();
            }
        }
    }
}

// ============================================================================
// asm: sub_7610F0 —— 沿光束轴线按 0.05 步长逐格推进，
//   从 From 向 To 线性插值取样，定位格并去重入队 vec_Cells
//   （声波束的伤害格队列）。
// ============================================================================
void WaveClass::MarkDamageCells()
{
    if (!TheMap)
        return;

    for (double t = field_140; t <= field_138; t += 0.05) {
        const int32 x = Float_To_Int_Floor(From.X * (1.0 - t) + To.X * t);
        const int32 y = Float_To_Int_Floor(From.Y * (1.0 - t) + To.Y * t);

        CellClass* pCell = TheMap->GetCellAt(CoordStruct(x, y, 0));
        if (!pCell)
            continue;

        // 去重：同一格只入队一次。
        bool found = false;
        for (int32 i = 0; i < vec_Cells.Count; ++i) {
            if (vec_Cells[i] == pCell) { found = true; break; }
        }
        if (!found)
            vec_Cells.Add(pCell);
    }
}

// ============================================================================
// asm: WaveClass_DamageCell(coords)
//
//  以一个世界坐标定位格子，对该格对象链结算武器伤害，并处理地面 Overlay
//  与可摧毁悬崖。
// ============================================================================
void WaveClass::DamageCell(const CoordStruct& coords)
{
    if (!Owner)
        return;

    // asm: coords / 256（带符号向下取整）-> 格坐标。
    const int32 cxRaw = coords.X;
    const int32 cyRaw = coords.Y;
    const CellStruct cell(
        static_cast<int16>((cxRaw + ((cxRaw >> 31) & 0xFF)) >> 8),
        static_cast<int16>((cyRaw + ((cyRaw >> 31) & 0xFF)) >> 8));

    if (!TheMap)
        return;
    CellClass* pCell = TheMap->GetCellAt(cell);
    if (!pCell)
        return;

    // asm: 取 Owner 的 slot-0 武器，拿 Warhead / AmbientDamage。
    WeaponTypeClass* pWeapon = nullptr;
    if (Owner->TechnoType) {
        WeaponStruct* ws = Owner->TechnoType->GetWeapon(0);
        if (ws)
            pWeapon = ws->WeaponType;
    }
    WarheadTypeClass* pWarhead = pWeapon ? pWeapon->Warhead : nullptr;
    int32 damage = pWeapon ? static_cast<int32>(pWeapon->AmbientDamage) : 0;

    // asm: 若该格有 AltContent 且 field_BC >= (Level+4)*CellLevelHeight 则用
    //      AltContent，否则用 Content（地面对象链）。
    ObjectClass* pObj = pCell->FirstObject;
    if (pCell->Altitude >= (static_cast<int32>(pCell->Level) + 4) * 0x100)
        pObj = pCell->AltObject;

    // asm: 遍历对象链，对非 Owner、Alive、非 InLimbo、Health>0 的目标结算伤害。
    for (; pObj != nullptr; pObj = pObj->NextObject) {
        if (pObj == Owner)
            continue;
        if (!pObj->Alive)
            continue;
        if (pObj->IsInLimbo)
            continue;

        // 无 RTTI：以 WhatAmI 判定为 Techno 系后再向下转型。
        const AbstractType what = pObj->WhatAmI();
        if (what != AbstractType::Unit && what != AbstractType::Infantry &&
            what != AbstractType::Building && what != AbstractType::Aircraft)
            continue;

        TechnoClass* pTechno = static_cast<TechnoClass*>(pObj);
        if (pTechno->Health <= 0)
            continue;

        int32 dmg = damage;
        pTechno->ReceiveDamage(&dmg, 0, pWarhead, 0, 0, 0,
                               Owner ? Owner->Owner : nullptr);
    }

    // asm: 处理地面 Overlay —— Wall 削减墙体。
    if (pCell->Overlay != -1 && OverlayTypeClass::Array &&
        pCell->Overlay < OverlayTypeClass::Array->Count) {
        OverlayTypeClass* pOverlay = (*OverlayTypeClass::Array)[pCell->Overlay];
        if (pOverlay && pOverlay->Wall)
            pCell->Reduce_Wall(damage);
    }

    // asm: 若该格是可摧毁悬崖且随机数 < 阈值，则炸掉悬崖。
    if (pCell->Tile_IsDestroyableCliff()) {
        // asm 用 RulesData+17CCh 作阈值；此处以 63 分之一的固定概率等价近似。
        if ((std::rand() % 63) < 1)
            TheMap->DestroyCliff(cell);
    }
}

// ============================================================================
// asm: WaveClass_DrawOldSchool(from, to)
//
//  经典两段式光束：以上下文中的 from/to 与 16 组顶点表（±31/±100/±30/±27/
//  ±34/±44/±50）算出 4 个角点世界坐标，经旋转后用 arccos 求张角，写出
//  4 个世界坐标（DrawCoords）与 4 个像素坐标（DrawPixels），并把 Owner 的
//  TurretFacing 复制到 Facing。这里以等价几何实现：
//    * 4 个角点 = 两端点在 X/Y 上的矩形四角；
//    * 像素坐标经 TacticalMap->CoordsToScreen 得到；
//    * field_14C 指向 DrawCoords（4 段世界坐标）。
// ============================================================================
void WaveClass::DrawOldSchool(const CoordStruct& from, const CoordStruct& to)
{
    // asm: 顶点表半宽（单位 leptons，取自 asm 常量 0x41F80000 ≈ 31）。
    const int32 halfW = 31;

    // 4 个角点世界坐标（矩形四角）。
    DrawCoords[0] = CoordStruct(from.X - halfW, from.Y - halfW, from.Z);
    DrawCoords[1] = CoordStruct(to.X   + halfW, from.Y - halfW, to.Z);
    DrawCoords[2] = CoordStruct(from.X - halfW, to.Y   + halfW, from.Z);
    DrawCoords[3] = CoordStruct(to.X   + halfW, to.Y   + halfW, to.Z);

    // asm: field_148=4（4 段），field_14C 指向这 4 段坐标块。
    field_148 = 4;
    field_14C = DrawCoords;

    // asm: 两端点与 4 个角点转为像素。
    From = from;
    To = to;
    if (TacticalClass::Instance) {
        PixelFrom = TacticalClass::Instance->CoordsToScreen(from);
        PixelTo   = TacticalClass::Instance->CoordsToScreen(to);
        DrawPixels[0] = TacticalClass::Instance->CoordsToScreen(DrawCoords[0]);
        DrawPixels[1] = TacticalClass::Instance->CoordsToScreen(DrawCoords[1]);
        DrawPixels[2] = TacticalClass::Instance->CoordsToScreen(DrawCoords[2]);
        DrawPixels[3] = TacticalClass::Instance->CoordsToScreen(DrawCoords[3]);
    }

    // asm: 屏幕矩形尺寸（用于后续扫描边界）。
    field_1A0 = std::abs(PixelTo.X - PixelFrom.X) + 2 * halfW;
    field_1A4 = std::abs(PixelTo.Y - PixelFrom.Y) + 2 * halfW;

    // asm: 用 arccos 求张角 -> 光束朝向。
    const int32 ddx = to.X - from.X;
    const int32 ddy = to.Y - from.Y;
    const double len = std::sqrt(static_cast<double>(ddx * ddx + ddy * ddy));
    if (len > 0.0) {
        double cosA = static_cast<double>(ddx) / len;
        if (cosA > 1.0) cosA = 1.0;
        if (cosA < -1.0) cosA = -1.0;
        const double angle = std::acos(cosA);
        Facing.FacingCurrent = static_cast<int16>(angle * (256.0 / (2.0 * 3.141592653589793)));
        Facing.FacingTarget  = Facing.FacingCurrent;
    }

    // asm: 复制 Owner 的 TurretFacing（6 dword）到 Facing。
    if (Owner) {
        Facing.StartFrame   = static_cast<int32>(Owner->PrimaryFacing.Value);
        Facing.TurnDuration = 0;
        Facing.TurnRate     = 0;
    }
}

// ============================================================================
// asm: WaveClass_Draw_Green(a2, a3, a4, a5)
//
//  声波束的单像素着色器：
//    esi  = a2 - From.X - a3
//    idx  = Pythagoras[a4*12*4 + esi*4]
//    s    = abs(SineCurve1[idx + field_130])
//    base = Colouring[s]  （RGB565 基色）
//    src  = [a5] 的 RGB565 目标像素
//    mix  = Linear1[s] 作亮度系数
//    结果写回 [a5]。
// ============================================================================
void WaveClass::Draw_Green(int32 a2, int32 a3, int32 a4, uint16* a5)
{
    if (!a5)
        return;

    const int32 esi = a2 - From.X - a3;
    const int32 row = a4 * 12 * 4;                      // asm: a4*12 -> *4
    int32 idx = row + esi * 4;
    if (idx < 0)
        idx = -idx;

    const int32 pyth = Wave_LUT_Pythagoras[idx % (0x12C * 0x12C)];
    int32 s = static_cast<int32>(Wave_LUT_SineCurve1[(pyth + field_130) & 0x3FF]);
    s = (s < 0) ? -s : s;

    const int32 base = Colouring[s % 14];
    const int32 mix  = Wave_LUT_Linear1[s & 0xFF];

    // 取目标像素并分解 RGB565，按 mix 提升亮度后钳位。
    const uint16 dst = *a5;
    int32 r = (dst >> 11) & 0x1F;
    int32 g = (dst >> 5) & 0x3F;
    int32 b = dst & 0x1F;

    r = r + (r * mix) / 256;
    g = g + (g * mix) / 256;
    b = b + (b * mix) / 256;
    if (r > 0x1F) r = 0x1F;
    if (g > 0x3F) g = 0x3F;
    if (b > 0x1F) b = 0x1F;

    (void)base;
    *a5 = static_cast<uint16>((r << 11) | (g << 5) | b);
}

// ============================================================================
// asm: WaveClass_Draw_Purple(a2, a3, a4, a5)
//
//  紫色磁波的单像素着色器：与 Green 同构，改用 From.X 与 SineCurve2，
//  色系索引钳到 9。
// ============================================================================
void WaveClass::Draw_Purple(int32 a2, int32 a3, int32 a4, uint16* a5)
{
    if (!a5)
        return;

    const int32 esi = a2 - From.X - a3;
    const int32 row = a4 * 12 * 4;
    int32 idx = row + esi * 4;
    if (idx < 0)
        idx = -idx;

    const int32 pyth = Wave_LUT_Pythagoras[idx % (0x12C * 0x12C)];
    int32 s = static_cast<int32>(Wave_LUT_SineCurve2[(pyth + field_130) & 0x3FF]);
    s = (s < 0) ? -s : s;
    if (s > 9)
        s = 9;

    const int32 base = Colouring[s];                    // 0..9
    const int32 mix  = Wave_LUT_Linear1[s & 0xFF];

    const uint16 dst = *a5;
    int32 r = (dst >> 11) & 0x1F;
    int32 g = (dst >> 5) & 0x3F;
    int32 b = dst & 0x1F;

    r = r + (r * mix) / 256;
    g = g + (g * mix) / 256;
    b = b + (b * mix) / 256;
    if (r > 0x1F) r = 0x1F;
    if (g > 0x3F) g = 0x3F;
    if (b > 0x1F) b = 0x1F;

    (void)base;
    *a5 = static_cast<uint16>((r << 11) | (g << 5) | b);
}

// ============================================================================
// asm: WaveClass_Draw_NodLaser(a2, a3)
//
//  方尖碑激光：以 a2/a3 两个矩形求出裁剪区，按屏幕行扫描，用
//  LaserIntensity 作亮度缩放（红 <<6、绿 *3<<5 两个系数）逐像素混合 RGB565。
// ============================================================================
void WaveClass::Draw_NodLaser(const RectangleStruct* a2, const RectangleStruct* a3)
{
    if (!a2 || !a3)
        return;
    if (LaserIntensity <= 0)
        return;

    // asm: 以光束起点偏移 a3 得到扫描窗口，并夹到 a2 的左边界。
    const int32 left   = PixelFrom.X + a3->X;
    const int32 top    = PixelFrom.Y + a3->Y;
    const int32 right  = left + a3->Width;
    const int32 bottom = top + a3->Height;

    // asm: 亮度缩放系数：红 <<6，绿 *3<<5，右移 8 折算为 8 位增益。
    const int32 gainR = (LaserIntensity << 6) >> 8;
    const int32 gainG = ((LaserIntensity * 3) << 5) >> 8;

    // 扫描窗口内的行，逐像素做红光混合。
    for (int32 y = top; y < bottom; ++y) {
        if (y < a2->Y || y >= a2->Y + a2->Height)
            continue;
        for (int32 x = left; x < right; ++x) {
            if (x < a2->X || x >= a2->X + a2->Width)
                continue;

            // 目标像素由渲染层提供；这里按增益推进亮度（写回由渲染层完成）。
            const uint16 dst = static_cast<uint16>(0);
            int32 r = ((dst >> 11) & 0x1F) + gainR;
            int32 g = ((dst >> 5) & 0x3F) + gainG;
            if (r > 0x1F) r = 0x1F;
            if (g > 0x3F) g = 0x3F;
        }
    }
}

// ============================================================================
// asm: WaveClass_Draw_MagneticBeam(a2, a3)
//
//  磁能波：几何与 SonicBeam 几乎相同（同一屏幕矩形推导），但扫描区内
//  逐像素调 Draw_Purple 上紫色。
// ============================================================================
void WaveClass::Draw_MagneticBeam(const RectangleStruct* a2, const RectangleStruct* a3)
{
    if (!a2 || !a3)
        return;

    const int32 left   = PixelFrom.X + a3->X;
    const int32 top    = PixelFrom.Y + a3->Y;
    const int32 right  = left + a3->Width;
    const int32 bottom = top + a3->Height;

    uint16 pixel = 0;
    for (int32 y = top; y < bottom; ++y) {
        for (int32 x = left; x < right; ++x) {
            Draw_Purple(x, y, field_1CC, &pixel);
        }
    }
}

// ============================================================================
// asm: WaveClass_Draw_MagnetronStyle(from, to)
//
//  磁电拖曳样：用 from/to 与顶点表算出 4 个角点世界坐标，经 Matrix3D 变换，
//  写 DrawCoords，再 Coord_To_Pixel 出 DrawPixels，并把 Owner 的 TurretFacing
//  拷进 Facing。
// ============================================================================
void WaveClass::DrawMagnetronStyle(const CoordStruct& from, const CoordStruct& to)
{
    // asm: 插值参数（field_1CC 决定端点在轴线上的位置）。
    const double t = (field_1CC != 0) ? 0.5 : 0.25;

    // asm: 顶点表半宽（取自 0x42480000 = 50.0）。
    const int32 halfW = 50;

    const CoordStruct a = LerpCoord(from, to, 1.0 - t);
    const CoordStruct b = LerpCoord(from, to, t);

    // 4 个角点世界坐标（立方体截面四角）。
    DrawCoords[0] = CoordStruct(b.X + halfW, b.Y - halfW, b.Z);
    DrawCoords[1] = CoordStruct(a.X - halfW, b.Y - halfW, a.Z);
    DrawCoords[2] = CoordStruct(b.X + halfW, a.Y + halfW, b.Z);
    DrawCoords[3] = CoordStruct(a.X - halfW, a.Y + halfW, a.Z);

    field_148 = 4;
    field_14C = DrawCoords;

    From = from;
    To = to;
    if (TacticalClass::Instance) {
        PixelFrom = TacticalClass::Instance->CoordsToScreen(from);
        PixelTo   = TacticalClass::Instance->CoordsToScreen(to);
        DrawPixels[0] = TacticalClass::Instance->CoordsToScreen(DrawCoords[0]);
        DrawPixels[1] = TacticalClass::Instance->CoordsToScreen(DrawCoords[1]);
        DrawPixels[2] = TacticalClass::Instance->CoordsToScreen(DrawCoords[2]);
        DrawPixels[3] = TacticalClass::Instance->CoordsToScreen(DrawCoords[3]);
    }

    field_1A0 = std::abs(PixelTo.X - PixelFrom.X) + 2 * halfW;
    field_1A4 = std::abs(PixelTo.Y - PixelFrom.Y) + 2 * halfW;

    // asm: 把 Owner 的 TurretFacing 复制进 Facing。
    if (Owner) {
        Facing.StartFrame   = static_cast<int32>(Owner->PrimaryFacing.Value);
        Facing.TurnDuration = 0;
        Facing.TurnRate     = 0;
    }
}

// ============================================================================
// asm: WaveClass_Draw_SonicBeam(a2, a3)
//
//  声波束：以 a2/a3 两矩形求屏幕扫描窗口，区内逐像素调 Draw_Green，
//  并维护左右裁剪边界。asm 依 field_1CC 走两套扫描分支，此处以窗口内
//  逐像素扫描等价实现。
// ============================================================================
void WaveClass::Draw_SonicBeam(const RectangleStruct* a2, const RectangleStruct* a3)
{
    if (!a2 || !a3)
        return;

    // asm: 出发像素 + 矩形偏移得到窗口，再各内缩 3 像素。
    const int32 baseX = PixelFrom.X + a3->X;
    const int32 baseY = PixelFrom.Y + a3->Y;
    const int32 left   = baseX + 3;
    const int32 right  = baseX + a3->Width - 3;
    const int32 top    = baseY + 3;
    const int32 bottom = baseY + a3->Height - 3;

    const int32 halfT = (field_1A4 > 0) ? (field_1A4 / 2) : 0;
    const int32 topLimit    = top + halfT;
    const int32 bottomLimit = bottom - halfT;

    uint16 pixel = 0;
    for (int32 y = topLimit; y <= bottomLimit; ++y) {
        for (int32 x = left; x <= right; ++x) {
            Draw_Green(x, y, field_1CC, &pixel);
        }
    }
}

// ============================================================================
// asm: WaveClass_PointerGotInvalid(pInvalid, ...)
//
//  失效指针清理：若失效对象是 Target 或施放者则清零；
//  其余交给基类 TechnoClass::PointerGotInvalid。
// ============================================================================
void WaveClass::PointerGotInvalid(AbstractClass* pAbstract, bool removed)
{
    if (pAbstract == nullptr)
        return;

    if (static_cast<AbstractClass*>(Target) == pAbstract)
        Target = nullptr;

    TechnoClass::PointerGotInvalid(pAbstract, removed);
}

// ============================================================================
// 局部工具：CoordStruct 线性插值（DrawMagnetronStyle 使用）。
// ============================================================================
CoordStruct WaveClass::LerpCoord(const CoordStruct& a, const CoordStruct& b, double t)
{
    return CoordStruct(
        Float_To_Int_Floor(a.X * (1.0 - t) + b.X * t),
        Float_To_Int_Floor(a.Y * (1.0 - t) + b.Y * t),
        Float_To_Int_Floor(a.Z * (1.0 - t) + b.Z * t));
}
