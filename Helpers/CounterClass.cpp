#include "CounterClass.h"

CounterClass::CounterClass() noexcept
    : Count(0)
    , Amount(0)
{
}

void CounterClass::AddItem(int32 amount)
{
    ++Count;
    Amount += amount;
}

int32 CounterClass::GetItemCount() const
{
    return Count;
}

int32 CounterClass::GetItemAmount() const
{
    return Amount;
}

void CounterClass::ResetToZero()
{
    Count = 0;
    Amount = 0;
}
