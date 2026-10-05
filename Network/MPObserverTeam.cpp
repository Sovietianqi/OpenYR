#include "MPObserverTeam.h"

MPObserverTeam::MPObserverTeam() noexcept
{
}

void MPObserverTeam::Assign(HouseClass* pHouse)
{
    if (pHouse != nullptr) {
        Members.AddItem(pHouse);
    }
}

bool MPObserverTeam::IsTeamIncluded(const HouseClass* pHouse) const
{
    const int32 count = Members.GetCount();
    for (int32 i = 0; i < count; ++i) {
        if (Members[i] == pHouse) {
            return true;
        }
    }
    return false;
}
