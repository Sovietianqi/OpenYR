#include "CountClass.h"
#include "../Core/Memory.h"
#include <cstring>

#ifdef _WIN32
#else
#include <arpa/inet.h>

// glibc 把 htonl/ntohl 定义成宏，会吞掉方法名；此处按函数语义使用。
#undef htonl
#undef ntohl
#endif

// ============================================================================
// 计数表（根据游戏行为实现）
// ============================================================================

CountClass::CountClass() noexcept
    : Values(nullptr)
    , Capacity(0)
    , Count(0)
    , Limit(0)
{
}

// 根据游戏行为，可知总数为各槽位之和。
int32 CountClass::GetTotal() const
{
    int32 total = 0;
    for (int32 i = 0; i < Count; ++i) {
        total += Values[i];
    }
    return total;
}

// 根据游戏行为，可知指定槽位累加。
void CountClass::IncValue(int32 idx)
{
    if (idx < 0 || idx >= Count) {
        return;
    }
    ++Values[idx];
}

// 根据游戏行为，可知容量不足时先扩容再返回新槽位下标；已够用时
// 返回最后一个槽位。
int32 CountClass::IncValue_OrAlloc()
{
    if (Count >= Capacity) {
        const int32 newCap = Capacity > 0 ? Capacity * 2 : 8;
        int32* pNew = static_cast<int32*>(YRMemory::Allocate(sizeof(int32) * newCap));
        if (Values != nullptr) {
            memcpy(pNew, Values, sizeof(int32) * Count);
            YRMemory::Deallocate(Values);
        }
        Values = pNew;
        Capacity = newCap;
    }
    ++Count;
    Values[Count - 1] = 0;
    return Count - 1;
}

// 根据游戏行为，可知移除一个槽位会把后续槽位整体前移。
void CountClass::Rem(int32 idx)
{
    if (idx < 0 || idx >= Count) {
        return;
    }
    for (int32 i = idx; i < Count - 1; ++i) {
        Values[i] = Values[i + 1];
    }
    --Count;
}

// 根据游戏行为，可知受限复位把超过上限的槽位截断，其余清零。
void CountClass::ResetLimited()
{
    const int32 keep = (Limit > 0 && Limit < Count) ? Limit : Count;
    Count = keep;
    ResetWithGivenCount(Count);
}

// 根据游戏行为，可知按给定槽位数整体清零。
void CountClass::ResetWithGivenCount(int32 count)
{
    if (count < 0 || count > Count) {
        count = Count;
    }
    for (int32 i = 0; i < count; ++i) {
        Values[i] = 0;
    }
    Count = count;
}

// 根据游戏行为，可知容量调整会保留原有槽位数据。
void CountClass::ResizeTo(int32 count)
{
    if (count <= Capacity) {
        Count = count;
        return;
    }
    int32* pNew = static_cast<int32*>(YRMemory::Allocate(sizeof(int32) * count));
    if (Values != nullptr) {
        memcpy(pNew, Values, sizeof(int32) * Count);
        YRMemory::Deallocate(Values);
    }
    Values = pNew;
    Capacity = count;
    Count = count;
}

// 根据游戏行为，可知发包前把整表转为主机序到网络序。
void CountClass::htonl()
{
    for (int32 i = 0; i < Count; ++i) {
        Values[i] = static_cast<int32>(::htonl(static_cast<uint32>(Values[i])));
    }
}

// 根据游戏行为，可知收包后把整表转回网络序到主机序。
void CountClass::ntohl()
{
    for (int32 i = 0; i < Count; ++i) {
        Values[i] = static_cast<int32>(::ntohl(static_cast<uint32>(Values[i])));
    }
}
