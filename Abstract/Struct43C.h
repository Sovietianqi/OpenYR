#pragma once

#include "../Core/Definitions.h"

// ============================================================================
// Struct43C - 生产进度计时结构
//
// 根据游戏行为，可知该结构以“起始帧 + 时长 + 总量”描述一个生产计时，
// 并用一对标志字节表达阶段状态（00=未开始，01=进行中，11=已完成）。
// ============================================================================
struct Struct43C {
    Struct43C() noexcept;

    void  Set00();
    void  Set01();
    bool  IsSet01() const;
    bool  IsSet10() const;
    bool  IsSet11() const;
    void  SetValuesDone();
    void  StartTimer11(int32 duration);
    bool  IsTimerFinished() const;
    double RatioSpent() const;

    int32 StartFrame;    // 计时起始帧（-1 表示未启动）
    int32 Duration;      // 计时时长（帧）
    int32 Total;         // 进度总量
    uint8 Flag0;         // 阶段标志低位
    uint8 Flag1;         // 阶段标志高位
};

// 全局帧钟由 FrameTimer 提供（Math/Timer.h）。
int32 Struct43C_CurrentFrame();
