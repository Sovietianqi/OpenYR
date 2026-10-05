#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"

class HouseClass;

// ------------------------------------------------------------------------
// 根据游戏行为，可知超武动作面按动作号分派：闪电打击与陨石雨
// 两条毁伤线各自落地，粒子系统随动作收尾清理。
// ------------------------------------------------------------------------
class ActionClass {
public:
    ActionClass() noexcept;

    static void LightningStrikeAt_unused(const CoordStruct& pos);
    static void RemoveParticleSystemsAt_unused(const CoordStruct& pos);
    static void DoMeteorShower(const CoordStruct& pos);
    static int32 GetActionInternalType(int32 actionIndex);
    bool SaveIntoINI(class CCINIClass* pINI, const char* pSection);

    int32 ActionIndex;
    int32 ChargeTime;
};
