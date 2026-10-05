#include "SlaveManagerClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Houses/HouseClass.h"

SlaveManagerClass::SlaveManagerClass() noexcept
    : Owner(nullptr)
    , OwnerHouse(nullptr)
    , SlaveCount(0)
    , StateTimer(0)
    , HarvestTick(0)
{
    for (int32 i = 0; i < MAX_SLAVE_SLOTS; ++i) {
        Slaves[i] = nullptr;
    }
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知矿场被移交或出售时，在役奴隶被统一转交给
// 平民阵营名下的接收方；找不到接收方则直接散伙。
// ------------------------------------------------------------------------
void SlaveManagerClass::ReleaseSlaves(HouseClass* pNewOwnerHouse)
{
    for (int32 i = 0; i < MAX_SLAVE_SLOTS; ++i) {
        TechnoClass* pSlave = Slaves[i];
        if (pSlave == nullptr) {
            continue;
        }

        if (pNewOwnerHouse != nullptr && pNewOwnerHouse != OwnerHouse) {
            pSlave->Set_Owner(pNewOwnerHouse);
        }
        Slaves[i] = nullptr;
    }
    SlaveCount = 0;
}

void SlaveManagerClass::ReplaceWhichBelongsToUnit(TechnoClass* pUnit)
{
    if (pUnit == nullptr) {
        return;
    }

    for (int32 i = 0; i < MAX_SLAVE_SLOTS; ++i) {
        if (Slaves[i] == pUnit) {
            Slaves[i] = nullptr;
            --SlaveCount;
        }
    }
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知唤醒判定先看矿场自身是否仍在采矿状态，再
// 看脚下是否还压着矿堆；两项都不成立时奴隶应当被唤醒回岗。
// ------------------------------------------------------------------------
bool SlaveManagerClass::ShouldWakeUp() const
{
    if (Owner == nullptr || OwnerHouse == nullptr) {
        return false;
    }
    return StateTimer == 0;
}

void SlaveManagerClass::LoseSlave(TechnoClass* pSlave)
{
    if (pSlave == nullptr) {
        return;
    }

    for (int32 i = SlaveCount - 1; i >= 0; --i) {
        if (Slaves[i] == pSlave) {
            for (int32 j = i; j < MAX_SLAVE_SLOTS - 1; ++j) {
                Slaves[j] = Slaves[j + 1];
            }
            Slaves[MAX_SLAVE_SLOTS - 1] = nullptr;
            --SlaveCount;
            return;
        }
    }
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知显式采收指令会把当前劳作计数清零，让在岗
// 奴隶立刻转入下一轮开采节奏。
// ------------------------------------------------------------------------
void SlaveManagerClass::ExplicitHarvest()
{
    HarvestTick = 0;
    ++StateTimer;
}
