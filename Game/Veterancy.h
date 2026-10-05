#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class HouseClass;

// ------------------------------------------------------------------------
// 根据游戏行为，可知经验等级以浮点累计值维护：阈值之上越级，
// 击杀经验按成本比折算后累加。
// ------------------------------------------------------------------------
class Veterancy {
public:
    Veterancy() noexcept;

    bool IsElite() const;
    bool IsVeteran() const;
    bool IsPreRookie() const;
    void SetRookie();
    void SetVeteran();
    void SetElite();
    void AddValue(float victimCost, float ownerCost);

    float Value;
};
