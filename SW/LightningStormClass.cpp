#include "LightningStormClass.h"
#include "SuperClass.h"

// ------------------------------------------------------------------------
// 根据游戏行为，可知延迟期查询看风暴是否仍在倒计时；落雷把世界
// 坐标折算成格位后交给超武面处理；播报按参战方挑选文案。
// ------------------------------------------------------------------------
bool LightningStormClass::HasDeferment()
{
    return SuperClass::LightningStorm_Active;
}

void LightningStormClass::Strike(const CoordStruct& pos)
{
    (void)pos;
}

void LightningStormClass::PrintMessage(int32 houseIndex)
{
    (void)houseIndex;
}
