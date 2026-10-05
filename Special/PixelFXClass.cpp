#include "PixelFXClass.h"

static constexpr int32 FX_SPARK_TABLE_SIZE = 12;

struct FXSparkEntry {
    int32 ColorR;
    int32 ColorG;
    int32 ColorB;
    int32 Reserved0;
    int32 Reserved1;
    int32 Reserved2;
    int32 BitMask;
};

// ------------------------------------------------------------------------
// 根据游戏行为，可知火花外观取自一张固定的抖动表：每次处理按
// 随机数折算横纵偏移并套用表内颜色与位掩码。
// ------------------------------------------------------------------------
static FXSparkEntry arr_FXSparkTable[FX_SPARK_TABLE_SIZE] = {
    {255, 255, 255, 0, 0, 0, 7},
    {255, 255, 128, 0, 0, 0, 7},
    {255, 192,  64, 0, 0, 0, 3},
    {255, 128,   0, 0, 0, 0, 3},
    {224,  96,   0, 0, 0, 0, 1},
    {192,  64,   0, 0, 0, 0, 1},
    {160,  48,   0, 0, 0, 0, 1},
    {128,  32,   0, 0, 0, 0, 0},
    { 96,  24,   0, 0, 0, 0, 0},
    { 64,  16,   0, 0, 0, 0, 0},
    { 32,   8,   0, 0, 0, 0, 0},
    {  0,   0,   0, 0, 0, 0, 0},
};

PixelFXClass::PixelFXClass() noexcept
    : OffsetX(0)
    , OffsetY(0)
    , ColorR(0)
    , ColorG(0)
    , ColorB(0)
    , BitMask(0)
    , Duration(0)
{
}

void PixelFXClass::Process(int32 kind)
{
    if (kind < 0 || kind >= FX_SPARK_TABLE_SIZE) {
        return;
    }

    const FXSparkEntry& entry = arr_FXSparkTable[kind];
    ColorR = entry.ColorR;
    ColorG = entry.ColorG;
    ColorB = entry.ColorB;
    BitMask = entry.BitMask;
    OffsetX = (kind % 64) - 31;
    OffsetY = (kind % 32) - 15;
}

void PixelFXClass::SetOffset(int32 x, int32 y)
{
    OffsetX = x;
    OffsetY = y;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知倒计时扣减到非正即报告终结，并把余量钳回零。
// ------------------------------------------------------------------------
bool PixelFXClass::IsFinished(int32 countdown)
{
    Duration -= countdown;
    if (Duration > 0) {
        return false;
    }
    Duration = 0;
    return true;
}

void PixelFXClass::Update()
{
    ++Duration;
}
