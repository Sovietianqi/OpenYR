#include "Struct43C.h"
#include "../Math/Timer.h"

#include <cmath>

// ============================================================================
// Struct43C —— 逐句对照 asm 的计时状态机
//   参考汇编区间：0x4A50E0(CTOR) / 0x4A5108(IsSet11) / 0x4A5128(IsSet10)
//                0x4A5146(IsTimerFinished) / 0x4A51A8(IsSet01)
//                0x4A51D0(IsSet00) / 0x4A51F0(StartTimer11)
//                0x4A523C(StartTimer10) / 0x4A52CC(Update)
//                0x4A52E0(Set01) / 0x4A52F4(Set00) / 0x4A531C(RatioSpent)
//                0x4A5363(SetValuesDone)
// ============================================================================

// 全局帧钟：asm 的 `Frame` 符号（逻辑帧计数，非墙钟毫秒）。
int32 Struct43C_CurrentFrame()
{
    return FrameTimer::GetTime();
}

// asm 0x4A50E0:
//   CycleTimer.TimeStart = Frame
//   CycleTimer.TimeLeft  = 0
//   RateInt2 = 0
//   field_18 = 0
//   field_19 = 0
//   （RateDouble 与 CycleTimer.LastCheck 原版不写，保持未初始化语义；
//     这里给 0 只是为了消除不确定值，不影响任何读取路径。）
Struct43C::Struct43C() noexcept
    : RateDouble(0.0)
    , CycleTimer()
    , RateInt2(0)
    , field_18(0)
    , field_19(0)
    , field_1A(0)
    , field_1B(0)
{
    CycleTimer.TimeStart = Struct43C_CurrentFrame();
    CycleTimer.TimeLeft  = 0;
    CycleTimer.LastCheck = 0;
}

// asm 0x4A52F4: xor al,al / mov [ecx+18h],al / mov [ecx+19h],al
void Struct43C::Set00()
{
    field_18 = 0;
    field_19 = 0;
}

// asm 0x4A52E0: mov byte [ecx+18h],0 / mov byte [ecx+19h],1
void Struct43C::Set01()
{
    field_18 = 0;
    field_19 = 1;
}

// asm 0x4A5108: 18 != 0 && 19 != 0
bool Struct43C::IsSet11() const
{
    return field_18 != 0 && field_19 != 0;
}

// asm 0x4A5128: 18 != 0 && 19 == 0
bool Struct43C::IsSet10() const
{
    return field_18 != 0 && field_19 == 0;
}

// asm 0x4A51A8: 18 == 0 && 19 == 1
bool Struct43C::IsSet01() const
{
    return field_18 == 0 && field_19 == 1;
}

// asm 0x4A51D0: 18 == 0 && 19 == 0
bool Struct43C::IsSet00() const
{
    return field_18 == 0 && field_19 == 0;
}

// asm 0x4A5363:
//   if (18 == 0) return;
//   if (19 != 0) { 18 = 0; 19 = 1; }
//   if (18 != 0 && 19 == 0) { 18 = 0; 19 = 0; }
void Struct43C::SetValuesDone()
{
    if (field_18 == 0)
        return;

    if (field_19 != 0)
    {
        field_18 = 0;
        field_19 = 1;
    }

    if (field_18 != 0 && field_19 == 0)
    {
        field_18 = 0;
        field_19 = 0;
    }
}

// asm 0x4A51F0 (retn 8, __thiscall double)：
//   if (!(18 != 0 || 19 != 1)) return;          // 即 18==0 && 19==1 时空转
//   RateDouble = seconds * 900
//   e = floor(seconds * 900)
//   18 = 1
//   CycleTimer.TimeStart = Frame
//   CycleTimer.TimeLeft  = e
//   RateInt2             = e
//   19 = 1
void Struct43C::StartTimer11(double seconds)
{
    if (field_18 == 0 && field_19 == 1)
        return;

    const double scaled = seconds * 900.0;
    const int32  frames = static_cast<int32>(scaled < 0.0 ? 0.0
                                        : (scaled > 2147483647.0 ? 2147483647.0
                                                                 : std::floor(scaled)));

    RateDouble          = scaled;
    field_18            = 1;
    CycleTimer.TimeStart = Struct43C_CurrentFrame();
    CycleTimer.LastCheck = 0;
    CycleTimer.TimeLeft  = frames;
    RateInt2             = frames;
    field_19             = 1;
}

