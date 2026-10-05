#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class TechnoClass;
class HouseClass;

static constexpr int32 MAX_SLAVE_SLOTS = 5;

class SlaveManagerClass {
public:
    SlaveManagerClass() noexcept;

    // 根据游戏行为，可知奴隶矿工的劳作面围绕五个槽位展开：移交、
    // 补员、唤醒判定与损失回收都按槽位逐一处理。
    void ReleaseSlaves(HouseClass* pNewOwnerHouse);
    void ReplaceWhichBelongsToUnit(TechnoClass* pUnit);
    bool ShouldWakeUp() const;
    void LoseSlave(TechnoClass* pSlave);
    void ExplicitHarvest();

    TechnoClass* Owner;
    HouseClass* OwnerHouse;
    TechnoClass* Slaves[MAX_SLAVE_SLOTS];
    int32 SlaveCount;
    int32 StateTimer;
    int32 HarvestTick;
};
