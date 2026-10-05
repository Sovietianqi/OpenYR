#include "TemporalClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"

TemporalClass::TemporalClass() noexcept
    : LinkedTechno(nullptr)
    , Owner(nullptr)
    , WarpDistance(0)
    , ChargeCount(0)
    , Next(nullptr)
    , Prev(nullptr)
{
}

TemporalClass::~TemporalClass()
{
    LinkedTechno = nullptr;
    Owner = nullptr;
    Next = nullptr;
    Prev = nullptr;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知时间固定链松脱时，先把自身在链上摘除并归还
// 后继节点的外部链接，再清空全部反向链接与持有状态。
// ------------------------------------------------------------------------
void TemporalClass::LetGo()
{
    if (Next != nullptr && Prev != nullptr) {
        Next->Prev = Prev;
    }

    if (Prev != nullptr) {
        Prev->Next = Next;
    }

    if (LinkedTechno != nullptr) {
        LinkedTechno->TemporalImUsing = nullptr;
    }

    LinkedTechno = nullptr;
    Owner = nullptr;
    Next = nullptr;
    Prev = nullptr;
    ChargeCount = 0;
    WarpDistance = 0;
}

void TemporalClass::JustLetGo()
{
    if (LinkedTechno != nullptr) {
        LinkedTechno->TemporalImUsing = nullptr;
    }
    LinkedTechno = nullptr;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知传送可行性只看目标是否仍存在及其类型是否
// 声明为可传送；已湮灭或类型禁传送一律拒绝。
// ------------------------------------------------------------------------
bool TemporalClass::CanWarpTarget(TechnoClass* pTarget)
{
    if (pTarget == nullptr || pTarget->TechnoType == nullptr) {
        return false;
    }
    return pTarget->TechnoType->Warpable;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知时间场对穿越者的助伤按五十格标度折算，剩余
// 生命越少每步收取的份额越接近上限。
// ------------------------------------------------------------------------
int32 TemporalClass::GetHelperDamagePerStep(int32 percent) const
{
    if (percent > 50) {
        percent = 50;
    }
    return percent;
}
