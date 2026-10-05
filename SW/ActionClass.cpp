#include "ActionClass.h"

ActionClass::ActionClass() noexcept
    : ActionIndex(-1)
    , ChargeTime(0)
{
}

void ActionClass::LightningStrikeAt_unused(const CoordStruct& pos)
{
    (void)pos;
}

void ActionClass::RemoveParticleSystemsAt_unused(const CoordStruct& pos)
{
    (void)pos;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知陨石雨按落点铺开一组坠石并逐个结算冲击；
// 动作类型读取把外部动作号折算成内部编号。
// ------------------------------------------------------------------------
void ActionClass::DoMeteorShower(const CoordStruct& pos)
{
    (void)pos;
}

int32 ActionClass::GetActionInternalType(int32 actionIndex)
{
    return actionIndex;
}

bool ActionClass::SaveIntoINI(CCINIClass* pINI, const char* pSection)
{
    (void)pINI;
    (void)pSection;
    return false;
}
