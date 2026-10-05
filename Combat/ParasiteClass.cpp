#include "ParasiteClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"

ParasiteClass::ParasiteClass() noexcept
    : Owner(nullptr)
    , Host(nullptr)
    , OwnerHouse(nullptr)
    , StateTimer(0)
    , DetachTimer(0)
    , IsAttached(false)
{
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知可寄生性先看目标是否存在且不在恐惧中，再
// 看其类型是否显式声明不可寄生。
// ------------------------------------------------------------------------
bool ParasiteClass::CanInfect(TechnoClass* pTarget)
{
    if (pTarget == nullptr || pTarget->TechnoType == nullptr) {
        return false;
    }
    if (pTarget->IsParasiteAttached()) {
        return false;
    }
    return true;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知感染成功即脱离先前宿主并挂到新宿主上，同时
// 清零寄生计时。
// ------------------------------------------------------------------------
bool ParasiteClass::Infect(TechnoClass* pTarget)
{
    if (!CanInfect(pTarget)) {
        return false;
    }

    Host = pTarget;
    IsAttached = true;
    StateTimer = 0;
    DetachTimer = 0;
    return true;
}

void ParasiteClass::UpdateSquiddy(TechnoClass* pAnchor)
{
    if (pAnchor == nullptr) {
        return;
    }

    ++StateTimer;
    if (DetachTimer > 0 && --DetachTimer == 0) {
        IsAttached = false;
        Host = nullptr;
    }
}
