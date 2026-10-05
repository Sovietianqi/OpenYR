#include "TimerClass.h"

TimerClass::TimerClass() noexcept
    : Started(0)
    , Accumulated(0)
    , Duration(0)
{
}

void TimerClass::Start()
{
    Started = 1;
    Accumulated = 0;
}

void TimerClass::StartWithDuration(int32 duration)
{
    Started = 1;
    Duration = duration;
    Accumulated = 0;
}

void TimerClass::StartIfEmpty(int32 duration)
{
    if (Started == 0 || Duration == 0) {
        StartWithDuration(duration);
    }
}

int32 TimerClass::GetTimeLeft() const
{
    if (Started == 0) {
        return 0;
    }
    return Duration - Accumulated;
}

int32 TimerClass::GetTimeLeft2() const
{
    const int32 left = GetTimeLeft();
    return (left > 0) ? left : 0;
}

void TimerClass::Stop()
{
    Started = 0;
}
