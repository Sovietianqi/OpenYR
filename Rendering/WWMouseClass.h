#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class WWMouseClass {
public:
    WWMouseClass() noexcept;

    // 根据游戏行为，可知鼠标驱动面围绕坐标读取与独占捕获展开：
    // 捕获要等互斥体，释放时归还。
    int32 GetX() const;
    int32 GetY() const;
    void GetPoint2D(int32* pX, int32* pY) const;
    bool CaptureMouse();
    void ReleaseMouse();

    void* Handle;
    int32 MouseDX;
    int32 MouseDY;
    int32 X;
    int32 Y;
    bool IsCaptured;
};
