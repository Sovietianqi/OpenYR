#include "Struct43C.h"
#include "../Math/Timer.h"

// ============================================================================
// 生产进度计时（根据游戏行为实现）
// ============================================================================

// 根据游戏行为，可知帧钟取自全局帧计数器。
int32 Struct43C_CurrentFrame()
{
    return FrameTimer::GetTime();
}

Struct43C::Struct43C() noexcept
    : StartFrame(-1)
    , Duration(0)
    , Total(0)
    , Flag0(0)
    , Flag1(0)
{
}

// 根据游戏行为，可知 00 状态表示计时未开始。
void Struct43C::Set00()
{
    Flag0 = 0;
    Flag1 = 0;
}

// 根据游戏行为，可知 01 状态表示计时进行中。
void Struct43C::Set01()
{
    Flag0 = 0;
    Flag1 = 1;
}

// 根据游戏行为，可知 01 判定要求低位为 0 且高位为 1。
bool Struct43C::IsSet01() const
{
    return Flag0 == 0 && Flag1 == 1;
}

// 根据游戏行为，可知 10 判定要求低位为 1 且高位为 0。
bool Struct43C::IsSet10() const
{
    return Flag0 != 0 && Flag1 == 0;
}

// 根据游戏行为，可知 11 判定要求两位都为 1。
bool Struct43C::IsSet11() const
{
    return Flag0 != 0 && Flag1 != 0;
}

// 根据游戏行为，可知“数值就绪”把两位都置 1。
void Struct43C::SetValuesDone()
{
    Flag0 = 1;
    Flag1 = 1;
}

// 根据游戏行为，可知启动计时会把起始帧记为当前帧、登记时长并进入
// 11 状态。
void Struct43C::StartTimer11(int32 duration)
{
    StartFrame = Struct43C_CurrentFrame();
    Duration = duration > 0 ? duration : 0;
    Flag0 = 1;
    Flag1 = 1;
}

// 根据游戏行为，可知计时完成的条件：标志已置、总量非零且起始帧有效
// 时，经过帧数达到时长即完成。
bool Struct43C::IsTimerFinished() const
{
    if (Flag0 == 0 || Total == 0) {
        return false;
    }
    if (StartFrame == -1) {
        return false;
    }
    const int32 elapsed = Struct43C_CurrentFrame() - StartFrame;
    return elapsed >= Duration;
}

// 根据游戏行为，可知已花费比例 = (总量 - 剩余量) / 总量；总量为零时
// 直接按 1.0 处理，未启动时按 0 处理。
double Struct43C::RatioSpent() const
{
    if (Total == 0) {
        return 1.0;
    }
    if (Flag0 == 0 || StartFrame == -1) {
        return 0.0;
    }
    int32 elapsed = Struct43C_CurrentFrame() - StartFrame;
    if (elapsed < 0) {
        elapsed = 0;
    }
    const int32 remaining = elapsed >= Duration ? 0 : Duration - elapsed;
    const int32 spent = Total - remaining;
    return static_cast<double>(spent) / static_cast<double>(Total);
}
