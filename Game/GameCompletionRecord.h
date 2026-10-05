#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

// ------------------------------------------------------------------------
// 根据游戏行为，可知战绩记录面按不同的结算口径创建条目，并把
// 新记录串进既有历史链。
// ------------------------------------------------------------------------
class GameCompletionRecord {
public:
    GameCompletionRecord() noexcept;

    bool Create2(int32 houseIndex, int32 difficulty);
    bool Create3(int32 houseIndex, int32 difficulty, int32 durationMinutes);
    bool Create4(int32 houseIndex, int32 difficulty, int32 durationMinutes, bool isCoop);
    void Chain(GameCompletionRecord* pNext);

    GameCompletionRecord* Next;
    int32 HouseIndex;
    int32 Difficulty;
    int32 Duration;
    bool IsCoop;
};
