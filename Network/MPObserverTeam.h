#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class HouseClass;

// ------------------------------------------------------------------------
// 根据游戏行为，可知观察者队伍把观战参战方收拢成组：指派入队
// 与归属查询都沿队伍清单进行。
// ------------------------------------------------------------------------
class MPObserverTeam {
public:
    MPObserverTeam() noexcept;

    void Assign(HouseClass* pHouse);
    bool IsTeamIncluded(const HouseClass* pHouse) const;

    DynamicVectorClass<HouseClass*> Members;
};
