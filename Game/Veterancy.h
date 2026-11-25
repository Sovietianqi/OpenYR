#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class HouseClass;

// ------------------------------------------------------------------------
// 经验值容器（asm: Veterancy_* 系列，0x74FFxx 段）。
//
// 原版用一个 float 存经验：Rookie 0 / Veteran 1.0 / Elite 2.0，
// SetRookie(true) 写 -0.25（把"该升老兵"的判断推迟到下一次结算），
// 三个 Set* 都带一个 bool：true 写对应值，false 清 0。
// ------------------------------------------------------------------------
class Veterancy {
public:
    Veterancy() noexcept;

    bool IsElite() const;
    bool IsVeteran() const;
    bool IsPreRookie() const;
    void SetRookie(bool value);
    void SetVeteran(bool value);
    void SetElite(bool value);

    // asm: Veterancy_AddValue(this, int ownerCost, int victimCost)
    //   Value += victimCost / (ownerCost * Rules->VeteranRatio)，再钳到
    //   Rules->VeteranCap。
    void AddValue(int32 ownerCost, int32 victimCost);

    float Value;
};
