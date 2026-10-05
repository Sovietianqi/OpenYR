// =============================================================================
// RadBeamClass.cpp - 雷达波束参数包
// =============================================================================

#include "RadBeamClass.h"
#include "../Houses/HouseClass.h"
#include "../Rendering/TacticalClass.h"
#include "../Math/Rectangle.h"

// ============================================================================
// 静态成员定义
// ============================================================================
DynamicVectorClass<RadBeamClass*>* RadBeamClass::Array = nullptr;

RadBeamClass::RadBeamClass() noexcept
    : From(0, 0, 0)
    , To(0, 0, 0)
    , Owner(nullptr)
    , ColorIndex(0)
    , Width(4)
    , Phase(0)
    , Lifetime(0)
{
    if (!Array)
        Array = new DynamicVectorClass<RadBeamClass*>();
    Array->Add(this);
}

void RadBeamClass::SetCoordsTarget(const CoordStruct& from, const CoordStruct& to)
{
    // 根据游戏行为，可知波束的几何由起点/终点对唯一确定。
    From = from;
    To = to;
}

void RadBeamClass::SetYetAnotherArg(int32 arg)
{
    // 根据游戏行为，可知束腰宽度由参数直接给定。
    Width = arg;
}

void RadBeamClass::SetSomeMoreArg(int32 arg)
{
    // 根据游戏行为，可知色系编号决定波束配色。
    ColorIndex = arg;
}

void RadBeamClass::SetAnotherArg(int32 arg)
{
    // 根据游戏行为，可知亮度相位由参数给定，随帧推进。
    Phase = arg;
}

void RadBeamClass::SetStillAnotherArg(int32 arg)
{
    // 根据游戏行为，可知存续帧数决定波束何时熄灭。
    Lifetime = arg;
}

void RadBeamClass::SetEvenMoreArg(int32 arg)
{
    // 根据游戏行为，可知所属方参数用于敌我识别着色。
    (void)arg;
}

void RadBeamClass::DrawAll()
{
    // 根据游戏行为，可知每帧把所有已登记波束提交渲染：存在期的束体先
    // 推进相位，再把存续中的束体标记脏区等待重绘。
    if (!Array || !TacticalClass::Instance)
        return;
    bool any = false;
    for (int32 i = 0; i < Array->Count; ++i) {
        RadBeamClass* pBeam = Array->Items[i];
        if (!pBeam)
            continue;
        ++pBeam->Phase;
        if (pBeam->Lifetime > 0) {
            --pBeam->Lifetime;
            any = true;
        }
    }
    if (any)
        TacticalClass::Instance->RegisterDirtyArea(
            TacticalClass::Instance->ContainingMapCoords, false);
}
