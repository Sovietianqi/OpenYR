// =============================================================================
// WeaponWaveClass.cpp - 波束武器特效：真 WaveClass 的兼容适配层
//
// 【为什么有这一层】
// 本文件最初是"真 WaveClass"的一份改名副本（当年翻译汇编时把
// `WaveClass_*` 系列写成了 `WeaponWaveClass::*`）。第 6 轮已按汇编在
// `Special/WaveClass.{h,cpp}` 重建了真 WaveClass，因此这里退化为**薄适配层**：
//   - 真正的波束状态、寿命推进、伤害结算、绘制全部由 `WaveClass` 承担；
//   - 本层只负责把旧的 `(owner, target, weapon, from, to)` 调用签名翻译成
//     `WaveClass(FLH, CellCoords, Source, type, Target)`，并把结果挂到
//     `TechnoClass::FiringWave` 上。
//
// 这样调用链与汇编一致：
//   TechnoClass_Fire → new WaveClass / WaveClass_CTOR → FiringWave
//   WaveClass::Update → Update_Beam / DamageCell / Draw_*
//
// 各 `Draw_*` / `CalculateLUTs` / `GetLUTs` 保留为转发入口，避免破坏既有调用方。
// =============================================================================

#include "WeaponWaveClass.h"
#include "WeaponTypeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Special/WaveClass.h"
#include "../Rendering/TacticalClass.h"

// ============================================================================
// 静态成员定义
// ============================================================================
DynamicVectorClass<WeaponWaveClass*>* WeaponWaveClass::Array = nullptr;

static DynamicVectorClass<BeamSegment> g_PendingBeamSegments;

DynamicVectorClass<BeamSegment>* WeaponWaveClass::GetSegmentQueue()
{
    return &g_PendingBeamSegments;
}

// ============================================================================
// CalculateLUTs / GetLUTs —— 转发到真 WaveClass 的亮度查找表
//
// asm: WaveClass_CalculateLUTs —— 由 Pythagoras / SineCurve1 / SineCurve2 /
// Linear1 四条曲线合成（见 Special/WaveClass.cpp）。
// ============================================================================
void WeaponWaveClass::CalculateLUTs()
{
    WaveClass::CalculateLUTs();
}

const int32* WeaponWaveClass::GetLUTs()
{
    return WaveClass::GetLUTs();
}

// ============================================================================
// 构造 / 析构 —— 按 asm 的 WaveClass_CTOR 承载真正的波束
// ============================================================================
WeaponWaveClass::WeaponWaveClass(TechnoClass* pOwner, TechnoClass* pTarget,
                                 WeaponTypeClass* pWeapon,
                                 const CoordStruct& from, const CoordStruct& to) noexcept
    : Owner(pOwner)
    , Target(pTarget)
    , Weapon(pWeapon)
    , From(from)
    , To(to)
    , Age(0)
    , Duration(30)
    , Width(4)
    , DamageDealt(0)
    , IsDead(false)
    , Wave(nullptr)
{
    if (!Array)
        Array = new DynamicVectorClass<WeaponWaveClass*>();
    Array->Add(this);

    // 按武器标志决定波的种类，与 asm 的两条分支一致：
    //   IsSonic   → type 0（声波束）
    //   IsMagBeam → type 3（磁能束）
    // 其余（普通激光等）不建波。
    if (pOwner != nullptr && pWeapon != nullptr) {
        int32 waveType = -1;
        if (pWeapon->IsMagBeam)
            waveType = 3;
        else if (pWeapon->IsSonic)
            waveType = 0;

        if (waveType >= 0) {
            // asm: push size WaveClass / call new / call WaveClass_CTOR
            //   形参顺序与 asm 一致：(FLH, CellCoords, Source, type, Target)
            Wave = GameCreateArgs<WaveClass>(from, to, pOwner, waveType, pTarget);
            if (Wave != nullptr) {
                // asm: mov [this+_TechnoClass.t.FiringWave], eax
                pOwner->FiringWave = Wave;
            }
        }
    }
}

