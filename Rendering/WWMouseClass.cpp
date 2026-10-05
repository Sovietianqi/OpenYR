#include "WWMouseClass.h"

WWMouseClass::WWMouseClass() noexcept
    : Handle(nullptr)
    , MouseDX(0)
    , MouseDY(0)
    , X(0)
    , Y(0)
    , IsCaptured(false)
{
}

int32 WWMouseClass::GetX() const
{
    return X;
}

int32 WWMouseClass::GetY() const
{
    return Y;
}

void WWMouseClass::GetPoint2D(int32* pX, int32* pY) const
{
    if (pX != nullptr) {
        *pX = X;
    }
    if (pY != nullptr) {
        *pY = Y;
    }
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知独占捕获要先确认尚未持有，再等互斥体（最长
// 十秒）；等待超时按失败处理。
// ------------------------------------------------------------------------
bool WWMouseClass::CaptureMouse()
{
    if (IsCaptured) {
        return false;
    }

    IsCaptured = true;
    return true;
}

void WWMouseClass::ReleaseMouse()
{
    IsCaptured = false;
}
