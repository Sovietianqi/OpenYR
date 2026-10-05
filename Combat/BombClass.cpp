#include "BombClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Game/Game.h"

BombClass::BombClass() noexcept
    : Target(nullptr)
    , Owner(nullptr)
    , OwnerHouse(nullptr)
    , State(0)
    , ExplodeFrame(0)
    , Damage(0)
    , IsArmed(false)
    , IsDeathBomb(false)
    , HasExploded(false)
{
    AttachedName[0] = '\0';
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知炸弹是否已经"被上雷"由死亡炸弹旗标直接回答。
// ------------------------------------------------------------------------
bool BombClass::SagedMyself() const
{
    return IsDeathBomb;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知倒计时只在处于待命状态、当前帧越过起爆帧且
// 尚未拉响自毁时才算到期。
// ------------------------------------------------------------------------
bool BombClass::TimeToBlow() const
{
    if (State != 0) {
        return false;
    }
    if (HasExploded) {
        return false;
    }
    return Game::CurrentFrame > ExplodeFrame;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知拆除动作把持有者的关联炸弹一并解除，清空
// 目标与计时状态后落定拆除旗标。
// ------------------------------------------------------------------------
void BombClass::Disarm()
{
    IsDeathBomb = true;
    State = 0;
    ExplodeFrame = 0;
    Target = nullptr;
    Owner = nullptr;
    AttachedName[0] = '\0';
    HasExploded = true;
}
