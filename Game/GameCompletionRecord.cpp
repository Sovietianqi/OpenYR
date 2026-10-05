#include "GameCompletionRecord.h"

GameCompletionRecord::GameCompletionRecord() noexcept
    : Next(nullptr)
    , HouseIndex(-1)
    , Difficulty(0)
    , Duration(0)
    , IsCoop(false)
{
}

bool GameCompletionRecord::Create2(int32 houseIndex, int32 difficulty)
{
    HouseIndex = houseIndex;
    Difficulty = difficulty;
    return true;
}

bool GameCompletionRecord::Create3(int32 houseIndex, int32 difficulty, int32 durationMinutes)
{
    Create2(houseIndex, difficulty);
    Duration = durationMinutes;
    return true;
}

bool GameCompletionRecord::Create4(int32 houseIndex, int32 difficulty, int32 durationMinutes, bool isCoop)
{
    Create3(houseIndex, difficulty, durationMinutes);
    IsCoop = isCoop;
    return true;
}

void GameCompletionRecord::Chain(GameCompletionRecord* pNext)
{
    Next = pNext;
}
