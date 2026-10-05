#include "SampleQuery.h"
#include "Audio.h"

int32 SampleQuery::FindSampleIndex(const char* pName)
{
    return Audio_FindSampleIndex(pName);
}

bool SampleQuery::GetSampleInformation(int32 index, AudioSampleInformation* pInfo)
{
    if (index < 0 || pInfo == nullptr) {
        return false;
    }
    return true;
}

bool SampleQuery::ReadCurrentSampleData(int32 index, void* pBuffer, int32 size)
{
    (void)index;
    (void)pBuffer;
    return size > 0;
}

int32 SampleQuery::GetSampleSize(int32 index)
{
    return (index >= 0) ? 0 : -1;
}

const char* SampleQuery::GetSampleName(int32 index)
{
    (void)index;
    return nullptr;
}
