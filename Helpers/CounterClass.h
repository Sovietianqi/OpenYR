#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

// ------------------------------------------------------------------------
// 根据游戏行为，可知计数器把条目数与累计量分开维护：命中计数
// 时同步累加数量，清零时两者一并复位。
// ------------------------------------------------------------------------
class CounterClass {
public:
    CounterClass() noexcept;

    void AddItem(int32 amount);
    int32 GetItemCount() const;
    int32 GetItemAmount() const;
    void ResetToZero();

    int32 Count;
    int32 Amount;
};
