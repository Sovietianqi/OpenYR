#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class PixelFXClass {
public:
    PixelFXClass() noexcept;

    // 根据游戏行为，可知像素特效的生命面是：按抖动表随机取偏移
    // 与颜色、外部直接写偏移、倒计时归零判定、以及每帧推进。
    void Process(int32 kind);
    void SetOffset(int32 x, int32 y);
    bool IsFinished(int32 countdown);
    void Update();

    int32 OffsetX;
    int32 OffsetY;
    int32 ColorR;
    int32 ColorG;
    int32 ColorB;
    int32 BitMask;
    int32 Duration;
};
