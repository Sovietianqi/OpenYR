#include "Veterancy.h"

#include "../Rules/RulesClass.h"

// asm 常量：Veterancy_SetVeteran 写 1.0、Veterancy_SetElite 写 2.0、
// Veterancy_SetRookie(true) 写 -0.25。
static constexpr float ROOKIE_SENTINEL = -0.25f;
static constexpr float VETERAN_THRESHOLD = 1.0f;
static constexpr float ELITE_THRESHOLD = 2.0f;

Veterancy::Veterancy() noexcept
    : Value(0.0f)
{
}

// asm: byte __fastcall Veterancy_IsElite(float *this)
//   fld [ecx] / fcomp 2.0 / Value >= 2.0 → 1
bool Veterancy::IsElite() const
{
    return Value >= ELITE_THRESHOLD;
}

// asm: byte __fastcall Veterancy_IsVeteran(float *this)
//   !(Value < 1.0) && (Value < 2.0)
bool Veterancy::IsVeteran() const
{
    return Value >= VETERAN_THRESHOLD && Value < ELITE_THRESHOLD;
}

// asm: byte __thiscall Veterancy_IsPreRookie(float *this)
//   fcomp 0.0 → Value < 0.0 才算"低于新兵"（-0.25 哨兵即此态）。
//   早先按 Value < 1.0 实现是错的：那会把所有新兵都算成 PreRookie。
bool Veterancy::IsPreRookie() const
{
    return Value < 0.0f;
}

// asm: char __thiscall Veterancy_SetRookie(float *this, char a2)
//   a2 ? [this] = -0.25 : [this] = 0
void Veterancy::SetRookie(bool value)
{
    Value = value ? ROOKIE_SENTINEL : 0.0f;
}

// asm: char __thiscall Veterancy_SetVeteran(float *this, char a2)
//   a2 ? [this] = 1.0 : [this] = 0
void Veterancy::SetVeteran(bool value)
{
    Value = value ? VETERAN_THRESHOLD : 0.0f;
}

// asm: char __thiscall Veterancy_SetElite(float *this, char a2)
//   a2 ? [this] = 2.0 : [this] = 0
void Veterancy::SetElite(bool value)
{
    Value = value ? ELITE_THRESHOLD : 0.0f;
}

// ------------------------------------------------------------------------
// asm: char __thiscall Veterancy_AddValue(float *this, int ownerCost, int victimCost)
//      (asm:1725745)
//
//   fild victimCost
//   fild ownerCost
//   fmul Rules->VeteranRatio        ; ownerCost * ratio
//   fdivp st(1), st                 ; victimCost / (ownerCost * ratio)
//   fadd [this] / fst [this]
//   if (Value > Rules->VeteranCap) Value = VeteranCap
//
//  注意原版把两个参数当整数读（fild），且除数是"自身成本 × 系数"，
//  不是早先实现的"先乘系数再除自身成本"。
// ------------------------------------------------------------------------
void Veterancy::AddValue(int32 ownerCost, int32 victimCost)
{
    const double ratio = (RulesClass::Instance != nullptr)
                       ? RulesClass::Instance->VeteranRatio : 0.0;
    const double cap   = (RulesClass::Instance != nullptr)
                       ? RulesClass::Instance->VeteranCap : 0.0;

    // 原版没有除零分支；这里只在"除数为 0"这一无法表示的输入上保持原值，
    // 避免 NaN/Inf 污染经验池。
    const double denom = static_cast<double>(ownerCost) * ratio;
    if (denom == 0.0) {
        return;
    }

    double total = static_cast<double>(Value)
                 + static_cast<double>(victimCost) / denom;

    if (total > cap) {
        total = cap;
    }

    Value = static_cast<float>(total);
}
