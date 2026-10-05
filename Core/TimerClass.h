#pragma once

#include "Definitions.h"
#include "Macros.h"
#include "Memory.h"

// ------------------------------------------------------------------------
// 根据游戏行为，可知计时器有三种启动姿势：立即起跑、沿用既有
// 剩余、以及带时长起跑；读取返回剩余步数。
// ------------------------------------------------------------------------
class TimerClass {
public:
    TimerClass() noexcept;

    void Start();
    void StartWithDuration(int32 duration);
    void StartIfEmpty(int32 duration);
    int32 GetTimeLeft() const;
    int32 GetTimeLeft2() const;
    void Stop();

    int32 Started;
    int32 Accumulated;
    int32 Duration;
};