// asm 0x4A523C (DoorClass::Close_Door)：
//   if (!(18 != 0 || 19 != 0)) return;          // 即 00 时空转
//   其余与 StartTimer11 相同，只是最后 19 = 0（得到 10 态）。
void Struct43C::StartTimer10(double seconds)
{
    if (field_18 == 0 && field_19 == 0)
        return;

    const double scaled = seconds * 900.0;
    const int32  frames = static_cast<int32>(scaled < 0.0 ? 0.0
                                        : (scaled > 2147483647.0 ? 2147483647.0
                                                                 : std::floor(scaled)));

    RateDouble          = scaled;
    field_18            = 1;
    CycleTimer.TimeStart = Struct43C_CurrentFrame();
    CycleTimer.LastCheck = 0;
    CycleTimer.TimeLeft  = frames;
    RateInt2             = frames;
    field_19             = 0;
}

// asm 0x4A52CC:
//   if (18 == 0) return;
//   left = CycleTimer.TimeLeft;
//   if (CycleTimer.TimeStart != -1) {
//       elapsed = Frame - CycleTimer.TimeStart;
//       left    = (elapsed >= left) ? 0 : (left - elapsed);
//   }
//   CycleTimer.TimeLeft = RateInt2 - left;
//   19 = (19 == 0);
void Struct43C::Update()
{
    if (field_18 == 0)
        return;

    int32 left = CycleTimer.TimeLeft;
    if (CycleTimer.TimeStart != -1)
    {
        const int32 elapsed = Struct43C_CurrentFrame() - CycleTimer.TimeStart;
        left = (elapsed >= left) ? 0 : (left - elapsed);
    }

    CycleTimer.TimeLeft = RateInt2 - left;
    field_19            = (field_19 == 0) ? 1 : 0;
}

// asm 0x4A5146:
//   if (18 == 0) return true;
//   total = RateInt2; if (total == 0) return true;
//   remaining = TimeLeft（TimeStart == -1 时）否则 max(0, TimeLeft - (Frame - TimeStart))
//   spent = total - remaining
//   return (spent / total >= 1.0)    // fcomp 1.0 → C3 置位即真
bool Struct43C::IsTimerFinished() const
{
    if (field_18 == 0)
        return true;

    const int32 total = RateInt2;
    if (total == 0)
        return true;

    int32 remaining = CycleTimer.TimeLeft;
    if (CycleTimer.TimeStart != -1)
    {
        const int32 elapsed = Struct43C_CurrentFrame() - CycleTimer.TimeStart;
        remaining = (elapsed >= remaining) ? 0 : (remaining - elapsed);
    }

    const double spent = static_cast<double>(total - remaining);
    return (spent / static_cast<double>(total)) >= 1.0;
}

// asm 0x4A531C:
//   if (18 == 0) return 1.0;
//   total = RateInt2; if (total == 0) return 1.0;
//   remaining = ...（同上）
//   return (total - remaining) / total
double Struct43C::RatioSpent() const
{
    if (field_18 == 0)
        return 1.0;

    const int32 total = RateInt2;
    if (total == 0)
        return 1.0;

    int32 remaining = CycleTimer.TimeLeft;
    if (CycleTimer.TimeStart != -1)
    {
        const int32 elapsed = Struct43C_CurrentFrame() - CycleTimer.TimeStart;
        remaining = (elapsed >= remaining) ? 0 : (remaining - elapsed);
    }

    return static_cast<double>(total - remaining) / static_cast<double>(total);
}
