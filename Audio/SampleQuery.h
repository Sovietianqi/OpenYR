#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

struct AudioSampleInformation;

// ------------------------------------------------------------------------
// 根据游戏行为，可知采样查询面按名字与索引两条通道取样本元数据。
// ------------------------------------------------------------------------
class SampleQuery {
public:
    static int32 FindSampleIndex(const char* pName);
    static bool GetSampleInformation(int32 index, AudioSampleInformation* pInfo);
    static bool ReadCurrentSampleData(int32 index, void* pBuffer, int32 size);
    static int32 GetSampleSize(int32 index);
    static const char* GetSampleName(int32 index);
};