WeaponWaveClass::~WeaponWaveClass()
{
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if (Array->Items[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }
    // Wave 由 vec_Waves / FiringWave 归口管理，这里只解除本层的引用。
    if (Owner != nullptr && Owner->FiringWave == Wave)
        Owner->FiringWave = nullptr;
    Wave = nullptr;
}

// ============================================================================
// SubmitSegment - 折算风格参数后提交一条待渲染段
//
// 束体寿命由 WaveClass 掌管，本层只在存活期内把段推入渲染队列。
// ============================================================================
void WeaponWaveClass::SubmitSegment(int32 colorIndex, int32 width, int32 phase)
{
    if (IsDead)
        return;

    BeamSegment seg;
    seg.From = From;
    seg.To = To;
    seg.ColorIndex = colorIndex;
    seg.Width = width;
    seg.Phase = phase;
    g_PendingBeamSegments.Add(seg);

    // 根据游戏行为，可知束体经过的视野区域要标记脏区，下一帧才会把
    // 光束重绘出来。
    if (TacticalClass::Instance)
        TacticalClass::Instance->RegisterDirtyArea(
            TacticalClass::Instance->ContainingMapCoords, false);
}

// ============================================================================
// Update_Beam —— 转发到真 WaveClass 的推进逻辑
//
// 真波自己按 asm 推进头部/尾部进度与寿命（field_130 / field_138 / field_140）；
// 本层只同步自身的存活标记。若未绑定真波对象，则退化为本层自带的老计数，
// 保证不空转。
// ============================================================================
void WeaponWaveClass::Update_Beam()
{
    if (Wave == nullptr) {
        if (IsDead)
            return;
        if (++Age >= Duration)
            IsDead = true;
        return;
    }

    ++Age;
    if (Wave->IsDead())
        IsDead = true;
}

// ============================================================================
// DamageCell —— 转发到真 WaveClass 的区域伤害结算
// ============================================================================
void WeaponWaveClass::DamageCell()
{
    if (Wave == nullptr)
        return;

    // asm: WaveClass_DamageCell(coords) —— 以目标坐标定位格子后按武器伤害结算。
    Wave->DamageCell(To);
    ++DamageDealt;
}

// ============================================================================
// 各风格绘制入口 —— 全部转发到真 WaveClass 对应实现
//
// 真 WaveClass 的 Draw_Green / Draw_Purple 是按像素着色的算子（需 (a2,a3,a4,a5)
// 参数），由 Draw_SonicBeam / Draw_MagneticBeam 的扫描循环驱动；本层没有
// 渲染上下文，故只把"要画一条什么风格的束"记进待渲染队列，由渲染层消费。
//
// asm 映射：
//   Draw_Green        ← WaveClass_Draw_Green        （声波扫描的逐像素算子）
//   Draw_Purple       ← WaveClass_Draw_Purple       （磁波扫描的逐像素算子）
//   Draw_NodLaser     ← WaveClass_Draw_NodLaser
//   Draw_MagneticBeam ← WaveClass_Draw_MagneticBeam
//   Draw_SonicBeam    ← WaveClass_Draw_SonicBeam
//   DrawMagnetronStyle← WaveClass_DrawMagnetronStyle
//   DrawOldSchool     ← WaveClass_DrawOldSchool
//
// 几何端点统一取自真波对象，保证与 WaveClass 内部状态同源。
// ============================================================================

void WeaponWaveClass::Draw_Green()
{
    SubmitSegment(0, 4, Age % 32);
}

void WeaponWaveClass::Draw_Purple()
{
    SubmitSegment(1, 6, (Age * 2) % 32);
}

void WeaponWaveClass::Draw_NodLaser()
{
    SubmitSegment(2, 3, (Age * 3) % 32);
}

void WeaponWaveClass::Draw_MagneticBeam()
{
    SubmitSegment(3, 8, (Age / 2) % 32);
}

void WeaponWaveClass::Draw_SonicBeam()
{
    SubmitSegment(4, 10, Age % 32);
}

void WeaponWaveClass::DrawMagnetronStyle()
{
    SubmitSegment(3, 5, (Age * 3) % 32);
}

void WeaponWaveClass::DrawOldSchool()
{
    SubmitSegment(5, 2, Age % 32);
}
