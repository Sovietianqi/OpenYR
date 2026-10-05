#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class TechnoClass;
class HouseClass;

class BombClass {
public:
    BombClass() noexcept;

    // 根据游戏行为，可知附着炸弹的引爆面围绕三件事：是否已被
    // 布雷、倒计时是否走完、以及被拆除时的现场清理。
    bool SagedMyself() const;
    bool TimeToBlow() const;
    void Disarm();

    TechnoClass* Target;
    TechnoClass* Owner;
    HouseClass* OwnerHouse;
    int32 State;
    int32 ExplodeFrame;
    int32 Damage;
    char AttachedName[16];
    bool IsArmed;
    bool IsDeathBomb;
    bool HasExploded;
};
