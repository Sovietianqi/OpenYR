#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class TechnoClass;
class HouseClass;
class BulletTypeClass;

class ParasiteClass {
public:
    ParasiteClass() noexcept;

    // 根据游戏行为，可知寄生单元的运作面分三步：感染前先验证
    // 目标可被寄生，感染成功后脱离旧宿主，最后按帧推进寄生状态。
    bool CanInfect(TechnoClass* pTarget);
    bool Infect(TechnoClass* pTarget);
    void UpdateSquiddy(TechnoClass* pAnchor);

    TechnoClass* Owner;
    TechnoClass* Host;
    HouseClass* OwnerHouse;
    int32 StateTimer;
    int32 DetachTimer;
    bool IsAttached;
};
