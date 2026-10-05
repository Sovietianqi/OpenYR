#include "Veterancy.h"

static constexpr float VETERAN_THRESHOLD = 1.0f;
static constexpr float ELITE_THRESHOLD = 2.0f;

Veterancy::Veterancy() noexcept
    : Value(0.0f)
{
}

bool Veterancy::IsElite() const
{
    return Value >= ELITE_THRESHOLD;
}

bool Veterancy::IsVeteran() const
{
    return Value >= VETERAN_THRESHOLD && Value < ELITE_THRESHOLD;
}

bool Veterancy::IsPreRookie() const
{
    return Value < VETERAN_THRESHOLD;
}

void Veterancy::SetRookie()
{
    Value = 0.0f;
}

void Veterancy::SetVeteran()
{
    Value = VETERAN_THRESHOLD;
}

void Veterancy::SetElite()
{
    Value = ELITE_THRESHOLD;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知经验增量是"受害者成本除以自身成本"的商，
// 经规则系数放大后累加并钳到精英上限。
// ------------------------------------------------------------------------
void Veterancy::AddValue(float victimCost, float ownerCost)
{
    if (ownerCost <= 0.0f) {
        return;
    }

    Value += victimCost / ownerCost * 0.5f;
    if (Value > ELITE_THRESHOLD) {
        Value = ELITE_THRESHOLD;
    }
}
