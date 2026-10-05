// =============================================================================
// WeaponWaveClass.cpp - 波束武器特效（激光/磁波/声波族）
// =============================================================================

#include "WeaponWaveClass.h"
#include "WeaponTypeClass.h"
#include "DamageArea.h"
#include "../Abstract/TechnoClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"
#include "../Rendering/TacticalClass.h"
#include "../Math/Rectangle.h"

// ============================================================================
// 静态成员定义
// ============================================================================
DynamicVectorClass<WeaponWaveClass*>* WeaponWaveClass::Array = nullptr;

static DynamicVectorClass<BeamSegment> g_PendingBeamSegments;
static int32 g_BeamLUTs[32];
static bool  g_BeamLUTsReady = false;

DynamicVectorClass<BeamSegment>* WeaponWaveClass::GetSegmentQueue()
{
    return &g_PendingBeamSegments;
}

// ============================================================================
// 亮度查找表：32 档从 256 线性衰减到 0，绘制层以相位取值调制束体亮度。
// ============================================================================
void WeaponWaveClass::CalculateLUTs()
{
    if (g_BeamLUTsReady)
        return;
    for (int32 i = 0; i < 32; ++i)
        g_BeamLUTs[i] = 256 - (i * 256) / 32;
    g_BeamLUTsReady = true;
}

const int32* WeaponWaveClass::GetLUTs()
{
    if (!g_BeamLUTsReady)
        CalculateLUTs();
    return g_BeamLUTs;
}

// ============================================================================
// 构造 / 析构
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
{
    if (!Array)
        Array = new DynamicVectorClass<WeaponWaveClass*>();
    Array->Add(this);
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
}

// ============================================================================
// SubmitSegment - 折算风格参数后提交一条待渲染段
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
// Update_Beam - 年龄推进与到期回收
// ============================================================================
void WeaponWaveClass::Update_Beam()
{
    if (IsDead)
        return;
    ++Age;
    // 根据游戏行为，可知声波束在存续期按固定节拍对目标格反复结算伤害，
    // 其余束体只在生成时结算一次。
    if (Weapon && Weapon->Warhead) {
        // 声波束以束腰加宽作为持续伤害的外观特征；这里以宽度随年龄增长
        // 识别声波风格并维持节拍伤害。
        if (Width > 6 && (Age % 8) == 0)
            DamageCell();
    }
    if (Age >= Duration) {
        IsDead = true;
    } else {
        // 根据游戏行为，可知声波束随年龄向外扩散变宽，其余束体随年龄
        // 收细：束腰按存续比演化。
        const int32 denom = Duration > 0 ? Duration : 1;
        if (Width >= 10)
            Width = Width + (Age * 8) / denom;
        else
            Width = 1 + ((denom - Age) * 4) / denom;
    }
}

// ============================================================================
// DamageCell - 对目标坐标所在区域按武器伤害结算
// ============================================================================
void WeaponWaveClass::DamageCell()
{
    if (!Weapon || IsDead)
        return;
    TechnoClass* pSource = Owner;
    HouseClass* pHouse = pSource ? pSource->Owner : nullptr;
    DamageArea::ApplyAtLocation(To, Weapon->Damage, pSource,
                                Weapon->Warhead, false, pHouse);
    ++DamageDealt;
}

// ============================================================================
// 各风格绘制入口
// ============================================================================

void WeaponWaveClass::Draw_Green()
{
    // 根据游戏行为，可知绿色激光束腰 4 像素，亮度相位随年龄推进。
    SubmitSegment(0, 4, Age % 32);
}

void WeaponWaveClass::Draw_Purple()
{
    // 根据游戏行为，可知紫色束腰 6 像素，亮度相位以 2 为步进推进。
    SubmitSegment(1, 6, (Age * 2) % 32);
}

void WeaponWaveClass::Draw_NodLaser()
{
    // 根据游戏行为，可知方尖碑激光为红色细束，束腰 3 像素，相位随年龄
    // 快速闪动。
    SubmitSegment(2, 3, (Age * 3) % 32);
}

void WeaponWaveClass::Draw_MagneticBeam()
{
    // 根据游戏行为，可知磁能波为宽束，束腰 8 像素，相位缓慢推进。
    SubmitSegment(3, 8, (Age / 2) % 32);
}

void WeaponWaveClass::Draw_SonicBeam()
{
    // 根据游戏行为，可知声波束最宽且随年龄扩散，相位按帧推进。
    SubmitSegment(4, 10, Age % 32);
}

void WeaponWaveClass::DrawMagnetronStyle()
{
    // 根据游戏行为，可知磁电拖曳样式与磁能波同色但束腰居中，相位以
    // 3 为步进抖动。
    SubmitSegment(3, 5, (Age * 3) % 32);
}

void WeaponWaveClass::DrawOldSchool()
{
    // 根据游戏行为，可知经典激光为 2 像素细束，亮度相位逐帧推进。
    SubmitSegment(5, 2, Age % 32);
}
